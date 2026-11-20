# Issue #190 – Abschlussbericht und Abnahmematrix H1–H7 (Stand 2026-11-20, PR #220)

Status: **fachlich abschliessbar, nicht geschlossen.** Die Schliessung erfolgt erst nach
der abschliessenden Fix Verification bzw. dem Merge von PR #220 und ausdruecklicher
Ownerfreigabe. `OPEN_BLOCKERS=0`.

`C1=DONE` · `C2=PASS` · `G2=CONSUMED` · `FLASH=DONE` (einmaliger autorisierter
App-only-Flash; **kein weiterer Flash freigegeben**) · `ACTUATOR_RELEASE=NO` ·
`REAL_PELTIER_TEST=NOT_RUN`.

Kategorien: **1 Erfuellt** (konkrete Evidence) · **2 Nicht mehr erforderlich**
(Ownerentscheid bzw. bewusst reduzierter R1-Abnahmeumfang) · **3 Anderweitig
zugeordnet** (spaetere Kalibrierung, Regelungs-/Aktorfreigabe, Gesamtabnahme).
Nicht ausgefuehrte Tests bleiben historisch `NOT_RUN`/`NOT_PROVEN`; Kategorie 2 und 3
machen sie **nicht** zu `PASS`.

Abgrenzung: Der in C2 beobachtete Pfad ist der **DEV-Sensorlesepfad** (Bring-up-Harness auf
dem integrierten #219-Stand). Er ist kein Nachweis einer vollstaendig geprueften produktiven Regelung:
`Valid`/Regelfreigabe fehlt bis #35 (Sensorqualitaetskonfiguration unkonfiguriert), Release
hat keine Sensorausgabe.

## 1. Evidence-Quellen

- M1–M6, Probe-Image `e0da8ae`, Release-Boot `c782156`, C1 und C2:
  [ISSUE190_DS18B20_BRINGUP_EVIDENCE.md](ISSUE190_DS18B20_BRINGUP_EVIDENCE.md)
- Akzeptanzzeilen `HW-30-01…06`: [../ACCEPTANCE_TESTS.md](../ACCEPTANCE_TESTS.md)
- Plan Rev. 3 (freigegeben `10cc3f7c9c86e92f0b1fe8ffa2d31808e77222d2`):
  [../tasks/issue-190-restnachweise-integrated-chain-plan.md](../tasks/issue-190-restnachweise-integrated-chain-plan.md)
- G2-Vorbereitung (historisch): [ISSUE190_G2_OWNER_GATE_PACKAGE.md](ISSUE190_G2_OWNER_GATE_PACKAGE.md)
- Software-/Fehlerpfade nativ: `SIM-30-01…10` (u. a. `test_ds18b20_sampling`, Fake-Bus)

## 2. Matrix H1–H7

| H | Anforderung | Kat. | Evidence bzw. Scopeentscheid |
|---|---|---|---|
| H1 | reale ROM-Erfassung | 1 | M1, C2 `ds18b20 report` |
| H1 | Entfernen/Anstecken, Neustart | 1 | M3/M4/M5 `PASS` |
| H1 | keine alte Messung als neu | 1 | M3/M4 `MissingSample` ohne Temperatur; C2 Zeitstempel/`age_ms` |
| H1 | Konvertierung ohne Pause der Hauptschleife | 1 (indirekt) | Heartbeat max. 1343 ms (M6) / 1108 ms (C2) bei Soll 1000 ms; Hauptschleife ruft keine Busfunktion (Architekturpruefung) |
| H1 | Aufloesung 9–12 Bit gezielt | 2 | Produkt nutzt feste 12 Bit; reduzierter M1–M6-Umfang (Ownerentscheid 2026-11-19, Plan Rev. 3) |
| H1 | reale CRC-Fehler | 2 | D4: keine erzwungenen Fehler; Abbildung nativ (`SIM-30-03`) |
| H2 | Zielverdrahtung (GPIO32 2 ROMs, GPIO33 1 ROM) | 1 | M1/M2, C2 |
| H2 | Zyklusdauer im 2-s-Takt | 1 | M2: 308 Zyklen je Kanal, 1895/2000/2160 ms; C2: 118 Zyklen, 1800/2000/2160 ms |
| H2 | 1000 Zyklen, 10 Neustarts | 2 | Ownerentscheid 2026-11-19 ersetzt sie durch ca. 10 min/M1–M6; keine neue Kampagne |
| H2 | Reset waehrend Konvertierung | 2 | Plan Rev. 3 §4 „nicht Teil“, nur bei konkretem Befund; keiner aufgetreten |
| H2 | Wiederinitialisierung | 1 | M5 (Neustart), C2 (Boot nach Flash), Bindung unveraendert |
| H3 | Task-Stack-HWM | 1 | Probe 4768 B frei von 6144 B (M6); **C2: 2728–2792 B frei bei 4096 B Stackbudget** (Bedarf ca. 1368 B) |
| H3 | Heap, groesster freier Block | 1 | M6, Release-Test, C2 (Minima 31 608 B / 29 696 B) |
| H3 | isolierter Taskstart-Heap-Delta | 2 | nicht isolierbar (parallele Netzwerkallokation); keine neue Ressourcenmessung ohne konkreten Befund |
| H3 | RMT-Kanaele | 2 | nicht per API messbar; aus Konfiguration abgeleitet (zwei Busse), Betrieb fehlerfrei (Plan Rev. 3 §4) |
| H3 | blockierte CPU-Zeit | 1 (indirekt) | Heartbeat-Abstaende (siehe H1) |
| H3 | Ownerfreigabe `kApprovedDs18b20TaskBudget` | 1 | C3 (PR #215), Budget `{4096, 2, 1}`; C2 misst darunter |
| H4 | `onewire_bus`-Version und Konvertierungsvariante | 1 | `dependencies.lock`: `espressif/onewire_bus 1.1.2`; Variante B (SKIP-ROM + 0x44, `Ds18b20OnewireBus::startConversionAll`) im Adapter; reale Funktion M2/C2 ohne Treiberfehler; kein Austausch noetig |
| H5 | ROM-Erfassung, Zuordnung, Commissioning-Eintrag, Dokumentation | 1 | M1: air `A60000006C724828`, heatsink `F90000006C4C6228`, product `CF0000006C415328`; `docs/OPEN_POINTS.md` Eintrag vorhanden |
| H6 | feste Sensoren einzeln abziehen | 1 | M3: `MissingSample` ohne Temperatur, kein vertauschter ROM, kein `Ok` |
| H6 | Produktfuehler Hot-Plug im Stillstand | 1 | M4 |
| H6 | Hot-Plug waehrend einer Konvertierung | 2 | Plan Rev. 3 §4 reduzierter Umfang; Logik nativ (`SIM-30-04`) |
| H6 | Bus stoeren | 2 | D4 und fruehere Ownerentscheidung (keine Kurzschluesse/Busstoerung); nativ (`SIM-30-03`) |
| H6 | ROM-Zuordnung, Wiedererkennung | 1 | M1/M3/M4/M5 |
| H6 | Offset-Wirkung an realen Sensoren | 3 | #34 (Offsets je ROM-Adresse) |
| H6 | reale Messwerte vs. Referenz/Diagnosemodell | 1 + 3 | Plausibilitaet zu den Ownereinstellungen (M1/M2/C2) belegt; Referenzvergleich/Temperaturverteilung in #34 |
| H7 | Evidence auf Implementierungs-Head, Aktoren getrennt/gesperrt | 1 | Probe `e0da8ae` (Aktoren getrennt); Release `c782156`; **C2: Image Source `63cddd3` (Code = C1 `0370b32`), Ausgaenge inaktiv, BTS7960 ohne Leistungsstufe/Peltier** |
| H7 | `logResources`-Vergleich, kein Panic/WDT/OOM | 1 | M6, Release-Test, C2 |

## 3. Sicherheitsrelevante Akzeptanzkriterien (keine pauschale Streichung)

| Kriterium | Stand |
|---|---|
| Fester Sensor fehlt → nie `Ok`/kein alter Wert | `1` M3; nativ `SIM-30-01/-03/-06` |
| Produktfuehler fehlt unterscheidbar von Fehler | `1` M4; nativ `SIM-30-04` |
| Luft-/Kuehlkoerpersensor fuer Peltierfreigabe erforderlich | Sensorseite `1` (M3); Entscheidung `Allowed` bleibt `3` → #35 |
| Hot-Plug ohne unkontrollierte Aktorfreigabe | Sensorseite `1`: in M3/M4 Aktoren getrennt, `ACTUATOR_RELEASE=NO`, Planner/Interlock ohne Parameter nie freigebend (#219-Integrationstests); Verhalten der gekoppelten Aktorkette bei Sensorverlust `3` → #33 (`HW-33-04` Sensorentfernung), #36 (Sensorabzug, Fehlerinjektionen), #37 |
| Reale `CrcFault`/`BusFault`-Pfade | `2` (D4, Ownerentscheid: fuer #190 nicht erforderlich); die verpflichtende Fehlerinjektionsmatrix von #36 bleibt unveraendert und wird hier nicht ersetzt |

Kein ursprueglich verbindliches Sicherheitskriterium ist weder nachgewiesen noch
zugeordnet noch ausgenommen → **kein STOP**, `OPEN_BLOCKERS=0`.

## 4. Offene Hinweise (keine Blocker)

- Die Aussage in #33 („restliche Pflichtsensor-/Fehlerevidence (#190)“) und die Zeile
  `HW-33-*` bleiben unveraendert; nach Schliessung von #190 sind diese Reste gemaess §3
  zugeordnet. Eine Textanpassung in #33 erfolgt nur auf Ownerwunsch.
- Das Board laeuft nach C2 das DEV-Harness-Image (`93c750db…`), nicht das Release; ein
  Wechsel braucht eine **neue** Freigabe (G2 ist verbraucht).
- Folgeissues fuer spaetere Pruefungen existieren bereits: #33 (BTS7960/Peltier-Gate),
  #34 (Offsets/Sensorvergleich/Thermik), #35 (Parameter, `Valid`/Regelfreigabe), #36
  (Hardwareabnahme/Fehlerinjektionen), #37 (7-Tage-Test). Es wird kein neues Issue/PR angelegt.
