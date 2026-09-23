# Issue #31 – Stage-4-Low-Level-Evaluation

Stand: 2026-09-23. Diese Bewertung verwendet ausschließlich die bereits
vorhandene Stage-1-, Stage-2- und Stage-3-Evidence. Es wurden keine neuen
Firmware- oder Hardwaretests ausgeführt und keine verworfene Alternative
erneut integriert.

## Status und Entscheidungsgrenze

```text
ISSUE=31
PR=156
APPROVED_PLAN_SHA=63fd88372b883887668566047c8a6acac48addcd
PLAN_REVIEW=GO
FIX_VERIFICATION=PASS
STAGE_0=PASS
STAGE_1=PASS
STAGE_2=PASS
STAGE_3=PASS
OPEN_STAGE_3_FINDINGS=0
STAGE_4=EVALUATED
STAGE_4_SELECTION=OWNER_DECISION_REQUIRED
OWNER_NON_DECISION_STAGE_EXECUTION_AUTHORIZATION=YES
OWNER_STAGE_4_EXECUTION_AUTHORIZED=YES
PRODUCT_IMPLEMENTATION=NOT_STARTED
LVGL_DECISION=DEFERRED_AFTER_STAGE_4_OWNER_SELECTION
ACTUATOR_RELEASE=NO
```

Die Stage-4-Evaluation ist abgeschlossen. Sie liefert eine begruendete
Auswahlgrundlage, erteilt aber nicht die finale Ownerauswahl.

## Kandidatenbewertung

### Vorgeschlagene Low-Level-Grundlage: offizieller ESP-IDF-Stack

```text
PREFERRED_LOW_LEVEL_PROPOSAL=ESP_IDF_ESP_LCD_ILI9341_ESP_LCD_TOUCH_XPT2046
OFFICIAL_LOW_LEVEL_STACK=PASS
FALLBACK_CANDIDATE=NONE
```

Die Grundlage besteht aus:

| Baustein | Aufgeloeste Provenienz | Lizenz | Evidence |
|---|---|---|---|
| ESP-IDF `esp_lcd` | ESP-IDF `v6.1@fff9895c82d744c7237be8847347bdd1b07c6643` | Apache-2.0 | Stage 1 PASS |
| `espressif/esp_lcd_ili9341` | `2.1.0`, Commit `93460b932beba7022a8a4ec4186e0d6b7533d05f` | Apache-2.0 | Stage 1 PASS, Stage 2/3 PASS |
| `espressif/esp_lcd_touch` | `1.2.1`, Commit `c927778a85eed239dd403c1719d4f543ad56e693` | Apache-2.0 | Stage 1 PASS, Stage 2/3 PASS |
| `atanisoft/esp_lcd_touch_xpt2046` | `1.0.6`, Commit `05f4ecb82f19e4aa11b855a8e57539ba34e9629c` | MIT | Stage 1 PASS, Stage 2/3 PASS |

Die transitive `espressif/cmake_utilities`-Abhaengigkeit `0.5.3` wurde im
Stage-1-Lock mit Apache-2.0 nachgewiesen. Die vollstaendige Lock-Provenienz
bleibt durch `DEPENDENCIES_LOCK_SHA256=9ef854367f503de7c8bdde3c555d2fb63d38cbcaff82cff411ee99c8f4752250`
referenziert.

## Kriteriennachweis

| Stage-4-Kriterium | Ergebnis | Nachweis |
|---|---|---|
| Funktion | `PASS` | Stage 2: ILI9341-Farb-/Eckenausgabe, Controlleridentitaet `FUNCTIONAL_VISUAL_PASS`, XPT2046-Raw-Touch in Polling/IRQ; Stage 3: Wiederholungs- und Busmatrix PASS |
| Stabilitaet | `PASS` | Stage 3: 1000 kombinierte Draw-/Touch-/Buszyklen in Polling/IRQ ohne Bus-/Touchfehler, keine Watchdog-/Reset-/Speicherfehler |
| Ressourcen | `PASS` | Stage 3: DRAM/IRAM, Heap, groesster Block, Main-Task-HWM, interner DMA-Buffer, Timing und Flashnachweis in beiden Profilen |
| Reproduzierbarer Build | `PASS` | Stage 1: ESP-IDF 6.1, ESP32-WROOM-32E, C++17, 4 MB, kein PSRAM, exakte Lock-Aufloesung; Stage 3: Polling-/IRQ-Binaries und UART-Captures referenziert |
| Lizenz/Herkunft | `PASS` | Direkte und relevante transitive Komponenten mit Registry-/Commit-/SPDX-Nachweis in Stage 1 |
| Upstream-/Wartungsumfang | `PASS` | Offizielle ESP-IDF-/Espressif-LCD-/Touchpfade plus schmaler XPT2046-Adapter; kein BSP- oder kombinierter Komplettstack erforderlich |
| Adapter-/Integrationsumfang | `PASS` | Rendererunabhaengige Low-Level-Grenze; keine zweite UI-, Touch-, Recovery- oder Persistenzwahrheit; LVGL bleibt ausserhalb Stage 4 |

## Verworfenes und nicht erneut zu pruefendes Material

```text
ESP_BSP_GENERIC=DROPPED_WITH_REASON
ESP_BSP_GENERIC_REASON=NO_CONCRETE_BOARD_VALUE_AND_NO_XPT2046_SPI_PATH;ADDITIONAL_DEPENDENCIES
ALTERNATIVE_STACKS=DROPPED_WITH_REASON
ALTERNATIVE_STACKS_REASON=OFFICIAL_STACK_PASS_NO_RESERVE_CONDITION
LVGL_AND_ESP_LVGL_PORT=OUTSIDE_STAGE_4_SCOPE
```

`espressif/esp_bsp_generic` fuehrt zusaetzlichen generischen BSP-Umfang und
keinen passenden XPT2046-SPI-Pfad fuer das Board ein. LovyanGFX, TFT_eSPI,
LCDWiki, Arduino_GFX und Adafruit_GFX wurden nicht weitergebaut, weil der
offizielle Stack alle Stage-1-bis-3-Gates bestanden hat und keine
Reservebedingung eingetreten ist. Fuer diese Kandidaten wird kein ungepruefter
Lizenz-, Build- oder Ressourcen-PASS behauptet.

LVGL und `esp_lvgl_port` sind kein Stage-4-Low-Level-Kandidat. Die Entscheidung
zwischen Lean-Projektion und LVGL erfolgt erst nach der Ownerauswahl der
Low-Level-Grundlage und dem spaeteren identischen Screen-/Ressourcenvergleich.

## Ergebnis

Der offizielle ESP-IDF-/LCD-/Touch-/XPT2046-Pfad ist der einzige verbleibende
zulaessige Low-Level-Kandidat und wird als sachlicher Owner-Auswahlvorschlag
vorgelegt. Ein Fallback ist nicht erforderlich. Die finale Auswahl bleibt

```text
STAGE_4_SELECTION=OWNER_DECISION_REQUIRED
```

Bis zu dieser Ownerentscheidung bleiben Produktimplementation,
Adapter-/Composition-Integration, Kalibrierung, LVGL-/Lean-Auswahl und
Aktorfreigabe unveraendert ausstehend.
