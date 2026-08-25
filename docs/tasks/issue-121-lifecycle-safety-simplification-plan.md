# Issue #121 – Release-1 Device-/Application-Lifecycle- und Safety-Policy-Vereinfachung (Plan R1)

## Status, Ziel und Owner-Gate

Diese Datei ist die vollständige, eigenständig ausführbare kanonische
Planrevision **R1** für Issue #121. Sie enthält **keine
Produktionsimplementation**. Bis zur ausdrücklichen Freigabe der exakten
Commit-SHA dieser Datei gilt:

```text
LIFECYCLE_SIMPLIFICATION_PLAN_PENDING_OWNER_APPROVAL
IMPLEMENTATION=NOT_STARTED
PRODUCTION_CODE_CHANGED=NO
TEST_CODE_CHANGED=NO
MATERIAL_ARCHITECTURE_DECISION_OPEN=NO
```

`MATERIAL_ARCHITECTURE_DECISION_OPEN=NO` gilt nur für die in dieser Datei
tatsächlich getroffenen und in Abschnitt 5–13 geschlossenen Entscheidungen.
Die Freigabe autorisiert noch keine Umsetzung.

```text
BASE_PR=#118
BASE_BRANCH=agent/issue-90-clean-restart-plan-r5.8
BASE_SHA=fd7e4e3ec58c9f3dda45710fd346752e083d7d19
ISSUE=#121
DRAFT_PR=#122
```

Keine #120-Implementation wird kopiert oder cherry-gepickt. Der Composition
Root (`main/app_main.cpp`) ist auf dieser Basis unverändert der #118-Stand:
`NvsOwningContext` erzeugt den `NvsStateStore`, aber `FermentationApplication`
verdrahtet bislang nur ein `SafetyCore` mit leerem `SafetyCoreInput`
(Abschnitt 4.1). R1.1 aus #119/PR #120 (Boot-Composition-Helper im Root) wird
**nicht** übernommen; die dort gemessene Root-Cause-Evidenz
(`app_main()`-Stackframe 112 → 16.432 Byte) wird als Randbedingung für
Abschnitt 12 (Composition) übernommen, nicht als Code.

### Korrekturrunden (diese Revision)

Der Owner hat sechs Fassungen dieser Plandatei zurückgewiesen:

- SHA `ea4f057` (Korrekturrunde 1): 6 Blocker, 5 Major-Befunde, 1 Minor-Befund.
- SHA `666525e` (Korrekturrunde 2): 7 Blocker, 5 Major-Befunde.
- SHA `fdb240a` (Korrekturrunde 3): 3 Blocker, 6 Major-Befunde.
- SHA `cd64c8f` (Korrekturrunde 4): 6 Blocker, 5 Major-Befunde.
- SHA `117b0d4` (Korrekturrunde 5): 7 Blocker, 2 Major-Befunde.
- SHA `658873c` (Korrekturrunde 6): 5 Blocker, 3 Major-Befunde.

Alle Befunde aller sechs Runden wurden gegen den realen Code auf `BASE_SHA`
nachverifiziert (siehe Abschnitt 4, neu ergänzte Unterpunkte) und sind in
dieser Fassung **in-place** korrigiert, nicht als Anhang neben dem alten
Text. Es gibt keine R2-/R3-/R4-/R5-/R6-/R7-Planrevision; diese Datei bleibt
„R1", sechsfach konsolidiert. **Korrekturrunde 6 hat Runde 5s eigenen
Reset-Vertrag für die neuen `Into()`-Kerne als selbst wieder stack-unsicher
erkannt:** `destination = CommandDecision{};`/`destination =
RunPersistenceSnapshot{};`/`destination = {status, std::nullopt};`
materialisieren als Zuweisungs-RHS ein **prvalue**-Temporary des vollen
Zieltyps – seit C++17 ist Kopierelision nur für Initialisierung garantiert,
nicht für Zuweisung, also exakt dieselbe Nicht-Elision-Lücke, die dieser
Plan bereits gegen NRVO bei Return-by-value-Pfaden verwendet. Korrektur:
ein field-by-field-Reset-Vertrag (neuer Abschnitt 12.4.9), der pro Typ nur
die Felder zurücksetzt, die nicht ohnehin in jedem Aufruf unbedingt
überschrieben werden (z. B. `CommandDecision.before`/`.after` brauchen
keinen Reset, da `resultInto()` sie unbedingt aus `current` überschreibt) –
real an den echten Schreibfunktionen nachgewiesen
(`run_commands.cpp:187-197`, `run_persistence_contract.cpp:411-464`), nicht
angenommen. Runde 6 korrigierte außerdem einen weiteren pauschalen
Fehlvertrag: der globale `nothrow`-Allokationsvertrag deckte nur die
**expliziten** `new (std::nothrow) T(...)`-Objektgrenzen ab, nicht die
verschachtelten `std::string`/`std::vector`-Allokationen innerhalb
`RunCommandState`/`RunPersistenceSnapshot` – der Vertrag wurde ehrlich auf
`EXPLICIT_ISSUE121_OBJECT_ALLOCATION_USES_NOTHROW` verengt (neuer Abschnitt
12.4.10), ohne einen neuen Allocator/PMR einzuführen. Zusätzlich bereinigte
Runde 6 einen Source-of-Truth-Widerspruch: Abschnitt 6 behauptete weiterhin
„kein einziger Codezeilen-Diff in `run_persistence_codec.cpp`", während
Abschnitt 12.4.6/14 (Runde 5) dort bereits verbindlich
`decodeRunPersistenceSnapshotInto()`/`decodeRunPersistenceRecordInto()`
verlangten. **Korrekturrunde 5 hat Runde 4s eigenen
Stack-Sicherheits-Vertrag (Abschnitt 12.4) als unvollständig erkannt: der
Vertrag schloss nur die Aufrufer-Ebene, nicht die intern von
`RunPersistenceCoordinator` und `run_persistence_codec.cpp` selbst
konstruierten grossen Werte.** Real geprüft: `writeSnapshotCore()`
(`run_persistence_coordinator.cpp:1571`) konstruiert weiterhin einen lokalen
`RunPersistenceRawRecord record{...}` (real gemessen `sizeof(
RunPersistenceRawRecord)=4152`, enthält selbst ein vollständiges
`RunPersistenceSnapshot`); `loadAndInitialize()`s `loadReference`-Lambda
(Zeile 347-386) gibt `std::optional<RunPersistenceRawRecord>` per Wert
zurück, zweimal aufgerufen als `currentRecord`/`fallbackRecord`-Lokale
(Zeile 388/465); `decodeRunPersistenceSnapshot()`
(`run_persistence_codec.cpp:1151`) konstruiert intern `RunPersistenceSnapshot
s;` als lokale Variable. Zusätzlich real gemessen (Host-Build):
`sizeof(RunPersistenceCoordinator)=8968` (die Klasse selbst ist bereits
gross, da `slots_[2]` zwei `std::optional<RunPersistenceRawRecord>` als
**Instanzmember** hält, nicht erst durch diesen Plan). Korrektur: ein
Coordinator-eigenes `RunPersistenceWorkingSet`-Wertmember
(`candidate`/`snapshot`/`record`, ersetzt Runde 4s separates
`candidateScratch_`) plus durchgängige In-place-/Out-Parameter-Kerne für
Snapshot-Projektion (`makeRunPersistenceSnapshotInto()`), Codec-Decode
(`decodeRunPersistenceSnapshotInto()`/`decodeRunPersistenceRecordInto()`)
und Laden (`loadAndInitializeInto()`) schliessen jetzt auch diese
Callee-interne Ebene (Abschnitt 12.4, vollständig überarbeitet). Runde 5
korrigierte ausserdem: Runde 4s Reentrancy-Begründung „das Scratch ist
durch `state_ == Busy` geschützt" war real falsch – `candidate`/`snapshot`
werden in `persistCommand()`/`discardAsNoActiveRun()` **vor**
`writeSnapshotCore()`s `state_ = Busy`-Zuweisung konstruiert (real geprüft,
`run_persistence_coordinator.cpp:1495`); der tatsächlich tragfähige
Reentrancy-Vertrag ist eine explizite Single-Task-Aufrufaffinität, kein
`state_`-basierter Lock (Abschnitt 12.4). Runde 4s Formulierung
„`candidateScratch_` wird im Konstruktor mit `new (std::nothrow)`
alloziert; der Konstruktor gibt bei Fehler `nullptr`/einen Fehlerstatus
zurück" war zusätzlich technisch unmöglich (ein Konstruktor hat keinen
Rückgabewert) und ist mit dem neuen `RunPersistenceWorkingSet`-Wertmember
gegenstandslos geworden: `RunPersistenceCoordinator` selbst wird bereits
als Ganzes über `new (std::nothrow)` durch `FermentationApplication`
alloziert (unverändert gegenüber Runde 4s §12.1-Ownership-Tabelle) – keine
zweite, separate Scratch-Allokation nötig. `decideProgramStartInto()`/
`decideManualStartInto()` waren in Runde 4 nur als Wrapper um die
bestehenden Return-by-value-Kerne (`beginDecision()`/`result()`) skizziert;
real geprüft (`run_commands.cpp:187-221`, `:687-797`) mutieren diese Kerne
ein einziges lokales `CommandDecision decision`-Objekt (`sizeof(
CommandDecision)=10520`) durchgängig über die gesamte Funktion – ein
Wrapper hätte sich auf nicht garantierte NRVO verlassen. Korrektur: ein
neuer `beginDecisionInto()`-Kern schreibt direkt in die vom Aufrufer
übergebene `CommandDecision&`; `decideProgramStartInto()`/
`decideManualStartInto()` operieren durchgängig über eine Referenz auf das
Zielobjekt statt über eine lokale Kopie; die bestehenden
Return-by-value-Funktionen delegieren jetzt umgekehrt an die neuen
In-place-Kerne (ein `CommandDecision decision; decideProgramStartInto(...,
decision); return decision;`), statt dass die In-place-Variante auf der
Return-by-value-Variante aufbaut. Schliesslich korrigierte Runde 5 die
Scope-Aussage in Abschnitt 14 („keine Änderung an
`run_persistence_codec.cpp`"/„`RunPersistenceCoordinator` (außer der einen
neuen Methode)"): beide Dateien erhalten jetzt zusätzlich rein mechanische
In-place-/Out-Parameter-Helfer zur Stack-Sicherheit, ohne Wire-/Schema-/
Semantikänderung (Byte-for-byte-Regression aller Schema-1/2/3-Fixtures
bleibt Pflichttest). **Korrekturrunde 4 hat das von Runde 3 als „später
zu messendes Restrisiko" behandelte Coordinator-Stackproblem als bereits
bewiesenen Blocker eingeordnet:** `docs/ISSUE_29_BUILD_REPORT.md` (reale
Xtensa-`-fstack-usage`-Messung auf `BASE_SHA`) dokumentiert
`RunPersistenceCoordinator::persistCommand(...) = 9280 B` statischen Frame
– bereits einzeln über `CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584`, unabhängig
von jeder Call-Path-Summierung. Zusätzlich real gemessen:
`sizeof(CommandDecision)=10520`, `sizeof(RunPersistenceSnapshot)=4096`,
`sizeof(RunPersistenceLoadResult)=4112` – ebenfalls alle einzeln über dem
Budget. Ein vollständiger Stack-Sicherheits-Vertrag (Out-Parameter-
Varianten für `RunCommandState`/`CommandDecision`-Restaurierung,
Coordinator-eigenes einmalig alloziertes Scratch, boot-transiente
Heap-Speicherung für `RunPersistenceLoadResult`, verbindlicher statischer
Produkt-Stackgate vor jeder Hardwarefreigabe) ist jetzt Abschnitt 12.4.
Runde 4 hat außerdem einen realen Fehler in Runde 3s eigenem Entwurf
gefunden: mehrere Flows verwendeten `std::make_unique<RunCommandState>(...)`
statt `new (std::nothrow) RunCommandState(...)` – `make_unique` ist
werfend, nicht `nothrow`, und erfüllt den im selben Plan geforderten
„prüfe auf `nullptr`"-Vertrag nicht (korrigiert, Abschnitt 12.4.1). Ferner
kompiliert `isValidFallbackRecoveryEvidence()` mit dem seit Runde 1
geltenden, `persistenceSnapshot`-freien `ActuationEvidence`-Input nicht
mehr – gedroppt statt angepasst, da R1s `FallbackRecovered`-Policy die
dortige Vertrauensausnahme ohnehin strukturell ausschließt (Abschnitt 10.3).
Korrekturrunde 2 hat zusätzlich einen realen Fehler
in Runde 1 selbst aufgedeckt und behoben: der damalige Beweis „`Boot` wird
nie persistiert, weil `RunPersistenceSnapshot.processState` nur bei
`variant != NoActiveRun` geschrieben wird" (Abschnitt 7.2) war sachlich
falsch – `makeRunPersistenceSnapshot()` kopiert `processState` unbedingt, vor
jeder Variant-Verzweigung (real geprüft). Der korrigierte, tatsächlich
tragfähige Beweis (Abschnitt 7.2) beruht auf `validStateFor()`/
`validateRunPersistenceSnapshot()`, die `ProcessState::Boot` für jede
Variante ausschließen und jeden Encode-Aufruf gaten (Abschnitt 4.2). Runde 2
hat außerdem einen zweiten, bislang unentdeckten Bug in Runde 1s eigenem
Entwurf gefunden: die für den R1-Resume-Schreibpfad vorgesehene
`RunPersistenceFallbackMode::ClearFallback`-Direktive hätte
`writeSnapshotCore()`s `directiveValid`-Prüfung nie bestanden (nur für
`Fault`/`NoActiveRun`-Snapshots zulässig, nicht für die drei aktiven
R1-Resume-Phasen) – korrigiert auf `UseStandardFallback` (Abschnitt 9.1).
**Korrekturrunde 3 hat einen realen, hart gemessenen Stack-Fehler in Runde
2s eigenem Entwurf gefunden:** `runtimeRunState_`/`pendingResume_` waren als
`std::optional<RunCommandState>`-Wertmember spezifiziert –
`std::optional<T>` speichert `T` **inline**. `sizeof(RunCommandState)` ist
real gemessen `5096` Byte (Abschnitt 9); zwei solcher Felder hätten
`sizeof(FermentationApplication)` um über 10 KB vergrößert, bei
`CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584` exakt dieselbe Stack-Fehlerklasse wie
bei #120 reproduziert. Korrigiert auf `std::unique_ptr<RunCommandState>`
(Abschnitt 9/12.1). Runde 3 hat außerdem ein verwandtes, **nicht** von
diesem Plan eingeführtes, sondern bereits auf `BASE_SHA` bestehendes
Stack-Risiko benannt (der `auto candidate = current;`-Kopiervorgang in
mehreren `RunPersistenceCoordinator`-Methoden, ebenfalls `~5096` Byte, jetzt
erstmals über den realen Produktpfad erreichbar) – als offenes, zu
messendes Risiko dokumentiert, nicht stillschweigend gelöst
(Abschnitt 12.2).

```text
ARCHITECTURE_AUDIT=COMPLETED
ARCHITECTURE_AUDIT_OWNER_REVIEW=PASS_WITH_CORRECTIONS
ARCHITECTURE_AUDIT_SOURCE_TEXT=NOT_PERSISTED_NOT_RECOVERABLE
ARCHITECTURE_VERDICT=SIMPLIFY
PLAN_BASIS=FRESH_CODE_INVENTORY_PLUS_OWNER_CORRECTIONS_A_J
PRIOR_PLAN_SHA=658873c62b35ce183e41866f1a48cad8a53ad567
PRIOR_PLAN_REVIEW=CORRECTION_REQUIRED
EARLIER_PLAN_SHA=117b0d4d6d5853dd26d20f23e9910da0767a0c19
EARLIEST_PLAN_SHA=cd64c8fca0d970ccc952c4cd9c0676b776cf1a20
FIRST_PLAN_SHA=ea4f05723bdcf78fd6e081484ef6ab0cb28f1bf6
```

## 1. Ownerkorrekturen A–J (verbindlich, vollständig referenziert)

Unverändert gegenüber der Vorfassung; hier erneut vollständig aufgeführt, da
diese Datei standalone bleibt.

- **A – Fail-closed ≠ Boot-Erfolg.** Ein Crash/Panic vor Freigabe ist
  funktional schlecht, verletzt aber nicht automatisch die fail-closed-
  Invariante. Getrennt zu führen:
  `FAIL_CLOSED_BOOT_POLICY`, `PRODUCT_BOOT_COMPLETION`,
  `PHYSICAL_BOOT_OUTPUT_SAFETY` (Letzteres bleibt #29-Gate). `Allowed` darf
  nie aus fehlender Evidenz entstehen.
- **B – `RunRecoveryCoordinator` unangetastet.** Er ist real vorhanden
  (`lib/fermentation_app/src/run_recovery.hpp/.cpp`), aber nicht produktiv
  verdrahtet. Kein Refactor, keine Löschung, keine Reduktion nur wegen seiner
  C2-/Fallback-/Weighted-Recovery-Fähigkeiten.
  `RunRecoveryCoordinator = PRESERVE_LEGACY_NOT_ACTIVE_R1`. Eine kleine neue
  R1-spezifische API an anderer Stelle (z. B. `RunPersistenceCoordinator`)
  verletzt diese Korrektur nicht, solange `RunRecoveryCoordinator` selbst
  unangetastet bleibt.
- **C – Wire-/Schema-Kompatibilität.** `BREAKING_PERSISTENCE_CHANGE=NO`,
  `SCHEMA_MIGRATION_REQUIRED=NO`, `WIRE_ENUM_RENUMBERING=NO`,
  `LEGACY_DECODING_REMOVAL=NO`. Insbesondere `ProcessState::Boot`,
  `::SafeBoot`, `::RecoveryEvaluation`, `::ServiceMode`, `::Fault` bleiben im
  Codec unverändert; alte Werte bleiben lesbar, auch wenn die neue aktive
  Policy sie nicht mehr neu erzeugt.
- **D – Keine neue BootPolicy-Serviceplattform.** `BootClassification` ist
  ein kleiner Werttyp/eine kleine deterministische Funktion, keine Registry,
  kein Service Locator, kein Pluginmodell, keine generische Recovery-Engine.
- **E – `APPLICATION_READY != ACTUATION_ALLOWED`.** Fehlende oder fehlerhafte
  Prozesssensor-Evidenz setzt nicht pauschal `SERVICE_REQUIRED`; sie blockiert
  nur die konkret betroffene Aktorfreigabe.
- **F – ResumeOffer-Ownership.** Ein vertrauenswürdig geladener,
  resumefähiger Snapshot lebt als `BootClassification::ResumeOffer` mit
  Aktoren `DENIED`, ohne aktiven `ProcessRuntimeState`, bis zur expliziten
  Nutzerentscheidung. Keine neue persistierte ResumeOffer-State-Machine.
- **G – SafetyCore wirklich verkleinern, nicht nur umbenennen.** Für
  `SafetyDisposition`, `activeFaultMask_`, `acknowledgedFaultMask_`,
  `unknownProducerSources_`, `canClearFault()`, `resetRequestWatchdog()`,
  `FaultCode` ist je ein Keep/Move/Drop-Urteil gegen den kleinen Interlock
  erforderlich. Producer bleiben Autorität ihrer eigenen Wahrheit; Ack ist
  Presentation und bleibt nur bei konkretem technischem Grund im Interlock.
  Der #23-Watchdog-Latch bleibt beim `ActuatorPlanner`. Umbenennung nur, wenn
  sie Klarheit bringt.
- **H – Permission/Reaktion getrennt, ImmediateStop-Vertrag erhalten.**
  `ActuationPermission = DENIED | ALLOWED` ist ein mögliches Ziel, aber nur
  wenn Permission, Denial-/Reaktionsgrund und physische Reaktion getrennt
  bleiben und der bestehende Peltier-Sofortsperr-/Lüfternachlaufvertrag
  (#23/#24) vollständig erhalten bleibt. Keine rein kosmetische
  Enum-Vereinfachung.
- **I – `INITIALIZING != STANDBY`.** Kein Verbraucher darf vor positiver
  Bootklassifikation `STANDBY` als Freigabeevidenz missverstehen; ein
  konkreter Publikationsmechanismus ist zu wählen, nicht mehrere zur Auswahl
  zu stellen.
- **J – `SERVICE_REQUIRED != SERVICE_MODE`.** `SERVICE_REQUIRED` = Device/
  Application kann normalen Betrieb nicht anbieten. `SERVICE_MODE` = bewusst
  gewählter Bedien-/Wartungsmodus. Kein Servicemodus erzeugt implizite
  Aktorfreigabe. Keine neue Berechtigungs-/PIN-Plattform.

## 2. Herkunft und Abgrenzung

Issue #119/PR #120 hat versucht, die produktive `IStateStore`-Composition
zwischen `device_platform_esp_idf::NvsStateStore` und
`FermentationApplication` herzustellen. Der ursprüngliche Boot-WDT wurde
real behoben (R1.1), die reale Reverifikation lief danach aber
deterministisch in einen weiterhin ungeklärten `LoadProhibited`-Panic
unmittelbar vor `composeAndBeginApplication(...)`. Unabhängig davon zeigte
der Architektur-Audit, dass die betroffenen Verträge (`SafetyCore`,
`ProcessStateMachine`, `RunPersistenceCoordinator`,
`ConfigurationRecoveryService`, `ActuatorPlanner`, Composition Root) für
Release 1 unnötig komplex ineinandergreifen. Dieser Plan schließt die
Composition-Lücke **und** vereinfacht die Verantwortungsteilung in einem
Schritt, auf der #118-Basis, ohne die #120-Implementation zu übernehmen.

Nicht Gegenstand: eine neue Ursachenanalyse des `LoadProhibited`-Panics.

## 3. Zielarchitektur-Überblick

Fünf Verantwortlichkeiten, je eine Autorität, keine doppelte Policy:

```text
Device/Application Lifecycle   -> FermentationApplication (erweitert)
Process Lifecycle              -> ProcessStateMachine (unveraendert)
Boot Classification            -> neu: boot_classification.hpp/.cpp
Persistence Technical Result   -> RunPersistenceCoordinator (+ 1 neue kleine
                                   R1-Methode, Abschnitt 9)
Actuation Permission           -> ActuationInterlock (umbenannt aus SafetyCore, verkleinert)
```

**Scope-Korrektur dieser Revision (Blocker 5):** Der produktive
ESP-IDF-Pfad von #121 bleibt vollständig **actor-free**.
`ActuatorPlanner`, `ActuatorPlanSinkDriver`, `TemperatureController`,
`TargetQualificationEvaluator` und die 3-/6-Parameter-Form von
`TemperatureControlApplicationOrchestrator` werden in dieser Revision
**nicht** produktiv in `FermentationApplication::begin()` komponiert (Grund:
`TemperatureController` verlangt reale `TemperatureControlParameters` im
Konstruktor, Abschnitt 4.9 – das würde Blocker 11 verletzen). **Präzisierung
gegenüber der Vorfassung (Major 8, Runde 6):** „vollständig unangetastet"
war ungenau – Abschnitt 4.9 verlangt verbindlich mechanische
API-Änderungen (`SafetyCore&`-Member entfällt, Konstruktor 6→5 Parameter,
`tickActuatorPlan(..., currentGate)`). Korrekt:

```text
ORCHESTRATOR_PRODUCT_COMPOSITION=UNCHANGED_NOT_ACTIVE
ORCHESTRATOR_DOMAIN_POLICY=UNCHANGED
ORCHESTRATOR_MECHANICAL_INTERLOCK_SIGNATURE_MIGRATION=IN_SCOPE
```

Der Orchestrator bleibt Ziel künftiger Issues (#33/#106) und wird in dieser
Revision **nicht produktiv komponiert**; seine Fachlogik/Domänenpolitik
bleibt unverändert, nur die mechanische Interlock-Signatur (Abschnitt 4.9)
migriert. Native/Integrationstests dürfen ihn weiterhin mit bestehenden
Mock-Parametern prüfen (Abschnitt 13). `FermentationApplication` ruft für
Fresh Start und R1-Resume `RunPersistenceCoordinator` **direkt** auf, nicht
über den Orchestrator (Abschnitt 8/9).

## 4. Current-State-Inventur (auf `BASE_SHA`, real geprüft)

### 4.1 `FermentationApplication`

`lib/fermentation_app/src/fermentation_application.hpp/.cpp` (26+33 Zeilen).
Einziges Feld: `SafetyCore safetyCore_`. `begin()` ruft
`safetyCore_.beginBoot(resetCause)` und wertet ein **leeres**
`SafetyCoreInput` aus. Kein `ConfigurationRecoveryService`, kein
`RunPersistenceCoordinator`, kein `RunRecoveryCoordinator`, kein
`TemperatureControlApplicationOrchestrator`, kein `ActuatorPlanner`
verdrahtet.

### 4.2 `ProcessStateMachine`

`process_state_machine.hpp/.cpp` (223+1085 Zeilen). `ProcessState` (15
Werte, Wirecodes siehe Abschnitt 6): `Boot=1, SafeBoot=2, Standby=3,
Preheating=4, WaitingForProduct=5, ReachingTarget=6, QualifyingTarget=7,
Fermenting=8, Cooling=9, CoolHolding=10, ManualHolding=11, Completed=12,
RecoveryEvaluation=13, Fault=14, ServiceMode=15`.

Boot-Übergänge laufen ausschließlich über `decideBootEvent()`, `ServiceMode`
ausschließlich aus `Standby` über `ProcessEvent::EnterServiceMode`/
`ExitServiceMode`. `ProcessState::Fault` wird ausschließlich über
`TransitionReason::CriticalFault` von einem aktiven Laufzustand aus erreicht
– ein reiner **Run-Lifecycle**-Endzustand.

`ProcessRuntimeState.state` **defaultet auf `ProcessState::Boot`**
(process_state_machine.hpp:73) – Stolperstein für Korrektur I (Abschnitt
7.2, **präzisiert**: Abschnitt 7.2 legt jetzt fest, dass `Boot` ein rein
interner, nie publizierter/persistierter Initialisierungswert bleibt, nicht
„aus der aktiven Policy entfernt" – Blocker 8).

**`validControlTopology()` (Zeile 241–270, real geprüft für diese Revision):**
Jede `TransitionReason` hat eine feste `from`/`to`-Bedingung.
`TransitionReason::RecoveryResumed` verlangt zwingend
`from == ProcessState::RecoveryEvaluation` – eine Selbsttransition
(`from == to == Preheating/Cooling/ManualHolding`) mit diesem Reason wird
von `applyProcessTransition()` **abgelehnt**. Ein direkter R1-Resume-Pfad
(Abschnitt 9) kann `applyProcessTransition()`/`propose()` deshalb **nicht**
für den Phasenwechsel nutzen und muss stattdessen dem bereits im Code
vorhandenen Präzedenzfall aus `activateLoadedRun()`s `Completed`-Zweig
folgen (Abschnitt 4.4): dort wird `candidate.processState.
stateEnteredAtMillis` **direkt** gesetzt, ohne `propose()`/
`applyProcessTransition()`-Aufruf. Kein neuer `TransitionReason`-Wert nötig,
keine Änderung an `process_state_machine.hpp/.cpp`.

`propose()`/`applyProcessTransition()` bleiben unverändert exportiert und
werden für `NoRun`/`ResumeRejected` (Abschnitt 7, dort `from`/`to` bereits
durch `validBootTopology()`/`RecoveryRejected` gedeckt) weiterhin verwendet.
**Korrektur (Major 8, Runde 2):** `CompletedRun` und `TerminalRunFault`
laufen **nicht** über `propose()`/`applyProcessTransition()` – `validBootTopology()`
(real geprüft) kennt **keine** `Boot→Fault`-Kante; ein
`propose(Boot, Fault, ...)`-Aufruf für `TerminalRunFault` wäre damit
semantisch falsch, selbst wenn `stateCanEnterFault(Boot)` ihn technisch nicht
zwingend ablehnen würde. Beide Flows laufen stattdessen über den erweiterten
`RunPersistenceCoordinator::activateR1EligibleRun()` (Abschnitt 9.1), der
exakt den bestehenden `Fault`-/`Completed`-Präzedenzfall aus
`activateLoadedRun()` direkt wiederverwendet (Abschnitt 4.4) – kein neuer
`TransitionReason`, keine `process_state_machine.hpp/.cpp`-Änderung.

**Neu real geprüft (Beweisbasis für Abschnitt 7.2, Major 9,
`run_persistence_contract.cpp:14-53`):** `validStateFor(variant, state)`
lehnt `ProcessState::Boot` für **jede** Variante explizit ab –
`ProgramRun`/`ManualRun` führen je eine feste Whitelist aktiver Zustände, die
`Boot` nicht enthält; `NoActiveRun` verlangt exakt `state == Standby`.
`validateRunPersistenceSnapshot()` (Zeile 304+) ruft `validStateFor()` als
ersten Prüfschritt auf und gated **jeden** Encode-Aufruf
(`run_persistence_codec.cpp:1081` und `:1259`, real geprüft) – ein Snapshot
mit `processState.state == Boot` kann diesen Encode-Pfad nie durchqueren,
unabhängig davon, welche Variant-Verzweigung `makeRunPersistenceSnapshot()`
intern nimmt.

### 4.3 `SafetyCore` (712+196 Zeilen)

Unverändert gegenüber der Vorfassung (siehe dortige Detailanalyse,
Abschnitt 10.2 dieser Datei übernimmt die Keep/Move/Drop-Tabelle).
**Neu real geprüft (Blocker 7):** `SafetyDisposition` wird auf `BASE_SHA`
**ausschließlich** von `TemperatureControlApplicationOrchestrator::
tickActuatorPlan()` konsumiert – und zwar nicht der `SafetyDisposition`-Wert
selbst, sondern nur `lastEvaluation().gate` (`ActuatorSafetyGateInput`,
nicht `SafetyDisposition`). Es gibt **keinen** realen, nicht-Test-Consumer
von `SafetyDisposition` auf `BASE_SHA`. Urteil (Abschnitt 10.2): **Drop**.

### 4.4 `RunPersistenceCoordinator` (290 Zeilen hpp, vollständig gelesen;
relevante `.cpp`-Abschnitte real geprüft für diese Revision)

Bereits vollständige API: `loadAndInitialize()`, `activateLoadedRun()`,
`activateFallbackRecoveredRun()`, `resolveRecoveryOutcome()`,
`discardAsNoActiveRun()`, `persistCommand`, `persistTransition`,
`persistSensorSelection`, `checkpointPeriodic`.

**`activateLoadedRun()` – vollständig real geprüft (Korrektur zur
Vorfassung, Blocker 2):** Für `current.processState.state == Fault` bzw.
`== Completed` gibt es je einen kleinen Sonderzweig, der **direkt**
`candidate.processState.stateEnteredAtMillis`/`state_ = Ready` setzt, ohne
`propose()`. **Präzisiert (Runde 2, Zeile 528–556 real gelesen):** Beide
Sonderzweige sind **RAM-only** – keiner ruft `writeSnapshotCore()` auf, das
Ergebnis trägt `RunPersistenceDurability::Unchanged`. Der `Fault`-Zweig lässt
`current` vollständig unverändert (keine Feldmutation außer `state_`); der
`Completed`-Zweig mutiert ausschließlich `candidate.processState.
stateEnteredAtMillis` lokal und gibt `candidate` zurück, ohne
`transitionSequence` zu erhöhen. Beide sind der exakte Präzedenzfall für den
erweiterten `activateR1EligibleRun()` (Abschnitt 9.1), der dieselben zwei
Zweige unverändert übernimmt (nicht neu erfindet) und zusätzlich die drei
R1-resumefähigen Phasen abdeckt – für sie ist (anders als für `Fault`/
`Completed`) ein echter durabler Schreibvorgang erforderlich, da ein aktiver
Lauf nach Resume weiterläuft und der Coordinator seinen internen
`currentHead_`/`slots_`-Zustand konsistent mit dem rebasten Zustand halten
muss.

Für **jeden anderen** Zustand mit `stateUsesRunSnapshot(...)`
– einschließlich der drei R1-resumefähigen Phasen `Preheating`/`Cooling`/
`ManualHolding` – konstruiert die Funktion unbedingt einen
`PendingRecoveryAnchor`, erhöht `recoveryEpisodeRevision`, setzt
`lastRecoveryEpisodeEvidence` und geht über
`propose(originalProcessState, ProcessState::RecoveryEvaluation,
TransitionReason::RecoveryReentryRequired, ...)` **immer** zuerst in
`ProcessState::RecoveryEvaluation` – nie direkt zurück in die
Ursprungsphase. Das ist der vollständige #18/C2-Recovery-Pfad
(Zeitverdikt, `foldObservedRunSeconds`, gewichtete gefaltete Segmente).
**`activateLoadedRun()` ist damit für R1 nicht wiederverwendbar, auch nicht
eingeschränkt auf die drei R1-Phasen.**

**Neu real gefunden (schließt Blocker 1):** `run_persistence_contract.hpp`
exportiert bereits

```cpp
// Technical restoration only: reconstructs exactly the run projection. It
// does not make recovery, boot, fault or safety decisions.
[[nodiscard]] std::optional<RunCommandState> restoreRunPersistenceSnapshot(
    const RunPersistenceSnapshot& snapshot);
```

bereits heute vielfach von der bestehenden Testsuite verwendet
(`test/test_run_persistence_coordinator`, `test/test_run_checkpoint_codec`).
Das ist die vollständige, bereits vorhandene, bereits getestete
Rekonstruktion eines `RunCommandState` aus einem geladenen
`RunPersistenceSnapshot` – exakt der fehlende Baustein für den
ResumeOffer-Ownership-Vertrag (Abschnitt 9).

**`eligibleTransition()` (Zeile 191–209, real geprüft):** nur 12
`TransitionReason`-Werte sind für den öffentlichen `persistTransition()`-
Eingang zulässig; `RecoveryResumed`/`RecoveryReentryRequired`/
`RecoveryRejected` gehören **nicht** dazu. `persistTransition()` verlangt
zusätzlich `state_ == Ready` (nicht `LoadedActiveRun`). Ein R1-Resume-Pfad
kann deshalb nicht über den öffentlichen `persistTransition()`-Eingang
laufen, sondern muss – wie `activateLoadedRun()` selbst – eine eigene
Methode auf `RunPersistenceCoordinator` sein, die intern die bereits
private `writeSnapshotCore(...)` verwendet (Abschnitt 9).

### 4.5 `ConfigurationRecoveryService`

Unverändert gegenüber Vorfassung. Kein Änderungsbedarf.

### 4.6 `ConfigurationService`

**Neu real geprüft (Blocker 6):** Konstruktor
(`configuration_service.hpp:257-260`) verlangt zwingend drei Referenzen:

```cpp
ConfigurationService(ConfigurationMutationCoordinator& mutationCoordinator,
                      ConfigurationGraphStore& graphStore,
                      const device_platform::ITimeZoneResolver& timeZoneResolver);
```

Alle drei müssen mindestens so lange leben wie `ConfigurationService`
selbst (Abschnitt 12.1, Ownership-Tabelle).

**Neu real geprüft (schließt Blocker 7, `StorageEpoch`-Quelle für
`RunPersistenceCoordinator`):** `ConfigurationService::acquireRuntime()`
(Zeile 269) liefert ein `RuntimeConfigurationReadResult{status, lease}` mit
`RuntimeConfigurationReadStatus ∈ {RuntimeLeaseGranted, RuntimeReadLeaseBusy,
ConfigurationRuntimeUnavailable}`. Die zurückgegebene
`RuntimeConfigurationReadLease` ist RAII, `.get()` liefert eine
`const RuntimeConfigurationSnapshot&`, deren `.storageEpoch()`-Methode
(`runtime_configuration_snapshot.hpp`, real geprüft) den
`device_platform::StorageEpoch`-Wert liefert, den
`RunPersistenceCoordinator`s Konstruktor benötigt. Das ist die reale,
bereits bestehende Quelle – kein neuer Epoch-Mechanismus. Vertrag für
`FermentationApplication::begin()` (Abschnitt 12): `acquireRuntime()` wird
**nach** Schritt 3 (`ConfigurationRecoveryService::boot()`) und **vor**
Schritt 4 (`RunPersistenceCoordinator`-Konstruktion) aufgerufen; bei
`status != RuntimeLeaseGranted` wird `RunPersistenceCoordinator` **nicht**
konstruiert, `FermentationApplication` setzt `SERVICE_REQUIRED` und
`Actuation DENIED` (derselbe fail-closed-Vertrag wie jeder andere
Boot-Fehlerpfad, Abschnitt 7), `begin()` läuft ohne
`RunPersistenceCoordinator`-Member weiter (`unique_ptr` bleibt `nullptr`,
Abschnitt 12.1). Die Lease selbst lebt nur für die Dauer des
Epoch-Auslesens (lokale Variable in `begin()`, kein Application-Lifetime-
Member).

**Neu real geprüft, echte zusätzliche Lücke, nicht in der Ownervorgabe
benannt:** Auf `BASE_SHA` existiert **keine produktive**
`device_platform_esp_idf`-Implementierung von `ITimeZoneResolver` –
lediglich das abstrakte Interface (`lib/device_platform/src/
time_zone_resolver.hpp`) und ein Test-Mock
(`lib/device_platform_test_support/src/mock_time_zone_resolver.hpp`). Ohne
einen produktiven Adapter kann `ConfigurationService` im echten ESP-IDF-Pfad
nicht konstruiert werden. Das ist derselbe Fund, den bereits die #119-Plandatei
(Abschnitt 5.1 dort) als eigenständige Lücke identifiziert und gelöst hatte
– hier wird **nicht** deren Code übernommen (Ownerauftrag: kein #120-Code),
sondern die Notwendigkeit eines neuen, kleinen, eigenständig in #121 zu
implementierenden `EspTimeZoneResolver`-Adapters als expliziter Scope-Zusatz
festgehalten (Abschnitt 12.2). Die konkrete Adapter-Ownership liegt dabei
bei der ESP-IDF-Composition-Root; `fermentation_app` konsumiert ausschließlich
die bereits bestehende abstrakte `device_platform::ITimeZoneResolver`-
Referenz. Dadurch bleibt die verbindliche ADR-013-Abhängigkeit
`fermentation_app -> device_platform` unverändert.

### 4.7 `ActuatorPlanner` / `ActuatorPlanSinkDriver`

Unverändert gegenüber Vorfassung (Abschnitt 4.7-Analyse bleibt gültig); in
dieser Revision **nicht produktiv komponiert** (Abschnitt 3).

### 4.8 `main/app_main.cpp` (193 Zeilen, `BASE_SHA`-Stand)

`NvsOwningContext::create()` initialisiert die `state_store`-Partition und
öffnet den `NvsStateStore`; der Store wird **nicht** an `application.begin()`
übergeben. **Neu real geprüft (Blocker 12):** `NvsOwningContext` besitzt auf
`BASE_SHA` **keinen** öffentlichen `store()`-Accessor – nur die privaten
Felder `config_`/`store_` (Zeile 89–90). Dieser Accessor war Teil der
späteren #120-Implementation und wird **nicht** übernommen; er ist als
eigener, minimaler #121-Zusatz zu implementieren (Abschnitt 12.2).

### 4.9 `TemperatureControlApplicationOrchestrator` (real vorhanden, in
dieser Revision nicht produktiv komponiert)

Datei `temperature_control_orchestrator.hpp/.cpp` (178+ Zeilen). Bleibt
unverändert existent und wiederverwendbar (native Tests), aber **nicht**
Teil der #121-Produktcomposition (Abschnitt 3, Blocker 5/11):

- Beide Konstruktoren (3- und 6-Parameter) verlangen `TemperatureController&`
  bzw. zusätzlich `ActuatorPlanner&`/`ActuatorPlanSinkDriver&`.
  **Neu real geprüft:** `TemperatureController`s Konstruktor
  (`temperature_control.hpp:49`) verlangt zwingend
  `TemperatureControlParameters parameters` als Wert – ein reales
  Produktparameter-Objekt (Regelparameter), dessen produktive Werte laut
  Issue #24/#121-Scope erst durch #35/#106 geliefert werden dürfen
  (Ownerkorrektur/Blocker 11: `INVENTED_PRODUCT_DEFAULTS=FORBIDDEN`). Eine
  Composition dieses Orchestrators im echten Produktpfad würde daher
  entweder erfundene Parameter oder eine verfrühte #35/#106-Abhängigkeit
  erzwingen. Deshalb: **kein** produktiver Konstruktoraufruf in #121.
- `persistFreshStartCommand()`/`reconcileR1LoadedRun()` sind reine
  Persistenz-Convenience-Wrapper um `RunPersistenceCoordinator`-Aufrufe
  (Abschnitt 8/9 zeigen die direkten Coordinator-Aufrufe, die #121
  stattdessen produktiv verwendet – funktional äquivalent für den
  Persistenzpfad, ohne die automatischen PI-/Planner-Resets, die ohne
  reale `TemperatureController`/`ActuatorPlanner`-Instanzen ohnehin
  wirkungslos wären).
- `activateRecovery(RunRecoveryCoordinator&, ...)`: wird von #121 nicht
  aufgerufen (Korrektur B bleibt dadurch trivial erfüllt).
- **Mandatorische mechanische Anpassung (Blocker 4, unabhängig vom
  Composition-Scope):** `tickActuatorPlan()` liest heute
  `safetyCore_->lastEvaluation().gate` (Zeile 346–347) – ein zustandsbehaftetes
  „letztes Ergebnis"-Cache-Read auf dem Interlock-Objekt. Da der neue
  `ActuationInterlock` (Abschnitt 10) als reine Funktion ohne
  `lastEvaluation()`-Methode geplant ist, **muss** diese Datei angepasst
  werden, damit das Repository überhaupt compiliert – unabhängig davon, ob
  der 6-Parameter-Pfad produktiv komponiert wird. Auflösung: Der
  `SafetyCore&`/`safetyCore_`-Member entfällt vollständig aus der Klasse;
  `tickActuatorPlan()` erhält stattdessen einen expliziten Parameter
  `const ActuatorSafetyGateInput& currentGate`, den der Aufrufer unmittelbar
  vor dem Tick aus einem frischen `ActuationInterlock::evaluate(...)`-Ergebnis
  liefert. `evaluateTemperatureControl()` liest `safetyCore_` nicht (real
  geprüft) und ist von dieser Änderung nicht betroffen. Konstruktor wird
  5-parametrig (`persistence, temperatureController, evaluator, planner,
  driver`), kein `SafetyCore&`/`ActuationInterlock&`-Parameter mehr.

### 4.10 `RunRecoveryCoordinator` (existiert, nicht aktiv)

Unverändert gegenüber Vorfassung.

## 5. Zielverantwortungen (5 Autoritäten, keine Doppel-Policy)

| Verantwortung | Typ | Autorität für |
|---|---|---|
| Device/Application Lifecycle | `FermentationApplication` (erweitert) | `INITIALIZING` / `READY` / `SERVICE_REQUIRED`; Komposition der übrigen vier; hält den einen kanonischen `runtimeRunState_` (Abschnitt 9, Blocker 1), aus dem `publishedProcessState()` als reine Projektion liest |
| Boot Classification | `boot_classification.hpp/.cpp` (neu, frei Funktion) | Klassifikation eines geladenen Snapshots + Konfigurationsvertrauen zu genau einem der Boot-Flow-Ergebnisse (Abschnitt 7) |
| Process Lifecycle | `ProcessStateMachine` (unverändert) | Laufzustand eines aktiven Laufs (Preheating…Fault) **und** weiterhin `ServiceMode` (Abschnitt 7.1, unverändert, nicht verschoben). Boot-transiente Werte (`Boot`, `SafeBoot`, `RecoveryEvaluation`) bleiben im Wire-Enum bestehen (Abschnitt 6), werden aber von der aktiven `BootClassification`-Policy nicht mehr neu erzeugt – die *Boot-Entscheidung selbst* liegt bei `BootClassification`, nicht bei `ProcessStateMachine` |
| Persistence Technical Result | `RunPersistenceCoordinator` (+ 1 neue kleine R1-Methode, Abschnitt 9) | technische Speicherwahrheit, `RunPersistenceLoadResult`, `RunPersistenceResult` |
| Actuation Permission | `ActuationInterlock` (umbenannt/verkleinert aus `SafetyCore`) | `DENIED`/`ALLOWED` für den `ActuatorPlanner`-Eingang (Autorität etabliert, physischer Consumer folgt in späteren Issues, Abschnitt 3) |
| Fault/Diagnostic Presentation | `PresentationState` (Abschnitt 11) | UI-/Diagnoseanzeige, nie gate-relevant |

**Präzisierung (Major 8, Runde 6):** „`RunPersistenceCoordinator` (+ 1 neue
kleine R1-Methode)" ist als **fachliche** Autoritätsaussage korrekt
(`activateR1EligibleRun()` ist die einzige neue Domänenpolitik-Methode),
verschweigt aber die zusätzlichen rein mechanischen Stack-Sicherheits-Helfer
aus Abschnitt 12.4 (`workingSet_`, `loadAndInitializeInto()`,
`makeRunPersistenceSnapshotInto()` u. a.):

```text
NEW_DOMAIN_POLICY_METHODS=activateR1EligibleRun only
ADDITIONAL_STACK_SAFETY_HELPERS=MECHANICAL_NO_NEW_POLICY_AUTHORITY
```

Die Architektur-Autorität bleibt unverändert klein; der reale Source-Diff
in `RunPersistenceCoordinator` ist grösser als „eine neue Methode", ohne
dass dies eine zweite Policy-Autorität einführt.

## 6. Legacy-/Wire-Vertrag

**Korrektur gegenüber der Vorfassung (Source-of-Truth-Widerspruch, Blocker 5,
Runde 6):** Dieser Abschnitt behauptete bislang „`run_persistence_codec.cpp`
bleibt vollständig unverändert … kein einziger Codezeilen-Diff in dieser
Datei" – das widersprach Abschnitt 12.4.6/14, die dort bereits verbindlich
`decodeRunPersistenceSnapshotInto()`/`decodeRunPersistenceRecordInto()`
verlangen (Blocker 3, Runde 5). Der korrekte, jetzt widerspruchsfreie
Vertrag:

```text
RUN_PERSISTENCE_CODEC_WIRE_SEMANTICS=UNCHANGED
RUN_PERSISTENCE_CODEC_SCHEMA=UNCHANGED
RUN_PERSISTENCE_CODEC_BYTES=UNCHANGED
RUN_PERSISTENCE_CODEC_VALIDATION_SEMANTICS=UNCHANGED
RUN_PERSISTENCE_CODEC_SOURCE_DIFF=MECHANICAL_STACK_SAFETY_HELPERS_ONLY
RUN_PERSISTENCE_CODEC_STACK_SAFE_IMPLEMENTATION_HELPERS=IN_SCOPE
OLD_CODEC_NO_DIFF_TEXT=NONE
```

Encode und decode beider Richtungen bleiben in ihrer **Wire-Semantik**
exakt wie auf `BASE_SHA` – kein neues Feld, kein neuer Enumwert, keine
geänderte Byte-Reihenfolge, keine geänderte Validierungssemantik
(Byte-for-byte-Regression aller Schema-1/2/3-Fixtures bleibt Pflicht,
Abschnitt 12.4.6/13). Der reale Source-Diff besteht ausschließlich aus den
in Abschnitt 12.4.6 spezifizierten, rein mechanischen In-place-Helfern.
**Klarstellung dieser Revision:**
Die in Abschnitt 7/9 beschriebene geänderte Behandlung von
`RunPersistenceLoadStatus::FallbackRecovered` ist eine Änderung der
**aktiven Klassifikationslogik** (`boot_classification.cpp`), **keine**
Wire-/Codec-Änderung – `RunPersistenceLoadStatus` selbst ist ohnehin kein
Wire-Enum (es wird pro Boot aus dem realen Store-Zustand technisch
abgeleitet, nicht persistiert).

| Wire-Wert | `ProcessState` | alte Bedeutung | neu aktiv erzeugt? | Legacy-Decode? | Klassifikation beim Lesen | Migration |
|---:|---|---|:---:|:---:|---|:---:|
| 1 | `Boot` | Boot-transienter Zustand vor Klassifikation | **Nein** – bleibt rein interner, nie publizierter Initialisierungswert (Abschnitt 7.2) | Ja | s. u. | NEIN |
| 2 | `SafeBoot` | Boot-transiente Safe-Boot-Anzeige | **Nein** | Ja | s. u. | NEIN |
| 3 | `Standby` | kein aktiver Lauf, bereit | Ja (unverändert) | Ja | – | NEIN |
| 4–11, 14 | Preheating…Fault | aktive Run-Lifecycle-Zustände | Ja (unverändert) | Ja | – | NEIN |
| 12 | `Completed` | Lauf abgeschlossen | Ja (unverändert) | Ja | – | NEIN |
| 13 | `RecoveryEvaluation` | Boot-transiente Wartezustand auf Resume/Reject | **Nein** | Ja | s. u. | NEIN |
| 15 | `ServiceMode` | bewusst gewählter Bedienmodus | **Ja, unverändert** (Abschnitt 7.1) | Ja | – | NEIN |

**Klassifikation beim Lesen für 1/2/13 (korrigiert, Major 4, Runde 3 – die
Vorfassung behandelte alle drei Werte fälschlich über denselben Mechanismus;
real sind es zwei verschiedene, beide bereits heute unverändert korrekte
Mechanismen, kein neuer Code für keinen der drei Fälle):**

- **`Boot`/`SafeBoot` (1/2):** Diese erreichen **nie** einen erfolgreichen
  `RunPersistenceLoadStatus::Current`-Load. `validStateFor(variant, state)`
  (Abschnitt 4.2) lehnt `Boot`/`SafeBoot` für **jede** Variante ab (weder in
  der `ProgramRun`- noch der `ManualRun`-Whitelist, `NoActiveRun` verlangt
  exakt `Standby`); `decodeRunPersistenceSnapshot()`
  (`run_persistence_codec.cpp:1259`, real geprüft) ruft
  `validateRunPersistenceSnapshotForSchema()` **innerhalb** des Decodierens
  auf – ein solcher Payload liefert `RunPersistenceCodecStatus::
  InvalidSnapshot`, keinen erfolgreich decodierten Snapshot. Der reale Lade-
  Fehlerpfad mündet in `RunPersistenceLoadStatus::NotReconstructible` (oder
  eine der benachbarten technischen Fehlerstatus, `run_persistence_
  coordinator.hpp:114-127`), die die bereits bestehende, unveränderte
  `classifyRunLoad()`-Switch (`safety_core.cpp:450-458`, real geprüft) direkt
  auf `RunLoadDisposition::SafeBoot` abbildet – **ohne** `isR1ResumeEligible()`
  je aufzurufen. Wire-Enum-Decodierbarkeit (Tabelle oben, Spalte
  „Legacy-Decode?") ist nicht dasselbe wie ein gültiger, ladbarer
  Persistence-Snapshot – Ersteres bezieht sich nur auf die reine
  `ProcessState`-Zahl-zu-Enum-Abbildung, Letzteres auf den vollständigen,
  vom Variant abhängigen `validStateFor()`-Vertrag.
- **`RecoveryEvaluation` (13):** Ist dagegen in **beiden** Whitelists
  (`ProgramRun`/`ManualRun`) enthalten, also ein gültiger, ladbarer
  `Current`-Snapshot. `classifyRunLoad()` erreicht hier tatsächlich
  `isR1ResumeEligible()`, die `RecoveryEvaluation` explizit ausschließt
  (`safety_core.cpp:401`, real geprüft) → `RunLoadDisposition::NoActiveRun`
  → `BootClassification::DiscardableRun`. Das ist die vom Owner geforderte
  **explizite, einzelne** R1-Policy-Entscheidung für diesen Fall (nicht
  „offen gelassen"):

  ```text
  LEGACY_RECOVERY_EVALUATION_POLICY=DiscardableRun
  ```

  Begründung: `RecoveryEvaluation` repräsentiert einen unentschiedenen
  C2-Zwischenzustand aus einer früheren, nicht-R1-Firmware; ein Discard
  (statt `SERVICE_REQUIRED`) ist wegen des vom Owner akzeptierten
  Chargeverlusts zulässig und erfordert keinen neuen Code – die bestehende
  `isR1ResumeEligible()`-Logik erzeugt dieses Ergebnis bereits unverändert.

`FaultCode` ist nicht in dieser Tabelle, weil es real nirgends persistiert
wird (Abschnitt 4.3) und daher keinem Wire-Vertrag unterliegt.

## 7. Boot-Flows

`BootClassification` (neuer Werttyp, Datei `boot_classification.hpp`):

```cpp
enum class BootClassification : std::uint8_t {
    Unresolved,      // interner Zwischenzustand waehrend der Klassifikation
    NoRun,           // = altes "Standby": kein persistierter Lauf/leer
    ResumeOffer,
    DiscardableRun,  // = altes "NoActiveRun": vertrauenswuerdig, nicht resumefaehig
    CompletedRun,    // = altes "Completed"
    TerminalRunFault,// = altes "TerminalFault"
    SafeBoot,        // inkl. FallbackRecovered (NEU, Abschnitt 7.3) und
                      // Config-/Persistenzfehler
};
```

| Flow | Application Lifecycle | Process State | Persistence Action | Actuation | Diagnose |
|---|---|---|---|---|---|
| `NoRun` | `READY` | `runtimeRunState_ = std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState()}` (Abschnitt 12.4.1, Runde 4 – nicht `make_unique`), `processState.state == Standby` (via `propose()`+`TransitionReason::BootCompleted`, deckt sich mit `validBootTopology()`); Allokationsfehler → fail-closed, `SERVICE_REQUIRED` statt `READY` (Abschnitt 9 Blocker 1); published erst nach erfolgreicher Allokation | keine | `DENIED` (bis Fresh Start) | keine |
| `ResumeOffer` | `READY` | **nicht published** | Snapshot bleibt technisch in `RunPersistenceCoordinator` (`LoadedActiveRun`); rekonstruierter `RunCommandState` lebt **in `FermentationApplication`** (Abschnitt 9, Blocker 1) | `DENIED` | ResumeOffer-Anzeige mit Snapshot-Vorschau |
| `ResumeConfirmed` | `READY` | published nach `RunPersistenceCoordinator::activateR1EligibleRun()` (Abschnitt 9.1, **neu**, deckt alle drei `LoadedActiveRun`-Ausgänge ab – Fault/Completed/3 Resume-Phasen –, kein `activateLoadedRun()`) | `activateR1EligibleRun()`, durabler Schreibvorgang (Recovery-Mutation, `UseStandardFallback`) | `DENIED` bis frische Interlock-Evidenz nach `Applied` | Lauf läuft weiter |
| `ResumeRejected` | `READY` | published: `Standby` nach Discard | `RunPersistenceCoordinator::discardAsNoActiveRun()` (write-before-apply, bestehend) | `DENIED` | Verworfen-Hinweis optional |
| `DiscardableRun` | `READY` | published: `Standby`, nur bei `Applied`, nach `discardAsNoActiveRun()` mit einem via `restoreRunPersistenceSnapshotInto()` rekonstruierten `RunCommandState&` (Abschnitt 9, Major 6 – **nicht** ein leerer/minimaler Stub) | `discardAsNoActiveRun()`, write-before-apply | `DENIED` | keine (stiller Discard, wie heute); bei Fehlschlag `SERVICE_REQUIRED` statt stillem Discard (Abschnitt 9 Fehlerklassifikation) |
| `CompletedRun` | `READY` | `target` heap-alloziert (`new (std::nothrow)`) + `restoreRunPersistenceSnapshotInto(*loaded.snapshot, *target)` (Abschnitt 12.4.2, Major 9, Runde 4 – identisches Muster wie `DiscardableRun`), dann published: `Completed` via `activateR1EligibleRun(*target, time, nullptr)` (Abschnitt 9.1, exakter `activateLoadedRun()`-Completed-Präzedenzfall: RAM-only, `stateEnteredAtMillis`-Refresh; nur bei `Applied`: `runtimeRunState_ = std::move(target)`) | RAM-Mutation, `RunPersistenceDurability::Unchanged` (kein Store-Schreibvorgang) | `DENIED` | Abschluss-Anzeige |
| `TerminalRunFault` | `READY` | `target` heap-alloziert (`new (std::nothrow)`) + `restoreRunPersistenceSnapshotInto(*loaded.snapshot, *target)` (Abschnitt 12.4.2, Major 9, Runde 4 – identisches Muster wie `DiscardableRun`), dann published: `Fault` via `activateR1EligibleRun(*target, time, nullptr)` (Abschnitt 9.1, exakter `activateLoadedRun()`-Fault-Präzedenzfall: RAM-only, unveränderter `current`; nur bei `Applied`: `runtimeRunState_ = std::move(target)`; **kein** `propose(Boot→Fault)` – `validBootTopology()` kennt diese Kante nicht, Abschnitt 4.2 Major 8) | RAM-Mutation, `RunPersistenceDurability::Unchanged` (kein Store-Schreibvorgang) | `DENIED` | Fehler-Anzeige |
| `UntrustedConfiguration` | `SERVICE_REQUIRED` | nicht published | keine | `DENIED` | `FaultCode ∈ {ConfigurationUnavailable, ConfigurationIntegrityFailure, ConfigurationCommitIndeterminate}` |
| `UntrustedPersistence` | `SERVICE_REQUIRED` | nicht published | keine | `DENIED` | `FaultCode = RunPersistenceUntrusted` |
| **`FallbackRecovered`** (neu benannter Flow, Abschnitt 7.3) | `SERVICE_REQUIRED` | nicht published | **kein** Discard/Tombstone | `DENIED` | eigener Diagnosehinweis „Fallback-Wiederherstellung, kein automatischer Resume" |
| `SensorUnavailableAtStartup` | **`READY`** (Korrektur E) | published: `Standby` | keine | `DENIED`, Grund `SafetySensorUnavailable` sobald Aktivierung versucht wird | Sensor-Warnung, kein Service-Zustand |
| `WatchdogLatched` | `READY` (Laufzeit-, kein Boot-Flow) | unverändert | keine | `DENIED`, Grund `RequestWatchdogFaultLatched` | Watchdog-Anzeige |

### 7.1 Servicebegriffe: `SERVICE_MODE` vs. `SERVICE_REQUIRED` (Korrektur J, geschlossen)

```text
SERVICE_MODE   = ProcessState::ServiceMode, UNVERAENDERT (Abschnitt 4.2).
                 Eigentuemer: ProcessStateMachine.
                 Nur aus Standby ueber ProcessEvent::EnterServiceMode
                 betretbar, nur ueber ExitServiceMode wieder verlassbar.

SERVICE_REQUIRED = Application-Lifecycle-Wert, Eigentuemer:
                 FermentationApplication. Gesetzt bei
                 BootClassification::SafeBoot (inkl. UntrustedConfiguration/
                 UntrustedPersistence/FallbackRecovered, Abschnitt 7) sowie
                 bei bestimmten Runtime-Producerfehlern (Abschnitt 7.4).
```

Beide sind unabhängig. `ServiceMode` erzeugt laut `activationEvidenceComplete()`
(unverändert) strukturell nie `ALLOWED` – fail-closed `DENIED` ohne neuen
Code. Keine neue Berechtigungs-/PIN-Plattform.

### 7.2 Bootzustandspublikation: `INITIALIZING` vs. `STANDBY`, und was mit `Boot` passiert (Korrektur I, Blocker 8, geschlossen)

**Entscheidung (unverändert gegenüber Vorfassung):**
`FermentationApplication` exponiert den Prozesszustand als
`std::optional<ProcessRuntimeState> publishedProcessState()`. Solange
`BootClassification` noch nicht abgeschlossen ist (`INITIALIZING`), bleibt
dieser Optional leer.

**Präzisierung dieser Revision (schließt Blocker 8 endgültig, zweite vom
Owner angebotene Option):** `ProcessState::Boot` und sein Default-Initialisierer
`ProcessRuntimeState::state{ProcessState::Boot}` (process_state_machine.hpp:73)
bleiben **unverändert im Code** und werden **nicht** „aus der aktiven Policy
entfernt" – diese Formulierung aus der Vorfassung war irreführend und wird
zurückgenommen. Der korrekte, beweisbare Vertrag lautet:

```text
ProcessState::Boot bleibt ein rein interner, nie extern beobachtbarer
Initialisierungswert eines default-konstruierten ProcessRuntimeState.
```

Beweis, dass `Boot` nie persistiert/published/als Permission verwendet wird:

1. **Nie published:** Jeder in Abschnitt 7 definierte Flow published
   entweder gar nichts (`ResumeOffer`, `SafeBoot`-Familie) oder ruft
   explizit `propose()`/den neuen `activateR1EligibleRun()`/
   `discardAsNoActiveRun()`-Pfad mit einem konkreten Zielzustand
   (`Standby`, `Completed`, `Fault`, der resumierten Phase) auf. Es gibt
   **keinen** Codepfad in `FermentationApplication`, der
   `publishedProcessState()` mit einem default-konstruierten
   `ProcessRuntimeState` befüllt.
2. **Nie persistiert (korrigierter Beweis, Runde 2 – Major 9; die
   Vorfassung hatte hier fälschlich behauptet, `processState` werde nur bei
   `variant != NoActiveRun` geschrieben; real gilt das Gegenteil:
   `makeRunPersistenceSnapshot()`, Zeile 415, kopiert `snapshot.processState
   = state.processState` **unbedingt**, vor jeder Variant-Verzweigung):**
   Der tatsächlich tragfähige Beweis liegt nicht in der Snapshot-Konstruktion,
   sondern in der Validierung davor: `validStateFor(variant, state)`
   (`run_persistence_contract.cpp:14-53`, real geprüft) lehnt
   `ProcessState::Boot` für **jede** der drei Varianten explizit ab
   (`ProgramRun`/`ManualRun`-Whitelists enthalten `Boot` nicht,
   `NoActiveRun` verlangt exakt `Standby`). `validateRunPersistenceSnapshot()`
   ruft `validStateFor()` als ersten Schritt auf und gated jeden
   Encode-Aufruf (`run_persistence_codec.cpp:1081`/`:1259`, real geprüft,
   Abschnitt 4.2). Ein Snapshot mit `processState.state == Boot` kann diesen
   gated Encode-Pfad nie erreichen – unabhängig von der unbedingten Kopie in
   `makeRunPersistenceSnapshot()`. Zusätzlich, enger und aus dem Plan selbst
   ableitbar: Kein Schreibpfad dieses Plans konstruiert je einen Kandidaten
   mit `processState.state == Boot` – die vier Schreibstellen
   (`activateR1EligibleRun()`s drei Zweige, Abschnitt 9.1, sowie
   `discardAsNoActiveRun()`) starten entweder aus
   `restoreRunPersistenceSnapshotInto()`s Ausgabe (die selbst nur aus einem
   bereits validierten, also nie-`Boot`-Snapshot rekonstruiert) oder aus
   einem `propose()`-erzeugten `Standby`-Kandidaten.
3. **Nie Permission:** `ActuationEvidence` (Abschnitt 10) enthält keinen
   `ProcessState`-Rohwert; `activationEvidenceComplete()`-artige Prüfungen
   hängen an `activationKind`/`processActivationApplied`/
   `activationPersistenceResult`, nicht am konkreten `ProcessState`-Wert.

Die erste tatsächliche Veröffentlichung geschieht ausschließlich über die
bestehenden Exportfunktionen `propose()`/`applyProcessTransition()` (für
`NoRun`/`ResumeRejected`, Abschnitt 4.2) bzw. über die direkte Feldsetzung im
neuen `activateR1EligibleRun()`-Pfad (für `CompletedRun`/`TerminalRunFault`/
den aktiven Resume, Abschnitt 9.1, mirrored aus dem bestehenden `Completed`-/
`Fault`-Präzedenzfall in `activateLoadedRun()`). **Korrektur gegenüber der
Vorfassung (Major 5, Runde 3):** `CompletedRun`/`TerminalRunFault` wurden
hier fälschlich weiterhin bei `propose()`/`applyProcessTransition()`
aufgeführt – das widerspricht Major 8 (Abschnitt 4.2), wonach beide
ausschließlich über `activateR1EligibleRun()`s RAM-only-Zweige laufen, da
`validBootTopology()` keine `Boot→Fault`-Kante kennt.

### 7.3 `FallbackRecovered` erzeugt in R1 kein `ResumeOffer` (Blocker 3, geschlossen)

**Befund (real geprüft, Vorfassung war falsch):** Die aus `SafetyCore::
classifyRunLoad()` migrierte Logik behandelte `RunPersistenceLoadStatus::
Current` und `::FallbackRecovered` bislang identisch – beide konnten
`RunLoadDisposition::ResumeOffer` liefern. Für R1 ist das nicht zulässig:
ein Fallback-wiederhergestellter Zustand ist technisch geladen, aber nicht
derselbe vertrauenswürdige Zustand wie ein direkter `Current`-Load.

**Korrektur:** `boot_classification.cpp`s migrierte Klassifikationsfunktion
(nicht mehr byteidentisch zu `SafetyCore::classifyRunLoad()`, siehe unten)
erhält einen frühen, expliziten Zweig:

```text
if (status == RunPersistenceLoadStatus::FallbackRecovered)
    return RunLoadDisposition::SafeBoot;   // -> BootClassification::SafeBoot
    // unabhaengig vom Snapshot-Inhalt/Phase; kein isR1ResumeEligible()-Aufruf
```

`Current` bleibt unverändert (inklusive `isR1ResumeEligible()`-Prüfung für
`ResumeOffer`).

**Wichtige Konsequenz, real nachverfolgt (Blocker 3 fordert dies explizit):**
`TemperatureControlApplicationOrchestrator::reconcileR1LoadedRun()` ruft
intern `SafetyCore::classifyRunLoad(loaded.status, snapshot) !=
RunLoadDisposition::NoActiveRun` als Eligibility-Guard für den stillen
`discardAsNoActiveRun()`-Pfad. Da `SafetyCore` gemäß Abschnitt 10 entfällt
und `classifyRunLoad()` nach `boot_classification` umzieht, **muss diese
Aufrufstelle mechanisch auf den neuen qualifizierten Namen angepasst
werden** (`temperature_control_orchestrator.cpp` ist ohnehin bereits als
geänderte Datei geführt, Abschnitt 4.9 – dieselbe Datei, ein weiterer
mechanischer Edit, keine zusätzliche Datei). **Nach** dieser einen
Namensanpassung gilt die neue `FallbackRecovered`-Regel automatisch an
beiden Aufrufstellen (Boot-Composition und `reconcileR1LoadedRun()`), weil
beide dieselbe Funktion aufrufen: Für `FallbackRecovered` liefert sie jetzt
`SafeBoot` statt potenziell `NoActiveRun`, und `reconcileR1LoadedRun()` gibt
folgerichtig `NotEligible` zurück – **kein stiller Tombstone-Discard mehr
für `FallbackRecovered`**, exakt wie vom Owner gefordert („kein Tombstone-
Discard zum Verstecken des beschädigten Current-Zustands"). Es entsteht
**keine** inhaltliche Änderung an `reconcileR1LoadedRun()`s eigener Logik,
nur die eine Namensanpassung der bereits umgezogenen Funktion.

**Korrektur gegenüber der Vorfassung:** Jede Aussage „`classifyRunLoad()`/
`isR1ResumeEligible()` unverändert/byteidentisch übernommen" ist damit
präzisiert: `isR1ResumeEligible()` bleibt tatsächlich byteidentisch;
`classifyRunLoad()` bekommt den oben gezeigten einen zusätzlichen frühen
Zweig für `FallbackRecovered` und ist damit eine **R1-spezifische
Variante**, keine 1:1-Kopie.

### 7.4 Runtime-Lifecycle nach erfolgreichem Boot (Blocker 9, neu geschlossen)

Nach `READY` können reale Producerzustände zur Laufzeit `SERVICE_REQUIRED`
auslösen. Vollständige Tabelle (nur Producer, die in der actor-free
#121-Composition, Abschnitt 3/12, tatsächlich laufen):

| Trigger | Autoritativer Producer | Process-State-Effekt | Persistence Action | Actuation Gate | Presentation | Clear-/Recovery-Autorität |
|---|---|---|---|---|---|---|
| `ConfigurationServiceMode::CommitIndeterminate` | `ConfigurationService` | keiner (Application-Ebene) | keine | `DENIED` | `PresentationState` zeigt `ConfigurationCommitIndeterminate` | `ConfigurationService::resolveIndeterminate()` (bestehend) → Application liest frisches Ergebnis |
| `ConfigurationServiceMode::RuntimeFailure` | `ConfigurationService` | keiner | keine | `DENIED` | `PresentationState` zeigt `ConfigurationRuntimeFailure` | `ConfigurationService::recoverRuntimeFailure()` (bestehend) |
| `RunPersistenceCoordinatorState::BlockedIndeterminate` | `RunPersistenceCoordinator` | keiner (laufender Prozesszustand bleibt, falls vorhanden, unverändert publiziert) | keine neue Mutation möglich | `DENIED` (Interlock liest `persistenceCoordinatorState` frisch, Abschnitt 10) | `PresentationState` zeigt `RunPersistenceUntrusted` | neuer Boot/Load-Zyklus (kein Runtime-Clear, bestehender Vertrag) |
| `RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed` | `RunPersistenceCoordinator` | keiner | keine | `DENIED` | wie oben | wie oben |

`READY -> SERVICE_REQUIRED` ist damit für jeden real in dieser Composition
laufenden Producer definiert. `SERVICE_REQUIRED -> READY` existiert nur über
die jeweils genannte, bereits bestehende Producer-eigene Recovery-Methode –
kein neuer genereller Fault-Lifecycle, keine neue Clear-Autorität.

Sensor-/Watchdog-Fehler sind in dieser actor-free Composition **nicht
erreichbar** (kein Sensor-/Planner-Producer verdrahtet, Abschnitt 3) – die
Regel aus Korrektur E (nur konkrete Aktorfreigabe sperren, nicht pauschal
`SERVICE_REQUIRED`) bleibt als **Policy-Vorgabe für die spätere Verdrahtung**
dokumentiert (Abschnitt 10), ohne in dieser Revision Code zu erzeugen.

## 8. Fresh Start

```text
Standby (Application READY, Actuation DENIED)
  // runtimeRunState_ bereits aus dem NoRun-Boot-Flow gesetzt (Abschnitt 7:
  // ein minimaler RunCommandState mit processState.state == Standby,
  // heapbesitzend, Abschnitt 9 Blocker 1)
  -> Nutzer Start-Kommando
  -> Application prueft CommandKind ∈ {StartProgram, StartManualHolding}
     (dieselbe Regel wie TemperatureControlApplicationOrchestrator::
     persistFreshStartCommand(), hier direkt in FermentationApplication
     angewendet statt ueber den – in dieser Revision nicht komponierten –
     Orchestrator, Abschnitt 4.9)
  -> decisionTarget = std::unique_ptr<CommandDecision>{
       new (std::nothrow) CommandDecision()}
     if (decisionTarget == nullptr) -> fail-closed, kein Start
     (Abschnitt 12.4.3, Blocker 4 Runde 4)
  -> decideProgramStartInto(..., *decisionTarget)  // oder
     decideManualStartInto(...) - stack-sicherer In-place-Kern
     (Abschnitt 12.4.3, Blocker 6 Runde 5), KEIN generisches "decision"
     bei !proposed() -> kein Start (bestehende Ablehnungslogik unveraendert)
  -> RunPersistenceCoordinator::persistCommand(*runtimeRunState_,
     *decisionTarget, time, liveSensorEvidence)  // bestehende API,
     #17 Write-before-Apply unveraendert; schliesst Major 7 (Runde 3) –
     der kanonische Runtime-State ist der Eingang, nicht ein generisches
     "current"
  -> RunPersistenceResultStatus::Applied  // writeSnapshot() mutiert
     *runtimeRunState_ selbst in-place (dieselbe write-before-apply-
     Konvention wie ueberall im Coordinator, Abschnitt 4.4/9.1); bei
     Fehlschlag bleibt *runtimeRunState_ UNVERAENDERT (weiterhin Standby),
     kein Start
  -> ActuationInterlock::evaluate(evidence) mit
     activationKind=FreshStart, explicitActivationRequested=true,
     processActivationApplied=true, activationPersistenceResult=Applied
  -> erst danach ALLOWED moeglich (physischer Consumer folgt spaeter,
     Abschnitt 3)
```

**Korrektur gegenüber der Vorfassung (Major 7, Runde 6):** „Kein neuer
Persistenzcode; nur die direkte Verdrahtung ist neu" war nach dem
inzwischen deutlich erweiterten Stack-Safety-Umbau (Abschnitt 12.4) nicht
mehr korrekt. Richtig: Keine neue Persistenz-**Semantik**, kein neues
Wireformat und keine neue Transaktionspolicy. Neu sind die direkte
Application-Verdrahtung (`RunPersistenceCoordinator::persistCommand` +
`ActuationInterlock::evaluate` mit den realen Post-Commit-Feldern) plus
rein mechanische, stack-sichere In-place-Helfer
(`decideProgramStartInto()`/`decideManualStartInto()`,
`makeRunPersistenceSnapshotInto()`, Abschnitt 12.4), die denselben
Schreibpfad ohne Semantikänderung stack-sicher machen.

## 9. Resume (Ownership-/Lifetime-Vertrag, Korrektur F, Blocker 1/2/3 geschlossen)

**Kanonischer `runtimeRunState_` (schließt Blocker 1, Runde 2; Speicherform
korrigiert, Blocker 1, Runde 3):**
`FermentationApplication` hält genau **einen** kanonischen
`std::unique_ptr<RunCommandState> runtimeRunState_`-Member für den aktuell
aktiven, publizierten Lauf (getrennt von `pendingResume_`, das nur die
unbestätigte Resume-Vorschau hält, Abschnitt 12.1). **Korrektur gegenüber
der Vorfassung (real gemessener Bug, Runde 3):** Ein `std::optional<T>`
speichert `T` **inline**, nicht auf dem Heap. `RunCommandState` ist real
gemessen `sizeof(RunCommandState) == 5096` Byte (Host-Build; reale
ESP32/xtensa-Größen können leicht abweichen, die Größenordnung bleibt
gleich) – zwei solcher Felder als `std::optional`-Wertmember hätten
`sizeof(FermentationApplication)` um über 10 KB vergrößert, bei
`CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584` (PR #118) exakt dieselbe
Stack-Fehlerklasse wie bei #120 reproduziert, für die `application` als
Stackobjekt in `main/app_main.cpp` (`fermentation::FermentationApplication
application;`) konstruiert wird. Beide Felder sind daher `unique_ptr`, nicht
`optional`:

```cpp
std::unique_ptr<RunCommandState> runtimeRunState_;
std::unique_ptr<RunCommandState> pendingResume_;
```

`publishedProcessState()` wird eine **reine Projektion**:

```cpp
std::optional<ProcessRuntimeState> publishedProcessState() const {
    return runtimeRunState_ != nullptr
               ? std::optional{runtimeRunState_->processState}
               : std::nullopt;
}
```

**Allokationsfehlerfall (verbindlich, Runde 3; Diagnoseattribution
korrigiert, Blocker 7/Major 8 Runde 4/5):** Schlägt die Heap-Allokation für
`runtimeRunState_` oder `pendingResume_` fehl (`new (std::nothrow)`), gilt
fail-closed: kein publizierter aktiver Lauf (`publishedProcessState() ==
std::nullopt`), kein Resume-Angebot, `Actuation DENIED`, **kein** Store-
Schreibvorgang. **Korrektur gegenüber der Vorfassung dieses Abschnitts
(real übersehener Widerspruch zu Abschnitt 11, Blocker 7, Runde 5):** dieser
Absatz mappte den Fehler weiterhin auf denselben `FaultCode::
RunPersistenceUntrusted`-Diagnosepfad wie ein technischer Ladefehler – das
widerspricht Major 8 (Abschnitt 11), wonach ein Application-/
Allokationsfehler die Persistence-Vertrauenswürdigkeit nicht falsifiziert
(„Producer bleiben Autorität ihrer eigenen Wahrheit"; ein Heap-Fehler bei
der Application-seitigen Objektkonstruktion sagt nichts über Store oder
Persistence aus). Kanonisch, konsistent mit Abschnitt 11:

```text
PresentationState.applicationAllocationFailure = true
FaultCode::None   // kein Safety-FaultCode, kein RunPersistenceUntrusted
Persistence-Trust unveraendert (RunPersistenceCoordinator::state()
  bleibt unberuehrt, dieser Fehler entsteht in FermentationApplication,
  nicht im Coordinator)
```

```text
RUNTIME_RUN_STATE_STORAGE=HEAP_OWNED
PENDING_RESUME_STORAGE=HEAP_OWNED
RUNTIME_RUN_STATE_ALLOCATION_FAILURE=FAIL_CLOSED
PENDING_RESUME_ALLOCATION_FAILURE=FAIL_CLOSED
FERMENTATION_APPLICATION_INLINE_LARGE_RUN_STATE=NO
```

Jeder Flow aus Abschnitt 7, der etwas published, schreibt zuerst
`runtimeRunState_` (bei `NoRun` einen minimalen `RunCommandState` mit
`processState.state == Standby`; bei `ResumeRejected`/`DiscardableRun` den
via `restoreRunPersistenceSnapshotInto()` rekonstruierten und durch
`discardAsNoActiveRun()` auf `Standby` mutierten `RunCommandState` –
**kein** minimaler Stub, Major 6, Abschnitt 9; sonst den von
`RunPersistenceCoordinator`/`activateR1EligibleRun()` gelieferten
Kandidaten) und liest `publishedProcessState()` nie ad hoc aus einem anderen
Objekt. Es gibt damit genau eine Schreibstelle für „was ist aktuell
published" – kein zweiter Publikationsmechanismus (Korrektur I bleibt damit
strikt erfüllt: vor dem ersten Schreiben von `runtimeRunState_` bleibt
`publishedProcessState()` `std::nullopt`, niemals ein
default-konstruiertes `ProcessRuntimeState{Boot}`).

```text
RunPersistenceCoordinator::loadAndInitializeInto(runPersistenceLoadResult&)
  -> RunPersistenceLoadResult{status, snapshot}
  -> boot_classification::classify(status, snapshot)  // Abschnitt 7.3:
     FallbackRecovered -> SafeBoot, sonst wie isR1ResumeEligible()
  -> ResumeOffer (nur fuer status==Current, Phase in
     {Preheating, Cooling, ManualHolding})
     -> pendingResume_ = std::unique_ptr<RunCommandState>{
          new (std::nothrow) RunCommandState()}
        // Stack-sicherer Produktpfad-Vertrag (Abschnitt 12.4.1/12.4.2,
        // Blocker 2.1/4, Runde 4): nothrow-Allokation ZUERST, dann
        // In-place-Restore direkt in den bereits allozierten Heap-
        // Speicher - KEIN restoreRunPersistenceSnapshot()-Rueckgabewert
        // (5096 B lokale Kopie, Standard garantiert keine NRVO), KEIN
        // std::make_unique (werfend, kein nothrow-Vertrag)
        if (pendingResume_ == nullptr) -> Allokationsfehler, fail-closed
          (kein Resume, kein publizierter Lauf, SERVICE_REQUIRED)
     -> if (!restoreRunPersistenceSnapshotInto(*loaded.snapshot,
          *pendingResume_)) -> SafeBoot/SERVICE_REQUIRED (technischer
          Konsistenzfehler, kein Resume moeglich), pendingResume_ geleert
        // NEU: Out-Parameter-Variante (Abschnitt 12.4.2), schreibt direkt
        // in *pendingResume_ - keine zusaetzliche lokale RunCommandState-
        // Kopie irgendwo im Aufrufpfad
     -> PENDING_RESUME_OWNER=FermentationApplication
        PENDING_RESUME_TYPE=std::unique_ptr<RunCommandState>
        PENDING_RESUME_LIFETIME=von Klassifikation bis Confirm/Reject/
          begin()-Fehlschlag (RAII-Member von FermentationApplication,
          kein dangling reference ueber begin()-Rueckkehr hinweg, da
          FermentationApplication selbst als Application-Lifetime-Objekt
          lebt, Abschnitt 12.1)
        PENDING_RESUME_CLEAR_ON=confirm (nach Applied verschoben in den
          aktiven RunCommandState), reject (verworfen), begin()-
          Allokationsfehlschlag (verworfen, fail-closed)
     -> Snapshot selbst bleibt zusaetzlich technisch im
        RunPersistenceCoordinator (state()==LoadedActiveRun); zwei
        Darstellungen desselben Laufs (Rohsnapshot beim Coordinator,
        rekonstruierter RunCommandState bei der Application) sind kein
        Policy-Konflikt, da nur die Application-Kopie fuer die
        Nutzerentscheidung gelesen/mutiert wird und der Coordinator seine
        technische Wahrheit (COORDINATOR_TECHNICAL_AUTHORITY=PRESERVED)
        unveraendert behaelt, bis activateR1EligibleRun()/
        discardAsNoActiveRun() sie fortschreibt.
        SECOND_POLICY_AUTHORITY=NO (Application trifft keine eigene
          Recovery-/Trust-Entscheidung, sie haelt nur die schreibgeschuetzte
          Vorschau bis zur Nutzerbestaetigung)
     -> Application published KEINEN ProcessRuntimeState (Abschnitt 7.2)
     -> Actuation DENIED
     -> kein automatisches FSM-Advancement

Bei explizitem Resume-Confirm durch Nutzer:
  -> Application prueft aktuelle Config-/Sensor-/Safety-Evidenz frisch
     (dieselben hasFreshConfigurationEvidence()/hasFreshSensorEvidence()-
     Helfer, unveraendert aus safety_core.cpp uebernommen)
  -> RunPersistenceCoordinator::activateR1EligibleRun(*pendingResume_,
     time, &liveSensorEvidence)  // NEU, kleine R1-spezifische Methode (siehe
     unten), KEIN activateLoadedRun(). Dritter Parameter jetzt Pointer
     (Blocker 3, Runde 3) – fuer den aktiven Resume-Zweig immer non-null.
     Sensor-Revalidierung (Blocker 5) geschieht INTERN im Coordinator
     (Abschnitt 9.1, Zweig 3), nicht in der Application – Begruendung: die
     dafuer noetige recoverySensorSelectionProgramContext()-Hilfsfunktion
     ist TU-privat in run_persistence_coordinator.cpp (anonymous
     namespace, real geprueft), also fuer FermentationApplication ohnehin
     nicht sichtbar; eine Duplikation dieser ~15-zeiligen Logik in der
     Application waere ein DRY-Verstoss. `*pendingResume_` wird als
     `RunCommandState&` uebergeben (Dereferenzierung des unique_ptr, KEINE
     Kopie) – bei Erfolg mutiert der Coordinator dieses Objekt selbst
     IN-PLACE (write-before-apply, Abschnitt 9.1); es entsteht kein
     separates "candidate", das die Application eigens uebernehmen muesste.
  -> RunPersistenceResult.status == Applied
  -> danach runtimeRunState_ = std::move(pendingResume_)  // verschiebt den
     unique_ptr selbst (O(1), KEIN 5096-Byte-Kopiervorgang) – *pendingResume_
     wurde vom Coordinator-Aufruf bereits in-place auf den rebasten Zustand
     aktualisiert (s. o.); ProcessRuntimeState damit erstmals als reine
     Projektion published (Abschnitt 9, Blocker 1; direkt aus
     runtimeRunState_->processState, ohne propose()/
     applyProcessTransition(), Abschnitt 4.2/7.2 – Praezedenzfall aus
     activateLoadedRun()s Completed-Zweig)
  -> pendingResume_ ist danach nullptr (Inhalt bereits nach runtimeRunState_
     verschoben, kein separates Leeren noetig)
  -> **Fehlerklassifikation bei `status != Applied`** (schliesst Blocker 2,
     Abschnitt 9.1 traegt die vollstaendige Logik): NUR wenn
     `persisted.coordinatorState == LoadedActiveRun` UND
     `persisted.durability == Unchanged`, bleibt `pendingResume_`
     unveraendert bestehen und ein erneuter Confirm ist zulaessig (sauber
     zurueckgerollter, retry-sicherer Fehler). In JEDEM anderen Fall
     (`BlockedIndeterminate`, `PersistenceCommittedApplyFailed`,
     `durability != Unchanged`) gilt: `pendingResume_` gilt NICHT mehr als
     aktuelle Store-Wahrheit (wird aber nicht geleert – Diagnosezweck),
     Application -> `SERVICE_REQUIRED`, `Actuation DENIED`, ein erneuter
     Resume-Confirm-Versuch ist `NOT_ALLOWED`, kein Tombstone/Repair-Versuch
     zur Verschleierung der Ambiguitaet.
  -> ActuationInterlock::evaluate() mit activationKind=Resume,
     processActivationApplied=true, activationPersistenceResult=Applied
  -> erst danach ALLOWED moeglich

  Bei RunPersistenceResult.status == NotEligible (Sensor-Revalidierung
  blockiert, Abschnitt 9.1 Zweig 3): pendingResume_ bleibt UNVERAENDERT
  bestehen (kein current-Mutation, kein Store-Zugriff,
  R1_RESUME_SENSOR_REVALIDATION_FAILURE_MUTATES_STORE=NO), Application
  bleibt im ResumeOffer-Zustand mit zusaetzlichem Sensor-blockiert-Hinweis
  (Presentation); Nutzer kann erneut bestaetigen (nach Sensorwechsel) oder
  ablehnen (Reject-Pfad unten). **Bewusste Abweichung vom C2-Praezedenzfall**
  (der bei blockierter Empfehlung ueber `RecoveryReject` in
  `ProcessState::RecoveryEvaluation` geht): R1 loest hier KEINE
  `propose(..., ProcessState::RecoveryEvaluation, RecoveryReject, ...)`-
  Transition aus (das wuerde `R1_RESUME_RECOVERY_EVALUATION_CREATED=NO`
  verletzen, Abschnitt 9.1) – die Blockade bleibt reine
  Read-Only-Konsequenz.

Bei Ablehnung (ResumeOffer, Nutzer lehnt ab):
  -> Application ruft RunPersistenceCoordinator::discardAsNoActiveRun(
     *pendingResume_, time) direkt (nicht ueber
     TemperatureControlApplicationOrchestrator::reconcileR1LoadedRun(),
     Abschnitt 4.9/8 – Aufruf ist identisch zur internen Wrapper-Logik, nur
     ohne die dort zusaetzliche automatische PI-/Planner-Reset-Seite, die
     hier wirkungslos waere). `*pendingResume_` (bereits durch
     `restoreRunPersistenceSnapshotInto()` rekonstruiert, s.o.) ist der
     benoetigte `RunCommandState&`-Eingang.
  -> RunPersistenceCoordinator::discardAsNoActiveRun() committet den
     NoActiveRun-Tombstone zuerst durabel (write-before-apply, bestehend)
  -> nur bei Applied: runtimeRunState_ = std::move(pendingResume_) (der nun
     durch discardAsNoActiveRun() in-place auf Standby mutierte Zustand),
     danach Standby published
  -> bei Fehlschlag: pendingResume_ bleibt bestehen (Diagnosezweck), kein
     Standby-Publish, Klassifikation nach `coordinatorState`/`durability`
     wie oben (retry-sicher nur bei `LoadedActiveRun`+`Unchanged`, sonst
     `SERVICE_REQUIRED`)

Bei `DiscardableRun` (bereits beim Boot als vertrauenswuerdig, aber nicht
resumefaehig klassifiziert – schliesst Major 6, Runde 3; **anders als der
ResumeOffer/Ablehnung-Pfad existiert hier noch kein `pendingResume_`**, da
diese Klassifikation nie durch `ResumeOffer` lief):
  -> discardTarget = std::unique_ptr<RunCommandState>{
       new (std::nothrow) RunCommandState()}
     // dieselbe stack-sichere Reihenfolge wie beim ResumeOffer-Zweig
     // (Abschnitt 12.4.1/12.4.2, Blocker 2.1/4, Runde 4): Allokation
     // zuerst, dann In-place-Restore
     if (discardTarget == nullptr) -> Allokationsfehler, fail-closed
       (SERVICE_REQUIRED, kein Standby-Publish)
  -> if (!restoreRunPersistenceSnapshotInto(*loaded.snapshot,
       *discardTarget)) -> SafeBoot/SERVICE_REQUIRED (technischer
       Konsistenzfehler), discardTarget wird verworfen
     // dieselbe Out-Parameter-Variante wie im ResumeOffer-Zweig, hier
     // direkt waehrend der Boot-Klassifikation aufgerufen statt erst bei
     // Nutzerinteraktion
  -> RunPersistenceCoordinator::discardAsNoActiveRun(*discardTarget, time)
     direkt (Application ruft Coordinator direkt, nicht ueber den
     Orchestrator, Abschnitt 3)
  -> nur bei Applied: runtimeRunState_ = std::move(discardTarget) – der
     lokale `unique_ptr` wird in den Application-Lifetime-Member verschoben
     (danach IST er dieses Objekt, kein separates Lifetime-Konzept), danach
     Standby published
  -> bei Fehlschlag: kein Standby-Publish, `runtimeRunState_` bleibt
     nullptr, Klassifikation nach `coordinatorState`/`durability` wie oben;
     `discardTarget` selbst wird beim Verlassen des `begin()`-Scopes
     freigegeben (RAII), da es nie in `runtimeRunState_` verschoben wurde

Bei FallbackRecovered (Abschnitt 7.3):
  -> BootClassification::SafeBoot, kein pendingResume_, kein Tombstone
  -> Application SERVICE_REQUIRED -> Actuation DENIED

Bei technisch untrusted Persistenz (sonstige SafeBoot-Faelle):
  -> kein Tombstone -> Application SERVICE_REQUIRED -> Actuation DENIED
```

### 9.1 `RunPersistenceCoordinator::activateR1EligibleRun()` – neue kleine R1-Methode (Blocker 2/3/4 geschlossen, Major 8 geschlossen)

Zulässig gemäß Korrektur B (kleine neue API, kein Refactor von
`RunRecoveryCoordinator`). **Name deckt drei Fälle ab** (Runde 2,
Klarstellung): trotz des Namens „eligible" behandelt die Methode alle drei
möglichen `LoadedActiveRun`-Ausgänge – `Fault`, `Completed` und die drei
R1-resumefähigen aktiven Phasen –, nicht nur den engeren Resume-Fall. Sie ist
damit der vollständige R1-Ersatz für `activateLoadedRun()`, nicht nur ein
Ausschnitt davon.

```text
RunPersistenceResult RunPersistenceCoordinator::activateR1EligibleRun(
    RunCommandState& current, const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence = nullptr)
```

Dritter Parameter `liveSensorEvidence` neu gegenüber der Vorfassung (schließt
Blocker 5 vollständig; identische Signatur-Erweiterung wie
`activateLoadedRun()` selbst sie trägt). **Korrektur gegenüber der
Vorfassung (Blocker 3, Runde 3):** als Pointer mit Default `nullptr`, nicht
als zwingende Referenz – `Fault`/`Completed` (Zweig 1/2) benötigen und lesen
`liveSensorEvidence` nie (#121 bleibt actor-free, für die reine
Historien-Restaurierung eines terminalen Laufs gibt es keine
Sensor-Fragestellung); nur Zweig 3 (aktiver Resume) benötigt echte Evidenz:

```text
Fault (Zweig 1): liveSensorEvidence darf nullptr sein
Completed (Zweig 2): liveSensorEvidence darf nullptr sein
Aktiver Resume (Zweig 3): liveSensorEvidence MUSS non-null sein;
  nullptr -> NotEligible, keine Mutation, kein Schreibvorgang
```

**Vorbedingung:** `state_ == LoadedActiveRun`, sonst `unavailableResult()`,
keine Mutation.

**Zweig 1 – `current.processState.state == Fault`** (exakter
`activateLoadedRun()`-Fault-Präzedenzfall, Abschnitt 4.4, unverändert
übernommen):

```text
state_ = Ready
current bleibt UNVERAENDERT (keine Feldmutation)
return Applied / RamApply / None / Unchanged
```

**Hinweis zu Zweig 1/2 und Blocker 4:** Beide Zweige sind RAM-only – es gibt
keinen Store-Schreibvorgang, an dem eine „nur nach `Applied` mutieren"-Regel
greifen könnte (kein Fehlschlagpfad existiert, solange die Vorbedingung
`state_ == LoadedActiveRun` erfüllt ist). Die unbedingte Mutation
widerspricht Blocker 4 daher nicht – Blocker 4 gilt für den durablen
Schreibpfad in Zweig 3.

**Zweig 2 – `current.processState.state == Completed`** (exakter
`activateLoadedRun()`-Completed-Präzedenzfall, unverändert übernommen):

```text
current.processState.stateEnteredAtMillis = time.monotonicMillis
  // KEIN transitionSequence-Inkrement (Praezedenzfall inkrementiert es
  // nicht) -> kein Overflow-Guard noetig (schliesst Blocker 3: der Guard
  // eruebrigt sich, weil der Wert gar nicht mutiert wird, statt ihn um
  // eine Pruefung zu ergaenzen)
state_ = Ready
return Applied / RamApply / None / Unchanged
```

**Zweig 3 – `current.processState.state ∈ R1_RESUME_ELIGIBLE_PHASES`**
(`{Preheating, Cooling, ManualHolding}`, identisch zu `isR1ResumeEligible()`,
Abschnitt 7.3, keine neue Liste) – **Write-before-Apply in-place**, exakt
nach dem real geprüften Muster von `discardAsNoActiveRun()`/
`persistTransition()` (Abschnitt 4.4/9):

```text
1. workingSet_.candidate = current  // Coordinator-eigenes Wertmember
   (Abschnitt 12.4.4, Runde 5 - ersetzt die vormalige lokale Kopie), current
   bleibt bis Schritt 6 unveraendert
2. workingSet_.candidate.processState = rebasedRecoveredState(
   current.processState, time.monotonicMillis)
   // schliesst Blocker 2: Wiederverwendung der bereits bestehenden,
   // bereits fuer den C2-Pfad genutzten Funktion
   // (run_persistence_coordinator.cpp:177-189), statt nur
   // stateEnteredAtMillis einzeln zu setzen. Reale Wirkung:
   //   recovered.stateEnteredAtMillis = monotonicMillis
   //   recovered.qualificationValidSinceMillis.reset()
   //   recovered.targetReachWarningIssued = false
   //   recovered.targetReachStartedAtMillis =
   //     (state==ReachingTarget ? monotonicMillis : 0)
   //   QualifyingTarget -> ReachingTarget-Remap (fuer R1 nie ausgeloest,
   //   da QualifyingTarget nicht in R1_RESUME_ELIGIBLE_PHASES ist)
   // KEIN transitionSequence-Inkrement (dieselbe Begruendung wie Zweig 2:
   // keine Topologietransition, kein propose()/applyProcessTransition(),
   // daher kein Overflow-Guard noetig, Blocker 3 geschlossen). Real
   // zusaetzlich abgesichert (Runde 2, gegen die vom Owner befuerchtete
   // stille Ablehnung geprueft): run_persistence_codec.cpp:1374-1375 und
   // :1512-1513 validieren beim Kopf-Encode/Decode ausschliesslich
   // `newTransitionSequence >= oldTransitionSequence` (nicht `>`) - ein
   // Null-Delta ist also ein gueltiger, encodierbarer Kopf. Die vier
   // Delta-Felder (old/newRunRevision, old/newTransitionSequence) werden
   // im gesamten `run_persistence_coordinator.cpp` ausschliesslich an der
   // einen Schreibstelle (Zeile 1606-1609) gesetzt und an KEINER Stelle
   // fuer eine Rollforward-/Rollback-Entscheidung wieder gelesen (real
   // grep-geprueft) - reine Provenienz-Bookkeeping-Felder, keine
   // Replay-Steuerung.
2a. Sensor-Revalidierung (schliesst Blocker 5, real gegen den bestehenden
   C2-Aufrufort gespiegelt, run_persistence_coordinator.cpp:682-699;
   Reihenfolge bewusst identisch zum Praezedenzfall - der Guard steht VOR
   dem Aufruf, der die Optionals dereferenziert):
     if (liveSensorEvidence == nullptr)
       return result(NotEligible, CandidateApply, None)
       // schliesst Blocker 3: Zweig 3 verlangt echte Evidenz, current
       // unveraendert, kein Schreibvorgang
     if (!workingSet_.candidate.sensorSelection.has_value() ||
         !workingSet_.candidate.activeRunSensorMode.has_value())
       return result(InvalidDecision, CandidateApply, InvalidProjection)
       // current unveraendert; real geprueft: fuer die drei
       // R1-eligiblen Phasen sind beide Felder immer gesetzt (Vertrag von
       // restoreRunPersistenceSnapshotInto()), dieser Zweig ist reine
       // Konsistenzsicherung wie im C2-Praezedenzfall (Zeile 683-686)
     recommendation = computeRestartSensorSelection(
       *workingSet_.candidate.sensorSelection,
       *workingSet_.candidate.activeRunSensorMode,
       recoverySensorSelectionProgramContext(workingSet_.candidate),
         // bereits bestehende TU-private Hilfsfunktion (anonymous
         // namespace, Zeile 78), unveraendert wiederverwendet - deckt
         // sowohl ProgramRun (liest activeProgramRun->snapshot().
         // sourceProgram.program) als auch ManualRun (Default-Kontext) ab,
         // exakt wie im bestehenden C2-Aufruf
       *liveSensorEvidence)
     if (recommendation.runtime.permission != SensorPeltierPermission::Allowed)
       return result(NotEligible, CandidateApply, None)
       // EXAKT dieselbe Gate-Bedingung wie im C2-Praezedenzfall (Zeile
       // 696: `recommendation.runtime.permission != Allowed`), NICHT
       // `.phase == RestartRevalidationPending` (das ist der Phase-Wert,
       // der bei Blockade zusaetzlich gesetzt wird, aber nicht das
       // tatsaechliche Gate-Feld). current UNVERAENDERT - keine Mutation,
       // kein Schreibvorgang (Abschnitt 9: Application bleibt im
       // ResumeOffer-Zustand)
     workingSet_.candidate.sensorSelectionRuntime = recommendation.runtime
     workingSet_.candidate.activeRunSensorMode = recommendation.activeMode
     if (workingSet_.candidate.activeManualRun.has_value())
       workingSet_.candidate.activeManualRun->values.sensorMode =
         recommendation.activeMode
       // exakt wie im C2-Praezedenzfall, Zeile 690-695
3. if (!makeRunPersistenceSnapshotInto(workingSet_.candidate, persistedIds_,
     persistedIdCount_, RunCheckpointTrigger::Transition, time,
     schedule_.intervalMinutes(), workingSet_.snapshot))
     -> InvalidDecision, current unveraendert
   // Stack-sicherer In-place-Kern (Abschnitt 12.4.1, Runde 5), ersetzt die
   // vormalige Return-by-value-Verwendung; schreibt direkt in
   // workingSet_.snapshot statt einen lokalen optional<RunPersistenceSnapshot>
   // (4096 B, real gemessen) zurueckzugeben
4. rollbackState = state_  // == LoadedActiveRun, VOR dem Aufruf gesichert
   persisted = writeSnapshotCore(workingSet_.snapshot, time,
     /*periodic=*/false, current, RunPersistenceMutationKind::Recovery,
     /*commandId=*/nullopt, /*targetSlotOverride=*/nullopt,
     RunPersistenceFallbackDirective{},  // Default = UseStandardFallback
       // KORREKTUR gegenueber Vorfassung (real gefundener Bug in der
       // Vorfassung selbst, Runde 2): ClearFallback wuerde
       // writeSnapshotCore()s clearFallbackAllowed-Bedingung
       // (nur Fault- oder NoActiveRun-Snapshots) NIE erfuellen, da
       // workingSet_.candidate.processState.state hier Preheating/Cooling/
       // ManualHolding ist -> directiveValid waere false ->
       // InvalidDecision fuer JEDEN Resume-Confirm-Versuch. Korrekt ist
       // der Default UseStandardFallback (kein reference), derselbe, den
       // persistTransition() ueber writeSnapshot() implizit verwendet.
     rollbackState)
   // writeSnapshotCore() selbst konstruiert intern KEINEN lokalen
   // RunPersistenceRawRecord mehr, sondern schreibt in workingSet_.record
   // (Abschnitt 12.4.4, Blocker 1 Runde 5) - fuer diesen Aufrufer
   // transparent, kein Signaturwechsel.
   // writeSnapshotCore() setzt bei einem Fehlschlag state_ = rollbackState
   // (== LoadedActiveRun) selbst zurueck (real geprüft) -> Application
   // bleibt im ResumeOffer-Zustand, kein zusaetzlicher Rollback-Code noetig.
5. if (persisted.status != Applied) return persisted;  // current
   UNVERAENDERT (Blocker 4: current wird ausschliesslich nach Applied
   mutiert) – vollstaendige Fehlerklassifikation fuer den Aufrufer siehe
   unten (Blocker 2)
6. current = workingSet_.candidate  // current erst JETZT mutiert
   // state_ wurde von writeSnapshotCore() bereits intern auf Ready gesetzt
   // (real geprüft, snapshot.variant != NoActiveRun -> Ready, nicht
   // ReadyEmpty)
   return persisted  // Applied, Durability::Changed
```

**Fehlerklassifikation bei `persisted.status != Applied` in Zweig 3
(schließt Blocker 2, Runde 3 – real vollständig gegen `writeSnapshotCore()`s
Fehlerpfade nachverfolgt, `run_persistence_coordinator.cpp:1477-1766`):**
`writeSnapshotCore()` durchläuft für den nicht-periodischen Schreibpfad
(`periodic=false`, wie hier verwendet) drei sequentielle Store-Operationen
(`PreparedHead`-Schreiben, `CheckpointSlot`-Schreiben, `CommittedHead`-
Schreiben) plus vorausgehende reine Encode-/Read-Schritte. Jeder `result(...)`-
Aufruf setzt `value.coordinatorState = state_` **zum Zeitpunkt der
Result-Konstruktion**, also **nach** einer eventuellen internen
`enterBlockedIndeterminate()`/Rollback-Mutation (`result()`,
`run_persistence_coordinator.cpp:241-252`, real geprüft) – der Aufrufer kann
sich daher vollständig auf `persisted.coordinatorState` verlassen, ohne den
internen Kontrollfluss nachzubilden:

```text
Retry-sicher (Application bleibt ResumeOffer, erneuter Confirm erlaubt):
  persisted.coordinatorState == LoadedActiveRun
  AND persisted.durability == Unchanged
  // real geprueft: dieser Fall tritt NUR ein bei (a) einem reinen
  // Encode-/Read-Fehler VOR jedem Store-Schreibversuch (Envelope-/
  // Kopf-Encode-CapacityExceeded, physicalTarget-Lesefehler faellt NICHT
  // hierunter - der ruft immer enterBlockedIndeterminate() auf) oder
  // (b) einem PreparedHead-Schreibfehler, der NICHT Indeterminate war
  // (readyState() -> state_ = rollbackState == LoadedActiveRun)

SERVICE_REQUIRED (kein Retry, kein Tombstone/Repair, pendingResume_ gilt
nicht mehr als aktuelle Store-Wahrheit):
  jeder andere Fall, insbesondere:
  - persisted.coordinatorState == BlockedIndeterminate
    (jeder CheckpointSlot-/CommittedHead-Schreibfehler nach erfolgreichem
    PreparedHead ruft IMMER enterBlockedIndeterminate() auf, unabhaengig
    vom Indeterminate-/Failed-Status der einzelnen Store-Operation - reale
    Zeilen 1727/1740/1743 real geprueft; ebenso jeder physicalTarget-
    Lesefehler und jeder PreparedHead-Schreibfehler MIT Indeterminate-
    Ergebnis)
  - persisted.coordinatorState == PersistenceCommittedApplyFailed
    (in Zweig 3 nicht erreichbar, da nach erfolgreichem Applied keine
    weitere RAM-Apply-Mutation stattfindet - candidate ist bereits vor dem
    Schreibversuch vollstaendig konstruiert; als Catch-all trotzdem in der
    Regel oben enthalten, falls ein spaeterer Umbau dies aendert)
  - persisted.durability != Unchanged (MayHaveChanged oder Changed)
```

```text
R1_RESUME_CLEAN_WRITE_FAILURE_CAN_RETRY_ONLY_IF_STORE_UNCHANGED=PASS
R1_RESUME_INDETERMINATE_WRITE_ENTERS_SERVICE_REQUIRED=PASS
R1_RESUME_INDETERMINATE_WRITE_CANNOT_RETRY_CONFIRM=PASS
R1_RESUME_FAILED_WRITE_NEVER_ACTUATES=PASS
```

**Fehlerfall (Vorbedingung/Konsistenzsicherung):** `state_ != LoadedActiveRun`
oder `current.processState.state` liegt in keinem der drei Zweige →
`RunPersistenceResultStatus::NotEligible`, keine Mutation. Da
`isR1ResumeEligible()` bereits beim Klassifizieren nur die drei
Resume-Phasen als `ResumeOffer` zulässt und `TerminalRunFault`/`CompletedRun`
(Abschnitt 7) die einzigen weiteren Aufrufer von `activateR1EligibleRun()`
sind, ist dieser Fehlerfall in der Praxis nur eine Konsistenzsicherung, kein
neues Gate.

```text
R1_RESUME_C2_FIELDS_CREATED = NO
  (kein PendingRecoveryAnchor, kein recoveryBootAnchorMonotonicMillis,
  kein lastRecoveryEpisodeEvidence, keine recoveryEpisodeRevision-Aenderung,
  keine foldObservedRunSeconds()/Zeitverdikt-Berechnung)
R1_RESUME_RECOVERY_EVALUATION_CREATED = NO
  (kein propose(..., ProcessState::RecoveryEvaluation, ...)-Aufruf)
```

**Explizit benannte Beobachtungskonsequenz je Phase (korrigiert, Major 11 –
die Vorfassung behauptete faelschlich eine „Verlaengerung um exakt die
Offline-Dauer"; real gilt das Gegenteil):**

- **`ManualHolding`:** `holdDurationMinutes` wird ueber `elapsed(now,
  stateEnteredAtMillis, holdDurationMinutes)` ausgewertet. Da
  `stateEnteredAtMillis` beim Resume auf den Resume-Zeitpunkt zurueckgesetzt
  wird, **verkuerzt** sich die noch verbleibende Haltezeit NICHT durch die
  Offline-Dauer – R1 rechnet die vor dem Neustart bereits verstrichene
  Haltezeit **nicht an** und startet die Zeitmessung bei Null.
  **Konkretes Beispiel:** `holdDurationMinutes = 60`; der Lauf betritt
  `ManualHolding` um `t=0`; nach 50 Minuten reeller Haltezeit (`t=50min`)
  erfolgt ein Absturz/Reboot; die Offline-Dauer betraegt 10 Minuten; beim
  Resume-Confirm um `t=60min` (Wanduhr) wird `stateEnteredAtMillis` auf den
  Resume-Zeitpunkt gesetzt – die verbleibende Haltezeit betraegt danach
  wieder volle 60 Minuten, nicht die real verbleibenden 10 Minuten. Die
  Gesamthaltezeit dieses Laufs betraegt damit real 50+60=110 Minuten statt
  der urspruenglich konfigurierten 60 Minuten. Das ist eine bewusste
  Vereinfachung, direkt gedeckt durch den #24-R1-Vertrag („keine gewichtete
  Recoveryzeit-/Progressrettung"): R1 verzichtet ausdruecklich auf praezise
  Vor-Boot-Zeitanrechnung, in beide Richtungen (weder Gutschrift noch
  Abzug der Offline-Dauer).
- **`Preheating`/`Cooling`:** kein `stateEnteredAtMillis`-gebundenes
  Zeitlimit in diesen Phasen (temperaturgetrieben, kein
  `stateHasTargetReachTimer()`-Zustand) – das Zuruecksetzen hat **keinen**
  beobachtbaren Zeiteffekt.

## 10. Interlock (`ActuationInterlock`, minimaler Vertrag)

Umbenennung von `SafetyCore` unverändert wie in der Vorfassung begründet
(Abschnitt 10, Korrektur G). Dateien: `safety_core.hpp/.cpp` →
`actuation_interlock.hpp/.cpp`; `test/test_safety_core/` →
`test/test_actuation_interlock/`.

**Betroffene Referenzstellen (real gezählt, unverändert gegenüber
Vorfassung plus dem in Abschnitt 4.9 spezifizierten
`tickActuatorPlan()`-Signaturwechsel):** `fermentation_application.hpp/.cpp`,
`temperature_control_orchestrator.hpp/.cpp` (zusätzlich: `SafetyCore&`-
Member entfällt, `tickActuatorPlan()`-Parameter neu, Abschnitt 4.9),
`main/issue_29_bringup_probe.cpp`,
`test/test_run_persistence_coordinator/…cpp`,
`test/test_issue90_product_recovery_oracle/…cpp`,
`test/esp_idf_nvs_adapter_host/main/test_nvs_state_store.cpp`.

**Minimaler Input** (`ActuationEvidence`, Nachfolger von `SafetyCoreInput`):

```text
bootValidationComplete, configurationValidated, persistenceValidated,
sensorEvidenceValidated, explicitActivationRequested,
plannerEvidenceValidated, activationKind,
configurationRecoveryStatus, configurationProducer,
configurationServiceMode, configurationCommitStatus,
persistenceLoadStatus, persistenceCoordinatorState,  // UNVERAENDERT aus
  // SafetyCoreInput uebernommen: laufend gelesene, aktuelle
  // Produzentenwahrheit des RunPersistenceCoordinator, kein zusaetzliches
  // Gedaechtnis des Interlocks (Korrektur G).
loadDisposition (aus boot_classification; ersetzt NUR persistenceSnapshot),
activationPersistenceResult, processActivationApplied,
peltierSensor, sensorSelectionRuntime,
actuatorPlanner (fuer den bestehenden #23-Watchdog-Lesezugriff; in dieser
  Revision optional/nullptr in der actor-free Composition, Abschnitt 3)
```

`persistenceLoadStatus` und `persistenceCoordinatorState` bleiben **beide**
im Interlock-Input (Begründung unverändert gegenüber Vorfassung: siehe
Abschnitt 4.4, `isTrustedCoordinatorState()`/`canClearFault()`-Konsumenten).
Nur `persistenceSnapshot` (Klassifikationsinhalt) verlässt den Interlock.

**Minimaler Output** (`ActuationEvaluation`):

```text
permission: ActuatorSafetyGateStatus  // UNVERAENDERT 3-wertig, Abschnitt 10.1
faultCode: FaultCode                  // UNVERAENDERT, RAM-only (4.3)
```

**Korrektur gegenüber Vorfassung (Blocker 7):** `disposition:
SafetyDisposition` **entfällt** aus dem Output. Real geprüfter Befund
(Abschnitt 4.3): Der einzige Konsument auf `BASE_SHA` liest ausschließlich
`.gate`, nie `SafetyDisposition`. `SERVICE_REQUIRED`/Diagnoseprojektion
lebt vollständig in `FermentationApplication`/`PresentationState`
(Abschnitt 7.4/11), abgeleitet aus `FaultCode`, nicht aus
`SafetyDisposition`. Kein Typ wird aus historischer Gewohnheit behalten.

### 10.1 Korrektur H – geschlossene Entscheidung: **kein Enum-Wechsel**

Unverändert gegenüber Vorfassung: `ActuatorSafetyGateStatus{Unresolved,
Allowed, ImmediateStop}` bleibt 3-wertig; der bestehende Vertrag trennt
Permission, Grund und physische Reaktion bereits vollständig
(`ActuatorPlanner::runPhaseB()`, real belegt).

### 10.2 Korrektur G – Keep/Move/Drop je Element

| Element | Urteil | Begründung |
|---|---|---|
| `SafetyDisposition` | **Drop** (Korrektur zur Vorfassung, Blocker 7) | kein realer Nicht-Test-Consumer auf `BASE_SHA` (Abschnitt 4.3); `FaultCode` + Gate + Application-Lifecycle genügen |
| `activeFaultMask_` | **Drop** | Interlock wird reine Funktion des aktuellen `ActuationEvidence` |
| `acknowledgedFaultMask_` | **Move** (nach `PresentationState`, Abschnitt 11) | eigener Kommentar bereits „presentation state only" |
| `unknownProducerSources_` | **Drop** | vollständiger, frischer Evidence-Snapshot pro Tick ersetzt dies stateless |
| `canClearFault()` | **Drop** | ohne Maske nichts zu „clearen"; Freshness-Helfer bleiben frei Funktionen |
| `resetRequestWatchdog()` | **Keep**, ohne Maskenzugriff | Vollständiger Vertrag (alle 13 unveränderten Einzelbedingungen + eine frische `evaluate()`-Neuprojektion statt der beiden alten Maskenprüfungen) in Abschnitt 11.1 (Major 11, Runde 4) |
| `FaultCode` | **Keep** (unverändert, RAM-only) | Diagnose-Enum, `dispositionForFault()`-Mapping entfällt mit `SafetyDisposition` (ersetzt durch direktes `FaultCode`→`SERVICE_REQUIRED`-Mapping in `FermentationApplication`, Abschnitt 7) |

### 10.3 `evaluate()` als reine Funktion

`ActuationInterlock::evaluate(const ActuationEvidence&)` wird
`[[nodiscard]] static`, kein Objektzustand, kein `lastEvaluation()`.
`resetCause_` entfällt aus dem Interlock; Composition übergibt `resetCause`
direkt aus `IResetCauseSource` an `PresentationState`. Bestehende
Hilfsfunktionen (`isPersistenceSafeBoot`, `hasFreshConfigurationEvidence`,
`hasFreshSensorEvidence`, `hasFreshIntegrityEvidence`,
`hasResolvedCommitEvidence`, `isTrustedCoordinatorState`) werden unverändert
übernommen.

**Korrektur `isValidFallbackRecoveryEvidence()` – DROP statt unverändert
übernehmen (schließt Blocker 7, Runde 4):** Real geprüft
(`safety_core.cpp:34-41`): die bestehende Funktion liest zwingend
`input.persistenceSnapshot != nullptr` – ein Feld, das der neue
`ActuationEvidence`-Input bewusst **nicht** mehr trägt (ersetzt durch
`loadDisposition`, Abschnitt 10). Eine unveränderte Übernahme kompiliert
also nicht. Fachlich ist die Funktion in R1 zudem überholt: sie gewährte
bislang eine Vertrauensausnahme für `FallbackRecoveryPending`
(„erlaubt trotz technisch noch unbestätigtem Fallback-Zustand, wenn die
Evidenz plausibel aussieht") – R1s bereits geschlossene Policy
(Abschnitt 7.3) lautet aber unbedingt `FallbackRecovered -> SafeBoot ->
SERVICE_REQUIRED`, ohne Ausnahme, ohne Resume, ohne Discard. Eine
Vertrauensausnahme widerspricht dieser Policy strukturell. Entscheidung:

```text
isValidFallbackRecoveryEvidence=DROP
FallbackRecovered / FallbackRecoveryPending -> niemals ALLOWED in R1
kein Trust-Exception-Helper
```

Der ursprüngliche Aufrufort (`hasKnownLoadStatus = isKnown(...) &&
(persistenceCoordinatorState != FallbackRecoveryPending ||
isValidFallbackRecoveryEvidence(input))`) vereinfacht sich zu
`hasKnownLoadStatus = isKnown(...) && persistenceCoordinatorState !=
FallbackRecoveryPending` – ein `FallbackRecoveryPending`-Zustand beobachtet
damit immer `FaultCode::RunPersistenceUntrusted`, ohne Ausnahme. Das ist
keine Verschärfung einer bislang laxeren Regel, sondern die konsequente
Umsetzung der bereits in Runde-1 geschlossenen `FallbackRecovered`-Policy.

```text
INTERLOCK_HAS_NO_PERSISTENCE_SNAPSHOT_INPUT=PASS
INTERLOCK_HAS_NO_FALLBACK_RECOVERY_TRUST_EXCEPTION=PASS
FALLBACK_RECOVERED_ALWAYS_DENIED=PASS
```

**Korrektur `gateNeedsSensorEvidence` (schließt Blocker 6, Runde 2):** Real
geprüft (`safety_core.cpp:270-278`):

```cpp
const bool gateNeedsSensorEvidence =
    input.explicitActivationRequested ||
    input.activationKind != SafetyActivationKind::None ||
    loadDisposition == RunLoadDisposition::ResumeOffer;   // <- entfaellt
```

Der dritte Disjunkt (`loadDisposition == ResumeOffer`) verlangt bereits
während der reinen, unbestätigten ResumeOffer-Anzeige (Abschnitt 9, vor
Confirm) frische Sensor-Evidenz, obwohl in diesem Zustand
`explicitActivationRequested == false` und `activationKind == None` bleiben
(Actuation ist ohnehin `DENIED`, kein Aktivierungsversuch läuft) – ein
verfrühter `SafetySensorUnavailable`-Diagnosehinweis, bevor der Nutzer
überhaupt etwas bestätigt hat. Für R1 entfällt dieser Disjunkt ersatzlos:

```text
gateNeedsSensorEvidence = explicitActivationRequested ||
                           activationKind != None
```

Kein Funktionsverlust: Der Resume-Confirm-Pfad (Abschnitt 9) setzt beim
`ActuationInterlock::evaluate()`-Aufruf nach `activateR1EligibleRun()`
bereits `activationKind=Resume` – die frische Sensor-Evidenzprüfung greift
dort weiterhin, nur nicht mehr während der reinen, folgenlosen
Vorschau-Anzeige davor. **Korrektur (Runde 4):** `loadDisposition` selbst
bleibt Teil von `ActuationEvidence` (Abschnitt 10) – **nicht** mehr für
`isValidFallbackRecoveryEvidence()` (DROP, s. o.), sondern weil der
korrigierte `resetRequestWatchdog()`-Vertrag (Abschnitt 11) ihn für seine
`loadDisposition == RunLoadDisposition::SafeBoot`-Vorbedingung benötigt.

## 11. Fehler-/Ack-/Watchdog-Ownership

| Wahrheit | Owner | current-boot/persistent | Clear-Autorität | Presentation-Owner | Interlock-Projektion |
|---|---|---|---|---|---|
| Konfigurationsfehler | `ConfigurationService`/`ConfigurationRecoveryService` | current-boot (RAM) | Producer selbst | `PresentationState` | `FaultCode ∈ {ConfigurationUnavailable, ConfigurationIntegrityFailure, ConfigurationCommitIndeterminate}` |
| `RunPersistenceUntrusted` | `RunPersistenceCoordinator`/`boot_classification` | current-boot (RAM) | neuer Load-Zyklus | `PresentationState` | `FaultCode::RunPersistenceUntrusted` |
| `SafetySensorUnavailable` | Sensor-Pipeline (#20/#21) | current-boot/laufend (RAM) | Producer selbst | `PresentationState` | `FaultCode::SafetySensorUnavailable` (nicht erreichbar in dieser Revision, Abschnitt 7.4) |
| `ActuatorRequestWatchdog` | ausschließlich `ActuatorPlanner` | current-boot (RAM) | `ActuatorPlanner::applyExternalWatchdogFaultReset()`, nur über `resetRequestWatchdog()` | `PresentationState` | `FaultCode::ActuatorRequestWatchdog` (nicht erreichbar in dieser Revision) |
| Ack | `PresentationState` (aus Interlock verschoben) | current-boot (RAM) | überschrieben durch neuen Fault oder expliziten Reset | `PresentationState` selbst | keine (fließt nie in `evaluate()` zurück) |

`PresentationState` ist ein kleiner, von `FermentationApplication` gehaltener
Typ (`FaultCode` + `bool acknowledged` + optional `ResetCause` + ein neues
`bool applicationAllocationFailure`, s. u.).

**Korrektur: Application-Allokationsfehler ≠ `RunPersistenceUntrusted`
(schließt Major 8, Runde 4):** Die Vorfassung mappte einen fehlgeschlagenen
`runtimeRunState_`/`pendingResume_`-Heapallokation (Abschnitt 9 Blocker 1)
auf denselben `RunPersistenceUntrusted`-Diagnosepfad wie einen technischen
Ladefehler des Stores. Das ist sachlich falsch: ein Application-/Heap-
Allokationsfehler sagt nichts über die Vertrauenswürdigkeit von Store oder
Persistence aus, und die Regel „Producer bleiben Autorität ihrer eigenen
Wahrheit" (Korrektur G) verlangt eine eigene, korrekt attribuierte
Diagnose. Fail-closed-Verhalten bleibt identisch (`SERVICE_REQUIRED`/
`begin()`-Fehlschlag, `Actuation DENIED`, kein Store-Write, kein Resume);
nur die **Diagnoseattribution** ändert sich. Keine neue generische
Fehlerplattform, kein neuer Safety-`FaultCode`-Enumwert (der bestehende
`FaultCode`-Enum bleibt Safety-Produzenten vorbehalten) – stattdessen ein
neues, rein präsentationsseitiges Flag auf `PresentationState`:

```cpp
struct PresentationState {
    FaultCode faultCode{FaultCode::None};
    bool acknowledged{false};
    std::optional<device_platform::ResetCause> resetCause;
    bool applicationAllocationFailure{false};  // NEU (Major 8): reine
      // Diagnoseanzeige, fliesst NIE in ActuationEvidence/evaluate()
      // zurueck, ist kein Safety-FaultCode
};
```

Bei einem Allokationsfehler (Abschnitt 9 Blocker 1, Abschnitt 12.4) setzt
`FermentationApplication`: `presentationState_.faultCode = FaultCode::None`
(kein falsch attribuierter Safety-Fund), `presentationState_.
applicationAllocationFailure = true`. Der `SERVICE_REQUIRED`/`DENIED`-
Zustand selbst wird – wie jeder andere Application-Lifecycle-Zustand –
direkt aus dem fehlgeschlagenen `begin()`/Allokationsschritt abgeleitet,
nicht aus einem Interlock-`evaluate()`-Ergebnis.

```text
APPLICATION_ALLOCATION_FAILURE_NE_PERSISTENCE_UNTRUSTED=YES
PERSISTENCE_TRUST_STATE_NOT_FALSIFIED_BY_OOM=YES
```

### 11.1 `resetRequestWatchdog()` – vollständiger stateless Vertrag (schließt Major 11, Runde 4)

Real geprüfte Vorbedingungsliste des bestehenden `SafetyCore::
resetRequestWatchdog()` (`safety_core.cpp:361-391`, vollständig gelesen):
15 Einzelbedingungen plus eine erneute `evaluate()`-Neuprojektion, dann
`planner.applyExternalWatchdogFaultReset()` + `clearFault()` +
`lastEvaluation_`-Reset. Mit `activeFaultMask_`/`lastEvaluation_`/
`persistenceSnapshot` entfallen drei Dinge, auf die der alte Code direkt
zugreift – der Ersatz wird hier **vollständig**, nicht nur als
Ein-Zeiler, spezifiziert:

```cpp
[[nodiscard]] static bool resetRequestWatchdog(
    ActuatorPlanner& planner, std::uint64_t nowMonotonicMillis,
    const ActuationEvidence& evidence);
```

```text
1.  evidence.actuatorPlanner != &planner            -> false (richtiger Planner)
2.  !planner.state().latchedWatchdogFault.has_value() -> false (Latch vorhanden)
3.  !evidence.explicitActivationRequested            -> false (expliziter Reset-Request)
4.  !evidence.bootValidationComplete                 -> false (Bootvalidierung komplett)
5.  !evidence.plannerEvidenceValidated                -> false (Planner-Evidenz gueltig)
6.  !hasFreshConfigurationEvidence(evidence)          -> false (Configuration frisch/vertrauenswuerdig,
                                                                unveraenderte Hilfsfunktion, Abschnitt 10.3)
7.  !hasFreshSensorEvidence(evidence)                 -> false (Sensor-Evidenz soweit erforderlich,
                                                                unveraenderte Hilfsfunktion; in der
                                                                #121-actor-free-Composition (Abschnitt
                                                                7.4/10.3) ist kein Sensor-Producer
                                                                verdrahtet und dieser gesamte
                                                                Watchdog-Pfad ueber
                                                                FaultCode::ActuatorRequestWatchdog
                                                                (Abschnitt 11) nicht erreichbar - die
                                                                Bedingung bleibt unveraendert fuer die
                                                                spaetere, verdrahtete Composition
                                                                stehen)
8.  !evidence.persistenceValidated                    -> false (Persistence validiert)
9.  !evidence.persistenceLoadStatus.has_value()        -> false
10. !isKnown(*evidence.persistenceLoadStatus)          -> false
11. isPersistenceSafeBoot(*evidence.persistenceLoadStatus) -> false (Persistence frisch/vertrauenswuerdig)
12. !isTrustedCoordinatorState(evidence.persistenceCoordinatorState) -> false
13. evidence.loadDisposition == RunLoadDisposition::SafeBoot -> false
    // ERSETZT classifyRunLoad(status, persistenceSnapshot)==SafeBoot -
    // evidence.loadDisposition ist bereits das vorberechnete
    // Klassifikationsergebnis (Abschnitt 10), persistenceSnapshot
    // existiert im neuen Input nicht mehr (Blocker 7)
14. fresh = ActuationInterlock::evaluate(evidence)     // EINE frische,
    // zustandslose Neuprojektion (Abschnitt 10.3) - ersetzt die alte
    // Zwei-Phasen-Pruefung "stale mask pruefen, dann neu evaluieren",
    // die nur noetig war, weil der alte Code eine zwischen Aufrufen
    // zwischengespeicherte Maske haben konnte. Ein zustandsloses
    // evaluate() hat nichts, das veralten koennte - eine einzige frische
    // Auswertung genuegt.
    if (fresh.faultCode != FaultCode::ActuatorRequestWatchdog) -> false
    // ERSETZT die beiden alten Maskenpruefungen
    // (hasFault(activeFaultMask_, ActuatorRequestWatchdog) &&
    // activeFaultMask_ == exakt dieses eine Bit, d. h. "kein anderer
    // aktuell blockierender Fault"): ein frisch berechnetes faultCode,
    // das exakt ActuatorRequestWatchdog ist, bedeutet bereits, dass kein
    // anderer, hoeher priorisierter Fault (primaryFault()-Praezedenz,
    // unveraendert wiederverwendet) gleichzeitig aktiv ist - dieselbe
    // Garantie wie die alte "nur dieses eine Bit gesetzt"-Pruefung,
    // ohne eine persistierte Maske zu benoetigen.
15. planner.applyExternalWatchdogFaultReset(nowMonotonicMillis)
16. return true
```

**Explizit (Owner-Anforderung):** Bedingungen 1-13 sind die **unveränderten**
Einzelbedingungen des bestehenden Codes (mechanisch auf `ActuationEvidence`
statt `SafetyCoreInput` umbenannt, keine inhaltlich neue Bedingung
entfernt) – nur die beiden maskenbasierten Zeilen (alt: `hasFault(...)`/
`activeFaultMask_ != faultBit(...)`) werden durch die eine frische
`evaluate()`-Neuprojektion (14) ersetzt. **Kein** Zusammenfassen der
übrigen 13 Bedingungen in einen einzigen `faultCode`-Vergleich – das würde
mindestens Bedingung 4/5 (`bootValidationComplete`/
`plannerEvidenceValidated`) verlieren: beide beeinflussen real
ausschließlich `permission` über `activationEvidenceComplete()`
(`safety_core.cpp:631-632`, real geprüft), nicht `faultCode` – ein reiner
`faultCode`-Vergleich würde diese beiden Sicherheitsbedingungen
stillschweigend fallen lassen.

**Nach erfolgreichem Reset gibt es keinen Interlock-State zu clearen** (im
Unterschied zum alten Code, der `clearFault()`/`lastEvaluation_ =
SafetyEvaluation{}` aufrief): `ActuationInterlock` ist zustandslos, der
nächste `evaluate()`-Aufruf projiziert ohnehin frisch – der einzige
tatsächlich mutierte Zustand ist `planner.applyExternalWatchdogFaultReset()`
(unverändert, `ActuatorPlanner`-Autorität, Korrektur G).

```text
WATCHDOG_RESET_REEVALUATES_FRESH_EVIDENCE=PASS
WATCHDOG_RESET_REJECTS_OTHER_CURRENT_FAULT=PASS
WATCHDOG_RESET_HAS_NO_INTERLOCK_LATCH_TO_CLEAR=PASS
```

## 12. Composition (`FermentationApplication`, kleinste, actor-free Fassung)

**Application-interne Fassade, kein neuer Root-Helper** (unverändert
begründet gegenüber Vorfassung). `main/app_main.cpp` ändert sich minimal.
Die konkrete ESP-IDF-Zeitzonenimplementierung bleibt ausschließlich in der
ESP-IDF-Composition-Root; die Fachkomponente sieht nur den bestehenden
abstrakten Plattformport.

```text
main/app_main.cpp (geaendert, minimal):
  NvsOwningContext::create()  // unveraendert
  const device_platform_esp_idf::EspTimeZoneResolver timeZoneResolver;
    // Root-Value-Objekt, keine Heapallokation
  application.begin(platform, stateStoreContext->store(), timeZoneResolver,
                    &resetCauseSource)
    // NEU: IStateStore&- und ITimeZoneResolver&-Parameter ueber die
    // bestehenden abstrakten Ports; beide Root-Objekte ueberleben die
    // Application (Abschnitt 12.2)

FermentationApplication::begin(
    platformServices, store, timeZoneResolver, resetCauseSource):
  // Vollstaendiger ESP32-Produkt-Overload; alle Typen in der
  // fermentation_app-Signatur stammen aus device_platform.
  2. bootstrapStore_, graphStore_, mutationCoordinator_,
     configurationService_ mit store/timeZoneResolver konstruieren
  3. ConfigurationRecoveryService::create(store, bootstrapStore_, graphStore_,
     configurationService_, mutationCoordinator_) -> boot()  // boot-only,
     danach zerstoert
  3a. configurationService_.acquireRuntime() -> RuntimeConfigurationReadResult
      // NEU spezifiziert (Blocker 7, Abschnitt 4.6): liefert epoch ueber
      // lease.get().storageEpoch(); bei status != RuntimeLeaseGranted wird
      // Schritt 4 UEBERSPRUNGEN, RunPersistenceCoordinator bleibt nullptr,
      // Application setzt SERVICE_REQUIRED (Abschnitt 4.6/7); lease ist
      // lokal, ueberlebt Schritt 3a nicht.
  4. runPersistenceCoordinator = std::unique_ptr<RunPersistenceCoordinator>{
       new (std::nothrow) RunPersistenceCoordinator(store, epoch, schedule)}
     if (runPersistenceCoordinator == nullptr) -> Allokationsfehler,
       fail-closed, SERVICE_REQUIRED (Abschnitt 12.1) - eine einzige
       Allokation fuer den gesamten Coordinator inkl. seines inline
       RunPersistenceWorkingSet-Wertmembers (Abschnitt 12.4.4, Runde 5),
       keine zweite/separate Scratch-Allokation
     runPersistenceLoadResult = std::unique_ptr<RunPersistenceLoadResult>{
       new (std::nothrow) RunPersistenceLoadResult()}
     if (runPersistenceLoadResult == nullptr) -> Allokationsfehler,
       fail-closed, SERVICE_REQUIRED
     runPersistenceCoordinator->loadAndInitializeInto(
       *runPersistenceLoadResult)
     // epoch == lease.get().storageEpoch() aus Schritt 3a. KEIN
     // `new (std::nothrow) RunPersistenceLoadResult(coordinator->
     // loadAndInitialize())` mehr (Abschnitt 12.4.5, Blocker 2 Runde 5):
     // loadAndInitialize()s interne loadReference()-Lambda konstruierte
     // weiterhin lokale std::optional<RunPersistenceRawRecord>-Werte
     // (real gemessen sizeof(RunPersistenceRawRecord)=4152 B) - ein
     // aeusserer Heap-Zielwert allein loeste diese Callee-interne Ebene
     // nicht. loadAndInitializeInto() schreibt jetzt durchgaengig in
     // Referenzparameter/das Coordinator-eigene workingSet_.record
     // (Abschnitt 12.4.5). sizeof(RunPersistenceLoadResult)==4112 B war
     // schon als lokaler Rueckgabewert allein ueber dem 3584-B-Budget;
     // Grundmuster uebernommen als Randbedingung aus dem bereits
     // ownerreviewten #119/#120-Stackfix (kein Code-Cherrypick).
  5. boot_classification::classify(configResult, *runPersistenceLoadResult)
     -> BootClassification (Abschnitt 7); runPersistenceLoadResult ist
     boot-transient (lokale unique_ptr-Variable in begin(), kein
     Application-Lifetime-Member) und wird nach diesem Schritt nicht mehr
     benoetigt
  6. je nach BootClassification: sofort Application-Lifecycle setzen,
     ProcessRuntimeState ggf. published (7.2), pendingResume_ ggf. gesetzt
     (9), Actuation bleibt DENIED
  7. ActuationInterlock::evaluate(evidence) mit den realen Werten aus
     Schritt 3-5 (kein leeres SafetyCoreInput mehr, 4.1)

KEIN ActuatorPlanner, KEIN ActuatorPlanSinkDriver, KEIN TemperatureController,
KEIN TargetQualificationEvaluator, KEIN TemperatureControlApplicationOrchestrator
in dieser Composition (Abschnitt 3/4.9, Blocker 5/11). RunRecoveryCoordinator
NICHT konstruiert (Korrektur B).
```

Der vollständige ESP32-Produkt-Overload lautet:

```cpp
[[nodiscard]] bool begin(
    device_platform::IPlatformServices& platformServices,
    device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& timeZoneResolver,
    const device_platform::IResetCauseSource* resetCauseSource = nullptr);
```

`fermentation_application.hpp/.cpp` enthalten dabei weder
`device_platform_esp_idf::` noch einen ESP-IDF-Header. `ConfigurationService`
erhält die injizierte abstrakte Referenz direkt:

```cpp
ConfigurationService(
    *mutationCoordinator_, *graphStore_, timeZoneResolver);
```

Der bestehende schmale Native-/Smoke-Overload bleibt zusätzlich erhalten:

```cpp
[[nodiscard]] bool begin(
    device_platform::IPlatformServices& platformServices,
    const device_platform::IResetCauseSource* resetCauseSource = nullptr);
```

Dieser Overload bleibt ein reiner Native-Smoke-Kompatibilitätspfad ohne
`IStateStore`-, `ITimeZoneResolver`-, ESP-IDF- oder
`device_platform_test_support`-Composition. Er erzeugt keine Persistence-/
Timezone-Composition und keine Aktorfreigabe. Der ESP32-Produktpfad verwendet
ausschließlich den vollständigen Overload oben. Es gibt keine dritte
`begin()`-Variante.

Die Architekturgrenzen bleiben dabei unverändert und sind Teil des
Schritt-6-Vertrags:

```text
lib/fermentation_app/CMakeLists.txt:
  REQUIRES device_platform
  unveraendert

main/CMakeLists.txt:
  PRIV_REQUIRES device_platform fermentation_app device_platform_esp_idf ...
  bereits ausreichend

scripts/check_architecture_boundaries.py:
  unveraendert
  device_platform_esp_idf bleibt unter fermentation_app verboten

docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md:
  unveraendert
```

`NvsOwningContext` und der konkrete `EspTimeZoneResolver` werden in
`main/app_main.cpp` vor der `FermentationApplication` deklariert und leben
bis nach deren Zerstörung. Die native `src/main.cpp`-Composition bleibt ohne
ESP-IDF- und ohne `device_platform_test_support`-Abhängigkeit; sie verwendet
nur den erhaltenen schmalen Native-/Smoke-Overload. Es gibt keine lokale
Fake-`IStateStore`-Implementierung und keinen neuen Host-StateStore.

### 12.1 Vollständige Ownership-/Lifetime-Tabelle (Blocker 6, geschlossen)

| Objekt | Owner | Lifetime | Depends On | Boot-only? | Zerstörungsreihenfolge | Allocation-Failure |
|---|---|---|---|---|---|---|
| `NvsStateStore` (`IStateStore`) | `NvsOwningContext` (in `main/app_main.cpp`, unverändert) | Prozesslaufzeit, überlebt alle Consumer | `NvsOwningContext`-Partition | NEIN | letztes (nach `FermentationApplication`) | bestehend (`nullptr`-Rückgabe von `create()`) |
| Konkreter `EspTimeZoneResolver` | ESP-IDF-Composition-Root (`main/app_main.cpp`) | Root-Value-Objekt; überlebt `FermentationApplication` | keine | NEIN | vor `FermentationApplication` zerstört | N/A – keine Heapallokation |
| `ITimeZoneResolver`-Referenz | `ConfigurationService` als non-owning Consumer | nur über die Lebensdauer des Consumers | Root-Value-Objekt `EspTimeZoneResolver` | NEIN | Provider lebt länger als `FermentationApplication` | N/A – keine Ownership |
| `ConfigurationMutationCoordinator` | `FermentationApplication` (`unique_ptr`) | Application-Laufzeit | keine externen | NEIN (`ConfigurationService` hält Referenz) | **nach** `configurationService_` (Runde-4-Korrektur, Major 10) | fail-closed |
| `ConfigurationBootstrapStore` | `FermentationApplication` (`unique_ptr`) | **boot-only möglich** – real geprüft: nur `ConfigurationRecoveryService::create()` nimmt sie entgegen, kein weiterer Konsument nach `boot()` gefunden | `IStateStore` | JA (kann nach Schritt 3 zerstört werden) | vor `IStateStore` | fail-closed |
| `ConfigurationGraphStore` | `FermentationApplication` (`unique_ptr`) | **Application-Laufzeit** – `ConfigurationService`-Konstruktor hält `ConfigurationGraphStore&` laufend (Abschnitt 4.6), NICHT boot-only | `IStateStore` | NEIN | **nach** `configurationService_` (Runde-4-Korrektur, Major 10), vor `IStateStore` | fail-closed |
| `ConfigurationService` | `FermentationApplication` (`unique_ptr`) | Application-Laufzeit | `mutationCoordinator_`, `graphStore_`, externe `ITimeZoneResolver`-Referenz (alle müssen mindestens gleich lang leben) | NEIN | **vor** den beiden Application-Dependencies; der Root-Resolver lebt ebenfalls weiter | fail-closed |
| `ConfigurationRecoveryService` | lokal in `begin()` (`unique_ptr`, per `create()`) | **boot-only** (nur `boot()` aufgerufen, danach freigegeben) | `store`, `bootstrapStore_`, `graphStore_`, `configurationService_`, `mutationCoordinator_` | JA | vor `bootstrapStore_` | bereits bestehender `nullptr`-Vertrag von `create()` |
| `RunPersistenceCoordinator` | `FermentationApplication` (`unique_ptr`) | Application-Laufzeit (auch für Fresh Start/Resume-Aufrufe außerhalb von `begin()`) | `IStateStore`, `epoch` (aus Schritt 3a, Abschnitt 4.6/12), `schedule` | NEIN | vor `IStateStore` | fail-closed **plus** (Blocker 7, neu) bei `acquireRuntime().status != RuntimeLeaseGranted`: `unique_ptr` bleibt `nullptr`, Coordinator wird gar nicht konstruiert, `FermentationApplication` setzt `SERVICE_REQUIRED` |
| `pendingResume_` (`std::unique_ptr<RunCommandState>`, Runde-3-Korrektur: NICHT `optional`, Abschnitt 9 Blocker 1) | `FermentationApplication` (`unique_ptr`) | von Klassifikation bis Confirm/Reject (Abschnitt 9) | keine externen Referenzen | NEIN | trivial (`unique_ptr`-Member) | fail-closed: `new (std::nothrow)` schlägt fehl → `nullptr` bleibt, kein Resume-Angebot, `SERVICE_REQUIRED` |
| `runtimeRunState_` (`std::unique_ptr<RunCommandState>`, Runde-3-Korrektur: NICHT `optional`, Blocker 1) | `FermentationApplication` (`unique_ptr`) | Application-Laufzeit; einzige Quelle für `publishedProcessState()` (Abschnitt 9) | keine externen Referenzen | NEIN | trivial (`unique_ptr`-Member) | fail-closed: `new (std::nothrow)` schlägt fehl → kein publizierter Lauf, `SERVICE_REQUIRED` |
| `PresentationState` | `FermentationApplication` (Wertmember) | Application-Laufzeit | keine | NEIN | trivial | entfällt |

**Verbindlich (Zerstörungsreihenfolge präzisiert, Major 10, Runde 4 – die
Vorfassung enthielt widersprüchliche Formulierungen: die Dependency-Zeilen
sagten „vor `configurationService_`" zerstört, während
`ConfigurationService`s eigene Zeile „vor den Dependencies" sagte –
beide können nicht gleichzeitig gelten):** `ConfigurationService` hält
`mutationCoordinator_`/`graphStore_` sowie den extern injizierten
`ITimeZoneResolver` als **Referenzen**
(Konstruktorvertrag, Abschnitt 4.6) – diese Referenzen müssen während der
gesamten Lebensdauer von `ConfigurationService`, **einschließlich seiner
eigenen Destruktorausführung**, gültig bleiben. Korrekt ist daher: die beiden
Application-Dependencies werden **nach** `ConfigurationService` zerstört
(nicht davor); der Root-Resolver lebt unabhängig davon ebenfalls länger als
die Application.
In C++ werden Member in **umgekehrter Deklarationsreihenfolge** zerstört;
um „`ConfigurationService` zuerst, Application-Dependencies danach" zu
erreichen, müssen die beiden Application-Dependency-Member **vor**
`configurationService_`
**deklariert** werden:

```cpp
class FermentationApplication {
    // ...
    std::unique_ptr<ConfigurationBootstrapStore> bootstrapStore_;
    std::unique_ptr<ConfigurationMutationCoordinator> mutationCoordinator_;
    std::unique_ptr<ConfigurationGraphStore> graphStore_;
    std::unique_ptr<ConfigurationService> configurationService_;  // NACH
      // den beiden Application-Dependencies deklariert -> wird bei
      // Zerstoerung ZUERST aufgeraeumt (umgekehrte Deklarationsreihenfolge),
      // Dependencies bleiben waehrend seiner Destruktorausfuehrung gueltig
    // ...
};
```

Konstruktionsreihenfolge in `begin()` entspricht dieser Deklarations-
reihenfolge (Member werden in Deklarationsreihenfolge konstruiert). Kein
manueller Destruktor nötig (reines Member-RAII). `IStateStore` bleibt
ausschließlich bei
`NvsOwningContext` (unverändert). Ein partiell fehlgeschlagenes `begin()`
(z. B. Allokationsfehler bei Schritt 2) gibt `false` zurück; bereits
konstruierte `unique_ptr`-Member von `FermentationApplication` werden beim
Zerstören des `FermentationApplication`-Objekts (durch `app_main()`s
bestehenden Fehlerpfad, Abschnitt 4.8) RAII-sicher freigegeben – keine
dangling References, da nichts von einem Objekt referenziert wird, das
bereits zerstört ist (`IStateStore` überlebt in jedem Fall, da
`NvsOwningContext` außerhalb von `FermentationApplication` liegt).

### 12.2 Neue kleine #121-Zusätze (Blocker 6/12, nicht aus #120 übernommen)

1. **`NvsOwningContext::store()`** (`main/app_main.cpp`):
   ```cpp
   [[nodiscard]] device_platform::IStateStore& store() const noexcept;
   ```
   `NvsOwningContext` bleibt alleiniger Eigentümer; die Rückgabe ist
   non-owning. `NvsOwningContext` überlebt `FermentationApplication` und
   jeden Store-Konsumenten (unverändert bestehender Vertrag, jetzt nur
   sichtbar gemacht).
2. **`device_platform_esp_idf::EspTimeZoneResolver`** (neu, eigenständig für
   #121 entworfen, nicht aus #120 kopiert): kleiner, zustandsloser Adapter,
   der `device_platform::ITimeZoneResolver` implementiert und ausschließlich
   von der ESP-IDF-Composition-Root als Root-Value-Objekt gehalten wird. Die
   `fermentation_app`-Composition erhält nur die abstrakte Referenz.
   Design-Referenz
   (nicht Code-Übernahme): #119-Plandatei Abschnitt 5.1 identifizierte
   dieselbe Lücke und dieselbe Zielarchitektur (ESP-IDF-native
   Zeitzonenauflösung ohne Netzwerkabhängigkeit). Scope-Grenze: nur so viel
   wie `ConfigurationService`s Konstruktor benötigt, keine WLAN-/NTP-
   Integration (bleibt spätere Issues).

   **Vollständiger Vertrag (schließt Major 10, Runde 2).** Interface real
   geprüft (`device_platform/src/time_zone_resolver.hpp`, unverändert):

   ```cpp
   enum class TimeZonePrepareStatus : std::uint8_t {
       Success, UnsupportedIdentifier, PreparationFailed,
   };
   struct PreparedTimeZone { std::string canonicalIdentifier; };
   struct TimeZonePrepareResult {
       TimeZonePrepareStatus status;
       std::optional<PreparedTimeZone> prepared;
   };
   class ITimeZoneResolver {
       virtual TimeZonePrepareResult prepare(
           const std::string& canonicalIdentifier) const = 0;
   };
   ```

   `EspTimeZoneResolver::prepare()` implementiert dies als reine, statische
   Nachschlagetabelle – **keine** OS-/IANA-Zeitzonendatenbank, **kein**
   NTP-Zugriff, **keine** WLAN-Abhängigkeit, **keine** externe Bibliothek
   (Interface-Kommentar bestätigt dies bereits explizit: „Eine reale
   ESP32-Zeitzonendatenbank ist nicht Bestandteil dieses Ports."). **Explizit
   ausgeschlossen (schließt Major 8, Runde 3 – präziser als das allgemeine
   `NO_OS_DEPENDENCY`):**

   ```text
   NO setenv("TZ", ...)
   NO tzset()
   NO system-local-time mutation
   ```

   `prepare()` mutiert keinerlei Prozess-/System-globalen Zustand (kein
   `TZ`-Environment, keine libc-Zeitzonenumschaltung) – die Rückgabe
   (`PreparedTimeZone{canonicalIdentifier}`) ist ein reiner Wert, den der
   Aufrufer (`ConfigurationService`) selbst hält; `EspTimeZoneResolver`
   besitzt keine Seiteneffekte außerhalb seines Rückgabewerts. R1-Scope:
   genau **ein** unterstützter Bezeichner.

   | `canonicalIdentifier` | Ergebnis |
   |---|---|
   | `"Europe/Zurich"` (real bereits etablierter App-Katalogwert, siehe `firmware_configuration_catalog.hpp/cpp`, `docs/SETTINGS_AND_STORAGE.md`, fünf bestehende Testdateien – nicht erfunden) | `Success`, `PreparedTimeZone{"Europe/Zurich"}` |
   | jeder andere String (inkl. leer) | `UnsupportedIdentifier`, `prepared = std::nullopt` |

   `PreparationFailed` ist für R1 **unerreichbar** (kein Fehlschlagpfad ohne
   echte externe Ressource – reine Tabellen-Nachschlage hat keinen
   Laufzeitfehlerfall) und bleibt nur für eine spätere, echte
   Zeitzonendatenbank reserviert; kein Code erzeugt ihn in dieser Revision.
   Der Adapter hat keinen Konstruktorparameter, keinen inneren Zustand, kein
   Caching – jeder `prepare()`-Aufruf ist unabhängig und deterministisch.

   Die ESP-IDF-Composition-Root hält den konkreten Adapter als normales
   Value-Objekt. Es gibt keine Heapallokation und damit keinen
   Resolver-Allokationsfehlerpfad:

   ```text
   ESP_TIME_ZONE_RESOLVER_OWNER=ESP_IDF_COMPOSITION_ROOT
   ESP_TIME_ZONE_RESOLVER_HEAP_ALLOCATION=NO
   ESP_TIME_ZONE_RESOLVER_ALLOCATION_FAILURE_PATH=NOT_APPLICABLE
   ```

**Lebenszeit-Klarstellung (Abweichung von einer ersten Fassung):** Anders
als #119/#120s rein *boot-only* Objekte müssen die meisten hier komponierten
Objekte für die **gesamte Laufzeit** existieren (siehe Tabelle 12.1), nicht
nur bis Boot-Ende – u. a. weil Fresh Start/Resume (Abschnitt 8/9) nach
`begin()` erneut `RunPersistenceCoordinator`/`ConfigurationService`
aufrufen. Application-eigene Objekte werden deshalb **heapbesitzende Member von
`FermentationApplication`** (`std::unique_ptr<T>`, `new (std::nothrow)`,
fail-closed bei Allokationsfehler), nicht Werte-Member und nicht lokale
Stack-Objekte in `begin()`. `sizeof(FermentationApplication)` bleibt dadurch
klein (nur Zeiger, inklusive der beiden `unique_ptr<RunCommandState>`-Member
`runtimeRunState_`/`pendingResume_`, Abschnitt 9 Blocker 1, plus
`PresentationState`), unabhängig von der Größe des komponierten Graphen.
Der konkrete `EspTimeZoneResolver` ist davon ausgenommen: Er bleibt als
zustandsloses Root-Value-Objekt außerhalb der Application bestehen.

**Stack-Sicherheit der neu produktiv erreichbaren Pfade (real gemessen,
kein offenes Restrisiko mehr – vollständiger Vertrag Abschnitt 12.4,
Blocker 1-5, Runde 4):** `RunPersistenceCoordinator::persistCommand()`
allein hat einen real Xtensa-gemessenen statischen Frame von 9280 Byte
(`docs/ISSUE_29_BUILD_REPORT.md`) – über dem gesamten
`CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584`-Budget, unabhängig von jeder
Call-Path-Summierung. Eine frühere Fassung dieses Plans bezeichnete das
zugrundeliegende `auto candidate = current;`-Muster fälschlich als „später
per Hardware-HWM zu messendes, in dieser Revision nicht gelöstes
Restrisiko" – das ist mit dieser realen Messung nicht mehr haltbar.
Abschnitt 12.4 legt für jeden der neu erreichbaren Pfade
(`RunCommandState`-Restore, `CommandDecision`-Konstruktion, Coordinator-
`RunPersistenceWorkingSet` inkl. Snapshot-/RawRecord-Projektion und
Decode, `RunPersistenceLoadResult`) einen konkreten, stack-sicheren Vertrag
fest (durchgängige Out-Parameter-/In-place-Kerne bzw. das inline
Coordinator-eigene Wertmember, Abschnitt 12.4.4-4.6) und macht die
Einhaltung zu einer **Pflichtmessung vor jeder Hardwarefreigabe**
(statischer Produkt-Stackgate, Abschnitt 12.4.7), nicht zu einer
nachgelagerten, optionalen HWM-Beobachtung.

**Abnahmemetrik (korrigiert, Major 6, Runde 4 – actor-free Scope
respektiert):** Eine frühere Fassung verlangte für den realen Main-Task-
HWM den vollständigen `begin()`-Aufruf **plus** einen gezielt ausgelösten
Fresh-Start- oder Resume-Aufruf. Das widerspricht dem actor-free #121-Scope
(Abschnitt 3/6): #121 komponiert bewusst keine reale Sensorpipeline, keine
UI-/Command-Quelle, keinen Planner, keine Aktoren; ein aktiver Resume
benötigt echte `CrossRolePlausibilityContext`-Evidenz, Fresh Start hat
bestehende Sensor-/Safety-Startvorbedingungen – #121 hat für beide **keinen
echten Produkttrigger**. Ein eigens dafür erfundener Wegwerf-Testpfad nur
für die HWM-Messung ist nicht zulässig. Korrigiert:

```text
REAL_HARDWARE_BOOT
  -> actor-free echter Boot-/Classification-Pfad
  -> realer Main-Task-HWM (uxTaskGetStackHighWaterMark, bereits heute in
     logResources() verwendet) dieses tatsaechlich erreichbaren Pfads
  -> gegen CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584 (Abschnitt 16)

STATIC_STACK_EVIDENCE=MANDATORY_NOW (Abschnitt 12.4.7, vor Hardware)
REAL_RUNTIME_HWM=NOT_RUN_UNTIL_REAL_PRODUCT_TRIGGER_PATH_EXISTS
  // Fresh Start/Resume-HWM bleibt ehrlich NOT_RUN, bis eine spaetere
  // Issue-Erweiterung (#25/#26/#31/#106, Abschnitt 3) einen echten,
  // nicht erfundenen Produkttrigger liefert. Existiert ohne neue
  // Architektur bereits ein echter Trigger, darf er genutzt werden.
```

`app_main()`s eigener Entry-Frame bleibt beim heutigen, real gemessenen
`BASE_SHA`-Wert von 112 Byte (Abschnitt 4.8) als zusätzlicher, unabhängiger
Nachweis.

### 12.3 Runtime-Verhalten (`update()`, Blocker 10)

```text
#121_PRODUCT_RUNTIME=ACTOR_FREE
```

**Entscheidung (keine offene Alternative):** `FermentationApplication::
update()` bleibt in dieser Revision **vollständig leer** (unverändert
gegenüber dem heutigen `BASE_SHA`-Stand, Abschnitt 4.1). Kein
`checkpointPeriodic()`-Aufruf: eine actor-free Composition hat außerhalb
eines expliziten Fresh-Start-/Resume-Aufrufs keinen periodisch zu
prüfenden aktiven Lauf, für den ein periodischer Checkpoint fachlich
etwas leisten würde. Keine produktiven PI-/Planner-Ticks; kein
`tickActuatorPlan()`-Aufruf im Produktpfad.

### 12.4 Stack-Sicherheits-Vertrag für #121-Produktpfade (Blocker 1-6, Runde 4+5+6, geschlossen)

**Ausgangslage – kein Restrisiko, sondern bewiesener Blocker (Blocker 1,
Runde 4):** `docs/ISSUE_29_BUILD_REPORT.md` (real vorhanden, Xtensa-
`-fstack-usage`-Messung auf `BASE_SHA`) dokumentiert bereits:

```text
RunPersistenceCoordinator::persistCommand(...) = 9280 B static frame
CONFIG_ESP_MAIN_TASK_STACK_SIZE = 3584 B
```

`9280 > 3584` – ein **einzelner** Funktionsaufruf sprengt bereits das
gesamte Main-Task-Budget, unabhängig von jeder Call-Path-Summierung. Kein
Primärfix über `CONFIG_ESP_MAIN_TASK_STACK_SIZE`
(`MAIN_TASK_STACK_PRIMARY_INCREASE=NO` bleibt in Kraft). Zusätzlich real
gemessen (Host-Build, `g++ -std=c++17`, dieselbe Technik wie Runde 3):

```text
sizeof(RunCommandState)           = 5096 B
sizeof(CommandDecision)           = 10520 B  (enthaelt RunCommandState
                                               before + after)
sizeof(RunPersistenceSnapshot)    = 4096 B
sizeof(RunPersistenceLoadResult)  = 4112 B   (optional<RunPersistenceSnapshot>)
sizeof(RunPersistenceRawRecord)   = 4152 B   (NEU real gemessen, Runde 5;
                                               enthaelt selbst ein
                                               vollstaendiges
                                               RunPersistenceSnapshot)
sizeof(RunPersistenceCoordinator) = 8968 B   (NEU real gemessen, Runde 5;
                                               bereits auf BASE_SHA gross,
                                               da slots_[2] zwei
                                               optional<RunPersistenceRawRecord>
                                               als Instanzmember haelt)
```

Jeder dieser Werte liegt bereits einzeln über dem 3584-B-Budget.
**Korrektur gegenüber Runde 4 (Runde 5):** Runde 4 schloss nur die
Aufrufer-Ebene (Application/Coordinator-öffentliche Signaturen). Real
geprüft blieben zwei Ebenen weiterhin stack-unsicher: (a) `writeSnapshotCore()`
(`run_persistence_coordinator.cpp:1571`) konstruiert intern weiterhin einen
lokalen `RunPersistenceRawRecord record{...}`; (b) `loadAndInitialize()`s
`loadReference`-Lambda (Zeile 347-386) gibt `std::optional<
RunPersistenceRawRecord>` per Wert zurück (zweimal aufgerufen als
`currentRecord`/`fallbackRecord`-Lokale, Zeile 388/465), und
`decodeRunPersistenceSnapshot()` (`run_persistence_codec.cpp:1151`)
konstruiert intern `RunPersistenceSnapshot s;` als lokale Variable. Beide
Ebenen sind jetzt Teil dieses Vertrags (4.4/4.5/4.6 unten). Stack-Sicherheit
der neu produktiv erreichbaren #121-Pfade bleibt **Software-/
Build-Vorbedingung vor jeder Hardwareverifikation** (Schritt 7/8,
Abschnitt 15), nicht eine nachgelagerte Messung.

**Scope:** Nur die durch #121 neu real erreichbaren Pfade werden
korrigiert – nicht pauschal alle 13 historischen `auto candidate =
current;`-Fundstellen (Runde 3), nicht die gesamte Command-/
Persistence-Architektur, nicht `RunRecoveryCoordinator`/C2:

```text
FermentationApplication::begin()
RunPersistenceCoordinator::loadAndInitializeInto()
Boot Current -> restore/classify/discard
Completed/Fault restore
ResumeOffer construction
Resume confirm -> activateR1EligibleRun()
Resume reject -> discardAsNoActiveRun()
Fresh Start decision creation
Fresh Start -> RunPersistenceCoordinator::persistCommand()
```

Zeigt die statische Callgraph-Analyse (Abnahmemetrik unten), dass eine
weitere bestehende Stelle real auf einem #121-Produktpfad liegt, wird sie
nach demselben Vertrag korrigiert. Würde das eine **neue Architektur**
statt einer begrenzten Speicher-/Scratchkorrektur erfordern: **STOP –
OWNER_REVIEW_REQUIRED**, keine eigenmächtige Architekturentscheidung.

**4.1 Fail-closed-Allokation – einheitliche `nothrow`-Semantik (schließt
Blocker 4, Runde 4):** Jede in diesem Plan spezifizierte Heap-Allokation
verwendet **ausschließlich**:

```cpp
std::unique_ptr<T> ptr{new (std::nothrow) T(...)};
```

**Korrektur gegenüber der Vorfassung (realer Fehler, Runde 4):** Mehrere
konkrete Flows dieses Plans (§7 `NoRun`-Zeile, §9 `pendingResume_`-
Konstruktion, §9 `DiscardableRun`-`discardTarget`-Konstruktion) schrieben
tatsächlich `std::make_unique<RunCommandState>(...)`. `std::make_unique`
verwendet intern werfendes `new T(...)`, nicht `new (std::nothrow) T(...)`
– bei Allokationsfehler wirft es `std::bad_alloc` (bzw. löst auf einer
Plattform ohne Exception-Unterstützung `abort()`/den `operator new`-
Fehlerpfad aus) statt `nullptr` zurückzugeben, und erfüllt damit den an
anderer Stelle im selben Plan geforderten „prüfe auf `nullptr`"-Vertrag
nicht. Jede der drei genannten Stellen wird auf
`std::unique_ptr<RunCommandState>{new (std::nothrow) RunCommandState(...)}`
korrigiert (Abschnitt 9, s. u.).

```text
FAIL_CLOSED_ALLOCATION_USES_NOTHROW=YES
MAKE_UNIQUE_USED_FOR_FAIL_CLOSED_ALLOCATION=NO
```

**Geltungsbereich präzisiert (Blocker 6, Runde 6, Abschnitt 4.10 unten):**
dieser `nothrow`-Vertrag gilt für die hier benannten **expliziten**
`new (std::nothrow) T(...)`-Objektallokationsgrenzen; er macht spätere,
innerhalb bereits konstruierter Objekte auftretende STL-Unterallokationen
(`std::string`/`std::vector` in `RunCommandState`/`RunPersistenceSnapshot`)
nicht automatisch `nothrow` – siehe Abschnitt 4.10 für den vollständigen,
ehrlich verengten Vertrag.

**4.2 `restoreRunPersistenceSnapshot()` – stack-sicherer Produktpfad-Vertrag
(schließt Blocker 2.1, Runde 4):** Real geprüft
(`run_persistence_contract.cpp:470+`): die bestehende Funktion konstruiert
intern `RunCommandState restored;` (5096 B) als lokale Variable und gibt
sie per `std::optional<RunCommandState>` zurück. Ob NRVO diese Kopie
eliminiert, ist **nicht** vom Standard garantiert – der Produktpfad darf
sich darauf nicht verlassen. Neue, zusätzliche Out-Parameter-Variante,
**nur** vom Produktpfad verwendet:

```cpp
// Schreibt direkt in destination, keine lokale RunCommandState-Kopie
// innerhalb der Funktion. Rueckgabe signalisiert nur Erfolg/Fehlschlag,
// identische Validierungslogik wie die bestehende Funktion
// (validateRunPersistenceSnapshot() zuerst).
[[nodiscard]] bool restoreRunPersistenceSnapshotInto(
    const RunPersistenceSnapshot& snapshot, RunCommandState& destination);
```

Die bestehende `restoreRunPersistenceSnapshot()` (Return-by-value) bleibt
**unverändert** für Host-/Legacy-Tests (`test_run_persistence_coordinator`,
`test_run_checkpoint_codec`) bestehen – kein API-Bruch. Jeder Produktpfad-
Aufrufer (§9 `ResumeOffer`, `DiscardableRun`; §9.1-Aufrufer für
`Completed`/`Fault`) folgt demselben Muster:

```text
1. target = std::unique_ptr<RunCommandState>{
     new (std::nothrow) RunCommandState()}
   if (target == nullptr) -> fail-closed (SERVICE_REQUIRED, kein Publish)
2. if (!restoreRunPersistenceSnapshotInto(*loaded.snapshot, *target))
   -> fail-closed (technischer Konsistenzfehler, SafeBoot/SERVICE_REQUIRED)
3. weiterverwendet als *target (kein zusaetzlicher Stackframe fuer den
   restaurierten Zustand)
```

**4.3 `CommandDecision` – echter In-place-Kern statt Wrapper (schließt
Blocker 2.2 Runde 4, Blocker 6 Runde 5):** Real geprüft
(`run_commands.hpp:394-414`, `:426/428`; Implementierung
`run_commands.cpp:187-221`, `:687-797`): `CommandDecision` enthält
`RunCommandState before; RunCommandState after;` (zusammen bereits >10 KiB,
real gemessen `sizeof(CommandDecision)=10520`). **Korrektur gegenüber
Runde 4 (real gefundener Entwurfsfehler, Blocker 6):** Runde 4 skizzierte
`decideProgramStartInto()`/`decideManualStartInto()` nur als dünne Wrapper
um die bestehenden Return-by-value-Kerne `beginDecision()`/`result()`. Real
geprüft mutieren `beginDecision()`/`result()` und alle `decide*()`-Funktionen
(z. B. `decideProgramStart()`, Zeile 687-797) ein einziges lokales
`CommandDecision decision`-Objekt **durchgängig über die gesamte Funktion**
(Dutzende Zugriffe auf `decision.status`/`.before`/`.after`, Hilfsaufrufe wie
`requireRunRevision(decision)`/`addEffect(decision, ...)`, die
`CommandDecision&` erwarten) – ein Wrapper, der diese Kerne unverändert
aufruft und das Ergebnis kopiert, hätte sich auf **nicht garantierte** NRVO
über eine Aufrufkette hinweg verlassen, genau das, was Blocker 2.2 bereits
für `restoreRunPersistenceSnapshot()` ausschließt. Korrektur: neuer,
**echter** In-place-Kern, der direkt in die vom Aufrufer übergebene
`CommandDecision&` schreibt – keine `bool`-Rückgabe als zweiter,
undefinierter Statuskanal (`CommandDecision.status` trägt den fachlichen
Ausgang bereits vollständig):

```cpp
void resultInto(const RunCommandState& current, const CommandEnvelope& envelope,
                 CommandKind kind, CommandStatus status,
                 CommandDecision& destination);
void beginDecisionInto(const RunCommandState& current,
                        const CommandEnvelope& envelope, CommandKind kind,
                        CommandDecision& destination);
void decideProgramStartInto(const RunCommandState& current,
                             const ProgramStartRequest& request,
                             CommandDecision& destination);
void decideManualStartInto(const RunCommandState& current,
                            const ManualStartRequest& request,
                            CommandDecision& destination);
```

**Korrektur gegenüber der Vorfassung (Blocker 2, Runde 6 – real gefundener
Entwurfsfehler im eigenen Runde-5-Reset):** `resultInto()` spezifizierte
bislang `destination = CommandDecision{};` als ersten Schritt. `T{}` ist als
Zuweisungs-RHS ein **prvalue**, dessen Materialisierung zu einem temporären
Objekt **nicht** unter die seit C++17 garantierte Elision fällt (die gilt
nur für Initialisierung, nicht für Zuweisung) – der Compiler darf, muss
aber nicht, das 10.520-B-Ganzobjekt-Temporary wegoptimieren. Dieselbe
Nicht-Elision-Skepsis, die dieser Plan bereits gegen NRVO anwendet
(Abschnitt 12.4.2/4.3 oben), gilt hier identisch. Korrektur nach dem
Reset-Vertrag in 4.9 unten: `resultInto()` setzt **ausschließlich** die
Felder zurück, die von `beginDecision()`/`result()` nicht ohnehin
unbedingt überschrieben werden – `status`/`kind`/`envelope`/`before`/
`after` werden in jedem Aufruf sofort danach mit echten Werten belegt
(real geprüft, `run_commands.cpp:190-195`: `decision.before = current;
decision.after = current;` passiert **unbedingt**, in jedem Aufruf –
diese beiden Felder allein sind >10 KiB und brauchen daher **keinen**
Reset, da sie ohnehin sofort vollständig überschrieben werden):

```cpp
void resultInto(const RunCommandState& current, const CommandEnvelope& envelope,
                 CommandKind kind, CommandStatus status,
                 CommandDecision& destination) {
    destination.startSummary.reset();
    destination.adjustmentPreview.reset();
    destination.effectCount = 0U;   // effects[] selbst bleibt ungeleert;
      // nur [0, effectCount) wird je gelesen/kodiert (identisches Muster
      // wie persistedRunCommandCount/revisionCount, Abschnitt 4.9)
    destination.sensorSelectionApplyStatus.reset();
    destination.sensorSelectionEvent.reset();
    destination.sensorSelectionNotice.reset();
    destination.startSensorSelectionNotice.reset();
    destination.committedControlContextTransition.reset();
    destination.status = status;
    destination.kind = kind;
    destination.envelope = envelope;
    destination.before = current;
    destination.after = current;
}
```

Kein Feld dieser Reset-Sequenz erzeugt ein Ganzobjekt-Temporary in
Budgetnähe – jeder `.reset()`-Aufruf auf einem `std::optional<T>` zerstört
nur einen eventuell vorhandenen Inhalt in-place, ohne ein neues `T`-Objekt
zu materialisieren; `effectCount = 0U` ist eine skalare Zuweisung.
Ohne diesen expliziten Reset auf `destination` könnten bei Wiederverwendung
desselben `decisionTarget`-Puffers über mehrere Aufrufe hinweg veraltete
Felder (`startSummary`, `effects`/`effectCount` u. a.) aus einem
vorherigen Aufruf stehen bleiben. `decideProgramStartInto()`/`decideManualStartInto()` operieren
**durchgängig über eine Referenz auf `destination`** statt über eine
lokale Kopie – mechanisch identisch zum bestehenden Funktionskörper, nur
mit `auto& decision = destination;` statt `auto decision = beginDecision(
...);` als erster Zeile, und jedem `return decision;` durch ein einfaches
`return;` ersetzt (der fachliche Ausgang steht bereits vollständig in
`destination`). **Richtungsumkehr gegenüber Runde 4 (verbindlich):** die
bestehenden Return-by-value-Funktionen `decideProgramStart()`/
`decideManualStart()` bauen jetzt auf dem neuen In-place-Kern auf, nicht
umgekehrt:

```cpp
CommandDecision decideProgramStart(const RunCommandState& current,
                                   const ProgramStartRequest& request) {
    CommandDecision decision;
    decideProgramStartInto(current, request, decision);
    return decision;
}
```

Diese Return-by-value-Wrapper bleiben für Host-/Legacy-Consumer
(`test_run_commands` u. a.) unverändert nutzbar – hier ist ein einzelnes
stack-lokales `CommandDecision` unkritisch, da kein 3584-B-Main-Task-Budget
gilt. Der Produktpfad (Fresh Start, Abschnitt 8) verwendet ausschließlich
die `Into()`-Kerne:

```text
1. decisionTarget = std::unique_ptr<CommandDecision>{
     new (std::nothrow) CommandDecision()}
   if (decisionTarget == nullptr) -> fail-closed, kein Start
2. decideProgramStartInto(..., *decisionTarget)  // oder ...ManualStartInto
   bei !proposed() -> kein Start (bestehende Ablehnungslogik unveraendert)
3. RunPersistenceCoordinator::persistCommand(*runtimeRunState_,
   *decisionTarget, time, liveSensorEvidence)
```

```text
PRODUCT_DECIDE_INTO_CALLS_RETURN_BY_VALUE_DECIDER=NO
PRODUCT_DECIDE_INTO_CALLS_RETURN_BY_VALUE_BEGIN_DECISION=NO
COMMAND_DECISION_OUT_PARAM_ALWAYS_RECEIVES_COMPLETE_STATUS=YES
SECOND_DECISION_STATUS_CHANNEL=NO
```

**4.4 `RunPersistenceWorkingSet` – Coordinator-eigenes Wertmember, ersetzt
Runde 4s `candidateScratch_` (schließt Blocker 2.3 Runde 4, Blocker 1/4/5
Runde 5):** `persistCommand()`, `discardAsNoActiveRun()` und
`activateR1EligibleRun()` benötigen weiterhin ein `candidate`-großes
Scratchobjekt für Write-before-Apply; `writeSnapshotCore()` benötigt
weiterhin ein `RunPersistenceRawRecord`-großes Scratchobjekt (real geprüft,
Zeile 1571 – Runde 4 hatte dies übersehen); die Snapshot-Projektion
benötigt ein `RunPersistenceSnapshot`-großes Scratchobjekt (unverändert:
`WRITE_BEFORE_APPLY=UNCHANGED`, `STORE_SEMANTICS=UNCHANGED`,
`WIRE_FORMAT=UNCHANGED`, `SCHEMA=UNCHANGED`, `RECOVERY_POLICY=UNCHANGED`).
**Korrektur gegenüber Runde 4 (Owner-Vorschlag Runde 5, KISS):** statt drei
getrennter Scratch-Konzepte (Runde 4s `candidateScratch_` plus zwei bislang
ungelöste interne Lokale) ein **einziges** Coordinator-eigenes Wertmember,
das alle drei write-before-apply-Zwischenwerte bündelt:

```cpp
struct RunPersistenceWorkingSet {
    RunCommandState candidate;
    RunPersistenceSnapshot snapshot;
    RunPersistenceRawRecord record;
};

class RunPersistenceCoordinator {
    // ...
    RunPersistenceWorkingSet workingSet_;   // Wertmember, KEINE separate
      // Allokation (s. u.) - fuer persistCommand()/discardAsNoActiveRun()/
      // activateR1EligibleRun()s Write-before-Apply-Arbeitskopie
      // (workingSet_.candidate), die Snapshot-Projektion
      // (workingSet_.snapshot) und writeSnapshotCore()s internen
      // RunPersistenceRawRecord (workingSet_.record)
};
```

**Snapshot-Projektion – In-place-Kern (schließt Blocker 1, Runde 5):**
`makeRunPersistenceSnapshot()` (`run_persistence_contract.hpp:144-148`,
real geprüft) gibt `std::optional<RunPersistenceSnapshot>` per Wert zurück
und wird im Produktpfad (§9.1 Zweig 3 sowie intern in `persistCommand()`/
`discardAsNoActiveRun()`) auf `candidate` angewendet. Exakt gewählte
Out-Parameter-Variante, **nur** vom Produktpfad verwendet:

```cpp
[[nodiscard]] bool makeRunPersistenceSnapshotInto(
    const RunCommandState& state,
    const std::array<CommandId, kMaximumPersistedRunCommandIds>& ids,
    std::size_t idCount, RunCheckpointTrigger trigger,
    const RunCheckpointTime& time, std::uint16_t intervalMinutes,
    RunPersistenceSnapshot& destination);
```

Produktpfad-Aufruf schreibt direkt in `workingSet_.snapshot`:

```cpp
if (!makeRunPersistenceSnapshotInto(workingSet_.candidate, persistedIds_,
        persistedIdCount_, trigger, time, schedule_.intervalMinutes(),
        workingSet_.snapshot))
  -> InvalidDecision, current unveraendert
```

**Zwingende Reset-Semantik für das wiederverwendete Ziel (Blocker 3,
Runde 6):** anders als `resultInto()`s `before`/`after` (4.3) oder
`decideProgramStartInto()`s freisch allozierter `decisionTarget` (der bei
jedem Fresh-Start-Versuch neu `new (std::nothrow)`-alloziert wird, 4.3) ist
`workingSet_.snapshot` ein **über die gesamte Coordinator-Lebensdauer
wiederverwendetes** Member. Der bestehende `makeRunPersistenceSnapshot()`
(real geprüft, `run_persistence_contract.cpp:411-464`) ist dagegen
semantisch sicher, weil jeder Aufruf mit einer **frischen** lokalen
`RunPersistenceSnapshot snapshot;` beginnt und nur die zur jeweiligen
`variant` (`ProgramRun`/`ManualRun`/`NoActiveRun`) gehörenden Felder
befüllt – bei `NoActiveRun` etwa bleibt nur `variant` gesetzt, alle
übrigen variantenspezifischen Felder bleiben beim (dort: frischen)
Default. Ohne Reset könnte eine Sequenz wie `ProgramRun -> NoActiveRun ->
ManualRun -> ProgramRun` auf demselben `workingSet_.snapshot` stale
Felder aus einem früheren Aufruf durchschlagen lassen (`program`,
`activeRunId`, `sensorSelection`, `manual`, `revisions`/`revisionCount`
u. a.). `makeRunPersistenceSnapshotInto()` setzt daher `destination` als
**erste** Zeile über denselben field-by-field-Reset zurück, den
`decodeRunPersistenceSnapshotInto()` (4.6) verwendet (Reset-Vertrag in 4.9
unten) – die anschließende, unveränderte Feldbefüllung nach `variant`
macht das Ergebnis danach bit-für-bit äquivalent zu einer frischen lokalen
Variablen:

```text
SNAPSHOT_INTO_STARTS_FROM_DEFAULT_EQUIVALENT_STATE=YES
SNAPSHOT_VARIANT_SWITCH_LEAVES_NO_STALE_FIELDS=YES
SNAPSHOT_REUSE_IS_SEMANTICALLY_EQUIVALENT_TO_FRESH_LOCAL=YES
```

Die bestehende `makeRunPersistenceSnapshot()` (Return-by-value) bleibt für
Host-/Legacy-Tests unverändert nutzbar und delegiert jetzt an
`makeRunPersistenceSnapshotInto()` (dieselbe Richtungsumkehr-Konvention wie
4.3/4.5/4.6).

```text
PRODUCT_MAKE_SNAPSHOT_LOCAL_4096B=NO
PRODUCT_WRITE_RAW_RECORD_LOCAL_4096B_PLUS=NO
PRODUCT_SNAPSHOT_STORAGE=COORDINATOR_HEAP_WORKING_SET
PRODUCT_RAW_RECORD_STORAGE=COORDINATOR_HEAP_WORKING_SET
WRITE_BEFORE_APPLY=UNCHANGED
WIRE_BYTES=UNCHANGED
PERSISTENCE_TRANSACTION_ORDER=UNCHANGED
FALLBACK_SEMANTICS=UNCHANGED
```

**Keine separate Allokation, kein Konstruktor-Rückgabewert-Widerspruch
(Blocker 5, Runde 5 – Korrektur eines technisch unmöglichen Vertrags aus
Runde 4):** Runde 4 spezifizierte, `candidateScratch_` werde „im
Konstruktor mit `new (std::nothrow)` alloziert; schlägt die Allokation
fehl, gibt der Konstruktor … `nullptr`/einen Fehlerstatus zurück" – ein
Konstruktor hat aber keinen Rückgabewert, dieser Vertrag war nicht
implementierbar. Mit `workingSet_` als **Wertmember** (kein `unique_ptr`,
keine zweite interne Allokation) entfällt das Problem strukturell:
`RunPersistenceCoordinator` selbst wird bereits als Ganzes über
`new (std::nothrow) RunPersistenceCoordinator(...)` durch
`FermentationApplication` alloziert (unverändert gegenüber Runde 4s
Ownership-Tabelle, Abschnitt 12.1/12 Schritt 4) – schlägt diese **eine**
Allokation fehl, wird der gesamte Coordinator (inklusive seines inline
`workingSet_`) gar nicht komponiert, exakt derselbe bereits bestehende
fail-closed-Pfad wie für jedes andere `unique_ptr`-Member in Abschnitt 12.1.
Keine zweite, separate Scratch-Allokation, keine `create()`-Factory nötig.

**Reentrancy – Korrektur einer realen Fehlbegründung aus Runde 4 (Blocker 4,
Runde 5):** Runde 4 behauptete, das Scratch sei „über die bereits
bestehende `state_`-Maschine" geschützt, da `writeSnapshotCore()`
`state_ = Busy` setze. Real geprüft (`run_persistence_coordinator.cpp:
1847`/`:1866`, `persistCommand()`) ist das falsch: `auto candidate =
current;` und `const auto snapshot = makeRunPersistenceSnapshot(...)`
werden **vor** dem Aufruf von `writeSnapshot()`/`writeSnapshotCore()`
konstruiert – `state_` ist zu diesem Zeitpunkt noch `Ready`/`ReadyEmpty`,
nicht `Busy` (`state_ = Busy` steht erst in `writeSnapshotCore()` selbst,
Zeile 1495, **nach** der Candidate-/Snapshot-Konstruktion des Aufrufers).
`state_ == Busy` schützt also die **Store-Schreiboperation**, nicht die
**Konstruktion** von `workingSet_.candidate`/`.snapshot`/`.record`. Der
tatsächlich tragfähige Reentrancy-Vertrag ist struktureller, nicht
zustandsbasierter Natur – explizit gemacht statt implizit behauptet:

```text
RUN_PERSISTENCE_COORDINATOR_THREAD_SAFE=NO
RUN_PERSISTENCE_COORDINATOR_REENTRANT=NO
PRODUCT_OWNER=FermentationApplication
PRODUCT_CALL_AFFINITY=SINGLE_SERIALIZED_APPLICATION_TASK
CONCURRENT_DIRECT_CALLS=FORBIDDEN
```

**Nachweis für #121 (real gegen `main/app_main.cpp` und
`lib/device_platform_esp_idf/src/nvs_state_store.*` geprüft, nicht nur
behauptet):** `app_main()` (`main/app_main.cpp:123-193`, vollständig
gelesen) ist der einzige Einstiegspunkt; nach `application.begin(...)`
läuft ausschließlich eine einzige `for (;;) { platform.update();
application.update(); ...; vTaskDelay(...); }`-Schleife in genau dieser
einen ESP-IDF-Maintask – kein `xTaskCreate`/`xTimerCreate`/
`esp_timer_create` im produktiven `main/`- oder `fermentation_app`-Code
(einzige Fundstelle: `main/issue_29_bringup_probe.cpp:547`, ausschließlich
unter `APP_ISSUE_29_BRINGUP_PROBE` kompiliert, in Release-/#121-Builds
nicht enthalten). `NvsStateStore`/`NvsOwningContext` (real geprüft,
`nvs_state_store.hpp/.cpp`) registrieren keinen Callback, keine ISR, keinen
zweiten Task, der in den Coordinator zurückrufen könnte. `Abschnitt 12.3`
legt zusätzlich fest, dass `FermentationApplication::update()` in dieser
Revision **leer** bleibt und **kein** `checkpointPeriodic()`-Aufruf
existiert – der einzige in dieser Revision tatsächlich produkt-erreichbare
Coordinator-Aufrufpfad ist damit `begin()`s synchrone Bootsequenz selbst
(Abschnitt 12); Fresh Start/Resume/Discard haben (wie bereits an anderer
Stelle dieses Plans dokumentiert, `REAL_RUNTIME_HWM_WITHOUT_REAL_TRIGGER=
NOT_CLAIMED`, Abschnitt 12.2) in dieser actor-free Composition noch keinen
echten Produkttrigger. Die actor-free Composition (Abschnitt 3/12)
verdrahtet also strukturell keinen zweiten Task, keinen Sensor-/Planner-/
Timer-Callback und keinen Store-/Codec-Rückruf, der
`RunPersistenceCoordinator` von außerhalb dieser einen Aufruferkette
erreichen könnte. Kein neuer Lock, keine neue Zustandsmaschine – die
Garantie ist eine **Aufrufkonvention** (dokumentierte Vorbedingung, keine
Laufzeitprüfung). Spätere UI-/Web-Producer (außerhalb dieses Plans) müssen
über denselben serialisierten Application-Aufrufpfad gehen, nicht direkt
aus einer fremden Task in den Coordinator rufen – reine
Dokumentationsvorgabe für spätere Issues, kein Code in dieser Revision.

**Zusätzliches, unabhängig tragfähiges Argument für `workingSet_.record`
speziell (`loadAndInitializeInto()` vs. `writeSnapshotCore()`):** selbst
ohne die obige Single-Task-Argumentation schließen sich diese beiden
Schreiber von `workingSet_.record` bereits über die bestehende
`state_`-Maschine gegenseitig aus – `loadAndInitialize()`/
`loadAndInitializeInto()` verlangt `state_ == Uninitialized` (real geprüft,
Zeile 274-276, sonst `AlreadyInitialized`), während jeder Aufrufer von
`writeSnapshotCore()` `state_ ∈ {Ready, ReadyEmpty, LoadedActiveRun}`
verlangt (Abschnitt 12.4.4 oben) – disjunkte Zustandsmengen. Da `state_`
den Coordinator nach dem ersten `loadAndInitializeInto()`-Aufruf
unwiderruflich aus `Uninitialized` heraus bewegt (kein Reset-Pfad zurück zu
`Uninitialized` existiert, real geprüft), kann `loadAndInitializeInto()`
über die gesamte Coordinator-Lebensdauer nur **einmal** laufen, bevor
`writeSnapshotCore()` überhaupt erstmals aufrufbar wird – dieselbe
Garantie wie die Single-Task-Argumentation, hier aber zusätzlich durch die
Zustandsmaschine selbst erzwungen, nicht nur durch Aufrufkonvention.

**4.5 `loadAndInitializeInto()` – stack-sicherer Load-/Decode-Kern (schließt
Blocker 2, Runde 5):** Real geprüft: `loadAndInitialize()`s interne
`loadReference`-Lambda (`run_persistence_coordinator.cpp:347-386`) gibt
`std::optional<RunPersistenceRawRecord>` per Wert zurück und wird zweimal
aufgerufen (`currentRecord`, Zeile 388; `fallbackRecord`, Zeile 465) – beide
sind lokale, stack-resident konstruierte `RunPersistenceRawRecord`-Werte
(4152 B). Runde 4s Fix (Application-seitiges `unique_ptr<
RunPersistenceLoadResult>`, Abschnitt 4.6 Vorfassung) löste nur die
**äußere** Rückgabe von `loadAndInitialize()`, nicht diese **innere**
Ebene. Korrektur – neue, **einzige** Coordinator-Methode für den
Produktpfad:

```cpp
void RunPersistenceCoordinator::loadAndInitializeInto(
    RunPersistenceLoadResult& destination);
```

Mechanische Transformation (kein Semantikwechsel, dieselbe Validierungs-/
Fehlerreihenfolge wie die bestehende Funktion):

```text
- loadReference() decodiert direkt in workingSet_.record statt einen
  lokalen optional<RunPersistenceRawRecord> zurueckzugeben:
    if (!decodeRunPersistenceRecordInto(bytes, epoch_, workingSet_.record))
      -> Fehlerstatus wie bisher
- Current/Fallback: slots_[slot] = workingSet_.record  // unveraendert:
  bereits vorher eine Kopie in ein bestehendes Heap-Member (slots_ ist
  bereits Coordinator-Instanzmember, Abschnitt "Ausgangslage"), nur die
  Quelle ist jetzt workingSet_.record statt einer lokalen Optional-Variable.
  Bewusst NICHT direkt in slots_[slot] decodiert: runCheckpointReferenceMatches()
  (real geprueft, run_persistence_codec.cpp) validiert den dekodierten
  Record ERST GEGEN die erwartete Reference, NACHDEM decodeRunPersistenceRecordInto()
  zurueckgekehrt ist - ein noch nicht validierter Record darf slots_ (die
  technische Wahrheit des Coordinators) nie erreichen; workingSet_.record
  ist daher die einzige Zwischenablage fuer noch-nicht-validierte Daten.
- jeder bisherige `return {status, snapshot};`/`return {status,
  std::nullopt};` wird zu genau zwei skalaren Feldzuweisungen statt einem
  Ganzobjekt-Aggregat (Korrektur Blocker 4, Runde 6 – s. u.):
  `destination.status = status; destination.snapshot = snapshot; return;`
  (Erfolg) bzw. `destination.status = status; destination.snapshot.reset();
  return;` (Fehler) - **niemals** `destination = {status, ...};`
- state_-Uebergaenge (ReadyEmpty/LoadedActiveRun/BlockedIndeterminate)
  unveraendert an denselben Stellen
```

**Korrektur gegenüber der Vorfassung (Blocker 4, Runde 6):** die Vorfassung
spezifizierte für die Fehlerpfade noch `destination = {status,
std::nullopt};` – dieselbe Ganzobjekt-Temporary-Problematik wie bei
`resultInto()` (4.3) und dem Codec-Reset (4.6), hier für
`sizeof(RunPersistenceLoadResult)=4112 B`. `destination.snapshot.reset()`
zerstört einen eventuell vorhandenen Inhalt in-place, ohne ein neues
`RunPersistenceLoadResult`- oder `RunPersistenceSnapshot`-Objekt zu
materialisieren; `destination.snapshot = snapshot;` (Erfolgsfall) kopiert/
verschiebt direkt aus dem bereits existierenden `snapshot`-Lvalue
(`workingSet_.record.snapshot`) in den bestehenden `optional`-Speicher von
`destination.snapshot` – ebenfalls kein Ganzobjekt-Temporary, da
`optional<T>::operator=(const T&)`/`operator=(T&&)` direkt aus dem
referenzierten Objekt konstruiert, nicht aus einer neu materialisierten
Kopie. Da `loadAndInitializeInto()` in der aktuellen Composition nur genau
einmal pro Coordinator-Lebensdauer läuft (Reentrancy-Nachweis, Abschnitt
12.4.4), betrifft dies zwar keine Wiederverwendung über mehrere Aufrufe
hinweg – der Ganzobjekt-Temporary-Fehler besteht aber unabhängig davon bei
jedem einzelnen Aufruf, da `{status, std::nullopt}` als RHS-Aggregat
unabhängig vom Wiederverwendungskontext ein volles
`RunPersistenceLoadResult` materialisiert.

```text
LOAD_RESULT_REUSE_ALREADY_INITIALIZED_CLEARS_OLD_SNAPSHOT=PASS
LOAD_RESULT_ERROR_NEVER_EXPOSES_STALE_SNAPSHOT=PASS
```

`FermentationApplication::begin()` (Abschnitt 12, Schritt 4) ruft:

```cpp
runPersistenceLoadResult = std::unique_ptr<RunPersistenceLoadResult>{
    new (std::nothrow) RunPersistenceLoadResult()};
if (runPersistenceLoadResult == nullptr) -> fail-closed (SERVICE_REQUIRED)
runPersistenceCoordinator->loadAndInitializeInto(*runPersistenceLoadResult);
```

**Kein** `new (std::nothrow) RunPersistenceLoadResult(coordinator->
loadAndInitialize())` mehr (Runde 4s Muster) – dieses Muster kopierte den
Rückgabewert der (weiterhin intern stack-schweren) `loadAndInitialize()`
exakt einmal in den Heap, löste aber nie die interne `loadReference`-Ebene.
Das Grundprinzip „Ziel zuerst heap-allozieren, dann in-place befüllen"
bleibt unverändert die Randbedingung aus dem bereits ownerreviewten
#119/#120-Stackfix (kein Code-Cherrypick), jetzt konsequent bis in die
Callee-Ebene durchgezogen. Die bestehende `loadAndInitialize()`
(Return-by-value) bleibt für Host-/Legacy-Tests unverändert nutzbar und
delegiert jetzt an `loadAndInitializeInto()` (dieselbe
Richtungsumkehr-Konvention wie 4.3):

```text
RUN_PERSISTENCE_LOAD_RESULT_STORAGE=HEAP_BOOT_TRANSIENT
RUN_PERSISTENCE_LOAD_RESULT_LOCAL_BY_VALUE=NO
PRODUCT_LOAD_RAW_RECORD_LOCAL=NO
PRODUCT_LOAD_RESULT_LOCAL_BY_VALUE=NO
PRODUCT_DECODE_SNAPSHOT_LOCAL=NO
PRODUCT_DECODE_RESULT_LARGE_LOCAL=NO
```

Lebensdauer: boot-transient (lokale `unique_ptr`-Variable in `begin()`,
nicht Application-Lifetime-Member – nach der Klassifikation nicht mehr
benötigt, `RunPersistenceCoordinator::state()` bleibt die technische
Wahrheit, Abschnitt 4.4).

**4.6 `run_persistence_codec.cpp` – rein mechanische In-place-Helfer
(schließt Blocker 3, Runde 5):** Scope-Korrektur gegenüber Runde 4
(Abschnitt 14 sagte bislang „keine Änderung an `run_persistence_codec.cpp`"
– das ist nach 4.5 nicht mehr haltbar, da `decodeRunPersistenceRecordInto()`
dort ergänzt werden muss). Real geprüft (`run_persistence_codec.cpp:1146-
1262`): `decodeRunPersistenceSnapshot()` konstruiert intern
`RunPersistenceSnapshot s;`, befüllt es über die gesamte Funktion
sequentiell per `ByteReader`, und gibt es am Ende per `{Status::Success,
std::move(s)}` zurück – rein mechanisch auf einen Out-Parameter umstellbar,
ohne jede Wire-/Schema-/Validierungsänderung:

```cpp
[[nodiscard]] RunPersistenceCodecStatus decodeRunPersistenceSnapshotInto(
    const std::string& payload, std::uint32_t schemaVersion,
    RunPersistenceSnapshot& destination);
[[nodiscard]] bool decodeRunPersistenceRecordInto(
    const std::string& bytes, device_platform::StorageEpoch epoch,
    RunPersistenceRawRecord& destination);
```

`decodeRunPersistenceSnapshotInto()`: identischer Funktionskörper, `s`
ersetzt durch `destination`. **Korrektur gegenüber der Vorfassung (Blocker
2/4, Runde 6 – real gefundener Entwurfsfehler im eigenen Runde-5-Reset):**
die Vorfassung setzte `destination = RunPersistenceSnapshot{};` als erste
Zeile – dieselbe Ganzobjekt-Temporary-Problematik wie bei `resultInto()`
(4.3): `T{}` als Zuweisungs-RHS ist ein prvalue, dessen Materialisierung zu
einem `sizeof(RunPersistenceSnapshot)=4096`-B-Temporary **nicht** unter die
C++17-Pflichtelision fällt. Korrektur nach dem Reset-Vertrag in 4.9 unten:
`destination` wird als erste Zeile über einen field-by-field-Reset
zurückgesetzt – **eigenständig, nicht identisch mit** dem Reset von
`makeRunPersistenceSnapshotInto()` (4.4): beide Funktionen befüllen zwar
denselben Typ nach demselben `variant`-abhängigen Grundmuster, aber der
Decode-Pfad hat zusätzlich ein Schema-Gate, das
`makeRunPersistenceSnapshotInto()` nicht kennt (real geprüft,
`run_persistence_codec.cpp:1229-1256` vs.
`run_persistence_contract.cpp:411-464`) – `recoveryTemperatureEvidence` und
`recoveryEpisodeRevision` werden von `decodeRunPersistenceSnapshotInto()`
nur bei `schemaVersion >= kRecoveryFieldsIntroducedInSchema` unbedingt
beschrieben, von `makeRunPersistenceSnapshotInto()` dagegen immer. Der
eigenständige Decode-Reset steht in 4.9 unten. Jedes `return {status, std::nullopt}`
wird zu `return status;`, das abschließende `return {Status::Success,
std::move(s)}` wird zu `return RunPersistenceCodecStatus::Success;` (die
Felder stehen bereits vollständig in `destination`). `decodeRunPersistenceRecordInto()`: ruft
`decodeRunPersistenceSnapshotInto(envelope.payload, schemaVersion,
destination.snapshot)` direkt auf `destination.snapshot`, setzt
anschließend `destination.bytes`/`.checkpointRevision`/`.utcUnixSeconds` –
kein zusätzlicher lokaler `RunPersistenceRawRecord`. **Gültigkeitsvertrag
(explizit gefordert, Blocker 4, Runde 6):** `destination` ist **nur bei
`SUCCESS` (Rückgabe `true`) gültig/verbrauchbar** – bei einem fehlgeschlagenen
Decode (Envelope-Mismatch, Schema unbekannt, Storage-Epoch-Mismatch,
`decodeRunPersistenceSnapshotInto()`-Fehlschlag) bleibt `destination`
möglicherweise nur teilweise beschrieben; der Aufrufer (`loadAndInitializeInto()`,
4.5) darf `destination`/`workingSet_.record` in diesem Fall **nie** nach
`slots_` kopieren – die bereits bestehende Reihenfolge (erst `decodeRunPersistenceRecordInto()`
plus `runCheckpointReferenceMatches()`-Validierung, **dann** `slots_[slot]
= workingSet_.record`, 4.5) stellt das strukturell sicher, ohne dass
`decodeRunPersistenceRecordInto()` selbst einen Reset auf einen
Fehlschlagpfad legen muss – der nächste erfolgreiche Aufruf überschreibt
`destination.snapshot` ohnehin vollständig über den field-by-field-Reset
oben, bevor er selbst gelesen wird.

```text
DECODE_SNAPSHOT_INTO_STARTS_FROM_DEFAULT_EQUIVALENT_STATE=YES
DECODE_SNAPSHOT_INTO_FAILURE_RESULT_NOT_CONSUMABLE=YES
DECODE_SNAPSHOT_INTO_NEXT_CALL_HAS_NO_STALE_FIELDS=YES
DECODE_RECORD_INTO_VALID_ONLY_ON_SUCCESS=YES
DECODE_RECORD_INTO_FAILURE_NEVER_UPDATES_SLOTS=YES
```

`encodeRunPersistenceSnapshot()`
nimmt bereits heute `std::string& out` als Out-Parameter entgegen
(`run_persistence_codec.hpp:27`, real geprüft) – die Encode-Richtung war
bereits stack-sicher, nur die Decode-Richtung fehlte.

**Nicht zulässig (unverändert gegenüber der Owner-Vorgabe):**

```text
kein neues Wireformat
keine Feldänderung
keine Schemaänderung
keine Enum-Nummerierung
keine Legacy-Decode-Entfernung
keine geänderte Validierungssemantik
```

```text
RUN_PERSISTENCE_CODEC_WIRE_SEMANTICS=UNCHANGED
RUN_PERSISTENCE_CODEC_SCHEMA=UNCHANGED
RUN_PERSISTENCE_CODEC_BYTES=UNCHANGED
RUN_PERSISTENCE_CODEC_STACK_SAFE_IMPLEMENTATION_HELPERS=IN_SCOPE
```

Pflicht: Byte-for-byte-Regression aller bestehenden Schema-1/2/3-Fixtures
(Abschnitt 13). Die bestehenden Return-by-value-Funktionen
`decodeRunPersistenceSnapshot()`/`decodeRunPersistenceRecord()` bleiben für
Host-/Legacy-Tests unverändert nutzbar und delegieren jetzt an die
`Into()`-Kerne (dieselbe Richtungsumkehr-Konvention wie 4.3/4.5).

**4.7 Statischer Produkt-Stackgate – Pflicht vor jeder Hardwarefreigabe
(schließt Blocker 5 Runde 4, Major 8 Runde 5):** Die bestehende
Instrumentierung (`docs/ISSUE_29_BUILD_REPORT.md`: `-fstack-usage`/
`-fcallgraph-info=su` für die Diagnose-Probe sowie „die tatsächlich
betroffenen `fermentation_app`-Quellen"; `scripts/analyze_issue_29_stack.py`
wertet die `.ci`-Kanten aus) wird auf die in Abschnitt 12.4 (Scope)
benannten #121-Produktpfad-TUs erweitert – **dieselbe** Technik,
**derselbe** Skript-Mechanismus, nicht neu erfunden; Zielpfad wird der
reale Produkt-Callgraph (`FermentationApplication::begin()` etc.) statt
(nur) der Diagnose-Probe. Nach Implementation, **vor** jeder
Hardwarefreigabe (Schritt 8, Abschnitt 15). **Erweiterte Frameliste
gegenüber Runde 4 (Major 8, Runde 5 – deckt jetzt auch die in 4.5/4.6
neu benannten internen Kerne ab):**

```text
CONFIGURED_RELEASE_MAIN_TASK_STACK=3584
APP_MAIN_ENTRY_FRAME=<measured>
FERMENTATION_APPLICATION_BEGIN_FRAME=<measured>
MAKE_RUN_PERSISTENCE_SNAPSHOT_INTO_FRAME=<measured>
DECODE_RUN_PERSISTENCE_SNAPSHOT_INTO_FRAME=<measured>
DECODE_RUN_PERSISTENCE_RECORD_INTO_FRAME=<measured>
RUN_PERSISTENCE_LOAD_AND_INITIALIZE_INTO_FRAME=<measured>
WRITE_SNAPSHOT_CORE_FRAME=<measured>
RESTORE_PRODUCT_PATH_FRAME=<measured>
FRESH_START_DECISION_FRAME=<measured>
DECIDE_PROGRAM_START_INTO_FRAME=<measured>
DECIDE_MANUAL_START_INTO_FRAME=<measured>
RUN_PERSISTENCE_PERSIST_COMMAND_FRAME=<measured>
DISCARD_AS_NO_ACTIVE_RUN_FRAME=<measured>
ACTIVATE_R1_ELIGIBLE_RUN_FRAME=<measured>
PRODUCT_BOOT_CUMULATIVE_STACK_PATH=<measured>
PRODUCT_FRESH_START_CUMULATIVE_STACK_PATH=<measured or NOT_PRODUCT_REACHABLE>
PRODUCT_RESUME_CUMULATIVE_STACK_PATH=<measured or NOT_PRODUCT_REACHABLE>
PRODUCT_DISCARD_CUMULATIVE_STACK_PATH=<measured>
PERSIST_COMMAND_FRAME_BEFORE=9280
PERSIST_COMMAND_FRAME_AFTER=<measured>
```

**Ergänzung Runde 6:** die in Abschnitt 4.9 gewählten Reset-Helfer messen,
falls sie als eigene, nicht-inline Funktionen im Callgraph erscheinen
(andernfalls sind sie Teil des jeweiligen `Into()`-Frames oben):

```text
RESET_COMMAND_DECISION_IN_PLACE_FRAME=<measured or INLINED>
RESET_RUN_PERSISTENCE_SNAPSHOT_IN_PLACE_FRAME=<measured or INLINED>
RESET_RUN_PERSISTENCE_LOAD_RESULT_FRAME=<measured or INLINED>
```

Abnahme:

```text
NO_PRODUCT_REACHABLE_SINGLE_FRAME_EXCEEDS_CONFIGURED_TASK_STACK=PASS
PRODUCT_REACHABLE_STATIC_STACK_GATE=PASS
```

Einzelne `.su`-Frames ersetzen die kumulative Callgraph-Betrachtung nicht
(derselbe Vorbehalt wie im bestehenden Bericht). Bei `dynamic`/`unbounded`-
Qualifier oder fehlender relevanter Callgraph-Kante (dieselbe Konvention
wie `docs/ISSUE_29_BUILD_REPORT.md`):

```text
PRODUCT_REACHABLE_STATIC_STACK_GATE=BLOCKED
STOP – OWNER_REVIEW_REQUIRED
```

Keine Hardware zum Ausprobieren, wenn die statische Evidenz bereits nicht
in das Taskbudget passt.

**4.8 Heap-/Largest-Block-Budget des vergrößerten Coordinators (schließt
Major 9, Runde 5):** `RunPersistenceCoordinator` wächst durch das inline
`workingSet_`-Wertmember (Abschnitt 4.4) um `candidate` (5096 B) +
`snapshot` (4096 B) + `record` (4152 B) ≈ 13,3 KiB gegenüber dem bereits
real gemessenen `sizeof(RunPersistenceCoordinator)=8968` B – **Heap**, kein
Stack (Abschnitt "Ausgangslage"), aber real relevant für den bereits aus
#120 bekannten Heap-/Fragmentierungs-Randbedingungswert:

```text
BOOT_HEAP_BEFORE_COMPOSITION ~= 280 KB          -- #120-Randbedingung,
BOOT_HEAP_LARGEST_BLOCK_BEFORE_COMPOSITION = 147456 B  -- kein #121-PASS-Beweis
```

Diese #120-Zahlen sind **keine** #121-Abnahmeevidenz – sie stammen aus
einer anderen Composition (Abschnitt 2). Nach Implementation, vor
Hardwarefreigabe zusätzlich zu messen (Software-/Build-Ebene, real ESP32,
nicht Host):

```text
SIZEOF_RUN_PERSISTENCE_COORDINATOR=<measured ESP32>
SIZEOF_RUN_PERSISTENCE_WORKING_SET=<measured ESP32>
STATIC_DRAM_DELTA=<measured>
```

Beim später ownerfreigegebenen actor-free Hardwareboot (Schritt 8,
Abschnitt 15) zusätzlich real zu messen:

```text
FREE_HEAP_BEFORE_APPLICATION_COMPOSITION=<measured>
LARGEST_BLOCK_BEFORE_COORDINATOR_ALLOCATION=<measured>
FREE_HEAP_AFTER_APPLICATION_COMPOSITION=<measured>
LARGEST_BLOCK_AFTER_APPLICATION_COMPOSITION=<measured>
```

Kein neuer Local-Min-Heap-Monitor (bereits in einer früheren Runde
verworfen, bleibt verworfen) – reine Boot-Zeit-Momentaufnahme, keine
laufende Überwachung.

**4.9 Reset-Vertrag für wiederverwendete In-place-Ziele (schließt
Blocker 2/3/4, Runde 6):** Grundregel, verbindlich für **jede**
produkt-erreichbare `Into()`-API dieses Plans:

```text
PRODUCT_INPLACE_RESET_WHOLE_OBJECT_TEMPORARY=NO
PRODUCT_INPLACE_RESET_RELIES_ON_COPY_ELISION=NO
PRODUCT_INPLACE_RESET_RELIES_ON_NRVO=NO
```

**Warum `destination = T{};`/`destination = {...};` nicht zulässig sind:**
seit C++17 ist Kopierelision nur für **Initialisierung** aus einem prvalue
garantiert (Rückgabewerte, Kopie-Initialisierung) – nicht für **Zuweisung**.
`destination = T{};` materialisiert `T{}` als temporäres Objekt (Temporary
Materialization), bevor `operator=` darauf zugreift; ob der Compiler dieses
Temporary danach wegoptimiert, ist eine reine Optimierungsfrage, keine
Standardgarantie – dieselbe Nicht-Elision-Skepsis, die dieser Plan bereits
gegen NRVO bei Return-by-value-Pfaden anwendet (Abschnitt 12.4.2/4.3), gilt
hier identisch für Zuweisung.

**Gewählte Technik: field-by-field Reset, kein TU-privater
Rekonstruktionshelfer.** Für alle drei betroffenen großen Typen gilt
dasselbe reale Muster (an den echten Schreibfunktionen nachgewiesen, nicht
angenommen): nur die Felder, die **nicht** in jedem Aufrufpfad ohnehin
unbedingt überschrieben werden, brauchen einen Reset – Felder, die jeder
Aufruf sofort danach mit einem echten Wert belegt, brauchen **keinen**
Reset (identisches Prinzip wie bei `CommandDecision.before`/`.after`,
Abschnitt 12.4.3).

- **`CommandDecision`** (`resultInto()`, Abschnitt 12.4.3): reset
  `startSummary`, `adjustmentPreview`, `effectCount = 0U` (das
  `effects`-Array selbst, `std::array<CommandEffect, 6>` ≈ 6 Byte, bleibt
  ungeleert – nur `[0, effectCount)` wird je gelesen/kodiert, identisches
  Gating-Muster wie unten), `sensorSelectionApplyStatus`,
  `sensorSelectionEvent`, `sensorSelectionNotice`,
  `startSensorSelectionNotice`, `committedControlContextTransition`.
  `status`/`kind`/`envelope`/`before`/`after` brauchen **keinen** Reset –
  jeder Aufruf von `resultInto()` überschreibt sie unbedingt (real geprüft,
  `run_commands.cpp:190-195`). Diese acht Reset- plus fünf
  Unbedingt-Felder (das `effects`-Array eingerechnet) erschöpfen alle 14
  Member von `CommandDecision` (`run_commands.hpp:385-419`) – kein Feld
  bleibt unklassifiziert, die Wiederverwendung ist damit provably
  äquivalent zu einer frischen lokalen Instanz, nicht nur „die Felder, an
  die wir gedacht haben".
- **`RunPersistenceSnapshot` – `makeRunPersistenceSnapshotInto()`**
  (Abschnitt 12.4.4): reset `activeRunId.clear()`,
  `activeRunSensorMode.reset()`, `sensorSelection.reset()`,
  `program.reset()`, `revisionCount = 0U` (das `revisions`-Array,
  `std::array<RunRevision, 32>`, bleibt ungeleert – nur `[0,
  revisionCount)` wird je gelesen/kodiert, real geprüft:
  `encodeRunPersistenceSnapshot()` iteriert exakt bis `revisionCount`,
  `run_persistence_codec.cpp:1109`), `manual.reset()`,
  `processRunSnapshot.reset()`, `pendingRecoveryAnchor.reset()`,
  `recoveryBootAnchorMonotonicMillis.reset()`,
  `lastRecoveryEpisodeEvidence.reset()`, `priorBootPhaseElapsed.reset()`,
  `nominalRecoveryAdjustment.reset()`, `runProgress = RunProgressState{}`
  (klein – `RunProgressBasis`-Enum + `uint32_t` + ein
  `optional<WeightedProgressState>`, weit unter jeder Budgetnähe,
  unproblematisch als direkte Feldzuweisung). `variant`, `trigger`,
  `checkpointMonotonicMillis`, `intervalMinutes`, `processState`,
  `runRevision`, `persistedRunCommandIds`, `persistedRunCommandCount`,
  `recoveryTemperatureEvidence`, `recoveryEpisodeRevision` brauchen
  **keinen** Reset – real geprüft (`run_persistence_contract.cpp:411-464`),
  jeder Aufruf von `makeRunPersistenceSnapshot()`/
  `makeRunPersistenceSnapshotInto()` überschreibt sie unbedingt,
  unabhängig von `variant`. Diese 13 Reset- plus 10 Unbedingt-Felder (die
  beiden Array/Count-Paare `revisions`/`revisionCount` und
  `persistedRunCommandIds`/`persistedRunCommandCount` je als ein Member
  gezählt) erschöpfen alle 24 Member von `RunPersistenceSnapshot`
  (`run_persistence_contract.hpp:88-126`) – kein Feld bleibt
  unklassifiziert.
- **`RunPersistenceSnapshot` – `decodeRunPersistenceSnapshotInto()`**
  (Abschnitt 12.4.6, **eigene Liste, nicht identisch mit der Make-Liste
  oben** – Korrektur gegenüber der Vorfassung, die beide Listen
  fälschlich gleichgesetzt hatte): real geprüft
  (`run_persistence_codec.cpp:1152-1256`) unterscheidet sich der
  Decode-Reset in genau zwei Feldern von der Make-Liste, weil der
  Decode-Pfad ein zusätzliches Schema-Gate hat, das `make()` nicht kennt –
  `recoveryTemperatureEvidence.lastKnown` (`CrossRoleEvidence{}`, klein,
  direkte Feldzuweisung unproblematisch) und `recoveryEpisodeRevision =
  0U` werden nur bei `schemaVersion >=
  kRecoveryFieldsIntroducedInSchema` unbedingt beschrieben (Zeile
  1229-1244) – bei einem Schema-1/2-Payload (jeder `variant`, auch
  `NoActiveRun`) bleiben beide Felder beim Decode **vollständig
  unberührt**. Ohne Reset würde ein wiederverwendetes `destination` einen
  stale Wert aus einem vorherigen Aufruf unbemerkt durchreichen – **und
  `validateRunPersistenceSnapshotForSchema()` fängt das nicht ab**: real
  geprüft, `noActiveRunHasNoRecoveryFields()`
  (`run_persistence_contract.cpp:105-112`) prüft `recoveryEpisodeRevision`
  und `recoveryTemperatureEvidence` überhaupt nicht (Letzteres bewusst,
  s. Kommentar `run_persistence_contract.hpp:114-117` –
  laufunabhängige, fortlaufend aktualisierte Evidenz; Ersteres eine reale
  Validierungslücke, die dieser Reset schließt, ohne die Validierung
  selbst zu ändern). Zusätzlich reset (identisch zur Make-Liste, aus
  demselben `variant`-Gating-Grund): `activeRunSensorMode.reset()`,
  `sensorSelection.reset()`, `processRunSnapshot.reset()` (alle drei nur
  bei `variant != NoActiveRun` beschrieben, Zeile 1160/1215),
  `program.reset()`, `revisionCount = 0U` (nur bei `ProgramRun`, Zeile
  1190), `manual.reset()` (nur bei `ManualRun`, Zeile 1209),
  `pendingRecoveryAnchor.reset()`, `recoveryBootAnchorMonotonicMillis.
  reset()`, `lastRecoveryEpisodeEvidence.reset()`,
  `priorBootPhaseElapsed.reset()`, `nominalRecoveryAdjustment.reset()`
  (alle fünf nur beim Schema-Gate oben beschrieben), sowie zwingend
  **unbedingt vor jedem Feldpfad** `runProgress = RunProgressState{}`:
  der Nicht-Schema-Gate-Zweig (Zeile 1245-1256) setzt bei
  `variant != NoActiveRun` nur `basis`/`weightedProgress`, nie
  `observedRunSeconds`, und bei `variant == NoActiveRun` gar nichts – ein
  reiner Schema-Gate-Reset wie bei den übrigen sieben Feldern reicht hier
  nicht aus. `variant`, `trigger`, `checkpointMonotonicMillis`,
  `intervalMinutes`, `runRevision`, `activeRunId`, `processState`,
  `persistedRunCommandCount`/`persistedRunCommandIds` brauchen **keinen**
  Reset – real geprüft (Zeile 1152-1228), jeder Decode-Aufruf überschreibt
  sie unbedingt vor jedem `return`, unabhängig von `variant`/Schema.
- **`RunPersistenceLoadResult`** (`loadAndInitializeInto()`, Abschnitt
  12.4.5): zwei skalare Feldzuweisungen statt Aggregat-Zuweisung –
  `destination.status = status; destination.snapshot.reset();` (Fehlerfall)
  bzw. `destination.status = status; destination.snapshot = snapshot;`
  (Erfolgsfall, kopiert/verschiebt direkt aus dem existierenden
  `snapshot`-Lvalue in den bestehenden `optional`-Speicher, kein
  Ganzobjekt-Temporary).
- **`RunPersistenceRawRecord`** (`decodeRunPersistenceRecordInto()`,
  Abschnitt 12.4.6): kein separater Reset nötig – `destination.snapshot`
  wird direkt per `decodeRunPersistenceSnapshotInto()` in-place befüllt
  (bereits selbst korrekt zurückgesetzt), `destination.bytes`/
  `.checkpointRevision`/`.utcUnixSeconds` werden bei jedem erfolgreichen
  Aufruf unbedingt überschrieben; bei Fehlschlag ist `destination` gemäß
  explizitem Gültigkeitsvertrag nicht konsumierbar (Abschnitt 12.4.6).
- **`RunCommandState`** (Ziel eines wiederverwendeten `Into`-Helfers):
  betrifft in dieser Revision **keinen** Produktpfad – `target` in den
  `restoreRunPersistenceSnapshotInto()`-Aufrufstellen (Abschnitt 9/9.1) ist
  in jedem Fall ein frisch `new (std::nothrow)`-alloziertes, damit
  bereits leeres Ziel (real geprüft, Abschnitt 12.4.2), kein über mehrere
  Aufrufe wiederverwendetes Member. Sollte ein späteres Issue
  `restoreRunPersistenceSnapshotInto()` auf ein wiederverwendetes Ziel
  anwenden, gilt derselbe field-by-field-Grundsatz.

Kein `memset` auf einen der genannten nichttrivialen C++-Typen (STL-
Container/`std::string`/`std::optional` haben keine trivial-kopierbare
Repräsentation, die `memset` sicher zurücksetzen könnte). Kein TU-privater
In-place-Rekonstruktionshelfer gewählt, da keiner der Typen ihn benötigt –
jeder Reset besteht ausschließlich aus `.reset()`/`.clear()`-Aufrufen auf
`std::optional`/`std::string` (die nur einen eventuell vorhandenen Inhalt
in-place zerstören, ohne ein neues Objekt zu materialisieren) und
skalaren Feldzuweisungen.

```text
SNAPSHOT_REUSE_PROGRAM_TO_NO_ACTIVE_CLEARS_PROGRAM_FIELDS=PASS
SNAPSHOT_REUSE_PROGRAM_TO_MANUAL_CLEARS_PROGRAM_FIELDS=PASS
SNAPSHOT_REUSE_MANUAL_TO_PROGRAM_CLEARS_MANUAL_FIELDS=PASS
SNAPSHOT_REUSE_AFTER_FAILED_PROJECTION_IS_CLEAN_ON_NEXT_SUCCESS=PASS
COMMAND_DECISION_REUSE_NO_STALE_OPTIONALS=PASS
COMMAND_DECISION_REUSE_NO_STALE_EFFECTS=PASS
COMMAND_DECISION_REUSE_NO_WHOLE_OBJECT_TEMPORARY=PASS
COMMAND_DECISION_RESET_LIST_EXHAUSTS_ALL_STRUCT_MEMBERS=PASS
SNAPSHOT_MAKE_RESET_LIST_EXHAUSTS_ALL_STRUCT_MEMBERS=PASS
DECODE_RESET_LIST_DIFFERS_FROM_MAKE_RESET_LIST=YES
DECODE_LEGACY_SCHEMA_REUSE_CLEARS_RECOVERY_EPISODE_REVISION=PASS
DECODE_LEGACY_SCHEMA_REUSE_CLEARS_RECOVERY_TEMPERATURE_EVIDENCE=PASS
DECODE_LEGACY_SCHEMA_REUSE_CLEARS_RUN_PROGRESS_OBSERVED_SECONDS=PASS
LOAD_RESULT_REUSE_ALREADY_INITIALIZED_CLEARS_OLD_SNAPSHOT=PASS
```

**4.10 Allokationsvertrag präzisiert – nur die äußere Objektallokation ist
`nothrow` (schließt Blocker 6, Runde 6):** Der bisherige Vertrag
(`FAIL_CLOSED_ALLOCATION_USES_NOTHROW=YES`,
`ALLOCATION_FAILURE_CAN_ABORT_OR_THROW=NO`, Abschnitt 12.4.1) galt implizit
pauschal. Real geprüft enthalten die produktiv betroffenen Typen
dynamisch allozierende STL-Unterobjekte: `RunCommandState.activeRunId`
(`std::string`), `RunCommandState.activeProgramRun` (`optional<ActiveRun>`
→ `ProgramDocument` mit `std::string id/name/notes` und
`std::vector<FermentationStage>`), analog `RunPersistenceSnapshot.
activeRunId`/`.program`. Operationen wie `workingSet_.candidate = current;`,
`workingSet_.snapshot.program = ...;`, `slots_[slot] = workingSet_.record;`
oder `destination.before = current;` können daher **intern** normale
(werfende) `std::string`-/`std::vector`-Allokationen auslösen – `new
(std::nothrow) RunPersistenceCoordinator(...)` macht nur die **eine**
explizite Objektallokation für den Coordinator selbst `nothrow`, nicht
diese späteren, verschachtelten STL-Allokationen; ebenso deckt `new
(std::nothrow) T(args...)` nur die Speicherallokation für `T` ab, nicht
einen eventuell werfenden Konstruktor von `T`. Kein neues Allocator-/PMR-/
Container-Redesign in #121 – der Vertrag wird stattdessen ehrlich
verengt:

```text
EXPLICIT_ISSUE121_OBJECT_ALLOCATION_USES_NOTHROW=YES
EXPLICIT_COMPOSITION_ALLOCATION_FAILURE_FAILS_CLOSED=YES

NESTED_STL_ALLOCATION_SEMANTICS=PREEXISTING_STANDARD_LIBRARY_BEHAVIOR
GRACEFUL_NESTED_STL_OOM_RECOVERY=NOT_CLAIMED
ISSUE121_CUSTOM_ALLOCATOR_OR_PMR=NO
ISSUE121_CONTAINER_REDESIGN=NO

SOFTWARE_ACTUATION_PERMISSION_BEFORE_PRODUCT_COPY_PERSIST_PATH=DENIED
PHYSICAL_OUTPUT_SAFETY_ON_ABORT_OR_RESET=REMAINS_ISSUE29_GATE
```

`ALLOCATION_FAILURE_CAN_ABORT_OR_THROW=NO` entfällt als pauschale Aussage
– sie gilt nur für die explizit benannten `new (std::nothrow)`-Grenzen
(die eine Coordinator-Objektallokation, `runtimeRunState_`/
`pendingResume_`/`decisionTarget`/`runPersistenceLoadResult`, Abschnitt
9/9.1/12.4.3/12.4.5), nicht für verschachtelte STL-Kopien innerhalb bereits
konstruierter Objekte – dasselbe bereits bestehende, unveränderte
Standardbibliotheksverhalten wie auf `BASE_SHA` (`RunCommandState` wird
dort ebenfalls per normaler Kopie/Zuweisung verwendet, ohne graceful-OOM-
Anspruch). Build-/Konfigurationsnachweis (ohne Config-Änderung):

```text
CONFIG_COMPILER_CXX_EXCEPTIONS=<captured>
CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS=<captured>
```

Falls die exakten sdkconfig-Symbolnamen unter der real eingesetzten
ESP-IDF-Version anders lauten als hier angenommen, werden die tatsächlich
generierten Namen dokumentiert, nicht die hier verwendeten Platzhalter
stillschweigend übernommen. Keine Toolchain-/Heap-Policy-Änderung ohne
neues Owner-Gate. Die Heap-/Largest-Block-Gates aus Abschnitt 4.8 bleiben
wichtig – gerade **weil** graceful OOM für verschachtelte STL-Kopien nicht
behauptet wird, ist die Kenntnis des verfügbaren Headrooms die einzige
reale Absicherung gegen diesen Fall.

```text
EXPLICIT_ISSUE121_OBJECT_ALLOCATION_USES_NOTHROW=PASS
GRACEFUL_NESTED_STL_OOM_RECOVERY_NOT_CLAIMED=PASS
```

## 13. Teststrategie

Aufbauend auf bestehender Testinfrastruktur (Abschnitt 4).

- `test/test_safety_core` → `test/test_actuation_interlock`: migriert auf
  `ActuationEvidence`/`ActuationEvaluation` (ohne `SafetyDisposition`,
  Blocker 7).
- **Neu:** `test/test_boot_classification` – Matrix über alle Flows aus
  Abschnitt 7, inklusive `FallbackRecovered → SafeBoot` (Abschnitt 7.3,
  Pflichttest `R1_FALLBACK_RECOVERED_NEVER_RESUME=PASS`) und der drei
  Legacy-Wire-Werte im geladenen Snapshot.
- `test/test_process_state_machine`: unverändert lauffähig.
- `test/test_run_persistence_coordinator`: neue Testfälle für
  `activateR1EligibleRun()` (alle drei eligible Phasen, Fehlerfall
  außerhalb der Phasenliste, Fehlerfall `state_ != LoadedActiveRun`).
- `test/test_temperature_control_orchestrator` (bzw. bestehende
  Orchestrator-Tests, sofern vorhanden): migriert auf die neue
  `tickActuatorPlan(current, now, currentGate)`-Signatur und den
  5-Parameter-Konstruktor (Abschnitt 4.9).

Zusätzlich verpflichtend (Blocker 13, vollständig übernommen):

```text
R1_FALLBACK_RECOVERED_NEVER_RESUME=PASS

RESUME_OFFER_SURVIVES_BEGIN_RETURN=PASS
RESUME_CONFIRM_HAS_VALID_PENDING_STATE=PASS
RESUME_REJECT_CLEARS_PENDING_STATE=PASS

R1_RESUME_DOES_NOT_CREATE_RECOVERY_EVALUATION=PASS
R1_RESUME_DOES_NOT_CREATE_C2_RECOVERY_FIELDS=PASS
R1_RESUME_REQUIRES_FRESH_EVIDENCE_AND_APPLIED=PASS

STATELESS_INTERLOCK_HAS_NO_STALE_GATE=PASS
ORCHESTRATOR_CURRENT_GATE_HANDOFF=PASS

APPLICATION_PARTIAL_COMPOSITION_FAILURE_RAII=PASS
CONFIG_DEPENDENCY_LIFETIMES=PASS

ISSUE121_PRODUCT_COMPOSITION_ACTOR_FREE=PASS
NO_PRODUCT_ACTUATOR_SINK_REQUIRED=PASS
NO_PRODUCT_ACTUATOR_PARAMETERS_REQUIRED=PASS

LEGACY_BOOT_SAFEBOOT_RECOVERYEVALUATION_DECODE=PASS
NO_WIRE_ENUM_RENUMBERING=PASS

R1_ACTIVATE_ELIGIBLE_FAULT_BRANCH_RAM_ONLY_UNCHANGED=PASS
R1_ACTIVATE_ELIGIBLE_COMPLETED_BRANCH_RAM_ONLY_UNCHANGED=PASS
R1_ACTIVATE_ELIGIBLE_RESUME_BRANCH_WRITE_BEFORE_APPLY=PASS
R1_ACTIVATE_ELIGIBLE_WRITE_FAILURE_LEAVES_CURRENT_UNCHANGED=PASS
R1_ACTIVATE_ELIGIBLE_USES_STANDARD_FALLBACK_NOT_CLEAR=PASS
R1_ACTIVATE_ELIGIBLE_NO_TRANSITION_SEQUENCE_INCREMENT=PASS
R1_ACTIVATE_ELIGIBLE_REUSES_REBASED_RECOVERED_STATE=PASS

R1_RESUME_SENSOR_REVALIDATION_BLOCKS_WITHOUT_STORE_MUTATION=PASS
R1_RESUME_SENSOR_REVALIDATION_REUSES_RECOVERY_PROGRAM_CONTEXT_HELPER=PASS
R1_RESUME_SENSOR_REVALIDATION_GATE_MATCHES_C2_PERMISSION_CHECK=PASS

R1_PUBLISHED_PROCESS_STATE_IS_PURE_PROJECTION=PASS
R1_NO_PUBLICATION_BEFORE_RUNTIME_RUN_STATE_WRITE=PASS

R1_TERMINAL_RUN_FAULT_NO_BOOT_TO_FAULT_PROPOSE=PASS
R1_COMPLETED_RUN_VIA_ACTIVATE_ELIGIBLE_NOT_PROPOSE=PASS

R1_STORAGE_EPOCH_FROM_ACQUIRE_RUNTIME_LEASE=PASS
R1_LEASE_DENIED_SKIPS_COORDINATOR_CONSTRUCTION_SERVICE_REQUIRED=PASS

R1_SENSOR_EVIDENCE_NOT_REQUIRED_DURING_UNCONFIRMED_RESUME_OFFER=PASS
R1_SENSOR_EVIDENCE_STILL_REQUIRED_ON_RESUME_CONFIRM=PASS

ESP_TIME_ZONE_RESOLVER_EUROPE_ZURICH_SUCCESS=PASS
ESP_TIME_ZONE_RESOLVER_UNKNOWN_IDENTIFIER_UNSUPPORTED=PASS
ESP_TIME_ZONE_RESOLVER_NO_NETWORK_NO_OS_DEPENDENCY=PASS
ESP_TIME_ZONE_RESOLVER_NO_SETENV_TZ=PASS
ESP_TIME_ZONE_RESOLVER_NO_TZSET=PASS
ESP_TIME_ZONE_RESOLVER_NO_SYSTEM_TIME_MUTATION=PASS

FERMENTATION_APP_DEPENDS_ON_DEVICE_PLATFORM_ESP_IDF=NO
FERMENTATION_APP_CMAKE_REQUIRES_DEVICE_PLATFORM_ONLY=PASS
ARCHITECTURE_CHECKER_UNCHANGED=PASS

ESP_TIME_ZONE_RESOLVER_OWNER=ESP_IDF_COMPOSITION_ROOT
ESP_TIME_ZONE_RESOLVER_ROOT_VALUE_STORAGE=PASS
ESP_TIME_ZONE_RESOLVER_OUTLIVES_APPLICATION=PASS

FERMENTATION_APPLICATION_USES_ABSTRACT_TIME_ZONE_PORT=PASS
CONFIGURATION_SERVICE_RECEIVES_INJECTED_ITIMEZONERESOLVER=PASS
ESP32_PRODUCT_BEGIN_REQUIRES_STORE_AND_TIME_ZONE_PORT=PASS

NATIVE_SMOKE_OVERLOAD_PRESERVED=PASS
NATIVE_SMOKE_USES_DEVICE_PLATFORM_ESP_IDF=NO
NATIVE_COMPOSITION_ROOT_HAS_NO_TEST_SUPPORT_DEPENDENCY=PASS
NATIVE_COMPOSITION_ROOT_HAS_NO_ESP_IDF_DEPENDENCY=PASS

BOOT_PROOF_NO_WRITE_PATH_PRODUCES_BOOT_STATE=PASS

RUNTIME_RUN_STATE_HEAP_OWNED_NOT_INLINE=PASS
PENDING_RESUME_HEAP_OWNED_NOT_INLINE=PASS
RUNTIME_RUN_STATE_ALLOCATION_FAILURE_FAIL_CLOSED=PASS
PENDING_RESUME_ALLOCATION_FAILURE_FAIL_CLOSED=PASS

R1_RESUME_CLEAN_WRITE_FAILURE_CAN_RETRY_ONLY_IF_STORE_UNCHANGED=PASS
R1_RESUME_INDETERMINATE_WRITE_ENTERS_SERVICE_REQUIRED=PASS
R1_RESUME_INDETERMINATE_WRITE_CANNOT_RETRY_CONFIRM=PASS
R1_RESUME_FAILED_WRITE_NEVER_ACTUATES=PASS

R1_FAULT_RESTORE_WITHOUT_SENSOR_CONTEXT=PASS
R1_COMPLETED_RESTORE_WITHOUT_SENSOR_CONTEXT=PASS
R1_ACTIVE_RESUME_WITHOUT_SENSOR_CONTEXT_FAILS_CLOSED=PASS

LEGACY_PROCESS_STATE_ENUM_DECODE=PASS
LEGACY_BOOT_SNAPSHOT_REJECTED_AS_INVALID=PASS
LEGACY_SAFEBOOT_SNAPSHOT_REJECTED_AS_INVALID=PASS
LEGACY_RECOVERY_EVALUATION_POLICY_DISCARDABLE_RUN=PASS

DISCARDABLE_RUN_HAS_VALID_RESTORED_CURRENT=PASS
DISCARDABLE_RUN_APPLIED_PUBLISHES_STANDBY=PASS
DISCARDABLE_RUN_WRITE_FAILURE_DOES_NOT_PUBLISH_STANDBY=PASS

FRESH_START_USES_CANONICAL_RUNTIME_RUN_STATE=PASS
FRESH_START_WRITE_FAILURE_LEAVES_RUNTIME_STATE_UNAPPLIED=PASS

NO_PRODUCT_REACHABLE_RUNCOMMANDSTATE_STACK_COPY=PASS
NO_PRODUCT_REACHABLE_COMMANDDECISION_STACK_OBJECT=PASS
PRODUCT_RESTORE_IN_PLACE_HEAP=PASS
RUN_PERSISTENCE_LOAD_RESULT_HEAP_BOOT_TRANSIENT=PASS
NOTHROW_ALLOCATION_FAILURE_FAILS_CLOSED=PASS
MAKE_UNIQUE_NOT_USED_FOR_FAIL_CLOSED_ALLOCATION=PASS
PERSIST_COMMAND_FRAME_BEFORE=9280
PERSIST_COMMAND_FRAME_AFTER=<measured>
PRODUCT_REACHABLE_STATIC_STACK_GATE=PASS
PRODUCT_BOOT_CUMULATIVE_STACK_GATE=PASS

INTERLOCK_HAS_NO_PERSISTENCE_SNAPSHOT_INPUT=PASS
INTERLOCK_HAS_NO_FALLBACK_RECOVERY_TRUST_EXCEPTION=PASS
FALLBACK_RECOVERED_ALWAYS_DENIED=PASS

APPLICATION_ALLOCATION_FAILURE_NOT_REPORTED_AS_PERSISTENCE_UNTRUSTED=PASS

FAULT_RESTORE_PRODUCT_PATH_STACK_SAFE=PASS
COMPLETED_RESTORE_PRODUCT_PATH_STACK_SAFE=PASS
FAULT_COMPLETED_RESTORE_ALLOCATION_FAILURE_FAILS_CLOSED=PASS

WATCHDOG_RESET_REEVALUATES_FRESH_EVIDENCE=PASS
WATCHDOG_RESET_REJECTS_OTHER_CURRENT_FAULT=PASS
WATCHDOG_RESET_HAS_NO_INTERLOCK_LATCH_TO_CLEAR=PASS

MAKE_SNAPSHOT_PRODUCT_PATH_HAS_NO_LARGE_LOCAL=PASS
WRITE_SNAPSHOT_CORE_HAS_NO_RAW_RECORD_LARGE_LOCAL=PASS

LOAD_AND_INITIALIZE_PRODUCT_PATH_HAS_NO_RAW_RECORD_LOCAL=PASS
DECODE_SNAPSHOT_PRODUCT_PATH_WRITES_INTO_HEAP_TARGET=PASS
DECODE_RECORD_PRODUCT_PATH_WRITES_INTO_HEAP_TARGET=PASS

SCHEMA1_CODEC_BYTE_REGRESSION=PASS
SCHEMA2_CODEC_BYTE_REGRESSION=PASS
SCHEMA3_CODEC_BYTE_REGRESSION=PASS
WIRE_FORMAT_UNCHANGED=PASS

DECIDE_PROGRAM_START_INTO_NO_LARGE_INTERNAL_DECISION=PASS
DECIDE_MANUAL_START_INTO_NO_LARGE_INTERNAL_DECISION=PASS
LEGACY_DECIDERS_DELEGATE_TO_INPLACE_CORE=PASS

RUN_PERSISTENCE_COORDINATOR_SINGLE_TASK_CONTRACT=PASS
NO_REENTRANT_PRODUCT_COORDINATOR_CALL=PASS

INPLACE_RESET_HAS_NO_WHOLE_LARGE_OBJECT_TEMPORARY=PASS

COMMAND_DECISION_REUSE_NO_STALE_FIELDS=PASS
COMMAND_DECISION_REUSE_NO_STALE_EFFECTS=PASS

SNAPSHOT_REUSE_PROGRAM_TO_NO_ACTIVE_CLEARS_PROGRAM_FIELDS=PASS
SNAPSHOT_REUSE_PROGRAM_TO_MANUAL_CLEARS_PROGRAM_FIELDS=PASS
SNAPSHOT_REUSE_MANUAL_TO_PROGRAM_CLEARS_MANUAL_FIELDS=PASS
SNAPSHOT_REUSE_AFTER_FAILED_PROJECTION_IS_CLEAN_ON_NEXT_SUCCESS=PASS

DECODE_SNAPSHOT_REUSE_NO_STALE_FIELDS=PASS
DECODE_SNAPSHOT_FAILURE_NOT_CONSUMED=PASS
DECODE_RECORD_FAILURE_NEVER_UPDATES_SLOTS=PASS
DECODE_LEGACY_SCHEMA_REUSE_CLEARS_RECOVERY_EPISODE_REVISION=PASS
DECODE_LEGACY_SCHEMA_REUSE_CLEARS_RECOVERY_TEMPERATURE_EVIDENCE=PASS
DECODE_LEGACY_SCHEMA_REUSE_CLEARS_RUN_PROGRESS_OBSERVED_SECONDS=PASS

LOAD_RESULT_REUSE_ALREADY_INITIALIZED_CLEARS_OLD_SNAPSHOT=PASS
LOAD_RESULT_ERROR_NEVER_EXPOSES_STALE_SNAPSHOT=PASS

EXPLICIT_ISSUE121_OBJECT_ALLOCATION_USES_NOTHROW=PASS
GRACEFUL_NESTED_STL_OOM_RECOVERY_NOT_CLAIMED=PASS
```

Pflichttest für die `CommandDecision`-Wiederverwendung (Blocker 2/Major 7):
denselben `decisionTarget`-Puffer nacheinander für mindestens einen
erfolgreichen Start, einen abgelehnten Start und einen zweiten,
unterschiedlichen erfolgreichen/abgelehnten Start wiederverwenden und
beweisen, dass keine Felder aus Entscheidung N in N+1 durchschlagen.

**Neuer Pflichttest für die `decodeRunPersistenceSnapshotInto()`-
Wiederverwendung über einen Schema-Wechsel (Blocker 2/3, Runde 6 – real
gefundene Lücke, nicht aus dem Korrekturbrief direkt, sondern beim
Verifizieren von dessen Blocker 3):** denselben `destination`-Puffer zuerst
mit einem Schema-3-`ProgramRun`-Payload befüllen, dessen
`recoveryEpisodeRevision` und `recoveryTemperatureEvidence` real ungleich
dem Default sind, danach denselben Puffer mit einem gültigen Schema-1- oder
Schema-2-`NoActiveRun`-Payload erneut decodieren und beweisen, dass
`recoveryEpisodeRevision == 0U`, `recoveryTemperatureEvidence` auf
`CrossRoleEvidence{}` zurückgesetzt und `runProgress.observedRunSeconds ==
0U` ist – **nicht** über `validateRunPersistenceSnapshotForSchema()`
prüfbar, da `noActiveRunHasNoRecoveryFields()`
(`run_persistence_contract.cpp:105-112`) `recoveryEpisodeRevision` und
`recoveryTemperatureEvidence` nicht kennt und ein stale Wert die
Validierung sonst unbemerkt passieren würde.

Bestehende relevante native Suiten vollständig weiterführen. Beide
ESP-IDF-Profile bleiben Pflicht. Realer Hardwareboot erst nach
vollständigem Software-Ownerreview.

## 14. Scope-Abgrenzung

### In Scope

Composition-Lücke schließen (`IStateStore`/`ITimeZoneResolver` als abstrakte
`device_platform`-Ports bis `FermentationApplication` durchreichen,
konkrete ESP-IDF-Adapter bleiben in der ESP-IDF-Composition-Root,
actor-free); `BootClassification`
extrahieren (inkl. `FallbackRecovered`-Korrektur); `SafetyCore` →
`ActuationInterlock` verkleinern und umbenennen; `PresentationState`
einführen; neue kleine `RunPersistenceCoordinator::activateR1EligibleRun()`;
`NvsOwningContext::store()`; neuer `EspTimeZoneResolver`-Adapter; zugehörige
Tests migrieren/ergänzen. **Ergänzt (Blocker 7, Runde 5):** rein mechanische
In-place-/Out-Parameter-Stack-Sicherheits-Helfer in `run_commands.hpp/.cpp`,
`run_persistence_contract.hpp/.cpp`, `run_persistence_coordinator.hpp/.cpp`
und `run_persistence_codec.hpp/.cpp` (Abschnitt 12.4) – ohne Wire-/Schema-/
Semantikänderung, siehe Abgrenzung unten.

### Nicht Scope

```text
- ESP-IDF-Upgrade
- neues NVS-Backend
- neues Wireformat / Schema 4
- Loeschen alter Schema-1/2/3-Decoder
- C2-/#18-Codebereinigung aus aesthetischen Gruenden
- RunRecoveryCoordinator-Neuentwurf
- #106-Produktivintegration
- reale Aktorfreigabe
- GPIO-/BTS-/Peltier-Inbetriebnahme
- UI-/WLAN-Redesign
- allgemeines App-Framework
- generische Multi-App-Recovery-Plattform
- produktive Composition von ActuatorPlanner/ActuatorPlanSinkDriver/
  TemperatureController/TargetQualificationEvaluator/
  TemperatureControlApplicationOrchestrator (Blocker 5/11; erfundene
  Produktparameter vor #35/#106 verboten)
```

**Korrektur gegenüber Runde 4 (Blocker 7, Runde 5):** die Vorfassung
behauptete „keine Änderung an `run_persistence_codec.cpp`" und
„`RunPersistenceCoordinator` (außer der einen neuen Methode)" – das ist mit
dem vollständigen Stack-Sicherheits-Vertrag (Abschnitt 12.4) nicht mehr
haltbar. Richtig: `run_persistence_codec.cpp` erhält
`decodeRunPersistenceSnapshotInto()`/`decodeRunPersistenceRecordInto()`
(Abschnitt 12.4.6); `RunPersistenceCoordinator` erhält zusätzlich zu
`activateR1EligibleRun()` das `workingSet_`-Wertmember und
`loadAndInitializeInto()` (Abschnitt 12.4.4/4.5); `run_commands.cpp`
erhält `beginDecisionInto()`/`decideProgramStartInto()`/
`decideManualStartInto()` (Abschnitt 12.4.3); `run_persistence_contract.cpp`
erhält `restoreRunPersistenceSnapshotInto()`/`makeRunPersistenceSnapshotInto()`
(Abschnitt 12.4.2/4.4). In jedem Fall ausschließlich mechanische
In-place-/Out-Parameter-Helfer ohne Wire-/Schema-/Semantikänderung, keine
neue öffentliche Fachlogik. Unverändert **keine** Änderung an
`ActuatorPlanner`, `ActuatorPlanSinkDriver`, `ConfigurationRecoveryService`,
`ConfigurationService`, `process_state_machine.hpp/.cpp`.

## 15. Umsetzungsschritte (owner-gated, keine Umsetzung in dieser Runde)

```text
Schritt 1: boot_classification.hpp/.cpp extrahieren (inkl. FallbackRecovered-
  Korrektur, Abschnitt 7.3), zugehoerige Tests migrieren/ergaenzen.
Schritt 2: RunPersistenceCoordinator::activateR1EligibleRun() ergaenzen
  (Abschnitt 9.1), Tests ergaenzen.
Schritt 3: safety_core.hpp/.cpp -> actuation_interlock.hpp/.cpp umbenennen
  und verkleinern (Abschnitt 10.2), PresentationState einfuehren,
  TemperatureControlApplicationOrchestrator::tickActuatorPlan()-Signatur
  anpassen (Abschnitt 4.9), alle Referenzstellen mechanisch anpassen.
Schritt 4: NvsOwningContext::store() + EspTimeZoneResolver-Adapter
  ergaenzen (Abschnitt 12.2).
Schritt 5: Stack-Sicherheits-Vertrag umsetzen (Abschnitt 12.4, Runde
  4+5+6): restoreRunPersistenceSnapshotInto()/beginDecisionInto()/
  decideProgramStartInto()/decideManualStartInto()/
  makeRunPersistenceSnapshotInto()/decodeRunPersistenceSnapshotInto()/
  decodeRunPersistenceRecordInto()/loadAndInitializeInto() als neue,
  durchgaengig In-place arbeitende Kerne (bestehende Return-by-value-APIs
  bleiben fuer Host-/Legacy-Tests unveraendert und delegieren jetzt an
  diese Kerne, Abschnitt 12.4.3/4.5/4.6). Coordinator-eigenes
  RunPersistenceWorkingSet-Wertmember workingSet_ (candidate/snapshot/
  record, Abschnitt 12.4.4 – ersetzt Runde 4s separates
  candidateScratch_, keine zweite Allokation), Application-eigenes
  decisionTarget (Fresh-Start-Pfad, Abschnitt 12.4.3), RunPersistenceLoadResult
  boot-transient heap-alloziert in begin() (Abschnitt 12.4.5). Jeder
  wiederverwendete In-place-Kern folgt dem field-by-field-Reset-Vertrag aus
  Abschnitt 12.4.9 (kein Ganzobjekt-Temporary via `destination = T{};`).
  Alle expliziten Objektallokationen ausschliesslich ueber
  new (std::nothrow), keine make_unique-Nutzung fuer fail-closed-Pfade;
  verschachtelte STL-Allokationen folgen dem verengten Vertrag aus
  Abschnitt 12.4.10 (kein neuer Allocator/PMR).
Schritt 6: main/app_main.cpp komponiert das konkrete
  `EspTimeZoneResolver`-Value-Objekt in der ESP-IDF-Composition-Root und
  injiziert `IStateStore&` plus abstrakte `ITimeZoneResolver&` in den
  vollständigen Produkt-Overload von `FermentationApplication::begin()`;
  die actor-free Application-Composition bleibt ausschließlich gegen
  `device_platform`-Ports, der Native-Smoke-Overload bleibt erhalten, und
  die Member-Deklarationsreihenfolge fuer die Application-eigenen
  `ConfigurationService`-Dependencies (Abschnitt 12.1) wird korrekt umgesetzt.
Schritt 7: Teststrategie vollstaendig umsetzen (Abschnitt 13), inklusive
  Byte-for-byte-Regression aller Schema-1/2/3-Fixtures gegen die neuen
  Codec-In-place-Kerne (Abschnitt 12.4.6).
Schritt 8: Gate-Reihenfolge exakt wie Abschnitt 16 (Major 9, Runde 6)
  einhalten, kein optionaler Nachweis: (1) statischer Produkt-Stackgate
  (Abschnitt 12.4.7) PASS, (2) reale ESP-IDF-Builds/statische
  Ressourcen-Gates PASS, (3) Owner-Hardwarefreigabe, (4) actor-free realer
  Hardwareboot, (5) Main-Task-Stack-HWM waehrend dieses Laufs messen, (6)
  Heap-/Largest-Block-Momentaufnahmen (Abschnitt 12.4.8) waehrend
  desselben Laufs messen, (7) Owner-Abnahme. Kein Hardwarelauf vor
  Schritt (3) – ein reales HWM kann per Definition erst waehrend eines
  Hardwarelaufs gemessen werden, nicht davor.
```

Jeder Schritt erfordert eine eigene Owner-Freigabe vor Beginn.

## 16. Abnahmekriterien

```text
FAIL_CLOSED_BOOT_POLICY=<PASS|FAIL>
PRODUCT_BOOT_COMPLETION=<PASS|FAIL|BLOCKED>
PHYSICAL_BOOT_OUTPUT_SAFETY=PENDING          -- bleibt #29-Gate, unberuehrt
ARCHITECTURE_BOUNDARIES=<PASS|FAIL>
CONFIGURATION_RECOVERY_TEST=<PASS|FAIL>
RUN_PERSISTENCE_TEST=<PASS|FAIL>
ACTUATION_INTERLOCK_TEST=<PASS|FAIL>
BOOT_CLASSIFICATION_TEST=<PASS|FAIL>
ESP_IDF_BRINGUP_BUILD=<PASS|FAIL>
ESP_IDF_RELEASE_BUILD=<PASS|FAIL>
APP_MAIN_ENTRY_FRAME_UNCHANGED=<PASS|FAIL>
APP_MAIN_ENTRY_FRAME=<measured, real release ELF>
SIZEOF_FERMENTATION_APPLICATION=<measured>
FERMENTATION_APPLICATION_INLINE_LARGE_RUN_STATE=NONE
MAIN_TASK_STACK_PRIMARY_INCREASE=NO
MAIN_TASK_STACK_HIGH_WATERMARK=<PASS|FAIL|NOT_MEASURED>
KNOWN_MAIN_TASK_STACK_CONFLICT=RESOLVED_IN_PLAN
PERSIST_COMMAND_FRAME_BEFORE=9280
PERSIST_COMMAND_FRAME_AFTER=<measured>
PRODUCT_REACHABLE_HEAVY_BY_VALUE_PATHS=CLOSED

PRODUCT_RUNCOMMANDSTATE_LOCAL=NO
PRODUCT_COMMANDDECISION_LOCAL=NO
PRODUCT_RUNPERSISTENCESNAPSHOT_LOCAL=NO
PRODUCT_RUNPERSISTENCERAWRECORD_LOCAL=NO
PRODUCT_RUNPERSISTENCELOADRESULT_LOCAL=NO

SNAPSHOT_PRODUCT_PATH=IN_PLACE
RESTORE_PRODUCT_PATH=IN_PLACE
DECODE_PRODUCT_PATH=IN_PLACE
DECISION_PRODUCT_PATH=IN_PLACE

PRODUCT_INPLACE_RESET_WHOLE_OBJECT_TEMPORARY=NO
PRODUCT_INPLACE_RESET_RELIES_ON_COPY_ELISION=NO
PRODUCT_INPLACE_RESET_RELIES_ON_NRVO=NO
SNAPSHOT_REUSE_IS_SEMANTICALLY_EQUIVALENT_TO_FRESH_LOCAL=YES
COMMAND_DECISION_REUSE_NO_STALE_FIELDS=YES
LOAD_RESULT_ERROR_NEVER_EXPOSES_STALE_SNAPSHOT=YES

RUN_PERSISTENCE_LOAD_RESULT_STORAGE=HEAP_BOOT_TRANSIENT
COORDINATOR_WORKING_SET_STORAGE=VALUE_MEMBER_INLINE_NO_SEPARATE_OBJECT_ALLOCATION
COORDINATOR_THREAD_SAFE=NO
COORDINATOR_REENTRANT=NO
COORDINATOR_PRODUCT_CALL_AFFINITY=SINGLE_SERIALIZED_APPLICATION_TASK

RUN_PERSISTENCE_CODEC_WIRE_SEMANTICS=UNCHANGED
RUN_PERSISTENCE_CODEC_STACK_SAFE_HELPERS=IN_SCOPE
OLD_CODEC_NO_DIFF_TEXT=NONE

EXPLICIT_ISSUE121_OBJECT_ALLOCATION_USES_NOTHROW=YES
NESTED_STL_ALLOCATION_SEMANTICS=PREEXISTING_STANDARD_LIBRARY_BEHAVIOR
GRACEFUL_NESTED_STL_OOM_RECOVERY=NOT_CLAIMED
ISSUE121_CUSTOM_ALLOCATOR_OR_PMR=NO
ISSUE121_CONTAINER_REDESIGN=NO

INTERLOCK_FALLBACK_TRUST_EXCEPTION=REMOVED
APPLICATION_OOM_DIAGNOSTIC=NON_PERSISTENCE
WATCHDOG_RESET_STATELESS_CONTRACT=CLOSED
PRODUCT_REACHABLE_STATIC_STACK_GATE=MANDATORY_BEFORE_HARDWARE_RUN_AUTHORIZATION
MAIN_TASK_STACK_HIGH_WATERMARK=MANDATORY_DURING_AUTHORIZED_HARDWARE_RUN
REAL_RUNTIME_HWM_WITHOUT_REAL_TRIGGER=NOT_CLAIMED

SIZEOF_RUN_PERSISTENCE_COORDINATOR_BEFORE=<measured ESP32>   -- Host-Build-Indikativwert 8968 B, Delta nur ESP32-vs-ESP32 aussagekraeftig
SIZEOF_RUN_PERSISTENCE_COORDINATOR_AFTER=<measured ESP32>
COORDINATOR_HEAP_ALLOCATION_LARGEST_BLOCK_GATE=NOT_RUN_BEFORE_HARDWARE_OR_PASS_FAIL_DURING_AUTHORIZED_RUN

PLAN_INTERNAL_CONFLICT=NONE
OLD_STACK_PATH_TEXT=NONE
OLD_OOM_ATTRIBUTION_TEXT=NONE
BREAKING_PERSISTENCE_CHANGE=NO
SCHEMA_MIGRATION_REQUIRED=NO
ISSUE121_PRODUCT_COMPOSITION_ACTOR_FREE=YES
REAL_HARDWARE_BOOT=NOT_RUN                    -- eigenes Owner-Gate, Schritt 8
```

`SIZEOF_FERMENTATION_APPLICATION`/`APP_MAIN_ENTRY_FRAME` werden aus dem
realen Release-ELF gemessen (Abschnitt 12.2), nicht geschätzt.

**Korrektur der Gate-Reihenfolge (Major 9, Runde 6):** die Vorfassung
formulierte `MAIN_TASK_STACK_HIGH_WATERMARK` und
`PRODUCT_REACHABLE_STATIC_STACK_GATE` sinngemäß als gleichrangige
Pflichtgates „vor jeder Hardwarefreigabe" – ein reales HWM kann aber per
Definition erst **während** eines Hardwarelaufs gemessen werden, nicht
davor. Exakte, zeitlich korrekte Reihenfolge:

```text
1. PRODUCT_REACHABLE_STATIC_STACK_GATE=PASS
2. ESP-IDF-Builds/statische Ressourcen-Gates=PASS
3. OWNER_HARDWARE_RUN_AUTHORIZATION
4. actor-free realer Hardwareboot
5. MAIN_TASK_STACK_HIGH_WATERMARK messen
6. Heap-/Largest-Block-Momentaufnahmen messen (Abschnitt 12.4.8)
7. OWNER_HARDWARE_ACCEPTANCE
```

```text
STATIC_STACK_GATE=MANDATORY_BEFORE_HARDWARE_RUN_AUTHORIZATION
MAIN_TASK_STACK_HIGH_WATERMARK=MANDATORY_DURING_AUTHORIZED_HARDWARE_RUN
COORDINATOR_HEAP_ALLOCATION_LARGEST_BLOCK_GATE=NOT_RUN_BEFORE_HARDWARE_OR_PASS_FAIL_DURING_AUTHORIZED_RUN
```

Kein Hardwarelauf vor Punkt 3 – `PRODUCT_REACHABLE_STATIC_STACK_GATE`
bleibt das eine echte **Vor**-Hardware-Pflichtgate, nicht optionale
Beobachtung – das vormals als „bewusst nicht gelöstes
Restrisiko" geführte Coordinator-Stackproblem (Runde 3) ist mit dem
vollständigen Stack-Sicherheits-Vertrag (Abschnitt 12.4, Runde 4+5) als
`KNOWN_MAIN_TASK_STACK_CONFLICT=RESOLVED_IN_PLAN` geschlossen. **Präzisierung
(unverändert gegenüber Runde 4):** „RESOLVED_IN_PLAN" heißt, dass der
**Entwurf** jeden real identifizierten schweren By-Value-Pfad schließt –
jetzt sowohl auf der Aufrufer-Ebene (`CommandDecision`-Rückgabe,
`RunCommandState`-Rückgabe, `RunPersistenceLoadResult`-Lokalwert) als auch
auf der zuvor übersehenen Callee-Ebene (Coordinator-interner
`RunPersistenceRawRecord`, `loadReference()`s interne Optionals,
`decodeRunPersistenceSnapshot()`s interne lokale Snapshot-Variable,
Abschnitt 12.4, Runde 5); das gemessene `PERSIST_COMMAND_FRAME_BEFORE=9280`
war der By-Value-Zustand vor diesem Plan. Es gibt bewusst noch **kein**
`PERSIST_COMMAND_FRAME_AFTER`-Ist-Wert – der bleibt bis zur Implementation
`<measured>` (Abschnitt 12.4.7) und wird ausschließlich durch den
`PRODUCT_REACHABLE_STATIC_STACK_GATE` real bewiesen, nicht durch diesen
Plantext behauptet. Ein `FAIL` dieser Gates in der Implementation wäre
danach ein realer Implementationsfehler gegen einen bereits im Entwurf
geschlossenen Plan, keine weiterhin offene Architekturfrage – aber die
Zahl selbst ist erst nach dem Gate bewiesen, nicht schon jetzt.

## 17. Statuszusammenfassung

```text
PR122_HEAD=<exact, nach Commit>
PLAN_PATH=docs/tasks/issue-121-lifecycle-safety-simplification-plan.md
PLAN_REVISION=R1
PLAN_SHA=<exact, nach Commit>

ARCHITECTURE_VERDICT=SIMPLIFY
PRIOR_APPROVED_PLAN_SHA=e249b51cedf6f6a3edbce3a0889c48d77b79e828
PLAN_CORRECTION_REASON=FERMENTATION_APP_MUST_NOT_DEPEND_ON_DEVICE_PLATFORM_ESP_IDF
ARCHITECTURE_BOUNDARY_CHANGE=NO
FERMENTATION_APP_DEPENDS_ON_DEVICE_PLATFORM_ESP_IDF=NO
ESP_TIME_ZONE_RESOLVER_OWNER=ESP_IDF_COMPOSITION_ROOT
FERMENTATION_APPLICATION_TIME_ZONE_PORT=ITimeZoneResolver
FERMENTATION_APP_CMAKE_CHANGE=NO
ARCHITECTURE_CHECKER_CHANGE=NO
ADR_013_CHANGE=NO
NATIVE_SMOKE_OVERLOAD=PRESERVED

RESUME_OFFER_OWNERSHIP=CLOSED
R1_RESUME_PATH=CLOSED_NO_C2
FALLBACK_RECOVERED_R1_POLICY=SERVICE_REQUIRED_NO_RESUME
STATELESS_INTERLOCK_HANDOFF=CLOSED
PRODUCT_COMPOSITION_ACTOR_FREE=YES
CONFIG_LIFETIME_GRAPH=CLOSED
BOOT_PROCESS_BOUNDARY=CLOSED
RUNTIME_APPLICATION_LIFECYCLE=CLOSED

KNOWN_MAIN_TASK_STACK_CONFLICT=RESOLVED_IN_PLAN
PERSIST_COMMAND_FRAME_BEFORE=9280
CONFIGURED_RELEASE_MAIN_TASK_STACK=3584
PRODUCT_REACHABLE_HEAVY_BY_VALUE_PATHS=CLOSED

PRODUCT_RUNCOMMANDSTATE_LOCAL=NO
PRODUCT_COMMANDDECISION_LOCAL=NO
PRODUCT_RUNPERSISTENCESNAPSHOT_LOCAL=NO
PRODUCT_RUNPERSISTENCERAWRECORD_LOCAL=NO
PRODUCT_RUNPERSISTENCELOADRESULT_LOCAL=NO

SNAPSHOT_PRODUCT_PATH=IN_PLACE
RESTORE_PRODUCT_PATH=IN_PLACE
DECODE_PRODUCT_PATH=IN_PLACE
DECISION_PRODUCT_PATH=IN_PLACE

PRODUCT_INPLACE_RESET_WHOLE_OBJECT_TEMPORARY=NO
SNAPSHOT_REUSE_IS_SEMANTICALLY_EQUIVALENT_TO_FRESH_LOCAL=YES
COMMAND_DECISION_REUSE_NO_STALE_FIELDS=YES
COMMAND_DECISION_RESET_LIST_EXHAUSTS_ALL_STRUCT_MEMBERS=YES
SNAPSHOT_MAKE_RESET_LIST_EXHAUSTS_ALL_STRUCT_MEMBERS=YES
DECODE_RESET_LIST_DIFFERS_FROM_MAKE_RESET_LIST=YES
LOAD_RESULT_ERROR_NEVER_EXPOSES_STALE_SNAPSHOT=YES

COORDINATOR_WORKING_SET_STORAGE=VALUE_MEMBER_INLINE_NO_SEPARATE_OBJECT_ALLOCATION
COORDINATOR_THREAD_SAFE=NO
COORDINATOR_REENTRANT=NO
COORDINATOR_PRODUCT_CALL_AFFINITY=SINGLE_SERIALIZED_APPLICATION_TASK

RUN_PERSISTENCE_CODEC_WIRE_SEMANTICS=UNCHANGED
RUN_PERSISTENCE_CODEC_STACK_SAFE_HELPERS=IN_SCOPE
OLD_CODEC_NO_DIFF_TEXT=NONE

EXPLICIT_ISSUE121_OBJECT_ALLOCATION_USES_NOTHROW=YES
NESTED_STL_ALLOCATION_SEMANTICS=PREEXISTING_STANDARD_LIBRARY_BEHAVIOR
GRACEFUL_NESTED_STL_OOM_RECOVERY=NOT_CLAIMED
ISSUE121_CUSTOM_ALLOCATOR_OR_PMR=NO
ISSUE121_CONTAINER_REDESIGN=NO

INTERLOCK_FALLBACK_TRUST_EXCEPTION=REMOVED
APPLICATION_OOM_DIAGNOSTIC=NON_PERSISTENCE
WATCHDOG_RESET_STATELESS_CONTRACT=CLOSED
MAIN_TASK_STACK_PRIMARY_INCREASE=NO
PRODUCT_REACHABLE_STATIC_STACK_GATE=MANDATORY_BEFORE_HARDWARE_RUN_AUTHORIZATION
MAIN_TASK_STACK_HIGH_WATERMARK=MANDATORY_DURING_AUTHORIZED_HARDWARE_RUN
REAL_RUNTIME_HWM_WITHOUT_REAL_TRIGGER=NOT_CLAIMED

PLAN_INTERNAL_CONFLICT=NONE
OLD_STACK_PATH_TEXT=NONE
OLD_OOM_ATTRIBUTION_TEXT=NONE
SOURCE_OF_TRUTH_CONFLICT=NONE

BREAKING_PERSISTENCE_CHANGE=NO
SCHEMA_MIGRATION_REQUIRED=NO

IMPLEMENTATION=NOT_STARTED
IMPLEMENTATION_STEP_6=NOT_STARTED
STEP_6_SOURCE_SHA=NOT_CREATED
IMPLEMENTATION_STEP_7=NOT_STARTED
MATERIAL_ARCHITECTURE_DECISION_OPEN=NO
OWNER_PLAN_REVIEW=PENDING
```

STOP – Owner Full Review der neuen exakten Plan-SHA. Keine Implementation.
