# G1b Stufe E – E-P0 Baseline-/Identitäts-Preflight: PASS (2026-10-15)

Freigabe: Owner, ausschliesslich Teilgate E-P0 (`G1B_STAGE_E_GATE.md` @ `188ddfc292498d35b456cb39c26103613959efda`), Baseline-Variante *voll*. H0 bestanden (Review-Branch sauber, Werkzeug-Manifest und Artefakt-`SHA256SUMS` OK). **Keine** Last, kein Testboot, kein Flash, kein UART-Mitschnitt; kein weiteres Teilgate gestartet.

| Punkt | Befund |
|---|---|
| Aktion | eine esptool-v5.3.1-Sitzung, `read-flash 0x0 0x400000`, nur lesend; 05:30:22–05:36:47 UTC (Dauer ≈ 6 min 25 s) |
| Resets | 2 (Reset in den Bootloader, Hard-Reset in die Anwendung); danach kein weiterer |
| Chip/MAC | ESP32-D0WD-V3 rev v3.1, MAC `20:50:0d:1b:2f:34` (erwartet) |
| Partitionstabelle (3 072 B) | `d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d` = Gate (A und B identisch) |
| App im Gerät | Bereich ab `0x10000`, 1 760 560 B: `da9e868874649bd274d21a4f1c5715afd9293288497027004c460efc46df8cf0` = **Image A (G1 IMAGE01)**; Abweichung: keine |
| Baseline | 4 194 304 B, privat (`…/diag/g1b/stage_e_20261015/p0/baseline_4MiB.bin`, Mode 0600, nicht veröffentlicht); Teilhashes für Boot/NVS/PHY (`0x0–0xFFFF`), Bereich hinter der App bis `0x2FFFFF` und State-Store (`0x300000–0x3FFFFF`) liegen privat in `p0_identity.json` für die Nicht-App-Prüfung nach einem späteren Flash |

`E_P0=PASS` · `E_FB=PENDING` · `E_B1=PENDING` · `G1B_STAGE_E_AUTHORIZATION=PENDING` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`. Die Baseline bleibt bis zur Ownerentscheidung nach B1 erhalten (Gate Abschnitt 3.1).
