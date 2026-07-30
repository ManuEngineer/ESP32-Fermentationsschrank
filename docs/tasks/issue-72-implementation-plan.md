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
- Ausgangslage: PR #68 ist gemergt, Issue #57 ist geschlossen; damit ist die
  fachliche Startabhaengigkeit von `#72` erfuellt.
- Planstatus: `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`
- Implementierung: nicht begonnen
- Ueberholte Planstaende: `5916f1f1cc88634eb1cdc3dd8dadd2686e181b36`; dieses
  vollstaendig ueberarbeitete Review benoetigt einen neuen commitgebundenen
  Ownerkommentar.

Dieser Plan ist die einzige Repositoryaenderung der Planungsphase. Er wird erst
nach Commit und Push durch den folgenden exakten Ownerkommentar zur
Implementierungsgrundlage:

```text
PLAN APPROVED
Approved plan commit: <commit-sha>
```

## Metadatenabweichung (nur Beobachtung, keine Aenderung)

Issue `#72` traegt weiterhin `Status: BLOCKED_DEPENDENCY` und die veraltete
lineare Darstellung `fermentation_app -> device_platform ->
device_platform_esp_idf -> ESP-IDF`. Beides ist ueberholt; siehe „Verbindliche
Architekturkorrektur“. Gemaess Plan-first-Workflow werden Issue-Bodies,
Labels oder Kommentare in dieser Phase nicht veraendert.

## Umgebungsnachweis (read-only, native ESP-IDF-Installation)

| Pruefung | Ergebnis |
|---|---|
| `idf.py --version` | `ESP-IDF v6.0.2` |
| `IDF_PATH` | `/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.0.2` |
| `git describe --tags --exact-match` (im ESP-IDF-Checkout) | `v6.0.2` |
| ESP-IDF-Commit-ID | `7101770dc6db2667b3c477cc31365dd1acd6db4e` |
| `git status --short` (im ESP-IDF-Checkout) | leer (sauber) |
| Submodule-Status | 25 Submodule, alle sauber initialisiert |

Ausschliesslich gelesen; keine Installation, kein Update, kein schreibender
Eingriff. Der absolute Hostpfad dient nur als Umgebungsnachweis und wird an
keiner Stelle in Produktionsdateien eingecheckt.

## Ziel

Ausschliesslich eine Planungsphase durchfuehren und danach anhalten: aktuellen
Stand pruefen (dieser Plan), die kleinstmoegliche tragfaehige
ESP-IDF-6.0.2-Buildbasis planen, den Draft-PR mit genau dieser Plan-Datei
pflegen, nach Push anhalten. Fachliches Ergebnis nach Freigabe und Umsetzung:
ein reproduzierbarer, **lokal** nachgewiesener ESP-IDF-6.0.2-Konfigurations-,
Compile- und Linkpfad fuer die bestehenden portablen Komponenten
`device_platform` und `fermentation_app`, parallel zum weiterhin gruenen
PlatformIO-/Arduino-Pfad. Keine CI-Aenderung, keine reale Laufzeitparitaet,
keine Hardwareintegration.

## Nicht-Ziele (Nicht-Scope von `#72`)

- produktive Laufzeitparitaet und echter Composition Root aus `#73`;
- echte Zeit-, Logging-, Reset-, Task-, Watchdog- oder Ressourcenadapter;
- jede Aenderung an `src/main.cpp` (siehe Abschnitt 2);
- jede Aenderung an `.github/workflows/build.yml` oder sonstige
  CI-Migration; diese bleibt vollstaendig `#74` vorbehalten;
- Entfernung des Arduino-Hauptframework-Pfads;
- Arduino-ESP32 als IDF-Komponente oder Auswahl einer Arduino-Bibliothek;
- NVS, GPIO, LEDC, Sensor, Display, Touch, WLAN oder Web;
- reale Partitionstabelle ohne technischen Zwang;
- Hardwaretests;
- Aenderungen an Fachmodellen, Safetyvertraegen, `#57`-Semantik oder
  Wireformaten;
- ESPRelayBoard, separates Device-Platform-Repository oder Template;
- `dependencies.lock`, `idf_component.yml` oder jede Fremdkomponente;
- Whole-Archive- oder sonstige Sonderkonstruktionen nur zum Zweck eines
  vollstaendigen symbolischen Linknachweises.

## Verbindliche Quellen und Entscheidungen

### Direkte Primaerquellen (keine allgemeine Websuche)

| Thema | Quelle |
|---|---|
| Repository / Release-Tag | <https://github.com/espressif/esp-idf> · <https://github.com/espressif/esp-idf/releases/tag/v6.0.2> |
| Build System | .../v6.0.2/esp32/api-guides/build-system.html |
| C++ Support | .../v6.0.2/esp32/api-guides/cplusplus.html |
| Migration 6.0 (Build System, Toolchain) | .../v6.0.2/esp32/migration-guides/release-6.x/6.0/ |
| Component Manager | <https://docs.espressif.com/projects/idf-component-manager/en/latest/> |
| Minimalstruktur-Referenz | <https://github.com/espressif/esp-idf/tree/v6.0.2/examples/get-started/hello_world> |

Fuer diesen Plan tatsaechlich entscheidungsrelevante Kernbefunde:

- `EXTRA_COMPONENT_DIRS` muss **vor** `include($ENV{IDF_PATH}/tools/cmake/project.cmake)`
  gesetzt werden, damit ESP-IDF Komponentenverzeichnisse ausserhalb der
  Konvention `components/` findet; `idf_build_set_property(MINIMAL_BUILD ON)`
  gehoert zwischen dieses Include und `project(...)` und baut nur `main`, die
  ESP-IDF-Common-Komponenten sowie deren transitive Abhaengigkeiten.
- `idf_component_register(SRC_DIRS ... INCLUDE_DIRS ... REQUIRES ...
  PRIV_REQUIRES ...)`: `REQUIRES` propagiert oeffentliche Header transitiv,
  `PRIV_REQUIRES` bleibt intern; `SRC_DIRS` sammelt alle `.c`/`.cpp`/`.S`-
  Dateien eines Verzeichnisses automatisch ein, ohne manuelle Einzelliste.
- Der Buildproperty-Report von ESP-IDF unterscheidet `IDF_VERSION_MAJOR/MINOR/
  PATCH` (nur Zahlen, unterscheidet nicht Release/RC/Beta/Dev) von `IDF_VER`
  (vollstaendiger, praeziserer Versionsstring, per `idf_build_get_property`
  nach `project(...)` lesbar).
- ESP-IDF 6.0.2 kompiliert C++ standardmaessig mit `-std=gnu++26`; ein
  abweichender Standard wird pro Komponente ueber `target_compile_options`
  gesetzt. Exceptions und RTTI sind standardmaessig deaktiviert. Die
  ESP-IDF-Common-Komponente `cxx` bindet die POSIX-Threads-Unterstuetzung
  (Komponente `pthread`) bereits intern fuer `libstdc++` ein.
- Migration 6.0: Orphan-ELF-Sections erzeugen jetzt einen Buildfehler; globale
  Konstruktoren laufen jetzt aufsteigend statt absteigend;
  `CONFIG_COMPILER_DISABLE_DEFAULT_ERRORS` ist jetzt deaktiviert (Warnungen
  sind Fehler, deckt sich mit dem bestehenden `-Werror`). GCC wurde von
  14.2.0 auf 15.1.0 angehoben (neue C++-Warnungen moeglich).
- Component Manager: `dependencies.lock` wird beim Konfigurieren erzeugt,
  nicht manuell editiert, und bleibt bei Registry-/Git-Abhaengigkeiten
  grundsaetzlich versioniert; `managed_components/` ist generierter
  Buildbestand.

### Projektentscheidungen (`AGENTS.md`, ADRs)

ADR-013 und die Modul-`AGENTS.md` unter `lib/device_platform/`,
`lib/device_platform_test_support/` und `lib/fermentation_app/` bleiben
unveraendert verbindlich. `docs/ADOPT_OR_BUILD.md`: vor Eigenentwicklung wird
zuerst Frameworkfunktion, dann offizielles Herstellerpaket, dann gepflegte
Fremdbibliothek, erst dann Eigenentwicklung geprueft – fuer `#72` bedeutet
das: keine neue Fremdkomponente, nur der offizielle ESP-IDF-Kern.
`scripts/check_architecture_boundaries.py` erzwingt bereits die
ADR-013-Grenzen und wird in `#72` nur um eine schmale IDF-Leak-Pruefung
erweitert (Abschnitt 9).

## Verbindliche Architekturkorrektur

Die im Issue-`#71`-Text verwendete lineare Darstellung
`fermentation_app -> device_platform -> device_platform_esp_idf -> ESP-IDF`
ist als Build-Abhaengigkeitsrichtung missverstaendlich und wird nicht
uebernommen. `device_platform` haengt niemals vom ESP-IDF-Adapter ab.
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
  `device_platform` ab; `device_platform_esp_idf` implementiert diese Ports
  und haengt von `device_platform` sowie ESP-IDF ab; der Composition Root
  kennt Anwendung und konkrete Adapter; `device_platform` kennt weder
  `device_platform_esp_idf` noch ESP-IDF; `device_platform_test_support`
  bleibt ausschliesslich Testcode.
- Keine IDF- oder Arduino-Typen in stabilen Ports, Fachmodellen, Wireformaten
  oder persistenten Modellen; keine privaten/internen ESP-IDF-APIs; keine
  generische Schatten-API; keine vorsorglichen Erweiterungspunkte.

`device_platform_esp_idf` wird in `#72` **nicht angelegt**. `device_platform`
ist bereits vollstaendig anwendungsneutral ohne jede ESP32-/Arduino-Bindung –
es gibt noch keinen produktiven ESP32-Adapter, der diese Grenze fuellen
wuerde. Der erste reale Adapter entsteht in `#73`; `#72` weist nur nach, dass
der bestehende portable Code unter ESP-IDF 6.0.2 kompiliert und ein gueltiges
Image linkt (Praezisierung siehe Abschnitt 2).

## 1. Kompakte Bestandsaufnahme

| Aspekt | Befund |
|---|---|
| Arduino-Kopplung | Ausschliesslich `src/main.cpp` (`Arduino.h`, `setup()`, `loop()`, `Serial`, `millis()`, hinter `#if defined(ARDUINO)`/`#else`). Kein Treffer in `lib/` oder `include/`. |
| ESP32-Profile (`platformio.ini`) | `native`, `esp32_bringup`, `esp32_release`; alle `-std=gnu++17`; Bringup/Release unterscheiden sich nur durch `APP_PROFILE_*`-Makro und `app_config`-Policy, nicht durch Sprachstandard oder Zielhardware. |
| Buildflags/Sprachstandard | `-std=gnu++17`; feste Makros `APP_TARGET_FLASH_MB=4`, `APP_REQUIRE_PSRAM=0`, `APP_WEB_OTA_ENABLED=0`, `APP_REAL_ACTUATORS_ENABLED=0`. Diese Makros werden ausschliesslich von `include/app_config.hpp` konsumiert, das wiederum ausschliesslich von `src/main.cpp` includiert wird. |
| Produktive Translation Units im ESP32-Image | `src/main.cpp` plus alle Quellen unter `lib/device_platform/src/` (7 `.cpp`-Dateien, Rest Header) und `lib/fermentation_app/src/` (47 Dateien). |
| Native Testabhaengigkeiten | `test_framework = unity`; 22 Testverzeichnisse; Tests haengen zusaetzlich von `lib/device_platform_test_support/` ab, das laut ADR-013 nie in ein Produktivbuild gelangen darf. |
| Bestehende Architektur-/Quality-Gates | `scripts/check_architecture_boundaries.py`, `scripts/check_platformio_config.py`, `scripts/check_secrets.py`, `scripts/selftest_quality_gates.py`; CI faehrt `clang-format`, `build_report.py` fuer alle drei PlatformIO-Envs, `pio test -e native`, `clang-tidy`, Architektur-, Secret- und Selftest-Pruefung. |
| Bereits vollstaendig portabler Code | `lib/device_platform/` (kein ESP32-/Arduino-Praeprozessorzweig ausser drei rein erklaerenden Kommentaren), `lib/fermentation_app/` (haengt laut eigenem `AGENTS.md` nur von `device_platform`-Ports ab), `include/app_config.hpp` (reine `constexpr`/Praeprozessor-Policy, keine Arduino-/ESP-IDF-Bindung, aber ausschliesslich fuer `main.cpp` relevant). |
| Erst in `#73` anzupassende Stellen | `src/main.cpp` bleibt in `#72` vollstaendig unveraendert (siehe Abschnitt 2); ein produktiver ESP-IDF-Composition-Root mit `app_main()`, echten Adaptern und `device_platform_esp_idf` entsteht dort, nicht in `#72`. |
| Codebefund Exceptions/RTTI/Threading | Kein `throw`, `try`/`catch`, `dynamic_cast`, `typeid(` in Produktionscode (Grep-verifiziert). `std::mutex` wird genau einmal verwendet (`configuration_service.hpp`); ESP-IDFs `cxx`-Common-Komponente bindet dafuer bereits intern `pthread` ein, sodass `fermentation_app` keine eigene direkte `pthread`-Abhaengigkeit braucht. Keine `<iostream>`, `<sstream>`, `<filesystem>`, `<thread>`. |

Die Migration betrifft nicht nur `main.cpp`: `#72` muss zusaetzlich beweisen,
dass die gesamte bestehende `device_platform`- und `fermentation_app`-
Quellbasis (54 Uebersetzungseinheiten) unter dem ESP-IDF-6.0.2-Toolchain
(GCC 15.1.0, `gnu++17`-Override, deaktivierte Exceptions/RTTI) tatsaechlich
kompiliert.

### Herstellerbasis-Entscheid (Kurzform)

| Kandidat | Status fuer `#72` |
|---|---|
| `esp-idf` | ausgewaehlte Plattformbasis |
| `esp-iot-solution`, `esp-bsp`, `idf-extra-components`, ESP Component Registry | Komponentenquellen fuer spaetere konkrete Spikes, keine Einbindung in `#72` |
| `esp-rainmaker`, `esp-matter`/`esp-lowcode-matter`, `esp-brookesia`, `esp-at`, `esp-zigbee-sdk` | gepruefte, nicht passende vollstaendige Solutions; keine Firmwarebasis |

Ergebnis: Dieses Repository bleibt die Produktfirmware, `espressif/esp-idf @
v6.0.2` ist die Plattformbasis, keine vollstaendige Espressif-Solution wird
geforkt. Diese Grundsatzentscheidung wird in `#72` nicht erneut mit einer
offenen Websuche aufgerollt.

## 2. Kleinstmoeglicher Scope von `#72`

Begrenzung auf genau das, was fuer einen reproduzierbaren, **lokal**
ausgefuehrten ESP-IDF-Compile-/Linkpfad noetig ist:

- gepinnte native ESP-IDF-6.0.2-Installation als Entwicklerumgebung nutzen
  (bereits vorhanden);
- minimale Top-Level-`CMakeLists.txt` mit `EXTRA_COMPONENT_DIRS`,
  `MINIMAL_BUILD` und `IDF_VER`-Guard (Abschnitt 4);
- Registrierung von `lib/device_platform/` und `lib/fermentation_app/` als
  ESP-IDF-Komponenten ohne Dateiumzuege und ohne manuelle Vollquelllisten
  (Abschnitt 3);
- ein gemeinsames, versioniertes `sdkconfig.defaults`;
- ein temporaerer, von der Fachanwendung entkoppelter Build-only-Stub;
- Erhalt des bestehenden PlatformIO-/Arduino-Vergleichspfads unveraendert;
- minimale Dokumentationsergaenzung in einer bestehenden Datei.

**Kein** CI-Schritt, keine `.github/workflows/build.yml`-Aenderung: `#74`
besitzt bereits ausdruecklich die ESP-IDF-Toolchain in GitHub Actions, CI-
Artefakte und die CI-Migration. `#72` liefert stattdessen einen lokal
reproduzierbar dokumentierten `idf.py build` als Implementierungs- und
Reviewnachweis (Abschnitt 9, Abschnitt „Dokumentationsaenderungen“).

### Build-only-Stub getrennt von `src/main.cpp`

`src/main.cpp` bleibt in `#72` **vollstaendig unveraendert**. Ein zusaetzlicher
Praeprozessorzweig dort wuerde den bestehenden Arduino-/Native-Composition-
Root fuer einen temporaeren Buildtest aendern, die globalen `DevicePlatform`-/
`FermentationApplication`-Instanzen auch im Build-only-Pfad konstruieren und
die fuenf `APP_PROFILE_*`-Makros ein zweites Mal im CMake-Buildsystem
verlangen. Stattdessen entsteht unter der ohnehin noetigen ESP-IDF-`main`-
Komponente eine einzige, ausdruecklich temporaere Datei:

`main/issue72_build_stub.c`

```c
void app_main(void) {}
```

Keine Includes, keine globalen Projektobjekte, keine Tasks, kein Aufruf der
Anwendung, keine `APP_PROFILE_*`-Makros. `main/CMakeLists.txt` registriert nur
diesen Stub und erklaert `device_platform` sowie `fermentation_app` als
private Buildabhaengigkeiten (siehe Abschnitt 3). Datei und Stub werden in
`#73` entfernt und durch den echten `app_main()`-Composition-Root ersetzt.
Das ist SRP und KISS: Der Buildanker prueft nur die Buildbasis; der
bestehende Composition Root bleibt unangetastet.

### Reichweite des Linknachweises

`device_platform` und `fermentation_app` werden von `main` ausschliesslich
als `PRIV_REQUIRES` angefordert; der Stub referenziert keine ihrer Symbole.
`#72` weist damit nach: alle vorgesehenen Produktionsquellen werden
erfolgreich als Teil ihrer ESP-IDF-Komponente kompiliert, die
Komponentenabhaengigkeiten werden CMake-seitig aufgeloest, und der Build-only-
Einstieg linkt zu einem gueltigen ESP-IDF-Image. `#72` weist **nicht** nach,
dass jedes einzelne unreferenzierte Objekt aus den Komponentenbibliotheken im
finalen ELF verlinkt ist – dafuer waere eine kuenstliche
`--whole-archive`-Konstruktion noetig, die hier bewusst nicht eingefuehrt
wird. Vollstaendige symbolische Laufzeitverlinkung und reale
Composition-Root-Paritaet werden in `#73` nachgewiesen.

## 3. Komponentenstruktur und Abhaengigkeiten

Root-`CMakeLists.txt` (Ablauf, Namen ggf. an Projektkonvention anpassbar):

```cmake
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/lib/device_platform"
    "${CMAKE_CURRENT_LIST_DIR}/lib/fermentation_app"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(esp32_fermentationsschrank)

idf_build_get_property(idf_ver IDF_VER)
if(NOT idf_ver STREQUAL "v6.0.2")
    message(FATAL_ERROR "ESP-IDF v6.0.2 required, active version is '${idf_ver}'")
endif()
```

`EXTRA_COMPONENT_DIRS` benennt ausschliesslich die beiden konkreten Pfade;
`lib/` wird nicht pauschal registriert. `lib/device_platform_test_support/`
wird dadurch strukturell aus dem Produktbuild ausgeschlossen, da es weder
unter `components/` liegt noch in `EXTRA_COMPONENT_DIRS` genannt wird.

| Komponente | Verzeichnis | `idf_component_register` | `REQUIRES` | `PRIV_REQUIRES` |
|---|---|---|---|---|
| `device_platform` | `lib/device_platform/` (neu: `CMakeLists.txt`) | `SRC_DIRS "src"`, `INCLUDE_DIRS "src"` | keine | keine |
| `fermentation_app` | `lib/fermentation_app/` (neu: `CMakeLists.txt`) | `SRC_DIRS "src"`, `INCLUDE_DIRS "src"` | `device_platform` | keine |
| `main` (ESP-IDF-Component) | neu: `main/CMakeLists.txt` | `SRCS "issue72_build_stub.c"` | keine | `device_platform`, `fermentation_app` |
| `device_platform_test_support` | `lib/device_platform_test_support/` | **nicht Teil des ESP-IDF-Builds** (kein `EXTRA_COMPONENT_DIRS`-Eintrag) | – | – |
| `device_platform_esp_idf` | – | **noch nicht angelegt**, Zielgrenze fuer `#73` | – | – |

`SRC_DIRS "src"` sammelt alle vorhandenen `.cpp`-Dateien automatisch ein,
statt sie ein zweites Mal manuell in der CMake-Datei aufzulisten (siehe
Abschnitt „SOLID/DRY/KISS“). Nach Hinzufuegen oder Entfernen einer
Quelldatei ist lokal `idf.py reconfigure` bzw. ein sauberer Build noetig;
das wird in der Dokumentationsergaenzung (Abschnitt „Dokumentations-
aenderungen“) knapp vermerkt. Sollte `SRC_DIRS` wegen der tatsaechlichen
Verzeichnisstruktur bei der Umsetzung nicht ausreichen, wird stattdessen eine
explizite Liste mit einem kleinen Vollstaendigkeitsnachweis gegen die realen
`.cpp`-Dateien verwendet – keine ungeschuetzte Kopie.

## 4. Toolchain- und Versionsvertrag

- Die bereits vorhandene native ESP-IDF-Installation (Tag `v6.0.2`, Commit
  `7101770dc6db2667b3c477cc31365dd1acd6db4e`, per `export.sh` aktiviert) ist
  die Entwicklerumgebung; sie wird nicht erneut installiert oder aktualisiert.
- Einzige Quelle der Wahrheit fuer den Versionspin ist der `IDF_VER`-Guard
  direkt im Root-`CMakeLists.txt` (Abschnitt 3) – kein separater `cmake/`-
  Ordner, keine zusaetzliche Hilfsdatei nur fuer diese eine Pruefung.
  `IDF_VER` wird bewusst statt `IDF_VERSION_MAJOR/MINOR/PATCH` verwendet, da
  Letztere Release/RC/Beta/Dev-Staende mit gleichen Zahlen nicht
  unterscheiden.
- Build und Skripte verwenden ausschliesslich die aktivierte `IDF_PATH`-
  Umgebung und `idf.py`; kein PlatformIO-ESP-IDF-Paket als zweite Quelle der
  Wahrheit; kein hart kodierter Hostpfad in Produktionsdateien.
- Ein spaeterer offizieller CI-Pfad wird vollstaendig in `#74` festgelegt;
  `#72` nimmt ihn nicht vorweg.

## 5. C++-Standard und Compilervertrag

ESP-IDF 6.0.2 kompiliert C++ standardmaessig mit `-std=gnu++26`; das Projekt
verwendet `gnu++17`. Da `main/issue72_build_stub.c` reines C ist, betrifft
das nur die beiden C++-Komponenten:

```cmake
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")
```

je einmal in `lib/device_platform/CMakeLists.txt` und
`lib/fermentation_app/CMakeLists.txt`. `PRIVATE` genuegt: Es liegt kein
konkreter oeffentlicher Headervertrag vor, der einen exakt identischen
Sprachstandard bei Konsumenten erzwingen muesste – `PUBLIC` wuerde hier
lediglich zusaetzliche, unbelegte Kopplung einfuehren. Die kleine, identische
Wiederholung dieses einen Einzeilers in zwei ohnehin zwingend noetigen
Komponenten-`CMakeLists.txt`-Dateien ist verstaendlicher als eine eigene
CMake-Hilfsdatei nur fuer zwei Vorkommen (KISS); der Wert `gnu++17` bleibt
textidentisch mit `platformio.ini`. Eine spaetere bewusste Standardanhebung
bleibt ein eigenes, separat zu begruendendes Vorhaben.

Exceptions und RTTI bleiben auf dem ESP-IDF-Standarddefault (aus); der
Codebefund in Abschnitt 1 bestaetigt, dass kein Produktionscode sie
benoetigt.

## 6. Konfigurationsprofile

- Ein gemeinsames, versioniertes `sdkconfig.defaults` im Repository-Root:
  `CONFIG_IDF_TARGET="esp32"` (ESP32-WROOM-32E, passend zu `board=esp32dev`),
  4 MB Flash, keine PSRAM-Aktivierung. Kein Bring-up-/CI-/Release-Overlay, da
  `#72` keine reale funktionale Kconfig-Differenz zwischen diesen Stufen
  besitzt.
- Da `main/issue72_build_stub.c` `app_config.hpp` nicht includiert (siehe
  Abschnitt 2), sind fuer `#72` **keine** `APP_PROFILE_*`- oder sonstigen
  `app_config`-Makros im ESP-IDF-Build noetig; die fruehere geplante
  Makro-Dopplung zwischen `platformio.ini` und CMake entfaellt vollstaendig.
- Reale Partitionierung: ESP-IDFs eingebaute Default-Partitionstabelle passt
  ohne Projektueberschreibung in 4 MB; keine `partitions.csv` in `#72`.
- Keine Arduino-spezifischen Kconfig-Werte, keine unterschiedliche Fach- oder
  Safetysemantik zwischen Profilen; `app_config.hpp`s bestehende
  `static_assert`-Vertraege bleiben unveraendert die einzige Quelle fuer
  diese Fachgrenzen und sind von `#72` gar nicht betroffen.

## 7. Component Manager und Fremdkomponenten

Fuer `#72` werden keine externen Komponenten benoetigt: kein
`idf_component.yml`, kein `dependencies.lock`. Spaeterer Trigger (nur
dokumentiert, nicht umgesetzt): `idf_component.yml` entsteht mit der ersten
realen verwalteten Fremdkomponente; das dabei vom Solver erzeugte
`dependencies.lock` wird nicht manuell editiert und bei Registry-/Git-
Abhaengigkeiten grundsaetzlich **versioniert** (nicht ignoriert); lokale
pfadabhaengige Lockfiles werden vor Versionierung auf Portabilitaet geprueft;
jedes Lockfile-Delta wird mit Lizenz-, Notice- und Abhaengigkeitspruefung
gemaess `docs/ADOPT_OR_BUILD.md` verbunden.

## 8. Dual-Build-Uebergang

Zeitlich begrenzt bestehen zwei parallele, **beide lokal auszufuehrende**
Pfade: der bestehende PlatformIO-/Arduino-Build (`native`, `esp32_bringup`,
`esp32_release`) und der neue reine ESP-IDF-6.0.2-Pfad (`idf.py build`).
Portable Quellen unter `lib/device_platform/src/` und
`lib/fermentation_app/src/` werden von beiden Pfaden gebaut. Da `#72` keine
CI-Aenderung vornimmt, ist Divergenzerkennung in dieser Phase eine lokale
Entwicklerpflicht (beide Pfade vor jedem Commit ausfuehren, siehe
„Abnahmekriterien“); ein automatisierter, gemeinsamer CI-Nachweis beider
Pfade entsteht erst mit der CI-Migration in `#74`. Bewusst erst `#73`: echte
Laufzeitparitaet, `device_platform_esp_idf`. Bewusst erst `#74`: Entfernung
des Arduino-Hauptframework-Pfads, CI-Migration. Der Arduino-Hauptframework-
Pfad bleibt strikt getrennt von einem moeglichen spaeteren, ownerfreigegebenen
Arduino-as-ESP-IDF-component-Adapter; `#72` bindet keinen von beiden als
IDF-Komponente ein.

## 9. Quality Gates – risikobasiert und KISS

| Gate | Umsetzung |
|---|---|
| Native Tests unveraendert gruen | `pio test -e native` unveraendert |
| Bestehende PlatformIO-Builds gruen | `esp32_bringup`, `esp32_release` unveraendert |
| ESP-IDF-6.0.2-Konfigurations-/Compile-/Linkbuild gruen | lokal `idf.py build`; **kein** CI-Schritt in `#72` |
| Format-/Static-Analysis | `clang-format --dry-run` deckt `src`/`include`/`lib`/`test` bereits ab; die drei neuen `CMakeLists.txt` und der C-Stub sind kein `clang-format`-Ziel im bestehenden Umfang |
| Architekturgrenzen inkl. IDF-Leak | `scripts/check_architecture_boundaries.py` erhaelt eine zusaetzliche Pruefung, beschraenkt auf `lib/device_platform/src/` und `lib/fermentation_app/src/`: verbotene Include-Praefixe (`esp_`, `driver/`, `freertos/`, `lwip/`, `hal/`, `soc/`, `nvs`, `sdkconfig.h`) sowie echte Praeprozessorverwendung von `ESP_PLATFORM`, `ARDUINO` oder `CONFIG_*`-Symbolen (`#ifdef`/`#if defined(...)`, nicht blosser Teilstring-Treffer); zusaetzlich werden `lib/device_platform/CMakeLists.txt` und `lib/fermentation_app/CMakeLists.txt` gegen eine kleine Allowlist erlaubter `REQUIRES`/`PRIV_REQUIRES`-Namen geprueft (leer bzw. `device_platform`). Genau ein positiver Clean-Fixture-Nachweis und ein gezielter negativer IDF-Leak-Selftest, analog zum bestehenden Selftest-Muster; keine Permutationsmatrix. |
| Secretpruefung, `git diff --check` | unveraendert `scripts/check_secrets.py`, `git diff --check` |
| Fail-fast-Versionspin | `IDF_VER`-Guard im Root-`CMakeLists.txt`; manueller Gegentest mit absichtlich falscher Zielversion, keine dauerhafte neue Testdatei nur dafuer |

Keine Hardwaretests, keine Heap-/Stack-/Watchdog-/NVS-/Flashlebensdauer-
Garantie fuer diese reine Buildstruktur.

## 10. Ressourcen- und Regressionsnachweis (Strukturbaseline, keine Paritaet)

Ein leerer IDF-Buildanker ist funktional nicht mit dem bisherigen
Arduino-Laufzeitbuild gleichwertig; Flash-/RAM-Werte bilden deshalb
**ausdruecklich keinen** Regressions- oder Paritaetsvergleich. Fuer `#72` wird
nur eine technische Strukturbaseline erfasst: IDF-BIN, ELF, Mapfile und
`idf.py size` fuer den build-only Stub, mit dem vorhandenen Arduino-
`build_report.py`-Ergebnis nur als Kontext danebengestellt, ausdruecklich als
„nicht funktionsparitaetisch, nicht als Regressionsgrenze verwendbar“
gekennzeichnet. Keine Prozentgrenze, kein Bestehen aufgrund kleinerer
Stub-Groesse. Keine neue Ressourceninstrumentierung im Fach- oder
Recoverycode. Echte Paritaets- und Ressourcenbewertung bleibt `#74`.

## 11. Adopt-or-build-Pruefung

1. Dieses Repository bleibt die Produktfirmware; ESP-IDF 6.0.2 ist die
   ausgewaehlte Herstellerplattform.
2. Fuer einen konkreten Bedarf werden zuerst ESP-IDF-Kernkomponenten und
   danach passende Komponenten aus ESP Component Registry,
   `esp-iot-solution`, `esp-bsp` und `idf-extra-components` geprueft.
3. Vollstaendige Espressif-Solutions werden nicht als Firmwarebasis
   uebernommen; einzelne Bausteine benoetigen einen konkreten Konsumenten und
   eigenen Spike.
4. Eigene technische Implementierung erfolgt erst, wenn kein geeigneter
   Baustein den konkreten Vertrag erfuellt.
5. Arduino-basierte Bibliotheken bleiben nur kuenftige adapterlokale
   Kandidaten; `#72` bindet weder Arduino noch eine andere Fremdkomponente
   ein.

```text
ESP-IDF 6.0.2: ausgewaehlte und exakt gepinnte Plattformbasis
Arduino als Hauptframework: nur temporaerer Vergleichspfad bis #74
Arduino-ESP32 als IDF-Komponente: nicht eingebunden und aktuell nicht als
kompatible 6.0.2-Baseline verfuegbar
Espressif-Solution-Fork: keiner
Fremdkomponenten: keine in #72
```

## 12. Kleiner Implementierungs- und Commit-Schnitt

| # | Commit | Inhalt |
|---|---|---|
| 1 | Buildbare ESP-IDF-Basis | `CMakeLists.txt` (Root, inkl. `EXTRA_COMPONENT_DIRS`, `MINIMAL_BUILD`, `IDF_VER`-Guard), `main/CMakeLists.txt`, `main/issue72_build_stub.c`, `lib/device_platform/CMakeLists.txt`, `lib/fermentation_app/CMakeLists.txt`, `sdkconfig.defaults`, minimale `.gitignore`-Ergaenzung; lokaler `idf.py build` sowie bestehende PlatformIO-/Native-Gates gruen |
| 2 | Grenzcheck und Dokumentation | schmale Erweiterung von `scripts/check_architecture_boundaries.py` (IDF-Leak-Check plus ein Selftest-Fall), kurze Ergaenzung in `docs/CI_AND_QUALITY_GATES.md`, knapper `CHANGELOG.md`-Eintrag |

Kein dritter Commit, da keine weitere eigenstaendig pruefbare Verantwortung
uebrig bleibt; insbesondere kein CI-Commit in `#72`.

## 13. Erwartete Dateiliste

**Neu:** `CMakeLists.txt`, `main/CMakeLists.txt`,
`main/issue72_build_stub.c`, `lib/device_platform/CMakeLists.txt`,
`lib/fermentation_app/CMakeLists.txt`, `sdkconfig.defaults`.

**Geaendert:** `.gitignore` (nur fehlende generierte IDF-Bestaende:
`managed_components/`, `sdkconfig`, `sdkconfig.old`; `build/` ist bereits
vorhanden und wird nicht doppelt eingetragen; `dependencies.lock` wird nicht
aufgenommen, da es grundsaetzlich versioniert wird, sobald es entsteht),
`scripts/check_architecture_boundaries.py` (schmale IDF-Leak-Grenze plus ein
Selftest), `docs/CI_AND_QUALITY_GATES.md` (kurze Aktivierungs- und lokale
Buildanleitung), `CHANGELOG.md` (knapper Eintrag).

**Nicht geaendert:** `src/main.cpp`, `.github/workflows/build.yml`,
`platformio.ini`, Fach-, Safety-, Persistenz- und Recoverycode,
`device_platform_test_support`, Issues `#71`–`#74`, externer ESP-IDF-Checkout.

Vermieden: neue allgemeine Frameworkordner ohne Konsumenten, leere Manifeste
oder Komponenten (`device_platform_esp_idf` entsteht nicht), mehrere fast
identische Buildskripte, kopierte Quelllisten, neue Test-Suites fuer reine
Buildmetadaten, eine eigene Aktivierungsdokumentdatei.

## Daten-, Zustands- und Schnittstellenvertraege

`#72` aendert keine Fach-, Wire- oder Persistenzformate und keine
oeffentliche Schnittstelle. Der leere `app_main()`-Stub initialisiert keinen
Zustand; die bestehenden `IStateStore`-, Zeit-, Aktor- und sonstigen
`device_platform`-Ports bleiben unveraendert und ohne neuen Implementor.

## Fehler-, Recovery-, Security- und Safetygrenzen

Peltier/H-Bruecke bleiben unberuehrt: `#72` enthaelt keinen Aktorcode, kein
GPIO, keine neue Adapterklasse. Der ESP-IDF-Stub initialisiert keine
Hardware und ruft keine Anwendungslogik auf; ein fehlschlagender lokaler
`idf.py build` blockiert nur diesen lokalen Nachweis, nicht den bestehenden
PlatformIO-Pfad. Keine neue Security-Flaeche.

## Teststrategie

- Bestehende native Testsuite bleibt unveraendert massgeblich; `#72`
  erweitert oder beschraenkt sie nicht.
- Neuer Nachweis ist ein lokales Buildgate (`idf.py build` fuer den
  build-only Stub), kein neuer Unit-Test.
- Genau ein neuer Selftest-Fall im erweiterten Architekturcheck
  (IDF-Include-/Kconfig-Leak in den portablen Wurzeln wird erkannt, saubere
  Fixture bleibt `PASS`).
- Ein manueller Fail-fast-Nachweis (absichtlich falsche `IDF_VER` im Test)
  bestaetigt den Versionsguard; keine dauerhafte neue automatisierte
  Testdatei nur dafuer.

## Dokumentationsaenderungen

- Kurzer neuer Abschnitt in `docs/CI_AND_QUALITY_GATES.md` (bestehendes
  Dokument, keine neue Datei): Aktivierung der bereits installierten nativen
  Toolchain (`. ${IDF_PATH}/export.sh` bzw. ein frei gewaehlter externer
  Checkoutpfad, kein kanonischer persoenlicher Hostpfad) und `idf.py build`
  als lokaler Nachweis; Hinweis auf `idf.py reconfigure` nach neuen/entfernten
  Quelldateien.
- `CHANGELOG.md`: ein knapper Eintrag fuer die neue parallele
  ESP-IDF-6.0.2-Buildbasis.
- Dieser Plan bleibt bis zum Abschlussreview im Branch; die Owner-
  Entscheidung ueber Verbleib, Ueberfuehrung oder Entfernung vor dem Merge
  folgt `AGENTS.md`.

## 14. SOLID-, DRY- und KISS-Pruefung (gebunden an den korrigierten Diff)

- **SRP:** Der temporaere C-Stub ist nur Buildanker; `src/main.cpp` bleibt
  unveraendert Composition Root; jede Komponenten-`CMakeLists.txt`
  registriert genau ein bestehendes Modul.
- **OCP:** Die portable Fach- und Plattformlogik wird fuer die neue
  Toolchain nicht veraendert; der neue Buildweg kommt additiv ueber
  CMake-Komponenten hinzu.
- **LSP:** Keine neuen Portimplementierungen und keine Vertragsaenderung in
  `#72`.
- **ISP:** Keine neue IDF-Schatten-API, kein leerer Universaladapter.
- **DIP:** `fermentation_app -> device_platform`; der portable Kern besitzt
  keine IDF-Abhaengigkeit; der reale ESP-IDF-Adapter entsteht erst in `#73`.
- **DRY:** `SRC_DIRS` statt manuell doppelt gepflegter Quelllisten, ein
  einziger `IDF_VER`-Guard direkt im Root-`CMakeLists.txt`, kein
  widerspruechlich ignoriertes `dependencies.lock`, keine doppelten
  `APP_PROFILE_*`-Makros (durch die main.cpp-Trennung vollstaendig entfallen).
- **KISS:** Minimalbuild, zwei explizite Komponentenpfade, ein leerer
  C-Stub, keine CI-Migration, keine Helperdatei nur fuer einen kurzen Guard,
  keine neue Aktivierungsdatei, keine Testmatrix.

## Offene Ownerentscheidungen

Keine. Die drei zuvor offenen Punkte sind durch dieses Review aufgeloest:
Dokumentation nutzt `docs/CI_AND_QUALITY_GATES.md` (kein neues Dokument);
`fermentation_app` erhaelt keine direkte `pthread`-Abhaengigkeit (von der
ESP-IDF-`cxx`-Komponente bereits abgedeckt); `#72` nimmt keine
CI-Workflowaenderung vor, das ist vollstaendig `#74` vorbehalten.

## TBD-/Platzhalterstatus

Fuer `#72` verbleibt kein `SPIKE_REQUIRED`, `TBD_HARDWARE`,
`TBD_COMMISSIONING` oder `FINAL_SELECTION_PENDING`: Der Plan trifft fuer den
reinen Compile-/Linknachweis ausschliesslich Entscheidungen, die bereits
durch bestehende Projektdokumente oder direkt verifizierte offizielle
ESP-IDF-6.0.2-Quellen gedeckt sind.

## Abnahmekriterien

- Lokal (nicht in CI): `idf.py build` konfiguriert, kompiliert und linkt den
  build-only Stub fehlerfrei und kompiliert dabei nachweislich alle
  Uebersetzungseinheiten aus `lib/device_platform/src/` und
  `lib/fermentation_app/src/` als Teil ihrer Komponentenbibliothek (kein
  Anspruch auf vollstaendige ELF-Symbolverlinkung jeder Datei, siehe
  Abschnitt 2).
- `native`, `esp32_bringup`, `esp32_release` bleiben unveraendert gruen.
- Der `IDF_VER`-Guard im Root-`CMakeLists.txt` bricht den Konfigurationslauf
  bei einer anderen ESP-IDF-Version als `v6.0.2` nachweislich ab (manueller
  Gegentest).
- `scripts/check_architecture_boundaries.py` erkennt einen absichtlichen
  IDF-Include-/Kconfig-Leak in den portablen Wurzeln (neuer Selftest-Fall)
  und meldet fuer den freigegebenen Diff weiterhin `PASS`.
- `clang-format --dry-run --Werror`, `scripts/check_platformio_config.py`,
  `scripts/check_secrets.py`, `scripts/selftest_quality_gates.py` und
  `git diff --check` bleiben gruen.
- `.github/workflows/build.yml` bleibt byteidentisch unveraendert.
- Keine Aenderung ausserhalb der in Abschnitt 13 gelisteten Dateien (fuer die
  Planungsphase selbst: ausschliesslich diese Plan-Datei).
- `device_platform_test_support` ist nachweislich nicht Teil des ESP-IDF-
  Komponentenbaums (kein `EXTRA_COMPONENT_DIRS`-Eintrag, kein
  `CMakeLists.txt` in diesem Verzeichnis).
