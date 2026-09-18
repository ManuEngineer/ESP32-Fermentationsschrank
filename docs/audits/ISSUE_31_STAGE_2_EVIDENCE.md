# Issue #31 – Stage-2-Hardware-Smoke-Evidence

Stand: 2026-09-18. Diese Evidence bezieht sich auf den freigegebenen
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
OWNER_DISPLAY_OBSERVATION_AFTER_FLASH=OL_WHITE_OR_GREEN_UL_RED_UR_BLUE
DISPLAY_CORNERS_VISIBLE=PASS
DISPLAY_ORIENTATION=SOFTWARE_ROTATION_CONFIGURABLE
```

Die physische Einbaulage ist damit kein eigener Blocker: Die Zuordnung der
logischen Ecken darf über die bereits getesteten `swap_xy`-/Mirror-/Rotation-
Einstellungen erfolgen; ein Wenden des Moduls ist nicht erforderlich.

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
