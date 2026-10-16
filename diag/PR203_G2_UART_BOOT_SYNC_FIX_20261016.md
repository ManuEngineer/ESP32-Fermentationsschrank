# PR #203 – G2-Web-UART-Boot-Synchronisierung: Offline-Fix

Stand: 2026-10-16, Diagnosebranch `agent/pr203-res01-g1-diag-local`, Korrekturbasis `a5afe56104a9f5a8a5d673b23f5ba9cd68d54096`. Produkt-PR weiterhin unverändert auf `55b41a5bb60f9de048f2a28e4684fa13ed438347`; installiertes Produktimage weiterhin aus Quelle `a9a1156a1d44d4679957526bbc34a30be6422e4e`. Auftrag ist die gezielte Ownerkorrektur nach G2-Web-Folgelauf 01, keine neue Ausführungsfreigabe.

**Builder-Regressionen: PASS. Independent Fix Verification: PENDING. Live-Boot-/Browserqualifikation: NOT_PROVEN.**

## Korrekturdiff und unveränderte Grenzen

`diag/g1_capture.py` verwendet eine explizite Boot-Synchronisierungsmarke. Vor dem ersten qualifizierten vollständigen `rst:0x1 (POWERON_RESET)` (mit bereits zugelassenem ROM-Suffix) bleiben die Daten bytegetreu in `uart_raw.bin`, mit Chunkzeiten in `uart_chunks.jsonl` und als vollständige Zeilen im Parserlog erhalten. Sie setzen aber keine neue Heartbeat-/Uptime-/Quell-/Ready-/Aktor-/Profil-/Netzwerk-Evidence. Numerische Ressourcenstichproben werden ebenfalls erst nach der Synchronisierung dem neuen Boot zugeordnet. Es gibt kein Leeren des UART-Puffers, kein Wiederöffnen und keinen zusätzlichen Reset.

Fatalmarker werden vor dieser Ausblendung geprüft: vollständige und unvollständige Panic-/Heap-/WDT-/Brownout-/Stackoverflow-Marker bleiben vor und nach dem Marker fail-closed. Andere Resetursachen, ein zweiter Reset und Resets nach qualifizierten Heartbeats bleiben `uart-unexpected-reset`.

Die ursprünglichen Fristen werden nicht erweitert:

- Gültiger vollständiger initialer Marker höchstens 5 s nach Capturestart; fehlt er darüber hinaus: `uart-initial-reset-timeout`. Eine erst nach Fristablauf vollständig empfangene Resetzeile bleibt `uart-unexpected-reset`.
- Auch der erste qualifizierte Heartbeat muss innerhalb der bestehenden initialen 5-s-Frist liegen; der Reset startet kein zusätzliches Zeitbudget. Alte Zeilen verlängern diese Frist nicht. Anschliessend bleiben Heartbeatlücken über 5 s ein Hard-Stop.
- Vollständige neue Bootidentität innerhalb der bestehenden 30-s-Frist; fehlt sie: `boot-identity-state-not-proven`. Auch ein früher `STOP`-Marker darf ohne qualifizierten Reset, vollständige Identität und mindestens einen qualifizierten Heartbeat keinen Capture-Complete-Status erzeugen.

Das Capture-JSON ergänzt `boot_synchronized`, `initial_reset_monotonic`, `pre_boot_lines_ignored` und `qualified_heartbeats`. Diese Werte unterscheiden verworfene alte Nachweise von tatsächlich nach dem Marker beobachteter Evidence. `expected_initial_reset_lines` bleibt der bestehende Resetzeilenzähler, keine eigene PASS-Aussage.

G1/default bleibt `strict` mit Main-Stack-Hard-Stop unter 11'160 B. `g2-trend` bleibt ausschliesslich an die exakte Gate-ID gebunden und zeichnet diesen Main-Stack-Befund auf. Die HTTPD-Grenze von 2'080 B und sämtliche echten Stack-/Panic-/Resetmarker bleiben unverändert. Produktcode, Firmware, Lasttreiber, WLAN/TCP/HTTPD und Ausführungsfreigaben wurden nicht geändert.

## Gezielte Offline-Regressionen

Befehl aus `diag/`: `python3 -m unittest -v test_g1_capture test_c0_load`.

**52 Tests PASS** (28 Capture- und 24 bestehende Runner-Tests), ohne echte 5-s-Wartezyklen für die neuen Zeitfensterfälle. [Vollständige Testausgabe](PR203_G2_UART_BOOT_SYNC_20261016_TESTS.txt).

| Fall | Ergebnis |
|---|---|
| 88 gleiche alte Heartbeats, gültiger vollständiger bzw. gesplitteter Reset, neue Identität und zwei neue Heartbeats | PASS; alte Zeilen vollständig erhalten, nur zwei Heartbeats qualifiziert |
| Alte Heartbeats ohne Reset laufen über die Frist weiter | PASS; `uart-initial-reset-timeout`, kein Livenessnachweis |
| Gültiger Reset und Identität, aber keine neuen Heartbeats | PASS; `uart-heartbeat-timeout`, alte Heartbeats ersetzen keinen neuen Nachweis |
| Falsche/widersprüchliche Ursache, Reset bei 5,001 s, zweiter Reset, Reset nach qualifiziertem Heartbeat, Uptime-Rücksprung | PASS; weiterhin STOP/HOLD |
| Quelle, Profil, Aktorstatus, Ready oder Netzwerkstatus nur vor dem Marker; jeweils nachher fehlend | PASS; keine Übernahme, `boot-identity-state-not-proven` |
| Alte vollständige Identität ohne Reset / fehlendes Ready trotz weiterlaufender neuer Heartbeats bis zur Identitätsfrist | PASS; STOP/HOLD |
| Falsche alte Quelle / falsche neue Quelle | PASS; alte Quelle ignoriert, neue Abweichung stoppt |
| Vollständige und partielle Fatalmarker vor und nach dem Reset | PASS; `uart-anomaly` |
| Bestehende G1-strict-/G2-trend-/HTTPD-/Gate-ID-/Abort-/Timeout-Regressionen | PASS |

## Private Original-Wiedergaben

[Sanitisierte Replay-Ergebnisse](PR203_G2_UART_BOOT_SYNC_20261016_REPLAY.json) enthalten Eingabehashes, Chunkanzahl und erwartete Ergebnisse. Beide Originale wurden nur gelesen, ihre Hashes danach erneut verglichen. Serial und Socket-Snapshot sind ersetzt; keine UART-Portöffnung oder Geräte-/HTTP-Anfrage erfolgt. Die alten passiven Nachläufe sind Teil der Wiedergabe; sie werden hypothetisch mit dem neuen Parser verarbeitet. Originale und unveränderte historische Ergebnisdateien bleiben privat.

| Mitschnitt | Originale Chunks / Bytes | Ergebnis mit korrigiertem Parser |
|---|---|---|
| G2-Web-Folgelauf 01 | 30 / 13 393 | Erwartetes Offline-Ergebnis PASS: 90 Zeilen vor dem Marker ignoriert (88 Heartbeats plus ROM-/Leerzeilen), eine gültige Resetzeile, vollständige neue Identität und genau zwei neue Heartbeats; `CAPTURE_COMPLETE_PENDING_REVIEW` |
| Erster UART-STOP | 27 / 11 796 | Erwartete Ablehnung PASS: Marker synchronisiert und zwei neue Heartbeats erkannt, aber `ready=false`; Replay endet bei EOF/STOP mit `boot-identity-state-not-proven` / FAIL/HOLD |

Beim älteren 27-Chunk-Mitschnitt steht weiterhin nur `application: eady`; fehlende Rohdaten werden nicht ergänzt. Beim 30-Chunk-Mitschnitt wird die aus den Original-Chunkzeiten rekonstruierte Markerzeit mit rund 0,07615 s angesetzt. Der reale frühere Parsezeitpunkt war rund 0,077392 s; die Wiedergabe simuliert keine CPU-Zeit zwischen den Zeilen. Beide liegen innerhalb von 5 s. Keine dieser Wiedergaben ersetzt den neuen durchgehenden Live-Bootnachweis oder einen Browserlauf.

Quellbindung:

- `diag/g1_capture.py` SHA-256 `955f1c3b1cc12587a012b6ecae0740657cd991152090de801b0458944f9571c5`
- `diag/test_g1_capture.py` SHA-256 `b1b17cb4d6daad74130f5e5dbfde987fd04b147aa27b80e556e652cdd783fd4a`
- 30-Chunk-Rohstrom SHA-256 `afbe144c20838b5120cdbe5b85566754ccf921e644df216a2e403ab88781f3b1`
- 27-Chunk-Rohstrom SHA-256 `2394e493d7fff4d01a34f99b7dfb4cfa005237f71b15ea0f8601ca7edb3a5db3`

## Übergabe

Bereit für gezielte unabhängige Fix Verification dieser Boot-Synchronisierung. Kein Firmwarebuild, Flash, Reset, Portzugriff oder Browserlauf. Historischer G2-Web-Folgelauf bleibt FAIL/HOLD; B0–B4 bleibt NOT_RUN. Nach erfolgreicher Fix Verification ist die separate zusammenhängende Ownerfreigabe für genau eine neue Monitor-Portöffnung und bedingt anschliessend B0–B4 erforderlich, ohne Zwischenfreigaben bei erfolgreichen Phasen.

`G2_WEB=FAIL/HOLD` · `G2_RETRY=NOT_AUTHORIZED` · `G1_QUALIFICATION=NOT_PROVEN` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`
