#!/usr/bin/env python3
"""Fail-closed 4 MB layout check for the Issue #90 NVS partitions."""

import argparse
import csv
import sys
from pathlib import Path


FLASH_END = 0x400000
PARTITION_TABLE_END = 0x9000
EXPECTED = {
    "issue_90_state_store.csv": {"state_store": (0x300000, 0x100000)},
    "issue_90_state_store_test.csv": {"state_store_test": (0x300000, 0x100000)},
}

# Derived from the current canonical production limits, not from leftover
# flash. The input budgets are configuration_limits.hpp and
# run_persistence_contract.hpp; the NVS page/entry constants are the pinned
# v6.0.2 private constants (4096-byte pages, 32-byte entries, 126 entries,
# 4000-byte multi-page chunk maximum).
NVS_PAGE_SIZE = 0x1000
NVS_ENTRY_SIZE = 32
NVS_ENTRY_COUNT = 126
NVS_CHUNK_MAX_SIZE = NVS_ENTRY_SIZE * (NVS_ENTRY_COUNT - 1)
CONFIGURATION_RECORD_BYTES = (
    4 * (256 + 45)  # uc0..uc3, payload plus envelope budget
    + 4 * 45  # sc0..sc3, empty service payload plus envelope
    + 4 * (32768 + 45)  # pc0..pc3
    + 3 * 149  # cm0..cm2
    + 2 * 114  # cr0..cr1
    + 2 * 42  # cb0..cb1
)
RUN_RECORD_BYTES = 2 * 8240 + 256  # rc0/rc1/rh0
TWO_GENERATION_WRITE_RESERVE = 2
GC_RESERVE_PAGES = 16
WEAR_HEADROOM_PAGES = 64
DERIVED_MINIMUM_STATE_STORE = (
    (
        (CONFIGURATION_RECORD_BYTES + RUN_RECORD_BYTES)
        * TWO_GENERATION_WRITE_RESERVE
        + (GC_RESERVE_PAGES + WEAR_HEADROOM_PAGES) * NVS_PAGE_SIZE
        + NVS_PAGE_SIZE
        - 1
    )
    // NVS_PAGE_SIZE
    * NVS_PAGE_SIZE
)


def read_partitions(path: Path) -> dict[str, tuple[int, int, str, str]]:
    partitions: dict[str, tuple[int, int, str, str]] = {}
    with path.open(newline="", encoding="utf-8") as stream:
        for row in csv.reader(stream):
            if not row or row[0].lstrip().startswith("#"):
                continue
            if len(row) < 5:
                raise ValueError(f"{path}: unvollstaendige Zeile {row!r}")
            name, kind, subtype, offset, size = (item.strip() for item in row[:5])
            if not name or name in partitions:
                raise ValueError(f"{path}: doppeltes/Leeres Label {name!r}")
            start = int(offset, 0)
            length = int(size, 0)
            if length <= 0:
                raise ValueError(f"{path}: ungueltige Groesse fuer {name}")
            end = start + length
            if start < PARTITION_TABLE_END or end > FLASH_END:
                raise ValueError(f"{path}: {name} ausserhalb des 4-MB-Raums")
            partitions[name] = (start, length, kind, subtype)
    ordered = sorted(partitions.items(), key=lambda item: item[1][0])
    for (_, left), (_, right) in zip(ordered, ordered[1:]):
        if left[0] + left[1] > right[0]:
            raise ValueError(f"{path}: Partitionen ueberlappen")
    return partitions


def check(path: Path) -> None:
    partitions = read_partitions(path)
    for label, (start, size) in EXPECTED[path.name].items():
        actual = partitions.get(label)
        if actual is None or actual[:2] != (start, size):
            raise ValueError(
                f"{path}: {label} erwartet start=0x{start:X}, size=0x{size:X}, "
                f"gefunden {actual}"
            )
        if actual[2:] != ("data", "nvs"):
            raise ValueError(f"{path}: {label} ist keine data,nvs-Partition")
        if start + size != FLASH_END:
            raise ValueError(f"{path}: {label} endet nicht bei 0x{FLASH_END:X}")
        if size < DERIVED_MINIMUM_STATE_STORE:
            raise ValueError(
                f"{path}: {label} ist kleiner als der abgeleitete Bedarf "
                f"0x{DERIVED_MINIMUM_STATE_STORE:X}"
            )
    if "state_store" in partitions and "state_store_test" in partitions:
        raise ValueError(f"{path}: Produkt- und Testpartition vermischt")
    if partitions["factory"][2:] != ("app", "factory"):
        raise ValueError(f"{path}: factory hat falschen Typ")


def check_built_app_sizes(root: Path) -> None:
    factory_size = EXPECTED["issue_90_state_store.csv"]["state_store"][0] - 0x10000
    for profile in ("esp32_bringup", "esp32_release"):
        image = root / "build" / profile / "esp32_fermentationsschrank.bin"
        if not image.is_file():
            raise ValueError(f"fehlendes gebautes {profile}-App-Image: {image}")
        image_size = image.stat().st_size
        if image_size > factory_size:
            raise ValueError(
                f"{profile}-App {image_size} > deklarierte factory-Groesse "
                f"{factory_size}"
            )
        print(
            f"PASS: {profile} app image {image_size} <= factory {factory_size}"
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--require-built-apps",
        action="store_true",
        help="fail closed if both ESP-IDF app images are missing or too large",
    )
    arguments = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    for filename in EXPECTED:
        check(root / "partitions" / filename)
        print(
            f"PASS: {filename} 4-MB layout, isolation and derived state-store "
            f"budget (0x{DERIVED_MINIMUM_STATE_STORE:X})"
        )
    if arguments.require_built_apps:
        check_built_app_sizes(root)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
