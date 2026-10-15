# G1b Stufe E – A/B-Hardwaregate für O1 (Vorschlag zur Ownerfreigabe)

`F2_S3_REVIEW=PASS` · `F2_HARDWARE=PENDING` · `G1B_STAGE_E_AUTHORIZATION=PENDING` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`

Dieses Dokument gewährt keine Freigabe. Jede Teilfreigabe (Abschnitt 3 und 6) gilt einzeln; ein Flash, Reset oder Lauf ohne die jeweilige ausdrückliche Ownerbestätigung ist ausgeschlossen. **Es gibt keine automatische Folgeaktion** von einem Teilgate zum nächsten. Umfang dieses Gates: **Baseline-/Identitäts-Preflight ohne Last** und ein **B-Pilot** (ein App-only-Flash von B, genau ein P-A-Lauf B1). Der **B-Pilot ersetzt nicht** den im F2-Plan verlangten gleichtägigen A/B-Vergleich, die B-Serie (P-A 3×), P-A2, P-B, G2 oder die Restgefahrentscheidung; ein fehlerfreies B1 ist nur ein ermutigender Befund, **kein O1-GO und kein Produkt-GO**. Der vollständige Vergleich bleibt als **spätere**, nach Sichtung von B1 einzeln zu entscheidende Option erhalten (Abschnitt 3.2). Aus diesem Gate folgt kein Abnahmeanspruch, keine Erfolgsbehauptung und keine F2-D5-Restgefahrakzeptanz. Plan: `docs/tasks/issue-27-res01-f2-escalation-plan.md` @ `3bb29481495c4baca540ada1f3ae5433a5b46a34`; O1-Umsetzung `ba32717dae4dcd35d57fd688b3fa2cc21966bc7e` (Independent Review PASS).

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

## 3. Teilgates

Hintergrund der Trennung: A ist unter P-A bereits zweimal nach einer Heap-Allokationsstörung abgebrochen (G1, R1b). Ein A'-Lauf als Pflichtvorstufe könnte einen kontrollierten Abbruch **vor** dem ersten O1-Test erzwingen. Deshalb ist der erste Lauf der Folge **B1**, und A'-Läufe sind keine Vorbedingung.

### 3.1 B-Pilot (jedes Glied einzeln und nacheinander ausdrücklich freizugeben)

| Teilgate | Inhalt | Geräteaktionen |
|---|---|---|
| **E-P0** | **Baseline-/Identitäts-Preflight ohne Last**: 4-MiB-Lesen (Variante *voll*, privat) auf dem installierten Image A; MAC/Chip, Partitionstabelle- und App-Hash gegen A prüfen; Baseline für die Nicht-App-Byteprüfung sichern | 2 Resets (eine esptool-Sitzung); danach **keine** Last, kein Testboot |
| **E-FB** | **App-only-Flash B** auf `0x10000` mit esptool-Hashprüfung und anschliessender Verifikationssitzung (Nicht-App-Bereiche byteidentisch zur Baseline, App-Hash = B) | 2 Resets Schreibsitzung + 2 Resets Verifikationssitzung |
| **E-B1** | **genau ein** P-A-Lauf **B1** unter den bestehenden Stopbedingungen | 1 Reset (erfasster Testboot) |

Gesamt: 7 Resets, 1 App-only-Flash, 1 erfasster Lauf. Zwischen den Teilgates keine Aktion. Die **Daten der Baseline** (privat, 0700, enthalten NVS/PHY/State-Store) bleiben bis zur Ownerentscheidung nach B1 erhalten und werden danach gelöscht, sofern kein späterer Vergleich sie braucht; nie veröffentlicht.

**Bei Anomalie (in jedem Teilgate): STOP/HOLD** ohne weiteren Reset, Flash, Retry, automatischen Rückflash oder Folgelauf. Ein eventuell installiertes B-Image bleibt bis zu einer nächsten ausdrücklichen Ownerentscheidung **unangetastet**; ein Rückflash von A ist ein eigenes, später freizugebendes Teilgate.

### 3.2 Spätere Option: vollständiger A/B-Vergleich (nicht zwingende Vorstufe, erst nach Sichtung von B1 einzeln entschieden)

Falls der Owner nach B1 den Vergleich wünscht, gilt die bestehende Vergleichslogik (Abschnitt 5, Testblatt Abschnitt 5) unverändert: gleichtägige A'-Referenzläufe mit demselben Image A und denselben Werkzeugen, B-Serie bis zu 3× P-A, Rückflash von A als eigenes Teilgate. Mögliche Zuschnitte (Auswahl erst nach B1, je Glied eigenes Teilgate): **(a)** A'-Klammer um die B-Serie (z. B. Rückflash A, A'1; B-Serie; A'2), **(b)** nur B-Serie ohne A'-Klammer (Tagesdrift nicht bewertbar), **(c)** nur weiterer Einzellauf. Die frühere Reihenfolge „A'1 zuerst“ ist **gestrichen**. Ohne diese spätere Entscheidung gibt es keine A'-Läufe, keinen Rückflash und keine B-Serie.

## 4. Geräteaktionen im Detail (nur unter der jeweiligen Teilfreigabe)

**Baseline (E-P0):** `esptool --chip esp32 --port /dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A5069RR4-if00-port0 --before default-reset --after hard-reset read-flash 0x0 0x400000 <privat>/baseline_4MiB.bin` (≈ 100 s; enthält NVS/PHY und State-Store, bleibt privat, wird nach der letzten Verifikation gelöscht). Prüfung: Partitionstabelle- und App-Hash gegen A, MAC/Chip; Abweichung ⇒ HOLD.

**Flash (E-FB; ein späterer Rückflash von A ist ein eigenes Teilgate nach 3.2):** genau **ein** App-only-Schreibvorgang: `esptool … --before default-reset --after hard-reset write-flash --flash-mode keep --flash-freq keep --flash-size keep 0x10000 <B- bzw. A-App>` (esptool prüft den geschriebenen Bereich per Hash). Danach **eine** Verifikationssitzung: `read-flash 0x0 0x400000 <privat>/post_<arm>.bin`; **Pflichtprüfung:** Bereich `0x000000–0x00FFFF` und `0x300000–0x3FFFFF` byteidentisch zur Baseline; Bereich `0x10000` + App-Länge hashgleich zum geschriebenen Image; Rest des Flashs unverändert. Abweichung oder Schreibfehler ⇒ **HOLD, kein Reparaturflash, kein Erase-all, kein Schreiben von Bootloader/Partition/NVS/PHY/State-Store, keine Kalibrierung, kein Werksreset**. Der Bootloader (`bootloader_NOT_FOR_FLASH.bin`) wird nie geflasht. Rückfall nur Flash von A (Bestandteil der Folge) oder, ausschliesslich mit neuer Zustimmung, die gepinnten Rückfallartefakte aus dem G1-Gate (C0 `9a9a8bd`, `ed29c9c`, Factory-Backup `b9564e6`).

**Lauf (E-B1; spätere Läufe nur nach 3.2):** je Lauf genau Ablauf, Reihenfolge, Zeitgrenzen und Befehle aus `G1B_R1_GATE.md` Abschnitt 4 (Bereit ≤ 120 s, `PCAP_RUNNING` ≤ 30 s, dann `START_LOAD`, Endsignal ≤ 60 s nach Recovery, Budget 1 961 s) mit `--expected-source` = Quellcommit des jeweiligen Arms, Gate-ID `PR203_RES01_G1B_STAGE_E_B1_20261015` und eigenem Ausgabeordner `…/diag/g1b/stage_e_20261015/run_b1/`. Port-Öffnungsreset = erfasster Testboot. Vor B1 gilt die Identität durch die Verifikationssitzung von E-FB; der Operator bestätigt vor dem Lauf schriftlich, dass seither kein Flash/esptool stattfand, **bei Zweifel HOLD**.

## 5. Stopbedingungen, Abnahme, Vergleich (unverändert)

- **Stop/HOLD** je Lauf und **beim ersten Anomalieereignis ohne Retry, weiteren Reset oder Folgeaktion**: wie `G1B_R1_GATE.md` Abschnitt 4 (u. a. `heap_alloc_failed`, Panic/Watchdog/Brownout, unerwarteter Reset, Heartbeat-Lücke > 5 s, Quellabweichung, Main-Stack < 11 160 B, HTTPD-Stack < 2 080 B, Login-/Request-Fehler, Capture-/Pcap-Fehler, Skripthash ≠ Manifest, Ownerstopp). Ein HOLD stoppt alle Teilgates; die Fortsetzung verlangt eine neue ausdrückliche Entscheidung, auch ein Rückflash.
- **Harte Kriterien für B** (Plan F1/F4 Abschnitt 7, über den **gesamten** B-Lauf, nie durch ein Vergleichsfenster relativiert): kein `heap_alloc_failed`, kein Abort/Panic/Watchdog/Brownout/unerwarteter Reset, HTTP 100 % 2xx, keine nicht abort-assoziierten Timeouts/Verbindungsfehler, Erholung ≥ 90 % des Vier-Session-Idle innerhalb 120 s, Stack-HWM nicht schlechter als Baseline um > 256 B, F4-Verhalten gemäss Kriterium 6.
- **Vergleich** (nur bei später freigegebenem Vergleich nach 3.2; für B1 allein gelten die harten Kriterien und die Berichtspflichten) nach `G1B_TEST_SHEET.md` Abschnitt 5 (zulässige A-Referenzen: vollständige Referenz bzw. früh endende nur im Fenster; Vergleichsfenster `[0, T]` mit `T = min(1200 s, kürzeste beobachtete Lastdauer der verwendeten Läufe)`; Kennzahlen, Drahtzähler mit `event_window`). **1,25× p95 ist Bewertungsgrösse, nicht automatische FAIL-Grenze** (Owner bestätigt); Überschreitung = Befund. n ≤ 3 je Arm ist kein statistischer Nachweis, die A-Fehlerrate wird neben B ausgewiesen.
- **Ausgang:** Bei hartem Fail von B1 kein automatischer Wechsel auf O2/O3 und kein Rückflash, Rückfall in den Plan. Ein fehlerfreies B1 ist nur ein ermutigender Befund (n = 1, ohne gleichtägige A-Referenz), **keine** Wirksamkeits- oder OOM-Sicherheitsaussage, kein O1-GO; es folgen die ausstehenden Teile von S4 und G2 unter eigenen Gates.

## 6. Ownerbestätigungen (einzeln, vor dem jeweiligen Teilgate; alle `PENDING`)

| # | Bestätigung | Wert |
|---|---|---|
| 1 | E-P0 (Baseline-/Identitäts-Preflight ohne Last) freigeben | `PENDING` |
| 2 | E-FB (App-only-Flash B + Verifikation) freigeben (nach Sichtung von E-P0) | `PENDING` |
| 3 | E-B1 (genau ein P-A-Lauf B1) freigeben (nach Sichtung von E-FB) | `PENDING` |
| 4 | Aktorfreier Devboard-Aufbau vor Ort bestätigt; Owner anwesend (je Teilgate) | `PENDING` |
| 5 | Baseline-Variante *voll* (4 MiB privat, Datenumgang wie oben) **oder** nur App und Partitionstabelle (dann keine Nicht-App-Vergleichsprüfung: schwächerer Nachweis) | `PENDING` |
| 6 | Resets (7) und der eine App-only-Flash sind akzeptiert | `PENDING` |
| 7 | F2-D4: 1,25× p95 nur Bewertungsgrösse; F2-D5 bleibt offen | `PENDING` |
| 8 | Kein automatisches Weiterlaufen; vollständiger Vergleich, B-Serie, A'-Läufe, Rückflash A, P-A2, P-B, G2 nur nach späterer ausdrücklicher Entscheidung; R2/R3 gesperrt | `PENDING` |

Nicht Teil dieses Gates: A'-Läufe, B-Serie, Rückflash von A, Produkt-/G2-Image, P-A2, P-B, Touch-/Hold-Sequenzen, O2/O3, weitere Konfigurations- oder Werkzeugänderungen, Rollback ohne neue Zustimmung.
