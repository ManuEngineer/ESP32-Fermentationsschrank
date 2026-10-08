# Issue #30 – S0 Stufe 1: Quelle, Lizenz, Build (ohne Hardware)

Ausgefuehrt gemaess freigegebenem Plan `docs/tasks/issue-30-ds18b20-sensor-adapters-plan.md`
(Plan-Commit `de6d2a0b3c77e1da13c2f830ba5865e11784185b`, Abschnitt S0 Stufe 1).
Reine Build-/Papierpruefung; **keine** Hardware, nichts geflasht, kein Produktcode.

```text
BASE=main 7b16dbeb95ab09fdbe13d6c524c7a09ec721fb7d (esp32_release, ESP-IDF v6.1 fff9895c)
ESPRESSIF_ONEWIRE_BUS_1_1_1_PLUS_DS18B20_0_4_0=PASS_BUILD_GATE
ESPRESSIF_ONEWIRE_BUS_1_1_2_PLUS_DS18B20_0_4_0=PASS_BUILD_GATE
DALLASTEMPERATURE_4_0_6_ONEWIRE_2_3_8=REQUIRES_UNAPPROVED_FRAMEWORK_CHANGE (Papier-Check, kein Build)
STAGE_1_RESULT=PASS (Espressif-Pfad)
BUILD_REPRODUCED=YES (zweiter Build in frischem Worktree: identische Groessen und Component-Hashes)
HARDWARE_SSOT_CHANGE_NEEDED=NO_EVIDENCE_IN_STAGE_1 (GPIO32/33, zwei Busse, 4,7 kOhm unveraendert; Hardwarepruefung erst Stufe 2/3)
STAGE_2_STAGE_3=NOT_RUN (Owner-Hardware H1-H4 offen)
VERSION_DECISION=PENDING (Empfehlung 1.1.2, siehe unten; endgueltig nach Stufe 2/3)
ACTUATOR_RELEASE=NO
```

## Methode

Drei isolierte Worktrees von `main` (`7b16dbe`), jeweils `scripts/build_esp_idf_profiles.py
release` auf ESP-IDF v6.1: (a) Base unveraendert; (b)/(c) Base plus `espressif/ds18b20 0.4.0`
und `espressif/onewire_bus` `1.1.1` bzw. `1.1.2` in
`lib/device_platform_esp_idf/idf_component.yml` (fest gepinnt), `PRIV_REQUIRES` und einer
reinen Link-Probe (`ds18b20_spike_probe.cpp`, aus `app_main` referenziert, nie ausgefuehrt).
Die Probe nutzt den geplanten API-Teil: zwei RMT-Busse auf GPIO32/33
(`onewire_new_bus_rmt`), `onewire_bus_reset`, Enumeration, `ds18b20_new_device_from_enumeration`,
`ds18b20_set_resolution`, Trigger-Variante A (`ds18b20_trigger_temperature_conversion_for_all`)
und Variante B (`onewire_bus_write_bytes` mit SKIP-ROM + 0x44), `ds18b20_get_temperature`,
`ds18b20_get_device_address`. Der erste Kandidatenbuild scheiterte nur an einem fehlenden
`#include <initializer_list>` in der Probe (kein Komponentenfehler) und wurde korrigiert.
Probe-Patch: `docs/audits/ISSUE30_S0_STAGE1_PROBE.patch` (ohne `dependencies.lock`, das der Component Manager aus dem Manifest regeneriert; Hashes siehe unten; Variante `onewire_bus 1.1.2`; die Variante `1.1.1` unterscheidet sich nur durch den Versionsstring in `idf_component.yml` und die Hashes in `dependencies.lock`). Er ist nur Evidence, kein Produktcode, und gehoert nicht zum Produktbaum.

## Quelle, Lizenz, Abhaengigkeiten

| Punkt | onewire_bus | ds18b20 |
|---|---|---|
| Version/Datum | `1.1.1` (2026-05-11), `1.1.2` (2026-09-15) | `0.4.0` (2026-05-29) |
| Lizenz | Apache-2.0 (`LICENSE` SHA-256 `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30`, SPDX in den Quelldateien) | Apache-2.0 (identische `LICENSE`) |
| Component-Hash (`dependencies.lock`) | `1.1.1`: `a85dc1ce…d05ce`; `1.1.2`: `dcce4fb1c2fc43c3d02de1cc9abdf062127468a785578723696de3817c526f99` | `38022d0c39c08b1df3e77f95a3f9544251549a1fd9b6263357bf714026f4bb6b` |
| Abhaengigkeiten | nur `idf >=5.0` (privat); Peripherie `esp_driver_rmt/uart/gpio` | `onewire_bus ^1.1.0` (privat); `sensor_hub` nur bei `DS18B20_SENSOR_HUB` |
| Transitiv geladen | keine weiteren Registry-Komponenten | `sensor_hub` **nicht** geladen (`CONFIG_DS18B20_SENSOR_HUB is not set`, nicht in `managed_components`) |
| Notice | Apache-2.0-`LICENSE` der Komponente muss im Distributionshinweis erhalten bleiben | gleich |

`dependencies.lock` waechst um genau zwei Komponenteneintraege (`espressif/ds18b20`, `espressif/onewire_bus`) samt Auflistung und neuem `manifest_hash`; weitere Registry-Komponenten kommen nicht hinzu.
Die Audits fuehrten `onewire_bus 1.1.1`; `1.1.2` ist neuer und behebt laut Changelog die
ROM-Suche (1-basierte Bitnummerierung), verlaengert `tRSTH` auf 480 µs und lehnt ungueltige
GPIO-/Puffergroessen ab.

## Reproduzierbarkeit

Ein zweiter Build von `main` (`7b16dbe`) plus Probe-Patch `1.1.2` in einem frischen Worktree
lieferte dieselbe `.bin`-Groesse (1'711'072 B), dieselben Sektionsgroessen
(`.flash.text` 1'364'646, `.flash.rodata` 225'180, `.iram0.text` 100'815, `.dram0.data` 18'975,
`.dram0.bss` 73'696 B) und dieselben Component-Hashes wie der erste. Der Build ist damit ueber
`dependencies.lock` und fest gepinnte Versionen reproduzierbar (Binaries sind wegen des
Kompilierzeitstempels nicht byte-identisch, die SHA-256 wird nicht verglichen).

## Hardware-SSOT

Kein Befund aus Stufe 1 spricht gegen die Boardprofil-SSOT: GPIO32/GPIO33 sind keine
Boot-Strap-/Flash-Pins, das RMT-Backend benoetigt je Bus einen TX- und einen RX-Kanal
(zwei Busse = vier von acht Kanaelen, ohne Konflikt mit dem SPI-Display/Touch), und die
Komponenten verlangen keine bestimmte Pull-up-Bestueckung (`en_pull_up` ist optional,
extern 4,7 kOhm bleibt Ausgangsbasis). Ob die reale Verdrahtung, Pull-ups und Kabel die
SSOT erfuellen, ist erst in Stufe 2/3 mit Hardware pruefbar. Es gibt keinen Aenderungsbedarf
und keine Alternative vorzulegen.

## Funktionspruefung am Quelltext (kein Laufzeitbeweis)

* Mehrbus/Mehrsensor: je Bus eigenes `onewire_bus_handle_t`; `onewire_new_device_iter`
  enumeriert mehrere Geraete; ROM = `uint64_t` (`onewire_device_address_t`), CRC im
  Treiber gepruefte ROM-/Scratchpad-CRC8 (`ESP_ERR_INVALID_CRC`).
* Nicht blockierend: **nein** fuer die Treiberfunktionen. `ds18b20_trigger_temperature_conversion`
  (`vTaskDelay` 100/200/400/800 ms) und `_for_all` (fest 800 ms); Busoperationen warten bis
  1000 ms und nehmen den Bus-Mutex mit `portMAX_DELAY`. Das Kommando "convert all" ist ueber die
  oeffentliche `onewire_bus_write_bytes`-API ohne Treiberpause moeglich (Variante B, kompiliert).
* Anwesenheit: `onewire_bus_reset` trennt `ESP_ERR_NOT_FOUND` (kein Presence-Puls) von
  `ESP_ERR_TIMEOUT`.
* Einschaltwert: `ds18b20_get_temperature` liefert `ESP_ERR_INVALID_STATE` bei 85,0 °C.
* `ds18b20_set_resolution` schreibt nur das Scratchpad (kein EEPROM-Kopierbefehl).
* RMT-Backend fuer ESP32 verfuegbar; UART-Backend vorhanden (Rueckfall). Je Bus ein RMT-TX-
  und ein RMT-RX-Kanal (ESP32: acht Kanaele).

## Ressourcen: Base gegen Kandidat (statisch, `esp32_release`)

| Groesse (Byte) | Base `7b16dbe` | + `1.1.1`/`0.4.0` | Delta | + `1.1.2`/`0.4.0` | Delta |
|---|---:|---:|---:|---:|---:|
| `esp32_fermentationsschrank.bin` | 1'687'280 | 1'710'832 | +23'552 | 1'711'072 | +23'792 |
| `.flash.text` | 1'349'402 | 1'364'506 | +15'104 | 1'364'646 | +15'244 |
| `.flash.rodata` | 219'000 | 225'084 | +6'084 | 225'180 | +6'180 |
| `.iram0.text` | 98'551 | 100'815 | +2'264 | 100'815 | +2'264 |
| `.dram0.data` | 18'867 | 18'975 | +108 | 18'975 | +108 |
| `.dram0.bss` | 73'680 | 73'696 | +16 | 73'696 | +16 |

Das Delta enthaelt den RMT-Treiber (`CONFIG_RMT_*_ISR_HANDLER_IN_IRAM=y` erklaert den
IRAM-Anteil). Die App-Partition (3 MB) hat ausreichend Reserve. **Heap, Task-Stack und
RMT-Laufzeitpuffer sind statisch nicht messbar** und folgen in Stufe 2/3
(`TBD_IMPLEMENTATION_BUDGET` bleibt nie Laufzeitwert).

## DallasTemperature / OneWire (Papier-Check)

`DallasTemperature.h` (v4.0.6, Tag geprueft) bindet `<Arduino.h>` ein; `OneWire.h` (v2.3.8)
bindet `<Arduino.h>`/`WProgram.h` und `util/OneWire_direct_regtype.h` mit Arduino-Pinmakros
ein. Beide setzen einen Arduino-Core voraus, der kein aktueller Produktionspfad ist.
Ergebnis `REQUIRES_UNAPPROVED_FRAMEWORK_CHANGE`; da der Espressif-Pfad Stufe 1 besteht, wird
dieser Kandidat gemaess Plan nicht weiter vertieft.

## Folgerung und naechste Schritte

* Stufe 1 ist fuer den Espressif-Pfad `PASS_BUILD_GATE` (beide `onewire_bus`-Versionen).
  Empfehlung: `onewire_bus 1.1.2` (neuer, korrigierte ROM-Suche); die endgueltige Version und
  die Konvertierungsvariante (A/B) werden nach Stufe 2/3 festgelegt.
* Stufe 2 (ein realer Sensor) und Stufe 3 (Zielverdrahtung, 1000 Zyklen, 10 Neustarts, Task-/
  Heap-Budget) brauchen die Owner-Antworten H1–H4 und Hardware.
