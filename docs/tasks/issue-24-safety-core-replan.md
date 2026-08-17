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

Live-Revalidierung vor dieser neuen Planrevision am 2026-08-17:

```text
PR #110: OPEN / Draft
PR-Branch: agent/issue-24-safety-core-replan-v2
PR-HEAD vor dieser Revision / direkter Parent: 8a3a38a28d725654c6d9954e095ae9468d7e8dc4
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

### Verbindliche Konsistenzkorrekturen dieser Planrevision

Diese Revision bleibt plan-only und ändert keine Produktions-, Test-, Header-,
CPP-, Build- oder Konfigurationsdatei. Sie schließt die folgenden Verträge
gegen `origin/main` @ `b8eae5f4da5f2666b5a9bda333d115254c4db5b2`:

- technischer #17-Abandon und RAM-seitige terminale FSM-Events sind getrennte
  Grenzen; `FaultResetCompleted`, `SafeBootExitCompleted` und der nicht
  freigebende `RunAbandonCompleted` erzeugen keinen zweiten #17-Write;
- `FaultAcknowledged` verwendet eine bounded, RAM-only
  `MessageId -> SafetyFaultMessageReference`-Bindung mit aktueller Lineage;
- `EmergencyMarker` besitzt die Scanstatus `TwoValid`, `OneValid`, `ZeroValid`
  und `ReadIndeterminate`, einschließlich der Factory-/HistoryLoss-
  Sonderverträge und des mechanischen Alternativwrite-Beweises;
- ein bootlokaler `S3RecoveryExclusiveMode` sperrt jeden normalen #17-
  Schreibeinstieg, bevor der Prepared-S3-Handoff klassifiziert ist;
- Mixed-Snapshots trennen P1/O2-RAM-Events von commitabhängigen S3/Y4-Events,
  ohne ein doppeltes `GateChanged` und mit einem neu hergeleiteten 36er-Batch;
- `oldLineageKnown` ist planweit `Continuous=0`, `PartialSuccessor=1` und
  `TotalDiscontinuity=0/1`; Acceptance-Tests trennen SafetyState-Decode von
  der anschließenden #17-Evidence-Klassifikation.
- `SafeBootExitCompleted` folgt erst auf den durablem `safeBootRequired=false`-
  Readback, den bereits durablem #17-`NoActiveRun`/`STANDBY`-Tombstone und
  alle Exitgates; es ist danach ausschließlich ein validierter RAM-FSM-Apply
  ohne zweiten #17-Write.
- `S3RecoveryExclusiveMode` verwendet eine private, einmalige und
  bootlokal gebundene `RecoveryWriteCapability`; öffentliche Coordinator-
  Einstiege, insbesondere der direkte `persistRecoveryCandidate()`-Aufruf,
  bleiben ohne diese intern erzeugte Berechtigung mechanisch blockiert.
- `FaultResetRejected` unterscheidet auflösbare persistente S3/Y4-Targets von
  nicht auflösbaren Targets einschließlich `targetInstanceId=0`; die
  Neutralform erfindet weder P1/O2- noch fehlende FaultIdentity.
- Die `SafetyFaultMessageReference`-Projektion in den 16 `RuntimeMessage`-
  Slots ist im SafetyCore-RAM-/Peak-Budget sichtbar; der #17-Wire bleibt
  unverändert.

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
| #17 RecoveryEvidence | aktueller `main`-Vertrag: `RunCheckpointReference`, bestehende Codec-/Load-/Coordinator-Verträge, Schema 3 und bestehender Fallback-/Recoverypfad | geplante schmale #17-Erweiterung: `RunPersistenceRecoveryEvidence`, gemeinsame Referenz-/Formathelfer, `captureSafetyRecoveryEvidence()`, `classifyRecoveryEvidence()` und read-only `prepareSafetyFallbackRecovery()`; keine Safety-/Intent-Semantik und kein Schema 4 |
| #17 Activation | `run_persistence_coordinator.hpp:214-218`, `run_persistence_coordinator.cpp:760-925` | neuer Safety-Handoff darf nur vor der bestehenden forward-only Recoveryaktivierung RAM-seitig auswählen |
| #17 Write | `run_persistence_coordinator.cpp:1404-1685` | Prepared -> Slot -> Committed bleibt vorwärts und crash-sicher |
| #18 Recovery | `docs/RUN_PERSISTENCE.md`, `docs/RECOVERY_AND_INTERRUPTION.md`, `RunRecoveryCoordinator` | Zeit, Phase, Sensorwahl, Progress und Terminalentscheidung bleiben #18 |
| Prozess | `process_state_machine.hpp:15-150`, `process_state_machine.cpp:230-285` | echter FSM-Vertrag wird um `FaultResetCompleted`, `SafeBootExitCompleted` und nicht freigebendes `RunAbandonCompleted` ergänzt; kein `NoActiveRun + Fault` |
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

Der direkte Main-Abgleich zeigt den noch caller-supplied #15-Vertrag:
`FaultResetRequest` enthält derzeit `CommandEnvelope` und eine vollständige
`FaultResetEvaluation`; diese Form ist für #24 nicht ausreichend. Die
Planrevision korrigiert den Vertrag gegen den echten Code und trennt öffentliche
Anforderung, interne Auswertung und endgültige Safety-Mutation:

```text
öffentlicher FaultResetRequest:
  CommandEnvelope envelope
  uint32_t targetInstanceId

interne TargetResetEvaluation in #24:
  targetIdentity = FaultCode + FaultSource
  targetResetAllowed
  causeCleared
  safetyChecksPassed
  otherBlockingFaultActive
  releaseAllowed
  expectedPersistentFaultRevision
  current safetyHistoryLineage
  rejection

interner, nicht fälschbarer ServiceResetProof:
  Auth-Session/Generation, Target-Instance, aktuelle Lineage,
  erwartete persistentFaultRevision und serviceseitig qualifizierte Evidenz
```

`authorizationSatisfied` bleibt kein caller-supplied Freigabefeld. Die
öffentliche Bewertung wird entfernt; der CommandCoordinator beziehungsweise
`SafetyFaultService` bildet die interne `TargetResetEvaluation` aus der
kanonischen SafetyState-/Producer-Evidenz und akzeptiert den Proof nur für
exakt dasselbe Target. Typ und Minting von `ServiceResetProof` gehören
ausschließlich `fermentation_app`; `device_platform_test_support` kennt weder
den Typ noch Fault-/Lineagefelder. Bis ein echter Service-PIN/Auth-Producer
existiert, erzeugt der Produktpfad keinen S3-/Y4-Resetproof. App-Tests erzeugen
ihn nur über einen app-internen Testhelper aus generischer Test-Auth-/
Sessionevidenz.

Andere aktive Faults machen `otherBlockingFaultActive=true` und
`releaseAllowed=false`, dürfen aber einen aktuell bewiesenen Target-Reset
nicht verhindern. Erst wenn der letzte Blocker entfernt ist, darf Release,
Standby oder SAFE_BOOT-Exit freigegeben werden. Die einzige Safetyrevision ist
`SafetyState.persistentFaultRevision`. `RunCommandState::faultRevision` wird
aus der Safety-Autorität entfernt; falls ein kurzlebiger #15-Kompatibilitäts-
oder Nachrichtenwert nötig bleibt, ist er eine ausschließlich abgeleitete
Projektion und darf weder unabhängig erhöht, persistiert noch als Safetywahrheit
geprüft werden.

## 3. FaultCatalog und Identität

### 3.1 Wire- und Identitätsregeln

`FaultCode` ist ein stabiler `uint16` und `FaultSource` ein stabiler `uint8`.
Die Kombination ist die einzige fachliche Identity. Measurement-, Run-,
Control-, Planner- und Zeitrevisionen gehören ausschließlich in Diagnostik.
P1/O2 sind transient und bootlokal; S3/Y4 sind persistent und rebootstabil.
P1/O2 verbrauchen weder persistente InstanceIds noch
`persistentFaultRevision`.

Für P1/O2 ist die Eventsemantik geschlossen: `inactive -> active` publiziert
`FaultRaised`, `active -> inactive` publiziert `FaultCleared`, jede spätere
neue Aktivierung wieder `FaultRaised`. `FaultRelapsed` ist ausschließlich
S3/Y4 vorbehalten, wenn dieselbe gelatchte Instance nach `causeCleared=true`
vor TargetReset erneut aktiv wird. P1/O2 haben niemals persistente InstanceId
oder FaultRevision.

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
ist `33 Byte Header + 892 Byte Payload + 4 Byte CRC = 929 Byte`, also unter
dem 1024-Byte-Limit.

### 4.2 Payload-Feldreihenfolge

Alle Felder sind Big-Endian ohne C++-Padding. Der Payload ist immer exakt
`kSafetyStatePayloadBytes = 892` Byte:

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
| 31 | `historyPreviousLineageKnown` | `uint8 = 0/1`, rebootstabiler Nachweis `oldLineageKnown` |
| 32 | `FaultRecord[0..32]` | exakt 33 Records à 24 Byte |
| 824 | `RunPersistenceRecoveryEvidence` | feste #17-Projektion, 68 Wirebytes; bei `None`/`SafetyTaskRecovery` vollständig zero |

`historyPreviousLineageKnown` ersetzt das bisherige `reserved0` und ist der
rebootstabile SafetyState-Anker für `oldLineageKnown`: `Continuous=0`,
`PartialSuccessor=1`, `TotalDiscontinuity=0 oder 1`. Der Decoder lehnt jede
andere Kombination ab; der Wert wird bei jedem History-/Lineage-Ereignis in
den typisierten #19-Handoff projiziert.

`recordRevision` ist ausschließlich Envelope `VersionValue`; er wird nicht
doppelt im Payload gespeichert. Der Payload ist deshalb exakt
`32 + (33 * 24) + 68 = 892 Byte`. Leere FaultRecord-Slots sind vollständig
null. Die Evidence-Projektion ist kein zweiter Safetyvertrag: ihre 68 Bytes
werden ausschließlich über den von #17 bereitgestellten typisierten Codec-
Helper geschrieben/gelesen.
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
  `restartAttempted=0`, `outcome=None` und eine vollständig zero gesetzte
  RecoveryEvidence-Projektion;
- `restartIntentState=Prepared` erzwingt `kind=SafetyTaskRecovery` oder
  `kind=S3RunRecovery`,
  `0 < source < nextPersistentInstanceId`, `restartAttempted` 0/1 und
  `outcome` None oder Rejected;
- bei `kind=SafetyTaskRecovery` ist die RecoveryEvidence-Projektion
  vollständig zero;
- bei `kind=S3RunRecovery` ist die Evidence-Projektion vollständig vorhanden
  und wird über den gemeinsamen #17-Typ-/Codec-Helper validiert; der reine
  SafetyState-Decoder prüft nur Wire, Format, Referenzform und Lineagefelder,
  nicht den aktuell geladenen #17-Head oder dessen Fault-Zustand;
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

Die `S3RunRecovery`-Candidate-Erzeugung beweist die Safety-seitige
Korrelation weiterhin mechanisch: Der Service-Target-Reset nimmt die aktuelle
SafetyState-Revision als Preimage, prüft aktive Source-Instance, CauseClear und
den exakten `ServiceResetProof`, entfernt genau diese Instance und setzt im
selben Kandidaten `Prepared(kind=S3RunRecovery, sourceInstanceId=source)`.
Dabei wird die von #17 unter derselben Orchestrierungs-/Mutationssperre
erfasste `RunPersistenceRecoveryEvidence` eingebettet. Kein Caller und kein
Decoder darf eine historische Source oder rohe #17-Wirebytes nachträglich
einsetzen. Der atomare SafetyState-Commit ist die gemeinsame persistente
Entscheidung; die mechanische Bedeutung der Evidence bleibt #17.

### 4.4a Typisierter #17-Recovery-Evidence-Vertrag

Der Typ gehört zur #17-RunPersistence-Domain in `fermentation_app` und bleibt
fixed-size, stringfrei und Safety-/Auth-/Intent-neutral:

```cpp
struct RunPersistenceRecoveryEvidence {
    std::uint64_t headRevision;
    RunCheckpointReference current;
    RunCheckpointReference fallback;
};
```

#17 persistiert dabei keine Safety-Episode, keinen Fault, keine S3-Instance und
keinen `S3RunRecovery`-Intent. Die Evidence beschreibt ausschließlich den
mechanisch bewiesenen RunPersistence-Anker, der von #24 in seiner eigenen
SafetyState-Instanz korreliert wird.

Die feste Projektion ist:

```text
headRevision                 uint64       8 Byte
current RunCheckpointReference            30 Byte
fallback RunCheckpointReference           30 Byte
Wire gesamt                               68 Byte
```

Die native Form ist nicht packed und erhält `static_assert(sizeof(...) ==
88)` sowie `static_assert(alignof(...) == alignof(std::uint64_t))`. Die
Referenz- und Evidence-Encoder/-Decoder/-Validatoren sind genau einmal in der
#17-RunPersistence-Schicht definiert und werden vom SafetyState-Codec nur als
gemeinsamer Helper aufgerufen. #24 kopiert weder
`runCheckpointReferenceMatches()` noch Envelope-/CRC-/Schema-/Epochregeln.
Der Helper unterscheidet ausschließlich die technisch erlaubte Projektion
`zero` (alle 68 Bytes null) von einer vollständig gültigen Evidence-Instanz;
`zero` ist nur bei Intent `None` oder `SafetyTaskRecovery` zulässig.

`captureSafetyRecoveryEvidence(...)` liefert nur dann einen Wert, wenn #17
unter der aktuellen Mutationssperre mechanisch nachweist:

- Committed Head, gültige `current`-Referenz mit aktivem `Fault`-Snapshot und
  gültige ältere `fallback`-Referenz;
- beide Referenzen physisch lesbar, Envelope/CRC/Schema/Epoch gültig und
  jeweils durch `runCheckpointReferenceMatches()` bestätigt;
- gleicher Run-/Program-/Manual-Kontext, gleicher aktiver Run, gültige
  `ProcessState::Fault`-Form und `fallback != current`;
- ältere Checkpoint-/Runrevision des Fallbacks, zwei unterschiedliche Slots,
  keine `NoActiveRun`-Variante und keine widersprüchlichen High-Watermarks;
- `headRevision`, Referenzen, Variant und alle Crossfields des Typs gültig.

Die API entscheidet weder S3, Auth, CauseClear noch Recovery. Sie darf nur
technische RunPersistence-Wahrheit beschreiben. `mutationKind` ist dafür kein
Beweis: Es steht ausschließlich im Prepared-Head und ist nach einem
Committed-Readback nicht persistiert.

`classifyRecoveryEvidence(expected)` bleibt ebenfalls #17. Nach
`loadAndInitialize()` validiert sie eigene physische Records und liefert exakt:

```text
PendingHandoff
RecoveryForwardAlreadyCommitted
RecoveryRejectedAlreadyCommitted
TerminalAlreadyCommitted
InvalidOrOrphaned
```

`PendingHandoff` verlangt exakte Übereinstimmung von committed Headrevision,
Current-/Fallback-Referenz und physisch validiertem aktivem Fault-current mit
`expected`. Die vier übrigen Klassen verlangen eine strikt vorwärts liegende,
physisch gültige Schema-3-Wahrheit nach `expected.headRevision` und die jeweils
kanonische #18-Snapshotform: gültige RecoveryEvaluation/Recovery-Snapshotform,
gültige Fault-/RecoveryRejected-Form ohne offenen Recoveryanker, oder exakt
`NoActiveRun`/`Standby`. Run-ID, Programmsnapshot, Manual-Kontext, Snapshot-
Crossfields, Fallbackkette und technische Referenz-/CRC-/Epochregeln werden
innerhalb #17 geprüft. Eine dritte, unvollständige oder nicht mehr an die
Evidence bindbare Wahrheit ist `InvalidOrOrphaned`. Kein Status wird aus
`ProcessState` allein geraten; der Committed-Head enthält keine
`mutationKind`-Provenienz.

`prepareSafetyFallbackRecovery(expected)` konsumiert die von #17 selbst
validierte Evidence, prüft Current/Fallback erneut, lädt den Fallback read-only
und setzt `FallbackRecoveryPending`. Es schreibt keinen Head, keinen Slot und
keine Fallbackpromotion. Normale #17-Mutationen bleiben ab bestätigtem
Prepared-Intent gesperrt; ausschließlich diese Klassifikation, der read-only
Handoff, explizite #18-Recoverymutationen und der definierte terminale Abandon
sind zulässig.

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
`UINT32_MAX` eine exakte Referenz auf den SafetyState-Kandidaten. Die
Markerrevision selbst ist `uint32`: `markerRevision==UINT32_MAX` ist
Exhaustion/fail-closed und kann keinen Wert oberhalb darstellen. Goldenbyte-
Tests über `UINT32_MAX` hinaus gelten ausschließlich für die uint64-Felder
`SafetyState recordRevision` und `attemptedSafetyRecordRevision`.

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
Der Scan klassifiziert die beiden Slotlesungen vor jeder normalen Mutation
explizit:

```text
TwoValid:
  beide Slots sind semantisch gültig; höchste markerRevision gewinnt;
  gleiche Revision plus identische Bytes -> sem0;
  gleiche Revision plus unterschiedliche gültige Bytes -> MarkerIndeterminate.

OneValid:
  genau ein Slot ist semantisch gültig; der andere ist mechanisch NotFound oder
  korrupt/semantisch ungültig; der gültige Slot ist der einzige Winner und der
  Peer-Repair-/Mutationvertrag bleibt bounded.

ZeroValid:
  beide Slots wurden deterministisch gelesen, aber kein Slot ist gültig
  (einschließlich corrupt+corrupt, corrupt+NotFound, beide semantisch
  ungültig oder beide NotFound). Es gibt keinen Winner und keine
  max(valid)-Revision. Weiter ist nur bei vollständig bewiesenem Factory-new-
  Bootstrap oder vollständig qualifiziertem HistoryLoss-/TotalDiscontinuity-
  Reinit-Vertrag zulässig; sonst MarkerIndeterminate, ImmediateStop, SAFE_BOOT,
  kein normaler Active-/Cleared-Write und kein Blindrepair.

ReadIndeterminate:
  mindestens eine Lesung ist ReadError oder CapacityError, oder Decode-/Scan-
  Evidenz lässt keinen verlässlichen Slotzustand zu. Es gibt keinen Winner,
  keinen Blindwrite und keinen zweiten Versuch.
```

`NotFound` ist damit nur eine determinierte Scanantwort. Die Sonderverträge
prüfen zusätzlich ihre vollständigen Preconditions: Factory-new verlangt die
vier vollständig qualifizierten `NotFound`-Keys; HistoryLoss verlangt einen
technisch les-/schreibbaren und bounded Storepfad, darf aber bei verlorener
Markerwahrheit `ZeroValid` vorfinden. Ein normaler Marker-Write läuft danach
exakt so:

```text
1. sem0/sem1 lesen und Scanstatus bestimmen; nur TwoValid/OneValid dürfen
   einen normalen Winner-/Zielslot bestimmen.
2. newMarkerRevision = max(valid) + 1 checked; UINT32_MAX -> Exhaustion.
3. Kandidat bilden, bisherige Zielbytes exakt merken und genau einen bevorzugten
   Writeversuch ausführen.
4. Success -> exaktes Readback. CommitOutcomeUnknown -> exaktes Readback:
   Kandidat = committed, alte Zielbytes = mechanisch NOT COMMITTED, jede dritte
   Wahrheit/ReadError/CapacityError = ReadIndeterminate.
5. Ein Alternativwrite auf dem anderen Slot ist nur zulässig, wenn der erste
   Versuch mechanisch als NOT COMMITTED bewiesen ist: `WriteError`/
   `NotWritten` oder `CommitOutcomeUnknown` plus exaktes Readback der alten
   Zielbytes. Nach ReadError, Capacity-/Decode-Indeterminate, ZeroValid oder
   MarkerIndeterminate gibt es keinen zweiten Write.
```

Der Bootscan beider Marker gewinnt die höchste semantisch gültige Revision.
Active hält fail-closed. Ein alter Active-Marker ist erst redundant repariert,
wenn ein neuer Cleared-Marker exact-readback-bestätigt ist, der Peer mit exakt
denselben Bytes auf dieselbe Revision repariert und beide Slots danach
qualifiziert sind. Ein einzelner Cleared-Slot neben einem unqualifizierten
Active-Peer meldet daher nie `healthy/Cleared`. Ein defekter Peer wird
repariert, aber ein Y4 bleibt bis CauseClear plus Service-Reset bestehen.
Marker-Recovery repariert Persistenz, ist weder FaultReset noch SAFE_BOOT-Exit.
Tests decken equal tie, divergente equal revision, NotFound, corrupt,
CommitOutcomeUnknown, Alternativwrite mit gleichen Bytes/Revision,
Cleared-Repair-Crash und `UINT32_MAX` ab.

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
 -> Storepfad technisch les-/schreibbar, bounded und für Readback qualifiziert;
    bestehende Markerwahrheit darf dabei `ZeroValid` sein
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
`SafetyEventBatch` mit dem faktischen `oldLineageKnown` und
`lineageState=TotalDiscontinuity`; bei vollständigem Verlust ist dies `0`, bei
bekannter alter Lineage (einschließlich `safetyHistoryEpoch==UINT32_MAX`) ist
es `1`. Es gibt keinen separaten
`SafetyHistoryDiscontinuity`-Handoff und keine globale Reihenfolge. Ein zweiter
Totalverlust erzeugt erneut dieses Event und macht alle vorherigen flüchtigen
Proofs ungültig.
Ein unqualifizierter unbekannter Zustand bleibt fail-closed. Der oben
vollständig qualifizierte TotalLoss-Recoveryvertrag (Serviceauth, Run terminal,
Configuration/Store gesund) darf dagegen eine geschützte
TotalDiscontinuity-Reinitialisierung durchführen. Counter-Exhaustion bleibt
separat fail-closed und wird niemals über HistoryLoss-Reinit zurückgesetzt.

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

Ein Producer-Snapshot wird zuerst vollständig ausgewertet, auch wenn er
gleichzeitig P1/O2-RAM-Mutationen und neue persistente S3/Y4-Identities
enthält. Die Grenzen sind dann exakt:

```text
1. alle Ursachen des Snapshots aggregieren und den neuen RAM-Gatezustand
   sofort fail-safe wirksam machen;
2. die vollständige P1/O2-FaultCore-RAM-Mutation atomar abschließen;
3. P1/O2-FaultRaised/FaultCleared und deren RAM-Mutationsbatch publizieren;
4. die neue persistente S3/Y4-Menge als EINEN SafetyState-Kandidaten
   committen und exact-readback-bestätigen;
5. S3/Y4-Fault-/Latch-/Terminal-Events erst nach diesem Commit publizieren.
```

P1/O2-Events dürfen bei einem Crash zwischen RAM-Mutation und SafetyState-
Commit bereits existieren; `ImmediateStop` bleibt wirksam. S3/Y4-Events und
ein persistenter Erfolgsclaim dürfen dann noch nicht existieren. `GateChanged`
wird für dieselbe tatsächliche aggregierte Gateänderung genau einmal beim
ersten atomaren Schritt publiziert, der sie als Event wirksam macht: bei einer
gemischten Mutation im P1/O2-RAM-Batch, bei einer rein persistenten Mutation im
bestätigten SafetyState-Batch. Der persistente Batch wiederholt ein bereits
publiziertes `GateChanged` niemals.

CauseClear ist nur zulässig, wenn die vollständige kanonische Producer-Evidenz
des konkreten Catalog-Eintrags gesund ist. Es setzt `causeCleared=true` und
erhöht `persistentFaultRevision`; der Latch bleibt bestehen.

Relapse vor Reset setzt `causeCleared=false`, erhöht die Revision und macht jede
ältere ResetEvaluation stale. Für einen `SafetyTaskRecovery`-Record mit bereits
`restartAttempted=1` bleibt der Versuch dabei verbraucht: gleiche Instance,
kein zweiter automatischer Restart und kein neuer Intentversuch. Erst ein
erfolgreicher TargetReset beendet diese Episode; eine später neu auftretende
Ursache erhält eine neue Instance und wieder höchstens einen
SafetyTaskRecovery-Versuch. Ein Reset entfernt erst nach aktueller
Target-Evaluation den Record, erhöht die Revision und nutzt den SafetyState-
Commit als Linearisierung. RAM-/Process-/Run-Handoff folgt erst danach.

Quittierung ist ausschließlich UI-/Message-Zustand: kein SafetyState-Feld, kein
CauseClear, kein Gatewechsel, keine Revisionserhöhung und keine persistenten
Ack-Writes. Nach Reboot darf die Meldung wieder unquittiert erscheinen.

### 6.1a FaultAcknowledged: exakte MessageId-Bindung

Die SafetyFault-Projektion erzeugt für jede `MessageCode::SafetyFault`-Meldung
eine bounded RAM-only-Referenz. Der Besitz ist eindeutig:

```cpp
struct SafetyFaultMessageReference {
    FaultCode faultCode;
    FaultSource faultSource;
    std::uint32_t instanceId;          // P1/O2 = 0
    std::uint32_t safetyHistoryEpoch;
};
```

`SafetyFaultService` beziehungsweise der von ihm kontrollierte Safety-Message-
Projektor erzeugt die Meldung und setzt exakt diese optionale fixed-size
`RuntimeMessage::safetyFaultReference`; sie darf nur bei
`MessageCode::SafetyFault` vorhanden sein. Das vorhandene optionale
`RuntimeMessage::faultRevision` ist dafür keine ausreichende Identitybindung
und wird weder geraten noch als Ersatz verwendet; falls es als #15-
Kompatibilitätsprojektion bleibt, ist es rein abgeleitet. Die Referenz ist nicht
Teil des SafetyState, nicht persistent und kein Ack-Wearpfad.

Der Ack-Pfad arbeitet atomar in der RAM-Kopie:

```text
MessageCommandRequest.messageId
 -> exakte RuntimeMessage suchen
 -> MessageCode::SafetyFault und safetyFaultReference vorhanden
 -> safetyHistoryEpoch == aktuelle Lineage und Referenz bindet exakt den
    aktuellen, noch nicht gelösten FaultCore-Eintrag
 -> nur diese Message und diese FaultIdentity quittieren
 -> FaultAcknowledged mit exakt derselben Reference publizieren
```

Für eine Nicht-SafetyFault-Message, unbekannte `messageId`, fehlende Referenz,
stale Lineage, nicht mehr aktiven/gelösten Fault oder inkonsistente Message-
/Fault-Revision gibt es keinen erfundenen `FaultAcknowledged`. Eine
Discontinuity invalidiert alle alten SafetyFaultMessageReferences. Die
sichtbare Priorität, der erste aktive Fault und eine passende Revision dürfen
niemals als implizite MessageId->Fault-Auflösung dienen.

### 6.2 Multi-Fault-Reset

Der tatsächliche #15-Vertrag wird für die #24-Integration explizit korrigiert:

```text
FaultResetRequest = CommandEnvelope + targetInstanceId

interne TargetResetEvaluation =
  targetIdentity, targetResetAllowed, causeCleared,
  safetyChecksPassed, otherBlockingFaultActive, releaseAllowed,
  expectedPersistentFaultRevision, safetyHistoryLineage, rejection
```

Ein Target darf intern erst entfernt werden, wenn diese Evaluation aktuell ist,
CauseClear und die Target-Safetychecks vorliegen und der CommandCoordinator
zusätzlich einen unforgeable `ServiceResetProof` für dieselbe Target-Instance,
Lineage und `persistentFaultRevision` prüft. Andere Blocker verhindern Release,
Recovery und Standby, aber nicht den so qualifizierten Target-Reset. Erst nach
dem letzten Blocker und erfüllter Y4-Terminalregel kann ein Gate auf `Allowed`
oder ein SAFE_BOOT-Exit entstehen. Das vorhandene öffentliche
`FaultResetEvaluation::authorizationSatisfied` wird entfernt; eine
Kompatibilitätsprojektion darf nie die Entscheidung beeinflussen.

Der Produktpfad akzeptiert kein öffentlich konstruierbares
`authorizationSatisfied=true`, keine vom Transport gelieferte Evaluation und
keinen fremd erzeugten Proof. `persistentFaultRevision` ist die einzige
Safetyrevision; `RunCommandState::faultRevision` wird nicht als zweite
Autorität fortgeführt. Bis ein produktiver Service-PIN/Auth-Producer existiert,
sind S3/Y4-Resets produktiv fail-closed; Tests erhalten ausschließlich einen
app-internen Testhelper, nicht `device_platform_test_support`.

Die Reset-Events verwenden dieselbe geschlossene Target-Auflösung:

```text
erfolgreicher FaultReset:
  ausschließlich ein erfolgreicher persistenter S3/Y4-TargetReset;
  konkrete Target-Instance sowie exakte FaultCode/FaultSource-Identity

FaultResetRejected:
  auflösbare aktive persistente S3/Y4-Instance -> konkrete Target-Instance,
  exakte Target-Identity und Rejection-Grund
  nicht auflösbare/entfernte Instance oder TargetNotActive ->
  faultCode=0, faultSource=0, instanceId=requested targetInstanceId,
  detail0=ActorSource, detail1=TargetNotActive
```

`targetInstanceId=0` ist dabei ausdrücklich nicht die Identity mehrerer
aktiver P1/O2-Faults. P1/O2 werden über ihren transienten Producervertrag
gerearmt und nie über diesen Target-Reset-Service targetiert. Ein unbekannter
oder bereits entfernter persistenter Target darf in `FaultResetRejected`
keine FaultIdentity erhalten; die Neutralform ist nur für diesen konkreten
`TargetNotActive`-/Nichtauflösbarkeitsfall zulässig.

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
5  bei Prepared `S3RunRecovery` bootlokal `S3RecoveryExclusiveMode` setzen;
   normalen Application-/RunPersistence-Writebetrieb noch nicht starten
6  #57 ConfigurationRecoveryService::boot()
7  #56 ConfigurationService Runtime-/Commitzustand konsumieren
8  #17 RunPersistence::loadAndInitialize() ausschließlich read-only laden
9  #20/#21 Sensor- und Sicherheits-Evidenz laden
10 Producer -> SafetyFaultService Catalog-Mapping
11 runRecoveryForbidden und beide Restartzwecke qualifizieren
12 SAFE_BOOT-Entscheidung und bestehenden #17-Fallbackstatus trennen
13 bestehendes `FallbackRecovered` ohne S3Intent -> bestehendes #18
14 `Current/Fault` plus gültiger `S3RunRecovery`-Intent -> read-only
   Safety-Fallback-Handoff -> #18
15 erst nach Klassifikation und dem ausdrücklich erlaubten Pfad normaler
   Orchestrator/Planner/Sink-Tick
```

Kein periodischer Checkpoint, Command, Adjust, Completion-, Sensorselektions-
oder sonstiger normaler #17-Write darf zwischen Schritt 5 und der
Evidence-Klassifikation dazwischentreten. Kein Planner-/Sink-Tick darf vor
Schritt 15 einen `Allowed`-Pfad erhalten.

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

Dann werden die Bedingungen neu geprüft, `safeBootRequired=false` committed
und exact-readback-bestätigt. Zusätzlich muss der kanonische #17-Zustand
`NoActiveRun`/`STANDBY` bereits durable und exact-readback-bestätigt sein;
der normale Exit darf keinen zweiten #17-Transition-Write erfinden. Sind alle
Exitgates weiterhin erfüllt, wird ausschließlich über den neuen
`ProcessEvent::SafeBootExitCompleted` mit gleichnamigem
`TransitionReason::SafeBootExitCompleted` ein RAM-FSM-Kandidat
`SafeBoot -> Standby` gebildet. Erst `valid...Topology()` und
`decide...Event()` validieren diesen Kandidaten RAM-seitig; danach wird der
RAM-FSM angewendet. Es gibt keinen weiteren #17-Snapshot und keinen zweiten
write-before-apply-Persistenzpfad. `STATE_MACHINE.md` und die FSM-Tests gehören
zum Umsetzungsslice. Es gibt keine direkte `processState.state = Standby`-
Zuweisung.
`SafeBootExitCompleted` ist kein FaultReset und auch nicht der aktive
S3-Recovery-Finalisierungsschritt aus Abschnitt 8.8. Ein Crash nach dem
Flag-Commit oder nach den Domain-Commits, aber vor dem RAM-FSM-Apply, wird beim
nächsten Boot erneut qualifiziert. Aus dem bereits durablem SafetyState und
dem #17-`NoActiveRun`/`STANDBY`-Readback wird derselbe RAM-FSM-Event erneut
gebildet und angewendet, ohne einen neuen #17-Write. Der normale Exit verlangt
weiterhin `NoActiveRun/STANDBY`, keinen Latch, keinen Intent und kein
Terminalflag.

### 8.7 SAFE_BOOT -> S3RecoveryDeparture

Ein S3 kann vor seinem Service-Reset rebooten. Dann sind S3-Latch,
`safeBootRequired=true`, #17 `Current=Fault` und der unveränderte Pre-Fault-
Fallback gemeinsam persistiert. Dieser Zustand darf nicht durch den normalen
SAFE_BOOT-Exit in `NoActiveRun` terminalisiert werden.

Die exakte `S3RecoveryDeparture`-Reihenfolge ist:

```text
S3 aktiv
 -> CauseClear
 -> ServiceResetProof und Target-Identity prüfen
 -> aktive Run-Domain vorhanden
 -> #17 captureSafetyRecoveryEvidence() unter derselben
    Orchestrierungs-/Mutationssperre
 -> #24 prüft kein Y4, runRecoveryForbidden=false, aktuelle
    Lineage/Target/Revision/Auth sowie Config-/Sensor-/Watchdog-Gates
 -> ein SafetyState-Kandidat:
    S3-Target entfernen, S3RunRecovery vorbereiten,
    sourceInstanceId setzen, Attempted=0 und Evidence einbetten
 -> SafetyState-Commit und Exact-Readback
 -> Attempted=1 in einem zweiten SafetyState-Kandidaten committen/readbacken
 -> genau ein kontrollierter Restart
```

Vor dem ersten SafetyState-Commit muss #17 die Evidence nochmals aus seinem
eigenen Committed-Head/Current/Fallback bestätigen. Die konkrete persistierte
Evidence-Instanz gehört zu #24 `SafetyState`; #17 besitzt Typ, Regeln und
mechanische Erzeugung. RAM bleibt bis zum Ende `SafeBoot/ImmediateStop`; kein
Planner- oder Sink-Tick wird freigegeben. Nach bestätigtem Prepared-Intent
sind normale #17-Mutationen gesperrt.

Ist die #17-Evidence oder ein Safety-/Config-/Reset-Gate vor dem Target-Reset
nicht beweisbar, wird kein Intent gebildet und kein Target entfernt. Der alte
Run wird unter aktivem S3-Latch über den technischen #17-Abandon zu
`NoActiveRun/STANDBY` forward-only terminalisiert und exact-readback-bestätigt;
RAM bleibt Fault/SafeBoot. Erst danach sind CauseClear und TargetReset ohne
RecoveryIntent zulässig. Es entsteht keine zusätzliche Y4-Identity nur für
diesen Terminalpfad.

Nach bestätigtem Candidate-Commit wird `restartAttempted=true` in einem zweiten
SafetyState-Kandidaten geschrieben. Erst danach darf der `IResetController`
einmal aufgerufen werden. Bei ausdrücklicher Port-Ablehnung wird `Rejected`
geschrieben, ohne Retry; eine unerwartete Rückkehr nach `Requested` ist kein
Rejected. Beim nächsten Boot ist nur `S3RunRecovery + Attempted=1` gemäß
ResetCause-Matrix handoffberechtigt. Attempted=0, jeder neue S3/Y4,
Marker-/Safety-Indeterminate, Configuration-/Run-Fehler oder Counterstatus
führt zu SAFE_BOOT ohne Handoff.

Jeder `S3RunRecovery`-Intent, der gemäß ResetCause-Matrix oder der von #17
gelieferten Evidence-Klassifikation nicht mehr handoffberechtigt ist, wird
niemals erneut gestartet. Stattdessen folgt zwingend:
ImmediateStop/SAFE_BOOT, `safeBootRequired=true` durable sofern schreibbar,
kein Restart-Retry und kein Resume; danach technischer #17-Tombstone zu
`NoActiveRun/STANDBY`, Intent/Evidence-Clear erst nach Exact-Readback, RAM
weiter Fault/SafeBoot und ausschließlich am Ende ein normaler
`SafeBootExitCompleted`-Pfad. Das umfasst Crash nach Prepared vor Attempted,
`Attempted=0` mit jedem Reboot, explizites RestartRejected, Attempted=1 mit
Brownout/Watchdog/Panic/Unknown, fehlende Fallback-Qualifikation und jede
andere unzulässige Evidence-Klasse. Bei bereits durablem
`NoActiveRun/STANDBY` erkennt der Boot nach Readback den Terminalpunkt und
löscht Intent/Evidence reparativ; kein Resume und kein Restart. Ein Crash vor
dem Departure-Commit lässt S3-Latch und `safeBootRequired=true` bestehen. Ein
neuer Safetyfault während Departure bricht den Handoff ab und lässt die neue
persistente Wahrheit dominieren.

### 8.8 S3RunRecovery: Schema-3-Klassifikation und Safety-Finalisierung

Ein Prepared `S3RunRecovery` stellt vor jeder normalen Application-Aktivierung
den bootlokalen Guard `S3RecoveryExclusiveMode` her. Der Guard ist keine
persistent gespeicherte Wahrheit und keine zweite Recovery-FSM; die persistente
Wahrheit bleibt der SafetyState-Intent. Er sitzt in der Safety-/Boot-
Orchestration und sperrt den gemeinsamen #17-Coordinator vor dessen
Schreibeinstiegen. Direkt zu blockieren sind mindestens:

```text
persistCommand()              // Start/Stop/Adjust/Completion/Ack-Commands
persistTransition()           // normale Prozessübergänge
checkpointPeriodic()          // periodische Checkpoints
persistSensorSelection()      // automatische Sensorselektionswrites
persistRecoveryCandidate()    // Zeit-/Correction-Writes außerhalb #18
writeSnapshot()/writeSnapshotCore() als letzte Coordinator-Schranke
```

Damit bleiben auch normale automatische Prozess- und Sensorwrites sowie
Recovery-Time-/Correction-Writes bis zu einer expliziten, intern autorisierten
#18-Freigabe gesperrt. Die mechanisch geschlossene Lösung ist eine
Coordinator-owned scoped permission:

```text
RecoveryWriteCapability:
  private/nested fixed-size token von S3RecoveryExclusiveMode bzw. dem
  RunPersistenceCoordinator; privater Konstruktor, nicht kopierbar und nicht
  verschiebbar; an aktuelle Bootinstanz, ExclusiveMode-Generation und genau
  den Zweck InitialS3RecoveryForward gebunden

öffentliche Coordinator-Einstiege:
  kein Capability-Parameter und keine öffentliche Mint-/Enter-/Exit-API;
  bei aktivem ExclusiveMode vor Candidate-/Slotbildung -> RecoveryPending /
  NotAllowedInState, ohne Head-/Slot-Write

interne #18-Brücke:
  ausschließlich der private, exakt benannte #18-Recoverypfad darf das Token
  vom Coordinator erhalten und an den privaten autorisierten
  writeSnapshotCore()-Overload weiterreichen; dieser Overload akzeptiert nur
  das unverbrauchte, owner- und bootgebundene Token

Lebensdauer:
  das Token ist single-use und verfällt bei jeder nicht exakt bestätigten
  Mutation. Nach dem ersten eindeutig durablem #18-Forward-Commit beendet die
  Safety-Finalisierung sofort Intent/Evidence, schließt die Generation und
  beendet ExclusiveMode; erst danach sind normale #17-Writes wieder möglich.
  Ein fehlender Readback, WriteError, CommitOutcomeUnknown oder neuer Safety-
  Blocker verbraucht die Berechtigung ohne Retry und hält ExclusiveMode.
```

Die öffentliche `persistRecoveryCandidate()`-Methode ist damit während
ExclusiveMode immer blockiert. Das gilt auch für einen direkten Aufruf sowie
für `RunRecoveryCoordinator::reevaluateRecoveryTime()` und
`applyRecoveryProgressWeighting()`: beide besitzen kein Token und können die
interne #18-Brücke nicht imitieren. Die bereits benannten #18-Pfade
(`activateLoadedRun`, `activateFallbackRecoveredRun`,
`resolveRecoveryOutcome`, `reevaluateRecoveryEvaluation` und
`persistRecoveryCandidate` ausschließlich innerhalb dieses privaten
Recoverypfads) erhalten das minimale einmalige Fenster; nicht benannte oder
öffentliche Varianten erhalten es nicht. Der technische terminale Abandon
besitzt, falls erforderlich, eine getrennte private Terminal-Capability und
verwendet niemals die Recovery-Capability. Es gibt weder caller-supplied
Bool/Enum noch öffentliches `setExclusiveMode(false)` oder
`enterRecoveryWindow()`.

Jeder blockierte Einstieg liefert ohne Head-/Slot-Write einen fail-closed
`RecoveryPending`/`NotAllowedInState`-Status; die Headrevision bleibt
unverändert. Solange ExclusiveMode aktiv ist, kann daher ein
`headRevision > expected` nur aus dem einmalig autorisierten #18-Forward-Pfad
stammen; eine normale Recovery-Reevaluation oder ein direkter
`persistRecoveryCandidate()`-Aufruf kann diese Provenienz nicht erzeugen. Die
Klassifikation wird vollständig an
`classifyRecoveryEvidence(expected)` in #17 delegiert. Schema 3 bleibt ohne
neues Feld; `mutationKind` wird ausdrücklich nicht als committed Provenienz
verwendet, weil es im Prepared-Head verschwindet.

| Klasse | mechanischer Schema-3-Nachweis | Wirkung |
|---|---|---|
| `PendingHandoff` | Committed Headrevision, Current- und Fallback-Referenz entsprechen exakt der Evidence; Current ist physisch ein gültiger aktiver `Fault` desselben Run-/Program-/Manual-Kontexts | genau einmal `prepareSafetyFallbackRecovery(expected)` -> `FallbackRecoveryPending` -> #18 |
| `RecoveryForwardAlreadyCommitted` | Headrevision liegt strikt vorwärts; #17 beweist eine gültige RecoveryEvaluation-/Recovery-Snapshotform derselben geladenen Evidence, inklusive Run-/Program-/Manual-Bindung und gültiger Recoveryfelder | kein Fallback-Replay, kein Restart, kein Terminalisieren; #18 führt den bestehenden Zustand weiter, danach Safety-Finalisierung |
| `RecoveryRejectedAlreadyCommitted` | Headrevision liegt strikt vorwärts; #17 beweist die kanonische `Fault`-/RecoveryRejected-Form ohne offenen Recoveryanker, mit derselben Run-/Program-/Manual-Bindung | kein Fallback-Replay/Restart; forward-only Tombstone, dann Intent/Evidence-Clear und normaler SAFE_BOOT-Exit |
| `TerminalAlreadyCommitted` | Headrevision liegt strikt vorwärts; Current ist exakt gültiges `NoActiveRun`/`Standby` ohne Fallback | kein Resume/Restart; Intent/Evidence nach Readback löschen, `safeBootRequired` bis normalem Exit gesetzt lassen |
| `InvalidOrOrphaned` | jede andere, unvollständige, widersprüchliche oder nicht mehr an die Evidence bindbare Kombination | kein Resume/Retry; forward-only terminalisieren, Intent/Evidence erst nach Tombstone löschen, SAFE_BOOT |

Der Nachweis ist tragfähig, weil der Guard jeden normalen Einstieg mechanisch
vor `writeSnapshotCore()` blockiert und nur die benannten #18-Mutationspfade
ein internes, bootlokal begrenztes Erlaubnisfenster erhalten. Die
`RunPersistenceMutationKind::Recovery`-Kennzeichnung bleibt eine technische
Prepared-Head-Eigenschaft und wird nicht als Committed-Provenienz behauptet.
Der tatsächliche Committed-Head wird immer über Current/Fallback und physische
Records geprüft. Kann eine künftige Implementierung eine der fünf Klassen nicht
eindeutig aus Schema-3-Daten, Evidence und der Forward-only-Lockgrenze belegen,
ist STOP/Owner-Entscheid zwingend; es gibt keinen Schemawechsel und kein Raten.

Nach einem eindeutig bestätigten #17/#18-Commit gilt:

```text
Fall A RecoveryForwardAlreadyCommitted:
  keine neue Safetyursache, kein S3/Y4, runRecoveryForbidden=false,
  Marker/SafetyState healthy und Config qualified
  -> ein SafetyState-Commit/readback: Intent=None, source=0, Evidence=zero,
     attempted=0, outcome=None, safeBootRequired=false
  -> erst nach diesem Readback darf der #18-/Orchestratorpfad weiterarbeiten.

Fall B RecoveryRejectedAlreadyCommitted oder TerminalAlreadyCommitted:
  Tombstone exact readback -> Intent/Evidence=None durable -> safeBootRequired bleibt true
  -> ausschließlich normaler SafeBootExitCompleted-Pfad.

Fall C #17/#18-Write nicht eindeutig bestätigt oder neuer S3/Y4 vor Finalisierung:
  keine Finalisierung, kein Intent-/Evidence-/safeBoot-Clear, ImmediateStop/SAFE_BOOT.
```

Das Clear von `safeBootRequired` in Fall A ist keine Runfreigabe und ersetzt
weder #18 RecoveryPending/Resume noch dessen Zeit-, Sensor- oder
Progressentscheidung. Vor seinem SafetyState-Readback gibt es keinen
Planner-/Sink-Tick. Ein gültiger S3-Recovery-Boot bleibt bis dahin in der
fail-closed Boot-/Recovery-Orchestration; falls die Umsetzung tatsächlich
`ProcessState::SafeBoot` betritt, plant sie zusätzlich einen expliziten,
validierten FSM-Übergang in den Recoveryzustand statt einer Direktzuweisung.

Pflichttests: ursprünglicher Fault-current genau ein Handoff; Crash direkt
nach Hop 1, nach weiterer RecoveryEvaluation, nach RecoveryResume, nach
RecoveryRejected/Fault und nach NoActiveRun-Tombstone; kein Fallback-Replay und
kein Terminalisieren eines bereits recoverten Runs; Fall-A-Finalisierung,
Crash davor/danach, RecoveryPending/Resume/Terminal, Finalisierung
WriteError/CommitOutcomeUnknown, neuer S3/Y4 zwischen #18-Commit und
Finalisierung sowie kein Planner-/Sink-Tick davor.

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
aktiver Run und kein Y4/safeBoot-Blocker besteht, wird ohne Intent erst nach
dem exakten `FaultResetCompleted`-FSM-Ereignis nach `Standby` gegangen.
Existiert ein aktiver Run und `runRecoveryForbidden=false`, werden #17-
Evidence, Prepared und dann Attempted wie in Abschnitt 8 committed und genau
ein Restartversuch ausgeführt. Bei einer SafetyTaskRecovery-Episode geschieht
diese S3RunRecovery erst nach erfolgreicher Task-/Treiber-Requalifikation und
dem Service-Target-Reset als der ausdrücklich zweite, getrennte Restartversuch;
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
   + SafetyState S3RunRecovery mit vollständiger Evidence
   + #17 classifyRecoveryEvidence(expected) == PendingHandoff
   + keine aktiven S3/Y4
   -> prepareSafetyFallbackRecovery(expected) liest/verifiziert den bestehenden
      Fallback
   -> FallbackRecoveryPending -> bestehendes #18
```

Fall A bleibt unverändert der normale #17/#18-Recoverypfad. Fall B ist die
neue, explizit durch S3-Reset, SafetyState-Intent und typisierte #17-Evidence
autorisierte Auswahl. Ein aktiver persistent gespeicherter S3/Y4-Latch
blockiert beide Fälle bis zum jeweiligen Reset-/Terminalvertrag. `#24` liest
oder dekodiert keine #17-Wirebytes und entscheidet keine der fünf #17-Klassen
selbst.

Für den Recovery-Boot werden die schmalen #17-APIs
`captureSafetyRecoveryEvidence(...)`, `classifyRecoveryEvidence(expected)` und
`prepareSafetyFallbackRecovery(expected)` geplant. Sie sind erforderlich, weil der
direkte #17-Code bei gültigem Current in `loadAndInitialize()` den Current lädt,
`LoadedActiveRun` setzt und zurückkehrt; ein gültiger Fault-current lädt seinen
Fallback **nicht** automatisch. Die API ist ein #17-interner, read-only
RAM-Handoff und darf nur:

- `currentHead_`, den bereits geladenen gültigen Fault-current und seine
  Fallback-Referenz erneut prüfen und die erwartete typisierte Evidence exakt
  binden;
- die lokale technische `loadReference(...)`-Logik aus
  `loadAndInitialize()` als private Helperfunktion wiederverwenden (nicht
  kopieren): Slotread, Envelope, Epoch, Schema, CRC/Codec und
  `runCheckpointReferenceMatches()`;
- den Fallback explizit in `slots_[fallback.slot]` laden und Current/Fallback
  an `runId`/Programmsnapshot/Manual-Runidentität binden;
- die technische Evidence prüfen; `runRecoveryForbidden`, S3-Intent, Config,
  Sensor- und Auth-Gates bleiben #24-Safetyentscheidungen;
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
gültigem Fault-current. Die drei neuen APIs extrahieren diese vorhandenen
Bausteine DRY für Capture, Klassifikation und den zusätzlichen, eng begrenzten
Safety-Handoff. Der bestehende
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
   |
   +--> Evidence nicht beweisbar / nicht handoffberechtigt:
          technischer #17-Abandon -> NoActiveRun/STANDBY durable
          -> exact readback; RAM bleibt Fault/SafeBoot
          -> CauseClear + TargetReset + letzter Blocker-Clear
          -> FaultResetCompleted oder SafeBootExitCompleted nur RAM-seitig
             nach den jeweils dauerhaft bestätigten Domaingrenzen; kein zweiter #17-Write
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
`ProcessEvent::FaultResetCompleted`, `ProcessEvent::SafeBootExitCompleted`
und `ProcessEvent::RunAbandonCompleted` sowie die gleichnamigen
`TransitionReason`-Werte. `FaultResetCompleted` erlaubt erst nach dem letzten
Blocker `Fault -> Standby`; `SafeBootExitCompleted` erlaubt erst nach dem
normalen SAFE_BOOT-Gate `SafeBoot -> Standby`. `RunAbandonCompleted` ist ein
technischer, nicht freigebender Lifecycle-Schritt und darf `Fault -> Fault`
oder `SafeBoot -> SafeBoot` markieren, aber niemals RAM-seitig Standby
freigeben. Der technische #17-Abandon ist die einzige Run-Domain-
Durabilitygrenze: alter Run -> #17-Abandon -> `NoActiveRun`/`STANDBY`-
Tombstone -> exaktes Readback. `RunAbandonCompleted` ist danach höchstens ein
RAM-seitiger Lifecycle-Hinweis; er ist kein zweiter persistenter
Runtransition-Write und darf nicht über `persistTransition()` nach
`NoActiveRun` geschrieben werden.

Erst wenn der Tombstone (falls ein Run vorhanden war), der #24-TargetReset im
SafetyState und die Entfernung des letzten Safetyblockers jeweils durable und
readback-bestätigt sind, wird `FaultResetCompleted` als validierter
RAM-FSM-Kandidat `Fault -> Standby` angewendet. Das Event schreibt keinen
weiteren #17-Snapshot. Für `SafeBootExitCompleted` gilt entsprechend:
`NoActiveRun`/`STANDBY` ist bereits durable, `safeBootRequired=false` ist im
SafetyState durable und alle Exitgates sind erfüllt; danach wird nur der
validierte RAM-FSM-Kandidat `SafeBoot -> Standby` angewendet, ohne zusätzlichen
#17-Write. Kein Event darf `NoActiveRun + Fault` oder `NoActiveRun + SafeBoot`
persistieren.

Ein Crash nach den dauerhaft bestätigten Domain-Commit(s), aber vor dem RAM-FSM-Apply, führt
beim nächsten Boot zur erneuten Qualifikation desselben Events aus #17-
Tombstone und SafetyState. Es gibt keine direkte State-Zuweisung und keinen
zweiten Write-before-Apply-Pfad.

### 10.2 Terminalpunkt

Die kanonische Topologie ist ausschließlich:

| Fall | ProcessEvent | TransitionReason | Tombstone-Reihenfolge |
|---|---|---|---|
| S3 vor TargetReset nicht beweisbar | kein `RunAbandonCompleted -> Standby` bei aktivem S3 | keiner bis nach TargetReset | S3-Latch bleibt, technischer #17-Abandon -> NoActiveRun/STANDBY durable; RAM bleibt Fault/SafeBoot; erst TargetReset ohne Intent, danach `FaultResetCompleted` oder `SafeBootExitCompleted` |
| letzter S3-Target-Reset, kein aktiver Run | `FaultResetCompleted` oder `SafeBootExitCompleted` | gleichnamig | kein zusätzlicher Tombstone; nur nach entferntem Latch/Intent und passendem FSM-Gate |
| recoverbarer S3-Run | kein Fault->Standby | kein terminaler Event | erst S3Intent, RAM-Fallback, #18; kein Tombstone vor #18 |
| orphaned/non-handoffable S3RunRecovery | `SafeBootExitCompleted` erst am Schluss | `SafeBootExitCompleted` | NoActiveRun/STANDBY durable -> Intent-Clear -> RAM bleibt SafeBoot -> normaler Exit |
| #18 `RecoveryRejected` | `SafeBootExitCompleted` erst am Schluss | `SafeBootExitCompleted` | RecoveryRejected/Fault forward-only -> Tombstone -> Intent-Clear -> RAM bleibt SafeBoot -> normaler Exit |
| Y4 mit aktivem Run | `RunAbandonCompleted` nach durablem Tombstone, gleicher RAM-Fault/SafeBoot-Zustand | `RunAbandonCompleted` | autorisierter Abandon, Tombstone, RAM bleibt Fault/SafeBoot; erst CauseClear + TargetReset + letzter Blocker und danach `FaultResetCompleted`/`SafeBootExitCompleted` -> Standby |
| SAFE_BOOT-Abandon | `RunAbandonCompleted` ohne Freigabe | `RunAbandonCompleted` | Tombstone durable; RAM bleibt SafeBoot bis separatem Exit |

Zwischen einem technischen Tombstone und `CauseClear`/Service-TargetReset
existiert kein `Allowed` und kein normaler RAM-Standby. Der Tombstone ist nur
Run-Domain-Durability; `FaultResetCompleted` oder `SafeBootExitCompleted`
werden erst nach der jeweils eigenen SafetyState-/Exit-Durabilitygrenze
RAM-seitig angewendet und schreiben keinen weiteren #17-Snapshot.

Für Y4-Abandon gilt die Reihenfolge:

```text
1 SafetyGate ImmediateStop und Safety-Latch
2 autorisierte technische Run-Abandon-Evaluation
3 aktiven Run vollständig löschen und kanonischen NoActiveRun/STANDBY-
  Kandidaten bilden
4 #17 Prepared -> Slot -> Committed, exact CAS/readback
5 erst nach bestätigtem Tombstone `RunAbandonCompleted` ohne Zustandsfreigabe
   anwenden; RAM bleibt Fault/SafeBoot
6 CauseClear + Service-Target-Reset nach dem Tombstone ausführen
7 erst nach dem letzten Blocker `runRecoveryForbidden=false` und
  `FaultResetCompleted` oder `SafeBootExitCompleted` ausführen
```

Für S3 vor TargetReset nicht beweisbar bleibt der S3-Latch bis zum
exact-readback-bestätigten Tombstone aktiv; `RunAbandonCompleted` darf deshalb
keinen Standby-RAMzustand freigeben. Für orphaned/non-handoffable
S3RunRecovery und #18-Reject existiert dagegen kein Source-Latch mehr: erst
der Tombstone, dann Intent-Clear, RAM weiter SafeBoot und ausschließlich am
Ende `SafeBootExitCompleted`.

Ein Crash vor Schritt 7 lässt `runRecoveryForbidden=true`; ein Crash zwischen
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
**serialisierte** Eventform ist manuell Big-Endian und exakt 12 Byte:

```text
eventKind:uint8, faultCode:uint16, faultSource:uint8, instanceId:uint32,
lineageState:uint8, oldLineageKnown:uint8, detail0:uint8, detail1:uint8
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
| `0x11` | `FaultAcknowledged` |
| `0x12` | `FaultResetRejected` |

Unbekannte Eventwerte werden von #19 beim Handoff abgelehnt; die
`static_assert`-Tabelle prüft `kSafetyEventKindCount=18`. `HistoryDiscontinuity`
ist Teil dieses bestehenden, bounded `SafetyEventBatch`; es gibt keinen zweiten
Handoff und keine Queue. Für diese EventKind ist `lineageState` exakt
`PartialSuccessor` oder `TotalDiscontinuity`, und `oldLineageKnown` ist der
explizite Boolnachweis. #19 bleibt alleiniger Persistenzbesitzer und darf IDs
vor und nach einer TotalDiscontinuity nicht korrelieren.

`ActorSource`, `FaultResetRejection`, `ResetCause` und `RestartIntentKind`
erhalten jeweils eigene geschlossene uint8-Wiretabellen; ihre Wirewerte sind
nicht von C++-Enum-Reihenfolgen abgeleitet:

```text
ActorSource:          1 LocalDisplay, 2 WebInterface
RestartIntentKind:    0 None, 1 SafetyTaskRecovery, 2 S3RunRecovery
ResetCause:           1 PowerOn, 2 SoftwareRestart, 3 WatchdogOrPanic,
                      4 Brownout, 5 External, 6 Unknown
FaultResetRejection:  1 TargetNotActive, 2 CauseNotCleared,
                      3 EvaluationDenied, 4 ServiceProofInvalid,
                      5 StaleRevisionOrLineage, 6 TerminalOrRecoveryBlocked
```

`FaultResetRejection` ist ausschließlich die stabile Projektion der bereits
intern getroffenen finalen TargetReset-Ablehnung; sie erweitert oder ersetzt
die #15-`FaultResetEvaluation` nicht. `ActorSource` ist die kompakte
Projektion der bestehenden `CommandEnvelope`-Provenienz. `FaultAcknowledged`
publiziert erst nach der erfolgreichen atomaren FaultCore-/Message-RAM-
Quittierung, ohne SafetyState-Write oder FaultRevision.
`FaultResetRejected` publiziert erst
nach der endgültigen internen TargetReset-Entscheidung; `FaultReset` bleibt
der erfolgreiche persistente Reset. Damit kann #19 später Fehler-, Reset- und
Bedienjournal führen, ohne dass #24 Journalpersistenz, Strings oder Queues
einführt.

Die Crossfields sind für jede Eventfamilie vollständig festgelegt:

| Eventfamilie | `instanceId` | `faultCode` / `faultSource` | `lineageState` | `oldLineageKnown` | Details |
|---|---|---|---|---|
| `FaultRaised`, `FaultCleared`, `FaultRelapsed` | bei P1/O2 `0`, bei S3/Y4 die konkrete Fault-Instance | exakt die Catalog-Identity | aktuelle Lineage | Continuous=`0`; Partial=`1`; Total faktisch `0` oder `1` | Details `0` |
| `FaultAcknowledged` | P1/O2 `0`, S3/Y4 konkrete Instance | exakt die acknowledged Identity und dieselbe MessageReference | aktuelle Lineage | wie oben | `detail0=ActorSource`, `detail1=0`; kein SafetyState-Write/keine FaultRevision |
| `FaultReset` | konkrete persistente S3/Y4-Target-Instance | exakt die Target-Identity | aktuelle Lineage | wie oben | `detail0=ActorSource`, `detail1=0`; P1/O2 sind über diesen Servicevertrag nicht targetierbar |
| `FaultResetRejected` bei auflösbarer persistenter S3/Y4-Instance | konkrete Target-Instance | exakt die Target-Identity | aktuelle Lineage | wie oben | `detail0=ActorSource`, `detail1=FaultResetRejection` |
| `FaultResetRejected` bei `TargetNotActive`/nicht auflösbarer Instance, einschließlich `targetInstanceId=0` | `requested targetInstanceId` | beide `0` | aktuelle Lineage | wie oben | `detail0=ActorSource`, `detail1=TargetNotActive`; ausschließlich diese Neutralform darf `faultCode=0`/`faultSource=0` verwenden |
| `GateChanged`, `TerminalRequiredSet`, `TerminalRequiredChanged` | immer `0` | beide `0` | aktuelle Lineage | wie oben | Details `0` |
| `IntentPrepared`, `RestartAttempted`, `RestartRejected` | immer `restartIntentSourceInstanceId` | beide `0` | aktuelle Lineage | wie oben | `detail0=RestartIntentKind`, `detail1=0` |
| `BootClassified` | immer `0` | beide `0` | aktuelle Lineage | wie oben | `detail0=ResetCause`, `detail1=abnormalRestartCount` |
| `SafeBootEntered`, `RedundancyRepair` | immer `0` | beide `0` | aktuelle Lineage | wie oben | Details `0` |
| `HistoryLossDetected`, `HistoryDiscontinuity`, `SafetyReinitialized` | immer `0` | beide `0` | `PartialSuccessor` oder `TotalDiscontinuity` | Partial exakt `1`; Total faktisch `0` oder `1` | Details `0` |

`faultCode=0` und `faultSource=0` sind damit ausschließlich reservierte,
event-kind-spezifische Neutralwerte für Eventfamilien ohne auflösbare
FaultIdentity sowie für den ausdrücklich definierten
`FaultResetRejected/TargetNotActive`-Fall; sie bedeuten dort nicht
`UnknownFaultIdentity` und dürfen nicht als FaultCatalog-Eintrag decodiert
werden. Goldenbyte- und Decode-Tests decken jede der fünf Familien,
beide Lineagezustände, beide zulässigen TotalDiscontinuity-
`oldLineageKnown`-Werte, P1/O2 mit `instanceId=0`, persistente
Fault-Identities, Actor-/Reject-/Reset-/Intent-Details und Neutralwerte ab.
Zusätzlich müssen eine gültige S3/Y4-`FaultResetRejected`-Identity, eine
unbekannte oder entfernte Target-Instance, `targetInstanceId=0` bei mehreren
aktiven P1/O2-Faults sowie ein erfolgreicher persistenter TargetReset jeweils
als eigene Goldenbyte-/Decoderfälle beweisen, dass keine P1/O2- oder fehlende
TargetIdentity erfunden wird.

Der native C++-Datentyp ist davon getrennt und darf nicht `packed` werden:

```cpp
struct NativeSafetyEvent {
    uint8_t eventKind;
    uint16_t faultCode;
    uint8_t faultSource;
    uint32_t instanceId;
    uint8_t lineageState;
    uint8_t oldLineageKnown;
    uint8_t detail0;
    uint8_t detail1;
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
serialisiert. Ein Event ist exakt `kSafetyEventWireBytes = 12` Byte. Die feste
Menge ist `kMaxSafetyEventsPerMutation = 36` und ergibt
`kSafetyEventBatchWireBytes = 13 + 36*12 = 445 Byte` pro Wire-Batch. Die
Herleitung bleibt korrekt, weil ein Batch genau eine Durability-/RAM-Grenze
beschreibt: Der reine persistente Multi-Fault-Worst-Case ist
`33 * FaultRaised + GateChanged + TerminalRequiredSet +
TerminalRequiredChanged = 36`. Bei einem Mixed-Snapshot wird der Batch an der
Grenze getrennt: bis zu acht P1/O2-FaultRaised/FaultCleared plus höchstens ein
`GateChanged` ergeben höchstens 9 RAM-Events; der bestätigte persistente Batch
enthält höchstens 33 Fault-/Latch-Events plus die nicht doppelt publizierte
Terminalfolge. Die Summe eines Mixed-Snapshots darf daher größer als 36 sein,
aber kein einzelner `SafetyEventBatch` überschreitet 36. Nicht persistente
P1-/O2-Ereignisse, `BootClassified`, `SafeBootEntered`, `RedundancyRepair`,
`HistoryLossDetected`, `HistoryDiscontinuity` und `SafetyReinitialized` tragen
`instanceId=0`; S3-/Y4-Faultereignisse tragen die zugehörige persistente
InstanceId.

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
P1/O2-RAM-Lifecycle: bis zu 8 FaultRaised/FaultCleared plus höchstens ein
             GateChanged = höchstens 9; spätere Aktivierung ist wieder
             FaultRaised, nie FaultRelapsed
Ack: FaultAcknowledged = 1 nach atomarer Message-/FaultCore-RAM-Mutation
TargetReset-Reject: FaultResetRejected = 1 nach atomarer Commandentscheidung
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
Einzelmutation. `static_assert` prüft `kSafetyEventKindCount == 18`,
`kMaxSafetyEventsPerMutation == 36`, `kSafetyEventWireBytes == 12`,
`kSafetyEventBatchWireBytes == 445`, `maxEvents <= 36`,
`sizeof(NativeSafetyEvent)==16`, `alignof(NativeSafetyEvent)==4` und
`sizeof(NativeSafetyEventBatch)==592` auf Native und ESP-IDF. Ein Überschreiten
ist `Y4_INTERNAL_SAFETY`; es werden keine Events still abgeschnitten.

Ein persistenter Multi-Fault-Raise publiziert seinen einen Batch erst nach dem
einen bestätigten SafetyState-Commit. Ein Crash davor publiziert keinen
persistenten Teil-Batch; ein Crash danach sieht die vollständige durable Menge.
Bei einem Mixed-Snapshot ist der P1/O2-RAM-Batch davon getrennt und darf vor
dem SafetyState-Commit bereits publiziert sein; der spätere persistente Batch
enthält nur die nach Readback bestätigten S3/Y4-Fakten und wiederholt
`GateChanged` nicht. HistoryLoss
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
SafetyState payload: 32 + (33 * 24) + 68 Evidence = 892 Byte
SafetyState max Envelope: 33 + 892 + 4 = 929 Byte <= 1024
RunPersistenceRecoveryEvidence wire: 8 + 30 + 30 = 68 Byte
RunPersistenceRecoveryEvidence native: sizeof=88 Byte, alignof=8 Byte
SafetyFaultMessageReference: sizeof=12 Byte, alignof=4 Byte
std::optional<SafetyFaultMessageReference>: sizeof=16 Byte, alignof=4 Byte
RuntimeMessage: sizeof vorher=56 Byte, nachher=72 Byte, Delta=16 Byte
RunCommandState::messages: 16 * Delta(RuntimeMessage) = 256 Byte
FaultCatalog: 41 Identities; persistente Records: 33 (19 S3 + 14 Y4)
EmergencyMarker payload: 26 Byte
EmergencyMarker max Envelope: 33 + 26 + 4 = 63 Byte <= 64
SafetyEventBatch Wire: 13 + (36 * 12) = 445 Byte
SafetyEventBatch native: 592 Byte
Factory-new Bootstrap: 4 bestätigte Writes (sf0, sf1, sem0, sem1),
  jeder Write mit exact readback; Cut-Points 0..4
```

Die neue `SafetyFaultMessageReference` liegt ausschließlich in der RAM-
Projektion `RuntimeMessage` und nicht im `RunPersistenceSnapshot` oder #17-
Wire. Die oben genannten Größen sind der geplante Native-/ESP-IDF-/ESP32-
ABI-Vertrag und werden in beiden Zielumgebungen per `static_assert` sowie im
Resource-Test nachgewiesen. Die bisherige `RuntimeMessage`-Größe von 56 Byte
steigt mit dem optionalen fixed-size-Feld auf 72 Byte; bei 16 Slots sind das
256 Byte zusätzlicher Issue-24-RAM.

SafetyState und EventBatch behalten ihre statischen Bytewerte: Die
`RunPersistenceRecoveryEvidence`-Projektion bleibt 68/88 Byte, der
`SafetyState` bleibt 892/929 Byte, und der größte einzelne EventBatch bleibt
`33 * FaultRaised + GateChanged + TerminalRequiredSet +
TerminalRequiredChanged = 36` Events beziehungsweise 445/592 Byte. Ein
Mixed-Snapshot kann über seine zwei Durabilitygrenzen zusammen mehr Events
erzeugen, benötigt aber nur zwei bereits im 36er-Bound enthaltene Batches; es
entsteht kein größerer Event-Peak und keine PSRAM-Abhängigkeit.

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
| persistente native Records + Evidence | `33 * 32 + 88` | 1144 B |
| transiente native Records, alle 8 Identities | `8 * 24` | 192 B |
| native EventBatch | `592` | 592 B |
| Codec-/Validator-Scratch einschließlich Event-Wirebuffer | fest | 512 B |
| FaultCatalog[41] plus SafetyFaultService | `41 * 32 + 256` | 1568 B |
| `RunCommandState::messages` SafetyFaultReference-Delta | `16 * 16` | 256 B |
| **SafetyCore fixed** | `6184 + 256` | **6440 B** |
| bestehender IStateStore-Heap | `3*1024 + 3*64 + 512` Allocatorreserve | **3776 B** |
| Safety-Call-Stack | gemessene feste Obergrenze | 2048 B |
| **SafetyCore Peak-Budget** | `6440 + 3776 + 2048` | **12264 B** |

`sizeof`-/`alignof`-Assertions gelten für Native und ESP-IDF/ESP32; der
Ressourcenbericht muss den tatsächlichen `RuntimeMessage`-/
`std::string`-/Allocator- und Stack-Peak gegen 12264 Byte ausweisen. Wird der
Budgetwert überschritten, ist der Plan-Gate fehlgeschlagen. Keine dynamische
Allokation ist im
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
  #17 RunPersistenceRecoveryEvidence, dessen Codec-/Referenz-/Validator-
  helper, Capture/Klassifikation/Fallback-Handoff; #24
  SafetyFaultService, FaultCatalog, SafetyState codec/store wrapper,
  EmergencyMarker, RestartSupervisor, ResetCause-Policy, ServiceResetProof,
  Boot-/Process-/Recovery-Orchestration, SafetyEventBatch und #24 Producer-
  Mapping; app-interner Testhelper für ServiceResetProof. #24 serialisiert
  die Evidence nur über den gemeinsamen #17-Helper und besitzt keine eigene
  RunCheckpointReference-/CRC-/Schema-/Epochsemantik.

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
   `captureSafetyRecoveryEvidence()` und `classifyRecoveryEvidence(expected)`
   verwenden denselben extrahierten privaten `loadReference`-Helper und
   beweisen Referenzen, CRC/Schema/Epoch, Run-/Program-/Manual-Kontext sowie
   Pending/Forward/Rejected/Terminal/Orphan mechanisch in #17.
   `prepareSafetyFallbackRecovery(expected)` lädt den referenzierten Fallback
   in RAM, stellt ID-Fenster und Caches korrekt her und setzt erst dann
   `FallbackRecoveryPending`. Weder Capture, Klassifikation noch Handoff
   schreiben Head, Slot oder Fallbackreferenz.
3. **Kein neues Wirefeld/Schemachange in #17:** #17 bleibt
   `kCurrentRunPersistenceSchema=3` (`run_persistence_contract.hpp:19-31`);
   der typisierte Evidence-Vertrag ist ein #17-Domain-Helper und kein neues
   Head-/Checkpoint-Wirefeld. Intent, Latch und Terminalpflicht leben im
   SafetyState RecordType 9/10, nicht in Head/Checkpoint.
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

Damit sind die Architektur-Kern-Gates PASS: #17 besitzt typisierte
RecoveryEvidence einschließlich mechanischer Validierung/Klassifikation und
Fallback-Handoff; #24 persistiert nur die Evidence-Instanz mit
S3RunRecovery; #18 bleibt alleinige fachliche Recoveryautorität; kein
#17-Schemawechsel, kein committed-`mutationKind`-Claim, forward-only
Persistenz, keine doppelte #17-Validierung und persistente Y4-Terminalität.

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
Y4 aktiv
   |
   +--> ImmediateStop; RAM bleibt Fault/SafeBoot
   +--> technischer #17-Abandon -> NoActiveRun/STANDBY durable
   +--> exact readback; RAM bleibt weiterhin Fault/SafeBoot
   |
CauseClear + Service TargetReset
   |
   +--> letzter Safetyblocker entfernt und SafetyState durable
   |
   +--> FaultResetCompleted oder SafeBootExitCompleted als RAM-FSM-Kandidat
   +--> kein zusätzlicher #17-Write
   |
   v
STANDBY / separater SAFE_BOOT-Exit

Zwischen Tombstone und CauseClear/TargetReset gibt es kein `Allowed` und
keinen normalen RAM-Standby. `RunAbandonCompleted` bleibt ein nicht
freigebender RAM-Hinweis. Ein Crash vor dem FSM-Apply qualifiziert denselben
Event beim nächsten Boot erneut aus den beiden dauerhaft bestätigten Domainwahrheiten.
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
| `Current/Fault` plus gültiger S3RunRecovery-Intent und vollständiger #17-Evidence | #24 implementiert | #17 klassifiziert Pending/Forward/Rejected/Terminal/Orphan; nur Pending read-only Handoff -> FallbackRecoveryPending, danach #18 |
| S3RunRecovery-Evidence mit Intent None/SafetyTaskRecovery | #24 implementiert | SafetyState-Crossfield-Decode fail-closed; Evidence zero oder nicht zulässig |
| committed `mutationKind` als Provenienz | #24 implementiert | kein Claim; Committed-Head enthält nur Current/optional Fallback |
| Forward/Rejected/Terminal/Orphan nach Reboot | bestehenden Vertrag erweitern | #17 mechanische Klassifikation, kein Fallback-Replay, kein Raten |
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
- zwei gleichzeitig aktive S3-Faults mit je eigener SafetyFault-Message:
  Ack A quittiert exakt A auch bei höherer sichtbarer Priorität von B; zusätzlich
  P1/O2-`instanceId=0`, unbekannte MessageId, fehlende/stale Reference,
  bereits quittierte Message und normaler MessageAck ohne FaultAcknowledged;
- erfolgreicher persistenter S3/Y4-TargetReset mit exakter Instance/Identity;
  `FaultResetRejected` für eine auflösbare persistente S3/Y4-Instance mit
  exakter Rejection-Identity sowie `TargetNotActive` für unbekannte/entfernte
  Targets und `targetInstanceId=0` mit `faultCode=0`/`faultSource=0` und
  requested instance; mehrere aktive P1/O2 werden nicht über den Service-
  Resetvertrag targetiert;
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
- gültiger Current/Fault plus capturebare Evidence: `loadAndInitialize()==Current`
  und `LoadedActiveRun`; erst `classifyRecoveryEvidence()==PendingHandoff`
  erlaubt `prepareSafetyFallbackRecovery(expected)`, lädt den Fallback
  read-only, setzt `FallbackRecoveryPending` und erlaubt danach die bestehende
  `activateFallbackRecoveredRun()`-Grenze;
- S3-Latch noch aktiv und Recovery nicht beweisbar: kein Intent, alter Run
  zunächst über den bestehenden #17-Abandon zu NoActiveRun/STANDBY mit exact
  readback, dann erst TargetReset ohne Intent und separater SAFE_BOOT-Exit;
- Prepared-S3RunRecovery ohne handoffberechtigte Matrix oder Fallback-Evidenz:
  Crash vor Tombstone, Tombstone indeterminate, Tombstone vor Intent-Clear,
  Prepared plus bereits durablem NoActiveRun/STANDBY sowie Intent-Clear vor
  SAFE_BOOT-Exit terminalisieren ohne Resume, Restart-Retry oder neue Y4;
- `Prepared(S3RunRecovery, Attempted=1)` wird nach `loadAndInitialize()`
  vollständig durch #17 als PendingHandoff, RecoveryForwardAlreadyCommitted,
  RecoveryRejectedAlreadyCommitted, TerminalAlreadyCommitted oder
  InvalidOrOrphaned klassifiziert: nur der originale Fault-current mit
  passendem Fallback darf einmal handoffen; Recovery-Fortschritt wird
  weitergeführt, Rejected/Terminal/Orphan werden ohne Fallback-Replay
  forward-only terminalisiert;
- erfolgreicher Handoff: ein #18-Recovery-Commit muss eindeutig durable sein,
  bevor ein einzelner SafetyState-Commit Intent und `safeBootRequired` löscht;
  Crash nach #18 vor Finalisierung, nach Finalisierung vor weiterer Recovery,
  Unknown/WriteError und ein neues S3/Y4 dazwischen bleiben fail-closed;
- #18 RecoveryPending, Resume und direkte Terminalisierung werden getrennt
  geprüft; ein Terminal-Tombstone löscht nur den Intent und behält
  `safeBootRequired=true` bis zum normalen SAFE_BOOT-Exit;
- Crash vor/nach Safety-Commit, Fault-Run-Commit, Reset-Commit,
  Prepared/Attempted, Restart-Port, Boot-Handoff und #18-Write;
- Crash nach durable #18-Fortschritt vor Intent-Clear: kein Replay, Intent
  anhand der neuen Run-Wahrheit abschließen;
- `Restart Rejected`: kein Auto-Retry, späterer Service-/Owner-Reboot möglich.
- nach technischem Tombstone gibt es keinen zweiten `persistTransition()`-
  Write: `RunAbandonCompleted` bleibt Fault/SafeBoot; erst nach durablem TargetReset,
  letztem Blocker-Clear und der jeweiligen SafetyState-/Exitgrenze folgen
  `FaultResetCompleted` oder `SafeBootExitCompleted` ausschließlich als
  RAM-FSM-Apply; Crash davor qualifiziert denselben Event beim nächsten Boot.
- bei Prepared-S3Recovery blockieren `persistCommand`, `persistTransition`,
  `checkpointPeriodic`, `persistSensorSelection`, normale Recovery-Time-/
  Correction-Writes und die gemeinsame Core-Schranke; nur die benannten #18-
  Recoverymutationen dürfen mit der privaten einmaligen
  `RecoveryWriteCapability` schreiben, danach klassifiziert #17 den Forward-
  oder Terminalstand. Pflichtfälle bei aktivem ExclusiveMode sind jeweils
  `persistCommand`, `persistTransition`, `checkpointPeriodic`,
  `persistSensorSelection` und ein direkter öffentlicher
  `persistRecoveryCandidate` -> `BLOCKED` ohne Write; der exakt interne #18-
  Initial-Forward-Pfad -> erlaubt; fehlende, gefälschte, kopierte,
  fremder-Generation oder verbrauchte Capability -> `BLOCKED`. Erst nach
  eindeutig durablem #18-Forward-Commit werden Intent/Evidence gelöscht und
  ExclusiveMode geschlossen.

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

- jeder Bootscan liefert genau einen Status `TwoValid`, `OneValid`, `ZeroValid`
  oder `ReadIndeterminate`; `ZeroValid` wird nur von vollständig qualifiziertem
  Factory-new- oder HistoryLoss-Reinit weitergeführt, sonst ImmediateStop/
  SAFE_BOOT ohne Winner, Active-/Cleared-Write oder Blindrepair;
- `ReadIndeterminate` durch ReadError/CapacityError/Decode-Indeterminate führt
  ohne Gewinnerclaim und ohne Alternativwrite zu ImmediateStop/SAFE_BOOT;
- `corrupt+corrupt`, `corrupt+NotFound` und beide semantisch ungültigen Marker
  sind ZeroValid; `CapacityError` beim Scan ist ReadIndeterminate;
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
- P1/O2 Raise/Clear und GateChanged werden ausschließlich nach ihrer
  atomaren RAM-FaultCore-Mutation publiziert, ohne SafetyState-Commit zu
  behaupten.
- NotFound, korruptes Peer, `CommitOutcomeUnknown`, Alternativwrite nur nach
  mechanischem NOT-COMMITTED-Beweis mit identischen Kandidatbytes/-revision,
  ReadError-/Capacity-/MarkerIndeterminate-Abbruch, Repair-Crash und
  `markerRevision==UINT32_MAX` decken den normalen Marker-Zwei-Slot-Algorithmus
  ab.

### 19.5 SAFE_BOOT/Restart

Factory-init, pre-#24-Upgrade, fehlender SafetyState auf Nicht-Factory-Gerät,
alle ResetCause-Werte, gültiger/ungültiger Intent, Attempted/Rejected,
Counter 0/1/2/3, 30-Minuten-Requalifikation, Safety-Redundanzrepair,
`safeBootRequired`, Crash nach Flag-Clear und die vollständige Exit-Matrix.
Der normale Exit beweist nach dem durablem Flag-Clear den FSM-Kandidaten
`ProcessEvent::SafeBootExitCompleted` /
`TransitionReason::SafeBootExitCompleted` und ausschließlich die Topologie
`SafeBoot -> Standby`; ein aktiver S3-Recovery-Handoff verwendet diesen Event
nicht als Recovery-Freigabe. Der Test muss außerdem den bereits durablem
#17-`NoActiveRun`/`STANDBY`-Readback, die RAM-seitige
`valid...Topology()`-/`decide...Event()`-Prüfung, den RAM-FSM-Apply und den
fehlenden zweiten #17-Write nachweisen; ein Crash davor qualifiziert denselben
Event beim nächsten Boot ohne neuen Tombstone-Write.

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
`Current/Fault + S3RunRecovery`-Handoff und prüft Capture-, Codec-,
Klassifikations- und Required-Status inklusive `AlreadyPersisted`-Readback.
Der reine SafetyState-Decode prüft nur Payload/CRC/Schema, Intent-Crossfields,
Evidence-Wire und Lineage; er verlangt keinen geladenen #17-Current/Fault-
Zustand. Erst danach lädt #17 und `classifyRecoveryEvidence(expected)` liefert
`PendingHandoff`, `RecoveryForwardAlreadyCommitted`,
`RecoveryRejectedAlreadyCommitted`, `TerminalAlreadyCommitted` oder
`InvalidOrOrphaned`. Nur `PendingHandoff` verlangt den ursprünglichen Fault-
Current, den physisch verifizierten Fallback und die passende ResetCause-
Qualifikation; die Forward-/Rejected-/Terminal-Formen werden nach jedem
erlaubten #18-Forward-Schritt einschließlich Crash vor Intent-Clear akzeptiert.
Candidate-time-Provenienz (aktives Preimage, CauseClear, exakter
ServiceResetProof, atomare Entfernung plus Intent/Evidence) bleibt davon
getrennt. Ressourcen-/Eventtests prüfen native und ESP-IDF-`sizeof`/`alignof`,
`SafetyFaultMessageReference`, `optional<SafetyFaultMessageReference>`,
`RuntimeMessage` vorher/nachher und `16 * Delta(RuntimeMessage)` in beiden
Zielumgebungen, den tatsächlichen RAM-/Heap-Peak und
Marker-Exhaustion bei `UINT32_MAX` sowie uint64-Grenzen von
`recordRevision`/`attemptedSafetyRecordRevision` oberhalb `UINT32_MAX`.

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
  Episode; Relapse nach CauseClear **vor** TargetReset behält dieselbe Instance
  und den verbrauchten Versuch, erst nach TargetReset ist eine neue Instance
  mit höchstens einem neuen Versuch zulässig;
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
- Mixed-Snapshot mit allen acht transienten P1/O2-Identities und neuen S3/Y4:
  bis zu acht Raise/Clear plus höchstens ein `GateChanged` im RAM-Batch,
  persistente Fault-/Terminal-Events erst nach dem SafetyState-Readback,
  kein doppeltes `GateChanged`, Crash zwischen den Grenzen mit P1/O2-Events
  aber ohne S3/Y4-Erfolgsclaim; reiner persistenter Worst-Case bleibt 36;
- `HistoryDiscontinuity` im fixed-size Batch einschließlich
  `lineageState`/`oldLineageKnown`, P1/O2 mit `instanceId=0`;
- `FaultAcknowledged`, erfolgreicher `FaultReset`, `FaultResetRejected`,
  `BootClassified` sowie Intent-/Attempt-/Rejected-Events beweisen jeweils
  ihre festen Actor-/Reject-/ResetCause-/Counter-/IntentKind-Details und bei
  Intent die `restartIntentSourceInstanceId`; unbekannte Detailwerte werden
  beim Handoff abgelehnt. Die Resettests trennen eine auflösbare S3/Y4-
  TargetIdentity von `TargetNotActive` mit requested `targetInstanceId`,
  neutralen `faultCode`/`faultSource` einschließlich `targetInstanceId=0`,
  mehreren aktiven P1/O2 und erfolgreichem persistentem Reset. Ack A/B,
  höhere Priorität B, P1/O2 `instanceId=0`, stale Lineage, unbekannte oder
  bereits quittierte Message und normaler MessageAck ohne
  `FaultAcknowledged` sind getrennte Nachweise;
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
4. #17-RecoveryEvidence: Typ, gemeinsame 68-Byte-Projektion, Capture,
   Pending/Forward/Rejected/Terminal/Orphan-Klassifikation und expliziter
   read-only Fallback-Handoff; Schema 3 bleibt unverändert.
5. Run-/Process-Integration: S3-RAM-Handoff, Y4-Terminalflag,
   Run-Abandon und FaultReset-/SafeBootExit-/nicht freigebendes
   RunAbandon-FSM; kein Head-Rollback.
6. #20/#21/#22/#23 Producer-Mapping, STALE/FAILED und Fan-/Thermik-Directive.
7. #15 Target-FaultReset gegen den echten Vertrag: öffentlicher
   `targetInstanceId`, interne TargetResetEvaluation, ServiceResetProof und
   einzige `persistentFaultRevision`.
8. #56/#57 Gate und kanonische Boot-Komposition.
9. No-bypass Command-, Orchestrator-, Planner- und Sink-Pfad.
10. Kanonische Dokumente, vollständige Injection-/Crashmatrix, Resource-/Wear-
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
- Y4-Tombstone gibt weder `Allowed` noch RAM-Standby frei: erst nach
  `CauseClear`, Service-TargetReset, letztem Blocker-Clear und den jeweiligen
  durable bestätigten SafetyState-Grenzen werden `FaultResetCompleted`/
  `SafeBootExitCompleted` RAM-seitig angewendet; kein zweiter #17-Write;
- `SafeBootExitCompleted` verlangt zusätzlich den bereits durablem und
  exact-readback-bestätigten #17-`NoActiveRun`/`STANDBY`-Tombstone sowie
  `safeBootRequired=false`; Topologie-/Decision-Validation und RAM-FSM-Apply
  sind danach rein RAM-seitig. Ein Crash davor qualifiziert denselben Event
  erneut, ohne neuen #17-Write und ohne zweiten write-before-apply-Pfad;
- gültiger Fault-current lädt den Fallback nicht automatisch: #17 Capture,
  Klassifikation und der neue read-only Handoff verwenden gemeinsam
  `loadReference`, stellen RAM/IDs/High-Watermarks korrekt her und rufen danach
  nur die bestehende #18-Grenze;
- EmergencyMarker klassifiziert `TwoValid`/`OneValid`/`ZeroValid`/
  `ReadIndeterminate`, prüft sem0-Tie, Envelope/Payload-Revision, vollständige
  State-/Reason-Crossfields und erreicht Cleared/healthy erst nach beiden
  qualifizierten Slots; Alternativwrite nur nach mechanischem NOT-COMMITTED-
  Beweis, nie nach Read-/Decode-/Marker-Indeterminate;
- HistoryLoss hat getrennte bestätigte Event-Lifecycle-Schritte; P1/O2
  publizieren nur nach atomarer RAM-FaultCore-Mutation;
- der öffentliche #15-FaultReset trägt nur `CommandEnvelope` plus
  `targetInstanceId`; die interne TargetResetEvaluation wird ausschließlich
  zusammen mit unforgeable ServiceResetProof und aktueller
  `persistentFaultRevision`/Lineage entschieden; andere Blocker blockieren
  Release, nicht den qualifizierten Target-Reset. Ein erfolgreicher Reset
  trägt nur eine exakte persistente S3/Y4-Identity; `FaultResetRejected` trägt
  bei auflösbarem Target diese Identity, sonst bei `TargetNotActive` die
  requested Instance und `faultCode=0`/`faultSource=0`, auch bei
  `targetInstanceId=0`; P1/O2 werden nicht targetiert;
- `safetyHistoryEpoch==UINT32_MAX` kann keinen PartialSuccessor wrappen;
  ausschließlich der servicegeschützte TotalDiscontinuity-Pfad bleibt;
- RestartIntent-Crossfields prüfen Rejected/Attempted, zero/full Evidence und
  aktive SafetyTaskRecovery-Source; S3RunRecovery wird candidate-time mit
  Evidence erzeugt und boot-time ausschließlich durch #17 als Pending,
  Forward, Rejected, Terminal oder Orphan klassifiziert;
- `S3RecoveryExclusiveMode` wird aus Prepared `S3RunRecovery` bootlokal vor
  #17-Load gesetzt und blockiert jeden normalen Coordinator-Schreibeinstieg;
  nur read-only Evidence und die ausdrücklich erlaubten #18-/Terminalpfade
  dürfen schreiben. Die #18-Schreibfreigabe ist eine private, nicht
  kopierbare, generationgebundene `RecoveryWriteCapability`, die nur der
  exakt benannte interne #18-Bridge an `writeSnapshotCore()` weiterreichen
  kann; `persistRecoveryCandidate()` ohne Token, fehlende/gefälschte/
  abgelaufene Berechtigung und öffentliche Enter/Exit-Methoden sind
  mechanisch blockiert. Nach dem ersten eindeutig durablem #18-Forward-Commit
  werden Intent/Evidence gelöscht und das Fenster geschlossen;
- SafetyEventBatch ist exakt an einen bestätigten atomaren FaultCore-RAM-,
  SafetyState-Commit-, Boot- oder Repair-Lifecycle-Schritt gebunden; Prepared,
  Attempted, Side-Effect und explizites Rejected werden getrennt publiziert,
  Mixed-Snapshots trennen bis zu 8 P1/O2-Events + GateChanged von den
  commitabhängigen S3/Y4-Events, GateChanged wird nicht doppelt publiziert,
  und Nicht-Fault-Crossfields sind je Eventfamilie goldenbyte-/decode-definiert;
- Quittierung, erfolgreicher Reset, Reset-Ablehnung und Bootklassifikation
  übergeben #19 ihre typisierten Journalfakten (`ActorSource`, `RejectReason`,
  `ResetCause`, `abnormalRestartCount`) ohne Strings oder eine #24-Queue;
- ein aktiver S3-Recovery-Handoff finalisiert seine SafetyState-Wahrheit erst
  nach eindeutig durablem #18-Fortschritt; der normale SAFE_BOOT-Exit ist ein
  eigener FSM-Übergang und kein Ersatz dafür;
- `Prepared(S3RunRecovery, Attempted=1)` wird anhand des Schema-3-Heads und
  der Recovery-Evidenz eindeutig als Pending, Forward, Rejected, Terminal oder
  Orphan klassifiziert; nur Pending darf den Fallback einmal read-only laden;
- ein SafetyTaskRecovery-Relapse vor TargetReset verbraucht keinen zweiten
  Restart; P1/O2 veröffentlichen nur Raise/Clear und keine Relapse-Events;
- die normale EmergencyMarker-Mutation folgt dem begrenzten Zwei-Slot-
  Kandidat-/Readback-/Alternativwrite-Algorithmus; `markerRevision` endet bei
  `UINT32_MAX` fail-closed;
- `Continuous` trägt `oldLineageKnown=0`, `PartialSuccessor` trägt `1`,
  `TotalDiscontinuity` beschreibt keine globale Fortsetzungsordnung und trägt
  deshalb faktisch `0` oder `1`;
- Catalog 41, gültige FaultSources 28 plus Reserved, persistente Records 33,
  SafetyState Payload 892 Byte, SafetyState Envelope 929 Byte, Evidence Wire
  68 Byte/native 88 Byte, `SafetyFaultMessageReference` 12/4 Byte,
  `optional<SafetyFaultMessageReference>` 16/4 Byte, `RuntimeMessage` vorher
  56/nachher 72 Byte, `16 * Delta(RuntimeMessage)` 256 Byte, EventKind 18,
  Event-Wire 12 Byte, Event-Batch-Wire 445 Byte, Native-Event 16 Byte,
  Native-Batch 592 Byte, SafetyCore fixed 6440 Byte, Peak 12264 Byte und
  Factory-Bootstrap 4 Writes sind neu gerechnet und per Assertions zu prüfen;
  es besteht keine PSRAM-Abhängigkeit;
- S3/Y4, Multi-Fault, Factory/HistoryLoss, SafetyState/Marker, Exhaustion,
  AirFallbackActive, Thermal 0..2, Fanpolicy, SafetyTaskRecovery,
  S3RunRecovery/#18, RestartLoop, ResetCause, #17-Mapping, Run-Abandon,
  Auth/Trust, HistoryDiscontinuity, simultane Ursachen, Ressourcen/Wear,
  ACCEPTANCE_TESTS und ADR-013 sind im vollständigen Rückcheck enthalten;
- Code, Source, Restartwhitelist, Oracle und Tests verwenden
  `S3_PERSISTENCE_PATH_STALL`/`PersistencePath` einheitlich;
- `displayPriority`-Richtung und `IResetController`-Returnsemantik sind
  explizit und fail-closed.

### 22.1 Vollständiger End-to-End-Owner-Gate-Rückcheck

Der Rückcheck wurde nach der Evidence-Korrektur nicht beim ersten Befund
beendet. Die folgende Matrix ist der Plan-/Architekturstatus; sie ist kein
Implementierungs- oder Testnachweis. Für jede Zeile gilt `PASS` als
Planvertrag, die tatsächliche Testausführung bleibt plan-only `NOT_RUN`:

| Gate | Status | Planbeweis |
|---|---|---|
| S3 `RecoverIfProvable` / nicht beweisbar `Terminal` | PASS | 8.7, 8.8, 9, 10 |
| Y4 `Terminal` | PASS | 3.3, 10 |
| P1/O2 transient | PASS | 3.1, 6.1, 13, 19 |
| simultane persistente Ursachen atomar | PASS | 6.1, 13, 19 |
| Multi-Fault TargetReset | PASS | 2.3, 6.2 |
| Primary/Follow-up | PASS | 3.3, 3.4, 6.1 |
| ImmediateStop | PASS | 7.1, 8.5 |
| SafetyState / EmergencyMarker | PASS | 4, 5 |
| Factory-new | PASS | 5.3, 19.5 |
| HistoryLoss / TotalDiscontinuity | PASS | 5.3, 13, 19.4a |
| Counter exhaustion | PASS | 5.2, 14, 19.5 |
| AirFallbackActive | PASS | 7.3, 18, 19 |
| STALE / FAILED | PASS | 7.3, 18 |
| Thermal 0..2 / Hard Emergency | PASS | 7.4, 19.6 |
| Fanpolicies | PASS | 3.2, 3.3, 7.2, 18 |
| Trust boundary | PASS | 2.3, 6.2, 18 |
| TargetReset / single FaultRevision | PASS | 2.3, 6.2, 22.1 |
| `FaultResetRejected` / TargetNotActive identity closure | PASS | 6.2, 13, 19.1, 19.6 |
| ServiceResetProof | PASS | 2.3, 6.2, 8.7 |
| FaultAcknowledged | PASS | 6.1, 13, 19.6 |
| SafetyTaskRecovery one-shot | PASS | 8.1, 8.2, 19.6 |
| TWDT / IWDT | PASS | 8.1, 15, 19.6 |
| ResetCause / RestartLoop | PASS | 8.1-8.3, 15, 19.5-19.6 |
| SAFE_BOOT | PASS | 8.4-8.6, 10, 19.5 |
| S3RecoveryDeparture | PASS | 8.7, 9.1 |
| typisierte #17 RecoveryEvidence | PASS | 4.4a, 8.7-9.2, 16 |
| S3RunRecovery crash-idempotence | PASS | 8.7-8.8, 19.2, 19.6 |
| Pending / Forward / Rejected / Terminal / Orphan | PASS | 4.4a, 8.8, 19.2 |
| #17 expliziter read-only Fallback-Load | PASS | 4.4a, 9.2, 16 |
| #18 sole recovery authority | PASS | 1, 9.3-9.4, 16 |
| kein committed `mutationKind`-Claim | PASS | 4.4a, 8.8, 16 |
| Y4 technical abandon ohne frühes Standby | PASS | 10.1-10.3 |
| `NoActiveRun` / `STANDBY` | PASS | 10, 16, 19.3-19.4 |
| `FaultResetCompleted` / `SafeBootExitCompleted` | PASS | 8.6, 10.1-10.2; NoActiveRun/Flag-Readback, RAM-only topology/decision/apply |
| terminale RAM-FSM-Events ohne zweiten #17-Write | PASS | 8.6, 9.4, 10.1-10.3, 19.2-19.3 |
| Y4-Tombstone ohne frühes Standby/Allowed | PASS | 10.1-10.3, 17, 19.3 |
| FaultAcknowledged exact MessageId->FaultReference | PASS | 6.1a, 13, 19.1, 19.6 |
| EmergencyMarker ZeroValid/ReadIndeterminate | PASS | 5.1, 5.3, 19.4a, 19.5 |
| Marker-Alternativwrite nur nach NOT-COMMITTED-Beweis | PASS | 5.1, 14.2, 19.4a |
| S3RunRecovery Exclusive-Writer-Gate | PASS | 8.4, 8.8, 19.2, 19.5; private single-use RecoveryWriteCapability |
| alle normalen #17-Write-Entrypoints bei Prepared blockiert | PASS | 8.4, 8.8, 19.2 |
| P1/O2 8er-Worst-Case und Mixed-Snapshot-Grenze | PASS | 6.1, 13, 19.6 |
| `oldLineageKnown` Continuous/Partial/Total konsistent | PASS | 4.2, 13, 19.4a, 19.6 |
| SafetyState-Decode getrennt von #17-Klassifikation | PASS | 4.4, 8.8, 19.5 |
| #56 / #57 Gate | PASS | 1, 5.3, 8.4-8.7, 12 |
| SafetyEvents / mixed snapshots | PASS | 13, 19.4a, 19.6 |
| Journal handoff an #19 | PASS | 13, 15, 19.6 |
| Resource / wear | PASS | 14, 19.4a, 19.5; RuntimeMessage Delta +256 B, fixed 6440 B, peak 12264 B |
| `ACCEPTANCE_TESTS.md` vollständig | PASS | 18, 19, 19.6 |
| ADR-013 | PASS | 1, 15, 16 |

Ein weiterer Widerspruch zwischen diesem Vertrag, dem echten #15-/#17-Code,
der FSM, den Safety-/Recovery-/Diagnostics-/#19-Quellen oder
`ACCEPTANCE_TESTS.md` ist vor Implementierung als Owner-Befund zu stoppen; es
wird kein stiller Parallelvertrag und kein automatischer Schema-4-Ausweg
eingeführt.

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
