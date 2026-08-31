# Issue #29 Build- und Ressourcenbericht

Dieser Bericht gehört zu Issue #29 und ist dem finalen Firmware-Source-Commit
`3bc5bfe4120d7ca6609733ab9d0736f1cfe99b59` (B2-Fix) zugeordnet. Der frühere
Firmwarestand `5950814fc21be557e565dad3aa6acf3dbe3c0b64` ist nur die
Pre-Fix-Provenienz. Die ESP-IDF-Profile wurden mit ESP-IDF `v6.0.2` / Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e` gebaut.

## Historischer KISS-Pivot V1 (SUPERSEDED_BY_KISS_V2)

Die nachfolgende Implementierungsevidenz aus dem exhaustive Versuch bleibt
als historische Evidenz erhalten. Sie ist durch die Owner-Korrektur
`EXHAUSTIVE_STATIC_GATE_ATTEMPT=SUPERSEDED_KISS` ersetzt und kein aktueller
Build- oder CI-Gate.

```text
PR129=OPEN_DRAFT
HISTORICAL_PLAN_PATH=docs/tasks/issue-29-panic-requalification-correction-plan.md
PLAN_SHA=42568610611ebffa6ace89e46f3fe3ea568e0e72
PLAN_STATUS=HISTORICAL_SUPERSEDED
EXHAUSTIVE_STATIC_GATE_ATTEMPT=SUPERSEDED_KISS
ANALYZER_REVERTED=YES (auf 3fbaf32)
MAIN_CMAKE_ISSUE29_HEAP_INSTRUMENTATION_REVERTED=YES
DEVICE_PLATFORM_ISSUE29_INSTRUMENTATION_REVERTED=YES

CURRENT_MAX_KNOWN_STATIC_PATH_BYTES=72224
CURRENT_MAX_KNOWN_STATIC_PATH_IS_UPPER_BOUND=NO
FULL_TRANSITIVE_STATIC_CALLGRAPH_CLOSURE=NOT_REQUIRED
STATIC_ANALYSIS_ROLE=DIAGNOSTIC_EVIDENCE_NOT_GLOBAL_UPPER_BOUND
HARDWARE_HWM_ROLE=PRIMARY_EMPIRICAL_STACK_EVIDENCE

OLD_PROBE_TASK_STACK_BYTES=67584
COMPILED_runProbe_FRAME_BYTES=65712
OLD_PROBE_TASK_STACK_IS_BELOW_KNOWN_STATIC_PATH=YES
ROOT_CAUSE=UNRESOLVED
IMPLEMENTATION=NOT_STARTED_KISS_REVISION
HARDWARE_RUN=NO
LEVEL_MEASUREMENTS=NOT_RUN
ISSUE25_STARTED=NO
MERGE=NO
```

Die drei neuen #29-Änderungen in `scripts/analyze_issue_29_stack.py`,
`main/CMakeLists.txt` und `lib/device_platform/CMakeLists.txt` werden auf
den Vor-Implementierungsstand `3fbaf32` zurückgeführt. Historische, bereits
vorher vorhandene #121-Instrumentierung bleibt unverändert. Der neue Plan
legt als späteren, separat freizugebenden Kausaltest ausschließlich einen
privaten Diagnose-Stack von 98304 B fest; Produkt-/Main-Task, Probe-Fachlogik,
Heap-API und Fault-Seam bleiben unverändert.

## `esp32_bringup`

- `size.json total_size`: 194215 Bytes
- DRAM: 13202 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN: 194336 Bytes
- ELF: 6971244 Bytes
- Mapfile: 5023889 Bytes
- Bootloader-BIN: 26096 Bytes
- Partitionstabellen-BIN: 3072 Bytes
- `sdkconfig` SHA-256: `2b1de6d6a368794932df27e4bdc9e7e4d3d0b709c788db2bf67bb2c487d07961`

## `esp32_release`

- `size.json total_size`: 126475 Bytes
- DRAM: 12618 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN: 126592 Bytes
- ELF: 3132728 Bytes
- Mapfile: 2722263 Bytes
- Bootloader-BIN: 26096 Bytes
- Partitionstabellen-BIN: 3072 Bytes
- `sdkconfig` SHA-256: `788bf5fda2065bdf0bcff4775021498a264d05f768ca9a08c23aef0bb53dfa78`

## Geltungsgrenze

Die Werte sind Build-/Artefaktwerte, keine Messung eines angeschlossenen
Boards. Beide Profile verwenden im generierten Build die aktuelle
ESP-IDF-Single-App-Tabelle:

```text
nvs,data,nvs,0x9000,24K,
phy_init,data,phy,0xf000,4K,
factory,app,factory,0x10000,1M,
```

Das ist eine erfasste aktuelle Buildbaseline und keine Festlegung der
finalen Produktionspartitionierung. Reale Flashgröße, Boardrevision und
PSRAM-Status bleiben bis zur Hardwaremessung offen.

## Vollständige lokale Verifikation

Der kombinierte Ressourcenbericht wurde nach dem B2-Fix erneut mit
`Source-Git-SHA`=`3bc5bfe4120d7ca6609733ab9d0736f1cfe99b59` erzeugt und
verifiziert; der tatsächliche Build-Commit dieses Laufs war
`a80999b55108aa5775e5cd5e44fae28911383643`. Die nachfolgenden
Software-/Buildnachweise wurden diesem korrigierten Firmwareinhalt zugeordnet.
`pio run -e native`, die vollständige native Suite mit
965/965 erfolgreichen Testfällen, beide ESP-IDF-Profile, beide
ESP-IDF-Static-Analysis-Profile, Clang-Tidy, Format-, Architektur-, Secret-,
Quality-Gate- und Artefakt-Scanprüfungen sowie die kumulative
Xtensa-Stackherleitung waren `PASS`. Diese Software-/Buildnachweise sind kein
Board-, UART-, Flash-, Pegel- oder Smoke-Nachweis.

Der ursprüngliche reale Hardwarelauf flashte beide Profile vom PR-HEAD
`c4c8b33f4dbaef727200ea410d887ec5417aa1b0` (`App version: c4c8b33` im
Bootlog); der Firmwareinhalt entsprach dem Pre-Fix-Stand `5950814`. Dieser
Lauf zeigte `esp32_bringup` real `FAILED`, während `esp32_release` `PASS` war.
Commit `3bc5bfe4120d7ca6609733ab9d0736f1cfe99b59` korrigiert die
B2-Nachweislogik in `main/issue_29_bringup_probe.cpp::run()` (die einzige
Firmwareänderung gegenüber `c4c8b33`/`5950814`). Der erste erfolgreiche
post-fix-Lauf wurde mit `App version: 3bc5bfe` 40 s erfasst; der zweite mit
`App version: 7024d15` ebenfalls 40 s. Der damalige Laufstand `7024d15` und
der PR-Head vor dieser Synchronisierung `a80999b55108aa5775e5cd5e44fae28911383643`
enthielten gegenüber `3bc5bfe` ausschließlich Dokumentänderungen in den
beiden Issue-29-Berichten. Diese Synchronisierung ergänzt keinen
Firmwarelogik-Fix, sondern nur den vorsichtigen Codekommentar und weitere
Dokumentation; die build-relevante Firmwareprovenienz der beiden erfolgreichen
Läufe bleibt der korrigierte Source-Stand `3bc5bfe`. Die vollständigen realen Board-, Flash-, PSRAM-, Smoke-,
Zyklus-Invarianz- und Probe-Ergebnisse sind in
[`ISSUE_29_MEASUREMENTS.md`](ISSUE_29_MEASUREMENTS.md) dokumentiert und
werden hier nicht dupliziert.

## Xtensa-Stack-Usage-Herleitung

Der Bring-up-Build dieses Heads aktiviert `-fstack-usage` und
`-fcallgraph-info=su` für die Diagnose-Probe sowie die tatsächlich
betroffenen `fermentation_app`-Quellen. Einzelne `.su`-Frames sind keine
kumulative Call-Path-Obergrenze. `python3 scripts/analyze_issue_29_stack.py
build/esp32_bringup` prüft die nicht-inlinierten `.ci`-Kanten und ergibt für
den deterministischen Candidate-Allocation-Failure-Pfad:

| gleichzeitig lebende Funktion | `.su`-Frame | Qualifier | kumulativ |
|---|---:|---|---:|
| `probeTask(void*)` | 48 B | `static` | 48 B |
| `runProbe(ProbeContext&)` | 53232 B | `static` | 53280 B |
| `persistFreshStartCommand(...)` | 32 B | `static` | 53312 B |
| `TemperatureControlApplicationOrchestrator::persistCommand(...)` | 304 B | `static` | 53616 B |
| `RunPersistenceCoordinator::persistCommand(...)` | 9280 B | `static` | 62896 B |
| `RunPersistenceCoordinator::result(...)` | 32 B | `static` | 62928 B |

Die gehaltene Xtensa-Objektsumme beträgt 24296 Bytes; sie ersetzt die
Call-Path-Summe nicht. Der begründete, begrenzte Diagnosepuffer beträgt
4096 Bytes für Task-Einstieg/RTOS-Rahmen und kleine
Compiler-/Instrumentierungsvariation. Auf 1024 Bytes ausgerichtet ergibt
das 67584 Bytes Diagnose-Taskgröße. Alle sechs Qualifier sind `static`; ein
`dynamic`-/`unbounded`-Qualifier oder fehlende Callgraph-Kante blockiert den
Hardwarelauf. Die Werte sind Compiler-/Build-Evidenz, keine On-Target-HWM-
Messung und kein Produktivbudget. `CONFIG_ESP_MAIN_TASK_STACK_SIZE` wurde
nicht verändert. Die `uxTaskGetStackHighWaterMark()`-Einheit ist für ESP32 in
ESP-IDF 6.0.2 als Bytes verifiziert.

## Historische Implementierung Plan-Abschnitt 4.1-4.5 (2026-08-31, SUPERSEDED_KISS)

Owner-Freigabe: `OWNER_PLAN_REVIEW=PASS`,
`APPROVED_PLAN_SHA=4a34967ac202196b7afceaebfe2b2429338d6d93`. Vollständige
technische Herleitung, Instrumentierungsdetails und die Klassifikation der
unaufgelösten Randbedingungen stehen in
[`ISSUE_29_MEASUREMENTS.md`](ISSUE_29_MEASUREMENTS.md), Abschnitt
"Implementierung Abschnitt 4.1-4.5"; hier nur die digitalen Gate-Ergebnisse.

### Digitale Gates (kein Hardwarelauf, kein Flash)

```text
SOURCE_TREE_CLEAN=YES @ a19cd1605fb1e74e6dd31f8b457741e9cf92b2d9 (alle
  digitalen Gates dieser Runde liefen gegen diesen exakten,
  firmwarerelevanten Stand; danach nur Markdown-Folgeaenderungen)
ISSUE29_STACK_ANALYZER=IMPLEMENTED (voll neu geschrieben: P0-P6-Traversierung,
  fail-closed nach 4.1.1, Whitelist fuer 4 verifizierte virtuelle
  Aufrufstellen, ROM-Boundary-Tabelle)
ISSUE29_DIAGNOSTIC_TASK_STATIC_STACK_GATE=BLOCKED
ALL_RELEVANT_PROBE_STACK_PATHS=BLOCKED
CURRENT_MAX_PROBE_TASK_CUMULATIVE_BYTES=72224
UNKNOWN_REACHABLE_EDGES=225 (32 eindeutige Symbole)
UNRESOLVED_INDIRECT_CALLS=1 (tlsf_walk_pool Callback)
UNRESOLVED_CALLGRAPH_CYCLES=0
STACK_CONSTANT_GATE=NOT_REACHED (Gate stoppt vor der Staleness-Pruefung)
kMeasuredCallPathBytes=62928 (main/issue_29_bringup_probe.cpp, UNVERAENDERT)
kProbeTaskStackBytes=67584 (main/issue_29_bringup_probe.cpp, UNVERAENDERT)
ESP_IDF_BRINGUP_BUILD=PASS
ESP_IDF_RELEASE_BUILD=PASS
ESP_IDF_STATIC_ANALYSIS_ESP_CLANG=PASS (beide Profile; bestaetigt, dass die
  neuen GNU-Guards um die -fstack-usage/-fcallgraph-info=su-Bloecke den
  esp-clang-Analysepfad unveraendert lassen, statt das nur anzunehmen)
FULL_NATIVE_BUILD=PASS
FULL_NATIVE_TESTS=PASS (1081/1081)
ARCHITECTURE_GATES=PASS (nach Ruecknahme von main PRIV_REQUIRES heap, siehe
  ISSUE_29_MEASUREMENTS.md)
SECRET_SCAN=PASS
QUALITY_GATE_SELFTEST=PASS
GIT_DIFF_CHECK=PASS
BRINGUP_HAS_ISSUE29_PROBE=YES (ELF-Symbolnachweis: probeTask vorhanden)
RELEASE_HAS_ISSUE29_PROBE=NO (kein issue_29-Quellcode im Build, kein
  probeTask-Symbol im ELF)
NATIVE_HAS_ISSUE29_PROBE=NO (kein Verweis unter src/, include/)
ISSUE90_HARNESS_HAS_ISSUE29_PROBE=NOT_RE_RUN (main/CMakeLists.txt-Gate-Logik
  unveraendert, per git diff belegt; kein neuer Harness-Build in dieser
  Runde ausgefuehrt, daher keine erneute ELF-Symbolpruefung)
FAULT_SEAM_UNCHANGED=YES (lib/fermentation_app/private/issue_29_bringup_fault_seam.hpp
  unveraendert)
PRODUCT_TASK_STACKS_UNCHANGED=YES (nur Diagnose-Konstanten betroffen, und
  auch diese wurden mangels STACK_GATE=PASS NICHT veraendert)
```

`kMeasuredCallPathBytes`/`kProbeTaskStackBytes` in
`main/issue_29_bringup_probe.cpp` wurden **nicht** verändert: der Plan
erlaubt das ausdrücklich nur nach einem echten `STACK_GATE=PASS` mit
exaktem Witness; dieser wurde in dieser Runde nicht erreicht.
`scripts/build_esp_idf_profiles.py` wurde **nicht** um den Analyzer-Aufruf
erweitert (Plan 4.4) — das würde jeden künftigen `esp32_bringup`-Build/CI-Lauf
sofort fehlschlagen lassen, solange `STACK_GATE=BLOCKED` real ist; diese
Konsequenz wird dem Owner vorgelegt statt einseitig scharf geschaltet.

### Owner-Entscheidungsfrage (`PLAN_CHANGE_REQUIRED=YES`)

Plan-Abschnitt 4.2 benennt ausdrücklich nur den ESP-IDF-Component `heap` als
"die aktuell einzige konkret bekannte reachable-aber-uninstrumentierte
Komponente". Der real implementierte, vollständig fail-closed Analyzer
findet nach Schließung von `heap` und `device_platform` noch 32 weitere
eindeutige unaufgelöste Randstellen, davon:

- **(A)** 12 Symbole in weiteren, quellcodeverfügbaren ESP-IDF-Components
  (`freertos`, `esp_system`/`heap`-Rest, `cxx`, `esp_libc`/newlib) —
  grundsätzlich mit demselben, bereits etablierten CMake-Muster schließbar,
  aber nicht durch Plan 4.2 namentlich freigegeben.
- **(B1)** 7 Symbole nicht-Template, garantiert toolchain-vorkompiliert
  (`libsupc++`, `picolibc`), kein Quellcode unter `$IDF_PATH` oder im
  Projekt verfügbar. `operator new` (`_Znwj`) ist objdump-bestätigt
  **nicht leaf** (ruft real `malloc` sowie die
  Exception-Allocation-/Throw-Kette auf). Eine Grenzbestimmung würde eine
  transitive Binär-Callgraph-Analyse über mehrere vorkompilierte
  Bibliotheksebenen erfordern (vom Plan als "neue generische
  Analyseplattform" ausgeschlossen) oder eine unbegründete Annahme
  einführen (durch "Nicht durch Annahmen umgehen" ausgeschlossen).
- **(B2)** 10 `basic_string<char>`-Templatemethoden: Quellcode liegt in den
  Toolchain-Headern vor und die dortige `_GLIBCXX_EXTERN_TEMPLATE=-1`-
  Konfiguration sollte lokale Instanziierung mit echtem Frame erlauben,
  bleibt aber empirisch in jeder aufrufenden TU frameless, während eine
  benachbarte Methode derselben Klasse (`_M_dispose`) korrekt lokal
  instanziiert wird. Ursache nicht abschließend isoliert — siehe
  `ISSUE_29_MEASUREMENTS.md`. Diese 10 Symbole sind technisch nicht
  gleichzusetzen mit (B1) und werden dem Owner als offene Teilfrage
  vorgelegt, nicht als bereits geklärt "strukturell unschließbar".

Damit ist der reale Umfang, um `STACK_GATE=PASS` zu erreichen, größer als in
Plan 4.2 vorgesehen, und (B) ist mit den im Plan erlaubten Mitteln aktuell
strukturell nicht schließbar. Das ist kein Implementierungsfehler dieser
Runde, sondern ein während der Implementierung entdeckter, plan-relevanter
Befund: `PLAN_CHANGE_REQUIRED=YES`. Weder (A) noch (B) wurden ohne
Ownerfreigabe umgesetzt oder umgangen.

## Historischer Status nach KISS-Korrektur V1 (SUPERSEDED_BY_KISS_V2)

```text
EXHAUSTIVE_STATIC_GATE_ATTEMPT=SUPERSEDED_KISS
CURRENT_MAX_KNOWN_STATIC_PATH_BYTES=72224
CURRENT_MAX_KNOWN_STATIC_PATH_IS_UPPER_BOUND=NO
ANALYZER_REVERTED=YES
MAIN_CMAKE_ISSUE29_HEAP_INSTRUMENTATION_REVERTED=YES
DEVICE_PLATFORM_ISSUE29_INSTRUMENTATION_REVERTED=YES
ISSUE90_HARNESS_HAS_ISSUE29_PROBE=NOT_RE_RUN_THIS_CORRECTION_ROUND

DIAGNOSTIC_PROBE_TASK_STACK_BYTES=98304
IMPLEMENTATION=NOT_STARTED_KISS_REVISION
ROOT_CAUSE=UNRESOLVED
HARDWARE_RUN=NO
LEVEL_MEASUREMENTS=NOT_RUN
ISSUE25_STARTED=NO
MERGE=NO
```

Die historische Analyse ist damit nur noch Diagnoseevidenz. Die 72224 B sind
kein globaler Upper Bound. Vor einem späteren Hardwarelauf sind die digitale
Isolation, ein frischer Issue-90-Harness-Nachweis und die exakte
Buildprovenienz erneut zu prüfen; nicht ausgeführte Nachweise bleiben
`NOT_RUN` oder `BLOCKED`.


## Aktueller KISS-Pivot V2 nach PR-#131-Synchronisierung (2026-08-31)

Die PR-#129-Historie wurde ohne Rebase und ohne Force-Push per Merge der neuen
Integrationsbaseline synchronisiert. PR #131 hat keine #29-Firmware-, Stack-
oder Analyzerdatei verändert; die neue R1-GPIO-/Wiring-SSOT ist für spätere
elektrische #29-Verifikation dennoch verbindlich.

~~~text
PR131=MERGED
PR131_MERGE_SHA=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
INTEGRATION_BASE_SHA=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
CURRENT_INTEGRATION_BASE_SHA=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
PANIC_REPRODUCTION_SOURCE_SHA=c1f5fbb5f19ab8e7d2c25708fe79777d523217d4
CURRENT_BASE_CONTROL_BOOT=NOT_RUN
CURRENT_BASELINE_PANIC_CONFIRMED=PENDING
PR131_GPIO_SSOT_PRESERVED=YES
GPIO_SSOT_PATH=config/board_profiles/esp32_32e_quad_mosfet_r1.yaml

EXHAUSTIVE_STATIC_GATE_ATTEMPT=SUPERSEDED_KISS
QUALIFIER_FAIL_CLOSED_BUG=FOUND
INDIRECT_CALL_EDGE_COLLAPSE_RISK=KNOWN
FULL_TRANSITIVE_STATIC_CALLGRAPH_CLOSURE=NOT_REQUIRED
GENERIC_BINARY_CALLGRAPH_ANALYZER=NO
TRANSITIVE_LIBSTDCXX_BINARY_STACK_PLATFORM=NO
STATIC_ANALYSIS_ROLE=DIAGNOSTIC_EVIDENCE_NOT_GLOBAL_UPPER_BOUND
HARDWARE_HWM_ROLE=PRIMARY_EMPIRICAL_STACK_EVIDENCE
UNKNOWN_REACHABLE_EDGES=225
UNRESOLVED_INDIRECT_CALLS=1
CURRENT_MAX_KNOWN_STATIC_PATH_BYTES=72224
CURRENT_MAX_KNOWN_STATIC_PATH_IS_GLOBAL_UPPER_BOUND=NO
ROOT_CAUSE=UNRESOLVED

ANALYZER_REVERTED=YES
MAIN_CMAKE_ISSUE29_HEAP_INSTRUMENTATION_REVERTED=YES
DEVICE_PLATFORM_ISSUE29_INSTRUMENTATION_REVERTED=YES
THREE_FILE_FUNCTIONAL_STATE=3FBAF32
~~~

`QUALIFIER_FAIL_CLOSED_BUG=FOUND` bezeichnet den historischen
Implementierungsbefund: Der Analyzer parste `NodeInfo.qualifier`, erzwingt
aber nicht für jeden traversierten Frame `qualifier == static`. Das bekannte
`INDIRECT_CALL_EDGE_COLLAPSE_RISK=KNOWN` bezeichnet die Reduktion mehrerer
GCC-Kanten desselben Callers zu `__indirect_call` in
`dict[str, set[str]]`; einzelne indirekte Call-Sites sind damit nicht robust
unterscheidbar. Beide Befunde sind zusätzliche Gründe für den KISS-Pivot und
werden nicht durch eine neue Analyzerarchitektur repariert.

Die Evidenz aus `a19cd1605fb1e74e6dd31f8b457741e9cf92b2d9` und
`92f6cf6c3ee16868b95b7ee6a4e9b233d9ffb0c6` bleibt historisch. Die drei
Exhaustive-Dateien entsprechen funktional `3fbaf32`; ältere #121-Flags und
die bestehende #29/#90-Compile-Time-Isolation bleiben erhalten. In diesem
Nachtrag wurden kein Build, kein Flash und kein Hardwarelauf ausgeführt.

Neue kanonische Planrevision:

~~~text
NEW_PLAN_PATH=docs/tasks/issue-29-panic-requalification-correction-plan.md
CURRENT_PLAN_SHA=b7d80de7d6e23fd792c2bd48eaa27052a8c61201
PLAN_BASE_SHA=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
CURRENT_BASE_CONTROL_BOOT_SPECIFIED=YES
DIAGNOSTIC_PROBE_TASK_STACK_BYTES=98304
HWM_ROOT_CAUSE_DISCRIMINATOR_SPECIFIED=YES
GPIO_SSOT_LEVEL_GATE_SYNC=YES
IMPLEMENTATION=NOT_STARTED_KISS_REVISION
HARDWARE_RUN=NO
LEVEL_MEASUREMENTS=NOT_RUN
ROOT_CAUSE=UNRESOLVED
~~~

## Current-base-Kontrollbuild (2026-08-31)

Dieser Abschnitt ist die aktuelle digitale Kontrollbuild-Evidenz auf dem
owner-freigegebenen Kontrollstand. Frühere historische Nachträge und ihre
damaligen `NOT_RUN`-Aussagen bleiben unverändert; die Artefakte dieses
Abschnitts wurden anschließend nicht geflasht und nicht auf Hardware gebootet.

```text
CONTROL_WORK_HEAD=7edda30de1d39d5a4945137146ab16da530c5dc6
PR129_BASE_SHA=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
CURRENT_INTEGRATION_BASE_SHA=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
PR129_BEHIND_INTEGRATION=0
CONTROL_BASE_INVALID=NO
PLAN_PATH=docs/tasks/issue-29-panic-requalification-correction-plan.md
CURRENT_PLAN_SHA=b7d80de7d6e23fd792c2bd48eaa27052a8c61201

SOURCE_TREE_CLEAN=YES
FIRMWARE_DIFF_VS_INTEGRATION=NONE
MAIN_DIFF_VS_INTEGRATION=NONE
LIB_DIFF_VS_INTEGRATION=NONE
SCRIPTS_DIFF_VS_INTEGRATION=NONE
CMAKE_DIFF_VS_INTEGRATION=NONE
SDKCONFIG_DIFF_VS_INTEGRATION=NONE
ONLY_NON_FIRMWARE_PLAN_DOCUMENTATION_DIFF=YES

kMeasuredCallPathBytes=62928
kMeasuredCallPathSafetyBufferBytes=4096
kProbeTaskStackBytes=67584
DIAGNOSTIC_PROBE_TASK_STACK_BYTES=67584
STACK_FIX_IMPLEMENTED=NO

ESP_IDF_BRINGUP_BUILD=PASS
ESP_IDF_RELEASE_BUILD=PASS
FULL_NATIVE_BUILD=PASS
FULL_NATIVE_TESTS=PASS (1081/1081)
ESP_CLANG_BRINGUP=PASS
ESP_CLANG_RELEASE=PASS
ARCHITECTURE_GATES=PASS
SECRET_SCAN=PASS
QUALITY_GATES=PASS
GIT_DIFF_CHECK=PASS

BRINGUP_HAS_ISSUE29_PROBE=YES
RELEASE_HAS_ISSUE29_PROBE=NO
NATIVE_HAS_ISSUE29_PROBE=NO
ISSUE90_HARNESS_HAS_ISSUE29_PROBE=NO

IMPLEMENTATION_SOURCE_SHA=7edda30de1d39d5a4945137146ab16da530c5dc6
ESP32_BRINGUP_BUILD_SOURCE_SHA=7edda30de1d39d5a4945137146ab16da530c5dc6
APP_EMBEDDED_SOURCE_SHA=7edda30de1d39d5a4945137146ab16da530c5dc6
ELF_SHA256=f3ab27542f2686ff7e8ce954bcdca4bf033e1485524824774022e2e70fbda0c4
BIN_SHA256=cd7cb24a62e9fd3092ef351574443b819607616641da8723c0eb78c0855d2184
ESP32_RELEASE_ELF_SHA256=1417bf0726cf9214dc8af6e702769c4a4621e36d4bc35e3d2b92f2cdbf5eb0dd
ESP32_RELEASE_BIN_SHA256=bfa3a4c164de13d34f2afb2dfe92a587084360820c13a3c2e2254b8e88189d84

ESP_IDF_TAG=v6.0.2
ESP_IDF_COMMIT=7101770dc6db2667b3c477cc31365dd1acd6db4e
XTENSA_TOOLCHAIN=esp-15.2.0_20251204
ESP_CLANG_TOOLCHAIN=esp-20.1.1_20250829
PYTHON_VERSION=3.13.5

GPIO_SSOT_PATH=config/board_profiles/esp32_32e_quad_mosfet_r1.yaml
PR131_GPIO_SSOT_PRESERVED=YES
GPIO_MATRIX_STATUS=PLANNED_NOT_CONFIRMED
ELECTRICAL_VERIFICATION=PENDING
CONFIRMED_TEST=NO
ACTUATOR_RELEASE=NO

CURRENT_BASE_CONTROL_BOOT=NOT_RUN
CURRENT_BASELINE_PANIC_CONFIRMED=PENDING
ROOT_CAUSE=UNRESOLVED
HARDWARE_RUN=NO
LEVEL_MEASUREMENTS=NOT_RUN
ISSUE25_STARTED=NO
MERGE=NO
```

Der Panic-Nachweis bleibt auf der historischen Quelle
`c1f5fbb5f19ab8e7d2c25708fe79777d523217d4`; dieser digitale Kontrollbuild
belegt keinen aktuellen Baseline-Panic. `CURRENT_BASE_CONTROL_BOOT` wurde
bewusst nicht ausgeführt. `SECRET_SCAN=PASS` umfasst den getrackten Scan, den
Scan der 15 hochgeladenen Build-Artefakte und die Artifact-Scan-Abdeckung.
