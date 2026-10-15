# G1b – Loopback-Trockentest (echtes tcpdump) – PASS

Datum 2026-10-15. Nur Loopback: lokaler Python-HTTP-Server auf `127.0.0.2:18080`, Clientverbindungen von `127.0.0.1`. **Kein** ESP32, kein UART, kein Reset, keine Last gegen das Gerät. Die Pcap-Datei blieb privat (Verzeichnis 0700 ausserhalb jedes Git-Worktrees) und wird nicht veröffentlicht; hier stehen nur abgeleitete Ergebnisse.

| Punkt | Wert |
|---|---|
| Host-Werkzeugstand | Review-Branch, Tool-SHA-256 von `g1_pcap.py` in `pcap_result.json`: `64aa5ee8a12ddf59fb7cf6b6b73db7cbf1a08d19061979f78381d5cd5584df4d` (siehe `HOST_TOOLS_SHA256SUMS`) |
| tcpdump | private Kopie `/home/manuel/g1b-tools/tcpdump` (SHA-256 `0d426e2571a22de0d30996fd01bb44c1f267f33065023abea092c88bad16d2d2`), `getcap`: `cap_net_raw=ep`, **ohne** `CAP_NET_ADMIN`; per temporärem `PATH` aufgerufen (`command -v tcpdump` = private Kopie); Aufruf mit `-p -n -U -s 54` |
| Ablauf | `capture` (Exitcode **0**, `PASS`, Grund `stop-signal`) mit `--stop-file …/STOP`; `PCAP_RUNNING` erschien erst nach laufendem tcpdump; 12 Requests (davon 4 mit ≈ 30 kB Antwort) mit Markern in Pfad, Cookie und Body; danach `STOP` |
| Prozessabschluss | Capture-Prozess beendet, `tcpdump_returncode 0`, danach kein tcpdump-Prozess mehr |
| Paketzahl | `packets_verified` 144 = `packets_reported_by_tcpdump` 144 |
| Payloadfrei | `g1_pcap.py verify`: 144 Pakete payloadfrei; unabhängig: 144 Records, **maximale gespeicherte Länge 54 B**, Dateigrösse = 24 + 16·N + Σ gespeicherte Längen (10 104 B); **0 Treffer** für `PAYLOAD_MARKER`, `GET /`, `Cookie` in der Datei |
| `analyze` | 12 Verbindungen, 24 Geräte-Datensegmente, 0 Retransmits/Dupacks/Zero-Window/SYN-Wiederholungen/RST, `synack_delay_ms` Median 0,0 / Max 0,1, `time_to_first_reply_ms` Median 1,1 / Max 5,1; UTC-Anker `2026-10-15T03:58:43.963554Z` bis `2026-10-15T03:58:46.191787Z`; `event_window` mit Ereigniszeit in der Mitte: `covered=true`, alle Gruppen 0; **keine Adressen/Ports** im Ergebnis |
| Negativlauf (echt) | Gleicher Aufruf mit dem Systembinary (ohne Rechte): Exitcode **3**, `HOLD`, Grund `tcpdump-exited-at-start`, kein tcpdump-Rest |

`HOST_CAPABILITY=PASS` · `LOOPBACK_TEST=PASS` · `G1B_AUTHORIZATION=PENDING` · keine Geräteaktion.
