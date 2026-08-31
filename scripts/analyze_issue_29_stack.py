#!/usr/bin/env python3
"""Evaluate the compiled #29 ESP32 bring-up probe call path.

This is intentionally a small, build-specific check rather than a general
stack-analysis framework.  GCC ``-fstack-usage`` reports one frame per
function; the ``-fcallgraph-info=su`` artefacts prove the non-inlined edges
used by the deterministic probe fault path.  The script sums only the frames
which are simultaneously live on that path and rejects dynamic/unbounded
qualifiers.
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Frame:
    name: str
    source: str
    bytes: int
    qualifier: str


FRAME_PATTERN = re.compile(r"\t(?P<bytes>\d+)\t(?P<qualifier>[^\t]+)$")
LOCATION_PATTERN = re.compile(r"^(?P<source>.*?):\d+:\d+:(?P<name>.*)$")
EDGE_PATTERN = re.compile(
    r'sourcename: "(?P<source>[^"]+)" targetname: "(?P<target>[^"]+)"'
)
NODE_TITLE_PATTERN = re.compile(r'node: \{ title: "(?P<title>[^"]+)"')
NODE_LABEL_PATTERN = re.compile(r'label: "(?P<label>[^"]*)"')


# These are the actual functions held simultaneously for the probe's
# deterministic candidate-allocation-failure path.  The final result() call
# is nested below persistCommand before that call returns.
CHAIN = (
    ("probeTask(void*)", "probeTask"),
    ("runProbe(ProbeContext&)", "runProbe"),
    (
        "TemperatureControlApplicationOrchestrator::persistFreshStartCommand(",
        "persistFreshStartCommand",
    ),
    (
        "TemperatureControlApplicationOrchestrator::persistCommand(",
        "orchestrator.persistCommand",
    ),
    (
        "RunPersistenceCoordinator::persistCommand(",
        "coordinator.persistCommand",
    ),
    ("RunPersistenceCoordinator::result(", "coordinator.result"),
)


def read_frames(build_dir: Path) -> list[Frame]:
    frames: list[Frame] = []
    for path in sorted(build_dir.rglob("*.su")):
        for line in path.read_text(encoding="utf-8").splitlines():
            match = FRAME_PATTERN.search(line)
            if match is None:
                continue
            signature = line[: match.start()].strip()
            location = LOCATION_PATTERN.match(signature)
            if location is None:
                raise SystemExit(f"cannot parse .su location: {signature}")
            frames.append(
                Frame(
                    name=location.group("name").strip(),
                    source=location.group("source"),
                    bytes=int(match.group("bytes")),
                    qualifier=match.group("qualifier").strip(),
                )
            )
    return frames


def read_callgraph(build_dir: Path) -> tuple[dict[str, str], set[tuple[str, str]]]:
    symbols: dict[str, str] = {}
    edges: set[tuple[str, str]] = set()
    for path in sorted(build_dir.rglob("*.ci")):
        for line in path.read_text(encoding="utf-8").splitlines():
            node = NODE_TITLE_PATTERN.search(line)
            if node is not None:
                label = NODE_LABEL_PATTERN.search(line)
                if label is not None:
                    symbols[label.group("label").split("\\n", 1)[0]] = node.group(
                        "title"
                    )
            edge = EDGE_PATTERN.search(line)
            if edge is not None:
                edges.add((edge.group("source"), edge.group("target")))
    return symbols, edges


def select_frame(frames: list[Frame], needle: str) -> Frame:
    matches = [frame for frame in frames if needle in frame.name]
    if len(matches) != 1:
        names = ", ".join(frame.name for frame in matches)
        raise SystemExit(
            f"expected exactly one .su frame for {needle!r}, found "
            f"{len(matches)}: {names}"
        )
    return matches[0]


def symbol_for(symbols: dict[str, str], needle: str) -> str:
    matches = [symbol for label, symbol in symbols.items() if needle in label]
    if len(matches) != 1:
        raise SystemExit(
            f"expected exactly one callgraph symbol for {needle!r}, found "
            f"{len(matches)}"
        )
    return matches[0]


def require_edge(
    symbols: dict[str, str], edges: set[tuple[str, str]], source: str, target: str
) -> None:
    source_symbol = symbol_for(symbols, source)
    target_symbol = symbol_for(symbols, target)
    if (source_symbol, target_symbol) not in edges:
        raise SystemExit(f"missing compiled callgraph edge: {source} -> {target}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "build_dir", type=Path, help="ESP-IDF esp32_bringup build directory"
    )
    arguments = parser.parse_args()
    build_dir = arguments.build_dir
    frames = read_frames(build_dir)
    symbols, edges = read_callgraph(build_dir)

    selected = [select_frame(frames, needle) for needle, _ in CHAIN]
    for frame in selected:
        if frame.qualifier != "static":
            raise SystemExit(
                f"unbounded or non-static stack qualifier for {frame.name}: "
                f"{frame.qualifier}"
            )

    for source, target in zip(CHAIN, CHAIN[1:]):
        require_edge(symbols, edges, source[0], target[0])

    cumulative = 0
    for frame, (_, label) in zip(selected, CHAIN):
        cumulative += frame.bytes
        print(
            f"{label}: frame={frame.bytes} bytes qualifier={frame.qualifier} "
            f"cumulative={cumulative} bytes source={frame.source}"
        )

    safety_buffer = 4096
    configured = ((cumulative + safety_buffer + 1023) // 1024) * 1024
    print(f"cumulative_call_path_bytes={cumulative}")
    print(f"safety_buffer_bytes={safety_buffer}")
    print(f"configured_task_stack_bytes={configured}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
