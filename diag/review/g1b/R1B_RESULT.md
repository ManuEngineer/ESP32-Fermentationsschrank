# G1b R1b – Ergebnis (2026-10-15): erste Anomalie `heap_alloc_failed` bei Lastsekunde 339,8 → STOP/HOLD

Gate `PR203_RES01_G1B_R1B_20261015` (Ownerfreigabe GO auf `348f0edb6d1225651af65a3f629457e324af286d`: genau ein Lauf, P1–P3 durch Variante b ausdrücklich verzichtet, Testboot-Reset genehmigt, aktorfreier Aufbau und Anwesenheit vom Owner bestätigt, F2-D4 1,25× nur Bewertungsgrösse, F2-D5 offen, R2/R3 gesperrt). Unverändert: Image G1 IMAGE01 (App `da9e8868…df8cf0`), Werkzeugcommit `07d1526678c171cf4e51a37650c221922d842f32` (Manifest OK, sauberer Baum), Lastprofil P-A, Konfiguration. Nach der Anomalie **keine weitere Geräteaktion**. Rohdaten, Pcap und vollständige UART-Aufzeichnung bleiben privat; Evidence unter `diag/review/g1b/r1b/`. `G1B_R1B=HOLD` · `G1B_R1=HOLD` · `G1_IMAGE01=FAIL/HOLD` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`.

**Vorbedingung des Owners (Punkt 2):** Vor dem Start bestätigt: seit der Hashprüfung von R1 (esptool-Sitzung 04:08:37–04:11:22 UTC, App und Partitionstabelle exakt) gab es in dieser Arbeitssitzung keine Flash-Schreibhandlung, kein esptool und kein Flash-Werkzeug; die einzige Geräteaktion war der Port-Öffnungsreset des R1-Testboots. Änderungen durch andere sind nicht beobachtbar; der UART-Boot zeigte Quelle `bb4f43a…` und `real actuators: disabled`.

## Lauf

| Punkt | Befund |
|---|---|
| Testboot | Port-Öffnung 04:39:39 UTC, `POWERON_RESET`, Quelle `bb4f43a…`, Release-Profil, Aktoren aus, `HomeConnected`, bereit nach 5 s |
| Reihenfolge | `PCAP_RUNNING` vorhanden, dann `START_LOAD` 04:39:52 UTC; Last ab Uptime ≥ 180 s |
| Login | **gelungen**: 4 Logins HTTP 200 (3 108 / 4 192 / 3 143 / 3 047 ms). Anders als R1; Ursache des R1-Ausfalls bleibt unbewiesen, kein Nachweis der Reproduzierbarkeit |
| Last | 859 × HTTP 200 (863 Samples gesamt), **339,9 s von 1200 s**; Latenz je Minute p50 175 / 390 / 360 / 400 / 399 / 377 ms, p95 bis 1 368 ms, Maximum 2 715 ms, **keine Anfrage > 3 s**, kein Timeout |
| Abbruch | `capture.json`: `failure = uart-anomaly`; `load.json`: `abort = g1-supervisor-stop`; vier `http-protocol-error` sind **abort-assoziiert** (`abort_induced = true`, ein Socket des Guards erfolgreich beendet), keine eigenständigen Fehler |
| Resets/Stack/Heartbeat | ein erwarteter Bootreset, kein weiterer; größte Heartbeat-Lücke 3,50 s; Main-Stack 11 688 B, HTTPD-Stack min. 2 368 B; kein Panic, kein Watchdog |
| Nachlauf | **funktionierte** (5,019 s, 3 969 B, `uart_tail.log`); Gerät lief weiter (Heartbeats, Gate-Zeilen) |

## Die Anomalie (Q1: Aufrufer jetzt dekodiert)

Erste Meldung (Lastsekunde 339,8, 04:49:39,114 UTC): `heap_alloc_failed size=1696 caps=0x00001800 function=heap_caps_malloc`, dazu `diag_alloc_fail`-Backtrace (`isr=0`, `core=0`), dekodiert gegen den gepinnten ELF (`24d8557f…`, Toolchain `xtensa-esp-elf` 15.2.0, `addr2line`):

`wDev_ProcessRxSucData → wDev_IndicateFrame → esf_buf_alloc → esf_buf_alloc_dynamic → wifi_malloc (esp_wifi/esp32/esp_adapter.c:82) → malloc → heap_caps_malloc_default → heap_caps_malloc → heap_caps_alloc_failed`

= **Wi-Fi-Treiber, dynamischer RX-Puffer (1 696 B)**, Task-Kontext, nicht Anwendungs-, HTTP- oder lwIP-Code. Derselbe Pfad wie im D3-Diagnoselauf. Es gab zusätzlich 11 weitere identische 1 696-B-Fehler innerhalb ≈ 0,25 s (im Nachlauf), danach Erholung: `gate_block` wieder 38 912 B bei Uptime 601 s (≈ 1,5 s später). **Die 1 626-B-/`caps=0x80c`-Signatur aus G1 trat in R1b nicht auf**; ihr Aufrufer bleibt `NOT_PROVEN` (G1-Frame abgeschnitten, hier nicht wiedergegeben). Die Erholung ist **nicht** dem Gerät zuzuschreiben: Der Supervisor hatte die Client-Sockets geschlossen, was Sendepuffer freigibt.

## Heap vor der Anomalie (Phasenstichproben der Shell-Antworten, `res01_http_phases.csv`)

- Zweite Shell-Antwort B (Lastsekunde 339,8): `pre` 15 484 B frei / grösster Block 9 728 B, `built` **15 248 B**; unmittelbar zuvor Shell A: `sent` 22 412 B / 15 872 B. Kurz darauf der RX-Fehler (≤ ≈ 50 ms-Lesefenster).
- Abfall `built→sent` (46 vollständige Tripel): Median 8 028 B, p90 13 272 B, **Maximum 16 884 B**. Kleinstes `free(built)` der vollständigen Tripel 19 732 B; das letzte (unvollständige) `built` mit 15 248 B liegt ≈ 4,5 KB darunter. **Spielraum = 15 248 − 16 884 = −1 636 B** (G1: −1 740 B; C0-R3: +716 B). Modellrechnung aus Phasenstichproben, kein Pufferzähler.
- Boot-Minimum bei Lastsekunde 71: 6 996 B (grösster Block 4 096 B) – früh überlebt, ohne Fehler. Gate: 595 Skips / 11 Resumes bis zum Ende.

## Drahtbefund (Q2; Pcap `PASS`, 10 922 Pakete = tcpdump-Meldung, payloadfrei verifiziert; nur Host-Schnittstelle)

853 Verbindungen; 1 941 Geräte-Datensegmente; **4 Geräte-Retransmits, 10 Dupacks des Hosts, 20 SYN-Wiederholungen**, 0 Zero-Window, 0 RST; SYN/ACK-Verzögerung Median 138,7 ms (Maximum 1 598 ms), erste Antwort Median 107,7 ms (Maximum 3 004 ms). Wiederholungs-Cluster treten mehrfach auf (z. B. um 220–230 s, 280–290 s, 410–420 s seit Mitschnittbeginn) und wurden überlebt; das letzte Cluster ist **nicht** einzigartig. **Abdeckungsgrenze:** Der Mitschnitt endet mit dem letzten Paket 04:49:38,514 UTC, **0,6 s vor der Ereigniszeit**, und enthält die SYNs der drei zuletzt gestarteten Requests (Start 04:49:38,694) **nicht**, obwohl Hostuhr und Pcap auf 0,2 ms übereinstimmen und tcpdump bis ≈ 04:49:39,28 lief. Ursache unbekannt (mögliche Kernel-Drops am Ende; Drop-Zähler wurde nicht gespeichert). Folge: `event_window` meldet `covered=false`; das Drahtbild gilt nur **bis 0,6 s vor dem Ereignis**. Die vom Werkzeug geprüfte Gleichheit „verifizierte = gemeldete Pakete“ erkennt solche Endverluste nicht.

## Stufe-D-Entscheidungsregel (Plan Abschnitt 4), mechanisch angewandt – **keine Entscheidung**

Der Aufrufer ist ein Wi-Fi-Task-Pfad (`esf_buf_alloc`), und die Drahtseite zeigt Retransmits/Verzögerungen. Nach der vorab festgelegten Regel würde Stufe E (O1) dem Owner zur Freigabe vorgelegt. Einschränkungen, die der Owner mitbewerten muss: (1) n = 1 dekodiertes Ereignis (RX, 1 696 B), die G1-Signatur 1 626 B bleibt unaufgelöst; (2) der Fehler liegt im RX-Pfad des Treibers während des Sendens der zweiten Shell-Antwort, das Speicherdefizit stammt aus der Summe aller Verbraucher (nicht zerlegt); (3) Drahtbefund endet 0,6 s vor dem Ereignis; (4) R1 (Login ohne Antwort) und R1b (Login ok) zeigen unterschiedliches Verhalten ohne geklärte Ursache; (5) keine Fehlerrate, keine statistische Aussage; (6) F2-D5 offen.

## Befunde zu den Werkzeugen (unverändert gelassen)

- UART-Nachlauf bei `uart-anomaly` funktioniert wie spezifiziert (bei `load-failure` weiterhin die bekannte Lücke).
- Der Pcap-Prüfer kann einen unvollständigen Mitschnitt-Endabschnitt nicht erkennen; ein Vergleich der letzten Pcap-Zeit mit der letzten Client-Sample-Zeit würde es zeigen.

## Evidence und Hashes

`diag/review/g1b/r1b/`: `uart_tail.log` (Nachlauf, ohne Netzwerkdaten), `res01_http_phases.csv`, `g1_gate_series.csv`, `load_summary.json`, `capture.json`, `pcap_result.json`, `pcap_analysis_derived.json` (UTC-Anker und Bin-Grenzen, keine Adressen/Ports), `PRIVATE_SOURCE_SHA256.json` (Hashes der privaten Quellen). Nicht veröffentlicht: Pcap, vollständige UART-Rohdaten, Passwort, Cookies.
