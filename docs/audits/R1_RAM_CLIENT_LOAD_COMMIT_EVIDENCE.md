# R1-RAM – Rückwechsel `AP_ONLY` → `HOME_WIFI` mit und ohne Client

Messreihe zur Untersuchung des Configuration-Commit-Absturzes. Nur Messung und
Dokumentation; keine Codeänderung, kein S5, kein Tuning, keine Budgets.

```text
BUILD=CLEAN_c089486_NO_DIRTY_UNCHANGED_FROM_CLEAN_NETWORK_SWITCH_TEST
HOME_WIFI_TO_AP_ONLY=PASS_X3_NO_CLIENT
AP_ONLY_TO_HOME_WIFI_NO_CLIENT=PASS_X2
AP_ONLY_TO_HOME_WIFI_CLIENT_CONNECTED_NO_PAGE=FAIL_ABORT_192B
AP_ONLY_TO_HOME_WIFI_CLIENT_CONNECTED_PAGE_OPEN=FAIL_ABORT_192B
CONFIGURATION_COMMIT_OOM_WITH_CLIENT=YES_X3_IN_CLEAN_BUILD
HEAP_ALLOC_FAILED_COUNT_THIS_LOG=3
ABORT_COUNT_THIS_LOG=2
RESET_COUNT_THIS_LOG=2_PLUS_POWERON
MAKE_REPRESENTATIVE_SCREEN_OOM=NO
S5_TO_S11=NOT_STARTED
```

## 1. Aufbau

- Firmware unverändert die des sauberen Netzwerk-Switch-Tests: Quell-SHA
  `c08948681fb04ef6b5b749c8d7a49e1c9f33e948`, `App version: c089486` (ohne
  `-dirty`), BIN `00a51d2f102392cade0303a33fd5a1e4b5191ad9ca296c3555f68d0fad6c1006`,
  ELF `eef53f9f7bbf72755261a5ed083c8cc9a11e0907bdc01bf847e314d3d4cd9b21`
  ([R1_RAM_CLEAN_NETWORK_SWITCH_EVIDENCE.md](R1_RAM_CLEAN_NETWORK_SWITCH_EVIDENCE.md)).
  Es wurde nichts geflasht, nichts gelöscht, nichts neu provisioniert.
- Der Mitschnitt öffnete den seriellen Port ohne beabsichtigten Reset; das
  Gerät hat dabei dennoch neu gebootet (`POWERON_RESET`), und der Log beginnt
  deshalb mit einem frischen Boot im persistierten Modus `HOME_WIFI`.
- Rohlog [R1_RAM_CLIENT_LOAD_COMMIT_20261003_RAW.txt](R1_RAM_CLIENT_LOAD_COMMIT_20261003_RAW.txt)
  (Heartbeats entfernt; Heimnetz-SSID, BSSID, MAC-Adressen und LAN-Adressen
  maskiert) und [Punkte-CSV](R1_RAM_CLIENT_LOAD_COMMIT_20261003_POINTS.csv).
  Der Mitschnitt lief durchgehend über drei Boots (Uptime der letzten Phase bis
  ~330 s) und wurde erst auf Owner-Wunsch beendet.

## 2. Ablauf (Owner-Schritte, im Log zugeordnet)

| Boot | Zeit (ms) | Schritt | Client | Ergebnis |
|---|---|---|---|---|
| 1 | 38 | `HOME_WIFI` → `AP_ONLY` | nein | ok |
| 1 | 108 | `AP_ONLY` → `HOME_WIFI` | nein | **ok** (Minimum danach 2956 B) |
| 1 | 184 | `HOME_WIFI` → `AP_ONLY` | nein | ok |
| 1 | 236 | Handy verbindet sich mit dem AP, danach Seite `192.168.4.1` geöffnet (Owner) | ja | ohne Fehler |
| 1 | 330 | `AP_ONLY` → `HOME_WIFI` | ja, Seite offen | **Abort**: `heap_alloc_failed` 192 B |
| 2 | 163 | `HOME_WIFI` → `AP_ONLY` | nein | ok |
| 2 | 193 | Handy verbindet sich mit dem AP, **Seite nicht aufgerufen** | ja | ohne Fehler |
| 2 | 225 | `AP_ONLY` → `HOME_WIFI` | ja, ohne Seite | **Abort**: `heap_alloc_failed` 192 B |
| 3 | 162 | `HOME_WIFI` → `AP_ONLY` | nein (Handy getrennt) | ok |
| 3 | 239 | `AP_ONLY` → `HOME_WIFI` | nein | **ok** (Minimum danach 2872 B) |
| 3 | ~281 | Idle in `HOME_WIFI`/`HomeConnected`, keine Bedienung | nein | `heap_alloc_failed` 1696 B, nicht fatal |

Der Whitescreen beider Abstürze wurde vom Owner bestätigt („Zurück auf heim >
whitescreen“, „Ja ich habe heim gedrückt“). Der Fehlschlag mit 1696 B bei
~281 s trat nach der Zwischenprüfung nach dem Rückwechsel auf (meine Meldung
„kein neuer `heap_alloc_failed`“ bezog sich auf den Stand bei 242 s).

## 3. Gegenüberstellung des Rückwechsels `AP_ONLY` → `HOME_WIFI`

Heap jeweils beim letzten Messpunkt vor dem Press (`network_page_press_before`):

| Fall | frei | Minimum | größter 8-Bit-Block | Stack-HWM | Ergebnis |
|---|---|---|---|---|---|
| ohne Client (Boot 1) | 10888 B | 6740 B | 7936 B | 6256 B | ok, danach Min 2956 B, Block 4608 B |
| ohne Client (Boot 3) | 10752 B | 6140 B | 8192 B | 5936 B | ok, danach Min 2872 B, Block 4608 B |
| Client, ohne Seitenaufruf (Boot 2) | 7756 B | 2576 B | 6400 B | 6256 B | **Abort** 192 B |
| Client, Seite offen (Boot 1) | 7300 B | 2956 B | 4864 B | 6048 B | **Abort** 192 B |

Bis zum Press liegen die Fälle mit Client beim freien Heap 3,0–3,5 kB und beim
Minimum 3,2–4,2 kB unter denen ohne Client (`stable_ap_only` ohne Client:
frei 10340–10736 B, Minimum 6140–6740 B). Auch ohne Client fällt das Minimum im
Commit auf 2,9–3,0 kB und der größte Block auf 4608 B; ein Fehlschlag trat
dort in beiden Läufen nicht auf. Der Abstand zum Absturz ist ohne Client
klein, und er wurde nicht ausgelotet.

Mit dem früheren sauberen Lauf (Client und Seite, Abort 192 B) sind es
**drei** Abstürze mit Client im Clean-Build, dazu S3 (Client im AP-Betrieb
nicht dokumentiert).

## 4. Absturzstelle (Backtrace, drei Abstürze)

Immer derselbe Pfad: `updateProductUi` → `processWorkspaceTouch` →
`dispatchWorkspacePress` → `FermentationUiCommandBridge::applyNetworkMode` →
`FermentationApplication::applyNetworkMode` (`fermentation_application.cpp:728`)
→ `ConfigurationService::confirmPreview` (`configuration_service.cpp:1166`) →
`ConfigurationGraphStore::executePreparedCommit` → `validationScan` →
`scanGroupMetadata<4>` → `std::vector<MetadataScanResult::RecordDescriptor>` →
`operator new` → `__cxa_allocate_exception` → `abort`. Die fehlschlagende
Allokation ist in allen drei Abstürzen 192 B (caps `0x1800`).

Die Rücksprungadresse in `validationScan` unterscheidet sich: `0x4010ff34`
(`configuration_graph_store.cpp:1195`, Scan der Servicekonfigurations-Slots)
im Absturz bei 330 s, `0x4010ff5d` (`configuration_graph_store.cpp:1200`, Scan
der Programmkatalog-Slots) im Absturz bei 225 s und im früheren sauberen Lauf.
Es scheitert also einer der aufeinanderfolgenden Metadaten-Scans, je nachdem,
welcher gerade die 192 B anfordert.

Symbolisierungshinweis: Der geflashte ELF (`eef53f9f…`) wurde nach dem
früheren Lauf mit dem Worktree gelöscht. Der Clean-Build wurde aus demselben
Commit am selben Pfad neu gebaut; er ist nicht bitgleich (ELF-SHA-256
`8d6da4345fa809b33c75aacaac4808e1733958bd342c7df7e76e5fa2e34f049c`,
Zeitstempel im Build), liefert aber für alle zuvor mit dem geflashten ELF
dekodierten Adressen dieselben Funktionen und diente deshalb zur
Dekodierung der Adresse `0x4010ff34`.

## 5. Kennzahlen dieses Mitschnitts

| Kennzahl | Wert |
|---|---|
| Boots | 3 (`POWERON_RESET` plus 2× `SW_CPU_RESET`) |
| `heap_alloc_failed` | 3 (192 B, 192 B, 1696 B; je caps `0x1800`) |
| Abort | 2 (Configuration-Commit) |
| kleinstes globales Heap-Minimum | 2576 B (vor dem Absturz bei 225 s) |
| kleinster 8-Bit-Block | 4608 B |
| niedrigster Main-Task-Stack-HWM | 5936 B (`AP_ONLY`, Boot 3) |
| LVGL-Pool maximal belegt | 15860 B (24 %) |
| `makeRepresentativeScreen()`-OOM | nein |
| Configuration-Commit-OOM | ja (nur mit verbundenem Client) |

INTERNAL und DMA stimmen mit den allgemeinen Heap-Werten überein (kein
PSRAM).

## 6. Idle-Fehlschlag mit 1696 B

Derselbe Fehlschlag (1696 B, caps `0x1800`, nicht fatal, kein Abort, kein
Reset, kein Screen-Aufbau) trat nun ein zweites Mal auf: im vorigen Lauf bei
~3450 s im Idle in `HOME_WIFI`/`HomeConnected`, in diesem Lauf ~40 s nach dem
erfolgreichen Rückwechsel (`HomeConnected`, ohne Bedienung). Der Aufrufer ist
aus dem Log nicht bestimmbar (die Diagnose meldet nur Größe, Caps und
`heap_caps_malloc`).

## 7. Aussagen und Grenzen

- Belegt (gleicher Build, gleicher Mitschnitt): Der Rückwechsel
  `AP_ONLY` → `HOME_WIFI` lief ohne Client zweimal durch und scheiterte mit
  verbundenem Client zweimal in diesem Mitschnitt (einmal ohne, einmal mit
  Seitenaufruf), jedes Mal im Configuration-Commit an einer 192-B-Allokation.
- Schon die bloße Client-Verbindung (Assoziation, DHCP, Block-Ack-Sitzungen)
  senkt den freien Heap und das Minimum deutlich, bevor irgendein Seitenaufruf
  geschieht.
- Nicht belegt: Wodurch der Client den Heap bindet, warum der Commit gerade
  192 B nicht mehr erhält, und ob ein Rückwechsel mit Client und mehr freiem
  Heap gelingen würde. Der Fall ohne Client ist zweimal gemessen, nicht
  ausgelotet (kleiner Abstand zum Absturz).
- Der Idle-Fehlschlag (1696 B) ist weiter ungeklärt und tritt unabhängig vom
  Commit auf.
- Es gibt keine Aussage zur Wirkung von S3/S4 auf den Commit-Peak: Der
  Absturz bleibt an derselben Stelle und Größe wie in S3.
