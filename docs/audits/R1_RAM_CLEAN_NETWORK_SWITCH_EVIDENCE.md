# R1-RAM – Clean Network-Switch Hardware Re-Measurement (S4-Stand)

Auftrag `PR174_Clean_Network_Switch_Hardware_Test`. Nur Messung und
Dokumentation; keine Codeänderung, kein S5, kein Tuning, keine Budgets.

```text
BUILD_WORKTREE_CLEAN=YES
APP_VERSION_DIRTY=NO
HOME_WIFI_TO_AP_ONLY=PASS
AP_ONLY_TO_HOME_WIFI=FAIL_ABORT_IN_CONFIGURATION_COMMIT
BROWSER_ACCESS=PASS_IN_AP_ONLY_BEFORE_THE_RETURN_SWITCH_PROTOCOL_DEVIATION
HEAP_ALLOC_FAILED_COUNT=2
ABORT_COUNT=1
RESET_COUNT=1
MAKE_REPRESENTATIVE_SCREEN_OOM=NO
CONFIGURATION_COMMIT_OOM=YES
S5_TO_S11=NOT_STARTED
OPEN_OWNER_GATES=O4_O5
```

## 1. Provenienz

- Quell-HEAD `c08948681fb04ef6b5b749c8d7a49e1c9f33e948` (S4-Code
  `68acea04…`, Unterschied nur Dokumentation).
- Der Haupt-Worktree enthielt unverändert die nicht committete
  `.codex/config.toml` des Owners (Ursache des früheren `-dirty`). Sie wurde
  weder bereinigt noch verworfen; stattdessen wurde aus einem **sauberen
  Git-Worktree** auf dem exakten HEAD gebaut (`git status --short` leer,
  Treiber mit `--require-clean-source-tree`). Boot meldet
  `App version: c089486` (ohne `-dirty`) und
  `source git sha: c08948681fb04ef6b5b749c8d7a49e1c9f33e948`.
- Profil `esp32_release`, ESP-IDF v6.1, 0 Warnungen. App-BIN SHA-256
  `00a51d2f102392cade0303a33fd5a1e4b5191ad9ca296c3555f68d0fad6c1006`, ELF
  `eef53f9f7bbf72755261a5ed083c8cc9a11e0907bdc01bf847e314d3d4cd9b21`,
  sdkconfig `6edbf61555023d42da33cdf212fbd1b6030b5af3c4b1203f5440cd6055b8e09f`
  (unverändert).
- Ohne Erase geflasht; `state_store`, Touchkalibrierung und gespeicherte
  Heimnetz-Konfiguration erhalten. UART ab Reset durchgehend:
  [R1_RAM_CLEAN_NETWORK_SWITCH_20261003_RAW.txt](R1_RAM_CLEAN_NETWORK_SWITCH_20261003_RAW.txt)
  (Heartbeats entfernt; Heimnetz-SSID, BSSID und MAC-Adressen maskiert),
  [Punkte-CSV](R1_RAM_CLEAN_NETWORK_SWITCH_20261003_POINTS.csv).
- Boot-Gate bestanden: `application: ready`, Touch `Available`, kein
  `UnsupportedNewerConfigurationSchema`, kein Panic/Watchdog/Brownout, ein
  `POWERON_RESET`. Das Gerät startet direkt in `HOME_WIFI` und erreicht
  `HomeConnected` nach 4,2 s (`stable_home_wifi`).

## 2. Ablauf und Abweichungen vom Protokoll

| Phase | Ergebnis |
|---|---|
| A: `HOME_WIFI` stabil, Idle | stabil bis `idle_120s`; danach lief das Gerät etwa eine Stunde ohne Bedienung im Idle (der Test begann erst bei Uptime ~3837 s) |
| **Abweichung 1:** `heap_alloc_failed` im Idle bei Uptime ~3450 s | 1× 1696 B (caps `0x1800`), nicht fatal, kein Abort/Reset, kein Screen-Aufbau, Heartbeats liefen weiter. Nach der Stopregel wäre der Test hier zu beenden gewesen; der Owner hat ausdrücklich entschieden, für Messdaten weiterzumachen. |
| B: `HOME_WIFI` → `AP_ONLY` per Touch (3837,5 s) | **PASS**: `network_mode=AP_ONLY`, `AccessPointOnly`, kein neues `heap_alloc_failed`, kein Abort/Reset; 30 s Ruhe danach |
| **Abweichung 2:** Der Owner hat vor dem Rückwechsel ein Handy mit dem AP verbunden und `192.168.4.1` aufgerufen | Seite lädt (Owner); damit lief der Rückwechsel **nicht** ohne Browser-/Clientlast. Das Protokoll sah die Browserlast erst nach beiden Wechseln vor. |
| C: `AP_ONLY` → `HOME_WIFI` per Touch (nach `network_page_press_before` bei 3918,5 s) | **FAIL**: Whitescreen = `abort()` nach `heap_alloc_failed`; Neustart (`SW_CPU_RESET`), danach wieder `application: ready`, Touch `Available`, `HOME_WIFI` |

Die Teilaussagen sind getrennt zu lesen: Der Wechsel `HOME_WIFI` → `AP_ONLY`
ohne Client lief sauber. Der Rückwechsel scheiterte **mit** verbundenem Client
(Heap vor dem Press: frei 7428 B, Minimum 2192 B, größter Block 5632 B, gegenüber
frei 10396 B / größter Block 8192 B ohne Client bei `stable_ap_only`). Ob er ohne
Client ebenfalls gescheitert wäre, ist aus diesem Lauf nicht zu entscheiden.

## 3. Absturz (ELF `eef53f9f…`, dekodiert)

`heap_alloc_failed: size=192 caps=0x00001800`, danach `abort()`:

`updateProductUi` → `processWorkspaceTouch` → `dispatchWorkspacePress` →
`FermentationUiCommandBridge::applyNetworkMode` →
`FermentationApplication::applyNetworkMode` →
`ConfigurationService::confirmPreview` →
`ConfigurationGraphStore::executePreparedCommit` → `validationScan` →
`scanGroupMetadata<4>` (`vector<RecordDescriptor>` / `operator new`).

Das ist dieselbe Absturzstelle wie in S3 (Configuration-Commit beim Wechsel
`AP_ONLY` → `HOME_WIFI`, damals ebenfalls 192 B, frei 7864 B, Minimum 1872 B,
größter Block 3200 B). `makeRepresentativeScreen()` trat nicht als
Absturzstelle auf. Nach dem Neustart steht der persistierte Modus auf
`HOME_WIFI`; warum, ist hier nicht untersucht.

## 4. Messpunkte (Auszug; vollständig in der CSV)

| Boot | ms | Punkt | Modus/Zustand | free | min free | größter 8-Bit-Block | Stack-HWM | LVGL frei / max / % |
|---|---|---|---|---|---|---|---|---|
| 1 | 1175 | after_platform_begin | UNSELECTED/Stopped | 154744 | 154744 | 110592 | 19680 | – |
| 1 | 1715 | after_application_begin | HOME_WIFI/ConnectingHome | 64060 | 60060 | 59392 | 12944 | – |
| 1 | 2235 | after_ui_init | HOME_WIFI/ConnectingHome | 15620 | 10612 | 12800 | 12944 | 49980 / 13940 / 21 |
| 1 | 4215 | stable_home_wifi | HOME_WIFI/HomeConnected | 13776 | 10124 | 12800 | 12944 | unavailable |
| 1 | 32325 | periodic_30s | HomeConnected | 13668 | 9820 | 12800 | 12944 | 50016 / 13940 / 21 |
| 1 | 122325 | idle_120s | HomeConnected | 13708 | 7904 | 12800 | 12944 | 50016 / 13940 / 21 |
| 1 | 3837495 | network_page_press_before | HomeConnected | 14456 | 2336 | 12800 | 12944 | 52924 / 13940 / 17 |
| 1 | 3837995 | network_page_press_after | AP_ONLY/AccessPointOnly | 10916 | 2336 | 8192 | 6256 | 52924 / 13940 / 17 |
| 1 | 3838395 | stable_ap_only | AccessPointOnly | 10396 | 2336 | 8192 | 6256 | 48388 / 15848 / 24 |
| 1 | 3918515 | network_page_press_before (Client verbunden) | AccessPointOnly | 7428 | 2192 | 5632 | 6256 | 48560 / 15848 / 24 |
| 2 | 2235 | after_ui_init (nach Neustart) | HomeConnecting | 15616 | 10108 | 13312 | 12944 | 49980 / 13940 / 21 |
| 2 | 4235 | stable_home_wifi | HomeConnected | 13564 | 9980 | 12288 | 12944 | unavailable |
| 2 | 32325 | periodic_30s | HomeConnected | 13944 | 4608 | 12288 | 12944 | 50016 / 13940 / 21 |

INTERNAL und DMA stimmen mit den allgemeinen Heap-Werten überein (kein
PSRAM). `network_page_press_after` für den Rückwechsel fehlt (Absturz davor).

**Globale Werte:** `heap_alloc_failed` 2 (1696 B im Idle, 192 B im Commit),
Abort 1, Resets 1 (zusätzlich der Power-on-Reset), kleinster 8-Bit-Block
5632 B, globales Heap-Minimum 2192 B, niedrigster Stack-HWM 6256 B,
`makeRepresentativeScreen()`-OOM nein, Configuration-Commit-OOM ja.

## 5. Vergleich (nur gleichwertige Pfade)

| | S2 (`UNSELECTED` → `AP_ONLY`) | S3 (`HOME_WIFI` → `AP_ONLY`) | dieser Lauf (`HOME_WIFI` → `AP_ONLY`) |
|---|---|---|---|
| frei vor / nach Press | 76792 / 11452 B | 9860 / 11316 B | 14456 / 10916 B |
| größter Block nach Press | 11264 B | 3200 B | 8192 B |
| Stack-HWM nach Press | 7072 B | 6864 B | 6256 B |
| Ergebnis | ok | ok | ok |

| | S3 Rückwechsel `AP_ONLY` → `HOME_WIFI` | dieser Lauf |
|---|---|---|
| Ausgangsheap | frei 7864 B, Minimum 1872 B, größter Block 3200 B | frei 7428 B, Minimum 2192 B, größter Block 5632 B (Client verbunden) |
| Ergebnis | Abort, 192 B, `validationScan`/`scanGroupMetadata` | Abort, 192 B, `validationScan`/`scanGroupMetadata` |

Die Heap-Minima sind nicht vergleichbar, weil sie im Idle bereits vor den
Wechseln gefallen waren (hier 2336 B vor dem ersten Press, im S3-Lauf 3392 B).
Browserlast wird separat betrachtet: Im S4-Lauf `e5a69f1` (Setup-Seite im
`HOME_WIFI`/`SetupAccessPoint`) stürzte das Gerät nicht ab, hier lief die
Seite im `AP_ONLY` ohne Fehler, doch der anschließende Commit scheiterte.
Daraus folgt keine Verbesserung durch S3/S4 am Commit-Peak: Die Absturzstelle
und die fehlschlagende Allokationsgröße sind unverändert zu S3.

## 6. Einordnung

- Ein Heimnetz-Betrieb (`HomeConnected`, kein SoftAP) hat deutlich mehr Heap
  als `SetupAccessPoint` (frei 13,6–14,5 kB und größter Block 12,3–12,8 kB statt
  ≈ 10 kB und 7,4–7,7 kB); das Minimum sinkt im Idle über Zeit auf
  2,3–7,9 kB. Zwei nicht fatale/transient niedrige Phasen im Idle sind belegt
  (7904 B bei 122 s; 2336 B vor dem ersten Press; 4608 B bei `periodic_30s`
  nach dem Neustart). Ursachen sind nicht untersucht.
- Der Configuration-Commit beim Rückwechsel nach `HOME_WIFI` ist der
  reproduzierbare, noch nicht behobene Peak (S3 und dieser Lauf, gleiche
  Stelle und Größe). Der nächste Schnitt wird aus dieser Evidenz vom
  Review/Owner entschieden; es wurde nichts implementiert.
- Gerätezustand nach dem Lauf: persistierter Modus `HOME_WIFI`, Gerät im
  Heimnetz; am Gerät wurde nichts geändert.

## 7. Nachtrag: Owner-Beobachtungen nach Mitschnittende (nicht im UART-Log)

Der Mitschnitt endete nach dem Absturz und dem Neustart (siehe Abschnitt 3).
Danach hat der Owner gemeldet:

- Die Webseite des Geräts unter `192.168.1.68` im Heimnetz funktioniert
  (nach dem Neustart im `HOME_WIFI`-Zustand).
- Der Owner hat danach wieder auf `AP` umgestellt („Bin wieder auf ap“).

Beides ist Owner-Beobachtung. Heap-, Reset- und `heap_alloc_failed`-Verhalten
während dieser Vorgänge sind nicht aufgezeichnet; es wurde in dieser Phase
kein Test durch mich ausgeführt und am Gerät nichts geändert. Der aktuelle
Gerätezustand ist daher nicht mehr gleich dem im Abschnitt 6 genannten
(persistierter Modus nach dem Lauf: `HOME_WIFI`; danach laut Owner wieder
`AP`).
