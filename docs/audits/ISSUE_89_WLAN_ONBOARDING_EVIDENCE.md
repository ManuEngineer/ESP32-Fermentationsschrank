# Issue #89 – Phase-A-Reuse-/Capability-Evidence

Dieser Bericht dokumentiert die historische ESP-IDF-6.0.2-Baseline und die
separat aufgezeichnete autorisierte Phase-A-Revalidierung auf ESP-IDF 6.1.
Die 6.0.2-Evidence wird nicht nachtraeglich umetikettiert. Die vergleichbare
Phase-B-Client-/Recovery-Evidence ist noch nicht ausgefuehrt. Der Bericht ist
eine Entscheidungsgrundlage und keine Produktivauswahl.

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

Die offiziellen und direkten 6.1-Ergebnisse wurden auf dem Quellstand
`0b0d125379a5fef79ca2760a101603c831dc7e7e` ausgefuehrt. Der native
HTTP-Probe wurde nach dem Fail-Closed-/RAM-Storage-Fix auf dem Quellstand
`aa0d231e9b97cfe489b69b27d0b1ab5dcd28c775` neu gebaut. Die nachfolgenden
Dokumentationscommits aendern keine Probequelle; beide Source-SHAs bleiben
die Provenienz der jeweils ausfuehrbaren Evidence.

```text
ISSUE=89
PLAN_SHA=6a0a83b3b037c47b89caa87f9d2cb1e65f498c59
PARENT_APPROVED_PLAN_SHA=5c582aa179cd6e382dc4442a6824bb79a1e2b22f
OWNER_DECISION_BASE_HEAD=42a495d7139d9810086d5be77f81b2fccf3fc949
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
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PENDING
PHASE_B_EXECUTION=NOT_RUN
OWNER_CANDIDATE_SELECTION_GATE=NOT_READY
CANDIDATE_SELECTION=OWNER_PENDING_AFTER_COMPARABLE_EVIDENCE
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
| WiFiManager v2.0.17 | `BLOCKED_FOR_NATIVE_IDF_6_1_SPIKE` | aktueller Tag ist ein Arduino-/PlatformIO-Kandidat; der vorhandene CMake-Pfad verlangt `arduino`; kein nativer ESP-IDF-6.1-Pfad ohne Arduino-Produktionsframework |

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
| WiFiManager | [`tzapu/WiFiManager@v2.0.17`](https://github.com/tzapu/WiFiManager/tree/v2.0.17), Commit `d82d0a1b9fca741b9ec44accdf553606a6576dda`; MIT (`LICENSE`) | `library.json`/`library.properties` deklarieren Arduino; `CMakeLists.txt` hat `PRIV_REQUIRES arduino`; kein `idf_component.yml`; README nennt ESP8266-/ESP32-Arduino und PlatformIO | `autoConnect()` startet AP-/DNS-/Webportal und speichert ueber den Arduino-WiFi-Laufzeitpfad; `resetSettings()` und Portal-Timeout sind eigene Bibliothekssemantik; kein #57-kompatibler Storage-/Recoveryvertrag belegt | `SOURCE_SCREEN=PASS`; `BUILD=NOT_RUN`; `BLOCKED_FOR_NATIVE_IDF_6_1_SPIKE` |
| `thorrak/esp_wifi_config` | [`WiFiConfig/esp_wifi_config@32c78805e9fc206610b7debe31d06638cbe5da09`](https://github.com/thorrak/esp_wifi_config/tree/32c78805e9fc206610b7debe31d06638cbe5da09), Version `0.4.0`; MIT | `idf_component.yml`: `idf >=5.4`, `espressif/network_provisioning ^1.0.0` ab `idf_version >=6.0`; `library.json` nennt `espidf`/`arduino`, `library.properties` Arduino-ESP32 `3.3.11+`; CMake ist ESP-IDF-Komponente | NVS-basierte Mehrfachnetze, Auto-Reconnect und Portal; Reset-/Recovery- und Commitgrenzen sind eigene Bibliothekssemantik und nicht als #57-Vertrag nachgewiesen; zusaetzliche Storage-/HTTP-Lifecycle-Integration | `SOURCE_SCREEN=PASS`; `MANIFEST_DECLARATION_ACCEPTS_6_1=PASS`; `BUILD=NOT_RUN`; kein Shortlisting |
| `tuanpmt/esp_wifi_manager` | [`tuanpmt/esp_wifi_manager@20f77d79e9cdde9e4d3f0c3c7a3bd3babfaf893a`](https://github.com/tuanpmt/esp_wifi_manager/tree/20f77d79e9cdde9e4d3f0c3c7a3bd3babfaf893a), Version `1.1.0`; MIT | `idf_component.yml`: `idf >=5.0.0`, `tuanpmt/esp_bus ^1.0.3`, `espressif/mdns ^1.2`; CMake: `esp_wifi`, `esp_netif`, `nvs_flash`, `esp_http_server`, `esp_event`, `mdns` sowie private `esp_bus`, JSON und mbedTLS | NVS-Persistenz, SoftAP, REST, mDNS und Auto-Reconnect; eigener Reset-/Lifecycle-/Storagebesitz und HTTP-Handler-Sharing erzeugen Integrationsrisiko gegen #57 und die Recoverygrenze | `SOURCE_SCREEN=PASS`; `MANIFEST_DECLARATION_ACCEPTS_6_1=PASS`; `BUILD=NOT_RUN`; kein Shortlisting |
| `nordesems/esp-captive-portal` | [`nordesems/esp-captive-portal@b937ee88b86de47b40cd195f829cfd70e5af03c0`](https://github.com/nordesems/esp-captive-portal/tree/b937ee88b86de47b40cd195f829cfd70e5af03c0), Version `1.3.0`; MIT | `idf_component.yml`: `idf >=5.0.0`; CMake benoetigt `esp_event`, `esp_http_server`, `esp_netif`, `esp_wifi`, FreeRTOS, Log und lwIP; keine weitere externe Dependency | DNS-/DHCP-Option-114-Portal ohne Credential-Storage, Scan oder Reconnect; Lifecycle-/HTTPD-Handler-Reihenfolge und Kombination mit einem getrennten Owner-Transport bleiben Integrationsrisiko | `SOURCE_SCREEN=PASS`; `MANIFEST_DECLARATION_ACCEPTS_6_1=PASS`; `BUILD=NOT_RUN`; kein Shortlisting |

Die drei Zusatz-Screens wurden nur auf 6.1-relevante Manifest-/Dependency-
Aenderungen und die geforderten Lizenz-/Lifecycle-Felder revalidiert. Es gibt
keine zusaetzlichen Vollkandidaten, keine Auswahl und keinen Arduino-
Produktionspfad. WiFiManager bleibt fuer den nativen ESP-IDF-6.1-Spike
`BLOCKED_FOR_NATIVE_IDF_6_1_SPIKE`.

### Phase-A-Grenzen und naechster Gate

Die neuen 6.1-Build-PASS ersetzen weder Client- noch Browser-, QR-, Hardware-
oder Recovery-Evidence. Flash, UART, Reset, QR-Kamera, Android, iOS/iPadOS und
Windows sind in dieser Umsetzung `NOT_RUN`; Power-Cut ist fuer Phase B durch
den Owner als `WAIVED_BY_OWNER` festgelegt. Der vorhandene
ESP32-WROOM-32E-Dev-Aufbau ist fuer den kontrollierten Issue-#89-Spike als
entbehrlicher Testtraeger freigegeben. Ein zusaetzliches Test-NVS, eine
separate physische Testpartition oder ein Backup sind fuer diesen Testtraeger
nicht erforderlich. Die vergleichbare Kandidatenmatrix bleibt `PENDING` und
die Ausfuehrung wartet auf die Freigabe der exakten neuen Plan-SHA; ein
zusaetzliches NVS-/Power-Cut-Owner-Gate ist nicht offen.

## Aktueller Phase-B-Testaufbau nach Ownerentscheid

Der folgende Status ist die aktuelle Testgrenze nach dem Ownerentscheid. In
diesem Dokumentationscommit wurde noch kein Phase-B-Flash, Client-, Browser-,
UART-, Reset- oder Recoverytest ausgefuehrt. `Reset != Power-Cut` bleibt die
technische Begriffsgrenze; Power-Cut-Tests sind kein verpflichtendes
Acceptance-Criterion.

```text
OWNER_DECISION_BASE_HEAD=42a495d7139d9810086d5be77f81b2fccf3fc949
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
POWER_CUT_TESTS_REQUIRED=NO
POWER_CUT_TESTS=WAIVED_BY_OWNER
POWER_CUT_PATH=WAIVED_BY_OWNER
TEST_NVS=DEFAULT_NVS_ALLOWED_ON_DISPOSABLE_DEV_BOARD
UART_ACCESS=PASS_FOR_BOOTLOADER_HANDSHAKE
RESET_PATH=FT232R_DTR_RTS_DEFAULT_RESET
RESET_IS_POWER_CUT=NO
PROJECT_USER_NVS_TOUCH=NOT_RUN
FIRST_FLASH=NOT_RUN
CLIENT_MATRIX=NOT_RUN
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PENDING
FIRMWARE_SOURCE_SHA=b2f08f4d568c60559e575c824844199012b80c30
CURRENT_ESP_IDF=v6.1@fff9895c82d744c7237be8847347bdd1b07c6643
NEXT_GATE=OWNER_APPROVES_EXACT_REVISED_PLAN_SHA
```

Die kontrollierte Freigabe gilt ausschliesslich fuer diesen ausdruecklich
freigegebenen Development-Testtraeger. Automatisches oder unbeabsichtigtes
Loeschen bleibt in Produktcode, Bibliotheks- und Recoveryvertraegen
unzulaessig. Android, iOS/iPadOS, Windows, Browser/Captive Portal, direkte IP,
Credential-Test/Commit, Reconnect, Neustart, UART/Reset und Recovery bleiben
nach Plan ausstehende Phase-B-Nachweise.

## Historischer Phase-B-Stop vor dem Ownerentscheid

Die folgenden Angaben sind der damalige konservative Status vor der
ausdruecklichen Ownerfreigabe. Sie bleiben unveraendert als historische
Evidence und beschreiben nicht die aktuelle Testgrenze.

Vor jedem Flash-/Clienttest wurde der folgende Aufbau festgehalten. Der
Chip-Handshake war nichtschreibend erfolgreich; ein Flash oder Clientlauf
wurde danach nicht gestartet, weil das Test-NVS und der Power-Cut-Pfad nicht
als isoliert beziehungsweise Owner-gesichert bestaetigt waren.

```text
PHASE_B_TEST_SETUP=BLOCKED_HARDWARE
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
POWER_CUT_PATH=NOT_VERIFIED
TEST_NVS=BLOCKED_NOT_EXPLICITLY_ISOLATED_OR_OWNER_SECURED
PROJECT_USER_NVS_TOUCH=NOT_RUN
FIRST_FLASH=NOT_RUN
CLIENT_MATRIX=NOT_RUN
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

### Phase-B-Firmware vor dem Flash-Gate

Alle drei actor-free Kandidaten wurden auf dem oben genannten Source-HEAD mit
ESP-IDF 6.1 gebaut. Die Artefakte sind vorbereitet, aber nicht geflasht.

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

Mangels bestaetigtem isoliertem beziehungsweise gesichertem Test-NVS und
fehlendem verifiziertem Power-Cut-Pfad wurde kein Kandidat geflasht. Deshalb
sind alle hardware-, client- und laufzeitabhaengigen Felder `NOT_RUN`; der
fehlende sichere Testaufbau ist `BLOCKED_HARDWARE` und kein simuliertes PASS.

| Kandidat | Flash / SoftAP | Browser / direkte IP / Captive | Scan / Credential-Eingabe | Falsches Passwort / Abbruch / Timeout | Reconnect / Neustart | Commit-/NVS-/Recovery-Cut-Points | Laufzeitressourcen |
|---|---|---|---|---|---|---|---|
| `espressif/network_provisioning` 1.2.4 | `NOT_RUN` (`BLOCKED_HARDWARE`) | `NOT_RUN` | `NOT_RUN` | `NOT_RUN` | `NOT_RUN` | `NOT_RUN`; Phase-A-Screen bleibt: native NVS-/Set-/Apply-Semantik offen | `NOT_RUN` |
| direkter `protocomm`-/ESP-IDF-Pfad | `NOT_RUN` (`BLOCKED_HARDWARE`) | `NOT_RUN` | `NOT_RUN` | `NOT_RUN` | `NOT_RUN` | `NOT_RUN`; Phase-A-Handler bleiben Boundary-only und RAM-only | `NOT_RUN` |
| kleiner nativer ESP-IDF-SoftAP-/HTTP-Pfad | `NOT_RUN` (`BLOCKED_HARDWARE`) | `NOT_RUN` | `NOT_RUN` | `NOT_RUN` | `NOT_RUN` | `NOT_RUN`; Phase-A-Probe bleibt ohne Credential-Commit | `NOT_RUN` |
| WiFiManager v2.0.17 | `BLOCKED_FOR_NATIVE_IDF_6_1_SPIKE` | `NOT_RUN` | `NOT_RUN` | `NOT_RUN` | `NOT_RUN` | `NOT_RUN` | `NOT_RUN` |

Damit gibt es keinen neuen Browser-only-Befund, keinen vergleichbaren
Client-/QR-Befund, keinen #57-/Security-/Backup-/Reset-Entscheid und keine
Kandidaten- oder Persistenzauswahl. Android, iOS/iPadOS und Windows waren in
diesem Lauf `NOT_RUN`; ein physischer QR-Scan bleibt getrennt und ist ohne
angeschlossene Displayhardware `BLOCKED_HARDWARE` beziehungsweise `NOT_RUN`.

### Recovery-, Reuse- und Owner-Gate des historischen Stops

Da kein Flash und kein Clientlauf stattgefunden hat, sind Write-, Readback-,
Reset-, Power-, CommitOutcomeUnknown-, alte-Konfiguration- und
superseded-Credential-Cut-Points `NOT_RUN`. Kein Projekt-/Benutzer-NVS wurde
beruehrt. Die bereits gescreenten Zusatzkomponenten wurden nicht gebaut; es
gab keinen Phase-B-Befund, der einen vertieften Reuse-Spike rechtfertigt.

```text
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=BLOCKED_HARDWARE
BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=OWNER_GATE_PENDING
OWNER_CANDIDATE_SELECTION_GATE=NOT_READY
PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED
ACTUATOR_RELEASE=NO
NEXT_GATE=OWNER_CONFIRMS_ISOLATED_TEST_NVS_AND_POWER_CUT_SETUP
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
| WiFiManager v2.0.17 | Quellen-/Lizenzscreen; Arduino-Framework und `CMakeLists.txt`-Abhaengigkeit auf `arduino` | kein direkter nativer ESP-IDF-6.0.2-Pfad ohne Frameworkwechsel; keine gleichwertige ESP-IDF-Produktintegration belegt | `FAIL/BLOCKED_FOR_NATIVE_IDF_SPIKE`; nur bei Owner-Entscheid fuer Arduino nochmals pruefen |
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
fehlgeschlagenem Wechsel nicht unbemerkt zerstoert wird. Die aufgezeichnete
Probe hatte keinen Client und keinen Set-/Apply-Vorgang; jede Aussage ueber
fehlendes Commit gilt nur fuer diesen konkreten Lauf. Der unveraenderte
Manager ist bei einem spaeteren Clienttest gerade nicht read-only/volatil.
Die Probe behauptet keinen alten Credential-Fallback; ein solcher waere
insbesondere kein zulaessiger Vertrag nach #57.

Der NVS-Fehlerpfad des offiziellen Probes stoppt fail-closed und ruft
`nvs_flash_erase()` nicht auf. Vor jedem spaeteren realen Client-/Credential-
Test sind ein explizit wegwerfbares oder gesichertes Test-NVS, ein eindeutig
isolierter Flash-/Partitionsaufbau und die dokumentierte Backup-/Resetgrenze
vorzubereiten. Ein bestehender Projekt-/Benutzerstore darf weder still
geloescht noch fuer den Test ueberschrieben werden. Der Komponentenquellcode
wird nicht geforkt oder gepatcht, um seinen Persistenzbefund zu verdecken.

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
| Portal explizit starten und kontrolliert beenden | PARTIAL | offizieller Manager-/native Probe-/direkter Protocomm-Transportstart belegt; Browser-UI, Stop, Timeout und Recovery fehlen |
| individuelle geschuetzte SoftAP-Zugangsdaten | PARTIAL | volatile individuelle Werte werden erzeugt und redigiert; keine reale Clientabnahme |
| WLAN-QR | PARTIAL | synthetisches Format und Escaping im Host-Oracle PASS; QR-Encoding, Anzeige und Kamera-Decoding NOT_RUN |
| direkte IP | PARTIAL | native Probe registriert eine direkte HTTP-Seite; realer Zugriff NOT_RUN |
| direkter Protocomm-Transport und oeffentliche Endpoint-Grenze | PASS fuer Capability | ESP-IDF-6.0.2-Build bindet Security-, Version- und Set/Test/Commit-Handlergrenzen ohne High-Level-Manager; kein Clientlauf |
| Captive Portal/DNS/OS-Erkennung | NOT_RUN | kein vollständiger Kandidatennachweis; zusätzlicher Portal-Screen bleibt konditional |
| Scan, Eingabe, Test, Abbruch, Timeout, Reconnect | NOT_RUN | keine Produktlogik vor Owner-Gate |
| Android | NOT_RUN | kein Client-/Hardwarelauf |
| iOS/iPadOS | NOT_RUN | kein Client-/Hardwarelauf |
| Windows | NOT_RUN | kein Client-/Hardwarelauf |
| Redaction in Logs, URLs, Diagnose, Backup | PARTIAL/PASS | Host-Oracle und statische Probeausgabe PASS; reale Bibliotheks-/Backuppfade nicht freigegeben |
| alte funktionierende Credentials bei Fehlversuch erhalten | PARTIAL | kandidatenneutrale Commitgrenze PASS; offizielle native Vorab-Flashsemantik ist Konfliktbefund, kein Produkt-PASS |
| Safety-/Regelungsunabhaengigkeit | PASS fuer Scope | keine Produktionskopplung; reale Laufzeitisolation bleibt Integrationsnachweis |
| Hardware-/UART-/Reset-Recovery | NOT_RUN/BLOCKED_HARDWARE | getrenntes Hardwarefenster mit ESP32, reproduzierbarem Boot/Reset und Clientmatrix erforderlich |
| Heap-/Stack-/Jitter-Messung | NOT_RUN | Buildgroessen sind dokumentiert; Laufzeitbudgets und Regelungs-/Safety-Jitter sind nicht gemessen |

Die Nachweise ohne zusätzliche Verkabelung sind damit auf Host-Oracle,
statischen Source-/Manifest-Screen und actor-free IDF-Builds begrenzt. Reale
Android-, iOS- und Windows-Browser-, QR-, SoftAP-, Reset- und
Reconnect-Nachweise benötigen den ESP32-Aufbau, UART/Resetzugang und die
jeweiligen Clientgeraete. Sie werden nicht durch Host- oder Build-PASS
ersetzt.

## Owner-Gate vor produktiver Auswahl

Nach der vergleichbaren Evidence entscheidet der Owner ausdrücklich:

- `BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=YES|NO`;
- welcher der vier Hauptpfade, gegebenenfalls mit einem begründeten
  Zusatz-Screen-Kandidaten als Teilkomponente, weiterverfolgt wird;
- ob native Component-/ESP-WiFi-Persistenz den Produktvertrag uebernehmen
  darf oder welche #57-/Security-/Backup-/Reset-Anpassung zulaessig ist;
- welche minimale Integrationsgrenze und welche realen Client-/Hardwaretests
  vor Phase D gelten.

Bis zu diesem Gate ist der Status:

```text
PHASE_A_CAPABILITY_EVIDENCE=PASS
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PENDING
OWNER_CANDIDATE_SELECTION_GATE=NOT_READY
CANDIDATE_SELECTION=OWNER_PENDING_AFTER_COMPARABLE_EVIDENCE
PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED
IMPLEMENTATION=PHASE_A_CAPABILITY_EVIDENCE_ONLY
```

Erst danach darf Phase D den kleinsten verbleibenden projektspezifischen
Integrationsdelta planen. Normale Review-/Pre-Ready-Gates folgen erst nach
dieser Auswahl und der entsprechenden Plan-/Vertragsfreigabe.
