# Plan – Issue #159: ESP-IDF 6.0.2 auf 6.1 aktualisieren

## Planstatus und harte Basis

Dieser Plan ist ein eigenstaendiges Plan-only-Artefakt. Er startet keine
Implementation, keinen ESP-IDF- oder vollstaendigen Testlauf, keine Hardware-
pruefung und keine Ready-, Merge- oder Issue-Schlussaktion. Die Umsetzung
beginnt erst nach ausdruecklicher Ownerfreigabe der exakten Plan-Commit-SHA.

```text
ISSUE=159
TITLE=ESP-IDF 6.0.2 auf 6.1 aktualisieren
BASE_BRANCH=main
BASE_SHA=2c010e8a8be8e351f89b79ae6c74f665d24a1f0e
EXPECTED_TARGET_TAG=v6.1
EXPECTED_TARGET_COMMIT=fff9895c82d744c7237be8847347bdd1b07c6643
PLAN_STATUS=OWNER_PLAN_APPROVAL_PENDING
IMPLEMENTATION=NOT_STARTED
OWNER_PLAN_APPROVAL_REQUIRED=YES
PR160=DRAFT
PR160_BASE=main@2c010e8a8be8e351f89b79ae6c74f665d24a1f0e
PR160_ISSUE_REFERENCE=Refs_#159
PR158=OPEN_DRAFT_OUT_OF_SCOPE
ACTUATOR_RELEASE=NO
```

Verifizierte Kontextbaseline:

```text
CONTEXT_BASELINE_BRANCH=main
CONTEXT_BASELINE_SHA=2c010e8a8be8e351f89b79ae6c74f665d24a1f0e
CONTEXT_HEAD_SHA=2c010e8a8be8e351f89b79ae6c74f665d24a1f0e
CONTEXT_PLAN_SHA=THIS_VERSIONED_PLAN_COMMIT
CONTEXT_REFRESH_MODE=FULL
CONTEXT_DELTA=live Issue #159, PR #158, origin/main, Roadmap, Upgradevertrag,
  CI-/Workflowvertrag, aktive Produktionsvertraege, ESP-IDF-Adapter,
  Component-Manager-Lockfile und offizielle ESP-IDF-6.1-Migrationsquellen
SOURCE_OF_TRUTH_CONFLICT=Der aktive ESP-IDF-Upgradevertrag behauptet noch,
  dass keine externe Komponente und kein Lockfile existieren; main enthaelt
  bereits den gemergten DS3231-/I2C-Component-Manager-Pfad mit beidem.
ESP_IDF_BASELINE_PATH=/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.0.2
ESP_IDF_BASELINE_TAG=v6.0.2
ESP_IDF_BASELINE_COMMIT=7101770dc6db2667b3c477cc31365dd1acd6db4e
ESP_IDF_TARGET_PATH=/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.1
ESP_IDF_TARGET_TAG=v6.1
ESP_IDF_TARGET_COMMIT=fff9895c82d744c7237be8847347bdd1b07c6643
ESP_IDF_LOCAL_CHECKOUTS=AVAILABLE_AND_CLEAN_AT_PLAN_TIME
```

`origin/main` wurde live auf dieselbe SHA verifiziert. PR #160 ist als Draft
gegen `main` hergestellt und referenziert Issue #159 mit `Refs #159`. PR #158
basiert live auf `main@2c010e8...`, ist Draft und bleibt samt Issue #89,
WLAN-Onboarding, Provisioning-Spikes und deren Evidence ausserhalb dieses
Scopes. Der aktuelle Checkout fuer diesen Plan ist ein separater frischer
Clone; der bereitgestellte Checkout auf `agent/issue-89-wlan-onboarding-plan`
wird nicht als Basis benutzt.

Die vorhandene Datei `Agent-Auftraege/Auftrag.md` ist auf dieser Baseline nur
eine allgemeine Vorlage. Der konkrete Auftrag ist durch den Live-Inhalt von
Issue #159 und den vorliegenden Owner-Auftrag eindeutig bestimmt.

## 1. Ziel und Nicht-Ziele

### Ziel

Die einzige aktive, fixierte ESP32-Produktionsbasis wird auf ESP-IDF `v6.1`
am Commit `fff9895c82d744c7237be8847347bdd1b07c6643` aktualisiert. Beide
Produktionsprofile `esp32_bringup` und `esp32_release`, der bestehende
Component-Manager-Pfad, die fail-closed Profil-/Safetygrenzen und die
ESP-IDF-Static-Analysis bleiben dabei reproduzierbar und nachweisbar.

Die 6.0-auf-6.1-Migrations- und Breaking-Change-Pruefung wird gegen den
tatsaechlich auf `main` vorhandenen Code und nicht gegen PR #158 oder gegen
Kandidaten aus historischen Audits ausgefuehrt. Jede relevante Aenderung wird
als `NOT_AFFECTED` oder mit der konkreten, minimalen Anpassung und ihrem
Nachweis dokumentiert. Framework-/Toolchain-Deltas werden von echten
Produktverhaltensaenderungen getrennt.

### Nicht-Ziele

- keine WLAN-Onboarding-, Provisioning-, HTTP- oder MQTT-Implementierung und
  keine Kandidatenauswahl aus Issue #89 / PR #158;
- keine Aenderung an PR #158, dessen Branch, Spikes oder Evidence;
- keine neue Produktfunktion, kein OTA, kein Downloadpfad und keine
  Partitions-/Speicherreserve fuer spaetere Funktionen;
- keine Hardware-, GPIO-, Verdrahtungs-, Partitions- oder Aktorpolicyaenderung;
  ein zwingender 6.1-Bedarf wird als Ownerentscheidung blockiert;
- keine vorsorglichen Versionswrapper, globalen IDF-Versionszweige,
  Schatten-APIs oder zusaetzlichen Abstraktionsschichten;
- kein erzwungenes Upgrade von `esp-clang`, `pyclang` oder anderen Werkzeugen
  ohne konkreten v6.1-Kompatibilitaetsnachweis;
- keine globale Umbenennung historischer `v6.0.2`-Nachweise.

## 2. Verbindliche Quellen und Rollen

| Verantwortung | Wiederverwendete Quelle |
|---|---|
| aktuelle Reihenfolge, Status und PR-Sync | `docs/ROADMAP.md` |
| Workflow, Planfreigabe, Builder-/Reviewer-Gates | `AGENTS.md`, `docs/AGENT_WORKFLOW.md` |
| Repository-first, KISS, Ressourcen- und Espressif-first-Grundsaetze | `docs/ENGINEERING_PRINCIPLES.md` |
| Upgradeumfang, Minor-Gates, Lockfile und Hardware-Smoke | `docs/ESP_IDF_UPGRADE_CONTRACT.md` |
| Ausfuehrungszeitpunkt, Profile, Runner und Statusbegriffe | `docs/CI_AND_QUALITY_GATES.md`, `scripts/run_pre_ready_gates.sh` |
| Produktionsscope und TBD-Grenzen | `docs/SPECIFICATION_REVIEW.md`, `docs/OPEN_POINTS.md` |
| Modulrichtung | `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`, lokale `AGENTS.md` |
| Produktionsadapter und Composition Root | `lib/device_platform_esp_idf/`, `main/` |
| ESP-IDF-Pin und Profilnamen | `scripts/esp_idf_contract.py` |
| Profil-/Herkunftsvalidierung | `scripts/check_build_profiles.py` |
| Profilbuild | `scripts/build_esp_idf_profiles.py` |
| Ressourcen- und Provenienzbericht | `scripts/build_report.py` |
| ESP-IDF-Static-Analysis | `scripts/run_esp_idf_static_analysis.py` |

Die vollstaendigen Gatebefehle und die clang-tidy-Dateiliste werden nicht in
diesen Plan kopiert. Die Ausfuehrung erfolgt ausschliesslich ueber den bereits
verbindlichen Runner und die dort referenzierten Owner-Skripte.

Offizielle Espressif-Quellen fuer die Umsetzung und Evidence:

- [ESP-IDF Release v6.1](https://github.com/espressif/esp-idf/releases/tag/v6.1),
  insbesondere die offizielle Liste der v6.1-Breaking Changes;
- [Migration von 6.0 auf 6.1 – ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/migration-guides/release-6.x/6.1/index.html),
  einschliesslich der Unterseiten `peripherals.html` und `tools.html`;
- [ESP-IDF v6.1 `tools.json`](https://github.com/espressif/esp-idf/blob/v6.1/tools/tools.json)
  fuer die zum Release gehoerende `esp-clang`-Provenienz;
- die versionierten API-/Komponentendokumente der tatsaechlich verwendeten
  ESP-IDF- und Component-Manager-Komponenten.

## 3. Aktive Produktionsoberflaeche und historische Grenze

### 3.1 Aktive Oberflaeche, die aktualisiert oder verifiziert wird

Die folgenden Stellen tragen auf `main` den aktuellen Produktionsstand oder
werden von aktivem Code als aktueller Provenienz-/Testvertrag verwendet:

| Bereich | Baselinefund | Planwirkung |
|---|---|---|
| zentraler Pin | `scripts/esp_idf_contract.py` pinnt Tag und Commit auf 6.0.2 | auf `v6.1` und die Ziel-SHA aktualisieren |
| CMake-Fail-fast | Top-Level-`CMakeLists.txt` prueft `IDF_VER == v6.0.2` | auf `v6.1` pruefen und Fehlermeldung synchronisieren |
| CI-Provenienz | `.github/workflows/build.yml` klont, verifiziert und aktiviert 6.0.2 | Tag, Commit, Pfadnamen und aktive Beschreibung auf 6.1 aktualisieren |
| aktive Gate-Dokumentation | `docs/CI_AND_QUALITY_GATES.md`, `docs/ESP_IDF_UPGRADE_CONTRACT.md` | aktuellen Pin, Minor-Gates, Komponentenstatus und Static-Analysis-Provenienz synchronisieren |
| aktive Produkt-/Architektur-SSOT | `README.md`, `docs/ARCHITECTURE.md`, `docs/SPECIFICATION_REVIEW.md` | nur aktuelle Produktionsaussagen aktualisieren; Safety-/Hardwaregrenzen unveraendert lassen |
| laufendes Komponentenregister | `docs/THIRD_PARTY_COMPONENTS.md` | die Plattform-/NVS-Produktionszeilen auf 6.1 aktualisieren; historische Kandidatenbewertungen nicht neu etikettieren |
| reale Component-Manager-Basis | `lib/device_platform_esp_idf/idf_component.yml`, `dependencies.lock` | Manifest nur bei nachgewiesenem Bedarf aendern; Lockfile mit v6.1 generieren und vollstaendig pruefen |
| aktive Profile/Adapter | `sdkconfig.defaults`, `main/CMakeLists.txt`, `lib/device_platform_esp_idf/CMakeLists.txt`, `lib/device_platform_esp_idf/src/nvs_state_store.cpp` | aktuelle Kommentare und v6.1-relevante Adapterannahmen pruefen; keine Kconfig-/Hardwareaenderung ohne Bedarf |
| aktive Tool-/Testvertraege | `scripts/run_esp_idf_static_analysis.py`, `scripts/check_secrets.py`, `test/esp_idf_nvs_adapter_host/main/test_nvs_state_store.cpp` | aktuelle v6.1-Provenienz in Selftests/Metadaten aktualisieren; keine zweite Gatewahrheit erzeugen |

Der aktive Contract wird dabei zugleich auf den realen Stand korrigiert:
`main` enthaelt bereits `esp-idf-lib/ds3231` `1.1.7`, `i2cdev` `2.1.2`, die
transitive `esp_idf_lib_helpers`-Abhaengigkeit und `dependencies.lock`. Die
veraltete Aussage "keine externe ESP-IDF-Komponente" wird nicht als neue
Governance ersetzt, sondern durch diesen tatsaechlichen Bestand korrigiert.
Der Component-Manager-Contract bleibt: feste Versionen, generiertes Lockfile,
keine unfixierten Quellen und keine manuelle Lockfilepflege.

### 3.2 Bewusst historische Nachweise

Nicht global umbenannt werden:

- abgeschlossene Plaene unter `docs/tasks/`;
- historische `docs/ISSUE_29_BUILD_REPORT.md` und
  `docs/ISSUE_29_MEASUREMENTS.md`;
- alte Audit-/Evidence-Fassungen unter `docs/audits/`, soweit sie ihren
  damaligen Bewertungs- oder Messstand dokumentieren;
- historische `CHANGELOG.md`-Eintraege;
- historische Abschlussprovenienz in `docs/ROADMAP.md`, insbesondere der
  Eintrag zu PR #79;
- die supersedete ADR-001-Provenienz in `docs/DECISIONS.md`, sofern die
  damalige 6.0.2-Entscheidung als historische Entscheidung zitiert wird.

Die aktuelle Issue-159-Evidence wird als neue, klar datierte Evidenz erfasst
und nicht durch Umschreiben alter Messwerte erzeugt. `docs/ROADMAP.md` wird
mit dem etablierten Draft-PR #160 und dem Plan-first-Status synchronisiert;
der historische PR-79-Eintrag bleibt unveraendert.

## 4. Tatsachliche API-Oberflaeche auf `main`

Die Untersuchung auf der Baseline ergab folgende produktive ESP-IDF-
Oberflaeche:

- `main/app_main.cpp`: `nvs_flash_init_partition`,
  `nvs_flash_deinit_partition`, `esp_system`, `esp_timer`, SNTP/`esp_netif`
  und der vorhandene NVS-/RTC-/I2C-Composition-Root;
- `device_platform_esp_idf`: `esp_timer_get_time`, Reset-Cause,
  `nvs_open_from_partition`, `nvs_get_blob`, `nvs_set_blob`, `nvs_commit`,
  `nvs_close`, `esp_netif_sntp_*`, `sntp_get_sync_status` sowie die schmalen
  `ds3231`-/`i2cdev`-APIs;
- der nur separat aktivierbare Issue-90-Slice-7-Harness:
  `driver/uart.h`, `uart_is_driver_installed`, `uart_driver_install`,
  `uart_get_buffered_data_len` und `uart_read_bytes`;
- die externe, exakt gepinnte DS3231-/I2C-Komponente aus Manifest und Lockfile.

Nicht im aktuellen Produktions-`main` verwendet werden direkte SPI-Master-/
Slave-APIs, SPI-Flash-OS-Strukturen oder private Flash-Header, GPIO-ROM- oder
Deep-Sleep-Wakeup-APIs, LCD-/DSI-APIs, `esp_wifi`, `esp_http_server`,
Provisioning-/`protocomm`-/MQTT-APIs, PSA-opaque persistent keys oder
Secure-Boot-Curve-APIs. Die Issue-89-Probes auf PR #158 werden fuer diese
Feststellung nicht herangezogen.

## 5. Migration- und Breaking-Change-Matrix

Die folgende Matrix ist die verbindliche Struktur fuer die spaetere Evidence;
die erwarteten Statuswerte werden erst nach der v6.1-Pruefung als Ergebnis
festgeschrieben. `NOT_AFFECTED` bedeutet hier: kein tatsaechlich verwendeter
API-/Konfigurationspfad im Scope und kein Build-/Verhaltensdelta.

| Offizieller 6.1-Punkt | Tatsachlicher Repositorypfad | Geplanter Nachweis / Ergebnisregel |
|---|---|---|
| MQTT aus IDF in Component Manager verschoben | kein MQTT in `main`, Manifest oder aktivem Produktionsgraph | `NOT_AFFECTED`; keine Dependency ergaenzen |
| FreeRTOS-Header nicht mehr implizit aus Peripheral-Headern | optionaler Harness inkludiert `driver/uart.h` und eigene FreeRTOS-Header explizit | `NOT_AFFECTED`, sofern v6.1-Build des Harnesses ohne implizite Includes auskommt; keine globale Include-Aenderung |
| GPIO-Wakeup-API umbenannt | keine `gpio_deep_sleep_wakeup_*`-Nutzung | `NOT_AFFECTED` |
| GPIO-ROM-Praefixe und entfernte GPIO-Makros | keine direkte ROM-Funktion und keine genannten Makros | `NOT_AFFECTED` |
| LCD-Farbformat-/FourCC-/DSI-Aenderungen | kein LCD-/DSI-Produktcode; Kandidaten in Auditunterlagen sind keine aktive Integration | `NOT_AFFECTED`; keine Displayarchitektur vorziehen |
| SPI shared interrupt flag nicht mehr akzeptiert | kein `spi_bus_initialize`, keine SPI-Interruptflags im Produktcode | `NOT_AFFECTED` |
| SPI-Flash-OS-`start(flags)` und private Header | keine Custom-Flash-Treiber, keine `esp_flash_t`-Member und keine privaten Flash-Header | `NOT_AFFECTED` |
| `soc/uart_channel.h` entfernt | kein Include; Harness nutzt nur `driver/uart.h` und Standard-RX-APIs | `NOT_AFFECTED`; Harness-Build als gezielter Test |
| Legacy-UART-Wakeup-APIs deprecated | kein `uart_set_wakeup_threshold`/`uart_get_wakeup_threshold` und kein Light-Sleep-Wakeup | `NOT_AFFECTED`; keine Wakeup-Compatibility-Schicht |
| `idf.py flash` standardmaessig Fast-Reflash | kein aktiver automatischer `idf.py flash`-Produktionspfad; Issue-90-Runner nutzt seinen bestehenden esptool-Vertrag | `NOT_AFFECTED` fuer Firmwareverhalten; falls ein manueller v6.1-Flashnachweis benoetigt wird, bei leerem/erased Chip den offiziellen Vollflashmodus verwenden |
| NVS-Blob-/Commit-Pfad | produktiver `nvs_*`-Adapter, NVS-Partition und NVS-Host-/Consumer-Evidence | `REVIEW_REQUIRED`: v6.1-API, `nvs_commit`-Semantik, BLOB-Fehlerbereinigung, Entry-/Chunk-Konstanten und `mapSetError` gegen Quellen und Tests pruefen; fail-closed `CommitOutcomeUnknown` nicht abschwaechen |
| I2C-/DS3231-Component-Manager-Pfad | reale `ds3231`-/`i2cdev`-Adapter und Lockfile | v6.1-Profile plus gezielter Adapter-/Consumer-Build; bei unveraenderten APIs `NOT_AFFECTED`, sonst nur konkrete schmale Anpassung |
| Wi-Fi-/Netzwerk-Migration | nur SNTP/`esp_netif_sntp`-Koordination ist auf `main`; kein `esp_wifi`-Lifecycle | `NOT_AFFECTED` nach v6.1-Build; Issue #89 bleibt getrennt |
| HTTP-/Provisioning-/`protocomm`-/MQTT-Pfade | keine aktive Verwendung auf `main` | `NOT_AFFECTED`; PR #158 wird nicht aktualisiert |
| PSA persistente ECDSA-/HMAC-Schluessel | keine PSA-Key-API und keine solche Persistenz | `NOT_AFFECTED`; keine Security-Migrationslogik erfinden |
| Secure-Boot-192-bit-Kurve bzw. SoC-spezifische Secure-Boot-Aenderungen | keine Secure-Boot-Konfiguration/API im Repository; Ziel ist ESP32, nicht H2/C5/P4 | `NOT_AFFECTED`; keine Securitypolicy veraendern |
| neue P4-Defaultrevision und neue PSRAM-Funktionen | `CONFIG_IDF_TARGET="esp32"`, 4 MB, kein PSRAM | `NOT_AFFECTED`; keine PSRAM-/P4-Konfiguration ergaenzen |

Die NVS-Pruefung ist besonders wichtig: v6.1 veraendert intern die
Fehlerbereinigung mehrseitiger BLOB-Schreibvorgaenge, waehrend `nvs_commit`
weiterhin kein nachgelagerter persistenter Cache-Commit ist. Die bestehende
Mapping-Logik darf nur dann weniger konservativ werden, wenn v6.1-Quellcode,
gezielte Fehler-/Cut-Evidence und der bestehende Persistenzvertrag das
zweifelsfrei tragen. Andernfalls bleibt ein nicht sicher bestimmbarer Ausgang
`CommitOutcomeUnknown`; nur Kommentare/Quellstellen werden synchronisiert.
Die unveraenderten NVS-Seitengroessen, Entry-Grenzen und die bestehende
Partition bleiben durch Quellenvergleich und den vorhandenen
`check_issue90_partitions.py`-Vertrag zu bestaetigen.

## 6. Toolchain- und Static-Analysis-Delta

ESP-IDF v6.1s eigenes `tools/tools.json` enthaelt fuer `esp-clang` den
empfohlenen Stand:

```text
ESP_CLANG_TOOL_VERSION=esp-21.1.3_20260408
ESP_CLANG_LLVM_VERSION=21.1.3
ESP_CLANG_LINUX_AMD64_SHA256=6e62bf1973b57b5388aad281ce1463e953e70b3d8df74ef4668c70f31fbeda63
```

Die Erhoehung von `esp-clang` ist deshalb ein konkreter v6.1-Toolchain-
Kompatibilitaetsschritt: der bestehende Vertrag `esp-20.1.1_20250829` ist im
v6.1-`tools.json` nicht mehr der empfohlene/installierbare ESP-IDF-Stand. Die
zugehoerigen aktiven Static-Analysis-Selftests und der CI-/Dokumentations-
vertrag werden mit diesen nachgewiesenen Werten aktualisiert.

`pyclang` bleibt zunaechst auf dem bestehenden festen Projektwert `0.7.0`.
Die v6.1-Installation und die gezielte Werkzeugkompatibilitaet werden damit
verifiziert. Eine Aenderung erfolgt nur, wenn die reale v6.1-Ausfuehrung diesen
Stand nachweisbar nicht unterstuetzt; dann wird die konkrete feste kompatible
Version samt Quelle, API-/Plugin-Evidence und Diff als Ownerentscheidung bzw.
Planrevision vorgelegt. Der vollstaendige Lauf
`scripts/run_esp_idf_static_analysis.py all` gehoert ausschliesslich in den
ownerautorisierten vollstaendigen Upgrade-/Pre-Ready-Nachweis und ist kein
Draft-/Builder-Nachweis. Die nativen clang-format-/clang-tidy-18-Werkzeuge
werden durch das IDF-Minor-Upgrade nicht veraendert.

## 7. Umsetzungsschnitte nach Planfreigabe

### Schnitt A – exakte Baselines und Pinvertrag

1. Vor jedem semantischen Edit Branch, Plan-SHA, `main`-Ancestry und Live-
   Issue erneut pruefen. Bei einer anderen `main`-SHA wird angehalten und der
   Kontext inkrementell aktualisiert; diese Plan-SHA ist dann nicht direkt
   freigegeben.
2. Die parallel installierten, exakt verifizierten ESP-IDF-Checkouts unter
   `/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.0.2` und
   `/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.1` als
   Vergleichs- beziehungsweise Zielwerkzeug verwenden. Beide waren zum
   Zeitpunkt dieses Planstands am erwarteten Tag/Commit und mit sauberem
   Arbeitsbaum verifiziert; vor jedem Build werden Tag, Commit und Clean-
   Status erneut geprueft. Die ESP-IDF-Checkouts werden nicht in das
   Repository eingecheckt.
3. Den vollstaendigen v6.0.2-Baseline-Build beider Produktionsprofile erst
   als Teil des vollstaendigen, ownerautorisierten Upgrade-/Pre-Ready-
   Nachweises ausfuehren. In der Draft-/Builderphase wird kein vollstaendiger
   Produktprofilbuild vor dem Independent Review eingeplant; zulässig bleiben
   nur konkret betroffene Configure-, Component-Manager-/Lockfile- und
   Selftest-Pruefungen.
4. `scripts/esp_idf_contract.py`, Top-Level-`CMakeLists.txt` und den CI-
   Installations-/Herkunftsschritt auf v6.1/Zielcommit umstellen. Aktive
   Pfadnamen, Fehlermeldungen und Versionsmetadaten muessen konsistent sein.
5. Den Component Manager mit v6.1 ueber dem unveraenderten Manifest laufen
   lassen und das generierte `dependencies.lock` pruefen. Keine manuelle
   Lockfilepflege, keine unfixierte Version und kein stiller neuer Kandidat.
   Eine notwendige Aenderung von `idf_component.yml`-Versionen ist ein
   Library-/Owner-Gate und wird nicht still entschieden.

### Schnitt B – aktive Vertrage und gezielte Kompatibilitaet

1. Aktive Aussagen in README, Architektur, Spezifikationsreview, CI-, Upgrade-
   und Komponentenvertrag synchronisieren. Der Upgradevertrag wird dabei auf
   die tatsaechlich vorhandenen DS3231-/I2C-Abhaengigkeiten korrigiert.
2. Aktive Profile-/Adapterkommentare und v6.1-Metadaten in den bestehenden
   Selftests/Host-Evidence aktualisieren. Historische Plaene, Reports, Audits
   und Changelogzeilen bleiben unveraendert, ausser eine neue Issue-159-
   Evidence verweist lediglich auf sie.
3. `scripts/run_esp_idf_static_analysis.py` und
   `scripts/esp_idf_contract.py` auf die v6.1-`esp-clang`-Provenienz bringen;
   die bestehende Analyseauswahl, getrennte Buildpfade, `.clang-tidy` und
   `pyclang=0.7.0` bleiben inhaltlich unveraendert.
4. Gegen den v6.1-Checkout nur die Adapter- und Composition-Root-Oberflaeche
   sowie die unmittelbar betroffenen Selftests gezielt pruefen. Nur
   tatsaechlich erforderliche Include-/API-/Kconfig-Anpassungen werden
   vorgenommen. Keine vollstaendigen Produktprofilbuilds und keine Wrapper
   fuer ungenutzte LCD-, SPI-, Wi-Fi-, HTTP-, Provisioning- oder Security-APIs.
5. Den vorhandenen NVS-Adapter, die Entry-/Chunk-Kapazitaetsannahmen und den
   separaten Issue-90-Harness gezielt gegen v6.1 verifizieren. Eine zwingende
   Aenderung an NVS-Partition, GPIO, Aktorpolicy oder Persistenzvertrag stoppt
   diesen Schnitt und verlangt eine Ownerentscheidung bzw. Planrevision.

### Schnitt C – Vergleich, Evidence und Roadmap-Sync

1. Gegenueber der reproduzierten v6.0.2-Baseline je Profil die generierte
   `sdkconfig` vollstaendig diffen. Dieser Vergleich ist ein Bestandteil des
   vollstaendigen Upgrade-Nachweises und kein Draft-/Builder-Lauf. Jede neue,
   entfernte oder geaenderte Option wird als erwartbarer Framework-/Toolchain-
   Default, projektvertragliche Option oder unerwartete Produktwirkung
   klassifiziert. Overlays bleiben minimal und profilgetrennt.
2. Mit den bestehenden Ressourcen-/Berichtsowner-Skripten Flash, DRAM/IRAM,
   ELF/Map/App-BIN und vorhandene statische Stackwerte vergleichen. Dieser
   Ressourcenvergleich wird zusammen mit den vollstaendigen Profilbuilds im
   Upgrade-Nachweis ausgefuehrt. Die Hardware-Smoke-Messung liefert weiterhin
   nur die im Upgradevertrag vorgesehenen Heap-/Stack-/Heartbeat-/Reset-
   Kriterien; keine neue harte Schwelle wird aus dem Minor-Upgrade erfunden.
3. Buildlogs und Static-Analysis-Warnings nach neuen Deprecations oder
   unerwarteten Warnungen auswerten. Diese Auswertung erfolgt im
   vollstaendigen Upgrade-Nachweis; bekannte Framework-/Toolchainmeldungen
   werden von Produktwarnungen getrennt. Jede ungeklärte Warnung bleibt offen
   und blockiert den Abschluss.
4. Eine neue Evidence-Datei
   `docs/audits/ISSUE_159_ESP_IDF_6_1_UPGRADE_EVIDENCE.md` erstellen. Sie
   enthaelt keine eigene Gate-Governance, sondern nur Issue-159-Provenienz,
   die vollstaendige Migrationsmatrix, Lockfile-/sdkconfig-/Ressourcen-/
   Warnungsdelta, `PASS`/`FAILED`/`BLOCKED`/`NOT_RUN`-Nachweise und Verweise
   auf die bestehenden Ownervertraege.
5. `docs/ROADMAP.md` mit dem etablierten Draft-PR #160 und dem Plan-first-
   Status synchronisieren. Historische Roadmap-Abschlusszeilen bleiben
   unveraendert.

Die geplanten Implementierungsschnitte sind logisch klein genug fuer getrennte
Commits; die tatsaechlichen Commit-SHAs und der exakte Diff werden erst nach
der Planfreigabe erzeugt. Eine materielle Abweichung an Architektur,
Persistenz, Hardware, Security, Bibliotheksauswahl, Partitionen oder
Akzeptanzkriterien stoppt die Umsetzung vor dem Commit und erfordert eine
vollstaendige Planrevision mit neuer Ownerfreigabe.

## 8. Issue-159-Nachweise und Hardware-Paritaet

### Draft-/Builder-Nachweise nach der Planfreigabe

In der Draft-/Builderphase sind nur diese Issue-159-spezifischen, gezielten
Pruefungen vorgesehen:

- exakte Herkunftspruefung beider parallel verfuegbarer ESP-IDF-Checkouts
  (Tag, Commit, sauberer Arbeitsbaum) und gezielte v6.1-Tool-/Configure-
  Pruefung;
- Component-Manager-/Manifest-/Lockfile-Erzeugung und -Pruefung ohne
  vollstaendigen Produktprofilbuild oder zweiten Gatepfad;
- gezielter Build des bestehenden `esp32_bringup_issue90`-Harnesses fuer die
  tatsaechlich betroffene UART-/NVS-Oberflaeche;
- vorhandene NVS-Partition-/Kapazitaets-Selbsttests und direkt betroffene
  Python-/C++-Metadaten-Selftests;
- `git diff --check` sowie gezielte Format-/Testpruefungen fuer geaenderte
  Dateien. Ein vollstaendiger v6.0.2- oder v6.1-Produktprofilbuild und
  `scripts/run_esp_idf_static_analysis.py all` sind vor dem Independent
  Review nicht vorgesehen.

### Vollstaendiger Upgrade-Nachweis

Der bestehende `docs/ESP_IDF_UPGRADE_CONTRACT.md` bleibt alleiniger Owner fuer
die vollstaendige Reihenfolge und den Umfang. Erst im dort geregelten,
ownerautorisierten vollstaendigen Upgrade-/Pre-Ready-Nachweis werden der
vollstaendige v6.0.2-Baseline-Build beider Profile, der vollstaendige
v6.1-Build beider Profile, `scripts/run_esp_idf_static_analysis.py all`,
der vollstaendige `sdkconfig`-/Ressourcen-/Versions-/Warnungsvergleich und
die nachfolgende Hardware-Paritaet auf dem finalen Upgrade-HEAD ausgefuehrt.
Diese Aufzaehlung beschreibt nur die Issue-159-Deltas; die Gateprozedur wird
nicht erneut definiert. Nicht ausgefuehrte oder wegen Werkzeug-/Hardwaremangel
unmoegliche Nachweise bleiben `NOT_RUN` beziehungsweise `BLOCKED`.

### Eindeutiger Minor-Upgrade-Hardwareumfang

Der im Upgradevertrag zusaetzlich verlangte vollstaendige Hardware-
Paritaetstest wiederholt genau die bereits unter v6.0.2 real bestaetigte
Hardwareoberflaeche. Er erweitert sie nicht um unbestaetigte Hardware:

| Bereits real bestaetigte v6.0.2-Oberflaeche | v6.1-Paritaetsnachweis auf finalem Upgrade-HEAD |
|---|---|
| Issue #29: dasselbe ESP32-D0WD-V3-Board (Revision v3.1), 4 MB Flash, kein PSRAM, FTDI-FT232R-UART/USB mit reproduzierbarem ROM-Bootloader-Flash und DTR/RTS-Run-Reset | dieselbe Board-/Flash-/PSRAM-/UART-/Boot-Reset-Oberflaeche; anderer Boardtyp, andere Versorgung oder neue Hardware sind kein Ersatz und keine neue Anforderung |
| Issue #29: `esp32_bringup` und `esp32_release` im unbelasteten, sicheren Lauf mit deaktivierten realen Aktoren, Heartbeat/Uptime, genau zwei Ressourcenpunkten und Reset-/Panic-/Watchdog-/Brownout-Pruefung | dieselben beiden 35-s-Smokes nach dem bestehenden Upgradevertrag; unveraenderte Aktorpolicies und keine Aktorfreigabe |
| Issue #90 / PR #128: sechs reale Power-Cuts (`REAL_POWER_CUTS=6_OF_6_PASS`), Produktionsrestore und anschliessender Produktboot | dieselbe bestehende Power-Cut-/Restore-/Produktboot-Kampagne mit identischem Board-/UART-/Restore-Aufbau; kein neuer Cut-Typ und keine neue Safety-Policy |

Die zugrunde liegende #29-Evidence weist sichere unbelastete Pegelmessungen
und belastete MOSFET-/Verbraucherwirkung ausdrücklich als `NOT_RUN` aus; sie
sind deshalb kein v6.1-Paritaetsgate. Ebenso bleiben
`IMPLEMENTED_DIGITAL_PENDING_HARDWARE`, `BLOCKED_HARDWARE`, Display-, Touch-,
Sensor-, Luefter-, BTS7960- und Peltierpfade sowie spaetere Commissioning-
Scopes ausserhalb. Die im #29-Nachweis vorhandene Aktor-Inaktivitaet ist
Bestandteil der Paritaet, nicht eine neue Hardware- oder Safety-Anforderung.

Fehlt der exakt gleiche bereits qualifizierte Board-/UART-/Restore-Aufbau,
bleibt die Paritaet `BLOCKED_HARDWARE`; sie wird nicht durch einen neuen
Boardtyp, eine unbestaetigte Hardwareoberflaeche oder eine gelockerte
Safety-Aussage ersetzt.

## 9. Abbruchkriterien und erwarteter Endzustand

Die Umsetzung wird angehalten bei:

- abweichender `main`-Ancestry oder nicht freigegebenem Plan-Commit;
- nicht reproduzierbarer v6.1-Provenienz oder schmutzigem ESP-IDF-Checkout;
- erforderlicher Aenderung an Hardware, GPIO, Partition, Aktorfreigabe,
  Persistenz-/Recoveryvertrag, Securitypolicy oder Bibliotheksauswahl;
- unerwartetem Lockfile-/Component-Manager-Drift;
- nicht erklaertem `sdkconfig`-, Ressourcen-, Warnungs- oder Produktverhalten;
- fehlender aktueller NVS-/UART-/Adapterevidence oder fehlgeschlagenem
  Profil-/Static-Analysis-/Hardware-Gate.

Der Scope ist abgeschlossen, wenn:

- alle aktiven Pins und Produktionsvertraege v6.1 sowie die Ziel-SHA tragen;
- die einzige aktive ESP32-Produktionsbasis v6.1 ist, waehrend historische
  v6.0.2-Nachweise als solche erhalten bleiben;
- Manifest/Lockfile, beide Profile und `device_platform_esp_idf` unter v6.1
  konsistent sind;
- alle tatsaechlich relevanten 6.1-Migrationspunkte in der Issue-159-Evidence
  mit `NOT_AFFECTED` oder konkreter Anpassung abgeschlossen sind;
- sdkconfig-, Lockfile-, Ressourcen-, Stack-/Heap- und Warnungsdeltas sowie
  die Trennung von Framework-/Toolchain- und Produktwirkung erklaert sind;
- der bestehende Upgradevertrag die Issue-159-Nachweise ohne eine zweite
  Test- oder Workflow-Governance traegt;
- PR #158 unveraendert bleibt und erst nach diesem Upgrade auf dem neuen
  `main`-Stand seine eigene ESP-IDF-abhaengige Evidence erneuern kann.

Nach dem Plan-Commit werden Planpfad, exakte Plan-SHA und offene
Ownerentscheidungen im Owner-/Draft-PR-Kanal genannt. Danach haelt der Builder
an und wartet auf die ausdrueckliche Freigabe dieser exakten Plan-SHA.
