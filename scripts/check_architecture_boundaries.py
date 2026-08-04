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

# Issue #72/#73: portable Quellwurzeln, die keinen ESP-IDF-/RTOS-Zugriff,
# keine Arduino-Abhaengigkeit und keinen Zugriff auf die Adaptergrenze
# device_platform_esp_idf enthalten duerfen. Bewusst eng gehalten (nicht
# main/ oder device_platform_esp_idf/ selbst, die beide ESP-IDF-Header
# verwenden duerfen), siehe docs/tasks/issue-72-implementation-plan.md,
# Abschnitt 9, und docs/tasks/issue-73-implementation-plan.md, Abschnitt 20.
IDF_LEAK_PORTABLE_ROOTS = (
    "lib/device_platform/src",
    "lib/fermentation_app/src",
)
IDF_LEAK_FORBIDDEN_EXACT_INCLUDES = (
    "Arduino.h",
    "sdkconfig.h",
    "nvs.h",
    "nvs_flash.h",
)
IDF_LEAK_FORBIDDEN_INCLUDE_PREFIXES = (
    "esp_",
    "driver/",
    "freertos/",
    "lwip/",
    "hal/",
    "soc/",
    "nvs/",
    "device_platform_esp_idf",
)
# Nur reale Include-Direktiven am Zeilenanfang, keine Kommentartreffer.
INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]')
PREPROCESSOR_CONDITION_PATTERN = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef|elif)\b(.*)$")
# Nur vollstaendige Praeprozessortokens, keine Teilstringtreffer
# (z. B. nicht in MY_ARDUINO_COMPATIBILITY).
PLATFORM_MACRO_PATTERN = re.compile(r"\b(?:ESP_PLATFORM|ARDUINO)\b")
CONFIG_TOKEN_PATTERN = re.compile(r"\bCONFIG_[A-Za-z0-9_]+\b")

# Issue #17: runtime code may decide commands/transitions, but only the
# persistence coordinator may apply an eligible mutation or release its
# effects/messages. Domain implementation and unit tests remain intentionally
# outside this production-path check.
RUN_PERSISTENCE_ALLOWED_FILES = frozenset(
    {
        "lib/fermentation_app/src/run_commands.cpp",
        "lib/fermentation_app/src/run_commands.hpp",
        "lib/fermentation_app/src/process_state_machine.cpp",
        "lib/fermentation_app/src/process_state_machine.hpp",
        "lib/fermentation_app/src/run_persistence_coordinator.cpp",
        "lib/fermentation_app/src/run_persistence_coordinator.hpp",
    }
)
RUN_PERSISTENCE_APPLY_PATTERN = re.compile(
    r"\b(?:applyRunCommand|applyProcessTransition)\s*\("
)
RUN_PERSISTENCE_DECISION_ASSIGNMENT_PATTERN = re.compile(
    r"\b(?:const\s+)?(?:auto|CommandDecision|TransitionDecision)\s+"
    r"(?P<name>[A-Za-z_]\w*)\s*=\s*decide[A-Za-z_]\w*\s*\("
)
RUN_PERSISTENCE_DECISION_MEMBER_PATTERN = re.compile(
    r"\b(?P<name>[A-Za-z_]\w*)\s*(?:\.|->)\s*"
    r"(?P<member>effects|messages)\b"
)
RUN_PERSISTENCE_TEMPORARY_MEMBER_PATTERN = re.compile(
    r"\bdecide[A-Za-z_]\w*\s*\([^;{}]*\)\s*\.\s*"
    r"(?:effects|messages)\b",
    re.DOTALL,
)

# Issue #72/#73: exakter idf_component_register()-REQUIRES/PRIV_REQUIRES-
# Vertrag je Komponente, getrennt nach oeffentlich (REQUIRES) und privat
# (PRIV_REQUIRES). Eine gemeinsame Menge wuerde z. B. ein faelschlich
# oeffentliches "REQUIRES esp_timer" nicht von einem korrekten
# "PRIV_REQUIRES esp_timer" unterscheiden. Geprueft werden beide Richtungen:
# unerlaubte zusaetzliche Namen UND fehlende vorgeschriebene Direktabhaengig-
# keiten (siehe add_component_requires_violations).
COMPONENT_REQUIRES_ALLOWLIST = {
    "lib/device_platform/CMakeLists.txt": {
        "public": frozenset(),
        "private": frozenset(),
    },
    "lib/fermentation_app/CMakeLists.txt": {
        "public": frozenset({"device_platform"}),
        "private": frozenset(),
    },
    "lib/device_platform_esp_idf/CMakeLists.txt": {
        "public": frozenset({"device_platform"}),
        "private": frozenset({"esp_timer"}),
    },
    "main/CMakeLists.txt": {
        "public": frozenset(),
        "private": frozenset(
            {"device_platform", "fermentation_app", "device_platform_esp_idf"}
        ),
    },
}
# Bekannte idf_component_register()-Schluesselwoerter: jedes davon beendet
# eine gerade offene REQUIRES-/PRIV_REQUIRES-Liste. Bewusst nur diese kleine,
# risikobasierte Menge -- keine vollstaendige CMake-Grammatik.
COMPONENT_REGISTER_KEYWORDS = frozenset(
    {
        "SRCS",
        "SRC_DIRS",
        "EXCLUDE_SRCS",
        "INCLUDE_DIRS",
        "PRIV_INCLUDE_DIRS",
        "REQUIRES",
        "PRIV_REQUIRES",
        "LDFRAGMENTS",
        "REQUIRED_IDF_TARGETS",
        "EMBED_FILES",
        "EMBED_TXTFILES",
        "KCONFIG",
        "KCONFIG_PROJBUILD",
        "WHOLE_ARCHIVE",
    }
)
COMPONENT_REGISTER_CALL_PATTERN = re.compile(r"idf_component_register\s*\(")
CMAKE_TOKEN_PATTERN = re.compile(r'"(?P<quoted>[^"]*)"|(?P<bare>\S+)')
CMAKE_IDENTIFIER_PATTERN = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


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
                include_match = INCLUDE_PATTERN.match(line)
                if include_match:
                    header = include_match.group(1)
                    if header in IDF_LEAK_FORBIDDEN_EXACT_INCLUDES or header.startswith(
                        IDF_LEAK_FORBIDDEN_INCLUDE_PREFIXES
                    ):
                        violations.append(
                            f"{path}:{line_number}: verbotener IDF-/RTOS-/Arduino-/"
                            f"Adapter-Include in portabler Wurzel: {header!r}"
                        )
                condition_match = PREPROCESSOR_CONDITION_PATTERN.match(line)
                if not condition_match:
                    continue
                condition_body = condition_match.group(1)
                for macro_match in PLATFORM_MACRO_PATTERN.finditer(condition_body):
                    violations.append(
                        f"{path}:{line_number}: verbotene Praeprozessorverwendung "
                        f"von {macro_match.group(0)!r} in portabler Wurzel"
                    )
                for token in CONFIG_TOKEN_PATTERN.findall(condition_body):
                    violations.append(
                        f"{path}:{line_number}: verbotene Kconfig-Verwendung "
                        f"{token!r} in portabler Wurzel"
                    )


def add_run_persistence_bypass_violations(violations: list[str], root: Path) -> None:
    """Reject direct productive apply/effect/message paths outside #17's gate."""
    for relative_root in ("lib/fermentation_app/src", "src", "main"):
        directory = root / relative_root
        for path in text_files(directory):
            relative = path.relative_to(root).as_posix()
            if relative in RUN_PERSISTENCE_ALLOWED_FILES:
                continue
            try:
                lines = path.read_text(encoding="utf-8").splitlines()
            except UnicodeDecodeError:
                continue
            source = "\n".join(lines)
            # Keep names scoped to their brace block. A local `result` from
            # one function must not taint an unrelated `result` elsewhere.
            blocks: list[tuple[int, int, set[str]]] = []
            stack: list[int] = []
            for index, character in enumerate(source):
                if character == "{":
                    stack.append(index)
                elif character == "}" and stack:
                    blocks.append((stack.pop(), index, set()))
            for match in RUN_PERSISTENCE_DECISION_ASSIGNMENT_PATTERN.finditer(source):
                containing = [
                    block
                    for block in blocks
                    if block[0] < match.start() < block[1]
                ]
                if containing:
                    scope = min(containing, key=lambda block: block[1] - block[0])
                    scope[2].add(match.group("name"))

            def decision_name_in_scope(name: str, position: int) -> bool:
                return any(
                    name in block[2] and block[0] < position < block[1]
                    for block in blocks
                )

            line_offsets: list[int] = []
            offset = 0
            for line in lines:
                line_offsets.append(offset)
                offset += len(line) + 1
            temporary_member_lines = {
                source[: match.start()].count("\n") + 1
                for match in RUN_PERSISTENCE_TEMPORARY_MEMBER_PATTERN.finditer(source)
            }
            for line_number, line in enumerate(lines, start=1):
                direct_apply = RUN_PERSISTENCE_APPLY_PATTERN.search(line)
                member = RUN_PERSISTENCE_DECISION_MEMBER_PATTERN.search(line)
                member_position = (
                    line_offsets[line_number - 1] + member.start()
                    if member is not None
                    else 0
                )
                decision_named_member = (
                    member is not None
                    and (
                        member.group("name") == "decision"
                        or member.group("name").endswith("Decision")
                        or decision_name_in_scope(
                            member.group("name"), member_position
                        )
                    )
                )
                if direct_apply or decision_named_member or line_number in temporary_member_lines:
                    violations.append(
                        f"{path}:{line_number}: produktiver Run-Persistenz-Bypass "
                        "(apply/effects/messages ausserhalb Domain/Coordinator)"
                    )


def strip_cmake_line_comments(text: str) -> str:
    """Entfernt CMake-Zeilenkommentare (# ausserhalb von Anführungszeichen)."""
    cleaned_lines = []
    for line in text.splitlines():
        in_quotes = False
        cut_at = len(line)
        for index, char in enumerate(line):
            if char == '"':
                in_quotes = not in_quotes
            elif char == "#" and not in_quotes:
                cut_at = index
                break
        cleaned_lines.append(line[:cut_at])
    return "\n".join(cleaned_lines)


def extract_component_register_body(text: str) -> str | None:
    """Liefert den Inhalt der ersten idf_component_register(...)-Klammer."""
    call_match = COMPONENT_REGISTER_CALL_PATTERN.search(text)
    if not call_match:
        return None
    depth = 1
    index = call_match.end()
    start = index
    while index < len(text) and depth > 0:
        char = text[index]
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        index += 1
    if depth != 0:
        return None
    return text[start : index - 1]


def collect_component_requires(body: str) -> tuple[set[str], set[str], list[str]]:
    """Wertet REQUIRES/PRIV_REQUIRES innerhalb einer Registrierung aus.

    Liefert (oeffentliche Namen aus REQUIRES, private Namen aus
    PRIV_REQUIRES, nicht statisch pruefbare Tokens aus beiden). Jedes andere
    bekannte Schluesselwort beendet die aktuell offene Liste; Quotes bieten
    keinen Bypass.
    """
    public_names: set[str] = set()
    private_names: set[str] = set()
    dynamic_tokens: list[str] = []
    mode: str | None = None
    for match in CMAKE_TOKEN_PATTERN.finditer(body):
        quoted = match.group("quoted")
        value = quoted if quoted is not None else match.group("bare")
        is_quoted = quoted is not None
        if not is_quoted and value in COMPONENT_REGISTER_KEYWORDS:
            mode = value if value in ("REQUIRES", "PRIV_REQUIRES") else None
            continue
        if mode is None:
            continue
        if CMAKE_IDENTIFIER_PATTERN.match(value):
            (public_names if mode == "REQUIRES" else private_names).add(value)
        else:
            dynamic_tokens.append(value)
    return public_names, private_names, dynamic_tokens


def add_component_requires_violations(violations: list[str], root: Path) -> None:
    for relative_path, allowed in COMPONENT_REQUIRES_ALLOWLIST.items():
        path = root / relative_path
        if not path.exists():
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        body = extract_component_register_body(strip_cmake_line_comments(text))
        if body is None:
            continue
        public_names, private_names, dynamic_tokens = collect_component_requires(body)
        for name in sorted(public_names - allowed["public"]):
            violations.append(
                f"{path}: unerlaubte oeffentliche IDF-Komponentenabhaengigkeit "
                f"in REQUIRES: {name!r}"
            )
        for name in sorted(allowed["public"] - public_names):
            violations.append(
                f"{path}: fehlende oeffentliche Direktabhaengigkeit "
                f"in REQUIRES: {name!r}"
            )
        for name in sorted(private_names - allowed["private"]):
            violations.append(
                f"{path}: unerlaubte private IDF-Komponentenabhaengigkeit "
                f"in PRIV_REQUIRES: {name!r}"
            )
        for name in sorted(allowed["private"] - private_names):
            violations.append(
                f"{path}: fehlende private Direktabhaengigkeit "
                f"in PRIV_REQUIRES: {name!r}"
            )
        for token in dynamic_tokens:
            violations.append(
                f"{path}: nicht statisch pruefbare CMake-Abhaengigkeit "
                f"in REQUIRES/PRIV_REQUIRES: {token!r}"
            )


def check(root: Path) -> list[str]:
    violations: list[str] = []
    platform = root / "lib" / "device_platform"
    test_support = root / "lib" / "device_platform_test_support"
    fermentation_app = root / "lib" / "fermentation_app"
    platform_esp_idf = root / "lib" / "device_platform_esp_idf"
    main_cpp = root / "src" / "main.cpp"
    main_dir = root / "main"

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
    add_reference_violations(
        violations,
        platform_esp_idf,
        ("fermentation_app", "device_platform_test_support"),
        "ESP-IDF-Adapter darf Anwendung oder Test-Support nicht verwenden",
    )

    if main_cpp.exists():
        add_reference_violations(
            violations,
            main_cpp.parent,
            ("device_platform_test_support",),
            "Composition Root darf Test-Support nicht verwenden",
        )
    if main_dir.exists():
        add_reference_violations(
            violations,
            main_dir,
            ("device_platform_test_support",),
            "ESP-IDF-Composition-Root darf Test-Support nicht verwenden",
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
    add_run_persistence_bypass_violations(violations, root)
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
        "lib/device_platform_esp_idf/src/esp_timer_time_source.hpp": (
            '#pragma once\n#include "time_source.hpp"\n'
        ),
        "lib/device_platform_esp_idf/CMakeLists.txt": (
            'idf_component_register(SRC_DIRS "src" INCLUDE_DIRS "src" '
            'REQUIRES device_platform PRIV_REQUIRES esp_timer)\n'
        ),
        "main/app_main.cpp": '#include "device_platform.hpp"\n',
        "main/CMakeLists.txt": (
            'idf_component_register(SRCS "app_main.cpp" '
            'PRIV_INCLUDE_DIRS "../include" PRIV_REQUIRES '
            "device_platform fermentation_app device_platform_esp_idf)\n"
        ),
    }
    for relative_path, content in files.items():
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")


# Issue-#72-Reviewfix: kleine tabellengesteuerte Selftest-Faelle statt
# mehrerer fast identischer Testfunktionen. Jeder Fall schreibt genau eine
# Zusatz-/Ersatzdatei auf eine sonst saubere Fixture.
IDF_LEAK_CLEAN_CASES = {
    "kommentierter_include": (
        "lib/device_platform/src/idf_leak_case.hpp",
        '// #include "esp_system.h"\n',
    ),
    "aehnlicher_eigener_makroname": (
        "lib/device_platform/src/idf_leak_case.hpp",
        "#if defined(MY_ARDUINO_COMPATIBILITY)\n#endif\n",
    ),
}
IDF_LEAK_VIOLATION_CASES = {
    "include_mit_leerzeichen_nach_raute": (
        "lib/device_platform/src/idf_leak_case.hpp",
        '  #  include "esp_system.h"\n',
    ),
    "arduino_header": (
        "lib/device_platform/src/idf_leak_case.hpp",
        "#include <Arduino.h>\n",
    ),
    "device_platform_esp_idf_include": (
        "lib/fermentation_app/src/idf_leak_case.hpp",
        '#include "device_platform_esp_idf/time_source.hpp"\n',
    ),
    "reales_arduino_makro": (
        "lib/device_platform/src/idf_leak_case.hpp",
        "#if defined(ARDUINO)\n#endif\n",
    ),
    "reales_esp_platform_makro": (
        "lib/fermentation_app/src/idf_leak_case.hpp",
        "#ifdef ESP_PLATFORM\n#endif\n",
    ),
    "bare_unerlaubte_requires": (
        "lib/device_platform/CMakeLists.txt",
        'idf_component_register(SRC_DIRS "src" INCLUDE_DIRS "src" '
        "REQUIRES driver)\n",
    ),
    "gequotete_unerlaubte_requires": (
        "lib/device_platform/CMakeLists.txt",
        'idf_component_register(SRC_DIRS "src" INCLUDE_DIRS "src" '
        'REQUIRES "driver")\n',
    ),
    "dynamische_requires": (
        "lib/fermentation_app/CMakeLists.txt",
        "set(PORTABLE_DEPS device_platform)\n"
        'idf_component_register(SRC_DIRS "src" INCLUDE_DIRS "src" '
        "REQUIRES ${PORTABLE_DEPS})\n",
    ),
    # Issue #73: device_platform_esp_idf und main/ ergaenzt.
    "esp_timer_faelschlich_oeffentlich": (
        "lib/device_platform_esp_idf/CMakeLists.txt",
        'idf_component_register(SRC_DIRS "src" INCLUDE_DIRS "src" '
        "REQUIRES device_platform esp_timer)\n",
    ),
    "adapter_referenziert_fermentation_app": (
        "lib/device_platform_esp_idf/src/bad.hpp",
        '#include "fermentation_application.hpp"\n',
    ),
    "adapter_unerlaubte_private_idf_komponente": (
        "lib/device_platform_esp_idf/CMakeLists.txt",
        'idf_component_register(SRC_DIRS "src" INCLUDE_DIRS "src" '
        "REQUIRES device_platform PRIV_REQUIRES esp_timer driver)\n",
    ),
    "main_referenziert_test_support": (
        "main/app_main.cpp",
        '#include "device_platform_test_support/mock_time_source.hpp"\n',
    ),
    "main_unerlaubte_hardwarekomponente": (
        "main/CMakeLists.txt",
        'idf_component_register(SRCS "app_main.cpp" '
        'PRIV_INCLUDE_DIRS "../include" PRIV_REQUIRES '
        "device_platform fermentation_app device_platform_esp_idf driver)\n",
    ),
    "main_faelschlich_oeffentliche_requires": (
        "main/CMakeLists.txt",
        'idf_component_register(SRCS "app_main.cpp" '
        'PRIV_INCLUDE_DIRS "../include" REQUIRES device_platform '
        "PRIV_REQUIRES fermentation_app device_platform_esp_idf)\n",
    ),
    "fehlende_oeffentliche_requires": (
        "lib/fermentation_app/CMakeLists.txt",
        'idf_component_register(SRC_DIRS "src" INCLUDE_DIRS "src")\n',
    ),
    "fehlende_private_requires_in_main": (
        "main/CMakeLists.txt",
        'idf_component_register(SRCS "app_main.cpp" '
        'PRIV_INCLUDE_DIRS "../include" '
        "PRIV_REQUIRES fermentation_app device_platform_esp_idf)\n",
    ),
    "runtime_umgeht_run_persistenz_apply": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { applyRunCommand(); }\n",
    ),
    "runtime_gibt_effects_vor_commit_frei": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { decision.effects; }\n",
    ),
    "runtime_gibt_effects_ueber_zeiger_vor_commit_frei": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { commandDecision->effects; }\n",
    ),
    "ui_umgeht_transition_apply": (
        "lib/fermentation_app/src/ui_path.cpp",
        "void f() { applyProcessTransition(); }\n",
    ),
    "ui_gibt_transition_messages_ueber_zeiger_frei": (
        "lib/fermentation_app/src/ui_path.cpp",
        "void f() { transitionDecision->messages; }\n",
    ),
    "composition_root_gibt_transition_messages_frei": (
        "main/app_main.cpp",
        "void f() { decision.messages; }\n",
    ),
    "composition_root_gibt_transition_messages_ueber_zeiger_frei": (
        "main/app_main.cpp",
        "void f() { transitionDecision->messages; }\n",
    ),
}


RUN_PERSISTENCE_BYPASS_CASES = {
    "runtime_apply_command": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { applyRunCommand(); }\n",
    ),
    "runtime_apply_transition": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { applyProcessTransition(); }\n",
    ),
    "runtime_effects_dot": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { decision.effects; }\n",
    ),
    "runtime_effects_arrow": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { commandDecision->effects; }\n",
    ),
    "runtime_effects_from_decide_result": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { auto result = decideRun(); result.effects; }\n",
    ),
    "runtime_messages_from_decide_result": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { const auto result = decideTransition(); result.messages; }\n",
    ),
    "runtime_effects_from_multiline_decide_result": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n  auto result =\n      decideRun();\n  result.effects;\n}\n",
    ),
    "runtime_messages_from_decide_temporary": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { decideTransition().messages; }\n",
    ),
    "ui_messages_dot": (
        "lib/fermentation_app/src/ui_path.cpp",
        "void f() { transitionDecision.messages; }\n",
    ),
    "ui_messages_arrow": (
        "lib/fermentation_app/src/ui_path.cpp",
        "void f() { transitionDecision->messages; }\n",
    ),
    "composition_messages_dot": (
        "main/app_main.cpp",
        "void f() { decision.messages; }\n",
    ),
    "composition_messages_arrow": (
        "main/app_main.cpp",
        "void f() { transitionDecision->messages; }\n",
    ),
}

# The bypass rule deliberately follows values originating from decide*().  It
# must not turn into a repository-wide ban on unrelated members with the same
# spelling.
RUN_PERSISTENCE_CLEAN_CASES = {
    "reused_result_name_is_not_a_bypass": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void a() { auto result = decideRun(); }\n"
        "struct RenderResult { int effects; };\n"
        "void b() { RenderResult result{}; use(result.effects); }\n",
    ),
    "unrelated_effects_member": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "struct RenderState { int effects; };\n"
        "void f() { RenderState state{}; (void)state.effects; }\n",
    ),
    "unrelated_messages_member": (
        "main/app_main.cpp",
        "struct DisplayState { int messages; };\n"
        "void f() { DisplayState state{}; (void)state.messages; }\n",
    ),
}


def _check_clean_fixture_with_extra_file(relative_path: str, content: str) -> list[str]:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        create_clean_fixture(root)
        extra = root / relative_path
        extra.parent.mkdir(parents=True, exist_ok=True)
        extra.write_text(content, encoding="utf-8")
        return check(root)


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

    for name, (relative_path, content) in IDF_LEAK_CLEAN_CASES.items():
        if _check_clean_fixture_with_extra_file(relative_path, content):
            print(
                f"{FAILED}: sauberer IDF-Leak-Fall {name!r} wurde faelschlich "
                "als Verstoss erkannt"
            )
            return 1

    for name, (relative_path, content) in IDF_LEAK_VIOLATION_CASES.items():
        if not _check_clean_fixture_with_extra_file(relative_path, content):
            print(f"{FAILED}: IDF-Leak-Verstossfall {name!r} wurde nicht erkannt")
            return 1

    for name, (relative_path, content) in RUN_PERSISTENCE_BYPASS_CASES.items():
        if not _check_clean_fixture_with_extra_file(relative_path, content):
            print(
                f"{FAILED}: Run-Persistenz-Bypass {name!r} wurde nicht erkannt"
            )
            return 1

    for name, (relative_path, content) in RUN_PERSISTENCE_CLEAN_CASES.items():
        if _check_clean_fixture_with_extra_file(relative_path, content):
            print(
                f"{FAILED}: fachfremder Member-Fall {name!r} wurde faelschlich "
                "als Bypass erkannt"
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
