# Issue #31 – Stage-3-Funktions-, Fehler- und Ressourcen-Evidence

Stand: 2026-09-23. Diese Evidence bezieht sich auf den unveraenderten
Produkt-HEAD und einen externen, nichtproduktiven actor-free Matrix-Harness.
Es wurden keine Produktquellen, Boardprofile oder Reset-/GPIO-Vertraege
geaendert.

## Status

```text
ISSUE=31
PR=156
FIRMWARE_SOURCE_HEAD=882c3364b59bef0f21576bf6dd4bbabe516995dc
DOCUMENTATION_BASELINE_HEAD=882c3364b59bef0f21576bf6dd4bbabe516995dc
APPROVED_PLAN_SHA=63fd88372b883887668566047c8a6acac48addcd
PLAN_REVIEW=GO
FIX_VERIFICATION=PASS
ESP_IDF=v6.1@fff9895c82d744c7237be8847347bdd1b07c6643
TARGET=ESP32-WROOM-32E
FLASH=4MB
PSRAM=NONE
ACTUATORS_DISABLED=YES
RESET_TOPOLOGY=MSP2807_RESET->EN_CHIP_PU_UNCHANGED
FT232_RTS_EN=REMOTE_FLASH_DEBUG_ALLOWED
PRODUCT_REPRESENTATIVE_COLDSTART=RTS_EN_DISCONNECTED
STAGE_2_TOUCH_STATIC_EVIDENCE=REUSED_WITHOUT_RETEST
STAGE_3=PASS
OPEN_STAGE_3_FINDINGS=0
STAGE_4=NOT_RUN
ACTUATOR_RELEASE=NO
```

Die vorhandene Stage-2-Evidence wird unveraendert wiederverwendet fuer:

```text
TOUCH_POSITION_MATRIX=REUSED_STAGE2_EVIDENCE
TOUCH_POSITION_POINTS=TOP_LEFT,TOP_RIGHT,BOTTOM_LEFT,BOTTOM_RIGHT,CENTER
TOUCH_POSITION_PROFILES=POLLING_PASS,IRQ_PASS
MANUAL_STAGE_2_TOUCH_RETEST=SKIPPED_BY_REUSE_BEFORE_RETEST
```

Damit werden die bereits nachgewiesene statische Touchfunktion, die
Controlleridentitaet und die fünf vorhandenen Raw-Touch-Positionen nicht als
neue Stage-3-Messung ausgegeben.

## Stage-3-Zusatzmatrix

Der externe Harness lag unter `/tmp/issue31-stage3-matrix` und verwendete
ausschliesslich den Stage-1-PASS-Stack:

```text
espressif/esp_lcd
espressif/esp_lcd_ili9341=2.1.0
espressif/esp_lcd_touch=1.2.1
atanisoft/esp_lcd_touch_xpt2046=1.0.6
DEPENDENCIES_LOCK_SHA256=9ef854367f503de7c8bdde3c555d2fb63d38cbcaff82cff411ee99c8f4752250
HARNESS_SOURCE_SHA256=2f051fadd2ba0c9415d37e793bc64268af1a01cda238d548e73bb8417c418899
POLLING_BINARY_SHA256=ba8dcd6de894b6502d49e57d87e1edb907caddd6d9153758e0bcc2f5cd2471fd
POLLING_BINARY_SIZE=199632
IRQ_CONFIG_SHA256=5a541483837314ca294645343b627531fd8d2e26f7e4a5969c9b2ebeb37ab97b
IRQ_BINARY_SHA256=6454dd443daf2f236d0963c4adc80591ef137bdfe5203289beea3783d2976a42
IRQ_BINARY_SIZE=199808
POLLING_UART_LOG_SHA256=2a62327cb40084e35ce08acb4abbbacd19b2843c2dc07af9eb9014b92d34ff8c
IRQ_UART_LOG_SHA256=1d2be25ffe8e1a72f6613eb08998715212a5427220498d16159a9467f4795b37
```

Board-/SSOT-Leitungen und Reset blieben unveraendert:

```text
SPI_SCK=18
SPI_MOSI=23
SPI_MISO=19
TFT_CS=5
TOUCH_CS=15
DISPLAY_DC=2
BACKLIGHT=4_ACTIVE_HIGH
TOUCH_IRQ=39
DISPLAY_RESET=EN_SHARED
```

### Polling und IRQ

```text
DISPLAY_REPEAT_MATRIX=PASS_POLLING_100_OF_100_AND_IRQ_100_OF_100
TOUCH_DRAW_BUS_STRESS=PASS_POLLING_AND_IRQ_1000_NO_ERRORS_CONTACTS_0
FAULT_INJECTION_MATRIX=PASS_POLLING_AND_IRQ_8_OF_8_FAIL_CLOSED
```

Jedes Profil initialisierte den offiziellen ILI9341-/XPT2046-Pfad, den
gemeinsamen SPI-Bus mit getrennten CS-Leitungen und einen internen DMA-
Displaybuffer. Der Harness meldete keinen Reset, Watchdog, Bus- oder
Speicherfehler. Die acht absichtlich ungueltigen Null-/uninitialisierten
Panel-, Bus-, CS-, Reset-, Touch- und Backlight-Aufrufe wurden mit
Fehlerstatus fail-closed abgewiesen.

Die kombinierte 1000-Zyklen-Pruefung belegt Draw-/Touch-/Bus-Stabilitaet und
Fehlerfreiheit unter wechselnden Zugriffen. `contacts=0` ist dabei bewusst
kein neuer Touch-Positions- oder Drucknachweis; die statische Touch-Evidence
kommt ausschliesslich aus Stage 2.

### Ressourcen und Timing

```text
FLASH_RESOURCE_EVIDENCE=PASS
FLASH_APP_PARTITION=0x100000
FLASH_BINARY_FREE=0xcf430_81_PERCENT
DRAM_IRAM_EVIDENCE=PASS_POLLING_AND_IRQ
HEAP_MIN_LARGEST_BLOCK=PASS_POLLING_AND_IRQ
TASK_STACK_HWM=PASS_MAIN_TASK;OFFICIAL_STACK_CREATES_NO_DEDICATED_LOW_LEVEL_TASK
DMA_DISPLAY_BUFFER_EVIDENCE=PASS_640_BYTES_INTERNAL_DMA_NO_PSRAM
DISPLAY_UPDATE_TIMING=PASS_POLLING_AND_IRQ_NO_DRAW_ERRORS
WATCHDOG_RESET_EVIDENCE=PASS_POLLING_AND_IRQ_NO_RESET_DURING_RUN
```

Gemessene Endwerte aus den UART-Captures:

```text
POLLING_FREE_HEAP=299236
POLLING_LARGEST_8BIT=172032
POLLING_LARGEST_DMA=172032
POLLING_DRAM=299236
POLLING_IRAM=201476
POLLING_MAIN_STACK_MIN_WORDS=2616
POLLING_DRAW_CALLS=212400
POLLING_DRAW_TIME_US=min_157_avg_252_max_316

IRQ_FREE_HEAP=298852
IRQ_LARGEST_8BIT=163840
IRQ_LARGEST_DMA=163840
IRQ_DRAM=298852
IRQ_IRAM=201476
IRQ_MAIN_STACK_MIN_WORDS=2608
IRQ_DRAW_CALLS=212400
IRQ_DRAW_TIME_US=min_156_avg_252_max_321
```

Der offizielle Stack verwendet fuer die SPI-/Panel-I/O keinen eigenen
anwendungsseitigen Low-Level- oder Bustask; deshalb ist der Main-Task-HWM der
relevante Task-Nachweis dieses Harnesses. Es wurde kein zusaetzlicher Task
fuer die Messung eingefuehrt.

## Stage-3-Abschluss nach Planrevision

Die revidierte und ownerfreigegebene Stage-3-Regel akzeptiert die vorhandene
Stage-2-Evidence fuer reale Touchfunktion, Ecken, Mitte, Polling, IRQ sowie
statische Kontakt-/`strength`-Werte. Die zusaetzlichen Kantenmittenkontakte und
ein neuer Kontakt-/Druckverlauf sind keine Stage-3-Abschlusskriterien mehr;
die fuenf Stage-2-Positionen wurden nicht erneut manuell getestet.

```text
STAGE_2_TOUCH_STATIC_EVIDENCE=REUSED_WITHOUT_RETEST
STAGE_2_STATIC_CONTACT_PRESSURE_SAMPLES=REUSED
STAGE_3_EDGE_TOUCH_MATRIX=NOT_REQUIRED_FOR_LOW_LEVEL_SELECTION
STAGE_3_CONTACT_PRESSURE_PROGRESSION=DEFERRED_TO_TOUCH_CALIBRATION_INTEGRATION
OPEN_STAGE_3_FINDINGS=0
STAGE_3=PASS
```

Die vorhandene automatisierte Zusatzmatrix bleibt unveraendert gueltig: Sie
belegt Display-Wiederholung, kombinierten Draw-/Touch-/SPI-Stress,
fail-closed Fehlerreaktionen, Ressourcen, Timing und Stabilitaet in Polling
und IRQ. Es wurden fuer diesen Abschluss keine neuen Hardware- oder
Owner-Touchtests ausgefuehrt.

## Reproduzierbarkeit

Polling wurde mit `sdkconfig.defaults` gebaut. Fuer IRQ wurde die zuerst
verwendete bestehende `sdkconfig` nicht als IRQ-Evidence akzeptiert, weil der
UART `TOUCH_MODE=POLLING` meldete. Danach wurde eine getrennte
`sdkconfig.stage3_irq` verwendet und vor dem Flash verifiziert:

```text
CONFIG_XPT2046_INTERRUPT_MODE=y
TOUCH_MODE=IRQ
```

Die gueltigen Endcaptures sind:

```text
/tmp/issue31-stage3-matrix/polling_reuse_flash_monitor.log
/tmp/issue31-stage3-matrix/irq_correct_flash_monitor.log
```

`STAGE_4=NOT_RUN`, keine Renderer-/LVGL-Auswahl, keine Produktimplementation
und keine Aktorfreigabe wurden aus dieser Evidence abgeleitet.
