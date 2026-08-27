#!/usr/bin/env python3
"""Check compile-time and ELF isolation of the Issue #90 Slice-7 harness."""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HARNESS_DEFINE = "APP_ISSUE_90_SLICE7_HARNESS"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def nm_output(path: Path) -> str:
    tool = shutil.which("xtensa-esp32-elf-nm") or shutil.which("nm")
    if tool is None:
        raise SystemExit("no nm tool found for ELF isolation check")
    result = subprocess.run(
        [tool, "-C", "--defined-only", str(path)],
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        raise SystemExit(f"nm failed for {path}: {result.stderr.strip()}")
    return result.stdout


def check_source_contract() -> None:
    main = read(ROOT / "main" / "CMakeLists.txt")
    app = read(ROOT / "lib" / "fermentation_app" / "CMakeLists.txt")
    if "if(APP_ISSUE_90_SLICE7_HARNESS)" not in main:
        raise SystemExit("main CMake does not gate the harness variable")
    if "CONFIG_APP_PROFILE_ESP32_BRINGUP" not in main:
        raise SystemExit("main CMake has no bring-up profile guard")
    if HARNESS_DEFINE not in main or HARNESS_DEFINE not in app:
        raise SystemExit("harness compile definition is not present in both scopes")
    for overlay in ("sdkconfig.defaults.bringup", "sdkconfig.defaults.release"):
        if HARNESS_DEFINE in read(ROOT / overlay):
            raise SystemExit(f"harness definition leaked into {overlay}")
    print("PASS: compile-time harness gate is bring-up-only")


def check_elf(path: Path, *, expect_harness: bool) -> None:
    output = nm_output(path)
    present = "issue_90_slice7::Harness" in output
    if present != expect_harness:
        expected = "present" if expect_harness else "absent"
        raise SystemExit(f"{path}: harness symbols are not {expected}")
    print(f"PASS: {path} harness symbols {'present' if present else 'absent'}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--harness-elf", type=Path)
    parser.add_argument("--release-elf", type=Path)
    args = parser.parse_args()
    check_source_contract()
    if args.harness_elf is not None:
        check_elf(args.harness_elf, expect_harness=True)
    if args.release_elf is not None:
        check_elf(args.release_elf, expect_harness=False)
    if args.harness_elf is None or args.release_elf is None:
        print("INFO: ELF checks require --harness-elf and --release-elf")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
