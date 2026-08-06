# CI und Qualitaets-Gates

## Zweck

Dieses Dokument ist die kanonische Quelle fuer Testbefehle, Buildprofile,
Werkzeuge, CI-Ausloesung und die Ergebnisbegriffe `PASS`, `FAILED` und
`BLOCKED`.

Der native Hostpfad verwendet PlatformIO `6.1.19`. Die ESP32-Produktionsprofile
verwenden ESP-IDF `v6.0.2` am Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e`.

## Ausfuehrungszeitpunkt

### Planung

Keine Builds und keine vollstaendigen Testlaeufe. Zulaessig sind nur
Repository-, Diff-, Link- und Dokumentationspruefungen, die den Plan
unterstuetzen.

### Draft-Umsetzung

Nur gezielte lokale Tests und Pruefungen fuer den tatsaechlich geaenderten
Bereich. Nicht betroffene Profile und der vollstaendige Gesamtlauf werden nicht
ritualistisch wiederholt.

### Vollstaendiger lokaler Lauf

Nur wenn:

- das vollstaendige Review keine offenen Befunde mehr hat;
- der zu pruefende `HEAD` final ist;
- der Owner den Lauf ausdruecklich anordnet.

### GitHub-CI

`.github/workflows/build.yml` reagiert auf:

- `pull_request.ready_for_review`;
- `pull_request.synchronize`;
- `pull_request.reopened`.

Der Firmwarejob laeuft nur, wenn der Pull Request kein Draft ist. Draft-Pushes
koennen einen sofort uebersprungenen Workfloweintrag erzeugen, fuehren aber
keine Builds oder Tests aus.

Reine Markdown-Aenderungen sind durch `paths-ignore` von der vollstaendigen
Firmware-CI ausgenommen. Es gibt keinen `push`-Trigger und damit keinen
identischen automatischen Wiederholungslauf nach dem Merge.

## Buildprofile

| Profil | Werkzeug | Zweck |
|---|---|---|
| `native` | PlatformIO/Host-Compiler | Fachlogik, Simulation und native Tests |
| `esp32_bringup` | ESP-IDF 6.0.2 | Produktionsbuild mit gesperrten Aktoren und unbestaetigter Hardware |
| `esp32_release` | ESP-IDF 6.0.2 | Releaseprofil; keine automatische Hardwarefreigabe |

`src/main.cpp` ist der native Composition Root. `main/app_main.cpp` ist der
ESP-IDF Composition Root.

## Gezielte lokale Pruefungen

Der konkrete Umfang wird aus Diff, Plan und betroffenen Tests abgeleitet.
Beispiele:

```bash
pio test -e native --filter <test-verzeichnis-oder-muster>
clang-format --dry-run --Werror <geaenderte-cpp-hpp-h-dateien>
python scripts/check_architecture_boundaries.py
python scripts/check_secrets.py
git diff --check
```

Ein gezielter Lauf muss im PR mit Befehl, Umfang und Ergebnis dokumentiert
werden.

## Vollstaendiger nativer Lauf

```bash
pio run -e native
pio test -e native
python scripts/check_architecture_boundaries.py
python scripts/check_secrets.py
python scripts/selftest_quality_gates.py
```

## ESP-IDF-Profile

Nach Aktivierung der festgelegten Toolchain:

```bash
. "$IDF_PATH/export.sh"
python scripts/build_esp_idf_profiles.py all
python scripts/run_esp_idf_static_analysis.py all
```

Der Upgrade-, Herkunfts- und Hardware-Smoke-Vertrag steht in
`ESP_IDF_UPGRADE_CONTRACT.md`.

## Formatierung und Static Analysis

| Werkzeug | Version | Umfang |
|---|---:|---|
| clang-format | 18 | C/C++ unter `src/`, `include/`, `lib/`, `test/`, `main/` |
| clang-tidy | 18.1.8 | hardwareunabhaengiger Produktionskern ueber die native Kompilierungsdatenbank |
| esp-clang | zur ESP-IDF-6.0.2-Toolchain passend | beide ESP-IDF-Profile |

Vollstaendige Formatpruefung:

```bash
clang-format --dry-run --Werror \
  $(find src include lib test main -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \))
```

Native Kompilierungsdatenbank:

```bash
pio run -e native -t compiledb
```

Die kanonische clang-tidy-Dateiliste und die CI-Schritte stehen im
versionierten Workflow `.github/workflows/build.yml`. Eine punktuelle
Unterdrueckung verwendet ausschliesslich:

```cpp
// NOLINT(check-name): konkrete Begruendung
```

## Architektur-, Secret- und Gate-Selbsttests

```bash
python scripts/check_architecture_boundaries.py
python scripts/check_secrets.py
python scripts/selftest_quality_gates.py
python scripts/check_ci_artifact_scan_coverage.py
```

Der Architekturguard erzwingt die in ADR-013 festgelegte Richtung und ersetzt
kein vollstaendiges Architekturreview.

Die Secretpruefung kontrolliert getrackte Textdateien, lokale Konfigurationen
und in CI zusaetzlich die erzeugten Artefakte. Beispielkonfigurationen duerfen
nur klar erkennbare Platzhalter enthalten.

Der Gate-Selbsttest beweist anhand temporaerer fehlerhafter Fixtures, dass die
Qualitaetspruefungen echte Verstoesse erkennen.

## Determinismus und Zeit

Native Tests verwenden keine reale Uhrzeit und keine Netzwerkabhaengigkeit.
`ITimeSource` trennt monotone Laufzeit von optionaler UTC-Zeit.
`VirtualTimeSource` schreitet nur durch explizite Testaktionen voran; ein
Neustart wird durch eine neue Instanz simuliert.

Der ESP-IDF-Adapter `EspTimerTimeSource` liefert monotone Laufzeit ueber
`esp_timer`. Eine fachliche Nutzung wird nur behauptet, wenn ein realer
Produktionskonsument existiert.

## Ressourcenbericht und Artefakte

```bash
python scripts/build_report.py --output build-report.md
. "$IDF_PATH/export.sh"
python scripts/build_report.py --output build-report.md --append \
  --esp-idf-profiles bringup release
```

Reale Byte-, Heap-, Partitions- und Puffergrenzen bleiben
`TBD_IMPLEMENTATION_BUDGET`, bis reale Builds und Belastungsmessungen
vorliegen.

Bei erfolgreicher GitHub-CI werden getrennt gesichert:

- finaler Ressourcenbericht;
- Bring-up-Binaer-, ELF-, Map-, Bootloader-, Partition-, Konfigurations-,
  Compile-Database-, Groessen-, Log- und Manifestartefakte;
- entsprechende Releaseartefakte.

Fehlgeschlagene Builds sichern den verfuegbaren Buildlog.

## CI-Pipeline

Der Firmwarejob fuehrt in dieser Reihenfolge aus:

1. Checkout und Python;
2. PlatformIO, clang-format und clang-tidy installieren;
3. Formatpruefung;
4. nativen Build und Ressourcenbericht;
5. native Tests;
6. native Static Analysis;
7. Architektur-, Secret- und Gate-Selbsttests;
8. ESP-IDF 6.0.2 am exakten Commit installieren und verifizieren;
9. Bring-up- und Releaseprofil bauen;
10. Ressourcenbericht ergaenzen;
11. ESP-IDF-Static-Analysis;
12. Artefakt-Scanabdeckung und Secretpruefung;
13. Berichte und Buildartefakte sichern.

`concurrency` bricht einen veralteten Lauf desselben Pull Requests ab, sobald
ein neuerer Lauf startet.

## Ergebnisstatus

- **PASS:** Pruefung wurde ausgefuehrt und war erfolgreich.
- **FAILED:** Pruefung wurde ausgefuehrt, ist fehlgeschlagen und blockiert den
  Abschluss.
- **BLOCKED:** Pruefung konnte wegen einer konkret benannten fehlenden
  Voraussetzung nicht ausgefuehrt werden.

`SKIPPED`, `NOT_RUN` oder eine fehlende Angabe duerfen nicht als `PASS`
bezeichnet werden.

## Ausnahmen

Jede Ausnahme benoetigt eine konkrete Begruendung:

- projektweit in Werkzeugkonfiguration oder diesem Dokument;
- punktuell direkt an der Unterdrueckung;
- auftragsbezogen im freigegebenen Plan und PR-Nachweis.

Unbegruendete Unterdrueckungen von Safety-, Security-, Architektur- oder
Kernfunktionstests sind unzulaessig.
