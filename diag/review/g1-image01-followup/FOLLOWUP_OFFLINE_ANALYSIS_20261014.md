# PR #203 / RES-01 – G1 IMAGE01: Offline-Ursachenanalyse (Follow-up)

`G1_IMAGE01=FAIL/HOLD` · `RES-01=BLOCKER` · `F4_THRESHOLD=PROVISIONAL_PENDING_G1` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`

Dieser Bericht ändert keinen dieser Status. Er ändert auch keine Datei des gepinnten Pakets `g1-image01/` (Stand `a361f51`); Korrekturen stehen unter Abschnitt 2.3 als Errata.

## 0. Umfang und Grenzen

- **Nur Offline-Arbeit.** Keine Geräteabfrage, kein UART-Port, keine Touch-Session, keine Last, kein Flash, Reset oder Rollback, kein neuer Build. Private Rohdaten wurden nur gelesen. Ihre Hashes stehen in `g1-image01/evidence/EVIDENCE_PROVENANCE.md`.
- Zusätzlich gelesen: Treiber und Capture (`diag/c0_load.py`, `diag/g1_capture.py`; Hashes identisch mit dem gepinnten Bundle `SHA256SUMS`, c0_load `8c4db427…767a5`, g1_capture `ce459258…c4de9`), die private C0-R3-Evidence (Vergleich) und der ESP-IDF-6.1-Quelltext.
- **Beweisklassen:** `BELEGT` = direkt aus Artefakt oder Code. `ABGELEITET` = zwingende Folge aus Belegtem unter genannter Annahme. `HYPOTHESE` = konsistent, aber nicht bewiesen. `NOT_PROVEN`.
- Nicht ausgeführt: jede Aussage über den Gerätezustand nach 14:12:12.5 UTC. Nach der Anomalie wurde der UART nicht mehr gelesen.

## 1. Ergebnis in Kürze

| Klasse | Befund |
|---|---|
| **BLOCKER** | `RES-01` bleibt Blocker. Ein einzelnes `heap_alloc_failed` ist nach Plan Abschnitt 7 Kriterium 5 ein Fail und löst die F2-Eskalationsbewertung aus; der Auslöser aus Plan Abschnitt 8 ist erfüllt. |
| **FOLLOW-UP** | Die sechs `http-protocol-error` sind **Abbruchfolgen** der Socket-Schliessung und keine unabhängigen Serverstörungen (`BELEGT`). Das Heapereignis liegt zeitlich **vor** diesen Fehlern (`ABGELEITET`). Der siebte Fehler (`timeout`) ist ein Verbindungsaufbau-Timeout, der vor dem Heapereignis begann. |
| **FOLLOW-UP** | Dem Heapereignis ging eine ca. 43 s lange Latenz-/Lieferstörung voraus (1,5–5,1 s je Request) und ein 2,8 s langes Fenster ohne einen einzigen abgeschlossenen Request. Der Heap war beim Start der zweiten Shell-Antwort mit 14 608 B (built) bereits 11 KB unter dem bisherigen Laufminimum. |
| **FOLLOW-UP** | Das Bedarfsmuster der Sendephase ist in G1 und C0-R3 praktisch identisch (Spitze 16,3 bzw. 17,3 KB). Der Unterschied liegt im Spielraum beim Start der Antwort (G1 −1,7 KB, C0-R3 +0,7 KB; `HYPOTHESE`, Modellrechnung). Das stützt kein «G1-Instrumentierung/F4 hat das verursacht»; es belegt aber auch keine Ursache (n = 1 je Image). |
| **FOLLOW-UP** | Die Produktseite ist als **Risiko** hinreichend belegt (Speicherspielraum ≈ 0 bei einer Lieferstörung). Die Auswahl des wirksamen Hebels ist es nicht, weil der Speicherverbraucher nicht gemessen wurde. Empfehlung: ownerpflichtiger F2-Plan mit eng begrenzter Diagnose (Abschnitt 5). |
| **NO-ACTION** | Keine Änderung der F4-Schwelle (Abschnitt 4). Keine Produktänderung aus dieser Evidence. Flash-Identität, Stackreserven und Idle-Referenz des G1-Berichts bleiben unberührt. |

## 2. Rekonstruktion des Ausfallfensters

### 2.1 Belege und Methode

| ID | Beleg | Klasse |
|---|---|---|
| E1 | Ein Sample erhält `epoch = time.time()` **nach** der Rückkehr von `guard.request` (`c0_load.py:138`); der gespeicherte Zeitpunkt ist also das **Ende** des Requests. Der Beginn ist `epoch − milliseconds`. | BELEGT (Code) |
| E2 | `load.json` (private, SHA-256 `434901644d9b95c0a8dc4457c2fc6994778b1502774a50cc568147a591e561bf`) trägt `abort = "g1-supervisor-stop"`. `Guard.abort` setzt den Grund nur beim **ersten** Aufruf (`c0_load.py:41-44`). Ein Request-Fehler (`abort("request-failure")`, `c0_load.py:77`) hat den Grund daher **nicht** gesetzt; der Supervisor-Stopp kam zuerst. | BELEGT |
| E3 | `FAILURE_WINDOW_UART.bin`: Die Zeile `res01_http: phase=built …` wird **unmittelbar** von `E heap_alloc_failed: size=1626 caps=0x0000080c function=heap_caps_mall` gefolgt; keine weitere Logzeile liegt dazwischen. | BELEGT |
| E4 | Dateizeiten der privaten Capture-Dateien (UTC): `ABORT`, `uart_raw.bin`, `uart.log` haben **identisch** mtime `14:12:12.512864766`. Die letzte gestempelte `built`-Zeile trägt `14:12:12.516011`. Plausibilität der Dateiuhr: `START_LOAD` hat mtime `14:05:45.434258` bei Inhalt `14:05:45.440424` (mtime läuft ≈ 6 ms vor; Auflösung der Dateiuhr = einige ms). Also: Der Chunk mit `built` und Marker wurde bei ≈ **14:12:12.513–.516** gelesen und der `ABORT` im selben Tick geschrieben. | ABGELEITET (Dateiuhr, ± einige ms) |
| E5 | Kette im Code: `BAD.search(offset)` auf den unvollständigen Frame (`g1_capture.py:85`) → `ABORT` schreiben (`g1_capture.py:109`) → der Monitor prüft die Datei alle 50 ms (`c0_load.py:93`, `finished.wait(0.05)`) → `guard.abort` → `close_sockets` → alle laufenden Requests werfen sofort. | BELEGT (Code) |
| E6 | Die sechs Fehler enden bei `14:12:12.552163 … .552894` (**0,731 ms** Spreizung = ein einziger `close_sockets`-Durchlauf). Der Abstand zur Markerlesung beträgt ≈ 36–39 ms und liegt damit im 50-ms-Pollintervall des Monitors. | ABGELEITET |
| E7 | Die Startzeiten der sechs Fehler (Ende − Dauer) sind **14:12:09.570** (Session 0: alerts, shell, temperatures; Dauer 2982–2983 ms) und **14:12:10.770** (Session 1: status, alerts, shell; Dauer 1782–1783 ms). Sie liegen exakt auf dem Treiberraster (Session 1 = Session 0 + 1,2 s). Die Dauer ist also »Abbruchzeitpunkt − Start«, keine Fehlerlaufzeit des Servers. | ABGELEITET |

### 2.2 Ereignisfolge (UTC, 2026-10-14; Zeiten mit `*` = UART-Lesezeit des Hosts)

| Zeit | Ereignis | Klasse |
|---|---|---|
| 14:12:09.363 | letzter Request-Abschluss vor einer Lücke (Session 2, temperatures, 2393 ms) | BELEGT |
| 14:12:09.570 | Session 0 startet 4 Requests (status, temperatures, alerts, shell) | ABGELEITET |
| 14:12:09.800* | Shell A: `pre` (free 35 556 B, grösster Block 32 768 B), `built` ≈ 10 ms später (33 952 / 31 744) | BELEGT |
| 14:12:10.770 | Session 1 startet 4 Requests; **`temperatures` bleibt im Verbindungsaufbau** (siehe unten) | ABGELEITET |
| 14:12:11.970 | Session 2 hätte nach dem Raster einen neuen Batch gestartet. Es gibt dafür **kein** Sample, die Requests waren beim Abbruch nicht verbunden (schwächere Folgerung). | HYPOTHESE |
| 14:12:12.158 | erster Abschluss nach der Lücke: Session 0 `status` (200, 2588 ms). **Lücke ohne jeden Abschluss: 2,795 s** (bei 8 offenen Requests) | BELEGT |
| 14:12:12.415* | Shell A: `sent` (free 17 900 B, grösster Block 12 288 B). Die Sendephase dauerte **2,6 s** | BELEGT |
| 14:12:12.516* | Shell B: `pre` (16 212 B / 10 752 B) und `built` (14 608 B / 10 752 B) | BELEGT |
| ≈ 14:12:12.513–.516 | `heap_alloc_failed size=1626 caps=0x80c` im selben Lese-Chunk, `ABORT` geschrieben | ABGELEITET (E4) |
| 14:12:12.552 | sechs `http-protocol-error` (Abbruchfolge, E2/E5/E6) | BELEGT/ABGELEITET |
| 14:12:13.771 | `timeout` Session 1 `temperatures`, 3,001 s nach Start 14:12:10.770 | BELEGT |
| 14:12:14.448 / .453 | Lastende / Capture-Stopp; UART nach dem Marker **nicht mehr gelesen** | BELEGT |

**Zum Timeout (Sample 783):** Die `Guard`-Logik trägt einen Request erst nach `connect()` in `connections` ein (`c0_load.py:56-58`). Hätte der Request beim Abbruch bereits eine Verbindung gehabt, wäre er wie die anderen sechs sofort beendet worden. Er endete aber erst nach exakt dem 3-s-Sockettimeout. Folgerung (`ABGELEITET`): Der TCP-Verbindungsaufbau blieb ab 14:12:10.770 über 3 s offen, also ab ≥ 1,8 s **vor** der Markerlesung. Ein Verbindungsaufbau, den das Gerät nicht innerhalb von 3 s abschliesst, ist ein echter Befund und kein Abbruchartefakt. Ursache: `NOT_PROVEN` (z. B. volle Annahmewarteschlange, fehlender Speicher für Verbindungszustand, Funkstörung).

### 2.3 Errata zu Berichtsaussagen (Original-Dateien bleiben hashgebunden unverändert)

| Fundstelle (`g1-image01/`) | Bisherige Aussage | Präzisierte Aussage |
|---|---|---|
| `G1_RESULT.md` »Last und Abbruch«; `INDEPENDENT_REVIEW_INDEX.md` Kopf | «sieben fehlgeschlagene Requests (6 Protokollfehler, 1 Timeout)»; «Sieben HTTP-Requests scheiterten im selben Abschlussfenster» | Sechs der sieben Fehler sind **vom Supervisor ausgelöste Socket-Schliessungen** (`abort = g1-supervisor-stop`, 0,731 ms Spreizung). Ein Fehler ist ein Verbindungsaufbau-Timeout, der ≥ 1,8 s vor dem Heapereignis begann. Es gibt **keine** sieben unabhängigen Serverstörungen. |
| `G1_RESULT.md` »UTC-/monotone Zeitzuordnung«; `FAILURE_WINDOW.md` | «Die Reihenfolge gegenüber den HTTP-Fehlern ist NOT_PROVEN.» | Das Heapereignis liegt **vor** den sechs Abbruchfehlern (Kette E2–E6, ± einige ms Dateiuhr). Nicht bewiesen bleiben: der Zeitpunkt auf dem Gerät auf < 50 ms genau (Lese-Chunk) und alles nach dem Marker. |
| `G1_RESULT.md` Statusmatrix »P-A-HTTP-Ergebnis FAIL« | 777 × 200, 7 Fehler | Unverändert FAIL. Die Fehlerzahl der Matrix ist 1 genuiner Timeout plus 6 Abbruchfolgen, zusätzlich die vorausgehende Latenzstörung (Abschnitt 3.2). |

Die Dateiuhr-Angaben (E4) und `abort` (E2) fehlen im veröffentlichten Paket; sie stehen hier als abgeleitete Fakten.

## 3. Allokationsursache eingrenzen

### 3.1 Was der Marker hergibt

- `size=1626`, `caps=0x80c`. Nach `esp_heap_caps.h` (IDF 6.1) ist das `MALLOC_CAP_INTERNAL (0x800) | MALLOC_CAP_DMA (0x008) | MALLOC_CAP_8BIT (0x004)`. Auf dem ESP32 ist der gesamte interne Heap DMA-fähig (`dma_free_bytes == internal_8bit_free_bytes` im `diag_30s`); die Capability schränkt also **nicht** ein.
- `function=heap_caps_mall…` ist abgeschnitten, weil der Host nach der Anomalie den Port nicht mehr las. Passend sind `heap_caps_malloc` und (Präfix) `heap_caps_malloc_prefer`. Wi-Fi-Adapter-Wrapper mit `calloc`/`realloc` (`esp_wifi/esp32/esp_adapter.c:404-418`) hätten `heap_caps_calloc`/`heap_caps_realloc` gemeldet; lwIP nutzt `heap_caps_malloc_prefer` mit `DEFAULT|INTERNAL` (`0x1800`, `lwipopts.h:1744`).
- **Statische Quellsuche in IDF 6.1** nach `heap_caps_malloc` mit genau `8BIT|DMA|INTERNAL`: Treffer für klassischen ESP32 zur Laufzeit ist `esp_coex_common_malloc_internal_wrapper` (`esp_coex/esp32/esp_coex_adapter.c:124-127`, vom geschlossenen Wi-Fi-/Coex-Blob genutzt). Andere Treffer liegen auf anderen Chips (mbedTLS-SHA-DMA ist für den klassischen ESP32 nicht gebaut, `mbedtls/CMakeLists.txt:345-350`), in nicht aktivem BT, oder sind Einmalallokationen bei der Initialisierung (LVGL-Anzeigepuffer mit `buff_dma`, 12 800 B). Im Produktcode (`main/`, `lib/`, `managed_components/`) gibt es keine Laufzeitallokation mit diesen Capabilities.
- **Bewertung:** Dass der Verursacher im Wi-Fi-/Coex-Blob liegt, ist `HYPOTHESE`. Es gibt keinen Backtrace, und geschlossene Bibliotheken wurden nicht durchsucht. Der Aufrufer ist, wie der Auftrag schon sagt, **nicht** dekodierbar.

### 3.2 Speicher- und Lieferverlauf vor dem Ereignis

Der Heap ging **nicht** allmählich aus. Alle Werte sind Phasenstichproben der Shell-Antworten (`res01_http`), keine Momentanspitzen.

| Messpunkt (Last-s) | Beobachtung |
|---|---|
| bis ≈ 250 s | Latenzen wie C0-R3 (p50 159 / 297 / 331 / 263 ms je Minute 0–3 in G1, 159 / 302 / 256 / 265 ms in C0-R3). |
| ≈ 267–310 s | anhaltende Störung (vereinzelt schon ab 182 s): viele GETs mit 1,5–5,1 s Dauer (`max 5149 ms`, sechs GETs > 3 s im Fenster 240–300 s); gleichzeitig `ui_skipped_low_memory` 79 → 396 (Last-s 273 → 293) bei grösstem Block 10–17 KB, dann Erholung auf 38 912 B (303 s) und erneuter Einbruch auf 13 824 B (308–309 s). |
| 309,6 s | Shell A `sent`: free 17 900 B, grösster Block 12 288 B, Sendephase 2,6 s. |
| 309,95 s | Shell B `pre`/`built`: free 16 212 / **14 608 B**. Das kleinste `free(built)` der 41 vollständigen Tripel war zuvor 25 628 B; der Wert liegt ≈ **11 KB unter dem bisherigen Laufminimum**. Die Boot-Minimumuhr stand dabei noch bei 13 048 B. |
| ≤ ≈ 50 ms später | `heap_alloc_failed`. Es ist belegt, dass die Allokation nach dem `built` von Shell B und im selben Lese-Chunk lag; es ist nicht belegt, ob sie innerhalb von Shell B oder in einem anderen Task geschah. |

**Bedarfsmodell (HYPOTHESE, aus den Phasenstichproben):** Der Heapabfall zwischen `built` und `sent` ist die Summe **aller** gleichzeitigen Verbraucher in dieser Zeit. Er ist nicht dem einzelnen Request zuzuordnen.

| Grösse | G1 (41 Tripel) | C0-R3 (160 Tripel) |
|---|---:|---:|
| Abfall `built`→`sent`, Median | 8 244 B | 8 220 B |
| Abfall `built`→`sent`, p90 | 14 696 B | 14 476 B |
| Abfall `built`→`sent`, **Maximum** | **16 348 B** | **17 268 B** |
| kleinstes `free(built)` | **14 608 B** (letztes, unvollständiges Tripel) | 17 984 B |
| Spielraum = kleinstes `free(built)` − Maximalabfall | **−1 740 B** | **+716 B** |
| kleinstes `free(sent)` / `largest` | 17 900 / 10 240 B (nur vollständige) | 10 336 / 3 584 B |

Lesart: Beide Läufe bewegen sich am selben Rand. C0-R3 überlebte dort 1200 s knapp, auch mit einem Boot-Minimum von 5 228 B und einem Block von 3 584 B bei Last-s 789,8. Bei G1 begann die Sendephase von Shell B mit weniger Spielraum, als der Bedarf der Vergleichsläufe erwarten lässt. Das erklärt das Ereignis **konsistent**, beweist es aber nicht: Es gibt keinen Pufferzähler, die Sendedauer von 2,6 s hielt Speicher fest (ungeklärt, ob Wi-Fi-Sendepuffer, TCP-Warteschlangen oder beides), und die Verbraucher sind nicht getrennt.

**Warum die Boot-Minimumuhr (13 048 B) kein Gegenbeweis ist:** Sie wird nur bei erfolgreichen Allokationen und Freigaben fortgeschrieben. Ein Abfall des grössten Blocks von 10 752 B auf unter 1,7 KB innerhalb von ≈ 50 ms bleibt in den Phasenstichproben (`pre/built/sent`) und im 30-s-`diag_30s` unsichtbar.

### 3.3 Vergleich mit C0-R3 und Instrumentierung (Vergleichsgrenzen)

| Grösse | G1 IMAGE01 (F1-4 + F4 + Probe + Instrumentierung) | C0-R3 (F1-4, kein F4) | Folgerung |
|---|---|---|---|
| Profil | P-A, gleiches Treiberbundle (Hashes) | P-A | vergleichbar; G1 verwendet identischen Treiber |
| Idle (60 s, vier Sessions) | 41 132 B frei / 38 912 B Block | 42 532 B / 38 912 B | G1 **−1 400 B**, gleicher Block |
| Statisches DRAM (G1 vs. Produktbuild `d35ee85`) | `.dram0.bss` +1 368 B, `.dram0.data` +84 B (= +1 452 B) | – | deckt den Idle-Unterschied ab; klein gegenüber den ≈ 16 KB der Sendephase |
| Verlauf des Boot-Minimums (Last-s) | 13 972 B bei 221 s, 13 048 B bei 307 s | 13 912 B bei 192 s, 12 184 B bei 492 s, 5 228 B bei 790 s | ähnlicher Verlauf im gemeinsamen Zeitraum |
| Lieferstörung | ≈ 43 s, 1,5–5,1 s je GET (Last-s ≈ 267–310) | ≈ 5 s, 15 GETs 2,2–5,4 s (Last-s ≈ 821–825); kürzer, danach normal | gleichartig, in G1 länger |
| Gleichzeitig offene Requests (Clientsicht) | max 8, ≥ 7 für 5,8 s | max 11, ≥ 7 für 5,2 s | `max_open_sockets` ist hier **nicht** unterscheidend (Default 7 in beiden) |
| Wi-Fi-Ereignisse im Mitschnitt | keine Trennung, kein Roaming, kein Scan; Ereignis `RX DELBA reason 39` nur im Idle (377 932 ms Uptime) | gleiche Art, ohne Zusammenhang mit der Störung | keine Funkereignis-Korrelation sichtbar (Log-Level `info`) |
| Ergebnis | `heap_alloc_failed` bei 311,9 s | 1200 s fehlerfrei | n = 1 je Image: **keine** Fehlerrate und **keine** Zurechnung |

**Instrumentierung/F4 als Ursache:** Die Daten sprechen nicht dafür (Bedarfsmuster und Minimumverlauf gleich, F4 überspringt im Gegenteil UI-Allokationen). Ausgeschlossen ist es mit n = 1 je Image nicht (`NOT_PROVEN`). C0-R3 bleibt die Vergleichsgrenze: gleicher Treiber, ohne F4/Probe.

### 3.4 Hypothesenmatrix

| # | Hypothese | Stand |
|---|---|---|
| H1 | Die sechs Protokollfehler sind Abbruchartefakte. | **BELEGT** |
| H2 | Vor dem Marker lag eine Lieferstörung (Antworten, Verbindungsaufbau), die in G1 und C0-R3 vorkommt. | Gestützt (Client-Latenzen, Lücke, Sendephase 2,6 s). Ursache der Störung `NOT_PROVEN`. |
| H3 | Während der Störung bleibt Sende-/Verbindungszustand im Heap gebunden; die Sendephase der nächsten Shell-Antwort erschöpft den Rest. | Konsistent mit Messung (Abschnitt 3.2). Kein Zähler: `HYPOTHESE`. |
| H4 | Die scheiternde Allokation stammt aus dem Wi-Fi-/Coex-Blob. | Quellabgleich passt; geschlossene Bibliotheken nicht prüfbar: `HYPOTHESE`. |
| H5 | G1-Instrumentierung bzw. F4 verursachte den Fehler. | Nicht gestützt; n = 1: `NOT_PROVEN`. |
| H6 | Die Verbindungszahl über `max_open_sockets` (7) erklärt den Unterschied. | Nicht unterscheidend (C0-R3 erreichte 11). Bleibt Risikofaktor. |
| H7 | Wi-Fi-Powersave (`pm start, type: 1`) verursacht Sekundenstörungen. | Nicht gestützt (Verzögerung ≈ DTIM/Beacon-Intervall ≈ 0,1 s); `NOT_PROVEN`. |

## 4. F4 korrekt abgrenzen

- Die 397 Skips / 7 Resumes und die Zeile `ui_skipped_low_memory count=397` belegen, dass das Gate **aktiv war**, und sie zeigen **dass** der Speicher im Zeitfenster 273–293 s knapp war. Sie belegen weder die Hold-Sicherheit noch eine Ursache der Allokation im Wi-Fi-Pfad.
- Der Rechenkandidat `2 × max(6 656, 8 688) = 17 376 B` ist nach wie vor weder qualifiziert noch angewendet. 8 688 B ist ein Stichprobenbedarf aus Home/Locale/Phase 0·2. Eine Schwellenänderung (hoch oder runter) ist ohne Render-/Touch-/Pfadcoverage und ohne Ursachenklärung des Heapereignisses nicht begründbar. **NO-ACTION.**
- Hinweis: Ein höherer Wert könnte die UI-Allokation früher abschalten. Er würde jedoch **nicht** den Bedarf der Sendephase (≈ 16 KB) adressieren. Das ist keine Empfehlung, nur eine Abgrenzung.

## 5. Kleinste nächste Massnahme (ownerpflichtiger Planvorschlag, nicht umgesetzt)

**Bewertung zuerst:** Ein Produktproblem ist als Risiko hinreichend belegt (Speicherspielraum der Sendephase ≈ 0 bei Lieferstörungen, in zwei unabhängigen Läufen). Die Wahl des Hebels ist es nicht, weil Verbraucher und Aufrufer unbekannt sind. Deshalb: **zuerst ein klar begrenzter F2-Plan mit eingebauter Diagnose, noch keine Produktänderung und kein weiterer Lauf.**

**Ownerentscheid 1 (keine Hardware):** F2-Plan nach Plan Abschnitt 8 eröffnen. Auslöser erfüllt: `heap_alloc_failed` trotz F1 + F4.

**Hebel nach Espressif-first** (jeweils Konfiguration oder vorhandene ESP-IDF-Funktion; Werte sind **nicht** gewählt, das ist Aufgabe des F2-Plans):

| Hebel | Ist (G1-Image, `artifacts/sdkconfig`) | Zweck / Preis |
|---|---|---|
| `CONFIG_LWIP_TCP_SND_BUF_DEFAULT` | 5760 | weniger gebundene Sendedaten je Verbindung (Plan nennt 5760 → 2880, ≈ 3 KB je Antwort); kostet Durchsatz/Shell-Dauer |
| HTTP-Server `max_open_sockets` / `lru_purge_enable` | `HTTPD_DEFAULT_CONFIG()` unverändert (7, ohne LRU; `esp_idf_http_server_lifecycle.cpp:192-199`) | begrenzt gleichzeitige Antworten; Preis: Wartezeiten/Verbindungsabbrüche für parallele Clients; entspricht »Begrenzung der Verbindungsparallelität« im Plan |
| `CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM` / `…_DYNAMIC_TX_BUFFER_NUM` | 32 / 32 (IDF-Default, RX theoretisch bis ≈ 50 KB bei ≈ 1,6 KB/Puffer) | begrenzt die Wi-Fi-Spitze; **ausserhalb des F2-Textes**, daher eigener Ownerentscheid; Preis: Paketverlust/Durchsatz |

Nicht empfohlen: eine Eigenentwicklung (Drosselung, Pool, Scheduler) vor einem dieser Hebel, F4-Schwellenänderung, Wi-Fi-Powersave-Änderung.

**Diagnose im F2-Plan (eng, nur Diagnosebranch, aktorfreies Dev-Board, eigene Hardware-Freigabe):**

1. **Aufrufer ermitteln:** Backtrace im bestehenden Failed-Alloc-Hook (`app_main.cpp:612`) oder `CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS` (Panic mit Backtrace). Der Hook muss IRAM/DRAM-sicher bleiben.
2. **Verbraucher trennen:** vorhandene lwIP-Statistik (`CONFIG_LWIP_STATS`: Speicher-/Pool-/TCP-Zähler) an den Messpunkten `pre/built/sent`, gleichzeitig `heap_caps_get_info`. Alternativ `CONFIG_HEAP_TASK_TRACKING` (Speicher je Task). RAM-Kosten der Instrumentierung vorab bewerten.
3. **Capture härten:** nach der Anomalie den UART noch ≤ einige Sekunden weiterlesen (Folgezustand: Panic, Reset, Erholung); jeden Rohchunk mit Hostzeit stempeln.
4. **Lasttreiber:** je Sample Start **und** Ende speichern; `guard.reason` samt Zeitpunkt in `load.json`; Abbruchfolgen getrennt von echten Fehlern klassifizieren; Verbindungsaufbau- und Lesetimeout unterscheiden.

**Notwendige Evidence für die Entscheidung:** Aufrufer des fehlgeschlagenen Aufrufs; Verbraucheraufteilung in der Sendephase; Wiederholbarkeit (G1 n = 1, C0-R3 n = 1; Plan fordert 3 × P-A); ein Vorher/Nachher-Vergleich der gewählten Hebel unter gleicher Last. **Ohne diese Evidence keine Schwellen- und keine Konfigurationsänderung.**

## 6. Offene Lücken

- Zustand des Geräts nach dem Marker: `NOT_PROVEN` (kein UART-Tail, keine weitere Abfrage).
- Ursache der Lieferstörung (Funk, Access Point, lwIP, Wi-Fi-Puffer): `NOT_PROVEN`. RSSI/Kanalauslastung sind nur beim Verbinden im Log (−44 dBm G1, −47 dBm C0-R3).
- Aufrufer des fehlgeschlagenen Aufrufs: `NOT_PROVEN`.
- Verbleibende Dateiuhr-Unschärfe (E4): einige ms. Die Reihenfolge Marker → Fehler hängt zusätzlich an der Codekette E2/E5 und ist damit nicht allein an Zeitstempeln festgemacht.
- G1-01, Touch-/Hold-Coverage und Recovery: unverändert `NOT_PROVEN`.

## 7. Reproduktion (nur lesend)

Alle Zahlen stammen aus lesenden Auswertungen der privaten Dateien `candidate_capture/{uart.log, uart_raw.bin, load/load.json, host_sockets.jsonl, ABORT}` (G1) und `g0_r3_20261014_run01/{candidate_load/load.json, uart_candidate.log}` (C0-R3) sowie `stat` für die Dateizeiten. Die Auswertung gruppiert Samples nach Ende − Dauer, zählt offene Requests je Zeitpunkt und bildet die Tripel `pre/built/sent` aus den `res01_http`-Zeilen. Die Quellvergleiche nutzen `grep` in `opt/espressif/esp-idf-v6.1/components/`. Es wurde keine Datei der privaten Evidence verändert.
