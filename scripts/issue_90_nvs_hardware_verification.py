#!/usr/bin/env python3
"""Run the non-release Issue #90 UART verification harness on an ESP32."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path


EXPECTED_KEYS = (
    "uc0", "uc1", "uc2", "uc3", "sc0", "sc1", "sc2", "sc3", "pc0",
    "pc1", "pc2", "pc3", "cm0", "cm1", "cm2", "cr0", "cr1", "cb0",
    "cb1", "rc0", "rc1", "rh0",
)
WINDOWS = ("blob_data", "blob_index", "old_removal", "gc_erase")


def root() -> Path:
    return Path(__file__).resolve().parents[1]


def build(root_path: Path, build_dir: Path) -> None:
    if not os.environ.get("IDF_PATH"):
        raise SystemExit("IDF_PATH must be activated with the pinned ESP-IDF export.sh")
    sdkconfig = build_dir / "sdkconfig"
    defaults = "sdkconfig.defaults;sdkconfig.defaults.bringup"
    command = [
        "idf.py", "-B", str(build_dir), f"-DSDKCONFIG={sdkconfig}",
        f"-DSDKCONFIG_DEFAULTS={defaults}",
        "-DAPP_ISSUE_90_NVS_HARDWARE_TEST=1", "build",
    ]
    subprocess.run(command, cwd=root_path, check=True)


class Harness:
    def __init__(self, serial_port, timeout: float) -> None:
        self.serial = serial_port
        self.timeout = timeout

    def send(self, command: str) -> None:
        self.serial.write((command + "\n").encode("ascii"))

    def wait_for(self, marker: str) -> list[str]:
        deadline = time.monotonic() + self.timeout
        lines: list[str] = []
        while time.monotonic() < deadline:
            raw = self.serial.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            lines.append(line)
            if marker in line:
                return lines
            if "ISSUE90" in line and "FAIL" in line:
                raise RuntimeError(line)
        raise TimeoutError(f"timeout waiting for {marker}; last={lines[-5:]}")

    def readback(self) -> dict[str, str]:
        self.send("READBACK_ALL")
        lines = self.wait_for("READBACK_RESULT")
        values: dict[str, str] = {}
        for line in lines:
            if "READBACK key=" not in line or "sha256=" not in line:
                continue
            fields = dict(
                field.split("=", 1)
                for field in line.split()
                if "=" in field
            )
            if fields.get("status") == "0" and fields.get("sha256", "-") != "-":
                values[fields["key"]] = fields["sha256"]
        if tuple(values) != EXPECTED_KEYS:
            missing = [key for key in EXPECTED_KEYS if key not in values]
            raise RuntimeError(f"incomplete READBACK_ALL; missing={missing}")
        return values

    def require_uart_loss(self, seconds: float) -> None:
        deadline = time.monotonic() + seconds
        observed = []
        while time.monotonic() < deadline:
            raw = self.serial.readline()
            if raw:
                line = raw.decode("utf-8", errors="replace").strip()
                observed.append(line)
                if "ROTATE_RESULT" in line:
                    raise RuntimeError(
                        "UART remained live through ROTATE_RESULT after TRIP"
                    )
        if observed:
            raise RuntimeError(f"UART did not become silent after TRIP: {observed[-5:]}")


def parse_fields(line: str) -> dict[str, str]:
    return dict(
        field.split("=", 1)
        for field in line.split()
        if "=" in field
    )


def parse_rotate_begin(lines: list[str]) -> tuple[str, str]:
    for line in reversed(lines):
        if "ROTATE_BEGIN" not in line:
            continue
        fields = parse_fields(line)
        if fields.get("key") in EXPECTED_KEYS and fields.get("new_sha256"):
            return fields["key"], fields["new_sha256"]
    raise RuntimeError(f"ROTATE_BEGIN marker has no key/hash: {lines[-5:]}")


def require_old_or_new(
    before: dict[str, str], after: dict[str, str], new_hashes: dict[str, str]
) -> None:
    for key in EXPECTED_KEYS:
        if after[key] not in (before[key], new_hashes[key]):
            raise RuntimeError(
                f"power-cut oracle failed for {key}: {after[key]} not old/new"
            )


def run_hook(repo: Path, hook: Path, command: str) -> None:
    result = subprocess.run(
        [sys.executable, str(hook)],
        cwd=repo,
        input=command + "\n",
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"power hook failed for {command!r}: {result.stderr.strip()}"
        )
    if result.stdout.strip() != {
        "ARM": "ARMED", "TRIP": "TRIPPED", "RESTORE": "RESTORED"
    }[command.split()[0]]:
        raise RuntimeError(
            f"power hook protocol mismatch for {command!r}: {result.stdout!r}"
        )


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
    parser.add_argument("--profile", choices=("esp32_bringup",),
                        default="esp32_bringup")
    parser.add_argument("--scenario", choices=("prefilled_gc",),
                        default="prefilled_gc")
    parser.add_argument("--artifact-dir", type=Path,
                        default=Path("build/issue_90_hardware_verification"))
    args = parser.parse_args()
    if args.repetitions < 10:
        raise SystemExit("--repetitions must be at least 10 for the hardware matrix")
    selected_windows = tuple(
        window for window in args.windows.split(",") if window
    )
    if not selected_windows or any(window not in WINDOWS for window in selected_windows):
        raise SystemExit(f"--windows must be selected from {','.join(WINDOWS)}")
    repo = root()
    if args.profile != "esp32_bringup" or args.scenario != "prefilled_gc":
        raise SystemExit("only the approved bring-up prefilled_gc scenario is supported")
    build_dir = args.build_dir if args.build_dir.is_absolute() else repo / args.build_dir
    hook = (args.power_cut_hook if args.power_cut_hook.is_absolute()
            else repo / args.power_cut_hook)
    artifact_dir = (args.artifact_dir if args.artifact_dir.is_absolute()
                    else repo / args.artifact_dir)
    artifact_dir.mkdir(parents=True, exist_ok=True)
    if args.build:
        build(repo, build_dir)
    try:
        import serial  # type: ignore
    except ImportError as error:
        raise SystemExit("pyserial is required for the owner-supplied UART runner") from error

    with serial.Serial(args.port, args.baud, timeout=0.1) as port:
        harness = Harness(port, args.timeout)
        harness.wait_for("READY partition=")
        harness.send("PREFILL seed=0")
        harness.wait_for("PREFILL_DONE")
        harness.readback()
        for control in range(3):
            harness.send("REBOOT")
            harness.wait_for("READY partition=")
            harness.readback()

        for window in selected_windows:
            for repetition in range(args.repetitions):
                harness.send("PREFILL seed=0")
                harness.wait_for("PREFILL_DONE")
                before_cut = harness.readback()
                token = f"issue90-{window}-{repetition}"
                run_hook(repo, hook, f"ARM token={token}")
                harness.send(f"CUT_ARM token={token}")
                harness.wait_for("CUT_ARMED")
                max_writes = 2048 if window == "gc_erase" else 1
                harness.send(f"ROTATE max_writes={max_writes}")
                rotate_lines = harness.wait_for("ROTATE_BEGIN")
                key, new_hash = parse_rotate_begin(rotate_lines)
                run_hook(repo, hook, "TRIP")
                harness.require_uart_loss(args.uart_loss_timeout)
                run_hook(repo, hook, "RESTORE")
                harness.wait_for("READY partition=")
                after_cut = harness.readback()
                new_hashes = dict(before_cut)
                new_hashes[key] = new_hash
                require_old_or_new(before_cut, after_cut, new_hashes)
                print(
                    f"PASS power-cut window={window} repetition={repetition} "
                    f"key={key} old_or_new=PASS"
                )

        harness.send("STOP")
        harness.wait_for("PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
