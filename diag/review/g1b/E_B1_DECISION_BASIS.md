# E-B1 – Entscheidungsgrundlage: unbeantwortete SYNs und nächster Schritt (nur offline)

Grundlage: `E_B1_RESULT.md` (`998b0ae0ee5a25ce27c58b48324cd78ee2283343`, Independent Review: `E_B1=FAIL/HOLD`), die private B1- und R1b-Evidence (Pcap, UART, `load.json`) und der ESP-IDF-6.1-Quelltext. Keine Firmware-, Build-, Hardware- oder Laufaktion. `E_B1=FAIL/HOLD` · `O1_QUALIFICATION=NOT_PROVEN` · `RES-01=BLOCKER` · `F2_HARDWARE=HOLD` · `ACTUATOR_RELEASE=NO`.

## 1. Technisch gestützte Mechanik (Quelltext, belegt)

| Befund | Quelle |
|---|---|
| lwIP **verwirft** ein SYN **stillschweigend** (kein RST, keine Antwort), wenn `accepts_pending ≥ backlog` (Backlog = `backlog_conn` = 5, `TCP_LISTEN_BACKLOG=1`) oder wenn `tcp_alloc` keinen PCB bekommt (`tcp.memerr`). Der Client sendet SYN erneut (1 s, 3 s) und läuft nach 3 s in den Timeout. `accepts_pending` zählt Verbindungen, die lwIP angenommen hat, die die Anwendung aber noch nicht per `accept()` geholt hat. | `lwip/src/core/tcp_in.c:655-680` |
| `esp_http_server` ist **ein** Task. Er horcht auf neue Verbindungen **nur, solange ein Sitzungsslot frei ist** (`max_open_sockets` = 7, ohne `lru_purge`), und nimmt je Schleifendurchlauf **eine** Verbindung an. Anfragen werden **nacheinander** bearbeitet; ein Handler, der in `send()` blockiert (bis zu `send_wait_timeout` 5 s), hält den gesamten Server an. | `httpd_main.c:54-100, 276-335`, `esp_idf_http_server_lifecycle.cpp:192-199` (Default unverändert) |
| Kapazität bis zum SYN-Verlust ≈ **7 Sitzungen + 5 wartende** = 12 gleichzeitige Verbindungen. Im geprüften `httpd_parse.c` findet sich keine Auswertung eines `Connection`-Headers; eine Sitzung endet, wenn der Server das Verbindungsende des Clients in seiner Schleife verarbeitet. | `httpd_parse.c` (Suche), `httpd_main.c` |
| Der Lasttreiber öffnet **je Request eine neue Verbindung** (`Connection: close`), und jede Session sendet ihre 3 (alle 6 Ticks 4) GETs **gleichzeitig**; vier Sessions im Raster von 1,2 s ⇒ 12–16 Verbindungsversuche innerhalb 3,6 s. | `c0_load.py` (`poll`), Plan Abschnitt 7 |

## 2. B1-Evidence (belegt, Host-Schnittstelle und Gerätelog)

- **Zeitleiste um 758 s (Pcap, je Verbindung):** Die Shell-Antwort von Verbindung 1916 braucht für 8 155 B etwa 1,7 s (Gerät: Handler-Sendephase 2,33 s, `res01_http`). In dieser Zeit liefern die bereits angenommenen Verbindungen 1917–1920 erst bei 759,8 s ihre erste Antwort, obwohl ihre Anfragen schon bei 758,0–758,8 s vorlagen (**Head-of-Line-Blockade** durch die sequenzielle Bearbeitung). Bei 760,0–761,0 s zeigt die Host-Sicht **7 belegte Sitzungen und 4 wartende Verbindungen**, genau in dem Zeitraum, in dem SYNs unbeantwortet bleiben (Verbindungen 1922/1923 ab 758,2 s, 1928/1931 ab 760,6 s; ein SYN/ACK kommt erst nach 3,18 s, also nach dem Client-Timeout). Das ist **konsistent mit** der Sättigung 7 + 5.
- **Kein Speichermangel beim Beginn der SYN-Verluste:** freier Heap 32 708 B (`res01_http` `pre` bei 758,1 s); die niedrigen Werte (grösster Block 3 200–4 608 B) treten erst bei ≈ 761,7–762,1 s auf, **nach** den ersten Verlusten. Ein Speicherfehler von `tcp_alloc` ist für den Beginn **nicht gestützt**.
- **Die Stillstände sind kein O1-Spezifikum:** Shell-Sendephasen (device-seitig, `built→sent`): B1 Median 0,20 s (= R1b 0,20 s), B1 p90 0,39 s (R1b 0,25 s); Sendephasen > 1 s: B1 3 (nur in der Endphase 758–762 s), R1b 0, im A-Lauf G1 2,6 s. Stationär (Pcap): Shell-Sendezeit B1 Median 0,29 s (R1b 0,39 s), API-Antwortzeit gleich (0,09 s gegenüber 0,11 s). **Keine systematische Verlangsamung durch die kleinere Sendewarteschlange.**
- **Früher SYN-Verlust ohne Folgen:** 130 B1-Verbindungen brauchten ein zweites SYN; bei fast allen lag die Zahl der offenen Verbindungen weit unter 12 (Median 2). Diese Verluste stammen **nicht** aus der Backlog-Sättigung (Funk/Zustellung möglich); nur die drei unbeantworteten fielen in die Sättigungsphase.

## 3. Hypothesen (nicht belegt) und fehlende Information

| # | Hypothese | Stand |
|---|---|---|
| H1 | Endzustand von B1 = Folge einer **Stillstandsphase** (Shell-Sendephasen 1,1 / 2,3 s, verzögerte SYN/ACKs) und anschliessender Sättigung 7 + 5 durch Lasttreiber (Bursts, neue Verbindung je Request) und sequenziellen Server | gestützt (Abschnitt 2), Backlog-Zähler selbst nicht beobachtbar |
| H2 | Auslöser der Stillstandsphase (Funk/AP-Zustellung, Powersave, Wi-Fi-Pufferengpass, Server blockiert in `send()` wegen späten ACKs) | **offen**; ein Funk-/AP-Beitrag ist auf der Host-Seite nicht sichtbar |
| H3 | O1 verhindert oder verschiebt die RX-Allokationsstörung (B1: Heap bis 5 468 B ohne Fehler, A: Fehler bei ≈ 15 KB) | mit n = 1 je Arm und verschiedenen Tagen **nicht entscheidbar** |
| H4 | O1 verschlimmert die Endphase (mehr Sendeschritte je Shell bei hoher ACK-Latenz) | im Stationärbetrieb nicht gestützt; in der Endphase **nicht unterscheidbar** (A zeigte 2,6 s ebenfalls) |
| H5 | Das Verhalten ist durch das Lastprofil (Bursts, kein Keep-Alive) überzeichnet gegenüber Browsern | plausibel, nur durch P-B prüfbar |

**Entscheidende fehlende Information:** (a) **Reproduzierbarkeit des B-Endzustands** und seiner Zeit bis zum Fehler (n = 1); (b) **gleichtägiger A-Bezug** (O1-Zuordnung); (c) Ursache der Stillstandsphase (kein vorhandenes Messmittel liefert die AP-/Funkseite).

## 4. Beurteilung

- **Eine minimale Produktkorrektur ist durch die Evidence nicht begründet.** Ein Defekt in O1 selbst ist nicht erkennbar; die einzige gestützte Schwachstelle (Sättigung 7 + 5 bei sequenzieller Bearbeitung) liegt in einer **anderen Stellschraube** (HTTP-Parallelität/Backlog, O2), die ausserhalb des genehmigten O1-Umfangs liegt, zusätzlichen RAM bindet und gegen das Hauptproblem (Heapknappheit) arbeitet; ohne Reproduktion und Lastprofil-Gegenprobe ist sie nicht zu rechtfertigen.
- **O1 zurücknehmen ist nicht begründet:** Kein Hinweis auf einen O1-Schaden (Abschnitt 2), der einzige O1-Lauf hielt 761 s ohne die Allokationsstörung, an der beide A-Läufe nach 312/340 s scheiterten (nicht belastbar, n = 1). Ein Rücknehmen entfernt den einzigen Hebel und führt zur bekannten fehlerhaften Konfiguration zurück.

## 5. Genau eine Handlungsempfehlung

**Eng begrenzte weitere Messung: ein einziger weiterer P-A-Lauf mit dem installierten Image B (B2), unverändert (gleiche Werkzeuge, kein Flash, 1 Port-Öffnungsreset), unter eigenem Ownergate.** Zweck: Reproduzierbarkeit des B-Endzustands (Zeit bis zum Fehler, Fehlerart) feststellen, **bevor** weitere Hebel (O2) oder ein aufwendiger A/B-Vergleich mit zwei Flashvorgängen entschieden werden.

Vorab festgelegte Lesart (keine Automatik, jede Folge braucht Ownerentscheid):

| B2-Ergebnis | Folge für die Entscheidung |
|---|---|
| vollständig (1200 s + 300 s) ohne harten Fail | B1 war ein nicht reproduzierter Stillstand; O1 gestärkt, aber nicht qualifiziert; danach gleichtägiger A'-Bezug (Rückflash) nach Testblatt Abschnitt 5 |
| erneut Verbindungs-Timeouts bei Sättigung 7 + 5 nach Stillstandsphase | reproduzierbare B-Signatur ⇒ Ownerentscheid über O2-artige Entlastung (neuer Plan, Hardwarebezug P-B) oder Lastprofilfrage; **kein** O1-Rückbau begründet |
| `heap_alloc_failed` (RX) | O1 allein unzureichend; Rückfall in den Plan |

Nicht empfohlen: Rücknahme von O1, Umbau von O1, O2-/O3-Umsetzung, neue Diagnoseinstrumente, weitere Flashvorgänge ohne vorherige B2-Sichtung. Die bekannten Werkzeuglücken (kein UART-Nachlauf bei `load-failure`) werden **nicht** vorsorglich behoben. F2-D5 bleibt offen; 1,25× p95 bleibt Bewertungsgrösse.
