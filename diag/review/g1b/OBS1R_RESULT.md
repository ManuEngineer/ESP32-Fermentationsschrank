# G1b OBS1R – Beobachtungslauf (Image B): Klasse O-B, erster Last-Timeout bei 117,0 s, Stopp beim achten Timeout (130,8 s)

Freigabe: Owner, genau ein OBS1R-Lauf (`G1B_OBS1_RETRY_GATE.md` @ `6740085a6cdb11f69901be1e2d2fc3cefe3e02ba`, Werkzeugcommit `16d03498ac25f9e8fa9ce7b6101324438e60dd68`, Fix Verification PASS). Beisein, Aktortrennung und unveränderter Gerätezustand seit E-FB und dem ersten OBS1-Versuch vom Owner ausdrücklich bestätigt; H0 bestanden; Image B unverändert (`4145dddb…`, Quelle `31c3ee48…`). Ein Port-Öffnungsreset (Testboot 10:19:48 UTC), kein Flash/esptool. **Nach dem Abbruch keine weitere Geräteaktion.** Rohdaten und Pcap bleiben privat; Evidence unter `diag/review/g1b/stage_obs1r/`. `OBS1R=FAIL/HOLD` (Diagnosekategorie **O-B**; kein PASS, keine Recovery-/Abnahmeaussage) · `O1_QUALIFICATION=NOT_PROVEN` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`.

## Lauf

| Punkt | Befund |
|---|---|
| Bootmarker (neuer Parser) | akzeptiert: `POWERON_RESET` 0,071 s nach der Portöffnung, 2 Zeilen davor; danach Quelle `31c3ee48…`, `tcp tx win: 2880`, Release-Profil, Aktoren aus, `HomeConnected`, bereit nach 5 s |
| Reihenfolge | `PCAP_RUNNING` vorhanden, dann `START_LOAD` (10:20:02 UTC); Last ab Uptime ≥ 180 s |
| Login | 4 × HTTP 200 (3 137 / 3 098 / 3 057 / 3 110 ms) |
| Last bis zum ersten Fehler | **117,0 s fehlerfrei**; Latenz Minute 0: p50 156 ms, p95 231 ms, max 289 ms; Minute 1 (60–120 s): p50 295 ms, p95 2 195 ms, max 3 270 ms |
| **Beobachtung** | erster echter Last-Timeout: Start **117,00 s**, Ende 120,00 s, Phase `connect`; **acht echte Timeouts** (4 × `connect`, 4 × `io`, siehe `load_summary.json`) erreicht bei **130,82 s** ⇒ Stopp `observation-timeout-limit` nach **10,82 s** Beobachtung (innerhalb des 20-s-Fensters); danach 11 weitere in-flight-Timeouts als `after_stop` nur protokolliert; Endzustand `FAIL/HOLD` |
| UART | **kein** `heap_alloc_failed`, kein Panic/Watchdog/Brownout, nur der eine erwartete Resetmarker, Heartbeats ohne Lücke > 1,3 s bis 133,3 s; HTTPD-/Main-Stack in Grenzen |
| Pcap | `PASS`, 4 281 Pakete = tcpdump-Meldung, payloadfrei verifiziert, **bis Lastsekunde 133,4 vollständig** (auch über das Stoppsignal hinaus) |

## Messdaten der Stillstandsphase (belegt, nicht gedeutet)

- **Verlauf auf dem Draht (Gerätepakete je Sekunde, `wire_rates_per_second.csv`):** normal 13–18/s bis 114 s; **2/s bei 115–116 s**; 1–9/s bei 117–124 s; Nachholsalve **33/s bei 126 s**, danach 20/19/7/2/4/0/s (Ende der Beobachtung). Die Host-SYNs liefen mit 3–10/s weiter. Das Gerät war **nicht dunkel**, sondern etwa **10 s lang stark gedrosselt**.
- **Beide Seiten der Verbindung verzögert:** SYN/ACK-Verzögerung bis **5,0 s** (Median insgesamt 98 ms), **11 Verbindungen ohne SYN/ACK**, erste Antwort bis 3,4 s; eine 339-B-API-Antwort (Verbindung 297) brauchte **6,8 s** (119,5 → 126,3 s) mit Geräte-Retransmits. Insgesamt 58 SYN-Wiederholungen, 5 Geräte-Retransmits, 15 Dupacks des Hosts, 0 Zero-Window, 0 Geräte-RST.
- **Kein Shell-Versand beim Einsetzen:** `res01_http` (protokolliert nur die Shell-Handler) zeigt **keine** Shell-Antwort zwischen Lastsekunde 100,8 und 127,8; die ersten verzögerten Antworten (ab 115,6 s: erste Daten nach 1,4–2,0 s) sind kleine API-Antworten (111–339 B). Der Beginn der Stillstandsphase ist damit **nicht** durch eine laufende Shell-Sendephase (Head-of-Line) erklärt.
- **Heap beim Einsetzen gesund:** grösster Block (`gate_block`) **38 912 B bei 115,2 s**, danach fallend während der Stillstandsphase: 30 720 (117,2 s) → 23 552 → 19 456 → 14 848 (123,3 s) → 10 240 (127,3 s) → 7 680 B (129,3 s); `skips` 215 → 772. Die Speicherabnahme **folgt** dem Beginn der Stillstandsphase. Kein Allokationsfehler.
- **Gerätelog:** nur `httpd_sock_err … error in recv : 104` bei 121,2 und 124,1 s (Clientabbruch nach Timeout) und `ui_skipped_low_memory`; Handler-Sendephasen der 16 Shell-Tripel: Median 0,05 s, Maximum 2,11 s.

## Einordnung und Grenzen

- **Klasse O-B:** echte Netzwerk-Timeouts, Stopp durch die Obergrenze; Auswertung der Stillstandsphase statt Abnahme. **Kein** automatischer Schluss auf Backlog-Sättigung und **keine** Root-Cause-Aussage.
- **Was die Daten stützen:** Die Stillstandsphase beginnt (a) bei gesundem Heap, (b) ohne laufende Shell-Antwort, (c) bei weiterlaufendem Heartbeat/Hauptloop, und verzögert **sowohl** die SYN/ACK-Erzeugung (lwIP) **als auch** Antworten bestehender Verbindungen. Das ist **mit** einer Störung auf Funk-/Wi-Fi-/lwIP-Ebene vereinbar und **gegen** Head-of-Line durch die Shell und gegen Speichermangel als **Auslöser**; der Heapverbrauch und die spätere Sättigung der Server-Sitzungen sind Folgen bzw. Verstärker. **Nicht belegt:** ob die Verzögerung im Access Point, im Funkpfad, im Wi-Fi-Treiber oder im lwIP-Task entsteht (das Host-Pcap zeigt nur die Host-Schnittstelle).
- **Erholung:** In den 10,8 s Beobachtung zeigte sich keine stabile Erholung (kurze Nachholsalve bei 126 s, danach erneute Verzögerung); eine Erholung über das Fenster hinaus ist **nicht** beobachtet.
- **Kontext (nicht vergleichend, verschiedene Tage; n klein):** Zeit bis zum ersten Fehler B: 761,2 s (B1), 100,7 s (B2), **117,0 s** (OBS1R); A: 311,9 s (G1), 339,9 s (R1b). Keine stabile Besserung durch O1 erkennbar, kein O1-Schaden belegt; Zuordnung nicht möglich.
- Volumenlimits nicht erreicht; bekannte Werkzeuglücke (kein UART-Nachlauf bei `load-failure`) unverändert; kein weiterer Lauf, kein Rückflash, keine O2-/O3-Umsetzung; F2-D4 Bewertungsgrösse, F2-D5 offen.
