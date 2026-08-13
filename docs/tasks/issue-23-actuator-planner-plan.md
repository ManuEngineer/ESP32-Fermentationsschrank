# Plan: Issue #23 – Aktorplaner, Mindestzeiten, Totzeit und Lüfterlogik

## 1. Status, Scope und Owner-Gate

- Revision: **1**.
- Live-Issue: #23, offen, Status `PLANNED_SPEC_PENDING`.
- Draft-PR: Branch `agent/issue-23-aktorplaner-plan` -> `main`.
- Planpfad: `docs/tasks/issue-23-actuator-planner-plan.md`.
- Diese Revision 1 ist ein vollständiger, eigenständig gültiger Plan. Sie setzt
  keine frühere Planrevision als fachliche oder normative Quelle voraus.
- Planbasis: `main` @ `2986dca5736a34171910c9245a3d5f43fa55da06`
  (Merge-Commit von PR #104 / Issue #22).
- Die Umsetzung bleibt gesperrt, bis der Owner exakt diesen Plan-Commit mit
  `PLAN APPROVED` beziehungsweise `Approved plan commit: <SHA>` freigibt.
- Diese Revision committet ausschließlich Plan- und Roadmap-Dokumentation. Sie
  implementiert keine Produktionslogik, keine produktiven Tests, keine
  Hardware-, GPIO-, Toolchain- oder CI-Änderung.
- Der PR bleibt Draft. Es gibt kein `Ready for review`, keinen Merge, kein
  Auto-Merge und kein Branch-Löschen.

```text
CONTEXT_BASELINE_BRANCH: agent/issue-23-aktorplaner-plan
CONTEXT_BASELINE_SHA: 2986dca5736a34171910c9245a3d5f43fa55da06
CONTEXT_HEAD_BEFORE_REVISION: 2986dca5736a34171910c9245a3d5f43fa55da06
CONTEXT_PLAN_SHA: NONE (wird nach dem Commit dieser Revision eingetragen)
CONTEXT_REFRESH_MODE: FULL
CONTEXT_DELTA: Erstplanung nach Merge von PR #104 (Issue #22). Live-Abgleich
  bestätigt PR #104 MERGED, Issue #22 CLOSED, Branch
  agent/issue-22-pi-regelung-plan remote gelöscht, Issue #23 OPEN ohne
  existierenden Branch/PR, Abhängigkeit #11 CLOSED.
SOURCE_OF_TRUTH_CONFLICT: NONE. `docs/ROADMAP.md` benannte vor dieser Revision
  noch PR #104 als Priorität 1 (Draft) und wird in dieser Revision
  nachgeführt; das ist die erwartete Pflegepflicht aus AGENTS.md, kein
  inhaltlicher Widerspruch.
```

## 2. Ziel, Reihenfolge und Nicht-Ziele

Issue #23 liefert einen deterministischen, hardwarefreien und nativ testbaren
Aktorplaner, der eine gültige abstrakte `ControlRequest` aus Issue #22 in
zeitlich korrekte, abstrakte Aktorbefehle für Peltier und Lüfter übersetzt:

- gemeinsames zeitproportionales Schaltfenster für Heizen und Kühlen;
- begrenzter Impulsakkumulator für Anforderungen unterhalb der
  Mindest-Einschaltzeit;
- Mindest-Einschaltzeit mit Vorrang für Sicherheits-/Fehlerabschaltung;
- Mindest-Auszeit vor erneuter Freigabe;
- bestätigte Gegenrichtungsanforderung (Schwelle + Dauer + Plausibilität) vor
  Richtungswechsel;
- sichere Polaritätstotzeit, kombiniert mit Mindest-Auszeit über
  Späteres-Ende-Regel (keine Addition);
- Außenlüfter ohne absichtlichen Vorlauf, mit Nachlauf;
- Innenlüfter während temperaturgeregelter Phasen und mit eigenem Nachlauf;
- Aktualisierungs-Watchdog für veraltete oder kontextfremde `ControlRequest`;
- ausschließlich abstrakte Aktorbefehle (Richtung/Lüfterzustand plus
  Diagnosegrund) als Ausgabe des fachlichen Kerns – keine GPIO-Ansteuerung im
  Fachkern.

Die fachliche Reihenfolge bleibt (siehe Issue-22-Plan Abschnitt 2 und
`docs/TEMPERATURE_CONTROL.md`):

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
  (`SAFETY_AND_FAULTS.md`).
- Keine Änderung an der PI-/Luftbegrenzungslogik aus Issue #22
  (`temperature_control.*`, `control_context.*`); `ControlRequest`,
  `ControlRequestContext`, `ControlSensorRole`, `AbstractControlDirection` und
  `PreviousControlRequestFeedback` werden ausschließlich wiederverwendet.
- Keine konkreten Sekundenwerte für Schaltfenster, Mindestzeiten, Totzeit,
  Umschaltschwelle, Akkumulatorgrenze oder Nachlaufzeiten; diese bleiben
  `TBD_COMMISSIONING` (Nachverfolgung #35 / `OPEN_POINTS.md`). Der Plan legt
  ausschließlich die Struktur, Typen und das deterministische Verhalten fest.
- Keine Persistenz des Aktorplanerzustands; er bleibt RAM-only wie der
  PI-/Qualifier-Zustand aus Issue #22 und wird an denselben kanonischen
  Lifecycle-Grenzen geleert.
- Keine Änderung an `docs/tasks/issue-22-pi-control-air-limits-plan.md`; dessen
  Verträge werden referenziert, nicht kopiert oder neu definiert.

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
- `docs/TEMPERATURE_CONTROL.md`, insbesondere „Architekturgrenzen“ (Abschnitt
  mit den acht getrennten Verantwortungen) und der bereits implementierte
  #22-Fachkern-Abschnitt;
- `docs/STATE_MACHINE.md` für `ProcessState`-Topologie, Recovery- und
  Lifecycle-Grenzen;
- `docs/SENSOR_TUNING_COMMISSIONING.md` für Integratorregeln und
  Commissioning-Eigentümerschaft;
- `docs/RUN_PERSISTENCE.md` für das Write-before-Apply-Muster (informativ;
  #23 besitzt keinen eigenen Persistenzvertrag);
- `docs/tasks/issue-22-pi-control-air-limits-plan.md`, insbesondere Abschnitt
  4.1 (Zuständigkeiten #22/#23/#24), Abschnitt 7.1 (Request-Identität und
  Kontextfrische, Zeilen 560–602) und Abschnitt 8 (Anti-Windup-Feedback,
  Zeilen 646–698) – dies ist der verbindliche Vertrag, den #23 konsumiert;
- ADR-013 und der Modulindex für die Abhängigkeitsrichtung
  `fermentation_app -> device_platform`;
- `docs/THIRD_PARTY_COMPONENTS.md`, Zeile „Regelung“ (Arduino PID / QuickPID
  `NOT_SELECTED`; eigene Logik bleibt Vertrag);
- `docs/CI_AND_QUALITY_GATES.md` und `docs/AGENT_WORKFLOW.md` für Testbefehle
  und Reviewpflichten;
- `AGENTS.md` (root) sowie `lib/fermentation_app/AGENTS.md` und
  `lib/device_platform/AGENTS.md`.

Bestehende Implementierungen und Tests wurden vor dieser Planung durchsucht.
Wiederzuverwenden sind insbesondere:

| Typ/Funktion | Datei | Verwendung in #23 |
|---|---|---|
| `ControlRequest`, `ControlRequestIdentity` | `temperature_control_types.hpp` | Eingabe des Planers; Sequenz/Timestamp für Watchdog |
| `ControlRequestContext`, `ControlSensorRole` | `control_context_types.hpp` | Kontextfrischeprüfung gegen aktuellen kanonischen Kontext |
| `AbstractControlDirection` | `sensor_selection_types.hpp` | Eingangsrichtung aus #22 (`Heating`/`Cooling`/`Idle`) |
| `PreviousControlRequestFeedback` | `temperature_control_types.hpp` | Ausgabe des Planers zurück an #22 (Disposition) |
| `TemperatureControlResult`, `TemperatureControlStatus` | `temperature_control_types.hpp` | Quelle der `ControlRequest` (nur `Demand`/`Off` tragen eine) |
| `isTemperatureControlledProcessState()` | `control_context.hpp` | Innenlüfter-Phasenklassifikation, keine Parallel-Klassifikation |
| `TemperatureControlLifecycleBoundary` | `temperature_control_orchestrator.hpp` | wiederverwendete Lifecycle-Grenzen für RAM-Reset (keine zweite Aufzählung) |
| `IBidirectionalActuatorSink` | `device_platform/bidirectional_actuator_sink.hpp` | bereits vorhandener, bisher unbenutzter Peltier-Port |
| `IBinaryOutputSink` | `device_platform/binary_output_sink.hpp` | bereits vorhandener, bisher unbenutzter Lüfter-Port (zwei Instanzen: außen/innen) |
| `ITimeSource`, `VirtualTimeSource` | `device_platform/time_source.hpp`, `virtual_time_source.hpp` | monotone Zeit im Orchestrator; native Tests |
| `MockBidirectionalActuatorSink`, `MockBinaryOutputSink` | `device_platform_test_support/` | bereits vorhandene Test-Doubles, keine Neuentwicklung nötig |

Kein paralleler Sensor-, Prozess-, Persistenz- oder PI-Vertrag wird erfunden.
Insbesondere wird `ControlRequestContext` nicht kopiert, sondern exakt wie in
Issue #22 als flüchtige Identität mitgeführt und geprüft.

## 4. Zuständigkeiten und Architekturgrenzen

### 4.1 Abgrenzung zu Issue #22

Issue #22 liefert ausschließlich `HEAT`/`OFF`/`COOL` mit Zeitquote,
Identität (`sequence`, `createdAtMonotonicMillis`) und Kontext
(`processTransitionSequence`, `runRevision`, `controlSensorRole`). Issue #22
kennt weder Schaltfenster noch Mindestzeiten noch Lüfter und behauptet keine
physische Aktorquote. Dieser Plan ändert an `temperature_control.*` und
`control_context.*` nichts; er konsumiert deren Ausgabe unverändert und liefert
im Gegenzug exakt den bereits vereinbarten `PreviousControlRequestFeedback`
für die *unmittelbar vorherige aktive* Request zurück (kein neuer
Feedbackvertrag).

### 4.2 Abgrenzung zu Issue #24

Issue #23 trifft keine Aussage darüber, *warum* eine Sicherheitsabschaltung
verlangt wird. Dafür wird ein schmaler, reiner Werttyp definiert (kein neues
Interface, siehe Abschnitt 6.6), den #24 zukünftig befüllt:

```cpp
struct ActuatorSafetyOverride {
    bool immediateShutdownRequired{false};
};
```

Dieser Typ trägt keine Fehlerklasse, keine Ursache und keine Wiederfreigabe-
logik – das bleibt vollständig Issue #24. Bis #24 existiert, liefert die
Application-/Composition-Root-Schicht einen Default-Wert
(`immediateShutdownRequired = false`); #23 erfindet keine eigene
Ersatz-Sicherheitsklassifikation und keine Kühlkörpersensor-Auswertung. Die
Akzeptanzanforderung „eine externe Sicherheitsabschaltung kann Mindest-Einzeit
überstimmen“ wird strukturell durch diesen Override-Eingang erfüllt und mit
Issue #23s eigenen deterministischen Tests nachgewiesen (Override-Input direkt
gesetzt, keine Fehlerklasse simuliert).

### 4.3 Modulzuordnung nach ADR-013

Der Aktorplaner kennt `ControlRequest`, `ControlSensorRole` und `ProcessState`
– alles fermentationsspezifische beziehungsweise bereits als
`fermentation_app`-Vertrag eingestufte Typen (Issue #22 liegt vollständig in
`lib/fermentation_app/`). Der Planer gehört deshalb konsequent ebenfalls zu
`lib/fermentation_app/`, nicht zu `lib/device_platform/`. Er ist selbst
hardwarefrei und referenziert aus `device_platform` ausschließlich die
bestehenden schmalen Ports (`ITimeSource` für die Application-Schicht,
`IBidirectionalActuatorSink`/`IBinaryOutputSink` für die
Anwendungs-Ausgabeschicht). Es entsteht kein neuer `device_platform`-Port und
keine Rückwärtsabhängigkeit; `check_architecture_boundaries.py` bleibt PASS.

## 5. Betroffene Module und voraussichtliche Dateien

Neue Dateien (alle unter `lib/fermentation_app/src/`, Namensmuster analog zu
`temperature_control_types.hpp` / `temperature_control.hpp` /
`temperature_control_orchestrator.hpp`):

```text
actuator_plan_types.hpp          Werttypen: PeltierDirection, ActuatorPlanReason,
                                  ActuatorSafetyOverride, ActuatorPlannerParameters,
                                  ActuatorPlanInput, ActuatorPlanResult,
                                  ActuatorPlannerRuntimeState
actuator_planner.hpp / .cpp      Reine, deterministische Klasse ActuatorPlanner
                                  (Fenster, Akkumulator, Mindestzeiten, Totzeit,
                                  Richtungswechsel, Watchdog, Lüfterlogik)
actuator_plan_orchestrator.hpp / .cpp
                                  Application-Grenze: verbindet TemperatureControlResult
                                  (#22-Ausgabe) mit ActuatorPlanner, wendet das Ergebnis
                                  auf die device_platform-Sinks an, liefert
                                  PreviousControlRequestFeedback zurück, kapselt
                                  Lifecycle-Reset
```

Neue Testverzeichnisse (analog `test/test_temperature_control/`,
`test/test_control_context/`):

```text
test/test_actuator_planner/test_actuator_planner.cpp
test/test_actuator_plan_orchestrator/test_actuator_plan_orchestrator.cpp
```

Voraussichtlich betroffene, aber nicht inhaltlich geänderte Dateien
(Include-Erweiterung beziehungsweise Composition-Root-Verdrahtung, kein
GPIO-Scope):

```text
lib/fermentation_app/src/temperature_control_orchestrator.hpp / .cpp
    (optional: previousControlRequestFeedback-Rückfluss in
    TemperatureControlEvaluationEvidence dokumentieren; keine
    Vertragsänderung, falls das bestehende Feld bereits ausreicht)
src/main.cpp, main/app_main.cpp
    (Verdrahtung bleibt außerhalb des Plan-only-Scopes; nur so weit
    erwähnt, wie zur Einordnung nötig – keine Umsetzung in diesem Auftrag)
docs/ACTUATOR_TIMING.md
    (Abschnitt „Akzeptierte Entscheidungen“/„Noch offen“ nach Umsetzung
    um implementierte Struktur ergänzen, keine TBD_COMMISSIONING-Werte
    erfinden)
docs/ROADMAP.md
    (bereits in dieser Planrevision aktualisiert, siehe Abschnitt 12)
```

Kein neuer `device_platform`-Port, keine neue Drittbibliothek, keine
Änderung an `run_persistence_*`, `run_commands.*` oder
`process_state_machine.*`.

## 6. Daten-, Zustands- und Schnittstellenverträge

### 6.1 `PeltierDirection` und `ActuatorPlanReason`

```cpp
enum class PeltierDirection : std::uint8_t {
    Off,
    Heating,
    Cooling,
};

enum class ActuatorPlanReason : std::uint8_t {
    NoValidRequest,               // keine gültige ControlRequest (Unavailable/InvalidInput)
    StaleRequestWatchdog,         // Request älter als Watchdog-Zeitraum
    StaleRequestContext,          // Prozess-/Lauf-/Rollenkontext passt nicht mehr
    DuplicateOrOldSequence,       // Sequenz bereits verarbeitet oder älter als letzte akzeptierte
    NeutralIdle,                  // gültige OFF-Anforderung, kein Zwang
    AccumulatingBelowThreshold,   // Anteil < Mindest-Einschaltzeit, wird gesammelt
    MinimumPulseTriggered,        // Akkumulator hat Mindest-Einschaltzeit erreicht
    ScheduledWithinWindow,        // Anforderung >= Mindest-Einschaltzeit, direkt geplant
    MinimumOnTimeHeld,            // aktive Richtung wird wegen Mindest-Einschaltzeit gehalten
    MinimumOffTimeHeld,           // neue Freigabe wegen Mindest-Auszeit gesperrt
    PolarityDeadTimeHeld,         // neue Richtung wegen Totzeit gesperrt
    CounterDirectionConfirming,   // Gegenanforderung noch nicht lang/stark genug bestätigt
    CounterDirectionAbandoned,    // Gegenanforderung vor Bestätigung wieder unzureichend
    DirectionChangeApplied,       // Richtungswechsel nach erfüllten Zeiten freigegeben
    ExternalSafetyOverride,       // ActuatorSafetyOverride erzwingt sofortiges AUS
    RequiredSensorUnavailable,    // gemäß #22-Status kein gültiger Regelwert
};
```

`ActuatorPlanReason` ist eine reine Diagnosekennung ohne Safety-Bedeutung;
#24 erfindet daraus keine Fehlerklasse und #23 erfindet keine
Safety-Ursache (siehe Abschnitt 4.2).

### 6.2 `ActuatorPlannerParameters`

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
    Unconfigured,  // Default-/Nullwerte: NoCommissioning, kein Fachfehler
    Valid,
    Invalid,       // strukturell unmöglich, z. B. minimumOnMillis == 0 mit switchingWindowMillis > 0
};

[[nodiscard]] ActuatorPlannerParametersValidation
classifyActuatorPlannerParameters(const ActuatorPlannerParameters& parameters);
```

Diese Klassifikation spiegelt exakt das in Issue #22 etablierte Muster
(`Unavailable/NoCommissioning` vs. `InvalidInput/InvalidConfiguration`, siehe
`classifyTemperatureControlParameters`). Konkrete Sekundenwerte bleiben
`TBD_COMMISSIONING`; die Validierungsstruktur selbst ist keine Kommissionierung
und wird bereits jetzt implementiert.

Firmwarefeste Mindestgrenzen (z. B. eine niemals unterschreitbare
Mindesttotzeit gemäß `ACTUATOR_TIMING.md`) werden als benannte Konstanten im
`.cpp` eingeführt, sobald die Umsetzung beginnt; dieser Plan reserviert dafür
ausdrücklich Raum, ohne die Zahl vorwegzunehmen.

### 6.3 `ActuatorPlanInput`

```cpp
struct ActuatorPlanInput {
    std::uint64_t nowMonotonicMillis{0U};
    std::optional<ControlRequest> controlRequest;    // aus TemperatureControlResult
    TemperatureControlStatus controlStatus{TemperatureControlStatus::Unavailable};
    ControlRequestContext currentCanonicalContext;   // aktueller #21/#22-Kontext, nicht aus der Request kopiert
    bool temperatureControlledPhase{false};          // isTemperatureControlledProcessState(phase)
    ActuatorSafetyOverride safetyOverride;
};
```

`currentCanonicalContext` wird von der Application-Grenze aus der aktuell
kanonischen Prozess-/Laufquelle projiziert (analog
`resolveEffectiveControlContext`) und nicht aus der zu prüfenden Request
übernommen – sonst könnte eine veraltete Request sich selbst als frisch
bestätigen.

### 6.4 `ActuatorPlanResult`

```cpp
struct ActuatorPlanResult {
    PeltierDirection peltierDirection{PeltierDirection::Off};
    bool outerFanEnabled{false};
    bool innerFanEnabled{false};
    ActuatorPlanReason reason{ActuatorPlanReason::NoValidRequest};
    std::optional<PreviousControlRequestFeedback> feedbackForPreviousRequest;
    std::optional<std::uint64_t> acceptedRequestSequence;
};
```

`feedbackForPreviousRequest` ist optional, weil ohne vorherige aktive
Heating-/Cooling-Request kein Feedback existiert (Issue-22-Plan Abschnitt 8.1:
fehlendes Feedback für eine vorherige aktive Anforderung friert den
Integralanteil ebenfalls ein – die Application-Grenze reicht `std::nullopt`
unverändert an #22 weiter).

### 6.5 `ActuatorPlannerRuntimeState` (RAM-only)

```cpp
struct ActuatorPlannerRuntimeState {
    PeltierDirection activeDirection{PeltierDirection::Off};
    std::uint64_t directionActivatedAtMonotonicMillis{0U};
    std::uint64_t directionDeactivatedAtMonotonicMillis{0U};
    std::uint64_t windowStartMonotonicMillis{0U};
    double pulseAccumulatorMillis{0.0};
    std::optional<AbstractControlDirection> counterDirectionCandidate;
    std::uint64_t counterDirectionObservedSinceMonotonicMillis{0U};
    std::optional<std::uint64_t> lastAcceptedRequestSequence;
    bool outerFanActive{false};
    std::optional<std::uint64_t> outerFanPostRunUntilMonotonicMillis;
    bool innerFanActive{false};
    std::optional<std::uint64_t> innerFanPostRunUntilMonotonicMillis;
};
```

Analog zu `TemperatureControlRuntimeState` bleibt dieser Zustand
ausschließlich RAM-only, wird nie persistiert und nach einem Neustart
verworfen. `lastAcceptedRequestSequence` wird – wie bei #22s
`requestIdentityExhausted_`/Sequenzhochwasserzeichen – auch bei
`resetRuntime()` bewusst nicht auf `0` zurückgesetzt, damit ein RAM-Reset
keine Sequenzwiederverwendung ermöglicht (dedupe bleibt über einen Neustart
hinweg konservativ, da nach einem echten Neustart ohnehin `sequence` bei #22
wieder bei `initialRequestSequence` beginnt und damit älter als jeder
`lastAcceptedRequestSequence`-Wert vor dem Neustart ist – die Prüfung ist
strikt „kleiner-gleich zuletzt akzeptiert“ und schützt so vor Replay
innerhalb derselben Boot-Session, nicht als Ersatz für #22s eigenen
Sequenzschutz).

### 6.6 `ActuatorSafetyOverride`

Siehe Abschnitt 4.2. Der Typ liegt in `actuator_plan_types.hpp`, ist ein
reiner Werttyp ohne virtuelle Schnittstelle (KISS: kein Interface für einen
einzelnen Boolean ohne zweite Implementierung; #24 erweitert diesen Typ bei
Bedarf um weitere Felder, ersetzt ihn aber nicht durch einen Parallelvertrag).

### 6.7 `ActuatorPlanner` (reiner Kern)

```cpp
class ActuatorPlanner {
   public:
    explicit ActuatorPlanner(ActuatorPlannerParameters parameters);

    [[nodiscard]] ActuatorPlanResult evaluate(const ActuatorPlanInput& input);

    void resetRuntime();  // Fail-closed RAM-Grenze, siehe Abschnitt 6.8

    [[nodiscard]] const ActuatorPlannerRuntimeState& state() const {
        return state_;
    }
    [[nodiscard]] const ActuatorPlannerParameters& parameters() const {
        return parameters_;
    }

   private:
    ActuatorPlannerParameters parameters_;
    ActuatorPlannerRuntimeState state_;
};
```

`evaluate()` ist deterministisch und seiteneffektfrei bezüglich Hardware; sie
mutiert ausschließlich `state_`. Kein GPIO-, Sink- oder Zeitzugriff im Kern
selbst – `nowMonotonicMillis` wird als Wert übergeben (gleiches Muster wie
`TemperatureControlInput::sampleTimestampMonotonicMillis`).

### 6.8 Lifecycle-Reset

`resetRuntime()` wird von der Application-Grenze exakt an denselben
Zeitpunkten aufgerufen wie `resetTemperatureControlAtBoundary()`, unter
Wiederverwendung von `TemperatureControlLifecycleBoundary` (kein zweiter
Lifecycle-Enum):

```cpp
void resetActuatorPlanAtBoundary(ActuatorPlanner& planner,
                                  TemperatureControlLifecycleBoundary boundary);
```

Boundary-Wirkung: bei `NewActiveRun`, `LeaveTemperatureControl`, `Recovery`,
`Fault`, `SafeBoot`, `Service` und `Standby` werden Akkumulator,
Gegenrichtungs-Bestätigungstimer und Fensterstart geleert; die aktive
Peltier-Richtung wird dabei nicht stillschweigend als „schon aus“ angenommen,
sondern der nächste `evaluate()`-Aufruf mit `controlRequest = std::nullopt`
beziehungsweise `Off` liefert regulär `PeltierDirection::Off` und
`outerFanEnabled` gemäß Nachlaufregel – kein direkter Zustandssprung ohne
Nachlaufberechnung.

### 6.9 `ActuatorPlanApplicationOrchestrator`

```cpp
class ActuatorPlanApplicationOrchestrator {
   public:
    ActuatorPlanApplicationOrchestrator(
        ActuatorPlanner& planner,
        device_platform::IBidirectionalActuatorSink& peltier,
        device_platform::IBinaryOutputSink& outerFan,
        device_platform::IBinaryOutputSink& innerFan);

    // Liest TemperatureControlResult (#22-Ausgabe), aktuellen kanonischen
    // Kontext, Prozessphase und ActuatorSafetyOverride; ruft
    // ActuatorPlanner::evaluate() auf; wendet das Ergebnis auf die
    // Sinks an; liefert das Feedback für die nächste #22-Evaluation zurück.
    [[nodiscard]] std::optional<PreviousControlRequestFeedback> planAndApply(
        const TemperatureControlResult& controlResult,
        const ControlRequestContext& currentCanonicalContext,
        bool temperatureControlledPhase,
        const ActuatorSafetyOverride& safetyOverride,
        std::uint64_t nowMonotonicMillis);

   private:
    ActuatorPlanner& planner_;
    device_platform::IBidirectionalActuatorSink& peltier_;
    device_platform::IBinaryOutputSink& outerFan_;
    device_platform::IBinaryOutputSink& innerFan_;
};
```

Die Übersetzung `PeltierDirection -> setForward()/setReverse()` erzwingt
programmtechnisch Exklusivität (niemals beide `true`); das entspricht der in
`bidirectional_actuator_sink.hpp` dokumentierten Erwartung, dass der Port
selbst keine Exklusivität erzwingt, sondern der Aktorplaner sie herstellt.

`planAndApply()` liegt in `fermentation_app`, da sie `TemperatureControlResult`
und `ControlRequestContext` kennt; die Sinks bleiben `device_platform`-Typen,
die per Referenz injiziert werden (keine Rückwärtsabhängigkeit).

## 7. Verhalten im Detail

### 7.1 Fenster und Quote-Umsetzung

Für die aktuell zulässige Richtung wird pro Schaltfenster
(`switchingWindowMillis`, beginnend bei `windowStartMonotonicMillis`) aus der
`ControlRequest.timeQuote` eine geforderte Einschaltzeit berechnet:

```text
requestedOnMillis = clamp(timeQuote * switchingWindowMillis, 0, switchingWindowMillis)
```

- `requestedOnMillis == 0` (Quote `0`): Richtung bleibt `Off` für dieses
  Fenster, kein Akkumulatorzuwachs.
- `0 < requestedOnMillis < minimumOnMillis` (kleine Quote):
  `requestedOnMillis` wird zum `pulseAccumulatorMillis` der aktuell
  zulässigen Richtung addiert (Reason `AccumulatingBelowThreshold`); erreicht
  der Akkumulator `>= minimumOnMillis`, wird ein Mindestimpuls geplant
  (Reason `MinimumPulseTriggered`) und `minimumOnMillis` vom Akkumulator
  abgezogen; der Rest bleibt bis `pulseAccumulatorCapMillis` erhalten (harte
  Obergrenze, kein unbegrenztes Wachstum).
- `requestedOnMillis >= minimumOnMillis` (normale/volle Quote): Richtung wird
  für `requestedOnMillis` direkt geplant (Reason `ScheduledWithinWindow`); der
  Akkumulator der aktuellen Richtung wird dabei nicht zusätzlich befüllt.

Rundungsregeln und die genaue Zuordnung des Mindestimpulses innerhalb des
Fensters bleiben `TBD_COMMISSIONING` (siehe `ACTUATOR_TIMING.md`,
„Noch offen“); die Struktur selbst – getrennter Akkumulator je Richtung, feste
Obergrenze, kein Nachholen entgegen einer Luftbegrenzung – ist bereits
verbindlich und wird durch Abschnitt 8 (Tests) mit konkreten Testparametern
(nicht Produktionswerten) nachgewiesen.

### 7.2 Mindest-Einschaltzeit

Ist `activeDirection != Off`, bleibt sie mindestens bis
`directionActivatedAtMonotonicMillis + minimumOnMillis` aktiv – außer:

1. `safetyOverride.immediateShutdownRequired == true` (Reason
   `ExternalSafetyOverride`, höchste Priorität, wird vor allen anderen
   Prüfungen ausgewertet);
2. `controlStatus` verlangt aktuell keine gültige Freigabe mehr
   (`Unavailable`/`InvalidInput`, Reason `RequiredSensorUnavailable`);
3. eine unzulässige H-Brücken-Kombination erkannt würde (defensive Prüfung,
   in der Praxis durch die Exklusivitätsgarantie aus 6.9 ausgeschlossen –
   dennoch als Fail-closed-Pfad vorgesehen);
4. keine aktuelle `ControlRequest` mehr vorliegt (Watchdog/Status, Reason
   `NoValidRequest`/`StaleRequestWatchdog`);
5. expliziter Stop (aus Prozess-/Lifecycle-Boundary abgeleitet, siehe 6.8).

Die Mindest-Einschaltzeit ist ausdrücklich keine Erlaubnis, trotz eines der
obigen Fälle weiterzuheizen oder zu kühlen (deckungsgleich mit
`ACTUATOR_TIMING.md`).

### 7.3 Mindest-Auszeit und Totzeit

Nach Deaktivierung bleibt `Off` mindestens bis
`directionDeactivatedAtMonotonicMillis + minimumOffMillis`. Bei einem
Richtungswechsel gilt zusätzlich die Totzeit
`directionDeactivatedAtMonotonicMillis + polarityDeadTimeMillis`. Beide Fristen
werden unabhängig berechnet; maßgeblich ist das jeweils spätere Ende
(`max(minimumOffDeadline, deadTimeDeadline)`), sie werden nicht addiert – exakt
gemäß `ACTUATOR_TIMING.md`, Abschnitt „Mindest-Auszeit“.

### 7.4 Bestätigter Richtungswechsel

Eine Gegenanforderung (`AbstractControlDirection` entgegengesetzt zur
`activeDirection`) startet einen Bestätigungstimer
(`counterDirectionCandidate`, `counterDirectionObservedSinceMonotonicMillis`),
sobald `timeQuote >= counterDirectionConfirmationQuoteThreshold` und
`controlStatus == Demand` (gültiger, plausibler Sensorwert). Der Timer läuft
nur weiter, solange die Gegenanforderung ununterbrochen bestehen bleibt; jede
Unterbrechung (Quote unter Schwelle, Richtung wechselt zurück, `Unavailable`,
`InvalidInput`) setzt den Kandidaten zurück (Reason
`CounterDirectionAbandoned`) ohne die alte Richtung zu beenden. Erst nach
`counterDirectionConfirmationDurationMillis` ununterbrochener Bestätigung
beginnt die in Abschnitt 7.3 beschriebene Abschalt-/Totzeit-Sequenz (Reason
`DirectionChangeApplied` nach Ablauf beider Fristen). Eine Restanforderung der
alten Richtung im Akkumulator wird beim bestätigten Wechsel verworfen
(`pulseAccumulatorMillis = 0` für die alte Richtung).

### 7.5 Watchdog und Kontextfrische

Vor jeder Planungsentscheidung wird geprüft (Reihenfolge wie in
Issue-22-Plan Abschnitt 7.1 beschrieben):

1. `controlRequest.has_value()` und `controlStatus` trägt eine Request
   (`Demand`/`Off`) – sonst `NoValidRequest`.
2. `input.currentCanonicalContext == controlRequest->context` – sonst
   `StaleRequestContext` (Prozessübergang, `TargetChanged`, `ProductInserted`,
   Laufübergang oder #21-Rollenwechsel seit Erzeugung der Request).
3. `nowMonotonicMillis - controlRequest->identity.createdAtMonotonicMillis <= requestWatchdogMillis`
   – sonst `StaleRequestWatchdog`. Da `ITimeSource` vertraglich monoton ist
   und niemals zurückspringt, wird eine berechnete negative Differenz
   (`now < createdAt`) defensiv als `StaleRequestWatchdog`/fail-closed
   behandelt statt unterzulaufen (uint64-Unterlaufschutz).
4. `controlRequest->identity.sequence <= lastAcceptedRequestSequence` (falls
   gesetzt) – sonst `DuplicateOrOldSequence`, keine erneute Verarbeitung
   derselben oder einer älteren Sequenz.

Scheitert eine dieser Prüfungen: `PeltierDirection::Off`,
`feedbackForPreviousRequest = std::nullopt` (kein Feedback für eine Request,
die #23 nie akzeptiert hat), Akkumulator und Bestätigungstimer werden
verworfen, Außenlüfter-Nachlauf wird gemäß 7.6 gestartet.

### 7.6 Außenlüfter

`outerFanEnabled = true`, sobald `peltierDirection != Off`. Bei Übergang auf
`Off` startet `outerFanPostRunUntilMonotonicMillis = now + outerFanPostRunMillis`;
`outerFanEnabled` bleibt bis zu diesem Zeitpunkt `true`. Eine erneute
Freigabe während des Nachlaufs setzt den Nachlauf-Timer zurück auf
„aktiv, kein Timer“, ohne dass der Lüfter zwischenzeitlich `false` war (keine
Unterbrechung). Es gibt keinen Vorlauf: `outerFanEnabled` wird im selben
`evaluate()`-Aufruf gesetzt, in dem auch die Peltierfreigabe erfolgt.

### 7.7 Innenlüfter

`innerFanEnabled = true`, solange `input.temperatureControlledPhase == true`
(Quelle: `isTemperatureControlledProcessState(phase)`, keine
Parallelklassifikation) – unabhängig davon, ob das Peltier innerhalb eines
Fensters oder wegen Totzeit gerade `Off` ist. Beim Verlassen der
temperaturgeregelten Phase startet ein eigener Nachlauf
(`innerFanPostRunMillis`), unabhängig vom Außenlüfter-Nachlauf. Kurze
Peltier-Auszeiten *innerhalb* einer weiterhin temperaturgeregelten Phase lösen
keinen Innenlüfter-Nachlauf aus (Dauerbetrieb bleibt ununterbrochen).

## 8. Adopt-or-build

Geprüfte Bausteine gemäß Espressif-first-Reihenfolge
(`ENGINEERING_PRINCIPLES.md`):

| Baustein | Kandidat geprüft | Entscheidung | Begründung |
|---|---|---|---|
| Zeitproportionaler Schaltfenster-/Mindestzeit-/Totzeit-Algorithmus | ESP-IDF Built-ins (keine passende Komponente), `espressif/*` (keine), Arduino PID/QuickPID (`docs/THIRD_PARTY_COMPONENTS.md`, Zeile „Regelung“, `NOT_SELECTED`) | **build** | Fachlich spezifische, sicherheitsrelevante Zeit-/Zustandslogik mit engem #22/#24-Vertrag; keine Bibliothek deckt Mindestzeiten, Totzeit, Impulsakkumulator und Kontext-Watchdog in dieser Kombination ab; Delegation würde den kanonischen #22/#23/#24-Vertrag verwischen (AGENTS.md, explizit ausgeschlossen) |
| Bidirektionaler Aktor-Port (Peltier/H-Brücke) | bereits vorhandener `device_platform::IBidirectionalActuatorSink` | **adopt (bereits vorhanden)** | Port existiert bereits inklusive Mock, bisher ungenutzt; keine neue Abstraktion nötig |
| Binärer Ausgangs-Port (Lüfter) | bereits vorhandener `device_platform::IBinaryOutputSink` | **adopt (bereits vorhanden)** | Zwei Instanzen (außen/innen) über Composition Root zugeordnet, kein neuer Port nötig |
| Zeitquelle | bereits vorhandener `device_platform::ITimeSource` / `VirtualTimeSource` | **adopt (bereits vorhanden)** | Deckt monotone Zeit und native Determinismus bereits vollständig ab |

Es wird keine neue Drittabhängigkeit vorgeschlagen; `docs/THIRD_PARTY_COMPONENTS.md`
wird durch diesen Plan nicht geändert.

## 9. SOLID, DRY, KISS

- **Single Responsibility:** `ActuatorPlanner` entscheidet ausschließlich
  Timing/Aktorplanung; PI-Mathematik bleibt vollständig in #22, Safety-Ursache
  vollständig in #24 (Abschnitt 4.1/4.2).
- **Open/Closed:** Der Planer erweitert die bestehende Kette
  (`ControlRequest -> ActuatorPlanResult`) über einen neuen Baustein, ohne
  `temperature_control.*` zu verändern.
- **Liskov/Interface Segregation:** Es werden ausschließlich bereits
  bestehende schmale Ports verwendet (`IBidirectionalActuatorSink`,
  `IBinaryOutputSink`, `ITimeSource`); kein neues, breiteres Interface.
- **Dependency Inversion:** `ActuatorPlanner` hängt von keiner Hardware ab;
  `ActuatorPlanApplicationOrchestrator` hängt nur von den abstrakten
  `device_platform`-Ports ab, nicht von ESP-IDF oder GPIO.
- **DRY:** Eine einzige Quelle für Request-Identität/-Kontext (#22s
  `ControlRequestIdentity`/`ControlRequestContext`, unverändert
  wiederverwendet), eine einzige Quelle für Lifecycle-Grenzen
  (`TemperatureControlLifecycleBoundary`, wiederverwendet statt einer zweiten
  Aufzählung), eine einzige Quelle für Phasenklassifikation
  (`isTemperatureControlledProcessState`).
- **KISS:** `ActuatorSafetyOverride` bleibt ein einfacher Werttyp statt eines
  vorzeitigen Interfaces ohne zweite Implementierung; keine Kaskaden- oder
  Mehrfach-Timer-Architektur, wo ein einzelner Fenster-/Akkumulatorzustand je
  Richtung ausreicht; keine vorsorgliche Generalisierung für andere
  Aktortypen oder zukünftige Geräte ohne aktuellen Bedarf.

## 10. Safety-, Security-, Recovery- und Hardwaregrenzen

- Fail-closed bei jedem der in Abschnitt 7.5 genannten Fälle: `Off`, kein
  Feedback, kein Akkumulator-Nachholen.
- Keine Aktorfreigabe wird bei Boot, Reset, Fehler, unbekanntem Zustand oder
  unbestätigter Hardware vorausgesetzt (AGENTS.md); `ActuatorPlannerRuntimeState`
  startet mit `PeltierDirection::Off` und keiner aktiven Freigabe.
- `ActuatorSafetyOverride` überstimmt die Mindest-Einschaltzeit ausschließlich
  in Richtung „sicherer machen“ (sofortiges `Off`), niemals in Richtung
  „Freigabe erzwingen“.
- Keine Persistenz, keine Recovery-Sonderpfade: Der Planer wird nach jedem
  Lifecycle-Boundary-Reset regulär aus `Off` neu bewertet; es wird kein
  Fortschritt aus der Zeit vor einem Neustart rekonstruiert (deckungsgleich
  mit `STATE_MACHINE.md`, `RECOVERY_EVALUATION`).
- Kein Türkontakt, keine Kühlkörper-Grenzwertlogik, keine absolute
  Temperatursicherheit – bleibt #24/`SAFETY_AND_FAULTS.md`.
- Keine Hardwarewerte (GPIOs, Pegel, BTS7960-Sequenz) werden in diesem Plan
  festgelegt; `OPEN_POINTS.md` (#29, #32, #33, #35) bleibt unverändert
  sichtbar offen.

## 11. Umsetzungs- und Commit-Schnitte

1. **Gemeinsame schmale Verträge / Request-Konsum** –
   `actuator_plan_types.hpp` (Abschnitt 6.1, 6.2, 6.3, 6.4, 6.6),
   `classifyActuatorPlannerParameters()`; keine Verhaltenslogik.
2. **Fenster, Akkumulator und Watchdog** – `ActuatorPlanner::evaluate()`
   Grundgerüst: Fensterlogik (7.1), Watchdog/Kontextfrische (7.5), RAM-State
   (6.5), `resetRuntime()`.
3. **Mindestzeiten / Gegenrichtung / Totzeit** – Mindest-Einschaltzeit (7.2),
   Mindest-Auszeit/Totzeit (7.3), bestätigter Richtungswechsel (7.4),
   `ActuatorSafetyOverride`-Vorrang.
4. **Lüfterlogik** – Außenlüfter (7.6) und Innenlüfter (7.7) als eigener,
   von der PI-/Timing-Mathematik getrennter Codeabschnitt gemäß Abschnitt 9.
5. **Application-/Device-Platform-Integration** –
   `ActuatorPlanApplicationOrchestrator` (6.9), `resetActuatorPlanAtBoundary()`
   (6.8), Übersetzung auf `IBidirectionalActuatorSink`/`IBinaryOutputSink`;
   keine Composition-Root-Verdrahtung (außerhalb des Plan-Scopes, siehe
   Abschnitt 2).
6. **Tests und Dokumentation** – vollständige native Testklassen gemäß
   Abschnitt 12; `docs/ACTUATOR_TIMING.md` „Akzeptierte Entscheidungen“ um
   Strukturhinweise ergänzen, ohne `TBD_COMMISSIONING`-Werte zu erfinden.
7. **Abschlussnachweise** – gezielte und (nach Owner-Freigabe) vollständige
   lokale Läufe, PR-Nachweis, `SESSION HANDOVER`.

Diese Reihenfolge folgt der im Auftrag vorgeschlagenen Struktur, da jede
Stufe auf der vorherigen aufbaut (Typen -> Kernlogik -> Zeitregeln ->
Lüfter -> Integration -> Tests -> Nachweis) und keine bessere
Abhängigkeitsreihenfolge erkennbar ist.

## 12. Teststrategie

Alle Tests sind native, deterministische Orakel unter `pio test -e native`
und verwenden ausschließlich `VirtualTimeSource`,
`MockBidirectionalActuatorSink` und `MockBinaryOutputSink` aus
`device_platform_test_support` (keine neuen Mocks nötig). Testparameter sind
frei gewählte, plausible Testwerte – keine Produktionskommissionierung.

`test/test_actuator_planner/test_actuator_planner.cpp` deckt mindestens:

- Quote `0`, kleine Quote (unterhalb Mindest-Einschaltzeit), normale Quote,
  volle Quote (Abschnitt 7.1);
- Akkumulator unterhalb, exakt auf und oberhalb der Auslöseschwelle;
- Begrenzung des angesammelten Guthabens (`pulseAccumulatorCapMillis`);
- Mindest-Einschaltzeit hält aktive Richtung trotz neuer OFF-Anforderung;
- Mindest-Auszeit verhindert verfrühte erneute Freigabe;
- kurze Gegenanforderung ohne Bestätigung ändert Richtung nicht;
- dauerhaft bestätigte Gegenanforderung löst Richtungswechsel nach Ablauf
  beider Fristen aus;
- Abschalten vor Richtungswechsel (kein direkter Sprung zwischen Heating und
  Cooling);
- Polaritätstotzeit: späteres Ende von Mindest-Auszeit/Totzeit greift, keine
  Addition;
- Heizen -> Kühlen und Kühlen -> Heizen symmetrisch geprüft;
- niemals gleichzeitig `Heating` und `Cooling` (Exklusivitätsinvariante über
  viele randomisierte/parametrisierte Sequenzen, nicht nur ein Einzelfall);
- `ActuatorSafetyOverride.immediateShutdownRequired` überstimmt eine aktive
  Mindest-Einschaltzeit;
- veraltete Request (Watchdog) und kontextfremde Request (`StaleRequestContext`)
  führen beide zu `Off` und `feedbackForPreviousRequest == std::nullopt`;
- fremde, alte oder doppelte Sequenz wird fail-closed nicht erneut verarbeitet;
- ungültiger `controlStatus` (`Unavailable`/`InvalidInput`) führt zu `Off`;
- monotone Zeit: `now < createdAt` (defensiver Unterlaufschutz) und
  `now == createdAt + requestWatchdogMillis` als Grenzfall exakt geprüft;
- Außenlüfter startet mit Peltierfreigabe im selben Aufruf, läuft während
  Nachlauf weiter, neue Freigabe während Nachlauf unterbricht ihn nicht;
- Innenlüfter läuft während `temperatureControlledPhase == true` dauerhaft
  trotz kurzer Peltier-Auszeit, startet eigenen Nachlauf erst beim Verlassen
  der Phase;
- keine direkte GPIO-/Sink-Ausgabe aus `ActuatorPlanner` selbst (Testaufbau
  verwendet den reinen Kern ohne jede Sink-Injektion – Architekturnachweis
  durch Kompilierbarkeit ohne `device_platform`-Sink-Abhängigkeit im Kern).

`test/test_actuator_plan_orchestrator/test_actuator_plan_orchestrator.cpp`
deckt zusätzlich:

- korrekte Übersetzung `PeltierDirection -> setForward()/setReverse()` ohne
  gleichzeitige `true`/`true`-Kombination (geprüft über die Mock-Introspektion);
- korrekte Weitergabe von `outerFanEnabled`/`innerFanEnabled` an die
  jeweilige `IBinaryOutputSink`-Instanz;
- `resetActuatorPlanAtBoundary()` für jede `TemperatureControlLifecycleBoundary`
  leert Akkumulator und Bestätigungstimer (RAM-Grenze, keine Recovery-Erfindung);
- Rückgabe des `PreviousControlRequestFeedback` an den Aufrufer für die
  nächste #22-Evaluation, inklusive `std::nullopt`-Fall ohne vorherige aktive
  Request.

Bei geänderten gemeinsamen Verträgen (`ControlRequestContext`,
`TemperatureControlLifecycleBoundary`) werden zusätzlich die direkt
betroffenen #22-Konsumententests (`test_temperature_control`,
`test_control_context`) gezielt mitgeführt, sofern die Umsetzung dort
tatsächlich etwas berührt – in diesem Plan ist das nicht vorgesehen, da #23
diese Dateien nur liest, nicht ändert.

## 13. Dokumentations- und Roadmapwirkung

- `docs/ACTUATOR_TIMING.md`: nach Umsetzung Ergänzung der bereits
  akzeptierten Struktur (Fenster-/Akkumulator-/Watchdog-Mechanik) im
  Abschnitt „Akzeptierte Entscheidungen“, ohne die dort weiterhin offenen
  `TBD_COMMISSIONING`-Werte zu verändern.
- `docs/ROADMAP.md`: in dieser Planrevision bereits aktualisiert (Abschnitt 1
  dieses Dokuments verweist auf den PR; die tatsächliche Datei wird im
  selben Commit wie dieser Plan geändert, siehe Abschnitt 14).
- Kein neues ADR erwartet: Die Modulzuordnung folgt unverändert ADR-013, die
  Zustandsautomat-Trennung unverändert ADR-014; `ActuatorSafetyOverride` ist
  ein interner Werttyp ohne Architekturentscheidung von ADR-Rang.
- `docs/THIRD_PARTY_COMPONENTS.md` bleibt unverändert (keine neue
  Abhängigkeit, siehe Abschnitt 8).

## 14. Roadmap-Aktualisierung (Vorschau)

Diese Planrevision aktualisiert `docs/ROADMAP.md` wie folgt (im selben Commit
wie dieser Plan):

- PR #104 / Issue #22 wird als abgeschlossen (gemergt, Issue geschlossen)
  nachgeführt, nicht mehr als „aktuelle Arbeit“ geführt;
- dieser Draft-PR / Issue #23 wird als aktuelle Arbeit mit Planpfad und
  Plan-SHA eingetragen (SHA wird nach dem Commit ergänzt);
- die fachliche Reihenfolge `#23 -> #24 -> #19` bleibt erhalten;
- das offene reale ESP32-Ressourcengate bleibt über #29 / `OPEN_POINTS.md`
  sichtbar;
- keine Detailanforderungen aus Issue #23 oder diesem Plan werden in die
  Roadmap kopiert.

## 15. Offene Fragen und materielle Risiken

- **Genaue Reihenfolge des Mindestimpulses innerhalb eines Fensters**
  (sofort bei Erreichen der Schwelle vs. am nächsten Fensteranfang) ist
  strukturell in Abschnitt 7.1 festgelegt, die exakte Platzierung wird bei
  der Umsetzung anhand der Testfälle konkretisiert, ohne die Struktur zu
  verändern – bewertet als geringes Risiko, da die Akzeptanzkriterien nur
  „begrenzt sammeln“ und „nicht unbegrenzt wachsen“ verlangen, nicht eine
  exakte Platzierung.
- **`ActuatorPlanApplicationOrchestrator` vs. Erweiterung von
  `TemperatureControlApplicationOrchestrator`**: Dieser Plan entscheidet sich
  für eine getrennte Klasse (Single Responsibility, Abschnitt 9). Sollte der
  Owner eine engere Kopplung an die bestehende Application-Grenze bevorzugen,
  ist das eine materielle Abweichung der Modulgrenzen und würde eine neue
  Planrevision mit erneuter Freigabe erfordern.
- **Zeitpunkt der Composition-Root-Verdrahtung** (`src/main.cpp`,
  `main/app_main.cpp`) ist bewusst außerhalb dieses Plans, da er GPIO-/
  Hardwarezuordnung voraussetzt, die laut `OPEN_POINTS.md` weiterhin absteht;
  die reine Anwendungsschicht (Abschnitt 6.9) ist jedoch bereits jetzt ohne
  reale Hardware vollständig nativ testbar und beeinflusst diese
  Verzögerung nicht.
- **Verhältnis zu #24**: `ActuatorSafetyOverride` ist ein bewusst minimaler
  Platzhalter-Vertrag. Sollte #24 einen breiteren Vertrag benötigen (z. B.
  mehrere Abstufungen statt eines einzelnen Booleans), ist das eine
  #24-Planungsentscheidung, keine #23-Nacharbeit; #23 muss dafür lediglich
  weiterhin einen schmalen, werttypbasierten Eingang akzeptieren.
