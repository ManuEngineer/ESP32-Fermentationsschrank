# Issue #218 – Evidence: R1-Regelungs- und Aktorkette (digital)

Plan Rev. 3, freigegebene SHA `4b5e3e03681a179ef46f385e0f9d7f7586985ca7` (D1–D3 vom Owner freigegeben).
Code-Stand: `e54796c`. Nur digitale Nachweise; kein Flash, keine Hardware, keine Peltier-Bestromung.
`ACTUATOR_RELEASE=NO`, `REAL_PELTIER_TEST=NOT_RUN`.

## Umgesetzt (C0–C4)

- **C0 Inventur** der mutierenden Coordinator-Aufrufstellen in `FermentationApplication`:
  (a) Laufzeit: `applyConfirmedPrepared` (Borrowed-Command), `confirmProductInserted` (`persistTransition`);
  (b) Boot/Recovery: `processTerminalClassification` (`discardAsNoActiveRun`/`activateR1EligibleRun`),
  `resumeFallback` (`activateFallbackRecoveredRun`); (c) nicht mutierend: `evaluateCurrentFermentingRecovery`,
  `prepareRecoveryEvaluationState`, `loadAndInitializeInto`. Nicht in der Application aufgerufen:
  `persistSensorSelection`, `checkpointPeriodic`, `activateLoadedRun`, `resolveRecoveryOutcome`.
  Alle (a)/(b) rufen `handoffControlChain`; Factory-Reset (Coordinator-Austausch) baut die Kette ab und neu auf.
- **C2** `TemperatureControlApplicationOrchestrator::handoffCommitted` (oeffentlich, ruft nur den bestehenden
  `complete()`-Kern in-place), `ControlChain` (privat in `FermentationApplication`), `attachActuationPorts`
  (vor `begin()`), endgueltiger Abbau mit explizitem Idle-Ergebnis (beide Luefter AUS) getrennt vom Lauf-STOP.
- **C3** Aktivierungs-Latch (RAM, nur explizites `StartProgram`/`StartManualHolding`, gebunden an `activeRunId`,
  geloescht bei jeder Lifecycle-Boundary im selben `handoffCommitted`-Aufruf und je Tick bei Run-/Phasenwechsel),
  Evidenz je Tick neu gebaut, genau eine Evidenzpublikation pro Tick (kein Alter-/Timeout-Wert erfunden).
- **C1** `ControlSensorEvidenceProducer` (reine Komposition vorhandener `SensorQualityPipeline`, keine eigene Logik).
- **C4** `app_main`: ein `attachActuationPorts`-Aufruf (nicht in den #32/#33-Harness-Builds), Konstanten
  `kControlChainParametersUnconfiguredUntilIssue35` und `kSensorQualityConfigUnconfiguredUntilIssue35`.

## Tests (alle PASS, native, Stand `e54796c`)

- Neu: `test_r1_control_chain_integration` 10/10 (SIM-218-01..03), `test_peltier_composition_guard` 5/5 (SIM-218-00).
- Regression (29 Suiten, 1081 Testfaelle, alle PASS): `test_actuator_planner`, `test_actuation_interlock`,
  `test_temperature_control`, `test_run_persistence_coordinator`, `test_actuator_plan_sink_driver`,
  `test_sensor_selection_architecture_guard`, `test_factory_reset_flow`, `test_fermentation_ui_commands`,
  `test_issue144_run_identity`, `test_sensor_commissioning`, `test_press_dispatcher`, `test_user_settings_application`,
  `test_configuration_service`, `test_web_application_routes`, `test_issue90_product_recovery_oracle`,
  `test_control_context`, `test_target_qualification`, `test_sensor_quality_pipeline`, `test_local_touch_ui`,
  `test_display_language_application`, `test_run_commands`, `test_sensor_selection`, `test_shared_enable_bridge_sink`,
  `test_sensor_actuator_mocks`, `test_ui_steady_state_allocations`, `test_process_state_machine`, `test_ds18b20_sampling`.
- Builder-Self-Check `scripts/run_pre_ready_gates.sh self-check` (clang-format/clang-tidy 21): `BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS` (erster Lauf FAIL wegen Special-Member-Regel in `ControlChain` und Kognitiv-Komplexitaet 26>25 in `beginAuthorizedFactoryReset`, beides in `4614583` behoben).
- `scripts/check_architecture_boundaries.py`: PASS. `clang-format` (esp-clang 21.1.3) auf allen geaenderten Dateien: PASS.
- Mutationsproben: Frische-Pflicht entfernt -> SIM-218-02 (stale) FAIL; Latch-Loeschung entfernt -> SIM-218-01/-06 FAIL;
  finales Idle entfernt -> Abbau-Test FAIL. Nach Rueckbau jeweils wieder PASS.
- ESP-IDF-Builds `esp32_bringup` und `esp32_release` (`scripts/build_esp_idf_profiles.py all --require-clean-source-tree`): PASS,
  0 Compiler-Warnungen; Release-App `1 805 120` B (41 % der kleinsten App-Partition frei), Bring-up `1 815 600` B (Stand Code-HEAD `4614583`, nach den clang-tidy-Korrekturen neu gebaut).
- Statische Stackframes (Release, `-fstack-usage`): `tickControlChain` 560 B, `evaluateTemperatureControl` 640 B,
  `TemperatureController::evaluate` 384 B, `ControlSensorEvidenceProducer::sample` 736 B, `handoffControlChain` 96 B;
  neue Kette in der Hauptschleife < 3 KB statisch, Main-Task-Stack 24 576 B. **Keine** Laufzeitmessung (kein Flash).

## Befunde / Grenzen (nicht verschwiegen)

1. **Produkt bleibt fail-closed (D2):** Mit den benannten Unconfigured-Konstanten entsteht keine `ControlRequest`, ohne
   Sensorqualitaetskonfiguration kein `Valid`-Sensor und damit kein startbarer Run. Der fertige PR ist **keine**
   lauffaehige Peltier-Regelfreigabe; Werte kommen mit #35.
2. **Innenluefter-Regel des Planners:** In jeder temperaturgeregelten Phase schaltet der bestehende Planner den Innenluefter EIN,
   unabhaengig vom Gate (`ActuatorPlanner::updateFanState`, unveraendert). Das betrifft nur einen laufenden Run mit gebundenen
   Planner-Parametern; im Produkt unerreichbar, solange #35 fehlt.
3. **Resume-Aktivierung nicht verdrahtet:** Die Application hat keinen expliziten Resume-Annahmepfad (`pendingResume_` wird nur
   angeboten), und `ActuationInterlock` verweigert `FallbackRecovered` in R1. Der Latch kennt deshalb nur `FreshStart`; Resume
   bleibt fail-closed (Plan nannte Resume optional).
4. **AbortAndCool / CoolAfterCompletion** (`RunStarted`-Effekt) sind Lifecycle-Boundary, **keine** Aktivierung (fail-closed);
   Bedienpfad ist zudem ueber die O5-Sperre der Kuehlplan-Requests nicht erreichbar, daher kein Test.
5. **Factory-Reset-Pfad:** Der Coordinator-Austausch ruft den endgueltigen Abbau (nur AUS-Befehle ueber den vorhandenen Driver).
   Der bestehende Guard `SIM-19-S1-03` bleibt unveraendert gruen (Sink-Interfaces nur im Driver, `ActuatorPlanSinkPorts` dort definiert).
6. **Sensor-Offsets:** Die Pipeline-Komposition setzt keine Kalibrierung (Offsets gehoeren zu #34); Quellen liefern Rohwerte.
7. **Latch bindet an `activeRunId`** (nicht `runRevision`): eine Laufanpassung desselben Runs behaelt die Freigabe, der Planner
   verwirft veraltete Kontexte ueber seine bestehende Revisionspruefung.
8. **Risiko R1 (Plan):** `loadDisposition` fuer den Interlock wird aus dem Latch (`FreshStart` -> `Standby`) abgeleitet; die
   Interlock-Tests und der Integrationstest tragen das (Neustart/Stop im selben Boot getestet). Interlock unveraendert.
9. Bring-up-Harness-Builds (#32/#33) haengen die Kette nicht an, damit der Tick die manuell gesteuerten Sinks nicht ueberschreibt.
