# Plan: Issue #23 – Aktorplaner, Mindestzeiten, Totzeit und Lüfterlogik

## 1. Status, Scope und Owner-Gate

- Revision: **4**. Ersetzt Revision 3 (`c1ca0db178442db64332a7b5ef7c66341c6ea500`)
  vollständig. Diese Revision ist ohne Rückgriff auf Revision 1, 2 oder 3
  vollständig ausführbar und reviewbar.
- Live-Issue: #23, offen, Status `PLANNED_SPEC_PENDING`.
- Draft-PR: #105, Branch `agent/issue-23-aktorplaner-plan` -> `main`.
- Planpfad: `docs/tasks/issue-23-actuator-planner-plan.md`.
- Planbasis: `main` @ `2986dca5736a34171910c9245a3d5f43fa55da06`
  (Merge-Commit von PR #104 / Issue #22, unverändert seit Revision 1).
- Issue **#106** „Aktorplaner Per-Run-Parameter-Snapshot und
  Recovery-Bindung" wurde vor diesem Plan-Commit live präzisiert (siehe
  gesonderter Governance-Nachweis in PR-Body/SESSION HANDOVER) und bleibt
  Abhängigkeit dieses Plans für die produktive Verdrahtung (Abschnitt 14).
- Die Umsetzung bleibt gesperrt, bis der Owner exakt diesen neuen
  Revision-4-Plan-Commit mit `PLAN APPROVED: <SHA>` freigibt.
- Diese Revision committet ausschließlich Plandokumentation. Sie implementiert
  keine Produktionslogik, keine produktiven Tests, keine Hardware-, GPIO-,
  Toolchain- oder CI-Änderung.
- Der PR bleibt Draft. Es gibt kein `Ready for review`, keinen Merge, kein
  Auto-Merge und kein Branch-Löschen. Issue #23 wird nicht geschlossen.

```text
CONTEXT_BASELINE_BRANCH: agent/issue-23-aktorplaner-plan
CONTEXT_BASELINE_SHA: 2986dca5736a34171910c9245a3d5f43fa55da06
CONTEXT_HEAD_BEFORE_REVISION: ab8ce9f (Roadmap-Metadaten-Commit nach Revision 3)
CONTEXT_PLAN_SHA: NONE (wird nach dem Commit dieser Revision eingetragen)
CONTEXT_REFRESH_MODE: FULL
CONTEXT_DELTA: Vollständiges Owner-Planreview von Revision 3 mit neun
  Befunden (R3.1-R3.9, davon acht BLOCKER und ein MAJOR) sowie fünf
  Befunden zur Live-Beschreibung von Issue #106 (I106.1-I106.5) erhalten.
  Issue #106 wurde vor diesem Plan-Commit separat live aktualisiert
  (siehe PR-Body). Die neun Planbefunde sind in dieser Revision vollständig
  gelöst:
  R3.1 (Fensterzustand nicht vollständig im Runtime-State abgebildet) ->
    neuer expliziter Wertetyp `ActiveSwitchingWindow` (Richtung und
    `scheduledOnMillis` werden am Fensterstart genau einmal eingefroren,
    Abschnitt 6.0, 8.1); `armedDirection` wird aus `activeWindow` abgeleitet,
    nicht mehr als eigenes Feld gehalten (zusätzliche Vereinfachung, schließt
    einen Redundanzfehler aus).
  R3.2 (Lücke bei Gegenrichtungsbestätigung nach Ende der Mindest-Einzeit)
    -> explizite Regel: das bereits eingefrorene alte Fenster läuft während
    der gesamten Bestätigungs-/Sperrphase unbeeinflusst nach eigener
    Fensterlogik weiter, die Gegenrichtung erhält weder Energie noch
    physische Wirkung vor bestätigter, gegateter Freigabe (Abschnitt 8.5);
    eine allgemeine, richtungsunabhängige Feedback-Dispositionsregel
    (Abschnitt 9) macht den physischen Entscheid für jeden Tick eindeutig,
    ohne einen zusätzlichen Status-Rang zu benötigen.
  R3.3 (Fenster-Neustart nach normalem OFF widerspricht Mindest-Auszeit) ->
    Planung (Fensterfortschritt) und physische Freigabefähigkeit (Arming)
    werden strukturell getrennt; neue persistente Felder
    `lastDeactivatedDirection`/`directionDeactivatedAtMonotonicMillis`
    (nie durch `forceStop()` verworfen, sondern von `forceStop()` selbst
    gesetzt) plus eine einheitliche Arming-Regel, die zwischen
    gleichgerichtetem Neustart (nur Mindest-Auszeit) und Richtungswechsel
    (Mindest-Auszeit UND Totzeit, Späteres-Ende-Regel) unterscheidet
    (Abschnitt 8.1).
  R3.4/R3.5/R3.6 (Feedback-Vertrag verletzt `ACTUATOR_TIMING.md`;
    `acceptedCommand` ist nicht immer das Feedback-Subjekt;
    `Rejected` darf zwischen Ticks nicht verloren gehen) -> vollständig
    entkoppeltes `pendingFeedback`-Handoff (`PendingControlRequestFeedback`)
    getrennt von `acceptedCommand`; ereignisgetriebene, exhaustive
    Update-Tabelle inklusive explizitem `Rejected` bei vollständiger
    Nichtannahme; Einzelverbrauch über einen expliziten
    Orchestrator-Vertrag (Abschnitt 6.1, 9) statt eines impliziten
    „nullopt reicht"-Arguments.
  R3.7 (Pflicht-Nachlauf Außenlüfter darf nicht 0 sein) -> Relation
    `outerFanPostRunMillis > 0` (verbindlich laut `ACTUATOR_TIMING.md`
    Zeile 186/220, zwingender Nachlauf mit firmwarefesten Grenzen);
    `innerFanPostRunMillis` bleibt ausdrücklich bei `>= 0` mit begründetem
    Unterschied (`ACTUATOR_TIMING.md` Zeile 223-242, kein „zwingend" für den
    Innenlüfter); zusätzliche strukturelle Relation gegen
    Double-Praezisions-/Konvertierungsfehler bei `timeQuote *
    switchingWindowMillis` (Abschnitt 13).
  R3.8 (unbekannter Safety-Gate-Enumwert muss fail-closed sein) -> Rang 1
    (`MalformedInput`, umbenannt von `MalformedEvaluation`) deckt jetzt
    explizit auch einen strukturell ungültigen `ActuatorSafetyGateStatus`-
    Wert sowie einen strukturell ungültigen `currentCanonicalContext` ab,
    nicht nur `newEvaluation` (Abschnitt 8.2).
  R3.9 (Sink-Driver-Aufruf bei normalen Ticks nicht explizit) -> explizite
    Aufrufsequenz für `tickActuatorPlan()` (Abschnitt 6.1, 11).
  Zusätzlich beim eigenen Review vor Fertigstellung entdeckt und behoben:
  die ursprünglich für Rang 9 vorgesehene Zeitbasis
  (`directionActivatedAtMonotonicMillis`, gemessen ab Erstarmierung) hätte
  einen über mehrere Fenster akkumulierten Mindestimpuls vorzeitig
  abschneiden können, wenn die Akkumulationsphase selbst physisch inaktiv
  war; die Mindest-Einschaltzeit wird jetzt ab dem tatsächlichen Beginn der
  aktuellen physischen Einschaltphase gemessen
  (`currentOnPhaseStartedAtMonotonicMillis`, Abschnitt 8.1/8.2); ein
  malformed Tick darf ein bereits gültig ermitteltes `Rejected` in
  `pendingFeedback` nicht überschreiben, nur ein tatsächlicher Verwurf des
  davon betroffenen `acceptedCommand` löst die Rejected-Überschreibung aus
  (Abschnitt 9); `forceStop()` löscht `pendingFeedback` unbedingt, analog zu
  #22s eigenem Reset an denselben Lifecycle-Grenzen.
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
  Späteres-Ende-Regel (keine Addition), ausschließlich bei tatsächlichem
  Richtungswechsel (nicht bei gleichgerichtetem Neustart);
- Außenlüfter ohne absichtlichen Vorlauf, mit zwingendem Nachlauf;
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
strukturell ungültig und wird **niemals** wie `Allowed` behandelt – er wird
in Abschnitt 8.2 Rang 1 (`MalformedInput`) fail-closed abgefangen, exakt wie
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

// Am Fensterstart genau einmal erzeugter, unveraenderlicher Snapshot
// (loest R3.1): Richtung und geplante Einschaltdauer eines laufenden
// Fensters aendern sich bis zum naechsten Fensterstart-Ereignis nicht,
// auch wenn acceptedCommand sich mitten im Fenster aendert.
struct ActiveSwitchingWindow {
    std::uint64_t startMonotonicMillis{0U};
    AbstractControlDirection direction{AbstractControlDirection::Idle};
    std::uint64_t scheduledOnMillis{0U};
};

// Getrenntes Feedback-Handoff (loest R3.4/R3.5/R3.6): das Subjekt, fuer das
// #23 als naechstes ein PreviousControlRequestFeedback an #22 liefert, ist
// NICHT zwangslaeufig identisch mit dem aktuell physisch massgeblichen
// acceptedCommand. Siehe Abschnitt 9 fuer die vollstaendige Update-Regel.
struct PendingControlRequestFeedback {
    std::uint64_t sequence{0U};
    PreviousControlRequestFeedback::Disposition disposition{
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
};

struct ActuatorPlannerRuntimeState {
    std::optional<AcceptedControlCommand> acceptedCommand;
    std::optional<ActiveSwitchingWindow> activeWindow;
    PulseAccumulator accumulator;

    // Beginn der aktuellen ununterbrochenen physischen Einschaltphase;
    // nullopt, sobald appliedDirection diesen Tick Idle ist. Alleinige
    // Zeitbasis fuer die Mindest-Einschaltzeit (Abschnitt 8.2 Rang 9,
    // korrigiert gegenueber Revision 3 - siehe CONTEXT_DELTA).
    std::optional<std::uint64_t> currentOnPhaseStartedAtMonotonicMillis;

    // Letzte tatsaechlich deaktivierte Richtung und ihr Zeitpunkt; wird von
    // JEDEM Teardown-Ereignis (inklusive forceStop()) gesetzt und NIE
    // geloescht - dient als alleinige Grundlage der Arming-Regel
    // (Abschnitt 8.1), die zwischen Erststart, gleichgerichtetem Neustart
    // und Richtungswechsel unterscheidet (loest R3.3).
    std::optional<AbstractControlDirection> lastDeactivatedDirection;
    std::optional<std::uint64_t> directionDeactivatedAtMonotonicMillis;

    std::optional<AbstractControlDirection> counterDirectionCandidate;
    std::uint64_t counterDirectionObservedSinceMonotonicMillis{0U};

    std::optional<std::uint64_t> lastAcceptedSequence;
    std::optional<std::uint64_t> lastNewRequestAcceptedAtMonotonicMillis;
    std::optional<ActuatorWatchdogFaultEvidence> latchedWatchdogFault;

    std::optional<PendingControlRequestFeedback> pendingFeedback;

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
    [[nodiscard]] AbstractControlDirection armedDirection() const;  // abgeleitet aus activeWindow, kein eigenes Feld

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
| `acceptedCommand` | Phase A, jede erfolgreiche Admission (6.2 Schritt 3/4) | jeder Rang-1/2/3/5/7-Verwurf (8.2), Rang-9-Ende-Teardown (8.1), `forceStop()` | Nein |
| `activeWindow` | Arming-Ereignis (8.1) | dieselben Trigger wie `acceptedCommand`-Verwurf, gegated durch Rang 9 (8.2) | Nein |
| `accumulator` | Fensterstart-Ereignis (8.1) | identisch zu `activeWindow` (8.4) | Nein |
| `currentOnPhaseStartedAtMonotonicMillis` | Tick, an dem `appliedDirection` Idle->Heating/Cooling wechselt | Tick, an dem `appliedDirection` diesen Tick Idle ist | Nein (wird bei `forceStop()` auf `nullopt`, da `appliedDirection` danach Idle ist) |
| `lastDeactivatedDirection` / `directionDeactivatedAtMonotonicMillis` | jedes Teardown-Ereignis von `activeWindow`, inklusive `forceStop()` | nie gelöscht, nur überschrieben vom jeweils nächsten Teardown | Ja (wird von `forceStop()` selbst geschrieben, nicht gelöscht) |
| `counterDirectionCandidate` / `...ObservedSinceMonotonicMillis` | 8.5 (neuer, plausibler Gegenrichtungskandidat) | Unterbrechung (8.5), `activeWindow`-Teardown, `forceStop()` | Nein |
| `lastAcceptedSequence` | Phase A, jede erfolgreiche Admission (6.2 Schritt 4 „sonst") | nie gelöscht (Hochwasserzeichen) | Ja |
| `lastNewRequestAcceptedAtMonotonicMillis` | Phase A, jede erfolgreich verarbeitete Evaluation (6.2 Schritt 3/4) | nie gelöscht | Ja |
| `latchedWatchdogFault` | Watchdog-Trip (Rang 5) | ausschließlich `applyExternalWatchdogFaultReset()` | Ja |
| `pendingFeedback` | ereignisgetrieben, siehe Abschnitt 9 | ereignisgetrieben (9), unbedingt bei `forceStop()` | Nein |
| `outerFanActive`/`...DeactivationRequestedAtMonotonicMillis` | Abschnitt 10 | Abschnitt 10 | Ja (Nachlauf läuft über `forceStop()` unverändert weiter, Abschnitt 11) |
| `innerFanActive`/`...DeactivationRequestedAtMonotonicMillis` | Abschnitt 10 | Abschnitt 10 | Ja (analog) |

`armedDirection()` ist keine gespeicherte Zustandsvariable, sondern eine
reine Ableitung: `activeWindow.has_value() ? activeWindow->direction :
AbstractControlDirection::Idle`. Ein separates `physicalDirection`-Feld
(Revision 3) entfällt, um zwei strukturell redundante, potenziell
divergierende Felder zu vermeiden.

### 6.1 Aufrufsemantik

Der Planer besitzt **einen** Tick-Einstiegspunkt (`tick()`, oben definiert).
Er wird vom Aufrufer (`TemperatureControlApplicationOrchestrator::tickActuatorPlan()`)
potenziell **häufiger** aufgerufen als #22 seine eigene, sensorgetaktete
`evaluateTemperatureControl()`-Berechnung durchführt.

`tickActuatorPlan()` folgt bei jedem Aufruf exakt dieser Reihenfolge (löst
R3.9):

```text
tickActuatorPlan(...)
  -> currentCanonicalContext/temperatureControlledPhase ableiten
     (bestehende resolveEffectiveControlContext()/
     isTemperatureControlledProcessState()-Kette, keine Parallel-Ableitung)
  -> result = planner.tick(input)          [genau einmal]
  -> driver.apply(result)                  [genau einmal, sowohl im
                                             normalen als auch im
                                             fail-closed Fall]
  -> result.feedbackForAcceptedRequest wird dem naechsten
     evaluateTemperatureControl()-Aufruf ueber
     TemperatureControlEvaluationEvidence.previousControlRequestFeedback
     zugefuehrt
  -> result zurueckgeben
```

**Verbindlicher Orchestrator-Vertrag (löst R3.6):** Jedes Ergebnis eines
`evaluateTemperatureControl()`-Aufrufs wird durch genau einen nachfolgenden
`tickActuatorPlan()`-Aufruf beobachtet, bevor der nächste
`evaluateTemperatureControl()`-Aufruf erfolgt. Dieser Vertrag stellt sicher,
dass `pendingFeedback` (Abschnitt 9) nicht doppelt an #22 übergeben wird,
selbst wenn `tickActuatorPlan()` selbst deutlich häufiger als
`evaluateTemperatureControl()` aufgerufen wird (der Planer liefert bei
wiederholten Ticks ohne neue Evaluation denselben, weiterhin gültigen
`pendingFeedback`-Wert zurück; die Einmaligkeit der tatsächlichen #22-Weitergabe
ist Aufgabe des Aufrufers, nicht eine Selbstlöschung im Planer). Ein direkter
Test dieses Vertrags ist Teil von Abschnitt 19.3.

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
   MalformedCandidate`. **Phase A endet hier**, kein vorgemerkter Kandidat,
   `pendingFeedback` bleibt unverändert (siehe Abschnitt 9 für die
   Ausnahme, falls Phase B denselben Tick `acceptedCommand` deshalb
   verwirft).
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
       `{sequence, Rejected}` (löst R3.5), `acceptedCommand` bleibt
       unverändert.
     - `context != input.currentCanonicalContext` ->
       `admissionOutcome = StaleOnArrivalContext`. Identisch zum vorigen
       Fall: `pendingFeedback = {sequence, Rejected}` bleibt bestehen,
       `acceptedCommand` unverändert.
     - trägt `demandClass` kein Heating/Cooling (`NeutralOff`/
       `AirLimitBlockedOff`): `admissionOutcome = Accepted`,
       `state_.pendingFeedback = std::nullopt` (OFF öffnet kein
       Feedbackfenster, Abschnitt 9). Der vorgemerkte Kandidat trägt
       `demandClass`, `direction = Idle`, `sequence` und `context`.
     - sonst (Heating/Cooling, alle Prüfungen bestanden):
       `admissionOutcome = Accepted`. Der vorgemerkte Kandidat trägt
       `demandClass`, `direction`, `timeQuote`, `sequence` und `context`;
       `pendingFeedback` bleibt vorläufig bei `{sequence, Rejected}` und
       wird ab dem Moment, in dem Phase B diesen Kandidaten zu
       `acceptedCommand` macht, jeden Tick live nachgeführt (Abschnitt 9).

Eine bei Admission abgelehnte neue Request (`DuplicateOrOldSequence`,
`StaleOnArrivalWatchdog`, `StaleOnArrivalContext`, `MalformedCandidate`)
berührt einen bereits gehaltenen `acceptedCommand` **nicht**: Phase B
bewertet in diesem Fall die bestehende Zeitbasis unverändert, so, als wäre
`newEvaluation` `std::nullopt` gewesen. Das schließt aus, dass ein Replay
oder ein verspätet eingetroffener Kandidat einen laufenden, weiterhin
gültigen Zeitplan zerstört. Es gilt jedoch **nicht** mehr für `pendingFeedback`:
`StaleOnArrivalWatchdog`/`StaleOnArrivalContext` erzeugen ein eigenständiges
`Rejected`-Feedbackfenster für den abgelehnten Kandidaten, unabhängig vom
Schicksal des physisch weiterhin maßgeblichen `acceptedCommand` (löst R3.5,
siehe Beispiel in Abschnitt 9.4).

### 6.3 Phase B – Physischer Ausgang (Prioritätsleiter, Abschnitt 8.2)

Phase B verwendet den in Phase A ermittelten vorgemerkten Kandidaten (falls
vorhanden) zusammen mit dem laufenden `ActuatorPlannerRuntimeState`, um genau
eine physische Ausgangsentscheidung für diesen Tick zu treffen. Sie ist in
Abschnitt 8.2 vollständig als Prioritätsleiter definiert und aktualisiert am
Ende jedes Ticks zusätzlich `pendingFeedback` gemäß der Live-Nachführungsregel
in Abschnitt 9.

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
Fensterstart-Ereignis genau einmal aus dem zu diesem Zeitpunkt gehaltenen
`acceptedCommand` erzeugt und ist für die Dauer des Fensters unveränderlich;
eine während des Fensters neu angenommene Request mit unveränderter Richtung
wirkt erst im nächsten Fenster, da sie zwar `acceptedCommand` sofort ersetzt,
aber `activeWindow` erst am nächsten Fensterstart-Ereignis neu gelesen wird.
Ereignisse der Prioritätsstufen 1–7 (Abschnitt 8.2) wirken dagegen sofort auf
`activeWindow` (unbedingter Verwurf).

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

**Arming-Regel (physische Freigabefähigkeit einer Richtung, löst R3.3).**
Eine Richtung `D` (`Heating`/`Cooling`) darf in dem Tick physisch neu
armiert werden (`activeWindow` wird neu erzeugt), in dem `acceptedCommand.direction
== D` gilt und `activeWindow` aktuell leer ist, sofern **eine** der folgenden
Bedingungen zutrifft:

- **(a) Erststart seit Konstruktion:** `state().directionDeactivatedAtMonotonicMillis
  == std::nullopt` (der Planer hat seit seiner Konstruktion noch nie eine
  Richtung deaktiviert). Keine Mindest-Auszeit, keine Totzeit – es gibt
  keine vorherige Deaktivierung, gegen die eine Frist geprüft werden
  müsste.
- **(b) Gleichgerichteter Neustart:**
  `state().lastDeactivatedDirection == D` **und**
  `deadlineReached(now, directionDeactivatedAtMonotonicMillis,
  minimumOffMillis)`. Keine Totzeit: Eine gleichgerichtete Wiederfreigabe
  ist kein Polaritätswechsel und erfordert laut `ACTUATOR_TIMING.md`
  ausschließlich die Mindest-Auszeit.
- **(c) Richtungswechsel:**
  `state().lastDeactivatedDirection != D` **und**
  `deadlineReached(now, directionDeactivatedAtMonotonicMillis,
  minimumOffMillis)` **und**
  `deadlineReached(now, directionDeactivatedAtMonotonicMillis,
  polarityDeadTimeMillis)` (Späteres-Ende-Regel: beide Fristen laufen
  parallel ab demselben `directionDeactivatedAtMonotonicMillis`, keine
  Addition; maßgeblich ist, dass **beide** erfüllt sind).

Diese Regel unterscheidet die von R3.3 geforderten fünf Fälle
widerspruchsfrei:

1. *Erstmaliger Start:* Fall (a).
2. *Normales Window-Off* (das aktuelle Fenster erreicht `elapsedInWindow >=
   activeWindow.scheduledOnMillis`, `acceptedCommand.direction` bleibt
   unverändert `D`): **kein** Teardown-Ereignis – `activeWindow` bleibt
   bestehen (siehe „Fensterfortschritt" unten) und wird am nächsten
   Fensterstart-Ereignis erneut aus dem dann aktuellen `acceptedCommand`
   gelesen, ohne die Arming-Regel erneut zu durchlaufen.
3. *Explizite OFF-Request* (`acceptedCommand.direction` wechselt nach
   `Idle`) oder ein anderer Rang-1-bis-7-Verwurf: `activeWindow`-Teardown
   (siehe Rang 9/10 unten), `lastDeactivatedDirection`/
   `directionDeactivatedAtMonotonicMillis` werden gesetzt.
4. *Gleichgerichteter Restart* nach (3): Fall (b) – nur Mindest-Auszeit.
5. *Richtungswechsel* (bestätigte Gegenrichtung, Abschnitt 8.5): Nach dem
   Teardown der alten Richtung (die dabei `lastDeactivatedDirection` setzt)
   greift für die neue Richtung Fall (c) – Mindest-Auszeit **und** Totzeit.

**Teardown-Timing (Zusammenspiel mit Mindest-Einschaltzeit, Rang 9 in
Abschnitt 8.2).** Ein Teardown-auslösendes Ereignis (Übergang von
`acceptedCommand.direction` nach `Idle`, Rang-1-bis-7-Verwurf, oder
bestätigter, gegateter Richtungswechsel) wird nicht zwingend im selben Tick
physisch vollzogen: Solange diesen Tick `elapsedInWindow <
activeWindow->scheduledOnMillis` (physisch noch im Einschaltanteil des
Fensters) **und** die Mindest-Einschaltzeit noch nicht erfüllt ist (siehe
Rang 9), bleibt `activeWindow` unverändert bestehen und der physische
Ausgang bleibt ohnehin `Active` (identisch zu dem, was die reine
Fensterarithmetik für diesen Tick ergäbe). Sobald entweder die
Mindest-Einschaltzeit erfüllt ist oder der Einschaltanteil des Fensters
ohnehin bereits vorbei ist, wird der Teardown ausgeführt.

**Maßgebliche Quote pro Fenster.** Bei jedem Fensterstart-Ereignis
(Erstarmierung, Richtungswechsel-Freigabe, oder regulärer Übergang in ein
Folgefenster derselben Richtung) wird `requestedOnMillisExact` aus der Quote
des zu diesem Zeitpunkt gehaltenen `acceptedCommand` berechnet:

```text
requestedOnMillisExact = clamp(timeQuote, 0.0, 1.0) * switchingWindowMillis   (double, ungerundet)
```

- `requestedOnMillisExact >= minimumOnMillis`: `scheduledOnMillis =
  min(round_half_up(requestedOnMillisExact), switchingWindowMillis)`
  (nächste ganze Millisekunde, `.5` aufgerundet, defensiv auf
  `switchingWindowMillis` begrenzt – siehe Abschnitt 13 für den Beweis,
  dass diese Grenze durch die Parametervalidierung strukturell nie greifen
  muss, aber als zweite Verteidigungslinie gegen Rundungs-/
  Konvertierungsfehler bestehen bleibt, löst R3.7), direkt geplant
  (`ScheduledWithinWindow`); der Akkumulator wird bei diesem Fensterstart
  nicht zusätzlich gefüttert.
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

Die neue Quote wird **ausschließlich** bei einem Fensterstart-Ereignis
gelesen – ein während eines laufenden Fensters neu angenommenes
`acceptedCommand` mit unveränderter Richtung ändert `activeWindow` nicht
rückwirkend (Fall 2 der Arming-Regel oben); seine Quote wirkt erst am
nächsten Fensterstart-Ereignis.

**Fensterfortschritt (overflow-sicher, O(1)).** Solange `activeWindow`
besteht und seine Richtung unverändert bleibt (Fall 2 oben), verwendet kein
Schritt dieser Berechnung eine ungeprüfte Deadline-Addition:

```cpp
if (now < activeWindow->startMonotonicMillis) {
    // Retrograde -> Rang „TimeInvalid" (Abschnitt 8.2 Rang 1-aequivalent)
}
elapsed = now - activeWindow->startMonotonicMillis;
if (elapsed >= switchingWindowMillis) {
    windowsElapsed = elapsed / switchingWindowMillis;              // ganzzahlig, >= 1
    activeWindow->startMonotonicMillis += windowsElapsed * switchingWindowMillis;
    // Beweis: windowsElapsed * switchingWindowMillis <= elapsed <= now (da elapsed = now - startMonotonicMillis),
    // also startMonotonicMillis(neu) <= startMonotonicMillis(alt) + elapsed = now.
    // Die Summe kann folglich nicht ueber einen bereits gueltigen now-Wert hinaus ueberlaufen.
    // -> genau EIN Fensterstart-Ereignis wird fuer den neuen startMonotonicMillis ausgewertet,
    //    activeWindow->scheduledOnMillis wird dabei aus dem dann aktuellen acceptedCommand neu gelesen.
}
elapsedInWindow = now - activeWindow->startMonotonicMillis;   // < switchingWindowMillis
```

Diese Berechnung ist **O(1)** unabhängig davon, wie lange die Pause zwischen
zwei Ticks war; es gibt keine Iterationsschleife und keine künstliche
Iterationsobergrenze. **Nicht-Nachhol-Semantik:** Ist `windowsElapsed > 1`
(ungewöhnlich lange Pause), werden die dazwischenliegenden, nicht
beobachteten Fenster **nicht** einzeln nachgeholt oder rückwirkend
akkumuliert – es wird genau ein Fensterstart-Ereignis für das aktuell
gültige, neu berechnete `startMonotonicMillis` mit der dann aktuellen Quote
des `acceptedCommand` ausgewertet. Das ist bewusst konservativ: ein langer,
unbeobachteter Zeitraum darf kein rückwirkend „nachgeholtes" Guthaben
erzeugen (dieselbe Nicht-Umgehungslogik wie in Abschnitt 7).

**Physischer Wunschzustand.** Für einen beliebigen Tick innerhalb eines
Fensters:

```text
desiredActive = activeWindow.has_value() && elapsedInWindow < activeWindow->scheduledOnMillis
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

Jeder Tick wird gegen genau eine dieser Stufen ausgewertet (höchste
zutreffende Stufe gewinnt; alle Verwurfsangaben in Rang 1–7 sind
**unbedingt** und **einheitlich**: Akkumulator, `activeWindow`,
Gegenrichtungskandidat und `acceptedCommand` werden gemeinsam verworfen –
es gibt **keine** dieser Stufen, die den Akkumulator „unverändert hält",
während gleichzeitig `acceptedCommand` verworfen wird):

| Rang | Bedingung | Status | Reason | Physisch | Verwurf |
|---:|---|---|---|---|---|
| 1 | Phase A liefert `admissionOutcome == MalformedCandidate` **oder** `input.safetyGate.status` ist strukturell ungültig **oder** `input.currentCanonicalContext` ist strukturell ungültig | `InvalidInput` | `MalformedInput` | `Idle` sofort | unbedingt: Akkumulator, `activeWindow`, Gegenrichtungskandidat, `acceptedCommand` |
| 2 | `classifyActuatorPlannerParameters(parameters_) == Unconfigured` | `Unconfigured` | `NoCommissioning` | `Idle` | unbedingt (identisch zu Rang 1) |
| 2 | `classifyActuatorPlannerParameters(parameters_) == Invalid` | `InvalidInput` | `InvalidConfiguration` | `Idle` | unbedingt (identisch zu Rang 1) |
| 3 | `safetyGate.status == Unresolved` | `Idle` | `SafetyGateUnresolved` | `Idle` sofort, überstimmt Mindest-Einschaltzeit | unbedingt |
| 3 | `safetyGate.status == ImmediateStop` | `Idle` | `ExternalSafetyOverride` | `Idle` sofort, überstimmt Mindest-Einschaltzeit | unbedingt |
| 4 | `state().latchedWatchdogFault.has_value()` | `Idle` | `RequestWatchdogFaultLatched` | `Idle` | bereits leer (Verwurf erfolgte im Trip-Tick, Rang 5) |
| 5 | laufender Watchdog löst diesen Tick aus (Abschnitt 6.4) | `Idle` | `StaleRequestWatchdog` | `Idle` sofort, überstimmt Mindest-Einschaltzeit | unbedingt; zusätzlich wird `latchedWatchdogFault` gesetzt |
| 6 | Phase-A-Kandidat vorhanden mit `demandClass == NoValidRequest` (explizite neue `Unavailable`/`InvalidInput`-Evaluation) | `Idle` | `NoValidRequest` | `Idle` sofort, überstimmt Mindest-Einschaltzeit | unbedingt |
| 7 | `acceptedCommand` vorhanden, aber `contextAtAcceptance != input.currentCanonicalContext` (bei jedem Tick geprüft, nicht nur bei Ankunft) | `Idle` | `StaleRequestContext` | `Idle` sofort | unbedingt |
| 8 | kein `acceptedCommand` vorhanden (nach den obigen Prüfungen, oder von Anfang an) | `Idle` | `NoValidRequest` | `Idle` | leer (nichts zu verwerfen) |
| 9 | `activeWindow` vorhanden **und** `elapsedInWindow < activeWindow->scheduledOnMillis` (physisch mid-on-phase) **und** ein Teardown-Ereignis liegt vor (`acceptedCommand.direction != activeWindow->direction`, z. B. weil `acceptedCommand` inzwischen `Idle` ist oder eine bestätigte Gegenrichtung zur Freigabe ansteht) **und** `NOT deadlineReached(now, currentOnPhaseStartedAtMonotonicMillis, minimumOnMillis)` | `Active` (alte Richtung bleibt) | `MinimumOnTimeHeld` | bisherige Richtung bleibt an (identisch zu reiner Fensterarithmetik) | keiner (`activeWindow` bleibt unverändert bestehen, Teardown wird verschoben) |
| 10 | `activeWindow` leer oder physisch bereits im Aus-Anteil des Fensters, `acceptedCommand.direction` will Heating/Cooling starten, aber die Arming-Regel (8.1) ist noch nicht erfüllt (Mindest-Auszeit und/oder Totzeit noch aktiv, je nach Fall (b)/(c)) | `Idle` | `MinimumOffTimeHeld` bzw. `PolarityDeadTimeHeld` | `Idle` | Teardown des alten `activeWindow`, falls noch vorhanden und Rang 9 nicht mehr greift (8.1) |
| 11 | Arming-Regel (8.1) für `acceptedCommand.direction` erfüllt **und** — bei einem Richtungswechsel — die Gegenrichtungsbestätigung ist abgeschlossen (8.5) | `Active` | `DirectionChangeApplied` | neue Richtung startet, neues Fenster (8.1) | altes Akkumulator-/Fenster-/Kandidatguthaben bereits verworfen (Teil des vorangegangenen Teardowns) |
| 12 | `acceptedCommand.direction == Idle`, kein Mindest-Einschaltzeit-Halt aus Rang 9 | `Idle` | `NeutralIdle` bzw. `AirLimitBlocked` | `Idle` | gemäß Abschnitt 7/8.4 |
| 13 | normale Fensterauswertung (8.1), Akkumulator unter Schwelle | `Idle` | `AccumulatingBelowThreshold` | `Idle` | keiner (Akkumulator wächst) |
| 13 | normale Fensterauswertung, Mindestimpuls ausgelöst | `Active` | `MinimumPulseTriggered` | Richtung an für `minimumOnMillis` | keiner |
| 13 | normale Fensterauswertung, direkte Planung | `Active`/`Idle` gemäß `desiredActive` | `ScheduledWithinWindow` | gemäß `desiredActive` | keiner |

**Gegenrichtungsbestätigung während laufender Mindest-Einzeit oder danach
(löst R3.2):** Solange eine Gegenrichtung noch unbestätigt ist (Abschnitt
8.5), bleibt sie in Rang 9/10/11 vollständig unsichtbar – `acceptedCommand`
zeigt zwar bereits die Gegenrichtung, aber Rang 9/10/11 werten weiterhin
gegen `activeWindow->direction` (die **alte**, noch physisch laufende
Richtung) aus. Das bereits eingefrorene alte Fenster läuft dabei nach
seiner eigenen Fensterarithmetik unbeeinflusst weiter (inklusive eigener
Fenstererneuerung an eigenen Fenstergrenzen), unabhängig davon, ob die
Mindest-Einschaltzeit der alten Richtung bereits erfüllt ist oder nicht:
Ein Teardown im Sinne von Rang 9/10 tritt für die alte Richtung **nicht**
allein deshalb ein, weil eine unbestätigte Gegenrichtung vorliegt – nur ein
tatsächlich **bestätigter** Richtungswechsel (Abschnitt 8.5) macht
`acceptedCommand.direction != activeWindow->direction` zu einem
Teardown-Ereignis im Sinne von Rang 9/10. Damit ist die von R3.2 benannte
Lücke (Zustand nach Ende der Mindest-Einzeit, Bestätigung noch offen)
eindeutig aufgelöst: Die alte Richtung läuft in diesem Fall exakt wie in
Rang 13 beschrieben normal weiter, **nicht** wie in Rang 9 (das wäre nur
bei tatsächlich anstehendem, aber noch durch Mindest-Einzeit blockiertem
Teardown zutreffend). `counterDirectionConfirming = true` wird zusätzlich,
unabhängig vom physisch bestimmenden Rang, als reine Zusatzauskunft im
Ergebnis gesetzt (siehe unten) – sie ändert die Rang-Auswertung selbst
nicht, weil die physische Entscheidung durch diese Regel bereits vollständig
eindeutig ist.

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
(`currentOnPhaseStartedAtMonotonicMillis`,
`directionDeactivatedAtMonotonicMillis`, `activeWindow->startMonotonicMillis`,
`counterDirectionObservedSinceMonotonicMillis`,
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
Gegenrichtungskandidat hat **keinen** eigenen Akkumulator und akkumuliert
laut Abschnitt 8.2 (Gegenrichtungsbestätigung) zu keinem Zeitpunkt eigenes
Guthaben, solange er nicht bestätigt ist.

`accumulator` und `activeWindow` werden **gemeinsam** verworfen (identischer
Trigger-Satz, keine getrennte Buchführung mehr nötig) bei:

- jedem Rang-1-bis-7-Ereignis (Abschnitt 8.2);
- dem tatsächlichen Vollzug eines Teardowns gemäß Rang 9/10 (Abschnitt 8.1/8.2)
  – also sobald ein anstehender Teardown nicht mehr durch die
  Mindest-Einschaltzeit blockiert ist;
- `forceStop()` (Abschnitt 11).

`counterDirectionCandidate` wird **zusätzlich und eigenständig** verworfen
bei jeder Unterbrechung der Bestätigung (Abschnitt 8.5) – unabhängig davon,
ob `activeWindow` zum selben Zeitpunkt ebenfalls verworfen wird. Insbesondere
gilt: **Der Teardown der alten Richtung bei einem bestätigten
Richtungswechsel (Rang 9/10-Ende, Abschnitt 8.1 Fall (c)) verwirft
`counterDirectionCandidate` als Teil desselben Übergangs.** Nach diesem
Teardown ist die weitere Freigabe der neuen Richtung ausschließlich durch
die allgemeine Arming-Regel (8.1) bestimmt – ein separat „gemerkter",
bereits bestätigter `counterDirectionCandidate` wird **nicht** über den
Teardown hinaus als Freigabegrundlage mitgeführt; fällt die
Gegenanforderung während Bestätigung oder während der anschließenden
Mindest-Auszeit/Totzeit weg (`acceptedCommand.direction` wechselt zurück
oder wird `Idle`), findet keine Freigabe „aus alter Evidenz" statt, weil die
Arming-Regel `acceptedCommand.direction == D` **live** zum Zeitpunkt der
tatsächlichen Freigabe verlangt (löst den in R3.2 benannten
Wegfall-Fall).

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

Solange `activeWindow` besteht (`armedDirection() != Idle`) und
`acceptedCommand.direction` die entgegengesetzte Richtung anzeigt, mit
Klasse `NormalDemand` oder `AirLimitReducedDemand` und `timeQuote >=
counterDirectionConfirmationQuoteThreshold`: Falls `counterDirectionCandidate`
leer oder `!=` dieser Gegenrichtung ist, wird ein neuer Kandidat mit
`counterDirectionObservedSinceMonotonicMillis = now` gestartet
(`counterDirectionConfirming = true`). Jede Unterbrechung (Quote fällt unter
die Schwelle, Richtung wechselt zurück, wird `NoValidRequest`/`NeutralOff`/
`AirLimitBlockedOff`, oder eine der Rang-1-bis-7-Bedingungen tritt ein)
setzt `counterDirectionCandidate` sofort zurück – die alte Richtung bleibt
davon unberührt, sofern sie nicht selbst durch dasselbe Ereignis betroffen
ist. Erst wenn `deadlineReached(now,
counterDirectionObservedSinceMonotonicMillis,
counterDirectionConfirmationDurationMillis)` **ununterbrochen** erreicht
wird, gilt die Gegenrichtung als bestätigt: Ab diesem Tick zeigt Rang 9/10
`acceptedCommand.direction != activeWindow->direction` als Teardown-Ereignis
für die alte Richtung an (siehe Abschnitt 8.2); die alte Richtung wird –
vorbehaltlich einer noch laufenden eigenen Mindest-Einschaltzeit (Rang 9) –
`Idle`; danach greift Rang 10 (Mindest-Auszeit/Totzeit, Fall (c) der
Arming-Regel), bevor Rang 11 die neue Richtung tatsächlich freigibt.

Vor Ablauf der Mindest-Einschaltzeit der alten Richtung bleibt eine
Gegenanforderung – bestätigt oder nicht – wirkungslos auf die physische
Ausgabe (Rang 9 hat Vorrang vor Rang 10/11, Abschnitt 8.2); der
Bestätigungstimer läuft davon unbeeinflusst weiter, solange die
Gegenanforderung selbst ununterbrochen bestehen bleibt.

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
`lastNewRequestAcceptedAtMonotonicMillis` werden aktualisiert, und –
sofern Heating/Cooling – `pendingFeedback` wird gemäß Abschnitt 9 ebenfalls
aktualisiert), öffnet aber **keine** neue physische Freigabe, solange Rang 4
vor Rang 6/8/9 usw. ausgewertet wird.

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
`std::optional<PreviousControlRequestFeedback>` und wird aus
`state().pendingFeedback` abgeleitet. Grundlage ist wörtlich
`docs/tasks/issue-22-pi-control-air-limits-plan.md` Abschnitt 8.2
„Feedbackfenster": „Nur eine unmittelbar vorherige aktive
`Heating`-/`Cooling`-ControlRequest öffnet ein Feedbackfenster", „vorhandenes
Feedback muss exakt die letzte Request-Sequence referenzieren", „fehlendes
Feedback ... wird als konservatives Einfrieren behandelt und das Fenster
geschlossen", „Feedback für eine vorherige gültige OFF-ControlRequest ist
unzulässig ... OFF benötigt kein Anti-Windup-Feedback".

### 9.1 Entkopplung von Feedback-Subjekt und physischer Governance (löst R3.5)

`pendingFeedback` (Abschnitt 6.0) ist ein von `acceptedCommand`
**unabhängiges** Feld. `acceptedCommand` bestimmt ausschließlich, welche
Richtung/Quote physisch geplant wird (Abschnitt 8). `pendingFeedback`
bestimmt ausschließlich, welches `PreviousControlRequestFeedback` #22 im
nächsten Aufruf erhält. Beide Felder werden in Phase A oft gemeinsam,
aber nicht immer identisch aktualisiert: Eine neue, höhere aktive Request B,
die bei Admission vollständig abgelehnt wird (`StaleOnArrivalWatchdog`/
`StaleOnArrivalContext`), erzeugt ein `Rejected`-Feedbackfenster für B, ohne
`acceptedCommand` zu berühren (Abschnitt 6.2 Schritt 4).

### 9.2 Ereignisgetriebene Update-Regel (löst R3.4, exhaustiv)

Bei jeder Phase-A-Verarbeitung einer neu beobachteten (nicht-duplizierten)
Evaluation:

| Beobachtetes Ereignis | Wirkung auf `pendingFeedback` |
|---|---|
| `newEvaluation == std::nullopt` | unverändert |
| `DuplicateOrOldSequence` (Replay) | unverändert – ein Replay darf ein bereits korrektes Fenster nicht zurückdrehen |
| `MalformedCandidate` | unverändert (siehe unten für die einzige Ausnahme über den allgemeinen Verwurf-Trigger) |
| `demandClass == NoValidRequest` | `= std::nullopt` (kein Feedbackfenster) |
| `demandClass ∈ {NeutralOff, AirLimitBlockedOff}` (OFF-Richtung, Admission besteht) | `= std::nullopt` (OFF benötigt kein Anti-Windup-Feedback, schließt ein offenes Fenster) |
| `demandClass ∈ {NormalDemand, AirLimitReducedDemand}`, aber `StaleOnArrivalWatchdog`/`StaleOnArrivalContext` | `= {sequence, Rejected}` – vollständige Nichtannahme, `acceptedCommand` bleibt unverändert |
| `demandClass ∈ {NormalDemand, AirLimitReducedDemand}`, Admission vollständig bestanden | `= {sequence, Rejected}` als Startwert, danach jeden Tick live nachgeführt (siehe 9.3), solange dieser Kandidat weiterhin `acceptedCommand` ist |

Zusätzlich, **unabhängig von einer neuen Evaluation**, bei jedem Tick, in dem
`acceptedCommand` durch einen unbedingten Verwurf (Rang 1/2/3/5/7,
Abschnitt 8.2) oder durch den tatsächlichen Vollzug eines Teardowns (Rang
9/10-Ende, Abschnitt 8.1) verworfen wird, **und** `pendingFeedback.has_value()
&& pendingFeedback->sequence == acceptedCommand->sequence` (also
`pendingFeedback` genau das gerade verworfene `acceptedCommand` verfolgt):

```text
pendingFeedback->disposition = Rejected   (sequence bleibt erhalten)
```

Diese Regel gilt **nicht** für den Übergang nach `Idle` durch eine neu
angenommene OFF-/`NoValidRequest`-Request (dort greift bereits die
`nullopt`-Zeile der obigen Tabelle) und **nicht** für `forceStop()` (siehe
9.4). Sie stellt sicher, dass ein bereits als `Rejected` erkanntes Subjekt
zwischen zwei `evaluateTemperatureControl()`-Aufrufen nicht durch einen
späteren Tick verlorengeht, selbst wenn der Aktorplaner deutlich häufiger
tickt als #22 evaluiert (löst R3.6).

Ein `MalformedCandidate`-Tick löscht `pendingFeedback` **nicht** pauschal:
Ein bereits gültig ermitteltes `Rejected` für ein anderes, älteres Subjekt
bleibt bestehen, wenn Rang 1 dieses Ticks ein *anderes* `acceptedCommand`
unbedingt verwirft. Nur falls `pendingFeedback` exakt dieses soeben
verworfene `acceptedCommand` verfolgt, greift die obige „Rejected"-Regel
auch hier.

### 9.3 Live-Nachführung während laufender Governance

Für jeden Tick, in dem `pendingFeedback.has_value() &&
acceptedCommand.has_value() && pendingFeedback->sequence ==
acceptedCommand->sequence` (das Feedback-Subjekt ist die aktuell
maßgebliche Request):

```text
acceptedCommand->direction != armedDirection()
    -> disposition = DeferredOrLimited
       (deckt: Mindest-Einschaltzeit der alten Richtung noch aktiv (Rang 9),
       Mindest-Auszeit/Totzeit (Rang 10), unbestaetigte oder noch nicht
       freigegebene Gegenrichtungsbestaetigung (Abschnitt 8.5) – in all
       diesen Faellen fuehrt eine ANDERE Richtung physisch aus als die,
       die acceptedCommand fordert)

acceptedCommand->direction == armedDirection()
    -> physisch Active diesen Tick (ScheduledWithinWindow/
       MinimumPulseTriggered/DirectionChangeApplied)
           -> disposition = NoIntegratorConstraint
       physisch Idle, AccumulatingBelowThreshold
           -> disposition = DeferredOrLimited
```

Diese eine, richtungsunabhängige Regel ersetzt die ranggebundene Tabelle aus
Revision 3 und macht die Disposition auch für den in R3.2 benannten
Zwischenzustand (Gegenrichtung wartet auf Bestätigung, alte Richtung läuft
nach eigener Fensterlogik weiter) eindeutig: `armedDirection()` bleibt in
diesem Zustand die alte Richtung, `acceptedCommand->direction` bereits die
neue – die Bedingung der ersten Zeile trifft zu, Disposition ist
`DeferredOrLimited`, exakt wie von R3.2 gefordert.

### 9.4 `forceStop()` und Lifecycle-Grenzen

`forceStop()` (Abschnitt 11) setzt `pendingFeedback = std::nullopt`
unbedingt – unabhängig davon, welchen Wert es zuvor trug. Dies ist bewusst
symmetrisch zu #22s eigenem Verhalten an denselben Lifecycle-Grenzen: Jede
der sieben `TemperatureControlLifecycleBoundary`-Werte löst über
`resetTemperatureControlAtBoundary()` bereits einen vollständigen Reset des
#22-eigenen Integrator-/Anker-Zustands aus, sodass #22 nach einer solchen
Grenze ohnehin kein Feedback zu einer Request aus der vorherigen
Laufphase mehr erwartet.

### 9.5 `ActuatorAdmissionOutcome` als eigener, von `ActuatorPlanReason` getrennter Diagnosevertrag

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
`reason` (welches die physische Tick-Entscheidung beschreibt) und getrennt
von `pendingFeedback`/`feedbackForAcceptedRequest` (welches das #22-Feedback
beschreibt). Alle drei Felder beantworten unterschiedliche Fragen: „Was
geschah mit dem gerade eingetroffenen Kandidaten?", „Warum ist der physische
Ausgang dieses Ticks so, wie er ist?" und „Welches Feedback erhält #22 als
nächstes?".

### 9.6 n/n+1/n+2-Beispiel (Heating -> Cooling, inklusive Admission-Reject-Fall)

```text
#22 Evaluation n   -> Heating Request A (sequence=A)
   Phase A: neu, alle Pruefungen bestanden -> acceptedCommand = A,
            pendingFeedback = {A, Rejected} (vorlaeufig)
   Rang 11 (Fall a oder b): activeWindow armiert fuer Heating
   pendingFeedback lebt fortan mit: acceptedCommand.direction(Heating) ==
     armedDirection()(Heating) -> disposition folgt dem physischen Rang
     (z. B. ScheduledWithinWindow -> NoIntegratorConstraint)
   -> vor #22-Aufruf n+1 liegt pendingFeedback = {A, NoIntegratorConstraint}
      (oder DeferredOrLimited, je nach Rang dieses Ticks)

#22 Evaluation n+1 -> Cooling Request B (sequence=B > A)
   Phase A: neu, alle Pruefungen bestanden -> acceptedCommand = B (sofort,
            unabhaengig vom physischen Zustand), pendingFeedback = {B, Rejected}
            (vorlaeufig, ersetzt den A-Eintrag)
   armedDirection() bleibt Heating (activeWindow der alten Richtung laeuft
     unveraendert weiter, Abschnitt 8.2 Gegenrichtungsbestaetigung)
   acceptedCommand.direction(Cooling) != armedDirection()(Heating)
     -> pendingFeedback.disposition = DeferredOrLimited, jeden Tick live
        nachgefuehrt, bis B entweder physisch uebernimmt (armedDirection()
        wird Cooling) oder durch eine neuere Request C ersetzt wird
   -> vor #22-Aufruf n+2 erhaelt #22 das zuletzt berechnete Feedback fuer B
      (DeferredOrLimited), nicht fuer A

Admission-Reject-Variante (loest R3.5 explizit):
#22 Evaluation n+1 -> Cooling Request B, aber B ist StaleOnArrivalContext
   Phase A: pendingFeedback = {B, Rejected}, acceptedCommand bleibt A
   -> vor #22-Aufruf n+2 erhaelt #22 Rejected fuer B, NICHT fuer A - #22
      erwartet exakt dieses Feedback fuer B (seine zuletzt erzeugte aktive
      Request) und wuerde ein Feedback fuer A als fremd/alt einstufen
```

Spiegelbildlich für Cooling -> Heating.

## 10. Lüfterlogik

- **Außenlüfter**: `outerFanEnabled = true`, sobald `armedDirection() !=
  Idle`. Beim Übergang auf physisch `Idle` wird
  `outerFanDeactivationRequestedAtMonotonicMillis = now` gesetzt;
  `outerFanEnabled` bleibt `true`, bis `deadlineReached(now,
  outerFanDeactivationRequestedAtMonotonicMillis, outerFanPostRunMillis)`.
  Eine erneute Freigabe während des Nachlaufs setzt
  `outerFanDeactivationRequestedAtMonotonicMillis` auf `std::nullopt`
  zurück, ohne dass der Lüfter zwischenzeitlich `false` war. Kein Vorlauf:
  `outerFanEnabled` wird im selben `tick()`-Aufruf gesetzt wie die
  Peltierfreigabe. Der Nachlauf ist laut `ACTUATOR_TIMING.md` (Zeile 186,
  220) **zwingend**: `outerFanPostRunMillis > 0` ist eine strukturelle
  Parametervoraussetzung (Abschnitt 13).
- **Innenlüfter**: `innerFanEnabled = true`, solange
  `input.temperatureControlledPhase == true` – unabhängig vom aktuellen
  Peltier-Fensterzustand. Beim Verlassen der temperaturgeregelten Phase
  startet ein eigener, unabhängiger Nachlauf. Kurze Peltier-Auszeiten
  *innerhalb* einer weiterhin temperaturgeregelten Phase lösen keinen
  Innenlüfter-Nachlauf aus. Der Innenlüfter-Nachlauf ist laut
  `ACTUATOR_TIMING.md` (Zeile 223-244) ausdrücklich **konfigurierbar**, aber
  – anders als beim Außenlüfter – an keiner Stelle als „zwingend"
  bezeichnet und besitzt dort auch keine explizit genannte firmwarefeste
  Mindestgrenze; `innerFanPostRunMillis == 0` („kein Nachlauf") bleibt daher
  strukturell zulässig (Abschnitt 13).

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
   Akkumulator/`activeWindow`/Gegenrichtungskandidat/`acceptedCommand`/
   `pendingFeedback` werden verworfen (Abschnitt 8.4, 9.4);
   `lastDeactivatedDirection`/`directionDeactivatedAtMonotonicMillis` werden
   gesetzt, sofern zuvor eine Richtung armiert war (Abschnitt 8.1); ein
   bereits laufender Außenlüfter-Nachlauf wird **nicht** verkürzt oder
   abrupt beendet, sondern exakt wie bei einem gewöhnlichen Übergang auf
   `Idle` fortgeschrieben (`outerFanDeactivationRequestedAtMonotonicMillis`
   wird gesetzt, nicht der Lüfter direkt auf `false`).
2. `driver.apply(result);` – dieselbe Übersetzungsfunktion wie bei jedem
   gewöhnlichen Tick (Abschnitt 12); keine zweite Ausgabelogik.
3. Danach: `armedDirection() == Idle`, `state().acceptedCommand ==
   std::nullopt`, `state().activeWindow == std::nullopt`,
   `state().pendingFeedback == std::nullopt`; `latchedWatchdogFault` bleibt
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
  (10) outerFanPostRunMillis > 0
       (strukturell zwingend laut ACTUATOR_TIMING.md Zeile 186/220 -
       "zwingender Nachlauf" mit firmwarefesten Mindest-/Maximalgrenzen;
       die konkrete Grenze selbst bleibt TBD_COMMISSIONING/#35, loest R3.7)
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
       erreicht wird (loest den Konvertierungs-/Ueberlaufteil von R3.7).
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
zwischenzeitlich geändert wurde. Das ist exakt die offene Lücke, die über
Issue #106 verfolgt wird; sie wird hier bewusst **nicht** als gelöst
behauptet.

### 14.3 Benanntes, blockierendes Integrationsgate: Issue #106

**Issue #106** „Aktorplaner Per-Run-Parameter-Snapshot und
Recovery-Bindung" ist Abhängigkeit dieses Plans für jede produktive
Verdrahtung. Die Live-Beschreibung von Issue #106 wurde vor diesem
Plan-Commit präzisiert (Producer-Eigentümerschaft, Laufpersistenz-/
Schema-Evolutionssemantik, Write-before-Apply-Reihenfolge, vollständige
Abhängigkeiten/Quellen, erweiterte Akzeptanzkriterien) und definiert
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
  Implementierung für Tick- und Stop-Pfad; `armedDirection()` als reine
  Ableitung aus `activeWindow` statt eines zweiten, redundanten
  Richtungsfeldes; ein gemeinsamer Verwurf-Trigger-Satz für Akkumulator und
  Fenster (Abschnitt 8.4) statt getrennter, potenziell divergierender
  Regeln.
- **KISS:** `ActuatorSafetyGateInput` bleibt ein einfacher Werttyp; kein
  zweiter Test-Recorder in Produktionscode (Decorator bleibt test-only);
  die Feedback-Dispositionsregel (Abschnitt 9.3) ist eine einzige,
  richtungsunabhängige Regel statt einer ranggebundenen Tabelle mit vielen
  Einzelfällen; keine vorsorgliche Generalisierung ohne aktuellen Bedarf
  (z. B. kein zusätzlicher `ActuatorPlanReason`-Wert für die
  Gegenrichtungsbestätigung, da die physische Entscheidung bereits durch
  die bestehenden Ränge eindeutig beschrieben ist, Abschnitt 8.2).

## 17. Safety-, Security-, Recovery- und Hardwaregrenzen

- Fail-closed bei jedem Rang 1–8 aus Abschnitt 8.2: `Idle`, kein Guthaben-
  Nachholen.
- Keine Aktorfreigabe wird bei Boot, Reset, Fehler, unbekanntem Zustand oder
  unbestätigter Hardware vorausgesetzt; `ActuatorSafetyGateInput` startet mit
  `Unresolved` und erzwingt `Idle`. Ein strukturell ungültiger
  `ActuatorSafetyGateStatus`-Wert (z. B. durch fehlerhaftes Casting) wird
  niemals wie `Allowed` behandelt, sondern fail-closed über Rang 1
  (`MalformedInput`) abgefangen (Abschnitt 4.2, 8.2).
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
- Ein anstehender Teardown (OFF, Kontext-Verlust, bestätigter
  Richtungswechsel) darf eine noch nicht erfüllte Mindest-Einschaltzeit
  niemals verkürzen (Abschnitt 8.1/8.2 Rang 9, korrigierte Zeitbasis über
  `currentOnPhaseStartedAtMonotonicMillis`); ausschließlich Safety-/
  Fault-Ränge (1/2/3/5/6) dürfen die Mindest-Einschaltzeit überstimmen.
- Keine Hardwarewerte werden in diesem Plan festgelegt; `OPEN_POINTS.md`
  (#29, #32, #33, #35) bleibt unverändert sichtbar offen.

## 18. Umsetzungs- und Commit-Schnitte

1. **Gemeinsame schmale Verträge / Klassifikation** – `actuator_plan_types.hpp`
   vollständig (Abschnitt 4.2, 7, 8.2, 8.3, 8.4, 8.6, 9.5, 13),
   `classifyActuatorDemand()`, `classifyActuatorPlannerParameters()`
   inklusive vollständiger struktureller Invarianten; keine
   Verhaltenslogik.
2. **Phase A / Annahme** – `ActuatorPlanner::tick()` Grundgerüst: Admission
   (Abschnitt 6.2), laufender Watchdog (6.4), Prioritätsleiter Rang 1–8
   (8.2), overflow-sichere Zeitarithmetik (8.3), `forceStop()`,
   `applyExternalWatchdogFaultReset()`.
3. **Fenster, Akkumulator, Mindestzeiten, Totzeit, Richtungswechsel** –
   Fensterlogik inklusive Arming-Regel und O(1)-Fortschritt (8.1), einziger
   Akkumulator mit vollständigen Verwurfsregeln (8.4), bestätigter
   Richtungswechsel (8.5), Prioritätsleiter Rang 9–13 vollständig.
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

**Fensterzustand vollständig im Runtime-State (R3.1):** Heating-Fenster
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

**Gegenrichtungsbestätigung nach Ende der Mindest-Einzeit (R3.2):** vor
Minimum-On-Ende; exakt Minimum-On-Ende; danach, Bestätigung noch offen
(altes Fenster läuft nach eigener Fensterlogik unverändert weiter,
`counterDirectionConfirming == true`, physischer Ausgang folgt exakt der
alten Richtung); exakt Bestätigungsdauer; Abbruch kurz davor (keine
Freigabe aus alter Evidenz, auch nicht nach anschließender Mindest-Auszeit/
Totzeit); bestätigte Gegenrichtung während Peltier bereits `Idle`; beide
Richtungen symmetrisch; Feedback für die aktive Gegenrichtungsrequest ist
während der gesamten Sperre `DeferredOrLimited` (Abschnitt 9.3).

**Explizite `NoValidRequest`-Evaluation:** eine aktive
Heating-/Cooling-Freigabe innerhalb ihrer Mindest-Einschaltzeit wird durch
eine neue, strukturell gültige `Unavailable`/`InvalidInput`-Evaluation
sofort beendet (Rang 6); ein `tick()`-Aufruf mit `newEvaluation =
std::nullopt` im selben Szenario tut dies ausdrücklich **nicht** und führt
die laufende Freigabe unverändert fort.

**Feedback-Vertrag (R3.4/R3.5/R3.6):** n/n+1/n+2-Orakel für Heating->Cooling
und Cooling->Heating exakt gemäß Abschnitt 9.6, inklusive der
Admission-Reject-Variante (neue, höhere Request B wird als
`StaleOnArrivalWatchdog`/`StaleOnArrivalContext` abgelehnt: `pendingFeedback`
zeigt `{B, Rejected}`, nicht A); OFF-Admission erzeugt kein nachträgliches
Feedback für die zuvor aktive Request (`pendingFeedback = nullopt`); jede
Zeile der Tabelle aus Abschnitt 9.2 einzeln; ein `Rejected`-Feedback
überlebt mehrere `tick()`-Aufrufe ohne neue Evaluation unverändert (direkter
Mehr-Tick-Test zwischen zwei `evaluate()`-Aufrufen, löst R3.6); ein
`MalformedCandidate`-Tick überschreibt ein bereits gesetztes `Rejected`
eines **anderen** Subjekts nicht, überschreibt es aber korrekt mit
`Rejected`, falls Rang 1 desselben Ticks genau das von `pendingFeedback`
verfolgte `acceptedCommand` verwirft; `forceStop()` setzt `pendingFeedback`
unbedingt auf `nullopt`, unabhängig vom vorherigen Wert;
`admissionOutcome` ist für jeden Fall aus Abschnitt 9.5 direkt geprüft.

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

**Safety-Gate:** `Unresolved`, `Allowed`, `ImmediateStop`; `ImmediateStop`
überstimmt eine aktive Mindest-Einschaltzeit; `Unresolved` erzwingt `Idle`
auch bei ansonsten vollständig gültiger Request; **unbekannter Enumwert**
`static_cast<ActuatorSafetyGateStatus>(0xFF)` wird niemals wie `Allowed`
behandelt, sondern erzeugt `InvalidInput`/`MalformedInput` mit sofortigem
`Idle` und unbedingtem Verwurf (Rang 1, löst R3.8).

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

**Mindestzeiten/Totzeit (R3.3):** Mindest-Einschaltzeit hält aktive Richtung
trotz neuer `Idle`-Anforderung (sofern nicht Rang 6 zutrifft); Mindest-
Auszeit verhindert verfrühte erneute Freigabe bei gleichgerichtetem Neustart
(nur Mindest-Auszeit, keine Totzeit erforderlich); Polaritätstotzeit
zusätzlich zur Mindest-Auszeit ausschließlich bei tatsächlichem
Richtungswechsel (Späteres-Ende-Regel, beide Richtungen sowie exakter
Gleichstand-Grenzfall); Heizen -> Kühlen und Kühlen -> Heizen symmetrisch;
niemals gleichzeitig `Forward` und `Reverse` über eine lange Sequenz; alle
fünf in Abschnitt 8.1 benannten Fälle (Erststart, normales Window-Off,
explizite OFF-Request, gleichgerichteter Restart, Richtungswechsel) einzeln
und in Kombination getestet.

**Parameterklassifikation:** vollständige Tabelle aus Abschnitt 13 (alle
Felder `0` -> `Unconfigured`; jede einzelne strukturell unmögliche Relation
1-12 einzeln getestet -> `Invalid`, inklusive `outerFanPostRunMillis == 0`
(jetzt `Invalid`, löst R3.7) und `innerFanPostRunMillis == 0` (weiterhin
`Valid`, sofern alle anderen Relationen erfüllt sind); vollständig
konsistente Testwerte -> `Valid`); Rang-2-Verwurf ist für `Unconfigured` und
`Invalid` identisch unbedingt.

**Zeit-/Overflow-Verträge:** Gleichheit an jeder Frist; knapp
davor/genau darauf/knapp danach für Mindest-Ein-/Auszeit, Totzeit,
Bestätigungsdauer, Watchdog, Fan-Nachlauf; Retrograde-Zeit an jeder
Referenzzeit (-> `TimeInvalid`); Werte nahe `UINT64_MAX`.

**Priorität/Fail-closed:** malformed `ControlRequest` (Sequenz `0`,
unbekannte Richtung, `timeQuote` `NaN`/`Infinity`/außerhalb `[0,1]`,
Status/Request-Mismatch, ungültiger Kontext) -> `MalformedInput`,
sofortiger Verwurf; strukturell ungültiger `currentCanonicalContext` ->
ebenfalls `MalformedInput` (Rang 1, löst R3.8); mehrere gleichzeitig
zutreffende Bedingungen (z. B. Safety `ImmediateStop` **und** Kontext-Stale
gleichzeitig) -> exakte Rang-Reihenfolge aus 8.2.

**Lifecycle/Stop:** für jede der sieben `TemperatureControlLifecycleBoundary`-
Werte: `forceStop()` liefert korrekten Nachlauf, verwirft Akkumulator/
`activeWindow`/Gegenrichtungskandidat/`acceptedCommand`/`pendingFeedback`,
setzt `lastDeactivatedDirection`/`directionDeactivatedAtMonotonicMillis`
(sofern zuvor armiert), lässt `latchedWatchdogFault` unverändert; „aktives
Peltier -> `Fault`" und „aktives Peltier -> `Standby`" mit weiterlaufendem
Außenlüfter-Nachlauf statt abruptem Fan-Stopp; ein `forceStop()` mit
anschließend sofort wieder gleichgerichteter Anforderung respektiert
weiterhin die Mindest-Auszeit (kein impliziter Fall-(a)-Erststart nach
`forceStop()`, da `directionDeactivatedAtMonotonicMillis` dabei gesetzt
wird).

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
  Kette ab, und ruft `planner.tick()`/`driver.apply()` in der in Abschnitt
  6.1 festgelegten Reihenfolge genau je einmal auf – sowohl im normalen als
  auch im fail-closed Fall (löst R3.9);
- `complete()`/`needsRuntimeReset()` ruft `resetActuatorPlanAtBoundary()` für
  dieselbe committed Lifecycle-Grenze auf wie
  `resetTemperatureControlAtBoundary()` (ein einziger Test-Fixture-Aufruf
  löst nachweislich beide Resets aus);
- **Einzelverbrauch des Feedback-Handoffs (löst R3.6, direkter Test des
  Orchestrator-Vertrags aus Abschnitt 6.1):** mehrere aufeinanderfolgende
  `tickActuatorPlan()`-Aufrufe zwischen zwei
  `evaluateTemperatureControl()`-Aufrufen liefern denselben, weiterhin
  gültigen `feedbackForAcceptedRequest`-Wert zurück; das Test-Fixture ruft
  `evaluateTemperatureControl()` exakt einmal pro tatsächlich neuer
  #22-Evaluation auf und weist nach, dass genau der zuletzt von
  `tickActuatorPlan()` gelieferte Wert (nicht ein älterer) in
  `TemperatureControlEvaluationEvidence.previousControlRequestFeedback`
  ankommt;
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
- **Governance:** Issue #106 wurde live aktualisiert, **bevor** dieser
  Plan-Commit erstellt wurde (eigenständiger Governance-Schritt, siehe
  PR-Body). Diese Revision-4-Planänderung wird als eigener, abgeschlossener
  Plan-Commit committet. Erst danach ist die exakte Revision-4-Plan-SHA
  bekannt und wird im Draft-PR #105 und im `SESSION HANDOVER` ausgewiesen.
  Eine Nachführung von `docs/ROADMAP.md` auf die exakte Revision-4-SHA
  erfolgt in einem separaten, rein redaktionellen Metadaten-Commit direkt
  im Anschluss – nicht durch eine fünfte Planrevision.

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
  Diese Revision ersetzt die ranggebundene Feedback-Tabelle aus Revision 3
  durch eine einzige, richtungsunabhängige Live-Nachführungsregel
  (Abschnitt 9.3). Sollte sich bei der Umsetzung zeigen, dass der
  tatsächliche #22-Code (nicht nur der Plan) an einer Stelle eine andere
  Disposition erwartet als hier hergeleitet, ist das ein materieller Befund
  gegen den bestehenden #22-Code (nicht gegen diesen Plan) und wird als
  Abweichung gemeldet, nicht still umgangen.
- **Asymmetrische Mindest-Auszeit-/Totzeit-Anwendung** (Abschnitt 8.1, Fall
  (b) vs. (c)): Diese Revision wendet die Polaritätstotzeit ausschließlich
  bei tatsächlichem Richtungswechsel an, nicht bei gleichgerichtetem
  Neustart. Dies ist eine gegenüber Revision 3 präzisierte, durch die
  physische Funktion der Totzeit (Polaritätsschutz) begründete
  Verhaltensänderung; sie wird hier als Teil der Auflösung von R3.3
  offengelegt, nicht als stille Vereinfachung behandelt.
