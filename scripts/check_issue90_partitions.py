#!/usr/bin/env python3
"""Fail-closed 4 MB layout check for the Issue #90 NVS partitions."""

import csv
import sys
from pathlib import Path


FLASH_END = 0x400000
PARTITION_TABLE_END = 0x9000
EXPECTED = {
    "issue_90_state_store.csv": {"state_store": (0x1D0000, 0x230000)},
    "issue_90_state_store_test.csv": {"state_store_test": (0x1D0000, 0x230000)},
}


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
    if "state_store" in partitions and "state_store_test" in partitions:
        raise ValueError(f"{path}: Produkt- und Testpartition vermischt")
    if partitions["factory"][2:] != ("app", "factory"):
        raise ValueError(f"{path}: factory hat falschen Typ")


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    for filename in EXPECTED:
        check(root / "partitions" / filename)
        print(f"PASS: {filename} 4-MB layout and partition isolation")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
