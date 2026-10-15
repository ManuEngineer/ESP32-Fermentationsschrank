# G1b R1 – Ergebnis (2026-10-15): HOLD vor Lastbeginn (`load-failure`, erstes Login ohne TCP-Antwort)

Gate `PR203_RES01_G1B_R1_20261015`, Werkzeugcommit `07d1526678c171cf4e51a37650c221922d842f32`, Image unverändert G1 IMAGE01. Genau ein Lauf; **kein zweiter Versuch, keine weitere Geräteaktion**. Pcap, Readbacks, Passwort und vollständige UART-Rohdaten bleiben privat; hier nur abgeleitete Angaben. `G1B_R1=HOLD` · `G1_IMAGE01=FAIL/HOLD` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO` · R2/R3 gesperrt.

## Preflight P1–P3 (PASS)

Eine esptool-v5.3.1-Sitzung 04:08:37–04:11:22 UTC (Reset in den Bootloader, `read-flash 0x8000 1793328` nur lesend, Hard-Reset): ESP32-D0WD-V3 rev 3.1, MAC `20:50:0d:1b:2f:34`; Partitionstabelle `d7f180e4…ccd9d` und App `da9e8868…df8cf0` stimmen exakt mit dem Gate überein. Variante A: die Readback-Datei (enthielt `0x9000–0xFFFF`) wurde nach der Hashprüfung gelöscht, nur die Hashes bleiben.

## Lauf R1

| Punkt | Befund |
|---|---|
| Testboot | Port-Öffnung 04:11:49 UTC, `POWERON_RESET`; Quelle `bb4f43a…`, Release-Profil, `real actuators: disabled`, `application: ready`, `HomeConnected`; Bereit nach ≈ 3 s |
| Reihenfolge | `g1_pcap capture` gestartet, `PCAP_RUNNING` vorhanden, **dann** `START_LOAD` (04:12:09 UTC); Last startet bei Uptime ≥ 180 s |
| Abbruch | 04:14:52–04:15:00 UTC, Uptime ≈ 189–191 s: **erstes Login (POST) überschritt das 8-s-Sockettimeout** (`milliseconds` 8065,8; `error_class timeout`, `error_phase io`, `abort_induced false`); `load.json`: `abort = request-failure`, 1 Sample, keine Lastanfrage; `capture.json`: `failure = load-failure`, `status FAIL/HOLD`, `max_heartbeat_gap_seconds` 3,51, ein erwarteter Bootreset |
| UART/Heap | keine `heap_alloc_failed`-, `diag_alloc_fail`-, Panic-, Watchdog-Zeile; `diag_30s` bei Uptime 182 s: frei 41 264 B, Minimum 32 420 B, grösster Block 38 912 B, Main-Stack 11 688 B |
| Mitschnitt | `pcap_result.json`: **PASS**, Exitcode 0, 9 Pakete = tcpdump-Meldung, payloadfrei verifiziert, tcpdump beendet; Tool-Hash = Manifest |

## Drahtbefund (Q2, abgeleitet aus `analyze`, relative Zeiten)

Eine Verbindung: SYN, **SYN/ACK nach 55,6 ms**, Handshake-ACK, danach zwei Datensegmente des Hosts (178 B + 31 B = Login-POST) bei 0,056 s. **Das Gerät sendet danach kein einziges Paket mehr** (keine ACKs, keine Antwort, kein RST, Zero-Window 0). Der Host wiederholt das 209-B-Segment bei 0,32 / 0,86 / 1,92 / 4,10 s, dann Abbruch durch den 8-s-Timeout. Es gibt damit **kein Anwendungs-, sondern ein TCP-Stillstandsbild**: Verbindung bei lwIP aufgebaut, eingehende Daten bleiben ≥ 4 s unbestätigt, obwohl Heap und Heartbeat normal sind. Ursache `NOT_PROVEN` (z. B. Wi-Fi-Empfang, `tcpip`-Task, Annahmewarteschlange, Funk).

## Befunde zu den Werkzeugen (nicht verändert)

- Beim Abbruchgrund `load-failure` läuft der UART-Nachlauf **nicht**: dieser Abbruch verlässt die Schleife über einen eigenen `break` vor dem Nachlaufcode (`uart_tail.log` fehlt). Im Lauf unkritisch (Gerät gesund), aber eine Lücke gegenüber Testblatt Abschnitt 2.
- Der Lasttreiber unterscheidet nicht, ob ein Login-Timeout vor oder nach dem TCP-Verbindungsaufbau auftrat; der Pcap liefert das.

## Offene Entscheidungen (Owner)

Ein Wiederholungslauf wäre ein **neues Gate** (R1 ist verbraucht). Diagnose-/Fix-Optionen stehen im Bericht an den Owner; keine automatische Fortsetzung.
