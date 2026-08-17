# [E3.5] Issue #24 – Safety Core neu von `main` planen

## 0. Planstatus, Basis und Scope

Dieser Plan ist eine eigenständige Neuplanung von Issue #24 auf dem bei der
Erstellung live verifizierten `origin/main`. Er ist die normative Quelle für
eine spätere Implementierung nach ausdrücklicher Owner-Freigabe genau dieser
Plan-SHA.

```text
Repository: ManuEngineer/ESP32-Fermentationsschrank
Issue: #24 [E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion
Base: origin/main @ b8eae5f4da5f2666b5a9bda333d115254c4db5b2
Branch: agent/issue-24-safety-core-replan-v2
Planpfad: docs/tasks/issue-24-safety-core-replan.md
Implementation: NOT_STARTED
```

Live-Revalidierung vor dieser abschließenden Planrevision am 2026-08-17:

```text
PR #110: OPEN / Draft
PR-Branch: agent/issue-24-safety-core-replan-v2
PR-HEAD vor dieser Revision: e3a7586032542d9c3e89a3c89f68f7d0f36efa2e
PR-Base: main @ b8eae5f4da5f2666b5a9bda333d115254c4db5b2
Issue #24: OPEN
origin/main: b8eae5f4da5f2666b5a9bda333d115254c4db5b2
Context refresh: FULL gegen origin/main und Live-Issue/PR
```

Die Branchbasis ist direkt `origin/main`; weder PR #107 noch PR #108 noch PR
#109 noch ein anderer früherer Issue-24-Plan ist eine normative Quelle. PR #107,
#108 und #109 bleiben unverändert offen und historisch; PR #109 ist durch PR
#110 superseded. Der neue PR ändert keine
Produktionsdatei, keinen Test, keine Konfigurationsdatei, keine Hardware und
keine Bibliothek.

### Verbindliche Ownerentscheidungen

```text
S3 = RecoverIfProvable
Y4 = Terminal
P1 < O2 < S3 < Y4
FaultIdentity = FaultCode + FaultSource
```

S3-Recovery verwendet den vorhandenen #17-Fallback nur RAM-seitig. Der
persistent gespeicherte #17-Head bleibt unverändert, das Wireformat bleibt
Schema 3 und #18 bleibt die einzige fachliche Recoveryautorität. Y4 setzt eine
rebootfeste Terminalpflicht; der alte Lauf endet ausschließlich über den
kanonischen #17-`NoActiveRun`/`STANDBY`-Tombstone.

## 1. Live-Vertragsabgleich

Die folgenden Verträge wurden am genannten Base-SHA direkt gelesen und sind
für die Umsetzung zu verwenden:

| Bereich | Aktueller Vertrag auf `main` | Issue-24-Grenze |
|---|---|---|
| State Store | `lib/device_platform/src/state_store.hpp:20-90`, vier typisierte Write-Ausgänge und atomarer Vollwert pro Key | SafetyState und Marker bleiben anwendungsseitige Records; `WriteError`/`CapacityError` bedeuten sicher unverändert, `CommitOutcomeUnknown` erfordert Readback |
| Storage-Port | `storage_envelope.*`, `storage_slot_candidates.*`, generische technische Slot-/Envelope-Prüfung | keine Safety-Fachtypen in `device_platform`; feste Keys und Recordbedeutung in `fermentation_app` |
| #17 Wire | `run_persistence_contract.hpp:19-145`, Schema 3; `run_persistence_codec.cpp:26-29`, Recordtypen 7/8, Checkpoint max. 8240, Head max. 256 | kein Schema 4 und kein Safety-Fallback-Wirefeld |
| #17 Store | `run_persistence_store.cpp:8-17`, `rh0`, `rc0`, `rc1` | S3 benutzt keine neuen #17-Keys; Safety hat eigene Records |
| #17 Load | `run_persistence_coordinator.cpp:257-497` | bestehendes `FallbackRecovered` bleibt ein eigenständiger #18-Pfad; `Current` mit Fault plus S3-Intent erhält nur einen zusätzlichen read-only Handoff |
| #17 Activation | `run_persistence_coordinator.hpp:214-218`, `run_persistence_coordinator.cpp:760-925` | neuer Safety-Handoff darf nur vor der bestehenden forward-only Recoveryaktivierung RAM-seitig auswählen |
| #17 Write | `run_persistence_coordinator.cpp:1404-1685` | Prepared -> Slot -> Committed bleibt vorwärts und crash-sicher |
| #18 Recovery | `docs/RUN_PERSISTENCE.md`, `docs/RECOVERY_AND_INTERRUPTION.md`, `RunRecoveryCoordinator` | Zeit, Phase, Sensorwahl, Progress und Terminalentscheidung bleiben #18 |
| Prozess | `process_state_machine.hpp:15-150`, `process_state_machine.cpp:230-285` | Fault -> Standby erhält einen eigenen terminalen Owner-/Recoverygrund; kein `NoActiveRun + Fault` |
| #21 Sensorwahl | `sensor_selection_types.hpp`, `sensor_selection.cpp:537-759` | `AirFallbackActive` ist ein vorhandener Runtimezustand; #24 dupliziert keine #21-FSM |
| #22/#23 Aktor | `actuator_plan_types.hpp:12-35,286-307`, `temperature_control_orchestrator.hpp`, `actuator_planner.cpp:1040-1080` | zentrale SafetyDirective, kein frei gelieferter `Allowed`-Wert, #23 Timing bleibt Eigentümer |
| Reset/Watchdog-Plattform | ESP-IDF 6.0.2 öffentliche `esp_system.h`-/`esp_task_wdt.h`-Verträge und `CONFIG_ESP_INT_WDT`, `CONFIG_ESP_INT_WDT_TIMEOUT_MS`, `CONFIG_ESP_TASK_WDT_*`, `CONFIG_ESP_SYSTEM_PANIC` | `device_platform` liefert `IResetCauseProvider`/`IResetController`; ESP-IDF mappt native Ursachen und nutzt TWDT/IWDT, `fermentation_app` bleibt ESP-IDF-frei; kein `esp_private/esp_int_wdt.h` |
| #56/#57 Config | `configuration_recovery_service.hpp:17-71`, `configuration_recovery_service.cpp:535-729`, `docs/CONFIGURATION_PERSISTENCE.md:773-808` | #24 konsumiert `ConfigurationRecoveryStatus`/`safetyProducer`; privater `FactoryNoveltyProof` bleibt #57-intern |
| Akzeptanz | `docs/ACCEPTANCE_TESTS.md` | alle Pflichtinjektionen werden in der Matrix in Abschnitt 18 klassifiziert |

Der neue Plan übernimmt keine unverifizierte Behauptung aus älteren PR-Plänen.

## 2. Nicht verhandelbare Architekturgrenzen

### 2.1 Eine Safety-Autorität

`SafetyFaultService` ist die einzige mutable Safety-/Fault-Autorität. Sie hält
im RAM und im SafetyState:

- aktiven Faultbestand und FaultCatalog-Auswertung;
- persistente S3-/Y4-Latches, InstanceIds und `persistentFaultRevision`;
- CauseClear-, Relapse-, Ack- und Target-Resetbewertung;
- Primary-/Follow-up-Beziehung;
- `runRecoveryForbidden`/`runTerminalRequired`;
- Restart-Episode, RestartIntent und `safeBootRequired`;
- Safety-Persistenz- und Counterstatus;
- die aus allen Ursachen aggregierte `ActuatorSafetyDirective`.

`RunCommandState`, `ProcessStateMachine`, `ConfigurationService`, Planner und
UI tragen davon nur typisierte Projektionen für ihre eigenen bestehenden
Verträge. Keine zweite Faultwahrheit, Recovery-State-Machine oder
Run-Persistenzengine entsteht.

### 2.2 Keine verbotenen Abkürzungen

Der Plan führt ausdrücklich nicht ein:

- Last-origin-wins, volatile `CorrelationKey` als FaultIdentity oder Eviction
  aktiver Safety-Latches;
- Safety-Head-Rollback, Fallback-Promotion durch Rückschreiben oder ein
  `safetyFallbackPromotion`-Feld;
- eine RunPersistence-Schemaversion nur für Issue #24;
- `Fault -> alte aktive Phase`, `NoActiveRun + Fault` oder einen alten
  Runresume nach Y4;
- caller-supplied `Allowed`, `safetyAllows...` oder
  `authorizationSatisfied` als produktiven Vertrauensnachweis;
- erfundene physische Fan-/Aktor-Rückmeldung;
- direkte H-Brücken-/GPIO-Ansteuerung aus FaultCore;
- einen zweiten Eventbus, Journalbesitzer, Recoverykern oder Persistenzkern.

### 2.3 Mechanische Trust-Boundary für Commands und Evidenz

Die aktuellen öffentlichen Requestfelder `safetyAllowsStart` in
`ProgramStartRequest` und `ManualStartRequest`, `safetyAllowsCooling` in
`StopRequest` und `CompletionRequest`, `safetyAllowsChange` in
`RunAdjustmentCommandRequest` und `SensorSelectionCommandRequest` sowie die
öffentlichen `airSensorValid`, `coolingSensorValid` und `productSensorValid`
werden im produktiven Zielvertrag nicht als Safetywahrheit akzeptiert. Die
Command-Schicht fragt eine interne, nur von `SafetyFaultService` erzeugbare
Read-only-`SafetyView` ab; öffentliche Plain-Structs mit `allowed=true` oder
gültigen Sensorbooleans existieren für diesen Pfad nicht. Die Sensorqualität
kommt ausschließlich aus den kanonischen #20/#21-Snapshots und wird nicht aus
einem Request übernommen.

`RunCommandState::criticalSafetyEventPending` bleibt höchstens eine abgeleitete
Projektion für bestehende #15-Verträge und darf keine zweite Safetyautorität
werden. UI, Web und Transport können mit `true` weder Start, Stop, Completion,
Adjust, SensorSelection noch Aktortest freigeben.

`FaultResetEvaluation` bleibt unverändert die reine, fachliche #15-Auswertung
von Target, CauseClear, Safetybedingungen und Blockern. Insbesondere ist ein
frei geliefertes `authorizationSatisfied` weder Teil dieser Evaluation noch
eine Vertrauensquelle. Erst `SafetyFaultService` beziehungsweise der interne
CommandCoordinator bildet aus der aktuellen `FaultResetEvaluation`, einem
nicht öffentlich konstruierbaren `fermentation_app::ServiceResetProof` und der
aktuellen `SafetyState`-Revision/-Lineage die endgültige TargetReset-Entscheidung.
Der Proof bindet Auth-Session/Generation, Target-Instance, aktuelle Lineage und
erwartete `persistentFaultRevision`; UI, Web und Transport können weder
Evaluation noch Proof als Freigabe fabrizieren. Typ und Minting gehören
ausschließlich `fermentation_app`; `device_platform_test_support` kennt weder
den Typ noch Fault-/Lineagefelder. Bis ein echter Service-PIN/Auth-Producer
existiert, erzeugt der Produktpfad keinen S3-/Y4-Resetproof. App-Tests erzeugen
ihn nur über einen app-internen Testhelper aus generischer Test-Auth-/
Sessionevidenz.

Der direkte Main-Abgleich zeigt noch das öffentliche
`FaultResetEvaluation::authorizationSatisfied`-Feld. Die #24-Integration
behält die #15-Target-/Safetysemantik, akzeptiert dieses Feld aber weder aus
`FaultResetRequest` noch aus einem Transportobjekt als Freigabe. Sie bildet die
Evaluation intern aus kanonischer Evidenz und ergänzt sie erst dort mit dem
unforgeable Proof; ein äußerer Kompatibilitätsrest darf nur Diagnose sein und
nie die finale TargetReset-Entscheidung beeinflussen.

## 3. FaultCatalog und Identität

### 3.1 Wire- und Identitätsregeln

`FaultCode` ist ein stabiler `uint16` und `FaultSource` ein stabiler `uint8`.
Die Kombination ist die einzige fachliche Identity. Measurement-, Run-,
Control-, Planner- und Zeitrevisionen gehören ausschließlich in Diagnostik.
P1/O2 sind transient und bootlokal; S3/Y4 sind persistent und rebootstabil.
P1/O2 verbrauchen weder persistente InstanceIds noch
`persistentFaultRevision`.

Die Werte werden als geschlossene Compile-time-Tabelle implementiert; unbekannte
Codes, Quellen und Reserved-Bits werden beim Decode abgelehnt.

Die `FaultSource`-Wirewerte sind ebenfalls geschlossen und stabil:

| Wirewert | FaultSource |
|---:|---|
| `0x01` | `ProcessDeviation` |
| `0x02` | `TimeSource` |
| `0x03` | `AirSensor` |
| `0x04` | `CoolingSensor` |
| `0x05` | `ProductSensor` |
| `0x06` | `SensorContradiction` |
| `0x07` | `ThermalResponse` |
| `0x08` | `NonCriticalPersistence` |
| `0x09` | `ThermalIntervention` |
| `0x0A` | `ThermalControl` |
| `0x0B` | `OuterFan` |
| `0x0C` | `InnerFan` |
| `0x0D` | `HBridge` |
| `0x0E` | `ActuatorOutput` |
| `0x0F` | `ActuatorPlanner` |
| `0x10` | `SensorTask` |
| `0x11` | `SafetyTask` |
| `0x12` | `ActuatorTask` |
| `0x13` | `MainTask` |
| `0x14` | `RunPersistence` |
| `0x15` | `Configuration` |
| `0x16` | `PersistencePath` |
| `0x17` | `RestartSupervisor` |
| `0x18` | `SafetyBoot` |
| `0x19` | `SafetyCore` |
| `0x1A` | `Reserved` |
| `0x1B` | `ControlTask` |
| `0x1C` | `RunPersistenceCounter` |
| `0x1D` | `ConfigurationCounter` |

Jeder Wirewert ist genau einmal im FaultSource-Catalog registriert; jede
FaultIdentity referenziert genau einen dieser Werte. Unbekannte oder
reservierte Werte führen zu Decode-Failure, `Y4_UNKNOWN_SAFETY_STATE` und
`SAFE_BOOT`; C++-Enum-Reihenfolge ist weder Wire- noch Prioritätsvertrag.

### 3.2 Policylegende

Die Matrix verwendet folgende exakt definierte Kürzel:

```text
Gate: IS = ImmediateStop, U = Unresolved, A = Allowed
Fan:  FO = ForceOff, FI = ForceOn, PM = PlannerManaged
Auth: AUTO = bestehende Auto-/Operatorpolicy, SVC = Service
Clear: QE = kanonische Producer-Evidenz, Q21 = Issue-21-Evidenz,
       Q23 = Issue-23-Evidenz, QF = konkrete Funktions-/Hardwareevidenz,
       QS = Safety-Storage-/Bootoracle, QL = History-/Lineageoracle,
       QR = RunPersistence-/Tombstone-Evidenz, QRS = RestartSupervisor-
       Stabilitätsevidenz
Run:  KEEP = Zustand nur sicher weiterführen,
      RESET_THEN_RECOVER = Target-Reset, danach ausschließlich provable S3-Recovery,
      TERM = aktiven Run terminalisieren, NONE = kein aktiver Run
```

`FI` wird nur bei einer elektrisch nicht selbst betroffenen Ausgabe verwendet.
Bei einem elektrischen Fanfehler ist der jeweilige Fan immer `FO`; dadurch wird
kein Einschalten eines möglicherweise kurzgeschlossenen Ausgangs behauptet.
`PM` ist nur eine zulässige Planner-Projektion, wenn kein höherer Safetygrund
für diesen Fan besteht.

`NO_RELEASE_UNTIL_REQUAL` bedeutet: keine neue Aktorfreigabe, bis die
kanonische #20/#21-Evidenz wieder gültig ist. Es ist keine eigene Fault- oder
Recovery-Autorität.

Die stabilen symbolischen Namen sind ebenfalls Teil des Catalogs und werden
nicht aus der C++-Enum-Reihenfolge abgeleitet:

| Code | Symbol |
|---|---|
| `0x0101` | `P1_PROCESS_DEVIATION` |
| `0x0102` | `P1_TIME_SOURCE` |
| `0x0201` | `O2_AIR_SENSOR` |
| `0x0202` | `O2_COOLING_SENSOR` |
| `0x0203` | `O2_PRODUCT` |
| `0x0204` | `O2_SENSOR_CONTRADICTION` |
| `0x0205` | `O2_THERMAL_RESPONSE` |
| `0x0206` | `O2_NONCRITICAL_PERSISTENCE` |
| `0x0301` | `S3_AIR_SENSOR` |
| `0x0302` | `S3_COOLING_SENSOR` |
| `0x0303` | `S3_SENSOR_CONTRADICTION` |
| `0x0304` | `S3_THERMAL_INTERVENTION` |
| `0x0305` | `S3_THERMAL_HARD_EMERGENCY` |
| `0x0306` | `S3_OUTER_FAN_FUNCTIONAL` |
| `0x0307` | `S3_OUTER_FAN_ELECTRICAL` |
| `0x0308` | `S3_INNER_FAN_FUNCTIONAL` |
| `0x0309` | `S3_INNER_FAN_ELECTRICAL` |
| `0x030A` | `S3_HBRIDGE_CONFLICT` |
| `0x030B` | `S3_UNEXPECTED_ACTUATOR_OUTPUT` |
| `0x030C` | `S3_PELTIER_UNSAFE_OUTPUT` |
| `0x030D` | `S3_ACTUATOR_WATCHDOG` |
| `0x030E` | `S3_SENSOR_TASK_STALL` |
| `0x030F` | `S3_SAFETY_TASK_STALL` |
| `0x0310` | `S3_ACTUATOR_TASK_STALL` |
| `0x0311` | `S3_MAIN_TASK_STALL` |
| `0x0312` | `S3_CONTROL_TASK_STALL` |
| `0x0313` | `S3_PERSISTENCE_PATH_STALL` |
| `0x0401` | `Y4_RUN_PREPARED_INTERRUPTED` |
| `0x0402` | `Y4_RUN_NOT_RECONSTRUCTIBLE` |
| `0x0403` | `Y4_RUN_ORPHANED_STATE` |
| `0x0404` | `Y4_RUN_READ_FAILED` |
| `0x0405` | `Y4_RUN_STORE_INTEGRITY` |
| `0x0406` | `Y4_CONFIGURATION_UNAVAILABLE` |
| `0x0407` | `Y4_CONFIGURATION_INTEGRITY` |
| `0x0408` | `Y4_CONFIGURATION_COMMIT_INDETERMINATE` |
| `0x0409` | `Y4_UNKNOWN_SAFETY_STATE` |
| `0x040A` | `Y4_RESTART_LOOP` |
| `0x040B` | `Y4_SAFETY_BOOT` |
| `0x040D` | `Y4_INTERNAL_SAFETY` |
| `0x040E` | `Y4_RUN_PERSISTENCE_COUNTER` |
| `0x040F` | `Y4_CONFIGURATION_COUNTER` |

Damit gilt unveränderlich: `kCatalogCount=41`, davon 8 transiente und 33
persistente Identities; `kFaultSourceCount=28` gültige Werte (0x1A bleibt
Reserved) und
`kDisplayPriorityCount=41`. Diese drei Werte werden jeweils per
`static_assert` gegen die vollständigen Tabellen geprüft.

### 3.3 Vollständige erlaubte Identity-Matrix

Jede Tabellenzeile ist eine eigenständige begrenzte Ursache. Policy, Reset,
CauseClear, Runwirkung und Producerstatus sind damit vollständig bestimmt.

| Code | FaultSource | Klasse / Persistenz | AutoRearm | Gate; outer; inner; Peltier | Restart | Reset / CauseClear | Runpolicy | Producer |
|---|---|---|---|---|---|---|---|---|
| `0x0101` | `ProcessDeviation` | P1 / transient | ja | `A; PM; PM; aus aktueller Regelung` | nein | AUTO / QE | KEEP | real / injection |
| `0x0102` | `TimeSource` | P1 / transient | ja | `A; PM; PM; keine Safetywirkung` | nein | AUTO / QE | KEEP | real / injection |
| `0x0201` | `AirSensor` | O2 / transient | ja nach STALE-Requalifikation | `IS; FI; PM; AUS` | nein | AUTO / QE: Air VALID, CRC, Plausibilität | KEEP, danach normal neu qualifizieren | #20 real, injection |
| `0x0202` | `CoolingSensor` | O2 / transient | ja nach STALE-Requalifikation | `IS; FI; PM; AUS` | nein | AUTO / QE: Cooling VALID, CRC, Plausibilität | KEEP, danach normal neu qualifizieren | #20 real, injection |
| `0x0203` | `ProductSensor` | O2 / transient | #21-Policy | `A*; PM; PM; #21-Directive` | nein | AUTO/Operator / Q21: #21 entscheidet Fallback oder Return | KEEP im validierten Air-Fallback, sonst #21 | #21 real, injection |
| `0x0204` | `SensorContradiction` | O2 / transient | nur nach eindeutiger Auflösung | `IS; FI; PM; AUS` | nein | AUTO / QE: #20/#21 widerspruchsfrei | `NO_RELEASE_UNTIL_REQUAL` | #20/#21 real, injection |
| `0x0205` | `ThermalResponse` | O2 / transient | höchstens begrenzte Diagnosewiederholung | `IS; FI; PM; AUS` | nein | AUTO / QE: Sensor-/Aktor-/Zeitprüfung | KEEP nur nach neuer Evidenz | #22/#23 real, injection |
| `0x0206` | `NonCriticalPersistence` | O2 / transient | ja nach Storeprüfung | `A; PM; PM; keine kritische Wirkung` | nein | AUTO / QS: nichtkritischer Fehler behoben | KEEP | #19/#17 real, injection |
| `0x0301` | `AirSensor` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: Air stabil gültig | `RESET_THEN_RECOVER` | #20 real, injection |
| `0x0302` | `CoolingSensor` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: Cooling stabil gültig | `RESET_THEN_RECOVER` | #20 real, injection |
| `0x0303` | `SensorContradiction` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: Sicherheitswiderspruch aufgelöst | `RESET_THEN_RECOVER` | #20/#21 real, injection |
| `0x0304` | `ThermalIntervention` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: Eingriffsgrenze verlassen, alle Checks | `RESET_THEN_RECOVER`; SafetyRecovery-Handoff | #22/#20 real, injection |
| `0x0305` | `ThermalIntervention` | S3 / persistent | nein | `IS; FI; FI; AUS` | nein | SVC / QE: Notgrenze-/Hardwareprüfung | `RESET_THEN_RECOVER`; keine Gegenrichtung | #20/#22 real, injection |
| `0x0306` | `OuterFan` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QF: konkrete Funktions-/Thermikevidenz | `RESET_THEN_RECOVER` | injection-only/future producer; #23 beweist keinen mechanischen Lauf |
| `0x0307` | `OuterFan` | S3 / persistent | nein | `IS; FO; FI; AUS` | nein | SVC / QF: elektrische Sicherheit nachgewiesen | `RESET_THEN_RECOVER` | injection-only/future electrical producer |
| `0x0308` | `InnerFan` | S3 / persistent | nein | `IS; FI; FO; AUS` | nein | SVC / QF: konkrete Funktions-/Thermikevidenz | `RESET_THEN_RECOVER` | injection-only/future producer; #23 beweist keinen mechanischen Lauf |
| `0x0309` | `InnerFan` | S3 / persistent | nein | `IS; FI; FO; AUS` | nein | SVC / QF: elektrische Sicherheit nachgewiesen | `RESET_THEN_RECOVER` | injection-only/future electrical producer |
| `0x030A` | `HBridge` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / Q23: keine Doppelrichtung, Diagnose gesund | `RESET_THEN_RECOVER` | #23 real, injection |
| `0x030B` | `ActuatorOutput` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QF: Ausgangszustand erklärt | `RESET_THEN_RECOVER` | injection-only ohne Hardwareproducer |
| `0x030C` | `ActuatorOutput` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QF: Strom-/Ausgangsevidenz, falls vorhanden | `RESET_THEN_RECOVER` | injection-only/Commissioning |
| `0x030D` | `ActuatorPlanner` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / Q23: Watchdogepisode beendet, Ausgang geprüft | `RESET_THEN_RECOVER` | #23 request-watchdog real; kein Task-Restart |
| `0x030E` | `SensorTask` | S3 / persistent | nein | `IS; FI; PM; AUS` | `SafetyTaskRecovery` | SVC / QE: Task/Driver requalifiziert | `RESET_THEN_RECOVER` | #24 SafetyWatchdogProducer, supervised only + injection |
| `0x030F` | `SafetyTask` | S3 / persistent | nein | `IS; FI; PM; AUS` | `SafetyTaskRecovery` | SVC / QS: Safety-/Fehleraufgabe requalifiziert | `RESET_THEN_RECOVER` | #24 SafetyWatchdogProducer, supervised only + injection |
| `0x0310` | `ActuatorTask` | S3 / persistent | nein | `IS; FI; PM; AUS` | `SafetyTaskRecovery` | SVC / Q23: Aktoraufgabe requalifiziert | `RESET_THEN_RECOVER` | #24 SafetyWatchdogProducer, supervised only + injection |
| `0x0311` | `MainTask` | S3 / persistent | nein | `IS; FI; PM; AUS` | `SafetyTaskRecovery` | SVC / QS: Main-/Task-Zustand requalifiziert | `RESET_THEN_RECOVER` | #24 SafetyWatchdogProducer, supervised only + injection |
| `0x0312` | `ControlTask` | S3 / persistent | nein | `IS; FI; PM; AUS` | `SafetyTaskRecovery` | SVC / QS: Regelaufgabe requalifiziert | `RESET_THEN_RECOVER` | #24 SafetyWatchdogProducer, supervised only + injection |
| `0x0313` | `PersistencePath` | S3 / persistent | nein | `IS; FI; PM; AUS` | `SafetyTaskRecovery` | SVC / QS: kritischer Persistenzpfad requalifiziert | `RESET_THEN_RECOVER` | #24 SafetyWatchdogProducer, supervised only + injection |
| `0x0401` | `RunPersistence` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QR: Tombstone durable | `TERM(active); NONE(no active)` | #17 real, injection |
| `0x0402` | `RunPersistence` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QR: Tombstone durable | `TERM(active); NONE(no active)` | #17 real, injection |
| `0x0403` | `RunPersistence` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QR: Tombstone durable | `TERM(active); NONE(no active)` | #17 real, injection |
| `0x0404` | `RunPersistence` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QR: Store wieder les-/schreibbar und Tombstone | `TERM(active); NONE(no active)` | #17 real, injection |
| `0x0405` | `RunPersistence` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QR: Schema/Epoch/Kapazität geklärt, Tombstone | `TERM(active); NONE(no active)` | #17 real, injection |
| `0x0406` | `Configuration` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: #57 `RuntimeReady` | `TERM(active); NONE(no active)` | #57 real, injection |
| `0x0407` | `Configuration` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: #57 Integrity-/Schemaursache behoben | `TERM(active); NONE(no active)` | #57 real, injection |
| `0x0408` | `Configuration` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: #56/#57 Indeterminate aufgelöst | `TERM(active); NONE(no active)` | #56/#57 real, injection |
| `0x0409` | `PersistencePath` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QS: SafetyState redundant gesund | `TERM(active); NONE(no active)` | #24 real, injection |
| `0x040A` | `RestartSupervisor` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QRS: 30 Minuten aktuelle Bootzeit, kein neuer abnormaler Reset, danach SVC | `TERM(active); NONE(no active)` | #24 real, injection |
| `0x040B` | `SafetyBoot` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QL: aktuelle History-/Bootintegrität nachgewiesen; nicht RestartLoop | `TERM(active); NONE(no active)` | #24 real, injection |
| `0x040D` | `SafetyCore` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QS: einzige Aggregate-Invariante „SafetyCore intern inkonsistent“ nachgewiesen | `TERM(active); NONE(no active)` | #24 real, injection |
| `0x040E` | `RunPersistenceCounter` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: #17-Counter behoben und Readback qualifiziert | `TERM(active); NONE(no active)` | #17 real, injection |
| `0x040F` | `ConfigurationCounter` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: #56/#57-Counter behoben und Readback qualifiziert | `TERM(active); NONE(no active)` | #56/#57 real, injection |

`0x040C SafetyCounterExhausted` ist keine FaultIdentity und kein
`FaultRecord`: safety-eigene High-Watermark-Exhaustion ist ein aus dem gültigen
Maximalwert abgeleiteter Runtime-/Bootstatus. Fremde Counter werden fachlich
nach realer Domain getrennt: #17 verwendet `0x040E`, #56/#57 verwenden
`0x040F`; eine weitere Domain erhält nur bei einem realen Producer eine eigene
Identity. `Y4_INTERNAL_SAFETY/SafetyCore` ist dagegen ausdrücklich genau eine
aggregierte Invariante „SafetyCore state is internally inconsistent“; interne
Detailgründe sind Diagnostik und keine weiteren gleichzeitig behaupteten
Safetyursachen. Die Matrix enthält damit
`kMaxPersistentFaultRecords = 33` (19 S3 und 14 Y4).
P1/O2 können nicht in diesen Bestand gelangen. Ein Y4-Record ist bis zum
Target-Reset ein Safety-Latch; ein CauseClear markiert ihn nur als
`causeCleared`, entfernt ihn aber nicht.

### 3.4 Deterministische Intra-Class-Priorität

`displayPriority` ist ein eigener stabiler Catalogwert und keine Ableitung aus
der Enum-Reihenfolge. Die vollständige Zuordnung lautet:

| Code | displayPriority | Code | displayPriority |
|---|---:|---|---:|
| `0x0101` | 10 | `0x0102` | 20 |
| `0x0201` | 100 | `0x0202` | 110 |
| `0x0203` | 120 | `0x0204` | 130 |
| `0x0205` | 140 | `0x0206` | 150 |
| `0x0301` | 200 | `0x0302` | 210 |
| `0x0303` | 220 | `0x0304` | 230 |
| `0x0305` | 240 | `0x0306` | 250 |
| `0x0307` | 260 | `0x0308` | 270 |
| `0x0309` | 280 | `0x030A` | 290 |
| `0x030B` | 300 | `0x030C` | 310 |
| `0x030D` | 320 | `0x030E` | 330 |
| `0x030F` | 340 | `0x0310` | 350 |
| `0x0311` | 360 | `0x0312` | 370 |
| `0x0313` | 380 | `0x0401` | 400 |
| `0x0402` | 410 | `0x0403` | 420 |
| `0x0404` | 430 | `0x0405` | 440 |
| `0x0406` | 450 | `0x0407` | 460 |
| `0x0408` | 470 | `0x0409` | 480 |
| `0x040A` | 490 | `0x040B` | 500 |
| `0x040D` | 510 | `0x040E` | 520 |
| `0x040F` | 530 |  |  |

Ein höherer numerischer `displayPriority`-Wert bedeutet innerhalb derselben
`FaultClass` eine höhere sichtbare Priorität. Die `FaultClass` dominiert immer
vor `displayPriority`; die Priorität entscheidet nur die sichtbare Reihenfolge
innerhalb der Klasse. Jede Klasse besitzt ausschließlich eindeutige Werte,
und `static_assert` prüft Eindeutigkeit, Catalogcount, alle Code-/Source-Paare
und den vollständigen Prioritätsbestand.

## 4. SafetyState: vollständiger Wirevertrag

### 4.1 Recordfamilie und Envelope

```text
RecordTypeId       = 9 (uint16 wire)
SchemaVersion      = 1 (uint32 wire)
SafetyStorageEpoch = 1 (uint64 wire, unabhängig von #57 ConfigurationEpoch)
Keys               = sf0, sf1
Slots              = exakt 2
max Envelope       = 1024 Byte
utcUnixSeconds     = nullopt
Envelope VersionValue = recordRevision (uint64, ab 1)
```

Die Keys sind statisch und werden ausschließlich als `StateStoreKey` erzeugt.
Es gibt keine dynamischen Safety-Keys. Ein technischer Envelope hat den
bestehenden Envelope v1 von `storage_envelope.*`; seine maximal geplante Größe
ist `33 Byte Header + 824 Byte Payload + 4 Byte CRC = 861 Byte`, also deutlich
unter dem 1024-Byte-Limit.

### 4.2 Payload-Feldreihenfolge

Alle Felder sind Big-Endian ohne C++-Padding. Der Payload ist immer exakt
`kSafetyStatePayloadBytes = 824` Byte:

| Offset | Wirefeld | Typ / Wert |
|---:|---|---|
| 0 | `formatTag` | `uint16 = 0x5346` (`SF`) |
| 2 | `safetyHistoryEpoch` | `uint32`, nur innerhalb einer Lineage geordnet; bei Totalverlust nicht global erhöht |
| 6 | `nextPersistentInstanceId` | `uint32`, erste vergebene ID 1; kein Wrap |
| 10 | `persistentFaultRevision` | `uint32`, Start 0; kein Wrap |
| 14 | `bootSequence` | `uint32`, erster erfolgreicher Erstboot 1; kein Wrap |
| 18 | `abnormalRestartCount` | `uint8`, nur 0..3 |
| 19 | `safeBootRequired` | `uint8`, `0=false`, `1=true` |
| 20 | `restartIntentState` | `0=None`, `1=Prepared` |
| 21 | `restartIntentKind` | `0=None`, `1=SafetyTaskRecovery`, `2=S3RunRecovery` |
| 22 | `restartIntentSourceInstanceId` | `uint32`, bei None 0 |
| 26 | `restartAttempted` | `uint8`, 0/1 |
| 27 | `restartOutcome` | `0=None`, `1=Rejected` |
| 28 | `runRecoveryForbidden` | `uint8`, 0/1 |
| 29 | `historyLineageState` | `uint8`, `0=Continuous`, `1=PartialSuccessor`, `2=TotalDiscontinuity` |
| 30 | `persistentFaultCount` | `uint8`, 0..33 |
| 31 | `reserved0` | `uint8 = 0` |
  | 32 | `FaultRecord[0..32]` | exakt 33 Records à 24 Byte |

`recordRevision` ist ausschließlich Envelope `VersionValue`; er wird nicht
doppelt im Payload gespeichert. Der Payload ist deshalb exakt
`32 + (33 * 24) = 824 Byte`. Leere FaultRecord-Slots sind vollständig null.
Die FaultRecords müssen aufsteigend nach `instanceId` folgen; die Zahl der
nichtleeren Records muss exakt `persistentFaultCount` sein.

### 4.3 FaultRecord-Wireformat

Jeder 24-Byte-Record hat die feste Reihenfolge:

```text
instanceId            uint32
faultCode             uint16
faultSource           uint8
flags                 uint8   bit0=causeCleared; bit1..7=0
firstSeenBootSequence uint32
firstSeenMonotonicMs  uint64
primaryInstanceId     uint32
```

Klasse, Persistenz, Gate, Fan- und Auth-Policy werden aus dem Catalog abgeleitet
und nicht redundant im Wire gespeichert. `primaryInstanceId` ist 0 oder eine
plausible, zuvor vergebene ID derselben `safetyHistoryEpoch`; Follow-up-Listen
und -Zähler werden nicht persistiert. Ein Ack-Status ist absichtlich nicht im
SafetyState.

### 4.4 Decode- und Crossfield-Validator

Ein SafetyState ist nur gültig, wenn zusätzlich zu Envelope/CRC/Schema/Epoch
alle folgenden Regeln gelten:

- `formatTag`, Reserved-Bits und alle Enumwerte sind bekannt;
- `persistentFaultCount ==` Zahl der vorhandenen Records;
- alle leeren Records sind vollständig null;
- `instanceId` ist streng aufsteigend und jede ID ist `< nextPersistentInstanceId`;
- `firstSeenBootSequence > 0` und `<= bootSequence`;
- `primaryInstanceId == 0` oder `< instanceId` und nur innerhalb derselben
  `SafetyHistoryLineage`;
- jeder Code/Source entspricht genau einer Catalog-Identity und persistent ist;
- keine doppelte FaultIdentity und keine doppelte InstanceId existiert;
- `nextPersistentInstanceId` ist nie 0, außer der Record ist ein ausdrücklich
  erkanntes fabrikneues Rohformat vor Erstinitialisierung;
- `restartIntentState=None` erzwingt `kind=None`, `source=0`,
  `restartAttempted=0`, `outcome=None`;
- `restartIntentState=Prepared` erzwingt `kind=SafetyTaskRecovery` oder
  `kind=S3RunRecovery`,
  `0 < source < nextPersistentInstanceId`, `restartAttempted` 0/1 und
  `outcome` None oder Rejected;
- `restartOutcome=Rejected` erzwingt immer `restartAttempted=1`; ein abgelehnter
  Port darf keinen unverbrauchten Restartintent hinterlassen;
- bei `kind=SafetyTaskRecovery` muss `source` exakt auf den aktuell aktiven,
  restartfähigen Task-/Liveness-S3-Record derselben `safetyHistoryEpoch`
  zeigen; eine physische Sensor-/Fan-/Aktorursache ist dafür unzulässig;
- bei `kind=S3RunRecovery` muss `source` ungleich 0 und kleiner als
  `nextPersistentInstanceId` sein; der Source-Record darf nicht mehr als
  aktiver Record vorhanden sein, es dürfen keine aktiven S3-/Y4-Records und
  kein `runRecoveryForbidden` bestehen, und Attempted/Outcome müssen gemäß
  dem RestartIntent-Vertrag gültig sein;
- der Boot-Decoder darf keine nicht mehr persistierte Preimage-Historie
  behaupten: Die zusätzliche Decode-/Handoff-Qualifikation verlangt
  `#17 Current/Fault`, einen physisch verifizierten passenden Fallback und
  eine passende Boot-/ResetCause-Qualifikation. Die Provenienz selbst wird
  ausschließlich zum Candidate-Zeitpunkt gebildet: Das geladene Preimage muss
  die Source-Instance aktiv enthalten, CauseClear und der exakte
  `ServiceResetProof` müssen für genau diese Instance gelten, genau diese
  Instance wird entfernt, und derselbe Kandidat setzt
  `Prepared(kind=S3RunRecovery, sourceInstanceId=source)`. Erst dessen
  bestätigter Commit ist die Provenienz-Evidenz;
- `runRecoveryForbidden=true` zusammen mit Prepared `S3RunRecovery` ist
  ungültig; `SafetyTaskRecovery` darf dadurch niemals einen Recovery-Handoff
  erhalten;
- ein `S3RunRecovery`-Intent ist nur ohne aktiven S3/Y4-Latch gültig;
- `abnormalRestartCount <= 3`, `safeBootRequired` ist 0/1 und
  `historyLineageState` ist nur 0, 1 oder 2;
- `safetyHistoryEpoch > 0`.

Jede Verletzung ist Decode-Failure und führt zu `Y4_UNKNOWN_SAFETY_STATE`,
`ImmediateStop` und `SAFE_BOOT`; sie wird nicht durch eine beliebige
Interpretation repariert.

Die `S3RunRecovery`-Candidate-Erzeugung beweist diese Herkunft mechanisch: Der
Service-Target-Reset nimmt die aktuelle SafetyState-Revision als Preimage,
prüft darin genau die aktive Source-Instance, CauseClear und den exakten
ServiceResetProof, entfernt genau diese Instance und setzt im selben
Kandidaten `Prepared(kind=S3RunRecovery, sourceInstanceId=source)`. Kein
Caller und kein Decoder darf einen historischen Source nachträglich
einsetzen. Der atomare Commit dieses Kandidaten ist die persistente Evidenz der
gemeinsamen Entfernung und Intent-Erzeugung; der spätere Boot-Decoder prüft nur
die verbleibenden Payload- und Handoff-Bedingungen.

### 4.5 Zwei-Slot-Commit

Der normale SafetyState-Commit ist exakt:

1. `sf0` und `sf1` lesen und semantisch validieren.
2. Den höchsten gültigen `recordRevision` bestimmen. Bei identischen Revisionen
   mit byteidentischen Payloads gewinnt deterministisch `sf0`; bei zwei
   unterschiedlichen semantisch gültigen Payloads gleicher Revision gibt es
   keinen Gewinner, sondern `Y4_UNKNOWN_SAFETY_STATE`.
3. `recordRevision + 1` checked berechnen. Bei `UINT64_MAX` wird nicht
   mutiert; es gilt der Exhaustion-Vertrag aus Abschnitt 5.
4. Den vollständigen Kandidaten im RAM bilden und crossfield-validieren.
5. Deterministisch den aktuell nicht gewinnenden Slot als Ziel wählen. Der
   bisherige Gewinner wird niemals zuerst überschrieben.
6. Genau einen normalen Zielslot-Write versuchen.
7. Bei `Success` den Zielslot lesen und die kanonischen Bytes exakt vergleichen.
   Nur bytegleiche Bestätigung ist Commit-Erfolg.
8. Bei `CommitOutcomeUnknown` den Zielslot lesen: exakt Kandidat bedeutet
   bestätigt; exakt vorheriger Zielinhalt bedeutet nicht committed; ReadError,
   CapacityError oder jede dritte Wahrheit bedeutet `BlockedIndeterminate`.
9. Bei `WriteError` oder `CapacityError` bleibt der alte Gewinner Wahrheit. Es
   gibt keinen zweiten SafetyState-Slotversuch.
10. Jeder erforderliche, nicht bestätigte Commit setzt RAM-seitig sofort
    `ImmediateStop`; danach gilt der begrenzte EmergencyMarker-Vertrag.

Ein älterer gültiger Peer nach erfolgreichem Commit ist normaler
Redundanz-/Rollbackstand und kein Repairfehler. Eine Boot-Reparatur eines
defekten Peer-Slots ist eine eigene, begrenzte SafetyPersistence-Mutation; sie
darf erst nach einem technisch gesunden Gewinner und Readback erfolgen.

`Healthy`, `MarkerRequired`, `BlockedIndeterminate` und
`SafetyCounterExhausted` sind Runtime-/Scanstatus und werden nie als
persistierte SafetyState-Wahrheit geschrieben. `historyLineageState` ist davon
getrennt eine sichtbare, tatsächlich persistierte Lineage-Eigenschaft.

## 5. EmergencyMarker, Counter und Safety-Historie

### 5.1 EmergencyMarker

```text
RecordTypeId       = 10
SchemaVersion      = 1
SafetyStorageEpoch = 1
Keys               = sem0, sem1
Slots              = exakt 2
max Envelope       = 64 Byte
utcUnixSeconds     = nullopt
Payload            = exakt 26 Byte; Envelope maximal 63 Byte <= 64
```

Payloadreihenfolge:

```text
markerRevision                  uint32 (Envelope VersionValue, ab 1)
state                           uint8  1=Active, 2=Cleared
reason                          uint8  1=SafetyCommit, 2=MarkerRepair,
                                        3=CounterExhausted, 4=HistoryLoss,
                                        5=UnknownSafetyState,
                                        6=FactoryInitialization,
                                        7=HistoryLossReinitialization
bootSequence                    uint32
monotonicMillis                 uint64
attemptedSafetyRecordRevision  uint64
```

Die Payloadsumme ist `4 + 1 + 1 + 4 + 8 + 8 = 26 Byte`; mit dem bestehenden
`33 Byte Header + 4 Byte CRC` ergibt dies 63 Byte. Die
`attemptedSafetyRecordRevision` bleibt dadurch auch oberhalb von
`UINT32_MAX` eine exakte Referenz auf den SafetyState-Kandidaten. Goldenbyte-
Tests decken `UINT32_MAX` und `UINT32_MAX+1` der SafetyState-Revision ab.

Zusätzlich zum Envelope-/CRC-/Schema-Check gilt exakt
`payload.markerRevision == Envelope.VersionValue` und
`markerRevision >= 1`. Für zwei semantisch gültige Marker gleicher Revision
gilt: Bei byteidentischem Payload gewinnt deterministisch `sem0`; bei
unterschiedlichem Payload ist der Zustand `MarkerIndeterminate` und führt zu
ImmediateStop/SAFE_BOOT. Erlaubt sind ausschließlich diese State-/Reason- und
Feldkombinationen (alle nicht optionalen Zahlenfelder sind immer vorhanden):

| State | erlaubte Reasons | `attemptedSafetyRecordRevision` | `bootSequence` / `monotonicMillis` |
|---|---|---|---|
| `Active` | `SafetyCommit` | `> 0` | `bootSequence > 0`; monotonic bootlokal, daher `0` zulässig |
| `Active` | `CounterExhausted`, `HistoryLoss`, `UnknownSafetyState` | `0` | `bootSequence > 0`; monotonic bootlokal, daher `0` zulässig |
| `Cleared` | `MarkerRepair`, `FactoryInitialization`, `HistoryLossReinitialization` | `0` | `bootSequence > 0`; monotonic bootlokal, daher `0` zulässig |

Unbekannte Werte, Reserved-Bits oder jede andere Kombination sind ungültig.
Bei einem normalen SafetyState-Commitfehler lautet die Reihenfolge:
RAM `ImmediateStop`, ein Active-Marker-Write auf den bevorzugten nicht
gewinnenden Marker-Slot, Readback, und bei fehlender Bestätigung genau ein
bounded Versuch auf dem anderen Slot. Danach kein Write-Loop.

Der Bootscan beider Marker gewinnt die höchste semantisch gültige Revision.
Active hält fail-closed. Ein alter Active-Marker ist erst redundant repariert,
wenn ein neuer Cleared-Marker exact-readback-bestätigt ist, der Peer mit exakt
denselben Bytes auf dieselbe Revision repariert und beide Slots danach
qualifiziert sind. Ein einzelner Cleared-Slot neben einem unqualifizierten
Active-Peer meldet daher nie `healthy/Cleared`. Ein defekter Peer wird
repariert, aber ein Y4 bleibt bis CauseClear plus Service-Reset bestehen.
Marker-Recovery repariert Persistenz, ist weder FaultReset noch SAFE_BOOT-Exit.

### 5.2 Erschöpfte High-Watermarks

Die folgenden Zustände sind selbst rebooterkennbare fail-closed Evidenz:

| Zähler | Exhaustion | Verhalten |
|---|---|---|
| `persistentFaultRevision` | `UINT32_MAX` | keine weitere Fault-Lifecycle-Mutation; RAM `ImmediateStop`; abgeleiteter Runtime-/Bootstatus `SafetyCounterExhausted`; SAFE_BOOT; Marker nur best effort |
| `nextPersistentInstanceId` | `UINT32_MAX` vor neuer Vergabe | keine neue persistente InstanceId und kein neuer Record; derselbe abgeleitete fail-closed Status |
| SafetyState `recordRevision` | `UINT64_MAX` | kein weiterer SafetyState-Commit; alter Winner bleibt Wahrheit; SAFE_BOOT |
| `bootSequence` | `UINT32_MAX` vor `+1` | Boot kann nicht normal fortfahren; ImmediateStop, SAFE_BOOT, Marker best effort |
| `safetyHistoryEpoch` | `UINT32_MAX` vor notwendigem `PartialSuccessor` | keine Addition und kein Wrap; ImmediateStop, SAFE_BOOT; nur der bereits servicegeschützte `TotalDiscontinuity`-Pfad darf eine neue, nicht global numerisch fortgesetzte Lineage eröffnen |
| Marker `markerRevision` | `UINT32_MAX` | kein neuer Markerwert darstellbar; RAM-/Bootscan-Evidenz bleibt fail-closed, kein Erfolgsclaim |

Bei Exhaustion wird keine nicht darstellbare Y4-Mutation behauptet und kein
`safeBootRequired=true` versprochen, wenn gerade dieser Commit unmöglich ist.
Die Maximalzahl beziehungsweise der Maximalwert selbst ist die persistente
Evidenz. `SafetyCounterExhausted` ist weder Catalog-Identity noch FaultRecord
und verbraucht keine InstanceId, FaultRevision oder Recordkapazität. Meldet
dagegen #17 einen noch mutierbaren RunPersistence-Counter-Overflow oder #56/#57
einen Configuration-Counter-Overflow, wird ausschließlich die passende,
domaingetrennte Identity `0x040E RunPersistenceCounter` beziehungsweise
`0x040F ConfigurationCounter` normal persistiert. SAFE_BOOT-Exit ist bei
`SafetyCounterExhausted` verboten.

### 5.3 Factory-new und Safety-History-Loss

`FactoryNoveltyProof` bleibt vollständig intern in
`ConfigurationRecoveryService::boot()`; #24 konstruiert oder konsumiert ihn
nicht. #24 liest vor jeder Freigabe selbst `sf0`, `sf1`, `sem0`, `sem1` und die
#17-RunPersistence-Evidenz. #57 wird über das öffentliche
`ConfigurationRecoveryStatus`-Ergebnis konsumiert.

Eine leere Safety-Erstinitialisierung ist ausschließlich gültig, wenn alle
folgenden Preconditions gleichzeitig bestätigt sind:

```text
sf0 = NotFound
sf1 = NotFound
sem0 = NotFound
sem1 = NotFound
#17 = keine RunPersistence-Historie
#57 = FactoryInitializationCompleted im selben Boot
keine widersprüchliche Store-Evidenz
```

Der Bootstrap-Vertrag ist danach exakt und redundant:

```text
1. SafetyState revision 1 bilden:
   safetyHistoryEpoch=1, nextPersistentInstanceId=1,
   persistentFaultRevision=0, bootSequence=1, kein Fault, kein Intent,
   safeBootRequired=false.
2. sf0 mit diesem SafetyState schreiben und exact readback-verifizieren.
3. sf1 mit semantisch und byteidentischem SafetyState revision 1 schreiben
   und exact readback-verifizieren.
4. sem0 als Cleared revision 1 mit reason=FactoryInitialization schreiben
   und exact readback-verifizieren.
5. sem1 als byteidentischen Cleared revision 1 schreiben und exact
   readback-verifizieren.
6. Erst jetzt gilt Factory-Safety-Bootstrap = redundantly healthy.
```

Bei zwei identischen gültigen SafetyState-Revisionen gilt weiterhin die
deterministische Tie-Regel: `sf0` ist der Winner. Ein Crash oder eine andere
Unterbrechung nach 0, 1, 2, 3 oder 4 bestätigten Bootstrap-Writes wird jeweils
als eigener Cut-Point getestet. Jeder Zustand mit weniger als allen vier
bestätigten Writes bleibt fail-closed in `SAFE_BOOT` oder im geschützten
Reinitialisierungspfad; ein partieller Bootstrap darf niemals als gesundes
Leersystem gelten. `bootSequence=1` wird bei dieser Erstinitialisierung gesetzt
und in demselben Boot nicht nochmals auf 2 erhöht.

Sind Safety-/Marker-Keys verloren, aber nicht nachweislich fabrikneu, bleibt
das Gerät in `SAFE_BOOT`. Ein geschützter, nicht automatischer Recoverypfad ist
jedoch zulässig:

```text
fehlender/unklarer SafetyState
 -> ImmediateStop und SAFE_BOOT
 -> keine Run-Recovery
 -> autorisierter Run-Abandon über #17 zu NoActiveRun/STANDBY
 -> Configuration gesund
 -> Store Read/Write-Test und Markerprüfung gesund
 -> Service-Autorisierung
 -> explizite SafetyState-Reinitialisierung
 -> neue SafetyHistoryLineage mit `historyLineageState=TotalDiscontinuity`
    eröffnen; kein numerisch größerer alter Epochwert wird behauptet
 -> konservativer SafetyState mit `recordRevision=1`,
    Y4_UNKNOWN_SAFETY_STATE und `markerRevision=1`
 -> beide Safety-Slots und beide `Cleared`-Marker mit
    `reason=HistoryLossReinitialization` redundant schreiben/readbacken
 -> History-Loss/Y4 bleibt bis CauseClear + Service-Target-Reset
 -> separater SAFE_BOOT-Exit
```

Programme und Configuration werden nicht gelöscht. `FaultInstanceId` ist nur
innerhalb der aktuellen `SafetyHistoryLineage` eindeutig. Bei partiell
verlorener, noch lesbarer Lineage wird `safetyHistoryEpoch` nur checked um 1
erhöht, wenn sein Wert kleiner als `UINT32_MAX` ist. Bei `UINT32_MAX` ist ein
PartialSuccessor nicht darstellbar: Es gibt weder Addition noch Wrap, sondern
ImmediateStop/SAFE_BOOT und ausschließlich den bereits servicegeschützten
TotalDiscontinuity-Pfad. Bei vollständigem Verlust wird ohnehin keine globale
numerische Ordnung behauptet. Der neue `Y4_UNKNOWN_SAFETY_STATE`-Record erhält in der neuen
Lineage `instanceId=1`, `persistentFaultRevision=1` und
`nextPersistentInstanceId=2`; alle weiteren Vergaben bleiben checked.
`FaultResetEvaluation`, `ServiceResetProof`, Target-Capability,
Primary-/Follow-up-Beziehung und RestartIntent binden die aktuelle Lineage und
werden bei jeder Discontinuity ungültig. Eine Capability aus einer früheren
Lineage wird nie akzeptiert, selbst wenn ihre Zahlen zufällig wieder bei 1
beginnen. Die Safety-Autorität mintet diese Capability nur aus dem aktuellen
Bootscan; die flüchtige Bootbindung verhindert eine Wiederverwendung alter
Evaluations. #19 erhält ein typisiertes `HistoryDiscontinuity`-Event im
bestehenden fixed-size
`SafetyEventBatch` mit `oldLineageKnown=false` und
`lineageState=TotalDiscontinuity`; es gibt keinen separaten
`SafetyHistoryDiscontinuity`-Handoff und keine globale Reihenfolge. Ein zweiter
Totalverlust erzeugt erneut dieses Event und macht alle vorherigen flüchtigen
Proofs ungültig.
Bei einer unbekannten Lineage oder Counter-Exhaustion bleibt die
Reinitialisierung fail-closed.

## 6. Fault-Lifecycle, Ack und Multi-Fault-Reset

### 6.1 Raise, CauseClear, Relapse, Reset

Ein abgeschlossener Producer-Snapshot ist die einzige Raise-Grenze. Zuerst
werden alle darin aktiven Ursachen RAM-seitig aggregiert; daraus folgen
ImmediateStop und Fan-Directive sofort, vor jeder Persistenz. Danach wird die
vollständige Menge der **neuen** persistenten S3-/Y4-Identities bestimmt. Vor
dem Candidate-Bilden wird geprüft, dass `persistentFaultCount + newCount <=
33` und genügend darstellbare InstanceIds vorhanden sind. Bei fehlender
Kapazität oder Counter-Exhaustion wird kein Teil der Menge committed, kein
aktiver Record verdrängt und der Pfad bleibt ImmediateStop/SAFE_BOOT mit dem
begrenzten Marker-/Exhaustionvertrag.

Für eine darstellbare Menge werden alle InstanceIds deterministisch vergeben:
Primary vor zugehörigem Follow-up, danach stabile Catalog- und
`displayPriority`-Reihenfolge. Alle neuen Records (`causeCleared=false`) werden
in **einen** vollständigen SafetyState-Kandidaten eingefügt. Betrifft irgendein
neuer Y4 einen aktiven Run, setzt derselbe Kandidat
`runRecoveryForbidden=true`. `persistentFaultRevision` erhöht sich genau einmal
für diese atomare Gesamtmutation; eine bereits aktive Identity erhält weiterhin
weder zweiten Record noch neue InstanceId. Genau ein normaler
SafetyState-Commit mit Exact-Readback linearisiert die gesamte Menge. Ein
Powerloss vor diesem Commit macht keinen Teil durable; nach bestätigtem Commit
ist die gesamte im Snapshot neu erkannte persistenzpflichtige Menge durable.

CauseClear ist nur zulässig, wenn die vollständige kanonische Producer-Evidenz
des konkreten Catalog-Eintrags gesund ist. Es setzt `causeCleared=true` und
erhöht `persistentFaultRevision`; der Latch bleibt bestehen.

Relapse vor Reset setzt `causeCleared=false`, erhöht die Revision und macht jede
ältere ResetEvaluation stale. Ein Reset entfernt erst nach aktueller
Target-Evaluation den Record, erhöht die Revision und nutzt den SafetyState-
Commit als Linearisierung. RAM-/Process-/Run-Handoff folgt erst danach.

Quittierung ist ausschließlich UI-/Message-Zustand: kein SafetyState-Feld, kein
CauseClear, kein Gatewechsel, keine Revisionserhöhung und keine persistenten
Ack-Writes. Nach Reboot darf die Meldung wieder unquittiert erscheinen.

### 6.2 Multi-Fault-Reset

`FaultResetEvaluation` aus #15 bleibt die reine Auswertung mit
Target-Identität, `targetResetAllowed`, `otherBlockingFaultActive` und
`releaseAllowed`. Ein Target darf intern erst entfernt werden, wenn diese
Evaluation aktuell ist, CauseClear und die Target-Safetychecks vorliegen und
der CommandCoordinator zusätzlich einen unforgeable `ServiceResetProof` für
dieselbe Target-Instance, Lineage und `persistentFaultRevision` prüft. Andere
Blocker verhindern Release, Recovery und Standby, aber nicht den so
qualifizierten Target-Reset. Erst nach dem letzten Blocker und erfüllter
Y4-Terminalregel kann ein Gate auf `Allowed` oder ein SAFE_BOOT-Exit entstehen.

Der Produktpfad akzeptiert kein öffentlich konstruierbares
`authorizationSatisfied=true`, keine vom Transport gelieferte Evaluation und
keinen fremd erzeugten Proof. Bis ein produktiver Service-PIN/Auth-Producer
existiert, sind S3/Y4-Resets produktiv fail-closed; Tests erhalten ausschließlich
einen app-internen Testhelper, nicht `device_platform_test_support`.

## 7. ImmediateStop, Aktor-Gate und Fan-/Thermikvertrag

### 7.1 Raise-Reihenfolge

Für S3/Y4 gilt atomar und immer:

```text
1 Ursache qualifizieren
2 RAM-SafetyGate sofort auf ImmediateStop setzen
3 Planner/Sink ab jetzt sperren und aktuelles Peltier sicher stoppen
4 SafetyState-Faultmutation versuchen
5 erst nach bestätigtem Commit Faultwahrheit und SafetyEventBatch veröffentlichen
6 bei Fehler EmergencyMarker anwenden; SAFE_BOOT/Runtimestatus fail-closed
```

Die physische Directive ist nicht commitabhängig. Events und persistente
Faultwahrheit sind commitabhängig; ein Commitfehler wird nie als erfolgreicher
Fault-Latch ausgegeben.

### 7.2 Aggregierte Directive

Der produktive Pfad ist:

```text
Producer -> SafetyFaultService -> ActuatorSafetyDirective
         -> TemperatureControlApplicationOrchestrator
         -> ActuatorPlanner -> ActuatorPlanSinkDriver
```

Die Directive ist nicht widersprüchlich:

```text
SafetyGateStatus: Unresolved | Allowed | ImmediateStop
FanSafetyAction:  PlannerManaged | ForceOn | ForceOff
ActuatorSafetyDirective:
  gate, outerFan, innerFan, safetyRevision
```

Aggregation über alle aktiven Ursachen ist deterministisch:

```text
Gate: ImmediateStop > Unresolved > Allowed
Fan:  ForceOff > ForceOn > PlannerManaged
```

Eine einzelne `sourceInstanceId` ist bei mehreren Ursachen nur Diagnose; sie
darf die Gesamt-Directive nicht repräsentieren. `ActuatorPlanner` bleibt für
#23-Mindest-Einschaltzeit, Mindest-Ausschaltzeit, Polaritätstotzeit,
Nachlauf, Watchdog und Accumulator zuständig. Der Orchestrator fragt die
Safety-Autorität intern bzw. über eine nicht frei konstruierbare Read-only-View
ab; der Caller liefert keinen Safetystatus.

### 7.3 STALE, FAILED und AirFallbackActive

Für Air/Cooling gilt:

| Evidenz | Issue-24-Wirkung |
|---|---|
| STALE | sofortiges transient fail-closed Gate, Peltier aus, Fanpolicy aus Catalog, begrenztes #20-Wiedererkennungsfenster; kein persistenter S3 allein wegen STALE |
| FAILED | persistenter S3, Service-Reset erst nach stabiler Requalifikation |
| Product invalid in `AirFallbackActive`, Air und Cooling valid | `O2_PRODUCT` allein blockiert den validierten Air-Ersatzbetrieb nicht; die bestehende #21-Directive bleibt Allowed, sofern keine andere Safetyursache besteht |
| Product invalid, Fallback pending/unresolved | `Unresolved`/Peltier aus; #21 entscheidet weiter |
| Air oder Cooling unsafe in `AirFallbackActive` | eigene O2/S3-Ursache blockiert sofort; keine Produkt-Fallback-Ausnahme |
| Rückkehr zu Product | erst nach #21-Requalifikation von Product, Air und Cooling |

Issue #24 implementiert keine parallele #21-FSM und ersetzt keine vorhandene
`AirFallbackActive`-Semantik.

`A*` in der ProductSensor-Catalogzeile bedeutet daher exakt: `Allowed` nur
bei `AirFallbackActive` plus gültigem Air/Cooling-Snapshot und ohne andere
Safetyursache; bei `FallbackPending`, `UserDecisionRequired` oder unaufgelöster
Evidenz ist der Status `Unresolved`/Peltier AUS. Die Rückkehr zu Product bleibt
vollständig #21.

### 7.4 Thermische Intervention und SafetyRecovery-Handoff

Die kanonische Eingriffsgrenze verlangt einen technisch möglichen, aber
vollständig zentral kontrollierten Handoff. #24 definiert deshalb den
nicht caller-fabrizierbaren `SafetyRecoveryActuatorRequest` mit exakt:

```text
direction                    Heating | Cooling
faultInstanceId              aktuelle S3-InstanceId
persistentFaultRevision     aktuelle SafetyRevision
safetyHistoryLineage        aktuelle Lineagebindung
attemptIndex                 1 oder 2
commissioningParameterRevision
effectiveAttemptLimit        0..2
```

Nur `SafetyFaultService`/Safety-Orchestrator kann den Request qualifizieren;
UI, Web, CommandRequest und Test-Transport können ihn nicht erzeugen. #23 ist
die einzige Ausführungsgrenze und erzwingt bestehende Mindest-Auszeit,
Polaritätstotzeit, Fanregeln, Abbruch und die abstrakte Aktorfolge. FaultCore
greift nie direkt auf H-Brücke oder GPIO zu.

Vor der Freigabe eines Commissioning-Satzes durch #35 und der erforderlichen
Hardware-/Peltiernachweise aus #33 gilt `effectiveAttemptLimit=0`: normale
Peltierpfade bleiben AUS und es wird kein Request erzeugt. Nach #35 darf der
validierte Satz zunächst 1 und erst nach weiterem Nachweis höchstens 2
Versuche erlauben. Damit wird die akzeptierte SAFETY_RECOVERY-Capability nicht
wegdefiniert, aber auch kein `TBD_COMMISSIONING`-Wert als Laufzeitwert benutzt.

`S3_THERMAL_INTERVENTION` sperrt sofort die auslösende Richtung, verwirft
Accumulator und Integrator und kann nur über diesen Handoff einen begrenzten
SafetyRecovery-Puls anfordern. `S3_THERMAL_HARD_EMERGENCY` schaltet beide
Richtungen aus und erzeugt niemals einen Gegenrichtungs-Request. Nach jedem
Versuch bleibt der S3-Latch bestehen; ein normaler Resume ist ausgeschlossen.

Die Versuchsgrenze ist firmwarefest und bootgebunden: `attemptsUsed` lebt nur
innerhalb derselben ununterbrochenen Bootinstanz. `effectiveAttemptLimit` wird
aus dem validierten #35-Satz gelesen und ist ohne gültige Commissioning-
Parameter exakt 0. Ein Reboot, Brownout, Watchdog, Panic oder ein
Persistenzstatus `CommitOutcomeUnknown` während der Episode beendet die
automatische SafetyRecovery-Fähigkeit dieser Faultepisode; ein Neustart setzt
keine Versuchsberechtigung zurück und es gibt keinen nächsten Puls.

Innerhalb desselben Boots gilt nach jedem Puls strikt:

```text
attemptsUsed += 1 (nur nach bestätigtem Pulsstart)
Peltier AUS -> Mindest-Auszeit/Totzeit
Sensor-, Aktor- und Fan-Evidenz erneut prüfen
Temperaturtrend gegen #35-Fortsetzungsorakel prüfen
Hard Emergency oder neuer Fault? -> sofort kein weiterer Versuch
Evidenz ungültig, Trend unzureichend oder attemptsUsed >= Limit? -> kein weiterer Versuch
sonst, attemptsUsed < effectiveAttemptLimit <= 2 -> höchstens ein nächster Puls
```

Hard Emergency sperrt die Gegenrichtung unabhängig von Trend und Limit. Die
konkreten Puls-, Trend- und Schwellenwerte liefert #35; sie werden in #24 nicht
als `TBD_COMMISSIONING`-Laufzeitwerte geraten. Tests decken Limit 0/1/2,
Reboot/Brownout nach Versuch 1, falschen Trend und neue Sensor-/Fan-/Aktorfehler
ab. Eine dritte Anforderung ist strukturell unmöglich.

## 8. RestartSupervisor und SAFE_BOOT

### 8.1 ResetCause, RestartIntent und Restartzwecke

`ResetCause` wird als geschlossene `uint8`-Tabelle geführt:

```text
1 PowerOn
2 SoftwareRestart
3 WatchdogOrPanic
4 Brownout
5 External
6 Unknown
```

Diese App-Policywerte stammen ausschließlich aus dem app-neutralen
`device_platform::IResetCauseProvider`; ein UI-, Command- oder App-Caller darf
keine `ResetCause` liefern. `fermentation_app` mappt den Portwert nur auf die
oben geschlossene Policy-Tabelle. Ein fehlender oder unbekannter Portwert ist
`Unknown` und damit abnormal/fail-closed.

Der Wirevertrag unterscheidet exakt:

```text
RestartIntentKind = None | SafetyTaskRecovery | S3RunRecovery
```

`SafetyTaskRecovery` ist ausschließlich für die sechs Catalog-Identities
`S3_SENSOR_TASK_STALL`, `S3_CONTROL_TASK_STALL`, `S3_SAFETY_TASK_STALL`,
`S3_ACTUATOR_TASK_STALL`, `S3_PERSISTENCE_PATH_STALL` und `S3_MAIN_TASK_STALL`
zulässig. Ein plausibler physischer Sensor-, Fan-, Peltier- oder
Verdrahtungsfehler erhält nie diesen Zweck. Der interne Producer muss für jede
Task-/Treiberüberwachung einen aktuellen Heartbeat und eine begründete Stall-
Evidenz liefern; die sechs Überwachungsbereiche sind Sensor, Regelung,
Safety/Fehler, Aktor, kritische Persistenz und Main/Task.

Der bestehende ESP-IDF-Produktpfad besitzt eine zentrale `app_main`-/Supervisor-
Schleife und bewusst keine sechs künstlichen FreeRTOS-Tasks. Deshalb sind zwei
Stallfälle strikt getrennt:

```text
A) Supervisor läuft noch:
   ein logischer Subpfad-/Heartbeat ist stale
   -> passende S3_<DOMAIN>_STALL-Identity
   -> ImmediateStop
   -> durable SafetyTaskRecovery (Prepared/Attempted)
   -> genau ein kontrollierter Restart

B) Supervisor/app_main selbst ist vollständig blockiert:
   kein eigener S3-/Intent-Write ist mehr möglich
   -> ESP-IDF TWDT-/IWDT-Backstop
   -> ungeplanter Watchdog-/Panic-Reset
   -> nächster Boot: WatchdogOrPanic, abnormalRestartCount++
   -> keine erfundene vorherige SafetyTaskRecovery-Persistenz
```

`S3_SAFETY_TASK_STALL` und die übrigen logischen Task-Domain-Identities sind
nur im Fall A vorpersistierbare `SafetyTaskRecovery`-Fälle, wenn ein noch
lauffähiger unabhängiger Supervisor die Evidenz erzeugt. Fall B darf nicht
nachträglich als `S3_SAFETY_TASK_STALL` oder als vorbereiteter
`SafetyTaskRecovery`-Intent rekonstruiert werden. Dafür wird keine neue
Watchdog-Task eingeführt.

`SafetyTaskRecovery` beantwortet ausschließlich, ob die intern blockierte
Software-/Taskfunktion technisch durch genau einen Neustart requalifiziert
werden kann. Es beantwortet nicht, ob der alte Fermentationslauf fortgesetzt
werden darf. Die verbindliche Sequenz für einen Task-/Treiberstall lautet:

```text
überwachter Task-/Subpfad-Stall-S3 während aktivem Run (Fall A)
 -> ImmediateStop
 -> S3-Latch
 -> #17 CriticalFault/Fault-current + vorhandener Pre-Fault-Fallback
 -> SafetyTaskRecovery Prepared/Attempted=0 in einem bestätigten SafetyState-
    Commit; danach Attempted=1 in einem zweiten bestätigten Commit
 -> genau ein kontrollierter Restart -> SAFE_BOOT, Latch bleibt
 -> Task-/Treiber-Evidenz neu qualifizieren
 -> CauseClear + Service-Target-Reset
 -> bei aktivem Run: atomar S3RecoveryDeparture/S3RunRecovery vorbereiten
 -> Attempted=1 durable -> genau ein zweiter kontrollierter Restart
 -> Bootqualifikation, RAM-only FallbackRecoveryPending -> #18
  -> #18 provable: Resume; sonst kanonischer NoActiveRun/STANDBY-Tombstone
```

Ein vollständiger Supervisor-/`app_main`-Stall (Fall B) nimmt diese Sequenz
nicht. Es gibt keinen vorherigen SafetyState-/Intent-Write, keinen
`SafetyTaskRecovery`-Handoff und keinen erfundenen FaultRecord; der nächste
Boot behandelt ausschließlich `WatchdogOrPanic` als abnormal und bleibt bis
zur normalen Boot-/SAFE_BOOT-Qualifikation fail-closed.

Der erste Neustart darf niemals einen #18-Handoff auslösen. Der
`SafetyTaskRecovery`-Intent und `restartAttempted=true` bleiben persistent,
solange seine zugehörige S3-Source-Instance aktiv ist. Der erste Boot nach dem
Neustart erzeugt keinen Side Effect und beendet den Intent nicht; er klassifiziert
nur die Resetursache und qualifiziert die Task-/Treiber-Evidenz. Erst der
qualifizierte Service-Target-Reset schließt die Episode oder überführt sie in
demselben SafetyState-Kandidaten in einen historischen `S3RunRecovery`-Intent.
Die beiden Task-Recovery-Schritte sind absichtlich getrennte durable Commits:
Ein Crash vor `Prepared` oder zwischen `Prepared/Attempted=0` und
`Attempted=1` darf die gleiche Fault-Instance nicht erneut automatisch
präparieren oder neustarten. Der nächste Boot bleibt für diese Episode
`SAFE_BOOT`/servicepflichtig; bei `Prepared/Attempted=0` ist kein Side Effect
angenommen und es gibt ebenfalls keinen Retry. Nur der qualifizierte
Service-Target-Reset darf die Episode schließen oder in den ausdrücklich
separaten `S3RunRecovery`-Vertrag überführen. Bei `Attempted=1` ist der eine
Restartversuch verbraucht, auch wenn der nachfolgende Reset nicht beobachtbar
war. Bei keinem aktiven Run wird der Intent nach dem Service-Target-Reset beendet
und `FaultResetCompleted` nach Standby beziehungsweise ein separater
SAFE_BOOT-Exit ausgeführt. Ein automatischer Loop ist ausgeschlossen.

Bei `S3RunRecovery` wird zuerst der S3-Target-Reset einschließlich des
Prepared-Intents durable gemacht. Der Intent ist niemals selbst ein
SAFE_BOOT-Exit: bei nicht handoffberechtigtem Prepared/Attempted-Zustand wird
`safeBootRequired=true` sicher wiederhergestellt und der unten definierte
Terminalpfad ausgeführt. Nur ein erfolgreich abgeschlossener #18-Handoff kann
anschließend die dafür erforderliche separate Exitmutation erreichen.
Voraussetzungen und Departure-Vertrag stehen in Abschnitt 8.7. Danach wird
`restartAttempted=true` in einem zweiten SafetyState-Kandidaten durable
bestätigt, bevor der Reset-Port aufgerufen wird. Der Reset-Port wird genau
einmal aufgerufen. Keinen `Requested`-Write nach dem Side Effect voraussetzen;
ESP-IDF kann vor dem physischen Reset nicht zuverlässig zurückkehren. `Rejected`
wird nur geschrieben, wenn der Port ausdrücklich beweist, dass kein Side Effect
angenommen wurde. Eine unerwartete Rückkehr nach `Requested` ist ein interner
Vertragsfehler, nicht `Rejected`; der Versuch bleibt verbraucht, der normale
Anwendungspfad endet fail-closed und es gibt keinen Retry. Der Intent bleibt
bis zur qualifizierten Service-Target-Reset- oder Handoff-Mutation bestehen.

### 8.2 ResetCause-Kompatibilität

| Intent | Attempted | ResetCause | abnormal | Handoff |
|---|---:|---|---:|---|
| `S3RunRecovery` | 1 | SoftwareRestart | nein | ja, nach vollständiger Bootqualifikation |
| `S3RunRecovery` | 1 | External | nein | ja, nach vollständiger Bootqualifikation |
| `S3RunRecovery` | 1 | PowerOn | nein | ja, nach vollständiger Bootqualifikation |
| `S3RunRecovery` | 0 | SoftwareRestart | ja | nein; SAFE_BOOT, kein Handoff |
| `S3RunRecovery` | 0 | External/PowerOn | nein | nein; kein kontrollierter Handoff |
| `S3RunRecovery` | 1 | Brownout | ja | nein; SAFE_BOOT, kein Handoff |
| `S3RunRecovery` | 1 | WatchdogOrPanic | ja | nein; SAFE_BOOT, kein Handoff |
| `S3RunRecovery` | 1 | Unknown | ja | nein; SAFE_BOOT, kein Handoff |
| `SafetyTaskRecovery` | 1 | SoftwareRestart | nein | nie #18; SAFE_BOOT wegen Latch |
| `SafetyTaskRecovery` | 1 | External/PowerOn | nein | nie #18; SAFE_BOOT wegen Latch |
| `SafetyTaskRecovery` | 0 | beliebig | ja | nein; SAFE_BOOT |
| `None`/ungültig | 0 | SoftwareRestart | ja | nein |
| `None`/ungültig | 0 | PowerOn/External | nein | nein; normaler Boot nur nach übriger Qualifikation |

`Prepared + Attempted=0 + SoftwareRestart` ist niemals ein kontrollierter
S3-Handoff. Bei inkompatibler Cause bleibt die Evidenz fail-closed; es gibt
keinen automatischen zweiten Restart. Nur ein bereits attempteter
`S3RunRecovery`-Intent darf nach External/PowerOn gemäß Tabelle übernehmen.

### 8.3 Restartzähler und Stabilität

`abnormalRestartCount` ist saturierend 0..3. Count 1 und 2 gehören zu einem
echten 30-Minuten-Fenster der **aktuellen** monotonen Bootzeit. Für Count 1
oder 2 gilt:

```text
30 Minuten aktuelle monotone Bootzeit
+ kein neuer abnormaler Reset
+ SafetyState/Marker redundant healthy
+ kein aktiver S3/Y4-Latch
+ kein Prepared Intent
-> abnormalRestartCount = 0
-> ein bestätigter SafetyState-Requalifikationscommit mit Readback
```

Ein abnormaler Reset vor Ablauf des Fensters qualifiziert nicht um; Count 2
geht dadurch auf 3. Der Übergang auf 3 erzeugt, solange noch darstellbar,
`Y4_RESTART_LOOP` und `safeBootRequired=true` in demselben SafetyState-
Kandidaten. Für das Target `Y4_RESTART_LOOP` selbst gilt die einzige
selbstbezügliche Ausnahme:

```text
30 Minuten aktuelle monotone Bootzeit
+ kein neuer abnormaler Reset
+ SafetyState/Marker redundant healthy
+ keine andere aktive S3/Y4-Identity als `Y4_RESTART_LOOP`
+ kein Prepared Intent
-> nur `causeCleared=true` für `Y4_RESTART_LOOP`; Count bleibt 3
```

Damit deadlockt sich RestartLoop nicht an seinem eigenen aktiven Latch. Ein
Service-Target-Reset von `Y4_RESTART_LOOP` ist vor CauseClear abgelehnt. Erst
danach setzt genau dieser Target-Reset `abnormalRestartCount=0` und entfernt
den RestartLoop-Record. Ein Restart allein setzt den Count nicht;
`safeBootRequired` bleibt bis zum separaten SAFE_BOOT-Exit. Ein zusätzlicher
S3- oder Y4-Record blockiert CauseClear. Das RestartLoop-Orakel ist ausschließlich
`QRS` (RestartSupervisor-Stabilitätsevidenz), niemals das #17-Orakel `QR`.
`bootSequence` wird einmal pro Boot checked erhöht; bei `UINT32_MAX` gilt
Abschnitt 5.2. Keine monotone Zeit wird über Reboots subtrahiert.

### 8.4 Kanonische Bootreihenfolge

```text
1  alle Peltier-/H-Brücken-/schaltbaren Ausgänge fail-closed AUS
2  EmergencyMarker sem0/sem1 scannen
3  SafetyState sf0/sf1 scannen, validieren, Redundanzstatus bestimmen
4  ResetCause, bootSequence, RestartIntent, abnormalRestartCount auswerten
5  #57 ConfigurationRecoveryService::boot()
6  #56 ConfigurationService Runtime-/Commitzustand konsumieren
7  #17 RunPersistence::loadAndInitialize()
8  #20/#21 Sensor- und Sicherheits-Evidenz laden
9  Producer -> SafetyFaultService Catalog-Mapping
10 runRecoveryForbidden und beide Restartzwecke qualifizieren
11 SAFE_BOOT-Entscheidung und bestehenden #17-Fallbackstatus trennen
12 bestehendes `FallbackRecovered` ohne S3Intent -> bestehendes #18
13 `Current/Fault` plus gültiger `S3RunRecovery`-Intent -> read-only
   Safety-Fallback-Handoff -> #18
14 erst nach vollständiger Qualifikation normaler Orchestrator/Planner/Sink-Tick
```

Kein Planner-/Sink-Tick darf vor Schritt 14 einen `Allowed`-Pfad erhalten.

### 8.5 SAFE_BOOT-Eintritt

SAFE_BOOT ist zwingend bei:

- SafetyState-/Marker-Indeterminate, erforderlicher Safety-Redundanzreparatur,
  `safeBootRequired` oder Count 3;
- jedem beim Boot vorhandenen persistenten S3-/Y4-FaultRecord;
- `runRecoveryForbidden`;
- ungültigem oder inkompatiblem RestartIntent;
- RunPersistence `NotReconstructible`, `PreparedInterrupted`, `ReadFailed`,
  `CapacityExceeded`, `UnsupportedSchema`, `ForeignEpoch` oder Orphan;
- kritischer #56/#57-Configintegrität/-unavailability/-indeterminate;
- unbekannter Safety-Evidenz oder Counterstatus.

Wenn SafetyState schreibbar ist, wird `safeBootRequired=true` vor dem RAM-
Zustandswechsel committed/readback-bestätigt. Ist gerade Safety-Persistenz die
Ursache, hält Marker-/Scanstatus unabhängig davon fail-closed.

Ein kontrollierter S3-Recovery-Boot ist die einzige Ausnahme: Vor Boot sind
alle S3/Y4-Records bewusst target-resettiert, es existiert ausschließlich ein
gültiger Prepared-Intent und `runRecoveryForbidden=false`. Ein aktiver
persistent gespeicherter S3/Y4-Latch führt immer zuerst nach SAFE_BOOT und nie
direkt nach normalem Fault/Standby/Allowed.

### 8.6 SAFE_BOOT-Exit

Alle Bedingungen müssen gleichzeitig gelten:

```text
ProcessState SafeBoot
safeBootRequired=true vor der Exitmutation
Marker Cleared und redundant healthy
SafetyState redundant healthy
kein S3/Y4-Record, kein runRecoveryForbidden
kein Prepared Intent und kein CounterExhaustion
#56 Operational, #57 qualified
#17 deterministisch, NoActiveRun/STANDBY, kein BlockedIndeterminate
Air/Cooling VALID, SensorSelection resolved
Planner/Watchdog healthy
Service-Auth-Proof gültig
```

Dann werden Bedingungen neu geprüft, `safeBootRequired=false` committed und
readback-bestätigt, `SafeBootExitCompleted` nur RAM-seitig gesetzt und
anschließend `Standby` hergestellt. Erst danach folgt eine neue normale
Gate-Evaluation. SAFE_BOOT-Exit ist kein FaultReset; ein Crash nach dem Flag-
Commit vor RAM-Transition wird beim nächsten Boot erneut qualifiziert und
 fail-closed behandelt. Ein recoverbarer aktiver Run ist ausschließlich die
 separate `S3RecoveryDeparture`-Ausnahme aus Abschnitt 8.7; der normale Exit
 verlangt weiterhin `NoActiveRun/STANDBY`.

### 8.7 SAFE_BOOT -> S3RecoveryDeparture

Ein S3 kann vor seinem Service-Reset rebooten. Dann sind S3-Latch,
`safeBootRequired=true`, #17 `Current=Fault` und der unveränderte Pre-Fault-
Fallback gemeinsam persistiert. Dieser Zustand darf nicht durch den normalen
SAFE_BOOT-Exit in `NoActiveRun` terminalisiert werden.

Ein eigener, fail-closed `S3RecoveryDeparture` ist nur zulässig, wenn ein
autorisierter Service-Target-Reset gleichzeitig in **einem** SafetyState-
Kandidaten ausführt:

```text
geladenes SafetyState-Preimage enthält die aktive sourceInstance
CauseClear + exakter ServiceResetProof für genau diese sourceInstance
genau diese S3-Instance aus dem Kandidaten entfernen
im selben Kandidaten Prepared(kind=S3RunRecovery, sourceInstanceId, Attempted=0)
setzen
```

Das ist die Candidate-time-Provenienzbildung. Nach dem Commit ist die entfernte
Source nicht mehr persistiert; der spätere Boot-Decoder darf deshalb nur noch
`sourceInstanceId != 0 && sourceInstanceId < nextPersistentInstanceId`,
kein aktives S3/Y4, `runRecoveryForbidden=false`, gültige Attempted-/Outcome-
Crossfields sowie #17 `Current/Fault`, den physisch passenden Fallback und die
ResetCause-Qualifikation prüfen.

Zusätzlich müssen vor dem Commit gelten: keine weiteren S3/Y4, kein
`runRecoveryForbidden`, redundanter SafetyState und Marker healthy,
#56/#57 qualifiziert, #17 `Current=Fault` plus vollständig passender
Pre-Fault-Fallback, aktuelle Air/Cooling-/Sensor-Evidenz, Planner-/Watchdog-
Evidenz healthy und kein Counter-Exhaustion. RAM bleibt bis zum Ende
`SafeBoot/ImmediateStop`; kein Planner- oder Sink-Tick wird freigegeben.

Ist diese Recoverability-Prüfung **vor** dem Target-Reset nicht beweisbar,
bleibt der S3-Latch aktiv: ImmediateStop, Service-TargetReset-Anforderung und
RecoverabilityGate werden zwar ausgeführt, aber es wird kein
`S3RunRecovery`-Intent gebildet und kein Target entfernt. Der alte Run wird
bei weiter aktivem S3-Latch über den bestehenden sicheren #17-Run-Abandon-
Vertrag zu `NoActiveRun/STANDBY` terminalisiert und exact-readback-bestätigt.
Erst danach ist ein S3-Target-Reset ohne RecoveryIntent zulässig;
`safeBootRequired` bleibt bis zum separaten SAFE_BOOT-Exit gesetzt. Es entsteht
keine zusätzliche Y4-Identity nur für diesen Terminalpfad.

Nach bestätigtem Kandidatencommit wird `restartAttempted=true` in einem zweiten
SafetyState-Kandidaten geschrieben. Erst danach darf der
`IResetController` einmal aufgerufen werden. Bei Port-Ablehnung/Return wird
`Rejected` geschrieben, ohne Retry. Beim nächsten Boot ist nur
`S3RunRecovery + Attempted=1` gemäß ResetCause-Matrix handoffberechtigt.
`Attempted=0`, jeder neue S3/Y4, Marker-/Safety-Indeterminate,
Configuration-/Run-Fehler oder Counterstatus führt zu SAFE_BOOT ohne Handoff.

Jeder `S3RunRecovery`-Intent, der gemäß ResetCause-Matrix oder aktueller
#17-/Fallback-/Config-Evidenz nicht mehr handoffberechtigt ist, wird niemals
erneut gestartet. Stattdessen folgt zwingend: ImmediateStop/SAFE_BOOT,
`safeBootRequired=true` durable sofern der SafetyState schreibbar, kein
Restart-Retry und kein Resume; dann wird der alte Run über #17 forward-only zu
`NoActiveRun/STANDBY` terminalisiert und exact-readback-bestätigt. Erst danach
wird `restartIntent=None` durable committed; `safeBootRequired` bleibt true
bis zum separaten Exit. Das umfasst Crash nach Prepared vor Attempted,
`Attempted=0` mit jedem Reboot, explizites `RestartRejected`, Attempted=1 mit
Brownout/Watchdog/Panic/Unknown, fehlende spätere Fallback-Qualifikation und
jeden dauerhaft unzulässigen Handoffgrund. Bei `Prepared S3RunRecovery` plus
bereits exakt durablem `NoActiveRun/STANDBY` erkennt der Boot nach Readback den
Terminalpunkt und löscht den Intent deterministisch reparativ; kein Resume und
kein Restart. Die Crashmatrix prüft Crash vor Tombstone, indeterminate
Tombstone, Tombstone vor Intent-Clear und Intent-Clear vor SAFE_BOOT-Exit.
Ein Crash vor dem Departure-Commit lässt den S3-Latch und
`safeBootRequired=true` bestehen. Ein neuer Safetyfault während Departure
bricht den Handoff ab, setzt ImmediateStop und lässt die neue persistente
Wahrheit dominieren.

## 9. S3-Recovery ohne Head-Rollback

### 9.1 Eintritt, CauseClear und Restart

Bei S3 während eines aktiven Runs gilt:

```text
S3 qualifiziert
 -> RAM ImmediateStop
 -> S3-Record im SafetyState durable
 -> bestehender ProcessEvent/TransitionReason CriticalFault -> FAULT
 -> #17 CriticalFault schreibt Fault-current mit Standard-Fallback-Rotation
 -> vorheriger gültiger Run-Checkpoint bleibt referenziert als fallback
```

Nach CauseClear und Service-Target-Reset ist der S3-Latch entfernt, der
ProcessState bleibt RAM-seitig `Fault`, der alte Run wird im selben Boot nicht
als aktiv zurückgeschrieben und es gibt kein Head-Rollback. Wenn danach kein
aktiver Run und kein Y4/safeBoot-Blocker besteht, wird ohne Intent über den
terminalen `FaultResetCompleted`-Pfad nach `Standby` gegangen. Existiert ein
aktiver Run und `runRecoveryForbidden=false`, werden `Prepared` und dann
`Attempted` wie in Abschnitt 8 committed und genau ein Restartversuch
ausgeführt. Bei einer SafetyTaskRecovery-Episode geschieht diese
S3RunRecovery erst nach erfolgreicher Task-/Treiber-Requalifikation und dem
Service-Target-Reset als der ausdrücklich zweite, getrennte Restartversuch;
der erste TaskRecovery-Boot darf diesen Pfad nicht vorwegnehmen.

### 9.2 RAM-only Fallback-Handoff

Der Bootpfad hat zwei mechanisch getrennte Fälle. Der bestehende #17-Fallback
ist nicht von einem S3Intent abhängig:

```text
A) loadAndInitialize() == FallbackRecovered
   + Safety/Config qualified + kein Y4/runRecoveryForbidden
   -> bestehendes activateFallbackRecoveredRun() -> #18
   -> kein S3Intent erforderlich

B) loadAndInitialize() == Current mit ProcessState::Fault
   + gültiger S3RunRecovery-Intent + keine aktiven S3/Y4
   -> prepareSafetyFallbackRecovery() liest/verifiziert den bestehenden Fallback
   -> FallbackRecoveryPending -> bestehendes #18
```

Fall A bleibt unverändert der normale #17/#18-Recoverypfad. Fall B ist die
neue, explizit durch S3-Reset und Intent autorisierte Auswahl. Ein aktiver
persistent gespeicherter S3/Y4-Latch blockiert beide Fälle bis zum jeweiligen
Reset-/Terminalvertrag.

Für den Recovery-Boot wird eine schmale API mit dem Zweck
`prepareSafetyFallbackRecovery(...)` geplant. Sie ist erforderlich, weil der
direkte #17-Code bei gültigem Current in `loadAndInitialize()` den Current lädt,
`LoadedActiveRun` setzt und zurückkehrt; ein gültiger Fault-current lädt seinen
Fallback **nicht** automatisch. Die API ist ein #17-interner, read-only
RAM-Handoff und darf nur:

- `currentHead_`, den bereits geladenen gültigen Fault-current und seine
  Fallback-Referenz erneut prüfen;
- die lokale technische `loadReference(...)`-Logik aus
  `loadAndInitialize()` als private Helperfunktion wiederverwenden (nicht
  kopieren): Slotread, Envelope, Epoch, Schema, CRC/Codec und
  `runCheckpointReferenceMatches()`;
- den Fallback explizit in `slots_[fallback.slot]` laden und Current/Fallback
  an `runId`/Programmsnapshot/Manual-Runidentität binden;
- `runRecoveryForbidden=false`, gültigen S3Intent, keine S3/Y4-Records,
  qualifizierte Config und gültige Sensorgrundlage prüfen;
- `persistedIds_` und `persistedIdCount_` aus dem Fallback herstellen,
  die bestehenden Head-/Checkpoint-High-Watermarks und Caches aber niemals
  zurücksetzen;
- erst dann den Coordinator auf `FallbackRecoveryPending` setzen;
- den Snapshot an die bestehende #18-Activation übergeben.

Sie darf keinen Head, Slot, Checkpointpayload oder Revision schreiben, keine
Fallback-Referenz persistent vertauschen und keine Zeit-/Progress-/Phasen-
Recovery entscheiden. Zwischen Handoff und #18 Hop 1 gibt es keinen
Temperature-Control-Tick, Planner-Tick, Sink-Write oder normales Run-Command.

`loadAndInitialize()` bleibt die physische Verifikation. Der bestehende Code
in `run_persistence_coordinator.cpp:331-369` beweist die
Referenzvalidierung; sein Fallbackpfad `446-485` beweist RAM-Laden und
`FallbackRecoveryPending` nur dann, wenn der Current nicht rekonstruiert werden
kann. Er beweist ausdrücklich nicht das automatische Fallbackladen bei
gültigem Fault-current. Der neue Helper nutzt diese vorhandenen Bausteine
DRY für den zusätzlichen, eng begrenzten Safety-Handoff. Der bestehende
`activateFallbackRecoveredRun()`-Pfad darf anschließend seine notwendige
forward-only Recoveryrevision schreiben; das ist nicht die Auswahlmutation.

### 9.3 #18 und Zeitsemantik

Nach dem RAM-Handoff entscheidet ausschließlich #18 über:

- UTC und bootlokale monotone Zeit;
- `t0` letzter gültiger Checkpoint, `t1` tatsächlicher Fault-Eintritt und `t2`
  Recovery-Boot;
- Temperatur-Evidenz, Progress und `QualifyingTarget`,
  `WaitingForProduct`, `Fermenting`, `Cooling`, `CoolHolding`,
  `ManualHolding`.

`t0..t1` ist konservative Recovery-Unsicherheit; `t1..t2` ist
Recovery-Unterbrechung. Keine Zeitspanne wird als normale Fermentationszeit
erfunden und #24 implementiert keinen eigenen Fortschrittszähler. Wenn #18
nicht beweisen kann, dass eine Fortsetzung sicher ist, wird der Run terminal
beendet.

### 9.4 S3-Recovery-Sequenz

```text
ACTIVE RUN
   |
   | S3
   v
FAULT
   |
   +--> SafetyState: S3 latch
   |
   +--> #17 current = Fault checkpoint
             fallback = previous valid checkpoint
   |
CauseClear + Service target reset
   |
   +--> S3 latch removed; RAM remains FAULT
   +--> active run + SAFE_BOOT -> Prepared; SAFE_BOOT bleibt bis zum
        qualifizierten Handoff/Terminal- und separaten Exitvertrag maßgeblich
   +--> active run + normal Fault -> Prepared/Attempted Intent
   |
controlled restart (at most once)
   |
   v
BOOT -> Safety/Config/Run/Sensor qualification
   |
   +--> #17 verifies current Fault + existing fallback
   |
   +--> RAM-only fallback selection
          -> FallbackRecoveryPending
          -> no persistent head/slot mutation
   |
   +--> existing #18 recovery
          +--> provable: forward durable Resume
          +--> not provable: canonical NoActiveRun/STANDBY Tombstone
```

## 10. Y4-Terminalität und Run-Abandon

### 10.1 Y4 mit aktivem Run

Ein Y4-Raise mit aktivem Run schreibt `Y4 latch` und
`runRecoveryForbidden=true` in denselben SafetyState-Kandidaten. Solange ein
Blocker besteht:

```text
ProcessState Fault oder SafeBoot
alter Run bleibt als persistierte Evidenz erhalten
kein S3-Recovery-Handoff
kein NoActiveRun + Fault
```

Der Prozessvertrag erhält die expliziten Events
`ProcessEvent::FaultResetCompleted` und `ProcessEvent::RunAbandonCompleted`
sowie die gleichnamigen `TransitionReason`-Werte. Beide werden vor Anwendung
als Kandidat validiert und durch #17 Write-before-Apply geschützt; keiner darf
`NoActiveRun + Fault` oder `NoActiveRun + SafeBoot` erzeugen.

### 10.2 Terminalpunkt

Die kanonische Topologie ist ausschließlich:

| Fall | ProcessEvent | TransitionReason | Tombstone-Reihenfolge |
|---|---|---|---|
| letzter S3-Target-Reset, kein aktiver Run | `FaultResetCompleted` | `FaultResetCompleted` | kein Tombstone; Fault -> Standby, danach kein Intent |
| recoverbarer S3-Run | kein Fault->Standby | kein terminaler Event | erst S3Intent, RAM-Fallback, #18; kein Tombstone vor #18 |
| #18 `RecoveryRejected` | `RunAbandonCompleted` nach durablem Tombstone | `RunAbandonCompleted` | Tombstone zuerst, dann terminaler Standby-Event |
| Y4 mit aktivem Run | `RunAbandonCompleted` nach durablem Tombstone | `RunAbandonCompleted` | autorisierter Abandon, Tombstone, dann Standby |
| SAFE_BOOT-Abandon | `RunAbandonCompleted` nur als persistenter Kandidat | `RunAbandonCompleted` | Tombstone durable; RAM bleibt SafeBoot bis separatem Exit |

Für Y4-Abandon und #18-Reject ist die Reihenfolge:

```text
1 SafetyGate ImmediateStop und Safety-Latch
2 autorisierte technische Run-Abandon-Evaluation
3 aktiven Run vollständig löschen und kanonischen NoActiveRun/STANDBY-
  Kandidaten bilden
4 #17 Prepared -> Slot -> Committed, exact CAS/readback
5 erst nach bestätigtem Tombstone `RunAbandonCompleted` anwenden
6 erst nach bestätigtem Tombstone `runRecoveryForbidden=false` schreiben,
  falls kein weiterer Blocker besteht
7 danach separater SAFE_BOOT-Exit, falls kein anderer Blocker besteht
```

Ein Crash vor Schritt 6 lässt `runRecoveryForbidden=true`; ein Crash zwischen
4 und 6 lässt den alten Run und die Terminalpflicht bestehen. Ein Crash nach
dem Tombstone vor dem Flag-Clear darf nur nach erneut bestätigtem
`NoActiveRun/STANDBY` reparieren. `NoActiveRun` wird nie mit Fault oder SafeBoot
als Prozesssnapshot geschrieben.

### 10.3 RunPersistence-Y4-Sonderpfad

Bei `Y4_RUN_*`, deren CauseClear den Tombstone voraussetzt, gibt es keinen
Reset-vor-Tombstone-Zyklus:

```text
Y4_RUN_*
 -> ImmediateStop / SAFE_BOOT
 -> technisch qualifizierter autorisierter Abandon trotz aktiver Y4-Latch
 -> NoActiveRun/STANDBY durable
 -> erst jetzt RunPersistence-CauseClear
 -> Service Target Reset des Y4
 -> runRecoveryForbidden erst nach Tombstone/letztem Blocker clear
 -> separater SAFE_BOOT-Exit
```

Technischer Abandon ist kein `FaultResetCompleted` und kein Recovery-Resume.

### 10.4 Nicht rekonstruierbare Runs

Für `NotReconstructible`, `NotReconstructibleOrphanedState`,
`PreparedInterrupted`, `ReadFailed`, `CapacityExceeded`, `UnsupportedSchema`
und `ForeignEpoch` gilt: kein Raten, kein alter Resume, kein Factory Reset,
keine Löschung von Configuration/Programmen.

Bei `BlockedIndeterminate`/`PreparedInterrupted` baut eine schmale #17-interne
Abandon-API exakt den kanonischen Snapshot:

```text
variant=NoActiveRun
ProcessState=Standby
keine Run-/Sensor-/Recoveryfelder
forward-only technische Revisionen
```

Der Prepared-Head wird nicht blind committed. Die API liest/validiert die
technisch sichtbaren Rohbytes, verwendet den tatsächlich gelesenen Raw-Head-
Stand als CAS-Preimage und schreibt den neuen Tombstone vorwärts. Alle
technischen Revisions-High-Watermarks werden gescannt; `max+1` ist checked.
Bei Overflow, Readback-Fehler oder dritter Wahrheit bleibt SAFE_BOOT.

## 11. Vollständiges #17-Statusmapping

### 11.1 Boot-/Loadstatus

| `RunPersistenceLoadStatus` | #24 Code / Wirkung | SAFE_BOOT / Run |
|---|---|---|
| `NoPersistedRun` | keine Fault; nur mit #57 Factory-Status als Erstboot leer initialisieren | nein / NONE |
| `Current` | keine Fault, aber Safety-/Latch-/Intentprüfung vor Activation; `ProcessState::Fault` benötigt explizit `S3RunRecovery` | bei persistentem S3/Y4 ja / normal #18 nur ohne Latch |
| `NoActiveRun` | keine Fault; kanonische Standby-Wahrheit | nein / NONE |
| `FallbackRecovered` | keine Fault; bestehender #17-Fallback geht ohne S3Intent an `activateFallbackRecoveredRun()` und danach #18 | nur bei sonstigem Grund / Fallback pending |
| `PreparedInterrupted` | `Y4_RUN_PREPARED_INTERRUPTED`, ImmediateStop | ja / TERM-Abandon |
| `NotReconstructible` | `Y4_RUN_NOT_RECONSTRUCTIBLE` | ja / TERM-Abandon |
| `NotReconstructibleOrphanedState` | `Y4_RUN_ORPHANED_STATE` | ja / TERM-Abandon |
| `ReadFailed` | `Y4_RUN_READ_FAILED` | ja / TERM-Abandon |
| `CapacityExceeded` | `Y4_RUN_STORE_INTEGRITY` | ja / TERM-Abandon |
| `UnsupportedSchema` | `Y4_RUN_STORE_INTEGRITY` | ja / TERM-Abandon |
| `ForeignEpoch` | `Y4_RUN_STORE_INTEGRITY` | ja / TERM-Abandon |
| `AlreadyInitialized` | an dieser Bootstelle unmöglich: `Y4_INTERNAL_SAFETY` | ja / kein Resume |

### 11.2 Runtimeresultate

| `RunPersistenceResultStatus` | normale Bedeutung | required Fault/Recovery/Tombstone-Commit |
|---|---|---|
| `Applied` | durable geschrieben und RAM angewendet | Erfolg |
| `CheckpointWritten` | periodischer Checkpoint durable | optionaler Erfolg; required = `Y4_INTERNAL_SAFETY`, weil kein vollständiger Required-Commit bewiesen ist |
| `AlreadyProcessed` | RAM-idempotent, keine Mutation | optional kein Fault; required = `Y4_INTERNAL_SAFETY`, kein Durability-Proof |
| `AlreadyPersisted` | durable-idempotent, keine Mutation | nur Erfolg nach exaktem Head-/Snapshot-/CRC-/Readback-Nachweis; sonst `Y4_INTERNAL_SAFETY` |
| `NotEligible` | optionaler Vorgang nicht fällig/zugelassen | kein Fault; required = `Y4_INTERNAL_SAFETY` |
| `NotAllowedInState` | fachlich unzulässig | optional kein Fault; required = `Y4_INTERNAL_SAFETY` |
| `NotInitialized` | Coordinator-/Bootreihenfolge verletzt | `Y4_INTERNAL_SAFETY`, SAFE_BOOT |
| `RecoveryPending` | kein Apply, Recovery noch nicht qualifiziert | SAFE_BOOT/kein Aktor |
| `Busy` | nur mit vorab definiertem bounded Retry außerhalb Aktorpfad | required ohne bestätigten Retry = `Y4_INTERNAL_SAFETY` |
| `InvalidDecision` | Kandidat/Command ungültig | required = fail-closed `Y4_INTERNAL_SAFETY` |
| `StaleDecision` | Revisionskonflikt | required = fail-closed `Y4_INTERNAL_SAFETY` |
| `TimeMismatch` | Zeit-/Checkpointkontext widerspricht | required = fail-closed `Y4_INTERNAL_SAFETY` |
| `TimeWentBackwards` | monotone Zeitverletzung | `Y4_INTERNAL_SAFETY`, SAFE_BOOT |
| `CounterOverflow` | #17- oder #56/#57-Counter erschöpft | passende domaingetrennte Counter-Identity `0x040E` oder `0x040F`, SAFE_BOOT |
| `WriteFailed` | sicher nicht geschrieben | `Y4_RUN_STORE_INTEGRITY`, EmergencyMarker, SAFE_BOOT |
| `CapacityExceeded` | Store-/Payloadlimit | `Y4_RUN_STORE_INTEGRITY`, SAFE_BOOT |
| `PersistenceIndeterminate` | Ausgang nicht auflösbar | `Y4_RUN_STORE_INTEGRITY`, SAFE_BOOT |
| `PersistenceCommittedApplyFailed` | durable Wahrheit, RAM-Anwendung verletzt | `Y4_INTERNAL_SAFETY`, SAFE_BOOT; nicht rollbacken |
| `Blocked` | Coordinator blockiert | `Y4_INTERNAL_SAFETY`, SAFE_BOOT |
| `NotDue` | periodischer Checkpoint nicht fällig | optional kein Fault; required = `Y4_INTERNAL_SAFETY` |
| `NoActiveRun` | kein Run für optionale Operation | optional kein Fault; required Tombstone nur bei separat bewiesenem kanonischem Readback, sonst `Y4_INTERNAL_SAFETY` |

`InvalidDecision`, `NotInitialized`, `NotAllowedInState`, `StaleDecision`,
`TimeMismatch`, `Blocked` und `Busy` werden im required Safety-/Tombstonepfad
niemals als harmlose Ablehnung behandelt. `Busy` hat nur den expliziten
bounded Retryvertrag für einen rein technischen, nicht aktorwirksamen Scan;
ein unbestimmtes Write-Ergebnis wird nicht erneut blind geschrieben. Für jeden
Required-Commit gilt damit exakt: `Applied` ist Erfolg; `AlreadyPersisted` ist
nur nach dem genannten vollständigen Nachweis Erfolg; alle anderen Status,
einschließlich `CheckpointWritten`, `AlreadyProcessed`, `NotDue` und
`NoActiveRun`, sind fail-closed auf die passende Y4-Identity zu mappen.

## 12. Configuration-Gate und Boot-Komposition

#24 konsumiert genau die reale öffentliche #57-API:

| `ConfigurationRecoveryStatus` | #24-Mapping |
|---|---|
| `RuntimeReady` | keine Config-Fault, Configuration qualified |
| `FactoryInitializationCompleted` | nur als Factory-new-Evidenz gemäß Abschnitt 5 |
| `FactoryResetCompleted` | Runtime qualified; Safety-Latches bleiben unverändert |
| `ConfigurationUnavailable`, `PersistenceReadFailure`, `PersistenceCapacityFailure`, `PersistenceWriteFailure`, `RuntimePreparationFailure` | `Y4_CONFIGURATION_UNAVAILABLE`, SAFE_BOOT |
| `ConfigurationIntegrityFailure`, `UnsupportedNewerConfigurationSchema` | `Y4_CONFIGURATION_INTEGRITY`, SAFE_BOOT |
| `BootstrapCommitIndeterminate`, `ConfigurationRecordOutcomeIndeterminate`, `ConfigurationCommitIndeterminate` | `Y4_CONFIGURATION_COMMIT_INDETERMINATE`, SAFE_BOOT |
| `CounterOverflow` | #56/#57 `Y4_CONFIGURATION_COUNTER`, SAFE_BOOT |
| `ConfigurationMutationBusy`, `ConfigurationModelBudgetBusy`, `StateTransitionRejected` | bei nicht operationalem Boot `Y4_CONFIGURATION_UNAVAILABLE`; bei sicher weiter gültiger alter Runtime kein neuer Producer, aber kein Gate-Bypass |

Der in `ConfigurationRecoveryService::boot()` erzeugte
`FactoryNoveltyProof` bleibt nicht kopierbar und wird nicht an #24
herausgegeben. #24 nutzt `FactoryInitializationCompleted` nur gemeinsam mit
seinem eigenen `NotFound`-Scan und RunPersistence-Evidenz.

## 13. SafetyEvents und #19-Handoff

Issue #24 produziert typisierte, flüchtige `SafetyEventBatch`-Werte; #19 ist
Eigentümer der Langzeithistorie und der Journalpersistenz. Es gibt keine
unbounded Queue und keine stille Trunkierung.

Variante B spart Wire-RAM: ein Batch trägt einmal
`bootSequence:uint32` und `occurredAtMonotonicMillis:uint64`. Ein
`SafetyEventBatch` beschreibt exakt **einen** bestätigten atomaren
FaultCore-RAM-, SafetyState-, Boot- oder Repair-Lifecycle-Schritt. Alle Events dieses einen
Schritts teilen den Zeitanker; Events aus späteren Commits, Side-Effects oder
späteren Bootschritten werden in einem neuen Batch publiziert. Die
**serialisierte** Eventform ist manuell Big-Endian und exakt 10 Byte:

```text
eventKind:uint8, faultCode:uint16, faultSource:uint8, instanceId:uint32,
lineageState:uint8, oldLineageKnown:uint8
```

Die Event-Wirewerte sind geschlossen und stabil:

| Wirewert | EventKind |
|---:|---|
| `0x01` | `FaultRaised` |
| `0x02` | `FaultCleared` |
| `0x03` | `FaultRelapsed` |
| `0x04` | `FaultReset` |
| `0x05` | `GateChanged` |
| `0x06` | `TerminalRequiredSet` |
| `0x07` | `TerminalRequiredChanged` |
| `0x08` | `IntentPrepared` |
| `0x09` | `RestartAttempted` |
| `0x0A` | `RestartRejected` |
| `0x0B` | `BootClassified` |
| `0x0C` | `SafeBootEntered` |
| `0x0D` | `RedundancyRepair` |
| `0x0E` | `HistoryLossDetected` |
| `0x0F` | `SafetyReinitialized` |
| `0x10` | `HistoryDiscontinuity` |

Unbekannte Eventwerte werden von #19 beim Handoff abgelehnt; die
`static_assert`-Tabelle prüft `kSafetyEventKindCount=16`. `HistoryDiscontinuity`
ist Teil dieses bestehenden, bounded `SafetyEventBatch`; es gibt keinen zweiten
Handoff und keine Queue. Für diese EventKind ist `lineageState` exakt
`PartialSuccessor` oder `TotalDiscontinuity`, und `oldLineageKnown` ist der
explizite Boolnachweis. #19 bleibt alleiniger Persistenzbesitzer und darf IDs
vor und nach einer TotalDiscontinuity nicht korrelieren.

Die Crossfields sind für jede Eventfamilie vollständig festgelegt:

| Eventfamilie | `instanceId` | `faultCode` / `faultSource` | `lineageState` | `oldLineageKnown` |
|---|---|---|---|---|
| `FaultRaised`, `FaultCleared`, `FaultRelapsed`, `FaultReset` | bei P1/O2 `0`, bei S3/Y4 die konkrete Fault-Instance | für P1/O2/S3/Y4 exakt die Catalog-Identity des Ereignisses; niemals neutral | aktueller persistierter Lineagezustand | `1`, außer während einer `TotalDiscontinuity`-Lineage `0` |
| `GateChanged`, `TerminalRequiredSet`, `TerminalRequiredChanged` | immer `0` | beide `0` als event-kind-spezifischer Neutralwert | aktueller persistierter Lineagezustand | `1`, außer während einer `TotalDiscontinuity`-Lineage `0` |
| `IntentPrepared`, `RestartAttempted`, `RestartRejected` | immer `0`; die Intent-Source bleibt im SafetyState, nicht im Event-`instanceId` | beide `0` als event-kind-spezifischer Neutralwert | aktueller persistierter Lineagezustand | `1`, außer während einer `TotalDiscontinuity`-Lineage `0` |
| `BootClassified`, `SafeBootEntered`, `RedundancyRepair` | immer `0` | beide `0` als event-kind-spezifischer Neutralwert | aktueller persistierter Lineagezustand | `1`, außer während einer `TotalDiscontinuity`-Lineage `0` |
| `HistoryLossDetected`, `HistoryDiscontinuity`, `SafetyReinitialized` | immer `0` | beide `0` als event-kind-spezifischer Neutralwert | exakt `PartialSuccessor` bei bekannter alter Lineage, sonst `TotalDiscontinuity` | exakt `1` bei `PartialSuccessor`, sonst `0` |

`faultCode=0` und `faultSource=0` sind damit ausschließlich reservierte,
event-kind-spezifische Neutralwerte für Nicht-Fault-Events; sie bedeuten dort
nicht `UnknownFaultIdentity` und dürfen nicht als FaultCatalog-Eintrag
decodiert werden. Goldenbyte- und Decode-Tests decken jede der fünf Familien,
beide Lineagezustände, `oldLineageKnown=0/1`, P1/O2 mit `instanceId=0`,
persistente Fault-Identities und die Neutralwerte ab.

Der native C++-Datentyp ist davon getrennt und darf nicht `packed` werden:

```cpp
struct NativeSafetyEvent {
    uint8_t eventKind;
    uint16_t faultCode;
    uint8_t faultSource;
    uint32_t instanceId;
    uint8_t lineageState;
    uint8_t oldLineageKnown;
};
static_assert(sizeof(NativeSafetyEvent) == 16);

struct NativeSafetyEventBatch {
    uint64_t occurredAtMonotonicMillis;
    uint32_t bootSequence;
    uint8_t eventCount;
    uint8_t reserved[3];
    std::array<NativeSafetyEvent, 36> events;
};
static_assert(sizeof(NativeSafetyEventBatch) == 592);
```

Die Wireform wird mit einem festen Bytewriter unabhängig von ABI-Padding
serialisiert. Die feste Menge ist `kMaxSafetyEventsPerMutation = 36` und ergibt
`13 + 36*10 = 373 Byte` pro Wire-Batch. Die Herleitung ist der echte Worst Case
einer atomaren Multi-Fault-Raise-Mutation: `33 * FaultRaised + GateChanged +
TerminalRequiredSet + TerminalRequiredChanged = 36`. Nicht persistente P1-/O2-Ereignisse,
`BootClassified`, `SafeBootEntered`, `RedundancyRepair`, `HistoryLossDetected`,
`HistoryDiscontinuity` und `SafetyReinitialized` tragen `instanceId=0`;
S3-/Y4-Faultereignisse tragen die zugehörige persistente InstanceId.

Die Einzelmutationen werden vollständig und ohne Crashgrenzen-Übergreifen
enumeriert:

```text
Raise mit einer neuen Identity: FaultRaised, GateChanged,
       TerminalRequiredSet, TerminalRequiredChanged = 4
atomarer Multi-Fault-Raise: höchstens 33 * FaultRaised + GateChanged +
       TerminalRequiredSet + TerminalRequiredChanged = 36
CauseClear: FaultCleared, GateChanged = 2
Relapse: FaultRelapsed, GateChanged = 2
TargetReset ohne Intent: FaultReset, GateChanged,
                          TerminalRequiredChanged = 3
TargetReset + S3RunRecovery Prepared in demselben Kandidaten:
             FaultReset, GateChanged, TerminalRequiredChanged,
             IntentPrepared = 4
Restart Prepared: IntentPrepared = 1
Restart Attempted: RestartAttempted = 1
Restart Rejected nach expliziter Port-Ablehnung: RestartRejected = 1
Boot-Klassifikation: BootClassified, SafeBootEntered = 2
RedundancyRepair: RedundancyRepair = 1
HistoryLoss Boot-Detection: HistoryLossDetected, SafeBootEntered = 2
HistoryLoss nach redundant bestätigter SafetyState-Reinitialisierung:
             HistoryDiscontinuity, FaultRaised = 2
HistoryLoss nach SafetyState und beiden qualifizierten Marker-Slots:
             SafetyReinitialized = 1
P1/O2-RAM-Lifecycle: FaultRaised/FaultCleared/FaultRelapsed (falls die
             transiente Policy Relapse veröffentlicht) plus GateChanged = höchstens 2
```

Die bisherige `RecoveryDepartureRejected`-Zeile war unzulässig, weil sie
`Commit A: TargetReset + Prepared`, `Commit B: Attempted=1`, den
Restart-Side-Effect und `Commit C: Rejected` als eine Mutation zusammenzog.
Verbindlich gilt stattdessen:

```text
Commit A: TargetReset + Prepared -> eigener Batch nach bestätigtem Commit
Commit B: Attempted=1 -> eigener Batch nach bestätigtem Commit
Side Effect: Restart request -> kein rückwirkend zusammengezogener Batch
Commit C: Rejected nur bei expliziter Port-Ablehnung -> eigener Batch
```

Nach jedem bestätigten Commit werden die zugehörigen Events unmittelbar als
eigener Batch publiziert. Ein späterer Commit darf keinen früheren Batch
nachträglich ergänzen; ein Crash zwischen den Schritten beendet den früheren
Batch an seiner bestätigten Commit-Grenze.

Ein Crash nach Prepared vor Attempted und ein Crash nach Attempted vor dem
Reset-Port dürfen daher keine gemeinsame Eventpublikation erzeugen. Die feste
Obergrenze ist `kMaxSafetyEventsPerMutation = 36`; die compile-time-Tabelle
prüft den hergeleiteten Worst Case, nicht eine künstlich kleingerechnete
Einzelmutation. `static_assert` prüft `maxEvents <= 36`,
`sizeof(NativeSafetyEvent)==16` und
`sizeof(NativeSafetyEventBatch)==592` auf Native und ESP-IDF. Ein Überschreiten
ist `Y4_INTERNAL_SAFETY`; es werden keine Events still abgeschnitten.

Ein persistenter Multi-Fault-Raise publiziert seinen einen Batch erst nach dem
einen bestätigten SafetyState-Commit. Ein Crash davor publiziert keinen
Teil-Batch; ein Crash danach sieht die vollständige durable Menge. HistoryLoss
behauptet ebenfalls nie einen künftigen Schritt: Boot-Detection publiziert nur
Detection/SAFE_BOOT; erst nach der redundant bestätigten SafetyState-Wahrheit
folgen Discontinuity/FaultRaised und erst nach beiden qualifizierten
Cleared-Markern `SafetyReinitialized`. P1/O2 sind dagegen explizit transient:
Ihre Events werden nach der abgeschlossenen atomaren RAM-FaultCore-Mutation
publiziert, ohne einen SafetyState-Commit zu behaupten. Persistente S3/Y4-Events
bleiben strikt commitabhängig.

## 14. Ressourcen-, Flash- und Wear-Proof

### 14.1 Statische Größen

```text
SafetyState payload: 32 + (33 * 24) = 824 Byte
SafetyState max Envelope: 33 + 824 + 4 = 861 Byte <= 1024
FaultCatalog: 41 Identities; persistente Records: 33 (19 S3 + 14 Y4)
EmergencyMarker payload: 26 Byte
EmergencyMarker max Envelope: 33 + 26 + 4 = 63 Byte <= 64
SafetyEventBatch Wire: 13 + (36 * 10) = 373 Byte
SafetyEventBatch native: 592 Byte
Factory-new Bootstrap: 4 bestätigte Writes (sf0, sf1, sem0, sem1),
  jeder Write mit exact readback; Cut-Points 0..4
```

Die ImmediateStop-/Gate-/FaultCore-/Directive-Hotpaths sind feste Arrays und
heapfrei. Der bestehende `IStateStore` ist dagegen ein `std::string`-Port; für
Safety-Persistenz wird deshalb ausdrücklich **keine** Heapfreiheit behauptet.
Der Port bleibt unverändert und erhält nur feste `maxBytes`-Grenzen. SafetyCore
hat keine PSRAM-Abhängigkeit; die Budgets gelten für den bestehenden bounded
String-Port und müssen in Native und ESP-IDF gegen den realen Allocator-/Stack-
Peak gemessen werden. Der verbindliche Peak-Budgetvertrag lautet:

| Anteil | Rechnung | Budget |
|---|---:|---:|
| zwei Safety-Encode-/Readback-Puffer | `2 * 1024` | 2048 B |
| zwei Marker-Puffer | `2 * 64` | 128 B |
| persistente native Records | `33 * 32` | 1056 B |
| transiente native Records, alle 8 Identities | `8 * 24` | 192 B |
| native EventBatch | `592` | 592 B |
| Codec-/Validator-Scratch | fest | 256 B |
| FaultCatalog[41] plus SafetyFaultService | `41 * 32 + 256` | 1568 B |
| **SafetyCore fixed** | Summe | **5840 B** |
| bestehender IStateStore-Heap | `3*1024 + 3*64 + 512` Allocatorreserve | **3776 B** |
| Safety-Call-Stack | gemessene feste Obergrenze | 2048 B |
| **SafetyCore Peak-Budget** | `5840 + 3776 + 2048` | **11664 B** |

`sizeof`-/`alignof`-Assertions gelten für Native und ESP-IDF; der
Ressourcenbericht muss den tatsächlichen `std::string`-/Allocator- und
Stack-Peak gegen 11664 Byte ausweisen. Wird der Budgetwert überschritten, ist
der Plan-Gate fehlgeschlagen. Keine dynamische Allokation ist im
ImmediateStop-Hotpath zulässig; Read-/Allocationfehler des bestehenden
IStateStore führen fail-closed.

### 14.2 Schreib- und Reparaturgrenzen

| Mutation | normale Writes | Readback / Repair |
|---|---:|---|
| Fault Raise/CauseClear/Relapse/TargetReset | 1 SafetyState | Zielslot readback; bei Fehler max. 2 Marker-Slotversuche |
| Restart Prepared | 1 SafetyState | exakt bestätigt |
| Restart Attempted | 1 SafetyState | exakt bestätigt vor Reset-Side-Effect |
| Restart Rejected | 1 SafetyState, nur bei expliziter Port-Ablehnung | exakt bestätigt; `Attempted=1` bleibt |
| unerwartete Rückkehr nach Requested | kein semantischer Rejected-Write | interner Fehler, kein Retry; vorhandener Attempted-Intent bleibt |
| Count-1/2-Stabilitätsrequalifikation | 1 SafetyState | exakt bestätigt; Count wird erst nach 30 Minuten aktueller Bootzeit 0 |
| Count-3 RestartLoop CauseClear | 1 SafetyState | exakt bestätigt; Record bleibt bis zum Target-Reset |
| Count-3 Service-Target-Reset | 1 SafetyState | exakt bestätigt; Record entfernt, Count 0, `safeBootRequired` bleibt |
| Bootsequence/Countermutation | 1 SafetyState | exakt bestätigt; bei Exhaustion kein Writeclaim |
| Factory-new Bootstrap | exakt 4: `sf0`, `sf1`, `sem0`, `sem1` | jeder Write exact readback; kein gesundes Leersystem an Cut-Point 0/1/2/3 |
| gleichzeitig neue persistente FaultIdentities | genau 1 SafetyState-Mutation für die vollständige neue Menge (höchstens 33 Records) | Candidate enthält alle neuen Records, Primary vor Follow-up, dann Catalog-/Priority-Reihenfolge; ein Commit/readback, keine Teil-Durability |
| normale Redundanzreparatur | höchstens 1 Write pro defektem Peer | exakt bestätigt, kein Loop |
| Marker Active/Cleared | 1 bevorzugter Slot, bei Nichtbestätigung höchstens 1 anderer Slot | exakt bestätigt |
| Ack/Mute | 0 | RAM-/Message-only |
| S3 Fallback-Auswahl | 0 | nur bestehende #17-Reads |
| #18 Recovery/Tombstone | bestehender #17 Forward-Path | dessen eigener Prepared/Slot/Committed-/CAS-Vertrag |

Damit wird weder ein unendlicher Flash-Write-Loop noch ein persistenter Ack-
Wearpfad eingeführt. Jede SafetyState-Mutation erhöht genau eine
`recordRevision`; Fault-Lifecycle-Mutationen erhöhen zusätzlich genau eine
`persistentFaultRevision`, sofern nicht Exhaustion vorliegt.
Ein gleichzeitiger Raise von bis zu 33 neuen persistenten Identities benötigt
dabei einen SafetyState-Write mit Exact-Readback statt bis zu 33 einzelner
Writes; die atomare vollständige Wahrheit reduziert gegenüber dem verworfenen
Einzelcommit-Entwurf auch den maximalen Flash-Wear dieses Falls. Der zusätzliche
EventBatch-RAM beträgt 480 Byte gegenüber dem alten 6-Event-Batch und ist im
Peak-Budget enthalten.
Der Factory-new-Bootstrap ist ein einmaliger Vier-Write-Pfad; ein Crash nach
jedem Cut-Point wird nicht durch blindes Wiederholen geheilt, sondern bleibt
SAFE_BOOT beziehungsweise folgt dem geschützten Reinitialisierungsvertrag.

Die 33er-Obergrenze ist ein Ressourcen-/Wear-Upper-Bound, kein normaler
Einzelfall: Eine Beobachtung darf jede neue Identity höchstens einmal je
aktiver Episode anlegen; Duplicate Raises bleiben ohne Mutation. Die
Einzelcommits verwenden jeweils denselben bounded SafetyState-/Markervertrag.

## 15. Modulbesitz nach ADR-013

Der kontrollierte Neustart erhält genau einen anwendungsneutralen Plattformport
in `device_platform`:

```cpp
namespace device_platform {
enum class PlatformResetCause : uint8_t {
    PowerOn,
    SoftwareRestart,
    WatchdogOrPanic,
    Brownout,
    External,
    Unknown,
};

class IResetCauseProvider {
 public:
    virtual ~IResetCauseProvider() = default;
    [[nodiscard]] virtual PlatformResetCause
    lastResetCause() const noexcept = 0;
};

enum class SoftwareRestartRequestStatus : uint8_t {
    Rejected,
    Requested,
};

class IResetController {
 public:
    virtual ~IResetController() = default;
    [[nodiscard]] virtual SoftwareRestartRequestStatus
    requestSoftwareRestart() noexcept = 0;
};
}
```

`Rejected` beweist, dass kein Restart-Side-Effect angenommen wurde. `Requested`
signalisiert in Native/Test die Reboot-Grenze; danach darf kein normaler
Anwendungspfad fortgesetzt werden. Der ESP-IDF-Adapter ruft
`esp_restart()` auf, das in ESP-IDF 6.0.2 als non-returning contractiert ist.
Kehrt ein `Requested`-Pfad wider Erwarten zurück, ist das ein interner
Vertragsfehler und nicht `Rejected`: Der bereits persistierte Versuch bleibt
verbraucht, es gibt keinen Retry und die Fachlogik geht fail-closed aus dem
normalen Pfad. Die Fachpolicy für Intent, Cause, Counter und SAFE_BOOT bleibt
vollständig in `fermentation_app::RestartSupervisor`.

Der ESP-IDF-Adapter mappt `esp_reset_reason()` vollständig und ohne App-Aufruf:

| ESP-IDF 6.0.2 Ursache | `PlatformResetCause` |
|---|---|
| `ESP_RST_POWERON` | `PowerOn` |
| `ESP_RST_SW` | `SoftwareRestart` |
| `ESP_RST_PANIC`, `ESP_RST_INT_WDT`, `ESP_RST_TASK_WDT`, `ESP_RST_WDT`, `ESP_RST_CPU_LOCKUP` | `WatchdogOrPanic` |
| `ESP_RST_BROWNOUT`, `ESP_RST_PWR_GLITCH` | `Brownout` |
| `ESP_RST_EXT` | `External` |
| `ESP_RST_UNKNOWN`, `ESP_RST_DEEPSLEEP`, `ESP_RST_SDIO`, `ESP_RST_USB`, `ESP_RST_JTAG`, `ESP_RST_EFUSE` | `Unknown` |

`Unknown` ist absichtlich fail-closed und darf keinen vorbereiteten
S3-Handoff freigeben. Der native Mock liefert nur einen deterministischen
`PlatformResetCause`; er kennt weder App-`ResetCause` noch Faulttypen.

ESP-IDF-first für Watchdog:

```text
Anwendung / Fall A:
  der reale #24 SafetyWatchdogProducer im bestehenden periodischen
  Safety-/Application-Pfad erkennt einen logischen Heartbeat-/Liveness-Stall,
  solange der Supervisor noch läuft
  -> ImmediateStop -> durable Fault + Attempted -> kontrollierter Restart

ESP-IDF TWDT:
  Produktionspfad = echter Reset-Backstop, nicht nur Logging.
  CONFIG_ESP_TASK_WDT_EN=y
  CONFIG_ESP_TASK_WDT_PANIC=y
  CONFIG_ESP_SYSTEM_PANIC wählt CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT
  -> TWDT-Timeout -> Panic-Handler -> automatischer Systemreset.
  Alternativ ist nur eine explizit getestete
  esp_task_wdt_config_t.trigger_panic=true-Konfiguration zulässig, wenn sie
  dieselbe Wirkung mit derselben expliziten Panic-Reboot-Policy herstellt.

ESP-IDF IWDT:
  CONFIG_ESP_INT_WDT=y und explizites CONFIG_ESP_INT_WDT_TIMEOUT_MS;
  eigener Interrupt-/CPU-Backstop, nicht der SafetyTaskRecovery-Producer.
  -> Panic oder zweiter Hardware-Watchdog-Reset
  -> nächster Boot klassifiziert WatchdogOrPanic.
```

Die TWDT-Panic- und automatische Reboot-Policy ist produktiv verbindlich; es
gibt keine Abhängigkeit von Kconfig-Defaults. Ein ESP-IDF-Test muss einen
echten TWDT-Timeout auslösen und nach dem Neustart nachweisen, dass nicht nur
eine Warnung geloggt wurde, sondern `WatchdogOrPanic` erkannt,
`abnormalRestartCount` erhöht und keine vorherige `SafetyTaskRecovery`-
Persistenz erfunden wurde. `esp_task_wdt_init`,
`esp_task_wdt_add`/`esp_task_wdt_add_user`, `esp_task_wdt_reset`/
`esp_task_wdt_reset_user` und `esp_task_wdt_reconfigure` werden nur im
ESP-IDF-Adapter über den öffentlichen `esp_task_wdt.h`-Vertrag verwendet.
IWDT läuft über `CONFIG_ESP_INT_WDT` und `CONFIG_ESP_INT_WDT_TIMEOUT_MS`; eine
private Espressif-API oder `esp_private/esp_int_wdt.h` ist unzulässig.

Der Plan führt keine eigene parallele Timer-, Hardware-Watchdog- oder
FreeRTOS-Task-Infrastruktur ein. Die sechs Catalog-Domains sind logische
Liveness-Checkpoints des einen Producers, keine behaupteten eigenen Tasks.
Ein physischer Sensorfehler und ein vollständiger Supervisor-Stall erzeugen
daher keinen vorpersistierten `SafetyTaskRecovery`-Intent.

```text
fermentation_app:
  SafetyFaultService, FaultCatalog, SafetyState codec/store wrapper,
  EmergencyMarker, RestartSupervisor, ResetCause-Policy, ServiceResetProof,
  Boot-/Process-/Recovery-Orchestration, SafetyEventBatch und #24 Producer-
  Mapping; app-interner Testhelper für ServiceResetProof

device_platform:
  IStateStore, StorageEnvelope, SlotScanner, Time-Port,
  IResetCauseProvider, PlatformResetCause, IResetController

device_platform_esp_idf:
  ESP-IDF ResetCause-/Reset-/TWDT-/Time-/Store-Adapter

device_platform_test_support:
  nur generische Store-, Auth-/Session-, Reset-, ResetCause-, Clock- und
  Producer-Injections; kein ServiceResetProof und keine Fault-/Lineage-Logik
```

`device_platform` erhält keine fermentation-spezifischen Fault-, SafetyState-
oder Run-Typen. `src/main.cpp` bleibt Composition Root.

## 16. Kern-Proof: #17/#18 wird nicht verbogen

1. **#17 bleibt forward-only:** `writeSnapshotCore()` in
   `run_persistence_coordinator.cpp:1404-1685` bildet Prepared, schreibt den
   Slot und committed danach den Head; `RunPersistenceHead` und
   `RunCheckpointReference` sind in `run_persistence_contract.hpp:46-86`
   unverändert forward-only.
2. **Kein Head-Rollback, ehrlicher Current/Fault-Proof:**
   `loadAndInitialize()` lädt bei gültigem Current in
   `run_persistence_coordinator.cpp:371-444` den Current, setzt
   `LoadedActiveRun` und kehrt zurück. Der bestehende Fallbackpfad
   `446-485` ist deshalb nur der Beweis für den beschädigten-Current-Fall.
   `prepareSafetyFallbackRecovery()` ist der neue schmale #17-interne
   read-only Handoff für `LoadedActiveRun + Current/Fault`: Er verwendet den
   extrahierten privaten `loadReference`-Helper, lädt den referenzierten
   Fallback in RAM, stellt ID-Fenster und Caches korrekt her und setzt erst
   dann `FallbackRecoveryPending`. Weder dieser Handoff noch der Helper
   schreibt Head, Slot oder Fallbackreferenz.
3. **Kein neues Wirefeld/Schemachange:** #17 bleibt
   `kCurrentRunPersistenceSchema=3` (`run_persistence_contract.hpp:19-31`);
   Intent, Latch und Terminalpflicht leben im RecordType 9/10, nicht in
   Head/Checkpoint.
4. **Fallback physisch unverändert:** `loadReference()` prüft Slotread,
   Envelope, Epoch, Schema, Codec und `runCheckpointReferenceMatches()` in
   `run_persistence_coordinator.cpp:331-369`; ein Fallback muss exakt zum
   bestehenden Head passen.
5. **#18 bleibt alleinige fachliche Recoveryautorität:** Die vorhandene
   `activateFallbackRecoveredRun()`-Grenze (`run_persistence_coordinator.cpp:
   760-925`) baut #18-Anker/RecoveryEvaluation und verwendet anschließend den
   bestehenden Write-before-Apply-Pfad. #24 liefert nur die geprüfte RAM-Quelle.
6. **Y4 ist rebootfest:** `runRecoveryForbidden` ist SafetyState-Payloadfeld
   und wird zusammen mit dem Y4-Latch geschrieben; ein Reboot kann es nicht
   aus RAM oder einem Fault-Text verlieren.
7. **NoActiveRun ist kanonisch:** Nur die geplante terminale Standby-
   Transition bildet den NoActiveRun-Snapshot; die #17-Validierung erlaubt
   keinen `NoActiveRun + Fault`-Zustand.
8. **Safety blockiert ohne Rekonstruktion:** Safety kann durch Gate,
   `runRecoveryForbidden` und SAFE_BOOT einen alten Run blockieren, ohne dessen
   Payload als neue gültige Run-Wahrheit zu kopieren oder zu reaktivieren.

Damit sind die sechs Kern-Gates PASS: verwendbarer Pre-Fault-Fallback,
explizite RAM-only Auswahl bei gültigem Fault-current, #18-Autorität, kein
#17-Schemawechsel, forward-only Persistenz und persistente Y4-Terminalität.

## 17. Architekturdiagramm Y4

```text
ACTIVE RUN
   |
   | Y4
   v
SafetyState:
  Y4 latch
  runRecoveryForbidden = true
   |
   v
FAULT / SAFE_BOOT
   |
   +--> old persisted run can NEVER enter S3 recovery
   |
Run-Abandon + Tombstone oder CauseClear + Service Reset
   |
   +--> blocker bleibt: Fault/SafeBoot, flag true
   |
terminaler Standby-Punkt:
   +--> Fault -> Standby-Kandidat
   +--> clearActiveRunState
   +--> kanonischer NoActiveRun/STANDBY-Tombstone via #17
   +--> readback/commit bestätigt
   +--> runRecoveryForbidden=false
   |
   v
STANDBY / separater SAFE_BOOT-Exit
```

## 18. Mandatory Injection-Matrix aus `ACCEPTANCE_TESTS.md`

Jede Pflichtinjektion erhält genau eine Zuständigkeitsklasse. `injection-only`
heißt, dass es ohne bestätigten Hardwareproducer keine produktive
Erkennungsthese gibt; der Safety-Effekt wird trotzdem deterministisch getestet.
Die Statusspalte verwendet ausschließlich genau eine der fünf Klassen
`#24 implementiert`, `bestehenden Vertrag wiederverwenden`, `injection-only`,
`BLOCKED_HARDWARE` oder `anderes exaktes Issue`; weitere Zuständigkeiten stehen
nur in der Erwartungsspalte.

| Injection | Klassifikation | Erwartung |
|---|---|---|
| Air-Sensor CRC/Bus/Read | bestehenden Vertrag wiederverwenden | STALE transient, FAILED S3; #20-Injection, kein erfundener Hardwarestatus |
| Schrankluftsensor-Ausfall in Standby | bestehenden Vertrag wiederverwenden | keine Aktorfreigabe; Requalifikation vor Start |
| Schrankluftsensor-Ausfall in Preheating | bestehenden Vertrag wiederverwenden | Peltier aus, Safety-Fanpolicy, kein stiller Laufresume |
| Schrankluftsensor-Ausfall in Fermenting | bestehenden Vertrag wiederverwenden | Peltier aus; O2/S3 gemäß STALE/FAILED |
| Schrankluftsensor-Ausfall in Cooling | bestehenden Vertrag wiederverwenden | Peltier aus; Cooling-/Fan-Safety bleibt aktiv |
| Cooling-Sensor CRC/Bus/Read | bestehenden Vertrag wiederverwenden | wie Air; Peltier bleibt aus |
| Cooling-/Kühlkörpersensor-Ausfall während Peltierbetrieb | bestehenden Vertrag wiederverwenden | Peltier sofort aus, #20-Injection und Lüfter gemäß Catalog |
| Product-Sensor entfernt/CRC/Stale/Failed | bestehenden Vertrag wiederverwenden | AirFallbackActive respektieren, Return nur requalifiziert |
| Missing/out-of-range/stuck/repeated sensor | bestehenden Vertrag wiederverwenden | exakte Sensorphase; kein stilles Fallback |
| widersprüchliche Produkt-, Luft- und Kühlkörperwerte | bestehenden Vertrag wiederverwenden | O2 bis eindeutig, S3 bei ungelöst sicherheitskritisch |
| Sensor contradiction | bestehenden Vertrag wiederverwenden | O2 bis eindeutig, S3 bei ungelöst sicherheitskritisch |
| veraltete Regelanforderung | bestehenden Vertrag wiederverwenden | #23 stale request abweisen, keine Aktorfreigabe |
| kurze Gegenanforderung | bestehenden Vertrag wiederverwenden | #23 Mindestzeit/Accumulator korrekt, keine direkte Safety-Recovery |
| dauerhafte Gegenanforderung | bestehenden Vertrag wiederverwenden | #23 Watchdog/Timing, kein Umgehen der Gate-Directive |
| Mindest-Ausschaltzeit | bestehenden Vertrag wiederverwenden | #23: kein früher Restart |
| Totzeit | bestehenden Vertrag wiederverwenden | #23: keine direkte Richtungsumschaltung |
| Abbruch Servicepuls | bestehenden Vertrag wiederverwenden | #23/Serviceboundary: sofortige sichere Abschaltung |
| Stopoption `Back` | bestehenden Vertrag wiederverwenden | #15: Dialog schließen, keinerlei Mutation |
| Stopoption `AbortAndTurnOff` | bestehenden Vertrag wiederverwenden | #15: alter Run atomar abbrechen, Peltier aus, Standby |
| Stopoption `AbortAndCool` | bestehenden Vertrag wiederverwenden | #15: alter Run beendet, neuer validierter manueller Cool-Run |
| Peltier-Test ohne Temperatursicherung | BLOCKED_HARDWARE | Serviceoperation blockiert |
| Aktortest aus SAFE_BOOT | #24 implementiert | immer blockiert |
| Safety-Intervention | #24 implementiert | #23 führt den Handoff aus; vor #35/#33 effectiveAttemptLimit 0, danach validierter Satz 1 bis höchstens 2 |
| Hard Emergency | #24 implementiert | beide Richtungen aus, keine Gegenrichtung |
| invalid runtime/config size | bestehenden Vertrag wiederverwenden | #56/#57: Y4 Config/SAFE_BOOT |
| ConfigurationRuntimeFailure | bestehenden Vertrag wiederverwenden | #56: Y4_CONFIG_UNAVAILABLE |
| ConfigurationUnavailable | bestehenden Vertrag wiederverwenden | #57: Y4_CONFIG_UNAVAILABLE |
| ConfigurationIntegrityFailure | bestehenden Vertrag wiederverwenden | #57: Y4_CONFIG_INTEGRITY |
| CommitOutcomeUnknown/Indeterminate | bestehenden Vertrag wiederverwenden | #56/#57: Y4_CONFIG_COMMIT_INDETERMINATE |
| simultaneous H-bridge directions | bestehenden Vertrag wiederverwenden | #23-Injection: S3_HBRIDGE_CONFLICT |
| invalid/unknown ActuatorPlan | injection-only | #23-Grenze: ImmediateStop, keine physische Behauptung |
| planner watchdog | bestehenden Vertrag wiederverwenden | #23 real, #24 Mapping: S3/Y4 gemäß Catalog, `forceStop()` |
| SafetyTaskRecovery bei internem Sensor-/Treiberstall | #24 implementiert | realer #24 SafetyWatchdogProducer, supervised-only plus deterministische Injection, genau ein Restartversuch, Latch bleibt, kein #18-Handoff im ersten Boot |
| SafetyTaskRecovery bei Safety-/Aktor-/Main-Taskstall | #24 implementiert | realer zentraler Producer mit logischem Checkpoint, supervised-only plus Injection, genau ein Versuch, kein künstlicher FreeRTOS-Task und kein Auto-Loop |
| plausibler physischer Sensorfehler | bestehenden Vertrag wiederverwenden | kein SafetyTaskRecovery-Restart |
| implausible actuator feedback | injection-only | S3_PELTIER_UNSAFE_OUTPUT, keine erfundene Hardware |
| fan failure/off during Peltier | injection-only | Functional/Electrical Fan-Identity exakt trennen; kein Tacho behaupten |
| Peltier/H-bridge fault | injection-only | S3/Y4 je Catalog, bis ein realer Producer existiert |
| missing thermal response | bestehenden Vertrag wiederverwenden | #22/#23: O2 Diagnose, bei Eskalation S3 |
| Mindest-Ausschaltzeit im SafetyRecovery | bestehenden Vertrag wiederverwenden | #23: kein Request vor Ablauf |
| Totzeit im SafetyRecovery | bestehenden Vertrag wiederverwenden | #23: keine direkte Richtungsumschaltung |
| SafetyRecovery-Versuch 1/2 | #24 implementiert | #23-Handoff; #35 bestimmt den validierten Satz; Limit 0/1/2 und rebootfeste Episode |
| Reboot/Brownout/Watchdog nach SafetyRecovery-Versuch 1 | #24 implementiert | Episode beendet automatische Recovery; kein Versuch 2 |
| Abbruch eines SafetyRecovery-Servicepulses | BLOCKED_HARDWARE | #23/#33/#35: sofort AUS, kein weiterer Versuch |
| interrupted SafetyState write | #24 implementiert | ImmediateStop, Marker, SAFE_BOOT |
| interrupted Marker write | #24 implementiert | RAM fail-closed, kein Retryloop |
| unvollständigen Transaktionsmarker hinterlassen | #24 implementiert | Marker Active/indeterminate, SAFE_BOOT und kein Latch-Clear |
| kritischen Speicher nicht lesbar oder nicht schreibbar | #24 implementiert | Injection-only Storefehler: ImmediateStop, EmergencyMarker best effort, SAFE_BOOT |
| kritischen Schreibfehler bei aktiver Aktoranforderung | #24 implementiert | Injection-only: kein weiterer Aktorbefehl nach Fehler, Marker/SAFE_BOOT |
| Persistenzfehler-Latch setzen und Neustart ausführen | #24 implementiert | Latch bleibt, Neustart ist kein Reset, SAFE_BOOT |
| Schreiben des minimalen persistenten Latches zusätzlich fehlschlagen lassen | #24 implementiert | Injection-only: RAM-Latch bleibt, kein Retryloop, fail-closed |
| interrupted RunPersistence write | bestehenden Vertrag wiederverwenden | #17: Y4 RunStore, SAFE_BOOT, Abandon |
| repeated watchdog reset | #24 implementiert | Count 3 -> Y4_RESTART_LOOP/SAFE_BOOT |
| Watchdog und Bootschleife bis SAFE_BOOT | #24 implementiert | genau ein zulässiger Recoveryversuch, danach Count/SAFE_BOOT; kein Auto-Loop |
| Brownout/Unknown reset | injection-only | abnormal, kein S3-Handoff |
| repeated Brownout | #24 implementiert | injection-only Resetfolge, abnormal counter, bei 3 Y4_RESTART_LOOP/SAFE_BOOT |
| Unterbrechung in jeder wesentlichen Phase | bestehenden Vertrag wiederverwenden | #18 entscheidet konservativ, keine erfundene Laufzeit |
| Neustart mit persistierter Sicherheitsverriegelung | #24 implementiert | SAFE_BOOT, kein Allowed und kein direkter Recovery-Handoff |
| invalid persisted enum | #24 implementiert | Codec-Decode fail, Y4_UNKNOWN_SAFETY_STATE |
| impossible persisted state | #24 implementiert | Crossfield-Decode fail, SAFE_BOOT |
| interrupted multi-object/config write | bestehenden Vertrag wiederverwenden | #56/#57: Config Y4, keine Runtimefreigabe |
| PreparedInterrupted | bestehenden Vertrag wiederverwenden | #17: Y4 RunPrepared, Abandon |
| corrupt current + valid fallback | #24 implementiert | #17 liefert Evidenz; RAM-only FallbackRecoveryPending, #18 entscheidet |
| `FallbackRecovered` ohne S3Intent | bestehenden Vertrag wiederverwenden | bestehender #18-Pfad bleibt aktiv |
| `Current/Fault` plus gültiger S3RunRecovery-Intent | #24 implementiert | read-only Handoff -> FallbackRecoveryPending, danach #18 |
| corrupt fallback | bestehenden Vertrag wiederverwenden | #17-Evidenz: kein Resume, Y4 RunStore/terminal |
| one-slot SafetyState corruption | #24 implementiert | Winner konservativ laden, Repair/Y4 bleibt |
| total SafetyState loss | #24 implementiert | SAFE_BOOT, Service-Reinit mit historyLoss |
| vier Keys NotFound plus #57 FactoryInitializationCompleted | #24 implementiert | #57-Status konsumieren; SafetyState und beide Cleared-Marker schreiben/readback |
| Crash nach jedem Factory-/Marker-Initialwrite | #24 implementiert | persistente Cut-Point-Injection; kein gesundes Leersystem behaupten |
| partieller SafetyState-Loss mit bekannter Lineage | #24 implementiert | checked successor lineage, alte Proofs stale |
| totaler SafetyState-Loss ohne bekannte Lineage | #24 implementiert | TotalDiscontinuity, keine globale Epochbehauptung |
| zweiter Totalverlust | #24 implementiert | neue Discontinuity, alte Proofs/Intents ungültig |
| Active/corrupt Marker | #24 implementiert | Active/Corrupt hält fail-closed |
| full Fault capacity | #24 implementiert | keine Eviction, Y4/counterstatus |
| counter overflow | #24 implementiert | keine nicht darstellbare Mutation, SAFE_BOOT |
| Safety-eigene High-Watermark-Exhaustion | #24 implementiert | abgeleiteter Status, kein FaultRecord/InstanceId-Verbrauch, SAFE_BOOT |
| #17 CounterOverflow | #24 implementiert | Producerstatus konsumieren; `0x040E Y4_RUN_PERSISTENCE_COUNTER`, wenn SafetyState mutierbar |
| #56/#57 CounterOverflow | #24 implementiert | Producerstatus konsumieren; `0x040F Y4_CONFIGURATION_COUNTER`, wenn SafetyState mutierbar |
| latest run checkpoint corruption + fallback | #24 implementiert | #17-Fallback nur referenziert/validiert, kein Head-Rollback |
| neuesten Kontrollpunkt beschädigen | #24 implementiert | #17 Fault-current bleibt Head; vorhandener Fallback nur read-only |
| latest configuration revision damaged | bestehenden Vertrag wiederverwenden | #57-Status entscheidet; alte Runtime sonst Y4 |
| Rueckfallrevision prüfen | bestehenden Vertrag wiederverwenden | #17: Referenz, Slot, CRC, Runidentität und Schema müssen exakt passen |
| Recovery write success but readback fails | #24 implementiert | #17/#24 required path nicht bestätigt -> fail-closed |
| `AlreadyPersisted` ohne exakten Readback-Nachweis | #24 implementiert | Y4/fail-closed, kein Durability-Claim |
| `CheckpointWritten`, `AlreadyProcessed`, `NotDue` im Required-Pfad | #24 implementiert | Y4/fail-closed, kein Durability-Claim |
| Latch reset before storage check | #24 implementiert | Reject |
| Latch reset außerhalb des geschützten Serviceablaufs | #24 implementiert | #15-Grenze: Reject; keine Fault- oder SafetyState-Mutation |
| Latch reset with open Marker | #24 implementiert | Reject |
| Latch reset after Read/Write-Test und dokumentiertem Serviceereignis | #24 implementiert | nur bei aktueller Lineage, CauseClear und echter Auth; danach Target-Reset |
| History store full | bestehenden Vertrag wiederverwenden | #19; kein kritischer Safetyverlust, Y4 wenn Safety-Record betroffen |
| noncritical RAM/export error | bestehenden Vertrag wiederverwenden | O2/warning, kein Safety-Truth overwrite |
| missing NTP | bestehenden Vertrag wiederverwenden | #18: `RECOVERY_TIME_PENDING`, kein erfundener Progress |
| later NTP sync | bestehenden Vertrag wiederverwenden | #18: Intervall/Bounds, keine monotone Cross-Boot-Subtraktion |
| outage inside phase | bestehenden Vertrag wiederverwenden | #18-Phaseentscheidung |
| outage across phase boundary | bestehenden Vertrag wiederverwenden | kein automatischer Abschluss bei Unsicherheit |
| WLAN outage with safe process | bestehenden Vertrag wiederverwenden | Safety/Regelung ohne WLAN |
| persisted COMPLETED after reboot | bestehenden Vertrag wiederverwenden | #17: COMPLETED bleibt COMPLETED |
| acknowledge without reset | #24 implementiert | Message-only, Gate unverändert |
| reset while cause active | #24 implementiert | #15 Target rejected |
| service function without PIN | #24 implementiert | Auth-Grenze: fail-closed |
| vergessene Service-PIN mit lokalem PIN-unabhängigem Vollreset | anderes exaktes Issue | #15: kein Safety-/Aktorbypass; nur kanonischer Vollresetvertrag |
| isolierter PIN-Reset ohne Vollresetkontext | anderes exaktes Issue | #15: abgelehnt; keine Safety-Latch-Mutation |
| FaultReset mit frei gesetztem `authorizationSatisfied=true` | #24 implementiert | Trust-Boundary: Produktionsreset abgelehnt |
| Start/Stop/Adjust/SensorSelection mit frei gesetztem Safety-Boolean | #24 implementiert | keine Freigabe; interne SafetyView entscheidet |
| Request mit frei gesetzten Sensor-valid-Booleans | bestehenden Vertrag wiederverwenden | #20/#21: Request beweist keine Sensorqualität |
| actuator test during run | bestehenden Vertrag wiederverwenden | #15/#23 boundary: blockiert |
| actuator test in SAFE_BOOT | #24 implementiert | blockiert |
| conflicting Display/Web action | bestehenden Vertrag wiederverwenden | #15 envelope/revision: stale/conflict, kein Safetybypass |

## 19. Lifecycle-, S3-, Y4- und SAFE_BOOT-Testmatrix

### 19.1 Lifecycle

- duplicate Raise derselben Identity ohne neue InstanceId;
- zwei gleichzeitige persistente Faults, S3+Y4, mehrere Y4 und alle 33
  zulässigen persistenten Identities in einem Producer-Snapshot: genau ein
  Candidate/Commit, keine Teil-Durability, keine Eviction, Primary vor
  Follow-up und danach Catalog-/Priority-Reihenfolge;
- Powerloss vor dem einzigen Multi-Fault-Commit und nach dessen bestätigtem
  Readback; eine stale ResetEvaluation nach der atomaren Mutation wird
  abgelehnt, und `persistentFaultRevision` steigt genau einmal;
- gleichzeitige S3/Y4-Identities, Dominanz und vollständige Fanaggregation;
- unabhängige Unknown-/Internal-Ursachen ohne künstliche Primary-Beziehung;
- CauseClear, Relapse und stale ResetEvaluation;
- Target-Reset bei vorhandenem Blocker, danach Release erst beim letzten Blocker;
- 2x S3, S3+Y4, 2x Y4 und Primary-/Follow-up-Reihenfolge;
- Ack ohne Safetywirkung; P1/O2 ohne persistente IDs/Revision;
- Reboot erhält Latches, Revision, History-Epoch und Terminalflag.

### 19.2 S3

Für `Preheating`, `WaitingForProduct`, `ReachingTarget`, `QualifyingTarget`,
`Fermenting`, `Cooling`, `CoolHolding` und `ManualHolding`:

- S3 -> ImmediateStop -> Fault-current + Pre-Fault-Fallback;
- CauseClear + Service-Target-Reset, Intent und at-most-once Restart;
- SoftwareRestart/External/PowerOn-Kompatibilität und Brownout/Watchdog/Unknown;
- RAM-only Fallback-Auswahl ohne Store-Write, identische Run-/Programmbindung,
  vollständige `persistedIds_`-/High-Watermark-Herstellung;
- kein Planner-/Sink-Tick vor #18, #18 Resume oder Terminalentscheidung;
- `t0..t1` und `t1..t2` als Unsicherheit/Unterbrechung;
- fehlender, korrupt/falscher Fallback -> kein Resume, Tombstone;
- gültiger Current/Fault plus gültiger Fallback: `loadAndInitialize()==Current`
  und `LoadedActiveRun`; erst `prepareSafetyFallbackRecovery()` lädt den
  Fallback read-only, setzt `FallbackRecoveryPending` und erlaubt danach die
  bestehende `activateFallbackRecoveredRun()`-Grenze;
- S3-Latch noch aktiv und Recovery nicht beweisbar: kein Intent, alter Run
  zunächst über den bestehenden #17-Abandon zu NoActiveRun/STANDBY mit exact
  readback, dann erst TargetReset ohne Intent und separater SAFE_BOOT-Exit;
- Prepared-S3RunRecovery ohne handoffberechtigte Matrix oder Fallback-Evidenz:
  Crash vor Tombstone, Tombstone indeterminate, Tombstone vor Intent-Clear,
  Prepared plus bereits durablem NoActiveRun/STANDBY sowie Intent-Clear vor
  SAFE_BOOT-Exit terminalisieren ohne Resume, Restart-Retry oder neue Y4;
- Crash vor/nach Safety-Commit, Fault-Run-Commit, Reset-Commit,
  Prepared/Attempted, Restart-Port, Boot-Handoff und #18-Write;
- Crash nach durable #18-Fortschritt vor Intent-Clear: kein Replay, Intent
  anhand der neuen Run-Wahrheit abschließen;
- `Restart Rejected`: kein Auto-Retry, späterer Service-/Owner-Reboot möglich.

### 19.3 Y4 und Abandon

- Y4 allein mit aktivem Run, Y4+S3, mehrere Y4;
- Reboot bei aktivem Y4; späterer S3-Reset darf nie recovern;
- RunPersistence-Y4 Abandon vor CauseClear/Reset;
- Crash vor, zwischen und nach Tombstone und Flag-Clear;
- Standby-Tombstone ist exakt `NoActiveRun` + `Standby`, niemals Fault;
- SAFE_BOOT und separater Exit bleiben bei jedem Y4-Blocker korrekt.

### 19.4 Run-Abandon und Store

Für `NotReconstructible`, `NotReconstructibleOrphanedState`,
`PreparedInterrupted`, `ReadFailed`, `CapacityExceeded`, `UnsupportedSchema`
und `ForeignEpoch` werden technische Lesbarkeit, CAS-Preimage,
Forward-Revision, Readback und unveränderte Configuration/Programme geprüft.
Zusätzlich: beide SafetySlots verloren, ein SafetySlot korrupt, Marker Active,
Marker corrupt, History-Loss-Reinit und Counterexhaustion.

### 19.4a EmergencyMarker und History/Event-Grenzen

- Factory-Tie und Active-Tie gleicher Revision und byteidentischem Payload
  wählen `sem0`; gleiche Revision mit unterschiedlichem gültigem Payload ist
  `MarkerIndeterminate`, ImmediateStop und SAFE_BOOT;
- jedes Marker-Envelop-/Payload-Crossfield, alle erlaubten State-/Reason-
  Kombinationen sowie Null-/Nichtnull-Semantik von attempted revision,
  bootSequence und monotonic millis erhalten Goldenbyte-/Decode-Tests;
- ein alter Active-Marker ist erst nach neuem Cleared Exact-Readback, Peer-
  Repair auf identische Revision/Bytes und Qualifikation beider Slots gesund;
  Crash während dieses Repairs bleibt nicht healthy/Cleared;
- HistoryLoss publiziert getrennt Boot-Detection, nach redundanter SafetyState-
  Reinitialisierung bestätigte Discontinuity/FaultRaised und erst nach beiden
  Markern SafetyReinitialized; ein Crash zwischen diesen Schritten kündigt
  keinen künftigen Schritt an;
- P1/O2 Raise/Clear/Relapse und GateChanged werden ausschließlich nach ihrer
  atomaren RAM-FaultCore-Mutation publiziert, ohne SafetyState-Commit zu
  behaupten.

### 19.5 SAFE_BOOT/Restart

Factory-init, pre-#24-Upgrade, fehlender SafetyState auf Nicht-Factory-Gerät,
alle ResetCause-Werte, gültiger/ungültiger Intent, Attempted/Rejected,
Counter 0/1/2/3, 30-Minuten-Requalifikation, Safety-Redundanzrepair,
`safeBootRequired`, Crash nach Flag-Clear und die vollständige Exit-Matrix.

Zusätzlich sind folgende Cut-Points Pflicht: S3 ActiveRun -> Reboot vor
Reset -> `safeBootRequired=true` -> CauseClear + ServiceReset -> atomarer
`Prepared`-Commit bei weiter fail-closed qualifiziertem Handoff -> Crash
davor/danach -> Attempted
vor `IResetController` -> #18 Resume und #18 Reject; neuer Safetyfault während
Departure; `SafetyTaskRecovery` genau einmal; physischer Sensorfehler ohne
Task-Recovery; S3RunRecovery separat; `Attempted=0 + SoftwareRestart` ohne
Handoff; Brownout/Watchdog/Unknown; Count 3 -> Y4/SAFE_BOOT.

Factory-/Marker-Cutpoints decken die vier `NotFound`-Keys,
`FactoryInitializationCompleted`, beide SafetyState-Slots und beide
Cleared-Marker ab. Es werden exakt die Cut-Points nach 0, 1, 2, 3 und 4
bestätigten Bootstrap-Writes getestet; nur Cut-Point 4 ist redundant healthy,
alle früheren bleiben fail-closed. History-Loss deckt partielle bekannte Lineage, totalen Loss ohne
bekannte Lineage, zweiten Totalverlust und ungültige alte
ResetEvaluation/ServiceProof/RestartIntent ab. Safety-Countertests trennen
abgeleitete eigene Exhaustion, externe Domain-Counter und Goldenbytes an
`UINT32_MAX`/`UINT64_MAX`.

Der #17-Test trennt `FallbackRecovered` ohne S3Intent vom
`Current/Fault + S3RunRecovery`-Handoff und prüft Required-Status inklusive
`AlreadyPersisted`-Readback. S3RunRecovery-Tests trennen Candidate-time-
Provenienz (aktives Preimage, CauseClear, exakter ServiceResetProof, atomare
Entfernung plus Intent) von Decode-time (Source nicht aktiv, gültige Lineage-
und Crossfields, #17 Current/Fault, physisch verifizierter Fallback und
ResetCause). Ressourcen-/Eventtests prüfen native und ESP-IDF-`sizeof`/`alignof`,
alle acht transienten Identities, tatsächlichen RAM-/Heap-Peak und
Markerrevision oberhalb `UINT32_MAX`.

### 19.6 Verbindliche Cross-Contract-Zusatzmatrix dieser Revision

Restart-Fenster:

- Count 1 nach 30 Minuten aktueller gesunder Bootzeit wird 0;
- Count 2 nach 30 Minuten aktueller gesunder Bootzeit wird 0;
- Count 2 mit abnormalem Reset vor Ablauf wird 3;
- Count 3 nach 30 Minuten erlaubt nur CauseClear für `Y4_RESTART_LOOP`;
- RestartLoop selbst zählt bei dieser Prüfung nicht als „anderer Y4“;
- zusätzlicher S3- oder Y4-Latch blockiert CauseClear;
- Service-Reset vor CauseClear wird abgelehnt;
- Service-Reset danach entfernt den RestartLoop und setzt Count 0, während
  `safeBootRequired` bis zum separaten Exit gesetzt bleibt;
- `QRS` wird verwendet, `QR` niemals.

SafetyTaskRecovery:

- jede der sechs logischen Watchdog-Domains verwendet den einen realen
  `SafetyWatchdogProducer` und eine deterministische Injection;
- Fall A: ein noch laufender Supervisor erkennt einen stale Subpfad und
  persistiert `S3_<DOMAIN>_STALL` plus `SafetyTaskRecovery` vor dem kontrollierten
  Restart;
- Fall B: vollständiger Supervisor-/`app_main`-Stall lässt keinen eigenen Write
  zu und führt ausschließlich über TWDT/IWDT zu `WatchdogOrPanic`,
  `abnormalRestartCount++` und ohne erfundene SafetyTaskRecovery-Persistenz;
- Crash vor Prepared, zwischen Prepared und Attempted, nach Attempted vor dem
  Reset, erfolgreicher SoftwareRestart und zweiter Boot ohne ServiceReset;
- derselbe Stall bleibt aktiv: kein zweiter automatischer Side Effect;
- Task-/Treiber-Requalifikation, CauseClear und ServiceReset schließen die
  Episode; Relapse nach CauseClear erzeugt eine neue Instance und wieder genau
  einen Versuch;
- aktiver Run folgt danach ausschließlich dem separaten
  `S3RunRecovery`-Restart und #18-/Terminalpfad;
- kein aktiver Run folgt `FaultResetCompleted` nach Standby beziehungsweise
  separatem SAFE_BOOT-Exit;
- plausibler physischer Sensorfehler erzeugt keinen TaskRecovery-Restart.

ESP-IDF-Adapter:

- vollständige `esp_reset_reason()`-Mappingtabelle einschließlich
  SoftwareRestart, Brownout, Panic, TWDT und IWDT;
- TWDT-Timeout beweist echten Panic-/Systemreset statt bloßem Log,
  `CONFIG_ESP_TASK_WDT_PANIC=y` plus automatische
  `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT`-Policy (oder explizit äquivalentes
  `trigger_panic=true`), echter TWDT-User/realer Pfad statt künstlicher Tasks
  und IWDT als eigener Backstop;
- `esp_restart()` als non-returning Grenze; Mock-`Requested` beendet den
  normalen Pfad, unerwartete Rückkehr ist nicht `Rejected`.

ThermalIntervention:

- Limit 0, 1 und 2; Versuch 1 und Versuch 2;
- Reboot, Brownout, Watchdog/Panic oder Persistenz-Indeterminate nach Versuch 1
  sperren weitere automatische Versuche dauerhaft für diese Episode;
- HardEmergency während des Pulses, neuer Fan-/Sensor-/Aktorfehler, falscher
  Trend und keine dritte Anforderung;
- nach jedem Puls Peltier AUS, Evidenzen neu prüfen und nur bei gültigem
  #35-Fortsetzungsorakel fortsetzen.

Events und Identitäten:

- zwei, S3+Y4, mehrere Y4 und maximal alle 33 gleichzeitig erkannten
  persistenten Ursachen werden zuerst aggregiert und dann gemeinsam in einem
  Kandidaten/Commit durable; Primary/Follow-up, deterministische InstanceIds,
  genau eine FaultRevision und kein Teilzustand sind nachzuweisen;
- Powerloss vor dem einzigen Commit und nach dessen bestätigtem Readback,
  stale ResetEvaluation nach einer Multi-Fault-Mutation, Crash nach Prepared
  vor Attempted, Crash nach Attempted vor dem Port und explizites Rejected
  haben jeweils getrennte, konsistente Eventpublikationen; keine stille
  Trunkierung und kein Teil-Batch;
- `HistoryDiscontinuity` im fixed-size Batch einschließlich
  `lineageState`/`oldLineageKnown`, P1/O2 mit `instanceId=0`;
- alle Nicht-Fault-Crossfields mit event-kind-spezifischen Neutralwerten und
  Goldenbyte-/Decode-Abdeckung je Eventfamilie;
- 33 persistente Slots, neue `0x040E`-/`0x040F`-Domaincounter und
  `Y4_INTERNAL_SAFETY` als Aggregate-Invariante; kein last-origin-wins.

## 20. Umsetzungsslices nach Planfreigabe

Keine Safetyentscheidung wird in die Implementierung verschoben.

1. Fault-Typen, vollständiger Catalog, reine FaultCore und Lifecycle.
2. SafetyState-Wire/Codec/Store, Crossfield-Validator, EmergencyMarker und
   Redundanz-/History-Loss-Recovery.
3. RestartSupervisor, ResetCause, Counter und SAFE_BOOT.
4. Run-/Process-Integration: S3-RAM-Handoff, Y4-Terminalflag,
   Run-Abandon und Fault->Standby/Tombstone; kein Head-Rollback.
5. #20/#21/#22/#23 Producer-Mapping, STALE/FAILED und Fan-/Thermik-Directive.
6. #15 Target-FaultReset, Multi-Fault, Auth-Proof und SafetyRevision.
7. #56/#57 Gate und kanonische Boot-Komposition.
8. No-bypass Command-, Orchestrator-, Planner- und Sink-Pfad.
9. Kanonische Dokumente, vollständige Injection-/Crashmatrix, Resource-/Wear-
   Bericht und unabhängiger Owner-Review.

## 21. Betroffene kanonische Dokumente nach Implementierung

Nur wenn die Umsetzung beginnt, werden die fachlich betroffenen Quellen
aktualisiert: `SAFETY_AND_FAULTS.md`, `SAFETY_COMPONENT_FAULTS.md`,
`SYSTEM_SAFETY_AND_RECOVERY.md`, `ACCEPTANCE_TESTS.md`, `RUN_COMMANDS.md`,
`STATE_MACHINE.md`, `RUN_PERSISTENCE.md`, `RECOVERY_AND_INTERRUPTION.md`,
`ACTUATOR_TIMING.md`, `CONFIGURATION_PERSISTENCE.md`,
`ARCHITECTURE.md` und `ROADMAP.md`. Reviewchronik wird nicht in kanonische
Doku kopiert. Der veraltete Alias `ACTUATOR_TIMING_AND_FANS.md` wird nicht als
eigene Norm dupliziert; er wird nur geändert, falls ein technischer Verweis
nach der Umsetzung angepasst werden muss.

## 22. Plan-Gates und Stop

Vor dem Plancommit werden Plan und aktueller `main` nochmals vollständig
gegen Issue #24, die verpflichtenden Akzeptanztests und die Architekturgrenzen
gelesen. Es gibt kein offenes Safety-/Persistenz-/Recovery-Design-TBD. Das
einzige zulässige `TBD_COMMISSIONING` betrifft reale Schwellen/Parameter; die
fehlende-Wert-Wirkung ist immer bereits fail-closed festgelegt.

Die abschließende Plan-Konsistenzliste ist verbindlich:

- abnormal restart count 1/2 besitzt ein echtes aktuelles 30-Minuten-Fenster;
- Count 3 hat kein RestartLoop-Selbstdeadlock und verwendet QRS;
- SafetyTaskRecovery ist über Reboot at-most-once und technisch von
  S3RunRecovery/#18 getrennt; aktiver Run folgt der vollständigen zweistufigen
  Sequenz;
- Factory-new ist erst nach den vier bestätigten Bootstrap-Writes
  `sf0`, `sf1`, `sem0`, `sem1` redundant healthy; Cut-Points 0/1/2/3 bleiben
  fail-closed und die Tie-Regel lautet `sf0`;
- S3RunRecovery-Provenienz wird beim Candidate-Time-Commit aus dem aktiven
  Preimage, CauseClear, exaktem ServiceResetProof und atomarer Source-Entfernung
  gebildet; Decode-Time behauptet keine verlorene Preimage-Historie und prüft
  nur Payload-, #17-, Fallback- und ResetCause-Qualifikation;
- SafetyWatchdogProducer trennt supervised subpath stall mit durablem
  SafetyTaskRecovery von vollständigem Supervisor-/`app_main`-Stall mit echtem
  TWDT/IWDT-Reset und ohne erfundene Vorpersistenz;
- TWDT ist mit expliziter Panic-/automatischer Reboot-Policy ein echter
  Produktions-Reset-Backstop; IWDT bleibt ein eigener Backstop und kein
  `esp_private`-Header wird verwendet;
- ThermalRecovery hat maximal zwei Versuche innerhalb eines Boots und verliert
  die Fähigkeit bei Reset/Crash/Indeterminate;
- `Y4_INTERNAL_SAFETY` ist eine einzige bewiesene Aggregate-Invariante,
  #17/#56/#57-Counter sind domaingetrennte Identities;
- `ServiceResetProof` bleibt in `fermentation_app`; Plattform- und Test-Support
  kennen nur generische Ports/Evidenz;
- ResetCause kommt ausschließlich über `IResetCauseProvider`, ESP-IDF wird nur
  im Adapter gemappt;
- TWDT/IWDT/`esp_reset_reason()`/`esp_restart()` sind Espressif-first und es
  entstehen keine künstlichen parallelen Tasks;
- HistoryDiscontinuity ist Bestandteil des bounded SafetyEventBatch;
- simultane persistente Ursachen werden als eine begrenzte atomare Mutation
  in Primary-/Follow-up-/Catalog-/Priority-Reihenfolge committed, ohne
  Trunkierung, Eviction, Teil-Durability oder last-origin-wins;
- S3 not provable terminalisiert zuerst unter aktivem Latch; jeder nicht mehr
  handoffberechtigte S3RunRecovery-Intent endet über Tombstone, Intent-Clear
  und separaten SAFE_BOOT-Exit statt Retry oder neuer Y4-Identity;
- gültiger Fault-current lädt den Fallback nicht automatisch: der neue
  #17-interne read-only Helper verwendet `loadReference`, stellt RAM/IDs/
  High-Watermarks korrekt her und ruft danach nur die bestehende #18-Grenze;
- EmergencyMarker prüft sem0-Tie, Envelope/Payload-Revision, vollständige
  State-/Reason-Crossfields und erreicht Cleared/healthy erst nach beiden
  qualifizierten Slots;
- HistoryLoss hat getrennte bestätigte Event-Lifecycle-Schritte; P1/O2
  publizieren nur nach atomarer RAM-FaultCore-Mutation;
- FaultResetEvaluation bleibt #15-Auswertung und wird ausschließlich zusammen
  mit unforgeable ServiceResetProof und aktueller Lineage/Revision entschieden;
- `safetyHistoryEpoch==UINT32_MAX` kann keinen PartialSuccessor wrappen;
  ausschließlich der servicegeschützte TotalDiscontinuity-Pfad bleibt;
- RestartIntent-Crossfields prüfen Rejected/Attempted und aktive
  SafetyTaskRecovery-Source; S3RunRecovery-Source-Provenienz wird ausschließlich
  beim Candidate-Commit bewiesen, nicht beim späteren Decoder;
- SafetyEventBatch ist exakt an einen bestätigten atomaren Commit-/Boot-/Repair-
  Schritt gebunden; Prepared, Attempted, Side-Effect und explizites Rejected
  werden getrennt publiziert, und Nicht-Fault-Crossfields sind je Eventfamilie
  goldenbyte-/decode-definiert;
- Catalog 41, gültige FaultSources 28 plus Reserved, persistente Records 33,
  Payload 824 Byte, Event-Wire 373 Byte, Native-Batch 592 Byte und
  Factory-Bootstrap 4 Writes sowie SafetyCore-Peak 11664 Byte sind neu
  gerechnet und per Assertions zu prüfen; es besteht keine PSRAM-Abhängigkeit;
- S3/Y4, Multi-Fault, Factory/HistoryLoss, SafetyState/Marker, Exhaustion,
  AirFallbackActive, Thermal 0..2, Fanpolicy, SafetyTaskRecovery,
  S3RunRecovery/#18, RestartLoop, ResetCause, #17-Mapping, Run-Abandon,
  Auth/Trust, HistoryDiscontinuity, simultane Ursachen, Ressourcen/Wear,
  ACCEPTANCE_TESTS und ADR-013 sind im vollständigen Rückcheck enthalten;
- Code, Source, Restartwhitelist, Oracle und Tests verwenden
  `S3_PERSISTENCE_PATH_STALL`/`PersistencePath` einheitlich;
- `displayPriority`-Richtung und `IResetController`-Returnsemantik sind
  explizit und fail-closed.

Für diese Planrevision gelten:

```text
git diff --check                         REQUIRED
python3 scripts/check_architecture_boundaries.py  REQUIRED
python3 scripts/check_secrets.py        REQUIRED
Tests/Builds/Hardware                   NOT_RUN (Plan-only)
```

Nach Commit, Push, synchronisiertem Draft-PR-Body und aktuellem SESSION
HANDOVER ist STOP. Keine Implementierung, kein Ready, kein Merge, kein
Issue-Schluss und keine Änderung an PR #107/#108/#109.
