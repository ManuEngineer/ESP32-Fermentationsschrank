# Implementierungsplan fuer Issue #17

## Planstatus

- Issue: `#17 – [E2.2] Laufpersistenz und Kontrollpunkte implementieren`
- Basis: `main@6909b90f518190131eb41c1c707a5b8738d5ba3f`
- Planbranch: `plan/issue-17-run-persistence-checkpoints`
- Ersetzter Plan-Head: `28909f619314184297128b3a503b2079efb966b2`
- Planstatus: `PLAN_DRAFT_REVIEW_REQUIRED`
- Harte Grundlagen: #13, #14, #15 und #54 abgeschlossen.

```text
IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

```text
PROCESS_DEVIATION:
Der ursprünglich verlangte Ein-Korrekturcommit-Prozess wurde überschritten.
Die Historie bleibt unverändert; Bereinigung per Rebase/Force-Push ist unzulässig.
Der Owner entscheidet später über die Annahme dieser transparent dokumentierten Abweichung.
```

Dieser V3-Plan wird in genau einem weiteren additiven Commit dokumentiert.
Implementierung ist ausschliesslich nach `PLAN APPROVED` mit dem exakten SHA
dieses Commits erlaubt.

## Ziel, Architektur und Grenzen

#17 bindet #13/#14/#15 crash-konsistent an `device_platform::IStateStore` aus
#54. Es implementiert später innerhalb `fermentation_app` Fachmodell, Codec,
Zwei-Slot-/Headprotokoll und `RunPersistenceCoordinator`; `device_platform`
bleibt bei vorhandenem Store und technischen Wirebausteinen,
`device_platform_esp_idf` enthält später den realen Adapter.

```text
kanonischen Post-Apply-Zielzustand bilden und vollständig kodieren
-> Prepared-Head dauerhaft bestätigen
-> Zielkontrollpunkt im inaktiven Slot dauerhaft bestätigen
-> Committed-Head dauerhaft bestätigen
-> exakt den persistierten Zielzustand im RAM anwenden
```

Es gibt keinen Completed-Write nach RAM-Apply. `Committed` bedeutet dauerhaft
bestätigt und zur RAM-Anwendung freigegeben, nicht bereits angewandt. Fällt die
Stromversorgung danach aus, ist der committed Zustand die Bootwahrheit.

Nicht Scope: #18-Recovery/Fortschritts- und Zeitintervallentscheidung,
#20/#21-Sensormodelle, #24-Fault/Latch/SAFE_BOOT/Aktorsperre, #19-Journal und
Historie, Konfigurationspersistenz, NVS/ESP-IDF/Preferences/Arduino/GPIO,
neue Plattformports oder produktive Verkabelung in `FermentationApplication`,
`IPlatformServices`, `src/main.cpp` und `main/app_main.cpp`.

## Autoritative Grenzen und kanonischer Zustand

`run_persistence_limits.hpp` definiert die alleinige Lauf-ID-Grenze: 1..48
Bytes, nicht leer und nicht aus Program-ID abgeleitet. Exakt dieselbe
Validierung wird an Programmstart, manuellem Start, Restore und Codec benutzt.

Die spätere #15-Korrektur schreibt bei jeder vorgeschlagenen `CommandDecision`
bereits in `after` das aktualisierte 32er-`processedCommandIds`-Fenster,
`processedCommandCount`, `lastCommandMonotonicMillis` und jede sonstige
fachliche Änderung. `applyRunCommand()` prüft danach nur `before` und übernimmt
exakt `after`; es ergänzt nichts. Damit gilt:

```text
persistierter Zielzustand = RAM nach applyRunCommand() = Restorezustand
```

Das persistierte Idempotenzfenster ist genau dieses begrenzte Fenster plus
Count und Zeit; kein unbegrenztes Journal oder Transport-Replay entsteht.

## Kennungen, Slots und Coordinator

Nächste IDs: `RunCheckpoint=7`, `RunPersistenceHead=8`, beide Schema 1; Keys
`rc0`, `rc1`, `rh0`. `rc0` und `rc1` sind exakt zwei logische
Kontrollpunktslots. `rh0` ist ein separater Head-/Transaktionsrecord, kein
dritter Slot. Headreferenzen bestimmen aktuellen und optionalen Rückfallslot;
Orphans werden nie geraten.

```text
RunPersistenceCoordinator(IStateStore& store, StorageEpoch epoch,
                          RunCheckpointSchedule schedule) noexcept
persistCommand(RunCommandState&, const CommandDecision&) -> RunPersistenceResult
persistTransition(ProcessRuntimeState&, const ProcessRunSnapshot&,
                  const TransitionDecision&, const RunCommandState&) -> RunPersistenceResult
checkpointPeriodic(const RunCommandState&) -> RunPersistenceResult
persistNoActiveRun(const RunCommandState&) -> RunPersistenceResult
loadConfirmed() const -> RunPersistenceLoadResult
state() const -> RunPersistenceCoordinatorState
```

Der Store ist injiziert und überlebt den nicht kopier-/bewegbaren Coordinator.
Er ist single-threaded, nicht reentrant, hat höchstens eine Transaktion und
keine globale Instanz. Busy liefert `MutationBusy`. Ein unerwartet abgelehnter
RAM-Apply nach Committed setzt `PersistenceCommittedApplyFailed` und liefert
einen typisierten internen Vertragsfehler. Bis später #18/#24 ihn behandelt,
sperrt er neue Kommando-/Prozessmutationen, periodische Writes,
Transaktionen, Effects, Aktorabsichten und Runtimeweitergabe, ohne selbst
Fault, SAFE_BOOT, Latch oder Aktorbefehl zu erzeugen.

## Vollständiger Schema-1-Wirevertrag

Alle Integer sind Big-Endian. Bool ist nur `0`/`1`; jedes Optionalfeld hat u8
Tag `0=absent`, `1=present`. String/embedded record ist u16-Länge plus Bytes.
Zähl- und Indexfelder sind feste Breiten, nie `std::size_t`. Decoder prüfen vor
Allokation Länge, Grenzen, Ende des Records und Cross-Field-Invarianten.
Unbekannte Enums/Tags, Trunkierung, Zusatzbytes, NaN, Infinity, negative Null,
0-Revision und jede ungültige Invariante werden typisiert abgelehnt.

| Reihenfolge / Typ | Wire, Grenzen, Enumwerte | Cross-Field / Ablehnung |
| --- | --- | --- |
| Checkpointheader | variant u8: Program=1, Manual=2, Tombstone=3; trigger u8: Command=1, Transition=2, Periodic=3, Tombstone=4; checkpointRevision u64>=1; monotonic u64; interval u16 1..60; runId u16+1..48; commandSequence/runRevision u32 | Revision = Envelope.versionValue; Tombstone nur Tombstone-Trigger |
| RunProgramSnapshot | u16+ProgramDocument-Bytes; sourceKind u8 Factory=1/User=2; sourceRevision u32>=1 | `ActiveRun::start/restore()` akzeptiert |
| ProgramDocument | u32 schema, u64 field mask, id/name/notes je u16+Bytes, 7 bool, Sensor-/Failure-u8, optional u32 Fallback, stageCount u8, je Stage optional binary64/u32, optionale Qualifikation/Limits, Completion-u8 plus optionale Werte | exakt `writeProgram/readProgram` aus `configuration_document_codec.cpp`; bestehende Schema-4/5-, String-, Stage-, Enum- und `validateProgram(Runnable)`-Prüfungen; Byteformat des Catalog unverändert |
| EffectiveRunValues | nicht serialisiert | ausschliesslich `ActiveRun::restore()` aus Snapshot+Revisionen |
| RunTimestamp | monotonic u64, optional unix i64 | jede historische UTC bleibt erhalten; Envelope UTC ist nur Kontrollpunktanker |
| RunRevision | sequence/epoch/stageIndex/completedCount je u32; before/after je binary64+u32; 2 bool; Effect/Source/Reason je u8; Timestamp | count u8 0..32; keine `size_t`; volle `ActiveRun::restore()`-Folge gültig |
| ManualRunPlan | runId u16+1..48; target binary64; sensor u8 Product=1/Air=2; preheat bool; optional wait u32; band binary64; qualification/reach u32; CommandSource-u8; createdAt u64; ProcessKind-u8 Manual=2 | `validateManualRunPlan()` und Lauf-ID-Vertrag |
| ProcessRunSnapshot | kind u8 Timed=1/Manual=2; preheat bool; Completion u8 1..4; qualification/reach u32; optional wait/fermentation/hold u32 | `validateProcessRunSnapshot()` |
| ProcessRuntimeState | state u8 1..15; entered/targetReach u64; optional qualificationSince u64; warning bool; transitionSequence u32 | Form, Zeit und Snapshotbezug gültig |
| processedCommandIds | count u8 0..32, dann count*u64 | keine 0 oder Duplikate |
| processedCommandCount | u8 0..32 | exakt gleich Id-Count |
| lastCommandMonotonicMillis | u64 | bei Count>0 gültige Kommandozeit |
| #15-Command-State-Rest | messageCount u8 0..16 plus jede RuntimeMessage: id u32, Code/Class/Trigger/Acoustic u8, priority u8, monotonic u64, active/ack/resolved/decision/muted je bool, optionale run/state/fault u32, revision u32; messageRevision/faultRevision u32; criticalSafety bool | vorhandene #15-Daten vollständig und ohne neue #19/#24-Semantik erhalten |
| CheckpointReference | slot u8 0/1, revision u64>=1, schema u32=1, epoch u64>=1, length u32<=8192, crc u32, variant u8 1..3 | stimmt exakt mit referenziertem Envelope/Payload überein |
| Head | state u8 Prepared=1/Committed=2, headRevision u64>=1 | = Envelope.versionValue |
| Prepared | old current/fallback, target reference, mutation u8 Command=1/Transition=2/Tombstone=3, optional CommandId u64, alte/neue runRevision/transitionSequence u32 | Ziel inaktiv, Referenzen kollisionsfrei |
| Committed | current reference, optional fallback reference | Slots verschieden; aktiver Tombstone hat keinen aktiven Fallback |

ProgramRun enthält Snapshot und Revisionsfolge; ManualRun nur ManualPlan;
Tombstone keine aktive Laufstruktur. Es werden keine Sensorwerte/-qualität,
Fortschrittsmodelle, Recoverywerte oder Aktorpegel gespeichert. Checkpoint-
Payload <=8192 / Envelope <=8237, Headpayload <=256 / Envelope <=301 Bytes;
Überschreitung stoppt mit Messwerten, nie durch Grenzerhöhung.

## Write-, Readback- und Cut-Point-Vertrag

Jeder Write und jeder Unknown-Outcome wird mit den erwarteten exakten
Envelopbytes zurückgelesen. Neue Bytes bestätigen den Schritt; alte Bytes
bedeuten nicht erfolgt; NotFound, ReadError, CapacityError, korruptes,
fremdes oder widersprüchliches Bytebild sind unauflösbar und niemals Erfolg.

| Schritt | erwartete Bytes | alt / neu / unauflösbar |
| --- | --- | --- |
| Prepared | Head mit bisherigem Stand und Zielreferenz | abbrechen ohne Apply / weiter / `PersistenceIndeterminate` |
| Zielslot | vollständiger Zielcheckpoint im inaktiven Slot | Prepared bleibt, kein Apply / weiter / blockiert |
| Committed | Head mit Ziel aktuell und Altstand fallback | kein Apply / Apply freigegeben / blockiert |
| Periodenslot | Snapshot im inaktiven Slot | kein Headwrite / Head aktualisieren / blockiert |
| Periodenhead | Committed mit neuem aktuell | Orphan, RAM unverändert / Erfolg / blockiert |

Tests decken vor Prepared, WriteError/CapacityError, Unknown alt/neu/
unauflösbar für jeden Schritt, nach Prepared, nach Zielslot, nach Committed
vor Apply, erfolgreichen/abgelehnten Apply und Neustart an jedem Cut-Point ab.
Periodisch werden Slot/Head-Unknown, Orphan und unveränderter RAM geprüft.

## Laden und Transition-Grenze

Laden ist strikt head-first: Head lesen/validieren, nur seinen aktuellen Slot
laden, bei zulässigem Fehler exakt seinen Fallback prüfen, nie höchste
unreferenzierte Revision raten.

| Fall | Ergebnis |
| --- | --- |
| Head NotFound oder beide Slots leer | NoPersistedRun |
| Head Read/Capacity/CRC/Epoch/Schemafehler, Kollision, Widerspruch | NotReconstructible |
| Prepared | PreparedInterrupted, niemals Apply |
| Committed/current gültig | Current oder NoActiveRun |
| current ungültig, referenzierter Fallback gültig | FallbackRecovered |
| beide ungültig, fremde Epoch, neues Schema | NotReconstructible |
| Orphan | Diagnose, nie Wahrheit |
| Tombstone gültig / beschädigt | NoActiveRun / NotReconstructible, nie alten Lauf beleben |

Eigenständige automatische aktive Übergänge: `QualificationTrackingStarted`,
`QualificationReset`, `PreheatQualified`, `ProductWaitExpired`,
`TargetReachTimeExceeded`, `TargetQualified`, `FermentationCompleted`,
`CoolingTargetReached`, `HoldDurationCompleted`. Nicht separat, weil bereits
in CommandDecision.after: `RunStarted`, `RunAborted`,
`CompletionAcknowledged`, `HoldFinishedByUser`, `TargetChangedReevaluation`.
Ausgeschlossen: BootReady/BootSafe/BootRestoreCompleted/BootRecoverRun,
RecoveryResume/RecoveryReject, Enter/ExitServiceMode, CriticalFault sowie alle
Boot-, Recovery-, Fault- und #18/#24-Entscheidungen.

## Umgehungssicherung, Tests und Ressourcen

Nach Umsetzung sind direkte produktive Apply-Aufrufe nur in
`run_commands.cpp`, `process_state_machine.cpp` und
`run_persistence_coordinator.cpp` erlaubt. Verboten sind Runtime/UI/
Composition-Root-Aufrufe in `FermentationApplication`, `src/`, `main/`,
Adaptern und allen anderen Produktionsdateien. Unit-Tests bleiben erlaubt.
`scripts/check_architecture_boundaries.py` erhält den Call-site-Guard;
`scripts/selftest_quality_gates.py` negative Fixtures für Runtime-, UI- und
Composition-Verstösse. Kein Friend-Netz, breites Interface oder Framework.

Neue Tests: `test_run_checkpoint_codec`, `test_run_persistence_head_codec`,
`test_run_checkpoint_store`, `test_run_persistence_coordinator`,
`test_run_checkpoint_schedule`, #15-Post-Apply-Regression, Architekturguard-
Negativtests und Quality-Gate-Selftests. Neue Dateien: Limits, Contract,
Checkpoint/Codec, Head/Codec, Store, Schedule und Coordinator. Nur
`run_commands.*` (Post-Apply/Lauf-ID) und `configuration_document_codec.*`
(interne bytegleiche Einzelprogrammhilfe) dürfen zusätzlich ändern.

Historisch x86-64: `RunCommandState=4520`, `CommandDecision=9264` Byte;
historisch Xtensa/ESP32: `4200` und `8608` Byte. Dies sind getrennte ABI-
Messreihen derselben Messung. Aktueller ESP-IDF-6.0.2-Nachweis: ausstehend.
Das PR-#53-Hardware-Ressourcengate bleibt vor Runtime/UI/Composition-
Aktivierung zwingend; kein vorsorglicher Delta-Decision-Umbau.

SRP trennt Modell, Codec, Head, Store, Schedule und Coordinator. DIP nutzt nur
IStateStore; kein neuer Port. DRY nutzt Envelope, CRC, Reader/Writer,
Slotprüfung, ProgramDocument-Wirelogik, `ActiveRun::restore`, `decide*` und
`apply*`. KISS bleibt bei zwei Slots, einem Head und einem Coordinator ohne
Journal, Datenbank, Event-Sourcing oder Zukunftsmodelle.

## Abschluss der Planprüfung

```text
ARCHITECTURE_ALIGNMENT: PASS
ISSUE_BOUNDARIES: PASS
RUN_TRANSACTION_CONTRACT: PASS
CANONICAL_POST_APPLY_STATE: PASS
FAIL_CLOSED_CONTRACT: PASS
COORDINATOR_BYPASS_GUARD: PASS
SCHEMA_1_WIRE_CONTRACT: PASS
UTC_CONTRACT: PASS
TRANSITION_SCOPE: PASS
APPLICATION_INTEGRATION: PASS
TWO_SLOT_CONTRACT: PASS
ESP_IDF_BOUNDARY: PASS
RESOURCE_BASELINE: PASS
SOLID: PASS
DRY: PASS
KISS: PASS
IMPLEMENTATION: NOT_STARTED
DRAFT_PR: OPEN
HALTED_FOR_OWNER_REVIEW
```
