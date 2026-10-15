# G1b – R1b-Gate (Einzelwiederholung der Stufe D; Vorschlag zur Ownerfreigabe)

`G1B_R1B_AUTHORIZATION=PENDING` · `G1B_R1=HOLD` · `G1_IMAGE01=FAIL/HOLD` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`

Dieses Dokument gewährt keine Freigabe und nimmt kein GO vorweg. Es deckt **genau einen** Lauf **R1b** ab. R1 bleibt `HOLD`, auch wenn R1b gelingt. Bereits abgeschlossene R1-Aktionen (P1–P3, Testboot, Last) gelten **nicht** als Freigabe für R1b. R2/R3, Touch, Rollback, Flash und jeder Folgelauf sind ausgeschlossen. F2-D4: 1,25× p95 nur Bewertungsgrösse, kein Fail. F2-D5: keine Restgefahrakzeptanz.

## 1. Ausgangslage (R1) und was sie nicht beweist

R1 (`diag/review/g1b/R1_RESULT.md`, `d783f08f1f0e10c11b80ef636b8dcfe18ac35e61`): Das erste Login (POST, 8-s-Timeout) scheiterte bei Uptime ≈ 189 s **vor Lastbeginn**. Das Host-Pcap (gültig, payloadfrei) zeigt Handshake (SYN/ACK 55,6 ms), danach vom Host gesendete Login-Daten und **keine vom Host beobachtete** Bestätigung oder Antwort bis zum Timeout. Kein Heapfehler (41 264 B frei), kein Panic, kein unerwarteter Reset, Heartbeats normal. Das Ergebnis beweist weder eine WLAN-/lwIP-Root-Cause noch eine Wirkung von O1; das Pcap zeigt nur die Host-Schnittstelle, nicht die Zustellung durch das AP.

### 1.1 Vergleich der Host-/Netzumstände (nur vorhandene Evidence, offline)

| Umstand | R1 (2026-10-15) | G1 IMAGE01 (2026-10-14) | C0-R3 (2026-10-14) | Bewertung |
|---|---|---|---|---|
| Image/Quelle, Board, Host, Interface | G1 IMAGE01 `bb4f43a`, gleiches Board, `enp2s0` | gleich | C0-Image `9a9a8bd`, gleiches Board | vergleichbar |
| AP / Kanal / Bandbreite | gleicher AP, Kanal 1, BW20 | gleicher AP (BSSID-Vergleich privat), Kanal 1 | gleicher AP, Kanal 1 | gleich |
| RSSI beim Verbinden | −48 dBm | −44 dBm | −47 dBm | ähnlich; **kein** Laufzeitwert (nur beim Verbinden) |
| Wi-Fi-Powersave | `pm start, type: 1` | gleich | gleich | gleich |
| Tageszeit (lokal, Europe/Zurich) | ≈ 06:14 | ≈ 16:05 | ≈ 08:55 | **abweichend** (andere Funkumgebung möglich), nicht bewertbar |
| Zeitpunkt des ersten Logins nach Boot | Uptime ≈ 189 s | ≈ 355 s | ≈ 180 s | im Bereich von C0-R3 |
| Block-Ack-Verlauf bis zum Login | `tid 0` aufgebaut, `DELBA reason 39` bei 20,7 s, **kein** weiteres `ba-add` | `tid 0`, danach `tid 6` bei 32 s, `DELBA` bei 47 s | `tid 0`, `DELBA` bei 41 s | R1 ohne zweite Block-Ack-Sitzung; Bedeutung unbekannt |
| Vorangegangene Geräteaktion | esptool-Sitzung (Reset in Bootloader, Lesen, Hard-Reset) ≈ 38 s vor dem Testboot | Flash-/Readback-Sitzungen vorher | Flash vorher | in allen Läufen vorhanden |
| Neu in R1 | **parallel laufender tcpdump** auf dem Host (`-p`, nur Header) | – | – | Rückwirkung auf das Gerät nicht plausibel, aber der einzige Unterschied im Aufbau |
| Host-Socketzustände (`ss`) | während R1 leer | während Idle leer | – | kein Befund |

Offensichtliche Setup-Abweichungen: **keine** (Hashes, Skripte, Interface, Gerät unverändert). Nicht prüfbar mit vorhandener Evidence: Zustellung durch das AP, Funkumgebung zur Messzeit, ARP-/Nachbarzustand (nicht gefiltert). Es werden **keine** neuen Messinstrumente und keine zusätzlichen Geräteabfragen eingeführt.

## 2. Gepinnter Stand (alles unverändert)

| Gegenstand | Wert |
|---|---|
| Plan | `docs/tasks/issue-27-res01-f2-escalation-plan.md` @ `3bb29481495c4baca540ada1f3ae5433a5b46a34` |
| Host-Werkzeuge | Werkzeugcommit `07d1526678c171cf4e51a37650c221922d842f32` (Review-Branch `agent/pr203-res01-g1-review-nonmerge`), Manifest `diag/review/g1b/HOST_TOOLS_SHA256SUMS`; spätere Commits nur Doku (**keine** Werkzeugänderung für R1b, auch nicht für die Nachlauf-Lücke) |
| Image | App `da9e868874649bd274d21a4f1c5715afd9293288497027004c460efc46df8cf0` (1 760 560 B), ELF `24d8557f965053d566ba6ef3c4bfe745044a505994835e5831b85ff0dbe8c369`, Quelle `bb4f43a69740f234d26a951707991f792cc3ffde`; **kein Build, kein Flash** |
| Partitionstabelle | `d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d` |
| Lastprofil | unverändert P-A: vier Sessions, 60 s Idle, 1200 s Last, 300 s Recovery; Login-Timeout 8 s, übrige 3 s; harter Abbruch, kein Logout, kein Retry |
| Konfiguration | unverändert (keine O1-, F4-, WLAN-, HTTP-Änderung) |
| tcpdump | `/home/manuel/g1b-tools/tcpdump` (SHA-256 `0d426e25…16d2d2`, `cap_net_raw=ep`), nur über temporären `PATH` |
| Ausgabeordner (privat 0700) | `/var/lib/docker/data/ESP32-Projekte/hw-evidence-pr203/diag/g1b/run_20261015_r1b/` |
| Gate-ID | `PR203_RES01_G1B_R1B_20261015` |

## 3. Geräteaktionen (jede einzeln autorisierungspflichtig; nichts gilt aus R1 weiter)

| Nr. | Aktion | Wirkung |
|---|---|---|
| P1–P3 | **eine** esptool-Sitzung wie in `G1B_R1_GATE.md` Abschnitt 3 (Reset in Bootloader, ein `read-flash 0x8000 1793328`, Hard-Reset); Hashprüfung gegen Abschnitt 2; Readback sofort nach der Prüfung löschen | 2 Resets, nur lesend; Variante A (mit NVS-Bereich, private Datei) oder B (nur App) |
| Boot | `g1_capture.py` öffnet den Port (bekannter FT232R-Reset) | 1 Reset = erfasster Testboot R1b |
| Last | `START_LOAD` erst bei laufendem `PCAP_RUNNING` | P-A unverändert |

**Ownerwahl zu P1–P3:** (a) **ausführen** (konservativ: Identität unmittelbar vor R1b erneut belegt) oder (b) **ausdrücklich verzichten**. Verzicht stützt sich nur auf: R1 verifizierte App und Partitionstabelle am 2026-10-15 04:11 UTC; seitdem wurde weder ein Flash-Werkzeug noch esptool benutzt; die Quelle wird beim Testboot per UART geprüft. Der Verzicht ersetzt den App-Hash **nicht**; er ist eine bewusste Ownerentscheidung und ohne Wahl gilt (a).

Ablauf, Reihenfolge, Zeitgrenzen (Bereit ≤ 120 s, `PCAP_RUNNING` ≤ 30 s, Endsignal, Budget 1961 s) und Befehle unverändert wie `G1B_R1_GATE.md` Abschnitt 4 mit Gate-ID `PR203_RES01_G1B_R1B_20261015` und dem R1b-Ausgabeordner.

## 4. Stopbedingungen und Rückfall

Hard-Stop und HOLD bei der **ersten Anomalie** wie G1: `uart-anomaly` (inkl. `heap_alloc_failed`, Panic, Watchdog, Brownout), unerwarteter Reset, Heartbeat-Lücke > 5 s, Quellabweichung, Main-Stack < 11 160 B, HTTPD-Stack < 2 080 B, **Login- oder Request-Fehler** (kein Retry), Capture-/Pcap-Fehler, Skripthash ≠ Manifest, Ownerstopp. **Rückfall:** keiner nötig und keiner erlaubt (kein Schreibzugriff, kein Flash, kein Rollback); bei Identitäts- oder Hashabweichung HOLD ohne Reparatur.

## 5. Auswertungsregeln (vorab)

- **Login-Timeout ohne beobachtete TCP-Bestätigung wie R1:** keine weitere identische Wiederholung; stattdessen neue Ursachenbewertung des Empfangswegs mit vorhandener Evidence (Wi-Fi/AP-Zustellung, Powersave, Block-Ack, lwIP-Annahme) und neuer Plan-/Ownerentscheid.
- **Login gelingt:** Messung bis zur ersten Anomalie oder zum regulären Laufende gemäss genehmigtem Profil; R1b wird als **eigenständiger** Lauf nach Plan Abschnitt 4 ausgewertet (Aufrufer-Dekodierung gegen den gepinnten ELF, Drahtzähler mit `event_window`, Heapverlauf, `abort_induced`-getrennte Fehler). n bleibt klein, keine Fehlerrate, keine statistische Aussage.
- **R1 bleibt `HOLD`** unabhängig vom R1b-Ergebnis; Aussagen über das Zustellverhalten des AP/WLAN werden nicht aus dem Host-Pcap geschlossen.

## 6. FOLLOW-UP (dokumentiert, nicht umgesetzt)

**Nachlauf-Lücke bei `load-failure`:** In `g1_capture.py` verlässt der Abbruch durch den Lasttreiber die Schleife über einen eigenen `break` vor dem UART-Nachlaufcode (`uart_tail.log` fehlt). Für R1b **nicht** vorsorglich umgebaut: Die UART-Überwachung läuft bis zum Abbruch, und Pcap plus `load.json` genügen für die Entscheidung über diese eine Wiederholung. Nur wenn ein konkret fehlender Nachlauf zur Bewertung zwingend wird, wird ein separater kleinster Delta-Vorschlag vorgelegt.

## 7. Ownerbestätigungen (einzeln, vor Beginn)

| # | Bestätigung | Wert |
|---|---|---|
| 1 | Dieses Gate (Gate-ID, Werkzeugcommit, Hashes) für **genau einen** Lauf R1b freigeben | `PENDING` |
| 2 | Aktorfreier Devboard-Aufbau vor Ort bestätigt; Owner anwesend | `PENDING` |
| 3 | P1–P3: (a) ausführen oder (b) ausdrücklicher Verzicht; bei (a) Variante A oder B | `PENDING` |
| 4 | Resets akzeptiert: Testboot (und bei (a) zwei Resets in P1–P3) | `PENDING` |
| 5 | F2-D4: 1,25× p95 nur Bewertungsgrösse; F2-D5 offen | `PENDING` |
| 6 | Kein automatischer zweiter Versuch; R2/R3 gesperrt | `PENDING` |
