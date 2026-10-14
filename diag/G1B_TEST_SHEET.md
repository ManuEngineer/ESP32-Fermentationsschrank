# PR203 RES-01 – G1b Stufe-D-Diagnose: Hardware-Testblatt (Entwurf)

Status: **PREPARED, OWNER_G1B_AUTHORIZATION=PENDING.** Dieses Blatt gewährt keine
Freigabe. Es setzt Plan `docs/tasks/issue-27-res01-f2-escalation-plan.md` @
`3bb29481495c4baca540ada1f3ae5433a5b46a34` (Stufe D, Abschnitt 4) um; Ownerfreigabe des Plans
liegt vor (F2-D1/D2/D3 GO; F2-D4 grundsätzlich; F2-D5 **nicht** erteilt; F2-D6 nur mit
geprüfter payloadfreier Erfassung und privater Aufbewahrung). Keine Hardwarehandlung ist
damit autorisiert. `G1_IMAGE01=FAIL/HOLD`, `RES-01=BLOCKER`, `PR203_FINAL_GO=HOLD`,
`ACTUATOR_RELEASE=NO`, `F4_THRESHOLD=PROVISIONAL_PENDING_G1`.

## 1. Image: kein neuer Build, kein neuer Flash vorgesehen

Der Hook mit Backtrace (Plan D1) ist **bereits** im Image von G1 IMAGE01 (Diagnosequelle
`bb4f43a69740f234d26a951707991f792cc3ffde`, `main/app_main.cpp` `logFailedHeapAllocation`:
Cache-Abfrage vor Dereferenzierung, höchstens 4 Ereignisse, atomarer Zähler,
`diag_alloc_fail: … pc:`-Zeile). Die Backtrace-Zeile folgt im Rohstrom unmittelbar auf die
`heap_alloc_failed`-Zeile und ging in G1 verloren, weil der Host den Port sofort schloss.
Das „v3-Hook-Delta“ des Plans ist deshalb leer.

| Artefakt | SHA-256 |
|---|---|
| App `g1_app_bb4f43a.bin` (1 760 560 B) | `da9e868874649bd274d21a4f1c5715afd9293288497027004c460efc46df8cf0` |
| ELF `g1_bb4f43a.elf` | `24d8557f965053d566ba6ef3c4bfe745044a505994835e5831b85ff0dbe8c369` |
| Partitionstabelle | `d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d` |

Statische Prüfung am **bereits gebauten** ELF (`hw-evidence-pr203/diag/check_hook_cache_safety.py`,
Toolchain `xtensa-esp-elf` 15.2.0, in dieser Vorbereitung wiederholt): Aufrufabschluss des Hooks =
`esp_backtrace_get_next_frame`, `esp_backtrace_get_start`, `spi_flash_cache_enabled`,
`cache_hal_is_cache_enabled`, `esp_dport_access_reg_read`, `esp_ptr_executable`,
`xPortInIsrContext`, alle IRAM/ROM; **`FLASH_REFS=0`**, identisch mit dem Ergebnis für `3e94cea`
(`check_v3_3e94cea.txt`). Overhead (gemessen in v3, hier unverändert gültig, da kein neuer Code):
+832 B Image, ≈ +112 B Stack in der fehlschlagenden Task, +10,5 ms je Ereignis, ≤ 4 Ereignisse.

Folge: Ändert der Owner dieses Image nicht, ist **kein Flash** nötig. Ob noch dieses Image
läuft, wird vor dem ersten Lauf ohne Schreibzugriff geprüft (Abschnitt 3). Bei Abweichung: HOLD,
kein Reparaturflash ohne neue konkrete Freigabe.

## 2. Host-Werkzeuge (Hosttests grün, 40 Tests)

| Werkzeug | Änderung | Zweck (Plan) |
|---|---|---|
| `diag/g1_capture.py` | Nach einer Anomalie bleibt der Port **≤ 5 s** (höchstens 120 Reads) offen, die Last ist zuvor über `ABORT` gestoppt; Nachlauf in `uart_tail.log` und Rohstrom; jeder Rohchunk mit Hostzeit in `uart_chunks.jsonl`; Tail-Dauer/Bytes in `capture.json`. Kein Nachlauf bei Ctrl-C und Capture-Fehler. | D3, Q3 |
| `diag/c0_load.py` | Je Sample zusätzlich `start_epoch`, `error_phase` (`connect`/`io`), `abort_induced` (nur wahr, wenn `close_sockets` genau diese Verbindung beendete); `load.json` zusätzlich `abort_epoch`/`abort_monotonic` des ersten Abbruchs. `epoch` bleibt das Request-**Ende**. Fehlerklassen unverändert. | D3 |
| `diag/g1_pcap.py` (neu) | `capture` (tcpdump, `-s 54`, Filter `tcp and port 80 and host <Gerät>`), `verify` (schlägt geschlossen fehl bei Länge > 54, nicht untaggedem Ethernet/IPv4/TCP, IP-Optionen, anderem Linktyp; `capture` löscht die Datei bei Fehlschlag), `analyze` (Zähler ohne Adressen/Ports). | D2, Q2 |

Treiber-/Tool-Hashes werden im Gate-Manifest mit dem finalen Commit gebunden.

## 3. Ablauf (je Schritt nur unter konkreter Ownerfreigabe)

1. **Identität ohne Schreibzugriff:** MAC `20:50:0d:1b:2f:34`, Chip ESP32-D0WD-V3; Partitionstabelle
   und App-Bereich lesen (`0x10000`, 1 760 560 B) und gegen die Hashes oben prüfen; State-Store
   (`0x300000–0x3FFFFF`) nicht verändern. Abweichung ⇒ HOLD.
2. **Host-Voraussetzungen (F2-D6):** Paketmitschnitt-Berechtigung für tcpdump (Owner richtet sie
   ein, z. B. Capabilities für das Binary; keine Passwortweitergabe an den Agenten); Interface
   `enp2s0`; Zielverzeichnis privat (`chmod 700`). Vor dem ersten Lauf ein Trockentest auf
   `lo` mit einem lokalen HTTP-Server: `verify` PASS, `analyze` plausibel, **keine Bytes jenseits
   Offset 54** in der Datei (`g1_pcap.py verify` plus Sichtprüfung der Dateigrösse = 24 + 16·N + Σcaplen).
3. **Lauf (bis zu 3 frische Boots):** je Boot Portöffnung (bekanntes FT232R-Reset), P-A wie G1
   (vier Sessions, 60 s Idle, 1200 s Last, 300 s Recovery), `g1_capture.py` mit Tail,
   `g1_pcap.py capture` parallel, Stopp bei erster Anomalie wie G1 (harter Lastabbruch, kein
   Logout, kein Retry). Zwischen den Läufen keine weitere Aktion; nach dem letzten Lauf keine
   Abfrage.
4. **Nach jedem Lauf:** `verify` über die Pcap-Datei; bei Fehlschlag Datei gelöscht und Lauf als
   `PCAP_NOT_PAYLOAD_FREE` markiert; Pcap bleibt privat (nie veröffentlichen); veröffentlicht werden nur
   `analyze`-Zähler, `uart_tail.log` (ohne Netzwerk-Initialisierungszeilen), `uart_chunks.jsonl`,
   `load.json`-Auszüge und die Bytebereiche des Fehlerfensters, jeweils mit Quellhash.
5. **Stopp/HOLD:** unerwarteter Reset ausserhalb des erwarteten Bootresets, Stack unter den
   G1-Stopgrenzen (Main 11 160 B, HTTPD 2 080 B), Identitätsabweichung, Host-Capture-Fehler.

## 4. Entscheidungsregel nach dem Lauf

Mechanisch nach Plan Abschnitt 4 (Tabelle „Entscheidungsregel“). Der Bericht nennt je Lauf:
Dekodierung der `diag_alloc_fail`-Zeile gegen den **gepinnten ELF** (`addr2line`; Symbol,
Task nicht rekonstruiert), Drahtzähler in den 10-s-Bins um die Anomalie, Verlauf des Heaps nach der
Anomalie (selbstverstärkend ja/nein, 5-s-Tail), `abort_induced`-getrennte Fehlerzahlen.

## 5. Vergleichsdefinition für die spätere Stufe E (F2-D4, vorab festgelegt)

*Vorbehaltlich der Ownerbestätigung dieses Blatts; sie wird vor dem ersten Hardwaregate gebunden.*

- **Referenzläufe A:** genau die in Stufe D gefahrenen Läufe mit diesem Image (bis zu 3). Der
  historische Lauf G1 IMAGE01 gilt nur als Kontext (anderer Tag, andere Capture-Fassung).
- **Arm B:** dieselbe Konfiguration mit der **einen** Änderung O1; B-Läufe nur nach Ownerfreigabe
  der Implementierung und in wechselnder Reihenfolge mit A am selben Tag, soweit praktikabel.
- **Gleiche Bedingungen:** Board, AP/Kanal (UART-Beleg), Treiber-/Capture-/Pcap-Hashes, vier frische
  Sessions mit dokumentiertem Alter, Idle-Referenz je Boot, Uhrzeitfenster dokumentiert.
- **Vergleichsfenster (fest):** Lastsekunden `[0, T]` mit `T = min(1200 s, kürzeste beobachtete
  Lastdauer über alle für den Vergleich verwendeten A- und B-Läufe)`. Kennzahlen im Fenster und
  getrennt über den vollständigen Lauf. Es wird kein anderes Fenster nachträglich gewählt.
- **Kennzahlen:** freier interner 8-Bit-Heap und grösster Block (`pre/built/sent`, `diag_30s`),
  Allokationsfehler, Abfall `built→sent`, HTTP-Erfolgsquote und Socketfehler (mit
  `abort_induced` getrennt), API- und Shell-Latenz mit p50/p95/p99/max je Minute und Anzahl > 3 s,
  Shell-Gesamtdauer, Stackreserven, Recovery, Drahtzähler.
- **Toleranz (Ownervorschlag 1,25×):** p95 je Pfad und Shell-Gesamtdauer von B ≤ 1,25 × Maximum der
  p95-Werte der verwendeten A-Läufe im Vergleichsfenster; Überschreitung ist Befund für den Owner,
  kein automatischer Fail. **Harte Fails** (unabhängig von der Toleranz): jedes
  `heap_alloc_failed`, Panic/Watchdog/Brownout/Reset, HTTP-Nicht-2xx, jeder nicht
  abbruchbedingte Timeout oder Verbindungsfehler.
- **Aussagegrenze:** Bei n ≤ 3 je Arm keine statistische Abnahme; die gemessene A-Fehlerrate wird
  neben B ausgewiesen. Eine Restgefahrakzeptanz (F2-D5) ist **nicht** erteilt und wird erst bei
  der Produktabnahme verlangt.

## 6. Nicht Teil dieses Gates

Kein Flash, kein Reset ausser dem bekannten Port-Öffnungsreset, keine Touch-/Hold-Sequenz, keine
Konfigurationsänderung (O1), kein P-A2/P-B/G2, kein Task-Tracking, keine lwIP-Statistik, kein
`CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS`. Jede dieser Handlungen braucht ein eigenes Gate.
