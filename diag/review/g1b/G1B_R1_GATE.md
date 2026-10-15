# G1b – R1-Hardwaregate (Vorschlag zur Ownerfreigabe)

`G1B_AUTHORIZATION=PENDING` · `HOST_CAPABILITY=PASS` · `LOOPBACK_TEST=PASS` · `G1_IMAGE01=FAIL/HOLD` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`

Dieses Dokument gewährt keine Freigabe. Es gilt erst, wenn der Owner es ausdrücklich bestätigt (Abschnitt 6). Es umfasst **genau einen** erfassten Testlauf **R1**. R2 und R3 sind gesperrt und brauchen eine spätere, getrennte Entscheidung. Alle Aktionen unten sind Geräteaktionen im Sinn von `diag/G1B_TEST_SHEET.md` Abschnitt 3.

## 1. Gepinnter Stand

| Gegenstand | Wert |
|---|---|
| Plan (Ownerfreigabe F2-D1/D2/D3) | `docs/tasks/issue-27-res01-f2-escalation-plan.md` @ `3bb29481495c4baca540ada1f3ae5433a5b46a34` |
| Host-Werkzeuge | Review-Branch `agent/pr203-res01-g1-review-nonmerge`, Werkzeugcommit **`07d1526678c171cf4e51a37650c221922d842f32`**; Manifest `diag/review/g1b/HOST_TOOLS_SHA256SUMS` (7 Dateien); spätere Commits dürfen nur Doku ändern |
| Image (unverändert G1 IMAGE01, **kein Build, kein Flash**) | App `da9e868874649bd274d21a4f1c5715afd9293288497027004c460efc46df8cf0` (1 760 560 B); ELF `24d8557f965053d566ba6ef3c4bfe745044a505994835e5831b85ff0dbe8c369`; Diagnosequelle `bb4f43a69740f234d26a951707991f792cc3ffde` |
| Partitionstabelle (3 072 B) | `d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d` |
| Hook | `diag_alloc_fail` bereits im Image; `FLASH_REFS=0` (`diag/review/g1b/HOOK_STATIC_CHECK_G1_ELF.txt`) |
| Board | aktorfreies Devboard, FT232R `A5069RR4` (`/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A5069RR4-if00-port0`), erwartete MAC `20:50:0d:1b:2f:34`, ESP32-D0WD-V3; **Owner bestätigt vor Ort aktorfrei** (Abschnitt 6) |
| tcpdump | private Kopie `/home/manuel/g1b-tools/tcpdump`, SHA-256 `0d426e2571a22de0d30996fd01bb44c1f267f33065023abea092c88bad16d2d2`, `cap_net_raw=ep` (ohne `CAP_NET_ADMIN`); nur über temporären `PATH` |
| esptool | `v5.3.1` (`/home/manuel/.local/bin/esptool`) |
| Ausgabeordner (privat, 0700, ausserhalb jedes Git-Worktrees) | `/var/lib/docker/data/ESP32-Projekte/hw-evidence-pr203/diag/g1b/run_20261015_r1/` |
| Gate-ID | `PR203_RES01_G1B_R1_20261015` |

## 2. Host-Vorbedingungen (keine Geräteaktion)

1. **H0:** Arbeitsbaum des Review-Branches sauber; `git diff 07d1526678c171cf4e51a37650c221922d842f32 HEAD -- diag/*.py` leer; `sha256sum -c diag/review/g1b/HOST_TOOLS_SHA256SUMS` = alle OK. Sonst HOLD.
2. `timedatectl show -p NTPSynchronized` = `yes` (dokumentiert in der Evidence); Hostuhr dient Pcap und UART.
3. Ausgabeordner anlegen (`mkdir -m 700`), freier Platz ≥ 1 GB.
4. Passwortdatei `~/pw` für den Lasttreiber vorhanden (Inhalt wird nie ausgegeben).

## 3. Geräteaktionen vor dem Testboot (P1–P3, **eine** esptool-Sitzung)

| Nr. | Aktion | Wirkung |
|---|---|---|
| P1 | `esptool` verbindet mit `--before default-reset` | 1 Reset in den ROM-Bootloader; MAC/Chip werden gelesen |
| P2 | **ein** `read-flash` (kein Schreibzugriff), Befehl unten | liest Flash nur lesend |
| P3 | Sitzungsende `--after hard-reset` | 1 Reset in die Anwendung; UART nicht Teil der Messung |

```
esptool --chip esp32 --port /dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A5069RR4-if00-port0 \
  --before default-reset --after hard-reset read-flash 0x8000 1793328 <Ausgabeordner>/p2_readback.bin
```

`1 793 328 B` = `0x10000 − 0x8000` + 1 760 560 B. Prüfung: Bytes `[0, 3072)` = Partitionstabelle (Hash oben), Bytes `[32 768, 1 793 328)` = App (Hash oben). **Der Bereich `0x9000–0xFFFF` enthält NVS/PHY (kann WLAN-Zugangsdaten enthalten):** Er wird nur in diese private Datei gelesen, **die Datei wird nach Hashprüfung und Dokumentation der beiden Hashes sofort gelöscht**; nie veröffentlicht. *Alternative B (Ownerwahl):* nur App lesen (`read-flash 0x10000 1760560`), dann entfällt der NVS-Bereich, aber auch die Partitionstabellenprüfung (dann nur App-Hash und UART-Quellzeile). Der State-Store (`0x300000–0x3FFFFF`) wird in keinem Fall gelesen.

**HOLD** (kein R1) bei: MAC/Chip abweichend, Lesefehler, App- oder Partitionshash abweichend, jeder weiteren Abweichung vom erwarteten Reset-Verhalten. Kein Reparaturflash.

## 4. Lauf R1 (genau einer; 1 Reset = erfasster Testboot)

Reihenfolge und Zeitgrenzen gemäss Testblatt Abschnitt 3 Schritt 4; `PATH=/home/manuel/g1b-tools:$PATH` nur im Aufruf.

1. `python3 diag/g1_capture.py --run --gate-id PR203_RES01_G1B_R1_20261015 --allow-port-open-boot --expected-source bb4f43a69740f234d26a951707991f792cc3ffde --output <Ausgabeordner>/candidate_capture --load` (Portöffnung = bekannter FT232R-Reset = Testboot).
2. Bereit (≤ 120 s): Quelle, `application: ready`, `real actuators: disabled`, Release-Profil, `HomeConnected`, frischer Heartbeat in `uart.log`; sonst `STOP` und HOLD.
3. `python3 diag/g1_pcap.py capture --run --gate-id PR203_RES01_G1B_R1_20261015 --interface enp2s0 --device-ip <Geräte-IP, Standard von g1_capture.py> --output <Ausgabeordner>/pcap/cap.pcap --stop-file <Ausgabeordner>/candidate_capture/ABORT --stop-file <Ausgabeordner>/candidate_capture/STOP` (`--max-seconds` Vorgabe 1961). Auf `pcap/PCAP_RUNNING` warten (≤ 30 s), sonst `STOP` und HOLD.
4. Erst dann `START_LOAD` anlegen (Inhalt: UTC-Zeitstempel). Last startet bei Uptime ≥ 180 s; P-A wie G1 (vier Sessions, 60 s Idle, 1200 s Last, 300 s Recovery, harter Abbruch bei erster Anomalie, kein Logout/Retry).
5. Endsignal: Anomalie ⇒ `ABORT` durch den Supervisor; reguläres Ende (`load.json` `COMPLETED_PENDING_REVIEW`) ⇒ Operator legt `STOP` an, **spätestens 60 s** danach.
6. Danach **keine** weitere Geräteaktion. Port wird vom Tool geschlossen; kein R2/R3, kein Recovery-Reset, kein Rollback ohne neue Freigabe.

**Stopbedingungen (HOLD, wie G1):** `uart-anomaly` (inkl. `heap_alloc_failed`, Panic, Watchdog, Brownout), unerwarteter Reset, Heartbeat-Lücke > 5 s, Quellabweichung, Main-Stack < 11 160 B, HTTPD-Stack < 2 080 B, Last- oder Capture-Fehler, Skripthash ≠ Manifest, Pcap-Exitcode ≠ 0 (wertet nur die Drahtevidence des Laufs als HOLD, die UART-Evidence bleibt gültig), Ownerstopp.

## 5. Auswertung und Veröffentlichung

- Bericht nach Plan Abschnitt 4 (Entscheidungsregel): Dekodierung `diag_alloc_fail` gegen den gepinnten ELF (`addr2line`), Drahtzähler und `event_window` zur UART-Ereigniszeit, Heapverlauf nach dem Ereignis (5-s-Tail), `abort_induced`-getrennte Fehler, Fehlerrate (n = 1, keine Statistik).
- Veröffentlicht werden nur: abgeleitete `analyze`-Zähler, `uart_tail.log` (ohne Netzwerk-Initialisierungszeilen), `uart_chunks.jsonl`, `load.json`-Auszüge, Fehlerfenster-Bytebereiche, `capture.json`/`pcap_result.json` (Tool-Hashes). **Nie:** Pcap, Readbacks, Passwort, Cookies, Bodies.
- Vergleichsdefinition F2-D4 (Testblatt Abschnitt 5): **Owner-Vorschlag**, p95-Vergleich 1,25× nur als **Befund**, kein automatischer Fail. F2-D5 (Restgefahrakzeptanz) bleibt offen.

## 6. Ownerbestätigungen (vor Beginn einzeln zu erteilen)

| # | Bestätigung | Wert |
|---|---|---|
| 1 | Genau dieses Gate (Gate-ID, Werkzeugcommit, Hashes) für **genau einen** Lauf R1 freigeben | `PENDING` |
| 2 | Aktorfreier Aufbau vor Ort bestätigt; Owner anwesend | `PENDING` |
| 3 | P1–P3 (zwei Resets, ein Lesevorgang) und der Port-Öffnungsreset von R1 sind akzeptiert | `PENDING` |
| 4 | P2: Variante A (mit NVS-Bereich in privater, sofort gelöschter Datei) **oder** Variante B (nur App) | `PENDING` |
| 5 | F2-D4: 1,25× p95 als Befund, nicht als Fail-Grenze | `PENDING` |
| 6 | R2/R3 bleiben gesperrt; Fortsetzung nur nach Sichtung des R1-Ergebnisses und neuer Entscheidung | `PENDING` |

Nicht Teil dieses Gates: Flash, Neubuild, Touch-/Hold-Sequenzen, Konfigurationsänderung (O1), P-A2/P-B/G2, zusätzliche Hostrechte.
