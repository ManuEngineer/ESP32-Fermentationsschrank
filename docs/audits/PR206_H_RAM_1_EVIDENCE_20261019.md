# PR #206 / Issue #27 – H-RAM-1 Hardware-Evidence (Mess-Overlay M), 2026-10-19

**Ergebnis: begrenzter Teilnachweis (Standby, Ein-Client). `RAM_PASS` wird nicht
erteilt; Empfehlung `HOLD` fuer den Lauf-Pfad.** Plan
[`3376a19`](../tasks/issue-27-r1-run-dashboard-info-plan.md), Gate-Vorbereitung
[PR206_H_RAM_1_GATE_20261019.md](PR206_H_RAM_1_GATE_20261019.md). Ownerfreigabe
2026-10-19: G-U UART, G-R Reset/BOOT, G-B Web/HTTP (Ziel `192.168.1.68`)
freigegeben, G-F bedingt (siehe 2). `ACTUATOR_RELEASE=NO`. Keine Touchaktion, kein
Programmstart, keine PIN-/Serviceaktion, keine Commissioning-Umgehung.
Rohlogs und Binaerdaten liegen privat unter `hw-evidence-pr206-hram1/run1/`
(Hashes: [SHA256SUMS_run1](PR206_H_RAM_1/SHA256SUMS_run1)); im Repository nur
bereinigte Zeitlogs und Skripte. Das Webpasswort wurde nur aus einer privaten
`600`-Kopie der ersten Zeile von `/tmp/pw` (Quelle `664`, PIN-Zeile nicht kopiert)
gelesen, nie ausgegeben oder protokolliert; die Kopie ist geloescht (`shred`), eine
Suche nach dem Wert in allen Laufdateien blieb ohne Treffer.

## 1. Identifikation und Rueckfall (MEASURED)

Mit einem dokumentierten EN-Puls (`uart_capture.py --reset`, ein Mitschnitt) lief
vor dem Test: `esp32_release`, Source `a9a1156a1d44d4679957526bbc34a30be6422e4e`,
`real actuators: disabled`, `application: ready`, HOME_WIFI verbunden; esptool:
ESP32-D0WD-V3 rev 3.1, MAC `20:50:0d:1b:2f:34` (gleiches Geraet wie G0-R2/R3).

**Sicherung (Korrektur G-F):** gesamte Factory-App-Partition ab `0x10000`, Laenge
`0x2F0000` (3 080 192 B), **zweimal** gelesen, beide Lesungen identisch:
SHA-256 `3b907b251b32b034f2ac112f25862aaa317c827707837ccf2982232e9dc05cc9`.
Die ersten 1 756 544 B sind byteidentisch mit dem dokumentierten a9a1156-Release
(`2d069d2d…9305`). Zusaetzlich `0x0..0xFFFF` (Bootloader, Partitionstabelle, NVS-
Anfang/PHY-Bereich) gelesen: `2244ca13dde080028dc1943ba830c65d0ee5304317ed2ae5773aebf9b139f46a`.

## 2. Flash Mess-Overlay M und Rueckflash (MEASURED)

App-only `write-flash` ab `0x10000`, Flash-Parameter `keep`, kein Erase-all:
Image `overlay.bin` SHA-256 `b8ac4001dd893597480b86e77dc5c3f45a8a0d4c952883018be13a7db32d15eb`
(1 763 568 B, Quelle `49dc35f` + Overlay-Diff, Commit `0062dea`). esptool
`Hash of data verified`; Readback-Hash gleich dem Image; `0x0..0xFFFF` nach dem
Flash byteidentisch zu vorher. Bootlog: Source `0062dea…`, aktorlos.

**Wiederherstellung:** die gesicherte Partition (`3b907b25…`) zurueckgeschrieben
(`Hash of data verified`), anschliessend die gesamte Partition ausgelesen:
`3b907b25…` (identisch), `0x0..0xFFFF` unveraendert (`2244ca13…`). Kontrollboot:
Source `a9a1156…`, `esp32_release`, `real actuators: disabled`, `application: ready`,
Heartbeats, Main-HWM 11 704 B wie vor dem Test. **Rueckfall verifiziert; das Geraet
laeuft wieder auf dem urspruenglichen Stand.** Der Rueckfall-Ablauf ist
[restore.sh](PR206_H_RAM_1/restore.sh); Flash-/Backup-Befehle:
[backup.sh](PR206_H_RAM_1/backup.sh), [flash_overlay.sh](PR206_H_RAM_1/flash_overlay.sh).

## 3. Ablauf (Zeitfenster, Host-Uhr 2026-10-19, ein durchgehender UART-Mitschnitt)

| Phase | Zeit | Inhalt |
|---|---|---|
| S0 | 17:49:30–17:54:30 | Boot M (EN-Reset), 5 min Idle |
| S1–S3 gescriptet | 17:54:30–18:05:06 | Login, 1 Shellabruf, 5-s-Polling ([hram1_client.py](PR206_H_RAM_1/hram1_client.py)) |
| S5 gescriptet | 18:05:06–18:10:06 | 5 min Erholung, keine Anfrage |
| Browser S1–S3 | 18:10:58–18:21:11 | echter headless Chromium via CDP ([hram1_browser.py](PR206_H_RAM_1/hram1_browser.py)): Login ueber das Formular, 10 min eigenes 5-s-Polling der Shell |
| S5 Browser | 18:21:11–18:26:11 | Logout, Browser geschlossen, 5 min Erholung |
| Rueckflash | 18:26 ff. | Wiederherstellung und Kontrollboot |

Zeitlich getrennt, immer genau ein Client. **S4 `NOT_RUN`:** kein aktiver Lauf ohne
Commissioning-Trick erreichbar bzw. angestrebt; `run` war im gesamten Test `null`
(Standby). **Lauf-Pfad auf Hardware `NOT_PROVEN`**; die Host-Evidence (Handler-Fenster
Lauf, Spitze 3 718 B Host) bleibt Teilnachweis.

## 4. Messwerte (MEASURED, Messbild M; Auswertung [analysis_summary.txt](PR206_H_RAM_1/analysis_summary.txt))

| Messgroesse | Wert | Einordnung |
|---|---|---|
| Reset/Panic/Watchdog/Brownout/Bootloop im Testfenster | **keine** (nur der eine geplante `POWERON_RESET` zu Beginn) | |
| `heap_alloc_failed` / `ui_skipped_low_memory` | **0 / 0** | |
| Heartbeat-Luecke (2 199 Heartbeats) | max. 1,50 s, nie > 5 s | |
| Idle-Baseline (S0) intern frei / groesster Block / Minimum | 42 760 B / 40 960 B / 33 900 B | G0-R3: 42 532 / 38 912 B (anderes Image) |
| Minimaler interner Heap im Test | **25 512 B** (gescriptet), **19 896 B** (Browser, beim Shell-Abruf, t=1 292 s, dort 32 032 B frei nach dem Senden) | Phasenstichprobe, Boot-Minimum 33 900 B; kein F4-Skip (Schwelle auf groessten Block 13 312 B) |
| Heap waehrend Status-/API-Verkehr: frei minimal / groesster Block minimal | 29 840 B / 26 624 B (API); Shell 29 844 B / 22 528 B | |
| Shell | 3 Abrufe (1 gescriptet 10 410 B vollstaendig in 0,19 s; 2 Browser), alle komplett; Geraeteseitig `pre`→`sent` einmal 3,0 s (Browser) | |
| `result` der HTTP-Antworten im Geraet (710 + 712 Zeilen) | **immer 0** | |
| **HTTPD-Stack-HWM** (Messbild, obere Schranke durch das Logging) | **2 260 B** (gescriptet), **2 196 B** (Browser); Reserve 116 B ueber dem alten Testfloor 2 080 B | G0-R3: 2 336 B; Messbild-Wert nahe am Floor |
| **Main-Stack-HWM** (gleiche Stelle im Boot wie a9a1156) | **11 272 B** gegen 11 704 B auf a9a1156 (−432 B; `after_platform_begin` 18 232 gegen 18 536 B, −304 B) | Reserve 112 B ueber dem alten Testfloor 11 160 B; Differenz entspricht der statischen Schaetzung (app_main +288 B) |
| Erholung nach S5 gescriptet | 42 368 B frei (99,1 % der Baseline), groesster Block 40 960 B (100 %) bereits bei der ersten 30-s-Probe nach Ende der Last und stabil bis zum Ende der 5 min | Kriterium G0-R3 (>= 90 % in 120 s) erfuellt |
| Erholung nach S5 Browser | 42 348 B (99,0 %), Block 40 960 B | |
| Browser-Netz (CDP) | 355 x HTTP 200, 1 x 401 (anonyme Gruppe vor dem Login, erwartet), 3 x `ERR_ABORTED` (Geschwister der anonymen Gruppe); 20 Proben `online`, Gating `P` stets true, Gruppen 1→111, `run=null`, Anmeldestatus `password-protected` | |

## 5. Befunde und Grenzen

1. **Client-Timeouts (gescriptet), Ursache NOT_PROVEN.** 117 von 118 Gruppen waren
   vollstaendig und gueltig (p50 0,32 s, p95 1,4 s). Einzelne Antworten waren langsam
   (Gruppe 97 `temperatures` 14,4 s, Gruppe 100 `alerts` 4,1 s) und in Gruppe 118
   (18:05:06) kamen `temperatures` und `alerts` nach 30 s nicht an (Timeout), obwohl
   das Geraet beide Antworten mit `result=0` und Handlerdauern von rund 20 ms
   protokollierte und Heap, Stack und Heartbeat unauffaellig waren. Die Verzoegerung
   liegt zwischen Sendeaufruf und Client (WLAN/TCP; im Log kurz zuvor
   `wifi: ADDBA RX DELBA`), nicht im Speicher. Der Client hielt planmaessig bei der
   ersten ungueltigen Gruppe an (ohne Logout; die Session endete mit dem spaeteren
   Neustart). Im Browserlauf trat kein vergleichbarer Ausfall auf (keine HAR/Latenzmessung).
   Das passt zu den offenen historischen Transportbefunden (RES-01, B0–B4) und wird
   nicht als Speicherbefund gewertet; es ist aber ein Stabilitaetsbefund fuer den
   Fernzugriff und deshalb kein `RAM_PASS`-Argument.
2. **Reserven eng gegen die alten Testfloors:** HTTPD 2 196 B (Floor 2 080 B) und
   Main 11 272 B (Floor 11 160 B). Beide sind Testkriterien aus G0-R3, keine
   Safety-Grenzen; es wurden keine neuen Schwellen erfunden. Das Messbild-Logging
   macht den HTTPD-Wert konservativ; der Main-Wert stammt vor dem Overlay-Code
   (Boot-Punkt) und gilt damit auch fuer P.
3. **Unterschiede zu G0-R3:** anderes Image (Spike C0 ohne F4 gegen M mit F4),
   1 statt 4 Sessions, Normalprofil statt Stress, Funklage nicht instrumentiert.
4. **Messbild M ist nicht das Produktimage P** (verschiedene SHA/Hash, siehe Gate-
   Dokument); DRAM/IRAM identisch.
5. Nicht abgedeckt: aktiver Lauf mit `run`-Objekt (S4), Mehrclient, Touch, Langzeit,
   HAR-Latenzen des Browsers, Wiederholung des Timeouts.

## 6. Status je Testteil und Empfehlung

| Teil | Status |
|---|---|
| Identifikation, Backup (2x identisch), Flash M, Readback, Boot-Region unveraendert | MEASURED |
| S0 Boot + Idle | MEASURED (PASS_OBSERVED) |
| S1 Login, S2 Shell (vollstaendig) | MEASURED (PASS_OBSERVED, gescriptet und Browser) |
| S3 gescriptet | MEASURED: 117/118 Gruppen gueltig, 1 Transport-Timeout, Ursache NOT_PROVEN |
| S3 Browser | MEASURED (PASS_OBSERVED) |
| S5 Erholung (2x) | MEASURED (PASS gegen G0-R3-Kriterium) |
| S4 aktiver Lauf mit `run` | NOT_RUN; Lauf-RAM auf Hardware NOT_PROVEN |
| Rueckflash, Vollpartitions-Readback, Kontrollboot a9a1156 | MEASURED (PASS) |
| HTTPD-/Main-HWM-Spitzen unter Last in P | ESTIMATED aus Messbild M |

**Empfehlung: `RAM_PASS=NOT_GRANTED`, begrenzter Teilnachweis fuer Standby/Ein-Client.**
Gruende: S4 nicht ausfuehrbar, schmale Stackreserven gegen die alten Floors, ein
ungeklaerter Transport-Timeout. Fuer einen `RAM_PASS` nach Ownerentscheid waeren
ein akzeptiertes Floor-Verhalten (oder neue Ownerwerte) und eine Entscheidung zum
Lauf-Pfad (Host-Evidence genuegt oder spaeterer Hardwarelauf nach #35) noetig.
