#!/usr/bin/env python3
"""Verhindert, dass ein in `.github/workflows/build.yml` hochgeladenes
Textartefakt aus der expliziten `check_secrets.py --scan-path`-Menge
herausfaellt (docs/tasks/issue-74-implementation-plan.md, Abschnitt 7.7.2/
7.7.4: "Alle vor actions/upload-artifact hochgeladenen Textartefakte werden
erfasst").

Bewusst kein YAML-Parser und keine neue Abhaengigkeit: die Struktur dieses
einen Workflows ist stabil und einfach (zwei Zeilenformen fuer `path:`,
ein stabiles `--scan-path <wert>`-Muster). Ein generischer CI-Contract-
Checker fuer beliebige Workflows waere hier Ueberabstraktion (KISS).

Geprueft wird jeder Step-Block, der `uses: actions/upload-artifact`
verwendet und NICHT `if: failure()` enthaelt (strukturelle Erkennung statt
einer festen Liste von Step-Namen — ein umbenannter oder neu hinzugefuegter
Erfolgs-Upload-Schritt faellt dadurch nicht unbemerkt aus der Pruefung
heraus). Der bestehende, nur bei Fehlschlag hochgeladene
`platformio-build-log` (Commit 1/2, unveraendert) ist damit bewusst
ausgenommen. Binaerdateien (`.bin`, `.elf`) sind laut Plan ohnehin vom
Textscan ausgenommen.
"""

import argparse
import re
import sys
from pathlib import Path

WORKFLOW_PATH = (
    Path(__file__).resolve().parent.parent / ".github" / "workflows" / "build.yml"
)

BINARY_SUFFIXES = {".bin", ".elf"}

UPLOAD_ARTIFACT_MARKER = "uses: actions/upload-artifact"
FAILURE_ONLY_MARKER = "if: failure()"

STEP_HEADER_PATTERN = re.compile(r"^      - name: (?P<name>.+)$", re.MULTILINE)


def is_success_upload_step(step_block: str) -> bool:
    return UPLOAD_ARTIFACT_MARKER in step_block and FAILURE_ONLY_MARKER not in step_block


def extract_scanned_paths(workflow_text: str) -> set[str]:
    return set(re.findall(r"--scan-path\s+(\S+)", workflow_text))


def split_into_steps(workflow_text: str) -> list[str]:
    return re.split(r"\n(?=      - name: )", workflow_text)


def extract_uploaded_text_paths(step_block: str) -> set[str]:
    """Liest den Wert von `path:` innerhalb eines einzelnen Step-Blocks,
    sowohl als Einzeiler (`path: datei`) als auch als YAML-Blockskalar
    (`path: |` gefolgt von eingerueckten Zeilen)."""
    path_key_match = re.search(r"\n\s*path:\s*(?P<inline>.*)", step_block)
    if not path_key_match:
        return set()

    inline_value = path_key_match.group("inline").strip()
    if inline_value and inline_value != "|":
        candidates = [inline_value]
    else:
        candidates = []
        rest = step_block[path_key_match.end():]
        for line in rest.splitlines():
            if not line.strip():
                continue
            if not line.startswith("            "):
                break
            candidates.append(line.strip())

    return {
        candidate for candidate in candidates
        if Path(candidate).suffix.lower() not in BINARY_SUFFIXES
    }


def find_scan_coverage_gaps(workflow_text: str) -> list[tuple[str, str]]:
    scanned_paths = extract_scanned_paths(workflow_text)

    gaps = []
    for step_block in split_into_steps(workflow_text):
        if not is_success_upload_step(step_block):
            continue
        header_match = STEP_HEADER_PATTERN.match(step_block)
        step_name = header_match["name"] if header_match else "<unbenannter Schritt>"
        for uploaded_path in sorted(extract_uploaded_text_paths(step_block)):
            if uploaded_path not in scanned_paths:
                gaps.append((step_name, uploaded_path))
    return gaps


def count_success_upload_steps(workflow_text: str) -> int:
    return sum(
        1 for step_block in split_into_steps(workflow_text)
        if is_success_upload_step(step_block)
    )


def check_repository() -> int:
    if not WORKFLOW_PATH.is_file():
        raise SystemExit(f"Workflow-Datei fehlt: {WORKFLOW_PATH}")

    workflow_text = WORKFLOW_PATH.read_text(encoding="utf-8")
    gaps = find_scan_coverage_gaps(workflow_text)

    if gaps:
        for step_name, path in gaps:
            print(
                f"FAILED: '{path}' wird in Schritt '{step_name}' hochgeladen, "
                "aber nicht per --scan-path geprueft",
                file=sys.stderr,
            )
        print(f"FAILED: {len(gaps)} ungeprueft hochgeladene Textartefakt(e)",
              file=sys.stderr)
        return 1

    step_count = count_success_upload_steps(workflow_text)
    print(
        f"PASS: alle hochgeladenen Textartefakte in {step_count} bei Erfolg "
        "hochladenden Schritten (uses: actions/upload-artifact ohne "
        "if: failure()) sind durch --scan-path abgedeckt."
    )
    return 0


def run_selftest() -> int:
    covered_workflow = """\
jobs:
  firmware:
    steps:
      - name: Secret- und Pfadpruefung generierter Artefakte
        run: |
          python scripts/check_secrets.py \\
            --scan-path build-report.md \\
            --scan-path build/esp32_bringup/fixture.map

      - name: Groessenbericht sichern
        if: success()
        uses: actions/upload-artifact@fixture
        with:
          name: build-report
          path: build-report.md

      - name: ESP-IDF-Bringup-Artefakte sichern
        if: success()
        uses: actions/upload-artifact@fixture
        with:
          name: esp-idf-esp32_bringup-fixture
          path: |
            build/esp32_bringup/app.bin
            build/esp32_bringup/app.elf
            build/esp32_bringup/fixture.map
"""

    uncovered_workflow = covered_workflow.replace(
        "--scan-path build/esp32_bringup/fixture.map\n", "",
    )

    unrelated_step_workflow = covered_workflow + """\
      - name: Fehlgeschlagenen Buildlog sichern
        if: failure()
        uses: actions/upload-artifact@fixture
        with:
          name: platformio-build-log
          path: platformio-build.log
"""

    renamed_uncovered_workflow = uncovered_workflow.replace(
        "ESP-IDF-Bringup-Artefakte sichern", "Voellig anders benannter Schritt",
    )

    checks = [
        (
            "Vollstaendig gescannter Workflow meldet keine Luecke",
            find_scan_coverage_gaps(covered_workflow) == [],
        ),
        (
            "Hochgeladene, aber ungescannte Textdatei wird als Luecke erkannt",
            find_scan_coverage_gaps(uncovered_workflow)
            == [("ESP-IDF-Bringup-Artefakte sichern", "build/esp32_bringup/fixture.map")],
        ),
        (
            "Binaerdateien (.bin/.elf) werden nicht als Luecke gewertet",
            all(
                not path.endswith((".bin", ".elf"))
                for _, path in find_scan_coverage_gaps(uncovered_workflow)
            ),
        ),
        (
            "Nur bei Fehlschlag hochgeladene Artefakte (if: failure()) "
            "werden nicht verlangt",
            find_scan_coverage_gaps(unrelated_step_workflow)
            == find_scan_coverage_gaps(covered_workflow),
        ),
        (
            "Ein umbenannter Erfolgs-Upload-Schritt wird weiterhin geprueft "
            "(strukturelle Erkennung, keine feste Namensliste)",
            find_scan_coverage_gaps(renamed_uncovered_workflow)
            == [("Voellig anders benannter Schritt", "build/esp32_bringup/fixture.map")],
        ),
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
        help="Prueft die Luecken-Erkennung selbst anhand synthetischer "
             "Workflow-Fixtures, ohne die reale build.yml zu lesen.",
    )
    arguments = parser.parse_args()

    if arguments.selftest:
        return run_selftest()
    return check_repository()


if __name__ == "__main__":
    raise SystemExit(main())
