# G1b – OBS1-Neuversuch (genau ein Diagnoselauf im Beobachtungsmodus; Vorschlag zur Ownerfreigabe)

`OBS1_RETRY_GATE=PREPARED` · `OBS1_RETRY_HARDWARE=PENDING` · `OBS1=HOLD_NO_MEASUREMENT` · `O1_QUALIFICATION=NOT_PROVEN` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`

Dieses Dokument gewährt keine Freigabe. Es deckt **höchstens einen** Diagnoselauf ab (**OBS1R**), den Neuversuch nach dem Abbruch im Testboot (`OBS1_ATTEMPT_RESULT.md` @ `bc29fd1deb8cc84c0618424c6a5aef9ca9698abb`, **unverändert** als `HOLD_NO_MEASUREMENT` dokumentiert: Werkzeug-Artefakt durch gepufferte Altdaten, keine Last, kein Pcap). Der Beobachtungsmodus und seine Regeln sind **unverändert** (`G1B_OBSERVE_GATE.md` Abschnitt 1, Gate-Review PASS); geändert ist ausschliesslich der Bootmarker-Parser in `g1_capture.py`. Keine Firmware-, Image-, O1-, F4-, HTTPD-, Konfigurations-, Last- oder Pcap-Änderung. Kein weiterer Lauf, kein Flash/Rückflash/esptool, keine Folgeaktion ohne neue ausdrückliche Ownerfreigabe. F2-D4: 1,25× p95 nur Bewertungsgrösse; F2-D5 offen.

## 1. Korrektur des Bootmarker-Parsers (einziger Unterschied zum geprüften Werkzeugstand `373bafb`)

| Regel | Verhalten |
|---|---|
| Akzeptierter Testboot | **erster** Resetmarker `rst:0x1 (POWERON_RESET)` **innerhalb von 10 s ab der tatsächlichen Portöffnung**. Weder das Zeitfenster allein noch eine andere Ursache genügt. |
| Zustand beim Marker | alle zuvor gesammelten Zustände werden **verworfen**: `last_uptime`, `last_heartbeat`, Quelle, Bereitschaft, Aktoren, Profil, Netzwerk. Erst die **nachfolgenden** Zeilen können die Bereitschaft für `START_LOAD` belegen. |
| Zeilen vor dem Marker | bleiben **vollständig im Log** (nichts gefiltert); sie werden nur auf Sicherheitsereignisse geprüft (`heap_alloc_failed`, Panic, Watchdog, Brownout, Stack-Grenzen) und beweisen nichts über den neuen Boot. |
| Weiterhin Hard-Stop | zweiter Reset, Reset nach 10 s, anders begründeter Reset (`SW_RESET`, `SW_CPU_RESET`, `TG0WDT_SYS_RESET`, `rst:0x1` ohne `POWERON_RESET`, …), Uptime-Rückgang nach dem Boot, Heartbeat-Lücke > 5 s, Stack-Verletzung, `heap_alloc_failed`/Panic/Watchdog/Brownout (vor **und** nach dem Marker), Quellabweichung nach dem Marker |
| Ohne frischen Bootbeleg | **HOLD**: ohne erkannten Marker zählen Heartbeats und Identitätszeilen nicht; der Lauf endet fail-closed (`uart-heartbeat-timeout` bzw. `boot-identity-state-not-proven`), `START_LOAD` wird nie ausgeführt |
| Nebenkorrektur | ein noch nicht zeilenvollständiger `rst:0x`-Marker (Chunkgrenze mitten in der Zeile) wartet auf sein Zeilenende, statt fälschlich als Anomalie zu stoppen; ein zweiter, geteilter Marker bleibt ein Hard-Stop |
| Evidence | `capture.json`: `boot_marker` (Ursache, Sekunden nach dem Öffnen, `lines_before_marker`) |

Hosttests (82 grün, mehrfach wiederholt): Altdaten und alte Identitätszeilen vor dem erwarteten Reset mit frischem korrektem Boot (a); alte Identität beweist keinen frischen Boot (a2); Marker über Chunkgrenze (a3); frischer Boot ohne eigene Identität (a4); zweiter Reset, vollständig und geteilt (b); erster Reset nach 10 s (c) und knapp innerhalb 9,9 s (c2); andere Resetursachen (d); Uptime-Rückgang nach erkanntem Boot (e); fehlender Marker ohne belastbaren Bootbeleg (f); Sicherheitsanomalien vor und nach dem Marker (g); alle bisherigen Tests des Beobachtungsmodus und der Standardmodus unverändert.

## 2. Gepinnter Stand

| Gegenstand | Wert |
|---|---|
| Plan / Gates | Plan `docs/tasks/issue-27-res01-f2-escalation-plan.md` @ `3bb29481495c4baca540ada1f3ae5433a5b46a34`; `G1B_OBSERVE_GATE.md` @ `b034577568d0d6cfac7a6b6c52aad0262a8bda20` |
| Host-Werkzeuge | **neuer Werkzeugcommit `16d03498ac25f9e8fa9ce7b6101324438e60dd68`** (Review-Branch `agent/pr203-res01-g1-review-nonmerge`), Manifest `diag/review/g1b/HOST_TOOLS_SHA256SUMS`; gegenüber `373bafb` geändert: **nur** `g1_capture.py`, `test_g1_capture.py`, `G1B_TEST_SHEET.md` (`c0_load.py`, `g1_pcap.py` byteidentisch). H0: Baum sauber, `git diff 16d0349… HEAD -- 'diag/*.py'` leer, `sha256sum -c` OK. |
| Image (installiert, **nicht** neu geschrieben) | Image B: App `4145dddb272e639c9a055df8f9877d4395af32702f15ba92d4f43dae834c7b68`, ELF `06c1fb10d72cc4c8ecaf47ef86675f1b305b25035cf62b8cd89811ff3f9d4ef7`, Quelle `31c3ee48b52f44852b2e5cd5f542ba2fce614537`, `CONFIG_LWIP_TCP_SND_BUF_DEFAULT=2880`. Seit E-FB (Verifikation 2026-10-15 06:52:30 UTC) nur UART-Läufe B1, B2 und der OBS1-Versuch; **keine** esptool-/Flash-Aktion. Das Gerät läuft seit dem Testboot des OBS1-Versuchs (2026-10-15 09:53:17 UTC) im Leerlauf. |
| Lastprofil | P-A unverändert (vier Sessions, 60 s Idle, höchstens 1200 s Last, 300 s Recovery nur bei fehlerfreiem Lauf, Login-Timeout 8 s, übrige 3 s, kein Logout/Retry) |
| Mitschnitt | `g1_pcap.py` unverändert, private tcpdump-Kopie (`cap_net_raw=ep`) nur über temporären `PATH`, Interface `enp2s0`, `--max-seconds` 1961 |
| Ausgabeordner (privat 0700) | `/var/lib/docker/data/ESP32-Projekte/hw-evidence-pr203/diag/g1b/stage_e_20261015/run_obs1_retry/` (neu, der Abbruchversuch `run_obs1/` bleibt unberührt) |
| Gate-ID | `PR203_RES01_G1B_OBS1_RETRY` |

## 3. Geräteaktionen (nur unter ausdrücklicher Ownerfreigabe)

| Nr. | Aktion | Wirkung |
|---|---|---|
| 1 | Port-Öffnung durch `g1_capture.py … --observe-network-timeouts` (bekannter FT232R-Reset) | **1 Reset = erfasster Testboot** (neuer Reset, der Reset des Abbruchversuchs ist verbraucht) |
| 2 | P-A-Last über HTTP im Beobachtungsmodus | nur Netzwerklast |

Nicht Teil: esptool-Sitzung, Flash, Rückflash, weiterer Reset, Touch, P1–P3. Identität: Der Operator bestätigt schriftlich, dass seit E-FB nichts geflasht oder verändert wurde und seit dem OBS1-Versuch (09:53 UTC) niemand am Gerät gearbeitet hat; nach dem Marker zeigt der UART Quelle `31c3ee48…`, `tcp tx win: 2880`, Release-Profil, `real actuators: disabled`, `HomeConnected`. Bei Zweifel oder Abweichung HOLD.

## 4. Ablauf

H0 wie `G1B_B2_GATE.md` Abschnitt 4 (gegen Werkzeugcommit `16d0349…`). (1) `g1_capture.py --run --gate-id PR203_RES01_G1B_OBS1_RETRY --allow-port-open-boot --expected-source 31c3ee48b52f44852b2e5cd5f542ba2fce614537 --output <Ausgabeordner>/candidate_capture --load --observe-network-timeouts`. (2) **Bereit (≤ 120 s)** = nach dem akzeptierten Resetmarker (`capture.json` bzw. Log: erster `rst:0x1 (POWERON_RESET)`) folgen **in dieser Reihenfolge nachfolgende** Zeilen Quelle, `application: ready`, `real actuators: disabled`, Release-Profil, `HomeConnected`, frischer Heartbeat und `tcp tx win: 2880`; Altdaten vor dem Marker zählen nicht; sonst `STOP` und HOLD. (3) `g1_pcap.py capture … --stop-file …/ABORT --stop-file …/STOP`, `PCAP_RUNNING` ≤ 30 s. (4) erst dann `START_LOAD`. (5) Endsignal wie im Observe-Gate (Supervisor/Treiber `ABORT`, bei fehlerfreiem Ende `STOP` spätestens 60 s nach `COMPLETED_PENDING_REVIEW`). Danach **keine** Geräteaktion.

**Beobachtungsmodus (unverändert, Diagnosegrenzen des Owners, keine Abnahmeschwellen):** erster echter, nicht abort-induzierter Last-Timeout (`connect`/`io`) ⇒ unumkehrbar FAIL/HOLD, höchstens 20 s Nachbeobachtung ohne Retries/Logins/Mehrlast, harter Abbruch beim achten; sofortiger STOP/HOLD bei erstem `heap_alloc_failed`, Panic, Watchdog, Brownout, Reset, Heartbeat-Lücke > 5 s, Stack-Verletzung, Identitäts-/Toolabweichung, HTTP-Statusfehler, jedem anderen Request-Fehler, Login-/Auth-Fehler, Supervisor-/Mitschnittversagen; Volumenlimits fail-closed; nie PASS nach einem echten Fehler.

## 5. Einordnung (unverändert O-A bis O-D)

O-A fehlerfrei bis 1200 s + Recovery (O1 nicht qualifiziert); O-B echte Netzwerk-Timeouts mit Auswertung der Stillstandsphase (Dauer, Wiederholung, Erholung im Fenster, Heap-/Gate-Verlauf, Verbindungs-/Sitzungssicht; kein automatischer Schluss auf Backlog-Sättigung); O-C erstes `heap_alloc_failed` (sofort, dekodiert); O-D anderes (inkl. erneuter Parser-/Werkzeugfehler: HOLD, keine Wiederholung). Kein weiterer Lauf ohne Ownerentscheidung.

## 6. Veröffentlichung

Nur abgeleitete Zähler und Auszüge wie B1/B2/OBS1 (`load_summary.json` mit `observation`, `res01_http_phases.csv`, `g1_gate_series.csv`, `capture.json` mit `boot_marker`, `pcap_result.json`, `pcap_analysis_derived.json`, UART-Endauszug, `PRIVATE_SOURCE_SHA256.json`); nie Pcap, Passwort, Cookies, vollständige UART-Rohdaten.

## 7. Ownerbestätigungen (einzeln, vor dem Lauf; alle `PENDING`)

| # | Bestätigung | Wert |
|---|---|---|
| 1 | OBS1R (genau ein Diagnoselauf, Werkzeugcommit `16d0349…`) freigeben | `PENDING` |
| 2 | Aktorfreier Devboard-Aufbau vor Ort bestätigt; Owner tatsächlich anwesend | `PENDING` |
| 3 | Genau ein Reset (Port-Öffnung/Testboot); kein Flash, kein Rückflash, keine esptool-Sitzung | `PENDING` |
| 4 | Seit E-FB (und seit dem OBS1-Versuch) hat niemand das Gerät geflasht oder anderweitig verändert | `PENDING` |
| 5 | 20 s / acht Timeouts als Diagnosegrenzen und Einordnung O-A bis O-D akzeptiert; Bootmarker-Regel (Abschnitt 1) akzeptiert | `PENDING` |
| 6 | Kein weiterer Lauf, kein dritter identischer B-Lauf; F2-D4 Bewertungsgrösse, F2-D5 offen | `PENDING` |
