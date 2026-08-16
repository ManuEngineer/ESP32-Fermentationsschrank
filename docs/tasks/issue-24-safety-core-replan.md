# Issue #24 – Safety Core neu von `origin/main`

## 1. Auftrag, Status und Owner-Gate

Dieses Dokument ist der vollständige, eigenständige Plan für Issue #24
`[E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion`.
Es wird direkt vom unten verifizierten `origin/main` abgeleitet. Es ist ein
Plan für einen Draft-PR; Produktionscode und Testimplementation gehören nicht
zu diesem Commit.

Der Implementierungsstart ist gesperrt, bis der Owner genau diesen Plan-Commit
freigibt. Eine Änderung an Safety-, Recovery-, Persistenz-, Aktor- oder
Konfigurationsverträgen erzeugt vorher eine neue Planrevision und benötigt
erneut die Freigabe des exakten Commits.

**Planstatus:** `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`

**Nicht Bestandteil dieses Plans:**

- kein Produktionscode, keine Testimplementation und keine Hardwareänderung;
- kein Merge, kein `Ready for review`, kein Auto-Merge und kein Issue-Schluss;
- kein Rebase, Cherry-Pick oder Codeübernehmen aus PR #107 oder PR #108;
- keine neue Release-1-Funktion für OTA, Netzwerk oder Web;
- keine erfundenen GPIOs, Pegel, Controller, Fanrückmeldungen, Grenzwerte oder
  Inbetriebnahmedaten.

PR #107 und PR #108 bleiben offen und Draft. Sie sind ausschließlich
nicht-normative historische Fehler-/Lernreferenzen; weder ihre Pläne noch ihr
Reviewtext sind Quellen dieses Plans.

## 2. Live-Basis und normative Quellen

Zum Planbeginn wurde live geprüft:

| Gegenstand | Verifizierter Stand |
|---|---|
| Repository | `ManuEngineer/ESP32-Fermentationsschrank` |
| Basisbranch | `main` |
| Basiscommit | `b8eae5f4da5f2666b5a9bda333d115254c4db5b2` |
| Issue | #24 offen, Titel wie oben |
| alter PR #107 | offen, Draft, nicht implementiert, nicht-normativ |
| alter PR #108 | offen, Draft, `a448ad591d147b6388f8fc8b896440baad4facc9`, nicht implementiert, nicht-normativ |
| neuer Branch | `agent/issue-24-safety-core-replan`, direkt vom Basiscommit |
| Roadmap vor dieser Arbeit | keine aktuelle #24-Arbeit und kein veralteter #107/#108-Status; Reihenfolge #23 → #24 → #19 bleibt bestehen |

Die Quellenmatrix für diesen Plan ist:

| Bereich | Normative Quelle / geprüfte Codequelle |
|---|---|
| Governance | `AGENTS.md`, `docs/AGENT_WORKFLOW.md`, `docs/ENGINEERING_PRINCIPLES.md` |
| Architektur | `docs/DECISIONS.md` ADR-013/014, `lib/*/AGENTS.md` |
| Produkt/Scope | `docs/SPECIFICATION_REVIEW.md`, `docs/ROADMAP.md` |
| Safety | `docs/SAFETY_AND_FAULTS.md`, `docs/SAFETY_COMPONENT_FAULTS.md`, `docs/SYSTEM_SAFETY_AND_RECOVERY.md` |
| Prozess/Recovery | `docs/STATE_MACHINE.md`, `docs/RUN_COMMANDS.md`, `docs/RUN_PERSISTENCE.md`, `docs/RECOVERY_AND_INTERRUPTION.md` |
| Aktoren | `docs/ACTUATOR_TIMING.md`, `docs/ACTUATOR_TIMING_AND_FANS.md` als obsolete Verweis, `actuator_*`, `temperature_control_*` |
| Konfiguration | `docs/CONFIGURATION_PERSISTENCE.md`, `configuration_service.*`, `configuration_recovery_service.*` |
| Qualität | `docs/ACCEPTANCE_TESTS.md`, `docs/CI_AND_QUALITY_GATES.md` |
| aktueller #17-Code | `run_persistence_contract.*`, `run_persistence_codec.*`, `run_persistence_store.*`, `run_persistence_coordinator.*` |
| aktueller #18-Code | `run_recovery.*`, `run_recovery_time.*`, `run_recovery_types.hpp` |
| aktueller Prozess | `run_commands.*`, `process_state_machine.*`, `fermentation_application.*`, `src/main.cpp` |

Der Basiscommit ist ein Mergecommit von PR #105. Die Prüfung dieses Plans
verwendet ausschließlich den aktuellen Code an diesem Basiscommit, nicht den
lokalen alten #108-Checkout.

## 3. Ziel und Abnahmeschnitt

Issue #24 liefert eine einzige Safety-/Fault-Autorität mit deterministischer
Fehlerklassifikation, persistenten S3-/Y4-Verriegelungen, fail-closed
PERSISTENZ, kontrolliertem Restart, SAFE_BOOT, Fehlerinjektion und einem
internen Aktor-Gate. Die Fachlogik bleibt hardwarefrei; die Safety- und
Recovery-Verträge bleiben ohne Netzwerk, Web oder Anzeige funktionsfähig.

Die Implementierung ist erst abgeschlossen, wenn:

1. alle unten festgelegten Wire-, Zustands-, Dominanz-, Reset-, Recovery- und
   Aktorverträge im Code und in den Akzeptanztests nachgewiesen sind;
2. die realen Producer aus #20, #21, #22, #23, #56 und #57 angeschlossen sind;
3. die S3- und Y4-Matrizen einschließlich Crash-/Readback-Fällen als
   `PASS`, `FAILED`, `BLOCKED` oder `NOT_RUN` vorliegen;
4. kein alter Run ohne Beweis reaktiviert wird und kein Aktor-Gate umgangen
   werden kann;
5. der Owner einen vollständigen Review des finalen Implementierungsdiffs
   durchgeführt und anschließend den eigenen CI-Gate-Schritt auslöst.

## 4. Architekturgrenzen

Die Modulrichtung aus ADR-013 bleibt verbindlich:

```text
device_platform
  portable Ports/Dienste, SafetyState-/Restart-/Store-Ports ohne Fachbegriffe

device_platform_esp_idf
  ESP-IDF-Adapter, ResetCause/Bootzeit/StateStore, keine Safety-Entscheidung

fermentation_app
  FaultCatalog, SafetyFaultService, Safety-Orchestrierung, Prozess-/Run-
  Integration und Aktor-Direktive gegen abstrakte Ports

device_platform_test_support
  deterministische Mocks und Injection-only-Producer, nie Produktionsabhängigkeit
```

`main.cpp`/`main/app_main.cpp` bleiben Composition Roots. Der
`ProcessStateMachine` bleibt hardware- und persistenzfrei und entscheidet nicht
über Safety. `RunPersistenceCoordinator` bleibt der einzige #17-Persistenzkern.
`RunRecoveryCoordinator` bleibt die einzige #18-Recoveryentscheidung. #19
besitzt die Langzeithistorie; #24 erzeugt nur typisierte, begrenzte
SafetyEvents.

## 5. Kanonische Safety-Verträge

### 5.1 Klassen, Dominanz und Identität

| Klasse | Bedeutung | Lebensdauer | Default-Gate | Reset / Recovery |
|---|---|---|---|---|
| P1 | Hinweis/Warnung | bootlokal/transient | kein neues Safety-Gate | bestehende P1-Policy |
| O2 | behebbarer Betriebsfehler | bootlokal/transient | bestehende O2-Policy | automatische oder Operator-Policy aus der Ursache |
| S3 | verriegelter Sicherheitsfehler | persistent/rebootstabil | Safety-Gate zu, Peltier/H-Brücke aus | Ursache klar, aktuelle Checks, Service und gezielter Reset |
| Y4 | schwerer Systemfehler | persistent/rebootstabil | Safety-Gate zu, SAFE_BOOT nach Vertrag | Service-Reset; aktiver Run terminalisieren |

Die Dominanz ist `Y4 > S3 > O2 > P1`. Innerhalb einer Klasse entscheidet die
compile-time-Katalogreihenfolge deterministisch. Eine neue Ursache überschreibt
keine aktive Ursache: jede erlaubte `FaultIdentity` bleibt als eigener Record
aktiv; Primary-/Follow-up-Beziehungen werden separat geführt.

```text
FaultIdentity = FaultCode + FaultSource
```

MeasurementSequence, RunRevision, ControlRevision, PlannerRevision,
Zeitstempel und CorrelationKey sind niemals Bestandteil der Identity. Eine
InstanceId identifiziert ein konkretes Fault-Ereignis, nicht den Fault-Typ.
P1/O2 verbrauchen keine persistenten InstanceIds; S3/Y4 erhalten persistente,
rebootstabile InstanceIds und FaultRevisions.

### 5.2 Compile-time FaultCatalog

Die folgenden Paarungen sind die vollständige Release-1-Katalogmenge. Andere
Code-/Source-Kombinationen sind nicht konstruierbar. Der numerische `rank` ist
die deterministische Priorität innerhalb der Klasse; Policy, Gate, Fan- und
Auth-Regeln werden aus dem Katalog abgeleitet und nicht als freie Wire-Policy
gespeichert.

| rank | Code | erlaubte Source | Klasse | persistent | Erstreaktion / Recovery |
|---:|---|---|---|---|---|
| 10 | `P1_RECOVERY_EVIDENCE_UNCERTAIN` | `RecoveryEvidence` | P1 | nein | Hinweis; keine eigene Fortschrittsentscheidung |
| 20 | `O2_PRODUCT_SENSOR_UNAVAILABLE` | `ProductSensor` | O2 | nein | bestehende #21-Produktfallback-/Operatorpolicy |
| 100 | `S3_SAFETY_AIR_SENSOR` | `SafetyAirSensor` | S3 | ja | sofort sicher; CauseClear + Service-Reset |
| 110 | `S3_COOLING_SENSOR` | `CoolingSensor` | S3 | ja | sofort sicher; CauseClear + Service-Reset |
| 120 | `S3_SENSOR_CONTRADICTION` | `SensorCrossCheck` | S3 | ja | sofort sicher; kein stiller Sensorwechsel |
| 130 | `S3_THERMAL_BOUNDARY` | `ThermalGuard` | S3 | ja | Peltier/H-Brücke aus; ursachenspezifische Lüfter |
| 140 | `S3_PELTIER_H_BRIDGE` | `PeltierDriver` | S3 | ja | Peltier/H-Brücke aus; keine Hardwarebehauptung ohne Producer |
| 150 | `S3_OUTER_FAN` | `OuterFan` | S3 | ja | Peltier aus, Restwärmestrategie nach Safety-Direktive |
| 160 | `S3_INNER_FAN` | `InnerFan` | S3 | ja | ursachenspezifische Innenlüfterreaktion |
| 170 | `S3_ACTUATOR_WATCHDOG` | `ActuatorPlanner` | S3 | ja | keine alte Regelanforderung weiter ausführen |
| 180 | `S3_UNCONFIRMED_HARDWARE` | `HardwareGate` | S3 | ja | keine Aktorfreigabe; injection-only bis echter Producer |
| 200 | `Y4_RUN_NOT_RECONSTRUCTIBLE` | `RunPersistence` | Y4 | ja | Run-Abandon-Pfad, niemals Raten oder Factory Reset |
| 210 | `Y4_RUN_PERSISTENCE_INDETERMINATE` | `RunPersistence` | Y4 | ja | `BlockedIndeterminate`; autorisierter Tombstone nur bei sicherem Store |
| 220 | `Y4_SAFETY_STATE_PERSISTENCE` | `SafetyStateStore` | Y4 | ja | EmergencyMarker, SAFE_BOOT-Pflicht, kein Endlos-Write-Loop |
| 230 | `Y4_CONFIGURATION_RUNTIME` | `ConfigurationRuntime` | Y4 | ja | reale #56/#57-Quelle konsumieren, produktiv fail-closed |
| 240 | `Y4_CONFIGURATION_UNAVAILABLE` | `ConfigurationAvailability` | Y4 | ja | reale #57-Quelle konsumieren, produktiv fail-closed |
| 250 | `Y4_CONFIGURATION_INTEGRITY` | `ConfigurationIntegrity` | Y4 | ja | reale #57-Quelle konsumieren, produktiv fail-closed |
| 260 | `Y4_CONFIGURATION_COMMIT_INDETERMINATE` | `ConfigurationCommit` | Y4 | ja | `CommitOutcomeUnknown`/`ConfigurationCommitIndeterminate` fail-closed |
| 270 | `Y4_RESTART_LOOP` | `RestartSupervisor` | Y4 | ja | nach drei abnormalen Boots SAFE_BOOT |
| 280 | `Y4_UNKNOWN_SAFETY_STATE` | `SafetyBoot` | Y4 | ja | SAFE_BOOT, keine Rekonstruktion ohne eindeutigen Readback |
| 290 | `Y4_INTERNAL_SAFETY` | `SafetyFaultService` | Y4 | ja | unmittelbarer Stop, EmergencyMarker falls nötig |

Damit gilt `kFaultCatalogIdentityCount = 21` und
`kMaxActiveFaultRecords = kFaultCatalogIdentityCount`. Das feste Array wird
compile-time gegen die Kataloggröße geprüft. Es gibt keine Eviction aktiver
Safety-Latches. Ein Katalog-/Kapazitätsfehler selbst wird als Y4 behandelt und
führt fail-closed in SAFE_BOOT; P1/O2 können nicht die persistenten S3/Y4-
InstanceId-/FaultRevision-Fenster verbrauchen.

### 5.3 Eine Safety-/Fault-Autorität

`SafetyFaultService` ist die einzige mutable Safety-Wahrheit. Sie besitzt:

- aktive Records und den unveränderlichen Catalog-Verweis;
- S3-/Y4-Latches, FaultRevision und persistente InstanceIds;
- CauseClear und Primary-/Follow-up-Beziehungen;
- RestartEpisode, RestartIntent und Restart-Ausgang;
- `safeBootRequired` und `runRecoveryForbidden`;
- Resetbewertung und Safety-Persistenzstatus.

`RunCommandState` darf nur Projektionen für bestehende Commandverträge tragen.
`ProcessStateMachine`, `ActuatorPlanner`, `ConfigurationService` und
`TemperatureControlApplicationOrchestrator` dürfen keine zweite Faultrevision,
Latch- oder Resetwahrheit besitzen.

### 5.4 Autorisierung

Bis ein kanonischer Ausnahmevertrag ergänzt ist, gilt:

```text
S3 Reset = Service
Y4 Reset = Service
```

Transport/UI darf `authorizationSatisfied=true` nicht selbst erzeugen. Ohne
echten produktiven Auth-Producer ist der Produktpfad fail-closed. Native Tests
dürfen einen deterministischen, typisierten Injection-Producer verwenden; das
ist keine Produktionsautorisierung.

## 6. Persistenter SafetyState und EmergencyMarker

### 6.1 Domäne und Commit

SafetyState erhält einen eigenen, begrenzten Store-Port und feste Record-
Schlüssel/Slots. Er ist unabhängig vom Configuration-`StorageEpoch`; ein
Konfigurations-Werksreset darf keine Safety-Latches löschen. Der Safety-Store
verwendet einen eigenen fest verdrahteten Safety-Domain-Identifier und eine
eigene Recordrevision, nicht den Configuration-Epoch-Zähler.

Es gibt einen normalen SafetyState-Commit mit Readback und einen getrennten,
minimalen EmergencyMarker-Pfad. Der SafetyState-Commit ist die
FaultReset-Linearisierung. Bei `CommitOutcomeUnknown` wird nichts als
gelöscht, resettet oder freigegeben behauptet.

### 6.2 Deterministisches SafetyState-Wireformat

SafetyState-Wire-Version 1 wird als feste Feldfolge im bestehenden
`StorageEnvelope`-Mechanismus kodiert. Alle Integer sind unsigned, feste
Breiten und little-endian; Optionals werden mit einem Tagbyte vor dem festen
Payload kodiert; unbekannte Tags, Längen, CRCs, Versionen oder Restbytes sind
ungültig. Keine Policy-Werte werden aus dem Wire als Katalogeigenschaft
interpretiert.

Payload-Feldfolge:

1. `recordMagic` und `wireVersion`;
2. `recordRevision: uint64`;
3. `bootSequence: uint64`;
4. `abnormalRestartCount: uint8`;
5. `restartEpisode: uint64`;
6. `persistentFaultRevision: uint64`;
7. `nextPersistentInstanceId: uint64`;
8. `safeBootRequired: bool`;
9. `runRecoveryForbidden: bool`;
10. `terminalRunId` als Länge-plus-UTF-8-Feld mit der festen maximalen Länge
    `kMaxRunIdBytes`; Überlänge ist ungültig;
11. `restartIntent` als Tag (`None`, `S3Recovery`) plus `sourceInstanceId:
    uint64`, `sourceRunId` mit `kMaxRunIdBytes`, und `restartEpisode: uint64`;
12. `restartOutcome` als Tag (`None`, `Requested`, `Rejected`, `Consumed`);
13. `activeFaultCount: uint8`, danach genau `kMaxActiveFaultRecords` feste
    Recordplätze; jeder Platz hat `present: bool`, `FaultCode: uint16`,
    `FaultSource: uint8`, `instanceId: uint64`, `firstSeenBoot: uint64`,
    `lastSeenBoot: uint64`, `status: uint8`, `causeClearEvidence: uint8`,
    `primaryInstanceId` als Tag plus `uint64`, `followUpCount: uint8` und
    genau `kMaxFollowUpsPerFault` feste `followUpInstanceId: uint64`-Felder;
14. `safetyPersistedStatus` als Tag (`Healthy`, `EmergencyMarkerRequired`,
    `BlockedIndeterminate`).

Die Katalogprüfung validiert die Code-/Source-Paarung, Klasse, persistenten
Lebenszyklus und die statische Recordkapazität. `FaultClass`, Fanpolicy,
Gatepolicy und Authpolicy werden nicht redundant im Wire gespeichert. Eine
aktive Y4-Identität oder ein gesetztes `runRecoveryForbidden` bleibt bei jedem
Reset/Reboot erhalten.

### 6.3 EmergencyMarker

Der EmergencyMarker hat einen eigenen festen Schlüssel und eine minimale,
deterministische Feldfolge: `recordMagic`, `wireVersion`, `markerRevision`,
`bootSequence`, `causeCode`, `causeSource`, `persistentFaultRevision`,
`safeBootRequired`, `runRecoveryForbidden`, Payloadlänge und CRC. Er enthält
keine Run-/Config-Daten und ersetzt weder SafetyState noch FaultReset.

Bei normalem SafetyState-Commitfehler:

1. RAM sofort fail-closed und Aktor-ImmediateStop;
2. genau einen begrenzten EmergencyMarker-Schreib-/Readbackversuch;
3. kein unendlicher Write-Loop und keine Freigabe bei unklarem Ergebnis;
4. Marker-Recovery repariert nur die Safety-Persistenz und ist weder
   FaultReset noch SAFE_BOOT-Exit.

## 7. RestartSupervisor und SAFE_BOOT

Der `RestartSupervisor` ist kein zweiter Faultkern. Er verwaltet nur die
plattformseitigen Reset-/Bootübergänge und schreibt über `SafetyFaultService`
dessen persistente Felder.

| Ereignis | Persistente Wirkung | Aktorwirkung |
|---|---|---|
| Boot | `ResetCause` lesen, `bootSequence` erhöhen, Intent prüfen | noch keine Freigabe |
| normaler Boot | abnormales Episodefenster auswerten | Safety-Gate bleibt bis zur Qualifikation zu |
| kontrollierter S3-Restart | genau ein Intent je auslösender InstanceId | Reset ist kein Latch-Clear |
| Restart `Rejected` | Ergebnis `Rejected`, Fault/ImmediateStop bleibt | kein Auto-Retry |
| abnormaler Boot | `abnormalRestartCount` erhöhen | kein produktiver Aktorpfad |
| drei abnormale Boots | `safeBootRequired=true` und Y4-Katalogrecord | SAFE_BOOT, Aktoren gesperrt |
| 30 Minuten aktuelle stabile Bootzeit | Episode requalifizieren und Zähler nach Vertrag abbauen | erst nach allen Safetychecks |

Über Reboots wird keine monotone Zeit subtrahiert. Die 30 Minuten werden nur
aus der aktuellen Bootinstanz abgeleitet; UTC darf für Recovery-Evidenz fehlen
und ersetzt die bootlokale Zeit nicht. Kein Restart löscht Latches, setzt
`runRecoveryForbidden` zurück oder aktiviert einen alten Run.

## 8. S3 `RecoverIfProvable`

### 8.1 Eintritt, Reset und Restart

Bei S3 während eines aktiven Runs gilt diese Reihenfolge:

1. SafetyFaultService erzeugt/aktualisiert den S3-Record und nimmt das
   Safety-Gate sofort zu; Peltier/H-Brücke werden sicher ausgesteuert,
   Lüfterreaktionen kommen aus der Ursache.
2. S3-Latch wird im SafetyState persistiert; bei Commitfehler gilt der
   EmergencyMarker-Vertrag.
3. Die Prozessentscheidung `TransitionReason::CriticalFault` führt nach
   `ProcessState::Fault`.
4. #17 persistiert den Fault-Checkpoint mit der bestehenden Standard-Fallback-
   Rotation. Der vorherige gültige Checkpoint bleibt die `currentHead.fallback`-
   Referenz.

CauseClear ist nur aktuelle, eindeutige CauseClear-Evidenz; es ist kein Reset.
Erst ein Service-Reset mit passender Target-InstanceId, FaultRevision,
Command-Envelope, erwarteter Prozesslage und Bestätigung darf die folgende
SafetyState-Transaktion ausführen:

```text
S3-Latch entfernen + RestartIntent(S3Recovery, sourceInstanceId, sourceRunId)
  in einem SafetyState-Commit linearisieren
```

Danach bleibt `ProcessState::Fault`, der Safety-Gate bleibt zu, kein alter Run
wird im selben Boot aktiv geschrieben, und kein #17-Head wird zurückgedreht.
Der Supervisor versucht genau einen kontrollierten Restart für diese
InstanceId. Bei `Rejected` bleiben Fault und ImmediateStop; es gibt kein
Auto-Retry. Ein späterer Service-/Owner-Schritt darf rebooten oder den Run
terminal beenden.

### 8.2 Kontrollierter Boot-Handoff

Der Bootpfad lädt zuerst SafetyState und qualifiziert Ursache, ResetCause,
Intent, `safeBootRequired` und `runRecoveryForbidden`. Nur bei gültigem
`S3Recovery`-Intent und erlaubter alter Run-Recovery wird #17 wie folgt benutzt:

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
CauseClear + Service Reset
   |
   +--> S3 latch removed + RestartIntent(S3Recovery) in SafetyState
   +--> RAM stays FAULT
   |
controlled restart
   |
   v
BOOT
   |
   +--> Safety qualifies intent
   |
   +--> #17 verifies current Fault + existing fallback
   |
   +--> RAM-only fallback selection
          -> FallbackRecoveryPending
          -> NO persistent head rollback
   |
   +--> existing #18 recovery
          |
          +--> provable -> Resume
          |
          +--> not provable -> terminal
```

Dafür wird eine kleine read-only/ram-only API am bestehenden
`RunPersistenceCoordinator` geplant, beispielsweise
`prepareSafetyFallbackRecovery(...)`. Sie darf:

- den vorhandenen Committed-Head, den `Fault`-Current-Checkpoint und die
  bereits referenzierte Fallback-Referenz lesen und verifizieren;
- beide physischen Slots mit dem vorhandenen Codec, CRC, Schema, Epoch und
  `RunCheckpointReference` prüfen;
- sicherstellen, dass Current und Fallback denselben `activeRunId`, dieselbe
  Program-/Manual-Snapshotidentität, dieselbe `ProcessRunSnapshot`-Identität
  und denselben Run-Kontext besitzen; nur monotone Checkpoint-/Runrevisionen
  dürfen sich als Fortschritt unterscheiden;
- die Fallback-Rohdaten in den vorhandenen RAM-Slot laden und den bestehenden
  Zustand `FallbackRecoveryPending` herstellen;
- den geladenen Snapshot an `activateFallbackRecoveredRun()` übergeben.

Sie darf nicht den Head oder einen Slot schreiben, Current/Fallback vertauschen,
Checkpoint-Payload kopieren, eine Revision zurückdrehen, Schemafelder ändern,
eine eigene Recoveryentscheidung treffen oder #18 duplizieren. Die Safety-API
qualifiziert nur den Intent und die Berechtigung; #18 entscheidet weiterhin
über Sensorwahl, UTC/monotone Zeit, Progress, Phase und Resume/Terminal.

Der normale Bootpfad darf `activateLoadedRun()` für diesen Intent nicht als
Resumequelle aufrufen: Ein `current`-Snapshot mit `ProcessState::Fault` bleibt
bis zur expliziten Fallback-Auswahl Fault. Nur die qualifizierte,
RAM-seitige API darf den bereits geprüften Fallback als Quelle für
`FallbackRecoveryPending` markieren; ohne gültigen Intent oder bei
`runRecoveryForbidden` bleibt der Fault-Current gesperrt.

### 8.3 Zeit- und Phasenvertrag

Für

```text
t0 = letzter gültiger Pre-Fault-Checkpoint
t1 = tatsächlicher Fault-Eintritt
t2 = Recovery-Boot / neuer Recovery-Anker
```

gilt konservativ:

- `t0..t1` ist Recovery-Unsicherheit;
- `t1..t2` ist Recovery-Unterbrechung;
- kein Abschnitt wird als sicher beobachtete normale Fermentationszeit
  erfunden;
- keine #24-eigene Fortschritts- oder Phasenrechnung;
- #18 nutzt ausschließlich seine vorhandenen Zeit-/Sensor-/Progressregeln.

Das gilt jeweils für `QualifyingTarget`, `WaitingForProduct`, `Fermenting`,
`Cooling`, `CoolHolding` und `ManualHolding`. Kann #18 die sichere Fortsetzung
nicht beweisen, wird der Run terminalisiert.

## 9. Y4 `Terminal`

### 9.1 Persistente Recovery-Sperre

Sobald Y4 einen aktiven Run betrifft, setzt die Safety-Autorität im selben
SafetyState-Vertrag wie der Y4-Latch:

```text
runRecoveryForbidden = true
```

Das Flag ist unabhängig davon, ob der konkrete Y4-Latch später bereits
CauseClear erhalten hat. Es bleibt über Reset/Reboot bestehen und blockiert
jeden S3-Recovery-Handoff. Ein separates `faultEpisodeHadY4`-Modell ist nicht
nötig.

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
CauseClear + Service Reset
   |
   +--> while blockers remain: stay Fault/SafeBoot
   |
when terminal Standby can be committed:
   |
   +--> canonical NoActiveRun/STANDBY tombstone via #17
   +--> readback
   +--> clear runRecoveryForbidden
   |
   v
STANDBY / separate SAFE_BOOT exit
```

### 9.2 Terminalisierung ohne `NoActiveRun + Fault`

Der aktuelle #17-Vertrag erlaubt `NoActiveRun` nur mit `Standby`. Der Plan
verbietet deshalb `Fault -> clearActiveRunState()`. Stattdessen gilt:

1. Y4 setzt `runRecoveryForbidden` und hält den alten Run persistiert.
2. Solange ein blockierender Fault/SafeBoot besteht, bleibt RAM und Persistenz
   `Fault`; kein alter Run darf recovern.
3. Nach qualifiziertem Service-Reset und nur an einem zulässigen terminalen
   Standby-Punkt wird `Fault -> Standby` als Prozessentscheidung angenommen.
4. Erst dann erzeugt #17 den echten kanonischen
   `NoActiveRun/STANDBY`-Tombstone, schreibt Slot und Committed-Head, validiert
   CAS/Readback und bestätigt die neue Run-Wahrheit.
5. Erst nach erfolgreichem Readback-Commit wird `runRecoveryForbidden=false`
   im SafetyState persistiert.

Crash vor oder zwischen Tombstone- und SafetyState-Commit lässt die Sperre
erhalten. Crash nach bestätigtem Tombstone findet `NoActiveRun` als persistente
Run-Wahrheit vor. SAFE_BOOT verlässt sich nicht implizit: Der Tombstone muss
vor einem separaten SAFE_BOOT-Exit vollständig bestätigt sein; andernfalls
bleibt SAFE_BOOT aktiv.

## 10. Run-Abandon bei unrekonstruierbaren Daten

Die folgenden #17-Status werden nicht geraten, nicht durch Factory Reset
"behoben" und nicht als alter Run fortgesetzt:

| Status | Safety-/Run-Wirkung | späterer Abandon |
|---|---|---|
| `NotReconstructible` | Y4, Fault/SAFE_BOOT, `runRecoveryForbidden` wenn Runbezug unklar | nur nach technisch eindeutiger Store-Reparatur |
| `NotReconstructibleOrphanedState` | Y4, kein unreferenzierter Slot als Runquelle | gleicher Tombstonepfad |
| `PreparedInterrupted` | Y4/`BlockedIndeterminate`, kein Prepared-Head-Rollback | schmale #17-Recovery-/Abandon-API |
| `ReadFailed` | Y4, keine Rekonstruktion | Readback muss später eindeutig funktionieren |
| `CapacityExceeded` | Y4, keine Teilmutation | Kapazität erst technisch sicher beweisen |
| `UnsupportedSchema` | Y4, kein stilles Migrationsraten | nur ausdrücklich unterstützte Version |
| `ForeignEpoch` | Y4, fremde Domäne verwerfen | kein Configuration-Reset als Abkürzung |

Wenn der Store später eindeutig les-/schreibbar ist, verwendet eine schmale
Erweiterung des bestehenden `RunPersistenceCoordinator` den vorhandenen
Transaktionspfad:

```text
autorisierte Requalifikation
  -> kanonischer NoActiveRun/STANDBY-Snapshot
  -> physischer Checkpointslot
  -> Prepared-Head
  -> Committed-Head mit exakter Tombstone-Referenz
  -> CAS/readback
  -> erst danach RunPersistence-CauseClear
```

Programme und Konfiguration bleiben unangetastet. Es gibt keinen zweiten
Persistenzkern und keine neue Recovery-State-Machine. Für
`BlockedIndeterminate`/`PreparedInterrupted` wird nur der schmale technische
Abandon-/Readback-Handoff geplant; keine autonome Sicherheits- oder
Recoveryentscheidung.

## 11. Warum #17/#18 nicht verbogen werden

Der Kernbeweis am Basiscommit ist:

1. **#17 bleibt forward-only.** `run_persistence_contract.hpp:19-30` hält
   Schema 3 als aktuellen Vertrag; `run_persistence_codec.cpp:1370-1417`
   validiert monotone Run-/Transitionrevisionen, gültige Referenzen und
   unterschiedliche Current-/Fallback-Slots.
2. **S3 dreht keinen persistenten Head zurück.**
   `run_persistence_coordinator.cpp:1404-1477` schreibt nur über
   `writeSnapshotCore`; die neue Auswahl-API führt dort keinen Write aus.
3. **Kein neues #17-Wirefeld ist nötig.** Der Head besitzt bereits Current,
   Fallback, Slot, Schema, Epoch, Revision und Mutation; die Auswahl ist ein
   RAM-Zustandsübergang in den bestehenden `FallbackRecoveryPending`-Zustand.
4. **Der vorhandene Fallback bleibt physisch unverändert.**
   `run_persistence_coordinator.cpp:331-369` verifiziert Referenz, Envelope,
   CRC/Codec, Schema und Epoch; `:446-485` lädt eine vorhandene Fallback-
   Referenz in RAM, ohne Head- oder Slot-Write.
5. **#18 bleibt alleinige Recoveryautorität.**
   `activateFallbackRecoveredRun()` in `run_persistence_coordinator.cpp:760-820`
   stellt den vorhandenen Snapshot wieder her und führt danach den bestehenden
   Recoverypfad aus; die Zeit-/Ankerbildung in `:93-144` und die bestehende
   `run_recovery*`-Logik bleiben maßgeblich.
6. **Y4-Terminalität lebt rebootfest in SafetyState.** Das neue Flag ist ein
   SafetyState-Feld, nicht ein #17-Head-Feld und nicht ein temporäres
   `RunCommandState`-Bit.
7. **NoActiveRun ist nur ein kanonischer Standby-Tombstone.** Die bestehende
   `clearActiveRunState()`-Grenze in `run_commands.cpp:577-597` wird nicht aus
   Fault heraus aufgerufen; die Prozessentscheidung nach Standby und der
   #17-Commit bleiben getrennt.
8. **Safety blockiert, ohne einen Run zu rekonstruieren.**
   `run_persistence_coordinator.cpp:257-329` und `:487-498` zeigen bereits die
   fail-closed Statusgrenzen; `runRecoveryForbidden` verhindert zusätzlich,
   dass ein technisch gültiger alter Fallback als erlaubte Y4-Recoveryquelle
   in #18 gelangt.

**Core-Proof-Ergebnis vor Plancommit:**

| Nachweis | Ergebnis | Bedeutung |
|---|---|---|
| `CriticalFault` bewahrt nutzbaren Pre-Fault-Fallback | **PASS** | Standard-Fallback-Rotation in `writeSnapshotCore()` setzt den bisherigen Current-Head als Fallback; unresolved Commit bleibt fail-closed. |
| S3-Fallback kann ohne persistente Head-Mutation an #18 übergeben werden | **PASS** | vorhandene Referenz-/Slotprüfung plus `FallbackRecoveryPending` und `activateFallbackRecoveredRun()` erlauben die schmale RAM-Auswahl-API. |
| #18 bleibt Recoveryautorität | **PASS** | #24 wählt/verifiziert nur; #18 entscheidet Zeit, Sensor, Progress, Phase und Terminal. |
| kein #17-Schemawechsel für S3 nötig | **PASS** | Schema 3, Current/Fallback-Referenzen und Recovery-Mutation decken den Forward-Write ab; kein `safetyFallbackPromotion`-Feld. |
| Y4-Recovery ist persistent verboten | **PASS** | der Plan schreibt `runRecoveryForbidden` als SafetyState-Invariante vor jedem Boot-/S3-Handoff vor; Implementation ist noch nicht gestartet. |

Diese fünf PASS-Werte sind Architektur-/Codevertragsnachweise, keine
ausgeführten Tests. Umsetzung und Tests bleiben `NOT_RUN`.

## 12. Aktor-Gate und Fan-Safety

Der einzige produktive abstrakte Pfad ist:

```text
Producer
  -> SafetyFaultService
  -> ActuatorSafetyDirective
  -> TemperatureControlApplicationOrchestrator
  -> ActuatorPlanner
  -> ActuatorPlanSinkDriver
```

Der Orchestrator bezieht die Directive intern aus der zentralen Safetyautorität.
Ein caller-supplied `Allowed` ist kein gültiger Produktionsinput. Der Plan
entfernt/ersetzt die vorhandenen frei behauptbaren Safety-Boolean-Projektionen
aus dem produktiven Pfad; #15/#17 behalten ihre bestehenden Envelope-/Revision-
Prüfungen.

Die Directive muss mindestens `peltierAllowed`, `hBridgeAllowed`,
`outerFanDirective`, `innerFanDirective`, `immediateStop`, `causeRevision` und
`sourceInstanceId` tragen. Sie wird nur aus gültigem Catalog/SafetyState
gebildet.

- Peltier/H-Brücke sind bei Boot, Reset, Fault, Y4, SAFE_BOOT, Unknown und
  unbestätigter Hardware aus.
- `Y4 => beide Fans AUS` ist verboten. Außen-/Innenlüfter reagieren
  ursachenspezifisch; Restwärmestrategie und Nachlauf aus
  `ACTUATOR_TIMING.md` bleiben erhalten.
- Ohne echten Hardwareproducer gibt es keine physische Fanrückmeldung. Fan-
  und Aktorfehler sind injection-only.
- #23-Nachlauf, Totzeit, Mindest-Ein-/Auszeit, Integratorfeedback und Watchdog
  werden nur über den bestehenden Plannervertrag benutzt, nicht in #24
  dupliziert.

## 13. #15 FaultReset und Producer

Der bestehende `FaultResetEvaluation`-Vertrag wird erweitert, nicht kopiert:

1. #15 prüft Command-Envelope, erwarteten Zustand, erwartete FaultRevision,
   Bestätigung und die von #24 qualifizierte ResetEvaluation einschließlich
   Target-InstanceId.
2. `Service` ist die einzige produktive Autorisierung für S3/Y4.
3. `ResetFault` bleibt außerhalb des normalen #17-persistierten Commandpfads.
4. #15 mutiert weder FaultCore/SafetyFaultService noch Latches, FaultRevision
   oder Safety-Persistenz. Der SafetyState-Commit linearisiert den Reset.

Die realen Producer werden nur konsumiert, nicht in #24 nachgebaut:

| Quelle | Eingangsverträge | #24-Wirkung |
|---|---|---|
| #20 | reale Prozess-/Temperatur-Sicherheitsfehler | Catalog-Route mit Ursache und aktueller Evidenz |
| #21 | Sensorqualität, Stale/Failed/Contradictory | Rolle/Source-spezifisches S3/O2 gemäß Catalog |
| #22 | Control-/Integrator-/Sensorcontext | keine physische Aktorbehauptung, Safety-Direktive |
| #23 | Planner-Watchdog, stale/invalid plan | S3-Watchdog-/Aktorroute, bestehende Timinglogik bleibt |
| #56 | `ConfigurationRuntimeFailure`, Commitstatus | Y4 bei runtime/indeterminate, fail-closed |
| #57 | `ConfigurationUnavailable`, `ConfigurationIntegrityFailure` | Y4, kein Factory Reset und kein Safety-Clear |
| #56/#57 gemeinsam | nicht auflösbares `CommitOutcomeUnknown` / `ConfigurationCommitIndeterminate` | Y4 bis eindeutiger Readback/Recovery |

## 14. SafetyEvents und #19

`SafetyFaultService` darf pro Mutation nur eine feste, katalogseitig gezählte
Eventmenge erzeugen: höchstens ein Primärereignis, höchstens ein
Dominanz-/Follow-up-Ereignis pro betroffener Identity, höchstens ein
Persistenz-/Restart-Ereignis und höchstens eine Gate-Direktive. Die konkrete
Maximalzahl wird als `kMaxSafetyEventsPerMutation` statisch geprüft und die
API liefert bei Überschreitung einen Fehler statt still zu truncaten.

Die Events sind typisiert und bounded. #24 besitzt keine Journalpersistenz,
keinen allgemeinen Eventbus und keine unbounded Queue. #19 übernimmt die
Langzeithistorie über einen eigenen späteren Vertrag.

## 15. Umsetzungsslices nach Planfreigabe

Jeder Slice startet erst nach Freigabe dieses exakten Plancommits und bleibt
auf den jeweils genannten Vertrag beschränkt. Ein Slice, der eine materielle
Architekturentscheidung ändert, stoppt und erzeugt eine neue Planrevision.

### Slice 1 – Catalog und reine FaultCore

Neue fachliche Typen für `FaultCode`, `FaultSource`, `FaultIdentity`,
`FaultRecord`, Dominanz, Priorität und die compile-time Catalogmatrix.
`SafetyFaultService` erhält zunächst reine Mutation-/Bewertungsfunktionen;
keine Store- oder Hardwareabhängigkeit im Pure Core.

### Slice 2 – SafetyState Wire/Store und EmergencyMarker

Portable SafetyState-/EmergencyMarker-Ports, deterministischer Codec,
bounded Store-Adapter, Readback/CAS und fail-closed Indeterminate-Pfad.
SafetyState bleibt unabhängig von Configuration StorageEpoch und #17.

### Slice 3 – RestartSupervisor und SAFE_BOOT

ResetCause-Port, Bootsequenz, RestartIntent, Requested/Rejected, abnormales
Bootfenster, Drei-Boot-SAFE_BOOT und 30-Minuten-Requalifikation. Keine
Latch-Löschung durch Restart.

### Slice 4 – Run-/Process-Integration

`CriticalFault`, S3-Latchpersistenz, Y4 `runRecoveryForbidden`,
Run-Abandon/Tombstone, S3-Intent und die kleine read-only
`prepareSafetyFallbackRecovery()`-Grenze. Danach der unveränderte
`activateFallbackRecoveredRun()`-/#18-Pfad. Kein Head-Rollback, kein
Schemawechsel, keine zweite Recovery-State-Machine.

### Slice 5 – #20/#21/#22/#23 Producer und FanDirective

Reale typisierte Producer an SafetyFaultService anschließen, Fan-Safety und
`ActuatorSafetyDirective` integrieren, injection-only Wege für noch nicht
physisch produzierte Fehler beibehalten.

### Slice 6 – #15 FaultReset und #17/#18-Handoff

Target-InstanceId und qualified ResetEvaluation, SafetyState-Linearisierung,
Boot-Intent-Qualifikation, S3-Test-Handoff und Run-Abandon-Readback.

### Slice 7 – #56/#57 Configuration Gate und Boot Composition

Alle realen Configuration-Producer fail-closed konsumieren, Configuration-
Werksreset von Safety-Latches trennen, Safety-/Run-/Process-/Platform-Ports
im Composition Root in der verbindlichen Reihenfolge verdrahten.

### Slice 8 – No-bypass Actuator Path

Caller-`Allowed` aus dem produktiven Pfad entfernen, interne SafetyDirective
erzwingen, Sink-/Planner-Gate und SAFE_BOOT-/Fault-Tests gegen Umgehung führen.

### Slice 9 – Kanonische Dokumentation und Owner-Review

Safety-, Recovery-, State-, Run-, Configuration-, Aktor-, Acceptance- und
Roadmap-Verweise aktualisieren; vollständigen aktuellen Diff gegen diesen Plan
prüfen; danach Owner-Gate und erst auf ausdrückliche Anweisung vollständige
CI-/Buildläufe.

## 16. Verbindliche Testmatrix nach Umsetzung

### 16.1 S3

| Bereich | Pflichtfälle | Nachweis |
|---|---|---|
| normal | `FERMENTING -> S3 -> Fault checkpoint + pre-fault fallback -> CauseClear -> Service reset -> S3Recovery -> reboot -> explicit fallback -> #18 -> Resume` | S3-Latch, unveränderter alter Head bis #18-Forward-Write |
| nicht beweisbar | fehlender, korruptierter oder fremder Fallback | kein Resume, terminal |
| Zeit | `t0`, `t1`, `t2` mit/ohne UTC | Unsicherheitsintervalle; keine #24-Fortschrittsrechnung |
| Phasen | QualifyingTarget, WaitingForProduct, Fermenting, Cooling, CoolHolding, ManualHolding | ausschließlich #18-Entscheidung |
| Persistenzcrash | vor S3-Commit; nach Safety-Commit; vor/nach Fault-Run-Commit; CauseClear; Resetcommit; Intent; Requested; Rejected; Boot; invalid fallback; #18-Recovery-Write | jeder Zustand bleibt fail-closed und rebootfest |

### 16.2 Y4

Y4 allein mit aktivem Run; Y4 + S3; mehrere Y4; Reboot mit aktivem Y4;
Y4-Reset bei anderem aktivem S3; Crash nach Y4-CauseClear/Reset;
`runRecoveryForbidden` überlebt; späterer S3-Reset darf nicht recovern;
terminaler Standby-Tombstone; Crash vor, zwischen und nach Tombstone-Commit;
SAFE_BOOT und separater SAFE_BOOT-Exit.

### 16.3 Run-Abandon

Für `NotReconstructible`, `NotReconstructibleOrphanedState`,
`PreparedInterrupted`, `ReadFailed`, `CapacityExceeded`, `UnsupportedSchema`
und `ForeignEpoch` werden jeweils Fehlerursache, Safety-Latch, Store-Recovery,
Tombstone, exact CAS/readback, Configuration-/Programm-Erhalt und das Verbot
des alten Runresume geprüft.

### 16.4 Autorisierung, Kapazität und Bypass

Zusätzlich verpflichtend: Service-only S3/Y4 reset, falsche/fehlende
Target-InstanceId, stale FaultRevision, caller-supplied `Allowed`, leerer
FaultCatalog-Slot, voller aktiver Identity-Satz, Plattformadmin-/leere-/mal-
formte Injection-Scope, normaler Configuration-Werksreset, SAFE_BOOT-GPIO-
Anforderung sowie alle relevanten Sensor-/Fan-/Aktor-Injections.

Alle Tests werden mit den Statuswerten `PASS`, `FAILED`, `BLOCKED` oder
`NOT_RUN` aus `docs/CI_AND_QUALITY_GATES.md` dokumentiert. Ein nicht
ausgeführter Test ist kein bestandener Test.

## 17. Dokumentations-, Commit- und Gate-Vertrag

Dieser Plan ist der einzige normative Plan für die neue Branch-/PR-Arbeit.
Der erste Commit enthält ausschließlich diesen Plan und die notwendige
Roadmap-Korrektur. Der PR bleibt Draft. PR-Body und ein einziger aktueller
`SESSION HANDOVER` verweisen auf den exakten Plan-SHA und den aktuellen HEAD.

Vor dem Plancommit sind nur folgende Checks erlaubt und erforderlich:

```text
git diff --check
python scripts/check_architecture_boundaries.py
python scripts/check_secrets.py
```

Builds, native Tests, ESP-IDF-Profile, clang-tidy, Hardware-Smoke und
vollständige CI sind in der Planphase `NOT_RUN`, entsprechend
`docs/CI_AND_QUALITY_GATES.md`. Die Änderungen dieses Plancommits enthalten
keine Produktions- oder Testimplementation.

## 18. Roadmap-Wirkung

`docs/ROADMAP.md` wird am Beginn der neuen PR-Arbeit minimal ergänzt:

```text
Issue #24 – Safety-Core-Neuplanung von aktuellem origin/main; neuer Draft-PR,
Plan-only, Implementation NOT_STARTED, Owner-Gate auf exaktem Plan-SHA.
```

Die bestehende fachliche Reihenfolge #23 → #24 → #19 und die Sperre durch
produktive #56/#57-Producer werden nicht verändert. PR #107/#108 werden nicht
als aktuelle Arbeit eingetragen und nicht geschlossen.

## 19. Freigabekriterien für den Owner

Der Owner gibt vor einer Umsetzung exakt den Plan-SHA frei, der in PR-Body und
SESSION HANDOVER genannt ist. Die Freigabe umfasst ausdrücklich:

- Catalog und `kMaxActiveFaultRecords`;
- SafetyState-Wirefolge, EmergencyMarker und unabhängigen Storage;
- genau eine SafetyFaultService-Autorität;
- S3 `RecoverIfProvable` mit RAM-only Fallback-Auswahl;
- Y4 `runRecoveryForbidden` und Standby-Tombstone;
- #17-Schema 3 ohne Rollback-/Promotion-Feld;
- #18 als alleinige Recoveryentscheidung;
- zentrale Aktor-/Fan-Direktive ohne caller-supplied `Allowed`;
- reale #56/#57-Gates und die vollständige Test-/Crashmatrix.

Nach diesem Plancommit ist anzuhalten. Es gibt keinen ersten Implementierungs-
Slice ohne ausdrückliche Ownerfreigabe exakt dieser Plan-SHA.
