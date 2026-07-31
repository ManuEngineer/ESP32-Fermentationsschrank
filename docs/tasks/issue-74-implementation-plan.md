# Plan: Issue #74 – CI, Ressourcenbaseline und ESP-IDF-Upgradevertrag

Status: Planungsphase (`IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`)

Issue: `#74`
Tracking: `#71`
Basis-`main`-SHA: `c3f8044be0f081822ca8724f67fa99e9614d57ef` (Merge-Commit von PR #78)

## 1. Ziel

Die ESP-IDF-6.0.2-Migration abschliessen:

- ESP-IDF 6.0.2 ueber GitHub Actions bauen (CI-Paritaet zum bereits lokal
  funktionierenden Pfad aus #72/#73);
- Bring-up- und Releaseprofile aus versionierten `sdkconfig.defaults`-Overlays
  reproduzierbar erzeugen;
- eine ESP-IDF-Ressourcenbaseline (Firmwaregroesse, statisches RAM, Mapfile)
  neben der bestehenden Arduino-Baseline als CI-Artefakt sichern;
- Format- und Static-Analysis-Abdeckung auf `main/` und
  `lib/device_platform_esp_idf/` erweitern;
- genau einen kanonischen Hosttestpfad festlegen und dokumentieren;
- den alten Arduino-ESP32-Produktionspfad nach bestandener CI-Paritaet
  entfernen, ohne den nativen Testpfad zu verlieren;
- einen versionierten ESP-IDF-Upgradevertrag (Bugfix/Minor/Major) dokumentieren;
- den Komponenten-/Lockfilevertrag fuer den aktuellen (leeren)
  Abhaengigkeitsstand festschreiben.

## 2. Nicht-Ziele

- keine reale Sensor-, Display-, NVS-, Web-, WLAN- oder Aktorintegration;
- keine Aenderung fachlicher Modelle, Wireformate oder #57-Semantik;
- keine Pin-, GPIO- oder Boardrevisionsentscheidung;
- keine neue externe ESP-IDF-Komponente und kein `idf_component.yml` ohne
  echten Bedarf;
- kein Wechsel des kanonischen Hosttestpfads weg von PlatformIO `native`;
- keine harten Byte-Budget-Schwellenwerte ohne Messbasis;
- keine ADR-Erstellung oder -Aenderung und keine `AGENTS.md`-Aenderung ohne
  gesonderte Ownerfreigabe.

## 3. Verbindliche Quellen und Entscheidungen

- `AGENTS.md` (Plan-first-Workflow, Architekturregeln, Release-1-Abgrenzung);
- Issue #71 (Ownerentscheid ESP-IDF-Migration, verbindliche Reihenfolge
  `#72 -> #73 -> #74`, verbindliche Architektur);
- Issue #74 (Scope, Upgradegrundsaetze, Grenzen, Akzeptanzkriterien);
- `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md` und `docs/DECISIONS.md`
  (ADR-001 PlatformIO/Arduino, ADR-008 4-MB-Budget, ADR-012 Bring-up-Profil,
  ADR-013 Plattformtrennung);
- `docs/CI_AND_QUALITY_GATES.md` (bestehende lokale ESP-IDF-Entwicklerpflicht
  aus #72/#73, ausdruecklich "noch nicht CI-gebunden ... vollstaendig Issue
  #74 vorbehalten");
- `docs/OPEN_POINTS.md` (`TBD_IMPLEMENTATION_BUDGET`);
- PR #77 (Merge-Commit `bf3b1a8b008ce6169494fb2e444cedeadb456d39`, Issue #72)
  und PR #78 (Merge-Commit `c3f8044be0f081822ca8724f67fa99e9614d57ef`, Issue
  #73, Hardware-Smoke-Test `PASS` auf Head `ebc54b65d100590a0f2604e08b155110bc25c2f4`).

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
  `esp32_release`; dieses Gap muss #74 schliessen (Abschnitt 7.2).
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
  wird durch den PlatformIO-`native`-Build mitkompiliert.
- `main/app_main.cpp` (ESP-IDF, seit #73 hardwareverifiziert): echter
  `app_main()`-Composition-Root, kooperative Einzelschleife,
  `EspTimerTimeSource`, identisches Bootlogging/Heartbeat/Ressourcen-Logging.

### 4.2 CI und Quality Gates

`.github/workflows/build.yml` (`permissions: contents: read`, ein Job
`firmware` auf `ubuntu-latest`):

1. `actions/checkout@v6`, `actions/setup-python@v6` (3.13);
2. `pip install platformio==6.1.19`;
3. `apt-get install clang-format-18 clang-tidy-18` (versionsgepinnt);
4. `clang-format --dry-run --Werror` ueber
   `find src include lib test -name "*.cpp" -o -name "*.hpp" -o -name "*.h"`
   — **Befund:** `main/` fehlt in dieser Liste vollstaendig;
5. `scripts/build_report.py` baut `native esp32_bringup esp32_release` und
   erzeugt `build-report.md` (PlatformIO-Groessenbericht, informativ,
   `TBD_IMPLEMENTATION_BUDGET`);
6. `scripts/check_platformio_config.py` — **Befund:** hart auf
   `EXPECTED_PLATFORM = espressif32@7.0.1`, `EXPECTED_BOARD = esp32dev`
   verdrahtet; verliert mit Entfernung der Arduino-Envs seinen gesamten
   Pruefgegenstand;
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
vorhanden):

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
Ausgangsmessung fuer den in Abschnitt 7.5 geplanten CI-Ressourcenbericht;
keine Schwellenwerte werden daraus abgeleitet (`TBD_IMPLEMENTATION_BUDGET`).

## 5. Betroffene Module und voraussichtlich betroffene Dateien

Nur in der spaeteren Umsetzungsphase, nach Planfreigabe, zu aendern:

- `.github/workflows/build.yml`
- `CMakeLists.txt`, `main/CMakeLists.txt`
- neu: `sdkconfig.defaults.esp32_bringup`, `sdkconfig.defaults.esp32_release`
  (Namen vorbehaltlich Abschnitt 7.2)
- `platformio.ini` (Entfernung `[env:esp32_bringup]`/`[env:esp32_release]`)
- `src/main.cpp` (Entfernung, Arduino-Zweig verschwindet vollstaendig; der
  native `#else`-Zweig wird vor der Entfernung an die neue Eintrittsstelle
  fuer `native` gepruest/verschoben, siehe Abschnitt 7.3)
- `scripts/build_report.py` (Erweiterung um ESP-IDF-Groessendaten)
- `scripts/check_platformio_config.py` (Ablösung/Entfernung)
- `scripts/check_architecture_boundaries.py` (nur falls die Bestandsaufnahme
  in der Umsetzungsphase eine neue Grenze zeigt; aktuell keine Aenderung
  identifiziert)
- neu: `docs/ESP_IDF_UPGRADE_CONTRACT.md` (Upgradevertrag, Abschnitt 7.8)
- `docs/CI_AND_QUALITY_GATES.md`, `docs/ARCHITECTURE.md`, `README.md`,
  `CHANGELOG.md`
- `docs/DECISIONS.md` (ADR-001-Statuspflege, siehe offene Entscheidung 9.1)
- `.clang-tidy` (`HeaderFilterRegex` ggf. erweitern, falls `main/` erfasst
  werden soll)

Kein produktiver Fachcode (`lib/device_platform/`, `lib/fermentation_app/`,
`main/app_main.cpp`, `src/main.cpp`-Kernlogik) wird inhaltlich veraendert.

## 6. Abhaengigkeiten und Gates

- **Abhaengigkeit:** Issue #73 ist Voraussetzung und bereits erfuellt
  (geschlossen, PR #78, Merge-Commit
  `c3f8044be0f081822ca8724f67fa99e9614d57ef`, Hardware-Smoke-Test `PASS`).
  Keine offene technische Abhaengigkeit blockiert den Start der Umsetzung.
- **Gate 1 — Planfreigabe:** Umsetzung beginnt ausschliesslich nach
  commitgebundenem `PLAN APPROVED`-Ownerkommentar auf den finalen
  Plan-Commit dieser Datei (AGENTS.md, Plan-first-Workflow).
- **Gate 2 — Arduino-Entfernung:** Commit 5 (Abschnitt 8) darf erst erfolgen,
  nachdem die ESP-IDF-CI-Strecke (Commits 1–4) in mehreren aufeinander
  folgenden gruenen CI-Laeufen auf demselben Branch bestanden hat; kein
  gleichzeitiger Austausch in einem Commit (siehe Abschnitt 7.3).
- **Gate 3 — offene Ownerentscheidungen:** Die in Abschnitt 9 gelisteten
  Punkte (insbesondere ADR-001-Statuspflege und `AGENTS.md`-Verweis)
  benoetigen eine gesonderte Bestaetigung, bevor die zugehoerigen Dateien
  angefasst werden; ihr Fehlen blockiert nicht die uebrige Umsetzung.
- **Kein Hardware-Gate:** #74 fuehrt keine neue Hardware-, Sensor-, Display-
  oder Aktorintegration ein; ein Hardware-Smoke-Test ist fuer #74 selbst
  nicht Teil der Abnahmekriterien (anders als #73). Ob ein abschliessender
  Wiederholungs-Smoke-Test nach Entfernung des Arduino-Pfads sinnvoll ist,
  bleibt eine Umsetzungsentscheidung, kein Blocker fuer die Planfreigabe.

## 7. Geplante Loesung je Vorgabenbereich

### 7.1 CI-Varianten (offizielle Primaerquellen)

Verglichen (Stand dieser Recherche, Juli 2026):

| Variante | Quelle | Lizenz | Pinning auf v6.0.2 | Docker noetig | Zusaetzliche GH-Action-Vertrauensflaeche |
|---|---|---|---|---|---|
| 1. Rohes `docker run espressif/idf:v6.0.2` | [Espressif-Doku „IDF Docker Image“](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/idf-docker-image.html), Image `espressif/idf` | Docker-Image selbst ohne gesonderte Lizenzangabe in der Doku | exakter Tag `v6.0.2` | ja | keine (nur `docker run`) |
| 2. `espressif/install-esp-idf-action@<sha>` | [espressif/install-esp-idf-action](https://github.com/espressif/install-esp-idf-action) | Apache-2.0 | `version: "v6.0.2"` | nein, native Installation im Runner | eine offizielle Espressif-Action |
| 3. `espressif/esp-idf-ci-action@<sha>` | [espressif/esp-idf-ci-action](https://github.com/espressif/esp-idf-ci-action) | MIT | `esp_idf_version: v6.0.2` (Docker-Hub-Tag) | ja (kapselt Variante 1) | eine offizielle Espressif-Action; **Hinweis:** `@v1` ist laut eigener Doku ein bewusst gleitender Tag ("v1 tag always points to the latest compatible release") und muss fuer Reproduzierbarkeit auf einen konkreten Commit-SHA gepinnt werden |
| 4. Manuelle native Installation ohne jede Action | offizielle ESP-IDF-"Standard Setup"-Anleitung, exakt der bereits in `docs/CI_AND_QUALITY_GATES.md` dokumentierte lokale Ablauf (`git clone --recursive` auf Tag `v6.0.2`, `./install.sh esp32`, `. ./export.sh`) | – (kein Drittanbieter-Code, nur `espressif/esp-idf` selbst) | exakter Git-Tag `v6.0.2` | nein | keine |

**Empfehlung: Variante 4** (manuelle native Installation, ohne Docker und
ohne zusaetzliche Marketplace-Action).

Begruendung:

- **DRY:** derselbe Installationsablauf ist bereits die dokumentierte lokale
  Entwicklerpflicht in `docs/CI_AND_QUALITY_GATES.md`; CI fuehrt exakt diesen
  einen Ablauf aus, statt einen zweiten, abweichenden Beschaffungsweg zu
  etablieren.
- **Supply-Chain-Minimierung:** keine zusaetzliche Drittanbieter-Action und
  kein Docker-Image muessen bewertet, aktualisiert oder auf Kompromittierung
  ueberwacht werden; einzige Vertrauensbasis bleibt `espressif/esp-idf`
  selbst, exakt auf `v6.0.2` gepinnt (Tag plus, sofern zur Umsetzungszeit
  sinnvoll, zusaetzlich Commit-SHA-Verifikation).
- **Konsistenz:** `build.yml` installiert bereits heute Werkzeuge
  (`platformio`, `clang-format-18`, `clang-tidy-18`) direkt per `pip`/`apt`
  ohne Wrapper-Actions; Variante 4 fuegt sich in denselben Stil ein.
- Variante 2 (offizielle native Install-Action) bleibt die dokumentierte
  Alternative, falls sich in der Umsetzung Wartungsaufwand oder
  Laufzeitprobleme zeigen, die eine gepflegte Action rechtfertigen; sie ist
  wegen Apache-2.0-Lizenz und offizieller Espressif-Pflege unproblematisch,
  muesste aber ebenfalls auf einen Commit-SHA statt `@v1` gepinnt werden.
- Variante 3 wird nicht empfohlen: sie kapselt Docker zusaetzlich zu einer
  Action, ohne gegenueber Variante 4 einen belegbaren Vorteil zu bieten, und
  ihr `@v1`-Tag ist laut eigener Dokumentation ausdruecklich nicht
  versionsstabil.
- Variante 1 (rohes Docker) wird nicht empfohlen: fuehrt eine
  Docker-in-Runner-Abhaengigkeit ein, ohne dass das Projekt an anderer Stelle
  bereits Docker verwendet; grosses Image ohne erwiesenen Cache-Vorteil
  gegenueber einer gepinnten nativen Installation mit `actions/cache`.

Caching: `actions/cache` (bereits offizielle GitHub-Action, im Projekt noch
nicht verwendet) fuer den ESP-IDF-Checkout und die Toolchain, Cache-Key
gebunden an den exakten `v6.0.2`-Pin. Konkretes Cache-Schluesseldesign bleibt
`EVALUATE_BEFORE_RELEASE` (Abschnitt 9).

### 7.2 Profil- und sdkconfig-Vertrag

Aktuell setzt `main/CMakeLists.txt` `APP_PROFILE_ESP32_BRINGUP=1`
unbedingt; ein ESP-IDF-Aequivalent zu `esp32_release` existiert nicht.

**Verifizierter Vertrag aus `include/app_config.hpp`** (gelesen, nicht nur
angenommen): Die Datei erzwingt per Praeprozessor
`#if (defined(APP_PROFILE_NATIVE) + defined(APP_PROFILE_ESP32_BRINGUP) +
defined(APP_PROFILE_ESP32_RELEASE)) != 1` einen `#error`, falls nicht
**genau eine** der drei Profildefinitionen gesetzt ist — ebenso fuer
`APP_TARGET_FLASH_MB`, `APP_REQUIRE_PSRAM`, `APP_WEB_OTA_ENABLED` und
`APP_REAL_ACTUATORS_ENABLED` (je ein eigener `#error`, falls fehlend).
`kEsp32ReleaseProfilePolicy` setzt bereits heute (unveraendert, keine
Codeaenderung noetig) `ActuatorPolicy::RequireVerifiedHardware` und
`realActuatorsEnabled=false`; `hasSafeDefaults()` verlangt fuer beide
ESP32-Profile zwingend `HardwareState::HardwareUnverified` und die jeweils
passende `ActuatorPolicy`. Jede geplante Umstellung muss diesen bereits
bestehenden, `static_assert`-abgesicherten Vertrag unveraendert erfuellen,
nicht neu erfinden.

Geplant, analog zum bestehenden `platformio.ini`-Muster (`[profile]`-Basis +
Env-spezifische Ergaenzung), und mit diesem Vertrag als bindender
Randbedingung:

- eine gemeinsame `sdkconfig.defaults` (bereits vorhanden, bleibt Basis);
- zwei schmale Overlay-Dateien `sdkconfig.defaults.esp32_bringup` und
  `sdkconfig.defaults.esp32_release`, kombiniert ueber ESP-IDFs
  `SDKCONFIG_DEFAULTS`-Mechanismus (`idf.py -DSDKCONFIG_DEFAULTS=...`);
- `main/CMakeLists.txt` darf danach zu keinem Zeitpunkt mehr als eine der
  drei `APP_PROFILE_*`-Definitionen gleichzeitig setzen (sonst schlaegt der
  bestehende `#error` fehl) und muss weiterhin exakt eine setzen. Zwei
  technisch gangbare, gleichwertige Mechanismen dafuer: (a) ein schmales
  projektspezifisches Kconfig-Bool (z. B. ueber `Kconfig.projbuild`), das nur
  die `esp32_release`-Overlay-Datei auf `y` setzt und das
  `main/CMakeLists.txt` per generiertem `sdkconfig.cmake`/`CONFIG_*`
  ausliest, oder (b) ein an `idf.py` uebergebener CMake-Cache-Parameter
  (z. B. `-DAPP_PROFILE=release`), den `main/CMakeLists.txt` direkt
  auswertet, ohne Kconfig-Umweg. Die konkrete Wahl zwischen (a) und (b) ist
  eine kleine, produktlich unsichtbare technische Detailentscheidung ohne
  Schnittstellen-, Security- oder Safetywirkung (AGENTS.md, Plan-first,
  Abschnitt "Kleine technische Detailentscheidungen") und wird in der
  Umsetzungsphase getroffen, nicht hier vorweggenommen;
- `esp32_bringup` erzwingt weiterhin `HARDWARE_UNVERIFIED`/
  `LOCKED_FOR_BRINGUP`, `esp32_release` weiterhin `HARDWARE_UNVERIFIED`/
  `REQUIRE_VERIFIED_HARDWARE` — in beiden Faellen bleibt
  `APP_REAL_ACTUATORS_ENABLED=0` gesetzt (Release 1 gibt laut `AGENTS.md`
  nie automatisch reale Aktoren frei; `kRealActuatorsEnabledByDefault` ist
  in `include/app_config.hpp` fuer beide Policies bereits `false` und durch
  `static_assert` abgesichert);
- 4-MB-Flash, kein PSRAM, keine Web-OTA bleiben in der gemeinsamen
  `sdkconfig.defaults` verankert, nicht dupliziert;
- Konfigurationsdrift-Erkennung: `idf.py build` scheitert bereits heute bei
  widerspruechlicher Konfiguration; zusaetzlich prueft CI nach dem Build,
  dass das generierte `sdkconfig` die erwarteten Kernwerte (Flashgroesse,
  `APP_REAL_ACTUATORS_ENABLED=0`) tatsaechlich enthaelt (Erweiterung eines
  Skripts, kein neues Parallelwerkzeug — konkrete Zuordnung in der
  Umsetzungsphase).

### 7.3 Arduino-Ablösung

- `platformio.ini`: `[env:esp32_bringup]` und `[env:esp32_release]` entfernen,
  `default_envs` auf `native` reduzieren; `[env:native]` bleibt unveraendert
  bestehen (siehe Abschnitt 7.4).
- `src/main.cpp`: vollstaendig entfernen. Der native `#else`-Zweig
  (`int main() { ... }`) verliert damit seinen Wirtsort; er zieht in eine neue,
  schmale native Eintrittsdatei um (z. B. `test/test_smoke/` bleibt
  Testeintritt, ein dedizierter `native`-Eintrittspunkt wird nur angelegt,
  falls der PlatformIO-`native`-Build tatsaechlich eine eigene `main()`
  ausserhalb der Testsuiten benoetigt — genaue Datei erst in der
  Umsetzungsphase anhand des tatsaechlichen `native`-Buildgraphen
  festgelegt).
- `scripts/check_platformio_config.py`: entfernen. Sein einziger Zweck
  (Plattform-/Boardpin fuer die Arduino-Envs) entfaellt mit deren Entfernung
  vollstaendig; der verbleibende IDF-Versionspin wird bereits durch den
  `FATAL_ERROR`-Guard in `CMakeLists.txt` abgedeckt (keine Doppelpflege).
- `.github/workflows/build.yml`: Schritt „PlatformIO installieren“ bleibt nur
  fuer `native` bestehen; `esp32_bringup`/`esp32_release`-Buildaufrufe werden
  durch die neuen ESP-IDF-Buildschritte ersetzt.
- Reihenfolge: Der Arduino-Pfad wird **erst entfernt, nachdem** die
  ESP-IDF-CI-Strecke (Build beider Profile, Tests, Architekturguard,
  Ressourcenbericht) bereits in mehreren gruenen CI-Laeufen bestanden hat —
  kein Big-Bang-Austausch in einem Commit (siehe Commit-Schnitt, Abschnitt 8).
- Letzter Arduino-Ressourcenstand wird vor der Entfernung als Referenzwert in
  `docs/CI_AND_QUALITY_GATES.md`/`docs/ESP_IDF_UPGRADE_CONTRACT.md`
  dokumentiert (siehe Abschnitt 4.4 fuer die bereits gesammelten Zahlen).

### 7.4 Kanonischer Hosttestpfad

**Empfehlung: PlatformIO `[env:native]` bleibt der einzige kanonische
Hosttestpfad**, unveraendert. Begruendung (KISS/DRY, siehe auch Abschnitt 15):

- 420 bestehende native Tests laufen bereits reproduzierbar, ohne
  ESP-IDF-Abhaengigkeit;
- eine Migration auf reines CMake/CTest waere ein grosser Umbau ohne
  belegten fachlichen Nutzen und mit realem Regressionsrisiko fuer die
  gesamte bestehende Testbasis;
- PlatformIO bleibt ohnehin fuer den `native`-Pfad im Repository, es entsteht
  keine zusaetzliche Toolchain-Doppelung gegenueber dem Status quo;
- es wird **keine** zweite dauerhafte Hosttestloesung eingefuehrt.

### 7.5 Ressourcenbaseline und Artefakte

- `scripts/build_report.py` wird um einen ESP-IDF-Zweig erweitert (gleiche
  Datei, gleiche Berichtsstruktur, kein Parallelskript — DRY): Parsen der
  `idf.py build`-Groessenausgabe bzw. Auswertung von
  `build/<projekt>.bin`, `size`-Ausgabe des ELF und
  `build/bootloader/bootloader.bin`; Ablage als zusaetzlicher Abschnitt in
  `build-report.md`.
- CI sichert zusaetzlich `build/<projekt>.map` und `build/<projekt>.elf` als
  Artefakt (analog zum bestehenden `platformio-build-log`/`build-report`
  -Muster in `build.yml`).
- Exakte Byte-Schwellenwerte bleiben `TBD_IMPLEMENTATION_BUDGET`; #74 liefert
  die erste dokumentierte Baseline (Abschnitt 4.4), keine CI-Abbruchgrenze.
  Das spaetere Entscheidungskriterium: reale Belastungs-/Heap-Messung auf
  Zielhardware nach Anbindung von Sensoren/Display/Aktoren.

### 7.6 Format und Static Analysis

- `clang-format`-Suchpfad in `build.yml` um `main` ergaenzen
  (`find src include lib test main ...`, bzw. nach Entfernung von `src/`
  entsprechend reduziert).
- ESP-IDF liefert `build/compile_commands.json` automatisch
  (`idf.py build`); `clang-tidy` wird fuer `main/app_main.cpp` und
  `lib/device_platform_esp_idf/src/esp_timer_time_source.cpp` mit
  `-p build` (IDF-Compile-DB) statt `-p .` (native Compile-DB) aufgerufen,
  da die native Compile-DB keine ESP-IDF-Includes kennt — dieselbe bereits
  dokumentierte, begruendete Grenze wie bei `test/` und
  `device_platform_test_support` heute.
- `.clang-tidy`s `HeaderFilterRegex` bleibt `(include|lib)/.*`; ob `main/`
  und `device_platform_esp_idf` zusaetzlich in den Regex aufgenommen werden,
  wird in der Umsetzungsphase anhand der tatsaechlichen `clang-tidy`-Ausgabe
  entschieden (kein blindes Erweitern ohne Pruefung neuer Fundstellen).
- Werkzeugversionen bleiben `clang-format-18`/`clang-tidy-18`, unveraendert
  versioniert in `build.yml`.

### 7.7 Komponenten- und Lockfilevertrag

- Kein `idf_component.yml` fuer #74: es gibt aktuell null externe
  Komponenten; eine leere Manifestdatei waere reine Scheinstruktur
  (ausdruecklich verboten laut Agentenauftrag-Vorgabe 5.11).
- `dependencies.lock` bleibt bewusst ungeschrieben, bis die erste echte
  Espressif-Komponente (z. B. Display-/Sensortreiber in einem spaeteren
  Hardware-Issue) ueber den Component Manager eingebunden wird; die
  `.gitignore`-Ausnahme dafuer ist bereits korrekt vorbereitet (nicht
  ignoriert).
- Zukuenftige Prioritaet bei Bedarf: offizielle Espressif-Komponenten
  (`espressif/*` im Component Registry) vor externen Drittkomponenten,
  jede mit fixierter Version, keine direkten Git-Downloads, keine
  unfixierten Versionsbereiche — als Grundsatz in
  `docs/ESP_IDF_UPGRADE_CONTRACT.md` verankert (Abschnitt 7.8), keine
  Umsetzung jetzt.

### 7.8 ESP-IDF-Upgradevertrag

Neues Dokument `docs/ESP_IDF_UPGRADE_CONTRACT.md`, verlinkt aus
`docs/CI_AND_QUALITY_GATES.md`, mit den in Issue #74 vorgegebenen drei
Stufen:

- **Bugfix-Upgrade:** Vollbuild beider Profile, alle nativen Tests,
  ESP-IDF-Builds, Ressourcenvergleich gegen letzte Baseline, Config-Diff
  (`sdkconfig` generiert vs. Overlays), Hardware-Smoke-Test vor Merge.
- **Minor-Upgrade:** zusaetzlich offizielle Espressif-Migrationshinweise
  pruefen, Deprecated-/Removed-API-Scan, `sdkconfig`-Aenderungsdiff,
  Komponenten-/Lockfile-Diff (sobald vorhanden), vollstaendiger
  Hardware-Paritaetstest.
- **Major-Upgrade:** immer eigenes Plan-first-Issue, Parallelbuild bis zur
  Paritaet, kein direktes Ueberschreiben der stabilen Toolchain, Adapter-/
  Portpruefung (`device_platform_esp_idf`), eigene Ressourcen- und
  Hardwarefreigabe.
- Explizit ausgeschlossen (Uebernahme aus Issue #74): private ESP-IDF-Header,
  lokaler Fork ohne Ownerentscheid, unfixierte Produktionsversion, globale
  Versionszweige im Fachkern, generische Wrapper-Schatten-API.
- Notwendige Versionszweige (aktuell: keine) werden zentral in diesem
  Dokument mit einer expliziten Entfernungsbedingung erfasst, sobald sie
  entstehen.

## 8. Geplanter kleiner PR-/Commit-Schnitt

Alle Commits im selben Draft-PR nach Planfreigabe, in dieser Reihenfolge:

1. CI: ESP-IDF-Toolchain-Installation und Build beider Profile als neuer,
   zusaetzlicher Schritt in `build.yml` (Arduino-Envs bleiben unveraendert
   bestehen; reiner Additiv-Commit, kein Risiko fuer den bestehenden gruenen
   Pfad).
2. Profil-/`sdkconfig`-Vertrag: Overlays einfuehren,
   `main/CMakeLists.txt` von hart codiertem Bring-up auf profilgesteuert
   umstellen (Abschnitt 7.2).
3. Ressourcenbericht auf ESP-IDF erweitern, CI-Artefakte (`*.map`, `*.elf`)
   sichern (Abschnitt 7.5).
4. Format-/Static-Analysis-Scope erweitern (Abschnitt 7.6).
5. Arduino-Pfad entfernen (`src/main.cpp`, `platformio.ini`-Envs,
   `scripts/check_platformio_config.py`), sobald Schritte 1–4 mehrfach gruen
   sind (Abschnitt 7.3).
6. `docs/ESP_IDF_UPGRADE_CONTRACT.md` anlegen, `docs/CI_AND_QUALITY_GATES.md`,
   `docs/ARCHITECTURE.md`, `README.md`, `CHANGELOG.md` aktualisieren.
7. Abschlussdokumentation im PR (Statuszusammenfassung); `docs/DECISIONS.md`
   nur, falls die offene Entscheidung 9.1 vom Owner freigegeben wird.

Issue #71 wird **nicht** in diesem PR geschlossen, sondern erst danach,
analog zum bereits etablierten Muster bei #72/#73.

## 9. Offene Entscheidungen

1. **ADR-001-Statuspflege:** `docs/DECISIONS.md` dokumentiert aktuell nur
   ADR-001 ("PlatformIO mit Arduino Framework", `accepted`). Der
   Owner-Entscheid zum Wechsel steht bereits in Issue #71, aber nicht als
   Registereintrag. Vorschlag: ADR-001-Status auf `superseded` mit Verweis
   auf Issue #71 setzen, **keine** inhaltlich neue ADR verfassen (die
   Architekturentscheidung selbst ist bereits durch #71/ADR-013 getroffen).
   `FINAL_SELECTION_PENDING`: Owner bestaetigt, ob diese Statuspflege Teil
   von #74 ist oder gesondert freigegeben werden muss.
2. **`AGENTS.md`-Verweis auf den neuen Upgradevertrag:** rein additive
   Verlinkung in der Liste "Zentrale Einstiege". `FINAL_SELECTION_PENDING`:
   Owner entscheidet, ob dies im selben PR erlaubt ist oder separat erfolgt.
3. **CI-Caching-Design** fuer die ESP-IDF-Toolchain (`actions/cache`-Key,
   Groesse, Invalidierung): `EVALUATE_BEFORE_RELEASE`, wird empirisch beim
   ersten funktionierenden ESP-IDF-CI-Job festgelegt, nicht vorab spekuliert.
4. **Native Eintrittsdatei nach Entfernung von `src/main.cpp`:** exakter
   Dateiname/-ort fuer den verbleibenden nativen `#else`-Zweig:
   `FINAL_SELECTION_PENDING`, abhaengig vom tatsaechlichen `native`-
   Buildgraphen zum Umsetzungszeitpunkt. Gekoppelte Randbedingung:
   `scripts/build_report.py` liest den nativen Groesseneintrag heute aus
   `.pio/build/native/program`, dem Linkartefakt genau dieser
   `src/main.cpp`-`#else`-Eintrittsdatei. Die neue native Eintrittsdatei muss
   denselben Artefaktnamen/-pfad erzeugen, oder `build_report.py` muss im
   selben Commit, der `src/main.cpp` entfernt, entsprechend angepasst werden
   — sonst bricht Zeile "Host-Testbinaer" im Ressourcenbericht (Abschnitt
   4.4) kommentarlos weg.
5. **`sdkconfig`-Drift-Pruefung:** genaues Skript/genaue Erweiterung
   (Abschnitt 7.2) wird in der Umsetzungsphase am tatsaechlich generierten
   `sdkconfig` festgelegt, nicht spekulativ vorab spezifiziert.
6. Byte-Budget-Schwellenwerte bleiben `TBD_IMPLEMENTATION_BUDGET`.

## 10. Ausdruecklich verbotene Vorwegnahmen

- keine Pin-, GPIO-, Board-Revisions- oder Partitionsentscheidung;
- keine neue externe `idf_component.yml`-Abhaengigkeit;
- kein Wechsel des kanonischen Hosttestpfads;
- keine harten CI-Abbruch-Schwellenwerte fuer Ressourcen;
- keine ADR-Erstellung/-Aenderung und keine `AGENTS.md`-Aenderung ohne
  gesonderte Freigabe der offenen Entscheidungen aus Abschnitt 9;
- kein Docker-basierter CI-Pfad ohne erneute Owner-Bestaetigung der
  Empfehlung aus Abschnitt 7.1;
- keine reale Aktorfreigabe in irgendeinem Profil.

## 11. Daten-, Zustands- und Schnittstellenvertraege

Keine Aenderung an `ITimeSource`, `PlatformStartupContext`,
`ProfilePolicy`, Wireformaten oder #57-Vertraegen. `include/app_config.hpp`
selbst (Exklusivitaets-`#error`-Vertrag, `ProfilePolicy`-Structs,
`hasSafeDefaults()`, alle `static_assert`) wird inhaltlich **nicht**
angefasst (verifiziert durch Lektuere der Datei, Abschnitt 7.2). Die
Profilsteuerung aendert nur, **wie** `main/CMakeLists.txt` genau eine der
drei bestehenden `APP_PROFILE_*`-Definitionen setzt (Build-/Overlay-
Mechanismus), nicht die dadurch erzeugte Semantik (`HARDWARE_UNVERIFIED`,
`LOCKED_FOR_BRINGUP`/`REQUIRE_VERIFIED_HARDWARE`, reale Aktoren deaktiviert
bleiben in beiden Profilen unveraendert Pflicht).

## 12. Fehler-, Recovery-, Security- und Safetygrenzen

- reale Aktoren bleiben in `esp32_bringup` **und** `esp32_release`
  deaktiviert (bestaetigt: aktuell hart codiert, bleibt es auch nach
  Umstellung auf Overlays, siehe Abschnitt 7.2);
- CI-Workflow-`permissions` bleiben minimal (`contents: read`), keine
  Ausweitung fuer den neuen ESP-IDF-Schritt noetig;
- keine Secrets in Workflow, Cache oder Artefakten (`scripts/check_secrets.py`
  bleibt unveraendert wirksam, deckt auch neue Dateien ab);
- keine unfixierten Actions: jede neu eingefuehrte Action (falls offene
  Entscheidung 9.3 / CI-Caching-Design doch eine ergibt) wird auf
  Commit-SHA gepinnt, kein `@vN`-Alias;
- fehlgeschlagene ESP-IDF-Builds liefern Log-Artefakte, analog zum
  bestehenden `platformio-build-log`-Muster;
- Caches taeuschen keine Buildkorrektheit vor: ein Cache-Treffer ersetzt nie
  den tatsaechlichen `idf.py build`-Erfolg als Gate.

## 13. Teststrategie

- bestehende 420 native Tests bleiben unveraendert massgeblich
  (`pio test -e native`);
- `idf.py build` fuer beide Profile wird neuer CI-Pflichtschritt
  (Kompilieroffenheit, kein Laufzeittest ohne Hardware);
- `scripts/check_architecture_boundaries.py --selftest` bleibt Pflichtschritt,
  unveraendert;
- `scripts/selftest_quality_gates.py` wird um die neuen/geaenderten
  Pruefungen (Format-Scope, Ressourcenbericht) ergaenzt, sobald diese in der
  Umsetzung konkret vorliegen (kein Fixture-Fall wird vorab spekulativ
  angelegt).

## 14. Dokumentationsaenderungen (Umsetzungsphase)

- `README.md`: Arduino-/`espressif32@7.0.1`-Referenzen (u. a. Zeilen ~96,
  ~175) durch den ESP-IDF-6.0.2-Pfad ersetzen;
- `docs/ARCHITECTURE.md`: Profiltabelle und Buildwege aktualisieren;
- `docs/CI_AND_QUALITY_GATES.md`: ESP-IDF-Abschnitt von "lokale
  Entwicklerpflicht, CI folgt in #74" auf "CI-gebunden" umschreiben;
  PlatformIO-Abschnitt auf `native` reduzieren;
- `CHANGELOG.md`: Eintrag nach Abschluss;
- `docs/DECISIONS.md`: siehe offene Entscheidung 9.1.

## 15. Bewertung gegen SOLID, DRY, KISS

- **SRP:** Workflow-Datei, Buildkonfiguration (`CMakeLists.txt`/Overlays),
  Ressourcenbericht (`build_report.py`) und Upgradevertrag
  (`ESP_IDF_UPGRADE_CONTRACT.md`) bleiben vier getrennte Dateien mit je einer
  Verantwortung, keine Vermischung.
- **OCP:** `device_platform`, `fermentation_app`, `device_platform_esp_idf`
  werden inhaltlich nicht angefasst; die Migration wirkt ausschliesslich auf
  Build-/CI-/Doku-Ebene.
- **LSP:** keine Portschnittstelle aendert sich; `EspTimerTimeSource`
  bleibt einzige `ITimeSource`-Implementierung fuer ESP-IDF, unveraendert.
- **ISP:** kein universeller Toolchain-Wrapper; die empfohlene Variante 4
  (Abschnitt 7.1) vermeidet bewusst eine zusaetzliche abstrahierende
  Action-API.
- **DIP:** Fachkern bleibt unabhaengig von ESP-IDF und CI;
  `check_architecture_boundaries.py` sichert das automatisiert weiterhin ab.
- **DRY:** genau eine Quelle je Belang — ein Ressourcenberichtsskript
  (erweitert, nicht dupliziert), ein Hosttestpfad, ein Upgradevertrag-
  Dokument, ein Installationsablauf (identisch lokal und in CI).
- **KISS:** bestehender funktionierender `native`-Hosttestpfad bleibt
  unveraendert; kein Docker, keine zusaetzliche Wrapper-Action, kein
  spekulatives `idf_component.yml`; die Umstellung erfolgt in kleinen,
  einzeln gruenen Schritten statt eines Grossumbaus.

Bewusste Abweichung: Keine.

## 16. Abnahmekriterien

Deckungsgleich mit Issue #74:

- ESP-IDF 6.0.2 ist der einzige ESP32-Produktionsbuild in CI;
- alte Arduino-Produktionsprofile und `src/main.cpp` sind entfernt;
- native Fachtests besitzen genau einen kanonischen Hostpfad
  (PlatformIO `native`);
- CI-, Architektur-, Format-, Static-Analysis-, Secret- und
  Ressourcenchecks sind fuer beide ESP-IDF-Profile gruen;
- Upgradepolitik, Versionspin, Komponentenlockvertrag und
  Profilkonfiguration sind reproduzierbar in
  `docs/ESP_IDF_UPGRADE_CONTRACT.md` und `docs/CI_AND_QUALITY_GATES.md`
  dokumentiert;
- keine IDF-Typen sind in `fermentation_app` oder stabile Ports eingedrungen
  (automatisiert durch `check_architecture_boundaries.py` geprueft);
- Issue #71 kann im Anschluss geschlossen werden.

Plan-spezifisch zusaetzlich:

- genau ein Plan-Commit in dieser Planungsphase;
- Draft-PR mit Plan-Datei, Plan-Commit-SHA, offenen Entscheidungen und
  Status `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`;
- vollstaendiges Anhalten bis zu einem commitgebundenen
  `PLAN APPROVED`-Ownerkommentar.
