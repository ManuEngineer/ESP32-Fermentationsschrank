#!/usr/bin/env python3

import argparse
import json
import subprocess
import sys


EXPECTED_PLATFORM = "espressif32@7.0.1"
EXPECTED_BOARD = "esp32dev"
EXPECTED_FLASH_BYTES = 4 * 1024 * 1024
EXPECTED_INTERNAL_RAM_BYTES = 320 * 1024
ESP32_ENVIRONMENTS = ("env:esp32_bringup", "env:esp32_release")


def run_json(pio: str, *arguments: str):
    result = subprocess.run(
        [pio, *arguments],
        check=True,
        capture_output=True,
        text=True,
    )
    return json.loads(result.stdout)


def project_sections(pio: str) -> dict[str, dict[str, object]]:
    raw_sections = run_json(pio, "project", "config", "--json-output")
    return {
        section_name: dict(options)
        for section_name, options in raw_sections
    }


def check_project_config(sections: dict[str, dict[str, object]]) -> None:
    for environment in ESP32_ENVIRONMENTS:
        options = sections.get(environment)
        if options is None:
            raise ValueError(f"Missing PlatformIO environment: {environment}")

        expected_options = {
            "platform": EXPECTED_PLATFORM,
            "board": EXPECTED_BOARD,
            "board_build.flash_size": "4MB",
            "board_upload.flash_size": "4MB",
        }
        for option, expected in expected_options.items():
            actual = options.get(option)
            if actual != expected:
                raise ValueError(
                    f"{environment}: expected {option}={expected!r}, "
                    f"got {actual!r}"
                )

        build_flags = options.get("build_flags", [])
        if isinstance(build_flags, str):
            build_flags = build_flags.splitlines()
        if any("BOARD_HAS_PSRAM" in flag for flag in build_flags):
            raise ValueError(f"{environment}: PSRAM must not be enabled")


def check_board_metadata(pio: str) -> None:
    boards = run_json(pio, "boards", EXPECTED_BOARD, "--json-output")
    board = next(
        (candidate for candidate in boards if candidate.get("id") == EXPECTED_BOARD),
        None,
    )
    if board is None:
        raise ValueError(f"PlatformIO board not found: {EXPECTED_BOARD}")
    if board.get("platform") != "espressif32":
        raise ValueError(
            f"{EXPECTED_BOARD}: unexpected platform {board.get('platform')!r}"
        )
    if board.get("rom") != EXPECTED_FLASH_BYTES:
        raise ValueError(
            f"{EXPECTED_BOARD}: expected {EXPECTED_FLASH_BYTES} flash bytes, "
            f"got {board.get('rom')!r}"
        )
    if board.get("ram") != EXPECTED_INTERNAL_RAM_BYTES:
        raise ValueError(
            f"{EXPECTED_BOARD}: expected {EXPECTED_INTERNAL_RAM_BYTES} internal "
            f"RAM bytes without PSRAM, got {board.get('ram')!r}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate the resolved PlatformIO ESP32 configuration."
    )
    parser.add_argument("--pio", default="pio", help="PlatformIO executable")
    arguments = parser.parse_args()

    try:
        check_project_config(project_sections(arguments.pio))
        check_board_metadata(arguments.pio)
    except (FileNotFoundError, subprocess.CalledProcessError, json.JSONDecodeError,
            ValueError) as error:
        print(f"PlatformIO configuration check failed: {error}", file=sys.stderr)
        return 1

    print(
        "PlatformIO configuration valid: espressif32 7.0.1, "
        "esp32dev, 4 MB flash, no required PSRAM."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
