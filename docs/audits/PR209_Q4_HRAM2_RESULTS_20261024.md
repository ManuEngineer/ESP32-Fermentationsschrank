# PR #209 / H-RAM-2 – Messergebnisse (K0 + K1, Stack- und Heap-Profil)

Ausführung am 2026-10-24 (12:5x–15:04 +02:00) auf PR-HEAD `4afc6266aafb9413502a412522d349c35e9cba5d`
(Plan Rev. 3.3) unter der Ownerfreigabe in PR-Kommentar 6383231599 (`G-F`, `G-S`, `G-U`, `G-R`, `G-B`;
**nicht** `G-R7`, `G-RUN`). Board ohne Aktoren. **Dies ist ein Messbericht; er enthält keine Freigabe.**
Rohdaten, Backups und Images liegen privat lokal (Modus 700, enthalten Zugangsdaten/SSID) und sind
**nicht** im Repository; hier nur Hashes und bereinigte Auszüge
([Messwerte](PR209_Q4_HRAM2_MEASUREMENTS_20261024.tsv), erzeugt mit
[EXTRACT.py.txt](PR209_Q4_HRAM2_EXTRACT.py.txt)).

## 0. Gesamturteil (ehrlich)

| Gate / Frage | Status | Begründung |
|---|---|---|
| H-RAM-2 K0 + K1 Durchführung | **PARTIAL** | B0, B1, B1u, B3, B2, B4, B5(b), B6 und authentifizierter Heap-Lauf (15 min) ausgeführt, kein Reset/Panic/`heap_alloc_failed`. Nicht erreichbar/nicht gelaufen: erfolgreicher Run-Pfad, Run-Store, Checkpoint, Maximalkatalog (`NOT_RUN`). |
| Wiederherstellung | **PASS** (Hash) | `state_store`, Boot-Bereich, Factory-App byteidentisch zu `P0` (§5). |
| Kontrollboot der wiederhergestellten Original-Firmware | **NOT_RUN** | Die funktionale Startfähigkeit der wiederhergestellten Firmware wurde **nicht** erneut getestet; belegt ist nur Byte-Gleichheit mit `P0` (§5). |
| `SYSTEM_NVS_WRITE_AT_P1` | bestätigt `EXCLUDED` | Boot-Bereich (`nvs`/`phy_init`) nach jedem Boot hash-gleich `P0` (§4). |
| `HTTPD_FULL_CHAIN_STACK` | **NOT_PROVEN** | Statisches Modell hatte eine Lücke (B3 überschritt die Rechnung, §3.2); ein virtueller Aufruf bleibt unaufgelöst; erfolgreicher Run-Pfad nicht messbar. |
| `HTTPD_STACK_RESERVE` (512-B-Planungsboden, Stack 8 192 B) | **NOT_PROVEN** (statisch jetzt unter dem Boden) | Statisch korrigiert max. 7 664 B + 320 B = 7 984 B → 208 B Rest (< 512 B). Gemessen (nur abgelehnte/„unavailable“ Entscheidungspfade, Settings, Polling): größte Nutzung 6 468 B beim 8-KiB-Profil → 1 724 B Rest. Beides zusammen belegt die Reserve **nicht**, weil der tiefste Pfad (erfolgreicher Run) nicht gelaufen ist. Keine Stackänderung vorgeschlagen (Ownerbedingung 4). |
| `ESP32_HEAP_PEAK` / `ESP32_LARGEST_FREE_BLOCK` | **PARTIAL** | Normalbetrieb-Teilnachweis (§3.3); Peak-Szenarien (Run-Store, Maximalkatalog) nicht gelaufen. |
| `C5_RESOURCE_GATE` | **HOLD** | unverändert |
| `PR209_MERGE` | **HOLD** | bis zum unabhängigen Review der Ergebnisse |
| `ACTUATOR_RELEASE` | **NO** | unverändert |

## 1. Builds und Vorprüfung (vor dem ersten Flash)

Non-merge-Diagnoseoverlay (lokaler Worktree `local/pr209-hram2-diag`, nicht gepusht): Wrapper um
`Impl::handleRequest` (`hram_http pre/sent`: Heap, `uxTaskGetStackHighWaterMark` des HTTPD-Tasks),
30-s-Probe in `app_main` (`hram_30s`, `hram_httpd_task`). Produktbasis `80bfb28`.

| Profil | Quelle | Image `esp32_fermentationsschrank.bin` SHA-256 | HTTPD-Stack |
|---|---|---|---|
| Stack | `42a367c1d3e2df4a28fb58e626544094193e1827` | `668a2d6d73f3e292559b68b56dcd118ec586e23e313a3facae7378a7ef1b85ee` | 12 288 B |
| Heap | `39f7abe343de809175ec56f49f409808429da7dd` | `ada9f89cdedc6f66c56c2726d0d5789b70df16125e208d519c66da051526a543` | 8 192 B (Produkt) |

Bootloader `37d59dd2f161a136e0d7af32322b9386856fa36f7818d56eb3c8fca5e9e02470` und Partitionstabelle `d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d` sind in beiden Profilen identisch (nur der App-Bereich `0x10000` wurde geschrieben).
Vor dem ersten Flash: IDF-Pin `fff9895c…`, PHY-Bibliotheksstand und effektive `sdkconfig` der tatsächlichen
Diagnoseimages mit `PRE_P1_REPORT` verglichen (PHY im ELF verglichen; keine Abweichung) – daher keine
Auslösung der Bedingung `SYSTEM_NVS_WRITE_AT_P1=NOT_PROVEN`.
Zwei unabhängige byteidentische Backups (`A`/`B`) von App und `state_store` vor jedem Schreibzugriff; zusätzliche
Wiederholungslesung `C` vor dem Flash. SHA-256: Boot `2244ca13dde080028dc1943ba830c65d0ee5304317ed2ae5773aebf9b139f46a`,
Factory `3b907b251b32b034f2ac112f25862aaa317c827707837ccf2982232e9dc05cc9`,
`state_store` `fbdd870529bc6e9c0d87adc5aea9fe5b2eb496ab09ef6473c80129886d3d8b8d`.
Flash jeweils `G-F` app-only `0x10000`, Readback-Hash gleich Image (`APP_READBACK_EQUALS_IMAGE=YES`), Skripte
[FLASH_APP](PR209_Q4_HRAM2_FLASH_APP.sh.txt).

## 2. Stack-Profil (12 KiB): ein frischer Boot je Pfadmenge

HWM = `uxTaskGetStackHighWaterMark` des HTTPD-Tasks (frei gebliebene Bytes, kumulativ seit Taskstart).
`M(B) = 12 288 − HWM`. `R(B)` = Plan-Regel `max(S_entry 1 104, S_log, S(P) aller erreichten Pfade) + 320`
mit den **ursprünglichen** Modellwerten (Plan §15.6.3: Shell 3 808; Read-only 5 952; Login 3 792; Prepare 6 352;
Confirm 4 784; Visitor-Sensor 6 816; Settings 7 024; Decision 7 440). `R_neu` = mit korrigiertem Modell (§3.2).

| Boot | Pfadmenge | HWM [B] | M(B) [B] | R(B) alt | R_neu | Urteil |
|---|---|---|---|---|---|---|
| B0 | Idle, kein Request (30-s-Probe liest Task-HWM) | 11 360 | 928 | – | – | nur Basislinie |
| B1 | GET Shell (unauth.) | 8 620 | 3 668 | 4 128 | 4 944 | M ≤ R |
| B1u | GET Status ohne Session (401-Probe, **kein** Encodernachweis) | 9 032 | 3 256 | – | – | nur Fehlerpfad |
| B3 | Login (isoliert) | 7 564 | 4 724 | 4 112 | 4 928 | **M > R_alt (+612 B)**; M ≤ R_neu |
| B2 | Login + GET Status (kumulativ, nicht isoliert) | 6 348 | 5 940 | 6 272 | 6 784 | M ≤ R |
| B4 | Login + `sensor-selection` (abgelehnt) | 6 348 | 5 940 | 7 136 | 7 648 | M ≤ R |
| B5b | Login + acht weitere Intents (abgelehnt/„unavailable“) | 6 284 | 6 004 | 7 760 | 7 984 | M ≤ R |
| B6 | Login + Gerätenamen-Commit ×2 | 5 616 | 6 672 | 7 344 | 7 856 | M ≤ R |

B5 (erster Versuch) wurde nach einem Clientfehler (409 `sequence-conflict` wegen falscher Sequenzführung nach
„unavailable“) verworfen; B5b wiederholt mit korrigiertem Client ([K1_REQ](PR209_Q4_HRAM2_K1_REQ.py.txt)); B5-Wert
(6 272 B HWM) liegt ebenfalls im Rahmen. Kein Boot zeigte Panic, Reset oder `heap_alloc_failed`.
Die B0-Probe liefert den HTTPD-HWM ohne Handleraufruf (die 30-s-Task-HWM-Probe liegt im genehmigten Diagnoseumfang).

## 3. Auswertung

### 3.1 Befund
Alle Boote außer **B3** liegen innerhalb der Plan-Regel `M ≤ R`. Die größte gemessene Stacknutzung des
HTTPD-Tasks über alle Boote ist **6 672 B (B6)**. Die abgelehnten bzw. „unavailable“ Decision-Pfade (B4/B5b)
erreichen 6 004–5 940 B; der **erfolgreiche** Run-/Run-Store-Pfad (statisch tiefster Pfad) wurde nicht erreicht
(`G-RUN` nicht freigegeben, Aktorbezug). Deshalb ist der tiefste Pfad **nicht** gemessen.

### 3.2 Abweichung B3 und Korrektur des statischen Modells
B3 (Login, isoliert) nutzte 4 724 B, das Modell sagte 3 792 B + 320 B = 4 112 B voraus (Überschreitung 612 B).
Ursache: Die Aufrufgraph-Auflösung (ELF/ROM) folgte nur direkten `call*`/`j`-Befehlen. Mit `-mlongcalls`
sind Aufrufe über Regionen `l32r` + `callx8`; dadurch fehlten Teilbäume ohne `.ci`-Datei (SDK/PSA/`main/`),
u. a. die Key-Derivation im Login. **Modelllücke, nicht Messfehler.** Korrektur: Auflösung um `l32r`+`callx`-Kanten
erweitert ([RESOLVE9](PR209_Q4_HRAM2_RESOLVE9.py.txt); 8 943 Kanten aus dem Disassembly-Text und 1 141 aus der
Bytesweep-Auswertung im Release-Profil; 1 678 zusätzliche ELF-Knoten). Neue statische Werte (Release-Profil
`esp32_release`, „resolved“, ohne 320-B-Zuschlag; Bring-up-Profil identisch):

| Pfad | alt | neu |
|---|---|---|
| GET Shell | 3 808 | 4 624 |
| GET read-only | 5 952 | 6 464 |
| POST Login | 3 792 | 4 608 |
| Prepare | 6 352 | 6 864 |
| Confirm | 4 784 | 5 296 |
| Visitor-Sensor | 6 816 | 7 328 |
| Settings | 7 024 | 7 536 |
| Decision / Run-Store | 7 440 | **7 664** |

Das Maximum steigt von 7 440 auf **7 664 B**; mit 320 B Zuschlag 7 984 B → Rest zu 8 192 B = **208 B**
(vorher 432 B), also unter dem 512-B-Planungsboden. Zwei virtuelle Aufrufe bleiben unaufgelöst
(`unresolvedReach=2`), das Modell bleibt eine Näherung. Login neu 4 608 + 320 = 4 928 B ≥ gemessene 4 724 B
(Plausibilitätsprüfung bestanden; B3-Messung deckt sich mit der korrigierten Rechnung). Die Zahlen des
alten Modells (§15.8: 7 440 B / 432 B) sind damit **überholt**; sie wurden nicht rückwirkend still verändert.

### 3.3 Heap-Profil (8 KiB, Produktstackgröße), authentifizierter Lauf
Ablauf (Client [HEAP_RUN](PR209_Q4_HRAM2_HEAP_RUN.py.txt), ein Client): 310 s Idle → Login + Shell →
600 s Polling (120 Gruppen aus je GET `status`/`temperatures`/`alerts`, 5-s-Takt, gleichzeitig) → elf Mutationen
(abgelehnt/„unavailable“, wie B4/B5) → Gerätename setzen und zurücksetzen (beide `applied`) → Settings-GETs.

| Phase | Frei intern min. [B] | `minimum_free_heap` [B] | größter Block min. [B] | HTTPD-Stack-HWM min. [B] |
|---|---|---|---|---|
| Idle (0–310 s, 30-s-Probe) | 42 600 | 34 400 | 32 768 | – |
| Login + Shell | 33 884 | 32 964 | 30 720 | 2 252 |
| Polling (600 s, 3 parallele GETs) | 25 460 | 22 428 | 22 528 | 2 108 |
| Abgelehnte Intents | 37 548 | 22 428 | 32 768 | 2 108 |
| Namenscommit ×2 | 36 652 | 22 428 | 31 744 | **1 724** |
| Gesamt | 25 460 | **22 428** | **22 528** | **1 724** (Nutzung 6 468 B von 8 192 B) |

Ergebnis: 120/120 Pollinggruppen vollständig gültig (360 × HTTP 200, größte Gruppe 1,06 s); alle
Mutationen wie erwartet (Namenscommit und Rücksetzen `applied`); kein `heap_alloc_failed`, kein Panic, genau ein
Reset (Start). Main-Task-Stack-HWM konstant 11 272 B. Der Minimalwert der Heap-Reserve entsteht beim parallelen
Polling, nicht beim Namenscommit. Das belegt Normalbetrieb **mit abgelehnten Entscheidungen**, nicht den
Spitzenbedarf des erfolgreichen Run-Starts.

## 4. Persistenz: Hashkette und Zuordnung

Boot-Bereich (`0x0–0xFFFF`, also `nvs`/`phy_init`) nach **jedem** Boot und nach beiden Wiederherstellungen
hash-gleich `P0` (`2244ca13…`); `SYSTEM_NVS_WRITE_AT_P1=EXCLUDED` bestätigt. `state_store` (Reihenfolge der Boots;
ohne Zwischenwiederherstellung, Kette fortgesetzt, jede Abweichung dem Login-Schreibpfad zugeordnet):

| Nach Boot | `state_store` SHA-256 | Bewertung |
|---|---|---|
| P0 | `fbdd870529bc6e9c0d87adc5aea9fe5b2eb496ab09ef6473c80129886d3d8b8d` | Ausgangszustand |
| B0, B1, B1u | gleich P0 | kein Login, nichts geschrieben |
| B3 | `0239b416b8c0b9b74a3a3cf5652baac4d6e6a0702c778622da1800f4593c9507` | Login schreibt auth0 |
| B2 | `ce92ac6d7a9cca596a2c216c0e0a9949cb1d087bf57f02fd76477f69dc17ccf0` | Login |
| B4 | `894a89d53c4431b181bb53bed023ebbd94d53a91668f624cf83fdf7f9a7e8224` | Login |
| B5 | `300e82cdef1acdb73bec57cf8c53ee66fed2c652b91e7b94059b6e0d1d06540e` | Login |
| B5b | `05bb8560bc58f7138817921a125d7140eda342e023b8bfbd8e73b256b47366a4` | Login |
| B6 | `c3d59a7b46fe78c01fa9cca2a49579d531e6b4ca62abc198a14437a23c863a93` | Login + Konfigurations-Rotation (Gerätename) |
| RB-2 nach Stackserie | gleich P0 (Readback) | byteexakt wiederhergestellt |
| Heap-Lauf | `cf295bd53f532393d18edfd3231b5c5ac13ae9188e24d38410a51f072a984bc8` | s. u. |

Offline-Diff Heap-Lauf gegen P0 ([NVSDIFF](PR209_Q4_HRAM2_NVSDIFF.py.txt), 23/23 Schlüssel): geändert nur
`auth0` (Envelope-Version 82 → 83: ein Login), `cm0`, `cm2`, `cr0`, `cr1`, `uc1`, `uc2` (Konfigurations-Rotation durch
zwei Namenscommits); Gerätename wurde auf den Originalwert zurückgesetzt (Client bestätigt). Keine unerwarteten
Einträge. Das Gerätenamen-Zurücksetzen ist **logisch**, nicht byteidentisch; Byteidentität stellt erst RB-2 her.

## 5. Wiederherstellung (RB-2 und Rück-Flash)

| Schritt | Ergebnis |
|---|---|
| Nach Stackserie: `write-flash 0x300000 state_store_A.bin`, Readback | `STATE_STORE_RESTORED_EQUALS_P0=YES`, Boot-Bereich gleich P0 |
| Nach Heap-Lauf: gleicher Ablauf ([RESTORE](PR209_Q4_HRAM2_RESTORE.sh.txt)) | `STATE_STORE_RESTORED_EQUALS_P0=YES`, `BOOT_AREA_EQUALS_P0=YES` |
| Original-App Rück-Flash `0x10000`, volle Länge `0x2F0000` aus `factory_A.bin` | Readback-Hash `3b907b251b32b034f2ac112f25862aaa317c827707837ccf2982232e9dc05cc9` = P0, `APP_READBACK_EQUALS_IMAGE=YES` |
| Abschlussprüfung [HASHCHECK](PR209_Q4_HRAM2_HASHCHECK.sh.txt) | Boot `2244ca13…`, `state_store` `fbdd8705…` = P0 |
| **Kontrollboot der Original-Firmware (`a9a1156`)** | **`NOT_RUN`**: Das Board blieb im ROM-Bootloader. Funktionale Startfähigkeit der wiederhergestellten Firmware ist **nicht** erneut getestet; belegt ist nur Byte-Gleichheit aller drei Bereiche mit `P0`. |

Das Web-Passwort wurde nur in einer privaten Datei (Modus 600) gehalten und nach dem Lauf vernichtet; weder
Passwort, Cookie noch CSRF-Token wurden geloggt oder committed.

## 6. Nicht gelaufen / offene Punkte

- Erfolgreicher Run-Start, Run-Store/Checkpoint-Schreibpfad, RAM-only-Sensor, Maximalkatalog: `NOT_RUN` (`G-RUN` nicht
  freigegeben, Aktorbezug).
- Gesamtkette HTTPD-Stack: statisch jetzt 208 B Rest gegen 8 192 B (< 512 B Planungsboden); gemessene Pfade tiefer
  Decision-/Settings-Ablehnungen 1 724 B Rest. Ob 8 192 B genügen, ist ohne den tiefsten (erfolgreichen) Pfad
  **nicht belegt**; Entscheidung über Stackgröße/Partition ist **Ownerentscheid** und hier nicht vorweggenommen.
- Modell: zwei virtuelle Aufrufe unaufgelöst; Modell nur als Näherung, Messwerte ersetzen kein Pfadmodell.
- Kontrollboot Original-Firmware (s. o.).
- Die Diagnoseimages sind lokal (Worktree nicht gepusht); die Messung ist nur mit diesen Hashes reproduzierbar.

Alle Statuswerte vorbehaltlich unabhängigem Review.
