#!/usr/bin/env python3
"""Beweist, dass die Format-, Static-Analysis- und Geheimnispruefungen einen
absichtlich fehlerhaften Fall tatsaechlich erkennen.

Alle Fixtures werden in einem temporaeren Verzeichnis erzeugt und wieder
entfernt. Dadurch bleibt `main` immer gruen; kein absichtlich fehlerhafter Fall
wird jemals in dieses Repository eingecheckt.

Ergebnis je Teilpruefung: PASS oder FAILED. Wird ein Werkzeug selbst nicht
gefunden (z. B. clang-format/clang-tidy lokal nicht installiert), ist das
Ergebnis BLOCKED statt eines falschen PASS oder FAILED.
"""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

BLOCKED = "BLOCKED"
PASS = "PASS"
FAILED = "FAILED"


def report(name: str, status: str, detail: str = "") -> None:
    suffix = f": {detail}" if detail else ""
    print(f"{status}: {name}{suffix}")


def selftest_format() -> str:
    clang_format = shutil.which("clang-format")
    if clang_format is None:
        return BLOCKED

    with tempfile.TemporaryDirectory() as tmp:
        bad_file = Path(tmp) / "badly_formatted.cpp"
        bad_file.write_text("int f(int a,int b){return a+b;}\n")

        result = subprocess.run(
            [clang_format, "--dry-run", "--Werror", str(bad_file)],
            capture_output=True, text=True,
        )
        return FAILED if result.returncode == 0 else PASS


def selftest_static_analysis() -> str:
    clang_tidy = shutil.which("clang-tidy")
    if clang_tidy is None:
        return BLOCKED

    with tempfile.TemporaryDirectory() as tmp:
        bad_file = Path(tmp) / "missing_braces.cpp"
        bad_file.write_text(
            "bool f(bool b) {\n    if (b) return true;\n    return false;\n}\n"
        )

        result = subprocess.run(
            [clang_tidy, "--checks=-*,readability-braces-around-statements",
             "--warnings-as-errors=*", str(bad_file), "--", "-std=c++17"],
            capture_output=True, text=True,
        )
        return FAILED if result.returncode == 0 else PASS


def selftest_secrets(repo_root: Path) -> str:
    result = subprocess.run(
        [sys.executable, str(repo_root / "scripts" / "check_secrets.py"), "--selftest"],
        capture_output=True, text=True,
    )
    return PASS if result.returncode == 0 else FAILED


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent

    results = {
        "Format-Pruefung erkennt absichtlich fehlerhaften Fall": selftest_format(),
        "Static-Analysis erkennt absichtlich fehlerhaften Fall": selftest_static_analysis(),
        "Geheimnispruefung erkennt absichtlich fehlerhaften Fall": selftest_secrets(repo_root),
    }

    for name, status in results.items():
        report(name, status)

    if FAILED in results.values():
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
