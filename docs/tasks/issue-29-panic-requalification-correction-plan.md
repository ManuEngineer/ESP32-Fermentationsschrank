# Issue #29 – Panic-Requalifikation: KISS-Pivot V2

Diese vollständige Revision ist die neue kanonische Handlungsgrundlage für
Issue #29 und PR #129. Sie ersetzt die freigegebene Planrevision
`4a34967ac202196b7afceaebfe2b2429338d6d93` sowie die zwischenzeitliche
KISS-Revision `42568610611ebffa6ace89e46f3fe3ea568e0e72`. Die historischen
Plan- und Implementierungsstände bleiben als Provenienz erhalten.

Der exhaustive statische Callgraphversuch wird nicht fortgeführt. Statische
Analyse liefert nur Diagnoseevidenz; die primäre empirische Stackevidenz für
die spätere Requalifikation kommt vom Runtime-High-Water-Mark auf realer,
aktorfreier Hardware.

~~~text
PLAN_REVISION=KISS_PIVOT_V2
ISSUE29=OPEN
PR129=OPEN_DRAFT
BRANCH=agent/issue-29-requalification-r1
PLAN_BASE_SHA=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
INTEGRATION_BASE_SHA=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
PR131=MERGED
PR131_MERGE_SHA=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d

IMPLEMENTATION=NOT_STARTED_KISS_REVISION
ROOT_CAUSE=UNRESOLVED
HARDWARE_RUN=NO
LEVEL_MEASUREMENTS=NOT_RUN
ISSUE25_STARTED=NO
MERGE=NO
OWNER_PLAN_REVIEW_REQUIRED=YES

EXHAUSTIVE_STATIC_GATE_ATTEMPT=SUPERSEDED_KISS
FULL_TRANSITIVE_STATIC_CALLGRAPH_CLOSURE=NOT_REQUIRED
GENERIC_BINARY_CALLGRAPH_ANALYZER=NO
TRANSITIVE_LIBSTDCXX_BINARY_STACK_PLATFORM=NO
STATIC_ANALYSIS_ROLE=DIAGNOSTIC_EVIDENCE_NOT_GLOBAL_UPPER_BOUND
HARDWARE_HWM_ROLE=PRIMARY_EMPIRICAL_STACK_EVIDENCE

CURRENT_BASE_CONTROL_BOOT_SPECIFIED=YES
DIAGNOSTIC_PROBE_TASK_STACK_BYTES=98304
DIAGNOSTIC_PROBE_TASK_STACK_KIB=96
HWM_ROOT_CAUSE_DISCRIMINATOR_SPECIFIED=YES
GPIO_SSOT_LEVEL_GATE_SYNC=YES
~~~

## 1. Ziel und Grenzen

Ziel ist ein enges, fail-closed Kausalexperiment für den privaten
#29-Diagnose-Task:

1. Nach der Freigabe dieser exakten Planrevision einen einzigen
   Current-base-Kontrollbuild auf der neuen Integrationsbaseline herstellen
   und den bekannten Panic einmal actor-frei real verifizieren.
2. Nur wenn dieser Kontrollboot den Panic reproduziert, den privaten
   Diagnose-Task-Stack von 67584 B auf 98304 B erhöhen.
3. Den 96-KiB-Stand mit drei actor-freien Boots, Stack-HWM und den
   bestehenden digitalen Gates requalifizieren.
4. Erst nach einer erfolgreichen Requalifikation und Owner-Review die offenen
   elektrischen #29-Messungen gegen die kanonische R1-SSOT durchführen.

Nicht Bestandteil dieses Plans sind Produkt-Stackänderungen, eine Änderung der
Probe-Fachlogik, eine Änderung der Heap-API, ein neuer statischer
Global-Upper-Bound-Analyzer, ein Hardware-Safety-PASS oder eine Aktorfreigabe.

## 2. Verifizierte Ausgangslage und Provenienz

Die aktuelle Arbeitsgrundlage ist:

- Repository: `ManuEngineer/ESP32-Fermentationsschrank`;
- PR #129: `OPEN_DRAFT`, Branch
  `agent/issue-29-requalification-r1`;
- `integration/r1-development`:
  `1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d`;
- ESP-IDF: v6.0.2 am Commit
  `7101770dc6db2667b3c477cc31365dd1acd6db4e`;
- historische Panic-Evidenz:
  `PANIC_REPRODUCTION_SOURCE_SHA=c1f5fbb5f19ab8e7d2c25708fe79777d523217d4`;
- freigegebener alter Plan, nun superseded:
  `4a34967ac202196b7afceaebfe2b2429338d6d93`;
- vor-implementierter Stand der drei rückzunehmenden Dateien:
  `3fbaf32`;
- exhaustive Implementierungsversuche:
  `a19cd1605fb1e74e6dd31f8b457741e9cf92b2d9` und
  `92f6cf6c3ee16868b95b7ee6a4e9b233d9ffb0c6`;
- vorherige KISS-Zwischenrevision:
  `42568610611ebffa6ace89e46f3fe3ea568e0e72`.

PR #131 hat keine #29-Firmware-, Stack- oder Analyzerdatei geändert. Der
Merge führt aber die kanonische R1-GPIO-/Wiring-SSOT ein und beeinflusst damit
die spätere elektrische Verifikation. Ein Vergleich
`OLD_FAIL=c1f5fbb...` gegen einen späteren neuen Pass darf deshalb nicht als
"only change is stack" bezeichnet werden. Der Current-base-Kontrollboot ist
dafür zwingend.

Die historische Evidenz steht versioniert in
`docs/ISSUE_29_BUILD_REPORT.md` und
`docs/ISSUE_29_MEASUREMENTS.md`. Sie wird nicht als aktueller Hardwarelauf
umgedeutet.

## 3. Verbindliche Quellen, Architektur und Hardwarestatus

Für die spätere Ausführung gelten ausschließlich die einschlägigen
Repositoryquellen:

- `docs/SPECIFICATION_REVIEW.md` für Release-1-Scope und
  `TBD_HARDWARE`;
- `docs/DECISIONS.md`, insbesondere ADR-002 und ADR-013;
- `docs/HARDWARE.md` für Hardware-Gates und Statusbegriffe;
- `config/board_profiles/esp32_32e_quad_mosfet_r1.yaml` als einzige
  handgepflegte Quelle konkreter R1-GPIO-Zahlen;
- Issue #29 als fachlicher Bring-up- und Ressourcen-Scope;
- `docs/ACCEPTANCE_TESTS.md` und `docs/OPEN_POINTS.md` für die offenen
  Hardware- und Akzeptanzgates;
- `docs/CI_AND_QUALITY_GATES.md` für Befehle, Profile und Ergebnisstatus.

ADR-013 bleibt unverändert: `device_platform` enthält portable Ports,
`device_platform_esp_idf` konkrete ESP-IDF-Adapter,
`fermentation_app` die Fachlogik und
`device_platform_test_support` nur Testhilfen. Dieser Plan führt keine neue
Plattform- oder Recovery-Abstraktion ein.

Das R1-Boardprofil ist SSOT. Sein Status ist
`GPIO_MATRIX_STATUS=PLANNED_NOT_CONFIRMED`,
`ELECTRICAL_VERIFICATION=PENDING` und
`confirmed_test=false`. Der Owner-Referenzabgleich
`BOARD_FAMILY_REFERENCE_MATCH=CONFIRMED_BY_OWNER` ist kein elektrischer
PASS. `planned` und
`board_fixed_pending_electrical_verification` werden nicht als
`confirmed_test`, `pins_confirmed`, `active_levels_confirmed`,
`boot_levels_confirmed` oder `actuator_release` interpretiert. Eine zweite
GPIO-Matrix oder eine #29-Kopie der Pinzahlen ist verboten.

Für die spätere #29-Pegelverifikation werden aus der SSOT mindestens diese
Rollen verwendet, ohne ihre Werte erneut zu duplizieren:

~~~text
EN / CHIP_PU
GPIO0 / Boot-ROM-Download-Strap
GPIO16 = onboard_mosfet_1
GPIO17 = onboard_mosfet_2
GPIO26 = onboard_mosfet_3
GPIO27 = onboard_mosfet_4

active_level=TBD_HARDWARE
safe_boot_level=TBD_HARDWARE
external_bias=TBD_BOARD_CIRCUIT
~~~

Diese TBD-Werte bleiben bis zur realen Messung offen.

## 4. Requalifikationsbefund und statische Diagnoseevidenz

Der historische actor-freie Boot-1-Befund auf
`c1f5fbb5f19ab8e7d2c25708fe79777d523217d4` lautet:

~~~text
PROFILE=esp32_bringup
ISSUE90_HARNESS=ABSENT
BOOT_1=PANIC_REPRODUCED
BOOT_2=NOT_RUN_STOP_GATE
BOOT_3=NOT_RUN_STOP_GATE
PANIC=LoadProhibited
PANIC_INDUCED_RESET=YES
PANIC_RESET_REASON=rst:0xc (SW_CPU_RESET)
UNRELATED_UNEXPECTED_RESET=NO
WATCHDOG=NO
BROWNOUT=NO
ACTOR_RELEASE=false
ROOT_CAUSE=UNRESOLVED
~~~

Die beobachtete Kette ist:

~~~text
probeTask -> runProbe -> sampleResources
  -> heap_caps_get_largest_free_block -> heap_caps_get_info
  -> multi_heap_get_info_impl -> tlsf_walk_pool/block_is_last
~~~

Sie lokalisiert die Absturzbeobachtung, beweist aber nicht, dass der Heap-Walk
die ursprüngliche Ursache ist.

Aus den historischen Buildartefakten bleiben folgende Werte bekannt:

~~~text
OLD_PROBE_TASK_STACK_BYTES=67584
COMPILED_runProbe_FRAME_BYTES=65712
CURRENT_MAX_KNOWN_STATIC_PATH_BYTES=72224
CURRENT_MAX_KNOWN_STATIC_PATH_IS_GLOBAL_UPPER_BOUND=NO
CURRENT_MAX_KNOWN_STATIC_PATH_IS_COMPLETE_UPPER_BOUND=NO
OLD_PROBE_TASK_STACK_IS_BELOW_KNOWN_STATIC_PATH=YES
UNKNOWN_REACHABLE_EDGES=225
UNRESOLVED_INDIRECT_CALLS=1
HISTORICAL_PRE_TASK_LARGEST_FREE_BLOCK_BYTES=172032
ROOT_CAUSE=UNRESOLVED
~~~

Die 72224 B sind Diagnoseevidenz aus dem bisherigen Analyzerlauf. Sie sind
kein globaler oder vollständiger statischer Upper Bound. Die bekannte Rechnung
lautet:

~~~text
67584 < 72224
98304 - 72224 = 26080
~~~

## 5. KISS-Pivot und Rücknahme des Exhaustive-Versuchs

Der Exhaustive-Versuch ist endgültig superseded:

~~~text
EXHAUSTIVE_PROBE_TASK_ALL_PATH_STATIC_GATE=SUPERSEDED_KISS
FULL_TRANSITIVE_STATIC_CALLGRAPH_CLOSURE=NOT_REQUIRED
GENERIC_BINARY_CALLGRAPH_ANALYZER=NO
TRANSITIVE_LIBSTDCXX_BINARY_STACK_PLATFORM=NO
MORE_SDK_COMPONENT_INSTRUMENTATION=NO
STATIC_ANALYSIS_ROLE=DIAGNOSTIC_EVIDENCE_NOT_GLOBAL_UPPER_BOUND
HARDWARE_HWM_ROLE=PRIMARY_EMPIRICAL_STACK_EVIDENCE
~~~

Zusätzliche historische Analyzerbefunde sind ausdrücklich dokumentiert:

~~~text
QUALIFIER_FAIL_CLOSED_BUG=FOUND
INDIRECT_CALL_EDGE_COLLAPSE_RISK=KNOWN
~~~

Der Analyzer parst `NodeInfo.qualifier`, erzwingt aber bei der Traversierung
nicht für jeden Frame `qualifier == static`. Dadurch konnte ein nicht
statischer oder unbekannter Frame trotz gespeicherter Qualifierinformation in
die Summenbildung gelangen. Dieser Fail-closed-Befund wird nicht durch eine
neue Analyzerarchitektur repariert.

Mehrere GCC-Callgraph-Kanten desselben Callers zu
`__indirect_call` werden in `dict[str, set[str]]` auf denselben
Targetwert reduziert. Der Caller-basierte Whitelistmechanismus kann dadurch
einzelne indirekte Call-Sites nicht robust unterscheiden. Das ist ein
bekanntes Collapse-Risiko, keine automatische Garantie und kein künftiges
CI-Gate.

Die drei durch die Exhaustive-Implementierungsrunde betroffenen Dateien müssen
funktional exakt dem Stand vor `a19cd160` entsprechen:

~~~text
scripts/analyze_issue_29_stack.py=REVERTED_TO_3FBAF32
main/CMakeLists.txt=REVERTED_TO_3FBAF32
lib/device_platform/CMakeLists.txt=REVERTED_TO_3FBAF32
MAIN_CMAKE_ISSUE29_HEAP_INSTRUMENTATION=REMOVED
DEVICE_PLATFORM_ISSUE29_INSTRUMENTATION=REMOVED
EXHAUSTIVE_ANALYZER=REVERTED
~~~

Bestehende ältere #121-Analyseflags und die bestehende #29/#90
Compile-Time-Isolation bleiben erhalten:

~~~text
BRINGUP_HAS_ISSUE29_PROBE=YES
RELEASE_HAS_ISSUE29_PROBE=NO
NATIVE_HAS_ISSUE29_PROBE=NO
~~~

Ein frischer `ISSUE90_HARNESS_HAS_ISSUE29_PROBE=NO`-Nachweis ist erst bei
der späteren Ausführung erforderlich. Ein nicht ausgeführter Nachweis ist
`NOT_RUN`, nicht `PASS`.

## 6. Owner-Gate und exakte spätere Umsetzung

Diese Planrevision ist selbst keine Implementierungserlaubnis. Vor jeder
späteren Umsetzung müssen der exakte Commit dieser vollständigen Datei, der
live geprüfte PR-Status und die Zielcheckout-Baseline durch den Owner
freigegeben werden. Eine Freigabe der alten Plan-SHA autorisiert weder den
Kontrollboot noch den 96-KiB-Fix.

Nach dieser Freigabe gilt die folgende Reihenfolge.

### 6.1 Current-base-Kontrollbuild vor der Stackänderung

Auf genau einem sauberen Implementierungsbranch:

~~~text
CONTROL_BASE=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
PLUS_ONLY_APPROVED_NON_FIRMWARE_PLAN_METADATA
ISSUE29_PROBE_RUNTIME_CHANGE=NO
PROBE_TASK_STACK_BYTES=67584
~~~

Der kontrollierte Quellbaum darf gegenüber der neuen Integration nur die
freigegebenen nicht-Firmware-Planmetadaten enthalten. Vor dem Flash müssen die
digitalen Gates dieses exakten Builds bestanden sein. Der Kontrollbuild darf
nicht mit dem historischen `c1f5fbb`-Build gleichgesetzt werden.

Ein einziger realer, actor-freier Kontrollboot des normalen
`esp32_bringup`-Profils wird durchgeführt. Es gibt bei reproduziertem
bekanntem Panic keine zusätzliche 3er-Serie:

~~~text
CONTROL_BOOT_1=PANIC_REPRODUCED
CURRENT_BASELINE_PANIC_CONFIRMED=YES
~~~

Wenn der Panic auf der neuen Baseline nicht reproduziert wird:

~~~text
CONTROL_BOOT_1=NO_PANIC
CURRENT_BASELINE_PANIC_CONFIRMED=NO
STOP
~~~

Dann wird kein 96-KiB-Kausaltest durchgeführt; die Lage wird neu bewertet.

### 6.2 Minimaler 96-KiB-Diagnosefix

Nur nach einem bestätigten Current-base-Kontrollpanic darf genau die private
#29-Diagnose-Taskgröße geändert werden:

~~~text
DIAGNOSTIC_PROBE_TASK_STACK_BYTES=98304
DIAGNOSTIC_PROBE_TASK_STACK_KIB=96
~~~

Die einzige runtime-relevante Probeänderung des Kausaltests ist
`kProbeTaskStackBytes -> 98304`. Nicht geändert werden:

~~~text
PRODUCT_TASK_STACK_CHANGED=NO
PRODUCT_MAIN_TASK_STACK_CHANGED=NO
PROBE_LOGIC_CHANGED=NO
HEAP_API_CHANGED=NO
FAULT_SEAM_CHANGED=NO
PERSISTENCE_CHANGED=NO
GPIO_SSOT_CHANGED=NO
ACTOR_RELEASE=false
~~~

Die historischen Größen
`kMeasuredCallPathBytes`,
`kMeasuredCallPathSafetyBufferBytes` und
`kUnroundedProbeTaskStackBytes` sind nur diagnostische bzw. historische
Untergrenzen/Rechengrößen. Sie autorisieren keine andere Stackgröße und
werden nicht zu einem globalen Upper Bound.

### 6.3 Digitale Gates vor jedem späteren Flash

Die Befehle und Profile kommen ausschließlich aus
`docs/CI_AND_QUALITY_GATES.md`. Für den exakten Kontroll- und späteren
96-KiB-Stand müssen, soweit vom Owner für den jeweiligen Schnitt angeordnet,
mindestens folgende digitale Nachweise mit exakter Source-SHA vorliegen:

~~~text
SOURCE_TREE_CLEAN=YES
ESP_IDF_BRINGUP_BUILD=PASS
ESP_IDF_RELEASE_BUILD=PASS
FULL_NATIVE_BUILD=PASS
FULL_NATIVE_TESTS=PASS
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

IMPLEMENTATION_SOURCE_SHA=<exact>
ESP32_BRINGUP_BUILD_SOURCE_SHA=<same exact>
APP_EMBEDDED_SOURCE_SHA=<same exact>
ELF_SHA256=<exact>
~~~

Die vollständigen lokalen Läufe werden nur nach abgeschlossenem Review, auf
finalem HEAD und nach ausdrücklicher Owner-Anweisung ausgeführt. In dieser
Planrevision werden keine Builds, Flashs oder Hardwareläufe behauptet.

Nach PASS der jeweils angeordneten digitalen Gates folgt Owner Review vor dem
Flash. Ein digitaler PASS ist kein physischer Hardware- oder Aktor-PASS.

## 7. Runtime-HWM-Kausaltest

Der spätere 96-KiB-Test läuft nur nach
`CURRENT_BASELINE_PANIC_CONFIRMED=YES` und dem 96-KiB-Digitalnachweis.

### Boot 1

Ein realer, actor-freier `esp32_bringup`-Boot wird mindestens 35 s
beobachtet. Vor beziehungsweise beim Taskstart werden aus demselben Build
aufgezeichnet:

~~~text
before_task_create.free_heap_bytes=
before_task_create.largest_free_block_bytes=
CONFIGURED_PROBE_TASK_STACK_BYTES=98304
~~~

Bei Panic, Stackoverflow, Watchdog, Brownout oder unerwartetem Reset:

~~~text
BOOT_1=FAIL
ROOT_CAUSE=UNRESOLVED
STACK_BUDGET_CORRECTION=INSUFFICIENT
STOP
~~~

Boot 2 und Boot 3 werden dann nicht ausgeführt. Es gibt keine weitere
Stackvergrößerung auf Verdacht.

### Boot 2 und Boot 3

Nur bei vollständig bestandenem Boot 1 folgen zwei unabhängige Boots:

~~~text
BOOT_1=PASS
BOOT_2=PASS
BOOT_3=PASS
BOOT_REQUALIFICATION=3_OF_3_PASS
PANIC=NO
STACK_OVERFLOW=NO
WATCHDOG=NO
BROWNOUT=NO
UNRELATED_UNEXPECTED_RESET=NO
ACTOR_RELEASE=false
~~~

Je Boot werden mindestens aufgezeichnet:

~~~text
READY_TASK_STACK_HWM_BYTES=
COMPLETION_TASK_STACK_HWM_BYTES=
~~~

Über alle drei Boots:

~~~text
MIN_OBSERVED_PROBE_TASK_HWM_BYTES=
PEAK_OBSERVED_PROBE_TASK_STACK_USED_BYTES=
    98304 - MIN_OBSERVED_PROBE_TASK_HWM_BYTES
~~~

`MIN_OBSERVED_PROBE_TASK_HWM_BYTES` ist das Minimum aller Ready- und
Completion-HWM-Werte der drei Boots. Die HWM ist primäre empirische
Stackevidenz, aber kein globaler statischer Upper Bound.

## 8. HWM-Root-Cause-Discriminator

Die historische alte Taskkapazität ist die Vergleichsschwelle:

~~~text
OLD_PROBE_TASK_STACK_BYTES=67584
98304 - 67584 = 30720
~~~

### A: empirische Bestätigung des alten Stackbudgets

Nur wenn alle Bedingungen gelten:

~~~text
CURRENT_BASE_CONTROL=PANIC_REPRODUCED
NEW_96K_BUILD=3_OF_3_PASS
ONLY_RUNTIME_RELEVANT_PROBE_CHANGE=STACK_SIZE
PANIC=NO
STACK_OVERFLOW=NO
ACTOR_RELEASE=false
MIN_OBSERVED_PROBE_TASK_HWM_BYTES < 30720
~~~

ist bewiesen:

~~~text
PEAK_OBSERVED_PROBE_TASK_STACK_USED_BYTES > 67584
~~~

Dann darf dem Owner für das Final Review vorgeschlagen werden:

~~~text
ROOT_CAUSE=CONFIRMED_STALE_DIAGNOSTIC_TASK_STACK_BUDGET
~~~

Das ist keine automatische Root-Cause-Feststellung und keine automatische
Issue-Schließung.

### B: Mitigation PASS, Ursache nicht vollständig bewiesen

Wenn `BOOT_REQUALIFICATION=3_OF_3_PASS`, aber:

~~~text
MIN_OBSERVED_PROBE_TASK_HWM_BYTES >= 30720
~~~

gilt:

~~~text
ROOT_CAUSE=NOT_FULLY_PROVEN
STACK_INCREASE_MITIGATION=PASS
OWNER_DECISION_REQUIRED=YES
~~~

### C: Panic bleibt bestehen

~~~text
ROOT_CAUSE=UNRESOLVED
STACK_BUDGET_CORRECTION=INSUFFICIENT
STOP
~~~

Keine weitere Stackvergrößerung ohne einen separat freigegebenen
Owner-Auftrag.

## 9. Elektrische #29-Verifikation gegen die R1-SSOT

Diese Messungen sind in der aktuellen Runde nicht auszuführen. Sie sind erst
nach `BOOT_REQUALIFICATION=3_OF_3_PASS`, Owner Review der Runtime-
Disposition und erneuter Freigabe für das Hardwaregate zulässig.

Die deduplizierte Messliste lautet:

1. Board-/Modulidentität, Revision soweit ermittelbar, Flashgröße,
   Partitionseigenschaften, PSRAM-Nachweis und UART-/Recoveryweg gemäß
   Issue-#29-Scope.
2. `EN / CHIP_PU`: Power-on-, Reset-, Reset-release- und actor-freier
   Normalbootpegel.
3. `GPIO0 / Boot-ROM-Download-Strap`: Reset-/Bootloaderpegel und
   reproduzierbares ROM-Download-/UART-Verhalten.
4. Für die vier ausschließlich aus dem Boardprofil aufzulösenden Rollen
   `onboard_mosfet_1` bis `onboard_mosfet_4` die SSOT-seitig zugeordneten
   GPIO-Leitungen GPIO16, GPIO17, GPIO26 und GPIO27: jeweils
   Power-on-/Reset-/Normalbootpegel ohne Verbraucher, dokumentierter
   `active_level`, `safe_boot_level` und `external_bias` sowie
   Nachweis, dass kein Kanal eine Aktorfreigabe erzeugt.
5. Resetursache, unbelasteter Boot, Heap, größter freier Block und
   Ressourcen-/Allokationsfehlerverhalten des actor-freien Bring-up-Profiles,
   soweit noch offen und vom Issue-#29-Scope verlangt.

Die Liste erzeugt keine zweite GPIO-SSOT. Die konkreten Rollen, Zahlen und
Beschaltungen werden ausschließlich aus
`config/board_profiles/esp32_32e_quad_mosfet_r1.yaml` gelesen und gegen
`docs/HARDWARE.md` sowie Issue #29 abgeglichen. Für die vier MOSFET-Kanäle
bleiben `active_level=TBD_HARDWARE`,
`safe_boot_level=TBD_HARDWARE` und
`external_bias=TBD_BOARD_CIRCUIT`, bis reale Messungen als
`confirmed_test` dokumentiert sind.

Es gibt keine elektrische Bestätigung aus Boardfamilien- oder
Designzuordnung, keinen Produkt-/Aktorbetrieb und keine
`ActuatorSafetyGateStatus::Allowed`-Freigabe.

## 10. Akzeptanz, Scope und Stop

Für diese Planrevision ist der Nachweis vollständig, wenn:

~~~text
PR129_STATE=OPEN_DRAFT
INTEGRATION_BASE_SHA=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
PR131=MERGED
PR131_MERGE_SHA=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
PR131_GPIO_SSOT_PRESERVED=YES
GPIO_SSOT_PATH=config/board_profiles/esp32_32e_quad_mosfet_r1.yaml
EXHAUSTIVE_STATIC_GATE_ATTEMPT=SUPERSEDED_KISS
ANALYZER_REVERTED=YES
MAIN_CMAKE_ISSUE29_HEAP_INSTRUMENTATION_REVERTED=YES
DEVICE_PLATFORM_ISSUE29_INSTRUMENTATION_REVERTED=YES
CURRENT_MAX_KNOWN_STATIC_PATH_BYTES=72224
CURRENT_MAX_KNOWN_STATIC_PATH_IS_GLOBAL_UPPER_BOUND=NO
CURRENT_BASE_CONTROL_BOOT_SPECIFIED=YES
DIAGNOSTIC_PROBE_TASK_STACK_BYTES=98304
HWM_ROOT_CAUSE_DISCRIMINATOR_SPECIFIED=YES
GPIO_SSOT_LEVEL_GATE_SYNC=YES
ROOT_CAUSE=UNRESOLVED
IMPLEMENTATION=NOT_STARTED_KISS_REVISION
HARDWARE_RUN=NO
LEVEL_MEASUREMENTS=NOT_RUN
ISSUE25_STARTED=NO
MERGE=NO
OWNER_PLAN_REVIEW_REQUIRED=YES
~~~

In dieser Runde sind nicht auszuführen:

~~~text
96-KiB-Stackfix
Build
Flash
Hardwareboot
Pegelmessung
#25-Arbeit
Issue-#25-Start
Merge von PR129
Ready-for-review-Wechsel
Auto-Merge
Rebase
Force-Push
~~~

Nach dem Commit dieser Revision ist der nächste Schritt ausschließlich Owner
Full Plan Review der exakten neuen Plan-SHA. Erst nach dieser Freigabe darf
der Current-base-Kontrollbuild gemäß Abschnitt 6.1 beginnen.
