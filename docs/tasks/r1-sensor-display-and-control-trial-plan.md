# R1 – Temperaturanzeige, Firmware-Temperaturschutz und erster integrierter Regelversuch (Plan, Revision 2)

Status: **Revision 2 (Ownerentscheidungen D2/D3/D4/D5/D7 und Review-Korrekturen eingearbeitet) zur Ownerfreigabe
der exakten Plan-SHA (D1 offen).** Rev. 1 (`710f918`) ist **nicht** freigegeben und
ersetzt; diese Fassung ist eigenstaendig ausfuehrbar. Bis zur Freigabe: keine
Implementierung, keine Builds, kein Flash, keine Hardwareaktion.
`IMPLEMENTATION=NOT_STARTED`, `FLASH=NO`, `ACTUATOR_RELEASE=NO`,
`REAL_PELTIER_TEST=NOT_RUN`, `C2_PHYSICAL_PRECONDITIONS=NOT_VERIFIED`.

Basis: `origin/main` `124418acfbe61b364c416261d476e3b80b5f4840` (Merge PR #220;
Issue #190 `CLOSED`). Betroffene bestehende Issues: #32, #33, #34, #35; spaetere
Abnahme #36, Dauerlauf #37. Keine neuen Issues, kein zweiter PR.

## 0. Ownerentscheide und Review-Korrekturen (verbindlich eingearbeitet)

Quellen: Ownerentscheidungen 2026-11-20 (PR-Kommentar `7040200219`), Independent Plan
Review `5993022626`.

| Entscheid | Wirkung in diesem Plan |
|---|---|
| **D7 – Summer entfaellt aus R1** | Kein Summer-Treiber, keine Muster, keine OUT3-Pulse, keine akustische Abnahme; OUT3 bleibt softwareseitig AUS/unbenutzt. Sichtbare Meldungen und automatische Sicherheitsabschaltung bleiben Pflicht. Normative Dokumente gezielt synchronisiert (siehe §10). |
| **D5 – Temperaturschutz** | Beim ersten beaufsichtigten Trial: separates, unabhaengig ablesbares Messgeraet **und** sofort stromlos schaltbarer Leistungspfad durch den Owner = **manuelle Ueberwachung**, kein Thermoschutzsystem. Der dritte DS18B20 (Kuehlkoerper) uebernimmt die automatische Ueberwachung **im bestehenden Produkt-Safety-Pfad** (rohwertbasiert, fail-closed, verriegelnd) – eigener Schnitt S6. Die unabhaengige Absicherung aus `HARDWARE.md` bleibt eigene Pflicht. Unklarer/fehlender vorgeschriebener Schutz ⇒ **C2 `BLOCKED`**, kein stiller Waiver. |
| **D2** | Service-Writer fuer Planner-Parameter **mit validiertem Clear**. |
| **D3** | Frische Rohwerte ≤ 5 s nur lokal; qualifizierter Web-Vertrag unveraendert. |
| **D4** | Erstes Trial ueber ein Benutzerprogramm (kein manueller Halt-Lauf). |
| **D6** | Numerische Trialparameter und C2-Thermik-/Sicherheitsgrenzen werden **nicht erfunden und nicht jetzt freigegeben**, sondern an getrennten Gates anhand tatsaechlicher Parameter/Aufbau (§7). |
| P-1 (Review) | `Ki = 0` verworfen: `validDirectionParameters` fordert `Ki > 0` fuer alle vier PI-Saetze. |
| P-2 (Review) | `S-A ODER S-B` entfernt: C2 braucht **beide** Ebenen (Firmware-Cutoff **und** unabhaengige Schutzebene), nicht gegeneinander aufgerechnet. |
| P-3 (Review) | Trial-Plannerwerte: Geltung, semantischer Cleanup, Provenance-Kanon, G4 auch bei Confirm/Revalidate, C1 ohne Summer. |

## 1. Ziel und Nicht-Ziele

Ziel: (1) echte DS18B20-Temperaturen auf dem lokalen Display ohne
`SensorQuality::Valid` zu behaupten, (2) echte Messwerte kontrolliert in
Qualitaets-, Regelungs- und Sicherheitskette, (3) Schliessen der tatsaechlich
fehlenden Parameter-, Verbindungs-, Start- und Safety-Bausteine, (4) ein
beaufsichtigter integrierter Regelversuch ueber den Produktpfad, dessen
Ergebnisse #32/#33/#34/#35 zugeordnet werden.

Nicht-Ziele: keine Wiederholung von Luefterpuls-, unbelastetem H-Bruecken-,
Sensor-Grund- oder Stabilitaetstests; kein paralleler Testcontroller, keine zweite
Aktorsteuerung, keine zweite DS18B20-Pipeline, kein Sensorzugriff im Renderer; keine
Peltierfreigabe/`ACTUATOR_RELEASE`; Release-Image bleibt fail-closed;
keine finalen #35-Sicherheitsgrenzen erfinden; kein Summer; Offsets/Thermik #34,
finale Parameter #35, Fehlerinjektionen #36, Langzeittest #37.

## 2. Verifizierte Ausgangslage (Code `main` `124418a`)

- `app_main.cpp`: Sampler laeuft (Budget 4096/2/1); Quellen speisen
  `ControlSensorEvidenceProducer`; Qualitaetskonfiguration
  `kSensorQualityConfigUnconfiguredUntilIssue35 = std::nullopt` ⇒ `sample()` liefert
  keine Evidenz (`control_sensor_evidence.cpp`).
- `FermentationApplication::refreshUiSnapshot()` baut die Temperaturen
  ausschliesslich aus `resolveRuntimeEvidence().plausibility` (gespeist von
  `publishOwningRuntimeEvidence`); ohne Evidenz leere Snapshots ⇒ Display leer.
  `valueOf()` faellt auf `rawCelsius` zurueck, auch bei `Stale`/`Failed`.
- Renderer (`main/fermentation_ui_renderer.cpp`) zeigt Wert, Qualitaetstext und auf
  der Diagnoseseite Roh-/korrigiert-/gefiltert-Wert und Alter; Web-Codec
  (`web_json_codec.cpp`) emittiert `valueCelsius` (null wenn fehlend) und
  `quality`-Name.
- `ControlChainParameters` = Unconfigured-Konstante (`0`) ⇒ Regler
  `Unavailable/NoCommissioning`. `validDirectionParameters` fordert
  `Kp>0`, **`Ki>0`**, Neutralband>0, `0<maxQuote≤1` fuer alle **vier** Saetze,
  dazu `maximumIntegrationStepMillis>0` und geordnete Luftlimits
  (`temperature_control.cpp:14-43`).
- `ActuatorPlannerParameters` ausschliesslich aus persistiertem
  `ServiceConfiguration.actuatorPlannerParameters` (Codec V2), Fresh-Start-Snapshot
  via `FreshStartSnapshotProvenance::fromRuntimeLease`, `planner_->beginRun`;
  **kein Produktions-Writer**. Provenance bleibt actor-frei
  (`run_persistence_coordinator.cpp`).
- `kManualRunLimitsOwnerAvailable = false` sperrt nur manuelle Laeufe/Kuehlplaene,
  nicht `prepareStartProgram`. `prepareStartProgram` und
  `revalidatePreparedRequest` (Confirm) pruefen **keine** Regelbereitschaft.
- `ActuationInterlock` (#24): Evidenzfelder ohne Temperaturschwellen; Faults
  (`FaultCode`, `kFaultCount = 8`): Konfiguration, Persistenz, Sensor, Watchdog.
  Er haelt keinen eigenen Latch. Im Code existiert **kein** Eingriffs-/
  Notgrenzen-Mechanismus, kein Trend- und kein Kuehlkoerper-Uebertemperaturschutz;
  `CommandKind` enthaelt keinen Fault-Eintritt; `FaultCode` wird nur ueber
  `requireService(...)` (Lifecycle `ServiceRequired`, Wiederanlauf per Neustart)
  sichtbar gemacht. Ein allgemeiner Fault-Reset-/Service-PIN-Pfad ist fuer
  Temperaturfehler **nicht** vorhanden (nur `applyExternalWatchdogFaultReset`).
- Positiver Gesamtpfad ist digital bewiesen (`SIM-218-01`) mit
  `testChainParameters()`, `testPlannerParameters()`, `testQualityConfig()`;
  das Produkt scheitert an Quellen von Werten/Writer/Admission/Safety, nicht an der
  Kettenlogik.
- Aussenluefter-Nachlauf: Planner-Semantik vorhanden (`SIM-218-05/-08`); Summer: nur
  Ausgangs-Sink (`ready_inactive`), kein Treiber (jetzt D7: entfaellt).

## 3. Spur der Aktorfreigabe-Evidenz (alle Felder aus aktuellem Zustand, keines hart `false`)

| Feld (`ActuationEvidence`) | Produzent (`tickControlChain`) | Produkt heute |
|---|---|---|
| `bootValidationComplete` | `lifecycleState_ == Ready` | erfuellbar |
| `configurationValidated`, `-ServiceMode` | Runtime-Lease + Epoche | erfuellbar |
| `persistenceValidated`/`-LoadStatus` | Coordinator + `applicationReadiness()` | erfuellbar |
| `explicitActivationRequested`, `activationKind`, `processActivationApplied` | `freshStartActivation_` (nur expliziter Start) | erfuellbar |
| `plannerEvidenceValidated` | `classifyActuatorPlannerParameters(planner.parameters())==Valid` | **nie** (G3) |
| `sensorEvidenceValidated` | `freshPublication` | **nie** (G1) |
| `peltierSensor` (Kuehlkoerper) | `owningRuntimeEvidence_.cooling` | **nie `Valid`** (G1) |
| `sensorSelectionRuntime`, `actuatorPlanner` | Run-Zustand/Planner | erfuellbar |

Start-Admission (`makePreparedRequest`/`revalidatePreparedRequest`) verlangt
`safetyAllowsStart && airSensorValid && coolingSensorValid`; ohne Evidenz ist jeder
Start `Unavailable`. Begriffe: **`Unavailable`** = manuelle Laeufe (O5) oder fehlende
Sensorevidenz; **`Unconfigured`** = PI/Planner-Parameter alle `0` (`NoCommissioning`);
**`NotAllowed`** = Interlock ≠ `Allowed`.

## 4. Lueckenliste (V vorhanden/nur konfig, X fehlende Verbindung/Quelle, S echte Safety-Voraussetzung)

| Nr. | Luecke | V/X/S | Issue / Schnitt |
|---|---|---|---|
| G0 | Keine Messwertanzeige ohne Producer-Evidenz | X | S1 |
| G1 | `SensorQualityConfig` ohne Werte; ein Config fuer alle Rollen (Kuehlkoerper-Bereich abweichend) | V + Werte | #35 / S3 (Rolle), Werte §7 |
| G2 | PI-/Policy-/Luftbegrenzungswerte | V + Werte | #35 / §7 |
| G3 | Planner-Parameter ohne Writer | X + Werte | S4 |
| G4 | Start ohne Regelbereitschaft (stiller Lauf ohne Regelung sobald Sensoren `Valid`; gilt auch beim Confirm) – Klasse „Ready ohne Kette“ aus #219 | X | S2 |
| G5 | Manuelle Lauflimits (O5) | Owner | D4: nicht noetig fuer Programmstart |
| G6 | **Firmware-Temperaturschutz** (Kuehlkoerper/Luft Grenzen, Trend, fehlende/alte/fehlerhafte Werte → Peltier AUS, Latch/Rearm, Nachlauf) | **S – nicht erfuellt** | S6 (Pflicht vor C2) |
| G7 | Einmalige Temperatursicherung/Ueberstromsicherung am Bench-Aufbau | S (Hardware) | G-C2 Vorbedingung; #35 |
| G8 | Summer | entfaellt (D7) | – |
| G9 | Kein Luefter-Tacho | S | #35/#36 |
| G10 | `thermalCompatibility` bleibt `Unavailable` (kein Produzent); Wirkung auf Produkt-Sensormodus pruefen | V/X | S3 |

## 5. Teil A – Echte Temperaturen auf dem Display (S1)

Entwurf (kleinster Pfad, keine zweite Pipeline, keine Veraenderung der Evidenz):

- **Quelle:** dieselben drei `ITemperatureSource`-Referenzen des vorhandenen
  `ControlSensorEvidenceProducer` (`read()` je Kanal). Die in `app_main` bereits
  gelesenen Rohproben werden **einmal je Schleife** als schmaler, rein anzeigender
  `RawTemperatureSample`-Satz (je Rolle: Status, Celsius, Messzeitstempel, Identitaet)
  an die Application gegeben: `publishRawTemperatureSamples(...)`, gespeichert unter
  dem vorhandenen Application-Lock in einem eigenen Member.
- **Isolation:** `owningRuntimeEvidence_`/`publishOwningRuntimeEvidence`,
  `resolveRuntimeEvidence`, `validSensor`, Start-Admission, Interlock, Planner und
  Persistenz bleiben **unveraendert** und lesen die Anzeige-Eingabe nicht. (Der
  Firmware-Temperaturschutz S6 liest dieselben Rohproben ueber eine **eigene**,
  getrennte Leseseite; die Anzeige beeinflusst ihn nie.)
- **Frische zur Anzeigezeit:** `refreshUiSnapshot()` bewertet mit **einer**
  monotonen Zeitablesung (`timeSource_`) das Alter = jetzt − Messzeitstempel;
  angezeigt wird nur bei `status==Ok` **und** Alter ≤ `kRawDisplayMaxAgeMs`
  (**5000 ms**, Ownerentscheid D3). Ohne neue Proben verschwindet der Wert dadurch
  von selbst (digital mit virtueller Zeit testbar).
- **Vorrang:** liefert die qualifizierte Evidenz fuer eine Rolle einen Wert, gilt die
  bestehende Projektion unveraendert; Rohbasis nur bei leerer qualifizierter Rolle.
- **Projektion:** `TemperatureView` erhaelt `TemperatureValueBasis
  {Qualified, RawUnqualified}` (Standard `Qualified`); bei Rohbasis bleibt
  `SensorQualitySnapshot.quality = Stale` (nie `Valid`), `rawCelsius`,
  `lastAcceptedSampleAgeMs`, `identity` werden gesetzt. **Kein** neuer
  `SensorQuality`-Wert (Enum haengt in Persistenz-/Web-Codec und Renderer).
- **Renderer/Text:** Rohbasis zeigt den neuen Textschluessel `quality-unqualified`
  (Warnton) statt `quality-stale`; eine gemeinsame Hilfsfunktion in
  `fermentation_ui_text` ersetzt die zwei Direktaufrufe in Status- und
  Diagnoseseite. Stale/Failed-Werte *qualifizierter* Evidenz behalten ihre
  bestehende, spezifizierte Darstellung (Wert nur zusammen mit sichtbarem
  Warn-Label, Diagnoseseite mit Alter); ein alter Wert erscheint nie ohne
  Warnung/Alter als „live“.
- **Web:** unveraendert (Ownerentscheid D3): bei Rohbasis liefert `web_json_codec`
  `valueCelsius = null`, `quality = "stale"`; Test sichert die Wire-Gleichheit zum
  heutigen Stand.
- **`app_main`:** nur die Hilfsfunktion `publishControlSensorEvidence` erweitern
  (Komplexitaetsgrenze 25 beachten, Pre-Ready-esp-clang).

Tests (digital): frischer Rohwert → Anzeige `RawUnqualified`, Qualitaet nie
`Valid`; `MissingSample`/`CrcFault`/`BusFault`/zu alt → kein Wert; Ablauf ohne neue
Proben (virtuelle Zeit 5 s); Rolle folgt der gespeicherten ROM-Bindung (Quelle
liefert die Rolle); Vorrang qualifizierter Evidenz; **Isolation:**
`resolveRuntimeEvidence`/`validSensor`/Start-Admission/Interlock/Persistenz bleiben
bitgleich ohne/mit Rohanzeige; Renderer-Text (Status/Diagnose); Web-JSON unveraendert.

## 6. Teil B – Regelbereitschaft, Parameterquellen, Planner-Writer

### 6.1 Start-Admission „Regelbereitschaft“ (G4, S2)

`prepareStartProgram` verweigert mit dem bestehenden `Unavailable`, wenn nicht
gilt: Kette vorhanden **und** `classifyTemperatureControlParameters(PI)==Valid`
**und** `classifyActuatorPlannerParameters(Lease-Parameter)==Valid` **und** der
Firmware-Temperaturschutz (S6) konfiguriert und nicht verriegelt. Die Pruefung liegt
**vor** jeder identitaets-/persistenzwirksamen Nebenwirkung (vor
`runIdentity_->allocateForApplication()`), wird in
`revalidatePreparedRequest` (Confirm/Revalidate) wiederholt und gilt nach
Rebuild/Recovery. Das schliesst den Fall „Sensoren zunaechst qualifiziert, aber
PI-/Planner-Snapshot fehlt“. Keine Parallelsafety, nur eine zusaetzliche
Bedingung in der bestehenden Admission.

### 6.2 Parameterquellen und Trial-Profil (S3–S5)

- **Release/Produkt:** unveraendert die benannten Unconfigured-Konstanten;
  `peltier_composition_guard`/`SIM-218-00` bleiben gruen; Release enthaelt keine
  Trial-Symbole (`strings`-/Guard-Test).
- **Trial-Image:** eigener Bring-up-Build mit `APP_R1_CONTROL_TRIAL`
  (`esp32_bringup`, wechselseitig ausgeschlossen mit den #30/#32/#33/#90-Harnesses,
  nie Release; eigenes Buildskript mit Release-Verbot; Boot-Warnung
  `TRIAL_NOT_COMMISSIONED`). Compile-time: `SensorQualityConfig` je Rolle
  (Kuehlkoerper getrennt, kleine Producer-Erweiterung S3), `ControlChainParameters`.
  **Ein** geprueftes Image/ein Flash fuer C0→C1→C2 mit phasenweisen Gates; ein neuer
  Build nur, wenn sich compile-time Werte (PI/Qualitaet) fuer C2 aendern muessen
  (dann neues Flash-Gate).
- **Planner-Writer (D2, G3):** `applyActuatorPlannerParameters(...)` nach dem Muster
  `applySensorCommissioning`: validiert per `classifyActuatorPlannerParameters`
  (nur `Valid` oder explizites `nullopt` zum Clear), Revisions-/Lock-Vertrag der
  Service-Commit-Mechanik, nur ueber Trial-UART-Kommandos
  (`planner set|show|clear`), nie aus UI/Web. Provenance bleibt der kanonische Weg:
  der Fresh-Start-Snapshot liest weiter ausschliesslich
  `ServiceConfiguration.actuatorPlannerParameters`; keine Umgehung.
- **Geltung/Cleanup (P-3):** Persistierte Trial-Plannerwerte allein erzeugen **nie**
  eine `ControlRequest`, weil Release-/Nicht-Trial-Images PI-Parameter
  `Unconfigured` haben (G4 verweigert den Start; Interlock/Planner bleiben aus). Sie
  sind dort inert, werden aber nie still akzeptiert: Beim Boot meldet **jedes** Image
  mit Writer-Unterstuetzung einen Status `planner_params=present|absent` und im
  Trial-Image zusaetzlich `matches_trial_profile=0|1`. Cleanup (Ende, Abbruch,
  Reset, vor Reflash eines Nicht-Trial-Images): `planner clear` und anschliessende
  **semantische Verifikation** ueber `planner show` (`absent`) plus erhoehte
  Konfigurationsrevision; **keine** Behauptung identischer Partitionshashes
  (Journal-/Revisionsfortschritt). Der Werksreset entfernt den Slot
  (bestehender Resetpfad; Nachweis per Test in S4). Bei ungeplantem Abbruch ohne Cleanup: der naechste
  Trial-Boot meldet `present`; Cleanup bleibt Pflicht vor Abschluss.
- **C1-/C2-Werte:** siehe §7 und Anhang A.

### 6.3 Programmstart statt manueller Laeufe (D4, G5)

Das erste Trial laeuft ueber ein kurzes Benutzerprogramm aus dem vorhandenen Editor.
`kManualRunLimitsOwnerAvailable` und O5 bleiben unberuehrt; kein Manual-Hold-Scope.

## 7. Firmware-Temperaturschutz (G6, S6) – Voraussetzung fuer jedes C2

Der bestehende Produkt-Safety-Pfad ist `ActuationInterlock` (#24); der Schutz wird
**dort** eingehaengt, nicht parallel. Er ersetzt weder die unabhaengige
Hardware-Absicherung noch ist er hardwareunabhaengig (er faellt bei MCU-Haenger,
Treiberdefekt oder ausbleibender Sensorabfrage aus).

- **Eingaenge:** die Rohproben (Status, Celsius, Zeitstempel, Identitaet) von
  **Kuehlkoerper und Schrankluft** aus denselben Quellen, **unabhaengig** von
  `SensorQualityConfig`/Median/Tiefpass (rohwertbasiert, daher auch ohne G1 wirksam).
  Eigene Leseseite, getrennt von der Anzeige-Eingabe (§5).
- **`ThermalSafetySupervisor`** (`fermentation_app`, abstrakt, keine Hardware):
  Zustaende `Unconfigured | Ok | Blocked | Tripped(Latch, Grund)`.
  - **Grenzen:** obere/untere Eingriffsgrenze und weiter entfernte harte Notgrenze je
    Sensor (Kuehlkoerper, Luft) sowie **Trend** (Anstieg des Kuehlkoerpers je Zeit
    ueber Fenster); Grenzwerte nur innerhalb **firmwarefester** Aussengrenzen
    parametrierbar, geordnet und endlich.
  - **Ungueltige Daten:** fehlend/alt/CRC-/Busfehler/nicht-finit/ausserhalb absolutem
    Messbereich ⇒ sofort `Blocked` (Peltier AUS) und nach firmwarefester Dauer
    `Tripped` (Latch) – fail-closed, nicht durch Trial-PI-/Luftlimits umgehbar.
  - **Eingriffsgrenze/Notgrenze:** `Tripped` verriegelt, beide Richtungen gesperrt, kein
    automatischer Gegenrichtungsversuch im Trial (Versuchszahl `0`).
  - **Nicht konfiguriert:** `Unconfigured` zaehlt wie `Blocked` (Peltier AUS); im
    Release dauerhaft so.
- **Verdrahtung:** neues Feld `thermalSafety` in `ActuationEvidence`, neuer stabiler
  `FaultCode::TemperatureSafetyLimit`, `kFaultCount` 8→9, in `kFaultPriority`; jeder
  Zustand ≠ `Ok` ⇒ Interlock ≠ `Allowed`. Der Latch liegt im Supervisor (Interlock
  bleibt zustandslos). Bei `Tripped` ruft die Application `requireService(
  TemperatureSafetyLimit)` (sichtbarer Sicherheitsfehler im vorhandenen
  Fault-Anzeigepfad; Lifecycle `ServiceRequired`).
- **Lueftersemantik:** Peltier AUS; Aussenluefter-Nachlauf und Innenluefter gemaess
  vorhandener Planner-Semantik (nicht abgeschaltet); Nachweis per Test
  (analog `SIM-218-05/-08`).
- **Rearm (eindeutig, Trial):** Latch ist fluechtig (`SPECIFICATION_REVIEW`: Neustart
  erzwingt vollstaendige Neubewertung); Rearm = Neustart **und** erneutes Setzen der
  RAM-Grenzwerte ueber das geschuetzte Trial-UART-Kommando (`thermal limits set`);
  Parameter stehen **nie** persistent. Ein produktiver Service-PIN-Reset mit
  Stabilitaetspruefung gemaess `SAFETY_COMPONENT_FAULTS.md` existiert nicht und ist
  **kein** Teil dieses PR (→ #35/#36); das wird nicht als erfuellt behauptet.
- **Grenzwerte/Totzeiten/Trend:** keine Zahl in diesem Plan. Firmwarefeste
  Aussengrenzen und Trial-Grenzen werden mit dem tatsaechlichen Aufbau (Peltier-
  Datenblatt, Kuehlkoerper, Bench) am Gate G-C2 vom Owner genannt.

Tests (digital): jeder Grund → `Blocked`/`Tripped`, Latch haelt bis Neustart,
Peltier nie bei `Unconfigured`/`Blocked`/`Tripped`, Aussenluefter-Nachlauf im Trip,
Start-Admission verweigert bei `Unconfigured`/`Tripped`, Fault sichtbar, keine
Beeinflussung durch Anzeige-Eingabe, Release-Default fail-closed.

## 8. Teil C – Gestufter integrierter Regelversuch (Hardware nur nach Owner-Gate)

Alle Stufen laufen ueber den Produktpfad (Programmstart → Regler → Planner →
Interlock → Treiber), nicht ueber UART-Aktorbefehle; kein Summer. **Ein** geprueftes
Image, ein App-only-Flash (separates Gate **G-T**, Hash-Bindung; das verbrauchte
#190-G2 gilt nicht); phasenweise **physische** GOs statt Pflicht-Refash. Ein blosser
Boot mit Trialwerten aktiviert nichts: Regelung braucht expliziten Programmstart,
gueltige PI-/Planner-/Schutzparameter und G4.

| Stufe | Inhalt | Gate | Peltier-Leistung | Nachweise |
|---|---|---|---|---|
| C0 | Boot, nur Anzeige (§5) | G-T + G-C0 | **stromlos** | Messwerte auf Display, Rollen, Frische |
| C1 | Programmstart mit bewusst gesetzten **C1-Parametern**; BTS7960-Logik angeschlossen, **Leistungspfad physisch nachweislich stromlos**; Luefter an OUT1/OUT2 | G-P1 (Parameter) + G-C1 | **stromlos** | echte `ControlRequest`, Planner→Driver-Signale, Innen-/Aussenluefterzustaende, **Aussenluefter-Nachlauf bei Stop**, Fail-closed bei Sensorverlust/Trip (Schutz S6 digital+Bench), Admission G4 → #32 |
| C2 | Peltierleistung, beaufsichtigt, unabhaengige Schutzebene (a) **und** Firmware-Cutoff (b) | G-P2 (C2-Parameter + Thermikgrenzen) + G-C2 + physische Vorbedingungen | **bestromt** | Heiz-/Kuehlanforderung, Sollwert erreichen/halten, Richtungswechsel/Totzeit, Sicherheitsabschaltung → #33/#34/#35 |

**C2-Vorbedingungen (alle am Geraet vom Owner zu bestaetigen, sonst `C2=BLOCKED`):**
(a) **Unabhaengige Schutzebene**, nachpruefbar: 7,5-A-Ueberstromsicherung im
Peltier-Leistungspfad montiert; strombegrenzte/abgesicherte Versorgung; einmalige
Temperatursicherung montiert und auf Durchgang geprueft, Montageort dokumentiert
(`DIAGNOSTICS_AND_MAINTENANCE.md`, `HARDWARE.md`); Kuehlkoerper und Luefter
montiert, thermisch begrenzbar; Leistungspfad vom Owner sofort stromlos schaltbar.
(b) **Firmware-Cutoff S6** digital geprueft und mit freigegebenen Grenzen aktiv.
(c) **Manuelle Ueberwachung:** unabhaengiges, ablesbares Messgeraet; ueberwachte
Messstelle und physische Trennmoeglichkeit **konkret festgehalten**. Manuelle
Ueberwachung und Strombegrenzung gelten **nicht** als montierte Temperatursicherung
und nicht als Safety-PASS. (d) Aussenluefter zuvor funktionsgeprueft (`HW-32-05`),
BTS7960-Adapterinterlock auf Command-/GPIO-Ebene bestanden. Fehlt etwas oder ist es
unklar ⇒ `C2=BLOCKED`, Owner-Entscheid zum pruefbaren Aufbau anfordern.

Beobachtungskriterien je Stufe: echte Werte erreichen Software/Display; Heiz-/
Kuehlanforderung korrekt; Planner steuert BTS7960; Innen-/Aussenluefter in den
vorgesehenen Zustaenden; Sollwert erreicht/gehalten (C2); Stop, Sicherheits-
abschaltung, Aussenluefter-Nachlauf; ungueltige Sensoren/fehlende Voraussetzungen
fail-closed. Abbruch ohne Wiederherstellungspflicht (DEV): Test beenden, Leistungspfad
stromlos, dokumentieren, Planner-Cleanup (§6.2). Kein unbeaufsichtigter Betrieb.
Ergebnisse → #32/#33/#34/#35; vollstaendige Fehlerinjektionen → #36, Dauerlauf → #37.

## 9. Teil D – Vorlaeufige Bewertung #32 / #33 (Vorschlag, Owner entscheidet)

**#32:** `HW-32-04/-05` uebernommen, nicht wiederholt. Summer: `NOT_REQUIRED_R1
(Owner-Scopeentscheid D7)`, nicht `PASS`. #32 ist nach **nachgewiesener produktiver
Luefterfunktion und Aussenluefter-Nachlauf (C1)** fachlich abschliessbar
(Kanal-/Verbraucherzuordnung der Luefter, Nachlauf aus `HW-32-03`); bis dahin offen.
**#33:** `HW-33-06=PASS` bleibt. Nicht durch C0–C2 abgedeckt und Safety-relevant:
`HW-33-01` (Variante, 3,3-V-Logik, Enable, reale Polaritaet – indirekt in C2),
`HW-33-02` (Boot/Reset/Brownout mit Richtungen/Enable aus), `HW-33-03`
(Sicherung/Temperatursicherung/Kuehlkoerper/Pflichtsensoren – C2-Vorbedingung).
`HW-33-04/-05` werden von C1/C2 weitgehend abgedeckt. #33 bleibt offen, bis das reale
sichere C2-End-to-End-Mindestpaket belegt oder die uebergreifende #36-Restmatrix
legitim zugeordnet ist; keine Umetikettierung elektrischer Fehler-/Boot-Reset-Belege.

## 10. Umsetzungsschnitte (nach Planfreigabe, jeweils digital getestet)

- **S1 Anzeige** (§5). **S2 Regelbereitschaft** (§6.1, inkl. Confirm). **S3 Producer
  je Rolle + `thermalCompatibility`-Pruefung.** **S4 Planner-Writer + Clear + Status.**
  **S5 Trial-Image/Profil/Buildskript/Release-Guard** (Werte nur gemaess §7 Gates).
  **S6 Firmware-Temperaturschutz** (§7, eigener Review-Schwerpunkt; kein Peltier-C2
  ohne S6). **S7 Dokumentation:** `ROADMAP`, `ACCEPTANCE_TESTS` (Nachweiszeilen
  C0–C2 als `NOT_RUN` bis Ausfuehrung), `SENSOR_TUNING_COMMISSIONING`, Evidence.
- **Spezifikationssynchronisierung D7 (in diesem PR bereits erledigt, nur Doku):**
  `SPECIFICATION_REVIEW` (R1-Scopeentscheid), `PROGRAMS`, `RUNTIME_BEHAVIOR`,
  `STATE_MACHINE`, `STANDARD_PROGRAMS`, `LOCAL_RUNTIME_UI`,
  `SAFETY_COMPONENT_FAULTS`, `RECOVERY_AND_INTERRUPTION`, `HARDWARE` (inkl.
  korrigierter Luefter-/Planner-Aussage seit #219), `OPEN_POINTS`,
  `ACCEPTANCE_TESTS` (`NOT_REQUIRED_R1`), `DECISIONS` (ADR-Nachtrag). `REQUIREMENTS`
  enthaelt keine betroffene Aussage. Historische Aussagen bleiben als Nachweis
  erhalten, mit R1-Vorbehalt markiert; Meldungen bleiben sichtbar, Sicherheits-
  abschaltung bleibt Pflicht. Der vorhandene lokale „Stummschalten“-Code bleibt
  unveraendert (kein Produktcodeumbau fuer D7).
- Nach S1–S6: Builder-Self-Check, Independent Review, dann (getrennt) Gates G-T,
  G-C0, G-P1, G-C1 sowie G-P2, G-C2. Voller Pre-Ready erst auf Owneranweisung.

## 11. Entscheidungen und Gates

Entschieden: D2, D3, D4, D5 (Prinzip), D7. **Offen:** **D1** Freigabe der exakten
Plan-SHA; **D6 getrennt:** G-P1 (C1-Parameter) und G-P2 (C2-Parameter, Thermik-/
Sicherheitsgrenzen anhand tatsaechlicher Parameter/Aufbau); **G-T** (App-only-Flash,
Image-Hash); **G-C0, G-C1, G-C2** (physische Freigaben). Alle `NOT_GRANTED`.
Risiken: Trial-Werte thermisch ungeeignet (beaufsichtigt, strombegrenzt, kurz);
persistierte Plannerwerte verbleiben (inert + semantischer Cleanup);
`app_main` nahe der Komplexitaetsgrenze; S6 ist neuer Safetycode (Review-/Testaufwand);
fehlender produktiver Fault-Reset bleibt ausdruecklich offen (#35/#36).

## Anhang A – Ableitung gueltiger Trial-Werte (keine Zahlen, keine Freigabe)

- **PI (je Richtung/Rolle):** alle vier Saetze muessen `classifyTemperatureControl
  Parameters==Valid` erfuellen: `Kp>0`, **`Ki>0`** (strikt positiv, kleiner Wert aus
  `Ki = Kp/Ti` mit `Ti` deutlich groesser als das Schaltfenster), Neutralband>0,
  `0<maxQuote≤1` (klein fuer C1/C2), `maximumIntegrationStepMillis>0`, geordnete
  Luftlimits. Nachweis vor Vorlage: digitaler Validierungstest plus Simulation, dass
  der Integralanteil innerhalb `maxQuote` bleibt. **Keine** „P-only“-Vorgabe.
- **Planner:** nur `Valid` nach `classifyActuatorPlannerParameters`; Schaltfenster/
  Mindest-Ein aus `docs/ACTUATOR_TIMING.md` (Beispiel 30 s/3 s) als Ausgangspunkt,
  Mindest-Aus/Totzeit/Nachlauf als Ownerwerte.
- **Sensorqualitaet je Rolle:** innerhalb `sensor_limits.hpp`-Obergrenzen; Kuehlkoerper
  mit weiterem Plausibilitaetsbereich als Luft.
- **Schutz S6:** Grenzen/Trend/Totzeiten ausschliesslich aus Aufbau und Owner (G-P2).
- Jeder uebernommene Wert wird im Trial-Header mit Quelle, Gueltigkeit (nur
  Trial-Image/Versuch), Schutzgrenzen und Abloesebedingung durch #34/#35 dokumentiert;
  die konkreten Zahlen erscheinen erstmals im S5-/S6-Commit nach den digitalen
  Validierungstests und werden am jeweiligen Gate vom Owner freigegeben.
