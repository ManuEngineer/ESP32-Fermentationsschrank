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
# applyRunCommand/applyProcessTransition are two exact, internal domain
# symbols never legitimately spelled outside the allowlisted files above.
# Any textual occurrence of either token (direct call, qualified call,
# address-of with or without `&`, parenthesized, passed as a callback, or
# aliased under any local name) is already a bypass, so a single token guard
# on the masked source supersedes tracking every C++ spelling of "take this
# function's address" individually.
RUN_PERSISTENCE_APPLY_SYMBOL_PATTERN = re.compile(
    r"\b(?:applyRunCommand|applyProcessTransition)\b"
)
# `auto`/`const auto`, optionally bound as `&`/`&&`, assigned from a
# (possibly namespace-qualified) decide*() call.
RUN_PERSISTENCE_DECISION_ASSIGNMENT_PATTERN = re.compile(
    r"\b(?:const\s+)?(?:auto(?:\s*&{1,2})?|CommandDecision|TransitionDecision)"
    r"\s+(?P<name>[A-Za-z_]\w*)\s*=\s*"
    r"(?:[A-Za-z_]\w*\s*::\s*)*decide[A-Za-z_]\w*\s*\("
)
# A typed CommandDecision/TransitionDecision local variable is a decision
# origin regardless of its initializer (e.g. a default-constructed
# `CommandDecision decision;`). Kept separate from the assignment pattern
# above so a fachfremd (unrelated) type with the same declaration shape is
# never classified as a decision origin.
RUN_PERSISTENCE_TYPED_DECISION_DECLARATION_PATTERN = re.compile(
    r"\b(?:const\s+)?(?:CommandDecision|TransitionDecision)\s*[*&]?\s*"
    r"(?P<name>[A-Za-z_]\w*)\s*(?:=(?!=)|;|\{|\()"
)
RUN_PERSISTENCE_DECISION_MEMBER_PATTERN = re.compile(
    r"\b(?P<name>[A-Za-z_]\w*)\s*\)*\s*(?:\.|->)\s*"
    r"(?P<member>effects|messages)\b"
)
# Control-flow parentheses (`if (...) {`, `for (...) {`, ...) share the same
# `(...)  {` textual shape as a function signature but are never a
# parameter list.
RUN_PERSISTENCE_CONTROL_FLOW_BEFORE_PAREN = re.compile(
    r"\b(?:if|while|for|switch|catch)\s*$"
)
# Method qualifiers a signature's `)` may be followed by before `{`:
# `const`, `noexcept`, `override`, `final`, and ref-qualifiers `&`/`&&`, in
# any combination and order.
RUN_PERSISTENCE_METHOD_QUALIFIER_SUFFIX_PATTERN = re.compile(
    r"\)(?:\s*(?:const|noexcept|override|final|&&|&))*\s*$"
)
# A single `[const] [Namespace::]* Type[ &|*] name [= default]` parameter.
# The base type is captured after stripping any namespace qualification
# (`fermentation::CommandDecision` -> `CommandDecision`). Deliberately does
# not handle multi-name declarations or nested parentheses in default
# arguments -- out of scope for this narrow guard.
RUN_PERSISTENCE_PARAM_DECLARATION_PATTERN = re.compile(
    r"^(?:const\s+)?(?:[A-Za-z_]\w*\s*::\s*)*([A-Za-z_]\w*)\s*[&*]{0,2}\s*"
    r"([A-Za-z_]\w*)\s*(?:=.*)?$"
)
RUN_PERSISTENCE_DECISION_PARAMETER_TYPES = frozenset(
    {"CommandDecision", "TransitionDecision"}
)


def _run_persistence_function_parameter_list(
    code: str, brace_index: int
) -> str | None:
    """Return a function/method body's parameter-list text given the index
    of its opening `{`, or None if `brace_index` does not open one.

    A small balanced-delimiter scan, not a general C++ parser: walk
    backward from `{` over whitespace and method qualifiers to the
    parameter list's `)`, then backward again by paren balance to its `(`,
    and reject control-flow parentheses sharing the same shape.

    Known gap: a constructor with a member-initializer list
    (`Foo::Foo(const CommandDecision& pending) : cached_(pending) {`) has no
    qualifier suffix directly before `{`, so this scan does not resolve the
    real parameter list for that shape; it registers nothing for that block
    rather than mis-registering a wrong one. No current production file has
    this shape.
    """
    window_start = max(0, brace_index - 80)
    qualifier_match = RUN_PERSISTENCE_METHOD_QUALIFIER_SUFFIX_PATTERN.search(
        code[window_start:brace_index]
    )
    if not qualifier_match:
        return None
    close_paren = window_start + qualifier_match.start()
    depth = 0
    index = close_paren
    while index >= 0:
        if code[index] == ")":
            depth += 1
        elif code[index] == "(":
            depth -= 1
            if depth == 0:
                break
        index -= 1
    if index < 0:
        return None
    open_paren = index
    preceding = code[max(0, open_paren - 10) : open_paren]
    if RUN_PERSISTENCE_CONTROL_FLOW_BEFORE_PAREN.search(preceding):
        return None
    return code[open_paren + 1 : close_paren]


def _run_persistence_split_top_level(text: str) -> list[str]:
    """Split a parameter list at top-level commas, respecting nesting in
    `()`/`[]`/`<>` (default arguments, templates) without a general parser."""
    parts: list[str] = []
    depth = 0
    current: list[str] = []
    for character in text:
        if character in "([<":
            depth += 1
        elif character in ")]>":
            depth = max(0, depth - 1)
        if character == "," and depth == 0:
            parts.append("".join(current))
            current = []
        else:
            current.append(character)
    parts.append("".join(current))
    return parts
RUN_PERSISTENCE_TEMPORARY_MEMBER_PATTERN = re.compile(
    r"\bdecide[A-Za-z_]\w*\s*\([^;{}]*\)\s*\.\s*"
    r"(?:effects|messages)\b",
    re.DOTALL,
)
# Narrow, non-parsing shadowing detector: a block-local declaration of NAME
# whose immediately preceding token is not a control-flow/statement keyword
# (so `return result;` is never mistaken for a declaration of `result`).
RUN_PERSISTENCE_DECLARATION_KEYWORD_BLOCKLIST = (
    "return", "if", "while", "for", "switch", "else", "new", "delete",
    "sizeof", "throw", "case", "break", "continue", "goto", "using",
    "namespace", "co_return", "co_await", "co_yield", "typedef",
    "static_assert", "do", "catch", "try", "default",
)
RUN_PERSISTENCE_DECLARATION_KEYWORD_ALTERNATION = "|".join(
    re.escape(keyword)
    for keyword in RUN_PERSISTENCE_DECLARATION_KEYWORD_BLOCKLIST
)


def _run_persistence_local_declaration_pattern(name: str) -> re.Pattern:
    escaped = re.escape(name)
    return re.compile(
        rf"\b(?!(?:{RUN_PERSISTENCE_DECLARATION_KEYWORD_ALTERNATION})\b)"
        rf"[A-Za-z_]\w*(?:\s*<[^;{{}}()]*>)?\s*[*&]{{0,2}}\s*"
        rf"\b{escaped}\b\s*(?:=(?!=)|;|\{{|\()"
    )


def mask_cxx_comments_and_strings(source: str) -> str:
    """Mask comments and literals while preserving offsets and line breaks."""
    masked = list(source)
    state = "code"
    quote = ""
    escaped = False
    index = 0
    while index < len(source):
        character = source[index]
        next_character = source[index + 1] if index + 1 < len(source) else ""
        if state == "code":
            if character == "/" and next_character == "/":
                masked[index] = masked[index + 1] = " "
                state = "line_comment"
                index += 2
                continue
            if character == "/" and next_character == "*":
                masked[index] = masked[index + 1] = " "
                state = "block_comment"
                index += 2
                continue
            if character == '"':
                masked[index] = " "
                state = "string"
                quote = character
                escaped = False
            elif character == "'":
                masked[index] = " "
                state = "character"
                quote = character
                escaped = False
        elif state == "line_comment":
            if character == "\n":
                state = "code"
            else:
                masked[index] = " "
        elif state == "block_comment":
            if character == "*" and next_character == "/":
                masked[index] = masked[index + 1] = " "
                state = "code"
                index += 2
                continue
            if character != "\n":
                masked[index] = " "
        else:
            if character == "\n":
                masked[index] = " "
            elif escaped:
                masked[index] = " "
                escaped = False
            elif character == "\\":
                masked[index] = " "
                escaped = True
            elif character == quote:
                masked[index] = " "
                state = "code"
            else:
                masked[index] = " "
        index += 1
    return "".join(masked)

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
        "private": frozenset({"esp_timer", "nvs_flash"}),
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
            code = mask_cxx_comments_and_strings(source)
            # Keep names scoped to their brace block, walked from the
            # innermost enclosing block outward. Within each block, only
            # declarations positioned before the use are visible (so a
            # shadowing declaration written AFTER a use does not retroactively
            # cover it); the nearest visible declaration in the first block
            # that has one decides whether a member use is a decision origin
            # or unrelated (fachfremd) shadowing. (Apply-alias tracking was
            # removed: RUN_PERSISTENCE_APPLY_SYMBOL_PATTERN below already
            # flags every spelling of the two apply symbols directly.)
            blocks: list[tuple[int, int, dict[str, list[tuple[int, str]]]]] = []
            stack: list[int] = []
            for index, character in enumerate(code):
                if character == "{":
                    stack.append(index)
                elif character == "}" and stack:
                    blocks.append((stack.pop(), index, {}))
            block_by_open_brace = {block[0]: block for block in blocks}

            def containing_blocks(position: int):
                found = [
                    block
                    for block in blocks
                    if block[0] < position < block[1]
                ]
                found.sort(key=lambda block: block[1] - block[0])
                return found

            def register(name: str, position: int, kind: str) -> None:
                containing = containing_blocks(position)
                if containing:
                    containing[0][2].setdefault(name, []).append((position, kind))

            for match in RUN_PERSISTENCE_DECISION_ASSIGNMENT_PATTERN.finditer(code):
                register(match.group("name"), match.start(), "decision")
            for match in RUN_PERSISTENCE_TYPED_DECISION_DECLARATION_PATTERN.finditer(
                code
            ):
                register(match.group("name"), match.start(), "decision")

            # Typed CommandDecision/TransitionDecision function parameters
            # (including namespace-qualified and method-qualified forms) are
            # decision origins for their whole body (registered at the
            # block's own opening brace, i.e. visible from the first
            # statement on); other typed parameters register as "other" so a
            # fachfremd parameter merely named `decision` stays clean instead
            # of falling through to the bare-name fallback below.
            for block in blocks:
                parameter_list = _run_persistence_function_parameter_list(
                    code, block[0]
                )
                if parameter_list is None:
                    continue
                for raw_param in _run_persistence_split_top_level(parameter_list):
                    param = raw_param.strip()
                    if not param:
                        continue
                    param_match = RUN_PERSISTENCE_PARAM_DECLARATION_PATTERN.match(
                        param
                    )
                    if not param_match:
                        continue
                    param_type, param_name = param_match.group(1), param_match.group(2)
                    kind = (
                        "decision"
                        if param_type in RUN_PERSISTENCE_DECISION_PARAMETER_TYPES
                        else "other"
                    )
                    block[2].setdefault(param_name, []).append((block[0], kind))

            other_declaration_cache: dict[tuple[str, int, int], list[int]] = {}

            def other_declaration_positions(
                name: str, block: tuple[int, int, dict[str, list[tuple[int, str]]]]
            ) -> list[int]:
                key = (name, block[0], block[1])
                cached = other_declaration_cache.get(key)
                if cached is None:
                    pattern = _run_persistence_local_declaration_pattern(name)
                    cached = [
                        found.start()
                        for found in pattern.finditer(code, block[0], block[1])
                    ]
                    other_declaration_cache[key] = cached
                return cached

            def declared_kind_before(name: str, position: int) -> str | None:
                for block in containing_blocks(position):
                    specific = [
                        (decl_position, kind)
                        for decl_position, kind in block[2].get(name, ())
                        if decl_position < position
                    ]
                    if specific:
                        # A decision/parameter registration and the
                        # generic "other" fallback pattern can both match the
                        # same declaration statement, sometimes at different
                        # offsets (e.g. the specific pattern captures a
                        # leading `const` that the generic type-token does
                        # not). A real C++ block cannot validly hold a second,
                        # different declaration of the same name, so a
                        # visible specific registration is authoritative for
                        # this block; the generic pattern is not consulted.
                        specific.sort(key=lambda candidate: candidate[0])
                        return specific[-1][1]
                    if any(
                        decl_position < position
                        for decl_position in other_declaration_positions(name, block)
                    ):
                        return "other"
                return None

            def line_number(position: int) -> int:
                return code.count("\n", 0, position) + 1

            for match in RUN_PERSISTENCE_APPLY_SYMBOL_PATTERN.finditer(code):
                violations.append(
                    f"{path}:{line_number(match.start())}: produktiver "
                    "Run-Persistenz-Bypass (apply/effects/messages ausserhalb "
                    "Domain/Coordinator)"
                )

            for match in RUN_PERSISTENCE_DECISION_MEMBER_PATTERN.finditer(code):
                name = match.group("name")
                kind = declared_kind_before(name, match.start())
                is_bypass = kind == "decision" or (
                    kind is None and (name == "decision" or name.endswith("Decision"))
                )
                if is_bypass:
                    violations.append(
                        f"{path}:{line_number(match.start())}: produktiver "
                        "Run-Persistenz-Bypass (apply/effects/messages ausserhalb "
                        "Domain/Coordinator)"
                    )

            for match in RUN_PERSISTENCE_TEMPORARY_MEMBER_PATTERN.finditer(code):
                violations.append(
                    f"{path}:{line_number(match.start())}: produktiver "
                    "Run-Persistenz-Bypass (apply/effects/messages ausserhalb "
                    "Domain/Coordinator)"
                )


# Issue #21, Plan Abschnitt 7/9.7: die vier Sensorselektions-/Kommando-
# vertragsheader duerfen keinen gegenseitigen Include-Zyklus bilden. Nur die
# beiden explizit ausgeschlossenen Kopplungen sind ueberhaupt erreichbar
# (siehe Abschnitt 7); jede Richtung wird geprueft.
SENSOR_SELECTION_GUARDED_HEADERS = (
    "sensor_selection_types.hpp",
    "sensor_selection.hpp",
    "run_commands.hpp",
    "run_persistence_contract.hpp",
)
SENSOR_SELECTION_FORBIDDEN_PAIRS = (
    ("run_commands.hpp", "sensor_selection.hpp"),
    ("sensor_selection.hpp", "run_persistence_contract.hpp"),
)


def _local_header_includes(path: Path) -> set[str]:
    """Nur lokale, unqualifizierte .hpp-Includes (kein <...>, kein Unterpfad)
    - das ist bereits der vollstaendige Suchraum fuer flache
    lib/fermentation_app/src-Header."""
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (UnicodeDecodeError, FileNotFoundError):
        return set()
    includes: set[str] = set()
    for line in lines:
        match = INCLUDE_PATTERN.match(line)
        if not match:
            continue
        header = match.group(1)
        if header.endswith(".hpp") and "/" not in header:
            includes.add(header)
    return includes


def _transitive_includes(start: str, graph: dict[str, set[str]]) -> set[str]:
    seen: set[str] = set()
    stack = [start]
    while stack:
        current = stack.pop()
        for included in graph.get(current, ()):
            if included not in seen:
                seen.add(included)
                stack.append(included)
    return seen


def add_sensor_selection_include_cycle_violations(
    violations: list[str], root: Path
) -> None:
    """#21, Plan Abschnitt 7/9.7: kein gegenseitiger Include zwischen
    run_commands.hpp und sensor_selection.hpp, und keiner zwischen
    sensor_selection.hpp und run_persistence_contract.hpp - weder direkt noch
    transitiv ueber einen Zwischenheader. Greift nur, wenn es ueberhaupt ein
    lib/fermentation_app/src gibt (z. B. nicht fuer ein anderes Repository);
    fehlt dort jedoch einer der vier vorausgesetzten Vertragsheader, ist das
    selbst ein Befund statt eines stillen Uebersprungs - sonst wuerde eine
    Umbenennung/Entfernung eines Headers die Pruefung unbemerkt abschalten."""
    src_dir = root / "lib" / "fermentation_app" / "src"
    if not src_dir.is_dir():
        return
    guarded_paths = {
        header: src_dir / header for header in SENSOR_SELECTION_GUARDED_HEADERS
    }
    missing = [header for header, path in guarded_paths.items() if not path.exists()]
    if missing:
        for header in missing:
            violations.append(
                f"{src_dir / header}: von Issue #21 vorausgesetzter "
                "Vertragsheader fehlt - Include-Zyklus-Pruefung (Plan #21 "
                "Abschnitt 7/9.7) kann nicht durchgefuehrt werden"
            )
        return
    graph: dict[str, set[str]] = {}
    to_visit = list(SENSOR_SELECTION_GUARDED_HEADERS)
    visited = set(to_visit)
    while to_visit:
        current = to_visit.pop()
        if current not in graph:
            graph[current] = _local_header_includes(src_dir / current)
        for included in graph[current]:
            if included not in visited:
                visited.add(included)
                to_visit.append(included)

    for left, right in SENSOR_SELECTION_FORBIDDEN_PAIRS:
        if right in _transitive_includes(left, graph):
            violations.append(
                f"{src_dir / left}: verbotene Include-Abhaengigkeit (direkt "
                f"oder transitiv) auf {right!r} (Plan #21 Abschnitt 7)"
            )
        if left in _transitive_includes(right, graph):
            violations.append(
                f"{src_dir / right}: verbotene Include-Abhaengigkeit (direkt "
                f"oder transitiv) auf {left!r} (Plan #21 Abschnitt 7)"
            )


# Issue #21, Plan Abschnitt 9.7 (PR-#99-Abschlussreview-Korrektur):
# applySensorSelectionDecision ist die eine kanonische Entscheidungs-/
# Mutationsfunktion - exakt eine Deklaration in sensor_selection.hpp und
# eine Definition in sensor_selection.cpp, beide mit dem Rueckgabetyp
# SensorSelectionStateMutation. Jede weitere gleichnamige Signatur ist eine
# verbotene Parallelfunktion - auch innerhalb dieser beiden Dateien selbst
# (z. B. eine zweite Deklaration), und auch mit einem anderen
# Rueckgabetyp-Praefix wie dem vollstaendigen RunCommandState. Das
# Signaturmuster ist daher bewusst nicht auf den kanonischen Rueckgabetyp
# beschraenkt, sondern erkennt jeden Rueckgabetyp-prefixierten Aufruf; die
# Klassifikation (kanonisch vs. Parallelfunktion) erfolgt danach getrennt.
# Ein reiner Aufruf (`applySensorSelectionDecision(view, decision, now)`)
# traegt kein Rueckgabetyp-Praefix und loest den Guard nicht aus.
SENSOR_SELECTION_SIGNATURE_PATTERN = re.compile(
    r"(?P<returntype>[A-Za-z_]\w*(?:\s*::\s*[A-Za-z_]\w*)*\s*[*&]?)\s+"
    r"(?:fermentation\s*::\s*)?applySensorSelectionDecision\s*\("
)
SENSOR_SELECTION_CANONICAL_RETURN_TYPE = "SensorSelectionStateMutation"
SENSOR_SELECTION_DECLARATION_FILE = "lib/fermentation_app/src/sensor_selection.hpp"
SENSOR_SELECTION_DEFINITION_FILE = "lib/fermentation_app/src/sensor_selection.cpp"
SENSOR_SELECTION_CANONICAL_ALLOWED_FILES = frozenset(
    {SENSOR_SELECTION_DECLARATION_FILE, SENSOR_SELECTION_DEFINITION_FILE}
)


def add_sensor_selection_canonical_function_violations(
    violations: list[str], root: Path
) -> None:
    lib_dir = root / "lib"
    if not lib_dir.exists():
        return
    declaration_path = root / SENSOR_SELECTION_DECLARATION_FILE
    definition_path = root / SENSOR_SELECTION_DEFINITION_FILE
    canonical_site_found = {declaration_path: False, definition_path: False}
    for path in text_files(lib_dir):
        relative = path.relative_to(root).as_posix()
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        # Kommentare und Stringliterale (z. B. "applySensorSelectionDecision
        # (sensor_selection.hpp)" in einem Fliesstextkommentar) duerfen den
        # Guard nicht ausloesen - nur echter Code zaehlt.
        code = mask_cxx_comments_and_strings(text)
        matches = list(SENSOR_SELECTION_SIGNATURE_PATTERN.finditer(code))
        if not matches:
            continue
        if relative not in SENSOR_SELECTION_CANONICAL_ALLOWED_FILES:
            violations.append(
                f"{path}: applySensorSelectionDecision-Signatur ausserhalb "
                "sensor_selection.hpp/.cpp (Plan #21 Abschnitt 9.7 - keine "
                "Parallelfunktion)"
            )
            continue
        canonical_matches = [
            match
            for match in matches
            if match.group("returntype").strip()
            == SENSOR_SELECTION_CANONICAL_RETURN_TYPE
        ]
        if len(matches) > len(canonical_matches):
            violations.append(
                f"{path}: zusaetzliche applySensorSelectionDecision-Signatur "
                "mit abweichendem Rueckgabetyp (z. B. RunCommandState) "
                "erkannt (Plan #21 Abschnitt 9.7 - keine Parallelfunktion)"
            )
        if len(canonical_matches) > 1:
            violations.append(
                f"{path}: applySensorSelectionDecision ist dort mehrfach "
                "deklariert/definiert (Plan #21 Abschnitt 9.7 - genau eine "
                "kanonische Signatur je Datei)"
            )
        elif len(canonical_matches) == 1:
            canonical_site_found[path] = True

    if not canonical_site_found[declaration_path]:
        violations.append(
            f"{declaration_path}: erwartete kanonische "
            "applySensorSelectionDecision-Deklaration fehlt (Plan #21 "
            "Abschnitt 9.7)"
        )
    if not canonical_site_found[definition_path]:
        violations.append(
            f"{definition_path}: erwartete kanonische "
            "applySensorSelectionDecision-Definition fehlt (Plan #21 "
            "Abschnitt 9.7)"
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
    add_sensor_selection_include_cycle_violations(violations, root)
    add_sensor_selection_canonical_function_violations(violations, root)

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
            'REQUIRES device_platform PRIV_REQUIRES esp_timer nvs_flash)\n'
        ),
        "main/app_main.cpp": '#include "device_platform.hpp"\n',
        "main/CMakeLists.txt": (
            'idf_component_register(SRCS "app_main.cpp" '
            'PRIV_INCLUDE_DIRS "../include" PRIV_REQUIRES '
            "device_platform fermentation_app device_platform_esp_idf)\n"
        ),
        # Issue #21, Plan Abschnitt 7/9.7: minimale, in sich saubere Instanz
        # der vier gegenseitig eingeschraenkten Header - Grundlage fuer die
        # SENSOR_SELECTION_INCLUDE_CYCLE_VIOLATION_CASES unten, die einzelne
        # Dateien durch eine verbotene Include-Variante ersetzen. hpp/.cpp
        # tragen zusaetzlich die kanonische applySensorSelectionDecision-
        # Deklaration/-Definition, damit die genau-eine-Signatur-Pruefung
        # (SENSOR_SELECTION_CANONICAL_*) an der sauberen Fixture nicht
        # faelschlich "fehlt" meldet.
        "lib/fermentation_app/src/sensor_selection_types.hpp": "#pragma once\n",
        "lib/fermentation_app/src/sensor_selection.hpp": (
            '#pragma once\n#include "sensor_selection_types.hpp"\n'
            "SensorSelectionStateMutation applySensorSelectionDecision(int x);\n"
        ),
        "lib/fermentation_app/src/sensor_selection.cpp": (
            '#include "sensor_selection.hpp"\n'
            "SensorSelectionStateMutation applySensorSelectionDecision(int x) "
            "{ return {}; }\n"
        ),
        "lib/fermentation_app/src/run_commands.hpp": (
            '#pragma once\n#include "sensor_selection_types.hpp"\n'
        ),
        "lib/fermentation_app/src/run_persistence_contract.hpp": (
            '#pragma once\n#include "run_commands.hpp"\n'
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


# Issue #21, Plan Abschnitt 7/9.7: jede der beiden verbotenen Kopplungen in
# beiden Richtungen - direkt fuer run_commands.hpp<->sensor_selection.hpp,
# und einmal transitiv (ueber einen Zwischenheader) fuer
# sensor_selection.hpp<->run_persistence_contract.hpp, damit der Guard nicht
# nur direkte Includes erkennt.
SENSOR_SELECTION_INCLUDE_CYCLE_VIOLATION_CASES = {
    "run_commands_includes_sensor_selection": (
        "lib/fermentation_app/src/run_commands.hpp",
        '#pragma once\n#include "sensor_selection.hpp"\n',
    ),
    "sensor_selection_includes_run_commands": (
        "lib/fermentation_app/src/sensor_selection.hpp",
        '#pragma once\n#include "sensor_selection_types.hpp"\n'
        '#include "run_commands.hpp"\n',
    ),
    "sensor_selection_includes_run_persistence_contract": (
        "lib/fermentation_app/src/sensor_selection.hpp",
        '#pragma once\n#include "sensor_selection_types.hpp"\n'
        '#include "run_persistence_contract.hpp"\n',
    ),
    "run_persistence_contract_includes_sensor_selection": (
        "lib/fermentation_app/src/run_persistence_contract.hpp",
        '#pragma once\n#include "run_commands.hpp"\n'
        '#include "sensor_selection.hpp"\n',
    ),
}

# Issue #21, Advisor-Review zu Commit 6: der Guard darf bei einem fehlenden
# Vertragsheader nicht still ueberspringen (fail-open), sondern muss das
# Fehlen selbst als Befund melden. Jeder der vier Header wird einzeln
# entfernt.
SENSOR_SELECTION_MISSING_HEADER_VIOLATION_CASES = tuple(
    f"lib/fermentation_app/src/{header}" for header in SENSOR_SELECTION_GUARDED_HEADERS
)

SENSOR_SELECTION_CANONICAL_VIOLATION_CASES = {
    "parallel_apply_sensor_selection_decision_signature": (
        "lib/fermentation_app/src/rogue_sensor_selection.cpp",
        "SensorSelectionStateMutation applySensorSelectionDecision(int x) "
        "{ return {}; }\n",
    ),
    # PR-#99-Abschlussreview-Korrektur: eine zweite gleichnamige Signatur
    # innerhalb von sensor_selection.hpp/.cpp selbst muss ebenso erkannt
    # werden wie eine externe Parallelfunktion.
    "duplicate_declaration_inside_header": (
        "lib/fermentation_app/src/sensor_selection.hpp",
        '#pragma once\n#include "sensor_selection_types.hpp"\n'
        "SensorSelectionStateMutation applySensorSelectionDecision(int x);\n"
        "SensorSelectionStateMutation applySensorSelectionDecision(int y);\n",
    ),
    "duplicate_definition_inside_source": (
        "lib/fermentation_app/src/sensor_selection.cpp",
        '#include "sensor_selection.hpp"\n'
        "SensorSelectionStateMutation applySensorSelectionDecision(int x) "
        "{ return {}; }\n"
        "SensorSelectionStateMutation applySensorSelectionDecision(int y) "
        "{ return {}; }\n",
    ),
    # Ein abweichender Rueckgabetyp (z. B. der vollstaendige
    # RunCommandState statt der schmalen SensorSelectionStateMutation) ist
    # ebenfalls eine verbotene Parallelfunktion - sowohl innerhalb der
    # beiden kanonischen Dateien als auch ausserhalb.
    "run_command_state_return_type_inside_header": (
        "lib/fermentation_app/src/sensor_selection.hpp",
        '#pragma once\n#include "sensor_selection_types.hpp"\n'
        "SensorSelectionStateMutation applySensorSelectionDecision(int x);\n"
        "RunCommandState applySensorSelectionDecision(int y);\n",
    ),
    "run_command_state_return_type_external_file": (
        "lib/fermentation_app/src/rogue_run_command_state.cpp",
        "RunCommandState applySensorSelectionDecision(int x) { return {}; }\n",
    ),
}

# Ein reiner Aufruf traegt kein Rueckgabetyp-Praefix und darf den Guard nicht
# faelschlich ausloesen.
SENSOR_SELECTION_CANONICAL_CLEAN_CASES = {
    "plain_call_is_not_a_parallel_signature": (
        "lib/fermentation_app/src/caller.cpp",
        "void f() { auto m = applySensorSelectionDecision(view, decision, now); }\n",
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
    "runtime_apply_command_multiline": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { applyRunCommand\n    (state, decision); }\n",
    ),
    "runtime_apply_transition_multiline": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { applyProcessTransition\n    (state, decision, snapshot); }\n",
    ),
    "runtime_apply_command_parenthesized": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { (applyRunCommand)(state, decision); }\n",
    ),
    "runtime_apply_transition_parenthesized": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { (applyProcessTransition)(state, decision, snapshot); }\n",
    ),
    "runtime_apply_command_function_pointer": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { auto apply = &applyRunCommand; apply(state, decision); }\n",
    ),
    "runtime_apply_transition_function_pointer": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { auto apply = &applyProcessTransition; "
        "apply(state, decision, snapshot); }\n",
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
    "runtime_effects_multiline_member": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { auto result = decideRun(); consume(result\n"
        "    .effects); }\n",
    ),
    "runtime_messages_parenthesized_member": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { auto result = decideTransition(); consume((result)\n"
        "    .messages); }\n",
    ),
    "runtime_messages_multiline_pointer_member": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { auto result = decideTransition(); consume(result\n"
        "    ->messages); }\n",
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
    "outer_decision_used_in_nested_block": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n"
        "    auto result = decideRun();\n"
        "    if (ready) {\n"
        "        consume(result.effects);\n"
        "    }\n"
        "}\n",
    ),
    "outer_apply_alias_used_in_nested_block": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n"
        "    auto apply = &applyRunCommand;\n"
        "    if (ready) {\n"
        "        apply(state, decision);\n"
        "    }\n"
        "}\n",
    ),
    "outer_decision_used_in_multi_level_nested_block": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n"
        "    auto result = decideRun();\n"
        "    if (a) {\n"
        "        if (b) {\n"
        "            consume(result.effects);\n"
        "        }\n"
        "    }\n"
        "}\n",
    ),
    "typed_decision_without_decide_call_is_still_a_bypass": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n"
        "    CommandDecision pending;\n"
        "    if (ready) {\n"
        "        consume(pending.effects);\n"
        "    }\n"
        "}\n",
    ),
    "use_before_later_shadow_is_a_bypass": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n"
        "    auto result = decideRun();\n"
        "    if (ready) {\n"
        "        consume(result.effects);\n"
        "        RenderResult result{};\n"
        "    }\n"
        "}\n",
    ),
    "typed_command_decision_parameter_is_a_bypass": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void publish(const CommandDecision& pending) {\n"
        "    consume(pending.effects);\n"
        "}\n",
    ),
    "typed_transition_decision_pointer_parameter_is_a_bypass": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void publish(const TransitionDecision* pending) {\n"
        "    consume(pending->messages);\n"
        "}\n",
    ),
    "apply_alias_without_ampersand_is_a_bypass": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n"
        "    auto apply = applyRunCommand;\n"
        "    apply(state, decision);\n"
        "}\n",
    ),
    "qualified_const_method_command_parameter": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void Controller::publish(const CommandDecision& pending) const {\n"
        "    consume(pending.effects);\n"
        "}\n",
    ),
    "qualified_noexcept_method_transition_parameter": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void Controller::publish(\n"
        "    const TransitionDecision* pending) noexcept {\n"
        "    consume(pending->messages);\n"
        "}\n",
    ),
    "namespaced_command_decision_parameter": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void publish(const fermentation::CommandDecision& pending) {\n"
        "    consume(pending.effects);\n"
        "}\n",
    ),
    "namespaced_apply_alias_without_ampersand": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n"
        "    auto apply = fermentation::applyRunCommand;\n"
        "    apply(state, decision);\n"
        "}\n",
    ),
    "namespaced_apply_alias_with_ampersand": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n"
        "    auto apply = &fermentation::applyProcessTransition;\n"
        "    apply(state, decision, snapshot);\n"
        "}\n",
    ),
    "const_auto_reference_decide_result": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n"
        "    const auto& result = decideRun();\n"
        "    consume(result.effects);\n"
        "}\n",
    ),
    "namespaced_decide_result": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n"
        "    auto result = fermentation::decideTransition();\n"
        "    consume(result.messages);\n"
        "}\n",
    ),
    "trailing_unrelated_parameter_does_not_hide_decision_parameter": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void publish(const CommandDecision& pending, int retries) {\n"
        "    consume(pending.effects);\n"
        "}\n",
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
    "shadowed_result_name_is_not_a_bypass": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n"
        "  auto result = decideRun();\n"
        "  { RenderResult result{}; use(result.effects); }\n"
        "}\n",
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
    "comments_and_strings_are_not_code": (
        "lib/fermentation_app/src/runtime_path.cpp",
        '// applyRunCommand(state, decision);\n'
        'const char* text = "result.effects applyProcessTransition(";\n',
    ),
    "gateway_call_is_the_allowed_route": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() { RunMutationGate gate; gate.route(state, decision); }\n",
    ),
    "multi_level_nested_shadow_is_not_a_bypass": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n"
        "  auto result = decideRun();\n"
        "  if (a) {\n"
        "    if (b) {\n"
        "      RenderResult result{};\n"
        "      use(result.effects);\n"
        "    }\n"
        "  }\n"
        "}\n",
    ),
    "unrelated_type_named_decision_is_not_a_bypass": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void f() {\n"
        "  DisplayState decision{};\n"
        "  if (ready) {\n"
        "    use(decision.effects);\n"
        "  }\n"
        "}\n",
    ),
    "unrelated_parameter_named_decision_is_clean": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void render(const DisplayState& decision) {\n"
        "    consume(decision.effects);\n"
        "}\n",
    ),
    "qualified_const_method_unrelated_decision_parameter": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void Controller::render(const DisplayState& decision) const {\n"
        "    consume(decision.effects);\n"
        "}\n",
    ),
    "qualified_noexcept_method_unrelated_messages_parameter": (
        "lib/fermentation_app/src/runtime_path.cpp",
        "void Controller::render(\n"
        "    const DisplayMessages* transitionDecision) noexcept {\n"
        "    consume(transitionDecision->messages);\n"
        "}\n",
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


def _check_clean_fixture_without_file(relative_path: str) -> list[str]:
    """Fuer den #21-Header-Fehlt-Fall: entfernt statt ergaenzt eine Datei aus
    der ansonsten sauberen Fixture."""
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        create_clean_fixture(root)
        (root / relative_path).unlink()
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

    for name, (relative_path, content) in (
        SENSOR_SELECTION_INCLUDE_CYCLE_VIOLATION_CASES.items()
    ):
        if not _check_clean_fixture_with_extra_file(relative_path, content):
            print(
                f"{FAILED}: #21-Include-Zyklus-Verstossfall {name!r} wurde "
                "nicht erkannt"
            )
            return 1

    for relative_path in SENSOR_SELECTION_MISSING_HEADER_VIOLATION_CASES:
        if not _check_clean_fixture_without_file(relative_path):
            print(
                f"{FAILED}: fehlender #21-Vertragsheader {relative_path!r} "
                "wurde nicht erkannt (Guard darf nicht fail-open sein)"
            )
            return 1

    for name, (relative_path, content) in SENSOR_SELECTION_CANONICAL_VIOLATION_CASES.items():
        if not _check_clean_fixture_with_extra_file(relative_path, content):
            print(
                f"{FAILED}: #21-Parallelfunktionsfall {name!r} wurde nicht erkannt"
            )
            return 1

    for name, (relative_path, content) in SENSOR_SELECTION_CANONICAL_CLEAN_CASES.items():
        if _check_clean_fixture_with_extra_file(relative_path, content):
            print(
                f"{FAILED}: sauberer applySensorSelectionDecision-Aufruf "
                f"{name!r} wurde faelschlich als Parallelfunktion erkannt"
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
