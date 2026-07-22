#!/usr/bin/env python3
"""Baut die angegebenen PlatformIO-Profile und erzeugt einen Firmware- und
Ressourcen-Groessenbericht.

Der Bericht ist informativ. Verbindliche Byte-Budgets sind laut
`docs/OPEN_POINTS.md` weiterhin `TBD_IMPLEMENTATION_BUDGET` und werden hier
nicht erfunden; das Skript scheitert daher nur, wenn der zugrunde liegende
PlatformIO-Build selbst fehlschlaegt.
"""

import argparse
import os
import re
import subprocess
import sys
from typing import Optional

DEFAULT_ENVIRONMENTS = ("native", "esp32_bringup", "esp32_release")

SIZE_BLOCK_PATTERN = re.compile(
    r"Checking size \.pio/build/(?P<env>[\w-]+)/firmware\.elf\s*\n"
    r"(?:.*\n)*?RAM:\s+\[.*?\]\s+(?P<ram_percent>[\d.]+)% "
    r"\(used (?P<ram_used>\d+) bytes from (?P<ram_total>\d+) bytes\)\s*\n"
    r"Flash:\s+\[.*?\]\s+(?P<flash_percent>[\d.]+)% "
    r"\(used (?P<flash_used>\d+) bytes from (?P<flash_total>\d+) bytes\)",
)


def run_pio_build(environments: tuple[str, ...], pio: str) -> str:
    arguments = [pio, "run"]
    for environment in environments:
        arguments += ["-e", environment]

    result = subprocess.run(arguments, text=True, capture_output=True)
    print(result.stdout)
    print(result.stderr, file=sys.stderr)
    if result.returncode != 0:
        raise SystemExit(result.returncode)
    return result.stdout


def parse_esp32_size_reports(build_output: str) -> dict[str, dict[str, object]]:
    reports = {}
    for match in SIZE_BLOCK_PATTERN.finditer(build_output):
        reports[match["env"]] = {
            "ram_percent": float(match["ram_percent"]),
            "ram_used_bytes": int(match["ram_used"]),
            "ram_total_bytes": int(match["ram_total"]),
            "flash_percent": float(match["flash_percent"]),
            "flash_used_bytes": int(match["flash_used"]),
            "flash_total_bytes": int(match["flash_total"]),
        }
    return reports


def artifact_size(path: str) -> Optional[int]:
    return os.path.getsize(path) if os.path.isfile(path) else None


def build_report_lines(environments: tuple[str, ...],
                        esp32_reports: dict[str, dict[str, object]]) -> list[str]:
    lines = [
        "# Firmware- und Ressourcen-Groessenbericht",
        "",
        "Informativ. Verbindliche Byte-Budgets bleiben `TBD_IMPLEMENTATION_BUDGET` "
        "bis zu realen Hardware- und Belastungsmessungen (siehe `docs/OPEN_POINTS.md`).",
        "",
    ]

    for environment in environments:
        lines.append(f"## {environment}")
        lines.append("")

        if environment in esp32_reports:
            report = esp32_reports[environment]
            lines.append(
                f"- RAM: {report['ram_used_bytes']} / {report['ram_total_bytes']} Bytes "
                f"({report['ram_percent']}%)"
            )
            lines.append(
                f"- Flash: {report['flash_used_bytes']} / {report['flash_total_bytes']} Bytes "
                f"({report['flash_percent']}%)"
            )
            elf_size = artifact_size(f".pio/build/{environment}/firmware.elf")
            bin_size = artifact_size(f".pio/build/{environment}/firmware.bin")
            if elf_size is not None:
                lines.append(f"- firmware.elf: {elf_size} Bytes")
            if bin_size is not None:
                lines.append(f"- firmware.bin: {bin_size} Bytes")
        else:
            program_size = artifact_size(f".pio/build/{environment}/program")
            if program_size is not None:
                lines.append(
                    f"- Host-Testbinaer (`program`): {program_size} Bytes "
                    "(kein Flash-/RAM-Budget, da hardwareunabhaengig)"
                )
            else:
                lines.append("- kein Groessenartefakt gefunden")
        lines.append("")

    return lines


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pio", default="pio", help="PlatformIO executable")
    parser.add_argument(
        "--output", default="build-report.md",
        help="Zieldatei fuer den Markdown-Bericht",
    )
    parser.add_argument(
        "environments", nargs="*", default=DEFAULT_ENVIRONMENTS,
        help="Zu bauende PlatformIO-Umgebungen",
    )
    arguments = parser.parse_args()

    environments = tuple(arguments.environments)
    build_output = run_pio_build(environments, arguments.pio)
    esp32_reports = parse_esp32_size_reports(build_output)

    report_lines = build_report_lines(environments, esp32_reports)
    report_text = "\n".join(report_lines) + "\n"

    with open(arguments.output, "w", encoding="utf-8") as report_file:
        report_file.write(report_text)

    print(report_text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
