# Plan: Issue #23 – Aktorplaner, Mindestzeiten, Totzeit und Lüfterlogik

## 1. Status, Scope und Owner-Gate

- Revision: **5**. Ersetzt Revision 4 (`bf5c0973a06b518bb2c2f5e2dee95e235f4a7b37`)
  vollständig. Diese Revision ist ohne Rückgriff auf Revision 1, 2, 3 oder 4
  vollständig ausführbar und reviewbar.
- Live-Issue: #23, offen, Status `PLANNED_SPEC_PENDING`.
- Draft-PR: #105, Branch `agent/issue-23-aktorplaner-plan` -> `main`.
- Planpfad: `docs/tasks/issue-23-actuator-planner-plan.md`.
- Planbasis: `main` @ `2986dca5736a34171910c9245a3d5f43fa55da06`
  (Merge-Commit von PR #104 / Issue #22, unverändert seit Revision 1).
- Issue **#106** „Aktorplaner Per-Run-Parameter-Snapshot und
  Recovery-Bindung" wurde vor diesem Plan-Commit nochmals um I106.R1
  präzisiert: strukturelle Producer-/Schema-/Snapshot-Vorbereitung darf ohne
  Produktionswerte vor #35 erfolgen; produktive Werte-/Grenzenaktivierung und
  der Gate-Abschluss hängen zwingend von #35 ab. Issue #106 bleibt offen und
  bleibt Abhängigkeit dieses Plans für die produktive Verdrahtung (Abschnitt
  14).
- Die Umsetzung bleibt gesperrt, bis der Owner exakt diesen neuen
  Revision-5-Plan-Commit mit `PLAN APPROVED: <SHA>` freigibt.
- Diese Revision committet ausschließlich Plandokumentation. Sie implementiert
  keine Produktionslogik, keine produktiven Tests, keine Hardware-, GPIO-,
  Toolchain- oder CI-Änderung.
- Der PR bleibt Draft. Es gibt kein `Ready for review`, keinen Merge, kein
  Auto-Merge und kein Branch-Löschen. Issue #23 wird nicht geschlossen.

```text
CONTEXT_BASELINE_BRANCH: agent/issue-23-aktorplaner-plan
CONTEXT_BASELINE_SHA: 2986dca5736a34171910c9245a3d5f43fa55da06
CONTEXT_HEAD_BEFORE_REVISION: 88efb3e875718f92bcd6c9afc07389259e2f41b0 (Roadmap-Metadaten-Commit nach Revision 4)
CONTEXT_PLAN_SHA: NONE (wird nach dem Commit dieser Revision im PR/Handover
  ausgewiesen)
CONTEXT_REFRESH_MODE: FULL
CONTEXT_DELTA: Vollständiges Owner-Review von Revision 4 mit sechs neuen
  BLOCKER-Befunden (R4.1-R4.6) sowie der letzten Präzisierung I106.R1 wurde
  erhalten. Issue #106 wurde vor diesem Plan-Commit separat live aktualisiert
  (siehe PR-Body/SESSION HANDOVER). Die Befunde sind in dieser vollständigen
  Revision 5 konsistent gelöst:
  R4.1 -> `ActiveSwitchingWindow` bleibt ein reiner
    Planungssnapshot; der Runtime-State führt den tatsächlichen
    `lastAppliedDirection` sowie die letzte physische
    Deaktivierungsrichtung und -zeit. Jeder reale Übergang Active -> Idle,
    einschließlich des normalen Window-Off-Anteils, setzt diese Zeitbasis.
    Die Arming-Regel verwendet nur diesen physischen Anker; ein an
    Mindest-Auszeit gesperrter Fensterpuls wird deterministisch für dieses
    Fenster verworfen und nicht nachgeholt (Abschnitte 6.0, 8.1).
  R4.2 -> die Prioritätsleiter trennt vollständig unmittelbare
    Fail-closed-Ereignisse von normalen, nicht-faultigen Teardown-Wünschen.
    Malformed/ungültige Parameter, Safety-Unresolved/ImmediateStop,
    Watchdog-Trip/Latch, explizites NoValidRequest, stale Kontext und
    TimeInvalid schalten physisch im selben Tick aus und werden nie durch
    Minimum-On gehalten. Nur gültiges NeutralOff, AirLimitBlockedOff und ein
    bestätigter normaler Richtungswechsel dürfen dem explizit beschriebenen
    Minimum-On-Vertrag folgen (Abschnitt 8.2, 17).
  R4.3 -> eine unbestätigte Gegenrichtung darf das alte Fenster nur bis zu
    dessen bereits eingefrorenem Ende ausführen. An Fenstergrenzen wird kein
    neues altes Richtungsfenster aus der Gegenrequest erzeugt; B sammelt kein
    Guthaben. Die Bestätigung kann ohne alte Energie über mehrere Grenzen
    weiterlaufen und plant B danach aus der dann aktuellen Request neu
    (Abschnitt 8.5).
  R4.4 -> das Feedback-Handoff gehört der
    `TemperatureControlApplicationOrchestrator`-Grenze. Ein internes,
    revisionsbehaftetes Update wird dort gespeichert; `evaluate...()` nimmt
    den zuletzt gültigen Wert genau einmal, löscht ihn vor dem #22-Aufruf und
    injiziert kein caller-supplied Feedbackfeld mehr (Abschnitte 6.1, 9, 11).
  R4.5 -> eine malformed Evaluation mit nicht vertrauenswürdiger Identität
    löscht das Handoff fail-closed auf `nullopt`; sie erzeugt kein
    Rejected mit unsicherer Sequenz und reicht kein älteres Subjekt weiter
    (Abschnitt 6.2, 9).
  R4.6 -> Außenlüfterzustand und Nachlauf hängen ausschließlich am physischen
    Peltierausgang. Der Nachlauf startet bei jedem realen Active -> Idle, auch
    beim normalen Window-Off; ein neuer Puls hebt ihn ohne Unterbrechung auf
    (Abschnitt 10, 12).
  I106.R1 -> Issue #106 trennt strukturelle Producer-/Schema-/Snapshot-
    Vorbereitung ohne Produktionswerte von der produktiven Aktivierung und
    dem Gate-Abschluss, die zwingend auf #35-Werte/Grenzen warten.
SOURCE_OF_TRUTH_CONFLICT: NONE.
```

## 2. Ziel, Reihenfolge und Nicht-Ziele

Issue #23 liefert einen deterministischen, hardwarefreien und nativ testbaren
Aktorplaner, der eine gültige abstrakte `ControlRequest` aus Issue #22 in
zeitlich korrekte, abstrakte Aktorbefehle für Peltier und Lüfter übersetzt:

- gemeinsames zeitproportionales Schaltfenster für Heizen und Kühlen;
- begrenzter, einzelner, richtungsgebundener Impulsakkumulator für
  Anforderungen unterhalb der Mindest-Einschaltzeit;
- Mindest-Einschaltzeit mit Vorrang für unmittelbare Sicherheits-/Fehlerabschaltung;
- Mindest-Auszeit nach **jedem** physischen Peltier-AUS vor erneuter Freigabe;
- bestätigte Gegenrichtungsanforderung (Schwelle + Dauer + Plausibilität) vor
  Richtungswechsel;
- sichere Polaritätstotzeit, kombiniert mit Mindest-Auszeit über
  Späteres-Ende-Regel (keine Addition), ausschließlich bei tatsächlichem
  Richtungswechsel (nicht bei gleichgerichtetem Neustart);
- Außenlüfter ohne absichtlichen Vorlauf, mit zwingendem Nachlauf;
- physisch gestarteter Außenlüfter-Nachlauf auch beim normalen Off-Anteil eines
  Schaltfensters;
- Innenlüfter während temperaturgeregelter Phasen und mit eigenem,
  konfigurierbarem (nicht zwingendem) Nachlauf;
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
  ein benanntes, blockierendes Integrationsgate über Issue **#106**
  (Abschnitt 14).
- Keine Änderung an `docs/tasks/issue-22-pi-control-air-limits-plan.md`;
  dessen Verträge werden referenziert, nicht kopiert oder neu definiert.
- Keine Änderung an der Live-Beschreibung von Issue #106 als Teil *dieses*
  Plan-Commits; die Aktualisierung von Issue #106 ist ein separater,
  vorgelagerter Governance-Schritt (Abschnitt 20) und kein Bestandteil
  dieses Dokuments.

## 3. Verbindliche Quellen und bereits getroffene Entscheidungen

Vor der Umsetzung sind mindestens diese Quellen erneut gegen ihren dann
aktuellen Stand zu prüfen:

- `docs/SPECIFICATION_REVIEW.md` als Dokumentationspriorität;
- `docs/DECISIONS.md`, insbesondere ADR-013 (Modularchitektur) und ADR-014
  (deterministischer fachlicher Zustandsautomat);
- `docs/AGENT_WORKFLOW.md` und `docs/ENGINEERING_PRINCIPLES.md`;
- `docs/ACTUATOR_TIMING.md` als kanonische Spezifikation für Schaltfenster,
  Impulsakkumulator, Mindestzeiten, Richtungswechsel, Totzeit,
  Integratorkopplung, Außen-/Innenlüfter und Watchdog – insbesondere Zeile
  183-221 (Außenlüfter, zwingender Nachlauf mit firmwarefesten Grenzen) und
  Zeile 223-244 (Innenlüfter, konfigurierbarer, nicht als zwingend
  bezeichneter Nachlauf);
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
- **Issue #106** („Aktorplaner Per-Run-Parameter-Snapshot und
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
| `TemperatureControlEvaluationEvidence` | `temperature_control_orchestrator.hpp` | bestehende öffentliche Evidence-Grenze für Zeit/Sensoren; Feedback wird ausschließlich intern durch den Orchestrator injiziert |
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
    Unresolved,     // Default; kein #24-Urteil fuer diesen Zyklus vorhanden
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

`status` trägt strukturell **nur** einen der drei oben genannten Enumwerte.
Ein Wert außerhalb dieser Menge (z. B. durch fehlerhaftes Casting) ist
in Abschnitt 8.2 Klasse I-1 (`MalformedInput`) fail-closed abgefangen, exakt wie
ein strukturell ungültiges `newEvaluation` (siehe Abschnitt 8.2 und den dort
geforderten Test mit `static_cast<ActuatorSafetyGateStatus>(0xFF)`).

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
    AcceptedControlCommand, ActiveSwitchingWindow, PulseAccumulator,
    PendingControlRequestFeedback, ActuatorPlanTickInput,
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
      ab (keine Parallel-Ableitung). Interne Aufrufsequenz gemäß Abschnitt
      6.1 (genau ein planner.tick(), genau ein driver.apply()).
    - evaluateTemperatureControl() nimmt das interne, einmalige Handoff und
      injiziert es in den privaten TemperatureControlInput; die öffentliche
      TemperatureControlEvaluationEvidence enthält kein caller-supplied
      previousControlRequestFeedback mehr.
    - complete()/needsRuntimeReset() ruft für dieselbe erfolgreiche
      Lifecycle-/Commit-Grenze zusätzlich den Aktorplaner-Stop-Pfad auf
      (Abschnitt 11), der auch den internen Handoff-Slot leert; keine zweite
      Caller-Pflicht.
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

// Am Fensterstart genau einmal erzeugter, unveraenderlicher Planungssnapshot.
// Er beschreibt nur die natuerliche Pulsplatzierung dieses Fensters; er
// behauptet weder eine physische Freigabe noch einen laufenden Nachlauf.
struct ActiveSwitchingWindow {
    std::uint64_t startMonotonicMillis{0U};
    AbstractControlDirection direction{AbstractControlDirection::Idle};
    std::uint64_t scheduledOnMillis{0U};
};

// Getrenntes Feedback-Handoff: Das Subjekt, fuer das
// #23 als naechstes ein PreviousControlRequestFeedback an #22 liefert, ist
// NICHT zwangslaeufig identisch mit dem aktuell physisch massgeblichen
// acceptedCommand. Siehe Abschnitt 9 fuer die vollstaendige Update-Regel.
struct PendingControlRequestFeedback {
    std::uint64_t sequence{0U};
    PreviousControlRequestFeedback::Disposition disposition{
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
};

struct PendingControlRequestFeedbackUpdate {
    bool changed{false};
    std::optional<PreviousControlRequestFeedback> feedback;
};

struct ActuatorPlannerRuntimeState {
    std::optional<AcceptedControlCommand> acceptedCommand;
    std::optional<ActiveSwitchingWindow> activeWindow;
    PulseAccumulator accumulator;

    // Tatsächlicher physischer Ausgang, getrennt vom Planungs-Snapshot.
    // Ein Window-Off-Anteil setzt diesen Wert auf Idle, ohne
    // activeWindow zwingend zu löschen.
    AbstractControlDirection lastAppliedDirection{
        AbstractControlDirection::Idle};

    // Beginn der aktuellen ununterbrochenen physischen Einschaltphase;
    // nullopt, sobald lastAppliedDirection Idle ist. Alleinige Zeitbasis
    // fuer die normale Mindest-Einschaltzeit.
    std::optional<std::uint64_t> currentOnPhaseStartedAtMonotonicMillis;

    // Letzte physisch deaktivierte Richtung und ihr Zeitpunkt. Diese Felder
    // werden bei JEDEM tatsächlichen Active -> Idle gesetzt, auch beim
    // normalen Window-Off-Anteil, bei forceStop() und bei Fail-closed.
    // Sie werden nie gelöscht und sind die alleinige Mindest-Auszeit-/
    // Totzeit-Zeitbasis.
    std::optional<AbstractControlDirection>
        lastPhysicalDeactivationDirection;
    std::optional<std::uint64_t>
        lastPhysicalDeactivationAtMonotonicMillis;

    std::optional<AbstractControlDirection> counterDirectionCandidate;
    std::uint64_t counterDirectionObservedSinceMonotonicMillis{0U};
    bool counterDirectionConfirmed{false};

    std::optional<std::uint64_t> lastAcceptedSequence;
    std::optional<std::uint64_t> lastNewRequestAcceptedAtMonotonicMillis;
    std::optional<ActuatorWatchdogFaultEvidence> latchedWatchdogFault;

    std::optional<PendingControlRequestFeedback> pendingFeedback;
    bool pendingFeedbackUpdateAvailable{false};

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
    std::optional<std::uint64_t> acceptedCommandSequence;
    bool watchdogFaultActive{false};
};

class ActuatorPlanner {
   public:
    explicit ActuatorPlanner(ActuatorPlannerParameters parameters);

    [[nodiscard]] ActuatorPlanTickResult tick(const ActuatorPlanTickInput& input);
    [[nodiscard]] ActuatorPlanTickResult forceStop(std::uint64_t nowMonotonicMillis);
    [[nodiscard]] PendingControlRequestFeedbackUpdate takeFeedbackUpdate();
    void applyExternalWatchdogFaultReset(std::uint64_t nowMonotonicMillis);

    [[nodiscard]] const ActuatorPlannerRuntimeState& state() const;
    [[nodiscard]] const ActuatorPlannerParameters& parameters() const;

   private:
    [[nodiscard]] AbstractControlDirection plannedDirection() const;
    [[nodiscard]] AbstractControlDirection physicalDirection() const;

    ActuatorPlannerParameters parameters_;
    ActuatorPlannerRuntimeState state_;
};
```

`PulseAccumulator`, `ActuatorDemandClass`, `ActuatorSafetyGateInput`,
`ActuatorWatchdogFaultEvidence`, `ActuatorPlannerParameters`,
`ActuatorPlanStatus`, `ActuatorPlanReason` und `ActuatorAdmissionOutcome`
sind in den Abschnitten 4.2, 7, 8.2, 8.4, 9.3 und 13 definiert, auf die
dieser Abschnitt verweist, statt sie zu duplizieren.

**Normative Feldtabelle** (mechanischer Prüfnachweis: für jedes Feld ist
genau definiert, wer es setzt, wer es löscht und ob es `forceStop()`
überlebt – schließt die Klasse von Fehlern aus, die in früheren Revisionen
durch stillschweigend inkonsistente Feld-Lebenszyklen entstand):

| Feld | Gesetzt von | Gelöscht/verändert von | Überlebt `forceStop()`? |
|---|---|---|---|
| `acceptedCommand` | Phase A, jede erfolgreiche aktive Admission | jeder unmittelbare Fail-closed-Verwurf, tatsächlicher normaler Teardown und `forceStop()` | Nein |
| `activeWindow` | Fensterstart bzw. bestätigte neue B-Planung (8.1/8.5) | unmittelbarer Fail-closed-Verwurf, tatsächlicher normaler Teardown, unbestätigte Gegenrichtung am alten Fensterende, `forceStop()` | Nein |
| `accumulator` | Fensterstart-Ereignis (8.1) | unmittelbarer Fail-closed-Verwurf, tatsächlicher Teardown, `forceStop()`; keine Gutschrift für einen am Arming-Gate verpassten Puls | Nein |
| `lastAppliedDirection` | jede physische Ausgangsentscheidung | nächste physische Ausgangsentscheidung | Ja als RAM-Zustand; bei `forceStop()` auf `Idle` gesetzt |
| `currentOnPhaseStartedAtMonotonicMillis` | Tick, an dem `lastAppliedDirection` Idle->Heating/Cooling wechselt | Tick, an dem der physische Ausgang Idle wird | Nein |
| `lastPhysicalDeactivationDirection` / `lastPhysicalDeactivationAtMonotonicMillis` | jeder tatsächliche Active -> Idle-Übergang, einschließlich normalem Window-Off, Fail-closed und `forceStop()` | nie gelöscht, nur beim nächsten tatsächlichen Übergang überschrieben | Ja |
| `counterDirectionCandidate` / `...ObservedSinceMonotonicMillis` / `counterDirectionConfirmed` | 8.5 bei gültiger, aktueller Gegenrequest | Unterbrechung, erfolgreiche B-Übernahme, Fail-closed und `forceStop()` | Nein |
| `lastAcceptedSequence` | Phase A, jede erfolgreiche Admission (6.2 Schritt 4 „sonst") | nie gelöscht (Hochwasserzeichen) | Ja |
| `lastNewRequestAcceptedAtMonotonicMillis` | Phase A, jede erfolgreich verarbeitete Evaluation (6.2 Schritt 3/4) | nie gelöscht | Ja |
| `latchedWatchdogFault` | Watchdog-Trip (Klasse I-5) | ausschließlich `applyExternalWatchdogFaultReset()` | Ja |
| `pendingFeedback` | ereignisgetrieben, siehe Abschnitt 9 | ereignisgetrieben (9), unbedingt bei `forceStop()`; die Application-Grenze besitzt zusätzlich das nicht erneut auslieferbare Handoff-Slot | Nein |
| `outerFanActive`/`...DeactivationRequestedAtMonotonicMillis` | Abschnitt 10 | Abschnitt 10 | Ja (Nachlauf läuft über `forceStop()` unverändert weiter, Abschnitt 11) |
| `innerFanActive`/`...DeactivationRequestedAtMonotonicMillis` | Abschnitt 10 | Abschnitt 10 | Ja (analog) |

`plannedDirection()` ist die reine Ableitung
`activeWindow.has_value() ? activeWindow->direction :
AbstractControlDirection::Idle` und dient nur der Fenster-/Gegenrichtungs-
planung. `physicalDirection()` liest ausschließlich
`state_.lastAppliedDirection`. Diese beiden Felder sind bewusst nicht
redundant: das eine ist ein Planungssnapshot, das andere der tatsächlich
angewendete physische Ausgang.

### 6.1 Aufrufsemantik

Der Planer besitzt **einen** Tick-Einstiegspunkt (`tick()`, oben definiert).
Er wird vom Aufrufer (`TemperatureControlApplicationOrchestrator::tickActuatorPlan()`)
potenziell **häufiger** aufgerufen als #22 seine eigene, sensorgetaktete
`evaluateTemperatureControl()`-Berechnung durchführt.

`tickActuatorPlan()` folgt bei jedem Aufruf exakt dieser Reihenfolge:

```text
tickActuatorPlan(...)
  -> currentCanonicalContext/temperatureControlledPhase ableiten
     (bestehende resolveEffectiveControlContext()/
     isTemperatureControlledProcessState()-Kette, keine Parallel-Ableitung)
  -> result = planner.tick(input)          [genau einmal]
  -> driver.apply(result)                  [genau einmal, sowohl im
                                             normalen als auch im
                                             fail-closed Fall]
  -> update = planner.takeFeedbackUpdate() [genau einmal]
  -> falls update.changed: Orchestrator-internes
     pendingControlRequestFeedback = update.feedback
  -> result zurückgeben; kein Feedbackfeld muss vom Caller kopiert werden
```

Der Planer markiert eine Änderung seines `pendingFeedback` mit einer
internen Update-Revision. `takeFeedbackUpdate()` liefert höchstens einmal
pro Revision ein `PendingControlRequestFeedbackUpdate` und markiert diese
Revision als ausgeliefert. Eine unveränderte Disposition wird bei weiteren
Planner-Ticks nicht erneut in den Application-Slot kopiert; eine neue
Disposition (beispielsweise durch den Übergang von Minimum-Off zu physisch
Active) ersetzt dort den noch nicht konsumierten Wert. Ein Wechsel auf
`nullopt` ist ebenfalls ein echtes Update und schließt das Handoff.

Der Orchestrator besitzt in seiner kanonischen Application-Grenze genau einen
internen Slot für dieses Handoff. `evaluateTemperatureControl()` nimmt den
Slot vor dem Aufruf des #22-Kerns atomar aus dem Slot (bei leerem Slot
`nullopt`), injiziert diesen lokalen Wert einmalig in den internen
`TemperatureControlInput` und löscht den Slot bereits vor dem eigentlichen
#22-Aufruf. Die öffentliche
`TemperatureControlEvaluationEvidence::previousControlRequestFeedback`-
Callerpflicht entfällt; Sensor-/Zeit-Evidence bleibt caller-supplied. Mehrere
Planner-Ticks zwischen zwei #22-Evaluationen sind damit zulässig und speichern
nur den zuletzt fachlich gültigen Update-Stand. Ein zweiter
`evaluateTemperatureControl()`-Aufruf ohne neues Handoff erhält
`nullopt` und lässt #22 gemäß seinem bestehenden Vertrag konservativ
einfrieren.

**Verbindlicher Orchestrator-Vertrag:** Eine neue #22-Evaluation wird genau
einmal als `newEvaluation` an einen späteren `tickActuatorPlan()`-
Aufruf übergeben. Der Orchestrator kopiert kein Feedback zwischen Callern;
er besitzt, aktualisiert und konsumiert den Handoff selbst. Lifecycle-Reset
und fehlgeschlagenes Persistence bleiben davon getrennt: ein fehlgeschlagener
Commit konsumiert oder löscht kein Handoff, eine erfolgreiche kanonische
Lifecycle-Grenze löscht es zusammen mit dem Planner-Stop.

Intern läuft `tick()` in **zwei Phasen**, die absichtlich unabhängig
voneinander sind:

### 6.2 Phase A – Annahme (Admission), immer zuerst, unabhängig von Parametern/Safety/Fault

Diese Phase aktualisiert ausschließlich Buchführung (Replay-Schutz,
Watchdog-Lebenszeichen, Feedback-Subjekt) und liefert `admissionOutcome`
plus – bei Erfolg – einen „vorgemerkten Kandidaten" für Phase B. Sie läuft
**immer**, auch wenn Parameter ungültig sind, Safety nicht `Allowed` ist oder
ein Watchdog-Fault latched ist – denn ein strukturell gültiges neues
#22-Ergebnis ist ein Lebenszeichen von #22, unabhängig davon, ob #23 es
gerade physisch umsetzen darf.

1. `input.newEvaluation == std::nullopt` -> `admissionOutcome = NoCandidate`,
   kein vorgemerkter Kandidat, keine `pendingFeedback`-Änderung. **Phase A
   endet hier.**
2. `newEvaluation` strukturell ungültig (unbekannter Enumwert bei Richtung
   oder Status, `timeQuote` nicht-finit oder außerhalb `[0,1]` bei
   vorhandener `ControlRequest`, Status/`ControlRequest`-Präsenz widerspricht
   der #22-Matrix aus Abschnitt 7, `sequence == 0` bei vorhandener
   `ControlRequest`, strukturell ungültiger `currentCanonicalContext`, oder
   `input.safetyGate.status` ist keiner der drei bekannten
   `ActuatorSafetyGateStatus`-Werte) -> `admissionOutcome =
   MalformedCandidate`. **Phase A endet hier**, kein vorgemerkter Kandidat.
   Weil die aktuelle Request-Identität bei einem malformed Ergebnis nicht
   vertrauenswürdig ist, wird das Planner-Handoff fail-closed auf
   `pendingFeedback = std::nullopt` gesetzt und als `changed`-Update
   markiert. Es wird niemals eine unsichere Sequence für `Rejected`
   verwendet (Abschnitt 9).
3. `classifyActuatorDemand(newEvaluation.value())` (Abschnitt 7) ==
   `NoValidRequest` (also `Unavailable`/`InvalidInput` von #22, keine
   `ControlRequest` vorhanden): `admissionOutcome = Accepted`,
   `state_.lastNewRequestAcceptedAtMonotonicMillis = input.nowMonotonicMillis`
   (Lebenszeichen, unabhängig von einer Sequenz – es gibt hier keine).
   `state_.pendingFeedback = std::nullopt` (kein Feedbackfenster, Abschnitt
   9). Der vorgemerkte Kandidat trägt `demandClass = NoValidRequest` und
   **kein** `AcceptedControlCommand` (keine Richtung, keine Sequenz).
4. Andernfalls trägt `newEvaluation` eine `ControlRequest`
   (`NeutralOff`/`AirLimitBlockedOff`/`AirLimitReducedDemand`/`NormalDemand`).
   Geprüft wird der Reihe nach:
   - `identity.sequence <= state().lastAcceptedSequence.value_or(0)` ->
     `admissionOutcome = DuplicateOrOldSequence`. **Phase A endet hier**,
     kein vorgemerkter Kandidat, keine Bookkeeping-Änderung, `pendingFeedback`
     bleibt unverändert (ein Replay darf ein bereits korrektes
     Feedbackfenster nicht zurückdrehen, Abschnitt 9).
   - Andernfalls ist dies eine **neu beobachtete** aktive Request; ab hier
     wird `state_.lastAcceptedSequence = identity.sequence` bereits gesetzt
     (Hochwasserzeichen unabhängig vom weiteren Ausgang), und, falls
     `demandClass` Heating/Cooling trägt (`NormalDemand`/
     `AirLimitReducedDemand`), `state_.pendingFeedback = {identity.sequence,
     Rejected}` als **vorläufiger** Wert gesetzt (Abschnitt 9). Danach:
     - `deadlineReached(input.nowMonotonicMillis,
       identity.createdAtMonotonicMillis, requestWatchdogMillis)` (Abschnitt
       8.3) -> `admissionOutcome = StaleOnArrivalWatchdog`. **Phase A endet
       hier**, kein vorgemerkter Kandidat; `pendingFeedback` bleibt bei
       `{sequence, Rejected}`, `acceptedCommand` bleibt
       unverändert.
     - `context != input.currentCanonicalContext` ->
       `admissionOutcome = StaleOnArrivalContext`. Identisch zum vorigen
       Fall: `pendingFeedback = {sequence, Rejected}` bleibt bestehen,
       `acceptedCommand` unverändert.
     - trägt `demandClass` kein Heating/Cooling (`NeutralOff`/
       `AirLimitBlockedOff`): `admissionOutcome = Accepted`,
       `state_.pendingFeedback = std::nullopt` (OFF öffnet kein
       Feedbackfenster, Abschnitt 9). Der vorgemerkte Kandidat trägt
       `demandClass`, `direction = Idle`, `sequence` und
       `context`.
     - sonst (Heating/Cooling, alle Prüfungen bestanden):
       `admissionOutcome = Accepted`. Der vorgemerkte Kandidat trägt
       `demandClass`, `direction`, `timeQuote`, `sequence` und
       `context`;
       `pendingFeedback` bleibt vorläufig bei `{sequence, Rejected}`
       und wird ab dem Moment, in dem Phase B diesen Kandidaten zu
       `acceptedCommand` macht, jeden Tick live nachgeführt (Abschnitt 9).

Eine bei Admission abgelehnte neue Request (`DuplicateOrOldSequence`,
`StaleOnArrivalWatchdog`, `StaleOnArrivalContext`, `MalformedCandidate`)
berührt einen bereits gehaltenen `acceptedCommand` **nicht**, sofern es sich
um Replay oder eine vertrauenswürdig identifizierte, bei Ankunft stale gewordene
Request handelt: Phase B bewertet die bestehende Zeitbasis dann wie bei
`newEvaluation == std::nullopt`. Das schließt aus, dass ein Replay oder eine
verspätete Request einen laufenden Plan zerstört. Ein `MalformedCandidate`
ist die notwendige Ausnahme: Seine Identität ist nicht vertrauenswürdig, deshalb
schließt Phase A das Handoff und Phase B führt den unbedingten
Malformed-Fail-closed-Verwurf aus. `StaleOnArrivalWatchdog` und
`StaleOnArrivalContext` dürfen dagegen ein eigenständiges
`{sequence, Rejected}` für ihre vertrauenswürdig bekannte Sequence führen
(Abschnitt 9).

### 6.3 Phase B – Physischer Ausgang (Prioritätsleiter, Abschnitt 8.2)

Phase B verwendet den in Phase A ermittelten vorgemerkten Kandidaten (falls
vorhanden) zusammen mit dem laufenden `ActuatorPlannerRuntimeState`, um genau
eine physische Ausgangsentscheidung für diesen Tick zu treffen. Sie ist in
Abschnitt 8.2 vollständig definiert und aktualisiert am Ende jedes Ticks
`pendingFeedback` sowie dessen einmaliges Update-Signal gemäß Abschnitt 9.

### 6.4 Laufender Watchdog

Bei **jedem** Tick, unabhängig davon, ob Phase A einen Kandidaten
verarbeitet hat, prüft Phase B:

```text
deadlineReached(nowMonotonicMillis, lastNewRequestAcceptedAtMonotonicMillis, requestWatchdogMillis)
```

Da `lastNewRequestAcceptedAtMonotonicMillis` in Phase A bereits durch **jede**
erfolgreich angenommene neue Evaluation aktualisiert wird (auch durch eine
`NoValidRequest`-Evaluation, siehe 6.2 Schritt 3), ist der Watchdog
unabhängig von der #22-eigenen Kadenz und von der Planner-Tick-Kadenz: Läuft
#22 seltener als der Planer tickt, bleibt der Watchdog bis zum konfigurierten
Zeitraum ruhig; läuft #22 (theoretisch) so schnell, dass Sequenzen dupliziert
ankommen, werden diese in Phase A abgelehnt, ohne den Watchdog oder die
laufende Zeitbasis zu stören.

### 6.5 Welche Quote ein Fenster bestimmt / neue Quote mitten im Fenster

Siehe Abschnitt 8.1. Kurzfassung: `ActiveSwitchingWindow` wird bei jedem
zulässigen Fensterstart-Ereignis genau einmal aus der dann aktuellen Request
erzeugt und ist für die Dauer dieses Fensters unveränderlich. Eine neue
Request mit unveränderter Richtung wirkt erst im nächsten Fenster. Eine
unbestätigte Gegenrichtung ist davon ausdrücklich ausgenommen: Das bereits
laufende alte Fenster darf seinen eingefrorenen Puls beenden, aber an dessen
Fensterende wird kein Folgefenster aus der Gegenrequest erzeugt. Danach bleibt
der physische Ausgang Idle, bis die Gegenrequest bestätigt und neu geplant
wurde. Unbedingte Fail-closed-Ereignisse wirken dagegen sofort auf
`activeWindow` und den physischen Ausgang.

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
    NoValidRequest,        // Unavailable oder InvalidInput - keine ControlRequest
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
| `NoValidRequest` | kein `AcceptedControlCommand`; Abschnitt 8.2 Klasse I-6 | siehe Abschnitt 8.4 (Verwurf bei Übergang nach Idle) |
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
Mindest-Einschaltzeit-Haltelogik, Abschnitt 8.2 Klasse N); der Unterschied
wirkt ausschließlich auf den Akkumulator.

## 8. Verhalten im Detail: Fenster, Prioritätsleiter, Zeitarithmetik

### 8.1 Fenster- und Impulsplatzierung

**Zwei getrennte Wahrheiten.** `ActiveSwitchingWindow` ist ausschließlich
ein unveränderlicher Planungssnapshot. `lastAppliedDirection` ist der
physische Ausgang. Daher kann ein Window-Snapshot während seines Off-Anteils
weiterbestehen, obwohl der physische Ausgang bereits Idle ist und die
Mindest-Auszeit seit dem realen Abschalten läuft.

**Physische Übergänge und Zeitbasis.** Jede Ausgabeentscheidung wird über eine
zentrale Übergangsfunktion angewendet:

- Active -> Idle setzt genau einmal
  `lastPhysicalDeactivationDirection` und
  `lastPhysicalDeactivationAtMonotonicMillis = now`. Das gilt für
  einen normalen Window-Off-Anteil, einen normalen Stop, einen bestätigten
  Richtungswechsel, `forceStop()` und jede unmittelbare Fail-closed-
  Abschaltung.
- Idle -> Heating/Cooling setzt
  `currentOnPhaseStartedAtMonotonicMillis = now` und hebt einen
  laufenden Außenlüfter-Nachlauf ohne Unterbrechung auf.
- Active -> Gegenrichtung ist im Planner verboten; zwischen beiden Richtungen
  liegt immer ein physischer Idle-Tick. Ein bereits physisch Idle gewordener
  Window-Snapshot ist deshalb kein aktives Peltier.
- Bleibt der physische Ausgang Idle, wird der Deaktivierungsanker nicht bei
  jedem weiteren Idle-Tick neu datiert. Dadurch misst `minimumOffMillis`
  die reale Auszeit und nicht die Dauer der anschließenden Sperrphase.

**Arming-Regel (physische Freigabefähigkeit einer Richtung).** Eine Richtung
`D` darf nur dann physisch von Idle aus freigegeben werden, wenn alle
unmittelbaren Fail-closed-Bedingungen aus Abschnitt 8.2 nicht zutreffen und
eine der folgenden Bedingungen auf dem physischen Deaktivierungsanker erfüllt
ist:

- **(a) Erststart seit Konstruktion:** `lastPhysicalDeactivationAtMonotonicMillis
  == std::nullopt`. Es gab seit Konstruktion noch keinen Active -> Idle-
  Übergang.
- **(b) Gleichgerichteter Neustart:** `lastPhysicalDeactivationDirection
  == D` und
  `deadlineReached(at, lastPhysicalDeactivationAt, minimumOffMillis)`.
  Eine Totzeit ist bei gleichgerichtetem Neustart nicht erforderlich.
- **(c) Richtungswechsel:** `lastPhysicalDeactivationDirection != D` und
  sowohl `deadlineReached(at, lastPhysicalDeactivationAt, minimumOffMillis)`
  als auch `deadlineReached(at, lastPhysicalDeactivationAt,
  polarityDeadTimeMillis)`. Die Fristen laufen parallel ab demselben
  realen Abschaltzeitpunkt; es gilt das spätere Ende, nicht eine Addition.

Die Regel unterscheidet damit Erststart, gleichgerichtete Wiederfreigabe und
Richtungswechsel unabhängig davon, ob ein `activeWindow`-Snapshot noch
existiert.

**Fensterstart und Quote.** Ein Fensterstart-Ereignis ist der Erststart, der
reguläre Beginn eines Folgefensters derselben weiterhin gültigen Richtung oder
die bestätigte, gegatete Neuanlage eines B-Fensters. Die Quote wird nur an
diesem Ereignis aus der dann aktuellen, fachlich gültigen
`acceptedCommand` gelesen:

```text
requestedOnMillisExact = clamp(timeQuote, 0.0, 1.0) * switchingWindowMillis
```

- `requestedOnMillisExact >= minimumOnMillis`:
  `scheduledOnMillis = min(round_half_up(requestedOnMillisExact),
  switchingWindowMillis)`, Reason `ScheduledWithinWindow`. Der
  Akkumulator wird an diesem Fensterstart nicht zusätzlich gefüttert.
- `0 < requestedOnMillisExact < minimumOnMillis`:
  die ungerundete Quote wird an den einzigen richtungsgebundenen Akkumulator
  gutgeschrieben, maximal bis `pulseAccumulatorCapMillis`. Erreicht
  das Guthaben `minimumOnMillis`, wird ein vollständiger
  `minimumOnMillis`-Puls geplant und der Rest abgezogen; andernfalls
  bleibt `scheduledOnMillis = 0`.
- `requestedOnMillisExact == 0`: kein Peltierpuls; der Reason ist
  `NeutralIdle` beziehungsweise `AirLimitBlocked`.

**Deterministische Impulsplatzierung bei Mindest-Auszeit/Totzeit.** Der
geplante Puls hat seine natürliche, am Fensterstart eingefrorene Lage
`[window.startMonotonicMillis,
window.startMonotonicMillis + scheduledOnMillis)`. Er wird nicht hinter
das Fensterende verschoben und nicht in ein Folgefenster nachgeholt. Für ein
nichtnulliges `scheduledOnMillis` gilt:

1. Beim Fensterstart wird die Arming-Regel am exakten
   `window.startMonotonicMillis` gegen den letzten physischen
   Deaktivierungsanker geprüft.
2. Ist die Richtung zu diesem Zeitpunkt wegen `minimumOffMillis` oder
   `polarityDeadTimeMillis` gesperrt, wird der gesamte Puls dieses
   Fensters als `DeferredOrLimited` verworfen; die Quote wird weder
   nachträglich verschoben noch als neues Akkumulatorguthaben blind in das
   nächste Fenster übertragen.
3. Ist sie am Fensterstart zulässig, darf der erste Planner-Tick innerhalb des
   natürlichen On-Intervalls den physischen Puls starten. Bleibt die
   Aufruferkadenz hinter dem Intervall zurück, wird kein verspäteter Puls
   außerhalb dieses Intervalls begonnen. Ein physischer Puls endet am
   natürlichen Intervallende, sofern keine unmittelbare Fail-closed-Abschaltung
   ihn früher beendet; ein normaler Teardown darf ihn nur bis zum Ende der
   konkreten Mindest-Einschaltphase halten.
4. Ist das Arming-Gate exakt am Fensterstart erfüllt, ist der Puls zulässig
   (Gleichheitsgrenze); ist es erst danach erfüllt, bleibt dieses Fenster
   verworfen. Damit sind vor, auf und nach dem Minimum-Off-Ende sowie kurze und
   lange Off-Anteile ohne Nachholung unterscheidbar testbar.

Diese feste Lage ist die gewählte Revision-5-Regel. Sie ist konservativer als
ein verschobener Puls, verhindert aber jede stille Energieerzeugung aus einer
Sperrphase und ist O(1) sowie nativ deterministisch.

**Fensterfortschritt (overflow-sicher, O(1)).** Solange ein
`activeWindow` besteht, verwendet kein Schritt eine ungeprüfte
Deadline-Addition:

```cpp
if (now < activeWindow->startMonotonicMillis) {
    // Retrograde -> TimeInvalid
}
elapsed = now - activeWindow->startMonotonicMillis;
if (elapsed >= switchingWindowMillis) {
    windowsElapsed = elapsed / switchingWindowMillis;
    activeWindow->startMonotonicMillis += windowsElapsed * switchingWindowMillis;
    // windowsElapsed * switchingWindowMillis <= elapsed <= now:
    // der neue Start bleibt innerhalb des gültigen Zeitraums.
}
elapsedInWindow = now - activeWindow->startMonotonicMillis;
```

Die Berechnung bleibt O(1); übersprungene Fenster werden nicht einzeln
nachgeholt. Bei einem normalen Folgefenster derselben Richtung wird genau ein
neuer Snapshot aus der damals aktuellen Request erzeugt. Wenn jedoch eine
Gegenrequest B noch unbestätigt ist, wird am Ende des alten Snapshots kein
neues altes Richtungsfenster erzeugt: der alte Snapshot wird gelöscht, der
physische Ausgang bleibt Idle, und B erhält in dieser Wartephase kein
Akkumulatorguthaben. Die Bestätigungsbuchführung darf dabei weiterlaufen;
Abschnitt 8.5 definiert die spätere B-Neuanlage.

**Physischer Wunschzustand.** Für das natürliche Fensterintervall gilt:

```text
desiredActive = activeWindow.has_value()
             && elapsedInWindow < activeWindow->scheduledOnMillis
```

`desiredActive` ist kein physischer Freigabebefehl. Der zentrale
Übergangs-/Prioritätspfad aus Abschnitt 8.2 entscheidet zusätzlich über
Mindest-On, physische Mindest-Off-/Totzeit, Fail-closed und den Außenlüfter.
### 8.2 Prioritätsleiter (Phase B)

```cpp
enum class ActuatorPlanStatus : std::uint8_t {
    Active,
    Idle,
    Unconfigured,
    InvalidInput,
};

enum class ActuatorPlanReason : std::uint8_t {
    MalformedInput,
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

Die folgende Tabelle ist bewusst in zwei Klassen getrennt. Die unmittelbare
Fail-closed-Klasse ist nicht durch die normale Mindest-Einschaltzeit
verzögerbar. Die normale Klasse betrifft ausschließlich eine gültige,
nicht-faultige Teardown- oder Freigabeentscheidung und darf nur dort
`MinimumOnTimeHeld` liefern, wo der kanonische Vertrag das ausdrücklich
zulässt.

**Klasse I – unmittelbare Fail-closed-Abschaltung:**

| Klasse | Bedingung | Status | Reason | Physisch im selben Tick | Zustandswirkung |
|---|---|---|---|---|---|
| I-1 | Phase A liefert `MalformedCandidate` oder Safety-Enum, aktueller Kontext oder ein anderer struktureller Tickwert ist ungültig | `InvalidInput` | `MalformedInput` | `Idle`, sofort | Snapshot, Akkumulator, Gegenrichtung, Request und Handoff fail-closed schließen |
| I-2a | `classifyActuatorPlannerParameters() == Unconfigured` | `Unconfigured` | `NoCommissioning` | `Idle`, sofort | dieselbe vollständige Bereinigung wie I-1 |
| I-2b | `classifyActuatorPlannerParameters() == Invalid` | `InvalidInput` | `InvalidConfiguration` | `Idle`, sofort | dieselbe vollständige Bereinigung wie I-1 |
| I-3a | `safetyGate.status == Unresolved` | `Idle` | `SafetyGateUnresolved` | `Idle`, sofort | vollständige Bereinigung; kein Minimum-On-Hold |
| I-3b | `safetyGate.status == ImmediateStop` | `Idle` | `ExternalSafetyOverride` | `Idle`, sofort | vollständige Bereinigung; kein Minimum-On-Hold |
| I-4 | `latchedWatchdogFault.has_value()` | `Idle` | `RequestWatchdogFaultLatched` | `Idle`, sofort | keine Wiederfreigabe; Latch bleibt bestehen |
| I-5 | der laufende Watchdog trippt in diesem Tick | `Idle` | `StaleRequestWatchdog` | `Idle`, sofort | Fault-Evidenz latchen und alle Plan-/Peltierzustände verwerfen |
| I-6 | eine **neue, explizite** Phase-A-Evaluation klassifiziert als `NoValidRequest` | `Idle` | `NoValidRequest` | `Idle`, sofort | aktuelle Request, Snapshot und Guthaben verwerfen; kein altes Feedbacksubjekt |
| I-7 | der gehaltene Request-Kontext ist stale/inkompatibel zum aktuellen kanonischen Kontext | `Idle` | `StaleRequestContext` | `Idle`, sofort | aktuelle Request, Snapshot und Guthaben verwerfen |
| I-8 | eine Referenzzeit ist retrograd oder anderweitig `TimeInvalid` | `InvalidInput` | `TimeInvalid` | `Idle`, sofort | vollständige Bereinigung; keine Frist wird als erfüllt angenommen |

Für **jede einzelne** I-Zeile gilt: Wenn der physische Ausgang zuvor Active
war, wird derselbe Tick als der reale Active -> Idle-Übergang behandelt und
setzt `lastPhysicalDeactivationAtMonotonicMillis`. Die Lüfterlogik aus
Abschnitt 10 läuft danach weiter. Kein I-Ereignis wird durch
`currentOnPhaseStartedAtMonotonicMillis` verzögert. Ein
Lifecycle-Stop über `forceStop()` ist ebenfalls unmittelbares
Fail-closed und nutzt denselben physischen Übergangspfad, auch wenn er
außerhalb dieser Tick-Eingabe erfolgt.

**Klasse N – normale, nicht-faultige Teardown- und Freigabewünsche:**

| Klasse | Bedingung | Status/Reason | Physisch | Zustandswirkung |
|---|---|---|---|---|
| N-1 | gültiges `NeutralOff`, gültiges `AirLimitBlockedOff` oder bestätigter normaler Richtungswechsel fordert Teardown, physischer Puls ist noch in seiner Mindest-On-Phase | `Active / MinimumOnTimeHeld` | alte Richtung bleibt bis zum Ende dieser konkreten physischen Einschaltphase | Snapshot/Request bleiben nur so lange erhalten; kein neues Guthaben |
| N-2 | derselbe normale Teardown nach erfüllter Mindest-On oder während physischem Idle | `Idle / NeutralIdle` beziehungsweise `AirLimitBlocked` | Active -> Idle, falls erforderlich | Snapshot, Request, Akkumulator und Gegenrichtung werden verworfen |
| N-3 | ein gültiger Heating/Cooling-Puls soll aus Idle beginnen, aber der physische Deaktivierungsanker sperrt ihn | `Idle / MinimumOffTimeHeld` beziehungsweise `PolarityDeadTimeHeld` | Idle | der aktuelle Fensterpuls ist nach 8.1 verworfen; kein Nachholen |
| N-4 | gültige normale Fensterauswertung ohne Teardown | `Active`/`Idle` mit `ScheduledWithinWindow`, `MinimumPulseTriggered` oder `AccumulatingBelowThreshold` | gemäß natürlichem Snapshot-Intervall | nur reguläre Fenster-/Akkumulatorfortschreibung |

N-1 ist die einzige normale Mindest-On-Halteentscheidung. Sie gilt nicht für
I-1 bis I-8. Ein Window-Off-Anteil ist kein Teardown des Planungssnapshots:
Er schaltet den physischen Ausgang regulär Idle und datiert den physischen
Mindest-Off-Anker; der Snapshot darf für ein Folgefenster bestehen bleiben.
Eine unbestätigte Gegenrichtung ist ebenfalls kein N-1-Teardown: Sie darf den
aktuellen alten Snapshot beenden, erzeugt danach aber kein neues altes Fenster
(siehe Abschnitt 8.5).

`counterDirectionConfirming` bleibt eine Zusatzauskunft über einen
laufenden, noch nicht bestätigten Kandidaten. Sie ist keine dritte
Prioritätsklasse und darf keine physische Freigabe auslösen.

`NoValidRequest` aus einem neuen #22-Ergebnis steht ausschließlich in
I-6. Ein Tick ohne neue Evaluation ist kein neues Lebenszeichen und darf eine
bestehende physische Freigabe nicht allein deshalb abschalten.

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
(`currentOnPhaseStartedAtMonotonicMillis`,
`lastPhysicalDeactivationAtMonotonicMillis`, `activeWindow->startMonotonicMillis`,
`counterDirectionObservedSinceMonotonicMillis`,
`lastNewRequestAcceptedAtMonotonicMillis`,
`outerFanDeactivationRequestedAtMonotonicMillis`/
`innerFanDeactivationRequestedAtMonotonicMillis`) auf, wird dies als
`TimeInvalid` (Klasse I-8, `InvalidInput`, unbedingter Verwurf)
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
Gegenrichtungskandidat hat **keinen** eigenen Akkumulator und akkumuliert
laut Abschnitt 8.2 (Gegenrichtungsbestätigung) zu keinem Zeitpunkt eigenes
Guthaben, solange er nicht bestätigt ist.

`accumulator` und `activeWindow` werden gemeinsam verworfen bei
jedem einzelnen unmittelbaren Fail-closed-Ereignis I-1 bis I-8, beim
tatsächlichen Vollzug eines normalen Teardowns N-2 und bei
`forceStop()`. Der normale physische Window-Off-Anteil ist kein
`activeWindow`-Teardown: Der Snapshot und sein Akkumulator bleiben
für ein zulässiges Folgefenster erhalten, während die physische
Deaktivierungszeitbasis neu gesetzt wird.

Bei einer unbestätigten Gegenrequest am alten Fensterende werden Snapshot und
altes Guthaben verworfen, aber der Gegenrichtungskandidat bleibt erhalten,
solange B gültig und ununterbrochen aktuell ist. `counterDirectionCandidate`,
`counterDirectionConfirmed` und der Beobachtungszeitpunkt werden
zusätzlich bei jeder Unterbrechung der Bestätigung, bei I-1 bis I-8, bei
erfolgreicher B-Übernahme und bei `forceStop()` verworfen. So kann die
Bestätigung über mehrere alte Fenstergrenzen weiterlaufen, ohne alte Energie
oder ein zweites Guthaben mitzunehmen.

Diese gemeinsame Verwurfsregel schließt sowohl den in `ACTUATOR_TIMING.md`
explizit genannten Fall („Verlassen der temperaturgeregelten Phase, Stop,
Fehler oder ungültiger Sensorzustand") als auch beliebig lange gewöhnliche
Neutralband-Phasen ein: Ein Impulsguthaben überlebt **keine** noch so kurze
Rückkehr nach `Idle`. Eine spätere erneute Heating-/Cooling-Anforderung
beginnt strukturell immer als „frischer Start" oder „gleichgerichteter
Neustart" (Abschnitt 8.1) und sammelt ihr Guthaben neu. Dies schließt
zugleich die Lücke, dass ein sehr altes Guthaben nach einer langen Sperr-/
Neutralphase „blind ausgeführt" werden könnte (`ACTUATOR_TIMING.md`: „Nach
längerer Sperrung wird die Regelanforderung neu berechnet; ein alter
Akkumulator wird nicht blind ausgeführt.").

Damit ist zu jedem Zeitpunkt eindeutig entweder ein leerer oder ein an genau
eine Richtung gebundener Akkumulator vorhanden; ein zweiter, „versteckter"
Akkumulator für eine unbestätigte Gegenrichtung existiert im Datenmodell
nicht.

### 8.5 Bestätigter Richtungswechsel

Eine gültige Gegenrequest B wird als Gegenrichtungskandidat geführt, wenn sie
eine andere Richtung als der aktuell geplante Snapshot oder die aktuell
physisch gehaltene Richtung fordert, `NormalDemand` oder
`AirLimitReducedDemand` ist und
`timeQuote >= counterDirectionConfirmationQuoteThreshold` gilt. Falls
`counterDirectionCandidate` leer oder eine andere Richtung ist, beginnt
die ununterbrochene Beobachtung mit
`counterDirectionObservedSinceMonotonicMillis = now`. Quote,
Kontext, Safety und Request-Identität werden in jedem Tick erneut geprüft.

Bis zur Bestätigung gilt:

- Das bereits laufende alte `activeWindow` darf nur seinen am
  Fensterstart eingefrorenen aktuellen Puls beenden. Eine am Fensterende
  eintreffende oder bereits gehaltene B-Request erzeugt **kein** neues altes
  Richtungsfenster; das alte Snapshot-/Akkumulatorguthaben wird verworfen und
  der physische Ausgang bleibt Idle.
- B sammelt in dieser Wartephase kein Akkumulatorguthaben und erhält keine
  physische Wirkung. Auch über zwei oder mehr alte Fenstergrenzen wird keine
  Quote von B als Heating-/Cooling-Energie der alten Richtung gelesen.
- Sobald der alte Snapshot gelöscht ist, bleibt der Gegenrichtungskandidat
  selbst die einzige fachliche Warteidentität: Bei den zwei möglichen
  Peltier-Richtungen ist die alte Richtung eindeutig die Gegenrichtung zu B.
  Es wird dafür weder ein zweiter alter Request-/Quote-Snapshot noch ein
  zweiter Akkumulator gespeichert.
- Die Bestätigungsbuchführung darf nach dem alten Fensterende weiterlaufen.
  Fällt B unter die Schwelle, wechselt zurück, wird `NeutralOff`/
  `AirLimitBlockedOff` oder trifft ein unmittelbares Fail-closed-
  Ereignis ein, werden Kandidat und Bestätigungsstatus sofort verworfen.

Erst wenn
`deadlineReached(now, counterDirectionObservedSinceMonotonicMillis,
counterDirectionConfirmationDurationMillis)` ununterbrochen erfüllt ist,
wird `counterDirectionConfirmed = true`. Der alte Snapshot bleibt
dabei bereits verworfen. Wenn B in diesem Moment noch die aktuelle,
vertrauenswürdige Request ist, wird sie **neu** aus genau dieser aktuellen
B-Quote geplant, sobald `minimumOffMillis` und – beim echten
Richtungswechsel – `polarityDeadTimeMillis` ab dem realen alten
Active -> Idle erfüllt sind. Ist eine dieser Fristen noch aktiv, bleibt der
Ausgang Idle; die bestätigte B-Buchführung wird nicht als Energie oder
Akkumulatorguthaben ausgeführt und wartet nur auf die nächste zulässige
Neuanlage.

Sobald B physisch übernommen und ein neues `activeWindow` erzeugt ist,
werden `counterDirectionCandidate`, `counterDirectionConfirmed`
und der Beobachtungszeitpunkt gelöscht. B wird dann als aktueller Request-
Snapshot behandelt; eine spätere Quote-Änderung wirkt erst am nächsten
Fensterstart. Eine neue, höhere B-Sequence ersetzt die fachlich aktuelle
B-Request während der Bestätigung, erzeugt aber keine alte Energie. Spiegelbildlich
gilt alles für Heating -> Cooling und Cooling -> Heating.

Vor Ablauf der Mindest-Einschaltzeit der alten physischen Richtung bleibt eine
Gegenanforderung – bestätigt oder nicht – wirkungslos auf die physische Ausgabe.
Ein unmittelbares Fail-closed-Ereignis aus Klasse I beendet die alte Richtung
  dagegen sofort; ein gültiger normaler Teardown darf sie nur nach dem
  Mindest-On-Vertrag beenden.

### 8.6 Watchdog-Fault-Evidenz

Löst der laufende Watchdog aus (Klasse I-5), wird

```cpp
state_.latchedWatchdogFault = ActuatorWatchdogFaultEvidence{
    .detectedAtMonotonicMillis = now,
    .lastAcceptedSequenceBeforeFault = state_.lastAcceptedSequence.value_or(0U),
};
```

gesetzt. Solange `latchedWatchdogFault.has_value()`, bleibt Klasse I-4 in jedem
weiteren Tick maßgeblich (`Idle`, `RequestWatchdogFaultLatched`) –
**unabhängig davon**, was danach in Phase A geschieht: Eine strukturell
einwandfreie, kontextfrische, an sich akzeptable neue `newEvaluation` wird
in Phase A weiterhin für Sequenz-/Watchdog-Bücher berücksichtigt
(`admissionOutcome` kann `Accepted` sein, `lastAcceptedSequence`/
`lastNewRequestAcceptedAtMonotonicMillis` werden aktualisiert, und –
sofern Heating/Cooling – `pendingFeedback` wird gemäß Abschnitt 9 ebenfalls
aktualisiert), öffnet aber **keine** neue physische Freigabe, solange Klasse I-4
vor den weiteren Klassen ausgewertet wird.
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

Die fachliche Grundlage ist wörtlich
`docs/tasks/issue-22-pi-control-air-limits-plan.md` Abschnitt 8.2
„Feedbackfenster": Nur eine unmittelbar vorherige aktive
`Heating`-/`Cooling`-ControlRequest öffnet ein Feedbackfenster,
vorhandenes Feedback muss exakt deren Sequence referenzieren, fehlendes oder
fremdes Feedback friert #22 konservativ ein, und eine gültige OFF-Request
benötigt kein Anti-Windup-Feedback.

### 9.1 Entkopplung von Feedback-Subjekt und physischer Governance

`pendingFeedback` ist ein von `acceptedCommand` unabhängiges Planner-
Feld. `acceptedCommand` bestimmt ausschließlich die fachliche
Planungsrequest; `pendingFeedback` bestimmt ausschließlich das Feedback-
Subjekt für #22. Das Planner-Feld ist keine Caller-API. Änderungen daran werden
als `PendingControlRequestFeedbackUpdate` mit einer einmaligen internen
Update-Revision an die Application-Grenze ausgeliefert.

Der Orchestrator besitzt dort
`std::optional<PreviousControlRequestFeedback>
pendingControlRequestFeedback_`. Nach jedem Planner-Tick übernimmt er nur ein
tatsächlich neues Update. Er ersetzt damit keinen älteren Wert durch einen
unveränderten Replay. Ein Update mit `std::nullopt` löscht den
Application-Slot.

### 9.2 Ereignisgetriebene Update-Regel

Bei jeder Phase-A-Verarbeitung einer neu beobachteten Evaluation gilt:

| Beobachtetes Ereignis | Wirkung auf Planner-`pendingFeedback` | Handoff-Update |
|---|---|---|
| `newEvaluation == std::nullopt` | unverändert | kein Update |
| `DuplicateOrOldSequence` | unverändert | kein Update |
| `MalformedCandidate` | `std::nullopt`, weil die Identität unsicher ist | geändert auf `nullopt` |
| `NoValidRequest` | `std::nullopt` | geändert auf `nullopt` |
| gültiges `NeutralOff` oder `AirLimitBlockedOff` | `std::nullopt` | geändert auf `nullopt` |
| gültige aktive Request, aber `StaleOnArrivalWatchdog` oder `StaleOnArrivalContext` | `{sequence, Rejected}` | geändert auf genau diese vertrauenswürdige Sequence |
| gültige aktive Request, Admission bestanden | zunächst `{sequence, Rejected}`, danach physisch live nachgeführt | geändert auf genau diese Sequence |

Bei jeder Änderung der Disposition wird ein Update markiert, auch wenn die
neue Disposition wieder `Rejected` lautet. Ein identischer Wert wird
nicht erneut ausgeliefert.

Wird ein vertrauenswürdig bekanntes `acceptedCommand` durch ein
unbedingtes I-Ereignis oder den tatsächlichen Vollzug eines normalen Teardowns
verworfen und verfolgt `pendingFeedback` genau dessen Sequence, wird die
Disposition `Rejected` und als Update markiert. Das gilt nicht für
eine neue OFF-/`NoValidRequest`-Evaluation, die das Feedbackfenster
bereits auf `nullopt` schließt. Ein `MalformedCandidate` hat
keine vertrauenswürdige Sequence und löscht deshalb das gesamte Handoff; ein
älteres Subjekt wird nie als vermeintliches Feedback für diese neue Evaluation
weitergereicht. Ein Replay darf kein bestehendes Handoff zurückdrehen.

### 9.3 Live-Nachführung während laufender Governance

Für ein Handoff, dessen Sequence noch dem gehaltenen aktiven Command
entspricht, wird die Disposition pro relevantem Planner-Tick aus dem
**physischen** Ausgang abgeleitet:

```text
pendingFeedback.sequence == acceptedCommand.sequence
  && physicalDirection() == acceptedCommand.direction
  && physical output is Active in that direction
      -> NoIntegratorConstraint
otherwise while acceptedCommand is an active Heating/Cooling request
      -> DeferredOrLimited
```

Damit sind Mindest-On-Hold, Mindest-Off, Polaritätstotzeit,
unbestätigte Gegenrichtungsbestätigung, Window-Off-Idle und ein verpasster
Fensterpuls konservativ `DeferredOrLimited`. Nur der tatsächlich
aktive
physische Ausgang derselben Richtung erhält `NoIntegratorConstraint`.
Eine bestätigte B-Request bleibt bis zur physischen Übernahme deferred, auch
wenn der alte Snapshot bereits beendet und mehrere Fenstergrenzen vergangen
sind.

### 9.4 Single-use-Handoff und Lifecycle

`tickActuatorPlan()` ruft Planner und Sink-Driver je einmal auf und nimmt
danach genau einmal `takeFeedbackUpdate()`. Nur die kanonische
Application-Grenze schreibt den Update-Wert in ihren internen Slot.

`evaluateTemperatureControl()` übernimmt den Slot in eine lokale
`std::optional<PreviousControlRequestFeedback>` und löscht den
internen Slot **vor** dem #22-Aufruf. Sie injiziert diese lokale Kopie genau
einmal in `TemperatureControlInput`. Die öffentliche Struktur
`TemperatureControlEvaluationEvidence` enthält danach nur Zeit- und
Sensorsignale; ein Caller kann kein alternatives Feedbacksubjekt mehr
einschleusen und muss kein Planner-Ergebnis kopieren.

Mehrere Planner-Ticks zwischen zwei #22-Aufrufen sind erlaubt: ein neues
fachlich gültiges Update ersetzt den noch nicht konsumierten Slot, ein
unverändertes Update wird nicht erneut ausgeliefert. Ein zweiter
`evaluateTemperatureControl()`-Aufruf ohne neues Update erhält
`nullopt`; #22 friert gemäß seinem Vertrag ein. Die nachfolgende neue
#22-Evaluation wird als Ergebnis an einen späteren Planner-Tick übergeben, ohne
manuelle Feedbackkopie im Caller.

An jeder erfolgreich committed Lifecycle-Grenze ruft der Orchestrator den
gemeinsamen Planner-Stop auf und löscht den Application-Slot. Ein fehlgeschlagener
Persistence-Commit verändert weder Planner noch Slot.

### 9.5 `ActuatorAdmissionOutcome` als separater Diagnosevertrag

```cpp
enum class ActuatorAdmissionOutcome : std::uint8_t {
    NoCandidate,
    Accepted,
    MalformedCandidate,
    DuplicateOrOldSequence,
    StaleOnArrivalWatchdog,
    StaleOnArrivalContext,
};
```

`ActuatorPlanTickResult::admissionOutcome` bleibt von der physischen
`reason`-Entscheidung und dem internen Feedback-Handoff getrennt.
Die drei Antworten sind: Was geschah mit dem neuen Kandidaten? Warum ist der
physische Ausgang so? Und welches Feedback liegt intern einmalig für #22
bereit?

### 9.6 n/n+1/n+2-Beispiel ohne Caller-Ritual

```text
#22 Evaluation n -> Heating Request A
  -> Caller gibt nur das erzeugte Result an tickActuatorPlan(newEvaluation)
  -> Planner erzeugt ein internes Update {A, Rejected}; Orchestrator speichert es

Mehrere Planner-Ticks ohne neue Evaluation
  -> A bleibt unverändert, Disposition kann bei realer Governance einmalig
     auf DeferredOrLimited oder NoIntegratorConstraint aktualisiert werden
  -> nur der zuletzt gültige Update-Stand bleibt im Application-Slot

evaluateTemperatureControl() für Evaluation n+1
  -> Orchestrator nimmt {A, <letzte Disposition>} einmalig und löscht den Slot
  -> #22 erhält genau A; kein Caller kopiert ein Planner-Feedbackfeld

#22 Evaluation n+1 -> Cooling Request B
  -> Ergebnis wird an einen späteren Planner-Tick gegeben
  -> B wird als aktuelle Request angenommen, aber nicht sofort als physische
     Gegenrichtung ausgeführt
  -> während mindestens zwei alten Fenstergrenzen: altes A-Fenster darf nur
     enden; kein neues A-Fenster aus B, kein B-Guthaben
  -> nach bestätigter B-Anforderung und erfüllter Arming-Regel wird B aus
     der dann aktuellen B-Quote neu geplant; bis dahin B-Feedback deferred

Malformed neue Evaluation
  -> Planner löscht das interne Handoff auf nullopt, ohne A oder eine
     unsichere B-Sequence zu behaupten
  -> der nächste #22-Aufruf erhält kein fremdes altes Feedback und friert
     gemäß seinem bestehenden Vertrag konservativ ein
```

Spiegelbildlich gilt alles für Cooling -> Heating.
## 10. Lüfterlogik

**Außenlüfter und physischer Peltierausgang.** Der Außenlüfter wird nicht aus
`activeWindow` oder `plannedDirection()` abgeleitet, sondern aus
dem tatsächlichen Peltierzustand und dessen Nachlauf:

- Beim physischen Übergang Idle -> Heating/Cooling wird
  `outerFanEnabled = true` im selben Tick vor der H-Brückenfreigabe
  ausgegeben. Es gibt keinen absichtlichen Vorlauf.
- Bei **jedem** physischen Übergang Active -> Idle wird
  `outerFanDeactivationRequestedAtMonotonicMillis = now` gesetzt.
  Das umfasst den normalen Off-Anteil eines zeitproportionalen Fensters,
  einen normalen Stop, einen bestätigten Richtungswechsel, Watchdog,
  Safety-Fail-closed und `forceStop()`.
- Während des Nachlaufs bleibt `outerFanEnabled` true, bis
  `deadlineReached(now, outerFanDeactivationRequestedAtMonotonicMillis,
  outerFanPostRunMillis)` erfüllt ist. Ein neuer physischer Puls während
  dieser Zeit löscht die Deaktivierungsdeadline und lässt den Lüfter ohne
  Unterbrechung an; der Timer wird nicht auf den späteren Window-Off-Snapshot
  verschoben.
- Ist der Nachlauf abgelaufen und der physische Peltierausgang weiterhin Idle,
  wird der Außenlüfter aus geschaltet. Ein wiederholter Idle-Tick startet den
  Nachlauf nicht neu, weil kein neuer physischer Active -> Idle-Übergang
  stattgefunden hat.
- `outerFanPostRunMillis > 0` bleibt eine strukturelle Voraussetzung
  gemäß `ACTUATOR_TIMING.md` und Abschnitt 13.

Damit laufen beispielsweise bei einem 3-s-Puls in einem 30-s-Fenster Peltier
und Außenlüfter zunächst gemeinsam; beim realen Peltier-AUS nach 3 s beginnt
der Nachlauf, obwohl `activeWindow` bis zur Fenstergrenze als
Planungssnapshot bestehen kann. Ist der Off-Anteil länger als der Nachlauf,
geht der Lüfter vor dem nächsten Puls aus; ist er kürzer, bleibt er an. Ein
neuer Puls hebt den Nachlauf auf, ohne dass der Lüfter aus- und wieder
eingeschaltet wird.

**Innenlüfter.** `innerFanEnabled = true`, solange
`input.temperatureControlledPhase == true` – unabhängig vom
Peltier-Fenster. Beim Verlassen der temperaturgeregelten Phase startet ein
eigener Nachlauf. Kurze Peltier-Off-Zeiten innerhalb derselben geregelten
Phase lösen keinen Innenlüfter-Nachlauf aus. Der Innenlüfter-Nachlauf ist
konfigurierbar, aber nicht als zwingend bezeichnet; `innerFanPostRunMillis
== 0` bleibt gemäß Abschnitt 13 zulässig.

## 11. Lifecycle-Integration und Stop-Ablauf

`resetActuatorPlanAtBoundary()` ist eine interne Hilfsfunktion, die
**ausschließlich** von
`TemperatureControlApplicationOrchestrator::complete()` aufgerufen wird –
an derselben committed Lifecycle-Grenze, mit demselben
`TemperatureControlLifecycleBoundary`-Wert und im selben Funktionsaufruf,
in dem bereits `resetTemperatureControlAtBoundary()` aufgerufen wird.

```cpp
ActuatorPlanTickResult resetActuatorPlanAtBoundary(
    ActuatorPlanner& planner, ActuatorPlanSinkDriver& driver,
    TemperatureControlLifecycleBoundary boundary,
    std::uint64_t nowMonotonicMillis,
    std::optional<PreviousControlRequestFeedback>&
        applicationPendingFeedback);
```

Ablauf (identisch für alle sieben Grenzen; **keine** davon ruft
`applyExternalWatchdogFaultReset()` auf, Abschnitt 8.6):

1. `ActuatorPlanTickResult result = planner.forceStop(nowMonotonicMillis);`
   – berechnet denselben Ergebnistyp wie `tick()`: physisch `Idle`;
   Akkumulator/`activeWindow`/Gegenrichtungskandidat/`acceptedCommand`/
   `pendingFeedback` werden verworfen. Wenn der Ausgang zuvor
   physisch aktiv war, setzt `forceStop()` den realen
   `lastPhysicalDeactivationDirection`- und Zeitanker; ein bereits
   laufender Außenlüfter-Nachlauf wird nicht verkürzt oder abrupt beendet.
2. `driver.apply(result);` – dieselbe Übersetzungsfunktion wie bei jedem
   gewöhnlichen Tick (Abschnitt 12); keine zweite Ausgabelogik.
3. Danach: `plannedDirection() == Idle`,
   `state().lastAppliedDirection == Idle`,
   `state().acceptedCommand == std::nullopt`,
   `state().activeWindow == std::nullopt` und
   `state().pendingFeedback == std::nullopt`. Der interne
   Application-Slot wird im selben Hilfsaufruf auf `std::nullopt`
   gesetzt; `latchedWatchdogFault` bleibt unverändert bestehen,
   falls zuvor gesetzt.

`ActuatorPlanner::forceStop()` ist die einzige RAM-Stop-Operation; es gibt
keine separate `resetRuntime()`-Methode, die den physischen Deaktivierungs-
oder Lüfternachlauf verlieren könnte.

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
(oder umgekehrt) ist durch Klasse N und die physische Übergangsfunktion bereits
strukturell ausgeschlossen
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
  (10) outerFanPostRunMillis > 0
       (strukturell zwingend laut ACTUATOR_TIMING.md Zeile 186/220 -
       "zwingender Nachlauf" mit firmwarefesten Mindest-/Maximalgrenzen;
       die konkrete Grenze selbst bleibt TBD_COMMISSIONING/#35)
  (11) innerFanPostRunMillis: keine Relation noetig (0 = zulaessiger
       "kein Nachlauf"-Wert; ACTUATOR_TIMING.md Zeile 223-244 nennt den
       Innenluefter-Nachlauf ausdruecklich "konfigurierbar", nicht
       "zwingend", und benennt dort keine firmwarefeste Mindestgrenze -
       der Unterschied zu (10) ist damit begruendet, nicht erfunden)
  (12) switchingWindowMillis <= 9007199254740992 (2^53, die groesste im
       IEEE-754-double exakt darstellbare Ganzzahl). Diese Relation
       schliesst strukturell aus, dass
       `clamp(timeQuote,0,1) * switchingWindowMillis` (Abschnitt 8.1) einen
       Praezisions- oder Rundungsfehler ausserhalb der Ganzzahlgenauigkeit
       erzeugt, und schliesst zugleich einen Additionsueberlauf in der
       O(1)-Fensterarithmetik (Abschnitt 8.1) mit einem derart grossen
       switchingWindowMillis strukturell aus, bevor dieser Code ueberhaupt
       erreicht wird.
       Kein produktiver Wert in dieser Groessenordnung ist plausibel; die
       Relation ist ein struktureller Sicherheitsbeweis, keine erfundene
       Kommissionierungsgrenze.
```

Jede Mischkonfiguration (mindestens ein Feld `0`, mindestens ein anderes
Feld `!= 0`, ohne dass alle Relationen 1–12 zusätzlich erfüllt sind) ergibt
`Invalid`. Ein vollständig konsistenter Satz mit alle Relationen erfüllt
ergibt `Valid`, unabhängig vom konkreten (frei wählbaren) Zahlenwert.

Als zusätzliche, von der Parametervalidierung unabhängige Verteidigungslinie
(Abschnitt 8.1) gilt für jede Fensterberechnung zur Laufzeit strukturell:
`scheduledOnMillis <= switchingWindowMillis` (durch `min(...)` erzwungen,
nicht nur bewiesen) und keine `double`-zu-`uint64_t`-Konvertierung außerhalb
eines bereits über `clamp(timeQuote, 0.0, 1.0)` begrenzten Wertebereichs.

**Output-Wirkung (vollständig, keine Lücke mehr zu Abschnitt 8.2):**

| `classifyActuatorPlannerParameters` | `ActuatorPlanStatus` | `ActuatorPlanReason` | physisch |
|---|---|---|---|
| `Unconfigured` | `Unconfigured` | `NoCommissioning` | `Idle`, unmittelbarer Verwurf (I-2a) |
| `Invalid` | `InvalidInput` | `InvalidConfiguration` | `Idle`, unmittelbarer Verwurf (I-2b) |
| `Valid` | abhängig von der weiteren Klasse | abhängig von der weiteren Klasse | abhängig von der weiteren Klasse |

Diese Tabelle hält die Konfigurationsklassifikation unabhängig von der
weiteren Prioritätsentscheidung fest und beseitigt den früheren Widerspruch
vollständig: I-2a und I-2b verwerfen in **jedem** Fall (`Unconfigured` wie
`Invalid`) unbedingt und ohne Minimum-On-Hold; alle I-Klassen nutzen
denselben physischen Active -> Idle-Pfad (Abschnitt 8.4).
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
zwischenzeitlich geändert wurde. Das ist exakt die offene Lücke, die über
Issue #106 verfolgt wird; sie wird hier bewusst **nicht** als gelöst
behauptet.

### 14.3 Benanntes, blockierendes Integrationsgate: Issue #106

**Issue #106** „Aktorplaner Per-Run-Parameter-Snapshot und
Recovery-Bindung" ist Abhängigkeit dieses Plans für jede produktive
Verdrahtung. Die Live-Beschreibung von Issue #106 wurde vor diesem
Plan-Commit um I106.R1 präzisiert: strukturelle Producer-/Schema-/Snapshot-
Vorbereitung darf ohne Produktionswerte vor #35 erfolgen; produktive
Werte-/Grenzenaktivierung und der Abschluss dieses Integrationsgates hängen
zwingend von den durch #35 freigegebenen Werten/Grenzen ab. Sie definiert
weiterhin:

- einen unveränderlichen Pro-Lauf-Parametersnapshot, erzeugt genau einmal
  bei `NewActiveRun` aus den dann aktuellen, additiv und schema-versioniert
  eingeführten Service-Einstellungen und danach für die Lebensdauer des
  Laufes fix;
- eine Recovery-Bindung, die ausschließlich den persistierten, validierten
  Snapshot des unterbrochenen Laufes liest, nie aktuelle Live-Servicewerte;
- eine ausführbare Objekt-/Lebenszeit-Anbindung ohne dangling oder
  nicht-rebindbare Referenzsemantik (konkrete Wahl bleibt dem
  #106-eigenen Plan vorbehalten);
- einen rein strukturellen, typisierten und additiv schema-versionierten
  Producer-/Snapshot-Mechanismus, der vor #35 vorbereitet werden darf, aber
  keine produktiven Werte, Grenzwerte oder Defaults erfindet oder aktiviert;
  `ServiceConfiguration` Schema 1 bleibt dabei unverändert leer;
- die produktive ServiceConfiguration-Aktivierung und den produktiven
  Abschluss des #106-Gates erst, wenn #35 die erforderlichen Schaltfenster-,
  Mindestzeit-, Totzeit-, Nachlauf- und Gegenrichtungswerte/-grenzen
  freigegeben hat;
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
  Implementierung für Tick- und Stop-Pfad; `plannedDirection()` nur für
  Planung und `physicalDirection()` nur für den realen Ausgang; ein
  gemeinsamer Verwurf-Trigger-Satz für Akkumulator und Fenster (Abschnitt 8.4)
  statt getrennter, potenziell divergierender Regeln.
- **KISS:** `ActuatorSafetyGateInput` bleibt ein einfacher Werttyp; kein
  zweiter Test-Recorder in Produktionscode (Decorator bleibt test-only);
  die Feedback-Dispositionsregel (Abschnitt 9.3) ist eine einzige,
  richtungsunabhängige Regel statt einer mehrfach duplizierten Einzelfalltabelle mit vielen
  Einzelfällen; keine vorsorgliche Generalisierung ohne aktuellen Bedarf
  (z. B. kein zusätzlicher `ActuatorPlanReason`-Wert für die
  Gegenrichtungsbestätigung, da die physische Entscheidung bereits durch
  die bestehenden Ränge eindeutig beschrieben ist, Abschnitt 8.2).

## 17. Safety-, Security-, Recovery- und Hardwaregrenzen

- Fail-closed bei jeder einzelnen Klasse I-1 bis I-8 aus Abschnitt 8.2:
  `Idle` im selben Tick, kein Guthaben-Nachholen und keine Verzögerung
  durch `minimumOnMillis`.
- Keine Aktorfreigabe wird bei Boot, Reset, Fehler, unbekanntem Zustand oder
  unbestätigter Hardware vorausgesetzt; `ActuatorSafetyGateInput` startet mit
  `Unresolved` und erzwingt `Idle`. Ein strukturell ungültiger
  `ActuatorSafetyGateStatus`-Wert wird niemals wie `Allowed` behandelt,
  sondern über Klasse I-1 fail-closed abgefangen.
- `ImmediateStop`, Watchdog-Trip/Latch, explizites `NoValidRequest`,
  stale Kontext, `TimeInvalid`, ungültige Parameter und malformed Input
  überstimmen die Mindest-Einschaltzeit ausschließlich in Richtung „sicherer
  machen", niemals in Richtung „Freigabe erzwingen“.
- Ein gültiges `NeutralOff`, `AirLimitBlockedOff` oder ein bestätigter
  normaler Richtungswechsel darf dagegen gemäß Klasse N-1 bis N-2 durch
  `MinimumOnTimeHeld` gehalten werden.
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
- Ein normaler Teardown (OFF, bestätigter Richtungswechsel) darf die konkrete
  Mindest-Einschaltphase nicht verkürzen; die unmittelbaren I-Klassen dürfen
  sie dagegen nie verzögern. Jeder physische Active -> Idle setzt die
  Mindest-Auszeitbasis und startet den Außenlüfter-Nachlauf.
  (#29, #32, #33, #35) bleibt unverändert sichtbar offen.

## 18. Umsetzungs- und Commit-Schnitte

1. **Gemeinsame schmale Verträge / Klassifikation** – `actuator_plan_types.hpp`
   vollständig (Abschnitt 4.2, 7, 8.2, 8.3, 8.4, 8.6, 9.5, 13),
   `classifyActuatorDemand()`, `classifyActuatorPlannerParameters()`
   inklusive vollständiger struktureller Invarianten; keine
   Verhaltenslogik.
2. **Phase A / Annahme** – `ActuatorPlanner::tick()` Grundgerüst: Admission
   (Abschnitt 6.2), laufender Watchdog (6.4), Prioritätsleiter Klasse I/N
   (8.2), overflow-sichere Zeitarithmetik (8.3), `forceStop()`,
   `applyExternalWatchdogFaultReset()`.
3. **Fenster, Akkumulator, Mindestzeiten, Totzeit, Richtungswechsel** –
   Fensterlogik inklusive Arming-Regel und O(1)-Fortschritt (8.1), einziger
   Akkumulator mit vollständigen Verwurfsregeln (8.4), bestätigter
   Richtungswechsel (8.5), Klasse N der Prioritätsleiter vollständig.
4. **Lüfterlogik** – Außen-/Innenlüfter (10), eigener, von der
   Fenster-/Mindestzeitlogik klar getrennter Codeabschnitt.
5. **Feedback-Dispositionsmatrix** – vollständige Umsetzung von Abschnitt 9
   (`pendingFeedback`-Handoff, ereignisgetriebene Update-Regel, Live-
   Nachführung) innerhalb von `tick()`/`forceStop()`.
6. **Sink-Driver und Application-/Lifecycle-Integration** –
   `ActuatorPlanSinkDriver` (12), Erweiterung von
   `TemperatureControlApplicationOrchestrator` um `tickActuatorPlan()`
   (mit der in Abschnitt 6.1 festgelegten Aufrufsequenz) und die
   gemeinsame Lifecycle-Stop-Integration (11); keine
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

**Fensterbootstrap:** kleine Heating-Quote aus vollständig leerem Planner
akkumuliert korrekt über mehrere Fenster; Mindestimpuls wird nach exakt der
erwarteten Fensteranzahl ausgelöst; spiegelbildlich Cooling; reine
`Idle`-Anforderung erzeugt weiterhin kein Guthaben; Erststart,
gleichgerichteter Neustart und Richtungswechsel (Abschnitt 8.1, alle drei
Fälle der Arming-Regel) sind je durch eigene Tests abgedeckt.

**Fensterzustand vollständig im Runtime-State:** Heating-Fenster
armiert, Quote und Richtung bleiben über `activeWindow` bis zum nächsten
Fensterstart-Ereignis unverändert, auch wenn `acceptedCommand` sich mitten
im Fenster mit unveränderter Richtung ändert (wirkt erst nächstes Fenster)
oder mit einer Gegenrichtung ändert (armiertes Fenster bleibt unverändert
Heating, bis eine bestätigte Freigabe stattfindet); spiegelbildlich Cooling.

**Mindest-Einschaltzeit schützt akkumulierten Mindestimpuls (eigener Fund,
siehe CONTEXT_DELTA):** Guthaben akkumuliert über mehrere Fenster ohne
physische Einschaltphase, Mindestimpuls wird in einem späteren Fenster
ausgelöst; ein OFF/Kontextverlust unmittelbar nach Auslösung des Impulses
verkürzt die tatsächliche Einschaltdauer **nicht** unter `minimumOnMillis`,
gemessen ab `currentOnPhaseStartedAtMonotonicMillis` (Beginn dieser
konkreten Einschaltphase), nicht ab einer früheren Erstarmierung; ein
Teardown, der erst NACH Ablauf der Mindest-Einschaltzeit dieser konkreten
Einschaltphase eintrifft, wird sofort vollzogen, auch wenn das Fenster laut
eigener Arithmetik noch länger eingeschaltet geblieben wäre.

**Gegenrichtungsbestätigung nach Ende der Mindest-Einzeit (R4.3):** vor
Minimum-On-Ende; exakt Minimum-On-Ende; danach bleibt das alte Fenster nur bis
zu seinem bereits eingefrorenen Ende maßgeblich. An der ersten und an jeder
weiteren Fenstergrenze während unbestätigter B werden kein neues altes
Heating-/Cooling-Fenster und kein altes Energie-Guthaben aus B erzeugt; der
physische Ausgang bleibt danach Idle. Die Bestätigung läuft unabhängig davon
weiter; exakt Bestätigungsdauer und erfüllte Arming-Fristen planen B aus der
dann aktuellen Request neu. Abbruch kurz davor oder nach einer Grenze (keine
Freigabe aus alter Evidenz), beide Richtungen symmetrisch; Feedback für B ist
während der gesamten Sperre `DeferredOrLimited`.
**Explizite `NoValidRequest`-Evaluation:** Eine aktive
Heating-/Cooling-Freigabe innerhalb ihrer Mindest-Einschaltzeit wird durch
eine neue, strukturell gültige `Unavailable`/`InvalidInput`-Evaluation
sofort beendet (Klasse I-6); ein `tick()`-Aufruf mit
`newEvaluation = std::nullopt` tut dies ausdrücklich nicht und führt
die laufende Freigabe unverändert fort.
**Feedback-Vertrag (R4.4/R4.5):** Das n/n+1/n+2-Orakel für Heating -> Cooling
und Cooling -> Heating prüft den Orchestrator-internen Slot, nicht eine vom
Test-Caller kopierte Feedbackvariable. Mehrere `tickActuatorPlan()`-
Aufrufe dürfen zwischen zwei `evaluateTemperatureControl()`-Aufrufen
liegen; nur ein geändertes Update ersetzt den Slot, und genau ein
`evaluateTemperatureControl()`-Aufruf konsumiert den zuletzt gültigen
Wert. Die Admission-Reject-Variante (höhere B als
`StaleOnArrivalWatchdog`/`StaleOnArrivalContext`) führt genau zu
`{B, Rejected}`, nicht A. OFF schließt das Fenster auf
`nullopt`. Eine malformed neue Evaluation bei pending A löscht dagegen
das Handoff auf `nullopt` und erzeugt kein Sequence-Feedback.
`forceStop()` löscht Planner-Handoff und Application-Slot;
`admissionOutcome` ist für jeden Fall aus Abschnitt 9.5 geprüft.
**AirLimit-Klassifikation:** `NeutralOff` vs. `AirLimitBlockedOff`:
identische Mindestzeit-Behandlung, unterschiedliche Akkumulatorwirkung;
aufgebautes Guthaben, danach `AirLimitBlockedOff`: Guthaben verworfen;
Übergang zu `AirLimitReducedDemand`: Guthaben verworfen, folgende Fenster
akkumulieren nur aus der reduzierten Quote; fortlaufendes Verbleiben in
`AirLimitReducedDemand`: normale Akkumulation.

**Langes Idle:** aufgebautes Heiz-Guthaben, danach eine sehr lange Folge von
`NeutralOff`-Ticks, danach erneute kleine Heiz-Anforderung: Guthaben wurde
beim Übergang nach `Idle` vollständig verworfen, kein sofortiger
Mindestimpuls aus altem Guthaben; Fensterstart beginnt strukturell frisch
(Fall (a) oder (b) je nach seit der Deaktivierung verstrichener Zeit).

**Safety-Gate und Fail-closed-Klassen:** `Unresolved`,
`Allowed`, `ImmediateStop` und unbekannter Enumwert werden geprüft.
`ImmediateStop` und `Unresolved` erzwingen auch innerhalb
laufender Mindest-On-Zeit im selben Tick `Idle`; ein unbekannter Wert
`static_cast<ActuatorSafetyGateStatus>(0xFF)` wird als I-1
`InvalidInput/MalformedInput` behandelt. Zusätzlich werden I-2a/I-2b,
I-4/I-5, I-6, I-7 und I-8 jeweils an einer aktiven Mindest-On-Phase geprüft.

**Watchdog-Fault-Evidenz:** Watchdog-Trip erzeugt
`ActuatorWatchdogFaultEvidence`; eine danach eintreffende, ansonsten gültige
neue Request löscht ihn nicht; `NewActiveRun`-Boundary löscht ihn nicht;
`Recovery`/`Standby`/`Service`/`Fault`/`SafeBoot`/`LeaveTemperatureControl`
löschen ihn ebenfalls nicht; nur `applyExternalWatchdogFaultReset()` löscht
ihn; ein simulierter Neustart (neue `ActuatorPlanner`-Instanz) wird
ausdrücklich **nicht** als Ersatz für diesen Reset getestet oder behauptet.

**Einziger Akkumulator:** Heiz-Guthaben aufgebaut, kurze (nicht bestätigte)
Cool-Gegenanforderung lädt kein zweites Guthaben und wird nach Abbruch der
Bestätigung spurlos verworfen; bestätigter Wechsel verwirft das
Heiz-Restguthaben; spiegelbildlicher Test für Cool -> Heat; Gegenrichtung vor
Ablauf der Mindest-Einschaltzeit bleibt physisch wirkungslos, während der
Bestätigungstimer unabhängig weiterläuft.

**Fenster-/Impulsplatzierung/Overflow:** Quote `0`, kleine, normale, volle
Quote; Akkumulator unterhalb/exakt auf/oberhalb der Schwelle;
Akkumulator-Obergrenze; Rundungs-Grenzfälle inklusive `scheduledOnMillis`
niemals `> switchingWindowMillis`; neue Quote mitten im Fenster wirkt erst
im nächsten Fenster; Fensterstart und mehrere Fensterwechsel mit
`startMonotonicMillis`/`nowMonotonicMillis` nahe `UINT64_MAX` ohne
Additionsüberlauf (direkter Test der in Abschnitt 8.1 bewiesenen Schranke);
eine sehr lange Pause (mehrere Fenster) führt zu genau einem
Fensterstart-Ereignis ohne Nachholen der übersprungenen Fenster;
`switchingWindowMillis > 2^53` wird von `classifyActuatorPlannerParameters`
als `Invalid` eingestuft (Abschnitt 13, Relation 12).

**Mindestzeiten/Totzeit und physische Deaktivierung (R4.1/R4.2):**
Normaler `NeutralOff` und `AirLimitBlockedOff` dürfen die
konkrete Mindest-On-Phase halten; jede I-Klasse schaltet im selben Tick aus.
Ein normaler Window-Off-Anteil in Heating und Cooling setzt bei jedem realen
Active -> Idle den physischen Deaktivierungsanker. Gleichgerichteter Restart
wartet nur `minimumOffMillis`; ein tatsächlicher Richtungswechsel
wartet Mindest-Off und Totzeit nach der Späteres-Ende-Regel. Beide Richtungen,
exakter Gleichstand, voller und nahezu voller Puls sowie der Richtungswechsel
aus bereits laufendem Window-Off werden getestet.
**Parameterklassifikation:** vollständige Tabelle aus Abschnitt 13 (alle
Felder `0` -> `Unconfigured`; jede einzelne strukturell unmögliche Relation
1-12 einzeln getestet -> `Invalid`, inklusive `outerFanPostRunMillis == 0`
(jetzt `Invalid`) und `innerFanPostRunMillis == 0` (weiterhin
`Valid`, sofern alle anderen Relationen erfüllt sind); vollständig
konsistente Testwerte -> `Valid`); Klasse-I-2-Verwurf ist für `Unconfigured` und
`Invalid` identisch unbedingt.

**Zeit-/Overflow-Verträge:** Gleichheit an jeder Frist; knapp
davor/genau darauf/knapp danach für Mindest-Ein-/Auszeit, Totzeit,
Bestätigungsdauer, Watchdog, Fan-Nachlauf; Retrograde-Zeit an jeder
Referenzzeit (-> `TimeInvalid`); Werte nahe `UINT64_MAX`.

**Priorität/Fail-closed:** malformed `ControlRequest` (Sequenz `0`,
unbekannte Richtung, `timeQuote` `NaN`/`Infinity`/außerhalb `[0,1]`,
Status/Request-Mismatch, ungültiger Kontext) -> `MalformedInput`,
sofortiger Verwurf; strukturell ungültiger `currentCanonicalContext` ->
ebenfalls `MalformedInput` (Klasse I-1); mehrere gleichzeitig
zutreffende Bedingungen (z. B. Safety `ImmediateStop` **und** Kontext-Stale
gleichzeitig) -> exakte Klassen-Reihenfolge aus 8.2.

**Lifecycle/Stop und Fan-Nachlauf:** Für jede der sieben
`TemperatureControlLifecycleBoundary`-Werte ruft die Application-Grenze
denselben Stop-Pfad auf. `forceStop()` verwirft Akkumulator,
`activeWindow`, Gegenrichtung, `acceptedCommand` und Planner-Handoff,
setzt bei physischem Active -> Idle den
`lastPhysicalDeactivationDirection`/Zeitanker und lässt den
Außenlüfter-Nachlauf weiterlaufen; der interne Application-Slot wird ebenfalls
geleert, `latchedWatchdogFault` bleibt unverändert. Ein sofortiger
gleichgerichteter Restart respektiert weiterhin Minimum-Off. Stop und Fault
werden zusammen mit kurzem Duty-Puls, kürzerem/längerem Off-Anteil und
erneutem Puls im Nachlauf geprüft.
**Sequenzhochwasserzeichen:** `lastAcceptedSequence` bleibt über
`forceStop()` innerhalb derselben `ActuatorPlanner`-Instanz erhalten; eine
neue Instanz (simulierter Neustart) beginnt regulär bei einer neuen,
niedrigeren Sequenz, ohne dies fälschlich als persistierten Zustand zu
behaupten.

**Architekturnachweis:** `ActuatorPlanner` kompiliert und wird getestet ohne
jede Abhängigkeit auf `device_platform`-Sink-Header.

### 19.2 Revision-5-Direktmatrix für physische Zeit- und Handoff-Grenzen

Zusätzlich zu den allgemeinen Orakeln der vorherigen Absätze werden mindestens
diese Fälle als getrennte native Tests umgesetzt:

1. Normaler Window-Off-Anteil in Heating und Cooling setzt bei jedem
   tatsächlichen Active -> Idle die physische Deaktivierungszeit; ein nächster
   Puls vor dem Minimum-Off-Ende bleibt aus.
2. Ein nächster Fensterpuls wird vor, exakt auf und nach dem
   Minimum-Off-Ende geprüft: vor dem Ende bleibt er aus; exakt auf und danach
   ist er zulässig, sofern sein natürliches Fensterintervall noch läuft.
3. Kurzer und langer Off-Anteil, volle Quote und nahezu volle Quote werden
   gegen den physischen Ausgang und den Außenlüfter getrennt geprüft.
4. Ein akkumuliertes Mindestimpulsfenster, das am Arming-Gate gesperrt ist,
   wird als DeferredOrLimited verworfen und nicht in das Folgefenster
   nachgeholt; der Mindestimpulsfall wird für beide Richtungen geprüft.
5. Fail-closed-Ränge I-1, I-2a/I-2b, I-3a/I-3b, I-4, I-5, I-6, I-7 und I-8
   werden jeweils innerhalb einer laufenden Mindest-On-Zeit geprüft: in jedem
   Fall ist das Peltier im selben Tick Idle und der physische
   Deaktivierungsanker gesetzt.
6. Gültiges NeutralOff und AirLimitBlockedOff werden separat geprüft und
   folgen dagegen dem normalen Minimum-On-Hold-Vertrag.
7. Eine unbestätigte Gegenrichtung über mindestens zwei alte Fenstergrenzen
   erzeugt keine neue alte Energie, kein Gegenrichtungs-Guthaben und bleibt
   physisch Idle; nach Bestätigung wird B aus der dann aktuellen B-Request
   neu geplant.
8. Das interne Single-use-Handoff wird ohne manuelle Feedbackkopie im Caller
   geprüft: mehrere Planner-Ticks speichern den neuesten Stand, genau ein
   #22-Aufruf konsumiert ihn, ein zweiter erhält nullopt.
9. Eine malformed neue Evaluation bei pending A löscht das alte Handoff,
   erzeugt kein Rejected mit fremder Sequence und löst keinen Replay aus.
10. Der Außenlüfter-Nachlauf startet bei jedem physischen Peltier-AUS:
    kurzer Duty-Puls, Off-Anteil kürzer als der Nachlauf, Off-Anteil länger als
    der Nachlauf, normaler Stop und Fault werden getrennt geprüft.
11. Ein erneuter Peltierpuls während des Außenlüfter-Nachlaufs hält den Lüfter
    ohne Unterbrechung aktiv; der Nachlauf wird nicht doppelt gestartet.
12. Physische Deaktivierung, Mindest-Off und Nachlauf nahe `UINT64_MAX`
    verwenden ausschließlich elapsed-Vergleiche ohne Überlauf.
13. Ein Richtungswechsel mit bereits laufender Mindest-Off-Zeit aus einem
    normalen Window-Off erfordert zusätzlich die Totzeit und nutzt die
    Späteres-Ende-Regel; Heating -> Cooling und Cooling -> Heating sind
    symmetrisch.

### 19.3 `test/test_actuator_plan_sink_driver/test_actuator_plan_sink_driver.cpp`

Vollständige Cross-Sink-Reihenfolgeprüfung über `SharedActuatorCallTrace`
gemäß Abschnitt 12.1: Freigabe Heating/Cooling, Abschalten, Richtungswechsel
mit garantiertem Off-Zwischenzustand; `simultaneousActivationObserved() ==
false`; korrekte Weitergabe von `outerFanEnabled`/`innerFanEnabled` an die
jeweils richtige `IBinaryOutputSink`-Instanz.

### 19.4 `test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp` (gezielte Ergänzung)

- `tickActuatorPlan()` leitet `currentCanonicalContext`/
  `temperatureControlledPhase` korrekt aus der bestehenden
  `resolveEffectiveControlContext()`/`isTemperatureControlledProcessState()`-
  Kette ab und ruft `planner.tick()`/`driver.apply()` in der in Abschnitt
  6.1 festgelegten Reihenfolge genau je einmal auf – sowohl im normalen als
  auch im fail-closed Fall.
- `complete()`/`needsRuntimeReset()` ruft
  `resetActuatorPlanAtBoundary()` für dieselbe committed Lifecycle-Grenze
  auf wie `resetTemperatureControlAtBoundary()`; ein Fixture-Aufruf löst
  nachweislich beide Resets aus und leert den internen Handoff-Slot.
- **Interner Single-use-Handoff:** Das Test-Fixture ruft
  `evaluateTemperatureControl()` ohne caller-supplied
  `previousControlRequestFeedback` auf. Mehrere
  `tickActuatorPlan()`-Aufrufe zwischen zwei Evaluationen speichern nur
  den neuesten Planner-Update; genau der nächste #22-Aufruf erhält ihn genau
  einmal. Ein weiterer Evaluation-Aufruf ohne neues Planner-Update erhält
  `nullopt`. Die neue Evaluation wird danach als `newEvaluation`
  an den nächsten Planner-Tick gegeben.
- Eine malformed Evaluation B bei pending A leert den Orchestrator-Slot; der
  nächste #22-Aufruf erhält weder A noch eine unsichere B-Sequence.
- `ActuatorPlanner&`/`ActuatorPlanSinkDriver&` werden über die
  gesamte Lebenszeit des Test-Fixtures unverändert referenziert (Objektlebenszeit-
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
- **Governance:** Issue #106 wurde live aktualisiert, **bevor** dieser
  Plan-Commit erstellt wurde (eigenständiger Governance-Schritt, siehe
  PR-Body). Diese Revision-5-Planänderung wird als eigener, abgeschlossener
  Plan-Commit committet. Erst danach ist die exakte Revision-5-Plan-SHA
  bekannt und wird im Draft-PR #105 und im `SESSION HANDOVER` ausgewiesen.
  Eine Nachführung von `docs/ROADMAP.md` auf die exakte Revision-5-SHA
  erfolgt anschließend in einem separaten, rein redaktionellen
  Metadaten-Commit; Plantext und Roadmap-Metadaten werden nicht vermischt.

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
- **Einheitliche, ereignisgetriebene `pendingFeedback`-Regel** (Abschnitt 9):
  Diese Revision verwendet eine einzige, richtungsunabhängige
  Live-Nachführungsregel mit internem Single-use-Handoff (Abschnitt 9.3/9.4).
  Sollte sich bei der Umsetzung zeigen, dass der
  tatsächliche #22-Code (nicht nur der Plan) an einer Stelle eine andere
  Disposition erwartet als hier hergeleitet, ist das ein materieller Befund
  gegen den bestehenden #22-Code (nicht gegen diesen Plan) und wird als
  Abweichung gemeldet, nicht still umgangen.
- **Asymmetrische Mindest-Auszeit-/Totzeit-Anwendung** (Abschnitt 8.1, Fall
  (b) vs. (c)): Diese Revision wendet die Polaritätstotzeit ausschließlich
  bei tatsächlichem Richtungswechsel an, nicht bei gleichgerichtetem
  Neustart. Die Unterscheidung folgt der physischen Funktion der Totzeit als
  Polaritätsschutz und ist hier ausdrücklich festgelegt, nicht stillschweigend
  aus einer Mindest-Auszeit abgeleitet.
