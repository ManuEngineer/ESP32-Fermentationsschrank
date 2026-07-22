# CI, Tests, virtuelle Zeit und Qualitaetspruefungen

## Status

Dieses Dokument beschreibt die mit Issue #10 eingefuehrte Testausfuehrung,
virtuelle Zeitquelle, CI-Pipeline und Qualitaetswerkzeuge. Es ergaenzt
`docs/ACCEPTANCE_TESTS.md` (Testebenen und Release-Gates) und
`docs/IMPLEMENTATION_PLAN.md` (SW0-Grundlage) um die konkrete lokale und
CI-seitige Umsetzung.

## Native Tests lokal ausfuehren

```bash
pio run -e native -e esp32_bringup -e esp32_release
pio test -e native
python scripts/check_platformio_config.py
```

Die native Testausfuehrung ist reproduzierbar: Sie verwendet ausschliesslich
den Host-Compiler, keine reale Uhrzeit und keine Netzwerkzugriffe. Jede
Testsuite liegt in einem eigenen Verzeichnis unter `test/`:

- `test/test_smoke/` — Projektmetadaten, Profile und Plattform-/App-Grenze
- `test/test_time_source/` — virtuelle Zeitquelle (siehe unten)

## Virtuelle Zeitquelle

`lib/device_platform/src/time_source.hpp` definiert den anwendungsneutralen
Port `ITimeSource` mit zwei Werten:

- `monotonicMillis()`: monoton steigende Millisekunden seit Erstellung der
  Instanz. Faellt nie zurueck, wird von Aenderungen der absoluten Zeit nicht
  beeinflusst. Eine neue Instanz (z. B. nach einem Neustart) beginnt wieder
  bei 0.
- `unixTimeSeconds()`: optionale absolute UTC-Zeit. `std::nullopt`, solange
  keine verlaessliche Zeitquelle (z. B. NTP) vorliegt.

`lib/device_platform/src/virtual_time_source.hpp` implementiert
`VirtualTimeSource` fuer native Tests und Simulation: Die Zeit schreitet
ausschliesslich durch expliziten Aufruf von `advanceMonotonicMillis(deltaMs)`
voran; es wird nie auf reale Systemzeit oder das Netzwerk zugegriffen. Ein
Neustart wird durch eine neue Instanz simuliert (`monotonicMillis()` beginnt
wieder bei 0).

Diese Grundlage ist bewusst noch nicht in `DevicePlatform` oder `main.cpp`
verdrahtet, da es dafuer erst ab der fachlichen Logik (Issue #12 und folgende)
einen Verbraucher gibt. Ein realer ESP32-Zeitadapter (`millis()`, NTP) ist
Aufgabe von Issue #11 (Hardwareabstraktionen, Mockadapter und Simulator).

## Compilerwarnungen

`native` und beide ESP32-Profile bauen mit `-Wall -Wextra -Werror`
(`native` zusaetzlich mit `-Wpedantic`). Neue Warnungen im Projektcode werden
damit zu Buildfehlern, nicht still akzeptiert.

Begruendete Ausnahme: PlatformIOs `build_src_flags` gilt nur fuer `src/`, nicht
fuer `lib/`. Damit `lib/device_platform/` und `lib/fermentation_app/` auf den
ESP32-Profilen ebenfalls unter `-Wall -Wextra -Werror` gebaut werden, hat jedes
der beiden Module ein eigenes `library.json` mit `build.flags`. Framework- und
Fremdcode (Arduino-Core, Unity) sind davon nicht betroffen.

## Format- und Static-Analysis-Strategie

| Werkzeug | Version | Konfiguration | Umfang |
|---|---|---|---|
| clang-format | 18.1.8 | `.clang-format` | `src/`, `include/`, `lib/`, `test/` |
| clang-tidy | 18.1.8 | `.clang-tidy` | `include/app_config.hpp`, `lib/*/src/*.cpp`, `src/main.cpp` |

Lokale Ausfuehrung (Werkzeuge muessen installiert sein, z. B. via
`apt-get install clang-format-18 clang-tidy-18` oder
`pip install clang-format==18.1.8 clang-tidy==18.1.8`):

```bash
clang-format --dry-run --Werror $(find src include lib test -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \))
clang-format -i <datei>   # automatisch korrigieren

pio run -e native -t compiledb
clang-tidy -p . include/app_config.hpp \
  lib/device_platform/src/device_platform.cpp \
  lib/device_platform/src/virtual_time_source.cpp \
  lib/fermentation_app/src/fermentation_application.cpp \
  src/main.cpp
```

`clang-tidy` analysiert den hardwareunabhaengigen Kern ueber die
`native`-Kompilierungsdatenbank. Dokumentierte Ausnahmen:

- Der Arduino-Zweig von `src/main.cpp` (`#if defined(ARDUINO)`) wird nicht
  erfasst, da keine ESP32-Cross-Compile-Kompilierungsdatenbank fuer clang-tidy
  verfuegbar ist.
- `test/` ist nicht im Scope, da PlatformIOs `compiledb`-Ziel nur den
  Firmware-Build, nicht den Testbuild abbildet und Unity-Makros viele
  Static-Analysis-Regeln mit geringem Nutzen ausloesen wuerden.
- Einzelne Checks sind projektweit deaktiviert; Begruendung steht als
  Kommentar in `.clang-tidy` (z. B. `modernize-avoid-c-arrays` fuer
  Embedded-typische `char[]`-Konstanten, `misc-include-cleaner` als zu strikt
  fuer die schmalen Schnittstellenheader dieses Projekts).

Eine punktuelle Unterdrueckung im Code erfolgt ausschliesslich mit
`// NOLINT(check-name): Begruendung` und muss die Begruendung enthalten.

## Geheimnis- und Lokalkonfigurationspruefung

```bash
python scripts/check_secrets.py
```

Prueft, dass gitignorete Dateien (`include/secrets.hpp`,
`config/hardware.yaml`, `config/pins.yaml`, `*.pem`, `*.key`, ...) nicht
eingecheckt sind, und durchsucht getrackte Textdateien nach typischen
Geheimnismustern (private Schluessel, AWS-artige Zugangsschluessel,
zugewiesene Passwort-/Token-Werte). Dateien mit `example` im Namen sind von der
musterbasierten Zuweisungspruefung ausgenommen, da sie absichtlich
Platzhalterwerte wie `YOUR_WIFI_PASSWORD` enthalten.

## Firmware- und Ressourcen-Groessenbericht

```bash
python scripts/build_report.py --output build-report.md native esp32_bringup esp32_release
```

Baut die angegebenen Profile und erzeugt `build-report.md` mit RAM-/Flash-
Belegung je ESP32-Profil sowie der Groesse von `firmware.elf`/`firmware.bin`.
Fuer `native` wird die Groesse des Host-Testbinaers ausgewiesen. Der Bericht
ist informativ; verbindliche Byte-Budgets bleiben laut
`docs/OPEN_POINTS.md` weiterhin `TBD_IMPLEMENTATION_BUDGET` bis zu realen
Hardware- und Belastungsmessungen. CI sichert den Bericht als Artefakt
`build-report`.

## PASS / FAILED / BLOCKED

Jeder CI-Schritt liefert ein eindeutiges Ergebnis:

- **PASS**: Schritt erfolgreich (gruener GitHub-Actions-Schritt).
- **FAILED**: Schritt schlaegt fehl und blockiert den Merge (roter Schritt).
- **BLOCKED**: Eine Pruefung kann mangels Voraussetzung nicht ausgefuehrt
  werden (z. B. fehlendes Werkzeug lokal). In CI sind Format- und
  Static-Analysis-Werkzeuge fest installiert; `BLOCKED` tritt dort nicht auf.
  Lokal meldet `scripts/selftest_quality_gates.py` `BLOCKED` statt eines
  falschen `PASS`, wenn `clang-format`/`clang-tidy` nicht installiert sind.

`scripts/selftest_quality_gates.py` beweist bei jedem CI-Lauf anhand
temporaerer, absichtlich fehlerhafter Fixtures, dass Format-, Static-Analysis-
und Geheimnispruefung echte Verstoesse erkennen — ohne dass ein fehlerhafter
Fall jemals in `main` eingecheckt werden muss.

## Ausnahmen und Fehlalarme

Jede Ausnahme (deaktivierter Check, `NOLINT`-Kommentar, vom Format-/
Lint-Scope ausgeschlossene Datei) muss begruendet sein:

- projektweite Ausnahmen: als Kommentar in `.clang-format`/`.clang-tidy` oder
  in diesem Dokument
- punktuelle Ausnahmen: als `// NOLINT(check-name): Begruendung` im Code

Eine unbegruendete Unterdrueckung eines Sicherheits- oder Kernfunktionstests
ist nicht zulaessig (siehe `AGENTS.md`, Tests und Definition of Done).
