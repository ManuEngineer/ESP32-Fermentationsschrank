# Plan: Issue #74 – CI, Ressourcenbaseline und ESP-IDF-Upgradevertrag

Status: Planungsphase (`IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`)

Issue: `#74`
Tracking: `#71`
Basis-`main`-SHA: `c3f8044be0f081822ca8724f67fa99e9614d57ef` (Merge-Commit von PR #78)

Diese Fassung überarbeitet den ursprünglichen Plan-Commit
`05b987e3d2b375b82922990f718d0dc07c730a71` nach einem vollständigen Review
(`PLAN_REVIEW: CHANGES_REQUIRED`) auf PR #79, die erste Überarbeitung
(Plan-Commit `bbccd74d49b7fcb7c2c529054da5dcd2d8e9a754`) nach einer zweiten
Reviewrunde (`PLAN_REVIEW: CHANGES_REQUIRED`) sowie die zweite
Überarbeitung (Plan-Commit `6c8092755dde1fe0b39299abe94a0b3e02003beb`) nach
einer dritten, finalen Reviewrunde (`PLAN_REVIEW: CHANGES_REQUIRED`). Alle
drei vorherigen Plan-Commits bleiben in der Historie und sind überholt;
diese Fassung ersetzt sie inhaltlich.

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
- den alten Arduino-ESP32-Produktionspfad (PlatformIO-Envs) nach bestandener
  CI-Paritaet (`PRE_ARDUINO_REMOVAL_CI: PASS`) entfernen und danach auf dem
  finalen Implementierungs-Head **zwei verpflichtende Hardware-Smoke-Tests**
  (Bring-up und Release) als Pflicht-Merge-Gate bestehen — die Hardwaretests
  sind damit Voraussetzung fuer den Merge, nicht Voraussetzung fuer die
  Arduino-Entfernung selbst (siehe Abschnitt 6, Gates 2 und 3, sowie
  Abschnitt 8);
- einen versionierten ESP-IDF-Upgradevertrag (Bugfix/Minor/Major)
  dokumentieren;
- den Komponenten-/Lockfilevertrag fuer den aktuellen (leeren)
  Abhaengigkeitsstand festschreiben;
- ADR-001 auf `superseded` setzen, `AGENTS.md` an den ESP-IDF-only-Stand
  anpassen, das bestehende `docs/THIRD_PARTY_COMPONENTS.md` in-place
  aktualisieren sowie `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md` und
  `lib/README.md` auf zwei Composition Roots korrigieren (Ownerentscheid,
  Teil dieses Plans, siehe Abschnitt 7.13).

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
  `05b987e3d2b375b82922990f718d0dc07c730a71`): verbindliche Grundlage der
  ersten Überarbeitung, insbesondere fuer Profilmechanismus,
  Profilisolation, Commit-Reihenfolge, Hardware-Gate, Ressourcen-/
  Artefaktvertrag, CI-Reproduzierbarkeit, ESP-IDF-Installationsvariante und
  Dokumentationsumfang;
- **Reviewauftrag "PR #79 – zweite und abschliessende Plan-Korrekturrunde"**
  (`PLAN_REVIEW: CHANGES_REQUIRED` auf Plan-Commit
  `bbccd74d49b7fcb7c2c529054da5dcd2d8e9a754`): verbindliche Grundlage dieser
  Fassung, insbesondere fuer die korrekte Behandlung des bestehenden
  `docs/THIRD_PARTY_COMPONENTS.md`, die widerspruchsfreie Hardware-Gate-
  Reihenfolge, die vollstaendige Pinnung der ESP-IDF-Installations-Action
  samt EIM-Version, die reale Secret-/Pfadpruefung generierter Artefakte,
  die Aktualisierung von ADR-013 und `lib/README.md`, die Abgrenzung
  historischer Auditdateien und die korrekte Formulierung der
  Artefakt-Aufbewahrung;
- **Reviewauftrag "PR #79 – finale gezielte Plan-Korrekturen"**
  (`PLAN_REVIEW: CHANGES_REQUIRED` auf Plan-Commit
  `6c8092755dde1fe0b39299abe94a0b3e02003beb`): verbindliche Grundlage
  dieser Fassung, insbesondere fuer den zweiphasigen Uebergangsvertrag von
  `scripts/check_build_profiles.py`, die Unveraenderlichkeit gepinnter
  Action-SHAs, den dokumentierten Lizenzkonflikt der
  ESP-IDF-Installations-Action, den vollstaendigen Textartefakt-Scan und
  den exakten Zeitpunkt der Hardware-Smoke-Tests;
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
- neu: `scripts/check_build_profiles.py` (Commit 1: zeitlich begrenzter
  Parallelvertrag ESP-IDF + bestehende Arduino-Envs; Commit 5: Umstellung
  auf den finalen ESP-IDF-only-Vertrag im selben Skript, kein neues Skript
  und kein Bypass-Flag, Abschnitt 7.5.2)
- `platformio.ini` (Entfernung `[env:esp32_bringup]`/`[env:esp32_release]`
  in Commit 5; bleibt in Commits 1–4 unveraendert bestehen)
- `src/main.cpp` (**nur** Entfernung des `#if defined(ARDUINO)`-Zweigs;
  `startApplication()` und der native `#else`-Zweig bleiben unveraendert
  erhalten, siehe Abschnitt 7.5.1)
- `scripts/build_report.py` (Erweiterung um ESP-IDF-Groessendaten aus
  `idf.py size --format json2`)
- `scripts/check_secrets.py` (Erweiterung um optionalen Scan-Modus fuer
  alle hochgeladenen, nicht getrackten Textartefakte, Abschnitt 7.7.4)
- `scripts/check_platformio_config.py` (bleibt in Commits 1–4 unveraendert
  parallel aktiv; Entfernung erst in Commit 5, abgeloest durch
  `scripts/check_build_profiles.py`)
- `scripts/selftest_quality_gates.py` (neue Fixture-Faelle fuer
  Profilisolation und Driftpruefung, Abschnitt 7.5.2)
- `scripts/check_architecture_boundaries.py` (nur falls die Bestandsaufnahme
  in der Umsetzungsphase eine neue Grenze zeigt; aktuell keine Aenderung
  identifiziert)
- neu: `docs/ESP_IDF_UPGRADE_CONTRACT.md` (Upgradevertrag, Abschnitt 7.10)
- `docs/THIRD_PARTY_COMPONENTS.md` (**bestehende** Datei, In-place-
  Aktualisierung, kein neues Dokument, Abschnitt 7.13.3)
- `docs/CI_AND_QUALITY_GATES.md`, `docs/ARCHITECTURE.md`, `README.md`,
  `CHANGELOG.md`
- `docs/DECISIONS.md` (ADR-001-Status auf `superseded`, Abschnitt 7.13.1)
- `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md` (Statuszusatz und
  Repository-Mapping aktualisieren, Abschnitt 7.13.5)
- `lib/README.md` (Modulübersicht auf zwei Composition Roots korrigieren,
  Abschnitt 7.13.5)
- `AGENTS.md` (ESP-IDF-only-Stand, Abschnitt 7.13.2)
- `.clang-tidy` (`HeaderFilterRegex` auf `^(include|lib|main)/.*`
  erweitert, Abschnitt 7.8)

Kein produktiver Fachcode (`lib/device_platform/`, `lib/fermentation_app/`,
`main/app_main.cpp`, `startApplication()`/nativer Kern in `src/main.cpp`)
wird inhaltlich veraendert. `docs/audits/` (historische Auditdateien, u. a.
`docs/audits/RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`) wird in #74 **nicht**
angefasst (Abschnitt 7.13.3).

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
  ESP-IDF-Profile gruen, **der alte Arduino-Pfad unveraendert weiterhin
  gruen** (Parallelvertrag aus Abschnitt 7.5.2, Phase 1), native Tests
  gruen, Ressourcen-, Architektur-, Format-, Static-Analysis-, Secret- und
  Quality-Gate-Selftests gruen. Kein gleichzeitiger Austausch von
  CI-Einfuehrung und Arduino-Entfernung in einem Commit.
- **Gate 3 — Hardware-Smoke-Test-Gate:** Nach Commit 5 (Arduino-Entfernung)
  und Commit 6 (Abschlussdokumentation), auf dem finalen Implementierungs-
  Head, sind **zwei** Hardware-Smoke-Tests Pflicht (Bring-up **und**
  Release, Abschnitt 7.12) — als Pflicht-**Merge**-Gate, nicht als
  Voraussetzung fuer Commit 5 selbst. Anders als in der urspruenglichen
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

**Befund aus der zweiten Reviewrunde (live verifiziert):** Das Repository
`espressif/install-esp-idf-action` hat **keine** semver-Release-Tags; als
einzige Referenz existiert der bewegliche Branch `v1` (aktueller HEAD zum
Recherchezeitpunkt dieser Planungsphase:
`8fc05d1470d5591417e7a3707a1f2bec178db4ae`, Commit vom 2025-09-18, Titel
"Fixed user name (#10) * enabled use of fixed EIM version"). Die Action
delegiert die eigentliche Installation an den **ESP-IDF Installation
Manager (EIM)** (`espressif/idf-im-cli`); ihr Input `eim-version` ist
optional und veraltet ohne Angabe stillschweigend auf die jeweils neueste
EIM-Version — der Installationspfad waere damit trotz gepinntem
Action-Commit und gepinntem ESP-IDF-Tag weiterhin zeitabhaengig.
Zusaetzlicher Befund: `idf-im-cli` ist als Repository seit 25.02.2026
archiviert (read-only); die letzte veroeffentlichte EIM-Version ist
`v0.1.7` (12.02.2025) und bleibt damit die einzig sinnvolle, dauerhaft
stabile Pin-Wahl (keine kuenftigen EIM-Versionen sind zu erwarten).

**Verbindlich festgelegte Konfiguration** (exakte Werte, kein Platzhalter):

```yaml
uses: espressif/install-esp-idf-action@8fc05d1470d5591417e7a3707a1f2bec178db4ae  # v1, Stand 2025-09-18
with:
  version: "v6.0.2"
  eim-version: "v0.1.7"  # letzte veroeffentlichte EIM-Version; idf-im-cli seit 25.02.2026 archiviert
```

**Verbindlichkeit des Pins (Korrektur aus dritter Reviewrunde):** Die hier
genannten SHAs (`8fc05d1470d5591417e7a3707a1f2bec178db4ae` fuer die Action,
sowie die in Abschnitt 7.11 gelisteten SHAs fuer `actions/checkout`,
`actions/setup-python`, `actions/upload-artifact`) sind mit der
Planfreigabe verbindlich und werden in der Umsetzung **unveraendert**
verwendet. Ein anderer Commit enthaelt anderen ausfuehrbaren Drittcode;
"funktionale Gleichwertigkeit" laesst sich nicht allein am Branch- oder
Tag-Namen ablesen und wird daher nicht als Grund fuer eine stille
Aktualisierung akzeptiert. Die vorherige Planfassung erlaubte einen
solchen stillen Wechsel als "technische Detailkorrektur" — das ist
ersatzlos gestrichen.

Die Umsetzung **darf**:

- pruefen, dass die gepinnten Commits weiterhin im jeweiligen Repository
  existieren und erreichbar sind;
- Herkunft und Repository-Zugehoerigkeit der Commits kontrollieren;
- bekannte, oeffentlich dokumentierte Security-Hinweise zu den gepinnten
  Commits pruefen und dokumentieren;
- die Kompatibilitaet von Action-Commit, `eim-version: "v0.1.7"` und
  ESP-IDF `v6.0.2` auf dem GitHub-hosted Runner `ubuntu-24.04` (Abschnitt
  7.11) zu Beginn der Umsetzung (Commit 2) verifizieren, **ohne** dabei den
  gepinnten SHA zu aendern.

Die Umsetzung **darf nicht**:

- automatisch auf einen neueren `v1`-Branch- oder Tag-HEAD wechseln;
- einen anderen Action-Commit als technische Detailkorrektur behandeln;
- die hier genannten SHAs ohne neue Ownerfreigabe ersetzen.

Weitere Bedingungen:

- nach der Installation zusaetzliche Pruefung des tatsaechlich
  resultierenden ESP-IDF-Commits (nicht nur des Tags), Sollwert
  `7101770dc6db2667b3c477cc31365dd1acd6db4e` (siehe Abschnitt 7.11);
- Lizenz von `espressif/install-esp-idf-action` **nicht widerspruchsfrei**
  (`LICENSE_METADATA_CONFLICT`, siehe unten) sowie die transitive Nutzung
  von EIM (`espressif/idf-im-cli`, archiviert, letzte Version `v0.1.7`,
  Root-Lizenz Apache-2.0) werden im PR und im Supply-Chain-Teil von
  Abschnitt 7.11 dokumentiert;
- minimale Workflow-Permissions bleiben unveraendert (`contents: read`);
- kein Default `latest` fuer `version` oder `eim-version`, kein gleitender
  Tag;
- keine zusaetzliche inoffizielle Wrapper-Action.

**Lizenzstatus `espressif/install-esp-idf-action` — `LICENSE_METADATA_CONFLICT`
(neuer Befund aus dritter Reviewrunde, live verifiziert am exakt gepinnten
Commit `8fc05d1470d5591417e7a3707a1f2bec178db4ae`):**

- Root-`LICENSE`: Apache License 2.0 (voller Lizenztext, verifiziert);
- `package.json`: `"license": "MIT"` (verifiziert);
- `README.md`: "This project is licensed under the MIT License - see the
  LICENSE file for details." — behauptet MIT, verweist aber auf die
  Apache-2.0-`LICENSE`-Datei (in sich widerspruechlich, verifiziert);
- `dist/licenses.txt`: enthaelt separate MIT- und weitere Notices der
  bebuendelten Node-Laufzeitabhaengigkeiten (`@actions/core`,
  `@actions/exec` und weitere, verifiziert vorhanden).

Der Plan behauptet **keinen** widerspruchsfreien Lizenzstatus. Stattdessen
gilt: zwei permissive, aber einander widersprechende Eigenangaben
(Apache-2.0 vs. MIT) plus gebuendelte MIT-Drittnotices; Verwendung
ausschliesslich als CI-Werkzeug, nicht als Bestandteil der ausgelieferten
Firmware. Root-Lizenz und gebuendelte Notices werden im
Implementierungs-PR nachvollziehbar referenziert; der Widerspruch wird
dokumentiert, nicht rechtlich aufgeloest oder verschwiegen. Angesichts
zweier permissiver Angaben und der reinen CI-Verwendung ist dies kein
automatischer Implementierungsblocker.

Fuer EIM zusaetzlich dokumentiert: `espressif/idf-im-cli` `v0.1.7`,
Root-Lizenz Apache-2.0 (verifiziert), Repository archiviert
(`archived: true`, verifiziert).

**Fallback-Regel, jetzt vollstaendig fuer jeden Grund einer Pin-Aenderung**
(nicht nur technisches Versagen): Stellt sich in der Umsetzung heraus, dass
einer der in Abschnitt 7.1 oder 7.11 gepinnten SHAs nicht mehr verwendet
werden soll — sei es wegen eines Security-Fixes, Inkompatibilitaet,
Entfernung des Commits, einer neuen offiziellen Empfehlung oder einer
technischen Funktionsstoerung (z. B. die Action stellt ESP-IDF 6.0.2 nicht
reproduzierbar auf `ubuntu-24.04` bereit) —, gilt derselbe Ablauf:

1. Implementierung anhalten.
2. Plan-Datei aktualisieren.
3. neuen Plan-Commit erstellen.
4. neuen SHA, Diff zum vorherigen Commit (soweit einsehbar), Herkunft,
   Lizenz und konkrete Begruendung fuer den Wechsel dokumentieren.
5. erneut auf einen commitgebundenen `PLAN APPROVED`-Ownerkommentar warten.

Kein eigenmaechtiger Wechsel auf eine andere Installationsvariante oder
einen anderen Commit ausserhalb dieses Ablaufs.

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

#### 7.5.2 Nachfolger fuer `scripts/check_platformio_config.py` —
zweiphasiger Uebergangsvertrag (Korrektur aus dritter Reviewrunde)

**Befund:** Die vorherige Planfassung beschrieb fuer
`scripts/check_build_profiles.py` bereits den **finalen** Vertrag (nur
`native` unter PlatformIO, keine Arduino-Envs), waehrend Commit 1 laut
Commit-Schnitt (Abschnitt 8) dieselbe Profil-/Driftpruefung samt Selftests
einfuehren soll — zu einem Zeitpunkt, an dem die Arduino-Produktionsprofile
in `platformio.ini` laut Plan noch bestehen bleiben (Commits 1–4). Beide
Anforderungen sind gleichzeitig unerfuellbar. Die Korrektur legt daher
einen **zeitlich begrenzten Parallelvertrag** fest, den dasselbe Skript in
zwei Phasen durchlaeuft.

**Phase 1 — Commit 1 (Parallel-Migrationsvertrag):**

Neu: `scripts/check_build_profiles.py`. Prueft in dieser Phase:

- **ESP-IDF:** Bring-up- und Release-Buildpfade getrennt (Abschnitt 7.3);
  getrennte generierte `sdkconfig`-Dateien; korrektes Kconfig-Profil je
  Build; korrekte Compile-Definitionen; 4-MB-Vertrag; kein PSRAM; kein
  Web-OTA; reale Aktoren deaktiviert; kein Arduino im ESP-IDF-Build;
  ESP-IDF v6.0.2 und exakter Commit;
- **PlatformIO waehrend der Parallelphase:** `native` ist vorhanden und
  unveraendert korrekt; die beiden Arduino-Environments `esp32_bringup`
  und `esp32_release` sind noch vorhanden und entsprechen exakt dem
  bekannten Altvertrag (`espressif32@7.0.1`, `framework = arduino`,
  `board = esp32dev`); keine zusaetzlichen, unerwarteten PlatformIO-
  Environments; `scripts/check_platformio_config.py` bleibt in dieser
  Phase unveraendert **parallel aktiv** (wird erst in Commit 5 entfernt).

Dieser Parallelzustand ist ein ausdruecklich zeitlich begrenzter
Migrationsvertrag fuer Commits 1–4 und wird **nicht** als Endzustand
dokumentiert.

Selftests in Commit 1: getrennte ESP-IDF-Profile; vertauschte Overlays;
gemeinsamer Buildordner; gemeinsames `sdkconfig`; kein Profil; beide
Profile gleichzeitig; falscher IDF-Tag/-Commit; reale Aktoren aktiviert;
unerwartetes zusaetzliches PlatformIO-Environment.

**Phase 2 — Commit 5 (endgueltiger ESP-IDF-only-Vertrag):** Dasselbe
`scripts/check_build_profiles.py` wird im selben Commit, der den
Arduino-Pfad entfernt, auf den finalen Vertrag umgestellt:

- PlatformIO enthaelt nur noch `native`; keine Arduino-Environments; kein
  `framework = arduino`; keine `espressif32`-Plattform;
  `APP_PROFILE_NATIVE=1`; bestehende native Warnflags und C++17-Vertrag;
- beide ESP-IDF-Profile bleiben unveraendert vollstaendig geprueft (siehe
  Phase 1);
- `scripts/check_platformio_config.py` wird geloescht (Abschnitt 7.5.3);
- neuer Selftest: jede erneute Einfuehrung eines Arduino- oder
  `espressif32`-Environments schlaegt fehl.

**Kein dauerhafter Bypass:** Die Phase-1-Pruefung fuer die Arduino-
Environments ist ein Codepfad, der in Commit 5 **entfernt** (nicht per
Flag umgeschaltet) wird. Es gibt zu keinem Zeitpunkt eine Kommandozeilen-
option wie `--allow-legacy-arduino` oder `--migration-mode`; der
Parallelvertrag existiert ausschliesslich als der in Commit 1 geschriebene
und in Commit 5 durch die endgueltige Fassung ersetzte Skriptinhalt. Im
finalen Repository (nach Commit 5) akzeptiert der Guard den Parallelzustand
nicht mehr.

`.github/workflows/build.yml`: Schritt „PlatformIO installieren“ bleibt
waehrend Commits 1–4 fuer `native` **und** die Arduino-Envs bestehen und
wird erst in Commit 5 auf ausschliesslich `native` reduziert;
`esp32_bringup`/`esp32_release`-Buildaufrufe unter ESP-IDF laufen ab
Commit 2 zusaetzlich (nicht anstelle) ueber
`scripts/build_esp_idf_profiles.py`.

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

**Aufbewahrung (Korrektur):** Der bestehende Workflow setzt fuer die
bisherigen Artefakte (`platformio-build-log`, `build-report`) **keinen**
expliziten `retention-days`-Wert (live verifiziert: kein `retention-days`
in `.github/workflows/build.yml`). Fuer die neuen ESP-IDF-Artefakte wird in
#74 **erstmals explizit** `retention-days: 30` festgelegt — keine Analogie
zu einer bereits bestehenden Einstellung, da keine existiert. Eine
Vereinheitlichung der bestehenden Uploads auf denselben Wert ist fuer #74
nicht vorgesehen und wird nicht umgesetzt, ausser der Owner bestaetigt dies
ausdruecklich als zusaetzlichen, im Dateiumfang und Commit-Schnitt separat
zu benennenden Punkt.

Keine lokalen Secrets oder privaten absoluten Pfade in Artefaktmanifesten —
siehe Abschnitt 7.7.4 fuer die tatsaechliche Pruefung (nicht durch das
unveraenderte `scripts/check_secrets.py` allein abgedeckt).

#### 7.7.3 Schwellenwerte

Exakte Byte-Schwellenwerte bleiben `TBD_IMPLEMENTATION_BUDGET`; #74 liefert
die erste dokumentierte, maschinenlesbare Baseline (Abschnitt 4.4), keine
CI-Abbruchgrenze. Spaeteres Entscheidungskriterium: reale
Belastungs-/Heap-Messung auf Zielhardware nach Anbindung von
Sensoren/Display/Aktoren.

#### 7.7.4 Secret- und Pfadpruefung fuer generierte Artefakte (eingefuehrt
in zweiter Reviewrunde, Geltungsbereich in dritter Reviewrunde
vervollstaendigt)

**Befund (live verifiziert):** `scripts/check_secrets.py` ermittelt seine
Pruefmenge ueber `git ls-files` (`scripts/check_secrets.py:70`) — es scannt
ausschliesslich von Git getrackte Dateien. Die in Abschnitt 7.7.1 und
7.7.2 neu eingefuehrten generierten Artefakte (`size.json`, Buildlogs,
Artefaktmanifeste, generierte `sdkconfig`) sind bewusst **nicht** getrackt
(sie liegen unter `build/`, das gitignored bleibt) und werden von
`scripts/check_secrets.py` in seiner aktuellen Form **nicht** erfasst. Die
vorherige Planfassung behauptete faelschlich das Gegenteil.

**Verbindliche Korrektur:** `scripts/check_secrets.py` erhaelt einen
optionalen, zusaetzlichen CLI-Modus fuer explizit benannte, ungetrackte
Textdateien, ohne die bestehende Pruefung getrackter Repositorydateien zu
veraendern.

**Vollstaendiger Geltungsbereich (Korrektur aus dritter Reviewrunde):**
Alle vor `actions/upload-artifact` hochgeladenen **Textartefakte** werden
erfasst, nicht nur Manifest und Buildlog:

```bash
python scripts/check_secrets.py \
  --scan-path build/esp32_bringup/artifact-manifest.json \
  --scan-path build/esp32_release/artifact-manifest.json \
  --scan-path build/esp32_bringup/build.log \
  --scan-path build/esp32_release/build.log \
  --scan-path build/esp32_bringup/sdkconfig \
  --scan-path build/esp32_release/sdkconfig \
  --scan-path build/esp32_bringup/compile_commands.json \
  --scan-path build/esp32_release/compile_commands.json \
  --scan-path build/esp32_bringup/size.json \
  --scan-path build/esp32_release/size.json \
  --scan-path build/esp32_bringup/flasher_args.json \
  --scan-path build/esp32_release/flasher_args.json
```

(exakte, tatsaechlich vorhandene Text-Flashargumentdateien statt eines
festen Namens, falls `idf.py` einen abweichenden Dateinamen erzeugt).

Der genaue Flag-Name (`--scan-path` oder gleichwertig) wird in der
Umsetzung final benannt; der Vertrag ist bereits hier verbindlich:

- die bestehende Pruefung getrackter Repositorydateien bleibt unveraendert
  aktiv, auch wenn `--scan-path` genutzt wird;
- jede per `--scan-path` benannte Datei wird zusaetzlich auf dieselben
  Geheimnismuster geprueft wie getrackte Dateien;
- Binärdateien (z. B. `.bin`, `.elf`) werden nicht als Text interpretiert
  und nicht in den Musterabgleich einbezogen;
- **kontextbezogene Pfadregel:** selbst erzeugte Manifeste
  (`artifact-manifest.json`) muessen normalisiert sein — private absolute
  Benutzerpfade (z. B. `/home/<name>/...`) darin werden erkannt und
  abgelehnt oder auf einen projektrelativen/CI-Pfad normalisiert;
  fremdgenerierte Compile-Datenbanken (`compile_commands.json`) duerfen
  dagegen bekannte, ephemere CI-/Runner-Pfade (z. B. `/home/runner/...`,
  Toolchain-Installationspfade) enthalten, ohne als Fund gewertet zu
  werden — sie duerfen jedoch zu keinem Zeitpunkt Tokens, Credentials oder
  private lokale Owner-Pfade (z. B. Entwickler-Heimatverzeichnisse ausserhalb
  des CI-Runners) enthalten; diese werden weiterhin erkannt;
- eine erwartete, aber fehlende Textartefaktdatei ist ein harter Fehler,
  kein stiller Übersprung;
- der Aufruf erfolgt in der CI **vor** `actions/upload-artifact` fuer die
  betroffenen ESP-IDF-Artefakte (Commit 3, Abschnitt 8).

Neue Selftest-Faelle in `scripts/selftest_quality_gates.py`:

- Geheimnis in einer ungetrackten, per `--scan-path` benannten Manifestdatei
  wird erkannt;
- ein privater absoluter Benutzerpfad in einer selbst erzeugten
  Manifestdatei wird erkannt;
- ein normalisierter CI-/Projektpfad in einer Manifestdatei wird akzeptiert;
- ein bekannter ephemerer CI-Runner-Pfad in einer
  `compile_commands.json`-Fixture wird **nicht** als Fund gewertet;
- ein Token/Credential-Muster in einer `compile_commands.json`-Fixture wird
  weiterhin erkannt;
- eine fehlende, aber erwartete Textartefaktdatei fuehrt zu einem Fehler;
- eine Binärdatei wird nicht als Text gescannt.

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
- alle `uses:`-Actions auf vollstaendige Commit-SHAs pinnen, mit lesbarem
  Versionskommentar daneben. Die folgenden, zum Recherchezeitpunkt dieser
  Planungsphase verifizierten SHAs sind mit der Planfreigabe **verbindlich
  und werden in der Umsetzung unveraendert eingesetzt** (kein stiller
  Wechsel auf einen aktuelleren Tag-/Branch-HEAD; eine Aenderung erfordert
  den vollstaendigen Fallback-Prozess aus Abschnitt 7.1, nicht eine
  technische Detailkorrektur):

  ```text
  actions/checkout@d23441a48e516b6c34aea4fa41551a30e30af803          # v6
  actions/setup-python@ece7cb06caefa5fff74198d8649806c4678c61a1      # v6
  actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02   # v4
  espressif/install-esp-idf-action@8fc05d1470d5591417e7a3707a1f2bec178db4ae  # v1, siehe Abschnitt 7.1
  ```

  Die Umsetzung darf lediglich pruefen, dass diese Commits weiterhin
  existieren, ihre Herkunft kontrollieren und bekannte Security-Hinweise
  dazu dokumentieren — nicht sie ersetzen.

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

**Zeitpunkt (Korrektur aus dritter Reviewrunde, ersetzt die vormals
weichere Formulierung "nach Commit 5 beziehungsweise auf dem finalen
Implementierungs-Head"):** Beide Hardware-Smoke-Tests werden
**ausschliesslich auf dem exakten finalen Implementierungs-Head nach
Commit 6** durchgefuehrt — nicht bereits auf Commit 5 und nicht ein
weiteres Mal wiederholt, falls Commit 6 nach einem ersten Testlauf noch
Aenderungen erhaelt (in diesem Fall wird der Test auf dem dann neuen
finalen Head erneut ausgefuehrt). Je mindestens 35 Sekunden:

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

**Testnachweis** (je Profil, analog zum bereits bewaehrten Format aus PR
#78) enthaelt mindestens: finalen Implementierungs-Commit-SHA; Profil;
Firmware-Build-SHA (identisch mit dem finalen Commit-SHA, da beide Profile
aus demselben Head gebaut werden); seriellen Port; Beobachtungsdauer;
tatsaechlich gemessene Heartbeat-Werte (erster Wert, Anzahl, Abstaende);
tatsaechlich gemessene Ressourcenwerte (Heap, Stack-HWM, Zeitpunkte);
PASS/FAIL je Kriterium.

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

#### 7.13.3 Third-Party-Komponentenregister (Korrektur: bestehendes
Dokument, kein neues)

**Befund (live verifiziert):** `docs/THIRD_PARTY_COMPONENTS.md` existiert
bereits auf `main` (Stand 27.07.2026), Status `DRAFT – Ownerfreigabe
ausstehend`, verlinkt aus dem
`Release-1-Adopt-or-build-Audit`
(`docs/audits/RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`). Die Tabellenzeile
`ESP32-/Arduino-Plattform` nennt aktuell `PlatformIO espressif32@7.0.1,
Arduino-ESP32 2.0.17 (dcc1105b)` als Plattformbasis. Die urspruengliche
Planfassung sprach faelschlich von einem **neuen** Dokument gleichen Namens.

**Verbindliche Korrektur:** `docs/THIRD_PARTY_COMPONENTS.md` wird in
Commit 6 **in-place aktualisiert**, nicht neu erstellt. Struktur, Historie
und alle Statuswerte (`FRAMEWORK_CANDIDATE`, `FIRST_EVALUATION_CANDIDATE`
usw.) bleiben erhalten. Der bestehende Status
`DRAFT – Ownerfreigabe ausstehend` wird **nicht** still entfernt oder
geaendert; #74 aendert ausschliesslich die im Folgenden genannten
Tatsachenzeilen, nicht die Freigabesystematik des Dokuments.

Aktualisiert werden mindestens:

- die Zeile `ESP32-/Arduino-Plattform` auf die aktive Plattformbasis
  ESP-IDF 6.0.2 (exakter Tag `v6.0.2`, Commit
  `7101770dc6db2667b3c477cc31365dd1acd6db4e`);
- `PlatformIO espressif32@7.0.1`/`Arduino-ESP32 2.0.17` nicht mehr als
  aktive Produktionsplattform darstellen;
- Arduino-spezifische Kandidatenzeilen (`Persistenz` – Arduino-ESP32
  Preferences/NVS; `Webserver` – Arduino-ESP32 `WebServer`) nicht mehr als
  gegen eine aktive Arduino-Laufzeit bewertete Basis darstellen, sondern
  als unter ESP-IDF offen zu bewertende Kandidaten kennzeichnen;
- **keine** Ersatzbibliothek in #74 vorzeitig auswaehlen; die spaetere
  Neu-Evaluation unter ESP-IDF bleibt klar als offen markiert (z. B.
  weiterhin `FIRST_EVALUATION_CANDIDATE`/`SPIKE_REQUIRED`/
  `FINAL_SELECTION_PENDING`, nicht `NOT_SELECTED`, ohne echte Pruefung).

**Historische Auditdateien bleiben unangetastet:** `docs/audits/`
(`RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`,
`THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md`, `COMPONENT_EVALUATIONS.md` und
alle weiteren Dateien in diesem Verzeichnis) werden in #74 **nicht**
geaendert. Es gibt **keine** bedingte Erlaubnis, dort bei fehlendem
Statusbanner zusaetzlich einzugreifen — diese in der vorherigen Planfassung
enthaltene Bedingung ist ersatzlos gestrichen. Sollten historische Audits
spaeter ein Statusbanner benoetigen, geschieht das in einer separaten,
eigens begrenzten Dokumentationsaenderung ausserhalb von #74.

#### 7.13.4 `include/app_config.hpp`-Fehlermeldungen

Rein mechanische Korrektur veralteter Fehlermeldungen (kein Vertrags- oder
Verhaltenswechsel): von z. B. `"must be defined by the PlatformIO profile"`
zu `"must be defined by the active build profile"`.

#### 7.13.5 ADR-013 und `lib/README.md` (neuer Punkt aus zweiter Reviewrunde)

**Befund (live verifiziert):** `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`
beschreibt aktuell `src/main.cpp` als "Zusammensetzungsstelle und
Arduino-Einstieg" und listet unter anderem
`Produktionsadapter -> ESP32/Arduino oder anwendungsneutrale
Hostumgebung` sowie den Satz "`main.cpp` ist eine Composition Root. Die
Datei darf ... den Arduino-Einstieg und minimale Bootausgabe
bereitstellen." `lib/README.md` beschreibt in seiner Modulstruktur nur
`src/main.cpp` als "Composition Root: instanziiert Plattform und
Anwendung" und erwaehnt `lib/device_platform_esp_idf/` gar nicht. Beide
Dokumente sind nach #74 materiell falsch, da dann zwei Composition Roots
mit unterschiedlicher Rolle existieren
(`src/main.cpp` native-only, `main/app_main.cpp` ESP-IDF).

**Verbindliche Korrektur (Commit 6):**

`docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`:

- kein neues ADR, keine Ersetzung der Grundentscheidung;
- Status ergaenzt, z. B.
  `accepted; amended by Issue #71 / PR #79` (exakte Formulierung in der
  Umsetzung);
- Repository-Mapping auf zwei Composition Roots korrigiert
  (`src/main.cpp` native-only, `main/app_main.cpp` ESP-IDF);
- Arduino-Verweise aus dem beschriebenen aktuellen Produktionspfad entfernt
  (historische Begruendung der urspruenglichen Entscheidung bleibt
  nachvollziehbar, wird nicht rueckwirkend umgeschrieben);
- Abhaengigkeitsrichtung um
  `device_platform_esp_idf -> device_platform` und die ESP-IDF-
  Adaptergrenze ergaenzt;
- native Teststruktur unveraendert.

`lib/README.md`:

- Modulübersicht korrigiert auf `src/main.cpp` (native-only Composition
  Root), `main/app_main.cpp` (ESP-IDF Composition Root),
  `lib/device_platform_esp_idf/` (konkrete ESP-IDF-Adapter, abhaengig von
  `device_platform`);
- bestehende verbotene Abhaengigkeitsrichtungen (`-X->`) und der Ausschluss
  von `device_platform_test_support` aus Produktionsbuilds bleiben
  inhaltlich erhalten.

Beide Dateien sind Teil des Dateiumfangs (Abschnitt 5), von Commit 6
(Abschnitt 8), der Dokumentationsaenderungen (Abschnitt 14) und der
Abnahmekriterien (Abschnitt 16).

## 8. Geplanter kleiner PR-/Commit-Schnitt

Alle Commits im selben Draft-PR nach Planfreigabe, in dieser korrigierten
Reihenfolge:

1. **Profil- und lokaler Buildvertrag:** `main/Kconfig.projbuild`,
   Bring-up-/Release-Overlays, getrennte Build-/`sdkconfig`-Pfade
   (Abschnitt 7.3), `main/CMakeLists.txt`-Mapping (Abschnitt 7.2),
   `scripts/build_esp_idf_profiles.py`, `scripts/check_build_profiles.py`
   im **Parallel-Migrationsvertrag** (ESP-IDF-Profile plus unveraendert
   bestehende Arduino-Envs, Abschnitt 7.5.2 Phase 1) samt Selftests. Beide
   ESP-IDF-Profile lokal gruen. Arduino-Produktionsprofile und
   `scripts/check_platformio_config.py` bleiben unveraendert bestehen.
2. **ESP-IDF-CI additiv einfuehren:** gepinnte ESP-IDF-Installation
   (Abschnitt 7.1), beide Profile ueber den kanonischen Buildtreiber in
   `build.yml`; bestehender Arduino-Produktionspfad bleibt bestehen; CI auf
   exakt diesem Commit vollstaendig gruen.
3. **Ressourcenbericht und Artefakte:** `idf.py size --format json2`-
   Auswertung, Erweiterung von `scripts/build_report.py`,
   profilspezifische CI-Artefakte, Erweiterung von
   `scripts/check_secrets.py` um den Scan-Modus fuer generierte
   Artefakttexte samt neuer Selftests (Abschnitt 7.7, insbesondere 7.7.4).
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
   bleibt, Abschnitt 7.5.1); `scripts/check_platformio_config.py` loeschen;
   `scripts/check_build_profiles.py` im selben Commit vom
   Parallel-Migrationsvertrag auf den finalen ESP-IDF-only-Vertrag
   umgestellt (kein neues Skript, kein Bypass-Flag, Abschnitt 7.5.2 Phase
   2); `.github/workflows/build.yml`-Schritt „PlatformIO installieren“ auf
   ausschliesslich `native` reduziert.
6. **Upgradevertrag und Abschlussdokumentation:**
   `docs/ESP_IDF_UPGRADE_CONTRACT.md` (neu), `docs/THIRD_PARTY_COMPONENTS.md`
   (in-place aktualisiert, kein neues Dokument, Abschnitt 7.13.3),
   `docs/CI_AND_QUALITY_GATES.md`, `docs/ARCHITECTURE.md`, `README.md`,
   `docs/DECISIONS.md` (ADR-001), `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`
   und `lib/README.md` (Abschnitt 7.13.5), `AGENTS.md`, `CHANGELOG.md`.
   `docs/audits/` bleibt unangetastet (Abschnitt 7.13.3).

**Finaler Head:** Beide Hardware-Smoke-Tests (Bring-up, Release) werden
**ausschliesslich** auf dem exakten finalen Implementierungs-Head **nach
Commit 6** durchgefuehrt, zusammen mit einer erneuten vollstaendigen
CI-Ausfuehrung auf demselben Head (Abschnitt 7.12). Nicht auf Commit 5 und
nicht vor Commit 6.

Issue #71 wird **nicht** in diesem PR geschlossen, sondern erst nach
Owner-Merge und verifiziertem Abschluss von #74, analog zum bereits
etablierten Muster bei #72/#73.

## 9. Offene Entscheidungen

Gegenueber der urspruenglichen Planfassung sind die frueheren offenen
Punkte 1 (ADR-001), 2 (`AGENTS.md`-Verweis), 3 (CI-Caching), 4 (native
Eintrittsdatei) und 5 (Driftpruefung) durch das erste Review verbindlich
entschieden (Abschnitte 7.1, 7.2, 7.5.1, 7.5.2, 7.13). Das zweite Review hat
zusaetzlich die Behandlung von `docs/THIRD_PARTY_COMPONENTS.md` (Abschnitt
7.13.3), die Hardware-Gate-Reihenfolge (Abschnitte 1, 6, 8), die exakten
Action-/EIM-Pins (Abschnitt 7.1), die reale Secret-/Pfadpruefung (Abschnitt
7.7.4), die ADR-013-/`lib/README.md`-Aktualisierung (Abschnitt 7.13.5) und
die Artefakt-Aufbewahrung (Abschnitt 7.7.2) verbindlich entschieden. Das
dritte Review hat zusaetzlich den zweiphasigen Uebergangsvertrag fuer
`scripts/check_build_profiles.py` (Abschnitt 7.5.2), die Unveraenderlichkeit
der gepinnten Action-SHAs (Abschnitte 7.1, 7.11), den dokumentierten
Lizenzkonflikt der Installations-Action (Abschnitt 7.1), den vollstaendigen
Textartefakt-Scan (Abschnitt 7.7.4) und den exakten Zeitpunkt der
Hardware-Smoke-Tests (Abschnitt 7.12) verbindlich entschieden. Diese Punkte
entfallen als offene Punkte. Es verbleibt:

1. Byte-Budget-Schwellenwerte bleiben `TBD_IMPLEMENTATION_BUDGET`
   (Abschnitt 7.7.3) — kein Blocker, sondern ein dokumentierter,
   bewusst offener Wert bis zu realer Belastungsmessung.
2. Die exakte Kconfig-Syntax fuer `main/Kconfig.projbuild` (Abschnitt 7.2)
   wird unmittelbar zu Beginn von Commit 1 gegen den echten ESP-IDF-6.0.2-
   Build verifiziert; sollte sie von der hier skizzierten Form abweichen,
   ist das eine technische Detailkorrektur ohne Vertrags-/Safetywirkung,
   keine materielle Planabweichung.
3. Sollte die offizielle Installations-Action mit den in Abschnitt 7.1
   exakt gepinnten und **verbindlichen** Werten (Action-Commit
   `8fc05d1470d5591417e7a3707a1f2bec178db4ae`, `version: "v6.0.2"`,
   `eim-version: "v0.1.7"`) ESP-IDF 6.0.2 auf `ubuntu-24.04` nachweislich
   nicht reproduzierbar bereitstellen, ist ausschliesslich der in
   Abschnitt 7.1 beschriebene vollstaendige Fallback-Prozess (Anhalten,
   neuer Plan-Commit, erneute Freigabe) zu befolgen — **kein** stiller
   Wechsel auf einen aktuelleren `v1`-HEAD oder einen anderen Commit.
4. Der in Abschnitt 7.1 dokumentierte `LICENSE_METADATA_CONFLICT` von
   `espressif/install-esp-idf-action` (Apache-2.0 in `LICENSE` versus MIT
   in `package.json`/`README.md`) bleibt ein dokumentierter Befund, kein
   Blocker; eine rechtliche Klaerung durch das Projekt ist nicht Teil von
   #74.
5. Der genaue Flag-Name fuer den in Abschnitt 7.7.4 beschriebenen
   `check_secrets.py`-Scan-Modus (`--scan-path` oder gleichwertig) wird in
   der Umsetzung final benannt; der Pruefvertrag selbst ist bereits
   verbindlich festgelegt.

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
  beschriebenen Fallback-Prozess;
- kein neues Dokument unter dem Pfad `docs/THIRD_PARTY_COMPONENTS.md`; die
  bestehende Datei wird ausschliesslich in-place aktualisiert (Abschnitt
  7.13.3);
- keine Aenderung an `docs/audits/` (historische Auditdateien, Abschnitt
  7.13.3).

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
- keine Secrets in Workflow, Cache oder Artefakten: getrackte Dateien
  bleiben durch das unveraenderte `scripts/check_secrets.py` abgedeckt;
  generierte, ungetrackte Artefakttexte (Manifeste, Buildlogs) werden erst
  durch den neuen `--scan-path`-Modus aus Abschnitt 7.7.4 tatsaechlich
  geprueft — die vorherige Planfassung behauptete hier faelschlich eine
  bereits bestehende Abdeckung;
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
- `scripts/check_build_profiles.py` wird in zwei Phasen getestet: Commit 1
  gegen den Parallelvertrag (ESP-IDF-Profile plus bestehende Arduino-Envs),
  Commit 5 gegen den finalen ESP-IDF-only-Vertrag (Abschnitt 7.5.2) —
  jeweils mit eigenen Selftest-Faellen fuer die jeweilige Phase;
- `scripts/selftest_quality_gates.py` erhaelt neue Fixture-Faelle fuer
  `scripts/check_build_profiles.py` (beide Phasen, Abschnitt 7.5.2), den
  erweiterten Format-Scope und den `--scan-path`-Modus von
  `scripts/check_secrets.py` fuer alle hochgeladenen Textartefakte
  (Abschnitt 7.7.4), sobald diese in der Umsetzung konkret vorliegen;
- Hardware-Smoke-Tests (Bring-up, Release) werden ausschliesslich auf dem
  finalen Implementierungs-Head nach Commit 6 durchgefuehrt, als
  Pflichtabschluss vor dem Merge (Abschnitt 7.12).

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
- `docs/THIRD_PARTY_COMPONENTS.md`: **bestehende** Datei in-place
  aktualisieren, DRAFT-Status bleibt erhalten (Abschnitt 7.13.3);
- `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md` und `lib/README.md`:
  Repository-Mapping auf zwei Composition Roots korrigieren (Abschnitt
  7.13.5);
- `docs/audits/` bleibt unveraendert (Abschnitt 7.13.3);
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
  `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`, `lib/README.md`, `AGENTS.md`,
  `docs/THIRD_PARTY_COMPONENTS.md` (in-place aktualisiert, DRAFT-Status
  erhalten), `CHANGELOG.md` aktuell; `docs/audits/` unveraendert.

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
  sowohl `05b987e3d2b375b82922990f718d0dc07c730a71` als auch
  `bbccd74d49b7fcb7c2c529054da5dcd2d8e9a754` als überholt;
- Status bleibt `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`;
- vollstaendiges Anhalten bis zu einem commitgebundenen
  `PLAN APPROVED`-Ownerkommentar auf den neuen Plan-Commit.
