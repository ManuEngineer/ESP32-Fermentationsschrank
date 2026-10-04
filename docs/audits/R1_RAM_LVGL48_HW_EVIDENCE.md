# R1-RAM – LVGL-Pool 48 KiB: fokussierter Hardwaretest

Auftrag `PR174_LVGL48K_Focused_Hardware_Test`. Nur Messung und Dokumentation;
keine Codeänderung, keine Korrektur während der Messung, kein S9–S11, keine
WLAN-/lwIP-/Stack-/ConfigurationGraphStore-Änderung, keine O4/O5-Entscheidung,
reale Aktoren deaktiviert. Die vollständige S6-Netzwerkmatrix wurde bewusst
nicht wiederholt.

## 1. Provenienz

```text
SOURCE_HEAD=6c1e159bc14159eb144f3564c1d2a87154c8ea71
PROFILE=esp32_release
CONFIG_LV_MEM_SIZE=49152
CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576
CONFIG_SPIRAM=OFF
BIN_SHA256=305da6d49d89dc3a5406c50caf1eb9a11fb4c43187801d96a0a2cd63c16d3e19
ELF_SHA256=2dc88b3b3883f22e533ded9b14acd3c0bff44e5f7a4987004ba4b038abe1f630
SDKCONFIG_SHA256=e8d9202435a37b5872818f51d31efe13530f52c4b73b40668db5ec6b88b1eac6
ARTIFACTS_MATCH_REQUIRED_HASHES=YES
```

- Die lokal erhaltenen Artefakte wurden vor dem Flashen gegen diese SHA-256
  geprüft; sie stimmten exakt.
- Geflasht wurden nur Bootloader (`0x1000`), Partitionstabelle (`0x8000`) und
  App (`0x10000`) mit Hash-Verify. Kein Full-Erase, kein NVS-/State-Store-Erase;
  Touchkalibrierung und WLAN-Konfiguration blieben erhalten.
- Der UART-Mitschnitt wurde unmittelbar nach dem Flash gestartet; das Öffnen
  des Ports löst einen Reset aus, sodass der Mitschnitt den Boot (`POWERON_RESET`)
  ab der ersten Zeile enthält. Der Mitschnitt lief ohne Unterbrechung über die
  gesamte Messung (Geräte-Uptime 0 … 9873 s).
- Der Code-Unterschied zwischen dem S6-Referenzstand `0beafdf` und diesem Build
  ist außerhalb von `docs/` ausschließlich `sdkconfig.defaults`
  (`git diff --stat 0beafdf 6c1e159 -- . ':!docs'`).
- Die Originaldatei des Mitschnitts (688513 B, SHA-256
  `0524351ee1258f0d48c87680db526a3bd360252905e0eebbf23072cd915082a9`) liegt
  unsanitisiert lokal außerhalb des Repositorys. Committet ist die
  sanitisierte Fassung (`R1_RAM_LVGL48_HW_20261004_RAW.txt`): Heartbeat-Zeilen
  entfernt (9857, nur `uptime_ms`), SSID, MAC-Adressen und Heimnetz-IPs
  maskiert; es wurden keine Zugangsdaten ausgegeben. Setup-AP-Adressen
  (`192.168.4.x`) bleiben stehen.

## 2. Bootgate

```text
application: ready                         YES (Uptime 1765 ms)
touch calibration active_status=Available  YES (fallback_status=NotFound)
ServiceRequired                            NO
UnsupportedNewerConfigurationSchema        NO
Panic                                      NO
Watchdog                                   NO
Brownout                                   NO
ACTUATOR_RELEASE                           NO (real actuators: disabled, hardware state HARDWARE_UNVERIFIED)
```

## 3. Basis-Messung und Vergleich 64 KiB (S6) → 48 KiB

Alle Werte stammen aus `R1_RAM_LVGL48_HW_20261004_POINTS.csv`; die S6-Werte aus
`R1_RAM_S6_HW_CAMPAIGN_EVIDENCE.md` / S8. INTERNAL|8BIT und DMA sind in allen
Punkten identisch bis auf wenige Bytes (kein PSRAM, überlappende Sichten; nicht
addieren).

| Kennzahl | 64 KiB / S6 | 48 KiB | Delta |
|---|---:|---:|---:|
| `after_ui_init` free heap | 20468 B | 37176 B | **+16708 B** |
| `stable_home_wifi` free heap | 18624 B | 35084 B | **+16460 B** |
| `stable_ap_only` free heap | 15284–15744 B | 32420 B | +16676…+17136 B |
| pre-switch Client+Browser free heap | 12288 B | 28600 B | +16312 B |
| pre-switch largest block | 8192 B | 24576 B | +16384 B |
| network-switch low-water mark (`minimum_free_heap`) | 4124 B | 20612 B | +16488 B |
| LVGL gemeldeter Pool (`pool_total`) | ~63384 B | 46724–47000 B | −16660…−16384 B |
| LVGL `max_used` | 15856 B | 15852 B | −4 B |
| Main stack HWM min | 6256 B | 6256 B | 0 B |
| 1696-B-Idle-Failure | 2× in S6-Kampagne | 0 in 87,7 min | – |
| Abort/Reset | 0 | 0 (nur `POWERON_RESET` zu Beginn) | – |

Weitere Messwerte der Basis (kein Vergleichswert in S6 verfügbar):

- `after_ui_init`: min free 31432 B, largest 31744 B, Main-Stack-HWM 12944 B;
  LVGL pool_total 46724 B, free 33596 B, largest 33228 B, max_used 13940 B
  (29 %), frag 2 %.
- `stable_home_wifi`: min free 31432 B, largest 31744 B, Stack-HWM 12944 B;
  LVGL free 33632 B, max_used 13940 B (29 %), frag 2 %.

```text
STATIC_DRAM_REDUCTION_BYTES=16384
MEASURED_RUNTIME_FREE_HEAP_GAIN_AFTER_UI_INIT=16708
MEASURED_RUNTIME_FREE_HEAP_GAIN_STABLE_HOME_WIFI=16460
MEASURED_RUNTIME_FREE_HEAP_GAIN_STABLE_AP_ONLY=16676..17136
```

Statische Einsparung (Build, −16384 B) und gemessener Laufzeitgewinn sind
getrennte Aussagen. Der gemessene Gewinn liegt an den Basispunkten 76…324 B über
der statischen Reduktion. Ursache dieser Differenz wird nicht behauptet
(Einzelmessung, Zustandsabhängigkeit, S6-Werte aus anderer Sitzung).

## 4. Repräsentativer UI-/LVGL-Smoke

Durchgeführt vom Owner an der produktiven UI: Home und alle bestehenden Seiten
außer der Netzwerkseite durchgeklickt; danach Netzwerkseite und QR-Darstellung.
Der Sprachwechsel ist im produktiven Pfad nicht verfügbar (Owner-Meldung) und
wurde nicht ausgeführt; das ist kein Fehler dieses Tests.

Beobachtet: keine Owner-Meldung über leeren/weißen Screen, Hängen oder
sichtbare UI-Regression (keine gezielte Einzelbestätigung je Seite), kein `heap_alloc_failed`, kein LVGL-Fehler im
Log. LVGL-Pool an den Messpunkten:

| Punkt (Uptime) | pool_total | free | largest | max_used | used | frag |
|---|---:|---:|---:|---:|---:|---:|
| `stable_home_wifi` (3245 ms) | 46724 | 33632 | 33264 | 13940 | 29 % | 2 % |
| `network_page_press_after` (2182935 ms) | 47000 | 36540 | 33864 | 14060 | 23 % | 8 % |
| `stable_home_wifi_setup_access_point` (2183305 ms, QR sichtbar) | 46892 | 32004 | 30372 | **15848** | 32 % | 6 % |
| `network_page_press_before` AP→Heim (4608695 ms) | 46912 | 32176 | 31240 | 15852 | 32 % | 3 % |
| `stable_home_wifi` danach (4611625 ms) | 47000 | 36552 | 36356 | 15852 | 23 % | 1 % |

Einschränkung: `max_used` wird nur an den Ressourcenpunkten geloggt
(Boot, `periodic_30s`, `idle_120s`, Netzwerkseiten-Touch). Während des
Durchklickens der übrigen Seiten gibt es keinen Messpunkt; der Peak ist durch
den erst später geloggten Wert nach dem QR-Aufbau (15848 B) nach oben
eingeschlossen, weil `max_used` ein historisches Maximum ist.

```text
LVGL_POOL_CONFIG_BYTES=49152
LVGL_POOL_USABLE_BYTES=46724..47000 (gemeldeter pool_total)
LVGL_MAX_USED_BYTES=15852
LVGL_MAX_USED_PERCENT=33.7 (15852 / 47000; jeweils aktuelle used_pct höchstens 32)
LVGL_POOL_RESERVE_AT_PEAK_BYTES=31148 (47000 − 15852)
LVGL_ALLOC_FAILURE=NO
```

## 5. Kombinierter Worst-Case (einmalig)

Ablauf (Uptime aus dem Mitschnitt; keine Wiederholung):

| Schritt | Uptime | Beobachtung |
|---|---:|---|
| `HOME_WIFI` → Netzwerkseite (Setup-AP temporär) | 2182535–2183205 ms | `network_state=SetupAccessPoint`, free 30348 B |
| `HOME_WIFI` → `AP_ONLY` per Touch | 2281515–2281715 ms | `AP_ONLY/AccessPointOnly`, Stack-HWM 12944 → 6256 B |
| `stable_ap_only` | 2281925 ms | free 32420 B, largest 25600 B, min 22872 B |
| `idle_120s` (AP_ONLY, ohne Client) | 2402055 ms | free 32628 B |
| Client verbindet / trennt (Owner war zwischenzeitlich weg) | 3711055 / 3952995 / 4229775 / 4231785 / 4580545 ms | Join, Leave `reason=4`, Join, Leave `reason=1`, Join (letzter Join 28 s vor dem Rückwechsel) |
| Browser: Setup-Seite `192.168.4.1` geladen | vor 4608695 ms | Owner-Meldung „Seite geladen“, Seite beim Rückwechsel offen |
| Ressourcenpunkt unmittelbar vor Rückwechsel | 4608695 ms | free 28600 B, largest 24576 B, min 22832 B |
| `AP_ONLY` → `HOME_WIFI` per Touch | 4608695–4608925 ms | `ConnectingHome`, free 36004 B, min **20612 B**, largest 21504 B |
| `stable_home_wifi` | 4611525 ms | `HomeConnected`, free 35756 B, largest 21504 B |
| Weboberfläche im Heimnetz einmal geöffnet | nach 4611525 ms | Owner-Meldung „Geöffnet“ |

Während des synchronen Moduswechsel-Fensters sank `minimum_free_heap` von
22832 B auf 20612 B (−2220 B); S6 sank im gleichen Fenster auf 4124 B. Der
Wert ist wie in S8 das Low-Water-Mark des gesamten Fensters
(`processWorkspaceTouch`), die Teilphase ist nicht instrumentiert.

Log-Warnungen im Wechselfenster (identisches Muster in der S6-Kampagne,
dort 5×):
`W httpd_txrx: httpd_sock_err: error in recv : 113` bei 4608915 ms und 4608925 ms
(2× nach dem Wechsel; ein HTTP-Socket wurde beim Abbau des AP beendet,
Ursache nicht untersucht; beobachtet, nicht gewertet). Außerdem beim Boot
`W lcd_panel.io.spi: Callback on_color_trans_done was already set …`
(unverändert bekannt) und die üblichen WPA2-Hinweise.

```text
COMBINED_CLIENT_BROWSER_COMMIT=PASS
CONFIGURATION_COMMIT_OOM=NO
MAKE_REPRESENTATIVE_SCREEN_OOM=NO
COMBINED_WORST_CASE_COUNT=1
```

n = 1 Durchlauf, ein Client. Das ist ein Messwert dieses Falls, keine
statistische Aussage.

## 6. HOME_WIFI-Idle / 1696-B-Follow-up

- Idle ab `network_page_press_after` (4608925 ms) bis zum Ende des Mitschnitts
  (9873525 ms): **87,7 min**; Ende auf Ownerauftrag ca. 14:00 Uhr (Wanduhr).
- Keine Touch-/Browseraktivität im Idle (Owner abwesend).
- Mitschnitt lückenlos: 9857 Heartbeats, größte Lücke 1540 ms (bei 2181765 ms,
  Netzwerkseiten-Touch), keine Rückwärtssprünge, keine Reboots.
- `heap_alloc_failed size=1696 caps=0x1800`: **0**; überhaupt kein
  `heap_alloc_failed`.
- Einschränkung: Ressourcenpunkte werden nur bei 30 s und 120 s nach Boot
  geloggt; der letzte Heap-Messpunkt im Idle ist `idle_120s` bei 4729195 ms
  (free 35452 B, min 20612 B). Für die restlichen ~85 min gibt es keine
  Heap-Stichprobe, nur das Fehlen von Fehlermeldungen. Der Failed-Alloc-Hook
  war durchgehend aktiv.
- Eine fehlende Beobachtung in 87,7 min ist keine Beseitigung des Befundes
  (frühere Läufe: 4 Ereignisse in 3 Läufen, keine belastbare Rate).

```text
IDLE_1696B_COUNT=0
IDLE_DURATION_MINUTES=87.7
```

## 7. Zähler und Abbruchkriterien

```text
ABORT_COUNT=0
RESET_COUNT=1_POWERON_AT_CAPTURE_START_ONLY
WATCHDOG_COUNT=0
BROWNOUT_COUNT=0
PANIC_COUNT=0
HEAP_ALLOC_FAILED_COUNT=0
SERVICE_REQUIRED=NO
PERSISTENCE_SCHEMA_ERROR=NO
STOP_CRITERION_HIT=NO
```

## 8. Maschinenblock

```text
LVGL_POOL_48K_HARDWARE=PASS

MEASURED_RUNTIME_FREE_HEAP_GAIN_AFTER_UI_INIT=16708
MEASURED_RUNTIME_FREE_HEAP_GAIN_STABLE_HOME_WIFI=16460
MEASURED_RUNTIME_FREE_HEAP_GAIN_STABLE_AP_ONLY=16676..17136
STATIC_DRAM_REDUCTION_BYTES=16384

LVGL_POOL_USABLE_BYTES=46724..47000
LVGL_MAX_USED_BYTES=15852
LVGL_MAX_USED_PERCENT=33.7
LVGL_ALLOC_FAILURE=NO

COMBINED_CLIENT_BROWSER_COMMIT=PASS
CONFIGURATION_COMMIT_OOM=NO
MAKE_REPRESENTATIVE_SCREEN_OOM=NO

PRE_SWITCH_CLIENT_BROWSER_FREE_HEAP_BYTES=28600
PRE_SWITCH_CLIENT_BROWSER_LARGEST_BLOCK_BYTES=24576
NETWORK_MODE_SWITCH_LOW_WATER_MARK_BYTES=20612
LOWEST_MAIN_STACK_HWM_BYTES=6256

IDLE_1696B_COUNT=0
ABORT_COUNT=0
RESET_COUNT=1_POWERON_AT_CAPTURE_START_ONLY
WATCHDOG_COUNT=0
BROWNOUT_COUNT=0

O4=DEFERRED_PENDING_INDEPENDENT_REVIEW   # Stand bei Messende, siehe Statusabschluss
O5=DEFERRED_PENDING_INDEPENDENT_REVIEW   # Stand bei Messende, siehe Statusabschluss
S9_TO_S11=NOT_STARTED
```

`LVGL_POOL_48K_HARDWARE=PASS` gilt für den in diesem Auftrag definierten
fokussierten Scope (ein Release-Build, ein Gerät, ein kombinierter Lastfall,
87,7 min Idle). Es ist keine Aussage über die volle S6-Matrix, über andere
Seiten-/Sprachpfade (Sprachwechsel nicht verfügbar) oder über den 1696-B-Befund.

## 9. Offene Punkte für den Independent Hardware Review (Stand bei Messende, historisch)

- Ob 48 KiB ausreichen: Peak 15852 B bei 46724–47000 B Pool (Reserve rund
  31 kB); Peak nur an den geloggten Punkten bekannt.
- Wie viel reale Systemreserve gewonnen wurde: Gewinn je Messpunkt siehe §3.
- Ob weitere RAM-Optimierung sinnvoll ist, und die finalen O4/O5-Empfehlungen.
- 1696-B-Befund bleibt `FOLLOW_UP_NON_BLOCKING_FOR_PR174`; keine WLAN-/lwIP-
  Ursache behauptet.
- Keine Ownerentscheidung simuliert; `PR174_FINAL_GO=NO_UNTIL_OWNER_O4_O5`.

## 10. Statusabschluss (nach Independent Review)

Der Independent Hardware Review ist abgeschlossen, der Owner hat O4 und O5
freigegeben. Die Messdaten oben bleiben unverändert.

```text
LVGL_POOL_48K_HARDWARE=PASS
O4=APPROVED
O5=APPROVED
OPEN_OWNER_GATES=0
S9=NOT_NEEDED_FOR_PR174
S10=NOT_NEEDED
S11=NOT_NEEDED
FURTHER_PROACTIVE_RAM_OPTIMIZATION=NO
IDLE_1696B_CLASSIFICATION=FOLLOW_UP_NON_BLOCKING_FOR_PR174
PR174_FINAL_GO=PENDING_INDEPENDENT_FINAL_FIX_VERIFICATION
```

`IDLE_1696B_COUNT=0` in 87,7 min ist positive Evidenz, aber kein Nachweis einer
Ursachenbehebung; der Befund bleibt offenes Follow-up.

Rohdaten: `R1_RAM_LVGL48_HW_20261004_RAW.txt` (sanitisiert),
`R1_RAM_LVGL48_HW_20261004_POINTS.csv` (17 Ressourcenpunkte).
