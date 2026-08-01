#!/usr/bin/env python3
"""Prueft den Profil- und Driftvertrag fuer beide ESP-IDF-Profile
(Issue #74, docs/tasks/issue-74-implementation-plan.md Abschnitt 7.5.2).

Aktuelle Phase: **Parallel-Migrationsvertrag** (Phase 1, Commit 1 bis 4).
Prueft sowohl die neuen, isolierten ESP-IDF-Buildpfade als auch den noch
unveraendert bestehenden Arduino-Pfad unter PlatformIO. Dieser
Parallelzustand ist zeitlich begrenzt; in Commit 5 wird dieselbe Datei auf
den finalen ESP-IDF-only-Vertrag umgestellt (Phase 2), sobald der
Arduino-Pfad entfernt wird. Kein dauerhafter Bypass, kein
Kommandozeilenschalter fuer den Parallelzustand.

Im normalen Repositorymodus ist die ESP-IDF-Herkunftspruefung
verpflichtend: der ESP-IDF-Pfad wird ueber `--idf-path`, sonst ueber die
Umgebungsvariable `IDF_PATH` aufgeloest; fehlen beide, bricht das Skript
mit einem harten Fehler ab. Nur `--selftest` laeuft ohne realen
ESP-IDF-Checkout. Die Profilisolationspruefung (reale Pfadgleichheit
zwischen den generierten `sdkconfig`-Dateien) laeuft immer, unabhaengig
davon, welches einzelne Profil gerade angefordert wurde — eine bereits
vorhandene, mit dem anderen Profil geteilte Datei waere sonst auch bei
einem Einzelprofil-Aufruf unentdeckt.

`check_esp_idf_version()` ist die einzige Implementierung der
ESP-IDF-Herkunftspruefung im Repository; `scripts/build_esp_idf_profiles.py`
importiert genau diese Funktion fuer seine fruehe Pruefung, statt sie
erneut zu implementieren (DRY).

`--selftest` prueft die Erkennung selbst anhand temporaerer Fixture-Dateien
und eines minimalen echten Git-Repositorys, ohne dass ein absichtlich
fehlerhafter Fall jemals in dieses Repository eingecheckt werden muss.
"""

import argparse
import configparser
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

import esp_idf_contract

EXPECTED_PLATFORMIO_ARDUINO_ENVS = {
    f"env:{esp_idf_contract.build_dir_name(profile)}": {
        "platform": "espressif32@7.0.1",
        "framework": "arduino",
        "board": "esp32dev",
    }
    for profile in esp_idf_contract.PROFILES
}

EXPECTED_PLATFORMIO_NATIVE_ENV = {
    "platform": "native",
    "test_framework": "unity",
}

ALLOWED_PLATFORMIO_SECTIONS = {"platformio", "profile", "env:native"} | set(
    EXPECTED_PLATFORMIO_ARDUINO_ENVS
)

EXPECTED_PLATFORMIO_DEFAULT_ENVS = {"native"} | {
    esp_idf_contract.build_dir_name(profile) for profile in esp_idf_contract.PROFILES
}

REQUIRED_SAFETY_DEFINES = {
    "APP_TARGET_FLASH_MB": "4",
    "APP_REQUIRE_PSRAM": "0",
    "APP_WEB_OTA_ENABLED": "0",
    "APP_REAL_ACTUATORS_ENABLED": "0",
}

# sdkconfig speichert String-Werte mit Anführungszeichen im Text selbst;
# read_sdkconfig() entfernt sie bewusst nicht (verlustfreie Rohwertpruefung).
EXPECTED_IDF_TARGET = '"esp32"'

# Exakte Namen und Praefixe, die im ESP-IDF-Build keinesfalls vorkommen
# duerfen. Eine reine Substring-Suche im gesamten Kommando wuerde auch
# harmlose Zufallstreffer melden; stattdessen wird jede strukturiert
# geparste Definition einzeln gegen diese Regeln geprueft.
FORBIDDEN_DEFINE_EXACT_NAMES = ("ARDUINO",)
FORBIDDEN_DEFINE_PREFIXES = ("ARDUINO_", "CONFIG_ARDUINO_")


def is_forbidden_arduino_define(name: str) -> bool:
    if name in FORBIDDEN_DEFINE_EXACT_NAMES:
        return True
    return any(name.startswith(prefix) for prefix in FORBIDDEN_DEFINE_PREFIXES)


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


# --- sdkconfig ---------------------------------------------------------

def read_sdkconfig(sdkconfig_path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in sdkconfig_path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        if line.startswith("# ") and line.endswith("is not set"):
            option = line[2:].split(" ", 1)[0]
            values[option] = "n"
            continue
        if line.startswith("#"):
            continue
        if "=" in line:
            option, value = line.split("=", 1)
            values[option] = value
    return values


def check_sdkconfig_for_profile(sdkconfig_path: Path, profile: str) -> list[str]:
    if not sdkconfig_path.is_file():
        return [f"generierte sdkconfig fehlt: {sdkconfig_path}"]

    values = read_sdkconfig(sdkconfig_path)
    own_option = esp_idf_contract.profile_kconfig_option(profile)
    other_option = esp_idf_contract.profile_kconfig_option(
        esp_idf_contract.other_profile(profile)
    )

    violations: list[str] = []
    if values.get(own_option) != "y":
        violations.append(
            f"{sdkconfig_path}: erwartete {own_option}=y, gefunden "
            f"{values.get(own_option, '(fehlt)')}"
        )
    if values.get(other_option) == "y":
        violations.append(
            f"{sdkconfig_path}: widerspruechliche Profilwahl, "
            f"{other_option}=y ist zusaetzlich aktiv"
        )
    if values.get("CONFIG_ESPTOOLPY_FLASHSIZE_4MB") != "y":
        violations.append(f"{sdkconfig_path}: 4-MB-Flashkonfiguration fehlt")
    if values.get("CONFIG_SPIRAM") == "y":
        violations.append(f"{sdkconfig_path}: PSRAM (CONFIG_SPIRAM) ist aktiviert")
    if values.get("CONFIG_IDF_TARGET") != EXPECTED_IDF_TARGET:
        violations.append(
            f"{sdkconfig_path}: CONFIG_IDF_TARGET erwartet {EXPECTED_IDF_TARGET}, "
            f"gefunden {values.get('CONFIG_IDF_TARGET', '(fehlt)')}"
        )
    return violations


# --- Strukturierte -D-Definitionsauswertung (gemeinsam fuer
# compile_commands.json und platformio.ini build_flags) -------------------

def defines_from_tokens(tokens: list[str]) -> list[tuple[str, str | None]]:
    """Zerlegt eine Tokenliste in (Name, Wert)-Paare fuer jede `-D`-Definition,
    in Auftrittsreihenfolge (damit Mehrfachdefinitionen erkennbar bleiben).
    Akzeptiert sowohl `-DNAME=VALUE` als auch die getrennte Zwei-Token-Form
    `-D NAME=VALUE`."""
    defines: list[tuple[str, str | None]] = []
    index = 0
    while index < len(tokens):
        token = tokens[index]
        if token == "-D" and index + 1 < len(tokens):
            body = tokens[index + 1]
            index += 2
        elif token.startswith("-D") and token != "-D":
            body = token[2:]
            index += 1
        else:
            index += 1
            continue
        if "=" in body:
            name, value = body.split("=", 1)
        else:
            name, value = body, None
        defines.append((name, value))
    return defines


def group_defines(defines: list[tuple[str, str | None]]) -> dict[str, list[str | None]]:
    grouped: dict[str, list[str | None]] = {}
    for name, value in defines:
        grouped.setdefault(name, []).append(value)
    return grouped


def parse_defines(entry: dict) -> list[tuple[str, str | None]]:
    """Zerlegt `command`/`arguments` eines compile_commands.json-Eintrags."""
    arguments = entry.get("arguments")
    tokens = list(arguments) if arguments else shlex.split(entry.get("command", ""))
    return defines_from_tokens(tokens)


def check_compile_definitions_for_profile(
    compile_commands_path: Path, profile: str
) -> list[str]:
    if not compile_commands_path.is_file():
        return [f"compile_commands.json fehlt: {compile_commands_path}"]

    try:
        entries = json.loads(compile_commands_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        return [f"{compile_commands_path}: kein gueltiges JSON ({error})"]

    app_main_entry = next(
        (entry for entry in entries if str(entry.get("file", "")).endswith("app_main.cpp")),
        None,
    )
    if app_main_entry is None:
        return [f"{compile_commands_path}: kein Eintrag fuer app_main.cpp"]

    grouped = group_defines(parse_defines(app_main_entry))
    violations: list[str] = []

    own_name = esp_idf_contract.profile_define(profile)
    other_name = esp_idf_contract.profile_define(esp_idf_contract.other_profile(profile))

    own_values = grouped.get(own_name, [])
    if own_values != ["1"]:
        violations.append(
            f"{compile_commands_path}: erwartete genau eine Definition -D{own_name}=1, "
            f"gefunden {own_values or '(fehlt)'}"
        )
    other_values = grouped.get(other_name, [])
    if other_values:
        violations.append(
            f"{compile_commands_path}: unerwartete Definition -D{other_name} "
            f"zusaetzlich vorhanden ({other_values})"
        )

    for name, expected in REQUIRED_SAFETY_DEFINES.items():
        values = grouped.get(name, [])
        if not values:
            violations.append(f"{compile_commands_path}: erwartete Definition -D{name} fehlt")
        elif len(values) > 1:
            violations.append(
                f"{compile_commands_path}: Definition -D{name} mehrfach vorhanden ({values})"
            )
        elif values[0] != expected:
            violations.append(
                f"{compile_commands_path}: -D{name} erwartet {expected}, gefunden {values[0]}"
            )

    for name in grouped:
        if is_forbidden_arduino_define(name):
            violations.append(
                f"{compile_commands_path}: verbotene Arduino-Definition -D{name} im "
                "ESP-IDF-Build vorhanden"
            )

    return violations


def check_esp_idf_profile(build_root: Path, profile: str) -> list[str]:
    profile_dir = build_root / esp_idf_contract.build_dir_name(profile)
    violations = check_sdkconfig_for_profile(profile_dir / "sdkconfig", profile)
    violations += check_compile_definitions_for_profile(
        profile_dir / "compile_commands.json", profile
    )
    return violations


# --- Profilisolation (reale Pfadgleichheit) ------------------------------

def check_profiles_are_isolated(build_root: Path) -> list[str]:
    bringup_sdkconfig = build_root / esp_idf_contract.build_dir_name("bringup") / "sdkconfig"
    release_sdkconfig = build_root / esp_idf_contract.build_dir_name("release") / "sdkconfig"
    if not (bringup_sdkconfig.is_file() and release_sdkconfig.is_file()):
        return []

    try:
        same_file = bringup_sdkconfig.samefile(release_sdkconfig)
    except OSError:
        same_file = False
    if same_file:
        return [
            "Bring-up- und Release-Profil verwenden tatsaechlich dieselbe generierte "
            f"sdkconfig-Datei ({bringup_sdkconfig} ist derselbe Pfad wie "
            f"{release_sdkconfig}, Symlink/Hardlink oder gemeinsamer Buildordner)"
        ]
    return []


# --- PlatformIO-Parallelvertrag ------------------------------------------

_PLATFORMIO_REFERENCE = re.compile(r"\$\{([\w:]+)\.([\w]+)\}")


def resolve_platformio_option(
    parser: configparser.ConfigParser, section: str, key: str, *, _seen: frozenset = frozenset()
) -> str:
    """Loest das in diesem Repository tatsaechlich verwendete
    PlatformIO-Interpolationsmuster `${section.key}` auf eigenen Zeilen auf
    (kein generischer Templating-Motor, nur dieses eine, konkrete Muster)."""
    if section not in parser or section in _seen:
        return ""
    raw = parser[section].get(key, "")
    resolved_lines: list[str] = []
    for line in raw.splitlines():
        match = _PLATFORMIO_REFERENCE.fullmatch(line.strip())
        if match:
            ref_section, ref_key = match.group(1), match.group(2)
            resolved_lines.append(
                resolve_platformio_option(parser, ref_section, ref_key, _seen=_seen | {section})
            )
        else:
            resolved_lines.append(line)
    return "\n".join(resolved_lines)


def defines_from_platformio_flags(
    parser: configparser.ConfigParser, section: str
) -> dict[str, list[str | None]]:
    resolved = resolve_platformio_option(parser, section, "build_flags")
    return group_defines(defines_from_tokens(shlex.split(resolved)))


def check_platformio_parallel_contract(platformio_ini_path: Path) -> list[str]:
    if not platformio_ini_path.is_file():
        return [f"platformio.ini fehlt: {platformio_ini_path}"]

    parser = configparser.ConfigParser(interpolation=None)
    parser.read(platformio_ini_path, encoding="utf-8")

    violations: list[str] = []

    unexpected_sections = set(parser.sections()) - ALLOWED_PLATFORMIO_SECTIONS
    if unexpected_sections:
        violations.append(
            "unerwartete zusaetzliche PlatformIO-Sektion(en): "
            + ", ".join(sorted(unexpected_sections))
        )

    if "platformio" not in parser:
        violations.append("PlatformIO-Sektion [platformio] fehlt")
    else:
        raw_default_envs = parser["platformio"].get("default_envs", "")
        actual_envs = {token.strip() for token in raw_default_envs.split(",") if token.strip()}
        if actual_envs != EXPECTED_PLATFORMIO_DEFAULT_ENVS:
            violations.append(
                f"[platformio] default_envs erwartet die Menge "
                f"{sorted(EXPECTED_PLATFORMIO_DEFAULT_ENVS)}, gefunden {sorted(actual_envs)}"
            )

    if "profile" not in parser:
        violations.append("PlatformIO-Sektion [profile] fehlt")
    else:
        profile_defines = defines_from_platformio_flags(parser, "profile")
        for name, expected in REQUIRED_SAFETY_DEFINES.items():
            values = profile_defines.get(name, [])
            if not values:
                violations.append(f"[profile] build_flags: erwartete Definition -D{name} fehlt")
            elif len(values) > 1:
                violations.append(
                    f"[profile] build_flags: Definition -D{name} mehrfach vorhanden ({values})"
                )
            elif values[0] != expected:
                violations.append(
                    f"[profile] build_flags: -D{name} erwartet {expected}, gefunden {values[0]}"
                )

    if "env:native" not in parser:
        violations.append("PlatformIO-Environment env:native fehlt")
    else:
        for key, expected in EXPECTED_PLATFORMIO_NATIVE_ENV.items():
            actual = parser["env:native"].get(key)
            if actual != expected:
                violations.append(
                    f"env:native: erwartete {key}={expected!r}, gefunden {actual!r}"
                )
        native_defines = defines_from_platformio_flags(parser, "env:native")
        native_values = native_defines.get("APP_PROFILE_NATIVE", [])
        if native_values != ["1"]:
            violations.append(
                f"env:native: erwartete genau eine Definition -DAPP_PROFILE_NATIVE=1, "
                f"gefunden {native_values or '(fehlt)'}"
            )
        for esp32_profile in esp_idf_contract.PROFILES:
            forbidden_name = esp_idf_contract.profile_define(esp32_profile)
            if forbidden_name in native_defines:
                violations.append(
                    f"env:native: unerwartete ESP32-Profildefinition -D{forbidden_name} "
                    "vorhanden"
                )

    for profile in esp_idf_contract.PROFILES:
        section = f"env:{esp_idf_contract.build_dir_name(profile)}"
        expected_options = EXPECTED_PLATFORMIO_ARDUINO_ENVS[section]
        if section not in parser:
            violations.append(
                f"Parallelphase verletzt: PlatformIO-Environment {section} fehlt "
                "(Arduino-Pfad darf vor Commit 5 nicht entfernt werden)"
            )
            continue
        for key, expected in expected_options.items():
            actual = parser[section].get(key)
            if actual != expected:
                violations.append(
                    f"{section}: erwartete {key}={expected!r}, gefunden {actual!r} "
                    "(Altvertrag waehrend Parallelphase muss unveraendert bleiben)"
                )

        env_defines = defines_from_platformio_flags(parser, section)
        own_name = esp_idf_contract.profile_define(profile)
        other_name = esp_idf_contract.profile_define(esp_idf_contract.other_profile(profile))
        own_values = env_defines.get(own_name, [])
        if own_values != ["1"]:
            violations.append(
                f"{section}: erwartete genau eine Definition -D{own_name}=1, "
                f"gefunden {own_values or '(fehlt)'}"
            )
        other_values = env_defines.get(other_name, [])
        if other_values:
            violations.append(
                f"{section}: unerwartete Definition -D{other_name} zusaetzlich "
                f"vorhanden ({other_values}) — Profile vertauscht?"
            )

    return violations


# --- ESP-IDF-Herkunft (einzige Implementierung im Repository) -------------

def check_esp_idf_version(
    idf_git_dir: Path,
    expected_tag: str = esp_idf_contract.ESP_IDF_TAG,
    expected_commit: str = esp_idf_contract.ESP_IDF_COMMIT,
) -> list[str]:
    if not idf_git_dir.is_dir():
        return [f"ESP-IDF-Checkout nicht gefunden: {idf_git_dir}"]

    commit_result = subprocess.run(
        ["git", "-C", str(idf_git_dir), "rev-parse", "HEAD"],
        capture_output=True, text=True,
    )
    if commit_result.returncode != 0:
        return [
            f"git rev-parse HEAD fehlgeschlagen in {idf_git_dir}: "
            f"{commit_result.stderr.strip()}"
        ]
    actual_commit = commit_result.stdout.strip()
    if actual_commit != expected_commit:
        return [
            f"falscher ESP-IDF-Commit: erwartet {expected_commit}, "
            f"gefunden {actual_commit}"
        ]

    tag_result = subprocess.run(
        ["git", "-C", str(idf_git_dir), "describe", "--tags", "--exact-match"],
        capture_output=True, text=True,
    )
    if tag_result.returncode != 0:
        return [
            f"kein exakter Git-Tag am ESP-IDF-Commit gefunden (erwartet "
            f"{expected_tag}): {tag_result.stderr.strip()}"
        ]
    actual_tag = tag_result.stdout.strip()
    if actual_tag != expected_tag:
        return [f"falscher ESP-IDF-Tag: erwartet {expected_tag}, gefunden {actual_tag}"]

    status_result = subprocess.run(
        ["git", "-C", str(idf_git_dir), "status", "--short"],
        capture_output=True, text=True,
    )
    if status_result.returncode != 0:
        return [
            f"git status fehlgeschlagen in {idf_git_dir}: {status_result.stderr.strip()}"
        ]
    if status_result.stdout.strip():
        return [f"ESP-IDF-Arbeitsbaum ist nicht sauber:\n{status_result.stdout}"]

    return []


# --- Realer Repositorymodus -----------------------------------------------

def check_repository(
    idf_path: str,
    profiles: list[str],
    *,
    build_root: Path | None = None,
    platformio_ini_path: Path | None = None,
    expected_tag: str = esp_idf_contract.ESP_IDF_TAG,
    expected_commit: str = esp_idf_contract.ESP_IDF_COMMIT,
) -> int:
    root = repo_root()
    build_root = build_root if build_root is not None else root / "build"
    platformio_ini_path = (
        platformio_ini_path if platformio_ini_path is not None else root / "platformio.ini"
    )

    violations: list[str] = []

    for profile in profiles:
        violations += check_esp_idf_profile(build_root, profile)

    # Immer, unabhaengig vom angeforderten Profilfilter: eine bereits
    # vorhandene, mit dem jeweils anderen Profil geteilte sdkconfig waere
    # sonst auch bei einem Einzelprofil-Aufruf unentdeckt (Abschnitt 2 des
    # Reviews).
    violations += check_profiles_are_isolated(build_root)

    violations += check_platformio_parallel_contract(platformio_ini_path)
    violations += check_esp_idf_version(
        Path(idf_path), expected_tag=expected_tag, expected_commit=expected_commit,
    )

    if violations:
        for violation in violations:
            print(f"FAILED: {violation}", file=sys.stderr)
        print(f"FAILED: {len(violations)} Verletzung(en) gefunden", file=sys.stderr)
        return 1

    profile_names = ", ".join(esp_idf_contract.build_dir_name(p) for p in profiles)
    print(f"PASS: {profile_names} und der PlatformIO-Parallelvertrag sind korrekt.")
    return 0


# --- Selftest-Fixtures ------------------------------------------------------

def _write_sdkconfig(
    path: Path,
    active_options: set[str],
    *,
    flash_ok: bool = True,
    idf_target: str | None = EXPECTED_IDF_TARGET,
) -> None:
    lines = []
    for profile in esp_idf_contract.PROFILES:
        option = esp_idf_contract.profile_kconfig_option(profile)
        if option in active_options:
            lines.append(f"{option}=y")
        else:
            lines.append(f"# {option} is not set")
    if flash_ok:
        lines.append("CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y")
    if idf_target is not None:
        lines.append(f"CONFIG_IDF_TARGET={idf_target}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _write_compile_commands(path: Path, defines: list[str], *, command: str | None = None) -> None:
    if command is None:
        command = "clang++ " + " ".join(f"-D{define}" for define in defines) + " app_main.cpp"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps([{"file": "main/app_main.cpp", "command": command}]),
        encoding="utf-8",
    )


def _good_defines(profile: str) -> list[str]:
    return [f"{esp_idf_contract.profile_define(profile)}=1"] + [
        f"{name}={value}" for name, value in REQUIRED_SAFETY_DEFINES.items()
    ]


def _make_git_repo(root: Path, *, tag: str | None, dirty: bool = False) -> str:
    """Erzeugt ein minimales echtes Git-Repository fuer Herkunfts-Selftests.
    Gibt den tatsaechlichen Commit-SHA des einzigen Commits zurueck."""

    def git(*args: str) -> subprocess.CompletedProcess:
        return subprocess.run(
            ["git", "-C", str(root), *args], capture_output=True, text=True, check=True,
        )

    root.mkdir(parents=True, exist_ok=True)
    git("init", "--quiet")
    git("config", "user.email", "selftest@example.invalid")
    git("config", "user.name", "Selftest")
    (root / "marker.txt").write_text("selftest fixture\n", encoding="utf-8")
    git("add", "marker.txt")
    git("commit", "--quiet", "-m", "selftest fixture commit")
    commit_sha = git("rev-parse", "HEAD").stdout.strip()
    if tag is not None:
        git("tag", tag)
    if dirty:
        (root / "marker.txt").write_text("selftest fixture, modified\n", encoding="utf-8")
    return commit_sha


def _make_platformio_ini(
    *,
    default_envs: str = "native, esp32_bringup, esp32_release",
    profile_build_flags: str = (
        "    -DAPP_TARGET_FLASH_MB=4\n"
        "    -DAPP_REQUIRE_PSRAM=0\n"
        "    -DAPP_WEB_OTA_ENABLED=0\n"
        "    -DAPP_REAL_ACTUATORS_ENABLED=0\n"
    ),
    native_platform: str = "native",
    native_extra_flags: str = "    -DAPP_PROFILE_NATIVE=1\n",
    bringup_platform: str = "espressif32@7.0.1",
    bringup_framework: str = "arduino",
    bringup_board: str = "esp32dev",
    bringup_extra_flags: str = "    -DAPP_PROFILE_ESP32_BRINGUP=1\n",
    release_platform: str = "espressif32@7.0.1",
    release_framework: str = "arduino",
    release_board: str = "esp32dev",
    release_extra_flags: str = "    -DAPP_PROFILE_ESP32_RELEASE=1\n",
    include_native: bool = True,
    include_bringup: bool = True,
    include_release: bool = True,
    extra_sections: str = "",
) -> str:
    """Baut ein minimales, aber strukturell reales platformio.ini-Fixture
    (mit derselben `${profile.build_flags}`-Interpolation wie das
    tatsaechliche Repository) mit gezielt ueberschreibbaren Feldern, statt
    fragiler Textersetzung an einer festen Konstante."""
    sections = [f"[platformio]\ndefault_envs = {default_envs}\n"]
    sections.append(f"[profile]\nbuild_flags =\n    -std=gnu++17\n{profile_build_flags}")
    if include_native:
        sections.append(
            f"[env:native]\nplatform = {native_platform}\ntest_framework = unity\n"
            f"build_flags =\n    ${{profile.build_flags}}\n{native_extra_flags}"
        )
    if include_bringup:
        sections.append(
            f"[env:esp32_bringup]\nplatform = {bringup_platform}\n"
            f"framework = {bringup_framework}\nboard = {bringup_board}\n"
            f"build_flags =\n    ${{profile.build_flags}}\n{bringup_extra_flags}"
        )
    if include_release:
        sections.append(
            f"[env:esp32_release]\nplatform = {release_platform}\n"
            f"framework = {release_framework}\nboard = {release_board}\n"
            f"build_flags =\n    ${{profile.build_flags}}\n{release_extra_flags}"
        )
    if extra_sections:
        sections.append(extra_sections)
    return "\n".join(sections)


def run_selftest() -> int:
    checks: list[tuple[str, bool]] = []

    # --- ESP-IDF-Profile: getrennte, korrekte sdkconfig + compile_commands ---
    with tempfile.TemporaryDirectory() as tmp:
        build_root = Path(tmp) / "build"
        for profile in esp_idf_contract.PROFILES:
            _write_sdkconfig(
                build_root / esp_idf_contract.build_dir_name(profile) / "sdkconfig",
                {esp_idf_contract.profile_kconfig_option(profile)},
            )
            _write_compile_commands(
                build_root / esp_idf_contract.build_dir_name(profile) / "compile_commands.json",
                _good_defines(profile),
            )
        violations = (
            check_esp_idf_profile(build_root, "bringup")
            + check_esp_idf_profile(build_root, "release")
            + check_profiles_are_isolated(build_root)
        )
        checks.append(("Getrennte, korrekte ESP-IDF-Profile werden akzeptiert", not violations))

    with tempfile.TemporaryDirectory() as tmp:
        build_root = Path(tmp) / "build"
        release_sdkconfig = build_root / esp_idf_contract.build_dir_name("release") / "sdkconfig"
        _write_sdkconfig(release_sdkconfig, {esp_idf_contract.profile_kconfig_option("bringup")})
        violations = check_sdkconfig_for_profile(release_sdkconfig, "release")
        checks.append(("Release-Build mit faelschlich aktivem Bring-up-Profil wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        build_root = Path(tmp) / "build"
        bringup_sdkconfig = build_root / esp_idf_contract.build_dir_name("bringup") / "sdkconfig"
        _write_sdkconfig(bringup_sdkconfig, {esp_idf_contract.profile_kconfig_option("release")})
        violations = check_sdkconfig_for_profile(bringup_sdkconfig, "bringup")
        checks.append(("Bring-up-Build mit faelschlich aktivem Release-Profil wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        build_root = Path(tmp) / "build"
        real_path = build_root / esp_idf_contract.build_dir_name("bringup") / "sdkconfig"
        _write_sdkconfig(real_path, {esp_idf_contract.profile_kconfig_option("bringup")})
        release_dir = build_root / esp_idf_contract.build_dir_name("release")
        release_dir.mkdir(parents=True, exist_ok=True)
        (release_dir / "sdkconfig").symlink_to(real_path)
        violations = check_profiles_are_isolated(build_root)
        checks.append(("Tatsaechlich gemeinsam genutzte sdkconfig (Symlink) wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        build_root = Path(tmp) / "build"
        violations = check_esp_idf_profile(build_root, "bringup")
        checks.append(("Fehlende sdkconfig wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        build_root = Path(tmp) / "build"
        _write_sdkconfig(
            build_root / esp_idf_contract.build_dir_name("bringup") / "sdkconfig",
            {esp_idf_contract.profile_kconfig_option("bringup")},
        )
        violations = check_compile_definitions_for_profile(
            build_root / esp_idf_contract.build_dir_name("bringup") / "compile_commands.json",
            "bringup",
        )
        checks.append(("Fehlende compile_commands.json wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        build_root = Path(tmp) / "build"
        sdkconfig_path = build_root / esp_idf_contract.build_dir_name("bringup") / "sdkconfig"
        _write_sdkconfig(sdkconfig_path, set())
        violations = check_sdkconfig_for_profile(sdkconfig_path, "bringup")
        checks.append(("Kein aktives Profil wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        build_root = Path(tmp) / "build"
        sdkconfig_path = build_root / esp_idf_contract.build_dir_name("bringup") / "sdkconfig"
        _write_sdkconfig(
            sdkconfig_path,
            {esp_idf_contract.profile_kconfig_option("bringup"),
             esp_idf_contract.profile_kconfig_option("release")},
        )
        violations = check_sdkconfig_for_profile(sdkconfig_path, "bringup")
        checks.append(("Beide Profile gleichzeitig aktiv werden erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        build_root = Path(tmp) / "build"
        sdkconfig_path = build_root / esp_idf_contract.build_dir_name("bringup") / "sdkconfig"
        _write_sdkconfig(
            sdkconfig_path, {esp_idf_contract.profile_kconfig_option("bringup")},
            idf_target=None,
        )
        violations = check_sdkconfig_for_profile(sdkconfig_path, "bringup")
        checks.append(("Fehlender CONFIG_IDF_TARGET wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        build_root = Path(tmp) / "build"
        sdkconfig_path = build_root / esp_idf_contract.build_dir_name("bringup") / "sdkconfig"
        _write_sdkconfig(
            sdkconfig_path, {esp_idf_contract.profile_kconfig_option("bringup")},
            idf_target='"esp32s3"',
        )
        violations = check_sdkconfig_for_profile(sdkconfig_path, "bringup")
        checks.append(("Abweichender Zielchip (esp32s3 statt esp32) wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        build_root = Path(tmp) / "build"
        sdkconfig_path = build_root / esp_idf_contract.build_dir_name("bringup") / "sdkconfig"
        _write_sdkconfig(sdkconfig_path, {esp_idf_contract.profile_kconfig_option("bringup")})
        violations = check_sdkconfig_for_profile(sdkconfig_path, "bringup")
        checks.append(("Korrekter Zielchip CONFIG_IDF_TARGET=\"esp32\" wird akzeptiert", not violations))

    # --- Compile-Definitionen strukturiert -----------------------------
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "build" / esp_idf_contract.build_dir_name("bringup") / "compile_commands.json"
        defines = _good_defines("bringup") + ["APP_REAL_ACTUATORS_ENABLED=1"]
        _write_compile_commands(path, defines)
        violations = check_compile_definitions_for_profile(path, "bringup")
        checks.append(("Widerspruechlicher Aktorwert (0 und 1 gleichzeitig) wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "build" / esp_idf_contract.build_dir_name("bringup") / "compile_commands.json"
        defines = _good_defines("bringup") + ["APP_TARGET_FLASH_MB=8"]
        _write_compile_commands(path, defines)
        violations = check_compile_definitions_for_profile(path, "bringup")
        checks.append(("Widerspruechliche Flashgroesse (4 und 8 gleichzeitig) wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "build" / esp_idf_contract.build_dir_name("bringup") / "compile_commands.json"
        defines = [f"{esp_idf_contract.profile_define('bringup')}=1",
                   "APP_TARGET_FLASH_MB=4", "APP_REQUIRE_PSRAM=0", "APP_REAL_ACTUATORS_ENABLED=0"]
        _write_compile_commands(path, defines)
        violations = check_compile_definitions_for_profile(path, "bringup")
        checks.append(("Fehlender Safetywert (APP_WEB_OTA_ENABLED) wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "build" / esp_idf_contract.build_dir_name("bringup") / "compile_commands.json"
        defines = _good_defines("bringup") + [f"{esp_idf_contract.profile_define('release')}=1"]
        _write_compile_commands(path, defines)
        violations = check_compile_definitions_for_profile(path, "bringup")
        checks.append(("Zusaetzlich aktives gegenteiliges Profil wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "build" / esp_idf_contract.build_dir_name("bringup") / "compile_commands.json"
        defines = _good_defines("bringup") + ["ARDUINO=1"]
        _write_compile_commands(path, defines)
        violations = check_compile_definitions_for_profile(path, "bringup")
        checks.append(("Arduino-Definition -DARDUINO im ESP-IDF-Build wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "build" / esp_idf_contract.build_dir_name("bringup") / "compile_commands.json"
        defines = _good_defines("bringup") + ["ARDUINO_ARCH_ESP32=1"]
        _write_compile_commands(path, defines)
        violations = check_compile_definitions_for_profile(path, "bringup")
        checks.append(("Arduino-Praefixdefinition -DARDUINO_ARCH_ESP32 wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "build" / esp_idf_contract.build_dir_name("bringup") / "compile_commands.json"
        defines = _good_defines("bringup") + ["CONFIG_ARDUINO_RUNNING_CORE=1"]
        _write_compile_commands(path, defines)
        violations = check_compile_definitions_for_profile(path, "bringup")
        checks.append(("Arduino-Kconfig-Definition -DCONFIG_ARDUINO_RUNNING_CORE wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "build" / esp_idf_contract.build_dir_name("bringup") / "compile_commands.json"
        defines = _good_defines("bringup") + ["APP_HARMLESS_EXTRA_DEFINE=1"]
        _write_compile_commands(path, defines)
        violations = check_compile_definitions_for_profile(path, "bringup")
        checks.append(("Harmlose Definition ohne Arduino-Bezug wird akzeptiert", not violations))

    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "build" / esp_idf_contract.build_dir_name("release") / "compile_commands.json"
        _write_compile_commands(path, _good_defines("release"))
        violations = check_compile_definitions_for_profile(path, "release")
        checks.append(("Korrekte Compile-Definitionen werden akzeptiert", not violations))

    with tempfile.TemporaryDirectory() as tmp:
        # Getrennte Zwei-Token-Form "-D NAME=VALUE" (Abschnitt 5 des Reviews).
        path = Path(tmp) / "build" / esp_idf_contract.build_dir_name("bringup") / "compile_commands.json"
        good = _good_defines("bringup")
        first_flag = f"-D {good[0]}"
        rest_flags = " ".join(f"-D{define}" for define in good[1:])
        command = f"clang++ {first_flag} {rest_flags} app_main.cpp"
        _write_compile_commands(path, [], command=command)
        violations = check_compile_definitions_for_profile(path, "bringup")
        checks.append(("Getrennte Zwei-Token-Form (-D NAME=VALUE) wird korrekt geparst", not violations))

    # --- ESP-IDF-Herkunft: echtes temporaeres Git-Repository -----------
    with tempfile.TemporaryDirectory() as tmp:
        repo = Path(tmp) / "esp-idf"
        actual_commit = _make_git_repo(repo, tag=esp_idf_contract.ESP_IDF_TAG)
        violations = check_esp_idf_version(
            repo, expected_tag=esp_idf_contract.ESP_IDF_TAG, expected_commit=actual_commit,
        )
        checks.append(("Korrekter Commit und korrekter exakter Tag werden akzeptiert", not violations))

    with tempfile.TemporaryDirectory() as tmp:
        repo = Path(tmp) / "esp-idf"
        _make_git_repo(repo, tag=esp_idf_contract.ESP_IDF_TAG)
        violations = check_esp_idf_version(
            repo, expected_tag=esp_idf_contract.ESP_IDF_TAG, expected_commit="0" * 40,
        )
        checks.append(("Falscher erwarteter Commit wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        repo = Path(tmp) / "esp-idf"
        actual_commit = _make_git_repo(repo, tag="v0.0.0-selftest")
        violations = check_esp_idf_version(
            repo, expected_tag=esp_idf_contract.ESP_IDF_TAG, expected_commit=actual_commit,
        )
        checks.append(("Falscher erwarteter Tag wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        repo = Path(tmp) / "esp-idf"
        actual_commit = _make_git_repo(repo, tag=None)
        violations = check_esp_idf_version(
            repo, expected_tag=esp_idf_contract.ESP_IDF_TAG, expected_commit=actual_commit,
        )
        checks.append(("Fehlender exakter Tag wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        repo = Path(tmp) / "esp-idf"
        actual_commit = _make_git_repo(repo, tag=esp_idf_contract.ESP_IDF_TAG, dirty=True)
        violations = check_esp_idf_version(
            repo, expected_tag=esp_idf_contract.ESP_IDF_TAG, expected_commit=actual_commit,
        )
        checks.append(("Unsauberer Arbeitsbaum wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        missing = Path(tmp) / "does-not-exist"
        violations = check_esp_idf_version(missing)
        checks.append(("Fehlendes Verzeichnis wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        not_a_repo = Path(tmp) / "not-a-git-repo"
        not_a_repo.mkdir()
        violations = check_esp_idf_version(not_a_repo)
        checks.append(("Fehlgeschlagener Git-Befehl (kein Git-Repository) wird als Fehler behandelt", bool(violations)))

    # --- PlatformIO-Parallelvertrag -------------------------------------
    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(_make_platformio_ini(), encoding="utf-8")
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("Korrekter PlatformIO-Parallelvertrag wird akzeptiert", not violations))

    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(_make_platformio_ini(bringup_framework="espidf"), encoding="utf-8")
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("Veraendertes framework im Arduino-Environment wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(_make_platformio_ini(bringup_platform="espressif32@7.1.0"), encoding="utf-8")
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("Veraendertes platform im Arduino-Environment wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(_make_platformio_ini(bringup_board="esp32-s3-devkitc-1"), encoding="utf-8")
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("Veraendertes board im Arduino-Environment wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(
            _make_platformio_ini(include_bringup=False, default_envs="native, esp32_release"),
            encoding="utf-8",
        )
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("Vorzeitig entferntes Arduino-Environment wird in der Parallelphase erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(
            _make_platformio_ini(
                default_envs="native, esp32_bringup, esp32_release, esp32_unexpected",
                extra_sections="[env:esp32_unexpected]\nplatform = espressif32@7.0.1\n",
            ),
            encoding="utf-8",
        )
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("Unerwartetes zusaetzliches PlatformIO-Environment wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(
            _make_platformio_ini(include_native=False, default_envs="esp32_bringup, esp32_release"),
            encoding="utf-8",
        )
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("Fehlendes env:native wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(_make_platformio_ini(native_platform="espressif32@7.0.1"), encoding="utf-8")
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("env:native mit falscher Plattform wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(
            _make_platformio_ini(
                bringup_extra_flags="    -DAPP_PROFILE_ESP32_RELEASE=1\n",
                release_extra_flags="    -DAPP_PROFILE_ESP32_BRINGUP=1\n",
            ),
            encoding="utf-8",
        )
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("Vertauschte Bring-up-/Release-Profilzuordnung wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(
            _make_platformio_ini(
                profile_build_flags=(
                    "    -DAPP_TARGET_FLASH_MB=4\n"
                    "    -DAPP_REQUIRE_PSRAM=0\n"
                    "    -DAPP_WEB_OTA_ENABLED=0\n"
                ),
            ),
            encoding="utf-8",
        )
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("Fehlender gemeinsamer Safetywert in [profile] wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(
            _make_platformio_ini(
                profile_build_flags=(
                    "    -DAPP_TARGET_FLASH_MB=4\n"
                    "    -DAPP_REQUIRE_PSRAM=0\n"
                    "    -DAPP_WEB_OTA_ENABLED=0\n"
                    "    -DAPP_REAL_ACTUATORS_ENABLED=1\n"
                ),
            ),
            encoding="utf-8",
        )
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("APP_REAL_ACTUATORS_ENABLED=1 in [profile] wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(
            _make_platformio_ini(
                profile_build_flags=(
                    "    -DAPP_TARGET_FLASH_MB=4\n"
                    "    -DAPP_REQUIRE_PSRAM=0\n"
                    "    -DAPP_WEB_OTA_ENABLED=0\n"
                    "    -DAPP_REAL_ACTUATORS_ENABLED=0\n"
                    "    -DAPP_REAL_ACTUATORS_ENABLED=1\n"
                ),
            ),
            encoding="utf-8",
        )
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("Doppelter widerspruechlicher Safetywert in [profile] wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(
            _make_platformio_ini(default_envs="native, esp32_bringup"),
            encoding="utf-8",
        )
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("Unvollstaendiges default_envs wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(
            _make_platformio_ini(
                native_extra_flags="    -DAPP_PROFILE_NATIVE=1\n    -DAPP_PROFILE_ESP32_BRINGUP=1\n",
            ),
            encoding="utf-8",
        )
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("ESP32-Profildefinition in env:native wird erkannt", bool(violations)))

    # --- check_repository: Profilisolation auch bei Einzelprofil-Aufrufen ---
    with tempfile.TemporaryDirectory() as tmp:
        # Positivfall: Einzelprofil "release" ohne Konflikt wird geprueft
        # und akzeptiert.
        build_root = Path(tmp) / "build"
        _write_sdkconfig(
            build_root / esp_idf_contract.build_dir_name("release") / "sdkconfig",
            {esp_idf_contract.profile_kconfig_option("release")},
        )
        _write_compile_commands(
            build_root / esp_idf_contract.build_dir_name("release") / "compile_commands.json",
            _good_defines("release"),
        )
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(_make_platformio_ini(), encoding="utf-8")
        idf_repo = Path(tmp) / "esp-idf"
        idf_commit = _make_git_repo(idf_repo, tag=esp_idf_contract.ESP_IDF_TAG)
        exit_code = check_repository(
            str(idf_repo), ["release"], build_root=build_root, platformio_ini_path=ini_path,
            expected_tag=esp_idf_contract.ESP_IDF_TAG, expected_commit=idf_commit,
        )
        checks.append((
            "Einzelprofil-Aufruf (release) ohne Konflikt wird akzeptiert",
            exit_code == 0,
        ))

    with tempfile.TemporaryDirectory() as tmp:
        # Negativfall: Einzelprofil "release" angefordert, aber die bereits
        # vorhandene Bring-up-sdkconfig ist real dieselbe Datei -> muss auch
        # bei einem Einzelprofil-Aufruf erkannt werden.
        build_root = Path(tmp) / "build"
        release_sdkconfig = build_root / esp_idf_contract.build_dir_name("release") / "sdkconfig"
        _write_sdkconfig(release_sdkconfig, {esp_idf_contract.profile_kconfig_option("release")})
        _write_compile_commands(
            build_root / esp_idf_contract.build_dir_name("release") / "compile_commands.json",
            _good_defines("release"),
        )
        bringup_dir = build_root / esp_idf_contract.build_dir_name("bringup")
        bringup_dir.mkdir(parents=True, exist_ok=True)
        (bringup_dir / "sdkconfig").symlink_to(release_sdkconfig)
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(_make_platformio_ini(), encoding="utf-8")
        idf_repo = Path(tmp) / "esp-idf"
        idf_commit = _make_git_repo(idf_repo, tag=esp_idf_contract.ESP_IDF_TAG)
        exit_code = check_repository(
            str(idf_repo), ["release"], build_root=build_root, platformio_ini_path=ini_path,
            expected_tag=esp_idf_contract.ESP_IDF_TAG, expected_commit=idf_commit,
        )
        checks.append((
            "Einzelprofil-Aufruf (release) erkennt bereits vorhandene, mit "
            "Bring-up geteilte sdkconfig",
            exit_code != 0,
        ))

    all_passed = True
    for description, passed in checks:
        status = "PASS" if passed else "FAILED"
        print(f"{status}: {description}")
        all_passed = all_passed and passed
    return 0 if all_passed else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--idf-path", default=None,
        help="Pfad zum ESP-IDF-Checkout (sonst IDF_PATH, sonst harter Fehler).",
    )
    parser.add_argument(
        "--profile", choices=[*esp_idf_contract.PROFILES, "all"], default="all",
        help="zu pruefendes Profil, oder 'all' fuer beide (Default)",
    )
    parser.add_argument(
        "--selftest", action="store_true",
        help="Prueft die Erkennung selbst anhand temporaerer Fixtures.",
    )
    arguments = parser.parse_args()

    if arguments.selftest:
        return run_selftest()

    idf_path = arguments.idf_path or os.environ.get("IDF_PATH")
    if not idf_path:
        print("ESP-IDF-Pfad fehlt: --idf-path angeben oder IDF_PATH setzen.", file=sys.stderr)
        return 1

    profiles = (
        list(esp_idf_contract.PROFILES) if arguments.profile == "all" else [arguments.profile]
    )
    return check_repository(idf_path, profiles)


if __name__ == "__main__":
    raise SystemExit(main())
