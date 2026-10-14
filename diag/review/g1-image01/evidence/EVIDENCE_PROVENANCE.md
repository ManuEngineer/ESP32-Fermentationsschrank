# Evidence provenance, redactions, and omissions

All source artifacts remain unchanged in the private run directory. This publication contains selected evidence only. Hashes below identify the exact local sources used; they do not publish the sources themselves.

| Local source | SHA-256 | Use in public package |
|---|---|---|
| `candidate_capture/uart_raw.bin` | `86e8c5ba5514def89894305bfab05016fc4d877d5bc60abe5514914104031dad` | byte-exact failure-window excerpt only; complete UART raw file withheld |
| `candidate_capture/uart.log` | `f537253564744125920b950c0d687e61c82a62f842a8285d609f30543eee5e64` | selected UI/gate/heap/stack/resource records and timestamped heartbeat rows; unrelated lines omitted |
| `candidate_capture/load/load.json` | `434901644d9b95c0a8dc4457c2fc6994778b1502774a50cc568147a591e561bf` | HTTP samples reduced to time/category/status/duration/error class; no bodies or headers existed in samples |
| `candidate_capture/capture.json` | `298a1ee5c280cb07661af57fc5fd8caa02e74b159266dfd9813972c839ab8f85` | capture summary copied; contains stop and supervision metadata |
| `g1_ui_analysis.json` | `ea38ada3e58c14e086fac3b13ba8b94af8211b9b0d6c2157a8f1e249d91b9b6b` | UI/F4 summary copied |
| `app_flash.log` | `70d14d2fc5d4c5f111bf3a8a1cd46239dd54905d281243b63c98e4cedf9b92dc` | identity facts summarized from successful write/hash-verification output; raw tool log withheld |
| `preflight_uart.log` | `4ec4a656f3b5470e0321faaea6144fc0d234d1d83f399d40a61ffe741e33cd2c` | board/source/actor state used for identity summary; raw preflight log withheld |


## Sanitization

- No app/ELF binary, full flash readback, NVS/PHY/private flash bytes, HTTP body, authorization header, Cookie, CSRF value, password, token, or credential is included.
- `HTTP_SAMPLES.csv` contains only the request index, UTC, reconstructed monotonic estimate, session ordinal, request kind, status, elapsed milliseconds, and non-secret error class.
- `UART_RESOURCE_RECORDS.csv` and `UART_HEARTBEATS.csv` retain selected original diagnostic payloads and timing; unrelated startup/network/security lines are omitted. The raw failure excerpt is copied byte-for-byte and ends at the same incomplete frame as the source capture.
- `FLASH_IDENTITY.md` reports the pinned candidate hash and comparison outcomes but omits hashes of full private flash images and all readback bytes.
- Public `SHA256SUMS` covers the files in this package, excluding the manifest itself. The local source package and its original manifest were not modified.

These omissions limit review of the complete UART history and physical full-flash bytes. The published records preserve the actual stop marker, adjacent capture window, HTTP failures, diagnostic measurements, hashes of the source records, and the known time-ordering uncertainty.
