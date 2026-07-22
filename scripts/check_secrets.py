#!/usr/bin/env python3
"""Prueft, dass keine Geheimnisse oder lokalen Konfigurationsdateien
eingecheckt sind.

Zwei Pruefungen:

1. Dateinamen, die laut `.gitignore` lokal bleiben muessen (z. B.
   `include/secrets.hpp`, `config/hardware.yaml`, `*.pem`), duerfen nicht von
   Git getrackt sein.
2. Getrackte Textdateien werden auf typische Geheimnismuster durchsucht
   (private Schluessel, AWS-artige Zugangsschluessel, zugewiesene
   Passwort-/Token-Werte).

Bekannte, dokumentierte Ausnahme: Dateien mit `example` im Namen (z. B.
`include/secrets.example.hpp`) enthalten absichtlich Platzhalterwerte wie
`YOUR_WIFI_PASSWORD` und werden von der musterbasierten Zuweisungspruefung
ausgenommen. Private-Key-Header werden trotzdem in jeder Datei erkannt.

`--selftest` prueft die Erkennung selbst anhand temporaerer Fixture-Dateien,
ohne dass ein absichtlich fehlerhafter Fall jemals in dieses Repository
eingecheckt werden muss.
"""

import argparse
import fnmatch
import re
import subprocess
import sys
import tempfile
from pathlib import Path

PROTECTED_FILENAME_PATTERNS = (
    "include/secrets.hpp",
    "include/secrets*.hpp",
    "include/secrets.h",
    "config/pins.yaml",
    "config/hardware.yaml",
    "config/settings.local.json",
    "config/*.local.yaml",
    "config/*.local.yml",
    "config/*.local.json",
    "platformio_override.ini",
    "*.pem",
    "*.key",
    "credentials.*",
    ".env",
    ".env.*",
)

# Explizit erlaubte Beispieldateien trotz `secrets*`-Muster.
ALLOWED_EXAMPLE_FILES = {"include/secrets.example.hpp"}

SECRET_CONTENT_PATTERNS = (
    re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----"),
    re.compile(r"AKIA[0-9A-Z]{16}"),
    re.compile(
        r"(?i)(password|passwd|secret|api[_-]?key|token)\s*[:=]\s*"
        r"['\"][^'\"\s]{6,}['\"]"
    ),
)

TEXT_FILE_SUFFIXES = {
    ".hpp", ".h", ".cpp", ".c", ".py", ".ini", ".json", ".yaml", ".yml",
    ".md", ".txt", ".sh", ".cfg", ".toml",
}


def tracked_files() -> list[str]:
    result = subprocess.run(
        ["git", "ls-files"], check=True, capture_output=True, text=True,
    )
    return [line for line in result.stdout.splitlines() if line]


def find_protected_files_tracked(files: list[str]) -> list[str]:
    violations = []
    for pattern in PROTECTED_FILENAME_PATTERNS:
        for file in files:
            if file in ALLOWED_EXAMPLE_FILES:
                continue
            if fnmatch.fnmatch(file, pattern):
                violations.append(file)
    return sorted(set(violations))


def scan_file_for_secrets(path: Path) -> list[tuple[int, str]]:
    is_example = "example" in path.name.lower()
    try:
        lines = path.read_text(encoding="utf-8", errors="strict").splitlines()
    except (UnicodeDecodeError, OSError):
        return []

    findings = []
    for line_number, line in enumerate(lines, start=1):
        for index, pattern in enumerate(SECRET_CONTENT_PATTERNS):
            is_assignment_pattern = index == len(SECRET_CONTENT_PATTERNS) - 1
            if is_example and is_assignment_pattern:
                continue
            if pattern.search(line):
                findings.append((line_number, line.strip()))
    return findings


def check_repository() -> int:
    files = tracked_files()

    protected_violations = find_protected_files_tracked(files)
    for violation in protected_violations:
        print(f"FAILED: geheimnisverdaechtige Datei eingecheckt: {violation}",
              file=sys.stderr)

    content_violations = []
    for file in files:
        path = Path(file)
        if path.suffix.lower() not in TEXT_FILE_SUFFIXES:
            continue
        for line_number, line in scan_file_for_secrets(path):
            content_violations.append((file, line_number, line))
            print(
                f"FAILED: moegliches Geheimnis in {file}:{line_number}",
                file=sys.stderr,
            )

    if protected_violations or content_violations:
        print(
            f"FAILED: {len(protected_violations)} geschuetzte Datei(en) "
            f"eingecheckt, {len(content_violations)} verdaechtige Textstelle(n)",
            file=sys.stderr,
        )
        return 1

    print(f"PASS: {len(files)} getrackte Dateien geprueft, keine Geheimnisse gefunden.")
    return 0


def run_selftest() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)

        bad_key_file = tmp_path / "fixture_private_key.pem"
        # Literal split via concatenation so this source file does not itself
        # contain a complete secret pattern and can be scanned without exclusion.
        bad_key_file.write_text(
            "-----BEGIN RSA PRIVATE " + "KEY-----\nMIIfake==\n"
            "-----END RSA PRIVATE KEY-----\n"
        )
        bad_password_file = tmp_path / "fixture_password.ini"
        bad_password_file.write_text('wifi_pass' + 'word = "supersecretvalue"\n')

        good_example_file = tmp_path / "fixture.example.hpp"
        good_example_file.write_text(
            'inline constexpr char kWifiPassword[] = "YOUR_WIFI_PASSWORD";\n'
        )

        clean_file = tmp_path / "fixture_clean.cpp"
        clean_file.write_text("int add(int a, int b) { return a + b; }\n")

        key_findings = scan_file_for_secrets(bad_key_file)
        password_findings = scan_file_for_secrets(bad_password_file)
        example_findings = scan_file_for_secrets(good_example_file)
        clean_findings = scan_file_for_secrets(clean_file)

        checks = [
            ("Private-Key-Fixture wird erkannt", len(key_findings) > 0),
            ("Passwort-Zuweisungs-Fixture wird erkannt", len(password_findings) > 0),
            ("Beispieldatei mit Platzhalter wird NICHT gemeldet",
             len(example_findings) == 0),
            ("Unverdaechtige Datei wird NICHT gemeldet", len(clean_findings) == 0),
        ]

        all_passed = True
        for description, passed in checks:
            status = "PASS" if passed else "FAILED"
            print(f"{status}: {description}")
            all_passed = all_passed and passed

        return 0 if all_passed else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--selftest", action="store_true",
        help="Prueft die Erkennung selbst anhand temporaerer Fixtures.",
    )
    arguments = parser.parse_args()

    if arguments.selftest:
        return run_selftest()
    return check_repository()


if __name__ == "__main__":
    raise SystemExit(main())
