#!/usr/bin/env python3
"""Prueft, dass keine Geheimnisse oder lokalen Konfigurationsdateien
eingecheckt sind.

Zwei Pruefungen auf getrackten Dateien:

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

Zusaetzliche, optionale dritte Pruefung ueber `--scan-path` (wiederholbar):
`git ls-files` erfasst nur getrackte Dateien. Generierte, bewusst
ungetrackte Artefakte unter `build/` (ESP-IDF-Groessenberichte, Buildlogs,
Artefaktmanifeste, generierte `sdkconfig`, `compile_commands.json`) werden
dadurch nicht erfasst (docs/tasks/issue-74-implementation-plan.md,
Abschnitt 7.7.4). `--scan-path` benennt solche Dateien explizit und
unterwirft sie denselben Geheimnismustern, plus einer zusaetzlichen
Pruefung auf private absolute Benutzerpfade. Dabei gilt eine
kontextbezogene Regel: selbst erzeugte Manifeste muessen vollstaendig
normalisiert sein (jeder `/home/...`- oder `/Users/...`-Pfad ist ein Fund);
fremdgeneriertes Werkzeugausgabe wie `compile_commands.json` (CMake) und
`build.log` (ninja/idf.py) darf dagegen bekannte, ephemere CI-Runner-Pfade
(`/home/runner/...`) enthalten, ohne als Fund gewertet zu werden. Eine
erwartete, aber fehlende `--scan-path`-Datei ist ein harter Fehler.
Binaerdateien werden anhand ihrer Dekodierbarkeit erkannt und nicht als
Text durchsucht.

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
        r"(?i)(password|passwd|secret|api[_-]?key|token)['\"]?\s*[:=]\s*"
        r"['\"][^'\"\s]{6,}['\"]"
    ),
)

TEXT_FILE_SUFFIXES = {
    ".hpp", ".h", ".cpp", ".c", ".py", ".ini", ".json", ".yaml", ".yml",
    ".md", ".txt", ".sh", ".cfg", ".toml",
}

# Abschnitt 7.7.4: bekannte, ephemere CI-Runner-Pfade, die in fremdgenerierten
# Dateien (z. B. `compile_commands.json`, `build.log`) toleriert werden, ohne
# als privater Benutzerpfad gewertet zu werden. Andere `/home/...`- oder
# `/Users/...`-Pfade bleiben ein Fund. Selbst erzeugte Dateien (Manifest)
# stehen bewusst NICHT in dieser Liste: sie enthalten von vornherein keine
# Pfade und muessen es auch nicht.
KNOWN_CI_PATH_PREFIXES = ("/home/runner/",)
LENIENT_PATH_FILENAMES = {"compile_commands.json", "build.log"}

PRIVATE_ABSOLUTE_PATH_PATTERN = re.compile(r"(/home/[^\s\"']+|/Users/[^\s\"']+)")


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


def scan_for_private_paths(path: Path, lines: list[str]) -> list[tuple[int, str]]:
    lenient = path.name in LENIENT_PATH_FILENAMES
    findings = []
    for line_number, line in enumerate(lines, start=1):
        for match in PRIVATE_ABSOLUTE_PATH_PATTERN.finditer(line):
            candidate = match.group(0)
            if lenient and candidate.startswith(KNOWN_CI_PATH_PREFIXES):
                continue
            findings.append((line_number, line.strip()))
            break
    return findings


def scan_path_file(path: Path) -> tuple[list[tuple[int, str]], list[tuple[int, str]]]:
    """Prueft eine explizit per `--scan-path` benannte, ungetrackte Datei auf
    Geheimnisse und private absolute Pfade. Eine fehlende Datei ist ein
    harter Fehler (der vorherige Build-/Berichtsschritt haette sie erzeugen
    muessen); eine vorhandene, aber nicht als UTF-8 dekodierbare Datei gilt
    als Binaerdatei und wird nicht als Text durchsucht."""
    if not path.is_file():
        raise SystemExit(f"erwartetes Textartefakt fehlt: {path}")

    try:
        lines = path.read_text(encoding="utf-8", errors="strict").splitlines()
    except UnicodeDecodeError:
        return [], []

    secret_findings = scan_file_for_secrets(path)
    path_findings = scan_for_private_paths(path, lines)
    return secret_findings, path_findings


def check_repository(scan_paths: tuple[str, ...] = ()) -> int:
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

    scan_path_violations = []
    for scan_path in scan_paths:
        path = Path(scan_path)
        secret_findings, path_findings = scan_path_file(path)
        for line_number, line in secret_findings:
            scan_path_violations.append((scan_path, line_number, line))
            print(
                f"FAILED: moegliches Geheimnis in {scan_path}:{line_number}",
                file=sys.stderr,
            )
        for line_number, line in path_findings:
            scan_path_violations.append((scan_path, line_number, line))
            print(
                f"FAILED: privater absoluter Pfad in {scan_path}:{line_number}",
                file=sys.stderr,
            )

    if protected_violations or content_violations or scan_path_violations:
        print(
            f"FAILED: {len(protected_violations)} geschuetzte Datei(en) "
            f"eingecheckt, {len(content_violations)} verdaechtige Textstelle(n) "
            f"in getrackten Dateien, {len(scan_path_violations)} verdaechtige "
            "Textstelle(n) in --scan-path-Artefakten",
            file=sys.stderr,
        )
        return 1

    print(
        f"PASS: {len(files)} getrackte Dateien und {len(scan_paths)} "
        "--scan-path-Artefakt(e) geprueft, keine Geheimnisse oder privaten "
        "Pfade gefunden."
    )
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
        bad_password_file.write_text('pass' + 'word = "supersecretvalue"\n')

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

        # --scan-path: ungetrackte generierte Artefakte (Abschnitt 7.7.4)
        manifest_secret_file = tmp_path / "artifact-manifest-with-secret.json"
        manifest_secret_file.write_text(
            '{"token": "' + 'sk_live_abcdef1234567890"}\n'
        )
        manifest_private_path_file = tmp_path / "artifact-manifest-private-path.json"
        manifest_private_path_file.write_text(
            '{"note": "/home/exampleuser/private/build"}\n'
        )
        manifest_clean_file = tmp_path / "artifact-manifest-clean.json"
        manifest_clean_file.write_text(
            '{"profile": "esp32_bringup", "git_sha": "abc123", '
            '"idf_version": "v6.0.2", "idf_commit": "7101770dc"}\n'
        )
        compile_commands_ci_path_file = tmp_path / "compile_commands.json"
        compile_commands_ci_path_file.write_text(
            '[{"file": "/home/runner/work/repo/repo/src/main.cpp"}]\n'
        )
        compile_commands_credential_file_dir = tmp_path / "with_credential"
        compile_commands_credential_file_dir.mkdir()
        compile_commands_credential_file = (
            compile_commands_credential_file_dir / "compile_commands.json"
        )
        compile_commands_credential_file.write_text(
            # Literal per Konkatenation aufgeteilt, damit dieses Quellfile
            # sich nicht selbst bei der Repository-Geheimnispruefung meldet.
            '[{"file": "main.cpp", "define": "AWS_KEY=' + 'AKIA' + 'ABCDEFGHIJKLMNOP"}]\n'
        )
        build_log_ci_path_file = tmp_path / "build.log"
        build_log_ci_path_file.write_text(
            "Project build complete. To flash, run:\n"
            "  idf.py -B /home/runner/work/repo/repo/build/esp32_bringup flash\n"
        )
        missing_scan_path_file = tmp_path / "build" / "esp32_bringup" / "size.json"
        binary_scan_path_file = tmp_path / "fixture-binary.bin"
        binary_scan_path_file.write_bytes(b"\x7fELF\x00\x01\x02\xff\xfe\x00")

        manifest_secret_secrets, _ = scan_path_file(manifest_secret_file)
        _, manifest_private_paths = scan_path_file(manifest_private_path_file)
        manifest_clean_secrets, manifest_clean_paths = scan_path_file(manifest_clean_file)
        _, compile_commands_ci_paths = scan_path_file(compile_commands_ci_path_file)
        compile_commands_credential_secrets, _ = scan_path_file(
            compile_commands_credential_file
        )
        _, build_log_ci_paths = scan_path_file(build_log_ci_path_file)
        binary_secrets, binary_paths = scan_path_file(binary_scan_path_file)

        missing_scan_path_error = None
        try:
            scan_path_file(missing_scan_path_file)
        except SystemExit as error:
            missing_scan_path_error = error

        checks += [
            ("Geheimnis in ungetrackter --scan-path-Manifestdatei wird erkannt",
             len(manifest_secret_secrets) > 0),
            ("Privater absoluter Benutzerpfad in einer selbst erzeugten "
             "Manifestdatei wird erkannt", len(manifest_private_paths) > 0),
            ("Normalisierte, pfadfreie Manifestdatei wird akzeptiert",
             len(manifest_clean_secrets) == 0 and len(manifest_clean_paths) == 0),
            ("Bekannter ephemerer CI-Runner-Pfad in compile_commands.json "
             "wird NICHT als Fund gewertet", len(compile_commands_ci_paths) == 0),
            ("Token-/Credential-Muster in compile_commands.json wird "
             "weiterhin erkannt", len(compile_commands_credential_secrets) > 0),
            ("Bekannter ephemerer CI-Runner-Pfad in build.log wird NICHT "
             "als Fund gewertet", len(build_log_ci_paths) == 0),
            ("Fehlende, aber erwartete --scan-path-Datei fuehrt zu einem "
             "Fehler", missing_scan_path_error is not None),
            ("Binaerdatei wird nicht als Text gescannt (--scan-path)",
             len(binary_secrets) == 0 and len(binary_paths) == 0),
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
    parser.add_argument(
        "--scan-path", action="append", default=[], dest="scan_paths",
        metavar="PATH",
        help="Zusaetzliche, explizit benannte ungetrackte Textdatei, die "
             "ebenfalls auf Geheimnisse und private absolute Pfade geprueft "
             "wird (wiederholbar, Abschnitt 7.7.4).",
    )
    arguments = parser.parse_args()

    if arguments.selftest:
        return run_selftest()
    return check_repository(tuple(arguments.scan_paths))


if __name__ == "__main__":
    raise SystemExit(main())
