#!/usr/bin/env python3
"""Interactive actor-free Issue #90 Slice-7 product runner.

The runner speaks only the private bring-up protocol.  It never toggles RTS or
EN and never controls board power.  A later physical attempt pauses at the
machine-readable owner window so the Owner can remove and restore ESP32 power.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TEST_PARTITION = "state_store_test"
PROTOCOL_VERSION = 1
STATE_STORE_OFFSET = 0x300000
STATE_STORE_SIZE = 0x100000


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


def run_esptool(port: str, baud: int, command: list[str]) -> None:
    tool = shutil.which("esptool")
    if tool is None:
        raise RunnerError("pinned ESP-IDF esptool is required for partition safety")
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
            "no-reset",
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


def require_partition_image(path: Path) -> str:
    if not path.is_file():
        raise RunnerError(f"missing state-store backup: {path}")
    if path.stat().st_size != STATE_STORE_SIZE:
        raise RunnerError(
            f"state-store backup has {path.stat().st_size} bytes; "
            f"expected {STATE_STORE_SIZE}"
        )
    return sha256_file(path)


def capture_state_store_backup(port: str, baud: int, target: Path) -> str:
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="issue90-state-store-", dir=target.parent
    ) as directory:
        first = Path(directory) / "capture.bin"
        second = Path(directory) / "capture-readback.bin"
        command = ["read-flash", hex(STATE_STORE_OFFSET), hex(STATE_STORE_SIZE)]
        run_esptool(port, baud, [*command, str(first)])
        first_sha = require_partition_image(first)
        run_esptool(port, baud, [*command, str(second)])
        second_sha = require_partition_image(second)
        if first_sha != second_sha:
            raise RunnerError("state-store backup read-back hash mismatch")
        first.replace(target)
    return require_partition_image(target)


def restore_state_store_backup(port: str, baud: int, backup: Path) -> str:
    backup_sha = require_partition_image(backup)
    run_esptool(
        port,
        baud,
        ["write-flash", hex(STATE_STORE_OFFSET), str(backup)],
    )
    with tempfile.TemporaryDirectory(
        prefix="issue90-state-store-verify-", dir=backup.parent
    ) as directory:
        verify = Path(directory) / "restore-readback.bin"
        run_esptool(
            port,
            baud,
            [
                "read-flash",
                hex(STATE_STORE_OFFSET),
                hex(STATE_STORE_SIZE),
                str(verify),
            ],
        )
        if require_partition_image(verify) != backup_sha:
            raise RunnerError("state-store restore read-back hash mismatch")
    return backup_sha


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

    def close(self) -> None:
        self._serial.close()

    def __enter__(self) -> "ProductSerial":
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def send(self, command: str) -> None:
        self._serial.write((command + "\n").encode("ascii"))
        self._serial.flush()

    def read_until(self, marker: str) -> tuple[list[str], dict[str, str]]:
        deadline = time.monotonic() + self.timeout
        lines: list[str] = []
        while time.monotonic() < deadline:
            raw = self._serial.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            lines.append(line)
            self._observe_faults(line)
            if marker in line:
                return lines, marker_fields(line, marker)
        raise RunnerError(f"timeout waiting for {marker}; last={lines[-8:]}")

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
            for name in ("PANIC", "WATCHDOG", "BROWNOUT", "UNEXPECTED_RESET")
        }

    def ready(self) -> dict[str, str]:
        _, fields = self.read_until("ISSUE90_READY")
        if fields.get("partition") != TEST_PARTITION:
            raise RunnerError(f"unexpected test partition: {fields}")
        if fields.get("real_actuators_enabled") != "NO":
            raise RunnerError(f"actor-free contract missing: {fields}")
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

    def prepare_power_cut(self, scenario: str) -> dict[str, str]:
        command = {
            "config": "ARM_CONFIG_WRITE_LOAD",
            "run": "ARM_RUN_WRITE_LOAD",
        }[scenario]
        self.send(command)
        _, armed = self.read_until("ISSUE90_LOAD_ARMED")
        if armed.get("result") != "PASS":
            raise RunnerError(f"load arm failed: {armed}")
        _, window = self.read_until("ISSUE90_OWNER_POWER_CUT_WINDOW_ACTIVE")
        if window.get("mode") != scenario.upper():
            raise RunnerError(f"wrong power-cut mode: {window}")
        return window


def write_manifest(
    output: Path,
    *,
    source_sha: str,
    firmware: Path | None,
    scenario: str,
    attempts: int,
    backup_sha256: str,
) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    manifest = {
        "protocol_version": PROTOCOL_VERSION,
        "source_sha": source_sha,
        "firmware_sha256": None if firmware is None else sha256_file(firmware),
        "plan_sha": "baf0b2ae04cd42afa75dfa00e21d900116b38bc8",
        "partition": TEST_PARTITION,
        "scenario": scenario,
        "attempts": attempts,
        "actor_free": True,
        "real_actuators_enabled": False,
        "power_cut_type": "PHYSICAL_POWER_REMOVAL",
        "rts_en_software_reset_substitute": False,
        "state_store_backup_sha256": backup_sha256,
    }
    output.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def run_physical_campaign(args: argparse.Namespace) -> int:
    if args.attempts != 3:
        raise RunnerError("Slice-7 contract requires exactly 3 attempts per scenario")
    if args.profile != "esp32_bringup":
        raise RunnerError("the Slice-7 harness is bring-up-only")
    if not args.backup_file:
        raise RunnerError(
            "--backup-file is required; the test layout reuses the state-store "
            "flash range and must be restored"
        )
    firmware = Path(args.firmware) if args.firmware else None
    backup = Path(args.backup_file)
    with ProductSerial(args.port, args.baud, args.timeout) as device:
        ready = device.ready()
        if (
            ready.get("backup_required") != "YES"
            or ready.get("partition_offset") != "0x300000"
            or ready.get("partition_size") != "0x100000"
        ):
            raise RunnerError(f"firmware did not require a partition backup: {ready}")
        device.command("BACKUP_OR_CONFIRM_TEST_PARTITION")
        device.status()

    backup_sha = capture_state_store_backup(args.port, args.baud, backup)
    print(f"STATE_STORE_BACKUP=PASS sha256={backup_sha}", flush=True)
    write_manifest(
        Path(args.artifact_dir) / "issue90_slice7_manifest.json",
        source_sha=args.source_sha or git_sha(),
        firmware=firmware,
        scenario=args.scenario,
        attempts=args.attempts,
        backup_sha256=backup_sha,
    )

    try:
        with ProductSerial(args.port, args.baud, args.timeout) as device:
            device.ready()
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
                device.ready()
                status = device.status()
                faults = device.fault_status()
                recovery_pass = (
                    status.get("configuration_runtime_available") == "YES"
                    and status.get("published_process_state") is not None
                    and all(value == "NONE_OBSERVED" for value in faults.values())
                )
                result = "PASS" if recovery_pass else "FAIL"
                print(
                    f"SCENARIO={args.scenario} ATTEMPT={attempt} "
                    f"POST_REBOOT_NVS_STATUS={status.get('configuration_recovery_status', 'UNKNOWN')} "
                    f"RECORD_VALIDATION={status.get('configuration_runtime_available', 'UNKNOWN')} "
                    f"CONFIGURATION_RECOVERY_RESULT={status.get('configuration_recovery_status', 'UNKNOWN')} "
                    f"RUN_RECOVERY_RESULT={status.get('run_load_disposition', 'UNKNOWN')} "
                    f"LOGICAL_GATE_RESULT={'PASS' if recovery_pass else 'FAIL'} "
                    f"ACTUATOR_RELEASE=false PANIC={faults['PANIC']} "
                    f"WATCHDOG={faults['WATCHDOG']} BROWNOUT={faults['BROWNOUT']} "
                    f"UNEXPECTED_RESET={faults['UNEXPECTED_RESET']} RESULT={result}"
                )
                if not recovery_pass:
                    raise RunnerError(f"product recovery gate failed: {status}")
                if args.scenario == "run":
                    cleanup = device.command("RUN_CONTROL_DISCARD_PENDING")
                    if cleanup.get("result") != "PASS":
                        raise RunnerError(f"run cleanup failed: {cleanup}")
    finally:
        restored_sha = restore_state_store_backup(args.port, args.baud, backup)
        print(f"STATE_STORE_RESTORE=PASS sha256={restored_sha}", flush=True)
    return 0


def selftest() -> int:
    status = marker_fields(
        "I (10) issue90: ISSUE90_STATUS application_started=YES "
        "application_lifecycle=Ready configuration_recovery_status=RuntimeReady "
        "configuration_runtime_available=YES run_persistence_load_status=NoPersistedRun "
        "run_load_disposition=Standby published_process_state=Standby "
        "actuator_release=false",
        "ISSUE90_STATUS",
    )
    require_actor_free(status)
    if status["configuration_recovery_status"] != "RuntimeReady":
        raise RunnerError("status parser failed")
    ready = marker_fields(
        "I issue90: ISSUE90_READY partition=state_store_test actor_free=YES "
        "real_actuators_enabled=NO backup_required=YES "
        "partition_offset=0x300000 partition_size=0x100000 "
        "load_stop=STOP_OR_PHYSICAL_POWER_CUT",
        "ISSUE90_READY",
    )
    if ready.get("partition") != TEST_PARTITION:
        raise RunnerError("ready parser failed")
    if ready.get("backup_required") != "YES":
        raise RunnerError("backup safety parser failed")
    if ready.get("partition_offset") != "0x300000":
        raise RunnerError("partition offset parser failed")
    print("PASS: Issue #90 Slice-7 UART field parser")
    print("PASS: actor-free status oracle")
    print("PASS: dedicated test-partition guard")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--profile", choices=("esp32_bringup",), default="esp32_bringup")
    parser.add_argument("--scenario", choices=("config", "run"), default="config")
    parser.add_argument("--attempts", type=int, default=3)
    parser.add_argument("--artifact-dir", default="build/issue90_slice7_hardware")
    parser.add_argument("--backup-file")
    parser.add_argument("--firmware")
    parser.add_argument("--source-sha")
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    if not args.port:
        parser.error("--port is required unless --selftest is used")
    try:
        return run_physical_campaign(args)
    except RunnerError as error:
        print(f"BLOCKED/FAILED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
