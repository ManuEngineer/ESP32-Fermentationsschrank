#!/usr/bin/env python3
"""Reproduce the Issue #90 NVS capacity lower bound and R1 selection."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
from pathlib import Path


IDF_SHA = "7101770dc6db2667b3c477cc31365dd1acd6db4e"
PAGE_SIZE = 4096
PAGE_COUNT_SELECTION = 69
RESERVE_PAGES_SELECTION = 20
KEYS = (
    (
        "configuration.user",
        ("uc0", "uc1", "uc2", "uc3"),
        301,
        "lib/fermentation_app/src/configuration_storage_contract.hpp; "
        "lib/fermentation_app/src/configuration_limits.hpp",
    ),
    (
        "configuration.service",
        ("sc0", "sc1", "sc2", "sc3"),
        45,
        "lib/fermentation_app/src/configuration_storage_contract.hpp; "
        "lib/fermentation_app/src/configuration_graph_store.cpp",
    ),
    (
        "configuration.program",
        ("pc0", "pc1", "pc2", "pc3"),
        32813,
        "lib/fermentation_app/src/configuration_storage_contract.hpp; "
        "lib/fermentation_app/src/configuration_limits.hpp",
    ),
    (
        "configuration.manifest",
        ("cm0", "cm1", "cm2"),
        149,
        "lib/fermentation_app/src/configuration_storage_contract.hpp; "
        "lib/fermentation_app/src/configuration_limits.hpp",
    ),
    (
        "configuration.root",
        ("cr0", "cr1"),
        114,
        "lib/fermentation_app/src/configuration_storage_contract.hpp; "
        "lib/fermentation_app/src/configuration_limits.hpp",
    ),
    (
        "configuration.bootstrap",
        ("cb0", "cb1"),
        42,
        "lib/fermentation_app/src/configuration_storage_contract.hpp; "
        "lib/fermentation_app/src/configuration_limits.hpp",
    ),
    (
        "run.checkpoint",
        ("rc0", "rc1"),
        8240,
        "lib/fermentation_app/src/run_persistence_store.cpp; "
        "lib/fermentation_app/src/run_persistence_coordinator.cpp",
    ),
    (
        "run.head",
        ("rh0",),
        256,
        "lib/fermentation_app/src/run_persistence_store.cpp; "
        "lib/fermentation_app/src/run_persistence_coordinator.cpp",
    ),
)


def repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def parse_define(path: Path, name: str) -> int:
    match = re.search(rf"#define\s+{re.escape(name)}\s+(\d+)", path.read_text())
    if match is None:
        raise SystemExit(f"missing {name} in {path}")
    return int(match.group(1))


def entries(size: int, entry_size: int, chunk_max: int) -> int:
    if size <= chunk_max:
        return 1 + (size + entry_size - 1) // entry_size
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
    for _, _, _, source in KEYS:
        for source_path in source.split("; "):
            if not (root / source_path).is_file():
                raise SystemExit(f"missing canonical inventory source: {source_path}")

    rows = []
    persistent = 1  # namespace entry
    for group, key_names, size, source in KEYS:
        count = len(key_names)
        per_key = entries(size, entry_size, chunk_max)
        total = count * per_key
        persistent += total
        rows.append((group, key_names, count, size, per_key, total, source))
    largest = max(size for _, _, size, _ in KEYS)
    peak = persistent + entries(largest, entry_size, chunk_max)
    minimum_entries = peak + 2 * entry_count
    minimum_pages = (minimum_entries + entry_count - 1) // entry_count
    selection_entries = PAGE_COUNT_SELECTION * entry_count
    if minimum_pages != 49 or persistent != 4768 or peak != 5804:
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
