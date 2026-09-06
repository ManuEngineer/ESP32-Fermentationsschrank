# Akzeptanztests und Release-Gates

## Status

Dieses Dokument definiert die verbindlichen Testebenen, Fehlerinjektionen,
Hardwareabnahmen und Release-Gates fuer Release 1. Die Korrekturen aus den
Reviews von PR #38 sind integriert. Exakte thermische Grenzwerte,
Regelparameter und Ressourcenschwellen bleiben bis zu den jeweiligen Messungen
`TBD_COMMISSIONING` beziehungsweise `TBD_IMPLEMENTATION_BUDGET`.

## Grundsaetze

- Sicherheitskritische Funktionen werden gezielt unter Fehlerbedingungen getestet.
- Native Tests ersetzen keine Hardwaretests; Hardwaretests ersetzen keine
  deterministischen Softwaretests.
- Jeder formelle Test verweist auf eine Anforderung, Entscheidung, Fehlernummer
  oder Sicherheitsregel.
- Ein Test darf keine unkontrollierte Aktorfreigabe oder Umgehung der normalen
  Sicherheitslogik verlangen.
- Hardwaretests verwenden den bestaetigten Hardwarestand, die dokumentierte
  Verdrahtung und einen gespeicherten Servicebericht.
- Ein nicht ausgefuehrter Test ist `BLOCKED` oder `NOT_RUN`, nicht bestanden.
- Ein bestandener Einzeltest hebt keinen anderen aktiven Sicherheitsfehler auf.
- Ein Neustart gilt nie als Fehlerreset.
- `SAFE_BOOT` bleibt in allen Tests aktorfrei.

## Issue #24 Release-1-Testgrenze

Der #24-Schnitt testet nur reale R1-Pfade: ResetCause-Diagnose mit
all-off/`Unresolved`, frische Config-/Persistenz-/Sensorvalidierung,
`NoActiveRun` fuer integer nicht resumefaehige Laeufe, technisch untrusted
Load als `SAFE_BOOT`, die #17-Gesamttransaktion, die reale #20/#21-
Sensorprojektion, den #23-Current-Boot-Watchdog-Latch, Ack ohne Freigabe und
die E3/E5/#106-Negativgrenzen.

Ein Restart-Zaehler, Resetzeitfenster, persistenter allgemeiner Safety- oder
Watchdog-Latch, Service-PIN, automatische `SAFETY_RECOVERY`, Fallback-
Promotion, gewichteter Recoveryfortschritt sowie neue Thermal-/Hardwarefaults
sind keine #24-R1-Testfaelle. Historische Testpunkte dazu bleiben fuer ihre
spaeteren Issues/Hardware-Gates gekennzeichnet und gelten nicht als #24-
Abnahmekriterium.

## Issue #90 R5.9: getrennte Nachweise

Fuer die spaetere #90-Persistenz-/Recoveryverifikation werden technische
Backendcharakterisierung und das hoehere Produkt-Recovery-Gate getrennt und
maschinenlesbar ausgewiesen:

```text
backend_characterization:
    observed | known_limitation | unexpected_change

product_recovery_gate:
    PASS | FAIL | NOT_RUN
```

Callback 12/`NotFound` bleibt als sichtbare
`BACKEND_POWER_CUT_CHARACTERIZATION` / `KNOWN_BACKEND_LIMITATION` erhalten und
ist kein Backend-PASS. Slice 2 darf mit dem
`SimulatedPersistentStateStore` erwartete Produktoutcomes deterministisch im
Produktorakel pruefen. Ein finaler #90-Produkt-Recovery-PASS ist jedoch nur
zulaessig, wenn zusaetzlich jeder relevante real aus der gepinnten
NVS-/BDL-Charakterisierung hervorgehende Cut-/Recoveryzustand durch die
hoehere Produktions-Recovery laeuft:

```text
real charakterisierter NVS-/BDL-Cut-Zustand
-> Reinitialisierung / Reboot
-> vollstaendiges Reload
-> Record-/Envelope-/CRC-/Schema-/StorageEpoch-Pruefung
-> Generation/Root/Manifest/Fallback bzw. rc0/rc1/rh0
-> Prepared/Orphan/Indeterminate-Klassifikation
-> Produkt-Recovery-Outcome
-> stateless ActuationInterlock / Safe-Boot / logischer Actuator-Gate
```

Ein Simulator-PASS plus separat dokumentierte reale Backendcharakterisierung
reicht nicht fuer den finalen Produkt-PASS, solange die realen Zustaende nicht
auf Produktebene geprueft wurden. Callback 12 darf Backend-FAIL / Known
Limitation bleiben und zugleich zu einem sicheren Produkt-Recovery-PASS
fuehren, wenn die hoehere Ebene den realen Zustand korrekt erkennt und
fail-closed behandelt. Die gesamte Verifikation bleibt actor-free; UI und
physische Aktorsicherheit sind kein #90-Gate.

## Testebenen

### Ebene 1: Native Unit-Tests

Mindestens:

- Programm- und Konfigurationsvalidierung
- kanonische Zustandsuebergaenge
- Bootprioritaet fuer jede Resetcause, aktuelle Persistenzintegritaet und
  fail-closed `SAFE_BOOT`
- Wiederherstellung eines persistierten `COMPLETED`
- virtuelle monotone und absolute Zeit
- `C2-Legacy/#18`: Ausfallzeit als Unter-/Obergrenze und kein automatischer
  Phasenabschluss bei ueberlappendem Unsicherheitsintervall; kein #24-R1-Gate
- Zielqualifikation und Gnadenzeit
- PI-Reglerkern und Luftbegrenzung
- Impulsakkumulator
- Mindest-Einschaltzeit, Mindest-Ausschaltzeit und Totzeit
- Sensorstatus `VALID`, `STALE`, `FAILED`
- Regelsensorauswahl (Issue #21): vollstaendige Startmatrix ueber alle
  Programmpraeferenzen und Produktvaliditaeten; kanonische
  Entscheidungsfunktion fuer automatischen und manuellen Pfad identisch;
  laufzeitseitiger Auswahlzustand ausserhalb des Wireformats, fail-closed
  nach Restore; strukturell ungueltige externe Kompatibilitaetsevidenz
  blockiert nur die Rueckkehr, nicht unabhaengige Sicherheitsreaktionen
- begrenzte FaultCode-/Disposition-Projektion, Mehrfachfehler, Quittierung ohne
  Safetywirkung und code-spezifische positive Clear-Pfade
- Persistenzschema, atomare Revisionen und Rueckfall
- Transaktionsabsicht vor aktorwirksamer Zustandsaenderung
- #17-Transaktionsstatus und Bootauswertung ohne neue Safety-Persistenz
- reale Config-Producerprojektion ohne zweite Configuration-FSM; normale
  abgelehnte Mutationen bei gueltigem Operational-Runtime bleiben ohne
  `SAFE_BOOT`
- Unknown-Producer-Bits bleiben bei fehlender Quelle aktiv und loeschen sich nur
  durch einen spaeteren bekannten Wert derselben Quelle
- kritischer Schreibfehler sperrt neue Aktoranforderungen vor weiteren
  Persistenzversuchen; #17-Status und RAM/FSM bleiben ohne neuen persistenten
  Safety-Latch unknown-safe
- ein Fehler vor dem ersten dauerhaften #17-Write bleibt `Unchanged`; ein
  Fehler nach `PreparedHead` bleibt `BlockedIndeterminate`/`Changed`
- unvollstaendiger Transaktionsmarker fuehrt beim Boot zu `SAFE_BOOT`
- Resume-Angebot bleibt `Unresolved`; Resume und Fresh Start werden erst nach
  dem bestehenden Gesamtstatus `Applied`, FSM-Anwendung und frischer Evidenz
  freigeschaltet
- echter Fresh-Start-Bridge vom Start-Command ueber den #17-Gesamtstatus bis
  zur `ActuationInterlock`-Permission `Allowed`; Fehler vor `PreparedHead` und unaufgeloestes
  `CommitOutcomeUnknown` bleiben `Unresolved`
- normaler `Success` benoetigt keinen zweiten Readback; Readback erfolgt nur
  zur Aufloesung von `CommitOutcomeUnknown` durch `writeExact()`
- Aufbewahrung und Bereinigung
- physische Vollreset-/Service-PIN-Tests gehoeren zu spaeteren E4/E5-/Service-
  Gates und sind kein #24-R1-`ActuationInterlock`-Gate
- Device-Shell mit Header, exakt vier festen Slots, Home-/Zurueck-Hierarchie
  und sichtbaren leeren Slots
- gemeinsame rendererunabhaengige View-Modelle, Commands, strukturierte
  Command-Ergebnisse, Bestaetigungen und Snapshotaktualisierung fuer Touch/Web
- Textfallback aktive Sprache -> Englisch -> sichtbarer technischer Schluessel,
  Theme-Standardfallback und 320-x-240-Textlaengenvertrag
- lokale Servicefreigabe: 10 Minuten Inaktivitaet, kein UI-Parameter, keine
  R1-Maximaldauer sowie Sperre bei Neustart, Abmelden und Safetyzustandswechsel

Tests sind reproduzierbar und unabhaengig von realer Uhrzeit, Netzwerk und
zufaelliger Taskplanung.

### Ebene 2: Simulierte Gesamt- und Fehlerablaeufe

Mindestens:

- Standby -> Vorheizen -> Produkt einsetzen -> Zielqualifikation -> Fermentation
- luftgefuehrter Lauf ohne Produktfuehler
- Produktfuehlerausfall, Luft-Ersatzbetrieb (manuell und automatisch nach
  Wartezeit) sowie manuelle und automatisch validierte Rueckkehr
  (Issue #21): im aktiven Luft-Ersatzbetrieb (`AirFallbackActive`) bleibt die
  Regelung ueber Luft weiterhin freigegeben, solange Schrankluft- und
  Kuehlkoerperfuehler gueltig sind - ein ungueltiger Produktfuehler allein
  sperrt dort nicht; erst die Rueckkehr zu `NormalProduct` verlangt Produkt-,
  Schrankluft- und Kuehlkoerperfuehler gemeinsam gueltig
  (Sicherheits-Vorrangregel). Ein einzelner Schrankluft-/Kuehlkoerperausfall
  waehrend Ersatzbetrieb sperrt dagegen sofort in den sicheren Zustand
  (`SafeLocked`); Re-Arm nach einem abgebrochenen Rueckkehrversuch nur bei
  neuer Evidenzgeneration (geaenderte Kompatibilitaetsrevision oder
  zwischenzeitlicher erneuter Produktausfall), kein unbegrenztes Wiederholen
- Heizen, Neutralbereich, Kuehlen und Richtungswechsel
- Stromunterbrechung in jeder Prozessphase
- jede Resetcause: all-off/`Unresolved`, vollständige Revalidierung, keine
  Restart-Akkumulation und keine Aktorfreigabe allein aus Recovery
- #124-Current-`FERMENTING`: bei exakter gültiger Current-Evidenz und trusted
  UTC automatische logische Recovery ohne Benutzerbestätigung; ohne aktuelle
  UTC `RecoveryEvaluation/WaitingForTrustedTime` RAM-only, ohne
  Persistenzmutation und mit verweigerter Aktorpermission
- Resume-Phasenmatrix: `PREHEATING`, `COOLING` und `MANUAL_HOLDING` behalten
  `ResumeOffer`; non-resumable trusted Phasen behalten `NoActiveRun`.
  Ältere gültige Checkpoints bleiben nicht-aktivierende Angebote und werden
  weder automatisch resumed noch promotet.
- unvollstaendige Persistenztransaktion -> `SAFE_BOOT`
- kritischer Persistenzschreibfehler -> sofortige Aktorsperre, sichere
  Abschaltung und bestehender #17-Coordinator-/unknown-safe-Zustand; kein
  neuer allgemeiner Current-Boot-RAM-Latch
- Watchdog: neue Request und Ack loeschen nicht; expliziter Reset nur ueber
  den bestehenden #23-Pfad mit aktueller Evidenz
- `NoActiveRun`-Abschluss: `PreparedHead -> CheckpointSlot -> CommittedHead`
  und erst nach `Applied` Standby anwenden
- korrupter Kontrollpunkt -> bestehender technischer #17-Speichervertrag und
  fail-closed Recovery. Ein vollstaendig validierter aelterer Fallback darf im
  #90-Orakel als `OLDER_VALID_CHECKPOINT_RESUME`-Angebot klassifiziert werden,
  aber nicht automatisch resume, promoten oder `Allowed` werden; erst
  explizites Resume, `Applied`, FSM und frische Safety-Evidenz oeffnen den
  weiteren Gatepfad
- `COMPLETED` bleibt nach Neustart `COMPLETED`
- kein Service- oder Aktortest aus `SAFE_BOOT`
- Quittierung ohne Fehlerreset
- Benutzerentscheidung bei `WARNING_REQUIRES_ACTION`
- Touchnavigation ohne Wischgeste, sichtbares Pressfeedback, keine
  Doppelausloesung und erster Wake-Touch ohne Command
- SAFE_BOOT mit reduziertem aktorfreiem Diagnose-/Recoveryzugang, getrennt von
  normalem PIN-Service und von Raw-Touch-Kalibrierungsrecovery

Die Simulation prueft erwartete Zustaende, Meldungen, Revisionen und abstrakte
Aktorbefehle. Eine verbotene Aktorfreigabe laesst den Test fehlschlagen.

### Issue #26 – lokale Touch-Shell und Workspace

Die native Consumer-Simulation führt den kanonischen Projector über die
generische Shell in den Fermentations-Workspace. Sie prüft insbesondere:

| ID | Consumer-Nachweis |
|---|---|
| SIM-26-01 | Lifecycle-/Process-Projektion von Bereit, Aktiv, Wartet, Abgeschlossen, Eingeschränkt, Recovery und technischem Unavailable; `ServiceRequired` bleibt `Restricted` und überlagert Recoverydaten. |
| SIM-26-02 | Genau vier sichtbare Slots, Home/Back-Hierarchie, vertikaler Pager und erster Touch im Idle-Zustand als `WakeOnly` ohne Command. |
| SIM-26-03 | ManualHolding und ManualTimed bleiben getrennte UI-Intents; ManualTimed konsumiert `ManualTimedRunValues` über `prepareStartManualTimed()` und erzeugt keine UI-eigene Identität. |
| SIM-26-04 | ProductInsertedConfirmed verwendet ausschließlich die erwartete kanonische Zustandsrevision, `decideProcessTransition()` und bleibt vor einem owning Apply `DecisionOnly`. |
| SIM-26-05 | Numerische/Text-Editoren halten nur flüchtige Kandidaten; Validierung und Commit bleiben bei Programmmodell, Preview und ConfigurationService. |
| SIM-26-06 | Maskierte PIN-Eingabe projiziert ownergelieferte Pending-/Retry-/Accepted-/Rejected-Zustände und verändert keine Safety-/Aktorfreigabe. |
| SIM-26-07 | SAFE_BOOT-Ziele sind bestehenden Ownern zugeordnet: #57 Werksreset, #31 Raw-Touch/Kalibrierung, #89 Netzwerk/Provisionierung und #28 Diagnose/Export; #26 implementiert diese Owner nicht vorzeitig. |

Die bestehenden #144- und #152-Contract-Regressionen bleiben die
Provenienztests für Run-Identity, ManualTimed-Quelle, Schema-5-Persistenz und
Recovery. Diese #26-Simulation behauptet keine Display-, Touchcontroller-,
elektrische oder thermische Hardwareabnahme.

#### Direkter SIM-26-Trace-Index für PR #143

Die folgende Zuordnung ist die ausführbare Planmatrix: Jeder Plan-ID ist ein
konkreter Test beziehungsweise ein statischer Trace zugeordnet. Die mit
`existing-owner` markierten Tests bleiben Nachweise der jeweiligen bestehenden
Ownerverträge; sie werden von #26 nur konsumiert. Die Zuordnung ist kein
Hardware- oder Pre-Ready-Nachweis.

| Plan-ID | Direkter Test/Trace |
|---|---|
| SIM-26-01 | `test_fermentation_ui_models::test_projector_home_modes_follow_lifecycle_and_process_matrix`; `test_local_touch_ui::test_sim_26_workspace_action_matrix_and_owner_paths` |
| SIM-26-02 | `test_local_touch_ui::test_sim_26_workspace_action_matrix_and_owner_paths`; `test_device_ui_contracts::test_shell_has_exactly_four_slots_and_home_back_hierarchy` |
| SIM-26-03 | `test_fermentation_ui_models::test_projector_builds_shared_snapshot_without_surface_state`; `test_local_touch_ui::test_workspace_has_fixed_slots_and_manual_paths_are_separate` |
| SIM-26-04 | `test_device_ui_contracts::test_build_catalog_and_clock_contract_remain_renderer_independent`; `test_local_touch_ui::test_sim_26_shell_locale_and_service_boundaries` |
| SIM-26-05 | `test_local_touch_ui::test_sim_26_navigation_and_non_command_slots`; `test_device_ui_contracts::test_shell_has_exactly_four_slots_and_home_back_hierarchy` |
| SIM-26-06 | `test_fermentation_ui_editing::test_program_list_and_mutations_use_catalog_ownership`; `test_local_touch_ui::test_sim_26_program_editor_actions_are_real_requests` |
| SIM-26-07 | `test_run_commands::test_program_start_is_two_stage_and_contains_summary`; `test_local_touch_ui::test_sim_26_manual_and_program_consumer_paths` |
| SIM-26-08 | `test_process_state_machine::test_product_confirmation_starts_target_reach` (existing-owner); `test_run_persistence_coordinator::test_product_inserted_commits_before_advancing_and_restores` (existing-owner) |
| SIM-26-09 | `test_local_touch_ui::test_sim_26_workspace_action_matrix_and_owner_paths`; `test_run_commands::test_stop_back_is_inert_and_abort_off_is_atomic` (existing-owner) |
| SIM-26-10 | `test_local_touch_ui::test_sim_26_workspace_action_matrix_and_owner_paths`; `test_run_commands::test_completion_can_return_to_standby_or_start_manual_cooling` (existing-owner) |
| SIM-26-11 | `test_local_touch_ui::test_sim_26_message_sensor_and_recovery_actions`; `test_run_commands::test_message_priority_acknowledgement_and_mute_are_independent` (existing-owner) |
| SIM-26-12 | `test_local_touch_ui::test_sim_26_shell_locale_and_service_boundaries`; `test_device_ui_contracts::test_platform_sections_precede_isolated_application_sections` |
| SIM-26-13 | `test_local_touch_ui::test_sim_26_navigation_and_non_command_slots` |
| SIM-26-14 | `test_local_touch_ui::test_sim_26_workspace_action_matrix_and_owner_paths`; `test_device_ui_contracts::test_command_outcome_categories_stay_bounded` |
| SIM-26-15 | `test_local_touch_ui::test_shell_wake_is_first_touch_and_frame_is_deterministic` |
| SIM-26-16 | `test_fermentation_ui_models::test_refresh_revision_changes_only_on_new_publication`; `test_device_ui_contracts::test_touch_and_web_session_policies_remain_separate` |
| SIM-26-17 | `test_local_touch_ui::test_pin_model_is_masked_and_owner_states_are_display_only` |
| SIM-26-18 | `test_local_touch_ui::test_sim_26_shell_locale_and_service_boundaries`; `test_device_ui_contracts::test_expired_session_activity_cannot_resurrect_or_move_backwards` |
| SIM-26-19 | `test_device_ui_contracts::test_touch_and_web_session_policies_remain_separate` |
| SIM-26-20 | `test_run_persistence_coordinator::test_r1_time_pending_is_ram_only_and_rechecks_same_revision` (existing-owner); `test_local_touch_ui::test_sim_26_message_sensor_and_recovery_actions` |
| SIM-26-21 | `test_local_touch_ui::test_sim_26_workspace_action_matrix_and_owner_paths`; `test_run_persistence_coordinator::test_fallback_pending_never_allows_before_recovery_apply` (existing-owner) |
| SIM-26-22 | `test_boot_classification::test_all_load_outcomes_map_to_the_r1_boot_classification` (existing-owner); `test_local_touch_ui::test_sim_26_workspace_action_matrix_and_owner_paths` |
| SIM-26-23 | `test_actuation_interlock::test_recovery_evaluation_actuation_is_blocked` (existing-owner); `test_actuation_interlock::test_fallback_selection_required_never_allows_even_with_complete_evidence` (existing-owner) |
| SIM-26-24 | `test_local_touch_ui::test_sim_26_shell_locale_and_service_boundaries`; `test_device_ui_contracts::test_shell_has_exactly_four_slots_and_home_back_hierarchy` |
| SIM-26-25 | `test_local_touch_ui::test_sim_26_shell_locale_and_service_boundaries` (`SimulatedDeviceShellFrame::splash`) |
| SIM-26-26 | `test_local_touch_ui::test_sim_26_shell_locale_and_service_boundaries`; `test_device_ui_contracts::test_text_resolver_uses_active_then_english_then_visible_key` |
| SIM-26-27 | `test_fermentation_ui_models::test_refresh_revision_changes_only_on_new_publication`; `test_fermentation_ui_commands::test_canonical_validation_precedes_ui_confirmation` |
| SIM-26-28 | `test_fermentation_ui_commands::test_canonical_validation_precedes_ui_confirmation`; `test_run_commands::test_apply_run_command_staleness_regression_for_sensor_selection_and_other_commands` (existing-owner) |
| SIM-26-29 | `test_local_touch_ui::test_sim_26_navigation_and_non_command_slots`; `test_fermentation_ui_commands::test_ui_payloads_are_intents_and_not_owning_evidence` |
| SIM-26-30 | `test_fermentation_ui_models::test_projector_home_modes_follow_lifecycle_and_process_matrix`; `test_process_state_machine::test_boot_service_recovery_and_completion_topology_is_explicit` (existing-owner) |
| SIM-26-31 | `test_fermentation_ui_commands::test_product_inserted_decision_uses_state_revision_without_apply`; `test_process_state_machine::test_product_confirmation_starts_target_reach` (existing-owner) |
| SIM-26-32 | `test_fermentation_ui_commands::test_proposed_decision_is_not_reported_as_applied`; `test_run_persistence_coordinator::test_stale_invalid_and_time_mismatched_transitions_write_nothing` (existing-owner) |
| SIM-26-33 | `test_fermentation_ui_commands::test_product_inserted_decision_uses_state_revision_without_apply` |
| SIM-26-34 | `test_run_commands::test_processed_command_ids_form_a_bounded_rolling_window` (existing-owner); `test_run_persistence_coordinator::test_unknown_outcome_is_resolved_by_exact_readback_and_duplicate_is_safe` (existing-owner) |
| SIM-26-35 | `TRACE: rg -n 'RunPersistenceCoordinator' lib/fermentation_app/src/fermentation_touch_workspace.*` (zero matches); `python3 scripts/check_architecture_boundaries.py` |
| SIM-26-36 | `test_fermentation_ui_commands::test_proposed_decision_is_not_reported_as_applied`; `test_fermentation_ui_commands::test_command_result_preserves_typed_app_details` |
| SIM-26-37 | `test_run_commands::test_manual_start_summary_is_available_before_confirmation_but_never_masks_rejections` (existing-owner); `test_run_persistence_coordinator::test_manual_run_qualification_reaches_holding_via_application_path` (existing-owner) |
| SIM-26-38 | `test_run_commands::test_manual_timed_uses_timed_path_and_fixed_manual_sensor_semantics` (existing-owner); `test_local_touch_ui::test_sim_26_manual_and_program_consumer_paths` |
| SIM-26-39 | `test_fermentation_ui_editing::test_numeric_edit_model_keeps_actions_transient` |
| SIM-26-40 | `test_fermentation_ui_editing::test_text_edit_model_has_mode_and_commit_without_validation` |
| SIM-26-41 | `test_fermentation_ui_editing::test_program_list_and_mutations_use_catalog_ownership`; `test_configuration_service::test_program_editor_consumes_the_opening_catalog_revision` |
| SIM-26-42 | `test_fermentation_ui_editing::test_program_list_and_mutations_use_catalog_ownership`; `test_local_touch_ui::test_sim_26_program_editor_actions_are_real_requests` |
| SIM-26-43 | `test_local_touch_ui::test_sim_26_program_editor_actions_are_real_requests`; `test_fermentation_ui_editing::test_program_list_and_mutations_use_catalog_ownership` |
| SIM-26-44 | `test_configuration_service::test_program_catalog_expected_revision_is_checked_under_preview_lock`; `test_configuration_service::test_program_editor_consumes_the_opening_catalog_revision` |
| SIM-26-45 | `test_local_touch_ui::test_pin_model_is_masked_and_owner_states_are_display_only`; `test_fermentation_ui_models::test_projector_home_modes_follow_lifecycle_and_process_matrix` |
| SIM-26-46 | `test_local_touch_ui::test_sim_26_shell_locale_and_service_boundaries`; `test_device_ui_contracts::test_touch_and_web_session_policies_remain_separate`; `test_actuation_interlock::test_fresh_start_stays_unresolved_until_new_run_is_applied` (existing-owner) |
| SIM-26-47 | `test_local_touch_ui::test_sim_26_workspace_action_matrix_and_owner_paths`; `test_boot_classification::test_fallback_recovered_requires_explicit_selection` (existing-owner) |
| SIM-26-48 | `test_fermentation_ui_commands::test_product_inserted_decision_uses_state_revision_without_apply`; `test_run_persistence_coordinator::test_stale_invalid_and_time_mismatched_transitions_write_nothing` (existing-owner) |
| SIM-26-49 | `test_fermentation_ui_commands::test_command_result_preserves_typed_app_details`; `test_fermentation_ui_commands::test_proposed_decision_is_not_reported_as_applied` |
| SIM-26-50 | `test_fermentation_ui_commands::test_product_inserted_decision_uses_state_revision_without_apply`; `test_run_persistence_coordinator::test_stale_invalid_and_time_mismatched_transitions_write_nothing` (existing-owner) |
| SIM-26-51 | `test_run_persistence_coordinator::test_unknown_outcome_is_resolved_by_exact_readback_and_duplicate_is_safe`; `test_actuation_interlock::test_fresh_start_commit_failure_never_allows` (existing-owner) |
| SIM-26-52 | `test_issue144_run_identity::test_application_prepares_every_envelope_action_with_one_identity` (existing-owner); `test_local_touch_ui::test_sim_26_manual_and_program_consumer_paths` |
| SIM-26-53 | `test_issue144_run_identity::test_catalog_revision_maps_to_neutral_run_provenance_without_truncation` (existing-owner); `test_run_commands::test_program_start_sensor_matrix_covers_all_eleven_rows` (existing-owner) |
| SIM-26-54 | `test_fermentation_ui_editing::test_user_program_id_allocation_is_deterministic_and_non_overwriting`; `test_fermentation_ui_editing::test_program_list_and_mutations_use_catalog_ownership` |
| SIM-26-55 | `test_fermentation_ui_editing::test_numeric_edit_model_keeps_actions_transient`; `test_fermentation_ui_editing::test_text_edit_model_has_mode_and_commit_without_validation` |
| SIM-26-56 | `test_local_touch_ui::test_sim_26_shell_locale_and_service_boundaries`; `test_local_touch_ui::test_pin_model_is_masked_and_owner_states_are_display_only`; `test_device_ui_contracts::test_touch_and_web_session_policies_remain_separate` |
| SIM-26-57 | `test_local_touch_ui::test_sim_26_manual_and_program_consumer_paths`; `test_fermentation_ui_commands::test_manual_timed_ui_intent_uses_the_merged_application_contract` (existing-owner) |
| SIM-26-58 | `test_configuration_service::test_program_editor_consumes_the_opening_catalog_revision` |
| SIM-26-59 | `test_fermentation_ui_models::test_projector_home_modes_follow_lifecycle_and_process_matrix`; `test_fermentation_ui_models::test_projector_marks_recovery_home_from_canonical_disposition` |
| SIM-26-60 | `test_fermentation_ui_commands::test_command_result_preserves_typed_app_details` |
| SIM-26-61 | `test_issue144_run_identity::test_application_composes_all_run_identities_at_one_boundary` (existing-owner); `test_fermentation_ui_commands::test_ui_payloads_are_intents_and_not_owning_evidence` |
| SIM-26-62 | `test_configuration_service::test_ui_configuration_confirmation_uses_current_owning_basis`; `test_fermentation_ui_commands::test_command_result_preserves_typed_app_details` |
| SIM-26-63 | `test_configuration_service::test_program_catalog_expected_revision_is_checked_under_preview_lock`; `test_configuration_service::test_persistent_failure_causes_remain_distinct` |
| SIM-26-64 | `test_configuration_service::test_confirmed_preview_commits_root_then_publishes_runtime`; `test_fermentation_ui_commands::test_command_result_preserves_typed_app_details` |
| SIM-26-65 | `test_run_persistence_coordinator::test_fallback_pending_never_allows_before_recovery_apply`; `test_actuation_interlock::test_fallback_selection_required_never_allows_even_with_complete_evidence` (existing-owner) |
| SIM-26-66 | `test_local_touch_ui::test_sim_26_manual_and_program_consumer_paths`; `test_fermentation_ui_commands::test_manual_timed_ui_intent_uses_the_merged_application_contract` (existing-owner) |
| SIM-26-67 | `test_fermentation_ui_commands::test_manual_timed_ui_intent_uses_the_merged_application_contract`; `test_run_commands::test_manual_timed_rejects_invalid_values_without_starting` (existing-owner) |
| SIM-26-68 | `test_issue144_run_identity::test_application_prepares_manual_timed_with_shared_identity` (existing-owner); `test_fermentation_ui_commands::test_manual_timed_ui_intent_uses_the_merged_application_contract` |
| SIM-26-69 | `test_fermentation_ui_commands::test_canonical_validation_precedes_ui_confirmation`; `test_fermentation_ui_commands::test_manual_timed_ui_intent_uses_the_merged_application_contract` |
| SIM-26-70 | `test_run_commands::test_program_start_sensor_matrix_covers_all_eleven_rows`; `test_control_context::test_invalid_run_sensor_mode_does_not_fallback_to_air` (existing-owner) |
| SIM-26-71 | `test_run_persistence_coordinator::test_orchestrator_fresh_start_uses_existing_command_commit_boundary`; `test_run_persistence_coordinator::test_fresh_start_bridge_write_error_and_unresolved_unknown_never_allow` (existing-owner) |
| SIM-26-72 | `test_run_checkpoint_codec::test_schema_five_round_trips_manual_timed_without_catalog_provenance` (existing-owner); `test_run_checkpoint_codec::test_manual_snapshot_and_runtime_shape_must_be_canonical` (existing-owner) |
| SIM-26-73 | `test_run_persistence_coordinator::test_manual_timed_restore_resume_uses_fail_closed_sensor_gate` (existing-owner); `test_run_persistence_coordinator::test_r1_time_pending_is_ram_only_and_rechecks_same_revision` (existing-owner) |

### Ebene 3: Build- und statische Integrationstests

Mindestens:

- `native`, `esp32_bringup` und `esp32_release` bauen reproduzierbar
- reale Zielkonfiguration verwendet 4 MB Flash
- keine PSRAM-Abhaengigkeit
- dokumentierter Partitionsplan ohne Release-1-Web-OTA
- Firmware- und Ressourcenbericht
- Factory-Konfiguration und Schemaversionen
- Deutsch, Spanisch und Englisch
- konfiguriertes Branding, Sprach-/Theme-Pakete und gezielt erzeugte Fontassets
  innerhalb des 4-MB- und ohne-PSRAM-Budgets
- Web- und lokale UI-Ressourcen
- keine eingebetteten Geheimnisse
- keine produktiv verwendeten `TBD`-Werte
- keine unbestaetigten Pins, Controller oder Designwerte als freigegebene
  Werte; nicht gemessene Pegel werden nicht als PASS behauptet
- Zukunftsfunktionen bleiben deaktiviert

Ein Build ist keine Hardwarefreigabe.

### Ebene 4: Hardwareabnahme ohne vorgeschriebenes Spannungsmess-Gate

Vor einer thermischen Belastung:

```text
ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED
SSOT_CONFORMANCE=<PASS/PENDING/NOT_APPLICABLE>
FUNCTIONAL_HARDWARE_VERIFICATION=<PASS/PENDING/NOT_APPLICABLE>
ADAPTER_SAFETY_VERIFICATION=<PASS/PENDING/NOT_APPLICABLE>
THERMAL_COMMISSIONING=<PASS/PENDING/NOT_APPLICABLE>
MULTIMETER_REQUIRED_FOR_R1_ACCEPTANCE=NO
BOOT_LEVEL_MEASUREMENT_REQUIRED=NO
GPIO_VOLTAGE_MEASUREMENT_REQUIRED=NO
```

Die Felder werden nur für den jeweiligen Scope geführt. Der #32-Scope führt
kein separates `ADAPTER_SAFETY_VERIFICATION`; der produktionsnahe
MOSFET-/Lüfter-/Summer-Adapterpfad gehört vollständig in
`FUNCTIONAL_HARDWARE_VERIFICATION`. Das separate Adapter-Safety-Feld ist dem
#33-H-Brückenvertrag vorbehalten. `THERMAL_COMMISSIONING` bleibt ein späteres
Commissioning-Gate und ist kein Ersatz für die funktionale Hardwareprüfung.

- GPIO-Zuordnung gegen die SSOT sowie funktionale Kanal-/Verbraucherwirkung
- funktionales Boot-, Reset-, Brownout- und Bootloaderverhalten ohne
  unkontrollierten relevanten Verbraucherbetrieb
- sichere H-Bruecken- und MOSFET-Zustaende
- BTS7960-Pulldowns/fail-low Beschaltung, boot default disabled, Mutual
  Exclusion, Break-before-make und fail-closed Fehlerpfad
- drei DS18B20 mit ROM-Zuordnung
- 1-Wire-Bustopologie und Produkt-Hot-Plug
- Displaycontroller, Touchcontroller, Rotation und Kalibrierung
- Raw-Touch-Kalibrierungsrecovery im 10-Sekunden-Fenster getrennt von
  PIN-unabhaengigem Vollreset; keine physische Bedienannahme
- Innen- und Aussenluefter
- Summer
- R_IS/L_IS sind für R1 nicht angeschlossen, nicht gemessen und nicht
  implementiert; sie sind kein Akzeptanzkriterium, kein Required Test und kein
  DoD-Gate. Eine spätere Verwendung erfordert ein eigenes Issue, einen
  vollständigen Plan und ein eigenes Owner-Gate (`FUTURE_RELEASE`).
- PIN-unabhaengiger lokaler Vollreset ohne Aktorwirkung
- UART-Flash- und Recoveryweg

### Ebene 5: Thermische Inbetriebnahme

Mit leerem Schrank und definierten Testmassen:

- Aufheizen, Abkuehlen und Halten
- Temperaturverteilung
- Produkt-Luft-Differenz
- Kuehlkoerper- und Luefterreaktion
- PI-Parameter je Sensorrolle und Richtung
- Zielband, Qualifikation und Gnadenzeit
- Luftbegrenzungen
- Mindestimpuls, Mindestzeiten und Totzeit
- Sicherheits-Eingriffs- und harte Notgrenzen
- fehlende thermische Reaktion
- thermisches Modell fuer Unterbrechungen, sofern verwendet
- Temperatursicherung: Rating, Montageort und thermische Wirksamkeit

### Ebene 6: Praktische Fermentationslaeufe

Erst nach bestandenen Software-, Hardware- und thermischen Gates:

- Joghurt mild
- Joghurt stichfest
- Milchkefir
- Wasserkefir

Bewertet werden Bedienung, Vorheizen, Zielqualifikation, produkt- und
luftgefuehrter Betrieb, Temperaturverlauf, Abschluss, Kuehlen und Halten. Ein
gelungenes Produkt ersetzt keine technische Sicherheitspruefung.

## Automatische Pruefungen je relevantem PR

1. native Unit-Tests
2. simulierte Prozess- und Fehlerablaeufe
3. Konfigurations- und Schemavalidierung
4. Persistenz-, Transaktions-, Rueckfall- und Migrationspruefungen
5. PlatformIO-Builds
6. Geheimnis- und lokale-Konfigurationspruefung
7. Pruefung auf produktive `TBD`- oder unbestaetigte Hardwarewerte
8. Groessenbericht fuer Firmware und statische Ressourcen

Ein fehlgeschlagener Sicherheits-, Persistenz-, Recovery- oder Kernfunktionstest
blockiert den Merge und das Release.

## Release-Gates

### Gate 0: Spezifikation und Rueckverfolgbarkeit

Vor Implementierungsfreigabe:

- verbindliche Anforderung oder Entscheidung vorhanden
- zugehoerige Testidee vorhanden
- Hardware-, Inbetriebnahme- und Budget-TBDs sichtbar
- Reviewkorrekturen von PR #38 in aktuelle kanonische Fachquellen integriert
- keine sicherheitskritische Annahme als bestaetigte Tatsache

### Gate 1: Softwarekern

Vor realem Aktorbetrieb:

- Zustandsmaschine nativ getestet
- Bootreihenfolge und `SAFE_BOOT` getestet
- `COMPLETED`-Wiederherstellung getestet
- Sensor- und Fehlerlogik getestet
- Aktorfreigabelogik getestet
- Mindestzeiten, Totzeit und Watchdog getestet
- Persistenz, Transaktionsmarker und Rueckfall getestet
- kritischer Schreibfehler sperrt Aktoren; #17-Status und RAM/FSM bleiben bis
  `Applied` unknown-safe, ohne neuen persistenten Safety-Latch
- der bestehende #23-Current-Boot-Watchdog-Latch wird nur ueber den
  vorhandenen expliziten Resetpfad mit frischer Evidenz geloescht
- Recoveryangebot, Resume und Fresh Start bleiben vor `Applied`/FSM/frischer
  Evidenz `Unresolved`; ein Fresh Start wird über die echte Application-Bridge
  bis zur `ActuationInterlock`-Permission nachgewiesen. Die automatische
  #124-Current-`FERMENTING`-Recovery bleibt bis zu derselben frischen
  Aktivierungsevidenz aktorfrei
- normale Config-Ablehnungen mit gueltigem Operational-Runtime erzeugen keinen
  Safety-Fault; echte Producer-/Integrity-/Indeterminate-Signale bleiben
  fail-closed
- `C2-Legacy/#18`: Ausfallintervall, alte Zeitunsicherheit und gewichtete
  Charge-Recovery sind kein #24-R1-Gate
- kein Aktortest aus `SAFE_BOOT` erreichbar
- alle sicherheitsrelevanten automatischen Tests bestanden

### Gate 2A: Hardware- und Adapterfreigabe ohne Peltier

Vor Anschluss beziehungsweise Bestromung des Peltiers:

- GPIOs gegen die SSOT zugeordnet und reale Kanal-/Verbraucherfunktion
  funktional bestaetigt
- Boot-, Reset- und Bootloaderverhalten funktional fail-closed bestaetigt
- BTS7960 ohne Peltier geprueft
- Pull-down-/fail-low Beschaltung als vorhandener Aufbau dokumentiert
- boot default disabled, Mutual Exclusion, Break-before-make und fail-closed
  Adapterpfad nachgewiesen; beide Richtungen koennen nie gleichzeitig aktiv sein
- Richtung und Polaritaet werden erst im begrenzten, abgesicherten
  Peltier-Servicepuls funktional bestimmt
- Schrankluft- und Kuehlkoerpersensor bestaetigt
- Aussenluefter und Nachlauf bestaetigt
- 7,5-A-Ueberstromsicherung installiert
- Kuehlkoerper und Waermetauscher montiert
- Servicebericht bis zu diesem Gate gespeichert

### Gate 2B: Erster realer Peltier-Puls

Vor **jedem ersten bestromten Peltier-Puls** muessen zusaetzlich erfuellt sein:

- einmalige Temperatursicherung installiert
- Temperatursicherung auf Durchgang geprueft
- Montageort dokumentiert
- Rating innerhalb der aktuellen Inbetriebnahmerevision freigegeben
- Aussenluefter unmittelbar zuvor erfolgreich getestet
- Pflichtsensoren aktuell `VALID`
- kein Fehler, keine Verriegelung und kein `SAFE_BOOT`
- validiertes `STANDBY` und PIN-geschuetzter Serviceablauf
- Leistung und Dauer firmwarefest begrenzt
- grosser jederzeit wirksamer Abbruch

Fehlt eine dieser Voraussetzungen, bleibt das Peltier spannungslos. Die
Temperatursicherung darf nicht erst nach ersten Pulsen nachgeruestet werden.

Nach dem Heizpuls folgen Peltier AUS, Nachlauf, Mindest-Ausschaltzeit und Totzeit,
bevor ein begrenzter Kuehlpuls erlaubt ist.

### Gate 3: Thermische Inbetriebnahme

Vor echten Fermentationslaeufen:

- leerer Schrank sowie kleine und grosse Testmasse vermessen
- Heizen, Kuehlen und Richtungswechsel abgestimmt
- Luftbegrenzungen festgelegt
- Sicherheits-Eingriffs- und harte Notgrenzen validiert
- Temperaturverteilung und kritischste Stellen bestimmt
- Temperatursicherung thermisch bewertet und dokumentiert
- keine unbekannte sicherheitsrelevante thermische Abweichung

### Gate 4: Dauer- und Belastungstest

Vor Release 1:

- mindestens sieben zusammenhaengende Tage
- keine unerklaerten Resets, Watchdogs oder Brownouts
- keine unerlaubte Aktorfreigabe
- keine relevante fortschreitende RAM-Leckage
- Speicherbereinigung innerhalb der Budgets
- kritische Persistenz und Sperren nach Unterbrechung wiederherstellbar
- Web, Display, Sensoren, Exporte und Regelung parallel stabil
- Fehler- und Resetjournal innerhalb des Budgets

### Gate 5: Releasekandidat

- Standardprogramme praktisch geprueft
- lokale und Webbedienung geprueft
- Stromunterbrechungs- und Recoveryablaeufe geprueft
- Exporte und Diagnose geprueft
- bekannte Abweichungen bewertet
- keine offene unbekannte Sicherheitsursache
- alle sicherheits- und kernfunktionsrelevanten Tests `PASS`

## Verpflichtende Fehlerinjektionen

### Sensoren

- Schrankluftfuehler in Standby, Vorheizen, Fermentation und Kuehlen ausfallen lassen
- Produktfuehler entfernen, Fallback und Rueckkehr pruefen
- Kuehlkoerpersensor im Peltierbetrieb ausfallen lassen
- CRC-Fehler, Busunterbrechung, `STALE` und unrealistische Spruenge
- widerspruechliche Produkt-, Luft- und Kuehlkoerperwerte

### Aktoren und Thermik

- veraltete Regelanforderung
- gleichzeitige Richtungsanforderung
- kurze und dauerhafte Gegenanforderung
- Mindest-Ausschaltzeit und Totzeit
- Aussen- und Innenluefterfehler
- fehlende thermische Peltierreaktion
- Sicherheits-Eingriffsgrenze und harte Notgrenze
- Abbruch eines Servicepulses
- E5/#35/Future: Peltier-Test ohne bestaetigte Temperatursicherung muss
  blockiert werden
- Aktortest aus `SAFE_BOOT` muss blockiert werden

### Versorgung, Zeit und Boot

- Unterbrechung in jeder wesentlichen Phase
- Brownout und wiederholte Brownouts
- Watchdog-Trip und erneuter Boot: RAM-Latch ist nicht persistent, Boot bleibt
  trotzdem all-off und revalidiert vollstaendig
- Neustart mit persistiertem `COMPLETED`
- keine automatische Charge-Recovery, kein gewichteter/NTP-basierter R1-
  Fortschritt
- WLAN-Ausfall bei weiterlaufendem sicheren Prozess

### Persistenz und Speicher

- neuesten Kontrollpunkt beschaedigen
- neueste Konfigurationsrevision beschaedigen
- Rueckfallrevision pruefen
- Unterbrechung waehrend kritischem Schreibvorgang
- kritischen Schreibfehler bei aktiver Aktoranforderung injizieren und sofortige
  Sperre vor einem weiteren Aktorbefehl nachweisen
- unvollstaendigen Transaktionsmarker hinterlassen
- kritischen Speicher nicht lesbar oder nicht schreibbar simulieren
- #17-Cutpoints vor `PreparedHead`, nach `PreparedHead`/Slot und nach
  `CommittedHead` injizieren; Teiltransaktionen bleiben unknown-safe
- `Success` ohne zweiten Readback sowie `CommitOutcomeUnknown` mit allen drei
  vorhandenen `writeExact()`-Aufloesungen pruefen
- Historienspeicher bis zur Bereinigung fuellen
- nichtkritischen RAM- oder Exportfehler erzeugen

### Bedienung und Berechtigungen

- Quittieren ohne Fehlerreset
- Resetversuch bei bestehender Ursache
- Servicefunktion ohne PIN
- Aktortest waehrend Lauf und `SAFE_BOOT`
- konfliktierende Display- und Webaktion
- alle Stopoptionen
- Service-PIN- und Vollreset-Tests gehoeren zu den spaeteren Service-/Hardware-
  Gates, nicht zum #24-R1-`ActuationInterlock`

## Hardware-Abnahme

Jeder relevante Hardwarestand dokumentiert mindestens:

1. Hardwarekennung und Platinenrevision
2. Verdrahtungsreferenz und Fotos
3. Versorgungsspannungen
4. GPIO-/SSOT-Zuordnung, funktionale Kanalwirkung und Bootverhalten
5. BTS7960-Pulldowns/fail-low, Enable, Richtungen, Adapterinterlock und
   Abschaltung
6. Innen- und Aussenluefter inklusive Nachlauf
7. Summer
8. drei DS18B20 mit Rolle und ROM-Adresse
9. Bustopologie und Produkt-Hot-Plug
10. Display, Touch und Kalibrierung
11. 7,5-A-Sicherung und Leitungsquerschnitte
12. Temperatursicherung vor dem ersten Puls: Typ, Rating, Montageort und
    Durchgangspruefung
13. Peltierstrom, Heiz- und Kuehlrichtung
14. begrenzter Heiztest
15. Mindest-Ausschaltzeit und Totzeit
16. begrenzter Kuehltest
17. thermische Reaktion
18. gespeicherter Servicebericht
19. Abweichungen und Freigabestatus

Eine Aenderung an Leistungspfad, Sensorbussen, Lueftern, Temperatursicherung,
Controllerboard oder Peltier kann eine neue Teil- oder Vollabnahme verlangen.

## Siebentaegiger Dauer- und Belastungstest

Belastungsprofil:

- kontinuierliche Sensorerfassung
- Displaybetrieb und wiederholte Bedienung
- parallele Webzugriffe und Live-Aktualisierung
- wiederholte Exporte
- periodische Kontrollpunkte und Bereinigung
- mehrere Heiz-, Kuehl- und Richtungswechsel
- WLAN- und NTP-Ausfall
- mindestens eine kontrollierte Stromunterbrechung
- Meldungen, Quittierungen und Diagnoseabrufe

Aufzuzeichnen:

- freier und niedrigster Heap
- groesster zusammenhaengender Block
- Task-, Watchdog- und Resetereignisse
- Sensor- und Busfehler
- Regler- und Aktorereignisse
- Flash- und Historienbelegung
- Bereinigungen und Schreibfehler
- Web- und Exportfehler
- Temperaturstabilitaet und Richtungswechsel

Der Test besteht nur ohne unerlaubte Aktorfreigabe, unerklaerten Reset,
unbehandelten Watchdog, relevante RAM-Leckage, verlorene kritische Persistenz
oder unbekannte Sicherheitsabweichung.

## Testnachweis

Jeder formelle Test enthaelt:

```text
Test-ID
Titel
Anforderung / Entscheidung / Fehlercode
Testebene
Voraussetzungen
Hardwareversion
Firmwareversion und Commit
Konfigurations- und Tuningrevision
Testdaten und Referenzgeraete
Testschritte
erwartetes Ergebnis
gemessenes Ergebnis
Logs, Exporte, Bilder oder Servicebericht
PASS / FAILED / BLOCKED / NOT_RUN
Abweichungen
verantwortliche Person
Datum und Zeitbasis
```

Test-ID-Gruppen:

```text
UT-xxx    Native Unit-Tests
SIM-xxx   Simulation und Zustandsmaschine
BLD-xxx   Build und statische Integration
HW-xxx    Hardware und elektrische Abnahme
TH-xxx    Thermische Inbetriebnahme
FI-xxx    Fehlerinjektion
END-xxx   Dauer- und Belastungstest
FER-xxx   Praktische Fermentationslaeufe
REL-xxx   Release-Gates
```

`PASS_WITH_WARNINGS` kann in Serviceberichten vorkommen, ersetzt bei einem
formellen Gate aber kein `PASS`, wenn die Warnung eine Gate-Anforderung betrifft.

## Akzeptierte Entscheidungen

- [x] sechs Testebenen
- [x] automatische native, simulierte, Persistenz- und ESP32-Buildtests
- [x] sicherheits- und kernfunktionsrelevante Tests muessen bestanden sein
- [x] verpflichtende Fehlerinjektionen
- [x] dokumentierte Abnahme jedes relevanten Hardwarestands
- [x] Temperatursicherung vor dem ersten realen Peltier-Puls
- [x] `SAFE_BOOT` bleibt aktorfrei
- [x] Boot bewertet aktuelle Producer-/Persistenzintegritaet vor dem
      Resume-Angebot; es gibt keine allgemeine persistente Verriegelung
- [x] `C2-Legacy/#18`: Ausfallzeit wird als Intervall dokumentiert, ist aber
      kein #24-R1-Safety-Gate
- [x] `COMPLETED` wird nach Neustart wiederhergestellt
- [x] kritischer Persistenzfehler sperrt Aktoren und haelt den bestehenden
      #17-Coordinator unknown-safe; kein neuer RAM-/Persistenz-Latch
- [x] der #23-Current-Boot-Watchdog-Latch bleibt bis zum bestehenden
      expliziten Resetpfad aktiv und wird mit frischer Evidenz geloescht
- [x] `E4/E5/Future`: physischer Vollreset, Service-Gate und PIN-Regeln sind
      kein #24-R1-Safety-Clear
- [x] mindestens siebentaegiger Dauer- und Belastungstest
- [x] formeller versionierter Testnachweis
