# R1-RAM-Baseline – S2 Messbasis und Vorabmessung

Plan: `docs/tasks/memory-platform-course-plan.md` (ownerfreigegebene Plan-SHA
`2c20fe13d4d51a2bfabc00e630f1ad7c90c0e48a`), Schnitt S2, Abschnitt 3.

```text
S2_INSTRUMENTATION=PASS
BUILD_ESP32_RELEASE=PASS
BUILD_ESP32_BRINGUP=PASS
FINAL_S2_CODE_SHA=5bfc9bc0fb2f00b8ceac8d830bf000d04f8b37e7
O2=APPROVED
S2_TOUCH_PROVISIONING=PASS
PRODUCT_TOUCH_SMOKE=PASS_OWNER_OBSERVED
S2_BASELINE=BLOCKED_STOP_CONDITION_HEAP_ALLOC_FAILED_ABORT
O2_NETWORK_MODE_SWITCHES_COMPLETED=1_OF_5
```

Frühere Läufe (Vorab-Lauf `122c33e`, Erkundungslauf `0de006a` im Zustand
`ServiceRequired`) sind ausschließlich historisch und keine Baseline. Der
ownerfreigegebene O2-Lauf auf dem finalen S2-Stand ist Abschnitt 3b.



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

### Geflashter Stand für die Baseline

Die Quell-SHA wird beim CMake-Lauf aus `git rev-parse HEAD` eingebettet. Der
Flash (`idf.py flash`) hat das Release-Profil deshalb auf dem damaligen
Branch-HEAD `0de006a7d4d7d9f93a467df1ce0289e92f7ae603` neu gebaut; die
Firmware meldet entsprechend `source git sha: 0de006a7…`. Der Unterschied zu
`5bfc9bc0…` besteht ausschließlich aus Dokumentation: `git diff 5bfc9bc HEAD`
ändert nur `docs/ROADMAP.md` und diese Evidenzdatei, keine Buildeingabe
(`main`, `lib`, `src`, `include`, `CMakeLists.txt`, `sdkconfig.defaults*`,
`partitions`, `platformio.ini`, `dependencies.lock`: leerer Diff). Der
S2-Code-Stand ist damit unverändert `5bfc9bc`.

| Profil | App-BIN SHA-256 (geflasht) | ELF SHA-256 | sdkconfig SHA-256 (unverändert) |
|---|---|---|---|
| esp32_release | `dc593a9de2457820bb87c96ab0bab318af3535ba89800a71baf522e7d9a51fc3` | `364cf80d22e67ede38a4d7a1e0e7cf5b6a07fb2882ac73e64080a14b0782b3fb` | `6edbf61555023d42da33cdf212fbd1b6030b5af3c4b1203f5440cd6055b8e09f` |

Die Hashes der Tabelle oben (`89cc8e96…`, `fd098b6f…`) gehören zum Build direkt
auf `5bfc9bc`; sie unterscheiden sich nur durch die eingebettete SHA und die
davon abhängigen Metadaten. `esp32_bringup` wurde nicht neu gebaut.

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

## 3a. Erkundungslauf auf dem geflashten Stand (keine Baseline)

O2 (repräsentativer Lastpfad nach Plan Abschnitt 3 Punkt 6) ist vom Owner
freigegeben. Am Gerät war er nicht ausführbar. Der Owner bediente das Gerät
frei; Rohmitschnitt: [R1_RAM_BASELINE_S2_20261002_0DE006A_EXPLORATORY_UNSELECTED_RAW.txt](R1_RAM_BASELINE_S2_20261002_0DE006A_EXPLORATORY_UNSELECTED_RAW.txt)
(1096 Zeilen, 534 s ab Reset). Firmware `esp32_release`, Quell-SHA
`0de006a7…` (Code identisch mit `5bfc9bc`, siehe oben), NVS nicht gelöscht,
Touch-Kalibrierung `Available`, `hardware state: HARDWARE_UNVERIFIED`,
`application: service required`.

**Beobachtungen des Owners am Gerät** (nicht aus dem Log ableitbar):
- Die Anzeigesprache ist Englisch; das „EN“ im Header ist kein Button, ein
  Sprachwechsel war nicht möglich.
- Die Buttons für AP und Home ließen sich drücken, aber nicht aktivieren;
  es erschien kein QR-Code und im Setup war nichts Weiteres vorhanden.
- Die Rezeptseite war leer.
- Das Gerät stürzte bei allen Bedienungen nie ab.

**Aus dem Log:**
- Der Netzwerkmodus blieb während des gesamten Laufs `UNSELECTED`, der
  Netzwerkzustand `Stopped`. Damit gab es keinen Access Point (die QR-/
  SSID-Anzeige setzt `networkAccessPointInfo` voraus), keine `stable_*`-
  Punkte und keine Moduswechsel.
- 5 Touch-Dispatches mit `outcome=2` (`OwningOutcome`): Ein Netzwerkmodus-
  Press erreichte den Anwendungspfad; das Befehlsergebnis wird nicht
  geloggt, die Ablehnungsursache ist daher aus diesem Lauf **nicht**
  belegt.
- 13 Paare `network_page_press_before/after` mit `resources_lvgl`, ein
  `idle_120s` bei 180515 ms (120 s nach dem letzten Press), kein
  Failed-Alloc-Hook-Treffer.

| Größe | Wert im Lauf |
|---|---|
| freier Heap nach UI-Init / Ende | 106068 B / 105992 B |
| Minimum freier Heap (INTERNAL|8BIT = DMA) | 101608 B |
| größter Block (INTERNAL|8BIT) | 98304 B |
| Main-Task-Stack-HWM (Minimum) | 15920 B |
| LVGL-Pool maximal belegt | 14072 B (Pool 63384 B) |

**Aussage und Grenze:** Diese Werte belegen nur Boot, Seitennavigation und
Idle ohne Netzwerkmodus. Sie sind **keine Baseline**, da weder WLAN/HTTP
laufen noch die bekannten Peaks (Netzwerkmodus-Commit, Lastpfad)
erreicht wurden. Dass Sprachwechsel und Netzwerkmodus-Auswahl am Gerät
nicht funktionieren, liegt außerhalb von S2; es wurde nichts am
Produktverhalten geändert und der freigegebene Lastpfad nicht ersetzt.

## 3b. Ownerfreigegebener O2-Lauf auf dem finalen S2-Stand (2026-10-02)

Firmware `esp32_release`, Quell-SHA `0de006a7…` (Code identisch mit
`5bfc9bc`, Abschnitt „Geflashter Stand“), App-BIN `dc593a9d…`, ELF
`364cf80d…`. Testzustand: kontrolliert neuinitialisierter `state_store`,
Touchkalibrierung `Available` (Erstprovisionierung `2c71c4f`), Default-NVS/PHY
unberührt, keine erneute Provisionierung. Ein durchgehender UART-Mitschnitt
ab Reset: [R1_RAM_BASELINE_S2_O2_RUN_20261002_RAW.txt](R1_RAM_BASELINE_S2_O2_RUN_20261002_RAW.txt)
(993 Zeilen, Heartbeats entfernt), alle Messpunkte als
[R1_RAM_BASELINE_S2_O2_RUN_20261002_POINTS.csv](R1_RAM_BASELINE_S2_O2_RUN_20261002_POINTS.csv)
(alle geforderten Felder inkl. LVGL-Pool). Der Lauf wurde wegen
Stopbedingungen (`heap_alloc_failed`, `abort()`, unerwartete Resets)
abgebrochen; es wurde kein Fix implementiert.

**Boot:** `application: ready`, `touch calibration: active_status=Available
fallback_status=NotFound`, ein `POWERON_RESET` (durch den Mitschnitt-Start),
keine Panic/Watchdog/Brownout vor der Bedienung.

**Product-Touch-Smoke (Owner-Beobachtung):** Touchzuordnung stimmt, Navigation
durch alle erreichbaren Seiten korrekt: `PRODUCT_TOUCH_SMOKE=PASS`,
`TOUCH_ALIGNMENT=PASS`. `ONE_ACTION_PER_PRESS` und `GHOST_TOUCH=NO` wurden
nicht explizit gemeldet, es gab aber keine Auffälligkeit in der Navigation.

**O2-Ablauf (Owner-Beobachtung und Log):**

| O2-Schritt | Ergebnis |
|---|---|
| 10 Seitenwechsel | durchgeführt (alle erreichbaren Seiten), ohne Auffälligkeit |
| Sprachwechsel | `NOT_AVAILABLE_IN_CURRENT_R1_PATH`: das „EN“ im Header ist kein Button; nicht implementiert, nicht simuliert |
| Netzwerkmoduswechsel 1: `UNSELECTED` → `AP_ONLY` | **erfolgreich** (Log: `network_mode=AP_ONLY network_state=AccessPointOnly` bei `network_page_press_after`) |
| Browserzugriff `192.168.4.1` in `AP_ONLY` | erfolgreich (Owner) |
| Netzwerkmoduswechsel 2: `AP_ONLY` → `HOME_WIFI` | Whitescreen = Absturz (`abort()` nach `heap_alloc_failed`); nach dem Neustart steht `HOME_WIFI` persistiert |
| Weitere Wechsel 3–5 | nicht ausführbar; `O2_NETWORK_MODE_SWITCHES_COMPLETED=1_OF_5` |
| Browserzugriff im Modus `HOME_WIFI`/`SetupAccessPoint` | Absturz bzw. Seite lädt, danach Whitescreen (Owner) |
| 120 s Idle nach der Interaktion | nicht erreicht; nur Idle-Punkte vor und zwischen den Abstürzen |

**Resets:** 6 Boots, 5 `abort()` mit `SW_CPU_RESET`, 7
`heap_alloc_failed`-Zeilen, kein Watchdog, kein Brownout. Jeder Abort ist ein
C++-`operator new` ohne Heap (`__cxa_allocate_exception` → `abort`):

| Absturz | Zeitpunkt (ms im Boot) / Modus | fehlgeschlagene Allokation | dekodierte Stelle (ELF `364cf80d…`) |
|---|---|---|---|
| 1 | Boot 1, 384925, `AP_ONLY`, Heap-Minimum 3960 B | 251 B | `ConfigurationGraphStore::validationScan` → `validateProgramReferenceSemantically` → `loadReferencedRecord` → `decodeEnvelope` (`std::string`) |
| 2 | Boot 2, nach `periodic_30s`, `HOME_WIFI`, Minimum 2384 B | 1532 B, 1344 B | `updateProductUi` → `ProductiveLvglRenderer::render` → `makeRepresentativeScreen` → `vector<ScreenDrawCommand>::reserve` |
| 3 | Boot 3, nach `idle_120s`, `HOME_WIFI` | 2048 B | `makeRepresentativeScreen` → `vector::push_back/_M_realloc_append` |
| 4 | Boot 4, nach `periodic_30s`, `HOME_WIFI` | 722 B (caps 0x80c), 2048 B | `makeRepresentativeScreen` → `vector::push_back` |
| 5 | Boot 5, kurz nach `stable_home_wifi_setup_access_point` | 1350 B | `EspIdfHttpServerLifecycle::handleRequest` → `NetworkSetupRoutes::handle` (`std::string::operator=`) |

Welcher Auslöser (Browserzugriff, UI-Rendering) jeweils zuerst traf, ist
aus dem Log nicht eindeutig; die Tabelle nennt nur die dekodierte
Absturzstelle.

**Messpunkte (Auszug; vollständig in der CSV).** Werte in Bytes; `int-min` und
`dma-min` sind das Minimum der jeweiligen Capability-Sicht:

| Boot | ms | Punkt | Modus/Zustand | free | min free | größter 8-Bit-Block | Stack-HWM | int-min | dma-min | LVGL frei / max belegt / % / Frag |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 1175 | after_platform_begin | UNSELECTED/Stopped | 154876 | 154876 | 110592 | 20672 | 154876 | 154876 | – |
| 1 | 1225 | after_application_begin | UNSELECTED/Stopped | 123900 | 119800 | 110592 | 13936 | 119800 | 119800 | – |
| 1 | 1775 | after_ui_init | UNSELECTED/Stopped | 77380 | 71700 | 65536 | 13936 | 71700 | 71700 | 49980 / 13940 / 21 / 1 |
| 1 | 31825 | periodic_30s | UNSELECTED/Stopped | 77380 | 71700 | 65536 | 13936 | 71700 | 71700 | 49980 / 13940 / 21 / 1 |
| 1 | 121825 | idle_120s | UNSELECTED/Stopped | 77380 | 71700 | 65536 | 13936 | 71700 | 71700 | 49980 / 13940 / 21 / 1 |
| 1 | 179625 | network_page_press_before | UNSELECTED/Stopped | 76792 | 69644 | 65536 | 13936 | 69644 | 69644 | 52920 / 14072 / 17 / 6 |
| 1 | 180265 | network_page_press_after | AP_ONLY/AccessPointOnly | **11452** | 11412 | 11264 | 7072 | 11412 | 11412 | 52920 / 14072 / 17 / 6 |
| 1 | 180455 | stable_ap_only | AP_ONLY/AccessPointOnly | 11800 | 9212 | 8704 | 7072 | 9212 | 9212 | unavailable |
| 1 | 300575 | idle_120s | AP_ONLY/AccessPointOnly | 12160 | 8428 | 11264 | 7072 | 8428 | 8428 | 48576 / 15848 / 24 / 2 |
| 1 | 382455 | network_page_press_before | AP_ONLY/AccessPointOnly | 8040 | **3960** | **4352** | 7072 | 3960 | 3960 | 48576 / 15848 / 24 / 2 |
| 2 | 1725 | after_application_begin | HOME_WIFI/SetupAccessPoint | 56544 | 52572 | 51200 | 13936 | 52572 | 52572 | – |
| 2 | 2155 | after_ui_init | HOME_WIFI/SetupAccessPoint | 10264 | 5196 | 7680 | 13936 | 5196 | 5196 | unavailable |
| 2 | 2315 | stable_home_wifi_setup_access_point | HOME_WIFI/SetupAccessPoint | 10264 | 5180 | 7680 | 13936 | 5180 | 5180 | 49980 / 13940 / 21 / 1 |
| 2 | 32285 | periodic_30s | HOME_WIFI/SetupAccessPoint | 7456 | **2384** | 6400 | 13936 | 2384 | 2384 | 48568 / 15832 / 24 / 1 |
| 3 | 122245 | idle_120s | HOME_WIFI/SetupAccessPoint | 10432 | 4664 | 7680 | 13936 | 4664 | 4664 | 49980 / 13940 / 21 / 1 |

`stable_home_wifi` (verbundenes Heimnetz) wurde nicht erreicht;
`stable_home_wifi_setup_access_point` in jedem `HOME_WIFI`-Boot.

**Extremwerte des Laufs:**

| Größe | Wert |
|---|---|
| global niedrigstes `minimum_free_heap_bytes` | 2384 B (Boot 2, `periodic_30s`, `HOME_WIFI`) |
| kleinster beobachteter 8-Bit-Block | 4352 B (Boot 1, `AP_ONLY`) |
| niedrigste INTERNAL-/DMA-Reserve (Minimum) | 2384 B / 2384 B (identisch, kein PSRAM) |
| niedrigster Main-Task-Stack-HWM | 7072 B (ab `AP_ONLY`) |
| höchste LVGL-Poolbelegung | 24 % (`max_used` 15848 B von 63384 B) |
| `heap_alloc_failed` | ja, 7 Zeilen in 5 Abstürzen |
| Resets | 6 Boots, 5× `abort()`/`SW_CPU_RESET`, kein Watchdog, kein Brownout |

Auffällig im Verlauf (Zahlen, keine Ursachenbehauptung): Mit dem
Netzwerkmodus-Commit `UNSELECTED` → `AP_ONLY` sank der freie Heap von 76792 B
auf 11452 B und das Minimum danach bis 3960 B. In den `HOME_WIFI`-Boots
beträgt der freie Heap nach `after_application_begin` 56–57 kB und nach
`after_ui_init` rund 10 kB. LVGL-Pool unavailable trat 4-mal auf
(`pool=unavailable`; Ursache — Lock-Timeout oder noch nicht initialisiert —
aus dem Log nicht unterscheidbar).

**Ergebnis:** Die RAM-Baseline des Ist-Zustands ist gemessen und
reproduziert die im Plan beschriebene Heap-Erschöpfung im Netzwerkmodus,
jedoch als **abgebrochener, nicht vollständiger O2-Lauf** (1 von 5
Moduswechseln, kein 120-s-Idle nach der Interaktion, kein `stable_home_wifi`).
Es wird kein Mindestabstand und kein Budget festgelegt; das bleibt
S8/O4.

**Gerätezustand nach dem Lauf:** Der persistierte Netzwerkmodus ist
`HOME_WIFI` (aus dem neuinitialisierten, danach geänderten `state_store`);
das Gerät bootet seither jeweils mit rund 10 kB freiem Heap nach der
UI-Initialisierung.

## 4. Offen für die vollständige Baseline (Owner am Gerät)

- O2 ist freigegeben; der Lauf wurde in Abschnitt 3b wegen Stopbedingungen
  abgebrochen. Offen: Owner-/Reviewentscheid, wie die Baseline des
  Ist-Zustands gewertet wird (abgebrochener Lauf als Baseline oder
  Wiederholung in einem anderen Gerätezustand).
- Bedienung am Gerät: Netzwerkmodus auswählen (AP_ONLY und HOME_WIFI) für
  `stable_ap_only`/`stable_home_wifi`, Lastpfad (Seitenwechsel,
  Sprachwechsel, 5 Moduswechsel, ein Browserzugriff) mit
  `network_page_press_*`, danach 120 s Idle (`idle_120s`).
- Der Lauf wird mit der finalen S2-Code-SHA aufgenommen; danach Rohmitschnitt ergänzen und diese Evidenz um den Lauf erweitern.
