#!/usr/bin/env python3
"""Baut die angegebenen PlatformIO-Profile und/oder liest bereits gebaute
ESP-IDF-Profile aus und erzeugt einen Firmware- und Ressourcen-Groessenbericht.

Der Bericht ist informativ. Verbindliche Byte-Budgets sind laut
`docs/OPEN_POINTS.md` weiterhin `TBD_IMPLEMENTATION_BUDGET` und werden hier
nicht erfunden.

Zweiphasiger Uebergang (docs/tasks/issue-74-implementation-plan.md,
Abschnitt 7.7.5): Phase 1 (Commit 3, dieser Stand) erzeugt einen
Parallelbericht mit klar unterschiedenen Ueberschriften fuer Arduino-
/PlatformIO- und ESP-IDF-Messungen, damit beide Wertereihen nie unter
derselben Bezeichnung vermischt werden. Der PlatformIO-Teil und der
ESP-IDF-Teil koennen in getrennten Aufrufen in dieselbe Datei geschrieben
werden (`--append`), sodass die CI den ESP-IDF-Teil erst nach den
ESP-IDF-Builds ergaenzen kann, ohne den bestehenden fruehen
PlatformIO-Buildschritt (Commit 1/2) umzustrukturieren. Phase 2 (Commit 5)
stellt im selben Commit, der den Arduino-Pfad entfernt, auf einen
ESP-IDF-only-Bericht um.

Fuer ESP-IDF-Profile werden ausschliesslich die offiziellen, von `idf.py`
generierten Dateien ausgewertet (`size --format json2`,
`project_description.json`, `flasher_args.json`) — keine eigene
Reimplementierung der ESP-IDF-Groessenberechnung. Fehlt ein Profil oder ein
erwartetes Artefakt, ist das ein harter Fehler.
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Optional

import esp_idf_contract

DEFAULT_ENVIRONMENTS = ("native", "esp32_bringup", "esp32_release")

SIZE_BLOCK_PATTERN = re.compile(
    r"Checking size \.pio/build/(?P<env>[\w-]+)/firmware\.elf\s*\n"
    r"(?:.*\n)*?RAM:\s+\[.*?\]\s+(?P<ram_percent>[\d.]+)% "
    r"\(used (?P<ram_used>\d+) bytes from (?P<ram_total>\d+) bytes\)\s*\n"
    r"Flash:\s+\[.*?\]\s+(?P<flash_percent>[\d.]+)% "
    r"\(used (?P<flash_used>\d+) bytes from (?P<flash_total>\d+) bytes\)",
)


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def run_pio_build(environments: tuple[str, ...], pio: str) -> str:
    arguments = [pio, "run"]
    for environment in environments:
        arguments += ["-e", environment]

    result = subprocess.run(arguments, text=True, capture_output=True)
    print(result.stdout)
    print(result.stderr, file=sys.stderr)
    if result.returncode != 0:
        raise SystemExit(result.returncode)
    return result.stdout


def parse_esp32_size_reports(build_output: str) -> dict[str, dict[str, object]]:
    reports = {}
    for match in SIZE_BLOCK_PATTERN.finditer(build_output):
        reports[match["env"]] = {
            "ram_percent": float(match["ram_percent"]),
            "ram_used_bytes": int(match["ram_used"]),
            "ram_total_bytes": int(match["ram_total"]),
            "flash_percent": float(match["flash_percent"]),
            "flash_used_bytes": int(match["flash_used"]),
            "flash_total_bytes": int(match["flash_total"]),
        }
    return reports


def artifact_size(path: str) -> Optional[int]:
    return os.path.getsize(path) if os.path.isfile(path) else None


def platformio_report_lines(heading: str, environment: str,
                             esp32_reports: dict[str, dict[str, object]]) -> list[str]:
    lines = [f"## {heading}", ""]

    if environment in esp32_reports:
        report = esp32_reports[environment]
        lines.append(
            f"- RAM: {report['ram_used_bytes']} / {report['ram_total_bytes']} Bytes "
            f"({report['ram_percent']}%)"
        )
        lines.append(
            f"- Flash: {report['flash_used_bytes']} / {report['flash_total_bytes']} Bytes "
            f"({report['flash_percent']}%)"
        )
        elf_size = artifact_size(f".pio/build/{environment}/firmware.elf")
        bin_size = artifact_size(f".pio/build/{environment}/firmware.bin")
        if elf_size is not None:
            lines.append(f"- firmware.elf: {elf_size} Bytes")
        if bin_size is not None:
            lines.append(f"- firmware.bin: {bin_size} Bytes")
    else:
        program_size = artifact_size(f".pio/build/{environment}/program")
        if program_size is not None:
            lines.append(
                f"- Host-Testbinaer (`program`): {program_size} Bytes "
                "(kein Flash-/RAM-Budget, da hardwareunabhaengig)"
            )
        else:
            lines.append("- kein Groessenartefakt gefunden")
    lines.append("")
    return lines


# --- ESP-IDF-Zweig (Abschnitt 7.7.1/7.7.2/7.7.5 Phase 1) -------------------


def required_json(path: Path) -> dict:
    if not path.is_file():
        raise SystemExit(f"erwartete ESP-IDF-Artefaktdatei fehlt: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def required_artifact_size(path: Path) -> int:
    size = artifact_size(str(path))
    if size is None:
        raise SystemExit(f"erwartetes ESP-IDF-Artefakt fehlt: {path}")
    return size


def sha256_of(path: Path) -> str:
    if not path.is_file():
        raise SystemExit(f"erwartete ESP-IDF-Artefaktdatei fehlt: {path}")
    digest = hashlib.sha256()
    digest.update(path.read_bytes())
    return digest.hexdigest()


def layout_entry(size_data: dict, name: str) -> dict:
    for entry in size_data.get("layout", []):
        if entry.get("name") == name:
            return entry
    raise SystemExit(f"size.json enthaelt keinen Layout-Eintrag '{name}'")


def esp_idf_build_dir(profile: str) -> Path:
    return repo_root() / "build" / esp_idf_contract.build_dir_name(profile)


def git_head_sha() -> str:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=repo_root(),
        capture_output=True, text=True, check=True,
    )
    return result.stdout.strip()


def generate_esp_idf_build_log(build_dir: Path, *, idf_py: str) -> Path:
    """Erzeugt ein Buildlog fuer das Artefaktbuendel (Abschnitt 7.7.2). Der
    zugrunde liegende Build wurde bereits vom kanonischen Buildtreiber
    (`scripts/build_esp_idf_profiles.py`, Commit 1/2) durchgefuehrt; dieser
    erneute `idf.py build`-Aufruf ist dank Ninja inkrementell (kein
    Neubau) und dient ausschliesslich dazu, ein reales, aktuelles Log als
    Artefakt festzuhalten, ohne den bereits freigegebenen Buildtreiber oder
    dessen CI-Schritt zu veraendern."""
    log_path = build_dir / "build.log"
    command = [idf_py, "-B", str(build_dir), "build"]
    result = subprocess.run(command, cwd=repo_root(), capture_output=True, text=True)
    log_path.write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode != 0:
        raise SystemExit(
            f"idf.py build (Buildlog-Nachweis) fehlgeschlagen fuer "
            f"{build_dir.name} (Exit-Code {result.returncode}); siehe {log_path}"
        )
    return log_path


def generate_esp_idf_size_json(build_dir: Path, *, idf_py: str) -> Path:
    size_json_path = build_dir / "size.json"
    command = [
        idf_py, "-B", str(build_dir), "size",
        "--format", "json2", "--output-file", str(size_json_path),
    ]
    result = subprocess.run(command, cwd=repo_root(), capture_output=True, text=True)
    if result.returncode != 0 or not size_json_path.is_file():
        raise SystemExit(
            f"idf.py size --format json2 fehlgeschlagen fuer {build_dir.name} "
            f"(Exit-Code {result.returncode}): {result.stderr.strip()}"
        )
    return size_json_path


def build_esp_idf_profile_report(profile: str, build_dir: Path, *,
                                  idf_py: str = "idf.py") -> dict:
    if not build_dir.is_dir():
        raise SystemExit(
            f"ESP-IDF-Buildverzeichnis fehlt fuer Profil {profile}: {build_dir} "
            "(zuerst scripts/build_esp_idf_profiles.py ausfuehren)"
        )

    generate_esp_idf_build_log(build_dir, idf_py=idf_py)
    size_json_path = generate_esp_idf_size_json(build_dir, idf_py=idf_py)
    size_data = required_json(size_json_path)
    description = required_json(build_dir / "project_description.json")
    flasher_args = required_json(build_dir / "flasher_args.json")

    app_bin = build_dir / description["app_bin"]
    app_elf = build_dir / description["app_elf"]
    app_map = app_elf.with_suffix(".map")
    bootloader_bin = build_dir / flasher_args["bootloader"]["file"]
    partition_table_bin = build_dir / flasher_args["partition-table"]["file"]

    dram = layout_entry(size_data, "DRAM")
    iram = layout_entry(size_data, "IRAM")

    return {
        "profile": profile,
        "build_dir": build_dir,
        "total_size": size_data["total_size"],
        "dram_used": dram["used"],
        "dram_total": dram["total"],
        "iram_used": iram["used"],
        "iram_total": iram["total"],
        "app_bin_name": description["app_bin"],
        "app_bin_size": required_artifact_size(app_bin),
        "elf_size": required_artifact_size(app_elf),
        "map_size": required_artifact_size(app_map),
        "bootloader_bin_size": required_artifact_size(bootloader_bin),
        "partition_table_bin_size": required_artifact_size(partition_table_bin),
        "sdkconfig_sha256": sha256_of(build_dir / "sdkconfig"),
        "idf_tag": esp_idf_contract.ESP_IDF_TAG,
        "idf_commit": esp_idf_contract.ESP_IDF_COMMIT,
        "git_sha": git_head_sha(),
    }


def esp_idf_report_lines(profile: str, report: dict) -> list[str]:
    build_name = esp_idf_contract.build_dir_name(profile)
    lines = [f"## ESP-IDF {build_name} (JSON2-basierte IDF-Messung)", ""]
    lines.append(f"- Profil: {build_name}")
    lines.append(f"- ESP-IDF-Tag: {report['idf_tag']}")
    lines.append(f"- ESP-IDF-Commit: {report['idf_commit']}")
    lines.append(f"- Build-Commit: {report['git_sha']}")
    lines.append(
        f"- Gesamter Flashverbrauch (size.json total_size): "
        f"{report['total_size']} Bytes"
    )
    lines.append(f"- DRAM: {report['dram_used']} / {report['dram_total']} Bytes")
    lines.append(f"- IRAM: {report['iram_used']} / {report['iram_total']} Bytes")
    lines.append(f"- App-BIN ({report['app_bin_name']}): {report['app_bin_size']} Bytes")
    lines.append(f"- ELF: {report['elf_size']} Bytes")
    lines.append(f"- Mapfile: {report['map_size']} Bytes")
    lines.append(f"- Bootloader-BIN: {report['bootloader_bin_size']} Bytes")
    lines.append(f"- Partitionstabellen-BIN: {report['partition_table_bin_size']} Bytes")
    lines.append(f"- sdkconfig SHA-256: {report['sdkconfig_sha256']}")
    lines.append("")
    return lines


def write_artifact_manifest(report: dict) -> Path:
    """Kurzer Manifestbericht laut Abschnitt 7.7.2 (Profil, Git-SHA,
    IDF-Version, IDF-Commit) — bewusst kurz und getrennt vom ausfuehrlichen
    Markdown-Bericht, keine Redundanz der vollstaendigen Groessendaten."""
    manifest_path = report["build_dir"] / "artifact-manifest.json"
    manifest = {
        "profile": esp_idf_contract.build_dir_name(report["profile"]),
        "git_sha": report["git_sha"],
        "idf_version": report["idf_tag"],
        "idf_commit": report["idf_commit"],
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest_path


# --- Selftest ---------------------------------------------------------------


def write_fake_idf_py(directory: Path) -> str:
    """Schreibt einen minimalen `idf.py`-Ersatz fuer den Selftest, damit
    `build_esp_idf_profile_report()` real (kein separater Mockpfad) durchlaufen
    werden kann, ohne eine echte ESP-IDF-Installation zu benoetigen."""
    fake_idf_py = directory / "idf.py"
    fake_idf_py.write_text(
        "#!/usr/bin/env python3\n"
        "import json, sys\n"
        "args = sys.argv[1:]\n"
        "if 'size' in args:\n"
        "    out = args[args.index('--output-file') + 1]\n"
        "    with open(out, 'w', encoding='utf-8') as f:\n"
        "        json.dump({\n"
        "            'version': '1.2',\n"
        "            'total_size': 99999999,\n"
        "            'layout': [\n"
        "                {'name': 'DRAM', 'total': 180736, 'used': 12524, "
        "'free': 168212, 'parts': {}},\n"
        "                {'name': 'IRAM', 'total': 131072, 'used': 41951, "
        "'free': 89121, 'parts': {}},\n"
        "            ],\n"
        "        }, f)\n"
        "elif 'build' in args:\n"
        "    pass\n"
        "else:\n"
        "    sys.exit(2)\n"
        "sys.exit(0)\n",
        encoding="utf-8",
    )
    fake_idf_py.chmod(0o755)
    return str(fake_idf_py)


def populate_fake_esp_idf_build_dir(build_dir: Path) -> None:
    build_dir.mkdir(parents=True, exist_ok=True)
    (build_dir / "bootloader").mkdir(exist_ok=True)
    (build_dir / "partition_table").mkdir(exist_ok=True)

    (build_dir / "project_description.json").write_text(json.dumps({
        "app_elf": "fixture.elf", "app_bin": "fixture.bin",
    }), encoding="utf-8")
    (build_dir / "flasher_args.json").write_text(json.dumps({
        "bootloader": {"file": "bootloader/bootloader.bin"},
        "partition-table": {"file": "partition_table/partition-table.bin"},
    }), encoding="utf-8")
    (build_dir / "fixture.bin").write_bytes(b"\x00" * 32)
    (build_dir / "fixture.elf").write_bytes(b"\x00" * 64)
    (build_dir / "fixture.map").write_text("fixture map\n", encoding="utf-8")
    (build_dir / "bootloader" / "bootloader.bin").write_bytes(b"\x00" * 16)
    (build_dir / "partition_table" / "partition-table.bin").write_bytes(b"\x00" * 8)
    (build_dir / "sdkconfig").write_text("CONFIG_FIXTURE=y\n", encoding="utf-8")


def run_selftest() -> int:
    checks: list[tuple[str, bool]] = []

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        fake_idf_py = write_fake_idf_py(tmp_path)

        build_dir = tmp_path / "esp32_bringup"
        populate_fake_esp_idf_build_dir(build_dir)

        report = build_esp_idf_profile_report("bringup", build_dir, idf_py=fake_idf_py)
        checks.append((
            "ESP-IDF-Profilbericht liest total_size aus size.json",
            report["total_size"] == 99999999,
        ))

        arduino_reports = {
            "esp32_bringup": {
                "ram_percent": 12.3, "ram_used_bytes": 1111, "ram_total_bytes": 327680,
                "flash_percent": 4.5, "flash_used_bytes": 2222, "flash_total_bytes": 4194304,
            },
        }
        combined_lines = (
            platformio_report_lines(
                "Arduino/PlatformIO esp32_bringup (letzter Arduino-Vergleichspfad)",
                "esp32_bringup", arduino_reports,
            )
            + esp_idf_report_lines("bringup", report)
        )
        combined_text = "\n".join(combined_lines)

        checks.append((
            "Bericht enthaelt getrennte Arduino- und ESP-IDF-Ueberschriften "
            "fuer dasselbe Profil",
            "## Arduino/PlatformIO esp32_bringup" in combined_text
            and "## ESP-IDF esp32_bringup" in combined_text,
        ))

        arduino_section, _, esp_idf_section = combined_text.partition("## ESP-IDF")
        checks.append((
            "Arduino-Messwert erscheint nur im Arduino-Abschnitt, nicht im "
            "ESP-IDF-Abschnitt",
            "1111" in arduino_section and "1111" not in esp_idf_section,
        ))
        checks.append((
            "ESP-IDF-Messwert erscheint nur im ESP-IDF-Abschnitt, nicht im "
            "Arduino-Abschnitt",
            "99999999" in esp_idf_section and "99999999" not in arduino_section,
        ))

        manifest_path = write_artifact_manifest(report)
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        checks.append((
            "Artefaktmanifest enthaelt genau Profil, Git-SHA, IDF-Version, "
            "IDF-Commit",
            set(manifest.keys()) == {"profile", "git_sha", "idf_version", "idf_commit"}
            and manifest["idf_version"] == esp_idf_contract.ESP_IDF_TAG,
        ))

        missing_dir = tmp_path / "esp32_release"
        error_on_missing_dir = None
        try:
            build_esp_idf_profile_report("release", missing_dir, idf_py=fake_idf_py)
        except SystemExit as error:
            error_on_missing_dir = error
        checks.append((
            "Fehlendes ESP-IDF-Buildverzeichnis fuehrt zu einem harten Fehler",
            error_on_missing_dir is not None,
        ))

        incomplete_dir = tmp_path / "esp32_incomplete"
        populate_fake_esp_idf_build_dir(incomplete_dir)
        (incomplete_dir / "flasher_args.json").unlink()
        error_on_missing_artifact = None
        try:
            build_esp_idf_profile_report("bringup", incomplete_dir, idf_py=fake_idf_py)
        except SystemExit as error:
            error_on_missing_artifact = error
        checks.append((
            "Fehlendes erwartetes ESP-IDF-Artefakt (hier: flasher_args.json) "
            "fuehrt zu einem harten Fehler",
            error_on_missing_artifact is not None,
        ))

    all_passed = True
    for description, passed in checks:
        status = "PASS" if passed else "FAILED"
        print(f"{status}: {description}")
        all_passed = all_passed and passed
    return 0 if all_passed else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pio", default="pio", help="PlatformIO executable")
    parser.add_argument(
        "--idf-py", default="idf.py",
        help="idf.py-Aufruf fuer die ESP-IDF-Groessendaten (muss auf PATH "
             "liegen oder als Pfad angegeben werden, z. B. nach `. $IDF_PATH/export.sh`)",
    )
    parser.add_argument(
        "--output", default="build-report.md",
        help="Zieldatei fuer den Markdown-Bericht",
    )
    parser.add_argument(
        "--append", action="store_true",
        help="An eine bestehende --output-Datei anhaengen statt sie zu "
             "ueberschreiben (Abschnitt 7.7.5: ergaenzt denselben "
             "Parallelbericht um die ESP-IDF-Messung, ohne den fruehen "
             "PlatformIO-Buildschritt umzustrukturieren)",
    )
    parser.add_argument(
        "environments", nargs="*", default=(),
        help="Zu bauende PlatformIO-Umgebungen (leer lassen, wenn nur "
             "--esp-idf-profiles ausgewertet werden soll)",
    )
    parser.add_argument(
        "--esp-idf-profiles", nargs="*", default=(),
        choices=list(esp_idf_contract.PROFILES),
        help="Bereits gebaute ESP-IDF-Profile, deren Groessendaten und "
             "Artefaktmanifeste zusaetzlich in den Parallelbericht "
             "aufgenommen werden (Abschnitt 7.7.1/7.7.2)",
    )
    parser.add_argument(
        "--selftest", action="store_true",
        help="Prueft die Berichts- und Manifesterzeugung anhand temporaerer "
             "Fixtures, ohne eine echte ESP-IDF-Installation zu benoetigen.",
    )
    arguments = parser.parse_args()

    if arguments.selftest:
        return run_selftest()

    environments = tuple(arguments.environments)
    report_lines: list[str] = []

    if not arguments.append:
        report_lines += [
            "# Firmware- und Ressourcen-Groessenbericht",
            "",
            "Informativ. Verbindliche Byte-Budgets bleiben `TBD_IMPLEMENTATION_BUDGET` "
            "bis zu realen Hardware- und Belastungsmessungen (siehe `docs/OPEN_POINTS.md`).",
            "",
        ]

    if environments:
        build_output = run_pio_build(environments, arguments.pio)
        esp32_reports = parse_esp32_size_reports(build_output)
        for environment in environments:
            if environment == "native":
                heading = environment
            else:
                heading = f"Arduino/PlatformIO {environment} (letzter Arduino-Vergleichspfad)"
            report_lines += platformio_report_lines(heading, environment, esp32_reports)

    for profile in arguments.esp_idf_profiles:
        build_dir = esp_idf_build_dir(profile)
        report = build_esp_idf_profile_report(profile, build_dir, idf_py=arguments.idf_py)
        report_lines += esp_idf_report_lines(profile, report)
        write_artifact_manifest(report)

    report_text = "\n".join(report_lines) + "\n"
    mode = "a" if arguments.append else "w"
    with open(arguments.output, mode, encoding="utf-8") as report_file:
        report_file.write(report_text)

    print(report_text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
