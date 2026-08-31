#!/usr/bin/env python3
"""Evaluate the #29 ESP32 bring-up probe's static diagnostic task stack gate.

Implements ``ISSUE29_DIAGNOSTIC_TASK_STATIC_STACK_GATE`` per
docs/tasks/issue-29-panic-requalification-correction-plan.md, Abschnitt
4.1-4.3: the maximum over *all* statically reachable call paths from
``probeTask`` through the compiled GCC ``-fstack-usage``/
``-fcallgraph-info=su`` artefacts (``.su``/``.ci`` files) under an
ESP-IDF ``esp32_bringup`` build directory.

Fail-closed per Abschnitt 4.1.1: a reachable edge without a compiled stack
frame, an unresolved indirect/virtual call, an unbounded virtual target set,
or a reachable callgraph cycle each block the gate (``STACK_GATE=BLOCKED``).
The only exceptions are the small, explicit, source-verified boundaries
below (three closed virtual-dispatch call sites already used by the probe,
and the ROM-resident libc/libgcc leaf functions verified against the linked
ELF and the ESP32 mask-ROM ELF); see docs/ISSUE_29_MEASUREMENTS.md for the
underlying evidence. Nothing else is treated as bounded by assumption.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

NODE_RE = re.compile(
    r'node: \{ title: "(?P<title>[^"]+)" label: "(?P<label>(?:[^"\\]|\\.)*)"(?P<rest>[^}]*)\}'
)
EDGE_RE = re.compile(
    r'edge: \{ sourcename: "(?P<src>[^"]+)" targetname: "(?P<tgt>[^"]+)"'
)
FRAME_RE = re.compile(r"(?P<bytes>\d+) bytes \((?P<qual>[a-z,]+)\)")

ROOT_LABEL_NEEDLE = "{anonymous}::probeTask(void*)"
INDIRECT_CALL_TARGET = "__indirect_call"

# Closed virtual-dispatch target sets (plan Abschnitt 4.1.1). Each entry
# maps a unique label substring identifying the *caller* function that
# performs the dispatch to the concrete override(s) its target set is
# closed and fully enumerable to. Source-verified this round:
#   - IStateStore::read/write are dispatched only from
#     lib/fermentation_app/src/run_persistence_store.cpp
#     (RunPersistenceStore::readHead/readSlot, the anonymous-namespace
#     writeExact helper), resolving only to the probe's local
#     BringupStateStore (final, sole construction site reachable from
#     runProbe()).
#   - IBidirectionalActuatorSink::setForward/setReverse and
#     IBinaryOutputSink::setEnabled are dispatched only from
#     lib/fermentation_app/src/actuator_plan_sink_driver.cpp
#     (ActuatorPlanSinkDriver::apply), resolving only to the probe's local
#     AllOffBidirectionalSink/AllOffBinarySink (final, sole construction
#     site reachable from runProbe()).
VIRTUAL_DISPATCH_WHITELIST: dict[str, tuple[str, ...]] = {
    "RunPersistenceStore::readHead(": ("BringupStateStore::read(",),
    "RunPersistenceStore::readSlot(": ("BringupStateStore::read(",),
    "{anonymous}::writeExact(device_platform::IStateStore": (
        "BringupStateStore::write(",
        "BringupStateStore::read(",
    ),
    "ActuatorPlanSinkDriver::apply(const fermentation::ActuatorPlanTickResult": (
        "AllOffBidirectionalSink::setForward(",
        "AllOffBidirectionalSink::setReverse(",
        "AllOffBinarySink::setEnabled(",
    ),
}

# ROM-resident leaf boundaries. Verified this round via objdump against
# both the linked esp32_bringup ELF and esp-rom-elfs-20241011's
# esp32_rev0_rom.elf/esp32_rev300_rom.elf (identical addresses and bodies
# on both -> physical mask ROM, chip-revision independent, matching
# CONFIG_ESP32_REV_MIN=0/CONFIG_ESP32_REV_MAX_FULL=399). Each function's
# entire reachable body (including same-window branch targets below its
# own entry point, e.g. memcpy's shared __memcpy_aux tail) contains
# exactly one Xtensa windowed `entry a1, N` and zero further call/callx
# instructions - a measured, not assumed, bound. See
# docs/ISSUE_29_MEASUREMENTS.md for the full disassembly evidence.
ROM_LEAF_BOUNDARIES: dict[str, int] = {
    "memcpy": 16,
    "memset": 16,
    "memcmp": 32,
    "memmove": 32,
    "__udivdi3": 32,
    "__eqdf2": 16,
    "__gedf2": 16,
    "__gtdf2": 16,
    "__ledf2": 16,
    "__nedf2": 16,
    "__unorddf2": 16,
}


@dataclass(frozen=True)
class NodeInfo:
    title: str
    label: str
    frame_bytes: int | None
    qualifier: str | None
    is_thunk: bool


def parse_callgraph(
    build_dir: Path,
) -> tuple[dict[str, NodeInfo], dict[str, set[str]]]:
    nodes: dict[str, NodeInfo] = {}
    scores: dict[str, int] = {}
    edges: dict[str, set[str]] = defaultdict(set)
    ci_files = sorted(build_dir.rglob("*.ci"))
    if not ci_files:
        raise SystemExit(f"no .ci callgraph artefacts found under {build_dir}")
    for path in ci_files:
        for line in path.read_text(encoding="utf-8").splitlines():
            node_match = NODE_RE.search(line)
            if node_match is not None:
                title = node_match.group("title")
                label = node_match.group("label")
                rest = node_match.group("rest")
                frame = FRAME_RE.search(label)
                is_thunk = "triangle" in rest
                if frame is not None:
                    score = 2
                    info = NodeInfo(
                        title, label, int(frame.group("bytes")), frame.group("qual"), False
                    )
                elif is_thunk:
                    score = 1
                    info = NodeInfo(title, label, None, None, True)
                else:
                    score = 0
                    info = NodeInfo(title, label, None, None, False)
                if title not in nodes or score > scores[title]:
                    nodes[title] = info
                    scores[title] = score
            edge_match = EDGE_RE.search(line)
            if edge_match is not None:
                edges[edge_match.group("src")].add(edge_match.group("tgt"))
    return nodes, edges


def find_root(nodes: dict[str, NodeInfo]) -> str:
    matches = [
        title
        for title, info in nodes.items()
        if info.frame_bytes is not None and ROOT_LABEL_NEEDLE in info.label
    ]
    if len(matches) != 1:
        raise SystemExit(
            f"expected exactly one compiled probeTask(void*) frame, found {len(matches)}"
        )
    return matches[0]


def find_unique_override(nodes: dict[str, NodeInfo], needle: str) -> str | None:
    matches = [
        title
        for title, info in nodes.items()
        if info.frame_bytes is not None and needle in info.label
    ]
    if len(matches) == 1:
        return matches[0]
    return None


class StackGateBlocked(Exception):
    pass


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "build_dir", type=Path, help="ESP-IDF esp32_bringup build directory"
    )
    arguments = parser.parse_args()
    build_dir: Path = arguments.build_dir

    nodes, edges = parse_callgraph(build_dir)
    root = find_root(nodes)

    findings: list[tuple[str, str, str]] = []
    memo: dict[str, tuple[int, tuple[str, ...]]] = {}
    on_stack: list[str] = []

    def deref_thunk(title: str, visiting: set[str]) -> str | None:
        info = nodes.get(title)
        if info is None:
            return None
        if info.frame_bytes is not None:
            return title
        if not info.is_thunk:
            return None
        if title in visiting:
            findings.append(("REACHABLE_CALLGRAPH_CYCLE", title, "thunk cycle"))
            return None
        real_targets = [t for t in edges.get(title, ()) if t != INDIRECT_CALL_TARGET]
        if len(real_targets) != 1:
            findings.append(
                ("UNRESOLVED_EDGE_WITHOUT_STACK_FRAME", title, f"thunk with {len(real_targets)} targets")
            )
            return None
        return deref_thunk(real_targets[0], visiting | {title})

    def resolve_children(title: str) -> list[str]:
        info = nodes[title]
        children: list[str] = []
        for target in sorted(edges.get(title, ())):
            if target == INDIRECT_CALL_TARGET:
                overrides = None
                for needle, candidate_overrides in VIRTUAL_DISPATCH_WHITELIST.items():
                    if needle in info.label:
                        overrides = candidate_overrides
                        break
                if overrides is None:
                    findings.append(
                        ("UNRESOLVED_INDIRECT_CALL", title, "no closed virtual target set for this caller")
                    )
                    continue
                for override_needle in overrides:
                    resolved = find_unique_override(nodes, override_needle)
                    if resolved is None:
                        findings.append(
                            ("UNRESOLVED_INDIRECT_CALL", title, f"whitelisted override not found: {override_needle}")
                        )
                        continue
                    children.append(resolved)
                continue

            resolved = deref_thunk(target, set())
            if resolved is not None:
                children.append(resolved)
                continue

            if target in ROM_LEAF_BOUNDARIES:
                # The merged node may already hold a low-score placeholder
                # (e.g. a `shape: ellipse` __builtin_* stub with no frame
                # data); the objdump-verified ROM boundary always wins.
                nodes[target] = NodeInfo(
                    target,
                    target,
                    ROM_LEAF_BOUNDARIES[target],
                    "static (ROM boundary, objdump-verified)",
                    False,
                )
                children.append(target)
                continue

            target_info = nodes.get(target)
            if target_info is not None and target_info.is_thunk:
                # deref_thunk already recorded the specific finding.
                continue
            findings.append(("REACHABLE_EDGE_WITHOUT_STACK_FRAME", title, target))
        return children

    def longest_from(title: str) -> tuple[int, tuple[str, ...]]:
        if title in memo:
            return memo[title]
        if title in on_stack:
            findings.append(("REACHABLE_CALLGRAPH_CYCLE", title, " -> ".join(on_stack)))
            return (0, (title,))
        on_stack.append(title)
        info = nodes[title]
        best_bytes = 0
        best_path: tuple[str, ...] = ()
        for child in resolve_children(title):
            child_bytes, child_path = longest_from(child)
            if child_bytes > best_bytes:
                best_bytes = child_bytes
                best_path = child_path
        on_stack.pop()
        assert info.frame_bytes is not None
        result = (info.frame_bytes + best_bytes, (title,) + best_path)
        memo[title] = result
        return result

    total_bytes, path = longest_from(root)

    def short_name(title: str) -> str:
        info = nodes.get(title)
        if info is None:
            return title
        return info.label.split("\\n", 1)[0].split("\n", 1)[0]

    unresolved_edges = {
        (caller, detail)
        for kind, caller, detail in findings
        if kind == "REACHABLE_EDGE_WITHOUT_STACK_FRAME"
    }
    unresolved_indirect = {
        (caller, detail) for kind, caller, detail in findings if kind == "UNRESOLVED_INDIRECT_CALL"
    }
    unresolved_cycles = {
        (caller, detail) for kind, caller, detail in findings if kind == "REACHABLE_CALLGRAPH_CYCLE"
    }
    unresolved_thunks = {
        (caller, detail)
        for kind, caller, detail in findings
        if kind == "UNRESOLVED_EDGE_WITHOUT_STACK_FRAME"
    }

    print(f"CURRENT_MAX_PROBE_TASK_PATH={' -> '.join(short_name(t) for t in path)}")
    print(f"CURRENT_MAX_PROBE_TASK_CUMULATIVE_BYTES={total_bytes}")
    print(f"UNKNOWN_REACHABLE_EDGES={len(unresolved_edges) + len(unresolved_thunks)}")
    print(f"UNRESOLVED_INDIRECT_CALLS={len(unresolved_indirect)}")
    print(f"UNRESOLVED_CALLGRAPH_CYCLES={len(unresolved_cycles)}")

    if findings:
        print("ALL_RELEVANT_PROBE_STACK_PATHS=BLOCKED")
        print("STACK_GATE=BLOCKED")
        print("STOP_OWNER_REVIEW")
        print()
        print("-- REACHABLE_EDGE_WITHOUT_STACK_FRAME --")
        for caller, target in sorted(unresolved_edges):
            print(f"   caller={short_name(caller)!r} target={target!r}")
        print("-- UNRESOLVED_EDGE_WITHOUT_STACK_FRAME (thunk resolution failed) --")
        for caller, detail in sorted(unresolved_thunks):
            print(f"   caller={short_name(caller)!r} detail={detail}")
        print("-- UNRESOLVED_INDIRECT_CALL --")
        for caller, detail in sorted(unresolved_indirect):
            print(f"   caller={short_name(caller)!r} detail={detail}")
        print("-- REACHABLE_CALLGRAPH_CYCLE --")
        for caller, detail in sorted(unresolved_cycles):
            print(f"   caller={short_name(caller)!r} detail={detail}")
        return 1

    print("ALL_RELEVANT_PROBE_STACK_PATHS=PASS")

    # Staleness gate (plan Abschnitt 4.3), only reachable once the gate
    # above is a real PASS with an exact witness.
    probe_source = Path("main/issue_29_bringup_probe.cpp")
    probe_text = probe_source.read_text(encoding="utf-8")

    def read_constant(name: str) -> int:
        match = re.search(rf"{name}\s*=\s*(\d+)U?;", probe_text)
        if match is None:
            raise SystemExit(f"cannot find compiled constant {name} in {probe_source}")
        return int(match.group(1))

    measured_call_path_bytes = read_constant("kMeasuredCallPathBytes")
    safety_buffer_bytes = read_constant("kMeasuredCallPathSafetyBufferBytes")
    probe_task_stack_bytes = read_constant("kProbeTaskStackBytes")
    expected_task_stack_bytes = (
        (measured_call_path_bytes + safety_buffer_bytes + 1023) // 1024
    ) * 1024

    print(f"kMeasuredCallPathBytes={measured_call_path_bytes}")
    print(f"kMeasuredCallPathSafetyBufferBytes={safety_buffer_bytes}")
    print(f"kProbeTaskStackBytes={probe_task_stack_bytes}")
    print(f"EXPECTED_TASK_STACK_BYTES={expected_task_stack_bytes}")

    staleness_ok = True
    if measured_call_path_bytes != total_bytes:
        print(
            f"STACK_CONSTANT_MISMATCH: kMeasuredCallPathBytes={measured_call_path_bytes} "
            f"!= CURRENT_MAX_PROBE_TASK_CUMULATIVE_BYTES={total_bytes}"
        )
        staleness_ok = False
    if probe_task_stack_bytes != expected_task_stack_bytes:
        print(
            f"STACK_DERIVATION_MISMATCH: kProbeTaskStackBytes={probe_task_stack_bytes} "
            f"!= EXPECTED_TASK_STACK_BYTES={expected_task_stack_bytes}"
        )
        staleness_ok = False

    if not staleness_ok:
        print("STACK_GATE=BLOCKED")
        print("STOP_OWNER_REVIEW")
        return 1

    print("STACK_CONSTANT_GATE=PASS")
    print("STACK_DERIVATION_GATE=PASS")
    print("STACK_GATE=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
