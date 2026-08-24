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

### Korrekturrunde (diese Revision)

Der Owner hat die erste Fassung dieser Plandatei (SHA `ea4f057`) mit einem
Korrekturauftrag zurückgewiesen: 6 Blocker, 5 Major-Befunde, 1 Minor-Befund,
Teststrategie- und Source-of-Truth-Ergänzungen. Alle Befunde wurden gegen den
realen Code auf `BASE_SHA` nachverifiziert (siehe Abschnitt 4, neu ergänzte
Unterpunkte) und sind in dieser Fassung **in-place** korrigiert, nicht als
Anhang neben dem alten Text. Es gibt keine R2-Planrevision; diese Datei
bleibt „R1", konsolidiert.

```text
ARCHITECTURE_AUDIT=COMPLETED
ARCHITECTURE_AUDIT_OWNER_REVIEW=PASS_WITH_CORRECTIONS
ARCHITECTURE_AUDIT_SOURCE_TEXT=NOT_PERSISTED_NOT_RECOVERABLE
ARCHITECTURE_VERDICT=SIMPLIFY
PLAN_BASIS=FRESH_CODE_INVENTORY_PLUS_OWNER_CORRECTIONS_A_J
PRIOR_PLAN_SHA=ea4f05723bdcf78fd6e081484ef6ab0cb28f1bf6
PRIOR_PLAN_REVIEW=CORRECTION_REQUIRED
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
Konstruktor, Abschnitt 4.9 – das würde Blocker 11 verletzen). Der
Orchestrator bleibt vollständig unangetastet nutzbar und bleibt Ziel künftiger
Issues (#33/#106); native/Integrationstests dürfen ihn weiterhin mit
bestehenden Mock-Parametern prüfen (Abschnitt 13). `FermentationApplication`
ruft für Fresh Start und R1-Resume `RunPersistenceCoordinator` **direkt**
auf, nicht über den Orchestrator (Abschnitt 8/9).

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
werden für `NoRun`/`ResumeRejected`/`CompletedRun`/`TerminalRunFault`
(Abschnitt 7, dort `from`/`to` bereits durch `validBootTopology()`/
`RecoveryRejected` gedeckt oder als reine Erstpublikation ohne
Topologieprüfung nötig) weiterhin verwendet.

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
`propose()`. Für **jeden anderen** Zustand mit `stateUsesRunSnapshot(...)`
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
festgehalten (Abschnitt 12.2).

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
| Device/Application Lifecycle | `FermentationApplication` (erweitert) | `INITIALIZING` / `READY` / `SERVICE_REQUIRED`; Komposition der übrigen vier |
| Boot Classification | `boot_classification.hpp/.cpp` (neu, frei Funktion) | Klassifikation eines geladenen Snapshots + Konfigurationsvertrauen zu genau einem der Boot-Flow-Ergebnisse (Abschnitt 7) |
| Process Lifecycle | `ProcessStateMachine` (unverändert) | Laufzustand eines aktiven Laufs (Preheating…Fault); keine Boot-/Service-Anteile mehr aktiv angesteuert |
| Persistence Technical Result | `RunPersistenceCoordinator` (+ 1 neue kleine R1-Methode, Abschnitt 9) | technische Speicherwahrheit, `RunPersistenceLoadResult`, `RunPersistenceResult` |
| Actuation Permission | `ActuationInterlock` (umbenannt/verkleinert aus `SafetyCore`) | `DENIED`/`ALLOWED` für den `ActuatorPlanner`-Eingang (Autorität etabliert, physischer Consumer folgt in späteren Issues, Abschnitt 3) |
| Fault/Diagnostic Presentation | `PresentationState` (Abschnitt 11) | UI-/Diagnoseanzeige, nie gate-relevant |

## 6. Legacy-/Wire-Vertrag

`run_persistence_codec.cpp` bleibt **vollständig unverändert** – encode und
decode beider Richtungen bleiben exakt wie auf `BASE_SHA`. Es gibt keinen
einzigen Codezeilen-Diff in dieser Datei. **Klarstellung dieser Revision:**
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

**Klassifikation beim Lesen für 1/2/13:** Werden von der neuen
`BootClassification` nie mehr aktiv geschrieben. Ein alter Snapshot mit
`processState.state ∈ {Boot, SafeBoot, RecoveryEvaluation}` klassifiziert
bereits heute unverändert über `isR1ResumeEligible()` (Abschnitt 9) als
nicht resumefähig → `BootClassification::DiscardableRun`. Kein neuer Code
für diesen Fall nötig.

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
| `NoRun` | `READY` | published: `Standby` (direkt via `propose()`+`TransitionReason::BootCompleted`, deckt sich mit `validBootTopology()`) | keine | `DENIED` (bis Fresh Start) | keine |
| `ResumeOffer` | `READY` | **nicht published** | Snapshot bleibt technisch in `RunPersistenceCoordinator` (`LoadedActiveRun`); rekonstruierter `RunCommandState` lebt **in `FermentationApplication`** (Abschnitt 9, Blocker 1) | `DENIED` | ResumeOffer-Anzeige mit Snapshot-Vorschau |
| `ResumeConfirmed` | `READY` | published nach neuer `RunPersistenceCoordinator::activateR1EligibleRun()` (Abschnitt 9, **neu**, kein `activateLoadedRun()`) | `activateR1EligibleRun()` | `DENIED` bis frische Interlock-Evidenz nach `Applied` | Lauf läuft weiter |
| `ResumeRejected` | `READY` | published: `Standby` nach Discard | `RunPersistenceCoordinator::discardAsNoActiveRun()` (write-before-apply, bestehend) | `DENIED` | Verworfen-Hinweis optional |
| `DiscardableRun` | `READY` | published: `Standby` nach direktem `discardAsNoActiveRun()`-Aufruf (Application ruft Coordinator direkt, nicht über den Orchestrator, Abschnitt 3) | `discardAsNoActiveRun()` | `DENIED` | keine (stiller Discard, wie heute) |
| `CompletedRun` | `READY` | published: `Completed` (direkt via `propose()`, `validBootTopology()` deckt `Boot→Completed`; hier: Erstpublikation ohne vorherigen `Boot`-Zustand, siehe Abschnitt 7.2) | keine Mutation | `DENIED` | Abschluss-Anzeige |
| `TerminalRunFault` | `READY` | published: `Fault` (direkt via `propose()`) | keine Mutation | `DENIED` | Fehler-Anzeige |
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
2. **Nie persistiert:** `RunPersistenceSnapshot.processState` wird nur bei
   `variant != NoActiveRun` geschrieben (bestehender Codec-/Contract-Vertrag,
   `run_persistence_contract.hpp`); ein default-konstruierter
   `RunCommandState`/`ProcessRuntimeState` entsteht in keinem Schreibpfad
   dieses Plans.
3. **Nie Permission:** `ActuationEvidence` (Abschnitt 10) enthält keinen
   `ProcessState`-Rohwert; `activationEvidenceComplete()`-artige Prüfungen
   hängen an `activationKind`/`processActivationApplied`/
   `activationPersistenceResult`, nicht am konkreten `ProcessState`-Wert.

Die erste tatsächliche Veröffentlichung geschieht ausschließlich über die
bestehenden Exportfunktionen `propose()`/`applyProcessTransition()` (für
`NoRun`/`ResumeRejected`/`CompletedRun`/`TerminalRunFault`) bzw. über die
direkte Feldsetzung im neuen `activateR1EligibleRun()`-Pfad (Abschnitt 9,
mirrored aus dem bestehenden `Completed`-Präzedenzfall in
`activateLoadedRun()`).

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
  -> Nutzer Start-Kommando
  -> Application prueft CommandKind ∈ {StartProgram, StartManualHolding}
     (dieselbe Regel wie TemperatureControlApplicationOrchestrator::
     persistFreshStartCommand(), hier direkt in FermentationApplication
     angewendet statt ueber den – in dieser Revision nicht komponierten –
     Orchestrator, Abschnitt 4.9)
  -> RunPersistenceCoordinator::persistCommand(current, decision, time,
     liveSensorEvidence)  // bestehende API, #17 Write-before-Apply
     unveraendert
  -> RunPersistenceResultStatus::Applied
  -> ActuationInterlock::evaluate(evidence) mit
     activationKind=FreshStart, explicitActivationRequested=true,
     processActivationApplied=true, activationPersistenceResult=Applied
  -> erst danach ALLOWED moeglich (physischer Consumer folgt spaeter,
     Abschnitt 3)
```

Kein neuer Persistenzcode; nur die direkte Verdrahtung (Application ruft
`RunPersistenceCoordinator::persistCommand` und danach `ActuationInterlock::
evaluate` mit den realen Post-Commit-Feldern) ist neu.

## 9. Resume (Ownership-/Lifetime-Vertrag, Korrektur F, Blocker 1/2/3 geschlossen)

```text
RunPersistenceCoordinator::loadAndInitialize()
  -> RunPersistenceLoadResult{status, snapshot}
  -> boot_classification::classify(status, snapshot)  // Abschnitt 7.3:
     FallbackRecovered -> SafeBoot, sonst wie isR1ResumeEligible()
  -> ResumeOffer (nur fuer status==Current, Phase in
     {Preheating, Cooling, ManualHolding})
     -> pendingResume_ = restoreRunPersistenceSnapshot(*loaded.snapshot)
        // NEU gefundene, bereits bestehende, bereits getestete Funktion
        // (run_persistence_contract.hpp/.cpp), liefert std::optional<
        // RunCommandState>. "Technical restoration only: ... does not
        // make recovery, boot, fault or safety decisions."
     -> PENDING_RESUME_OWNER=FermentationApplication
        PENDING_RESUME_TYPE=std::optional<RunCommandState>
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
  -> RunPersistenceCoordinator::activateR1EligibleRun(pendingResume_.value(),
     time)  // NEU, kleine R1-spezifische Methode (siehe unten), KEIN
     activateLoadedRun()
  -> RunPersistenceResult.status == Applied
  -> danach ProcessRuntimeState erstmals published (direkt aus
     candidate.processState, ohne propose()/applyProcessTransition(),
     Abschnitt 4.2/7.2 – Praezedenzfall aus activateLoadedRun()s
     Completed-Zweig)
  -> pendingResume_ geleert
  -> ActuationInterlock::evaluate() mit activationKind=Resume,
     processActivationApplied=true, activationPersistenceResult=Applied
  -> erst danach ALLOWED moeglich

Bei Ablehnung / technisch vertrauenswuerdigem aber nicht resumefaehigem Run:
  -> Application ruft RunPersistenceCoordinator::discardAsNoActiveRun()
     direkt (nicht ueber TemperatureControlApplicationOrchestrator::
     reconcileR1LoadedRun(), Abschnitt 4.9/8 – Aufruf ist identisch zur
     internen Wrapper-Logik, nur ohne die dort zusaetzliche automatische
     PI-/Planner-Reset-Seite, die hier wirkungslos waere)
  -> RunPersistenceCoordinator::discardAsNoActiveRun() committet den
     NoActiveRun-Tombstone zuerst durabel (write-before-apply, bestehend)
  -> pendingResume_ geleert
  -> erst danach Standby published

Bei FallbackRecovered (Abschnitt 7.3):
  -> BootClassification::SafeBoot, kein pendingResume_, kein Tombstone
  -> Application SERVICE_REQUIRED -> Actuation DENIED

Bei technisch untrusted Persistenz (sonstige SafeBoot-Faelle):
  -> kein Tombstone -> Application SERVICE_REQUIRED -> Actuation DENIED
```

### 9.1 `RunPersistenceCoordinator::activateR1EligibleRun()` – neue kleine R1-Methode (Blocker 2, geschlossen)

Zulässig gemäß Korrektur B (kleine neue API, kein Refactor von
`RunRecoveryCoordinator`). Vollständige Vertragsdefinition:

```text
R1_RESUME_ELIGIBLE_PHASES = {Preheating, Cooling, ManualHolding}
  // identisch zu isR1ResumeEligible() (Abschnitt 7.3), keine neue Liste
R1_RESUME_STATE_REBASE = direkte Feldsetzung (kein propose()/
  applyProcessTransition(), Praezedenzfall activateLoadedRun()s
  Completed-Zweig, Abschnitt 4.2/4.4):
    candidate = current
    candidate.processState.stateEnteredAtMillis = time.monotonicMillis
    candidate.processState.transitionSequence += 1
R1_RESUME_PERSISTENCE_ACTION = makeRunPersistenceSnapshot(candidate, ...)
  (bestehende Funktion, wie im Completed-Zweig verwendet) + private
  writeSnapshotCore(..., RunPersistenceMutationKind::Recovery, ...,
  rollbackState=LoadedActiveRun) (bestehende private Methode, dieselbe
  Instanzberechtigung wie activateLoadedRun()/persistTransition())
R1_RESUME_APPLIED_EVIDENCE = RunPersistenceResultStatus::Applied bei Erfolg,
  state_ wird direkt auf Ready gesetzt (Praezedenzfall wie im Completed-/
  Fault-Zweig von activateLoadedRun(), Zeile 535/550)
R1_RESUME_FSM_APPLICATION = candidate.processState direkt (kein
  ProcessState-Wechsel, from==to, siehe validControlTopology()-Befund
  Abschnitt 4.2 – ein Fremdaufruf ueber propose()/applyProcessTransition()
  waere fuer diesen Fall ohnehin abgelehnt worden)
R1_RESUME_C2_FIELDS_CREATED = NO
  (kein PendingRecoveryAnchor, kein recoveryBootAnchorMonotonicMillis,
  kein lastRecoveryEpisodeEvidence, keine recoveryEpisodeRevision-Aenderung,
  keine foldObservedRunSeconds()/Zeitverdikt-Berechnung)
R1_RESUME_RECOVERY_EVALUATION_CREATED = NO
  (kein propose(..., ProcessState::RecoveryEvaluation, ...)-Aufruf)
```

**Explizit benannte Beobachtungskonsequenz (Owner-Anforderung: nicht nur
„bewusst zurueckgesetzt" behaupten, sondern den Effekt je Phase nennen):**

- **`ManualHolding`:** `holdDurationMinutes` wird ueber `elapsed(now,
  stateEnteredAtMillis, holdDurationMinutes)` an anderer Stelle (Tick-Ebene,
  ausserhalb dieses Coordinators) ausgewertet. Da `stateEnteredAtMillis` auf
  den Resume-Zeitpunkt zuruecksetzt wird, **verlaengert** sich die
  tatsaechlich erlebte Gesamthaltezeit um exakt die Reboot-/Offline-Dauer –
  die vor dem Neustart bereits verstrichene Haltezeit geht verloren. Das ist
  eine bewusste Vereinfachung, direkt gedeckt durch den #24-R1-Vertrag
  („keine gewichtete Recoveryzeit-/Progressrettung"): R1 verzichtet
  ausdruecklich auf pruezise Vor-Boot-Zeitanrechnung.
- **`Preheating`/`Cooling`:** kein `stateEnteredAtMillis`-gebundenes
  Zeitlimit in diesen Phasen (temperaturgetrieben, kein
  `stateHasTargetReachTimer()`-Zustand) – das Zuruecksetzen hat **keinen**
  beobachtbaren Zeiteffekt.

**Vorbedingung/Fehlerfall:** `state_ != LoadedActiveRun` oder
`current.processState.state ∉ R1_RESUME_ELIGIBLE_PHASES` →
`RunPersistenceResultStatus::NotEligible`, keine Mutation. Da
`isR1ResumeEligible()` bereits beim Klassifizieren nur genau diese drei
Phasen als `ResumeOffer` zulaesst, ist dieser Fehlerfall in der Praxis nur
eine Konsistenzsicherung, kein neues Gate.

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
| `resetRequestWatchdog()` | **Keep**, ohne Maskenzugriff | Vorbedingung wird direkt aus frischer `ActuationEvidence` + `planner.state().latchedWatchdogFault` neu berechnet |
| `FaultCode` | **Keep** (unverändert, RAM-only) | Diagnose-Enum, `dispositionForFault()`-Mapping entfällt mit `SafetyDisposition` (ersetzt durch direktes `FaultCode`→`SERVICE_REQUIRED`-Mapping in `FermentationApplication`, Abschnitt 7) |

### 10.3 `evaluate()` als reine Funktion

`ActuationInterlock::evaluate(const ActuationEvidence&)` wird
`[[nodiscard]] static`, kein Objektzustand, kein `lastEvaluation()`.
`resetCause_` entfällt aus dem Interlock; Composition übergibt `resetCause`
direkt aus `IResetCauseSource` an `PresentationState`. Bestehende
Hilfsfunktionen (`isPersistenceSafeBoot`, `hasFreshConfigurationEvidence`,
`hasFreshSensorEvidence`, `hasFreshIntegrityEvidence`,
`hasResolvedCommitEvidence`, `isTrustedCoordinatorState`,
`isValidFallbackRecoveryEvidence`) werden unverändert übernommen.

## 11. Fehler-/Ack-/Watchdog-Ownership

| Wahrheit | Owner | current-boot/persistent | Clear-Autorität | Presentation-Owner | Interlock-Projektion |
|---|---|---|---|---|---|
| Konfigurationsfehler | `ConfigurationService`/`ConfigurationRecoveryService` | current-boot (RAM) | Producer selbst | `PresentationState` | `FaultCode ∈ {ConfigurationUnavailable, ConfigurationIntegrityFailure, ConfigurationCommitIndeterminate}` |
| `RunPersistenceUntrusted` | `RunPersistenceCoordinator`/`boot_classification` | current-boot (RAM) | neuer Load-Zyklus | `PresentationState` | `FaultCode::RunPersistenceUntrusted` |
| `SafetySensorUnavailable` | Sensor-Pipeline (#20/#21) | current-boot/laufend (RAM) | Producer selbst | `PresentationState` | `FaultCode::SafetySensorUnavailable` (nicht erreichbar in dieser Revision, Abschnitt 7.4) |
| `ActuatorRequestWatchdog` | ausschließlich `ActuatorPlanner` | current-boot (RAM) | `ActuatorPlanner::applyExternalWatchdogFaultReset()`, nur über `resetRequestWatchdog()` | `PresentationState` | `FaultCode::ActuatorRequestWatchdog` (nicht erreichbar in dieser Revision) |
| Ack | `PresentationState` (aus Interlock verschoben) | current-boot (RAM) | überschrieben durch neuen Fault oder expliziten Reset | `PresentationState` selbst | keine (fließt nie in `evaluate()` zurück) |

`PresentationState` ist ein kleiner, von `FermentationApplication` gehaltener
Typ (`FaultCode` + `bool acknowledged` + optional `ResetCause`).

## 12. Composition (`FermentationApplication`, kleinste, actor-free Fassung)

**Application-interne Fassade, kein neuer Root-Helper** (unverändert
begründet gegenüber Vorfassung). `main/app_main.cpp` ändert sich minimal.

```text
main/app_main.cpp (geaendert, minimal):
  NvsOwningContext::create()  // unveraendert
  application.begin(platform, stateStoreContext->store(), &resetCauseSource)
    // NEU: IStateStore&-Parameter ueber den neuen store()-Accessor
    // (Abschnitt 12.2)

FermentationApplication::begin(platformServices, store, resetCauseSource):
  1. timeZoneResolver_ (neuer EspTimeZoneResolver-Adapter, Abschnitt 12.2)
  2. bootstrapStore_, graphStore_, mutationCoordinator_, configurationService_
     mit store/timeZoneResolver_ konstruieren
  3. ConfigurationRecoveryService::create(store, bootstrapStore_, graphStore_,
     configurationService_, mutationCoordinator_) -> boot()  // boot-only,
     danach zerstoert
  4. RunPersistenceCoordinator(store, epoch, schedule) -> loadAndInitialize()
  5. boot_classification::classify(configResult, loadResult)
     -> BootClassification (Abschnitt 7)
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

### 12.1 Vollständige Ownership-/Lifetime-Tabelle (Blocker 6, geschlossen)

| Objekt | Owner | Lifetime | Depends On | Boot-only? | Zerstörungsreihenfolge | Allocation-Failure |
|---|---|---|---|---|---|---|
| `NvsStateStore` (`IStateStore`) | `NvsOwningContext` (in `main/app_main.cpp`, unverändert) | Prozesslaufzeit, überlebt alle Consumer | `NvsOwningContext`-Partition | NEIN | letztes (nach `FermentationApplication`) | bestehend (`nullptr`-Rückgabe von `create()`) |
| `EspTimeZoneResolver` | `FermentationApplication` (`unique_ptr`) | Application-Laufzeit | `IStateStore`? NEIN (zeitzonenspezifisch, keine Store-Abhängigkeit, Abschnitt 12.2) | NEIN (`ConfigurationService` hält Referenz laufend) | vor `configurationService_` | fail-closed: `begin()` gibt `false` zurück |
| `ConfigurationMutationCoordinator` | `FermentationApplication` (`unique_ptr`) | Application-Laufzeit | keine externen | NEIN (`ConfigurationService` hält Referenz) | vor `configurationService_` | fail-closed |
| `ConfigurationBootstrapStore` | `FermentationApplication` (`unique_ptr`) | **boot-only möglich** – real geprüft: nur `ConfigurationRecoveryService::create()` nimmt sie entgegen, kein weiterer Konsument nach `boot()` gefunden | `IStateStore` | JA (kann nach Schritt 3 zerstört werden) | vor `IStateStore` | fail-closed |
| `ConfigurationGraphStore` | `FermentationApplication` (`unique_ptr`) | **Application-Laufzeit** – `ConfigurationService`-Konstruktor hält `ConfigurationGraphStore&` laufend (Abschnitt 4.6), NICHT boot-only | `IStateStore` | NEIN | vor `configurationService_`, vor `IStateStore` | fail-closed |
| `ConfigurationService` | `FermentationApplication` (`unique_ptr`) | Application-Laufzeit | `mutationCoordinator_`, `graphStore_`, `timeZoneResolver_` (alle müssen mindestens gleich lang leben, real erzwungen durch Referenzmember) | NEIN | vor allen drei Dependencies | fail-closed |
| `ConfigurationRecoveryService` | lokal in `begin()` (`unique_ptr`, per `create()`) | **boot-only** (nur `boot()` aufgerufen, danach freigegeben) | `store`, `bootstrapStore_`, `graphStore_`, `configurationService_`, `mutationCoordinator_` | JA | vor `bootstrapStore_` | bereits bestehender `nullptr`-Vertrag von `create()` |
| `RunPersistenceCoordinator` | `FermentationApplication` (`unique_ptr`) | Application-Laufzeit (auch für Fresh Start/Resume-Aufrufe außerhalb von `begin()`) | `IStateStore`, `epoch`, `schedule` | NEIN | vor `IStateStore` | fail-closed |
| `pendingResume_` (`std::optional<RunCommandState>`) | `FermentationApplication` (Wertmember) | von Klassifikation bis Confirm/Reject (Abschnitt 9) | keine externen Referenzen (reiner Wert) | NEIN | trivial (Wertmember) | entfällt (kein Heap) |
| `PresentationState` | `FermentationApplication` (Wertmember) | Application-Laufzeit | keine | NEIN | trivial | entfällt |

**Verbindlich:** Alle von langlebigen Objekten referenzierten Dependencies
(`ConfigurationService` → `mutationCoordinator_`/`graphStore_`/
`timeZoneResolver_`) leben als `unique_ptr`-Member von
`FermentationApplication` mindestens so lange wie `ConfigurationService`
selbst – Konstruktionsreihenfolge in `begin()` entspricht der Tabelle,
Destruktion läuft in umgekehrter Deklarationsreihenfolge (Member-RAII,
kein manueller Destruktor nötig). `IStateStore` bleibt ausschließlich bei
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
   der `device_platform::ITimeZoneResolver` implementiert. Design-Referenz
   (nicht Code-Übernahme): #119-Plandatei Abschnitt 5.1 identifizierte
   dieselbe Lücke und dieselbe Zielarchitektur (ESP-IDF-native
   Zeitzonenauflösung ohne Netzwerkabhängigkeit). Scope-Grenze: nur so viel
   wie `ConfigurationService`s Konstruktor benötigt, keine WLAN-/NTP-
   Integration (bleibt spätere Issues).

**Lebenszeit-Klarstellung (Abweichung von einer ersten Fassung):** Anders
als #119/#120s rein *boot-only* Objekte müssen die meisten hier komponierten
Objekte für die **gesamte Laufzeit** existieren (siehe Tabelle 12.1), nicht
nur bis Boot-Ende – u. a. weil Fresh Start/Resume (Abschnitt 8/9) nach
`begin()` erneut `RunPersistenceCoordinator`/`ConfigurationService`
aufrufen. Diese Objekte werden deshalb **heapbesitzende Member von
`FermentationApplication`** (`std::unique_ptr<T>`, `new (std::nothrow)`,
fail-closed bei Allokationsfehler), nicht Werte-Member und nicht lokale
Stack-Objekte in `begin()`. `sizeof(FermentationApplication)` bleibt dadurch
klein (nur Zeiger + `pendingResume_` + `PresentationState`), unabhängig von
der Größe des komponierten Graphen.

**Abnahmemetrik (gemessener Pfad exakt festgelegt, Abschnitt 12.3):**
`app_main()`s eigener Entry-Frame bleibt beim heutigen, real gemessenen
`BASE_SHA`-Wert von 112 Byte (Abschnitt 4.8), **und zusätzlich,
maßgeblich**, der reale Main-Task-Stack-High-Watermark
(`uxTaskGetStackHighWaterMark`, bereits heute in `logResources()`
verwendet) über genau den in #121 erreichbaren Pfad: den vollständigen
`begin()`-Aufruf plus einen einzelnen, gezielt ausgelösten
Fresh-Start-**oder**-Resume-Aufruf (kein periodischer `update()`-Tick, da
`update()` leer bleibt, Abschnitt 12.3) – gegen
`CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584` (Abschnitt 16).

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
```

Bestehende relevante native Suiten vollständig weiterführen. Beide
ESP-IDF-Profile bleiben Pflicht. Realer Hardwareboot erst nach
vollständigem Software-Ownerreview.

## 14. Scope-Abgrenzung

### In Scope

Composition-Lücke schließen (`IStateStore`/`ITimeZoneResolver` real bis
`FermentationApplication` durchreichen, actor-free); `BootClassification`
extrahieren (inkl. `FallbackRecovered`-Korrektur); `SafetyCore` →
`ActuationInterlock` verkleinern und umbenennen; `PresentationState`
einführen; neue kleine `RunPersistenceCoordinator::activateR1EligibleRun()`;
`NvsOwningContext::store()`; neuer `EspTimeZoneResolver`-Adapter; zugehörige
Tests migrieren/ergänzen.

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

Keine Änderung an `run_persistence_codec.cpp`, `ActuatorPlanner`,
`ActuatorPlanSinkDriver`, `RunPersistenceCoordinator` (außer der einen neuen
Methode), `ConfigurationRecoveryService`, `ConfigurationService`,
`process_state_machine.hpp/.cpp`.

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
Schritt 5: FermentationApplication::begin() um die actor-free Composition-
  Fassade erweitern (Abschnitt 12), main/app_main.cpp minimal anpassen.
Schritt 6: Teststrategie vollstaendig umsetzen (Abschnitt 13).
Schritt 7: reale ESP-IDF-Builds + Stack-/Heap-Nachweis; danach eigenes
  Owner-Gate fuer einen realen actor-free Hardwareboot.
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
MAIN_TASK_STACK_HIGH_WATERMARK=<PASS|FAIL|NOT_MEASURED>
BREAKING_PERSISTENCE_CHANGE=NO
SCHEMA_MIGRATION_REQUIRED=NO
ISSUE121_PRODUCT_COMPOSITION_ACTOR_FREE=YES
REAL_HARDWARE_BOOT=NOT_RUN                    -- eigenes Owner-Gate, Schritt 7
```

## 17. Statuszusammenfassung

```text
PR122_HEAD=<exact, nach Commit>
PLAN_PATH=docs/tasks/issue-121-lifecycle-safety-simplification-plan.md
PLAN_REVISION=R1
PLAN_SHA=<exact, nach Commit>

ARCHITECTURE_VERDICT=SIMPLIFY

RESUME_OFFER_OWNERSHIP=CLOSED
R1_RESUME_PATH=CLOSED_NO_C2
FALLBACK_RECOVERED_R1_POLICY=SERVICE_REQUIRED_NO_RESUME
STATELESS_INTERLOCK_HANDOFF=CLOSED
PRODUCT_COMPOSITION_ACTOR_FREE=YES
CONFIG_LIFETIME_GRAPH=CLOSED
BOOT_PROCESS_BOUNDARY=CLOSED
RUNTIME_APPLICATION_LIFECYCLE=CLOSED

BREAKING_PERSISTENCE_CHANGE=NO
SCHEMA_MIGRATION_REQUIRED=NO

SOURCE_OF_TRUTH_CONFLICT=NONE
IMPLEMENTATION=NOT_STARTED
MATERIAL_ARCHITECTURE_DECISION_OPEN=NO
OWNER_PLAN_REVIEW=PENDING
```

STOP – Owner Full Review der neuen exakten Plan-SHA. Keine Implementation.
