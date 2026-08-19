#!/usr/bin/env python3
"""Prove that the release profile contains no Issue-90 harness path."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


FORBIDDEN_TOKENS = (
    "issue_90_nvs_hardware_verification.cpp",
    "APP_ISSUE_90_NVS_HARDWARE_TEST",
    "APP_ISSUE_90_SOURCE_GIT_SHA",
    "APP_ISSUE_90_PLAN_SHA",
    "ISSUE90",
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release-build-dir", type=Path, required=True)
    parser.add_argument("--cmake", type=Path, default=Path("main/CMakeLists.txt"))
    args = parser.parse_args()
    build_dir = args.release_build_dir
    compile_commands = build_dir / "compile_commands.json"
    if not compile_commands.is_file():
        raise SystemExit(f"FAIL release isolation: missing {compile_commands}")
    entries = json.loads(compile_commands.read_text(encoding="utf-8"))
    command_text = "\n".join(
        json.dumps(entry, sort_keys=True) for entry in entries
        if isinstance(entry, dict)
    )
    violations = [token for token in FORBIDDEN_TOKENS if token in command_text]
    if violations:
        raise SystemExit(
            "FAIL release isolation: forbidden harness source/marker in "
            + ", ".join(violations)
        )
    ninja = build_dir / "build.ninja"
    if ninja.is_file():
        ninja_text = ninja.read_text(encoding="utf-8")
        if "issue_90_nvs_hardware_verification.cpp" in ninja_text:
            raise SystemExit(
                "FAIL release isolation: harness source is present in build.ninja"
            )
    cmake = args.cmake.read_text(encoding="utf-8")
    harness_block_start = cmake.find("if(APP_ISSUE_90_ENABLED)")
    harness_block_end = cmake.find("endif()", harness_block_start)
    if harness_block_start < 0 or harness_block_end < harness_block_start:
        raise SystemExit("FAIL release isolation: conditional harness dependency block missing")
    harness_block = cmake[harness_block_start:harness_block_end]
    for dependency in ("esp_partition", "mbedtls", "esp_driver_uart", "spi_flash"):
        if dependency not in harness_block:
            raise SystemExit(
                f"FAIL release isolation: {dependency} is not conditional harness-only"
            )
    production_register = cmake[cmake.find("idf_component_register("):harness_block_start]
    if any(dependency in production_register for dependency in
           ("esp_partition", "mbedtls", "esp_driver_uart", "spi_flash")):
        raise SystemExit("FAIL release isolation: harness dependency leaked into production register")
    print(
        "PASS release-isolation no-harness-source no-issue90-marker "
        "no-harness-compile-definitions"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
