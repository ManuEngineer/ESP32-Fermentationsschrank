# Issue #159 – ESP-IDF 6.1 Upgrade-Evidence

Stand: 2026-09-14. Diese Datei dokumentiert nur die Issue-159-Provenienz,
Migrationsergebnisse und Deltas. Ausführungszeitpunkt, Gate-Reihenfolge,
Profile, Hardware-Smokes und Ergebnisbegriffe bleiben im
[`ESP-IDF-Upgradevertrag`](../ESP_IDF_UPGRADE_CONTRACT.md), in
[`CI_AND_QUALITY_GATES.md`](../CI_AND_QUALITY_GATES.md) und im versionierten
Runner [`scripts/run_pre_ready_gates.sh`](../../scripts/run_pre_ready_gates.sh)
kanonisch.

## Provenienz und Scope

```text
ISSUE=159
PR=160
BASE=main@2c010e8a8be8e351f89b79ae6c74f665d24a1f0e
APPROVED_PLAN_SHA=8b181954949fe32d32a26312f8eefe415f65acd1
TARGET_TAG=v6.1
TARGET_COMMIT=fff9895c82d744c7237be8847347bdd1b07c6643
BASELINE_TAG=v6.0.2
BASELINE_COMMIT=7101770dc6db2667b3c477cc31365dd1acd6db4e
BASELINE_CHECKOUT=/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.0.2
TARGET_CHECKOUT=/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.1
PR158=OUT_OF_SCOPE_UNCHANGED
ISSUE89=OUT_OF_SCOPE
ACTUATOR_RELEASE=NO
```

Die beiden lokalen ESP-IDF-Checkouts sind die parallel installierten,
unveränderten Vergleichs- und Zielwerkzeuge. Die Checkouts werden nicht in
dieses Repository eingecheckt. Historische v6.0.2-Pläne, Reports, Audits,
ADR-/Changelog-Nachweise und ihre damaligen Messwerte bleiben unverändert.

## Implementierte Änderungen

| Bereich | Ergebnis |
|---|---|
| ESP-IDF-Pin und CMake-Fail-fast | `PASS`: aktive Verträge prüfen `v6.1` und `fff9895c82d744c7237be8847347bdd1b07c6643` |
| CI-/Profil-/Provenienztexte | `PASS`: aktive Produktionspfade und Pfadnamen sind auf v6.1 synchronisiert; 4 MB, kein PSRAM und Aktorpolicy unverändert |
| Component Manager / Lockfile | `PASS`: Manifestversionen `ds3231 1.1.7` und `i2cdev 2.1.2` unverändert; generiertes `dependencies.lock` aktualisiert nur die IDF-Version von `6.0.2` auf `6.1.0` |
| esp-clang | `PASS`: v6.1-`tools.json`-Provenienz `esp-21.1.3_20260408`, LLVM `21.1.3` und Linux-amd64-SHA im Vertrag/Selftest synchronisiert; `pyclang` bleibt `0.7.0` |
| Produktionsadapter | `PASS`: keine API-/Architekturänderung erforderlich; NVS-Kommentar/Quellenbezug auf v6.1 aktualisiert, fail-closed-Mapping bleibt unverändert |
| Hardware-/Partition-/Aktorpolicy | `NOT_CHANGED`: keine neue Anforderung, kein neuer GPIO-/Partitionswert und keine Aktorfreigabe |

## 6.0-auf-6.1-Migrationsmatrix

Die Bewertung bezieht sich auf den tatsächlich verwendeten Code auf `main`.
Nicht verwendete Kandidaten aus historischen Audits und PR #158 sind kein
Produktionspfad.

| Offizieller 6.1-Punkt | Tatsächlich geprüfter Repositorypfad | Ergebnis |
|---|---|---|
| MQTT aus IDF in Component Manager verschoben | kein MQTT in `main`, Manifest oder aktivem Produktionsgraph | `NOT_AFFECTED` |
| FreeRTOS-Header nicht mehr implizit aus Peripheral-Headern | Issue-90-Harness nutzt `driver/uart.h` und explizite eigene Includes | `NOT_AFFECTED`; gezielter Harness-Build |
| GPIO-Wakeup-API umbenannt | keine `gpio_deep_sleep_wakeup_*`-Nutzung | `NOT_AFFECTED` |
| GPIO-ROM-Präfixe und entfernte GPIO-Makros | keine direkte ROM-Funktion und keine genannten Makros | `NOT_AFFECTED` |
| LCD-Farbformat-/FourCC-/DSI-Änderungen | kein LCD-/DSI-Produktcode | `NOT_AFFECTED` |
| SPI shared interrupt flag entfernt | kein `spi_bus_initialize` und keine SPI-Interruptflags | `NOT_AFFECTED` |
| SPI-Flash-OS-`start(flags)` und private Header | keine Custom-Flash-Treiber, privaten Flash-Header oder `esp_flash_t`-Member | `NOT_AFFECTED` |
| `soc/uart_channel.h` entfernt | Harness und Produktionscode nutzen kein solches Include | `NOT_AFFECTED`; gezielter Harness-Build |
| Legacy-UART-Wakeup-APIs deprecated | keine UART-Wakeup-API und kein Light-Sleep-Wakeup | `NOT_AFFECTED` |
| `idf.py flash` standardmäßig Fast-Reflash | kein automatischer Produktions-Flashpfad; bestehender Issue-90-esptool-Vertrag | `NOT_AFFECTED` für Firmwareverhalten |
| NVS-BLOB-/Commit-Pfad | `nvs_flash`, `nvs_open_from_partition`, Blob-Read/Write, `nvs_commit`, bestehender Host-/Issue-90-Pfad | `PASS_NO_ADAPTER_CHANGE`: v6.1 ändert interne mehrseitige BLOB-Fehlerbereinigung; `nvs_commit` bleibt kein nachgelagerter Cache-Commit; fail-closed `CommitOutcomeUnknown` bleibt |
| I2C-/DS3231-Component-Manager-Pfad | `ds3231`-/`i2cdev`-Adapter und Lockfile | `PASS_NO_ADAPTER_CHANGE`: v6.1-Configure erkennt den neuen `i2c_master`-Pfad bei unveränderten fest gepinnten Komponenten |
| Wi-Fi-/Netzwerk-Migration | nur `esp_netif_sntp_*`/SNTP-Koordination; kein `esp_wifi`-Lifecycle | `NOT_AFFECTED`; #89/#158 nicht geändert |
| HTTP-/Provisioning-/`protocomm`-/MQTT-Pfade | keine aktive Verwendung | `NOT_AFFECTED` |
| PSA persistente ECDSA-/HMAC-Schlüssel | keine PSA-Key-API und keine solche Persistenz | `NOT_AFFECTED` |
| Secure-Boot-192-bit-Kurve und SoC-spezifische Änderungen | keine Secure-Boot-Konfiguration/API; Ziel ist ESP32 | `NOT_AFFECTED` |
| neue P4-Defaultrevision und neue PSRAM-Funktionen | `CONFIG_IDF_TARGET=esp32`, 4 MB, kein PSRAM | `NOT_AFFECTED` |

Quellen: [ESP-IDF v6.1 Release](https://github.com/espressif/esp-idf/releases/tag/v6.1),
[Migration 6.0 auf 6.1](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/migration-guides/release-6.x/6.1/index.html),
[v6.1 tools.json](https://github.com/espressif/esp-idf/blob/v6.1/tools/tools.json),
die v6.1-Quellen unter `components/nvs_flash/`, `components/esp_netif/`,
`components/driver/` und die fest gepinnten Component-Manager-Quellen.

## Gezielte Builder-Nachweise

| Nachweis | Status | Bemerkung |
|---|---|---|
| Herkunft beider ESP-IDF-Checkouts | `PASS` | Tag, exakter Commit und sauberer Arbeitsbaum vor den v6.1-Prüfungen verifiziert |
| v6.1 Component-Manager-Configure | `PASS` | `idf.py reconfigure` mit `sdkconfig.defaults` und Bring-up-Overlay; kein Produktprofilbuild |
| effektive v6.1-Konfiguration | `PASS` | Ziel `esp32`, 4 MB Flash, kein PSRAM; keine Overlayänderung erforderlich |
| Issue-90-UART-/NVS-Harness | `PASS` | `python3 scripts/build_issue90_slice7_harness.py`; v6.1-ESP32-Build, `state_store_test`, Bring-up enthalten, Release ausgeschlossen |
| direkt betroffene Python-Selftests | `PASS` | `check_issue90_partitions.py --self-test`, `check_secrets.py --selftest`, `run_esp_idf_static_analysis.py --selftest` |
| direkt betroffener NVS-Hosttest | `PASS` | v6.1-Linux-Hostbuild und Ausführung des erzeugten `issue90_nvs_adapter_host.elf`; `ISSUE90_HOST_ADAPTER_GATE=PASS`, Produktbrücke `PASS:3 FAIL:0 BLOCKED:0 NOT_RUN:0` |
| Builder-Static-Analysis-Self-Check | `PASS` | `PRE_READY_EXPECTED_HEAD=41546b6f4e24a46b32ead1b01d26093b36759d1a`; `CLANG_FORMAT=PASS`, `CLANG_TIDY=NOT_REQUIRED` |

Der erste v6.1-Compile des bestehenden Harnesses und anschließend des
NVS-Host-Orakels legte wegen GCC 15.2 mit `-Werror=switch` bereits vorhandene
Enum-Fälle offen. Die minimale Ergänzung der drei Statusfälle im Harness und
der zwei Oracle-Projektionen ist in der gezielten Prüfung enthalten; es gab
keine Änderung an Fach-, Persistenz- oder Safety-Semantik.

Im gezielten v6.1-Ausgabesatz wurden folgende Warnungen klassifiziert:

- `MINIMAL_BUILD ... disregarded because the COMPONENTS variable is defined`
  stammt aus der bestehenden separaten Harness-Projektstruktur mit
  `COMPONENTS=main` und ändert keinen Produktionsbuild;
- die v6.1-`component_validation.cmake`-Warnung zum privaten Include von
  `bootloader_support` durch das IDF-eigene `esp_partition` ist ein Upstream-
  Frameworkhinweis, keine geänderte Repository-Abhängigkeit.

Die vollständige Warnungs-/Deprecationsauswertung steht im folgenden
ownerautorisierten Upgrade-Nachweis. Die dort beschriebene erste
6.1-Analyseabweichung wurde vor der finalen Wiederholung lokal begrenzt
korrigiert.

## Vollständiger Upgrade-Nachweis und Hardware-Parität

Die Owner-Autorisierung für den vollständigen Nachweis lag für den
Independent-Review-HEAD `41546b6f4e24a46b32ead1b01d26093b36759d1a` vor. Nach
dem dabei gefundenen, semantikneutralen 6.1-`esp-clang`-Befund wurde die
begrenzte Korrektur als `fc306c4428a2bd770866e5f23ce0881f38bc1baf` gepusht
und die vollständige Fix Verification auf genau diesem HEAD wiederholt.

| Nachweis | Status | Provenienz / Ergebnis |
|---|---|---|
| vollständiger v6.0.2-Baseline-Build beider Profile | `PASS` | Source `2c010e8a8be8e351f89b79ae6c74f665d24a1f0e`; ESP-IDF `v6.0.2` / `7101770dc6db2667b3c477cc31365dd1acd6db4e`; `esp32_bringup` und `esp32_release` |
| vollständiger v6.1-Build beider Profile | `PASS` | Source `fc306c4428a2bd770866e5f23ce0881f38bc1baf`; ESP-IDF `v6.1` / `fff9895c82d744c7237be8847347bdd1b07c6643`; `esp32_bringup` und `esp32_release` |
| vollständiger Host-Pre-Ready-Lauf | `PASS` | `PRE_READY_EXPECTED_HEAD=fc306c4428a2bd770866e5f23ce0881f38bc1baf`; 1.178/1.178 native Tests, clang-tidy, Architektur-, Secret- und Quality-Gates |
| `scripts/run_esp_idf_static_analysis.py all` | `PASS` | final auf `fc306c4428a2bd770866e5f23ce0881f38bc1baf` mit `esp-clang 21.1.3` für Bring-up und Release |
| `sdkconfig`-Vergleich je Profil | `PASS` | beide Profile besitzen dasselbe normalisierte Delta mit 28 geänderten Schlüsseln; keine Overlayänderung |
| Flash-/RAM-/bestehender Ressourcenvergleich | `PASS` | exakte Werte und Deltas siehe Tabelle unten; keine verbindliche Budgetgrenze erfunden |
| Warnungs-/Deprecationsvergleich | `PASS` | keine Warnung/Deprecation im vollständigen Profil-`build.log`; erwartete Upstream-Clang-Konfigurationswarnung, keine Produkt- oder API-Warnung |
| Stack-/Heap-Messung | `NOT_RUN` | der bestehende JSON2-Buildbericht liefert keine reale Stack-/Heap-Messung; das bleibt ein Hardware-/Belastungsnachweis gemäß Vertrag |

### Generierte `sdkconfig`-Deltas

Die normalisierte Gegenüberstellung von `build/esp32_bringup/sdkconfig` und
`build/esp32_release/sdkconfig` gegen die getrennte v6.0.2-Baseline ist
identisch. Die folgenden Änderungen sind vollständig; Reihenfolge- und
Kommentaränderungen sind nicht als Delta gezählt:

```text
CONFIG_IDF_INIT_VERSION: "6.0.2" -> "6.1.0"
new unset defaults: CONFIG_APP_BUILD_MINIMIZE_BINARY_CHANGES,
  CONFIG_ESP_SLEEP_SET_FLASH_DPD, CONFIG_LWIP_ND6_SUPPORT_STATIC_ENTRIES,
  CONFIG_MBEDTLS_PSA_ITS_CUSTOM_STORAGE_BACKEND,
  CONFIG_MBEDTLS_SECURE_ELEMENT_DRIVER_ENABLED
new target metadata/defaults: CONFIG_ESPTOOLPY_FLASHMODE_VAL=3,
  CONFIG_ESP_EVENT_POST_FROM_ISR_SIZE=4,
  CONFIG_ESP_ROM_BOOTLOADER_OFFSET_FLASH=0x1000,
  CONFIG_ESP_ROM_HAS_REGI2C_IMPL=y, CONFIG_ESP_STDIO_MAX_VFS_ENTRIES=2,
  CONFIG_SECURE_BOOT_IMAGE_DIGEST_LEN=32,
  CONFIG_SECURE_BOOT_ROM_FAST_WAKE_RESERVE_SIZE=0,
  CONFIG_SOC_EMAC_REF_CLK_FROM_APLL=y,
  CONFIG_SOC_GPIO_HP_PERIPH_PD_SLEEP_WAKEABLE_MASK=0,
  CONFIG_SOC_GPIO_SUPPORT_HP_PERIPH_PD_SLEEP_WAKEUP=y,
  CONFIG_SOC_PM_RTC_NOT_SUPPORT_UART2_WAKEUP=y,
  CONFIG_SOC_REGI2C_SUPPORTED=y, CONFIG_SOC_RTC_TIMER_SUPPORTED=y,
  CONFIG_SOC_RTC_TIMER_V1=y, CONFIG_SOC_RTC_WDT_SUPPORTED=y,
  CONFIG_SOC_SPI_EXTERNAL_NOR_FLASH_SUPPORTED=y
removed/replaced target metadata: CONFIG_SOC_RTC_TIMER_V1_SUPPORTED,
  CONFIG_SOC_SPI_AS_CS_SUPPORTED, CONFIG_SOC_SPI_DMA_CHAN_NUM=2,
  CONFIG_SOC_SPI_MAX_CS_NUM=3, CONFIG_SOC_SPI_MAX_PRE_DIVIDER=8192,
  CONFIG_SOC_SPI_SUPPORT_CLK_APB
```

Die neuen und entfernten `SOC_*`-Werte sind 6.1-Targetmetadaten; im
Repository gibt es keinen entsprechenden SPI-, Flash-, Sleep-, Secure-Element-
oder UART-Wakeup-Produktpfad. `CONFIG_ESPTOOLPY_FLASHMODE_VAL=3` beschreibt
weiterhin den unveränderten `dio`-/40-MHz-/4-MB-Vertrag. Beide Profile bleiben
`CONFIG_IDF_TARGET=esp32`, 4 MB, ohne PSRAM und mit unveränderter
Partitionstabelle.

### Ressourcenvergleich aus den bestehenden JSON2-Berichten

| Profil / Messwert | v6.0.2 | v6.1 | Delta | Einordnung |
|---|---:|---:|---:|---|
| `bringup` `size.json total_size` | 414111 B | 417759 B | +3648 B | Framework-/Toolchain-Delta, innerhalb der bestehenden `TBD_IMPLEMENTATION_BUDGET`-Regel |
| `release` `size.json total_size` | 397063 B | 400699 B | +3636 B | Framework-/Toolchain-Delta, innerhalb der bestehenden `TBD_IMPLEMENTATION_BUDGET`-Regel |
| beide Profile DRAM used | 17246 B | 17526 B | +280 B | gleicher ESP32-Kapazitätswert 180736 B |
| beide Profile IRAM used | 48675 B | 48715 B | +40 B | gleicher ESP32-Kapazitätswert 131072 B |
| `bringup` App-BIN | 414224 B | 417872 B | +3648 B | kein Partitions- oder Policywechsel |
| `release` App-BIN | 397184 B | 400816 B | +3632 B | kein Partitions- oder Policywechsel |
| Bootloader-BIN, beide Profile | 26096 B | 26176 B | +80 B | 6.1 Frameworkdelta |
| Partitionstabellen-BIN, beide Profile | 3072 B | 3072 B | +0 B | unverändert |
| `bringup` ELF / Mapfile | 15613180 / 8032524 B | 15692420 / 8060170 B | +79240 / +27646 B | Debug-/Toolchain-Artefakte, nicht als Flashbudget verwendet |
| `release` ELF / Mapfile | 14471928 / 7695665 B | 14550104 / 7723150 B | +78176 / +27485 B | Debug-/Toolchain-Artefakte, nicht als Flashbudget verwendet |

### Warnungs- und Static-Analysis-Befund

Die vollständigen Profilbuilds erzeugten in beiden Versionen keine
`warning`-, `deprecated`- oder `error:`-Zeilen im jeweiligen Buildlog. Die
6.1-Analyse-Konfiguration erzeugt wie 6.0.2 den erwarteten Espressif-Hinweis,
dass der Clang-Build experimentell ist; das ist kein Produktbefund. Die in
`warnings.txt` sichtbaren Namen `modernize-deprecated-*` sind aktivierte
Checknamen und keine Findings.

Der erste vollständige v6.1-Analyseversuch auf `41546b6...` fand neun durch
`esp-clang 21.1.3` neu als Fehler behandelte Stilbefunde: sieben
`#if defined(...)`-Bedingungen und zwei `std::lock_guard`-Verwendungen. Der
v6.0.2-Vergleich mit `esp-clang 20.1.1` war für beide Profile PASS. Die
Korrektur auf `fc306c4...` verwendet ausschließlich `#ifdef` und
`std::scoped_lock`; sie verändert keine Fach-, Safety-, Persistenz-, GPIO-,
Flash-, Partition- oder Aktorsemantik. Die vollständige Wiederholung ist
danach für beide Profile PASS.

### Eindeutige Hardware-Parität

Die Hardware-Parität erweitert die bereits qualifizierte v6.0.2-Oberfläche
nicht. Ihr Umfang ist exakt:

- die in [`ISSUE_29_MEASUREMENTS.md`](../ISSUE_29_MEASUREMENTS.md) real
  qualifizierte ESP32-D0WD-V3-Revision 3.1 mit 4 MB Flash, ohne PSRAM,
  FTDI-FT232R-UART/ROM-Bootloader sowie DTR/RTS-Reset;
- die bereits unter v6.0.2 qualifizierten aktorfreien `esp32_bringup`- und
  `esp32_release`-Smokes mit Anwendung bereit, Heartbeat/Uptime und genau zwei
  Ressourcenpunkten ohne Reset, Panic, Watchdog oder Brownout;
- die in der bestehenden Roadmap- und Issue-90-Evidence ausgewiesene Kampagne
  mit sechs realen Power-Cuts, Produktionsrestore und anschließendem
  Produktboot auf derselben Board-/UART-/Restore-Oberfläche.

`HARDWARE_PARITY=PARTIAL`: In einer späteren Sitzung (2026-09-15) war die in
`ISSUE_29_MEASUREMENTS.md` qualifizierte reale Board-/UART-Basis verfügbar;
`HARDWARE_SMOKE_BRINGUP` und `HARDWARE_SMOKE_RELEASE` sind damit für den
finalen v6.1-HEAD `fc306c4428a2bd770866e5f23ce0881f38bc1baf` real belegt
(siehe Unterabschnitt unten). `ISSUE90_POWER_CUT_RESTORE` blieb in dieser
Sitzung außerhalb des Umfangs und bleibt `NOT_RUN`; die Sechsfach-Power-Cut-
Kampagne sowie die Stack-/Heap-Belastungsmessung sind damit weiterhin
offene Hardwarenachweise. Software-PASS wird nicht als Hardware-PASS
umetikettiert. `IMPLEMENTED_DIGITAL_PENDING_HARDWARE`, `BLOCKED_HARDWARE`,
Display, Touch, Sensoren, Lüfter, BTS/Peltier und spätere Commissioning-
Scope bleiben außerhalb. Es gibt keine Ersatzhardware, keine neue
Hardwareanforderung und keine Aktorfreigabe.

#### Realer Hardware-Smoke-Nachweis (2026-09-15)

Board: ESP32-D0WD-V3 Revision v3.1, 4 MB Flash, kein PSRAM,
FTDI-FT232R-UART-Adapter (`/dev/ttyUSB0`, MAC `20:50:0d:1b:2f:34`), DTR/RTS-
Reset über die vorhandene actor-free ESP32-Basis; identisch zur in
`ISSUE_29_MEASUREMENTS.md` qualifizierten Oberfläche. Beide Profile wurden
mit `esptool.py` (v4.11.0, `--flash_mode dio --flash_size 4MB --flash_freq
40m`) auf `fc306c4428a2bd770866e5f23ce0881f38bc1baf` geflasht; `Hash of data
verified` für Bootloader, Partitionstabelle und App-Image in beiden Fällen.
Der Boot-Log wurde über einen kontrollierten DTR/RTS-Reset (DTR/IO0 deassert
vor dem RTS/EN-Puls, um versehentlichen Download-Bootloader-Verbleib zu
vermeiden) direkt mitgeschnitten:

| Profil | `application: ready` | Ressourcenpunkte | Heartbeat/Uptime | Reset/Panic/Watchdog/Brownout | Aktorpolicy |
|---|---|---|---|---|---|
| `esp32_bringup` | ja | genau 2 (t=795 ms, t=31015 ms) | 38 Zeilen, exakt 1000 ms-Takt, keine Lücke/Duplikat | keiner außer dem einen erwarteten `POWERON_RESET` beim Boot | `LOCKED_FOR_BRINGUP`; `real actuators: disabled` |
| `esp32_release` | ja | genau 2 (t=799 ms, t=30809 ms) | 39 Zeilen, exakt 1000 ms-Takt, keine Lücke/Duplikat | keiner außer dem einen erwarteten `POWERON_RESET` beim Boot | `REQUIRE_VERIFIED_HARDWARE`; `real actuators: disabled` |

Beide Boot-Logs bestätigen `App version: fc306c4428a2bd770866e5f23ce0881f38bc1baf`
und `ESP-IDF: v6.1`. Der eingebettete `issue29_probe` meldet
`result=PASS`, `actor_release=false`, `safety_fail_closed=true`. Es gab
keine Aktorfreigabe, keinen Flash- oder Partitionswechsel gegenüber dem
Build und keine Fach-/Safety-Codeänderung; dieser Nachweis ist rein additiv
zur bestehenden Software-Evidence. `HARDWARE_SMOKE_BRINGUP=PASS`,
`HARDWARE_SMOKE_RELEASE=PASS`.
