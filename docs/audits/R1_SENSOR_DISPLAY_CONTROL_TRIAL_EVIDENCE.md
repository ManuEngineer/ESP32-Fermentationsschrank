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

## Review-Korrektur R221-4 (Fix Review auf `a14478a`, Fix-Code-HEAD `09c4b944ef8be1afbdaf5f6ffbb7d04510e9ec02`)

**Befund:** `judgeTrend()` konnte `Within` (damit `Ok`) liefern, ohne dass eine Vergleichsmessung im Trendfenster existierte (z. B.
Fenster 10 s, max. Probenalter 60 s, zwei Messungen 55 s auseinander: Beobachtungsspanne ≥ halbes Fenster, aber keine Baseline im Fenster).
**Korrektur:** `Within` nur mit mindestens einer gueltigen Baseline mit Spanne zwischen halbem und ganzem Fenster; sonst
`TrendObservationPending` (Peltier gesperrt). Rueckwaerts laufende oder ungeordnet eintreffende Messzeitstempel (aelter als die neueste
erfasste Messung) verwerfen die Historie und starten die Beobachtung neu (fail-closed, kein `Ok` bis zur neuen Beobachtung); gleicher Zeitstempel
bleibt idempotent.

| Tests (`test_thermal_safety_supervisor`, 23) | Inhalt |
|---|---|
| `test_two_readings_further_apart_than_the_window_never_yield_an_ok` | Fenster 10 s, Probenalter 60 s, 55 s Abstand → `Blocked/TrendObservationPending`; dauerhaft zu seltene Proben nie `Ok`; regulaere Proben danach `Ok` |
| `test_backwards_or_unordered_timestamps_are_fail_closed` | Zeitstempel rueckwaerts bzw. ungeordnet → Pending; `Ok` erst nach neuer ausreichender Beobachtung |
| unveraendert PASS | 600-s-Fenster (Anstieg loest aus / langsame Erwaermung bleibt `Ok`), unregelmaessige Intervalle (1–4 s), spaeter schneller Anstieg, Luecke, Idempotenz, Speichergroesse |

| Pruefung (Code-HEAD `09c4b94`, Arbeitsbaum sauber) | Ergebnis |
|---|---|
| native Suite | **1944 Testfaelle, alle bestanden** |
| `run_pre_ready_gates.sh self-check` | `BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS` |
| `build_esp_idf_profiles.py all --require-clean-source-tree` | PASS; Release 1 808 256 B, Bring-up 1 818 832 B; in beiden 0 Trial-Strings |
| Trial-Image `build_r1_control_trial.py --clean`, Source-HEAD `09c4b944ef8be1afbdaf5f6ffbb7d04510e9ec02` (sauber), `esp32_bringup` + `APP_R1_CONTROL_TRIAL` | PASS; `esp32_fermentationsschrank.bin` 1 832 944 B, **SHA-256 `fdd0bf305c11487a8f85a2a3c2ca82378035d44ce1fa3af2a464d8605475d23e`**; Partitionstabelle SHA-256 `d7f180e4…ccd9d` (unveraendert); Kopie `../hw-evidence-pr221/trial-image/esp32_fermentationsschrank.r221-4.bin`; **nicht geflasht** |

Dieses Image **ersetzt** `1b0be67f…` (Source `81c0afc`) und `86bf7eba…` (Source `67c7f08`); nur der hier genannte Hash gilt als Review-Vorlage fuer das
Flash-Gate G-T. Verbleibende Einschraenkungen unveraendert (esp-clang/Pre-Ready nicht ausgefuehrt; kein produktiver Rearm-Pfad; Firmware-Schutz ersetzt keine
unabhaengige Absicherung; Peltier nach Boot/Konfiguration/Datenluecke fuer das halbe Trendfenster gesperrt).

## Finaler Stand fuer das Fix-Review R221-4 (Code-HEAD `307c38281447243a1abed5a226cfa337bff3e9a3`)

Der Fix R221-4 steckt in `09c4b94` (siehe oben); `307c382` ergaenzt den exakten Auftragsfall (Fenster 10 s, `maxSampleAgeMillis=60000`,
`invalidLatchMillis=60000`, 20 °C bei 1 s und 30 °C bei 56 s → `Blocked/TrendObservationPending`). Firmware-Quellen unveraendert gegenueber `09c4b94`.
Rueckwaerts laufende Zeitstempel: Historie verworfen (kein Integer-Underflow, da nur `newest - ts` mit `ts < newest` gerechnet wird); gefaehrlicher
Anstieg loest weiter `Tripped/CoolingRiseRate` aus; 600-s-Fenster und unregelmaessige Intervalle bleiben PASS.

| Pruefung (Code-HEAD `307c382`, Arbeitsbaum sauber) | Ergebnis |
|---|---|
| native Suite | **1945 Testfaelle, alle bestanden** (`test_thermal_safety_supervisor`: 24) |
| Builder-Self-Check | `BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS` |
| `esp32_bringup` / `esp32_release` | PASS; 1 818 832 B / 1 808 256 B; 0 Treffer fuer `TRIAL_NOT_COMMISSIONED`, `R1_TRIAL`, `thermal set`, `planner set`, `r1_control_trial` |
| Trial-Image (`build_r1_control_trial.py --clean`, Source-HEAD `307c38281447243a1abed5a226cfa337bff3e9a3`) | PASS; `esp32_fermentationsschrank.bin` 1 832 944 B, **SHA-256 `bbaaa7643fc60786eeeb9b44daa3ffddcae8643d6bf88b06dd5c844f86004cec`**; Partitionstabelle SHA-256 `d7f180e4…ccd9d`: `nvs 0x9000/24K`, `phy_init 0xf000/4K`, `factory 0x10000/3008K`, `state_store 0x300000/1M`; Kopie `../hw-evidence-pr221/trial-image/esp32_fermentationsschrank.r221-4-final.bin`; **nicht geflasht** |

**Dieses Image (`bbaaa764…`) ersetzt `fdd0bf30…`, `1b0be67f…` und `86bf7eba…`; nur sein Hash gilt als Review-Vorlage fuer das Flash-Gate.**

## C0-Versuch 2026-11-21: BLOCKED, kein Flash (Adapterdefekt vermutet)

Gate G-T/G-C0 wurde vom Owner fuer App-Image SHA-256 `bbaaa7643fc60786eeeb9b44daa3ffddcae8643d6bf88b06dd5c844f86004cec` erteilt; die fuenf Aufbaupunkte des
Gatepakets §4 hat der Owner bestaetigt (Peltier-Leistungspfad getrennt: Ownerangabe, Details nicht erfragt). Image-Datei, Groesse (1 832 944 B) und SHA-256
wurden vor dem Versuch lokal gegen den Kandidaten geprueft: identisch.

| Pruefung | Ergebnis |
|---|---|
| `esptool read-flash` (0x8000), `--before default-reset`, `chip-id`, `no-reset`, `usb-reset`, bis 15 Verbindungsversuche, mehrfach auch nach Neustecken aller Kabel | immer `No serial data received` |
| passiver Mitschnitt nach reinem EN-Reset (115200, 74880, 921600 Baud, je 6–10 s) | 0 Bytes |
| Owner beobachtet bei Portzugriffen Wechsel Bootloader/Normalstart | DTR/RTS-Leitungen wirken |
| Loopback TXD–RXD am FT232 (`/dev/ttyUSB0`, `0403:6001`, Serie `A5069RR4`): pyserial und `stty`/`cat`, 9600–460800 Baud | 0 Bytes zurueck; Treiber `ftdi_sio` geladen, Port oeffnet, Schreiben ohne Fehler |

Befund: Datenstrecke des FT232-Adapters empfaengt nicht; Defekt des Adapters wahrscheinlich (Owner: Adapter war am Vortag ungewoehnlich warm). Es gibt keinen
weiteren Adapter, der Test wurde vom Owner abgebrochen.

Nicht ausgefuehrt: Partitionstabelle lesen, Vorher-/Nachher-Hashes, `write-flash`, `verify-flash`, Bootbeobachtung, Display-/Sensorpruefung.
**`FLASH=NOT_RUN`, `C0=BLOCKED` (Hardware/Adapter), C1=NOT_RUN, C2=BLOCKED.** Auf dem Geraet wurde nichts geschrieben. Hashgebundene Freigabe fuer
`bbaaa764…` bleibt bestehen, solange Image und Aufbau unveraendert sind; nach Adapterersatz zuerst Loopback, dann Verbindung zum ESP32, dann Ablauf aus dem Gatepaket.
