#!/usr/bin/env python3
"""Kanonischer ESP-IDF-Buildtreiber fuer beide Profile (Issue #74).

Baut `esp32_bringup` und/oder `esp32_release` in strikt getrennten
Buildpfaden (`build/<profil>/`, jeweils eigenes generiertes `sdkconfig`,
siehe docs/tasks/issue-74-implementation-plan.md, Abschnitt 7.3). Wird von
CI und von der lokalen Entwicklerdokumentation gleichermassen aufgerufen,
mit exakt denselben Befehlen (Abschnitt 7.4) — keine zweite, abweichende
Folge handgeschriebener `idf.py`-Befehle.

Installiert oder aendert die ESP-IDF-Toolchain nicht (das ist Aufgabe der
direkten Installation aus Abschnitt 7.1) und flasht nicht. Setzt voraus,
dass `idf.py` bereits auf dem `PATH` verfuegbar ist (z. B. durch vorheriges
`. $IDF_PATH/export.sh`).

Bricht bei einem Fehler mit Profilname und fehlgeschlagener Phase ab,
statt stillschweigend fortzufahren.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

EXPECTED_IDF_TAG = "v6.0.2"
EXPECTED_IDF_COMMIT = "7101770dc6db2667b3c477cc31365dd1acd6db4e"

# Profilname -> zusaetzliche Overlay-Datei (relativ zum Repository-Root).
# sdkconfig.defaults ist fuer beide Profile immer die gemeinsame Basis.
PROFILE_OVERLAYS = {
    "bringup": "sdkconfig.defaults.bringup",
    "release": "sdkconfig.defaults.release",
}


class BuildDriverError(RuntimeError):
    def __init__(self, profile: str, phase: str, detail: str) -> None:
        self.profile = profile
        self.phase = phase
        self.detail = detail
        super().__init__(f"[{profile}:{phase}] {detail}")


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def run(command: list[str], *, cwd: Path | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(
        command, cwd=cwd, capture_output=True, text=True,
    )


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

    commit_result = run(["git", "-C", str(idf_path), "rev-parse", "HEAD"])
    if commit_result.returncode != 0:
        raise BuildDriverError(
            "-", "environment",
            f"ESP-IDF-Commit konnte nicht ermittelt werden: {commit_result.stderr.strip()}",
        )
    actual_commit = commit_result.stdout.strip()
    if actual_commit != EXPECTED_IDF_COMMIT:
        raise BuildDriverError(
            "-", "environment",
            f"falscher ESP-IDF-Commit: erwartet {EXPECTED_IDF_COMMIT}, "
            f"gefunden {actual_commit}",
        )

    tag_result = run(["git", "-C", str(idf_path), "describe", "--tags", "--exact-match"])
    actual_tag = tag_result.stdout.strip()
    if tag_result.returncode != 0 or actual_tag != EXPECTED_IDF_TAG:
        raise BuildDriverError(
            "-", "environment",
            f"falscher ESP-IDF-Tag: erwartet {EXPECTED_IDF_TAG}, "
            f"gefunden {actual_tag or '(kein exakter Tag)'}",
        )

    status_result = run(["git", "-C", str(idf_path), "status", "--short"])
    if status_result.stdout.strip():
        raise BuildDriverError(
            "-", "environment",
            "ESP-IDF-Arbeitsbaum ist nicht sauber:\n" + status_result.stdout,
        )

    version_result = run(["idf.py", "--version"])
    if version_result.returncode != 0 or EXPECTED_IDF_TAG not in version_result.stdout:
        raise BuildDriverError(
            "-", "environment",
            f"idf.py --version meldet nicht {EXPECTED_IDF_TAG}: "
            f"{version_result.stdout.strip()} {version_result.stderr.strip()}",
        )


def build_profile(profile: str) -> None:
    if profile not in PROFILE_OVERLAYS:
        raise BuildDriverError(profile, "argument", f"unbekanntes Profil: {profile}")

    root = repo_root()
    build_dir = root / "build" / f"esp32_{profile}"
    sdkconfig_path = build_dir / "sdkconfig"
    overlay = PROFILE_OVERLAYS[profile]
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
            f"esp32_{profile}", "build",
            f"idf.py build fehlgeschlagen (Exit-Code {result.returncode})",
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "profile", choices=["bringup", "release", "all"],
        help="zu bauendes Profil, oder 'all' fuer beide nacheinander",
    )
    arguments = parser.parse_args()

    profiles = ["bringup", "release"] if arguments.profile == "all" else [arguments.profile]

    try:
        verify_idf_environment()
        for profile in profiles:
            print(f"=== Baue Profil esp32_{profile} ===")
            build_profile(profile)
    except BuildDriverError as error:
        print(f"FEHLGESCHLAGEN: {error}", file=sys.stderr)
        return 1

    print(f"Alle angeforderten Profile erfolgreich gebaut: {', '.join(profiles)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
