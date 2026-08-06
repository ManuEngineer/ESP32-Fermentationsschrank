# CI, Tests, virtuelle Zeit und Qualitaetspruefungen

## Status

Dieses Dokument beschreibt die Testausfuehrung, virtuelle Zeitquelle,
CI-Pipeline und Qualitaetswerkzeuge. Der native Hostpfad verwendet PlatformIO;
die ESP32-Produktionsprofile verwenden verbindlich ESP-IDF `v6.0.2`
(`7101770dc6db2667b3c477cc31365dd1acd6db4e`). Es ergaenzt
`docs/ACCEPTANCE_TESTS.md` und `docs/IMPLEMENTATION_PLAN.md` um die konkrete
lokale und CI-seitige Umsetzung.

## Owner-gesteuerte Vollpruefung und CI-Trigger

Die verbindliche Regel steht in `AGENTS.md`, Abschnitt 3a; dieser Abschnitt
beschreibt nur deren technische Umsetzung und dupliziert die Regel nicht.

Waehrend der Umsetzung eines freigegebenen Plans laeuft ausschliesslich
lokal, gezielt fuer die tatsaechlich betroffene(n) Testsuite(s):

```bash
pio test -e native -f "test_<betroffene_suite>"
```

Kein vollstaendiger `pio test -e native`, keine ESP-IDF-Profile, keine
`run_esp_idf_static_analysis.py` und kein `build_report.py` waehrend dieser
Phase.

`.github/workflows/build.yml` loest die schwere CI (native Volltest,
Static Analysis, beide ESP-IDF-Profile, Ressourcenbericht) ausschliesslich
aus bei:

```yaml
on:
  push:
    branches:
      - main
    paths-ignore:
      - "**/*.md"
  pull_request:
    types:
      - opened
      - reopened
      - ready_for_review
      - synchronize
    paths-ignore:
      - "**/*.md"

jobs:
  firmware:
    if: github.event_name == 'push' || github.event.pull_request.draft == false
```

Damit gilt:

- ein `push` auf einen Feature-/Plan-Branch mit offenem Draft-PR loest die
  schwere CI NICHT zusaetzlich zum `pull_request`-Event aus (kein doppelter
  Volllauf desselben Commits);
- `opened`/`reopened`/`synchronize` auf einem weiterhin als Draft gefuehrten
  PR ueberspringt den schweren Job (`github.event.pull_request.draft ==
  true`);
- der Owner setzt den Draft-PR auf `Ready for Review` - `ready_for_review`
  loest genau einen vollstaendigen Lauf fuer den reviewten Head aus;
- ein weiterer Push NACH `Ready for Review` (`synchronize`, `draft ==
  false`) loest erneut die volle CI aus - jede semantische Aenderung nach
  einer gruenen Vollpruefung verwirft den vorherigen Pruefnachweis
  (`AGENTS.md` 3a);
- `push` auf `main` (nach Merge) baut weiterhin vollstaendig, da der
  gemergte Commit ein neuer, zuvor nicht einzeln als Merge-Ergebnis
  gepruefter Stand ist und den Ressourcenbericht auf `main` fuer spaetere
  Basisvergleiche aktuell haelt.

Nach einem CI-Fehlschlag geht der PR zurueck auf Draft; nur der Fehler und
direkt abhaengige Tests werden lokal geprueft, danach erneutes Review und
erneutes `Ready for Review` durch den Owner (`AGENTS.md` 3a).

## Native Tests lokal ausfuehren

```bash
pio run -e native
pio test -e native
python scripts/check_architecture_boundaries.py
python scripts/check_secrets.py
python scripts/selftest_quality_gates.py
```

Die beiden ESP-IDF-Produktionsprofile werden nach Aktivierung der festgelegten
Toolchain gebaut und validiert:

```bash
. "$IDF_PATH/export.sh"
python scripts/build_esp_idf_profiles.py all
python scripts/run_esp_idf_static_analysis.py all
```

Die native Testausfuehrung ist reproduzierbar: Sie verwendet ausschliesslich
den Host-Compiler, keine reale Uhrzeit und keine Netzwerkzugriffe. Jede
Testsuite liegt in einem eigenen Verzeichnis unter `test/`, unter anderem:

- `test/test_smoke/` — Projektmetadaten, Profile und Plattform-/App-Grenze
- `test/test_time_source/` — virtuelle Zeitquelle
- `test/test_sensor_actuator_mocks/` — Sensor-, Aktor- und Simulationsadapter
- `test/test_persistence_journal_mocks/` — Persistenz- und Journaladapter
- `test/test_network_notification_mocks/` — Netzwerk- und Benachrichtigungsadapter
- `test/test_configuration_documents/` — Dokumentmodelle, Firmwarekataloge,
  UTF-8-/Unicode-, ID- und Zeitzonenvalidierung
- `test/test_configuration_codecs/` — feste Golden-Bytes, Grenzfaelle und
  native Allokationsregression grosser ProgramCatalog-Payloads
- `test/test_configuration_migration/` — Copy-Migration und Abbruch ohne
  Teilwirkung

## Virtuelle Zeitquelle

`lib/device_platform/src/time_source.hpp` definiert den anwendungsneutralen
Port `ITimeSource` mit zwei Werten:

- `monotonicMillis()`: monoton steigende Millisekunden seit Erstellung der
  Instanz. Faellt nie zurueck, wird von Aenderungen der absoluten Zeit nicht
  beeinflusst. Eine neue Instanz beginnt wieder bei 0.
- `unixTimeSeconds()`: optionale absolute UTC-Zeit. `std::nullopt`, solange
  keine verlaessliche Zeitquelle vorliegt.

`lib/device_platform/src/virtual_time_source.hpp` implementiert
`VirtualTimeSource` als anwendungsneutralen, deterministischen Plattformdienst:
Die Zeit schreitet ausschliesslich durch expliziten Aufruf von
`advanceMonotonicMillis(deltaMs)` voran; es wird nie auf reale Systemzeit oder
das Netzwerk zugegriffen. Ein Neustart wird durch eine neue Instanz simuliert.

`EspTimerTimeSource` existiert bereits unter
`lib/device_platform_esp_idf/` und ist in `main/app_main.cpp` fuer monotone
Laufzeit-, Heartbeat- und Ressourcen-Telemetrie eingebunden. Eine weitergehende
Verwendung durch Fachlogik oder `DevicePlatform` wird hier nicht behauptet und
bedarf eines tatsaechlich implementierten Verbrauchers.

## Compilerwarnungen

`native` baut mit `-Wall -Wextra -Werror -Wpedantic`; beide ESP-IDF-Profile
bauen mit `-Wall -Wextra -Werror`. Neue Warnungen im Projektcode werden damit
zu Buildfehlern, nicht still akzeptiert.

PlatformIOs `build_src_flags` gilt nur fuer `src/`, nicht fuer `lib/`. Deshalb
besitzen `lib/device_platform/`, `lib/device_platform_test_support/` und
`lib/fermentation_app/` je eine `library.json` mit denselben verbindlichen
Warnungsflags. `device_platform_test_support` wird nur durch native Tests
verwendet und nicht in die ESP32-Produktionsbuilds eingebunden.

## Format- und Static-Analysis-Strategie

| Werkzeug | Version | Konfiguration | Umfang |
|---|---|---|---|
| clang-format | 18 | `.clang-format` | `src/`, `include/`, `lib/`, `test/`, `main/` |
| clang-tidy | 18.1.8 | `.clang-tidy` | Produktions-Kompilierungsdatenbank des Profils `native` |

Lokale Ausfuehrung:

```bash
clang-format --dry-run --Werror $(find src include lib test main -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \))
clang-format -i <datei>

pio run -e native -t compiledb
clang-tidy -p . include/app_config.hpp \
  lib/device_platform/src/device_platform.cpp \
  lib/device_platform/src/virtual_time_source.cpp \
  lib/fermentation_app/src/configuration_document_codec.cpp \
  lib/fermentation_app/src/configuration_documents.cpp \
  lib/fermentation_app/src/configuration_migration.cpp \
  lib/fermentation_app/src/configuration_text.cpp \
  lib/fermentation_app/src/fermentation_application.cpp \
  lib/fermentation_app/src/firmware_configuration_catalog.cpp \
  lib/fermentation_app/src/process_state_machine.cpp \
  lib/fermentation_app/src/program_model.cpp \
  lib/fermentation_app/src/run_commands.cpp \
  lib/fermentation_app/src/run_snapshot.cpp \
  lib/fermentation_app/src/standard_program_catalog.cpp \
  src/main.cpp
```

`clang-tidy` analysiert den hardwareunabhaengigen Produktionskern ueber die
`native`-Kompilierungsdatenbank. Dokumentierte Ausnahmen:

- `test/` und `lib/device_platform_test_support/` sind nicht im Scope, da
  PlatformIOs `compiledb`-Ziel nur den Produktionsbuild und nicht den
  Test-Build-Graphen abbildet. Beide Bereiche werden mit denselben
  Compilerwarnungen gebaut und vollstaendig durch `clang-format` geprueft.
- Einzelne Checks sind projektweit deaktiviert; die Begruendung steht als
  Kommentar in `.clang-tidy`.

Eine punktuelle Unterdrueckung im Code erfolgt ausschliesslich mit
`// NOLINT(check-name): Begruendung` und muss die Begruendung enthalten.

## Architekturgrenzen nach ADR-013

```bash
python scripts/check_architecture_boundaries.py
```

Die Pruefung erzwingt die in `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`
festgelegte Abhaengigkeitsrichtung:

```text
device_platform_test_support -> device_platform
fermentation_app -> Plattform-Schnittstellen
main -> konkrete Plattform + konkrete Anwendung
```

Sie blockiert insbesondere:

- Abhaengigkeiten von `device_platform` auf `fermentation_app` oder
  `device_platform_test_support`,
- Abhaengigkeiten von `fermentation_app` oder `src/main.cpp` auf
  `device_platform_test_support`,
- Arduino-Abhaengigkeiten im Test-Support,
- reine Mock- oder Simulationsmodelle unter `lib/device_platform/src/`,
- offensichtliche Fermentationsbegriffe und schrankbezogene Aktorrollen in der
  allgemeinen Plattform-API.

Die Pruefung ersetzt kein Architekturreview. Sie sichert die klar automatisch
erkennbaren Grenzen ab und liefert bei einer Verletzung Datei und Zeile.

## ESP-IDF-6.0.2-Produktionspfad

ESP-IDF `v6.0.2` ist der einzige ESP32-Produktionspfad und wird in
`.github/workflows/build.yml` fuer `esp32_bringup` und `esp32_release`
getrennt gebaut. `main/app_main.cpp` ist die ESP-IDF Composition Root und
`lib/device_platform_esp_idf/` enthaelt die konkreten ESP-IDF-Adapter.
`src/main.cpp` und PlatformIO bleiben ausschliesslich fuer den nativen
Hosttestpfad.

Voraussetzung ist eine bereits installierte native ESP-IDF-6.0.2-Umgebung
(offizieller Tag `v6.0.2`, `--recursive` geklont). Aktivierung in der
jeweiligen Shell:

```bash
. ${IDF_PATH:-<pfad-zum-esp-idf-v6.0.2-checkout>}/export.sh
```

Lokaler Build und Profilnachweis:

```bash
python scripts/build_esp_idf_profiles.py all
```

`app_main()` startet keine reale Hardware; alle Aktoren bleiben deaktiviert.
Der kanonische Buildtreiber und Profilguard pruefen Herkunft, getrennte
`sdkconfig`-Pfade, Profilkonfiguration und Compile-Definitionen. Ein
`IDF_VER`-Guard im Root-`CMakeLists.txt` ist eine zusaetzliche Verteidigung.

Nach Hinzufuegen oder Entfernen einer Quelldatei unter `lib/device_platform/src/`,
`lib/fermentation_app/src/` oder `lib/device_platform_esp_idf/src/` kann
lokal `idf.py reconfigure` noetig sein, da diese Komponenten ueber
`SRC_DIRS` automatisch alle Quellen ihres Verzeichnisses einsammeln.

Generierte Bestaende (`build/`, `sdkconfig`, `sdkconfig.old`,
`managed_components/`) sind gitignored. `dependencies.lock` wird nicht
ignoriert: `#72`/`#73` binden keine Fremdkomponente ein, sobald eine reale
Component-Manager-Abhaengigkeit entsteht, wird das dabei erzeugte Lockfile
grundsaetzlich versioniert.

Die um eine schmale IDF-Leak-Pruefung erweiterte
`scripts/check_architecture_boundaries.py` (siehe oben) stellt sicher, dass
`lib/device_platform/src/` und `lib/fermentation_app/src/` keinen direkten
ESP-IDF-/RTOS-/Arduino-/Adapter-Include, keine `ESP_PLATFORM`-/`ARDUINO`-/
Kconfig-Praeprozessorverwendung und keine unautorisierte direkte IDF-
Komponentenabhaengigkeit in `REQUIRES`/`PRIV_REQUIRES` enthalten. Seit
Issue #73 prueft sie zusaetzlich, dass `lib/device_platform_esp_idf/` weder
`fermentation_app` noch Test-Support referenziert, dass `main/` keinen
Test-Support verwendet, und dass die `REQUIRES`/`PRIV_REQUIRES`-Eintraege
von `lib/device_platform_esp_idf/CMakeLists.txt` und `main/CMakeLists.txt`
getrennt nach oeffentlich/privat einer festen Allowlist entsprechen.

Die zwei Hardware-Smoke-Tests fuer Bring-up und Release sind ein verbindliches
Merge-Gate. Sie werden ausschliesslich auf dem exakten finalen
Implementierungs-Head ausgefuehrt; genaue Kriterien und der Upgradeprozess
stehen in [`ESP_IDF_UPGRADE_CONTRACT.md`](ESP_IDF_UPGRADE_CONTRACT.md).

## Geheimnis- und Lokalkonfigurationspruefung

```bash
python scripts/check_secrets.py
```

Die Pruefung stellt sicher, dass gitignorierte lokale Dateien nicht eingecheckt
sind, und durchsucht getrackte Textdateien nach typischen Geheimnismustern.
Dateien mit `example` im Namen sind von der musterbasierten
Zuweisungspruefung ausgenommen, da sie absichtlich Platzhalterwerte enthalten.

## Firmware- und Ressourcen-Groessenbericht

```bash
python scripts/build_report.py --output build-report.md
. "$IDF_PATH/export.sh"
python scripts/build_report.py --output build-report.md --append \
  --esp-idf-profiles bringup release
```

Der erste Befehl baut den nativen Hostpfad; der zweite wertet die bereits
gebauten ESP-IDF-Profile aus und ergaenzt ihre maschinenlesbaren
RAM-/Flash- und Artefaktdaten. Der Bericht ist informativ; verbindliche
Byte-Budgets bleiben
`TBD_IMPLEMENTATION_BUDGET` bis zu realen Hardware- und Belastungsmessungen. CI
sichert den Bericht als Artefakt `build-report`.

## PASS / FAILED / BLOCKED

Jeder CI-Schritt liefert ein eindeutiges Ergebnis:

- **PASS**: Schritt erfolgreich.
- **FAILED**: Schritt schlaegt fehl und blockiert den Merge.
- **BLOCKED**: Eine Pruefung kann mangels Voraussetzung lokal nicht ausgefuehrt
  werden. In CI sind die benoetigten Werkzeuge fest installiert.

`scripts/selftest_quality_gates.py` beweist bei jedem CI-Lauf anhand temporaerer,
absichtlich fehlerhafter Fixtures, dass Format-, Static-Analysis-, Geheimnis- und
Architekturpruefung echte Verstoesse erkennen, ohne einen fehlerhaften Fall in
das Repository einzuchecken.

## Ausnahmen und Fehlalarme

Jede Ausnahme muss begruendet sein:

- projektweite Ausnahmen: als Kommentar in der Werkzeugkonfiguration oder in
  diesem Dokument,
- punktuelle clang-tidy-Ausnahmen: als
  `// NOLINT(check-name): Begruendung` im Code.

Eine unbegruendete Unterdrueckung eines Sicherheits-, Architektur- oder
Kernfunktionstests ist nicht zulaessig.
