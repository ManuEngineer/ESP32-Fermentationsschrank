# PR #209 – C5-Teilabnahme (Ownerentscheid) und D9-Vorbereitung

Basis: PR-HEAD `0dc3aef3129130b01dbb9aba93dd63b103fd4579` (Auftrag `PR209_C5_D9_Abschlussauftrag.md`). **Stand: D9 ist
vorbereitet, aber `NOT_RUN`. Es wurde nichts geflasht, gebootet, zurückgesetzt oder geschrieben**; die Ausführung wartet auf die
gesonderte ausdrückliche Ownerfreigabe `G-F`, `G-S`, `G-U`, `G-R`, `G-B` für das unten genannte Image.

## 1. Ownerentscheid C5 – begrenzte Teilabnahme (so übernommen, nichts umgedeutet)
Status-Bezeichner: **`C5_RESOURCE_GATE=ACCEPTED_PARTIAL_FOR_PR209`** – eine eng begrenzte Abnahme der *vorhandenen* Ressourcen-Evidence
für den Software-Integrationsscope von PR #209. Das ist **kein** `C5_RESOURCE_GATE=PASS`, kein `RAM_PASS`, keine R1- und keine
Aktorfreigabe. `PR209_MERGE=HOLD` und `ACTUATOR_RELEASE=NO` bleiben.

| Für den PR-Software-Scope akzeptiert (Ownerentscheid) | Beleg |
|---|---|
| erreichte, funktionierende Webpfade; HTTPD-Stack **8 704 B** | [8 704-B-Evidence](PR209_Q4_HTTPD_8704_HW_20261024.md) |
| gemessener HTTPD-Reststack **2 236 B** auf den erreichten Pfaden | dieselbe Evidence §2 |
| kein beobachteter Panic/Reset/`heap_alloc_failed` im bisherigen Hardware-/Browserlauf | H-RAM-2- und 8 704-B-Evidence |
| B-1/B-2 nach gezielter Fix Verification geschlossen (**Angabe des Owners**; die Fix-Verification-Dokumente selbst liegen dem Builder nicht vor) | [B-1/B-2-Evidence](PR209_Q4_BROWSER_FOLLOWUPS_20261024.md) |

| **Nicht** als nachgewiesen deklariert (unverändert) | Status |
|---|---|
| `HTTPD_FULL_CHAIN_STACK` | `NOT_PROVEN` |
| vollständige `HTTPD_STACK_RESERVE` | `NOT_PROVEN` |
| `ESP32_HEAP_PEAK`, `ESP32_LARGEST_FREE_BLOCK` | `PARTIAL` |
| erfolgreicher Run-/Run-Store-/Checkpoint-/Maximalkatalog-Pfad | `NOT_RUN` |
| `TARGET_OOM_BEHAVIOR` (vorbestehender Abort/Neustart; laut Plan §15.2 kein eigenständiger C5-Blocker) | `FAIL`, unverändert |
| D9 | `PARTIAL` aus §15.10 (Desktop, Login/Navigation/Programme/Einstellungen/DE-EN-ES, Heap/HWM); Rest `NOT_RUN` bis zum Lauf unten |

**Risikoindikationen bleiben dokumentiert, in unterschiedlichen Messkontexten:** Browser-Heap-Tiefststand `internal_min` 17 104 B
und größter Block 18 432 B (Diagnoseimage, mehrere Sessions und Asset-Last, ≈ 30 min) sowie der Host-Einzelrequest von 19 890 B für
den Settings-Pfad (Host-Test, andere Identität). Beide sind nicht ineinander umrechenbar und werden nicht zu einem Wert verschmolzen.

**Qualifikationsgrenze – wohin die offenen Nachweise gehören (kanonische Folgescopes laut Roadmap/Plan):**
| Offener Nachweis | Folge-Scope |
|---|---|
| Run-/Run-Store-/Checkpoint-Pfad auf Hardware, Stackpfad des erfolgreichen Run-Starts (`G-RUN`) | #34/#35 (Sensorvergleich, PI-/Luft-/Aktor-/Sicherheitsparameter, `TBD_COMMISSIONING`) nach #30/#31/#32/#33 |
| DS18B20-Sampling-Task-Budget, Sensor-Ressourcenprüfung | #30 / #190 (`BLOCKED_HARDWARE`, H1–H7 `NOT_RUN`) |
| Aktorik (Lüfter/Summer/MOSFET, BTS7960/Peltier) | #32 / #33 (`BLOCKED_HARDWARE`) |
| Maximalkatalog (16 Programme) auf Hardware, Slice-7-Harness (`G-R7`) | #90 (Harness), spätere Run-Qualifikation |
| Hardwareabnahme, 7-Tage-Belastungstest, Gesamt-RAM-Wiederholung nach #35 | #36 / #37 (Plan §10: Ressourcenmessung nach wesentlichen Integrationen wiederholen) |

Es wird kein erfolgreicher Run simuliert und keine Aktorfreigabe abgeleitet.

## 2. D9 – Vorbereitung (Plan §9)
### 2.1 Exaktes Testimage (Messimage, nicht das Produktimage)
| Größe | Wert |
|---|---|
| Produkt-HEAD | `0dc3aef3129130b01dbb9aba93dd63b103fd4579` (enthält B-1/B-2, 8 704-B-Stack) |
| Messoverlay (lokal, non-merge, nicht gepusht) | Worktree `local/pr209-d9-diag`, Commit `98eb608b780e37d6648950a94693b557fadc200c` (= Produkt-HEAD + Overlay `hram_http`/`hram_30s`/`hram_httpd_task`, nur Logging) |
| **Messimage** `esp32_fermentationsschrank.bin` (`esp32_release`, `--require-clean-source-tree` PASS) | SHA-256 `7e693ebfad1a5e8b4ab2c3c7f107dae36327ecb80d92225bb45033141ad47a06`, 1 775 808 B |
| **Produktimage** (uninstrumentiert, **nicht geflasht/nicht gemessen**) | SHA-256 `796bf14165d6b90987a5d8d67c1515ec6cc186b28981989d768b3649c2ce0824`, 1 774 384 B (gleicher Quelltext ohne Overlay) |
| ESP-IDF | `fff9895c82d744c7237be8847347bdd1b07c6643` (v6.1, unverändert zu P0/H-RAM-2) |
| PHY / effektive `sdkconfig` | `sdkconfig.h` des Messimages **byteidentisch** zum Produktimage und zum früheren 8 704-B-Diagnoseimage; gleiche IDF-PHY-Bibliothek ⇒ `SYSTEM_NVS_WRITE_AT_P1=EXCLUDED` bleibt belegt (bei Abweichung: STOP) |
| Bootloader/Partitionstabelle | Partitionstabelle `d7f180e4…18d7ccd9d` unverändert; es wird nur `0x10000` (App) geschrieben |

Das Messimage und das Produktimage sind **nicht gleichzusetzen**; D9-Aussagen gelten für das Messimage. Optional (Ownerentscheid): ein
kurzer Rauchtest des uninstrumentierten Produktimages (nur Browserpfade, ohne Ressourcenmessung) – nicht Teil des Standardlaufs.

### 2.2 Lauf (ein kompakter Lauf, ≈ 10–12 min, ein Boot)
Skript [PR209_Q4_D9_BROWSER_RUN.mjs.txt](PR209_Q4_D9_BROWSER_RUN.mjs.txt) (Playwright/Chromium headless; jede Prüfung als `REAL`,
`SIMULATED` oder `EMULATION` gekennzeichnet; nie ein Run-/Programm-Intent, nie ein Namens-Speichern, immer Logout):
1. Boot (EN-Reset, UART-Capture), 75 s Idle-Bezug ohne Anfrage.
2. **Desktop** (1280×900, leerer Cache): Shell + versioniertes Asset (2 Requests, `Cache-Control`), **B-1** falsches Passwort je DE/EN/ES (max. 3 Fehlversuche; Sperre erst bei 5, Erfolg setzt zurück), Login, Navigation Übersicht/Meldungen/Programme/Einstellungen je DE/EN/ES, **B-2** leerer Name je DE/EN/ES (kein POST, keine Bestätigung), zweiter Shell-Abruf mit **Asset-Cache-Treffer** (Chromium-Cache, zusätzlich Gegenprobe über die Asset-Zähler der UART-Logs).
3. **Mobil, emuliert** (iPhone-13-Profil: Viewport, Touch, mobile UA – **EMULATION, kein physisches Gerät**): Shell + Asset, B-1, Login, Navigation, B-2.
4. **Alte Asset-Version:** reale `404`-Antwort des Geräts auf `/assets/app.00000000.js` (`REAL`); sichtbarer Hinweis und Recovery durch Reload über eine umgeschriebene Shell (**SIMULATED** – nur der Shell-Hash wird im Browser umgeschrieben).
5. **Asset blockiert/`503`:** Browser-Interception (`abort` / `fulfill 503`) – **SIMULATED**; erwartet sichtbarer Ausfallhinweis, keine Mutation.
6. Erholung/Nachlauf bis ≈ 90 s nach der letzten Anfrage (Heap-Erholung).

### 2.3 Ressourcenerfassung während des Laufs (UART, Overlay)
Idle-Bezug, `free`/Minimum/größter Block (`hram_http`/`hram_30s`), Main- und HTTPD-Stack-HWM, `ui_skipped_low_memory`,
`heap_alloc_failed`, Reset/Panic/Watchdog, HTTP-Statuscodes und Erholung. Vergleich mit H-RAM-2/8 704-B-Evidence **unter Angabe der
Unterschiede** (anderes Image, andere Testbedingungen); **keine neuen Schwellwerte**. Kein 12-KiB-Stacktest, kein langer Heap-Stresslauf,
solange kein neuer konkreter Defekt auftritt.

### 2.4 Recovery- und Sicherheitsrahmen (vor jeder Schreiboperation)
Zwei unabhängige, byteidentische, SHA-256-verifizierte Backups von Factory-App und `state_store` liegen vor (`A`/`B`, P0:
Boot `2244ca13…`, App `3b907b25…`, `state_store` `fbdd8705…`; zuletzt am Ende der 8 704-B-Messung hash-gleich zu P0 bestätigt).
Unmittelbar vor dem Flash wird eine **dritte unabhängige Lesung** (`C`) gegen P0 geprüft; Abweichung ⇒ STOP. Recovery ist im
ROM-Bootloader per `esptool` möglich (auch bei Bootloop): `write-flash 0x300000 state_store_A.bin` (RB-2) und
`write-flash 0x10000 factory_A.bin`, jeweils mit Readback-Hash. Nach dem Lauf: Original-App und `state_store` byte-exakt wiederherstellen,
Bootbereich lesend gegen P0 prüfen, Hashkette dokumentieren, Passwortdatei vernichten. STOP bei unerwarteter Persistenzänderung (anderes als
Login-/Fehlversuchsschreibzugriffe auf `auth0`), OOM/Panic oder unsicherem Restore. Web-Sessions werden immer abgemeldet (Sessions blockieren
sonst bis zu 30 min weitere Logins).

**Erwartete Schreibzugriffe im `state_store`:** nur Auth-Record (jeder Login und jeder Fehlversuch) – kein Namens-Speichern, kein Run-Intent.

### 2.4a Preflight-Korrekturen am Runner (Fix-Delta auf `ae54c1a`, kein Produktcode)
| ID | Befund | Korrektur | Offline-Nachweis |
|---|---|---|---|
| B-D9-1 | `end` wertete nur Browserfehler; fehlgeschlagene `check()`-Prüfungen konnten zu `end=PASS`/Exit 0 führen | `ev()` zählt jedes `FAIL` (Einzelprüfungen und Flow-Fehler); `finish()` meldet `PASS` nur bei 0 Fehlern **und** 0 unerwarteten Browserfehlern, sonst `end=FAIL` und `process.exitCode=1`; `REAL`/`SIMULATED`/`EMULATION`-Kennzeichnung und Einzelereignisse unverändert | absichtlich fehlschlagende Prüfung → Einzelprüfung `FAIL`, `end=FAIL`, Exit ≠ 0; werfende Prüfung und Browserfehler ebenso; sauberer Lauf → `end=PASS`, Exit 0 (je eigener Prozess) |
| B-D9-2 | B-1 wartete auf „irgendeinen nichtleeren `#note`“; nach Sprachwechsel steht dort noch die **vorherige** Meldung, die neue Antwort kann ausstehen | `b1Attempt()`: wartet auf die zu **diesem** Versuch gehörende Login-Antwort (`401`), danach auf die **erwartete Meldung der aktuellen Sprache**; kein pauschaler Sleep, kein zusätzlicher Loginversuch; Sichtbarkeit und Text danach wie bisher | lokaler Mock mit 1,5 s verzögerter Antwort: alte DE-Meldung sichtbar, Wechsel auf EN, erst das neue EN-Ergebnis zählt (Wartezeit ≈ 1,9 s) |

Der Runner ist jetzt importierbar (er führt `main()` nur bei direktem Aufruf aus); Skripte:
[Runner](PR209_Q4_D9_BROWSER_RUN.mjs.txt), [Preflight-Selbsttest](PR209_Q4_D9_PREFLIGHT_SELFTEST.mjs.txt) (nur lokaler Mock, kein Gerät,
keine echten Zugangsdaten), [Ausgabe](PR209_Q4_D9_PREFLIGHT_SELFTEST_OUTPUT.txt) (`PREFLIGHT_SELFTEST=PASS`). Image-Identität, C5-Entscheid und
Laufumfang unverändert; **D9 bleibt `NOT_RUN`** (kein Flash, kein Gerät, kein Browserlauf gegen das Gerät).

### 2.5 Offene Ownerfreigabe (nicht erteilt)
`G-F` (App-only-Flash `0x10000` des Messimages und anschließender Rück-Flash), `G-S` (byte-exakter `state_store`-Restore), `G-U` (UART),
`G-R` (EN/BOOT-Reset), `G-B` (HTTP/Browser). Ebenfalls benötigt: das Web-Passwort (die frühere private Datei wurde vernichtet).
**Nicht freigegeben und nicht beantragt:** `G-R7`, `G-RUN`, aktorwirksame Aktionen, destruktive Harness-/Partitionswege, Safety-Bypässe, Ready, Merge.
