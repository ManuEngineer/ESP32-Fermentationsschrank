# Plan: Issue #74 – CI, Ressourcenbaseline und ESP-IDF-Upgradevertrag

Status: Planungsphase (`IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`)

Issue: `#74`
Tracking: `#71`
Basis-`main`-SHA: `c3f8044be0f081822ca8724f67fa99e9614d57ef` (Merge-Commit von PR #78)

Diese Fassung überarbeitet den ursprünglichen Plan-Commit
`05b987e3d2b375b82922990f718d0dc07c730a71` nach einem vollständigen Review
(`PLAN_REVIEW: CHANGES_REQUIRED`) auf PR #79, die erste Überarbeitung
(Plan-Commit `bbccd74d49b7fcb7c2c529054da5dcd2d8e9a754`) nach einer zweiten
Reviewrunde (`PLAN_REVIEW: CHANGES_REQUIRED`), die zweite Überarbeitung
(Plan-Commit `6c8092755dde1fe0b39299abe94a0b3e02003beb`) nach einer
dritten Reviewrunde (`PLAN_REVIEW: CHANGES_REQUIRED`) sowie die dritte
Überarbeitung (Plan-Commit `806abbfd7400412285c1f83e94d80cc6c5a7bf31`)
nach einer vierten, abschliessenden Reviewrunde
(`PLAN_REVIEW: CHANGES_REQUIRED`). Alle vier vorherigen Plan-Commits
bleiben in der Historie und sind überholt; diese Fassung ersetzt sie
inhaltlich.

**Nach `PLAN APPROVED` auf Commit `7440d0964b94f06857a4e689f62e134f0da55931`
und nach freigegebener, verifizierter Umsetzung von Commit 1–3
(`COMMIT_1_REVIEW: PASS`, `COMMIT_2_REVIEW: PASS`, `COMMIT_3_REVIEW: PASS`,
zuletzt verifizierter Head `40bf011918555e397ec02d5ccb0e14cc57b99a5d`):**
Waehrend der Umsetzung von Commit 4 wurde festgestellt, dass der in
Abschnitt 7.8 fuer die ESP-IDF-spezifische statische Analyse woertlich
festgelegte Befehl nicht funktioniert (Xtensa-Backend-Luecke in
Standard-`clang-tidy-18`). Plan-Commit
`9607fbafc283dfb89623043f10dbff43780bc148` dokumentierte diesen Befund
mit drei offenen, nicht vorentschiedenen Optionen und war **nicht**
freigabefaehig. Die erste Ueberarbeitung (Plan-Commit
`72700d76aec8e0a4fb5a0e78bb17d3ebcaa2ad53`) setzte die verbindliche
Ownerentscheidung fuer den offiziellen `esp-clang`/`idf.py clang-check`-
Pfad um (Abschnitt 7.8.1) und dokumentierte zwei daraus live verifizierte,
zusaetzliche offene Punkte (Befund A, Befund B), war aber selbst nach dem
Reviewauftrag "PR #79 – Commit-4-Plan: letzte verbindliche Korrekturen"
noch **nicht** freigabefaehig: Befund A war fälschlich als drei offene,
nicht vorentschiedene Optionen dargestellt statt als reines
Makroexpansionsartefakt entschieden, die `pyclang`-Version war nur
protokolliert statt fail-closed erzwungen, die Werkzeugverifikation im
geplanten Analysetreiber war zu schwach ("enthaelt Espressif" statt
exakter Pfad-/Versionsabgleich), die PATTERNS-Dateiauswahl war nicht
gegen die real reproduzierte "0 von 28 Dateien"-Falle abgesichert, und
mehrere abhaengige Abschnitte (5, 6, 8, 9, 10, 13, die Liste ueberholter
Plan-Commits) waren inkonsistent. Diese Fassung schliesst alle diese
Punkte. Sie besteht aus den zwei Plan-Commits
`d1a1ec21f7463dc274f7bd9f03d1b46f00a655c4` (Hauptkorrektur) und
`bf9981b2676a0b7662713a4d8968b68274a075c5` (Selbstreview-Nachkorrektur
zweier verbliebener Widersprueche in Abschnitt 7.11 und der
7.8-Einleitung).

**Nach dem Reviewauftrag "PR #79 – Commit-4-Plan: Abschlusskorrektur vor
Freigabe"** (`COMMIT_4_PLAN_REVIEW: CHANGES_REQUIRED` auf Plan-Commit
`bf9981b2676a0b7662713a4d8968b68274a075c5`): Dieser Reviewauftrag
bestaetigte alle vorherigen Entscheidungen (offizieller `esp-clang`-Pfad,
`IgnoreMacros: 'true'` fuer Befund A, `main/app_main.cpp` unveraendert,
`pyclang` fail-closed) und deckte drei verbleibende Blocker auf: (1) die
zwischenzeitlich verlangte Forderung, `clang-tidy --version` muesse den
vollstaendigen `esp-clang`-Toolpaketnamen enthalten, widersprach dem im
selben Plan bereits dokumentierten realen Output (`clang-tidy --version`
meldet nachweislich nur die LLVM-Version, nicht den Toolpaketnamen); (2)
die verlangte Pruefung einer "post-filter", per PATTERNS gefilterten
`compile_commands.json` existiert real nicht — `pyclang`s eigener
Dateifilter arbeitet nach Projektwurzel-Zugehoerigkeit, nicht nach
PATTERNS, und die eigentliche PATTERNS-Auswahl geschieht ausschliesslich
in-memory in `run-clang-tidy`, ohne je auf Platte geschrieben zu werden
(live am Quelltext beider Werkzeuge sowie an realen Testlaeufen
verifiziert, Abschnitt 7.8.1 Detailbefund 4, Abschnitt 7.8.4); (3) die
Abnahmekriterien behaupteten faelschlich "genau ein
Plan-Korrekturcommit", obwohl diese Ueberarbeitung bereits aus zwei
Commits bestand. Diese Fassung korrigiert alle drei Punkte anhand realer
Verifikation gegen die lokale ESP-IDF-6.0.2-Installation (Abschnitt
7.8.7) und dokumentiert an mehreren Stellen, wo die urspruenglich vom
Reviewauftrag vorgeschlagene Loesung an der realen Werkzeugarchitektur
scheiterte und durch eine funktional gleichwertige, tatsaechlich
implementierbare Alternative ersetzt wurde. Plan-Commits
`9607fbafc283dfb89623043f10dbff43780bc148`,
`72700d76aec8e0a4fb5a0e78bb17d3ebcaa2ad53`,
`d1a1ec21f7463dc274f7bd9f03d1b46f00a655c4` **und**
`bf9981b2676a0b7662713a4d8968b68274a075c5` sind damit **ueberholt**;
diese Fassung ersetzt alle vier fuer Abschnitt 7.8 vollstaendig.
Abschnitte 1–7.7, 7.9–7.13 sowie die zugehoerige `PLAN APPROVED`-Freigabe
auf Commit `7440d0964b94f06857a4e689f62e134f0da55931` bleiben unveraendert
in Kraft; nur Abschnitt 7.8 und die davon abhaengigen Abschnitte 5, 6, 8,
9, 10, 12, 13 wurden in dieser Fassung angepasst.

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
  `6c8092755dde1fe0b39299abe94a0b3e02003beb`): verbindliche Grundlage der
  dritten Überarbeitung, insbesondere fuer den zweiphasigen
  Uebergangsvertrag von `scripts/check_build_profiles.py`, die
  Unveraenderlichkeit gepinnter Action-SHAs, den dokumentierten
  Lizenzkonflikt der ESP-IDF-Installations-Action, den vollstaendigen
  Textartefakt-Scan und den exakten Zeitpunkt der Hardware-Smoke-Tests;
- **Reviewauftrag "PR #79 – abschliessende Korrektur vor Planfreigabe"**
  (`PLAN_REVIEW: CHANGES_REQUIRED` auf Plan-Commit
  `806abbfd7400412285c1f83e94d80cc6c5a7bf31`): verbindliche Grundlage
  dieser Fassung. Deckte insbesondere einen sachlichen Fehler auf: die
  vorherige Fassung begruendete die Wahl von `eim-version: "v0.1.7"` mit
  dem Status von `espressif/idf-im-cli` (archiviert), obwohl der
  tatsaechliche Code der gepinnten Action seine EIM-Binaries live
  verifiziert aus `espressif/idf-im-ui` laedt — einem **aktiven**
  Repository mit haeufigen 2026er-Releases. Fuehrte zur vollstaendigen
  Abkehr von `espressif/install-esp-idf-action` zugunsten einer direkten
  offiziellen ESP-IDF-Installation (Abschnitt 7.1), zum zweiphasigen
  Uebergangsvertrag fuer `scripts/build_report.py` (Abschnitt 7.7.5), zur
  Bereinigung einer sprachlich widerspruechlichen Hardwaretest-Formulierung
  (Abschnitt 7.12), zur vollstaendigen Liste ueberholter Plan-Commits und
  zu einer praeziseren Beschreibung der `check_secrets.py`-Erweiterung;
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
  `idf.py size --format json2 --output-file`);
- **Reviewauftrag "PR #79 / Commit 4: ESP-IDF-Static-Analysis-Plan
  korrigieren"** (`COMMIT_4_PLAN_REVIEW: CHANGES_REQUIRED`,
  `OWNER_DECISION: USE_OFFICIAL_ESP_CLANG_PATH` auf Plan-Commit
  `9607fbafc283dfb89623043f10dbff43780bc148`, nach `PLAN APPROVED` und
  freigegebenen Commits 1–3): verbindliche Grundlage dieser
  Ueberarbeitung von Abschnitt 7.8 — legt die Ownerentscheidung fuer den
  offiziellen `esp-clang`/`idf.py clang-check`-Pfad fest, korrigiert die
  `pyclang`-Aussage aus Plan-Commit `9607fba`, verlangt die strikte
  Trennung von Produktions- und Analysebuild, einen kanonischen
  Analysetreiber sowie eine live verifizierte, exakte Kommandoform vor
  Freigabe; offizielle ESP-IDF-6.0.2-Dokumentation
  (`docs/en/api-guides/tools/idf-clang-tidy.rst`) und der installierte
  `pyclang`-Quelltext wurden fuer diese Ueberarbeitung read-only
  verifiziert;
- **Reviewauftrag "PR #79 – Commit-4-Plan: letzte verbindliche
  Korrekturen"** (`COMMIT_4_PLAN_REVIEW: CHANGES_REQUIRED`,
  `OWNER_DECISION_ESP_CLANG: CONFIRMED`,
  `OWNER_DECISION_COGNITIVE_COMPLEXITY: IGNORE_MACRO_EXPANSIONS` auf
  Plan-Commit `72700d76aec8e0a4fb5a0e78bb17d3ebcaa2ad53`): verbindliche
  Grundlage dieser Fassung — entscheidet Befund A abschliessend als
  Makroexpansionsartefakt (`readability-function-cognitive-complexity.
  IgnoreMacros: 'true'`, `main/app_main.cpp` bleibt unveraendert), macht
  die `pyclang`-Version zu einem fail-closed pruefbaren Vertrag, verlangt
  exakte Pfad-/Versionsverifikation fuer `esp-clang` **und** `pyclang` im
  Analysetreiber, verlangt eine `re.escape()`-verankerte PATTERNS-Regex
  mit positiver Nachweisfuehrung ueber die tatsaechlich ausgewaehlten
  Dateien, verlangt eine reale Nachweisfuehrung, dass `.clang-tidy` die
  alleinige Konfigurationsquelle bleibt, verlangt das Sichern von
  `warnings.txt` auch bei einem fehlgeschlagenen Analyselauf, erweitert
  die Selftestliste und die vollstaendige Liste ueberholter Plan-Commits,
  und verlangt einen im Plan dokumentierten, real durchgefuehrten
  Read-only-Verifikationsnachweis vor dieser Freigabe (Abschnitt 7.8.7);
- **Reviewauftrag "PR #79 – Commit-4-Plan: Abschlusskorrektur vor
  Freigabe"** (`COMMIT_4_PLAN_REVIEW: CHANGES_REQUIRED` auf Plan-Commit
  `bf9981b2676a0b7662713a4d8968b68274a075c5`): verbindliche Grundlage
  dieser Fassung — bestaetigt alle vorherigen Entscheidungen, deckt aber
  drei Blocker auf: eine nicht erfuellbare Forderung an `clang-tidy
  --version` (widerspricht dem im selben Plan bereits dokumentierten
  realen Output, korrigiert in Abschnitt 7.8.1/7.8.4 durch mehrere
  unabhaengige, zueinander passende Signale statt eines einzelnen, real
  nicht gelieferten Strings), eine real nicht existierende „post-filter
  compile_commands.json"-Pruefmethode (korrigiert in Abschnitt 7.8.4
  durch einen eigenen, unabhaengigen Dateiauswahlnachweis gegen die
  vollstaendige, ungefilterte Compile-Datenbank) und eine falsche
  Behauptung ueber die Anzahl der Plan-Korrekturcommits (korrigiert in
  Abschnitt 16, Plan-spezifisch).

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
- `scripts/build_report.py` (Commit 3: Parallelbericht um ESP-IDF-
  Groessendaten aus `idf.py size --format json2` erweitert, Arduino-
  Messungen bleiben; Commit 5: auf `native` + ESP-IDF-only umgestellt, im
  selben Skript, Abschnitt 7.7.5)
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
  erweitert, `CheckOptions:
  readability-function-cognitive-complexity.IgnoreMacros: 'true'`
  ergaenzt, Abschnitt 7.8)
- neu: `scripts/run_esp_idf_static_analysis.py` (kanonischer
  ESP-IDF-`esp-clang`-Analysetreiber, Abschnitt 7.8.4)
- `scripts/esp_idf_contract.py` (Erweiterung um die neuen
  Werkzeugvertragskonstanten `ESP_CLANG_TOOL_VERSION`,
  `ESP_CLANG_LLVM_VERSION`, `ESP_CLANG_LINUX_AMD64_SHA256`,
  `PYCLANG_VERSION` — bestehende Konstanten/Namenshelfer bleiben
  unveraendert, DRY-Quelle fuer beide Analysetreiber, Abschnitt 7.8.4)

Kein produktiver Fachcode (`lib/device_platform/`, `lib/fermentation_app/`,
`startApplication()`/nativer Kern in `src/main.cpp`, `main/app_main.cpp`)
wird inhaltlich veraendert. Befund A (Abschnitt 7.8.2) ist entschieden:
die Loesung liegt ausschliesslich in `.clang-tidy`
(`IgnoreMacros: 'true'`), `main/app_main.cpp` bleibt **ohne jede
Aenderung** — kein Refactoring, kein `NOLINT`, keine sonstige
Modifikation. `docs/audits/` (historische Auditdateien, u. a.
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
  ein vollstaendiger GitHub-Actions-Lauf bestanden hat mit:

  ```text
  beide ESP-IDF-GCC-Produktionsprofile: PASS
  ESP-IDF esp32_bringup static analysis with official esp-clang: PASS
  ESP-IDF esp32_release static analysis with official esp-clang: PASS
  clang-format including main/: PASS
  native static analysis (clang-tidy 18): PASS
  der alte Arduino-Pfad unveraendert weiterhin gruen (Parallelvertrag
    aus Abschnitt 7.5.2, Phase 1)
  Parallel-Ressourcenbericht mit beiden Arduino- und beiden ESP-IDF-
    Messungen gruen (Abschnitt 7.7.5, Phase 1)
  native Tests: PASS
  Ressourcen-, Architektur-, Secret- und Quality-Gate-Selftests: PASS
  ```

  Gate 2 wird **nicht** um die ESP-IDF-Static-Analysis erleichtert; die
  „ESP-IDF ... static analysis"-Zeilen sind ab Commit 4 verbindlicher
  Bestandteil des Gates, nicht optional. Befund A (Abschnitt 7.8.2) ist
  entschieden (`readability-function-cognitive-complexity.IgnoreMacros:
  'true'` in `.clang-tidy`, `main/app_main.cpp` unveraendert); diese
  beiden Zeilen koennen damit in der Umsetzung wahrheitsgemaess `PASS`
  melden, sobald der in Abschnitt 7.8.4 spezifizierte Analysetreiber
  implementiert ist. Kein gleichzeitiger Austausch von CI-Einfuehrung und
  Arduino-Entfernung in einem Commit.
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

**Verbindliche Neuentscheidung aus der vierten Reviewrunde (ersetzt die
Empfehlung "espressif/install-esp-idf-action" aus der ersten und zweiten
Reviewrunde vollstaendig):** primaere und einzige Loesung ist die
**direkte offizielle ESP-IDF-Installation** (Git-Checkout des exakten
Commits plus offizielle `install.sh`/`export.sh`-Skripte aus dem
ESP-IDF-Repository selbst). `espressif/install-esp-idf-action` und der
davon verwendete ESP-IDF Installation Manager (EIM) werden im aktiven
CI-Pfad von #74 **nicht** eingesetzt.

**Befund, der zu dieser Neuentscheidung fuehrt (live verifiziert am
Code des zuvor gepinnten Commits
`8fc05d1470d5591417e7a3707a1f2bec178db4ae`):** Die vorherige Planfassung
begruendete die Wahl von `eim-version: "v0.1.7"` damit, dass
`espressif/idf-im-cli` archiviert und `v0.1.7` dessen letzte Version sei.
Diese Begruendung war **sachlich falsch**: Der tatsaechliche
Action-Code (`index.js`) erzeugt seine Download-URL aus
`https://github.com/espressif/idf-im-ui/releases/download/<version>/...`
— die EIM-Binaries stammen aus `espressif/idf-im-ui`, nicht aus dem
archivierten `espressif/idf-im-cli`. `espressif/idf-im-ui` ist
**nicht archiviert** (`archived: false`, verifiziert), `pushed_at` liegt
zum Recherchezeitpunkt dieser Planungsphase auf dem Tag der Recherche
selbst, und die letzte Release-Version zu diesem Zeitpunkt ist `v0.17.3`
— nicht `v0.1.7`. Der zuvor gepinnte Wert `eim-version: "v0.1.7"` bezog
sich damit auf die Versionsnummerierung des falschen, archivierten
Repositorys und haette in der tatsaechlich verwendeten Aufloesung
(`idf-im-ui`) vermutlich gar keine gueltige Version bezeichnet. Die
Kombination aus Action-Commit, diesem `eim-version`-Wert, ESP-IDF `v6.0.2`
und `ubuntu-24.04` war zudem im Plan nie tatsaechlich als funktionierende
Kette nachgewiesen, sondern nur behauptet. Diese Korrektur uebernimmt
damit keinen weiteren, in dieser Planungsphase noch nicht verifizierten
Versionswert fuer `idf-im-ui`, sondern verzichtet stattdessen ganz auf die
Action und den davon abhaengigen EIM-Installationspfad.

**Verbindliche Struktur** (Repository, Tag und Commit exakt, kein
Platzhalter):

```text
Repository: espressif/esp-idf
Tag: v6.0.2
Commit: 7101770dc6db2667b3c477cc31365dd1acd6db4e
```

Vorgesehener Ablauf, sinngemaess (exakte, von ESP-IDF 6.0.2 tatsaechlich
unterstuetzte Syntax wird vor dem Implementierungscommit — Commit 2 —
anhand der offiziellen ESP-IDF-Dokumentation und der bereits vorhandenen
lokalen Installation aus #72/#73 read-only verifiziert; dies ist eine
technische Detailkorrektur ohne Vertrags-/Safetywirkung, keine materielle
Planabweichung, solange Repository, Tag und Commit unveraendert bleiben):

```bash
export IDF_PATH="$RUNNER_TEMP/esp-idf-v6.0.2"
export IDF_TOOLS_PATH="$RUNNER_TEMP/espressif"

git clone --filter=blob:none --no-checkout \
  https://github.com/espressif/esp-idf.git \
  "$IDF_PATH"

git -C "$IDF_PATH" fetch --depth 1 origin \
  7101770dc6db2667b3c477cc31365dd1acd6db4e

git -C "$IDF_PATH" checkout --detach \
  7101770dc6db2667b3c477cc31365dd1acd6db4e

git -C "$IDF_PATH" submodule update --init --recursive

git -C "$IDF_PATH" describe --tags --exact-match
git -C "$IDF_PATH" rev-parse HEAD
git -C "$IDF_PATH" status --short

"$IDF_PATH/install.sh" esp32
. "$IDF_PATH/export.sh"
idf.py --version
```

**Reproduzierbarkeitsvertrag:** Der CI-Schritt bricht ab, wenn

- der ausgecheckte Commit nicht exakt
  `7101770dc6db2667b3c477cc31365dd1acd6db4e` ist;
- der Tag nicht exakt `v6.0.2` ist;
- der ESP-IDF-Arbeitsbaum nicht sauber ist (`git status --short`
  nicht leer);
- `idf.py --version` nicht ESP-IDF 6.0.2 meldet;
- benoetigte Submodule fehlen;
- die offizielle Installation (`install.sh`) fehlschlaegt.

Keine automatische Aktualisierung; keine Installation ueber einen
gleitenden Branch oder ein gleitendes Release. Dieser Vertrag ersetzt den
bisherigen `IDF_VER`-CMake-Guard nicht, sondern ergaenzt ihn um eine
fruehere, CI-seitige Herkunftspruefung.

**Actions:** Verbindlich bleiben ausschliesslich die folgenden drei,
bereits in Abschnitt 7.11 gelisteten Pins (unveraendert, weiterhin ohne
stillen Wechsel):

```text
actions/checkout@d23441a48e516b6c34aea4fa41551a30e30af803
actions/setup-python@ece7cb06caefa5fff74198d8649806c4678c61a1
actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02
```

Der Pin `espressif/install-esp-idf-action@8fc05d1470d5591417e7a3707a1f2bec178db4ae`
ist **vollstaendig entfernt** — aus der aktiven Loesung, der Action-Liste
in Abschnitt 7.11 und den Abnahmekriterien (Abschnitt 16).

**Lizenzdokumentation der verworfenen Variante (bleibt als Begruendung
erhalten, ist aber kein aktiver Abhaengigkeits- oder Lieferkettenvertrag
des Projekts mehr):** Der in der dritten Reviewrunde festgestellte
`LICENSE_METADATA_CONFLICT` von `espressif/install-esp-idf-action`
(Root-`LICENSE`: Apache-2.0; `README.md`/`package.json`: MIT; gebuendelte
MIT-Drittanbieter-Notices in `dist/licenses.txt`) bleibt im Plan als
zusaetzliche, unterstuetzende Begruendung fuer die Nichtverwendung
dokumentiert — ausschlaggebend fuer die Neuentscheidung ist jedoch der
oben beschriebene sachliche Fehler zur EIM-Herkunft, nicht der
Lizenzkonflikt allein.

**Aktive CI-Toolchain-Lizenz:** ESP-IDF `v6.0.2`, mit der Lizenz und den
Third-Party-Hinweisen des offiziellen `espressif/esp-idf`-Repositories
(bereits Grundlage von #72/#73, keine neue Lizenzpruefung noetig).

**Fallback-Regel** (uebernommen, jetzt bezogen auf die direkte
Installation statt auf einen Action-Pin): Stellt sich in der Umsetzung
heraus, dass der exakte Commit `7101770dc6db2667b3c477cc31365dd1acd6db4e`
auf `ubuntu-24.04` nicht reproduzierbar installierbar ist oder die
offiziellen Installationsskripte in ihrer zu verifizierenden Syntax nicht
wie hier skizziert funktionieren, haelt der Agent an, aktualisiert die
Plan-Datei, erstellt einen neuen Plan-Commit mit Begruendung und wartet
erneut auf `PLAN APPROVED`. Kein eigenmaechtiger Ruecksprung auf
`espressif/install-esp-idf-action` oder eine andere Installationsvariante
ausserhalb dieses Ablaufs.

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
einer stillen Zeile "kein Groessenartefakt gefunden". Diese Erweiterung ist
Phase 1 eines zweiphasigen Uebergangs desselben Skripts — siehe Abschnitt
7.7.5 fuer den vollstaendigen Vertrag inklusive der Phase-2-Umstellung in
Commit 5.

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
siehe Abschnitt 7.7.4 fuer die tatsaechliche Pruefung durch den dort
beschriebenen, neuen Scan-Modus von `scripts/check_secrets.py` (die
bestehende Pruefung getrackter Dateien allein deckt generierte
Artefaktdateien nicht ab).

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

#### 7.7.5 Uebergangsvertrag fuer `scripts/build_report.py` (neuer Punkt
aus vierter Reviewrunde)

**Befund:** `scripts/build_report.py` hat aktuell
`DEFAULT_ENVIRONMENTS = ("native", "esp32_bringup", "esp32_release")`
(live verifiziert) und baut diese drei Profile ausschliesslich als
PlatformIO-Umgebungen. Abschnitt 7.7.1 erweitert das Skript in Commit 3 um
die ESP-IDF-JSON2-Auswertung, ohne dass die vorherige Planfassung fuer
Commit 5 (Entfernung von `[env:esp32_bringup]`/`[env:esp32_release]` aus
`platformio.ini`, Abschnitt 7.5) eine notwendige Anpassung dieses Skripts
nannte. Ohne eine solche Anpassung wuerde der finale CI-Pfad weiterhin
versuchen, die beiden entfernten PlatformIO-Environments zu bauen, und
zwangslaeufig fehlschlagen.

**Verbindliche Korrektur:** Dasselbe `scripts/build_report.py` durchlaeuft
denselben zweiphasigen Uebergang wie `scripts/check_build_profiles.py`
(Abschnitt 7.5.2) — kein Parallelskript, kein zweites Werkzeug.

**Phase 1 — Commit 3 (Parallelbericht):** Das Skript erzeugt einen
gemeinsamen Bericht mit klar unterschiedenen Ueberschriften fuer Arduino-
und ESP-IDF-Messungen, damit beide Wertereihen nicht unter derselben
Bezeichnung vermischt werden, z. B.:

```text
Arduino/PlatformIO esp32_bringup   (letzter Arduino-Vergleichspfad)
Arduino/PlatformIO esp32_release   (letzter Arduino-Vergleichspfad)
ESP-IDF esp32_bringup              (JSON2-basierte IDF-Messung)
ESP-IDF esp32_release              (JSON2-basierte IDF-Messung)
Native Host
```

Der letzte Arduino-Ressourcenvergleichsstand (Abschnitt 4.4, Abschnitt
7.5.3) bleibt damit im Bericht selbst nachvollziehbar dokumentiert, nicht
nur im Fliesstext des Plans.

**Phase 2 — Commit 5 (finaler Zustand):** Im selben Commit, der den
Arduino-Pfad entfernt, wird `scripts/build_report.py` umgestellt:

- PlatformIO-Teil baut nur noch `native`;
- ESP-IDF-Teil liest weiterhin `build/esp32_bringup/size.json` und
  `build/esp32_release/size.json` (unveraendert gegenueber Phase 1);
- das Skript ruft **keine** entfernten PlatformIO-Environments mehr auf;
- `DEFAULT_ENVIRONMENTS`, CLI-Hilfe und zugehoerige Tests werden auf den
  finalen Zustand angepasst.

**Selftests:** Parallelbericht (Phase 1) enthaelt beide Arduino- und beide
ESP-IDF-Messungen unter eindeutig unterschiedenen Ueberschriften; finaler
Bericht (Phase 2) ruft ausschliesslich PlatformIO `native` auf und enthaelt
weiterhin beide ESP-IDF-Profile; ein fehlendes ESP-IDF-Profil fuehrt in
beiden Phasen zu einem Fehler; ein entferntes Arduino-Environment wird im
finalen Modus nicht mehr aufgerufen; Arduino- und ESP-IDF-Werte werden zu
keinem Zeitpunkt unter derselben Ueberschrift vermischt.

**Commit-Zuordnung:** Commit 3 fuehrt die Parallelbericht-Erweiterung ein;
Commit 5 stellt im selben Commit, der den Arduino-Pfad entfernt, auf den
finalen Zustand um (Abschnitt 8). Gate 2
(`PRE_ARDUINO_REMOVAL_CI: PASS`, Abschnitt 6) verlangt damit implizit auch
einen gruenen Parallelbericht auf dem Head von Commit 4.

### 7.8 Format und Static Analysis

- `clang-format`-Suchpfad in `build.yml` um `main` ergaenzt (`find src
  include lib test main ...`). Lokal verifiziert: `clang-format-18
  --dry-run --Werror` ueber `src include lib test main`: PASS.
- Zwei getrennte statische Analysen mit zwei getrennten Werkzeugen
  (Ownerentscheidung, Abschnitt 7.8.1):

  ```text
  Portable/native Quellen:
  Debian clang-tidy 18 ueber die native PlatformIO-Compile-Datenbank
  (unveraendert)

  ESP-IDF-spezifische Quellen (main/app_main.cpp,
  lib/device_platform_esp_idf/src/esp_timer_time_source.cpp):
  Espressif esp-clang esp-20.1.1_20250829 ueber den offiziellen
  idf.py clang-check-Mechanismus, fuer beide Profile getrennt
  ```

- `.clang-tidy` erhaelt zwei Aenderungen (beide reine
  Konfigurationsaenderungen, keine Fachcodeaenderung): `HeaderFilterRegex`
  wird von `(include|lib)/.*` auf `^(include|lib|main)/.*` erweitert
  (lokal verifiziert); zusaetzlich wird unter `CheckOptions:` die Option
  `readability-function-cognitive-complexity.IgnoreMacros: 'true'`
  ergaenzt (entschiedene Loesung fuer Befund A, Abschnitt 7.8.2). Beide
  Aenderungen sind Teil desselben Commit-4-Datei-Scopes (Abschnitt 5, 8).
  Generierte ESP-IDF- und Drittanbieterheader bleiben ausserhalb des
  `HeaderFilterRegex`-Filters; die dokumentierte Einzelausnahme fuer
  `misc-header-include-cycle` (Abschnitt 7.8.2, Befund B) betrifft ein
  vom Headerfilter unabhaengiges Checkverhalten, keine Filtererweiterung,
  und wird nicht in `.clang-tidy`, sondern ausschliesslich als
  ESP-IDF-pfadspezifische `--run-clang-tidy-options`-Ergaenzung
  umgesetzt.
- Werkzeugversionen: `clang-format-18`/`clang-tidy-18` (nativ,
  unveraendert, Grenzen der Reproduzierbarkeit: Abschnitt 7.11) sowie
  `esp-clang esp-20.1.1_20250829` (ESP-IDF-Pfad, neu, exakte
  Versions-/Hash-Pruefung: Abschnitt 7.8.1).

#### 7.8.1 ESP-IDF-Static-Analysis-Vertrag: offizieller `esp-clang`-Pfad
(Ownerentscheidung, ersetzt Plan-Commit `9607fba`)

**Ownerentscheidung (verbindlich, ersetzt die drei Optionen aus
Plan-Commit `9607fbafc283dfb89623043f10dbff43780bc148`):** Fuer die
ESP-IDF-spezifische statische Analyse wird die offizielle, zu ESP-IDF
6.0.2 gehoerende Espressif-`esp-clang`-Toolchain ueber den offiziellen
`idf.py clang-check`-Mechanismus verwendet. Die produktiven GCC-Builds
und deren Buildverzeichnisse (`build/esp32_bringup`, `build/esp32_release`)
bleiben davon strikt getrennt (Abschnitt 7.8.3). Diese Entscheidung ersetzt
alle drei Optionen aus dem vorherigen Plan-Commit; Option 2
(`compile_commands.json`-Filterung) und Option 3 (Verzicht auf die
ESP-IDF-spezifische Analyse) sind damit verworfen.

**Sachliche Korrektur zu `pyclang` (live gegen die reale ESP-IDF-6.0.2-
Installation verifiziert):** `tools/requirements/requirements.core.txt`
enthaelt am gepinnten ESP-IDF-Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e` bereits die Zeile `pyclang`.
Nach dem bestehenden, bereits in Commit 2 verwendeten
`"$IDF_PATH/install.sh" esp32` ist `pyclang` **ohne zusaetzlichen
Installationsschritt** verfuegbar — live verifiziert:
`importlib.metadata.version("pyclang")` meldet `0.7.0`. `pyclang` ist in
`espidf.constraints.v6.0.txt` (der von ESP-IDF selbst fuer
reproduzierbare Python-Abhaengigkeiten verwendeten Constraints-Datei)
**nicht** aufgefuehrt (live verifiziert: kein Treffer) — die tatsaechlich
installierte `pyclang`-Version ist damit **nicht** durch ESP-IDFs eigenes
Constraints-File exakt fixiert.

**Verbindliche Korrektur (ersetzt die vorherige Log-only-Behandlung):**
Da ESP-IDF selbst keine exakte `pyclang`-Fixierung liefert, erzwingt
`scripts/run_esp_idf_static_analysis.py` diese Fixierung projekteigen als
**fail-closed Vertrag**, nicht als reine Protokollierung:

```text
PYCLANG_VERSION = "0.7.0"   (scripts/esp_idf_contract.py)
```

Der Analysetreiber prueft `importlib.metadata.version("pyclang") ==
PYCLANG_VERSION` **vor** jedem Analyselauf und bricht bei jeder
Abweichung (aeltere, neuere oder fehlende `pyclang`-Installation) mit
einem harten Fehler ab, bevor `idf.py clang-check` ueberhaupt
aufgerufen wird — kein Fortfahren mit einer nicht exakt verifizierten
`pyclang`-Version, keine reine Log-Zeile. Eine spaetere, bewusste
`pyclang`-Versionsaenderung erfordert eine explizite Aktualisierung von
`PYCLANG_VERSION` in `scripts/esp_idf_contract.py` als eigene,
nachvollziehbare Aenderung — kein stiller Drift. Weiterhin verbindlich:
kein separates `pip install --upgrade pyclang`.

**`esp-clang`-Herkunft (live installiert und gegen den gepinnten
ESP-IDF-Commit verifiziert):**

```text
Tool:                esp-clang
Installtyp:          on_request (tools.json "install")
Versionsstatus:       recommended (tools.json "versions[].status")
Version:             esp-20.1.1_20250829
Lizenz:              Apache-2.0
Repository:          espressif/llvm-project
Linux-AMD64-SHA-256: 88910c21350c06a521f243304d1a3adbdb78447123b3f8e27493aab75e3cc07f
Linux-AMD64-Groesse: 339870496 Bytes
```

Installationsschritt (live verifiziert, exakte Syntax):

```bash
python "$IDF_PATH/tools/idf_tools.py" install esp-clang
. "$IDF_PATH/export.sh"
```

Nach diesen zwei Schritten zeigen `command -v clang` und
`command -v clang-tidy` live verifiziert auf
`$IDF_TOOLS_PATH/tools/esp-clang/esp-20.1.1_20250829/esp-clang/bin/...`.

**Korrektur aus der Abschlussreviewrunde — unterschiedliche, real
verifizierte Versionsstrings von `clang` und `clang-tidy`:** `clang
--version` meldet den vollstaendigen Toolpaketnamen
(`Espressif clang version 20.1.1 (...esp-20.1.1_20250829)`); `clang-tidy
--version` meldet dagegen **nur** `Espressif LLVM version 20.1.1` —
**ohne** den Toolpaketnamen `esp-20.1.1_20250829`. Eine fruehere Fassung
dieses Plans verlangte faelschlich, dass auch `clang-tidy --version`
diesen Toolpaketnamen exakt enthalten muesse; das widerspricht dem real
beobachteten Output und ist damit nicht erfuellbar. Die Herkunftspruefung
verwendet deshalb **mehrere, zueinander passende, unabhaengige Signale**
statt eines einzelnen, vom realen Tool nicht gelieferten Strings
(vollstaendige Spezifikation in Abschnitt 7.8.4). `scripts/esp_idf_
contract.py` fuehrt dafuer zwei getrennte Konstanten statt einer:

```text
ESP_CLANG_TOOL_VERSION = "esp-20.1.1_20250829"   (Toolpaketname, tools.json)
ESP_CLANG_LLVM_VERSION = "20.1.1"                (von clang-tidy gemeldete LLVM-Version)
```

Ein Rueckfall auf `/usr/bin/clang-tidy-18` im ESP-IDF-Analyseschritt ist
ein harter Fehler (Selftest-Anforderung, Abschnitt 7.8.6).

**Verifizierte, tatsaechlich funktionierende Kommandoform** (live gegen
beide Profile ausgefuehrt, identische Funde auf beiden — Abschnitt
7.8.2):

```bash
export IDF_TOOLCHAIN=clang
idf.py -B build/clang_tidy/esp32_bringup \
  -DSDKCONFIG=build/clang_tidy/esp32_bringup/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.bringup" \
  clang-check --exit-code \
  --run-clang-tidy-options '-header-filter="^(include|lib|main)/.*"' \
  'main/app_main\.cpp|esp_timer_time_source\.cpp'
```

analog fuer `esp32_release` mit `sdkconfig.defaults.release`.

**Drei live verifizierte, verbindliche Detailbefunde zur Kommandoform**
(muessen in der Implementierung exakt so beruecksichtigt werden, kein
Interpretationsspielraum):

1. **PATTERNS ist genau ein Regex, keine Dateiliste.** `idf.py
   clang-check [PATTERNS]...` nimmt zwar mehrere Argumente entgegen,
   `pyclang/runner.py` fuegt sie jedoch mit `' '.join(...)` zu einem
   **einzigen** Regex zusammen (live im installierten pyclang-Quelltext
   verifiziert, `site-packages/pyclang/runner.py`). Zwei separate
   Dateiargumente ergeben daher einen kaputten, nie-treffenden Regex
   (live reproduziert: „Running clang-tidy for 0 files out of 28").
   Verbindlich ist **eine** Alternations-Regex wie im Beispiel oben
   (`datei_a\.cpp|datei_b\.cpp`).
2. **`--run-clang-tidy-options` ist zwingend, sonst gilt nicht die
   Projekt-`.clang-tidy`.** `pyclang/runner.py` hat einen fest codierten
   Default fuer `-header-filter` (`".*\..*"`, praktisch alles) und eine
   eigene, vom Projekt unabhaengige `-checks=...`-Liste (u. a.
   `clang-analyzer-*`, im Projekt `.clang-tidy` nicht aktiviert). Ohne
   expliziten `--run-clang-tidy-options`-Override werden ESP-IDF-eigene
   System-Header (z. B. `xtensa/xtruntime.h`) faelschlich als Fehler
   gemeldet (live reproduziert). Wird `--run-clang-tidy-options` wie
   oben **ohne** eigenen `-checks=`-Wert gesetzt, uebernimmt
   `clang-tidy` korrekt die Projekt-`.clang-tidy` (live verifiziert: die
   Fehlermeldungen aus System-Headern verschwinden, die verbleibenden
   Funde stammen ausschliesslich aus dem projekteigenen Checks-Umfang,
   Abschnitt 7.8.2).
3. **`--exit-code` funktioniert wie dokumentiert.** Live verifiziert:
   `idf.py clang-check --exit-code ...` beendet sich mit Exitcode `1`,
   sobald Funde als Fehler gewertet werden, und mit `0`, wenn keine
   vorliegen.
4. **Es existiert keine auf Platte geschriebene, per PATTERNS gefilterte
   `compile_commands.json`** (Korrektur aus der Abschlussreviewrunde,
   live anhand des installierten `pyclang`- und `run-clang-tidy`-
   Quelltexts sowie eines realen Testlaufs verifiziert — wichtig fuer den
   in Abschnitt 7.8.4 spezifizierten Dateiauswahlnachweis). Zwei getrennte
   Filterschritte sind beteiligt, die leicht verwechselt werden koennen:
   - `pyclang/runner.py`s `filter_cmd()` schreibt die
     `compile_commands.json` im Analyseverzeichnis tatsaechlich neu —
     aber gefiltert nach `--include-paths`/`--exclude-paths` (hier nicht
     gesetzt) beziehungsweise, mangels solcher Optionen, nach der
     einfachen Regel „liegt die Datei unterhalb des Projektwurzel-
     verzeichnisses". Live verifiziert: nach einem vollstaendigen
     `clang-check`-Lauf enthaelt die Datei exakt die 28 projekteigenen
     Quellen (`lib/`, `main/`) — nicht die zwei per PATTERNS gewaehlten
     Dateien, sondern **alle** projekteigenen Uebersetzungseinheiten.
   - Die eigentliche PATTERNS-Filterung auf genau die zwei gewuenschten
     Dateien geschieht **ausschliesslich in-memory** innerhalb des von
     `esp-clang` mitgelieferten `run-clang-tidy`-Skripts (`files =
     {f for f in files if file_name_re.search(f)}`, gefolgt von der
     Konsolenzeile „Running clang-tidy for N files out of M"). Dieses
     Zwischenergebnis wird **nie** in eine Datei geschrieben.
   - **Praktische Folge:** Es gibt keinen Zeitpunkt, zu dem eine Datei auf
     Platte die PATTERNS-gefilterte Zwei-Dateien-Auswahl widerspiegelt —
     weder vor noch nach dem `clang-check`-Aufruf. Ein unabhaengiger
     Nachweis muss die Auswahl daher **selbst nachvollziehen** (exakt
     dieselbe Pfad-/Regexlogik wie `run-clang-tidy`, angewendet auf die
     **vollstaendige, ungefilterte** `compile_commands.json`, die eine
     gewoehnliche `idf.py ... reconfigure` liefert), nicht eine vermeintlich
     bereits gefilterte Datei einlesen. Live verifiziert: Die vollstaendige,
     ungefilterte `compile_commands.json` des Release-Analyseverzeichnisses
     enthaelt 456 Eintraege (ESP-IDF-interne plus projekteigene Quellen);
     dieselbe verankerte PATTERNS-Regex angewendet auf diese 456 Eintraege
     liefert ebenfalls exakt die zwei erwarteten Dateien, keine zufaellige
     Kollision mit einer gleichnamigen ESP-IDF-internen Datei. Die genaue
     Spezifikation dieses unabhaengigen Nachweises steht in Abschnitt
     7.8.4.

#### 7.8.2 Zwei Funde aus dem realen Probelauf — Befund A entschieden,
Befund B als einzeln begruendete Fremdheader-Ausnahme umgesetzt

Der reale Probelauf gegen **beide** Profile (identische Funde auf
`esp32_bringup` und `esp32_release`, da dieselben Quellen und dieselbe
`.clang-tidy` verwendet werden) lieferte **zwei** Funde.

**Befund A — entschieden: Makroexpansionsartefakt, keine echte
Codekomplexitaet (`OWNER_DECISION_COGNITIVE_COMPLEXITY:
IGNORE_MACRO_EXPANSIONS`):**

```text
main/app_main.cpp:23:6: error: function 'logBootSummary' has cognitive
complexity of 145 (threshold 25)
[readability-function-cognitive-complexity,-warnings-as-errors]
```

**Ursache:** Der hohe gemeldete Wert entsteht durch die Expansion der in
`logBootSummary` verwendeten `ESP_LOGI`/`ESP_LOGE`-Makros — diese Makros
expandieren intern zu bedingten Anweisungen (`if`/`else`-Ketten fuer die
laufzeitabhaengige Log-Level-Pruefung), die `clang-tidy` beim Nachzaehlen
der kognitiven Komplexitaet als zusaetzliche Verzweigungen des
Quellcodes zaehlt, obwohl sie nicht vom Projektentwickler geschrieben
wurden und keine reale Kontrollflusskomplexitaet des Projekts darstellen.

**Loesung (verbindlich, einzige zulaessige Umsetzung):**
`readability-function-cognitive-complexity.IgnoreMacros: 'true'` wird in
`.clang-tidy`s `CheckOptions:` ergaenzt. `main/app_main.cpp` bleibt dabei
**vollstaendig unveraendert** — kein Refactoring, kein `NOLINT`, keine
sonstige Aenderung an der Datei. Der Check selbst
(`readability-function-cognitive-complexity`) bleibt **aktiv**, sowohl
fuer `main/app_main.cpp` als auch fuer alle anderen von der ESP-IDF- und
der nativen Analyse erfassten Quellen — es handelt sich um eine gezielte
Option desselben Checks, keine Deaktivierung des Checks (weder projektweit
noch fuer diese eine Datei).

**Real verifiziert (Read-only-Nachweis dieser Ueberarbeitung, Abschnitt
7.8.7):**

- Mit `IgnoreMacros: 'true'` gesetzt: `idf.py clang-check --exit-code`
  gegen `main/app_main.cpp` liefert Exitcode `0`, keine Funde — der
  Makro-Befund verschwindet vollstaendig.
- Mit identischer Konfiguration, aber **ohne** die `IgnoreMacros`-Option
  (Check aktiv, Standardverhalten): derselbe Aufruf liefert exakt
  denselben Befund wie oben (Exitcode `1`, `cognitive complexity of 145`)
  — der Check ist also nachweislich weiterhin voll aktiv, nur die
  Makro-verursachte Fehlmeldung wird durch die Option unterdrueckt.
- Gegenprobe mit einer synthetischen, **nicht** makrobasierten Fixture
  (funktionale Verschachtelung aus `if`/`for`/`while`/`switch` ohne
  Makroverwendung, echte kognitive Komplexitaet): derselbe Check mit
  `IgnoreMacros: 'true'` meldet fuer diese Fixture weiterhin einen Fund
  (`cognitive complexity of 75`, Schwellenwert 25) — die Option
  unterdrueckt also **ausschliesslich** makroverursachte Verzweigungen,
  nicht echte, im Projektcode selbst geschriebene Verschachtelung. Die
  Ownerentscheidung schwaecht den Check damit nachweislich nicht
  projektweit ab.

**Befund B — Fremdheader-Ausnahme, durch Abschnitt 9 bereits vorgesehen:**

```text
.../esp-idf-v6.0.2/components/freertos/esp_additions/include/freertos/idf_additions.h:19:10:
error: circular header file dependency detected while including
'FreeRTOS.h' [misc-header-include-cycle,-warnings-as-errors]
```

Live verifiziert: Dieser Fund tritt trotz `-header-filter=^(include|lib|
main)/.*` auf, weil `misc-header-include-cycle` seine Diagnose
unabhaengig vom Header-Filter meldet (checktypisches clang-tidy-
Verhalten) — betrifft ausschliesslich ESP-IDFs eigene, unveraenderliche
Headerstruktur (`components/freertos/...`), nicht Projektcode. Dies ist
die in Abschnitt 9 bereits vorgesehene, einzeln zu benennende Ausnahme:
`misc-header-include-cycle` wird fuer den ESP-IDF-Analysepfad gezielt
deaktiviert, per verifizierter, exakter Syntax:

```text
--run-clang-tidy-options '-header-filter="^(include|lib|main)/.*" -checks="-misc-header-include-cycle"'
```

**Real verifiziert (Abschnitt 7.8.7):** Diese `-checks=`-Ergaenzung ist
eine echte **inkrementelle** Ausnahme zusaetzlich zu `.clang-tidy`, keine
zweite, `.clang-tidy` ersetzende Checkliste — belegt durch denselben
Testlauf wie bei Befund A: mit `-checks="-misc-header-include-cycle"`
und **ohne** `IgnoreMacros: 'true'` gesetzt liefert der Analyselauf
weiterhin den Befund-A-Fund (Check aktiv), aber nicht mehr den
Befund-B-Fund (Check gezielt ausgenommen) — beide Ausnahmen wirken
nachweislich unabhaengig voneinander und unabhaengig vom Rest der in
`.clang-tidy` aktivierten Checks. Die native Analyse (Debian
clang-tidy 18) bleibt von dieser Ausnahme unberuehrt, da sie ESP-IDF-
Header ueberhaupt nicht compiliert. `.clang-tidy` bleibt fuer beide
Analysepfade die alleinige Konfigurationsquelle; die ESP-IDF-Kommandozeile
ergaenzt ausschliesslich diese eine, oben genannte Ausnahme, keine
weiteren Checks und keinen abweichenden `-header-filter`-Wert ausserhalb
des in Abschnitt 7.8.1 dokumentierten Werts (Abschnitt 7.8.4 verlangt
eine reale Nachweisfuehrung dieses Vertrags im Analysetreiber).

#### 7.8.3 Strikte Trennung von Produktionsbuild und Analysebuild

`idf.py clang-check` re-konfiguriert und baut die per `-B` angegebene
Compile-Datenbank. Live verifiziert (Vergleich der `sdkconfig`-mtimes von
`build/esp32_bringup`/`build/esp32_release` vor und nach mehreren
Analyselaeufen mit fremdem `-B`): Bei Verwendung eines vom
Produktionsbuild vollstaendig getrennten `-B`-Verzeichnisses bleiben die
produktiven Buildverzeichnisse unveraendert.

Verbindlich fuer die Umsetzung:

```text
build/clang_tidy/esp32_bringup
build/clang_tidy/esp32_release
```

je mit eigenem CMake-Cache, eigener generierter `sdkconfig`, eigener
`compile_commands.json`, demselben gemeinsamen `sdkconfig.defaults` und
exakt dem passenden Profiloverlay (`sdkconfig.defaults.bringup`
beziehungsweise `sdkconfig.defaults.release`, analog zum
Produktionsbuildtreiber aus Abschnitt 7.4), `IDF_TOOLCHAIN=clang` bereits
vor dem ersten Configure-Aufruf, keine Wiederverwendung oder Mutation des
produktiven GCC-Buildordners. Die beiden Analyseprofile bleiben
untereinander ebenfalls strikt getrennt. Ein Analysefehler darf keine
produktiven Artefakte loeschen, ueberschreiben oder neu erzeugen.

#### 7.8.4 Kanonischer Analysetreiber `scripts/run_esp_idf_static_analysis.py`

Neues, projekteigenes Skript (kein generisches Toolchain-Framework):

```text
scripts/run_esp_idf_static_analysis.py
```

Aufgaben:

- Argumente `bringup`, `release`, `all` (analog zu
  `scripts/build_esp_idf_profiles.py`);
- Verwendung der bestehenden Konstanten/Namenshelfer aus
  `scripts/esp_idf_contract.py` (DRY, keine erneute Profil-/
  Pfadnamensdefinition), erweitert um `ESP_CLANG_TOOL_VERSION`,
  `ESP_CLANG_LLVM_VERSION`, `ESP_CLANG_LINUX_AMD64_SHA256`,
  `PYCLANG_VERSION` (Abschnitt 5);
- Pruefung der aktiven ESP-IDF-Herkunft (Wiederverwendung von
  `check_build_profiles.check_esp_idf_version()`, analog zum bestehenden
  Muster in `scripts/build_esp_idf_profiles.py`, Abschnitt 7.4 — keine
  zweite Implementierung);
- **Exakte Werkzeugverifikation vor jedem Analyselauf, aus mehreren
  unabhaengigen, jeweils zum real beobachteten Tooloutput passenden
  Signalen** (ersetzt die vorherige, zu schwache Pruefung
  „`clang-tidy --version` enthaelt Espressif"; ersetzt auch die
  zwischenzeitlich verlangte, aber am realen Output nachweislich
  scheiternde Forderung, `clang-tidy --version` muesse den vollstaendigen
  Toolpaketnamen enthalten, Abschnitt 7.8.1; jeder einzelne Punkt ist ein
  harter Fehler bei Abweichung, bevor `idf.py clang-check` aufgerufen
  wird):
  1. der aufgeloeste `clang`-Pfad liegt exakt unter
     `$IDF_TOOLS_PATH/tools/esp-clang/esp-20.1.1_20250829/esp-clang/bin/clang`;
  2. der aufgeloeste `clang-tidy`-Pfad liegt exakt unter
     `$IDF_TOOLS_PATH/tools/esp-clang/esp-20.1.1_20250829/esp-clang/bin/clang-tidy`
     (beide Pfade exakt, nicht nur ein Teilstring-Vergleich);
  3. `clang --version` enthaelt exakt `ESP_CLANG_TOOL_VERSION`
     (`esp-20.1.1_20250829`) — dieser vollstaendige Toolpaketname wird
     **nur** von `clang`, nicht von `clang-tidy` gemeldet (Abschnitt
     7.8.1);
  4. `clang-tidy --version` enthaelt exakt `ESP_CLANG_LLVM_VERSION`
     (`20.1.1`) — **ohne** den Toolpaketnamen zu verlangen, da
     `clang-tidy --version` ihn real nachweislich nicht ausgibt;
  5. die `tools.json`-Definition im exakt gepinnten ESP-IDF-Checkout
     (Abschnitt 7.1) enthaelt fuer `esp-clang` exakt
     `version: esp-20.1.1_20250829`, `status: recommended` und den
     Linux-AMD64-`sha256`-Wert `ESP_CLANG_LINUX_AMD64_SHA256`
     (Abschnitt 7.8.1) — bestaetigt die Werkzeugherkunft unabhaengig von
     jeder Versionsausgabe eines bereits installierten Binaries;
  6. `/usr/bin/clang-tidy-18` (das native, in Abschnitt 7.8 fuer den
     PlatformIO-Pfad verwendete Debian-Werkzeug) wird **ausdruecklich
     nicht** als aktives `clang-tidy` fuer den ESP-IDF-Analysepfad
     akzeptiert — expliziter Vergleich des aufgeloesten Pfads gegen
     diesen bekannten Fehlerfall, harter Fehler bei Treffer;
  7. `importlib.metadata.version("pyclang") == PYCLANG_VERSION`
     (`0.7.0`, Abschnitt 7.8.1) — fail-closed, kein Log-only;
  8. `.clang-tidy` bleibt nachweislich die alleinige Konfigurationsquelle:
     der Treiber verifiziert nach dem Zusammenbau von
     `--run-clang-tidy-options`, dass ausschliesslich der in
     Abschnitt 7.8.1 dokumentierte `-header-filter`-Wert und die eine in
     Abschnitt 7.8.2 dokumentierte `-checks="-misc-header-include-cycle"`-
     Ausnahme uebergeben werden — keine zusaetzlichen, `.clang-tidy`
     ueberschreibenden `-checks=`- oder `WarningsAsErrors`-Werte.

     Kein einzelner dieser Punkte allein ist hinreichend (ein passender
     Pfad allein beweist keine passende Version, ein passender
     Versionsstring allein beweist keine passende Herkunft) — erst die
     Kombination aus Pfad, Versionsausgabe **und** `tools.json`-Eintrag
     ist der verbindliche Herkunftsnachweis (Abschnitt 7.8.1);
- Aufbau der in Abschnitt 7.8.3 festgelegten, strikt getrennten
  Analyseverzeichnisse mit denselben Profiloverlays wie der
  Produktionsbuild;
- **Unabhaengiger Dateiauswahlnachweis vor dem eigentlichen
  `clang-check`-Aufruf, gegen die vollstaendige, ungefilterte
  `compile_commands.json`** (ersetzt die zwischenzeitlich verlangte,
  aber real nicht existierende „post-filter compile_commands.json"-
  Pruefung, Abschnitt 7.8.1 Detailbefund 4 — es gibt zu keinem Zeitpunkt
  eine auf Platte per PATTERNS gefilterte Datei):
  1. die PATTERNS-Regex wird programmatisch aus den beiden exakten,
     repository-relativen Pfaden `main/app_main.cpp` und
     `lib/device_platform_esp_idf/src/esp_timer_time_source.cpp` mittels
     `re.escape()` aufgebaut und verankert (`(?:^|/)` am Anfang, `$` am
     Ende jedes Pfadsegments), nicht aus einem frei geschriebenen
     Regex-String;
  2. der Treiber fuehrt im jeweiligen, strikt getrennten Analyse-
     verzeichnis (Abschnitt 7.8.3) selbst einen einfachen
     `idf.py -B <analyseverzeichnis> ... reconfigure`-Schritt aus (mit
     denselben Profiloverlays wie der eigentliche Analyselauf), um eine
     frische, **vollstaendige** `compile_commands.json` zu erzeugen —
     diese enthaelt sowohl ESP-IDF-interne als auch projekteigene
     Quellen (live verifiziert: 456 Eintraege im Release-Profil) und ist
     zu diesem Zeitpunkt **noch nicht** durch `pyclang`s eigenen,
     abweichenden Projektwurzel-Filter reduziert;
  3. der Treiber baut aus `directory` und `file` jedes Eintrags denselben
     absoluten Pfad, den auch `run-clang-tidy` intern bildet, und wendet
     dieselbe Regex aus Punkt 1 mit `.search()` darauf an — exakt
     dieselbe Auswahllogik wie im esp-clang-eigenen `run-clang-tidy`-
     Skript (Abschnitt 7.8.1 Detailbefund 4), aber als eigene,
     unabhaengige Berechnung, nicht durch Parsen von dessen Textausgabe;
  4. das Ergebnis muss **exakt zwei** Treffer liefern, je einer fuer die
     beiden erwarteten Dateien — harter Fehler bei: null Treffern, genau
     einem Treffer, drei oder mehr Treffern, doppelten Treffern derselben
     Datei, oder einem Treffer ausserhalb des erwarteten Repository-Pfads;
  5. erst nach bestandenem Nachweis wird `idf.py clang-check` mit
     derselben Regex aufgerufen; dessen eigener, in Abschnitt 7.8.1
     Detailbefund 4 dokumentierter interner Ablauf (erneutes
     `reconfigure`, `pyclang`s Projektwurzel-Filter, `run-clang-tidy`s
     eigene, nicht persistierte PATTERNS-Auswahl) waehlt dadurch
     nachweislich dieselben zwei, bereits unabhaengig bestaetigten
     Dateien aus;
  6. **Invariante, die diese Gleichheit traegt:** der eigene
     `reconfigure`-Schritt (Punkt 2) und der interne `reconfigure`-Schritt
     von `clang-check` (Punkt 5) laufen gegen **dasselbe**
     `-B`-Analyseverzeichnis mit **identischen**
     `-DSDKCONFIG`/`-DSDKCONFIG_DEFAULTS`-Argumenten, unmittelbar
     nacheinander, ohne dass der Treiber dazwischen den Quellbaum
     veraendert. Nur unter dieser Invariante liefert der interne
     `reconfigure`-Schritt von `clang-check` zwangslaeufig dieselbe
     Dateimenge wie der vorab selbst durchgefuehrte Schritt — der Treiber
     darf zwischen Punkt 2 und Punkt 5 keine andere Aktion ausfuehren, die
     `compile_commands.json` beeinflussen koennte;
- Aufruf von `idf.py clang-check` in der in Abschnitt 7.8.1 verifizierten
  Kommandoform (die oben verankerte Ein-Regex-PATTERNS,
  `--run-clang-tidy-options` mit Header-Filter und der Befund-B-Ausnahme,
  `--exit-code`);
- Analyse genau `main/app_main.cpp` und
  `lib/device_platform_esp_idf/src/esp_timer_time_source.cpp` je Profil;
- **`warnings.txt` wird auch bei einem fehlgeschlagenen Analyselauf
  gesichert** (nicht nur bei Erfolg): vor jedem Profillauf entfernt der
  Treiber einen eventuell vorhandenen alten profilspezifischen
  `warnings.txt`-Zielnachweis, ruft dann `idf.py clang-check` auf, sichert
  die neu erzeugte `warnings.txt` in den profilspezifischen Zielpfad
  (Abschnitt 7.8.5) **unabhaengig vom Exitcode**, und wertet den Exitcode
  erst danach aus (vergleichbar einem `finally`-Block: erst
  Beweissicherung, dann Fehlerbehandlung) — ein fehlgeschlagener Lauf darf
  nicht dazu fuehren, dass kein Nachweis der tatsaechlich gefundenen
  Verstoesse vorliegt;
- harter Fehler bei Toolchain-, Konfigurations- oder Analysefehler, mit
  klarer Ausgabe des betroffenen Profils und der fehlgeschlagenen Phase;
- keine Installation der Toolchain (das bleibt Aufgabe des in Abschnitt
  7.8.1 dokumentierten, separaten Installationsschritts);
- kein Flashen;
- keine Aenderung an `scripts/build_esp_idf_profiles.py`.

Lokaler und CI-Aufruf sind identisch; `build.yml` enthaelt keine zweite,
abweichende Folge komplexer `idf.py`-Befehle.

#### 7.8.5 Profilgetrennte Ausgaben

Live verifiziert: `idf.py clang-check` schreibt `warnings.txt` stets in
das aktuelle Arbeitsverzeichnis (Repository-Wurzel bei Aufruf ueber
`idf.py` aus dem Projektroot), **nicht** in das per `-B` angegebene
Analyseverzeichnis — ein zweiter Profillauf ueberschreibt sonst
unbemerkt den Nachweis des ersten (live reproduziert: der zweite
Testlauf gegen `esp32_release` ueberschrieb den `warnings.txt`-Stand des
ersten Testlaufs gegen `esp32_bringup`). Verbindlich:

```text
build/clang_tidy/esp32_bringup/warnings.txt
build/clang_tidy/esp32_release/warnings.txt
```

`scripts/run_esp_idf_static_analysis.py` verschiebt `warnings.txt`
unmittelbar nach jedem Profillauf vom Arbeitsverzeichnis in den
profilspezifischen Zielpfad — **unabhaengig davon, ob `clang-check` mit
Exitcode `0` oder `1` endet** (Abschnitt 7.8.4: Beweissicherung vor
Fehlerauswertung); eine fehlende Datei danach ist ein harter Fehler; vor
jedem Profillauf wird ein eventuell vorhandener alter Zielnachweis
entfernt, damit kein veralteter Stand stillschweigend akzeptiert wird.
Erst nachdem `warnings.txt` gesichert ist, wertet der Treiber den
Exitcode von `clang-check` aus und bricht bei Funden mit einem harten
Fehler ab — der gesicherte Nachweis bleibt dabei erhalten und ist damit
Teil der Fehlerdiagnose, nicht nur des Erfolgsfalls. Ein Upload dieser
Logs ist fuer #74 nicht zwingend; falls sie hochgeladen werden, gelten
der bestehende Artefaktscan (Abschnitt 7.7.4) und die
Scanabdeckungspruefung (`scripts/check_ci_artifact_scan_coverage.py`)
auch fuer diese neuen Textartefakte.

#### 7.8.6 Selftests

`scripts/run_esp_idf_static_analysis.py --selftest` beweist ohne echte
Toolchain-Installation:

1. Bring-up und Release verwenden verschiedene Analyse-Buildverzeichnisse.
2. Beide Profile verwenden verschiedene generierte `sdkconfig`-Pfade.
3. Bring-up verwendet ausschliesslich das Bring-up-Overlay, Release
   ausschliesslich das Release-Overlay.
4. `IDF_TOOLCHAIN=clang` ist bereits beim ersten Configure-/Clang-Check-
   Aufruf gesetzt.
5. Beide festgelegten Projektquellen sind in der zusammengesetzten
   PATTERNS-Regex fuer beide Profile enthalten.
6. Ein simulierter Diagnosefehler (nicht-Null-Ruckgabe des
   `clang-check`-Aufrufs) fuehrt zu einem harten Fehler des Treibers.
7. Ein simuliertes fehlendes oder falsches `esp-clang` (aufgeloester
   `clang`- oder `clang-tidy`-Pfad ausserhalb von
   `$IDF_TOOLS_PATH/tools/esp-clang/...`) fuehrt zu einem harten Fehler,
   bevor ein Analyselauf versucht wird — die reine Teilstring-Pruefung
   „enthaelt Espressif" allein ist dafuer laut Abschnitt 7.8.4 **nicht**
   ausreichend; siehe die praezisierenden Faelle 11–15 fuer die einzelnen
   Pfad-/Versions-/`tools.json`-Kriterien.
8. Die produktiven Verzeichnisse `build/esp32_bringup`/
   `build/esp32_release` werden vom Treiber nie als `-B`-Ziel verwendet.
9. `scripts/build_esp_idf_profiles.py` wird vom neuen Treiber nicht
   aufgerufen oder veraendert (analog zum bestehenden Selftest-Muster).
10. Eine fehlende profilspezifische `warnings.txt` nach dem Verschieben
    wird nicht still akzeptiert.
11. Ein simulierter korrekter `clang`-Pfad unter
    `$IDF_TOOLS_PATH/tools/esp-clang/...` mit `clang --version`, der
    exakt `ESP_CLANG_TOOL_VERSION` enthaelt, wird akzeptiert; derselbe
    Versionsstring an einem Pfad ausserhalb dieses Toolverzeichnisses
    wird abgelehnt.
12. Ein simulierter korrekter `clang-tidy`-Pfad unter
    `$IDF_TOOLS_PATH/tools/esp-clang/...` mit `clang-tidy --version`, der
    exakt `ESP_CLANG_LLVM_VERSION` (**ohne** den Toolpaketnamen) enthaelt,
    wird akzeptiert; derselbe Pfad mit abweichender LLVM-Version wird
    abgelehnt.
13. Eine simulierte `tools.json`-Fixture mit abweichender `esp-clang`-
    Version, abweichendem `status` oder abweichendem
    Linux-AMD64-`sha256` fuehrt zu einem harten Fehler — auch dann, wenn
    die Pfad- und Versionspruefungen aus den Faellen 11–12 fuer sich
    genommen bestehen wuerden (kein einzelnes Signal ist allein
    hinreichend, Abschnitt 7.8.4).
14. Ein aufgeloester `clang-tidy`-Pfad, der exakt
    `/usr/bin/clang-tidy-18` entspricht, wird explizit als bekannter
    Fehlerfall erkannt und abgelehnt (nicht nur implizit ueber die
    Pfadpruefung aus Fall 12).
15. Eine simulierte `clang --version`-Ausgabe ohne den exakten
    `ESP_CLANG_TOOL_VERSION`-String (z. B. abweichende Toolpaket-Version)
    fuehrt zu einem harten Fehler.
16. Eine simulierte `pyclang`-Version exakt `PYCLANG_VERSION` (`0.7.0`)
    wird akzeptiert.
17. Eine simulierte `pyclang`-Version ungleich `PYCLANG_VERSION` (aelter
    oder neuer) fuehrt zu einem harten Fehler, bevor `clang-check`
    aufgerufen wird.
18. Eine fehlende `pyclang`-Installation (kein Treffer in
    `importlib.metadata`) fuehrt zu einem harten Fehler mit klarer
    Fehlermeldung, nicht zu einer stillen Ausnahme.
19. Die aus `re.escape()` aufgebaute, verankerte PATTERNS-Regex liefert
    gegen eine synthetische, **vollstaendige, ungefilterte**
    `compile_commands.json`-Fixture mit sowohl ESP-IDF-internen als auch
    projekteigenen Eintraegen (analog zur real verifizierten, 456
    Eintraege umfassenden Datei, Abschnitt 7.8.1 Detailbefund 4) exakt
    zwei Treffer fuer die beiden erwarteten Dateien — unabhaengig von
    etwaigen gleichnamigen oder aehnlich benannten ESP-IDF-internen
    Dateien in derselben Fixture.
20. Dieselbe Fixture ohne einen der beiden erwarteten Dateipfade (null
    Treffer fuer diese Datei) fuehrt zu einem harten Fehler.
21. Dieselbe Fixture mit einem zusaetzlichen, aehnlich benannten
    Lookalike-Pfad (z. B. `test/fake/app_main.cpp` oder ein Pfad in einem
    fremden Wurzelverzeichnis mit identischem Dateinamen) fuehrt zu einem
    harten Fehler statt einer stillschweigenden Mehrfachauswahl.
22. Eine simulierte doppelte Auflistung derselben Datei in der Fixture
    fuehrt zu einem harten Fehler (keine versteckte Dopplung als „zwei
    Treffer" akzeptiert).
23. Gegen einen simulierten (nicht echten) `idf.py`-Aufruf geprueft: der
    Treiber ruft seinen eigenen `reconfigure`-Schritt **vor** dem Lesen
    der `compile_commands.json` und **vor** dem eigentlichen
    `clang-check`-Aufruf auf (Aufrufreihenfolge, nicht Dateiinhalt — ein
    Selftest ohne echte Toolchain kann keine reale Datei beobachten,
    Abschnitt 7.8.4 Punkt 6). Der Dateiauswahlnachweis selbst (Faelle
    19–22) wird gegen eine synthetische Fixture gefuehrt, die eine vom
    Treiber selbst per `reconfigure` erzeugte, vollstaendige
    `compile_commands.json` nachbildet — nicht gegen eine vermeintlich
    bereits von `clang-check` per PATTERNS gefilterte Datei (Abschnitt
    7.8.1 Detailbefund 4).
24. Die zusammengesetzten `--run-clang-tidy-options` enthalten fuer beide
    Profile ausschliesslich den dokumentierten `-header-filter`-Wert und
    die eine `-checks="-misc-header-include-cycle"`-Ausnahme — kein
    zusaetzlicher, `.clang-tidy` ueberschreibender Wert.
25. `warnings.txt` wird auch bei einem simulierten fehlgeschlagenen
    `clang-check`-Aufruf (Exitcode `1`) in den profilspezifischen Zielpfad
    verschoben, **bevor** der Treiber den Fehler weiterreicht (Reihenfolge
    wird explizit geprueft, nicht nur das Endergebnis).
26. Ein bereits vorhandener, veralteter `warnings.txt`-Zielnachweis wird
    vor einem neuen Profillauf entfernt und nicht als aktueller Nachweis
    stehen gelassen.

`scripts/selftest_quality_gates.py` ruft diesen neuen Selftest ab Commit 4
zusaetzlich auf (bestehendes `run_script_selftest()`-Muster).

Realer Commit-4-Nachweis (zusaetzlich zu den Selftests): native Static
Analysis weiterhin PASS; ESP-IDF-Static-Analysis Bring-up und Release
PASS; vollstaendiger Formatcheck inklusive `main/` PASS; 420 native Tests
PASS; beide produktiven ESP-IDF-GCC-Builds PASS; beide bestehenden
Arduino-Produktionsprofile weiterhin PASS; Ressourcenbericht- und
Artefaktchecks weiterhin PASS; Architektur-, Secret- und Quality-Gates
PASS.

#### 7.8.7 Read-only-Verifikationsnachweis dieser Korrekturrunde

Vor dem Schreiben dieses Plan-Korrekturcommits real, ohne Aenderung an
einer Commit-4-Datei, gegen die lokale ESP-IDF-6.0.2-Installation
durchgefuehrt (kein Commit-4-Skript existiert bereits; alle Befehle
wurden manuell ausgefuehrt, `.clang-tidy` nur temporaer und nicht
committet veraendert, danach per `git checkout -- .clang-tidy`
vollstaendig zurueckgesetzt und gegen `HEAD` als identisch verifiziert;
Scratch-Analyseverzeichnisse wurden ausschliesslich unter `/tmp/`
angelegt und nach der Verifikation vollstaendig entfernt):

```text
1.  esp-clang Pfad/Version/Hash-Kontrakt (esp-20.1.1_20250829,
    SHA-256 88910c21...cc07f, Groesse 339870496 Bytes,
    tools.json status: recommended):                          PASS
2.  clang --version enthaelt exakt esp-20.1.1_20250829
    (ESP_CLANG_TOOL_VERSION):                                  PASS
3.  clang-tidy --version enthaelt exakt "Espressif LLVM version
    20.1.1" (ESP_CLANG_LLVM_VERSION), OHNE den Toolpaketnamen —
    bestaetigt die Notwendigkeit der Korrektur aus Blocker 1:      PASS
4.  pyclang installierte Version == 0.7.0
    (importlib.metadata.version):                              PASS
5.  IgnoreMacros: 'true' unterdrueckt Befund A
    (main/app_main.cpp, Exitcode 0, keine Funde):               PASS
6.  Ohne IgnoreMacros bleibt Befund A aktiv
    (identischer Fund, Exitcode 1):                             PASS
7.  IgnoreMacros unterdrueckt keine echte, nicht-makrobasierte
    Verschachtelung (synthetische Fixture, cognitive complexity
    75 gegen Schwellenwert 25, weiterhin gemeldet):              PASS
8.  -checks="-misc-header-include-cycle" unterdrueckt Befund B,
    waehrend Befund A (ohne IgnoreMacros) weiterhin aktiv bleibt
    (inkrementelle, nicht ersetzende Ausnahme):                  PASS
9.  pyclangs filter_cmd() filtert compile_commands.json real
    nach Projektwurzel-Zugehoerigkeit (28 projekteigene Dateien),
    NICHT nach der PATTERNS-Regex — live am Quelltext und an
    einem realen Lauf verifiziert (Blocker 2):                   PASS
10. Die tatsaechliche PATTERNS-Auswahl geschieht ausschliesslich
    in-memory in run-clang-tidy und wird nie auf Platte
    persistiert — live am Quelltext von
    esp-clang/.../bin/run-clang-tidy verifiziert:                PASS
11. Anchored re.escape()-Regex waehlt exakt 2 von 456 Dateien aus
    der vollstaendigen, ungefilterten Bring-up-
    compile_commands.json (frisch per eigenem reconfigure
    erzeugt):                                                    PASS
12. Dieselbe Pruefung fuer das Release-Profil: ebenfalls exakt
    2 von 456 Dateien, dieselben zwei Dateien (schliesst die in
    der vorherigen Runde offen gelassene Release-Luecke):        PASS
13. Produktive Buildverzeichnisse (build/esp32_bringup,
    build/esp32_release) bleiben durch einen separaten
    -B-Analyseordner unveraendert (mtime-Vergleich):              PASS
14. esp-clang-Herkunftswerte (Version, Linux-AMD64-SHA-256, Groesse)
    stimmen mit der bereits fruehere dokumentierten Fassung
    ueberein (keine Drift seit der vorherigen Ueberarbeitung):     PASS
15. .clang-tidy nach allen Testlaeufen vollstaendig auf den
    committeten Stand zurueckgesetzt (diff gegen HEAD leer):       PASS
16. Arbeitsverzeichnis nach Abschluss der Verifikation sauber
    (git status --short leer, keine Scratch-Dateien verblieben):   PASS
```

**Nicht als real verifiziert behauptet (bewusst offen fuer die
Implementierungsphase, kein PASS):**

- die in Abschnitt 7.8.4 spezifizierte Reihenfolge „`warnings.txt`
  sichern, dann Exitcode auswerten" ist eine Anforderung an die noch zu
  implementierende Steuerlogik von `scripts/run_esp_idf_static_
  analysis.py`, keine bereits beobachtete Laufzeiteigenschaft eines
  vorhandenen Treibers — real verifiziert wurde ausschliesslich, dass das
  zugrundeliegende Werkzeug (`idf.py clang-check`) selbst `warnings.txt`
  unabhaengig vom Exitcode in das Arbeitsverzeichnis schreibt (Abschnitt
  7.8.5); die Sicherungsreihenfolge im eigenen Treiber ist Commit-4-
  Implementierung und wird dort durch Selftest-Fall 25 geprueft;
- die anhand von `re.escape()` verankerte Regex verhindert nachweislich
  keine Uebereinstimmung mit einem identischen relativen Teilpfad unter
  einer fremden Wurzel (z. B. `anderer/ordner/main/app_main.cpp`) — dies
  ist unschaedlich, weil Nachweise 11–12 (die tatsaechliche Pruefung gegen
  die reale, aus dem eigenen Repository erzeugte, vollstaendige
  `compile_commands.json`) den eigentlichen Schutz bilden, nicht die
  Regex allein; kein realer Kollisionsfall trat gegen die tatsaechliche
  456-Eintraege-Datenbank auf; Selftest-Fall 21 deckt diesen synthetischen
  Grenzfall zusaetzlich ab;
- der eigene `reconfigure`-Schritt des Treibers (Abschnitt 7.8.4) wurde in
  dieser Runde manuell nachgebildet (separater `idf.py -B ... reconfigure`-
  Aufruf gegen ein Scratch-Analyseverzeichnis je Profil), nicht durch den
  noch nicht existierenden Treiber selbst ausgefuehrt — die Reihenfolge
  „eigener `reconfigure`, dann Regexpruefung, dann `clang-check`" ist
  Commit-4-Implementierung und wird dort durch Selftest-Fall 23 geprueft.

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
  ```

  **Korrektur aus vierter Reviewrunde:** `espressif/install-esp-idf-action`
  ist keine `uses:`-Action mehr in diesem Plan (Abschnitt 7.1); die
  ESP-IDF-Bereitstellung erfolgt direkt per `git clone`/`install.sh`, nicht
  ueber eine gepinnte Marketplace-Action. Nur die drei oben genannten
  Actions sind noch Teil der Loesung.

  Die Umsetzung darf bei diesen drei Actions lediglich pruefen, dass die
  Commits weiterhin existieren, ihre Herkunft kontrollieren und bekannte
  Security-Hinweise dazu dokumentieren — nicht sie ersetzen.

- `platformio==6.1.19` bleibt exakt gepinnt (unveraendert);
- `clang-format-18`/`clang-tidy-18` bleiben auf Major 18 fixiert; die
  tatsaechlich geladene Patch-Version wird im Build-Log und im
  Artefaktmanifest (Abschnitt 7.7.2) erfasst, ohne eine nicht garantierte
  Patch-Fixierung zu behaupten;
- ESP-IDF-Herkunft wird zwingend geprueft: Tag `v6.0.2`, Commit
  `7101770dc6db2667b3c477cc31365dd1acd6db4e`, sauberer Arbeitsbaum, ueber
  den in Abschnitt 7.1 beschriebenen Reproduzierbarkeitsvertrag. Der
  bestehende CMake-Guard auf `IDF_VER == v6.0.2` bleibt zusaetzliche
  Verteidigung, ist aber nicht der einzige Herkunftsnachweis.
- Zwei getrennte Static-Analysis-Werkzeuge mit unterschiedlichem
  Reproduzierbarkeitsgrad (Abschnitt 7.8.1):

  ```text
  Native:
  clang-tidy 18 aus dem Debian/Ubuntu-Paket; nur Major-Version fixiert,
  tatsaechliche Patchversion wird im Log erfasst (siehe oben)

  ESP-IDF:
  esp-clang esp-20.1.1_20250829 aus der Tooldefinition des exakt
  gepinnten ESP-IDF-Commits; Version und Linux-AMD64-SHA-256 sind exakt
  geprueft (Abschnitt 7.8.1). Die zusaetzlich benoetigte `pyclang`-
  Python-Abhaengigkeit ist dagegen nicht ueber ein Constraints-File
  fixiert (live verifiziert, Abschnitt 7.8.1); da ESP-IDF selbst keine
  exakte Fixierung liefert, erzwingt der Analysetreiber sie projekteigen
  als fail-closed Vertrag (`PYCLANG_VERSION = "0.7.0"` in
  `scripts/esp_idf_contract.py`, harte Pruefung vor jedem Lauf,
  Abschnitt 7.8.1/7.8.4) statt sie nur zu protokollieren.
  ```

### 7.12 Hardware-Smoke-Test-Gate (Bring-up und Release, Pflicht)

**Korrektur gegenueber der urspruenglichen Fassung:** #74 aendert die
Quelle der Profilwahl, fuehrt den Release-Profilbuild neu ein, aendert die
ESP-IDF-CI-Strecke, macht ESP-IDF zum einzigen ESP32-Produktionspfad und
entfernt den bisherigen Arduino-Produktionspfad. Ein reiner Build beweist
nicht, dass auf realer Hardware das beabsichtigte Profil und die richtige
Aktorpolicy tatsaechlich starten. Ein Hardware-Gate ist daher **Pflicht**,
nicht optional.

**Zeitpunkt (Korrektur aus dritter Reviewrunde, sprachlich widerspruchsfrei
gemacht in vierter Reviewrunde):** Beide Hardware-Smoke-Tests werden
**ausschliesslich auf dem exakten finalen Implementierungs-Head nach
Commit 6** durchgefuehrt — nicht bereits auf Commit 5. Aendert sich der
Head nach einem Testlauf (z. B. durch einen weiteren Commit), verlieren
beide bisherigen Hardwaretest-Nachweise ihre Gueltigkeit; Bring-up- und
Release-Smoke-Test muessen dann auf dem neuen finalen Head vollstaendig
wiederholt werden. Je mindestens 35 Sekunden:

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
2. **ESP-IDF-CI additiv einfuehren:** direkte offizielle ESP-IDF-
   Installation per exaktem Commit-Checkout und `install.sh`/`export.sh`
   (Abschnitt 7.1, **kein** `espressif/install-esp-idf-action`), beide
   Profile ueber den kanonischen Buildtreiber in `build.yml`; bestehender
   Arduino-Produktionspfad bleibt bestehen; CI auf exakt diesem Commit
   vollstaendig gruen.
3. **Ressourcenbericht und Artefakte:** `idf.py size --format json2`-
   Auswertung, Erweiterung von `scripts/build_report.py` um den
   **Parallelbericht** (Arduino- und ESP-IDF-Messungen unter eindeutig
   unterschiedenen Ueberschriften, Abschnitt 7.7.5 Phase 1),
   profilspezifische CI-Artefakte, Erweiterung von
   `scripts/check_secrets.py` um den Scan-Modus fuer generierte
   Artefakttexte samt neuer Selftests (Abschnitt 7.7, insbesondere 7.7.4).
4. **Format und Static Analysis:** vollstaendiger Format-Scope (`main`
   ergaenzt), `.clang-tidy`-`HeaderFilterRegex`-Erweiterung und
   `CheckOptions:
   readability-function-cognitive-complexity.IgnoreMacros: 'true'`
   (Befund A, Abschnitt 7.8.2 — entschieden, kein offener Punkt mehr),
   offizieller `esp-clang`-Installationsschritt (Abschnitt 7.8.1), neuer
   kanonischer Analysetreiber `scripts/run_esp_idf_static_analysis.py`
   (Abschnitt 7.8.4) fuer beide ESP-IDF-Profile getrennt, Erweiterung von
   `scripts/esp_idf_contract.py` um die neuen Werkzeugvertragskonstanten
   (Abschnitt 5), Selftests (Abschnitt 7.8.6), Integration in
   `scripts/selftest_quality_gates.py`.

   **Exakter Dateiumfang dieses Commits:**
   `.github/workflows/build.yml`, `.clang-tidy`,
   `scripts/esp_idf_contract.py`, `scripts/run_esp_idf_static_analysis.py`,
   `scripts/selftest_quality_gates.py`. **Nicht** Teil dieses Commits:
   `main/app_main.cpp` (bleibt laut Befund-A-Entscheidung unveraendert,
   Abschnitt 7.8.2).

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
   2); `scripts/build_report.py` im selben Commit vom Parallelbericht auf
   den finalen `native` + ESP-IDF-only-Bericht umgestellt (Abschnitt 7.7.5
   Phase 2); `.github/workflows/build.yml`-Schritt „PlatformIO
   installieren“ auf ausschliesslich `native` reduziert.
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
Hardware-Smoke-Tests (Abschnitt 7.12) verbindlich entschieden. Das vierte,
abschliessende Review hat zusaetzlich die vollstaendige Abkehr von
`espressif/install-esp-idf-action` zugunsten einer direkten offiziellen
ESP-IDF-Installation (Abschnitt 7.1, siehe dortiger Befund zur falschen
`idf-im-cli`/`idf-im-ui`-Zuordnung), den zweiphasigen Uebergangsvertrag fuer
`scripts/build_report.py` (Abschnitt 7.7.5), die widerspruchsfreie
Hardwaretest-Formulierung (Abschnitt 7.12) und die vollstaendige Liste
ueberholter Plan-Commits verbindlich entschieden. Diese Punkte entfallen
als offene Punkte. Es verbleibt:

1. Byte-Budget-Schwellenwerte bleiben `TBD_IMPLEMENTATION_BUDGET`
   (Abschnitt 7.7.3) — kein Blocker, sondern ein dokumentierter,
   bewusst offener Wert bis zu realer Belastungsmessung.
2. Die exakte Kconfig-Syntax fuer `main/Kconfig.projbuild` (Abschnitt 7.2)
   sowie die exakte, von ESP-IDF 6.0.2 tatsaechlich unterstuetzte Syntax
   der direkten Installation (Abschnitt 7.1) werden unmittelbar zu Beginn
   von Commit 1 beziehungsweise Commit 2 gegen den echten ESP-IDF-6.0.2-
   Build verifiziert; sollten sie von der hier skizzierten Form abweichen,
   ist das eine technische Detailkorrektur ohne Vertrags-/Safetywirkung,
   keine materielle Planabweichung, solange Repository, Tag und Commit aus
   Abschnitt 7.1 unveraendert bleiben.
3. Sollte die direkte ESP-IDF-Installation mit dem in Abschnitt 7.1 exakt
   genannten Commit `7101770dc6db2667b3c477cc31365dd1acd6db4e` auf
   `ubuntu-24.04` nachweislich nicht reproduzierbar funktionieren, ist
   ausschliesslich der in Abschnitt 7.1 beschriebene Fallback-Prozess
   (Anhalten, neuer Plan-Commit, erneute Freigabe) zu befolgen — **kein**
   eigenmaechtiger Ruecksprung auf `espressif/install-esp-idf-action` oder
   eine andere Installationsvariante.
4. Der in Abschnitt 7.1 dokumentierte `LICENSE_METADATA_CONFLICT` von
   `espressif/install-esp-idf-action` bleibt als Begruendung der
   Nichtverwendung im Plan dokumentiert, ist aber kein aktiver
   Abhaengigkeits- oder Lieferkettenvertrag mehr, da die Action im aktiven
   CI-Pfad nicht mehr verwendet wird.
5. Der genaue Flag-Name fuer den in Abschnitt 7.7.4 beschriebenen
   `check_secrets.py`-Scan-Modus (`--scan-path` oder gleichwertig) wird in
   der Umsetzung final benannt; der Pruefvertrag selbst ist bereits
   verbindlich festgelegt.
6. **Entschieden (Abschnitt 7.8.1):** Der urspruenglich in Abschnitt 7.8
   woertlich festgelegte Befehl `clang-tidy -p build/esp32_bringup
   main/app_main.cpp ...` funktionierte nachweislich nicht gegen die von
   ESP-IDF 6.0.2 generierte `compile_commands.json` (kein Xtensa-Backend
   in `clang-tidy-18`, live verifiziert, Plan-Commit `9607fba`). Der
   Owner hat die ESP-IDF-spezifische statische Analyse verbindlich auf
   den offiziellen `esp-clang`/`idf.py clang-check`-Pfad festgelegt
   (Abschnitt 7.8.1); die beiden anderen zuvor offenen Optionen
   (`compile_commands.json`-Filterung, Verzicht auf die ESP-IDF-
   spezifische Analyse) sind verworfen. Dieser Punkt entfaellt damit als
   offene Entscheidung.
7. **Entschieden (Abschnitt 7.8.2, Befund A):** `main/app_main.cpp`
   enthielt einen gemeldeten `readability-function-cognitive-complexity`-
   Befund (`logBootSummary`, Komplexitaet 145 gegen Schwellenwert 25),
   live mit `esp-clang` gegen beide Profile verifiziert. Der Owner hat
   diesen Befund als Makroexpansionsartefakt der `ESP_LOG*`-Makros
   identifiziert (`OWNER_DECISION_COGNITIVE_COMPLEXITY:
   IGNORE_MACRO_EXPANSIONS`) und die Loesung
   `readability-function-cognitive-complexity.IgnoreMacros: 'true'` in
   `.clang-tidy` verbindlich festgelegt; `main/app_main.cpp` bleibt
   unveraendert, der Check bleibt aktiv (real gegenverifiziert anhand
   einer synthetischen, nicht-makrobasierten Fixture, Abschnitt 7.8.7).
   Dieser Punkt entfaellt damit als offene Entscheidung.
8. **Entschieden (Abschnitt 7.8.2, Befund B):** `misc-header-include-cycle`
   feuert trotz `HeaderFilterRegex`-Scoping auf ESP-IDFs eigener
   `FreeRTOS.h`/`idf_additions.h`-Headerstruktur (live verifiziert, beide
   Profile identisch betroffen). Dies ist keine Ownerentscheidung, sondern
   eine technische, einzeln begruendete Fremdheader-Ausnahme fuer den
   ESP-IDF-Analysepfad (Abschnitt 7.8.2): die exakte
   Deaktivierungssyntax (`-checks="-misc-header-include-cycle"` als
   inkrementelle Ergaenzung zu `--run-clang-tidy-options`) ist real
   verifiziert und verbindlich (Abschnitt 7.8.7). Dieser Punkt entfaellt
   damit als offene Entscheidung.
9. **Entschieden, fail-closed (Abschnitt 7.8.1):** Die tatsaechlich
   installierte `pyclang`-Version (live beobachtet: `0.7.0`) ist nicht
   ueber `espidf.constraints.v6.0.txt` exakt fixiert (live verifiziert:
   kein Eintrag). Dies wird **nicht** nur protokolliert, sondern durch
   eine eigene, projektweite `PYCLANG_VERSION`-Konstante
   (`scripts/esp_idf_contract.py`) und eine harte
   `importlib.metadata.version("pyclang") == PYCLANG_VERSION`-Pruefung im
   Analysetreiber vor jedem Lauf erzwungen (Abschnitt 7.8.4). Kein
   offener Punkt mehr — ein Versionswechsel erfordert eine bewusste
   Aenderung dieser Konstante, keine stille Drift.

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
- kein Wechsel von der direkten offiziellen ESP-IDF-Installation
  (Abschnitt 7.1) auf `espressif/install-esp-idf-action` oder eine andere
  Installationsvariante ohne den dort beschriebenen Fallback-Prozess;
- kein neues Dokument unter dem Pfad `docs/THIRD_PARTY_COMPONENTS.md`; die
  bestehende Datei wird ausschliesslich in-place aktualisiert (Abschnitt
  7.13.3);
- keine Aenderung an `docs/audits/` (historische Auditdateien, Abschnitt
  7.13.3);
- fuer Befund A (Abschnitt 7.8.2) ausschliesslich die entschiedene Loesung
  `readability-function-cognitive-complexity.IgnoreMacros: 'true'` in
  `.clang-tidy`; **keine** Aenderung an `main/app_main.cpp` (kein
  Refactoring, kein `NOLINT`), und **keine** projektweite Deaktivierung
  des Checks (`readability-function-cognitive-complexity` bleibt aktiv);
- keine zweite, `.clang-tidy` ueberschreibende oder ergaenzende
  `-checks=`-Liste im ESP-IDF-Analysepfad ausser der einen dokumentierten
  `-checks="-misc-header-include-cycle"`-Ausnahme (Abschnitt 7.8.2);
- kein `pip install --upgrade pyclang` und kein sonstiger unfixierter
  Versions-Bump der ESP-IDF-eigenen Python-Abhaengigkeiten (Abschnitt
  7.8.1); keine Lockerung der fail-closed `PYCLANG_VERSION`-Pruefung zu
  einer reinen Protokollierung;
- keine vorsorgliche Installation von `esp-clang-libs`, solange kein
  realer Testnachweis zeigt, dass `idf.py clang-check` sie benoetigt
  (Abschnitt 7.8.1).

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
- keine Secrets in Workflow, Cache oder Artefakten: die bestehende
  Pruefung getrackter Repositorydateien in `scripts/check_secrets.py`
  bleibt unveraendert erhalten; das Skript selbst wird zusaetzlich um den
  in Abschnitt 7.7.4 beschriebenen Scan explizit benannter, ungetrackter
  Textartefakte (Manifeste, Buildlogs, `sdkconfig`,
  `compile_commands.json`, `size.json`, Flashargumentdateien) erweitert —
  die Datei bleibt also nicht unveraendert, nur ihre bisherige Pruefung
  getrackter Dateien bleibt es;
- keine unfixierten Actions: alle `uses:`-Referenzen werden auf
  Commit-SHA gepinnt (Abschnitt 7.11);
- fehlgeschlagene ESP-IDF-Builds liefern Log-Artefakte, analog zum
  bestehenden `platformio-build-log`-Muster;
- kein Cache taeuscht Buildkorrektheit vor: #74 fuehrt bewusst keinen
  Toolchain-Cache ein (Abschnitt 7.1);
- zwei Hardware-Smoke-Tests (Bring-up, Release) sind vor Merge Pflicht
  (Abschnitt 7.12) — kein rein simulierter Nachweis fuer den
  Produktionswechsel;
- `esp-clang` (Apache-2.0, `espressif/llvm-project`) und `pyclang` (durch
  ESP-IDFs `requirements.core.txt` installiert) sind reine Build-/CI-
  Werkzeuge fuer die statische Analyse (Abschnitt 7.8.1); keine dieser
  Abhaengigkeiten wird in die Firmware gelinkt oder als
  Laufzeitkomponente registriert, kein `idf_component.yml` und kein
  Runtime-Komponentenlock dafuer (Abschnitt 7.9 bleibt unveraendert);
- `esp-clang`s Herkunft wird ueber die exakte Version
  (`esp-20.1.1_20250829`) und den Linux-AMD64-SHA-256
  (`88910c21350c06a521f243304d1a3adbdb78447123b3f8e27493aab75e3cc07f`)
  aus der Tooldefinition des gepinnten ESP-IDF-Commits geprueft
  (Abschnitt 7.8.1); ein Rueckfall auf System-`clang-tidy-18`
  (`/usr/bin/clang-tidy-18`, explizit als bekannter Fehlerfall geprueft)
  im ESP-IDF-Analyseschritt ist ein harter Fehler (Abschnitt 7.8.4);
- `pyclang`s Version ist **fail-closed** erzwungen, nicht nur
  protokolliert: `importlib.metadata.version("pyclang") ==
  PYCLANG_VERSION` (`0.7.0`, `scripts/esp_idf_contract.py`) wird vor
  jedem Analyselauf geprueft; ein Abweichen bricht den Lauf ab, bevor
  `idf.py clang-check` aufgerufen wird (Abschnitt 7.8.1, 7.8.4).

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
- `scripts/run_esp_idf_static_analysis.py --selftest` (26 Fixture-Faelle,
  Abschnitt 7.8.6) wird ab Commit 4 zusaetzlicher Pflichtschritt in
  `scripts/selftest_quality_gates.py`;
- `idf.py clang-check` fuer beide Profile getrennt (Abschnitt 7.8.1/7.8.4)
  wird neuer CI-Pflichtschritt, mit der in Abschnitt 7.8.2 entschiedenen
  `IgnoreMacros: 'true'`-Konfiguration;
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

**Commit-4-spezifische Bewertung (diese Ueberarbeitung, Abschnitt 7.8):**

- **SRP:** `scripts/run_esp_idf_static_analysis.py` (Analyse) bleibt
  strikt getrennt von `scripts/build_esp_idf_profiles.py`
  (Produktionsbuild) — unterschiedliche Verzeichnisse (Abschnitt 7.8.3),
  unterschiedliche Toolchains, keine gegenseitige Aufrufkette; der
  Produktionsbuildtreiber bleibt unveraendert (Abschnitt 7.8.4).
- **OCP/DRY:** Die neuen Werkzeugvertragskonstanten
  (`ESP_CLANG_TOOL_VERSION`, `ESP_CLANG_LLVM_VERSION`,
  `ESP_CLANG_LINUX_AMD64_SHA256`, `PYCLANG_VERSION`) erweitern das
  bestehende
  `scripts/esp_idf_contract.py` statt eine zweite, parallele
  Konstantenquelle einzufuehren (Abschnitt 5); `.clang-tidy` bleibt die
  alleinige Checkkonfiguration fuer beide Analysepfade — der
  ESP-IDF-Pfad ergaenzt genau eine dokumentierte Ausnahme
  (`-checks="-misc-header-include-cycle"`), keine zweite, konkurrierende
  Positivliste (Abschnitt 7.8.2, 7.8.4).
- **KISS ohne Safety-/Reproduzierbarkeitsabschwaechung:** Befund A wird
  ueber die kleinstmoegliche, bereits im Check vorgesehene
  Konfigurationsoption geloest (`IgnoreMacros: 'true'`) statt ueber ein
  Refactoring, eine Inline-Ausnahme oder eine projektweite
  Checkdeaktivierung — die am wenigsten eingreifende Loesung, die den
  Check dabei nachweislich nicht schwaecht (Abschnitt 7.8.7). Kein
  zusaetzlicher `pip install` ausserhalb des ohnehin bestehenden
  ESP-IDF-Installationsschritts; `pyclang` wird nicht separat installiert,
  nur seine bereits vorhandene Version fail-closed geprueft. Die
  Dateiauswahl fuer die statische Analyse verlaesst sich nicht auf
  Werkzeug-Textausgabe, sondern auf eine einfache, direkt gegen die reale
  `compile_commands.json` verifizierbare Mengenpruefung (exakt zwei
  Treffer) — die einfachste Absicherung, die die real reproduzierte
  "0 von 28 Dateien"-Falle strukturell ausschliesst, ohne ein generisches
  Validierungsframework einzufuehren.
- **DIP:** Der Analysetreiber haengt von der offiziellen `idf.py
  clang-check`-Schnittstelle und der Projektkonfiguration
  (`scripts/esp_idf_contract.py`, `.clang-tidy`) ab, nicht von
  Implementierungsdetails der Produktionsbuildtreiber.

## 16. Abnahmekriterien

### CI

- Profil- und Konfigurationsguard (`scripts/check_build_profiles.py`)
  gruen fuer beide Profile;
- native Tests gruen;
- Bring-up-IDF-Build gruen (isolierter Buildpfad);
- Release-IDF-Build gruen (isolierter Buildpfad);
- Format gruen (inklusive `main/`);
- native `clang-tidy` (Debian, Major 18) gruen;
- Bring-up-ESP-IDF-`clang-tidy` (offizieller `esp-clang`-Pfad,
  Abschnitt 7.8.1) gruen;
- Release-ESP-IDF-`clang-tidy` (offizieller `esp-clang`-Pfad,
  Abschnitt 7.8.1) gruen;
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

- keine Umschreibung der bestehenden Plan-Historie (kein Amend, kein
  Rebase, kein Force-Push auf einen bereits gepushten Plan-Commit);
- alle tatsaechlich vorhandenen Plan-Korrekturcommits werden transparent
  dokumentiert, statt eine nicht zutreffende feste Commitanzahl zu
  behaupten. Diese Regel ersetzt die urspruengliche Vorgabe „genau ein
  Plan-Korrekturcommit" endgueltig — bereits zwei vorherige
  Ueberarbeitungsrunden dieses Plans bestanden real aus mehreren Commits:
  eine Runde aus den zwei Commits
  `d1a1ec21f7463dc274f7bd9f03d1b46f00a655c4` und
  `bf9981b2676a0b7662713a4d8968b68274a075c5`; eine spaetere Runde aus den
  drei Commits `969cd9607dfd382e0510e588872207b96530b79a`,
  `f43e227db98fd5a4e71ec7f443b5befc331a8627` und
  `ce251f49a02e3b9defc0513a84c781f9636a7f70`,
  jeweils weil ein Selbstreview nach dem ersten Commit derselben Runde
  reale, im ersten Commit uebersehene Widersprueche fand — einschliesslich
  eines Falls, in dem der Nachbesserungscommit selbst wieder eine feste
  Zahl behauptete und dadurch einen weiteren Nachbesserungscommit noetig
  machte. Eine erneut fest behauptete Commitzahl fuer kuenftige Runden
  waere dieselbe Art von
  nicht belastbarer Zusicherung;
- stattdessen verbindlich: jede Ueberarbeitungsrunde bleibt auf
  `docs/tasks/issue-74-implementation-plan.md` (und die PR-Beschreibung)
  beschraenkt, verwendet ausschliesslich neue, sequentielle Commits ohne
  Historienumschreibung, und jeder zusaetzliche Commit innerhalb
  derselben Runde wird im PR-Beschreibungstext einzeln benannt und
  begruendet (kein stillschweigendes Nachbessern);
- Draft-PR-Beschreibung verweist auf den neuen Plan-Commit und markiert
  **alle vorherigen Plan-Korrekturcommits** als überholt (keine feste
  Zahl nennen — jede Ueberarbeitungsrunde kann, wie die beiden
  vorherigen bereits gezeigt haben, aus mehr als einem Commit bestehen;
  eine im Voraus genannte Zahl wird dadurch bei der naechsten Runde
  zuverlaessig wieder falsch). Vollstaendige Liste zum Zeitpunkt dieser
  Fassung:
  `05b987e3d2b375b82922990f718d0dc07c730a71`,
  `bbccd74d49b7fcb7c2c529054da5dcd2d8e9a754`,
  `6c8092755dde1fe0b39299abe94a0b3e02003beb`,
  `806abbfd7400412285c1f83e94d80cc6c5a7bf31`,
  `9607fbafc283dfb89623043f10dbff43780bc148`,
  `72700d76aec8e0a4fb5a0e78bb17d3ebcaa2ad53`,
  `d1a1ec21f7463dc274f7bd9f03d1b46f00a655c4`,
  `bf9981b2676a0b7662713a4d8968b68274a075c5`,
  `969cd9607dfd382e0510e588872207b96530b79a`,
  `f43e227db98fd5a4e71ec7f443b5befc331a8627`,
  `ce251f49a02e3b9defc0513a84c781f9636a7f70`;
- Status bleibt `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`;
- vollstaendiges Anhalten bis zu einem commitgebundenen
  `PLAN APPROVED`-Ownerkommentar auf den neuen Plan-Commit.
