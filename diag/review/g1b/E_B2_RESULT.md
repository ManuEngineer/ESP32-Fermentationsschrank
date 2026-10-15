# G1b Stufe E – B2 (einmalige Wiederholung mit Image B): `load-failure` bei Lastsekunde 100,7 → STOP/HOLD

Freigabe: Owner, genau ein Lauf B2 (`G1B_B2_GATE.md` @ `f69ae0891728e26def31b1c1d1de6a63cf4d9c16`, Gate-Review PASS). Image B unverändert installiert (`4145dddb…`, Quelle `31c3ee48…`), Werkzeugcommit `07d1526…` (H0 bestanden), seit E-FB (06:52:30 UTC) keine esptool-/Flash-Aktion. Ein Port-Öffnungsreset (Testboot 08:59:45 UTC), kein Flash, kein Rückflash. Nach dem automatischen Abbruch **keine** weitere Geräteaktion; B bleibt installiert. Rohdaten und Pcap bleiben privat; Evidence unter `diag/review/g1b/stage_e_b2/`. `E_B2=FAIL/HOLD` (harte Kriterien) · `B2_HARDWARE=DONE_HOLD` · `O1_QUALIFICATION=NOT_PROVEN` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`.

**Hinweis zum Abbruchverhalten:** Während des Laufs hat der Owner gewünscht, nicht bei der ersten Anomalie zu stoppen. Das gilt für die freigegebenen Werkzeuge nicht (automatischer Hard-Stop, Gate Abschnitt 5, „uneingeschränkt“); es wurde nichts verändert. Der Beobachtungsmodus wäre ein neues Gate mit Werkzeugänderung.

## Lauf

| Punkt | Befund |
|---|---|
| Testboot | `POWERON_RESET`; UART: Quelle `31c3ee4…`, `tcp tx win: 2880`, Release-Profil, Aktoren aus, `HomeConnected`; bereit nach 5 s |
| Reihenfolge | `PCAP_RUNNING` vorhanden (08:59:59 UTC), dann `START_LOAD`; Last ab Uptime ≥ 180 s |
| Login | 4 × HTTP 200 (3 110 / 3 080 / 3 085 / 3 116 ms) |
| Last | **100,7 s von 1200 s**; 246 Lastanfragen HTTP 200 (plus 4 Logins) bis zum Fehler; Recovery nicht ausgeführt |
| Abbruch | `load.json`: `abort = request-failure`, `capture.json`: `failure = load-failure`, `FAIL/HOLD` (Lasttreiber, nicht UART-Anomalie) |
| UART | **kein** `heap_alloc_failed`/`diag_alloc_fail`, kein Panic/Watchdog/Brownout, kein weiterer Reset; keine Heartbeat-Lücke > 1,3 s; HTTPD-Stack min 2 392 B, Main-Stack 11 688 B (Grenzen eingehalten) |
| Pcap | `PASS`, 3 392 Pakete = tcpdump-Meldung, payloadfrei verifiziert, bis Lastsekunde 101,1 vollständig |

## Der erste Fehler

**Erster echter Fehler: Lese-Timeout (`timeout`, `error_phase io`, `abort_induced false`)** einer Shell-Anfrage (Start Lastsekunde 97,0; Fehler bei 100,70). Es ist **kein** Verbindungsaufbau-Timeout. Die Verbindung wurde normal aufgebaut (SYN/ACK nach 0,12 s), die Anfrage kam an (97,20 s), das Gerät begann zu antworten (97,41 s) und lieferte **7 200 B in 6 Segmenten bis 97,95 s – dann nichts mehr** (die Shell-Antwort umfasst ≈ 8 155 B); der Client wartete bis zum Abbruch. Eine zeitgleiche API-Anfrage (Verbindung 247, Start 97,0) bekam die erste Antwort erst bei 98,02 s und die letzte (nach einem Geräte-Retransmit) bei 101,01 s. Die übrigen fünf Fehler sind abort-assoziiert (`abort_induced true`). Danach gestarteten vier Verbindungen (100,6 s) fehlt das SYN/ACK teilweise (0,1 s vor dem Abbruch, nicht auswertbar).

- **Einordnung nach Gate Abschnitt 6: D (andere Anomalie)**, nicht B: Der erste Fehler ist ein Lese-Timeout mitten in einer Antwort, kein Verbindungsaufbau-Timeout. Es gab auch keinen Heap-Allokationsfehler (nicht C). Aus der Fehlerart folgt **kein** Nachweis einer Backlog-Sättigung: Beim Fehler waren aus Host-Sicht nur wenige Verbindungen offen (Verbindungstabelle: 246–251 parallel angenommen, keine unbeantworteten SYNs vor 100,6 s).
- **Geräteseitig (UART):** Heap bis zu diesem Punkt unkritisch: Minimum 8 520 B bei Lastsekunde 99,5 (grösster Block 9 216 B), `free(built)` min 17 196 B, kein Allokationsfehler; Handler-Sendephasen der Shell (device-seitig `built→sent`) Median 0,05 s, Maximum 0,60 s (alle < 1 s, 14 Tripel). Der F4-Gate-Zähler lief an (`skips` 98 → 193 in 1 s, `sample_age_ms` 1 040 → 2 049 ms): die Gate-Messung wurde in den letzten zwei Sekunden nicht erneuert, während Heartbeats weiterliefen.
- **Draht (Host-Schnittstelle, 3 392 Pakete):** 260 Verbindungen, 1 Geräte-Retransmit, 2 Dupacks, 1 SYN-Wiederholung, 0 Zero-Window, 0 Geräte-RST, SYN/ACK-Median 83,5 ms (Maximum 1 147 ms), erste Antwort Median 73,8 ms. Das Gerät sendete in der Stillstandsphase weiter, einzelne Verbindungen blieben aber ohne vollständige Antwort.

## Latenz vor dem Fehler

Minute 0: p50 156 ms, p95 248 ms, max 322 ms; Minute 1 bis 100,7 s: p50 200 ms, p95 1 295 ms, **max 3 695 ms** (2 Anfragen > 3 s: die beiden Fehler selbst, 3 694/3 695 ms nach Abbruchzeit-Messung).

## Bewertung gegen die vorab festgelegten Kriterien (Gate Abschnitt 5 und 6)

| Kriterium | B2 |
|---|---|
| Kein `heap_alloc_failed`/Abort/Panic/Watchdog/Brownout/Reset | erfüllt im beobachteten Zeitraum (100,7 s) |
| HTTP 100 % 2xx, keine Timeouts/Verbindungsabbrüche | **verletzt** (Lese-Timeout einer Shell-Antwort) |
| Voller Lauf und Erholung | **nicht erreicht** |
| Stack-HWM, F4 | eingehalten / Gate aktiv (Befund) |

Ergebnisklasse **D**; keine Wiederholung, kein Rückflash, keine Automatik.

## Einordnung gegenüber den bisherigen Läufen (Kontext, ausdrücklich nicht vergleichend: verschiedene Tage und Tageszeiten, n klein)

| Lauf | Image | Zeit bis zum ersten Fehler (Last) | erste Anomalie |
|---|---|---|---|
| G1 | A | 311,9 s | `heap_alloc_failed` (1626 B / 0x80c, Frame abgeschnitten) |
| R1b | A | 339,9 s | `heap_alloc_failed` (1696 B, Wi-Fi-RX, dekodiert) |
| B1 | B | 761,2 s | Verbindungsaufbau-Timeouts (SYN unbeantwortet) |
| **B2** | **B** | **100,7 s** | **Lese-Timeout mitten in einer Shell-Antwort** |
| C0-R3 | F1-4 ohne Overlay/F4 | 1200 s vollständig | keine |

**Streuung:** Bei B liegen die Zeiten bis zum Fehler zwischen 100,7 und 761,2 s, bei A zwischen 311,9 und 339,9 s; die Fehlerarten wechseln (Allokation, Verbindungsaufbau, Lese-Timeout). B2 liefert **keinen** Hinweis auf eine stabile Verbesserung durch O1 und keinen Hinweis auf einen O1-Schaden (im Stationärbetrieb unverändert); eine Zuordnung ist mit diesen Läufen nicht möglich. Gemeinsam ist allen Fehlläufen eine **Stillstandsphase** von ein bis drei Sekunden (verzögerte oder ausbleibende Antworten bei weiterlaufenden Heartbeats), deren Ursache offen ist.

## Offene Punkte

- **Ursache der Stillstandsphase `NOT_PROVEN`** (Wi-Fi-Zustellung, Powersave, Wi-Fi-Pufferengpass, Server-Handler, Funkumgebung). B2 erlaubt keine Aussage zu Backlog-Sättigung.
- **Datenmenge:** Der automatische Hard-Stop beendet jeden Lauf in oder kurz nach der ersten Stillstandsphase; für Dauer, Wiederholung und Erholung der Stillstandsphase liegen keine Messdaten vor. Dafür wäre ein neues Gate mit **Beobachtungsmodus** (nur Reset/Panic/Watchdog/Brownout/Heartbeat-Lücke/Stack als harter Stopp, Heap- und Request-Fehler protokollieren, Aufzeichnungsobergrenze, Schwelle für „wiederholt“) und kleiner Werkzeugänderung nötig; Owner-Schwellen noch offen.
- **Kein dritter identischer Lauf** ohne neue Ursachenbewertung und Ownerentscheidung (Gate Abschnitt 6). Werkzeuglücke bei `load-failure` (kein UART-Nachlauf) unverändert.
