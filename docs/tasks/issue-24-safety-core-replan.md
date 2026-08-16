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

Die Branchbasis ist direkt `origin/main`; weder PR #107 noch PR #108 noch ein
anderer früherer Issue-24-Plan ist eine normative Quelle. PR #107 und #108
bleiben unverändert offen und historisch. Der neue PR ändert keine
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
| #17 Load | `run_persistence_coordinator.cpp:257-497` | `Current`, `FallbackRecovered`, `NoActiveRun` und alle Indeterminate-Zustände werden explizit gemappt |
| #17 Activation | `run_persistence_coordinator.hpp:214-218`, `run_persistence_coordinator.cpp:760-925` | neuer Safety-Handoff darf nur vor der bestehenden forward-only Recoveryaktivierung RAM-seitig auswählen |
| #17 Write | `run_persistence_coordinator.cpp:1404-1685` | Prepared -> Slot -> Committed bleibt vorwärts und crash-sicher |
| #18 Recovery | `docs/RUN_PERSISTENCE.md`, `docs/RECOVERY_AND_INTERRUPTION.md`, `RunRecoveryCoordinator` | Zeit, Phase, Sensorwahl, Progress und Terminalentscheidung bleiben #18 |
| Prozess | `process_state_machine.hpp:15-150`, `process_state_machine.cpp:230-285` | Fault -> Standby erhält einen eigenen terminalen Owner-/Recoverygrund; kein `NoActiveRun + Fault` |
| #21 Sensorwahl | `sensor_selection_types.hpp`, `sensor_selection.cpp:537-759` | `AirFallbackActive` ist ein vorhandener Runtimezustand; #24 dupliziert keine #21-FSM |
| #22/#23 Aktor | `actuator_plan_types.hpp:12-35,286-307`, `temperature_control_orchestrator.hpp`, `actuator_planner.cpp:1040-1080` | zentrale SafetyDirective, kein frei gelieferter `Allowed`-Wert, #23 Timing bleibt Eigentümer |
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

### 3.2 Policylegende

Die Matrix verwendet folgende exakt definierte Kürzel:

```text
Gate: IS = ImmediateStop, U = Unresolved, A = Allowed
Fan:  FO = ForceOff, FI = ForceOn, PM = PlannerManaged
Auth: AUTO = bestehende Auto-/Operatorpolicy, SVC = Service
Clear: QE = kanonische Producer-Evidenz, Q21 = Issue-21-Evidenz,
       Q23 = Issue-23-/Hardware-Evidenz, QS = Safety-Storage-/Bootoracle,
       QR = RunPersistence-/Tombstone-Evidenz
Run:  KEEP = Zustand nur sicher weiterführen, TERM = Run terminalisieren,
      NONE = kein aktiver Run, NOAUTO = kein automatischer Resume
```

`FI` wird nur bei einer elektrisch nicht selbst betroffenen Ausgabe verwendet.
Bei einem elektrischen Fanfehler ist der jeweilige Fan immer `FO`; dadurch wird
kein Einschalten eines möglicherweise kurzgeschlossenen Ausgangs behauptet.
`PM` ist nur eine zulässige Planner-Projektion, wenn kein höherer Safetygrund
für diesen Fan besteht.

### 3.3 Vollständige erlaubte Identity-Matrix

Jede Tabellenzeile ist eine eigenständige begrenzte Ursache. Policy, Reset,
CauseClear, Runwirkung und Producerstatus sind damit vollständig bestimmt.

| Code | FaultSource | Klasse / Persistenz | AutoRearm | Gate; outer; inner; Peltier | Restart | Reset / CauseClear | Runpolicy | Producer |
|---|---|---|---|---|---|---|---|---|
| `0x0101` | `ProcessDeviation` | P1 / transient | ja | `A; PM; PM; aus aktueller Regelung` | nein | AUTO / QE | KEEP | real / injection |
| `0x0102` | `TimeSource` | P1 / transient | ja | `A; PM; PM; keine Safetywirkung` | nein | AUTO / QE | KEEP | real / injection |
| `0x0201` | `AirSensor` | O2 / transient | ja nach STALE-Requalifikation | `IS; FI; PM; AUS` | nein | AUTO / QE: Air VALID, CRC, Plausibilität | KEEP, danach normal neu qualifizieren | #20 real, injection |
| `0x0202` | `CoolingSensor` | O2 / transient | ja nach STALE-Requalifikation | `IS; FI; PM; AUS` | nein | AUTO / QE: Cooling VALID, CRC, Plausibilität | KEEP, danach normal neu qualifizieren | #20 real, injection |
| `0x0203` | `ProductSensor` | O2 / transient | #21-Policy | `IS; FI; PM; AUS` | nein | AUTO/Operator / Q21: #21 entscheidet Fallback oder Return | KEEP im validierten Air-Fallback, sonst #21 | #21 real, injection |
| `0x0204` | `SensorContradiction` | O2 / transient | nur nach eindeutiger Auflösung | `IS; FI; PM; AUS` | nein | AUTO / QE: #20/#21 widerspruchsfrei | NOAUTO bis #21/#20-Kontext gültig | #20/#21 real, injection |
| `0x0205` | `ThermalResponse` | O2 / transient | höchstens begrenzte Diagnosewiederholung | `IS; FI; PM; AUS` | nein | AUTO / QE: Sensor-/Aktor-/Zeitprüfung | KEEP nur nach neuer Evidenz | #22/#23 real, injection |
| `0x0206` | `NonCriticalPersistence` | O2 / transient | ja nach Storeprüfung | `A; PM; PM; keine kritische Wirkung` | nein | AUTO / QS: nichtkritischer Fehler behoben | KEEP | #19/#17 real, injection |
| `0x0301` | `AirSensor` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: Air stabil gültig | NOAUTO, Fault bleibt bis Reset | #20 real, injection |
| `0x0302` | `CoolingSensor` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: Cooling stabil gültig | NOAUTO, Fault bleibt bis Reset | #20 real, injection |
| `0x0303` | `SensorContradiction` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: Sicherheitswiderspruch aufgelöst | NOAUTO | #20/#21 real, injection |
| `0x0304` | `ThermalIntervention` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: Eingriffsgrenze verlassen, alle Checks | NOAUTO; Variant A ohne Puls | #22/#20 real, injection |
| `0x0305` | `ThermalHardEmergency` | S3 / persistent | nein | `IS; FI; FI; AUS` | nein | SVC / QE: Notgrenze-/Hardwareprüfung | NOAUTO; keine Gegenrichtung | #20/#22 real, injection |
| `0x0306` | `OuterFanFunctional` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / Q23: Funktion/thermische Evidenz | NOAUTO | #23 real, injection |
| `0x0307` | `OuterFanElectrical` | S3 / persistent | nein | `IS; FO; PM; AUS` | nein | SVC / Q23: Ausgang elektrisch sicher | NOAUTO | injection-only ohne Producer |
| `0x0308` | `InnerFanFunctional` | S3 / persistent | nein | `IS; FI; FO; AUS` | nein | SVC / Q23: Funktion/thermische Evidenz | NOAUTO | #23 real, injection |
| `0x0309` | `InnerFanElectrical` | S3 / persistent | nein | `IS; FI; FO; AUS` | nein | SVC / Q23: Ausgang elektrisch sicher | NOAUTO | injection-only ohne Producer |
| `0x030A` | `HBridgeConflict` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / Q23: keine Doppelrichtung, Diagnose gesund | NOAUTO | #23 real, injection |
| `0x030B` | `UnexpectedActuatorOutput` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / Q23: Ausgangszustand erklärt | NOAUTO | injection-only ohne Hardwareproducer |
| `0x030C` | `PeltierUnsafeOutput` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / Q23: Strom-/Ausgangsevidenz, falls vorhanden | NOAUTO | injection-only/Commissioning |
| `0x030D` | `ActuatorWatchdog` | S3 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / Q23: Watchdogepisode beendet, Ausgang geprüft | NOAUTO | #23 real, injection |
| `0x0401` | `RunNotReconstructible` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QR: Tombstone durable | TERM | #17 real, injection |
| `0x0402` | `RunOrphanedState` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QR: Tombstone durable | TERM | #17 real, injection |
| `0x0403` | `RunPreparedInterrupted` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QR: Tombstone durable | TERM | #17 real, injection |
| `0x0404` | `RunReadFailed` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QR: Store wieder les-/schreibbar und Tombstone | TERM | #17 real, injection |
| `0x0405` | `RunStoreIntegrity` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QR: Schema/Epoch/Kapazität geklärt, Tombstone | TERM | #17 real, injection |
| `0x0406` | `ConfigurationUnavailable` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: #57 `RuntimeReady` | NOAUTO | #57 real, injection |
| `0x0407` | `ConfigurationIntegrity` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: #57 Integrity-/Schemaursache behoben | NOAUTO | #57 real, injection |
| `0x0408` | `ConfigurationCommitIndeterminate` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QE: #56/#57 Indeterminate aufgelöst | NOAUTO | #56/#57 real, injection |
| `0x0409` | `SafetyStateStore` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QS: SafetyState redundant gesund | NOAUTO | #24 real, injection |
| `0x040A` | `RestartLoop` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QR: 30 Minuten stabil, danach SVC | NOAUTO | #24 real, injection |
| `0x040B` | `UnknownSafetyState` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QS: History-Loss-Reinitialisierung und Storeproof | NOAUTO/TERM | #24 real, injection |
| `0x040C` | `SafetyCounterExhausted` | Y4 / persistent evidence | nein | `IS; FI; PM; AUS` | nein | SVC / QS: nicht automatisch heilbar; nur neue freigegebene Generation | NOAUTO | #24 real, injection |
| `0x040D` | `InternalSafety` | Y4 / persistent | nein | `IS; FI; PM; AUS` | nein | SVC / QS: konkrete interne Invariante nachgewiesen | NOAUTO | #24 real, injection |

Die 26 S3/Y4-Identities der Matrix sind `kMaxPersistentFaultRecords = 26`
(13 S3 und 13 Y4).
P1/O2 können nicht in diesen Bestand gelangen. Ein Y4-Record ist bis zum
Target-Reset ein Safety-Latch; ein CauseClear markiert ihn nur als
`causeCleared`, entfernt ihn aber nicht.

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
ist `33 Byte Header + 656 Byte Payload + 4 Byte CRC = 693 Byte`, also deutlich
unter dem 1024-Byte-Limit.

### 4.2 Payload-Feldreihenfolge

Alle Felder sind Big-Endian ohne C++-Padding. Der Payload ist immer exakt
`kSafetyStatePayloadBytes = 656` Byte:

| Offset | Wirefeld | Typ / Wert |
|---:|---|---|
| 0 | `formatTag` | `uint16 = 0x5346` (`SF`) |
| 2 | `safetyHistoryEpoch` | `uint32`, Start 1, checked erhöht bei History-Loss-Reinit |
| 6 | `nextPersistentInstanceId` | `uint32`, erste vergebene ID 1; kein Wrap |
| 10 | `persistentFaultRevision` | `uint32`, Start 0; kein Wrap |
| 14 | `bootSequence` | `uint32`, erster erfolgreicher Erstboot 1; kein Wrap |
| 18 | `abnormalRestartCount` | `uint8`, nur 0..3 |
| 19 | `safeBootRequired` | `uint8`, `0=false`, `1=true` |
| 20 | `restartIntentState` | `0=None`, `1=Prepared` |
| 21 | `restartIntentKind` | `0=None`, `1=S3Recovery` |
| 22 | `restartIntentSourceInstanceId` | `uint32`, bei None 0 |
| 26 | `restartAttempted` | `uint8`, 0/1 |
| 27 | `restartOutcome` | `0=None`, `1=Rejected` |
| 28 | `runRecoveryForbidden` | `uint8`, 0/1 |
| 29 | `historyLoss` | `uint8`, 0/1; sichtbare rebootfeste History-Loss-Evidenz |
| 30 | `persistentFaultCount` | `uint8`, 0..26 |
| 31 | `reserved0` | `uint8 = 0` |
| 32 | `FaultRecord[0..25]` | exakt 26 Records à 24 Byte |

`recordRevision` ist ausschließlich Envelope `VersionValue`; er wird nicht
doppelt im Payload gespeichert. Leere FaultRecord-Slots sind vollständig null.
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
- `primaryInstanceId == 0` oder `< instanceId`;
- jeder Code/Source entspricht genau einer Catalog-Identity und persistent ist;
- keine doppelte FaultIdentity und keine doppelte InstanceId existiert;
- `nextPersistentInstanceId` ist nie 0, außer der Record ist ein ausdrücklich
  erkanntes fabrikneues Rohformat vor Erstinitialisierung;
- `restartIntentState=None` erzwingt `kind=None`, `source=0`,
  `restartAttempted=0`, `outcome=None`;
- `restartIntentState=Prepared` erzwingt `kind=S3Recovery`,
  `0 < source < nextPersistentInstanceId`, `restartAttempted` 0/1 und
  `outcome` None oder Rejected;
- `runRecoveryForbidden=true` zusammen mit Prepared `S3Recovery` ist ungültig;
- ein RestartIntent ist nur ohne aktiven S3/Y4-Latch gültig;
- `abnormalRestartCount <= 3`, `safeBootRequired` und `historyLoss` sind 0/1;
- `safetyHistoryEpoch > 0`.

Jede Verletzung ist Decode-Failure und führt zu `Y4_UNKNOWN_SAFETY_STATE`,
`ImmediateStop` und `SAFE_BOOT`; sie wird nicht durch eine beliebige
Interpretation repariert.

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

`Healthy`, `MarkerRequired` und `BlockedIndeterminate` sind Runtime-/Scanstatus
und werden nie als persistierte SafetyState-Wahrheit geschrieben.

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
Payload            = exakt 22 Byte; Envelope maximal 59 Byte
```

Payloadreihenfolge:

```text
markerRevision                  uint32 (Envelope VersionValue, ab 1)
state                           uint8  1=Active, 2=Cleared
reason                          uint8  1=SafetyCommit, 2=MarkerRepair,
                                        3=CounterExhausted, 4=HistoryLoss,
                                        5=UnknownSafetyState
bootSequence                    uint32
monotonicMillis                 uint64
attemptedSafetyRecordRevision  uint32
```

Unbekannte Werte, Reserved-Bits oder widersprüchliche Kombinationen sind
ungültig. Bei einem normalen SafetyState-Commitfehler lautet die Reihenfolge:
RAM `ImmediateStop`, ein Active-Marker-Write auf den bevorzugten nicht
gewinnenden Marker-Slot, Readback, und bei fehlender Bestätigung genau ein
bounded Versuch auf dem anderen Slot. Danach kein Write-Loop.

Der Bootscan beider Marker gewinnt die höchste semantisch gültige Revision.
Active hält fail-closed. Cleared darf Active nur überstimmen, wenn die
Markerhistorie selbst redundant gesund und die Cleared-Bytes exakt bestätigt
sind. Ein defekter Peer wird repariert, aber ein Y4 bleibt bis CauseClear plus
Service-Reset bestehen. Marker-Recovery repariert Persistenz, ist weder
FaultReset noch SAFE_BOOT-Exit.

### 5.2 Erschöpfte High-Watermarks

Die folgenden Zustände sind selbst rebooterkennbare fail-closed Evidenz:

| Zähler | Exhaustion | Verhalten |
|---|---|---|
| `persistentFaultRevision` | `UINT32_MAX` | keine weitere Fault-Lifecycle-Mutation; RAM `ImmediateStop`; Runtime `SafetyCounterExhausted`; SAFE_BOOT; Marker nur best effort |
| `nextPersistentInstanceId` | `UINT32_MAX` vor neuer Vergabe | keine neue persistente InstanceId und kein neuer Record; gleicher fail-closed Status |
| SafetyState `recordRevision` | `UINT64_MAX` | kein weiterer SafetyState-Commit; alter Winner bleibt Wahrheit; SAFE_BOOT |
| `bootSequence` | `UINT32_MAX` vor `+1` | Boot kann nicht normal fortfahren; ImmediateStop, SAFE_BOOT, Marker best effort |
| Marker `markerRevision` | `UINT32_MAX` | kein neuer Markerwert darstellbar; RAM-/Bootscan-Evidenz bleibt fail-closed, kein Erfolgsclaim |

Bei Exhaustion wird keine nicht darstellbare Y4-Mutation behauptet und kein
`safeBootRequired=true` versprochen, wenn gerade dieser Commit unmöglich ist.
Die Maximalzahl beziehungsweise der Maximalwert selbst ist die persistente
Evidenz. SAFE_BOOT-Exit ist bei `SafetyCounterExhausted` verboten.

### 5.3 Factory-new und Safety-History-Loss

`FactoryNoveltyProof` bleibt vollständig intern in
`ConfigurationRecoveryService::boot()`; #24 konstruiert oder konsumiert ihn
nicht. #24 liest vor jeder Freigabe selbst `sf0`, `sf1`, `sem0`, `sem1` und die
#17-RunPersistence-Evidenz. #57 wird über das öffentliche
`ConfigurationRecoveryStatus`-Ergebnis konsumiert.

Eine leere Safety-Erstinitialisierung ist ausschließlich gültig, wenn:

1. alle vier Safety-/Marker-Keys vorher `NotFound` waren;
2. #17 keine RunPersistence-Historie besitzt (`NoPersistedRun`/keine Slots);
3. derselbe Boot `FactoryInitializationCompleted` von #57 bestätigt;
4. keine widersprüchliche Store-Evidenz vorliegt.

Dann wird ein konservativer SafetyState mit `safetyHistoryEpoch=1`,
`nextPersistentInstanceId=1`, `persistentFaultRevision=0`,
`bootSequence=1`, keinem Fault, keinem Intent und `safeBootRequired=false`
gebildet, redundant geschrieben/readback-verifiziert und in diesem Boot nicht
noch einmal auf 2 erhöht.

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
 -> safetyHistoryEpoch checked erhöhen
 -> konservativer SafetyState mit historyLoss=true und
    Y4_UNKNOWN_SAFETY_STATE
 -> beide Safety-Slots und Marker redundant readbacken
 -> History-Loss/Y4 bleibt bis CauseClear + Service-Target-Reset
 -> separater SAFE_BOOT-Exit
```

Programme und Configuration werden nicht gelöscht. Ein History-Loss erhöht die
`safetyHistoryEpoch`; alte InstanceIds werden nie in derselben History-Epoche
wiederverwendet. Der neue `Y4_UNKNOWN_SAFETY_STATE`-Record erhält dabei
`instanceId=1`, `persistentFaultRevision=1` und
`nextPersistentInstanceId=2`; alle weiteren Vergaben bleiben checked. Bei
Epoch- oder Counter-Exhaustion bleibt die Reinitialisierung fail-closed.

## 6. Fault-Lifecycle, Ack und Multi-Fault-Reset

### 6.1 Raise, CauseClear, Relapse, Reset

Bei einer neuen persistenten Identity werden `instanceId`,
`causeCleared=false`, Record und `persistentFaultRevision++` in einem
SafetyState-Kandidaten gebildet und committed. Eine bereits aktive Identity
erhält weder einen zweiten Record noch eine neue InstanceId. Bei einem Raise ist
die RAM-Directive bereits vor dem Write `ImmediateStop`.

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

`FaultResetEvaluation` erhält Target-Identität, `targetResetAllowed`,
`otherBlockingFaultActive` und `releaseAllowed`. Ein Target darf entfernt
werden, wenn es existiert, CauseClear hat, die Target-Safetychecks bestanden
sind, die Service-Autorisierung echt ist, die erwartete FaultRevision aktuell
ist und die Confirmation stimmt. Andere Blocker verhindern Release, Recovery
und Standby, aber nicht den qualifizierten Target-Reset. Erst nach dem letzten
Blocker und erfüllter Y4-Terminalregel kann ein Gate auf `Allowed` oder ein
SAFE_BOOT-Exit entstehen.

Der Produktpfad darf kein öffentlich konstruierbares
`authorizationSatisfied=true` als Proof akzeptieren. Bis ein produktiver
Service-PIN/Auth-Producer existiert, sind S3/Y4-Resets produktiv fail-closed;
Tests erhalten ausschließlich einen Test-Support-Proof, der nicht aus der
Produktionskommandoschicht importiert wird. Ein zukünftiger Proof bindet
Auth-Session/Generation, Target-Instance und erwartete FaultRevision.

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

### 7.4 Thermische Intervention

Für Release 1 gilt eindeutig Variante A: `S3_THERMAL_INTERVENTION` schaltet
normale Peltierpfade aus, sperrt die auslösende Richtung, verwirft Accumulator
und Integrator und führt die exakt im Catalog festgelegte Fanreaktion aus.
Es gibt in #24 keinen automatischen Gegenrichtungsversuch. Die firmwarefeste
Obergrenze ist daher aktuell null; fehlende Commissioningparameter wirken
fail-closed AUS. Eine spätere Gegenrichtung ist ein eigenes Owner-freigegebenes
Commissioning-/Hardwareissue und wird nicht vorbereitet.

`S3_THERMAL_HARD_EMERGENCY` schaltet beide Richtungen aus und versucht niemals
eine Gegenrichtung. Keine direkte H-Brückensteuerung entsteht.

## 8. RestartSupervisor und SAFE_BOOT

### 8.1 ResetCause und RestartIntent

`ResetCause` wird als geschlossene `uint8`-Tabelle geführt:

```text
1 PowerOn
2 SoftwareRestart
3 WatchdogOrPanic
4 Brownout
5 External
6 Unknown
```

Vor dem kontrollierten Restart wird zuerst ein SafetyState-Kandidat mit
`RestartIntentState=Prepared`, `RestartIntentKind=S3Recovery`, historischer
`sourceInstanceId`, `restartAttempted=0` durable committed. RAM bleibt Fault.
Danach wird `restartAttempted=true` in einem zweiten SafetyState-Kandidaten
durable committed, bevor der Reset-Port aufgerufen wird. Der Reset-Port wird
genau einmal aufgerufen. Keinen `Requested`-Write nach dem Side Effect
voraussetzen; ESP-IDF kann vor dem physischen Reset nicht zuverlässig
zurückkehren. Kehrt der Port zurück oder lehnt ab, wird `outcome=Rejected`
durable committed. Kein Auto-Retry.

### 8.2 ResetCause-Kompatibilität

| Prepared S3Recovery | ResetCause | abnormal | Handoff |
|---|---|---:|---:|
| ja, Attempted beliebig | SoftwareRestart | nein | ja, nach vollständiger Bootqualifikation |
| ja, Attempted | External | nein | ja, nach vollständiger Bootqualifikation |
| ja, Attempted | PowerOn | nein | ja, nach vollständiger Bootqualifikation |
| ja, Attempted | Brownout | ja | nein; SAFE_BOOT/kein Auto-Handoff |
| ja, Attempted | WatchdogOrPanic | ja | nein; SAFE_BOOT/kein Auto-Handoff |
| ja, Attempted | Unknown | ja | nein; SAFE_BOOT/kein Auto-Handoff |
| nein/kein gültiger Intent | SoftwareRestart | ja | nein |
| nein/kein gültiger Intent | PowerOn/External | nein | kein S3-Handoff |

Bei inkompatibler Cause bleibt der Intent als diagnostische Evidenz erhalten,
aber es wird kein automatischer zweiter Restart und kein Fallback-Handoff
ausgeführt. Ein späterer manueller Reboot kann einen validen, bereits
attempteten Intent gemäß Tabelle übernehmen.

### 8.3 Restartzähler und Stabilität

`abnormalRestartCount` ist saturierend 0..3. Der Übergang auf 3 erzeugt,
solange noch darstellbar, `Y4_RESTART_LOOP` und `safeBootRequired=true` in
demselben SafetyState-Kandidaten. Bei Count 3 gilt:

```text
30 Minuten aktuelle monotone Bootzeit
+ kein neuer abnormaler Reset
+ SafetyState/Marker redundant healthy
+ kein aktiver S3/Y4-Latch
+ kein Prepared Intent
-> nur CauseClear von RestartLoop; Count bleibt 3
```

Ein Service-Target-Reset von `Y4_RESTART_LOOP` setzt den Count auf 0; ein
Restart allein nicht. `safeBootRequired` bleibt bis zum separaten Exit.
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
10 runRecoveryForbidden und S3RecoveryIntent qualifizieren
11 SAFE_BOOT-Entscheidung treffen
12 nur normal + gültiger S3Intent: RAM-only Fallback-Handoff -> #18
13 erst nach vollständiger Qualifikation normaler Orchestrator/Planner/Sink-Tick
```

Kein Planner-/Sink-Tick darf vor Schritt 13 einen `Allowed`-Pfad erhalten.

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
fail-closed behandelt.

## 9. S3-Recovery ohne Head-Rollback

### 9.1 Eintrittt, CauseClear und Restart

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
ausgeführt.

### 9.2 RAM-only Fallback-Handoff

Für den Recovery-Boot wird eine schmale API mit dem Zweck
`prepareSafetyFallbackRecovery(...)` geplant. Sie darf nur:

- den normalen #17-Head und seinen Fault-Current verifizieren;
- die bestehende Fallback-Referenz, Slot, Schema, Epoch, Revision, CRC und
  Variant prüfen;
- Current und Fallback an `runId`/Programmsnapshot/Manual-Runidentität binden;
- `runRecoveryForbidden=false`, gültigen S3Intent, keine S3/Y4-Records,
  qualifizierte Config und gültige Sensorgrundlage prüfen;
- den bereits geladenen Fallback in den bestehenden RAM-Zustand einsetzen;
- `persistedIds_`, `persistedIdCount_`, High-Watermark-/Cachewerte und
  `FallbackRecoveryPending` vollständig herstellen;
- den Snapshot an die bestehende #18-Activation übergeben.

Sie darf keinen Head, Slot, Checkpointpayload oder Revision schreiben, keine
Fallback-Referenz persistent vertauschen und keine Zeit-/Progress-/Phasen-
Recovery entscheiden. Zwischen Handoff und #18 Hop 1 gibt es keinen
Temperature-Control-Tick, Planner-Tick, Sink-Write oder normales Run-Command.

`loadAndInitialize()` bleibt die physische Verifikation. Die aktuelle
Implementierung zeigt in `run_persistence_coordinator.cpp:446-485`, dass ein
valider referenzierter Fallback in `slots_[]` geladen und
`FallbackRecoveryPending` gesetzt wird, ohne einen Store-Write. Das neue
Safety-Handoff schränkt diesen vorhandenen RAM-Pfad um Intent-, Fault- und
Runidentitätsprüfungen ein. Der bestehende
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
   +--> active run and !runRecoveryForbidden -> Prepared/Attempted Intent
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

Der aktuelle Prozessvertrag erhält dafür den expliziten Event
`ProcessEvent::FaultResetCompleted` und
`TransitionReason::FaultResetCompleted` für den Wechsel `Fault -> Standby`;
`RunAbandonCompleted`/`TransitionReason::RunAbandonCompleted` ist davon
getrennt und darf nur die terminale Persistenzanwendung markieren. Beide
Transitiongründe werden vor Anwendung als Kandidat validiert und durch #17
Write-before-Apply geschützt.

### 10.2 Terminalpunkt

Für einen normalen Y4-Abandon ist die Reihenfolge:

```text
1 SafetyGate ImmediateStop und Safety-Latch
2 autorisierte technische Run-Abandon-Evaluation
3 Fault -> Standby-Kandidat über RunAbandonCompleted
4 activeRun vollständig löschen
5 kanonisches NoActiveRun/STANDBY-Snapshot erzeugen
6 #17 Prepared -> Slot -> Committed, exact CAS/readback
7 erst nach bestätigtem Tombstone runRecoveryForbidden=false schreiben
8 danach separater SAFE_BOOT-Exit, falls kein anderer Blocker besteht
```

Ein Crash vor Schritt 6 lässt `runRecoveryForbidden=true`; ein Crash zwischen
6 und 7 darf nur Flag-Reparatur nach bestätigtem `NoActiveRun` ausführen. Ein
Crash nach 7 findet die persistente Run-Wahrheit vor. `NoActiveRun` wird nie mit
Fault oder SafeBoot als Prozesssnapshot geschrieben.

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
| `Current` | keine Fault, aber Safety-/Latch-/Intentprüfung vor Activation | bei persistentem S3/Y4 ja / normal #18 nur ohne Latch |
| `NoActiveRun` | keine Fault; kanonische Standby-Wahrheit | nein / NONE |
| `FallbackRecovered` | keine Fault; nur Safety-Handoff und danach #18 | nur bei sonstigem Grund / Fallback pending |
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
| `CheckpointWritten` | periodischer Checkpoint durable | Erfolg, kein SafetyEvent |
| `AlreadyProcessed` | RAM-idempotent, keine Mutation | kein Fault |
| `AlreadyPersisted` | durable-idempotent, keine Mutation | kein Fault |
| `NotEligible` | optionaler Vorgang nicht fällig/zugelassen | kein Fault; required = `Y4_INTERNAL_SAFETY` |
| `NotAllowedInState` | fachlich unzulässig | optional kein Fault; required = `Y4_INTERNAL_SAFETY` |
| `NotInitialized` | Coordinator-/Bootreihenfolge verletzt | `Y4_INTERNAL_SAFETY`, SAFE_BOOT |
| `RecoveryPending` | kein Apply, Recovery noch nicht qualifiziert | SAFE_BOOT/kein Aktor |
| `Busy` | nur mit vorab definiertem bounded Retry außerhalb Aktorpfad | required ohne bestätigten Retry = `Y4_INTERNAL_SAFETY` |
| `InvalidDecision` | Kandidat/Command ungültig | required = fail-closed `Y4_INTERNAL_SAFETY` |
| `StaleDecision` | Revisionskonflikt | required = fail-closed `Y4_INTERNAL_SAFETY` |
| `TimeMismatch` | Zeit-/Checkpointkontext widerspricht | required = fail-closed `Y4_INTERNAL_SAFETY` |
| `TimeWentBackwards` | monotone Zeitverletzung | `Y4_INTERNAL_SAFETY`, SAFE_BOOT |
| `CounterOverflow` | betroffener #17-Zähler erschöpft | `Y4_COUNTER_EXHAUSTED`, SAFE_BOOT |
| `WriteFailed` | sicher nicht geschrieben | `Y4_RUN_STORE_INTEGRITY`, EmergencyMarker, SAFE_BOOT |
| `CapacityExceeded` | Store-/Payloadlimit | `Y4_RUN_STORE_INTEGRITY`, SAFE_BOOT |
| `PersistenceIndeterminate` | Ausgang nicht auflösbar | `Y4_RUN_STORE_INTEGRITY`, SAFE_BOOT |
| `PersistenceCommittedApplyFailed` | durable Wahrheit, RAM-Anwendung verletzt | `Y4_INTERNAL_SAFETY`, SAFE_BOOT; nicht rollbacken |
| `Blocked` | Coordinator blockiert | `Y4_INTERNAL_SAFETY`, SAFE_BOOT |
| `NotDue` | periodischer Checkpoint nicht fällig | kein Fault |
| `NoActiveRun` | kein Run für optionale Operation | kein Fault; required Tombstone-Kontext ist Invarianzfehler |

`InvalidDecision`, `NotInitialized`, `NotAllowedInState`, `StaleDecision`,
`TimeMismatch`, `Blocked` und `Busy` werden im required Safety-/Tombstonepfad
niemals als harmlose Ablehnung behandelt. `Busy` hat nur den expliziten
bounded Retryvertrag für einen rein technischen, nicht aktorwirksamen Scan;
ein unbestimmtes Write-Ergebnis wird nicht erneut blind geschrieben.

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
| `CounterOverflow` | `Y4_COUNTER_EXHAUSTED`, SAFE_BOOT |
| `ConfigurationMutationBusy`, `ConfigurationModelBudgetBusy`, `StateTransitionRejected` | bei nicht operationalem Boot `Y4_CONFIGURATION_UNAVAILABLE`; bei sicher weiter gültiger alter Runtime kein neuer Producer, aber kein Gate-Bypass |

Der in `ConfigurationRecoveryService::boot()` erzeugte
`FactoryNoveltyProof` bleibt nicht kopierbar und wird nicht an #24
herausgegeben. #24 nutzt `FactoryInitializationCompleted` nur gemeinsam mit
seinem eigenen `NotFound`-Scan und RunPersistence-Evidenz.

## 13. SafetyEvents und #19-Handoff

Issue #24 produziert typisierte, flüchtige `SafetyEventBatch`-Werte; #19 ist
Eigentümer der Langzeithistorie und der Journalpersistenz. Es gibt keine
unbounded Queue und keine stille Trunkierung.

Variante B spart Event-RAM: ein Batch trägt einmal
`bootSequence:uint32` und `occurredAtMonotonicMillis:uint64`; alle Events einer
atomaren Mutation teilen diesen Zeitanker. Ein Event ist exakt 8 Byte:

```text
eventKind:uint8, faultCode:uint16, faultSource:uint8, instanceId:uint32
```

Die feste Menge ist `kMaxSafetyEventsPerMutation = 6` und ergibt
`13 + 6*8 = 61 Byte` pro Batch. Die maximalen Mutationen werden explizit
enumeriert:

```text
Raise: FaultRaised, GateChanged, TerminalRequiredSet = 3
CauseClear: CauseCleared, GateChanged = 2
Relapse: FaultRelapsed, GateChanged = 2
TargetReset: FaultReset, GateChanged, TerminalRequiredChanged = 3
Restart: IntentPrepared, RestartAttempted, RestartRejected = 3
Boot: BootClassified, SafeBootEntered, RedundancyRepair = 3
HistoryLoss: HistoryLossDetected, SafetyReinitialized, SafeBootEntered = 3
```

`static_assert(maxEvents == 6)` ist Pflicht. Ein Überschreiten ist ein
`Y4_INTERNAL_SAFETY`-/fail-closed-Fehler; es werden keine ersten sechs Events
still abgeschnitten.

## 14. Ressourcen-, Flash- und Wear-Proof

### 14.1 Statische Größen

```text
SafetyState payload: 32 + (26 * 24) = 656 Byte
SafetyState max Envelope: 33 + 656 + 4 = 693 Byte <= 1024
EmergencyMarker payload: 22 Byte
EmergencyMarker max Envelope: 33 + 22 + 4 = 59 Byte <= 64
SafetyEventBatch: 61 Byte
```

Die Implementierung verwendet keine dynamische Allokation im Safety-Hot-Path.
Mindestens zwei 1024-Byte-Safety-Encode/Readback-Puffer, zwei 64-Byte-Marker-
Puffer, 26 persistente Runtime-Records, ein transienter P1/O2-Bestand mit
Cataloglimit 6 und ein 61-Byte-EventBatch werden als feste Grenzen im
Ressourcenbericht ausgewiesen. Die Safety-FaultCore-RAM-Grenze ist damit
`2*1024 + 2*64 + 26*32 + 6*24 + 61 = 3.901 Byte` zuzüglich statischer
Serviceobjekte; der Build muss diesen Wert und Stack-Peaks berichten.

### 14.2 Schreib- und Reparaturgrenzen

| Mutation | normale Writes | Readback / Repair |
|---|---:|---|
| Fault Raise/CauseClear/Relapse/TargetReset | 1 SafetyState | Zielslot readback; bei Fehler max. 2 Marker-Slotversuche |
| Restart Prepared | 1 SafetyState | exakt bestätigt |
| Restart Attempted | 1 SafetyState | exakt bestätigt vor Reset-Side-Effect |
| Restart Rejected | 1 SafetyState, nur wenn Port zurückkehrt | exakt bestätigt |
| Bootsequence/Countermutation | 1 SafetyState | exakt bestätigt; bei Exhaustion kein Writeclaim |
| normale Redundanzreparatur | höchstens 1 Write pro defektem Peer | exakt bestätigt, kein Loop |
| Marker Active/Cleared | 1 bevorzugter Slot, bei Nichtbestätigung höchstens 1 anderer Slot | exakt bestätigt |
| Ack/Mute | 0 | RAM-/Message-only |
| S3 Fallback-Auswahl | 0 | nur bestehende #17-Reads |
| #18 Recovery/Tombstone | bestehender #17 Forward-Path | dessen eigener Prepared/Slot/Committed-/CAS-Vertrag |

Damit wird weder ein unendlicher Flash-Write-Loop noch ein persistenter Ack-
Wearpfad eingeführt. Jede SafetyState-Mutation erhöht genau eine
`recordRevision`; Fault-Lifecycle-Mutationen erhöhen zusätzlich genau eine
`persistentFaultRevision`, sofern nicht Exhaustion vorliegt.

## 15. Modulbesitz nach ADR-013

```text
fermentation_app:
  SafetyFaultService, FaultCatalog, SafetyState codec/store wrapper,
  EmergencyMarker, RestartSupervisor, Boot-/Process-/Recovery-Orchestration,
  SafetyEventBatch und #24 Producer-Mapping

device_platform:
  IStateStore, StorageEnvelope, SlotScanner, Time-Port, generische Reset-Port-
  Schnittstelle nur wenn anwendungsneutral

device_platform_esp_idf:
  ESP-IDF Reset-/Time-/Store-Adapter

device_platform_test_support:
  deterministische Store-, Reset-, Zeit- und Producer-Injections
```

`device_platform` erhält keine fermentation-spezifischen Fault-, SafetyState-
oder Run-Typen. `src/main.cpp` bleibt Composition Root.

## 16. Kern-Proof: #17/#18 wird nicht verbogen

1. **#17 bleibt forward-only:** `writeSnapshotCore()` in
   `run_persistence_coordinator.cpp:1404-1685` bildet Prepared, schreibt den
   Slot und committed danach den Head; `RunPersistenceHead` und
   `RunCheckpointReference` sind in `run_persistence_contract.hpp:46-86`
   unverändert forward-only.
2. **Kein Head-Rollback:** Bei S3 liest `loadAndInitialize()` in
   `run_persistence_coordinator.cpp:446-485` den referenzierten Fallback nur in
   `slots_[]` und setzt den Coordinator auf `FallbackRecoveryPending`; dieser
   Schritt enthält keinen Store-Write. Das neue Safety-API schreibt ebenfalls
   nichts.
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
RAM-only Auswahl, #18-Autorität, kein #17-Schemawechsel, forward-only
Persistenz und persistente Y4-Terminalität.

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

| Injection | Klassifikation | Erwartung |
|---|---|---|
| Air-Sensor CRC/Bus/Read | bestehender #20 Producer konsumiert + injection-only | STALE transient, FAILED S3; kein erfundener Hardwarestatus |
| Cooling-Sensor CRC/Bus/Read | #20 + injection-only | wie Air; Peltier bleibt aus |
| Product-Sensor entfernt/CRC/Stale/Failed | bestehender #21 Vertrag + injection-only | AirFallbackActive respektieren, Return nur requalifiziert |
| Missing/out-of-range/stuck/repeated sensor | #20/#21 konsumiert | exakte Sensorphase; kein stilles Fallback |
| Sensor contradiction | #20/#21 Producer konsumiert | O2 bis eindeutig, S3 bei ungelöst sicherheitskritisch |
| veraltete Regelanforderung | #23 bestehender Planner-Test | stale request abweisen, keine Aktorfreigabe |
| kurze Gegenanforderung | #23 bestehender Vertrag | Mindestzeit/Accumulator korrekt, keine direkte Safety-Recovery |
| dauerhafte Gegenanforderung | #23 + injection-only | Watchdog/Timing, kein Umgehen der Gate-Directive |
| Mindest-Ausschaltzeit | #23 wiederverwenden | kein früher Restart |
| Totzeit | #23 wiederverwenden | keine direkte Richtungsumschaltung |
| Abbruch Servicepuls | #23/Serviceboundary konsumiert | sofortige sichere Abschaltung |
| Peltier-Test ohne Temperatursicherung | Hardware/Commissioning BLOCKED | Serviceoperation blockiert |
| Aktortest aus SAFE_BOOT | #24 implementiert | immer blockiert |
| Safety-Intervention | #24 implementiert, Variant A | Peltier aus, 0 Gegenrichtungsversuche |
| Hard Emergency | #24 implementiert | beide Richtungen aus, keine Gegenrichtung |
| invalid runtime/config size | #56/#57 konsumiert | Y4 Config/SAFE_BOOT |
| ConfigurationRuntimeFailure | bestehender #56 Producer konsumiert | Y4_CONFIG_UNAVAILABLE |
| ConfigurationUnavailable | bestehender #57 Producer konsumiert | Y4_CONFIG_UNAVAILABLE |
| ConfigurationIntegrityFailure | bestehender #57 Producer konsumiert | Y4_CONFIG_INTEGRITY |
| CommitOutcomeUnknown/Indeterminate | #56/#57 konsumiert | Y4_CONFIG_COMMIT_INDETERMINATE |
| simultaneous H-bridge directions | #23 real + injection | S3_HBRIDGE_CONFLICT |
| invalid/unknown ActuatorPlan | #23/injection-only | ImmediateStop, keine physische Behauptung |
| planner watchdog | #23 real, #24 Mapping | S3/Y4 gemäß Catalog, `forceStop()` |
| implausible actuator feedback | injection-only | S3_PELTIER_UNSAFE_OUTPUT, keine erfundene Hardware |
| fan failure/off during Peltier | injection-only ohne Tacho | Functional/Electrical Fan-Identity exakt trennen |
| Peltier/H-bridge fault | #23/injection-only | S3/Y4 je Catalog |
| missing thermal response | #22/#23 + injection-only | O2 Diagnose, bei Eskalation S3 |
| interrupted SafetyState write | #24 implemented | ImmediateStop, Marker, SAFE_BOOT |
| interrupted Marker write | #24 implemented | RAM fail-closed, kein Retryloop |
| interrupted RunPersistence write | #17 consumed | Y4 RunStore, SAFE_BOOT, Abandon |
| repeated watchdog reset | #24 RestartSupervisor | Count 3 -> Y4_RESTART_LOOP/SAFE_BOOT |
| Brownout/Unknown reset | reset injection | abnormal, kein S3-Handoff |
| invalid persisted enum | #24 codec | Decode fail, Y4_UNKNOWN_SAFETY_STATE |
| impossible persisted state | #24 crossfield validator | Decode fail, SAFE_BOOT |
| interrupted multi-object/config write | #56/#57 consumed | Config Y4, keine Runtimefreigabe |
| PreparedInterrupted | #17 load status consumed | Y4 RunPrepared, Abandon |
| corrupt current + valid fallback | #17 existing path + #24 handoff | RAM-only FallbackRecoveryPending, #18 entscheidet |
| corrupt fallback | #17 existing path | kein Resume, Y4 RunStore/terminal |
| one-slot SafetyState corruption | #24 repair | Winner konservativ laden, Repair/Y4 bleibt |
| total SafetyState loss | #24 History-Loss-Pfad | SAFE_BOOT, Service-Reinit mit historyLoss |
| Active/corrupt Marker | #24 marker scan | Active/Corrupt hält fail-closed |
| full Fault capacity | #24 catalog bound | keine Eviction, Y4/counterstatus |
| counter overflow | #24 checked counters | keine nicht darstellbare Mutation, SAFE_BOOT |
| latest run checkpoint corruption + fallback | #17 + #24 | Fallback nur referenziert/validiert, kein Head-Rollback |
| latest configuration revision damaged | #57 consumed | alte Runtime nur nach #57-Status; sonst Y4 |
| Recovery write success but readback fails | #17/#24 required path | nicht bestätigt -> fail-closed |
| Latch reset before storage check | #24 | Reject |
| Latch reset with open Marker | #24 | Reject |
| History store full | #19 consumed, #24 criticality | kein kritischer Safetyverlust; Y4 wenn Safety-Record betroffen |
| noncritical RAM/export error | existing noncritical policy | O2/warning, kein Safety-Truth overwrite |
| missing NTP | #18 consumed | `RECOVERY_TIME_PENDING`, kein erfundener Progress |
| later NTP sync | #18 consumed | Intervall/Bounds, keine monotone Cross-Boot-Subtraktion |
| outage inside phase | #18 consumed | #18 Phase decision |
| outage across phase boundary | #18 consumed | kein automatischer Abschluss bei Unsicherheit |
| WLAN outage with safe process | existing local contract | Safety/Regelung ohne WLAN |
| persisted COMPLETED after reboot | existing #17 state | COMPLETED bleibt COMPLETED |
| acknowledge without reset | #24 lifecycle | Message-only, Gate unverändert |
| reset while cause active | #24/#15 | Target rejected |
| service function without PIN | auth boundary | fail-closed |
| actuator test during run | #15/#23 boundary | blockiert |
| actuator test in SAFE_BOOT | #24 | blockiert |
| conflicting Display/Web action | #15 envelope/revision | stale/conflict, kein Safetybypass |

## 19. Lifecycle-, S3-, Y4- und SAFE_BOOT-Testmatrix

### 19.1 Lifecycle

- duplicate Raise derselben Identity ohne neue InstanceId;
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

### 19.5 SAFE_BOOT/Restart

Factory-init, pre-#24-Upgrade, fehlender SafetyState auf Nicht-Factory-Gerät,
alle ResetCause-Werte, gültiger/ungültiger Intent, Attempted/Rejected,
Counter 0/1/2/3, 30-Minuten-Requalifikation, Safety-Redundanzrepair,
`safeBootRequired`, Crash nach Flag-Clear und die vollständige Exit-Matrix.

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
`ACTUATOR_TIMING_AND_FANS.md`, `CONFIGURATION_PERSISTENCE.md`,
`ARCHITECTURE.md` und `ROADMAP.md`. Reviewchronik wird nicht in kanonische
Doku kopiert.

## 22. Plan-Gates und Stop

Vor dem Plancommit werden Plan und aktueller `main` nochmals vollständig
gegen Issue #24, die verpflichtenden Akzeptanztests und die Architekturgrenzen
gelesen. Es gibt kein offenes Safety-/Persistenz-/Recovery-Design-TBD. Das
einzige zulässige `TBD_COMMISSIONING` betrifft reale Schwellen/Parameter; die
fehlende-Wert-Wirkung ist immer bereits fail-closed festgelegt.

Für diese Planrevision gelten:

```text
git diff --check                         REQUIRED
python3 scripts/check_architecture_boundaries.py  REQUIRED
python3 scripts/check_secrets.py        REQUIRED
Tests/Builds/Hardware                   NOT_RUN (Plan-only)
```

Nach Commit, Push, Draft-PR, synchronisiertem PR-Body und aktuellem SESSION
HANDOVER ist STOP. Keine Implementierung, kein Ready, kein Merge, kein
Issue-Schluss und keine Änderung an PR #107/#108.
