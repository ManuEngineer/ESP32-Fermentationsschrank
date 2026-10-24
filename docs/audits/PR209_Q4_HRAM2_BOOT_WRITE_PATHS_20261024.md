# PR #209 / H-RAM-2 – Boot-Schreibpfade bis zum stabilen Idle (statische Quellenprüfung)

Ergänzt [Plan §15.6.5](../tasks/issue-27-r1-web-remaining-scope-plan.md). Basis HEAD `1202ba1688e2a23fb2cc4cee34e55eb1f17f2709`,
Produktstand `80bfb28`, ESP-IDF `fff9895c82d744c7237be8847347bdd1b07c6643` (`components/...` unten sind IDF-Pfade).
**Nur Quellenprüfung; keine Messung, kein Build, kein Gerätezugriff.** Ein geöffneter Store ist kein Beleg für
Byte-Unveränderlichkeit; ein möglicher Schreibpfad ist kein Beleg einer tatsächlichen Änderung.

## 1. Bootpfad und seine Schreibmöglichkeiten

Reihenfolge in `app_main` ([app_main.cpp](../../main/app_main.cpp)): Ausgänge → `nvs_flash_init()` (Z. 629, Standardpartition `nvs`) →
`NvsOwningContext::create()` (Z. 147–190: `nvs_flash_init_partition("state_store")`, `NvsStateStore::open` mit `NVS_READWRITE`) → Zeit/SNTP →
`platform.begin()` → `application.begin()` (Konfigurations-Recovery, Auth-Handoff, Netzwerk-Credentials, Run-Persistenz) → HTTPD/WLAN → Hauptschleife.

| # | Operation | Quelle | Partition | Schreibt nur wenn … | Steady-State (initialisiert, bereitgestellt, keine Reste) |
|---|---|---|---|---|---|
| 1 | `nvs_flash_init()` Standard-NVS | `app_main.cpp:629`; IDF `nvs_flash/src/nvs_pagemanager.cpp:12` (`PageManager::load`), `nvs_storage.cpp:183` (`Storage::init`) | `nvs` | Partition leer (erste Seite wird aktiviert); Duplikat-Items nach unterbrochenem Schreiben; Seite im Zustand `FREEING` (unterbrochene GC); verwaiste/inkonsistente Blob-Indizes (`eraseMismatchedBlobIndexes`, `eraseOrphanDataBlobs`, Z. 90/154) | kein Schreiben |
| 2 | `nvs_flash_init_partition("state_store")` | `app_main.cpp:158`; gleiche IDF-Pfade | `state_store` | wie 1 (inkl. leere Partition) | kein Schreiben |
| 3 | `NvsStateStore::open(NVS_READWRITE)` | `nvs_state_store.cpp:69` → `Storage::createOrOpenNamespace` (`nvs_storage.cpp:610`, Schreiben Z. 638) | `state_store` | Namespace `fermentation` fehlt (frische/gelöschte Partition) | kein Schreiben |
| 4 | PHY-Kalibrierung | IDF `esp_phy/src/phy_init.c:895–966` (`esp_phy_load_cal_and_init`; `CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE=1`, `RF_CAL_PARTIAL`) | `nvs` (Namespace `phy`) | Kaldaten fehlen/ungültig (`err != ESP_OK`) **oder** Prüfsumme/Version/MAC passt nicht (`ESP_CAL_DATA_CHECK_FAIL`) → `esp_phy_store_cal_data_to_nvs` (Z. 759, 848: `nvs_set_blob` ×2, `nvs_set_u32`, `nvs_commit`) | kein Schreiben, solange die gespeicherte Kalibrierung zu Chip-MAC und PHY-Bibliotheksversion passt |
| 5 | Konfigurations-Recovery `boot()` | `configuration_recovery_service.cpp:946–1100` | `state_store` | Bootstrap-Scan `Empty` (Werksinitialisierung, Z. 997ff), Zustand `Initializing`/`Resetting` (Finalisierung, Z. 985ff/1119ff), offene `pendingRoot_` | Zustand `Initialized`: nur `loadCanonicalGraph` (Lesen), Publikation im RAM (Z. 1058–1094) |
| 6 | Auth-Bootstrap-Auflösung | `configuration_recovery_service.cpp:316–470` (`resolveAuthenticationBootstrap`), `fermentation_application.cpp:2299–2341` (`inspect` = Lesen) | `state_store` | Auth-Domain-Handoff `None`/`Unconsumed`/`InProgress` (Z. 369–449: `writeAuthDomainHandoff`, `writeInitialRoot`) | Handoff `Consumed`: nur Lesen |
| 7 | Netzwerk-Credentials | `network_configuration_service.cpp:59–101` (`ensureCredential`), `fermentation_application.cpp:882–960` | `state_store` | Credential fehlt/andere Epoche, oder Schema v1 ohne SoftAP-Passwort (Migration) → `writeNewCredential` | Schema v2 mit SoftAP-Passwort: nur Lesen |
| 8 | Run-Persistenz laden | `run_persistence_coordinator.cpp:772` (`loadAndInitializeInto`: nur `readHead`/`readSlot`) | `state_store` | nur Lesen; Schreiben erst bei Kommandos/Transitionen eines Laufs | kein Schreiben (kein persistierter Lauf) |
| 9 | Lesepfad-Reparatur | `nvs_get_blob → Storage::readMultiPageBlob → eraseMultiPageBlob` (Pfad in [STACK.tsv](PR209_Q4_RESOURCE_QUALIFICATION_20261023_STACK.tsv)) | `state_store` | nur bei beschädigten Blob-Chunks | kein Schreiben |
| 10 | WLAN-/Netif-Initialisierung | `esp_idf_network_lifecycle.cpp:137–146` (`init.nvs_enable = 0`, `esp_wifi_set_storage(WIFI_STORAGE_RAM)`) | – | schreibt keine WLAN-Konfiguration nach NVS | kein Schreiben |
| 11 | Login / authentifizierte Pfade | `authentication_records.cpp:832–888` | `state_store` | **jeder erfolgreiche Login** (`recordSequence + 1`, `writeCredentials`) | schreibt immer (siehe Plan §15.6.1 Zeile 2) – nicht Teil des Boots |

Nicht gefunden: ein Boot-Zähler, ein Epochen-Inkrement pro Boot oder ein periodisches Idle-Schreiben im Bootpfad
(`kNormalBootRevisionHeadroom` ist eine RAM-Revisionsprüfung). Bootloader/Partitionstabelle: nur Lesen (einzelne `factory`-App, keine `otadata`).

## 2. Was belegt ist und was offen bleibt

| Aussage | Status |
|---|---|
| Im Steady-State (Bootstrap `Initialized`, Auth-Handoff `Consumed`, Credential v2, valide PHY-Kalibrierung, keine NVS-Reste) enthält der Code **keinen** Schreibpfad bis zum stabilen Idle | belegt durch Code (Zeilen oben) |
| Der **tatsächliche Zustand dieses Geräts** erfüllt diese Vorbedingungen | **nicht belegt** (kein Gerätezugriff; ohne lesendes `P0` nicht entscheidbar) |
| NVS-Interna (Reste unterbrochener Schreibvorgänge früherer Tests/Resets, `FREEING`-Seiten, Blob-Reste) | **nicht aus Code allein entscheidbar**; nur aus dem `P0`-Abbild klassifizierbar |
| PHY-Kaldaten passen zur PHY-Bibliotheksversion des Diagnoseimages | **nicht belegt** (Diagnoseimage und installierte Firmware stammen vermutlich aus derselben IDF-Linie; Versionsgleichheit nicht geprüft) |
| Ein Hash-Gleichstand nach P1 | beweist nur diesen Boot unter diesem Zustand, keine pauschale Schreibfreiheit |

## 3. Entscheidung

```text
P1_BOOT_WRITES_STATE_STORE=CONDITIONAL
  (Code: im Steady-State nein; ja bei leerer Partition/fehlendem Namespace, Bootstrap Empty/Initializing/Resetting,
   Auth-Handoff None/Unconsumed/InProgress, fehlendem/v1-Credential, NVS-Resten. Gerätezustand NOT_PROVEN.)
P1_BOOT_WRITES_NVS_PHY=CONDITIONAL
  (nvs/phy_init: PHY-Kaldaten fehlen/ungültig/MAC- oder Versionsabweichung, NVS-Reste in `nvs`.)
K0_WITHOUT_G_S=NOT_ELIGIBLE   (tragfähiger Vor-Flash-Nachweis liegt nicht vor; erster Diagnoseflash mit P1/B0 ohne
                               vorbereiteten byte-exakten Rückfall ist nicht zulässig → HOLD vor dem Flash)
```

Ein Vor-Flash-Nachweis wäre nur das offline klassifizierte `P0`-Abbild (`nvs_tool.py`/`nvs_check.py` aus IDF `components/nvs_flash/nvs_partition_tool`,
lesend), und selbst dann bleiben Unbekannte (siehe 2). Deshalb wird `G-S` als Voraussetzung ab dem ersten Diagnoseflash geplant; es gibt keine
rückwirkende Genehmigung für einen erst nach P1 festgestellten Schreibzugriff.
