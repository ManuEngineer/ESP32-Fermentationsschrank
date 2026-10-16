# PR #203: Offline-Fix des G2-UART-Monitors

Stand: Diagnosebranch `agent/pr203-res01-g1-diag-local`, Basis `6abd685f51ca69f5e6a5b10a332ed014351d8e43`.

## Korrektur

`diag/g1_capture.py` wertet Resetmarker im unvollständigen UART-Puffer nicht mehr als Anomalie. Sobald eine Zeile vollständig ist, gilt nach M1: Ausschliesslich die erste vollständige `rst:0x1 (POWERON_RESET)`-Zeile (gegebenenfalls mit ROM-`boot:0x…`-Suffix) vor dem ersten Heartbeat und innerhalb der bestehenden initialen 5-s-Heartbeat-Frist wird akzeptiert. Andere Ursachen, unvollständige oder widersprüchliche Ursachenangaben, ein weiterer Reset oder ein Reset nach Heartbeat führen zu `uart-unexpected-reset`. Die 30-s-Frist für den vollständigen Bootidentitätsnachweis erweitert das Resetfenster nicht. Die übrigen `BAD`-Marker, einschliesslich Panic, OOM, WDT, Brownout und Stackoverflow, werden auch in Teilpuffern weiter fail-closed erkannt.

Die Main-Stack-HWM-Grenze von 11'160 B bleibt im Standardmodus (`strict`) ein Hard-Stop und gilt damit weiter für G1 und andere Läufe. Nur mit der exakten Gate-ID `PR203_SINGLE_FLIGHT_G2_WEB_20261016_IMAGE01` kann `g2-trend` gewählt werden. Dieser Modus protokolliert den beobachteten Main-Stack-HWM und eine Unterschreitung der Grenze, stoppt deswegen aber nicht. Die HTTPD-Stack-Grenze von 2'080 B bleibt in allen Modi ein Hard-Stop. Panic, WDT, Reset und andere UART-Anomalien bleiben unabhängig von der Stack-Policy Hard-Stops.

Geänderte Diagnosedateien:

- `diag/g1_capture.py` SHA-256 `3af98e77234efb90f7fafa1e9488702a65851efabf55055b7ddf44014fbd32d9`
- `diag/test_g1_capture.py` SHA-256 `b4e14903bd1cdfaac1493aa766ab443621bda596d4ed3c943257a55c10efe995`

## Offline-Verifikation

`python3 -m unittest -v test_g1_capture test_c0_load` aus `diag/`: **44 Tests PASS** (20 Capture- und 24 Runner-Tests). Die bestehenden Teilpuffer-, Heartbeat-, Stack- und Abort-Regressionen bleiben enthalten. Für M1 wird der echte `capture()`-Pfad mit Fake-Serial und kontrollierter monotoner Zeit ausgeführt; keine reale 5-s-Wartezeit ist dafür erforderlich.

Der nach M1 korrigierte Parser wurde offline mit dem lokal gespeicherten Originalmitschnitt, dessen 27 ursprünglichen Chunkgrenzen und den gespeicherten monotonen Chunkzeiten wiedergegeben. Fake-Serial und ein ersetzter lokaler Socket-Snapshot verhindern Geräte- und Netzwerkanfragen. Die vollständige erste `rst:0x1 (POWERON_RESET)`-Zeile lag im zweiten Chunk, rund 0,079 s nach dem ursprünglichen Capturestart und damit innerhalb der initialen 5-s-Frist. Die privaten Eingaben sind an UART-Rohdaten-SHA-256 `2394e493d7fff4d01a34f99b7dfb4cfa005237f71b15ea0f8601ca7edb3a5db3` und Chunkmanifest-SHA-256 `21d83781162e58fe0004231da10c9cb48eb068ccbfa2aefde54544dde487f18a` gebunden. Der Parser klassifizierte genau eine vollständige anfängliche `POWERON_RESET`-Zeile und keine weiteren Reset- oder erkannten Fatalmarker. Quellidentität, `real actuators: disabled`, Release-Profil und `HomeConnected` wurden erkannt. Der Main-Stack-HWM-Minimalwert im vorhandenen Mitschnitt war 11'704 B; die 11'160-B-Trendgrenze wurde dort nicht unterschritten.

**`ready` ist im Originalmitschnitt nicht bewiesen.** Die vorhandene Zeile lautet `application: eady`; eine vollständige `application: ready`-Zeile ist nicht vorhanden. Damit kann die Wiedergabe die Parserkorrektur gegen den Reset-Fehlalarm stützen, aber nicht die Voraussetzungen für den Browserlauf bestätigen. Die Wiedergabe bewahrt die aufgezeichneten Chunkzeiten, enthält aber auch den ursprünglichen Nachlauf nach STOP und ersetzt keine durchgehende Live-Überwachung oder neue Heartbeat-Qualifikation. Originalmitschnitt und Wiedergabe bleiben privat; sie sind nicht Bestandteil dieses Branches.

## M1: gezieltes Review-Delta

Korrekturbasis `4ecd1737b5fe699ef46b27e06e6ee86955e78fd6`; kumulative Diagnosebasis weiterhin `6abd685f51ca69f5e6a5b10a332ed014351d8e43`. Das Produkt-HEAD bleibt `0a665bbe659841883733a6c10b69f12c1462b543`.

Der Code-Diff für M1 ergänzt nur das vollständige Resetzeilenmuster und die Prüfung des Abschlusszeitpunkts gegen die bestehende initiale 5-s-Frist. Teilpufferprüfung, G2-Stack-Policy, HTTPD-Grenze, alle übrigen Hard-Stop-Marker und der begrenzte Nachlauf bleiben unverändert.

| Offline-Fall | Erwartung und Ergebnis |
|---|---|
| Erste gültige POWERON_RESET-Zeile vollständig oder über Reads geteilt | Akzeptiert: PASS |
| Erste SW_CPU_RESET-/RTCWDT-Ursache, widersprüchliche oder fehlende Ursache | `uart-unexpected-reset`: PASS |
| Gültige erste Resetzeile bei genau 5,000 s | Akzeptiert: PASS |
| Read innerhalb der Frist begonnen, vollständige Resetzeile erst bei 5,001 s | `uart-unexpected-reset`: PASS |
| Zweiter gültiger Reset / gültiger Reset nach Heartbeat | `uart-unexpected-reset`: PASS |
| Bestehende übrige Monitor-/Runner-Regressionen | PASS |
| Private Wiedergabe, 27 Chunks / 11 796 Bytes | Eine zulässige Resetzeile, kein Parserfehler; PASS nur für Offline-Klassifikation |
| Vollständiges `ready` aus diesen Rohdaten | NOT_PROVEN, unverändert |

Die Eingaben und bisherigen Ergebnisse wurden nicht verändert. Neues Replay, Testausgabe und Prüfsummen liegen separat privat; ein bereinigter Testnachweis ist [PR203_G2_UART_MONITOR_M1_20261016_TESTS.txt](PR203_G2_UART_MONITOR_M1_20261016_TESTS.txt). Keine Firmware gebaut oder Geräte-/Browseraktion ausgeführt. M1 ist bereit für unabhängige Fix Verification; die einmalige Ausführungsfreigabe steht weiter aus.

## Gate-Status

Der historische Monitor-STOP `uart-anomaly` bleibt **FAIL/HOLD** und wird nicht rückwirkend geändert. B0–B4 wurde bei diesem Lauf nicht ausgeführt. Diese Offline-Korrektur beweist weder einen Hardware-PASS noch einen B4-Root-Cause-Fix. Eine neue Portöffnung und Browserfortsetzung benötigen die gesonderte einmalige Ownerfreigabe; vor einem Browserlauf muss der korrigierte Live-Monitor `ready`, Kandidatenidentität, Safe Mode, `HomeConnected` und Heartbeats nachweisen. Andernfalls gilt STOP/HOLD.

`PR203_FINAL_GO=HOLD` · `G1_QUALIFICATION=NOT_PROVEN` · `ACTUATOR_RELEASE=NO`
