# Issue #31 – Stage-2-Hardware-Smoke-Evidence

Stand: 2026-09-20. Diese Evidence bezieht sich auf den freigegebenen
Stage-1-/Stage-2-Vertrag und enthält ausschließlich den nichtproduktiven,
actor-free Low-Level-Smoke.

## Provenienz und Status

```text
ISSUE=31
PR=156
PR156_HEAD_BEFORE_STAGE_2=802c205f7f11ac9b91eb35e09d77c88544138033
APPROVED_PLAN_SHA=ec6d6b596bd0bc926dbcccb8b3d13b132ed81855
REVIEWED_STAGE_1_HEAD=802c205f7f11ac9b91eb35e09d77c88544138033
OWNER_STAGE_2_APPROVED=YES
ESP_IDF=v6.1@fff9895c82d744c7237be8847347bdd1b07c6643
TARGET=ESP32-WROOM-32E
FLASH=4MB
PSRAM=NONE
RESET_TOPOLOGY=MSP2807_RESET->EN_CHIP_PU_UNCHANGED
ACTUATORS_DISABLED=PASS
STAGE_2_HARNESS_LOG_RESULT=PASS
STAGE_2_DISPLAY_DRIVER_INIT=PASS
STAGE_2_DISPLAY_VISIBLE_OUTPUT_AFTER_FLASH=PASS
STAGE_2_DISPLAY_COLDSTART_REPRODUCIBILITY=FAILED
STAGE_2_DISPLAY_FUNCTION=FAILED_COLDSTART_NOT_REPRODUCIBLE
STAGE_2_TOUCH_FUNCTION=POLLING_PASS_IRQ_PASS
DISPLAY_CONTROLLER_IDENTITY=FUNCTIONAL_VISUAL_PASS_WARM_COLDSTART_REPRODUCIBILITY_FAILED
TOUCH_CONTROLLER_IDENTITY=FUNCTIONAL_RAW_TOUCH_PASS
COLDSTART_BOOT=PASS_WITH_BROWNOUT_MARKER_OBSERVED
HISTORICAL_COLDSTART_DIAGNOSTIC_REPEAT=PASS
HISTORICAL_COLDSTART_DIAGNOSTIC_BROWNOUT_CURRENT_REPEAT=NOT_OBSERVED
HISTORICAL_COLDSTART_DIAGNOSIS=INTERMITTENT_FAILURE_NOT_REPRODUCED
COLDSTART_ROOT_CAUSE=UNDETERMINED
SMOKE_PANEL_RESET_PATH=PASS
PANEL_RESET_BEFORE_INIT=YES
PANEL_RESET_GPIO_NUM=GPIO_NUM_NC
POST_STABLE_RAIL_SHARED_EN_RESET_RECOVERY=PASS
POST_STABLE_RAIL_SHARED_EN_RESET_PATTERN=TL_WHITE_TR_GREEN_BL_RED_BR_BLUE
POST_STABLE_RAIL_SHARED_EN_RESET=RTS_ONLY_NO_FLASH_NO_DTR_GPIO0
COLDSTART_FAILURE_CLASS=POWER_ON_OR_RESET_SEQUENCE_STRONGLY_SUPPORTED
HISTORICAL_NORMAL_COLDSTART_RETEST=FAIL
HISTORICAL_NORMAL_COLDSTART_OFF_TIME_SECONDS=APPROX_30
HISTORICAL_NORMAL_COLDSTART_BROWNOUT=REPRODUCED
HISTORICAL_NORMAL_COLDSTART_DISPLAY=ONLY_WHITE
DELAYED_EN_POWERON_RETEST=PASS_3_OF_3
DELAYED_EN_POWERON_OFF_TIME=AT_LEAST_10_SECONDS_EACH
DELAYED_EN_POWERON_SEQUENCE=RTS_ASSERTED_BEFORE_POWER_ON_HOLD_1S_THEN_RELEASE
DELAYED_EN_BROWNOUT_MARKER=NOT_OBSERVED_RUNS_1_TO_3
DELAYED_EN_RUN_1_DISPLAY=PATTERN_PASS
DELAYED_EN_RUN_2_DISPLAY=PATTERN_PASS
DELAYED_EN_RUN_3_DISPLAY=PATTERN_PASS
HARDWARE_SPIKE_STAGE_2=FAILED
STAGE_2=FAILED
STAGE_3=NOT_RUN
STAGE_4=NOT_RUN
PRODUCT_IMPLEMENTATION=NOT_STARTED
ACTUATOR_RELEASE=NO
```

Der offizielle Low-Level-Stack wurde initialisiert und konnte nach dem
Builder-Flash/Reset ein sichtbares Vier-Ecken-Muster ausgeben. Die reale
Owner-Beobachtung des wiederholten Laufs war:

```text
OWNER_DISPLAY_OBSERVATION_AFTER_FLASH_TOP_LEFT=WHITE
OWNER_DISPLAY_OBSERVATION_AFTER_FLASH_TOP_RIGHT=GREEN
OWNER_DISPLAY_OBSERVATION_AFTER_FLASH_BOTTOM_LEFT=RED
OWNER_DISPLAY_OBSERVATION_AFTER_FLASH_BOTTOM_RIGHT=BLUE
OWNER_DISPLAY_OBSERVATION_BEFORE_COLDSTART_TOP_LEFT=WHITE
OWNER_DISPLAY_OBSERVATION_BEFORE_COLDSTART_TOP_RIGHT=GREEN
OWNER_DISPLAY_OBSERVATION_BEFORE_COLDSTART_BOTTOM_LEFT=RED
OWNER_DISPLAY_OBSERVATION_BEFORE_COLDSTART_BOTTOM_RIGHT=BLUE
DISPLAY_CORNERS_VISIBLE=PASS
DISPLAY_ORIENTATION=SOFTWARE_ROTATION_CONFIGURABLE
```

Die gleiche Eckenzuordnung war bereits vor dem Kaltstart sichtbar. Die
physische Einbaulage ist damit kein eigener Blocker: Die Zuordnung der
logischen Ecken darf über die bereits getesteten `swap_xy`-/Mirror-/Rotation-
Einstellungen erfolgen; ein Wenden des Moduls ist nicht erforderlich. Der
Eckenzuordnungsbefund ist daher kein zusätzlicher Stage-2-Fehler.

Beim vorherigen realen Kaltstart nach Stromunterbrechung meldete der UART den
vollständigen Boot und den Start des gleichen Smoke-Pfads, der Owner sah dabei
jedoch nur weißes Backlight ohne das erwartete Farbmuster:

```text
OWNER_DISPLAY_OBSERVATION_COLDSTART=ONLY_WHITE_BACKLIGHT_NO_EXPECTED_COLOR_PATTERN
DISPLAY_COLDSTART_VISUAL=FAILED
```

Dieser konkrete Widerspruch wird fail-closed als fehlende Kaltstart-
Reproduzierbarkeit gewertet. Die spätere sichtbare Ausgabe nach erneutem
Flash/Hard-Reset hebt den Kaltstartbefund nicht stillschweigend auf. Deshalb
ist Stage 2 insgesamt `FAILED`; Stage 3 und Stage 4 wurden nicht gestartet.

### Gezielte Kaltstartdiagnose

Mit demselben offiziellen IRQ-Stack, derselben Verdrahtung
`MSP2807_RESET -> EN_CHIP_PU` und weiterhin actor-free wurde ein zweiter echter
Power-Cycle überwacht. Der aktuelle UART-Capture zeigte:

```text
POWERON_RESET=PASS
SPI_FAST_FLASH_BOOT=PASS
STAGE2_START=PASS
SPI_BUS=PASS
DISPLAY_CONTROLLER_DRIVER=ILI9341_CREATE_PASS
TOUCH_CONTROLLER_DRIVER=XPT2046_CREATE_PASS
DISPLAY_VISUAL_PASS=PASS
CURRENT_REPEAT_BROWNOUT_MARKER=NOT_OBSERVED
CURRENT_REPEAT_TOP_LEFT=WHITE
CURRENT_REPEAT_TOP_RIGHT=GREEN
CURRENT_REPEAT_BOTTOM_LEFT=RED
CURRENT_REPEAT_BOTTOM_RIGHT=BLUE
```

Die frühere Owner-Beobachtung `ONLY_WHITE_BACKLIGHT_NO_EXPECTED_COLOR_PATTERN`
wurde im anschließenden Konvergenz-Retest in Lauf 1 erneut konkret beobachtet.
Nach Wiederkehr der Versorgung meldete der UART erneut den vollständigen Boot
und den Smoke-Start; der Owner sah diesmal nur weißes Display. Die Logs
erlauben weiterhin keine belastbare Root Cause.
Es wird keine Produktkorrektur und kein allgemeines Pegel-/Mess-Gate abgeleitet.
Der ursprüngliche Stage-2-Fehler bleibt bis zu einer reproduzierbaren Ursache
oder einer ausdrücklich neuen Owner-Entscheidung bestehen.

### Stable-rail shared-EN recovery und kontrollierter Kaltstart

Nach dem Brownout-/White-Display-Zustand blieb die Versorgung mindestens zwei
Sekunden stabil. Danach wurde genau ein gemeinsamer
`EN_CHIP_PU/MSP2807_RESET`-Hardware-Reset über den FT232-RTS-Pfad ausgelöst;
es erfolgten kein Flash und kein DTR/GPIO0-Downloadmodus. Das erwartete Muster
war danach sichtbar:

```text
POST_STABLE_RAIL_SHARED_EN_RESET_RECOVERY=PASS
POST_STABLE_RAIL_SHARED_EN_RESET_PATTERN=TL_WHITE_TR_GREEN_BL_RED_BR_BLUE
POST_STABLE_RAIL_SHARED_EN_RESET=RTS_ONLY_NO_FLASH_NO_DTR_GPIO0
```

Die erfolgreiche Wiederherstellung durch den gemeinsamen EN-Reset stützt die
Fehlerklasse `POWER_ON_OR_RESET_SEQUENCE_STRONGLY_SUPPORTED`; sie beweist keine
konkrete Versorgungskomponente und ändert `COLDSTART_ROOT_CAUSE=UNDETERMINED`
nicht.

Anschließend wurde historisch genau ein normaler kontrollierter Kaltstart mit unveränderter
Verdrahtung, demselben offiziellen IRQ-Smoke und actor-free Bedingungen
ausgeführt. Die Auszeit betrug nach Owner-Angabe ungefähr 30 Sekunden. Der
UART zeigte erneut den Brownout-Marker; das Display blieb weiß. Deshalb wurde
der damalige Dreier-Retest nach Lauf 1 beendet:

```text
HISTORICAL_NORMAL_COLDSTART_RETEST=FAIL
HISTORICAL_NORMAL_COLDSTART_OFF_TIME_SECONDS=APPROX_30
HISTORICAL_NORMAL_COLDSTART_BROWNOUT=REPRODUCED
HISTORICAL_NORMAL_COLDSTART_DISPLAY=ONLY_WHITE
HISTORICAL_NORMAL_COLDSTART_UART=POWERON_RESET_SPI_FAST_FLASH_BOOT_STAGE2_START_PANEL_INIT_PASS
HISTORICAL_NORMAL_COLDSTART_BROWNOUT_TIMING=POST_POWER_ON_BOOT_CAPTURE_EXACT_OFFSET_NOT_INSTRUMENTED
COLDSTART_FAILURE_CLASS=POWER_ON_OR_RESET_SEQUENCE_STRONGLY_SUPPORTED
COLDSTART_ROOT_CAUSE=UNDETERMINED
```

Dieser historische Brownout-Marker wurde zeitlich nur dem Boot-Capture nach
Wiederkehr der Versorgung zugeordnet; ein exakter Millisekundenabstand zum
Power-On wurde nicht erfasst. Der Marker allein wird nicht als bewiesene
Startup-Ursache behandelt.

### Verzögerter EN-Power-On-Retest

Die Harnessquelle wurde vor dem Retest direkt geprüft. In
`/tmp/issue31-stage2-smoke/main/main.cpp` steht
`esp_lcd_panel_reset(panel)` unmittelbar vor
`esp_lcd_panel_init(panel)`. Der Harness setzt
`panel_config.reset_gpio_num=GPIO_NUM_NC`; der physische Panel-Reset bleibt
damit der unveränderte gemeinsame Pfad `MSP2807_RESET -> EN_CHIP_PU`.

Für den diagnostischen Retest wurde der offizielle IRQ-Smoke unverändert
verwendet. Vor jedem Power-On wurde über den bereits bewiesenen FT232-RTS-
Pfad EN LOW gehalten, die Versorgung mindestens 10 Sekunden ausgeschaltet,
nach dem Einschalten diagnostisch 1 Sekunde gewartet und EN anschließend
freigegeben. DTR/GPIO0 blieben unbenutzt, es wurde nicht geflasht und die
Versorgung/SSOT blieb unverändert.

```text
SMOKE_PANEL_RESET_PATH=PASS
PANEL_RESET_BEFORE_INIT=YES
PANEL_RESET_GPIO_NUM=GPIO_NUM_NC
DELAYED_EN_POWERON_RETEST=PASS_3_OF_3
DELAYED_EN_POWERON_OFF_TIME=AT_LEAST_10_SECONDS_EACH
DELAYED_EN_POWERON_SEQUENCE=RTS_ASSERTED_BEFORE_POWER_ON_HOLD_1S_THEN_RELEASE
DELAYED_EN_BROWNOUT_MARKER=NOT_OBSERVED_RUNS_1_TO_3
DELAYED_EN_RUN_1_UART=POWERON_RESET_SPI_FAST_FLASH_BOOT_STAGE2_START_PANEL_INIT_PASS
DELAYED_EN_RUN_1_DISPLAY=PATTERN_PASS
DELAYED_EN_RUN_2_UART=POWERON_RESET_SPI_FAST_FLASH_BOOT_STAGE2_START_PANEL_INIT_PASS
DELAYED_EN_RUN_2_DISPLAY=PATTERN_PASS
DELAYED_EN_RUN_3_UART=POWERON_RESET_SPI_FAST_FLASH_BOOT_STAGE2_START_PANEL_INIT_PASS
DELAYED_EN_RUN_3_DISPLAY=PATTERN_PASS
COLDSTART_ROOT_CAUSE=UNDETERMINED
```

In allen drei verzögerten Läufen wurde im vollständigen UART-Fenster vom
Power-On über Boot, Panel-Init und erste Draw-Ausgabe kein Brownout-Marker
beobachtet; alle drei Owner-Sichtprüfungen bestätigten das erwartete Muster.
Die verzögerte EN-Sequenz ist damit `PASS_3_OF_3`, beweist aber keine konkrete
Root Cause für die historische normale Kaltstartabweichung. Es wurden keine
Hardware-/SSOT-Änderung, keine Produktkorrektur und keine Stage-3-/Stage-4-
Arbeit begonnen. Stage 2 bleibt wegen der historischen normalen
Kaltstartabweichung `FAILED`.

### Historischer Kaltstart-Konvergenz-Retest

Der frühere Retest sollte fünf echte überwachte Power-Cycles mit exakt demselben
IRQ-Smoke, derselben `MSP2807_RESET -> EN_CHIP_PU`-Verdrahtung und weiterhin
actor-free Bedingungen umfassen. Lauf 1 wurde bis zum konkreten Fehler
ausgeführt und beendet den Retest fail-closed:

```text
HISTORICAL_COLDSTART_REPRODUCIBILITY_RETEST=FAIL
HISTORICAL_COLDSTART_RETEST_COMPLETED_CYCLES=1_OF_5
HISTORICAL_COLDSTART_RETEST_FAILURE_RUN=1
HISTORICAL_COLDSTART_RETEST_FAILURE_MARKER=E_BOD_BROWNOUT_DETECTOR_WAS
HISTORICAL_COLDSTART_RETEST_OWNER_DISPLAY=ONLY_WHITE
POWERON_RESET=PASS_AFTER_POWER_RESTORE
SPI_FAST_FLASH_BOOT=PASS_AFTER_POWER_RESTORE
STAGE2_START=PASS_AFTER_POWER_RESTORE
SPI_BUS=PASS_AFTER_POWER_RESTORE
DISPLAY_CONTROLLER_DRIVER=ILI9341_CREATE_PASS_AFTER_POWER_RESTORE
TOUCH_CONTROLLER_DRIVER=XPT2046_CREATE_PASS_AFTER_POWER_RESTORE
DISPLAY_VISUAL_UART_MARKER=PASS_AFTER_POWER_RESTORE
HISTORICAL_COLDSTART_ROOT_CAUSE=UNDETERMINED
STAGE_2=FAILED
```

Läufe 2–5 wurden nach dem ersten Brownout-/Displayfehler nicht gestartet.

## Testaufbau

Der Smoke lief außerhalb des Produktrepositories unter
`/tmp/issue31-stage2-smoke` und wurde nicht in die Produktsoftware übernommen.
Verwendet wurden ausschließlich:

```text
espressif/esp_lcd
espressif/esp_lcd_ili9341=2.1.0
espressif/esp_lcd_touch=1.2.1
atanisoft/esp_lcd_touch_xpt2046=1.0.6
DEPENDENCIES_LOCK_SHA256=9ef854367f503de7c8bdde3c555d2fb63d38cbcaff82cff411ee99c8f4752250
```

Boardprofil und SSOT-Leitungen:

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

Die direkte Reset-Topologie `MSP2807_RESET -> EN_CHIP_PU` blieb unverändert.
Es wurde kein Reset-Jumper isoliert oder entfernt. Peltier, BTS7960, Lüfter,
MOSFET-Verbraucher und Summer blieben getrennt beziehungsweise inaktiv.

## Builder-Nachweise

Beide Stage-2-Profile bauten und wurden geflasht. Die unveränderten
Smoke-Binaries waren:

```text
SMOKE_MAIN_SHA256=b27abdd32356205f3bce4a289ee3232d4c22d67db81e2c27a159fd5f7caa40e4
POLLING_BINARY_SHA256=0f38a10a5cc1c6e364b39c8838a11c2b5f35bf7e29e3d838ac2512862ad10abb
POLLING_BINARY_SIZE=197072
IRQ_BINARY_SHA256=5e41e899aa3cf6fdb103f34d2e3efd70a6ca66ea162141141c7c77fe23330acc
IRQ_BINARY_SIZE=197248
```

Der UART meldete bei beiden Profilen ESP-IDF v6.1, 4 MB Flash, keinen PSRAM,
gemeinsamen SPI-Bus mit getrennten TFT-/Touch-CS, erfolgreiche ILI9341-
Treibererzeugung und erfolgreiche XPT2046-Treibererzeugung. Ein roher
Display-ID-Readback lieferte `00,00,00` und wurde nicht als Identitätsnachweis
verwendet; die Identitätsaussage stützt sich nur auf den funktionalen
Treiber-/Zeichenpfad und die Owner-Sichtprüfung.

Die fünf Wiederholzyklen wurden in beiden Profilen mit diesen Einstellungen
ausgeführt:

```text
cycle1: 320x240 swap_xy=1 mirror_x=0 mirror_y=0
cycle2: 240x320 swap_xy=0 mirror_x=1 mirror_y=0
cycle3: 320x240 swap_xy=1 mirror_x=1 mirror_y=1
cycle4: 240x320 swap_xy=0 mirror_x=0 mirror_y=0
cycle5: 320x240 swap_xy=1 mirror_x=0 mirror_y=0
```

Für jeden Zyklus meldete der Harness Schwarz/Weiß/Rot/Grün/Blau, vier Ecken,
Backlight-Aus/Ein und `REPEAT_CYCLE=PASS`. Diese UART-Marker belegen die
Treiberaufrufe und die vollständigen Schleifen; sie ersetzen nicht die
physische Sichtprüfung. Der spätere sichtbare Lauf bestätigte die vier Farben,
aber nicht die Kaltstart-Reproduzierbarkeit.

## Touch-Evidence

Polling und IRQ wurden getrennt ausgeführt. Die Rohwerte waren in beiden
Profilen an allen fünf definierten Positionen vorhanden. Beispiel IRQ:

```text
TOP_LEFT     raw_x=516  raw_y=522  strength=2118  irq_events=4
TOP_RIGHT    raw_x=591  raw_y=3713 strength=700   irq_events=19
BOTTOM_LEFT  raw_x=3518 raw_y=551  strength=2438  irq_events=34
BOTTOM_RIGHT raw_x=3696 raw_y=3743 strength=1688  irq_events=48
CENTER       raw_x=2243 raw_y=2463 strength=1928  irq_events=63
```

Der Polling-Lauf lieferte ebenfalls fünf Rohkontakt-/Druckproben ohne
Treiber- oder Busfehler. Getrennte CS-Zugriffe und alternierende Zugriffe auf
den gemeinsamen SPI-Bus waren erfolgreich. Damit ist der Touch-Teil des
Stage-2-Smokes `PASS`; dies ist kein Kalibrierungs- oder Produkt-UI-Nachweis.

## Abschluss

```text
STAGE_2_DISPLAY_FUNCTION=FAILED_COLDSTART_NOT_REPRODUCIBLE
STAGE_2_TOUCH_FUNCTION=PASS
STAGE_2=FAILED
STAGE_3=NOT_RUN
STAGE_4=NOT_RUN
PRODUCT_CODE_CHANGED=NO
PLAN_CHANGED=NO
ACTUATOR_RELEASE=NO
```

Der nächste zulässige Schritt ist ausschließlich die gezielte Diagnose der
konkreten Display-Kaltstartabweichung. Keine Stage-3-/Stage-4-Matrix,
Produktimplementation, Renderer-/LVGL-Auswahl, Ready-/Merge-Aktion oder
Aktorfreigabe ist aus dieser Evidence abgeleitet.
