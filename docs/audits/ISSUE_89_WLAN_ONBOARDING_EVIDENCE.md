# Issue #89 – Phase-A-Reuse-/Capability-Evidence

Dieser Bericht dokumentiert die historische ESP-IDF-6.0.2-Baseline, die
separat aufgezeichnete autorisierte Phase-A-Revalidierung auf ESP-IDF 6.1 und
die abgeschlossene proportionale Phase-B-Kandidatenevaluation. Die 6.0.2-
Evidence wird nicht nachtraeglich umetikettiert. Die vorhandene Android-/Linux-
Evidence und die aktuellen Owner-Waiver reichen fuer das minimale
Owner-Kandidatengate; der Bericht ist weiterhin keine Produktivauswahl.

## Historische 6.0.2-Baseline

```text
ISSUE=89
PLAN_SHA=d8d506da1d5bde129c09d623263d7657c38f28a3
BASE=main@2c010e8a8be8e351f89b79ae6c74f665d24a1f0e
SCOPE=PHASE_A_CAPABILITY_EVIDENCE_ONLY
PHASE_A_CAPABILITY_EVIDENCE=PASS
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PENDING
OWNER_CANDIDATE_SELECTION_GATE=NOT_READY
PRODUCTIVE_IMPLEMENTATION=NOT_STARTED
PRODUCTIVE_CANDIDATE=OWNER_PENDING
BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=OWNER_GATE_PENDING
PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED
ACTUATOR_RELEASE=NO
```

## Aktuelle Phase-A-Revalidierung auf ESP-IDF 6.1

Die offiziellen und direkten 6.1-Ergebnisse der Phase-A-Revalidierung wurden
auf dem Quellstand `0b0d125379a5fef79ca2760a101603c831dc7e7e` ausgefuehrt. Der
native HTTP-Probe wurde nach dem Fail-Closed-/RAM-Storage-Fix auf dem
Quellstand `aa0d231e9b97cfe489b69b27d0b1ab5dcd28c775` neu gebaut. Die aktuelle
Phase-B-Firmware mit lokalem Credential-Testweg und Start-Ressourcenlogging
stammt aus `7d68c66589ffffe2ca943eb3583189207b8fdd87`; der lokale Credential-
Wert wurde nicht aufgezeichnet und die ungetrackte Eingabedatei nach dem Build
entfernt. Historische Source-SHAs bleiben die Provenienz ihrer jeweiligen
Evidence.

```text
ISSUE=89
PLAN_SHA=74474268391b47718aa3c751d16a0d5e815efc5c
PARENT_APPROVED_PLAN_SHA=5c582aa179cd6e382dc4442a6824bb79a1e2b22f
OWNER_DECISION_BASE_HEAD=42a495d7139d9810086d5be77f81b2fccf3fc949
PHASE_B_TEST_RUN_HEAD=7d68c66589ffffe2ca943eb3583189207b8fdd87
BASE=main@7029df3997bb92e60379eb218f1894f86c5f7d55
EVIDENCE_SOURCE_SHA=aa0d231e9b97cfe489b69b27d0b1ab5dcd28c775
PREVIOUS_PHASE_A_EVIDENCE_SOURCE_SHA=0b0d125379a5fef79ca2760a101603c831dc7e7e
CURRENT_ESP_IDF=v6.1@fff9895c82d744c7237be8847347bdd1b07c6643
ESP_IDF_CHECKOUT=clean
ESP_IDF_PYTHON=3.13.5
ESPTOOL=5.4.0
XTENSA_ESP_ELF_GCC=15.2.0
ESP_CLANG=21.1.3
TARGET=esp32
FLASH=4MB
PSRAM=NONE
SCOPE=PHASE_A_CAPABILITY_EVIDENCE_ONLY
PHASE_A_6_1_REVALIDATION=PASS
PHASE_B_TEST_SETUP=OWNER_AUTHORIZED
DEV_BOARD_DISPOSABLE=YES
FLASH_OVERWRITE_ALLOWED=YES
NVS_ERASE_ALLOWED_FOR_TEST=YES
POWER_CUT_TESTS=WAIVED_BY_OWNER
PHASE_B_FLASH_BOOT_EVIDENCE=PASS
PHASE_B_CLIENT_EVIDENCE=PASS_MINIMAL_PROPORTIONAL_WITH_CANDIDATE_GAPS
ANDROID_CLIENT_EVIDENCE=PASS
ANDROID_DIRECT_IP_TEST=PASS
ANDROID_CAPTIVE_PORTAL_AUTO_OPEN=NOT_OBSERVED
OFFICIAL_NETWORK_PROVISIONING_SOFTAP=PASS
OFFICIAL_BROWSER_R1_CONTRACT=GAP
DIRECT_PROTOCOMM_SOFTAP=PASS
DIRECT_PROTOCOMM_BROWSER_R1_CONTRACT=GAP
NATIVE_HTTP_SOFTAP=PASS
NATIVE_HTTP_BROWSER_TRANSPORT=PASS
IOS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
WINDOWS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
PHYSICAL_DISPLAY_QR_TEST=DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PASS_MINIMAL_PROPORTIONAL
OWNER_CANDIDATE_SELECTION_GATE=COMPLETED
CANDIDATE_SELECTION=NATIVE_ESP_IDF_HTTP
OWNER_CANDIDATE_SELECTION=COMPLETED
R1_AP_ONLY=YES
R1_HOME_WIFI=YES
R1_HOME_WIFI_COUNT=1
CAPTIVE_PORTAL_REQUIRED=NO
WIFI_CREDENTIAL_OWNER=PROJECT_CONFIGURATION_DOMAIN
TEST_BEFORE_COMMIT=YES
SECOND_CREDENTIAL_STORE=NO
R1_NETWORK_SCOPE_SYNC=PASS
FUTURE_SCOPE_ISSUE=163
R1_IMPLEMENTATION_ISSUE=164
PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED
ACTUATOR_RELEASE=NO
```

### A1 – kandidatenneutrale Integritaet

| Nachweis | Ergebnis | Befehl / Befund |
|---|---|---|
| Host-Oracle | PASS | `python3 spikes/issue89_wlan_onboarding/host_contract_test.py`; 5/5 Tests fuer Bytegrenzen, Commitgrenze, Redaction und QR-Escaping |
| Secret-Scan | PASS | `python3 scripts/check_secrets.py`; 482 getrackte Dateien, keine geschuetzten Dateien oder Geheimnismuster; der Standardlauf prueft keine privaten Pfade |
| Portable-Reproduktionspfad | PASS | `grep -nE '/(home|Users|var/lib|srv)/' spikes/issue89_wlan_onboarding/README.md`; kein Treffer; Reproduktion verwendet `IDF_PATH` und `IDF_TOOLS_PATH` |
| Repository-Integritaet | PASS | `git diff --check origin/main...HEAD`; keine Whitespace-Fehler; nur Roadmap, Plan/Evidence und isolierte Spike-Artefakte im PR-Scope |
| Produktionsgraph | PASS | keine `network_provisioning`-, `protocomm`-, `esp_http_server`- oder `nvs_flash_erase`-Referenz in Root-/Produktionsquellen |
| Probegrenzen | PASS | keine produktive Credential-Domaene, keine Connectivity-Persistenz und keine Aktorfreigabe; `WIFI_STORAGE_RAM` wird im direkten Protocomm- und nativen HTTP-Probe vor `esp_wifi_set_config()` gesetzt |
| Toolchain-Provenienz | PASS | sauberer Checkout `ESP-IDF v6.1` am exakten Commit; `idf.py --version`, `esptool v5.4.0`, GCC 15.2.0 und esp-clang 21.1.3 verifiziert |

### A2 bis A4 – aktuelle 6.1-Build-Evidence

Alle drei Builds liefen mit eigenem ignoriertem Buildverzeichnis und dem
kanonischen ESP-IDF-6.1-Checkout. Die Prozentwerte und Speicherwerte stammen
aus `idf.py size`; die historische Groesse ist die im alten Bericht
aufgezeichnete `.bin`-Groesse. Es wird keine neue feste Budgetgrenze aus dem
Delta abgeleitet.

| Kandidat / Ergebnis | Befehl | App `.bin` / Delta zu 6.0.2 | Partition frei | IRAM frei | DRAM frei | App-Binary-SHA256 |
|---|---|---:|---:|---:|---:|---|
| offizieller `network_provisioning`-Probe: PASS | `idf.py -C spikes/issue89_wlan_onboarding/official_network_provisioning -B build/issue89_official_network_provisioning_idf61 build` | 909424 B / +14512 B (historisch 894912 B) | 139152 B / 13 % | 43585 B | 143297 B | `bb5e0fb66dc970436c4050b30d3efd318375dffa6bdea3fbd9cfc1fd7768108a` |
| direkter Protocomm-Probe: PASS | `idf.py -C spikes/issue89_wlan_onboarding/direct_protocomm -B build/issue89_direct_protocomm_idf61 build` | 838128 B / +12416 B (historisch 825712 B) | 210448 B / 20 % | 43585 B | 143369 B | `253d4ec2d9da56b4925c70cd9fdb268d83b46329cd82f68acac5a923e0ea600a` |
| nativer HTTP-Probe: PASS | `idf.py -C spikes/issue89_wlan_onboarding/native_http_adapter -B build/issue89_native_http_adapter_idf61 build` | 815488 B / +22588 B (historisch 792900 B); `idf.py size` Gesamtbild 815372 B | 233088 B / 22 % | 43585 B | 143393 B | `b2b971ff0c7a46d503f1e53f27a533b6cdebb47d93819edbe52833b310dde54c` |

Der korrigierte native HTTP-Probe verwendet `nvs_flash_init()` fail-closed:
bei jedem Initialisierungsfehler wird weder `nvs_flash_erase()` aufgerufen noch
eine Konfiguration weiterverwendet. Unmittelbar nach `esp_wifi_init()` wird
`esp_wifi_set_storage(WIFI_STORAGE_RAM)` gesetzt; erst danach darf der Probe
`esp_wifi_set_config(WIFI_IF_AP, ...)` aufrufen. Die zusaetzliche
Komponentenabhaengigkeit `nvs_flash` ist ausschliesslich fuer diese
Initialisierung deklariert.

```text
NATIVE_HTTP_SOURCE_SHA=aa0d231e9b97cfe489b69b27d0b1ab5dcd28c775
NATIVE_HTTP_APP_BIN=815488
NATIVE_HTTP_APP_BIN_SHA256=b2b971ff0c7a46d503f1e53f27a533b6cdebb47d93819edbe52833b310dde54c
NATIVE_HTTP_APP_ELF_SHA256=776830e23ca35f2fc05897984f6d445a35a744a555092fea43ac36d71927f472
NATIVE_HTTP_TOTAL_IMAGE_SIZE=815372
NATIVE_HTTP_PARTITION_FREE=233088
NATIVE_HTTP_IRAM_FREE=43585
NATIVE_HTTP_DRAM_FREE=143393
NATIVE_HTTP_BOOTLOADER_BIN_SHA256=64a25a4d64fc7d1116cd7cb3c385cf37d66658d12c42c103219eb619c997dafc
NATIVE_HTTP_PARTITION_TABLE_BIN_SHA256=7f00b6c042a89b15b0cac534f82ed988caf29278ff5700b0c511eb1b5bb7c820
```

Die gemeinsame 6.1-Provenienz der drei Buildartefakte ist:

```text
BOOTLOADER_BIN_SHA256=64a25a4d64fc7d1116cd7cb3c385cf37d66658d12c42c103219eb619c997dafc
PARTITION_TABLE_BIN_SHA256=7f00b6c042a89b15b0cac534f82ed988caf29278ff5700b0c511eb1b5bb7c820
PARTITION_MODE=PARTITION_TABLE_SINGLE_APP
APP_PARTITION=0x100000
```

Der offizielle Component-Manager-Lauf wurde mit
`idf.py -C spikes/issue89_wlan_onboarding/official_network_provisioning
-B build/issue89_official_network_provisioning_idf61 update-dependencies`
erneut aufgeloest. Die daraus bestaetigte Lockfile-Aufloesung ist:

| Dependency | Version | Component-Hash / Herkunft |
|---|---|---|
| `idf` | 6.1.0 | exakter lokaler ESP-IDF-Checkout `fff9895c82d744c7237be8847347bdd1b07c6643` |
| `espressif/network_provisioning` | 1.2.4 | `72d27784e3daf807418a34fb00be136ec50c6db49d989ce981d22e031fc0e7f8` |
| `espressif/cjson` | 1.7.19~2 | `e788323270d90738662d66fffa910bfe1fba019bba087f01557e70c40485b469` |

`dependencies.lock` wurde durch den 6.1-Resolve unveraendert bestaetigt;
`manifest_hash=068c79db5865d491673373872e8a40ddcb0bcc8ac0d5b95b8750828d2a96dc64`.
Der direkte Probe verwendet die eingebauten 6.1-Komponenten
`protocomm`, `protobuf-c`, `esp_http_server`, `esp_wifi`, `esp_netif` und
`nvs_flash`; der native Probe verwendet `esp_http_server`, `esp_wifi`,
`esp_netif`, `esp_event` und `nvs_flash`. Kein dieser Pfade wurde in den
Produktionsgraphen uebernommen.

### A5 – aktueller Kandidatenstatus

Die fachliche Vergleichslogik, vier Kandidaten und Owner-Gates bleiben
unveraendert. Nur die unter 6.1 ausgefuehrten Capability-Eigenschaften sind
aktuell als `PASS` bezeichnet:

| Kandidat | Aktueller 6.1-Status | Weiterhin offene R1-Frage |
|---|---|---|
| `espressif/network_provisioning` 1.2.4 | `PASS` fuer Component-Aufloesung und actor-free 6.1-Build | kein Browser-Portalnachweis; native Set/Apply-Persistenz und Fehler-/Recovery-Semantik bleiben offen |
| direkter `protocomm`-/ESP-IDF-Pfad | `PASS` fuer 6.1-Build sowie Security-/Versions-/Set-/Test-/Commit-Handlergrenzen | kein Browser-, DNS-, Scan-, Reconnect- oder Recovery-Nachweis; Handler bleiben Boundary-only |
| kleiner nativer ESP-IDF-Adapter | `PASS` fuer 6.1-SoftAP-/direkte-IP-HTTP-Capability | DNS/Captive Portal, Scan, Reconnect, Persistenz, Recovery und Commit sind nicht implementiert |
| WiFiManager v2.0.17 | `CAPABILITY_NOT_PRESENT_FOR_CURRENT_NATIVE_IDF_GATE` | aktueller Tag ist ein Arduino-/PlatformIO-Kandidat; der vorhandene CMake-Pfad verlangt `arduino`; kein nativer ESP-IDF-6.1-Pfad ohne Arduino-Produktionsframework |

### A5.1 – aktueller Source-/Manifest-/Lizenzscreen

Der folgende Screen wurde gegen die aktuellen oeffentlichen Quellen und deren
exakte Tags beziehungsweise HEAD-SHAs erstellt. `SOURCE_SCREEN=PASS` bedeutet
vollstaendige Quellen-/Manifest-/Lizenzpruefung, nicht Build-, Client- oder
Produktionsfreigabe. Nicht in diesem Delta gebaute Drittanbieter bleiben
`BUILD=NOT_RUN`.

Wartung und Ressourcen bleiben ebenfalls getrennt bewertet: Die aktuellen
Ressourcenwerte der drei ESP-IDF-Spikes stehen in A2 bis A4; fuer WiFiManager
und die drei Zusatzkomponenten ist ein Build `NOT_RUN`. Die
Wartungsverantwortung bleibt jeweils beim upstream beziehungsweise beim
Owner-entscheid fuer einen spaeteren Produktionspfad; aus diesem Screen folgt
kein Shortlisting.

| Kandidat | Quelle, Version und Lizenz | IDF-/Arduino-Annahme und Dependencies | Storage, Lifecycle, Reset und Integrationsrisiko | Status |
|---|---|---|---|---|
| `espressif/network_provisioning` | [`espressif/network_provisioning@1.2.4`](https://components.espressif.com/components/espressif/network_provisioning), Component-Hash `72d27784e3daf807418a34fb00be136ec50c6db49d989ce981d22e031fc0e7f8`; Apache-2.0-Komponente, isolierter Probe CC0-1.0 | Managed ESP-IDF-Komponente, Manifest `idf >=5.1`, fuer IDF >=6.0 `espressif/cjson ^1.7.19`; kein Arduino; 6.1-Lockfile loest `idf 6.1.0`, `cjson 1.7.19~2` auf | Standard-Protocomm-/HTTP-SoftAP-Manager; native WiFi-/NVS-Zustands- und Set/Apply-Semantik bleibt Bibliotheksbesitz; der actor-free Build-only-Probe fuehrt keinen Client-Commit aus und loescht nichts; Browser-/HTTPD-Sharing-, #57-, Reset- und Recovery-Vertrag offen | `SOURCE_SCREEN=PASS`; `6.1_RESOLVE_BUILD=PASS`; kein Browser-/Recovery-PASS |
| direkter `protocomm`-/ESP-IDF-Pfad | ESP-IDF-Built-ins am exakten `v6.1@fff9895c82d744c7237be8847347bdd1b07c6643`; ESP-IDF Apache-2.0, isolierter Probe CC0-1.0 | ESP-IDF-only ohne Arduino; `protocomm`, `protobuf-c`, `esp_http_server`, `esp_wifi`, `esp_netif`, `esp_event`, `nvs_flash`; der Probe registriert eigene HTTPD-/Protocomm-Endpunkte | `WIFI_STORAGE_RAM` und fail-closed NVS-Init; Set/Test/Commit-Handler sind Boundary-only und parsen, wenden oder persistieren keine Credentials; Stop-/Reset-/Recovery- und Browservertrag fehlen; eigenes HTTPD-/Security-/Endpoint-Sharing waere Integrationsrisiko | `SOURCE_SCREEN=PASS`; `6.1_BUILD=PASS`; `BUILD=actor-free`, kein Client-PASS |
| kleiner nativer ESP-IDF-Adapter | Probequelle `aa0d231e9b97cfe489b69b27d0b1ab5dcd28c775`, CC0-1.0; ESP-IDF v6.1-Built-ins Apache-2.0 | ESP-IDF-only ohne Arduino; `esp_wifi`, `esp_netif`, `esp_event`, `esp_http_server`; `nvs_flash` nur fuer `nvs_flash_init()`; keine externe Managed-Dependency | fail-closed NVS-Init ohne Erase, `WIFI_STORAGE_RAM` vor `esp_wifi_set_config()`, direkter HTTPD-Transport ohne Server-Sharing; keine Credential-Commit-, DNS-, Scan-, Reconnect- oder Recovery-Semantik; spaetere produktive Integration muesste HTTP-/Lifecycle-/#57-Vertraege erst ownerfreigeben | `SOURCE_SCREEN=PASS`; `6.1_BUILD=PASS`; kein produktiver Adapter |
| WiFiManager | [`tzapu/WiFiManager@v2.0.17`](https://github.com/tzapu/WiFiManager/tree/v2.0.17), Commit `d82d0a1b9fca741b9ec44accdf553606a6576dda`; MIT (`LICENSE`) | `library.json`/`library.properties` deklarieren Arduino; `CMakeLists.txt` hat `PRIV_REQUIRES arduino`; kein `idf_component.yml`; README nennt ESP8266-/ESP32-Arduino und PlatformIO | `autoConnect()` startet AP-/DNS-/Webportal und speichert ueber den Arduino-WiFi-Laufzeitpfad; `resetSettings()` und Portal-Timeout sind eigene Bibliothekssemantik; kein #57-kompatibler Storage-/Recoveryvertrag belegt | `SOURCE_SCREEN=PASS`; `BUILD=NOT_RUN`; `CAPABILITY_NOT_PRESENT_FOR_CURRENT_NATIVE_IDF_GATE` |
| `thorrak/esp_wifi_config` | [`WiFiConfig/esp_wifi_config@32c78805e9fc206610b7debe31d06638cbe5da09`](https://github.com/thorrak/esp_wifi_config/tree/32c78805e9fc206610b7debe31d06638cbe5da09), Version `0.4.0`; MIT | `idf_component.yml`: `idf >=5.4`, `espressif/network_provisioning ^1.0.0` ab `idf_version >=6.0`; `library.json` nennt `espidf`/`arduino`, `library.properties` Arduino-ESP32 `3.3.11+`; CMake ist ESP-IDF-Komponente | NVS-basierte Mehrfachnetze, Auto-Reconnect und Portal; Reset-/Recovery- und Commitgrenzen sind eigene Bibliothekssemantik und nicht als #57-Vertrag nachgewiesen; zusaetzliche Storage-/HTTP-Lifecycle-Integration | `SOURCE_SCREEN=PASS`; `MANIFEST_DECLARATION_ACCEPTS_6_1=PASS`; `BUILD=NOT_RUN`; kein Shortlisting |
| `tuanpmt/esp_wifi_manager` | [`tuanpmt/esp_wifi_manager@20f77d79e9cdde9e4d3f0c3c7a3bd3babfaf893a`](https://github.com/tuanpmt/esp_wifi_manager/tree/20f77d79e9cdde9e4d3f0c3c7a3bd3babfaf893a), Version `1.1.0`; MIT | `idf_component.yml`: `idf >=5.0.0`, `tuanpmt/esp_bus ^1.0.3`, `espressif/mdns ^1.2`; CMake: `esp_wifi`, `esp_netif`, `nvs_flash`, `esp_http_server`, `esp_event`, `mdns` sowie private `esp_bus`, JSON und mbedTLS | NVS-Persistenz, SoftAP, REST, mDNS und Auto-Reconnect; eigener Reset-/Lifecycle-/Storagebesitz und HTTP-Handler-Sharing erzeugen Integrationsrisiko gegen #57 und die Recoverygrenze | `SOURCE_SCREEN=PASS`; `MANIFEST_DECLARATION_ACCEPTS_6_1=PASS`; `BUILD=NOT_RUN`; kein Shortlisting |
| `nordesems/esp-captive-portal` | [`nordesems/esp-captive-portal@b937ee88b86de47b40cd195f829cfd70e5af03c0`](https://github.com/nordesems/esp-captive-portal/tree/b937ee88b86de47b40cd195f829cfd70e5af03c0), Version `1.3.0`; MIT | `idf_component.yml`: `idf >=5.0.0`; CMake benoetigt `esp_event`, `esp_http_server`, `esp_netif`, `esp_wifi`, FreeRTOS, Log und lwIP; keine weitere externe Dependency | DNS-/DHCP-Option-114-Portal ohne Credential-Storage, Scan oder Reconnect; Lifecycle-/HTTPD-Handler-Reihenfolge und Kombination mit einem getrennten Owner-Transport bleiben Integrationsrisiko | `SOURCE_SCREEN=PASS`; `MANIFEST_DECLARATION_ACCEPTS_6_1=PASS`; `BUILD=NOT_RUN`; kein Shortlisting |

Die drei Zusatz-Screens wurden nur auf 6.1-relevante Manifest-/Dependency-
Aenderungen und die geforderten Lizenz-/Lifecycle-Felder revalidiert. Es gibt
keine zusaetzlichen Vollkandidaten, keine Auswahl und keinen Arduino-
Produktionspfad. WiFiManager bleibt fuer den nativen ESP-IDF-6.1-Spike
`CAPABILITY_NOT_PRESENT_FOR_CURRENT_NATIVE_IDF_GATE`.

### Phase-A-Grenzen und naechster Gate

Die neuen 6.1-Build-PASS und die drei realen Flash-/Boot-/DTR-/RTS-Reset-
Nachweise werden durch die vorhandene Android-/Linux-Client-Evidence ergaenzt.
Der physische Display-/Kamera-QR-Test ist
`PHYSICAL_DISPLAY_QR_TEST=DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION`; iOS/iPadOS
und Windows sind durch den Owner-Waiver vor dem Auswahlgate nicht erforderlich.
Power-Cut ist fuer Phase B durch den Owner als `WAIVED_BY_OWNER` festgelegt.
Der vorhandene ESP32-WROOM-32E-Dev-Aufbau ist fuer den kontrollierten
Issue-#89-Spike als entbehrlicher Testtraeger freigegeben. Ein zusaetzliches
Test-NVS, eine separate physische Testpartition oder ein Backup sind fuer
diesen Testtraeger nicht erforderlich. Die drei Flash-/Boot-/SoftAP-
Teilnachweise sind `PASS`; die vergleichbare Kandidatenmatrix ist fuer das
aktuelle Owner-Gate mit `PASS_MINIMAL_PROPORTIONAL` abgeschlossen. Der
Owner-Waiver fuer iOS/iPadOS und Windows sowie der deferred Display-/Kamera-
QR-Test bleiben unveraendert; ein zusaetzliches NVS-/Power-Cut-Owner-Gate ist
nicht offen.

## Aktueller Phase-B-Testaufbau nach Ownerentscheid

Der folgende Status ist die aktuelle Testgrenze nach dem Ownerentscheid. Die
drei autorisierten Flash-/Boot-/SoftAP-Laeufe sowie der owner-interaktive Lauf
mit einem Linux-Host und Android wurden fuer alle drei ausfuehrbaren
Kandidaten ausgefuehrt. Die Android-Evidence ist PASS;
iOS/iPadOS und Windows sind durch den Owner-Waiver vor dem Kandidatengate nicht
mehr verpflichtend. Kandidatenspezifische fehlende Funktionen bleiben
`CAPABILITY_NOT_PRESENT` beziehungsweise dokumentierte Produkt-/
Integrationsluecken. `Reset != Power-Cut` bleibt die technische
Begriffsgrenze; Power-Cut-Tests sind kein verpflichtendes Acceptance-Criterion.

```text
OWNER_DECISION_BASE_HEAD=42a495d7139d9810086d5be77f81b2fccf3fc949
PHASE_B_TEST_RUN_HEAD=7d68c66589ffffe2ca943eb3583189207b8fdd87
PHASE_B_TEST_SETUP=OWNER_AUTHORIZED
BOARD_FAMILY=esp32_32e_quad_mosfet
BOARD_MODULE=ESP32-WROOM-32E
DEV_BOARD_DISPOSABLE=YES
FLASH_OVERWRITE_ALLOWED=YES
NVS_ERASE_ALLOWED_FOR_TEST=YES
EXISTING_DEV_STATE_PRESERVATION_REQUIRED=NO
PRE_TEST_FLASH_BACKUP_REQUIRED=NO
UART_RESET_TESTS_ALLOWED=YES
FLASH_TESTS_ALLOWED=YES
REAL_CLIENT_TESTS_ALLOWED=YES
EFUSE_WRITE=NO
SECURE_BOOT_CHANGE=NO
FLASH_ENCRYPTION_CHANGE=NO
ROM_DOWNLOAD_MODE_DISABLE=NO
POWER_CUT_TESTS_REQUIRED=NO
POWER_CUT_TESTS=WAIVED_BY_OWNER
POWER_CUT_PATH=WAIVED_BY_OWNER
TEST_NVS=DEFAULT_NVS_ALLOWED_ON_DISPOSABLE_DEV_BOARD
UART_ACCESS=PASS_FOR_BOOTLOADER_HANDSHAKE
RESET_PATH=FT232R_DTR_RTS_DEFAULT_RESET
RESET_IS_POWER_CUT=NO
PROJECT_USER_NVS_TOUCH=NOT_RUN
FIRST_FLASH=PASS
FLASH_ERASE_AND_WRITE=PASS
CLIENT_MATRIX=PASS_MINIMAL_PROPORTIONAL_WITH_CANDIDATE_GAPS
PHASE_B_FLASH_BOOT_EVIDENCE=PASS
PHASE_B_CLIENT_EVIDENCE=PASS_MINIMAL_PROPORTIONAL_WITH_CANDIDATE_GAPS
ANDROID_CLIENT_EVIDENCE=PASS
ANDROID_DIRECT_IP_TEST=PASS
ANDROID_CAPTIVE_PORTAL_AUTO_OPEN=NOT_OBSERVED
OFFICIAL_NETWORK_PROVISIONING_SOFTAP=PASS
OFFICIAL_BROWSER_R1_CONTRACT=GAP
DIRECT_PROTOCOMM_SOFTAP=PASS
DIRECT_PROTOCOMM_BROWSER_R1_CONTRACT=GAP
NATIVE_HTTP_SOFTAP=PASS
NATIVE_HTTP_BROWSER_TRANSPORT=PASS
IOS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
WINDOWS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
PHYSICAL_DISPLAY_QR_TEST=DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PASS_MINIMAL_PROPORTIONAL
PHASE_B_LOCAL_TEST_CREDENTIAL=EPHEMERAL_UNTRACKED_OVERRIDE
PHASE_B_SECRET_REDACTION=PASS
FIRMWARE_SOURCE_SHA=7d68c66589ffffe2ca943eb3583189207b8fdd87
CURRENT_ESP_IDF=v6.1@fff9895c82d744c7237be8847347bdd1b07c6643
NEXT_GATE=OWNER_CANDIDATE_SELECTION_GATE
```

Die kontrollierte Freigabe gilt ausschliesslich fuer diesen ausdruecklich
freigegebenen Development-Testtraeger. Automatisches oder unbeabsichtigtes
Loeschen bleibt in Produktcode, Bibliotheks- und Recoveryvertraegen
unzulaessig. Linux-Host und Android wurden real getestet; iOS/iPadOS und
Windows sind vor dem Kandidatengate ownerseitig waived. Kandidatengaps bei
Captive/DNS, Scan/Form, Test/Commit und dem offiziellen Spezialclient bleiben
als `CAPABILITY_NOT_PRESENT` beziehungsweise Produkt-/Integrationsluecke
dokumentiert. UART-/DTR-/RTS-Reset wurde fuer alle drei geflashten Kandidaten
ausgefuehrt; ein Reset ist kein Power-Cut.

### Aktuelle Phase-B-Hardware- und Transport-Evidence

Der kontrollierte Ablauf loeschte vor jedem Kandidatenlauf den vollstaendigen
Flash des freigegebenen Development-Testtraegers und schrieb danach
Bootloader, Partitionstabelle und die jeweilige App. Alle drei
`write-flash`-Laeufe meldeten `Hash of data verified`. Es wurde kein Backup
angelegt und kein nicht freigegebenes Projekt-/Benutzer-NVS verwendet.

| Kandidat | Firmware-/Binary-Provenienz | Flash und Boot | SoftAP-/Transportbefund |
|---|---|---|---|
| `espressif/network_provisioning` 1.2.4 | Source `7d68c66589ffffe2ca943eb3583189207b8fdd87`; App `909712 B`; Binary-SHA `3c8a52004d26d0505fc7927ca7d27bb3eab657fbd019b16dc7ca6d6647c07f16` | Vollerase und Flash `PASS`; ESP-IDF-6.1-Boot/UART `PASS`; DTR/RTS-Monitor-Reset `PASS` | Geschuetzter Dienststart `PASS`; Test-SSIDs `R1SPK-84221C` und nach Reset `R1SPK-197EC9`; DHCP/AP-IP `192.168.4.1`; Linux/Android real verbunden |
| direkter `protocomm`-/ESP-IDF-Pfad | Source `7d68c66589ffffe2ca943eb3583189207b8fdd87`; App `838400 B`; Binary-SHA `30af8e93b5e5c7ef85b2acad0eb58a683d671078adfbb98f9e16d2d8f4449544` | Vollerase und Flash `PASS`; ESP-IDF-6.1-Boot/UART `PASS`; DTR/RTS-Monitor-Reset `PASS` | Geschuetzter SoftAP-Start `PASS`; Test-SSIDs `R1PC-B77A7F` und nach Reset `R1PC-6FA9FC`; Endpoint-Bind `PASS`; DHCP/AP-IP `192.168.4.1`; Linux/Android real verbunden |
| kleiner nativer ESP-IDF-SoftAP-/HTTP-Pfad | Source `7d68c66589ffffe2ca943eb3583189207b8fdd87`; App `815792 B`; Binary-SHA `3ed75740e05ab08b9efa4ad404bdc71b8150d61ecec602857f2e0246fbc7605a` | Vollerase und Flash `PASS`; ESP-IDF-6.1-Boot/UART `PASS`; DTR/RTS-Monitor-Reset `PASS` | Geschuetzter SoftAP-Start `PASS`; Test-SSIDs `R1NAT-34B205` und nach Reset `R1NAT-7D8FE3`; DHCP/AP-IP `192.168.4.1`; direkte HTTP-Seite und Linux/Android real verbunden |

Die zugehoerigen ESP-IDF-6.1-Build- und Laufzeitwerte des geflashten
Phase-B-Images sind:

| Kandidat | Gesamtbild | App-Partition frei | IRAM frei | DRAM frei | Start: Free Heap | Minimum | groesster Block | Stack-Watermark | ELF-SHA256 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `network_provisioning` 1.2.4 | `909600 B` (`.bin` `909712 B`) | `138864 B / 13 %` | `43585 B` | `143297 B` | `212024 B` / `211892 B` | `211620` / `211340` | `110592` / `110592` | `1916` / `1916` | `f18d64685522e0d522d08389542ccae40dfaf19e7a32fdab06cb537caf097817` |
| direkter Protocomm-Pfad | `838284 B` (`.bin` `838400 B`) | `210176 B / 20 %` | `43585 B` | `143369 B` | `215568 B` / `215568 B` | `215444` / `215444` | `110592` / `110592` | `2308` / `2308` | `eb2c6090654ee8fe52067ed3582518dccb0b7e8a5dd79140fb27b0cf4bb60346` |
| nativer HTTP-Pfad | `815672 B` (`.bin` `815792 B`) | `232784 B / 22 %` | `43585 B` | `143393 B` | `214820 B` / `214820 B` | `214696` / `214696` | `110592` / `110592` | `2332` / `2332` | `ab2c3474d567413867aa4a01bd55f6054dbd19453ada288eafb80178037e69cd` |

Alle drei Images wurden mit ESP-IDF v6.1 gebaut; die App-Binary-SHAs stehen
oben in der Kandidatentabelle. Die beiden Ressourcenwerte je Kandidat sind
`erster Start / Start nach DTR/RTS-Reset`; sie sind als Fruehindikator fuer
dieses Owner-Gate ausreichend. Eine vollstaendige Clientlast-Qualifikation
bleibt nach der Auswahl und produktiven Integration vorbehalten.

Die Schutzkonfigurationen sind in den drei Probequellen WPA2-geschuetzt. Fuer
den kontrollierten Lauf wurde der Passwortwert ueber die ungetrackte lokale
Build-Datei gesetzt, ohne ihn in Evidence oder Logs zu uebernehmen; die Datei
wurde danach entfernt. Die UART-Ausgaben redigieren den Wert. Eine
echte WLAN-Assoziation wurde fuer Linux-Host und Android ausgefuehrt. Ein
expliziter SoftAP-Stop-Lifecycle wurde in keinem Probe implementiert und ist
deshalb `NOT_RUN`; der DTR/RTS-Reset beendet jeweils die laufende Firmware und
startet sie mit einem neu erzeugten SoftAP erneut.

| Phase-B-Nachweis | Status | Aktueller Befund |
|---|---|---|
| Flash-Erase, App-/Bootloader-/Partition-Write und Hash-Verifikation | `PASS` | je Kandidat mit ESP-IDF-6.1/esptool 5.4.0 ausgefuehrt |
| Boot, UART und ESP-IDF-Provenienz | `PASS` | alle drei Logs zeigen ESP-IDF v6.1 und Firmware-Source `7d68c66589ffffe2ca943eb3583189207b8fdd87` |
| SoftAP-Start, Schutz und AP-IP-Ankuendigung | `PASS` | geschuetzter SoftAP; DHCP/AP-IP `192.168.4.1`; Secrets redigiert |
| SoftAP-Stop ohne Reset | `CAPABILITY_NOT_PRESENT` | kein Stop-Lifecycle im jeweiligen Probe vorhanden; keine Nachimplementierung vor der Auswahl |
| WLAN-Assoziation und geschuetzter Zugang | `PASS` fuer Linux-Host und Android | alle drei Kandidaten wurden mit je einem neuen temporaeren WPA2-Wert getestet; beide Geraete erhielten DHCP |
| direkte HTTP-/Protocomm-IP-Anfrage | `PASS` mit Kandidatengap | official und direct: Root `404 Nothing matches the given URI`; native: `200` und Setup-Seite |
| Captive Portal / DNS / Browser | `PASS_MINIMAL_PROPORTIONAL` | native direkter Browser `PASS`; official `BROWSER_R1_CONTRACT=GAP`; direct Root `404`; DNS/Captive in direct/native als `CAPABILITY_NOT_PRESENT` dokumentiert |
| WLAN-Scan und Credential-Eingabe | `CAPABILITY_NOT_PRESENT` / `DEFERRED_AFTER_SELECTION` | officialer Spezialclient nicht verfuegbar; direct/native haben diese Capability nicht; keine Nachimplementierung |
| falsches Passwort | `EXPECTED_FAIL` je Kandidat | interaktiver `nmcli`-Versuch mit falschem Wert lief in Timeout; korrekte Verbindung wurde anschliessend wiederhergestellt; explizite Protokoll-Auth-Ablehnung nicht separat beobachtet |
| Protokoll-Abbruch/Timeout, Test-vor-Commit, Commitgrenze | `DEFERRED_AFTER_SELECTION` / `CAPABILITY_NOT_PRESENT` | kein gueltiger `esp_prov`-/Protocomm-Spezialclient verfuegbar; Browser-POST ist kein Ersatz; keine Credentials an den offiziellen Manager gesendet; direct Handler bleiben Boundary-only |
| Host-Disconnect/Reconnect | `PASS` | je Kandidat `nmcli`-Down/Up erfolgreich; UART bestaetigt erneuten Join/DHCP |
| Firmware-Neustart getrennt vom DTR/RTS-Reset | `CAPABILITY_NOT_PRESENT` | kein eigener Restart-/Stop-Endpunkt im Probe |
| DTR/RTS-Reset | `PASS` | alle drei Kandidaten booteten danach erneut; `Reset != Power-Cut` |
| Recovery nach DTR/RTS-Reset | `PASS` fuer Transport-Recovery | alle drei Kandidaten erzeugten eine neue SSID; Host und Android wurden danach erneut verbunden; kein produktiver Credential-Recoverypfad |
| Credential-/NVS-/Recovery-Cut-Points | `DEFERRED_AFTER_SELECTION` | kein Set/Apply/Commit; Vollerase war kontrollierter Testaufbau; produktive Cutpoints folgen erst nach Integration |
| Laufzeit-Heap, Minimum, groesster Block und Stack-Watermark am Transportstart | `PASS` | belastbare Messzeile fuer alle drei geflashten Kandidaten; keine Clientlast |
| Handles, Leaks, Watchdog unter Clientlast und Jitter | `DEFERRED_AFTER_SELECTION` | keine weitere Clientlastqualifikation vor der Owner-Auswahl |
| Power-Cut-Tests | `WAIVED_BY_OWNER` | nicht verpflichtendes Acceptance-Criterion und kein offener Testpunkt |

Fehlende Capabilities bleiben als `CAPABILITY_NOT_PRESENT` beziehungsweise
dokumentierte Produkt-/Integrationsluecken erhalten: Der direkte Protocomm-
Probe bindet nur Boundary-Handler ohne Credentialinterpretation; der native
HTTP-Probe hat nur direkte HTTP-Seite; beide enthalten keinen DNS-/Captive-,
Scan-, Formular- oder Commitpfad. Der offizielle Manager startet seinen
Standardtransport, aber ein normaler Browser-GET/POST ist kein gueltiger
`esp_prov`-Client. Der Rootzugriff war `404`; deshalb ist
`BROWSER_R1_CONTRACT=GAP`. Ein Spezialclient war in der vorhandenen Umgebung
nicht verfuegbar und wurde nicht nachgebaut. Es wurde kein Wrapper und kein
Arduino-Produktionspfad eingefuehrt.

### Aktuelle Clientmatrix und QR-Grenze

Der Linux-Host wurde als realer WLAN-Client ueber `wlp1s0` verwendet; Android
wurde als zweiter realer Client verwendet. Die nachstehenden SSIDs sind keine
Passwoerter und dienen nur der Zuordnung der UART-/Hostnachweise. Die
temporaeren WPA2-Werte wurden weder hier noch in UART-/PR-Evidence
aufgezeichnet.

| Kandidat / Plattform | WLAN / AP-IP | Captive-Angebot | Browser / direkte IP | Formular / Scan | Falsches Passwort | Abbruch / Test / Commit | Reconnect | Neustart / DTR/RTS / Recovery | Runtime unter Last | Redaction |
|---|---|---|---|---|---|---|---|---|---|---|
| official / Linux-Host | `PASS` / `192.168.4.1` | `NOT_OBSERVED` | `404`; `BROWSER_R1_CONTRACT=GAP` | `NOT_RUN`; Spezialclient nicht verfuegbar | `EXPECTED_FAIL`; NM-Timeout | `NOT_RUN`; kein gueltiger `esp_prov`-Client | `PASS` | `NOT_RUN` / `PASS` / `PASS` | `NOT_RUN` | `PASS` |
| official / Android | `PASS` / `192.168.4.1` | `NOT_OBSERVED` | manueller Zugriff `404`; kein Browser-R1-Portal | `NOT_RUN`; kein App-/CLI-Spezialclient | `NOT_RUN` | `NOT_RUN`; keine Credential-Apply-Aktion | `PASS` | `NOT_RUN` / `PASS` / `PASS` | `NOT_RUN` | `PASS` |
| direct Protocomm / Linux-Host | `PASS` / `192.168.4.1` | `NOT_OBSERVED` | `404`; Browser-Capability-Gap | `GAP` | `EXPECTED_FAIL`; NM-Timeout | `NOT_RUN`; Handler Boundary-only | `PASS` | `NOT_RUN` / `PASS` / `PASS` | `NOT_RUN` | `PASS` |
| direct Protocomm / Android | `PASS` / `192.168.4.1` | `NOT_OBSERVED` | manueller Zugriff `404`; Browser-Capability-Gap | `GAP` | `NOT_RUN` | `NOT_RUN`; Handler Boundary-only | `PASS` | `NOT_RUN` / `PASS` / `PASS` | `NOT_RUN` | `PASS` |
| native HTTP / Linux-Host | `PASS` / `192.168.4.1` | `NOT_OBSERVED` | `200`; native Setup-Seite | `GAP` | `EXPECTED_FAIL`; NM-Timeout | `NOT_RUN`; kein Commitpfad | `PASS` | `NOT_RUN` / `PASS` / `PASS` | `NOT_RUN` | `PASS` |
| native HTTP / Android | `PASS` / `192.168.4.1` | `NOT_OBSERVED` | `200`; „Direct local setup transport“ | `GAP` | `NOT_RUN` | `NOT_RUN`; kein Commitpfad | `PASS` | `NOT_RUN` / `PASS` / `PASS` | `NOT_RUN` | `PASS` |
| official / iOS/iPadOS | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `DEFERRED_AFTER_SELECTION` | `PASS` |
| direct Protocomm / iOS/iPadOS | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `DEFERRED_AFTER_SELECTION` | `PASS` |
| native HTTP / iOS/iPadOS | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `DEFERRED_AFTER_SELECTION` | `PASS` |
| official / Windows | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `DEFERRED_AFTER_SELECTION` | `PASS` |
| direct Protocomm / Windows | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `DEFERRED_AFTER_SELECTION` | `PASS` |
| native HTTP / Windows | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `WAIVED_BY_OWNER` | `DEFERRED_AFTER_SELECTION` | `PASS` |

Damit gilt explizit:

```text
ANDROID_CLIENT_EVIDENCE=PASS
ANDROID_DIRECT_IP_TEST=PASS
ANDROID_CAPTIVE_PORTAL_AUTO_OPEN=NOT_OBSERVED
IOS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
WINDOWS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
PHYSICAL_DISPLAY_QR_TEST=DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PASS_MINIMAL_PROPORTIONAL
OWNER_CANDIDATE_SELECTION_GATE=COMPLETED
CANDIDATE_SELECTION=NATIVE_ESP_IDF_HTTP
OWNER_CANDIDATE_SELECTION=COMPLETED
```

Der physische QR-Scan ueber das spaetere Geraetedisplay ist
`PHYSICAL_DISPLAY_QR_TEST=DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION`.
Synthetisches QR-Oracle und die drei UART-/SoftAP-Nachweise bleiben gueltig;
der reale Display-/Kamera-Test wird erst mit angeschlossener Displayhardware
nachgeholt.

## Historischer Phase-B-Stop vor dem Ownerentscheid

Die folgenden Angaben sind der damalige konservative Status vor der
ausdruecklichen Ownerfreigabe. Sie bleiben als historische Evidence erhalten,
sind durch die Ownerfreigabe superseded und beschreiben nicht die aktuelle
Testgrenze.

Vor jedem Flash-/Clienttest wurde der folgende Aufbau festgehalten. Der
Chip-Handshake war nichtschreibend erfolgreich; ein Flash oder Clientlauf
wurde danach nicht gestartet, weil das Test-NVS und der Power-Cut-Pfad nicht
als isoliert beziehungsweise Owner-gesichert bestaetigt waren.

```text
PHASE_B_TEST_SETUP=SUPERSEDED_PRE_OWNER_STOP
BOARD_FAMILY=esp32_32e_quad_mosfet
BOARD_MODULE=ESP32-WROOM-32E
BOARD_REVISION=TBD_HARDWARE
CHIP_HANDSHAKE=PASS_NON_WRITING
CHIP_TYPE=ESP32-D0WD-V3_REVISION_v3.1
FLASH_ID=PASS_NON_WRITING
FLASH_SIZE=4MB_CONFIRMED
UART=FT232R_USB_UART_SESSION_PORT_/dev/ttyUSB0
UART_ACCESS=PASS_FOR_BOOTLOADER_HANDSHAKE
RESET_PATH=FT232R_DTR_RTS_DEFAULT_RESET
RESET_IS_POWER_CUT=NO
POWER_AT_HANDSHAKE=BOARD_RESPONDED
POWER_CUT_PATH=WAIVED_BY_OWNER
TEST_NVS=SUPERSEDED_BY_OWNER_AUTHORIZATION
PROJECT_USER_NVS_TOUCH=NOT_RUN
FIRST_FLASH=SUPERSEDED_BY_CURRENT_PASS
CLIENT_MATRIX=SUPERSEDED_BY_CURRENT_MINIMAL_EVIDENCE
FIRMWARE_SOURCE_SHA=b2f08f4d568c60559e575c824844199012b80c30
CURRENT_ESP_IDF=v6.1@fff9895c82d744c7237be8847347bdd1b07c6643
```

Der Handshake wurde mit `esptool v5.4.0 chip-id` und
`--before default-reset --after no-reset` ausgefuehrt. Er bestaetigt nur,
dass der ESP32 in diesem Moment ueber den FT232R erreichbar und versorgt war;
er bestaetigt keinen Power-Cut und keinen sicheren NVS-Zustand. Der vom Tool
ausgegebene Hardware-MAC wurde nicht in die Evidence uebernommen. Es wurden
keine Projekt-/Benutzer-Credentials gelesen, ausgegeben, ueberschrieben oder
geloescht.

### Historische Phase-B-Firmware vor dem damaligen Flash-Gate

Alle drei actor-free Kandidaten wurden auf dem damals dokumentierten Source-HEAD
mit ESP-IDF 6.1 gebaut. Die folgende Tabelle ist historische Evidence vor der
später erfolgten Ownerfreigabe und wird nicht als aktueller Hardwarestatus
verwendet.

| Kandidat | Build | Partition-/NVS-Aufbau | App `.bin` | App-Binary-SHA256 | App-ELF-SHA256 |
|---|---|---|---:|---|---|
| `espressif/network_provisioning` 1.2.4 | `PASS` | `nvs 0x9000/0x6000`, `phy_init 0xf000/0x1000`, `factory 0x10000/0x180000`; kein Test-NVS geflasht | 909424 B | `228af151ac107f9c1a4e8290b95e6e452863f64d08b3ee0a022e06e6d9f1237b` | `f231354b4be1ba91098573f6599b374d556704ad700c9c311b48e1f4f5ae16c6` |
| direkter `protocomm`-/ESP-IDF-Pfad | `PASS` | `nvs 0x9000/0x6000`, `phy_init 0xf000/0x1000`, `factory 0x10000/0x180000`; kein Test-NVS geflasht | 838128 B | `ace70b0464aeceb086fc4c9d4d18f4957ed38ed26dd01f0fe5eccc1e311917a3` | `6ad3eec5b0c250c48cdbf93dc3ad7964e67432f5cce06040e4a2f01b1ffdbbba` |
| kleiner nativer ESP-IDF-SoftAP-/HTTP-Pfad | `PASS` | IDF-Default `partitions_singleapp`: `nvs 0x9000/0x6000`, `phy_init 0xf000/0x1000`, `factory 0x10000/1M`; kein Test-NVS geflasht | 815488 B | `eddc3164770949cfd697e9b9eacc29cc4fd83590e467def390b2242a4898c4b5` | `1d7651c23223d6cf08e0b60027b80fe4d48019c82efe0b4fbb1b9483576cac32` |

Die gemeinsamen Buildartefakte haben weiterhin
`BOOTLOADER_BIN_SHA256=64a25a4d64fc7d1116cd7cb3c385cf37d66658d12c42c103219eb619c997dafc`
und
`PARTITION_TABLE_BIN_SHA256=7f00b6c042a89b15b0cac534f82ed988caf29278ff5700b0c511eb1b5bb7c820`.
Build-/Imagegroessen sind keine Laufzeit- oder Client-Evidence.

### Phase-B-Matrix

Die damalige Matrix wurde vor der ausdruecklichen Ownerfreigabe konservativ
geschlossen. Sie ist durch die aktuelle reale Phase-B-Evidence und die
proportionalen Owner-Waiver ersetzt; daraus werden keine aktuellen
`BLOCKED`- oder `FAILED`-Statuswerte fuer das Owner-Kandidatengate abgeleitet.

| Kandidat | Flash / SoftAP | Browser / direkte IP / Captive | Scan / Credential-Eingabe | Falsches Passwort / Abbruch / Timeout | Reconnect / Neustart | Commit-/NVS-/Recovery-Cut-Points | Laufzeitressourcen |
|---|---|---|---|---|---|---|---|
| `espressif/network_provisioning` 1.2.4 | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP`; native NVS-/Set-/Apply-Semantik bleibt Integrationsbefund | `SUPERSEDED_PRE_OWNER_STOP` |
| direkter `protocomm`-/ESP-IDF-Pfad | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP`; Handler bleiben Boundary-only und RAM-only | `SUPERSEDED_PRE_OWNER_STOP` |
| kleiner nativer ESP-IDF-SoftAP-/HTTP-Pfad | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP` | `SUPERSEDED_PRE_OWNER_STOP`; Probe bleibt ohne Credential-Commit | `SUPERSEDED_PRE_OWNER_STOP` |
| WiFiManager v2.0.17 | `CAPABILITY_NOT_PRESENT_FOR_CURRENT_GATE` | `DEFERRED_AFTER_SELECTION` | `DEFERRED_AFTER_SELECTION` | `DEFERRED_AFTER_SELECTION` | `DEFERRED_AFTER_SELECTION` | `DEFERRED_AFTER_SELECTION` | `DEFERRED_AFTER_SELECTION` |

Damit gab es in dieser historischen Momentaufnahme noch keinen Browser-only-
Befund, keinen vergleichbaren Client-/QR-Befund, keinen #57-/Security-/
Backup-/Reset-Entscheid und keine Kandidaten- oder Persistenzauswahl. Diese
Momentaufnahme ist durch den aktuellen Android-/Linux-Lauf und den
Ownerentscheid superseded; der physische QR-Scan ist heute
`PHYSICAL_DISPLAY_QR_TEST=DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION`.

### Recovery-, Reuse- und Owner-Gate des historischen Stops

Da kein Flash und kein Clientlauf stattgefunden hat, sind Write-, Readback-,
Reset-, Power-, CommitOutcomeUnknown-, alte-Konfiguration- und
superseded-Credential-Cut-Points `NOT_RUN`. Kein Projekt-/Benutzer-NVS wurde
beruehrt. Die bereits gescreenten Zusatzkomponenten wurden nicht gebaut; es
gab keinen Phase-B-Befund, der einen vertieften Reuse-Spike rechtfertigt.

```text
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=SUPERSEDED_PRE_OWNER_STOP
BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=OWNER_GATE_PENDING
OWNER_CANDIDATE_SELECTION_GATE=SUPERSEDED_PRE_OWNER_STOP
PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED
ACTUATOR_RELEASE=NO
NEXT_GATE=SUPERSEDED_BY_CURRENT_OWNER_CANDIDATE_GATE
```

## Ausfuehrungsgrenze

Die Artefakte unter `spikes/issue89_wlan_onboarding/` liegen ausserhalb des
Produktions-CMake-Graphs. Sie fuehren keinen projektspezifischen
`ConnectivityCredential`-Record, keine RecordTypeId 9, keine `cc0`-/`cc1`-
Semantik, keine Slotrotation, keinen Fallback-Selektor, keine
`StorageEpoch`-Mutation und keinen produktiven DNS-, Reconnect-, Portal- oder
Credential-Persistenzpfad ein. Die erzeugten SoftAP-Zugangsdaten sind nur
volatile Probe-Werte; ihre Schluessel werden nicht ausgegeben.

Damit ist insbesondere keine native Bibliotheks-Persistenz stillschweigend
zur zweiten Projektwahrheit geworden. Die offizielle Probe fuehrt im
dokumentierten Build-only-/No-Client-Scope keinen Set-/Apply-Vorgang aus;
deshalb wurde kein Credential-Commit ausgefuehrt oder beobachtet. Das macht den
unveraenderten `network_provisioning`-Manager aber nicht zu einem read-only-
oder volatilen Credentialpfad: beim tatsaechlichen Set/Apply nutzt er native
ESP-WiFi-/NVS-Persistenz. Die Probe kann beim
`network_prov_mgr_is_wifi_provisioned`-Aufruf vorhandenen nativen Zustand
lesen, startet bei bereits provisioniertem Zustand keinen Reset und fuehrt
keinen automatischen NVS-Erase mehr aus.

Der direkte Protocomm- und der native HTTP-Probe setzen jeweils
`WIFI_STORAGE_RAM` und schreiben keine Credentialkonfiguration in NVS. Ihre
Endpoint- beziehungsweise HTTP-Handler sind statische
Boundary-Nachweise; Requestdaten werden nicht interpretiert, angewendet oder
persistiert.

## Reproduzierbare lokale Evidence

Die Builds wurden mit ESP-IDF 6.0.2, Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e`, fuer die vorhandene 4-MB-
ESP32-/kein-PSRAM-Baseline erzeugt. Alle drei Projekte verwenden actor-free
Probe-Code; `APP_REAL_ACTUATORS_ENABLED=0` und
`ACTUATOR_RELEASE=NO` bleiben unveraendert.

| Nachweis | Ergebnis | Detail |
|---|---|---|
| `python3 spikes/issue89_wlan_onboarding/host_contract_test.py` | PASS | 5/5 Tests: bytebasierte SSID-/WPA2-Validierung, volatile Commitgrenze, Erhalt der bestehenden Konfiguration, Redaction und WLAN-QR-Escaping |
| offizieller ESP-IDF-Probe-Build | PASS | `idf.py -C spikes/issue89_wlan_onboarding/official_network_provisioning -B build/issue89_official_network_provisioning build`; `network_provisioning` 1.2.4, Paket-Hash `72d27784e3daf807418a34fb00be136ec50c6db49d989ce981d22e031fc0e7f8`; 894912 Bytes, Partition frei 15 %, IRAM frei 43477, DRAM frei 145297; NVS-Fehlerpfad ohne Erase |
| nativer ESP-IDF-Probe-Build | PASS | `idf.py -C spikes/issue89_wlan_onboarding/native_http_adapter -B build/issue89_native_http_adapter build`; 792900 Bytes, Partition frei 24 %, IRAM frei 45593, DRAM frei 145609 |
| direkter Protocomm-Probe-Build | PASS | `idf.py -C spikes/issue89_wlan_onboarding/direct_protocomm -B build/issue89_direct_protocomm build`; ESP-IDF-6.0.2-Built-in `protocomm`/`protobuf-c`/`esp_http_server`, Apache-2.0; 825712 Bytes, Partition frei 21 %, IRAM frei 43477, DRAM frei 145353 |
| Secret-Scan | PASS | `python3 scripts/check_secrets.py` ueber die vier neuen Python-/C-Artefakte: keine geschuetzten Dateien oder Geheimnismuster; der Standardlauf prueft keine privaten Pfade |
| Produktionsgraph | PASS | keine neue WLAN-Komponente, Persistenz oder Laufzeitkopplung in `lib/` bzw. im Root-Produktionsbuild |
| Flash-/UART-/Reset-Lauf | NOT_RUN | kein Flash war fuer die lokale Capability-Evidence erforderlich; ein spaeterer actor-free Hardwarelauf benoetigt einen separat dokumentierten Zielaufbau |

`managed_components/`, `sdkconfig` und Build-Ausgaben sind lokale generierte
Artefakte. Der reproduzierbare offizielle Dependency-Stand ist in
`official_network_provisioning/dependencies.lock` festgehalten; die
Komponenten werden nicht als produktive Abhaengigkeit in den Root-Graphen
uebernommen.

## Historischer 6.0.2-Reuse-Screen

Die folgende Tabelle ist die historische 6.0.2-Reuse-Evidence und bleibt als
solche unveraendert zitierbar. Die aktuelle 6.1-Source-/Manifest-/Lizenz-
Revalidierung steht in Abschnitt A5.1. Die vier Plan-Kandidaten bleiben die
gemeinsame Hauptmatrix; es gibt keine automatische Aufnahme in die
Hardwarematrix. Der direkte Protocomm-Pfad ist kein vergleichbarer
Client-/Recovery-PASS.

| Kandidat | Reuse-/Capability-Evidence | R1-/Vertragsluecke | Status fuer vertieften Spike |
|---|---|---|---|
| `espressif/network_provisioning` 1.2.4 | PASS fuer ESP-IDF-6.0.2-Build; native SoftAP-/Protocomm-/HTTP-Transportfunktionen | Standard-SoftAP-Schema startet Protocomm-HTTPD, liefert aber nicht automatisch ein normales Browser-Portal; native WiFi-Konfiguration wird vor erfolgreichem Verbindungstest in Flash gesetzt; Fehlerpfad stellt die bisherige funktionierende Konfiguration nicht als R1-Vertrag wieder her | JA, browser- und Persistenz-Gate offen |
| direkter `protocomm`-/ESP-IDF-SoftAP-/HTTP-/DNS-Pfad | PASS fuer reproduzierbaren ESP-IDF-6.0.2-Build; `protocomm_new`, `protocomm_httpd_start`, `protocomm_set_security`, `protocomm_set_version` und `protocomm_add_endpoint` werden ohne `network_provisioning` verwendet; isolierter SoftAP, HTTPD-Transport und `r1-set`/`r1-test`/`r1-commit`-Handlergrenzen; ESP-IDF-/Protocomm-/protobuf-c-/HTTPD-Abhaengigkeiten, Apache-2.0 | Handler verarbeiten keine Credentialdaten; kein Browser-/Captive-/DNS-/Scan-/Reconnect-/Commit-Nachweis und keine Produktsemantik; Security 0 ist nur Build-/Boundary-Evidence und kein R1-Sicherheitsnachweis | Capability PASS; Phase-B-Matrix PENDING |
| kleiner eigener nativer ESP-IDF-Adapter | PASS nur fuer isolierten SoftAP-/direkte-IP-HTTP-Capability-Build; kein produktiver Adapter | DNS/Captive Portal, Scan, Reconnect, Persistenz, Recovery und Commit sind absichtlich nicht implementiert; Eigenbau ist vor Reuse- und Owner-Gate keine Umsetzungsrichtung | JA, nur nach Gate und gegen Reuse-Evidence |
| WiFiManager v2.0.17 | Quellen-/Lizenzscreen; Arduino-Framework und `CMakeLists.txt`-Abhaengigkeit auf `arduino` | kein direkter nativer ESP-IDF-6.0.2-Pfad ohne Frameworkwechsel; keine gleichwertige ESP-IDF-Produktintegration belegt | `CAPABILITY_NOT_PRESENT_FOR_CURRENT_NATIVE_IDF_GATE`; nur bei Owner-Entscheid fuer Arduino nochmals pruefen |
| `thorrak/esp_wifi_config` v0.4.0 | liefert SoftAP, Captive Portal/DNS, Web-UI, Scan, Reconnect/Lifecycle und HTTPD-Sharing; MIT; aktueller Stand `32c78805e9fc206610b7debe31d06638cbe5da09`; Manifest ab IDF 5.4 und IDF-6-Hinweis auf `network_provisioning` | eigene NVS-/Auto-Commit-/Reconnect-/Reset-Semantik und Zusatzabhaengigkeiten muessen gegen #57, Security, Backup und Reset geprueft werden | `CONDITIONAL_YES`; nur bei realem Vorteil vertiefen |
| `tuanpmt/esp_wifi_manager` v1.1.0 | liefert SoftAP, Captive Portal/DNS, Web-UI, Scan, Multinetwork-Reconnect/Lifecycle und Reset; MIT; aktueller Stand `20f77d79e9cdde9e4d3f0c3c7a3bd3babfaf893a` | eigene NVS-/REST-/Config-Semantik, `esp_bus`-/mDNS-Abhaengigkeit, leeres Default-AP-Passwort und unredigierte Config-/REST-Risiken; kein belegter Vorteil gegenueber den Hauptkandidaten | `CONDITIONAL_NO`; kein Deep-Spike ohne neuen Vorteil |
| `nordesems/esp-captive-portal` v1.3.0 | MIT; aktueller Stand `b937ee88b86de47b40cd195f829cfd70e5af03c0`; DNS, DHCP Option 114, OS-Probes und Registrierung an bestehenden `esp_http_server` | liefert weder SoftAP, Credentialfluss, Scan, Reconnect, Storage noch Reset; Handler-Reihenfolge und Lifecycle muessen integriert geprueft werden | `CONDITIONAL_SUBCOMPONENT`; nur als kleiner DNS/OS-Probe-Teil vertiefen |

Historische Quellen dieses Screens: [`network_provisioning`](https://components.espressif.com/components/espressif/network_provisioning),
[`esp_wifi_config`](https://github.com/thorrak/esp_wifi_config),
[`esp_wifi_manager`](https://github.com/tuanpmt/esp_wifi_manager),
[`esp-captive-portal`](https://github.com/nordesems/esp-captive-portal) und
[`WiFiManager`](https://github.com/tzapu/WiFiManager). Die genannten Stände
und die URL-Auswertung sind Bestandteil dieser Evidence; ein Screen ist kein
Produktfreigabenachweis.

## Browser-only-Gate

Die offizielle Probe bestaetigt den Transport-Baustein, nicht den
browserbasierten R1-Vertrag. Der Standardpfad von
`network_provisioning` startet Protocomm ueber HTTP; die offizielle
Dokumentation und das Beispiel verwenden einen spezialisierten Client wie
`esp_prov.py` beziehungsweise einen App-orientierten Provisioning-/QR-Fluss.
Eine normale HTML-Seite mit Scan, verdeckter Eingabe, Test, expliziter
Bestaetigung, direkter IP und Fehler-/Recoveryanzeige ist damit nicht
automatisch vorhanden.

Vor einer Produktentscheidung sind deshalb getrennt zu messen:

1. Zusatzaufwand und Angriffs-/Wartungsflaeche fuer einen browserbasierten
   Client des offiziellen Protocomm-Protokolls;
2. Aufwand und Nutzen einer vorhandenen nativen Captive-Portal-Komponente,
   einschliesslich DNS-/OS-Probes, HTTPD-Sharing, Handlerplaetzen und
   kontrolliertem Start/Stop;
3. die Vereinfachung, falls der Owner eine Espressif-App oder einen anderen
   Standardclient akzeptiert.

Es wird keine grosse eigene Browser-Protocomm-Schicht gebaut, bevor der
Owner entschieden hat, ob Browser-only wirklich hart bleibt:

```text
BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=OWNER_GATE_PENDING
```

## Persistenz-, Commit- und Recovery-Befund

Der statische Review des offiziellen Managers ist fuer die bestehende #57-
Semantik entscheidend: `network_prov_mgr_configure_wifi_sta` setzt
`WIFI_STORAGE_FLASH` und schreibt die neue STA-Konfiguration mit
`esp_wifi_set_config`, bevor der Verbindungstest erfolgreich ist.
`NETWORK_PROV_WIFI_CRED_SUCCESS` wird erst spaeter nach dem IP-Ereignis
gemeldet. Ein Fehler nach diesem Vorab-Schreiben ist daher kein PASS fuer
das harte R1-Ergebnis, dass eine funktionierende Heim-WLAN-Konfiguration bei
fehlgeschlagenem Wechsel nicht unbemerkt zerstoert wird. Der aktuelle
Clientlauf hatte keinen Set-/Apply-Vorgang; jede Aussage ueber fehlendes
Commit gilt nur fuer diesen konkreten Lauf. Der unveraenderte
Manager ist bei einem spaeteren Clienttest gerade nicht read-only/volatil.
Die Probe behauptet keinen alten Credential-Fallback; ein solcher waere
insbesondere kein zulaessiger Vertrag nach #57.

Der NVS-Fehlerpfad des offiziellen Probes stoppt fail-closed und ruft
`nvs_flash_erase()` nicht auf. Fuer den kontrollierten, vom Owner freigegebenen
Phase-B-Lauf wurde der entbehrliche Development-Testtraeger verwendet; sein
vollstaendiger Flash und Default-NVS durften vor jedem Kandidatenlauf geloescht
und neu beschrieben werden. Ein zusaetzliches Test-NVS, eine separate
Testpartition und ein Backup waren nicht erforderlich. Ein nicht freigegebener
Projekt-/Benutzerstore darf weiterhin weder still geloescht noch fuer Tests
ueberschrieben werden. Der Komponentenquellcode wird nicht geforkt oder
gepatcht, um seinen Persistenzbefund zu verdecken.

Vor dem Owner-Gate wird deshalb keine Connectivity-Persistenz implementiert.
Falls der Owner die native Persistenz eines ausgewaehlten Components oder des
ESP-WiFi-Stacks akzeptiert, muessen die davon betroffenen #57-, Security-,
Backup- und Resetvertraege vor der Umsetzung ausdruecklich angepasst werden.
Falls eine eigene Connectivity-Domaene verbleibt, muss ihr spaeterer
Detailvertrag Commitidentitaet, Widerruf/Forward-Progress und die
Unerreichbarkeit alter `StorageEpoch`s festlegen; ein
`hoechster-gueltiger-Record + vorherige-Revision`-Fallback ist nicht
freigegeben.

## R1-Testmatrix und Grenzen

| Nachweis | Status | Grenze / naechster Nachweis |
|---|---|---|
| Portal explizit starten und kontrolliert beenden | PASS_MINIMAL_PROPORTIONAL | drei Transportstarts und native direkte HTTP-Seite real belegt; offizieller Browser-R1-Vertrag bleibt GAP; produktive Recovery folgt nach Integration |
| individuelle geschuetzte SoftAP-Zugangsdaten | PASS fuer den Testaufbau | volatile individuelle Werte wurden erzeugt, fuer Linux/Android verwendet und redigiert; produktive Credential-Persistenz bleibt ausserhalb des Scopes |
| WLAN-QR | DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION | synthetisches Format und Escaping im Host-Oracle PASS; realer Display-/Kamera-Test folgt mit Displayhardware |
| direkte IP | PASS_MINIMAL_PROPORTIONAL mit Kandidatengaps | official/direct Root `404`; native direkte HTTP-Seite `200`; Host und Android real verbunden |
| direkter Protocomm-Transport und oeffentliche Endpoint-Grenze | PASS fuer Capability | ESP-IDF-6.1-Build, realer Boot und echter WLAN-Transport; Set/Test/Commit bleiben Boundary-only und wurden nicht als Credentialvorgang ausgefuehrt |
| Captive Portal/DNS/OS-Erkennung | NOT_OBSERVED / CAPABILITY_NOT_PRESENT | Android-Captive-Auto-Open nicht beobachtet; native/direct ohne DNS-/Captive-Capability; offizieller Browser-R1-Vertrag `GAP` |
| Scan, Eingabe, Test, Abbruch, Timeout, Reconnect | PASS_MINIMAL_PROPORTIONAL mit deferred Capabilities | WLAN-/Host-Reconnect und falsches Passwort real; Scan/Form, Protokoll-Abbruch, Test/Commit und Spezialclient bleiben `CAPABILITY_NOT_PRESENT` oder nach Auswahl deferred |
| Android | PASS | alle drei SoftAPs beigetreten; direkte IP real; official/direct Root `404`, native Root `200` |
| iOS/iPadOS | WAIVED_BY_OWNER | vor dem Owner-Kandidatengate nicht verpflichtend |
| Windows | WAIVED_BY_OWNER | vor dem Owner-Kandidatengate nicht verpflichtend |
| Redaction in Logs, URLs, Diagnose, Backup | PARTIAL/PASS | Host-Oracle und statische Probeausgabe PASS; reale Bibliotheks-/Backuppfade nicht freigegeben |
| alte funktionierende Credentials bei Fehlversuch erhalten | PARTIAL | kandidatenneutrale Commitgrenze PASS; offizielle native Vorab-Flashsemantik ist Konfliktbefund, kein Produkt-PASS |
| Safety-/Regelungsunabhaengigkeit | PASS fuer Scope | keine Produktionskopplung; reale Laufzeitisolation bleibt Integrationsnachweis |
| Hardware-/UART-/Reset-Recovery | PASS fuer Transport-Recovery | drei ESP-IDF-6.1-Flash-/Bootlaeufe, DTR/RTS-Resets und anschliessende Host-/Android-Reconnects PASS; Credential-/Recovery-Cut-Points fehlen |
| Heap-/Stack-/Jitter-Messung | PASS_AT_TRANSPORT_START / DEFERRED_AFTER_SELECTION | vorhandene Start-/Reset-Werte reichen als Fruehindikator; vollstaendige Clientlast-/Gesamtsystemqualifikation folgt nach Integration |

Die proportionale Kandidatenevaluation ist damit fuer das Owner-Gate
ausreichend. Fehlende Spike-Funktionen werden nicht nachgebaut. Die
vollstaendige WLAN-/Web-/Reconnect-/Safety-Ressourcenqualifikation sowie der
Display-/Kamera-QR-Test erfolgen erst nach Auswahl und produktiver
Integration.

## Owner-Kandidatenauswahl abgeschlossen und naechster Integrationsplan

Der Owner hat nach der proportionalen vergleichbaren Evidence den nativen
ESP-IDF-HTTP-Pfad fuer die R1-Integration ausgewaehlt. Die Evaluation in Issue
#89 und PR #158 bleibt die historische Entscheidungsgrundlage; ihre
Kandidatengaps werden nicht als Implementierungs-PASS umgedeutet. Die separate
Integration wird in Issue #164 und einem eigenstaendigen, noch freizugebenden
Plan vorbereitet. Der vollstaendige R1-Produktvertrag ist bis dahin vom
aktuellen Owner-Gate getrennt.

Aktueller Status:

```text
PHASE_A_CAPABILITY_EVIDENCE=PASS
PHASE_B_TEST_SETUP=OWNER_AUTHORIZED
PHASE_B_FLASH_BOOT_EVIDENCE=PASS
PHASE_B_CLIENT_EVIDENCE=PASS_MINIMAL_PROPORTIONAL_WITH_CANDIDATE_GAPS
ANDROID_CLIENT_EVIDENCE=PASS
ANDROID_DIRECT_IP_TEST=PASS
ANDROID_CAPTIVE_PORTAL_AUTO_OPEN=NOT_OBSERVED
OFFICIAL_NETWORK_PROVISIONING_SOFTAP=PASS
OFFICIAL_BROWSER_R1_CONTRACT=GAP
DIRECT_PROTOCOMM_SOFTAP=PASS
DIRECT_PROTOCOMM_BROWSER_R1_CONTRACT=GAP
NATIVE_HTTP_SOFTAP=PASS
NATIVE_HTTP_BROWSER_TRANSPORT=PASS
IOS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
WINDOWS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
PHYSICAL_DISPLAY_QR_TEST=DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PASS_MINIMAL_PROPORTIONAL
PHASE_B_LOCAL_TEST_CREDENTIAL=EPHEMERAL_UNTRACKED_OVERRIDE
PHASE_B_SECRET_REDACTION=PASS
PHASE_B_RUNTIME_RESOURCE_EVIDENCE=PASS_AT_TRANSPORT_START; CLIENT_LOAD=NOT_RUN
POWER_CUT_TESTS=WAIVED_BY_OWNER
EFUSE_WRITE=NO
SECURE_BOOT_CHANGE=NO
FLASH_ENCRYPTION_CHANGE=NO
ROM_DOWNLOAD_MODE_DISABLE=NO
OWNER_CANDIDATE_SELECTION_GATE=COMPLETED
CANDIDATE_SELECTION=NATIVE_ESP_IDF_HTTP
OWNER_CANDIDATE_SELECTION=COMPLETED
R1_AP_ONLY=YES
R1_HOME_WIFI=YES
R1_HOME_WIFI_COUNT=1
CAPTIVE_PORTAL_REQUIRED=NO
WIFI_CREDENTIAL_OWNER=PROJECT_CONFIGURATION_DOMAIN
TEST_BEFORE_COMMIT=YES
SECOND_CREDENTIAL_STORE=NO
R1_NETWORK_SCOPE_SYNC=PASS
FUTURE_SCOPE_ISSUE=163
R1_IMPLEMENTATION_ISSUE=164
PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED
ACTUATOR_RELEASE=NO
IMPLEMENTATION=SPIKE_ONLY_EVIDENCE
```

Naechster Schritt ist die unabhaengige Planpruefung und anschliessende
Ownerfreigabe der exakten Integrationsplan-SHA. Bis dahin bleibt die produktive
Connectivity-Persistenz ungestartet und die Aktorfreigabe aus.
