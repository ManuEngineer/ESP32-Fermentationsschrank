#!/usr/bin/env python3
"""Build the private Issue #30 sensor commissioning bring-up image.

The image is the bring-up profile with the UART commissioning harness
(``APP_ISSUE_30_SENSOR_COMMISSIONING``) that writes the persisted sensor record
through ``FermentationApplication::applySensorCommissioning``. The release
profile never contains the harness. The script builds only; it never flashes.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
from pathlib import Path

import check_build_profiles


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build" / "esp32_bringup_issue30"
SDKCONFIG = BUILD_DIR / "sdkconfig"
DEFAULTS = "sdkconfig.defaults;sdkconfig.defaults.bringup"


def require_idf() -> None:
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


def build(*, allow_dirty: bool) -> None:
    require_idf()
    if not allow_dirty:
        try:
            check_build_profiles.require_clean_source_tree(ROOT)
        except RuntimeError as error:
            raise SystemExit(str(error)) from error
        print("SOURCE_TREE_CLEAN=YES")
    command = [
        "idf.py",
        "-B",
        str(BUILD_DIR),
        f"-DSDKCONFIG={SDKCONFIG}",
        f"-DSDKCONFIG_DEFAULTS={DEFAULTS}",
        "-DAPP_ISSUE_30_SENSOR_COMMISSIONING=1",
        "build",
    ]
    result = subprocess.run(command, cwd=ROOT, text=True)
    if result.returncode != 0:
        raise SystemExit(result.returncode)


def verify_effective_configuration() -> None:
    values: dict[str, str] = {}
    for raw_line in SDKCONFIG.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    if values.get("CONFIG_APP_PROFILE_ESP32_BRINGUP") != "y":
        raise SystemExit("commissioning sdkconfig did not select the bring-up profile")
    if values.get("CONFIG_APP_PROFILE_ESP32_RELEASE") == "y":
        raise SystemExit("commissioning sdkconfig selected the release profile")
    print("PASS: ISSUE30_COMMISSIONING=INCLUDED_IN_BRINGUP_ONLY")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clean", action="store_true", help="remove only the dedicated build directory")
    parser.add_argument("--allow-dirty-source-tree", action="store_true", help="local development builds only; never a hardware image")
    arguments = parser.parse_args()
    if arguments.clean and BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    build(allow_dirty=arguments.allow_dirty_source_tree)
    verify_effective_configuration()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
