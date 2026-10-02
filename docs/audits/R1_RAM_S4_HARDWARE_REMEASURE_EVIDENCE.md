# R1-RAM – S4 Hardware-Re-Messung gegen S2/S3

Auftrag `PR174_S4_Hardware_Remeasure_Followup`. Nur Messung und Dokumentation;
keine weitere RAM-Optimierung, kein Fix, kein S5, keine Budgets.

```text
S4_CODE=PASS
S4_FIX1=PASS
S4_HARDWARE_REMEASURE=FAIL_STOP_CONDITION_NONFATAL_HEAP_ALLOC_FAILED_X2
STEADY_STATE_HEADERNETWORK_120S=PASS
BROWSER_ACCESS=PASS_PAGE_LOADED_DEVICE_STABLE
NETWORK_MODE_SWITCH_BY_TOUCH=NOT_EXECUTED_RUN_STOPPED
HOME_NETWORK_CONNECT_VIA_SETUP_PAGE=OWNER_OBSERVED_SUCCESS_NOT_IN_UART_LOG
ABORT_OR_RESET=0
S5_TO_S11=NOT_STARTED
OPEN_OWNER_GATES=O4_O5
```

## 1. Aufbau

- `esp32_release`, frisch gebaut aus HEAD
  `e5a69f1887f4f48fbf4d25fea8d07af4cc4260b8` (S4-Fix-1-Code
  `68acea04…`, Unterschied nur Dokumentation), 0 Warnungen, Profilvalidierung
  PASS. Source-SHA in der Firmware bestätigt. App-BIN SHA-256
  `af7f5eca1d82e514e6c6a6015456a010e646ed43b8dc4d066d01ff03af3450ca`,
  ELF `0c6c40142aec122a0b81f04431d0c185517ed6b6c619bcf1ff98894ca61f4855`,
  sdkconfig `6edbf61555023d42da33cdf212fbd1b6030b5af3c4b1203f5440cd6055b8e09f`
  (unverändert: Main-Stack 24576, LVGL builtin 64 KiB, kein PSRAM).
- Geflasht ohne Erase (nur Bootloader, Partitionstabelle, App);
  `state_store`, Touchkalibrierung und Produktkonfiguration erhalten,
  persistierter Zustand `HOME_WIFI`/`SetupAccessPoint`.
- UART ab Reset durchgehend mitgeschnitten:
  [R1_RAM_S4_REMEASURE_20261002_RAW.txt](R1_RAM_S4_REMEASURE_20261002_RAW.txt)
  (Heartbeats entfernt, die MAC-Adresse des Handys maskiert; ~570 s),
  Messpunkte in [R1_RAM_S4_REMEASURE_20261002_POINTS.csv](R1_RAM_S4_REMEASURE_20261002_POINTS.csv).
- Boot: `application: ready`, `touch calibration: active_status=Available`,
  ein `POWERON_RESET` (Mitschnittstart), kein
  `UnsupportedNewerConfigurationSchema`, kein Panic, Watchdog oder Brownout.

## 2. Ablauf

| Schritt | Ergebnis |
|---|---|
| Boot, 30 s, `periodic_30s`, `idle_120s` (erstes) | stabil |
| `HeaderNetwork` öffnen, 2 Presses | stabil |
| 120 s Idle auf `HeaderNetwork` (260,8 s bis 381,1 s) | **stabil**: kein `heap_alloc_failed`, kein Reset; Heap konstant 9624 B frei, Minimum 4888 B, größter Block 7424 B, Stack-HWM 12944 B |
| Handy mit dem AP verbunden (438 s), Setup-Seite `192.168.4.1` aufgerufen (~465 s) | Seite lädt, Gerät stabil (Owner) |
| Zwei `heap_alloc_failed` (je 1532 B, caps `0x1800`) bei ~476 s | Stopbedingung; **kein** `abort()`, kein Reset, kein Neuaufbau des Screens in diesem Fenster (kein Messpunkt, kein Press); Heartbeats laufen bis zum Mitschnittende (568 s) weiter |
| Moduswechsel `HOME_WIFI` → `AP_ONLY` per Touch | **nicht ausgeführt** (Lauf wegen der Stopbedingung beendet) |

## 3. Auswertung der Stopbedingung

`heap_alloc_failed: size=1532 caps=0x00001800 function=heap_caps_malloc`,
zweimal direkt hintereinander, etwa 11 s nach dem Browserzugriff und 38 s
nach dem Beitritt des Clients. Es gibt keinen Panic und keinen Backtrace, ein
Symbolisieren ist daher nicht möglich; der Hook meldet nur Größe, Caps und
`heap_caps_malloc`, nicht den Aufrufer. Aus den Daten folgt:

- Der Fehlschlag war **nicht fatal**. Die früheren OOM-Abstürze in
  `makeRepresentativeScreen()` und im Configuration-Commit waren
  C++-`operator new`-Fehlschläge mit `abort()`; hier trat kein `abort()` auf,
  die Allokation wurde also von einem Aufrufer behandelt, der Fehlschläge
  zurückmeldet (vermutlich Treiber-/Netzwerkcode; nicht belegt).
- Es wurde in diesem Fenster **kein** Screen-Modell aufgebaut: kein Press, keine
  Änderung des Tastenzustands, keine `network_page_press_*`- oder
  `resources`-Zeile; `makeRepresentativeScreen()` ist nicht als Fehlerstelle
  beobachtet.
- Dieselbe Größe 1532 B trat in S2 und S3 vor den Abstürzen in
  `makeRepresentativeScreen()` auf (vector der Zeichenbefehle). Ob es dieselbe
  Allokationsquelle ist, ist aus dem Log **nicht** zu entscheiden.
- Der Heap lag zum Zeitpunkt knapp (Minimum 4888 B, größter Block 7424 B);
  das Ereignis fällt mit dem Client-Betrieb (WLAN-Verkehr, Browsersitzung)
  zusammen. Eine Ursache wird nicht behauptet.

## 4. Vergleich S2 / S3 / S4

| Kennzahl | S2 | S3 | S4 |
|---|---|---|---|
| `HOME_WIFI` frei nach `after_ui_init` | 10016–10284 B | 10156–10288 B | 9972 B |
| Minimum frei nach `after_ui_init` | 5196–5236 B | 5108–5252 B | 4888 B |
| größter Block nach `after_ui_init` | 7680 B | 7168–7936 B | 7424 B |
| Steady State (`periodic_30s`/`idle_120s`) frei / min | 10432–10452 / 4484–4680 B | 9392–9516 / 4580–4692 B | 8696–9624 / 4888 B (konstant) |
| globales Heap-Minimum | 2384 B | 1872 B | **4888 B** |
| kleinster 8-Bit-Block | 4352 B | 3200 B | **7424 B** |
| Main-Task-Stack-HWM (`HOME_WIFI`) | 13936 B | 13680 B | **12944 B** |
| LVGL-Pool max belegt / % | 15848 B / 24 % | 15852 B / 24 % | 15852 B / 24 % |
| `heap_alloc_failed` | 7 | 6 | 2 |
| Abort / Reset | 5 | 2 | 0 |
| `makeRepresentativeScreen()`-OOM | ja | ja | nicht beobachtet |

INTERNAL und DMA stimmen mit den allgemeinen Heap-Werten überein (kein
PSRAM, überlappende Sichten). Vorsicht: S4 enthält **keinen
Moduswechsel**, die S2/S3-Minima (2384 B, 1872 B, kleinster Block 4352/3200 B)
stammen aus Modus-Commits auf `AP_ONLY`. Das globale Minimum und der kleinste
Block von S4 sind deshalb mit S2/S3 nicht gleichwertig vergleichbar. Gleich
vergleichbar sind die Werte nach `after_ui_init` und im `HOME_WIFI`-Steady-
State: Dort liegt S4 beim freien Heap nicht über S2/S3 (9972 B gegenüber
≈ 10,0–10,3 kB), das Minimum ist mit 4888 B etwas niedriger; im Steady State
ist es dagegen stabil und ändert sich nicht. Der Stack-HWM ist um 736 B
niedriger als in S3, passend zum um 720–736 B größeren `app_main`-Frame
(3872 B). Eine Verbesserung des Heaps wird aus diesen Messwerten **nicht**
behauptet.

## 4a. Nachtrag: Heimnetz-Verbindung nach Mitschnittende (Owner-Beobachtung)

Nach den zwei `heap_alloc_failed` habe ich den UART-Mitschnitt beendet
(Stopregel, ~568 s). Der Owner hat danach **über die Setup-Seite das Gerät mit
dem Heimnetz verbunden**; das Gerät ist seither im Heimnetz unter
`192.168.1.68` auffindbar. Ein Ping vom Entwicklungsrechner bestätigt das
(3/3 Antworten, 0 % Verlust, ~50 ms; das ist kein Teil der UART-Evidenz).

Was belegt ist und was nicht:

- **Belegt:** Erreichbarkeit des Geräts im Heimnetz (Ping).
- **Owner-Beobachtung, nicht im Log:** Die Verbindung mit dem Heimnetz gelang
  und das Gerät lief danach weiter. In S2 und S3 führten die
  Heimnetz-Wechsel zu Abstürzen (Configuration-Commit, Whitescreen).
- **Nicht belegt:** Heap, Minimum, Stack-HWM und Reset-/`heap_alloc_failed`-
  Verhalten während und nach dem Verbindungsaufbau, weil der Mitschnitt
  vorher endete. Ein `stable_home_wifi`-Messpunkt wurde nicht erfasst.

Die Aussage „Moduswechsel nicht ausgeführt“ in Abschnitt 2 gilt für den
Wechsel `HOME_WIFI` → `AP_ONLY` per Touch; die Heimnetz-Verbindung über die
Setup-Seite ist davon getrennt und wurde nicht aufgezeichnet. Eine
Wiederholung mit durchgehendem Mitschnitt (Reset ohne Flash und ohne Erase,
persistierter Zustand mit gespeicherten Zugangsdaten) ist als nächster
Messschritt vorgeschlagen, aber nicht ausgeführt und nicht freigegeben. Am
Gerät wurde nach dem Mitschnittende nichts geändert.

## 5. Ergebnis nach der Entscheidungsregel

Fall B (weiterhin OOM-Ereignis), aber ohne Reset und ohne Abort:

- Positiv belegt: 120-s-Idle auf `HeaderNetwork` ohne `heap_alloc_failed` und
  ohne Reset; Browserzugriff auf die Setup-Seite im Modus
  `HOME_WIFI`/`SetupAccessPoint` ohne Absturz (in S2 und S3 stürzte das
  Gerät dabei ab); 0 Aborts und 0 Resets im gesamten Lauf; kein
  `makeRepresentativeScreen()`-OOM beobachtet.
- Offen: zwei nicht fatale Allokationsfehler (1532 B) nach dem Browserzugriff
  mit unbekanntem Aufrufer; der Moduswechsel (der Peak, der in S3 den
  Configuration-Commit-Absturz auslöste) wurde nicht ausgeführt. S4 wird
  nicht zurückgebaut, kein Fix, kein S5/S6/S9/S10 begonnen. Der nächste
  Schnitt wird aus dieser Evidenz entschieden.
