# PR #187 – Hardware-Fix-Verification Boot-Loop (2026-10-07)

Folgt auf `PR187_HW_SMOKE_20261007_EVIDENCE.md` (Boot-Loop auf `475db0c`, bleibt
als historisches `FAIL` bestehen).

```text
TESTED_HEAD=fb8978f79a48c8f6741502919e059be71ae8d72d
PROFILE=esp32_release
BUILD_EXACT=sauberer separater Worktree, --require-clean-source-tree, App version fb8978f
FLASH_ERASE=NO (Partitionstabelle byte-identisch d7f180e4...)
PRE_READY_LOCAL_ON_TESTED_HEAD=PASS (host + esp, pre-ready/local=success)

TEXTPACK_BOOT_LOOP_FIXED=PASS (kein abort(), kein std::bad_alloc, kein Reset-Loop; uptime > 80 s)
HARDWARE_FIX_VERIFICATION=FAIL (Kriterium "ohne heap_alloc_failed" verletzt)
HEAP_ALLOC_FAILED=1 (size=12800 caps=0x8 function=heap_caps_aligned_alloc, LVGL buf1)
LVGL_DISPLAY=UNAVAILABLE ("productive LVGL display unavailable; UI remains fail-closed")
UI_USABLE=NO (kein LVGL-Pool, kein Touch-Test moeglich)
FREE_HEAP_STABLE_HOME_WIFI=7748 B (min 6200 B, largest block 6912 B)
PANIC=0 WATCHDOG=0 BROWNOUT=0
ACTUATOR_RELEASE=NO (real actuators: disabled)

S1_S3_S4_S6_S7_S8_S9_S10_D10=BLOCKED_NO_DISPLAY
S2=NOT_APPLICABLE
S5=NOT_APPLICABLE
```

## Befund

Der Textpack-Fix beseitigt den Abbruch aus `makeFermentationUiTextPacks()`. Der
Boot bleibt stabil, die Anwendung meldet `application: ready`, WLAN verbindet.
Beim Initialisieren des ILI9341/LVGL scheitert danach aber die Allokation des
LVGL-Zeichenpuffers (`buf1`, 12800 Byte, `MALLOC_CAP_8BIT`):

```text
E heap_alloc_failed: size=12800 caps=0x00000008 function=heap_caps_aligned_alloc
E (2044) LVGL: lvgl_port_add_disp_priv(386): Not enough memory for LVGL buffer (buf1) allocation!
W (2054) app_main: productive LVGL display unavailable; UI remains fail-closed
I (2094) app_main: resources_lvgl: point=after_ui_init pool=unavailable
```

Freier interner Heap nach dem Boot (`stable_home_wifi`): 7748 Byte frei, 6200 Byte
Minimum, groesster Block 6912 Byte. Auf dem PR-#179-Smoke (`2e81d09`) lagen dieselben
Messpunkte bei ca. 19,9 kB frei / 16,5 kB Minimum; das Display lief. Der verbleibende
Heap ist also um rund 12 kB geschrumpft; der Textpack-Aufbau-Fix verhindert nur den
Abbruch, schafft aber keinen Platz fuer den LVGL-Puffer. Die Ursache der Schrumpfung
(Textpack-Daten, UI-Zustand, Codegroesse im internen RAM) ist nicht gemessen und wird
nicht geraten. Es wurde keine Optimierung und keine weitere Korrektur versucht
(D10: keine vorsorgliche RAM-Optimierung); die Entscheidung liegt beim Owner.

## Provenienz

```text
0d314a42067898051c59a61c6eab7fd617a4c2dea327175f066104634d4b32f7  bootloader.bin
50e5110d934bc907fdf61a0d14db7aadde9ab6718377b75575bafbcb90538916  esp32_fermentationsschrank.bin (exakt, fb8978f)
d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d  partition-table.bin
bda26a441e6ebdf90b8997feb5a60229b7dc26c0c4a0a966efae3b14269668c7  Probe-Build app (fb8978f + PR187_D10_COMMIT_PROBE.patch, nicht geflasht)
513344d7fa576d4639af43d0910adacf5d05248af3ef8e45777ffc52a73b5f8b  uart_01_exact.raw.txt
aa7100c7dea5c4a43b1892e3491caf72872198b5aefa1ade36acd427d1fe6f03  flash_exact.log
```

Rohlog lokal (nicht im Repository). Das Geraet ist mit dem exakten Image `fb8978f`
geflasht; es bootet stabil, zeigt aber kein Display (UI fail-closed, Aktoren
deaktiviert).
