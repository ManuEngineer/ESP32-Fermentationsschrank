# Plan: Issue #21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik

## 1. Metadaten und Status

```text
Issue: #21 [E3.2] Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik
Epic: #5 (E3)
Branch: plan/issue-21-sensor-selection-fallback-return
Baseline main: ff2e66a8c340d61c8c4517f90fd3fba5a8fc3db2
Context HEAD: ff2e66a8c340d61c8c4517f90fd3fba5a8fc3db2
PLAN_ONLY: YES
IMPLEMENTATION_STARTED: NO
PLAN_STATUS: PLAN_DRAFT_REVIEW_REQUIRED
IMPLEMENTATION_STATUS: IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

Der Branch wurde nach einem Fast-forward von `main` auf die live verifizierte
Merge-Basis von PR #98 erstellt. Der Plan-Commit wird nach dem Commit im
Draft-PR exakt eingetragen. Produktionscode, produktive Tests, Toolchain,
Buildkonfiguration, Hardwarekonfiguration und Abhaengigkeiten werden in dieser
Planungsphase nicht geaendert.

## 2. Live-Issue- und Abhaengigkeitsabgleich

| Quelle | Live-Stand am 2026-08-06 | Bedeutung fuer diesen Plan |
|---|---|---|
| Issue #21 | OPEN, Body-Status `PLANNED_SPEC_PENDING`, keine Kommentare | eigener Plan-first-Draft-PR, keine Implementierungsfreigabe |
| Issue #14 | CLOSED | kanonische Prozesszustaende und Uebergangstopologie stehen zur Verfuegung |
| Issue #20 | CLOSED, Body-Status `READY` ist historisch | `SensorQualitySnapshot` und `SensorQualityPipeline` sind die bestehende Qualitaetsquelle |
| Epic #5 | OPEN | Issue bleibt Teil des E3-Sensor-/Regel-/Safety-Kerns |
| PR #98 | MERGED am 2026-08-06, Merge-Commit `ff2e66a...` | Roadmap-Status wurde auf Issue #21 als aktuelle Planungsarbeit umgestellt |

Issue #21 hat keine Kommentare, die zusaetzliche Anforderungen oder
Ownerentscheidungen enthalten. Die Live-Issue-Beschreibung ist die
Scopequelle. Issue #14 und #20 werden nicht veraendert.

## 3. Verbindliche Quellen und Lesematrix

Bei Widerspruechen gilt die in `docs/SPECIFICATION_REVIEW.md` festgelegte
Reihenfolge:

1. akzeptierte ADRs im Register `docs/DECISIONS.md`;
2. `docs/SPECIFICATION_REVIEW.md` fuer Release-Scope, Quellenrollen und TBDs;
3. der fachlich zustaendige spezialisierte Vertrag;
4. `REQUIREMENTS.md`, `ARCHITECTURE.md` und `HARDWARE.md`;
5. Beispielkonfigurationen;
6. historische Audits, Plaene und Revisionsdokumente.

Fuer Issue #21 wurden gelesen und gegen `main@ff2e66a...` abgeglichen:

- Root-`AGENTS.md`, `docs/AGENT_WORKFLOW.md`,
  `docs/CI_AND_QUALITY_GATES.md` und die lokalen Regeln von
  `lib/device_platform/`, `lib/device_platform_test_support/` und
  `lib/fermentation_app/`;
- `docs/DECISIONS.md`, insbesondere ADR-004 (Produkt-/Luftfuehrung),
  ADR-012 (Software-first), ADR-013 (Modulgrenzen) und ADR-014
  (deterministische Entscheidungen);
- `docs/TEMPERATURE_CONTROL.md`,
  `docs/RECOVERY_AND_INTERRUPTION.md`,
  `docs/SAFETY_COMPONENT_FAULTS.md` und
  `docs/SENSOR_TUNING_COMMISSIONING.md`;
- `docs/STATE_MACHINE.md`, `docs/RUN_COMMANDS.md`,
  `docs/RUN_PERSISTENCE.md`, `docs/LOCAL_RUNTIME_UI.md`,
  `docs/DIAGNOSTICS_AND_MAINTENANCE.md`,
  `docs/ACCEPTANCE_TESTS.md`, `docs/OPEN_POINTS.md`,
  `docs/IMPLEMENTATION_PLAN.md` und `docs/IMPLEMENTATION_ISSUES.md`;
- historische Planquelle `docs/tasks/issue-20-sensor-quality-filtering-plan.md`
  nur zur Scopeabgrenzung: #20 liefert Evidenz je einzelner Sensorinstanz,
  trifft aber keine rollenuebergreifende Auswahl, Schuldzuweisung oder
  Rueckkehrentscheidung;
- Code und Tests in `lib/device_platform/src/sensor_quality*`,
  `lib/device_platform/src/temperature_source.hpp`,
  `lib/fermentation_app/src/program_model.*`, `run_commands.*`,
  `run_snapshot.*`, `run_persistence_*`, `process_state_machine.*`,
  `event_journal.hpp`, `fermentation_application.*` sowie den direkt
  betroffenen nativen Tests.

Roadmaps und historische Plaene werden nicht als implementierter Ist-Stand
behandelt. Der aktuelle Status ist aus Live-GitHub, dem aktuellen Code und
`docs/ROADMAP.md` abgeleitet.

## 4. Ziel und Nicht-Ziele

### Ziel

Issue #21 liefert einen deterministischen, nativ testbaren
Fermentations-Anwendungsdienst, der aus dem unveraenderlichen Laufvertrag,
dem Programmsensorvertrag und den drei rollenbezogenen #20-
`SensorQualitySnapshot`s ableitet:

- welcher Sensor im Lauf primaer fuer die Regelung ist;
- ob der ausgewaehlte Modus die benoetigte Peltierfreigabe zulaesst;
- wie ein Produktfuehlerfehler zuerst sicher zur Peltierabschaltung fuehrt;
- wann ein programmabhaengiger Luft-Ersatzbetrieb zulaessig ist;
- wie `remain_on_air_until_end`, `manual_return_to_product` und
  `automatic_validated_return_to_product` umgesetzt werden;
- wie jeder tatsaechliche Wechsel als sichtbares fachliches Laufereignis,
  Laufrevision und spaeter auslesbare Meldungs-/Journalreferenz weitergegeben
  wird.

Die Entscheidung ist reversibel: Sie mutiert den laufenden Zustand nicht
selbst, sondern liefert eine erwartete Vorher-/Nachher-Entscheidung. Eine
lauf- oder aktorwirksame Anwendung erfolgt erst nach der bestehenden
atomaren Persistenz-/Anwendungskette.

### Nicht-Ziele

- keine DS18B20-, 1-Wire-, GPIO-, Display-, Touch-, WLAN- oder
  ESP-IDF-Treiberimplementierung;
- keine konkrete Pin-, Pegel-, Bus- oder Steckerentscheidung;
- keine PI-Regelung, Luftbegrenzungsregel, Aktorplanung, Totzeit oder
  Lueftersteuerung aus #22/#23;
- keine Fehlerklassen, persistente Verriegelung, `SAFE_BOOT` oder
  automatische Fehlerfreigabe aus #24;
- keine direkte Aktorfreigabe: der Dienst erzeugt nur eine abstrakte
  Freigabe-/Sperrabsicht;
- keine Kaskadenregelung, kein PID-Autotuning und keine sonstige
  `FUTURE_RELEASE`-Funktion;
- keine neue allgemeine Plattformabstraktion und keine Bibliothek;
- keine UI-/Web-Rendererimplementierung. Es werden nur die gemeinsamen
  fachlichen Meldungs-/Ereignisdaten geliefert;
- keine unbestaetigten Hardware- oder thermischen Werte als Runtimewerte;
- keine stille Aenderung des Programmschnappschusses oder der
  Programmkatalogrevision.

## 5. Befund des aktuellen Codes

### Bereits vorhanden

- `device_platform::SensorQualityPipeline` verarbeitet eine einzelne Quelle
  und liefert `VALID`, `STALE`, `FAILED`, Roh-/Korrektur-/Filterwerte,
  Messalter, Trend, Fehlerursache und Wiedererkennungsfortschritt.
- `SensorQualitySnapshot` ist bewusst rollenunabhaengig. #21 muss drei
  Instanzen in der Anwendung zu `Schrankluft`, `Produkt` und `Kuehlkoerper /
  Aussenwaermetauscher` zuordnen, ohne `device_platform` mit
  Fermentationsbegriffen zu belasten.
- `ProgramDefinition` kennt `SensorPreference` und
  `ProductSensorFailurePolicy` sowie die validierte
  `fallbackDelaySeconds`. Die Werte werden bereits im Programmdokument
  kodiert und validiert.
- `ProgramStartRequest` und `ManualRunPlanRequest` besitzen einen
  `RunSensorMode`; die bestaetigte Startzusammenfassung zeigt den gewaehlten
  Modus.
- `RunCommandState::activeRunSensorMode` wird bei aktiven Programm- und
  manuellen Laeufen gefuehrt. `RunPersistenceSnapshot` und der bestehende
  Schema-1-Codec speichern diesen Modus bereits; die Persistenzimplementierung
  fuehrt aber noch keine Auswahlentscheidung aus.
- Die Zustandsmaschine kennt die relevanten Phasen und akzeptiert nur
  abstrakte `ProcessSignals`. Sie kennt noch keine Sensorrollen-
  oder Fallbacklogik.
- Laufzeitmeldungen sind begrenzt und zeigen Fehler-/Prozessmeldungen an;
  `IEventJournal` ist ein schmaler anwendungsneutraler Port. Ein
  Sensorwechselcode ist noch nicht vorhanden.
- `FermentationApplication` und beide Composition Roots sind derzeit nur
  Grundgeruest. Es gibt keinen produktiven Sensor- oder Regelzyklus, an den
  #21 voreilig Hardware koppeln duerfte.

### Fehlende Teile

- kein `SensorSelection`-Wertmodell oder Entscheidungsdienst;
- keine verbindliche Abbildung von Programmpraeferenz und angefordertem
  `RunSensorMode` auf einen wirksamen Startmodus;
- keine Trennung zwischen Moduswahl und Peltierfreigabe bei `STALE`/
  `FAILED`-Snapshots;
- kein programmabhaengiger Warte-/Fallback-Timer im Sensorentscheid;
- kein Rueckkehrpruefvertrag mit rollenuebergreifender Plausibilitaet;
- keine manuelle Aktion fuer Fortsetzung mit Luft beziehungsweise Rueckkehr zum
  Produktfuehler;
- keine fachliche Sensorwechselmeldung oder atomare Laufrevisionswirkung;
- keine gezielten Tests fuer Ausfall, gleichzeitig ungueltige feste Sensoren,
  Rueckkehr oder Moduserhalt in Kuehl-/Haltephasen.

## 6. Vorgeschlagene Fachvertraege

Die folgenden Typen und Regeln sind der umsetzbare Planvorschlag. Ihre
materielle Freigabe erfolgt erst mit `PLAN APPROVED` fuer diesen Plan-Commit.
Kleine Namen koennen innerhalb dieses Vertrags mechanisch angepasst werden;
Produkt-, Safety-, Persistenz- oder API-Wirkung darf danach nicht still
veraendert werden.

### 6.1 Eingaben und Rollen

Die Anwendung ordnet je Lauf genau einen Snapshot jeder Rolle zu:

```text
air      = fester Schrankluftfuehler
product  = abnehmbarer Produktfuehler
cooling  = Kuehlkoerper-/Aussenwaermetauscherfuehler
```

Der Selektor kennt die `SensorQualitySnapshot`-Felder, aber keine
`ITemperatureSource` und keinen Bus. Alle drei Snapshots werden in einem
expliziten Eingabewert zusammen mit folgenden fachlichen Daten uebergeben:

- unveraenderlicher `ProgramDefinition`-/Laufkontext;
- aktueller `RunSensorMode` und Auswahlstatus des Laufes;
- aktueller `ProcessState` einschliesslich `COOLING` und `COOL_HOLDING`;
- monotone Zeit des Bewertungsaufrufs;
- explizite Benutzeraktion, falls die konfigurierte Strategie eine verlangt;
- optional Richtung/Alter der letzten abstrakten Regelanforderung, aber keine
  konkrete Aktorimplementierung.

`SensorQuality::Valid` ist die notwendige Qualitaet fuer einen nutzbaren
Regelwert. Ein Snapshot mit `STALE`, fehlendem Wert, zu hohem Alter oder
`FAILED` ist nicht fuer eine Peltierfreigabe nutzbar. Ein alter Wert darf in
Diagnose und Fehlerbewertung sichtbar bleiben, wird aber nicht als aktuell
verwendet.

### 6.2 Ausgabewert und Zustandsseparation

Der neue Dienst unter `lib/fermentation_app/src/sensor_selection.*` liefert
eine noch nicht angewendete Entscheidung mit mindestens:

```text
before.activeMode
after.activeMode
before/after.selectionPhase
controlSensor: Product | Air | None
peltierPermission: Allowed | Blocked
blockReason: None | ProductSensorUnusable | AirSensorUnusable |
             CoolingSensorUnusable | SimultaneousFixedSensorFailure |
             CrossRoleEvidenceIndeterminate | PolicyWait |
             UserActionRequired | SafeStateRequired | InvalidContext
event: optional structured sensor/run event
expectedRunRevision
```

`activeMode` und `peltierPermission` sind bewusst getrennt. Beim ersten
Produktfehler bleibt der aktive Modus zunächst nachvollziehbar Produkt,
während die Peltierfreigabe sofort `Blocked` ist. Erst eine validierte
Fallbackentscheidung darf `activeMode` auf Luft setzen.

Der Dienst kennt keine GPIO-Pegel. `Allowed` bedeutet nur, dass die
fachlichen Sensorvoraussetzungen fuer den nachgelagerten Safety-/Aktorpfad
vorliegen; #23/#24 entscheiden zusaetzlich ueber Regel-, Aktor- und
Verriegelungsbedingungen.

### 6.3 Startauswahl

Die Implementierung muss die Programmpraeferenz mit dem bereits angeforderten
`RunSensorMode` abgleichen. Sie darf eine inkompatible Anfrage nicht still
umdeuten. Der wirksame Modus wird vor dem bestaetigten Start in der
Startzusammenfassung ausgegeben und bei einem automatischen initialen
`ProductIfAvailableElseAir`-Fallback als Auswahlereignis gekennzeichnet.

Der Plan verwendet folgende vorlaeufige Matrix als Reviewgrundlage:

| `SensorPreference` | vorgesehene Startregel |
|---|---|
| `ProductIfAvailableElseAir` | Produkt, wenn angefordert und gueltig; sonst Luft nur als explizit erlaubte Ersatzwahl |
| `AirProductOptional` | Luft ist der Standard; Produkt darf nur bei expliziter, kompatibler Startauswahl primaer werden |
| `ProductRequired` | Produkt muss gueltig sein; ein Luftfallback ist nicht zulaessig |
| `AirOnly` | ausschliesslich Luft; Produkt ist Anzeige-/Plausibilitaetsevidenz |

Die endgueltige Matrix und insbesondere die Kombination
`ProductRequired + FallbackToAirAfterTimeout` ist ein Owner-Gate (siehe
Abschnitt 13). Bis zur Entscheidung darf die Implementierung keine
Kombination als fachlich gueltig behaupten.

### 6.4 Produktfehler und Ersatzbetrieb

Im produktgefuehrten Lauf gilt:

```text
Produkt-Snapshot nicht nutzbar
  -> Peltierfreigabe sofort sperren
  -> Air und Cooling pruefen
  -> sichtbares Warn-/Ereignisobjekt erzeugen
  -> programmabhaengige Wartezeit abwarten
  -> Strategie pruefen
  -> nur bei gueltigen festen Sensoren auf Air wechseln
```

Verbindliche Regeln:

- `Air` und `Cooling` muessen fuer jede Peltierfreigabe aktuell und `VALID`
  sein; ein gleichzeitiger Ausfall verhindert jeden Ersatzbetrieb.
- Ein ungueltiger Produktwert allein macht den Luftsensor nicht automatisch
  zum Ersatzsensor. Erst die komplette Entscheidung mit Wartezeit,
  Policy und Sicherheitsbedingungen darf den Modus wechseln.
- `FallbackToAirAfterTimeout` wechselt automatisch nach der programmierten
  Wartezeit, nie frueher und nicht aus einem einzelnen wieder gueltigen
  Produktwert.
- `WaitForUser` bleibt nach der Wartezeit ohne Luftwechsel, bis eine
  explizite, validierte Benutzerentscheidung vorliegt.
- `StopToSafeState` erzeugt keine Regelmodusfreigabe und bleibt fuer #24 als
  verriegelungs-/Fehlerkandidat sichtbar.
- Ist der Luft- oder Kuehlkoerpersensor nicht sicher gueltig, wird nicht auf
  Luft umgeschaltet. `peltierPermission` bleibt `Blocked`.
- Die Wartezeit ist ein konfigurierter Programwert; der reale Wert bleibt
  `TBD_COMMISSIONING`. Firmwarefeste Obergrenzen und eine fehlende/ungueltige
  Konfiguration fuehren fail-closed zu einer gesperrten Entscheidung.

### 6.5 Rueckkehr zum Produktfuehler

Eine Rueckkehr ist nur erlaubt, wenn:

- die Strategie die Rueckkehr erlaubt;
- der Produkt-Snapshot `VALID` ist und einen verwendbaren Filterwert besitzt;
- #20s Wiedererkennungsregeln bereits mehrere plausible Proben und die
  geforderte Stabilitaet belegen;
- Produkt- und Luftwerte sowie Trends im aktuellen Prozesskontext
  kompatibel sind;
- Air und Cooling weiterhin gueltig sind;
- keine ungeklaerte konkurrierende Safety-/Fehlerlage besteht.

Ein einzelner gueltiger Messwert reicht nie. Ein grosser Unterschied zwischen
Produkt und Luft beweist allein keinen Fehler; der Selektor darf eine
kurzzeitige thermische Traegheit nicht als Ausfall klassifizieren. Bei
unklarer rollenuebergreifender Evidenz bleibt er konservativ bei `Blocked`
beziehungsweise im Luftmodus und liefert einen Verdachts-/Diagnosestatus,
keine unbelegte Schuldzuweisung.

Die Strategien sind:

```text
remain_on_air_until_end
  -> aktive Luftregelung bleibt bis zum Laufende erhalten

manual_return_to_product
  -> nur explizite Benutzeraktion nach vollstaendiger Validierung erlaubt

automatic_validated_return_to_product
  -> nach #20-VALID, Stabilitaet und rollenuebergreifender Plausibilitaet
     automatisch auf Produkt wechseln
```

Der Wechsel zurueck wird genauso sichtbar, revidiert und protokolliert wie
der Fallback. Ein Quittieren einer Meldung ist keine Rueckkehrfreigabe.

### 6.6 Phasen, Kuehlen und Halten

Der Wechsel von `FERMENTING` nach `COOLING` oder `COOL_HOLDING` loest keine
automatische Neuauswahl aus. Der wirksame Regelsensor bleibt erhalten:

```text
Produktmodus -> Produktmodus
Luftmodus    -> Luftmodus
```

Ein Sensorfehler waehrend `COOLING` oder `COOL_HOLDING` durchlaeuft jedoch
denselben Sicherheits- und Policyvertrag. Der Plan legt keine Sonderregel
fest, die einen Moduswechsel nur wegen der Phase erzwingt.

### 6.7 Ereignis-, Meldungs- und Revisionsvertrag

Jeder tatsaechliche Wechsel von `Product` nach `Air` oder von `Air` nach
`Product` erzeugt einen strukturierten `SensorSelectionEvent` mit:

- Lauf-ID und Laufrevision;
- monotone Zeit und, falls vorhanden, UTC-Anker;
- vorherigem und neuem Modus;
- Ursache: Produktfehler, automatische Rueckkehr oder Benutzeraktion;
- angewandter Policy und kurzer Diagnosegrund;
- Sensorqualitaet/Alter der drei Rollen als Evidenzreferenz, nicht als
  scheinbar aktueller Wert bei `FAILED`;
- Konfidenz-/Plausibilitaetsstatus.

Die bestehende runtime-seitige Meldungsstruktur wird um einen klaren
Sensorwechselcode beziehungsweise eine gleichwertige strukturierte
Ereignisreferenz ergaenzt. Die Entscheidung darf eine
`IEventJournal`-Aufzeichnung nicht als Voraussetzung fuer die fachliche
Berechnung machen; ein fehlgeschlagenes Komfort-/Journal-Write darf keine
Aktorfreigabe erzeugen. Die dauerhafte Aufbewahrung und Exportform des
Journals bleibt im Scope von #19. #21 muss jedoch die Ereignisdaten so
liefern, dass #19 sie ohne Parallelvertrag persistieren kann.

### 6.8 Atomare Laufwirkung und Neustart

Der bestehende `activeRunSensorMode` wird als kanonischer aktiver Modus
weiterverwendet. Ein angewendeter Moduswechsel:

1. wird gegen erwartete Lauf-/Zustandsrevision geprueft;
2. wird zusammen mit neuer Laufrevision und Ereignisreferenz atomar
   vorbereitet und gespeichert;
3. wird erst nach bestaetigtem Commit im RAM angewendet;
4. wird danach fuer Regel- und Safetyentscheidungen verwendet.

Direkte GPIO- oder alte Aktorzustaende werden nicht gespeichert. Ein Neustart
setzt keine Produktfehlerquittierung zurueck und darf aus einem alten Modus
keine blinde Aktorfreigabe ableiten. Falls der laufende Fallback-/Rueckkehr-
Zwischenzustand fuer eine exakte Fortsetzung benoetigt wird, muss er in einer
versionierten Laufprojektion mit Rueckfallrevision landen; eine fluechtige
zweite Wahrheit ist nicht zulaessig. Ob dafuer neben dem bereits persistierten
Modus weitere Felder erforderlich sind, ist ein Owner-Gate.

## 7. Modul- und Abhaengigkeitsgrenzen

Die geplante Richtung bleibt:

```text
fermentation_app sensor_selection
  -> device_platform::SensorQualitySnapshot / allgemeine Werttypen
  -> fermentation_app ProgramDefinition / RunSensorMode / ProcessState

device_platform_test_support -> device_platform
native tests -> fermentation_app + test support
Composition Roots -> passende Plattform + Anwendung
```

`device_platform` erhaelt keine Begriffe wie `ProductSensorFailurePolicy`,
`FERMENTING` oder `RunSensorMode`. Der Selektor bleibt in
`fermentation_app`, weil er konkrete Fermentationsrollen, Programme und
Prozessphasen kennt. Er verwendet keine Arduino-, ESP-IDF-, Datei-, Netzwerk-
oder Echtzeit-API.

Die Composition Roots bleiben unveraendert, solange keine reale
Sensorverkabelung existiert. Eine spaetere Laufzeitintegration ruft den
Selektor ueber schmale Anwendungsports und deterministische monotone Zeit auf;
sie darf keine Logik in `src/main.cpp` oder `main/app_main.cpp` verschieben.

## 8. Voraussichtlicher Datei- und Commit-Schnitt

Die genaue Liste wird nach Planfreigabe nur innerhalb dieses Scopes umgesetzt.

### Commit 1 – Auswahlkern und direkte native Unit-Tests

Voraussichtliche Dateien:

- neu: `lib/fermentation_app/src/sensor_selection.hpp`;
- neu: `lib/fermentation_app/src/sensor_selection.cpp`;
- neu: `lib/fermentation_app/src/sensor_selection_limits.hpp`, nur falls
  fuer firmwarefeste Validierungsobergrenzen erforderlich;
- neu: `test/test_sensor_selection/test_sensor_selection.cpp`;
- gegebenenfalls `lib/fermentation_app/CMakeLists.txt` nur fuer eine
  notwendige, bestehende Modulregistrierung.

Inhalt: Werttypen, reine Entscheidung, Start-/Fallback-/Rueckkehrmatrix,
Sicherheitsvoraussetzungen, Zeitvergleich, Rollenvergleich und
fehlgeschlagene/unklare Eingaben. Keine Persistenz und keine Aktoradapter.

### Commit 2 – Lauf-, Meldungs- und Persistenzanschluss

Voraussichtliche Dateien, nur sofern die Owner-Gates in Abschnitt 13 dies
freigeben:

- `lib/fermentation_app/src/run_commands.hpp/.cpp` fuer schmale
  Sensorwechselereignisse, sichere manuelle Aktionen und atomare
  Laufrevisionsvorbereitung;
- `lib/fermentation_app/src/run_persistence_contract.hpp/.cpp` und
  `run_persistence_coordinator.hpp/.cpp` fuer die Auswahlentscheidung als
  eigenstaendigen, nicht als Benutzerkommando behandelten Laufmutationspfad;
- `lib/fermentation_app/src/run_persistence_codec.hpp/.cpp` nur bei einer
  erforderlichen versionierten Persistenzprojektion;
- direkt betroffene Tests unter
  `test/test_run_commands/`, `test/test_run_checkpoint_codec/` und
  `test/test_run_persistence_coordinator/`.

Die bestehende Schema-1-Kompatibilitaet, der unveraenderliche
Programmschnappschuss, die begrenzten Revisions-/Nachrichtenpuffer und die
fail-closed Persistenzordnung bleiben erhalten. Eine neue Schema- oder
Wireformatentscheidung ist nicht still in diesem Commit enthalten; sie muss
als materielle Abweichung neu geplant und freigegeben werden, falls der
bestehende `activeRunSensorMode`-Vertrag nicht ausreicht.

### Commit 3 – fachliche Dokumentation und Abschlussnachweise

Voraussichtliche Dokumentationsdateien:

- `docs/TEMPERATURE_CONTROL.md` fuer die konkret implementierte
  Auswahl-/Fallback-/Rueckkehrauslegung;
- `docs/RECOVERY_AND_INTERRUPTION.md` fuer Neustart und Fallbackzustand;
- `docs/SAFETY_COMPONENT_FAULTS.md` fuer Produkt-/Fest-Sensorfehler und
  die Grenze zu #24;
- `docs/LOCAL_RUNTIME_UI.md` und/oder
  `docs/DIAGNOSTICS_AND_MAINTENANCE.md` fuer sichtbare Wechselereignisse;
- `docs/ACCEPTANCE_TESTS.md` fuer die nativen/simulierten #21-Faelle;
- `CHANGELOG.md` nur mit einem knappen, nicht duplizierenden Eintrag;
- `docs/ROADMAP.md` nur bei einer tatsaechlichen Status-/Gateaenderung.

Keine ADR-, Issue-, Hardware- oder Bibliotheksdatei wird ohne explizite
Ownerfreigabe angelegt oder geaendert.

## 9. Teststrategie und Testmatrix

In der Planungsphase werden keine Builds und keine produktiven Testlaeufe
ausgefuehrt. Nach der Planfreigabe gelten waehrend der Umsetzung nur gezielte
Tests. Die vollstaendige CI startet ausschliesslich durch den Owner auf
`Ready for review`.

### 9.1 Unit-Tests des Auswahlkerns

- Luftgefuehrter Start bleibt Luft, Produkt ist nur optionale Evidenz;
- produktgefuehrter Start mit gueltigem Produkt waehlt Produkt;
- unzulaessige Startpraeferenz wird ohne stilles Umdeuten abgelehnt;
- Produkt `STALE` und `FAILED` sperren Peltier sofort;
- Produktfehler vor Ablauf der Wartezeit bleibt ohne Luftwechsel;
- `FallbackToAirAfterTimeout` wechselt erst nach der exakten monotone Zeit;
- Zeitruecklauf, fehlende Zeitbasis und ungueltige Konfiguration bleiben
  fail-closed;
- `WaitForUser` wartet auch nach Timeout auf eine ausdrueckliche Aktion;
- `StopToSafeState` liefert keine Luftfreigabe;
- ungueltige Luft sperrt Ersatzbetrieb;
- ungueltiger Kuehlkoerpersensor sperrt jede Peltierfreigabe;
- gleichzeitiger Ausfall von Luft und Kuehlkoerper liefert keinen
  Ersatzmodus;
- Produkt allein darf eine feste Sicherheitsrolle nie ersetzen;
- `remain_on_air_until_end` verhindert jede automatische Rueckkehr;
- `manual_return_to_product` lehnt fehlende, unbestaetigte oder ungueltige
  Benutzeraktionen ab;
- automatische Rueckkehr wird mit nur einem gueltigen Messwert abgelehnt;
- automatische Rueckkehr wird nach mehreren #20-konformen stabilen,
  plausiblen Proben zugelassen;
- unplausible Produkt-/Luftdifferenz oder unklare Evidenz verhindert die
  Rueckkehr ohne unbelegte Schuldzuweisung;
- Wechsel erzeugt genau ein Ereignis mit altem/neuem Modus, Ursache und
  Laufrevision;
- wiederholte Bewertung ohne neue Modusaenderung erzeugt kein doppeltes
  Wechselereignis;
- `COOLING` und `COOL_HOLDING` aendern den Modus nicht allein durch den
  Phasenwechsel;
- Sensorfehler in `COOLING`/`COOL_HOLDING` verwenden weiterhin den gleichen
  Safety-/Policyvertrag;
- ungueltige Rolle, Enum-, Lauf- oder Snapshotdaten erzeugen keine Teilwirkung.

### 9.2 Konsumenten- und Laufvertragstests

- Startzusammenfassung zeigt den effektiven Sensorbetrieb vor Bestaetigung;
- akzeptierte Moduswechsel aktualisieren `activeRunSensorMode` und
  Laufrevision nur atomar;
- abgelehnte oder nicht gespeicherte Entscheidungen veraendern weder RAM
  noch Aktoranforderung;
- der bestehende Programmschnappschuss bleibt bei jedem Wechsel unveraendert;
- Laufpersistenz round-tript den aktiven Modus und die neue Revision;
- Restart-/Fallbackmetadaten werden entweder vollstaendig versioniert
  wiederhergestellt oder – falls der Owner die Minimalvariante freigibt –
  konservativ als neuer pending Zustand ohne Aktorfreigabe bewertet;
- alte Schema-Records werden nicht durch neue Auswahlfelder unlesbar;
- Sensorwechsel-/Warnmeldungen bleiben begrenzt, quittierbar und getrennt
  vom Fehlerreset;
- Journal-/Meldungsausfall darf keine neue Peltierfreigabe erzeugen.

### 9.3 Gezielte Ausfuehrung nach Freigabe

Mindestens direkt betroffen:

```bash
pio test -e native --filter test_sensor_selection
pio test -e native --filter test_run_commands
pio test -e native --filter test_run_checkpoint_codec
pio test -e native --filter test_run_persistence_coordinator
python scripts/check_architecture_boundaries.py
python scripts/check_secrets.py
git diff --check
```

Der exakte Filter wird an die tatsaechlich geaenderten Tests angepasst. Nur
ausgefuehrte Befehle werden im PR als Nachweis genannt. Vollstaendige native
und ESP-IDF-Laeufe sind in dieser Umsetzungsphase nicht doppelt lokal
auszufuehren; sie bleiben Owner-/Remote-CI-Gate gemaess
`docs/CI_AND_QUALITY_GATES.md`.

## 10. Safety-, Security-, Recovery- und Hardwaregrenzen

- Bei Boot, Reset, `STALE`, `FAILED`, unklarer Auswahl, ungueltigen festen
  Sensoren, fehlendem Persistenzcommit oder unklarer Rollenplausibilitaet
  wird keine neue Peltierfreigabe erzeugt.
- Der Produktfuehler ist niemals Ersatz fuer Schrankluft oder Kuehlkoerper.
- Heizen/Kuehlen bleibt ausschliesslich #22/#23 vorbehalten; #21 setzt keine
  Richtung und keinen GPIO.
- Eine Sicherheitsabschaltung ueberstimmt Warte-, Mindest- oder
  Rueckkehrzeiten.
- Quittierung ist keine Rueckkehr- oder Fehlerresetfreigabe; Neustart ist
  kein Fehlerreset.
- Programmschnappschuss, Laufrevision und Sensorwechselereignis muessen
  konsistent sein; bei Persistenzunsicherheit bleibt der bisherige fachliche
  Zustand wirksam.
- Ereignisse enthalten keine Secrets, keine WLANdaten, keine Service-PIN,
  keine Rohkonfiguration und keine unbestaetigten Hardwarewerte.
- `TBD_HARDWARE` bleibt bei Sensorbus, ROM-/Rollenverdrahtung und realer
  Quelle offen. `TBD_COMMISSIONING` bleibt bei Fallbackwartezeit,
  Stabilitaetsdauer, Differenz-/Trendgrenzen und thermischer Kompatibilitaet
  offen. Kein TBD-Wert darf unbemerkt als gueltiger Produktivwert dienen.
- Hardwaretests, reales Sensorabziehen, Peltierpulse und thermisches Tuning
  sind nicht Bestandteil dieses Plan-PRs und bleiben in den verknuepften
  Hardware-/Inbetriebnahme-Issues blockiert.

## 11. Ressourcen- und Betriebsbudget

- Der Auswahlkern verwendet feste Werttypen, keine unbounded Historie und
  keine neue Bibliothek.
- Sensorqualitaetswerte werden als begrenzte Snapshots referenziert; keine
  zweite unlimitierte Messhistorie wird angelegt.
- Laufereignisse verwenden die bestehenden begrenzten Lauf-/Meldungs-/Journal-
  grenzen. Neue Felder benoetigen eine sichtbare Groessenbewertung.
- Erwartete RAM-Wirkung: ein kleiner Auswahlstatus und ein Ereigniswert pro
  aktivem Lauf; reale Byte-/Heapwerte bleiben
  `TBD_IMPLEMENTATION_BUDGET`, bis ein reproduzierbarer Build sie belegt.
- Keine PSRAM-, OTA-, Netzwerk- oder Echtzeitabhaengigkeit.

## 12. SOLID-, DRY- und KISS-Bewertung des geplanten Diffs

- **Single Responsibility:** `SensorQualityPipeline` bleibt Evidenzlieferant;
  `SensorSelection` entscheidet Rollen und Policy; Lauf-/Persistenzcode
  wendet nur atomare Entscheidungen an; Safety-/Aktorlogik bleibt in #24/#23.
- **Open/Closed:** neue Sensorstrategien werden ueber den bestehenden
  `RunSensorMode`-/Policyvertrag und kleine Werttypen erweitert, nicht durch
  Hardware-`if`-Ketten in Composition Roots.
- **Liskov:** alle Mocks liefern weiterhin den kanonischen
  `ITemperatureSource`-/Snapshotvertrag; keine konkrete Hardwareklasse wird
  im Auswahlkern vorausgesetzt.
- **Interface Segregation:** der Auswahlkern erhaelt nur Snapshots,
  Laufkontext und monotone Zeit; Eventjournal, Aktor und UI werden nicht als
  Universalinterface hineingezogen.
- **Dependency Inversion:** Fachentscheidung haengt von `SensorQualitySnapshot`
  und schmalen Anwendungsdaten ab, nicht von DS18B20, ESP-IDF, GPIO oder
  PlatformIO.
- **DRY:** `RunSensorMode`, Programmpolicies und #20-Qualitaetsstatus werden
  wiederverwendet. Es entsteht kein zweiter Qualitaetsautomat und keine
  parallele Wire-Darstellung.
- **KISS:** ein reiner Entscheidungsdienst plus begrenzter Eventwert ist
  ausreichend. Eine generische Regel-Engine, Plugin-Registry oder
  Universalplattform waere unnoetige Abstraktion.

Bewusste Grenze: Die rollenuebergreifende Plausibilitaet muss in #21 liegen,
obwohl #20 bereits Einzelwerte filtert. Das ist keine DRY-Verletzung, weil
die Rollen- und Prozessbedeutung in `fermentation_app` entsteht und #20
bewusst keine Fermentationsrollen kennt.

## 13. Offene Ownerentscheidungen und Gates

Diese Punkte sind vor der Umsetzung eindeutig zu beantworten. Eine allgemeine
Zustimmung ohne Bezug auf den Plan-Commit reicht nicht.

| ID | Offene Entscheidung | Planvorschlag / Stopwirkung |
|---|---|---|
| P21-01 | Wie genau werden `SensorPreference`, angeforderter `RunSensorMode` und Produktverfuegbarkeit beim Start kombiniert? | inkompatible Kombinationen ablehnen; niemals still umdeuten. Ohne Matrixfreigabe kein Startvertrag |
| P21-02 | Darf `ProductRequired` nach Produktfehler jemals Luft als Ersatz verwenden? | Vorschlag: nein; Policy-/Modellkonflikt fail-closed. Ohne Entscheidung keine Fallbackimplementierung fuer diese Kombination |
| P21-03 | Muss der pending Fallback-/Rueckkehrzeitpunkt ueber Neustart exakt fortgesetzt werden? | Vorschlag: aktive Auswahl und jede Modusrevision dauerhaft; bei zusaetzlichem Zwischenzustand nur versioniert und atomar. Keine fluechtige Parallelwahrheit |
| P21-04 | Wie wird ein Sensorwechsel in #21 an runtime message, `IEventJournal` und #19 gebunden? | Vorschlag: strukturierter Selektor-Event plus bestehender begrenzter Meldungspfad; dauerhafte Journalaufbewahrung bleibt #19 |
| P21-05 | Welche numerischen Fallback-, Stabilitaets-, Differenz- und Trendwerte gelten? | bleiben `TBD_COMMISSIONING`; nur firmwarefeste Obergrenzen in zentralem Limits-Header, kein Magic Default |
| P21-06 | Gilt automatische Rueckkehr auch waehrend `COOLING`/`COOL_HOLDING`, wenn die aktive Strategie sie erlaubt? | Vorschlag: keine phasenbedingte Neuauswahl, Policy gilt weiter; bei Abweichung Plan vor Umsetzung anhalten |
| P21-07 | Ist ein eigener persistenter Mutationspfad fuer Sensorwechsel im Scope von #21 zulaessig? | erforderlich fuer die in `RUN_PERSISTENCE.md` geforderte Laufrevision; bei Ablehnung nur Auswahlkern planen und Integration explizit nachfolgend blockieren |

Eine Entscheidung, die Schema, Wireformat, Fehlerklasse, Safetyfreigabe,
Hardwareannahme oder Issue-/PR-Struktur veraendert, ist eine materielle
Planabweichung. Dann wird die Umsetzung angehalten, der Plan aktualisiert,
neu committed und erneut freigegeben.

## 14. Dokumentations- und Abschlussnachweise

Vor `Ready for review` werden im Draft-PR auf dem exakten HEAD dokumentiert:

- freigegebene Plan-SHA und Zuordnung jedes umgesetzten Planpunkts zu
  Commits;
- tatsaechlich geaenderte Dateien und jede Abweichung vom erwarteten Diff;
- direkte Testbefehle und Ergebnisse, einschliesslich `BLOCKED`/
  `NOT_RUN` fuer nicht angeordnete Volltests oder Hardware;
- `git diff --check`, Secret- und Architekturpruefung;
- Nachweis, dass #20-Vertraege wiederverwendet und keine #22/#23/#24-
  Verantwortungen vorweggenommen wurden;
- konkrete SOLID-/DRY-/KISS-Pruefung gegen den tatsaechlichen Diff;
- verbleibende `TBD_HARDWARE`, `TBD_COMMISSIONING`,
  `TBD_IMPLEMENTATION_BUDGET` und Owner-Gates;
- offene Reviewthreads, ohne sie ohne ausdrueckliche Autorisierung zu
  beantworten oder zu schliessen;
- Nachweis, dass PR Draft bleibt und der Owner allein `Ready for review`,
  vollstaendige Remote-CI, Merge oder Branchloeschung steuert.

## 15. Verbindliche `/task`-Taskliste fuer die Umsetzung

Diese Liste ist die geplante Arbeitsliste fuer die Phase nach
`PLAN APPROVED`. Aufgaben werden nur nach echtem Nachweis abgehakt; neue
materielle Funde werden als Untertask aufgenommen.

```text
/task
[ ] exakten freigegebenen Plan-Commit und Ownerkommentar `PLAN APPROVED` verifizieren
[ ] aktuellen Branch, HEAD, Live-Issue #21, Abhaengigkeiten und Roadmap erneut pruefen
[ ] seit der Planfreigabe geaenderte Quellen, ADRs, Vertraege und lokale Regeln inkrementell lesen
[ ] P21-01 bis P21-07 aufgeloeste Ownerentscheidungen gegen den Plan abgleichen
[ ] `SensorQualitySnapshot`-Inputs ohne Parallelqualitaetsmodell anschliessen
[ ] Auswahlkern mit getrenntem activeMode-/peltierPermission-Vertrag implementieren
[ ] Startmatrix, Produktfehler, Wartezeit und alle drei Fallbackstrategien implementieren
[ ] feste Schrankluft-/Kuehlkoerpersensoren in jeder Peltierfreigabe erzwingen
[ ] Rueckkehr erst nach #20-VALID, stabilen Proben und Plausibilitaetspruefung zulassen
[ ] Moduserhalt bei Kuehlen und Halten implementieren und testen
[ ] strukturiertes Sensorwechselereignis und sichtbaren Meldungspfad anschliessen
[ ] atomare Laufrevision/Persistenzwirkung nur im freigegebenen Vertrag umsetzen
[ ] direkte, gezielte Unit-Tests fuer Auswahl, Fehler, Safetyblock und Rueckkehr ausfuehren
[ ] gezielte Laufkommand-/Laufpersistenz-/Codec-Konsumententests ausfuehren
[ ] gezielte Architektur-, Secret-, Format- und `git diff --check`-Pruefungen ausfuehren
[ ] betroffene Fachvertraege und `docs/ACCEPTANCE_TESTS.md` aktualisieren
[ ] `docs/ROADMAP.md` nur bei tatsaechlicher Status- oder Gatewirkung synchronisieren
[ ] Ressourcenwirkung, begrenzte Puffer und offene Hardware-/Commissioning-Gates dokumentieren
[ ] Review des vollstaendigen aktuellen Diffs gegen Issue, Plan, ADRs und Fachvertraege durchfuehren
[ ] SOLID-, DRY- und KISS-Bewertung gegen den tatsaechlichen Diff durchfuehren
[ ] keinen unbelegten Hardware-, Safety-, Persistenz- oder Bibliotheksentscheid im Diff belassen
[ ] alle Reviewbefunde fachlich bewerten; Threads nur nach ausdruecklicher Autorisierung bearbeiten
[ ] PR-Beschreibung mit Plan-SHA, aktuellem HEAD, Tests, Abweichungen und Restgates aktualisieren
[ ] Owner setzt Draft erst nach befundleerem Review auf `Ready for review`
[ ] genau eine vollstaendige Remote-CI fuer den reviewten HEAD abwarten und Ergebnis dokumentieren
[ ] bei CI-Fehler PR-Draft-/Korrektur-/Reviewzyklus gemaess Workflow durchfuehren
[ ] Abschlussnachweise, geaenderte Dateien und offene Gates vollstaendig dokumentieren
[ ] `HALTED_FOR_OWNER_REVIEW` beziehungsweise Owner-Entscheidung dokumentieren
```

## 16. Stopbedingung

Nach Commit und Push dieses Plans sowie der notwendigen Roadmap-Aktualisierung
wird im Draft-PR der exakte Plan-Commit, der aktuelle HEAD und

```text
IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

eingetragen. Danach haelt der Agent an. Implementierung beginnt ausschliesslich
nach einem Ownerkommentar der Form:

```text
PLAN APPROVED
Approved plan commit: <exakte Plan-Commit-SHA>
```

Die Freigabe gilt nur fuer genau diese Planversion. Der PR bleibt Draft; der
Agent setzt ihn nicht auf `Ready for review`, startet keine vollstaendige
Remote-CI, merged nicht und loescht den Branch nicht.

## 17. Planungs-Taskliste

```text
/task
[x] Live-main, Branch, Arbeitsbaum und PR #98 verifizieren
[x] Live-Issue #21 und Kommentare lesen
[x] Abhaengigkeiten #14 und #20 live verifizieren
[x] Root- und lokale AGENTS-Regeln lesen
[x] Dokumentationsprioritaet und relevante Fachvertraege lesen
[x] bestehenden #20-Sensorqualitaetskern und Lauf-/Persistenzmodelle inventarisieren
[x] fehlende Auswahl-, Fallback-, Rueckkehr- und Ereignisvertraege abgrenzen
[x] offene Ownerentscheidungen und materielle Planabweichungen dokumentieren
[x] SOLID-, DRY- und KISS-Bewertung des geplanten Diffs erstellen
[x] vollstaendige Umsetzung-, Test-, Dokumentations-, Review- und Abschluss-Taskliste erstellen
[x] `docs/ROADMAP.md` auf PR #98 merged und Issue #21 als aktuelle Planungsarbeit aktualisieren
[x] ausschliesslich Plan und notwendige Roadmap-Aktualisierung aendern
[ ] Plan committen und pushen
[ ] Draft-PR mit exakter Plan-SHA und aktuellem HEAD aktualisieren
[ ] HALTED_FOR_OWNER_REVIEW
```
