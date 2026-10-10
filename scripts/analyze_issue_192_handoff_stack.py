#!/usr/bin/env python3
"""Static main-task stack check for the authorised run-epoch handoff (#192).

Build-specific, intentionally small (pattern: ``analyze_issue_29_stack.py``):
the ``entry a1, N`` frame of every function and the direct ``call0/4/8/12``
edges are read from ``objdump -d -C`` of the linked ESP32 ELF.  The script sums
the frames which are live at the same time on three call chains and reports the
resulting reserve against the main task stack.

Limits of a static direct-edge analysis, reported explicitly instead of hidden:

* ``callx*`` (virtual / pointer calls, e.g. the state-store port) have no
  static target.  Their number per function on the chain is listed; the sums
  are therefore lower bounds.
* Interrupt frames, ROM callees without ``entry`` and recursion are not
  covered.  Cycles met while walking the graph are counted and listed.

``--calibrate`` checks the pre-fix ELF (``f859ef6``) against the values of
``docs/audits/ISSUE192_FACTORY_RESET_HW_20261010_EVIDENCE.md`` section 3b.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

STACK_BUDGET = 24576
RESERVE_TARGET = 6144
CHAIN_LIMIT = STACK_BUDGET - RESERVE_TARGET

HEADER = re.compile(r"^[0-9a-f]{8} <(.+)>:$")
ENTRY = re.compile(r"\tentry\ta1, (0x[0-9a-f]+|\d+)\s*$")
CALL = re.compile(r"\tcall(?:0|4|8|12)\t[0-9a-f]+ <(.+?)(\+0x[0-9a-f]+)?>\s*$")
CALLX = re.compile(r"\tcallx(?:0|4|8|12)\t")

# (chain, function holding the entry point of the chain, handoff function)
CHAINS = (
    ("K-HOLD", "FermentationApplication::beginAuthorizedFactoryReset", "prepareAuthorizedEpochHandoff"),
    ("K-BOOT-P", "completeAuthorizedEpochHandoff", "prepareAuthorizedEpochHandoff"),
    ("K-BOOT-C", "completeAuthorizedEpochHandoff", "finalizeAuthorizedEpochHandoff"),
)
MAKE = "makeAuthorizedEpochHandoffTarget"
REPORTED = (MAKE, "prepareAuthorizedEpochHandoff", "finalizeAuthorizedEpochHandoff")

# Values of evidence 3b (ELF f859ef6), keyed by label.  The manual chain of 3b
# (38 928 B) skipped the 32 B frame of main_ui::releaseFactoryResetHold, which
# the release path of the main loop passes through; the call graph finds it, so
# the calibrated K-HOLD value is 38 928 + 32 = 38 960 B.
CALIBRATION = {
    MAKE: 17520,
    "prepareAuthorizedEpochHandoff": 12160,
    "finalizeAuthorizedEpochHandoff": 8512,
    "K-HOLD direct": 38960,
    "K-BOOT-P direct": 35744,
}


@dataclass
class Graph:
    frames: dict[str, int] = field(default_factory=dict)
    edges: dict[str, set[str]] = field(default_factory=dict)
    callx: dict[str, int] = field(default_factory=dict)
    cycles: set[str] = field(default_factory=set)
    interior_calls: int = 0


def short(name: str) -> str:
    return name if len(name) <= 110 else name[:107] + "..."


def read_graph(elf: Path, objdump: str) -> Graph:
    graph = Graph()
    process = subprocess.Popen(
        [objdump, "-d", "-C", str(elf)],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        errors="replace",
    )
    assert process.stdout is not None
    current: str | None = None
    for line in process.stdout:
        header = HEADER.match(line)
        if header:
            current = header.group(1)
            graph.edges.setdefault(current, set())
            continue
        if current is None:
            continue
        entry = ENTRY.search(line)
        if entry:
            graph.frames.setdefault(current, int(entry.group(1), 0))
            continue
        call = CALL.search(line)
        if call:
            if call.group(2):
                # Call into the middle of a symbol: shared/outlined code, not a
                # function entry; counted but not treated as a callee edge.
                graph.interior_calls += 1
            else:
                graph.edges[current].add(call.group(1))
            continue
        if CALLX.search(line):
            graph.callx[current] = graph.callx.get(current, 0) + 1
    if process.wait() != 0:
        raise SystemExit(f"objdump failed for {elf}")
    return graph


def find(graph: Graph, fragment: str) -> str:
    needle = fragment + "("
    candidates = [
        name
        for name in graph.frames
        if needle in name and "{lambda" not in name and "::operator()" not in name
    ]
    if not candidates:
        raise KeyError(fragment)
    return max(candidates, key=lambda name: graph.frames[name])


def find_optional(graph: Graph, fragment: str) -> str | None:
    try:
        return find(graph, fragment)
    except KeyError:
        return None


def longest_path(graph: Graph, start: str, target: str) -> tuple[int, list[str]] | None:
    """Largest frame sum over direct-edge paths start -> target (inclusive)."""
    reverse: dict[str, set[str]] = {}
    for source, targets in graph.edges.items():
        for name in targets:
            reverse.setdefault(name, set()).add(source)
    reaches = {target}
    pending = [target]
    while pending:
        for parent in reverse.get(pending.pop(), ()):
            if parent not in reaches:
                reaches.add(parent)
                pending.append(parent)
    if start not in reaches:
        return None

    memo: dict[str, tuple[int, list[str]]] = {}
    active: set[str] = set()

    def walk(node: str) -> tuple[int, list[str]]:
        if node in memo:
            return memo[node]
        own = graph.frames.get(node, 0)
        if node == target:
            return memo.setdefault(node, (own, [node]))
        active.add(node)
        best = (-1, [])
        for child in graph.edges.get(node, ()):
            if child not in reaches:
                continue
            if child in active:
                graph.cycles.add(child)
                continue
            depth, path = walk(child)
            if depth > best[0]:
                best = (depth, path)
        active.discard(node)
        result = (own + best[0], [node] + best[1])
        memo[node] = result
        return result

    return walk(start)


def deepest(graph: Graph, root: str) -> tuple[int, list[str]]:
    memo: dict[str, tuple[int, list[str]]] = {}
    active: set[str] = set()

    def walk(node: str) -> tuple[int, list[str]]:
        if node in memo:
            return memo[node]
        active.add(node)
        best = (0, [])
        for child in graph.edges.get(node, ()):
            if child in active:
                graph.cycles.add(child)
                continue
            depth, path = walk(child)
            if depth > best[0]:
                best = (depth, path)
        active.discard(node)
        result = (graph.frames.get(node, 0) + best[0], [node] + best[1])
        memo[node] = result
        return result

    return walk(root)


def analyse(graph: Graph, elf: Path, calibrate: bool) -> int:
    sys.setrecursionlimit(50000)
    print(f"ELF: {elf}")
    results: dict[str, int] = {}

    print("\nEinzelframes (entry a1, N):")
    names: dict[str, str | None] = {}
    for fragment in REPORTED:
        name = find_optional(graph, fragment)
        names[fragment] = name
        if name is None:
            print(f"  {fragment:<34} nicht als eigene Funktion vorhanden (inline)")
        else:
            results[fragment] = graph.frames[name]
            print(f"  {fragment:<34} {graph.frames[name]:>6} B")
    for name in sorted(graph.frames):
        if "prepareAuthorizedEpochHandoff" in name and "{lambda" in name:
            print(f"  lambda in prepare{'':<17} {graph.frames[name]:>6} B  {short(name)}")

    root = "app_main"
    if root not in graph.frames:
        raise SystemExit("app_main nicht im ELF")

    failures: list[str] = []
    print(
        f"\nStack-Budget {STACK_BUDGET} B, Reserve-Ziel {RESERVE_TARGET} B, "
        f"Kettengrenze {CHAIN_LIMIT} B"
    )
    for label, via_fragment, handoff_fragment in CHAINS:
        via = find_optional(graph, via_fragment)
        handoff = names.get(handoff_fragment) or find_optional(graph, handoff_fragment)
        print(f"\n{label}: app_main -> ... -> {via_fragment} -> {handoff_fragment}")
        if via is None or handoff is None:
            print("  FEHLER: Funktion der Kette fehlt im ELF")
            failures.append(f"{label}: Funktion fehlt")
            continue
        prefix = longest_path(graph, root, via)
        middle = longest_path(graph, via, handoff)
        if prefix is None or middle is None:
            print("  FEHLER: Kette ueber direkte Kanten nicht aufloesbar")
            failures.append(f"{label}: nicht aufloesbar")
            continue
        to_handoff = prefix[0] + middle[0] - graph.frames[via]
        path = prefix[1] + middle[1][1:]
        make = names.get(MAKE)
        make_frame = (
            graph.frames[make] if make is not None and make in graph.edges.get(handoff, ()) else 0
        )
        direct = to_handoff + make_frame
        below, below_path = deepest(graph, handoff)
        deep = to_handoff + below - graph.frames[handoff]
        reserve = STACK_BUDGET - deep
        results[f"{label} direct"] = direct
        results[f"{label} deep"] = deep
        print("  Pfad (Frame B):")
        for node in path:
            print(f"    {graph.frames.get(node, 0):>6}  {short(node)}")
        if make_frame:
            print(f"    {make_frame:>6}  {short(make or '')}  (direkter Callee)")
        print(f"  Summe direkt benannte Kette : {direct:>6} B")
        print(f"  Summe inkl. tiefstem Callee : {deep:>6} B  (Reserve {reserve} B)")
        print("  Tiefster Callee-Pfad unterhalb der Handoff-Funktion:")
        for node in below_path[1:6]:
            print(f"    {graph.frames.get(node, 0):>6}  {short(node)}")
        unresolved = [
            (graph.callx[node], node) for node in path + below_path if node in graph.callx
        ]
        print(
            f"  Unaufgeloeste callx-Kanten auf Pfad/Callee-Pfad: "
            f"{sum(count for count, _ in unresolved)} in {len(unresolved)} Funktionen"
        )
        for count, node in unresolved:
            print(f"    {count:>3} x  {short(node)}")
        if not calibrate and deep > CHAIN_LIMIT:
            failures.append(f"{label}: {deep} B > {CHAIN_LIMIT} B")
            print(f"  ERGEBNIS: UEBER Kettengrenze ({deep} > {CHAIN_LIMIT})")
        else:
            print(f"  ERGEBNIS: {'KALIBRIERUNGSLAUF' if calibrate else 'innerhalb Kettengrenze'}")

    print(f"\nCalls auf Symbol-Inneres (kein Funktionseintritt, nicht gezaehlt): {graph.interior_calls}")
    if graph.cycles:
        print(f"\nHinweis: {len(graph.cycles)} Zyklus-Kanten uebersprungen (Rekursion).")

    if calibrate:
        print("\nKalibrierung gegen Evidence 3b:")
        for label, expected in CALIBRATION.items():
            actual = results.get(label)
            state = "OK" if actual == expected else "ABWEICHUNG"
            print(f"  {label:<34} erwartet {expected:>6}  ist {actual}  {state}")
            if actual != expected:
                failures.append(f"Kalibrierung {label}: {actual} != {expected}")

    print("\nGESAMT: " + ("FAIL" if failures else "PASS"))
    for failure in failures:
        print(f"  - {failure}")
    return 1 if failures else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("elf", type=Path)
    parser.add_argument("--objdump", default=None)
    parser.add_argument("--calibrate", action="store_true")
    arguments = parser.parse_args()
    objdump = arguments.objdump or shutil.which("xtensa-esp32-elf-objdump")
    if objdump is None:
        raise SystemExit("xtensa-esp32-elf-objdump nicht gefunden (--objdump angeben)")
    graph = read_graph(arguments.elf, objdump)
    return analyse(graph, arguments.elf, arguments.calibrate)


if __name__ == "__main__":
    sys.exit(main())
