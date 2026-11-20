# R1 – Temperaturanzeige, Firmware-Temperaturschutz, Regelbereitschaft, Trial-Profil: digitale Evidence (S1–S6)

Plan: `docs/tasks/r1-sensor-display-and-control-trial-plan.md` Rev. 2, freigegeben auf Plan-SHA
`ae68a14ef611cf69b3765a2438b90ab0840107c5` (D1, Owner), PR #221. Stand dieser Evidence:
Code-HEAD (nach Review-Korrektur R221-1..3) `81c0afcb390b3ef1e01eb7e0224c9c4669d9924a`; der Reviewstand `0ccb3ac` ist durch den Abschnitt "Review-Korrektur" am Ende ersetzt.

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

## Review-Korrektur (Independent Code Review `5995028654`, reviewter HEAD `0ccb3ac`, Fix-Code-HEAD `81c0afcb390b3ef1e01eb7e0224c9c4669d9924a`)

**R221-1 Trendueberwachung.** Der 64-Punkte-Ring (~126 s bei 2-s-Takt) machte zulaessige Fenster bis 600 s unauswertbar; der
Anstiegsalarm konnte nie ausloesen. Neu: zeitgruppierte Historie (`ThermalSafetySupervisor`, 32 Slots × 24 B = 768 B, Bucketbreite
`ceil(Fenster/30)`, je Bucket die erste Messung), Worst-Case-Anstieg ueber jede Baseline zwischen einem halben und einem ganzen
Fenster. Variable Abtastintervalle sind unkritisch (zeitbasiert). **Kein `Ok` ohne Beobachtung:** bis mindestens die Haelfte des Fensters
durchgehend beobachtet ist, liefert der Supervisor `Blocked` mit `TrendObservationPending`; eine Luecke groesser als das
Sample-Alter-Limit verwirft die Historie und die Beobachtung beginnt neu. Folge: nach Boot, Konfiguration oder Datenluecke ist der
Peltier fuer die Haelfte des Fensters gesperrt (kurzes Fenster = kurze Wartezeit; Fenster 10 s → 5 s, 600 s → 300 s).

**R221-2 Konfigurationssperre.** `configureThermalSafety()` verweigert bei aktivem Programm-/manuellem Lauf oder unbekanntem
Run-Zustand mit `RunActiveOrUnknown` (gleiches Praedikat wie der Planner-Writer) und veraendert dann weder Grenzen noch Zustand, Latch,
Trendhistorie oder Invalid-Episode. `clearThermalSafetyConfiguration()` stoesst die Regelschleife sofort an: der Peltier ist ohne weitere
Schleifeniteration gesperrt, der Aussenluefter-Nachlauf bleibt erhalten, ein stiller Wiederanlauf ist ausgeschlossen (Neukonfiguration
waehrend des Laufs verweigert; nach dem Lauf braucht es Konfiguration und neuen Start).

**R221-3 Aktorstatus im Bootlog.** `real actuators: disabled` und `ready_all_off_not_connected_to_planner` waren seit #219 falsch.
Jetzt: `real actuators: initial_off; build policy flag only, not a physical inhibit; real GPIO/BTS7960 outputs are possible on an
explicit run when permitted; disconnect the Peltier power path physically if it must stay dead` und `peltier bridge:
ready_initial_all_off chain=attached_to_control_chain|not_attached_manual_harness`. Trial-Banner/Status: `control_chain_attached=YES
initial_outputs=OFF real_gpio_bts7960_outputs_possible=YES (explicit run + permission only)` und `peltier_power_path_must_be_physically_
disconnected_for_C0_C1=EXTERNAL_PRECONDITION`. Keine Sicherheitsfreigabe wurde veraendert. Das Legacy-#90-Runnerskript erkennt die neue
Markierung zusaetzlich.

| Neue/geaenderte Tests | Inhalt |
|---|---|
| `test_thermal_safety_supervisor` (21) | laengstes Fenster (600 s) bei 2-s-Samples: gefaehrlicher Anstieg loest aus; langsame Erwaermung (1200 s, +2 K/min) bleibt `Ok` ab halbem Fenster, davor `Blocked/Pending`; unregelmaessige Intervalle (1–4 s) loesen aus; spaeter schneller Anstieg gegen junge Baseline; Luecke → Pending; Idempotenz; Speichergroesse < 1600 B |
| `test_r1_control_chain_integration` (22) | `thermal set` bei aktivem Lauf verweigert (Limits unveraendert, engere Grenze greift weiter); verweigerte Konfiguration setzt weder Invalid-Episode noch Trendhistorie zurueck; `thermal clear` im Lauf sperrt den Peltier sofort, Nachlauf bleibt, kein stiller Wiederanlauf; ohne Run-Zustand verweigert |
| `test_peltier_composition_guard` (9) | Bootlog-Wortlaute (`initial_off`, `not a physical inhibit`, Kettenstatus), Trial-Banner (`EXTERNAL_PRECONDITION` …), alte falsche Texte nicht mehr vorhanden |

| Pruefung (Fix-Code-HEAD `81c0afc`, Arbeitsbaum sauber) | Ergebnis |
|---|---|
| native Suite | **1942 Testfaelle, alle bestanden** |
| `run_pre_ready_gates.sh self-check` | `BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS` (clang-format, clang-tidy) |
| `build_esp_idf_profiles.py all --require-clean-source-tree` | PASS; Release 1 808 224 B, Bring-up 1 818 784 B; in beiden 0 Treffer fuer Trial-Strings, `not_connected_to_planner`, `real actuators: disabled`; Release enthaelt `real actuators: initial_off` |
| Trial-Image `scripts/build_r1_control_trial.py --clean` | PASS, Source-HEAD `81c0afcb390b3ef1e01eb7e0224c9c4669d9924a` (sauber), `esp32_bringup` + `APP_R1_CONTROL_TRIAL`; `esp32_fermentationsschrank.bin` 1 832 912 B, **SHA-256 `1b0be67fcc25ab11392d1b9debb5e126d8364fbedc29e085b1130c07b91d7e08`**; Partitionstabelle SHA-256 `d7f180e4…ccd9d` (unveraendert); enthaelt `TRIAL_NOT_COMMISSIONED`, `EXTERNAL_PRECONDITION`, `real_gpio_bts7960_outputs_possible=YES`, `thermal set`, `trend_observation_pending`; Kopie `../hw-evidence-pr221/trial-image/esp32_fermentationsschrank.r221-corrected.bin`; **nicht geflasht** |

Das fruehere Trial-Image (`86bf7eba…`, Source `67c7f08`) ist **ersetzt** und darf nicht geflasht werden. Der Bootloader des Builds wird bei
einem App-only-Flash nicht geschrieben (sein Hash ist build-abhaengig: `38f987bc…`).

**Verbleibende Einschraenkungen:** esp-clang-Static-Analysis der ESP-IDF-Profile und voller Pre-Ready-Lauf nicht ausgefuehrt; kein produktiver
Rearm-Pfad nach einem Trip (Neustart + erneutes Setzen); der Firmware-Schutz ersetzt keine unabhaengige Absicherung; nach Boot/Konfiguration
ist der Peltier fuer die Haelfte des Trendfensters gesperrt (Absicht, fail-closed); C1/C2 brauchen die genannten Owner-Gates.
