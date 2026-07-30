# Implementierungsplan fuer Issue #72

## Planstatus

- Issue: `#72 – [Platform A] Reproduzierbare ESP-IDF-6.0.2-Buildbasis und
  Komponentenstruktur`
- Tracking: `#71` (bleibt offen bis `#72` -> `#73` -> `#74` abgeschlossen sind)
- Planbranch: `plan/issue-72-esp-idf-6-buildbasis`
- Basisbranch: `main`
- Basis-Commit: `b6d2385934db288cb25b125abbcbe5e307aca294`
  (Merge-Commit von PR #68, zugleich aktueller `origin/main`-Stand zum
  Planzeitpunkt)
- Ausgangslage: PR #68 ist gemergt, Issue #57 ist geschlossen. Damit ist die
  fachliche Startabhaengigkeit von `#72` erfuellt.
- Planstatus: `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`
- Implementierung: nicht begonnen

Dieser Plan ist die einzige Repositoryaenderung der Planungsphase. Er wird erst
nach Commit und Push durch den folgenden exakten Ownerkommentar zur
Implementierungsgrundlage:

```text
PLAN APPROVED
Approved plan commit: <commit-sha>
```

Laenge: Der Richtwert des Auftrags liegt bei 300 bis 600 Zeilen; dieser Plan
liegt knapp darueber. Grund ist die Summe der zwingend vorgeschriebenen
Mindestinhalte (14 nummerierte Abschnitte des Auftrags plus die generischen
Pflichtabschnitte aus dem `AGENTS.md`-Plan-first-Workflow, u. a. Daten-/
Schnittstellenvertraege, Safety-/Recoverygrenzen, Teststrategie,
Dokumentationsaenderungen, TBD-Status), nicht wiederholte Vollmatrizen; jede
Tabelle erscheint genau einmal.

## Metadatenabweichung (nur Beobachtung, keine Aenderung)

Der Text von Issue `#72` traegt weiterhin `Status: BLOCKED_DEPENDENCY` und die
in seinem Body dokumentierte lineare Abhaengigkeitsdarstellung
`fermentation_app -> device_platform -> device_platform_esp_idf -> ESP-IDF`.
Beides ist veraltet: `#72` -> `#73` -> `#74` sind seit dem Merge von PR #68 und
dem Schliessen von Issue #57 fachlich startbereit, und die lineare Darstellung
wird durch die in diesem Plan verwendete Dependency-Inversion-Struktur
ersetzt (siehe Abschnitt „Verbindliche Architekturkorrektur“). Gemaess
Plan-first-Workflow werden Issue-Bodies, Labels oder Kommentare in dieser
Phase nicht veraendert; eine Metadatensynchronisierung bleibt einem separat
freigegebenen Auftrag vorbehalten.

## Umgebungsnachweis (read-only, native ESP-IDF-Installation)

Vor Planerstellung wurde die bereits installierte native ESP-IDF-Umgebung
ausschliesslich lesend gegen den Auftrag verifiziert:

| Pruefung | Ergebnis |
|---|---|
| `idf.py --version` | `ESP-IDF v6.0.2` |
| `IDF_PATH` | `/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.0.2` |
| `git describe --tags --exact-match` (im ESP-IDF-Checkout) | `v6.0.2` |
| ESP-IDF-Commit-ID | `7101770dc6db2667b3c477cc31365dd1acd6db4e` |
| `git status --short` (im ESP-IDF-Checkout) | leer (sauber) |
| Submodule-Status | 25 Submodule, alle sauber initialisiert |

Der ESP-IDF-Checkout wurde ausschliesslich gelesen; keine Installation, kein
Update, kein schreibender Eingriff. Der absolute Hostpfad dient in diesem Plan
nur als Umgebungsnachweis und wird an keiner Stelle als eingecheckter Pfad in
Produktionsdateien vorgesehen (siehe Abschnitt „Toolchain- und
Versionsvertrag“).

## Ziel

Fuer Issue `#72` ausschliesslich eine Planungsphase durchfuehren und danach
anhalten:

1. aktuellen Repo-, Issue- und Abhaengigkeitsstand pruefen (dieser Plan);
2. die kleinstmoegliche tragfaehige ESP-IDF-6.0.2-Buildbasis planen;
3. einen Draft-PR mit genau dieser Plan-Datei erstellen;
4. nach Push des Plan-Commits anhalten.

Fachliches Ergebnis nach Freigabe und Umsetzung: ein reproduzierbarer
ESP-IDF-6.0.2-Konfigurations-, Compile- und Linkpfad fuer die bestehenden
portablen Komponenten `device_platform` und `fermentation_app`, parallel zum
weiterhin gruenen PlatformIO-/Arduino-Pfad. Keine reale Laufzeitparitaet,
keine Hardwareintegration.

## Nicht-Ziele (Nicht-Scope von `#72`)

- produktive Laufzeitparitaet und echter Composition Root aus `#73`;
- echte Zeit-, Logging-, Reset-, Task-, Watchdog- oder Ressourcenadapter;
- Entfernung des Arduino-Hauptframework-Pfads aus `#74`;
- endgueltige GitHub-Actions-Umstellung;
- Arduino-ESP32 als IDF-Komponente;
- Auswahl oder Integration einer Arduino-basierten Bibliothek;
- NVS, GPIO, LEDC, Sensor, Display, Touch, WLAN oder Web;
- reale Partitionstabelle ohne technischen Zwang;
- Hardwaretests;
- Aenderungen an Fachmodellen, Safetyvertraegen, `#57`-Semantik oder
  Wireformaten;
- ESPRelayBoard;
- separates Device-Platform-Repository oder allgemeines Template.

## Verbindliche Quellen und Entscheidungen

### Direkte Primaerquellen (keine allgemeine Websuche)

| Thema | Quelle |
|---|---|
| Repository | <https://github.com/espressif/esp-idf> |
| Release-Tag | <https://github.com/espressif/esp-idf/releases/tag/v6.0.2> |
| Versionsgebundene Doku | <https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/> |
| Build System | .../v6.0.2/esp32/api-guides/build-system.html |
| C++ Support | .../v6.0.2/esp32/api-guides/cplusplus.html |
| Project Configuration | .../v6.0.2/esp32/api-guides/kconfig/index.html |
| Migration 6.0 (Build System, Toolchain) | .../v6.0.2/esp32/migration-guides/release-6.x/6.0/ |
| Component Manager | <https://docs.espressif.com/projects/idf-component-manager/en/latest/> |
| Minimalstruktur-Referenz | <https://github.com/espressif/esp-idf/tree/v6.0.2/examples/get-started/hello_world> |

Kernbefunde daraus (fuer diesen Plan tatsaechlich relevant):

- Minimales Projekt-`CMakeLists.txt` (Referenz `hello_world`):
  `cmake_minimum_required(VERSION 3.22)`, `include($ENV{IDF_PATH}/tools/cmake/project.cmake)`,
  `project(<name>)`; optional `idf_build_set_property(MINIMAL_BUILD ON)`.
- `idf_component_register(SRCS ... INCLUDE_DIRS ... REQUIRES ... PRIV_REQUIRES ...)`
  registriert eine Komponente; `REQUIRES` propagiert oeffentliche Header
  transitiv, `PRIV_REQUIRES` bleibt intern; der spezielle Component `main`
  erhaelt implizit alle im Projekt gefundenen Komponenten, sofern nicht ueber
  `COMPONENTS`/`EXTRA_COMPONENT_DIRS` eingeschraenkt.
- `sdkconfig` ist generiert und versionierbar, `sdkconfig.defaults` liefert
  Startwerte; zusaetzlich wird `sdkconfig.defaults.<target>` geladen, falls
  vorhanden.
- ESP-IDF 6.0.2 verwendet standardmaessig `-std=gnu++26` fuer C++; ein
  abweichender Standard wird pro Komponente ueber
  `target_compile_options(${COMPONENT_LIB} PUBLIC|PRIVATE -std=...)` gesetzt.
  Exceptions und RTTI sind standardmaessig deaktiviert
  (`CONFIG_COMPILER_CXX_EXCEPTIONS`, `CONFIG_COMPILER_CXX_RTTI`).
- Migration 6.0 (Build System): Orphan-ELF-Sections erzeugen jetzt einen
  Buildfehler; globale Konstruktoren laufen jetzt in aufsteigender statt
  absteigender Reihenfolge; `CONFIG_COMPILER_DISABLE_DEFAULT_ERRORS` ist jetzt
  deaktiviert, das heisst Standardwarnungen sind standardmaessig Fehler
  (deckt sich mit dem bestehenden `-Werror` des Projekts).
- Migration 6.0 (Toolchain): GCC wurde von 14.2.0 auf 15.1.0 angehoben; neue
  potenzielle Warnungen, u. a. C++-spezifisch `-Wself-move`,
  `-Wtemplate-body`, `-Wdangling-reference`, `-Wdefaulted-function-deleted`.
  Kein Hinweis auf eine Aenderung am C++-Standarddefault in diesem
  Migrationsabschnitt; der Default `gnu++26` ist ueber die C++-Support-Seite
  bestaetigt.
- Component Manager: `idf_component.yml` beschreibt Abhaengigkeiten,
  `dependencies.lock` wird beim Konfigurieren erzeugt und nicht manuell
  editiert, `managed_components/` ist ein generierter lokaler Buildbestand.

### Herstellerbasis-Entscheid

Bereits vor diesem Auftrag anhand der offiziellen Repositories geprueft;
Kurzfassung als Pflichttabelle in Abschnitt 1 („Herstellerbasis-Entscheid,
Kurzform“). Ergebnis: Dieses Repository bleibt die Produktfirmware,
`espressif/esp-idf @ v6.0.2` ist die Plattformbasis, keine vollstaendige
Espressif-Solution wird geforkt oder als Firmwarebasis uebernommen. Diese
Grundsatzentscheidung wird in `#72` nicht erneut mit einer offenen Websuche
aufgerollt.

### Projektentscheidungen (`AGENTS.md`, ADRs)

- ADR-001 (`PlatformIO mit Arduino Framework`) bleibt fuer den bestehenden
  Vergleichspfad bis `#74` in Kraft; `#71`/`#72`/`#73`/`#74` loesen sie fuer
  die Zielarchitektur ab, ohne PR `#68` oder abgeschlossene Issues zu
  aendern.
- ADR-013 (`docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`) und die Modul-
  `AGENTS.md` unter `lib/device_platform/`, `lib/device_platform_test_support/`
  und `lib/fermentation_app/` bleiben unveraendert verbindlich; dieser Plan
  fuegt keine neue Ausnahme hinzu.
- `docs/ADOPT_OR_BUILD.md`: vor Eigenentwicklung wird zuerst Frameworkfunktion,
  dann offizielles Herstellerpaket, dann gepflegte Fremdbibliothek, erst dann
  Eigenentwicklung geprueft. Fuer `#72` bedeutet das: keine neue
  Fremdkomponente, nur der offizielle ESP-IDF-Kern.
- `scripts/check_architecture_boundaries.py` erzwingt bereits die
  ADR-013-Grenzen (`device_platform` kennt weder `device_platform_test_support`
  noch `fermentation_app`; `fermentation_app` kennt nicht
  `device_platform_test_support`; `main.cpp` bindet keinen Test-Support ein).
  Dieser Plan haelt diese Grenzen ein und erweitert das Skript nur um eine
  IDF-Leak-Pruefung (siehe „Quality Gates“).

## Verbindliche Architekturkorrektur

Die im Issue-`#71`-Text verwendete lineare Darstellung

```text
fermentation_app -> device_platform -> device_platform_esp_idf -> ESP-IDF
```

ist als Build-Abhaengigkeitsrichtung missverstaendlich und wird in diesem Plan
nicht uebernommen. `device_platform` haengt niemals vom ESP-IDF-Adapter ab.
Verbindlich ist die folgende Dependency-Inversion-Struktur:

```text
                         +----------------------+
                         | main / Composition   |
                         | Root                 |
                         +----------+-----------+
                                    |
                    +---------------+---------------+
                    |                               |
                    v                               v
          fermentation_app              device_platform_esp_idf
                    |                               |
                    +---------------+---------------+
                                    |
                                    v
                           device_platform
                                    ^
                                    |
                         stabile Projektports

          device_platform_esp_idf -> ESP-IDF 6.0.2
```

- `fermentation_app` haengt nur von schmalen Ports und portablen Hilfen aus
  `device_platform` ab.
- `device_platform_esp_idf` implementiert diese Ports und haengt von
  `device_platform` sowie ESP-IDF ab.
- Der Composition Root (`src/main.cpp`) kennt Anwendung und konkrete Adapter.
- `device_platform` kennt weder `device_platform_esp_idf` noch ESP-IDF.
- `device_platform_test_support` bleibt ausschliesslich Testcode.
- Keine IDF- oder Arduino-Typen in stabilen Ports, Fachmodellen, Wireformaten
  oder persistenten Modellen; keine privaten/internen ESP-IDF-APIs; keine
  generische Schatten-API; keine vorsorglichen Erweiterungspunkte.

`device_platform_esp_idf` wird in `#72` **nicht angelegt**. Die aktuelle
Bestandsaufnahme (naechster Abschnitt) zeigt, dass `device_platform` bereits
vollstaendig anwendungsneutral und ohne jede ESP32-/Arduino-Bindung ist – es
gibt noch keinen einzigen produktiven ESP32-Adapter, der diese Grenze fuellen
wuerde. Der erste reale Adapter (Zeit, Logging, Reset, Tasks) entsteht in
`#73`; `#72` weist nur nach, dass der bestehende portable Code unter
ESP-IDF 6.0.2 kompiliert und linkt.

## 1. Kompakte Bestandsaufnahme

| Aspekt | Befund |
|---|---|
| Arduino-Kopplung | Ausschliesslich `src/main.cpp` (`#include <Arduino.h>`, `setup()`, `loop()`, `Serial`, `millis()`, hinter `#if defined(ARDUINO)`/`#else`). Kein weiterer Treffer in `lib/` oder `include/`. |
| ESP32-Profile (`platformio.ini`) | `native` (Unit-Tests, `gnu++17`, `-Wall -Wextra -Wpedantic -Werror`), `esp32_bringup` und `esp32_release` (beide `espressif32@7.0.1`, `board=esp32dev`, `framework=arduino`, `gnu++17`, `-Wall -Wextra -Werror`, 4 MB Flash, kein PSRAM). Bringup/Release unterscheiden sich nur durch `APP_PROFILE_*`-Makro und `app_config`-Policy, nicht durch Sprachstandard oder Zielhardware. |
| Buildflags/Sprachstandard | `-std=gnu++17` fuer alle drei Profile; feste Makros `APP_TARGET_FLASH_MB=4`, `APP_REQUIRE_PSRAM=0`, `APP_WEB_OTA_ENABLED=0`, `APP_REAL_ACTUATORS_ENABLED=0` aus `[profile] build_flags`. |
| Produktive Translation Units im ESP32-Image | `src/main.cpp` (Composition Root) plus alle `.cpp`-Dateien unter `lib/device_platform/src/` (7 Stueck: `crc32`, `device_platform`, `storage_envelope`, `storage_slot_candidates`, `virtual_time_source`, dazu reine Header) und `lib/fermentation_app/src/` (47 Dateien, ueberwiegend `.cpp`/`.hpp`-Paare). Keine Datei ausserhalb `src/main.cpp` bindet Arduino ein. |
| Native Testabhaengigkeiten | PlatformIO `test_framework = unity`; 22 Testverzeichnisse unter `test/`; Tests haengen zusaetzlich von `lib/device_platform_test_support/` ab (Mocks/Simulation), die laut ADR-013 nie in ein Produktivbuild (auch nicht ESP-IDF) gelangen darf. |
| Bestehende Architektur-/Quality-Gates | `scripts/check_architecture_boundaries.py` (ADR-013-Grenzen inkl. Selftest), `scripts/check_platformio_config.py`, `scripts/check_secrets.py`, `scripts/selftest_quality_gates.py`; CI (`\.github/workflows/build.yml`) faehrt `clang-format`, `build_report.py` fuer alle drei PlatformIO-Envs, `check_platformio_config.py`, `pio test -e native`, `clang-tidy` gegen eine feste Dateiliste, die Architekturpruefung, die Secretpruefung und die Selftest-Suite. |
| `Arduino.h`/`setup()`/`loop()`/`Serial`/`millis()` | Ausschliesslich in `src/main.cpp`, jeweils hinter `#if defined(ARDUINO)`. Der native Zweig (`#else`) nutzt bereits einen reinen `int main()`. |
| Bereits vollstaendig portabler Code | `lib/device_platform/` (kein Treffer fuer `ESP32`/`Arduino`-Praeprozessorzweige ausser drei Kommentaren, die nur die Wortbreite (32 Bit auf ESP32) beschreiben; `DevicePlatform::begin()`/`update()` enthalten noch keine Hardwarezugriffe), `lib/fermentation_app/` (haengt laut eigenem `AGENTS.md` nur von `device_platform`-Ports ab), `include/app_config.hpp` (reine `constexpr`/Praeprozessor-Policy ohne Arduino- oder ESP-IDF-Bezug). |
| Erst in `#73` anzupassende Stellen | `src/main.cpp`s `#if defined(ARDUINO)`-Zweig (echter Composition-Root-Betrieb via `setup()`/`loop()`) bleibt unveraendert; ein produktiver ESP-IDF-Composition-Root mit `app_main()`, echten Zeit-/Logging-/Reset-/Task-Adaptern und `device_platform_esp_idf` entsteht dort, nicht in `#72`. |

Die Migration betrifft damit nicht nur `main.cpp`: `main.cpp` ist die einzige
Datei mit echter Arduino-*API*-Kopplung, aber `#72` muss zusaetzlich beweisen,
dass die gesamte bestehende `device_platform`- und `fermentation_app`-Quellbasis
(54 Uebersetzungseinheiten ausserhalb `main.cpp`) unter dem ESP-IDF-6.0.2-
Toolchain (GCC 15.1.0, `gnu++17`-Override, deaktivierte Exceptions/RTTI)
tatsaechlich kompiliert und linkt.

### Herstellerbasis-Entscheid (Kurzform)

| Kandidat | Status fuer `#72` |
|---|---|
| `esp-idf` | ausgewaehlte Plattformbasis |
| `esp-iot-solution`, `esp-bsp`, `idf-extra-components`, ESP Component Registry | Komponentenquellen fuer spaetere konkrete Spikes, keine Einbindung in `#72` |
| `esp-rainmaker`, `esp-matter`/`esp-lowcode-matter`, `esp-brookesia`, `esp-at`, `esp-zigbee-sdk` | gepruefte, nicht passende vollstaendige Solutions; keine Firmwarebasis |

### Codebefund zu Exceptions/RTTI/Threading (fuer Abschnitt 5 und 6 relevant)

Grep ueber `lib/`, `include/`, `src/` zeigt: kein `throw`, kein `try`/`catch`,
kein `dynamic_cast`, kein `typeid(` in Produktionscode. `std::mutex` wird genau
einmal verwendet (`lib/fermentation_app/src/configuration_service.hpp`), sonst
nur `<algorithm>`, `<array>`, `<atomic>`, `<cmath>`, `<cstddef>`, `<cstdint>`,
`<cstring>`, `<limits>`, `<memory>`, `<optional>`, `<set>`, `<string>`,
`<type_traits>`, `<utility>`, `<vector>` – keine `<iostream>`, `<sstream>`,
`<filesystem>` oder `<thread>`. Damit sind die ESP-IDF-Standarddefaults
(Exceptions aus, RTTI aus) fuer `#72` ausreichend; einzig `std::mutex`
benoetigt laut ESP-IDF-C++-Support-Seite die POSIX-Threads-Unterstuetzung
(ESP-IDF-Komponente `pthread`, auf FreeRTOS-Tasks abgebildet).

## 2. Kleinstmoeglicher Scope von `#72`

Begrenzung auf genau das, was fuer einen reproduzierbaren ESP-IDF-Compile-/
Linkpfad noetig ist:

- gepinnte native ESP-IDF-6.0.2-Installation als Entwicklerumgebung nutzen
  (bereits vorhanden, siehe „Umgebungsnachweis“);
- minimale Top-Level-`CMakeLists.txt` nach dem `hello_world`-Muster;
- Registrierung von `lib/device_platform/` und `lib/fermentation_app/` als
  ESP-IDF-Komponenten ohne Dateiumzuege (siehe Abschnitt 3);
- ein gemeinsames, versioniertes `sdkconfig.defaults`;
- ein sicherer reiner Build-/Linknachweis ueber einen **temporaeren
  build-only Stub**, kein produktiver `app_main()`;
- Erhalt des bestehenden PlatformIO-/Arduino-Vergleichspfads (`native`,
  `esp32_bringup`, `esp32_release` bleiben unveraendert gruen);
- minimale Dokumentation und ein Fail-fast-Versionsguard.

Nicht in `#72` vorgezogen: echte Laufzeitparitaet, produktive
`app_main()`-Logik, Zeit-/Logging-/Reset-/Task-/Watchdogadapter, reale
Hardware-/Netzwerk-/Persistenzadapter, endgueltige CI-Migration, Entfernung
der Arduino-Profile.

### Build-only Stub statt zweitem Einstiegspunkt

ESP-IDF verlangt zum Linken eine `app_main()`. Statt einer zweiten
Composition-Root-*Datei* wird `src/main.cpp` um genau einen zusaetzlichen,
rein praeprozessorgesteuerten Zweig erweitert:

```text
#if defined(ARDUINO)
    ... unveraendert (setup()/loop()) ...
#elif defined(ESP_PLATFORM)
    extern "C" void app_main(void) {}
#else
    ... unveraendert (nativer int main()) ...
#endif
```

`ESP_PLATFORM` wird von ESP-IDF fuer jeden IDF-Build gesetzt und ist in einer
reinen ESP-IDF-Anwendung (ohne Arduino-as-component) nie gleichzeitig mit
`ARDUINO` gesetzt; die bestehende Zweigreihenfolge (`ARDUINO` zuerst) bleibt
dadurch eindeutig. Der neue Zweig startet keine Anwendung, keine Tasks, keine
Hardware – er ist ein leerer Compile-/Linkanker und wird in `#73` durch den
echten `app_main()`-Composition-Root ersetzt.

Der bereits bestehende, von `ARDUINO`/`ESP_PLATFORM`/nativ unabhaengige
Dateikopf von `main.cpp` (Include von `app_config.hpp`, `device_platform.hpp`,
`fermentation_application.hpp` sowie die anonymen Namespace-Instanzen
`platform` und `application`) bleibt unveraendert bestehen und wird dadurch
automatisch auch im ESP-IDF-Zweig uebersetzt und statisch verlinkt – ohne
`begin()` aufzurufen. Das erzwingt einen echten Link alle drei Komponenten
(`device_platform`, `fermentation_app`, der neue Stub selbst), ohne die
Anwendung zu starten, und dupliziert keinen bestehenden Code (DRY: derselbe
Instanziierungs-Idiom wie im bereits vorhandenen Arduino-/nativen Pfad).

## 3. Komponentenstruktur und Abhaengigkeiten

| Komponente | Verzeichnis | Verantwortung | `REQUIRES` | `PRIV_REQUIRES` | Dateiumzug |
|---|---|---|---|---|---|
| `device_platform` | `lib/device_platform/` (neu: `CMakeLists.txt`) | bestehende anwendungsneutrale Ports/Dienste, `SRCS` = vorhandene `.cpp`-Dateien, `INCLUDE_DIRS "src"` | keine (reine C++-Stdlib) | keine | keiner |
| `fermentation_app` | `lib/fermentation_app/` (neu: `CMakeLists.txt`) | konkrete Fermentationsanwendung, `SRCS` = vorhandene `.cpp`-Dateien, `INCLUDE_DIRS "src"` | `device_platform` (oeffentliche Header verwenden `device_platform`-Typen) | `pthread` (fuer `std::mutex` in `configuration_service.hpp`, sofern dieser Header oeffentlich sichtbar ist – waehrend der Umsetzung empirisch am Compilelauf bestaetigen und andernfalls auf `REQUIRES` anheben) | keiner |
| `main` (ESP-IDF-Component, nicht zu verwechseln mit der Datei `src/main.cpp`) | neu: `main/CMakeLists.txt` | reine CMake-Registrierung; `SRCS "../src/main.cpp"`, `INCLUDE_DIRS "../include"` | keine | `device_platform`, `fermentation_app` | keiner (referenziert bestehende Datei ausserhalb des eigenen Verzeichnisses) |
| `device_platform_test_support` | `lib/device_platform_test_support/` | **nicht Bestandteil** des ESP-IDF-Builds | – | – | kein neues `CMakeLists.txt`; kein `EXTRA_COMPONENT_DIRS`-Eintrag |
| `device_platform_esp_idf` | – | **noch nicht angelegt**, Zielgrenze fuer `#73` | – | – | – |

Begruendung fuer die Nichtaufnahme von `device_platform_test_support` in den
ESP-IDF-Komponentenbaum: ADR-013 verbietet jede Abhaengigkeit von
Produktions- oder Composition-Root-Code auf Test-Support; da das
Top-Level-`CMakeLists.txt` keinen `EXTRA_COMPONENT_DIRS`-Eintrag dafuer
erhaelt, kann `idf.py build` diese Bibliothek gar nicht erst einbinden. Ein
Selftest im erweiterten Architekturcheck bestaetigt das (siehe „Quality
Gates“).

`REQUIRES`/`PRIV_REQUIRES` werden ausschliesslich aus tatsaechlich verwendeten
`#include`-Beziehungen abgeleitet, nicht vorsorglich erweitert. Bestehende
Verzeichnisse werden direkt ueber `SRCS`/`INCLUDE_DIRS` referenziert; es wird
keine Datei verschoben, da ESP-IDF Komponenten frei benennbare Quellpfade
akzeptiert und keine bestehende Verantwortungsverletzung vorliegt, die einen
Umzug rechtfertigen wuerde.

## 4. Toolchain- und Versionsvertrag

- Die bereits vorhandene native ESP-IDF-Installation unter
  `/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.0.2`
  (Tag `v6.0.2`, Commit `7101770dc6db2667b3c477cc31365dd1acd6db4e`, per
  `export.sh` aktivierte Umgebung) ist die Entwicklerumgebung fuer `#72`.
  Sie wird nicht erneut installiert, ersetzt oder aktualisiert.
- Der Build verwendet ausschliesslich die aktivierte Espressif-Python-Umgebung
  und `idf.py`; kein PlatformIO-ESP-IDF-Paket als zweite Quelle der Wahrheit.
- **Einzige Quelle der Wahrheit fuer den Versionspin:** eine neue, kleine
  Datei `cmake/esp_idf_version_guard.cmake`, eingebunden aus dem
  Top-Level-`CMakeLists.txt` nach `project()`. Sie vergleicht
  `IDF_VERSION_MAJOR`/`IDF_VERSION_MINOR`/`IDF_VERSION_PATCH` (von ESP-IDF
  selbst als CMake-Variablen bereitgestellt) gegen `6`/`0`/`2` und bricht den
  Konfigurationslauf mit `message(FATAL_ERROR ...)` bei Abweichung ab. Damit
  existiert der Pin nur an dieser einen Stelle, nicht zusaetzlich verstreut in
  Skripten oder Dokumentation.
- Der absolute Hostpfad der Installation erscheint **nur** im Plan und in der
  kurzen Aktivierungsdokumentation (siehe Dateiliste) als Beispiel, nicht in
  CMake-, Quell- oder Konfigurationsdateien; Build und CI verwenden immer die
  jeweils aktivierte `IDF_PATH`-Umgebungsvariable.
- `build/`, `managed_components/`, generiertes `sdkconfig` (nicht
  `sdkconfig.defaults`) und `dependencies.lock` (erst ab `#73`/`#74` relevant,
  da `#72` keine Fremdkomponente einbindet) werden ueber `.gitignore`
  ausgeschlossen.
- Ein spaeterer offizieller CI-Pfad (GitHub Actions mit `idf.py`) wird in
  `#74` festgelegt; `#72` nimmt ihn nicht vorweg und aendert
  `.github/workflows/build.yml` nicht.

## 5. C++-Standard und Compilervertrag

ESP-IDF 6.0.2 kompiliert C++ standardmaessig mit `-std=gnu++26`; das Projekt
verwendet heute durchgaengig `gnu++17` (`platformio.ini`, `[profile]
build_flags`). Ein unbemerkter Sprachstandardwechsel wird verhindert, indem
jede der drei neuen `idf_component_register(...)`-Aufrufe (`device_platform`,
`fermentation_app`, `main`) denselben expliziten Aufruf erhaelt:

```cmake
target_compile_options(${COMPONENT_LIB} PUBLIC "-std=gnu++17")
```

`PUBLIC` stellt sicher, dass sowohl die Komponente selbst als auch alle
Konsumenten ihrer oeffentlichen Header mit demselben Standard uebersetzen.
Die drei Aufrufe sind bewusst je einmal in den ohnehin zwingend
notwendigen Komponenten-`CMakeLists.txt`-Dateien plaziert (keine zusaetzliche
Hilfsdatei, kein globaler Eingriff auf alle IDF-Kernkomponenten); der Wert
`gnu++17` ist textidentisch mit `platformio.ini` und wird bei Bedarf in beiden
Dateien synchron gepflegt. Eine gemeinsame CMake-Hilfsdatei fuer nur drei
Vorkommen desselben Einzeilers wuerde zusaetzliche Indirektion ohne
nachweisbaren Wartungsvorteil einfuehren (KISS) – anders als bei
`cmake/esp_idf_version_guard.cmake`, das eine mehrzeilige Pruefung mit
Fehlermeldung buendelt.

Eine spaetere bewusste Anhebung auf einen neueren Standard (z. B. im Zuge
einer eigenen ESP-IDF-Major-Migration) bleibt ein eigenes, separat zu
begruendendes Vorhaben und wird in `#72` nicht vorgezogen.

Exceptions (`CONFIG_COMPILER_CXX_EXCEPTIONS`) und RTTI
(`CONFIG_COMPILER_CXX_RTTI`) bleiben auf dem ESP-IDF-Standarddefault (aus);
der Codebefund in Abschnitt 1 bestaetigt, dass kein Produktionscode `throw`,
`try`/`catch`, `dynamic_cast` oder `typeid(` verwendet.

## 6. Konfigurationsprofile

- Ein gemeinsames, versioniertes `sdkconfig.defaults` im Repository-Root:
  `CONFIG_IDF_TARGET="esp32"` (ESP32-WROOM-32E, passend zu `board=esp32dev`),
  Flashgroesse 4 MB, keine PSRAM-Aktivierung. Kein Bring-up-/CI-/Release-
  Overlay, da `#72` noch keine reale funktionale Differenz zwischen diesen
  Stufen besitzt (die vorhandene Unterscheidung ist rein
  `APP_PROFILE_*`-Makro-basiert und lebt bereits im C++-Code, nicht in
  Kconfig). Ueberlays folgen erst, wenn `#73`/`#74` tatsaechlich
  unterschiedliche Kconfig-Werte benoetigen.
- Reale Partitionierung: ESP-IDF's eingebaute Default-Partitionstabelle
  (Single-Factory-App, keine OTA-Slots) passt ohne Projektueberschreibung in
  4 MB und wird fuer den reinen Compile-/Linknachweis unveraendert
  uebernommen; keine `partitions.csv` in `#72`. Eine reale, hardwaregebundene
  Partitionsentscheidung bleibt hinter dem vorgesehenen Hardwaregate.
- Keine Arduino-spezifischen Kconfig-Werte, keine unterschiedliche Fach- oder
  Safetysemantik zwischen Profilen; `app_config.hpp`s bestehende
  `static_assert`-Vertraege (4 MB, kein PSRAM, kein Web-OTA, keine echten
  Aktoren) bleiben unveraendert die einzige Quelle fuer diese Fachgrenzen.
- Das build-only-Stub verwendet das Makro-Set `APP_PROFILE_ESP32_BRINGUP=1`
  (`HARDWARE_UNVERIFIED`, `LockedForBringup`), gesetzt ueber
  `target_compile_definitions` in `main/CMakeLists.txt`, textidentisch mit
  `platformio.ini`s `esp32_bringup`-Env. Diese Werte existieren damit
  zwangslaeufig an zwei Stellen (PlatformIO-`.ini`-Syntax und CMake-Syntax);
  diese Dopplung ist auf fuenf Makros begrenzt, technisch durch die zwei
  parallelen Build-Systeme des Dual-Build-Uebergangs bedingt und entfaellt mit
  der Arduino-Ablösung in `#74`.

## 7. Component Manager und Fremdkomponenten

Fuer `#72` werden keine externen Komponenten benoetigt: kein
`idf_component.yml`, kein `dependencies.lock`, keine Arduino-, Sensor-,
Display-, Touch-, WLAN-, Web- oder NVS-Komponente.

Spaeterer Trigger (nur dokumentiert, nicht umgesetzt):

- `idf_component.yml` entsteht mit der ersten realen verwalteten
  Fremdkomponente (voraussichtlich `#73` fuer Zeit-/Log-Adapter oder ein
  spaeteres Hardware-Issue fuer Display/Touch/Sensor);
- `dependencies.lock` wird vom Solver erzeugt, nicht manuell editiert, und bei
  Registry-/Git-Abhaengigkeiten grundsaetzlich versioniert;
- lokale pfadabhaengige Lockfiles werden vor Versionierung auf Portabilitaet
  geprueft;
- jedes Lockfile-Delta wird mit Lizenz-, Notice- und Abhaengigkeitspruefung
  gemaess `docs/ADOPT_OR_BUILD.md` verbunden.

## 8. Dual-Build-Uebergang

Zeitlich begrenzt bestehen zwei parallele Pfade:

```text
bestehender PlatformIO-/Arduino-Build (native, esp32_bringup, esp32_release)
+
neuer reiner ESP-IDF-6.0.2-Compile-/Linkpfad (idf.py build)
```

- Portable Quellen: alle Dateien unter `lib/device_platform/src/` und
  `lib/fermentation_app/src/` sowie der arduino-/plattformunabhaengige
  Dateikopf von `src/main.cpp` werden von beiden Pfaden gebaut.
- Divergenzerkennung: beide Pfade laufen in derselben CI-Definition
  (`.github/workflows/build.yml` wird um einen zusaetzlichen `idf.py`-Schritt
  ergaenzt, siehe „Quality Gates“); ein fehlschlagender ESP-IDF-Schritt macht
  die Pipeline rot, ohne den PlatformIO-Schritt zu beeinflussen. Eine
  Quellcode- oder Flagdivergenz wird dadurch spaetestens beim naechsten
  gemeinsamen CI-Lauf sichtbar, nicht durch eine neue eigene
  Vergleichsinfrastruktur.
- Bewusst erst `#73`: echte Laufzeitparitaet, `device_platform_esp_idf`.
  Bewusst erst `#74`: Entfernung des Arduino-Hauptframework-Pfads,
  endgueltige CI-Migration.
- Der Arduino-Hauptframework-Pfad (dieser Dual-Build) ist strikt getrennt vom
  moeglichen spaeteren, ownerfreigegebenen Arduino-as-ESP-IDF-component-Adapter
  fuer einen einzelnen konkreten Konsumenten (siehe „Verbindlicher
  Plattformentscheid“ im Auftrag); `#72` bindet keinen von beiden als
  IDF-Komponente ein.

## 9. Quality Gates – risikobasiert und KISS

| Gate | Umsetzung |
|---|---|
| Native Tests unveraendert gruen | `pio test -e native` unveraendert |
| Bestehende PlatformIO-Builds gruen | `esp32_bringup`, `esp32_release` unveraendert |
| ESP-IDF-6.0.2-Konfigurations-/Compile-/Linkbuild gruen | neuer `idf.py build` in CI, gleicher Job oder separater Job derselben Pipeline |
| Format-/Static-Analysis | `clang-format --dry-run` deckt bereits `src`/`include`/`lib`/`test` ab (neue `.cpp`/`.hpp` fallen automatisch darunter); die drei neuen `CMakeLists.txt` sind kein `clang-format`-Ziel |
| Architekturgrenzen inkl. IDF-Leak | `scripts/check_architecture_boundaries.py` erhaelt eine zusaetzliche, schmale Pruefung: kein `#include <esp_...>`/`#include "esp_...` und kein IDF-Kconfig-Symbol (`CONFIG_...`) ausserhalb der neuen `main/`-Komponente und einer zukuenftigen `device_platform_esp_idf/`; ein Selftest-Fall (absichtlicher IDF-Include in `device_platform`) ergaenzt die bestehende Selftest-Suite des Skripts, analog zum bereits vorhandenen Muster |
| Secretpruefung, `git diff --check` | unveraendert `scripts/check_secrets.py`, `git diff --check` |
| Fail-fast-Versionspin | `cmake/esp_idf_version_guard.cmake` (siehe Abschnitt 4); manuell durch Testlauf mit absichtlich falscher Zielversion nachgewiesen, nicht durch eine neue automatisierte Testdatei |

Keine Hardwaretests, keine Heap-/Stack-/Watchdog-/NVS-/Flashlebensdauer-
Garantie fuer diese reine Buildstruktur. Genau ein gezielter
IDF-Include-Leak-Selftest wird ergaenzt; keine Matrix aus Arduino-/IDF-/Typ-/
Header-/Verzeichnispermutationen.

## 10. Ressourcen- und Regressionsnachweis

Eine kompakte Vergleichstabelle wird in der finalen PR-Dokumentation (nicht in
diesem Plan) aus den tatsaechlichen Buildlaeufen erzeugt:

| Kennzahl | Quelle Arduino-Pfad | Quelle ESP-IDF-Pfad |
|---|---|---|
| Flash (statisch) | PlatformIO-`build_report.py`-Ausgabe fuer `esp32_release` | ESP-IDF-Groessenbericht (`idf.py size`) fuer den build-only Stub |
| Statisches RAM | PlatformIO-`build_report.py`-Ausgabe | `idf.py size` |
| Artefakte | `.elf`/`.bin` aus PlatformIO | `.elf`/`.bin`/Mapfile aus `idf.py build` |

Unterschiedliche Toolchains (Arduino-ESP32 2.0.17 auf `espressif32@7.0.1` vs.
natives ESP-IDF 6.0.2/GCC 15.1.0) werden explizit als nicht direkt
vergleichbar gekennzeichnet; keine bytegenaue Gleichheit wird verlangt. Keine
neue Ressourceninstrumentierung im Fach- oder Recoverycode. Vollstaendige
Laufzeit-, Heap-, Stack- und Hardwarebaseline bleibt `#74` vorbehalten.

## 11. Adopt-or-build-Pruefung

1. Dieses Repository bleibt die Produktfirmware; ESP-IDF 6.0.2 ist die
   ausgewaehlte Herstellerplattform.
2. Fuer einen konkreten Bedarf werden zuerst ESP-IDF-Kernkomponenten und
   danach passende Komponenten aus ESP Component Registry,
   `esp-iot-solution`, `esp-bsp` und `idf-extra-components` geprueft.
3. Vollstaendige Espressif-Solutions wie RainMaker, Matter oder Brookesia
   werden nicht als Firmwarebasis uebernommen; einzelne Bausteine benoetigen
   einen konkreten Konsumenten und eigenen Spike.
4. Danach werden geeignete gepflegte externe IDF-Komponenten geprueft.
5. Eigene technische Implementierung erfolgt erst, wenn kein geeigneter
   Baustein den konkreten Vertrag erfuellt.
6. Arduino-basierte Bibliotheken bleiben nur als kuenftige adapterlokale
   Kandidaten offen; `#72` bindet weder Arduino noch eine andere
   Fremdkomponente ein.

```text
ESP-IDF 6.0.2: ausgewaehlte und exakt gepinnte Plattformbasis
Arduino als Hauptframework: nur temporaerer Vergleichspfad bis #74
Arduino-ESP32 als IDF-Komponente: nicht eingebunden und aktuell nicht als
kompatible 6.0.2-Baseline verfuegbar
Espressif-Solution-Fork: keiner
Fremdkomponenten: keine in #72; spaetere Komponenten selektiv aus den
verbindlich genannten Quellen evaluieren
```

## 12. Kleiner Implementierungs- und Commit-Schnitt

| # | Commit | Inhalt |
|---|---|---|
| 1 | ESP-IDF-Pin und Build-/Linkanker | `CMakeLists.txt` (Root), `cmake/esp_idf_version_guard.cmake`, `main/CMakeLists.txt`, `sdkconfig.defaults`, neuer `#elif defined(ESP_PLATFORM)`-Zweig in `src/main.cpp`, `.gitignore`-Ergaenzung |
| 2 | Portable Komponentenregistrierung | `lib/device_platform/CMakeLists.txt`, `lib/fermentation_app/CMakeLists.txt` (inkl. `-std=gnu++17`-Override je Komponente) |
| 3 | Guards, Dual-Build-Nachweis, Dokumentation | Erweiterung `scripts/check_architecture_boundaries.py` (IDF-Leak-Check + Selftest), CI-Ergaenzung in `.github/workflows/build.yml` fuer `idf.py build`, kurze Aktivierungsdokumentation, `CHANGELOG.md`-Eintrag |

Eine andere Aufteilung ist bei der Umsetzung zulaessig, wenn sie kleiner oder
klarer ist. Kein Commit zieht fachliche Laufzeitadapter aus `#73` oder
Abschlussarbeiten aus `#74` vor.

## 13. Erwartete Dateiliste

| Datei | Neu/Geaendert | Begruendung |
|---|---|---|
| `CMakeLists.txt` (Root) | neu | Top-Level-ESP-IDF-Projektdatei (Pflicht laut Build-System-Doku) |
| `cmake/esp_idf_version_guard.cmake` | neu | einzige Quelle der Wahrheit fuer den Fail-fast-Versionspin `v6.0.2` |
| `main/CMakeLists.txt` | neu | ESP-IDF-`main`-Komponente, referenziert bestehendes `src/main.cpp`, keine eigene Logikdatei |
| `lib/device_platform/CMakeLists.txt` | neu | Komponentenregistrierung fuer bestehende, unveraenderte Quellen |
| `lib/fermentation_app/CMakeLists.txt` | neu | Komponentenregistrierung fuer bestehende, unveraenderte Quellen |
| `sdkconfig.defaults` | neu | reproduzierbare Kconfig-Startwerte (Zielchip, Flashgroesse) |
| `src/main.cpp` | geaendert | ein zusaetzlicher `#elif defined(ESP_PLATFORM)`-Zweig mit leerem `app_main()` |
| `scripts/check_architecture_boundaries.py` | geaendert | schmale IDF-Include-/Kconfig-Leak-Pruefung plus ein Selftest-Fall |
| `.github/workflows/build.yml` | geaendert | zusaetzlicher `idf.py build`-Schritt neben den bestehenden PlatformIO-Schritten |
| `.gitignore` | geaendert | `build/`, `managed_components/`, generiertes `sdkconfig` ausschliessen |
| `docs/tasks/issue-72-esp-idf-activation.md` (oder vergleichbarer kurzer Abschnitt in bestehender Dokumentation) | neu, kurz | knappe Aktivierungs-/Buildanleitung fuer die bereits installierte native Toolchain; keine erneute Installationsanleitung |
| `CHANGELOG.md` | geaendert | ein Eintrag fuer die neue ESP-IDF-Buildbasis |

Vermieden: neue allgemeine Frameworkordner ohne Konsumenten, leere Manifeste
oder Komponenten (`device_platform_esp_idf` entsteht nicht), mehrere fast
identische Buildskripte, kopierte Quelllisten, neue Test-Suites fuer reine
Buildmetadaten, vorsorgliche Templates fuer weitere Projekte.

## Daten-, Zustands- und Schnittstellenvertraege

`#72` aendert keine Fach-, Wire- oder Persistenzformate. Der einzige neue
„Vertrag“ ist rein buildseitig: der leere `app_main()`-Stub garantiert, dass
kein Zustand, keine Aktoren und keine Tasks initialisiert werden; die
bestehenden `IStateStore`-, Zeit-, Aktor- und sonstigen `device_platform`-Ports
bleiben unveraendert und ohne neuen Implementor. Es entsteht keine neue
oeffentliche Schnittstelle ausserhalb der drei neuen `CMakeLists.txt`-Dateien.

## Fehler-, Recovery-, Security- und Safetygrenzen

- Peltier/H-Bruecke bleiben unberuehrt: Es gibt in `#72` keinen Aktorcode, kein
  GPIO, keine neue Adapterklasse.
- Der ESP-IDF-Stub initialisiert keine Hardware und ruft `begin()`/`update()`
  nicht auf; ein `HARDWARE_UNVERIFIED`-Zustand wird nicht einmal erreicht, da
  die Anwendung gar nicht startet.
- Ein fehlschlagender `idf.py build` blockiert nur den neuen CI-Schritt, nicht
  den bestehenden PlatformIO-Pfad (siehe „Dual-Build-Uebergang“); es entsteht
  keine neue Fehlerklasse im Fachcode.
- Keine neue Security-Flaeche: keine Netzwerk-, Web- oder Auth-Komponente wird
  eingebunden.

## Teststrategie

- Bestehende native Testsuite (`pio test -e native`) bleibt unveraendert
  massgeblich fuer Fachlogik; sie wird durch `#72` weder erweitert noch
  eingeschraenkt.
- Neuer Nachweis ist ein **Buildgate**, kein neuer Unit-Test: `idf.py build`
  muss fuer den build-only Stub fehlerfrei durchlaufen und dabei tatsaechlich
  alle Uebersetzungseinheiten aus `device_platform` und `fermentation_app`
  kompilieren und linken (verifizierbar ueber die generierten
  Komponentenbibliotheken/das Mapfile).
- Genau ein neuer Selftest-Fall in `scripts/check_architecture_boundaries.py`
  (IDF-Include ausserhalb der erlaubten Grenzen wird erkannt), analog zum
  bestehenden Selftest-Muster des Skripts.
- Ein manueller Fail-fast-Nachweis (absichtlich falsche `IDF_VERSION_*` im
  Test) bestaetigt `cmake/esp_idf_version_guard.cmake`; keine dauerhafte neue
  automatisierte Testdatei nur dafuer.

## Dokumentationsaenderungen

- `CHANGELOG.md`: ein Eintrag fuer die neue parallele ESP-IDF-6.0.2-Buildbasis.
- Eine kurze neue Aktivierungs-/Builddokumentation (siehe Dateiliste), die nur
  beschreibt, wie die bereits installierte native Toolchain aktiviert
  (`. <IDF_PATH>/export.sh`) und der neue Pfad gebaut wird (`idf.py build`);
  keine erneute Installationsanleitung.
- Dieser Plan selbst bleibt bis zum Abschlussreview im Branch; die
  Owner-Entscheidung ueber Verbleib, Ueberfuehrung oder Entfernung vor dem
  Merge folgt dem in `AGENTS.md` beschriebenen Ablauf.

## 14. SOLID-, DRY- und KISS-Pruefung

- **SRP:** Jede neue `CMakeLists.txt` registriert genau eine bestehende
  Verantwortung (`device_platform`, `fermentation_app`, Buildanker); keine
  Datei uebernimmt mehrere Rollen.
- **OCP:** Der bestehende `#if defined(ARDUINO)`/`#else`-Mechanismus in
  `main.cpp` wird um einen dritten Zweig erweitert statt umgeschrieben; die
  beiden bestehenden Zweige bleiben unveraendert.
- **LSP:** Nicht anwendbar auf reine Build-Konfigurationsdateien; es werden
  keine neuen Schnittstellenimplementierungen eingefuehrt.
- **ISP:** Es entsteht keine neue Portschnittstelle; `device_platform_esp_idf`
  wird bewusst nicht als leerer Universaladapter vorgezogen.
- **DIP:** `fermentation_app` und der Composition-Root-Stub haengen von den
  bestehenden `device_platform`-Ports ab, nicht umgekehrt; die
  Dependency-Inversion-Struktur aus dem Architekturabschnitt wird durch die
  Komponententabelle (Abschnitt 3) exakt umgesetzt.
- **DRY:** Der ESP-IDF-Versionspin lebt an genau einer Stelle
  (`cmake/esp_idf_version_guard.cmake`); der C++-Standard `gnu++17` ist auf
  drei zwingend ohnehin vorhandene Komponenten-CMakeLists beschraenkt statt in
  einer zusaetzlichen Hilfsdatei dupliziert; die fuenf `APP_PROFILE_*`-Makros
  sind die einzige, explizit begruendete Ausnahme (zwei parallele
  Build-Systeme waehrend des zeitlich begrenzten Dual-Build-Uebergangs).
- **KISS:** Kein neues Verzeichnis fuer `device_platform_esp_idf`, kein
  `idf_component.yml`, keine Profil-Overlays, keine Partitionstabelle, keine
  neue Testsuite – nur die fuer den Compile-/Linknachweis zwingend
  notwendigen sieben neuen bzw. geaenderten Produktionsartefakte (siehe
  Abschnitt 13).

## Offene Ownerentscheidungen

1. Genauer Dateiname/Ort der kurzen Aktivierungsdokumentation
   (`docs/tasks/issue-72-esp-idf-activation.md` als eigene Datei versus ein
   neuer Abschnitt in einer bestehenden Datei wie `docs/CI_AND_QUALITY_GATES.md`).
2. Ob `REQUIRES pthread` fuer `fermentation_app` tatsaechlich oeffentlich
   (`REQUIRES`) statt privat (`PRIV_REQUIRES`) sein muss – abhaengig davon, ob
   `configuration_service.hpp` mit dem `std::mutex`-Member Teil eines von
   `main`/`fermentation_application.hpp` transitiv eingebundenen oeffentlichen
   Headers ist; waehrend der Umsetzung am tatsaechlichen Compilelauf zu
   bestaetigen, keine Produktwirkung.
3. Ob der neue CI-`idf.py build`-Schritt im selben `firmware`-Job wie die
   PlatformIO-Schritte oder als separater Job derselben Pipeline erfolgt;
   beides erfuellt „paralleler, klar getrennter Nachweis ohne Beeinflussung
   des PlatformIO-Pfads“.

Keine dieser drei Fragen aendert Scope, Architektur, Sicherheits- oder
Persistenzvertraege dieses Plans.

## Verbotene Vorwegnahmen

- kein `device_platform_esp_idf`-Verzeichnis, keine leere Komponente darin;
- keine produktive `app_main()`-Logik, kein Start von `platform`/`application`;
- kein Zeit-, Logging-, Reset-, Task- oder Watchdogadapter;
- keine reale Hardware-, Netzwerk- oder Persistenzintegration;
- keine `idf_component.yml`/`dependencies.lock`, keine Fremdkomponente;
- keine Profil-Overlays oder reale Partitionstabelle ohne technischen Zwang;
- keine Entfernung der Arduino-Profile oder des Arduino-Einstiegs;
- keine Arduino-ESP32-Komponente und keine Arduino-basierte Bibliothek;
- keine Aenderung an Issue-Bodies, Labels, Milestones oder Kommentaren von
  `#71`, `#73`, `#74`.

## TBD-/Platzhalterstatus

Fuer `#72` verbleibt kein `SPIKE_REQUIRED`, `TBD_HARDWARE`,
`TBD_COMMISSIONING` oder `FINAL_SELECTION_PENDING`: Der Plan trifft fuer den
reinen Compile-/Linknachweis ausschliesslich Entscheidungen, die bereits
durch bestehende Projektdokumente (ADR-013, `ADOPT_OR_BUILD.md`) oder direkt
verifizierte offizielle ESP-IDF-6.0.2-Quellen gedeckt sind. Die drei unter
„Offene Ownerentscheidungen“ genannten Punkte sind technische
Implementierungsdetails ohne Produkt-, Architektur- oder Sicherheitswirkung,
keine Platzhalterkategorien im Sinn von `AGENTS.md`.

## Abnahmekriterien

- `idf.py build` (mit dem bereits aktivierten nativen ESP-IDF-6.0.2-Checkout)
  konfiguriert, kompiliert und linkt den build-only Stub fehlerfrei und bindet
  dabei nachweislich alle Uebersetzungseinheiten aus `lib/device_platform/src/`
  und `lib/fermentation_app/src/` ein.
- `native`, `esp32_bringup`, `esp32_release` bleiben unveraendert gruen
  (`pio test -e native`, `build_report.py` fuer alle drei Envs).
- `cmake/esp_idf_version_guard.cmake` bricht den Konfigurationslauf bei einer
  anderen ESP-IDF-Version als `6.0.2` nachweislich ab (manueller Gegentest).
- `scripts/check_architecture_boundaries.py` erkennt einen absichtlichen
  IDF-Include-Leak ausserhalb der erlaubten Grenzen (neuer Selftest-Fall) und
  meldet fuer den freigegebenen Diff weiterhin `PASS`.
- `clang-format --dry-run --Werror`, `scripts/check_platformio_config.py`,
  `scripts/check_secrets.py`, `scripts/selftest_quality_gates.py` und
  `git diff --check` bleiben gruen.
- Keine Aenderung ausserhalb der in Abschnitt 13 gelisteten Dateien (fuer die
  Planungsphase selbst: ausschliesslich diese Plan-Datei).
- `device_platform_test_support` ist nachweislich nicht Teil des ESP-IDF-
  Komponentenbaums (kein `EXTRA_COMPONENT_DIRS`-Eintrag, kein
  `CMakeLists.txt` in diesem Verzeichnis).
