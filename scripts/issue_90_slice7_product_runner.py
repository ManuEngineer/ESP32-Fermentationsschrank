#!/usr/bin/env python3
"""Actor-free Issue #90 Slice-7 product runner.

The runner is deliberately split into fail-closed phases:

* ``prepare`` reads and verifies the production layout before a harness is
  flashed or booted;
* ``campaign`` only accepts an already verified pre-harness manifest;
* ``restore`` writes the production layout and release artifacts back;
* ``verify-restored-boot`` verifies the normal product boot after an Owner
  power cycle.

``prepare``/``restore`` let esptool auto-reset the board into the ROM
bootloader for flash/read access (``--before default-reset``); this is
ordinary flash tooling over the same control lines, not a substitute for a
real power interruption, and no such reset ever counts as one of the six
campaign power-cut attempts. The runner never removes or restores board
power itself: physical power removal during the campaign is performed only
by the Owner at the explicit power-cut window. All recovery decisions use
the raw current product status and the explicit Slice-7 classifier below;
no status-string existence is a PASS criterion.
"""

from __future__ import annotations

import argparse
import contextlib
import hashlib
import io
import json
import re
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from unittest import mock

import check_build_profiles


ROOT = Path(__file__).resolve().parents[1]
TEST_PARTITION = "state_store_test"
PROTOCOL_VERSION = 2
PLAN_SHA = "baf0b2ae04cd42afa75dfa00e21d900116b38bc8"
STATE_STORE_OFFSET = 0x300000
STATE_STORE_SIZE = 0x100000
PARTITION_TABLE_OFFSET = 0x8000
PARTITION_TABLE_RESERVED_SIZE = 0x1000
BOOTLOADER_OFFSET = 0x1000
APPLICATION_OFFSET = 0x10000
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
GIT_SHA_PATTERN = re.compile(r"^[0-9a-f]{40}$")


class RunnerError(RuntimeError):
    pass


def marker_fields(line: str, marker: str) -> dict[str, str]:
    if marker not in line:
        return {}
    fields: dict[str, str] = {}
    for token in line.split(marker, 1)[1].strip().split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        fields[key] = value
    return fields


def require_actor_free(fields: dict[str, str]) -> None:
    if fields.get("actuator_release") != "false":
        raise RunnerError(f"actor gate was not false: {fields}")


def git_sha() -> str:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True, capture_output=True
    )
    if result.returncode != 0:
        raise RunnerError(result.stderr.strip())
    return result.stdout.strip()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_file(path: Path, description: str) -> None:
    if not path.is_file():
        raise RunnerError(f"{description} missing: {path}")


def require_exact_blob(path: Path, expected_size: int, description: str) -> str:
    require_file(path, description)
    actual_size = path.stat().st_size
    if actual_size != expected_size:
        raise RunnerError(
            f"{description} has {actual_size} bytes; expected {expected_size}"
        )
    return sha256_file(path)


def require_source_sha(value: str) -> str:
    if not GIT_SHA_PATTERN.fullmatch(value):
        raise RunnerError(f"invalid documented production source SHA: {value}")
    return value


def require_clean_source_tree() -> None:
    try:
        check_build_profiles.require_clean_source_tree(ROOT)
    except RuntimeError as error:
        raise RunnerError(str(error)) from error


def verify_harness_source_sha(expected_source_sha: str, observed_source_sha: str) -> str:
    """Accept only the complete exact SHA emitted by ISSUE90_READY."""
    try:
        expected = require_source_sha(expected_source_sha)
        observed = require_source_sha(observed_source_sha)
    except RunnerError as error:
        raise RunnerError("BLOCKED_HARNESS_SOURCE_SHA_MISMATCH") from error
    if observed != expected:
        raise RunnerError("BLOCKED_HARNESS_SOURCE_SHA_MISMATCH")
    return observed


def release_build_provenance(report_path: Path) -> dict[str, str]:
    """Read exact release provenance from the existing build report."""

    require_file(report_path, "release build provenance report")
    report = report_path.read_text(encoding="utf-8")
    section_match = re.search(
        r"^## ESP-IDF esp32_release .*?(?=^## |\Z)",
        report,
        flags=re.MULTILINE | re.DOTALL,
    )
    if section_match is None:
        raise RunnerError("release build provenance section missing")
    section = section_match.group(0)
    values: dict[str, str] = {}
    for name in ("Build-Commit", "Source-Git-SHA"):
        match = re.search(rf"^- {re.escape(name)}: ([0-9a-f]{{40}})$", section, re.MULTILINE)
        if match is None:
            raise RunnerError(f"release build provenance field missing: {name}")
        values[name] = require_source_sha(match.group(1))
    values["report_sha256"] = sha256_file(report_path)
    return values


def require_embedded_source_sha(path: Path, source_sha: str) -> None:
    """Require the exact build SHA in the application provenance string."""

    expected = require_source_sha(source_sha).encode("ascii")
    require_file(path, "production release application artifact")
    if expected not in path.read_bytes():
        raise RunnerError("BLOCKED_RELEASE_ARTIFACT_PROVENANCE_MISMATCH")


def run_esptool(port: str, baud: int, command: list[str]) -> str:
    tool = shutil.which("esptool")
    if tool is None:
        raise RunnerError("pinned ESP-IDF esptool is required for flash safety")
    result = subprocess.run(
        [
            tool,
            "--chip",
            "esp32",
            "--port",
            port,
            "--baud",
            str(baud),
            "--before",
            "default-reset",
            "--after",
            "no-reset",
            *command,
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        details = (result.stderr or result.stdout).strip().splitlines()
        raise RunnerError(
            f"esptool {' '.join(command[:1])} failed: "
            f"{details[-1] if details else 'no diagnostic'}"
        )
    return result.stdout + result.stderr


def confirm_rom_bootloader(args: argparse.Namespace, action: str) -> bool:
    # esptool's own "--before default-reset" handles the actual ROM-bootloader
    # entry automatically via the existing control lines; this is ordinary
    # flash/read tooling access, never a substitute for a real power-cut
    # attempt. The Owner only confirms the board is powered on and connected
    # before esptool resets it into the bootloader.
    print(f"OWNER_ACTION_REQUIRED={action}", flush=True)
    print(
        "OWNER_INSTRUCTIONS=CONFIRM_BOARD_POWERED_ON_AND_USB_CONNECTED_"
        "ESPTOOL_WILL_AUTO_RESET_INTO_ROM_BOOTLOADER",
        flush=True,
    )
    try:
        input(
            "After confirming the ESP32 is powered on and connected, press "
            "Enter to let esptool reset it into the ROM bootloader. "
        )
    except (EOFError, KeyboardInterrupt) as error:
        print("ROM_BOOTLOADER_READY=FAIL", flush=True)
        raise RunnerError("owner flash/restore-step confirmation missing") from error
    try:
        probe_output = run_esptool(args.port, args.baud, ["chip-id"])
    except RunnerError as error:
        print("ROM_BOOTLOADER_READY=FAIL", flush=True)
        raise RunnerError("ROM_BOOTLOADER_READY=FAIL") from error
    if "connected to esp32" not in probe_output.lower():
        print("ROM_BOOTLOADER_READY=FAIL", flush=True)
        raise RunnerError("ROM_BOOTLOADER_READY=FAIL; chip-id response unrecognized")
    print("ROM_BOOTLOADER_READY=PASS", flush=True)
    return True


def confirm_normal_power_cycle() -> bool:
    print("OWNER_ACTION_REQUIRED=EXIT_ROM_BOOTLOADER_AND_NORMAL_POWER_CYCLE", flush=True)
    print(
        "OWNER_INSTRUCTIONS=RELEASE_IO0_FROM_GND_THEN_PERFORM_REAL_NORMAL_POWER_CYCLE",
        flush=True,
    )
    try:
        input("After IO0 is released and the normal power cycle is complete, press Enter. ")
    except (EOFError, KeyboardInterrupt) as error:
        raise RunnerError("owner normal-power-cycle confirmation missing") from error
    return True


def capture_verified_flash_region(
    port: str, baud: int, offset: int, size: int, target: Path, description: str
) -> str:
    """Read one flash region twice and publish it only after equal hashes."""

    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="issue90-flash-", dir=target.parent
    ) as directory:
        first = Path(directory) / "capture.bin"
        second = Path(directory) / "capture-readback.bin"
        command = ["read-flash", hex(offset), hex(size)]
        run_esptool(port, baud, [*command, str(first)])
        first_sha = require_exact_blob(first, size, description)
        run_esptool(port, baud, [*command, str(second)])
        second_sha = require_exact_blob(second, size, description)
        if first_sha != second_sha:
            raise RunnerError(f"{description} read-back hash mismatch")
        first.replace(target)
    return require_exact_blob(target, size, description)


def readback_hash(
    port: str, baud: int, offset: int, size: int, expected_sha: str, description: str
) -> str:
    with tempfile.TemporaryDirectory(prefix="issue90-readback-") as directory:
        readback = Path(directory) / "readback.bin"
        run_esptool(
            port,
            baud,
            ["read-flash", hex(offset), hex(size), str(readback)],
        )
        actual_sha = require_exact_blob(readback, size, description)
    if actual_sha != expected_sha:
        raise RunnerError(f"{description} read-back hash mismatch")
    return actual_sha


def write_and_verify_flash_region(
    port: str,
    baud: int,
    offset: int,
    path: Path,
    expected_size: int,
    expected_sha: str,
    description: str,
) -> str:
    actual_sha = require_exact_blob(path, expected_size, description)
    if actual_sha != expected_sha:
        raise RunnerError(f"{description} source hash mismatch")
    run_esptool(port, baud, ["write-flash", hex(offset), str(path)])
    return readback_hash(port, baud, offset, expected_size, expected_sha, description)


def require_production_partition_artifact(path: Path) -> tuple[int, str]:
    require_file(path, "production partition-table artifact")
    size = path.stat().st_size
    if size <= 0 or size > PARTITION_TABLE_RESERVED_SIZE:
        raise RunnerError(
            "production partition-table artifact has no valid canonical "
            f"ESP-IDF boundary: {size} bytes"
        )
    return size, sha256_file(path)


def release_artifact_paths(release_dir: Path) -> dict[str, Path]:
    return {
        "bootloader": release_dir / "bootloader" / "bootloader.bin",
        "partition_table": release_dir / "partition_table" / "partition-table.bin",
        "application": release_dir / "esp32_fermentationsschrank.bin",
    }


def require_release_artifacts(release_dir: Path) -> dict[str, dict[str, object]]:
    paths = release_artifact_paths(release_dir)
    offsets = {
        "bootloader": BOOTLOADER_OFFSET,
        "partition_table": PARTITION_TABLE_OFFSET,
        "application": APPLICATION_OFFSET,
    }
    artifacts: dict[str, dict[str, object]] = {}
    for name, path in paths.items():
        require_file(path, f"production release {name} artifact")
        size = path.stat().st_size
        if size <= 0:
            raise RunnerError(f"production release {name} artifact is empty: {path}")
        artifacts[name] = {
            "file": str(path.resolve()),
            "offset": offsets[name],
            "size": size,
            "sha256": sha256_file(path),
        }
    table_size, table_sha = require_production_partition_artifact(paths["partition_table"])
    if artifacts["partition_table"]["size"] != table_size:
        raise RunnerError("partition-table artifact size changed during inspection")
    if artifacts["partition_table"]["sha256"] != table_sha:
        raise RunnerError("partition-table artifact hash changed during inspection")
    return artifacts


def prepare_pre_harness_backup(
    args: argparse.Namespace, *, rom_bootloader_ready: bool = False
) -> int:
    if not rom_bootloader_ready:
        raise RunnerError("BLOCKED_ROM_BOOTLOADER_REQUIRED_FOR_PRE_HARNESS_BACKUP")
    require_clean_source_tree()
    source_sha = require_source_sha(args.production_source_sha)
    production_table = Path(args.production_partition_table)
    table_size, table_sha = require_production_partition_artifact(production_table)
    release_artifacts = require_release_artifacts(Path(args.production_release_dir))
    provenance_path = Path(args.release_build_report)
    provenance = release_build_provenance(provenance_path)
    if (
        provenance["Source-Git-SHA"] != source_sha
        or provenance["Build-Commit"] != source_sha
    ):
        raise RunnerError("BLOCKED_RELEASE_ARTIFACT_PROVENANCE_MISMATCH")
    require_embedded_source_sha(
        Path(str(release_artifacts["application"]["file"])), source_sha
    )
    if release_artifacts["partition_table"]["sha256"] != table_sha:
        raise RunnerError(
            "production partition-table argument differs from release artifact"
        )

    backup_dir = Path(args.backup_dir)
    table_backup = backup_dir / "production_partition_table.bin"
    state_backup = backup_dir / "production_state_store.bin"
    table_backup_sha = capture_verified_flash_region(
        args.port,
        args.baud,
        PARTITION_TABLE_OFFSET,
        table_size,
        table_backup,
        "production partition-table backup",
    )
    if table_backup_sha != table_sha:
        raise RunnerError(
            "production board partition-table bytes do not match the documented "
            "release artifact"
        )
    state_backup_sha = capture_verified_flash_region(
        args.port,
        args.baud,
        STATE_STORE_OFFSET,
        STATE_STORE_SIZE,
        state_backup,
        "production state-store backup",
    )

    manifest = {
        "protocol_version": PROTOCOL_VERSION,
        "phase": "PRE_HARNESS_BACKUP",
        "plan_sha": PLAN_SHA,
        "production_source_sha": source_sha,
        "release_build_source_sha": provenance["Source-Git-SHA"],
        "release_build_commit": provenance["Build-Commit"],
        "release_build_provenance": {
            "file": str(provenance_path.resolve()),
            "sha256": provenance["report_sha256"],
            "profile": "esp32_release",
        },
        "backup_source_state": "PRODUCTION_LAYOUT",
        "test_partition": {
            "label": TEST_PARTITION,
            "offset": STATE_STORE_OFFSET,
            "size": STATE_STORE_SIZE,
            "physically_separate": False,
            "reuses_production_flash_range": True,
        },
        "backups": {
            "production_partition_table": {
                "file": str(table_backup.resolve()),
                "offset": PARTITION_TABLE_OFFSET,
                "size": table_size,
                "sha256": table_backup_sha,
            },
            "production_state_store": {
                "file": str(state_backup.resolve()),
                "offset": STATE_STORE_OFFSET,
                "size": STATE_STORE_SIZE,
                "sha256": state_backup_sha,
            },
        },
        "production_release_artifacts": release_artifacts,
        "sensitive_artifacts_local_only": True,
        "full_flash_backup": False,
        "actor_free": True,
        "real_actuators_enabled": False,
        "power_cut_type": "PHYSICAL_POWER_REMOVAL",
        "rts_en_power_cut_substitute": False,
        "esptool_prepare_restore_reset_mode": "default-reset",
        "production_restore_required": True,
        "campaign_result": "NOT_RUN",
        "restore_state": "PENDING_OWNER_BOOTLOADER_ACTION",
        "source_tree_clean": True,
    }
    output = Path(args.manifest_output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("PRE_HARNESS_BACKUP=PASS", flush=True)
    print("BACKUP_SOURCE_STATE=PRODUCTION_LAYOUT", flush=True)
    print("SOURCE_TREE_CLEAN=YES", flush=True)
    print("TEST_PARTITION_PHYSICALLY_SEPARATE=NO", flush=True)
    print("TEST_PARTITION_REUSES_PRODUCTION_FLASH_RANGE=YES", flush=True)
    print(f"PRODUCTION_SOURCE_SHA={source_sha}", flush=True)
    print(
        f"RELEASE_BUILD_SOURCE_SHA={provenance['Source-Git-SHA']}", flush=True
    )
    print("RELEASE_ARTIFACT_PROVENANCE_GATE=PASS", flush=True)
    print(f"PRODUCTION_PARTITION_TABLE_BACKUP_SHA256={table_backup_sha}", flush=True)
    print(f"PRODUCTION_STATE_STORE_BACKUP_SHA256={state_backup_sha}", flush=True)
    print("OWNER_ACTION_REQUIRED=ENTER_ROM_BOOTLOADER_FOR_HARNESS_FLASH", flush=True)
    return 0


def _manifest_file(entry: object, name: str) -> tuple[Path, int, str, int]:
    if not isinstance(entry, dict):
        raise RunnerError(f"backup manifest entry malformed: {name}")
    try:
        path = Path(str(entry["file"]))
        offset = int(entry["offset"])
        size = int(entry["size"])
        expected_sha = str(entry["sha256"])
    except (KeyError, TypeError, ValueError) as error:
        raise RunnerError(f"backup manifest entry malformed: {name}") from error
    if not SHA256_PATTERN.fullmatch(expected_sha):
        raise RunnerError(f"backup manifest hash malformed: {name}")
    actual_sha = require_exact_blob(path, size, name)
    if actual_sha != expected_sha:
        raise RunnerError(f"backup manifest hash mismatch: {name}")
    return path, offset, expected_sha, size


def load_pre_harness_manifest(path: Path) -> dict[str, object]:
    require_file(path, "pre-harness backup manifest")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RunnerError(f"backup manifest cannot be read: {path}") from error
    if not isinstance(data, dict):
        raise RunnerError("backup manifest is not an object")
    if data.get("protocol_version") != PROTOCOL_VERSION:
        raise RunnerError("backup manifest protocol version rejected")
    if data.get("phase") != "PRE_HARNESS_BACKUP":
        raise RunnerError("backup manifest phase rejected")
    if data.get("plan_sha") != PLAN_SHA:
        raise RunnerError("backup manifest plan SHA rejected")
    if data.get("backup_source_state") != "PRODUCTION_LAYOUT":
        raise RunnerError("BACKUP_SOURCE_STATE rejected; production layout required")
    if data.get("actor_free") is not True or data.get("real_actuators_enabled") is not False:
        raise RunnerError("backup manifest actor-free contract rejected")
    if data.get("source_tree_clean") is not True:
        raise RunnerError("SOURCE_TREE_CLEAN proof missing")
    if not isinstance(data.get("production_restore_required"), bool):
        raise RunnerError("production restore requirement missing")
    if data.get("campaign_result") not in {
        "NOT_RUN",
        "PASS",
        "FAIL_OR_INTERRUPTED",
    }:
        raise RunnerError("backup manifest campaign state rejected")
    restore_state = data.get("restore_state")
    if restore_state not in {
        "PENDING_OWNER_BOOTLOADER_ACTION",
        "RESTORE_FAILED_RETRY_REQUIRED",
        "RESTORED_PENDING_NORMAL_BOOT",
        "COMPLETE",
    }:
        raise RunnerError("backup manifest restore state rejected")
    production_restore_required = data.get("production_restore_required")
    if restore_state == "COMPLETE":
        if production_restore_required is not False:
            raise RunnerError("completed restore manifest still requires restore")
    elif production_restore_required is not True:
        raise RunnerError("pending restore manifest does not require restore")
    partition = data.get("test_partition")
    if not isinstance(partition, dict):
        raise RunnerError("backup manifest test partition missing")
    if (
        partition.get("label") != TEST_PARTITION
        or partition.get("offset") != STATE_STORE_OFFSET
        or partition.get("size") != STATE_STORE_SIZE
        or partition.get("physically_separate") is not False
        or partition.get("reuses_production_flash_range") is not True
    ):
        raise RunnerError("test partition physical-layout contract rejected")
    source_sha = require_source_sha(str(data.get("production_source_sha", "")))
    release_source_sha = require_source_sha(
        str(data.get("release_build_source_sha", ""))
    )
    release_build_commit = require_source_sha(
        str(data.get("release_build_commit", ""))
    )
    if source_sha != release_source_sha or source_sha != release_build_commit:
        raise RunnerError("BLOCKED_RELEASE_ARTIFACT_PROVENANCE_MISMATCH")
    provenance = data.get("release_build_provenance")
    if not isinstance(provenance, dict):
        raise RunnerError("release build provenance missing")
    provenance_path = Path(str(provenance.get("file", "")))
    if provenance.get("profile") != "esp32_release":
        raise RunnerError("release build provenance profile rejected")
    expected_report_sha = str(provenance.get("sha256", ""))
    if not SHA256_PATTERN.fullmatch(expected_report_sha):
        raise RunnerError("release build provenance hash malformed")
    if sha256_file(provenance_path) != expected_report_sha:
        raise RunnerError("release build provenance file hash mismatch")
    current_provenance = release_build_provenance(provenance_path)
    if (
        current_provenance["Source-Git-SHA"] != release_source_sha
        or current_provenance["Build-Commit"] != release_build_commit
    ):
        raise RunnerError("BLOCKED_RELEASE_ARTIFACT_PROVENANCE_MISMATCH")
    if restore_state in {"RESTORED_PENDING_NORMAL_BOOT", "COMPLETE"}:
        restored_application_sha = str(data.get("restored_application_sha256", ""))
        if not SHA256_PATTERN.fullmatch(restored_application_sha):
            raise RunnerError("restored application provenance missing")
    if restore_state == "COMPLETE":
        post_restore_source_sha = require_source_sha(
            str(data.get("post_restore_product_source_sha", ""))
        )
        if post_restore_source_sha != source_sha:
            raise RunnerError("post-restore product provenance mismatch")
    backups = data.get("backups")
    if not isinstance(backups, dict):
        raise RunnerError("backup manifest backups missing")
    table_backup = _manifest_file(
        backups.get("production_partition_table"),
        "production partition-table backup",
    )
    if table_backup[1] != PARTITION_TABLE_OFFSET:
        raise RunnerError("production partition-table backup offset rejected")
    if table_backup[3] <= 0 or table_backup[3] > PARTITION_TABLE_RESERVED_SIZE:
        raise RunnerError("production partition-table backup boundary rejected")
    state_backup = _manifest_file(
        backups.get("production_state_store"), "production state-store backup"
    )
    if state_backup[1] != STATE_STORE_OFFSET or state_backup[3] != STATE_STORE_SIZE:
        raise RunnerError("production state-store backup range rejected")

    release = data.get("production_release_artifacts")
    if not isinstance(release, dict):
        raise RunnerError("production release artifacts missing")
    expected_offsets = {
        "bootloader": BOOTLOADER_OFFSET,
        "partition_table": PARTITION_TABLE_OFFSET,
        "application": APPLICATION_OFFSET,
    }
    for name, expected_offset in expected_offsets.items():
        artifact = release.get(name)
        if not isinstance(artifact, dict):
            raise RunnerError(f"production release artifact missing: {name}")
        artifact_path, offset, artifact_sha, size = _manifest_file(
            artifact, f"production release {name} artifact"
        )
        if offset != expected_offset:
            raise RunnerError(f"production release {name} offset rejected")
        artifact["file"] = str(artifact_path)
        artifact["sha256"] = artifact_sha
        artifact["size"] = size
    if release["partition_table"]["size"] != table_backup[3]:
        raise RunnerError(
            "production release partition-table size differs from captured table"
        )
    if release["partition_table"]["sha256"] != table_backup[2]:
        raise RunnerError(
            "production release partition-table differs from captured production table"
        )
    data["production_source_sha"] = source_sha
    data["release_build_source_sha"] = release_source_sha
    data["release_build_commit"] = release_build_commit
    data["backups"] = backups
    data["production_release_artifacts"] = release
    return data


def require_pre_harness_manifest(path: Path | None) -> dict[str, object]:
    if path is None:
        raise RunnerError("BLOCKED_PRE_HARNESS_BACKUP_REQUIRED")
    return load_pre_harness_manifest(path)


def restore_production_layout(
    args: argparse.Namespace,
    manifest: dict[str, object],
    *,
    rom_bootloader_ready: bool = False,
) -> str:
    if not rom_bootloader_ready:
        raise RunnerError("BLOCKED_ROM_BOOTLOADER_REQUIRED_FOR_PRODUCTION_RESTORE")
    backups = manifest.get("backups")
    if not isinstance(backups, dict):
        raise RunnerError("backup manifest backups missing during restore")
    table_backup, table_offset, table_sha, table_size = _manifest_file(
        backups.get("production_partition_table"),
        "production partition-table backup",
    )
    state_backup, state_offset, state_sha, state_size = _manifest_file(
        backups.get("production_state_store"), "production state-store backup"
    )
    write_and_verify_flash_region(
        args.port,
        args.baud,
        state_offset,
        state_backup,
        state_size,
        state_sha,
        "production state-store restore",
    )
    write_and_verify_flash_region(
        args.port,
        args.baud,
        table_offset,
        table_backup,
        table_size,
        table_sha,
        "production partition-table restore",
    )
    release = manifest.get("production_release_artifacts")
    if not isinstance(release, dict):
        raise RunnerError("production release artifacts missing during restore")
    command = ["write-flash"]
    ordered_names = ("bootloader", "partition_table", "application")
    for name in ordered_names:
        artifact = release.get(name)
        if not isinstance(artifact, dict):
            raise RunnerError(f"production release artifact missing during restore: {name}")
        command.extend([hex(int(artifact["offset"])), str(artifact["file"])])
    run_esptool(args.port, args.baud, command)
    application_sha = ""
    for name in ordered_names:
        artifact = release.get(name)
        if not isinstance(artifact, dict):
            raise RunnerError(f"production release artifact missing during verify: {name}")
        actual_sha = readback_hash(
            args.port,
            args.baud,
            int(artifact["offset"]),
            int(artifact["size"]),
            str(artifact["sha256"]),
            f"production release {name} restore",
        )
        if name == "application":
            application_sha = actual_sha
    print("PRODUCTION_LAYOUT_RESTORED=PASS", flush=True)
    print("PRODUCTION_STATE_STORE_RESTORED=PASS", flush=True)
    print("PRODUCTION_RELEASE_FIRMWARE_RESTORED=PASS", flush=True)
    return application_sha


def update_manifest_state(path: Path, **updates: object) -> None:
    require_file(path, "pre-harness backup manifest")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RunnerError(f"backup manifest cannot be updated: {path}") from error
    if not isinstance(data, dict):
        raise RunnerError("backup manifest is not an object")
    data.update(updates)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def require_restore_pending(manifest: dict[str, object]) -> None:
    if manifest.get("production_restore_required") is not True:
        raise RunnerError("PRODUCTION_RESTORE_REQUIRED is not asserted")


def verify_observed_product_source_sha(
    expected_source_sha: str, observed_source_sha: str
) -> str:
    expected = require_source_sha(expected_source_sha)
    observed = require_source_sha(observed_source_sha)
    if observed != expected:
        raise RunnerError("POST_RESTORE_PRODUCT_SOURCE_SHA_MISMATCH")
    return observed


REQUIRED_STATUS_FIELDS = (
    "application_lifecycle",
    "configuration_recovery_status",
    "configuration_runtime_available",
    "run_persistence_load_status",
    "run_load_disposition",
    "published_process_state",
    "actuator_release",
)
REQUIRED_FAULT_FIELDS = ("PANIC", "WATCHDOG", "BROWNOUT", "UNEXPECTED_RESET")
KNOWN_LIFECYCLES = {"Ready", "ServiceRequired"}
KNOWN_CONFIG_SUCCESS = {
    "RuntimeReady",
    "FactoryInitializationCompleted",
    "FactoryResetCompleted",
}
KNOWN_CONFIG_FAIL_CLOSED = {
    "ConfigurationUnavailable",
    "ConfigurationIntegrityFailure",
    "UnsupportedNewerConfigurationSchema",
    "PersistenceReadFailure",
    "PersistenceCapacityFailure",
    "PersistenceWriteFailure",
    "CounterOverflow",
    "RuntimePreparationFailure",
    "BootstrapCommitIndeterminate",
    "ConfigurationRecordOutcomeIndeterminate",
    "ConfigurationCommitIndeterminate",
}
KNOWN_CONFIG_STATUSES = KNOWN_CONFIG_SUCCESS | KNOWN_CONFIG_FAIL_CLOSED | {
    "ConfigurationMutationBusy",
    "ConfigurationModelBudgetBusy",
    "StateTransitionRejected",
}
KNOWN_RUN_LOAD_SUCCESS = {
    "NoPersistedRun",
    "Current",
    "NoActiveRun",
}
KNOWN_RUN_LOAD_FAIL_CLOSED = {
    "FallbackRecovered",
    "PreparedInterrupted",
    "NotReconstructible",
    "NotReconstructibleOrphanedState",
    "ReadFailed",
    "CapacityExceeded",
    "UnsupportedSchema",
    "ForeignEpoch",
    "AlreadyInitialized",
}
KNOWN_RUN_LOAD_STATUSES = KNOWN_RUN_LOAD_SUCCESS | KNOWN_RUN_LOAD_FAIL_CLOSED | {
    "NOT_AVAILABLE"
}
KNOWN_DISPOSITIONS = {
    "Standby",
    "ResumeOffer",
    "RecoveryEvaluation",
    "NoActiveRun",
    "Completed",
    "TerminalFault",
    "SafeBoot",
}
KNOWN_PROCESS_STATES = {
    "NONE",
    "Standby",
    "Preheating",
    "WaitingForProduct",
    "ReachingTarget",
    "QualifyingTarget",
    "Fermenting",
    "Cooling",
    "CoolHolding",
    "ManualHolding",
    "Completed",
    "RecoveryEvaluation",
    "Fault",
    "ServiceMode",
    "Boot",
    "SafeBoot",
}


@dataclass(frozen=True)
class RecoveryClassification:
    classification: str
    reason: str
    result: str


def _fail_closed_configuration(status: dict[str, str]) -> bool:
    return (
        status["application_lifecycle"] == "ServiceRequired"
        and status["configuration_runtime_available"] == "NO"
        and status["run_persistence_load_status"] == "NOT_AVAILABLE"
        and status["run_load_disposition"] == "SafeBoot"
        and status["published_process_state"] == "NONE"
    )


def _fail_closed_run(status: dict[str, str]) -> bool:
    return (
        status["application_lifecycle"] == "ServiceRequired"
        and status["configuration_runtime_available"] == "YES"
        and status["run_persistence_load_status"] in KNOWN_RUN_LOAD_FAIL_CLOSED
        and status["run_load_disposition"] == "SafeBoot"
        and status["published_process_state"] == "NONE"
    )


def classify_product_recovery(
    scenario: str, status: dict[str, str], faults: dict[str, str]
) -> RecoveryClassification:
    """Classify only exact current-product status combinations.

    The allowed combinations mirror the current recovery sources: a valid
    configuration publishes ``Standby``; a no-run/tombstone result maps to
    ``Standby``; and an untrusted configuration/run path is
    ``ServiceRequired``/``SafeBoot``. ``ResumeOffer`` remains a valid
    disposition for a resume-eligible run (``Preheating``/``Cooling``/
    ``ManualHolding``), but this harness's ``run`` scenario cannot currently
    drive a run into one of those states (no automatic control loop is
    composed yet), so it stays untested here rather than unreachable-by-
    design. ``RecoveryEvaluation`` (a loaded ``Current``+``Fermenting``
    record) is deliberately never classified ``PASS``: its status fields
    alone cannot distinguish a trusted-time-pending record from a rejected
    one, so it is routed to owner review instead of guessed as safe or
    unsafe.
    """

    if scenario not in {"config", "run"}:
        return RecoveryClassification("UNMAPPED", "unknown_scenario", "BLOCKED")
    missing = [key for key in REQUIRED_STATUS_FIELDS if key not in status]
    missing += [key for key in REQUIRED_FAULT_FIELDS if key not in faults]
    if missing:
        return RecoveryClassification(
            "UNMAPPED", "missing_raw_product_fields", "BLOCKED"
        )
    if status["actuator_release"] != "false":
        return RecoveryClassification(
            "UNSAFE_ACTUATOR_RELEASE", "actuator_release_not_false", "FAIL"
        )
    if any(faults[key] != "NONE_OBSERVED" for key in REQUIRED_FAULT_FIELDS):
        return RecoveryClassification("FAULT_OBSERVED", "runtime_fault_observed", "FAIL")
    if status["application_lifecycle"] not in KNOWN_LIFECYCLES:
        return RecoveryClassification("UNMAPPED", "unknown_application_lifecycle", "BLOCKED")
    if status["configuration_recovery_status"] not in KNOWN_CONFIG_STATUSES:
        return RecoveryClassification(
            "UNMAPPED", "unknown_configuration_recovery_status", "BLOCKED"
        )
    if status["configuration_runtime_available"] not in {"YES", "NO"}:
        return RecoveryClassification("UNMAPPED", "unknown_runtime_availability", "BLOCKED")
    if status["run_persistence_load_status"] not in KNOWN_RUN_LOAD_STATUSES:
        return RecoveryClassification("UNMAPPED", "unknown_run_load_status", "BLOCKED")
    if status["run_load_disposition"] not in KNOWN_DISPOSITIONS:
        return RecoveryClassification("UNMAPPED", "unknown_run_disposition", "BLOCKED")
    if status["published_process_state"] not in KNOWN_PROCESS_STATES:
        return RecoveryClassification("UNMAPPED", "unknown_published_process_state", "BLOCKED")

    config_status = status["configuration_recovery_status"]
    if config_status in KNOWN_CONFIG_FAIL_CLOSED and _fail_closed_configuration(status):
        return RecoveryClassification(
            "CONFIGURATION_FAIL_CLOSED",
            "configuration_runtime_unavailable_service_required_safeboot",
            "PASS_FAIL_CLOSED",
        )

    if config_status in KNOWN_CONFIG_SUCCESS:
        if status["application_lifecycle"] == "ServiceRequired" and _fail_closed_run(
            status
        ):
            return RecoveryClassification(
                "RUN_FAIL_CLOSED",
                "run_persistence_untrusted_service_required_safeboot",
                "PASS_FAIL_CLOSED",
            )
        if (
            status["application_lifecycle"] != "Ready"
            or status["configuration_runtime_available"] != "YES"
        ):
            return RecoveryClassification(
                "UNEXPECTED_CONFIGURATION_STATE",
                "successful_configuration_not_operational",
                "FAIL",
            )
        if scenario == "config":
            allowed = (
                status["run_persistence_load_status"] in {"NoPersistedRun", "NoActiveRun"}
                and status["run_load_disposition"] == "Standby"
                and status["published_process_state"] == "Standby"
            )
            if allowed:
                return RecoveryClassification(
                    "CONFIGURATION_RECOVERED",
                    "valid_old_or_new_configuration_published_standby",
                    "PASS",
                )
            return RecoveryClassification(
                "UNEXPECTED_CONFIGURATION_OUTCOME",
                "configuration_scenario_status_tuple_not_allowed",
                "FAIL",
            )

        if status["run_load_disposition"] == "RecoveryEvaluation":
            return RecoveryClassification(
                "RECOVERY_EVALUATION_REQUIRES_OWNER_REVIEW",
                "recovery_evaluation_disposition_not_classifiable_by_slice7_status",
                "BLOCKED",
            )

        run_tuple = (
            status["run_persistence_load_status"],
            status["run_load_disposition"],
            status["published_process_state"],
        )
        if run_tuple in {
            ("NoPersistedRun", "Standby", "Standby"),
            ("NoActiveRun", "Standby", "Standby"),
            ("Current", "NoActiveRun", "Standby"),
        }:
            return RecoveryClassification(
                "RUN_RECOVERED_STANDBY",
                "valid_no_run_or_non_resume_run_published_standby",
                "PASS",
            )
        if run_tuple == ("Current", "ResumeOffer", "NONE"):
            return RecoveryClassification(
                "RUN_RECOVERED_RESUME_OFFER",
                "valid_r1_resume_offer_not_published_before_decision",
                "PASS",
            )
        return RecoveryClassification(
            "UNEXPECTED_RUN_OUTCOME", "run_scenario_status_tuple_not_allowed", "FAIL"
        )

    return RecoveryClassification(
        "UNEXPECTED_FAIL_CLOSED_STATE",
        "known_recovery_failure_not_in_contractual_fail_closed_shape",
        "FAIL",
    )


class ProductSerial:
    def __init__(self, port_name: str, baud: int, timeout: float) -> None:
        try:
            import serial  # type: ignore
        except ImportError as error:
            raise RunnerError("pyserial is required for a hardware run") from error
        self._serial = serial.Serial(
            port=None,
            baudrate=baud,
            timeout=0.1,
            write_timeout=1.0,
            rtscts=False,
            dsrdtr=False,
        )
        self._serial.port = port_name
        self._serial.rts = False
        self._serial.dtr = False
        self._serial.open()
        self.timeout = timeout
        self.observed_faults: set[str] = set()
        self.observed_product_source_sha: str | None = None

    def close(self) -> None:
        self._serial.close()

    def __enter__(self) -> "ProductSerial":
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def send(self, command: str) -> None:
        self._serial.write((command + "\n").encode("ascii"))
        self._serial.flush()

    def _read_line(self) -> str | None:
        raw = self._serial.readline()
        if not raw:
            return None
        line = raw.decode("utf-8", errors="replace").strip()
        self._observe_faults(line)
        return line

    def read_until(self, marker: str) -> tuple[list[str], dict[str, str]]:
        deadline = time.monotonic() + self.timeout
        lines: list[str] = []
        while time.monotonic() < deadline:
            line = self._read_line()
            if line is None:
                continue
            lines.append(line)
            if marker in line:
                return lines, marker_fields(line, marker)
        raise RunnerError(f"timeout waiting for {marker}; last={lines[-8:]}")

    def wait_for_product_boot(self) -> str:
        deadline = time.monotonic() + self.timeout
        lines: list[str] = []
        saw_actor_free = False
        saw_ready = False
        while time.monotonic() < deadline:
            line = self._read_line()
            if line is None:
                continue
            lines.append(line)
            lowered = line.lower()
            saw_actor_free |= "real actuators: disabled" in lowered
            saw_ready |= "application: ready" in lowered
            source_marker = "source git sha:"
            if source_marker in lowered:
                observed = line[
                    lowered.index(source_marker) + len(source_marker) :
                ].strip()
                if GIT_SHA_PATTERN.fullmatch(observed):
                    self.observed_product_source_sha = observed
            if saw_actor_free and saw_ready and self.observed_product_source_sha:
                if self.observed_faults:
                    raise RunnerError(f"fault during restored product boot: {lines[-8:]}")
                return self.observed_product_source_sha
        raise RunnerError(
            "normal product boot/provenance not verified; "
            f"last={lines[-8:]}"
        )

    def _observe_faults(self, line: str) -> None:
        lowered = line.lower()
        if "guru meditation" in lowered or "panic" in lowered:
            self.observed_faults.add("PANIC")
        if "watchdog" in lowered or "wdt" in lowered:
            self.observed_faults.add("WATCHDOG")
        if "brownout" in lowered:
            self.observed_faults.add("BROWNOUT")
        if "unexpected reset" in lowered:
            self.observed_faults.add("UNEXPECTED_RESET")

    def fault_status(self) -> dict[str, str]:
        return {
            name: ("NONE_OBSERVED" if name not in self.observed_faults else "OBSERVED")
            for name in REQUIRED_FAULT_FIELDS
        }

    def ready(self, expected_source_sha: str) -> dict[str, str]:
        _, fields = self.read_until("ISSUE90_READY")
        if fields.get("partition") != TEST_PARTITION:
            raise RunnerError(f"unexpected test partition: {fields}")
        if fields.get("real_actuators_enabled") != "NO":
            raise RunnerError(f"actor-free contract missing: {fields}")
        if fields.get("test_partition_physically_separate") != "NO":
            raise RunnerError(f"test partition physical separation claim rejected: {fields}")
        if fields.get("test_partition_reuses_production_flash_range") != "YES":
            raise RunnerError(f"test partition range reuse not declared: {fields}")
        observed_source_sha = fields.get("source_sha", "UNKNOWN")
        try:
            verify_harness_source_sha(expected_source_sha, observed_source_sha)
        except RunnerError:
            print(f"HARNESS_OBSERVED_SOURCE_SHA={observed_source_sha}", flush=True)
            print(f"EXPECTED_HARNESS_SOURCE_SHA={expected_source_sha}", flush=True)
            print("HARNESS_SOURCE_SHA_VERIFICATION=FAIL", flush=True)
            raise
        print(f"HARNESS_OBSERVED_SOURCE_SHA={observed_source_sha}", flush=True)
        print(f"EXPECTED_HARNESS_SOURCE_SHA={expected_source_sha}", flush=True)
        print("HARNESS_SOURCE_SHA_VERIFICATION=PASS", flush=True)
        return fields

    def status(self) -> dict[str, str]:
        self.send("STATUS")
        _, fields = self.read_until("ISSUE90_STATUS")
        require_actor_free(fields)
        if fields.get("application_started") != "YES":
            raise RunnerError(f"application did not start: {fields}")
        return fields

    def command(self, command: str) -> dict[str, str]:
        self.send(command)
        _, fields = self.read_until("ISSUE90_COMMAND_RESULT")
        if fields.get("command") != command:
            raise RunnerError(f"unexpected response to {command}: {fields}")
        return fields

    def set_trusted_utc(self, unix_seconds: int) -> dict[str, str]:
        self.send(f"SET_TRUSTED_UTC {unix_seconds}")
        _, fields = self.read_until("ISSUE90_TRUSTED_UTC_SET")
        return fields

    def prepare_power_cut(self, scenario: str) -> dict[str, str]:
        command = {
            "config": "ARM_CONFIG_WRITE_LOAD",
            "run": "ARM_RUN_WRITE_LOAD",
        }[scenario]
        if scenario == "run":
            preflight = self.status()
            if preflight.get("trusted_utc") != "NOT_AVAILABLE":
                raise RunnerError(
                    f"trusted_utc was already available before injection: {preflight}"
                )
            injected = self.set_trusted_utc(int(time.time()))
            if injected.get("result") != "PASS":
                raise RunnerError(f"SET_TRUSTED_UTC failed: {injected}")
            confirmed = self.status()
            trusted_utc = confirmed.get("trusted_utc")
            if trusted_utc is None or trusted_utc == "NOT_AVAILABLE":
                raise RunnerError(
                    f"trusted_utc not numeric after injection: {confirmed}"
                )
        self.send(command)
        _, armed = self.read_until("ISSUE90_LOAD_ARMED")
        if armed.get("result") != "PASS":
            raise RunnerError(f"load arm failed: {armed}")
        _, window = self.read_until("ISSUE90_OWNER_POWER_CUT_WINDOW_ACTIVE")
        if window.get("mode") != scenario.upper():
            raise RunnerError(f"wrong power-cut mode: {window}")
        return window


def print_attempt_result(
    scenario: str,
    attempt: int,
    window: dict[str, str],
    status: dict[str, str],
    faults: dict[str, str],
) -> RecoveryClassification:
    classification = classify_product_recovery(scenario, status, faults)
    fields = {
        "SCENARIO": scenario,
        "ATTEMPT": str(attempt),
        "CUT_WINDOW": window.get("iteration", "UNKNOWN"),
        "CONFIGURATION_RECOVERY_STATUS": status.get(
            "configuration_recovery_status", "UNKNOWN"
        ),
        "CONFIGURATION_RUNTIME_AVAILABLE": status.get(
            "configuration_runtime_available", "UNKNOWN"
        ),
        "RUN_PERSISTENCE_LOAD_STATUS": status.get(
            "run_persistence_load_status", "UNKNOWN"
        ),
        "RUN_LOAD_DISPOSITION": status.get("run_load_disposition", "UNKNOWN"),
        "PUBLISHED_PROCESS_STATE": status.get("published_process_state", "UNKNOWN"),
        "TRUSTED_UTC": status.get("trusted_utc", "UNKNOWN"),
        "ACTUATOR_RELEASE": status.get("actuator_release", "UNKNOWN"),
        "PRODUCT_RECOVERY_CLASSIFICATION": classification.classification,
        "PRODUCT_RECOVERY_REASON": classification.reason,
        "PANIC": faults.get("PANIC", "UNKNOWN"),
        "WATCHDOG": faults.get("WATCHDOG", "UNKNOWN"),
        "BROWNOUT": faults.get("BROWNOUT", "UNKNOWN"),
        "UNEXPECTED_RESET": faults.get("UNEXPECTED_RESET", "UNKNOWN"),
        "RESULT": classification.result,
    }
    print(" ".join(f"{key}={value}" for key, value in fields.items()), flush=True)
    return classification


def run_physical_campaign(args: argparse.Namespace) -> int:
    if args.attempts != 3:
        raise RunnerError("Slice-7 contract requires exactly 3 attempts per scenario")
    if args.profile != "esp32_bringup":
        raise RunnerError("the Slice-7 harness is bring-up-only")
    require_clean_source_tree()
    manifest = require_pre_harness_manifest(Path(args.pre_harness_manifest))
    expected_source_sha = require_source_sha(
        str(manifest.get("production_source_sha", ""))
    )
    print("PRE_HARNESS_BACKUP=PASS", flush=True)
    print("BACKUP_SOURCE_STATE=PRODUCTION_LAYOUT", flush=True)
    print("TEST_PARTITION_PHYSICALLY_SEPARATE=NO", flush=True)
    print("TEST_PARTITION_REUSES_PRODUCTION_FLASH_RANGE=YES", flush=True)
    print("OWNER_ACTION_REQUIRED=ENTER_ROM_BOOTLOADER_FOR_HARNESS_FLASH", flush=True)

    try:
        with ProductSerial(args.port, args.baud, args.timeout) as device:
            ready = device.ready(expected_source_sha)
            if (
                ready.get("backup_required") != "YES"
                or ready.get("partition_offset") != "0x300000"
                or ready.get("partition_size") != "0x100000"
            ):
                raise RunnerError(f"firmware did not expose expected test range: {ready}")
            device.command("BACKUP_OR_CONFIRM_TEST_PARTITION")
            device.status()
            for attempt in range(1, args.attempts + 1):
                window = device.prepare_power_cut(args.scenario)
                print(
                    f"SCENARIO={args.scenario} ATTEMPT={attempt} "
                    f"CUT_WINDOW={window.get('iteration', 'UNKNOWN')} "
                    "OWNER_ACTION_NOW=POWER_OFF",
                    flush=True,
                )
                input("After the ESP32 supply is physically OFF, press Enter. ")
                print("OWNER_ACTION_NOW=POWER_ON", flush=True)
                input("After the ESP32 supply is physically ON, press Enter. ")
                device.ready(expected_source_sha)
                status = device.status()
                classification = print_attempt_result(
                    args.scenario,
                    attempt,
                    window,
                    status,
                    device.fault_status(),
                )
                if classification.result not in {"PASS", "PASS_FAIL_CLOSED"}:
                    raise RunnerError(
                        "product recovery classifier rejected observed status: "
                        f"{classification}"
                    )
                if args.scenario == "run":
                    cleanup = device.command("RUN_CONTROL_DISCARD_PENDING")
                    if cleanup.get("result") != "PASS":
                        raise RunnerError(f"run cleanup failed: {cleanup}")
    except BaseException:
        update_manifest_state(
            Path(args.pre_harness_manifest),
            campaign_result="FAIL_OR_INTERRUPTED",
            production_restore_required=True,
            restore_state="PENDING_OWNER_BOOTLOADER_ACTION",
        )
        print("CAMPAIGN_RESULT=FAIL_OR_INTERRUPTED", flush=True)
        print("PRODUCTION_RESTORE_REQUIRED=YES", flush=True)
        print("RESTORE_PENDING_OWNER_BOOTLOADER_ACTION=YES", flush=True)
        raise

    update_manifest_state(
        Path(args.pre_harness_manifest),
        campaign_result="PASS",
        production_restore_required=True,
        restore_state="PENDING_OWNER_BOOTLOADER_ACTION",
    )
    print("CAMPAIGN_RESULT=PASS", flush=True)
    print("PRODUCTION_RESTORE_REQUIRED=YES", flush=True)
    print("RESTORE_PENDING_OWNER_BOOTLOADER_ACTION=YES", flush=True)
    return 0


def verify_restored_product_boot(
    args: argparse.Namespace,
    manifest: dict[str, object],
    *,
    normal_power_cycle_ready: bool = False,
) -> None:
    if not normal_power_cycle_ready:
        raise RunnerError("BLOCKED_NORMAL_POWER_CYCLE_REQUIRED_FOR_PRODUCT_BOOT")
    expected_source_sha = require_source_sha(
        str(manifest.get("production_source_sha", ""))
    )
    with ProductSerial(args.port, args.baud, args.timeout) as device:
        observed_source_sha = device.wait_for_product_boot()
    print(f"POST_RESTORE_PRODUCT_SOURCE_SHA={observed_source_sha}", flush=True)
    print(f"EXPECTED_PRODUCT_SOURCE_SHA={expected_source_sha}", flush=True)
    verify_observed_product_source_sha(expected_source_sha, observed_source_sha)
    print("POST_RESTORE_SOURCE_SHA_VERIFICATION=PASS", flush=True)
    update_manifest_state(
        Path(args.pre_harness_manifest),
        production_restore_required=False,
        restore_state="COMPLETE",
        post_restore_product_source_sha=observed_source_sha,
    )
    print("POST_RESTORE_PRODUCT_BOOT=PASS", flush=True)
    print("RESTORE_GATE=PASS", flush=True)


def run_restore(args: argparse.Namespace) -> int:
    manifest_path = Path(args.pre_harness_manifest)
    manifest = require_pre_harness_manifest(manifest_path)
    require_restore_pending(manifest)
    print("RESTORE_REQUIRED=YES", flush=True)
    rom_bootloader_ready = confirm_rom_bootloader(
        args, "CONFIRM_BOARD_READY_FOR_PRODUCTION_RESTORE"
    )
    try:
        application_sha = restore_production_layout(
            args, manifest, rom_bootloader_ready=rom_bootloader_ready
        )
    except BaseException:
        update_manifest_state(
            manifest_path,
            production_restore_required=True,
            restore_state="RESTORE_FAILED_RETRY_REQUIRED",
        )
        print("RESTORE_GATE=FAIL", flush=True)
        print("PRODUCTION_RESTORE_REQUIRED=YES", flush=True)
        raise
    update_manifest_state(
        manifest_path,
        production_restore_required=True,
        restore_state="RESTORED_PENDING_NORMAL_BOOT",
        restored_application_sha256=application_sha,
    )
    print("POST_RESTORE_PRODUCT_BOOT=NOT_RUN_OWNER_POWER_CYCLE_REQUIRED", flush=True)
    print("RESTORE_GATE=NOT_RUN_OWNER_POWER_CYCLE_REQUIRED", flush=True)
    print("OWNER_ACTION_REQUIRED=EXIT_ROM_BOOTLOADER_AND_NORMAL_POWER_CYCLE", flush=True)
    return 0


def _base_status(**overrides: str) -> dict[str, str]:
    status = {
        "application_lifecycle": "Ready",
        "configuration_recovery_status": "RuntimeReady",
        "configuration_runtime_available": "YES",
        "run_persistence_load_status": "NoPersistedRun",
        "run_load_disposition": "Standby",
        "published_process_state": "Standby",
        "actuator_release": "false",
    }
    status.update(overrides)
    return status


def _no_faults() -> dict[str, str]:
    return {key: "NONE_OBSERVED" for key in REQUIRED_FAULT_FIELDS}


def _expect_rejected(action: object, expected: str) -> None:
    try:
        action()  # type: ignore[operator]
    except RunnerError as error:
        if expected not in str(error):
            raise RunnerError(f"expected rejection {expected}, got {error}") from error
        return
    raise RunnerError(f"expected rejection: {expected}")


def selftest_backup_workflow() -> None:
    with tempfile.TemporaryDirectory(prefix="issue90-selftest-") as directory:
        root = Path(directory)
        release_dir = root / "release"
        (release_dir / "bootloader").mkdir(parents=True)
        (release_dir / "partition_table").mkdir(parents=True)
        table = b"production-partition-table" * 100
        bootloader = b"bootloader" * 100
        application = b"application" * 1000
        source_sha = "1" * 40
        release_report = root / "build-report.md"
        release_report.write_text(
            "## ESP-IDF esp32_release (JSON2-basierte IDF-Messung)\n\n"
            f"- Build-Commit: {source_sha}\n"
            f"- Source-Git-SHA: {source_sha}\n",
            encoding="utf-8",
        )
        release_report_original = release_report.read_bytes()
        (release_dir / "partition_table" / "partition-table.bin").write_bytes(table)
        (release_dir / "bootloader" / "bootloader.bin").write_bytes(bootloader)
        (release_dir / "esp32_fermentationsschrank.bin").write_bytes(
            application + source_sha.encode("ascii")
        )
        state = bytes((index % 251 for index in range(STATE_STORE_SIZE)))

        regions: dict[int, bytes] = {
            PARTITION_TABLE_OFFSET: table,
            STATE_STORE_OFFSET: state,
        }
        tamper_state_readback = False

        def fake_esptool(_port: str, _baud: int, command: list[str]) -> str:
            nonlocal tamper_state_readback
            operation = command[0]
            if operation == "chip-id":
                return "Connected to ESP32 on /dev/ttyFAKE:\nChip type: ESP32-D0WD-V3"
            if operation == "read-flash":
                offset = int(command[1], 0)
                size = int(command[2], 0)
                target = Path(command[3])
                source = bytearray(b"\xff" * size)
                for region_offset, region in regions.items():
                    start = max(offset, region_offset)
                    end = min(offset + size, region_offset + len(region))
                    if start < end:
                        source[start - offset : end - offset] = region[
                            start - region_offset : end - region_offset
                        ]
                if tamper_state_readback and offset == STATE_STORE_OFFSET:
                    source[0] ^= 0x01
                target.write_bytes(bytes(source))
            elif operation == "write-flash":
                pairs = command[1:]
                for index in range(0, len(pairs), 2):
                    regions[int(pairs[index], 0)] = Path(pairs[index + 1]).read_bytes()
            else:
                raise RunnerError(f"unexpected mocked esptool operation: {operation}")
            return ""

        prepare_args = argparse.Namespace(
            port="mock",
            baud=115200,
            production_source_sha=source_sha,
            production_partition_table=str(
                release_dir / "partition_table" / "partition-table.bin"
            ),
            production_release_dir=str(release_dir),
            backup_dir=str(root / "backup"),
            manifest_output=str(root / "manifest.json"),
            release_build_report=str(release_report),
        )
        with mock.patch(__name__ + ".run_esptool", side_effect=fake_esptool):
            with mock.patch.object(
                check_build_profiles,
                "source_tree_status",
                return_value=" M dirty-source.cpp\n",
            ):
                _expect_rejected(
                    lambda: prepare_pre_harness_backup(
                        prepare_args, rom_bootloader_ready=True
                    ),
                    "BLOCKED_DIRTY_SOURCE_TREE",
                )
            _expect_rejected(
                lambda: prepare_pre_harness_backup(prepare_args),
                "BLOCKED_ROM_BOOTLOADER_REQUIRED_FOR_PRE_HARNESS_BACKUP",
            )
            _expect_rejected(
                lambda: require_source_sha("wrong-source"),
                "invalid documented production source SHA",
            )
            wrong_source_args = argparse.Namespace(**vars(prepare_args))
            wrong_source_args.production_source_sha = "2" * 40
            _expect_rejected(
                lambda: prepare_pre_harness_backup(
                    wrong_source_args, rom_bootloader_ready=True
                ),
                "BLOCKED_RELEASE_ARTIFACT_PROVENANCE_MISMATCH",
            )
            application_path = release_dir / "esp32_fermentationsschrank.bin"
            application_with_wrong_provenance = application_path.read_bytes()
            application_path.write_bytes(application)
            _expect_rejected(
                lambda: prepare_pre_harness_backup(
                    prepare_args, rom_bootloader_ready=True
                ),
                "BLOCKED_RELEASE_ARTIFACT_PROVENANCE_MISMATCH",
            )
            application_path.write_bytes(application_with_wrong_provenance)
            with contextlib.redirect_stdout(io.StringIO()):
                prepare_pre_harness_backup(
                    prepare_args, rom_bootloader_ready=True
                )
            manifest_path = Path(prepare_args.manifest_output)
            manifest = load_pre_harness_manifest(manifest_path)
            _expect_rejected(
                lambda: require_pre_harness_manifest(None),
                "BLOCKED_PRE_HARNESS_BACKUP_REQUIRED",
            )

            wrong_layout = root / "wrong-layout.json"
            wrong_data = json.loads(manifest_path.read_text(encoding="utf-8"))
            wrong_data["backup_source_state"] = "HARNESS_LAYOUT"
            wrong_layout.write_text(json.dumps(wrong_data), encoding="utf-8")
            _expect_rejected(
                lambda: load_pre_harness_manifest(wrong_layout),
                "BACKUP_SOURCE_STATE",
            )

            wrong_hash = root / "wrong-hash.json"
            wrong_data = json.loads(manifest_path.read_text(encoding="utf-8"))
            wrong_data["backups"]["production_state_store"]["sha256"] = "0" * 64
            wrong_hash.write_text(json.dumps(wrong_data), encoding="utf-8")
            _expect_rejected(lambda: load_pre_harness_manifest(wrong_hash), "hash mismatch")

            missing_table = root / "missing-table.json"
            missing_data = json.loads(manifest_path.read_text(encoding="utf-8"))
            missing_data["backups"]["production_partition_table"]["file"] = str(
                root / "does-not-exist.bin"
            )
            missing_table.write_text(json.dumps(missing_data), encoding="utf-8")
            _expect_rejected(
                lambda: load_pre_harness_manifest(missing_table),
                "production partition-table backup missing",
            )

            release_report.write_bytes(release_report_original + b"stale\n")
            _expect_rejected(
                lambda: load_pre_harness_manifest(manifest_path),
                "release build provenance file hash mismatch",
            )
            release_report.write_bytes(release_report_original)

            restore_args = argparse.Namespace(
                port="mock",
                baud=115200,
                timeout=1.0,
                pre_harness_manifest=str(manifest_path),
            )
            _expect_rejected(
                lambda: restore_production_layout(restore_args, manifest),
                "BLOCKED_ROM_BOOTLOADER_REQUIRED_FOR_PRODUCTION_RESTORE",
            )
            restore_missing_table = json.loads(
                json.dumps(manifest)
            )
            restore_missing_table["backups"]["production_partition_table"][
                "file"
            ] = str(root / "missing-production-partition-table.bin")
            _expect_rejected(
                lambda: restore_production_layout(
                    restore_args,
                    restore_missing_table,
                    rom_bootloader_ready=True,
                ),
                "production partition-table backup missing",
            )
            tamper_state_readback = True
            _expect_rejected(
                lambda: restore_production_layout(
                    restore_args, manifest, rom_bootloader_ready=True
                ),
                "production state-store restore read-back hash mismatch",
            )
            tamper_state_readback = False
            with contextlib.redirect_stdout(io.StringIO()):
                restore_production_layout(
                    restore_args, manifest, rom_bootloader_ready=True
                )
            _expect_rejected(
                lambda: verify_restored_product_boot(restore_args, manifest),
                "BLOCKED_NORMAL_POWER_CYCLE_REQUIRED_FOR_PRODUCT_BOOT",
            )
            _expect_rejected(
                lambda: verify_observed_product_source_sha(source_sha, "2" * 40),
                "POST_RESTORE_PRODUCT_SOURCE_SHA_MISMATCH",
            )
            update_manifest_state(
                manifest_path,
                campaign_result="FAIL_OR_INTERRUPTED",
                production_restore_required=True,
                restore_state="PENDING_OWNER_BOOTLOADER_ACTION",
            )

            class FailingProductSerial:
                def __enter__(self) -> "FailingProductSerial":
                    return self

                def __exit__(self, *_: object) -> None:
                    return None

                def ready(self, _expected_source_sha: str) -> dict[str, str]:
                    raise RunnerError("selftest campaign interruption")

            campaign_args = argparse.Namespace(
                port="mock",
                baud=115200,
                timeout=1.0,
                profile="esp32_bringup",
                scenario="config",
                attempts=3,
                pre_harness_manifest=str(manifest_path),
            )
            with mock.patch.object(
                check_build_profiles, "source_tree_status", return_value=""
            ):
                with mock.patch(
                    __name__ + ".ProductSerial",
                    return_value=FailingProductSerial(),
                ):
                    _expect_rejected(
                        lambda: run_physical_campaign(campaign_args),
                        "selftest campaign interruption",
                    )
            failure_manifest = load_pre_harness_manifest(manifest_path)
            if (
                failure_manifest.get("production_restore_required") is not True
                or failure_manifest.get("campaign_result") != "FAIL_OR_INTERRUPTED"
                or failure_manifest.get("restore_state")
                != "PENDING_OWNER_BOOTLOADER_ACTION"
            ):
                raise RunnerError("campaign failure did not retain restore requirement")
            update_manifest_state(
                manifest_path,
                campaign_result="PASS",
                production_restore_required=True,
                restore_state="RESTORED_PENDING_NORMAL_BOOT",
                restored_application_sha256=sha256_file(application_path),
            )

            class FakeProductSerial:
                def __init__(self, observed_source_sha: str) -> None:
                    self.observed_source_sha = observed_source_sha

                def __enter__(self) -> "FakeProductSerial":
                    return self

                def __exit__(self, *_: object) -> None:
                    return None

                def wait_for_product_boot(self) -> str:
                    return self.observed_source_sha

            with mock.patch(
                __name__ + ".ProductSerial",
                return_value=FakeProductSerial("2" * 40),
            ):
                _expect_rejected(
                    lambda: verify_restored_product_boot(
                        restore_args,
                        manifest,
                        normal_power_cycle_ready=True,
                    ),
                    "POST_RESTORE_PRODUCT_SOURCE_SHA_MISMATCH",
                )
            with mock.patch(
                __name__ + ".ProductSerial",
                return_value=FakeProductSerial(source_sha),
            ):
                verify_restored_product_boot(
                    restore_args,
                    manifest,
                    normal_power_cycle_ready=True,
                )
            complete_manifest = load_pre_harness_manifest(manifest_path)
            if (
                complete_manifest.get("restore_state") != "COMPLETE"
                or complete_manifest.get("production_restore_required") is not False
                or complete_manifest.get("post_restore_product_source_sha") != source_sha
            ):
                raise RunnerError("successful post-restore provenance was not committed")
    print("PASS: pre-harness backup required before campaign", flush=True)
    print("PASS: backup source/layout/hash rejection selftests", flush=True)
    print("PASS: complete production restore sequence selftest", flush=True)
    print("PASS: ROM bootloader and restore phase state selftests", flush=True)
    print("PASS: exact release/post-restore provenance selftests", flush=True)
    print("PASS: proof build/prepare rejects dirty source tree", flush=True)


def selftest_recovery_classifier() -> None:
    faults = _no_faults()
    valid_configuration = classify_product_recovery("config", _base_status(), faults)
    if valid_configuration.result != "PASS":
        raise RunnerError(f"valid configuration rejected: {valid_configuration}")
    for config_status in (
        "RuntimeReady",
        "FactoryInitializationCompleted",
        "FactoryResetCompleted",
    ):
        status = _base_status(configuration_recovery_status=config_status)
        result = classify_product_recovery("config", status, faults)
        if result.result != "PASS":
            raise RunnerError(f"valid old/new configuration rejected: {result}")

    valid_run = _base_status(
        run_persistence_load_status="Current",
        run_load_disposition="ResumeOffer",
        published_process_state="NONE",
    )
    if classify_product_recovery("run", valid_run, faults).result != "PASS":
        raise RunnerError("valid resume offer rejected")
    for status in (
        _base_status(),
        _base_status(
            run_persistence_load_status="NoActiveRun",
            run_load_disposition="Standby",
        ),
        _base_status(
            run_persistence_load_status="Current",
            run_load_disposition="NoActiveRun",
        ),
    ):
        if classify_product_recovery("run", status, faults).result != "PASS":
            raise RunnerError(f"valid standby/run outcome rejected: {status}")

    recovery_evaluation = _base_status(
        run_persistence_load_status="Current",
        run_load_disposition="RecoveryEvaluation",
        published_process_state="RecoveryEvaluation",
    )
    recovery_evaluation_classification = classify_product_recovery(
        "run", recovery_evaluation, faults
    )
    if (
        recovery_evaluation_classification.result != "BLOCKED"
        or recovery_evaluation_classification.classification
        != "RECOVERY_EVALUATION_REQUIRES_OWNER_REVIEW"
    ):
        raise RunnerError(
            f"RecoveryEvaluation disposition not routed to owner review: "
            f"{recovery_evaluation_classification}"
        )

    config_fail_closed = _base_status(
        application_lifecycle="ServiceRequired",
        configuration_recovery_status="ConfigurationIntegrityFailure",
        configuration_runtime_available="NO",
        run_persistence_load_status="NOT_AVAILABLE",
        run_load_disposition="SafeBoot",
        published_process_state="NONE",
    )
    if (
        classify_product_recovery("config", config_fail_closed, faults).result
        != "PASS_FAIL_CLOSED"
    ):
        raise RunnerError("configuration fail-closed outcome rejected")

    run_fail_closed = _base_status(
        application_lifecycle="ServiceRequired",
        run_persistence_load_status="FallbackRecovered",
        run_load_disposition="SafeBoot",
        published_process_state="NONE",
    )
    if (
        classify_product_recovery("run", run_fail_closed, faults).result
        != "PASS_FAIL_CLOSED"
    ):
        raise RunnerError("run fail-closed outcome rejected")

    if classify_product_recovery(
        "run",
        _base_status(
            run_persistence_load_status="Current",
            run_load_disposition="ResumeOffer",
            published_process_state="Standby",
        ),
        faults,
    ).result != "FAIL":
        raise RunnerError("published NONE/Standby semantic counterexample accepted")
    if classify_product_recovery(
        "config", _base_status(published_process_state="NONE"), faults
    ).result != "FAIL":
        raise RunnerError("configuration runtime was mistaken for record validation")
    if classify_product_recovery(
        "config", _base_status(actuator_release="true"), faults
    ).result != "FAIL":
        raise RunnerError("actuator release was not rejected")
    if classify_product_recovery(
        "config", _base_status(run_load_disposition="Unknown"), faults
    ).result != "BLOCKED":
        raise RunnerError("unknown status was not blocked")
    incomplete = _base_status()
    del incomplete["published_process_state"]
    if classify_product_recovery("config", incomplete, faults).result != "BLOCKED":
        raise RunnerError("missing raw status was not blocked")
    print("PASS: explicit configuration/run recovery classifier matrix", flush=True)
    print("PASS: published NONE and fail-closed semantic counterexamples", flush=True)


def selftest() -> int:
    source_sha = "1" * 40
    require_clean_source_tree()
    status = _base_status()
    status_line = (
        "I issue90: ISSUE90_STATUS application_started=YES "
        + " ".join(f"{key}={value}" for key, value in status.items())
    )
    parsed = marker_fields(status_line, "ISSUE90_STATUS")
    require_actor_free(parsed)
    if classify_product_recovery("config", parsed, _no_faults()).result != "PASS":
        raise RunnerError("status parser/classifier failed")
    ready = marker_fields(
        "I issue90: ISSUE90_READY partition=state_store_test actor_free=YES "
        "real_actuators_enabled=NO backup_required=YES "
        "partition_offset=0x300000 partition_size=0x100000 "
        "test_partition_physically_separate=NO "
        "test_partition_reuses_production_flash_range=YES "
        f"source_sha={source_sha} "
        "load_stop=STOP_OR_PHYSICAL_POWER_CUT",
        "ISSUE90_READY",
    )
    if ready.get("partition") != TEST_PARTITION:
        raise RunnerError("ready parser failed")
    if ready.get("backup_required") != "YES":
        raise RunnerError("backup safety parser failed")
    if ready.get("test_partition_physically_separate") != "NO":
        raise RunnerError("physical partition claim parser failed")
    if ready.get("source_sha") != source_sha:
        raise RunnerError("harness source SHA parser failed")
    for observed_source_sha in ("", "not-a-sha", "2" * 40):
        _expect_rejected(
            lambda observed=observed_source_sha: verify_harness_source_sha(
                source_sha, observed
            ),
            "BLOCKED_HARNESS_SOURCE_SHA_MISMATCH",
        )
    if verify_harness_source_sha(source_sha, source_sha) != source_sha:
        raise RunnerError("exact harness source SHA was not accepted")
    selftest_backup_workflow()
    selftest_recovery_classifier()
    print("PASS: clean source tree accepted", flush=True)
    print("PASS: Issue #90 Slice-7 UART field parser", flush=True)
    print("PASS: exact harness source SHA field and verification", flush=True)
    print("PASS: actor-free and reused-flash-range guard", flush=True)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument(
        "--phase",
        choices=("prepare", "campaign", "restore", "verify-restored-boot"),
        default="campaign",
    )
    parser.add_argument("--port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--profile", choices=("esp32_bringup",), default="esp32_bringup")
    parser.add_argument("--scenario", choices=("config", "run"), default="config")
    parser.add_argument("--attempts", type=int, default=3)
    parser.add_argument("--pre-harness-manifest")
    parser.add_argument(
        "--manifest-output",
        default="build/issue90_slice7_hardware/pre_harness_manifest.json",
    )
    parser.add_argument(
        "--backup-dir", default="build/issue90_slice7_hardware/pre_harness_backup"
    )
    parser.add_argument(
        "--production-partition-table",
        default="build/esp32_release/partition_table/partition-table.bin",
    )
    parser.add_argument("--production-release-dir", default="build/esp32_release")
    parser.add_argument("--release-build-report", default="build-report.md")
    parser.add_argument("--production-source-sha")
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    if not args.port:
        parser.error("--port is required unless --selftest is used")
    try:
        if args.phase == "prepare":
            if not args.production_source_sha:
                parser.error("--production-source-sha is required for --phase prepare")
            rom_bootloader_ready = confirm_rom_bootloader(
                args, "CONFIRM_BOARD_READY_FOR_PRE_HARNESS_BACKUP"
            )
            return prepare_pre_harness_backup(
                args, rom_bootloader_ready=rom_bootloader_ready
            )
        if args.phase == "campaign":
            if not args.pre_harness_manifest:
                raise RunnerError("BLOCKED_PRE_HARNESS_BACKUP_REQUIRED")
            return run_physical_campaign(args)
        if not args.pre_harness_manifest:
            raise RunnerError("BLOCKED_PRE_HARNESS_BACKUP_REQUIRED")
        if args.phase == "restore":
            return run_restore(args)
        manifest_path = Path(args.pre_harness_manifest)
        manifest = load_pre_harness_manifest(manifest_path)
        if manifest.get("restore_state") != "RESTORED_PENDING_NORMAL_BOOT":
            raise RunnerError("BLOCKED_RESTORE_NOT_VERIFIED_BEFORE_NORMAL_PRODUCT_BOOT")
        confirm_normal_power_cycle()
        verify_restored_product_boot(
            args, manifest, normal_power_cycle_ready=True
        )
        return 0
    except RunnerError as error:
        print(f"BLOCKED/FAILED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
