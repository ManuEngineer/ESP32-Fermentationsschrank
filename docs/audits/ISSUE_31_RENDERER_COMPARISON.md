# Issue #31 – Stage-4 Lean-vs.-LVGL-Vergleich

Stand: 2026-09-23. Der Vergleich wurde auf Basis der ownerseitig
festgeschriebenen Low-Level-Auswahl ausgeführt. Der Vergleich entscheidet nicht
zwischen Lean und LVGL.

```text
ISSUE=31
PR=156
IMPLEMENTATION_SOURCE_HEAD=b433e34bd46eaa6c5c0ad13bf167ea5747e3b47c
APPROVED_PLAN_SHA=63fd88372b883887668566047c8a6acac48addcd
STAGE_4=PASS
STAGE_4_OWNER_SELECTION=APPROVED
SELECTED_LOW_LEVEL=ESP_IDF_ESP_LCD_ILI9341_ESP_LCD_TOUCH_XPT2046
FALLBACK_CANDIDATE=NONE
LOW_LEVEL_BOUNDARY_IMPLEMENTATION=PASS
REPRESENTATIVE_26_SCREEN=PASS
LEAN_COMPARISON=PASS
LVGL_COMPARISON=PASS
MEASURED_R1_ADVANTAGE_LVGL=INCONCLUSIVE
OWNER_DECISION_REQUIRED=LEAN_VS_LVGL
ACTUATOR_RELEASE=NO
```

## Vergleichsbedingungen

Beide Runner verwenden dieselbe ESP32-WROOM-32E-4-MB-No-PSRAM-Hardware, den
ESP-IDF-6.1-Vertrag, dieselben SSOT-Pins, dieselbe ausgewählte
ILI9341-/XPT2046-Grundlage, dasselbe repräsentative #26-Screen-Modell, dieselben
DE/EN/ES-Texte, dieselben Eingabeelemente und denselben Command-Rückweg.
Rotation und Touch-Transformation werden genau einmal im gemeinsamen
Low-Level-Adapter angewandt. Die unveränderte Stage-2-/Stage-3-Evidence für
statische Touchfunktion und Low-Level-Stabilität wurde wiederverwendet; keine
identische manuelle Owner-Touchprüfung wurde wiederholt.

## Reproduzierbare Ergebnisse

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

Die Lean-Variante benötigt keinen zusätzlichen UI-Task, Timer oder UI-Mutex.
Die LVGL-Variante benötigt `esp_lvgl_port`, einen Port-Task, Mutex-
Synchronisation und den konfigurierten 7.168-Byte-Taskstack. Der LVGL-
Zeitvorteil von rund 2,5 ms steht höheren Flash-, DRAM- und Heap-Kosten
gegenüber; ein eindeutiger R1-Vorteil ist daraus nicht ableitbar.

Lean- und LVGL-Runner meldeten jeweils `PASS` und
`ACTUATORS_DISABLED=PASS`; in den UART-Captures trat kein Panic-, Assert- oder
Watchdog-Reset auf. Die Mehrzyklus-/Fehler-/Busstabilität bleibt die bereits
bestandene Stage-3-Matrix. Die Captures liegen unter:

```text
/tmp/issue31-lean-uart-final.log
/tmp/issue31-lvgl-uart-final.log
```

Die produktive Rendererwahl, WakeOnly-/Kalibrierungsintegration,
Produkt-Composition und Aktorfreigabe bleiben offen bzw. ausgeschlossen.
