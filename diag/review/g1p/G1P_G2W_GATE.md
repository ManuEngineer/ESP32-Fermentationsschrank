# PR #203 – Gate-Karte G1′ (F4-Hardware-Evidence) und G2-Web (Teilnachweis, ohne Sensoren)

Status: `GATE_PROPOSAL_NOT_AUTHORIZED` — **kein** Flash, Reset, Portzugriff oder Lauf bis zur konkreten Ownerbestätigung.
`RES-01=BLOCKER`, `PR203_FINAL_GO=HOLD`, `ACTUATOR_RELEASE=NO`.

Grundlage: Produktbranch `agent/issue-27-r1-web-dashboard-plan` @ `162a6670526a10b2fd957fe90ab665a1fae837b7` (Live geprüft, Arbeitsbaum sauber),
Plan `8ce57d78f1544728bf1868d965551d3de7c302f0` (SSOT `docs/tasks/issue-27-res01-heap-hardening-plan.md` §§4, 7.2, 7.4, 7.5),
Ownerauftrag Devboard-Restnachweise. Ist-Aufbau (Owner): ESP32-WROOM-32E, ILI9341 + XPT2046 angeschlossen, keine Temperatursensoren, keine Aktoren.

## 1. Preflight-Ergebnis (offline, lokal verifiziert)

| Prüfung | Ergebnis |
|---|---|
| Produktcode `162a667` gegenüber `d35ee85` | nur Doku; `sdkconfig.defaults` identisch (O1 gesetzt und revertiert) ⇒ aufgelöst `TCP_SND_BUF_DEFAULT=5760` |
| `esp32_release` (C5-Build) App / ELF SHA-256 | `ebb296c08b79045e6e081f328c9f5fd7e576e20722670968b131e722a8b36633` (1 756 528 B) / `45bc809ed67b19d49f1f6bcc89386c8cfc83f3e7e103729e237293cfac5403e3` — **lokal gegen Dateien verifiziert** |
| Partitionstabelle `esp32_release` | `d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d` = Tabelle auf dem Gerät (E-FB-Verifikation) |
| G1′-Diagnoseimage = vorhandenes **IMAGE01** (kein Neubau) | App `da9e868874649bd274d21a4f1c5715afd9293288497027004c460efc46df8cf0` (1 760 560 B), ELF `24d8557f965053d566ba6ef3c4bfe745044a505994835e5831b85ff0dbe8c369`, Quelle `bb4f43a` = `d35ee85` + Overlay (`main/g1_ui_probe.hpp`, Messpunkte Step/Touch/Render, Seiten-/Locale-/Home-/Hold-Zähler, Skip-Coverage); **lokal gegen Dateien verifiziert** (`hw-evidence-pr203/diag/g1/prepare_20261014_image01/artifacts/`). Gleiche Partitionstabelle, 5760-Konfiguration; bereits unabhängig geprüft und bei G1 eingesetzt |
| Geräteidentität / aktuell installiertes Image | zuletzt installiert: Image B (`4145dddb…`, E-FB); **aktueller Zustand erst per Preflight-Lesung beweisbar** (Gate G1P-0). Geräte-MAC siehe private E-P0-Evidence, nicht veröffentlicht |
| Rückfallartefakte | `rollback/`: `image_ed29c9c_release_app.bin`, `image_c0_9a9a8bd_release_app.bin`, `flash_factory_app_backup_b9564e6.bin` (SHA siehe `SHA256SUMS` dort); zusätzlich `esp32_release` (C5) als bevorzugter Rückfall |
| Vorhandene Evidenz für F4 (übernommen, nicht wiederholt) | native Tests Skip/Resume/Reset-Hold (`SIM-27-H-*`, `SIM-19-R08`); G1-IMAGE01-Lauf: 41 gültige UI-Aggregate, beobachteter maximaler Checkpoint-Bedarf 8 688 B, Vektorbound 6 656 B, `threshold_status=NOT_QUALIFIED`, `touch_contact_evidence=NOT_PROVEN`, physisches Hold/Skip/Release/neuer Kontakt `NOT_PROVEN` |

## 2. Gate G1′ – F4-Render-/Touch-Heapbedarf (Diagnose-Overlay IMAGE01)

Zweck: **nur fehlende** Hardwareevidenz: realer Heapbedarf von Render/Touch auf erreichbaren Seiten **mit Touchkontakten** und Locale-Varianten, mit normalem 5-s-Polling eines Browsers als einziger Netzwerklast.

**Image:** IMAGE01 (App-only `0x10000`, 1 760 560 B). Keine Änderung an Overlay, Produkt oder Werkzeugen (`g1_capture.py` @ `16d0349` für UART; `c0_load.py` **nicht** verwendet).

| Schritt | Aktion | Hardware |
|---|---|---|
| G1P-0 | Lesesitzung `read-flash 0x0 0x400000` (4 MiB, privat, 0600), Identität (Chip/MAC), Partitionstabelle `d7f180e4…`, aktuelle App = Image B (`4145dddb…`) bestätigen; Baseline für Nicht-App-Bereiche | 1 esptool-Sitzung, 2 Resets |
| G1P-1 | App-only-Flash IMAGE01 bei `0x10000` (esptool `--flash-mode keep …`), Verifikation durch 4-MiB-Readback: Boot/NVS/PHY, Partitionstabelle, Rest, State-Store byteidentisch zu G1P-0; App = IMAGE01 | 1 Schreib- + 1 Verifikationssitzung, 4 Resets |
| G1P-2 | Frischer Boot, UART-Mitschnitt (`g1_capture.py`), Browser (Login, normales 5-s-Polling), Owner bedient die **vorhandenen** UI-Seiten per Touch: Home, Menüs, Uhr/Zurück, erreichbare Status-/Netzwerk-/Programmseiten, Sprachen de/en/es (soweit umschaltbar) mit kurzen Einzelkontakten; ca. 15–20 min | 1 Portöffnungsreset |
| G1P-3 | Auswertung (`g1_analyze.py`): gemessener Bedarf je Seite/Locale/Phase mit Coverage; Schwellenqualifikation nach Plan §4 `max(a,b)×2` **nur für abgedeckte Pfade**; Rest `NOT_PROVEN` | — |

**Ausdrücklich nicht:** künstliches Heap-Erschöpfen/OOM, Last (P-A…), 5-s-Vollhold bzw. Halten der Werksreset-Fläche, echter Werksreset, Programmstart, Sensor-/Aktorzugriff.
**Erwartete Grenzen:** Skip-/Resume-Pfad der Hardware ist ohne Speicherdruck unter sicheren Umständen nicht erreichbar ⇒ `NOT_PROVEN` (nativ abgedeckt); Reset-Hold-Sperre auf Hardware `NOT_PROVEN` (nativ abgedeckt). Fehlende Coverage hält die Schwellenqualifikation `HOLD`; es werden keine Zahlen geschätzt.

**Stopp (hart):** `heap_alloc_failed`, Panic/Abort/Watchdog/Brownout, unerwarteter Reset (nur der erste `rst:0x1` ≤ 10 s nach Portöffnung akzeptiert), Uptime-Regression, Heartbeat-Lücke, Stack-HWM schlechter als Baseline um > 256 B, Browserfehler/Timeouts, Owner-Abbruch. Bei Anomalie: Evidence sichern, STOP/HOLD, **kein** automatischer Rückflash, kein Retry.
**Abnahme G1′:** keine harten Stoppereignisse; Messwerte und Coverage berichtet; Qualifikation je Pfad `PASS`/`NOT_PROVEN`.

## 3. Gate G2-Web – reines Produktimage, Webteilnachweis (ausdrücklich `PARTIAL`)

Image: **`esp32_release` C5** @ `162a667`, App `ebb296c0…6633` (App-only `0x10000`, 1 756 528 B). Setzt G1′ abgeschlossen voraus (separater Ownerentscheid, sonst nicht).

| Schritt | Aktion | Hardware |
|---|---|---|
| G2W-0/1 | App-only-Flash `esp32_release` bei `0x10000` + 4-MiB-Verifikation (Nicht-App-Bereiche byteidentisch zu G1P-0; App = Gate-Hash) | 2 Sitzungen, 4 Resets |
| G2W-2 | Frischer Boot, UART-Mitschnitt; **ein** Browser: Login, Shell, Status, Standard-5-s-Polling **15 min**, normale Touch-/UI-Bedienung ohne Reset | 1 Portöffnungsreset |
| G2W-3 | Browser schließen; Gerät **20 min** ohne Browser (UART läuft weiter) | — |
| G2W-4 | Browser neu öffnen: Erreichbarkeit, Login-/Sessionverhalten (Sessionablauf 30 min Inaktivität beachten), Anzeige, aktueller Standby-/Sensorfehlstatus; ≤ 10 min Beobachtung | — |

Dokumentiert: UART-Fehler/Resets, Heap-/Blocktrend (nur Trend, **kein** 90-%-`R_ref`), Stackreserven, Browser-Konsole/Netzwerkfehler/Timeouts, UniFi-AP-/RSSI-/Retry-Daten nur soweit bereits verfügbar (`NOT_AVAILABLE` sonst). Keine RSSI-Schranke, keine Kausalbehauptung.
**Nicht beanspruchbar (D9):** U2/U2R, gültiger Programmstart, autonome Regelung eines Programms, `R_ref`-Vergleich, R1-U-Gesamt-PASS. Ergebnisstatus höchstens `PARTIAL`.
**Stopp/HOLD:** wie G1′; zusätzlich reproduzierbares Erreichbarkeitsproblem im normalen Browserablauf ⇒ HOLD mit konkretem Browserfehler (Ursache offen); kein O2/O3/`WIFI_PS_NONE`.

## 4. Hardwaregrenzen (für beide Gates)

Aktorfrei (`real actuators: disabled`), keine Erase-all-/NVS-/Kalibrierungsänderung, Schreiben nur `0x10000`.
Rückfall: Hash-geprüftes `esp32_release`-App-Image bzw. die genannten `rollback/`-Artefakte, jeweils App-only; kein Rückflash bei Anomalie ohne Ownerentscheid.
Passwort nur aus `~/pw`; Rohdaten privat (`hw-evidence-pr203/diag/g1p/`, 0700), veröffentlicht werden nur sanitierte Ableitungen.
Gesamte Hardwareaktionen G1′: 3 esptool-Sitzungen + 1 Portöffnung; G2-Web: 2 Sitzungen + 1 Portöffnung.

## 5. Ownerbestätigungen (konkret, einzeln)

1. G1′ auf Image IMAGE01 (App `da9e8868…`) ausführen: **Ja/Nein**.
2. Reihenfolge G1′ → G2-Web mit separater Bestätigung vor G2W-0: **Ja/Nein**.
3. Owner bedient das Display (Touch) während G1P-2 und beobachtet am Gerät: **Ja/Nein**.
4. Browserkontext (Browser/Version) und UniFi-Daten für das Zeitfenster bereitstellen: **Ja/Nein**.
