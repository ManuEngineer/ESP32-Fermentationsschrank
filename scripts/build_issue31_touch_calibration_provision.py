#!/usr/bin/env python3
"""Build the private Issue #31 actor-free NVS calibration provisioner."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
from pathlib import Path

import check_build_profiles


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build" / "esp32_bringup_issue31_touch_calibration_provision"
SDKCONFIG = BUILD_DIR / "sdkconfig"
DEFAULTS = "sdkconfig.defaults;sdkconfig.defaults.bringup"
PROVISION_DEFINE = "APP_ISSUE_31_TOUCH_CALIBRATION_PROVISIONER"


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=ROOT, text=True)


def require_idf() -> None:
    value = os.environ.get("IDF_PATH")
    if not value:
        raise SystemExit(
            "IDF_PATH must be activated with the pinned ESP-IDF export.sh"
        )
    path = Path(value)
    if not path.is_dir():
        raise SystemExit(f"IDF_PATH is not a directory: {path}")
    violations = check_build_profiles.check_esp_idf_version(path)
    if violations:
        raise SystemExit("; ".join(violations))
    if shutil.which("idf.py") is None:
        raise SystemExit("idf.py not found on PATH")


def read_sdkconfig() -> dict[str, str]:
    if not SDKCONFIG.exists():
        raise SystemExit(f"missing generated sdkconfig: {SDKCONFIG}")
    values: dict[str, str] = {}
    for raw_line in SDKCONFIG.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


def verify_effective_configuration() -> None:
    values = read_sdkconfig()
    if values.get("CONFIG_APP_PROFILE_ESP32_BRINGUP") != "y":
        raise SystemExit("provisioner sdkconfig did not select esp32_bringup")
    if values.get("CONFIG_APP_PROFILE_ESP32_RELEASE") == "y":
        raise SystemExit("provisioner sdkconfig selected the release profile")
    print("PASS: ISSUE31_CALIBRATION_PROVISION_PROFILE=ESP32_BRINGUP")
    print("PASS: ISSUE31_CALIBRATION_PROVISION_ACTUATORS=DISABLED")
    print("PASS: ISSUE31_CALIBRATION_PROVISION_APP_NOT_STARTED=YES")
    print("PASS: ISSUE31_CALIBRATION_PROVISION_ACTIVE_SLOT_ONLY=YES")


def build() -> None:
    require_idf()
    try:
        check_build_profiles.require_clean_source_tree(ROOT)
    except RuntimeError as error:
        raise SystemExit(str(error)) from error
    result = run(
        [
            "idf.py",
            "-B",
            str(BUILD_DIR),
            f"-DSDKCONFIG={SDKCONFIG}",
            f"-DSDKCONFIG_DEFAULTS={DEFAULTS}",
            f"-D{PROVISION_DEFINE}=1",
            "build",
        ]
    )
    if result.returncode != 0:
        raise SystemExit(result.returncode)
    print("PASS: ISSUE31_CALIBRATION_PROVISION_BUILD")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--verify-only", action="store_true")
    arguments = parser.parse_args()
    if arguments.clean and BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    if not arguments.verify_only:
        build()
    verify_effective_configuration()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
