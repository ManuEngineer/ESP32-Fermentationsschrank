# Issue #218 – R1-Regelungs- und Aktorkette integrieren (Plan Revision 1)

Status: **Plan zur Review und Ownerfreigabe der exakten Plan-Commit-SHA.** Keine
Implementierung, kein Image, kein Flash, keine reale Schaltung vor Freigabe.
Baseline: `origin/main` `5bbdef8173073c92c0737ed72d2193457dff6136` (PR #217 gemergt).
`ACTUATOR_RELEASE=NO`, `REAL_PELTIER_TEST=NOT_RUN`. Auftrag: Owner-Datei
`ESP32_Issue218_Codex_Integrationsauftrag.md`, Issue #218.

## 1. Ziel und Nicht-Ziele

Ziel: die bereits vorhandenen Bausteine (Sensorquellen, Qualitaetspipeline, Run-/
Persistenzzustand, `ActuationInterlock`, `TemperatureController`,
`TemperatureControlApplicationOrchestrator`, `ActuatorPlanner`, `ActuatorPlanSinkDriver`,
`innerFanOutput`/`outerFanOutput`/`peltierBridge`) an den **fehlenden Aufrufstellen**
verbinden und diese Kette digital fail-closed nachweisen.

Nicht-Ziele: kein neuer Regelalgorithmus, Planner, Hardwaretreiber, RTC-Adapter,
Sensor-FSM, zweites Safety-Gate oder paralleler Persistenz-/Parametervertrag; keine
erfundenen `TBD_COMMISSIONING`-Werte, kein synthetisches `Allowed`, kein Flash, keine
reale Peltier-Bestromung, keine Wiederholung realer GPIO-/Luefter-/DS18B20-/BTS7960-
Einzelpruefungen. #34/#35 (Messwerte, Parameter), #36/#37 (Abnahme) bleiben ausserhalb.

## 2. Gap-Analyse (am Quellstand `5bbdef8` verifiziert)

Vorhanden und wiederzuverwenden: `Ds18b20Sampler::source(ch)` (`ITemperatureSource`);
`SensorQualityPipeline::ingest/snapshot`; `ActuationInterlock::evaluate`;
`TemperatureControlApplicationOrchestrator::{evaluateTemperatureControl, tickActuatorPlan,
persist*}` inkl. Feedback-Handoff und Lifecycle-Reset (`complete()`); Planner mit
Mindestzeiten, Totzeit, Nachlauf, Request-Watchdog; Driver mit Aussenluefter-vor-
Richtung-Reihenfolge; Bruecke mit gegenseitigem Interlock; Run-Snapshot
`actuatorPlannerParametersSnapshot` (Unconfigured bei fehlendem Snapshot → kein Betrieb).

Fehlende Produkt-Aufrufstellen (alle per Repository-Suche belegt):

| # | Luecke | Beleg |
|---|---|---|
| G1 | Keine `SensorQualityPipeline`-Instanz im Produkt; Sampler-Quellen erreichen Regler/Safety nie. `FermentationApplication::publishOwningRuntimeEvidence` hat ausser Tests **keinen** Aufrufer (UI/Web sehen daher nie Live-Temperaturen). | `grep SensorQualityPipeline|publishOwningRuntimeEvidence` ueber `main/`, `lib/` |
| G2 | Kein `SensorQualityConfig` mit Produktwerten; Medianfenster, Tiefpass, Plausibilitaet, Stale-/Fehlergrenzen sind `TBD_COMMISSIONING` (`SENSOR_TUNING_COMMISSIONING.md`). | `grep SensorQualityConfig` ausserhalb der Pipeline: keine Treffer |
| G3 | `FermentationApplication` persistiert direkt ueber `RunPersistenceCoordinator` (`fermentation_application.cpp` `applyConfirmedPrepared` ~L731/735, `confirmProductInserted` ~L765, Recovery-/Resume-/Fallback-Pfade ~L2448–2559). Der Orchestrator (Lifecycle-Reset von Regler/Planner/Driver, `beginRun`, Feedback) wird nur von #29-Probe und Tests instanziiert. Der Orchestrator bietet keine Varianten fuer `persistBorrowedCommand`. | `grep TemperatureControlApplicationOrchestrator` |
| G4 | `ActuationInterlock::evaluate` hat **keinen** Produktaufrufer (nur `evaluate({})` in der #29-Probe); es gibt keinen Evidenz-Bauer (`ActuationEvidence`: Boot, Konfiguration, Persistenz, Sensor, explizite Aktivierung, Aktivierungsergebnis). | `grep ActuationInterlock::evaluate` |
| G5 | Kein periodischer Tick in der `app_main`-Schleife: `evaluateTemperatureControl` → `tickActuatorPlan` (Gate) wird nie aufgerufen; `innerFanOutput`/`outerFanOutput`/`peltierBridge` sind keinem `ActuatorPlanSinkDriver` zugeordnet. | `app_main.cpp` L609–L650, Hauptschleife |
| G6 | Composition-Guard `test_peltier_composition_guard` verbietet `ActuatorPlanner`/`ActuatorPlanSinkDriver` in `app_main.cpp` und `setForward(`/`setReverse(` in `main/`; muss gezielt auf den neuen Komposer eingegrenzt werden (wie in #33). | Testquelle |
| G7 | Produktive Planner-Parameter (`ActuatorPlannerParameters`) sind alle `TBD_COMMISSIONING`/#35; ohne Konfigurationsproducer bleibt der Run-Snapshot leer. | `ACTUATOR_TIMING.md`, ROADMAP #106 |

**Konsequenz:** Nach #218 ist die Kette im Produkt vollstaendig **verdrahtet**, bleibt
aber ohne #35-Werte **zwingend fail-closed** (G2/G7: keine gueltige Sensorqualitaet, kein
Planner-Snapshot → `Unresolved`/`Unconfigured` → alle Ausgaenge AUS). Das ist gewollt und
kein Defekt; Wertefreigabe ist #35.

## 3. Umsetzung (kleinster Patch-Scope)

**C1 – Sensorevidenz-Komposition (fermentation_app/main, Reuse).**
Ein schmaler Evidenz-Zusammenbau (kein neuer Algorithmus): pro Rolle (Luft, Produkt,
Kuehlkoerper/Peltier) eine `SensorQualityPipeline`, gespeist aus `Ds18b20Sampler::source(ch)`
ueber die persistierte Rollenbindung (`sensor_commissioning`), Snapshot mit
`monotonicMillis` → `CrossRolePlausibilityContext` → `publishOwningRuntimeEvidence` und
`TemperatureControlEvaluationEvidence`. `SensorQualityConfig` kommt aus einem injizierten
Provider; **im Produkt liefert er bis #35 `nullopt`** (keine Pipeline → kein `Valid`).
Testwerte nur in Testfixtures.

**C2 – Orchestrator als einzige Persistenzgrenze der Application.**
Orchestrator um die von der Application tatsaechlich genutzten Borrowed-/Recovery-
Varianten ergaenzen, die **denselben** `complete()`-Pfad nutzen; `FermentationApplication`
haelt einen optionalen Orchestrator-Zeiger (abstrakte Ports, ADR-013) und ruft ihn an
allen mutierenden Coordinator-Aufrufstellen (G3-Liste, C0-Inventur vollstaendig) statt des
Coordinators direkt. Ohne angeschlossene Aktorik: Verhalten unveraendert.

**C3 – Evidenz-Bauer und Tick.**
`ActuationEvidence` ausschliesslich aus vorhandenem Application-/Konfigurations-/
Persistenz-/Sensorzustand ableiten; `explicitActivationRequested`/`activationKind`/
`activationPersistenceResult` nur aus dem realen Start-Kommando (#17-Persistenz Applied),
nie bei Boot/Reset/Recovery. Ein Tick (`evaluateTemperatureControl` → `ActuationInterlock::
evaluate` → `tickActuatorPlan`) pro Schleifendurchlauf in `app_main`, STOP/Fault/Watchdog/
SafeBoot laufen ueber die bestehenden Lifecycle-Boundaries.

**C4 – Composition-Root.**
`app_main.cpp` konstruiert `ActuatorPlanner`, `ActuatorPlanSinkDriver(peltierBridge,
outerFanOutput, innerFanOutput)` und den Orchestrator und uebergibt sie; `OUT3` (Summer)
und `OUT4` (Reserve) bleiben unverbunden. Guard (G6) gezielt auf die Composition-Stelle
begrenzen, `setForward(`/`setReverse(` bleiben ausserhalb von `ActuatorPlanSinkDriver`
verboten. RTC/SNTP: unveraendert, fehlende vertrauenswuerdige UTC sperrt weiter den Start.

## 4. Tests (wenige, gezielt)

Neuer Hosttest `test/test_r1_control_chain_integration` (Test-Fixtures mit expliziten,
als Testwerte markierten `ActuatorPlannerParameters`/`SensorQualityConfig`; keine
Produktkonstanten):

- **T1 positiv:** gueltiger, explizit gestarteter, persistierter Run + frische Pflichtsensoren
  (Luft, Kuehlkoerper) → Regler → Planner → Aussenluefter EIN **vor genau einer**
  Brueckenrichtung; Planner-Feedback erreicht den naechsten Regler-Tick.
- **T2 negativ (parametrisiert):** Pflichtsensor fehlt/`Stale`/`Failed`; Aussenluefter-EIN
  verweigert; unsicherer Boot-/Run-/Persistenzzustand; Request-Watchdog; STOP → Bruecke
  AUS/gesperrt, Nachlauf und Richtungs-Totzeit unveraendert.
- **T3 Produkt-Default:** Produktkomposition ohne Konfigurationswerte → alle Sinks AUS,
  kein `Allowed`.
- Regression nur betroffener Bereiche: `test_actuator_planner`, `test_actuation_interlock`,
  `test_temperature_control*`, `test_run_persistence_coordinator`, `test_peltier_composition_guard`,
  `test_actuator_plan_sink_driver`, Architekturpruefung `check_architecture_boundaries.py`.
- Builds: beide ESP-IDF-Profile (`esp32_bringup`, `esp32_release`) ohne Flash; Stack-/
  Flash-Delta dokumentieren.

Pre-Ready-Lauf nur nach Independent Review und Owner-Anweisung (AGENTS.md).

## 5. Gates und Entscheidungen

Kein Hardware-Gate in diesem PR. **Ownerentscheidungen:**

- **D1:** Plan-Freigabe dieser exakten SHA.
- **D2:** Bestaetigung, dass #218 im Produkt ohne #35-Werte **verdrahtet, aber fail-closed**
  endet (Sensorqualitaets- und Plannerparameter-Provider liefern bis #35 nichts).
- **D3:** Orchestrator-Zeiger in `FermentationApplication` (C2, empfohlen: kleinster
  Schnitt, ein Persistenzpfad) statt Komposition der Persistenz ausserhalb der Application.
  Alternative ist ein zweiter Persistenzweg und widerspricht dem Vertrag.

## 6. Abgrenzung

#32/#33/#190 Hardwareevidence unveraendert (`HW-33-06` belegt nur den unbelasteten
Leerlauf). #106: Per-Run-Vertrag wird konsumiert, produktive Werte #35. #34/#35, #36/#37
offen. `ACTUATOR_RELEASE=NO`.
