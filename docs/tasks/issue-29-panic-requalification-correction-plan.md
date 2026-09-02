# Issue #29 – Panic-Requalifikation und Abschlussdisposition

> **Historischer abgeschlossener Planstatus:** Dieser Plan wurde für die
> damalige #29-Umsetzung erstellt und ist nach PR #129 / Issue-Abschluss keine
> aktuelle Statusquelle. Planzeitliche Legacy-Felder wie
> `ELECTRICAL_VERIFICATION` und `ELECTRICAL_MEASUREMENT_PASS` bleiben als
> Provenienz erhalten; aktuelle Statuswerte stehen in `docs/ROADMAP.md` und im
> geschlossenen Live-Issue #29.

Diese vollständige kanonische Revision ersetzt den zuvor ownerfreigegebenen
KISS-V2-Plan `b7d80de7d6e23fd792c2bd48eaa27052a8c61201`. Sie erhält dessen
historische Provenienz und alle bereits ausgeführten Nachweise, aktualisiert
aber die Abschlussdisposition nach dem Owner-Hardware-Review und dem
ausdrücklichen Verzicht auf Multimeter-/Boot-Pegelmessungen. Sie ist kein
Delta- oder Patchplan.

```text
PLAN_REVISION=KISS_V2_COMPLETION_AND_MULTIMETER_WAIVER
ISSUE29=OPEN
PR129=OPEN_DRAFT
BRANCH=agent/issue-29-requalification-r1
INTEGRATION_BASE_SHA=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
PREVIOUS_APPROVED_PLAN_SHA=b7d80de7d6e23fd792c2bd48eaa27052a8c61201
PREVIOUS_PLAN_STATUS=SUPERSEDED_BY_THIS_COMPLETE_REVISION

ROOT_CAUSE=CONFIRMED_STALE_DIAGNOSTIC_TASK_STACK_BUDGET
BOOT_REQUALIFICATION=3_OF_3_PASS
OWNER_96K_HARDWARE_REVIEW=PASS

OWNER_MULTIMETER_MEASUREMENT_WAIVED=YES
OWNER_ACCEPTS_UNMEASURED_BOOT_LEVEL_RESIDUAL_RISK=YES
OWNER_29_ELECTRICAL_LEVEL_MEASUREMENT_WAIVED=YES
LEVEL_MEASUREMENTS=NOT_RUN_WAIVED_BY_OWNER
MULTIMETER_REQUIRED=NO
ISSUE29_CLOSURE_BLOCKED_BY_MULTIMETER=NO

GPIO_SSOT_PATH=config/board_profiles/esp32_32e_quad_mosfet_r1.yaml
GPIO_MATRIX_STATUS=PLANNED_NOT_CONFIRMED
ELECTRICAL_VERIFICATION=PENDING
CONFIRMED_TEST=NO
ACTUATOR_RELEASE=NO
ISSUE25_STARTED=NO
MERGE=NO
```

## 1. Zweck und abschließender Scope

Issue #29 hat den actor-freien ESP32-Bring-up, die Ressourcen-/Panic-Ursache
und die sichere Nichtfreigabe von Aktoren qualifiziert. Der private
Diagnose-Task-Stack wurde als einzige runtime-relevante Kausaländerung von
67584 B auf 98304 B erhöht. Der gegen die aktuelle Integrationsbaseline
reproduzierte Kontrollpanic, drei gültige actor-freie Boots und der
Runtime-HWM-Discriminator bestätigen die Ursache.

Der Owner verzichtet für Issue #29 bewusst auf die vormals geplanten
Multimeter-/Boot-Pegelmessungen. Das hebt weder die SSOT-Status noch die
späteren Hardware- und Adaptergates der zuständigen Issues auf. Es erzeugt
insbesondere keinen elektrischen PASS.

Nicht Bestandteil dieses Plans sind:

- eine Produkt- oder Main-Task-Stackänderung;
- Probe-Fachlogik, Heap-API, Persistence- oder Fault-Seam-Änderungen;
- GPIO-/Boardprofiländerungen oder eine zweite GPIO-Matrix;
- ein realer BTS7960-/IBT-2-Adapter, Peltierbetrieb oder Aktorfreigabe;
- Pegelmessungen, eine elektrische Bestätigung oder Arbeit an Issue #25;
- Schließen von Issue #29, Ready-for-review, Merge, Rebase oder Force-Push.

## 2. Verbindliche Quellen und Statusgrenzen

Die konkreten R1-GPIOs, geplanten Pegel und externen Beschaltungen kommen
ausschließlich aus
`config/board_profiles/esp32_32e_quad_mosfet_r1.yaml`. Sie werden hier nicht
wiederholt. Das Profil enthält für BTS7960-RPWM, BTS7960-LPWM und das
gemeinsame R_EN/L_EN-Enable jeweils `safe_boot_level: low` und einen
erforderlichen 10-kOhm-Pull-down nach GND. Dies ist eine kanonische
Designanforderung und laut Owner Teil des realen Aufbaus; es ist keine
elektrische Messung und kein `confirmed_test`.

Folgende Aussagen bleiben deshalb gleichzeitig wahr:

```text
GPIO_MATRIX_STATUS=PLANNED_NOT_CONFIRMED
ELECTRICAL_VERIFICATION=PENDING
LEVEL_MEASUREMENTS=NOT_RUN_WAIVED_BY_OWNER
ELECTRICAL_MEASUREMENT_PASS=NOT_CLAIMED
ACTUATOR_RELEASE=NO
```

Die Onboard-MOSFET-Rollen mit `TBD_HARDWARE` bleiben ungeprüft. Der Owner
akzeptiert dieses nicht gemessene Boot-Level-Restrisiko für den engen
#29-Abschluss; es wird nicht in eine Produkt- oder Aktorfreigabe umgedeutet.

## 3. Historische KISS-V2-Provenienz und bestätigter Kausalbefund

Die frühere KISS-V2-Revision ersetzte den nicht mergefähigen exhaustive
Static-Analysis-Versuch. Dessen bekannte Werte bleiben historische
Diagnoseevidenz:

```text
EXHAUSTIVE_STATIC_GATE_ATTEMPT=SUPERSEDED_KISS
FULL_TRANSITIVE_STATIC_CALLGRAPH_CLOSURE=NOT_REQUIRED
STATIC_ANALYSIS_ROLE=DIAGNOSTIC_EVIDENCE_NOT_GLOBAL_UPPER_BOUND
HARDWARE_HWM_ROLE=PRIMARY_EMPIRICAL_STACK_EVIDENCE
CURRENT_MAX_KNOWN_STATIC_PATH_BYTES=72224
CURRENT_MAX_KNOWN_STATIC_PATH_IS_GLOBAL_UPPER_BOUND=NO
QUALIFIER_FAIL_CLOSED_BUG=HISTORICAL_ONLY
INDIRECT_CALL_EDGE_COLLAPSE_RISK=HISTORICAL_ONLY
```

Die historische Plain-Bring-up-Panic-Evidenz stammt von
`c1f5fbb5f19ab8e7d2c25708fe79777d523217d4`. Der verpflichtende
Current-base-Kontrollboot wurde später mit dem unveränderten Artefakt aus
`7edda30de1d39d5a4945137146ab16da530c5dc6` auf der aktuellen
Integrationsbasis ausgeführt und reproduzierte den Panic.

Der frühere 96-KiB-Lauf auf `3d7b02260e18dd203cd609d97cc66fa96e435cdf`
bleibt historische, nichtqualifizierende Diagnoseevidenz: Runtime-Log-Labels
waren gegenüber dem Kontrollstand verändert und der Capture enthielt einen
zusätzlichen Reset. Der korrigierte Kausalstand
`b14b5a0d9fa1ef5e1c453a6d8e32072d01dd30e6` stellte die Einvariablenbedingung
wieder her:

```text
OLD_PROBE_TASK_STACK_BYTES=67584
NEW_PROBE_TASK_STACK_BYTES=98304
ONLY_RUNTIME_RELEVANT_PROBE_CHANGE=STACK_SIZE
RUNTIME_LOG_FORMAT_CHANGED=NO
PRODUCT_TASK_STACK_CHANGED=NO
PRODUCT_MAIN_TASK_STACK_CHANGED=NO
PROBE_LOGIC_CHANGED=NO
HEAP_API_CHANGED=NO
FAULT_SEAM_CHANGED=NO
PERSISTENCE_CHANGED=NO
GPIO_SSOT_CHANGED=NO
```

Das exakt gebaute und geflashte korrigierte Artefakt hatte:

```text
IMPLEMENTATION_SOURCE_SHA=b14b5a0d9fa1ef5e1c453a6d8e32072d01dd30e6
ESP32_BRINGUP_BUILD_SOURCE_SHA=b14b5a0d9fa1ef5e1c453a6d8e32072d01dd30e6
APP_EMBEDDED_SOURCE_SHA=b14b5a0d9fa1ef5e1c453a6d8e32072d01dd30e6
ELF_SHA256=071bd5e6024ea7206eb378b6b18f946291726e58fc2d810106516907e3c2b487
BIN_SHA256=a1ab38d8fe60e1aeaa001b57d3c994c419989aa413d11dc750e07a8f2236989c
```

Die deterministische Capture-Prozedur erlaubte pro Boot genau eine
`rst:`-Sequenz. Drei unabhängige Boots bestanden actor-frei, ohne Panic,
Stackoverflow, Watchdog, Brownout oder Aktorfreigabe. Die kleinste gültige
Ready-/Completion-HWM war 25840 B; damit betrug die beobachtete Peak-Nutzung
72464 B und überschritt die alte 67584-B-Kapazität.

```text
CURRENT_BASE_CONTROL_67584=PANIC_REPRODUCED
BOOT_1=PASS
BOOT_2=PASS
BOOT_3=PASS
BOOT_REQUALIFICATION=3_OF_3_PASS
MIN_OBSERVED_PROBE_TASK_HWM_BYTES=25840
PEAK_OBSERVED_PROBE_TASK_STACK_USED_BYTES=72464
HWM_THRESHOLD_BYTES=30720
HWM_RESULT=3_OF_3_VALID_DISCRIMINATOR_MET
RESOURCE_GATE=PASS_CONSERVATIVE
PANIC=NO
STACK_OVERFLOW=NO
ACTOR_RELEASE=false
ROOT_CAUSE=CONFIRMED_STALE_DIAGNOSTIC_TASK_STACK_BUDGET
OWNER_96K_HARDWARE_REVIEW=PASS
```

Die Rohlogs, Hashes, digitalen Gates und die chronologische Evidenz bleiben
in `docs/ISSUE_29_MEASUREMENTS.md` und
`docs/ISSUE_29_BUILD_REPORT.md` erhalten.

## 4. Waiver der elektrischen #29-Pegelmessung

Der Owner hat die frühere Multimeterpflicht für den #29-Abschluss bewusst
aufgehoben:

```text
OWNER_29_ELECTRICAL_LEVEL_MEASUREMENT_WAIVED=YES
LEVEL_MEASUREMENTS=NOT_RUN_WAIVED_BY_OWNER
MULTIMETER_REQUIRED=NO
ISSUE29_CLOSURE_BLOCKED_BY_MULTIMETER=NO
```

Damit entfallen für #29 die bisher geplanten Messungen von EN/CHIP_PU,
GPIO0 und den Onboard-MOSFET-Bootpegeln. Nicht gemessene Werte bleiben
`NOT_RUN`; weder `LEVEL_MEASUREMENTS=PASS` noch
`ELECTRICAL_VERIFICATION=PASS` ist zulässig. Die späteren tatsächlichen
Adapter-, Aktor- und Peltiernachweise werden weder ersetzt noch vorgezogen:

- Issue #32 bleibt Eigentümer der realen Onboard-MOSFET-, Lüfter- und
  Summerhardware;
- Issue #33 bleibt Eigentümer des realen BTS7960-/IBT-2-Adapters und der
  begrenzten Peltierprüfung;
- die SSOT bleibt `PLANNED_NOT_CONFIRMED`, bis die jeweils zuständigen
  Hardware-Issues echte Tests dokumentieren.

## 5. H-Brücken-Sicherheitsvertrag an der Issue-#33-Grenze

Der vorhandene App-Pfad liefert bereits eine zusätzliche fachliche Sequenz:
`ActuatorPlanSinkDriver` deaktiviert die Gegenrichtung vor der angeforderten
Richtung; der `ActuatorPlanner` erhält seine Totzeit. Diese App-Schutzschicht
ersetzt keinen Low-Level-Hardwareinterlock, weil
`IBidirectionalActuatorSink` absichtlich zwei unabhängige Methoden besitzt
und noch kein realer ESP-IDF-Adapter existiert.

Issue #33 muss deshalb für jeden späteren realen Adapter mindestens fordern:

```text
HBRIDGE_MUTUAL_EXCLUSION_REQUIRED=YES
HBRIDGE_BREAK_BEFORE_MAKE_REQUIRED=YES
HBRIDGE_HARDWARE_ADAPTER_FAIL_CLOSED_REQUIRED=YES
HBRIDGE_BOOT_DEFAULT_DISABLED_REQUIRED=YES
MULTIMETER_REQUIRED_FOR_R1_ACCEPTANCE=NO
```

Der Adapter muss an seiner Hardwaregrenze nachweisbar gewährleisten:

1. Initialisierung ist fail-closed: RPWM AUS, LPWM AUS und gemeinsames
   R_EN/L_EN AUS.
2. Vor einer neuen Richtung wird die Gegenrichtung deaktiviert.
3. Jeder Richtungswechsel enthält einen realen All-Off-/Break-before-make-
   Zustand.
4. Fehler, unbekannter Zustand oder Initialisierungsfehler setzen beide
   Richtungen und Enable AUS.
5. Keine API- oder Adapterimplementierung kann stabil
   `RPWM=active && LPWM=active` ausgeben.
6. Adaptertests beweisen diese Invarianten auf Command-/GPIO-Ebene.
7. Die bestehende Planner-Totzeit bleibt erhalten und ist kein Ersatz für den
   Adapterinterlock.

Eine reale Polaritäts- oder Richtungszuordnung darf später über kurze,
abgesicherte Service-Pulse mit realem Peltier funktional ermittelt werden.
Die erste reale Peltierfreigabe bleibt an die in Issue #33 verlangte
Sicherung, geprüften Lüfter, gültigen Sicherheitssensoren und den
Service-/Bring-up-Modus gebunden. Vorher ist keine Multimeter-Spannungsmessung
für die R1-Abnahme erforderlich.

## 6. Akzeptanz und verbleibende Owner-Gates

Der #29-Ressourcen-/Panic-/Bring-up-Befund ist durch den Multimeterverzicht
nicht mehr blockiert. Issue #29 bleibt in dieser Revision dennoch offen, bis
der Owner die vollständige Abschlussdisposition und die getrennten
Folge-Issues bewertet. Kein Punkt dieses Plans autorisiert eine
Aktorfreigabe, elektrische Verifikation oder einen Merge.

```text
PR129_STATE=OPEN_DRAFT
ROOT_CAUSE=CONFIRMED_STALE_DIAGNOSTIC_TASK_STACK_BUDGET
BOOT_REQUALIFICATION=3_OF_3_PASS
OWNER_96K_HARDWARE_REVIEW=PASS
OWNER_29_ELECTRICAL_LEVEL_MEASUREMENT_WAIVED=YES
LEVEL_MEASUREMENTS=NOT_RUN_WAIVED_BY_OWNER
ISSUE29_CLOSURE_BLOCKED_BY_MULTIMETER=NO
ISSUE29_CLOSE=NO
ACTUATOR_RELEASE=NO
ISSUE25_STARTED=NO
MERGE=NO
OWNER_FINAL_REVIEW_REQUIRED=YES
```

Vor einem späteren realen BTS7960-/Peltier- oder Onboard-MOSFET-Test ist im
zuständigen Live-Issue ein eigener vollständiger, ownerfreigegebener Plan
erforderlich. Diese Revision erlaubt keine Firmware-, GPIO- oder
Hardwareaktion.
