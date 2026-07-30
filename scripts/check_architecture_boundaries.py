#!/usr/bin/env python3
"""Prueft die in ADR-013 festgelegten Modul- und Abhaengigkeitsgrenzen."""

from __future__ import annotations

import argparse
import re
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

# Issue #72: portable Quellwurzeln, die keinen ESP-IDF-/RTOS-Zugriff und
# keinen Zugriff auf die noch nicht angelegte Adaptergrenze
# device_platform_esp_idf enthalten duerfen. Bewusst eng gehalten (nicht
# main/ oder eine kuenftige device_platform_esp_idf/), siehe
# docs/tasks/issue-72-implementation-plan.md, Abschnitt 9.
IDF_LEAK_PORTABLE_ROOTS = (
    "lib/device_platform/src",
    "lib/fermentation_app/src",
)
IDF_LEAK_FORBIDDEN_INCLUDE_PREFIXES = (
    "esp_",
    "driver/",
    "freertos/",
    "lwip/",
    "hal/",
    "soc/",
    "nvs",
    "sdkconfig.h",
)
IDF_LEAK_FORBIDDEN_MACROS = ("ESP_PLATFORM", "ARDUINO")
INCLUDE_PATTERN = re.compile(r'#include\s*[<"]([^">]+)[">]')
PREPROCESSOR_CONDITION_PATTERN = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef|elif)\b(.*)$")
CONFIG_TOKEN_PATTERN = re.compile(r"\bCONFIG_[A-Za-z0-9_]+\b")

# Issue #72: erlaubte idf_component_register()-REQUIRES/PRIV_REQUIRES-Namen
# je Komponente. Jede andere direkte IDF-Komponentenabhaengigkeit dieser
# beiden Dateien ist eine unerlaubte Vorwegnahme von device_platform_esp_idf.
COMPONENT_REQUIRES_ALLOWLIST = {
    "lib/device_platform/CMakeLists.txt": frozenset(),
    "lib/fermentation_app/CMakeLists.txt": frozenset({"device_platform"}),
}
REQUIRES_PATTERN = re.compile(r"(?<!PRIV_)\bREQUIRES\b\s+([A-Za-z0-9_\s]*)")
PRIV_REQUIRES_PATTERN = re.compile(r"\bPRIV_REQUIRES\b\s+([A-Za-z0-9_\s]*)")


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


def add_idf_leak_violations(violations: list[str], root: Path) -> None:
    for relative_root in IDF_LEAK_PORTABLE_ROOTS:
        for path in text_files(root / relative_root):
            try:
                lines = path.read_text(encoding="utf-8").splitlines()
            except UnicodeDecodeError:
                continue
            for line_number, line in enumerate(lines, start=1):
                include_match = INCLUDE_PATTERN.search(line)
                if include_match:
                    header = include_match.group(1)
                    if header.startswith(IDF_LEAK_FORBIDDEN_INCLUDE_PREFIXES):
                        violations.append(
                            f"{path}:{line_number}: verbotener IDF-/RTOS-Include "
                            f"in portabler Wurzel: {header!r}"
                        )
                condition_match = PREPROCESSOR_CONDITION_PATTERN.match(line)
                if not condition_match:
                    continue
                condition_body = condition_match.group(1)
                for macro in IDF_LEAK_FORBIDDEN_MACROS:
                    if macro in condition_body:
                        violations.append(
                            f"{path}:{line_number}: verbotene Praeprozessorverwendung "
                            f"von {macro!r} in portabler Wurzel"
                        )
                for token in CONFIG_TOKEN_PATTERN.findall(condition_body):
                    violations.append(
                        f"{path}:{line_number}: verbotene Kconfig-Verwendung "
                        f"{token!r} in portabler Wurzel"
                    )


def add_component_requires_violations(violations: list[str], root: Path) -> None:
    for relative_path, allowed in COMPONENT_REQUIRES_ALLOWLIST.items():
        path = root / relative_path
        if not path.exists():
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        found: set[str] = set()
        for pattern in (REQUIRES_PATTERN, PRIV_REQUIRES_PATTERN):
            for match in pattern.finditer(text):
                found.update(re.findall(r"[A-Za-z0-9_]+", match.group(1)))
        for name in sorted(found - allowed):
            violations.append(
                f"{path}: unerlaubte direkte IDF-Komponentenabhaengigkeit "
                f"in REQUIRES/PRIV_REQUIRES: {name!r}"
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

    add_idf_leak_violations(violations, root)
    add_component_requires_violations(violations, root)

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
        "lib/device_platform/CMakeLists.txt": (
            'idf_component_register(SRC_DIRS "src" INCLUDE_DIRS "src")\n'
        ),
        "lib/fermentation_app/CMakeLists.txt": (
            'idf_component_register(SRC_DIRS "src" INCLUDE_DIRS "src" '
            "REQUIRES device_platform)\n"
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

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        create_clean_fixture(root)
        if check(root):
            print(
                f"{FAILED}: saubere Architektur-Fixture wurde durch die "
                "IDF-Leak-Erweiterung abgelehnt"
            )
            return 1

        idf_leak_file = root / "lib" / "device_platform" / "src" / "idf_leak.hpp"
        idf_leak_file.write_text('#include "esp_system.h"\n', encoding="utf-8")
        if not check(root):
            print(f"{FAILED}: absichtlicher IDF-Include-Leak wurde nicht erkannt")
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
