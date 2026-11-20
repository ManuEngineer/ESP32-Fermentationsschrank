# R1 – Gatepaket C0 (nur Displaytemperaturen), PR #221

Status: **vorbereitet, NICHT erteilt.** `FLASH=NO`, `ACTUATOR_RELEASE=NO`, `REAL_PELTIER_TEST=NOT_RUN`, `FIX_REVIEW_R221_4=AUSSTEHEND`.
Dieses Paket fuehrt nichts am Geraet aus. Ein Flash setzt (1) ein PASS des Independent Fix-Reviews zu R221-4 und (2) die ausdrueckliche
Freigabe **dieses exakten Image-Hashes** (Gate G-T + G-C0) voraus. Es nutzt keinen verbrauchten Gate aus #190.

## 1. Image (Clean-Tree-Build, nichts geflasht)

| Feld | Wert |
|---|---|
| Source-HEAD | `307c38281447243a1abed5a226cfa337bff3e9a3` (sauber, `SOURCE_TREE_CLEAN=YES`) |
| Build | `python3 scripts/build_r1_control_trial.py --clean` (`esp32_bringup` + `APP_R1_CONTROL_TRIAL`) |
| App-Datei | `build/esp32_bringup_r1_trial/esp32_fermentationsschrank.bin`, 1 832 944 B; lokale Kopie `../hw-evidence-pr221/trial-image/esp32_fermentationsschrank.r221-4-final.bin` (Modus 700) |
| **SHA-256 App** | `bbaaa7643fc60786eeeb9b44daa3ffddcae8643d6bf88b06dd5c844f86004cec` |
| Partitionstabelle (Build) | SHA-256 `d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d`; `nvs 0x9000/24K`, `phy_init 0xf000/4K`, `factory 0x10000/3008K`, `state_store 0x300000/1M` |
| Inhalt | Trial-Banner (`TRIAL_NOT_COMMISSIONED`, `EXTERNAL_PRECONDITION`), Trial-Konsole, Firmware-Temperaturschutz, Rohwertanzeige |

Aeltere Trial-Images (`86bf7eba…`, `1b0be67f…`, `fdd0bf30…`) sind ersetzt und duerfen nicht geflasht werden.

## 2. Ablauf C0 nach Freigabe (App-only, kein Erase)

1. Port `/dev/ttyUSB0` (FT232, Autoreset RTS→EN, DTR→IO0) fuer die Sitzung dauerhaft offen halten (Logger).
2. Read-only: installierte Partitionstabelle (`0x8000`, 3072 B) lesen; muss der Build-Tabelle entsprechen, sonst Abbruch ohne Flash.
3. Read-only: SHA-256 von `nvs` (24 KiB) und `state_store` (1 MiB) als Integritaetsvergleich (nur Hash, keine Sicherungskopie).
4. **Einmalig App-only:** `esptool write-flash --flash-mode dio --flash-freq 40m --flash-size 4MB 0x10000 <App-Datei>`, danach `verify-flash`
   (`Hash of data verified` + `digest matched`). Kein `erase_flash`, kein Bootloader-/Partitionstabellen-/NVS-/`state_store`-Schreiben.
5. Ein Reset, passiv beobachten (ca. 3 min); Display parallel beobachten.
6. Danach erneut `nvs`/`state_store`-Hash: unveraendert.

**Nicht Teil von C0:** Programmstart, Aktorfreigabe, `planner set`, `thermal set`, jede Parametrierung, Peltierbetrieb, Luefterregelung.
Erlaubt als rein lesend: `trial status`, `planner show`, `thermal show`. Kein anderer Befehl.

## 3. Erwartungen und Beobachtungen

**Bootlog:** `real actuators: initial_off; build policy flag only, not a physical inhibit …`; `peltier bridge: ready_initial_all_off
chain=attached_to_control_chain …`; `R1_TRIAL TRIAL_NOT_COMMISSIONED … control_chain_attached=YES initial_outputs=OFF
real_gpio_bts7960_outputs_possible=YES`; `R1_TRIAL peltier_power_path_must_be_physically_disconnected_for_C0_C1=EXTERNAL_PRECONDITION`;
`ds18b20 sampler: started`; `R1_TRIAL_UART_RX_READY result=PASS`; Ausgaenge `ready_inactive`; keine Panic/WDT/`E`-Zeilen ausser den bekannten
Warnungen (WiFi-Authmode, LCD-Callback).

**Sensorstatus (`R1_TRIAL_RAW`/`R1_TRIAL_STATUS`, alle 5 s):** je Rolle `status=Ok`, `age_ms` typisch unter ca. 2,5 s;
`R1_TRIAL_THERMAL status=unconfigured reason=not_configured`; `R1_TRIAL_PLANNER state=absent`; `run_active=0`; `fault_code=0x0000`.

**Display:** Schrankluft, Produkt und Kuehlkoerper zeigen ihre echten Temperaturen mit Label `roh` (DE) / `raw` (EN) / `crudo` (ES),
nie `gueltig/valid`; Werte aktualisieren sich im ca. 2-s-Takt; Rollenzuordnung entspricht der gespeicherten ROM-Bindung (Luft
`A60000006C724828`, Kuehlkoerper `F90000006C4C6228`, Produkt `CF0000006C415328`) und einer vom Owner angewaermten Probe je Rolle (je eine
Rolle einzeln erwaermen, ohne Sensoren abzuziehen). Diagnoseseite: Rohwert, Alter, Label.

**Fehlende Messwerte (Owner, je einmal, Peltier weiter getrennt):** einen Fuehler abziehen → nach hoechstens ca. 5 s (Anzeigegrenze) zeigt
das Display fuer diese Rolle `--` statt eines alten Werts; die uebrigen Rollen bleiben unberuehrt; Wiederanstecken → Wert kehrt zurueck.
Keine Reaktion ausser der Anzeige (kein Start, kein Aktor).

**Abbruchkriterien:** Panic/WDT/Abort/Backtrace; `E`-Zeilen ausser bekannten Warnungen; Partitionstabelle oder Hash weicht ab; Sampler nicht
`started`; Rohwert dauerhaft ohne Sensoreingriff nicht `Ok`; Heartbeat-Luecke > 3000 ms; unerwartete Ausgangsaktivitaet. Abbruch ohne
Restore: Strom/EN trennen, Fehler und Log-Auszug dokumentieren, ggf. nach neuer Freigabe korrigiertes Image flashen (Ownerentscheid DEV).

## 4. Vom Owner am Geraet zu bestaetigen (nicht vermutet; Antworten stehen aus)

1. **Peltier-Leistungspfad physisch getrennt oder stromlos** (nicht Software, nicht `EN`/Reset, nicht das Buildflag): wie genau (Klemmen/Versorgung)?
2. **Versorgung/Lasten:** welche Quellen liegen an (ESP32/FT232, 12-V-Zweig?), welche Lasten sind angeschlossen (Luefter OUT1/OUT2; kein Summer in R1).
   Das Trial-Image haengt die Aktorkette an; ohne Programmstart und ohne gueltige Parameter gibt es keine Freigabe, das ersetzt aber keine physische Trennung.
3. **Sensoren:** alle drei DS18B20 wie bisher verdrahtet; Bindung im `state_store` unveraendert.
4. **Aufbau:** FT232 `/dev/ttyUSB0` verbunden und stabil.
5. **Kenntnisnahme:** C0 beweist nur die Anzeige; kein Release-Sensor-/Regel-/Peltiernachweis; kein Backup/Restore.
6. **Review:** Independent Fix-Review R221-4 mit PASS.

## 5. Freigabe

G-T/G-C0 wird nur durch eine ausdrueckliche Ownerfreigabe erteilt, die den Image-Hash aus §1 nennt und §4 bestaetigt. Ohne sie: `FLASH=NO`.
C1 erfordert zusaetzlich G-P1 (Parameter inkl. Thermal-Schutz) bei weiterhin stromlosem Peltier; C2 zusaetzlich G-P2 und die bestaetigte unabhaengige Absicherung.
