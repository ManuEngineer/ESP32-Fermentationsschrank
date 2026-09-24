# Issue #31 – Touch-Kalibrierung: Provisionierungs-Evidence

```text
ISSUE=31
PR=156
HARDWARE_SOURCE_HEAD=8b8c8116996d274ee5945c7313a14caa11adfd6a
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
HARNESS_BINARY_SHA256=6d91fab0a18c210c9a701415dbb5da691e83817ff49adbcf90a800767b2eb8be
ACTIVE_SLOT=tc0
FALLBACK_SLOT=UNCHANGED
FIRST_PROVISION_ACTIVE_PRESTATE=NOT_FOUND
FIRST_PROVISION_WRITE=COMMITTED
FIRST_PROVISION_READBACK=PASS
REPEAT_PROVISION_ACTIVE_PRESTATE=MATCHING
REPEAT_PROVISION_WRITE=NOT_NEEDED
REPEAT_PROVISION_READBACK=PASS
PROVISION_UART_LOG=docs/audits/ISSUE_31_TOUCH_CALIBRATION_PROVISION_UART_20260924_MATCHING_RAW.txt
PROVISION_UART_LOG_SHA256=9196ab010a3044271732cb93f8c79396f9b48e95f326f812da56363164ce3dea
PRODUCT_Z_THRESHOLD=UNCHANGED
```

## Product-load attempt

Der normale `esp32_bringup`-Build ohne Provisionierungsdefine wurde auf denselben
Source-HEAD gebaut und geflasht. Der Produktpfad meldete den Active-Record als
verfügbar:

```text
PRODUCT_BINARY_SHA256=7898e98fed2b4f982ca044cbeac51fe46a4e025d67d5dfb4c40b801d2c42c49a
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
PRODUCT_TOUCH_SMOKE_UART_LOG=docs/audits/ISSUE_31_PRODUCT_TOUCH_SMOKE_UART_20260924_BLOCKED_RAW.txt
PRODUCT_TOUCH_SMOKE_UART_LOG_SHA256=eccfb78bbc70d1d170e668306751ce4d6dcf555ad1725e16aa8f7ac16e7a9b53
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
