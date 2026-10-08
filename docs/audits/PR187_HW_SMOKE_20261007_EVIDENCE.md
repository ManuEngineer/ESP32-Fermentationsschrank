# PR #187 – konsolidierter Hardware-Smoke Issue #172 (2026-10-07)

```text
TESTED_HEAD=475db0cc3316b068d4396dd47269757031dcafb4
PROFILE=esp32_release
BUILD_EXACT=sauberer separater Worktree, --require-clean-source-tree (SOURCE_TREE_CLEAN=YES), App version 475db0c
FLASH_ERASE=NO (kein Full-/NVS-/State-Store-Erase, Partitionstabelle byte-identisch d7f180e4...)
SERIAL_PORT=/dev/ttyUSB0 (FTDI FT232R)
PRE_READY_LOCAL_ON_TESTED_HEAD=PASS

HARDWARE_SMOKE_PR187=FAIL
BOOT_STABILITY=FAIL
BOOT_LOOP=REPRODUCIBLE_EXACT_HEAD_AND_PROBE_BUILD
HEAP_ALLOC_FAILED=1_PER_BOOT (size=13176 caps=0x1800)
PANIC_ABORT=1_PER_BOOT (abort() nach std::bad_alloc)
WATCHDOG=0
BROWNOUT=0
UNEXPECTED_RESET=SW_CPU_RESET_LOOP (rst:0xc)
UI_STATUS=KEIN_STABILER_UI_ZUSTAND_ERREICHT

S1_PROGRAM_LIST=BLOCKED_BOOT_LOOP
S3_LANGUAGE=BLOCKED_BOOT_LOOP
S3_D10_COMMIT_LOG=BLOCKED_BOOT_LOOP
S4_CLOCK_REGRESSION=BLOCKED_BOOT_LOOP
S6_PROGRAM_MANAGEMENT=BLOCKED_BOOT_LOOP
S6_D10_COMMIT_LOG=BLOCKED_BOOT_LOOP
S7_CONTENT_PAGES=BLOCKED_BOOT_LOOP
S8_S9_INPUT_AND_FAIL_CLOSED=BLOCKED_BOOT_LOOP
S10_SETTINGS_KEYBOARD_EDITOR=BLOCKED_BOOT_LOOP
S10_KEY_30X34_AND_LABEL_WIDTHS=BLOCKED_BOOT_LOOP
S10_D10_DEVICE_NAME_COMMIT_LOG=BLOCKED_BOOT_LOOP
S2=NOT_APPLICABLE
S5=NOT_APPLICABLE
ACTUATOR_RELEASE=NO (real actuators: disabled)
```

## Befund

Beide geflashten Images laufen nach `application: ready` und WLAN-Verbindung in
einen Abbruch-Neustart-Zyklus (ca. alle 2 s, 11 Zyklen in 30 s). Ein UI-/Touch-
Test war dadurch nicht moeglich; alle abhaengigen Slices sind `BLOCKED`, nicht
bestanden und nicht als einzeln gescheitert bewertet. Keine Korrektur wurde
versucht.

```text
I (1834) app_main: source git sha: 475db0cc3316b068d4396dd47269757031dcafb4
I (1844) app_main: hardware state: HARDWARE_UNVERIFIED
I (1844) app_main: actuator policy: REQUIRE_VERIFIED_HARDWARE
I (1844) app_main: real actuators: disabled
I (1854) app_main: application: ready
I (1864) wifi:connected with Valhalla, aid = 1, channel 1, BW20, bssid = 80:2a:a8:4a:05:c2
I (1864) wifi:security: WPA2-PSK, phy: bgn, rssi: -47, cipher(pairwise:0x3, group:0x3), pmf:0
I (1874) wifi:pm start, type: 1

I (1874) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (1884) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (1904) wifi:<ba-add>idx:0 (ifx:0, 80:2a:a8:4a:05:c2), tid:0, ssn:0, winSize:64
E heap_alloc_failed: size=13176 caps=0x00001800 function=heap_caps_malloc

abort() was called at PC 0x402016c2 on core 0


Backtrace: 0x4008c5f9:0x3ffcc280 0x4008c5c1:0x3ffcc2a0 0x4008bfd9:0x3ffcc2c0 0x402016c2:0x3ffcc330 0x401fff2d:0x3ffcc350 0x400fb23d:0x3ffcc370 0x400fc65e:0x3ffcc390 0x400fc70c:0x3ffcc3b0 0x400fc74d:0x3ffcc3d0 0x400fc76c:0x3ffcc3f0
```

Backtrace-Aufloesung (Probe-Build, gleiche Quelle): `app_main` (`main/app_main.cpp:688`)
→ `fermentation::makeFermentationUiTextPacks()`
(`lib/fermentation_app/src/fermentation_ui_text.cpp:793`) →
`std::vector<TextPackManifest>(initializer_list)` → Kopie von
`std::vector<TextTranslation>` → `operator new` → `heap_caps_malloc`
(13176 Byte, `MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL`) schlaegt fehl →
`__cxa_allocate_exception` → `abort()`.

Einordnung (Hypothese, nicht auf dem Geraet geprueft): Die Textpacks wuchsen in
PR C/D (`152U` → `183U` Eintraege je Locale); die Initialisierungsliste wird
beim Aufbau pro Locale kopiert, sodass der benoetigte zusammenhaengende
Block (13176 Byte) nach dem WLAN-Start nicht mehr verfuegbar ist. Ob der
Defekt schon auf PR C (`0718ef9`) auftritt oder durch S10 ausgeloest wird, ist
nicht gemessen. Der Stand `2ce9fed` (S4-Hardware-Fix-Verification, PASS) hatte
weniger Textpack-Eintraege. Dies ist ein neuer reproduzierbarer
Ressourcenfehler im Sinne des Auftrags und wird nicht durch Optimierung
verdeckt; die Entscheidung ueber die Korrektur liegt beim Owner.

## Provenienz

- Exakter Build (`475db0c`, sauber): App-Bin SHA-256
  `bf303f03e19146f326053c1e678fc98a7d67c6c4b684997e78a75a270160d52d`,
  Bootloader `40c9306def084e9fc8d599f997eb5db2ee524afedf8e2f6f898387116b1e9aae`,
  Partitionstabelle `d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d`.
- Probe-Build (`475db0c` + `docs/audits/PR187_D10_COMMIT_PROBE.patch`, App version
  `475db0c-dirty`, nur Evidence-Logpunkte `commit_probe_before/after` fuer D10):
  App-Bin `010ea7dd9c54e8e4400a9d04b82e48ef37dc0b439fa573ecf23bebb23e80243b`.
  Das Probe-Image wurde zuerst geflasht und zeigte den Boot-Loop; danach wurde
  das exakte Image geflasht und zeigte denselben Fehler (gleiche Groesse
  13176 Byte, gleiche Stelle). Das Geraet blieb danach mit dem exakten Image
  `475db0c` geflasht.
- Lokale Rohlogs (nicht im Repository), SHA-256:

```text
bac26bf2601f821311b34452cd5c6e8f87dd4d50f47ba27cfc3633d43d62f070  uart_01.raw.txt (Probe-Build)
243ac87d5b93ccd7a058ff9e620fe5cb8c2cd0f93f998efd34a07ac924c99f54  uart_02_exact.raw.txt (exakt)
eac765ba264025167fa44fc8a8dfed5b22efb3e5b8a79869a9e9b8fe9a9a2cac  flash.log.txt (Probe)
00bb3e2806a95c2dcefbdd0dce61474fec7d9564c46399abf2bc7f6acf99032f  flash_exact.log.txt
```

Keine Zugangsdaten wurden erfasst (die WLAN-SSID im Log ist die des Testnetzes).
Nicht ausgefuehrt, weil blockiert: alle interaktiven Pruefungen (Owner-Touch).
`ACTUATOR_RELEASE=NO`, reale Aktoren deaktiviert.
