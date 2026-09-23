# Issue #31 – Stage-4-Low-Level-Auswahl und Lean-vs.-LVGL-Vergleich

Stand: 2026-09-23. Diese Evidence dokumentiert die auf dem realen Testboard
ausgeführte Stage-4-Auswertung. Die produktive Lean-vs.-LVGL-Ownerentscheidung
wird nicht vorweggenommen.

## Status

```text
ISSUE=31
PR=156
TESTED_IMPLEMENTATION_HEAD=cc36e9dab5db3a0361b715027f7574526d4a8acb
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
RAW_TOUCH_CONTRACT=PASS
DMA_BUFFER_LIFECYCLE=PASS
REPRESENTATIVE_26_SCREEN=PASS
ARCHITECTURE_ROLE_GUARDS=PASS
LEAN_COMPARISON=PASS
LVGL_COMPARISON=PASS
R1_COMPATIBILITY_MATRIX=PASS
MEASURED_R1_ADVANTAGE_LVGL=INCONCLUSIVE
OWNER_DECISION_REQUIRED=LEAN_VS_LVGL
OWNER_RENDERER_SELECTION=NOT_YET_GRANTED
PRODUCT_IMPLEMENTATION=NOT_STARTED
ACTUATOR_RELEASE=NO
```

`STAGE_4_SELECTION=OWNER_APPROVED` bezeichnet ausschließlich die bereits
festgelegte Low-Level-Grundlage. `OWNER_DECISION_REQUIRED=LEAN_VS_LVGL` ist
die davon getrennte noch offene produktive Rendererentscheidung.

## Provenienz und Bedingungen

Die beiden Varianten wurden als getrennte actor-free ESP32-WROOM-32E-Profile
mit 4 MB Flash, ohne PSRAM, C++17, ESP-IDF
`v6.1@fff9895c82d744c7237be8847347bdd1b07c6643` und identischen SSOT-Pins
gebaut, geflasht und über UART erfasst. Die getestete Implementierung ist
`cc36e9dab5db3a0361b715027f7574526d4a8acb`; der vorherige
Implementierungscommit `b433e34…` ist nicht die Evidence-Provenienz.

| Baustein | Aufgelöste Provenienz | Lizenz |
|---|---|---|
| ESP-IDF `esp_lcd` | `v6.1@fff9895c82d744c7237be8847347bdd1b07c6643` | Apache-2.0 |
| `espressif/esp_lcd_ili9341` | `2.1.0`, Commit `93460b932beba7022a8a4ec4186e0d6b7533d05f` | Apache-2.0 |
| `espressif/esp_lcd_touch` | `1.2.1`, Commit `c927778a85eed239dd403c1719d4f543ad56e693` | Apache-2.0 |
| `atanisoft/esp_lcd_touch_xpt2046` | `1.0.6`, Commit `05f4ecb82f19e4aa11b855a8e57539ba34e9629c` | MIT |
| `espressif/esp_lvgl_port` (nur Vergleich) | `2.9.0` | Apache-2.0 |
| `lvgl/lvgl` (nur Vergleich) | `9.6.0~1` | MIT |

Der Component-Lock war bei beiden Builds unverändert:
`SHA256=b43981b0aee6503510d94ba9014f71ab860d821fcf1c16e9a0e0034a00aec861`.
`esp_bsp_generic` und alternative Grafik-/Touchstacks wurden nicht erneut
integriert; die definierte Reservebedingung trat nicht ein.

## Rendererunabhängige Grenze und gemeinsames Modell

`device_platform::IDisplayTouchPort` stellt nur bounded RGB565-Flush,
Rotation, Backlight und typisierte controller-native Raw-Touchdaten bereit.
Der ESP-IDF-Adapter kapselt Panel-, Touch- und SPI-Typen. Der XPT2046-Pfad
liefert Roh-X/Y, Kontakt-/Druckinformation, Status und Zeit ohne
Kalibrierung/Transformation; die Displayrotation liegt genau einmal im
Displaypfad. Die DMA-Fläche wird bis zum ESP-IDF-Completion-Callback
serialisiert und erst danach wiederverwendet.

Lean und LVGL verwenden dasselbe `RepresentativeScreen`, dieselben
`ThemeToken`-/Textpack-/Command-Verträge, dieselben DE/EN/ES-Textmodelle und
denselben `routePress()`-Rückweg. Das Modell enthält die 320x240-Shell, den
32-px-Header mit Branding/Logo, Sprache, WLAN und Uhrzeit, Content mit
Statusdarstellung, vier 80x40-Slots ab y=200 sowie Pressfeedback und den
strukturierten Status-/Pager-Fall. Es gibt keine zweite Theme-, Command- oder
Fachzustandswahrheit. `fermentation_app` bleibt frei von ESP-IDF-, LVGL- und
Treibertypen.

## Reproduzierbare Build- und UART-Evidence

```text
LEAN_BUILD=PASS
LEAN_BUILD_APP_BIN_BYTES=1159680
LEAN_BUILD_PARTITION_FREE=62_PERCENT
LVGL_BUILD=PASS
LVGL_BUILD_APP_BIN_BYTES=1488816
LVGL_BUILD_PARTITION_FREE=52_PERCENT

LEAN_FUNCTIONAL_RESULT=PASS
LEAN_DRAW_COMMANDS=21
LEAN_TEXT_BYTES=91
LEAN_FILLED_PIXELS=104085
LEAN_DISPLAY_SUBMISSIONS=21
LEAN_FRAME_SUBMIT_TIME_US=643858
LEAN_FRAME_FULLY_FLUSHED_TIME_US=710041
LEAN_FRAME_COMPLETION=PASS

LVGL_FUNCTIONAL_RESULT=PASS
LVGL_DRAW_COMMANDS=21
LVGL_TEXT_BYTES=91
LVGL_PARTIAL_BUFFER_PIXELS=6400
LVGL_TASK_STACK_BYTES=7168
LVGL_FRAME_SUBMIT_TIME_US=823998
LVGL_FRAME_FULLY_FLUSHED_TIME_US=990871
LVGL_FRAME_COMPLETION=PASS
LVGL_TASK_STACK_HWM_WORDS=4688
```

Die Lean-Adapterfläche beträgt `320x8` RGB565-Pixel im internen DMA-Speicher
(`5120` Bytes); der LVGL-Port verwendet einen `320x20`-Partialbuffer
(`6400` Pixel). Beide werden bis zur Transfer-Completion nicht mutiert.
`RAW_TOUCH_CONTRACT=CONTROLLER_NATIVE_NO_TRANSFORM` und
`DMA_DISPLAY_BUFFER=SERIALIZED_UNTIL_CALLBACK` wurden im UART-Lauf gemeldet.

### Full-Graph-Ressourcen

Die Werte stammen aus dem normalen actor-free Vergleichsgraphen nach Startup
und nach Rendereraktivierung, nicht aus einem verkürzten Hostmodell.

```text
LEAN_FULL_GRAPH_BASELINE_FREE_HEAP=201636
LEAN_FULL_GRAPH_BASELINE_MIN_FREE_HEAP=197664
LEAN_FULL_GRAPH_BASELINE_LARGEST_BLOCK=110592
LEAN_FULL_GRAPH_BASELINE_INTERNAL_FREE=232460
LEAN_FULL_GRAPH_BASELINE_IRAM_FREE=0
LEAN_AFTER_FREE_HEAP=184308
LEAN_AFTER_MIN_FREE_HEAP=167100
LEAN_AFTER_LARGEST_BLOCK=110592
LEAN_AFTER_INTERNAL_FREE=215132
LEAN_AFTER_IRAM_FREE=0
LEAN_MAIN_TASK_STACK_HWM_WORDS=6596

LVGL_FULL_GRAPH_BASELINE_FREE_HEAP=133028
LVGL_FULL_GRAPH_BASELINE_MIN_FREE_HEAP=129056
LVGL_FULL_GRAPH_BASELINE_LARGEST_BLOCK=110592
LVGL_FULL_GRAPH_BASELINE_INTERNAL_FREE=163764
LVGL_FULL_GRAPH_BASELINE_IRAM_FREE=0
LVGL_AFTER_FREE_HEAP=115336
LVGL_AFTER_MIN_FREE_HEAP=92088
LVGL_AFTER_LARGEST_BLOCK=110592
LVGL_AFTER_INTERNAL_FREE=146072
LVGL_AFTER_IRAM_FREE=0
LVGL_MAIN_TASK_STACK_HWM_WORDS=6592
LVGL_TASK_STACK_HWM_WORDS=4688
```

Beide UART-Captures meldeten `ACTUATORS_DISABLED=PASS`, keine Panic-/Assert-
und keinen Watchdog-Reset. Der anschließende normale Heartbeat blieb aktiv.
Die ausführliche Stage-3-Mehrzyklus-/Fehler-/Busmatrix bleibt die bereits
bestandene separate Evidence; sie wurde nicht als neue Stage-4-Messung
ausgegeben.

## R1-Kompatibilitätsmatrix

Diese Matrix ist eine Boundary-/Integrationsbewertung, keine vorgezogene
produktive UI- oder Kalibrierungsimplementierung.

| Vertrag | OWNER_LAYER | LEAN_GLUE | LVGL_GLUE | NEW_POLICY_DUPLICATION |
|---|---|---|---|---|
| typed `routePress()` / Command-Rückweg | `FermentationTouchWorkspace` / `FermentationApplication` | Immediate-mode-Zielauflösung und Weitergabe | LVGL-Event auf denselben typisierten Rückweg | NO |
| `WakeOnly` | bestehender Application-/Recovery-Owner | Touchsample nur weiterreichen | LVGL-Event nur weiterreichen | NO |
| Touchkalibrierung / Transformation | `device_platform`-Kalibriervertrag | controller-native Raw-Samples konsumieren | controller-native Raw-Samples konsumieren | NO |
| Raw-Touch-Recovery `>=10 s` | bestehender `SafeBoot`-/Application-Owner | keine Recoveryentscheidung | keine Recoveryentscheidung | NO |
| PIN-/Service-/Recovery-Screens | bestehende UI-/Command-/Recovery-Owner | Snapshot/Command-Projektion | Snapshot/Command-Projektion | NO |
| UI-Fehlerisolation gegen Regelung/Safety | `FermentationApplication` und Safety-Owner | Renderfehler melden, keine Freigabe | Renderfehler melden, keine Freigabe | NO |
| Backlight / Dimming | Composition und neutraler Backlight-Port | Portaufruf, kein eigener Zustand | Portaufruf, kein eigener Zustand | NO |

`R1_COMPATIBILITY_MATRIX=PASS` bedeutet, dass beide Darstellungsvarianten an
die bestehenden Ownergrenzen anschließen können. Die produktive Composition,
Kalibrierung, WakeOnly-/Recovery-Integration und Rendererwahl bleiben bis zur
Ownerentscheidung offen.

## Ergebnis und Stop-Gate

Der Lean-Pfad benötigt keinen zusätzlichen UI-Task. LVGL benötigt den
Port-Task, Mutex-Synchronisation und den konfigurierten 7168-Byte-Stack. Die
LVGL-Variante hat damit höhere Flash-/Heapkosten; die gemessenen vollständig
geflushten Framezeiten sind wegen der unterschiedlichen Pfadkosten kein
ausreichender eindeutiger R1-Vorteil:

```text
MEASURED_R1_ADVANTAGE_LVGL=INCONCLUSIVE
OWNER_DECISION_REQUIRED=LEAN_VS_LVGL
OWNER_RENDERER_SELECTION=NOT_YET_GRANTED
ACTUATOR_RELEASE=NO
```

Es wurde keine Produktimplementation, keine Stage-4-Auswahländerung, keine
Aktorfreigabe und kein Ready-/Merge-Schritt aus dieser Evidence abgeleitet.
