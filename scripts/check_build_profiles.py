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

`--selftest` prueft die Erkennung selbst anhand temporaerer Fixture-Dateien,
ohne dass ein absichtlich fehlerhafter Fall jemals in dieses Repository
eingecheckt werden muss.
"""

import argparse
import configparser
import json
import sys
import tempfile
from pathlib import Path

EXPECTED_IDF_TAG = "v6.0.2"
EXPECTED_IDF_COMMIT = "7101770dc6db2667b3c477cc31365dd1acd6db4e"

PROFILE_CONFIG_OPTION = {
    "bringup": "CONFIG_APP_PROFILE_ESP32_BRINGUP",
    "release": "CONFIG_APP_PROFILE_ESP32_RELEASE",
}

PROFILE_DEFINE = {
    "bringup": "APP_PROFILE_ESP32_BRINGUP=1",
    "release": "APP_PROFILE_ESP32_RELEASE=1",
}

REQUIRED_APP_DEFINES = (
    "APP_TARGET_FLASH_MB=4",
    "APP_REQUIRE_PSRAM=0",
    "APP_WEB_OTA_ENABLED=0",
    "APP_REAL_ACTUATORS_ENABLED=0",
)

FORBIDDEN_DEFINE_SUBSTRINGS = ("ARDUINO",)

EXPECTED_PLATFORMIO_ARDUINO_ENVS = {
    "env:esp32_bringup": {
        "platform": "espressif32@7.0.1",
        "framework": "arduino",
        "board": "esp32dev",
    },
    "env:esp32_release": {
        "platform": "espressif32@7.0.1",
        "framework": "arduino",
        "board": "esp32dev",
    },
}

EXPECTED_PLATFORMIO_NATIVE_ENV = {
    "platform": "native",
    "test_framework": "unity",
}

ALLOWED_PLATFORMIO_SECTIONS = {"platformio", "profile", "env:native"} | set(
    EXPECTED_PLATFORMIO_ARDUINO_ENVS
)


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def read_sdkconfig(sdkconfig_path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in sdkconfig_path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") and "is not set" not in line:
            continue
        if line.startswith("# ") and line.endswith("is not set"):
            option = line[2:].split(" ", 1)[0]
            values[option] = "n"
            continue
        if "=" in line:
            option, value = line.split("=", 1)
            values[option] = value
    return values


def check_sdkconfig_for_profile(sdkconfig_path: Path, profile: str) -> list[str]:
    violations: list[str] = []
    if not sdkconfig_path.is_file():
        return [f"generierte sdkconfig fehlt: {sdkconfig_path}"]

    values = read_sdkconfig(sdkconfig_path)
    own_option = PROFILE_CONFIG_OPTION[profile]
    other_profile = "release" if profile == "bringup" else "bringup"
    other_option = PROFILE_CONFIG_OPTION[other_profile]

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

    return violations


def check_compile_definitions_for_profile(
    compile_commands_path: Path, profile: str
) -> list[str]:
    violations: list[str] = []
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

    command_text = app_main_entry.get("command")
    if command_text is None:
        command_text = " ".join(app_main_entry.get("arguments", []))

    own_define = PROFILE_DEFINE[profile]
    other_profile = "release" if profile == "bringup" else "bringup"
    other_define = PROFILE_DEFINE[other_profile]

    if f"-D{own_define}" not in command_text:
        violations.append(f"app_main.cpp: erwartete Definition -D{own_define} fehlt")
    if f"-D{other_define}" in command_text:
        violations.append(
            f"app_main.cpp: unerwartete gegenteilige Definition -D{other_define} vorhanden"
        )
    for required in REQUIRED_APP_DEFINES:
        if f"-D{required}" not in command_text:
            violations.append(f"app_main.cpp: erwartete Definition -D{required} fehlt")
    for forbidden in FORBIDDEN_DEFINE_SUBSTRINGS:
        if forbidden in command_text:
            violations.append(
                f"app_main.cpp: verbotene Arduino-Definition enthaelt '{forbidden}'"
            )

    return violations


def check_profiles_are_isolated(build_root: Path) -> list[str]:
    bringup_sdkconfig = build_root / "esp32_bringup" / "sdkconfig"
    release_sdkconfig = build_root / "esp32_release" / "sdkconfig"
    if not (bringup_sdkconfig.is_file() and release_sdkconfig.is_file()):
        return []

    if bringup_sdkconfig.resolve() == release_sdkconfig.resolve():
        return ["Bring-up- und Release-Profil teilen dieselbe generierte sdkconfig-Datei"]

    bringup_values = read_sdkconfig(bringup_sdkconfig)
    release_values = read_sdkconfig(release_sdkconfig)
    if bringup_values.get(PROFILE_CONFIG_OPTION["bringup"]) == release_values.get(
        PROFILE_CONFIG_OPTION["bringup"]
    ):
        return [
            "Bring-up- und Release-sdkconfig unterscheiden sich nicht in der Profilwahl "
            "(gemeinsamer Buildordner oder vertauschte Overlays wahrscheinlich)"
        ]
    return []


def check_esp_idf_profile(build_root: Path, profile: str) -> list[str]:
    profile_dir = build_root / f"esp32_{profile}"
    violations: list[str] = []
    violations += check_sdkconfig_for_profile(profile_dir / "sdkconfig", profile)
    violations += check_compile_definitions_for_profile(
        profile_dir / "compile_commands.json", profile
    )
    return violations


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

    if "env:native" not in parser:
        violations.append("PlatformIO-Environment env:native fehlt")
    else:
        for key, expected in EXPECTED_PLATFORMIO_NATIVE_ENV.items():
            actual = parser["env:native"].get(key)
            if actual != expected:
                violations.append(
                    f"env:native: erwartete {key}={expected!r}, gefunden {actual!r}"
                )

    for section, expected_options in EXPECTED_PLATFORMIO_ARDUINO_ENVS.items():
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

    return violations


def check_esp_idf_version(idf_git_dir: Path) -> list[str]:
    import subprocess

    if not idf_git_dir.is_dir():
        return [f"ESP-IDF-Checkout nicht gefunden: {idf_git_dir}"]

    commit_result = subprocess.run(
        ["git", "-C", str(idf_git_dir), "rev-parse", "HEAD"],
        capture_output=True, text=True,
    )
    actual_commit = commit_result.stdout.strip()
    if commit_result.returncode != 0 or actual_commit != EXPECTED_IDF_COMMIT:
        return [
            f"falscher ESP-IDF-Commit: erwartet {EXPECTED_IDF_COMMIT}, "
            f"gefunden {actual_commit or '(nicht ermittelbar)'}"
        ]

    tag_result = subprocess.run(
        ["git", "-C", str(idf_git_dir), "describe", "--tags", "--exact-match"],
        capture_output=True, text=True,
    )
    actual_tag = tag_result.stdout.strip()
    if tag_result.returncode != 0 or actual_tag != EXPECTED_IDF_TAG:
        return [
            f"falscher ESP-IDF-Tag: erwartet {EXPECTED_IDF_TAG}, "
            f"gefunden {actual_tag or '(kein exakter Tag)'}"
        ]

    return []


def check_repository(idf_path: str | None) -> int:
    root = repo_root()
    violations: list[str] = []

    for profile in ("bringup", "release"):
        violations += check_esp_idf_profile(root / "build", profile)
    violations += check_profiles_are_isolated(root / "build")
    violations += check_platformio_parallel_contract(root / "platformio.ini")

    if idf_path:
        violations += check_esp_idf_version(Path(idf_path))

    if violations:
        for violation in violations:
            print(f"FAILED: {violation}", file=sys.stderr)
        print(f"FAILED: {len(violations)} Verletzung(en) gefunden", file=sys.stderr)
        return 1

    print("PASS: beide ESP-IDF-Profile und der PlatformIO-Parallelvertrag sind korrekt.")
    return 0


def _write_sdkconfig(path: Path, active_option: str | None, *, flash_ok: bool = True,
                      other_option: str | None = None, other_active: bool = False) -> None:
    lines = []
    for option in (PROFILE_CONFIG_OPTION["bringup"], PROFILE_CONFIG_OPTION["release"]):
        if option == active_option:
            lines.append(f"{option}=y")
        elif option == other_option and other_active:
            lines.append(f"{option}=y")
        else:
            lines.append(f"# {option} is not set")
    if flash_ok:
        lines.append("CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _write_compile_commands(path: Path, defines: list[str]) -> None:
    command = "clang++ " + " ".join(f"-D{define}" for define in defines) + " app_main.cpp"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps([{"file": "main/app_main.cpp", "command": command}]),
        encoding="utf-8",
    )


def _good_defines(profile: str) -> list[str]:
    return [PROFILE_DEFINE[profile], *REQUIRED_APP_DEFINES]


def run_selftest() -> int:
    checks: list[tuple[str, bool]] = []

    with tempfile.TemporaryDirectory() as tmp:
        build_root = Path(tmp) / "build"

        # Fall: getrennte, korrekte ESP-IDF-Profile -> keine Verletzung.
        _write_sdkconfig(build_root / "esp32_bringup" / "sdkconfig", PROFILE_CONFIG_OPTION["bringup"])
        _write_compile_commands(
            build_root / "esp32_bringup" / "compile_commands.json", _good_defines("bringup")
        )
        _write_sdkconfig(build_root / "esp32_release" / "sdkconfig", PROFILE_CONFIG_OPTION["release"])
        _write_compile_commands(
            build_root / "esp32_release" / "compile_commands.json", _good_defines("release")
        )
        violations = check_esp_idf_profile(build_root, "bringup")
        violations += check_esp_idf_profile(build_root, "release")
        violations += check_profiles_are_isolated(build_root)
        checks.append(("Getrennte, korrekte ESP-IDF-Profile werden akzeptiert", not violations))

    with tempfile.TemporaryDirectory() as tmp:
        # Fall: vertauschte Overlays / gemeinsamer Buildordner (identische sdkconfig).
        build_root = Path(tmp) / "build"
        _write_sdkconfig(build_root / "esp32_bringup" / "sdkconfig", PROFILE_CONFIG_OPTION["bringup"])
        _write_sdkconfig(build_root / "esp32_release" / "sdkconfig", PROFILE_CONFIG_OPTION["bringup"])
        violations = check_profiles_are_isolated(build_root)
        checks.append(("Vertauschte Overlays/gemeinsamer Buildordner werden erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        # Fall: kein Profil aktiv.
        build_root = Path(tmp) / "build"
        _write_sdkconfig(build_root / "esp32_bringup" / "sdkconfig", None)
        _write_compile_commands(
            build_root / "esp32_bringup" / "compile_commands.json", list(REQUIRED_APP_DEFINES)
        )
        violations = check_esp_idf_profile(build_root, "bringup")
        checks.append(("Kein aktives Profil wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        # Fall: beide Profile gleichzeitig aktiv.
        build_root = Path(tmp) / "build"
        _write_sdkconfig(
            build_root / "esp32_bringup" / "sdkconfig", PROFILE_CONFIG_OPTION["bringup"],
            other_option=PROFILE_CONFIG_OPTION["release"], other_active=True,
        )
        violations = check_sdkconfig_for_profile(
            build_root / "esp32_bringup" / "sdkconfig", "bringup"
        )
        checks.append(("Beide Profile gleichzeitig aktiv werden erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        # Fall: falscher ESP-IDF-Tag/-Commit.
        fake_idf = Path(tmp) / "not-a-real-esp-idf-checkout"
        violations = check_esp_idf_version(fake_idf)
        checks.append(("Falscher/fehlender ESP-IDF-Checkout wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        # Fall: reale Aktoren aktiviert.
        build_root = Path(tmp) / "build"
        bad_defines = [PROFILE_DEFINE["bringup"], "APP_TARGET_FLASH_MB=4",
                       "APP_REQUIRE_PSRAM=0", "APP_WEB_OTA_ENABLED=0",
                       "APP_REAL_ACTUATORS_ENABLED=1"]
        _write_compile_commands(
            build_root / "esp32_bringup" / "compile_commands.json", bad_defines
        )
        violations = check_compile_definitions_for_profile(
            build_root / "esp32_bringup" / "compile_commands.json", "bringup"
        )
        checks.append(("Reale Aktoren aktiviert wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        # Fall: Arduino-Definition im ESP-IDF-Build.
        build_root = Path(tmp) / "build"
        bad_defines = [*_good_defines("bringup"), "ARDUINO=1"]
        _write_compile_commands(
            build_root / "esp32_bringup" / "compile_commands.json", bad_defines
        )
        violations = check_compile_definitions_for_profile(
            build_root / "esp32_bringup" / "compile_commands.json", "bringup"
        )
        checks.append(("Arduino-Definition im ESP-IDF-Build wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        # Fall: unerwartetes zusaetzliches PlatformIO-Environment.
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(
            "[platformio]\n"
            "default_envs = native, esp32_bringup, esp32_release\n\n"
            "[env:native]\nplatform = native\ntest_framework = unity\n\n"
            "[env:esp32_bringup]\nplatform = espressif32@7.0.1\nframework = arduino\n"
            "board = esp32dev\n\n"
            "[env:esp32_release]\nplatform = espressif32@7.0.1\nframework = arduino\n"
            "board = esp32dev\n\n"
            "[env:esp32_unexpected]\nplatform = espressif32@7.0.1\n",
            encoding="utf-8",
        )
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("Unerwartetes zusaetzliches PlatformIO-Environment wird erkannt", bool(violations)))

    with tempfile.TemporaryDirectory() as tmp:
        # Fall: Arduino-Env vorzeitig entfernt (Parallelphase verletzt).
        ini_path = Path(tmp) / "platformio.ini"
        ini_path.write_text(
            "[platformio]\ndefault_envs = native\n\n"
            "[env:native]\nplatform = native\ntest_framework = unity\n",
            encoding="utf-8",
        )
        violations = check_platformio_parallel_contract(ini_path)
        checks.append(("Vorzeitig entfernte Arduino-Envs werden in der Parallelphase erkannt", bool(violations)))

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
        help="Pfad zum ESP-IDF-Checkout fuer die Herkunftspruefung (optional)",
    )
    parser.add_argument(
        "--selftest", action="store_true",
        help="Prueft die Erkennung selbst anhand temporaerer Fixtures.",
    )
    arguments = parser.parse_args()

    if arguments.selftest:
        return run_selftest()
    return check_repository(arguments.idf_path)


if __name__ == "__main__":
    raise SystemExit(main())
