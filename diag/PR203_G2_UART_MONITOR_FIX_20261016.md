# PR #203: Offline-Fix des G2-UART-Monitors

Stand: Diagnosebranch `agent/pr203-res01-g1-diag-local`, Basis `6abd685f51ca69f5e6a5b10a332ed014351d8e43`.

## Korrektur

`diag/g1_capture.py` wertet Resetmarker im unvollständigen UART-Puffer nicht mehr als Anomalie. Sobald eine Zeile vollständig ist, bleibt die bisherige Regel aktiv: genau der erste Boot-Reset wird erwartet; jeder weitere Reset und jeder Reset nach einem Heartbeat stoppt. Die übrigen `BAD`-Marker, einschliesslich Panic, OOM, WDT, Brownout und Stackoverflow, werden auch in Teilpuffern weiter fail-closed erkannt.

Die Main-Stack-HWM-Grenze von 11'160 B bleibt im Standardmodus (`strict`) ein Hard-Stop und gilt damit weiter für G1 und andere Läufe. Nur mit der exakten Gate-ID `PR203_SINGLE_FLIGHT_G2_WEB_20261016_IMAGE01` kann `g2-trend` gewählt werden. Dieser Modus protokolliert den beobachteten Main-Stack-HWM und eine Unterschreitung der Grenze, stoppt deswegen aber nicht. Die HTTPD-Stack-Grenze von 2'080 B bleibt in allen Modi ein Hard-Stop. Panic, WDT, Reset und andere UART-Anomalien bleiben unabhängig von der Stack-Policy Hard-Stops.

Geänderte Diagnosedateien:

- `diag/g1_capture.py` SHA-256 `a0ece489d0bbaa2bfdedfbf2e0935f968c0e71660ba5d87157a91b8f3ef6665c`
- `diag/test_g1_capture.py` SHA-256 `320ee2fcda5b18d9fed91ce218d14dc6106efcfba7cce532abeaf0b4a5871dd3`

## Offline-Verifikation

`python3 -m unittest -v test_g1_capture` aus `diag/`: **17 Tests PASS**. Die Regressionen decken geteilte und vollständige erste Resetzeilen, einen zweiten Reset, Reset nach Heartbeat, Teilpuffer-Hard-Stop-Marker, G1-Hard-Stop unter 11'160 B, G2-Trendprotokollierung, Gate-ID-Bindung und unveränderten HTTPD-Hard-Stop ab. `python3 -m py_compile diag/g1_capture.py diag/test_g1_capture.py` ist ebenfalls erfolgreich.

Der korrigierte Parser wurde offline mit dem lokal gespeicherten Originalmitschnitt und dessen 27 ursprünglichen Chunkgrenzen wiedergegeben. Die privaten Eingaben sind an UART-Rohdaten-SHA-256 `2394e493d7fff4d01a34f99b7dfb4cfa005237f71b15ea0f8601ca7edb3a5db3` und Chunkmanifest-SHA-256 `21d83781162e58fe0004231da10c9cb48eb068ccbfa2aefde54544dde487f18a` gebunden. Der Parser klassifizierte genau eine vollständige anfängliche `POWERON_RESET`-Zeile und keine weiteren Reset- oder erkannten Fatalmarker. Quellidentität, `real actuators: disabled`, Release-Profil und `HomeConnected` wurden erkannt. Der Main-Stack-HWM-Minimalwert im vorhandenen Mitschnitt war 11'704 B; die 11'160-B-Trendgrenze wurde dort nicht unterschritten.

**`ready` ist im Originalmitschnitt nicht bewiesen.** Die vorhandene Zeile lautet `application: eady`; eine vollständige `application: ready`-Zeile ist nicht vorhanden. Damit kann die Wiedergabe die Parserkorrektur gegen den Reset-Fehlalarm stützen, aber nicht die Voraussetzungen für den Browserlauf bestätigen. Die echte Zeitlücke zwischen Heartbeats lässt sich durch diese Offline-Wiedergabe ebenfalls nicht neu qualifizieren. Originalmitschnitt und Wiedergabe bleiben privat; sie sind nicht Bestandteil dieses Branches.

## Gate-Status

Der historische Monitor-STOP `uart-anomaly` bleibt **FAIL/HOLD** und wird nicht rückwirkend geändert. B0–B4 wurde bei diesem Lauf nicht ausgeführt. Diese Offline-Korrektur beweist weder einen Hardware-PASS noch einen B4-Root-Cause-Fix. Eine neue Portöffnung und Browserfortsetzung benötigen die gesonderte einmalige Ownerfreigabe; vor einem Browserlauf muss der korrigierte Live-Monitor `ready`, Kandidatenidentität, Safe Mode, `HomeConnected` und Heartbeats nachweisen. Andernfalls gilt STOP/HOLD.

`PR203_FINAL_GO=HOLD` · `G1_QUALIFICATION=NOT_PROVEN` · `ACTUATOR_RELEASE=NO`
