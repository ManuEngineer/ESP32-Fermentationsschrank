# Issue #218 – R1-Regelungs- und Aktorkette integrieren (Plan Revision 2)

Status: **Plan zur Review und Ownerfreigabe der exakten Plan-Commit-SHA.** Keine
Implementierung, kein Image, kein Flash, keine reale Schaltung vor Freigabe.
Baseline: `origin/main` `5bbdef8173073c92c0737ed72d2193457dff6136` (PR #217 gemergt). Rev. 2 korrigiert nur C2/C4-Ownership und C3-Freigabeevidenz (Korrekturauftrag PR #219).
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
| G3 | `FermentationApplication` (privater Coordinator, private Borrowed-Operation, privater Run) persistiert direkt ueber `RunPersistenceCoordinator` (`fermentation_application.cpp` `applyConfirmedPrepared` ~L731/735, `confirmProductInserted` ~L765, Recovery-/Resume-/Fallback-Pfade ~L2448–2559). Der Orchestrator (Lifecycle-Reset von Regler/Planner/Driver, `beginRun`, Feedback) wird nur von #29-Probe und Tests instanziiert. Der Orchestrator bietet keine Varianten fuer `persistBorrowedCommand`. | `grep TemperatureControlApplicationOrchestrator` |
| G4 | `ActuationInterlock::evaluate` hat **keinen** Produktaufrufer (nur `evaluate({})` in der #29-Probe); es gibt keinen Evidenz-Bauer (`ActuationEvidence`: Boot, Konfiguration, Persistenz, Sensor, explizite Aktivierung, Aktivierungsergebnis). | `grep ActuationInterlock::evaluate` |
| G5 | Kein periodischer Tick in der `app_main`-Schleife: `evaluateTemperatureControl` → `tickActuatorPlan` (Gate) wird nie aufgerufen; `innerFanOutput`/`outerFanOutput`/`peltierBridge` sind keinem `ActuatorPlanSinkDriver` zugeordnet. | `app_main.cpp` L609–L650, Hauptschleife |
| G6 | Composition-Guard `test_peltier_composition_guard` verbietet `ActuatorPlanner`/`ActuatorPlanSinkDriver` in `app_main.cpp` und `setForward(`/`setReverse(` in `main/`; muss gezielt auf den neuen Komposer eingegrenzt werden (wie in #33). | Testquelle |
| G7 | Produktive Planner-Parameter (`ActuatorPlannerParameters`) sind alle `TBD_COMMISSIONING`/#35; ohne Konfigurationsproducer bleibt der Run-Snapshot leer. | `ACTUATOR_TIMING.md`, ROADMAP #106 |

**Konsequenz:** Nach #218 ist die Kette im Produkt vollstaendig **verdrahtet**, bleibt
aber ohne #35-Werte **zwingend fail-closed** (G2/G7: keine gueltige Sensorqualitaet, kein
Planner-Snapshot → `Unresolved`/`Unconfigured` → alle Ausgaenge AUS). Das ist gewollt und
kein Defekt; Wertefreigabe ist #35.

## 3. Umsetzung (kleinster Patch-Scope)

**Festgelegte Ownership (eine Struktur, Rev. 2).** `main/app_main.cpp` kann weder den
privaten `runPersistenceCoordinator_` noch `runtimeRunState_` erreichen und erhaelt sie
auch nicht. Daher gilt: **`FermentationApplication` besitzt die gesamte Regelkette
intern**; `app_main` uebergibt nur die drei bereits vorhandenen, von `app_main`
besessenen abstrakten Sinks (ADR-013, nur Ports aus `device_platform`).

- **Anschluss:** neue schmale Methode `FermentationApplication::attachActuationPorts(
  IBidirectionalActuatorSink& peltier, IBinaryOutputSink& outerFan, IBinaryOutputSink&
  innerFan)`, aufrufbar **vor** `begin()`; speichert nur Referenzen (Zeiger). Nach
  erfolgreichem `begin()` ist ein Aufruf wirkungslos abgelehnt. Ohne Aufruf (alle Tests,
  #29-Probe, Bring-up-Harnesses) bleibt das Verhalten bytegleich unveraendert.
  Lebensdauer: die Sinks in `app_main` (L609–L650) sind vor `application` (L698)
  deklariert und werden daher nach ihr zerstoert.
- **Entstehung:** in `beginPersistent()` **unmittelbar nach** dem Erzeugen des
  `runPersistenceCoordinator_` (~L2242) legt die Application einen `ControlChain`-
  Verbund als **privates Mitglied** an (`TemperatureController`,
  `TargetQualificationEvaluator`, `ActuatorPlanner`, `ActuatorPlanSinkDriver`,
  `TemperatureControlApplicationOrchestrator` mit Referenz auf `*runPersistenceCoordinator_`;
  ein `unique_ptr`, nur wenn Ports angeschlossen). Planner startet ohne Run-Snapshot
  (`Unconfigured`, Ausgaenge AUS).
- **Abbau (alle Pfade):** an jeder Stelle, an der der Coordinator zurueckgesetzt wird
  (`begin()`-Wiederholung ~L787, Fehlerpfade/`beginPersistent()` ~L2155/2156, Destruktor)
  zuerst `planner.forceStop` + `driver.apply` (alle drei Sinks AUS), dann Verbund
  zerstoeren, **danach** Coordinator. So sind Referenzen nie dangling; ein fruehzeitiger
  Initialisierungsfehler hinterlaesst nie eine aktive Kette.
- **Persistenzpfad unveraendert:** `beginCommandOperation`/`commandCandidate`/
  `persistBorrowedCommand` und `CommandDecisionView` bleiben privat bei der Application;
  kein zweiter Coordinator, keine oeffentlichen Run-/Speicherzugriffe, keine Kopie.
  Der Orchestrator erhaelt **eine** neue oeffentliche, schmale Post-Commit-Methode
  `handoffCommitted(before, RunPersistenceResult&, RunCommandState& current, nowMs,
  recoveryBoundary)`, die **nur** den bestehenden privaten `complete()`-Pfad aufruft
  (Hint-Verbrauch ueber `consumeCommittedControlContextTransition`, Lifecycle-Reset,
  `beginRun`, Feedback-Loeschung). Die Application ruft sie direkt nach jedem bestehenden
  Coordinator-Aufruf auf, dessen `status == Applied` ist; das Ergebnis wird per
  Referenz uebergeben und der Hint damit **genau einmal** verbraucht. Die bisherigen
  `persist*`-Wrapper des Orchestrators bleiben fuer Tests unveraendert.
- **C0 – Inventur (vor Code, in Evidence):** alle mutierenden Coordinator-Aufrufstellen
  der Application, klassifiziert: (a) Laufzeitmutationen (`applyConfirmedPrepared`
  ~L731/735, `confirmProductInserted` ~L765, Sensorselektion) → `handoffCommitted(...,
  false)`; (b) Boot-/Recovery-/Resume-Aktivierungen (`activateR1EligibleRun`,
  `discardAsNoActiveRun`, `activateFallbackRecoveredRun`, `resumeFallback`, Recovery-
  Aufloesung; ~L2448–2559, ~L2814–2880) → `handoffCommitted(..., true)`;
  (c) nicht mutierend (Evaluation/Load/Periodic-Checkpoint) → kein Handoff. Eine nicht
  klassifizierte Aufrufstelle blockiert den Review.

**C1 – Sensorevidenz.** Pro Rolle (Luft, Produkt, Kuehlkoerper/Peltier) eine
`SensorQualityPipeline`, gespeist aus `Ds18b20Sampler::source(ch)` ueber die persistierte
Rollenbindung; ein Zusammenbau in `main/` (nicht in der Application) liefert pro
Schleifendurchlauf `publishOwningRuntimeEvidence(CrossRolePlausibilityContext)` (bestehender
oeffentlicher Eingang, bisher ohne Produktaufrufer). `SensorQualityConfig` kommt aus einem
injizierten Provider, der **im Produkt bis #35 `nullopt`** liefert (keine Pipeline → kein
`Valid`). Testwerte nur in Fixtures.

**C3 – Tick und Freigabeevidenz je Tick (Rev. 2).** `FermentationApplication::update()`
fuehrt (nur bei vorhandener Kette und `ready()`) pro Aufruf unter dem bestehenden
`applicationCallSerializer_` genau einen Tick aus:
`evaluateTemperatureControl(*runtimeRunState_, evidence aus owningRuntimeEvidence_)` →
`ActuationEvidence` neu bauen → `ActuationInterlock::evaluate` → `tickActuatorPlan`. Kein
Zustand ausserhalb des Application-Locks, kein Zugriff von `app_main` auf Run-Objekte.

*Aktivierungs-Latch (nur RAM, kein persistentes Gate, kein neues Safety-Gate).* Die
Application haelt `activation_{kind, activeRunId, runRevision-at-activation}` und setzt es
**ausschliesslich** in `handoffCommitted`, wenn dasselbe `Applied`-Ergebnis (a) einen
expliziten Start (`RunStarted`/`ManualRunStarted` → `FreshStart`) oder eine explizit
bestaetigte Resume-Aktivierung (→ `Resume`) beschreibt. Daraus folgt pro Tick:
`explicitActivationRequested`, `activationKind`, `activationPersistenceResult=Applied`
und `processActivationApplied` werden **aus dem Latch plus dem aktuellen Live-Zustand**
abgeleitet, nicht gespeichert wiederverwendet. Der Latch gilt nur, solange **alle**
zutreffen: `runtimeRunState_->activeRunId` und Prozessphase sind unveraendert
temperaturgeregelt, kein Stop/Fault/Recovery/Standby/SafeBoot/Service-Boundary
(jeder `needsRuntimeReset`-Fall ausser dem aktivierenden Commit selbst loescht den Latch
**im selben `handoffCommitted`-Aufruf**), Coordinator-Zustand vertrauenswuerdig,
Konfiguration/Persistenz/Sensor/Planner (inkl. Watchdog-Latch) in diesem Tick frisch
gueltig. Neuer Run, Boot, `begin()`, Recovery, Stop, Fehler oder neue Run-Identitaet
loeschen den Latch; ein altes `Applied` kann deshalb nie einen anderen Lauf freigeben.
Gewoehnliche Phasenwechsel innerhalb desselben Laufs loeschen ihn nicht, weil der Latch
an `activeRunId`, nicht an `transitionSequence` haengt.
`loadDisposition` fuer die Auswertung wird aus der `activationKind` des Latch abgeleitet
(FreshStart → `Standby`, Resume → `ResumeOffer`), weil `ActuationInterlock::evaluate`
`Allowed` nur fuer diese beiden kennt; **Risiko R1:** falls die Interlock-Tests diese
Ableitung nach Stop/Neustart im selben Boot nicht tragen, wird gemeldet und eine
Ownerentscheidung eingeholt statt den Interlock still zu aendern.
STOP/Fault/Recovery enden die Freigabe sofort ueber die bestehenden Boundaries
(Latch weg im selben Aufruf → naechster Tick `Unresolved` → Bruecke/Luefter laut
bestehender Nachlauflogik AUS). Watchdog/Mindestzeiten/Totzeit unveraendert (Planner).

**C4 – Composition-Root.** `app_main.cpp` ruft nur `application.attachActuationPorts(
peltierBridge, outerFanOutput, innerFanOutput)` vor `application.begin()` und den
Sensor-Zusammenbau aus C1 in der Schleife auf; `ActuatorPlanner`/`ActuatorPlanSinkDriver`
erscheinen **nicht** in `app_main.cpp`. `OUT3` (Summer) und `OUT4` (Reserve) bleiben
unverbunden. Guard (G6): `setForward(`/`setReverse(` bleiben in `main/` verboten; die
Verbote gegen Planner/Driver in `app_main.cpp` bleiben bestehen; neu geprueft wird, dass
die Anbindung ausschliesslich ueber `attachActuationPorts` erfolgt. RTC/SNTP unveraendert
(aktuell `kRtcConfig{}` deaktiviert, separates R1-Thema, kein Teil von #218).

## 4. Tests (wenige, gezielt)

Neuer Hosttest `test/test_r1_control_chain_integration` ueber die **echte**
`FermentationApplication` mit Test-Sinks (Fixtures mit expliziten, als Testwerte markierten
`ActuatorPlannerParameters`/`SensorQualityConfig` ueber den normalen Run-Snapshot-/
Evidenzeingang; **kein** fixture-seitiges `Allowed`, der produktive Evidenzbauer wird
durchlaufen):

- **T1 positiv:** expliziter Start (`Applied`) + frische Pflichtsensoren (Luft,
  Kuehlkoerper) → Regler → Planner → Aussenluefter EIN **vor genau einer**
  Brueckenrichtung; **mindestens zwei aufeinanderfolgende positive Ticks desselben
  committed Runs** mit korrekter Feedback-Weitergabe (zweiter Tick erhaelt das Feedback
  des ersten); Phasenwechsel im selben Run haelt die Freigabe; danach STOP, Fault und
  Recovery → Bruecke sofort AUS/gesperrt, Latch geloescht, Nachlauf/Totzeit unveraendert.
- **T2 negativ (parametrisiert):** Pflichtsensor fehlt/`Stale`/`Failed`; Aussenluefter-EIN
  verweigert; unsicherer Boot-/Run-/Persistenzzustand; Request-Watchdog; Neustart
  (`begin()`) und neuer Run mit altem `Applied` → Peltier AUS/gesperrt.
- **T3 Produkt-Default:** Kette angeschlossen, aber ohne Konfigurationswerte → alle Sinks AUS,
  kein `Allowed`; ohne `attachActuationPorts` Verhalten unveraendert; `begin()`-Wiederholung
  und Initialisierungsfehler lassen alle Sinks AUS (Lebensdauer/Abbau).
- Regression nur betroffener Bereiche: `test_actuator_planner`, `test_actuation_interlock`,
  `test_temperature_control*`, `test_run_persistence_coordinator`, `test_peltier_composition_guard`,
  `test_actuator_plan_sink_driver`, `check_architecture_boundaries.py`.
- Builds: beide ESP-IDF-Profile (`esp32_bringup`, `esp32_release`) ohne Flash; Stack-/
  Flash-Delta dokumentieren.

Pre-Ready-Lauf nur nach Independent Review und Owner-Anweisung (AGENTS.md).

## 5. Gates und Entscheidungen

Kein Hardware-Gate in diesem PR. Der fertige PR #219 ist **noch keine lauffaehige
Peltier-Regelfreigabe**. **Ownerentscheidungen:**

- **D1:** Plan-Freigabe dieser exakten SHA.
- **D2:** #218 endet im Produkt **verdrahtet, aber fail-closed**, bis #35 die Sensor-/
  Plannerwerte liefert (Etappengrenze, keine geratenen Parameter).
- **D3 (Rev. 2):** Ownership gemaess §3: Application besitzt Kette, Anschluss nur ueber
  `attachActuationPorts` mit Ports, Post-Commit-Handoff `handoffCommitted` auf dem
  bestehenden privaten Persistenzpfad. Es wird **kein** Orchestrator-Zeiger von `app_main`
  und kein oeffentlicher Coordinator-/Run-Zugriff freigegeben.

## 6. Abgrenzung

#32/#33/#190 Hardwareevidence unveraendert (`HW-33-06` belegt nur den unbelasteten
Leerlauf). #106: Per-Run-Vertrag wird konsumiert, produktive Werte #35. #34/#35, #36/#37
offen. `ACTUATOR_RELEASE=NO`.
