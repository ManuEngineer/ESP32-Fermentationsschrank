# G1b – Beobachtungsmodus-Gate (ein aktorfreier Diagnoselauf; Vorschlag zur Ownerfreigabe)

`OBSERVE_GATE=PREPARED` · `OBSERVE_HARDWARE=PENDING` · `E_B2=FAIL/HOLD` · `O1_QUALIFICATION=NOT_PROVEN` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`

Dieses Dokument gewährt keine Freigabe und deckt **maximal einen** Diagnoselauf (**OBS1**) ab. Es ändert **keine** harten P-A-Abnahmekriterien; die Läufe B1 und B2 bleiben `FAIL/HOLD`. Der Beobachtungsmodus ist ein **separater Diagnosemodus**, keine Abnahme: nach dem ersten echten Netzwerk-Fehler ist **kein PASS und keine Recovery-/Abnahmeaussage** möglich. Kein weiterer Lauf, kein Flash/Rückflash, kein dritter identischer B-Lauf, keine Folgeaktion ohne neue ausdrückliche Ownerfreigabe. Keine Firmware-, O1-, F4-, HTTPD- oder ESP-IDF-Konfigurationsänderung; O2/O3 nicht umgesetzt. F2-D4: 1,25× p95 nur Bewertungsgrösse; F2-D5 offen.

## 1. Was der Modus tut (Hosttests: 71 Tests grün)

| Regel | Verhalten |
|---|---|
| Aktivierung | nur explizit mit `g1_capture.py --observe-network-timeouts` (reicht `observe` an `c0_load.py` weiter); **Standard unverändert der Hard-Stop** |
| Auslöser der Beobachtung | erster **echter, nicht abort-induzierter** Netzwerk-Timeout einer **Lastanfrage** (`timeout`, Phase `connect` oder `io`) |
| Folge | Lauf wird **unumkehrbar `FAIL/HOLD`**; Startzeit, Endzeit und Phase des ersten Timeouts werden protokolliert; die geplante Last wird **höchstens 20 s** weiter beobachtet (UART und Pcap laufen weiter; keine Retries, keine neuen Logins, keine zusätzliche Last) |
| Obergrenze | **harter Abbruch beim achten** echten Timeout (Timeouts nach dem Stopp zählen nicht) |
| Nach dem Fenster | Stop (`observation-window-end`); endet die geplante Last vorher, Stop `observation-load-end` **ohne** Recovery |
| Sofortiger Hard-Stop (auch im Modus) | erstes `heap_alloc_failed`, Panic, Watchdog, Brownout, unerwarteter Reset, Heartbeat-Lücke > 5 s, Stack-Grenzverletzung (Main < 11 160 B, HTTPD < 2 080 B), Identitäts-/Toolabweichung, HTTP-Statusfehler, **jeder andere** Request-Fehler (`connection-error`, nicht abort-induzierter `http-protocol-error`), **Login-/Auth-Fehler**, Supervisor-/Mitschnittversagen |
| Volumenschutz | > 20 000 Samples oder > 256 MiB UART-Rohdaten stoppen **fail-closed** (`sample-volume-limit`, `log-volume-limit`); es werden keine UART-Zeilen verworfen |
| Evidence | `load.json` `observation`: `first_network_timeout`, alle `network_timeouts` (mit `after_stop`), Zähler vor dem Stopp, `stop_reason`, `observation_seconds`, `final_state`; `capture.json` `observation_mode`; erste Anomalie, Folgefehler, Stopgrund, Beobachtungsdauer und Endzustand getrennt |

20 s und acht Timeouts sind **konservative Diagnosegrenzen** des Owners, **keine** Sicherheits- oder Abnahmeschwellen des Produkts; bei einem Timeout-Sturm darf der Lauf früher enden. Hosttests belegen: Standard-Hard-Stop unverändert, Einzel-Timeout mit Nachbeobachtung (`io` und `connect`), Sturm mit Abbruch beim achten, Sofortstopps auch im Modus (Heap, Panic, Reset, Heartbeat/Uptime), UART-/Supervisor-Stopp gewinnt und bricht den Timer ab, abort-induzierte Fehler werden nicht als Timeouts gezählt, Timeouts nach dem Stopp nur protokolliert, saubere Timer-/Thread-Beendigung, Login-Timeouts nie beobachtet.

## 2. Gepinnter Stand

| Gegenstand | Wert |
|---|---|
| Plan / Gates | `docs/tasks/issue-27-res01-f2-escalation-plan.md` @ `3bb29481495c4baca540ada1f3ae5433a5b46a34`; `G1B_STAGE_E_GATE.md` @ `188ddfc2…`; `G1B_B2_GATE.md` @ `f69ae08…` |
| Host-Werkzeuge | **neuer** Werkzeugcommit **`373bafb948d15f25cb8668981c53bf905003ae99`** (Review-Branch `agent/pr203-res01-g1-review-nonmerge`), Manifest `diag/review/g1b/HOST_TOOLS_SHA256SUMS`; gegenüber `07d1526` geändert: `c0_load.py`, `g1_capture.py` und deren Tests, `G1B_TEST_SHEET.md`; `g1_pcap.py` unverändert; Standardverhalten ohne Flag unverändert; spätere Commits nur Doku (H0: `git diff 373bafb… HEAD -- 'diag/*.py'` leer, Manifest OK) |
| Image (installiert, **nicht** neu geschrieben) | Image B: App `4145dddb272e639c9a055df8f9877d4395af32702f15ba92d4f43dae834c7b68` (1 760 576 B), ELF `06c1fb10d72cc4c8ecaf47ef86675f1b305b25035cf62b8cd89811ff3f9d4ef7`, Quelle `31c3ee48b52f44852b2e5cd5f542ba2fce614537`, `CONFIG_LWIP_TCP_SND_BUF_DEFAULT=2880`; unverändert seit E-FB (Verifikation 2026-10-15 06:52:30 UTC), danach nur UART-Läufe B1 und B2, keine esptool-/Flash-Aktion |
| Lastprofil | P-A unverändert: vier Sessions, 60 s Idle, höchstens 1200 s Last, 300 s Recovery (nur bei fehlerfreiem Lauf), Login-Timeout 8 s, übrige 3 s, kein Logout/Retry |
| Mitschnitt | `g1_pcap.py` unverändert, private tcpdump-Kopie (`cap_net_raw=ep`) nur über temporären `PATH`, Interface `enp2s0`, `--max-seconds` 1961 |
| Ausgabeordner (privat 0700) | `/var/lib/docker/data/ESP32-Projekte/hw-evidence-pr203/diag/g1b/stage_e_20261015/run_obs1/` |
| Gate-ID | `PR203_RES01_G1B_OBS1` |

## 3. Geräteaktionen (nur unter ausdrücklicher Ownerfreigabe)

| Nr. | Aktion | Wirkung |
|---|---|---|
| 1 | Port-Öffnung durch `g1_capture.py --observe-network-timeouts` (bekannter FT232R-Reset) | **1 Reset = erfasster Testboot** |
| 2 | P-A-Last über HTTP im Beobachtungsmodus | nur Netzwerklast, keine Schreib-/Konfigurationszugriffe |

Nicht Teil: esptool-Sitzung, Flash, Rückflash, weiterer Reset, Touch, P1–P3. Identität wie im B2-Gate (Operator bestätigt schriftlich, dass seit E-FB nichts geflasht oder verändert wurde; UART zeigt Quelle `31c3ee48…`, `tcp tx win: 2880`, Release-Profil, `real actuators: disabled`, `HomeConnected`); bei Zweifel HOLD.

## 4. Ablauf (wie B2, mit einem zusätzlichen Flag)

H0 wie `G1B_B2_GATE.md` Abschnitt 4 (jetzt gegen Werkzeugcommit `373bafb…`), dann: (1) `g1_capture.py --run --gate-id PR203_RES01_G1B_OBS1 --allow-port-open-boot --expected-source 31c3ee48b52f44852b2e5cd5f542ba2fce614537 --output <Ausgabeordner>/candidate_capture --load --observe-network-timeouts`; (2) Bereit ≤ 120 s; (3) `g1_pcap.py capture … --stop-file …/ABORT --stop-file …/STOP`, `PCAP_RUNNING` ≤ 30 s; (4) erst dann `START_LOAD`; (5) Endsignal: Supervisor/Treiber schreiben `ABORT` am Ende der Beobachtung oder bei Sofortstopp; bei fehlerfreiem Laufende legt der Operator spätestens 60 s nach `COMPLETED_PENDING_REVIEW` `STOP` an. Danach **keine** Geräteaktion.

## 5. Vorab festgelegte Einordnung

| OBS1-Ergebnis | Einordnung |
|---|---|
| **O-A** fehlerfrei bis 1200 s + Recovery | gleiche harten Kriterien wie B2-Gate Abschnitt 6 A; O1 nicht qualifiziert (kein A-Bezug) |
| **O-B** echte Netzwerk-Timeouts, Fenster oder Obergrenze erreicht | `FAIL/HOLD`; Auswertung der **Stillstandsphase**: Dauer (Pcap/UART), Wiederholung, Zahl und Phase der Timeouts, Erholung innerhalb des Fensters (Antwortlatenzen danach), Heap-/Gate-Verlauf, Verbindungs-/Sitzungssicht; **keine** Abnahme-/Recovery-Aussage; **kein** automatischer Schluss auf Backlog-Sättigung |
| **O-C** erstes `heap_alloc_failed` | sofortiger Stopp; Backtrace dekodieren; Rückfall in den Plan |
| **O-D** anderes (Login-/Statusfehler, Panic, Reset, Heartbeat/Stack, Volumenlimit, Werkzeug-/Mitschnittfehler) | HOLD, eigene Bewertung |

B2 hat gezeigt, dass der Hard-Stop die Stillstandsphase nach 100,7 s beendet; OBS1 soll Messdaten über den ersten Fehler hinaus liefern, **nicht** die Kriterien lockern. Kein weiterer Lauf ohne Ownerentscheidung nach Sichtung.

## 6. Veröffentlichung

Nur abgeleitete Zähler und Auszüge wie B1/B2 (`load_summary.json` mit `observation`, `res01_http_phases.csv`, `g1_gate_series.csv`, `capture.json`, `pcap_result.json`, `pcap_analysis_derived.json`, UART-Endauszug, `PRIVATE_SOURCE_SHA256.json`); **nie** Pcap, Passwort, Cookies, vollständige UART-Rohdaten. Datenschutzgrenzen unverändert.

## 7. Ownerbestätigungen (einzeln, vor dem Lauf; alle `PENDING`)

| # | Bestätigung | Wert |
|---|---|---|
| 1 | OBS1 (genau ein Diagnoselauf im Beobachtungsmodus, Werkzeugcommit `373bafb…`) freigeben | `PENDING` |
| 2 | Aktorfreier Devboard-Aufbau vor Ort bestätigt; Owner anwesend | `PENDING` |
| 3 | Genau ein Reset (Port-Öffnung/Testboot); kein Flash, kein Rückflash, keine esptool-Sitzung | `PENDING` |
| 4 | Seit E-FB hat niemand das Gerät geflasht oder anderweitig verändert | `PENDING` |
| 5 | 20 s / acht Timeouts als **Diagnosegrenzen** (keine Abnahmeschwellen) und Einordnung O-A bis O-D akzeptiert | `PENDING` |
| 6 | Kein weiterer Lauf, kein dritter identischer B-Lauf, F2-D4 Bewertungsgrösse, F2-D5 offen | `PENDING` |
