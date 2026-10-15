# G1b Stufe E – E-B1 (Pilotlauf mit Image B): `load-failure` bei Lastsekunde 761,2 → STOP/HOLD

Freigabe: Owner, ausschliesslich E-B1 (genau ein P-A-Pilotlauf mit dem installierten Image B, `G1B_STAGE_E_GATE.md` @ `188ddfc292498d35b456cb39c26103613959efda`; aufbauend auf `E_FB=PASS`, `94744879d56d2afeabc6c37fecc70c3f57934d32`). Image B `4145dddb…7b68`, Quelle `31c3ee48b52f44852b2e5cd5f542ba2fce614537`. Vorab geprüft: Review-Branch sauber, Werkzeug-Manifest und Artefakt-Hashes OK, seit E-FB (06:52:30 UTC) keine esptool-Sitzung. Werkzeugcommit `07d1526…` unverändert. Ein Port-Öffnungsreset (Testboot), sonst keine Geräteaktion; nach dem Abbruch **kein** Retry, Reset, Flash, Rückflash oder Folgelauf. Das Gerät behält Image B. Rohdaten und Pcap bleiben privat; Evidence unter `diag/review/g1b/stage_e_b1/`. **Das Ergebnis ist n = 1 ohne gleichtägige A-Referenz: kein O1-GO, keine Wirksamkeitsaussage.** `E_B1=FAIL/HOLD` (harte Kriterien) · `F2_HARDWARE=PENDING` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`.

## Lauf

| Punkt | Befund |
|---|---|
| Testboot | 07:54:12 UTC, `POWERON_RESET`; UART: Quelle `31c3ee4…`, `tcp tx win: 2880` (O1 auf dem Gerät wirksam), Release-Profil, Aktoren aus, `HomeConnected`; bereit nach 5 s |
| Reihenfolge | `PCAP_RUNNING` vorhanden, dann `START_LOAD` (07:54:24 UTC); Last ab Uptime ≥ 180 s |
| Login | 4 × HTTP 200 (3 035 / 3 103 / 3 123 / 3 079 ms) |
| Last | **761,2 s von 1200 s**, 1 915 Lastanfragen HTTP 200 (plus 4 Logins) **ohne** Fehler bis Lastsekunde ≈ 758; Recovery nicht ausgeführt |
| Abbruch | `load.json`: `abort = request-failure`, `capture.json`: `failure = load-failure`, `FAIL/HOLD`; Stopp durch den Lasttreiber, nicht durch eine UART-Anomalie |
| UART | **kein** `heap_alloc_failed`, kein `diag_alloc_fail`, kein Panic/Watchdog/Brownout, kein weiterer Reset; Heartbeat-Lücke max. 3,55 s (Zeit bis zum ersten Heartbeat nach dem Boot), **keine Lücke > 1,6 s** im ganzen Lauf; Main-Stack min 11 688 B, HTTPD-Stack min 2 304 B (Grenzen 11 160 / 2 080 B eingehalten) |
| Pcap | `PASS`, 24 860 Pakete = tcpdump-Meldung, payloadfrei verifiziert; letztes Paket bei Lastsekunde 762,8 (**Abdeckung bis zum Abbruch vollständig**) |

## Der Fehler (erste Auffälligkeit)

Die ersten echten Fehler sind **Verbindungsaufbau-Timeouts** (`timeout`, `error_phase connect`, 3 s Sockettimeout, `abort_induced false`): Requests `alerts` und `status`, Start Lastsekunde 758,2, danach weitere bei 760,6. Die übrigen Fehler (`http-protocol-error`, `io`) sind abort-assoziiert (`abort_induced true`, auf Sockets, die der Guard beendete). Insgesamt 10 fehlgeschlagene Anfragen, davon 4 Verbindungs-Timeouts.

- **Vorlauf:** Latenz je Minute p50 166 / 222 / 325 / 305 / 264 / 258 / 266 / 252 / 261 / 279 / 278 / 263 ms (Minuten 0–11), p95 max. 1 204 ms, **keine Anfrage > 3 s bis Minute 12**; erst ab Lastsekunde ≈ 750 häufen sich Antworten von 1,5–4,4 s (Minute 12: 11 Anfragen > 3 s, Maximum 4 436 ms).
- **Heap (Phasenstichproben der Shell-Antworten, 103 vollständige Tripel):** Abfall `built→sent` Median 2 072 B, p90 9 864 B, **Maximum 13 784 B**; kleinstes `free(built)` 15 900 B; Boot-Minimum bis Lastsekunde 131: 12 304 B, **danach 10 420 B (759,8 s) und 5 468 B (762,1 s, grösster Block 4 608 B)**. Gate: `gate_block` 3 200 B und 7 680 B bei `sample_age_ms` 2 350–3 359 ms in den letzten Sekunden. **Trotzdem kein einziger gemeldeter Allokationsfehler** (der Hook hätte jeden protokolliert; UART bis Lastsekunde ≈ 763,6 gelesen).
- **Draht (nur Host-Schnittstelle):** 1 820 Verbindungen, 4 576 Geräte-Datensegmente, 11 Geräte-Retransmits, 18 Dupacks, **142 SYN-Wiederholungen**, 12 Host-RST (abort-bedingt), 0 Zero-Window; **drei Verbindungen ohne SYN/ACK** (die Verbindungs-Timeouts); SYN/ACK-Verzögerung Median 116 ms (Maximum 3 183 ms). `event_window` um 758,2 s (`covered=true`): Wiederholungsaktivität im Fenster der letzten 10–20 s deutlich erhöht (Bin 754–764 s: 15 SYN-Wiederholungen, 4 Dupacks). Das Gerät **sendete weiter** (9–17 Pakete/s zwischen 758 und 762 s, kurze Lücken bei 741, 746, 750–752, 756–757 s), war also nicht dunkel; es beantwortete einzelne neue Verbindungen nicht.

## Bewertung gegen die vorab festgelegten Kriterien (Gate Abschnitt 5; Plan F1/F4 Abschnitt 7)

| Kriterium | B1 |
|---|---|
| Kein `heap_alloc_failed`/Abort/Panic/Watchdog/Brownout/Reset | **erfüllt im beobachteten Zeitraum (761 s)** |
| HTTP 100 % 2xx, **keine Timeouts/Verbindungsabbrüche** | **verletzt** (4 echte Verbindungs-Timeouts) |
| Voller Lauf (1200 s) und Erholung (300 s) | **nicht erreicht**, Erholung nicht messbar |
| Stack-HWM, F4-Verhalten | eingehalten / Gate aktiv (Befund, kein Fail) |

Damit gilt B1 nach den harten Kriterien als **FAIL/HOLD**, obwohl die zuvor bei A beobachtete Allokationsstörung des Wi-Fi-RX-Puffers in diesem Lauf nicht auftrat. Es gibt keinen automatischen Wechsel auf O2/O3 und keinen Rückflash.

## Kontext (nicht vergleichend, nur Einordnung; verschiedene Tage/Tageszeiten, B1 gegen frühe A-Fail-Läufe)

Lastsekunden 0–311, nur erfolgreiche Requests: p50 / p95 aller Pfade **B1 254 / 663 ms** (C0-R3 247 / 737, G1 290 / 1 137, R1b 359 / 989); Shell p50 / p95 B1 538 / 1 397 ms (G1 585 / 1 907, R1b 746 / 1 116, C0-R3 460 / 1 968); Anfragen > 3 s: B1 0, G1 6, R1b 0, C0-R3 0. **Kein Hinweis auf eine systematische Verlangsamung der Shell durch die kleinere Sendewarteschlange im Anfangsfenster**; ein Vergleich nach Testblatt Abschnitt 5 (gleichtägige A'-Referenzen, Fenster `T = min(…)`) liegt **nicht** vor. 1,25× p95 ist nur Bewertungsgrösse und wurde nicht angewendet. Die Abbruchzeit von B1 (761 s) übertrifft die der A-Läufe (312 s, 340 s), das ist ein **ermutigender, aber statistisch nicht belastbarer** Befund (n = 1 je Arm, verschiedene Tage); die SYN-Wiederholungsrate liegt in B1 höher (142 von 1 820 Verbindungen = 7,8 % gegenüber 20 von 853 = 2,3 % in R1b), ohne Zuordnung zu O1 (Tageszeit/Funkumgebung nicht kontrolliert).

## Offene Punkte

- **Ursache der Verbindungs-Timeouts `NOT_PROVEN`:** Kandidaten sind volle Annahmewarteschlange bei belegtem HTTP-Server, Speichermangel für Verbindungszustand (ohne gemeldeten Allokationsfehler), Funk/AP-Zustellung; das Host-Pcap zeigt nur die Host-Schnittstelle. Ob O1 zum Verhalten beigetragen hat (mehr Sendeschritte je Antwort, längere Handlerdauer), ist mit n = 1 nicht entscheidbar.
- **Werkzeuglücke (unverändert):** bei `load-failure` läuft kein UART-Nachlauf; die UART wurde bis zum Ende des Lasttreibers (≈ 763,6 s) gelesen, das genügt hier zur Aussage „kein Allokationsfehler im Fenster“.
- **Keine Entscheidung:** F2-D5 offen; B-Serie, A'-Läufe, P-A2, P-B, G2, Rückflash von A und jeder weitere Lauf bleiben gesperrt und brauchen eine eigene ausdrückliche Ownerentscheidung.
