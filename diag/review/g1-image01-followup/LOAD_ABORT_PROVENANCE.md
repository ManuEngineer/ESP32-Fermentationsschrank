# G1 IMAGE01 – bereinigte Provenienz des ersten Abbruchgrunds

Ergänzt `FOLLOWUP_OFFLINE_ANALYSIS_20261014.md` (Commit `d4cbce8`); das versiegelte Paket `g1-image01/` (Stand `a361f51`) bleibt unverändert. Alle Angaben stammen aus lesenden Auszügen der privaten Dateien; es sind keine Cookies, Tokens, Passwörter, HTTP-Bodies oder Header enthalten (die Felder von `load.json` sind nur `abort`, Profilmetadaten, `marks` und je Sample `epoch/kind/session/status/milliseconds/error_class`).

## Auszug `candidate_capture/load/load.json`

| Feld | Wert |
|---|---|
| SHA-256 der Datei (identisch mit `EVIDENCE_PROVENANCE.md`) | `434901644d9b95c0a8dc4457c2fc6994778b1502774a50cc568147a591e561bf` |
| `status` / `evidence_gate` | `FAIL` / `HOLD` |
| **`abort`** (erster `Guard.abort`-Grund) | **`g1-supervisor-stop`** |
| Anzahl `samples` | 784 |

`marks` (UTC-Epoch, Host-monotonic in s):

| kind | epoch | monotonic |
|---|---|---|
| uart-heartbeat-wait | 1791986745.4903076 | 177950.452278 |
| uart-heartbeat-ready | 1791986746.295016 | 177951.256986 |
| login-start | 1791986746.295855 | 177951.257826 |
| four-session-idle-reference-start | 1791986762.569074 | 177967.531045 |
| four-session-idle-reference-end-load-start | 1791986822.5692623 | 178027.531234 |
| load-ended-client-sockets-closed-no-logout | 1791987134.4483693 | 178339.410341 |

## Weitere Auszüge

| Quelle | Inhalt |
|---|---|
| `candidate_capture/ABORT` (13 B, SHA-256 `35329095935cf661a95cbe581a2576fc0d08c288097dd47d3b75d23784a7dd6c`) | `uart-anomaly` |
| `candidate_capture/capture.json` | `failure = uart-anomaly`, `status = FAIL/HOLD` |
| Treiber `diag/c0_load.py` (SHA-256 `8c4db4272e9e3a49bac8fadfef2a0e05bb4ec6b151ef8914d7dce5312b6767a5`), `diag/g1_capture.py` (`ce459258e0b08f72e2b5b608a01f64d2c7663d092fabc597cc312f0c2f4c4de9`) | im Review-Branch und im gepinnten Bundle byteidentisch |

## Belastbarkeit der Aussage (Präzisierung zu `d4cbce8`)

- **Stark gestützt, nicht als unabhängig bewiesen:** Die sechs `http-protocol-error` sind Abbruchfolgen des Supervisor-Stopps. Tragend sind `abort = g1-supervisor-stop` (Guard-Code: nur der erste Grund wird gesetzt), die Codekette Marker → `ABORT` → Monitor → `close_sockets` und die Spreizung von 0,731 ms. Eine unabhängige Messung auf dem Draht gibt es nicht.
- **Zeitordnung:** Sie hängt an dieser Codekette, nicht an Gerätezeit. Die Dateizeiten (`mtime`) der privaten Dateien sind nur eine Plausibilitätsprüfung mit einer Auflösung von einigen ms und wurden nicht zu einer millisekundengenauen Gerätezeit erhoben. Auf dem Gerät ist die Reihenfolge Heapmarker/HTTP-Verhalten nicht auf unter 50 ms bestimmbar (Lesefenster des Hosts).
- **Siebter Fehler:** Der `timeout` (Session 1, `temperatures`, 3,001 s) bleibt ein realer Clientbefund. Seine netzwerk- bzw. geräteinterne Ursache ist offen.
