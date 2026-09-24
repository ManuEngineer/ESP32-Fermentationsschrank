# Issue #31 – Touch-Kalibrierung: Provisionierungs-Evidence

```text
ISSUE=31
PR=156
HARDWARE_SOURCE_HEAD=49dad74c2980bc6fcb5c622adefd1bdc52fc42ad
R1_DISPLAY_ROTATION=ROTATE90
ACTUATOR_RELEASE=NO
```

## Reviewed model

Die vier FIT-Punkt-Mediane wurden aus dem unveränderten Capture neu berechnet.
Die unabhängige affine Least-Squares-Rechnung reproduzierte das reviewed Modell
bis auf Floating-Point-Rundung. Center und Top-Mid blieben aus dem Fit
ausgeschlossen.

```text
SOURCE_CAPTURE_SHA256=d2ec93bed9723d1cbe1e8c460c2c601e2b469323a57fdad3f781704e932ae827
AFFINE_MODEL=REVIEWED_AND_PROVISIONED
FIT_TOP_LEFT_MEDIAN=(677.0,3566.5)
FIT_TOP_RIGHT_MEDIAN=(733.5,654.0)
FIT_BOTTOM_LEFT_MEDIAN=(3368.0,3570.0)
FIT_BOTTOM_RIGHT_MEDIAN=(3378.0,656.0)
VALIDATION_CENTER=(159.2070,121.2653), error=1.4933 px
VALIDATION_TOP_MID=(159.9813,47.9909), error=0.0208 px
```

## Actor-free provision

Die Provisionierungsfirmware lief auf dem exakt oben genannten Source-HEAD und
verwendete ausschließlich den bestehenden `TouchCalibrationStore`-Active-Slot
`tc0`. Der erste Lauf schrieb den zuvor nicht vorhandenen Record; ein zweiter
Lauf bestätigte die Idempotenz ohne erneutes Schreiben. `tc1` wurde nicht
beschrieben.

```text
HARNESS_BINARY_SHA256=258ea354362d2a218c3d74603ffe7c5616fb8fa1617caebbe988cc2b01fe8b93
ACTIVE_SLOT=tc0
FALLBACK_SLOT=UNCHANGED
FIRST_PROVISION_ACTIVE_PRESTATE=NOT_FOUND
FIRST_PROVISION_WRITE=COMMITTED
FIRST_PROVISION_READBACK=PASS
REPEAT_PROVISION_ACTIVE_PRESTATE=MATCHING
REPEAT_PROVISION_WRITE=NOT_NEEDED
REPEAT_PROVISION_READBACK=PASS
PROVISION_UART_LOG=docs/audits/ISSUE_31_TOUCH_CALIBRATION_PROVISION_UART_20260924_FINAL_HEAD_RAW.txt
PROVISION_UART_LOG_SHA256=27d21f2073252007105bac6e66eb84025b6891622347c2f75851cca7265c6285
PRODUCT_Z_THRESHOLD=UNCHANGED
```

## Product-load attempt

Der normale `esp32_bringup`-Build ohne Provisionierungsdefine wurde auf denselben
Source-HEAD gebaut und geflasht. Der Produktpfad meldete den Active-Record als
verfügbar:

```text
PRODUCT_BINARY_SHA256=c71360b52aba68d31c92c116dafc3b8a1e5f7023d20e07098ad78fb09a383032
PRODUCT_ACTIVE_LOAD=PASS
UART_MARKER=touch calibration: active_status=Available fallback_status=NotFound
```

Der Touch-Smoke konnte in diesem Lauf noch nicht stattfinden: Der bestehende
`esp32_bringup`-Issue-29-Diagnosepfad meldete unmittelbar danach
`diagnostic task creation failed` und beendete den Produktpfad fail-closed, bevor
Owner-Navigationstouches möglich waren. Die vollständige UART-Evidence ist
deshalb als Blockerlog erhalten; es werden keine Touch-PASS-Marker behauptet.

```text
PRODUCT_TOUCH_SMOKE=BLOCKED
PRODUCT_TOUCH_SMOKE_UART_LOG=docs/audits/ISSUE_31_PRODUCT_TOUCH_SMOKE_UART_20260924_FINAL_HEAD_BLOCKED_RAW.txt
PRODUCT_TOUCH_SMOKE_UART_LOG_SHA256=731bd5498f250fd0ecefd6797b449b31283741fd47cd93c5d21dbb20cb60935b
PRODUCT_TOUCH_LEFT=NOT_RUN
PRODUCT_TOUCH_MID_LEFT=NOT_RUN
PRODUCT_TOUCH_MID_RIGHT=NOT_RUN
PRODUCT_TOUCH_RIGHT=NOT_RUN
ONE_ACTION_PER_PRESS=NOT_RUN
RELEASE_STOPS_PRESS_FEEDBACK=NOT_RUN
NO_GHOST_TOUCH=NOT_RUN
```

Weitere Checks: `esp32_bringup` und `esp32_release` wurden ohne aktiven
Provisionierungsdefine gebaut und validiert. Der alte abgebrochene Capture
bleibt erhalten, weil die Datei nicht leer ist. `PROBE_FAIL_CLOSED_GT_60S` bleibt
korrekt `NOT_VERIFIED`.

```text
PROVISION_READBACK=PASS
CALIBRATION_FIT_NOT_YET_PRODUCTIZED=YES
ACTUATOR_RELEASE=NO
NEXT_STEP=RESOLVE_EXISTING_ISSUE29_BRINGUP_BLOCKER_THEN_REPEAT_PRODUCT_TOUCH_SMOKE
```
