# G1b Stufe E – B2-Gate (einmalige Wiederholung mit dem installierten Image B; Vorschlag zur Ownerfreigabe)

`B2_GATE=PREPARATION_GO` · `B2_HARDWARE=PENDING` · `E_B1=FAIL/HOLD` · `O1_QUALIFICATION=NOT_PROVEN` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`

Dieses Dokument gewährt keine Freigabe. Es baut auf `G1B_STAGE_E_GATE.md` @ `188ddfc292498d35b456cb39c26103613959efda` und auf `E_B1_DECISION_BASIS.md` (`f8699f2d336a5077065e17c5d49140851bbe5f46`) auf. Es deckt **genau einen** Lauf **B2** ab. **Kein** dritter identischer Lauf ohne neue Ursachenbewertung und ausdrückliche Ownerentscheidung. Keine Firmware-, Konfigurations- oder Werkzeugänderung, keine Umsetzung von O2/O3, kein Flash, kein Rückflash, kein P-A2/P-B/G2, keine Folgeaktion. B2 ist **kein** A/B-Vergleich; 1,25× p95 wird nicht angewendet; F2-D5 bleibt offen.

## 1. Zweck

Reproduzierbarkeit des B-Endzustands prüfen (Zeit bis zum Fehler, Fehlerart), bevor O2-artige Entlastung oder ein A/B-Vergleich mit zwei Flashvorgängen entschieden wird. B1 endete nach 761,2 s von 1200 s mit vier echten Verbindungs-Timeouts (SYN unbeantwortet) in einer Stillstandsphase, ohne `heap_alloc_failed`. Ursache der Stillstandsphase: offen.

## 2. Gepinnter Stand (alles unverändert gegenüber B1)

| Gegenstand | Wert |
|---|---|
| Image B (installiert, **nicht** neu geschrieben) | App `4145dddb272e639c9a055df8f9877d4395af32702f15ba92d4f43dae834c7b68` (1 760 576 B), ELF `06c1fb10d72cc4c8ecaf47ef86675f1b305b25035cf62b8cd89811ff3f9d4ef7`, Quelle `31c3ee48b52f44852b2e5cd5f542ba2fce614537` (Branch `agent/pr203-res01-stage-e-b-diag`); aufgelöst `CONFIG_LWIP_TCP_SND_BUF_DEFAULT=2880` |
| Identitätsbeleg ohne neue esptool-Sitzung | E-FB-Verifikation (App-Bereich = B, Nicht-App-Bereiche byteidentisch zur Baseline, Ende 2026-10-15 06:52:30 UTC, `E_FB_RESULT.md` `94744879d56d2afeabc6c37fecc70c3f57934d32`) **und** der B1-Testboot (UART: Quelle `31c3ee4…`, `tcp tx win: 2880`). Seit E-FB gab es nur den B1-Lauf (UART-Portöffnung), **keine** esptool-/Flash-Aktion. |
| Host-Werkzeuge | Werkzeugcommit `07d1526678c171cf4e51a37650c221922d842f32` (Manifest `HOST_TOOLS_SHA256SUMS`, H0 wie `G1B_R1_GATE.md` Abschnitt 2), private tcpdump-Kopie (`cap_net_raw=ep`) nur über temporären `PATH`, Interface `enp2s0`; Treiber/Capture/Pcap unverändert |
| Lastprofil | P-A unverändert: vier Sessions, 60 s Idle, **maximal 1200 s Last + 300 s Recovery**, Login-Timeout 8 s, übrige 3 s, kein Logout/Retry |
| Ausgabeordner (privat 0700) | `/var/lib/docker/data/ESP32-Projekte/hw-evidence-pr203/diag/g1b/stage_e_20261015/run_b2/` |
| Gate-ID | `PR203_RES01_G1B_STAGE_E_B2` |

## 3. Geräteaktionen (nur unter ausdrücklicher Ownerfreigabe)

| Nr. | Aktion | Wirkung |
|---|---|---|
| 1 | Port-Öffnung durch `g1_capture.py` (bekannter FT232R-Reset) | **1 Reset = erfasster Testboot**; kein weiterer Reset |
| 2 | Last gemäss P-A über HTTP (Lasttreiber), parallel UART-/Pcap-Überwachung | nur Netzwerklast, keine Konfigurations- oder Schreibzugriffe |

**Nicht Teil:** esptool-Sitzung, Flash, Rückflash, weiterer Reset, Touch, P1–P3. Identität: Der Operator bestätigt vor dem Lauf schriftlich, dass seit E-FB (06:52:30 UTC) kein Flash/esptool stattfand; zusätzlich prüft er im UART des Testboots Quelle `31c3ee48…`, `tcp tx win: 2880`, Release-Profil, `real actuators: disabled`, `HomeConnected` (Bereit-Kriterium). **Bei Zweifel oder Abweichung HOLD**, kein Reparaturflash.

## 4. Ablauf (identisch zu B1)

Vorbedingungen H0: Review-Branch sauber, `git diff 07d1526… HEAD -- 'diag/*.py'` leer, `sha256sum -c diag/review/g1b/HOST_TOOLS_SHA256SUMS` OK, Artefakt-`SHA256SUMS` unter `stage_e_20261015/` OK, NTP synchron, `~/pw` vorhanden, keine anderen Prozesse am Port. Alle Befehle mit `PATH=/home/manuel/g1b-tools:$PATH` nur beim Pcap-Aufruf.

1. `g1_capture.py --run --gate-id PR203_RES01_G1B_STAGE_E_B2 --allow-port-open-boot --expected-source 31c3ee48b52f44852b2e5cd5f542ba2fce614537 --output <Ausgabeordner>/candidate_capture --load`.
2. Bereit (≤ 120 s): Quelle, `application: ready`, `real actuators: disabled`, Release-Profil, `HomeConnected`, frischer Heartbeat und `tcp tx win: 2880`; sonst `STOP` und HOLD.
3. `g1_pcap.py capture --run --gate-id PR203_RES01_G1B_STAGE_E_B2 --interface enp2s0 --device-ip <Standard von g1_capture.py> --output <Ausgabeordner>/pcap/cap.pcap --stop-file <…>/candidate_capture/ABORT --stop-file <…>/candidate_capture/STOP` (`--max-seconds` Vorgabe 1961), auf `PCAP_RUNNING` ≤ 30 s warten, sonst HOLD.
4. Erst dann `START_LOAD`; Last startet bei Uptime ≥ 180 s.
5. Endsignal: Anomalie ⇒ `ABORT` durch den Supervisor; reguläres Ende (`load.json` `COMPLETED_PENDING_REVIEW`) ⇒ Operator legt spätestens 60 s danach `STOP` an.
6. Danach **keine** Geräteaktion.

## 5. Stopbedingungen (unverändert, STOP/HOLD ohne Retry, weiteren Reset, Flash, Rückflash oder Folgelauf)

`uart-anomaly` (inkl. `heap_alloc_failed`, Panic, Watchdog, Brownout), unerwarteter Reset, Heartbeat-Lücke > 5 s, Quellabweichung, Main-Stack < 11 160 B, HTTPD-Stack < 2 080 B, **jeder Login- oder Request-Fehler** (kein Retry), Capture-/Pcap-Fehler, Skripthash ≠ Manifest, Ownerstopp. Die harten Kriterien aus Gate Abschnitt 5 (Plan F1/F4 Abschnitt 7) gelten über den **gesamten** Lauf.

## 6. Vorab festgelegte Einordnung des Ergebnisses (keine Automatik; jede Folge braucht eine Ownerentscheidung)

Massgeblich ist die **erste Anomalie**; beide Befundarten werden zusätzlich berichtet.

| B2-Ergebnis | Kriterium | Einordnung / Folge |
|---|---|---|
| **A. Fehlerfreier Lauf** | 1200 s Last und 300 s Recovery vollständig, alle harten Kriterien erfüllt (kein `heap_alloc_failed`/Abort/Panic/Watchdog/Brownout/Reset, HTTP 100 % 2xx, keine nicht abort-assoziierten Timeouts/Verbindungsabbrüche, Erholung ≥ 90 % innerhalb 120 s, Stack-Reserven, F4-Verhalten) | B1 war ein nicht reproduzierter Stillstand; O1 gestärkt, **nicht qualifiziert** (n = 2, kein A-Bezug). Nächster Schritt nur nach Ownerentscheid: gleichtägiger A'-Bezug (Rückflash) nach Testblatt Abschnitt 5 |
| **B. Erneute Verbindungs-Timeouts** | erster Fehler = `timeout` mit `error_phase connect`, `abort_induced false`, im Pcap SYN ohne SYN/ACK, ohne `heap_alloc_failed` | reproduzierbare B-Signatur (Stillstand + Sättigung 7 + 5); **kein** O1-Rückbau begründet; Ownerentscheid über O2-artige Entlastung (neuer Plan) oder Lastprofilfrage (P-B) |
| **C. Heap-Allokationsfehler** | `heap_alloc_failed` (Hook-Backtrace dekodiert gegen den gepinnten ELF) als erste Anomalie | O1 allein unzureichend; Rückfall in den Plan |
| **D. Andere Anomalie** | Login-Timeout vor Last (R1-Typ), unerwarteter Reset, Panic, andere Fehlerart | HOLD, eigene Bewertung mit vorhandener Evidence; keine Wiederholung |

**Nach B2: kein dritter identischer Wiederholungslauf** ohne neue Ursachenbewertung und Ownerentscheidung; B2 liefert keine Fehlerrate und keine statistische Aussage.

## 7. Auswertung und Veröffentlichung

Bericht nach dem Muster `E_B1_RESULT.md`: Boot-/Identitätsbelege, Latenz je Minute, erste Anomalie mit Zeit (Lastsekunde), Heap-Phasen (`res01_http`), Gate-Reihe, Drahtzähler und Verbindungsverlauf (Backlog/Sitzungs-Sicht), Einordnung A–D, Kontext-Vergleich ausdrücklich nicht vergleichend. Veröffentlicht werden nur abgeleitete Zähler, `uart_tail.log`/Endauszug, `res01_http_phases.csv`, `g1_gate_series.csv`, `load_summary.json`, `capture.json`, `pcap_result.json`, `pcap_analysis_derived.json`, `PRIVATE_SOURCE_SHA256.json`; **nie** Pcap, Readbacks, Passwort, Cookies, vollständige UART-Rohdaten. Bekannte Werkzeuglücken (kein UART-Nachlauf bei `load-failure`) bleiben als Bewertungsgrenze.

## 8. Ownerbestätigungen (einzeln, vor dem Lauf; alle `PENDING`)

| # | Bestätigung | Wert |
|---|---|---|
| 1 | B2 (genau ein Lauf) freigeben | `PENDING` |
| 2 | Aktorfreier Devboard-Aufbau vor Ort bestätigt; Owner anwesend | `PENDING` |
| 3 | Genau ein Reset (Port-Öffnung/Testboot); kein Flash, kein Rückflash, keine esptool-Sitzung | `PENDING` |
| 4 | Seit E-FB hat niemand das Gerät geflasht oder anderweitig verändert (Owner bestätigt) | `PENDING` |
| 5 | Einordnung A–D nach Abschnitt 6 akzeptiert; **kein** dritter identischer Lauf ohne neue Ursachenbewertung | `PENDING` |
| 6 | F2-D4: 1,25× p95 nur Bewertungsgrösse; F2-D5 bleibt offen | `PENDING` |

Nicht Teil dieses Gates: O2/O3, Konfigurations-/Firmware-/Werkzeugänderungen, A'-Läufe, Rückflash von A, B-Serie, P-A2, P-B, G2, Rollback.
