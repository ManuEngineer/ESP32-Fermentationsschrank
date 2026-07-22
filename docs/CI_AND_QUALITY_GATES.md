# CI, Tests, virtuelle Zeit und Qualitaetspruefungen

## Status

Dieses Dokument beschreibt die mit Issue #10 eingefuehrte Testausfuehrung,
virtuelle Zeitquelle, CI-Pipeline und Qualitaetswerkzeuge sowie die mit Issue #11
ergaenzte Pruefung der Architekturgrenzen. Es ergaenzt
`docs/ACCEPTANCE_TESTS.md` und `docs/IMPLEMENTATION_PLAN.md` um die konkrete
lokale und CI-seitige Umsetzung.

## Native Tests lokal ausfuehren

```bash
pio run -e native -e esp32_bringup -e esp32_release
pio test -e native
python scripts/check_platformio_config.py
python scripts/check_architecture_boundaries.py
python scripts/check_secrets.py
python scripts/selftest_quality_gates.py
```

Die native Testausfuehrung ist reproduzierbar: Sie verwendet ausschliesslich
den Host-Compiler, keine reale Uhrzeit und keine Netzwerkzugriffe. Jede
Testsuite liegt in einem eigenen Verzeichnis unter `test/`, unter anderem:

- `test/test_smoke/` — Projektmetadaten, Profile und Plattform-/App-Grenze
- `test/test_time_source/` — virtuelle Zeitquelle
- `test/test_sensor_actuator_mocks/` — Sensor-, Aktor- und Simulationsadapter
- `test/test_persistence_journal_mocks/` — Persistenz- und Journaladapter
- `test/test_network_notification_mocks/` — Netzwerk- und Benachrichtigungsadapter

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

Die Grundlage ist noch nicht in `DevicePlatform` oder `main.cpp` verdrahtet, da
es dafuer erst ab der fachlichen Logik einen Verbraucher gibt. Der reale
ESP32-Zeitadapter (`millis()`, NTP) folgt mit der realen Hardwareintegration.

## Compilerwarnungen

`native` und beide ESP32-Profile bauen mit `-Wall -Wextra -Werror`
(`native` zusaetzlich mit `-Wpedantic`). Neue Warnungen im Projektcode werden
damit zu Buildfehlern, nicht still akzeptiert.

PlatformIOs `build_src_flags` gilt nur fuer `src/`, nicht fuer `lib/`. Deshalb
besitzen `lib/device_platform/`, `lib/device_platform_test_support/` und
`lib/fermentation_app/` je eine `library.json` mit denselben verbindlichen
Warnungsflags. `device_platform_test_support` wird nur durch native Tests
verwendet und nicht in die ESP32-Produktionsbuilds eingebunden.

## Format- und Static-Analysis-Strategie

| Werkzeug | Version | Konfiguration | Umfang |
|---|---|---|---|
| clang-format | 18.1.8 | `.clang-format` | `src/`, `include/`, `lib/`, `test/` |
| clang-tidy | 18.1.8 | `.clang-tidy` | Produktions-Kompilierungsdatenbank des Profils `native` |

Lokale Ausfuehrung:

```bash
clang-format --dry-run --Werror $(find src include lib test -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \))
clang-format -i <datei>

pio run -e native -t compiledb
clang-tidy -p . include/app_config.hpp \
  lib/device_platform/src/device_platform.cpp \
  lib/device_platform/src/virtual_time_source.cpp \
  lib/fermentation_app/src/fermentation_application.cpp \
  lib/fermentation_app/src/program_model.cpp \
  lib/fermentation_app/src/standard_program_catalog.cpp \
  src/main.cpp
```

`clang-tidy` analysiert den hardwareunabhaengigen Produktionskern ueber die
`native`-Kompilierungsdatenbank. Dokumentierte Ausnahmen:

- Der Arduino-Zweig von `src/main.cpp` wird nicht erfasst, da keine
  ESP32-Cross-Compile-Kompilierungsdatenbank fuer clang-tidy verfuegbar ist.
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
python scripts/build_report.py --output build-report.md native esp32_bringup esp32_release
```

Der Befehl baut die angegebenen Profile und erzeugt `build-report.md` mit
RAM-/Flash-Belegung je ESP32-Profil sowie der Groesse der erzeugten Binaerdateien.
Der Bericht ist informativ; verbindliche Byte-Budgets bleiben
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
