#!/usr/bin/env python3
"""Build the private Issue #90 Slice-7 bring-up harness.

The harness uses the current product composition and persistence services but
is compiled only for the bring-up profile. It uses the ``state_store_test``
label for the reused production flash range and never flashes a device.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

import check_build_profiles


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build" / "esp32_bringup_issue90"
SDKCONFIG = BUILD_DIR / "sdkconfig"
DEFAULTS = (
    "sdkconfig.defaults;sdkconfig.defaults.bringup;"
    "sdkconfig.defaults.issue90_slice7"
)


def run(command: list[str], *, check: bool = False) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=ROOT, text=True, check=check)


def require_idf() -> Path:
    value = os.environ.get("IDF_PATH")
    if not value:
        raise SystemExit("IDF_PATH must be activated with the pinned ESP-IDF export.sh")
    path = Path(value)
    if not path.is_dir():
        raise SystemExit(f"IDF_PATH is not a directory: {path}")
    violations = check_build_profiles.check_esp_idf_version(path)
    if violations:
        raise SystemExit("; ".join(violations))
    if shutil.which("idf.py") is None:
        raise SystemExit("idf.py not found on PATH")
    return path


def build() -> None:
    require_idf()
    command = [
        "idf.py",
        "-B",
        str(BUILD_DIR),
        f"-DSDKCONFIG={SDKCONFIG}",
        f"-DSDKCONFIG_DEFAULTS={DEFAULTS}",
        "-DAPP_ISSUE_90_SLICE7_HARNESS=1",
        "build",
    ]
    result = run(command)
    if result.returncode != 0:
        raise SystemExit(result.returncode)


def read_sdkconfig() -> dict[str, str]:
    values: dict[str, str] = {}
    for raw_line in SDKCONFIG.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line.startswith("CONFIG_") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key] = value
    return values


def verify_effective_configuration() -> None:
    values = read_sdkconfig()
    expected = {
        "CONFIG_APP_PROFILE_ESP32_BRINGUP": "y",
        "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME": '"partitions/issue_90_state_store_test.csv"',
    }
    for key, value in expected.items():
        if values.get(key) != value:
            raise SystemExit(
                f"harness sdkconfig mismatch: {key}={values.get(key)!r}, "
                f"expected {value!r}"
            )
    if values.get("CONFIG_APP_PROFILE_ESP32_RELEASE") == "y":
        raise SystemExit("harness sdkconfig selected the release profile")
    print("PASS: ESP32_BRINGUP_HARNESS=INCLUDED")
    print("PASS: harness effective partition=state_store_test")
    print("PASS: ESP32_RELEASE_HARNESS=EXCLUDED")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clean", action="store_true", help="remove only the dedicated harness build directory")
    parser.add_argument("--verify-only", action="store_true")
    arguments = parser.parse_args()

    if arguments.clean and BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    if not arguments.verify_only:
        build()
    if not SDKCONFIG.exists():
        raise SystemExit(f"missing generated sdkconfig: {SDKCONFIG}")
    verify_effective_configuration()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
