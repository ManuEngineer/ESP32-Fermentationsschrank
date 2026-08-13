# Plan: Issue #23 – Aktorplaner, Mindestzeiten, Totzeit und Lüfterlogik

## 1. Status, Scope und Owner-Gate

- Revision: **3**. Ersetzt Revision 2 (`892ac9c0ceec122253a2fa280b3624aaf7e712b4`)
  vollständig. Diese Revision ist ohne Rückgriff auf Revision 1 oder 2
  vollständig ausführbar und reviewbar.
- Live-Issue: #23, offen, Status `PLANNED_SPEC_PENDING`.
- Draft-PR: #105, Branch `agent/issue-23-aktorplaner-plan` -> `main`.
- Planpfad: `docs/tasks/issue-23-actuator-planner-plan.md`.
- Planbasis: `main` @ `2986dca5736a34171910c9245a3d5f43fa55da06`
  (Merge-Commit von PR #104 / Issue #22, unverändert seit Revision 1).
- Neu in dieser Revision: Issue **#106** „Aktorplaner Per-Run-Parameter-
  Snapshot und Recovery-Bindung" wurde als konkretes, nachverfolgtes
  Tracking-Issue angelegt (Owner-Entscheidung, siehe R2.6 unten) und ist
  Abhängigkeit dieses Plans für die produktive Verdrahtung.
- Die Umsetzung bleibt gesperrt, bis der Owner exakt diesen neuen
  Revision-3-Plan-Commit mit `PLAN APPROVED: <SHA>` freigibt.
- Diese Revision committet ausschließlich Plandokumentation. Sie implementiert
  keine Produktionslogik, keine produktiven Tests, keine Hardware-, GPIO-,
  Toolchain- oder CI-Änderung.
- Der PR bleibt Draft. Es gibt kein `Ready for review`, keinen Merge, kein
  Auto-Merge und kein Branch-Löschen. Issue #23 wird nicht geschlossen.

```text
CONTEXT_BASELINE_BRANCH: agent/issue-23-aktorplaner-plan
CONTEXT_BASELINE_SHA: 2986dca5736a34171910c9245a3d5f43fa55da06
CONTEXT_HEAD_BEFORE_REVISION: 1492490 (Roadmap-Metadaten-Commit nach Revision 2)
CONTEXT_PLAN_SHA: NONE (wird nach dem Commit dieser Revision eingetragen)
CONTEXT_REFRESH_MODE: FULL
CONTEXT_DELTA: Vollständiges Owner-Planreview von Revision 2 mit acht
  Blocker-Befunden (R2.1–R2.8) erhalten und in dieser Revision vollständig
  gelöst: R2.1 nicht-zirkulärer Fensterbeginn (Abschnitt 8.1); R2.2 explizite
  neue Unavailable/InvalidInput-Evaluation überstimmt Mindest-Einschaltzeit
  sofort (Abschnitt 8.2 Rang 6); R2.3 Feedback-Subjekt ist ausschließlich
  `acceptedCommand`, kein „alte Sequenz"-Konzept mehr, `ActuatorAdmissionOutcome`
  als eigener typisierter Diagnosevertrag getrennt von `ActuatorPlanReason`
  (Abschnitt 6, 9); R2.4 Watchdog-Fault-Latch wird von keiner Lifecycle-Grenze
  und keinem Neustart mehr implizit gelöscht, Methode umbenannt in
  `applyExternalWatchdogFaultReset()` (Abschnitt 8.6, Vokabular aus
  `SAFETY_AND_FAULTS.md`); R2.5 vollständiger Parametervertrag tatsächlich
  ausgeschrieben (Abschnitt 13), Rang-2-Widerspruch beseitigt (einheitlicher
  Verwurf); R2.6 boot-session-fixes, nicht pro-Lauf-rekonstruiertes
  Objektmodell (löst den Referenz-/Rebinding-Widerspruch) plus neues,
  konkretes Tracking-Issue #106 als benanntes Blockiergate vor produktiver
  Verdrahtung (Abschnitt 14) – Owner-Entscheidung per Rückfrage eingeholt,
  nicht selbst improvisiert; R2.7 test-only gemeinsamer Call-Trace für
  Cross-Sink-Reihenfolge (Abschnitt 12); R2.8 overflow-sichere O(1)-
  Fensterarithmetik über Ganzzahldivision statt wiederholter Addition oder
  Iterationsobergrenze (Abschnitt 8.1), inklusive expliziter Nicht-Nachhol-
  Semantik für lange Pausen. Zusätzlich beim eigenen Review entdeckt und
  behoben: unbegrenztes Überleben des Impulsguthabens über beliebig lange
  Idle-Phasen (neue Regel: jeder Übergang von Heating/Cooling nach Idle
  verwirft Akkumulator, Fensterzustand und Gegenrichtungskandidat
  unbedingt, Abschnitt 8.4) sowie ein zweiphasiges Annahme-/Prioritätsmodell
  (Abschnitt 8.2), das Sequenz-/Watchdog-Bookkeeping unabhängig von
  Parameter-/Safety-/Fault-Zustand konsistent hält. `docs/tasks/issue-22-pi-control-air-limits-plan.md`
  Abschnitt 8.2 „Feedbackfenster" wurde für diese Revision gezielt erneut
  gelesen und ist die verbindliche Grundlage für die R2.3-Lösung.
SOURCE_OF_TRUTH_CONFLICT: NONE.
```

## 2. Ziel, Reihenfolge und Nicht-Ziele

Issue #23 liefert einen deterministischen, hardwarefreien und nativ testbaren
Aktorplaner, der eine gültige abstrakte `ControlRequest` aus Issue #22 in
zeitlich korrekte, abstrakte Aktorbefehle für Peltier und Lüfter übersetzt:

- gemeinsames zeitproportionales Schaltfenster für Heizen und Kühlen;
- begrenzter, einzelner, richtungsgebundener Impulsakkumulator für
  Anforderungen unterhalb der Mindest-Einschaltzeit;
- Mindest-Einschaltzeit mit Vorrang für Sicherheits-/Fehlerabschaltung;
- Mindest-Auszeit vor erneuter Freigabe;
- bestätigte Gegenrichtungsanforderung (Schwelle + Dauer + Plausibilität) vor
  Richtungswechsel;
- sichere Polaritätstotzeit, kombiniert mit Mindest-Auszeit über
  Späteres-Ende-Regel (keine Addition);
- Außenlüfter ohne absichtlichen Vorlauf, mit Nachlauf;
- Innenlüfter während temperaturgeregelter Phasen und mit eigenem Nachlauf;
- Aktualisierungs-Watchdog für veraltete oder kontextfremde `ControlRequest`
  mit latched Fault-Evidenz für #24 (kein stilles Verschwinden, keine
  implizite Wiederfreigabe);
- ausschließlich abstrakte Aktorbefehle (Richtung/Lüfterzustand plus
  Diagnosegrund) als Ausgabe des fachlichen Kerns – keine GPIO-Ansteuerung im
  Fachkern.

Die fachliche Reihenfolge bleibt:

```text
#21 persistierter Laufmodus und Sensorselektion
  -> #22 effektive Prozessrolle, PI und abstrakte ControlRequest
  -> #23 Aktorplanung (dieser Plan)
  -> #24 Safety-/Fehlerkern
```

### Nicht-Ziele dieses Plans

- Keine GPIO-Zuordnung, keine BTS7960-Ansteuerung, kein ESP-IDF-Adapter für
  Peltier oder Lüfter (bleibt in `device_platform_esp_idf`, kein
  Release-1-Scope dieses Plans).
- Keine eigene Safety-/Fehlerklassifikation, keine Kühlkörper-Grenzwertlogik
  und keine absolute Temperatursicherheit; das bleibt Issue #24
  (`SAFETY_AND_FAULTS.md`). #23 erzeugt für #24 ausschließlich konsumierbare
  Evidenz (Abschnitt 8.6), keine Ursache, keine Wiederfreigabeentscheidung
  und keinen eigenen Fehlerreset.
- Keine Änderung an der PI-/Luftbegrenzungslogik aus Issue #22
  (`temperature_control.*`, `control_context.*`); `ControlRequest`,
  `ControlRequestContext`, `ControlSensorRole`, `AbstractControlDirection`,
  `TemperatureControlResult` und `PreviousControlRequestFeedback` werden
  ausschließlich wiederverwendet.
- Keine konkreten Sekundenwerte für Schaltfenster, Mindestzeiten, Totzeit,
  Umschaltschwelle, Akkumulatorgrenze oder Nachlaufzeiten; diese bleiben
  `TBD_COMMISSIONING` (Nachverfolgung #35 / `OPEN_POINTS.md`). Der Plan legt
  die vollständige Algorithmus- und Prioritätssemantik fest, nicht die
  Zahlen.
- Keine Persistenz des Aktorplanerzustands; er bleibt RAM-only wie der
  PI-/Qualifier-Zustand aus Issue #22 und wird an denselben kanonischen
  Lifecycle-Grenzen über dieselbe Application-Grenze zurückgesetzt
  (Abschnitt 11). Kein `run_persistence_*`-Schema wird geändert. Die
  Per-Run-Parameterbindung ist **bewusst nicht Teil dieses Plans**, sondern
  ein benanntes, blockierendes Integrationsgate über das neu angelegte
  Issue **#106** (Abschnitt 14) – diese Entscheidung wurde dem Owner
  ausdrücklich zur Wahl vorgelegt, nicht einseitig getroffen.
- Keine Änderung an `docs/tasks/issue-22-pi-control-air-limits-plan.md`;
  dessen Verträge werden referenziert, nicht kopiert oder neu definiert.

## 3. Verbindliche Quellen und bereits getroffene Entscheidungen

Vor der Umsetzung sind mindestens diese Quellen erneut gegen ihren dann
aktuellen Stand zu prüfen:

- `docs/SPECIFICATION_REVIEW.md` als Dokumentationspriorität;
- `docs/DECISIONS.md`, insbesondere ADR-013 (Modularchitektur) und ADR-014
  (deterministischer fachlicher Zustandsautomat);
- `docs/AGENT_WORKFLOW.md` und `docs/ENGINEERING_PRINCIPLES.md`;
- `docs/ACTUATOR_TIMING.md` als kanonische Spezifikation für Schaltfenster,
  Impulsakkumulator, Mindestzeiten, Richtungswechsel, Totzeit,
  Integratorkopplung, Außen-/Innenlüfter und Watchdog;
- `docs/RUNTIME_BEHAVIOR.md` für Lüfterrollen, Richtungswechselsequenz und
  Meldungspriorität (informativ, keine Aktorlogikänderung);
- `docs/TEMPERATURE_CONTROL.md`, insbesondere „Architekturgrenzen" und der
  implementierte #22-Fachkern-Abschnitt (Status-/Reason-/AirLimit-Matrix);
- `docs/STATE_MACHINE.md` für `ProcessState`-Topologie, Recovery- und
  Lifecycle-Grenzen;
- `docs/SENSOR_TUNING_COMMISSIONING.md` für Integratorregeln und
  Commissioning-Eigentümerschaft;
- `docs/SAFETY_AND_FAULTS.md`, insbesondere die getrennten Begriffe
  „Quittieren" und „Fehlerreset beziehungsweise automatische Wiederfreigabe"
  – maßgeblich für die Benennung und Abgrenzung der #23-Watchdog-Evidenz
  (Abschnitt 8.6);
- `docs/RUN_PERSISTENCE.md` für das Write-before-Apply-Muster (informativ;
  #23 besitzt keinen eigenen Persistenzvertrag, siehe Abschnitt 14);
- `docs/tasks/issue-22-pi-control-air-limits-plan.md`, insbesondere Abschnitt
  4.1 (Zuständigkeiten #22/#23/#24), Abschnitt 7.1 (Request-Identität und
  Kontextfrische), Abschnitt 7.2 (Status-/Reason-/AirLimit-Matrix) und
  Abschnitt **8.2 „Feedbackfenster"** (Zeilen 698–719) – dieser Abschnitt ist
  die unmittelbare, wörtlich zu beachtende Grundlage für die
  Feedback-Dispositionslogik in Abschnitt 9 dieses Plans;
- ADR-013 und der Modulindex für die Abhängigkeitsrichtung
  `fermentation_app -> device_platform`;
- `docs/THIRD_PARTY_COMPONENTS.md`, Zeile „Regelung" (Arduino PID / QuickPID
  `NOT_SELECTED`; eigene Logik bleibt Vertrag);
- `docs/CI_AND_QUALITY_GATES.md` und `docs/AGENT_WORKFLOW.md` für Testbefehle
  und Reviewpflichten;
- `AGENTS.md` (root) sowie `lib/fermentation_app/AGENTS.md` und
  `lib/device_platform/AGENTS.md`;
- **Issue #106** (neu, „Aktorplaner Per-Run-Parameter-Snapshot und
  Recovery-Bindung") als benanntes Integrationsgate, siehe Abschnitt 14.

Bestehende Implementierungen und Tests wurden vor dieser Planung durchsucht.
Wiederzuverwenden sind insbesondere:

| Typ/Funktion | Datei | Verwendung in #23 |
|---|---|---|
| `ControlRequest`, `ControlRequestIdentity` | `temperature_control_types.hpp` | Quelle für Sequenz/Timestamp/Richtung/Quote der neuen Evaluation |
| `ControlRequestContext`, `ControlSensorRole` | `control_context_types.hpp` | Kontextfrischeprüfung gegen aktuellen kanonischen Kontext |
| `AbstractControlDirection` | `sensor_selection_types.hpp` | **einziges** Richtungs-Enum in #23 |
| `PreviousControlRequestFeedback`, `Disposition` | `temperature_control_types.hpp` | Ausgabe des Planers zurück an #22 (Abschnitt 9) |
| `TemperatureControlResult`, `TemperatureControlStatus`, `TemperatureControlReason`, `AirLimitState` | `temperature_control_types.hpp` | vollständige Eingabe je Tick mit neuer Evaluation; Quelle der kanonischen AirLimit-Klassifikation (Abschnitt 7) |
| `isTemperatureControlledProcessState()` | `control_context.hpp` | Innenlüfter-Phasenklassifikation, keine Parallel-Klassifikation |
| `TemperatureControlLifecycleBoundary` | `temperature_control_orchestrator.hpp` | wiederverwendete Lifecycle-Grenzen (kein zweiter Enum) |
| `TemperatureControlApplicationOrchestrator` | `temperature_control_orchestrator.hpp/.cpp` | wird erweitert (nicht dupliziert), einzige Application-/Lifecycle-Grenze auch für #23 (Abschnitt 11) |
| `TemperatureControlEvaluationEvidence::previousControlRequestFeedback` | `temperature_control_orchestrator.hpp` | bereits vorhandenes Eingabefeld für #22; wird von #23s Feedback exakt befüllt, kein neuer Mechanismus |
| `IBidirectionalActuatorSink` | `device_platform/bidirectional_actuator_sink.hpp` | bereits vorhandener, bisher unbenutzter Peltier-Port |
| `IBinaryOutputSink` | `device_platform/binary_output_sink.hpp` | bereits vorhandener, bisher unbenutzter Lüfter-Port (zwei Instanzen: außen/innen) |
| `ITimeSource`, `VirtualTimeSource` | `device_platform/time_source.hpp`, `virtual_time_source.hpp` | monotone Zeit im Aufrufer; native Tests |
| `MockBidirectionalActuatorSink`/`MockBinaryOutputSink` (inkl. `commandJournal()`, `simultaneousActivationObserved()`) | `device_platform_test_support/` | bereits vorhandene Test-Doubles; für Cross-Sink-Reihenfolge zusätzlich mit test-only Decorator-Sinks kombiniert (Abschnitt 12) |

Kein paralleler Sensor-, Prozess-, Persistenz- oder PI-Vertrag wird erfunden.
Insbesondere wird `ControlRequestContext` nicht kopiert, sondern exakt wie in
Issue #22 als flüchtige Identität mitgeführt und geprüft.

## 4. Zuständigkeiten und Architekturgrenzen

### 4.1 Abgrenzung zu Issue #22

Issue #22 liefert ausschließlich `HEAT`/`OFF`/`COOL` mit Zeitquote,
Identität, Kontext und der vollständigen `TemperatureControlStatus`/
`TemperatureControlReason`/`AirLimitState`-Matrix. Issue #22 kennt weder
Schaltfenster noch Mindestzeiten noch Lüfter und behauptet keine physische
Aktorquote. Dieser Plan ändert an `temperature_control.*` und
`control_context.*` nichts. Die Feedbacksemantik in Abschnitt 9 folgt
wörtlich `docs/tasks/issue-22-pi-control-air-limits-plan.md` Abschnitt 8.2
„Feedbackfenster".

### 4.2 Abgrenzung zu Issue #24

Issue #23 trifft keine Aussage darüber, *warum* eine Sicherheitsabschaltung
verlangt wird oder wann ein Fehler wieder freigegeben werden darf, und
implementiert **keinen eigenen Fehlerreset**. Dafür werden zwei schmale,
reine Werttypen definiert (keine neuen Interfaces):

```cpp
enum class ActuatorSafetyGateStatus : std::uint8_t {
    Unresolved,     // Default; kein #24-Urteil für diesen Zyklus vorhanden
    Allowed,        // #24 (oder Interims-Composition-Root) erlaubt normale Planung
    ImmediateStop,  // #24 verlangt sofortige Abschaltung
};

struct ActuatorSafetyGateInput {
    ActuatorSafetyGateStatus status{ActuatorSafetyGateStatus::Unresolved};
};

struct ActuatorWatchdogFaultEvidence {
    std::uint64_t detectedAtMonotonicMillis{0U};
    std::uint64_t lastAcceptedSequenceBeforeFault{0U};
};
```

`ActuatorSafetyGateInput` trägt keine Fehlerklasse und keine Ursache. Bis #24
real verdrahtet ist, bleibt `ActuatorSafetyGateInput` im
Composition-Root-Default `Unresolved`; dies führt strukturell zu `Idle`
(Abschnitt 8.2) und ist **kein** implizites „Safety=erlaubt". Die reale
Verdrahtung eines `Allowed`-Zustands vor Fertigstellung von #24 ist
ausdrücklich als offenes Integrationsgate benannt und nicht Teil dieses
Plans.

`ActuatorWatchdogFaultEvidence` ist reine, für #24 konsumierbare Evidenz.
Ihre Freigabe erfolgt ausschließlich über
`ActuatorPlanner::applyExternalWatchdogFaultReset()` (Abschnitt 8.6) – eine
bewusst **nicht** als „acknowledge" benannte Methode, da
`docs/SAFETY_AND_FAULTS.md` „Quittieren" strikt von „Fehlerreset
beziehungsweise automatische Wiederfreigabe" trennt und Quittieren
ausdrücklich **keine** Aktorsperre lösen darf. Diese Methode wird laut
diesem Plan ausschließlich von #24-getriebener Logik aufgerufen; #23 ruft
sie an keiner Stelle selbst auf (weder bei einer neuen Request noch an
irgendeiner Lifecycle-Grenze noch implizit durch einen simulierten
Neustart).

### 4.3 Modulzuordnung nach ADR-013

Der Aktorplaner kennt `ControlRequest`, `TemperatureControlResult`,
`ControlSensorRole` und `ProcessState` – alles fermentationsspezifische
Typen. Der Planer gehört deshalb konsequent zu `lib/fermentation_app/`, nicht
zu `lib/device_platform/`. Er ist selbst hardwarefrei und referenziert aus
`device_platform` ausschließlich die bestehenden schmalen Ports. Es entsteht
kein neuer `device_platform`-Port und keine Rückwärtsabhängigkeit;
`check_architecture_boundaries.py` bleibt PASS.

## 5. Betroffene Module und voraussichtliche Dateien

Neue Dateien (alle unter `lib/fermentation_app/src/`):

```text
actuator_plan_types.hpp
    Werttypen: ActuatorDemandClass, ActuatorSafetyGateStatus,
    ActuatorSafetyGateInput, ActuatorWatchdogFaultEvidence,
    ActuatorPlannerParameters, ActuatorPlannerParametersValidation,
    ActuatorPlanStatus, ActuatorPlanReason, ActuatorAdmissionOutcome,
    AcceptedControlCommand, PulseAccumulator, ActuatorPlanTickInput,
    ActuatorPlanTickResult, ActuatorPlannerRuntimeState; freie Funktion
    classifyActuatorDemand(), classifyActuatorPlannerParameters()

actuator_planner.hpp / .cpp
    Reine, deterministische Klasse ActuatorPlanner: tick(), forceStop(),
    applyExternalWatchdogFaultReset(), Zugriff auf state()/parameters().
    Kapselt Fenster, Akkumulator, Mindestzeiten, Totzeit, Richtungswechsel,
    Watchdog und Lüfterlogik. Kein Sink-, kein GPIO-Zugriff.

actuator_plan_sink_driver.hpp / .cpp
    ActuatorPlanSinkDriver: dünne, zustandslose Übersetzung eines
    ActuatorPlanTickResult auf genau drei injizierte device_platform-Sinks
    (Peltier, Außenlüfter, Innenlüfter) in der in Abschnitt 12 festgelegten
    Reihenfolge. Wird sowohl von normalen Ticks als auch vom
    Lifecycle-Stop-Pfad verwendet (keine zweite Ausgabewahrheit).
```

Geänderte Dateien (Erweiterung der bestehenden kanonischen Application-Grenze,
keine neue parallele Orchestrierung):

```text
lib/fermentation_app/src/temperature_control_orchestrator.hpp / .cpp
    - Konstruktor zusätzlich mit ActuatorPlanner& und ActuatorPlanSinkDriver&
      (analog zu temperatureController_/evaluator_ injiziert; Lebenszeit
      identisch zur bestehenden TemperatureController&-Injektion, siehe
      Abschnitt 14 – kein Rekonstruktions-/Rebinding-Mechanismus).
    - Neue Methode tickActuatorPlan(const RunCommandState&,
      std::optional<TemperatureControlResult> newEvaluation,
      const ActuatorSafetyGateInput&, std::uint64_t nowMonotonicMillis)
      -> ActuatorPlanTickResult. Leitet currentCanonicalContext und
      temperatureControlledPhase intern über die bereits vorhandenen
      resolveEffectiveControlContext()/isTemperatureControlledProcessState()
      ab (keine Parallel-Ableitung).
    - complete()/needsRuntimeReset() ruft für dieselbe erfolgreiche
      Lifecycle-/Commit-Grenze zusätzlich den Aktorplaner-Stop-Pfad auf
      (Abschnitt 11); keine zweite Caller-Pflicht.
```

Neue Testverzeichnisse:

```text
test/test_actuator_planner/test_actuator_planner.cpp
test/test_actuator_plan_sink_driver/test_actuator_plan_sink_driver.cpp
    inklusive test-only SharedActuatorCallTrace-Hilfstyp und zwei
    Decorator-Sinks (Abschnitt 12), lokal in diesem Testverzeichnis, keine
    Produktions- oder device_platform_test_support-Erweiterung.
```

Geändertes Testverzeichnis (bereits vorhandener direkter Konsument von
`TemperatureControlApplicationOrchestrator`):

```text
test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp
    - gezielte Ergänzung um die neuen Lifecycle-/Stop-Fälle der
      Aktorplaner-Integration (Abschnitt 19); keine bestehenden Fälle
      werden entfernt.
```

Betroffen, aber ohne Vertragsänderung durch diesen Plan:

```text
src/main.cpp, main/app_main.cpp
    (Composition-Root-Verdrahtung bleibt außerhalb des Plan-only-Scopes,
    siehe Abschnitt 2)
docs/ACTUATOR_TIMING.md
    (nach Umsetzung Ergänzung der akzeptierten Struktur, keine
    TBD_COMMISSIONING-Werte)
docs/ROADMAP.md
    (Plan-SHA-Nachführung gemäß Abschnitt 20 in separatem Metadaten-Commit)
```

Kein neuer `device_platform`-Port, keine neue Drittbibliothek, keine
Änderung an `run_persistence_*`, `run_commands.*` oder
`process_state_machine.*` (letztere drei bleiben Gegenstand von Issue #106,
nicht dieses Plans).

## 6. Aufruf-/Zeitmodell

### 6.0 Vollständige Zustands- und Ein-/Ausgabetypen

Diese Typen werden von allen folgenden Abschnitten (6–12) vorausgesetzt und
sind hier vollständig definiert, um Vorwärtsreferenzen ohne Definition zu
vermeiden:

```cpp
struct AcceptedControlCommand {
    std::uint64_t sequence{0U};
    AbstractControlDirection direction{AbstractControlDirection::Idle};
    double timeQuote{0.0};
    ActuatorDemandClass demandClass{ActuatorDemandClass::NoValidRequest};
    ControlRequestContext contextAtAcceptance;
};

struct ActuatorPlannerRuntimeState {
    std::optional<AcceptedControlCommand> acceptedCommand;
    AbstractControlDirection physicalDirection{AbstractControlDirection::Idle};
    std::uint64_t directionActivatedAtMonotonicMillis{0U};
    bool hasDeactivated{false};
    std::uint64_t directionDeactivatedAtMonotonicMillis{0U};
    bool windowInitialized{false};
    std::uint64_t windowStartMonotonicMillis{0U};
    PulseAccumulator accumulator;
    std::optional<AbstractControlDirection> counterDirectionCandidate;
    std::uint64_t counterDirectionObservedSinceMonotonicMillis{0U};
    std::optional<std::uint64_t> lastAcceptedSequence;
    std::optional<std::uint64_t> lastNewRequestAcceptedAtMonotonicMillis;
    std::optional<ActuatorWatchdogFaultEvidence> latchedWatchdogFault;
    bool outerFanActive{false};
    std::optional<std::uint64_t> outerFanDeactivationRequestedAtMonotonicMillis;
    bool innerFanActive{false};
    std::optional<std::uint64_t> innerFanDeactivationRequestedAtMonotonicMillis;
};

struct ActuatorPlanTickInput {
    std::uint64_t nowMonotonicMillis{0U};
    std::optional<TemperatureControlResult> newEvaluation;
    ControlRequestContext currentCanonicalContext;
    bool temperatureControlledPhase{false};
    ActuatorSafetyGateInput safetyGate;
};

struct ActuatorPlanTickResult {
    ActuatorPlanStatus status{ActuatorPlanStatus::Unconfigured};
    ActuatorPlanReason reason{ActuatorPlanReason::NoCommissioning};
    AbstractControlDirection appliedDirection{AbstractControlDirection::Idle};
    bool outerFanEnabled{false};
    bool innerFanEnabled{false};
    bool counterDirectionConfirming{false};
    ActuatorAdmissionOutcome admissionOutcome{ActuatorAdmissionOutcome::NoCandidate};
    std::optional<PreviousControlRequestFeedback> feedbackForAcceptedRequest;
    std::optional<std::uint64_t> acceptedCommandSequence;
    bool watchdogFaultActive{false};
};

class ActuatorPlanner {
   public:
    explicit ActuatorPlanner(ActuatorPlannerParameters parameters);

    [[nodiscard]] ActuatorPlanTickResult tick(const ActuatorPlanTickInput& input);
    [[nodiscard]] ActuatorPlanTickResult forceStop(std::uint64_t nowMonotonicMillis);
    void applyExternalWatchdogFaultReset(std::uint64_t nowMonotonicMillis);

    [[nodiscard]] const ActuatorPlannerRuntimeState& state() const;
    [[nodiscard]] const ActuatorPlannerParameters& parameters() const;

   private:
    ActuatorPlannerParameters parameters_;
    ActuatorPlannerRuntimeState state_;
};
```

`PulseAccumulator`, `ActuatorDemandClass`, `ActuatorSafetyGateInput`,
`ActuatorWatchdogFaultEvidence`, `ActuatorPlannerParameters`,
`ActuatorPlanStatus`, `ActuatorPlanReason` und `ActuatorAdmissionOutcome`
sind in den Abschnitten 4.2, 7, 8.2, 8.4, 9.3 und 13 definiert, auf die
dieser Abschnitt verweist, statt sie zu duplizieren.

### 6.1 Aufrufsemantik

Der Planer besitzt **einen** Tick-Einstiegspunkt (`tick()`, oben definiert).
Er wird vom Aufrufer (`TemperatureControlApplicationOrchestrator::tickActuatorPlan()`)
potenziell **häufiger** aufgerufen als #22 seine eigene, sensorgetaktete
`evaluateTemperatureControl()`-Berechnung durchführt.

Intern läuft `tick()` in **zwei Phasen**, die absichtlich unabhängig
voneinander sind:

### 6.2 Phase A – Annahme (Admission), immer zuerst, unabhängig von Parametern/Safety/Fault

Diese Phase aktualisiert ausschließlich Buchführung (Replay-Schutz,
Watchdog-Lebenszeichen) und liefert `admissionOutcome` plus – bei Erfolg –
einen „vorgemerkten Kandidaten" für Phase B. Sie läuft **immer**, auch wenn
Parameter ungültig sind, Safety nicht `Allowed` ist oder ein Watchdog-Fault
latched ist – denn ein strukturell gültiges neues #22-Ergebnis ist ein
Lebenszeichen von #22, unabhängig davon, ob #23 es gerade physisch umsetzen
darf.

1. `input.newEvaluation == std::nullopt` -> `admissionOutcome = NoCandidate`,
   kein vorgemerkter Kandidat. **Phase A endet hier.**
2. `newEvaluation` strukturell ungültig (unbekannter Enumwert bei Richtung
   oder Status, `timeQuote` nicht-finit oder außerhalb `[0,1]` bei
   vorhandener `ControlRequest`, Status/`ControlRequest`-Präsenz widerspricht
   der #22-Matrix aus Abschnitt 7, `sequence == 0` bei vorhandener
   `ControlRequest`, strukturell ungültiger `currentCanonicalContext`) ->
   `admissionOutcome = MalformedCandidate`. **Phase A endet hier**, kein
   vorgemerkter Kandidat.
3. `classifyActuatorDemand(newEvaluation.value())` (Abschnitt 7) ==
   `NoValidRequest` (also `Unavailable`/`InvalidInput` von #22, keine
   `ControlRequest` vorhanden): `admissionOutcome = Accepted`,
   `state_.lastNewRequestAcceptedAtMonotonicMillis = input.nowMonotonicMillis`
   (Lebenszeichen, unabhängig von einer Sequenz – es gibt hier keine). Der
   vorgemerkte Kandidat trägt `demandClass = NoValidRequest` und **kein**
   `AcceptedControlCommand` (keine Richtung, keine Sequenz).
4. Andernfalls trägt `newEvaluation` eine `ControlRequest`
   (`NeutralOff`/`AirLimitBlockedOff`/`AirLimitReducedDemand`/`NormalDemand`).
   Geprüft wird der Reihe nach:
   - `identity.sequence <= state().lastAcceptedSequence.value_or(0)` ->
     `admissionOutcome = DuplicateOrOldSequence`. **Phase A endet hier**,
     kein vorgemerkter Kandidat, keine Bookkeeping-Änderung.
   - `deadlineReached(input.nowMonotonicMillis,
     identity.createdAtMonotonicMillis, requestWatchdogMillis)` (Abschnitt
     8.3) -> `admissionOutcome = StaleOnArrivalWatchdog`. **Phase A endet
     hier**, kein vorgemerkter Kandidat.
   - `context != input.currentCanonicalContext` ->
     `admissionOutcome = StaleOnArrivalContext`. **Phase A endet hier**, kein
     vorgemerkter Kandidat.
   - sonst: `admissionOutcome = Accepted`,
     `state_.lastAcceptedSequence = identity.sequence`,
     `state_.lastNewRequestAcceptedAtMonotonicMillis = input.nowMonotonicMillis`.
     Der vorgemerkte Kandidat trägt `demandClass`, `direction`, `timeQuote`,
     `sequence` und `context`.

Eine abgelehnte neue Request (`DuplicateOrOldSequence`,
`StaleOnArrivalWatchdog`, `StaleOnArrivalContext`, `MalformedCandidate`)
berührt einen bereits gehaltenen `acceptedCommand` **nicht**: Phase B
bewertet in diesem Fall die bestehende Zeitbasis unverändert, so, als wäre
`newEvaluation` `std::nullopt` gewesen. Das schließt aus, dass ein Replay
oder ein verspätet eingetroffener Kandidat einen laufenden, weiterhin
gültigen Zeitplan zerstört.

### 6.3 Phase B – Physischer Ausgang (Prioritätsleiter, Abschnitt 8.2)

Phase B verwendet den in Phase A ermittelten vorgemerkten Kandidaten (falls
vorhanden) zusammen mit dem laufenden `ActuatorPlannerRuntimeState`, um genau
eine physische Ausgangsentscheidung für diesen Tick zu treffen. Sie ist in
Abschnitt 8.2 vollständig als Prioritätsleiter definiert.

### 6.4 Laufender Watchdog

Bei **jedem** Tick, unabhängig davon, ob Phase A einen Kandidaten
verarbeitet hat, prüft Phase B:

```text
deadlineReached(nowMonotonicMillis, lastNewRequestAcceptedAtMonotonicMillis, requestWatchdogMillis)
```

Da `lastNewRequestAcceptedAtMonotonicMillis` in Phase A bereits durch **jede**
erfolgreich angenommene neue Evaluation aktualisiert wird (auch durch eine
`NoValidRequest`-Evaluation, siehe 6.1 Schritt 3), ist der Watchdog
unabhängig von der #22-eigenen Kadenz und von der Planner-Tick-Kadenz: Läuft
#22 seltener als der Planer tickt, bleibt der Watchdog bis zum konfigurierten
Zeitraum ruhig; läuft #22 (theoretisch) so schnell, dass Sequenzen dupliziert
ankommen, werden diese in Phase A abgelehnt, ohne den Watchdog oder die
laufende Zeitbasis zu stören.

### 6.5 Welche Quote ein Fenster bestimmt / neue Quote mitten im Fenster

Siehe Abschnitt 8.1. Kurzfassung: Die für ein laufendes Fenster maßgebliche
Quote wird beim Fensterstart aus dem zu diesem Zeitpunkt gehaltenen
`acceptedCommand` gelesen und für die Dauer des Fensters nicht erneut
ausgewertet; eine während des Fensters neu angenommene Request mit
unveränderter Richtung wirkt erst im nächsten Fenster. Ereignisse der
Prioritätsstufen 1–7 (Abschnitt 8.2) wirken dagegen sofort.

### 6.6 Beenden einer geplanten Peltierfreigabe

Da `tick()` bei jedem Regelzyklus aufgerufen wird, endet ein geplanter Impuls
zuverlässig in dem Tick, dessen `nowMonotonicMillis` die berechnete
Ende-Zeit (elapsed-Vergleich, Abschnitt 8.3) erreicht oder überschreitet –
vorausgesetzt, die Aufrufer-Kadenz ist deutlich feiner als die kürzeste
vorkommende geplante Ein-/Aus-Dauer (Kadenzanforderung an den Aufrufer,
außerhalb dieses Plans dokumentiert, siehe Abschnitt 21).

## 7. Kanonische AirLimit-/Demand-Klassifikation

`ActuatorPlanTickInput::newEvaluation` trägt den **vollständigen**
`TemperatureControlResult` (Status, Reason, `AirLimitState`,
`ControlRequest`). #23 erfindet daraus keine eigene Luftlimitlogik, sondern
bildet die bereits abschließende #22-Matrix (Issue-22-Plan Abschnitt 7.2)
über eine reine, tabellarische Klassifikation ab.

```cpp
enum class ActuatorDemandClass : std::uint8_t {
    NoValidRequest,        // Unavailable oder InvalidInput – keine ControlRequest
    NeutralOff,             // Off / NeutralBand
    AirLimitBlockedOff,     // Off / AirLimitBlocked
    AirLimitReducedDemand,  // Demand / AirLimitReduced
    NormalDemand,           // Demand / None oder Saturated
};

[[nodiscard]] ActuatorDemandClass classifyActuatorDemand(
    const TemperatureControlResult& controlResult);
```

Die Implementierung ist eine direkte, erschöpfende Fallunterscheidung über
die bereits im Issue-22-Plan (Abschnitt 7.2) dokumentierte
Status-/Reason-/AirLimitState-Tabelle – kein neuer Schwellenwert, keine
Neuberechnung. Jede laut #22-Matrix unzulässige Kombination wird in Phase A
als strukturell ungültig eingestuft (`MalformedCandidate`, Abschnitt 6.2).

| `ActuatorDemandClass` | Effekt auf Richtung | Effekt auf Akkumulator |
|---|---|---|
| `NoValidRequest` | kein `AcceptedControlCommand`; Abschnitt 8.2 Rang 6 | siehe Abschnitt 8.4 (Verwurf bei Übergang nach Idle) |
| `NeutralOff` | Richtung `Idle` | siehe Abschnitt 8.4 |
| `AirLimitBlockedOff` | Richtung `Idle` | zusätzlich: sofortiger, unbedingter Verwurf bei **Übergang** in diese Klasse |
| `AirLimitReducedDemand` | Richtung Heating/Cooling, Quote bereits von #22 reduziert | bei Übergang aus einer weniger restriktiven Klasse: Verwurf vor Gutschrift der aktuellen (reduzierten) Fensterquote; bei fortlaufendem Verbleib: normale Akkumulation aus der jeweils aktuellen, bereits reduzierten Quote |
| `NormalDemand` | Richtung Heating/Cooling | normale Akkumulation |

Damit ist entschieden: `AirLimitBlocked` verwirft das betroffene Guthaben
immer sofort. `AirLimitReduced` kann kein vor der Reduktion gesammeltes
höheres Guthaben zur Umgehung der aktuellen Reduktion nutzen, weil der
Übergang selbst das alte Guthaben verwirft. `AirLimitBlockedOff` ist
**keine** Safety-Sofortabschaltung – eine bereits aktive Richtung wird davon
nicht anders behandelt als von `NeutralOff` (beide unterliegen der normalen
Mindest-Einschaltzeit-Haltelogik, Abschnitt 8.2 Rang 9); der Unterschied
wirkt ausschließlich auf den Akkumulator.

## 8. Verhalten im Detail: Fenster, Prioritätsleiter, Zeitarithmetik

### 8.1 Fenster- und Impulsplatzierung

**Fensterbeginn (nicht-zirkulär).** `windowStartMonotonicMillis`
(re-)initialisiert sich am frühesten der folgenden zwei Ereignisse:

- **(a) Frischer Start:** in dem Tick, in dem `acceptedCommand.direction`
  erstmals `Heating` oder `Cooling` wird, während `windowInitialized ==
  false`. Dieser Fall tritt genau dann ein, wenn zuvor kein physischer
  Halte-/Wartezustand existieren kann – entweder weil der Planer noch nie
  eine Richtung physisch aktiviert hat (`hasDeactivated == false`) oder weil
  `windowInitialized` zuvor durch einen der in Abschnitt 8.4 aufgeführten
  Verwurfsauslöser (unter anderem: Übergang nach `Idle`) zurückgesetzt
  wurde. In diesem Fall gibt es keine Mindest-Auszeit/Totzeit, die die
  physische Freigabe verzögern könnte (es existiert keine vorherige
  Deaktivierung, gegen die eine Frist geprüft werden müsste), sodass
  Fensterbeginn und physische Eignung zusammenfallen. Dies löst die
  Zirkularität: Die erste, noch so kleine Anforderung beginnt sofort ein
  Fenster und kann ab dem ersten Fensterstart-Ereignis Guthaben sammeln,
  ohne zuvor bereits physisch aktiv gewesen zu sein.
- **(b) Bestätigter Richtungswechsel:** in dem Tick, in dem ein bestätigter
  Richtungswechsel tatsächlich physisch enabled (Abschnitt 8.2 Rang 11).
  Dieser Fall tritt ein, wenn zuvor eine andere Richtung physisch aktiv war;
  Mindest-Auszeit und Totzeit haben zu diesem Zeitpunkt bereits regulär
  abgelaufen sein müssen (Rang 10), sodass kein Guthaben „durch die Sperre
  hindurch" gesammelt werden kann – während der Bestätigungs-/Haltephase
  läuft ausschließlich das Fenster der noch aktiven alten Richtung
  (Rang 9) unverändert weiter; die neue Richtung besitzt bis zu ihrer
  tatsächlichen Enablement keinerlei eigenes Fenster oder Guthaben.

**Maßgebliche Quote pro Fenster.** Bei jedem Fensterstart-Ereignis (siehe
unten, „Fensterfortschritt") wird `requestedOnMillisExact` aus der Quote des
zu diesem Zeitpunkt gehaltenen `acceptedCommand` berechnet:

```text
requestedOnMillisExact = clamp(timeQuote, 0.0, 1.0) * switchingWindowMillis   (double, ungerundet)
```

- `requestedOnMillisExact >= minimumOnMillis`: `scheduledOnMillis =
  round_half_up(requestedOnMillisExact)` (nächste ganze Millisekunde, `.5`
  aufgerundet), direkt geplant (`ScheduledWithinWindow`); der Akkumulator
  wird bei diesem Fensterstart nicht zusätzlich gefüttert.
- `0 < requestedOnMillisExact < minimumOnMillis`:
  `accumulator.accumulatedMillis += requestedOnMillisExact` (double,
  ungerundet; `accumulator.direction` wird dabei auf die aktuelle Richtung
  gebunden), begrenzt auf `pulseAccumulatorCapMillis`. Erreicht
  `accumulatedMillis >= minimumOnMillis`: `scheduledOnMillis =
  minimumOnMillis` (exakt), `accumulatedMillis -= minimumOnMillis` (Rest
  bleibt erhalten), Reason `MinimumPulseTriggered`. Sonst:
  `scheduledOnMillis = 0`, Reason `AccumulatingBelowThreshold`.
- `requestedOnMillisExact == 0`: `scheduledOnMillis = 0`, Reason
  `NeutralIdle` beziehungsweise `AirLimitBlocked` (Heating/Cooling mit Quote
  `0` tritt laut #22-Matrix nicht auf).

Die Fensterzuordnung erfolgt **einmalig pro Fensterstart-Ereignis**, nicht
pro Tick und nicht pro angenommener neuer Request innerhalb desselben
Fensters – das schließt Mehrfach-Akkumulation derselben Fensterquote
strukturell aus.

**Fensterfortschritt (overflow-sicher, O(1)).** Kein Schritt dieser Berechnung
verwendet eine ungeprüfte Deadline-Addition:

```cpp
if (now < windowStartMonotonicMillis) {
    // Retrograde -> Rang „TimeInvalid" (Abschnitt 8.2 Rang 1-äquivalent)
}
elapsed = now - windowStartMonotonicMillis;
if (elapsed >= switchingWindowMillis) {
    windowsElapsed = elapsed / switchingWindowMillis;              // ganzzahlig, >= 1
    windowStartMonotonicMillis += windowsElapsed * switchingWindowMillis;
    // Beweis: windowsElapsed * switchingWindowMillis <= elapsed <= now (da elapsed = now - windowStartMonotonicMillis),
    // also windowStartMonotonicMillis(neu) <= windowStartMonotonicMillis(alt) + elapsed = now.
    // Die Summe kann folglich nicht ueber einen bereits gueltigen now-Wert hinaus ueberlaufen.
    // -> genau EIN Fensterstart-Ereignis wird fuer den neuen windowStartMonotonicMillis ausgewertet.
}
elapsedInWindow = now - windowStartMonotonicMillis;   // < switchingWindowMillis
```

Diese Berechnung ist **O(1)** unabhängig davon, wie lange die Pause zwischen
zwei Ticks war; es gibt keine Iterationsschleife und keine künstliche
Iterationsobergrenze. **Nicht-Nachhol-Semantik:** Ist `windowsElapsed > 1`
(ungewöhnlich lange Pause), werden die dazwischenliegenden, nicht
beobachteten Fenster **nicht** einzeln nachgeholt oder rückwirkend
akkumuliert – es wird genau ein Fensterstart-Ereignis für das aktuell
gültige, neu berechnete `windowStartMonotonicMillis` mit der dann aktuellen
Quote des `acceptedCommand` ausgewertet. Das ist bewusst konservativ: ein
langer, unbeobachteter Zeitraum darf kein rückwirkend „nachgeholtes"
Guthaben erzeugen (dieselbe Nicht-Umgehungslogik wie in Abschnitt 7).

**Physischer Wunschzustand.** Für einen beliebigen Tick innerhalb eines
Fensters:

```text
desiredActive = elapsedInWindow < scheduledOnMillis
```

Dieser Wunschzustand wird durch die Mindestzeit-/Totzeit-/Safety-Schicht
(Abschnitt 8.2) gefiltert.

### 8.2 Prioritätsleiter (Phase B)

```cpp
enum class ActuatorPlanStatus : std::uint8_t {
    Active,        // Heating oder Cooling wird diesen Tick physisch angesteuert
    Idle,          // Idle wird diesen Tick physisch angesteuert
    Unconfigured,  // Parameter NoCommissioning
    InvalidInput,  // strukturell ungueltige Parameter/Evaluation oder TimeInvalid
};

enum class ActuatorPlanReason : std::uint8_t {
    MalformedEvaluation,
    NoCommissioning,
    InvalidConfiguration,
    TimeInvalid,
    SafetyGateUnresolved,
    ExternalSafetyOverride,
    RequestWatchdogFaultLatched,
    StaleRequestWatchdog,
    NoValidRequest,
    StaleRequestContext,
    MinimumOnTimeHeld,
    MinimumOffTimeHeld,
    PolarityDeadTimeHeld,
    DirectionChangeApplied,
    NeutralIdle,
    AirLimitBlocked,
    AccumulatingBelowThreshold,
    MinimumPulseTriggered,
    ScheduledWithinWindow,
};
```

Jeder Tick wird gegen genau eine dieser Stufen ausgewertet (höchste
zutreffende Stufe gewinnt; alle Verwurfsangaben sind **unbedingt** und
**einheitlich**: Akkumulator, `windowInitialized`, Gegenrichtungskandidat und
`acceptedCommand` werden gemeinsam verworfen – es gibt **keine** Stufe, die
den Akkumulator „unverändert hält", während gleichzeitig `acceptedCommand`
verworfen wird):

| Rang | Bedingung | Status | Reason | Physisch | Verwurf |
|---:|---|---|---|---|---|
| 1 | Phase A liefert `admissionOutcome == MalformedCandidate` | `InvalidInput` | `MalformedEvaluation` | `Idle` sofort | unbedingt: Akkumulator, Fenster, Gegenrichtungskandidat, `acceptedCommand` |
| 2 | `classifyActuatorPlannerParameters(parameters_) == Unconfigured` | `Unconfigured` | `NoCommissioning` | `Idle` | unbedingt (identisch zu Rang 1) |
| 2 | `classifyActuatorPlannerParameters(parameters_) == Invalid` | `InvalidInput` | `InvalidConfiguration` | `Idle` | unbedingt (identisch zu Rang 1) |
| 3 | `safetyGate.status == Unresolved` | `Idle` | `SafetyGateUnresolved` | `Idle` sofort, überstimmt Mindest-Einschaltzeit | unbedingt |
| 3 | `safetyGate.status == ImmediateStop` | `Idle` | `ExternalSafetyOverride` | `Idle` sofort, überstimmt Mindest-Einschaltzeit | unbedingt |
| 4 | `state().latchedWatchdogFault.has_value()` | `Idle` | `RequestWatchdogFaultLatched` | `Idle` | bereits leer (Verwurf erfolgte im Trip-Tick, Rang 5) |
| 5 | laufender Watchdog löst diesen Tick aus (Abschnitt 6.4) | `Idle` | `StaleRequestWatchdog` | `Idle` sofort | unbedingt; zusätzlich wird `latchedWatchdogFault` gesetzt |
| 6 | Phase-A-Kandidat vorhanden mit `demandClass == NoValidRequest` (explizite neue `Unavailable`/`InvalidInput`-Evaluation) | `Idle` | `NoValidRequest` | `Idle` sofort, überstimmt Mindest-Einschaltzeit | unbedingt |
| 7 | `acceptedCommand` vorhanden, aber `contextAtAcceptance != input.currentCanonicalContext` (bei jedem Tick geprüft, nicht nur bei Ankunft) | `Idle` | `StaleRequestContext` | `Idle` sofort | unbedingt |
| 8 | kein `acceptedCommand` vorhanden (nach den obigen Prüfungen, oder von Anfang an) | `Idle` | `NoValidRequest` | `Idle` | leer (nichts zu verwerfen) |
| 9 | `physicalDirection != Idle` **und** Mindest-Einschaltzeit noch nicht erfüllt **und** `acceptedCommand.direction != physicalDirection` | `Active` (alte Richtung bleibt) | `MinimumOnTimeHeld` | bisherige Richtung bleibt an | keiner (Fenster der alten Richtung läuft normal weiter) |
| 10 | `physicalDirection == Idle`, `acceptedCommand.direction` will Heating/Cooling starten, aber Mindest-Auszeit **oder** Totzeit noch aktiv (spätere maßgeblich; bei exaktem Gleichstand: `PolarityDeadTimeHeld`) | `Idle` | `MinimumOffTimeHeld` bzw. `PolarityDeadTimeHeld` | `Idle` | keiner |
| 11 | bestätigter Richtungswechsel wird in diesem Tick enabled (Mindestzeiten erfüllt, Bestätigung abgeschlossen, Abschnitt 8.5) **oder** frischer Start aus vollständigem Leerzustand (8.1 Fall a) | `Active` | `DirectionChangeApplied` | neue Richtung startet, neues Fenster (8.1) | altes Akkumulator-/Fenster-/Kandidatguthaben wird verworfen (bereits Teil des Übergangs) |
| 12 | `acceptedCommand.direction == Idle`, kein Mindest-Einschaltzeit-Halt aus Rang 9 | `Idle` | `NeutralIdle` bzw. `AirLimitBlocked` | `Idle` | gemäß Abschnitt 7/8.4 |
| 13 | normale Fensterauswertung (8.1), Akkumulator unter Schwelle | `Idle` | `AccumulatingBelowThreshold` | `Idle` | keiner (Akkumulator wächst) |
| 13 | normale Fensterauswertung, Mindestimpuls ausgelöst | `Active` | `MinimumPulseTriggered` | Richtung an für `minimumOnMillis` | keiner |
| 13 | normale Fensterauswertung, direkte Planung | `Active`/`Idle` gemäß `desiredActive` | `ScheduledWithinWindow` | gemäß `desiredActive` | keiner |

Zusätzlich, unabhängig vom Rang: `counterDirectionConfirming` (Bool im
Ergebnis) wird gesetzt, wenn ein Gegenrichtungskandidat aktuell beobachtet,
aber noch nicht bestätigt ist (Abschnitt 8.5) – eine Zusatzauskunft, keine
eigene Prioritätsstufe.

`RequiredSensorUnavailable` aus früheren Revisionen entfällt vollständig: Ein
fehlender/ungültiger Pflichtsensor erscheint ausschließlich als
`ActuatorDemandClass::NoValidRequest` und landet damit in Rang 6 (bei
expliziter neuer Evaluation) oder Rang 8 (kein aktueller Bezug), ohne mit
`InvalidConfiguration` oder `TimeInvalid` vermischt zu werden.

### 8.3 Overflow-sichere Zeitarithmetik

Jede Fristprüfung – nicht nur die Fensterlogik – verwendet ausschließlich
elapsed-time-Vergleiche:

```cpp
[[nodiscard]] bool deadlineReached(std::uint64_t now, std::uint64_t since,
                                    std::uint64_t durationMillis) {
    if (now < since) {
        return false;  // Retrograde -> Frist gilt defensiv als NICHT erreicht
    }
    return (now - since) >= durationMillis;
}
```

Ein `now < since`-Fall ist laut `ITimeSource`-Vertrag ausgeschlossen, wird
aber defensiv abgefangen: Tritt er an einer der Referenzen
(`directionActivatedAtMonotonicMillis`, `directionDeactivatedAtMonotonicMillis`,
`windowStartMonotonicMillis`, `counterDirectionObservedSinceMonotonicMillis`,
`lastNewRequestAcceptedAtMonotonicMillis`,
`outerFanDeactivationRequestedAtMonotonicMillis`/
`innerFanDeactivationRequestedAtMonotonicMillis`) auf, wird dies als
`TimeInvalid` (Rang-1-äquivalent, `InvalidInput`, unbedingter Verwurf)
eingestuft. Es gibt keine Stelle im gesamten Vertrag, an der eine Deadline
durch `start + dauer` berechnet und direkt mit `now` verglichen wird; der
Lüfter-Nachlauf merkt sich `fanDeactivationRequestedAtMonotonicMillis` und
prüft `deadlineReached(now, fanDeactivationRequestedAtMonotonicMillis,
fanPostRunMillis)`. Die Fensterarithmetik (Abschnitt 8.1) verwendet dasselbe
Muster über eine geprüfte Ganzzahldivision statt wiederholter Addition.

### 8.4 Einziger richtungsgebundener Akkumulator und Verwurfsregeln

```cpp
struct PulseAccumulator {
    AbstractControlDirection direction{AbstractControlDirection::Idle};
    double accumulatedMillis{0.0};
};
```

Es existiert zu jedem Zeitpunkt genau **ein** `PulseAccumulator`.
`direction == Idle` bedeutet „leer/nicht gebunden"; nur `Heating` oder
`Cooling` tragen jemals ein Guthaben `> 0`. Ein unbestätigter
Gegenrichtungskandidat hat **keinen** eigenen Akkumulator.

Der Akkumulator, `windowInitialized` und `counterDirectionCandidate` werden
gemeinsam und unbedingt auf ihren Leerzustand zurückgesetzt bei:

- jedem Rang-1-bis-7-Ereignis (Abschnitt 8.2);
- **jedem Übergang von `acceptedCommand.direction` von `Heating` oder
  `Cooling` nach `Idle`** (also jeder neu angenommenen `NeutralOff`- oder
  `AirLimitBlockedOff`-Request, unabhängig davon, wie lange die
  anschließende Idle-Phase dauert). Diese Regel schließt sowohl den in
  `ACTUATOR_TIMING.md` explizit genannten Fall („Verlassen der
  temperaturgeregelten Phase, Stop, Fehler oder ungültiger Sensorzustand")
  als auch beliebig lange gewöhnliche Neutralband-Phasen ein: Ein
  Impulsguthaben überlebt **keine** noch so kurze Rückkehr nach `Idle`. Eine
  spätere erneute Heating-/Cooling-Anforderung beginnt strukturell immer als
  „frischer Start" (Abschnitt 8.1 Fall a) und sammelt ihr Guthaben neu. Dies
  schließt zugleich die Lücke, dass ein sehr altes Guthaben nach einer
  langen Sperr-/Neutralphase „blind ausgeführt" werden könnte
  (`ACTUATOR_TIMING.md`: „Nach längerer Sperrung wird die Regelanforderung
  neu berechnet; ein alter Akkumulator wird nicht blind ausgeführt.");
- bestätigtem, enabled Richtungswechsel (Rang 11) – das Restguthaben der
  alten Richtung wird verworfen, bevor das neue Fenster der neuen Richtung
  beginnt;
- Übergang in `AirLimitBlockedOff` oder in `AirLimitReducedDemand` aus einer
  weniger restriktiven Klasse (Abschnitt 7) – bereits durch die vorherige
  Regel abgedeckt, sofern die alte Klasse Heating/Cooling war; für den
  Fensterwechsel innerhalb einer fortlaufenden Heating/Cooling-Demand (z. B.
  `NormalDemand -> AirLimitReducedDemand` ohne Zwischenstopp) gilt diese
  Regel zusätzlich und eigenständig;
- `forceStop()` (Abschnitt 11).

Damit ist zu jedem Zeitpunkt eindeutig entweder ein leerer oder ein an genau
eine Richtung gebundener Akkumulator vorhanden; ein zweiter, „versteckter"
Akkumulator für eine unbestätigte Gegenrichtung existiert im Datenmodell
nicht.

### 8.5 Bestätigter Richtungswechsel

Solange `physicalDirection != Idle` besteht und `acceptedCommand.direction`
die entgegengesetzte Richtung anzeigt, mit Klasse `NormalDemand` oder
`AirLimitReducedDemand` und `timeQuote >=
counterDirectionConfirmationQuoteThreshold`: Falls
`counterDirectionCandidate` leer oder `!=` dieser Gegenrichtung ist, wird ein
neuer Kandidat mit `counterDirectionObservedSinceMonotonicMillis = now`
gestartet (`counterDirectionConfirming = true`). Jede Unterbrechung (Quote
fällt unter die Schwelle, Richtung wechselt zurück, wird
`NoValidRequest`/`NeutralOff`/`AirLimitBlockedOff`, oder eine der
Rang-1-bis-7-Bedingungen tritt ein) setzt `counterDirectionCandidate` sofort
zurück. Erst wenn `deadlineReached(now,
counterDirectionObservedSinceMonotonicMillis,
counterDirectionConfirmationDurationMillis)` **ununterbrochen** erreicht
wird, gilt die Gegenrichtung als bestätigt: Die alte Richtung wird `Idle`;
Rang-10-Logik (Mindest-Auszeit/Totzeit) greift, bevor Rang 11 tatsächlich
enabled.

Vor Ablauf der Mindest-Einschaltzeit der alten Richtung bleibt eine
Gegenanforderung – bestätigt oder nicht – wirkungslos auf die physische
Ausgabe (Rang 9 hat Vorrang vor Rang 10/11); der Bestätigungstimer läuft
davon unbeeinflusst weiter.

### 8.6 Watchdog-Fault-Evidenz

Löst der laufende Watchdog aus (Rang 5), wird

```cpp
state_.latchedWatchdogFault = ActuatorWatchdogFaultEvidence{
    .detectedAtMonotonicMillis = now,
    .lastAcceptedSequenceBeforeFault = state_.lastAcceptedSequence.value_or(0U),
};
```

gesetzt. Solange `latchedWatchdogFault.has_value()`, bleibt Rang 4 in jedem
weiteren Tick maßgeblich (`Idle`, `RequestWatchdogFaultLatched`) –
**unabhängig davon**, was danach in Phase A geschieht: Eine strukturell
einwandfreie, kontextfrische, an sich akzeptable neue `newEvaluation` wird
in Phase A weiterhin für Sequenz-/Watchdog-Bücher berücksichtigt
(`admissionOutcome` kann `Accepted` sein, `lastAcceptedSequence`/
`lastNewRequestAcceptedAtMonotonicMillis` werden aktualisiert), öffnet aber
**keine** neue physische Freigabe, solange Rang 4 vor Rang 6/8/9 usw.
ausgewertet wird.

```cpp
void applyExternalWatchdogFaultReset(std::uint64_t nowMonotonicMillis);
```

Diese Methode löscht `latchedWatchdogFault`. Sie wird ausschließlich
aufgerufen, nachdem eine **externe, #24-getriebene** Logik einen gültigen
Fehlerreset beziehungsweise eine automatische Wiederfreigabe für diesen
konkreten Fehler abgeschlossen hat – exakt das in `docs/SAFETY_AND_FAULTS.md`
verwendete Vokabular „Fehlerreset beziehungsweise automatische
Wiederfreigabe", ausdrücklich **nicht** „Quittieren" (Quittieren entfernt
laut `SAFETY_AND_FAULTS.md` nur die Melde-/Bestätigungsanforderung, nicht
die Aktorsperre). `#23` ruft diese Methode an **keiner** Stelle selbst auf:

- **Keine** Lifecycle-Grenze (`NewActiveRun`, `LeaveTemperatureControl`,
  `Recovery`, `Fault`, `SafeBoot`, `Service`, `Standby`) löscht
  `latchedWatchdogFault` implizit. Ein neuer Lauf ist kein Fehlerreset.
- Ein simulierter oder realer Neustart (neue `ActuatorPlanner`-Instanz) ist
  ebenfalls **kein** fachlicher Fehlerreset und wird in diesem Plan nicht
  als solcher bezeichnet. Da der Watchdog-Fault-Zustand in dieser Revision
  RAM-only bleibt (siehe Nicht-Ziele, Abschnitt 2), wird hiermit
  ausdrücklich festgehalten: **Vor jeder produktiven Aktorverdrahtung ist
  ein #24-Integrationsgate zwingend erforderlich, das sicherstellt, dass ein
  realer Neustart eine bestehende Systemsperre nicht umgeht** (persistente
  Verriegelung beziehungsweise gleichwertiger Mechanismus ist Aufgabe von
  #24, `docs/SAFETY_AND_FAULTS.md` Abschnitt „Nach einem Neustart"). Bis
  dieses Gate erfüllt ist, verlässt sich eine produktive Instanziierung
  **nicht** auf #23s RAM-only-Evidenz als alleinigen Schutz vor einem
  Reboot-Umgehungspfad; #23 selbst behauptet diesen Schutz nicht.

## 9. Feedback-Dispositionsmatrix

`ActuatorPlanTickResult::feedbackForAcceptedRequest` ist
`std::optional<PreviousControlRequestFeedback>`. Grundlage ist wörtlich
`docs/tasks/issue-22-pi-control-air-limits-plan.md` Abschnitt 8.2
„Feedbackfenster": „Nur eine unmittelbar vorherige aktive
`Heating`-/`Cooling`-ControlRequest öffnet ein Feedbackfenster", „vorhandenes
Feedback muss exakt die letzte Request-Sequence referenzieren", „fehlendes
Feedback ... wird als konservatives Einfrieren behandelt und das Fenster
geschlossen", „Feedback für eine vorherige gültige OFF-ControlRequest ist
unzulässig ... OFF benötigt kein Anti-Windup-Feedback".

### 9.1 Ein einziges Subjekt: `acceptedCommand`

Es gibt **kein** Konzept einer „alten aktiven Sequenz" getrennt von
`acceptedCommand`. Da `acceptedCommand` in Phase A ausschließlich durch die
zuletzt erfolgreich angenommene Evaluation ersetzt wird (niemals durch die
physische Enablement-Historie), ist `acceptedCommand` – falls seine
Richtung `Heating`/`Cooling` ist – **immer genau die Request, die #22 im
nächsten Aufruf als unmittelbar vorherige aktive Request erwartet.** Eine
physisch noch nachlaufende alte Richtung (Rang 9, `MinimumOnTimeHeld`)
ändert das Feedback-Subjekt nicht zurück: Sobald eine neue
Gegenrichtungsrequest B in Phase A angenommen wurde, ist `acceptedCommand`
bereits B, auch wenn physisch noch die alte Richtung A läuft. Es gibt daher
**keinen** Fall, in dem #23 Feedback für eine bereits durch eine neuere
Admission ersetzte Sequenz aussendet – das ist strukturell ausgeschlossen,
nicht nur durch Sorgfalt vermieden.

### 9.2 Regel

```text
acceptedCommand fehlt ODER acceptedCommand.direction == Idle
    -> feedbackForAcceptedRequest = std::nullopt
       (deckt: nie akzeptiert, NeutralOff/AirLimitBlockedOff admittiert,
       oder durch einen Rang-1-7-Verwurf soeben geleert – #22s eigener
       Vertrag behandelt ein fehlendes Feedback bereits korrekt als
       konservatives Einfrieren mit geschlossenem Fenster; #23 muss dafür
       keinen zusätzlichen expliziten Rejected-Wert erzeugen und riskiert
       damit auch nicht, versehentlich eine bereits überholte Sequenz zu
       referenzieren)

acceptedCommand.direction in {Heating, Cooling}
    -> feedbackForAcceptedRequest = PreviousControlRequestFeedback{
           controlRequestSequence = acceptedCommand.sequence,
           disposition = f(aktueller Rang)
       }
```

Disposition `f(Rang)`:

| Rang (Abschnitt 8.2) | Disposition |
|---|---|
| 9 `MinimumOnTimeHeld` | `DeferredOrLimited` |
| 10 `MinimumOffTimeHeld`/`PolarityDeadTimeHeld` | `DeferredOrLimited` |
| 11 `DirectionChangeApplied` | `NoIntegratorConstraint` |
| 13 `AccumulatingBelowThreshold` | `DeferredOrLimited` |
| 13 `MinimumPulseTriggered` | `NoIntegratorConstraint` |
| 13 `ScheduledWithinWindow` (unabhängig davon, ob dieser konkrete Tick im An- oder Aus-Teil des Fensters liegt – beides ist normale, unbeschränkte Ausführung der berechneten Quote) | `NoIntegratorConstraint` |

Ränge 1–7 lassen `acceptedCommand` bereits leer zurück (unbedingter Verwurf,
Abschnitt 8.4), sodass für sie stets die erste Zeile der Regel
(`std::nullopt`) greift, **falls** vor dem jeweiligen Verwurf überhaupt ein
Heating-/Cooling-`acceptedCommand` bestand – bestand keines, ändert sich
ohnehin nichts. Damit sendet #23 nie einen expliziten `Rejected`-Wert; die
konservative Wirkung (Integral einfrieren) entsteht identisch über das von
#22 selbst dokumentierte Verhalten bei fehlendem Feedback.

### 9.3 `ActuatorAdmissionOutcome` als eigener, von `ActuatorPlanReason` getrennter Diagnosevertrag

```cpp
enum class ActuatorAdmissionOutcome : std::uint8_t {
    NoCandidate,             // newEvaluation war std::nullopt
    Accepted,                 // Kandidat hat alle Pruefungen bestanden
    MalformedCandidate,       // strukturell ungueltig
    DuplicateOrOldSequence,   // Sequenz <= Hochwasserzeichen
    StaleOnArrivalWatchdog,   // createdAtMonotonicMillis bereits zu alt
    StaleOnArrivalContext,    // Kontext bei Ankunft bereits fremd
};
```

`ActuatorPlanTickResult::admissionOutcome` trägt dieses Ergebnis getrennt von
`reason` (welches die physische Tick-Entscheidung beschreibt). Beide Felder
beschreiben unterschiedliche Fragen: „Was geschah mit dem gerade
eingetroffenen Kandidaten?" versus „Warum ist der physische Ausgang dieses
Ticks so, wie er ist?". `DuplicateOrOldSequence` ist damit vollständig
typisiert und widerspruchsfrei, ohne `ActuatorPlanReason` künstlich zu
erweitern.

### 9.4 n/n+1/n+2-Beispiel (Heating -> Cooling)

```text
#22 Evaluation n   -> Heating Request A (sequence=A)
   Phase A akzeptiert A -> acceptedCommand = A
   physicalDirection wird Heating (Rang 11, Fall a oder b je nach Vorzustand)
   folgende Ticks: Rang 13 -> Feedback fuer A: NoIntegratorConstraint/DeferredOrLimited

#22 Evaluation n+1 -> Cooling Request B (sequence=B > A)
   Phase A akzeptiert B -> acceptedCommand = B (sofort, unabhaengig vom physischen Zustand)
   physicalDirection bleibt Heating (Rang 9, MinimumOnTimeHeld, da Mindest-Einzeit von A noch aktiv)
   Feedback fuer DIESEN Tick und alle folgenden, bis B enabled oder ersetzt wird: DeferredOrLimited fuer B
   (A erhaelt ab diesem Zeitpunkt kein Feedback mehr - es ist nicht mehr acceptedCommand)
   -> vor #22-Aufruf n+2 erhaelt #22 exakt das zuletzt berechnete Feedback fuer B, nicht fuer A

#22 Evaluation n+2 -> liest evidence.previousControlRequestFeedback (fuer B) und wertet entsprechend aus
```

Spiegelbildlich für Cooling -> Heating.

## 10. Lüfterlogik

- **Außenlüfter**: `outerFanEnabled = true`, sobald `physicalDirection !=
  Idle`. Beim Übergang auf physisch `Idle` wird
  `outerFanDeactivationRequestedAtMonotonicMillis = now` gesetzt;
  `outerFanEnabled` bleibt `true`, bis `deadlineReached(now,
  outerFanDeactivationRequestedAtMonotonicMillis, outerFanPostRunMillis)`.
  Eine erneute Freigabe während des Nachlaufs setzt
  `outerFanDeactivationRequestedAtMonotonicMillis` auf `std::nullopt`
  zurück, ohne dass der Lüfter zwischenzeitlich `false` war. Kein Vorlauf:
  `outerFanEnabled` wird im selben `tick()`-Aufruf gesetzt wie die
  Peltierfreigabe.
- **Innenlüfter**: `innerFanEnabled = true`, solange
  `input.temperatureControlledPhase == true` – unabhängig vom aktuellen
  Peltier-Fensterzustand. Beim Verlassen der temperaturgeregelten Phase
  startet ein eigener, unabhängiger Nachlauf. Kurze Peltier-Auszeiten
  *innerhalb* einer weiterhin temperaturgeregelten Phase lösen keinen
  Innenlüfter-Nachlauf aus.

## 11. Lifecycle-Integration und Stop-Ablauf

`resetActuatorPlanAtBoundary()` ist eine freie Funktion, die **ausschließlich**
von `TemperatureControlApplicationOrchestrator::complete()` aufgerufen wird
– an derselben committed Lifecycle-Grenze, mit demselben
`TemperatureControlLifecycleBoundary`-Wert, im selben Funktionsaufruf, in dem
bereits `resetTemperatureControlAtBoundary()` aufgerufen wird.

```cpp
ActuatorPlanTickResult resetActuatorPlanAtBoundary(
    ActuatorPlanner& planner, ActuatorPlanSinkDriver& driver,
    TemperatureControlLifecycleBoundary boundary,
    std::uint64_t nowMonotonicMillis);
```

Ablauf (identisch für alle sieben Grenzen; **keine** davon ruft
`applyExternalWatchdogFaultReset()` auf, Abschnitt 8.6):

1. `ActuatorPlanTickResult result = planner.forceStop(nowMonotonicMillis);`
   – berechnet denselben Ergebnistyp wie `tick()`: physisch `Idle`;
   Akkumulator/Fenster/Gegenrichtungskandidat/`acceptedCommand` werden
   gemäß Abschnitt 8.4 verworfen; ein bereits laufender
   Außenlüfter-Nachlauf wird **nicht** verkürzt oder abrupt beendet, sondern
   exakt wie bei einem gewöhnlichen Übergang auf `Idle` fortgeschrieben
   (`outerFanDeactivationRequestedAtMonotonicMillis` wird gesetzt, nicht der
   Lüfter direkt auf `false`).
2. `driver.apply(result);` – dieselbe Übersetzungsfunktion wie bei jedem
   gewöhnlichen Tick (Abschnitt 12); keine zweite Ausgabelogik.
3. Danach: `state().physicalDirection == Idle`,
   `state().acceptedCommand == std::nullopt`,
   `state().windowInitialized == false`; `latchedWatchdogFault` bleibt
   unverändert bestehen, falls zuvor gesetzt.

`ActuatorPlanner::forceStop()` ist die einzige RAM-Stop-Operation; es gibt
keine separate `resetRuntime()`-Methode, die den Nachlauf verlieren könnte.

## 12. Sink-Ausgabereihenfolge und Cross-Sink-Testnachweis

`ActuatorPlanSinkDriver::apply(const ActuatorPlanTickResult&)` setzt die
Sinks in dieser Reihenfolge:

**Freigabe Heating:** 1. `peltier_.setReverse(false)`; 2.
`outerFan_.setEnabled(true)`; 3. `peltier_.setForward(true)`.

**Freigabe Cooling:** 1. `peltier_.setForward(false)`; 2.
`outerFan_.setEnabled(true)`; 3. `peltier_.setReverse(true)`.

**Abschalten:** 1. `peltier_.setForward(false)`; `peltier_.setReverse(false)`;
2./3. Außen-/Innenlüfter gemäß jeweiligem Nachlauf.

**Richtungswechsel:** Ein direkter Wechsel `Forward true -> Reverse true`
(oder umgekehrt) ist durch Rang 9–11 bereits strukturell ausgeschlossen
(mindestens ein Tick mit `appliedDirection == Idle` dazwischen); der Driver
setzt zusätzlich defensiv vor jedem `setForward(true)`/`setReverse(true)`
immer zuerst die jeweils andere Richtung explizit `false`.

### 12.1 Test-only gemeinsamer Call-Trace (löst die Cross-Sink-Testbarkeitslücke)

Die vorhandenen `MockBidirectionalActuatorSink`/`MockBinaryOutputSink`
besitzen **getrennte** `commandJournal()`-Vektoren; sie können allein
**keine** globale Reihenfolge über mehrere Objekte hinweg beweisen. Für den
Testfall wird deshalb in `test/test_actuator_plan_sink_driver/` ein kleiner,
rein testlokaler Hilfstyp ergänzt (keine Produktions- oder
`device_platform_test_support`-Erweiterung):

```cpp
// test-only, lokal in test/test_actuator_plan_sink_driver/
struct SharedActuatorCallTrace {
    enum class Sink : std::uint8_t { Peltier, OuterFan, InnerFan };
    struct Entry { Sink sink; std::uint8_t call; bool value; };  // call: Forward=0/Reverse=1 fuer Peltier, sonst 0
    std::vector<Entry> entries;
};

class TracingBidirectionalActuatorSink final
    : public device_platform::IBidirectionalActuatorSink {
   public:
    TracingBidirectionalActuatorSink(
        device_platform::IBidirectionalActuatorSink& inner,
        SharedActuatorCallTrace& trace);
    void setForward(bool enabled) override;   // ruft inner_.setForward() UND trace_.entries.push_back(...)
    void setReverse(bool enabled) override;
   private:
    device_platform::IBidirectionalActuatorSink& inner_;
    SharedActuatorCallTrace& trace_;
};

class TracingBinaryOutputSink final : public device_platform::IBinaryOutputSink {
    // analog, wrapt IBinaryOutputSink&, mit Sink::OuterFan/InnerFan-Tag
};
```

Ein Test konstruiert die drei realen `Mock*Sink`-Instanzen, wrapt jede in
einen Tracing-Decorator mit gemeinsamer `SharedActuatorCallTrace`, übergibt
die Decorators an `ActuatorPlanSinkDriver` und prüft:

- Freigabe Heating: `trace.entries` enthält in dieser Reihenfolge
  `{Peltier,Reverse,false}`, `{OuterFan,-,true}`, `{Peltier,Forward,true}`
  (Gegenrichtung vor Fan vor gewünschter Richtung);
- Freigabe Cooling spiegelbildlich;
- Abschaltreihenfolge korrekt;
- ein Richtungswechsel enthält mindestens einen Trace-Zeitpunkt mit
  `Forward=false ∧ Reverse=false` zwischen den beiden aktiven Zuständen;
- `simultaneousActivationObserved() == false` über die gesamte Sequenz
  (weiterhin über den zugrunde liegenden echten `MockBidirectionalActuatorSink`
  geprüft, da der Decorator jeden Aufruf durchreicht).

Die vorhandenen Mocks bleiben die alleinige Quelle für Exklusivitäts- und
Einzelsink-Journalprüfung; der Decorator fügt ausschließlich eine
globale Reihenfolge über mehrere Objekte hinweg hinzu und ist keine neue
Produktionsabstraktion.

## 13. Vollständiger Parametervertrag

```cpp
struct ActuatorPlannerParameters {
    std::uint64_t switchingWindowMillis{0U};
    std::uint64_t minimumOnMillis{0U};
    std::uint64_t minimumOffMillis{0U};
    std::uint64_t polarityDeadTimeMillis{0U};
    std::uint64_t pulseAccumulatorCapMillis{0U};
    double counterDirectionConfirmationQuoteThreshold{0.0};
    std::uint64_t counterDirectionConfirmationDurationMillis{0U};
    std::uint64_t requestWatchdogMillis{0U};
    std::uint64_t outerFanPostRunMillis{0U};
    std::uint64_t innerFanPostRunMillis{0U};
};

enum class ActuatorPlannerParametersValidation : std::uint8_t {
    Unconfigured,
    Valid,
    Invalid,
};

[[nodiscard]] ActuatorPlannerParametersValidation
classifyActuatorPlannerParameters(const ActuatorPlannerParameters& p);
```

**Klassifikationsalgorithmus (vollständig, keine erfundene numerische
Produktionsgrenze):**

```text
allZero = (switchingWindowMillis == 0 && minimumOnMillis == 0 &&
           minimumOffMillis == 0 && polarityDeadTimeMillis == 0 &&
           pulseAccumulatorCapMillis == 0 &&
           counterDirectionConfirmationQuoteThreshold == 0.0 &&
           counterDirectionConfirmationDurationMillis == 0 &&
           requestWatchdogMillis == 0 &&
           outerFanPostRunMillis == 0 && innerFanPostRunMillis == 0)

if allZero: Unconfigured

sonst prüfe ALLE folgenden Relationen; hält jede: Valid, sonst Invalid:
  (1) switchingWindowMillis > 0
  (2) minimumOnMillis > 0
  (3) minimumOnMillis <= switchingWindowMillis
  (4) minimumOffMillis > 0
  (5) polarityDeadTimeMillis > 0
      (strukturell: eine "Totzeit" von exakt 0 ist begrifflich keine Totzeit;
      die firmwarefeste numerische Mindestgrenze selbst bleibt
      TBD_COMMISSIONING und wird hier NICHT als Zahl vorausgesetzt)
  (6) pulseAccumulatorCapMillis >= minimumOnMillis
      (muss mindestens einen vollen Mindestimpuls aufnehmen können)
  (7) counterDirectionConfirmationQuoteThreshold ist finit UND
      0.0 < counterDirectionConfirmationQuoteThreshold <= 1.0
  (8) counterDirectionConfirmationDurationMillis > 0
  (9) requestWatchdogMillis > 0
  (10) outerFanPostRunMillis: keine Relation nötig (0 = zulässiger
       "kein Nachlauf"-Wert, jeder uint64_t-Wert strukturell gültig)
  (11) innerFanPostRunMillis: wie (10)
```

Jede Mischkonfiguration (mindestens ein Feld `0`, mindestens ein anderes
Feld `!= 0`, ohne dass alle Relationen 1–11 zusätzlich erfüllt sind) ergibt
`Invalid`. Ein vollständig konsistenter Satz mit alle Relationen erfüllt
ergibt `Valid`, unabhängig vom konkreten (frei wählbaren) Zahlenwert.

**Output-Wirkung (vollständig, keine Lücke mehr zu Abschnitt 8.2):**

| `classifyActuatorPlannerParameters` | `ActuatorPlanStatus` | `ActuatorPlanReason` | physisch |
|---|---|---|---|
| `Unconfigured` | `Unconfigured` | `NoCommissioning` | `Idle`, unbedingter Verwurf (Rang 2) |
| `Invalid` | `InvalidInput` | `InvalidConfiguration` | `Idle`, unbedingter Verwurf (Rang 2) |
| `Valid` | abhängig vom weiteren Rang | abhängig vom weiteren Rang | abhängig vom weiteren Rang |

Diese Tabelle beseitigt den in Revision 2 vorhandenen Widerspruch
vollständig: Rang 2 verwirft in **jedem** Fall (`Unconfigured` wie
`Invalid`) unbedingt, identisch zu allen anderen Rang-1-bis-7-Ereignissen
(Abschnitt 8.4).

## 14. Objektlebenszeit und Lauf-Snapshot-Bindung (Integrationsgate #106)

### 14.1 Boot-session-fixes Objektmodell (löst den Referenz-/Rebinding-Widerspruch)

`ActuatorPlanner` ist ein **langlebiges** Objekt: Es wird einmalig bei
Composition-Root-Start konstruiert und lebt für die gesamte Boot-Session,
exakt analog zu `TemperatureController` (das ebenfalls unveränderliche
`TemperatureControlParameters` bei Konstruktion übernimmt und nie
rekonstruiert wird). `TemperatureControlApplicationOrchestrator` hält
`ActuatorPlanner&` für seine gesamte eigene Lebenszeit – es gibt **keine**
Rekonstruktion und **kein** Rebinding dieser Referenz an irgendeiner
Lifecycle-Grenze. `ActuatorPlannerParameters` sind damit für die gesamte
Boot-Session fix, nicht nur „unveränderlich pro Lauf".

Dieses Modell erfüllt bereits vollständig die **schwächere** Garantie
„keine Parameteränderung mitten in einem laufenden Prozess" (da es
überhaupt keinen Änderungspfad gibt). Es erfüllt **nicht** die
**stärkere**, in `ACTUATOR_TIMING.md` verlangte Garantie „unterschiedliche
Läufe können unterschiedliche, zum jeweiligen Laufstart aktuelle
Service-Werte verwenden, und eine Serviceänderung wirkt nur auf zukünftige
Läufe" – dafür gibt es in dieser Implementierung schlicht **keinen**
Mechanismus, echte Pro-Lauf-Werte überhaupt entgegenzunehmen.

### 14.2 Recovery

Da es in dieser Implementierung keine Pro-Lauf-Parametervariation gibt,
verwendet **jede** Berechnung – auch nach Recovery – zwangsläufig dieselben,
für die Boot-Session fixen Parameter. Das ist **kein** Beweis dafür, dass
Recovery „automatisch korrekt" den für den unterbrochenen Lauf wirksamen
Wert verwendet: Nach einem echten Neustart liest die Composition Root beim
Start die dann aktuellen Service-Einstellungen neu (dies ist die einzige
Stelle, an der Werte überhaupt in die Konstruktion einfließen) – **nicht**
den zum Zeitpunkt des unterbrochenen Laufes wirksamen Wert, falls dieser
zwischenzeitlich geändert wurde. Das ist exakt die von R2.6 benannte,
weiterhin offene Lücke; sie wird hier bewusst **nicht** als gelöst
behauptet.

### 14.3 Benanntes, blockierendes Integrationsgate: Issue #106

Nach ausdrücklicher Rückfrage beim Owner (Entscheidung: eigenes Tracking-Issue
statt Erweiterung des #23-Scopes) wurde **Issue #106** „Aktorplaner
Per-Run-Parameter-Snapshot und Recovery-Bindung" angelegt. Es ist
Abhängigkeit dieses Plans für jede produktive Verdrahtung und definiert:

- einen unveränderlichen Pro-Lauf-Parametersnapshot, erzeugt genau einmal
  bei `NewActiveRun` aus den dann aktuellen Service-Einstellungen und
  danach für die Lebensdauer des Laufes fix;
- eine Recovery-Bindung, die ausschließlich den persistierten Snapshot des
  unterbrochenen Laufes liest, nie aktuelle Live-Servicewerte;
- eine ausführbare Objekt-/Lebenszeit-Anbindung ohne dangling oder
  nicht-rebindbare Referenzsemantik (konkrete Wahl bleibt dem
  #106-eigenen Plan vorbehalten);
- vollständige DoD und Akzeptanzkriterien (siehe Issue #106).

**Blockierende Wirkung, verbindlich für diesen Plan:** Eine produktive
Aktorverdrahtung von Issue #23 (insbesondere ein reales
`ActuatorSafetyGateStatus::Allowed` in Produktion sowie jede reale, über
rein native Tests hinausgehende Zuordnung von `ActuatorPlannerParameters`)
darf **nicht** erfolgen, solange Issue #106 offen ist. Die in diesem Plan
beschriebene #23-Implementierung selbst ist davon nicht blockiert – sie ist
bereits jetzt vollständig nativ testbar mit frei gewählten Testparametern
und bleibt so lange auf boot-session-fixe Parameter beschränkt, bis #106
abgeschlossen ist.

## 15. Adopt-or-build

| Baustein | Geprüfter Kandidat | Entscheidung | Begründung |
|---|---|---|---|
| Fachlicher #23-Kern (Fenster, Mindestzeiten, Totzeit, Akkumulator, Request-Kontext, Watchdog, #22-Feedback, #24-Gate) | ESP-IDF `esp_timer`/GPTimer, MCPWM; `espressif/*`-Registry (keine passende fachliche Komponente); Arduino PID/QuickPID (`NOT_SELECTED`) | **build** | `esp_timer`/GPTimer und MCPWM sind Hardware-/Peripherieprimitiven für Zeitgabe beziehungsweise PWM-Erzeugung; sie ersetzen nicht den portablen, fachlich definierten #23-Vertrag. Eine Delegation würde entweder Hardwareabhängigkeit in den hardwarefreien Fachkern tragen (verboten laut ADR-013) oder den kanonischen #22/#23/#24-Vertrag verwischen. Keine geprüfte Komponente deckt Mindestzeiten, Totzeit, Impulsakkumulator und Kontext-Watchdog in dieser fachlichen Kombination ab. |
| Zeitquelle | bereits vorhandener `device_platform::ITimeSource`/`VirtualTimeSource` | **adopt (bereits vorhanden)** | Deckt monotone Zeit und native Determinismus bereits vollständig ab |
| Bidirektionaler Aktor-Port | bereits vorhandener `device_platform::IBidirectionalActuatorSink` | **adopt (bereits vorhanden)** | Port existiert bereits inklusive Mock mit Reihenfolge-/Exklusivitätsprüfung, bisher ungenutzt |
| Binärer Ausgangs-Port | bereits vorhandener `device_platform::IBinaryOutputSink` | **adopt (bereits vorhanden)** | Zwei Instanzen (außen/innen), kein neuer Port nötig |
| Spätere Hardware-Adapterprimitiven (`esp_timer`, GPTimer, MCPWM) | s.o. | **spätere Adapterentscheidung, nicht Teil dieses PR** | Kämen frühestens beim `device_platform_esp_idf`-Adapter infrage; keine Vorwegnahme hier |

Es wird keine neue Drittabhängigkeit vorgeschlagen; `docs/THIRD_PARTY_COMPONENTS.md`
wird durch diesen Plan nicht geändert.

## 16. SOLID, DRY, KISS

- **Single Responsibility:** `ActuatorPlanner` entscheidet ausschließlich
  Timing/Aktorplanung; `ActuatorPlanSinkDriver` übersetzt ausschließlich ein
  fertiges Ergebnis in Sink-Aufrufe.
- **Open/Closed:** Die bestehende `TemperatureControlApplicationOrchestrator`-
  Grenze wird erweitert, nicht dupliziert.
- **Liskov/Interface Segregation:** Ausschließlich bereits bestehende
  schmale Ports; keine neuen virtuellen Interfaces für Werttypen ohne
  zweite Implementierung.
- **Dependency Inversion:** `ActuatorPlanner` hängt von keiner Hardware ab.
- **DRY:** Eine einzige Quelle für Richtung (`AbstractControlDirection`),
  Request-Identität/-Kontext, Lifecycle-Grenzen, Phasenklassifikation und
  Application-/Lifecycle-Autorität; ein einziger, bereits vorhandener
  Feedback-Übergabemechanismus; eine einzige Ausgabereihenfolge-
  Implementierung für Tick- und Stop-Pfad; ein einziges Feedback-Subjekt
  (`acceptedCommand`, Abschnitt 9.1) statt eines parallelen „alte
  Sequenz"-Konzepts.
- **KISS:** `ActuatorSafetyGateInput` bleibt ein einfacher Werttyp; kein
  zweiter Test-Recorder in Produktionscode (Decorator bleibt test-only);
  Feedback verzichtet bewusst auf einen expliziten `Rejected`-Pfad, wo ein
  einfaches `nullopt` bereits die vertraglich korrekte, dokumentierte
  #22-Fallback-Wirkung erzielt (Abschnitt 9.2); keine vorsorgliche
  Generalisierung ohne aktuellen Bedarf.

## 17. Safety-, Security-, Recovery- und Hardwaregrenzen

- Fail-closed bei jedem Rang 1–8 aus Abschnitt 8.2: `Idle`, kein Guthaben-
  Nachholen.
- Keine Aktorfreigabe wird bei Boot, Reset, Fehler, unbekanntem Zustand oder
  unbestätigter Hardware vorausgesetzt; `ActuatorSafetyGateInput` startet mit
  `Unresolved` und erzwingt `Idle`.
- `ImmediateStop` überstimmt die Mindest-Einschaltzeit ausschließlich in
  Richtung „sicherer machen", niemals in Richtung „Freigabe erzwingen".
- Ein Watchdog-Fault bleibt latched, bis ausschließlich eine externe,
  #24-getriebene Logik `applyExternalWatchdogFaultReset()` aufruft; weder
  eine neue Request noch irgendeine Lifecycle-Grenze noch ein simulierter
  Neustart lösen ihn (Abschnitt 8.6). Vor produktiver Verdrahtung ist das
  #24-Integrationsgate zur Reboot-Sicherheit dieser Sperre zwingend zu
  schließen.
- Keine Persistenz, keine Recovery-Sonderpfade in dieser Implementierung
  selbst: Der Planer wird nach jedem Lifecycle-Boundary-Stop
  (`forceStop()`) regulär aus `Idle` neu bewertet. Die stärkere,
  laufgebundene Recovery-Garantie ist explizit über Issue #106 offen
  (Abschnitt 14), nicht stillschweigend als erfüllt behauptet.
- Kein Türkontakt, keine Kühlkörper-Grenzwertlogik, keine absolute
  Temperatursicherheit – bleibt #24/`SAFETY_AND_FAULTS.md`.
- Keine Hardwarewerte werden in diesem Plan festgelegt; `OPEN_POINTS.md`
  (#29, #32, #33, #35) bleibt unverändert sichtbar offen.

## 18. Umsetzungs- und Commit-Schnitte

1. **Gemeinsame schmale Verträge / Klassifikation** – `actuator_plan_types.hpp`
   vollständig (Abschnitt 4.2, 7, 8.2, 8.3, 8.4, 8.6, 9.3, 13),
   `classifyActuatorDemand()`, `classifyActuatorPlannerParameters()`
   inklusive vollständiger struktureller Invarianten; keine
   Verhaltenslogik.
2. **Phase A / Annahme** – `ActuatorPlanner::tick()` Grundgerüst: Admission
   (Abschnitt 6.2), laufender Watchdog (6.4), Prioritätsleiter Rang 1–8
   (8.2), overflow-sichere Zeitarithmetik (8.3), `forceStop()`,
   `applyExternalWatchdogFaultReset()`.
3. **Fenster, Akkumulator, Mindestzeiten, Totzeit, Richtungswechsel** –
   Fensterlogik inklusive nicht-zirkulärem Start und O(1)-Fortschritt
   (8.1), einziger Akkumulator mit vollständigen Verwurfsregeln (8.4),
   bestätigter Richtungswechsel (8.5), Prioritätsleiter Rang 9–13
   vollständig.
4. **Lüfterlogik** – Außen-/Innenlüfter (10), eigener, von der
   Fenster-/Mindestzeitlogik klar getrennter Codeabschnitt.
5. **Feedback-Dispositionsmatrix** – vollständige Umsetzung von Abschnitt 9
   innerhalb von `tick()`/`forceStop()`.
6. **Sink-Driver und Application-/Lifecycle-Integration** –
   `ActuatorPlanSinkDriver` (12), Erweiterung von
   `TemperatureControlApplicationOrchestrator` um `tickActuatorPlan()` und
   die gemeinsame Lifecycle-Stop-Integration (11); keine
   Composition-Root-Verdrahtung.
7. **Tests und Dokumentation** – vollständige native Testklassen gemäß
   Abschnitt 19; `docs/ACTUATOR_TIMING.md` „Akzeptierte Entscheidungen" um
   Strukturhinweise ergänzen, ohne `TBD_COMMISSIONING`-Werte zu erfinden.
8. **Abschlussnachweise** – gezielte und (nach Owner-Freigabe) vollständige
   lokale Läufe, PR-Nachweis, `SESSION HANDOVER`.

## 19. Teststrategie

Alle Tests sind native, deterministische Orakel unter `pio test -e native`
und verwenden ausschließlich `VirtualTimeSource`,
`MockBidirectionalActuatorSink`/`MockBinaryOutputSink` sowie die neuen
test-only Tracing-Decorators (Abschnitt 12.1). Testparameter sind frei
gewählte, plausible Testwerte – keine Produktionskommissionierung.

### 19.1 `test/test_actuator_planner/test_actuator_planner.cpp`

**Fensterbootstrap (R2.1):** kleine Heating-Quote aus vollständig leerem
Planner akkumuliert korrekt über mehrere Fenster; Mindestimpuls wird nach
exakt der erwarteten Fensteranzahl ausgelöst; spiegelbildlich Cooling; reine
`Idle`-Anforderung erzeugt weiterhin kein Guthaben; Fall (a) und Fall (b) aus
Abschnitt 8.1 sind je durch einen eigenen Test abgedeckt.

**Explizite `NoValidRequest`-Evaluation (R2.2):** eine aktive
Heating-/Cooling-Freigabe innerhalb ihrer Mindest-Einschaltzeit wird durch
eine neue, strukturell gültige `Unavailable`/`InvalidInput`-Evaluation
sofort beendet (Rang 6); ein `tick()`-Aufruf mit `newEvaluation =
std::nullopt` im selben Szenario tut dies ausdrücklich **nicht** und führt
die laufende Freigabe unverändert fort.

**Feedback (R2.3):** n/n+1/n+2-Orakel für Heating->Cooling und
Cooling->Heating exakt gemäß Abschnitt 9.4; OFF-Admission erzeugt kein
nachträgliches Feedback für die zuvor aktive Request; jede Zeile der Tabelle
aus Abschnitt 9.2; `admissionOutcome` ist für jeden Fall aus Abschnitt 9.3
direkt geprüft.

**AirLimit-Klassifikation:** `NeutralOff` vs. `AirLimitBlockedOff`:
identische Mindestzeit-Behandlung, unterschiedliche Akkumulatorwirkung;
aufgebautes Guthaben, danach `AirLimitBlockedOff`: Guthaben verworfen;
Übergang zu `AirLimitReducedDemand`: Guthaben verworfen, folgende Fenster
akkumulieren nur aus der reduzierten Quote; fortlaufendes Verbleiben in
`AirLimitReducedDemand`: normale Akkumulation.

**Langes Idle (neu entdeckt beim eigenen Review):** aufgebautes
Heiz-Guthaben, danach eine sehr lange Folge von `NeutralOff`-Ticks, danach
erneute kleine Heiz-Anforderung: Guthaben wurde beim Übergang nach `Idle`
vollständig verworfen, kein sofortiger Mindestimpuls aus altem Guthaben;
Fensterstart beginnt strukturell frisch (Fall a).

**Safety-Gate:** `Unresolved`, `Allowed`, `ImmediateStop`; `ImmediateStop`
überstimmt eine aktive Mindest-Einschaltzeit; `Unresolved` erzwingt `Idle`
auch bei ansonsten vollständig gültiger Request.

**Watchdog-Fault-Evidenz (R2.4):** Watchdog-Trip erzeugt
`ActuatorWatchdogFaultEvidence`; eine danach eintreffende, ansonsten gültige
neue Request löscht ihn nicht; `NewActiveRun`-Boundary löscht ihn nicht;
`Recovery`/`Standby`/`Service`/`Fault`/`SafeBoot`/`LeaveTemperatureControl`
löschen ihn ebenfalls nicht; nur `applyExternalWatchdogFaultReset()` löscht
ihn; ein simulierter Neustart (neue `ActuatorPlanner`-Instanz) wird
ausdrücklich **nicht** als Ersatz für diesen Reset getestet oder behauptet.

**Einziger Akkumulator (R7):** Heiz-Guthaben aufgebaut, kurze (nicht
bestätigte) Cool-Gegenanforderung lädt kein zweites Guthaben und wird nach
Abbruch der Bestätigung spurlos verworfen; bestätigter Wechsel verwirft das
Heiz-Restguthaben; spiegelbildlicher Test für Cool -> Heat; Gegenrichtung vor
Ablauf der Mindest-Einschaltzeit bleibt physisch wirkungslos, während der
Bestätigungstimer unabhängig weiterläuft.

**Fenster-/Impulsplatzierung/Overflow (R2.8):** Quote `0`, kleine, normale,
volle Quote; Akkumulator unterhalb/exakt auf/oberhalb der Schwelle;
Akkumulator-Obergrenze; Rundungs-Grenzfälle; neue Quote mitten im Fenster
wirkt erst im nächsten Fenster; Fensterstart und mehrere Fensterwechsel mit
`windowStartMonotonicMillis`/`nowMonotonicMillis` nahe `UINT64_MAX` ohne
Additionsüberlauf (direkter Test der in Abschnitt 8.1 bewiesenen Schranke);
eine sehr lange Pause (mehrere Fenster) führt zu genau einem
Fensterstart-Ereignis ohne Nachholen der übersprungenen Fenster.

**Mindestzeiten/Totzeit:** Mindest-Einschaltzeit hält aktive Richtung trotz
neuer `Idle`-Anforderung (sofern nicht Rang 6 zutrifft); Mindest-Auszeit
verhindert verfrühte erneute Freigabe; Polaritätstotzeit: späteres Ende von
Mindest-Auszeit/Totzeit greift (beide Richtungen sowie exakter
Gleichstand-Grenzfall); Heizen -> Kühlen und Kühlen -> Heizen symmetrisch;
niemals gleichzeitig `Forward` und `Reverse` über eine lange Sequenz.

**Parameterklassifikation (R2.5):** vollständige Tabelle aus Abschnitt 13
(alle Felder `0` -> `Unconfigured`; jede einzelne strukturell unmögliche
Relation einzeln getestet -> `Invalid`; vollständig konsistente Testwerte
-> `Valid`); Rang-2-Verwurf ist für `Unconfigured` und `Invalid`
identisch unbedingt (kein „unverändert"-Fall mehr).

**Zeit-/Overflow-Verträge:** Gleichheit an jeder Frist; knapp
davor/genau darauf/knapp danach für Mindest-Ein-/Auszeit, Totzeit,
Bestätigungsdauer, Watchdog, Fan-Nachlauf; Retrograde-Zeit an jeder
Referenzzeit (-> `TimeInvalid`); Werte nahe `UINT64_MAX`.

**Priorität/Fail-closed:** malformed `ControlRequest` (Sequenz `0`,
unbekannte Richtung, `timeQuote` `NaN`/`Infinity`/außerhalb `[0,1]`,
Status/Request-Mismatch, ungültiger Kontext) -> `MalformedEvaluation`,
sofortiger Verwurf; mehrere gleichzeitig zutreffende Bedingungen (z. B.
Safety `ImmediateStop` **und** Kontext-Stale gleichzeitig) -> exakte
Rang-Reihenfolge aus 8.2.

**Lifecycle/Stop (R6):** für jede der sieben `TemperatureControlLifecycleBoundary`-
Werte: `forceStop()` liefert korrekten Nachlauf, verwirft Akkumulator/
Fenster/Gegenrichtungskandidat/`acceptedCommand`, lässt `latchedWatchdogFault`
unverändert; „aktives Peltier -> `Fault`" und „aktives Peltier -> `Standby`"
mit weiterlaufendem Außenlüfter-Nachlauf statt abruptem Fan-Stopp.

**Sequenzhochwasserzeichen:** `lastAcceptedSequence` bleibt über
`forceStop()` innerhalb derselben `ActuatorPlanner`-Instanz erhalten; eine
neue Instanz (simulierter Neustart) beginnt regulär bei einer neuen,
niedrigeren Sequenz, ohne dies fälschlich als persistierten Zustand zu
behaupten.

**Architekturnachweis:** `ActuatorPlanner` kompiliert und wird getestet ohne
jede Abhängigkeit auf `device_platform`-Sink-Header.

### 19.2 `test/test_actuator_plan_sink_driver/test_actuator_plan_sink_driver.cpp`

Vollständige Cross-Sink-Reihenfolgeprüfung über `SharedActuatorCallTrace`
gemäß Abschnitt 12.1: Freigabe Heating/Cooling, Abschalten, Richtungswechsel
mit garantiertem Off-Zwischenzustand; `simultaneousActivationObserved() ==
false`; korrekte Weitergabe von `outerFanEnabled`/`innerFanEnabled` an die
jeweils richtige `IBinaryOutputSink`-Instanz.

### 19.3 `test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp` (gezielte Ergänzung)

- `tickActuatorPlan()` leitet `currentCanonicalContext`/
  `temperatureControlledPhase` korrekt aus der bestehenden
  `resolveEffectiveControlContext()`/`isTemperatureControlledProcessState()`-
  Kette ab;
- `complete()`/`needsRuntimeReset()` ruft `resetActuatorPlanAtBoundary()` für
  dieselbe committed Lifecycle-Grenze auf wie
  `resetTemperatureControlAtBoundary()` (ein einziger Test-Fixture-Aufruf
  löst nachweislich beide Resets aus);
- `feedbackForAcceptedRequest` aus einem `tickActuatorPlan()`-Aufruf landet
  unverändert im nächsten `evaluateTemperatureControl()`-Aufruf über
  `TemperatureControlEvaluationEvidence.previousControlRequestFeedback`;
- `ActuatorPlanner&`/`ActuatorPlanSinkDriver&` werden über die gesamte
  Lebenszeit des Test-Fixtures unverändert referenziert (Objektlebenszeit-
  Nachweis gemäß Abschnitt 14.1, kein Rebinding).

Bei geänderten gemeinsamen Verträgen werden zusätzlich die direkt betroffenen
#22-Konsumententests gezielt mitgeführt, sofern die Umsetzung dort
tatsächlich etwas berührt – in diesem Plan ist das nicht vorgesehen.

## 20. Dokumentations- und Roadmapwirkung sowie Plan-SHA-Governance

- `docs/ACTUATOR_TIMING.md`: nach Umsetzung Ergänzung der akzeptierten
  Struktur im Abschnitt „Akzeptierte Entscheidungen", ohne die dort
  weiterhin offenen `TBD_COMMISSIONING`-Werte zu verändern.
- Kein neues ADR erwartet: Modulzuordnung folgt unverändert ADR-013,
  Zustandsautomat-Trennung unverändert ADR-014.
- `docs/THIRD_PARTY_COMPONENTS.md` bleibt unverändert.
- **Governance:** Diese Revision-3-Planänderung wird als eigener,
  abgeschlossener Plan-Commit committet. Erst danach ist die exakte
  Revision-3-Plan-SHA bekannt und wird im Draft-PR #105 und im `SESSION
  HANDOVER` ausgewiesen. Eine Nachführung von `docs/ROADMAP.md` auf die
  exakte Revision-3-SHA erfolgt in einem separaten, rein redaktionellen
  Metadaten-Commit direkt im Anschluss – nicht durch eine vierte
  Planrevision. Issue #106 wurde bereits vor diesem Plan-Commit angelegt
  (siehe Abschnitt 1) und ist damit zum Zeitpunkt der Freigabe bereits
  live nachverfolgbar.

## 21. Offene Fragen und materielle Risiken

- **Kadenzanforderung an den Aufrufer** (Abschnitt 6.6): Dieser Plan setzt
  voraus, dass der Aufrufer `tickActuatorPlan()` deutlich häufiger aufruft
  als die kürzeste vorkommende geplante Ein-/Aus-Dauer. Die konkrete
  Aufrufer-Kadenz ist Teil der noch ausstehenden Composition-Root-
  Verdrahtung (außerhalb dieses Plans) und wird dort dokumentiert.
- **Issue #106 als eigenständiges, paralleles Vorhaben:** Der Owner hat sich
  für ein separates Tracking-Issue statt einer #23-Scope-Erweiterung
  entschieden. Damit bleibt #23 selbst vollständig umsetzbar und nativ
  testbar, ohne auf #106 zu warten; lediglich die **produktive**
  Aktorverdrahtung bleibt bis zum Abschluss von #106 gesperrt
  (Abschnitt 14.3). Sollte der Owner diese Reihenfolge später ändern
  wollen, ist das eine materielle Abweichung und erfordert eine neue
  Planrevision.
- **`acceptedCommand` als alleiniges Feedback-Subjekt** (Abschnitt 9): Diese
  Vereinfachung verzichtet bewusst auf einen expliziten `Rejected`-Pfad und
  verlässt sich auf #22s dokumentiertes Fallback-Verhalten bei fehlendem
  Feedback. Sollte sich bei der Umsetzung zeigen, dass der tatsächliche
  #22-Code (nicht nur der Plan) an einer Stelle doch ein explizites
  `Rejected` statt `nullopt` erwartet, ist das ein materieller Befund gegen
  den bestehenden #22-Code (nicht gegen diesen Plan) und wird als
  Abweichung gemeldet, nicht still umgangen.
