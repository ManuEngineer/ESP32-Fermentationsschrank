# Issue #190 – Restnachweise am Testaufbau auf dem integrierten #219-Stand (Plan, Revision 1)

Status: **Entwurf zur Ownerfreigabe (D1 offen).** Bis zur Freigabe der exakten
Plan-Commit-SHA: keine Implementierung, kein Flash, keine Hardwareaktion.
`FLASH=NO`, `ACTUATOR_RELEASE=NO`, `REAL_PELTIER_TEST=NOT_RUN`.

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
  `APP_ISSUE_30_SENSOR_COMMISSIONING`. Gepruefter Probebau (nur Build, kein Flash):
  Source-HEAD `4b496b4` sauber (`SOURCE_TREE_CLEAN=YES`), `ISSUE30_COMMISSIONING=
  INCLUDED_IN_BRINGUP_ONLY`, Datei `build/esp32_bringup_issue30/
  esp32_fermentationsschrank.bin` 1 828 336 B, SHA-256
  `14e155c2eb032c685b698800e758bc6ae207814ff7e92b50bd59533e72d3a8f6`
  (Arbeitsbaum-Build; verbindlich ist der spaetere Clean-Tree-Build nach Freigabe).
- Dieses Image enthaelt den **integrierten Produktpfad**: Die #219-Kettenanbindung
  (`attachControlChain`, nur fuer #32/#33-Harnesses ausgeschlossen), der
  `ControlSensorEvidenceProducer` und der Sampler sind in `app_main.cpp`
  unbedingt komponiert; das Harness haengt zusaetzlich nur lesend an
  `ds18b20Sampler.source(channel)` – derselben Quelle, die der Evidenzproduzent
  verwendet. Kein zweiter Messpfad.
- Das Harness gibt bereits aus: `ISSUE30_BINDING engine_binding_valid=…
  record_present=…` und `record_air/heatsink/product`-ROMs (Bindung geladen),
  `ISSUE190_READING channel=… status=… celsius=… rom=… timestamp_ms=… age_ms=…`
  alle ca. 2 s je Kanal (Frischeevidenz gegen `monotonicMillis`) und
  `ISSUE190_RESOURCES task_stack_hwm_bytes=… free_heap=… largest_free_block=…`
  alle ca. 10 s. Alle Zeilen laufen ohne UART-Eingabe.

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
  Images (Source-HEAD, SHA-256, Partitionen dokumentiert), ca. 150–300 s passiv:
  (1) `ISSUE30_BINDING engine_binding_valid=1` mit den drei erwarteten ROMs,
  (2) je Kanal `Ok` mit korrekter Rolle/ROM, `age_ms` begrenzt (erwartet < 2,5 s wie
  in M2), Zeitstempel fortschreitend,
  (3) HWM-Zeilen, (4) Heartbeat-Luecken, kein Panic/WDT.
  Kein `bind`/`offset`/`clear`, keine Sensoreingriffe, kein M1–M6-Wiederholen.
- **C3 – Evidence/Doku (Docs-Commit):** neue SHA-gebundene Evidence-Sektion in
  `docs/audits/ISSUE190_DS18B20_BRINGUP_EVIDENCE.md` bzw. eigene Datei;
  `docs/ACCEPTANCE_TESTS.md` `HW-30-03/-06` nur auf belegte Ergebnisse
  synchronisieren, Rest bleibt `PARTIAL`/`NOT_RUN`; `docs/ROADMAP.md` (§6).
- **Restore (nach C2):** Rueckkehr auf ein Release-Image, **gebaut aus dem dann
  aktuellen `main`** (App-only-Flash, gleiche Verifikation). Das aktuell laufende
  Release `cad364c0…` (Source `c782156`, vor #219) ist nicht bitgleich
  wiederherstellbar, sofern der Build nicht aufbewahrt ist (Owner prueft, D3).

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
   `ISSUE30_READY … real_actuators_enabled=NO`, Peltier-Bruecke `ready_all_off`;
   keine Ausgabezeile ausser inaktiv. Offen zu verifizieren: Der Logtext
   `peltier bridge: ready_all_off_not_connected_to_planner` (`app_main.cpp:668`)
   bleibt im Image textlich erhalten, obwohl der Planner seit #219 angebunden ist;
   Bedeutung vor dem Boot gemeinsam klaeren (kein Aktorbefehl, `ActuationInterlock`
   unveraendert; Parameter unkonfiguriert, daher keine `ControlRequest`).
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
7. **Rueckfall:** Restore-Weg gemaess §5 bereitgelegt und verifiziert gebaut,
   **bevor** geflasht wird.

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

## 10. Owner-Entscheidungen / Gates

- **D1:** Freigabe der exakten Plan-SHA dieses Dokuments.
- **D2:** A (kein Codediff) oder B (DEV-Option §4, empfohlen).
- **D3:** Restore-Image: Build aus aktuellem `main` akzeptiert, oder ist das
  Image `cad364c0…` (Source `c782156`) bitgleich vorhanden und zu verwenden?
- **D4:** `CrcFault`/`BusFault` bleiben an Hardware `NOT_RUN` (Empfehlung), oder
  nennt der Owner einen konkreten sicheren Weg?
- **G2 (spaeter, getrennt):** ausdrueckliche Flash-/Hardwarefreigabe nach dem
  Owner-Check §7 mit konkretem Image-Hash. Ohne G2: `FLASH=NO`.
