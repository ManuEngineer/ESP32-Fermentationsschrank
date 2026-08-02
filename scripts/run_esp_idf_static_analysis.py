#!/usr/bin/env python3
"""Kanonischer ESP-IDF-`esp-clang`-Analysetreiber (Issue #74, Commit 4).

Fuehrt die offizielle `idf.py clang-check`-Static-Analysis (Espressif
`esp-clang`) fuer `main/app_main.cpp` und
`lib/device_platform_esp_idf/src/esp_timer_time_source.cpp` getrennt fuer
beide Profile aus, siehe docs/tasks/issue-74-implementation-plan.md,
Abschnitt 7.8.1-7.8.7. Kein generisches Toolchain-Framework - ein
projekteigenes Skript fuer genau diesen einen Analysepfad.

Verbindliche Eigenschaften (Plan Abschnitt 7.8.3/7.8.4):

- strikte Trennung von Produktions- (`build/<profil>/`) und
  Analysebuild (`build/clang_tidy/<profil>/`); der Produktionsbuildtreiber
  `scripts/build_esp_idf_profiles.py` wird von diesem Skript weder
  aufgerufen noch veraendert;
- exakte, mehrsignalige Werkzeugverifikation (Pfad + Version + `tools.json`
  fuer `esp-clang`, exakte Version fuer `pyclang`) vor jedem Analyselauf,
  fail-closed statt Log-only;
- unabhaengiger Dateiauswahlnachweis (`re.escape()`-verankerte PATTERNS-
  Regex, angewendet auf die vollstaendige, ungefilterte
  `compile_commands.json` aus einem eigenen `reconfigure`-Schritt) *vor*
  dem eigentlichen `clang-check`-Aufruf - es existiert nachweislich keine
  auf Platte per PATTERNS gefilterte Datei (Abschnitt 7.8.1 Detailbefund 4);
- `warnings.txt` wird auch bei einem fehlgeschlagenen Analyselauf profil-
  getrennt gesichert, bevor der Exitcode ausgewertet wird.
"""

from __future__ import annotations

import argparse
import contextlib
import importlib.metadata
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import check_build_profiles
import esp_idf_contract

TARGET_SOURCES = (
    "main/app_main.cpp",
    "lib/device_platform_esp_idf/src/esp_timer_time_source.cpp",
)

HEADER_FILTER = "^(include|lib|main)/.*"
MISC_HEADER_INCLUDE_CYCLE_EXCLUSION = "-misc-header-include-cycle"

KNOWN_SYSTEM_CLANG_TIDY = "/usr/bin/clang-tidy-18"

WARNINGS_FILENAME = "warnings.txt"


class AnalysisError(RuntimeError):
    def __init__(self, profile: str, phase: str, detail: str) -> None:
        self.profile = profile
        self.phase = phase
        self.detail = detail
        super().__init__(f"[{profile}:{phase}] {detail}")


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def analysis_dir(root: Path, profile: str) -> Path:
    """Strikt vom Produktionsbuild getrenntes Analyseverzeichnis
    (Plan Abschnitt 7.8.3): `build/clang_tidy/<profil>`, niemals
    `build/<profil>` (das produktive Buildverzeichnis)."""
    return root / "build" / "clang_tidy" / esp_idf_contract.build_dir_name(profile)


def run(command: list[str], *, cwd: Path | None = None, env: dict[str, str] | None = None
        ) -> subprocess.CompletedProcess:
    return subprocess.run(command, cwd=cwd, env=env, capture_output=True, text=True)


def _clang_toolchain_env() -> dict[str, str]:
    """`IDF_TOOLCHAIN=clang` muss bereits vor dem ersten Configure-/
    Clang-Check-Aufruf gesetzt sein (Plan Abschnitt 7.8.6, Fall 4)."""
    env = dict(os.environ)
    env["IDF_TOOLCHAIN"] = "clang"
    return env


# --- ESP-IDF-Herkunftspruefung (DRY: keine zweite Implementierung) --------

def verify_idf_environment() -> None:
    """Wiederverwendet `check_build_profiles.check_esp_idf_version()` fuer
    die Commit-/Tag-/Arbeitsbaumpruefung, analog zum bestehenden Muster in
    `scripts/build_esp_idf_profiles.py` (Plan Abschnitt 7.4/7.8.4)."""
    idf_path_value = os.environ.get("IDF_PATH")
    if not idf_path_value:
        raise AnalysisError(
            "-", "environment",
            "IDF_PATH ist nicht gesetzt; zuerst `. $IDF_PATH/export.sh` ausfuehren",
        )
    idf_path = Path(idf_path_value)
    if not idf_path.is_dir():
        raise AnalysisError(
            "-", "environment", f"IDF_PATH zeigt auf kein Verzeichnis: {idf_path}",
        )
    if shutil.which("idf.py") is None:
        raise AnalysisError(
            "-", "environment",
            "idf.py nicht auf PATH gefunden; zuerst `. $IDF_PATH/export.sh` ausfuehren",
        )

    origin_violations = check_build_profiles.check_esp_idf_version(idf_path)
    if origin_violations:
        raise AnalysisError("-", "environment", "; ".join(origin_violations))


# --- Werkzeugverifikation (Plan Abschnitt 7.8.4, Punkte 1-7) --------------

def expected_esp_clang_bin_dir() -> Path | None:
    idf_tools_path = os.environ.get("IDF_TOOLS_PATH")
    if not idf_tools_path:
        return None
    return (
        Path(idf_tools_path) / "tools" / "esp-clang" /
        esp_idf_contract.ESP_CLANG_TOOL_VERSION / "esp-clang" / "bin"
    )


def verify_tool_path(resolved: str | None, tool_name: str, profile: str) -> Path:
    """Punkte 1/2/6: exakter Pfad unter dem esp-clang-Toolverzeichnis;
    expliziter Fehlschlag bei einem Rueckfall auf das native
    Debian-`clang-tidy-18` statt eines reinen Teilstring-Vergleichs."""
    if resolved is None:
        raise AnalysisError(profile, "toolchain", f"{tool_name} nicht auf PATH gefunden")

    resolved_path = Path(resolved).resolve()
    if str(resolved_path) == KNOWN_SYSTEM_CLANG_TIDY:
        raise AnalysisError(
            profile, "toolchain",
            f"{tool_name} zeigt auf das native Debian-Werkzeug "
            f"{KNOWN_SYSTEM_CLANG_TIDY}; fuer den ESP-IDF-Analysepfad wird "
            "esp-clang benoetigt (Plan Abschnitt 7.8.4 Punkt 6)",
        )

    expected_dir = expected_esp_clang_bin_dir()
    if expected_dir is None:
        raise AnalysisError(
            profile, "toolchain",
            "IDF_TOOLS_PATH ist nicht gesetzt; kann esp-clang-Pfad nicht verifizieren",
        )
    if resolved_path.parent != expected_dir.resolve():
        raise AnalysisError(
            profile, "toolchain",
            f"{tool_name}-Pfad {resolved_path} liegt nicht unter dem erwarteten "
            f"esp-clang-Toolverzeichnis {expected_dir}",
        )
    return resolved_path


def verify_tool_version(
    tool_path: Path, expected_substring: str, tool_name: str, profile: str,
    *, run_fn=run,
) -> None:
    """Punkte 3/4: `clang --version` meldet den vollstaendigen
    Toolpaketnamen, `clang-tidy --version` nur die LLVM-Version - beide
    Werte werden getrennt und ausschliesslich gegen den jeweils real
    gelieferten String geprueft (Plan Abschnitt 7.8.1)."""
    result = run_fn([str(tool_path), "--version"])
    if result.returncode != 0 or expected_substring not in result.stdout:
        raise AnalysisError(
            profile, "toolchain",
            f"{tool_name} --version enthaelt nicht den erwarteten Wert "
            f"'{expected_substring}': {result.stdout.strip()} {result.stderr.strip()}",
        )


def _tools_json_path() -> Path:
    idf_path_value = os.environ.get("IDF_PATH")
    if not idf_path_value:
        raise RuntimeError("IDF_PATH ist nicht gesetzt")
    return Path(idf_path_value) / "tools" / "tools.json"


def verify_tools_json_data(data: dict, profile: str) -> None:
    """Punkt 5: `tools.json`-Abgleich (Version, Status, SHA-256) -
    unabhaengig von jeder Versionsausgabe eines bereits installierten
    Binaries (Plan Abschnitt 7.8.4 Punkt 5/8)."""
    tool_entry = next(
        (tool for tool in data.get("tools", []) if tool.get("name") == "esp-clang"), None,
    )
    if tool_entry is None:
        raise AnalysisError(profile, "toolchain", "kein esp-clang-Eintrag in tools.json gefunden")

    version_entry = next(
        (
            version for version in tool_entry.get("versions", [])
            if version.get("name") == esp_idf_contract.ESP_CLANG_TOOL_VERSION
        ),
        None,
    )
    if version_entry is None:
        raise AnalysisError(
            profile, "toolchain",
            f"tools.json enthaelt keine esp-clang-Version "
            f"{esp_idf_contract.ESP_CLANG_TOOL_VERSION}",
        )
    if version_entry.get("status") != "recommended":
        raise AnalysisError(
            profile, "toolchain",
            f"tools.json esp-clang-Status ist nicht 'recommended': "
            f"{version_entry.get('status')}",
        )
    linux_amd64 = version_entry.get("linux-amd64", {})
    if linux_amd64.get("sha256") != esp_idf_contract.ESP_CLANG_LINUX_AMD64_SHA256:
        raise AnalysisError(
            profile, "toolchain",
            "tools.json Linux-AMD64-SHA-256 fuer esp-clang weicht vom "
            "erwarteten Wert ab",
        )


def verify_tools_json(profile: str) -> None:
    tools_json_path = _tools_json_path()
    if not tools_json_path.is_file():
        raise AnalysisError(profile, "toolchain", f"tools.json nicht gefunden: {tools_json_path}")
    data = json.loads(tools_json_path.read_text(encoding="utf-8"))
    verify_tools_json_data(data, profile)


def _installed_pyclang_version() -> str | None:
    try:
        return importlib.metadata.version("pyclang")
    except importlib.metadata.PackageNotFoundError:
        return None


def verify_pyclang_version(profile: str, *, version_fn=_installed_pyclang_version) -> None:
    """Punkt 7: fail-closed statt Log-only (Plan Abschnitt 7.8.1)."""
    installed = version_fn()
    if installed is None:
        raise AnalysisError(profile, "toolchain", "pyclang ist nicht installiert")
    if installed != esp_idf_contract.PYCLANG_VERSION:
        raise AnalysisError(
            profile, "toolchain",
            f"pyclang-Version {installed} weicht von der erwarteten "
            f"{esp_idf_contract.PYCLANG_VERSION} ab",
        )


def verify_toolchain(
    profile: str,
    *,
    which_fn=shutil.which,
    run_fn=run,
    tools_json_data_fn=None,
    pyclang_version_fn=_installed_pyclang_version,
) -> None:
    """Kombiniert alle sieben Signale aus Plan Abschnitt 7.8.4 - kein
    einzelnes Signal allein ist hinreichend (Punkt 8's Begruendung)."""
    clang_path = verify_tool_path(which_fn("clang"), "clang", profile)
    clang_tidy_path = verify_tool_path(which_fn("clang-tidy"), "clang-tidy", profile)
    verify_tool_version(
        clang_path, esp_idf_contract.ESP_CLANG_TOOL_VERSION, "clang", profile, run_fn=run_fn,
    )
    verify_tool_version(
        clang_tidy_path, esp_idf_contract.ESP_CLANG_LLVM_VERSION, "clang-tidy", profile,
        run_fn=run_fn,
    )
    if tools_json_data_fn is None:
        verify_tools_json(profile)
    else:
        verify_tools_json_data(tools_json_data_fn(), profile)
    verify_pyclang_version(profile, version_fn=pyclang_version_fn)


# --- .clang-tidy als alleinige Konfigurationsquelle (Punkt 8) -------------

def build_run_clang_tidy_options() -> str:
    """Ausschliesslich der dokumentierte Header-Filter und die eine
    Befund-B-Ausnahme - keine zweite, `.clang-tidy` ueberschreibende
    Checkliste (Plan Abschnitt 7.8.2/7.8.4)."""
    return f'-header-filter="{HEADER_FILTER}" -checks="{MISC_HEADER_INCLUDE_CYCLE_EXCLUSION}"'


def verify_run_clang_tidy_options(options: str, profile: str) -> None:
    expected = build_run_clang_tidy_options()
    if options != expected:
        raise AnalysisError(
            profile, "configuration",
            f"--run-clang-tidy-options weicht vom alleinigen .clang-tidy-"
            f"Vertrag ab: '{options}' != '{expected}'",
        )


# --- Unabhaengiger Dateiauswahlnachweis (Plan Abschnitt 7.8.4) ------------

def build_patterns_regex() -> str:
    """`re.escape()`-verankerte Ein-Regex-PATTERNS (Plan Abschnitt 7.8.1
    Detailbefund 1/4): `(?:^|/)` am Anfang, `$` am Ende jedes Pfadsegments,
    aus den exakten repository-relativen Zielpfaden aufgebaut."""
    segments = [r"(?:^|/)" + re.escape(source) + r"$" for source in TARGET_SOURCES]
    return "|".join(segments)


def read_compile_commands(path: Path) -> list[dict]:
    if not path.is_file():
        raise RuntimeError(f"compile_commands.json nicht gefunden: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def matched_translation_units(entries: list[dict], patterns_regex: str) -> list[str]:
    """Repliziert exakt die Auswahllogik von `run-clang-tidy` (absoluter
    Pfad aus `directory`+`file`, `.search()`) - als eigene Berechnung,
    nicht durch Parsen von dessen Textausgabe (Plan Abschnitt 7.8.4 Punkt
    3). Gibt eine Liste zurueck (keine Menge!), damit doppelte
    Rohtreffer nicht stillschweigend zu einem einzigen Treffer
    zusammenfallen (Selftest-Fall 22)."""
    compiled = re.compile(patterns_regex)
    matches: list[str] = []
    for entry in entries:
        directory = entry.get("directory", "")
        file_field = entry.get("file", "")
        absolute_path = os.path.abspath(os.path.join(directory, file_field))
        if compiled.search(absolute_path):
            matches.append(absolute_path)
    return matches


def verify_file_selection(
    entries: list[dict], patterns_regex: str, root: Path, profile: str,
) -> set[str]:
    """Punkt 4: exakt zwei Treffer, je einer fuer die beiden erwarteten
    Dateien. Harter Fehler bei 0/1/3+/Duplikat/Pfad ausserhalb des Repos -
    unabhaengig von der spaeteren Textausgabe von `run-clang-tidy`."""
    matches = matched_translation_units(entries, patterns_regex)
    expected = {os.path.abspath(str(root / source)) for source in TARGET_SOURCES}

    if len(matches) == 0:
        raise AnalysisError(
            profile, "file-selection",
            "PATTERNS-Regex traf keine Datei (0 von erwarteten 2)",
        )
    if len(matches) == 1:
        raise AnalysisError(
            profile, "file-selection",
            f"PATTERNS-Regex traf nur eine Datei statt der erwarteten 2: {matches[0]}",
        )
    if len(matches) > 2:
        raise AnalysisError(
            profile, "file-selection",
            f"PATTERNS-Regex traf {len(matches)} Dateien statt der erwarteten 2: {matches}",
        )

    distinct = set(matches)
    if len(distinct) != 2:
        raise AnalysisError(
            profile, "file-selection",
            f"PATTERNS-Regex traf zweimal dieselbe Datei statt zweier "
            f"verschiedener Dateien: {matches}",
        )
    if distinct != expected:
        raise AnalysisError(
            profile, "file-selection",
            f"PATTERNS-Regex traf unerwartete Datei(en) ausserhalb des "
            f"Repository-Pfads: {sorted(distinct - expected)}; "
            f"fehlend: {sorted(expected - distinct)}",
        )
    return distinct


# --- Buildkommandos (Plan Abschnitt 7.8.3) --------------------------------

def build_reconfigure_command(profile: str, directory: Path) -> list[str]:
    sdkconfig_path = directory / "sdkconfig"
    overlay = esp_idf_contract.sdkconfig_overlay(profile)
    return [
        "idf.py",
        "-B", str(directory),
        f"-DSDKCONFIG={sdkconfig_path}",
        f"-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;{overlay}",
        "reconfigure",
    ]


def build_clang_check_command(profile: str, directory: Path, patterns_regex: str) -> list[str]:
    sdkconfig_path = directory / "sdkconfig"
    overlay = esp_idf_contract.sdkconfig_overlay(profile)
    return [
        "idf.py",
        "-B", str(directory),
        f"-DSDKCONFIG={sdkconfig_path}",
        f"-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;{overlay}",
        "clang-check", "--exit-code",
        "--run-clang-tidy-options", build_run_clang_tidy_options(),
        patterns_regex,
    ]


def run_reconfigure(profile: str, directory: Path, root: Path) -> None:
    """Eigener, unabhaengiger `reconfigure`-Schritt fuer den
    Dateiauswahlnachweis (Plan Abschnitt 7.8.4 Punkt 2) - erzeugt eine
    frische, vollstaendige `compile_commands.json`, *nicht* durch
    `pyclang`s Projektwurzel-Filter reduziert."""
    command = build_reconfigure_command(profile, directory)
    result = subprocess.run(command, cwd=root, env=_clang_toolchain_env())
    if result.returncode != 0:
        raise AnalysisError(
            profile, "reconfigure",
            f"idf.py reconfigure fehlgeschlagen (Exit-Code {result.returncode})",
        )


def run_clang_check(profile: str, directory: Path, root: Path, patterns_regex: str) -> int:
    command = build_clang_check_command(profile, directory, patterns_regex)
    result = subprocess.run(command, cwd=root, env=_clang_toolchain_env())
    return result.returncode


# --- Profilgetrennte warnings.txt (Plan Abschnitt 7.8.5) ------------------

def warnings_txt_source(root: Path) -> Path:
    """`idf.py clang-check` schreibt `warnings.txt` stets in das aktuelle
    Arbeitsverzeichnis, nicht in das `-B`-Analyseverzeichnis (live
    verifiziert, Plan Abschnitt 7.8.5)."""
    return root / WARNINGS_FILENAME


def warnings_txt_target(directory: Path) -> Path:
    return directory / WARNINGS_FILENAME


def remove_stale_warnings(directory: Path) -> None:
    """Vor jedem Profillauf: ein alter Zielnachweis wird entfernt, damit
    kein veralteter Stand stillschweigend akzeptiert wird (Selftest-Fall
    26)."""
    target = warnings_txt_target(directory)
    if target.exists():
        target.unlink()


def secure_warnings(root: Path, directory: Path, profile: str) -> None:
    """Sichert `warnings.txt` unabhaengig vom `clang-check`-Exitcode -
    Beweissicherung *vor* Fehlerauswertung (Plan Abschnitt 7.8.4/7.8.5)."""
    source = warnings_txt_source(root)
    if not source.is_file():
        raise AnalysisError(
            profile, "warnings-secure",
            f"warnings.txt wurde nicht im Arbeitsverzeichnis erzeugt: {source}",
        )
    directory.mkdir(parents=True, exist_ok=True)
    shutil.move(str(source), str(warnings_txt_target(directory)))


# --- Orchestrierung --------------------------------------------------------

def analyze_profile(
    profile: str,
    *,
    root: Path | None = None,
    directory: Path | None = None,
    verify_toolchain_fn=verify_toolchain,
    reconfigure_fn=run_reconfigure,
    read_compile_commands_fn=read_compile_commands,
    run_clang_check_fn=run_clang_check,
    secure_warnings_fn=secure_warnings,
) -> None:
    root = root if root is not None else repo_root()
    directory = directory if directory is not None else analysis_dir(root, profile)

    verify_toolchain_fn(profile)
    remove_stale_warnings(directory)

    reconfigure_fn(profile, directory, root)

    entries = read_compile_commands_fn(directory / "compile_commands.json")
    patterns_regex = build_patterns_regex()
    verify_file_selection(entries, patterns_regex, root, profile)

    options = build_run_clang_tidy_options()
    verify_run_clang_tidy_options(options, profile)

    exit_code = run_clang_check_fn(profile, directory, root, patterns_regex)

    # Beweissicherung vor Fehlerauswertung (finally-aehnlich): ein
    # fehlgeschlagener Lauf darf den Nachweis nicht verschlucken.
    secure_warnings_fn(root, directory, profile)

    if exit_code != 0:
        raise AnalysisError(
            profile, "clang-check",
            f"idf.py clang-check meldete Funde (Exit-Code {exit_code}); "
            f"siehe {warnings_txt_target(directory)}",
        )


def orchestrate(profiles: list[str], analyze_fn=analyze_profile) -> None:
    for profile in profiles:
        build_dir_name = esp_idf_contract.build_dir_name(profile)
        print(f"=== ESP-IDF-Static-Analysis (esp-clang) fuer Profil {build_dir_name} ===")
        analyze_fn(profile)
        print(f"PASS: ESP-IDF {build_dir_name} static analysis with official esp-clang")
    print("PASS: ESP-IDF-Static-Analysis (esp-clang) fuer alle angeforderten Profile bestanden.")


# --- Selftest --------------------------------------------------------------

@contextlib.contextmanager
def _temp_env(**overrides: str):
    original = {key: os.environ.get(key) for key in overrides}
    os.environ.update(overrides)
    try:
        yield
    finally:
        for key, value in original.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value


def _raises_analysis_error(fn) -> bool:
    try:
        fn()
    except AnalysisError:
        return True
    return False


def _does_not_raise(fn) -> bool:
    try:
        fn()
    except AnalysisError:
        return False
    return True


def _fake_completed(returncode: int, stdout: str = "", stderr: str = "") -> subprocess.CompletedProcess:
    return subprocess.CompletedProcess(args=[], returncode=returncode, stdout=stdout, stderr=stderr)


def _good_tools_json_data() -> dict:
    return {
        "tools": [
            {
                "name": "esp-clang",
                "install": "on_request",
                "versions": [
                    {
                        "name": esp_idf_contract.ESP_CLANG_TOOL_VERSION,
                        "status": "recommended",
                        "linux-amd64": {
                            "sha256": esp_idf_contract.ESP_CLANG_LINUX_AMD64_SHA256,
                            "size": 339870496,
                        },
                    },
                ],
            },
        ],
    }


def _synthetic_compile_commands_entries(
    root: Path,
    *,
    targets: tuple[str, ...] = TARGET_SOURCES,
    duplicate_first_target: bool = False,
    foreign_lookalike_for: str | None = None,
) -> list[dict]:
    """Synthetische, vollstaendige (ungefilterte) `compile_commands.json`-
    Fixture mit ESP-IDF-internen und projekteigenen Eintraegen, analog zur
    real verifizierten 456-Eintraege-Datei (Plan Abschnitt 7.8.1
    Detailbefund 4)."""
    entries: list[dict] = []

    esp_idf_internal_files = [
        "/opt/esp-idf/components/freertos/freertos.c",
        "/opt/esp-idf/components/esp_system/esp_system.c",
        "/opt/esp-idf/components/log/log.c",
        "/opt/esp-idf/components/driver/gpio.c",
    ]
    for internal_file in esp_idf_internal_files:
        entries.append({"directory": "/", "file": internal_file, "command": "cc -c"})

    directory = str(root)
    for index, source in enumerate(targets):
        entries.append({"directory": directory, "file": source, "command": "cc -c"})
        if index == 0 and duplicate_first_target:
            entries.append({"directory": directory, "file": source, "command": "cc -c"})

    if foreign_lookalike_for is not None:
        entries.append({
            "directory": "/some/other/checkout",
            "file": foreign_lookalike_for,
            "command": "cc -c",
        })

    for index in range(20):
        entries.append({
            "directory": directory,
            "file": f"lib/fake_module_{index}/src/fake_{index}.cpp",
            "command": "cc -c",
        })

    return entries


def run_selftest() -> int:
    checks: list[tuple[str, bool]] = []
    root = Path("/fake/repo")

    # Faelle 1-3: getrennte Analyseverzeichnisse, sdkconfig-Pfade, Overlays.
    dir_bringup = analysis_dir(root, "bringup")
    dir_release = analysis_dir(root, "release")
    checks.append((
        "Bring-up und Release verwenden verschiedene Analyse-Buildverzeichnisse",
        dir_bringup != dir_release,
    ))
    checks.append((
        "Beide Profile verwenden verschiedene generierte sdkconfig-Pfade",
        (dir_bringup / "sdkconfig") != (dir_release / "sdkconfig"),
    ))
    reconfigure_bringup = build_reconfigure_command("bringup", dir_bringup)
    reconfigure_release = build_reconfigure_command("release", dir_release)
    checks.append((
        "Bring-up verwendet ausschliesslich das Bring-up-Overlay, Release "
        "ausschliesslich das Release-Overlay",
        "sdkconfig.defaults.bringup" in "".join(reconfigure_bringup)
        and "sdkconfig.defaults.release" not in "".join(reconfigure_bringup)
        and "sdkconfig.defaults.release" in "".join(reconfigure_release)
        and "sdkconfig.defaults.bringup" not in "".join(reconfigure_release),
    ))

    # Fall 4: IDF_TOOLCHAIN=clang bereits vor dem ersten Aufruf gesetzt.
    checks.append((
        "IDF_TOOLCHAIN=clang ist bereits beim ersten Configure-/"
        "Clang-Check-Aufruf gesetzt",
        _clang_toolchain_env().get("IDF_TOOLCHAIN") == "clang",
    ))

    # Fall 5: beide Projektquellen in der zusammengesetzten PATTERNS-Regex.
    patterns_regex = build_patterns_regex()
    compiled_patterns = re.compile(patterns_regex)
    checks.append((
        "Beide festgelegten Projektquellen sind in der zusammengesetzten "
        "PATTERNS-Regex fuer beide Profile enthalten",
        all(compiled_patterns.search(f"/any/root/{source}") for source in TARGET_SOURCES),
    ))

    # Fall 6: nicht-Null-Ruckgabe von clang-check fuehrt zu hartem Fehler.
    def _fake_verify_ok(profile: str) -> None:
        return None

    def _fake_reconfigure_ok(profile: str, directory: Path, root_arg: Path) -> None:
        return None

    def _fake_read_good(path: Path) -> list[dict]:
        return _synthetic_compile_commands_entries(root)

    def _fake_secure_ok(root_arg: Path, directory: Path, profile: str) -> None:
        return None

    def _fake_clang_check_fails(profile: str, directory: Path, root_arg: Path, patterns: str) -> int:
        return 1

    diagnose_failure = _raises_analysis_error(lambda: analyze_profile(
        "bringup", root=root, directory=dir_bringup,
        verify_toolchain_fn=_fake_verify_ok,
        reconfigure_fn=_fake_reconfigure_ok,
        read_compile_commands_fn=_fake_read_good,
        run_clang_check_fn=_fake_clang_check_fails,
        secure_warnings_fn=_fake_secure_ok,
    ))
    checks.append((
        "Ein simulierter Diagnosefehler (nicht-Null-Ruckgabe des "
        "clang-check-Aufrufs) fuehrt zu einem harten Fehler des Treibers",
        diagnose_failure,
    ))

    # Fall 7: fehlendes/falsches esp-clang fuehrt vor jedem Analyselauf zu
    # einem harten Fehler (aggregiert - Details in Faellen 11-15).
    with _temp_env(IDF_TOOLS_PATH="/fake/tools"):
        missing_esp_clang = _raises_analysis_error(
            lambda: verify_toolchain("bringup", which_fn=lambda name: None),
        )
    checks.append((
        "Ein simuliertes fehlendes oder falsches esp-clang fuehrt zu einem "
        "harten Fehler, bevor ein Analyselauf versucht wird",
        missing_esp_clang,
    ))

    # Fall 8: produktive Verzeichnisse werden nie als -B-Ziel verwendet.
    production_dir_bringup = root / "build" / esp_idf_contract.build_dir_name("bringup")
    production_dir_release = root / "build" / esp_idf_contract.build_dir_name("release")
    checks.append((
        "Die produktiven Verzeichnisse build/esp32_bringup/build/esp32_release "
        "werden vom Treiber nie als -B-Ziel verwendet",
        dir_bringup != production_dir_bringup and dir_release != production_dir_release
        and str(production_dir_bringup) not in "".join(reconfigure_bringup)
        and str(production_dir_release) not in "".join(reconfigure_release),
    ))

    # Fall 9: build_esp_idf_profiles.py wird nicht aufgerufen/veraendert.
    # Ohne einen echten Import-Ausdruck kann dieses Modul es nicht
    # aufrufen; die blosse Nennung des Dateinamens in Docstrings/
    # Kommentaren (zur Dokumentation der Abgrenzung) ist keine
    # Abhaengigkeit und darf diesen Fall nicht faelschlich scheitern
    # lassen.
    own_source = Path(__file__).read_text(encoding="utf-8")
    no_import_statement = re.search(
        r"^\s*(import|from)\s+build_esp_idf_profiles\b", own_source, re.MULTILINE,
    ) is None
    checks.append((
        "scripts/build_esp_idf_profiles.py wird vom neuen Treiber nicht "
        "aufgerufen oder veraendert",
        no_import_statement,
    ))

    # Fall 10: fehlende profilspezifische warnings.txt nach dem Verschieben.
    with _temp_directory_pair() as (missing_root, missing_dir):
        missing_warnings = _raises_analysis_error(
            lambda: secure_warnings(missing_root, missing_dir, "bringup"),
        )
    checks.append((
        "Eine fehlende profilspezifische warnings.txt nach dem Verschieben "
        "wird nicht still akzeptiert",
        missing_warnings,
    ))

    # Faelle 11-15: granulare Werkzeugverifikation.
    with _temp_env(IDF_TOOLS_PATH="/fake/tools"):
        good_clang_path = str(expected_esp_clang_bin_dir() / "clang")
        good_clang_tidy_path = str(expected_esp_clang_bin_dir() / "clang-tidy")

        accepted_good_clang = _does_not_raise(
            lambda: verify_tool_path(good_clang_path, "clang", "bringup"),
        )
        rejected_bad_clang = _raises_analysis_error(
            lambda: verify_tool_path("/usr/bin/clang", "clang", "bringup"),
        )
    checks.append((
        "Ein simulierter korrekter clang-Pfad unter dem esp-clang-"
        "Toolverzeichnis mit passendem clang --version wird akzeptiert; "
        "ein Pfad ausserhalb dieses Verzeichnisses wird abgelehnt",
        accepted_good_clang and rejected_bad_clang,
    ))

    with _temp_env(IDF_TOOLS_PATH="/fake/tools"):
        accepted_good_clang_tidy = _does_not_raise(
            lambda: verify_tool_version(
                Path(good_clang_tidy_path),
                esp_idf_contract.ESP_CLANG_LLVM_VERSION,
                "clang-tidy", "bringup",
                run_fn=lambda cmd: _fake_completed(
                    0, stdout=f"Espressif LLVM version {esp_idf_contract.ESP_CLANG_LLVM_VERSION}\n",
                ),
            ),
        )
        rejected_wrong_llvm_version = _raises_analysis_error(
            lambda: verify_tool_version(
                Path(good_clang_tidy_path),
                esp_idf_contract.ESP_CLANG_LLVM_VERSION,
                "clang-tidy", "bringup",
                run_fn=lambda cmd: _fake_completed(0, stdout="Espressif LLVM version 19.0.0\n"),
            ),
        )
    checks.append((
        "Ein simulierter korrekter clang-tidy-Pfad mit clang-tidy --version, "
        "der exakt ESP_CLANG_LLVM_VERSION ohne Toolpaketnamen enthaelt, wird "
        "akzeptiert; abweichende LLVM-Version wird abgelehnt",
        accepted_good_clang_tidy and rejected_wrong_llvm_version,
    ))

    good_tools_json = _good_tools_json_data()
    bad_version_tools_json = json.loads(json.dumps(good_tools_json))
    bad_version_tools_json["tools"][0]["versions"][0]["name"] = "esp-19.9.9_20200101"
    bad_status_tools_json = json.loads(json.dumps(good_tools_json))
    bad_status_tools_json["tools"][0]["versions"][0]["status"] = "deprecated"
    bad_sha_tools_json = json.loads(json.dumps(good_tools_json))
    bad_sha_tools_json["tools"][0]["versions"][0]["linux-amd64"]["sha256"] = "0" * 64
    checks.append((
        "Eine simulierte tools.json-Fixture mit abweichender esp-clang-"
        "Version, abweichendem status oder abweichendem "
        "Linux-AMD64-sha256 fuehrt zu einem harten Fehler, auch wenn Pfad-/"
        "Versionspruefungen fuer sich genommen bestehen wuerden",
        _does_not_raise(lambda: verify_tools_json_data(good_tools_json, "bringup"))
        and _raises_analysis_error(lambda: verify_tools_json_data(bad_version_tools_json, "bringup"))
        and _raises_analysis_error(lambda: verify_tools_json_data(bad_status_tools_json, "bringup"))
        and _raises_analysis_error(lambda: verify_tools_json_data(bad_sha_tools_json, "bringup")),
    ))

    with _temp_env(IDF_TOOLS_PATH="/fake/tools"):
        rejected_known_system_clang_tidy = _raises_analysis_error(
            lambda: verify_tool_path(KNOWN_SYSTEM_CLANG_TIDY, "clang-tidy", "bringup"),
        )
    checks.append((
        "Ein aufgeloester clang-tidy-Pfad, der exakt /usr/bin/clang-tidy-18 "
        "entspricht, wird explizit als bekannter Fehlerfall erkannt und "
        "abgelehnt",
        rejected_known_system_clang_tidy,
    ))

    rejected_missing_tool_version_token = _raises_analysis_error(
        lambda: verify_tool_version(
            Path("/fake/tools/tools/esp-clang/esp-20.1.1_20250829/esp-clang/bin/clang"),
            esp_idf_contract.ESP_CLANG_TOOL_VERSION,
            "clang", "bringup",
            run_fn=lambda cmd: _fake_completed(0, stdout="Espressif clang version 20.1.1 (some other build)\n"),
        ),
    )
    checks.append((
        "Eine simulierte clang --version-Ausgabe ohne den exakten "
        "ESP_CLANG_TOOL_VERSION-String fuehrt zu einem harten Fehler",
        rejected_missing_tool_version_token,
    ))

    # Faelle 16-18: pyclang-Versionspruefung.
    checks.append((
        "Eine simulierte pyclang-Version exakt PYCLANG_VERSION wird akzeptiert",
        _does_not_raise(lambda: verify_pyclang_version(
            "bringup", version_fn=lambda: esp_idf_contract.PYCLANG_VERSION,
        )),
    ))
    checks.append((
        "Eine simulierte pyclang-Version ungleich PYCLANG_VERSION fuehrt zu "
        "einem harten Fehler, bevor clang-check aufgerufen wird",
        _raises_analysis_error(lambda: verify_pyclang_version(
            "bringup", version_fn=lambda: "0.6.9",
        )),
    ))
    checks.append((
        "Eine fehlende pyclang-Installation fuehrt zu einem harten Fehler "
        "mit klarer Fehlermeldung, nicht zu einer stillen Ausnahme",
        _raises_analysis_error(lambda: verify_pyclang_version(
            "bringup", version_fn=lambda: None,
        )),
    ))

    # Faelle 19-22: Dateiauswahlnachweis gegen vollstaendige Fixture.
    good_entries = _synthetic_compile_commands_entries(root)
    checks.append((
        "Die aus re.escape() aufgebaute, verankerte PATTERNS-Regex liefert "
        "gegen eine synthetische, vollstaendige, ungefilterte "
        "compile_commands.json-Fixture exakt zwei Treffer fuer die beiden "
        "erwarteten Dateien",
        _does_not_raise(lambda: verify_file_selection(good_entries, patterns_regex, root, "bringup")),
    ))
    missing_one_entries = _synthetic_compile_commands_entries(root, targets=(TARGET_SOURCES[0],))
    checks.append((
        "Dieselbe Fixture ohne einen der beiden erwarteten Dateipfade "
        "fuehrt zu einem harten Fehler",
        _raises_analysis_error(
            lambda: verify_file_selection(missing_one_entries, patterns_regex, root, "bringup"),
        ),
    ))
    lookalike_entries = _synthetic_compile_commands_entries(
        root, targets=(TARGET_SOURCES[0],), foreign_lookalike_for=TARGET_SOURCES[1],
    )
    checks.append((
        "Dieselbe Fixture mit einem zusaetzlichen, aehnlich benannten "
        "Lookalike-Pfad in einem fremden Wurzelverzeichnis fuehrt zu einem "
        "harten Fehler statt einer stillschweigenden Mehrfachauswahl",
        _raises_analysis_error(
            lambda: verify_file_selection(lookalike_entries, patterns_regex, root, "bringup"),
        ),
    ))
    duplicate_entries = _synthetic_compile_commands_entries(
        root, targets=(TARGET_SOURCES[0],), duplicate_first_target=True,
    )
    checks.append((
        "Eine simulierte doppelte Auflistung derselben Datei in der "
        "Fixture fuehrt zu einem harten Fehler (keine versteckte Dopplung "
        "als 'zwei Treffer' akzeptiert)",
        _raises_analysis_error(
            lambda: verify_file_selection(duplicate_entries, patterns_regex, root, "bringup"),
        ),
    ))

    # Fall 23: Aufrufreihenfolge (reconfigure vor Dateiauswahl/clang-check).
    call_order: list[str] = []

    def _order_verify(profile: str) -> None:
        call_order.append("verify_toolchain")

    def _order_reconfigure(profile: str, directory: Path, root_arg: Path) -> None:
        call_order.append("reconfigure")

    def _order_read(path: Path) -> list[dict]:
        call_order.append("read_compile_commands")
        return good_entries

    def _order_clang_check(profile: str, directory: Path, root_arg: Path, patterns: str) -> int:
        call_order.append("clang_check")
        return 0

    def _order_secure(root_arg: Path, directory: Path, profile: str) -> None:
        call_order.append("secure_warnings")

    analyze_profile(
        "bringup", root=root, directory=dir_bringup,
        verify_toolchain_fn=_order_verify,
        reconfigure_fn=_order_reconfigure,
        read_compile_commands_fn=_order_read,
        run_clang_check_fn=_order_clang_check,
        secure_warnings_fn=_order_secure,
    )
    checks.append((
        "Der Treiber ruft seinen eigenen reconfigure-Schritt vor dem Lesen "
        "der compile_commands.json und vor dem eigentlichen clang-check-"
        "Aufruf auf",
        call_order == [
            "verify_toolchain", "reconfigure", "read_compile_commands",
            "clang_check", "secure_warnings",
        ],
    ))

    # Fall 24: --run-clang-tidy-options enthaelt ausschliesslich die
    # dokumentierten Werte.
    options = build_run_clang_tidy_options()
    checks.append((
        "Die zusammengesetzten --run-clang-tidy-options enthalten fuer "
        "beide Profile ausschliesslich den dokumentierten -header-filter-"
        "Wert und die eine -checks=-misc-header-include-cycle-Ausnahme",
        HEADER_FILTER in options
        and MISC_HEADER_INCLUDE_CYCLE_EXCLUSION in options
        and _does_not_raise(lambda: verify_run_clang_tidy_options(options, "bringup"))
        and _raises_analysis_error(
            lambda: verify_run_clang_tidy_options(options + ' -checks="clang-analyzer-*"', "bringup"),
        ),
    ))

    # Fall 25: warnings.txt wird vor der Fehlerweitergabe gesichert.
    with _temp_directory_pair() as (fail_root, fail_dir):
        (fail_root / WARNINGS_FILENAME).write_text("simulierter Fund\n", encoding="utf-8")
        fail_root_entries = _synthetic_compile_commands_entries(fail_root)

        def _failing_clang_check(profile: str, directory: Path, root_arg: Path, patterns: str) -> int:
            return 1

        raised = _raises_analysis_error(lambda: analyze_profile(
            "bringup", root=fail_root, directory=fail_dir,
            verify_toolchain_fn=_fake_verify_ok,
            reconfigure_fn=_fake_reconfigure_ok,
            read_compile_commands_fn=lambda path: fail_root_entries,
            run_clang_check_fn=_failing_clang_check,
            secure_warnings_fn=secure_warnings,
        ))
        warnings_secured = warnings_txt_target(fail_dir).is_file()
    checks.append((
        "warnings.txt wird auch bei einem simulierten fehlgeschlagenen "
        "clang-check-Aufruf in den profilspezifischen Zielpfad verschoben, "
        "bevor der Treiber den Fehler weiterreicht",
        raised and warnings_secured,
    ))

    # Fall 26: veralteter warnings.txt-Zielnachweis wird entfernt.
    with _temp_directory_pair() as (_stale_root, stale_dir):
        stale_target = warnings_txt_target(stale_dir)
        stale_target.parent.mkdir(parents=True, exist_ok=True)
        stale_target.write_text("veralteter Stand\n", encoding="utf-8")
        remove_stale_warnings(stale_dir)
        checks.append((
            "Ein bereits vorhandener, veralteter warnings.txt-Zielnachweis "
            "wird vor einem neuen Profillauf entfernt und nicht als "
            "aktueller Nachweis stehen gelassen",
            not stale_target.exists(),
        ))

    all_passed = True
    for description, passed in checks:
        status = "PASS" if passed else "FAILED"
        print(f"{status}: {description}")
        all_passed = all_passed and passed
    return 0 if all_passed else 1


@contextlib.contextmanager
def _temp_directory_pair():
    """Liefert (root, analyseverzeichnis) unter einem gemeinsamen
    temporaeren Verzeichnis, fuer Selftests die echte Dateisystempfade
    benoetigen (warnings.txt-Handling)."""
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        directory = root / "build" / "clang_tidy" / "esp32_bringup"
        yield root, directory


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "profile", nargs="?", choices=[*esp_idf_contract.PROFILES, "all"], default=None,
        help="zu analysierendes Profil, oder 'all' fuer beide nacheinander",
    )
    parser.add_argument(
        "--selftest", action="store_true",
        help="Prueft die Analyse-/Verifikationslogik selbst, ohne echte "
             "ESP-IDF-/esp-clang-Installation.",
    )
    arguments = parser.parse_args()

    if arguments.selftest:
        return run_selftest()

    if arguments.profile is None:
        parser.error("profile ist erforderlich, ausser bei --selftest")

    profiles = (
        list(esp_idf_contract.PROFILES) if arguments.profile == "all" else [arguments.profile]
    )

    try:
        verify_idf_environment()
        orchestrate(profiles)
    except AnalysisError as error:
        print(f"FEHLGESCHLAGEN: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
