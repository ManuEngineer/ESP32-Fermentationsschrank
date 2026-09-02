#!/usr/bin/env python3
"""Kanonischer ESP-IDF-Buildtreiber fuer beide Profile (Issue #74).

Baut `esp32_bringup` und/oder `esp32_release` in strikt getrennten
Buildpfaden (`build/<profil>/`, jeweils eigenes generiertes `sdkconfig`,
siehe docs/tasks/issue-74-implementation-plan.md, Abschnitt 7.3). Wird von
CI und von der lokalen Entwicklerdokumentation gleichermassen aufgerufen,
mit exakt denselben Befehlen (Abschnitt 7.4) — keine zweite, abweichende
Folge handgeschriebener `idf.py`-Befehle.

Ein technisch erfolgreicher `idf.py build` allein gilt **nicht** als
Erfolg: ein vorhandenes generiertes `sdkconfig` hat Vorrang vor den
Defaults, sodass ein Build technisch gelingen kann, obwohl das falsche
Profil aktiv ist. Nach allen angeforderten Builds fuehrt dieser Treiber
deshalb zwingend die reale Profil- und Herkunftspruefung aus
`scripts/check_build_profiles.py` aus (per `sys.executable`, als eigener
Prozess, kein eigener Guard-Code in dieser Datei — SOLID: dieser Treiber
orchestriert Build und Validierung, `check_build_profiles.py` enthaelt die
Pruef- und Vertragslogik). Erst nach bestandener Validierung wird
Gesamterfolg gemeldet.

Die fruehe ESP-IDF-Herkunftspruefung (Commit/Tag/sauberer Arbeitsbaum) vor
dem Build importiert dieselbe kanonische Funktion
`check_build_profiles.check_esp_idf_version()`, statt sie erneut zu
implementieren (DRY: genau eine Implementierung der Herkunftspruefung).
Dieser Treiber behaelt nur seine eigenen Vorbedingungen (`IDF_PATH`
vorhanden, Pfad ist ein Verzeichnis, `idf.py` auf `PATH`,
`idf.py --version` passt).

Installiert oder aendert die ESP-IDF-Toolchain nicht (das ist Aufgabe der
direkten Installation aus Abschnitt 7.1) und flasht nicht. Setzt voraus,
dass `idf.py` bereits auf dem `PATH` verfuegbar ist (z. B. durch vorheriges
`. $IDF_PATH/export.sh`).

Bricht bei einem Fehler mit Profilname und fehlgeschlagener Phase ab,
statt stillschweigend fortzufahren.
"""

import argparse
import io
import os
import shutil
import subprocess
import sys
from contextlib import redirect_stdout
from pathlib import Path

import check_build_profiles
import esp_idf_contract


class BuildDriverError(RuntimeError):
    def __init__(self, profile: str, phase: str, detail: str) -> None:
        self.profile = profile
        self.phase = phase
        self.detail = detail
        super().__init__(f"[{profile}:{phase}] {detail}")


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def run(command: list[str], *, cwd: Path | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True)


def verify_idf_environment() -> None:
    """Prueft aktive ESP-IDF-Version und exakten Commit (Abschnitt 7.1/7.4)."""
    idf_path_value = os.environ.get("IDF_PATH")
    if not idf_path_value:
        raise BuildDriverError(
            "-", "environment",
            "IDF_PATH ist nicht gesetzt; zuerst `. $IDF_PATH/export.sh` ausfuehren",
        )
    idf_path = Path(idf_path_value)
    if not idf_path.is_dir():
        raise BuildDriverError(
            "-", "environment", f"IDF_PATH zeigt auf kein Verzeichnis: {idf_path}",
        )

    if shutil.which("idf.py") is None:
        raise BuildDriverError(
            "-", "environment",
            "idf.py nicht auf PATH gefunden; zuerst `. $IDF_PATH/export.sh` ausfuehren",
        )

    # Herkunftspruefung (Commit/Tag/sauberer Arbeitsbaum) ist keine zweite
    # Implementierung, sondern ein Aufruf der kanonischen Pruefung aus
    # check_build_profiles.py (DRY, Abschnitt 1 des Reviews).
    origin_violations = check_build_profiles.check_esp_idf_version(idf_path)
    if origin_violations:
        raise BuildDriverError("-", "environment", "; ".join(origin_violations))

    version_result = run(["idf.py", "--version"])
    if version_result.returncode != 0 or esp_idf_contract.ESP_IDF_TAG not in version_result.stdout:
        raise BuildDriverError(
            "-", "environment",
            f"idf.py --version meldet nicht {esp_idf_contract.ESP_IDF_TAG}: "
            f"{version_result.stdout.strip()} {version_result.stderr.strip()}",
        )


def build_profile(profile: str) -> None:
    root = repo_root()
    build_dir = root / "build" / esp_idf_contract.build_dir_name(profile)
    sdkconfig_path = build_dir / "sdkconfig"
    overlay = esp_idf_contract.sdkconfig_overlay(profile)
    sdkconfig_defaults = f"sdkconfig.defaults;{overlay}"

    command = [
        "idf.py",
        "-B", str(build_dir),
        f"-DSDKCONFIG={sdkconfig_path}",
        f"-DSDKCONFIG_DEFAULTS={sdkconfig_defaults}",
        "build",
    ]
    result = subprocess.run(command, cwd=root)
    if result.returncode != 0:
        raise BuildDriverError(
            esp_idf_contract.build_dir_name(profile), "build",
            f"idf.py build fehlgeschlagen (Exit-Code {result.returncode})",
        )


def run_profile_contract_validation(profiles: list[str]) -> None:
    """Ruft check_build_profiles.py als eigenen Prozess auf (kein Import,
    keine Kopie der Guardlogik). Prueft genau die soeben gebauten Profile."""
    root = repo_root()
    idf_path = os.environ["IDF_PATH"]
    profile_argument = (
        "all" if len(profiles) == len(esp_idf_contract.PROFILES) else profiles[0]
    )
    command = [
        sys.executable, str(root / "scripts" / "check_build_profiles.py"),
        "--idf-path", idf_path,
        "--profile", profile_argument,
    ]
    result = subprocess.run(command, cwd=root)
    if result.returncode != 0:
        raise BuildDriverError(
            "+".join(esp_idf_contract.build_dir_name(p) for p in profiles), "validation",
            f"Profil-/Driftpruefung nach dem Build fehlgeschlagen (Exit-Code {result.returncode})",
        )


def orchestrate(
    profiles: list[str],
    build_fn=build_profile,
    validate_fn=run_profile_contract_validation,
    *,
    require_clean_source_tree: bool = False,
) -> None:
    """Build -> reale Profil-/Herkunftspruefung -> erst dann Erfolg melden."""
    if require_clean_source_tree:
        try:
            check_build_profiles.require_clean_source_tree(repo_root())
        except RuntimeError as error:
            raise BuildDriverError(
                "+".join(profiles), "source-cleanliness", str(error)
            ) from error
        print("SOURCE_TREE_CLEAN=YES")
    for profile in profiles:
        print(f"=== Baue Profil {esp_idf_contract.build_dir_name(profile)} ===")
        build_fn(profile)
    validate_fn(profiles)
    print("PASS: angeforderte ESP-IDF-Profile wurden gebaut und validiert.")


def run_selftest() -> int:
    """Beweist die Guard-Integration ohne echte ESP-IDF-Builds: ein
    technisch 'erfolgreicher' Build mit fehlschlagendem Guard darf nicht als
    Gesamterfolg gemeldet werden, und der Guard muss nach dem Build
    tatsaechlich aufgerufen werden, bevor Erfolg gemeldet wird."""
    checks: list[tuple[str, bool]] = []

    def fake_build_ok(profile: str) -> None:
        return None

    def fake_validate_fails(profiles: list[str]) -> None:
        raise BuildDriverError(
            "+".join(esp_idf_contract.build_dir_name(p) for p in profiles), "validation",
            "simulierter Guard-Fehlschlag (Selftest, kein echter Build)",
        )

    stdout_buffer = io.StringIO()
    guard_error: BuildDriverError | None = None
    try:
        with redirect_stdout(stdout_buffer):
            orchestrate(["bringup"], build_fn=fake_build_ok, validate_fn=fake_validate_fails)
    except BuildDriverError as error:
        guard_error = error

    checks.append((
        "Technisch erfolgreicher Build mit fehlschlagendem Guard wird nicht "
        "als Gesamterfolg gemeldet",
        guard_error is not None and "PASS:" not in stdout_buffer.getvalue(),
    ))
    checks.append((
        "Guardfehler wird mit Phase 'validation' korrekt weitergereicht",
        guard_error is not None and guard_error.phase == "validation",
    ))

    validation_calls: list[list[str]] = []

    def fake_validate_ok(profiles: list[str]) -> None:
        validation_calls.append(list(profiles))

    stdout_buffer_ok = io.StringIO()
    with redirect_stdout(stdout_buffer_ok):
        orchestrate(["bringup"], build_fn=fake_build_ok, validate_fn=fake_validate_ok)
    checks.append((
        "Guard wird nach dem Build tatsaechlich aufgerufen, bevor Erfolg gemeldet wird",
        validation_calls == [["bringup"]] and "PASS:" in stdout_buffer_ok.getvalue(),
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
        "profile", nargs="?", choices=[*esp_idf_contract.PROFILES, "all"], default=None,
        help="zu bauendes Profil, oder 'all' fuer beide nacheinander",
    )
    parser.add_argument(
        "--selftest", action="store_true",
        help="Prueft die Guard-Integration selbst, ohne echte ESP-IDF-Builds.",
    )
    parser.add_argument(
        "--require-clean-source-tree",
        action="store_true",
        help=(
            "fordert fuer beweisfuehrende Release-/Bring-up-Artefakte "
            "einen sauberen Git-Baum"
        ),
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
        orchestrate(
            profiles, require_clean_source_tree=arguments.require_clean_source_tree
        )
    except BuildDriverError as error:
        print(f"FEHLGESCHLAGEN: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
