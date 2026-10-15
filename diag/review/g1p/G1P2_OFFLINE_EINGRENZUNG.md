# PR #203 – G1P-2 Retest: Offline-Eingrenzung (Stack-HWM, F4-Bedarf)

Status: `OFFLINE_ANALYSIS_ONLY` — nur bestehende private Evidence (`hw-evidence-pr203/diag/g1p/run_g1p2_retry/`), kein Flash/Reset/Port/Netzwerk/Build, kein Produktcode-Commit.
Ausgangsstand: Produkt-HEAD `162a6670526a10b2fd957fe90ab665a1fae837b7`, IMAGE01 App `da9e8868…f8cf0` (Quelle `bb4f43a`). Der Lauf bleibt **`FAIL/HOLD`** (Gate-Hard-Stop `main-stack-reserve`); keine rückwirkende Umwertung.
`RES-01=BLOCKER`, `PR203_FINAL_GO=HOLD`, `ACTUATOR_RELEASE=NO`.

## 1. Stackbefund – Zuordnung

Main-Task-Stack: `CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576` B. HWM (frei, Minimum) laut `diag_30s`: 11 688 B konstant von 1,8 s bis 692,5 s; **11 192 B** bei 722,5 s; **10 984 B** bei 752,5 s (belegt dann 13 592 B = 55 %).

| Fenster (Geräte-Uptime) | HWM-Änderung | erstmals im Fenster aufgetretene UI-Seiten (`g1_ui`, Seitenindex = `FermentationUiPage`) |
|---|---|---|
| ≤ 692,5 s (ca. 11 min Browser: An-/Abmelden, Aktualisieren, Programm-Buttons) | keine (11 688 B) | nur Seite 0 (Home) wird gerendert; Webzugriffe ändern die Main-Stack-HWM nicht |
| 692,5–722,5 s | **−496 B** | 16 Status @709,7 s; 12 Technical @710,8 s; 13 Messages @712,9 s; 26 Settings @722,4 s |
| 722,5–752,5 s | **−208 B** | 19 Pin @728,9 s; 18 Service @739,5 s (Touch-Press `outcome=2` @739,2 s); 1 ProgramList @751,2 s |

- Belegt: Beide Abfälle fallen mit dem **erstmaligen Touch-Navigieren auf neue UI-Seiten** zusammen; der Browserbetrieb allein hat die HWM nicht bewegt.
- Der grössere Abfall (−496 B) liegt **vor jeder PIN-/Service-Seite**; PIN/Service ist daher **nicht** der alleinige Verursacher. Für das zweite Fenster enthält das Fenster drei neue Seiten (Pin, Service, ProgramList); eine Einzelzuordnung ist mit einem monotonen Task-Höchstwert **`NOT_PROVEN`**. Kein Callsite-Trace, keine Compiler-Stackreports für diesen Pfad vorhanden; kein neuer Build/Audit durchgeführt.
- Der Wert 11 192 B trat bereits in der Evidence von B2 (Image B, 12 Samples) auf, ohne Overflow/Reset; 11 688 B ist der Leerlaufwert von IMAGE01 in G1 und G1P-2.

## 2. Gategrenze

- Gate-Wert: Baseline 11 416 B frei **−256 B = 11 160 B** (Plan §7.4 Kriterium 5; `g1_capture.py` Schwelle 11 160).
- Herkunft Baseline: D3-Diagnoselauf (`run/uart_D3.log`, 16 Samples konstant 11 416 B, erster Wert 18 584 B) auf dem damaligen Baseline-/Diagnoseimage, **nicht** IMAGE01 (dort Leerlauf 11 688 B, Start 18 520 B). Zudem ohne die jetzt erreichte Touch-Navigation über neue Seiten. Die Vergleichsbasis ist damit für diese Seitenabdeckung **nicht gleichwertig**; die Grenze ist ein relativer Regressionswert, keine absolute Überlaufgrenze.
- Fakten: kleinster HWM 10 984 B (176 B unter der Gategrenze, 432 B unter der D3-Baseline, 704 B unter dem IMAGE01-Leerlauf); weiterhin 10 984 B frei von 24 576 B; **kein** Stacküberlauf, Panic, Reset oder Watchdog.
- Einordnung: **korrekter Gate-Hard-Stop** (Kriterium wie freigegeben verletzt); ein **produktiver Stackfehler ist nicht nachgewiesen**. Der Lauf wird nicht rückwirkend `PASS`; die Grenze wird nicht geändert.

## 3. F4-Befund 16 768 B

Datensatz: `t=739,562 s`, Phase 0 (Step, äusserer Scope), Seite 19→18 (Pin→Service), Locale 1, `fresh=1`, `n=1`: `pre=39 900`, `low=23 132`, `block_pre=32 768`, `block_low=19 456` ⇒ `demand = pre − low = 16 768 B`. Zugehöriger Render-Datensatz (Phase 2, Seite 18): `pre=33 848`, `low=23 132` ⇒ 10 716 B; der Step reserviert vor dem Render bereits ca. 6 KB (Modell/Befehle). Direkt vorher: Touch-Press-Dispatch (`outcome=2`).
- Korrelation: In 692–753 s gibt es **keine** `res01_http`/`httpd_txrx`-Zeilen (kein Shell-Handler); parallel liefen API-Polling und WLAN-Aktivität (nicht einzeln geloggt). `demand` ist der Rückgang des **gemeinsamen** Internal-8-bit-Heaps, keine dem UI zurechenbare Spitzenallokation. Beleg für Fremdanteile im selben Lauf: @733,1 s sinkt `pre` ohne Seitenwechsel von 39 900 auf 35 776 B (Seite 19), also ca. 4 KB externe Schwankung.
- Aussage: der Wert ist eine **obere Schranke** für UI-plus-Parallelaktivität an dieser Stelle, **kein belastbares `b`**. Bedingte Rechengrösse nach Plan `max(6656, b) × 2`: bei `b = 16 768` ⇒ **33 536 B** (nicht als Schwelle erklärt).
- Abgleich: grösster je beobachteter freier Block im stabilen Betrieb **32 768 B** (Boot 40 960 B); 33 536 B läge darüber ⇒ ein Gate mit diesem Wert würde im Normalbetrieb **dauerhaft skippen** (Dauer-Skip). Die aktuelle vorläufige Schwelle (Skip 13 312 B / Resume 16 640 B) wurde nie unterschritten: niedrigster `block_low` im UI 19 456 B, `skips=0`.
- Coverage: Seiten {0,1,12,13,16,18,19,26}, Locales {1,2}. **`NOT_PROVEN`:** Spanisch, übrige Seiten (Programm-/Prozess-/Dialog-/Header-/Wert-/Text-/FactoryReset-Seiten), Hold/Skip/Resume auf Hardware.

## 4. Entscheidungsvorlage

| Befund | Einordnung | Kleinstes notwendiges Delta |
|---|---|---|
| Main-Stack-HWM unter Gategrenze | **(a)** Gate-/Vergleichsgrenzenbefund, kein erwiesener Produktdefekt; Verursacher je Seite **(c) offen** | Nur Ownerentscheid: Kriterium 5 für das Touch-UI-Profil als Reserve-/Trendbefund mit Owner-Akzeptanz (wie früher bei der HTTPD-Reserve) behandeln oder die Baseline-Basis neu festlegen. Keine Stackvergrösserung, keine Instrumentierung, kein neuer Lauf |
| F4-Bedarf 16 768 B / Formel 33 536 B | **(c)** offen: kein belastbares `b`; Formelwert würde Dauer-Skip erzeugen | Kein Produktdelta. Ownerentscheid, ob die vorläufige Schwelle (13 312/16 640 B, `PROVISIONAL_PENDING_G1`) mit offener Coverage und dokumentiertem Bedarf bleibt (Qualifikation `HOLD`/`NOT_PROVEN`) oder ein anderes Bewertungsprinzip freigegeben wird |
| Fehlende Coverage / Hardware-Hold-Skip-Resume | `NOT_PROVEN` | Nativ abgedeckt; kein zusätzlicher Hardwaretest ohne konkreten Befund |

Alle Planänderungen (Gate-Kriterium, F4-Schwelle) bedürfen der Ownerfreigabe; es wurde nichts geändert. Offen für den Owner: ob G2-Web (reines Produktimage, ohne F4-Schwellenaussage) unabhängig von G1′ fortgesetzt werden soll.
