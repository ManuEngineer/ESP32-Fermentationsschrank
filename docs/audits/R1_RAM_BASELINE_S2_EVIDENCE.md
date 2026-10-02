# R1-RAM-Baseline – S2 Messbasis und Vorabmessung

Plan: `docs/tasks/memory-platform-course-plan.md` (ownerfreigegebene Plan-SHA
`2c20fe13d4d51a2bfabc00e630f1ad7c90c0e48a`), Schnitt S2, Abschnitt 3.

```text
S2_INSTRUMENTATION=IMPLEMENTED
BUILD_ESP32_RELEASE=PASS
BUILD_ESP32_BRINGUP=PASS
FINAL_S2_CODE_SHA=5bfc9bc0fb2f00b8ceac8d830bf000d04f8b37e7
BASELINE_STATUS=NOT_STARTED_PENDING_OWNER_O2_AND_DEVICE
PRECHECK_122C33E=HISTORICAL_ONLY_NOT_A_BASELINE
```

`122c33e` war der erste S2-Stand; die stabilen Netzwerkpunkte loggten dort den
LVGL-Pool noch nicht. Der Fix `5bfc9bc` behebt das. Die vollständige Baseline
wird ausschließlich mit der finalen S2-Code-SHA `5bfc9bc…` (oder einer später
dokumentierten, noch finaleren) aufgenommen.

## 1. Umfang der Instrumentierung (reine Diagnose)

- `logResources()`: bestehende Schlüssel unverändert, angehängt
  `internal_8bit_*` (INTERNAL|8BIT) und `dma_*` (DMA) je freier Heap,
  Minimum und größter Block. Überlappende Sichten werden nicht addiert.
- `resources_lvgl:` an allen Messpunkten nach LVGL-Init, einschließlich
  `stable_ap_only`, `stable_home_wifi`, `stable_home_wifi_setup_access_point`
  und `network_page_press_*` (ab `5bfc9bc`; vor LVGL-Init nicht verfügbar): Pool-Total, frei,
  größter Block, Maximum, Belegung und Fragmentierung. Nur bei aktivem
  eingebautem LVGL-Allocator (`lv_mem_monitor()` unter `lvgl_port_lock(100)`),
  sonst `pool=unavailable`. Im erzeugten `sdkconfig` beider Profile:
  `CONFIG_LV_USE_BUILTIN_MALLOC=y`, `CONFIG_LV_USE_STDLIB_MALLOC=0`
  (builtin), `CONFIG_LV_MEM_SIZE=65536`; `lv_mem_monitor` ist im ELF gelinkt.
- Neue Messpunkte: `after_platform_begin`, `after_application_begin`,
  `after_ui_init`, `idle_120s`. **Entscheidung:** der bisherige Punkt
  `startup` liegt an derselben Stelle wie der geforderte `after_ui_init` und
  wurde umbenannt statt dupliziert (kein Konsument des Namens im Repository).
  `idle_120s` feuert 120 s ohne Bedienung seit Boot bzw. seit dem letzten
  Touch-Press und wird nach jedem Press neu scharf (`updateProductUi()` meldet
  dazu die bereits vorhandene `freshPressEdge`).
- `platform.begin() && application.begin()` wurde aufgeteilt; der
  Kurzschluss bleibt erhalten.
- `heap_caps_register_failed_alloc_callback()` ist die erste Anweisung in
  `app_main()`; der Hook ist `IRAM_ATTR` und gibt nur über `ESP_DRAM_LOGE` mit
  `DRAM_STR` Größe, Caps und Funktionsname aus. Auch erwartete
  Fehlschläge (nothrow-new, Capability-Fallbacks) lösen ihn aus.
  `CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS` ist nicht gesetzt.
- `scripts/build_report.py` blieb unverändert: Der bestehende JSON2-Pfad
  liefert statisches DRAM/IRAM bereits; eine Aufteilung je Komponente ist für
  diese Baseline nicht nötig.

## 2. Builds (finaler S2-Stand `5bfc9bc`)

Frischer Build beider Profile über `scripts/build_esp_idf_profiles.py all`
(zuvor `build/esp32_*` gelöscht), ESP-IDF v6.1 (`fff9895c…`), 0 Warnungen,
Profilvalidierung PASS. Quell-SHA `5bfc9bc0fb2f00b8ceac8d830bf000d04f8b37e7`
(in der Firmware eingebettet). Der Arbeitsbaum enthielt beim Build zusätzlich
nur die unbeteiligte, nicht committete `.codex/config.toml` (daher
`-dirty` in der IDF-App-Version); sie ist keine Buildeingabe.

| Profil | App-BIN SHA-256 | ELF SHA-256 | sdkconfig SHA-256 | DRAM | IRAM | Gesamt |
|---|---|---|---|---|---|---|
| esp32_release | `89cc8e967f3c59402c80e289d2c8300caf181c55e86427597d6de853a7b5d28e` | `fd098b6fd1e2b2e4733da135da5abed7f0528bed193d45da58f332355e523d4b` | `6edbf61555023d42da33cdf212fbd1b6030b5af3c4b1203f5440cd6055b8e09f` | 108907 / 180736 | 99579 / 131072 | 1572820 |
| esp32_bringup | `f4dc7acd5a24c1efc997f536ac8c1cdf5bf19027797ee74761fbb292f53a9290` | `a55527a8a714987c1ee156d868daffa0e0e0719eed6c1688b6a926c9de57e7c3` | `044f9dcd1de26bd006e5208601479748c22667f074e2cebf3d38592989caa1f5` | 108907 / 180736 | 99579 / 131072 | 1584636 |

Effektive Konfiguration (aus dem erzeugten `sdkconfig`; in beiden Profilen
identisch für die hier genannten Werte):

| Bereich | Werte |
|---|---|
| Main-Task / PSRAM | `CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576`; `# CONFIG_SPIRAM is not set`; `# CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS is not set` |
| LVGL | `CONFIG_LV_USE_BUILTIN_MALLOC=y`, `CONFIG_LV_USE_STDLIB_MALLOC=0` (builtin), `CONFIG_LV_MEM_SIZE=65536` |
| WLAN | `STATIC_RX_BUFFER_NUM=10`, `DYNAMIC_RX_BUFFER_NUM=32`, `TX_BUFFER_TYPE=1` (dynamisch), `DYNAMIC_TX_BUFFER_NUM=32`, `RX_MGMT_BUF_NUM_DEF=5`, `MGMT_SBUF_NUM=32`, `AMPDU_TX/RX_ENABLED=y`, `TX/RX_BA_WIN=6`, `WIFI_IRAM_OPT=y`, `WIFI_RX_IRAM_OPT=y` (Präfix `CONFIG_ESP_WIFI_`) |
| lwIP | `MAX_SOCKETS=10`, `TCPIP_RECVMBOX_SIZE=32`, `TCP_MSS=1440`, `TCP_SND_BUF_DEFAULT=5760`, `TCP_WND_DEFAULT=5760`, `TCP_RECVMBOX_SIZE=6`, `UDP_RECVMBOX_SIZE=6`, `TCPIP_TASK_STACK_SIZE=3072`, `# LWIP_IRAM_OPTIMIZATION is not set` (Präfix `CONFIG_LWIP_`) |

Statisches DRAM belegt keinen Laufzeit-Heap.

## 3. Historische Vorabmessung auf `122c33e` (keine Baseline)

Gerät: ESP32-Devboard, MAC `20:50:0d:1b:2f:34`, `/dev/ttyUSB0`. Der Owner hat
das Überschreiben dieses Geräts freigegeben. Geflasht: `esp32_release`
(`idf.py flash`, Quell-SHA `122c33e…`, inzwischen überholt). **NVS-Zustand: nicht gelöscht**
(Touch-Kalibrierung `active_status=Available`), Netzwerkmodus beim Boot
`UNSELECTED`, Netzwerkzustand `Stopped`, Meldung `application: service
required`. Rohmitschnitt: [R1_RAM_BASELINE_S2_20261002_122C33E_UNATTENDED_RAW.txt](R1_RAM_BASELINE_S2_20261002_122C33E_UNATTENDED_RAW.txt),
150 s ab Reset, ohne Berührung.

| Punkt | t seit Boot (ms) | free heap | min free | größter Block (INTERNAL/8BIT) | DMA frei | DMA min | Stack-HWM | LVGL-Pool frei/Max belegt |
|---|---|---|---|---|---|---|---|---|
| after_platform_begin | 1175 | 154744 | 154744 | 110592 | 154744 | 154744 | 20688 | n/a (vor LVGL) |
| after_application_begin | 1205 | 153200 | 153140 | 110592 | 153200 | 153140 | 17296 | n/a (vor LVGL) |
| after_ui_init | 1755 | 106068 | 101632 | 98304 | 106068 | 101632 | 17152 | 49980 / 13940 |
| periodic_30s | 31805 | 106068 | 101608 | 98304 | 106068 | 101608 | 16720 | 49980 / 13940 |
| idle_120s | 121805 | 106068 | 101608 | 98304 | 106068 | 101608 | 16720 | 49980 / 13940 |

Der Failed-Alloc-Hook meldete in diesem Lauf keine Allokationsfehler.

**Aussage und Grenze:** Diese Messung stammt vom Vorstand `122c33e`, nicht vom finalen S2-Stand, und ist ausdrücklich **nicht** die Baseline. Das Gerät
blieb in `UNSELECTED`/`Stopped`, es gab weder WLAN noch HTTP noch einen
`stable_*`-Punkt, und die bekannten Fehlerbilder (Heap-Minimum 2576 B im
Netzwerkmodus-/Lastpfad, Abschnitt 2.2 des Plans) wurden nicht berührt. Die
Werte belegen nur die Boot- und Idle-Situation vor der Netzwerkmodus-Auswahl;
INTERNAL|8BIT und DMA sind auf diesem Board identisch (alle interne DRAM).
Ein Vergleich mit den historischen Messankern ist nicht zulässig.

## 4. Offen für die vollständige Baseline (Owner am Gerät)

- O2: Freigabe des repräsentativen R1-Lastpfads (Plan Abschnitt 3, Punkt 6).
- Bedienung am Gerät: Netzwerkmodus auswählen (AP_ONLY und HOME_WIFI) für
  `stable_ap_only`/`stable_home_wifi`, Lastpfad (Seitenwechsel,
  Sprachwechsel, 5 Moduswechsel, ein Browserzugriff) mit
  `network_page_press_*`, danach 120 s Idle (`idle_120s`).
- Der Lauf wird mit der finalen S2-Code-SHA aufgenommen; danach Rohmitschnitt ergänzen und diese Evidenz um den Lauf erweitern.
