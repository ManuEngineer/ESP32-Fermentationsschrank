# PR #203 / RES-01 – G1 IMAGE01 Independent Review Index

**Gesamtergebnis: G1 IMAGE01 = FAIL/HOLD.** Der einzige freigegebene P-A-Lauf stoppte nach 311.879 s wegen `uart-anomaly`; der letzte unvollständige UART-Rohframe enthält `heap_alloc_failed`. Sieben HTTP-Requests scheiterten im selben Abschlussfenster. Die exakte Reihenfolge zwischen Heapmarker und HTTP-Fehlern ist nicht belegt. Keine Recoveryphase und keine weitere Geräteaktion erfolgten.

## Gepinnter Stand

- Gate: `PR203_RES01_G1_20261014_IMAGE01`; Review-Doku-Commit `9b8cc8f66b0a8ac511091e34440cccd82f16f290`.
- Produktbasis: `d35ee85edf4f93f20e9edbe13f8851e406d1b82b`.
- Diagnosequelle: `bb4f43a69740f234d26a951707991f792cc3ffde` (separat, nicht zum Merge).
- App SHA-256: `da9e868874649bd274d21a4f1c5715afd9293288497027004c460efc46df8cf0`.
- ELF SHA-256: `24d8557f965053d566ba6ef3c4bfe745044a505994835e5831b85ff0dbe8c369`.

## Review-Einstieg

- [G1_RESULT.md](G1_RESULT.md): tatsächlicher Lauf, Statusmatrix, Messwerte und Grenzen.
- [TIMELINE.json](TIMELINE.json): UTC-/monotone Ereignismarken und explizite Zeitlücken.
- [POST_RUN_UI_SMOKE.md](POST_RUN_UI_SMOKE.md): separater Owner-Smoke-Test, nicht G1-Coverage.
- [EVIDENCE_PROVENANCE.md](evidence/EVIDENCE_PROVENANCE.md): Quellhashes, bytegetreue Fehlerfenster, Ableitungen und Auslassungen.
- [FLASH_IDENTITY.md](evidence/FLASH_IDENTITY.md): gepinnte Imageidentität, App-only-Schreibvorgang und Nicht-App-Vergleich.
- [HTTP_SAMPLES.csv](evidence/HTTP_SAMPLES.csv): alle 784 bereinigten Request-Records ohne Request-/Response-Body oder Credentials.
- [UART_RESOURCE_RECORDS.csv](evidence/UART_RESOURCE_RECORDS.csv): relevante UI-, Gate-, Heap- und Stack-Messzeilen mit Hostzeit.
- [UART_HEARTBEATS.csv](evidence/UART_HEARTBEATS.csv): vom Host zeitmarkierte Heartbeats über den erfassten Zeitraum.
- [FAILURE_WINDOW_UART.bin](evidence/FAILURE_WINDOW_UART.bin): bytegetreuer Ausschnitt aus dem privaten Original-Rohmitschnitt mit den abschliessenden Diagnosezeilen und dem unvollständigen Heap-Fehlerframe.
- [FAILURE_WINDOW.md](evidence/FAILURE_WINDOW.md): Bytebereich und UTC-/Monotonic-Begrenzung des Fehlerfensters.
- [CAPTURE_SUMMARY.json](evidence/CAPTURE_SUMMARY.json), [UI_ANALYSIS.json](evidence/UI_ANALYSIS.json) und [CHECKPOINTS.csv](evidence/CHECKPOINTS.csv): Capture-Stop, UI-/F4-Messungen und Ressourcenstichproben.
- [SHA256SUMS](SHA256SUMS): Prüfsummen aller veröffentlichten Paketdateien ausser dem Manifest selbst.

## PASS / FAIL / NOT_PROVEN

| Status | Befund |
|---|---|
| **PASS** | Gepinnte Image-/ELF-Identität, einzelner App-only-Flash auf `0x10000`, App-Hashverifikation, Nicht-App-Bereiche im Vorher-/Nachhervergleich unverändert, Kandidatenboot/Actor-disabled, Idle-Referenz und beobachtete Stackminima. |
| **FAIL** | P-A HTTP: 777/784 HTTP 200, sechs `http-protocol-error`, ein Timeout; Heap-Allokationsfehler; Lauf nach 311.879/1,200 s abgebrochen. |
| **NOT_PROVEN** | 300-s-Recovery, exakter Zeitpunkt und Reihenfolge des unvollständigen Heapmarkers gegenüber HTTP-Fehlern, G1-01, reale Touch-/Hold-/Skip-Coverage und finale F4-Schwelle. |
| **PASS – separat** | Späterer, Owner-beobachteter UI-Smoke-Test: Home sichtbar und Menünavigation per Touch funktionierte. Nicht während G1 beobachtet, nicht monoton zeitgestempelt und nicht mit UART-/Probe-Rohdaten korreliert. |

## Datenschutz und Grenzen

Das veröffentlichte Paket enthält keine Zugangsdaten, Cookies, Tokens, HTTP-Bodies, vollständigen privaten Flash-Readbacks oder vollständigen UART-Aufzeichnungen. Das Fehlerfenster ist ein unveränderter Byteausschnitt aus dem lokalen UART-Rohmitschnitt; sein Quellhash und Bytebereich sind dokumentiert. Nicht veröffentlichte Rohdaten bleiben lokal unverändert. Auslassungen beeinträchtigen nicht die sichtbare Stop-Evidenz; sie begrenzen die unabhängige Prüfung vollständiger UART-Historie und des physischen Flash-Readbacks. `RES-01=BLOCKER`, `PR203_FINAL_GO=HOLD`, `G2=NOT_AUTHORIZED`, `ACTUATOR_RELEASE=NO`.
