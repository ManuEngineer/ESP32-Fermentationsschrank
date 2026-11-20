# Issue #190 – Restnachweise am Testaufbau auf dem integrierten #219-Stand (Plan, Revision 2)

Status: **Revision 2 nach Independent Review zur Fix Verification und
Ownerfreigabe (D1 offen).** Bis zur Freigabe der exakten Plan-Commit-SHA: keine
Implementierung, keine Builds, kein Flash, keine Hardwareaktion.
`IMPLEMENTATION=NOT_STARTED`, `FLASH=NO`, `ACTUATOR_RELEASE=NO`,
`REAL_PELTIER_TEST=NOT_RUN`.

Rev. 2 behebt genau die drei Review-Blocker (B1 Bindungsnachweis per
read-only-Befehl, B2 echter Rueckfallweg, B3 Roadmap-Konsistenz) und die
Klarstellungen zu Logtext, Planungsbuild und `age_ms`; Rev. 1 (`de4c145`) ist
ersetzt.

**Hinweis Planungsbuild:** In Rev. 1 wurde ein Probe-Image gebaut
(`build_issue30_sensor_commissioning.py`), obwohl `docs/CI_AND_QUALITY_GATES.md`
fuer die Planung keine Builds vorsieht. Dieser Build wird weder rueckwirkend
freigegeben noch wiederholt; seine Werte sind **kein** Nachweis und keine
Plangrundlage. Massgeblich ist ein spaeterer Clean-Tree-Build nach Freigabe.

Basis: `origin/main` `4b496b4274b33539f23b079e4035f79c55f0b785` (Merge PR #219,
R1-Regelungs-/Aktorkette integriert; Issue #218 abgeschlossen; PR #213 ohne
Merge als ueberholt geschlossen). Ersetzt/ergaenzt den historischen Plan
`docs/tasks/issue-190-ds18b20-bringup-plan.md` Rev. 3 nicht; dessen Nachweise
(M1–M6, Release-Boot) bleiben unveraendert gueltig fuer ihre dokumentierten
Image-Bedingungen.

## 1. Ziel und Abgrenzung

Ziel: die noch fehlenden #190-Restnachweise (geladene feste ROM-Bindung, richtige
Rollen, *frische* Messwerte mit Zeit-/Altersangabe, Task-Stack-HWM) auf einem Image
nachweisen, das den **aktuellen integrierten Produktcode** enthaelt, und den
spaeteren sicheren Erstboot dieses Stands am Testaufbau vorbereiten.

Nicht-Ziele: kein neuer Sensortreiber, keine zweite Telemetrie/API, keine neue
Diagnoseoberflaeche, keine Aenderung von Release-Firmware, Sensorqualitaets-Gates,
`ActuationInterlock`, Planner, PI oder Parametern (#35), kein synthetisches
`Allowed`, kein Test mit Peltier/Last, keine Wiederholung von M1–M6, OUT1/OUT2/OUT4-
Pulsen oder des unbelasteten BTS7960-Tests, keine absichtlichen Kurzschluesse oder
Bus-Stoerungen.

## 2. Delta-/Evidence-Raster (nur Zusammenfassung; Quellen unveraendert)

| Punkt | Stand | Quelle |
|---|---|---|
| ROM-Erfassung, Rollenzuordnung (air `A60000006C724828`, heatsink `F90000006C4C6228`, product `CF0000006C415328`) | `PASS` (Probe-Image `e0da8ae`) | Evidence M1, `HW-30-04` |
| 10-min-Messreihe, 308 Zyklen je Kanal, Zyklus 1895/2000/2160 ms | `PASS` (Probe-Image) | M2, `HW-30-02` |
| Hot-Plug fest/Produkt, koordinierter Neustart | `PASS` (Probe-Image) | M3–M5 |
| Stack-HWM 4768 B frei von 6144 B (Bedarf ca. 1376 B), Heap, Heartbeat | gemessen unter **Probe-Budget 6144/2/1**; `PARTIAL` | M6, `HW-30-03/-06` |
| Release `cad364c0…` (Source `c782156`, Produktbudget 4096/2/1): Boot, `sampler: started`, 150 s stabil, NVS/`state_store` byteidentisch | `PASS` fuer Boot/Stabilitaet | Evidence Release-Test |
| Geladene Bindung und frische Werte **im Release**; Stack-HWM im Release | `NOT_PROVEN` (bewusst: keine Release-Ausgabe, Ownerentscheid D3) | Evidence Release-Test |
| Frische Werte/Bindung/HWM auf dem **integrierten #219-Stand** | **nicht gemessen** (neu, dieser Plan) | – |
| `CrcFault`/`BusFault` an realer Hardware | `NOT_RUN`; bewusst ausgeschlossen (keine Kurzschluesse/Stoerungen); Logik per Hosttests (`test_ds18b20_sampling`) | `HW-30-03/-05/-06` |
| #219-Kette auf einem daraus gebauten Image auf Hardware | `NOT_RUN` | Issue #33 Statusblock |

Historische PASS/PARTIAL/NOT_RUN werden nicht umetikettiert; neue Ergebnisse werden
als neue, SHA-gebundene Evidence ergaenzt.

## 3. Befund: vorhandene Beobachtungsmoeglichkeit (Repository-first, verifiziert)

Auf `main` `4b496b4` existiert sie bereits, **ohne Produktcodeaenderung**:

- `scripts/build_issue30_sensor_commissioning.py` baut `esp32_bringup` +
  `APP_ISSUE_30_SENSOR_COMMISSIONING` (Harness nur Bring-up, nie Release). Die
  Existenz des Pfades ist aus dem Quelltext belegt; ein Build gehoert nicht in
  diese Planungsphase (siehe Hinweis oben).
- Dieses Image enthaelt den **integrierten Produktpfad**: Die #219-Kettenanbindung
  (`attachControlChain`, nur fuer #32/#33-Harnesses ausgeschlossen), der
  `ControlSensorEvidenceProducer` und der Sampler sind in `app_main.cpp`
  unbedingt komponiert; das Harness haengt zusaetzlich nur lesend an
  `ds18b20Sampler.source(channel)` – derselben Quelle, die der Evidenzproduzent
  verwendet. Kein zweiter Messpfad.
- Das Harness gibt aus:
  - **ohne UART-Eingabe, periodisch:** `ISSUE190_READING channel=… status=…
    celsius=… rom=… timestamp_ms=… age_ms=…` alle ca. 2 s je Kanal
    (Frischeevidenz gegen `monotonicMillis`) und `ISSUE190_RESOURCES
    task_stack_hwm_bytes=… free_heap=… largest_free_block=…` alle ca. 10 s
    (`Harness::update()` vor der UART-Schleife);
  - **nur nach dem read-only-UART-Befehl `ds18b20 report`** (`Harness::
    processLine()` -> `Harness::report()`): `ISSUE30_SAMPLER running=…`,
    `ISSUE30_BINDING engine_binding_valid=… record_present=…`, die Bus-/ROM-
    Zeilen und `record_air/heatsink/product`. Die geladene Bindung wird also
    **nicht passiv** ausgegeben, sondern erst durch diesen einen Befehl sichtbar.

Grenzen dieser Beobachtung (ehrlich):
1. Das Harness-Image nutzt das **Probe-Budget 6144/2/1**, nicht das Produktbudget
   4096/2/1. Der gemessene Stackbedarf (benutzt = Stack − HWM) ist groessenunabhaengig;
   die Reserve unter 4096 waere daraus **abgeleitet**, nicht gemessen.
2. Mit der produktiven (unkonfigurierten) Sensorqualitaetskonfiguration liefert der
   Evidenzproduzent keine Evidenz; ein `Valid`-Sensorstatus der Kette ist deshalb
   **nicht** beobachtbar und wird nicht kuenstlich erzeugt. Nachgewiesen wird nur der
   Rohlesepfad (Quelle: Bindung, Rolle, Messwert, Alter), nicht die Regelfreigabe.
3. Eine erfolgreiche Kettenanbindung loggt das Image nicht (nur der Fehlerfall
   `attachActuationPorts rejected`); sie wird per Abwesenheit dieser Zeile und
   den Host-/Integrationstests aus #219 belegt, nicht per Hardware.
4. Es ist ein DEV-Image, kein Release (Release hat gemaess D3 keine Ausgabe).

## 4. Optionen und Empfehlung (Entscheidung D2)

- **A – nur vorhandenes Harness-Image, kein Codediff.** Beantwortet Bindung, Rollen,
  frische Werte mit Alter und HWM unter Probe-Budget auf dem integrierten Stand.
  Stack-Reserve unter 4096 nur abgeleitet.
- **B – A plus kleinste DEV-Option (empfohlen):** Im bereits bestehenden
  `#ifdef APP_ISSUE_30_SENSOR_COMMISSIONING`-Block von `app_main.cpp` (Probe-Budget)
  wird stattdessen `kApprovedDs18b20TaskBudget` (4096/2/1) uebergeben; Log
  `ds18b20 budget: approved product budget (bring-up harness)`. Nur dieser Block, rein
  buildseitig (nur `esp32_bringup`+Harness), Release-Pfad unveraendert (per
  `strings`-/Build-Vergleich zu pruefen). Vorteil: der HWM wird unter dem **exakten
  Produktbudget** gemessen; ein zu kleiner Stack wuerde sichtbar (Canary/Panic) statt
  abgeleitet. Diff ca. 10 Zeilen, ersetzt eine Ownerentscheidung (D2 aus Rev. 3
  gilt weiter fuer Probe-Messungen) nicht – er verwendet den freigegebenen Wert.
- **C – neue Release-Telemetrie/Diagnoseausgabe:** verworfen (Ownerentscheid D3).

`CrcFault`/`BusFault`: Es ist kein geeigneter, nicht destruktiver Hardwareweg
identifiziert (Ownerentscheid: keine Kurzschluesse/Stoerungen). Bleiben `NOT_RUN`
fuer Hardware, `PARTIAL` fuer HW-30-03/-05/-06; Logik ist per Hosttests abgedeckt.
Nur wenn der Owner einen konkreten sicheren Weg benennt, gesondert planen.

## 5. Umsetzungsschnitte (nach Freigabe)

- **C1 (nur bei D2=B):** Budget-Auswahl im Harness-Block wie in §4; gezielte Tests:
  `test_ds18b20_sampling`, `test_r1_control_chain_integration`,
  `check_architecture_boundaries.py`, Builds `esp32_release` (Strings-/Groessenvergleich
  zum Stand `main`: unveraendert) und Harness-Build; Self-Check. Kein Flash.
- **C2 – Hardwarenachweis (nur nach G2):** je ein Boot des sauber gebauten Harness-
  Images (Source-HEAD, SHA-256, Partitionen dokumentiert), ca. 150–300 s:
  1. Passiv beobachten (ohne UART-Eingabe): `ISSUE190_READING` und
     `ISSUE190_RESOURCES`.
  2. **Genau einmal** nach dem Boot der read-only-Befehl `ds18b20 report`
     (liefert `ISSUE30_BINDING`, Bus-/ROM-Zeilen und die `record_*`-ROMs). Kein
     anderer Befehl: `bind`, `offset`, `clear` und jedes weitere Schreibkommando
     sind verboten; kein zusaetzlicher Daten- oder Telemetriepfad.
  3. Auswertung: `engine_binding_valid=1` mit den drei erwarteten ROMs/Rollen;
     je Kanal `Ok` mit richtiger Rolle/ROM; fortschreitende Zeitstempel; Zykluszeit
     und `age_ms` im Rahmen der Referenz aus M2 (Richtwert: `age_ms` typisch unter
     ca. 2,5 s, Zeitstempel ca. 2-s-Takt; Abweichungen werden mit Zeitstempeln,
     Zykluszeit und Scheduling gewuerdigt und nicht allein am Schwellwert
     entschieden); HWM-Zeilen; Heartbeat-Luecken; kein Panic/WDT.
  4. **PASS-Regel:** `engine_binding_valid=1`, ROM-Zuordnung und Messfrische
     werden erst dann als `PASS` gewertet, wenn diese Werte tatsaechlich auf der
     geprueften Firmware (Image-Hash) beobachtet und protokolliert wurden;
     bis dahin `NOT_RUN`/`NOT_PROVEN`.
  Keine Sensoreingriffe, kein M1–M6-Wiederholen.
- **C3 – Evidence/Doku (Docs-Commit):** neue SHA-gebundene Evidence-Sektion in
  `docs/audits/ISSUE190_DS18B20_BRINGUP_EVIDENCE.md` bzw. eigene Datei;
  `docs/ACCEPTANCE_TESTS.md` `HW-30-03/-06` nur auf belegte Ergebnisse
  synchronisieren, Rest bleibt `PARTIAL`/`NOT_RUN`; `docs/ROADMAP.md` (§6).
- **Restore (Rueckfall nach C2 oder bei Abbruch):** Rueckfall bedeutet Rueckkehr auf
  das **vor dem Test tatsaechlich lauffaehige App-Image**, nicht ein neuer Build.
  Ein Build aus `main` ist ggf. ein neues *Ziel*-Image, kein Rueckfall-Image.
  Pruefbefund (read-only, Dateisuche im Projekt-/Evidenzbereich): Das zuletzt
  geflashte Release `cad364c0…` (Source `c782156`, 1 779 216 B) ist **lokal nicht
  archiviert** gefunden worden; `build/esp32_release` enthaelt einen anderen Stand.
  Vorgehen, in Reihenfolge:
  1. bevorzugt das archivierte, exakt identische `.bin` (SHA-256 gegen
     `cad364c0a9e58024d71ace26f2d71a582e456a59c863d97d7159d7fdf6e1d6db`) lokal
     bereitlegen;
  2. andernfalls **vor** dem G2-Flash ein read-only App-Readback der Partition
     `factory` (`0x10000`, 3008 KiB; nur Lesen) mit ueberpruefter Imageidentitaet:
     Image-Header/Pruefsumme und angehaengte SHA-256 (`esptool image-info`),
     Vergleich der App-Descriptor-/ELF-SHA mit der Boot-Log-Angabe `ba0da9081…`,
     SHA-256 der Readback-Datei und des auf Imagelaenge gekuerzten Images,
     beides im Evidence festgehalten;
  3. Wiederherstellungsverfahren technisch vorbereitet und trocken geprueft
     (fester App-only-Befehl bei `0x10000`, `default-reset`, Hash-Verifikation und
     `verify-flash`; ohne Schreiben pruefbar: Dateipfad, Hash, Imagelaenge <
     Partitionsgroesse).
  **Fehlt eine verifizierte lokale Kopie oder ist das Verfahren nicht vorbereitet,
  gilt `G2=BLOCKED`.** Kein `erase_flash`, kein Eingriff in Bootloader,
  Partitionstabelle, NVS oder `state_store`.

## 6. Dokumentationsabgleich (Roadmap, kein Statusduplikat)

`docs/ROADMAP.md`: #213 `CLOSED` ohne Merge (ueberholt); #218 durch PR #219 erledigt
(Merge-Commit `4b496b4`); #190/#32/#33 bleiben `OPEN / BLOCKED_HARDWARE` mit ihren
konkreten Restnachweisen; neuer Verweis auf diesen Plan. Issue-Bodies #32/#33/#190
wurden am 2026-11-20 bereits ergaenzt und werden **nicht** erneut mit Statusblöcken
dupliziert; Issue-Updates erst nach belegten Ergebnissen und nur auf Ownerwunsch.

## 7. Sicherer Bench-Uebergang – Owner-Check vor dem ersten Boot (G2)

Nichts davon wird vermutet; jede Zeile bestaetigt der Owner **am Geraet**:

1. **Image:** exakter Source-HEAD, sauberer Baum (`--require-clean-source-tree` bzw.
   `SOURCE_TREE_CLEAN=YES`), SHA-256 der `.bin`, Gesamtgroesse < 3008 KiB-Partition;
   Partitionstabelle read-only gegen Build identisch (`nvs 0x9000`, `phy_init 0xf000`,
   `factory 0x10000/3008K`, `state_store 0x300000/1M`).
2. **Versorgung/Lasten:** welche Quellen real anliegen (ESP32, 12-V-Zweig, BTS7960,
   Lueftersummen), und welche Lasten tatsaechlich angeschlossen sind
   (Peltier, Luefter). Aktueller Aufbau wird nicht angenommen.
3. **Physisch/elektrisch wirksame Aktorsperre:** Peltier-/BTS7960-Leistungspfad
   getrennt oder stromlos (nicht nur Software); Lueftersummen und
   Summer-Bestueckung bestaetigt; Software-LOW ist kein Sperrnachweis.
4. **Erwarteter fail-closed-Zustand im Log:** `onboard outputs: … ready_inactive`,
   `ISSUE30_READY … real_actuators_enabled=NO`; keine Ausgabezeile ausser inaktiv.
   **Logtext `peltier bridge: ready_all_off_not_connected_to_planner`
   (`app_main.cpp:668`) ist nach #219 veraltet.** Er kann hoechstens die
   Initialisierungsausgabe der Bruecke ("bereit, alle Richtungen/Enable AUS")
   bedeuten; er sagt **nichts** ueber fehlende Software-Anbindung (die Kette ist
   seit #219 im Code angebunden), nichts ueber eine Produktfreigabe und nichts
   ueber eine physische Aktortrennung. Er wird vor G2 im Owner-Check so erklaert
   und **nicht** als Trennnachweis gewertet. Keine stille Produktcodeaenderung
   fuer einen Log-Fix in diesem Plan. Parameter bleiben unkonfiguriert, daher keine
   `ControlRequest`; `ActuationInterlock` unveraendert.
   **Physische Sperre unabhaengig bestaetigen:** Der Owner bestaetigt am Geraet
   die elektrisch wirksame Trennung/Stromlosigkeit des Peltier-/BTS7960-
   Leistungspfads (z. B. abgezogene Leistungsversorgung/Klemmen). Software-LOW,
   Log-Text, `EN` oder Reset gelten **nicht** als physische Leistungsabschaltung.
5. **Abbruchkriterien (logbasiert):** Panic/WDT/Abort/Backtrace, `E`-Zeilen ausser
   den dokumentierten vorbestehenden Warnungen, `ISSUE190_RESOURCES task=absent`,
   `ds18b20 sampler` ungleich `started`, jeder Kanal dauerhaft nicht `Ok` ohne
   Sensoreingriff, Heartbeat-Luecke > 3000 ms (Vorwert max. 1343 ms), unerwartete
   Ausgangsaktivitaet; bei Abbruch Strom/EN trennen und Restore.
6. **Flash:** einmalig App-only bei `0x10000` (`default-reset`, FT232 `/dev/ttyUSB0`),
   Hash-Verifikation plus `verify-flash`; **kein** `erase_flash`, kein Bootloader-/
   Partitionstabellen-/NVS-/`state_store`-Schreiben; Readback von NVS/`state_store`
   vorher/nachher byteidentisch (Bindung bleibt erhalten); Port waehrend der Sitzung
   offen halten. Nur nach separater ausdruecklicher Flash-/Hardwarefreigabe.
7. **Rueckfall:** verifizierte lokale Kopie des vor dem Test lauffaehigen
   App-Images (Hash geprueft) und vorbereiteter App-only-Restore gemaess §5;
   sonst `G2=BLOCKED`.

## 8. Kleinste Voraussetzungen fuer den spaeteren #33-Owner-Gate-Test mit Peltier
(nicht Teil dieses Plans, nicht implementiert, nicht ausgefuehrt)

- Eigener Plan und Owner-Freigabe; `ACTUATOR_RELEASE` bleibt bis dahin `NO`;
  keine Umgehung des Interlocks, keine Testwerte als Freigabe.
- Dokumentierte Versorgung/Strombegrenzung des Peltierzweigs, Sicherung und
  einmalige Temperatursicherung (HW-33-03), Kuehlkoerper und funktionsgeprueften
  Aussenlueftern/Innenlueftern; Pflichtsensoren (Luft, Kuehlkoerper) belegt frisch
  und rollenrichtig (dieser Plan).
- Begrenzte Servicepulse mit Software-Timeout **und** physisch wirksamem
  Abschaltweg; Abschalt-/Fehlerweg (Sensorentfernung, Reset) vorab festgelegt.
- Grenz-/Schaltzeitwerte stammen aus #34/#35, nicht aus dem Test; Vollabnahme
  #36/#37. Bereitschaft der Komponenten ersetzt keine Commissioning-Freigabe.

## 9. Tests und Nachweise (lokal, ohne Hardware)

Gezielt: bei D2=B die in §5 C1 genannten; bei D2=A keine Codeaenderung, daher nur
Dokumentationspruefung (Links/Format). Volles Pre-Ready erst nach Independent
Review auf Owneranweisung. Nicht Ausgefuehrtes bleibt `NOT_RUN`.

## 10. Owner-Entscheidungen / Gates (Empfehlungen, **keine** Ownerfreigaben)

- **D1:** Freigabe der exakten Plan-SHA erst nach Rev. 2 und erneuter kurzer
  Fix Verification.
- **D2 (bevorzugt B):** vorhandenes DEV-Harness mit `kApprovedDs18b20TaskBudget` =
  `{4096, 2, 1}` (§4), nur nach expliziter Planfreigabe; Release-Pfad unveraendert.
  Stack-HWM direkt unter dem Produktbudget messen; die Probe-Stack-Naeherung
  (6144) wird nicht als Produktnachweis ausgegeben. Bei A bleibt die Reserve unter
  4096 ausdruecklich nur abgeleitet.
- **D3:** Restore auf das bekannte bisherige App-Image (§5), nicht auf einen
  beliebigen `main`-Rebuild. Liegt das Image nicht vor, Readback mit verifizierter
  Identitaet planen; bis zum Nachweis kein G2.
- **D4:** keine kuenstlichen Bus-/CRC-Fehler per Hardware. Bestehende Hosttests
  (`test_ds18b20_sampling`) werden referenziert; nicht ausgefuehrte reale Faelle
  bleiben `NOT_RUN`/`PARTIAL`; #190 wird damit nicht vollumfaenglich als erledigt
  deklariert.
- **G2 (spaeter, getrennt):** Owner-Entscheidung mit exaktem Image-Hash, am Geraet
  belegter Bench-Verdrahtung/Versorgung, physisch gesperrtem Peltier-Leistungspfad,
  verifiziertem Restore (§5) und Abbruchkriterien (§7). Ein DEV-Test des
  Sensorlesepfads beweist weder Release-Sensorevidenz noch eine Peltier-Freigabe;
  der Aufbau besteht aus den definitiven Komponenten im provisorischen Testaufbau
  ausserhalb des Schranks, ein Schrank-Thermiknachweis folgt daraus nicht.
  Ohne G2: `FLASH=NO`.
