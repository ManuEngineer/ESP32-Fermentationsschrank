# R1 – Temperaturanzeige, Firmware-Temperaturschutz, Regelbereitschaft, Trial-Profil: digitale Evidence (S1–S6)

Plan: `docs/tasks/r1-sensor-display-and-control-trial-plan.md` Rev. 2, freigegeben auf Plan-SHA
`ae68a14ef611cf69b3765a2438b90ab0840107c5` (D1, Owner), PR #221. Stand dieser Evidence:
Code-HEAD `3fd1fd36b208f5d0c5cf3e8e4b671b99bd22cafc`.

**Reine Software-/Buildevidence.** `FLASH=NO`, keine Hardwareaktion, `ACTUATOR_RELEASE=NO`,
`REAL_PELTIER_TEST=NOT_RUN`. G-P1, G-P2, G-T, G-C0, G-C1, G-C2 sind `NOT_GRANTED`; kein
Trial-Image wurde geflasht. Die Trial-Werte im Profil sind `PROPOSAL_NOT_APPROVED`.

## Umgesetzte Schnitte

| Schnitt | Inhalt | Wichtigste Dateien |
|---|---|---|
| S1 Anzeige | Rohwerte je Rolle (`RawTemperatureSamples`) aus denselben Quellen; `publishRawTemperatureSamples`; Anzeige nur bei `status==Ok` und Alter ≤ `kRawDisplayMaxAgeMs` (5000 ms, bewertet zur Anzeigezeit); `TemperatureValueBasis {Qualified, RawUnqualified}`; Label `quality-unqualified` (roh/raw/crudo); Qualitaet nie `Valid`; Web unveraendert (`valid=false`, `valueCelsius=null`) | `raw_temperature_samples.hpp`, `fermentation_application.cpp`, `fermentation_ui_*`, `main/fermentation_ui_renderer.cpp` |
| S6 Temperaturschutz | `ThermalSafetySupervisor` (rohwertbasiert, Kuehlkoerper + Luft, je zwei Ebenen Eingriffs-/Notgrenze, Trend, ungueltige Daten → `Blocked`, nach firmwarefester Dauer Latch `Tripped`); neues Feld `ActuationEvidence.thermalSafety` (Default `Unconfigured`), `FaultCode::TemperatureSafetyLimit` (`0x0601`, `kFaultCount` 9); Trip ruft `requireService(...)`; fluechtiger Latch, Rearm = Neustart + erneutes Setzen der Grenzen | `thermal_safety_supervisor.*`, `actuation_interlock.*`, `fermentation_application.cpp` |
| S2 Regelbereitschaft | `controlReadyForStart()`: Kette vorhanden, PI `Valid`, Planner-Parameter (Lease) `Valid`, `thermalSafety == Ok` auf frischer Auswertung; in `prepareStartProgram` **vor** `allocateForApplication()` und in `revalidatePreparedRequest` (Confirm); nur fuer Kompositionen, die `attachActuationPorts` aufrufen | `fermentation_application.cpp` |
| S3 Producer je Rolle | `ControlSensorQualityConfigs` (Luft/Kuehlkoerper/Produkt getrennt); Luft + Kuehlkoerper Pflicht, Produkt optional; bisheriger Konstruktor unveraendert | `control_sensor_evidence.*` |
| S4 Planner-Writer | `applyActuatorPlannerParameters(optional, revision)` + `actuatorPlannerParameters()`: nur `Valid` oder `nullopt` (Clear), Revisionsvertrag, nicht bei aktivem Lauf; Provenance bleibt der kanonische Lesepfad | `fermentation_application.*` |
| S5 Trial-Profil | `APP_R1_CONTROL_TRIAL` (nur `esp32_bringup`, gegenseitig ausgeschlossen mit allen Harnesses); `main/r1_control_trial_profile.hpp` (PI `Ki>0`, Qualitaet je Rolle); strikter Konsolenparser `r1_trial_commands.*` (`planner show/set/clear`, `thermal show/set/clear`, `trial status`); Harness mit Boot-Warnung und 5-s-Status; Buildskript `scripts/build_r1_control_trial.py` (nur Build) | `main/r1_control_trial_*`, `main/CMakeLists.txt`, `main/app_main.cpp`, `lib/.../r1_trial_commands.*` |

## Abweichungen und Befunde (offen benannt)

1. **Planner-/Thermalwerte zur Laufzeit statt compile-time:** Der Plan nannte fuer den Writer `planner set|show|clear`; die Werte
   kommen als Zahlen ueber die Konsole (kein Zahlenwert im Code), damit der Owner sie erst an G-P1/G-P2 festlegt. Compile-time
   bleiben nur PI und Sensorqualitaet (Injektionspunkte). Ein `matches_trial_profile`-Status entfaellt deshalb; stattdessen zeigt
   `planner show` die gespeicherten Werte (`present`/`absent`).
2. **Fail-closed-Default mit Testfolgen:** `ActuationEvidence.thermalSafety` ist standardmaessig `Unconfigured`; alle Fixtures, die
   `Allowed` erwarten, setzen explizit `Ok` (Interlock-, Coordinator-, Oracle-Tests). Die FaultCode-Switches in Oracle- und
   NVS-Hosttest wurden um die neue ID ergaenzt.
3. **G10 (`thermalCompatibility`):** bleibt `Unavailable` (kein Produzent). Wirkung gemaess Code (`sensor_selection.cpp`): nur die
   *automatische validierte Rueckkehr zum Produktfuehler* nach Ausfall bleibt aus (fail-closed); Start und Regelung im Luftmodus
   sind nicht betroffen. Keine Codeaenderung.
4. **Kein produktiver Rearm-Pfad:** Nach einem Trip gibt es nur Neustart + erneutes Setzen der RAM-Grenzen (Trial-Konsole). Ein
   Service-PIN-Reset mit Stabilitaetspruefung gemaess `SAFETY_COMPONENT_FAULTS.md` existiert nicht (→ #35/#36).
5. **Kein Hardware-unabhaengiger Schutz:** S6 faellt bei MCU-Haenger/Treiberdefekt aus; die unabhaengige Absicherung (7,5-A-Sicherung,
   einmalige Temperatursicherung) bleibt eigene Voraussetzung vor C2.
6. **Nur Programmstart:** `kManualRunLimitsOwnerAvailable` bleibt `false` (D4); die S2-Pruefung wirkt auf Programm- und (Confirm)
   manuelle Starts.

## Digitale Tests (alle PASS)

Vollstaendige native Suite auf Code-HEAD `3fd1fd3`: **1930 Testfaelle, alle bestanden**. (Ein frueherer Lauf auf `3a02ea5`
scheiterte nur an einem Compile-Fehler in `test_issue90_product_recovery_oracle`: nicht behandelter neuer `FaultCode`-Wert,
`-Werror=switch`; behoben in `3fd1fd3`, dort und im NVS-Hosttest.) Neue/erweiterte Suiten:

| Suite | Inhalt |
|---|---|
| `test_raw_temperature_display` (7) | Anzeige leer ohne Publikation; frisch → `RawUnqualified`, nie `Valid`; Fehler/fehlend nicht angezeigt; Ablauf nach 5 s ohne neue Probe; Wiederkehr; Isolation (kein Evidenz-/Start-Einfluss); Web `null` |
| `test_renderer_boundary` (+1) | eigenes Label `roh/raw/crudo`, nicht `stale/valid` |
| `test_thermal_safety_supervisor` (15) | Unconfigured; Limitvalidierung (11 Fehlerfaelle); alle 8 Grenzen (Latch haelt); Notgrenze vor Eingriff; ungueltige Daten → `Blocked`, Latch nach Frist; Zeit rueckwaerts; Limit trotz ungueltigem Zweitsensor; Trend (laenger/kurz/langsam); Idempotenz; Latch verweigert Konfiguration |
| `test_actuation_interlock` (+5) | `thermalSafety` Pflicht fuer jede Freigabe, Default fail-closed, Trip ohne Aktivierung beobachtet, `0x0601`/`kFaultCount 9`, Watchdog-Reset verlangt `Ok` |
| `test_r1_control_chain_integration` (+7, gesamt 17) | Trip sperrt Peltier, sichtbar (`ServiceRequired`, `FaultCode 0x0601`), Latch haelt, Aussenluefter-Nachlauf endet planmaessig (nicht dauerhaft an, nicht abgeschnitten); Stale blockiert ohne Latch; Admission-Matrix (Planner fehlt, PI unkonfiguriert, Thermal unkonfiguriert/blockiert/tripped); Zustandssprung Prepare→Confirm (Trip/Stale/Fault); Programmstart vor Command-Identitaet verweigert (High-Water unveraendert) |
| `test_actuator_planner_writer` (9) | Default absent; gueltig speichern/lesen; ungueltig/unkonfiguriert abgelehnt ohne Slot; fehlende/veraltete Revision; Clear mit fortgeschrittener Revision; nicht bei aktivem Lauf; nicht gestartet; Persistenz ueber Neustart inkl. Clear; Werksreset entfernt Slot |
| `test_control_sensor_evidence` (4) | Kuehlkoerper weiterer Bereich; Luft + Kuehlkoerper Pflicht; Produkt ohne Config nie `Valid`; alter Konstruktor unveraendert |
| `test_r1_trial_profile_and_commands` (7) | Trial-PI `Valid` mit `Ki>0` in allen vier Saetzen (Produkt-Default bleibt `Unconfigured`); Qualitaet je Rolle; strikter Parser (10/12 Felder, keine Hex/Exponent/NaN/Inf/negative ms) |
| `test_peltier_composition_guard` (+3) | Trial-Harness nur unter seinem Flag und ohne Aktor/Treiber/Interlock/Regler; keine Lib kennt das Profil; CMake-Block nur bring-up und exklusiv; `app_main` erreicht den Trial nur im Flag-Zweig |

## Builds und Static Analysis

| Pruefung (Code-HEAD `3fd1fd3`, Arbeitsbaum sauber) | Ergebnis |
|---|---|
| `scripts/run_pre_ready_gates.sh self-check` | `BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS` (clang-format, clang-tidy `PASS`) |
| `build_esp_idf_profiles.py all --require-clean-source-tree` | PASS; Release 1 807 616 B, Bring-up 1 818 192 B; `strings` in **beiden**: 0 Treffer fuer `TRIAL_NOT_COMMISSIONED`, `R1_TRIAL`, `thermal set`, `planner set`, `r1_control_trial` |
| `scripts/build_r1_control_trial.py --clean` (Source-HEAD `67c7f0829f1bc408c615e300285580fad522a9f3`, sauber; `lib/`, `main/`, `scripts/` identisch zum Code-HEAD `3fd1fd3`) | PASS; `build/esp32_bringup_r1_trial/esp32_fermentationsschrank.bin` 1 831 696 B, **SHA-256 `86bf7eba8fc05d8286e61ce9de8898c9018f35464602a5eee62a738e10fbcdce`**; Partitionstabelle SHA-256 `d7f180e4…ccd9d` (`nvs 0x9000/24K`, `phy_init 0xf000/4K`, `factory 0x10000/3008K`, `state_store 0x300000/1M`); enthaelt `TRIAL_NOT_COMMISSIONED`, `R1_TRIAL_READY`, `thermal set`, `planner set`; Kopie `../hw-evidence-pr221/trial-image/` (Modus 700); **kein Flash** (Hash gilt nur fuer eine Review-Vorlage; jedes Gate bindet einen eigenen Hash) |

**Nicht ausgefuehrt:** esp-clang-Static-Analysis der ESP-IDF-Profile und voller Pre-Ready-Lauf (nicht angeordnet);
`pre-ready/local` nicht gesetzt (kein PASS behauptet). Das Trial-Image ist kein Hardwarenachweis; sein Hash gilt nicht fuer ein
Gate (Neubau auf dem final freigegebenen Stand).

## Zuordnung

C0/C1/C2-Hardwarenachweise sind `NOT_RUN` (gestufte Gates, siehe Plan §8). Ergebnisse spaeter: #32 (Luefter/Nachlauf C1),
#33 (Peltier C2, `HW-33-01/-02/-03` weiter offen), #34/#35 (Parameter), #36 (Fehlerinjektion/Abnahme), #37 (Dauerlauf).
Summer: entfaellt aus R1 (Ownerentscheid D7).
