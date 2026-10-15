# G1b – NETPROBE1-Gate (hostseitige ICMP-Leerlaufprobe; Vorschlag zur Ownerfreigabe)

`NETPROBE1_GATE=PREPARED` · `NETPROBE1_AUTHORIZATION=PENDING` · `O1_QUALIFICATION=NOT_PROVEN` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`

Dieses Dokument gewährt **keine** Freigabe; ICMP-Probe, Netzwerkverkehr zum Gerät und jede andere Hardwareaktion sind **nicht** erlaubt, bevor der Owner dieses Gate ausdrücklich bestätigt (Abschnitt 8). Grundlage: `OBS1R_NETPATH_ASSESSMENT.md` @ `1388ccfc9799f1c915b9d1112020497287ab5984` (Independent Fix Verification PASS). Das Gate deckt **genau eine** hostseitige Probe NETPROBE1 ab. Ausgeschlossen: Portöffnung (UART), Reset, Flash, esptool, HTTP, Last, Pcap, jede Werkzeug-, Firmware-, Konfigurations-, O1/O2/O3-Änderung, jede Folgeaktion. Die Auswertung bleibt **bedingte Hinweise ohne Kausalüberdehnung** (Abschnitt 6). F2-D4: 1,25× p95 Bewertungsgrösse; F2-D5 offen.

## 1. Zweck

Prüfen, ob die Stillstandsphase (verzögerte oder ausbleibende Zustellung zum Gerät) auch **ohne HTTP-Last** auftritt, insbesondere beim **Erstkontakt nach Leerlauf** (R1-ähnlich, aber nicht als gleiche Ursache behandelt), und wie sich die Antwortzeit bei dichtem Verkehr verhält. Es werden **keine** AP-, Funk-, Treiber- oder lwIP-Aussagen aus einem Einzelergebnis abgeleitet (ICMP-Echo wird auf dem ESP32 ebenfalls im IP-/lwIP-Pfad beantwortet).

## 2. Gerät und Hostwerkzeuge (alles unverändert)

| Gegenstand | Wert |
|---|---|
| Gerät | aktorfreies Devboard, **unverändert** Image B (App `4145dddb272e639c9a055df8f9877d4395af32702f15ba92d4f43dae834c7b68`, Quelle `31c3ee48b52f44852b2e5cd5f542ba2fce614537`, `CONFIG_LWIP_TCP_SND_BUF_DEFAULT=2880`); läuft seit dem Testboot des OBS1R-Laufs (2026-10-15 10:19:48 UTC) im Leerlauf; seit E-FB keine esptool-/Flash-Aktion. Es wird **nichts** am Gerät geändert. |
| Host | derselbe Host; Interface `enp2s0` (kabelgebunden); Hostuhr NTP-synchron (zu bestätigen) |
| Werkzeuge | nur Betriebssystem-Werkzeuge: `ping` (iputils 20240905, **mit `-D` und `-i 0.2` ohne Zusatzrechte auf Loopback geprüft**), `ip` (nur lesend: `route get`, `neigh show`), `date`, `sleep`. **Kein** Repository-Werkzeug, **kein** `g1_pcap.py` (reines TCP/IPv4-Werkzeug, für ARP/ICMP ungeeignet), **kein** `g1_capture.py`. Die Befehle in Abschnitt 5 sind **dokumentierte Shell-Befehle**, keine neue Software. |
| Evidence-Ablage | privat (0700), ausserhalb jedes Git-Worktrees: `/var/lib/docker/data/ESP32-Projekte/hw-evidence-pr203/diag/netprobe1_20261015/` |

## 3. Host-Vorprüfung (offline erledigt, ohne ein Paket an das Gerät) – tatsächliche Route und Identität

| Befund | Ergebnis |
|---|---|
| Route (`ip route get`, Kernel-Tabelle) | `dev enp2s0 src <Host>`: **direkt (on-link), kein Gateway** – Host und Gerät liegen im selben IPv4-Subnetz ohne Router dazwischen; die einzige drahtlose Strecke ist AP ↔ Gerät |
| Neighbor-Cache (`ip neigh show`, nur Cache lesen) | Eintrag für das Gerät vorhanden, Zustand `STALE`, `lladdr` = **Geräte-MAC** aus dem UART-Boot-Log (`mode : sta (…)`) |
| Warum `ip neigh` allein nicht genügt | Ein Eintrag mit der Geräte-MAC belegt nur, dass die ARP-Antwort diese MAC nannte; ein AP mit Proxy-ARP könnte stellvertretend für ein schlafendes Gerät mit dessen MAC antworten. **Deshalb zusätzliche, stärkere Belege aus vorhandener Evidence:** in den privaten Pcaps von OBS1R, B1 und B2 tragen **100 % der Gerätepakete (1 822 / 10 578 / 1 472) die Geräte-MAC als Ethernet-Quelle und TTL 64**, und alle Hostpakete zum Gerät (2 459 / 14 282 / 1 920) sind an die Geräte-MAC adressiert. TTL 64 = lwIP-Default (`CONFIG_LWIP_IP_DEFAULT_TTL=64`) ohne Router-Hop ⇒ **direkte Layer-2-Nachbarschaft**, die Antworten stammen vom Gerät selbst. |
| Hostseitige Einstellungen | `proxy_arp=0`, `rp_filter=2` (loose), `arp_ignore=0`; Neighbor-Zeiten `base_reachable_time_ms=30000`, `gc_stale_time=60`: bei 180-s-Abstand ist der Eintrag beim Erstkontakt typischerweise `STALE` ⇒ der Host kann zusätzlich ein ARP-Probe senden (Eigenschaft des Messaufbaus, nicht Defekt) |
| Ping-Rechte | `ping -D -i 0.2 -W <s>` läuft ohne Zusatzrechte (Loopback-Trockentest, kein Gerätekontakt) |

**Identitätsnachweis in der Probe (statt `ip neigh` allein):** (1) Die Antwortquelle ist die Geräte-IP; (2) die Antwort-**TTL ist 64** (Abweichung ⇒ HOLD: möglicher Router-/Proxy-Hop oder anderes Gerät); (3) der Neighbor-Eintrag wird **vor und nach jedem Burst** protokolliert und muss die Geräte-MAC zeigen (Abweichung wird gemeldet); (4) der Owner bestätigt, dass nichts am Gerät verändert wurde. Ein Reset ist hostseitig **nicht erkennbar** und **kein** Gate-Kriterium.

## 4. Geräte-/Netzaktionen (nur unter ausdrücklicher Ownerfreigabe)

| Nr. | Aktion | Wirkung |
|---|---|---|
| 1 | Host liest Route und Neighbor-Cache (lokal, kein Paket) | keine |
| 2 | **ICMP-Echo** vom Host an die Geräte-IP, Phasen 1 und 2 (Abschnitt 5); dazu die vom Kernel ausgelösten ARP-Anfragen | höchstens 5 Echo/s, in Summe etwa 9 200 Pakete in 90 min an ein aktorfreies Devboard; keine Schreib-/Konfigurationszugriffe |

**Nicht Teil:** UART-Portöffnung (würde das Board zurücksetzen), HTTP, Last, Reset, Flash, esptool, Pcap, Aktoren.

## 5. Ablauf (host-only, zwei Phasen, 90 min)

Parameter: Antwort-Timeout `-W 2`, Zeitstempel `-D` (Host-Epoch, UTC-Auswertung), Zieladresse = Geräte-IP (Standard `g1_capture.py`-Host), alle Ausgaben in eine private Datei (Befehlstext und Ausgabehash werden notiert).

0. **Vorab (ohne Gerätekontakt):** NTP-Synchronisation prüfen, `ip route get <Gerät>` und `ip neigh show <Gerät>` notieren (erwartet: on-link, Geräte-MAC), Ausgabeverzeichnis anlegen.
1. **Phase 1 – Erstkontakt nach Leerlauf (60 min):** Zyklus alle **180 s**, 20 Zyklen: Neighbor-Zustand notieren → `ping -D -c 3 -i 0.2 -W 2 <Gerät>` → Neighbor-Zustand notieren. Zwischen den Zyklen **kein** Verkehr zum Gerät (kein Hintergrundping).
2. **Phase 2 – dichte Probe (30 min):** `ping -D -i 0.2 -W 2 <Gerät>` fortlaufend über 1 800 s (≈ 9 000 Echos).
3. **Ende:** Neighbor-Zustand und `ip -s link show enp2s0` (Zähler) notieren; keine weiteren Pakete.

**Stopp/HOLD:** Ownerabbruch; Antwort-TTL ≠ 64 oder Antwortquelle ≠ Geräte-IP; Neighbor-Eintrag zeigt eine **andere** MAC; anhaltender Totalverlust (drei aufeinanderfolgende Bursts in Phase 1 ohne jede Antwort oder 60 s ohne Antwort in Phase 2) wird als **Verbindungsverlust protokolliert, Ursache `UNKNOWN`** (ein Geräte-Reset, ein Funkproblem und ein AP-Zustand sind hostseitig nicht unterscheidbar); danach Abbruch der Probe, **keine** Wiederholung, **keine** UART-/Reset-/Flash-Aktion.

## 6. Auswertung (bedingte Hinweise; verbindliche Grenzen)

Offline, nur aus den privaten Logs; abgeleitete Zähler und UTC-Zeiten, **keine** Adressen im Bericht. Berichtet werden: Antwortzeitverteilung (p50/p95/p99/Max) je Phase, Verlust, Stösse mit Antwortzeit ≥ 1 s oder Verlustserien, Erstkontakt-Tabelle (je Zyklus: erste Antwortzeit, Verluste, Neighbor-Zustände vorher/nachher), TTL/Quelle der Antworten, Host-Zähler.

**Auswertungsgrenzen:** ICMP-Echo-Antworten entstehen auf dem ESP32 über den IP-/lwIP-Pfad. Verzögerungen im Leerlauf belegen ein Problem **ohne HTTP-Last**, nicht „vor lwIP“ und nicht AP gegenüber WLAN-Treiber. Ausreisser nur in der dichten Phase belegen keinen AP-/Funkfehler. **Zwei unauffällige Phasen widerlegen seltene Leerlaufereignisse nicht und beweisen keine ausschliesslich lastgebundene Ursache.** Keine erwartete Ereigniszahl wird behauptet.

| Befund | Bedingter Hinweis | Nächster Entscheidungsschritt (Owner; keine Automatik) |
|---|---|---|
| Phase 1: Verzögerungen/Verluste ≥ 1 s beim Erstkontakt | Zustellungs-/Verarbeitungsproblem **ohne HTTP-Last** (kompatibel mit AP/Funk, Geräte-Wi-Fi/PS und Geräte-lwIP; nicht trennbar) | AP-/Controller-Daten zu diesen Zeitpunkten einholen; danach **eine** Einflussgrösse am Gerät prüfen: `WIFI_PS_NONE` (`esp_wifi_set_ps`) |
| Nur Phase 2: Stösse | Stösse unter dichtem Verkehr; **kein** Beleg für AP/Funk allein | AP-/Controller-Daten und Kanalauslastung prüfen; Reproduzierbarkeit bewerten, bevor Firmware angefasst wird |
| Beide Phasen unauffällig | **kein Nachweis** der Abwesenheit seltener Leerlaufereignisse, **kein** Nachweis einer rein lastgebundenen Ursache | Owner entscheidet über eine längere Probe oder einen einzigen Beobachtungslauf mit paralleler ICMP-Probe |

## 7. Veröffentlichung

Nur abgeleitete Statistik und Zeitpunkte (UTC) mit Hashes der privaten Logs auf dem Review-Branch; **nie** die Rohlogs (enthalten Geräte-IP), keine MAC-/IP-Adressen, keine AP-Daten.

## 8. Ownerbestätigungen (einzeln, vor Beginn; alle `PENDING`)

| # | Bestätigung | Wert |
|---|---|---|
| 1 | NETPROBE1 (genau eine hostseitige ICMP-Probe nach Abschnitt 5, 90 min) freigeben | `PENDING` |
| 2 | Aktorfreier Devboard-Aufbau vor Ort bestätigt; Owner anwesend | `PENDING` |
| 3 | Das Gerät ist unverändert (Image B, kein Flash/Eingriff seit E-FB und seit OBS1R); Netzwerkverkehr (ICMP, bis 5 Echo/s) zum Gerät ist akzeptiert | `PENDING` |
| 4 | **Kein** UART-Zugriff, Reset, Flash, HTTP, Last oder Pcap; Verbindungsverlust nur protokolliert (Ursache `UNKNOWN`), kein Reset-Kriterium | `PENDING` |
| 5 | Auswertungsgrenzen und bedingte Hinweise (Abschnitt 6) akzeptiert; keine Folgeaktion ohne neue Ownerentscheidung | `PENDING` |
| 6 | Optional: AP-/Controller-Export (nur lesend) für die Probenfenster bereitgestellt; F2-D4 Bewertungsgrösse, F2-D5 offen | `PENDING` |
