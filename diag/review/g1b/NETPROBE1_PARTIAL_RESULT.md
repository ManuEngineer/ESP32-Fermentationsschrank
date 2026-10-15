# NETPROBE1 – Teilergebnis (vom Owner abgebrochen nach 12 von 20 Bursts; Phase 2 nicht ausgeführt)

Freigabe: Owner bestätigte alle sechs Punkte aus Abschnitt 8 von `G1B_NETPROBE1_GATE.md` @ `2c9bc2e3541688b9633fbc763cc47fabfd9d4e48`. Ausführung nach dem Gate: Vorprüfung (Gate-Stand, NTP, Route/Neighbor lokal), Loopback-Trockenlauf unmittelbar vorher (`-w 3`: 3,00 s, Exit 0, 15/15; Burst `-c 3`: 0,41 s, Exit 0), dann Phase 1 mit `t_start` 2026-10-15 14:04:04 UTC. **Abbruch durch den Owner** um 14:38:45 UTC (Anweisung: stoppen, sichern, committen, pushen). Controller und `ping` sind beendet (geprüft); nach dem Abbruch wurde **kein weiteres Paket** an das Gerät geschickt. **Phase 2 (dichte Probe) wurde nicht gestartet.** Es gab keinen UART-Zugriff, Reset, Flash, HTTP, Last oder Pcap; keine Werkzeug-, Firmware- oder Konfigurationsänderung. Rohlogs (enthalten Geräte-IP/MAC) bleiben privat; veröffentlicht sind nur abgeleitete Daten unter `diag/review/g1b/netprobe1_partial/`.

`NETPROBE1=ABORTED_BY_OWNER (Teilergebnis)` · `O1_QUALIFICATION=NOT_PROVEN` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`

## Durchgeführt

Die Steuerung war ein dokumentiertes Shell-Skript (nur `ping`/`ip`/`date`/`sleep`, sanitisiert und gehasht abgelegt: `netprobe1_controller_sanitized.sh`), das genau die Gate-Befehle in absoluten 180-s-Zeitpunkten ausführte und Neighbor-Zustand vor/nach jedem Burst sowie TTL/Quelle prüfte. **Kein Stopp-Kriterium des Gates wurde ausgelöst** (keine TTL-/Quellen-/MAC-Abweichung; keine drei aufeinanderfolgenden leeren Bursts).

## Messdaten Phase 1 (12 Bursts à 3 Echos; `phase1_bursts_derived.csv`)

| Burst (UTC) | Empfangen | Antwortzeit (ms) | Neighbor vor → nach | Klasse |
|---|---|---|---|---|
| 0 (14:04:04) | 0/3 | – | STALE → DELAY | auswertbar |
| 1 (14:07:04) | 0/3 | – | **FAILED → INCOMPLETE** | **INDETERMINATE** |
| 2 (14:10:04) | 1/3 | **2 165** (erste Antwort erst bei Echo 1) | **FAILED →** REACHABLE | **INDETERMINATE** |
| 3 (14:13:04) | 2/3 | 198; 70,6 (Echo 1 verloren) | STALE → DELAY | auswertbar |
| 4 (14:16:04) | 2/3 | 228; 111 (Echo 1 verloren) | STALE → DELAY | auswertbar |
| 5 (14:19:04) | 1/3 | 248 (Echos 1 und 3 verloren) | STALE → DELAY | auswertbar |
| 6 (14:22:04) | 2/3 | 162; 318 | STALE → DELAY | auswertbar |
| 7 (14:25:04) | **0/3** | – | STALE → DELAY | auswertbar |
| 8 (14:28:04) | 3/3 | 166; 6,6; 121 | STALE → DELAY | auswertbar |
| 9 (14:31:04) | 2/3 | 239; 84,7 | STALE → DELAY | auswertbar |
| 10 (14:34:04) | 2/3 | 170; 198 | STALE → DELAY | auswertbar |
| 11 (14:37:04) | **0/3** | – | **FAILED → INCOMPLETE** | **INDETERMINATE** |

Summe: 36 Echos gesendet, **15 empfangen**; **4 von 12 Bursts ohne jede Antwort** (0, 1, 7, 11); alle empfangenen Antworten mit TTL 64 und Quelle = Geräte-IP. Auswertbar nach dem Gate (Neighbor vorher und nachher aufgelöst, MAC unverändert): 9 Bursts, davon **2 mit Totalverlust (0 und 7)** und 5 mit Verlust des ersten Echos bei späteren Antworten; **INDETERMINATE** (Neighbor `FAILED`/`INCOMPLETE`, ARP-/Host-Nachbarschaft-konfundiert): die Bursts 1, 2, 11. Die Antwortzeit schwankt im aufgelösten Fall zwischen 6,6 und 318 ms; die 2 165 ms stammen aus einem konfundierten Burst.

## Bedingte Hinweise (Gate Abschnitt 6; keine Kausalaussage)

- **Beobachtet (belegt):** Im Leerlauf mit 180 s Pause werden Erstkontakte vom Host häufig (4 von 12 Bursts komplett, weitere teilweise) nicht oder spät beantwortet, und der Host markierte den Neighbor-Eintrag dreimal als `FAILED`/`INCOMPLETE` (die ARP-Auflösung des Geräts schlug innerhalb der Host-Wartezeit fehl). Das ist ein Hinweis auf ein Zustellungs-/Verarbeitungsproblem **ohne HTTP-Last** und **nicht** an die HTTP-Last gebunden.
- **Nicht gesichert / Grenzen:** ICMP-Echo (und ARP) wird auf dem ESP32 im IP-/lwIP-Pfad beantwortet; der Befund ist daher **nicht** „vor lwIP“ und **nicht** AP gegen WLAN-Treiber zuzuordnen. Die hostseitige ARP-/Neighbor-Wechselwirkung wurde **nicht beobachtet** (kein Pcap): ein Leerlaufverlust beim Erstkontakt kann Host-ARP-Zustand, AP-Pufferung, Powersave des Geräts oder Geräteverarbeitung enthalten. Die `INDETERMINATE`-Bursts zählen nicht für die Hypothesen. Phase 2 fehlt, daher keine Aussage zum dichten Verkehr; 12 Bursts sind keine Rate.
- **Kompatibel mit dem R1-Muster** (Leerlauf-Stillstand beim ersten Kontakt), nicht als gleiche Ursache bewiesen.

## Nächster Entscheidungsschritt (Owner; keine Automatik)

AP-/Controller-Daten für 14:04–14:38 UTC (16:04–16:38 Europe/Zurich) einholen (Reassoziation, Retries, Airtime, DTIM, Powersave-Pufferung für das Gerät); **erst danach** entscheidet der Owner, ob **eine** Einflussgrösse am Gerät geprüft wird (Kandidatin: `WIFI_PS_NONE`). Die fehlende Phase 2 und ein ARP-sichtbarer Aufbau (Pcap für ARP/ICMP, derzeit kein Werkzeug) sind optionale Folgeentscheidungen. Kein weiterer Lauf ohne neue Ownerfreigabe.
