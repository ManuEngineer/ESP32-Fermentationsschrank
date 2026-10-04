# R1-RAM – S6 Hardware-Messreihe (direkter Draw-Pfad, Heap-Gewinn, Netzwerk-Matrix, Idle)

Auftrag `PR174_S6_Hardware_Full_Measurement_GO`. Nur Messung und Dokumentation;
keine Codeänderung, kein S7, kein WLAN-/Stack-/ConfigurationGraphStore-Tuning,
keine O4/O5-Entscheidung, reale Aktoren deaktiviert.

```text
S6_DIRECT_DRAW_HARDWARE_SMOKE=PASS_VISUAL_COMPLETE_ROTATION_NOT_ASSESSED_BY_OWNER

S6_MEASURED_HEAP_GAIN_AFTER_UI_INIT=+4796..+4980_BYTES_(S6_20468_VS_PRE_S6_15488..15672)
S6_MEASURED_HEAP_GAIN_STABLE_HOME_WIFI=+4800..+5060_BYTES_(S6_18624_VS_PRE_S6_13564..13824)
S6_MEASURED_HEAP_GAIN_STABLE_AP_ONLY=+4640..+5404_BYTES_NO_CLIENT_(MEAN_+5018)

AP_ONLY_TO_HOME_WIFI_NO_CLIENT_RUN1=PASS
AP_ONLY_TO_HOME_WIFI_NO_CLIENT_RUN2=PASS

AP_ONLY_TO_HOME_WIFI_CLIENT_NO_PAGE_RUN1=PASS
AP_ONLY_TO_HOME_WIFI_CLIENT_NO_PAGE_RUN2=PASS

AP_ONLY_TO_HOME_WIFI_CLIENT_PAGE_OPEN=PASS

CONFIGURATION_COMMIT_OOM_COUNT=0
COMMIT_FAILED_ALLOC_SIZE=NONE
MAKE_REPRESENTATIVE_SCREEN_OOM=NO

IDLE_1696B_FAILURE=YES
IDLE_1696B_COUNT=2

ABORT_COUNT=0
RESET_COUNT=1_POWERON_AT_CAPTURE_START_ONLY
WATCHDOG_COUNT=0
BROWNOUT_COUNT=0

S7_TO_S11=NOT_STARTED
```

Alle Aussagen sind Messwerte dieser Serie; Ursachen werden nicht behauptet.
Jeder Matrixfall ist ein einzelner Durchlauf (n = 1 je Fall, n = 2 je
Bedingung ohne Client und mit Client ohne Seite).

## 1. Build-Provenienz

Aus einem sauberen Git-Worktree (`git status --short` leer) auf dem PR-HEAD
`0beafdfdc724f5d9a8687f49c82796a506c4ff19`, ESP-IDF v6.1, Treiber mit
`--require-clean-source-tree`, 0 Warnungen, Profilvalidierung PASS, kein
`-dirty`. Der Release-Boot meldet `App version: 0beafdf` und
`source git sha: 0beafdfdc724f5d9a8687f49c82796a506c4ff19`.

| Build | App-BIN SHA-256 | **geflashter** ELF SHA-256 | sdkconfig SHA-256 |
|---|---|---|---|
| `esp32_release` (Profil release) | `f794ab7a6827d28ef9d94c0ba3ac231584141aa481fbe2094c3586661e41f9c5` | `02d72ba6080d69d1d2e2d6bf40042239f85ef076306ed4cec7cc3840c0952a9d` | `6edbf61555023d42da33cdf212fbd1b6030b5af3c4b1203f5440cd6055b8e09f` |
| `esp32_bringup` (nicht geflasht) | `5cf68763eb26580bd5733578ecc44d365aa38a33d85ffa34b6fe86c2bc4f320e` | `23a8642b10a73db8ca118bb3bf0d323859dd8c00e8e26634b9e3a976d2a4dba6` | `044f9dcd1de26bd006e5208601479748c22667f074e2cebf3d38592989caa1f5` |
| Issue-31-Display-Harness (`esp32_bringup` + `APP_ISSUE_31_TOUCH_CALIBRATION_HARNESS`) | `a48d6e4dbd75055a65677db8285e686de46860599f05f0885e849be3807524b4` | `ecae2b988ee72d9ab9097dd56df5493dd559522d642bf96a36ea88af792309c1` | `afb177b0776b7079bafcfca95ce32f5d6f3b309e8d69c950c1e3588c1a4fed95` |

Die exakten geflashten BIN/ELF/Map/sdkconfig/Bootloader/Partitionstabelle
liegen schreibgeschützt außerhalb des Repositories unter
`~/esp32-fermentationsschrank-backups/s6_hw_20261003_0beafdf/` und werden bis
nach dem Independent Review nicht durch einen Rebuild ersetzt. Der Release-ELF
wurde vor dem Flashen per SHA-256 gegen diese Datei geprüft.

## 2. Ablauf und Abweichungen

1. **Display-Smoke:** Harness geflasht (nur Bootloader, Partitionstabelle, App;
   kein Erase). Der Owner führte die Referenz-/Fadenkreuz-Sequenz bis zum Ende
   aus (`CALIBRATION_CAPTURE_HARNESS=COMPLETE`, 179 Samples,
   `CALIBRATION_RECORD_WRITTEN=NO`, `FIT_VALUES_COMPUTED=NO`). Das war der
   vollständige Capture-Ablauf, nicht nur ein kurzer Smoke; die bestehende
   Touchkalibrierung blieb unverändert. UART: kein Panic, Watchdog,
   Transferfehler, `heap_alloc_failed` oder zusätzlicher Reset. Visuell
   (Owner): Bild vollständig, korrekte Anzeige. **Rotation:** Der Owner kann sie
   nicht beurteilen; der erste Zielpunkt (`FIT_TOP_LEFT`, 32/32) erschien oben
   links, was zur Erwartung passt, aber keine Rotationsprüfung ist.
   Rohlog ohne die 179 `CALIBRATION_SAMPLE`-Zeilen:
   [R1_RAM_S6_HW_DRAW_SMOKE_20261003_RAW.txt](R1_RAM_S6_HW_DRAW_SMOKE_20261003_RAW.txt).
2. **Release-Flash** ohne Erase (nur Bootloader, Partitionstabelle, App);
   `state_store`, Touchkalibrierung und Netzwerkkonfiguration erhalten.
   Durchgehender UART-Mitschnitt ab Reset, beendet erst auf Owner-Wunsch nach
   ~5028 s Uptime (~84 min) ohne Unterbrechung:
   [R1_RAM_S6_HW_CAMPAIGN_20261003_RAW.txt](R1_RAM_S6_HW_CAMPAIGN_20261003_RAW.txt)
   (Heartbeats entfernt; Heimnetz-SSID, BSSID, MAC- und LAN-Adressen maskiert),
   [Punkte-CSV](R1_RAM_S6_HW_CAMPAIGN_20261003_POINTS.csv).
3. **Boot-Gate:** `application: ready`, Touch `Available`, kein
   `UnsupportedNewerConfigurationSchema`, kein `ServiceRequired`, kein Panic,
   Watchdog oder Brownout, `ACTUATOR_RELEASE=NO`. Der einzige Reset ist der
   `POWERON_RESET` beim Öffnen des seriellen Ports.
4. **Matrix** (alle Rückwechsel per Touch auf der Netzwerkseite):

| Fall | Zeit (ms) | Bedingung | Ergebnis |
|---|---|---|---|
| A | 123 / 175 | `HOME_WIFI` → `AP_ONLY` → `HOME_WIFI`, kein Client | PASS |
| B | 429 / 522 | wie A | PASS |
| C | 743 / 836 | `AP_ONLY` mit Client (Join 755 s, DHCP `192.168.4.2`), keine Seite bewusst geöffnet | PASS |
| D | 970 / 1026 | wie C (Join 980 s) | PASS |
| E | 1263 / 1365 | Client (Join 1271 s), Setup-Seite `192.168.4.1` vom Owner mehrfach geladen, Rückwechsel mit offener Browser-Sitzung | PASS |

5. **Nachlauf im Heimnetz:** Die Geräte-Webseite im Heimnetz lud normal
   (Owner). Danach 2 Minuten (`idle_120s` bei 1485 s) und anschließend
   Ruhe ohne Touch und ohne Browser bis zum Mitschnittende (~61 min Idle ab
   `stable_home_wifi` bei 1367 s; ein einzelner `httpd_sock_err ... recv : 104`
   bei 2507 s deutet auf einen kurzen Verbindungsabbruch eines Browsers, nicht
   gedeutet).

Abweichungen von der Vorgabe: Der Harness-Lauf war vollständig statt kurz. Der
Reset beim Öffnen des Ports gehört zum Start des Mitschnitts. Die Handy-
Beitritte in den Fällen C/D lösen vermutlich automatische Anfragen des Handys
aus (`httpd_sock_err: error in recv : 113` im Log bei jedem Client-Rückwechsel,
ohne Deutung); die Fälle gelten bewusst als „ohne Webseite“, weil der Owner
keine Seite geöffnet hat. Für den Fall E ist die Seitenlast nur als Zustand vor
dem Press (`network_page_press_before`) erfasst, nicht während des Ladens.

## 3. Vergleich Pre-S6 (Clean-Reihen) → S6

Pre-S6: S4-Stand/Clean-Build `c089486`
([R1_RAM_CLEAN_NETWORK_SWITCH_EVIDENCE.md](R1_RAM_CLEAN_NETWORK_SWITCH_EVIDENCE.md),
[R1_RAM_CLIENT_LOAD_COMMIT_EVIDENCE.md](R1_RAM_CLIENT_LOAD_COMMIT_EVIDENCE.md)),
ebenfalls im Heimnetz-Start, daher gleichwertige Pfade.

| Kennzahl | Pre-S6 | S6 |
|---|---:|---:|
| `after_ui_init` frei | 15488–15672 B | **20468 B** |
| `after_ui_init` Minimum | 9984–10612 B | 13228 B |
| `stable_home_wifi` frei | 13564–13824 B | **18624–19308 B** (Boot, nach Wechseln 18888–19308 B) |
| `stable_ap_only` ohne Client frei | 10340–10736 B | **15284–15744 B** |
| `network_page_press_before` mit Client frei | 7300–7756 B | 12288–12620 B |
| Client-Heap-Kosten (`stable_ap_only` → Press, mit Client) | 2980–3096 B | 2796–2996 B |
| größter Block vor dem Press ohne Client | 7936–8192 B | 12800 / 10752 B |
| größter Block vor dem Press mit Client | 4864–6400 B | 8192 B (C, D, E) |
| Low-Water-Mark im erfolgreichen Moduswechsel-Fenster | 2872–2956 B (ohne Client) | 7880 / 7856 B (ohne), 4124 B (C), kein neues Minimum in D und E |
| Stack-HWM Minimum | 5936 B | 6256 B |
| LVGL max belegt | 15848–15860 B (24 %) | 15856 B (24 %) |
| 192-B-Commit-OOM | 3/3 mit Client (ohne Seite 1×, mit Seite 2×) | **0** |
| 1696-B-Idle-Failure | 2 | **2** |
| Abort / Reset | 3 Aborts | **0** / nur Power-on |

Die Vor-S6-Werte stammen aus mehreren Boots und Läufen und streuen um einige
hundert Byte; die Bereiche sind Minimum und Maximum der dort gemessenen Punkte.

```text
MEASURED_S6_FREE_HEAP_DELTA_AFTER_UI_INIT=+4796..+4980_B   (mean_pre_S6_15575_B_vs_S6_20468_B_=_+4893_B)
MEASURED_S6_FREE_HEAP_DELTA_STABLE_HOME_WIFI=+4800..+5060_B (S6_18624_B_at_boot)
MEASURED_S6_FREE_HEAP_DELTA_STABLE_AP_ONLY=+4640..+5404_B   (mean_+5018_B_no_client)
MEASURED_CLIENT_HEAP_COST_AFTER_S6=2796..2996_B             (mean_2883_B_vs_pre_S6_2980..3096_B)
```

Der gemessene Gewinn des freien Heaps liegt bei etwa 4,8–5,0 kB und damit in
der Nähe, aber nicht exakt bei den 5120 B des entfernten Puffers (Allokator-
Overhead, Boot-zu-Boot-Streuung; ein einzelner S6-Boot).

DMA und INTERNAL stimmen mit den allgemeinen Heap-Werten überein (kein PSRAM,
überlappende Sichten); die DMA-Reserve steigt entsprechend (`dma_free_bytes`
20468 B nach `after_ui_init`).

## 4. Kennzahlen der Serie

| Kennzahl | Wert |
|---|---|
| globales Heap-Minimum | 4124 B (Fall C) |
| kleinster 8-Bit-Block | 7168 B (nach Fall E) |
| niedrigster Main-Task-Stack-HWM | 6256 B (ab erstem `AP_ONLY`) |
| LVGL-Pool maximal belegt | 15856 B (24 %) |
| `heap_alloc_failed` | 2, beide 1696 B, caps `0x1800` |
| Abort / Panic / Watchdog / Brownout | 0 / 0 / 0 / 0 |
| `makeRepresentativeScreen()`-OOM | nein |
| Configuration-Commit-OOM | nein (0) |

Der Heap fiel über die Wechsel hinweg stufenweise (größter Block 14848 →
12800 → 10752 → 8192 → 7424 → 7168 B) und erholte sich nicht; das Minimum
sank in den Moduswechsel-Fenstern von Fall A bis C von 11364 B auf 4124 B und blieb dann
stehen.

## 5. 1696-B-Idle-Fehler

Zwei nichtfatale Fehlschläge (`size=1696 caps=0x1800`), beide im reinen
`HOME_WIFI`/`HomeConnected`-Idle ohne Touch und ohne Browser, kein Abort,
kein Reset, kein Screen-Aufbau:

Zeitstempel-Herkunft: Die Zeiten stammen aus dem **ursprünglichen,
unveränderten UART-Mitschnitt** dieser Sitzung (lokal gesichert, unsanitisiert,
nicht im Repository: `s6_campaign_ORIGINAL_UNSANITIZED.raw`, SHA-256
`d02928ac11c66371b5a0435b640e38df587a47c74bf6a657f1048325155551cc`,
378762 Bytes, identisch zur Capture-Datei der Messreihe). Die Fehlerzeile
trägt selbst keinen Zeitstempel (ESP-ROM-Ausgabe aus dem Allokator-Hook); sie ist
durch die beiden benachbarten 1-s-Heartbeats des Originals eingeklammert
(Auszug: [R1_RAM_S6_HW_IDLE_1696B_EVENT_BRACKETS.txt](R1_RAM_S6_HW_IDLE_1696B_EVENT_BRACKETS.txt)).
Der committete sanitisierte Rohlog enthält keine Heartbeats und erlaubt diese
Herleitung allein nicht.

```text
IDLE_1696B_TIMESTAMP_SOURCE=ORIGINAL_UNSANITIZED_UART_CAPTURE_SHA256_d02928ac11c66371b5a0435b640e38df587a47c74bf6a657f1048325155551cc
IDLE_1696B_EVENT1_UPTIME_MS=BETWEEN_1745915_AND_1746915_LOG_TICK_(HEARTBEAT_UPTIME_1744744..1745744)
IDLE_1696B_EVENT2_UPTIME_MS=BETWEEN_3370355_AND_3371355_LOG_TICK_(HEARTBEAT_UPTIME_3369183..3370183)
```

| # | Fenster (Log-Tick, ms seit Boot) | Zeit seit `stable_home_wifi` (1367505) | letzter Ressourcenpunkt davor | letztes WLAN-Ereignis davor |
|---|---|---|---|---|
| 1 | 1745915 … 1746915 | ~378–379 s | `idle_120s` bei 1485175: frei 18924 B, Minimum 4124 B, größter Block 7168 B, Stack-HWM 6256 B | `RX DELBA reason:39` bei 1502505 (~243 s vorher) |
| 2 | 3370355 … 3371355 | ~2003 s | derselbe | keines im Log davor (letztes: DELBA 1502505) |

Die früher genannten ungefähren Werte „~1745,9 s“ und „~3370,4 s“ entsprachen
dem unteren Ende dieser Ein-Sekunden-Fenster. Nach beiden Ereignissen zeigt
das Log in den folgenden ~12 Minuten keinen Abort, Reset, keine WLAN-Trennung
und keine Zustandsänderung; das nächste WLAN-Ereignis nach Ereignis 2 ist ein
normales `ADDBA`/`DELBA` (3632405 / 3653275).

Beide Male war der letzte gemessene Heap hoch (≈ 18,9 kB frei, größter Block
7168 B, deutlich über 1696 B). Wie die Allokation dennoch fehlschlug, ist aus
den Daten nicht ableitbar: Es gibt keinen Ressourcenpunkt im Fehlerzeitpunkt,
und der Hook nennt nur Größe, Caps und `heap_caps_malloc`, nicht den Aufrufer
(keine neue Instrumentierung in diesem Auftrag). Der Fehler trat vor und nach
S6 gleich auf (2 vor S6, 2 in S6) und ist als **separater offener
Follow-up-Befund** zu führen.

## 6. Einordnung

- S6 hat in dieser Serie den freien Heap um rund 4,8–5,0 kB erhöht und den
  zuvor 3/3 reproduzierbaren Configuration-Commit-Abort mit Client nicht mehr
  ausgelöst: Fälle C, D und E (Client ohne Seite, zweimal, und mit Seite)
  liefen durch, ohne `heap_alloc_failed` im Commit.
- Es ist kein Beweis, dass der Abort „behoben“ ist. Je Bedingung liegen nur
  ein bis zwei Durchläufe vor, die Low-Water-Mark im Moduswechsel-Fenster sank mit Client auf
  4124 B (gegenüber 2576–2956 B vor dem Absturz davor) und die Reserve schrumpft
  über wiederholte Wechsel (Block 14848 → 7168 B). Eine Zuordnung des
  Erfolgs allein zum Wegfall des Puffers ist aus dieser Serie nicht ableitbar
  (Heap-Reserve und Fragmentierung sind gleichzeitig verändert).
- `makeRepresentativeScreen()` trat nicht als OOM auf (seit S4 nicht mehr).
- Der 1696-B-Idle-Befund bleibt bei höherer Reserve unverändert bestehen.
- S6 wurde nicht zurückgebaut, es wurde nichts implementiert, S7 nicht
  begonnen. Die Beurteilung (R1-RAM-Blocker geschlossen, S7-Nutzen, 1696-B-
  Follow-up, S8) bleibt dem Independent Review.

Hinweis zu 4124 B: Der Wert ist die Low-Water-Mark des gesamten synchronen
Netzwerkmoduswechsel-Fensters (`network_page_press_before` bis `_after` umfasst
auch die Transport-/Network-Lifecycle-Aktivierung), nicht exklusiv ein
Configuration-Commit-Wert; die Teilphase ist nicht instrumentiert.
