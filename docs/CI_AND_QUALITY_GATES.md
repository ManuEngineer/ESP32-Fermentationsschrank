# CI und Qualitaets-Gates

## Zweck

Dieses Dokument ist die kanonische Quelle fuer Ausfuehrungszeitpunkt,
Buildprofile, Werkzeugvertraege, CI-Ausloesung und die Ergebnisbegriffe
`PASS`, `FAILED` und `BLOCKED`. Die vollstaendigen ausfuehrbaren Befehle fuer
die gemeinsamen portablen Runner-Gates und die clang-tidy-Dateiliste stehen
ausschliesslich im versionierten Runner
`scripts/run_pre_ready_gates.sh`; GitHub-CI-only Gates stehen im Workflow.

Der native Hostpfad verwendet PlatformIO `6.1.19`. Die ESP32-Produktionsprofile
verwenden ESP-IDF `v6.0.2` am Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e`.

Der gemeinsame versionierte Gate-Owner ist
`scripts/run_pre_ready_gates.sh`. Er verifiziert vor den Gates PlatformIO
`6.1.19`, clang-format und clang-tidy aus der Major-Linie 18 sowie fuer die
ESP-Phase die bestehende exakte ESP-IDF-/esp-clang-Provenienz. Installation und
Provisionierung der Werkzeuge bleiben Umgebungsaufgabe; eine Abweichung wird
vom Runner als `BLOCKED` oder `FAILED` behandelt und kann keinen lokalen
Pre-Ready-PASS erzeugen.

Der Runner enthaelt ausschliesslich portable Engineering-Gates, die lokal und
in GitHub-CI denselben Checkout unabhaengig vom GitHub-Artefakttransport pruefen.
GitHub-, Upload-, Artefakt- und Privacy-Gates bleiben ausschliesslich im
Workflow. Der lokale Pre-Ready-Lauf emuliert weder GitHub noch den
Artefakt-Upload.

## Ausfuehrungszeitpunkt

### Planung

Keine Builds und keine vollstaendigen Testlaeufe. Zulaessig sind nur
Repository-, Diff-, Link- und Dokumentationspruefungen, die den Plan
unterstuetzen.

### Draft-Umsetzung

Nur gezielte lokale Tests und Pruefungen fuer den tatsaechlich geaenderten
Bereich. Bei geaenderten gemeinsamen Vertraegen gehoeren die direkt betroffenen
Konsumententests zum gezielten Umfang. Nicht betroffene Profile und der
vollstaendige Gesamtlauf werden nicht ritualistisch wiederholt.

### Vollstaendiger lokaler Lauf

Nur wenn:

- der vollstaendige Independent Full Review abgeschlossen ist
  (`FULL_REVIEW_COMPLETE`);
- `OPEN_BLOCKERS=0` gilt; klassifizierte `FOLLOW-UP` und `NO-ACTION` blockieren
  den Lauf nicht;
- der zu pruefende `HEAD` final ist;
- der Owner den Lauf ausdruecklich anordnet.

Der autorisierte Lauf wird danach auf demselben finalen `HEAD` in zwei Phasen
ausgefuehrt. Der Runner ist die einzige Quelle fuer die gemeinsamen portablen
Gatebefehle und die clang-tidy-Dateiliste:

```bash
export PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)"
bash scripts/run_pre_ready_gates.sh host

# Danach die kanonische ESP-IDF-6.0.2-/esp-clang-Umgebung bereitstellen und
# export.sh aktivieren; dies ist Provisionierung, kein zweiter Gatepfad.
export IDF_TOOLS_PATH="${IDF_TOOLS_PATH:-$HOME/.espressif}"
python3 "$IDF_PATH/tools/idf_tools.py" install esp-clang
. "$IDF_PATH/export.sh"

bash scripts/run_pre_ready_gates.sh esp
```

Bei einer normalen lokalen ESP-IDF-Installation verwendet diese Zuweisung den
Default `$HOME/.espressif`, ohne einen bereits explizit gesetzten Pfad zu
ueberschreiben. `IDF_TOOLS_PATH` muss vor `idf_tools.py install esp-clang` auf
ein vorhandenes Verzeichnis zeigen. Der Runner prueft diesen Pfad gemeinsam
mit `python3`, `IDF_PATH` und `idf.py` vor dem ersten ESP-Build. Die
detaillierte esp-clang-Pfad-, Versions-, `tools.json`- und `pyclang`-Pruefung
bleibt beim bestehenden Static-Analysis-Owner.

`host` umfasst den vollständigen clang-format-18-Check, nativen Build und
Ressourcenbericht, komplette native Tests, Compile-Datenbank und den exakten
clang-tidy-18-Lauf sowie Architekturguard und Quality-Gate-Selbsttests. `esp`
umfasst Bring-up-/Release-Build, Ressourcenbericht und esp-clang-Static-
Analysis. Nur wenn beide Aufrufe mit dem gleichen `PRE_READY_EXPECTED_HEAD`
erfolgreich sind, darf
`PRE_READY_LOCAL_GATES=PASS` dokumentiert werden. Ein nicht ausgeführter
Teil bleibt `NOT_RUN`.

### GitHub-CI

`.github/workflows/build.yml` reagiert auf:

- `pull_request.opened`;
- `pull_request.ready_for_review`;
- `pull_request.synchronize`;
- `pull_request.reopened`.

Der Firmwarejob laeuft nur, wenn der Pull Request kein Draft ist. Draft-Pushes
koennen einen sofort uebersprungenen Workfloweintrag erzeugen, fuehren aber
keine Builds oder Tests aus.

Der Ownerwechsel auf `Ready for review` startet die vollstaendige CI fuer den
reviewten Head. Jeder spaetere semantische Push auf einen Nicht-Draft-PR startet
sie erneut und verwirft den vorherigen Firmware-Pruefnachweis.

Markdown-only- und Kommentaraenderungen sind durch `paths-ignore` von der
Firmware-CI ausgenommen. Fuer den Reviewnachweis gilt bei semantischen
Aenderungen die Materialitaetsregel: Eine lokal begrenzte Korrektur wird durch
Fix Verification und den erforderlichen Regression Check verifiziert; eine
materielle Aenderung oder ein breiter neuer Diff erfordert einen neuen Full
Review. Rein redaktionelle Korrekturen ohne Bedeutungs-, Scope-, Vertrags- oder
Akzeptanzwirkung duerfen den bisherigen Reviewnachweis behalten.

Nach einem CI-Fehlschlag dokumentiert der Agent Fehler, Auswirkung und gezielte
Korrektur. Nur der Owner entscheidet ueber eine Rueckstufung auf Draft. Nach
einer CI-Korrektur gilt dieselbe Fix-Verification-/Materialitaetsregel: Der
Korrekturdiff, zuvor offene Blocker sowie direkt betroffene Vertraege und
Regressionen werden geprueft; nur bei materieller Aenderung oder breitem neuem
Diff ist ein neuer Full Review erforderlich. Den neuen Wechsel auf `Ready for
review` fuehrt nur der Owner aus.

Es gibt keinen `push`-Trigger und damit keinen automatischen identischen
Wiederholungslauf nach dem Merge.

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
python3 scripts/check_architecture_boundaries.py
git diff --check
```

`check_secrets.py` sowie die Artefakt-Scanabdeckung sind keine lokalen
Pre-Ready-Gates. Ihre Aufrufe stehen ausschliesslich im GitHub-Workflow.

Ein gezielter Lauf muss im PR mit Befehl, Umfang und Ergebnis dokumentiert
werden.

## Vollstaendiger nativer Lauf

Der vollständige native Lauf ist die `host`-Phase des gemeinsamen Runners. Er
wird nicht durch eine zweite manuelle Befehlsliste dokumentiert:

```bash
bash scripts/run_pre_ready_gates.sh host
```

## ESP-IDF-Profile

Nach erfolgreicher `host`-Phase und Aktivierung der festgelegten Toolchain:

```bash
bash scripts/run_pre_ready_gates.sh esp
```

Der Runner verwendet die bestehenden Owner für Profilbuild,
Ressourcenbericht und esp-clang-Analyse. Die lokale Ausführung muss die
ESP-IDF-/esp-clang-Umgebung vor dem Aufruf bereitstellen; bei fehlender oder
abweichender Provenienz bleibt der ESP-Teil `NOT_RUN` beziehungsweise
`BLOCKED`.

Der Upgrade-, Herkunfts- und Hardware-Smoke-Vertrag steht in
`ESP_IDF_UPGRADE_CONTRACT.md`.

## Formatierung und Static Analysis

| Werkzeug | Version | Umfang |
|---|---:|---|
| clang-format | 18 | C/C++ unter `src/`, `include/`, `lib/`, `test/`, `main/` |
| clang-tidy | 18 | hardwareunabhaengiger Produktionskern ueber die native Kompilierungsdatenbank |
| esp-clang | zur ESP-IDF-6.0.2-Toolchain passend | beide ESP-IDF-Profile |

Die vollständige Formatprüfung, die native Kompilierungsdatenbank und die
kanonische clang-tidy-Dateiliste stehen ausschließlich im versionierten
Runner `scripts/run_pre_ready_gates.sh`. CI und lokaler Pre-Ready-Lauf rufen
denselben Runner auf. Eine punktuelle
Unterdrueckung verwendet ausschliesslich:

```cpp
// NOLINT(check-name): konkrete Begruendung
```

## Architektur- und Gate-Selbsttests

Architekturguard und Quality-Gate-Selbsttests sind Bestandteile der
gemeinsamen `host`-Phase. Ihre Gatebefehle werden nicht nochmals hier oder im
Workflow dupliziert; fuer den lokalen Pre-Ready-Lauf genügt der Runner-Aufruf.

Der Architekturguard erzwingt die in ADR-013 festgelegte Richtung und ersetzt
kein vollstaendiges Architekturreview.

Der Gate-Selbsttest beweist anhand temporaerer fehlerhafter Fixtures, dass die
Qualitaetspruefungen echte Verstoesse erkennen.

### GitHub-CI-only: Privacy und Artefakte

`check_secrets.py` kontrolliert im GitHub-Workflow getrackte Textdateien,
lokale Konfigurationen und nach den ESP-Builds die hochzuladenden Textartefakte.
`check_ci_artifact_scan_coverage.py` prueft dort, dass jeder erfolgreiche
Textartefakt-Upload durch die im Workflow angegebene Scanmenge abgedeckt ist.
Diese Gates und ihre Artefaktpfade werden nicht vom gemeinsamen Runner
ausgefuehrt und sind kein lokaler Pfad-Whitelist-Vertrag.

## Determinismus und Zeit

Native Tests verwenden keine reale Uhrzeit und keine Netzwerkabhaengigkeit.
`ITimeSource` trennt monotone Laufzeit von optionaler UTC-Zeit.
`VirtualTimeSource` schreitet nur durch explizite Testaktionen voran; ein
Neustart wird durch eine neue Instanz simuliert.

Der ESP-IDF-Adapter `EspTimerTimeSource` liefert monotone Laufzeit ueber
`esp_timer`. Eine fachliche Nutzung wird nur behauptet, wenn ein realer
Produktionskonsument existiert.

## Ressourcenbericht und Artefakte

Der native und der ESP-IDF-Ressourcenbericht werden durch die `host`- und
`esp`-Phase des gemeinsamen Runners erzeugt. Die einzelnen Owner-Skripte und
ihre Datenformate bleiben unverändert.

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
3. den gemeinsamen Runner in der `host`-Phase ausfuehren; dieser bricht bei
   Format, Build, Tests oder clang-tidy fail-fast ab;
4. die GitHub-CI-only Quelltext-/Privacy-Pruefung ausfuehren;
5. ESP-IDF `v6.0.2` am exakten Commit installieren und verifizieren;
6. esp-clang installieren, die ESP-IDF-Umgebung aktivieren und den gemeinsamen
   Runner in der `esp`-Phase ausfuehren;
7. die GitHub-CI-only Artefakt-Scanabdeckung und Artefakt-/Privacy-Pruefung
   ausfuehren;
8. Berichte und Buildartefakte sichern.

Damit bleiben die billigen Host-Gates vor der teuren ESP-IDF-Provisionierung,
waehrend der lokale Ownerlauf beide portablen Runner-Phasen auf demselben
finalen HEAD vollstaendig ausfuehrt. Die GitHub-CI-only Gates laufen nur im
Workflow.

`concurrency` bricht einen veralteten Lauf desselben Pull Requests ab, sobald
ein neuerer Lauf startet.

## Ergebnisstatus

- **PASS:** Pruefung wurde ausgefuehrt und war erfolgreich.
- **FAILED:** Pruefung wurde ausgefuehrt, ist fehlgeschlagen und blockiert den
  Abschluss.
- **BLOCKED:** Pruefung konnte wegen einer konkret benannten fehlenden
  Voraussetzung nicht ausgefuehrt werden.

Die Statusfelder werden getrennt gefuehrt:

- `INDEPENDENT_REVIEW=PASS` und `OPEN_BLOCKERS=0` bezeichnen den abgeschlossenen
  fachlichen Review;
- `PRE_READY_LOCAL_GATES=PASS` bezeichnen den autorisierten vollständigen
  lokalen Lauf auf dem finalen HEAD;
- `GITHUB_CI=PASS` bezeichnet den CI-Anteil des Merge-Gates.

`Ready for review` darf erst nach `PRE_READY_LOCAL_GATES=PASS` durch den Owner
gesetzt werden. `INDEPENDENT_REVIEW=PASS` oder `OPEN_BLOCKERS=0` allein sind
dafür nicht ausreichend.

`SKIPPED`, `NOT_RUN` oder eine fehlende Angabe duerfen nicht als `PASS`
bezeichnet werden.

## Ausnahmen

Jede Ausnahme benoetigt eine konkrete Begruendung:

- projektweit in Werkzeugkonfiguration oder diesem Dokument;
- punktuell direkt an der Unterdrueckung;
- auftragsbezogen im freigegebenen Plan und PR-Nachweis.

Unbegruendete Unterdrueckungen von Safety-, Security-, Architektur- oder
Kernfunktionstests sind unzulaessig.
