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
| Builder-Static-Analysis-Self-Check | `NOT_RUN` | nach den gezielten Tests auf finalem Builder-HEAD auszuführen |

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

Die vollständige Warnungs-/Deprecationsauswertung bleibt bis zum
ownerautorisierten Upgrade-Nachweis `NOT_RUN`.

## Vollständiger Upgrade-Nachweis und Hardware-Parität

Diese Nachweise sind in der Draft-/Builderphase absichtlich `NOT_RUN`:

| Nachweis | Status | Zuordnung |
|---|---|---|
| vollständiger v6.0.2-Baseline-Build beider Profile | `NOT_RUN` | ownerautorisierter vollständiger Upgrade-/Pre-Ready-Nachweis |
| vollständiger v6.1-Build beider Profile | `NOT_RUN` | ownerautorisierter vollständiger Upgrade-/Pre-Ready-Nachweis |
| `scripts/run_esp_idf_static_analysis.py all` | `NOT_RUN` | ownerautorisierter vollständiger Upgrade-/Pre-Ready-Nachweis |
| sdkconfig-Diff je Profil | `NOT_RUN` | zusammen mit den vollständigen Profilbuilds |
| Flash-/RAM-/Stack-/Heap-/Warnungs-/Versionsvergleich | `NOT_RUN` | bestehende Ressourcen- und Berichtseigner; keine neue Schwelle |
| Hardware-Parität | `NOT_RUN` | finaler Upgrade-HEAD: Issue-29-qualifiziertes Board/UART, sichere Bring-up-/Release-Smokes und Issue-90-Power-Cut/Restore/Product-Boot |

Die Hardware-Parität erweitert die bereits qualifizierte v6.0.2-Oberfläche
nicht. `IMPLEMENTED_DIGITAL_PENDING_HARDWARE`, `BLOCKED_HARDWARE`, Display,
Touch, Sensoren, Lüfter, BTS/Peltier und spätere Commissioning-Scope bleiben
außerhalb. Eine fehlende identische Board-/UART-/Restore-Oberfläche ist
`BLOCKED_HARDWARE`, kein Anlass für eine Ersatzhardware oder eine gelockerte
Safety-Policy.
