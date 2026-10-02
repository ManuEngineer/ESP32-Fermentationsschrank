# R1-RAM-Baseline – S2 Messbasis und Vorabmessung

Plan: `docs/tasks/memory-platform-course-plan.md` (ownerfreigegebene Plan-SHA
`2c20fe13d4d51a2bfabc00e630f1ad7c90c0e48a`), Schnitt S2, Abschnitt 3.

```text
S2_INSTRUMENTATION=IMPLEMENTED
BUILD_ESP32_RELEASE=PASS
BUILD_ESP32_BRINGUP=PASS
BASELINE_STATUS=INCOMPLETE_PENDING_OWNER
UNATTENDED_PRECHECK=DONE_UNSELECTED_BOOT_AND_IDLE
```

## 1. Umfang der Instrumentierung (reine Diagnose)

- `logResources()`: bestehende Schlüssel unverändert, angehängt
  `internal_8bit_*` (INTERNAL|8BIT) und `dma_*` (DMA) je freier Heap,
  Minimum und größter Block. Überlappende Sichten werden nicht addiert.
- `resources_lvgl:` je Messpunkt (außer vor LVGL-Init): Pool-Total, frei,
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

## 2. Builds

Frischer Build beider Profile über `scripts/build_esp_idf_profiles.py all`
(zuvor `build/esp32_*` gelöscht), ESP-IDF v6.1 (`fff9895c…`), 0 Warnungen,
Profilvalidierung PASS. Quell-SHA `122c33e6535b8ab443198c232e9dde75523409f5`
(in der Firmware eingebettet). Der Arbeitsbaum enthielt beim Build zusätzlich
nur die unbeteiligte, nicht committete `.codex/config.toml` (daher
`-dirty` in der IDF-App-Version); sie ist keine Buildeingabe.

| Profil | App-BIN SHA-256 | ELF SHA-256 | sdkconfig SHA-256 | DRAM | IRAM | Gesamt |
|---|---|---|---|---|---|---|
| esp32_release | `040bae8ffe04424d37c8a7f3f9b46266e1e7e98784c1530ce957ef0398005785` | `a2bf814e3ff2176731a1a915dfd1e92cbe273227a3c5ff8d0bd15ac8b4fdeda6` | `6edbf61555023d42da33cdf212fbd1b6030b5af3c4b1203f5440cd6055b8e09f` | 108907 / 180736 | 99579 / 131072 | 1572800 |
| esp32_bringup | `0e61a3ed76792999233652e422e582e9de59d9adfe9af8ad4a23ea9db349870f` | `95d04bb022ac2e0081d906d01f3601c82ba83722a463cd69a658b58d8adc11a5` | `044f9dcd1de26bd006e5208601479748c22667f074e2cebf3d38592989caa1f5` | 108907 / 180736 | 99579 / 131072 | 1584616 |

Effektive Konfiguration: `CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576`,
`# CONFIG_SPIRAM is not set`, LVGL-Allocator builtin mit 64-KiB-Pool.
Statisches DRAM belegt keinen Laufzeit-Heap.

## 3. Vorabmessung ohne Bedienung (keine Baseline)

Gerät: ESP32-Devboard, MAC `20:50:0d:1b:2f:34`, `/dev/ttyUSB0`. Der Owner hat
das Überschreiben dieses Geräts freigegeben. Geflasht: `esp32_release`
(`idf.py flash`, Quell-SHA `122c33e…`). **NVS-Zustand: nicht gelöscht**
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

**Aussage und Grenze:** Dies ist ausdrücklich **nicht** die Baseline. Das Gerät
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
- Danach Rohmitschnitt ergänzen und diese Evidenz um den Lauf erweitern.
