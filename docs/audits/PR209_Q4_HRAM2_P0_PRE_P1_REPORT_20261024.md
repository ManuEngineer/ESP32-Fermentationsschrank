# PR #209 / H-RAM-2 – P0-Preflight (nur lesend) und `PRE_P1_REPORT`

Auftrag: P0-Preflight gemäß Planrevision 3.2 auf `0cb3d2bf2824699279e65959735c333a2961b773` (Owner: nur dieser Schritt;
**keine** Freigabe von P1, `H-RAM-2`, `G-F`, `G-S`, Diagnoseflash oder Merge). Messzeitpunkt 2026-10-24 11:52–12:00 (+02:00).
Durchgeführt wurden ausschließlich lesende Host-Zugriffe über den ROM-Bootloader und eine Offline-Auswertung der Abbilder.
**Kein** Produkt-Firmwarestart, kein Flash-/NVS-Schreiben oder -Erase, keine Wiederherstellung, kein Diagnoseimage, kein P1/B0,
kein Login/Web-/API-Zugriff, keine zusätzliche Instrumentierung.

## 1. Abgleich vor dem Zugriff
| Punkt | Ergebnis |
|---|---|
| Repo-/PR-HEAD | `0cb3d2bf2824699279e65959735c333a2961b773` (= `origin`, Draft-PR #209) |
| Gerät | ESP32-D0WD-V3 rev v3.1, MAC `20:50:0d:1b:2f:34` (wie H-RAM-1), 4 MB Flash (Hersteller `b3`, Gerät `4016`), 3,3 V; `esptool v5.4.0` (ESP-IDF `fff9895c82d744c7237be8847347bdd1b07c6643`) |
| Zugang | `--before default-reset --after no-reset` (ROM-Bootloader per DTR/RTS); nur `read-mac`, `flash-id`, `read-flash`; der Chip blieb im Bootloader („Staying in bootloader“) |
| Partitionstabelle (`0x8000`, aus dem Abbild) | `nvs 0x9000/24K`, `phy_init 0xf000/4K`, `factory 0x10000/3008K`, `state_store 0x300000/1M` = `partitions/issue_90_state_store.csv` |
| Installiertes Image | App-Descriptor: Version `a9a1156`, Projekt `esp32_fermentationsschrank`, IDF `v6.1`, Build `Oct 16 2026 16:40:57` |

## 2. P0 – zwei unabhängige Lesungen je Bereich
Skript: [READ.sh.txt](PR209_Q4_HRAM2_P0_READ.sh.txt) (nur `read-flash`). Jede Lesung ist ein eigener esptool-Aufruf mit erneutem Verbindungsaufbau.

| Bereich | Offset / Länge | A = B (byteidentisch, `cmp`) | SHA-256 |
|---|---|---|---|
| Boot, Partitionstabelle, `nvs`, `phy_init` | `0x0` / `0x10000` | ja | `2244ca13dde080028dc1943ba830c65d0ee5304317ed2ae5773aebf9b139f46a` |
| Factory-App | `0x10000` / `0x2F0000` | ja | `3b907b251b32b034f2ac112f25862aaa317c827707837ccf2982232e9dc05cc9` |
| `state_store` | `0x300000` / `0x100000` | ja | `fbdd870529bc6e9c0d87adc5aea9fe5b2eb496ab09ef6473c80129886d3d8b8d` |

`P0_BYTE_IDENTICAL=YES`. **Bezug zu H-RAM-1:** Boot-Bereich (`2244ca13…`) und Factory-App (`3b907b25…`) sind hash-gleich zu den dort dokumentierten Sicherungen
([H-RAM-1](PR206_H_RAM_1_EVIDENCE_20261019.md) §1): Seither (zwei Image-Wechsel, mehrere Boots) hat sich der Boot-Bereich – und damit `nvs`/`phy_init` – nicht geändert.
Für `state_store` existiert kein früherer Hash. Die Abbilder liegen **privat lokal** (`/var/lib/docker/data/ESP32-Projekte/hw-evidence-pr209-p0/`, Modus 700, enthalten Zugangsdaten) und sind nicht im Repository; hier stehen nur Hashes.
Abgeleitete Teilabbilder (aus dem Boot-Abbild geschnitten): `nvs` `0x9000–0xEFFF` `71574dfa72b90bab5796b55bc57a6d38e94abf09948e46f512235308cd836964`, `phy_init` `0xF000–0xFFFF` `f47a8ec3e9aff2318d896942282ad4fe37d6391c82914f54a5da8a37de1300c6`, Partitionstabelle `0x8000–0x8FFF` `21ce282df0f3653aa98d3b8190fdea3d29e7466f5ec31705eaef92918cd237aa`.

## 3. Offline-Auswertung (IDF-Werkzeuge, lesend)
Werkzeuge: `components/nvs_flash/nvs_partition_tool/nvs_tool.py` (`-i`, `-d storage_info|namespaces|written|blobs`), `gen_esp32part.py` (nur Anzeige). Strukturdekodierung: [DECODE.py.txt](PR209_Q4_HRAM2_P0_DECODE.py.txt) (gibt nur Statusfelder aus, keine Secret-Payloads).

### 3.1 System-NVS `nvs` (`0x9000`)
- Eine aktive Seite, **CRC OK**, 5 leere Seiten; 6 geschriebene, 0 gelöschte, 0 ungültige Einträge; **keine** Duplikate, `FREEING`-/beschädigten Seiten oder verwaisten Blobs. Standardpartition nicht leer.
- Namespace `phy`: `cal_data` (Blob, 1 904 B, CRC OK), `cal_mac` = `20:50:0d:1b:2f:34` (**gleich der Chip-MAC**, ebenso im `cal_data`-Kopf), `cal_version` = `4863`.
- `phy_init` (`0xF000`, 4 KiB): vollständig `0xFF`; `CONFIG_ESP_PHY_INIT_DATA_IN_PARTITION` ist nicht gesetzt, die Firmware liest/schreibt diese Partition nicht.

### 3.2 `state_store` (`0x300000`)
- 18 belegte Seiten (17 voll + 1 aktiv, alle **CRC OK**), 238 leere; keine `FREEING`-/beschädigten/ungültigen Seiten; Namespace `fermentation`.
- 23 Schlüssel, 47 geschriebene Einträge = 1 Namespace + 23 × (Blob-Index + Blob-Daten); **keine Duplikate, keine Daten ohne Index, kein Index ohne Daten**.
- Bootstrap (`cb0` v19, `cb1` v18, Schema 3): Zustand **Initialized**, Run-Epochen-Handoff **Consumed**, Epochen 2→3, Auth-Domain-Handoff **Consumed** (`cb1` = Vorgänger `InProgress`), Sequenz 17.
- Auth-Root: **Provisioned**, Domänengeneration 3, Bindung 17 (= Handoff-Sequenz); Credential-Record vorhanden (Sequenz 82; Payload nicht ausgewertet).
- Netzwerk-Credential `cc0`: **Schema V2** (mit SoftAP-Passwort), Sequenz 2.
- Run-Persistenz: Head **Committed**, beide Checkpoint-Slots **NoActiveRun** → kein persistierter Lauf.
- Alle Bedingungen aus [BOOT_WRITE_PATHS](PR209_Q4_HRAM2_BOOT_WRITE_PATHS_20261024.md) für Schreibpfade beim Boot (leere Partition/fehlender Namespace, Bootstrap `Empty`/`Initializing`/`Resetting`, Handoff `None`/`Unconsumed`/`InProgress`, fehlendes/v1-Credential, NVS-Reste) sind für dieses Abbild **nicht erfüllt** (offline belegt). Das ist **keine** Aussage über Byte-Unveränderlichkeit eines Boots.

### 3.3 PHY-Konfiguration/-Bibliothek: installiertes Image gegen die vorgesehene Diagnoseimage-Konfiguration
| Prüfpunkt | Befund |
|---|---|
| IDF-Commit | Vertrag fixiert `fff9895c82d744c7237be8847347bdd1b07c6643` (seit `ada7097`, vor `a9a1156`); zwischen `a9a1156` und dem Produktstand `80bfb28` kein Diff an `docs/ESP_IDF_UPGRADE_CONTRACT.md`/`scripts/esp_idf_contract.py` |
| PHY-Konfiguration | `git diff a9a1156 80bfb28` an `sdkconfig*`, `CMakeLists.txt`, `main/CMakeLists.txt`, `partitions/`, `main/idf_component.yml`: leer; Kandidat: `CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE=1`, `RF_CAL_PARTIAL`, `CALIBRATION_MODE=0`; Diagnoseimages ändern nur den Overlay-Code (Plan §15.6.3) |
| PHY-Bibliothek | installierte App (`a9a1156`) und Build des Produktstands `80bfb28` enthalten dieselbe PHY-Versionsinformation (`14:30:06`, `Oct 28 2025`, `a3a4459`; Kontextbytes identisch); `cal_version` 4863 passt zur `phy_version 4863,a3a4459,Oct 28 2025,14:30:06` aus den vorhandenen Bootlogs (`R1_RAM_*_RAW.txt`) |
| Stabilität über Boots | Boot-Bereich vor und nach H-RAM-1 hash-gleich (mehrere Boots zweier Images): die Kalibrierprüfung hat dort nicht neu gespeichert |
| Nicht belegt | der von der PHY-Bibliothek des **noch nicht gebauten** Diagnoseimages berechnete `phy_get_rf_cal_version()`-Wert wurde nicht ausgeführt, sondern aus Bibliotheksgleichheit und dem stabilen Bootverhalten geschlossen |

## 4. `PRE_P1_REPORT`
```text
P0_BYTE_IDENTICAL=YES                        (3 Bereiche, je 2 unabhängige Lesungen, SHA-256 oben)
STATE_STORE_BOOT_WRITE_CONDITIONS_PRESENT=NO (offline, aus dem Abbild; Gerät/Byte-Unveränderlichkeit damit nicht bewiesen)
SYSTEM_NVS_WRITE_AT_P1=EXCLUDED
```
**Begründung (alle vier Planbedingungen §15.6.5a belegt):** (1) `nvs` integer, ohne Reste, nicht leer; (2) `phy`-Kaldaten gültig, `cal_mac` = Chip-MAC;
(3) PHY-Bibliothek/Kalibrierformat von installiertem Image und Produktstand gleich (gleicher IDF-Pin, identische PHY-Versionsstrings, stabile NVS über frühere Boots);
(4) PHY-Konfiguration unverändert (keine Konfigurationsdiffs).
**Voraussetzungen/Restunsicherheit, ausdrücklich benannt:**
- Gilt nur für Diagnoseimages, die mit demselben IDF-Pin und derselben `sdkconfig`-Basis gebaut werden; beim Bau ist die PHY-Versionsinformation und die Konfiguration erneut zu vergleichen (sonst `NOT_PROVEN`).
- Nicht untersuchte IDF-Interna und der konkrete `phy_get_rf_cal_version()`-Wert des Diagnoseimages sind nicht ausgeführt belegt (Schluss aus Gleichheit).
- Die Aussage ist ein Quellen-/Abbildbefund und ersetzt weder `G-S` noch die Hashkette: Nach P1 bleibt der `nvs`-/Boot-Bereich-Hash gegen `P0` zu prüfen (Boot-Bereich-Hash oben); bei Delta Stopp.
- `EXCLUDED` ist **keine** Freigabe von P1.

## 5. Zustand nach dem Preflight und offene Punkte
- Das Gerät wurde mit `--after no-reset` im ROM-Bootloader belassen; es wurde **kein** Produkt-Firmwarestart ausgelöst. Der Rückgang in den Normalbetrieb (Reset/Power-Cycle) ist ein regulärer Boot der installierten Firmware und liegt beim Owner.
- Weiterhin gesperrt: `P1=HOLD`, `G-F`, `G-S`, `H-RAM-2`, `G-R7`, `G-RUN`, `C5_RESOURCE_GATE=HOLD`, `PR209_MERGE=HOLD`, `ACTUATOR_RELEASE=NO`.
- Nächster Schritt: unabhängige Prüfung dieses Berichts; danach `H-RAM-2` (K0+K1) mit `G-F` und `G-S` auf der dann aktuellen Plan-SHA zur ausdrücklichen Freigabe vorlegen.
