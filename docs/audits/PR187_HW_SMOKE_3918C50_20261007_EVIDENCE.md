# PR #187 – Hardware-Fix-Verification und Teil-Smoke auf 3918c50 (2026-10-07/08)

Folgt auf `PR187_HW_BOOTLOOP_FIX_VERIFICATION_20261007_EVIDENCE.md` und
`PR187_RAM_HEADROOM_DIAGNOSIS_20261007_EVIDENCE.md`. Die frueheren FAIL-
Evidences (`475db0c`, `fb8978f`) bleiben unveraendert.

```text
PRODUCT_HEAD=3918c508e6c5d10fc09c80b32003585534990c23
PROFILE=esp32_release (sauberer Worktree, --require-clean-source-tree, App version 3918c50)
PRE_READY_LOCAL_ON_PRODUCT_HEAD=PASS
RESOURCE_EVIDENCE_BUILD=3918c50 + probe-only instrumentation (PR187_RAM_DIAG_AND_D10_PROBE_3918c50.patch), App version 3918c50-dirty
FLASH_ERASE=NO (Partitionstabelle byte-identisch d7f180e4...)
ACTUATOR_RELEASE=NO (real actuators: disabled)

RAM_LVGL_FIX_VERIFICATION=PASS
HEAP_ALLOC_FAILED=0
BAD_ALLOC_ABORT_PANIC_RESET_LOOP=0
WATCHDOG=0 BROWNOUT=0
LVGL_DISPLAY=INITIALIZED (Pool 46724 B)
BOOT_STABILITY=PASS
UART_IDLE_STABILITY_2H=PASS (Aufnahme 20:20-22:20: 1 Boot, 0 Fehler, Uptime 7199 s)

HARDWARE_SMOKE_PR187=PARTIAL_OWNER_OBSERVED_NOT_COMPLETE
```

## 1. RAM-/LVGL-Fix: PASS

Exakter Build bootet stabil (`application: ready`, Touch-Kalibrierung verfuegbar,
LVGL-Pool vorhanden). Freier Heap in Byte (frei / Minimum / groesster 8-Bit-Block,
Probe-Build mit Messpunkten):

| Messpunkt | `2e81d09` Referenz | `fb8978f` (FAIL) | `3918c50` |
|---|---:|---:|---:|
| vor Textpacks | 71372 | 71388 | 72084 |
| nach Textpacks | 46428 / 20996 / 20480 | 18564 / 16792 / 17408 | **70740 / 69440 / 65536** |
| Textpack-Verbrauch | 24944 | 52824 | **1344** |
| vor Display-Init | 45480 / 20996 / 20480 | 17588 / 16792 / 16384 | 70132 / 69240 / 65536 |
| nach Display-Init | 22252 / 20248 / 20480 | 7240 / 7180 / 6912 (Allokation 12800 B fehlgeschlagen) | **46948 / 45164 / 45056** |
| stabil Home/WLAN | 20112 / 16068 / 18432 | 7752 / 4696 / 6912 | **44752 / 40176 / 43008** |

Gewinn gegenueber `fb8978f` im stabilen Zustand ca. 37 kB, gegenueber der
Referenz `2e81d09` ca. 24,6 kB. Display-Init braucht wie auf der Basis ca. 23 kB.
Der exakte Build zeigte denselben stabilen Zustand (frei 44756 B, Minimum 41172 B,
groesster Block 43008 B). Keine neuen Grenzwerte abgeleitet.

## 2. Smoke – Owner-Beobachtungen (nicht UART-belegt)

Die Interaktionen wurden vom Owner nach der UART-Aufnahme (20:20-22:20)
durchgefuehrt; es existiert keine UART-Aufzeichnung der Touch-Handlungen und
keine `commit_probe_*`-Messung (die Aufnahme enthielt 0 Touch-Dispatches). Die
Ergebnisse stammen allein aus der Meldung des Owners.

```text
S1_PROGRAM_LIST_START_SELECT=PASS_OWNER_OBSERVED   (Programmseite, Start -> Programm auswaehlen)
S3_LANGUAGE_CHANGE_AND_PERSISTENCE=PASS_OWNER_OBSERVED   (Sprache geaendert, Reset, noch gespeichert)
S4_LOCAL_TIME_AND_CLOCK_PAGE=PASS_OWNER_OBSERVED   (Zeit korrekt inkl. Seite)
NETWORK_WLAN_PAGE=PASS_OWNER_OBSERVED
S6_PROGRAM_EDIT_AND_PERSISTENCE=PASS_OWNER_OBSERVED   (Programm bearbeitet, Reset, noch gespeichert)
S10_SETTINGS_MENU=PASS_OWNER_OBSERVED
S10_KEYBOARD=PASS_OWNER_OBSERVED   (bedienbar; 30x34-px-Treffbarkeit nicht separat vermessen)
SERVICE_ENTRY=FAIL_OR_EXPECTED_SEE_BELOW   (nicht oeffenbar, keine PIN-Abfrage)
WEB_ACCESS_PAGE=OBSERVATION_SEE_BELOW   (zeigt nur "Webzugang nicht verfuegbar")
OWNER_STABILITY=NO_CRASH_NO_WHITESCREEN_NO_OTHER_PROBLEM   (Owner)
S7_CONTENT_PAGES_LAYOUT=NOT_RUN
S8_S9_INPUT_AND_FAIL_CLOSED=NOT_RUN
S10_DEVICE_NAME_CHANGE_AND_PERSISTENCE=NOT_RUN
S10_LABEL_WIDTHS_EINSTELL=NOT_RUN
D10_COMMIT_RESOURCE_LOGS=NOT_RUN   (Probe-Build vorhanden, aber Interaktion ohne Aufnahme)
S2=NOT_APPLICABLE
S5=NOT_APPLICABLE
```

### Service nicht oeffenbar

Beobachtung: `Service (PIN)` laesst sich nicht oeffnen, es erscheint keine PIN-
Abfrage. Quellbefund (nicht auf dem Geraet nachgewiesen): `snapshot.service.available`
ist `false` (`ServiceAvailabilityView`/`FermentationUiServiceSource`, Default
`false`); kein Producer setzt es auf `true`. Service-/PIN-Owner sind bis #28
zurueckgestellt (Seiten zeigen `zurueckgestellt (#28)`). Damit ist das Verhalten
mit dem fail-closed Design erklaerbar; ob der Eintrag in diesem Zustand
**deaktiviert mit sichtbarem Grund** erscheint, wurde nicht dokumentiert. Kein
Fix, Owner entscheidet, ob dies als erwartet gilt.

### Webzugang "nicht verfuegbar"

Die Seite zeigt "Web setup not available"-Text, wenn
`FermentationApplication::webAccessState()` `NotApplicable` liefert, d. h. wenn
der Authentifizierungs-Bootstrap nicht erlaubt ist (z. B. Zugang bereits
provisioniert, Auth-Kontext nicht bereit). Dieses Geraet war in frueheren
Hardware-Gates (PR #170) provisioniert; die Ursache fuer genau diesen Zustand
wurde nicht gemessen. Kein Fix, kein Befund bis zur Klaerung durch den Owner.

## 3. Nicht geprueft / offen

S7-Layout, S8/S9-Eingabepfade, Geraetename aendern und Persistenz, Abschneiden
von `Einstell.` und Zeilentexten, 30-/34-px-Tastenbewertung im Detail und alle
D10-Commit-Ressourcenlogs (Sprache, Programm, Geraetename) sind `NOT_RUN`. Der
Smoke ist daher **nicht vollstaendig**; ein finales GO ist daraus nicht
ableitbar.

## 4. Provenienz

```text
602a77a01ccdfd1434758d01f4b5c5672cecf7513aaea7bd7e1772616440affa  bootloader.bin
59918c4d8ff57756674cc55b53923ff75198756cb5fc575b3b5c52200d179d12  esp32_fermentationsschrank.bin (exakt, 3918c50)
d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d  partition-table.bin
8f03ee7b75c89b64f962501ecc140ec6fd7eb865d85314125752be4e49444d56  Probe-Build app (3918c50-dirty)
323649fc62d26335225a1cc7354c96fc218583d3789f9db4890d11f09f7f8c21  uart_01_exact.raw.txt
d0fc038baa338572d575eaecf80c2552a13291accf1997b6626c5736e47c43e4  uart_02_probe_boot.raw.txt
98f279781d6881de9f2e4b11075122e128790ef42ff74e75db4066823f0bd859  uart_03_smoke.raw.txt (Aufnahme 20:20-22:20; die ersten 487 Zeilen sind ein Pufferartefakt beim Oeffnen des Ports, der Boot beginnt in Zeile 488)
8cbb4581eb68ea5354adb4aa9c6dfcff93ff6e0679b8aa3f01439d7e904276a8  flash_exact.log
f96bf7203f9294778ee3f5333b72c0f9f4a61830de0c13ceeac516327f4f7abf  flash_probe.log
```

Rohlogs lokal (nicht im Repository). Nach der Aufnahme wurde das Geraet mit dem
exakten Produkt-Image `3918c50` neu geflasht (ohne Erase). `ACTUATOR_RELEASE=NO`.
