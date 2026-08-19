#!/usr/bin/env python3
"""Run the non-release Issue #90 UART verification harness on an ESP32."""

from __future__ import annotations

import argparse
import hashlib
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


def require_old_or_new(
    before: dict[str, str], after: dict[str, str], new_hashes: dict[str, str]
) -> None:
    for key in EXPECTED_KEYS:
        if after[key] not in (before[key], new_hashes[key]):
            raise RuntimeError(
                f"power-cut oracle failed for {key}: {after[key]} not old/new"
            )


def value(size: int, seed: int) -> bytes:
    return bytes((seed + (index % 251)) & 0xFF for index in range(size))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--build-dir", type=Path, default=Path("build/issue_90_hw"))
    parser.add_argument("--rotations", type=int, default=2048)
    parser.add_argument("--power-hook", type=Path,
                        default=Path("scripts/issue_90_power_cut_hook.py"))
    args = parser.parse_args()
    repo = root()
    build_dir = args.build_dir if args.build_dir.is_absolute() else repo / args.build_dir
    if args.build:
        build(repo, build_dir)
    try:
        import serial  # type: ignore
    except ImportError as error:
        raise SystemExit("pyserial is required for the owner-supplied UART runner") from error

    with serial.Serial(args.port, args.baud, timeout=1.0) as port:
        harness = Harness(port, args.timeout)
        harness.wait_for("READY partition=")
        harness.send("PREFILL seed=0")
        harness.wait_for("PREFILL_DONE")
        harness.readback()
        harness.send(f"ROTATE max_writes={args.rotations}")
        harness.wait_for("ROTATE_RESULT")
        harness.readback()

        # Each power-cut repetition is externally timed by the configured
        # controller. The firmware marker is the only synchronization point;
        # no guessed delay or machine-local controller path is used.
        for repetition in range(3):
            token = f"issue90-{repetition}"
            before_cut = harness.readback()
            subprocess.run([sys.executable, str(repo / args.power_hook), "ARM", token],
                           cwd=repo, check=True)
            harness.send(f"CUT_ARM token={token}")
            harness.wait_for("CUT_ARMED")
            harness.send("ROTATE max_writes=1")
            subprocess.run([sys.executable, str(repo / args.power_hook), "TRIP", token],
                           cwd=repo, check=True)
            subprocess.run([sys.executable, str(repo / args.power_hook), "RESTORE", token],
                           cwd=repo, check=True)
            harness.wait_for("READY partition=")
            after_cut = harness.readback()
            new_hashes = dict(before_cut)
            new_hashes["pc0"] = hashlib.sha256(value(32813, 3)).hexdigest()
            require_old_or_new(before_cut, after_cut, new_hashes)
        harness.send("STOP")
        harness.wait_for("PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
