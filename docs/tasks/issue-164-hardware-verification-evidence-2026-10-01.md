# Issue #164 – Hardware-Verifikation, Evidence 2026-10-01

Dieser Bericht ergänzt die historische Hardware-Evidence vom 2026-09-29. Die
historische Datei wird nicht rückwirkend geändert. Festgehalten werden nur die
in diesem Lauf tatsächlich belegten Ergebnisse; die interaktiven Owner-Tests
bleiben bis zur Anwesenheit am Gerät offen.

## Provenienz und Flash

```text
ISSUE=164
PR=171
PR_STATUS=OPEN_DRAFT
REVIEWED_HEAD=a245e686558b18c9b6421ed985edd0d7aca31f7c
APPROVED_PLAN_SHA=3cedb448de6bbf4ce91ac029c55fad1b873bea2d
FLASH_PROFILE=esp32_release
BUILD_SOURCE_HEAD=a245e686558b18c9b6421ed985edd0d7aca31f7c
BUILD=PASS
BUILD_PROFILE_VALIDATION=PASS
ESP_IDF=6.1
ESPTOOL=5.4.0
REBUILD_FROM_EXACT_HEAD=YES
FULL_ERASE=NO
NVS_ERASE=NO
STATE_STORE_ERASE=NO
ACTUATOR_RELEASE=NO
PRODUCT_CODE_CHANGED=NO
SOFTWARE_TESTS=NOT_RUN_IN_THIS_EVIDENCE_COMMIT
```

Der kanonische Release-Treiber `scripts/build_esp_idf_profiles.py release`
wurde aus dem sauberen Checkout des exakten Reviewed HEAD ausgeführt. Der
Flash schrieb ausschließlich die drei Release-Images; es wurde kein Full-
Erase, NVS-Erase oder State-Store-Erase angefordert oder ausgeführt.

| Flash-Image | Offset | SHA-256 |
|---|---:|---|
| `bootloader/bootloader.bin` | `0x1000` | `1e12e2be0290ca1f153ca06ee390c0a0b2459518ab0710e76c1c65746d46f9f0` |
| `partition_table/partition-table.bin` | `0x8000` | `d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d` |
| `esp32_fermentationsschrank.bin` | `0x10000` | `8ca4bdef855de5769e23f7c6a02b34af37ab460c595089de58f371b5d702313a` |

Esptool meldete für alle drei Images `Hash of data verified`. Der Flash wurde
zunächst mit `--after no-reset` beendet; der anschließende Reset erfolgte über
die FT232R-RTS/EN-Steuerung ohne erneuten Flash.

## UART-Boot- und Stabilitätsnachweis

Der verwertbare Mitschnitt wurde mit `esp-idf-monitor 1.10.0` bei 115200 Baud
auf dem FT232R-Port `/dev/ttyUSB0` aufgenommen. Der erste `screen`-Versuch
enthielt wiederholte Bootloader-Fragmente und wird nicht als Firmware-
Resetnachweis verwendet. Die folgende ESP-IDF-Monitor-Aufnahme ist die
maßgebliche Evidence.

```text
UART_MONITOR=esp-idf-monitor-1.10.0
UART_BAUD=115200
UART_VALID_RUNTIME=approximately_315_seconds
HEARTBEATS=315
SOURCE_GIT_SHA=a245e686558b18c9b6421ed985edd0d7aca31f7c
PROFILE=esp32_release
APPLICATION_READY=YES
LCD_PANEL_CREATE=YES
LVGL_TASK_START=YES
PANIC=NO_OBSERVED
WATCHDOG=NO_OBSERVED
BROWNOUT=NO_OBSERVED
UNEXPECTED_RESET_AFTER_APPLICATION_READY=NO_OBSERVED
REAL_ACTUATORS=DISABLED
HARDWARE_STATE=HARDWARE_UNVERIFIED
ACTUATOR_POLICY=REQUIRE_VERIFIED_HARDWARE
```

Der gültige Bootlauf meldete `POWERON_RESET`, lud die Factory-App und
bestätigte anschließend den Source-SHA, das Release-Profil,
`application: ready`, die LCD-Panel-Initialisierung und den LVGL-Start. Der
UART meldete außerdem DHCP auf `192.168.4.1`; dies ist noch kein Nachweis eines
Client-Beitritts oder Browser-Tests.

## Ressourcen-Evidence

Die vorhandenen Produkt-Ressourcenpunkte wurden ohne neue Diagnosearchitektur
aufgenommen:

| Punkt | Netzwerkzustand | Freier Heap | Minimum | größter 8-bit-Block | Stack-HWM |
|---|---|---:|---:|---:|---:|
| `startup` | `HOME_WIFI / SetupAccessPoint` | 10060 | 5024 | 7424 | 14000 |
| `stable_home_wifi_setup_access_point` | `HOME_WIFI / SetupAccessPoint` | 10060 | 4976 | 7424 | 14000 |
| `periodic_30s` | `HOME_WIFI / SetupAccessPoint` | 10228 | 4632 | 7424 | 14000 |

Nach dem gültigen App-Start liefen die Heartbeats monoton bis zu etwa 315 s;
es wurde dabei kein Resetmarker beobachtet.

## Interaktive Hardwaretests

Diese Prüfungen konnten ohne anwesenden Owner nicht durchgeführt werden und
sind ausdrücklich nicht als bestanden zu werten:

| Prüffeld | Ergebnis |
|---|---|
| Physische Display-Sichtprüfung auf 320×240 | `NOT_RUN` |
| Netzwerkseite per Header-Touch | `NOT_RUN` |
| `AP_ONLY` auswählen | `NOT_RUN` |
| SSID aus Device-Name am Gerät bestätigen | `NOT_RUN` |
| SoftAP-Passwort-Persistenz über Reboot/Mode-Wechsel | `NOT_RUN` |
| WLAN-QR mit Smartphone scannen | `NOT_RUN` |
| SoftAP-Client-Join und DHCP-Zuweisung | `NOT_RUN` |
| Zugriff auf `192.168.4.1` | `NOT_RUN` |
| HOME_WIFI Setup, Commit und Reconnect | `NOT_RUN` |
| Aktuatorfreigabe | `NO` |

Die reale Display-Lesbarkeit und die QR-/WLAN-Abnahme bleiben damit das
folgende Owner-zu-Gerät-Gate.

## Evidence-Aufbewahrung und Status

Die Rohmitschnitte liegen lokal, außerhalb dieses Commits, mit Modus `0600`:

```text
UART_MONITOR_LOG_SHA256=76537642e43fa99f2d488655af16f673da48640d5fe37c157db29de3ea34af0f
UART_RAW_CAPTURE_SHA256=f97403053b680e799547c3c86ed0e1f8d29d67ab64532c903be2e4124e0c377d
SECRETS_IN_COMMIT=NO
```

```text
BOOT_EVIDENCE=PASS
INTERACTIVE_HARDWARE_EVIDENCE=NOT_RUN
INDEPENDENT_HARDWARE_EVIDENCE_REVIEW=OPEN
READY=NO
MERGE=NO
ISSUE164_CLOSE=NO
NEXT_GATE=INDEPENDENT_HARDWARE_EVIDENCE_REVIEW
```
