# Issue #31 – kontrollierte tc0-Migration und Product-Touch-Smoke

```text
ISSUE=31
PR=156
SOURCE_HEAD=c39b420ac6c2fec1c989fe8691ebe88cf3e44731
APPROVED_PLAN_SUPPLEMENT_SHA=5a52f0147e9d277fc38f5b65489e7126301ebda6
R1_DISPLAY_ROTATION=ROTATE90
FINAL_R1_PANEL_TRANSFORM=swap_xy:true,mirror_x:true,mirror_y:true
ACTUATOR_RELEASE=NO
```

## Deterministische Modellkomposition

Die bestehende, reviewed `tc0`-Sequenz 1 wurde unabhängig mit
`x_final=319-x_old` komponiert. Die sechs Koeffizienten reproduzieren die
freigegebenen Werte bis auf normale IEEE-754-Rundung; es wurde kein neuer
FIT-/Validation-/Hold-Capture durchgeführt.

```text
SOURCE_CAPTURE_SHA256=d2ec93bed9723d1cbe1e8c460c2c601e2b469323a57fdad3f781704e932ae827
OLD_MODEL_SEQUENCE=1
COMPOSED_MODEL_SEQUENCE=2
COMPOSED_MODEL=(-0.00009043639686374949,0.08753007024919984,-25.146273472211817,0.06559259340326797,0.0007485020274466806,-15.832052617145878)
ACTIVE_SLOT=tc0
FALLBACK_SLOT=tc1_UNCHANGED
```

## Actor-free Provisionierung

Der erste Flash-Reset führte die Sequenz-1-zu-Sequenz-2-Migration aus, bevor
der UART-Monitor angehängt war. Der angehängte vollständige Monitorlauf ist
deshalb bewusst als idempotenter Readback-Check dokumentiert und behauptet
nicht, den nicht mitgeschnittenen `COMMITTED`-Marker erneut zu belegen.

```text
HARNESS_BINARY_SHA256=193087890fb6e8109aaa5f12aad452b1fe1d3ad79292efd81590c46d1351af92
PROVISION_IDEMPOTENT_CHECK=PASS
ACTIVE_PRESTATE=COMPOSED_SEQUENCE_2
WRITE=NOT_NEEDED
READBACK=PASS
PRODUCT_Z_THRESHOLD=UNCHANGED
```

Lesbare UART-Evidence:

```text
PROVISION_UART_RAW=docs/audits/ISSUE_31_TOUCH_CALIBRATION_PROVISION_UART_20260924_COMPOSED_MIGRATION_RAW.txt
PROVISION_UART_RAW_SHA256=4207237b2b36f7d4b0528d99045c004391268b0b2139bcb5bdff700b5bc9f846
PROVISION_UART_READABLE=docs/audits/ISSUE_31_TOUCH_CALIBRATION_PROVISION_UART_20260924_COMPOSED_MIGRATION_READABLE.txt
PROVISION_UART_READABLE_SHA256=1f42cda07d7d26280f082d0d46f72ff3dca1d97d9c099a5fe4a28d957b41b0f8
```

## Normaler Product-Smoke

Der normale `esp32_bringup`-Build lief ohne Provisionierungsdefine. Issue-29
bestand vor der UI-Initialisierung; der Produktpfad lud `tc0` als verfügbar
und blieb im sicheren Runtime-Loop. Die Owner-Taps wurden einzeln und nur auf
Navigation/Statuspfaden ausgeführt: linker Start-Slot öffnete Rezepte,
Home kehrte zurück, Status öffnete Status, Meldungen öffnete Meldungen; der
rechte Details-Tap blieb mangels verfügbarer Meldung ein sicherer No-op.

```text
PRODUCT_BINARY_SHA256=69238ffa32aec075782b47f865c27596c6c7615275f6b284db0ca98cf7cb7c9f
ISSUE29_PROBE=PASS
PRODUCT_ACTIVE_LOAD=PASS
PRODUCT_TOUCH_LEFT=PASS
PRODUCT_TOUCH_MID_LEFT=PASS
PRODUCT_TOUCH_MID_RIGHT=PASS
PRODUCT_TOUCH_RIGHT=PASS_SAFE_NOOP
ONE_ACTION_PER_PRESS=PASS
RELEASE_STOPS_PRESS_FEEDBACK=PASS
NO_GHOST_TOUCH=PASS
```

```text
PRODUCT_UART_RAW=docs/audits/ISSUE_31_PRODUCT_TOUCH_SMOKE_20260924_COMPOSED_MODEL_RAW.txt
PRODUCT_UART_RAW_SHA256=00e0a9a9603ece2f548ccbfe1aab93ffa2eea5e6435d4325e9004c80b59668bd
PRODUCT_UART_READABLE=docs/audits/ISSUE_31_PRODUCT_TOUCH_SMOKE_20260924_COMPOSED_MODEL_READABLE.txt
PRODUCT_UART_READABLE_SHA256=b23dd295cef9ed2720f93f170c25e9f7a6ae807eac45ae649da05c55b9014479
```

```text
CALIBRATION_MODEL_PROVISIONED=YES
PRODUCT_ACTIVE_LOAD=PASS
PRODUCT_TOUCH_SMOKE=PASS
PRODUCT_Z_THRESHOLD=UNCHANGED
ACTUATOR_RELEASE=NO
PROBE_FAIL_CLOSED_GT_60S=NOT_VERIFIED
NEXT_STEP=INDEPENDENT_FIX_VERIFICATION
```

Die historische Raw-Capture-Evidence bleibt unverändert; `tc1`, Touch-
Thresholds, Text/WiFi/Service/Branding und Aktorpolicy wurden nicht geändert.
