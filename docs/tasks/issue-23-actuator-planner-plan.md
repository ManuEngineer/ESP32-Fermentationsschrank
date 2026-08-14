# Plan: Issue #23 – Aktorplaner, Mindestzeiten, Totzeit und Lüfterlogik

## 1. Status, Scope und Owner-Gate

- Revision: **7**. Ersetzt Revision 6
  (`62cd53c9f727e00e24c1ed6f99e400af059f1b24`) vollständig. Diese Revision ist
  ohne Rückgriff auf Revision 1, 2, 3, 4, 5 oder 6
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
- Die Umsetzung bleibt gesperrt, bis der Owner exakt den nach diesem Commit
  bekannten Revision-7-Plan-Commit mit `PLAN APPROVED: <SHA>` freigibt.
- Diese Revision committet ausschließlich Plandokumentation. Sie implementiert
  keine Produktionslogik, keine produktiven Tests, keine Hardware-, GPIO-,
  Toolchain- oder CI-Änderung.
- Der PR bleibt Draft. Es gibt kein `Ready for review`, keinen Merge, kein
  Auto-Merge und kein Branch-Löschen. Issue #23 wird nicht geschlossen.

```text
CONTEXT_BASELINE_BRANCH: agent/issue-23-aktorplaner-plan
CONTEXT_BASELINE_SHA: 2986dca5736a34171910c9245a3d5f43fa55da06
CONTEXT_HEAD_BEFORE_REVISION: cd49a6131b4e98bcaae28a52a067fbde96e45c50 (Roadmap-Metadaten-Commit nach Revision 6)
CONTEXT_PLAN_SHA: 62cd53c9f727e00e24c1ed6f99e400af059f1b24 (vollstaendig zu ersetzender Revision-6-Plan)
CONTEXT_REFRESH_MODE: FULL
CONTEXT_DELTA: Vollständiges Owner-Review von Revision 5 mit den Befunden
  R5.1-R5.8 wurde erhalten. Issue #106 wurde vor Revision 5 separat live um
  I106.R1 präzisiert, ist seitdem unverändert offen und wird in dieser Revision
  nicht geändert. Die bisherigen R4.1-R4.6- und I106.R1-Verträge bleiben
  erhalten. Die neuen Befunde aus Revision 6 sind in dieser vollständigen
  Revision 7 konsistent erhalten und erweitert. Revision 7 ergänzt zusätzlich:
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
  R5.1 -> die zentrale H-Regel in Phase A aktualisiert das
    `lastNewRequestAcceptedAtMonotonicMillis`-Lebenszeichen genau für jede
    neue, strukturell gültige und nicht stale-on-arrival #22-Evaluation,
    einschließlich `NoValidRequest`; nullopt, Replay, malformed und
    stale-on-arrival verlängern die Frist nicht.
  R5.2 -> ein normaler Puls startet nur, wenn die verbleibende natürliche
    On-Zeit am ersten Aktivierungstick mindestens `minimumOnMillis` beträgt;
    ein verspätetes Fenster wird vollständig verworfen, ohne Verlängerung oder
    Nachholung.
  R5.3 -> die Feedbackdisposition unterscheidet planmäßigen Window-Off als
    vertragsgemäße Ausführung von downstream Begrenzungen; nur letztere sind
    `DeferredOrLimited`.
  R5.4 -> die Dispositionsschwere ist je Request-Sequence monoton
    `NoIntegratorConstraint < DeferredOrLimited < Rejected`; ein regulärer
    Window-Off-Anteil verschärft die Disposition nicht.
  R5.5 -> eine exhaustive Trusted-Sequence-Regel vereinheitlicht
    Prioritätstabelle, Feldlebenszeit, Phase A, Abschnitt 9 und `forceStop()`;
    korrupte Safety-Evidence bleibt von einer validen Request-Identität
    getrennt.
  R5.6 -> der Orchestrator verwaltet ein `outstandingEvaluation` und verhindert
    eine zweite #22-Evaluation, bevor die vorige genau einmal intern an den
    Planner übergeben oder an einer terminalen Lifecycle-Grenze fail-closed
    abgeschrieben wurde.
  R5.7 -> die Entfernung von
    `TemperatureControlEvaluationEvidence::previousControlRequestFeedback`
    ist als öffentlicher #23-Integrationsschritt mit vollständiger
    Repository-Callsite-Suche, Migration und Regressionstests im Scope.
  R5.8 -> nach dem Plan-Commit wird die Roadmap in einem separaten
    redaktionellen Metadaten-Commit auf die exakte Revision-7-SHA synchronisiert;
    alle alten Revision-4/5-Statusstellen werden geprüft.
  R6.1 -> eine neue `outstandingEvaluation`-Episode schließt das vorige
    #22-Feedbackfenster bereits vor dem Planner-Tick; nur die neue
    vertrauenswürdige aktive Sequence darf neues Feedbacksubjekt werden.
    OFF/NoValidRequest erzeugen kein neues Fenster, stale-on-arrival erzeugt
    ausschließlich `{sequence, Rejected}`, malformed Identität kein
    Sequence-Feedback. `forceStop()` darf nur ein noch offenes
    Feedbacksubjekt verschärfen; ein vorhandenes `outstandingEvaluation`
    sperrt die Resurrektion des alten `acceptedCommand`.
  R6.2 -> Variante A: `ActiveSwitchingWindow::sourceRequestSequence` beweist
    die Window-Ownership. Ein A-Fenster bleibt A, eine gleichgerichtete
    Mid-window-B-Request wird nicht als ausgeführt behauptet, und ein
    Folgefenster aus B trägt B als Quelle. Das Ownership-Gate ist in der
    Feedbackmatrix und den Tests ausdrücklich nachgewiesen.
  R6.3 -> `watchdogEpisodeStartedAtMonotonicMillis` definiert den frischen
    Startanker pro überwachte Temperaturregel-Episode. Vor dem ersten H gilt
    der Episodenanker, danach das letzte H; beim Verlassen wird der
    Heartbeat-/Episodenanker gelöscht, beim Wiedereintritt neu gesetzt.
    `latchedWatchdogFault` bleibt über alle Rebasings erhalten.
  R6.4 -> `CounterDirectionConfirming`, `WindowPulseDeferred` und
    `WindowPulseMissed` sind echte
    `ActuatorPlanReason`-Werte. `DeferredOrLimited` bleibt ausschließlich
    Feedback-Disposition; die N-5-Tabelle ist typgetrennt und exhaustiv.
  R6.5 -> das Replay-Hochwasserzeichen heißt
    `lastObservedSequenceHighWatermark`; die Watchdog-Evidenz bezeichnet ihr
    Feld ebenfalls ehrlich als Hochwasserzeichen und behauptet keine
    erfolgreiche H-Akzeptanz für stale-on-arrival Sequences.
  Governance -> Revision 7 wird als eigener Plancommit erstellt, der PR-Body
    danach auf die exakte Revision-7-SHA gesetzt, die Roadmap anschließend in
    einem separaten rein redaktionellen Metadatencommit synchronisiert und ein
    neuer Handover veröffentlicht.
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
- Keine Änderung an der PI-/Luftbegrenzungsmathematik oder ihren #22-Orakeln
  (`temperature_control.cpp`, `control_context.*`); `ControlRequest`,
  `ControlRequestContext`, `ControlSensorRole`, `AbstractControlDirection`,
  `TemperatureControlResult` und `PreviousControlRequestFeedback` werden
  ausschließlich wiederverwendet. Die öffentliche Application-Evidence-API
  wird jedoch als notwendiger #23-Integrationsschritt geändert: Das Feld
  `TemperatureControlEvaluationEvidence::previousControlRequestFeedback`
  entfällt, und der Orchestrator injiziert das interne Handoff in das private
  #22-Input. Alle direkt betroffenen Callsites und Regressionstests werden
  migriert (Abschnitte 5, 6.1, 18 und 19); dies ist keine Änderung der
  eigentlichen #22-PI-Mathematik.
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
| `TemperatureControlEvaluationEvidence` | `temperature_control_orchestrator.hpp` | öffentliche Evidence-Grenze für Zeit/Sensoren; `previousControlRequestFeedback` wird in diesem #23-Integrationsschritt entfernt, Feedback injiziert ausschließlich der Orchestrator |
| `IBidirectionalActuatorSink` | `device_platform/bidirectional_actuator_sink.hpp` | bereits vorhandener, bisher unbenutzter Peltier-Port |
| `IBinaryOutputSink` | `device_platform/binary_output_sink.hpp` | bereits vorhandener, bisher unbenutzter Lüfter-Port (zwei Instanzen: außen/innen) |
| `ITimeSource`, `VirtualTimeSource` | `device_platform/time_source.hpp`, `virtual_time_source.hpp` | monotone Zeit im Aufrufer; native Tests |
| `MockBidirectionalActuatorSink`/`MockBinaryOutputSink` (inkl. `commandJournal()`, `simultaneousActivationObserved()`) | `device_platform_test_support/` | bereits vorhandene Test-Doubles; für Cross-Sink-Reihenfolge zusätzlich mit test-only Decorator-Sinks kombiniert (Abschnitt 12) |

Kein paralleler Sensor-, Prozess-, Persistenz- oder PI-Vertrag wird erfunden.
Insbesondere wird `ControlRequestContext` nicht kopiert, sondern exakt wie in
Issue #22 als flüchtige Identität mitgeführt und geprüft.

Vor der ersten Implementierungsänderung ist die vollständige Repository-Suche
für mindestens diese Symbole zu wiederholen und als Migrationsliste im
Implementierungscommit festzuhalten:

```text
TemperatureControlEvaluationEvidence
previousControlRequestFeedback
evaluateTemperatureControl()
```

Der aktuell geprüfte Stand umfasst mindestens
`temperature_control_orchestrator.hpp/.cpp`,
`temperature_control.hpp/.cpp`,
`test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp`
und `test/test_temperature_control/test_temperature_control.cpp`. Jede
weitere Fundstelle wird entweder auf die neue öffentliche Evidence-Semantik
migriert oder begründet als internes #22-Feedback-Input ausgeschlossen. Kein
Callsite bleibt mit einem caller-supplied Evidence-Feedbackfeld bestehen.

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
    std::uint64_t lastObservedSequenceHighWatermarkBeforeFault{0U};
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
      const ActuatorSafetyGateInput&, std::uint64_t nowMonotonicMillis)
      -> ActuatorPlanTickResult. Sie nimmt keine caller-supplied
      newEvaluation mehr entgegen: Die Application besitzt die intern
      erzeugte `outstandingEvaluation` und übergibt sie genau einmal selbst
      an `planner.tick()`; danach sind beliebig viele Ticks ohne neue
      Evaluation möglich. Leitet currentCanonicalContext und
      temperatureControlledPhase intern über die bereits vorhandenen
      resolveEffectiveControlContext()/isTemperatureControlledProcessState()
      ab (keine Parallel-Ableitung). Interne Aufrufsequenz gemäß Abschnitt
      6.1 (genau ein planner.tick(), genau ein driver.apply()).
    - evaluateTemperatureControl() nimmt das interne, einmalige Handoff und
      injiziert es in den privaten TemperatureControlInput; die öffentliche
      TemperatureControlEvaluationEvidence enthält kein caller-supplied
      previousControlRequestFeedback mehr. Jede erfolgreich erzeugte
      #22-Evaluation wird in `outstandingEvaluation` registriert. Ein zweiter
      #22-Aufruf vor dessen Planner-Beobachtung wird kanonisch verhindert und
      fail-closed beantwortet; er ersetzt die offene Evaluation nicht.
      Nach erfolgreicher Erzeugung schließt die Application vor dem Setzen von
      `outstandingEvaluation` das vorige Planner-Feedbackfenster intern; das
      alte `acceptedCommand` bleibt dadurch ausschließlich physischer Zustand.
    - tickActuatorPlan() setzt beim ersten Tick einer überwachten Episode den
      frischen `watchdogEpisodeStartedAtMonotonicMillis`-Anker, rebased das
      H-Lebenszeichen bei NewActiveRun/Recovery und beendet beide Zeitanker
      beim Verlassen über die kanonische Lifecycle-Grenze. Kein alter Run kann
      so einen neuen Watchdog sofort oder nie auslösen.
    - complete()/needsRuntimeReset() ruft für dieselbe erfolgreiche
      Lifecycle-/Commit-Grenze zusätzlich den Aktorplaner-Stop-Pfad auf
      (Abschnitt 11). Eine unverbrauchte Evaluation wird dabei terminal
      fail-closed aus dem Application-Zustand entfernt; kein Ergebnis darf in
      die nächste Lifecycle-Episode oder als zweite Evaluation gelangen.
    - Alle bestehenden Produktions-/Test-Callsites der öffentlichen Evidence-
      Struktur werden in demselben API-Migrationsschnitt angepasst; die
      direkte #22-PI-Eingabe `TemperatureControlInput` bleibt intern erhalten
      und wird nur noch aus dem Orchestrator-Handoff befüllt.
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

Direkt betroffene #22-API-Konsumenten, die im selben passenden Commit-Schnitt
mitgeführt werden:

```text
lib/fermentation_app/src/temperature_control_orchestrator.hpp / .cpp
lib/fermentation_app/src/temperature_control.hpp / .cpp
test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp
test/test_temperature_control/test_temperature_control.cpp
```

Die Testdateien werden nicht pauschal neu geschrieben: Fixtures, die den
privaten `TemperatureControlInput`-Vertrag des reinen #22-Kerns prüfen, behalten
ihre direkte Feedback-Prüfung. Fixtures, die `TemperatureControlEvaluationEvidence`
oder `evaluateTemperatureControl()` als Application-API prüfen, werden auf den
Orchestrator-Handoff und die neue Evidence-Struktur umgestellt. Vor dem
Commit wird die in Abschnitt 3 geforderte vollständige Symbolsuche erneut
ausgeführt und mit dem tatsächlichen Diff abgeglichen.

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
    // Immutable ownership of this natural window. It is the only sequence
    // that may receive RequestFullyExecuted for this window.
    std::uint64_t sourceRequestSequence{0U};
    std::uint64_t scheduledOnMillis{0U};
    // Exactly one first-start attempt is allowed for this natural window.
    // A late tick that cannot guarantee minimumOnMillis sets this marker and
    // discards the entire pulse without retry or carry into the next window.
    bool pulseStartAttempted{false};
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

enum class ActuatorFeedbackEpisodeAtStop : std::uint8_t {
    ExistingEpisodeOpen,
    ClosedByOutstandingEvaluation,
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

    // Replay high-watermark: every new structurally valid sequence advances
    // this field before stale-on-arrival admission is decided.
    std::optional<std::uint64_t> lastObservedSequenceHighWatermark;
    // H heartbeat, valid only while a watched temperature-control episode is
    // active. It is rebased at episode entry and cleared on episode exit.
    std::optional<std::uint64_t> lastNewRequestAcceptedAtMonotonicMillis;
    std::optional<std::uint64_t> watchdogEpisodeStartedAtMonotonicMillis;
    std::optional<ActuatorWatchdogFaultEvidence> latchedWatchdogFault;

    // Last actually observed disposition for the currently tracked feedback
    // episode. A new active sequence starts with no disposition yet; Phase B
    // records the first real outcome. Once recorded, severity only increases
    // until a new sequence or an explicit OFF/no-request closes the window.
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
    [[nodiscard]] ActuatorPlanTickResult forceStop(
        std::uint64_t nowMonotonicMillis,
        ActuatorFeedbackEpisodeAtStop feedbackEpisodeAtStop);
    // Application-only hook: closes the consumed episode without creating a
    // new feedback update or accepting a caller-supplied sequence.
    void closeFeedbackEpisodeForOutstandingEvaluation();
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
| `acceptedCommand` | Phase B, sobald ein frisch validierter Heating/Cooling-Kandidat tatsächlich als aktuelle Planungsrequest übernommen wird | jeder unmittelbare Fail-closed-Verwurf, tatsächlicher normaler Teardown und `forceStop()` | Nein |
| `activeWindow` einschließlich `sourceRequestSequence` | Fensterstart bzw. bestätigte neue B-Planung (8.1/8.5); `pulseStartAttempted` wird beim ersten Aktivierungsversuch des natürlichen Fensters gesetzt | unmittelbarer Fail-closed-Verwurf, tatsächlicher normaler Teardown, unbestätigte Gegenrichtung am alten Fensterende, `forceStop()` | Nein |
| `accumulator` | Fensterstart-Ereignis (8.1) | unmittelbarer Fail-closed-Verwurf, tatsächlicher Teardown, `forceStop()`; keine Gutschrift für einen am Arming-Gate verpassten Puls | Nein |
| `lastAppliedDirection` | jede physische Ausgangsentscheidung | nächste physische Ausgangsentscheidung | Ja als RAM-Zustand; bei `forceStop()` auf `Idle` gesetzt |
| `currentOnPhaseStartedAtMonotonicMillis` | Tick, an dem `lastAppliedDirection` Idle->Heating/Cooling wechselt | Tick, an dem der physische Ausgang Idle wird | Nein |
| `lastPhysicalDeactivationDirection` / `lastPhysicalDeactivationAtMonotonicMillis` | jeder tatsächliche Active -> Idle-Übergang, einschließlich normalem Window-Off, Fail-closed und `forceStop()` | nie gelöscht, nur beim nächsten tatsächlichen Übergang überschrieben | Ja |
| `counterDirectionCandidate` / `...ObservedSinceMonotonicMillis` / `counterDirectionConfirmed` | 8.5 bei gültiger, aktueller Gegenrequest | Unterbrechung, erfolgreiche B-Übernahme, Fail-closed und `forceStop()` | Nein |
| `lastObservedSequenceHighWatermark` | Phase A, jede neue strukturell valide ControlRequest-Identität vor Stale-on-arrival-/Safety-/Parameterentscheidung | nur durch eine höhere neue strukturell valide Sequence ersetzt; bleibt über `forceStop()` und Lifecycle-RAM-Stop als Replay-Schutz erhalten | Ja |
| `lastNewRequestAcceptedAtMonotonicMillis` | **Zentrale H-Regel:** Phase A setzt dieses Lebenszeichen genau bei `NoValidRequest` sowie bei jeder neuen, strukturell gültigen, tatsächlich angenommenen aktiven #22-Evaluation, sofern sie weder Replay/Duplicate noch eine malformed #22-Evaluation noch stale-on-arrival Watchdog/Context ist; Safety-/Parameter-/Allowed-Sperren verhindern dieses Lebenszeichen nicht | beim Eintritt in eine neue überwachte Episode auf `nullopt` rebased; beim Verlassen gelöscht; innerhalb der Episode nur durch H ersetzt | Nein, wenn der Stop die Episode beendet |
| `watchdogEpisodeStartedAtMonotonicMillis` | Eintritt in eine überwachte Temperaturregel-Episode, spätestens im ersten Planner-Tick mit `temperatureControlledPhase == true` | beim Verlassen der überwachten Episode gelöscht; bei NewActiveRun/Recovery und erneutem Eintritt frisch gesetzt; nie durch Fault-Reset gelöscht, weil es kein Fault-Latch ist | Nein, wenn der Stop die Episode beendet |
| `latchedWatchdogFault` | Watchdog-Trip (Klasse I-5) | ausschließlich `applyExternalWatchdogFaultReset()` | Ja |
| `pendingFeedback` | erste tatsächlich bestimmte Disposition der aktuellen Feedback-Episode sowie spätere Verschärfung gemäß Abschnitt 9; planmäßiger Window-Off setzt keine Verschärfung | eine neue `outstandingEvaluation`-Episode schließt das alte Subjekt; neue aktive Sequence eröffnet ein frisches Subjekt; OFF/NoValidRequest schließen ohne neues Fenster; `forceStop()`/I-Ereignis verschärfen nur ein noch offenes Subjekt, sonst `nullopt` | Nein |
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

`tickActuatorPlan()` folgt bei jedem Aufruf exakt dieser Reihenfolge. Die
öffentliche Methode besitzt **keinen** caller-supplied
`std::optional<TemperatureControlResult>`-Parameter; dadurch kann ein Caller
weder eine andere Evaluation unterschieben noch dieselbe Evaluation als
Replay erneut einspeisen:

```text
tickActuatorPlan(...)
  -> currentCanonicalContext/temperatureControlledPhase ableiten
     (bestehende resolveEffectiveControlContext()/
     isTemperatureControlledProcessState()-Kette, keine Parallel-Ableitung)
  -> evaluation = move(outstandingEvaluation) oder std::nullopt
     [eine gespeicherte Evaluation genau einmal; Slot vor planner.tick()
      leeren]
  -> result = planner.tick(input{newEvaluation = evaluation}) [genau einmal]
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
internen Slot für dieses Handoff. Vor jedem neuen #22-Aufruf prüft
`evaluateTemperatureControl()` zuerst die `outstandingEvaluation`-Invariante.
Nur wenn dieser Slot leer ist, nimmt die Methode den Feedback-Slot vor dem
Aufruf des #22-Kerns atomar aus dem Slot (bei leerem Slot `nullopt`), injiziert
diesen lokalen Wert einmalig in den internen `TemperatureControlInput` und
löscht den Slot bereits vor dem eigentlichen #22-Aufruf. Die öffentliche
`TemperatureControlEvaluationEvidence::previousControlRequestFeedback`-
Callerpflicht entfällt; Sensor-/Zeit-Evidence bleibt caller-supplied. Mehrere
Planner-Ticks zwischen zwei #22-Evaluationen sind damit zulässig und speichern
nur den zuletzt fachlich gültigen Update-Stand. Die Application führt
zusätzlich genau einen internen `outstandingEvaluation`-Slot:

Die öffentliche Evidence-Struktur ist nach der Migration exakt auf Zeit- und
Sensorsignale begrenzt:

```cpp
struct TemperatureControlEvaluationEvidence {
    std::uint64_t sampleTimestampMonotonicMillis{0U};
    device_platform::SensorQualitySnapshot air;
    device_platform::SensorQualitySnapshot product;
};
```

Der private Application-Zustand enthält mindestens:

```cpp
std::optional<TemperatureControlResult> outstandingEvaluation_;
std::optional<PreviousControlRequestFeedback>
    pendingControlRequestFeedback_;
```

Beide Slots gehören ausschließlich dem
`TemperatureControlApplicationOrchestrator`; sie werden nicht in
`TemperatureControlEvaluationEvidence`, `TemperatureControlResult` oder
Persistenz kopiert.

- Nach jedem tatsächlich ausgeführten #22-Aufruf wird dessen vollständiges
  `TemperatureControlResult` genau einmal in diesen Slot gestellt. Das gilt
  auch für `NoValidRequest`/`Unavailable`/`InvalidInput`, weil auch diese
  Ergebnisse vom Planner beobachtet werden müssen.
- Auch ein von der Application selbst erzeugtes fail-closed Ergebnis wegen
  ungültigem effektivem Kontext wird als neue `outstandingEvaluation`
  registriert und schließt die vorige Episode; es darf nicht dazu führen,
  dass der alte `acceptedCommand` später noch Feedbacksubjekt wird.
- In dem Moment, in dem dieser Slot belegt wird, schließt der Orchestrator die
  bisherige Feedback-Episode intern. Die einzige dafür zulässige
  Application-Operation `closeFeedbackEpisodeForOutstandingEvaluation()` ist
  keine öffentliche Caller-Pflicht und erzeugt kein altes Sequence-Feedback.
  Sie verhindert insbesondere, dass ein späterer `forceStop()` das alte
  `acceptedCommand` als Subjekt der neuen Episode resurrecten kann.
- Der nächste `tickActuatorPlan()` nimmt diesen Slot atomar und verwendet ihn
  als `ActuatorPlanTickInput::newEvaluation`; danach ist der Slot leer. Weitere
  Planner-Ticks liefern `std::nullopt`, bis eine neue #22-Evaluation entsteht.
- Ruft ein Caller `evaluateTemperatureControl()` erneut auf, solange der Slot
  noch belegt ist, führt die Application den #22-Kern nicht erneut aus. Sie
  liefert ein deterministisches fail-closed
  `InvalidInput/InvalidConfiguration`-Ergebnis mit `Idle` zurück, lässt den
  bestehenden Slot unverändert und markiert dies nicht als neue Evaluation.
  Damit ist `evaluate n -> Planner-Consume(n) -> evaluate n+1` eine intern
  geschützte Invariante und keine ungesicherte Caller-Konvention.
- Eine erfolgreiche Lifecycle-Grenze leert einen unverbrauchten
  `outstandingEvaluation`-Slot terminal fail-closed zusammen mit dem Lauf-
  Reset; er darf nicht in die nächste Lifecycle-Episode gelangen. Der
  Orchestrator übergibt `forceStop()` den internen Wert
  `ActuatorFeedbackEpisodeAtStop::ClosedByOutstandingEvaluation`, wenn dieser
  Slot bereits belegt war; andernfalls
  `ActuatorFeedbackEpisodeAtStop::ExistingEpisodeOpen`. Nur der zweite Fall
  darf ein noch offenes aktives Feedbacksubjekt als `{S, Rejected}`
  verschärfen. Der erste Fall verwirft `acceptedCommand` und alle
  Planungsdaten ohne altes Sequence-Feedback. Ein fehlgeschlagener
  Persistence-Commit konsumiert oder löscht weder Evaluation noch
  Feedback-Handoff.

Der Orchestrator kopiert kein Feedback zwischen Callern; er besitzt,
aktualisiert und konsumiert Handoff und Evaluation selbst. Die neue
#22-Evaluation wird intern genau einmal als `newEvaluation` an den Planner
übergeben. Dieses interne Weiterreichen ist die geschützte Bedeutung der
Reihenfolge `evaluate n -> tickActuatorPlan(newEvaluation=n)`.

Intern läuft `tick()` in **zwei Phasen**, die absichtlich unabhängig
voneinander sind:

### 6.2 Phase A – Annahme (Admission), immer zuerst, unabhängig von Parametern/Safety/Fault

Diese Phase aktualisiert ausschließlich Buchführung (Replay-Schutz,
Watchdog-Lebenszeichen und die vertrauenswürdige Request-Identität) und
liefert `admissionOutcome` plus – bei Erfolg – einen vorgemerkten Kandidaten
für Phase B. Sie läuft **immer**, auch wenn Parameter ungültig sind, Safety
nicht `Allowed` ist oder ein Watchdog-Fault latched ist. Ein gültiges neues
#22-Ergebnis ist zunächst eine #22-Auswertung; ob es physisch ausgeführt wird,
entscheidet erst Phase B.

**Neue-Evaluation-Episodengrenze:** Sobald
`input.newEvaluation.has_value()` gilt, wird das bisherige Planner-
`pendingFeedback` vor jeder weiteren Klassifikation geschlossen. Die
vorherige #22-Evaluation hat dieses Feedbackfenster bereits konsumiert;
`acceptedCommand` und ein noch laufendes physisches Fenster bleiben davon als
Planungs-/Mindestzeitdaten unberührt, dürfen aber nicht mehr als Feedback-
Subjekt verwendet werden. Phase A darf in diesem Tick ausschließlich ein
Subjekt für die neue Evaluation eröffnen: eine neue vertrauenswürdige aktive
Sequence, die neue stale-on-arrival Sequence oder kein Subjekt bei OFF,
`NoValidRequest` oder malformed Identität. Ein `std::nullopt`-Tick schließt
dagegen keine Episode und führt die bisherige Governance fort.

Die folgende H-Regel ist die einzige Stelle, an der das Watchdog-Lebenszeichen
gesetzt werden darf:

> **H:** `lastNewRequestAcceptedAtMonotonicMillis = now` genau bei
> `NoValidRequest` sowie bei jeder neuen, strukturell gültigen und tatsächlich
> angenommenen aktiven #22-Evaluation, sofern sie weder Replay/Duplicate noch
> eine malformed #22-Evaluation noch stale-on-arrival Watchdog/Context ist.
> Safety-, Parameter- und `Allowed`-Sperren verhindern H nicht. `std::nullopt`,
> Replay/Duplicate, eine malformed #22-Evaluation und stale-on-arrival
> Watchdog/Context setzen H nicht.

1. `input.newEvaluation == std::nullopt` -> `admissionOutcome = NoCandidate`,
   kein vorgemerkter Kandidat, keine `pendingFeedback`-Änderung und kein H.
   **Phase A endet hier.**
2. Zuerst wird ausschließlich die #22-Evaluation und der aktuelle kanonische
   Kontext strukturell geprüft: unbekannter Status-/Richtungswert,
   nicht-finite oder außerhalb `[0,1]` liegende Quote, Status-/Request-
   Widerspruch, `sequence == 0` bei vorhandener Request oder ein strukturell
   ungültiger `currentCanonicalContext` -> `admissionOutcome =
   MalformedCandidate`, kein H, kein vorgemerkter Kandidat und
   `pendingFeedback = std::nullopt` als geändertes Update. Die Identität ist
   nicht vertrauenswürdig; es wird kein Sequence-Feedback erfunden.
3. Ein ungültiger externer `input.safetyGate.status` wird **separat** als
   `MalformedSafetyGate` markiert. Ist die #22-Evaluation selbst strukturell
   gültig, bleibt ihre Request-Identität vertrauenswürdig: H wird nach den
   folgenden Regeln gesetzt, `lastObservedSequenceHighWatermark` wird normal geführt und
   Phase B verwirft den Tick als Klasse I-1. Safety-Enum-Korruption macht
   daher eine valide ControlRequest-Sequence nicht automatisch unsicher.
4. `classifyActuatorDemand(newEvaluation.value())` (Abschnitt 7) ==
   `NoValidRequest` (`Unavailable`/`InvalidInput` ohne ControlRequest):
   `admissionOutcome = Accepted` beziehungsweise `MalformedSafetyGate`, falls
   nur das externe Gate malformed ist; H wird gesetzt. Der Kandidat trägt
   `NoValidRequest` ohne Sequence. Falls bereits ein vertrauenswürdig aktives
   `acceptedCommand` besteht, wird dessen Sequence **nicht** als Feedback
   verwendet: Die neue Evaluation hat das alte Fenster bereits geschlossen.
   `pendingFeedback = std::nullopt`. Diese Evaluation zählt als lebende
   #22-Auswertung, eröffnet aber selbst kein Feedbackfenster und schaltet in
   Phase B physisch fail-closed ab.
5. Andernfalls trägt die Evaluation eine ControlRequest. Falls
   `identity.sequence <= lastObservedSequenceHighWatermark` gilt, ist sie
   `DuplicateOrOldSequence`: kein H, kein Kandidat, keine Bookkeeping- oder
   Feedbackänderung. Ein Replay darf weder eine laufende Zeitbasis noch ein
   bestehendes Handoff zurückdrehen.
6. Für eine neue, strukturell valide Sequence wird zunächst
   `lastObservedSequenceHighWatermark = identity.sequence` gesetzt. Danach gilt:
   - `deadlineReached(now, identity.createdAtMonotonicMillis,
     requestWatchdogMillis)` -> `StaleOnArrivalWatchdog`, kein H, kein
     Kandidat; die Sequence ist vertrauenswürdig bekannt und erhält
     `pendingFeedback = {sequence, Rejected}`. `acceptedCommand` bleibt
     unverändert.
   - `context != currentCanonicalContext` -> `StaleOnArrivalContext`, kein H,
     kein Kandidat und ebenfalls `{sequence, Rejected}`; `acceptedCommand`
     bleibt unverändert.
   - Gültiges `NeutralOff`/`AirLimitBlockedOff` -> H; `admissionOutcome`
     `Accepted` beziehungsweise `MalformedSafetyGate`; kein
     Feedbackfenster, Kandidat mit Richtung `Idle`.
   - Gültiges Heating/Cooling (`NormalDemand` oder `AirLimitReducedDemand`)
   -> H; `admissionOutcome` `Accepted` beziehungsweise
   `MalformedSafetyGate`; Kandidat mit Richtung, Quote, Sequence und
   Kontext. Es wird **kein vorläufiges `Rejected`** gesetzt: Phase B trägt
   die erste tatsächliche Disposition ein, damit die Monotonie-Regel nicht
   durch einen nicht beobachteten Platzhalter verletzt wird.

Bei jedem `newEvaluation` bleibt ein altes `acceptedCommand` ausschließlich
für die physische Mindest-On-/Stop-Logik relevant. Es darf weder eine alte
Sequence als `pendingFeedback` wieder einsetzen noch die neue Sequence als
bereits ausgeführt markieren. Eine neue aktive Sequence erhält ihre erste
Disposition nur aus ihrem eigenen Candidate-/Window-Pfad; bei einer noch
nicht möglichen B-Fensteranlage ist das `DeferredOrLimited` mit einem echten
`ActuatorPlanReason` nach Abschnitt 8.2. Eine neue `OFF`- oder
`NoValidRequest`-Evaluation beendet dagegen ohne Feedbacksubjekt.

Eine bei Admission abgelehnte neue Request (`DuplicateOrOldSequence`,
`StaleOnArrivalWatchdog`, `StaleOnArrivalContext`, `MalformedCandidate` oder
`MalformedSafetyGate`) berührt einen bereits gehaltenen `acceptedCommand`
nicht, soweit Phase B nicht selbst die unmittelbare Fail-closed-Klasse
ausführt. Replay und stale-on-arrival Requests lassen die bestehende
Zeitbasis weiterlaufen; eine malformed Request-Identität schließt das
Handoff und führt I-1 aus. Eine malformed Safety-Evidence führt ebenfalls I-1
aus, darf aber bei valider Request-Identität das `{sequence, Rejected}`-Feedback
verwenden. Die vollständige Trusted-Sequence-Regel steht in Abschnitt 9 und
gilt identisch für I-Ereignisse und `forceStop()`.

### 6.3 Phase B – Physischer Ausgang (Prioritätsleiter, Abschnitt 8.2)

Phase B verwendet den in Phase A ermittelten vorgemerkten Kandidaten (falls
vorhanden) zusammen mit dem laufenden `ActuatorPlannerRuntimeState`, um genau
eine physische Ausgangsentscheidung für diesen Tick zu treffen. Sie ist in
Abschnitt 8.2 vollständig definiert und aktualisiert am Ende jedes Ticks
`pendingFeedback` sowie dessen einmaliges Update-Signal gemäß Abschnitt 9.

### 6.4 Laufender Watchdog

Der Watchdog wird nur überwacht, wenn
`input.temperatureControlledPhase == true` ist. Diese Information wird vom
Orchestrator ausschließlich über `isTemperatureControlledProcessState()`
abgeleitet. Eine überwachte Episode beginnt beim Eintritt aus einem nicht
temperaturgeregelten Zustand; falls der erste Planner-Tick erst danach
eintrifft, setzt genau dieser erste Tick
`watchdogEpisodeStartedAtMonotonicMillis = now` und prüft die Frist noch
nicht gegen einen alten Run. Beim Eintritt nach `NewActiveRun` oder
`Recovery` werden der Episodenanker frisch gesetzt und das H-Lebenszeichen
auf `std::nullopt` rebased. Ein Übergang zwischen zwei weiterhin
temperaturgeregelten Zuständen derselben Episode rebased nichts.

Beim Verlassen der überwachten Episode beendet der Orchestrator am selben
erfolgreichen Lifecycle-Reset, an dem `resetTemperatureControlAtBoundary()`
läuft, den Heartbeat-/Episodenanker sauber: beide Zeitanker werden gelöscht.
Das gilt für `LeaveTemperatureControl`, `Fault`, `SafeBoot`, `Service` und
`Standby`; `NewActiveRun`/`Recovery` beginnen danach bei einem späteren
temperaturgeregelten Eintritt eine frische Episode. Diese Rebasings löschen
`latchedWatchdogFault` niemals und sind kein Fehlerreset.

Bei **jedem** Tick innerhalb einer überwachten Episode, unabhängig davon, ob
Phase A einen Kandidaten verarbeitet hat, prüft Phase B:

```text
watchdogReferenceAt = lastNewRequestAcceptedAtMonotonicMillis.value_or(
    watchdogEpisodeStartedAtMonotonicMillis.value())
deadlineReached(nowMonotonicMillis, watchdogReferenceAt,
                requestWatchdogMillis)
```

`watchdogEpisodeStartedAtMonotonicMillis` ist an dieser Stelle garantiert
vorhanden. `std::nullopt` bei
`lastNewRequestAcceptedAtMonotonicMillis` bedeutet daher nicht „Watchdog nie
prüfen“, sondern „seit Episodenbeginn wurde noch kein H empfangen“. Vor dem
ersten H gilt die exakte Frist ab dem frischen Episodenanker; nach H gilt die
Frist ab dem letzten H.

Die H-Regel aus Phase A ist hier **wortgleich** maßgeblich:

> `lastNewRequestAcceptedAtMonotonicMillis = now` genau bei
> `NoValidRequest` sowie bei jeder neuen, strukturell gültigen und tatsächlich
> angenommenen aktiven #22-Evaluation, sofern sie weder Replay/Duplicate noch
> eine malformed #22-Evaluation noch stale-on-arrival Watchdog/Context ist.
> Safety-, Parameter- und `Allowed`-Sperren verhindern H nicht. `std::nullopt`,
> Replay/Duplicate, eine malformed #22-Evaluation und stale-on-arrival
> Watchdog/Context setzen H nicht.

Damit ist der Watchdog unabhängig von der #22-eigenen Kadenz und von der
Planner-Tick-Kadenz: Neue HEAT-, COOL-, OFF- und `NoValidRequest`-Evaluationen
halten ihn jeweils am Leben, sofern H gilt; Replay, malformed und
stale-on-arrival verlängern die Frist nicht. Eine `NoValidRequest`-Evaluation
zählt als lebende #22-Auswertung, schaltet aber in Phase B physisch sofort
fail-closed ab. Ein Tick außerhalb einer überwachten Episode startet keinen
Watchdog und kann keinen alten Zeitanker verwenden.

### 6.5 Welche Quote ein Fenster bestimmt / neue Quote mitten im Fenster

Siehe Abschnitt 8.1. Kurzfassung: `ActiveSwitchingWindow` wird bei jedem
zulässigen Fensterstart-Ereignis genau einmal aus der dann aktuellen Request
erzeugt, übernimmt deren Sequence als unveränderliche
`sourceRequestSequence` und bleibt für die Dauer dieses Fensters
unveränderlich. Eine neue Request mit unveränderter Richtung wirkt erst im
nächsten Fenster. Eine
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
ein unveränderlicher Planungssnapshot. Er trägt zusätzlich mit
`sourceRequestSequence` die vertrauenswürdige Sequence, aus der dieses
Fenster erzeugt wurde. `lastAppliedDirection` ist der
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
`acceptedCommand` gelesen; dabei wird
`activeWindow.sourceRequestSequence = acceptedCommand.sequence` atomar mit
dem Snapshot gesetzt. Die Quelle ändert sich niemals mitten im Fenster.

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
   natürlichen On-Intervalls den physischen Puls nur starten, wenn vor der
   Aktivierung gilt:

   ```text
   elapsedInWindow = now - window.startMonotonicMillis
   remainingNaturalOnMillis =
       window.scheduledOnMillis - elapsedInWindow
   remainingNaturalOnMillis >= minimumOnMillis
   ```

   Die Subtraktion wird nur nach `elapsedInWindow < scheduledOnMillis`
   ausgeführt. Der erste Aktivierungsversuch setzt
   `activeWindow.pulseStartAttempted = true`. Ist
   `remainingNaturalOnMillis < minimumOnMillis`, wird der gesamte
   Fensterpuls als `DeferredOrLimited` verworfen; das Fenster erhält keinen
   späteren Retry, keine Verlängerung über sein natürliches Ende und keine
   Nachholung im Folgefenster. Ein verspäteter Tick darf daher niemals einen
   realen normalen Puls unter `minimumOnMillis` starten. Ein zugelassener
   physischer Puls endet am natürlichen Intervallende, sofern keine
   unmittelbare Fail-closed-Abschaltung ihn früher beendet; ein normaler
   Teardown darf ihn nur bis zum Ende der konkreten Mindest-Einschaltphase
   halten.
4. Ist das Arming-Gate exakt am Fensterstart erfüllt, ist der Puls zulässig
   (Gleichheitsgrenze); ist es erst danach erfüllt, bleibt dieses Fenster
   verworfen. Damit sind vor, auf und nach dem Minimum-Off-Ende sowie kurze und
   lange Off-Anteile ohne Nachholung unterscheidbar testbar.

Diese feste Lage ist die gewählte Revision-7-Regel. Sie ist konservativer als
ein verschobener Puls, verhindert aber jede stille Energieerzeugung aus einer
Sperrphase und ist O(1) sowie nativ deterministisch.

**Variante A – Window-Ownership ist feedbackrelevant.** Diese Revision
entscheidet ausdrücklich gegen die alternative Variante B. Die natürliche
Window-Synchronisierung ist für eine neue Sequence dann eine downstream
Begrenzung, wenn deren eigenes Fenster noch nicht begonnen hat. Ein laufendes
Fenster A darf niemals als Ausführung von B bewertet werden, auch wenn
`acceptedCommand` zwischenzeitlich auf eine gleichgerichtete B-Request zeigt.
Am nächsten zulässigen Fensterstart wird aus B ein neues Snapshot erzeugt und
dieses trägt `sourceRequestSequence == B.sequence`. Wird ein A-Fenster
verworfen, gelöscht oder durch OFF/Fail-closed beendet, wird seine Ownership
ebenfalls gelöscht. Die Entscheidung `RequestFullyExecuted` ist damit aus dem
Runtime-Datenmodell beweisbar und nicht aus physischer Richtung allein
hergeleitet. Die Wahl folgt `ACTUATOR_TIMING.md`: Das gemeinsame
zeitproportionale Fenster friert die Quote je Fenster ein; eine neue Quote
wirkt erst am Folgefenster. Die nicht ausgeführte B-Quote ist daher bis zu
ihrem eigenen Fenster tatsächlich begrenzt und darf #22 nicht als
ungehindert ausgeführt gemeldet werden.

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

`desiredActive` ist nur der natürliche Zeitwunsch. Der zentrale
Übergangs-/Prioritätspfad aus Abschnitt 8.2 entscheidet zusätzlich über
Mindest-On, physische Mindest-Off-/Totzeit, Fail-closed und den Außenlüfter.
Ist der physische Ausgang in diesem Fenster noch `Idle` und
`pulseStartAttempted == true`, gilt der natürliche Start als bereits
verworfen und es gibt keinen zweiten Startversuch; ist der Ausgang bereits
aktiv, darf der begonnene Puls bis zum natürlichen Ende weiterlaufen.
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
    CounterDirectionConfirming,
    WindowPulseDeferred,
    WindowPulseMissed,
    ScheduledWithinWindow,
};
```

Die folgende Tabelle ist bewusst in zwei Klassen getrennt. Die unmittelbare
Fail-closed-Klasse ist nicht durch die normale Mindest-Einschaltzeit
verzögerbar. Die normale Klasse betrifft ausschließlich eine gültige,
nicht-faultige Teardown- oder Freigabeentscheidung und darf nur dort
`MinimumOnTimeHeld` liefern, wo der kanonische Vertrag das ausdrücklich
zulässt.

**Exhaustive Trusted-Sequence-Regel für jede vollständige Nichtannahme:**

- Eine Sequence ist vertrauenswürdig bekannt, wenn sie aus einer strukturell
  validierten aktuellen #22-ControlRequest stammt (auch wenn sie wegen
  Stale-on-arrival oder eines korrupten externen Safety-Gates nicht
  ausgeführt werden darf) oder wenn sie bereits als `acceptedCommand`
  gehalten wird.
- Führt ein unmittelbares I-Ereignis oder `forceStop()` zur vollständigen
  Nichtannahme und ist genau ein **noch offenes** Feedbacksubjekt `S` bekannt,
  wird ausschließlich `{S, Rejected}` als `pendingFeedback` gesetzt
  beziehungsweise beibehalten und bis zum einmaligen #22-Konsum
  weitergereicht. Ein bloß gehaltenes `acceptedCommand` genügt dafür nicht,
  wenn eine neue `outstandingEvaluation`-Episode das vorige Fenster bereits
  geschlossen hat.
- Ist die aktuelle Request-Identität nicht vertrauenswürdig, wird kein
  Sequence-Feedback erfunden und der Handoff auf `std::nullopt` geschlossen.
  Die Korruption des externen Safety-Gate-Werts ändert die Vertrauenswürdigkeit
  einer ansonsten validen ControlRequest-Identität nicht.
- Wenn `newEvaluation` vorhanden ist, schließt sie das alte Feedbackfenster
  vor dieser Regel. `OFF`/`NoValidRequest` eröffnen kein neues Fenster und
  erzeugen insbesondere kein spätes Feedback für das alte
  `acceptedCommand`. Eine neue stale-on-arrival Evaluation erhält dagegen
  ausschließlich ihr eigenes `{sequence, Rejected}`; eine malformed
  Identität erzeugt gar kein Sequence-Feedback.

Diese vier Sätze sind die gemeinsame Bedeutung von Feldtabelle, Phase A,
Prioritätsleiter, Abschnitt 9 und `forceStop()`; „vollständige Bereinigung"
bedeutet daher nicht pauschal „Feedback löschen".

**Klasse I – unmittelbare Fail-closed-Abschaltung:**

| Klasse | Bedingung | Status | Reason | Physisch im selben Tick | Zustandswirkung |
|---|---|---|---|---|---|
| I-1 | Phase A liefert `MalformedCandidate`/`MalformedSafetyGate`, aktueller Kontext oder ein anderer struktureller Tickwert ist ungültig | `InvalidInput` | `MalformedInput` | `Idle`, sofort | Snapshot, Akkumulator, Gegenrichtung und Request schließen; Feedback nach Trusted-Sequence-Regel |
| I-2a | `classifyActuatorPlannerParameters() == Unconfigured` | `Unconfigured` | `NoCommissioning` | `Idle`, sofort | dieselbe vollständige Bereinigung wie I-1; Feedback nach Trusted-Sequence-Regel |
| I-2b | `classifyActuatorPlannerParameters() == Invalid` | `InvalidInput` | `InvalidConfiguration` | `Idle`, sofort | dieselbe vollständige Bereinigung wie I-1; Feedback nach Trusted-Sequence-Regel |
| I-3a | `safetyGate.status == Unresolved` | `Idle` | `SafetyGateUnresolved` | `Idle`, sofort | vollständige Bereinigung; kein Minimum-On-Hold; Feedback nach Trusted-Sequence-Regel |
| I-3b | `safetyGate.status == ImmediateStop` | `Idle` | `ExternalSafetyOverride` | `Idle`, sofort | vollständige Bereinigung; kein Minimum-On-Hold; Feedback nach Trusted-Sequence-Regel |
| I-4 | `latchedWatchdogFault.has_value()` | `Idle` | `RequestWatchdogFaultLatched` | `Idle`, sofort | keine Wiederfreigabe; Latch bleibt bestehen; Feedback nach Trusted-Sequence-Regel |
| I-5 | der laufende Watchdog trippt in diesem Tick | `Idle` | `StaleRequestWatchdog` | `Idle`, sofort | Fault-Evidenz latchen und alle Plan-/Peltierzustände verwerfen; Feedback nach Trusted-Sequence-Regel |
| I-6 | eine **neue, explizite** Phase-A-Evaluation klassifiziert als `NoValidRequest` | `Idle` | `NoValidRequest` | `Idle`, sofort | alte Planungsdaten schließen; wegen der neuen Episode kein altes Sequence-Feedback resurrecten; neue `NoValidRequest` eröffnet kein Feedbackfenster |
| I-7 | der gehaltene Request-Kontext ist stale/inkompatibel zum aktuellen kanonischen Kontext | `Idle` | `StaleRequestContext` | `Idle`, sofort | aktuelle Request, Snapshot und Guthaben verwerfen; Feedback nach Trusted-Sequence-Regel |
| I-8 | eine Referenzzeit ist retrograd oder anderweitig `TimeInvalid` | `InvalidInput` | `TimeInvalid` | `Idle`, sofort | vollständige Bereinigung; keine Frist wird als erfüllt angenommen; Feedback nach Trusted-Sequence-Regel |

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
| N-4 | gültige normale Fensterauswertung ohne downstream Begrenzung | `Active`/`Idle` mit `ScheduledWithinWindow` oder `MinimumPulseTriggered`; der planmäßige Window-Off-Anteil gehört ausdrücklich hierher | gemäß natürlichem Snapshot-Intervall | reguläre Fensterfortschreibung; Feedback `NoIntegratorConstraint` unabhängig von Sample-Phase |
| N-5a | gültige Request, Akkumulation bleibt unter `minimumOnMillis` | `Idle` / `AccumulatingBelowThreshold` | Idle | Guthaben bleibt innerhalb des Caps; Feedback `DeferredOrLimited` |
| N-5b | gültige Request, Fensterpuls am Minimum-Off-Gate gesperrt | `Idle` / `MinimumOffTimeHeld` | Idle | aktueller Puls wird verworfen, nicht verschoben; Feedback `DeferredOrLimited` |
| N-5c | gültige Request, Fensterpuls am Polaritäts-Totzeit-Gate gesperrt | `Idle` / `PolarityDeadTimeHeld` | Idle | aktueller Puls wird verworfen, nicht verschoben; Feedback `DeferredOrLimited` |
| N-5d | gültige Gegenrequest, Bestätigungsdauer noch nicht erfüllt | `Idle` / `CounterDirectionConfirming` | Idle | alte Energie endet höchstens am eingefrorenen Fensterende; Feedback `DeferredOrLimited` |
| N-5e | gültige Request, erster Tick verpasst den natürlichen Mindestpuls | `Idle` / `WindowPulseMissed` | Idle | Puls wird einmalig verworfen, nicht verlängert oder nachgeholt; Feedback `DeferredOrLimited` |
| N-5f | neue gleichgerichtete Request trifft während eines fremden laufenden Fensters ein | `Idle` / `WindowPulseDeferred` | Idle beziehungsweise alter Window-Verlauf ohne B-Ausführung | B erhält bis zum eigenen Folgefenster `DeferredOrLimited`; A wird nicht für B behauptet |

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

`DeferredOrLimited` ist ausschließlich eine
`PreviousControlRequestFeedback::Disposition`. Es ist niemals ein
`ActuatorPlanReason`; jede N-5-Ursache verwendet genau einen der echten
Reason-Werte N-5a bis N-5f.

**Exhaustive Ursache-/Typ-/Ausgangstabelle für die normale Planung:**

| Ursache | `ActuatorPlanStatus` | `ActuatorPlanReason` | Feedback-Disposition | physischer Ausgang |
|---|---|---|---|---|
| planmäßige Quote innerhalb des zulässigen Fensters | `Active` oder `Idle` | `ScheduledWithinWindow` / `MinimumPulseTriggered` | `NoIntegratorConstraint` | natürliche Quote oder planmäßiges Window-Off |
| Quote unter Mindestimpuls, Guthaben noch zu klein | `Idle` | `AccumulatingBelowThreshold` | `DeferredOrLimited` | `Idle` |
| Mindest-Auszeit sperrt den natürlichen Fensterpuls | `Idle` | `MinimumOffTimeHeld` | `DeferredOrLimited` | `Idle` |
| Polaritäts-Totzeit sperrt den natürlichen Fensterpuls | `Idle` | `PolarityDeadTimeHeld` | `DeferredOrLimited` | `Idle` |
| Gegenrichtungsbestätigung läuft noch | `Idle` | `CounterDirectionConfirming` | `DeferredOrLimited` | `Idle` |
| erster Tick verpasst den verbleibenden Mindestpuls | `Idle` | `WindowPulseMissed` | `DeferredOrLimited` | `Idle` |
| neue gleichgerichtete Request wartet auf ihr eigenes Folgefenster | `Idle` | `WindowPulseDeferred` | `DeferredOrLimited` | `Idle` beziehungsweise altes A-Fenster ohne B-Ausführung |
| normaler OFF-/AirLimit-Teardown nach erfüllter Mindest-On-Zeit | `Idle` | `NeutralIdle` / `AirLimitBlocked` | kein neues Feedbackfenster | `Idle` |
| normaler OFF-/AirLimit-Teardown während Mindest-On-Zeit | `Active` | `MinimumOnTimeHeld` | kein neues Feedbackfenster; alte Episode wird nur nach ihrer Regel weitergeführt | alte Richtung bleibt bis Mindest-On-Ende aktiv |

Die I-1 bis I-8-Tabelle bleibt davon getrennt: Ihre physischen Ausgänge sind
immer sofort `Idle`, und ihre Feedback-Disposition folgt ausschließlich der
Trusted-Sequence-Regel. Damit wird kein Status-/Reason-Typ als Ersatz für die
Feedback-Disposition missbraucht.

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
`watchdogEpisodeStartedAtMonotonicMillis`,
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
    .lastObservedSequenceHighWatermarkBeforeFault =
        state_.lastObservedSequenceHighWatermark.value_or(0U),
};
```

gesetzt. Solange `latchedWatchdogFault.has_value()`, bleibt Klasse I-4 in jedem
weiteren Tick maßgeblich (`Idle`, `RequestWatchdogFaultLatched`) –
**unabhängig davon**, was danach in Phase A geschieht: Eine strukturell
einwandfreie, kontextfrische, an sich akzeptable neue `newEvaluation` wird
in Phase A weiterhin für Sequenz-/Watchdog-Bücher berücksichtigt
(`admissionOutcome` kann `Accepted` sein,
`lastObservedSequenceHighWatermark`/
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
Planungsrequest; `pendingFeedback` bestimmt ausschließlich das noch offene
Feedbacksubjekt für #22. Das Planner-Feld ist keine Caller-API. Sobald eine
neue Evaluation als `outstandingEvaluation` registriert wird, ist das alte
Subjekt geschlossen; `acceptedCommand` darf danach nur noch für physische
Mindestzeit-/Stop-Logik verwendet werden. Änderungen an einem neuen Subjekt
werden als `PendingControlRequestFeedbackUpdate` mit einer einmaligen
internen Update-Revision an die Application-Grenze ausgeliefert.

Der Orchestrator besitzt dort
`std::optional<PreviousControlRequestFeedback>
pendingControlRequestFeedback_`. Nach jedem Planner-Tick übernimmt er nur ein
tatsächlich neues Update. Er ersetzt damit keinen älteren Wert durch einen
unveränderten Replay. Ein Update mit `std::nullopt` löscht den
Application-Slot.

Für eine einzelne aktive Request-Sequence gilt die feste Severity-Ordnung:

```text
NoIntegratorConstraint (0)
    < DeferredOrLimited (1)
        < Rejected (2)
```

`mergeDisposition(sequence, candidate)` darf nur für dieselbe Sequence
aufgerufen werden und speichert den jeweils schwereren Wert. Eine neue aktive
Sequence eröffnet einen neuen, zunächst noch nicht ausgegebenen
Dispositionszustand; sie erbt weder `Rejected` noch `DeferredOrLimited` der
alten Sequence. `OFF`/`NoValidRequest` schließen das alte Fenster nach der
Trusted-Sequence-Regel und eröffnen kein neues.

### 9.2 Ereignisgetriebene Update-Regel

Bei jeder Phase-A-Verarbeitung einer neu beobachteten Evaluation gilt:

| Beobachtetes Ereignis | Wirkung auf Planner-`pendingFeedback` | Handoff-Update |
|---|---|---|
| `newEvaluation == std::nullopt` | unverändert | kein Update |
| `newEvaluation` vorhanden | bisheriges Subjekt schließen; kein altes Sequence-Feedback resurrecten | kein altes Feedback; die folgenden Zeilen gelten nur für die neue Episode |
| `DuplicateOrOldSequence` | unverändert | kein Update |
| `MalformedCandidate` | `std::nullopt`, weil die Identität unsicher ist | geändert auf `nullopt` |
| `MalformedSafetyGate` bei ansonsten valider Request-Identität | Klasse I-1; vertrauenswürdige Sequence erhält `{sequence, Rejected}` | geändert auf genau diese Sequence |
| `NoValidRequest` ohne aktive vertrauenswürdige Sequence | `std::nullopt` | geändert auf `nullopt` |
| `NoValidRequest` nach bereits geschlossener alter Episode | `std::nullopt`; die neue NoValidRequest eröffnet kein Fenster | geändert auf `nullopt` |
| gültiges `NeutralOff` oder `AirLimitBlockedOff` | `std::nullopt` | geändert auf `nullopt` |
| gültige aktive Request, aber `StaleOnArrivalWatchdog` oder `StaleOnArrivalContext` | `{sequence, Rejected}` | geändert auf genau diese vertrauenswürdige Sequence |
| gültige aktive Request, Admission bestanden | erste tatsächliche Phase-B-Disposition; kein vorläufiges `Rejected` | geändert auf genau diese Sequence nach Beobachtung |

Bei jeder Änderung der Disposition wird ein Update markiert, auch wenn die
neue Disposition wieder `Rejected` lautet. Ein identischer Wert wird
nicht erneut ausgeliefert.

Wird ein noch offenes, vertrauenswürdig bekanntes Feedbacksubjekt `S` durch
ein unbedingtes I-Ereignis, `forceStop()` oder eine explizite vollständige
Nichtannahme außerhalb des planmäßigen Window-Off verworfen, wird dessen
Disposition mit `Rejected` gemerged und als Update markiert. Ein bloß
gehaltenes `acceptedCommand` ist nach der Registrierung einer neuen
`outstandingEvaluation` kein solches Subjekt. Eine `OFF`-/`NoValidRequest`-
Evaluation schließt ohne neues Feedbackfenster. Ein `MalformedCandidate` hat
keine vertrauenswürdige Sequence und löscht deshalb das gesamte Handoff; ein
älteres Subjekt wird nie als vermeintliches Feedback für diese neue Evaluation
weitergereicht. Ein Replay darf kein bestehendes Handoff zurückdrehen.

### 9.3 Live-Nachführung während laufender Governance

Feedback wird nicht aus `physicalDirection() == requestedDirection` allein
abgeleitet. Phase B führt intern je Fenster eine Ausführungsfeststellung:

```text
RequestFullyExecuted:
  aktuelles Fenster trägt sourceRequestSequence == der vertrauenswürdigen
  aktiven Feedback-Sequence,
  die natürliche Zeitquote wurde ohne #23-seitige Reduktion geplant,
  der Puls durfte am Arming-Gate starten und wurde nicht wegen
  Minimum-Off, Polaritätstotzeit, Gegenrichtungsbestätigung, Counterdirection,
  verspätetem Start oder sonstigem #23-Gate verzögert/gekürzt;
  sowohl der laufende Duty-Puls als auch der planmäßige Window-Off-Anteil
  derselben korrekt ausgeführten Quote gehören hierher.

DownstreamLimited:
  mindestens eine #23-seitige Sperre, Verzögerung, Reduktion oder ein
  verpasster/verworfener Fensterpuls wirkte, insbesondere Minimum-Off,
  Polaritätstotzeit, unbestätigte Gegenrichtung, Akkumulation unter
  minimumOnMillis, Counterdirection-Gate, fehlende Window-Ownership oder
  verspäteter erster Tick.
```

Die Window-Ownership-Prüfung ist ein echter Datenvergleich:
`activeWindow.sourceRequestSequence` muss exakt dem Feedbacksubjekt
entsprechen. Ein gleichgerichtetes B mitten in einem noch laufenden A-Fenster
erhält deshalb niemals `RequestFullyExecuted`; bis zum eigenen B-Folgefenster
ist es `DownstreamLimited` mit `WindowPulseDeferred` beziehungsweise dem
jeweils tatsächlich eingetretenen Gate. Ein A-Fenster darf nach Schließung der
A-Episode physisch noch auslaufen, erzeugt aber kein A-Feedback mehr.

Für ein aktives Heating/Cooling-Handoff gilt danach:

- `RequestFullyExecuted` -> Kandidat `NoIntegratorConstraint`. Das gilt
  ausdrücklich unabhängig davon, ob der #22-Folgesample während des laufenden
  Peltier-EIN oder während des normalen, planmäßigen AUS-Anteils derselben
  Zeitquote eintrifft.
- `DownstreamLimited` -> Kandidat `DeferredOrLimited`.
- Vollständige Nichtannahme durch Klasse I oder `forceStop()` -> Kandidat
  `Rejected` nach der Trusted-Sequence-Regel.

Der Kandidat wird anschließend mit `mergeDisposition()` gegen die bisherige
Disposition derselben Sequence gemerged. Ein einmaliges `DeferredOrLimited`
bleibt daher auch nach späterer physischer Freigabe mindestens deferred; ein
`Rejected` bleibt rejected. Der planmäßige Window-Off-Anteil ist keine
Verschärfung und darf nicht allein wegen seines zufälligen Sample-Zeitpunkts
`DeferredOrLimited` erzeugen. Eine bestätigte B-Request bleibt bis zur
physischen Übernahme downstream-limitiert, auch wenn der alte Snapshot bereits
beendet und mehrere Fenstergrenzen vergangen sind. Erst ein B-eigenes Fenster
mit `sourceRequestSequence == B` kann für B `RequestFullyExecuted` liefern;
die Severity bleibt danach gemäß der monotonen Regel bestehen.

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
`evaluateTemperatureControl()`-Aufruf vor dem internen
`outstandingEvaluation`-Consume wird jedoch verhindert (Abschnitt 6.1), nicht
als `nullopt` an #22 weitergereicht und nicht als neue Evaluation registriert.
Nach dem Consume erzeugt der nächste #22-Aufruf eine neue Evaluation, die der
Orchestrator selbst für den nächsten Planner-Tick bereithält; manuelle
Feedback- oder Evaluationkopien im Caller gibt es nicht.

An jeder erfolgreich committed Lifecycle-Grenze ruft der Orchestrator den
gemeinsamen Planner-Stop auf. Er übergibt
`ExistingEpisodeOpen`, wenn kein `outstandingEvaluation` besteht, und
`ClosedByOutstandingEvaluation`, wenn #22 das vorige Feedbackfenster bereits
konsumiert und die neue Evaluation noch nicht vom Planner beobachtet hat. Nur
im ersten Fall darf ein noch offenes vertrauenswürdiges aktives Subjekt nach
der Trusted-Sequence-Regel als `Rejected` handoff-fähig gemacht werden. Im
zweiten Fall wird der alte Planner-`acceptedCommand` nicht resurrected; ein
unverbrauchter `outstandingEvaluation` wird terminal verworfen, damit kein
altes Ergebnis in die nächste Episode gelangt. Nur wenn kein vertrauenswürdiges
aktives Subjekt vorhanden ist, wird der Application-Slot auf `nullopt`
geschlossen. Ein fehlgeschlagener Persistence-Commit verändert weder Planner
noch Slot.

### 9.5 `ActuatorAdmissionOutcome` als separater Diagnosevertrag

```cpp
enum class ActuatorAdmissionOutcome : std::uint8_t {
    NoCandidate,
    Accepted,
    MalformedCandidate,
    MalformedSafetyGate,
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
  -> Orchestrator speichert A intern als outstandingEvaluation
  -> nächster tickActuatorPlan() übergibt A genau einmal intern an planner.tick()
  -> Planner erzeugt die erste reale Disposition für A; Orchestrator speichert sie

Mehrere Planner-Ticks ohne neue Evaluation
  -> A bleibt unverändert, Disposition kann bei realer Governance einmalig
     auf DeferredOrLimited oder NoIntegratorConstraint aktualisiert werden
  -> nur der zuletzt gültige Update-Stand bleibt im Application-Slot

evaluateTemperatureControl() für Evaluation n+1
  -> Orchestrator nimmt {A, <letzte Disposition>} einmalig und löscht den Slot
  -> #22 erhält genau A; kein Caller kopiert ein Planner-Feedbackfeld

#22 Evaluation n+1 -> Cooling Request B
  -> das vorige A-Feedback ist beim #22-Aufruf geschlossen; Orchestrator hält B
     als outstandingEvaluation zurück
  -> erst der nächste tickActuatorPlan() beobachtet B genau einmal
  -> B wird als aktuelle Request angenommen, aber nicht sofort als physische
     Gegenrichtung ausgeführt
  -> während mindestens zwei alten Fenstergrenzen: altes A-Fenster darf nur
     enden; kein neues A-Fenster aus B, kein B-Guthaben; A darf danach kein
     Feedback mehr erzeugen
  -> nach bestätigter B-Anforderung und erfüllter Arming-Regel wird B aus
     der dann aktuellen B-Quote neu geplant; bis dahin B-Feedback deferred

NoValidRequest oder OFF als neue Evaluation
  -> altes A-Feedback bleibt geschlossen; es gibt kein spätes A-Feedback
  -> physischer Stop erfolgt gemäß I-6 beziehungsweise N-2

Stale-on-arrival B als neue Evaluation
  -> ausschließlich `{B, Rejected}` wird eröffnet, niemals A

Malformed neue Evaluation
  -> Planner löscht das interne Handoff auf nullopt, ohne A oder eine
     unsichere B-Sequence zu behaupten
  -> der nächste #22-Aufruf erhält kein fremdes altes Feedback und friert
     gemäß seinem bestehenden Vertrag konservativ ein

Ein direkter `evaluate n -> evaluate n+1`-Aufruf ohne zwischenliegenden
Planner-Consume wird von der Application verhindert und fail-closed beantwortet;
`n+1` ersetzt weder A noch wird es als beobachtete Planner-Evaluation gezählt.
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
        applicationPendingFeedback,
    std::optional<TemperatureControlResult>& outstandingEvaluation);
```

Ablauf (identisch für alle sieben Grenzen; **keine** davon ruft
`applyExternalWatchdogFaultReset()` auf, Abschnitt 8.6):

1. Der Orchestrator bestimmt ausschließlich aus seinem
   `outstandingEvaluation`-Slot den internen Episodenwert und ruft
   `ActuatorPlanTickResult result = planner.forceStop(
   nowMonotonicMillis, feedbackEpisodeAtStop);` auf. Bei leerem Slot gilt
   `ExistingEpisodeOpen`; bei belegtem Slot gilt
   `ClosedByOutstandingEvaluation`. Der Planer muss dadurch weder einen
   Caller-übergebenen Sequence-Wert noch eine Feedbackkopie akzeptieren.
   – berechnet denselben Ergebnistyp wie `tick()`: physisch `Idle`;
   Akkumulator/`activeWindow`/Gegenrichtungskandidat/`acceptedCommand` werden
   verworfen. Nur bei `ExistingEpisodeOpen` darf ein noch offenes
   vertrauenswürdiges Feedbacksubjekt vor dem Löschen genau ein Update
   `{sequence, Rejected}` erzeugen. Bei
   `ClosedByOutstandingEvaluation` erzeugt `forceStop()` kein Feedback,
   selbst wenn `acceptedCommand` noch A enthält. Wenn der Ausgang zuvor
   physisch aktiv war, setzt `forceStop()` den realen
   `lastPhysicalDeactivationDirection`- und Zeitanker; ein bereits laufender
   Außenlüfter-Nachlauf wird nicht verkürzt oder abrupt beendet.
2. `driver.apply(result);` – dieselbe Übersetzungsfunktion wie bei jedem
   gewöhnlichen Tick (Abschnitt 12); keine zweite Ausgabelogik.
3. Danach: `plannedDirection() == Idle`,
   `state().lastAppliedDirection == Idle`,
   `state().acceptedCommand == std::nullopt`,
   `state().activeWindow == std::nullopt` und
   `state().pendingFeedback == std::nullopt`. Ein Update aus dem
   `ExistingEpisodeOpen`-Fall darf bei einem nichtterminalen Stop einmalig in
   den Application-Slot übernommen werden; an der erfolgreichen
   Lifecycle-Grenze wird es dagegen zusammen mit dem alten
   `outstandingEvaluation` atomar terminal ausgemustert und nicht in eine neue
   Episode eingespeist. `outstandingEvaluation` wird in derselben Grenze
   geleert. Der Heartbeat-/Episodenanker wird nur gelöscht, wenn die Grenze die
   überwachte Episode verlässt; `latchedWatchdogFault` bleibt unverändert
   bestehen, falls zuvor gesetzt.

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
  Einzelfällen; keine vorsorgliche Generalisierung ohne aktuellen Bedarf.
  Die drei kleinen zusätzlichen Reasons `CounterDirectionConfirming`,
  `WindowPulseDeferred` und `WindowPulseMissed` sind keine Enum-Explosion,
  sondern die minimalen
  Diagnosewerte, die die exhaustive N-5-Tabelle und den Issue-#23-Vertrag
  tatsächlich verlangt (Abschnitt 8.2).

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
- Der Watchdog-Zeitanker ist davon getrennt: Er gilt nur innerhalb einer
  `isTemperatureControlledProcessState()`-Episode, startet dort frisch,
  verwendet vor dem ersten H den Episodenanker und wird beim Verlassen
  gelöscht. NewActiveRun/Recovery rebased die Episode, löschen aber niemals
  das latched Fault-Evidence.
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
   (Abschnitt 6.2), Watchdog-Episodenanker und laufender Watchdog (6.4), Prioritätsleiter Klasse I/N
   (8.2), overflow-sichere Zeitarithmetik (8.3), `forceStop()`,
   `applyExternalWatchdogFaultReset()`.
3. **Fenster, Akkumulator, Mindestzeiten, Totzeit, Richtungswechsel** –
   Fensterlogik inklusive Arming-Regel und O(1)-Fortschritt (8.1), einziger
   Akkumulator mit vollständigen Verwurfsregeln (8.4), bestätigter
   Richtungswechsel (8.5), `sourceRequestSequence`-Ownership und Klasse N der
   Prioritätsleiter vollständig.
4. **Lüfterlogik** – Außen-/Innenlüfter (10), eigener, von der
   Fenster-/Mindestzeitlogik klar getrennter Codeabschnitt.
5. **Feedback-Dispositionsmatrix** – vollständige Umsetzung von Abschnitt 9
   (`pendingFeedback`-Handoff, Trusted-Sequence-Regel, Severity-Merge,
   Episodengrenze bei `newEvaluation`, planmäßiger Window-Off als
   `NoIntegratorConstraint`, ereignisgetriebene Update-Regel,
   `sourceRequestSequence`-Nachweis und Live-Nachführung) innerhalb von
   `tick()`/`forceStop()`.
6. **Sink-Driver und Application-/Lifecycle-Integration** –
   `ActuatorPlanSinkDriver` (12), Erweiterung von
   `TemperatureControlApplicationOrchestrator` um `tickActuatorPlan()`
   (mit der in Abschnitt 6.1 festgelegten Aufrufsequenz), geschützter
   `outstandingEvaluation`-Invariante, vollständiger öffentlicher
   `TemperatureControlEvaluationEvidence`-API-Migration und gemeinsamer
   Lifecycle-Stop-Integration (11); keine Composition-Root-Verdrahtung.
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

**Window-Ownership (R6.2, Variante A):** Zwei gleichgerichtete Requests A/B
mit deutlich unterschiedlichen Quotes werden im selben natürlichen Fenster
eingespeist. Das laufende Fenster behält `sourceRequestSequence == A`; ein
Feedbacksample vor und nach der Fenstergrenze darf B nicht als durch A
ausgeführt markieren. Erst das B-eigene Folgefenster trägt
`sourceRequestSequence == B`. Die Disposition ist bis dahin
`DeferredOrLimited` mit dem tatsächlich eingetretenen Reason und kann danach
höchstens monoton verschärft werden.

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
`outstandingEvaluation = std::nullopt` tut dies ausdrücklich nicht und führt
die laufende Freigabe unverändert fort.
**Feedback-Vertrag (R4.4/R4.5/R6.1):** Das n/n+1/n+2-Orakel für Heating -> Cooling
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
Nach bereits konsumiertem A-Feedback erzeugen `NoValidRequest` und OFF kein
spätes A-Feedback; eine aktive stale-on-arrival-B-Request erzeugt nur
`{B, Rejected}`. `forceStop()` vor der Entstehung von
`outstandingEvaluation` darf das noch offene A-Fenster genau einmal als
`{A, Rejected}` verschärfen. Nach Entstehung des Slots darf derselbe Stop A
nicht resurrecten. Beide Pfade werden mit demselben Orchestrator-Handoff
geprüft; eine Sequence erhält kein doppeltes Feedback.
`admissionOutcome` ist für jeden Fall aus Abschnitt 9.5 geprüft.

**Plan-Reason-Typtrennung (R6.4):** Jede N-5-Ursache wird einzeln geprüft:
`AccumulatingBelowThreshold`, `MinimumOffTimeHeld`,
`PolarityDeadTimeHeld`, `CounterDirectionConfirming`,
`WindowPulseDeferred` und `WindowPulseMissed`. Kein Test akzeptiert
`DeferredOrLimited` als
`ActuatorPlanReason`; die Feedback-Disposition wird separat geprüft.
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

**Watchdog-Episode und Fault-Evidenz (R6.3/R6.5):** Der erste Tick einer
neuen temperaturgeregelten Episode ohne Evaluation setzt den frischen
`watchdogEpisodeStartedAtMonotonicMillis`-Anker und trippt nicht sofort.
Ohne H lösen ein Tick knapp vor, exakt auf und knapp nach
`requestWatchdogMillis` ausgehend vom Episodenanker den Watchdog aus. Das
erste H innerhalb der Frist rebased auf den H-Zeitpunkt. Eine lange
Standby-/Service-Phase und ein neuer Run erzeugen keinen Soforttrip aus dem
alten Zeitanker; ein bereits gelatchter Fault bleibt über dieses Rebase
erhalten.

Der Watchdog-Trip erzeugt
`ActuatorWatchdogFaultEvidence`; eine danach eintreffende, ansonsten gültige
neue Request löscht ihn nicht; `NewActiveRun`-Boundary löscht ihn nicht;
`Recovery`/`Standby`/`Service`/`Fault`/`SafeBoot`/`LeaveTemperatureControl`
löschen ihn ebenfalls nicht; nur `applyExternalWatchdogFaultReset()` löscht
ihn; ein simulierter Neustart (neue `ActuatorPlanner`-Instanz) wird
ausdrücklich **nicht** als Ersatz für diesen Reset getestet oder behauptet.
Die direkte Sequenzfolge A=10 gültig, B=11 stale-on-arrival, anschließend
Watchdog prüft exakt `lastObservedSequenceHighWatermarkBeforeFault == 11`;
die Bezeichnung behauptet keine erfolgreiche H-Akzeptanz von B.

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

**Verspäteter erster Puls (R5.2):** Für Heating und Cooling werden ein Tick
exakt am Fensterstart, kurz danach, exakt bei
`elapsedInWindow = scheduledOnMillis - minimumOnMillis`, 1 ms danach und
nahe dem natürlichen Fensterende getrennt geprüft. Zusätzlich werden
`scheduledOnMillis == minimumOnMillis` (nur exakter Fensterstart zulässig)
und eine normale größere Quote geprüft. Jeder tatsächlich gestartete normale
Puls ist mindestens `minimumOnMillis` aktiv; ein verspäteter Versuch wird
einmalig als `DeferredOrLimited` verworfen und weder verlängert noch im
Folgefenster nachgeholt.

**Mindestzeiten/Totzeit und physische Deaktivierung (R4.1/R4.2):**
Normaler `NeutralOff` und `AirLimitBlockedOff` dürfen die
konkrete Mindest-On-Phase halten; jede I-Klasse schaltet im selben Tick aus.
Ein normaler Window-Off-Anteil in Heating und Cooling setzt bei jedem realen
Active -> Idle den physischen Deaktivierungsanker. Gleichgerichteter Restart
wartet nur `minimumOffMillis`; ein tatsächlicher Richtungswechsel
wartet Mindest-Off und Totzeit nach der Späteres-Ende-Regel. Beide Richtungen,
exakter Gleichstand, voller und nahezu voller Puls sowie der Richtungswechsel
aus bereits laufendem Window-Off werden getestet.

**Feedback bei normalem Duty-Off (R5.3/R5.4):** Dieselbe 30-%-Request wird
je Richtung zweimal geprüft: Der unmittelbar folgende #22-Sample trifft
einmal während des planmäßigen Peltier-EIN und einmal während des
planmäßigen Window-Off; beide liefern `NoIntegratorConstraint`, sofern kein
anderes #23-Gate wirkte. Minimum-Off-AUS, Polaritätstotzeit,
Gegenrichtungsbestätigung, Akkumulation unter Mindestimpuls und ein
verspäteter/verworfener Fensterpuls liefern dagegen `DeferredOrLimited`.
Eine Sequenz mit `DeferredOrLimited` wird nach späterer physischer
Freigabe nicht auf `NoIntegratorConstraint` zurückgestuft; eine Sequenz mit
`Rejected` bleibt `Rejected`. Eine neue Sequence beginnt unabhängig davon
frisch.
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

**Watchdog-Lebenszeichen (R5.1):** Die H-Regel wird direkt als Zustandsorakel
geprüft: fortlaufend neue Heating-, Cooling-, gültige OFF- und
`NoValidRequest`-Evaluationen setzen jeweils `lastNewRequestAcceptedAt...`
auf `now`; `NoValidRequest` schaltet Peltier im selben Tick aus, löst aber
keinen zusätzlichen Request-Staleness-Trip aus. `std::nullopt`, Replay,
malformed #22-Evaluation sowie stale-on-arrival Watchdog/Context verlängern
die Frist nicht. Parameterfehler, Safety `Unresolved`/`ImmediateStop` und
korruptes externes Safety-Gate werden getrennt geprüft: Sie fail-closen
physisch, ohne eine ansonsten valide neue #22-Request-Identität zu verfälschen.

**Priorität/Fail-closed:** malformed `ControlRequest` (Sequenz `0`,
unbekannte Richtung, `timeQuote` `NaN`/`Infinity`/außerhalb `[0,1]`,
Status/Request-Mismatch, ungültiger Kontext) -> `MalformedInput`,
sofortiger Verwurf; strukturell ungültiger `currentCanonicalContext` ->
ebenfalls `MalformedInput` (Klasse I-1); mehrere gleichzeitig
zutreffende Bedingungen (z. B. Safety `ImmediateStop` **und** Kontext-Stale
gleichzeitig) -> exakte Klassen-Reihenfolge aus 8.2.

**Trusted-Sequence-Fail-closed (R5.5/R6.1):** Safety, InvalidConfig,
Watchdog, StaleContext und `forceStop()` erzeugen nur dann bei einer
vertrauenswürdig bekannten aktiven Sequence deren `{sequence, Rejected}` bis
zum Single-use-Konsum, wenn dieses Feedbackfenster noch offen ist. Nach einer
neuen `outstandingEvaluation` wird das alte Subjekt nicht resurrected; eine
malformed Request-Identität erzeugt kein erfundenes Sequence-Feedback. Ein
korruptes externes Safety-Gate wird separat geprüft und führt bei ansonsten
valider neuer Request-Identität zu `Rejected` für diese neue Sequence.
`OFF`/`NoValidRequest` nach konsumiertem A-Feedback eröffnen kein neues
Feedbackfenster.

**Lifecycle/Stop und Fan-Nachlauf:** Für jede der sieben
`TemperatureControlLifecycleBoundary`-Werte ruft die Application-Grenze
denselben Stop-Pfad auf. `forceStop()` verwirft Akkumulator,
`activeWindow`, Gegenrichtung und `acceptedCommand` sowie den Planner-RAM-
Zustand; vor einem `outstandingEvaluation` wird ein noch offenes
Feedbacksubjekt genau einmal als `{sequence, Rejected}` übernommen, danach
bleibt der alte Slot `nullopt`. Der reale Active ->
Idle-Übergang setzt den
`lastPhysicalDeactivationDirection`/Zeitanker und lässt den
Außenlüfter-Nachlauf weiterlaufen; `latchedWatchdogFault` bleibt unverändert.
Der Lifecycle-Abschluss mustert ein nicht mehr passendes Rejected-Handoff
terminal aus, bevor eine neue Episode beginnen kann. Ein sofortiger
gleichgerichteter Restart respektiert weiterhin Minimum-Off. Stop und Fault
werden zusammen mit kurzem Duty-Puls, kürzerem/längerem Off-Anteil und
erneutem Puls im Nachlauf geprüft.
**Sequenzhochwasserzeichen:** `lastObservedSequenceHighWatermark` bleibt über
`forceStop()` innerhalb derselben `ActuatorPlanner`-Instanz erhalten; eine
neue Instanz (simulierter Neustart) beginnt regulär bei einer neuen,
niedrigeren Sequenz, ohne dies fälschlich als persistierten Zustand zu
behaupten.

**Architekturnachweis:** `ActuatorPlanner` kompiliert und wird getestet ohne
jede Abhängigkeit auf `device_platform`-Sink-Header.

### 19.2 Revision-7-Direktmatrix für Episoden-, Zeit- und Handoff-Grenzen

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
14. H setzt das Watchdog-Lebenszeichen für jede neue gültige HEAT-, COOL-,
    OFF- und NoValidRequest-Evaluation; Replay, malformed #22-Evaluation und
    stale-on-arrival Watchdog/Context setzen es nicht. NoValidRequest schaltet
    Peltier aus, erzeugt aber keinen zusätzlichen Staleness-Trip.
15. Für beide Richtungen werden verspätete erste Ticks exakt am Fensterstart,
    kurz danach, bei scheduledOn-minus-minimumOn, 1 ms später, bei
    scheduledOn == minimumOn und nahe dem Fensterende geprüft; kein realer
    normaler Puls unterschreitet minimumOnMillis.
16. Eine identische 30-%-Request liefert während des planmäßigen EIN und des
    planmäßigen Window-Off jeweils NoIntegratorConstraint; Minimum-Off,
    Deadtime, Counterdirection, Akkumulation und verspäteter Puls liefern
    DeferredOrLimited.
17. Die Severity je Sequence wird als
    NoIntegratorConstraint -> DeferredOrLimited -> Rejected geprüft; keine
    spätere Freigabe stuft zurück, ein planmäßiger Window-Off-Anteil
    verschärft nicht.
18. Safety/InvalidConfig/Watchdog/StaleContext und forceStop erzeugen bei
    vertrauenswürdiger aktiver Sequence deren Rejected-Feedback; eine
    malformed Request-Identität erzeugt kein erfundenes Sequence-Feedback;
    korruptes Safety-Gate wird bei valider Request-Identität separat geprüft.
19. evaluate n -> interner Planner-Consume n -> evaluate n+1 wird erzwungen;
    evaluate n+1 ohne Consume wird fail-closed verhindert und erzeugt keinen
    zweiten Planner-Kandidaten.
20. Alle in Abschnitt 19.5 gefundenen Evidence-/evaluate-Callsites werden
    kompiliert und gezielt regressionsgeprüft; kein öffentlicher
    caller-supplied Evidence-Feedbackpfad bleibt bestehen.
21. A-Feedback wird durch `evaluate n+1` konsumiert; danach führt eine neue
    `NoValidRequest`-Evaluation zu keinem A-Feedback, ebenso eine neue OFF-
    Evaluation. Der Test fragt den nächsten #22-Aufruf ab und prüft
    `nullopt`/konservative #22-Reaktion statt einer verspäteten A-Sequence.
22. Nach konsumiertem A-Feedback erzeugt eine aktive stale-on-arrival-
    Evaluation B ausschließlich `{B, Rejected}`; A darf weder aus
    `acceptedCommand` noch aus `activeWindow` als Feedbacksubjekt erscheinen.
23. Safety-/Watchdog-I-Ereignis ohne neue Evaluation verschärft das noch
    offene Subjekt A korrekt auf `{A, Rejected}`; derselbe Stop nach
    Entstehung von `outstandingEvaluation` erzeugt kein altes A-Feedback.
24. Zwei gleichgerichtete Quotes A/B mit deutlich unterschiedlichen Werten
    prüfen `sourceRequestSequence`: laufendes Fenster A bleibt A, B ist vor
    seinem Folgefenster nicht `RequestFullyExecuted`, und das Folgefenster
    trägt B. Die Orakel liegen vor und nach der Fenstergrenze.
25. Der erste Planner-Tick einer neuen überwachten Episode ohne Evaluation
    setzt den Episodenanker; knapp vor, exakt auf und nach der Watchdogfrist
    wird der Trip deterministisch geprüft. Ein erstes H rebased die Frist.
26. Lange Standby-/Service-Zeit und danach NewActiveRun beziehungsweise
    Recovery erzeugen keinen Soforttrip aus einem alten H-/Episodenanker;
    `latchedWatchdogFault` bleibt durch jedes Rebase erhalten.
27. Jede N-5-Zeile wird auf ihren echten `ActuatorPlanReason` geprüft;
    insbesondere erscheinen `CounterDirectionConfirming`,
    `WindowPulseDeferred` und `WindowPulseMissed`, während
    `DeferredOrLimited` ausschließlich in der Feedback-Disposition vorkommt.
28. A=10 wird gültig angenommen, B=11 stale-on-arrival verworfen, danach
    trippt der Watchdog. Das Evidence-Orakel prüft das ehrlich benannte
    `lastObservedSequenceHighWatermarkBeforeFault == 11` und keine falsche
    `lastAccepted`-Semantik.

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
  nachweislich beide Resets aus, leert `outstandingEvaluation` und behandelt
  ein trusted aktives Stop-Subjekt nach der Trusted-Sequence-Regel.
- **Geschützte Evaluation-Reihenfolge (R5.6):**
  `evaluate n -> tickActuatorPlan()` übergibt n genau einmal intern als
  `newEvaluation`; mehrere weitere Planner-Ticks erhalten `nullopt`. Ein
  direkter `evaluate n+1`-Aufruf vor diesem Consume wird ohne zweiten
  #22-Kernaufruf fail-closed verhindert; n bleibt unverändert ausstehend.
  Nach dem Consume darf n+1 erzeugt und im nächsten Planner-Tick beobachtet
  werden. Der Test deckt auch `NoValidRequest` als zu beobachtende Evaluation
  ab.
- **Internes Feedback-Handoff:** Mehrere Planner-Ticks speichern nur das
  neueste geänderte Update; genau der nächste #22-Aufruf erhält es genau
  einmal. Das öffentliche Evidence-Objekt enthält kein caller-supplied
  `previousControlRequestFeedback` mehr. Ein malformed #22-Ergebnis bei
  pending A löscht das Handoff auf `nullopt`, erzeugt kein Sequence-Feedback
  und löst keinen Replay aus; eine malformed Safety-Evidence bei valider
  Request-Identität erzeugt dagegen `{sequence, Rejected}`.
- `ActuatorPlanner&`/`ActuatorPlanSinkDriver&` werden über die
  gesamte Lebenszeit des Test-Fixtures unverändert referenziert (Objektlebenszeit-
  Nachweis gemäß Abschnitt 14.1, kein Rebinding).

### 19.5 `test/test_temperature_control/test_temperature_control.cpp` und vollständige API-Callsite-Migration

Vor Umsetzung wird die vollständige Repository-Suche aus Abschnitt 3 mit
`rg` wiederholt. Alle direkten Konsumenten von
`TemperatureControlEvaluationEvidence`,
`previousControlRequestFeedback` und
`evaluateTemperatureControl()` werden im API-Migrationsschnitt geprüft:

- Application-Fixtures entfernen die öffentliche Feedback-Feldinitialisierung
  und prüfen stattdessen, dass nur der Orchestrator-Slot in das private
  `TemperatureControlInput` gelangt.
- Direkte #22-Tests des reinen `TemperatureControlInput`-Vertrags behalten
  ihre expliziten `previousControlRequestFeedback`-Orakel; sie sind keine
  caller-supplied Evidence-API und werden gegen die unveränderte
  PI-Mathematik regressionsgeprüft.
- Alle `evaluateTemperatureControl()`-Callsites werden auf die geschützte
  Application-Semantik migriert; kein Caller kopiert mehr ein Planner-
  Feedbackfeld in Evidence.

Mindestens ein Regressionstest bestätigt, dass `TemperatureControlResult`
und `TemperatureControlEvaluationEvidence` nach der Migration dieselben
#22-Status-/Reason-/AirLimit-/Request-Orakel liefern und dass die API-Änderung
keine PI-Mathematik, keine Sequenzsemantik und keinen Feedbackfenstervertrag
verändert.

## 20. Dokumentations- und Roadmapwirkung sowie Plan-SHA-Governance

- `docs/ACTUATOR_TIMING.md`: nach Umsetzung Ergänzung der akzeptierten
  Struktur im Abschnitt „Akzeptierte Entscheidungen", ohne die dort
  weiterhin offenen `TBD_COMMISSIONING`-Werte zu verändern.
- Kein neues ADR erwartet: Modulzuordnung folgt unverändert ADR-013,
  Zustandsautomat-Trennung unverändert ADR-014.
- `docs/THIRD_PARTY_COMPONENTS.md` bleibt unverändert.
- **Governance:** Issue #106 wurde live bereits um I106.R1 präzisiert und ist
  in dieser Revision unverändert offen; es gibt in diesem Auftrag keine
  zusätzliche #106-Scope-Erweiterung. Diese Revision-7-Planänderung wird als
  eigener, abgeschlossener Plan-Commit committet. Erst danach ist die exakte
  Revision-7-Plan-SHA bekannt und wird im Draft-PR #105, im
  `SESSION HANDOVER` und im PR-Body ausgewiesen.
  Anschließend wird `docs/ROADMAP.md` in einem separaten, rein redaktionellen
  Metadaten-Commit auf Revision 7 und diese exakte Plan-SHA synchronisiert.
  Vor diesem Roadmap-Commit werden alle aktuellen Statusstellen, insbesondere
  „Naechste fachliche Arbeit“, nach alten Revision-4/5-SHAs durchsucht;
  Plantext und Roadmap-Metadaten werden nicht vermischt.

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
