# Plan: Issue #74 – CI, Ressourcenbaseline und ESP-IDF-Upgradevertrag

Status: Planungsphase (`IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`)

Issue: `#74`
Tracking: `#71`
Basis-`main`-SHA: `c3f8044be0f081822ca8724f67fa99e9614d57ef` (Merge-Commit von PR #78)

Diese Fassung überarbeitet den ursprünglichen Plan-Commit
`05b987e3d2b375b82922990f718d0dc07c730a71` nach einem vollständigen Review
(`PLAN_REVIEW: CHANGES_REQUIRED`) auf PR #79. Der alte Plan-Commit bleibt in
der Historie und ist überholt; diese Fassung ersetzt ihn inhaltlich.

## 1. Ziel

Die ESP-IDF-6.0.2-Migration abschliessen:

- ESP-IDF 6.0.2 ueber GitHub Actions bauen, mit **strikt getrennten**
  Bring-up- und Release-Buildpfaden (CI-Paritaet zum bereits lokal
  funktionierenden Pfad aus #72/#73);
- Bring-up- und Releaseprofile ueber eine projekteigene Kconfig-Profilwahl
  und versionierte `sdkconfig.defaults`-Overlays reproduzierbar erzeugen;
- eine ESP-IDF-Ressourcenbaseline (Firmwaregroesse, statisches RAM, Mapfile)
  auf Basis der offiziellen maschinenlesbaren `idf.py size`-Ausgabe je Profil
  als CI-Artefakt sichern;
- Format- und Static-Analysis-Abdeckung auf `main/` und
  `lib/device_platform_esp_idf/` fuer **beide** Profile erweitern;
- genau einen kanonischen Hosttestpfad festlegen und dokumentieren;
- den Arduino-Zweig aus `src/main.cpp` entfernen und `src/main.cpp` als
  reine native Composition Root erhalten (kein Loeschen der Datei);
- den alten Arduino-ESP32-Produktionspfad (PlatformIO-Envs) erst nach
  bestandener CI-Paritaet und **zwei bestandenen Hardware-Smoke-Tests**
  (Bring-up und Release) entfernen;
- einen versionierten ESP-IDF-Upgradevertrag (Bugfix/Minor/Major)
  dokumentieren;
- den Komponenten-/Lockfilevertrag fuer den aktuellen (leeren)
  Abhaengigkeitsstand festschreiben;
- ADR-001 auf `superseded` setzen und `AGENTS.md` an den ESP-IDF-only-Stand
  anpassen (Ownerentscheid, Teil dieses Plans, siehe Abschnitt 7.13).

## 2. Nicht-Ziele

- keine reale Sensor-, Display-, NVS-, Web-, WLAN- oder Aktorintegration;
- keine Aenderung fachlicher Modelle, Wireformate oder #57-Semantik;
- keine Pin-, GPIO- oder Boardrevisionsentscheidung;
- keine neue externe ESP-IDF-Komponente und kein `idf_component.yml` ohne
  echten Bedarf;
- kein Wechsel des kanonischen Hosttestpfads weg von PlatformIO `native`;
- keine harten Byte-Budget-Schwellenwerte ohne Messbasis
  (`TBD_IMPLEMENTATION_BUDGET` bleibt bestehen);
- keine neue ADR ausserhalb der Statuspflege von ADR-001 (Abschnitt 7.13);
- kein ESP-IDF-Toolchain-Cache in #74 (Abschnitt 7.1);
- keine Auswahl einer konkreten Ersatzbibliothek fuer spaetere Arduino-only-
  Funktionalitaet (nur Registerpflege, Abschnitt 7.13.3).

## 3. Verbindliche Quellen und Entscheidungen

- `AGENTS.md` (Plan-first-Workflow, Architekturregeln, Release-1-Abgrenzung);
- Issue #71 (Ownerentscheid ESP-IDF-Migration, verbindliche Reihenfolge
  `#72 -> #73 -> #74`, verbindliche Architektur);
- Issue #74 (Scope, Upgradegrundsaetze, Grenzen, Akzeptanzkriterien);
- **Reviewauftrag "PR #79 – Plan für Issue #74 vollständig korrigieren"**
  (`PLAN_REVIEW: CHANGES_REQUIRED` auf Plan-Commit
  `05b987e3d2b375b82922990f718d0dc07c730a71`): verbindliche Grundlage dieser
  überarbeiteten Fassung, insbesondere fuer Profilmechanismus,
  Profilisolation, Commit-Reihenfolge, Hardware-Gate, Ressourcen-/
  Artefaktvertrag, CI-Reproduzierbarkeit, ESP-IDF-Installationsvariante und
  Dokumentationsumfang;
- `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md` und `docs/DECISIONS.md`
  (ADR-001 PlatformIO/Arduino, ADR-008 4-MB-Budget, ADR-012 Bring-up-Profil,
  ADR-013 Plattformtrennung);
- `docs/CI_AND_QUALITY_GATES.md` (bestehende lokale ESP-IDF-Entwicklerpflicht
  aus #72/#73, ausdruecklich "noch nicht CI-gebunden ... vollstaendig Issue
  #74 vorbehalten");
- `docs/OPEN_POINTS.md` (`TBD_IMPLEMENTATION_BUDGET`);
- PR #77 (Merge-Commit `bf3b1a8b008ce6169494fb2e444cedeadb456d39`, Issue #72)
  und PR #78 (Merge-Commit `c3f8044be0f081822ca8724f67fa99e9614d57ef`, Issue
  #73, Hardware-Smoke-Test `PASS` auf Head
  `ebc54b65d100590a0f2604e08b155110bc25c2f4`);
- offizielle ESP-IDF-6.0.2-Dokumentation, in dieser Ueberarbeitung
  read-only verifiziert: Build-System-Referenz
  (`SDKCONFIG`/`SDKCONFIG_DEFAULTS`-Cache-Variablen, Semikolon-getrennte
  Mehrfachdateien) und `idf.py`-Referenz (`-B`, `-D<NAME>=<WERT>`,
  `idf.py size --format json2 --output-file`).

## 4. Aktuelle Ausgangslage (Bestandsaufnahme)

### 4.1 Buildpfade

- Root-`CMakeLists.txt`: registriert `device_platform`, `fermentation_app`,
  `device_platform_esp_idf` ueber `EXTRA_COMPONENT_DIRS`; `lib/` wird nicht
  pauschal aufgenommen, `device_platform_test_support` bleibt ausserhalb.
  Fail-fast-Guard auf `IDF_VER == v6.0.2`.
- `main/CMakeLists.txt`: registriert `main/app_main.cpp`,
  `PRIV_REQUIRES device_platform fermentation_app device_platform_esp_idf`,
  erzwingt `-std=gnu++17`. **Befund:** setzt `APP_PROFILE_ESP32_BRINGUP=1`
  und `APP_REAL_ACTUATORS_ENABLED=0` **unbedingt**, unabhaengig von einem
  Profil-Parameter. Es existiert aktuell **kein** ESP-IDF-Gegenstueck zu
  `esp32_release`; dieses Gap schliesst #74 ueber den in Abschnitt 7.2
  festgelegten Kconfig-Mechanismus.
- Komponenten-`CMakeLists.txt` (`device_platform`, `fermentation_app`,
  `device_platform_esp_idf`): je `idf_component_register` mit
  `SRC_DIRS "src"`, korrekten `REQUIRES`/`PRIV_REQUIRES`, `-std=gnu++17`.
- `sdkconfig.defaults` existiert bereits (minimal), `sdkconfig`/`sdkconfig.old`/
  `build/`/`managed_components/` sind korrekt `.gitignore`t.
- `platformio.ini`: `[env:native]` (Hostbuild, Unity, `-Wall -Wextra
  -Wpedantic -Werror`), `[env:esp32_bringup]`/`[env:esp32_release]`
  (`espressif32@7.0.1`, `board=esp32dev`, `framework=arduino`,
  `-Wall -Wextra -Werror`, kein `-Wpedantic`).
- `src/main.cpp`: gemeinsamer `startApplication()`-Kern, danach
  `#if defined(ARDUINO)`-Zweig (Arduino-`setup()`/`loop()`, identisches
  Bootlogging/Heartbeat wie `main/app_main.cpp`) und `#else`-Zweig
  (`int main()` fuer den nativen Hostbuild). Der native Zweig dieser Datei
  wird durch den PlatformIO-`native`-Build mitkompiliert und bleibt nach
  #74 bestehen (Abschnitt 7.5).
- `main/app_main.cpp` (ESP-IDF, seit #73 hardwareverifiziert): echter
  `app_main()`-Composition-Root, kooperative Einzelschleife,
  `EspTimerTimeSource`, identisches Bootlogging/Heartbeat/Ressourcen-Logging.
- `include/app_config.hpp` (gelesen, verifiziert): erzwingt per
  `#if (defined(APP_PROFILE_NATIVE) + defined(APP_PROFILE_ESP32_BRINGUP) +
  defined(APP_PROFILE_ESP32_RELEASE)) != 1` einen `#error`, falls nicht
  **genau eine** der drei Profildefinitionen gesetzt ist; ebenso je ein
  `#error` fuer fehlendes `APP_TARGET_FLASH_MB`, `APP_REQUIRE_PSRAM`,
  `APP_WEB_OTA_ENABLED`, `APP_REAL_ACTUATORS_ENABLED`.
  `kEsp32ReleaseProfilePolicy` setzt bereits heute (unveraendert)
  `ActuatorPolicy::RequireVerifiedHardware` und `realActuatorsEnabled=false`;
  `hasSafeDefaults()` erzwingt fuer beide ESP32-Profile
  `HardwareState::HardwareUnverified` und die passende `ActuatorPolicy`,
  abgesichert durch `static_assert`. Dieser Vertrag bleibt in #74
  inhaltlich unveraendert.

### 4.2 CI und Quality Gates

`.github/workflows/build.yml` (`permissions: contents: read`, ein Job
`firmware` auf `ubuntu-latest`):

1. `actions/checkout@v6`, `actions/setup-python@v6` (3.13) — **Befund:**
   `@vN`-Tags sind bewegliche Referenzen, kein Commit-SHA-Pin (Abschnitt 7.11);
2. `pip install platformio==6.1.19`;
3. `apt-get install clang-format-18 clang-tidy-18` — **Befund:** Major-Pin
   ueber `apt`, keine exakte Patch-Fixierung garantiert (Abschnitt 7.11);
4. `clang-format --dry-run --Werror` ueber
   `find src include lib test -name "*.cpp" -o -name "*.hpp" -o -name "*.h"`
   — **Befund:** `main/` fehlt in dieser Liste vollstaendig;
5. `scripts/build_report.py` baut `native esp32_bringup esp32_release` und
   erzeugt `build-report.md` (PlatformIO-Groessenbericht, informativ,
   `TBD_IMPLEMENTATION_BUDGET`);
6. `scripts/check_platformio_config.py` — **Befund:** hart auf
   `EXPECTED_PLATFORM = espressif32@7.0.1`, `EXPECTED_BOARD = esp32dev`
   verdrahtet; verliert mit Entfernung der Arduino-Envs seinen gesamten
   Pruefgegenstand, wird durch `scripts/check_build_profiles.py` abgeloest
   (Abschnitt 7.5.2);
7. `pio test -e native` (420 native Tests);
8. `pio run -e native -t compiledb` + `clang-tidy` ueber eine feste Dateiliste
   (`include/app_config.hpp`, `lib/device_platform/...`,
   `lib/fermentation_app/...`, `src/main.cpp`) — **Befund:** `main/app_main.cpp`
   und `lib/device_platform_esp_idf/src/esp_timer_time_source.cpp` fehlen;
   die native Compile-DB kennt ohnehin keine ESP-IDF-Includes fuer diese
   Dateien (bestaetigt bekannte PlatformIO-`compiledb`-Grenze: `test/` und
   `device_platform_test_support` sind aus demselben Grund schon heute
   ausgenommen, siehe `docs/CI_AND_QUALITY_GATES.md`);
9. `scripts/check_architecture_boundaries.py` (real + Selftest-Fixtures,
   inzwischen bidirektionale `REQUIRES`/`PRIV_REQUIRES`-Pruefung aus #73);
10. `scripts/check_secrets.py`, `scripts/selftest_quality_gates.py`.

Der ESP-IDF-Pfad ist **nicht** in diesem Workflow eingebunden; laut
`docs/CI_AND_QUALITY_GATES.md` ist er bis #74 eine manuelle, lokale
Entwicklerpflicht vor jedem betroffenen Commit.

### 4.3 Abhaengigkeiten und Reproduzierbarkeit

- kein `idf_component.yml`, kein `dependencies.lock`, keine Git-Submodule im
  Repository;
- `.gitignore` ignoriert `build/`, `sdkconfig`, `sdkconfig.old`,
  `managed_components/`; `dependencies.lock` ist bewusst **nicht** ignoriert
  (fuer den Moment, in dem es entsteht);
- keine externen ESP-IDF-Komponenten werden aktuell eingebunden.

### 4.4 Ressourcenbaseline (real gemessen, dieser Planungsphase)

**Arduino/PlatformIO, letzter gruener `main`-CI-Lauf** (Run
`30629717878`, Head `c3f8044be0f081822ca8724f67fa99e9614d57ef`,
Artefakt `build-report.md`):

| Env | RAM | Flash | firmware.elf | firmware.bin |
|---|---|---|---|---|
| `esp32_bringup` | 21472/327680 B (6.6 %) | 267637/1310720 B (20.4 %) | 6189176 B | 268000 B |
| `esp32_release` | 21472/327680 B (6.6 %) | 267637/1310720 B (20.4 %) | 6189176 B | 268000 B |
| `native` | – | – | – | Host-Testbinaer 18032 B |

**ESP-IDF, lokaler Build auf demselben Baumstand** (Head
`ebc54b65d100590a0f2604e08b155110bc25c2f4`, waehrend der #73-Hardware-Smoke-
Test-Session in diesem Arbeitsverzeichnis gebaut, `build/`-Artefakte noch
vorhanden — dieser Build war zum Zeitpunkt der urspruenglichen Planungsphase
noch ein gemeinsamer, nicht profilisolierter Buildordner, siehe Abschnitt
7.3 fuer die kuenftige Trennung):

| Artefakt | Wert |
|---|---|
| `esp32_fermentationsschrank.elf` (`size`) | text 85521 B, data 37224 B, bss 2393 B, dec 125138 B |
| `esp32_fermentationsschrank.bin` | 122864 B |
| `bootloader.bin` | 26096 B |
| `partition-table.bin` | 3072 B |

**Hardware-Smoke-Test (PR #78, real gemessen auf ESP32-WROOM-32E):**
`free_heap_bytes=304764` unveraendert bei Boot und bei `uptime_ms=30283`;
`stack_hwm_bytes` 3104 → 3088.

Diese Zahlen sind **nicht direkt 1:1 vergleichbar** (unterschiedliche
Partitionslayouts, Arduino-Framework-Grundlast versus bare-metal
`app_main()`; beide Pfade sind funktional aber gleich minimal, da keine
Sensor-/Display-/Aktorintegration existiert). Sie dienen ausschliesslich als
Ausgangsmessung fuer den in Abschnitt 7.7 geplanten CI-Ressourcenbericht;
keine Schwellenwerte werden daraus abgeleitet (`TBD_IMPLEMENTATION_BUDGET`).

## 5. Betroffene Module und voraussichtlich betroffene Dateien

Nur in der spaeteren Umsetzungsphase, nach Planfreigabe, zu aendern:

- `.github/workflows/build.yml`
- `CMakeLists.txt`, `main/CMakeLists.txt`
- neu: `main/Kconfig.projbuild` (Profilwahl, Abschnitt 7.2)
- neu: `sdkconfig.defaults.bringup`, `sdkconfig.defaults.release`
  (endgueltige Namen, Abschnitt 7.2)
- neu: `scripts/build_esp_idf_profiles.py` (kanonischer Buildtreiber,
  Abschnitt 7.4)
- neu: `scripts/check_build_profiles.py` (Nachfolger von
  `check_platformio_config.py`, Abschnitt 7.5.2)
- `platformio.ini` (Entfernung `[env:esp32_bringup]`/`[env:esp32_release]`)
- `src/main.cpp` (**nur** Entfernung des `#if defined(ARDUINO)`-Zweigs;
  `startApplication()` und der native `#else`-Zweig bleiben unveraendert
  erhalten, siehe Abschnitt 7.5.1)
- `scripts/build_report.py` (Erweiterung um ESP-IDF-Groessendaten aus
  `idf.py size --format json2`)
- `scripts/check_platformio_config.py` (Entfernung, abgeloest durch
  `scripts/check_build_profiles.py`)
- `scripts/selftest_quality_gates.py` (neue Fixture-Faelle fuer
  Profilisolation und Driftpruefung, Abschnitt 7.5.2)
- `scripts/check_architecture_boundaries.py` (nur falls die Bestandsaufnahme
  in der Umsetzungsphase eine neue Grenze zeigt; aktuell keine Aenderung
  identifiziert)
- neu: `docs/ESP_IDF_UPGRADE_CONTRACT.md` (Upgradevertrag, Abschnitt 7.10)
- neu: `docs/THIRD_PARTY_COMPONENTS.md` (Registerpflege, Abschnitt 7.13.3)
- `docs/CI_AND_QUALITY_GATES.md`, `docs/ARCHITECTURE.md`, `README.md`,
  `CHANGELOG.md`
- `docs/DECISIONS.md` (ADR-001-Status auf `superseded`, Abschnitt 7.13.1)
- `AGENTS.md` (ESP-IDF-only-Stand, Abschnitt 7.13.2)
- `.clang-tidy` (`HeaderFilterRegex` auf `^(include|lib|main)/.*`
  erweitert, Abschnitt 7.8)

Kein produktiver Fachcode (`lib/device_platform/`, `lib/fermentation_app/`,
`main/app_main.cpp`, `startApplication()`/nativer Kern in `src/main.cpp`)
wird inhaltlich veraendert.

## 6. Abhaengigkeiten und Gates

- **Abhaengigkeit:** Issue #73 ist Voraussetzung und bereits erfuellt
  (geschlossen, PR #78, Merge-Commit
  `c3f8044be0f081822ca8724f67fa99e9614d57ef`, Hardware-Smoke-Test `PASS`).
  Keine offene technische Abhaengigkeit blockiert den Start der Umsetzung.
- **Gate 1 — Planfreigabe:** Umsetzung beginnt ausschliesslich nach
  commitgebundenem `PLAN APPROVED`-Ownerkommentar auf den finalen
  Plan-Commit dieser Datei (AGENTS.md, Plan-first-Workflow).
- **Gate 2 — `PRE_ARDUINO_REMOVAL_CI: PASS`:** Commit 5 (Arduino-Entfernung,
  Abschnitt 8) darf erst erfolgen, nachdem exakt auf dem Head von Commit 4
  ein vollstaendiger GitHub-Actions-Lauf bestanden hat mit: beide
  ESP-IDF-Profile gruen, native Tests gruen, Ressourcen-, Architektur-,
  Format-, Static-Analysis-, Secret- und Quality-Gate-Selftests gruen. Kein
  gleichzeitiger Austausch von CI-Einfuehrung und Arduino-Entfernung in
  einem Commit.
- **Gate 3 — Hardware-Smoke-Test-Gate:** Nach Commit 5 beziehungsweise auf
  dem finalen Umsetzungs-Head sind **zwei** Hardware-Smoke-Tests Pflicht
  (Bring-up **und** Release, Abschnitt 7.12) — anders als in der urspruenglichen
  Planfassung ist dies **kein optionales** Gate mehr.
- **Gate 4 — Dokumentierte Ownerentscheidungen bereits getroffen:**
  ADR-001-Statuspflege und `AGENTS.md`-Aktualisierung sind laut
  Reviewauftrag verbindlicher Bestandteil von #74 (Abschnitt 7.13); sie
  blockieren die Umsetzung nicht zusaetzlich.

## 7. Geplante Loesung je Vorgabenbereich

### 7.1 Bereitstellung der ESP-IDF-Toolchain in CI

**Reviewentscheid (verbindlich, ersetzt die urspruengliche Empfehlung
"manuelle native Installation"):** primaere Loesung ist die offizielle
Espressif-Action

```text
espressif/install-esp-idf-action
```

Bedingungen:

- Pin auf einen vollstaendigen Commit-SHA (kein `@v1`-Alias), mit lesbarem
  Versionskommentar daneben;
- `version: "v6.0.2"`;
- nach der Installation zusaetzliche Pruefung des tatsaechlich
  resultierenden ESP-IDF-Commits (nicht nur des Tags), Sollwert
  `7101770dc6db2667b3c477cc31365dd1acd6db4e` (siehe Abschnitt 7.11);
- Lizenz (Apache-2.0) und der exakte, in der Umsetzung tatsaechlich
  verwendete Action-Commit werden im PR dokumentiert;
- minimale Workflow-Permissions bleiben unveraendert (`contents: read`);
- keine zusaetzliche inoffizielle Wrapper-Action.

Fallback-Regel: Stellt sich in der Umsetzung heraus, dass die offizielle
Action ESP-IDF 6.0.2 nicht reproduzierbar bereitstellt, haelt der Agent an.
Ein Wechsel auf manuelle Installation ist dann eine materielle
Planabweichung mit neuem Plan-Commit und erneuter Freigabe — keine
eigenmaechtige Ersatzloesung.

**Caching:** Fuer #74 wird **kein** Toolchain-Cache eingefuehrt (KISS: zuerst
ein korrekter, reproduzierbarer Clean-Build; keine versteckte Cache-Drift;
Laufzeitmessung erst sammeln). Caching bleibt ein spaeterer, eigens
begruendeter Optimierungsschritt.

### 7.2 Profil- und sdkconfig-Vertrag

**Verifizierter Bestandsvertrag** (`include/app_config.hpp`, Abschnitt 4.1):
genau eine der drei `APP_PROFILE_*`-Definitionen muss gesetzt sein, sonst
`#error`. Dieser Vertrag bleibt inhaltlich unveraendert.

**Verbindlich festgelegter Mechanismus** (ersetzt die vormals offene
Auswahl zwischen Kconfig und freiem CMake-Parameter):

1. **Projekt-Kconfig** `main/Kconfig.projbuild` mit einer sich gegenseitig
   ausschliessenden `choice`:

   ```text
   choice APP_ESP32_PROFILE
       prompt "ESP32 application profile"
       default APP_PROFILE_ESP32_BRINGUP

   config APP_PROFILE_ESP32_BRINGUP
       bool "ESP32 bring-up profile"

   config APP_PROFILE_ESP32_RELEASE
       bool "ESP32 release profile"

   endchoice
   ```

   Vertrag: exakt eine der beiden `CONFIG_APP_PROFILE_ESP32_*`-Optionen ist
   aktiv; ohne explizites Release-Overlay bleibt Bring-up der sichere
   Default; keine Option aktiviert reale Aktoren. Die exakte Kconfig-Syntax
   (Schluesselwoerter `choice`/`config`/`bool`/`default`/`endchoice`) folgt
   dem in ESP-IDF-eigenen Komponenten durchgaengig verwendeten Kconfig-Stil;
   eine Konfigurationsprobe (`idf.py menuconfig`/`idf.py build`) in der
   Umsetzungsphase bestaetigt die Syntax vor dem ersten Commit dieses
   Bereichs.

2. **Profil-Overlays** `sdkconfig.defaults.bringup` und
   `sdkconfig.defaults.release` setzen ausschliesslich die jeweilige
   Kconfig-Profiloption. Gemeinsame ESP-IDF-Konfiguration (4-MB-Flash, kein
   PSRAM) bleibt in der gemeinsamen `sdkconfig.defaults`, nicht dupliziert.

3. **Abbildung auf den bestehenden App-Vertrag:** `main/CMakeLists.txt`
   liest `CONFIG_APP_PROFILE_ESP32_BRINGUP`/`CONFIG_APP_PROFILE_ESP32_RELEASE`
   (generiert von ESP-IDFs Kconfig-System, verfuegbar ueber die vom Build
   erzeugte `sdkconfig.cmake`/`CONFIG_*`-Cache-Variablen) und setzt daraus
   **genau eine** der bestehenden Definitionen
   `APP_PROFILE_ESP32_BRINGUP=1` oder `APP_PROFILE_ESP32_RELEASE=1`. Bei
   keiner oder mehreren aktiven Profildefinitionen bricht CMake bereits vor
   der C++-Kompilierung mit `message(FATAL_ERROR ...)` ab — zusaetzlich zum
   bestehenden `#error`-Vertrag in `include/app_config.hpp`, nicht als
   Ersatz dafuer.

4. **Nicht als Kconfig-Werte behandelt** bleiben die unveraenderlichen
   Release-1-Grenzen `APP_TARGET_FLASH_MB=4`, `APP_REQUIRE_PSRAM=0`,
   `APP_WEB_OTA_ENABLED=0`, `APP_REAL_ACTUATORS_ENABLED=0` — feste
   Compile-Definitionen in `main/CMakeLists.txt`, keine
   benutzeraenderbaren Menuoptionen. Der Plan behauptet nicht, dass diese
   vier App-Makros automatisch Bestandteil des generierten `sdkconfig`
   sind.

5. **Zweiteilige Driftpruefung** (Teil von
   `scripts/check_build_profiles.py`, Abschnitt 7.5.2), getrennt fuer
   Bring-up und Release:
   - Pruefung des generierten `sdkconfig` je Profil: korrektes
     ESP32-Profil, 4-MB-IDF-Flashkonfiguration, keine widerspruechliche
     Profilwahl, relevante gemeinsame ESP-IDF-Konfiguration;
   - Pruefung der effektiven Compile-Definitionen je Profil (ueber die
     profilspezifische `compile_commands.json`, Abschnitt 7.3): genau eine
     `APP_PROFILE_ESP32_*`-Definition, `APP_TARGET_FLASH_MB=4`,
     `APP_REQUIRE_PSRAM=0`, `APP_WEB_OTA_ENABLED=0`,
     `APP_REAL_ACTUATORS_ENABLED=0`, keine Arduino-Definition.

`esp32_bringup` erzwingt weiterhin `HARDWARE_UNVERIFIED`/
`LOCKED_FOR_BRINGUP`, `esp32_release` weiterhin `HARDWARE_UNVERIFIED`/
`REQUIRE_VERIFIED_HARDWARE`; in beiden Faellen bleibt
`APP_REAL_ACTUATORS_ENABLED=0` (Release 1 gibt laut `AGENTS.md` nie
automatisch reale Aktoren frei; durch `static_assert` in
`include/app_config.hpp` bereits abgesichert und in #74 unveraendert).

### 7.3 Strikte Profilisolation

**Befund/Risiko (Review):** Ein vorhandenes generiertes `sdkconfig` hat
laut ESP-IDF-Build-System Vorrang vor `sdkconfig.defaults`. Ohne
ausdrueckliche Trennung wuerden zwei nacheinander gebaute Profile denselben
Buildordner und dieselbe generierte `sdkconfig`-Datei teilen und sich
gegenseitig ueberschreiben.

**Verbindlich festgelegt, mit offizieller ESP-IDF-6.0.2-Dokumentation
verifiziert** (Build-System-Referenz: `SDKCONFIG`-Cache-Variable
"output path of generated sdkconfig file"; `idf.py`-Referenz: `-B <dir>`
fuer den Builddirectory-Override, `-D<NAME>=<WERT>` fuer beliebige
CMake-Cache-Variablen, offizielles Dokumentationsbeispiel
`idf.py -DSDKCONFIG=./build/production/sdkconfig reconfigure`):

```text
build/esp32_bringup/
build/esp32_bringup/sdkconfig
build/esp32_release/
build/esp32_release/sdkconfig
```

Kanonische Befehle (exakte Syntax verifiziert, Feinschliff in der
Umsetzung):

```bash
idf.py \
  -B build/esp32_bringup \
  -DSDKCONFIG=build/esp32_bringup/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.bringup" \
  build
```

```bash
idf.py \
  -B build/esp32_release \
  -DSDKCONFIG=build/esp32_release/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.release" \
  build
```

Weitere verbindliche Folgen:

- kein gemeinsam verwendetes Root-`sdkconfig`;
- kein Umschalten eines bestehenden Buildordners zwischen Profilen;
- `clang-tidy` verwendet den Compile-DB-Pfad des jeweiligen Profils
  (Abschnitt 7.8);
- Ressourcenberichte und Artefakte sind profilspezifisch (Abschnitt 7.7);
- Cleanup eines Profils (`idf.py -B build/esp32_bringup fullclean`) darf
  das andere Profil nicht beeinflussen.

### 7.4 Kanonischer lokaler und CI-Buildtreiber

Neu: `scripts/build_esp_idf_profiles.py`. Aufgaben:

- aktive ESP-IDF-Version und exakten Commit pruefen (Abschnitt 7.11);
- Bring-up und Release in den getrennten Buildpfaden aus Abschnitt 7.3
  bauen, mit exakt denselben Befehlen lokal und in CI;
- Profilauswahl ueber Argument: `bringup`, `release` oder `all`;
- **keine** Installation oder Aenderung der ESP-IDF-Toolchain (das bleibt
  Aufgabe von Abschnitt 7.1);
- **kein** Flashen;
- bei einem Fehler mit Profilnamen und fehlgeschlagener Phase abbrechen
  (nicht stillschweigend fortfahren).

CI ruft dieses Skript auf; die lokale Entwicklerdokumentation
(`docs/CI_AND_QUALITY_GATES.md`) beschreibt denselben Aufruf. Keine zweite,
abweichende Folge handgeschriebener `idf.py`-Befehle im Workflow (DRY).

### 7.5 Arduino-Ablösung

#### 7.5.1 `src/main.cpp` bleibt erhalten (Korrektur gegenueber der
urspruenglichen Planfassung)

Die urspruengliche Fassung sah die vollstaendige Entfernung von
`src/main.cpp` und eine neue, noch unbestimmte native Eintrittsdatei vor.
Das war unnoetig: `src/main.cpp` trennt Arduino und native Ausfuehrung
bereits sauber ueber `#if defined(ARDUINO)`.

**`src/main.cpp` bleibt als native-only Composition Root erhalten.** In
Commit 5 (Abschnitt 8) wird ausschliesslich entfernt:

- `#include <Arduino.h>`;
- Arduino-spezifische globale Zustaende (`applicationStarted`,
  `lastHeartbeatMs` im Arduino-Zweig);
- `printBootSummary()` fuer Arduino;
- `setup()`, `loop()`;
- die `#if defined(ARDUINO)` / `#else` / `#endif`-Verzweigung selbst.

Erhalten bleiben unveraendert: `startApplication()`, der native
`int main() { ... }`-Zweig, das native Laufzeitverhalten, das
`.pio/build/native/program`-Artefakt und der bestehende
PlatformIO-`native`-Test-/Buildpfad. Damit entfaellt die in der
urspruenglichen Fassung offene Entscheidung zur neuen nativen
Eintrittsdatei vollstaendig; `scripts/build_report.py`s bestehende
`.pio/build/native/program`-Auswertung bleibt unveraendert gueltig.

Dokumentationsvertrag nach #74:

```text
src/main.cpp       native-only Composition Root
main/app_main.cpp  ESP-IDF Composition Root
```

Keine gemeinsame Universal-Composition-Root-Abstraktion.

#### 7.5.2 Nachfolger fuer `scripts/check_platformio_config.py`

`scripts/check_platformio_config.py` entfaellt mit den Arduino-Envs
vollstaendig. Sein Schutzvertrag verschwindet nicht ersatzlos, sondern geht
in ein neues, breiteres Skript ueber: `scripts/check_build_profiles.py`.
Es prueft mindestens:

- **PlatformIO:** genau ein produktiver Pfad `native`; keine
  ESP32-Arduino-Envs; kein `framework = arduino`; keine
  `espressif32`-Plattform; `APP_PROFILE_NATIVE=1`; bestehende native
  Warnflags und C++17-Vertrag;
- **ESP-IDF Bring-up:** getrennte Build-/Konfigurationspfade (Abschnitt
  7.3); korrektes Kconfig-Profil; korrekte App-Compile-Definitionen;
  ESP-IDF v6.0.2 und exakter Commit; reale Aktoren deaktiviert;
- **ESP-IDF Release:** getrennte Build-/Konfigurationspfade; korrektes
  Kconfig-Profil; `REQUIRE_VERIFIED_HARDWARE`-Profil; reale Aktoren
  deaktiviert; kein Arduino.

`scripts/selftest_quality_gates.py` erhaelt neue Fixture-Faelle: beide
Profile gleichzeitig aktiv; kein Profil aktiv; vertauschte Overlay-Datei;
gemeinsames `sdkconfig`; gemeinsamer Buildordner; reale Aktoren aktiviert;
falsche Flashgroesse; Arduino-Env erneut hinzugefuegt; falscher IDF-Tag
oder -Commit.

`.github/workflows/build.yml`: Schritt „PlatformIO installieren“ bleibt nur
fuer `native` bestehen; `esp32_bringup`/`esp32_release`-Buildaufrufe werden
durch `scripts/build_esp_idf_profiles.py` ersetzt.

#### 7.5.3 Reihenfolge

Der Arduino-Pfad wird **erst entfernt, nachdem** Gate 2
(`PRE_ARDUINO_REMOVAL_CI: PASS`, Abschnitt 6) bestanden ist — kein
Big-Bang-Austausch in einem Commit. Der letzte Arduino-Ressourcenstand wird
vor der Entfernung als Referenzwert in `docs/CI_AND_QUALITY_GATES.md`/
`docs/ESP_IDF_UPGRADE_CONTRACT.md` dokumentiert (Abschnitt 4.4 fuer die
bereits gesammelten Zahlen).

### 7.6 Kanonischer Hosttestpfad

**Empfehlung unveraendert: PlatformIO `[env:native]` bleibt der einzige
kanonische Hosttestpfad**, unveraendert. Begruendung (KISS/DRY, siehe auch
Abschnitt 15):

- 420 bestehende native Tests laufen bereits reproduzierbar, ohne
  ESP-IDF-Abhaengigkeit;
- eine Migration auf reines CMake/CTest waere ein grosser Umbau ohne
  belegten fachlichen Nutzen und mit realem Regressionsrisiko;
- PlatformIO bleibt ohnehin fuer den `native`-Pfad im Repository, keine
  zusaetzliche Toolchain-Doppelung;
- es wird **keine** zweite dauerhafte Hosttestloesung eingefuehrt;
- `src/main.cpp` bleibt (Abschnitt 7.5.1) unveraendert Teil dieses Pfads.

### 7.7 Ressourcenbaseline und Artefakte

#### 7.7.1 Maschinenlesbare Groessendaten (Korrektur: kein Parsen der
Konsolenausgabe)

Fuer jedes Profil nach erfolgreichem Build die offizielle, verifizierte
maschinenlesbare Groessenausgabe verwenden:

```bash
idf.py -B build/esp32_bringup \
  size --format json2 \
  --output-file build/esp32_bringup/size.json
```

analog fuer `build/esp32_release`. Zusaetzlich darf ein menschenlesbarer
Markdown-Bericht erzeugt werden. `scripts/build_report.py` wird um diesen
ESP-IDF-Zweig erweitert (gleiche Datei, gleiche Berichtsstruktur, kein
Parallelskript — DRY); keine eigene Reimplementierung der ESP-IDF-
Groessenberechnung, ausschliesslich Parsing der offiziellen JSON2-Ausgabe.
Fehlt ein Profil oder Artefakt, fuehrt das zu einem harten Fehler, nicht zu
einer stillen Zeile "kein Groessenartefakt gefunden".

Mindestens erfasst je Profil: App-BIN, ELF, Bootloader-BIN,
Partitionstabellen-BIN, gesamter Flashverbrauch laut `size.json`,
statisches DRAM/IRAM laut `size.json`, Mapfile, Profilname, ESP-IDF-Tag,
ESP-IDF-Commit, Build-Commit, Pruefsumme der generierten `sdkconfig`.

#### 7.7.2 Artefaktvertrag

Je Profil ein eindeutig benanntes CI-Artefakt:

```text
esp-idf-esp32_bringup-<git-sha>
esp-idf-esp32_release-<git-sha>
```

Inhalt je Profil: Applikations-BIN, ELF, Mapfile, Bootloader-BIN,
Partitionstabellen-BIN, `flasher_args.json` (bzw. vorhandene
Flashargumentdateien), generierte `sdkconfig`, `compile_commands.json`,
`size.json`, Buildlog, kurzer Manifestbericht (Profil, Git-SHA,
IDF-Version, IDF-Commit).

Aufbewahrung: `retention-days: 30`, analog zur bereits bestehenden
Artefaktaufbewahrung in `build.yml`. Keine lokalen Secrets oder privaten
absoluten Pfade in Artefaktmanifesten (geprueft durch
`scripts/check_secrets.py`, das unveraendert auch neue Dateien abdeckt).

#### 7.7.3 Schwellenwerte

Exakte Byte-Schwellenwerte bleiben `TBD_IMPLEMENTATION_BUDGET`; #74 liefert
die erste dokumentierte, maschinenlesbare Baseline (Abschnitt 4.4), keine
CI-Abbruchgrenze. Spaeteres Entscheidungskriterium: reale
Belastungs-/Heap-Messung auf Zielhardware nach Anbindung von
Sensoren/Display/Aktoren.

### 7.8 Format und Static Analysis

- `clang-format`-Suchpfad in `build.yml` um `main` ergaenzen
  (`find src include lib test main ...`, nach Entfernung des
  Arduino-Zweigs unveraendert, da `src/main.cpp` selbst bestehen bleibt).
- ESP-IDF liefert `build/esp32_bringup/compile_commands.json` und
  `build/esp32_release/compile_commands.json` automatisch (`idf.py build`
  je Profil, Abschnitt 7.3). `clang-tidy` wird fuer
  `main/app_main.cpp` und
  `lib/device_platform_esp_idf/src/esp_timer_time_source.cpp` **fuer beide
  Profile getrennt** aufgerufen:

  ```text
  clang-tidy -p build/esp32_bringup main/app_main.cpp ...
  clang-tidy -p build/esp32_release main/app_main.cpp ...
  ```

  statt eines einzelnen generischen `-p build`, da die beiden Profile
  eigene Compile-Datenbanken besitzen (Abschnitt 7.3). Portable Quellen
  bleiben ueber die native Compile-Datenbank geprueft (unveraendert).
- `.clang-tidy`s `HeaderFilterRegex` wird von `(include|lib)/.*` auf
  `^(include|lib|main)/.*` erweitert, unter Ausschluss generierter
  ESP-IDF- und Drittanbieterheader. Diese Erweiterung ist damit
  **festgelegt**, keine offene Implementierungsentscheidung mehr.
  Diagnosen aus ESP-IDF selbst (ausserhalb des Projekt-Headerfilters)
  werden nicht als Projektfehler behandelt.
- Werkzeugversionen bleiben `clang-format-18`/`clang-tidy-18` (Grenzen der
  Reproduzierbarkeit dieser Fixierung: Abschnitt 7.11).

### 7.9 Komponenten- und Lockfilevertrag

- Kein `idf_component.yml` fuer #74: es gibt aktuell null externe
  Komponenten; eine leere Manifestdatei waere reine Scheinstruktur
  (ausdruecklich verboten laut Agentenauftrag-Vorgabe 5.11).
- `dependencies.lock` bleibt bewusst ungeschrieben, bis die erste echte
  Espressif-Komponente (z. B. Display-/Sensortreiber in einem spaeteren
  Hardware-Issue) ueber den Component Manager eingebunden wird; die
  `.gitignore`-Ausnahme dafuer ist bereits korrekt vorbereitet (nicht
  ignoriert).
- Zukuenftige Prioritaet bei Bedarf: offizielle Espressif-Komponenten vor
  externen Drittkomponenten, jede mit fixierter Version, keine direkten
  Git-Downloads, keine unfixierten Versionsbereiche — als Grundsatz in
  `docs/ESP_IDF_UPGRADE_CONTRACT.md` verankert (Abschnitt 7.10), keine
  Umsetzung jetzt.

### 7.10 ESP-IDF-Upgradevertrag

Neues Dokument `docs/ESP_IDF_UPGRADE_CONTRACT.md`, verlinkt aus
`docs/CI_AND_QUALITY_GATES.md`, mit den in Issue #74 vorgegebenen drei
Stufen:

- **Bugfix-Upgrade:** Vollbuild beider Profile (getrennt, Abschnitt 7.3),
  alle nativen Tests, Ressourcenvergleich gegen letzte Baseline,
  Config-Diff (`sdkconfig` generiert vs. Overlays), **beide**
  Hardware-Smoke-Tests vor Merge (Abschnitt 7.12).
- **Minor-Upgrade:** zusaetzlich offizielle Espressif-Migrationshinweise
  pruefen, Deprecated-/Removed-API-Scan, `sdkconfig`-Aenderungsdiff je
  Profil, Komponenten-/Lockfile-Diff (sobald vorhanden), vollstaendiger
  Hardware-Paritaetstest.
- **Major-Upgrade:** immer eigenes Plan-first-Issue, Parallelbuild bis zur
  Paritaet, kein direktes Ueberschreiben der stabilen Toolchain, Adapter-/
  Portpruefung (`device_platform_esp_idf`), eigene Ressourcen- und
  Hardwarefreigabe.
- Explizit ausgeschlossen: private ESP-IDF-Header, lokaler Fork ohne
  Ownerentscheid, unfixierte Produktionsversion, globale Versionszweige im
  Fachkern, generische Wrapper-Schatten-API.
- Notwendige Versionszweige (aktuell: keine) werden zentral in diesem
  Dokument mit einer expliziten Entfernungsbedingung erfasst, sobald sie
  entstehen.

### 7.11 CI-Reproduzierbarkeit

**Befund:** `ubuntu-latest`, `actions/checkout@v6`, `actions/setup-python@v6`,
`actions/upload-artifact@v4` sind bewegliche Referenzen; `apt-get install
clang-format-18 clang-tidy-18` fixiert nur die Major-Version, keine exakte
Patch-Version.

**Verbindlich festgelegt:**

- Runner nicht `ubuntu-latest`, sondern eine feste Runnerfamilie
  (`ubuntu-24.04`);
- alle `uses:`-Actions (`actions/checkout`, `actions/setup-python`,
  `actions/upload-artifact`, `espressif/install-esp-idf-action`) auf
  vollstaendige Commit-SHAs pinnen, mit lesbarem Versionskommentar daneben;
- `platformio==6.1.19` bleibt exakt gepinnt (unveraendert);
- `clang-format-18`/`clang-tidy-18` bleiben auf Major 18 fixiert; die
  tatsaechlich geladene Patch-Version wird im Build-Log und im
  Artefaktmanifest (Abschnitt 7.7.2) erfasst, ohne eine nicht garantierte
  Patch-Fixierung zu behaupten;
- ESP-IDF-Herkunft wird zwingend geprueft: Tag `v6.0.2`, Commit
  `7101770dc6db2667b3c477cc31365dd1acd6db4e`, sauberer Arbeitsbaum. Der
  bestehende CMake-Guard auf `IDF_VER == v6.0.2` bleibt zusaetzliche
  Verteidigung, ist aber nicht der einzige Herkunftsnachweis.

### 7.12 Hardware-Smoke-Test-Gate (Bring-up und Release, Pflicht)

**Korrektur gegenueber der urspruenglichen Fassung:** #74 aendert die
Quelle der Profilwahl, fuehrt den Release-Profilbuild neu ein, aendert die
ESP-IDF-CI-Strecke, macht ESP-IDF zum einzigen ESP32-Produktionspfad und
entfernt den bisherigen Arduino-Produktionspfad. Ein reiner Build beweist
nicht, dass auf realer Hardware das beabsichtigte Profil und die richtige
Aktorpolicy tatsaechlich starten. Ein Hardware-Gate ist daher **Pflicht**,
nicht optional.

Nach Commit 5 beziehungsweise auf dem finalen Implementierungs-Head, je
mindestens 35 Sekunden:

**Bring-up-Smoke-Test:**

```text
profile: esp32_bringup
hardware state: HARDWARE_UNVERIFIED
actuator policy: LOCKED_FOR_BRINGUP
real actuators: disabled
application: ready
```

**Release-Smoke-Test:**

```text
profile: esp32_release
hardware state: HARDWARE_UNVERIFIED
actuator policy: REQUIRE_VERIFIED_HARDWARE
real actuators: disabled
application: ready
```

Fuer beide zusaetzlich: erster Heartbeat ~1000 ms, folgende Differenzen
~1000 ms, Uptime monoton, genau zwei Ressourcenmessungen, kein Reset,
Watchdog, Panic oder Brownout, keine unerwartete Hardwareaktivität.

Testaufbau (beide Laeufe): dasselbe unbelastete ESP32-Board, UART/USB,
keine externe 12-V-Versorgung, kein BTS7960, kein Display, keine Sensoren,
keine Luefter, kein Peltier, MOSFET-Ausgaenge ohne Last — identisch zum
bereits etablierten und bewaehrten Vorgehen aus #73/PR #78.

Merge-Gates fuer #74:

```text
CODE_REVIEW: PASS
CI: PASS
HARDWARE_SMOKE_TEST_BRINGUP: PASS
HARDWARE_SMOKE_TEST_RELEASE: PASS
```

### 7.13 Dokumentationsumfang

#### 7.13.1 ADR-001

**Ownerentscheid (Teil von #74, nicht mehr offen):** ADR-001 ("PlatformIO
mit Arduino Framework") wird in #74 auf `superseded` gesetzt, mit Verweis
auf Issue #71 und PR #79. Begruendung: Nach Entfernung des
Arduino-Produktionspfads darf die zentrale Entscheidung nicht als aktive
Projektentscheidung stehen bleiben. Kein neues ADR-Dokument noetig; die
Architekturentscheidung selbst ist bereits durch #71/ADR-013 getroffen.

#### 7.13.2 `AGENTS.md`

**Ownerentscheid (Teil von #74, nicht mehr offen):** `AGENTS.md` wird in
#74 aktualisiert:

- `src/main.cpp` als native-only Composition Root benennen;
- `main/app_main.cpp` als ESP-IDF Composition Root benennen;
- ESP-IDF 6.0.2 als einzigen ESP32-Produktionspfad benennen;
- `docs/ESP_IDF_UPGRADE_CONTRACT.md` zu den zentralen Einstiegen
  hinzufuegen;
- **keine** Aenderung fachlicher Safetyregeln.

#### 7.13.3 Third-Party-Komponentenregister

Neues Dokument `docs/THIRD_PARTY_COMPONENTS.md`:

- Plattformzeile von Arduino/PlatformIO-ESP32 auf ESP-IDF 6.0.2
  aktualisieren;
- exakten ESP-IDF-Tag und -Commit dokumentieren;
- Arduino-spezifische Kandidaten (z. B. Preferences, Arduino-WebServer)
  nicht laenger als aktive Plattformbasis darstellen;
- **keine** Ersatzbibliothek in #74 vorzeitig auswaehlen; spaetere
  Neu-Evaluation unter ESP-IDF klar als offen markieren;
- historische Auditdateien bleiben inhaltlich unveraendert; falls dort ein
  Statusbanner fehlt und Verwechslungsgefahr besteht, nur eine rein
  dokumentarische Kennzeichnung ergaenzen, keine rueckwirkende
  Kandidatenbewertung.

#### 7.13.4 `include/app_config.hpp`-Fehlermeldungen

Rein mechanische Korrektur veralteter Fehlermeldungen (kein Vertrags- oder
Verhaltenswechsel): von z. B. `"must be defined by the PlatformIO profile"`
zu `"must be defined by the active build profile"`.

## 8. Geplanter kleiner PR-/Commit-Schnitt

Alle Commits im selben Draft-PR nach Planfreigabe, in dieser korrigierten
Reihenfolge:

1. **Profil- und lokaler Buildvertrag:** `main/Kconfig.projbuild`,
   Bring-up-/Release-Overlays, getrennte Build-/`sdkconfig`-Pfade
   (Abschnitt 7.3), `main/CMakeLists.txt`-Mapping (Abschnitt 7.2),
   `scripts/build_esp_idf_profiles.py`, Profil-/Driftpruefung samt
   Selftests. Beide Profile lokal gruen. Arduino-Produktionsprofile
   bleiben unveraendert bestehen.
2. **ESP-IDF-CI additiv einfuehren:** gepinnte ESP-IDF-Installation
   (Abschnitt 7.1), beide Profile ueber den kanonischen Buildtreiber in
   `build.yml`; bestehender Arduino-Produktionspfad bleibt bestehen; CI auf
   exakt diesem Commit vollstaendig gruen.
3. **Ressourcenbericht und Artefakte:** `idf.py size --format json2`-
   Auswertung, Erweiterung von `scripts/build_report.py`,
   profilspezifische CI-Artefakte (Abschnitt 7.7).
4. **Format und Static Analysis:** vollstaendiger Format-Scope, native
   Compile-DB fuer portable Quellen, beide ESP-IDF-Compile-DBs fuer
   IDF-spezifische Quellen, `.clang-tidy`-`HeaderFilterRegex`-Erweiterung,
   Selftests (Abschnitt 7.8).

   **Gate vor Commit 5:** `PRE_ARDUINO_REMOVAL_CI: PASS` — ein vollstaendiger
   erfolgreicher GitHub-Actions-Lauf exakt auf dem Head von Commit 4
   (Abschnitt 6, Gate 2).

5. **Arduino-Produktionspfad entfernen:** nur nach bestandenem Gate;
   `[env:esp32_bringup]`/`[env:esp32_release]` aus `platformio.ini`
   entfernen; Arduino-Zweig aus `src/main.cpp` entfernen (Datei selbst
   bleibt, Abschnitt 7.5.1); `scripts/check_platformio_config.py` durch
   `scripts/check_build_profiles.py` ersetzen (Abschnitt 7.5.2).
6. **Upgradevertrag und Abschlussdokumentation:**
   `docs/ESP_IDF_UPGRADE_CONTRACT.md`, `docs/THIRD_PARTY_COMPONENTS.md`,
   `docs/CI_AND_QUALITY_GATES.md`, `docs/ARCHITECTURE.md`, `README.md`,
   `docs/DECISIONS.md` (ADR-001), `AGENTS.md`, `CHANGELOG.md`.

**Finaler Head:** Auf dem finalen Implementierungs-Head erneut vollstaendige
CI **und** beide Hardware-Smoke-Tests (Bring-up, Release) ausfuehren
(Abschnitt 7.12).

Issue #71 wird **nicht** in diesem PR geschlossen, sondern erst nach
Owner-Merge und verifiziertem Abschluss von #74, analog zum bereits
etablierten Muster bei #72/#73.

## 9. Offene Entscheidungen

Gegenueber der urspruenglichen Planfassung sind die frueheren offenen
Punkte 1 (ADR-001), 2 (`AGENTS.md`-Verweis), 3 (CI-Caching), 4 (native
Eintrittsdatei) und 5 (Driftpruefung) durch dieses Review verbindlich
entschieden (Abschnitte 7.1, 7.2, 7.5.1, 7.5.2, 7.13) und entfallen als
offene Punkte. Es verbleibt:

1. Byte-Budget-Schwellenwerte bleiben `TBD_IMPLEMENTATION_BUDGET`
   (Abschnitt 7.7.3) — kein Blocker, sondern ein dokumentierter,
   bewusst offener Wert bis zu realer Belastungsmessung.
2. Die exakte Kconfig-Syntax fuer `main/Kconfig.projbuild` (Abschnitt 7.2)
   wird unmittelbar zu Beginn von Commit 1 gegen den echten ESP-IDF-6.0.2-
   Build verifiziert; sollte sie von der hier skizzierten Form abweichen,
   ist das eine technische Detailkorrektur ohne Vertrags-/Safetywirkung,
   keine materielle Planabweichung.
3. Sollte `espressif/install-esp-idf-action` (Abschnitt 7.1) ESP-IDF 6.0.2
   in der Umsetzung nachweislich nicht reproduzierbar bereitstellen, ist
   der in Abschnitt 7.1 beschriebene Fallback-Prozess (Anhalten, neuer
   Plan-Commit) zu befolgen.

## 10. Ausdruecklich verbotene Vorwegnahmen

- keine Pin-, GPIO-, Board-Revisions- oder Partitionsentscheidung;
- keine neue externe `idf_component.yml`-Abhaengigkeit;
- kein Wechsel des kanonischen Hosttestpfads;
- keine harten CI-Abbruch-Schwellenwerte fuer Ressourcen;
- keine neue ADR ausserhalb der in Abschnitt 7.13.1 festgelegten
  ADR-001-Statuspflege;
- kein ESP-IDF-Toolchain-Cache in #74 (Abschnitt 7.1);
- keine Auswahl einer konkreten Ersatzbibliothek im
  Third-Party-Komponentenregister (Abschnitt 7.13.3);
- keine reale Aktorfreigabe in irgendeinem Profil;
- kein Wechsel von der offiziellen `espressif/install-esp-idf-action`
  (Abschnitt 7.1) auf eine andere Installationsvariante ohne den dort
  beschriebenen Fallback-Prozess.

## 11. Daten-, Zustands- und Schnittstellenvertraege

Keine Aenderung an `ITimeSource`, `PlatformStartupContext`,
`ProfilePolicy`, Wireformaten oder #57-Vertraegen. `include/app_config.hpp`
selbst (Exklusivitaets-`#error`-Vertrag, `ProfilePolicy`-Structs,
`hasSafeDefaults()`, alle `static_assert`) wird inhaltlich **nicht**
angefasst, ausser der rein mechanischen Fehlermeldungskorrektur aus
Abschnitt 7.13.4 (keine Verhaltens- oder Vertragsaenderung). Die
Profilsteuerung aendert nur, **wie** `main/CMakeLists.txt` genau eine der
drei bestehenden `APP_PROFILE_*`-Definitionen setzt (jetzt: ueber Kconfig,
Abschnitt 7.2), nicht die dadurch erzeugte Semantik.

## 12. Fehler-, Recovery-, Security- und Safetygrenzen

- reale Aktoren bleiben in `esp32_bringup` **und** `esp32_release`
  deaktiviert (bestaetigt: aktuell hart codiert, bleibt es auch nach
  Umstellung auf Kconfig-Overlays, siehe Abschnitt 7.2, zusaetzlich durch
  die zweiteilige Driftpruefung aus Abschnitt 7.2 Punkt 5 abgesichert);
- CI-Workflow-`permissions` bleiben minimal (`contents: read`);
- keine Secrets in Workflow, Cache oder Artefakten
  (`scripts/check_secrets.py` bleibt unveraendert wirksam, deckt auch neue
  Dateien und Artefaktmanifeste ab, Abschnitt 7.7.2);
- keine unfixierten Actions: alle `uses:`-Referenzen werden auf
  Commit-SHA gepinnt (Abschnitt 7.11);
- fehlgeschlagene ESP-IDF-Builds liefern Log-Artefakte, analog zum
  bestehenden `platformio-build-log`-Muster;
- kein Cache taeuscht Buildkorrektheit vor: #74 fuehrt bewusst keinen
  Toolchain-Cache ein (Abschnitt 7.1);
- zwei Hardware-Smoke-Tests (Bring-up, Release) sind vor Merge Pflicht
  (Abschnitt 7.12) — kein rein simulierter Nachweis fuer den
  Produktionswechsel.

## 13. Teststrategie

- bestehende 420 native Tests bleiben unveraendert massgeblich
  (`pio test -e native`);
- `idf.py build` fuer beide Profile (getrennte Buildpfade) wird neuer
  CI-Pflichtschritt (Kompilieroffenheit, kein Laufzeittest ohne Hardware);
- `scripts/check_architecture_boundaries.py --selftest` bleibt
  Pflichtschritt, unveraendert;
- `scripts/selftest_quality_gates.py` erhaelt neue Fixture-Faelle fuer
  `scripts/check_build_profiles.py` (Abschnitt 7.5.2) und den erweiterten
  Format-Scope, sobald diese in der Umsetzung konkret vorliegen;
- Hardware-Smoke-Tests (Bring-up, Release) als Pflichtabschluss nach der
  Arduino-Entfernung (Abschnitt 7.12).

## 14. Dokumentationsaenderungen (Umsetzungsphase)

- `README.md`: Arduino-/`espressif32@7.0.1`-Referenzen durch den
  ESP-IDF-6.0.2-Pfad ersetzen;
- `docs/ARCHITECTURE.md`: Profiltabelle und Buildwege aktualisieren;
- `docs/CI_AND_QUALITY_GATES.md`: ESP-IDF-Abschnitt von "lokale
  Entwicklerpflicht" auf "CI-gebunden" umschreiben; PlatformIO-Abschnitt
  auf `native` reduzieren; Verweis auf
  `scripts/build_esp_idf_profiles.py` und
  `docs/ESP_IDF_UPGRADE_CONTRACT.md` ergaenzen;
- `docs/DECISIONS.md`: ADR-001 auf `superseded` (Abschnitt 7.13.1);
- `AGENTS.md`: siehe Abschnitt 7.13.2;
- neu: `docs/THIRD_PARTY_COMPONENTS.md` (Abschnitt 7.13.3);
- `CHANGELOG.md`: Eintrag nach Abschluss.

## 15. Bewertung gegen SOLID, DRY, KISS

- **SRP:** Workflow-Datei, Kconfig-Profilwahl, kanonischer Buildtreiber
  (`build_esp_idf_profiles.py`), Ressourcenbericht (`build_report.py`,
  aggregiert nur), Profil-/Driftpruefung (`check_build_profiles.py`) und
  Upgradevertrag (`ESP_IDF_UPGRADE_CONTRACT.md`) bleiben getrennte Dateien
  mit je einer Verantwortung. `build_report.py` reimplementiert die
  ESP-IDF-Groessenberechnung nicht selbst, sondern parst ausschliesslich
  die offizielle `size --format json2`-Ausgabe (Abschnitt 7.7.1).
- **OCP:** `device_platform`, `fermentation_app`, `device_platform_esp_idf`
  werden inhaltlich nicht angefasst; die Migration wirkt ausschliesslich
  auf Build-/CI-/Doku-Ebene.
- **LSP:** keine Portschnittstelle aendert sich; `EspTimerTimeSource`
  bleibt einzige `ITimeSource`-Implementierung fuer ESP-IDF, unveraendert.
- **ISP:** kein universeller Toolchain-Wrapper; `build_esp_idf_profiles.py`
  kapselt ausschliesslich Bauen und Validieren, nicht Installation oder
  Flashen.
- **DIP:** Fachkern bleibt unabhaengig von ESP-IDF und CI;
  `check_architecture_boundaries.py` sichert das automatisiert weiterhin
  ab.
- **DRY:** genau eine Quelle je Belang — ein Ressourcenberichtsskript
  (erweitert, nicht dupliziert), ein Hosttestpfad, ein Upgradevertrag-
  Dokument, ein kanonischer Buildtreiber (identisch lokal und in CI), ein
  Profil-/Driftpruefskript.
- **KISS:** bestehender funktionierender `native`-Hosttestpfad bleibt
  unveraendert; `src/main.cpp` bleibt erhalten statt einer neuen,
  unbestimmten Eintrittsdatei; kein Toolchain-Cache in #74; die
  Profilisolation nutzt ausschliesslich offiziell dokumentierte
  `idf.py`-Mechanismen (`-B`, `-DSDKCONFIG`, `-DSDKCONFIG_DEFAULTS`) statt
  einer eigenen Buildordner-Abstraktion; die Umstellung erfolgt in
  kleinen, einzeln gruenen Schritten mit einem harten Gate vor der
  Arduino-Entfernung.

Bewusste Abweichung: Der urspruengliche Plan bewertete den
Profilmechanismus als "kleine technische Detailentscheidung ohne
Safetywirkung" und liess ihn offen. Das Review korrigiert das: Der
Mechanismus bestimmt die aktive `ActuatorPolicy` und ist damit
sicherheitsrelevant, wird daher in diesem Plan verbindlich festgelegt
(Abschnitt 7.2) statt in die Umsetzungsphase verschoben.

## 16. Abnahmekriterien

### CI

- Profil- und Konfigurationsguard (`scripts/check_build_profiles.py`)
  gruen fuer beide Profile;
- native Tests gruen;
- Bring-up-IDF-Build gruen (isolierter Buildpfad);
- Release-IDF-Build gruen (isolierter Buildpfad);
- Format gruen;
- native `clang-tidy` gruen;
- Bring-up-`clang-tidy` gruen;
- Release-`clang-tidy` gruen;
- Architekturguard real + Selftests gruen;
- Secretcheck gruen;
- Quality-Gate-Selftests gruen;
- Ressourcenbericht (JSON2-basiert) vorhanden;
- Artefaktmanifest je Profil vorhanden;
- kein Arduino-Produktionspfad mehr im Repository.

### Hardware

- Bring-up-Smoke-Test auf dem finalen Head: `PASS`;
- Release-Smoke-Test auf dem finalen Head: `PASS`.

### Dokumentation

- `docs/ESP_IDF_UPGRADE_CONTRACT.md`, `docs/CI_AND_QUALITY_GATES.md`,
  `docs/ARCHITECTURE.md`, `README.md`, `docs/DECISIONS.md` (ADR-001),
  `AGENTS.md`, `docs/THIRD_PARTY_COMPONENTS.md`, `CHANGELOG.md` aktuell.

### Abschlussstatus

```text
CODE_REVIEW: PASS
CI: PASS
HARDWARE_SMOKE_TEST_BRINGUP: PASS
HARDWARE_SMOKE_TEST_RELEASE: PASS
ARDUINO_PRODUCTION_PATH_REMOVED: PASS
ESP_IDF_ONLY_PRODUCTION_BUILD: PASS
```

Issue #71 wird erst nach Owner-Merge und verifiziertem Abschluss von #74
geschlossen.

### Plan-spezifisch

- genau ein Plan-Korrekturcommit fuer diese Ueberarbeitung;
- Draft-PR-Beschreibung verweist auf den neuen Plan-Commit und markiert
  `05b987e3d2b375b82922990f718d0dc07c730a71` als überholt;
- Status bleibt `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`;
- vollstaendiges Anhalten bis zu einem commitgebundenen
  `PLAN APPROVED`-Ownerkommentar auf den neuen Plan-Commit.
