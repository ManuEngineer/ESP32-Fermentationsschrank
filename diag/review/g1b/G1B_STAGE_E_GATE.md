# G1b Stufe E – A/B-Hardwaregate für O1 (Vorschlag zur Ownerfreigabe)

`F2_S3_REVIEW=PASS` · `F2_HARDWARE=PENDING` · `G1B_STAGE_E_AUTHORIZATION=PENDING` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`

Dieses Dokument gewährt keine Freigabe. Jede Teilfreigabe (Abschnitt 6) gilt einzeln; ein Flash, Reset oder Lauf ohne die jeweilige ausdrückliche Ownerbestätigung ist ausgeschlossen. Es gibt keine automatische Folgeaktion zwischen den Teilgates. Umfang: **P-A-Vergleich A gegenüber B** (Teil 1 von S4). P-A2, P-B (echte Browser), vollständige Abnahme nach Plan F1/F4 Abschnitt 7 und G2 (reines Produktimage) sind **nicht** Teil dieses Gates und bleiben ausstehend; aus diesem Gate folgt **kein Abnahmeanspruch**, keine Erfolgsbehauptung, keine F2-D5-Restgefahrakzeptanz. Plan: `docs/tasks/issue-27-res01-f2-escalation-plan.md` @ `3bb29481495c4baca540ada1f3ae5433a5b46a34`; O1-Umsetzung `ba32717dae4dcd35d57fd688b3fa2cc21966bc7e` (Independent Review PASS).

## 1. A und B – gepinnte Images (einziger funktionaler Unterschied: O1)

Beide Arme sind dasselbe Diagnose-Overlay (G1-Overlay `bb4f43a`: Backtrace-Hook, G1-Probe, F4-Gate) auf Produktbasis `d35ee85`; B ist **genau** A plus die 9 Zeilen `CONFIG_LWIP_TCP_SND_BUF_DEFAULT=2880` in `sdkconfig.defaults` (dieselbe Änderung wie `ba32717`, per `cherry-pick` angewendet).

| | **A** (G1 IMAGE01, unverändert) | **B** (A + O1) |
|---|---|---|
| Quellcommit (eingebettet, `source git sha`) | `bb4f43a69740f234d26a951707991f792cc3ffde` | `31c3ee48b52f44852b2e5cd5f542ba2fce614537` (Kind von `bb4f43a`, Diff = nur `sdkconfig.defaults`, +9 Zeilen) |
| Branch (öffentlich abrufbar) | in der Historie von B enthalten | `agent/pr203-res01-stage-e-b-diag` (nicht zum Merge) |
| App-SHA-256 / Bytes | `da9e868874649bd274d21a4f1c5715afd9293288497027004c460efc46df8cf0` / 1 760 560 | `4145dddb272e639c9a055df8f9877d4395af32702f15ba92d4f43dae834c7b68` / 1 760 576 |
| ELF-SHA-256 | `24d8557f965053d566ba6ef3c4bfe745044a505994835e5831b85ff0dbe8c369` | `06c1fb10d72cc4c8ecaf47ef86675f1b305b25035cf62b8cd89811ff3f9d4ef7` |
| Partitionstabelle (3 072 B) | `d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d` | **identisch** `d7f180e4…ccd9d` |
| Aufgelöstes `sdkconfig` (SHA-256) | `9ab3490da37da6bd5a9689eb067c9739276974c0b3cfe20d8c40def2d925c3ea` | `2fb0e682b9b862692436a218cf78e1f3ede7336c8fffe1ef1a34b9a00ef92d7d` |
| `CONFIG_LWIP_TCP_SND_BUF_DEFAULT` (+ Alias `CONFIG_TCP_SND_BUF_DEFAULT`) | **5760** | **2880** (auch in `config/sdkconfig.h`) |
| Hook-Prüfung am ELF (`check_hook_cache_safety.py`) | `FLASH_REFS=0` | `FLASH_REFS=0`, gleicher Aufrufabschluss |

**Nachweis „einziger Unterschied“ (offline, vor dem Gate erbracht):**
1. Aufgelöstes `sdkconfig` A gegen B (ohne Kommentarzeilen): **genau zwei** Zeilen unterscheiden sich (`CONFIG_LWIP_TCP_SND_BUF_DEFAULT`, `CONFIG_TCP_SND_BUF_DEFAULT`: 5760 → 2880).
2. ELF-Symbolvergleich (`nm -S`): **12 085 Symbole in beiden**, keines nur in A oder B; nur zwei Symbole mit geänderter Grösse: `tcp_alloc` 206 → 212 B (aus `TCP_SND_BUF` abgeleitete Konstanten) und `esp_wifi_config_info` 266 → 270 B (die ausgegebene Konfigurationszeile `tcp tx win`). Die App ist 16 B grösser.
3. Quelltext: `git diff bb4f43a..31c3ee4` = nur `sdkconfig.defaults`. Gleiche Toolchain (`xtensa-esp-elf` 15.2.0, ESP-IDF v6.1), `build_esp_idf_profiles.py release --require-clean-source-tree`, `SOURCE_TREE_CLEAN=YES`.
4. Beide Images tragen ihre eigene Quellzeile (`bb4f43a…` bzw. `31c3ee4…`); `g1_capture.py` prüft sie über `--expected-source` je Arm.

Private Artefaktablage (0700): `/var/lib/docker/data/ESP32-Projekte/hw-evidence-pr203/diag/g1b/stage_e_20261015/{A,B}/` mit `SHA256SUMS`. Vor jedem Flash werden die Hashes erneut gegen diese Tabelle geprüft; Abweichung ⇒ HOLD.

## 2. Gepinnte Host- und Messbedingungen (gleich für alle Läufe)

Werkzeugcommit `07d1526678c171cf4e51a37650c221922d842f32` (Review-Branch; Manifest `HOST_TOOLS_SHA256SUMS`, H0 vor jedem Lauf wie `G1B_R1_GATE.md`), Lastprofil P-A unverändert (vier Sessions, 60 s Idle, 1200 s Last, 300 s Recovery, Login-Timeout 8 s, übrige 3 s, kein Logout/Retry), Treiber `c0_load.py`/`g1_capture.py`/`g1_pcap.py` identisch, private tcpdump-Kopie (`cap_net_raw=ep`, nur über `PATH`), Interface `enp2s0`, dasselbe Board (MAC `20:50:0d:1b:2f:34`), derselbe AP (UART-Beleg), Hostuhr NTP-synchron, **ein Kalendertag**, Reihenfolge und Tageszeit je Lauf dokumentiert. Keine neuen Diagnosefunktionen, keine weiteren Konfigurationsänderungen, keine Werkzeugänderung (bekannte Lücken bleiben: UART-Nachlauf bei `load-failure`, Pcap-Endverlust nicht erkannt; beides ist als Bewertungsgrenze zu führen).

## 3. Vorgeschlagene Folge (jedes Glied ein eigenes Teilgate; Owner darf an jeder Stelle stoppen)

| Teilgate | Inhalt | Geräteaktionen |
|---|---|---|
| **E-A1** | Baseline lesen (Variante *voll*, 4 MiB, privat) und Lauf **A'1** auf dem derzeit installierten Image A | 2 Resets Baselinesitzung + 1 Reset Testboot |
| **E-FB** | **Flash B**: App-only auf `0x10000` | 2 Resets Schreibsitzung + 2 Resets Verifikationssitzung |
| **E-B1, E-B2, E-B3** | je ein Lauf **B1/B2/B3** (Plan F1/F4 Abschnitt 7: P-A 3×) | je 1 Reset Testboot |
| **E-FA** | **Flash A** zurück (Rückfall und Bracket) | 2 + 2 Resets |
| **E-A2** | Lauf **A'2** | 1 Reset Testboot |

Gesamt bei vollständiger Folge: 15 Resets, 2 App-only-Flashvorgänge, 5 erfasste Läufe. **Alternativen zur Ownerwahl:** (i) vollständige Folge A'1–B1–B2–B3–A'2; (ii) ohne Rückflash: A'1–B1–B2–B3 (ein Flash; die A'-Klammer um B fehlt, Tagesdrift nicht bewertbar); (iii) Pilot: nur A'1–B1 (ein Flash, n = 1 je Arm). Ohne Wahl gilt keine Freigabe.

## 4. Geräteaktionen im Detail (nur unter der jeweiligen Teilfreigabe)

**Baseline (E-A1):** `esptool --chip esp32 --port /dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A5069RR4-if00-port0 --before default-reset --after hard-reset read-flash 0x0 0x400000 <privat>/baseline_4MiB.bin` (≈ 100 s; enthält NVS/PHY und State-Store, bleibt privat, wird nach der letzten Verifikation gelöscht). Prüfung: Partitionstabelle- und App-Hash gegen A, MAC/Chip; Abweichung ⇒ HOLD.

**Flash (E-FB bzw. E-FA):** genau **ein** App-only-Schreibvorgang: `esptool … --before default-reset --after hard-reset write-flash --flash-mode keep --flash-freq keep --flash-size keep 0x10000 <B- bzw. A-App>` (esptool prüft den geschriebenen Bereich per Hash). Danach **eine** Verifikationssitzung: `read-flash 0x0 0x400000 <privat>/post_<arm>.bin`; **Pflichtprüfung:** Bereich `0x000000–0x00FFFF` und `0x300000–0x3FFFFF` byteidentisch zur Baseline; Bereich `0x10000` + App-Länge hashgleich zum geschriebenen Image; Rest des Flashs unverändert. Abweichung oder Schreibfehler ⇒ **HOLD, kein Reparaturflash, kein Erase-all, kein Schreiben von Bootloader/Partition/NVS/PHY/State-Store, keine Kalibrierung, kein Werksreset**. Der Bootloader (`bootloader_NOT_FOR_FLASH.bin`) wird nie geflasht. Rückfall nur Flash von A (Bestandteil der Folge) oder, ausschliesslich mit neuer Zustimmung, die gepinnten Rückfallartefakte aus dem G1-Gate (C0 `9a9a8bd`, `ed29c9c`, Factory-Backup `b9564e6`).

**Läufe (E-A1, E-B*, E-A2):** je Lauf genau Ablauf, Reihenfolge, Zeitgrenzen und Befehle aus `G1B_R1_GATE.md` Abschnitt 4 (Bereit ≤ 120 s, `PCAP_RUNNING` ≤ 30 s, dann `START_LOAD`, Endsignal ≤ 60 s nach Recovery, Budget 1 961 s) mit `--expected-source` = Quellcommit des jeweiligen Arms, Gate-ID `PR203_RES01_G1B_STAGE_E_<LAUF>_20261015` und eigenem Ausgabeordner `…/diag/g1b/stage_e_20261015/run_<lauf>/`. Port-Öffnungsreset = erfasster Testboot. Keine P1–P3-Wiederholung vor Läufen derselben Image-Identität, solange seit der letzten Verifikation kein Flash/esptool stattfand; der Operator bestätigt dies vor jedem Lauf schriftlich, **bei Zweifel HOLD**.

## 5. Stopbedingungen, Abnahme, Vergleich (unverändert)

- **Stop/HOLD** je Lauf und **beim ersten Anomalieereignis ohne Retry, weiteren Reset oder Folgeaktion**: wie `G1B_R1_GATE.md` Abschnitt 4 (u. a. `heap_alloc_failed`, Panic/Watchdog/Brownout, unerwarteter Reset, Heartbeat-Lücke > 5 s, Quellabweichung, Main-Stack < 11 160 B, HTTPD-Stack < 2 080 B, Login-/Request-Fehler, Capture-/Pcap-Fehler, Skripthash ≠ Manifest, Ownerstopp). Ein HOLD stoppt die **gesamte** Folge; die Fortsetzung verlangt eine neue Entscheidung.
- **Harte Kriterien für B** (Plan F1/F4 Abschnitt 7, über den **gesamten** B-Lauf, nie durch ein Vergleichsfenster relativiert): kein `heap_alloc_failed`, kein Abort/Panic/Watchdog/Brownout/unerwarteter Reset, HTTP 100 % 2xx, keine nicht abort-assoziierten Timeouts/Verbindungsfehler, Erholung ≥ 90 % des Vier-Session-Idle innerhalb 120 s, Stack-HWM nicht schlechter als Baseline um > 256 B, F4-Verhalten gemäss Kriterium 6.
- **Vergleich** nach `G1B_TEST_SHEET.md` Abschnitt 5 (zulässige A-Referenzen: vollständige Referenz bzw. früh endende nur im Fenster; Vergleichsfenster `[0, T]` mit `T = min(1200 s, kürzeste beobachtete Lastdauer der verwendeten Läufe)`; Kennzahlen, Drahtzähler mit `event_window`). **1,25× p95 ist Bewertungsgrösse, nicht automatische FAIL-Grenze** (Owner bestätigt); Überschreitung = Befund. n ≤ 3 je Arm ist kein statistischer Nachweis, die A-Fehlerrate wird neben B ausgewiesen.
- **Ausgang:** Bei hartem Fail von B kein automatischer Wechsel auf O2/O3, Rückfall in den Plan. Bei fehlerfreien B-Läufen **keine** Wirksamkeits- oder OOM-Sicherheitsaussage; es folgen die ausstehenden Teile von S4 und G2 unter eigenen Gates.

## 6. Ownerbestätigungen (einzeln, vor dem jeweiligen Teilgate; alle `PENDING`)

| # | Bestätigung | Wert |
|---|---|---|
| 1 | Gesamtfolge wählen: (i), (ii) oder (iii); jedes Teilgate einzeln freigeben | `PENDING` |
| 2 | Aktorfreier Devboard-Aufbau vor Ort bestätigt; Owner anwesend (je Teilgate) | `PENDING` |
| 3 | Baseline: Variante *voll* (4 MiB, enthält NVS/State-Store privat, Löschung nach der Folge) **oder** nur App + Partitionstabelle (dann **keine** Nicht-App-Vergleichsprüfung beim Flash: schwächerer Nachweis) | `PENDING` |
| 4 | Zwei App-only-Flashvorgänge (B, dann A zurück) bzw. einer bei (ii)/(iii) und die genannten Resets sind akzeptiert | `PENDING` |
| 5 | F2-D4: 1,25× p95 nur Bewertungsgrösse; F2-D5 bleibt offen | `PENDING` |
| 6 | Kein automatischer Folgelauf; R2/R3 und alle anderen Läufe ausserhalb der Folge gesperrt | `PENDING` |

Nicht Teil dieses Gates: Produkt-/G2-Image, P-A2, P-B, Touch-/Hold-Sequenzen, O2/O3, weitere Konfigurations- oder Werkzeugänderungen, Rollback ohne neue Zustimmung.
