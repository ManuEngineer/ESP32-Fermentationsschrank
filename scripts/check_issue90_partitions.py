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

# ESP-IDF v6.0.2 NVS model used by the host backend. Page::writeItem() stores
# one variable-length metadata entry and ceil(chunk_size / ENTRY_SIZE) data
# entries for each BLOB_DATA chunk. writeMultiPageBlob() stores one separate
# BLOB_IDX entry after all chunks have been written.
NVS_PAGE_SIZE = 0x1000
NVS_ENTRY_SIZE = 32
NVS_ENTRY_COUNT = 126
NVS_CHUNK_MAX_SIZE = NVS_ENTRY_SIZE * (NVS_ENTRY_COUNT - 1)
NVS_BLOB_INDEX_ENTRIES = 1
NVS_NAMESPACE_METADATA_ENTRIES = 1
NVS_PAGE_HEADER_BYTES = 32
NVS_PAGE_ENTRY_TABLE_BYTES = 32

CONFIGURATION_BLOB_SIZES = (
    (256 + 45,) * 4,  # uc0..uc3
    (45,) * 4,  # sc0..sc3
    (32768 + 45,) * 4,  # pc0..pc3
    (149,) * 3,  # cm0..cm2
    (114,) * 2,  # cr0..cr1
    (42,) * 2,  # cb0..cb1
)
RUN_BLOB_SIZES = (8240, 8240, 256)  # rc0/rc1/rh0

# Two pages keep a destination page and a live source page available while
# NVS compacts one update; two more pages cover fragmentation across the
# simultaneous configuration/run mutation set. These are technical GC/update
# reserves, separate from the planning-only wear headroom below.
NVS_GC_RESERVE_PAGES = 2
NVS_FRAGMENTATION_RESERVE_PAGES = 2
# Planning headroom for future R1 overwrite/erase cycles. It is reported
# separately and is not part of the technical minimum.
NVS_WEAR_HEADROOM_PAGES = 64


def blob_data_chunks(blob_size: int) -> tuple[int, ...]:
    if blob_size == 0:
        return (0,)
    chunks: list[int] = []
    remaining = blob_size
    while remaining:
        chunk_size = min(remaining, NVS_CHUNK_MAX_SIZE)
        chunks.append(chunk_size)
        remaining -= chunk_size
    return tuple(chunks)


def blob_data_entries(chunk_size: int) -> int:
    data_entries = (chunk_size + NVS_ENTRY_SIZE - 1) // NVS_ENTRY_SIZE
    return 1 + data_entries


def nvs_blob_entries(blob_size: int) -> int:
    return NVS_BLOB_INDEX_ENTRIES + sum(
        blob_data_entries(chunk_size) for chunk_size in blob_data_chunks(blob_size)
    )


def legacy_blob_entries(blob_size: int) -> int:
    """The pre-correction formula, retained only for a regression assertion."""
    chunk_count = (blob_size + NVS_CHUNK_MAX_SIZE - 1) // NVS_CHUNK_MAX_SIZE
    return NVS_BLOB_INDEX_ENTRIES + max(chunk_count, 1)


def capacity_model() -> dict[str, int]:
    configuration = tuple(
        blob_size
        for group in CONFIGURATION_BLOB_SIZES
        for blob_size in group
    )
    full_slot_keyspace_entries = NVS_NAMESPACE_METADATA_ENTRIES + sum(
        nvs_blob_entries(size) for size in configuration + RUN_BLOB_SIZES
    )
    # A same-key update keeps the old value until the new index is committed.
    # The complete old slot set is already represented above; only the largest
    # additional candidate blob is transiently co-resident. No other
    # transaction-wide duplicate is proven by the current production path.
    same_key_update_transient_entries = max(
        nvs_blob_entries(size) for size in configuration + RUN_BLOB_SIZES
    )
    other_proven_transient_entries = 0
    technical_minimum_entries = (
        full_slot_keyspace_entries
        + same_key_update_transient_entries
        + other_proven_transient_entries
    )
    technical_minimum_pages = (
        technical_minimum_entries + NVS_ENTRY_COUNT - 1
    ) // NVS_ENTRY_COUNT
    gc_fragmentation_reserve_pages = (
        NVS_GC_RESERVE_PAGES + NVS_FRAGMENTATION_RESERVE_PAGES
    )
    planned_state_store_pages = (
        technical_minimum_pages
        + gc_fragmentation_reserve_pages
        + NVS_WEAR_HEADROOM_PAGES
    )
    return {
        "configuration_records": len(configuration),
        "run_records": len(RUN_BLOB_SIZES),
        "full_slot_keyspace_entries": full_slot_keyspace_entries,
        "same_key_update_transient_entries": same_key_update_transient_entries,
        "other_proven_transient_entries": other_proven_transient_entries,
        "technical_minimum_entries": technical_minimum_entries,
        "technical_minimum_pages": technical_minimum_pages,
        "technical_minimum_bytes": technical_minimum_pages * NVS_PAGE_SIZE,
        "gc_fragmentation_reserve_pages": gc_fragmentation_reserve_pages,
        "wear_headroom_pages": NVS_WEAR_HEADROOM_PAGES,
        "planned_state_store_pages": planned_state_store_pages,
        "planned_state_store_bytes": planned_state_store_pages * NVS_PAGE_SIZE,
        "planned_headroom_pages": 0x100000 // NVS_PAGE_SIZE - planned_state_store_pages,
    }


CAPACITY = capacity_model()
DERIVED_MINIMUM_STATE_STORE = CAPACITY["planned_state_store_bytes"]


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

    boundary_expectations = {
        0: 2,
        1: 3,
        31: 3,
        32: 3,
        33: 4,
        NVS_CHUNK_MAX_SIZE - 1: 127,
        NVS_CHUNK_MAX_SIZE: 127,
        NVS_CHUNK_MAX_SIZE + 1: 129,
        8240: 262,
        32768 + 45: 1036,
    }
    for blob_size, expected_entries in boundary_expectations.items():
        actual_entries = nvs_blob_entries(blob_size)
        if actual_entries != expected_entries:
            raise AssertionError(
                f"BLOB-Grenze {blob_size}: erwartet {expected_entries}, "
                f"gefunden {actual_entries}"
            )
    if nvs_blob_entries(8240) <= legacy_blob_entries(8240):
        raise AssertionError("Selftest erkennt die alte BLOB-Entry-Formel nicht")

    capacity = capacity_model()
    if capacity["technical_minimum_entries"] != (
        capacity["full_slot_keyspace_entries"]
        + capacity["same_key_update_transient_entries"]
        + capacity["other_proven_transient_entries"]
    ):
        raise AssertionError("transienter Entrybedarf doppelt oder falsch gezählt")
    if capacity["full_slot_keyspace_entries"] >= capacity["technical_minimum_entries"]:
        raise AssertionError("Same-Key-Transientenbedarf fehlt")
    if capacity["planned_state_store_pages"] >= 0x100000 // NVS_PAGE_SIZE:
        raise AssertionError("Kapazitätsmodell lässt keinen Headroom")
    print("PASS: NVS-Entry-/Chunk-Grenztests und alte Formel erkannt")


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
            f"(technical=0x{CAPACITY['technical_minimum_bytes']:X}, "
            f"planned=0x{CAPACITY['planned_state_store_bytes']:X})"
        )
    print(f"full_slot_keyspace_entries={CAPACITY['full_slot_keyspace_entries']}")
    print(
        "same_key_update_transient_entries="
        f"{CAPACITY['same_key_update_transient_entries']}"
    )
    print(
        "other_proven_transient_entries="
        f"{CAPACITY['other_proven_transient_entries']}"
    )
    print(f"technical_minimum_entries={CAPACITY['technical_minimum_entries']}")
    print(f"nvs_page_header_bytes={NVS_PAGE_HEADER_BYTES}")
    print(f"nvs_page_entry_table_bytes={NVS_PAGE_ENTRY_TABLE_BYTES}")
    print(f"nvs_entry_size_bytes={NVS_ENTRY_SIZE}")
    print(f"nvs_usable_entries_per_page={NVS_ENTRY_COUNT}")
    print(f"nvs_blob_data_chunk_max_bytes={NVS_CHUNK_MAX_SIZE}")
    print(f"technical_minimum_pages={CAPACITY['technical_minimum_pages']}")
    print(f"technical_minimum_bytes=0x{CAPACITY['technical_minimum_bytes']:X}")
    print(
        "gc_fragmentation_reserve_pages="
        f"{CAPACITY['gc_fragmentation_reserve_pages']}"
    )
    print(f"wear_headroom_pages={CAPACITY['wear_headroom_pages']}")
    print(f"planned_state_store_pages={CAPACITY['planned_state_store_pages']}")
    print(
        f"planned_state_store_bytes=0x{CAPACITY['planned_state_store_bytes']:X}"
    )
    print(f"planned_headroom_pages={CAPACITY['planned_headroom_pages']}")
    if arguments.require_built_apps:
        check_built_app_sizes(root)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
