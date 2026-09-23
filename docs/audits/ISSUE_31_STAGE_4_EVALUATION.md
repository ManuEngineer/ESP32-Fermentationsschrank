# Issue #31 – Stage-4-Low-Level-Auswahl und Vergleich

Stand: 2026-09-23. Die Auswahl des Low-Level-Stacks wurde ownerseitig
festgeschrieben. Die bestehende Stage-2-/Stage-3-Hardware-Evidence wurde
wiederverwendet; identische manuelle Touchtests wurden nicht wiederholt.
Der neue actor-free Vergleichsrunner wurde auf dem Testboard gebaut,
geflasht und per UART erfasst. Es war keine Owner-Sichtprüfung erforderlich.

## Status

```text
ISSUE=31
PR=156
IMPLEMENTATION_SOURCE_HEAD=b433e34bd46eaa6c5c0ad13bf167ea5747e3b47c
APPROVED_PLAN_SHA=63fd88372b883887668566047c8a6acac48addcd
STAGE_0=PASS
STAGE_1=PASS
STAGE_2=PASS
STAGE_3=PASS
OPEN_STAGE_3_FINDINGS=0
STAGE_4=PASS
STAGE_4_SELECTION=OWNER_APPROVED
SELECTED_LOW_LEVEL=ESP_IDF_ESP_LCD_ILI9341_ESP_LCD_TOUCH_XPT2046
FALLBACK_CANDIDATE=NONE
LOW_LEVEL_BOUNDARY_IMPLEMENTATION=PASS
REPRESENTATIVE_26_SCREEN=PASS
LEAN_COMPARISON=PASS
LVGL_COMPARISON=PASS
OWNER_DECISION_REQUIRED=LEAN_VS_LVGL
PRODUCT_IMPLEMENTATION=NOT_STARTED
ACTUATOR_RELEASE=NO
```

Die Low-Level-Auswahl ist damit abgeschlossen. Offen bleibt ausschließlich
die Ownerentscheidung, ob die spätere produktive UI auf der Lean-Projektion
oder auf LVGL aufsetzt. Diese Evidence trifft diese Entscheidung nicht.

## Ownerauswahl und Provenienz

```text
STAGE_4_OWNER_SELECTION=APPROVED
SELECTED_LOW_LEVEL=ESP_IDF_ESP_LCD_ILI9341_ESP_LCD_TOUCH_XPT2046
FALLBACK_CANDIDATE=NONE
```

| Baustein | Aufgelöste Provenienz | Lizenz |
|---|---|---|
| ESP-IDF `esp_lcd` | `v6.1@fff9895c82d744c7237be8847347bdd1b07c6643` | Apache-2.0 |
| `espressif/esp_lcd_ili9341` | `2.1.0`, Commit `93460b932beba7022a8a4ec4186e0d6b7533d05f` | Apache-2.0 |
| `espressif/esp_lcd_touch` | `1.2.1`, Commit `c927778a85eed239dd403c1719d4f543ad56e693` | Apache-2.0 |
| `atanisoft/esp_lcd_touch_xpt2046` | `1.0.6`, Commit `05f4ecb82f19e4aa11b855a8e57539ba34e9629c` | MIT |
| `espressif/esp_lvgl_port` (nur Vergleich) | `2.9.0` | Apache-2.0 |
| `lvgl/lvgl` (nur Vergleich) | `9.6.0~1` | MIT |

`dependencies.lock` ist mit `SHA256=b43981b0aee6503510d94ba9014f71ab860d821fcf1c16e9a0e0034a00aec861`
festgehalten. `esp_bsp_generic` und alternative Grafik-/Touchstacks wurden
nicht erneut integriert; die definierte Reservebedingung ist nicht
eingetreten.

## Rendererunabhängige Grenze und repräsentativer Screen

Die additive portable Grenze liegt in
`lib/device_platform/src/device_ui_hardware_ports.hpp`. Sie enthält nur
rechteckigen Flush, Rotation, Backlight und typisierte Raw-Touchdaten.
Der konkrete ESP-IDF-Adapter liegt in
`lib/device_platform_esp_idf/src/esp_idf_display_touch_adapter.*`; ESP-IDF-,
ILI9341- und XPT2046-Typen bleiben privat. Die kleine
`main/fermentation_ui_renderer.*`-Schicht projiziert den bestehenden #26-
Workspace und benutzt dessen vorhandenen `press()`-Pfad. `fermentation_app`
bleibt frei von ESP-IDF-, LVGL- und konkreten Display-/Touchtypen.

Lean und LVGL verwenden denselben `RepresentativeScreen`, dieselben
DE/EN/ES-Textpacks, dieselben Status-/Info-/Control-Inhalte und denselben
`routePress()`-Rückweg. Rotation und Touch-Transform werden genau einmal im
Low-Level-Adapter angewandt. WakeOnly, Kalibrierpersistenz, finale Raw-
Schwellen und die Sicherheits-/Aktorlogik wurden nicht vorgezogen.

## Messvergleich

Die Werte stammen aus getrennten ESP32-WROOM-32E-4-MB-No-PSRAM-Builds mit
ESP-IDF 6.1, C++17 und identischen SSOT-Pins. Die Lean-Messung stammt aus dem
Lean-only-Runner; die LVGL-Messung aus dem aktivierten LVGL-Runner. UART-
Captures liegen unter:

```text
/tmp/issue31-lean-uart-final.log
/tmp/issue31-lvgl-uart-final.log
```

```text
LEAN_FUNCTIONAL_RESULT=PASS_RUNTIME_UART_RENDER_ADAPTER
LVGL_FUNCTIONAL_RESULT=PASS_RUNTIME_UART_RENDER_ADAPTER
LEAN_FLASH=0x38750_BYTES_IMAGE;TOTAL_IMAGE=231127
LVGL_FLASH=0x887a0_BYTES_IMAGE;TOTAL_IMAGE=558887
LEAN_DRAM_IRAM=DRAM_USED=14458;IRAM_USED=55679
LVGL_DRAM_IRAM=DRAM_USED=81018;IRAM_USED=57155
LEAN_FREE_MIN_LARGEST_HEAP=FREE_MIN=254648;LARGEST_8BIT=139264
LVGL_FREE_MIN_LARGEST_HEAP=FREE_MIN=168948;LARGEST_8BIT=98304
LEAN_TASK_STACK_HWM=MAIN_TASK=12816_WORDS
LVGL_TASK_STACK_HWM=MAIN_TASK=12784_WORDS;LVGL_TASK_CONFIG=7168_BYTES
LEAN_DMA_PARTIAL_BUFFER=PASS_320x8_PIXELS=2560;BYTES=5120;INTERNAL_DMA
LVGL_DMA_PARTIAL_BUFFER=PASS_320x20_PIXELS=6400;BYTES=12800;INTERNAL_DMA
LEAN_UPDATE_TIMING=251033_US
LVGL_UPDATE_TIMING=248501_US
LEAN_WATCHDOG_RESET_STABILITY=PASS_NO_RESET_SINGLE_RUN;STAGE3_LOW_LEVEL_REUSE=PASS
LVGL_WATCHDOG_RESET_STABILITY=PASS_NO_RESET_SINGLE_RUN;STAGE3_LOW_LEVEL_REUSE=PASS
LEAN_DEPENDENCY_CODE_FOOTPRINT=esp_lcd=3846;ili9341=2314;esp_lcd_touch=1531;xpt2046=1358;adapter=1547;main=7207
LVGL_DEPENDENCY_CODE_FOOTPRINT=lvgl=369662;lvgl_port=5240;esp_lcd=3983;ili9341=2314;esp_lcd_touch=1641;xpt2046=1358;adapter=1611;main=8183
```

Die Buildberichte weisen für Lean kein gelinktes LVGL-/`esp_lvgl_port`-
Archiv aus; die Komponenten werden wegen der direkten Vergleichsauflösung
mitgebaut, bleiben aber außerhalb des Lean-Images. LVGL benötigt dagegen
einen eigenen Port-Task, eine Mutex-Synchronisation und den konfigurierten
7.168-Byte-Taskstack. Lean nutzt keinen zusätzlichen UI-Task, Timer oder
UI-Mutex. Der gemessene LVGL-Zeitvorteil von rund 2,5 ms steht höheren
Flash-/DRAM-/Heapkosten gegenüber; daraus wird kein eindeutiger R1-Vorteil
abgeleitet:

```text
MEASURED_R1_ADVANTAGE_LVGL=INCONCLUSIVE
```

Die Stack-HWM-Messung betrifft bei beiden Läufen den Main-Task; der interne
LVGL-Port-Task wird durch den Port nicht als HWM-Wert herausgegeben und wird
deshalb nicht als gemessen behauptet. Die Einzelrunner meldeten
`rst:0x1 (POWERON_RESET)`, beide Funktions-PASS und
`ACTUATORS_DISABLED=PASS`; kein Panic-/Assert-/Watchdog-Reset wurde im
Capture beobachtet. Die belastbare Mehrzyklus-Watchdog-/Fehler-/Busstabilität
bleibt die unveränderte Stage-3-Matrix.

## Assets, Befehlsweg und Grenzen

Lean erzeugt keine externen Font- oder Bildassets; der begrenzte ASCII-
Glyphpfad verwendet die bestehende compile-time Textquelle. LVGL verwendet
für den Vergleich nur die vorhandene Standardfont-API, ohne neue Projekt-
Assets oder historische Laufdaten. DE/EN/ES-Textpacks, Info, Controls und
der bestehende #26-Command-Backpath sind in beiden Renderpfaden identisch.

`LVGL_DECISION=OWNER_DECISION_REQUIRED`, keine Produktintegration,
Kalibrierung, finale WakeOnly-/Recovery-Komposition, Stage-4-Auswahländerung,
Aktorfreigabe, Ready- oder Merge-Aktion wird aus diesem Vergleich abgeleitet.
