# Issue #24 – Safety Core neu von origin/main

## 1. Planstatus, Basis und Scope

Dies ist die vollständige plan-only Revision für Issue #24
[E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion.
Sie ersetzt die bisherige Fassung dieses Pfades vollständig und bleibt ein
Owner-Gate vor jeder Implementation.

| Gegenstand | Verbindlicher Stand |
|---|---|
| Repository | ManuEngineer/ESP32-Fermentationsschrank |
| Branch | agent/issue-24-safety-core-replan |
| Draft-PR | #109 |
| Base | main @ b8eae5f4da5f2666b5a9bda333d115254c4db5b2 |
| Planpfad | docs/tasks/issue-24-safety-core-replan.md |
| Planstatus | IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL |

Die Implementierung beginnt erst nach Freigabe des exakten neuen Plan-SHA.
Diese Revision ändert ausschließlich Plan- und Roadmap-Dokumentation sowie
danach PR-Body und einen aktuellen SESSION HANDOVER.

Nicht erlaubt sind in dieser Revision:

- Produktionscode, Testimplementation, Konfiguration, Bibliotheken oder
  Hardwareänderungen;
- Rebase, Cherry-Pick oder Übernahme aus PR #107 oder PR #108;
- Ready for review, Merge, Auto-Merge, Issue-Schluss oder Branchlöschung;
- eine neue Recovery-, Persistenz- oder Eventengine;
- erfundene GPIOs, Pegel, Controller, Fanrückmeldungen, Hardwaregrenzen oder
  Commissioning-Ergebnisse.

PR #107 enthält aufgegebene Implementation- und Reviewhistorie und ist
nicht-normativ. PR #108 ist eine aufgegebene, superseded Planungshistorie und
ist ebenfalls nicht-normativ. Beide PRs bleiben unverändert offen und Draft.

## 2. Live-Verifikation und Quellen

Vor dieser Revision wurden live geprüft:

- origin/main unverändert auf
  b8eae5f4da5f2666b5a9bda333d115254c4db5b2;
- PR #109 offen, Draft, ungemergt, Head
  84c8669d37a066323383676237878e4fa453f1a6;
- Issue #24 offen mit Scope Fehlerklassen, sicherer Reaktion, Quittierung/
  Reset, SAFE_BOOT, Injection und Configuration-Gate;
- PR #107 offen/Draft/ungemergt, Head
  9ba857545094dafe132f43d95e975b43bab31c38;
- PR #108 offen/Draft/ungemergt, Head
  a448ad591d147b6388f8fc8b896440baad4facc9;
- Issues #14, #15, #17, #18, #20, #21, #22, #23, #56 und #57 live sowie die
  jeweils referenzierten Verträge direkt am Basiscommit.

Normative Quellen:

| Bereich | Quellen |
|---|---|
| Governance | AGENTS.md, docs/AGENT_WORKFLOW.md, docs/ENGINEERING_PRINCIPLES.md |
| Architektur | docs/DECISIONS.md ADR-013/014, lokale lib/*/AGENTS.md |
| Scope | docs/SPECIFICATION_REVIEW.md, docs/ROADMAP.md, Issue #24 |
| Safety | docs/SAFETY_AND_FAULTS.md, docs/SAFETY_COMPONENT_FAULTS.md, docs/SYSTEM_SAFETY_AND_RECOVERY.md |
| Prozess | docs/STATE_MACHINE.md, docs/RUN_COMMANDS.md |
| Run/Recovery | docs/RUN_PERSISTENCE.md, docs/RECOVERY_AND_INTERRUPTION.md, run_persistence_*, run_recovery* |
| Sensoren/Aktoren | docs/ACTUATOR_TIMING.md, docs/ACTUATOR_TIMING_AND_FANS.md, sensor_quality.*, sensor_selection.*, temperature_control*, actuator_* |
| Konfiguration | docs/CONFIGURATION_PERSISTENCE.md, configuration_service.*, configuration_recovery_service.*, configuration_bootstrap.* |
| Qualität | docs/ACCEPTANCE_TESTS.md, docs/CI_AND_QUALITY_GATES.md |

Der aktuelle Code bestätigt außerdem:

- Recordtypen 1–6 gehören zur Konfigurationsdomäne, 7/8 zu #17; Recordtypen
  9/10 sind am Basiscommit frei.
- Der generische StorageEnvelope verwendet Big-Endian, utcUnixSeconds als
  optionales Feld und ohne UTC 37 Byte festen Overhead inklusive CRC.
- StateStoreKey erlaubt feste ASCII-Schlüssel bis 15 Byte.
- #17 verwendet Schema 3, Current/Fallback-Referenzen, zwei Run-Slots sowie
  FallbackRecoveryPending; #18 verwendet den bestehenden
  activateFallbackRecoveredRun()-Pfad.
- Der aktuelle FaultResetEvaluation-Vertrag lehnt noch jeden Reset bei
  otherBlockingFaultActive ab und enthält noch caller-supplied
  safetyAllows...-Booleans. Das wird in der späteren Implementation durch die
  hier normierten Verträge ersetzt.
- PR #105 ist bereits in main gemergt. Die bisherige Roadmap-Zeile über einen
  aktuellen Draft-PR #105 ist deshalb zu korrigieren.

## 3. Unveränderliche Architekturentscheidungen

Diese Architektur bleibt verbindlich:

~~~text
S3 = RecoverIfProvable
Y4 = Terminal
~~~

S3 wählt nach CauseClear und Service-Reset den vorhandenen Pre-Fault-
Fallback ausschließlich RAM-seitig aus und übergibt ihn an den bestehenden
FallbackRecoveryPending-/#18-Pfad. Es gibt keinen #17-Head-Rollback, kein
safetyFallbackPromotion-Wirefeld, keinen Schemawechsel und keine zweite
Recoveryentscheidung.

Y4 setzt bei aktivem Run atomar die rebootfeste Terminalpflicht
runRecoveryForbidden=true. Der alte Run darf nie in S3/#18 gelangen. Nach
qualifizierter Terminalisierung wird ein physischer kanonischer
NoActiveRun/STANDBY-Tombstone geschrieben; erst danach darf die
Terminalpflicht gelöscht werden.

Die einzige mutable Safety-/Fault-Wahrheit ist SafetyFaultService. Sie
besitzt Catalog, aktive persistente FaultRecords, CauseClear, FaultRevision,
InstanceIds, Primary-Verweise, RestartIntent, SAFE_BOOT und
runRecoveryForbidden. RunCommandState, Prozessautomat, Planner,
ConfigurationService und Orchestrator tragen keine zweite Safety-Wahrheit.

FaultIdentity = FaultCode + FaultSource. MeasurementSequence, RunRevision,
ControlRevision, PlannerRevision, Zeitstempel und CorrelationKey gehören nicht
in die Identity. P1/O2 sind transient und bootlokal. S3/Y4 sind persistent und
rebootstabil. Aktive FaultRecords werden niemals evicted.

Die Modulrichtung aus ADR-013 bleibt:

~~~text
device_platform
  generische Ports für Store, Zeit und Reset; StorageEnvelope und Key-Typen

device_platform_esp_idf
  ESP-IDF-Adapter für diese Ports; keine Fachentscheidung

fermentation_app
  FaultCatalog, SafetyFaultService, SafetyState-Orchestrierung,
  Run-/Process-/Recovery-Integration und Aktor-Sicherheitsprojektion

device_platform_test_support
  deterministische Mocks, Simulatoren und Injection-only-Producer
~~~

## 4. Stable FaultCatalog

### 4.1 Wiretypen und Reservierungen

Alle Wirewerte sind explizit; C++-Enum-Reihenfolge ist kein Wirevertrag.

FaultClass:

| Wirewert | Klasse |
|---:|---|
| 0x01 | P1 |
| 0x02 | O2 |
| 0x03 | S3 |
| 0x04 | Y4 |
| 0x00, 0x05..0xff | reserviert, Decode-Fail-Closed |

FaultSource:

| Wirewert | Source |
|---:|---|
| 0x01 | RecoveryEvidence |
| 0x02 | SafetyAirSensor |
| 0x03 | CoolingSensor |
| 0x04 | ProductSensor |
| 0x05 | SensorCrossCheck |
| 0x06 | ThermalGuard |
| 0x07 | PeltierDriver |
| 0x08 | OuterFanFunctional |
| 0x09 | OuterFanElectrical |
| 0x0a | InnerFanFunctional |
| 0x0b | InnerFanElectrical |
| 0x0c | ActuatorPlanner |
| 0x0d | HardwareGate |
| 0x0e | RunPersistence |
| 0x0f | SafetyStateStore |
| 0x10 | ConfigurationRuntime |
| 0x11 | ConfigurationAvailability |
| 0x12 | ConfigurationIntegrity |
| 0x13 | ConfigurationCommit |
| 0x14 | RestartSupervisor |
| 0x15 | SafetyBoot |
| 0x16 | SafetyFaultService |
| 0x00, 0x17..0xff | reserviert, Decode-Fail-Closed |

FaultCode ist ein stabiler uint16_t:

| Code | Name | Source | Klasse | Persistent |
|---:|---|---|---|---|
| 0x0101 | P1_RECOVERY_EVIDENCE_UNCERTAIN | RecoveryEvidence | P1 | nein |
| 0x0201 | O2_SAFETY_AIR_STALE | SafetyAirSensor | O2 | nein |
| 0x0202 | O2_COOLING_STALE | CoolingSensor | O2 | nein |
| 0x0203 | O2_PRODUCT_SENSOR_UNAVAILABLE | ProductSensor | O2 | nein |
| 0x3101 | S3_SAFETY_AIR_SENSOR_FAILED | SafetyAirSensor | S3 | ja |
| 0x3102 | S3_COOLING_SENSOR_FAILED | CoolingSensor | S3 | ja |
| 0x3103 | S3_SENSOR_CONTRADICTION | SensorCrossCheck | S3 | ja |
| 0x3104 | S3_THERMAL_INTERVENTION | ThermalGuard | S3 | ja |
| 0x3105 | S3_THERMAL_HARD_EMERGENCY | ThermalGuard | S3 | ja |
| 0x3106 | S3_PELTIER_H_BRIDGE | PeltierDriver | S3 | ja |
| 0x3107 | S3_OUTER_FAN_FUNCTIONAL | OuterFanFunctional | S3 | ja |
| 0x3108 | S3_OUTER_FAN_ELECTRICAL | OuterFanElectrical | S3 | ja |
| 0x3109 | S3_INNER_FAN_FUNCTIONAL | InnerFanFunctional | S3 | ja |
| 0x310a | S3_INNER_FAN_ELECTRICAL | InnerFanElectrical | S3 | ja |
| 0x310b | S3_ACTUATOR_WATCHDOG | ActuatorPlanner | S3 | ja |
| 0x310c | S3_UNCONFIRMED_HARDWARE | HardwareGate | S3 | ja |
| 0x4101 | Y4_RUN_NOT_RECONSTRUCTIBLE | RunPersistence | Y4 | ja |
| 0x4102 | Y4_RUN_PERSISTENCE_FAILURE | RunPersistence | Y4 | ja |
| 0x4103 | Y4_RUN_PERSISTENCE_INDETERMINATE | RunPersistence | Y4 | ja |
| 0x4104 | Y4_SAFETY_STATE_STORE | SafetyStateStore | Y4 | ja |
| 0x4105 | Y4_CONFIGURATION_RUNTIME | ConfigurationRuntime | Y4 | ja |
| 0x4106 | Y4_CONFIGURATION_UNAVAILABLE | ConfigurationAvailability | Y4 | ja |
| 0x4107 | Y4_CONFIGURATION_INTEGRITY | ConfigurationIntegrity | Y4 | ja |
| 0x4108 | Y4_CONFIGURATION_COMMIT_INDETERMINATE | ConfigurationCommit | Y4 | ja |
| 0x4109 | Y4_RESTART_LOOP | RestartSupervisor | Y4 | ja |
| 0x410a | Y4_UNKNOWN_SAFETY_STATE | SafetyBoot | Y4 | ja |
| 0x410b | Y4_INTERNAL_SAFETY | SafetyFaultService | Y4 | ja |

Damit gilt:

~~~text
kCatalogIdentities = 27
kMaxPersistentFaultRecords = 23
~~~

P1/O2 erscheinen nie im SafetyState-Recordarray, erhöhen weder
persistentFaultCount noch nextPersistentInstanceId und verändern
persistentFaultRevision nicht. Die 23 persistenten Pairings bilden eine
compile-time std::array; static_assert prüft Anzahl, eindeutige Codes,
eindeutige Pairings und Recordkapazität.

### 4.2 Normative Policy-Matrix

Die Catalogzeile referenziert genau eine der folgenden Policy-Definitionen;
die Referenz ist Teil der Zeile und keine spätere Implementationsentscheidung.

| Policy | Auto-Rearm | Gate | Peltier/H-Brücke | Außenlüfter | Innenlüfter | Restart | Reset | CauseClear | Run nach Reset |
|---|---|---|---|---|---|---|---|---|---|
| P1_NOTICE | Boot-Clear | Allowed | Planner | Planner | Planner | keiner | keiner | neue Evidenz | unverändert |
| O2_STALE | ProducerHealthy | Unresolved | AUS / keine neue Freigabe | ForceOn, sofern elektrisch nicht unsicher | PlannerManaged | keiner | bestehende #21-Policy | aktueller Sensor gültig | bestehende #21-Policy |
| O2_PRODUCT | ProducerHealthy gemäß #21 | Unresolved | bestehende #21-Peltier-Sperre | bestehende #23-Nachlaufregel | Planner | keiner | bestehende Operator-/Auto-Policy | #21-Produktorakel | bestehende #21-Policy |
| S3_SENSOR_FAILED | nein | ImmediateStop | AUS | ForceOn, sofern nicht elektrische Fanursache | Planner, solange Planner vertrauenswürdig | S3Recovery, nur letzter S3 und kein Y4 | Service | alle Safety-Sensorchecks gesund | RecoverIfProvable |
| S3_THERMAL_INTERVENTION | nein | ImmediateStop | aktuelle Richtung AUS, accumulator verwerfen, Integral sperren | ForceOn | ForceOn | S3Recovery, nur nach qualifizierter CauseClear | Service | Temperatur-/Sensororacle gesund | RecoverIfProvable |
| S3_THERMAL_HARD | nein | ImmediateStop | beide Richtungen AUS | ForceOn | ForceOn | S3Recovery, ohne Gegenrichtung | Service | Hard-Emergency-Oracle gesund | RecoverIfProvable |
| S3_PELTIER | nein | ImmediateStop | AUS | ForceOn, sofern nicht Außen-Elektrik | ForceOn | S3Recovery nach Ursache | Service | Driver-/H-Bridge-Oracle gesund | RecoverIfProvable |
| S3_FAN_FUNCTIONAL | nein | ImmediateStop | AUS | betroffener Fan ForceOn, sofern elektrisch sicher | nicht betroffener Fan gemäß Ursache | S3Recovery | Service | funktionale Fan-Evidenz gesund | RecoverIfProvable |
| S3_FAN_ELECTRICAL | nein | ImmediateStop | AUS | betroffener Fan ForceOff; anderer Fan Restwärme | anderer Fan ForceOn/Planner nach Ursache | S3Recovery | Service | elektrische Ursache behoben | RecoverIfProvable |
| S3_WATCHDOG | nein | ImmediateStop | AUS | ForceOn | ForceOn | S3Recovery | Service | neuer gültiger Plannerkontext | RecoverIfProvable |
| S3_HARDWARE | nein | ImmediateStop | AUS | ForceOn | ForceOn | S3Recovery | Service | bestätigte Hardware-/Safety-Evidenz | RecoverIfProvable |
| Y4_RUN | nein | ImmediateStop | AUS | ForceOn | ForceOn | keiner | Service | Store-/Terminaloracle | Terminal |
| Y4_SAFETY | nein | ImmediateStop | AUS | ForceOn | ForceOn | keiner | Service | redundanter SafetyStore gesund | Terminal |
| Y4_CONFIG | nein | ImmediateStop | AUS | ForceOn | ForceOn | keiner | Service | reale #56/#57-Evidenz gesund | Terminal |
| Y4_RESTART | nein | ImmediateStop | AUS | ForceOn | ForceOn | keiner | Service | Restart-/Bootoracle gesund | Terminal |
| Y4_INTERNAL | nein | ImmediateStop | AUS | ForceOn | ForceOn | keiner | Service | SafetyFaultService-Oracle gesund | Terminal |

S3_THERMAL_INTERVENTION darf höchstens zwei firmwarefeste
SAFETY_RECOVERY-Versuche ausführen. Das zählt ausschließlich die
ursachenspezifische Gegenrichtungsintervention; fehlen Commissioningparameter,
bleibt der Versuch bei null und die Ausgabe AUS. S3_THERMAL_HARD_EMERGENCY
führt nie eine Gegenrichtung aus. Beide Pfade bleiben gelatcht und benötigen
Service-Reset. Timing, Mindest-Auszeit, Totzeit und Integratorfeedback kommen
aus #23/#22; FaultCore steuert keine H-Brücke direkt.

Eine funktionale Fanursache ist nicht automatisch eine elektrische
Ausgangsursache. Die getrennten Catalog-Identities verhindern ForceOn an einem
elektrisch unsicheren Ausgang. Eine physische Fanrückmeldung wird nicht
behauptet; fehlende Hardwareproducer bleiben injection-only.

`Y4_INTERNAL_SAFETY` bezeichnet ausschließlich die eine begrenzte Invariante
„SafetyFaultService kann seine kanonische Fault-/Revision-/Event-Wahrheit nicht
mehr sicher erhalten“; dazu gehören nur die in diesem Plan genannten
Überläufe, Bounds- und internen Konsistenzverletzungen. Unbekannte SafetyState-
Persistenzdaten und Restart-Schleifen bleiben die getrennten Identities
`Y4_UNKNOWN_SAFETY_STATE` und `Y4_RESTART_LOOP`. Unabhängige Ursachen werden
nicht unter einem generischen Internal-Code zusammengefasst.

### 4.3 Vollständige Zeilenauflösung

Jede der 27 erlaubten Identities wird zusätzlich durch diese feste Zuordnung
vollständig aufgelöst. Gate, Fan, CauseClear und PostReset kommen aus der
referenzierten Policy; die hier genannten Werte legen Priorität, Restartzweck,
Resetautorität und Producerstatus je Identity fest.

| Code | displayPriority | Policy | Restartzweck | Reset | Producerstatus |
|---:|---:|---|---|---|---|
| 0x0101 | 10 | P1_NOTICE | keiner | keiner | #18/Injection |
| 0x0201 | 20 | O2_STALE | keiner | #20/#21-Policy | realer #20/#21-Producer |
| 0x0202 | 21 | O2_STALE | keiner | #20/#21-Policy | realer #20/#21-Producer |
| 0x0203 | 22 | O2_PRODUCT | keiner | #21-Policy | realer #21-Producer |
| 0x3101 | 100 | S3_SENSOR_FAILED | S3Recovery | Service | #20/#21 oder Injection |
| 0x3102 | 110 | S3_SENSOR_FAILED | S3Recovery | Service | #20/#21 oder Injection |
| 0x3103 | 120 | S3_SENSOR_FAILED | S3Recovery | Service | #20/#21 oder Injection |
| 0x3104 | 130 | S3_THERMAL_INTERVENTION | S3Recovery | Service | #20/#22 oder Injection |
| 0x3105 | 140 | S3_THERMAL_HARD | S3Recovery ohne Gegenrichtung | Service | #20 oder Injection |
| 0x3106 | 150 | S3_PELTIER | S3Recovery | Service | #23 oder Injection |
| 0x3107 | 160 | S3_FAN_FUNCTIONAL | S3Recovery | Service | Injection bis Producer |
| 0x3108 | 161 | S3_FAN_ELECTRICAL | S3Recovery | Service | Injection bis Producer |
| 0x3109 | 170 | S3_FAN_FUNCTIONAL | S3Recovery | Service | Injection bis Producer |
| 0x310a | 171 | S3_FAN_ELECTRICAL | S3Recovery | Service | Injection bis Producer |
| 0x310b | 180 | S3_WATCHDOG | S3Recovery | Service | realer #23-Producer |
| 0x310c | 190 | S3_HARDWARE | S3Recovery | Service | Injection-only |
| 0x4101 | 200 | Y4_RUN | keiner, Terminal | Service | realer #17-Producer |
| 0x4102 | 201 | Y4_RUN | keiner, Terminal | Service | realer #17-Producer |
| 0x4103 | 202 | Y4_RUN | keiner, Terminal | Service | realer #17-Producer |
| 0x4104 | 210 | Y4_SAFETY | keiner, Terminal | Service | #24/Store-Producer |
| 0x4105 | 220 | Y4_CONFIG | keiner, Terminal | Service | realer #56-Producer |
| 0x4106 | 221 | Y4_CONFIG | keiner, Terminal | Service | realer #57-Producer |
| 0x4107 | 222 | Y4_CONFIG | keiner, Terminal | Service | realer #57-Producer |
| 0x4108 | 223 | Y4_CONFIG | keiner, Terminal | Service | realer #56/#57-Producer |
| 0x4109 | 230 | Y4_RESTART | keiner, Terminal | Service | #24/Reset-Producer |
| 0x410a | 240 | Y4_RESTART | keiner, Terminal | Service | #24/Injection |
| 0x410b | 250 | Y4_INTERNAL | keiner, Terminal | Service | #24/Injection |

### 4.4 Dominanz, Priorität und Beziehungen

~~~text
Y4 > S3 > O2 > P1
~~~

Innerhalb einer Klasse entscheidet eine feste displayPriority aus dem Catalog.
Alle aktiven Ursachen bleiben erhalten. primaryInstanceId ist nur eine
Diagnosebeziehung. Ein unabhängiger unbekannter Fault erhält keine künstliche
gemeinsame Primary-Identity.

## 5. SafetyState: exakter persistenter Vertrag

### 5.1 Recordfamilie

Die SafetyState-Familie ist vom Configuration-StorageEpoch fachlich getrennt,
verwendet aber den vorhandenen generischen StorageEnvelope und IStateStore.

~~~text
RecordTypeId       = 9 (u16)
SchemaVersion      = 1 (u32)
SafetyStorageEpoch = 1 (u64, eigene Safety-Domäne)
Keys               = sf0, sf1
Slotanzahl         = 2
MaxEnvelope        = 1024 Byte
utcUnixSeconds     = nullopt
Envelope-Encoding  = bestehender Big-Endian-Envelope
~~~

sf0 und sf1 sind compile-time erzeugte StateStoreKeys. Dynamische Keys,
Konfigurationskeys und StorageEpoch-Werte aus #56/#57 sind unzulässig. Die
Recordtypen 9/10 sind am Basiscommit frei; ein späterer Konflikt stoppt die
Implementation und erfordert eine neue Planfreigabe.

Der Envelope besitzt ohne UTC exakt 37 Byte festen Overhead: 33 Byte Header
ohne CRC/UTC plus 4 Byte CRC. versionValue ist die recordRevision des
SafetyState und darf nicht null sein. Die höchste semantisch gültige
recordRevision gewinnt. Gleiche Revision mit unterschiedlichen kanonischen
Bytes ist Integritätsfehler.

### 5.2 Exakte SafetyState-Payload

Die Payload wird mit den bestehenden Big-Endian-Helfern kodiert. Bool ist genau
ein Byte (0x00=false, 0x01=true); andere Werte sind Decode-Fehler. Alle
Enumwerte sind unten festgelegt; unbekannte Werte, Reserved-Bits, falsche
Längen, doppelte Records, unsortierte Records und Restbytes werden abgelehnt.

Feste Feldfolge:

| Offset/Größe | Feld | Wertevertrag |
|---:|---|---|
| 0/4 | nextPersistentInstanceId: uint32 | 1..UINT32_MAX-1; 0 ist im dekodierten SafetyState ungültig |
| 4/4 | persistentFaultRevision: uint32 | checked, kein Wraparound |
| 8/4 | bootSequence: uint32 | checked +1 je Boot, nicht saturierend |
| 12/1 | safeBootRequired | 0x00/0x01 |
| 13/1 | abnormalRestartCount | 0..3, kein höherer Wert |
| 14/1 | restartIntentState | 0=None, 1=Prepared, andere reserviert |
| 15/1 | restartIntentKind | 0=None, 1=S3Recovery, andere reserviert |
| 16/4 | restartIntentSourceInstanceId: uint32 | 0 bei None, sonst 0 < id < nextPersistentInstanceId |
| 20/1 | restartRequestOutcome | 0=None, 1=Requested, 2=Rejected |
| 21/1 | runRecoveryForbidden | 0x00/0x01 |
| 22/1 | persistentFaultCount | exakt Anzahl gesetzter Records, 0..23 |
| 23/575 | kMaxPersistentFaultRecords Records | genau 23 × 25 Byte |
| 598 | Payloadende | keine weiteren Bytes |

Ein persistenter FaultRecord hat die feste Feldfolge:

| Größe | Feld | Wertevertrag |
|---:|---|---|
| 1 | present | 0x00/0x01; leere Plätze vollständig null |
| 4 | instanceId: uint32 | nicht null, aufsteigend über gesetzte Records |
| 2 | faultCode: uint16 | exakt Catalogcode |
| 1 | faultSource: uint8 | exakt Catalogsource zum Code |
| 1 | flags: uint8 | Bit 0 causeCleared; Bits 1..7 reserviert und null |
| 4 | firstSeenBootSequence: uint32 | nicht null |
| 8 | firstSeenMonotonicMs: uint64 | bootlokaler Diagnoseanker |
| 4 | primaryInstanceId: uint32 | 0 absent; sonst zuvor vergebene ID, 0 < primaryInstanceId < instanceId < nextPersistentInstanceId |

Die 25 Wirebytes enthalten das present-Byte. Die RAM-Repräsentation darf dieses
Byte weglassen und einen leeren Platz ausschließlich mit instanceId=0
repräsentieren; die geordnete RAM-Feldfolge ist instanceId, firstSeenBootSequence,
firstSeenMonotonicMs, primaryInstanceId, faultCode, faultSource, flags und
hat exakt 24 Byte. Wire- und RAM-Größe werden getrennt static_asserted.

Die 23 Records belegen 575 Byte. Die maximale Payload ist 598 Byte und der
maximale Envelope ist damit 635 Byte (598 + 37), also unter dem Limit von
1024 Byte. Es gibt keinen SafetyState-UTC-Wert, keine Ackdaten, keine
Follow-up-ID-Liste, keine Follow-up-Zählung, keine Runprogressdaten und keine
Policyduplikation.

FaultClass, Gate, Fanpolicy, Restartpolicy und Autorisierung werden aus dem
compile-time Catalog abgeleitet. persistentFaultCount zählt S3/Y4-Records
inklusive causeCleared-Latch, nicht P1/O2.

### 5.3 Revisionen und Überläufe

persistentFaultRevision ist exakt uint32_t, damit er ohne Adapterkonvertierung
mit CommandEnvelope.expectedFaultRevision,
FaultResetEvaluation.faultRevision und RunCommandState.faultRevision
verglichen werden kann.

Bei UINT32_MAX gilt für jede weitere resetrelevante Mutation:

~~~text
keine Mutation
Y4_INTERNAL_SAFETY
safeBootRequired=true
fail-closed
~~~

nextPersistentInstanceId wird checked erhöht. Der Wert UINT32_MAX wird nie als
neue persistente InstanceId vergeben; bei Erreichen gibt es keine weitere
persistente Faultidentität, sondern Y4_INTERNAL_SAFETY und SAFE_BOOT. Es gibt
keinen Wraparound und keine Wiederverwendung.

recordRevision ist das Envelope-versionValue und bleibt uint64_t; er steigt bei
jedem SafetyState-Commit. Sein Überlauf ist ebenfalls ein Y4/fail-closed-
Zustand.

### 5.4 Scan, Auswahl und Redundanzrepair

Der Bootscan liest beide Keys mit maxBytes=1024:

1. technische Envelope-, CRC-, RecordType-, Schema-, Epoch-, UTC- und
   Payloadprüfung;
2. Payloaddecode mit allen obigen Enum-/Reserved-/Sortierregeln;
3. höchste recordRevision auswählen;
4. gleiche Revision mit bytegleichen Records ist Duplikat und gesund;
5. gleiche Revision mit verschiedenen Records ist Y4-SafetyState-Integrität;
6. ein gültiger und ein fehlender/defekter Slot lädt den gültigen Record
   konservativ, setzt aber Y4_SAFETY_STATE_STORE, repariert den Peer und liest
   beide Slots zurück;
7. zwei technisch gültige, semantisch widersprüchliche Slots bleiben
   fail-closed;
8. ein gültiger SafetyState plus Active-/defekter Marker bleibt bis Marker-
   Scan und Repair fail-closed.

Redundanzrepair ist keine CauseClear- oder Resetaktion. Der
Y4_SAFETY_STATE_STORE-Record bleibt bis vollständiger CauseClear-Evidenz und
Service-Reset gelatcht; ein erfolgreicher Repair allein macht den Zustand nicht
freigegeben.

Healthy ist kein Wirefeld. Der Runtime-Status Healthy, MarkerRequired oder
BlockedIndeterminate wird bei jedem Boot aus beiden SafetyState-Slots, beiden
Marker-Slots und den Readbackergebnissen abgeleitet.

## 6. EmergencyMarker: eigene redundante Familie

~~~text
RecordTypeId       = 10
SchemaVersion      = 1
SafetyStorageEpoch = 1
Keys               = sem0, sem1
Slotanzahl         = 2
MaxEnvelope        = 64 Byte
utcUnixSeconds     = nullopt
Encoding           = bestehender Big-Endian-Envelope
~~~

`reason` ist ebenfalls ein stabiler Wirewert und kein impliziter C++-Enumwert:

| Wirewert | MarkerReason |
|---:|---|
| 0x0001 | SafetyStateCommitFailed |
| 0x0002 | SafetyStateCommitIndeterminate |
| 0x0003 | SafetyStateRedundancyRepair |
| 0x0004 | FactoryInitialization |
| 0x0000, 0x0005..0xffff | reserviert, Decode-Fail-Closed |

Der Marker-Payload ist exakt 27 Byte:

| Größe | Feld | Werte |
|---:|---|---|
| 4 | markerRevision: uint32 | nicht null, checked; UINT32_MAX blockiert weitere Marker-Mutation und erzwingt Y4/fail-closed |
| 1 | state | 1=Active, 2=Cleared, andere reserviert |
| 2 | reason: uint16 | stabiler Markergrund, 0 reserviert |
| 4 | bootSequence: uint32 | SafetyState-Bootfolge |
| 8 | monotonicMillis: uint64 | aktueller Boot, keine Rebootarithmetik |
| 8 | attemptedSafetyRecordRevision: uint64 | 0 ohne zugehörigen Versuch, sonst betroffene SafetyState-Revision |

27 + 37 = 64 Byte exakt. Die Envelope-versionValue-Revision und die
Payload-Revision müssen übereinstimmen.

### 6.1 Commitfehler

Bei normalem SafetyState-Commitfehler:

1. RAM sofort ImmediateStop und Gate nicht freigeben;
2. Marker Active mit Revision max(validMarkerRevision)+1 auf den bevorzugten
   Slot schreiben und readbacken;
3. bei nicht bestätigtem Ergebnis genau ein Versuch auf dem anderen Marker-
   Slot;
4. danach kein Retry-Loop und keine Freigabe bei unklarem Ergebnis.

Ein WriteError/CapacityError verändert den vorherigen Marker nicht; ein
CommitOutcomeUnknown wird nur durch den vorgeschriebenen Readbackversuch auf-
gelöst. Ein zweiter Slotversuch ist die einzige begrenzte Recoveryhandlung.

### 6.2 Bootscan und Marker-Clear

Beide Marker werden wie SafetyState geprüft. Die höchste semantisch gültige
Revision gewinnt. Ein gültiger Active hält fail-closed. Ein Cleared darf einen
alten Active nur überstimmen, wenn beide Markerhistorien technisch und
semantisch gesund sind. Ein beschädigter oder fehlender Peer löst
Y4_SAFETY_STATE_STORE/Redundanzrepair aus; es gibt keine stille Rückkehr zu
Healthy.

Marker-Recovery repariert Persistenz, ist aber weder FaultReset noch
SAFE_BOOT-Exit. Ein Cleared-Marker wird erst nach bestätigt gesundem
SafetyState-Commit geschrieben. Marker-Clear selbst erzeugt keine
FaultResetwirkung und entfernt keine FaultRecord-Latch.

## 7. Factory-new, verlorene History und SafetyState-Verlust

Zwei fehlende SafetyState-Slots bedeuten nicht automatisch einen leeren,
gesunden Zustand.

### 7.1 Echte Factory-Initialisierung

Eine leere Initialisierung ist nur erlaubt, wenn alle Bedingungen erfüllt sind:

- beide SafetyState- und Marker-Slots sind NotFound;
- #57/ConfigurationBootstrap liefert ConfigurationBootstrapState::Initializing
  oder die exakt gleichwertige Erstinitialisierung;
- die bestehende FactoryNoveltyProof ist für denselben Store, dieselbe
  Mutation-Lease, Service-Revision und Recovery-Generation gültig und wird
  phasenrichtig verbraucht;
- es gibt keinen vorherigen SafetyState-, Marker- oder RunPersistence-Record;
- Ausgänge bleiben bis zum erfolgreichen redundanten Initial-Readback AUS.

Dann wird ein leerer SafetyState mit bootSequence=1,
nextPersistentInstanceId=1, persistentFaultRevision=0,
persistentFaultCount=0, safeBootRequired=false,
runRecoveryForbidden=false und recordRevision=1 auf beide Slots geschrieben
und verifiziert. Zusätzlich werden Cleared-Marker mit
MarkerReason::FactoryInitialization auf beide Marker-Slots geschrieben und
verifiziert. Der erste Slot allein ist noch nicht gesund; Factory-Initialisierung
ist erst nach redundanter SafetyState- und Marker-Bestätigung abgeschlossen.

### 7.2 Nicht-Fabrikzustand

Fehlen beide SafetyState-Slots ohne gültige Factory-Evidenz oder fehlen Marker
bei unbekanntem Gerät:

~~~text
Y4_SAFETY_STATE_STORE
safeBootRequired=true, soweit SafetyState noch schreibbar
sonst Active EmergencyMarker
SAFE_BOOT
keine Aktorfreigabe
~~~

Es wird keine Fault-History erfunden und kein Configuration-/Programmreset
ausgeführt. Wenn SafetyState selbst nicht schreibbar ist, halten Marker-/Store-
fehler fail-closed; ein späterer Repair ist autorisiert und bounded, nicht
automatisch gesund.

## 8. Fault-Lifecycle, Ack und Beziehungen

### 8.1 Raise

Für eine neue persistente S3/Y4-Identity:

1. nächste checked instanceId vergeben;
2. causeCleared=false, present=true;
3. persistentFaultRevision++;
4. persistentFaultCount++;
5. optional plausible primaryInstanceId setzen;
6. SafetyState commit/readback;
7. erst nach Commit die typisierte Directive-/Eventprojektion veröffentlichen.

Ist dieselbe Identity bereits vorhanden, entsteht kein zweiter Record und keine
neue InstanceId. Ein wiederholtes Producer-Signal darf nur Evidenz auffrischen,
nicht die Persistenz unnötig schreiben.

### 8.2 CauseClear und Relapse

CauseClear verlangt die vollständige kanonische Producer-Evidenz des Catalog-
Eintrags. Einzelne UI-, Transport- oder Testbooleans sind keine CauseClear-
Evidenz. Bei Erfolg:

~~~text
flags.CauseCleared = 1
persistentFaultRevision++
SafetyState commit/readback
Latch bleibt aktiv
~~~

Wird die Ursache vor Reset wieder aktiv:

~~~text
flags.CauseCleared = 0
persistentFaultRevision++
SafetyState commit/readback
~~~

Eine alte ResetEvaluation wird dadurch wegen Revision/Target stale.

### 8.3 Reset und Ack

Ein qualifizierter Target-Reset entfernt erst nach SafetyState-Commit den
Target-Record, vermindert persistentFaultCount um eins und erhöht
persistentFaultRevision. Der Commit ist die Reset-Linearisierung. Y4
runRecoveryForbidden wird durch FaultReset nicht gelöscht.

Quittierung ist von Reset getrennt. Ack/Mute ist Message-/UI-Zustand:

- entfernt keinen Fault;
- setzt CauseClear nicht;
- ändert Gate und persistentFaultRevision nicht;
- wird nicht im SafetyState persistiert;
- darf nach Reboot erneut unacked erscheinen.

Follow-ups persistieren ausschließlich primaryInstanceId. Es gibt weder
followUpCount noch followUpIds[]. Follow-ups werden aus dem begrenzten
Recordbestand und den aktiven Records abgeleitet. Ein fehlender historischer
Primary ist zulässig. Primary-Reset und Follow-up-Reset sind unabhängig.

## 9. Multi-Fault-Reset und Vertrauensgrenze

Die spätere qualifizierte FaultResetEvaluation enthält mindestens:

~~~text
targetInstanceId: uint32
targetResetAllowed: bool
causeStillActive: bool
safetyChecksPassed: bool
authorizationSatisfied: bool
otherBlockingFaultActive: bool
releaseAllowed: bool
faultRevision: uint32
rejection: stable enum
~~~

targetResetAllowed ist nur wahr, wenn Target vorhanden, CauseClear,
Targetchecks, Service-Autorisierung, expected Revision und Confirmation
stimmen. otherBlockingFaultActive blockiert nicht den Target-Reset, sondern
releaseAllowed, Recovery und Standby.

Damit gilt ohne Deadlock:

- zwei S3: einen Target-Latch sauber entfernen, der andere bleibt blockierend;
- S3 + Y4: S3 darf entfernt werden, Y4-Terminalpflicht bleibt;
- zwei Y4: jeder Target-Reset ist separat möglich, Recovery bleibt verboten;
- erst nach dem letzten Blocker und erfüllter Y4-Terminalregel darf ein
  RestartIntent oder Standby-Tombstone entstehen.

#15 prüft Envelope, erwarteten Zustand, expected FaultRevision, Target,
Confirmation und die von #24 gelieferte qualifizierte Evaluation. #15 mutiert
weder FaultCore noch SafetyState. ResetFault bleibt außerhalb des normalen
#17-persistierten Commandpfads.

Bis zu einem echten produktiven Auth-/Service-PIN-Producer gilt:

~~~text
produktiver S3/Y4-Reset = fail-closed
deterministische Testinjection = zulässig
~~~

## 10. Exakte ActuatorSafetyDirective

Der produktive Pfad lautet:

~~~text
Producer
 -> SafetyFaultService
 -> ActuatorSafetyDirective
 -> TemperatureControlApplicationOrchestrator
 -> ActuatorPlanner
 -> ActuatorPlanSinkDriver
~~~

Es gibt keine widersprüchlichen Einzelbooleans wie
peltierAllowed=true zusammen mit immediateStop=true. Der Vertrag lautet:

~~~text
SafetyGateStatus:
  0 Unresolved
  1 Allowed
  2 ImmediateStop

FanSafetyAction:
  0 PlannerManaged
  1 ForceOn
  2 ForceOff

ActuatorSafetyDirective:
  gate: SafetyGateStatus
  outerFan: FanSafetyAction
  innerFan: FanSafetyAction
  safetyRevision: uint32
~~~

`safetyRevision` ist eine ausschließlich RAM-seitige, von
SafetyFaultService vergebene Decision-Generation der Directive. Sie ist nicht
`persistentFaultRevision`, wird nicht im SafetyState gespeichert und darf von
P1/O2 nicht die persistente Revision oder InstanceId-Hochwasserstände
verbrauchen. Auch diese volatile Generation ist checked; Überlauf hält die
Directive Unresolved/ImmediateStop und erzeugt den normierten Internal-Fault.

Bei Unresolved und ImmediateStop bleiben Peltier/H-Brücke aus. Die
Aggregationsregeln für alle aktiven Ursachen sind:

~~~text
ImmediateStop > Unresolved > Allowed
ForceOff > ForceOn > PlannerManaged
~~~

Ein sourceInstanceId ist bei mehreren Ursachen nur Diagnose; die Directive
aggregiert alle aktiven Records. Ist der Planner selbst untrusted, darf die
notwendige Safety-Fanreaktion nicht PlannerManaged sein. Allowed wird intern
aus SafetyFaultService bezogen und kann nicht von UI/Web/Transport oder
Commandcaller fabriziert werden.

## 11. Sensor-, Thermal-, Fan- und Producer-Verträge

### 11.1 STALE versus FAILED

Für Safety-Air und Cooling gilt:

| Zustand | Safety-Wirkung | Persistence |
|---|---|---|
| STALE innerhalb der Producer-Erkennungs-/Recoverylogik | O2 transient, Gate Unresolved, Peltier aus/keine neue Freigabe, Restwärmestrategie | kein S3 allein durch STALE |
| FAILED, fehlend, CRC-/Bus-/Range-Fehler nach Erkennung | S3 *_SENSOR_FAILED, gelatcht, Service-Reset nach stabiler Evidenz | SafetyState |
| Contradiction/unaufgelöst | S3 S3_SENSOR_CONTRADICTION | SafetyState |
| Productsensor temporär nicht verfügbar | bestehende #21-O2-/Fallback-Policy | nicht in SafetyState |

Die genaue Erkennung kommt aus #20/#21. #24 bildet Status und Source ab, baut
keine parallele Sensor-FSM.

### 11.2 Configuration

#24 konsumiert die realen Producer aus #56/#57:

| Producerstatus | Catalog | Wirkung |
|---|---|---|
| ConfigurationRuntimeFailure | Y4_CONFIGURATION_RUNTIME | ImmediateStop, SAFE_BOOT/Terminal nach Runbezug |
| ConfigurationUnavailable | Y4_CONFIGURATION_UNAVAILABLE | ImmediateStop, kein Start/Release |
| ConfigurationIntegrityFailure | Y4_CONFIGURATION_INTEGRITY | ImmediateStop, kein Factory Reset |
| ConfigurationCommitIndeterminate | Y4_CONFIGURATION_COMMIT_INDETERMINATE | fail-closed bis Readback/Recovery |
| nicht auflösbares CommitOutcomeUnknown | Y4_CONFIGURATION_COMMIT_INDETERMINATE | keine alte/neue Config behaupten |

Configuration-Werksreset löscht SafetyState nicht. #24 dupliziert keine
Configuration-State-Machine.

### 11.3 Thermal- und Fan-Grenze

Safety-Intervention bedeutet aktuelle Richtung AUS, accumulator verwerfen,
Integrator sperren, Mindest-Auszeit/Totzeit abwarten und erst nach neuen
Checks eine begrenzte, katalogseitig erlaubte Gegenrichtung ausführen. Hard
Emergency bedeutet beide Richtungen AUS und keine Gegenrichtung. Beide
Policies bleiben gelatcht.

Y4 => beide Fans AUS ist verboten. Fans bleiben ursachenspezifisch. Eine
elektrische Fanursache darf nicht mit ForceOn übersteuert werden. Eine
funktionale Ursache darf ForceOn nur über den Safety-Pfad anfordern, wenn die
elektrische Ausgabe nicht als unsicher klassifiziert ist. Es gibt keine
physische Rückmeldung ohne realen Producer.

## 12. RestartIntent und RestartSupervisor

### 12.1 Wirewerte

~~~text
RestartIntentState:
  0 None
  1 Prepared

RestartIntentKind:
  0 None
  1 S3Recovery

RestartRequestOutcome:
  0 None
  1 Requested
  2 Rejected

ResetCause:
  1 PowerOn
  2 SoftwareRestart
  3 WatchdogOrPanic
  4 Brownout
  5 External
  6 Unknown
~~~

Null-/Reservedwerte sind Decode-Fehler, mit Ausnahme der ausdrücklich
definierten None-Werte. sourceInstanceId ist bei Prepared nicht null und
kleiner als nextPersistentInstanceId.

### 12.2 S3-Intent

Wenn ein aktiver Run nach Target-Reset keine aktiven S3/Y4-Blocker mehr hat,
runRecoveryForbidden=false und die Ursache CauseClear ist, werden Target-
Latch-Entfernung und

~~~text
RestartIntentState=Prepared
RestartIntentKind=S3Recovery
RestartIntentSourceInstanceId=target/source instance
~~~

in einem SafetyState-Kandidaten linearisiert. RAM bleibt Fault. Es gibt genau
einen automatischen Restartrequest für diese Episode.

`restartRequestOutcome=None` wird beim Prepared-Commit gesetzt. Ein erfolgreich
ausgelöster Request setzt ihn in demselben SafetyState-Kandidaten auf
`Requested`; ein vom Reset-Port abgelehnter Request setzt ihn auf `Rejected`.
Ein bereits gesetztes `Requested` oder `Rejected` darf keinen zweiten
automatischen Request auslösen. `Rejected` erhöht keinen Retryzähler und
startet keinen Auto-Retry. Der Intent bleibt Prepared; Fault und ImmediateStop
bleiben. Ein späterer manueller oder externer Reboot darf den Intent übernehmen.

Der Intent wird erst gelöscht, wenn eine neue dauerhafte Run-Wahrheit vorliegt:

- bestehender #18-Recovery-/Resume-Kandidat ist forward committed; oder
- kanonischer NoActiveRun-Tombstone ist committed und readbackbestätigt.

Crash nach einem #18-Commit und vor Intent-Clear findet den bereits
weitergeschriebenen Current-Head vor. Es erfolgt kein zweiter Fallback-Select;
der normale #18/#17-Pfad setzt fort und löscht den Intent nach Qualifikation.
Ein ungültiger Fallback führt zu Terminalisierung; Intent-Clear folgt erst dem
Tombstone.

### 12.3 ResetCause, abnormal boots und Stabilität

~~~text
valid Prepared S3Recovery + expected SoftwareRestart = controlled, not abnormal
Brownout/WatchdogOrPanic/Unknown = abnormal
SoftwareRestart ohne gültigen Intent = abnormal
Prepared + unerwartete ResetCause = abnormal
PowerOn/External ohne Intent = nicht abnormal
~~~

PowerOn oder External ohne Intent löschen einen bestehenden
`abnormalRestartCount` nicht allein; die oben definierte 30-Minuten-
Stabilitätsqualifikation bleibt erforderlich.

abnormalRestartCount zählt saturierend 0..3. Der Übergang auf 3 erzeugt
Y4_RESTART_LOOP, setzt safeBootRequired=true und commitet atomar.

Für Count 1/2 setzt eine aktuelle Bootinstanz den Zähler erst nach 30 Minuten
stabiler bootlokaler Zeit auf null, wenn gleichzeitig kein neuer abnormaler
Reset, gesunder redundanter SafetyState/Marker, kein aktiver S3/Y4 und kein
offener RestartIntent vorliegen. Bei Count 3/Y4 führen 30 stabile Minuten nur
zu CauseClear-Evidenz; der Zähler bleibt 3. Erst Service-Reset setzt ihn auf
null, SAFE_BOOT bleibt bis zum separaten Exit.

bootSequence ist uint32_t, wird genau einmal je Boot checked erhöht und bei
Overflow als Y4/fail-closed/SAFE_BOOT behandelt. Keine monotone Zeit wird über
Reboots subtrahiert.

## 13. Kanonische Bootreihenfolge und SAFE_BOOT

Die Composition Root führt exakt diese Reihenfolge aus:

~~~text
1  Ausgänge fail-closed AUS
2  EmergencyMarker beide Slots scannen
3  SafetyState beide Slots scannen und Redundanzstatus bestimmen
4  bootSequence, ResetCause, RestartIntent und Restartcounter qualifizieren
5  #57 ConfigurationRecovery einschließlich FactoryNoveltyProof
6  #56 ConfigurationRuntime und alle Configuration-Gates
7  #17 RunPersistenceCoordinator::loadAndInitialize()
8  #20/#21 Sensor-/Safety-Evidenz laden
9  Producer -> SafetyFaultService/Catalog mappen
10 runRecoveryForbidden und S3RecoveryIntent auswerten
11 SAFE_BOOT-Entscheidung treffen
12 nur normal und qualifiziert: RAM-only Fallback -> #18
13 erst nach vollständiger Qualifikation normalen Planner-/Sink-Gate-Schritt ausführen
~~~

Vor Schritt 13 gibt es keinen Planner-Tick, keinen Sink-Write und kein
caller-supplied Allowed.

### 13.1 SAFE_BOOT-Eintritt

SAFE_BOOT wird mindestens ausgelöst durch:

- fehlenden SafetyState außerhalb echter Factory-Initialisierung;
- safeBootRequired;
- abnormal count 3;
- SafetyState-/Marker-Indeterminate oder notwendige Redundanzreparatur;
- RunPersistence NotReconstructible, PreparedInterrupted, Read-/Schema-/
  Epoch-/Capacity-Fehler;
- kritische Configuration-Integrity/Runtime/Commit-Indeterminate;
- unknown Safety-Evidence oder FaultCatalog-/Counter-Overflow.

Ist SafetyState noch schreibbar, wird safeBootRequired=true vor der RAM-
Transition persistiert und readbackbestätigt. Ist SafetyState die Ursache,
halten Marker-/Storefehler den Zustand fail-closed.

### 13.2 SAFE_BOOT-Exit

Alle Bedingungen müssen gleichzeitig gelten:

- ProcessState ist SafeBoot;
- safeBootRequired=true ist qualifiziert;
- Markerstatus Cleared/healthy und beide SafetyState-Slots redundant gesund;
- keine aktiven S3/Y4-Records;
- runRecoveryForbidden=false;
- kein Prepared RestartIntent;
- #56 operational, #57 qualifiziert;
- #17 deterministic, NoActiveRun/STANDBY, kein BlockedIndeterminate;
- Air/Cooling gültig, SensorSelection resolved, Planner/Watchdog healthy;
- Service-Autorisierung für den Exit liegt vor.

Dann werden die Bedingungen erneut geprüft, safeBootRequired=false als
SafetyState-Commit/readback linearisiert, SafeBootExitCompleted nur RAM-seitig
markiert und anschließend Standby gesetzt. Erst ein weiterer normaler
Gate-Evaluationsschritt darf danach den Plannerpfad betrachten. Ein Crash nach
dem Flag-Commit und vor RAM-Transition bootet erneut fail-closed und repariert
den RAM-Zustand; es gibt keine implizite Aktorfreigabe.

## 14. S3-Recovery: mechanischer RAM-only Handoff

### 14.1 Fault-Eintritt und Reset

Bei aktivem Run und S3:

1. SafetyFaultService bildet die Ursache, Gate wird sofort ImmediateStop;
2. S3-Record wird im SafetyState committed/readbackbestätigt;
3. bei SafetyState-Commitfehler gilt EmergencyMarker und SAFE_BOOT;
4. TransitionReason::CriticalFault führt den Prozess nach Fault;
5. #17 schreibt den Fault-Checkpoint mit Standard-Fallback-Rotation:
   current=Fault checkpoint, fallback=previous valid checkpoint.

CauseClear und aktuelle Sensor-/Safetychecks reichen nicht für Reset. Ein
Service-Target-Reset entfernt den S3-Record erst über SafetyState-Commit. RAM
bleibt Fault. Nur wenn danach kein S3/Y4-Blocker und
runRecoveryForbidden=false bestehen, wird der Prepared S3Recovery-Intent als
derselbe SafetyState-Kandidat vorbereitet.

### 14.2 API-Vertrag

Die spätere schmale API, z. B. prepareSafetyFallbackRecovery(...), darf nur
lesen, verifizieren und RAM-Zustand wählen. Voraussetzungen:

- gültiger Committed-Head;
- current ist ein gültiger aktiver Fault-Checkpoint;
- Fallback-Referenz vorhanden;
- Slot, Schema, Safety-/Run-Epoch, Revision, Payloadlänge, CRC und Variant
  stimmen mit der Referenz überein;
- Fallback ist aktiver Program- oder Manual-Run;
- activeRunId, Program-/Manual-Snapshot und Run-Kontext stimmen zwischen
  Current und Fallback überein; lediglich monotone Checkpoint-/Runrevisionen
  dürfen den älteren Fallback unterscheiden;
- runRecoveryForbidden=false, gültiger S3Recovery-Intent, kompatible
  ResetCause, qualifizierte #56/#57-Konfiguration, #17-Status
  LoadedActiveRun/FallbackRecoveryPending und keine aktiven S3/Y4 nach
  Safety-Qualifikation.

Die API muss exakt die Bookkeepingdaten des normalen Fallback-Loads herstellen:

- Fallback-Rohrecord in vorhandenen RAM-Slot laden;
- persistedIds_ und persistedIdCount_ restaurieren;
- physische Checkpoint-/Head-High-Watermarks nicht absenken;
- Zustand FallbackRecoveryPending setzen;
- Snapshot an activateFallbackRecoveredRun() übergeben.

Sie darf nicht:

- Head, Slot oder Marker schreiben;
- Current und Fallback vertauschen;
- Payload kopieren oder Revision zurücksetzen;
- Schema ändern;
- eine Recoveryentscheidung treffen;
- activateLoadedRun() als neue Resumequelle für Fault-Current aufrufen;
- #18 duplizieren.

Zwischen Restore und #18-Hop-1 gibt es keinen Temperature-Control-Tick,
Planner-Tick, Sink-Write oder normalen Run-Command.

### 14.3 #18 und Zeit

#24 wählt ausschließlich die physisch/semantisch verifizierte Quelle. #18
entscheidet UTC, bootlokale Zeit, Sensorwahl, Temperatur-Evidenz, Progress,
QualifyingTarget, WaitingForProduct, ReachingTarget, Fermenting, Cooling,
CoolHolding, ManualHolding sowie Resume oder Terminal.

~~~text
t0 = letzter gültiger Pre-Fault-Checkpoint
t1 = tatsächlicher Fault-Eintritt
t2 = Recovery-Boot/Anker
~~~

t0..t1 ist konservative Recovery-Unsicherheit; t1..t2 ist Recovery-
Unterbrechung. Kein Abschnitt wird als normale biologische Laufzeit erfunden.
Fehlender Fallback, falscher Run, korruptierter Fallback oder nicht beweisbare
#18-Fortsetzung führt zum terminalen Tombstone.

## 15. Y4-Terminalisierung und Fault -> Standby

### 15.1 Y4-Eintritt

Bei Y4 mit aktivem Run wird atomar im SafetyState gesetzt:

~~~text
Y4 latch present
runRecoveryForbidden = true
~~~

Das geschieht beim Raise, nicht erst beim Reset. CauseClear oder das Entfernen
des Y4-Records löscht die Terminalpflicht nicht.

### 15.2 Normaler Fault-Pfad

Der aktuelle FSM besitzt noch keinen normalen Fault->Standby-Event. Der Plan
führt hierfür den expliziten Vertrag ein:

~~~text
ProcessEvent::FaultResetCompleted
TransitionReason::FaultResetCompleted
~~~

Er ist nur zulässig, wenn:

- alle targetbezogenen S3/Y4-Resets qualifiziert und committed sind;
- keine aktiven S3/Y4-Records verbleiben;
- die Service-Autorisierung für Terminalisierung vorliegt;
- die alte Run-Recovery durch runRecoveryForbidden ausdrücklich verboten und
  daher ein Tombstone erforderlich ist.

Dann gilt atomar in der fachlichen Reihenfolge:

1. Prozesskandidat Fault -> Standby;
2. clearActiveRunState(candidate) nur auf diesem Standby-Kandidaten;
3. Snapshot NoActiveRun + ProcessState::Standby erzeugen;
4. #17 Prepared-Head -> physischer Checkpointslot -> Committed-Head;
5. CAS/Readback und Referenzprüfung;
6. erst danach runRecoveryForbidden=false im SafetyState committen.

Es wird nie NoActiveRun + Fault geschrieben. Crash vor dem Tombstone oder
zwischen Tombstone und Flag-Clear lässt die Terminalpflicht erhalten. Crash
nach bestätigtem Tombstone findet NoActiveRun als Run-Wahrheit vor; ein Repair
darf das Flag anschließend löschen, aber nicht einen alten Run öffnen.

### 15.3 SAFE_BOOT-Tombstone

In SAFE_BOOT bleibt der RAM-ProcessState SafeBoot und der Planner gesperrt.
Wenn alle Bedingungen für autorisierte Run-Terminalisierung vorliegen, baut
der bestehende #17-Kern einen technischen Tombstone-Kandidaten mit
NoActiveRun und persistiertem ProcessState::Standby; dieser Persistenz-
Kandidat ist kein NoActiveRun + SafeBoot und ändert RAM-SafeBoot nicht.

Der Tombstone muss dieselben sechs laufgebundenen Recovery-/Progressfelder
leer lassen, die der aktuelle Schema-3-Validator für NoActiveRun verlangt.
Die zwei bestehenden nicht-laufgebundenen Schema-3-Felder
recoveryTemperatureEvidence und recoveryEpisodeRevision werden auf ihren
kanonischen Null-/Defaultwert gesetzt; es wird kein Laufkontext erhalten.
Erst nach Tombstone-Readback darf runRecoveryForbidden gelöscht werden.
SAFE_BOOT bleibt bis zum separaten vollständigen SAFE_BOOT-Exit aktiv.

## 16. Run-Abandon und vollständige Statusabbildung

### 16.1 Load-Status

| #17-Status | #24-Code/Source | Faultklasse | Terminal/SAFE_BOOT | CauseClear |
|---|---|---|---|---|
| NoPersistedRun | keiner | keine | nein, nur bei gesundem SafetyState | nicht nötig |
| Current | keiner; Fault-Snapshot wird separat qualifiziert | keine oder Snapshot-Fault | nur Safety-/Run-Gate | Producer/Fault-abhängig |
| FallbackRecovered | keiner | keine | kein automatisches Resume | #18 entscheidet |
| PreparedInterrupted | 0x4103/RunPersistence | Y4 | ja | Store-/Tombstone-Oracle |
| NotReconstructible | 0x4101/RunPersistence | Y4 | ja | Store-/Tombstone-Oracle |
| NotReconstructibleOrphanedState | 0x4101/RunPersistence | Y4 | ja | Store-/Tombstone-Oracle |
| ReadFailed | 0x4102/RunPersistence | Y4 | ja | eindeutiger Readback |
| CapacityExceeded | 0x4102/RunPersistence | Y4 | ja | technische Kapazität |
| UnsupportedSchema | 0x4101/RunPersistence | Y4 | ja | ausdrücklich unterstützte Version |
| ForeignEpoch | 0x4101/RunPersistence | Y4 | ja | richtige Domäne/Readback |
| AlreadyInitialized im Bootpfad | 0x410b/SafetyFaultService | Y4 | ja | Composition-/Lifecycle-Oracle |

Current mit ProcessState::Fault ist keine Resumeentscheidung. Der normale
Current-Pfad bleibt Fault, bis ein gültiger S3Recovery-Intent die explizite
Fallback-API aktiviert. FallbackRecovered bedeutet nur geladene technische
Quelle, nicht Resume.

### 16.2 Runtime-Status

| Runtime-Status | Abbildung | Wirkung |
|---|---|---|
| Applied | keiner | Forward-Commit bestätigt |
| WriteFailed | Y4_RUN_PERSISTENCE_FAILURE | bei erforderlichem Fault-/Tombstone-/Recovery-Commit fail-closed; rein periodischer bekannter No-Write bleibt bestehender #17-Status |
| CapacityExceeded | Y4_RUN_PERSISTENCE_FAILURE | keine Teilmutation, SAFE_BOOT bei Boot-/Safetyentscheidung |
| PersistenceIndeterminate | Y4_RUN_PERSISTENCE_INDETERMINATE | BlockedIndeterminate, kein Resume/Tombstone ohne neue Qualifikation |
| PersistenceCommittedApplyFailed | Y4_RUN_PERSISTENCE_INDETERMINATE | Readback/Run-Wahrheit maßgeblich, kein Replay |
| CounterOverflow | Y4_INTERNAL_SAFETY oder Y4_RUN_PERSISTENCE_FAILURE je betroffenem Zähler | kein Wraparound, SAFE_BOOT |
| TimeWentBackwards | Y4_RUN_PERSISTENCE_FAILURE bei Recovery-/Safety-Commit, sonst bestehende #18-Ablehnung | kein erfundener Fortschritt |
| InvalidDecision | kein neuer Fault, Gate bleibt unverändert | kein Write, Caller-/Contractfehler |
| Busy | kein neuer Fault | kein Tick/Write; bounded retry nur im Aufrufervertrag |
| BlockedIndeterminate | Y4_RUN_PERSISTENCE_INDETERMINATE | SAFE_BOOT, schmaler Abandon-Handoff |
| RecoveryPending | kein eigener Fault | kein Planner/Sink; #18-Handoff erforderlich |

### 16.3 Abandon-Pfad

Für NotReconstructible, NotReconstructibleOrphanedState, PreparedInterrupted,
ReadFailed, CapacityExceeded, UnsupportedSchema und ForeignEpoch gilt: kein
Raten, kein Runresume, kein Factory Reset, Programme und Configuration bleiben
erhalten.

Ein technisch gültig geladener aktiver Run wird bei erforderlicher
Terminalisierung nicht verworfen: Er folgt dem in Abschnitt 15 normierten
`FaultResetCompleted`-Pfad zu `Standby`, `clearActiveRunState()` und dem
kanonischen Tombstone. Nur die ausdrücklich nicht rekonstruierbaren oder
indeterminierten Fälle verwenden den schmalen Abandon-Handoff unten.

Bei technisch eindeutig wieder les-/schreibbarem Store verwendet der bestehende
#17-Kern eine schmale autorisierte Tombstone-API:

~~~text
NoActiveRun
ProcessState::Standby
activeRunId/sensor/program/manual/recovery fields leer
forward-only technische Revisionen
Prepared Head
physischer Slot
Committed Head
CAS/readback
erst danach RunPersistence-CauseClear
~~~

Für PreparedInterrupted wird die rohe, tatsächlich gelesene Head-Bytefolge als
CAS-Preimage verwendet. Die alte Prepared-Transaktion wird nie blind committed
und nie als alter Run fortgesetzt. Alle technisch lesbaren Head-, Checkpoint-
und Slotrevisionen werden gescannt; max+1 ist checked, Overflow ist Y4.
Fremde Epochs/unsupported Schemas werden nicht interpretiert; ein autorisierter
technischer Discard darf nur in einen neuen validierten NoActiveRun-Tombstone
führen.

## 17. Command-, Auth- und Bypass-Inventar

Alle öffentlich konstruierbaren Safetyinputs werden als untrusted behandelt:

| Input | aktueller Vertrag | neue Grenze |
|---|---|---|
| ProgramStartRequest.safetyAllowsStart | caller-supplied bool | aus produktiver Startentscheidung entfernen, SafetyView intern |
| ManualStartRequest.safetyAllowsStart | caller-supplied bool | aus produktiver Startentscheidung entfernen |
| StopRequest.safetyAllowsCooling | caller-supplied bool | interne Directive + #21-Sensororacle |
| CompletionRequest.safetyAllowsCooling | caller-supplied bool | interne Directive + #21-Sensororacle |
| RunAdjustmentCommandRequest.safetyAllowsChange | caller-supplied bool | interne Safety-/Run-Revision-Prüfung |
| SensorSelectionCommandRequest.safetyAllowsChange | caller-supplied bool | interne Safety-/#21-Prüfung |
| RunAdjustmentContext.safetyAllowsChange | öffentlich konstruierbarer Kontext | nur intern erzeugte SafetyView, kein Caller-Grant |
| Start/Stop/Completion `airSensorValid`, `coolingSensorValid`, `productSensorValid` | caller-supplied Evidenzflags | ausschließlich kanonische #20/#21-Snapshots, `true` nicht vertrauenswürdig |
| FaultResetEvaluation.authorizationSatisfied | caller-supplied bool | qualifizierter Service/Auth-Producer |
| ActuatorPlanTickInput.safetyGate.status | öffentlich konstruierbares Gate | nur Orchestrator-Projektion; `Allowed` von außen wird abgelehnt |
| ActuatorWatchdogFaultEvidence / ProcessSignals.criticalFault | externe bzw. Prozessprojektion | SafetyFaultService qualifiziert Producer-Evidenz, keine Parallel-Faultwahrheit |
| RunCommandState.criticalSafetyEventPending | mutable Projektion | nur aus SafetyFaultService ableiten, nicht als zweite Autorität |
| Planner-/Sink-Gate | potenziell frei projiziert | Orchestrator holt Directive zentral |

Transport/UI/Web dürfen Allowed, safetyAllows...=true oder
authorizationSatisfied=true nicht produktiv fabrizieren. Tests verwenden
deterministische typisierte Injections, die im Produktionsbuild nicht als
Producer verdrahtet werden.

## 18. SafetyEvents und Ressourcen-/Wear-Beweis

### 18.1 Bounded Events

Die festen Eventtypen sind:

~~~text
0x01 FaultRaised
0x02 CauseCleared
0x03 FaultRelapsed
0x04 FaultReset
0x05 RestartPrepared
0x06 RestartRejected
0x07 SafeBootRequired
0x08 SafetyPersistenceIndeterminate
0x09 TerminalRecoveryForbidden
0x0a ActuatorGateChanged
~~~

Ein SafetyEvent ist exakt 12 Byte: instanceId:u32, safetyRevision:u32,
faultCode:u16, faultSource:u8, kind:u8. Eine Mutation kann höchstens 27
Identity-Events, einen Gate-Event, einen Restart-Event und einen Persistenz-
Event erzeugen:

~~~text
kMaxSafetyEventsPerMutation = 27 + 1 + 1 + 1 = 30
sizeof(SafetyEventBatch) = 30 * 12 = 360 Byte
~~~

Für Gate-, Restart- und Persistenzereignisse sind instanceId, faultCode und
faultSource jeweils 0; Fault-Ereignisse tragen die konkrete Identity. Eine
`std::array<SafetyEvent,30>` und static_assert sind verbindlich. Ein
überschrittenes Limit ist ein Fehler/fail-closed, niemals stille Trunkierung.
Keine dynamische Queue, kein allgemeiner Eventbus, keine Journalpersistenz in
#24; #19 bleibt Historieneigentümer.

### 18.2 Safety-Core-RAM und Persistenzpuffer

Die Implementierung weist folgende Obergrenzen nach:

| Bereich | exakte Obergrenze |
|---|---:|
| 23 PersistentFaultRecord, je 24 Byte | 552 Byte |
| FaultCore-Skalare/Alignment | 216 Byte |
| FaultCore gesamt, statisch geprüft | 768 Byte |
| Eventbatch | 360 Byte |
| ActuatorSafetyDirective | höchstens 24 Byte |
| SafetyState Payload | 598 Byte |
| SafetyState max Envelope | 1024 Byte |
| Marker max Envelope | 64 Byte |
| Safety-Commit old/new/readback scratch | 3 * 1024 = 3072 Byte |
| Marker bounded scratch | 2 * 64 = 128 Byte |
| neue Safety-RAM-Obergrenze | 768 + 360 + 24 + 3072 + 128 = 4352 Byte |

static_assert begrenzt den neuen Safety-Core auf 4608 Byte und jeden
Einzelpuffer auf die oben genannte Größe. SafetyFaultService, Codec,
EmergencyMarker, Commitpfad und Eventprojektion allokieren im Hot Path nicht
dynamisch; sie verwenden die genannten festen Puffer. Eine vorhandene
`std::string`-Grenze darf nur außerhalb dieses Pfads oder mit nachgewiesenem
festem Vorabpuffer verwendet werden; Allokationsfehler bleiben fail-closed.
Kein PSRAM, keine Annahme über zusätzliche 4-MB-Ressourcen.

### 18.3 Flash-Wear und Write Bound

| Mutation | SafetyState writes | Marker writes bei Commitfehler | weitere Writes |
|---|---:|---:|---|
| Raise | 1 | höchstens 2 | Readback pro Versuch |
| CauseClear | 1 | höchstens 2 | Ack schreibt nichts |
| Relapse | 1 | höchstens 2 | kein Retry-Loop |
| Target-Reset | 1 | höchstens 2 | Intent ggf. derselbe Kandidatencommit |
| Restart Prepared/Outcome | 1 | höchstens 2 | Request selbst nicht persistent wiederholen |
| Bootcounter/Stable-Update | 1 | höchstens 2 | nur nach definierter Bootaktion |
| Redundanzrepair | höchstens 1 Peer-Write je Familie | höchstens 1 Marker-Peer-Write | Readback beider Slots |
| Factory-Initialisierung | höchstens 2 SafetyState-Slot-Writes | höchstens 2 Marker-Slot-Writes | nur bei echter Factory-Evidenz |

Ein bekannter WriteError/CapacityError wird nicht wiederholt. Ein
CommitOutcomeUnknown wird nur per vorgeschriebenem Readback und beim Marker
einmal auf dem zweiten Slot behandelt. Es gibt keinen persistenten Ack-Write.

## 19. Mandatory Injection-/Producer-Matrix

Jeder Fall bekommt in der Implementation genau einen Status aus der Spalte
Verantwortung; kein Hardwarefall wird als PASS behauptet.

| Injection/Fall | Verantwortung | Mapping |
|---|---|---|
| Sensor CRC/Bus/Read | bestehender #20 Producer konsumiert | SafetyAir/Cooling FAILED -> S3 |
| missing/out-of-range safety sensor | bestehender #20 Producer konsumiert | nach Erkennung S3, vorher Unresolved/O2 |
| stuck/repeated | #20/#21 Producer konsumiert | STALE O2 oder FAILED S3 nach Oracle |
| stale Air/Cooling | #20/#21 Producer konsumiert | O2 transient, kein S3 allein |
| failed Air/Cooling | #20/#21 Producer konsumiert | persistenter S3 |
| Sensor contradiction | #20/#21 Producer konsumiert | S3 contradiction |
| Safety intervention | #20/TemperatureControl konsumiert | S3 intervention, max 2 |
| hard emergency | #20/TemperatureControl konsumiert | S3 hard, keine Gegenrichtung |
| invalid runtime/config sizes | #56/#57 konsumiert | ConfigurationRuntime Y4 |
| ConfigurationRuntimeFailure | #56 konsumiert | Y4 |
| ConfigurationUnavailable | #57 konsumiert | Y4 |
| ConfigurationIntegrityFailure | #57 konsumiert | Y4 |
| CommitOutcomeUnknown/Indeterminate | #56/#57 konsumiert | Y4 config commit |
| simultaneous H-bridge directions | #22/#23 producer | S3 Peltier/H-Bridge |
| invalid/unknown ActuatorPlan | #23 producer | S3 watchdog/planner |
| planner watchdog | #23 producer | S3 watchdog |
| implausible actuator feedback | injection-only bis realer Producer | S3/HW injection |
| fan off during Peltier | #23/injection-only | Fan functional/electrical nach Evidenz |
| Peltier/H-bridge fault | injection-only bis echter Producer | S3 Peltier |
| missing thermal response | #20/TemperatureControl producer | intervention oder hard policy |
| interrupted SafetyState write | #24 test support | Y4 SafetyState/Marker |
| interrupted Marker write | #24 test support | Y4 SafetyState |
| interrupted RunPersistence write | #17 test support konsumiert | Y4 RunPersistence |
| repeated watchdog reset | ResetSupervisor injection | Y4 RestartLoop |
| Brownout/Unknown reset | ResetCause injection | abnormal count |
| invalid persisted enum | codec injection | Y4 Unknown/Store |
| impossible persisted state | codec injection | Y4 Unknown/Store |
| interrupted multi-object/config write | #56/#57 injection | Y4 config indeterminate |
| PreparedInterrupted | #17 injection | Y4 RunPersistence, Tombstone only |
| corrupt current + valid fallback | #17 injection | explicit S3 fallback only |
| corrupt fallback | #17 injection | terminal, kein Resume |
| one-slot SafetyState corruption | #24 injection | valid Slot laden, Y4 + repair |
| total SafetyState loss | #24/#57 Factory injection | Factory only empty, sonst Y4/SAFE_BOOT |
| Active/corrupt Marker | #24 injection | Active/fail-closed/repair |
| full Fault capacity | #24 injection | no eviction, Y4 Internal |
| counter overflow | #24 injection | Y4 Internal, kein Wraparound |

## 20. Testmatrix: Lifecycle und Multi-Fault

Pflichtfälle:

- duplicate Raise derselben Identity ohne neue InstanceId;
- simultane alle erlaubten persistenten Identities;
- unabhängige unbekannte Ursachen ohne künstliche Primary-Verknüpfung;
- CauseClear, Relapse und stale ResetEvaluation;
- Target-Reset trotz anderem Blocker;
- zwei S3, S3+Y4 und zwei Y4;
- Primary-/Follow-up-Reihenfolge und unabhängige Resets;
- Ack/Mute ohne Gate-/Revision-/Latch-Wirkung;
- P1/O2 ohne persistenten ID-/Revision-Verbrauch;
- Reboot erhält S3/Y4-Latches, runRecoveryForbidden, FaultRevision und
  Prepared Intent;
- persistentFaultRevision bei UINT32_MAX und InstanceId-Grenze;
- unbekannte FaultCode/Source, Reserved-Bits, doppelte/unsortierte Records.

## 21. Testmatrix: S3-Recovery

Für jede Phase Preheating, WaitingForProduct, ReachingTarget,
QualifyingTarget, Fermenting, Cooling, CoolHolding und ManualHolding:

- S3 Raise -> Fault-Checkpoint und vorheriger Fallback;
- CauseClear + Service-Target-Reset;
- letzter S3 ohne Y4 -> Prepared Intent und genau ein Restartrequest;
- mehrere S3 -> Target-Reset ohne Recoveryfreigabe bis zum letzten Blocker;
- current Fault bleibt current, keine normale activateLoadedRun()-Resumequelle;
- RAM-only Selection erzeugt keinen Store-/Slot-/Head-Write;
- persistedIds_, High-Watermark und Fallback-State sind wie beim normalen Load
  restauriert;
- kein Planner-/Sink-Tick zwischen Restore und #18;
- Fallback wird an unverändertes #18 übergeben;
- konservative t0..t1-/t1..t2-Unsicherheit ohne #24-Progress;
- fehlender, korruptierter, falscher oder Y4-verbotener Fallback -> Terminal;
- Crash vor/nach SafetyState-Commit, Fault-Run-Commit, Intent-Commit,
  Restartrequest, Boot-Handoff und #18-Commit;
- Crash nach #18-Forward-Commit vor Intent-Clear -> kein Replay/kein zweiter
  Fallback-Select;
- Restart Rejected, Prepared + External/Power/Brownout/Unknown Cause;
- Y4-Flag verhindert jede S3-Recovery.

## 22. Testmatrix: Y4, SAFE_BOOT und Restart

- echte Factory-Initialisierung mit FactoryNoveltyProof;
- fehlender SafetyState auf Nicht-Factory-Gerät;
- ein SafetyState-Slot defekt, beide defekt, gleiche Revision widersprüchlich;
- Marker Active, Cleared, korrupter Slot, Repair und OutcomeUnknown;
- SafetyState-/Marker-Revision- und bootSequence-Overflow;
- alle sechs ResetCause-Werte mit gültigem/ungültigem Prepared Intent;
- Counter 0/1/2/3, 30-Minuten-Count-Reset und Count-3-Service-Semantik;
- Y4 Restart-Reset, offene Intentzustände, Reboot mit Rejected;
- safeBootRequired, alle Exitbedingungen und jeder fehlende Exitblocker;
- Crash nach safeBootRequired=false vor RAM-Transition;
- Y4 Tombstone vor/zwischen/nach Prepared/Slot/Committed/SafetyState-Clear;
- SAFE_BOOT-Tombstone ohne NoActiveRun + SafeBoot;
- späterer S3-Reset kann alten Y4-Run nicht recovern.

## 23. Testmatrix: Thermal, Fan, Actuator und Trust

- Air/Cooling STALE gegenüber FAILED;
- Product O2 und #21-Fallback;
- contradiction;
- Intervention boundary, fehlende Commissioningparameter, null/1/2 Versuche;
- hard emergency ohne Gegenrichtung;
- Outer-Fan functional gegenüber electrical;
- Inner-Fan functional gegenüber electrical;
- Planner untrusted und Safety-Fanreaktion ohne PlannerManaged;
- ForceOff/ForceOn/PlannerManaged-Aggregation mehrerer Faults;
- Peltier/H-Bridge fault, watchdog, unconfirmed hardware;
- jeder Y4 hält Peltier/H-Brücke sicher AUS;
- jede öffentlich konstruierbare safetyAllows..., Allowed- und
  authorizationSatisfied-Umgehung;
- Service-only Reset, falsche/fehlende Target-InstanceId, stale Revision;
- Application-/Planner-/Sinkpfad ohne zentral bezogene Directive.

## 24. Implementation Slices nach Ownerfreigabe

1. Fault types, Catalog und Lifecycle: explizite Codes/Sources/Policies,
   Pure Core, Raise/CauseClear/Relapse/Reset/Primary.
2. SafetyState, Marker und Repair: Recordtypen 9/10, Codec, zwei Slots,
   Factory-Proof, Scan, Readback, EmergencyMarker, Größen-/Wear-Gates.
3. RestartSupervisor und SAFE_BOOT: ResetCause, Counter, Bootsequenz,
   Intent, 30-Minuten-Regel und Exitmatrix.
4. Run-/Process-Integration: S3 RAM-Handoff, Y4-Terminalflag,
   FaultResetCompleted, Tombstone, Blocked-Abandon.
5. #20/#21/#22/#23-Producer und Thermal/Fan-Directive: bestehende Producer
   konsumieren, Intervention/Hard-Emergency trennen.
6. #15 Target-Reset, Multi-Latch und Auth: trusted Evaluation,
   Target-Identity, Release-vs-Reset, Service-Gate.
7. #56/#57 und Boot Composition: reale Configuration-Gates sowie die
   vollständige Bootreihenfolge verdrahten.
8. No-bypass Command-/Actuatorpfad: caller-supplied Safetybooleans aus dem
   produktiven Gate entfernen, zentral aggregierte Directive erzwingen.
9. Kanonische Dokumentation und vollständiges Review: betroffene Verträge,
   Acceptance-Matrix, Roadmap und finalen Diff synchronisieren.

Keine fundamentale Safetyentscheidung wird in die Implementationsphase
verschoben. Jeder materielle Vertragswiderspruch erzeugt vor dem nächsten
Slice eine neue Planrevision.

## 25. Dokumentationsscope nach Implementation

Nach Umsetzung werden nur die betroffenen kanonischen Quellen aktualisiert:

~~~text
docs/SAFETY_AND_FAULTS.md
docs/SAFETY_COMPONENT_FAULTS.md
docs/SYSTEM_SAFETY_AND_RECOVERY.md
docs/ACCEPTANCE_TESTS.md
docs/RUN_COMMANDS.md
docs/STATE_MACHINE.md
docs/RUN_PERSISTENCE.md
docs/RECOVERY_AND_INTERRUPTION.md
docs/ACTUATOR_TIMING.md
docs/ACTUATOR_TIMING_AND_FANS.md
docs/CONFIGURATION_PERSISTENCE.md
docs/ARCHITECTURE.md
docs/ROADMAP.md
~~~

Keine Reviewchronik wird in normative Dokumente kopiert. #19 bleibt Eigentümer
der Langzeithistorie.

## 26. Architekturproof am Basiscommit

Die folgenden Nachweise sind direkt am aktuellen origin/main geprüft und
werden in der Implementation als Akzeptanztests wiederholt:

1. CriticalFault erhält den bestehenden Fallback:
   `run_persistence_coordinator.cpp:1404-1554` erzeugt für einen aktiven
   Fault-Snapshot den Prepared-/Committed-Vorwärtspfad und referenziert bei
   `UseStandardFallback` den bisherigen `currentHead_->current` als Fallback.
2. Kein #17-Head-Rollback: `run_persistence_codec.cpp:1370-1417` prüft
   Referenzen und monotone Run-/Transitionrevisionen; der Storepfad schreibt
   `Prepared -> Checkpointslot -> Committed` und senkt keine Revision.
3. Kein #17-Schemawechsel für S3: `run_persistence_contract.hpp:19-30`
   bestätigt Schema 3; Current, Fallback, Recovery-Mutation und
   FallbackRecoveryPending existieren bereits. Die neue Selection ist
   read-only und schreibt kein Wirefeld.
4. Physisch prüfbarer RAM-only Fallback:
   `run_persistence_coordinator.cpp:257-495` liest/verifiziert Current und
   Fallback, restauriert bereits `persistedIds_`, High-Watermark und
   `FallbackRecoveryPending`; die neue API darf denselben vorhandenen
   Handoff für einen weiterhin gültigen Fault-Current ohne Storewrite
   ausführen.
5. #18 bleibt Recoveryautorität:
   `run_persistence_coordinator.cpp:759-815` und die bestehenden
   `run_recovery*` führen den anschließenden Vorwärtscommit sowie Zeit-,
   Sensor-, Progress-, Phasen- und Terminalentscheidung aus. #24 wählt nur
   die Quelle und ruft keinen zweiten Recoverykern auf.
6. Ein normaler Fault-Current wird nicht Resumequelle:
   `run_persistence_coordinator.cpp:439-444` klassifiziert einen gültigen
   Current als `LoadedActiveRun`; Fallback-Auswahl ist ausschließlich der
   vorbereitete, intent- und Safety-qualifizierte Hand-off. Ein Fault-Current
   wird nicht über `activateLoadedRun()` resumiert.
7. Y4 bleibt persistent verboten: `runRecoveryForbidden` liegt in der
   unabhängigen SafetyState-Familie, wird beim Y4-Raise gesetzt und erst nach
   bestätigtem NoActiveRun-Tombstone gelöscht.

Die Proofs sind Architektur-/Codevertragsnachweise, keine ausgeführten Tests.

## 27. Plan-Gates und Owner-Freigabe

Vor dem neuen Plancommit wird der vollständige Dokumentdiff nochmals gelesen
und genau diese Prüfungen ausgeführt:

~~~text
git diff --check
python3 scripts/check_architecture_boundaries.py
python3 scripts/check_secrets.py
~~~

Builds, native Tests, ESP-IDF-Profile, Hardware-Smoke und vollständige CI sind
in dieser Planrevision NOT_RUN, entsprechend
docs/CI_AND_QUALITY_GATES.md. Ein nicht ausgeführter Test ist kein PASS.

Der Owner muss den exakten neuen Plan-SHA freigeben. Danach sind vor Slice 1
Branch, Base, PR-Head, Plan-SHA, PR-Body, Roadmap und SESSION HANDOVER erneut
live zu revalidieren.

Warte nach Commit, Push, PR-Body, Handover und Remote-Head-Verifikation auf
Ownerfreigabe exakt der neuen Plan-SHA. Keine Implementation.
