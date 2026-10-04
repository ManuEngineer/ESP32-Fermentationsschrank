# R1-RAM – S3 Hardware-Re-Messung gegen die S2-failing-baseline

Auftrag `PR174_S3_Hardware_Re-Messung`. Nur Messung und Dokumentation; keine
weitere RAM-Optimierung, kein Fix, keine Budgets, keine Kausalbehauptung, die
aus den Messungen nicht folgt. S4–S11 nicht begonnen.

```text
S3_CODE=PASS
S3_HARDWARE_REMEASURE=FAIL_STOP_CONDITION
STOP_EVENTS=heap_alloc_failed_x6,abort_x2
S4_TO_S11=NOT_STARTED
OPEN_OWNER_GATES=O4_O5
```

## 1. Aufbau

- Firmware `esp32_release` aus dem S3-Stand, Quell-SHA
  `1019d35ac0558742f96d6b723081b323101be52c` (im Boot-Log bestätigt).
  Mit `esptool write-flash @flash_args` aus dem unveränderten Build geflasht
  (kein Neubau, kein Erase).
- App-BIN SHA-256 `920207ebb0c42bb07d4b3e31eb41b744011c1a59cb836c97ab9a0d3c4637094e`,
  ELF `f5edfe41dcb61913ec05eaa9372290f003e978077a2f9371c3de25b1dc8689a9`,
  sdkconfig `6edbf61555023d42da33cdf212fbd1b6030b5af3c4b1203f5440cd6055b8e09f`
  (identisch zur S2-Konfiguration: Main-Stack 24576, LVGL builtin 64 KiB,
  `# CONFIG_SPIRAM is not set`, WLAN-/lwIP-Werte unverändert).
- S2-Instrumentierung unverändert. Persistierter Zustand
  `HOME_WIFI`/`SetupAccessPoint` erhalten, keine Provisionierung,
  `application: ready`, Touchkalibrierung `Available`.
- UART ab Reset mitgeschnitten: [R1_RAM_S3_REMEASURE_20261002_RAW.txt](R1_RAM_S3_REMEASURE_20261002_RAW.txt)
  (Heartbeats entfernt), alle Messpunkte in
  [R1_RAM_S3_REMEASURE_20261002_POINTS.csv](R1_RAM_S3_REMEASURE_20261002_POINTS.csv).

## 2. Ablauf (Owner-Beobachtung und Log)

| Schritt | Beobachtung |
|---|---|
| Boot 1 (`HOME_WIFI`), 30 s ruhig | stabil, `periodic_30s` erfasst |
| Verbindung mit AP, Aufruf `192.168.4.1` | ok (Owner) |
| Moduswechsel `HOME_WIFI` → `AP_ONLY` (Netzwerkseite) | erfolgreich (`network_mode=AP_ONLY`, `AccessPointOnly`) |
| Wechsel auf Heimnetz (`AP_ONLY` → `HOME_WIFI`) | Whitescreen = **Absturz 1** |
| Boot 2 (`HOME_WIFI`), erneut mit AP verbunden, Aufruf `192.168.4.1` | Seite erreichbar; nach drei Netzwerkseiten-Presses **Absturz 2** (Whitescreen) |
| Boot 3 (`HOME_WIFI`) | ohne Bedienung stabil bis `idle_120s` (Mitschnitt danach beendet) |

Stopbedingungen: 6× `heap_alloc_failed`, 2× `abort()`/`SW_CPU_RESET`, kein
Watchdog, kein Brownout, kein `ServiceRequired`, kein
`UnsupportedNewerConfigurationSchema`. Der O2-Rest (10 Seitenwechsel,
bis zu 5 Moduswechsel, 120 s Idle nach Interaktion) wurde wegen der
Stopbedingung nicht fortgesetzt; Sprachwechsel bleibt
`NOT_AVAILABLE_IN_CURRENT_R1_PATH`.

## 3. Absturzstellen (ELF `f5edfe41…`, dekodiert)

| Absturz | Zeitpunkt | fehlgeschlagene Allokation | Stelle |
|---|---|---|---|
| 1 | Boot 1, nach `network_page_press_before` bei 133606 ms (`AP_ONLY`; frei 7864 B, Minimum 1872 B, größter Block 3200 B) | 192 B | `updateProductUi` → `processWorkspaceTouch` → `dispatchWorkspacePress` → `FermentationUiCommandBridge::applyNetworkMode` → `FermentationApplication::applyNetworkMode` → `ConfigurationService::confirmPreview` → `ConfigurationGraphStore::executePreparedCommit` → `validationScan` → `scanGroupMetadata<4>` (`vector<RecordDescriptor>`) |
| 2 | Boot 2, nach dem dritten `network_page_press_after` bei 12145 ms (`HOME_WIFI`; frei 9564 B, Minimum 4772 B) | 4× 1532 B, 1× 1344 B | `app_main` → `updateProductUi` → `ProductiveLvglRenderer::render` → `makeRepresentativeScreen` (`vector<ScreenDrawCommand>`) |

Der Configuration-Commit scheiterte (Absturz 1) bei einer Modusumschaltung
aus `AP_ONLY`; `makeRepresentativeScreen()` erscheint weiterhin als
OOM-Absturzstelle (Absturz 2). Die HTTP-Route trat in diesem Lauf nicht als
Absturzstelle auf (in S2 einmal).

## 4. Vergleich mit der S2-Referenz

S2: Lauf `R1_RAM_BASELINE_S2_O2_RUN_20261002`, 6 Boots, davon 5 in
`HOME_WIFI`. S3: 3 Boots, ebenfalls ab persistiertem `HOME_WIFI`.

| Kennzahl | S2 | S3 |
|---|---|---|
| `HOME_WIFI` `after_application_begin` free / min | 56544–56864 / 52572–52876 B | 56556–56736 / 52568–52748 B |
| `HOME_WIFI` `after_ui_init` free / min (Referenz ≈ 10 kB / ≈ 5,2 kB) | 10016–10284 / 5196–5236 B | 10156–10288 / 5108–5252 B |
| `HOME_WIFI` `stable_…_setup_access_point` free / min | 9936–10284 / 5144–5192 B | 9216–9348 / 5060–5208 B |
| `HOME_WIFI` `periodic_30s` ohne Presses free / min | 10432–10452 / 4484–4680 B | 9392–9516 / 4592–4692 B |
| `HOME_WIFI` `idle_120s` free / min | 10432 / 4664 B | 9392 / 4580 B |
| größter 8-Bit-Block nach UI-Init (`HOME_WIFI`) | 7680 B | 7168–7936 B |
| INTERNAL free / min / largest, DMA free / min / largest | gleich dem `free`/`min`/Blockwert (kein PSRAM, überlappend) | gleich dem `free`/`min`/Blockwert |
| Main-Task-Stack-HWM `HOME_WIFI` / `AP_ONLY` | 13936 / 7072 B | 13680 / 6864 B |
| LVGL-Pool `HOME_WIFI` frei / max belegt / Frag | 49980 / 13940 B / 1 % (bis 48552 / 15848 B / 4 %) | 49980 / 13940 B / 1 % (bis 48560 / 15852 B / 4 %); 1× `unavailable` |
| globales Heap-Minimum | **2384 B** (`HOME_WIFI`, `periodic_30s`) | **1872 B** (`AP_ONLY`, Modus-Commit `HOME_WIFI` → `AP_ONLY`) |
| kleinster beobachteter 8-Bit-Block | **4352 B** (`AP_ONLY`) | **3200 B** (`AP_ONLY`) |
| `heap_alloc_failed` | 7 | 6 |
| Abort / Reset (`SW_CPU_RESET`) / Panic / Watchdog / Brownout | 5 / 5 / 5 (Abort-Panic) / 0 / 0 | 2 / 2 / 2 / 0 / 0 |

Vorsicht beim Lesen: Die Läufe sind nicht deckungsgleich. S2 startete
`UNSELECTED` und schaltete erstmals auf `AP_ONLY`; S3 startete in
`HOME_WIFI` und schaltete auf `AP_ONLY`. Die Anzahl der Abstürze und
Fehlschläge hängt von der Bedienung und der Laufdauer ab (S2: 6 Boots,
S3: 3 Boots) und ist kein Maß für den Effekt von S3.

## 5. Gezielte Fragen

- **Heap nach UI-Init bzw. in `HOME_WIFI` messbar höher?** Nein. Nach
  `after_ui_init` liegen S3 (10156–10288 B) und S2 (10016–10284 B) im
  selben Bereich; im Steady State (`periodic_30s`, `idle_120s`) liegt S3 mit
  9392–9516 B nicht über S2 (10432–10452 B).
- **`uiPresentationSource()` im unveränderten Steady State?** Aus der
  vorhandenen Evidenz **nicht erkennbar**: Es gibt keinen Zähler oder Log für
  Aufrufe. Auch der freie Heap im Steady State bleibt zwischen
  `periodic_30s` und `idle_120s` konstant (9392 B), sagt aber nur, dass dort
  keine dauerhaft wachsende Belegung auftritt, nicht, ob kurzzeitig
  allokiert wird.
- **`makeRepresentativeScreen()` weiterhin OOM-Absturzstelle?** Ja
  (Absturz 2).
- **Configuration-Commit oder HTTP-Route zuerst?** In diesem Lauf scheiterte
  zuerst der Configuration-Commit beim Moduswechsel (Absturz 1), später das
  Rendering (Absturz 2); die HTTP-Route trat nicht auf.

## 6. Ergebnis

Die S3-Re-Messung zeigt gegenüber der S2-failing-baseline **keinen messbaren
Heapgewinn** nach der UI-Initialisierung und im Steady State, und die
bekannten Absturzstellen (`makeRepresentativeScreen`, Configuration-Commit)
treten weiterhin auf. Der Lauf endete mit `FAIL_STOP_CONDITION`. Daraus
folgt nicht, dass S3 wirkungslos oder falsch ist: S3 betrifft die
wiederholte Katalogkopie im UI-Loop, der Boot-Heap der `HOME_WIFI`-Boots wird
vor dem ersten Loop von WLAN und UI-Initialisierung bestimmt, und ein
Nachweis der Wirkung im Steady State fehlt (keine Aufrufzähler). Es wird
keine Ursache als behoben erklärt, kein Budget abgeleitet und keine
Aussage zur Gesamt-RAM-Stabilisierung getroffen.

Gerätezustand nach dem Lauf: persistierter Netzwerkmodus `HOME_WIFI`.
