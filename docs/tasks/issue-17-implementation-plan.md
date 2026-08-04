# Implementierungsplan fuer Issue #17

## Status

- Issue: `#17 – Laufpersistenz und Kontrollpunkte`
- Basis: `main@6909b90f518190131eb41c1c707a5b8738d5ba3f`
- Branch: `plan/issue-17-run-persistence-checkpoints`
- Diese Fassung ersetzt alle frueheren Plantexte in PR #84.
- Implementierung bleibt bis zur Freigabe dieses exakten Plan-Commits gesperrt.

```text
PLAN_STATUS: PLAN_DRAFT_REVIEW_REQUIRED
IMPLEMENTATION: NOT_STARTED
DRAFT_PR: OPEN
```

Die additive Planhistorie wird transparent beibehalten; kein Rebase und kein
Force-Push.

## 1. Quellen und Auslegungsregel

Vor Umsetzung sind Issue #17 sowie die aktuellen Fassungen von
`RUN_PERSISTENCE.md`, `ARCHITECTURE.md`, `DECISIONS.md` und der bestehende Code
in `run_snapshot.*`, `process_state_machine.*`, `run_commands.*`,
`configuration_document_codec.*` und `device_platform` zu pruefen.

Bestehende Modelle und Validierungen sind die Quelle der Wahrheit. Dieser Plan
definiert nur die neuen #17-Vertraege. Bei einem materiellen Widerspruch wird
vor Implementierung angehalten.

## 2. Ziel und Grenzen

#17 liefert ein nativ testbares Fundament, das den technischen Speicherzustand
nach Neustart eindeutig klassifiziert als:

- kein Lauf;
- aktueller Lauf;
- `NoActiveRun`-Tombstone;
- gueltiger Rueckfall;
- unterbrochene Transaktion;
- nicht rekonstruierbarer Zustand.

Architektur:

```text
fermentation_app -> device_platform::IStateStore
device_platform_esp_idf -> device_platform
```

Nicht Scope:

- NVS, ESP-IDF, Arduino, GPIO oder Hardware;
- `FermentationApplication`, `IPlatformServices` und Composition Roots;
- Recovery-/Fortschrittslogik aus #18;
- Sensorqualitaet aus #20/#21;
- Fault, Latch, `SAFE_BOOT` und Aktorsperren aus #24;
- Journal und Historie aus #19;
- direkte Aktorzustaende.

## 3. Ownerentscheidungen

1. #17 enthaelt den minimalen persistenten Lauf-Transaktionsmechanismus.
2. Schema 1 persistiert nur heutige #13/#14/#15-Modelle und technische
   #17-Metadaten. Spaetere Fachmodelle erhalten eine neue Schemaversion.
3. Die Integration erfolgt ueber einen `RunPersistenceCoordinator` in
   `fermentation_app`, ohne produktive Plattformverkabelung.
4. Es gibt exakt zwei Kontrollpunktslots und einen separaten Headrecord.
5. Lauf-IDs sind 1 bis 48 Bytes lang; der Tombstone besitzt keine Lauf-ID.

## 4. Persistenzprojektion

#17 persistiert eine `RunPersistenceSnapshot`, nicht den vollstaendigen
`RunCommandState`.

Enthalten:

- Variante `ProgramRun`, `ManualRun` oder `NoActiveRun`;
- `runRevision`;
- `ProcessRuntimeState`;
- Trigger, Intervall und Kontrollpunktzeit;
- optionaler UTC-Anker im bestehenden Envelope;
- maximal 32 bestaetigte eligible Laufkommando-IDs;
- bei `ProgramRun`: Lauf-ID, `RunProgramSnapshot`, `RunRevision`-Folge,
  `ProcessRunSnapshot`;
- bei `ManualRun`: Lauf-ID, `ManualRunPlan`, `ProcessRunSnapshot`.

`NoActiveRun` enthaelt nur leere Lauf-ID, `ProcessState::Standby`,
`runRevision` und das persistierte Laufkommando-Fenster.

Nicht enthalten:

- Meldungs-, Fault-, Safety- oder Journaldaten;
- das globale #15-`processedCommandIds`-Fenster;
- `commandSequence`, `lastCommandMonotonicMillis`;
- Sensor-, Recovery-, Fortschritts- oder Aktordaten.

Ein Restore aus #17 veraendert nur diese Laufprojektion. Andere
`RunCommandState`-Domaenen bleiben unangetastet.

Verbindlich ist:

```text
Projektion aus dem validierten RAM-Kandidaten
=
persistierte Projektion
=
dekodierte Projektion nach Neustart
```

## 5. Laufkommando-Idempotenz

Persistiert werden nur IDs dieser Kommandos:

```text
StartProgram
StartManualHolding
AbortAndTurnOff
AbortAndCool
AcknowledgeCompletion
CoolAfterCompletion
AdjustRun
```

Nicht eligible:

```text
AcknowledgeMessage
MuteMessage
ResetFault
```

Regeln:

- bereits persistierte ID -> `AlreadyPersisted`, kein Write, kein Apply,
  keine Effects;
- nicht eligible -> `NotEligible`, kein Write und kein Apply;
- neue ID wird in den Zielcheckpoint aufgenommen;
- bei 32 Eintraegen wird die aelteste ID verdraengt;
- das Fenster wird erst nach bestaetigtem Commit intern aktualisiert.

## 6. Coordinator

Vorgesehene API:

```cpp
struct RunCheckpointTime {
    std::uint64_t monotonicMillis;
    std::optional<std::int64_t> utcUnixSeconds;
};

RunPersistenceLoadResult loadAndInitialize();

RunPersistenceResult persistCommand(
    RunCommandState& current,
    const CommandDecision& decision,
    const RunCheckpointTime& time);

RunPersistenceResult persistTransition(
    RunCommandState& current,
    const TransitionDecision& decision,
    const RunCheckpointTime& time);

RunPersistenceResult checkpointPeriodic(
    const RunCommandState& current,
    const RunCheckpointTime& time);
```

Der Coordinator haelt eine nicht besessene `IStateStore&`, ist nicht
kopier-/verschiebbar, single-threaded und nicht reentrant.

Lebenszyklus:

```text
Uninitialized
ReadyEmpty
Ready
Busy
Blocked
PersistenceCommittedApplyFailed
```

Vor `loadAndInitialize()` wird nie geschrieben. Rueckfall, Prepared-Head und
nicht rekonstruierbare Daten liefern einen typisierten Ladebefund und setzen
`Blocked`; #18/#24 entscheiden spaeter ueber das weitere Vorgehen.

### Kandidatenpruefung

Vor dem ersten Write:

```text
candidate = current
bestehende apply-Funktion auf candidate ausfuehren
RunPersistenceSnapshot aus candidate bilden und validieren
```

Nur ein erfolgreicher Kandidaten-Apply darf persistiert werden. Nach
bestaetigtem Commit wird dieselbe Decision auf den bis dahin unveraenderten
realen Zustand angewendet. Ein dortiger Fehlschlag setzt fail-closed
`PersistenceCommittedApplyFailed`.

`ProductWaitExpired` entfernt im Kandidaten den aktiven Lauf und erzeugt den
Tombstone.

### Wirkungsfreigabe

Effects und `ProcessMessage` werden nur nach bestaetigtem Commit und
erfolgreichem RAM-Apply ueber das Coordinatorresultat freigegeben. Bei jedem
anderen Ergebnis bleiben sie leer.

Ein Architekturguard verhindert spaetere produktive Direktaufrufe der
`apply*`-Funktionen sowie die Vorabnutzung von Effects und Messages. Reine
Domain-Unit-Tests bleiben erlaubt.

## 7. Erfasste Prozessuebergaenge

Eigenstaendig persistiert:

```text
QualificationTrackingStarted
QualificationReset
PreheatQualified
ProductInserted
ProductWaitExpired
TargetReachTimeExceeded
TargetQualified
FermentationCompleted
CoolingTargetReached
HoldDurationCompleted
```

Nicht separat, weil bereits Teil einer eligible `CommandDecision`:

```text
RunStarted
RunAborted
CompletionAcknowledged
HoldFinishedByUser
TargetChangedReevaluation
```

Alle Boot-, Recovery-, Service-, Fault- und Safety-Uebergaenge sind
ausgeschlossen.

Zulaessige Zustandskombinationen:

| Variante | `ProcessState` |
| --- | --- |
| `ProgramRun` | `Preheating`, `WaitingForProduct`, `ReachingTarget`, `QualifyingTarget`, `Fermenting`, `Cooling`, `CoolHolding`, `Completed` |
| `ManualRun` | `Preheating`, `WaitingForProduct`, `ReachingTarget`, `QualifyingTarget`, `ManualHolding` |
| `NoActiveRun` | nur `Standby` |

## 8. Speicherprotokoll

Stabile Kennungen:

```text
RecordTypeId 7: RunCheckpoint
RecordTypeId 8: RunPersistenceHead
Schema: 1
Keys: rc0, rc1, rh0
```

`rc0` und `rc1` sind die einzigen Kontrollpunktslots. `rh0` ist der
Head-/Transaktionsrecord.

Der Head ist `Prepared` oder `Committed` und referenziert Slots exakt durch:

- Slot-ID;
- Checkpointrevision;
- Schema;
- `StorageEpoch`;
- Payloadlaenge und CRC;
- Checkpointvariante.

Unreferenzierte Slots sind Orphans und werden nie als Wahrheit geraten.

### Mutation

```text
1. Decision auf Kandidat anwenden und Projektion validieren
2. Zielslot und Revisionen bestimmen
3. alle Records vorab kodieren
4. Prepared-Head bestaetigen
5. Zielcheckpoint bestaetigen
6. Committed-Head bestaetigen
7. realen RAM-Zustand anwenden
8. Effects/Messages freigeben
```

Kein Post-Apply-Abschlusswrite. `Committed` bedeutet dauerhaft bestaetigt und
zur RAM-Anwendung freigegeben.

- Fehler vor bestaetigtem Prepared: alter Head bleibt autoritativ.
- Fehler nach bestaetigtem Prepared: kein RAM-Apply, Coordinator `Blocked`.
- Commit bestaetigt, RAM-Apply fehlgeschlagen: fail-closed.

### Periodisch

```text
aktuelle Projektion validieren
-> inaktiven Slot bestaetigen
-> neuen Committed-Head bestaetigen
```

Ein sicher fehlgeschlagener Headwrite laesst den alten Head autoritativ und den
neuen Slot als Orphan. Ein unaufloesbarer Ausgang blockiert.

## 9. `CommitOutcomeUnknown`

Vor jedem Write ist der alte Zustand bekannt als `Absent` oder
`Existing(exakte Bytes)`.

| Readback | Alter Zustand | Ergebnis |
| --- | --- | --- |
| exakt neue Bytes | beliebig | bestaetigt |
| exakt alte Bytes | `Existing` | nicht erfolgt |
| `NotFound` | `Absent` | nicht erfolgt |
| `NotFound` | `Existing` | unaufloesbar |
| andere Bytes, `ReadError`, `CapacityError` | beliebig | unaufloesbar |

Unaufloesbar wird nie als Erfolg behandelt.

## 10. Revisionen und Rueckfall

```text
erste Checkpointrevision: 1
erste Headrevision: 1
erster Zielslot: rc0

kein current -> rc0
current rc0 -> rc1
current rc1 -> rc0
```

- Ein bestaetigter Zielcheckpoint verbraucht eine Checkpointrevision.
- Mutation: `Prepared=N`, `Committed=N+1`.
- Periodisch: eine neue Committed-Headrevision.
- Ueberlauf wird vor dem ersten Write abgelehnt.
- Nach aktivem Commit wird der bisherige current zum Fallback.
- Ein Tombstone hat keinen aktiven Fallback.
- Ein neuer Lauf darf den Tombstone als sicheren Fallback referenzieren.
- Ein beschaedigter Tombstone belebt nie einen alten Lauf.

## 11. Laden

`loadAndInitialize()` arbeitet strikt head-first:

| Befund | Ergebnis | Zustand |
| --- | --- | --- |
| Head und beide Slots fehlen | `NoPersistedRun` | `ReadyEmpty` |
| Head fehlt, Slot vorhanden | `OrphanedState` | `Blocked` |
| `Prepared` | `PreparedInterrupted` | `Blocked` |
| `Committed`, current gueltig | `Current` oder `NoActiveRun` | `Ready` |
| current ungueltig, Fallback gueltig | `FallbackRecovered` | `Blocked` |
| kein gueltiger current/Fallback | `NotReconstructible` | `Blocked` |
| fremde Epoch, Schema- oder Integritaetsfehler | typisierter Fehler | `Blocked` |

Nur vom Head referenzierte Slots duerfen als current oder Fallback gelten.
Der Load liefert eine optionale Laufprojektion, aber keine Recovery-,
Fortsetzungs- oder Safetyentscheidung.

## 12. Schedule

- Intervall 1 bis 60 Minuten, Standard 5;
- explizite monotone Zeit, keine versteckte Uhr;
- rueckwaerts laufende Zeit wird abgelehnt;
- nur bestaetigte Ereignis- oder Periodenwrites setzen die naechste
  Faelligkeit neu;
- vor Faelligkeit, ohne aktiven Lauf sowie in `Blocked` oder fail-closed kein
  periodischer Write;
- kein Write im Sensorzyklus.

## 13. Wireformat Schema 1

Allgemein:

- bestehender Envelope V1;
- Big-Endian;
- Bool nur `0/1`;
- Optionaltag `0/1`;
- Strings/Records mit u16-Laenge;
- keine direkte `std::size_t`-Serialisierung;
- unbekannte Werte, Trunkierung, Zusatzbytes, NaN, Infinity und ungueltige
  Invarianten werden abgelehnt;
- Checkpointpayload maximal 8192 Bytes, Headpayload maximal 256 Bytes;
- `Envelope.versionValue` ist die einzige Recordrevision;
- `Envelope.utcUnixSeconds` ist der Kontrollpunkt-UTC-Anker.

Checkpointheader, exakt einmal:

```text
variant u8
trigger u8
checkpointMonotonicMillis u64
intervalMinutes u16
runRevision u32
runId u16 + Bytes
```

Varianten:

```text
ProgramRun:
  RunProgramSnapshot
  revisionCount u8
  RunRevision[]
  ProcessRunSnapshot
  ProcessRuntimeState
  PersistedRunCommandIds

ManualRun:
  ManualRunPlan ohne runId
  ProcessRunSnapshot
  ProcessRuntimeState
  PersistedRunCommandIds

NoActiveRun:
  ProcessRuntimeState
  PersistedRunCommandIds
```

`EffectiveRunValues` werden durch `ActiveRun::restore()` rekonstruiert und nicht
redundant gespeichert. Die Manual-Run-ID wird beim Restore aus dem Header in
den Plan eingesetzt und danach validiert.

Der bestehende interne Einzelprogrammcodec wird aus
`configuration_document_codec.cpp` als gemeinsame bytegleiche Hilfe
extrahiert. Es entsteht keine zweite ProgramDocument-Kodierung.

Neue Enums erhalten explizite stabile Werte:

```text
CheckpointVariant: ProgramRun=1, ManualRun=2, NoActiveRun=3
CheckpointTrigger: Command=1, Transition=2, Periodic=3, Tombstone=4
HeadState: Prepared=1, Committed=2
MutationKind: Command=1, Transition=2, Tombstone=3
```

Bestehende persistierte Domain-Enums erhalten im Codec explizite,
1-basierte Name-zu-Wirewert-Tabellen in heutiger Deklarationsreihenfolge;
kein ungepruefter `static_cast`. Goldenbytes frieren diese Werte ein.

Verschachtelte Typen folgen den vorhandenen Feldern, aber mit festen
Wirebreiten. Insbesondere werden `stageIndex`, `completedStageCount` und alle
Counts als gepruefte u32 beziehungsweise u8 kodiert. Bestehende `validate*()`
und `ActiveRun::restore()` bleiben die fachliche Invariantenquelle.

## 14. Resultate und Fehlerwirkung

Ein `RunPersistenceResult` enthaelt:

- Status;
- betroffenen Schritt;
- technischen Store-/Codecgrund;
- freigegebene Effects/Messages nur bei Erfolg.

Wesentliche Status:

```text
Applied
CheckpointWritten
AlreadyPersisted
NotEligible
NotInitialized
Busy
InvalidDecision
StaleDecision
TimeRejected
CounterOverflow
PersistenceFailure
PersistenceIndeterminate
PersistenceCommittedApplyFailed
Blocked
```

Wirkung:

- `Applied`: durable Projektion und RAM aktualisiert, Effects/Messages frei.
- `CheckpointWritten`: nur durable Projektion aktualisiert.
- Ablehnung vor Prepared: keine Aenderung, Coordinator bleibt bereit.
- Fehler nach Prepared: kein RAM-Apply, Coordinator blockiert.
- `PersistenceIndeterminate`: durable Aenderung moeglich, blockiert.
- `PersistenceCommittedApplyFailed`: durable Zielprojektion ist Wahrheit,
  fail-closed.

## 15. Dateien

Neue Dateien unter `lib/fermentation_app/src/`:

```text
run_persistence_contract.hpp
run_persistence_codec.hpp/.cpp
run_persistence_store.hpp/.cpp
run_checkpoint_schedule.hpp
run_persistence_coordinator.hpp/.cpp
```

Begruendete Aenderungen:

```text
run_commands.hpp/.cpp
configuration_document_codec.hpp/.cpp
scripts/check_architecture_boundaries.py
scripts/selftest_quality_gates.py
```

`run_commands.*` erhaelt nur die gemeinsame Lauf-ID-Grenze; die bestehende
Decision-/Apply-Logik wird wiederverwendet.

Keine Aenderung an `device_platform`, `device_platform_esp_idf`,
`FermentationApplication` oder den Composition Roots.

## 16. Tests und Gates

Native Tests fuer Codec, Storeprotokoll, Coordinator, Schedule und
Lauf-ID-Grenzen decken mindestens ab:

- alle Varianten, Goldenbytes und ungueltige Wirewerte;
- zwei Slots, Revisionen, Fallback und Tombstone;
- alle Unknown-Outcome-Faelle;
- Stromunterbruch an jeder Transaktionsgrenze;
- Initialisierung, Orphans, Prepared und Rueckfall;
- Idempotenz nach Neustart;
- stale/ungueltige Decisions vor Prepared;
- `ProductInserted` und `ProductWaitExpired`;
- Effects/Messages erst nach Commit plus Apply;
- Schedule und kein Sensorzykluswrite;
- Architekturguard inklusive negativer Fixtures.

Auszufuehren:

```text
python3 scripts/check_architecture_boundaries.py
python3 scripts/selftest_quality_gates.py
pio test -e native
idf.py build fuer esp32_bringup
idf.py build fuer esp32_release
git diff --check
```

Vor einer spaeteren Runtimeaktivierung bleiben aktuelle ESP-IDF-6.0.2-RAM-,
Stack-, Payload- und Flashmessungen zwingend. Release 1 bleibt bei 4 MB Flash
ohne PSRAM. Bei Budgetueberschreitung wird mit Messwerten angehalten und kein
Delta-Design oder groesserer Record still erfunden.

## 17. SOLID, DRY, KISS

- **SRP:** Contract, Codec, Store, Schedule und Coordinator sind getrennt.
- **DIP/OCP:** Der Fachkern kennt nur `IStateStore`; das Backend bleibt
  austauschbar.
- **ISP:** Kein breites neues Plattforminterface.
- **DRY:** Bestehende Modelle, Validierungen, Envelope, CRC, Bytehelfer,
  `ActiveRun::restore()` und ProgramDocument-Codec werden wiederverwendet.
- **KISS:** Zwei Slots, ein Head, eine Projektion, ein Coordinator; kein
  Journal, keine Datenbank, kein Event-Sourcing und kein allgemeines
  Transaktionsframework.

## 18. Stopbedingungen

Anhalten bei:

- Widerspruch zu einer kanonischen Quelle;
- Bedarf an neuem Plattformport oder ESP-IDF-/NVS-Code;
- Bedarf an Modellen aus #18/#19/#20/#21/#24;
- nicht einhaltbarem Ressourcenbudget;
- fachlich oder sicherheitsrelevant offener Alternative.

## 19. Abschluss

```text
FOUR_OWNER_DECISIONS_PRESERVED: PASS
SINGLE_SOURCE_OF_TRUTH: PASS
ARCHITECTURE_ALIGNMENT: PASS
ISSUE_BOUNDARIES: PASS
RUN_PERSISTENCE_PROJECTION: PASS
RUN_TRANSACTION_CONTRACT: PASS
UNKNOWN_OUTCOME_CONTRACT: PASS
COORDINATOR_LIFECYCLE: PASS
RUN_COMMAND_IDEMPOTENCY: PASS
EFFECT_AND_MESSAGE_RELEASE_GATE: PASS
TWO_SLOT_CONTRACT: PASS
HEAD_FIRST_LOAD_CONTRACT: PASS
SCHEMA_1_WIRE_CONTRACT: PASS
SCHEDULE_CONTRACT: PASS
ESP_IDF_BOUNDARY: PASS
SOLID: PASS
DRY: PASS
KISS: PASS
IMPLEMENTATION: NOT_STARTED
DRAFT_PR: OPEN
HALTED_FOR_OWNER_REVIEW
```
