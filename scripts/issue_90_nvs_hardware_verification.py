#!/usr/bin/env python3
"""Run the non-release Issue #90 UART verification harness on an ESP32.

The runner deliberately treats the firmware UART as a versioned machine
contract.  Power-cut timing is obtained from an explicit calibration artifact;
the controller hook is only the transport for ARM/TRIP/RESTORE and does not
carry hidden window semantics.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable


EXPECTED_IDF_SHA = "7101770dc6db2667b3c477cc31365dd1acd6db4e"
PLAN_SHA = "da693e8a24735ff2cc09f019b119083f3792882e"
EXPECTED_PARTITION = {
    "partition_label": "state_store",
    "namespace": "fermentation",
    "partition_type": "1",
    "partition_subtype": "2",
    "offset": "65536",
    "size": "282624",
    "pages": "69",
    "page_bytes": "4096",
}
EXPECTED_KEYS = (
    "uc0", "uc1", "uc2", "uc3", "sc0", "sc1", "sc2", "sc3", "pc0",
    "pc1", "pc2", "pc3", "cm0", "cm1", "cm2", "cr0", "cr1", "cb0",
    "cb1", "rc0", "rc1", "rh0",
)
EXPECTED_SIZES = dict(zip(
    EXPECTED_KEYS,
    (301, 301, 301, 301, 45, 45, 45, 45, 32813, 32813, 32813, 32813,
     149, 149, 149, 114, 114, 42, 42, 8240, 8240, 256),
))
WINDOWS = ("blob_data", "blob_index", "old_removal", "gc_erase")
HEX64 = set("0123456789abcdef")


def root() -> Path:
    return Path(__file__).resolve().parents[1]


def git_source_sha(repo: Path) -> str:
    status = subprocess.run(
        ["git", "status", "--porcelain", "--untracked-files=all"],
        cwd=repo, text=True, capture_output=True, check=True,
    )
    if status.stdout.strip():
        raise SystemExit(
            "hardware verification requires a clean committed checkout; "
            "source provenance would otherwise include uncommitted files"
        )
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=repo, text=True,
        capture_output=True, check=True,
    )
    return result.stdout.strip()


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def require_relative(repo: Path, value: Path, option: str) -> Path:
    if value.is_absolute():
        raise SystemExit(f"{option} must be repository-relative")
    resolved = (repo / value).resolve()
    if repo.resolve() not in resolved.parents and resolved != repo.resolve():
        raise SystemExit(f"{option} must remain below the repository root")
    return resolved


def build(repo: Path, build_dir: Path, source_sha: str) -> None:
    if not os.environ.get("IDF_PATH"):
        raise SystemExit("IDF_PATH must be activated with the pinned ESP-IDF export.sh")
    sdkconfig = build_dir / "sdkconfig"
    defaults = "sdkconfig.defaults;sdkconfig.defaults.bringup"
    command = [
        "idf.py", "-B", str(build_dir), f"-DSDKCONFIG={sdkconfig}",
        f"-DSDKCONFIG_DEFAULTS={defaults}",
        "-DAPP_ISSUE_90_NVS_HARDWARE_TEST=1",
        f"-DAPP_ISSUE_90_SOURCE_GIT_SHA={source_sha}",
        f"-DAPP_ISSUE_90_PLAN_SHA={PLAN_SHA}", "build",
    ]
    build_env = os.environ.copy()
    # ESP-IDF's early component-requirement pass does not carry arbitrary
    # -Dcache variables.  Mirror the explicit harness switch in the
    # environment so main/CMakeLists.txt can keep harness dependencies out of
    # the release graph during that pass as well.
    build_env["APP_ISSUE_90_NVS_HARDWARE_TEST"] = "1"
    subprocess.run(command, cwd=repo, env=build_env, check=True)


def parse_fields(line: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for field in line.split():
        if "=" not in field:
            continue
        key, value = field.split("=", 1)
        if key in fields:
            raise RuntimeError(f"duplicate UART field {key}: {line}")
        fields[key] = value
    return fields


def require_sha(value: str, field: str) -> str:
    if len(value) != 64 or any(char not in HEX64 for char in value):
        raise RuntimeError(f"invalid {field}: {value!r}")
    return value


def parse_int(fields: dict[str, str], field: str) -> int:
    try:
        return int(fields[field], 10)
    except (KeyError, ValueError) as error:
        raise RuntimeError(f"invalid UART integer {field}: {fields}") from error


class Harness:
    def __init__(self, serial_port, timeout: float, uart_log: Path) -> None:
        self.serial = serial_port
        self.timeout = timeout
        self.uart_log = uart_log

    def send(self, command: str) -> None:
        self.serial.write((command + "\n").encode("ascii"))

    def _read_line(self) -> str | None:
        raw = self.serial.readline()
        if not raw:
            return None
        line = raw.decode("utf-8", errors="replace").strip()
        with self.uart_log.open("a", encoding="utf-8") as stream:
            stream.write(line + "\n")
        return line

    def wait_for_prefix(self, prefix: str) -> list[str]:
        deadline = time.monotonic() + self.timeout
        lines: list[str] = []
        while time.monotonic() < deadline:
            line = self._read_line()
            if line is None:
                continue
            lines.append(line)
            if line.startswith(prefix):
                return lines
            if line.startswith("ISSUE90 ") and "status=FAIL" in line:
                raise RuntimeError(line)
        raise TimeoutError(f"timeout waiting for {prefix}; last={lines[-5:]}")

    def ready(self, source_sha: str) -> dict[str, str]:
        lines = self.wait_for_prefix("ISSUE90 READY protocol=1 ")
        line = lines[-1]
        fields = parse_fields(line)
        required = {"idf_sha", "source_sha", "plan_sha", "profile", *EXPECTED_PARTITION,
                    "flash_size_bytes"}
        if not required.issubset(fields):
            raise RuntimeError(f"READY contract incomplete: {line}")
        if fields["idf_sha"] != EXPECTED_IDF_SHA:
            raise RuntimeError(f"unexpected IDF SHA in READY: {fields}")
        if fields["source_sha"] != source_sha or fields["plan_sha"] != PLAN_SHA:
            raise RuntimeError(f"unexpected provenance in READY: {fields}")
        if fields["profile"] != "bringup":
            raise RuntimeError(f"unexpected profile in READY: {fields}")
        for field, expected in EXPECTED_PARTITION.items():
            if fields[field] != expected:
                raise RuntimeError(f"unexpected partition field {field}: {fields}")
        if parse_int(fields, "flash_size_bytes") < 282624 + 65536:
            raise RuntimeError(f"partition does not fit in reported flash: {fields}")
        return fields

    def readback(self) -> dict[str, tuple[int, str]]:
        self.send("READBACK_ALL")
        lines = self.wait_for_prefix("ISSUE90 READBACK_RESULT protocol=1 ")
        values: dict[str, tuple[int, str]] = {}
        for line in lines:
            prefix = "ISSUE90 READBACK protocol=1 "
            if not line.startswith(prefix):
                continue
            fields = parse_fields(line)
            key = fields.get("key")
            if key not in EXPECTED_KEYS or key in values:
                raise RuntimeError(f"invalid or duplicate READBACK key: {line}")
            if fields.get("status") != "Success":
                raise RuntimeError(f"READBACK did not succeed: {line}")
            length = parse_int(fields, "length")
            if length != EXPECTED_SIZES[key]:
                raise RuntimeError(f"READBACK length mismatch: {line}")
            digest = require_sha(fields.get("sha256", ""), "READBACK sha256")
            values[key] = (length, digest)
        result = parse_fields(lines[-1])
        if result.get("status") != "PASS" or parse_int(result, "count") != len(EXPECTED_KEYS):
            raise RuntimeError(f"READBACK_RESULT contract failed: {lines[-1]}")
        if set(values) != set(EXPECTED_KEYS):
            missing = [key for key in EXPECTED_KEYS if key not in values]
            raise RuntimeError(f"incomplete READBACK_ALL; missing={missing}")
        return values

    def page_evidence(self) -> dict[int, dict[str, str]]:
        self.send("PAGE_EVIDENCE")
        lines = self.wait_for_prefix("ISSUE90 PAGE_EVIDENCE_RESULT protocol=1 ")
        pages: dict[int, dict[str, str]] = {}
        for line in lines:
            if not line.startswith("ISSUE90 PAGE_EVIDENCE protocol=1 "):
                continue
            fields = parse_fields(line)
            page = parse_int(fields, "page")
            if page not in range(69):
                raise RuntimeError(f"PAGE_EVIDENCE page out of range: {line}")
            if page in pages:
                raise RuntimeError(f"duplicate page evidence: {line}")
            require_sha(fields.get("sha256", ""), "page sha256")
            for field in (
                "valid", "all_erased", "header_crc_valid", "entries_crc_valid",
                "state", "sequence", "written_entries", "erased_entries",
                "live_mask", "blob_data_mask", "blob_index_mask", "removed_mask",
            ):
                if field not in fields:
                    raise RuntimeError(f"incomplete PAGE_EVIDENCE contract: {line}")
                try:
                    int(fields[field], 16 if field in {
                        "state", "live_mask", "blob_data_mask",
                        "blob_index_mask", "removed_mask",
                    } else 10)
                except ValueError as error:
                    raise RuntimeError(
                        f"invalid PAGE_EVIDENCE field {field}: {line}"
                    ) from error
            counts = fields.get("live_counts", "").split(",")
            if len(counts) != len(EXPECTED_KEYS) or any(
                not item.isdigit() for item in counts
            ):
                raise RuntimeError(f"invalid PAGE_EVIDENCE live_counts: {line}")
            pages[page] = fields
        result = parse_fields(lines[-1])
        if result.get("status") != "PASS" or parse_int(result, "pages") != 69:
            raise RuntimeError(f"PAGE_EVIDENCE_RESULT contract failed: {lines[-1]}")
        if set(pages) != set(range(69)):
            raise RuntimeError("PAGE_EVIDENCE did not return exactly 69 pages")
        return pages

    def rotate_begin(self, lines: Iterable[str]) -> dict[str, str]:
        candidates = [line for line in lines if line.startswith(
            "ISSUE90 ROTATE_BEGIN protocol=1 ")]
        if len(candidates) != 1:
            raise RuntimeError(f"expected one ROTATE_BEGIN, got {candidates}")
        fields = parse_fields(candidates[0])
        for field in ("sequence", "rotation", "target_rotation", "max_writes",
                      "old_length", "new_length"):
            parse_int(fields, field)
        if fields.get("key") not in EXPECTED_KEYS:
            raise RuntimeError(f"invalid ROTATE_BEGIN key: {fields}")
        require_sha(fields.get("old_sha256", ""), "old_sha256")
        require_sha(fields.get("new_sha256", ""), "new_sha256")
        return fields

    def wait_for_gc_result(self) -> tuple[list[str], dict[str, str]]:
        deadline = time.monotonic() + self.timeout
        lines: list[str] = []
        gc_evidence: dict[str, str] | None = None
        while time.monotonic() < deadline:
            line = self._read_line()
            if line is None:
                continue
            lines.append(line)
            if line.startswith("ISSUE90 GC_ERASE_DETECTED protocol=1 "):
                gc_evidence = parse_fields(line)
                required = {
                    "old_page", "old_state", "old_seq", "old_written",
                    "old_live_mask", "old_blob_data_mask", "old_blob_index_mask",
                    "old_sha256", "new_page", "new_state", "new_seq",
                    "new_written", "new_live_mask", "new_blob_data_mask",
                    "new_blob_index_mask", "new_removed_mask", "old_live_counts",
                    "new_live_counts", "new_sha256", "stats_total", "stats_used",
                    "stats_free", "stats_available", "stats_namespaces",
                }
                if not required.issubset(gc_evidence):
                    raise RuntimeError(f"incomplete GC_ERASE_DETECTED: {line}")
                require_sha(gc_evidence["old_sha256"], "old_sha256")
                require_sha(gc_evidence["new_sha256"], "new_sha256")
                for field in ("old_state", "new_state", "old_live_mask",
                              "old_blob_data_mask", "old_blob_index_mask",
                              "new_live_mask", "new_blob_data_mask",
                              "new_blob_index_mask", "new_removed_mask"):
                    int(gc_evidence[field], 16)
                for field in ("old_page", "old_seq", "old_written", "new_page",
                              "new_seq", "new_written", "stats_total",
                              "stats_used", "stats_free", "stats_available",
                              "stats_namespaces"):
                    int(gc_evidence[field], 10)
                for field in ("old_live_counts", "new_live_counts"):
                    counts = gc_evidence[field].split(",")
                    if len(counts) != len(EXPECTED_KEYS) or any(
                        not item.isdigit() for item in counts
                    ):
                        raise RuntimeError(f"invalid GC live counts: {line}")
            if not line.startswith("ISSUE90 ROTATE_RESULT protocol=1 "):
                continue
            fields = parse_fields(line)
            if fields.get("status") == "PASS" and "writes" in fields:
                if gc_evidence is None:
                    raise RuntimeError("GC result was not preceded by GC_ERASE_DETECTED")
                return lines, fields
            if fields.get("status") == "FAIL":
                raise RuntimeError(line)
        raise TimeoutError(f"timeout waiting for GC ROTATE_RESULT; last={lines[-5:]}")

    def require_uart_loss(self, seconds: float) -> None:
        deadline = time.monotonic() + seconds
        observed: list[str] = []
        while time.monotonic() < deadline:
            line = self._read_line()
            if line is not None:
                observed.append(line)
                if line.startswith("ISSUE90 ROTATE_RESULT protocol=1 "):
                    raise RuntimeError("UART remained live through ROTATE_RESULT")
        if observed:
            raise RuntimeError(f"UART did not become silent after TRIP: {observed[-5:]}")


def run_hook(repo: Path, hook: Path, command: str, hook_log: Path) -> str:
    result = subprocess.run(
        [sys.executable, str(hook)], cwd=repo, input=command + "\n",
        text=True, capture_output=True, check=False,
    )
    with hook_log.open("a", encoding="utf-8") as stream:
        stream.write(f"{utc_now()} command={command} rc={result.returncode}\n")
        stream.write(result.stdout)
        stream.write(result.stderr)
    if result.returncode != 0:
        raise RuntimeError(f"power hook failed for {command!r}: {result.stderr.strip()}")
    expected = {"ARM": "ARMED", "TRIP": "TRIPPED", "RESTORE": "RESTORED"}
    action = command.split()[0]
    if result.stdout.strip() != expected[action]:
        raise RuntimeError(f"power hook protocol mismatch for {command!r}: {result.stdout!r}")
    return result.stdout.strip()


def require_old_or_new(
    before: dict[str, tuple[int, str]],
    after: dict[str, tuple[int, str]],
    key: str,
    replacement: tuple[int, str],
) -> None:
    for inventory_key in EXPECTED_KEYS:
        allowed = {before[inventory_key]}
        if inventory_key == key:
            allowed.add(replacement)
        if after[inventory_key] not in allowed:
            raise RuntimeError(
                f"power-cut oracle failed for {inventory_key}: {after[inventory_key]}"
            )


def classify_transition(
    before: dict[int, dict[str, str]], after: dict[int, dict[str, str]],
) -> str | None:
    erased = [
        page for page in range(69)
        if before[page]["valid"] == "1"
        and before[page]["all_erased"] == "0"
        and before[page]["header_crc_valid"] == "1"
        and before[page]["entries_crc_valid"] == "1"
        and int(before[page]["written_entries"]) > 0
        and after[page]["all_erased"] == "1"
        and before[page]["sha256"] != after[page]["sha256"]
    ]
    if erased:
        for old_page in erased:
            old = before[old_page]
            old_live = int(old["live_mask"], 16)
            old_data = int(old["blob_data_mask"], 16)
            old_index = int(old["blob_index_mask"], 16)
            old_counts = tuple(int(item) for item in old["live_counts"].split(","))
            for page in range(69):
                candidate = after[page]
                if (candidate["valid"] == "1" and candidate["all_erased"] == "0"
                        and candidate["header_crc_valid"] == "1"
                        and candidate["entries_crc_valid"] == "1"
                        and int(candidate["written_entries"]) > 0
                        and int(candidate["sequence"]) > int(old["sequence"])
                        and int(candidate["live_mask"], 16) & old_live == old_live
                        and int(candidate["blob_data_mask"], 16) & old_data == old_data
                        and int(candidate["blob_index_mask"], 16) & old_index == old_index):
                    candidate_counts = tuple(
                        int(item) for item in candidate["live_counts"].split(",")
                    )
                    if (len(candidate_counts) == len(old_counts) and
                            all(new >= previous for new, previous in
                                zip(candidate_counts, old_counts)) and
                            int(candidate["written_entries"]) >=
                                int(old["written_entries"])):
                        return "gc_erase"
    changed_data = any(
        before[page]["blob_data_mask"] != after[page]["blob_data_mask"]
        for page in range(69)
    )
    changed_index = any(
        before[page]["blob_index_mask"] != after[page]["blob_index_mask"]
        for page in range(69)
    )
    changed_removed = any(
        before[page]["removed_mask"] != after[page]["removed_mask"]
        for page in range(69)
    )
    candidates = [
        window for window, changed in (
            ("blob_data", changed_data), ("blob_index", changed_index),
            ("old_removal", changed_removed),
        ) if changed
    ]
    return candidates[0] if len(candidates) == 1 else None


def parse_delays(value: str) -> tuple[int, ...]:
    try:
        delays = tuple(int(item) for item in value.split(",") if item)
    except ValueError as error:
        raise SystemExit("--calibration-delays must be comma-separated integers") from error
    if not delays or any(delay < 0 for delay in delays):
        raise SystemExit("--calibration-delays must contain non-negative values")
    return delays


def calibration_path(repo: Path, value: Path) -> Path:
    return require_relative(repo, value, "--calibration")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--uart-loss-timeout", type=float, default=1.0)
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--build-dir", type=Path, default=Path("build/issue_90_hw"))
    parser.add_argument("--repetitions", type=int, default=10)
    parser.add_argument("--windows", default=",".join(WINDOWS))
    parser.add_argument("--power-cut-hook", type=Path,
                        default=Path("scripts/issue_90_power_cut_hook.py"))
    parser.add_argument("--profile", choices=("esp32_bringup",), default="esp32_bringup")
    parser.add_argument("--scenario", choices=("prefilled_gc",), default="prefilled_gc")
    parser.add_argument("--artifact-dir", type=Path,
                        default=Path("build/issue_90_hardware_verification"))
    parser.add_argument("--calibration", type=Path,
                        default=Path("build/issue_90_hardware_verification/calibration.json"))
    parser.add_argument("--calibrate", action="store_true")
    parser.add_argument("--calibration-delays", default="0,100,250,500,1000,2000,5000,10000")
    args = parser.parse_args()
    if args.repetitions < 10:
        raise SystemExit("--repetitions must be at least 10 for the hardware matrix")
    selected_windows = tuple(window for window in args.windows.split(",") if window)
    if not selected_windows or any(window not in WINDOWS for window in selected_windows):
        raise SystemExit(f"--windows must be selected from {','.join(WINDOWS)}")
    repo = root()
    if args.profile != "esp32_bringup" or args.scenario != "prefilled_gc":
        raise SystemExit("only the approved bring-up prefilled_gc scenario is supported")
    source_sha = git_source_sha(repo)
    build_dir = args.build_dir if args.build_dir.is_absolute() else repo / args.build_dir
    hook = require_relative(repo, args.power_cut_hook, "--power-cut-hook")
    artifact_root = require_relative(repo, args.artifact_dir, "--artifact-dir")
    calibration_file = calibration_path(repo, args.calibration)
    artifact_root.mkdir(parents=True, exist_ok=True)
    if args.build:
        build(repo, build_dir, source_sha)
    try:
        import serial  # type: ignore
    except ImportError as error:
        raise SystemExit("pyserial is required for the owner-supplied UART runner") from error

    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + f"-{os.getpid()}"
    run_dir = artifact_root / f"run-{run_id}"
    run_dir.mkdir(parents=False, exist_ok=False)
    uart_log = run_dir / "uart.log"
    hook_log = run_dir / "hook.log"
    results: list[dict[str, object]] = []
    manifest: dict[str, object] = {
        "run_id": run_id, "started_at": utc_now(), "source_git_sha": source_sha,
        "firmware_source_sha": source_sha, "plan_sha": PLAN_SHA,
        "idf_sha": EXPECTED_IDF_SHA, "profile": args.profile,
        "artifact_paths": {"uart": "uart.log", "hook": "hook.log"},
        "windows": selected_windows, "repetitions": args.repetitions,
        "power_cut_hook": str(hook.relative_to(repo)),
        "controller_contract": "ARM/TRIP/RESTORE stdout tokens",
        "status": "RUNNING", "repetitions_status": results,
    }

    def save_manifest() -> None:
        manifest["finished_at"] = utc_now()
        (run_dir / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        (run_dir / "results.json").write_text(
            json.dumps(results, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

    def save_failure_artifact() -> None:
        if manifest.get("status") == "RUNNING":
            manifest["status"] = "FAIL"
            manifest["error"] = "runner terminated before a PASS/NOT_RUN status"
            save_manifest()

    # Keep the run directory useful even when a UART, hook, or contract check
    # fails.  The runner itself still raises the original failure; this hook
    # only closes the reproducible artifact with a fail-closed status.
    import atexit
    atexit.register(save_failure_artifact)

    with serial.Serial(args.port, args.baud, timeout=0.1) as port:
        harness = Harness(port, args.timeout, uart_log)
        ready = harness.ready(source_sha)
        manifest["partition"] = {field: ready[field] for field in EXPECTED_PARTITION}
        harness.send("RESET_PARTITION")
        reset_lines = harness.wait_for_prefix("ISSUE90 TEST_RESET protocol=1 ")
        if parse_fields(reset_lines[-1]).get("status") != "PASS":
            raise RuntimeError("test reset did not pass")
        harness.send("PREFILL seed=0")
        prefill = harness.wait_for_prefix("ISSUE90 PREFILL_DONE protocol=1 status=PASS ")
        if parse_fields(prefill[-1]).get("seed") != "0":
            raise RuntimeError("unexpected PREFILL seed")
        baseline = harness.readback()
        manifest["prefill_baseline"] = baseline
        for control in range(3):
            harness.send("REBOOT")
            harness.ready(source_sha)
            reboot_values = harness.readback()
            if reboot_values != baseline:
                raise RuntimeError(f"clean reboot {control} changed PREFILL baseline")
        if args.calibrate:
            delays = parse_delays(args.calibration_delays)
            calibration: dict[str, object] = {
                "run_id": run_id, "created_at": utc_now(), "source_git_sha": source_sha,
                "plan_sha": PLAN_SHA, "idf_sha": EXPECTED_IDF_SHA,
                "windows": {},
            }
            accepted: set[str] = set()
            target_rotations = {window: 0 for window in selected_windows}
            if "gc_erase" in selected_windows:
                harness.send("RESET_PARTITION")
                reset = harness.wait_for_prefix("ISSUE90 TEST_RESET protocol=1 ")
                if parse_fields(reset[-1]).get("status") != "PASS":
                    raise RuntimeError("GC calibration reset did not pass")
                harness.send("PREFILL seed=0")
                harness.wait_for_prefix("ISSUE90 PREFILL_DONE protocol=1 status=PASS ")
                harness.send("ROTATE max_writes=2048 target_rotation=0")
                _, gc_result = harness.wait_for_gc_result()
                target_rotations["gc_erase"] = parse_int(gc_result, "rotation")
            for window in selected_windows:
                found: dict[str, object] | None = None
                for delay_us in delays:
                    harness.send("RESET_PARTITION")
                    reset = harness.wait_for_prefix("ISSUE90 TEST_RESET protocol=1 ")
                    if parse_fields(reset[-1]).get("status") != "PASS":
                        continue
                    harness.send("PREFILL seed=0")
                    harness.wait_for_prefix("ISSUE90 PREFILL_DONE protocol=1 status=PASS ")
                    before = harness.page_evidence()
                    token = f"issue90-cal-{window}-{delay_us}"
                    run_hook(repo, hook, f"ARM token={token}", hook_log)
                    harness.send(f"CUT_ARM token={token}")
                    harness.wait_for_prefix("ISSUE90 CUT_ARMED protocol=1 ")
                    max_writes = 2048 if window == "gc_erase" else 1
                    target_rotation = target_rotations[window]
                    harness.send(
                        f"ROTATE max_writes={max_writes} "
                        f"target_rotation={target_rotation}"
                    )
                    begin_lines = harness.wait_for_prefix("ISSUE90 ROTATE_BEGIN protocol=1 ")
                    begin = harness.rotate_begin(begin_lines)
                    time.sleep(delay_us / 1_000_000.0)
                    run_hook(repo, hook, "TRIP", hook_log)
                    harness.require_uart_loss(args.uart_loss_timeout)
                    run_hook(repo, hook, "RESTORE", hook_log)
                    harness.ready(source_sha)
                    after = harness.page_evidence()
                    observed = classify_transition(before, after)
                    harness.readback()
                    if observed == window and window not in accepted:
                        found = {
                            "window": window, "delay_us": delay_us,
                            "rotation": int(begin["rotation"]),
                            "target_rotation": target_rotation,
                            "max_writes": max_writes,
                            "observed_window": observed,
                            "before_pages": before, "after_pages": after,
                            "accepted_at": utc_now(),
                        }
                        accepted.add(window)
                        break
                if found is None:
                    raise RuntimeError(f"no reproducible calibration for {window}")
                calibration["windows"][window] = found
            calibration_file.parent.mkdir(parents=True, exist_ok=True)
            calibration_file.write_text(
                json.dumps(calibration, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            manifest["calibration"] = str(calibration_file.relative_to(repo))
            manifest["status"] = "CALIBRATION_PASS"
            save_manifest()
            print(f"PASS calibration artifact={calibration_file.relative_to(repo)}")
            return 0

        if not calibration_file.is_file():
            raise RuntimeError(f"calibration artifact is required: {calibration_file}")
        calibration = json.loads(calibration_file.read_text(encoding="utf-8"))
        if (calibration.get("source_git_sha") != source_sha or
                calibration.get("plan_sha") != PLAN_SHA or
                calibration.get("idf_sha") != EXPECTED_IDF_SHA):
            raise RuntimeError("calibration provenance does not match this run")
        manifest["calibration"] = str(calibration_file.relative_to(repo))
        for window in selected_windows:
            parameters = calibration.get("windows", {}).get(window)
            if not isinstance(parameters, dict):
                raise RuntimeError(f"missing calibration window {window}")
            for repetition in range(args.repetitions):
                harness.send("RESET_PARTITION")
                reset = harness.wait_for_prefix("ISSUE90 TEST_RESET protocol=1 ")
                if parse_fields(reset[-1]).get("status") != "PASS":
                    raise RuntimeError("test reset did not pass")
                harness.send("PREFILL seed=0")
                harness.wait_for_prefix("ISSUE90 PREFILL_DONE protocol=1 status=PASS ")
                before = harness.page_evidence()
                before_readback = harness.readback()
                token = f"issue90-{window}-{repetition}"
                run_hook(repo, hook, f"ARM token={token}", hook_log)
                harness.send(f"CUT_ARM token={token}")
                harness.wait_for_prefix("ISSUE90 CUT_ARMED protocol=1 ")
                max_writes = int(parameters["max_writes"])
                delay_us = int(parameters["delay_us"])
                target_rotation = int(parameters["target_rotation"])
                harness.send(
                    f"ROTATE max_writes={max_writes} "
                    f"target_rotation={target_rotation}"
                )
                begin_lines = harness.wait_for_prefix("ISSUE90 ROTATE_BEGIN protocol=1 ")
                begin = harness.rotate_begin(begin_lines)
                time.sleep(delay_us / 1_000_000.0)
                run_hook(repo, hook, "TRIP", hook_log)
                harness.require_uart_loss(args.uart_loss_timeout)
                run_hook(repo, hook, "RESTORE", hook_log)
                harness.ready(source_sha)
                after = harness.page_evidence()
                after_readback = harness.readback()
                replacement = (
                    int(begin["new_length"]), require_sha(begin["new_sha256"], "new_sha256")
                )
                require_old_or_new(before_readback, after_readback, begin["key"], replacement)
                observed = classify_transition(before, after)
                if observed != window:
                    raise RuntimeError(
                        f"calibrated window mismatch expected={window} observed={observed}"
                    )
                results.append({
                    "window": window, "repetition": repetition, "status": "PASS",
                    "token": token, "delay_us": delay_us, "rotation": begin["rotation"],
                    "key": begin["key"], "old_readback": before_readback,
                    "new_readback": after_readback, "observed_window": observed,
                })
                save_manifest()
        harness.send("STOP")
        harness.wait_for_prefix("ISSUE90 COMPLETE protocol=1 status=PASS")
    manifest["status"] = "PASS"
    save_manifest()
    print(f"PASS hardware matrix artifact={run_dir.relative_to(repo)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
