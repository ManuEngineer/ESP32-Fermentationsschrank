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
läuft, wird vor dem ersten Lauf ohne Schreibzugriff, aber mit den in Abschnitt 3 benannten Resets P1/P3
geprüft. Bei Abweichung: HOLD,
kein Reparaturflash ohne neue konkrete Freigabe.

## 2. Host-Werkzeuge (Hosttests grün)

| Werkzeug | Verhalten | Zweck (Plan) |
|---|---|---|
| `diag/g1_capture.py` | Nach einer Anomalie bleibt der Port **≤ 5 s** (höchstens 120 Reads) offen, die Last ist zuvor über `ABORT` gestoppt; Nachlauf in `uart_tail.log` und Rohstrom; jeder Rohchunk mit Hostzeit in `uart_chunks.jsonl`; Tail-Dauer/Bytes und die SHA-256 von `g1_capture.py`/`c0_load.py` in `capture.json`. Kein Nachlauf bei Ctrl-C und Capture-Fehler. | D3, Q3 |
| `diag/c0_load.py` | Je Sample zusätzlich `start_epoch`, `error_phase` (`connect`/`io`), `abort_induced`; `load.json` zusätzlich `abort_epoch`/`abort_monotonic` des ersten Abbruchs. `epoch` bleibt das Request-**Ende**. Fehlerklassen unverändert. **`abort_induced` ist abort-assoziiert, nicht beweisend kausal:** wahr nur, wenn der Guard auf genau dieser Verbindung ein aktives Socket erfolgreich per `shutdown` beendet hat (inaktive Verbindung oder fehlgeschlagenes `shutdown` ⇒ falsch). Ein Fehler ohne dieses Merkmal ist ein eigener Clientbefund. | D3 |
| `diag/g1_pcap.py` | `capture`: tcpdump mit `-p` (kein Promiscuous-Modus, daher ohne `CAP_NET_ADMIN`) und `-s 54`, Filter `tcp and port <80> and host <Gerät>` (`--port` nur für den Loopback-Trockentest, im Lauf immer 80); endet bei einem der `--stop-file`-Pfade (für G1b: `<Lauf>/candidate_capture/ABORT` **und** `STOP`) oder `PCAP_STOP`, bei hartem Limit (`--max-seconds`, Vorgabe 1961 s, gezählt ab dem tatsächlichen tcpdump-Start, Herleitung in Abschnitt 3), bei Interrupt, und beendet tcpdump immer (SIGINT → terminate → kill). Ergebnis in `pcap_result.json` (Status, Exitcode, Grund, Paketzahl, Tool-Hash); Exitcodes: 0 PASS, 2 nicht payloadfrei (Datei gelöscht), 3 tcpdump-Fehler (Start, Berechtigung, Exitcode, Beenden), 4 Datei fehlt/leer/unvollständig/Paketzahl ≠ tcpdump-Meldung, 5 Interrupt, 6 hartes Limit. **Nur Exitcode 0 ist ein PASS**; sonst HOLD für die Drahtevidence des Laufs (eine gültige, payloadfreie Datei bleibt privat erhalten). Verweigert Speicherorte innerhalb eines Git-Worktrees. `verify`: streng (siehe unten). `capture` legt `PCAP_RUNNING` erst an, wenn tcpdump nach der 1-s-Prüfung läuft. `analyze`: Zähler ohne Adressen/Ports, mit UTC-Anker (Abschnitt 4). | D2, Q2 |

**Beobachtungsmodus (nur diagnostisch, explizit mit `--observe-network-timeouts`, Standard unverändert der Hard-Stop):** Nach dem ersten **echten** (nicht abort-induzierten) Netzwerk-Timeout einer **Lastanfrage** (`connect` oder `io`) ist der Lauf unumkehrbar `FAIL/HOLD` (`load.json` `observation`: erster Timeout mit Start-/Endzeit und Phase, alle Folge-Timeouts, Stopgrund, Beobachtungsdauer, Endzustand); die geplante Last wird höchstens **20 s** weiter beobachtet (UART/Pcap laufen weiter, keine Retries, keine neuen Logins, keine zusätzliche Last), **harter Abbruch beim achten** echten Timeout. 20 s und acht sind konservative Diagnosegrenzen, **keine** Sicherheits- oder Abnahmeschwellen. Sofortiger Hard-Stop bleibt bei: erstem `heap_alloc_failed`, Panic, Watchdog, Brownout, Reset, Heartbeat-Lücke > 5 s, Stack-Grenzverletzung, Identitäts-/Toolabweichung, HTTP-Statusfehler, jedem anderen Request-Fehler (`connection-error`, nicht abort-induzierter `http-protocol-error`), Login-/Auth-Fehler, Supervisor-/Mitschnittversagen. Endet die geplante Last nach einem beobachteten Fehler, folgt **keine** Recovery und keine Abnahme. Volumenschutz: mehr als 20 000 Samples oder 256 MiB UART-Rohdaten stoppen fail-closed (`sample-volume-limit`, `log-volume-limit`), es werden keine Zeilen verworfen. Nie `PASS` nach einem echten Fehler.

**Payloadfreier Nachweis (`verify`, fail-closed):** `-s 54` ist nur eine Obergrenze; der Beweis ist die
Prüfung jedes Records: klassisches pcap, Linktyp Ethernet, keine Restbytes, keine abgeschnittenen oder
zu kurzen Records, gespeicherte Länge **genau** `min(Originallänge, 54)`, ungetaggtes Ethernet, IPv4 mit
IHL 5, **ohne Fragmentierung** (MF und Fragment-Offset 0), TCP, TCP-Headerlänge ≥ 20 B,
IP-Gesamtlänge konsistent mit TCP-Header und Drahtlänge, gespeicherte Bytes enden spätestens im TCP-Header.
Ein leerer Mitschnitt ist kein PASS. Nicht eindeutig header-only lesbare Frames ⇒ Datei verweigert.
Negativtests: Länge > 54, VLAN, UDP, IP-Optionen, Fragmente, ungültige/inkonsistente Längen, Restbytes,
abgeschnittene Records, leere/zu kurze Dateien, falscher Linktyp (`test_g1_pcap.py`).
Die privaten Originale werden **nie** veröffentlicht oder ins Repository gelegt; nur abgeleitete Zähler.

**Skriptidentität (H0, vor jedem Gate-Lauf):** Die auszuführenden Skripte müssen dem geprüften,
**gepushten** Commit des Review-Branches entsprechen: `git rev-parse HEAD` des Arbeitsbaums = im Gate
genannter Commit, sauberer Baum, und `sha256sum -c diag/review/g1b/HOST_TOOLS_SHA256SUMS` = OK.
Zusätzlich schreiben `g1_capture.py` und `g1_pcap.py` ihre tatsächlichen Hashes in `capture.json` bzw.
`pcap_result.json`; Abweichung vom Manifest ⇒ HOLD, Lauf nicht auswerten.

## 3. Ablauf (je Schritt nur unter konkreter, noch zu erteilender Ownerfreigabe)

**Geräteaktionen, ausdrücklich und nummeriert (Anzahl und Reihenfolge sind Teil des Gates):**

| Nr. | Aktion | Wirkung auf das Gerät | Trennung |
|---|---|---|---|
| P1 | esptool-Sitzung zur Identität: automatischer Reset in den ROM-Bootloader über DTR/RTS des FT232R, MAC/Chip lesen | **1 Reset** (Bootloader), App läuft nicht | Preflight, nicht Testboot |
| P2 | in derselben Sitzung: Partitionstabelle (`0x8000`, 3072 B) und App-Bereich (`0x10000`, 1 760 560 B) **nur lesen**, SHA-256 gegen Abschnitt 1 prüfen; State-Store nicht lesen/schreiben | kein Schreibzugriff | Preflight |
| P3 | Sitzungsende mit Hard-Reset zurück in die Anwendung | **1 Reset** (App-Boot), UART dabei **nicht** Teil der Messung | Preflight, kein Testboot |
| R1–R3 | je Lauf: Portöffnung `g1_capture.py` (bekannter FT232R-Reset beim Öffnen), danach P-A | **1 Reset je Lauf** = frischer, erfasster Testboot | getrennt von P1–P3 |

Keine weiteren Resets, Flashvorgänge, Touch-Sequenzen oder Abfragen. Reihenfolge: P1→P2→P3, dann
R1, (R2, R3 nur wenn der Owner nach Sichtung des vorherigen Laufs fortsetzt oder das Gate sie vorab
nennt). Ist die gepinnte App **ohne** diese ausdrücklich genehmigten Aktionen nicht verifizierbar
(Gate nennt P1–P3 nicht, Reset-Verhalten abweichend, Hash ≠ Abschnitt 1, MAC ≠ erwartet, Lesefehler):
**HOLD**, kein Lauf. Eine Prüfung allein über die UART-Zeile der Quellversion beim Testboot ersetzt
P2 nicht (sie beweist die Quelle, nicht den App-Inhalt).

1. **H0** Skriptidentität (Abschnitt 2); keine Gerätekontakte.
2. **Host-Voraussetzungen (F2-D6):** Paketmitschnitt-Berechtigung für tcpdump (Owner richtet sie ein,
   eingerichtet: private Kopie `/home/manuel/g1b-tools/tcpdump` mit `cap_net_raw=ep`, Mode 0700 im eigenen
   Verzeichnis; sie wird nur über einen temporären `PATH` (`/home/manuel/g1b-tools` vorn) genutzt, keine
   dauerhafte Hostkonfiguration; keine Passwortweitergabe an den Agenten); Interface `enp2s0`;
   Zielverzeichnis privat (`chmod 700`, ausserhalb jedes Git-Worktrees). Trockentest auf `lo` mit lokalem
   HTTP-Server vor P1 (**bestanden**, `diag/review/g1b/LOOPBACK_DRYRUN_EVIDENCE.md`): `capture`→`verify`→`analyze` PASS; Dateigrösse = 24 + 16·N + Σ gespeicherte Längen;
   `pcap_result.json` zeigt `PASS`.
3. **P1–P3** wie oben (nur mit Freigabe).
4. **Lauf R1 (…R3) – feste Reihenfolge (Mitschnitt vor der Last):**
   1. `g1_capture.py` starten (**ohne** `START_LOAD`); Port-Öffnungsreset = Testboot.
   2. **Bereit** = in `uart.log` sind Quelle, `application: ready`, `real actuators: disabled`, Release-Profil,
      `HomeConnected` und ein frischer Heartbeat belegt. Maximal **120 s** warten; sonst `STOP` setzen und HOLD.
   3. `g1_pcap.py capture … --stop-file <Lauf>/candidate_capture/ABORT --stop-file <Lauf>/candidate_capture/STOP`
      starten (`--max-seconds` Vorgabe 1961). Auf `<Ausgabeordner>/PCAP_RUNNING` warten, **maximal 30 s**; sonst
      `STOP` setzen und HOLD (kein Lastlauf ohne laufenden Mitschnitt).
   4. **Erst bei vorhandenem `PCAP_RUNNING`** `START_LOAD` anlegen. `g1_capture.py` startet die Last, sobald
      zusätzlich die Uptime ≥ 180 s erreicht ist (bestehende Bedingung); Login, 60 s Idle, 1200 s Last,
      300 s Recovery wie G1. Stopp bei erster Anomalie wie G1 (harter Lastabbruch, kein Logout, kein Retry).
   5. **Endsignal:** Bei Anomalie schreibt der Supervisor `ABORT`. Bei regulärem Ende (Recovery abgeschlossen,
      `load.json` mit `status = COMPLETED_PENDING_REVIEW`) legt der Operator **spätestens 60 s danach** `STOP`
      an; beide beenden Capture und Mitschnitt über `--stop-file`. Ohne Signal endet der Mitschnitt am harten
      Limit mit HOLD (Exitcode 6).
   6. **Zeitbudget ab tcpdump-Start (Vorgabe `RUN_BUDGET_SECONDS`, in `g1_pcap.py`):** START_LOAD-Wartezeit 60 s +
      Uptime-Gate ≤ 180 s + erster Heartbeat 5 s + 4 Logins × (8 s Timeout + 1 s Pause) = 36 s + Idle 60 s +
      Last 1200 s + Recovery 300 s + Endsignal 60 s + Aufschlag 60 s = **1961 s**. Der veröffentlichte G1-Verlauf
      (Login bis Lastende 1575 s) plus Wartezeiten bleibt darunter; ein früherer Start des Mitschnitts mit dem
      Capture (≈ 431 s Vorlauf) wäre mit 1800 s abgebrochen und ist **nicht zulässig**. Überschreitung = HOLD.
5. **Nach jedem Lauf:** `pcap_result.json` prüfen (nur `PASS` zählt); `analyze` nur über verifizierte
   Dateien; veröffentlicht werden nur `analyze`-Zähler, `uart_tail.log` (ohne Netzwerk-
   Initialisierungszeilen), `uart_chunks.jsonl`, `load.json`-Auszüge, Fehlerfenster-Bytebereiche und die
   Tool-Hashes, jeweils mit Quellhash.
6. **Stopp/HOLD:** unerwarteter Reset ausserhalb der erwarteten Bootresets, Stack unter den G1-Stopgrenzen
   (Main 11 160 B, HTTPD 2 080 B), Identitätsabweichung, Skripthash-Abweichung, Host-Capture-Fehler.

## 4. Entscheidungsregel nach dem Lauf

Mechanisch nach Plan Abschnitt 4 (Tabelle „Entscheidungsregel“). Der Bericht nennt je Lauf:
Dekodierung der `diag_alloc_fail`-Zeile gegen den **gepinnten ELF** (`addr2line`; Symbol,
Task nicht rekonstruiert), Drahtzähler in den 10-s-Bins um die Anomalie, Verlauf des Heaps nach der
Anomalie (selbstverstärkend ja/nein, 5-s-Tail), `abort_induced`-getrennte Fehlerzahlen.

**Zeitverknüpfung Draht ↔ UART:** `analyze` gibt `first_packet_epoch`/`first_packet_utc`,
`last_packet_epoch`/`last_packet_utc`, die Bin-Definition (Bin k = `[first + k·10 s, first + (k+1)·10 s)`, ein
fehlendes Bin = 0 Ereignisse innerhalb der erfassten Spanne) sowie je Bin `start_epoch`/`end_epoch` aus. Mit
`g1_pcap.py analyze … --event-epoch <Host-Epoch des heap_alloc_failed>` trennt `event_window` die Bins in
`before` (Ende ≤ Ereignis, ≤ 30 s davor), `straddling` (enthält das Ereignis, nicht zuordenbar) und `after`;
`covered=false` heisst: Ereignis ausserhalb der erfassten Spanne. Ereigniszeit = Host-Epoch des Rohchunks, der die
`heap_alloc_failed`-Zeile enthält (`uart_chunks.jsonl`, Unsicherheit ≤ 50 ms Lesefenster). Pcap- und UART-Zeiten
stammen von derselben Hostuhr; der Uhrzustand (NTP-Synchronisation) wird im Lauf dokumentiert, ein Versatz wird nicht korrigiert.

## 5. Vergleichsdefinition für die spätere Stufe E (F2-D4; Ownerbestätigung vor dem Gate offen)

`OWNER_CONFIRMATION_F2_D4=PENDING` – dieses Blatt wird erst mit der Bestätigung dieses Abschnitts
gebunden. Stufe D und Stufe E sind zeitlich und durch eigene Gates getrennt.

- **Zulässige A-Referenzen:**
  1. *Vollständige Referenz:* ein A-Lauf, der Last (1200 s) und Recovery (300 s) ohne harten Fail
     durchläuft. Nur solche Läufe dienen dem Vergleich über den vollständigen Lauf.
  2. *Frühe-FAIL-Referenz:* ein A-Lauf, der früh endet (z. B. bei 312 s), ist **nur** für das begrenzte
     Vergleichsfenster unten zulässig und wird ausdrücklich als solcher ausgewiesen.
  3. Der historische Lauf G1 IMAGE01 ist Kontext, keine Referenz (anderer Tag, andere Capture-Fassung).
- **Stufe-D-A-Läufe sind keine Referenzen für Stufe E am selben Tag.** Das Stufe-E-Gate legt eigene
  A'-Vergleichsläufe fest: dasselbe unveränderte Image (Hashes aus Abschnitt 1) und dieselben
  Host-Skripte (H0), je Lauf mit eigenem Unter-Gate, am selben Tag und in wechselnder Reihenfolge mit B
  (z. B. A'–B–B–A'), Anzahl und Reihenfolge im Gate. Stufe-D-Läufe liefern Fehlerrate, Aufrufer, Fenstergrösse
  und Erwartungswerte, nicht die Tagesreferenz.
- **Arm B:** dieselbe Konfiguration mit der **einen** Änderung O1; nur nach Ownerfreigabe der
  Implementierung (noch nicht erteilt).
- **Gleiche Bedingungen:** Board, AP/Kanal (UART-Beleg), Treiber-/Capture-/Pcap-Hashes, vier frische
  Sessions mit dokumentiertem Alter, Idle-Referenz je Boot, Uhrzeitfenster dokumentiert.
- **Vergleichsfenster (fest, nur für relative Kennzahlen):** Lastsekunden `[0, T]`, `T = min(1200 s,
  kürzeste beobachtete Lastdauer der für den Vergleich verwendeten Läufe)`. Relative Kennzahlen
  (Latenz-Perzentile, Heap-/Blockverteilung, Abfall `built→sent`, Shell-Dauer, Drahtzähler) werden im
  Fenster und getrennt über den vollständigen Lauf berichtet; kein anderes Fenster nachträglich.
- **Harte B-Kriterien werden durch das Fenster nie relativiert:** Sie gelten für den **gesamten** B-Lauf
  (volle Last und Recovery gemäss Plan F1/F4 Abschnitt 7): jedes `heap_alloc_failed`, Panic/Watchdog/
  Brownout/Reset, HTTP-Nicht-2xx, jeder nicht abort-assoziierte Timeout oder Verbindungsfehler,
  fehlende Erholung, Stack unter Grenze. Ein B-Lauf, der früher endet als ein A'-Lauf, wird nicht
  mit einem verkürzten Fenster „gerettet“.
- **Bewertungsgrösse 1,25×:** p95 je Pfad und Shell-Gesamtdauer von B ≤ 1,25 × grösster p95 der
  zulässigen A'-Läufe im Fenster ist **Bewertungsgrösse, keine automatische FAIL-Grenze**; eine
  Überschreitung ist Befund für den Owner.
- **Aussagegrenze:** n ≤ 3 je Arm ist kein statistischer Nachweis; die A-Fehlerrate wird neben B
  ausgewiesen. F2-D5 (Restgefahrakzeptanz) ist **nicht** erteilt.

## 5a. Instrumentierung und Vergleichsgrenzen (Evidence-Grenze)

G1 IMAGE01 zeigte gegenüber C0-R3 im Idle ≈ 1,4 KB weniger freien Heap (41 132 B gegenüber 42 532 B bei gleichem
grösstem Block) und +1 452 B statisches DRAM gegenüber dem Produktbuild `d35ee85`. G1b nutzt **dieselbe
Firmware** wie G1; es entsteht **kein weiterer Firmware-Overhead**, und der Hook (+832 B Image, ≈ +112 B Stack
in der fehlschlagenden Task, +10,5 ms je Ereignis) ist darin bereits enthalten. Weder ein A/B-Vergleich
noch C0-R3 begründen ohne gleichartigen Vergleich eine **monokausale** Zuordnung des G1-Ausfalls zu
Instrumentierung, F4 oder Last. C0-R3 gilt in der Evidence **ausschliesslich als Vergleichsgrenze**
(anderes Image, anderer Tag); Aussagen über Ursachen stützen sich nur auf gleichartige Läufe desselben
Images.

## 6. Nicht Teil dieses Gates

Kein Flash, kein Reset ausser P1, P3 und den Port-Öffnungsresets R1–R3 (Abschnitt 3), keine Touch-/Hold-Sequenz, keine
Konfigurationsänderung (O1), kein P-A2/P-B/G2, kein Task-Tracking, keine lwIP-Statistik, kein
`CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS`. Jede dieser Handlungen braucht ein eigenes Gate.
