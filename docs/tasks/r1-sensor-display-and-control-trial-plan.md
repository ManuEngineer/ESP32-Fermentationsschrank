# R1 – Temperaturanzeige und erster integrierter Regelversuch (Plan, Revision 1)

Status: **Entwurf zur Ownerfreigabe (D1 offen).** Bis zur Freigabe der exakten
Plan-Commit-SHA: keine Implementierung, keine Builds, kein Flash, keine
Hardwareaktion. `IMPLEMENTATION=NOT_STARTED`, `FLASH=NO`, `ACTUATOR_RELEASE=NO`,
`REAL_PELTIER_TEST=NOT_RUN`.

Basis: `origin/main` `124418acfbe61b364c416261d476e3b80b5f4840` (Merge PR #220;
Issue #190 `CLOSED`). Betroffene bestehende Issues: #32, #33, #34, #35; spaetere
Abnahme #36, Dauerlauf #37. Es werden **keine** neuen Issues angelegt.

## 1. Ziel und Nicht-Ziele

Ziel: die vorhandene Software produktnah vervollstaendigen, sodass

1. alle drei DS18B20 ihre echten Temperaturen auf dem lokalen Display zeigen
   (ohne `SensorQuality::Valid` zu behaupten),
2. echte Messwerte kontrolliert in Qualitaets-, Regelungs- und Sicherheitskette
   gelangen koennen,
3. die tatsaechlich fehlenden Parameter-, Verbindungs- und Startbedingungen
   identifiziert und gezielt geschlossen sind,
4. danach ein erster **beaufsichtigter** Heiz-/Kuehl-Regelversuch ueber den
   vorhandenen Produktpfad moeglich ist, dessen Ergebnisse zugleich Nachweise
   fuer #32/#33/#34/#35 liefern.

Nicht-Ziele: keine Wiederholung von Luefterpuls-, unbelastetem H-Bruecken-,
Sensor-Grund- oder reinen Stabilitaetstests; kein paralleler Testcontroller,
keine zweite Aktorsteuerung, keine zweite DS18B20-Pipeline, kein direkter
Sensorzugriff im Renderer; keine Aenderung an `ActuationInterlock`, PI- und
DS18B20-Algorithmen, ausser wo unten ausdruecklich begruendet; keine
Peltierfreigabe, keine Freigabe von `ACTUATOR_RELEASE`; Release-Image bleibt
unveraendert fail-closed. Vollstaendige Fehlerinjektionen gehoeren zu #36, der
Langzeittest zu #37, Offsets/Thermik zu #34, finale Parameter zu #35.

## 2. Verifizierte Ausgangslage (Code `main` `124418a`)

- `main/app_main.cpp`: `Ds18b20Sampler` laeuft (Produktbudget 4096/2/1). Die drei
  Quellen speisen `ControlSensorEvidenceProducer`; die Qualitaetskonfiguration ist
  `kSensorQualityConfigUnconfiguredUntilIssue35 = std::nullopt`, daher liefert
  `sample()` **keine Evidenz** (`control_sensor_evidence.cpp`).
- `FermentationApplication::refreshUiSnapshot()` bezieht die drei Temperaturen
  ausschliesslich aus `resolveRuntimeEvidence().plausibility` (gespeist von
  `publishOwningRuntimeEvidence`). Ohne Evidenz bleibt jede Rolle ein leerer
  `SensorQualitySnapshot` (Qualitaet `Stale`, kein Wert) → Display zeigt nichts.
- Renderer (`main/fermentation_ui_renderer.cpp`) zeigt je Rolle Wert
  (`temperatureText`), Qualitaetstext (`sensorQualityTextKey`) und auf der
  Diagnoseseite Roh-/korrigiert-/gefiltert-Wert und Messalter. Die
  Anzeige-Infrastruktur existiert; es fehlt nur ein Wert.
- `ControlChainParameters`/`kControlChainParametersUnconfiguredUntilIssue35`
  (alles `0`): `classifyTemperatureControlParameters` → `Unconfigured`; der Regler
  liefert `Unavailable/NoCommissioning`, nie eine `ControlRequest`.
- `ActuatorPlannerParameters` kommen **ausschliesslich** aus
  `ServiceConfiguration.actuatorPlannerParameters` (persistiert, Codec V2 vorhanden),
  werden beim Fresh-Start per `FreshStartSnapshotProvenance::fromRuntimeLease`
  als Per-Run-Snapshot uebernommen und beim Run per `planner_->beginRun` gesetzt. Es gibt
  **keinen Produktions-Writer** (nur ein Test-Fixture setzt sie). Der Kommentar in
  `run_persistence_coordinator.cpp` stellt klar: Provenance-API ist actor-frei und
  injiziert nie Planner-Parameter.
- `kManualRunLimitsOwnerAvailable = false` (`fermentation_application.cpp:167`)
  sperrt **nur** `prepareStartManualHolding`, `prepareStartManualTimed`,
  `prepareStop(AbortAndCool)` und `prepareCompletion(startCooling)` mit
  `Unavailable`. `prepareStartProgram` ist **nicht** so gesperrt.
- Der positive Gesamtpfad ist digital bewiesen: `SIM-218-01` treibt eine reale
  `ControlRequest` durch Regler → Planner → Interlock → Driver bis zu den Aktoren,
  mit Test-Fixtures `testChainParameters()`, `testPlannerParameters()`,
  `testQualityConfig()` (`test_r1_control_chain_integration.cpp`). Der Produktpfad
  scheitert also an **Quellen von Werten**, nicht an fehlender Logik.

## 3. Befund: Spur der Aktorfreigabe-Evidenz

`FermentationApplication::tickControlChain()` baut `ActuationEvidence` je Tick aus
aktuellem Zustand; **kein Feld ist hart `false`**. Sie sind im Produkt aber
unerfuellbar, solange die Quellen fehlen:

| Feld (`ActuationEvidence`) | Produzent | Produkt heute |
|---|---|---|
| `bootValidationComplete` | `lifecycleState_ == Ready` | erfuellbar |
| `configurationValidated`, `configurationServiceMode` | Runtime-Lease + Epoche | erfuellbar |
| `persistenceValidated`/`persistenceLoadStatus` | Coordinator + `applicationReadiness()` | erfuellbar |
| `explicitActivationRequested`, `activationKind`, `processActivationApplied` | `freshStartActivation_` (nur expliziter Programm-/Hold-Start) | erfuellbar durch Start |
| `plannerEvidenceValidated` | `classifyActuatorPlannerParameters(planner.parameters())==Valid` | **nie** (kein Snapshot, G3) |
| `sensorEvidenceValidated` | `freshPublication` (Producer-Publikation) | **nie** (kein Producer-Output, G1) |
| `peltierSensor` (Kuehlkoerper) | `owningRuntimeEvidence_.cooling` | **nie `Valid`** (G1) |
| `sensorSelectionRuntime`, `actuatorPlanner` | Run-Zustand/Planner | erfuellbar |

Start-Admission (`makePreparedRequest`) verlangt `safetyAllowsStart && airSensorValid
&& coolingSensorValid` (aus `validSensor(plausibility.*)`); ohne Producer-Evidenz ist
jeder Start `Unavailable`. Die Gruende der drei Zustaende:

- **`Unavailable` (Request):** manuelle Laeufe/Kuehlplaene (`kManualRunLimitsOwnerAvailable`),
  sonst fehlende Sensorevidenz (Start-Admission verweigert).
- **`Unconfigured`:** PI-Parameter bzw. Planner-Parameter alle `0` (`NoCommissioning`).
- **`NotAllowed`:** `ActuationInterlock` ≠ `Allowed` (Planner/Sensor/Aktivierung nicht erfuellt).

## 4. Lueckenliste (Antwort auf die vier Pruefragen je Luecke)

Legende: V = Funktion vorhanden, nur nicht konfiguriert; X = fehlende produktive
Verbindung/Quelle; S = echte Safety-Voraussetzung; I = Zuordnung Issue.

| Nr. | Luecke | V/X/S | I |
|---|---|---|---|
| G0 | Keine Anzeige von Messwerten ohne Producer-Evidenz | X (Anzeigepfad) | dieser Plan (A) |
| G1 | `SensorQualityConfig` ohne Werte (ein Config fuer alle drei Rollen; Kuehlkoerper-Bereich weicht von Luft ab) | V + Werte fehlen | #35 (Filter, Rueckkehrstabilitaet), vorlaeufig Trial |
| G2 | PI-/`IntegratorTransitionPolicy`-/Luftbegrenzungswerte (`ControlChainParameters`) | V + Werte fehlen | #35 |
| G3 | `ActuatorPlannerParameters`: persistierter Slot vorhanden, **kein Writer** | X (Writer) + Werte | #35 (Werte), #106 (Bindung) |
| G4 | **Start ohne Regelbereitschaft:** `prepareStartProgram` prueft nicht, ob Kette, gueltige PI- und Planner-Parameter vorliegen. Sobald G1 gesetzt ist (Sensoren `Valid`), koennte ein Programm als aktiv laufen, **ohne dass geregelt wird** (`NoCommissioning` wird nirgends der Bedienung gemeldet) – dieselbe Klasse wie der Rebuild-Fehler aus #219 („Ready ohne Kette“) | X (Admission) | dieser Plan (B) |
| G5 | Technische Lauflimits fuer manuelle Laeufe (O5) | S/Owner | #34/#35; Entscheidung D4 |
| G6 | **Software-Sicherheitsbereich** (Sicherheits-Eingriffsgrenze, harte Notgrenze, Kuehlkoerper-Uebertemperatur) laut `docs/SAFETY_COMPONENT_FAULTS.md` §Temperaturueberwachung: im Code nicht vorhanden (nur PI-Luftbegrenzung, ausdruecklich keine Safety-Grenze; Interlock prueft nur Sensorqualitaet/Watchdog/Persistenz/Aktivierung) | **S – Voraussetzung nicht erfuellt** | #35 (festlegen), #36 (validieren); Entscheidung D5 |
| G7 | Einmalige Temperatursicherung (Hardware) nicht ausgewaehlt/montiert | S (Hardware) | #35 |
| G8 | Summer (OUT3): im Produkt nur der Ausgangs-Sink (`ready_inactive`); **kein** Treiber fuer Muster/Typ | offen | #32 (O3=B) |
| G9 | Kein Luefter-Tacho: Luefterausfall nur indirekt erkennbar | S | #35/#36 |
| G10 | `CrossRolePlausibilityContext.thermalCompatibility` bleibt `Unavailable` (kein Produzent); Auswirkung auf den Produkt-Sensormodus ist in S3 zu pruefen (Luftmodus nicht betroffen, `SIM-218-01` positiv) | V/X (pruefen) | #34/#35 |

Verifiziert **nicht** vorhanden (Code gelesen, `actuation_interlock.cpp` enthaelt keine
Temperaturschwellen) und damit nicht als „nur Konfiguration“ zu behandeln: G4, G6,
G3-Writer.

## 5. Teil A – Echte Temperaturen auf dem Display

### 5.1 Entwurf (kleinster Pfad, keine zweite Pipeline)

- **Quelle:** dieselben drei `ITemperatureSource`-Referenzen, die
  `ControlSensorEvidenceProducer` bereits haelt (`read()` je Kanal). Es entsteht keine
  zweite Messpipeline und kein zweiter Sensorzugriff im Renderer.
- **Rein anzeigende Eingabe, getrennt von Evidenz:** neue schmale
  Application-Eingabe `publishRawDisplayTemperatures(...)` (je Rolle: Wert, Alter,
  Identitaet, `fresh`), gespeichert unter dem bestehenden Application-Lock in einem
  eigenen Member. Sie wird **nie** von `resolveRuntimeEvidence()`,
  `validSensor()`, Start-Admission, `tickControlChain()`, Interlock, Planner oder
  Persistenz gelesen. `owningRuntimeEvidence_` und `publishOwningRuntimeEvidence`
  bleiben unveraendert (dort steht weiterhin nur qualifizierte Evidenz).
- **Frische-Regel:** angezeigt wird nur `status == Ok` **und** Alter ≤
  `kRawDisplayMaxAgeMs` (benannte firmwarefeste Konstante, Vorschlag 5000 ms
  ≈ 2,5 Zyklen; gemessen: Zyklus 1800–2160 ms, `age_ms` 889–1368 ms). Fehlend,
  fehlerhaft oder zu alt → kein Wert (`--`).
- **Vorrang:** liefert die qualifizierte Evidenz fuer eine Rolle einen Wert (spaeter,
  sobald G1 gesetzt ist), gilt die bestehende Projektion unveraendert; die
  Rohanzeige springt nur ein, wenn die qualifizierte Evidenz fuer diese Rolle leer ist.
- **Projektion:** `TemperatureView` erhaelt ein zusaetzliches Feld
  `TemperatureValueBasis {Qualified, RawUnqualified}` (Standard `Qualified`).
  `SensorQualitySnapshot.quality` bleibt bei Rohwerten `Stale` (nie `Valid`);
  `rawCelsius`, `lastAcceptedSampleAgeMs`, `identity` werden gefuellt. **Kein**
  neuer `SensorQuality`-Wert (der Enum ist in Persistenzcodec, Web-Codec und
  Renderer verdrahtet).
- **Renderer:** Statt `quality-stale` zeigt eine Rohbasis den neuen Textschluessel
  `quality-unqualified` (Warnton). Eine gemeinsame Hilfsfunktion in
  `fermentation_ui_text` ersetzt die zwei Direktaufrufe
  (`main/fermentation_ui_renderer.cpp` Status-/Diagnosezeile).
- **Web-JSON:** Aenderung am Wire-Vertrag wird vermieden: bei `RawUnqualified`
  liefert `web_json_codec` weiterhin `quality=stale` und `valueCelsius = null`
  (Rohwerte erscheinen nur lokal); Test sichert das. Falls der Owner Web-Anzeige
  wuenscht, ist das eine separate additive Erweiterung (D3).
- **app_main:** die vorhandene Hilfsfunktion `publishControlSensorEvidence` ruft
  zusaetzlich die Rohanzeige-Eingabe; keine neue Schleife, keine Zusatzlast ausser
  drei `read()`-Aufrufen je Durchlauf (Komplexitaetsgrenze beachten).
- **Bestandsregel (Nachfrage D3):** ein *qualifizierter* Snapshot mit `Stale`/`Failed`
  zeigt heute den letzten Wert mit Qualitaetslabel (`valueOf()` faellt auf `rawCelsius`
  zurueck). Das entspricht `SENSOR_TUNING_COMMISSIONING.md` (`STALE` = sichtbar
  veraltet). Vorschlag: unveraendert lassen; fuer `Failed` den Wert ausblenden nur
  auf Ownerwunsch.

### 5.2 Tests (digital)

Hosttests: Rohwert gueltig/frisch → Anzeige mit Basis `RawUnqualified`, Qualitaet
nie `Valid`; `MissingSample`/`CrcFault`/zu alt → kein Wert; Rollen entsprechen der
gespeicherten ROM-Bindung (Quelle liefert Rolle); Vorrang qualifizierter Evidenz;
**Isolation:** Rohanzeige beeinflusst weder `airSensorValid` noch Start-Admission noch
Interlock noch Persistenz (Assertion auf `resolveRuntimeEvidence`/`validSensor`);
Renderer-Text fuer Status- und Diagnoseseite; Web-JSON unveraendert (`null`).

## 6. Teil B – Regelbereitschaft und Parameterquellen

### 6.1 Start-Admission „Regelbereitschaft“ (G4, noetig vor jeder Sensor-Qualifizierung)

`prepareStartProgram` (und die manuellen Pfade, sobald freigegeben) verweigern mit
dem bestehenden `Unavailable`, wenn nicht gilt: Kette vorhanden **und**
`classifyTemperatureControlParameters(PI) == Valid` **und**
`classifyActuatorPlannerParameters(Lease-Parameter) == Valid`. Damit wird weder ein
Lauf ohne Regelung als „aktiv“ gestartet noch das Produktverhalten veraendert
(heute ist Start ohnehin wegen G1 verweigert). Test: Produktstandard (alles
unkonfiguriert) → Start `Unavailable`; Trial-Parameter + qualifizierte Sensoren →
Start moeglich.

### 6.2 Parameterquellen (nur explizit begrenzte, freigabepflichtige Testwerte)

Es gibt **kein** stilles Hardcoding im Release. Struktur:

- **Release/Produkt:** unveraendert die benannten Unconfigured-Konstanten
  (`kControlChainParametersUnconfiguredUntilIssue35`,
  `kSensorQualityConfigUnconfiguredUntilIssue35`); `peltier_composition_guard` und
  `SIM-218-00` bleiben gruen.
- **Trial-Profil (nur Bring-up):** neuer Build mit `APP_R1_CONTROL_TRIAL`
  (`esp32_bringup`, wechselseitig ausgeschlossen mit den #30/#32/#33/#90-Harnesses,
  nie im Release; eigenes Buildskript analog `build_issue30_sensor_commissioning.py`
  mit Release-Verbot). Das Profil liefert **explizit benannte** Testkonstanten
  (`TRIAL_NOT_COMMISSIONED`), beim Boot als Warnung geloggt:
  1. `SensorQualityConfig` **je Rolle** (Kuehlkoerper-Plausibilitaetsbereich
     getrennt; heute ein Config fuer alle drei → kleine, begruendete Erweiterung am
     Producer-Konstruktor),
  2. `ControlChainParameters` (PI je Richtung/Rolle, Luftbegrenzung, Policy),
  3. `ActuatorPlannerParameters`.
  Jeder Wert traegt Herkunft/Begruendung und Hardgrenzen (siehe Anhang A, Werte
  `PROPOSAL_NOT_APPROVED`, einzeln freizugeben).
- **Planner-Parameter (G3):** kanonischer Ort bleibt der persistierte
  `ServiceConfiguration.actuatorPlannerParameters` (kein Provenance-Bypass). Ein
  **schmaler Writer** `applyActuatorPlannerParameters(...)` nach dem Muster
  `applySensorCommissioning` (validiert per
  `classifyActuatorPlannerParameters`, Revisions-/Lock-Vertrag der vorhandenen
  Service-Commit-Mechanik), nur ueber UART-Kommandos des Trial-Harness
  (`planner set <Werte>`, `planner clear`) erreichbar; nie aus UI/Web. Das aendert
  `state_store` → eigenes Owner-Gate (G3) und expliziter **Clear-/Revert-Weg**;
  persistierte Trial-Werte werden nach dem Versuch per `planner clear` entfernt
  (Nachweis per `state_store`-Hash). Alternative (nicht empfohlen): nicht
  persistierter Provenance-Bypass – widerspricht dem Vertrag in
  `run_persistence_coordinator.cpp`.
- **Geltung/Ablösung:** Trial-Werte gelten nur im Trial-Image; Ablösung durch #34/#35
  erfolgt ueber dieselben Injektionspunkte (kein Umbau noetig); ein Digitaltest sperrt
  das Trial-Profil aus dem Release (`strings`-/Guard-Test, `esp32_release` ohne
  `TRIAL`-Symbole).

### 6.3 Manuelle Laeufe (G5)

Der erste Versuch nutzt den **vorhandenen Programmstart** mit einem kurzen
Benutzerprogramm aus dem vorhandenen Editor (`installed=true`, eigene Stufe/Ziel);
dafuer ist `kManualRunLimitsOwnerAvailable` nicht noetig, und O5 bleibt unberuehrt.
Manueller Halt-Lauf nur auf Ownerwunsch (D4): dann braeuchte es einen
Lauflimits-Owner (Trial: Band/Dauer/max. Erreichungszeit als Testwerte) –
Designaenderung an O5, deshalb nicht Teil des Grundumfangs.

### 6.4 Software-Sicherheitsbereich (G6) – Voraussetzung fuer Peltierleistung

Der laut Vertrag geforderte Eingriffs-/Notgrenzen-Mechanismus fehlt im Code und
**kann nicht durch Trial-Parameter ersetzt werden**. Peltierleistung wird deshalb an
**eine** der folgenden Bedingungen gebunden (Owner-Entscheidung D5):

- **S-A (empfohlen fuer den Versuch):** unabhaengiges Hardware-Abschaltsystem, das der
  Owner am Geraet bestaetigt (unabhaengige Strombegrenzung/Absicherung, mechanisch
  oder elektrisch wirksame Abschaltung, einmalige Temperatursicherung gemaess #35
  montiert, Wiederbeschaltung nur von Hand), plus beaufsichtigter Lauf mit
  Abbruchgrenzen; die Software-Luftbegrenzung bleibt aktiv, gilt aber **nicht** als
  Safety-Nachweis. Kein Produkt-/Dauerbetrieb.
- **S-B:** minimaler Firmware-Hard-Cutoff (Kuehlkoerper/Luft ueber/unter harter Grenze →
  beide Richtungen gesperrt, Luefter sicher, verriegelt) in diesem PR als eigener
  Schnitt vor Peltierstufe C2 (neuer Safetycode → zusaetzlicher Plan-/Review-Aufwand,
  Werte der Grenzen weiterhin Ownerfreigabe).

Ohne eine davon: **kein Peltierbetrieb**; Stufe C1 (BTS7960 ohne Leistungsstufe)
bleibt davon unberuehrt.

## 7. Teil C – Integrierter, gestufter Regelversuch (Hardware nur nach Owner-Gate)

Alle Stufen laufen ueber den Produktpfad (Programmstart → Regler → Planner →
Interlock → Treiber), nicht ueber UART-Aktorbefehle. Jede Stufe hat ein **eigenes**
Owner-Gate (G3 `NOT_GRANTED`; bereits verbrauchte Gates aus #190 gelten nicht):

| Stufe | Inhalt | Gate | Verwertbare Nachweise |
|---|---|---|---|
| C0 | Boot des Trial-/Anzeige-Images, Temperaturen auf Display (nur Anzeige aktiv, Aktoren sicher) | G-C0 | Messwerte erreichen Display (A); Rollen; Frische |
| C1 | Programmstart, BTS7960-Logik angeschlossen, **Leistungsstufe/Peltier physisch nicht versorgt**; Luefter und Summer angeschlossen | G-C1 | echte `ControlRequest`, Planner→Driver, Innen-/Aussenluefterzustaende, **Aussenluefter-Nachlauf bei Stop**, Stop/Fail-closed bei ungueltigem Sensor (Sensor abziehen), ohne Peltierrisiko → #32 |
| C2 | wie C1 mit Peltierleistung an strombegrenzter Quelle, Wärmeabfuhr, beaufsichtigt, unabhaengige Abschaltung, nur nach D5 | G-C2 | Heiz-/Kuehlanforderung, Sollwert erreichen/halten, Richtungswechsel/Totzeit, Sicherheitsabschaltung → #33/#34/#35 |

Beobachtungskriterien (je Stufe, Logs + Messmittel): echte Werte → Software/Display;
Anforderung Heizen/Kuehlen korrekt; Planner steuert BTS7960 (Richtung/Enable); Innen-
und Aussenluefter in den vorgesehenen Zustaenden; Sollwert erreicht/gehalten (C2);
Stop, Sicherheitsabschaltung, Aussenluefternachlauf; ungueltige Sensoren/fehlende
Voraussetzungen fail-closed. Abbruch ohne Wiederherstellungspflicht (DEV, wie
Ownerentscheid D3 aus #190): Test beenden, Leistungspfad stromlos, dokumentieren.
Ergebnisse werden den Issues #32/#33/#34/#35 zugeordnet; verbleibende
Fehlerinjektionen → #36, Dauerlauf → #37. Kein unbeaufsichtigter Betrieb.

## 8. Teil D – Vorlaeufige Abschlussbewertung #32 / #33 (nicht entschieden)

Nur Vorschlag; den Abschluss entscheidet der Owner.

**#32 (Luefter/Summer/Ausgaenge):** `HW-32-04`/`-05` werden uebernommen, nicht
wiederholt. Produktive Luefteransteuerung und Aussenluefternachlauf werden in C1
nachgewiesen (keine eigene Kampagne). **Noch offen und entscheidend:** Summer
(`HW-32-03`: Typ aktiv/passiv, nichtblockierende Muster, Ownerentscheid O3=B) – es
existiert kein Produktpfad, der den Summer treibt, C1 beruehrt ihn nicht. #32 ist
nur abschliessbar, wenn der Owner den Summerumfang ausdruecklich auf einen anderen
Scope verschiebt oder streicht, oder er separat umgesetzt wird. Zusaetzlich
`HW-32-02`/Reset-Verhalten (Owner hat das Restrisiko fuer R1 akzeptiert).

**#33 (BTS7960/Peltier):** unbelasteter Richtungstest `HW-33-06=PASS` bleibt. Nicht
durch C0–C2 abgedeckt und **Safety-relevant**: `HW-33-01` (Variante, 3,3-V-
Logikvertraeglichkeit, Enable-Funktion, reale Polaritaet – Polaritaet wird in C2
indirekt sichtbar), `HW-33-02` (Boot/Reset/Brownout mit Richtungen und Enable aus),
`HW-33-03` (Sicherung, Temperatursicherung, Kuehlkoerper, Pflichtsensoren) – das sind
Voraussetzungen *vor* Peltierleistung. `HW-33-04` (Servicepulse, Totzeit,
Sensorentfernung) und `HW-33-05` (Gesamtkette) werden von C1/C2 weitgehend
abgedeckt und koennen dort nachgewiesen statt separat getestet werden. **Vorlaeufig:
#33 nicht abschliessbar**, bis `HW-33-01/-02/-03` belegt oder vom Owner
ausdruecklich eingeordnet sind.

## 9. Umsetzungsschnitte (nach Planfreigabe, jeweils digital getestet)

- **S1 – Anzeige (A):** Rohanzeige-Eingabe, `TemperatureValueBasis`, Renderer-/
  Textschluessel, Web-`null`, `app_main`-Aufruf. Tests §5.2, Renderer/Projection,
  Web-Codec, `peltier_composition_guard`/`check_architecture_boundaries`.
- **S2 – Regelbereitschaft (G4):** Start-Admission gemaess §6.1 + Tests (auch
  Regression `test_r1_control_chain_integration`, Start-Matrix).
- **S3 – Producer je Rolle konfigurierbar:** kleine Erweiterung (Kuehlkoerper-
  Bereich getrennt); Tests `test_r1_control_chain_integration`.
- **S4 – Planner-Writer (G3):** `applyActuatorPlannerParameters` + Clear +
  Trial-UART-Kommandos; Tests Service-Commit/Persistenz/Revision.
- **S5 – Trial-Profil/Build:** Profil-Header, Buildskript mit Release-Verbot, Boot-Warnung,
  Release-Guard-Test, beide ESP-IDF-Profile. Nur Werte aus Anhang A, einzeln
  freigegeben.
- **S6 (nur bei D5=S-B):** minimaler Hard-Cutoff mit eigener Plan-Ergaenzung.
- **S7 – Doku:** `docs/ROADMAP.md`, `docs/ACCEPTANCE_TESTS.md` (Nachweiszeilen fuer
  Stufen C0–C2, nicht als `PASS` vorab), `docs/SENSOR_TUNING_COMMISSIONING.md`
  (Rohanzeige/Trial), Evidence-Datei fuer den Versuch.

Nach S1–S5: Builder-Self-Check, Independent Review, danach (separat) Owner-Gates
G-C0/G-C1 (und G-C2 nach D5). Voller Pre-Ready-Lauf erst auf Owneranweisung.

## 10. Dokumentationswirkung

Dieser PR aktualisiert `docs/ROADMAP.md` (u. a. #190 `CLOSED`, Verweis auf diesen
Plan unter #32/#33/#35). Keine Statusduplikate in Issue-Bodies ausser knappen
Verweisen nach Planfreigabe. Nicht ausgefuehrte Nachweise bleiben `NOT_RUN`.

## 11. Risiken und offene Entscheidungen

- **D1:** Freigabe der exakten Plan-SHA.
- **D2:** Quelle der Planner-Parameter: Writer in Service-Konfiguration (empfohlen,
  vertragskonform, mit Clear) oder verwerfen (nicht empfohlen: Bypass).
- **D3:** Anzeigeumfang: Rohwerte nur lokal (empfohlen) oder auch Web; `Failed`-Wert
  weiter mit Label anzeigen (empfohlen) oder ausblenden; `kRawDisplayMaxAgeMs`.
- **D4:** nur Programmstart (empfohlen) oder zusaetzlich manueller Halt-Lauf mit
  Trial-Lauflimits.
- **D5:** Sicherheitsbereich vor Peltierleistung: S-A (unabhaengige
  Hardware-Abschaltung, beaufsichtigt, empfohlen fuer den Versuch) oder S-B
  (Firmware-Hard-Cutoff im PR).
- **D6:** Trial-Werte (Anhang A) einzeln freigeben; Geltung = Trial-Image/Versuch.
- **D7:** Summerumfang #32 (verschieben, streichen oder umsetzen).
- Risiken: Trial-Werte koennen thermisch ungeeignet sein (daher kurz, beaufsichtigt,
  strombegrenzt); persistierte Planner-Parameter koennen versehentlich bleiben
  (Clear + Hash-Nachweis); Kuehlkoerper-Uebertemperatur ohne G6 (D5); `app_main`
  nahe der Komplexitaetsgrenze (Helferfunktionen).

## Anhang A – Trial-Wertekandidaten (`PROPOSAL_NOT_APPROVED`, keine Freigabe)

Keine Zahl ist freigegeben. Ableitungsregeln; Zahlen setzt der Owner:

| Parameter | Regel/Quelle | Vorschlag |
|---|---|---|
| Planner: Schaltfenster, Mindest-Ein | `docs/ACTUATOR_TIMING.md` Beispiel | 30 s / 3 s (Beispiel der Spezifikation) |
| Planner: Mindest-Aus, Polaritaetstotzeit, Nachlauf aussen/innen | Owner/Waermeabfuhr | Ownerwert erforderlich |
| PI Luft Heizen/Kuehlen | keine Messbasis | nur P, kleine Verstaerkung, `Ki=0`, `maximumQuote` konservativ klein |
| Luftbegrenzung (block/reduce) | Programmziel + Bauteilgrenzen | eng um den Versuchsbereich, Ownerwert |
| Sensorqualitaet je Rolle | `sensor_limits.hpp` (firmwarefeste Obergrenzen) | Luft/Produkt: aus M2 (Zyklus ~2 s) ableiten; Kuehlkoerper: weiterer Bereich |
| `kRawDisplayMaxAgeMs` | Zyklus 1800–2160 ms | 5000 ms |
| Versuchsprogramm | vorhandener Editor | kurze Stufe, Ziel nahe Ist, Abbruch von Hand |

Jeder uebernommene Wert wird im Trial-Header mit Quelle, Gueltigkeit und
Ablösebedingung dokumentiert.
