# Issue #190 – G2-Gatepaket DEV-Bench-Test (Plan Rev. 3 `10cc3f7`, PR #220)

Status: **vorbereitet, G2 NICHT erteilt.** `FLASH=NO`, `ACTUATOR_RELEASE=NO`,
`REAL_PELTIER_TEST=NOT_RUN`. Dieses Paket fuehrt nichts am Geraet aus; es enthaelt
das zu pruefende Image und die Owner-Checkliste (Plan §7). Ein Flash setzt eine
ausdrueckliche, separate G2-Freigabe **dieses exakten Hashes** und den
abgeschlossenen Independent Review voraus.

## 1. Image (Clean-Tree-Build, nichts geflasht)

| Feld | Wert |
|---|---|
| Source-HEAD | `63cddd32bd8f6b3ff131e2fcd6cae2018ed899cf` (Arbeitsbaum sauber, `SOURCE_TREE_CLEAN=YES`; Firmware-Code identisch zu C1 `0370b32`) |
| Build | `python3 scripts/build_issue30_sensor_commissioning.py --clean` (`esp32_bringup` + `APP_ISSUE_30_SENSOR_COMMISSIONING`) |
| App-Datei | `build/esp32_bringup_issue30/esp32_fermentationsschrank.bin`, 1 828 432 B (Kopie `../hw-evidence-pr220/g2-image/`, Modus 700) |
| **SHA-256 App** | `93c750db95f649cbae35afd5a16613328c6b69b7b9c8b343acbd912ac9a9496b` |
| Partitionstabelle (Build) | SHA-256 `d7f180e4…ccd9d`; `nvs 0x9000/24K`, `phy_init 0xf000/4K`, `factory 0x10000/3008K`, `state_store 0x300000/1M` |
| Inhalt | `approved product budget (bring-up harness)` vorhanden, `PROBE budget` nicht vorhanden; Harness nur Bring-up, nicht im Release |

Der Hash weicht vom frueheren Harness-Build (`ce388bc2…`, Source `0370b32`) ab,
weil die Source-HEAD-Identitaet in den Build eingeht; massgeblich ist ausschliesslich
der Hash oben. Wird danach Code geaendert, ist dieses Paket ungueltig (Neubau).

## 2. Ablauf nach G2 (keine Aktion vor Freigabe)

1. Port `/dev/ttyUSB0` (FT232, Autoreset RTS→EN, DTR→IO0) dauerhaft fuer die Sitzung offen halten.
2. Read-only: installierte Partitionstabelle (`0x8000`) auslesen; muss der Build-Tabelle (oben) entsprechen, sonst Abbruch ohne Flash.
3. Read-only: SHA-256 von `nvs` (24 KiB) und `state_store` (1 MiB) als Integritaetsvergleich (nur Hash, keine Sicherungskopie).
4. **Einmalig App-only:** `esptool write-flash --flash-mode dio --flash-freq 40m --flash-size 4MB 0x10000 <App-Datei>`, danach `verify-flash`; beides muss den Hash bestaetigen. Kein `erase_flash`, kein Bootloader-/Partitionstabellen-/NVS-/`state_store`-Schreiben.
5. Ein Reset, passiv beobachten (ca. 150–300 s): `ISSUE190_READING`, `ISSUE190_RESOURCES`.
6. **Genau einmal** `ds18b20 report` (read-only). Kein `bind`/`offset`/`clear`, kein anderer Befehl.
7. Erneut `nvs`/`state_store`-Hash: unveraendert.

## 3. Erwartung und PASS-Regel

- Boot: `ds18b20 budget: approved product budget (bring-up harness): stack=4096 priority=2 core=1`, `ds18b20 sampler: started`, `ISSUE30_READY … real_actuators_enabled=NO`, Ausgaenge `ready_inactive`.
- `engine_binding_valid=1`, ROMs air `A60000006C724828`, heatsink `F90000006C4C6228`, product `CF0000006C415328`.
- Je Kanal `status=Ok`, fortschreitende Zeitstempel, `age_ms` typisch unter ca. 2,5 s (Richtwert, kein alleiniger Schalter).
- `task_stack_hwm_bytes` unter dem Produktbudget 4096 B; Stack-Reserve = HWM.
- PASS erst bei tatsaechlicher Beobachtung auf diesem Image-Hash; sonst `NOT_RUN`/`NOT_PROVEN`.
- Logtext `peltier bridge: ready_all_off_not_connected_to_planner` ist veraltet (Plan §7 Nr. 4) und kein Trennnachweis.

## 4. Abbruchkriterien (logbasiert)

Panic/WDT/Abort/Backtrace; `E`-Zeilen ausser vorbestehenden Warnungen (WiFi-Authmode, LCD-Callback); `ISSUE190_RESOURCES task=absent`; Sampler nicht `started`; Kanal dauerhaft nicht `Ok` ohne Eingriff; Heartbeat-Luecke > 3000 ms; unerwartete Ausgangsaktivitaet; Partitionstabelle/Hash weicht ab. Ablauf ohne Restore: Test sicher beenden (Strom/EN trennen, Leistungspfad bleibt gesperrt), Fehler dokumentieren, ggf. nach neuer Freigabe korrigierte DEV-Firmware flashen (Ownerentscheid D3, kein Backup).

## 5. Vom Owner am Geraet zu bestaetigen (nicht vermutet; Antworten stehen aus)

1. **Aktorsperre:** Peltier-/BTS7960-Leistungspfad physisch getrennt oder stromlos (nicht nur Software; `EN`/Reset zaehlt nicht). Wie genau (Klemmen/Versorgung abgezogen)?
2. **Versorgung/Lasten:** welche Quellen liegen an (ESP32 per USB/FT232? 12-V-Zweig?), welche Lasten sind angeschlossen (Luefter an OUT1/OUT2, Summer an OUT3?). Das Harness-Image haengt die #219-Kette an; Software-LOW ist kein Sperrnachweis.
3. **Sensoren:** alle drei DS18B20 wie bei M1 verdrahtet (GPIO32 zwei Sensoren, GPIO33 Produkt); Bindung im `state_store` weiterhin die persistierte aus M1.
4. **Aufbau:** FT232 `/dev/ttyUSB0` verbunden, Verdrahtung stabil (frueherer Kabelausfall bei M2).
5. **Kenntnisnahme:** Kein Backup/Restore (D3), kein Schrank-Thermiknachweis, keine Release-Sensorevidenz, keine Peltierfreigabe.
6. **Review:** Independent Review des Diffs abgeschlossen, keine offenen Blocker (Status aktuell: nicht von mir bestaetigt).

## 6. Freigabe

G2 wird nur durch eine ausdrueckliche Ownerfreigabe erteilt, die den Image-Hash aus §1 nennt und §5 bestaetigt. Ohne sie bleibt `FLASH=NO`.
