# G1b Stufe E – E-FB App-only-Flash von Image B: PASS (2026-10-15)

Freigabe: Owner, ausschliesslich Teilgate E-FB (`G1B_STAGE_E_GATE.md` @ `188ddfc292498d35b456cb39c26103613959efda`), aufbauend auf `E_P0=PASS` (`0f40530a1c58b8c6136827d4269340c7e6c78727`); aktorfreier Aufbau vom Owner bestätigt, Owner anwesend. Vorab geprüft: Review-Branch sauber, Werkzeug-Manifest und Artefakt-`SHA256SUMS` OK, B-Hash und Länge, Baseline intakt, kein anderer Prozess am Port. **Kein** Testboot mit UART-Aufzeichnung, **kein** Lauf, keine Last, kein Rückflash, keine weitere Geräteaktion; E-B1 bleibt gesperrt.

| Schritt | Befund |
|---|---|
| Schreibsitzung | eine esptool-v5.3.1-Sitzung 06:44:15–06:45:57 UTC: `write-flash --flash-mode keep --flash-freq keep --flash-size keep 0x10000 <B-App>`; **1 760 576 B bei `0x10000`** (1 103 000 komprimiert, 98,1 s), **`Hash of data verified`**, Exitcode 0; MAC `20:50:0d:1b:2f:34`, ESP32-D0WD-V3 |
| Verifikationssitzung | eine esptool-Sitzung 06:46:06–06:52:30 UTC: `read-flash 0x0 0x400000` (4 194 304 B), Exitcode 0 |
| Resets | 4 wie genehmigt (je Sitzung Reset in den Bootloader und Hard-Reset); nach dem zweiten Hard-Reset läuft B unbeobachtet (kein UART-Port geöffnet) |

## Nicht-App-Prüfung gegen die private E-P0-Baseline (alle bestanden)

| Prüfung | Ergebnis |
|---|---|
| Boot/NVS/PHY `0x000000–0x00FFFF` | **byteidentisch** |
| Partitionstabelle `0x8000` (3 072 B) | **byteidentisch** (`d7f180e4…ccd9d`) |
| Bereich hinter der App bis `0x2FFFFF` | **byteidentisch** |
| State-Store `0x300000–0x3FFFFF` | **byteidentisch** |
| App ab `0x10000`, 1 760 576 B | **= Image B**, SHA-256 `4145dddb272e639c9a055df8f9877d4395af32702f15ba92d4f43dae834c7b68` = Gate |
| Alle Abweichungen gegenüber der Baseline | liegen **ausschliesslich** im App-Bereich (`0x10030`–`0x1BDD3F`, innerhalb `0x10000` bis Ende von B) – erwartet, da A und B unterschiedliche Images sind |

Die privaten Dateien (`post_B_4MiB.bin`, Baseline) bleiben unveröffentlicht (Mode 0600, enthalten NVS/State-Store); Rohergebnis `verify_result.json` privat. Kein Reparaturflash, kein Erase-all, kein Schreiben ausserhalb `0x10000`.

`E_P0=PASS` · `E_FB=PASS` · `E_B1=PENDING` (gesperrt) · `G1B_STAGE_E_AUTHORIZATION=PENDING` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`. Auf dem Gerät ist jetzt Image B installiert; es bleibt unangetastet, bis der Owner über E-B1 oder einen Rückflash entscheidet.
