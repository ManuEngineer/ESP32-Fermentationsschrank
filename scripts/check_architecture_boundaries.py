#!/usr/bin/env python3
"""Prueft die in ADR-013 festgelegten Modul- und Abhaengigkeitsgrenzen."""

from __future__ import annotations

import argparse
import tempfile
from pathlib import Path

PASS = "PASS"
FAILED = "FAILED"

SCANNED_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".ini", ".json"}
PLATFORM_FORBIDDEN_TERMS = (
    "fermentation",
    "joghurt",
    "yogurt",
    "kefir",
    "kombucha",
)
PLATFORM_FORBIDDEN_ROLES = (
    "InsideFan",
    "OutsideFan",
    "insideFan",
    "outsideFan",
)


def text_files(directory: Path):
    if not directory.exists():
        return
    for path in directory.rglob("*"):
        if path.is_file() and path.suffix.lower() in SCANNED_SUFFIXES:
            yield path


def add_reference_violations(
    violations: list[str],
    directory: Path,
    forbidden: tuple[str, ...],
    description: str,
) -> None:
    for path in text_files(directory):
        try:
            lines = path.read_text(encoding="utf-8").splitlines()
        except UnicodeDecodeError:
            continue
        for line_number, line in enumerate(lines, start=1):
            for token in forbidden:
                if token in line:
                    violations.append(
                        f"{path}:{line_number}: {description}: {token!r}"
                    )


def check(root: Path) -> list[str]:
    violations: list[str] = []
    platform = root / "lib" / "device_platform"
    test_support = root / "lib" / "device_platform_test_support"
    fermentation_app = root / "lib" / "fermentation_app"
    main_cpp = root / "src" / "main.cpp"

    add_reference_violations(
        violations,
        platform,
        ("device_platform_test_support", "fermentation_app"),
        "unerlaubte Rueckwaertsabhaengigkeit in device_platform",
    )
    add_reference_violations(
        violations,
        fermentation_app,
        ("device_platform_test_support",),
        "Produktionsanwendung darf Test-Support nicht verwenden",
    )
    add_reference_violations(
        violations,
        test_support,
        ("fermentation_app", "Arduino.h"),
        "Test-Support darf Anwendung oder reale Arduino-Hardware nicht verwenden",
    )

    if main_cpp.exists():
        add_reference_violations(
            violations,
            main_cpp.parent,
            ("device_platform_test_support",),
            "Composition Root darf Test-Support nicht verwenden",
        )

    src_dir = platform / "src"
    if src_dir.exists():
        for path in src_dir.rglob("*"):
            if not path.is_file():
                continue
            lowered = path.name.lower()
            if lowered.startswith("mock_") or "simulation_model" in lowered:
                violations.append(
                    f"{path}: reine Mock-/Simulationsdatei gehoert nach "
                    "lib/device_platform_test_support/"
                )

    add_reference_violations(
        violations,
        src_dir,
        PLATFORM_FORBIDDEN_TERMS,
        "Fermentationsbegriff in anwendungsneutraler Plattform",
    )
    add_reference_violations(
        violations,
        src_dir,
        PLATFORM_FORBIDDEN_ROLES,
        "geraetespezifische Aktorrolle in allgemeiner Plattform-API",
    )

    return violations


def create_clean_fixture(root: Path) -> None:
    files = {
        "lib/device_platform/src/time_source.hpp": (
            "#pragma once\nnamespace device_platform { class ITimeSource {}; }\n"
        ),
        "lib/device_platform_test_support/src/mock_time_source.hpp": (
            '#pragma once\n#include "time_source.hpp"\n'
        ),
        "lib/fermentation_app/src/application.cpp": '#include "time_source.hpp"\n',
        "src/main.cpp": (
            '#include "device_platform.hpp"\n'
            '#include "fermentation_application.hpp"\n'
        ),
    }
    for relative_path, content in files.items():
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")


def selftest() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        create_clean_fixture(root)
        if check(root):
            print(f"{FAILED}: saubere Architektur-Fixture wurde abgelehnt")
            return 1

        bad_file = root / "lib" / "device_platform" / "src" / "bad.hpp"
        bad_file.write_text(
            '#include "device_platform_test_support/mock_time_source.hpp"\n',
            encoding="utf-8",
        )
        if not check(root):
            print(
                f"{FAILED}: absichtliche Rueckwaertsabhaengigkeit wurde nicht erkannt"
            )
            return 1

    print(f"{PASS}: Architekturpruefung erkennt absichtliche Grenzverletzung")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="Repository-Wurzel (Standard: Elternverzeichnis von scripts/)",
    )
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()

    if args.selftest:
        return selftest()

    violations = check(args.root.resolve())
    if violations:
        print(f"{FAILED}: Architekturgrenzen verletzt")
        for violation in violations:
            print(f"- {violation}")
        return 1

    print(f"{PASS}: Architekturgrenzen gemaess ADR-013 eingehalten")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
