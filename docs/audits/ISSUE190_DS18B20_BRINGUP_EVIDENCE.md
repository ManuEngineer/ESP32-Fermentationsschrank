# Issue #190 – DS18B20-Bring-up: DEV-Flash Probe-Image und Boot-Smoke (2026-11-19)

Scope: Plan `docs/tasks/issue-190-ds18b20-bringup-plan.md` Rev. 3 (Plan-SHA
`c47e3cce50e69d855ab9a456f23e1ccdf65e8d0f`), PR #215. Ownerfreigabe: einmaliger
DEV-Flash des Probe-Images, D4 (kein App-Backup). **Keine** Freigabe fuer
Messungen M1–M6, Release oder Aktoren. `ACTUATOR_RELEASE=NO`.

## Image

| Feld | Wert |
|---|---|
| Source-HEAD | `e0da8ae8228c334c33bab52772fd9380796befe7` (Arbeitsbaum sauber, `SOURCE_TREE_CLEAN=YES`) |
| Profil/Build | `esp32_bringup` + `APP_ISSUE_30_SENSOR_COMMISSIONING`, `scripts/build_issue30_sensor_commissioning.py` (nach `--clean`) |
| Datei | `build/esp32_bringup_issue30/esp32_fermentationsschrank.bin`, 1 802 400 B |
| SHA-256 | `06c74d6e490176b9adf5d673d2c974250499d034891327032834a27385755df8` |
| Trennung | enthaelt `PROBE budget`, kein `ISSUE32`-Code (#32-Pulse-Harness nicht enthalten) |

## Verifizierte Partition

Installierte Partitionstabelle (Read-only-Auslesen `0x8000`) ist
byteidentisch zur Build-Tabelle: `nvs 0x9000/24K`, `phy_init 0xf000/4K`,
`factory app 0x10000/3008K`, `state_store 0x300000/1M`. Geschrieben wurde nur die
App-Partition `factory` bei **`0x10000`**.

## Flash

`esptool write-flash --flash-mode dio --flash-freq 40m --flash-size 4MB 0x10000 <app.bin>`
(Autoreset RTS→EN, DTR→IO0, FT232 `/dev/ttyUSB0`): `Wrote 1802400 bytes (1127334
compressed) at 0x00010000`, `Hash of data verified.`; unabhaengige Gegenprobe
`verify-flash`: `Verification successful (digest matched).` Kein `erase_flash`,
kein Bootloader-/Partitionstabellen-/NVS-/State-Store-/OTA-Schreibzugriff, kein
Backup (D4); Rueckfallweg: erneuter App-only-Flash eines bekannten Images.

## Zwischenfaelle

1. Erster Verbindungsversuch scheiterte (`No serial data received`), nichts
   geschrieben; nach Verdrahtungsoptimierung durch den Owner erfolgreich.
2. Nach dem Auslesen der Partitionstabelle scheiterten zwei Folgeaufrufe mit
   `--before no-reset` (`Invalid head of packet`), vor jedem Schreibzugriff; der
   Flash lief danach mit Autoreset (`default-reset`) in einem Aufruf. Es wurde
   kein Schreibvorgang wiederholt.

## Boot-Smoke (ein EN-Reset, ca. 45 s passiv, kein `bind`/`offset`/`clear`, keine Sensoraktion)

Ausgewaehlte Zeilen (Netzwerkangaben nicht uebernommen, Rohlog nicht veroeffentlicht):

```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
I boot: Loaded app from partition at offset 0x10000
I app_init: ELF file SHA256:  016cd2d05...
I app_main: onboard outputs: inner_fan=ready_inactive outer_fan=ready_inactive buzzer=ready_inactive reserve=ready_inactive
I app_main: peltier bridge: ready_all_off_not_connected_to_planner
W app_main: ds18b20 PROBE budget (not approved for product): stack=6144 priority=2 core=1
I app_main: ds18b20 sampler: started
I issue30_commissioning: ISSUE30_UART_RX_READY result=PASS
I issue30_commissioning: ISSUE30_READY bring_up_only=YES real_actuators_enabled=NO commands=...
I issue30_commissioning: ISSUE190_READING channel=air status=MissingSample celsius=- rom=- ...
I issue30_commissioning: ISSUE190_READING channel=heatsink status=MissingSample celsius=- rom=- ...
I issue30_commissioning: ISSUE190_READING channel=product status=Ok celsius=22.188 rom=CF0000006C415328 timestamp_ms=2662 age_ms=735
I issue30_commissioning: ISSUE190_RESOURCES task_stack_hwm_bytes=4832 free_heap=31260 largest_free_block=23552
I issue30_commissioning: ISSUE190_RESOURCES task_stack_hwm_bytes=4768 free_heap=30692 largest_free_block=23552
I app_main: heartbeat: safe test mode, uptime_ms=43499
```

Befunde: 66 `ISSUE190_READING`-Zeilen in ca. 45 s (ca. 2-s-Takt je Kanal), `ISSUE190_RESOURCES`
alle ca. 10 s, 21 Zeilen `status=Ok` (nur `product`), Heartbeats normal, kein
Panic/WDT/Abort; einzige Warnungen: WiFi-Authmode und LCD-Callback (vorbestehend).
`air`/`heatsink` melden bis zum Binding erwartungsgemaess `MissingSample` ohne
Temperatur (fail-closed). Der Produktfuehler liefert ohne Binding plausible Werte
(ca. 22,2 °C). **Vorlaeufige Beobachtung, keine Messreihe:** Stack-HWM ca. 4,8 kB
von 6144 B, groesster freier Block 23 552 B; diese Werte gehen nicht in ein
Produktbudget ein; belastbare Werte siehe M6 unten (C3 braucht Ownerentscheid).

## Hardwaremessung M1–M6 (2026-11-19, gleiches Image `e0da8ae`, Owner am Geraet)

Messaufbau: ein Konsolen-Logger hielt den Port durchgehend offen (kein Reset ausser
den genannten); Rohlogs nicht veroeffentlicht (enthalten Netzwerkdaten). Zwischenfaelle:
(1) nach ca. 90 s eines ersten M2-Versuchs fiel die FT232-Verbindung aus (Owner: Kabel
defekt, danach verloetet); der Versuch ist **nicht** gewertet. (2) Waehrend des
wiederholten M2 loeste der Owner versehentlich einen kurzen EN-Reset aus; ausgewertet
wird ab dem Neustart. (3) Das erste `bind` ohne Praefix `ds18b20` wurde mit `REJECTED`
abgelehnt (Syntax `ds18b20 bind air=<ROM> heatsink=<ROM>`), danach PASS.

**M1 – ROM-Report und Zuordnung: `PASS`.** `ds18b20 report`: GPIO32 (expected_rom_bus)
`count=2`: `A60000006C724828`, `F90000006C4C6228`; GPIO33 (single_device_bus) `count=1`:
`CF0000006C415328`; kein Overflow, kein Bindungskonflikt. Vorlaeufiges
`ds18b20 bind air=A60000006C724828 heatsink=F90000006C4C6228`, ein Neustart, Binding
geladen (`engine_binding_valid=1`). Der Owner hatte die drei Fuehler auf unterschiedliche
Temperaturen gebracht: air-Kanal 43,1 °C, heatsink-Kanal 4,9 °C, product 22,1 °C;
Ownerzuordnung 43,1 °C = Innenluft, 4,9 °C = Heatsink entspricht dem vorlaeufigen Binding
(kein Tausch). **Endgueltige Zuordnung:** air = `A60000006C724828`, heatsink =
`F90000006C4C6228`, product = `CF0000006C415328`.

**M2 – Messreihe: `PASS`.** Ab Neustart ca. 10,3 min: je Kanal 308 Zyklen, alle `Ok`,
Zeitstempel alle verschieden, Zykluszeit 1895/2000/2160 ms (Min/Median/Max), Alter max.
1,2 s, ROM je Kanal konstant. Temperaturen: air 29,75–32,56 °C (sinkend), heatsink
6,63–7,19 °C, product 22,81–22,88 °C. Keine CRC-/Busfehler, kein Panic/WDT, keine
`E`/`W`-Zeilen.

**M3 – festen Fuehler trennen: `PASS`.** Luft: abgezogen -> `air=MissingSample` ohne
Temperatur (ROM-Feld bleibt `A6…4828`), `heatsink` und `product` blieben `Ok` mit eigenem
ROM; wieder angesteckt -> `air=Ok`, gleicher ROM. Heatsink: spiegelbildlich dasselbe
(`heatsink=MissingSample`, `air`/`product` `Ok`; Wiederkehr `Ok`, gleicher ROM). Kein
gueltiger Altwert, keine vertauschte Identitaet.

**M4 – Produktfuehler Hot-Plug: `PASS`.** Abgezogen -> `product=MissingSample` ohne
Temperatur, `air`/`heatsink` unberuehrt; angesteckt -> `Ok`, ROM `CF…5328` unveraendert.

**M5 – koordinierter Neustart: `PASS`.** Ein EN-Reset: `engine_binding_valid=1`,
`record_air`/`record_heatsink` identisch, ROM-Liste 2+1 unveraendert, alle Kanaele `Ok`.

**M6 – Ressourcen: `PASS` (Messwerte, kein Budget).** Ueber die gesamte Sitzung
(122 `ISSUE190_RESOURCES`-Stichproben im 10-s-Takt, ca. 20 min): Task-Stack-HWM
(Minimum freier Stack) 4768 B von 6144 B, d. h. hoechster beobachteter Stackbedarf ca.
1376 B; **kleinster in diesen Stichproben beobachteter aktueller freier Heap 30 532 B** und
**kleinster beobachteter groesster freier 8-Bit-Block 22 528 B** (Stichproben-Minima, **nicht**
der absolute Systemtiefstwert, siehe Heap-Vergleich unten); Heartbeat-Abstand der Hauptschleife max. 1343 ms bei Soll 1000 ms
(keine Blockade durch Busarbeit); kein Panic/WDT. RMT-Nutzung nicht gemessen
(aus der Konfiguration: zwei Busse). Nicht ausgeloest wurden `CrcFault`/`BusFault`
(nur `MissingSample` durch Abziehen); der Stackbedarf dieser Fehlerpfade ist damit
**nicht** gemessen.

**Heap-/Block-Vergleich am Taskstart (Vorher/Nachher, `NOT_PROVEN` fuer isolierte Taskkosten).**
Aus denselben Bootsequenzen (je Boot: `app_main: resources: point=after_ui_init` unmittelbar
vor `ds18b20Sampler.start()` und erstes `ISSUE190_RESOURCES` nach `ds18b20 sampler: started`;
Tick = ESP-Log-Zeit in ms seit Boot):

| Boot (Log) | Vorher `after_ui_init` (Tick): frei / min. frei / groesster Block | `sampler: started` (Tick) | Nachher erstes `ISSUE190_RESOURCES` (Tick): frei / groesster Block | Differenz frei / Block |
|---|---|---|---|---|
| Flash-Smoke (`boot190`) | 2016: 45 940 / 37 288 / 43 008 | 2426 | 2676: 31 260 / 23 552 | −14 680 / −19 456 |
| M1 (Boot nach `bind`, `m1b`) | 2026: 45 916 / 37 260 / 43 008 | 2426 | 2686: 31 228 / 23 552 | −14 688 / −19 456 |
| M2-Vorlauf (Boot, `m3` erster Boot) | 2416: 45 932 / 37 284 / 43 008 | 2486 | 2736: 31 256 / 29 696 | −14 676 / −13 312 |
| M2 (Boot nach versehentlichem EN-Reset) | 2016: 45 752 / 37 284 / 43 008 | 2416 | 2696: 31 244 / 22 528 | −14 508 / −20 480 |
| M5 (Neustart) | 2136: 45 752 / 37 284 / 43 008 | 2476 | 2716: 31 252 / 23 552 | −14 500 / −19 456 |

Zwischen den beiden Logs liegen 250–340 ms; in diesem Fenster laufen parallel Netzwerkaufbau
(`network_state=ConnectingHome`), Display/UI und weitere Allokationen. Die Differenz (ca.
14,5–14,7 kB frei) ist daher **keine isolierte Taskkostenmessung**; der Anteil des
Sampler-Tasks (Stack 6144 B, TCB, zwei RMT-Busse, Kanalquellen) ist aus diesen Logs nicht
trennbar. `NOT_PROVEN`: Heap-Verbrauch des Sampler-Tasks isoliert; fehlender Wert: ein
`logResources` unmittelbar vor und nach `start()` ohne parallele Netzwerkallokation (nicht
vorhanden, keine Firmwareaenderung in diesem Delta). Die absoluten Systemtiefstwerte
(`minimum_free_heap_bytes` aus `app_main: resources`) liegen je Boot bei 19 448 B
(Flash-Smoke), 21 660 B (M1), 21 524 B (M2) und 21 748 B (M5), also **unter** den
10-s-Stichprobenminima; sie fallen in die Netzwerkphase (`stable_home_wifi`/`periodic_30s`/
`idle_120s`), nicht in eine Taskstart-Messung.

## Status

| Schritt | Status |
|---|---|
| Flash Probe-Image, Boot-Smoke | `PASS` |
| M1–M6 (ROM-Report, vorlaeufiges Binding, Zuordnung, 10-min-Reihe, Hot-Plug, Neustart, Ressourcen) | `PASS` (siehe oben) |
| C3 Produktbudget, Release-Nachweis | `NOT_RUN` |

**Endzustand Board:** laeuft das Probe-Image (Source `e0da8ae`, Bring-up, nicht
Release), unveraenderte Partitionen; **persistierte Bindung geschrieben** (air `A60000006C724828`, heatsink `F90000006C4C6228`); alle drei Sensoren angeschlossen, Aktoren getrennt.

## Release-Test (2026-11-19, C3-Release-Image, Owner-Anweisung "Release-Test durchfuehren")

Scope: das tatsaechlich zu flashende Release-Image wurde aus dem aktuellen, sauberen
Source-HEAD neu gebaut und verifiziert geflasht; keine erneute Sensor-Messreihe,
keine Eingriffe an Sensoren. `ACTUATOR_RELEASE=NO`.

| Feld | Wert |
|---|---|
| Source-HEAD | `c782156dbb8afb1dcf2803da92373c2f1b059fdd` (Arbeitsbaum sauber, `SOURCE_TREE_CLEAN=YES`) |
| Build | `build/esp32_release` zuvor entfernt, `python3 scripts/build_esp_idf_profiles.py release --require-clean-source-tree`, 0 Warnungen/Fehler |
| Datei | `build/esp32_release/esp32_fermentationsschrank.bin`, 1 779 216 B |
| SHA-256 | `cad364c0a9e58024d71ace26f2d71a582e456a59c863d97d7159d7fdf6e1d6db` |
| Trennung | 0 Treffer fuer `ISSUE30`, `PROBE`, `ISSUE190`, "not approved for product" |
| Produktbudget | `kApprovedDs18b20TaskBudget = {4096, 2, 1}` (C3) |
| ELF-SHA (Boot-Log) | `ba0da9081...` |

Der Hash weicht vom Release-Build des vorherigen Handovers (`128bcfe6…`) ab; es wird keine
bitgleiche Reproduzierbarkeit zwischen Builds behauptet, massgeblich ist der oben
geflashte und verifizierte Hash.

**Flash:** installierte Partitionstabelle (Read-only) identisch zum Build (`factory app
0x10000/3008K`, `nvs 0x9000`, `state_store 0x300000/1M`). App-only bei `0x10000`:
`Hash of data verified.`, unabhaengig `verify-flash`: `Verification successful (digest
matched).` Kein `erase_flash`, kein Bootloader-/Partitionstabellen-/NVS-/State-Store-/OTA-
Schreibzugriff, kein Backup (D4, separate Flashfreigabe durch die Owner-Anweisung).

**Boot (ein EN-Reset, ca. 150 s passiv):**

```
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
I boot: Loaded app from partition at offset 0x10000
I app_main: ds18b20 sampler: started
```

Kein Panic/WDT/Abort/Backtrace, keine `E`-Zeilen; einzige Warnungen (vorbestehend):
WiFi-Authmode und LCD-Callback. 147 Heartbeats (`safe test mode`), groesster Abstand
1010 ms.

**Ressourcen (`app_main: resources`, Release):**

| Punkt (Tick ms) | frei | min. frei | groesster 8-Bit-Block |
|---|---|---|---|
| `after_ui_init` (2099) | 45 932 | 37 284 | 43 008 |
| `stable_home_wifi` (5069) | 34 500 | 26 204 | 25 600 |
| `periodic_30s` (32429) | 34 380 | 26 204 | 25 600 |
| `idle_120s` (122429) | 34 400 | 25 152 | 25 600 |

Der Heap liegt im Release-Image ohne Harness hoeher als im Probe-Image (dort
`stable_home_wifi` 26 480 frei, min. 19 448); der Heap-Verbrauch des Sampler-Tasks bleibt
damit weiterhin nicht isoliert messbar (`NOT_PROVEN`).

**Persistierte Bindung:** `NVS` (24 KiB) und `state_store` (1 MiB) sind vor und nach Flash
und Boot **byteidentisch** (SHA-256 `state_store` `8c89180d…a772`, `nvs` `24354ddf…a57d`);
beide ROMs des Commissioning-Datensatzes (`A60000006C724828`, `F90000006C4C6228`) liegen
im `state_store` (je einmal). Das Release-Image hat bewusst **keine** Ausgabe der
geladenen Bindung oder der Messwerte (D3, keine neue Release-Telemetrie); daher ist
`engine_binding_valid` und eine frische Temperatur im Release **`NOT_PROVEN`**. Der
Ladepfad (`application.sensorCommissioning()` -> `toChannelBindings` -> `start()`) ist im
Quelltext identisch zum Probe-Image, in dem `engine_binding_valid=1` und `Ok`-Werte
nachgewiesen sind (M1/M5).

**Befund Release-Test:** Das Release-Image mit Produktbudget 4096/2/1 bootet, startet den
Sampler (`started`), laeuft 150 s ohne Panic/WDT bei normalen Heartbeats und unkritischen
Heap-Werten; die persistierten Daten blieben unveraendert. **Nicht belegt:** geladene
Bindung und frische Messwerte im Release (keine Ausgabe), Stack-HWM des Sampler-Tasks im
Release (keine Ausgabe), Fehlerpfade `CrcFault`/`BusFault`, Aktorentrennung fuer diesen
Flash nicht neu abgefragt (Aktoren/BTS7960 laut Owner zuvor getrennt; Release initialisiert
alle Ausgaenge inaktiv).

**Endzustand Board:** laeuft das Release-Image `cad364c0…d6db` (Source `c782156`),
Partitionen unveraendert, persistierte Bindung unveraendert, alle Sensoren angeschlossen.

## C1 – DEV-Harness mit Produktbudget 4096/2/1 (Plan Rev. 3 `10cc3f7`, PR #220; digital, `FLASH=NO`)

Scope: Im Bring-up-Harness (`APP_ISSUE_30_SENSOR_COMMISSIONING`) wird statt des
Probe-Budgets 6144/2/1 das freigegebene `kApprovedDs18b20TaskBudget` (4096/2/1)
verwendet (Ownerentscheid D2=B); Log `ds18b20 budget: approved product budget
(bring-up harness)`. Release-Pfad unveraendert. **Keine Hardwaremessung; alle
Hardware-Restnachweise (Bindung, frische Werte, Stack-HWM unter 4096) bleiben
`NOT_RUN`/`NOT_PROVEN` bis zum separaten Gate G2.**

| Pruefung (Code-HEAD `0370b32`, Arbeitsbaum sauber) | Ergebnis |
|---|---|
| `test_ds18b20_sampling`, `test_r1_control_chain_integration`, `test_temperature_source` | PASS (39 Faelle) |
| `scripts/check_architecture_boundaries.py` | PASS |
| `run_pre_ready_gates.sh self-check` | `BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS` (clang-format PASS, clang-tidy `NOT_REQUIRED`) |
| Harness-Build `build_issue30_sensor_commissioning.py --clean` | PASS; 1 828 432 B, SHA-256 `ce388bc2a1d91dcbd85ed62fed44fae474b06217e1c482a035ca91f3a2c5eca1`; enthaelt `approved product budget (bring-up harness)`, kein `PROBE budget` |
| Release-Build `build_esp_idf_profiles.py release --require-clean-source-tree` | PASS; 1 805 040 B (identisch zur Groesse des Release-Builds vor der Aenderung), 0 Treffer fuer `ISSUE30`/`PROBE`/`ISSUE190`/`bring-up harness`; nur CMake-Komponentenvalidierungs-Hinweise, keine Compilerwarnung |

Grenzen: Der Release-Hash (`470904b1…`) ist nicht bitgleich zum frueheren Build
(keine Reproduzierbarkeitsbehauptung); die Aussage "Release unveraendert" stuetzt
sich auf identische Groesse, Quelltextdiff (nur `#ifdef`-Block und Hilfsfunktion)
und die fehlenden Harness-Strings. Der Harness-Hash ist ein Clean-Tree-Build; ein
G2-Flash nutzt den dann freigegebenen Hash. Kein ESP-IDF-esp-clang-Gesamtlauf,
kein Pre-Ready-Lauf.
