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

### Herkunft dieses Plans

Ein separat beauftragter Architektur-Audit der Lifecycle-/Safety-/
Persistenz-Verträge wurde in einer früheren, nicht in diesem Repository
persistierten Session durchgeführt. Der Owner hat dessen Hauptrichtung
angenommen und dazu die verbindlichen Korrekturen A–J erteilt (Abschnitt 1).
**Diese Plandatei zitiert aus dem Audit ausschließlich das, was der Owner in
seinen Korrekturen A–J wörtlich referenziert** (z. B. "Audit bewertet
fail-closed Boot = PARTIAL"); der vollständige Audittext selbst ist nicht
verfügbar und wird nicht rekonstruiert oder paraphrasiert. Die
Current-State-Inventur (Abschnitt 4) und die Zielarchitektur (Abschnitt 5–13)
sind stattdessen vollständig neu und direkt gegen den realen Code auf
`BASE_SHA` erstellt, wie es die Ownervorgabe für Abschnitt 14.1 verlangt.
Damit ist der Audittext für die Gültigkeit dieses Plans nicht tragend.

```text
ARCHITECTURE_AUDIT=COMPLETED
ARCHITECTURE_AUDIT_OWNER_REVIEW=PASS_WITH_CORRECTIONS
ARCHITECTURE_AUDIT_SOURCE_TEXT=NOT_PERSISTED_NOT_RECOVERABLE
ARCHITECTURE_VERDICT=SIMPLIFY
PLAN_BASIS=FRESH_CODE_INVENTORY_PLUS_OWNER_CORRECTIONS_A_J
```

## 1. Ownerkorrekturen A–J (verbindlich, vollständig referenziert)

Diese Korrekturen sind der bindende Vertrag für Abschnitt 4–13. Jede spätere
Abschnittsreferenz auf „Korrektur X" bezieht sich auf diese Liste.

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
  `RunRecoveryCoordinator = PRESERVE_LEGACY_NOT_ACTIVE_R1`.
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
- **G – SafetyCore wirklich verkleinern.** Für `SafetyDisposition`,
  `activeFaultMask_`, `acknowledgedFaultMask_`, `unknownProducerSources_`,
  `canClearFault()`, `resetRequestWatchdog()`, `FaultCode` ist je ein
  Keep/Move/Drop-Urteil gegen den kleinen Interlock erforderlich. Producer
  bleiben Autorität ihrer eigenen Wahrheit; Ack ist Presentation und bleibt
  nur bei konkretem technischem Grund im Interlock. Der #23-Watchdog-Latch
  bleibt beim `ActuatorPlanner`. Umbenennung nur, wenn sie Klarheit bringt.
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

Nicht Gegenstand: eine neue Ursachenanalyse des `LoadProhibited`-Panics. Die
neue, kleinere Composition (Abschnitt 12) hat einen anderen, kleineren
Objektgraphen als #120s Boot-Helper; sollte nach Umsetzung real erneut ein
Boot-Fehler auftreten, ist das ein neuer, eigener Befund und kein
Fortsetzungsauftrag dieses Plans.

## 3. Zielarchitektur-Überblick

Fünf Verantwortlichkeiten, je eine Autorität, keine doppelte Policy:

```text
Device/Application Lifecycle   -> FermentationApplication (erweitert)
Process Lifecycle              -> ProcessStateMachine (unveraendert)
Boot Classification            -> neu: BootClassification (frei Funktion/Enum)
Persistence Technical Result   -> RunPersistenceCoordinator (unveraendert)
Actuation Permission           -> ActuationInterlock (umbenannt aus SafetyCore, verkleinert)
```

`TemperatureControlApplicationOrchestrator` bleibt der bestehende
Persistenz-/Process-Lifecycle-Orchestrator (Abschnitt 4.9); er wird nicht
neu erfunden, sondern in Abschnitt 12 erstmals real komponiert.

## 4. Current-State-Inventur (auf `BASE_SHA`, real geprüft)

### 4.1 `FermentationApplication`

`lib/fermentation_app/src/fermentation_application.hpp/.cpp` (26+33 Zeilen).
Einziges Feld: `SafetyCore safetyCore_`. `begin()` ruft
`safetyCore_.beginBoot(resetCause)` und wertet ein **leeres**
`SafetyCoreInput` aus („Composition root has not supplied the real
configuration, persistence, sensor and planner evidence yet"). Kein
`ConfigurationRecoveryService`, kein `RunPersistenceCoordinator`, kein
`RunRecoveryCoordinator`, kein `TemperatureControlApplicationOrchestrator`,
kein `ActuatorPlanner` verdrahtet. Das ist die reale, ungeschönte
Ausgangslage – dieselbe Lücke, die #119/#120 schließen wollten.

### 4.2 `ProcessStateMachine`

`process_state_machine.hpp/.cpp` (223+1085 Zeilen). `ProcessState` (15
Werte, Wirecodes siehe Abschnitt 6): `Boot=1, SafeBoot=2, Standby=3,
Preheating=4, WaitingForProduct=5, ReachingTarget=6, QualifyingTarget=7,
Fermenting=8, Cooling=9, CoolHolding=10, ManualHolding=11, Completed=12,
RecoveryEvaluation=13, Fault=14, ServiceMode=15`.

Boot-Übergänge laufen ausschließlich über `decideBootEvent()`
(`ProcessEvent::BootReady→Standby`, `BootSafe→SafeBoot`,
`BootRestoreCompleted→Completed`, `BootRecoverRun→RecoveryEvaluation`) und
`decideRecoveryEvent()` (`RecoveryResume`/`RecoveryReject` aus
`RecoveryEvaluation`). `ServiceMode` wird **ausschließlich** aus `Standby`
über `ProcessEvent::EnterServiceMode` betreten und über `ExitServiceMode`
wieder verlassen (`decideStandbyEvent()`, Zeile 629;
`decideExplicitEvent()`-Fall `ServiceMode`, Zeile 767) – kein Boot-Pfad
berührt `ServiceMode` direkt. `ProcessState::Fault` wird ausschließlich über
`TransitionReason::CriticalFault` von einem aktiven Laufzustand aus erreicht
(Zeile 1026 f., `stateCanEnterFault()`), nie vom Boot aus. `Fault` ist damit
ein reiner **Run-Lifecycle**-Endzustand, kein Device-/Boot-Zustand.

`ProcessRuntimeState.state` **defaultet auf `ProcessState::Boot`**
(process_state_machine.hpp:73) – das ist der konkrete Stolperstein für
Korrektur I (Abschnitt 7.2).

`propose()` und `applyProcessTransition()` sind bereits öffentlich für
Recovery-Orchestrierung außerhalb dieser Übersetzungseinheit exportiert
(Kommentar Zeile 205–214) und werden heute schon direkt von
`RunPersistenceCoordinator::activateLoadedRun`/`resolveRecoveryOutcome`
verwendet.

### 4.3 `SafetyCore` (712+196 Zeilen)

Zentrale, stateful Klasse. Felder: `resetCause_`, `activeFaultMask_`,
`acknowledgedFaultMask_`, `unknownProducerSources_`, `lastEvaluation_`.
`evaluate(SafetyCoreInput)` ist **fast**, aber nicht vollständig, eine reine
Funktion des aktuellen Inputs:

- `activeFaultMask_ |= observedFaults` akkumuliert über Aufrufe hinweg; ein
  Fehler, der in einem Tick nicht erneut beobachtet wird, bleibt aktiv, bis
  `canClearFault(code, input, loadDisposition)` **zusätzlich** zustimmt.
  `canClearFault()` selbst ist aber bereits rein eine Funktion von `input`
  (kein weiterer historischer Zustand außer der Tatsache, dass der Code
  überhaupt gesetzt war) – die Maske speichert also nur „war je gesetzt",
  nicht zusätzliche Fachinformation.
- `unknownProducerSources_`: `observeProducerKnownness()` hat ein
  `if (!provided) return;`-Frühausstieg (Zeile 146), der ein einmal als
  „unknown" markiertes Producer-Bit **nicht** löscht, wenn derselbe Producer
  in einem späteren Tick einfach kein Feld mehr liefert. Das ist echte,
  über den aktuellen Input hinausgehende Erinnerung.
- `ActuatorRequestWatchdog` (Bit 6) wird sowohl in `activeFaultMask_`
  gespiegelt als auch nativ vom `ActuatorPlanner` selbst gelatcht
  (`state().latchedWatchdogFault`) – zwei Wahrheitsquellen für denselben
  Fakt (siehe Korrektur G, „Interlock soll fremde Fehlerzustände nicht
  nochmals als zweite Wahrheitsquelle latchen").
- `acknowledge()`/`isAcknowledged()`: laut eigenem Kommentar „changes
  presentation state only. It never changes the gate or the active fault
  condition." – bereits als Presentation dokumentiert, aber im
  Safety-kritischen Typ untergebracht.
- `resetRequestWatchdog()`: benötigt als Vorbedingung
  `activeFaultMask_ == faultBit(ActuatorRequestWatchdog)` (Zeile 378) – liest
  also die Maske, nicht frische Evidenz allein.
- `classifyRunLoad()` und `isR1ResumeEligible()` sind **bereits
  zustandslose `static`-Funktionen** ohne Zugriff auf Instanzfelder – reine
  Boot-/Persistenz-Klassifikation, fälschlich im Actuation-Typ
  untergebracht.
- `FaultCode` besitzt eine `wireValue()`-Methode, wird aber **nirgends
  persistiert**: keine Referenz in `run_snapshot.hpp`,
  `run_persistence_contract.hpp` oder `run_persistence_codec.cpp` (real
  geprüft). `FaultCode` ist RAM-only/Diagnose und unterliegt keinem
  Wire-Kompatibilitätszwang.

`SafetyBootDisposition{Unresolved, Standby, ResumeOffer, NoActiveRun,
Completed, TerminalFault, SafeBoot}` ist inhaltlich bereits die
Boot-Classification.

### 4.4 `RunPersistenceCoordinator` (290+? Zeilen, hpp vollständig gelesen)

Bereits vollständige, wiederverwendbare API: `loadAndInitialize()` →
`RunPersistenceLoadResult{status, snapshot}`; `activateLoadedRun()`,
`activateFallbackRecoveredRun()`, `resolveRecoveryOutcome()`,
`discardAsNoActiveRun()` (R1-Discard-Pfad für technisch vertrauenswürdige,
aber nicht resumefähige Läufe – bereits write-before-apply, committet den
`NoActiveRun`-Kandidaten zuerst durabel, RAM erst danach), `persistCommand`,
`persistTransition`, `persistSensorSelection`, `checkpointPeriodic`. Keine
Änderung an diesem Vertrag ist für diesen Plan nötig.

### 4.5 `ConfigurationRecoveryService`

`create()` nimmt bereits `IStateStore&` als expliziten Konstruktorparameter
(zusammen mit Bootstrap-/GraphStore, `ConfigurationService`,
`ConfigurationMutationCoordinator`). `boot()` liefert
`ConfigurationRecoveryResult{status, diagnostics, safetyProducer}`. Kein
Änderungsbedarf.

### 4.6 `ConfigurationService`

397 Zeilen, `ConfigurationServiceMode` (10 Werte), `ConfigurationCommitStatus`
(11 Werte) – beide bereits vollständig von `SafetyCoreInput`/`evaluate()`
konsumiert. Kein Änderungsbedarf.

### 4.7 `ActuatorPlanner` / `ActuatorPlanSinkDriver`

`ActuatorSafetyGateStatus{Unresolved, Allowed, ImmediateStop}` (3 Werte,
`actuator_plan_types.hpp`). In `runPhaseB()` (actuator_planner.cpp:1069–1078)
bereits **strukturell getrennt**: `Unresolved` →
`ActuatorPlanReason::SafetyGateUnresolved`, `ImmediateStop` →
`ActuatorPlanReason::ExternalSafetyOverride`; beide enden identisch in
`rejectToIdle(...)` (Idle, kein Ausgang). Der bestehende Vertrag trennt
Permission (3-wertiges Gate), Grund (`ActuatorPlanReason`, 21 Werte) und
physische Reaktion (`rejectToIdle`, Lüfternachlauf über
`ActuatorPlannerParameters::outerFanPostRunMillis`/`innerFanPostRunMillis`)
bereits vollständig. `ActuatorPlanSinkDriver` ist ein reiner, zustandsloser
Übersetzer von `ActuatorPlanTickResult` auf die Plattform-Sinks – kein
eigener Gate-Zugriff, kein Änderungsbedarf.

### 4.8 `main/app_main.cpp` (193 Zeilen, `BASE_SHA`-Stand)

`NvsOwningContext::create()` initialisiert die `state_store`-Partition und
öffnet den `NvsStateStore`; der Store wird **nicht** an `application.begin()`
übergeben. `application.begin(platform, &resetCauseSource)` ist der einzige
Aufruf; kein `IStateStore&`-Parameter existiert heute. Genau das ist die zu
schließende Lücke (Abschnitt 12).

### 4.9 `TemperatureControlApplicationOrchestrator` (real vorhanden, zentral)

Datei `temperature_control_orchestrator.hpp/.cpp` (178+ Zeilen). **Dies ist
nicht der von der Ownervorgabe unter dem Namen
„TemperatureControlApplicationOrchestrator" vermutete neue Baustein,
sondern eine bereits vollständig implementierte, bislang nur unverdrahtete
Klasse** – der wichtigste Fund dieser Inventur:

- Zwei Konstruktoren: ein 3-Parameter-Konstruktor (`persistence`,
  `temperatureController`, `evaluator`) für reine Persistenz-/Lifecycle-Tests
  und ein 6-Parameter-Konstruktor, der zusätzlich `ActuatorPlanner&`,
  `ActuatorPlanSinkDriver&` und `SafetyCore&` bindet – **ohne** diese
  Referenzen ist `evaluateTemperatureControl()` strukturell dauerhaft
  `Unavailable`/`NoCommissioning` (Owner-Review F4, Klassenkommentar
  Zeile 136–145).
- `persistFreshStartCommand()`: bereits der **vollständige #17
  Write-before-Apply Fresh-Start-Pfad** (Abschnitt 9).
- `reconcileR1LoadedRun()`: bereits die **vollständige R1-Discard-Logik**
  für einen technisch vertrauenswürdigen, aber laut
  `SafetyCore::classifyRunLoad()` nicht resumefähigen Snapshot – ruft intern
  `persistence_.discardAsNoActiveRun(current, time)` (Abschnitt 8).
- `activateRecovery(RunRecoveryCoordinator&, ...)`: bindet
  `RunRecoveryCoordinator::activate()` – **wird von diesem Plan nie
  aufgerufen**, wodurch `RunRecoveryCoordinator` exakt gemäß Korrektur B
  weiterhin `PRESERVE_LEGACY_NOT_ACTIVE_R1` bleibt, ohne dass eine Zeile
  daran geändert werden muss.
- `resolveRecoveryOutcome()`, `reevaluateRecoveryEvaluation()`: existieren
  bereits, werden von diesem Plan ebenfalls nicht aufgerufen (sie gehören zum
  heutigen `RecoveryEvaluation`-Pfad, der in R1 nicht mehr aktiv erzeugt
  wird, Abschnitt 6).
- `complete()`/`needsRuntimeReset()`: leiten aus dem Prozesszustand vor/nach
  einer Persistenzmutation automatisch PI-/Planner-Resets ab
  (`resetTemperatureControlAtBoundary`, `resetActuatorPlanAtBoundary`).

Diese Klasse wird von diesem Plan **nicht verändert**, nur zum ersten Mal
real komponiert (Abschnitt 12). Das reduziert den Umsetzungsumfang
gegenüber einer naiven Neuimplementation erheblich.

### 4.10 `RunRecoveryCoordinator` (existiert, nicht aktiv)

`lib/fermentation_app/src/run_recovery.hpp/.cpp` (59+344 Zeilen). Bestätigt
Korrektur B: real vorhanden, von `TemperatureControlApplicationOrchestrator`
referenzierbar, aber von keinem Composition-Pfad aktuell konstruiert oder
aufgerufen. Dieser Plan ändert daran nichts.

## 5. Zielverantwortungen (5 Autoritäten, keine Doppel-Policy)

| Verantwortung | Typ | Autorität für |
|---|---|---|
| Device/Application Lifecycle | `FermentationApplication` (erweitert) | `INITIALIZING` / `READY` / `SERVICE_REQUIRED`; Komposition der übrigen vier |
| Boot Classification | `boot_classification.hpp/.cpp` (neu, frei Funktion) | Klassifikation eines geladenen Snapshots + Konfigurationsvertrauen zu genau einem der 8 Boot-Flow-Ergebnisse (Abschnitt 7) |
| Process Lifecycle | `ProcessStateMachine` (unverändert) | Laufzustand eines aktiven Laufs (Preheating…Fault); keine Boot-/Service-Anteile mehr aktiv angesteuert |
| Persistence Technical Result | `RunPersistenceCoordinator` (unverändert) | technische Speicherwahrheit, `RunPersistenceLoadResult`, `RunPersistenceResult` |
| Actuation Permission | `ActuationInterlock` (umbenannt/verkleinert aus `SafetyCore`) | `DENIED`/`ALLOWED` für den `ActuatorPlanner`-Eingang |
| Fault/Diagnostic Presentation | `Ack`-Zustand + `FaultCode`, aus dem Interlock in eine reine Presentation-Struktur verschoben (Abschnitt 11) | UI-/Diagnoseanzeige, nie gate-relevant |

Kein Baustein hat mehr als eine dieser Zeilen inne. Insbesondere hat der
Interlock **keine** Boot-/Persistenz-Klassifikationsautorität mehr;
`BootClassification` hat **keine** Aktorfreigabeautorität.

## 6. Legacy-/Wire-Vertrag

`run_persistence_codec.cpp` bleibt **vollständig unverändert** – encode
(Zeile 248–279) und decode (Zeile 531 ff.) beider Richtungen bleiben exakt
wie auf `BASE_SHA`. Es gibt keinen einzigen Codezeilen-Diff in dieser Datei.

| Wire-Wert | `ProcessState` | alte Bedeutung | neu aktiv erzeugt? | Legacy-Decode? | Klassifikation beim Lesen | Migration |
|---:|---|---|:---:|:---:|---|:---:|
| 1 | `Boot` | Boot-transienter Zustand vor Klassifikation | **Nein** | Ja | s. u. | NEIN |
| 2 | `SafeBoot` | Boot-transiente Safe-Boot-Anzeige | **Nein** | Ja | s. u. | NEIN |
| 3 | `Standby` | kein aktiver Lauf, bereit | Ja (unverändert) | Ja | – | NEIN |
| 4–11, 14 | Preheating…Fault | aktive Run-Lifecycle-Zustände | Ja (unverändert) | Ja | – | NEIN |
| 12 | `Completed` | Lauf abgeschlossen | Ja (unverändert) | Ja | – | NEIN |
| 13 | `RecoveryEvaluation` | Boot-transiente Wartezustand auf Resume/Reject | **Nein** | Ja | s. u. | NEIN |
| 15 | `ServiceMode` | bewusst gewählter Bedienmodus | **Ja, unverändert** (Abschnitt 7.1) | Ja | – | NEIN |

**Klassifikation beim Lesen für 1/2/13:** Diese Werte werden von der neuen
`BootClassification` **nie mehr aktiv geschrieben**, weil `ProcessEvent::
BootReady/BootSafe/BootRecoverRun` von der neuen Composition nicht mehr
gesendet werden (Abschnitt 12). Sollte ein alter, vor diesem Plan
geschriebener Snapshot dennoch `processState.state ∈ {Boot, SafeBoot,
RecoveryEvaluation}` enthalten (praktisch nur bei sehr alten
Dev-/Testartefakten denkbar, da diese Zustände nach heutigem
`RunCheckpointSchedule`-Vertrag nie während eines aktiven, checkpointpflicht
igen Laufs erreicht werden), greift **bereits heute unverändert**
`SafetyCore::isR1ResumeEligible()` (Zeile 396–431 in `safety_core.cpp`, in
`boot_classification.cpp` unverändert weitergeführt, Abschnitt 5): Der
`switch` über `snapshot.processState.state` behandelt `Boot`, `SafeBoot`,
`RecoveryEvaluation` bereits explizit als `return false` (nicht
resumefähig). `classifyRunLoad()` liefert für einen solchen Snapshot damit
`RunLoadDisposition::NoActiveRun` → `BootClassification::DiscardableRun`
(Abschnitt 7). **Kein neuer Code für diesen Fall nötig** – die bestehende
Logik ist bereits korrekt fail-closed und wird 1:1 in `boot_classification.
cpp` übernommen.

`FaultCode` ist nicht in dieser Tabelle, weil es real nirgends persistiert
wird (Abschnitt 4.3) und daher keinem Wire-Vertrag unterliegt.

## 7. Boot-Flows

`BootClassification` (neuer Werttyp, Datei `boot_classification.hpp`,
Namespace `fermentation`) übernimmt `SafetyBootDisposition` **unverändert**
als Enum (nur die Datei/Ownership wandert, keine Wertänderung):

```cpp
enum class BootClassification : std::uint8_t {
    Unresolved,    // interner Zwischenzustand waehrend der Klassifikation
    NoRun,         // = altes "Standby": kein persistierter Lauf/leer
    ResumeOffer,
    DiscardableRun,  // = altes "NoActiveRun": vertrauenswuerdig, nicht resumefaehig
    CompletedRun,    // = altes "Completed"
    TerminalRunFault,// = altes "TerminalFault"
    SafeBoot,
};
```

(Umbenennung von drei Werten gegenüber `SafetyBootDisposition` für Klarheit
im neuen, eigenständigen Typ; keine Wire-Bedeutung, da `BootClassification`
selbst nie persistiert wird – nur `ProcessState`/`RunLoadStatus` sind
Wire-Typen, Abschnitt 6.)

Für jeden Flow: Application Lifecycle / Process State / Persistence Action /
Actuation Permission / UI-Diagnose-Ergebnis.

| Flow | Application Lifecycle | Process State | Persistence Action | Actuation | Diagnose |
|---|---|---|---|---|---|
| `NoRun` | `READY` | published: `Standby` (frisch via `propose()`+`TransitionReason::BootCompleted`) | keine | `DENIED` (bis Fresh Start) | keine |
| `ResumeOffer` | `READY` | **nicht published** (kein `ProcessRuntimeState` vor Nutzerentscheidung) | Snapshot bleibt in `RunPersistenceCoordinator` (`LoadedActiveRun`) | `DENIED` | ResumeOffer-Anzeige mit Snapshot-Vorschau |
| `ResumeConfirmed` | `READY` | published nach `activateLoadedRun()` (bestehende API, ruft intern `propose`/`applyProcessTransition`) | `RunPersistenceCoordinator::activateLoadedRun()` | `DENIED` bis frische Interlock-Evidenz nach `Applied` | Lauf läuft weiter |
| `ResumeRejected` | `READY` | published: `Standby` nach Discard | `RunPersistenceCoordinator::discardAsNoActiveRun()` (write-before-apply) | `DENIED` | Verworfen-Hinweis optional |
| `DiscardableRun` | `READY` | published: `Standby` nach `TemperatureControlApplicationOrchestrator::reconcileR1LoadedRun()` (bereits vorhanden, Abschnitt 4.9) | `discardAsNoActiveRun()` (intern) | `DENIED` | keine (stiller Discard, wie heute) |
| `CompletedRun` | `READY` | published: `Completed` (direkt via `propose()`+vorhandener `CompletedRunRestored`-Reason, kein `BootRestoreCompleted`-Event nötig) | keine Mutation | `DENIED` | Abschluss-Anzeige |
| `TerminalRunFault` | `READY` | published: `Fault` (direkt via `propose()`) | keine Mutation | `DENIED` | Fehler-Anzeige |
| `UntrustedConfiguration` | `SERVICE_REQUIRED` | nicht published | keine | `DENIED` | `FaultCode ∈ {ConfigurationUnavailable, ConfigurationIntegrityFailure, ConfigurationCommitIndeterminate}` (bestehende Werte, Abschnitt 4.3/11) |
| `UntrustedPersistence` | `SERVICE_REQUIRED` | nicht published | keine | `DENIED` | `FaultCode = RunPersistenceUntrusted` (bestehend) |
| `SensorUnavailableAtStartup` | **`READY`** (Korrektur E!) | published: `Standby` (Boot selbst braucht keine Sensor-Evidenz, nur Aktivierung tut das) | keine | `DENIED`, Grund `SafetySensorUnavailable` sobald Aktivierung versucht wird | Sensor-Warnung, kein Service-Zustand |
| `WatchdogLatched` | `READY` (Laufzeit-, kein Boot-Flow – Latch entsteht nur während eines laufenden #23-Tickzyklus) | unverändert (Run läuft technisch weiter, Aktoren gesperrt) | keine | `DENIED`, Grund `RequestWatchdogFaultLatched`, entsperrbar nur über `resetRequestWatchdog()` (Abschnitt 11) | Watchdog-Anzeige |

`UntrustedConfiguration` und `UntrustedPersistence` sind zwei **Diagnose-**
Flows, keine zwei `BootClassification`-Werte: beide liefern
`BootClassification::SafeBoot`; die Unterscheidung transportiert der
begleitende, bereits existierende `FaultCode` (Abschnitt 11), exakt wie
heute in `SafetyCore::dispositionForFault()`.

`SensorUnavailableAtStartup` ist **kein eigener `BootClassification`-Wert**:
Ein bloßer Boot mit ungültigem Sensor klassifiziert normal zu `NoRun`/
`ResumeOffer`/etc.; `hasFreshSensorEvidence()` wird nur verlangt, wenn
`explicitActivationRequested` oder ein `SafetyActivationKind` gesetzt ist
oder `loadDisposition == ResumeOffer` (bestehende Regel,
`safety_core.cpp:270-279`, unverändert übernommen). Das ist bereits exakt
Korrektur E; kein neuer Code nötig.

### 7.1 Servicebegriffe: `SERVICE_MODE` vs. `SERVICE_REQUIRED` (Korrektur J, geschlossen)

Zwei getrennte, orthogonale Konzepte auf zwei verschiedenen Ebenen:

```text
SERVICE_MODE   = ProcessState::ServiceMode, UNVERAENDERT (Abschnitt 4.2).
                 Eigentuemer: ProcessStateMachine.
                 Nur aus Standby ueber ProcessEvent::EnterServiceMode
                 betretbar, nur ueber ExitServiceMode wieder verlassbar.
                 Bewusst vom Nutzer gewaehlter Bedien-/Wartungsmodus.

SERVICE_REQUIRED = neuer Application-Lifecycle-Wert, Eigentuemer:
                 FermentationApplication (Abschnitt 5). Wird bei
                 BootClassification::SafeBoot (UntrustedConfiguration/
                 UntrustedPersistence, Abschnitt 7) gesetzt: Device/
                 Application kann den normalen Betrieb nicht anbieten.
```

Beide sind **unabhängig**: `SERVICE_MODE` setzt kein `SERVICE_REQUIRED`
voraus und umgekehrt (ein Gerät kann `SERVICE_REQUIRED` sein und dennoch nie
`ServiceMode` betreten haben, wenn z. B. die Konfiguration beim Boot
verworfen wurde). `ServiceMode` bewegt sich ursprünglich nur zwischen
`Standby`, wodurch es real ohnehin nur nach erfolgreicher Bootklassifikation
(`NoRun`, Abschnitt 7) erreichbar ist – bei `SERVICE_REQUIRED` wird gar kein
`ProcessRuntimeState` published (Abschnitt 7.2), `ServiceMode` ist dann
strukturell unerreichbar.

**Kein Servicemodus erzeugt implizite Aktorfreigabe:** `ServiceMode` erzeugt
laut `activationEvidenceComplete()` (unverändert, `safety_core.cpp:627-644`)
strukturell nie `ALLOWED`, weil dort kein `SafetyActivationKind ≠ None`
gesetzt wird und kein `processActivationApplied`/`activationPersistenceResult
== Applied` aus einer Aktivierung vorliegt – der Zustand bleibt fail-closed
`DENIED`, ohne dass dafür neuer Code nötig ist. Keine neue Berechtigungs-/
PIN-Plattform (Korrektur J).

### 7.2 Bootzustandspublikation: `INITIALIZING` vs. `STANDBY` (Korrektur I, geschlossen)

**Entscheidung:** `FermentationApplication` exponiert den Prozesszustand als
`std::optional<ProcessRuntimeState> publishedProcessState()`, **nicht**
über einen bare `ProcessRuntimeState`. Solange `BootClassification` noch
nicht abgeschlossen ist (`INITIALIZING`), bleibt dieser Optional leer –
kein Verbraucher kann einen defaultkonstruierten `ProcessRuntimeState`
(`state == ProcessState::Boot`, Abschnitt 4.2) fälschlich als `Standby`
oder sonstigen Freigabezustand lesen, weil vor Abschluss der Klassifikation
schlicht **kein** Wert veröffentlicht wird.

Der intern in `process_state_machine.hpp` bestehende Default-Initialisierer
`ProcessRuntimeState::state{ProcessState::Boot}` bleibt **unverändert** –
er wird nicht extern beobachtbar, solange die Application-Fassade
konsequent über den Optional-Accessor veröffentlicht und keinen
default-konstruierten `ProcessRuntimeState` je direkt zurückgibt. Das ist der
minimalste der drei von der Ownervorgabe genannten Wege (Optional statt
Default-Änderung) und vermeidet jede Änderung an
`process_state_machine.hpp/.cpp` oder an bestehenden Tests, die sich auf den
heutigen Default verlassen.

Die erste tatsächliche Veröffentlichung geschieht ausschließlich über die
bereits bestehenden, unveränderten Exportfunktionen `propose()` +
`applyProcessTransition()` (Abschnitt 4.2/8/9) – nie durch direktes Setzen
eines Feldes.

```text
Application Lifecycle: INITIALIZING
  -> BootClassification abgeschlossen
  -> genau EINE der Abschnitt-7-Flow-Aktionen (propose()+applyProcessTransition()
     oder bewusst "nicht published" fuer ResumeOffer/SERVICE_REQUIRED)
  -> Application Lifecycle: READY oder SERVICE_REQUIRED
```

`INITIALIZING != STANDBY` ist damit strukturell erzwungen, nicht nur
dokumentiert: Es gibt keinen Codepfad, der `Standby` published, ohne zuvor
`BootClassification::NoRun` (oder einen anderen Flow, der explizit
`Standby` published, Abschnitt 7) durchlaufen zu haben.

## 8. Fresh Start

Exakter, vollständig aus Bestehendem zusammengesetzter Pfad:

```text
Standby (Application READY, Actuation DENIED)
  -> Nutzer Start-Kommando
  -> TemperatureControlApplicationOrchestrator::persistFreshStartCommand(...)
     (bestehend, 4.9: prueft CommandKind::StartProgram/StartManualHolding,
      delegiert an persistCommand -> RunPersistenceCoordinator::persistCommand,
      #17 Write-before-Apply unveraendert)
  -> RunPersistenceResultStatus::Applied
  -> complete() leitet automatisch Process-/Planner-/PI-Reset an der
     Lifecycle-Grenze ab (needsRuntimeReset(), bestehend)
  -> ActuationInterlock::evaluate(evidence) mit
     activationKind=FreshStart, explicitActivationRequested=true,
     processActivationApplied=true, activationPersistenceResult=Applied
  -> erst danach ALLOWED moeglich
```

Kein neuer Code für den Persistenz-/Prozess-Anteil; nur die Verdrahtung
(Application ruft `persistFreshStartCommand` und danach
`ActuationInterlock::evaluate` mit den realen Post-Commit-Feldern) ist neu
(Abschnitt 12).

## 9. Resume (Ownership-/Lifetime-Vertrag, Korrektur F)

```text
RunPersistenceCoordinator::loadAndInitialize()
  -> RunPersistenceLoadResult{status=Current, snapshot=Some}
  -> BootClassification-Funktion klassifiziert via
     classifyRunLoad(status, &snapshot) (unveraendert aus SafetyCore
     uebernommen) + isR1ResumeEligible(snapshot) (unveraendert)
  -> ResumeOffer
     -> Snapshot bleibt EINZIG im RunPersistenceCoordinator
        (state() == LoadedActiveRun), kein zweiter Kopierort
     -> Application published KEINEN ProcessRuntimeState (Korrektur I)
     -> Actuation DENIED (Interlock evaluate() ohne activationKind)
     -> kein automatisches FSM-Advancement

Bei explizitem Resume-Confirm durch Nutzer:
  -> Application prueft aktuelle Config-/Sensor-/Safety-Evidenz frisch
     (dieselben hasFreshConfigurationEvidence()/hasFreshSensorEvidence()-
     Helfer, unveraendert aus safety_core.cpp uebernommen)
  -> RunPersistenceCoordinator::activateLoadedRun(current, time, sensorEvidence)
     (bestehende API; ruft intern bereits propose()+applyProcessTransition()
     und damit den Process-State-Wechsel selbst)
  -> RecoveryActivationOutcome.persistenceResult.status == Applied
  -> danach ProcessRuntimeState erstmals published
  -> ActuationInterlock::evaluate() mit activationKind=Resume,
     processActivationApplied=true, activationPersistenceResult=Applied
  -> erst danach ALLOWED moeglich

Bei Ablehnung / technisch vertrauenswuerdigem aber nicht resumefaehigem Run:
  -> TemperatureControlApplicationOrchestrator::reconcileR1LoadedRun(...)
     (bestehend, 4.9) bzw. bei explizitem Nutzer-Reject dieselbe
     discardAsNoActiveRun()-API direkt aufgerufen
  -> RunPersistenceCoordinator::discardAsNoActiveRun() committet den
     NoActiveRun-Tombstone zuerst durabel (write-before-apply, bestehend)
  -> erst danach Standby published

Bei technisch untrusted Persistenz:
  -> kein Tombstone (BootClassification::SafeBoot/UntrustedPersistence,
     Abschnitt 7) -> Application SERVICE_REQUIRED -> Actuation DENIED
```

Es entsteht **keine neue persistierte ResumeOffer-State-Machine**: der
einzige durable Zustand bleibt der bereits vorhandene
`RunPersistenceCoordinator`/Store-Inhalt; "ResumeOffer" existiert nur als
RAM-Klassifikationsergebnis, solange der Nutzer noch nicht entschieden hat.

## 10. Interlock (`ActuationInterlock`, minimaler Vertrag)

**Umbenennung von `SafetyCore` beschlossen** (Korrektur G, letzter Satz):
Der Typ verliert die Boot-/Persistenz-Klassifikation (→
`boot_classification.hpp`) und die Ack-Presentation (→ Abschnitt 11) und
wird dadurch klar kleiner und fokussierter; `SafetyCore` als Name würde
weiterhin die volle alte Verantwortung suggerieren. Dateien:
`safety_core.hpp/.cpp` → `actuation_interlock.hpp/.cpp`;
`test/test_safety_core/` → `test/test_actuation_interlock/`. Betroffene
Referenzstellen (real gezählt, Abschnitt 4.3): `fermentation_application.
hpp/.cpp`, `temperature_control_orchestrator.hpp/.cpp`,
`main/issue_29_bringup_probe.cpp`,
`test/test_run_persistence_coordinator/…cpp`,
`test/test_issue90_product_recovery_oracle/…cpp`,
`test/esp_idf_nvs_adapter_host/main/test_nvs_state_store.cpp` – ausschließlich
mechanisches Rename des Typnamens, keine Vertragsänderung an diesen
Aufrufstellen außer dem in Abschnitt 12 beschriebenen neuen
`IStateStore&`-Parameter.

**Minimaler Input** (`ActuationEvidence`, Nachfolger von `SafetyCoreInput`,
Felder unverändert übernommen minus der bereits durch `BootClassification`
abgedeckten Persistenz-Rohfelder, die weiterhin als bereits klassifizierte
`loadDisposition` durchgereicht werden – kein Duplikat der Klassifikation
im Interlock):

```text
bootValidationComplete, configurationValidated, persistenceValidated,
sensorEvidenceValidated, explicitActivationRequested,
plannerEvidenceValidated, activationKind,
configurationRecoveryStatus, configurationProducer,
configurationServiceMode, configurationCommitStatus,
persistenceLoadStatus, persistenceCoordinatorState,  // UNVERAENDERT aus
  // SafetyCoreInput uebernommen: beides ist laufend gelesene, aktuelle
  // Produzentenwahrheit des RunPersistenceCoordinator (state()/letztes
  // loadAndInitialize()-Ergebnis), kein zusaetzliches Gedaechtnis des
  // Interlocks - das entspricht Korrektur G ("Producer bleiben Autoritaet
  // ihrer technischen Wahrheit") und wird frisch pro evaluate()-Aufruf
  // gelesen, nicht zwischengespeichert.
loadDisposition (aus BootClassification; ersetzt NUR persistenceSnapshot -
  die Klassifikationsentscheidung selbst, nicht die rohe Coordinator-/
  Load-Status-Wahrheit, wandert zu BootClassification, da SafeBoot/
  ResumeOffer/etc. Klassifikation ist, nicht Aktorpermission),
activationPersistenceResult, processActivationApplied,
peltierSensor, sensorSelectionRuntime,
actuatorPlanner (fuer den bestehenden #23-Watchdog-Lesezugriff)
```

**Korrektur gegenüber einem ersten Entwurf:** `persistenceLoadStatus` und
`persistenceCoordinatorState` bleiben **beide** im Interlock-Input, nicht nur
das klassifizierte `loadDisposition`. Grund: `classifyRunLoad(status,
snapshot)` allein reproduziert nicht die Coordinator-Zustandsprüfung, die
heute an drei Stellen zusätzlich gate-relevant ist –
`isTrustedCoordinatorState()` in `activationEvidenceComplete()` und
`canClearFault()`, der `switch (persistenceCoordinatorState)` in
`evaluate()` (Zeile 253–266: `Busy`/`BlockedIndeterminate`/
`PersistenceCommittedApplyFailed`/`Uninitialized` → `RunPersistenceUntrusted`,
unabhängig vom Load-Status) und `isValidFallbackRecoveryEvidence()` (braucht
`FallbackRecovered` **und** `FallbackRecoveryPending` gemeinsam). Ohne die
rohe Coordinator-State-Wahrheit wäre `ALLOWED` erreichbar, während der
Coordinator z. B. `Busy` ist – eine reale fail-closed-Regression. Nur
`persistenceSnapshot` (der Inhalt, den ausschließlich die Klassifikation
konsumiert) verlässt den Interlock-Input.

**Minimaler Output** (`ActuationEvaluation`):

```text
permission: ActuatorSafetyGateStatus  // UNVERAENDERT 3-wertig, Abschnitt 10.1
disposition: SafetyDisposition        // UNVERAENDERT (Information/
                                       //   BlockedImmediateStop/SafeBoot)
faultCode: FaultCode                  // UNVERAENDERT, RAM-only (4.3)
```

### 10.1 Korrektur H – geschlossene Entscheidung: **kein Enum-Wechsel**

`ActuatorSafetyGateStatus{Unresolved, Allowed, ImmediateStop}` bleibt
**unverändert 3-wertig**. Begründung (Abschnitt 4.7, real belegt): Der
bestehende Vertrag trennt Permission (das 3-wertige Gate), Grund
(`ActuatorPlanReason`, u. a. `SafetyGateUnresolved` vs.
`ExternalSafetyOverride`) und physische Reaktion (`rejectToIdle` +
Lüfternachlauf) bereits vollständig – `ActuatorPlanner::runPhaseB()` behandelt
beide Denial-Fälle strukturell identisch (`rejectToIdle`), unterscheidet sie
aber im Diagnosegrund. Ein Wechsel auf ein 2-wertiges `Permission{Denied,
Allowed}` würde am Interlock-Ausgang eine **zusätzliche** Projektionsfunktion
zurück auf die 3 Planner-Werte erfordern (Denied+"noch nie evaluiert" →
Unresolved, Denied+"aktiver Trip" → ImmediateStop) und damit **mehr**, nicht
weniger, Typen erzeugen – exakt die von Korrektur H verbotene „rein
kosmetische Enum-Vereinfachung". `ImmediateStop` entfällt daher **nicht**;
der bestehende #23/#24-Peltier-Sofortsperr-/Lüfternachlaufvertrag bleibt
byteidentisch erhalten, weil an `ActuatorPlanner`/`ActuatorPlanSinkDriver`
gar nichts geändert wird.

### 10.2 Korrektur G – Keep/Move/Drop je Element

| Element | Urteil | Begründung |
|---|---|---|
| `SafetyDisposition` | **Keep** (im Interlock) | Ist exakt die „physische Reaktion"-Kategorie aus Korrektur H; wird von `ActuatorPlanSinkDriver`-Aufrufern zur Reaktionswahl gebraucht |
| `activeFaultMask_` | **Drop** | Interlock wird reiner Funktion des aktuellen `ActuationEvidence`; die Maske speicherte nur „war je gesetzt", was `canClearFault()` (bereits rein aus `input`) redundant macht |
| `acknowledgedFaultMask_` | **Move** (nach `PresentationState`, Abschnitt 11) | Eigener Kommentar bereits: „presentation state only … never changes the gate" – gehört laut Korrektur G nur bei konkretem technischem Grund in den Interlock; ein solcher Grund besteht nicht |
| `unknownProducerSources_` | **Drop** | Der `if (!provided) return`-Frühausstieg ist genau die von G verbotene zweite Wahrheitsquelle (Erinnerung an einen Producer, der aktuell gar nichts mehr meldet); ein vollständiger, frischer Evidence-Snapshot pro Tick ersetzt dies stateless: fehlendes/unbekanntes Pflichtfeld verweigert sofort, ohne Gedächtnis |
| `canClearFault()` | **Drop** | Ohne Maske nichts zu „clearen"; die Freshness-Prüfungen (`hasFreshConfigurationEvidence` etc.) bleiben als freie Hilfsfunktionen erhalten und werden direkt in `evaluate()` verwendet |
| `resetRequestWatchdog()` | **Keep**, aber ohne Maskenzugriff | Bleibt als freie Funktion/Methode; Vorbedingung wird direkt aus frischer `ActuationEvidence` + `planner.state().latchedWatchdogFault` neu berechnet statt `activeFaultMask_` zu lesen; `ActuatorPlanner::applyExternalWatchdogFaultReset()` bleibt einziger physischer Reset-Aufruf (#23-Latch bleibt beim Planner, unverändert) |
| `FaultCode` | **Keep** (unverändert, RAM-only, kein Wire-Vertrag, 4.3) | Bleibt Diagnose-Enum; `dispositionForFault()`-Mapping unverändert übernommen |

### 10.3 `evaluate()` als reine Funktion

`ActuationInterlock::evaluate(const ActuationEvidence&)` wird
`[[nodiscard]] static` (kein Objektzustand außer optional `resetCause_`, das
rein deskriptiv ist und ebenfalls an die Presentation-Grenze verschoben
werden kann – Composition übergibt `resetCause` direkt aus
`IResetCauseSource` an die Presentation, nicht durch den Interlock). Die
bestehenden Hilfsfunktionen (`isPersistenceSafeBoot`,
`hasFreshConfigurationEvidence`, `hasFreshSensorEvidence`,
`hasFreshIntegrityEvidence`, `hasResolvedCommitEvidence`,
`isTrustedCoordinatorState`, `isValidFallbackRecoveryEvidence`) werden
unverändert übernommen; sie sind bereits reine Funktionen von `input`.

## 11. Fehler-/Ack-/Watchdog-Ownership

| Wahrheit | Owner | current-boot/persistent | Clear-Autorität | Presentation-Owner | Interlock-Projektion |
|---|---|---|---|---|---|
| Konfigurationsfehler (`ConfigurationUnavailable`/`IntegrityFailure`/`CommitIndeterminate`) | `ConfigurationService`/`ConfigurationRecoveryService` (bestehend) | current-boot (RAM) | Producer selbst (neue frische `ConfigurationRecoveryResult`) | neue `PresentationState` | `SafetyDisposition::SafeBoot` |
| `RunPersistenceUntrusted` | `RunPersistenceCoordinator`/`BootClassification` | current-boot (RAM, Klassifikation aus geladenem Snapshot) | neuer Load-Zyklus (kein Runtime-Clear) | `PresentationState` | `SafetyDisposition::SafeBoot` |
| `SafetySensorUnavailable` | Sensor-Pipeline (#20/#21, bestehend) | current-boot/laufend (RAM) | Producer selbst (frische `SensorQualitySnapshot`) | `PresentationState` | `SafetyDisposition::BlockedImmediateStop` |
| `ActuatorRequestWatchdog` | **ausschließlich `ActuatorPlanner`** (`state().latchedWatchdogFault`) | current-boot (RAM, nicht persistiert, #24-Vertrag unverändert) | `ActuatorPlanner::applyExternalWatchdogFaultReset()`, nur über `ActuationInterlock::resetRequestWatchdog()` | `PresentationState` | `SafetyDisposition::BlockedImmediateStop` |
| Ack (Nutzer hat Fehler gesehen) | **neu: `PresentationState`** (aus Interlock verschoben) | current-boot (RAM) | überschrieben durch neuen Fault oder expliziten Reset | `PresentationState` selbst | keine (fließt nie in `evaluate()` zurück) |

`PresentationState` ist ein neuer, sehr kleiner Typ (ein `FaultCode` +
`bool acknowledged` + optional `ResetCause`), der von `FermentationApplication`
gehalten wird (Device/Application-Lifecycle-Ebene, Abschnitt 5) – kein neues
Modul, keine neue Registry, entspricht Korrektur D.

## 12. Composition (`FermentationApplication`, kleinste Fassung)

**Entscheidung (14.9): Application-interne Fassade, kein neuer Root-Helper.**
#120 hatte einen privaten `composeAndBeginApplication(...)`-Helfer direkt in
`main/app_main.cpp` eingeführt; dieser Plan baut nicht auf #120 auf
(Abschnitt 2) und wählt stattdessen die von der Ownervorgabe favorisierte
Richtung: Der Objektgraph entsteht **innerhalb von
`FermentationApplication::begin()`**, nicht im Composition Root selbst. Der
Root (`main/app_main.cpp`) ändert sich minimal: er übergibt zusätzlich die
bereits vorhandene `NvsOwningContext`-Store-Referenz an `begin()`.

```text
main/app_main.cpp (geaendert, minimal):
  NvsOwningContext::create()  // unveraendert
  application.begin(platform, stateStoreContext->store(), &resetCauseSource)
    // NEU: ein zusaetzlicher IStateStore&-Parameter, exakt das Muster,
    // das ConfigurationBootstrapStore/ConfigurationGraphStore/
    // RunPersistenceCoordinator/ConfigurationRecoveryService::create
    // bereits fuer denselben Store verwenden (4.5)

FermentationApplication::begin(platformServices, store, resetCauseSource):
  1. bootstrapStore_, graphStore_ (bestehende Typen) mit store konstruieren
  2. ConfigurationRecoveryService::create(store, bootstrapStore_, graphStore_,
     configurationService_, mutationCoordinator_) -> boot()
  3. RunPersistenceCoordinator(store, epoch, schedule) -> loadAndInitialize()
  4. boot_classification::classify(configResult, loadResult)
     -> BootClassification
  5. TemperatureControlApplicationOrchestrator(persistence_, controller_,
     evaluator_, planner_, driver_, interlock_)  // 6-Parameter-Form,
     RunRecoveryCoordinator NICHT konstruiert/uebergeben (Korrektur B)
  6. je nach BootClassification (Abschnitt 7):
     - NoRun/DiscardableRun/UntrustedConfiguration/UntrustedPersistence/
       CompletedRun/TerminalRunFault: sofort Application-Lifecycle setzen,
       ProcessRuntimeState ggf. published, Actuation bleibt DENIED
     - ResumeOffer: nichts weiter tun, auf Nutzerentscheidung warten
  7. ActuationInterlock::evaluate(evidence) mit den realen Werten aus
     Schritt 2-4 (kein leeres SafetyCoreInput mehr wie heute, 4.1)
```

Kein neuer Objektgraph im `main/app_main.cpp`-Root außer der einen
zusätzlichen Store-Referenz; `NvsOwningContext` bleibt unverändert
Eigentümer der Partitions-/Store-Lebenszeit (ADR-013, unverändert). Kein
DI-Framework, kein Service Locator (Korrektur D).

**Lebenszeit der komponierten Objekte – Präzisierung gegenüber #120:** Anders
als #119/#120s rein *boot-only* Objekte (dort: `RunPersistenceCoordinator`/
`RunPersistenceLoadResult` mit Lebenszeit nur bis Boot-Ende) müssen die
meisten hier komponierten Objekte für die **gesamte Laufzeit** existieren,
nicht nur bis zum Boot-Abschluss: `RunPersistenceCoordinator`,
`ConfigurationService`, `ActuatorPlanner`, `ActuatorPlanSinkDriver`,
`TemperatureController`, `TargetQualificationEvaluator` und
`TemperatureControlApplicationOrchestrator` werden von jedem
`persistCommand`/`tick()`/`evaluateTemperatureControl()`-Aufruf während der
gesamten Laufzeit benötigt (Abschnitt 4.9). Nur `ConfigurationRecoveryService`
selbst ist genuin boot-only (nur `boot()` wird aufgerufen, danach nicht mehr
gebraucht).

Diese Laufzeit-Objekte werden deshalb **heapbesitzende Member von
`FermentationApplication`** (`std::unique_ptr<T>`, analog zum bereits in
R1.1 etablierten Muster für `RunPersistenceCoordinator`/
`RunPersistenceLoadResult`, hier aber mit Application-Lebenszeit statt
Boot-only-Scope), in `begin()` per `new (std::nothrow)` konstruiert – nicht
als Werte-Member und nicht als lokale Stack-Objekte in `begin()`.
`sizeof(FermentationApplication)` bleibt dadurch unabhängig von der Größe
des komponierten Graphen klein (nur Zeiger), wodurch weder
`app_main()`s Stackframe (dort liegt weiterhin nur die eine
`FermentationApplication application;`-Instanz, unverändert wie heute)
noch `begin()`s eigener Stackframe in dieselbe Größenordnung wie #120s
16.432-Byte-Befund wachsen kann. Fehlgeschlagene Allokation ist explizit
fail-closed: `begin()` gibt `false` zurück, exakt das bestehende
`FermentationApplication::begin()`-Fehlerpfadmuster (Abschnitt 4.1), analog
zum bereits etablierten `BOOT_COMPOSITION_ALLOCATION_FAILURE`-Vertrag aus
R1.1.

**Abnahmemetrik (Korrektur zu einer ersten Fassung):** Nicht nur
`app_main()`s eigener Entry-Frame (der bleibt beim heutigen, real gemessenen
`BASE_SHA`-Wert von 112 Byte, Abschnitt 4.8, weil sich an `app_main.cpp`
außer dem einen zusätzlichen Parameter nichts ändert), sondern **zusätzlich
und maßgeblich** der reale Main-Task-Stack-High-Watermark über den
vollständigen `begin()`-Aufruf plus mindestens einen Laufzeit-Tick
(`uxTaskGetStackHighWaterMark`, bereits heute in `logResources()`
verwendet) gegen `CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584` (Abschnitt 16).
Ein alleiniger Verweis auf den unveränderten Entry-Frame würde das
tatsächliche #120-Risiko (großer Objektgraph irgendwo im Main-Task-Aufruf-
pfad, nicht zwingend im allerersten Frame) fälschlich als bereits
ausgeräumt darstellen.

## 13. Teststrategie

Aufbauend auf realer, bestehender Testinfrastruktur (Abschnitt 4, real
gezählt): `test/test_safety_core`, `test/test_process_state_machine`,
`test/test_run_persistence_coordinator`, `test/test_configuration_recovery_
service`, `test/test_actuator_planner`, `test/test_actuator_plan_sink_driver`,
`test/test_issue90_product_recovery_oracle`.

- `test/test_safety_core` → `test/test_actuation_interlock`: bestehende
  Testfälle werden auf den neuen, kleineren `ActuationEvidence`/
  `ActuationEvaluation`-Vertrag migriert; Fälle, die ausschließlich
  Boot-/Persistenz-Klassifikation prüften, wandern in einen neuen
  `test/test_boot_classification`.
- **Neu:** `test/test_boot_classification` – reine Matrix über
  `classifyRunLoad`/`isR1ResumeEligible` (unverändert aus `safety_core.cpp`
  übernommen) für alle 8 Flows aus Abschnitt 7, inklusive der drei
  Legacy-Wire-Werte (Abschnitt 6: `Boot`/`SafeBoot`/`RecoveryEvaluation`
  im geladenen Snapshot → `DiscardableRun`).
- `test/test_process_state_machine`: **unverändert lauffähig**, da
  `process_state_machine.hpp/.cpp` nicht verändert wird; ergänzt um Fälle,
  die bestätigen, dass `decideBootEvent`/`ServiceMode`-Übergänge weiterhin
  funktionieren (falls je manuell angesteuert), aber von der neuen
  Composition nicht mehr erreicht werden.
- Fresh Start: bestehende `TemperatureControlApplicationOrchestrator`-Tests
  (falls vorhanden) bzw. neue Integrationstests über
  `persistFreshStartCommand()` mit realer `ActuationInterlock`-Bindung.
  Reject-Pfad ohne `Applied`: `DENIED` bleibt.
- Resume Offer/Confirm/Reject: neue Integrationstests über
  `activateLoadedRun()`/`discardAsNoActiveRun()` + Interlock-Reevaluation
  nach `Applied`.
- Untrusted persistence / Config failure / Sensor failure: bestehende
  `SafetyCoreInput`-Fallmatrix (jetzt `ActuationEvidence`) bleibt inhaltlich
  gültig, nur Typnamen migrieren.
- Watchdog: bestehende `resetRequestWatchdog()`-Tests migrieren auf die
  maskenfreie Vorbedingung (Abschnitt 10.2); #23-Latch-Eigentum bei
  `ActuatorPlanner` bleibt durch bestehende `test/test_actuator_planner`
  abgedeckt (unverändert).
- Actuation `DENIED`-Default: neuer Test, dass ein frisch konstruierter
  `ActuationEvidence{}` (alle Felder default) `permission ==
  ActuatorSafetyGateStatus::Unresolved` liefert (Nachfolgetest von
  `FermentationApplication::begin()`s heutigem leerem
  `SafetyCoreInput`-Test).
- Kein `ALLOWED` ohne `Applied` + aktuelle Safety-Evidenz: bestehende
  `activationEvidenceComplete()`-Fallmatrix migriert unverändert.
- Planner/Sink fail-closed: unverändert, `test/test_actuator_planner` und
  `test/test_actuator_plan_sink_driver` bleiben unangetastet (Abschnitt 4.7).
- Architekturgrenzen: `scripts/check_architecture_boundaries.py` bleibt
  grün (kein neuer `device_platform`→`fermentation_app`-Pfad).
- ESP-IDF-Builds: `esp32_bringup`/`esp32_release` real bauen, Stack-/
  Heap-Nachweis für den neuen `begin()`-Graphen (Abschnitt 12).
- Realer actor-free Hardwareboot: erst nach vollständigem Software-
  Ownerreview dieses Plans plus Umsetzung, als eigenes Owner-Gate
  (Abschnitt 15, Schritt 3).

## 14. Scope-Abgrenzung

### In Scope

Composition-Lücke schließen (`IStateStore` real bis `FermentationApplication`
durchreichen); `BootClassification` extrahieren; `SafetyCore` →
`ActuationInterlock` verkleinern und umbenennen; `PresentationState` für
Ack/FaultCode einführen; `TemperatureControlApplicationOrchestrator` real
komponieren (ohne `activateRecovery`); zugehörige Tests migrieren/ergänzen.

### Nicht Scope (unverändert aus Ownervorgabe Abschnitt 15 übernommen)

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
```

Zusätzlich, aus der realen Inventur (Abschnitt 4) neu erkannt: keine
Änderung an `run_persistence_codec.cpp`, `ActuatorPlanner`,
`ActuatorPlanSinkDriver`, `RunPersistenceCoordinator`,
`ConfigurationRecoveryService`, `ConfigurationService`,
`process_state_machine.hpp/.cpp` (nur Aufrufer-seitig weniger Events
gesendet, keine Signaturänderung).

## 15. Umsetzungsschritte (owner-gated, keine Umsetzung in dieser Runde)

```text
Schritt 1: boot_classification.hpp/.cpp extrahieren (reiner Code-Zug aus
  safety_core.cpp, keine Verhaltensaenderung), zugehoerige Tests migrieren.
Schritt 2: safety_core.hpp/.cpp -> actuation_interlock.hpp/.cpp umbenennen
  und verkleinern (Abschnitt 10.2), PresentationState einfuehren
  (Abschnitt 11), alle Referenzstellen (Abschnitt 10) mechanisch anpassen.
Schritt 3: FermentationApplication::begin() um die Composition-Fassade
  erweitern (Abschnitt 12); main/app_main.cpp minimal um den
  IStateStore&-Parameter erweitern.
Schritt 4: Teststrategie umsetzen (Abschnitt 13).
Schritt 5: reale ESP-IDF-Builds + Stack-/Heap-Nachweis; danach eigenes
  Owner-Gate fuer einen realen actor-free Hardwareboot.
```

Jeder Schritt erfordert eine eigene Owner-Freigabe vor Beginn, gemaess
`docs/AGENT_WORKFLOW.md`. Diese Planrunde autorisiert keinen der Schritte.

## 16. Abnahmekriterien

```text
FAIL_CLOSED_BOOT_POLICY=<PASS|FAIL>          -- Korrektur A, getrennt gefuehrt
PRODUCT_BOOT_COMPLETION=<PASS|FAIL|BLOCKED>  -- Korrektur A, getrennt gefuehrt
PHYSICAL_BOOT_OUTPUT_SAFETY=PENDING          -- bleibt #29-Gate, unberuehrt
ARCHITECTURE_BOUNDARIES=<PASS|FAIL>
CONFIGURATION_RECOVERY_TEST=<PASS|FAIL>
RUN_PERSISTENCE_TEST=<PASS|FAIL>
ACTUATION_INTERLOCK_TEST=<PASS|FAIL>          -- Nachfolger SAFETY_CORE_TEST
BOOT_CLASSIFICATION_TEST=<PASS|FAIL>          -- neu
ESP_IDF_BRINGUP_BUILD=<PASS|FAIL>
ESP_IDF_RELEASE_BUILD=<PASS|FAIL>
APP_MAIN_ENTRY_FRAME_UNCHANGED=<PASS|FAIL>     -- ggü. 112 Byte, Abschnitt 4.8/12
MAIN_TASK_STACK_HIGH_WATERMARK=<PASS|FAIL|NOT_MEASURED>  -- ueber begin()+Tick,
                                                          -- massgeblich, Abschnitt 12
BREAKING_PERSISTENCE_CHANGE=NO
SCHEMA_MIGRATION_REQUIRED=NO
REAL_HARDWARE_BOOT=NOT_RUN                    -- eigenes Owner-Gate, Schritt 5
```

## 17. Statuszusammenfassung

```text
PLAN_PATH=docs/tasks/issue-121-lifecycle-safety-simplification-plan.md
ARCHITECTURE_VERDICT=SIMPLIFY
BREAKING_PERSISTENCE_CHANGE=NO
SCHEMA_MIGRATION_REQUIRED=NO
IMPLEMENTATION=NOT_STARTED
MATERIAL_ARCHITECTURE_DECISION_OPEN=NO
```

STOP – Owner Full Plan Review. Keine Implementation vor Freigabe der exakten
Plan-SHA.
