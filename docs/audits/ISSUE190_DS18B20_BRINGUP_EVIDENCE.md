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
Produktbudget ein (C3 erst nach M1–M6).

## Status

| Schritt | Status |
|---|---|
| Flash Probe-Image, Boot-Smoke | `PASS` |
| M1–M6 (ROM-Report, vorlaeufiges Binding, Erwaermung, 10-min-Reihe, Hot-Plug, Ressourcen) | `NOT_RUN` |
| C3 Produktbudget, Release-Nachweis | `NOT_RUN` |

**Endzustand Board:** laeuft das Probe-Image (Source `e0da8ae`, Bring-up, nicht
Release), unveraenderte Partitionen, keine Bindung geschrieben, Aktoren getrennt.
