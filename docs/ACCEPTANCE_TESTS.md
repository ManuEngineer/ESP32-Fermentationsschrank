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

Berichtigung (Issue #172, S11, O8 (b)): Die Testzuordnung von SIM-26-04 bis
SIM-26-07 war vorbestehend falsch (Locale-/Clock-/Editor-Tests statt der
definierten Nachweise) und ist nur in der Tabelle unten auf die passenden
bestehenden Tests umgehaengt; die Definitionen oben sind unveraendert.

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
| SIM-26-04 | `test_fermentation_ui_commands::test_product_inserted_decision_uses_state_revision_without_apply`; `test_press_dispatcher::test_product_inserted_waiting_for_product_reaches_target_and_persists` |
| SIM-26-05 | `test_fermentation_ui_editing::test_numeric_edit_model_keeps_actions_transient`; `test_fermentation_ui_editing::test_text_edit_model_has_mode_and_commit_without_validation`; `test_configuration_service::test_program_editor_consumes_the_opening_catalog_revision` |
| SIM-26-06 | `test_local_touch_ui::test_pin_model_is_masked_and_owner_states_are_display_only` |
| SIM-26-07 | `test_boot_classification::test_all_load_outcomes_map_to_the_r1_boot_classification` (existing-owner); `test_renderer_boundary::test_deferred_pages_show_the_hint_in_all_locales_and_keep_owner_reason`; `TRACE: docs/LOCAL_UI_SETTINGS_SERVICE.md#safe_boot-oberflaeche` (Owner #57/#31/#89/#28, keine vorgezogene Implementation) |
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
| SIM-26-43 | `test_local_touch_ui::test_sim_26_program_editor_actions_are_real_requests`; `test_local_touch_ui::test_sim_26_program_delete_owner_usage_gate`; `test_local_touch_ui::test_sim_26_standard_delete_uses_two_confirmations`; `test_configuration_service::test_program_delete_in_use_is_rejected_before_preview`; `test_fermentation_ui_editing::test_program_list_and_mutations_use_catalog_ownership` |
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

#### Issue #172 – PR B Application-Owner (S5, S6)

Neue native Nachweise fuer die owning Application-Pfade der lokalen Touch-UI.
Die UI liefert nur Intent und erwartete Revision; Entscheidung und Persistenz
bleiben bei den bestehenden Runtime-/Persistence-Ownern. Hardware fuer S5 ist
`NOT_APPLICABLE`, solange der Produktbuild `WaitingForProduct` ohne #30/#35
nicht real erzeugt.

| ID | Nachweis |
|---|---|
| SIM-172-S5-01 | Ohne Runtime-/Persistence-Kontext liefert `confirmProductInserted()` fail-closed `ContextMissing`: `test_press_dispatcher::test_product_inserted_without_runtime_context_is_context_missing`; `test_press_dispatcher::test_dispatch_transition_action_without_context_is_decision_only`. |
| SIM-172-S5-02 | `WaitingForProduct` wird ueber `decideProcessTransition(ProductInsertedConfirmed)` und `persistTransition()` zu `ReachingTarget` und persistiert (Head aendert sich): `test_press_dispatcher::test_product_inserted_waiting_for_product_reaches_target_and_persists`; Dispatcher-Pfad `test_press_dispatcher::test_dispatch_transition_action_reaches_the_owning_application`. |
| SIM-172-S5-03 | Falscher Zustand und veraltete `transitionSequence` werden ohne Zustands- und Persistenzaenderung abgelehnt: `test_press_dispatcher::test_product_inserted_in_wrong_state_is_rejected_without_change`; `test_press_dispatcher::test_product_inserted_stale_sequence_is_rejected_without_change`. |
| SIM-172-S5-04 | Persistenzfehler ist fail-closed (Zustand unveraendert, erneuter Versuch moeglich) und ein wiederholter Press wird abgewiesen: `test_press_dispatcher::test_product_inserted_persistence_failure_is_fail_closed_and_retryable`; `test_press_dispatcher::test_product_inserted_repeated_press_is_rejected`. |
| SIM-172-S6-01 | F14: ein von `applyProgramEditPreview()` erzeugtes Preview ist bestaetigbar und nach Reload aus dem Store sichtbar (kanonische Wire-Werte `LocalDisplay=2`, `NormalEdit=1`, `StandardProgramReset=6`): `test_configuration_service::test_program_edit_preview_confirms_and_reloads_from_the_store`. |
| SIM-172-S6-02 | Copy, New, Delete, Uninstall (mit Bestaetigung) und Reset laufen ueber `FermentationApplication::applyProgramEdit()` bis zur Aktivierung; Werkprogramme sind nicht loeschbar: `test_press_dispatcher::test_program_copy_and_new_create_user_programs`; `test_press_dispatcher::test_program_delete_removes_user_program_but_never_a_factory_program`; `test_press_dispatcher::test_program_uninstall_needs_confirmation_and_reset_restores_factory`. |
| SIM-172-S6-03 | `Edit`, `Reset`, `Uninstall` und `Delete` eines aktiven Programms werden vor dem Preview blockiert (`NotAllowed`, Usage-Evidence aus dem Run-State); `Copy` und `New` bleiben moeglich, der laufende Run-Snapshot bleibt unveraendert, es bleibt kein Preview-Slot belegt: `test_press_dispatcher::test_program_edit_of_the_active_program_is_blocked_before_preview`; `test_press_dispatcher::test_program_edit_and_reset_of_the_active_program_are_blocked`. |
| SIM-172-S6-04 | Veraltete oder fehlende `ProgramCatalogRevision` und fehlende `UserConfigurationRevision` ergeben `StateChanged` ohne Aenderung; eine veraltete `UserConfigurationRevision` des gesehenen Snapshots wird bei der Bestaetigung abgelehnt (die aktuelle Runtime-Revision wird nicht eingesetzt): `test_press_dispatcher::test_program_edit_from_a_stale_user_configuration_snapshot_is_rejected`; `test_press_dispatcher::test_program_edit_without_expected_revisions_is_fail_closed`; Capacity wird als `InvalidCandidate` gemeldet und ist nach Delete wiederherstellbar; ohne Run-State/Service fail-closed: `test_press_dispatcher::test_program_edit_with_stale_catalog_revision_is_rejected`; `test_press_dispatcher::test_program_capacity_is_reported_and_recoverable`; `test_press_dispatcher::test_program_edit_without_run_state_or_service_is_fail_closed`. |
| SIM-172-S6-05 | Persistenz ueber Neustart und Dispatcher-Pfad (`programEdit` erreicht den Owner, ohne Katalogrevision im Snapshot keine Aenderung): `test_press_dispatcher::test_program_edit_persists_across_a_restart`; `test_press_dispatcher::test_dispatch_program_edit_reaches_the_owning_application`. |
| SIM-172-D5-01 | Die neuen Application-Entries `confirmProductInserted()` und `applyProgramEdit()` warten auf den gemeinsamen `ApplicationCallSerializer` und setzen nach Freigabe fort: `test_press_dispatcher::test_confirm_product_inserted_waits_for_the_application_gate`; `test_press_dispatcher::test_apply_program_edit_waits_for_the_application_gate`. |

#### Issue #172 – PR A, C und D lokale Touch-UI (S1–S4, S7–S10)

Native Nachweise der funktional vervollstaendigten lokalen Touch-UI. Die UI
liefert nur Intent und erwartete Revision; Validierung, Entscheidung und
Persistenz bleiben bei den bestehenden Ownern. Alle Zeilen sind native
Software-Nachweise; Hardware (Display/Touch, Sprachwechsel mit Neustart,
Programmverwaltung, Ressourcenlogs, 30-/34-px-Tastengroessen und Labelbreiten)
ist fuer PR C/D `NOT_RUN` bis zur konsolidierten Abnahme.
`ACTUATOR_RELEASE=NO`.

| ID | Nachweis |
|---|---|
| SIM-172-S1-01 | Programm-/Meldungslisten waehlen die Zeile des sichtbaren Fensters; nicht startbare Programme sind mit Grund waehlbar ohne Start; Zeilen sind 40-px-Touchzeilen mit exakten Hitzonen und stationaer ohne Allokation: `test_local_touch_ui::test_program_list_cell_selects_the_row_of_the_visible_window`; `test_local_touch_ui::test_unstartable_program_is_selectable_with_reason_and_no_start`; `test_local_touch_ui::test_message_list_cell_selects_the_canonical_message_id`; `test_local_touch_ui::test_waiting_home_opens_the_canonical_decision_message_not_an_earlier_one`; `test_renderer_boundary::test_program_list_rows_are_40px_touch_rows_with_exact_hit_zones`; `test_ui_steady_state_allocations::test_program_list_page_steady_state_allocates_nothing_and_rows_redraw`. |
| SIM-172-S3-01 | Sprachwahl laeuft ueber den Owner (`applyDisplayLanguage`): Commit aendert die Revision, ueberlebt einen Neustart, ein abgelehnter Wechsel ist sichtbar und wird durch einen Erfolg geloescht: `test_display_language_application::test_commit_activates_the_language_and_changes_the_revision`; `test_display_language_application::test_language_persists_across_a_restart`; `test_press_dispatcher::test_language_row_press_reaches_the_owning_configuration_commit`; `test_press_dispatcher::test_refused_language_change_is_visible_and_cleared_by_a_success`; `test_renderer_boundary::test_language_page_rows_show_endonyms_mark_the_active_language_and_hit`. |
| SIM-172-S4-01 | Lokalzeit im Header und auf der Zeitseite kommt aus dem Lokalzeit-Owner (#178); ohne vertrauenswuerdiges UTC oder Zonenregel zeigt die Anzeige keine Zeit; Hitzonen ueberlappen nicht; stationaer ohne Allokation: `test_local_time::test_zurich_reference_vectors_including_dst_boundaries`; `test_renderer_boundary::test_clock_text_dash_without_trusted_utc_even_with_a_zone_rule`; `test_renderer_boundary::test_header_clock_hit_zone_edges_do_not_overlap_network_or_language`; `test_renderer_boundary::test_header_clock_page_shows_trust_zone_and_local_time`; `test_ui_steady_state_allocations::test_clock_page_steady_state_and_local_time_path_allocate_nothing`. |
| SIM-172-S7-01 | Read-only Inhaltsseiten sind begrenzt, deterministisch, ueberlappungsfrei und stationaer ohne Allokation; Diagnose/Service/PIN zeigen nur den Hinweis `zurueckgestellt (#28)`; eine Recovery-Zeitkorrektur wird nie angeboten: `test_renderer_boundary::test_content_pages_are_bounded_deterministic_and_do_not_overlap`; `test_renderer_boundary::test_deferred_pages_show_the_hint_in_all_locales_and_keep_owner_reason`; `test_local_touch_ui::test_recovery_time_correction_is_never_offered_even_with_a_staged_value`; `test_ui_steady_state_allocations::test_s7_content_pages_steady_state_allocate_nothing`. |
| SIM-172-S8-01 | Startwerte werden ueber den Ziffernblock nur als fluechtiger Kandidat des naechsten Laufs gesetzt; Sensorwahl folgt der strukturellen Startmatrix und der gespeicherten `SensorPreference`; Zuruecksetzen nur bei geaendertem Kandidaten; Ziffernblock-Hitzonen exakt: `test_local_touch_ui::test_start_value_keypad_edit_sets_only_the_next_run_candidate`; `test_local_touch_ui::test_sensor_cycle_follows_the_structural_start_matrix`; `test_local_touch_ui::test_reset_start_values_is_offered_only_for_a_changed_candidate`; `test_issue144_run_identity::test_program_start_requested_sensor_mode_follows_stored_preference`; `test_fermentation_ui_editing::test_keypad_key_sequences_commit_the_edited_value`; `test_renderer_boundary::test_value_edit_page_shows_the_candidate_and_a_keypad_with_exact_zones`. |
| SIM-172-S9-01 | Manueller Start und Kuehlplaene sind auf jeder Oberflaeche `Unavailable`, solange kein Owner technischer Laufgrenzen existiert (O5, #34/#35); die Anwendung verbraucht dabei keine Identitaet: `test_issue144_run_identity::test_manual_run_and_cooling_requests_are_unavailable_on_every_surface`; `test_press_dispatcher::test_manual_start_has_no_path_without_an_owner_of_the_run_limits`; `test_web_application_routes::test_handler_refuses_manual_runs_and_cooling_plans_without_a_limits_owner`. |
| SIM-172-S10-01 | Einstellungen in fester Reihenfolge inkl. Service als lokalisiertes Textlabel `Service (PIN)`; ein deaktivierter Eintrag nennt seinen Grund; Standby-Slot 3 oeffnet Einstellungen: `test_local_touch_ui::test_settings_rows_are_in_the_decided_order_and_open_their_pages`; `test_local_touch_ui::test_standby_slot_three_opens_settings_and_service_lives_below_it`; `test_renderer_boundary::test_settings_page_draws_the_rows_in_the_decided_order`; `test_renderer_boundary::test_settings_disabled_rows_show_their_reason`. |
| SIM-172-S10-02 | Geraetename ueber `applyUserSettings`: Owner-Regeln und Byte-Limit, Persistenz ueber Neustart, aktiver Lauf blockiert die Aenderung in der Application: `test_press_dispatcher::test_device_name_commit_reaches_the_owner_and_an_active_run_refuses_it`; `test_user_settings_application::test_an_active_run_blocks_the_change_in_the_application`; `test_user_settings_application::test_the_name_persists_across_a_restart`; `test_local_touch_ui::test_keyboard_follows_the_owning_text_rules_and_byte_limit`. |
| SIM-172-S10-03 | Programmeditor: Kandidat ist fluechtig und nutzt dieselben Programmregeln wie der Validator; Speichern macht den Editor erst nach akzeptiertem Owner-Ergebnis sauber, ein abgelehnter Save behaelt Kandidat und `ConfirmDiscard`; Verwerfen ist ein expliziter Slot; die Tastatur zeigt die Route ihres Aufrufers: `test_local_touch_ui::test_program_editor_numeric_fields_use_the_program_validator`; `test_program_models::test_unexpected_value_rules_are_shared_by_validator_and_editor`; `test_local_touch_ui::test_program_save_marks_the_editor_clean_only_after_the_owner_accepts`; `test_press_dispatcher::test_program_save_outcome_decides_whether_the_editor_is_clean`; `test_local_touch_ui::test_program_editor_discard_is_an_explicit_slot_and_drops_the_candidate`; `test_local_touch_ui::test_text_edit_route_follows_the_program_editor_caller`. |
| SIM-172-S10-04 | Tastatur- und Editorseiten sind begrenzt und ueberlappungsfrei, Tastenhoehe 34 px, stationaer ohne Allokation: `test_renderer_boundary::test_s10_pages_are_bounded_deterministic_and_do_not_overlap`; `test_renderer_boundary::test_keyboard_page_draws_the_mode_keys_and_has_exact_34px_hit_rows`; `test_ui_steady_state_allocations::test_settings_page_adopts_the_device_name_and_is_a_steady_state`; `test_ui_steady_state_allocations::test_keyboard_held_key_redraws_once_per_target`. |

#### Issue #30 – DS18B20-Softwarepfad (C1–C3, nativ und Build)

Native Simulationen mit Fake-Bus und virtueller Zeit sowie ESP-IDF-Buildnachweise
fuer den hardwareunabhaengigen Softwareumfang von Issue #30. Sie sind **kein**
Hardwarenachweis: Die `HW-30-*`-Eintraege unten gehoeren zum Hardware-Folgeissue
und stehen auf `NOT_RUN`. Die Plattform kennt nur technische Kanaele; Rollen
liegen in `fermentation_app` (ADR-013).

| ID | Nachweis |
|---|---|
| SIM-30-01 | Ungebunden/ungueltig ist fail-closed: leere, unvollstaendige, Null-/Doppel-ROM-Bindung liefert nie `Ok` und keine Identitaet; ohne Datensatz bleibt die Engine ungebunden, mit Datensatz folgt die Rollenabbildung ueber die Kanaele: `test_ds18b20_sampling::test_unbound_expected_rom_channels_never_ok_and_have_no_identity`; `test_ds18b20_sampling::test_invalid_bindings_stay_unbound`; `test_sensor_commissioning::test_engine_is_fail_closed_without_record_and_bound_through_the_mapping`. |
| SIM-30-02 | Bindung am Mehrteilnehmerbus: erstes `Ok` nur nach bestaetigter Bindung, 12 Bit genau einmal, Zuordnung nach ROM statt Enumerationsreihenfolge, fehlendes ROM betrifft nur seinen Kanal, unbekanntes Zusatz-ROM bzw. mehr als vier Teilnehmer = `BindingConflict`: `test_ds18b20_sampling::test_first_ok_after_verified_binding_fresh_conversion_and_resolution`; `test_ds18b20_sampling::test_channel_mapping_follows_rom_not_enumeration_order`; `test_ds18b20_sampling::test_missing_expected_rom_affects_only_its_channel`; `test_ds18b20_sampling::test_unknown_extra_rom_is_binding_conflict_for_both_channels`; `test_ds18b20_sampling::test_more_than_four_devices_is_a_binding_conflict`. |
| SIM-30-03 | Typisierte Fehler: Bus-/Presence-/CRC-/85-°C-/Konvertierungsfehler werden abgebildet, Einzelsensorfehler wirken nur auf den eigenen Kanal, ein Busfehler trifft nur seinen Bus: `test_ds18b20_sampling::test_bus_wide_failures_hit_both_expected_rom_channels`; `test_ds18b20_sampling::test_single_sensor_faults_do_not_affect_the_other_channel`; `test_ds18b20_sampling::test_power_on_value_is_never_a_measurement`; `test_ds18b20_sampling::test_start_conversion_and_set_resolution_failures_are_typed`; `test_ds18b20_sampling::test_bus_faults_are_independent_between_the_two_buses`. |
| SIM-30-04 | Einzelgeraet-Bus (Produktfuehler): 0/1/2/>4 Geraete, Entfernen und Wiederkehr mit erneutem 12-Bit-Setzen, ROM-Wechsel meldet die neue Identitaet: `test_ds18b20_sampling::test_single_device_bus_zero_one_two_and_overflow`; `test_ds18b20_sampling::test_single_device_absence_keeps_last_seen_rom_and_recovers`; `test_ds18b20_sampling::test_single_device_rom_change_reports_the_new_identity`. |
| SIM-30-05 | Takt und Frische: 2-s-Zyklus, gemeinsame Wartezeit, kein Lesen ohne frische Konvertierung, monotone Zeitstempel, kein Zyklus-Burst nach spaetem Scheduling, nie gesteppt/Task-Stillstand = fail-closed: `test_ds18b20_sampling::test_cycle_timing_freshness_and_monotonic_timestamps`; `test_ds18b20_sampling::test_late_scheduling_does_not_burst_cycles`; `test_ds18b20_sampling::test_engine_never_stepped_is_fail_closed_and_invalid_channel_is_missing`; `test_ds18b20_sampling::test_channel_source_returns_the_engine_reading`; `test_ds18b20_sampling::test_task_stall_leaves_a_non_valid_pipeline_state`. |
| SIM-30-06 | Identitaet bei Nicht-Ok-Proben gegen die echte `SensorQualityPipeline` (Plan 4b), verglichen mit einer von Hand gebauten Referenzfolge: `Ok(A)->Luecke->Ok(B)` setzt den Filter zurueck und uebernimmt weder A-Filter noch A-Offset, `Ok(A)->Luecke->Ok(A)` bleibt kontinuierlich (Luecke = abwesend/Busfehler/CRC/Mehrfachgeraet): `test_ds18b20_sampling::test_ok_a_gap_ok_b_resets_filter_for_every_gap_kind`; `test_ds18b20_sampling::test_ok_a_gap_ok_a_keeps_filter_history_and_offset`; `test_ds18b20_sampling::test_boot_missing_without_history_then_first_sensor_is_normal_start`; `test_ds18b20_sampling::test_non_ok_readings_never_lose_the_known_identity`. |
| SIM-30-07 | Datensatzmodell, Kalibrierung je ROM, Kanalabbildung und Kommando-Parser inkl. 1-Wire-ROM-CRC-Pruefung (manipulierte CRC wird in Datensatz, Codec und Kanalbindung fail-closed abgelehnt): `test_sensor_commissioning::test_model_validation_covers_every_invalid_shape`; `test_sensor_commissioning::test_rom_crc_check_matches_the_dallas_reference`; `test_sensor_commissioning::test_calibration_lookup_is_per_rom_and_unknown_rom_has_none`; `test_sensor_commissioning::test_record_maps_roles_to_technical_channels_and_invalid_stays_unbound`; `test_sensor_commissioning::test_parser_accepts_exact_commands_only`; `test_sensor_commissioning::test_command_application_builds_valid_records_only`. |
| SIM-30-08 | Persistenz Schema 3: Roundtrip/Strenge/Kanonikform, Schema-1/2-Migration und Referenzpruefung, Kapazitaet 179 B, Neustart-Persistenz: `test_configuration_codecs::test_service_configuration_schema_three_sensor_section_round_trips`; `test_configuration_codecs::test_service_configuration_schema_three_sensor_section_is_strict`; `test_configuration_codecs::test_service_configuration_schema_two_decodes_and_keeps_its_canonical_form`; `test_configuration_graph_codecs::test_manifest_rejects_inconsistent_wire_metadata_and_reference_contracts`; `test_configuration_graph_store::test_v2_service_record_survives_validation_and_changes_until_v3_write`; `test_sensor_commissioning::test_record_persists_across_a_restart`. |
| SIM-30-09 | Application-Owner `applySensorCommissioning`: gueltiger Commit, NoChange, Loeschen, ungueltige Datensaetze ohne Preview, fehlende/veraltete Revision, aktiver Lauf, nicht gestartete Application, Anwendungsgate: `test_sensor_commissioning::test_application_starts_unbound_and_commits_a_valid_record`; `test_sensor_commissioning::test_application_rejects_invalid_records_without_a_preview`; `test_sensor_commissioning::test_application_missing_and_stale_revisions_are_rejected`; `test_sensor_commissioning::test_application_refuses_changes_while_a_run_is_active`; `test_sensor_commissioning::test_not_started_application_has_no_record_and_refuses`; `test_sensor_commissioning::test_apply_sensor_commissioning_waits_for_the_application_gate`. |
| SIM-30-10 | ADR-013-Rollenneutralitaet der DS18B20-Plattform-/Adapter-/Testsupport-Dateien und Komponentenvertrag: `TRACE: python3 scripts/check_architecture_boundaries.py` und `--selftest`; Builds `python3 scripts/build_esp_idf_profiles.py bringup` und `release` sowie `python3 scripts/build_issue30_sensor_commissioning.py` (Harness nur im Bring-up, im Release-Image keine Harness-Symbole). |
| HW-30-01 | `NOT_RUN` (Hardware-Folgeissue H1): ein realer Sensor, ROM, 9–12 Bit, CRC, Entfernen/Anstecken, Neustart. |
| HW-30-02 | `NOT_RUN` (H2): Zielverdrahtung, Zyklusdauer, 1000 Zyklen, 10 Neustarts, Reset waehrend Konvertierung. |
| HW-30-03 | `NOT_RUN` (H3/H4): Task-Budget, Heap/Stack/RMT-Messung, Versions-/Variantenbestaetigung. |
| HW-30-04 | `NOT_RUN` (H5): ROM-Erfassung und -Zuordnung ueber das Commissioning, ROM-Liste. |
| HW-30-05 | `NOT_RUN` (H6): Verifikationsmatrix aus #30 (Abziehen, Stoerungen, Hot-Plug, Offsets, reale Messwerte). |
| HW-30-06 | `NOT_RUN` (H7): Hardware-Evidence auf dem exakten Head, `logResources`-Vergleich. |

### Issue #19 – lokaler Werksreset (R1-Pflicht, hardwarefrei)

Native Simulationen fuer den lokalen Werksreset-Ablauf (Plan
`docs/tasks/issue-19-journals-retention-backup-import-plan.md`, Abschnitt 4,
Ownerfreigabe `6fbf130`). Sie sind **kein** Hardwarenachweis; die physischen
Eintraege stehen auf `NOT_RUN`. Journal, Laufhistorie, Bereinigung, Laufexport
sowie Backup/Import sind **nicht** Teil dieser Nachweise (`DEFERRED` bzw.
bedingt, Plan Abschnitte 5–7).

| ID | Nachweis |
|---|---|
| SIM-19-R01 | Ablauf-Zustandsautomat: ohne Ownerparameter nicht verfuegbar, Stufen nur der Reihe nach, Abbruch in jeder Stufe wirkungslos, Dauer erst bei durchgehendem Kontakt erreicht, Loslassen/rueckwaerts laufende Zeit setzt zurueck, Variante A wartet auf verifizierte PIN, B nie: `test_factory_reset_flow::test_flow_without_configured_hold_is_unavailable_and_has_no_default`; `::test_flow_requires_every_stage_in_order_and_cancel_resets`; `::test_flow_hold_needs_continuous_contact_for_the_full_duration`; `::test_flow_variant_a_waits_for_a_verified_pin_and_b_never_does`. |
| SIM-19-R02 | Anwendungseinstieg: Vorbedingungen unter dem `ApplicationCallSerializer`; ein laufender Prozess blockiert Beginn und Ausfuehrung (Resetkern wird nicht aufgerufen); ohne geladene Runtime nicht angeboten (Stoppbefund S1); nur Variante B wird angeboten (A bis O-R2 nicht): `test_factory_reset_flow::test_a_running_process_blocks_the_flow_and_the_core_is_not_called`; `::test_a_configuration_without_runtime_is_reported_unavailable`; `::test_application_offers_only_the_pin_independent_variant`; `::test_application_flow_is_unavailable_until_the_owner_parameter_is_set`. |
| SIM-19-R03 | Abbruch aendert nichts: kein Resetkern-Aufruf, keine Epochenaenderung, kein Netzwerk-/HTTP-Stopp: `test_factory_reset_flow::test_cancel_at_every_stage_changes_no_reset_state`. Auth-Writes der PIN-Pruefung sind hier nicht betroffen (Variante A nicht angeboten); vorhandene PIN-/Lockout-Regressionen unveraendert: `test_authentication_records::test_lockout_is_persisted_and_skips_kdf_while_active_and_after_reboot`, `::test_credential_change_reports_denial_and_lockout_separately`. |
| SIM-19-R04 | Vollstaendiger Ablauf (Ablauf B): Resetkern, Epoche +1, danach Netzwerk-Stopp, dann HTTP-Stopp, alter Netzwerkmodus verworfen, kein Neustart mit alten Daten, Touchkalibrierung unveraendert: `test_factory_reset_flow::test_full_flow_runs_the_core_then_ends_network_then_http`; vorhandene Regressionen: `test_configuration_recovery_service::test_factory_reset_advances_epoch_and_preserves_touch_key`, `::test_factory_reset_preserves_real_touch_calibration_record`, `test_web_application_routes::test_composed_dispatcher_factory_reset_revokes_old_sessions`, `test_issue144_run_identity::test_application_reset_hands_off_existing_run_store_to_new_epoch`, `::test_application_reconstructs_reset_handoff_after_run_write_cut`. |
| SIM-19-R05 | Netzwerk/HTTP strikt KISS: HTTP-Stopp laeuft nicht unter dem Application-Guard (ein auf den Guard wartender Handler blockiert nicht; Mutationsprobe schlaegt fehl): `test_factory_reset_flow::test_http_is_stopped_outside_the_application_gate`. Nicht bestaetigter Netzwerk- oder HTTP-Stopp wird nie als Erfolg gemeldet, keine Neuprovisionierung: `::test_unconfirmed_network_stop_is_never_reported_as_success`; `::test_unconfirmed_http_stop_is_never_reported_as_success`; Ergebnisabbildung: `::test_outcome_never_reports_success_for_an_unconfirmed_network_stop`. |
| SIM-19-R06 | Nur lokal: der Resetschritt ist keine Alternative des gemeinsamen UI-Command-Variants (`static_assert` in `test_factory_reset_flow`); keine HTTP-Route erreicht Ablauf oder Kern: `test_web_application_routes::test_no_http_route_reaches_the_local_factory_reset`. |
| SIM-19-R07 | Zwei Zugaenge, ein Ablauf (O-R1 = B+) ueber den echten Touch-Adapter: "PIN vergessen?" auf der PIN-Seite (bei gesperrtem Servicebereich erreichbar, ohne PIN-Eingabe), `SAFE_BOOT`-Eintrag, Hold-Ziel nur bei durchgehendem Kontakt, Zurueck/Abbruch beendet den Ablauf, ohne Ownerparameter nirgends angeboten: `test_press_dispatcher::test_forgot_pin_entry_runs_the_whole_flow_through_touch`; `::test_safe_boot_entry_and_every_exit_end_the_flow_without_a_reset`. |
| SIM-19-R08 | Review-Fix B1: der Release-/No-Contact-Pfad erreicht den Ablauf in der **echten Schleifenentscheidung** (`touchLoopAction`, von der Firmware-Schleife und vom Test gemeinsam genutzt): Teil-Hold -> Loslassen -> lange Pause -> kurzer erneuter Kontakt loest **nicht** aus (keine Epochenerhoehung, kein Netzwerk-/HTTP-Stopp); Verschieben vom Halteziel und Abbrechen setzen zurueck; erst 5000 ms neuer ununterbrochener Kontakt loesen aus: `test_press_dispatcher::test_interrupted_hold_never_survives_a_pause_in_the_real_loop_decision` (Mutationsprobe: ohne Release-Zweig schlaegt der Test fehl). |
| HW-19-R01 | `NOT_RUN` (Hardware-Folgeissue #192): physische Erreichbarkeit beider Zugaenge am Geraet, Langgedrueckthalten (5000 ms, Ownerentscheid), Anzeige der Seite, Aktoren AUS am realen Geraet. |
| HW-19-R02 | `NOT_RUN`: tatsaechliches Verhalten von `esp_wifi_stop()` und `httpd_stop()` (ehrliche Rueckgabe, Abschluss von AP und HTTP), Powercut mitten im Reset auf echtem Flash. |
| HW-19-R03 | `NOT_RUN`: Werksreset aus `SAFE_BOOT` am Geraet (nur Fall mit geladener Konfigurations-Runtime; Fall ohne Runtime = Stoppbefund S1, nicht umgesetzt). |

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
