# Plan: Issue #23 – Aktorplaner, Mindestzeiten, Totzeit und Lüfterlogik

## 1. Status, Scope und Owner-Gate

- Revision: **2**. Ersetzt Revision 1 (`212313c5c18384bcacd3b84f49523fb8ee27bff2`)
  vollständig. Diese Revision ist ohne Rückgriff auf Revision 1 vollständig
  ausführbar und reviewbar.
- Live-Issue: #23, offen, Status `PLANNED_SPEC_PENDING`.
- Draft-PR: #105, Branch `agent/issue-23-aktorplaner-plan` -> `main`.
- Planpfad: `docs/tasks/issue-23-actuator-planner-plan.md`.
- Planbasis: `main` @ `2986dca5736a34171910c9245a3d5f43fa55da06`
  (Merge-Commit von PR #104 / Issue #22, unverändert seit Revision 1).
- Die Umsetzung bleibt gesperrt, bis der Owner exakt diesen neuen
  Revision-2-Plan-Commit mit `PLAN APPROVED: <SHA>` freigibt.
- Diese Revision committet ausschließlich Plandokumentation. Sie implementiert
  keine Produktionslogik, keine produktiven Tests, keine Hardware-, GPIO-,
  Toolchain- oder CI-Änderung.
- Der PR bleibt Draft. Es gibt kein `Ready for review`, keinen Merge, kein
  Auto-Merge und kein Branch-Löschen. Issue #23 wird nicht geschlossen.

```text
CONTEXT_BASELINE_BRANCH: agent/issue-23-aktorplaner-plan
CONTEXT_BASELINE_SHA: 2986dca5736a34171910c9245a3d5f43fa55da06
CONTEXT_HEAD_BEFORE_REVISION: 212313c5c18384bcacd3b84f49523fb8ee27bff2
CONTEXT_PLAN_SHA: NONE (wird nach dem Commit dieser Revision eingetragen)
CONTEXT_REFRESH_MODE: FULL
CONTEXT_DELTA: Vollständiges Owner-Planreview von Revision 1 mit 14 Blocker-
  Befunden (R1–R14) erhalten und in dieser Revision vollständig gelöst:
  R1 eindeutiges Aufruf-/Zeitmodell (Abschnitt 6); R2 exhaustive #22-Feedback-
  Dispositionsmatrix (Abschnitt 9); R3 kanonische AirLimit-Klassifikation und
  Akkumulator-Invalidierung (Abschnitt 7.3/8.4); R4 dreiwertiges Safety-Gate
  (Abschnitt 7.5); R5 latched Watchdog-Fault-Evidenz ohne stille
  Wiederfreigabe (Abschnitt 8.6); R6 Lifecycle-Reset direkt in der
  bestehenden `TemperatureControlApplicationOrchestrator`-Grenze verankert
  (Abschnitt 11); R7 einziges Richtungs-Enum (`AbstractControlDirection`) und
  einziger richtungsgebundener Akkumulator (Abschnitt 7.4/8.4); R8
  deterministische Fenster-/Impulsplatzierungs-/Rundungssemantik
  (Abschnitt 8); R9 vollständige strukturelle Parameterinvarianten und
  Status-/Reason-Matrix ohne erfundene Firmwaregrenzen (Abschnitt 7.6/8.2);
  R10 unveränderlicher Per-Run-Parametervertrag mit benanntem
  Integrationsgate (Abschnitt 13); R11 verbindliche Sink-Ausgabereihenfolge
  (Abschnitt 12); R12 overflow-sichere elapsed-time-Arithmetik und
  Prioritätsleiter (Abschnitt 8.2); R13 präzisiertes Adopt-or-build
  (Abschnitt 14); R14 Roadmap-/Plan-SHA-Governance (Abschnitt 20). Kein
  Produktionscode wurde in dieser Session verändert.
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
  mit latched Fault-Evidenz für #24 (kein stilles Verschwinden);
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
  Evidenz (Abschnitt 8.6), keine Ursache und keine Wiederfreigabeentscheidung.
- Keine Änderung an der PI-/Luftbegrenzungslogik aus Issue #22
  (`temperature_control.*`, `control_context.*`); `ControlRequest`,
  `ControlRequestContext`, `ControlSensorRole`, `AbstractControlDirection`,
  `TemperatureControlResult` und `PreviousControlRequestFeedback` werden
  ausschließlich wiederverwendet.
- Keine konkreten Sekundenwerte für Schaltfenster, Mindestzeiten, Totzeit,
  Umschaltschwelle, Akkumulatorgrenze oder Nachlaufzeiten; diese bleiben
  `TBD_COMMISSIONING` (Nachverfolgung #35 / `OPEN_POINTS.md`). Der Plan legt
  die vollständige Algorithmus- und Prioritätssemantik fest (siehe R8/R9/R12
  in Abschnitt 1), nicht die Zahlen.
- Keine Persistenz des Aktorplanerzustands; er bleibt RAM-only wie der
  PI-/Qualifier-Zustand aus Issue #22 und wird an denselben kanonischen
  Lifecycle-Grenzen über dieselbe Application-Grenze zurückgesetzt
  (Abschnitt 11). Kein `run_persistence_*`-Schema wird geändert; die
  Per-Run-Parameterbindung bleibt ein benanntes, unimplementiertes
  Integrationsgate (Abschnitt 13).
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
- `docs/TEMPERATURE_CONTROL.md`, insbesondere „Architekturgrenzen“ und der
  implementierte #22-Fachkern-Abschnitt (Status-/Reason-/AirLimit-Matrix);
- `docs/STATE_MACHINE.md` für `ProcessState`-Topologie, Recovery- und
  Lifecycle-Grenzen;
- `docs/SENSOR_TUNING_COMMISSIONING.md` für Integratorregeln und
  Commissioning-Eigentümerschaft;
- `docs/RUN_PERSISTENCE.md` für das Write-before-Apply-Muster (informativ;
  #23 besitzt keinen eigenen Persistenzvertrag, siehe Abschnitt 13);
- `docs/tasks/issue-22-pi-control-air-limits-plan.md`, insbesondere Abschnitt
  4.1 (Zuständigkeiten #22/#23/#24), Abschnitt 7.1 (Request-Identität und
  Kontextfrische), Abschnitt 7.2 (Status-/Reason-/AirLimit-Matrix) und
  Abschnitt 8 (Anti-Windup-Feedback) – dies ist der verbindliche Vertrag, den
  #23 konsumiert;
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
| `ControlRequest`, `ControlRequestIdentity` | `temperature_control_types.hpp` | Quelle für Sequenz/Timestamp/Richtung/Quote der neuen Evaluation |
| `ControlRequestContext`, `ControlSensorRole` | `control_context_types.hpp` | Kontextfrischeprüfung gegen aktuellen kanonischen Kontext |
| `AbstractControlDirection` | `sensor_selection_types.hpp` | **einziges** Richtungs-Enum in #23 (kein zweites, siehe R7/Abschnitt 7.4) |
| `PreviousControlRequestFeedback`, `Disposition` | `temperature_control_types.hpp` | Ausgabe des Planers zurück an #22 (exhaustive Matrix, Abschnitt 9) |
| `TemperatureControlResult`, `TemperatureControlStatus`, `TemperatureControlReason`, `AirLimitState` | `temperature_control_types.hpp` | vollständige Eingabe je Tick mit neuer Evaluation; Quelle der kanonischen AirLimit-Klassifikation (Abschnitt 7.3) |
| `isTemperatureControlledProcessState()` | `control_context.hpp` | Innenlüfter-Phasenklassifikation, keine Parallel-Klassifikation |
| `TemperatureControlLifecycleBoundary` | `temperature_control_orchestrator.hpp` | wiederverwendete Lifecycle-Grenzen (kein zweiter Enum, siehe R6/Abschnitt 11) |
| `TemperatureControlApplicationOrchestrator` | `temperature_control_orchestrator.hpp/.cpp` | wird erweitert (nicht dupliziert), einzige Application-/Lifecycle-Grenze auch für #23 (Abschnitt 11) |
| `TemperatureControlEvaluationEvidence::previousControlRequestFeedback` | `temperature_control_orchestrator.hpp` | bereits vorhandenes Eingabefeld für #22; wird von #23s Feedback exakt befüllt, kein neuer Mechanismus |
| `IBidirectionalActuatorSink` | `device_platform/bidirectional_actuator_sink.hpp` | bereits vorhandener, bisher unbenutzter Peltier-Port |
| `IBinaryOutputSink` | `device_platform/binary_output_sink.hpp` | bereits vorhandener, bisher unbenutzter Lüfter-Port (zwei Instanzen: außen/innen) |
| `ITimeSource`, `VirtualTimeSource` | `device_platform/time_source.hpp`, `virtual_time_source.hpp` | monotone Zeit im Aufrufer; native Tests |
| `MockBidirectionalActuatorSink` (inkl. `commandJournal()`, `simultaneousActivationObserved()`) | `device_platform_test_support/` | bereits vorhandenes Test-Double mit eingebauter Reihenfolge-/Exklusivitätsprüfung – kein neuer Test-Recorder nötig (löst R11) |
| `MockBinaryOutputSink` (inkl. `commandJournal()`) | `device_platform_test_support/` | bereits vorhandenes Test-Double für beide Lüfterinstanzen |

Kein paralleler Sensor-, Prozess-, Persistenz- oder PI-Vertrag wird erfunden.
Insbesondere wird `ControlRequestContext` nicht kopiert, sondern exakt wie in
Issue #22 als flüchtige Identität mitgeführt und geprüft.

## 4. Zuständigkeiten und Architekturgrenzen

### 4.1 Abgrenzung zu Issue #22

Issue #22 liefert ausschließlich `HEAT`/`OFF`/`COOL` mit Zeitquote,
Identität (`sequence`, `createdAtMonotonicMillis`), Kontext
(`processTransitionSequence`, `runRevision`, `controlSensorRole`) und der
vollständigen `TemperatureControlStatus`/`TemperatureControlReason`/
`AirLimitState`-Matrix. Issue #22 kennt weder Schaltfenster noch
Mindestzeiten noch Lüfter und behauptet keine physische Aktorquote. Dieser
Plan ändert an `temperature_control.*` und `control_context.*` nichts; er
konsumiert deren vollständige Ausgabe (nicht nur `ControlRequest` und
`status`, siehe R3) unverändert und liefert im Gegenzug exakt den bereits
vereinbarten `PreviousControlRequestFeedback` über das bereits existierende
Eingabefeld `TemperatureControlEvaluationEvidence::previousControlRequestFeedback`
zurück (kein neuer Feedbackvertrag, kein neuer Übergabemechanismus).

### 4.2 Abgrenzung zu Issue #24

Issue #23 trifft keine Aussage darüber, *warum* eine Sicherheitsabschaltung
verlangt wird oder wann ein Fehler wieder freigegeben werden darf. Dafür
werden zwei schmale, reine Werttypen definiert (keine neuen Interfaces):

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

`ActuatorSafetyGateInput` trägt keine Fehlerklasse und keine Ursache – das
bleibt vollständig Issue #24 (Abschnitt 7.5). `ActuatorWatchdogFaultEvidence`
ist reine, für #24 konsumierbare Evidenz ohne Fehlerklassifikation
(Abschnitt 8.6). Bis #24 real verdrahtet ist, bleibt `ActuatorSafetyGateInput`
im Composition-Root-Default `Unresolved`; dies führt strukturell zu `Off`
(Abschnitt 8.2, Rang 3) und ist **kein** implizites „Safety=erlaubt“. Die
reale Verdrahtung eines `Allowed`-Zustands vor Fertigstellung von #24 ist
ausdrücklich als noch offenes Integrationsgate benannt (Abschnitt 13) und
nicht Teil dieses Plans.

### 4.3 Modulzuordnung nach ADR-013

Der Aktorplaner kennt `ControlRequest`, `TemperatureControlResult`,
`ControlSensorRole` und `ProcessState` – alles fermentationsspezifische
beziehungsweise bereits als `fermentation_app`-Vertrag eingestufte Typen
(Issue #22 liegt vollständig in `lib/fermentation_app/`). Der Planer gehört
deshalb konsequent ebenfalls zu `lib/fermentation_app/`, nicht zu
`lib/device_platform/`. Er ist selbst hardwarefrei und referenziert aus
`device_platform` ausschließlich die bestehenden schmalen Ports
(`IBidirectionalActuatorSink`/`IBinaryOutputSink` für die dünne
Ausgabeschicht, Abschnitt 12). Es entsteht kein neuer `device_platform`-Port
und keine Rückwärtsabhängigkeit; `check_architecture_boundaries.py` bleibt
PASS.

## 5. Betroffene Module und voraussichtliche Dateien

Neue Dateien (alle unter `lib/fermentation_app/src/`):

```text
actuator_plan_types.hpp
    Werttypen: ActuatorDemandClass, ActuatorSafetyGateStatus,
    ActuatorSafetyGateInput, ActuatorWatchdogFaultEvidence,
    ActuatorPlannerParameters, ActuatorPlannerParametersValidation,
    ActuatorPlanStatus, ActuatorPlanReason, ActuatorPlanTickInput,
    ActuatorPlanTickResult, ActuatorPlannerRuntimeState (inkl.
    AcceptedControlCommand); freie Funktion classifyActuatorDemand()

actuator_planner.hpp / .cpp
    Reine, deterministische Klasse ActuatorPlanner: tick(), forceStop(),
    acknowledgeWatchdogFault(), Zugriff auf state()/parameters(). Kapselt
    Fenster, Akkumulator, Mindestzeiten, Totzeit, Richtungswechsel, Watchdog
    und Lüfterlogik. Kein Sink-, kein GPIO-Zugriff.

actuator_plan_sink_driver.hpp / .cpp
    ActuatorPlanSinkDriver: dünne, zustandslose Übersetzung eines
    ActuatorPlanTickResult auf genau drei injizierte device_platform-Sinks
    (Peltier, Außenlüfter, Innenlüfter) in der in Abschnitt 12 festgelegten
    Reihenfolge. Wird sowohl von normalen Ticks als auch vom
    Lifecycle-Stop-Pfad verwendet (keine zweite Ausgabewahrheit, löst R6/R11).
```

Geänderte Dateien (Erweiterung der bestehenden kanonischen Application-Grenze,
keine neue parallele Orchestrierung, löst R6):

```text
lib/fermentation_app/src/temperature_control_orchestrator.hpp / .cpp
    - Konstruktor zusätzlich mit ActuatorPlanner& und ActuatorPlanSinkDriver&
      (analog zu temperatureController_/evaluator_ injiziert).
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
```

Geändertes Testverzeichnis (bereits vorhandener direkter Konsument von
`TemperatureControlApplicationOrchestrator`, siehe Repository-first-Suche):

```text
test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp
    - gezielte Ergänzung um die neuen Lifecycle-/Stop-Fälle der
      Aktorplaner-Integration (Abschnitt 18); keine bestehenden Fälle
      werden entfernt.
```

Betroffen, aber ohne Vertragsänderung durch diesen Plan:

```text
src/main.cpp, main/app_main.cpp
    (Composition-Root-Verdrahtung bleibt außerhalb des Plan-only-Scopes,
    siehe Abschnitt 2; nur so weit erwähnt, wie zur Einordnung nötig)
docs/ACTUATOR_TIMING.md
    (nach Umsetzung Ergänzung der akzeptierten Struktur, keine
    TBD_COMMISSIONING-Werte)
docs/ROADMAP.md
    (in dieser Revision inhaltlich unverändert gegenüber Revision 1; die
    Plan-SHA-Nachführung erfolgt gemäß Abschnitt 20 in einem separaten
    reinen Metadaten-Commit)
```

Kein neuer `device_platform`-Port, keine neue Drittbibliothek, keine
Änderung an `run_persistence_*`, `run_commands.*` oder
`process_state_machine.*`.

## 6. Aufruf-/Zeitmodell (löst R1)

Der Planer besitzt **einen** Tick-Einstiegspunkt mit explizitem, optionalem
Eingang für eine neu eingetroffene #22-Evaluation. Er wird vom Aufrufer
(`TemperatureControlApplicationOrchestrator::tickActuatorPlan()`) potenziell
**häufiger** aufgerufen als #22 seine eigene, sensorgetaktete
`evaluateTemperatureControl()`-Berechnung durchführt:

```cpp
struct ActuatorPlanTickInput {
    std::uint64_t nowMonotonicMillis{0U};
    std::optional<TemperatureControlResult> newEvaluation;
    ControlRequestContext currentCanonicalContext;
    bool temperatureControlledPhase{false};
    ActuatorSafetyGateInput safetyGate;
};

class ActuatorPlanner {
   public:
    explicit ActuatorPlanner(ActuatorPlannerParameters parameters);

    [[nodiscard]] ActuatorPlanTickResult tick(const ActuatorPlanTickInput& input);
    [[nodiscard]] ActuatorPlanTickResult forceStop(std::uint64_t nowMonotonicMillis);
    void acknowledgeWatchdogFault();

    [[nodiscard]] const ActuatorPlannerRuntimeState& state() const;
    [[nodiscard]] const ActuatorPlannerParameters& parameters() const;
};
```

### 6.1 Genau eine Aufrufsemantik

Ein Aufrufer ruft `tick()` bei **jedem** eigenen Regelzyklus auf (feinere
Kadenz als #22, z. B. jede Task-Iteration), unabhängig davon, ob #22 in
diesem Zyklus eine neue Bewertung geliefert hat:

- Läuft #22 seltener: `newEvaluation = std::nullopt` in den dazwischen
  liegenden Ticks; der Planer führt seinen bereits akzeptierten Befehl
  (`state().acceptedCommand`) anhand der aktuellen Zeit unverändert
  zeitlich fort (Fenster-, Mindestzeit-, Totzeit- und Lüfterauswertung
  laufen bei jedem Tick, nicht nur bei einer neuen Anforderung).
- Läuft #22 (theoretisch) öfter oder liefert dieselbe Sequenz erneut: Eine
  Sequenz `<= state().lastAcceptedSequence` wird **nicht** angenommen und
  löst **keine** Abschaltung aus. Der Tick verhält sich exakt so, als wäre
  `newEvaluation = std::nullopt` gewesen (Reason `DuplicateOrOldSequence`
  wird nur als Diagnoseinformation für diesen spezifischen
  Annahmeversuch geführt, ohne die laufende Zeitbasis zu berühren). Dies
  löst den zweiten und dritten in R1 genannten Fehlerfall explizit auf.

### 6.2 Wann eine neue Request genau einmal angenommen wird

Bei vorhandenem `newEvaluation` prüft `tick()` in dieser Reihenfolge, bevor
irgendetwas übernommen wird:

1. **Struktur**: `newEvaluation` muss strukturell gültig sein (Abschnitt 8.2,
   Rang 1). Ungültig -> Kandidat wird nicht angenommen, laufende Zeitbasis
   wird zusätzlich fail-closed verworfen (siehe Abschnitt 8.2, Rang 1 wirkt
   *sofort*, nicht nur auf den Kandidaten).
2. **Klassifikation**: `classifyActuatorDemand(newEvaluation.value())`
   (Abschnitt 7.3) liefert `NoValidRequest`, `NeutralOff`,
   `AirLimitReducedDemand`, `AirLimitBlockedOff` oder `NormalDemand`.
3. **Sequenzprüfung**: Trägt die Evaluation eine `ControlRequest`
   (`NormalDemand`, `AirLimitReducedDemand`, `NeutralOff`,
   `AirLimitBlockedOff` – alle vier tragen laut #22-Vertrag eine gültige
   `ControlRequest`, nur `NoValidRequest` nicht) und ist
   `identity.sequence <= state().lastAcceptedSequence.value_or(0)`: Kandidat
   wird ignoriert (Reason `DuplicateOrOldSequence` für den Annahmeversuch,
   siehe 6.1). `NoValidRequest` selbst hat keine Sequenz und wird gesondert
   in Rang 7 (Abschnitt 8.2) behandelt.
4. **Ankunfts-Watchdog**: Ist `identity.createdAtMonotonicMillis` bereits vor
   der Ankunft älter als `requestWatchdogMillis` relativ zu
   `input.nowMonotonicMillis` (elapsed-time-Vergleich, Abschnitt 8.3): Kandidat
   wird nicht angenommen (`StaleRequestWatchdog` für den Annahmeversuch); die
   laufende Zeitbasis eines bereits akzeptierten Befehls bleibt davon
   *unberührt* – maßgeblich für ein tatsächliches Abschalten ist
   ausschließlich der laufende Watchdog aus 6.3.
5. **Kontextprüfung bei Ankunft**: `newEvaluation`s `ControlRequest.context`
   muss `input.currentCanonicalContext` entsprechen. Nicht passend: Kandidat
   wird nicht angenommen (`StaleRequestContext` für den Annahmeversuch); ein
   bereits akzeptierter, weiterhin kontextfrischer Befehl bleibt unberührt.
6. Bestehen alle Prüfungen: Der neue Befehl wird als
   `AcceptedControlCommand` übernommen (Sequenz, Richtung, Quote,
   Demand-Klasse, Kontext-Snapshot, Quell-Timestamp), `lastAcceptedSequence`
   wird auf die neue Sequenz angehoben und
   `lastNewRequestAcceptedAtMonotonicMillis = input.nowMonotonicMillis`
   gesetzt (**Zeitpunkt der Annahme**, nicht `createdAtMonotonicMillis` –
   entkoppelt den laufenden Watchdog bewusst von der #22-internen Kadenz,
   siehe 6.3).

Diese Reihenfolge stellt sicher, dass eine neue Request **genau einmal**
angenommen wird (Schritt 6 mutiert `lastAcceptedSequence` nur einmal pro
gültiger neuer Sequenz) und dass eine Ablehnung des Kandidaten niemals
rückwirkend einen bereits laufenden, weiterhin gültigen Zeitplan zerstört.

### 6.3 Laufender Watchdog (unabhängig von neuen Requests)

Bei **jedem** Tick, unabhängig von `newEvaluation`, wird geprüft:

```text
elapsed = nowMonotonicMillis - lastNewRequestAcceptedAtMonotonicMillis
(mit Retrograde-Guard gemäß Abschnitt 8.3)
elapsed > requestWatchdogMillis?
```

Ist `lastNewRequestAcceptedAtMonotonicMillis` noch nie gesetzt worden (seit
dem letzten `forceStop()`/Konstruktion), gilt der Watchdog nicht, solange
auch kein `acceptedCommand` gehalten wird (Rang 7, `NoValidRequest`, greift
zuerst). Löst der Watchdog aus, wird `latchedWatchdogFault` gesetzt
(Abschnitt 8.6) und der Befehl sowie sein Guthaben verworfen. Diese Prüfung
verwendet ausschließlich `nowMonotonicMillis` aus dem aktuellen Tick und die
zuletzt beobachtete *Annahmezeit* – sie ist damit unabhängig von der
#22-eigenen Kadenz und von der Planner-Tick-Kadenz.

### 6.4 Welche Quote ein Fenster bestimmt / neue Quote mitten im Fenster

Siehe Abschnitt 8.1. Kurzfassung: Die für ein laufendes Fenster maßgebliche
Quote wird beim **Fensterstart** aus dem zu diesem Zeitpunkt gehaltenen
`acceptedCommand` gelesen und für die Dauer des Fensters nicht erneut
ausgewertet. Eine während des Fensters neu angenommene Request mit
unveränderter Richtung ändert die bereits berechnete Ein-/Aus-Aufteilung des
laufenden Fensters nicht; sie wird beim **nächsten** Fensterstart wirksam.
Ereignisse aus Abschnitt 8.2 Rang 1–6 (strukturell ungültig, fehlende
Kommissionierung, Safety, latched Fault, laufender Watchdog, Kontext-Stale)
wirken dagegen **sofort**, unabhängig vom Fensterstand.

### 6.5 Beenden einer geplanten Peltierfreigabe

Da `tick()` bei jedem Regelzyklus aufgerufen wird, endet ein geplanter Impuls
zuverlässig in dem Tick, dessen `nowMonotonicMillis` die berechnete
Ende-Zeit (`directionActivatedAtMonotonicMillis`-relativer elapsed-Vergleich,
Abschnitt 8.3) erreicht oder überschreitet – vorausgesetzt, die
Aufrufer-Kadenz ist feiner als die kürzeste vorkommende geplante
Ein-/Aus-Dauer (Kadenzanforderung an den Aufrufer, keine #23-interne Annahme
über Echtzeitgarantien; siehe Abschnitt 15).

## 7. Kanonische AirLimit-/Demand-Klassifikation (löst R3, Teil 1)

### 7.1 `TemperatureControlResult` als vollständige Eingabe

`ActuatorPlanTickInput::newEvaluation` trägt den **vollständigen**
`TemperatureControlResult` (Status, Reason, `AirLimitState`, `ControlRequest`),
nicht nur `ControlRequest`/`status` wie in Revision 1. #23 erfindet daraus
keine eigene Luftlimitlogik, sondern bildet die bereits abschließende
#22-Matrix (Issue-22-Plan Abschnitt 7.2) über eine reine, tabellarische
Klassifikation ab.

### 7.2 `ActuatorDemandClass`

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
Neuberechnung. Jede sonstige, laut #22-Matrix unzulässige Kombination
(z. B. `Demand` mit `AirLimitBlocked`) wird defensiv als strukturell
ungültig eingestuft (Abschnitt 8.2, Rang 1).

### 7.3 Wirkung der Klassifikation

| `ActuatorDemandClass` | Effekt auf Richtung/Fenster | Effekt auf Akkumulator |
|---|---|---|
| `NoValidRequest` | keine Annahme; Rang 7 in Abschnitt 8.2 | kein Effekt durch diese Klassifikation selbst |
| `NeutralOff` | angenommene Richtung `Idle`, normale Fensterbehandlung (Quote `0`) | keine Fütterung; unverändert, sofern nicht durch anderen Trigger verworfen |
| `AirLimitBlockedOff` | angenommene Richtung `Idle` | **sofortiger, unbedingter Verwurf** des Akkumulators bei Übergang in diese Klasse (Abschnitt 8.4) |
| `AirLimitReducedDemand` | angenommene Richtung Heating/Cooling, Quote bereits von #22 reduziert | bei **Übergang** in diese Klasse aus einer weniger restriktiven Klasse: Akkumulator wird verworfen, bevor die aktuelle (bereits reduzierte) Fensterquote gutgeschrieben wird; bei **fortlaufendem** Verbleib in dieser Klasse über mehrere Fenster: normale Akkumulation aus der jeweils aktuellen, bereits reduzierten Quote (Abschnitt 8.4) |
| `NormalDemand` | angenommene Richtung Heating/Cooling, normale Fensterbehandlung | normale Akkumulation |

Damit ist explizit entschieden (löst R3 vollständig):

- `AirLimitBlocked` verwirft das betroffene Guthaben immer sofort und kann
  keine alte Energie nachholen.
- `AirLimitReduced` kann kein *vor* der Reduktion gesammeltes höheres
  Guthaben zur Umgehung der aktuellen Reduktion nutzen, weil der
  Übergang selbst das alte Guthaben verwirft; jede weitere Akkumulation
  erfolgt ausschließlich aus der bereits reduzierten Quote.
- Interaktion mit Mindest-Einschaltzeit: `AirLimitBlockedOff` ist **keine**
  Safety-Sofortabschaltung. Eine bereits aktive Richtung wird durch
  `AirLimitBlockedOff` nicht anders behandelt als durch `NeutralOff` – beide
  sind gewöhnliche `Idle`-Anforderungen von #22 und unterliegen der normalen
  Mindest-Einschaltzeit-Haltelogik (Abschnitt 8.2, Rang 9). Der Unterschied
  wirkt ausschließlich auf den Akkumulator (sofortiger Verwurf bei
  `AirLimitBlockedOff`), nicht auf die Haltezeit selbst. `AirLimitBlocked`
  steht damit ausdrücklich **nicht** in der Sofort-Override-Liste aus
  `ACTUATOR_TIMING.md` (Sicherheitsfehler, ungültiger Pflichtsensor,
  unzulässige H-Brücken-Kombination, fehlende aktuelle Anforderung,
  expliziter Stop) – Revision 1 hat OFF-Anforderungen pauschal gleich
  behandelt; Revision 2 unterscheidet nun explizit `NeutralOff` von
  `AirLimitBlockedOff` in der Akkumulatorwirkung, behält aber für beide die
  identische, nicht-safety-artige Mindestzeit-Behandlung bei.

## 8. Verhalten im Detail: Prioritätsleiter, Status-/Reason-Matrix, Fenster

### 8.1 Fenster- und Impulsplatzierung (löst R8)

**Fensterausrichtung**: `windowStartMonotonicMillis` wird lazily beim ersten
Tick initialisiert, der einen `acceptedCommand` mit Richtung `Heating` oder
`Cooling` physisch aktiviert (nicht bei Konstruktion, nicht bei einer reinen
`Idle`-Anforderung – ein leerlaufender Schrank erzeugt kein künstliches
Fenster). Nach Initialisierung ist ein Fenster `[windowStartMonotonicMillis,
windowStartMonotonicMillis + switchingWindowMillis)`, danach folgt
kontinuierlich das nächste, phasentreue Fenster
(`windowStartMonotonicMillis += switchingWindowMillis`, wiederholt bis
`windowStartMonotonicMillis + switchingWindowMillis > nowMonotonicMillis`,
mit einer Sicherheitsobergrenze von `1000` Iterationen pro Tick – wird diese
überschritten (nur nach einer unrealistisch langen Tick-Pause möglich), wird
`windowStartMonotonicMillis` defensiv direkt auf `nowMonotonicMillis`
resynchronisiert und dies als `TimeInvalid`-Diagnose vermerkt, ohne eine
Endlosschleife zu riskieren).

**Maßgebliche Quote pro Fenster**: beim Überschreiten einer Fenstergrenze
(„Fensterstart-Ereignis“) wird `requestedOnMillisExact` aus der Quote des zu
diesem Zeitpunkt gehaltenen `acceptedCommand` berechnet:

```text
requestedOnMillisExact = clamp(timeQuote, 0.0, 1.0) * switchingWindowMillis   (double, ungerundet)
```

- `requestedOnMillisExact >= minimumOnMillis`:
  `scheduledOnMillis = round_half_up(requestedOnMillisExact)` (auf die
  nächste ganze Millisekunde, `.5` aufgerundet), direkt geplant
  (`ScheduledWithinWindow`); der Akkumulator der aktuellen Richtung wird bei
  diesem Fensterstart **nicht** zusätzlich gefüttert.
- `0 < requestedOnMillisExact < minimumOnMillis`:
  `pulseAccumulatorMillis += requestedOnMillisExact` (double, ungerundet,
  gebunden an `accumulatorDirection == aktuelle Richtung`, sonst vorher
  gemäß Abschnitt 8.4 verworfen und neu an die aktuelle Richtung gebunden),
  begrenzt auf `pulseAccumulatorCapMillis`. Erreicht
  `pulseAccumulatorMillis >= minimumOnMillis` durch diesen Zuwachs:
  `scheduledOnMillis = minimumOnMillis` (exakt, ungerundet, da bereits
  ganzzahlig aus Parametern), `pulseAccumulatorMillis -= minimumOnMillis`
  (Rest bleibt erhalten, weiterhin `<= pulseAccumulatorCapMillis`), Reason
  `MinimumPulseTriggered`. Sonst: `scheduledOnMillis = 0`, Reason
  `AccumulatingBelowThreshold`.
- `requestedOnMillisExact == 0`: `scheduledOnMillis = 0`, Reason
  `NeutralIdle` (bei Richtung `Idle`) beziehungsweise faktisch irrelevant
  (Heating/Cooling mit Quote `0` tritt laut #22-Matrix nicht auf).

Die Fensterzuordnung erfolgt **einmalig pro Fensterstart-Ereignis** – nicht
pro Tick und nicht pro angenommener neuer Request innerhalb desselben
Fensters. Das schließt die in R1 benannte Mehrfach-Akkumulation strukturell
aus: Ein Fensterstart-Ereignis wird ausschließlich durch das Überschreiten
der Fenstergrenze ausgelöst, unabhängig davon, wie viele `newEvaluation`s
innerhalb desselben Fensters angenommen wurden.

**Impulsposition und Ende**: `scheduledOnMillis` gilt ab
`windowStartMonotonicMillis` des jeweiligen Fensters. Der physisch
gewünschte Zustand („`desiredActive`“) für einen beliebigen Tick innerhalb
des Fensters ist:

```text
elapsedInWindow = now - windowStartMonotonicMillis   (elapsed, Abschnitt 8.3)
desiredActive = elapsedInWindow < scheduledOnMillis
```

Dieser Wunschzustand wird durch die Mindestzeit-/Totzeit-/Safety-Schicht
(Abschnitt 8.2) gefiltert – ein Wunsch nach „aus“ kann durch eine noch
laufende Mindest-Einschaltzeit gehalten werden; ein Wunsch nach „ein“ kann
durch Mindest-Auszeit/Totzeit/Safety verzögert werden. Ein durch eine
laufende Mindestzeit verlängerter „ein“-Zustand verlängert nicht
`scheduledOnMillis` selbst – beim nächsten Fensterstart wird ganz normal neu
gerechnet.

**Richtungswechsel und Fenster**: Ein bestätigter Richtungswechsel
(Abschnitt 8.5) setzt bei tatsächlicher Enablement der neuen Richtung
`windowStartMonotonicMillis` auf den Enablement-Zeitpunkt zurück
(neue Richtung beginnt mit einem frischen Fenster) und setzt den
Akkumulator gemäß Abschnitt 8.4 zurück.

### 8.2 Prioritätsleiter, Status und Reason (löst R9, R12)

```cpp
enum class ActuatorPlanStatus : std::uint8_t {
    Active,        // Heating oder Cooling wird diesen Tick physisch angesteuert
    Idle,          // Idle wird diesen Tick physisch angesteuert (gueltig oder gehalten)
    Unconfigured,  // Parameter NoCommissioning
    InvalidInput,  // strukturell ungueltige Parameter oder Evaluation oder TimeInvalid
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
    StaleRequestContext,
    NoValidRequest,
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
zutreffende Stufe gewinnt; niedrigere Prüfungen werden nicht mehr
ausgewertet – dies ist die in R12 verlangte Prioritätsleiter):

| Rang | Bedingung | Status | Reason | Physisch | Akkumulator-Effekt |
|---:|---|---|---|---|---|
| 1 | `newEvaluation` strukturell ungültig (unbekannter Enumwert, `sequence == 0` bei vorhandener `ControlRequest`, `timeQuote` nicht-finit oder außerhalb `[0,1]`, Status/`ControlRequest`-Präsenz widerspricht der #22-Matrix, `currentCanonicalContext` strukturell ungültig) | `InvalidInput` | `MalformedEvaluation` | `Idle` sofort | sofortiger, unbedingter Verwurf |
| 2 | `classifyActuatorPlannerParameters(parameters_) == Unconfigured` | `Unconfigured` | `NoCommissioning` | `Idle` | unverändert gehalten (kein Tick-Fortschritt möglich) |
| 2 | `classifyActuatorPlannerParameters(parameters_) == Invalid` | `InvalidInput` | `InvalidConfiguration` | `Idle` | unverändert gehalten |
| 3 | `safetyGate.status == Unresolved` | `Idle` | `SafetyGateUnresolved` | `Idle` sofort, überstimmt Mindest-Einschaltzeit | sofortiger, unbedingter Verwurf |
| 3 | `safetyGate.status == ImmediateStop` | `Idle` | `ExternalSafetyOverride` | `Idle` sofort, überstimmt Mindest-Einschaltzeit | sofortiger, unbedingter Verwurf |
| 4 | `state().latchedWatchdogFault.has_value()` | `Idle` | `RequestWatchdogFaultLatched` | `Idle` | bleibt verworfen (bereits bei Rang 5 verworfen) |
| 5 | laufender Watchdog löst diesen Tick aus (Abschnitt 6.3) | `Idle` | `StaleRequestWatchdog` | `Idle` sofort | sofortiger, unbedingter Verwurf; `latchedWatchdogFault` wird gesetzt |
| 6 | `acceptedCommand` vorhanden, aber `contextAtAcceptance != input.currentCanonicalContext` | `Idle` | `StaleRequestContext` | `Idle` sofort | sofortiger, unbedingter Verwurf |
| 7 | kein `acceptedCommand` vorhanden | `Idle` | `NoValidRequest` | `Idle` | leer |
| 8 | physisch aktive Richtung `!= Idle` **und** Mindest-Einschaltzeit noch nicht erfüllt **und** `acceptedCommand.direction` verlangt aktuell `Idle` oder Gegenrichtung (unbestätigt oder in Bestätigung) | `Active` (alte Richtung bleibt) | `MinimumOnTimeHeld` | bisherige Richtung bleibt an | unverändert (Fensterlogik der alten Richtung läuft normal weiter) |
| 9 | physisch `Idle`, neue Richtung soll starten, aber Mindest-Auszeit **oder** Totzeit noch aktiv (spätere der beiden maßgeblich) | `Idle` | `MinimumOffTimeHeld` bzw. `PolarityDeadTimeHeld` (bei exaktem Gleichstand beider Fristen: `PolarityDeadTimeHeld`) | `Idle` | unverändert |
| 10 | bestätigter Richtungswechsel wird in diesem Tick enabled (Mindestzeiten erfüllt, Bestätigung abgeschlossen, siehe 8.5) | `Active` | `DirectionChangeApplied` | neue Richtung startet, neues Fenster (8.1) | Verwurf des Altrichtungs-Guthabens (8.4), neues Fenster startet leer |
| 11 | `acceptedCommand.direction == Idle` (`NeutralOff`/`AirLimitBlockedOff`), kein Mindest-Einschaltzeit-Halt aus Rang 8 | `Idle` | `NeutralIdle` bzw. `AirLimitBlocked` | `Idle` | siehe Abschnitt 7.3 |
| 12 | normale Fensterauswertung (8.1), Akkumulator unter Schwelle | `Idle` | `AccumulatingBelowThreshold` | `Idle` | `+= requestedOnMillisExact` |
| 12 | normale Fensterauswertung, Akkumulator löst Mindestimpuls aus | `Active` | `MinimumPulseTriggered` | Richtung an für `minimumOnMillis` | `-= minimumOnMillis` |
| 12 | normale Fensterauswertung, direkte Planung | `Active` (falls `elapsedInWindow < scheduledOnMillis`) sonst `Idle` | `ScheduledWithinWindow` | gemäß `desiredActive` | unverändert |

Zusätzlich, unabhängig vom Rang: `counterDirectionConfirming` (Bool im
Ergebnis) wird gesetzt, wenn ein Gegenrichtungskandidat aktuell beobachtet,
aber noch nicht bestätigt ist (Abschnitt 8.5) – dies ist eine Zusatzauskunft,
keine eigene Prioritätsstufe, um keine kombinatorische Reason-Explosion zu
erzeugen.

`RequiredSensorUnavailable` aus Revision 1 entfällt: Ein fehlender/ungültiger
Pflichtsensor erscheint aus #23-Sicht ausschließlich als
`ActuatorDemandClass::NoValidRequest` (#22 liefert dafür bereits
`Unavailable/SensorUnavailable` oder `InvalidInput/InvalidSample`) und landet
damit korrekt in Rang 7, ohne mit `InvalidConfiguration` oder `TimeInvalid`
vermischt zu werden (löst den expliziten Revision-1-Mangel aus R12).

### 8.3 Overflow-sichere Zeitarithmetik (löst R12, Teil 1)

Jede Fristprüfung verwendet ausschließlich elapsed-time-Vergleiche, nie eine
ungeprüfte Deadline-Addition:

```cpp
// Kanonisches Muster fuer jede einzelne Frist:
[[nodiscard]] bool deadlineReached(std::uint64_t now, std::uint64_t since,
                                    std::uint64_t durationMillis) {
    if (now < since) {
        return false;  // Retrograde -> Frist gilt defensiv als NICHT erreicht
    }
    return (now - since) >= durationMillis;
}
```

Ein `now < since`-Fall ist laut `ITimeSource`-Vertrag ausgeschlossen
(monoton, nie rückwärts), wird aber defensiv abgefangen und zusätzlich als
eigene Diagnose behandelt: Tritt `now < since` an einer der folgenden
Referenzen auf – `directionActivatedAtMonotonicMillis`,
`directionDeactivatedAtMonotonicMillis`, `windowStartMonotonicMillis`,
`counterDirectionObservedSinceMonotonicMillis`,
`lastNewRequestAcceptedAtMonotonicMillis`,
`outerFanPostRunUntilMonotonicMillis`/`innerFanPostRunUntilMonotonicMillis`
(hier: `now < deadline` ist normal, aber `now` kleiner als der zuletzt
gesehene `now` eines vorherigen Ticks wäre die Anomalie) – wird dies als
`TimeInvalid` (Rang 2-äquivalent, `InvalidInput`) eingestuft und der Tick
verhält sich wie Rang 1 (sofortiger, unbedingter Verwurf, `Idle`). Es gibt
keine Stelle im gesamten Vertrag, an der eine Deadline durch `start + dauer`
berechnet und dann direkt mit `now` verglichen wird; `now + fanPostRunMillis`
aus Revision 1 entfällt zugunsten der elapsed-Form: Der Lüfter-Nachlauf merkt
sich stattdessen `fanDeactivationRequestedAtMonotonicMillis` und prüft
`deadlineReached(now, fanDeactivationRequestedAtMonotonicMillis,
fanPostRunMillis)`.

### 8.4 Einziger richtungsgebundener Akkumulator (löst R7)

```cpp
struct PulseAccumulator {
    AbstractControlDirection direction{AbstractControlDirection::Idle};
    double accumulatedMillis{0.0};
};
```

Es existiert zu jedem Zeitpunkt genau **ein** `PulseAccumulator` im
`ActuatorPlannerRuntimeState`. `direction == Idle` bedeutet „leer/nicht
gebunden“; nur `Heating` oder `Cooling` tragen jemals ein Guthaben `> 0`.
Ein unbestätigter Gegenrichtungskandidat (Abschnitt 8.5,
`counterDirectionCandidate`) hat **keinen** eigenen Akkumulator und lädt
keinen versteckten zweiten Wert – er ist ausschließlich ein Timer/Kandidat
ohne Guthabenwirkung, bis die Bestätigung abgeschlossen ist.

Der Akkumulator wird unbedingt auf `{Idle, 0.0}` zurückgesetzt bei:

- bestätigtem, enabled Richtungswechsel (Rang 10) – Restguthaben der alten
  Richtung wird verworfen, bevor das neue Fenster startet;
- Übergang in `AirLimitBlockedOff` (Rang 11, Abschnitt 7.3);
- Übergang in `AirLimitReducedDemand` aus einer weniger restriktiven
  Klasse (Abschnitt 7.3);
- jedem Rang-1-bis-6-Ereignis (strukturell ungültig, fehlende
  Kommissionierung/ungültige Konfiguration, Safety, latched Fault, laufender
  Watchdog, Kontext-Stale);
- `forceStop()` (Abschnitt 11).

Wird gleichzeitig ein Gegenrichtungskandidat verworfen (jeder der obigen
Trigger, sowie ein Abbruch der Bestätigung selbst, Abschnitt 8.5), wird
`counterDirectionCandidate` in derselben Aktion auf `std::nullopt` gesetzt.
Der Akkumulator ist damit zu jedem Zeitpunkt eindeutig entweder leer oder an
exakt eine Richtung gebunden; ein zweiter, „versteckter“ Akkumulator für eine
unbestätigte Gegenrichtung existiert im Datenmodell nicht (kein zweites
`double`-Feld).

### 8.5 Bestätigter Richtungswechsel

Solange eine physisch aktive Richtung besteht (Rang 8/12 mit
Richtung `!= Idle`) und `acceptedCommand.direction` die entgegengesetzte
Richtung anzeigt (`Heating` während `Cooling` aktiv ist oder umgekehrt) mit
`acceptedCommand`s zugehöriger `TemperatureControlResult`-Quote
`>= counterDirectionConfirmationQuoteThreshold` und Klasse `NormalDemand`
oder `AirLimitReducedDemand`: Falls `counterDirectionCandidate` leer oder
`!= Gegenrichtung` ist, wird ein neuer Kandidat mit
`counterDirectionObservedSinceMonotonicMillis = now` gestartet
(`counterDirectionConfirming = true` im Ergebnis). Jede Unterbrechung
(Gegenrichtung fällt unter die Schwelle, wechselt zurück, wird
`NoValidRequest`/`NeutralOff`/`AirLimitBlockedOff`, oder einer der
Rang-1-bis-6-Trigger tritt ein) setzt `counterDirectionCandidate` sofort
zurück (Reason des Ticks bleibt unabhängig davon die normale Reason der noch
aktiven alten Richtung, siehe 8.2). Erst wenn
`deadlineReached(now, counterDirectionObservedSinceMonotonicMillis,
counterDirectionConfirmationDurationMillis)` **ununterbrochen** erreicht
wird, gilt die Gegenrichtung als bestätigt: Die alte Richtung wird
`Idle` (Rang 9-Logik: Mindest-Auszeit und Totzeit müssen ab diesem Moment
zusätzlich ablaufen, bevor Rang 10 tatsächlich enabled).

Vor Ablauf der Mindest-Einschaltzeit der alten Richtung bleibt eine
Gegenanforderung – bestätigt oder nicht – wirkungslos auf die physische
Ausgabe (Rang 8 hat Vorrang vor Rang 9/10); der Bestätigungstimer selbst
läuft davon unbeeinflusst weiter, damit eine bereits während der
Mindest-Einschaltzeit begonnene, durchgehende Bestätigung nicht künstlich neu
gestartet werden muss, sobald die Mindest-Einschaltzeit endet.

### 8.6 Watchdog-Fault-Evidenz (löst R5)

Löst der laufende Watchdog aus (Abschnitt 6.3, Rang 5), wird

```cpp
state_.latchedWatchdogFault = ActuatorWatchdogFaultEvidence{
    .detectedAtMonotonicMillis = now,
    .lastAcceptedSequenceBeforeFault =
        state_.lastAcceptedSequence.value_or(0U),
};
```

gesetzt. Solange `latchedWatchdogFault.has_value()`, bleibt Rang 4 in jedem
weiteren Tick maßgeblich (`Idle`, `RequestWatchdogFaultLatched`) – **auch**
wenn danach eine strukturell einwandfreie, kontextfrische, an sich
akzeptable neue `newEvaluation` eintrifft. Eine neue Request wird zwar
weiterhin für die Sequenz-/Kontext-Bücher (`lastAcceptedSequence`,
Dedupe-Schutz) berücksichtigt, öffnet aber **keine** neue physische Freigabe.
Die einzige Methode, die `latchedWatchdogFault` löscht, ist
`acknowledgeWatchdogFault()`. Diese Methode wird laut diesem Plan
**ausschließlich** von #24-getriebener Logik aufgerufen (die künftige
Fault-/Wiederfreigabe-Entscheidung), niemals implizit aus `tick()` selbst.
Bis #24 real existiert, verdrahtet die Composition Root
`acknowledgeWatchdogFault()` **nicht** – ein einmal ausgelöster
Watchdog-Fault bleibt für den Rest der Boot-Session verriegelt; erst ein
echter Neustart (neue `ActuatorPlanner`-Instanz) hebt ihn auf. Dies ist
ausdrücklich das in R5 geforderte, benannte Integrationsgate vor #24 und kein
permissiver Default.

`NewActiveRun` (Lifecycle-Grenze, Abschnitt 11) löscht
`latchedWatchdogFault` zusätzlich implizit, da ein neuer, über die volle
Kommando-/Persistenzkette bestätigter Lauf einen fachlich neuen Kontext
darstellt (analog dazu, dass #22 an derselben Grenze ebenfalls vollständig
zurückgesetzt wird); alle anderen Lifecycle-Grenzen
(`LeaveTemperatureControl`, `Recovery`, `Fault`, `SafeBoot`, `Service`,
`Standby`) lassen `latchedWatchdogFault` unverändert bestehen.

## 9. Feedback-Dispositionsmatrix (löst R2)

`ActuatorPlanTickResult::feedbackForAcceptedRequest` ist
`std::optional<PreviousControlRequestFeedback>` und referenziert exakt die
Sequenz des **physisch aktiven** (Heating/Cooling) `acceptedCommand`, dessen
Disposition dieser Tick bewertet. Ein `Idle`-`acceptedCommand`
(`NeutralOff`/`AirLimitBlockedOff`) öffnet – wie im gemergten #22-Vertrag
festgelegt – **niemals** einen Feedbackvertrag: `feedbackForAcceptedRequest`
bleibt `std::nullopt`, solange keine Heating-/Cooling-Sequenz zu bewerten ist.

| Fall (Rang aus 8.2, sofern zutreffend) | Disposition | Feedback erzeugt? |
|---|---|---|
| Rang 12, `ScheduledWithinWindow` physisch angewendet | `NoIntegratorConstraint` | ja, für die aktuell aktive Sequenz |
| Rang 12, `MinimumPulseTriggered` physisch angewendet | `NoIntegratorConstraint` | ja |
| Rang 12, `AccumulatingBelowThreshold` | `DeferredOrLimited` | ja |
| Rang 8, `MinimumOnTimeHeld` (alte Richtung gehalten) | `DeferredOrLimited` | ja, für die alte aktive Sequenz |
| Rang 9, `MinimumOffTimeHeld`/`PolarityDeadTimeHeld` (neue Richtung wartet) | `DeferredOrLimited` | ja, für die wartende neue Sequenz, sobald sie als `acceptedCommand` geführt wird |
| Gegenrichtung wird beobachtet/bestätigt, alte Richtung läuft unverändert nach ihrer eigenen Rang-12-Reason weiter | Disposition der eigenen Rang-12/8-Reason (siehe oben) – keine eigene Zusatzdisposition | wie die zugrunde liegende Reason |
| Rang 10, `DirectionChangeApplied` (alte Richtung endet legitim durch bestätigten Wechsel) | `DeferredOrLimited` für die soeben beendete alte Sequenz (letztmalig) | ja, genau einmal, im selben Tick, in dem der Wechsel enabled wird |
| Rang 3, `SafetyGateUnresolved`/`ExternalSafetyOverride` mit zuvor aktivem `acceptedCommand` | `Rejected` | ja, einmalig für die soeben verworfene Sequenz |
| Rang 6, `StaleRequestContext` mit zuvor aktivem `acceptedCommand` | `Rejected` | ja, einmalig |
| Rang 5, `StaleRequestWatchdog` (Trip-Tick) mit zuvor aktivem `acceptedCommand` | `Rejected` | ja, einmalig, im Trip-Tick |
| Rang 4, `RequestWatchdogFaultLatched` (Folge-Ticks) | kein `acceptedCommand` mehr vorhanden | nein |
| Rang 1, `MalformedEvaluation` mit zuvor aktivem `acceptedCommand` | `Rejected` | ja, einmalig, im selben Tick |
| Rang 7, `NoValidRequest` | kein `acceptedCommand` | nein |
| Rang 11, `NeutralIdle`/`AirLimitBlocked` (Richtung `Idle`) | kein Feedbackvertrag laut #22 | nein |
| Annahmeversuch abgelehnt: `DuplicateOrOldSequence` (6.2 Schritt 3) | betrifft nicht die laufende, weiterhin gültige Sequenz | nein zusätzliches Feedback für den abgelehnten Kandidaten; die laufende Sequenz erhält ihre normale Disposition gemäß der obigen Fälle, unbeeinflusst |
| Annahmeversuch abgelehnt: `StaleRequestWatchdog`/`StaleRequestContext` bei Ankunft (6.2 Schritt 4/5) | betrifft nicht die laufende, weiterhin gültige Sequenz | nein zusätzliches Feedback für den abgelehnten Kandidaten |

Damit ist eindeutig festgelegt:

- `NoIntegratorConstraint` ist ausschließlich zulässig, wenn diese konkrete
  Sequenz in diesem Tick tatsächlich physisch (direkt oder als ausgelöster
  Mindestimpuls) angesteuert wird.
- `DeferredOrLimited` ist zwingend bei jeder Form von zeitlichem Aufschub
  (Akkumulation, Mindest-Ein-/Auszeit, Totzeit) sowie beim legitimen Ende
  durch einen bestätigten Richtungswechsel.
- `Rejected` ist zwingend bei vollständigem, nicht-zeitlichem Verwurf
  (Safety, Kontext-Stale, Watchdog-Trip, strukturell ungültige Evaluation).
- Kein Feedback entsteht für `Idle`-Anforderungen, für bereits latched
  Watchdog-Fault-Folgeticks (kein `acceptedCommand` mehr vorhanden) und für
  abgelehnte *neue* Kandidaten, die eine weiterhin laufende, gültige
  Sequenz unberührt lassen.
- Jede physisch aktive Sequenz erzeugt pro Tick höchstens eine Disposition;
  der Aufrufer (`TemperatureControlApplicationOrchestrator`) konsumiert
  jeweils den **letzten** `tick()`-Ergebniswert unmittelbar vor dem nächsten
  `evaluateTemperatureControl()`-Aufruf über das bereits bestehende Feld
  `TemperatureControlEvaluationEvidence::previousControlRequestFeedback` –
  das ist der bereits vorhandene, einzige Konsumpunkt (kein neuer
  Mechanismus); „höchstens einmal konsumierbar“ ergibt sich daraus, dass
  jeder #22-`evaluate()`-Aufruf ohnehin genau einen frischen
  Feedback-Eingabewert erwartet.

## 10. Lüfterlogik

Bleibt inhaltlich zu Revision 1 unverändert, jedoch mit overflow-sicherer
Arithmetik (Abschnitt 8.3) und explizit an den Lifecycle-Stop-Ablauf
angebunden (Abschnitt 11):

- **Außenlüfter**: `outerFanEnabled = true`, sobald die physische Richtung
  `!= Idle` ist. Beim Übergang auf physisch `Idle` wird
  `outerFanDeactivationRequestedAtMonotonicMillis = now` gesetzt;
  `outerFanEnabled` bleibt `true`, bis
  `deadlineReached(now, outerFanDeactivationRequestedAtMonotonicMillis,
  outerFanPostRunMillis)`. Eine erneute Freigabe während des Nachlaufs setzt
  `outerFanDeactivationRequestedAtMonotonicMillis` zurück auf
  `std::nullopt`, ohne dass der Lüfter zwischenzeitlich `false` war. Kein
  Vorlauf: `outerFanEnabled` wird im selben `tick()`-Aufruf gesetzt, in dem
  auch die Peltierfreigabe erfolgt.
- **Innenlüfter**: `innerFanEnabled = true`, solange
  `input.temperatureControlledPhase == true` – unabhängig vom aktuellen
  Peltier-Fensterzustand. Beim Verlassen der temperaturgeregelten Phase
  startet ein eigener, unabhängiger Nachlauf
  (`innerFanDeactivationRequestedAtMonotonicMillis`/`innerFanPostRunMillis`,
  gleiches Muster wie oben). Kurze Peltier-Auszeiten *innerhalb* einer
  weiterhin temperaturgeregelten Phase lösen keinen Innenlüfter-Nachlauf aus.

## 11. Lifecycle-Integration und Stop-Ablauf (löst R6)

`resetActuatorPlanAtBoundary()` ist eine freie Funktion, die **ausschließlich**
von `TemperatureControlApplicationOrchestrator::complete()` (beziehungsweise
demselben Chokepoint wie `resetTemperatureControlAtBoundary()`) aufgerufen
wird – an derselben committed Lifecycle-Grenze, mit demselben
`TemperatureControlLifecycleBoundary`-Wert, im selben Funktionsaufruf, in dem
bereits `resetTemperatureControlAtBoundary()` aufgerufen wird. Es gibt keinen
zweiten, separat zu erinnernden Caller-Schritt.

```cpp
ActuatorPlanTickResult resetActuatorPlanAtBoundary(
    ActuatorPlanner& planner, ActuatorPlanSinkDriver& driver,
    TemperatureControlLifecycleBoundary boundary,
    std::uint64_t nowMonotonicMillis);
```

Ablauf (identisch für jede der sieben Grenzen `NewActiveRun`,
`LeaveTemperatureControl`, `Recovery`, `Fault`, `SafeBoot`, `Service`,
`Standby`; `NewActiveRun` zusätzlich mit dem in Abschnitt 8.6 genannten
Watchdog-Fault-Löscheffekt):

1. `ActuatorPlanTickResult result = planner.forceStop(nowMonotonicMillis);`
   – `forceStop()` berechnet **denselben** Ergebnistyp wie `tick()`
   (Status `Idle`, Reason abhängig vom vorherigen Zustand – bei zuvor
   physisch aktiver Richtung wird intern exakt der Abschalt-Pfad aus
   Abschnitt 12 durchlaufen, inklusive Start/Fortsetzung des
   Außenlüfter-Nachlaufs; ein bereits laufender Nachlauf wird dabei
   **nicht** verkürzt oder verworfen, da `forceStop()` dieselbe
   `outerFanDeactivationRequestedAtMonotonicMillis`-Fortschreibung wie ein
   gewöhnlicher Übergang auf `Idle` verwendet, nicht einen abrupten
   Reset auf `false`).
2. Akkumulator und Gegenrichtungskandidat werden gemäß Abschnitt 8.4
   unbedingt verworfen (bereits Teil von `forceStop()`).
3. `driver.apply(result);` – dieselbe dünne Übersetzungsfunktion wie bei
   jedem gewöhnlichen Tick (Abschnitt 12) wendet das Ergebnis auf die realen
   Sinks an; es gibt keine zweite, abweichende Ausgabelogik für den
   Lifecycle-Pfad.
4. Danach ist `state().physicalDirection == Idle`,
   `state().acceptedCommand == std::nullopt`,
   `state().windowInitialized == false`; ein nachfolgender gewöhnlicher
   `tick()`-Aufruf beginnt regulär aus diesem sicheren Ausgangszustand.

`planner.resetRuntime()` als eigenständige, vom Stop-Ablauf getrennte
Methode existiert **nicht** mehr (löst den in R6 explizit benannten Mangel:
„`resetRuntime()` darf einen vorher aktiven Peltierzustand nicht so löschen,
dass der notwendige Lüfternachlauf verloren geht“) – `forceStop()` ist die
einzige RAM-Reset-Operation und berechnet dabei immer korrekt den nötigen
Nachlauf, statt ihn stillschweigend zu verwerfen.

## 12. Sink-Ausgabereihenfolge (löst R11)

`ActuatorPlanSinkDriver::apply(const ActuatorPlanTickResult&)` setzt die
Sinks in genau dieser Reihenfolge:

**Freigabe Heating** (Übergang zu `appliedDirection == Heating`):
1. `peltier_.setReverse(false)`;
2. `outerFan_.setEnabled(true)`;
3. `peltier_.setForward(true)`.

**Freigabe Cooling** (Übergang zu `appliedDirection == Cooling`):
1. `peltier_.setForward(false)`;
2. `outerFan_.setEnabled(true)`;
3. `peltier_.setReverse(true)`.

**Abschalten** (Übergang zu `appliedDirection == Idle`):
1. `peltier_.setForward(false)`; `peltier_.setReverse(false)`;
2. Außenlüfter bleibt gemäß Nachlauf (`outerFanEnabled` aus dem Ergebnis)
   unverändert `true` oder wird `false`, falls der Nachlauf bereits
   abgelaufen ist;
3. Innenlüfter analog gemäß eigenem Nachlauf.

**Richtungswechsel**: Ein direkter Wechsel `Forward true -> Reverse true`
(oder umgekehrt) innerhalb eines einzigen `apply()`-Aufrufs ist durch die
Planner-Logik (Abschnitt 8.5, Rang 8–10) bereits strukturell ausgeschlossen
– zwischen einer alten und einer neuen Richtung liegt immer mindestens ein
Tick mit `appliedDirection == Idle`. Der Driver selbst erzwingt dies
zusätzlich defensiv: Vor jedem `setForward(true)`/`setReverse(true)` wird
immer zuerst die jeweils andere Richtung explizit `false` gesetzt (siehe
Schritte 1 oben), auch wenn sie laut internem Zustand bereits `false` sein
sollte.

Der Driver ist zustandslos bezüglich der Reihenfolge-Entscheidung selbst
(die Entscheidung *was* anzuwenden ist, trifft ausschließlich der Planner);
er hält nur die drei Sink-Referenzen. Für Tests wird kein neuer
Produktions- oder Test-Recorder benötigt: `MockBidirectionalActuatorSink`
journalisiert bereits jeden `setForward`/`setReverse`-Aufruf mit Reihenfolge
und stellt `simultaneousActivationObserved()` bereit;
`MockBinaryOutputSink` journalisiert ebenso. Ein Test ruft `driver.apply()`
mit einer kontrollierten Sequenz von `ActuatorPlanTickResult`-Werten auf und
prüft die drei `commandJournal()`-Verläufe sowie
`simultaneousActivationObserved() == false` über die gesamte Sequenz.

## 13. Lauf-Snapshot-/Parameterbindung (löst R10)

`ActuatorPlannerParameters` ist ein unveränderlicher Wert, der ausschließlich
bei Konstruktion eines `ActuatorPlanner` übergeben wird; es gibt keinen
Setter und keine Methode, die die Parameter eines existierenden
`ActuatorPlanner` nachträglich ändert (identisch zum bereits etablierten
Muster von `TemperatureController`, das ebenfalls unveränderliche
`TemperatureControlParameters` bei Konstruktion übernimmt – kein neues
Muster, sondern Wiederverwendung).

Die geforderte Laufbindung („ein laufender Prozess verwendet den im
Laufschnappschuss festgelegten wirksamen Wert, Änderungen gelten erst für
zukünftige Läufe“) wird durch folgende, in diesem Plan verbindlich
festgelegte Regel erreicht: **Die Composition Root rekonstruiert die aktive
`ActuatorPlanner`-Instanz ausschließlich am `NewActiveRun`-Lifecycle-Grenze
und liest dabei genau einmal die zu diesem Zeitpunkt aktuellen,
PIN-geschützten Service-Einstellungen.** Innerhalb eines laufenden Prozesses
wird `ActuatorPlanner` nicht neu konstruiert und seine Parameter nicht
anderweitig verändert.

Die konkrete Persistenz-/Konfigurationsanbindung, über die diese
Service-Einstellungen als unveränderlicher Per-Run-Snapshot erfasst und der
Composition Root zum `NewActiveRun`-Zeitpunkt bereitgestellt werden (z. B.
eine künftige `ActuatorPlanParametersSnapshot`-Struktur analog zu den
bestehenden laufgebundenen Effektivwert-Schnappschüssen), ist **ausdrücklich
nicht Teil dieses Plans**, da Issue #23 laut seinem Scope keine
`run_persistence_*`-Änderung vornimmt (Abschnitt 2). Dies wird hiermit
namentlich als offenes Integrationsgate für ein späteres Issue (am
plausibelsten im Umfeld von Issue #19 oder einem eigenen Folge-Issue, sofern
nicht bereits durch #21/#22-Folgearbeit abgedeckt) festgehalten. Der
strukturelle Beitrag von Issue #23 – unveränderliche Parameter pro
`ActuatorPlanner`-Instanz, Rekonstruktion ausschließlich an `NewActiveRun` –
ist bereits jetzt vollständig und ausreichend, um dieses spätere Gate ohne
weitere #23-Änderung zu bedienen.

## 14. Adopt-or-build (löst R13)

| Baustein | Geprüfter Kandidat | Entscheidung | Begründung |
|---|---|---|---|
| Fachlicher #23-Kern (Fenster, Mindestzeiten, Totzeit, Akkumulator, Request-Kontext, Watchdog, #22-Feedback, #24-Gate) | ESP-IDF `esp_timer`/GPTimer (monotone Hardware-Timer), MCPWM (Hardware-PWM-Peripherie); `espressif/*`-Registry (keine passende fachliche Komponente); Arduino PID/QuickPID (`docs/THIRD_PARTY_COMPONENTS.md`, Zeile „Regelung“, `NOT_SELECTED`) | **build** | `esp_timer`/GPTimer und MCPWM sind Hardware-/Peripherieprimitiven für Zeitgabe beziehungsweise PWM-Erzeugung; sie ersetzen nicht den portablen, fachlich definierten #23-Vertrag aus Schaltfenster-, Akkumulator-, Kontext-, Watchdog- und Feedbacksemantik (Abschnitt 6–9). Eine Delegation an eine dieser Komponenten würde entweder Hardwareabhängigkeit in den nativen, hardwarefreien Fachkern tragen (verboten laut ADR-013/AGENTS.md) oder den kanonischen #22/#23/#24-Vertrag verwischen (ausdrücklich ausgeschlossen laut AGENTS.md). Keine geprüfte Komponente deckt Mindestzeiten, Totzeit, Impulsakkumulator und Kontext-Watchdog in dieser fachlichen Kombination ab. |
| Zeitquelle | bereits vorhandener `device_platform::ITimeSource`/`VirtualTimeSource` | **adopt (bereits vorhanden)** | Deckt monotone Zeit und native Determinismus bereits vollständig ab; kein Grund für eine Hardware-Timer-Abhängigkeit im Fachkern |
| Bidirektionaler Aktor-Port (Peltier/H-Brücke) | bereits vorhandener `device_platform::IBidirectionalActuatorSink` | **adopt (bereits vorhanden)** | Port existiert bereits inklusive Mock mit Reihenfolge-/Exklusivitätsprüfung, bisher ungenutzt |
| Binärer Ausgangs-Port (Lüfter) | bereits vorhandener `device_platform::IBinaryOutputSink` | **adopt (bereits vorhanden)** | Zwei Instanzen (außen/innen) über Composition Root zugeordnet, kein neuer Port nötig |
| Spätere Hardware-Adapterprimitiven (`esp_timer`, GPTimer, MCPWM) | s.o. | **spätere Adapterentscheidung, nicht Teil dieses PR** | Kämen frühestens beim `device_platform_esp_idf`-Adapter für die reale H-Brücken-/Lüfteransteuerung infrage (außerhalb des Plan-Scopes, siehe Abschnitt 2); keine Vorwegnahme einer Produktauswahl hier |

Es wird keine neue Drittabhängigkeit vorgeschlagen; `docs/THIRD_PARTY_COMPONENTS.md`
wird durch diesen Plan nicht geändert.

## 15. SOLID, DRY, KISS

- **Single Responsibility:** `ActuatorPlanner` entscheidet ausschließlich
  Timing/Aktorplanung; `ActuatorPlanSinkDriver` übersetzt ausschließlich ein
  bereits fertiges Ergebnis in Sink-Aufrufe; PI-Mathematik bleibt vollständig
  in #22, Safety-Ursache vollständig in #24.
- **Open/Closed:** Der Planer erweitert die bestehende Kette
  (`TemperatureControlResult -> ActuatorPlanTickResult`) über neue Bausteine,
  ohne `temperature_control.*` zu verändern; die bestehende
  `TemperatureControlApplicationOrchestrator`-Grenze wird durch zusätzliche
  Methoden/Konstruktorparameter erweitert, nicht dupliziert.
- **Liskov/Interface Segregation:** Es werden ausschließlich bereits
  bestehende schmale Ports verwendet; kein neues, breiteres Interface. Die
  neuen Werttypen (`ActuatorSafetyGateInput`, `ActuatorWatchdogFaultEvidence`)
  sind bewusst keine virtuellen Interfaces, solange keine zweite
  Implementierung existiert.
- **Dependency Inversion:** `ActuatorPlanner` hängt von keiner Hardware ab;
  `ActuatorPlanSinkDriver` hängt nur von den abstrakten `device_platform`-
  Ports ab, nicht von ESP-IDF oder GPIO.
- **DRY:** Eine einzige Quelle für Richtung (`AbstractControlDirection`,
  kein zweites Enum, löst R7), eine einzige Quelle für Request-Identität/
  -Kontext (#22s `ControlRequestIdentity`/`ControlRequestContext`), eine
  einzige Quelle für Lifecycle-Grenzen (`TemperatureControlLifecycleBoundary`),
  eine einzige Quelle für Phasenklassifikation
  (`isTemperatureControlledProcessState`), eine einzige Application-/
  Lifecycle-Autorität (`TemperatureControlApplicationOrchestrator`, löst R6),
  ein einziger bereits vorhandener Feedback-Übergabemechanismus
  (`previousControlRequestFeedback`, löst R2), eine einzige
  Ausgabereihenfolge-Implementierung für Tick- und Stop-Pfad (löst R6/R11).
- **KISS:** `ActuatorSafetyGateInput` bleibt ein einfacher Werttyp statt
  eines vorzeitigen Interfaces; keine Kaskaden- oder Mehrfach-Timer-
  Architektur, wo ein einzelner Fenster-/Akkumulatorzustand je Zeitpunkt
  ausreicht; kein zweiter Test-Recorder, da die vorhandenen Mocks bereits
  Reihenfolge und Exklusivität journalisieren; keine vorsorgliche
  Generalisierung für andere Aktortypen oder zukünftige Geräte ohne
  aktuellen Bedarf.

## 16. Safety-, Security-, Recovery- und Hardwaregrenzen

- Fail-closed bei jedem Rang 1–7 aus Abschnitt 8.2: `Idle`, kein Feedback
  außer dem einmaligen `Rejected` für eine soeben verworfene Sequenz, kein
  Akkumulator-Nachholen.
- Keine Aktorfreigabe wird bei Boot, Reset, Fehler, unbekanntem Zustand oder
  unbestätigter Hardware vorausgesetzt; `ActuatorSafetyGateInput` startet mit
  `Unresolved` und erzwingt `Idle`, bis explizit `Allowed` gesetzt wird
  (löst R4).
- `ActuatorSafetyGateInput.status == ImmediateStop` überstimmt die
  Mindest-Einschaltzeit ausschließlich in Richtung „sicherer machen“, niemals
  in Richtung „Freigabe erzwingen“.
- Ein Watchdog-Fault bleibt latched und wird nicht durch eine bloß neue,
  ansonsten gültige Request stillschweigend gelöscht (löst R5); die einzige
  Wiederfreigabe ist die künftige, hier nicht implementierte #24-Logik über
  `acknowledgeWatchdogFault()`.
- Keine Persistenz, keine Recovery-Sonderpfade: Der Planer wird nach jedem
  Lifecycle-Boundary-Stop (`forceStop()`, Abschnitt 11) regulär aus `Idle`
  neu bewertet; es wird kein Fortschritt aus der Zeit vor einem Neustart
  rekonstruiert.
- Kein Türkontakt, keine Kühlkörper-Grenzwertlogik, keine absolute
  Temperatursicherheit – bleibt #24/`SAFETY_AND_FAULTS.md`.
- Keine Hardwarewerte (GPIOs, Pegel, BTS7960-Sequenz) werden in diesem Plan
  festgelegt; `OPEN_POINTS.md` (#29, #32, #33, #35) bleibt unverändert
  sichtbar offen.

## 17. Betroffene Module – Zusammenfassung

Siehe Abschnitt 5 für die vollständige Datei-/Modulliste.

## 18. Umsetzungs- und Commit-Schnitte

1. **Gemeinsame schmale Verträge / Klassifikation** –
   `actuator_plan_types.hpp` vollständig (Abschnitt 4.2, 7.2, 8.2, 8.3, 8.4,
   8.6), `classifyActuatorDemand()`, `classifyActuatorPlannerParameters()`
   inklusive vollständiger struktureller Invarianten (Abschnitt 8.2 Rang 2);
   keine Verhaltenslogik.
2. **Tick-/Annahme-Kernlogik** – `ActuatorPlanner::tick()` Grundgerüst:
   Annahmelogik (6.2), laufender Watchdog (6.3), Prioritätsleiter Rang 1–7
   (8.2), overflow-sichere Zeitarithmetik (8.3), `forceStop()`,
   `acknowledgeWatchdogFault()`.
3. **Fenster, Akkumulator, Mindestzeiten, Totzeit, Richtungswechsel** –
   Fensterlogik (8.1), einziger Akkumulator (8.4), bestätigter
   Richtungswechsel (8.5), Prioritätsleiter Rang 8–12 vollständig.
4. **Lüfterlogik** – Außen-/Innenlüfter (10), eigener, von der
   Fenster-/Mindestzeitlogik klar getrennter Codeabschnitt.
5. **Feedback-Dispositionsmatrix** – vollständige Umsetzung von Abschnitt 9
   innerhalb von `tick()`/`forceStop()`.
6. **Sink-Driver und Application-/Lifecycle-Integration** –
   `ActuatorPlanSinkDriver` (12), Erweiterung von
   `TemperatureControlApplicationOrchestrator` um `tickActuatorPlan()` und
   die gemeinsame Lifecycle-Stop-Integration (11); keine
   Composition-Root-Verdrahtung (außerhalb des Plan-Scopes).
7. **Tests und Dokumentation** – vollständige native Testklassen gemäß
   Abschnitt 19; `docs/ACTUATOR_TIMING.md` „Akzeptierte Entscheidungen“ um
   Strukturhinweise ergänzen, ohne `TBD_COMMISSIONING`-Werte zu erfinden.
8. **Abschlussnachweise** – gezielte und (nach Owner-Freigabe) vollständige
   lokale Läufe, PR-Nachweis, `SESSION HANDOVER`.

Diese Reihenfolge folgt der Abhängigkeitsstruktur des Vertrags selbst (Typen
-> Annahme/Zeitmodell -> Fenster/Zeiten -> Lüfter -> Feedback -> Integration
-> Tests -> Nachweis); jede Stufe baut ausschließlich auf bereits in einer
vorherigen Stufe festgelegten Verträgen auf.

## 19. Teststrategie

Alle Tests sind native, deterministische Orakel unter `pio test -e native`
und verwenden ausschließlich `VirtualTimeSource`,
`MockBidirectionalActuatorSink` und `MockBinaryOutputSink` aus
`device_platform_test_support`. Testparameter sind frei gewählte, plausible
Testwerte – keine Produktionskommissionierung. Randomisierte Sequenzen (sofern
verwendet) nutzen einen festen Seed und geben bei Fehlschlag den Seed und die
Eingabesequenz reproduzierbar aus.

### 19.1 `test/test_actuator_planner/test_actuator_planner.cpp`

**Aufruf-/Zeitmodell (R1):**
- unterschiedliche PI-Request- und Planner-Tick-Zeitpunkte: mehrere `tick()`-
  Aufrufe mit `newEvaluation = std::nullopt` zwischen zwei tatsächlichen
  #22-Evaluationen führen einen laufenden Impuls korrekt zu Ende;
- ein `tick()`-Aufruf mit derselben Sequenz wie zuvor akzeptiert
  (`newEvaluation` erneut gesetzt) schaltet nicht ab und stört die laufende
  Zeitbasis nicht;
- mehrere neue Evaluationen mit identischer Quote innerhalb desselben
  Fensters führen zu genau einer Akkumulator-Gutschrift für dieses Fenster
  (keine Mehrfach-Akkumulation);
- Replay einer alten Sequenz zerstört einen bereits laufenden, gültigen
  Zeitplan nicht.

**Feedback-Dispositionsmatrix (R2):** je ein direkter Test pro Zeile aus
Abschnitt 9, inklusive: Feedback referenziert exakt die bewertete Sequenz;
OFF-`acceptedCommand` erzeugt nie Feedback; latched Watchdog-Fault-Folgeticks
erzeugen kein Feedback mehr.

**AirLimit-Klassifikation (R3):**
- `NeutralOff` vs. `AirLimitBlockedOff`: identische Mindestzeit-Behandlung,
  unterschiedliche Akkumulatorwirkung;
- aufgebautes Heiz-Guthaben, danach `AirLimitBlockedOff` beobachtet: Guthaben
  verworfen, kein Nachhol-Mindestimpuls danach;
- aufgebautes Guthaben, danach Übergang zu `AirLimitReducedDemand`: Guthaben
  verworfen, folgende Fenster akkumulieren ausschließlich aus der reduzierten
  Quote;
- fortlaufendes Verbleiben in `AirLimitReducedDemand` über mehrere Fenster:
  normale, nicht zusätzlich beschnittene Akkumulation.

**Safety-Gate (R4):** `Unresolved`, `Allowed`, `ImmediateStop`; `ImmediateStop`
überstimmt eine aktive Mindest-Einschaltzeit; `Unresolved` erzwingt `Idle`
auch bei ansonsten vollständig gültiger Request.

**Watchdog-Fault-Evidenz (R5):** Watchdog-Trip erzeugt
`ActuatorWatchdogFaultEvidence`; eine danach eintreffende, ansonsten gültige
neue Request löscht den latched Fault **nicht**; nur
`acknowledgeWatchdogFault()` löscht ihn; `NewActiveRun`-Boundary löscht ihn
zusätzlich (siehe Lifecycle-Tests unten).

**Einziger Akkumulator (R7):** Heiz-Guthaben aufgebaut, kurze (nicht
bestätigte) Cool-Gegenanforderung lädt kein zweites Guthaben und wird nach
Abbruch der Bestätigung spurlos verworfen; bestätigter Wechsel verwirft das
Heiz-Restguthaben; spiegelbildlicher Test für Cool -> Heat.

**Fenster-/Impulsplatzierung (R8):** Quote `0`, kleine Quote, normale Quote,
volle Quote; Akkumulator unterhalb/exakt auf/oberhalb der Schwelle;
Akkumulator-Obergrenze; Grenzfälle der Rundung
(`requestedOnMillisExact` exakt auf `minimumOnMillis`, exakt `.5ms` über
einer ganzen Millisekunde); neue Quote mitten im Fenster wirkt erst im
nächsten Fenster; sofort wirkende Ereignisse (Rang 1–6) unterbrechen ein
laufendes Fenster sofort.

**Mindestzeiten/Totzeit (R8/R9):** Mindest-Einschaltzeit hält aktive Richtung
trotz neuer `Idle`-Anforderung; Mindest-Auszeit verhindert verfrühte erneute
Freigabe; Polaritätstotzeit: späteres Ende von Mindest-Auszeit/Totzeit greift
(beide Richtungen der Ungleichheit sowie der exakte Gleichstand-Grenzfall);
Heizen -> Kühlen und Kühlen -> Heizen symmetrisch; niemals gleichzeitig
`Forward` und `Reverse` über eine lange, parametrisierte Sequenz.

**Parameterklassifikation (R9):** vollständige Tabelle aus Abschnitt 8.2
Rang 2 (alle Felder `0` -> `Unconfigured`; jede einzelne strukturell
unmögliche Relation -> `Invalid`; vollständig konsistente Testwerte ->
`Valid`); `ActuatorPlanStatus`/`Reason` korrekt für `Unconfigured` und
`Invalid`.

**Zeit-/Overflow-Verträge (R12):** Gleichheit an jeder Frist; knapp
davor/genau darauf/knapp danach für Mindest-Ein-/Auszeit, Totzeit,
Bestätigungsdauer, Watchdog, Fan-Nachlauf; Retrograde-Zeit an jeder
Referenzzeit (-> `TimeInvalid`); Werte nahe `UINT64_MAX` für
`nowMonotonicMillis` und Referenzfelder ohne Absturz/Wrap-Fehlverhalten;
Fensterwechsel nach einer sehr langen simulierten Pause (Iterationsobergrenze
aus 8.1 greift, kein Hänger).

**Priorität/Fail-closed (R12):** malformed `ControlRequest` (Sequenz `0` bei
vorhandener Request, unbekannte `AbstractControlDirection`, `timeQuote`
`NaN`/`Infinity`/außerhalb `[0,1]`, Status/Request-Mismatch laut #22-Matrix,
ungültiger `currentCanonicalContext`) -> `MalformedEvaluation`, sofortiger
Verwurf; mehrere gleichzeitig zutreffende Bedingungen (z. B. Safety
`ImmediateStop` **und** Kontext-Stale gleichzeitig) -> Rang-1-Reihenfolge aus
8.2 wird exakt eingehalten.

**Lifecycle/Stop (R6):** für jede der sieben `TemperatureControlLifecycleBoundary`-
Werte: `forceStop()` liefert korrekten Nachlauf, verwirft Akkumulator/
Gegenrichtungskandidat, hinterlässt `Idle`/kein `acceptedCommand`;
insbesondere „aktives Peltier -> `Fault`“ und „aktives Peltier -> `Standby`“
mit weiterlaufendem Außenlüfter-Nachlauf statt abruptem Fan-Stopp;
`NewActiveRun` löscht zusätzlich einen latched Watchdog-Fault, alle anderen
sechs Grenzen lassen ihn unverändert bestehen.

**Sequenzhochwasserzeichen:** `lastAcceptedSequence` bleibt über
`forceStop()` innerhalb derselben `ActuatorPlanner`-Instanz erhalten (kein
Replay-Schutzverlust durch einen Lifecycle-Reset innerhalb derselben
Boot-Session); eine neue `ActuatorPlanner`-Instanz (simulierter Neustart)
beginnt regulär bei einer neuen, niedrigeren Sequenz, ohne dass dies
fälschlich als persistierter Zustand behauptet wird.

**Architekturnachweis:** `ActuatorPlanner` kompiliert und wird getestet ohne
jede Abhängigkeit auf `device_platform`-Sink-Header (nur `ITimeSource`-freie,
reine Werttypen) – keine direkte GPIO-/Sink-Ausgabe aus dem Kern selbst.

### 19.2 `test/test_actuator_plan_sink_driver/test_actuator_plan_sink_driver.cpp`

- korrekte Befehlsreihenfolge für Freigabe Heating, Freigabe Cooling,
  Abschalten und Richtungswechsel exakt gemäß Abschnitt 12, geprüft über
  `commandJournal()` beider Mocks;
- `simultaneousActivationObserved() == false` über eine lange, gemischte
  Sequenz von `ActuatorPlanTickResult`-Werten;
- korrekte Weitergabe von `outerFanEnabled`/`innerFanEnabled` an die jeweils
  richtige `IBinaryOutputSink`-Instanz (keine Vertauschung).

### 19.3 `test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp` (gezielte Ergänzung)

- `TemperatureControlApplicationOrchestrator::tickActuatorPlan()` leitet
  `currentCanonicalContext`/`temperatureControlledPhase` korrekt aus der
  bestehenden `resolveEffectiveControlContext()`/
  `isTemperatureControlledProcessState()`-Kette ab (keine Parallel-Ableitung);
- `complete()`/`needsRuntimeReset()` ruft `resetActuatorPlanAtBoundary()` für
  dieselbe committed Lifecycle-Grenze auf wie `resetTemperatureControlAtBoundary()`
  (ein einziger Test-Fixture-Aufruf löst nachweislich beide Resets aus, kein
  vergessener zweiter Schritt möglich);
- `feedbackForAcceptedRequest` aus einem `tickActuatorPlan()`-Aufruf landet
  unverändert im nächsten `evaluateTemperatureControl()`-Aufruf über
  `TemperatureControlEvaluationEvidence.previousControlRequestFeedback`.

Bei geänderten gemeinsamen Verträgen werden zusätzlich die direkt betroffenen
#22-Konsumententests gezielt mitgeführt, sofern die Umsetzung dort
tatsächlich etwas berührt – in diesem Plan ist das nicht vorgesehen, da #23
`temperature_control.*`/`control_context.*` nur liest, nicht ändert.

## 20. Dokumentations- und Roadmapwirkung sowie Plan-SHA-Governance (löst R14)

- `docs/ACTUATOR_TIMING.md`: nach Umsetzung Ergänzung der bereits
  akzeptierten Struktur im Abschnitt „Akzeptierte Entscheidungen“, ohne die
  dort weiterhin offenen `TBD_COMMISSIONING`-Werte zu verändern.
- Kein neues ADR erwartet: Die Modulzuordnung folgt unverändert ADR-013, die
  Zustandsautomat-Trennung unverändert ADR-014; die neuen Werttypen sind
  interne Verträge ohne Architekturentscheidung von ADR-Rang.
- `docs/THIRD_PARTY_COMPONENTS.md` bleibt unverändert (Abschnitt 14).
- **Governance dieser Revision:** Diese Revision-2-Planänderung wird als
  eigener, in sich abgeschlossener Plan-Commit committet. Erst nach diesem
  Commit ist die exakte Revision-2-Plan-SHA bekannt und wird im Draft-PR
  #105 und im `SESSION HANDOVER` ausgewiesen. `docs/ROADMAP.md` führt bereits
  seit Revision 1 keinen sich selbst referenzierenden SHA-Wert; sofern nach
  diesem Plan-Commit eine Nachführung der Roadmap auf die exakte
  Revision-2-SHA sinnvoll ist, erfolgt dies in einem **separaten, rein
  redaktionellen Metadaten-Commit** direkt im Anschluss – nicht durch eine
  dritte Planrevision. Eine reine SHA-Metadatenpflege ist keine materielle
  Planänderung und erzeugt keine neue Revision.

## 21. Offene Fragen und materielle Risiken

- **Genaue Platzierung eines Mindestimpulses innerhalb eines Fensters, das
  ihn auslöst** (an dessen eigenem Fensterstart, wie in Abschnitt 8.1
  festgelegt) ist eine bewusste, bereits getroffene Entscheidung dieser
  Revision, kein offener Punkt mehr. Ein Owner-Wunsch nach einer anderen
  Platzierung (z. B. verzögert auf das übernächste Fenster) wäre eine
  materielle Abweichung der Algorithmussemantik und würde eine neue
  Planrevision erfordern.
- **Zeitpunkt der Composition-Root-Verdrahtung** (`src/main.cpp`,
  `main/app_main.cpp`, inklusive der realen `Allowed`-Verdrahtung des
  Safety-Gates) ist bewusst außerhalb dieses Plans, da sie GPIO-/
  Hardwarezuordnung beziehungsweise Issue #24 voraussetzt; die reine
  Anwendungsschicht (Abschnitt 6–12) ist bereits jetzt ohne reale Hardware
  vollständig nativ testbar und von dieser Verzögerung unabhängig.
- **Benanntes Integrationsgate Per-Run-Parameter-Snapshot** (Abschnitt 13):
  Die konkrete Konfigurations-/Persistenzanbindung ist bewusst nicht Teil
  dieses Plans; das strukturelle #23-seitige Fundament (unveränderliche
  Parameter pro Instanz, Rekonstruktion nur an `NewActiveRun`) ist jedoch
  bereits vollständig definiert und erfordert bei Umsetzung dieses späteren
  Gates keine erneute #23-Planrevision, solange die Grundannahme
  (Rekonstruktion ausschließlich an `NewActiveRun`) nicht verletzt wird.
- **Benanntes Integrationsgate `acknowledgeWatchdogFault()`-Verdrahtung**
  (Abschnitt 8.6): Bis Issue #24 real existiert, bleibt ein ausgelöster
  Watchdog-Fault für die verbleibende Boot-Session verriegelt. Das ist
  beabsichtigt konservativ und kein #23-Mangel; #24 wird diese Methode
  aufrufen, sobald eine entsprechende Fehlerklassifikations-/
  Wiederfreigabeentscheidung existiert.
- **Kadenzanforderung an den Aufrufer** (Abschnitt 6.5): Dieser Plan setzt
  voraus, dass der Aufrufer `tickActuatorPlan()` deutlich häufiger aufruft
  als die kürzeste vorkommende geplante Ein-/Aus-Dauer. Die konkrete
  Aufrufer-Kadenz ist Teil der noch ausstehenden Composition-Root-
  Verdrahtung (außerhalb dieses Plans) und wird dort explizit dokumentiert,
  nicht hier als Zahl vorweggenommen.
