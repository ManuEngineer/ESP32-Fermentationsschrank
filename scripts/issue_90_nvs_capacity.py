#!/usr/bin/env python3
"""Derive and cross-check the Issue #90 NVS capacity inventory."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path


IDF_SHA = "7101770dc6db2667b3c477cc31365dd1acd6db4e"
PAGE_SIZE = 4096
PAGE_COUNT_SELECTION = 69
RESERVE_PAGES_SELECTION = 20
RUN_PERSISTENCE_KEYS = ("rc0", "rc1", "rh0")


@dataclass(frozen=True)
class RecordGroup:
    name: str
    key_symbol: str
    key_source: str
    size_source: str
    size_constant: str | None = None
    size_mode: str = "constant"


GROUPS = (
    RecordGroup(
        "configuration.user",
        "kUserConfigurationSlotKeys",
        "lib/fermentation_app/src/configuration_storage_contract.hpp",
        "lib/fermentation_app/src/configuration_limits.hpp",
        "kMaximumUserConfigurationPayloadBytes",
        "envelope_plus_service",
    ),
    RecordGroup(
        "configuration.service",
        "kServiceConfigurationSlotKeys",
        "lib/fermentation_app/src/configuration_storage_contract.hpp",
        "lib/fermentation_app/src/configuration_graph_store.cpp",
        size_mode="graph_service",
    ),
    RecordGroup(
        "configuration.program",
        "kProgramCatalogSlotKeys",
        "lib/fermentation_app/src/configuration_storage_contract.hpp",
        "lib/fermentation_app/src/configuration_limits.hpp",
        "kMaximumProgramCatalogPayloadBytes",
        "envelope_plus_service",
    ),
    RecordGroup(
        "configuration.manifest",
        "kConfigurationManifestSlotKeys",
        "lib/fermentation_app/src/configuration_storage_contract.hpp",
        "lib/fermentation_app/src/configuration_limits.hpp",
        "kMaximumConfigurationManifestEnvelopeBytes",
    ),
    RecordGroup(
        "configuration.root",
        "kConfigurationRootSlotKeys",
        "lib/fermentation_app/src/configuration_storage_contract.hpp",
        "lib/fermentation_app/src/configuration_limits.hpp",
        "kMaximumConfigurationRootEnvelopeBytes",
    ),
    RecordGroup(
        "configuration.bootstrap",
        "kConfigurationBootstrapSlotKeys",
        "lib/fermentation_app/src/configuration_storage_contract.hpp",
        "lib/fermentation_app/src/configuration_limits.hpp",
        "kMaximumConfigurationBootstrapEnvelopeBytes",
    ),
    RecordGroup(
        "run.checkpoint",
        "run-persistence-literal:rc0,rc1",
        "lib/fermentation_app/src/run_persistence_store.cpp",
        "lib/fermentation_app/src/run_persistence_coordinator.cpp; "
        "lib/fermentation_app/src/run_persistence_codec.cpp",
        "kMaximumCheckpointRecordBytes",
    ),
    RecordGroup(
        "run.head",
        "run-persistence-literal:rh0",
        "lib/fermentation_app/src/run_persistence_store.cpp",
        "lib/fermentation_app/src/run_persistence_coordinator.cpp; "
        "lib/fermentation_app/src/run_persistence_codec.cpp",
        "kMaximumHeadRecordBytes",
    ),
)


def repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def read_source(root: Path, relative: str) -> str:
    path = root / relative
    if not path.is_file():
        raise SystemExit(f"missing canonical inventory source: {relative}")
    return path.read_text()


def parse_define(path: Path, name: str) -> int:
    match = re.search(rf"#define\s+{re.escape(name)}\s+(\d+)", path.read_text())
    if match is None:
        raise SystemExit(f"missing {name} in {path}")
    return int(match.group(1))


def parse_cpp_constant(root: Path, relative_sources: str, name: str) -> int:
    values: list[int] = []
    for relative in (part.strip() for part in relative_sources.split(";")):
        path = root / relative
        text = read_source(root, relative)
        match = re.search(
            rf"\b{re.escape(name)}\b\s*=\s*(\d+)\s*U?\s*;", text
        )
        if match is None:
            raise SystemExit(f"missing C++ constant {name} in {path}")
        values.append(int(match.group(1)))
    if not values or len(set(values)) != 1:
        raise SystemExit(
            f"inconsistent C++ constant {name} in {relative_sources}: {values}"
        )
    return values[0]


def parse_key_array(root: Path, relative: str, symbol: str) -> tuple[str, ...]:
    text = read_source(root, relative)
    match = re.search(rf"\b{re.escape(symbol)}\b\s*\{{(.*?)\}}", text, re.S)
    if match is None:
        raise SystemExit(f"missing key inventory {symbol} in {relative}")
    keys = tuple(re.findall(r'"([a-z0-9]+)"', match.group(1)))
    if not keys:
        raise SystemExit(f"empty key inventory {symbol} in {relative}")
    return keys


def parse_service_size(root: Path) -> int:
    text = read_source(root, "lib/fermentation_app/src/configuration_graph_store.cpp")
    values = [
        int(value)
        for value in re.findall(
            r"kServiceConfigurationSlotKeys\s*,\s*[^;]{0,240}?\b(\d+)U\s*\)",
            text,
            re.S,
        )
    ]
    if not values or len(set(values)) != 1 or values[0] != 45:
        raise SystemExit(f"service record size is not consistently 45: {values}")
    return values[0]


def parse_literal_keys(root: Path, literal: str) -> tuple[str, ...]:
    source = read_source(root, "lib/fermentation_app/src/run_persistence_store.cpp")
    expected = tuple(literal.split(":", 1)[1].split(","))
    found = tuple(re.findall(r'create\("([a-z0-9]+)"\)', source))
    if found != RUN_PERSISTENCE_KEYS:
        raise SystemExit(
            "run persistence key inventory drift: "
            f"expected exact order/set {RUN_PERSISTENCE_KEYS}, found {found}"
        )
    return expected


def entries(size: int, entry_size: int, chunk_max: int) -> int:
    if size <= chunk_max:
        return 1 + (size + entry_size - 1) // entry_size + 1
    chunks, remainder = divmod(size, chunk_max)
    result = chunks * (1 + chunk_max // entry_size)
    if remainder:
        result += 1 + (remainder + entry_size - 1) // entry_size
    return result + 1


def current_sha(root: Path) -> str:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=root, check=True, text=True,
        capture_output=True,
    )
    return result.stdout.strip()


def group_inventory(root: Path) -> list[tuple[str, tuple[str, ...], int, str]]:
    inventory = []
    for group in GROUPS:
        if group.key_symbol.startswith("run-persistence-literal:"):
            keys = parse_literal_keys(root, group.key_symbol)
        else:
            keys = parse_key_array(root, group.key_source, group.key_symbol)
        if group.size_mode == "graph_service":
            size = parse_service_size(root)
        else:
            size = parse_cpp_constant(
                root, group.size_source, group.size_constant or ""
            )
            if group.size_mode == "envelope_plus_service":
                graph = read_source(root, "lib/fermentation_app/src/configuration_graph_store.cpp")
                expression = (
                    rf"configuration_limits::{re.escape(group.size_constant or '')}"
                    r"\s*\+\s*45U"
                )
                if not re.search(expression, graph):
                    raise SystemExit(
                        f"{group.name} is not consumed as limit + 45U in configuration_graph_store.cpp"
                    )
                size += 45
        inventory.append((group.name, keys, size, group.key_source + "; " + group.size_source))
    return inventory


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    parser.add_argument("--source-git-sha")
    args = parser.parse_args()
    root = repository_root()
    idf_value = os.environ.get("IDF_PATH")
    if not idf_value:
        raise SystemExit("IDF_PATH must point to the pinned ESP-IDF checkout")
    idf_path = Path(idf_value)
    constants = idf_path / "components/nvs_flash/private_include/nvs_constants.h"
    if not constants.is_file():
        raise SystemExit(f"missing pinned NVS constants: {constants}")
    entry_size = parse_define(constants, "NVS_CONST_ENTRY_SIZE")
    entry_count = parse_define(constants, "NVS_CONST_ENTRY_COUNT")
    chunk_max = entry_size * (entry_count - 1)
    if (entry_size, entry_count, chunk_max) != (32, 126, 4000):
        raise SystemExit("unexpected ESP-IDF v6.0.2 NVS constants")
    try:
        idf_sha = current_sha(idf_path)
    except (subprocess.CalledProcessError, FileNotFoundError) as error:
        raise SystemExit(f"IDF_PATH is not a git checkout: {idf_path}") from error
    if idf_sha != IDF_SHA:
        raise SystemExit(f"IDF_PATH SHA mismatch: expected {IDF_SHA}, got {idf_sha}")

    inventory = group_inventory(root)
    rows = []
    persistent = 1
    for group, key_names, size, source in inventory:
        count = len(key_names)
        per_key = entries(size, entry_size, chunk_max)
        total = count * per_key
        persistent += total
        rows.append((group, key_names, count, size, per_key, total, source))
    largest = max(size for _, _, size, _ in inventory)
    peak = persistent + entries(largest, entry_size, chunk_max)
    minimum_entries = peak + 2 * entry_count
    minimum_pages = (minimum_entries + entry_count - 1) // entry_count
    selection_entries = PAGE_COUNT_SELECTION * entry_count
    if minimum_pages != 49 or persistent != 4784 or peak != 5820:
        raise SystemExit(
            f"capacity regression: persistent={persistent} peak={peak} "
            f"minimum_pages={minimum_pages}"
        )
    if PAGE_COUNT_SELECTION != minimum_pages + RESERVE_PAGES_SELECTION:
        raise SystemExit("selection/reserve arithmetic changed")
    if selection_entries <= minimum_entries:
        raise SystemExit("selected partition does not cover the deterministic minimum")

    source_sha = args.source_git_sha or current_sha(root)
    lines = [
        "# Issue #90 NVS capacity evidence",
        "",
        f"- source git SHA: `{source_sha}`",
        f"- ESP-IDF: `v6.0.2 @ {IDF_SHA}`",
        f"- pinned NVS constants: `{entry_size}` B/entry, `{entry_count}` entries/page, `{chunk_max}` B/chunk",
        "- status: arithmetic evidence only; real NVS statistics and GC remain hardware/BDL evidence",
        "- inventory provenance: keys and limits are mechanically parsed from the canonical sources below; contract drift fails this command",
        "",
        "| Record group | Key inventory | Slots | Max bytes | Entries/record | Entries | Canonical sources (keys / limits) |",
        "|---|---|---:|---:|---:|---:|---|",
    ]
    lines.extend(
        f"| {group} | `{', '.join(key_names)}` | {count} | {size} | {per_key} | {total} | `{source}` |"
        for group, key_names, count, size, per_key, total, source in rows
    )
    lines.extend(
        [
            "",
            f"Persistent recordbestand (without namespace): `{persistent - 1}` entries.",
            f"Persistent inventory including namespace: `{persistent}` entries.",
            f"Peak with one simultaneous `{largest}`-byte replacement: `{peak}` entries.",
            f"Two free pages for update/GC reserve: `{2 * entry_count}` entries.",
            f"Deterministic minimum: `{minimum_entries}` entries = `{minimum_pages}` pages = `{minimum_pages * PAGE_SIZE // 1024}` KiB.",
            f"Selected R1 adapter partition: `{PAGE_COUNT_SELECTION}` pages = `{PAGE_COUNT_SELECTION * PAGE_SIZE // 1024}` KiB; `{RESERVE_PAGES_SELECTION}` pages remain above the computed minimum.",
            "",
            "The existing 24 KiB baseline is smaller than the single 32,813-byte program record before NVS metadata/chunking; it cannot host that record. The selected size is not a claim about unrelated production reservations. The report must be followed by real partition, readback, GC/erase, resource and flash verification.",
            "",
        ]
    )
    output = "\n".join(lines)
    if args.output:
        target = args.output if args.output.is_absolute() else root / args.output
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(output + "\n")
    else:
        print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
