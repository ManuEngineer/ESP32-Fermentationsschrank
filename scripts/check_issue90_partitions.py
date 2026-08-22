#!/usr/bin/env python3
"""Fail-closed 4 MB layout and NVS capacity checks for Issue #90."""

import argparse
import csv
import sys
from pathlib import Path


FLASH_END = 0x400000
PARTITION_TABLE_END = 0x9000
DATA_ALIGNMENT = 0x1000
APP_ALIGNMENT = 0x10000

# The complete table is intentional: a later CSV change must affect this
# check, including the factory app size used by the image-size gate.
EXPECTED = {
    "issue_90_state_store.csv": {
        "nvs": (0x9000, 0x6000, "data", "nvs"),
        "phy_init": (0xF000, 0x1000, "data", "phy"),
        "factory": (0x10000, 0x2F0000, "app", "factory"),
        "state_store": (0x300000, 0x100000, "data", "nvs"),
    },
    "issue_90_state_store_test.csv": {
        "nvs": (0x9000, 0x6000, "data", "nvs"),
        "phy_init": (0xF000, 0x1000, "data", "phy"),
        "factory": (0x10000, 0x2F0000, "app", "factory"),
        "state_store_test": (0x300000, 0x100000, "data", "nvs"),
    },
}

# ESP-IDF v6.0.2 NVS model used by the host backend: 32-byte entries, 126
# usable entries per 4 KiB page, and 4000-byte multi-page BLOB_DATA chunks.
# A blob consumes one BLOB_INDEX entry plus one entry per data chunk.
NVS_PAGE_SIZE = 0x1000
NVS_ENTRY_SIZE = 32
NVS_ENTRY_COUNT = 126
NVS_CHUNK_MAX_SIZE = NVS_ENTRY_SIZE * (NVS_ENTRY_COUNT - 1)
NVS_BLOB_INDEX_ENTRIES = 1
NVS_NAMESPACE_METADATA_ENTRIES = 1

CONFIGURATION_BLOB_SIZES = (
    (256 + 45,) * 4,  # uc0..uc3
    (45,) * 4,  # sc0..sc3
    (32768 + 45,) * 4,  # pc0..pc3
    (149,) * 3,  # cm0..cm2
    (114,) * 2,  # cr0..cr1
    (42,) * 2,  # cb0..cb1
)
RUN_BLOB_SIZES = (8240, 8240, 256)  # rc0/rc1/rh0

# R1 permits two model generations. During a write, the old generation, new
# candidate, and prepared transaction can coexist. These are capacity-planning
# reserves, not a claim about a guaranteed flash-wear lifetime.
NVS_TRANSACTION_GENERATION_MULTIPLIER = 3
NVS_PAGE_METADATA_ENTRIES = 2
NVS_GC_RESERVE_PAGES = 2
NVS_FRAGMENTATION_RESERVE_PAGES = 2
NVS_WEAR_HEADROOM_PAGES = 64


def nvs_blob_entries(blob_size: int) -> int:
    chunks = (blob_size + NVS_CHUNK_MAX_SIZE - 1) // NVS_CHUNK_MAX_SIZE
    return NVS_BLOB_INDEX_ENTRIES + max(chunks, 1)


def capacity_model() -> dict[str, int]:
    configuration = tuple(
        blob_size
        for group in CONFIGURATION_BLOB_SIZES
        for blob_size in group
    )
    logical_entries = NVS_NAMESPACE_METADATA_ENTRIES + sum(
        nvs_blob_entries(size) for size in configuration + RUN_BLOB_SIZES
    )
    transaction_entries = (
        logical_entries * NVS_TRANSACTION_GENERATION_MULTIPLIER
        + NVS_PAGE_METADATA_ENTRIES
    )
    data_pages = (transaction_entries + NVS_ENTRY_COUNT - 1) // NVS_ENTRY_COUNT
    minimum_pages = (
        data_pages
        + NVS_GC_RESERVE_PAGES
        + NVS_FRAGMENTATION_RESERVE_PAGES
        + NVS_WEAR_HEADROOM_PAGES
    )
    return {
        "configuration_records": len(configuration),
        "run_records": len(RUN_BLOB_SIZES),
        "logical_entries": logical_entries,
        "transaction_entries": transaction_entries,
        "data_pages": data_pages,
        "minimum_pages": minimum_pages,
        "minimum_bytes": minimum_pages * NVS_PAGE_SIZE,
    }


CAPACITY = capacity_model()
DERIVED_MINIMUM_STATE_STORE = CAPACITY["minimum_bytes"]


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


def validate_layout(
    path: Path, partitions: dict[str, tuple[int, int, str, str]]
) -> None:
    expected = EXPECTED[path.name]
    if set(partitions) != set(expected):
        raise ValueError(
            f"{path}: unerwartete Partitionlabels; erwartet {sorted(expected)}, "
            f"gefunden {sorted(partitions)}"
        )
    for label, expected_entry in expected.items():
        actual = partitions[label]
        if actual != expected_entry:
            raise ValueError(
                f"{path}: {label} erwartet {expected_entry}, gefunden {actual}"
            )
        start, size, kind, subtype = actual
        alignment = APP_ALIGNMENT if kind == "app" else DATA_ALIGNMENT
        if start % alignment or size % alignment:
            raise ValueError(
                f"{path}: {label} verletzt {alignment:#x}-Alignment"
            )
        if start + size > FLASH_END:
            raise ValueError(f"{path}: {label} endet ausserhalb des 4-MB-Raums")
        if kind == "app" and subtype != "factory":
            raise ValueError(f"{path}: Apppartition {label} hat falschen Subtyp")
        if kind == "data" and subtype not in {"nvs", "phy"}:
            raise ValueError(f"{path}: Datenpartition {label} hat falschen Subtyp")

    nvs_label = "state_store" if "state_store" in partitions else "state_store_test"
    nvs_start, nvs_size, _, _ = partitions[nvs_label]
    if nvs_start + nvs_size != FLASH_END:
        raise ValueError(f"{path}: {nvs_label} endet nicht bei 0x{FLASH_END:X}")
    if nvs_size < DERIVED_MINIMUM_STATE_STORE:
        raise ValueError(
            f"{path}: {nvs_label} ist kleiner als der abgeleitete Bedarf "
            f"0x{DERIVED_MINIMUM_STATE_STORE:X}"
        )
    if "state_store" in partitions and "state_store_test" in partitions:
        raise ValueError(f"{path}: Produkt- und Testpartition vermischt")


def check(path: Path) -> None:
    validate_layout(path, read_partitions(path))


def check_built_app_size_values(
    factory_size: int, image_size: int, profile: str
) -> None:
    if image_size > factory_size:
        raise ValueError(
            f"{profile}-App {image_size} > deklarierte factory-Groesse "
            f"{factory_size}"
        )


def check_built_app_sizes(root: Path) -> None:
    production = read_partitions(root / "partitions" / "issue_90_state_store.csv")
    factory_start, factory_size, factory_kind, factory_subtype = production[
        "factory"
    ]
    if (factory_kind, factory_subtype) != ("app", "factory"):
        raise ValueError("factory aus CSV ist keine app,factory-Partition")
    del factory_start
    for profile in ("esp32_bringup", "esp32_release"):
        image = root / "build" / profile / "esp32_fermentationsschrank.bin"
        if not image.is_file():
            raise ValueError(f"fehlendes gebautes {profile}-App-Image: {image}")
        image_size = image.stat().st_size
        check_built_app_size_values(factory_size, image_size, profile)
        print(f"PASS: {profile} app image {image_size} <= factory {factory_size}")


def _expect_failure(description: str, callback) -> None:
    try:
        callback()
    except (OSError, ValueError):
        print(f"PASS: layout selftest rejects {description}")
        return
    raise AssertionError(f"layout selftest accepted {description}")


def run_self_tests() -> None:
    from copy import deepcopy

    path = Path("issue_90_state_store.csv")
    baseline = EXPECTED[path.name]
    _expect_failure(
        "shifted factory",
        lambda: validate_layout(
            path,
            {**baseline, "factory": (0x11000, 0x2EF000, "app", "factory")},
        ),
    )
    _expect_failure(
        "undersized factory image",
        lambda: check_built_app_size_values(0x1000, 0x1001, "selftest"),
    )
    _expect_failure(
        "misaligned factory",
        lambda: validate_layout(
            path,
            {**baseline, "factory": (0x10001, 0x2EFFFF, "app", "factory")},
        ),
    )
    _expect_failure(
        "wrong factory type",
        lambda: validate_layout(
            path,
            {**baseline, "factory": (0x10000, 0x2F0000, "data", "nvs")},
        ),
    )
    mixed = deepcopy(baseline)
    mixed["state_store_test"] = mixed["state_store"]
    _expect_failure(
        "product/test partition mixture", lambda: validate_layout(path, mixed)
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--require-built-apps",
        action="store_true",
        help="fail closed if both ESP-IDF app images are missing or too large",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="run fail-closed layout and app-size negative self-tests",
    )
    arguments = parser.parse_args()
    if arguments.self_test:
        run_self_tests()
    root = Path(__file__).resolve().parent.parent
    for filename in EXPECTED:
        check(root / "partitions" / filename)
        print(
            f"PASS: {filename} 4-MB layout, isolation and NVS model "
            f"(0x{DERIVED_MINIMUM_STATE_STORE:X}, "
            f"{CAPACITY['transaction_entries']} entries, "
            f"{CAPACITY['minimum_pages']} pages)"
        )
    if arguments.require_built_apps:
        check_built_app_sizes(root)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
