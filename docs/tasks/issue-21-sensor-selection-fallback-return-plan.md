# Plan: Issue #21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik

## 1. Metadaten und Status

```text
Issue: #21 [E3.2] Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik
Epic: #5 (E3)
Branch: plan/issue-21-sensor-selection-fallback-return
Baseline main: ff2e66a8c340d61c8c4517f90fd3fba5a8fc3db2
Vorheriger, NICHT freigegebener Plan-Commit: c505fce6cbd12a02f9c195cdba7bf0dc37d3c8bd
Context HEAD bei dieser Revision: 7822227be7cfcb085a068e1c100b99ed2c7549aa
PLAN_ONLY: YES
IMPLEMENTATION_STARTED: NO
PLAN_STATUS: PLAN_DRAFT_REVIEW_REQUIRED
IMPLEMENTATION_STATUS: IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

Diese Revision ersetzt den Plan-Commit `c505fce6...` vollstaendig. Sie
behebt die im PR-#99-Review vom 2026-08-06 benannten materiellen Befunde:
fehlende kanonische Rueckkehrstrategie im Programmmodell, unvollstaendiger
Auswahlzustandsautomat, undefinierter Vertrag fuer produktgefuehrte manuelle
Laeufe, unsauber getrennte Owner-Gates, fehlende vollstaendige Startmatrix,
zu flacher Plausibilitaetsvertrag und fehlende Cross-Field-Validierung.
Produktionscode, produktive Tests, Toolchain, Buildkonfiguration,
Hardwarekonfiguration und Abhaengigkeiten werden weiterhin in dieser
Planungsphase nicht geaendert.

## 2. Live-Issue- und Abhaengigkeitsabgleich

| Quelle | Live-Stand am 2026-08-06 | Bedeutung fuer diesen Plan |
|---|---|---|
| Issue #21 | OPEN, Body-Status `PLANNED_SPEC_PENDING`, keine Kommentare | eigener Plan-first-Draft-PR, keine Implementierungsfreigabe |
| Issue #14 | CLOSED | kanonische Prozesszustaende und Uebergangstopologie stehen zur Verfuegung |
| Issue #20 | CLOSED, Body-Status `READY` ist historisch | `SensorQualitySnapshot` und `SensorQualityPipeline` sind die bestehende Qualitaetsquelle |
| Epic #5 | OPEN | Issue bleibt Teil des E3-Sensor-/Regel-/Safety-Kerns |
| PR #99 | OPEN, Draft, dieser Plan ist die zweite Revision | Reviewbefunde vom 2026-08-06 sind Grundlage dieser Ueberarbeitung |

Issue #21 hat weiterhin keine Kommentare, die zusaetzliche Anforderungen oder
Ownerentscheidungen enthalten. Die Live-Issue-Beschreibung bleibt die
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

Zusaetzlich zu den in der ersten Revision gelesenen Quellen wurden fuer diese
Ueberarbeitung konkret nachvollzogen:

- `lib/fermentation_app/src/program_model.hpp/.cpp` und
  `lib/fermentation_app/src/program_limits.hpp` im Detail: Feldmaske,
  `kRequiredFields`-Array, `static_assert`-Absicherung, Validierungs- und
  Migrationsfunktion `migrateProgramToCurrentSchema` (bisher einstufig,
  Schema 4 -> 5);
- `lib/fermentation_app/src/configuration_document_codec.cpp` im Detail: die
  Wire-ID-Funktionen `sensorPreferenceToWireId` /
  `productSensorFailurePolicyToWireId`, `readProgramSchema` (inline
  Migration beim Decode), `readProgramIdentityAndFlags`,
  `readProgramLimitsAndCompletion` (schemaversionsabhaengiges optionales
  Feld) und die Payload-Groessenberechnung;
- `lib/fermentation_app/src/standard_program_catalog.cpp`: alle vier
  Werkskatalogeintraege verwenden `ProductIfAvailableElseAir` oder
  `AirProductOptional`, keiner `AirOnly`;
- `config/programs.example.yaml`: aktuell `schema_version: 4`, obwohl der
  Code bereits Schema 5 (`kCurrentProgramSchemaVersion`) verlangt - die
  Beispielkonfiguration war bereits vor diesem Plan nicht synchron;
- `lib/fermentation_app/src/run_commands.cpp`, insbesondere
  `decideProgramStart` (Zeilen ~373-448): der angeforderte `RunSensorMode`
  wird bislang **nicht** gegen `program.sensorPreference` geprueft, sondern
  unveraendert in `StartSummary` uebernommen; `validateManualRunPlan`
  akzeptiert `RunSensorMode::Product` ohne jede Policy-, Wartezeit- oder
  Rueckkehrzuordnung;
- `lib/fermentation_app/src/run_snapshot.hpp`: `RunRevision` und
  `RunChangeReason` sind gezielt auf Zieltemperatur-/Restdauer-Anpassungen
  zugeschnitten (`EffectiveRunValues before/after` kennt keinen Sensormodus);
  eine Wiederverwendung dieser Struktur fuer Sensorwechsel wuerde ein
  fachfremdes Datenfeld erzwingen und wird deshalb verworfen (siehe 6.11);
- `lib/fermentation_app/src/run_persistence_contract.hpp/.cpp` und
  `run_persistence_codec.cpp`: `RunPersistenceSnapshot` persistiert bereits
  `activeRunSensorMode` sowie das gesamte eingebettete `ProgramDocument`
  (`program->sourceProgram`) ueber denselben
  `encodeProgramDocumentPayload`/`decodeProgramDocumentPayload`-Codec wie der
  Programmkatalog; ein Laufcheckpoint enthaelt also immer ein vollstaendiges,
  versioniertes Programmdokument und profitiert automatisch von dessen
  Schema-Migration beim Decode. Das separate `kRunPersistenceSchema` (aktuell
  `1`, ohne Migrationsmechanismus) bleibt davon unberuehrt, solange keine
  neuen Felder in `RunPersistenceSnapshot` selbst noetig werden;
- `lib/device_platform/src/sensor_quality_snapshot.hpp`: `SensorQualitySnapshot`
  enthaelt keinen Fehlerbeginn-Zeitstempel und keine Rollenkennung; Alter,
  Trend (`changeRateCelsiusPerSecond`) und Wiedererkennungsfortschritt
  (`recoveryProgressCount`) sind pro Instanz vorhanden.

Roadmaps und historische Plaene werden nicht als implementierter Ist-Stand
behandelt. Der aktuelle Status ist aus Live-GitHub, dem aktuellen Code und
`docs/ROADMAP.md` abgeleitet.

## 4. Ziel und Nicht-Ziele

### Ziel

Issue #21 liefert einen deterministischen, nativ testbaren
Fermentations-Anwendungsdienst, der aus dem unveraenderlichen Laufvertrag,
dem kanonisch erweiterten Programmsensorvertrag und den drei rollenbezogenen
#20-`SensorQualitySnapshot`s ableitet:

- welcher Sensor im Lauf primaer fuer die Regelung ist;
- ob der ausgewaehlte Modus die benoetigte Peltierfreigabe zulaesst;
- wie ein Produktfuehlerfehler zuerst sicher zur Peltierabschaltung fuehrt;
- wann ein programmabhaengiger Luft-Ersatzbetrieb zulaessig ist;
- wie `remain_on_air_until_end`, `manual_return_to_product` und
  `automatic_validated_return_to_product` als **kanonisches, typisiertes
  Programmfeld** (nicht nur als Laufzeitverhalten) umgesetzt werden;
- wie jeder tatsaechliche Wechsel als sichtbares fachliches Laufereignis,
  Laufrevision und spaeter auslesbare Meldungs-/Journalreferenz weitergegeben
  wird.

Die Entscheidung ist reversibel: Sie mutiert den laufenden Zustand nicht
selbst, sondern liefert eine erwartete Vorher-/Nachher-Entscheidung. Eine
lauf- oder aktorwirksame Anwendung erfolgt erst nach der bestehenden
atomaren Persistenz-/Anwendungskette.

Diese Revision macht zusaetzlich explizit zum Ziel:

- die Rueckkehrstrategie ist ein **Programmschema-Feld** mit eigener
  Feldmaske, Validierung, Migration, Codec-Wirkung, Factory-Default und
  Beispielkonfiguration - kein reines Laufzeitverhalten des Selektors;
- der Auswahlzustandsautomat ist vollstaendig benannt, inklusive
  Restart-Verhalten ohne verlaessliche verstrichene Zeit;
- produktgefuehrte manuelle Laeufe erhalten einen expliziten, festen
  Fehler-/Rueckkehrvertrag fuer Release 1.

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
  Programmkatalogrevision;
- keine Erweiterung von `kRunPersistenceSchema` (siehe 6.12) - der
  Restart-Vertrag wird bewusst ohne neues persistiertes Feld geloest;
- keine Wiederverwendung von `RunRevision`/`RunChangeReason` fuer
  Sensorwechsel (siehe 6.11) - das waere eine fachfremde Zweckentfremdung
  einer auf Temperatur-/Dauer-Anpassungen zugeschnittenen Struktur.

## 5. Befund des aktuellen Codes

### Bereits vorhanden

- `device_platform::SensorQualityPipeline` verarbeitet eine einzelne Quelle
  und liefert `VALID`, `STALE`, `FAILED`, Roh-/Korrektur-/Filterwerte,
  Messalter, Trend, Fehlerursache und Wiedererkennungsfortschritt.
- `SensorQualitySnapshot` ist bewusst rollenunabhaengig und enthaelt keinen
  Fehlerbeginn-Zeitstempel. #21 muss drei Instanzen in der Anwendung zu
  `Schrankluft`, `Produkt` und `Kuehlkoerper/Aussenwaermetauscher` zuordnen,
  ohne `device_platform` mit Fermentationsbegriffen zu belasten.
- `ProgramDefinition` kennt `SensorPreference` und
  `ProductSensorFailurePolicy` sowie die validierte
  `fallbackDelaySeconds`. **Es kennt jedoch keine Rueckkehrstrategie.** Das
  Feld fehlt im Modell, im Feldmaskenschema, im Binaercodec, im
  Werkskatalog und in `config/programs.example.yaml`.
- `ProgramStartRequest` und `ManualRunPlanRequest` besitzen einen
  `RunSensorMode`; die bestaetigte Startzusammenfassung zeigt den
  angeforderten Modus. **`decideProgramStart` prueft diesen Modus jedoch
  nicht gegen `program.sensorPreference`** - eine inkompatible Anfrage
  wuerde heute unveraendert durchgereicht, nicht abgelehnt.
- `RunCommandState::activeRunSensorMode` wird bei aktiven Programm- und
  manuellen Laeufen gefuehrt. `RunPersistenceSnapshot` und der bestehende
  Schema-1-Codec speichern diesen Modus bereits; die Persistenzimplementierung
  fuehrt aber noch keine Auswahlentscheidung aus.
- `RunPersistenceSnapshot.program->sourceProgram` durchlaeuft beim Decode
  denselben `readProgram`-Pfad wie der Programmkatalog, inklusive der
  bestehenden einstufigen Migration `kMigratableProgramSchemaVersion ->
  kCurrentProgramSchemaVersion`. Ein Laufcheckpoint mit eingebettetem
  Altschema-Programm wird also automatisch mitmigriert, sofern das
  Altschema noch die migrierbare Version ist.
- `RunRevision`/`RunChangeReason` (`run_snapshot.hpp`) sind eine
  Anpassungshistorie fuer Zieltemperatur und Restdauer
  (`EffectiveRunValues before/after`), keine generische Ereignishistorie.
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

- kein `ReturnStrategy`-Feld im Programmschema (Modell, Maske, Validierung,
  Migration, Codec, Katalog, Beispielkonfiguration);
- keine Cross-Field-Validierung zwischen `SensorPreference`,
  `ProductSensorFailurePolicy` und `ReturnStrategy` (z. B.
  `ProductRequired + FallbackToAirAfterTimeout`);
- keine Startmoduspruefung: `decideProgramStart` deutet keine inkompatible
  Kombination ab, weil es sie gar nicht kennt;
- kein `SensorSelection`-Wertmodell oder Entscheidungsdienst;
- kein vollstaendig benannter Auswahlzustandsautomat;
- keine Trennung zwischen Moduswahl und Peltierfreigabe bei `STALE`/
  `FAILED`-Snapshots;
- kein programmabhaengiger Warte-/Fallback-Timer im Sensorentscheid;
- kein Rueckkehrpruefvertrag mit rollenuebergreifender Plausibilitaet;
- kein Vertrag fuer produktgefuehrte manuelle Laeufe;
- keine manuelle Aktion fuer Fortsetzung mit Luft beziehungsweise Rueckkehr zum
  Produktfuehler;
- keine fachliche Sensorwechselmeldung oder atomare Laufrevisionswirkung;
- keine gezielten Tests fuer Ausfall, gleichzeitig ungueltige feste Sensoren,
  Rueckkehr, Moduserhalt in Kuehl-/Haltephasen, Startmatrix, Migration,
  Cross-Field-Validierung oder Restart.

## 6. Fachvertraege

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

- unveraenderlicher `ProgramDefinition`-/Laufkontext, jetzt inklusive
  `ReturnStrategy` (6.2);
- aktueller `RunSensorMode` und RAM-Auswahlzustand des Laufes (6.4);
- aktueller `ProcessState` einschliesslich `COOLING` und `COOL_HOLDING`;
- monotone Zeit des Bewertungsaufrufs;
- explizite Benutzeraktion, falls die konfigurierte Strategie eine verlangt;
- der begrenzte `CrossRolePlausibilityContext` (6.10).

`SensorQuality::Valid` ist die notwendige Qualitaet fuer einen nutzbaren
Regelwert. Ein Snapshot mit `STALE`, fehlendem Wert, zu hohem Alter oder
`FAILED` ist nicht fuer eine Peltierfreigabe nutzbar. Ein alter Wert darf in
Diagnose und Fehlerbewertung sichtbar bleiben, wird aber nicht als aktuell
verwendet.

### 6.2 Rueckkehrstrategie im kanonischen Programmmodell

Dies ist die zentrale Korrektur gegenueber der ersten Planrevision: die
Rueckkehrstrategie ist ein **Programmschemafeld**, kein reiner
Selektor-Laufzeitparameter.

#### 6.2.1 Modell

`lib/fermentation_app/src/program_model.hpp` erhaelt:

```cpp
enum class ReturnStrategy : std::uint8_t {
    RemainOnAirUntilEnd,
    ManualReturnToProduct,
    AutomaticValidatedReturnToProduct,
};

struct ProductSensorFailure {
    ProductSensorFailurePolicy policy{
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout};
    std::optional<std::uint32_t> fallbackDelaySeconds;
    ReturnStrategy returnStrategy{ReturnStrategy::ManualReturnToProduct};
};
```

Die Einordnung in `ProductSensorFailure` (statt eines neuen Top-Level-Felds
in `ProgramDefinition`) folgt der bestehenden fachlichen Gruppierung: die
Rueckkehr ist die Gegenbewegung zum in derselben Struktur konfigurierten
Ersatzverhalten und teilt dessen Policy-Kontext. Ein neues, paralleles
Top-Level-Feld waere eine unnoetige zweite Gruppierung fuer denselben
Fachzusammenhang (DRY/KISS).

`ProgramField` erhaelt ein neues Bit:

```cpp
ReturnStrategy = 1ULL << 16U,
```

`kSchema4RequiredProgramFields` bleibt unveraendert. Das bisherige
`kCurrentRequiredProgramFields` (Schema 5) wird in
`kSchema5RequiredProgramFields` umbenannt und bleibt inhaltlich identisch.
Das neue `kCurrentRequiredProgramFields` wird:

```cpp
kSchema5RequiredProgramFields | fieldMask(ProgramField::ReturnStrategy)
```

```text
kCurrentProgramSchemaVersion:    5 -> 6
kMigratableProgramSchemaVersion: 4 -> 5
```

Damit bleibt die bestehende Konvention erhalten, dass genau eine
Vorgaengerversion migrierbar ist (kein Mehrschritt-Upgrade). Ein
eingebettetes Programmdokument mit Schema 4 (z. B. ein sehr alter
Laufcheckpoint) wird nach diesem Bump `UnsupportedSchema`/
`UnsupportedSchemaVersion` statt migriert - das ist keine neue Regel,
sondern dieselbe bestehende Einschritt-Konvention, jetzt eine Version
weitergeschoben. Dieser Effekt wird explizit als Testfall erwartet (9.1)
und im Abschlussnachweis benannt, nicht stillschweigend in Kauf genommen.

`lib/fermentation_app/src/program_model.cpp`:

- `kRequiredFields` (aktuell 16 Eintraege) erhaelt einen 17. Eintrag
  `{ProgramField::ReturnStrategy, "defaults.product_sensor_failure.return_strategy"}`.
  Der bestehende `static_assert(kRequiredFields.size() ==
  kCurrentProgramFieldCount)` erzwingt bereits heute, dass ein vergessener
  Eintrag den Build bricht - dieser Mechanismus wird nicht umgangen.
- `validReturnStrategy(ReturnStrategy)` analog zu `validSensorPreference`/
  `validFailurePolicy`.
- `migrateProgramToCurrentSchema` bekommt einen zweiten Migrationsschritt
  (Schema 5 -> 6, bisheriger Schritt 4 -> 5 entfaellt ersatzlos, da
  `kMigratableProgramSchemaVersion` jetzt 5 ist). Migrationsabbildung:

  ```text
  program.sensorPreference != AirOnly
    -> returnStrategy = AutomaticValidatedReturnToProduct

  program.sensorPreference == AirOnly
    -> returnStrategy = RemainOnAirUntilEnd
       (operativ ohne Wirkung, siehe 6.9 und 6.13)
  ```

  Dies ist die vom Review empfohlene Grundlage und wird als konkrete,
  begruendete Zuordnung zur Ownerfreigabe vorgelegt (P21-M1, Abschnitt 13).
  Sie wird nicht still erfunden: jede migrierte Programmdatei bekommt genau
  diese nachvollziehbare Regel, und der Migrationstest prueft sie fuer alle
  vier `SensorPreference`-Werte einzeln.

#### 6.2.2 Validierung

`validateProgram` erhaelt:

- `InvalidEnumValue` fuer `defaults.product_sensor_failure.return_strategy`,
  falls kein gueltiger `ReturnStrategy`-Wert vorliegt;
- die Cross-Field-Regeln aus 6.13, u. a. `ReturnStrategy != RemainOnAirUntilEnd`
  bei `SensorPreference::AirOnly` wird abgelehnt.

`ValidationErrorCode` erhaelt einen neuen Wert `IncompatibleCombination` fuer
alle Cross-Field-Ablehnungen aus 6.13. Dieser Code ist nicht persistiert
(nur Laufzeit-/Testergebnis der Validierungsfunktion) und erweitert daher
kein Wireformat.

#### 6.2.3 Codec

`configuration_document_codec.cpp` erhaelt `returnStrategyToWireId`/
`returnStrategyFromWireId` nach demselben Muster wie
`productSensorFailurePolicyToWireId`. Schreiben/Lesen erfolgt innerhalb des
bestehenden `productSensorFailure`-Blocks (aktuell rund um Zeile 244-246 und
493-496), die Payload-Groessenberechnung (~Zeile 355) wird um ein festes
`uint8` erweitert (kein `optional`, da das Feld immer gesetzt ist).
`readProgramLimitsAndCompletion`/`readProgramIdentityAndFlags` bleiben nach
Schemaversion bedingt lesbar, analog zum bestehenden Muster fuer
`maximumProductWaitMinutes`.

#### 6.2.4 Werkskatalog, Beispielkonfiguration, Persistenz

- `standard_program_catalog.cpp`: alle vier Werksprogramme verwenden
  `ProductIfAvailableElseAir` oder `AirProductOptional` und erhalten
  `ReturnStrategy::AutomaticValidatedReturnToProduct` (Werkseinstellung fuer
  produktgefuehrte Standardprogramme, siehe `docs/TEMPERATURE_CONTROL.md`,
  Abschnitt "Rueckkehr des Produktfuehlers nach Ersatzbetrieb").
- `config/programs.example.yaml`: `schema_version: 6`, jeder
  `product_sensor_failure`-Block erhaelt `return_strategy:
  automatic_validated_return_to_product`; `allowed_values.return_strategy`
  wird als neue Liste ergaenzt (`remain_on_air_until_end`,
  `manual_return_to_product`, `automatic_validated_return_to_product`).
- Programmkatalog-/Konfigurationspersistenz (`configuration_documents.*`,
  `configuration_document_codec.*`) benoetigt keine strukturelle Aenderung
  ausser der oben beschriebenen Feld-/Wire-Erweiterung, da der Katalog
  bereits `ProgramDocument` verwendet.

### 6.3 Ausgabewert und Zustandsseparation

Der neue Dienst unter `lib/fermentation_app/src/sensor_selection.*` liefert
eine noch nicht angewendete Entscheidung mit mindestens:

```text
before.activeMode
after.activeMode
before/after.selectionPhase   (6.4)
controlSensor: Product | Air | None
peltierPermission: Allowed | Blocked
blockReason: None | ProductSensorUnusable | AirSensorUnusable |
             CoolingSensorUnusable | SimultaneousFixedSensorFailure |
             CrossRoleEvidenceIndeterminate | PolicyWait |
             UserActionRequired | SafeStateRequired |
             RestartRevalidationRequired | InvalidContext
event: optional SensorSelectionEvent (6.11)
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

### 6.4 Auswahlzustandsautomat

#### 6.4.1 Zustaende

```text
NormalProduct              - produktgefuehrt, Produkt VALID, keine Wartung faellig
NormalAir                  - luftgefuehrt (Programmwahl oder Strategie remain_on_air),
                              kein Rueckkehrpfad aktiv
ProductFailureDetected     - Produktausfall erkannt, Peltier sofort Blocked,
                              Wartezeit noch nicht abgelaufen
FallbackWaitPending         - Alias von ProductFailureDetected waehrend die
                              programmierte Wartezeit laeuft (siehe 6.4.3)
UserDecisionRequired       - Wartezeit abgelaufen und Policy = WaitForUser,
                              oder Strategie verlangt manuelle Fortsetzung
AirFallbackActive          - auf Luft gewechselt (FallbackToAirAfterTimeout
                              nach Ablauf, oder bestaetigte manuelle Aktion)
ReturnValidationPending    - automatic_validated_return_to_product prueft
                              Wiedereintrittsbedingungen (6.10)
SafeLocked                 - StopToSafeState, gleichzeitiger Ausfall von Air/
                              Cooling, oder unklare Cross-Role-Evidenz;
                              Peltier dauerhaft Blocked bis externe Aufloesung
RestartRevalidationPending - unmittelbar nach Wiederherstellung des Laufs
                              (6.12), bevor die erste Bewertung nach dem
                              Neustart abgeschlossen ist
```

`SafeLocked` ist kein Fehlerzustand im Sinne von #24; er ist der
Selektor-interne "kein Modus ist sicher waehlbar"-Zustand und bleibt fuer
#24 als Verriegelungs-/Fehlerkandidat sichtbar (siehe 6.9 im alten Plan,
jetzt 6.3).

#### 6.4.2 Zulaessige und verbotene Uebergaenge

```text
NormalProduct -> ProductFailureDetected      [Produkt nicht mehr Valid]
NormalProduct -> NormalProduct               [keine Aenderung, Re-Evaluation]

ProductFailureDetected -> NormalProduct      [Produkt wieder Valid VOR Ablauf
                                               der Wartezeit; siehe 6.4.4]
ProductFailureDetected -> UserDecisionRequired
                                              [Wartezeit abgelaufen, Policy
                                               WaitForUser]
ProductFailureDetected -> AirFallbackActive  [Wartezeit abgelaufen, Policy
                                               FallbackToAirAfterTimeout,
                                               Air+Cooling VALID]
ProductFailureDetected -> SafeLocked         [Policy StopToSafeState, oder
                                               Air/Cooling gleichzeitig
                                               ungueltig]

UserDecisionRequired -> AirFallbackActive    [explizite, validierte
                                               Benutzeraktion "mit Luft
                                               fortsetzen"]
UserDecisionRequired -> NormalProduct        [Produkt wieder Valid UND
                                               explizite Benutzerbestaetigung;
                                               kein automatischer Rueckfall
                                               ohne Aktion, da bereits eine
                                               Entscheidung angefordert wurde]
UserDecisionRequired -> SafeLocked           [Air/Cooling waehrenddessen
                                               ungueltig]

AirFallbackActive -> ReturnValidationPending [ReturnStrategy =
                                               AutomaticValidatedReturnToProduct
                                               UND Produkt wieder Valid]
AirFallbackActive -> NormalProduct           [ReturnStrategy =
                                               ManualReturnToProduct UND
                                               explizite validierte
                                               Benutzeraktion "zu Produkt
                                               zurueckkehren", nur wenn
                                               Produkt zu diesem Zeitpunkt
                                               Valid ist]
AirFallbackActive -> AirFallbackActive       [ReturnStrategy =
                                               RemainOnAirUntilEnd; jede
                                               Rueckkehranfrage wird
                                               abgelehnt]
AirFallbackActive -> SafeLocked              [Air/Cooling wird waehrenddessen
                                               ungueltig]

ReturnValidationPending -> NormalProduct     [6.10-Evidenz vollstaendig
                                               positiv]
ReturnValidationPending -> AirFallbackActive [Evidenz wird waehrend der
                                               Pruefung wieder unplausibel,
                                               unvollstaendig oder Produkt
                                               faellt erneut aus]
ReturnValidationPending -> SafeLocked        [Air/Cooling wird waehrenddessen
                                               ungueltig]

RestartRevalidationPending -> *              [siehe 6.12; niemals direkt
                                               nach NormalProduct/
                                               AirFallbackActive mit Allowed,
                                               erst nach vollstaendiger
                                               Neubewertung]

SafeLocked -> *                              [nur ueber denselben Pfad wie
                                               ein frischer Start dieser
                                               Bewertungskette; kein
                                               impliziter Austritt]
```

Explizit verboten:

- jeder Uebergang, der `activeMode` ohne ein zugehoeriges
  `SensorSelectionEvent` (6.11) aendert;
- ein Uebergang von `ProductFailureDetected`/`UserDecisionRequired` direkt
  nach `AirFallbackActive`, ohne dass die fuer die jeweilige Policy
  vorgesehene Bedingung (Wartezeit abgelaufen bzw. Benutzeraktion) erfuellt
  ist;
- jeder Uebergang nach `NormalProduct`/`AirFallbackActive` mit
  `peltierPermission = Allowed`, waehrend `RestartRevalidationPending`
  aktiv ist.

#### 6.4.3 Start der Fallback-Wartezeit

Die Wartezeit beginnt mit der monotonen Zeit der ersten Bewertung, in der
der Produkt-Snapshot als nicht nutzbar erkannt wird (`ProductFailureDetected`
wird betreten). Dieser Zeitpunkt wird **nur im RAM** als Teil des neuen,
nicht persistierten Auswahlzustands (6.12) gehalten - nicht in
`SensorQualitySnapshot` (das kennt keinen Fehlerbeginn) und nicht in
`RunPersistenceSnapshot` (das wuerde eine Schemaerweiterung erzwingen, siehe
6.12).

#### 6.4.4 Erneut gueltiger Produktwert waehrend der Wartezeit

Wird der Produkt-Snapshot waehrend `ProductFailureDetected` (vor Ablauf der
Wartezeit) wieder `Valid`, kehrt der Zustand direkt zu `NormalProduct`
zurueck. Es fand nie ein tatsaechlicher Moduswechsel statt (`activeMode`
blieb durchgehend `Product`), deshalb entsteht **kein**
`SensorSelectionEvent` und **keine** neue Laufrevision - nur eine interne
Diagnosemeldung ist zulaessig. Dies unterscheidet sich bewusst von einer
Rueckkehr aus `AirFallbackActive`, die immer einen tatsaechlichen Wechsel
war und immer protokolliert wird.

#### 6.4.5 Einmalige Verarbeitung manueller Aktionen

Jede Benutzeraktion (`UserDecisionRequired -> AirFallbackActive`,
`AirFallbackActive -> NormalProduct`) traegt dieselbe
`CommandEnvelope`-Idempotenzsicherung wie bestehende Kommandos
(`CommandId`, `expectedRunRevision`, `processedCommandIds`-Fenster aus
`RunCommandState`). Eine bereits verarbeitete Aktion liefert
`CommandStatus::AlreadyProcessed` und erzeugt keine zweite Bewertung.

#### 6.4.6 Idempotenz und doppelte Wechselereignisse

Eine wiederholte Bewertung ohne tatsaechliche Zustands- oder Modusaenderung
(z. B. wiederholter Timer-Tick waehrend `AirFallbackActive` ohne neue
Evidenz) liefert `after == before` mit `event = std::nullopt`. Ein
`SensorSelectionEvent` entsteht ausschliesslich beim tatsaechlichen
Verlassen eines Zustands mit unterschiedlichem `activeMode` oder beim
Eintritt in `SafeLocked` aus einem zuvor freigegebenen Zustand.

#### 6.4.7 Abbruch, Neustart und Wiederaufnahme der Rueckkehrvalidierung

- `ReturnValidationPending` wird abgebrochen (zurueck nach
  `AirFallbackActive`), sobald ein einzelnes Kriterium aus 6.10 nicht mehr
  erfuellt ist; der Abbruch selbst ist kein `SensorSelectionEvent` (kein
  `activeMode`-Wechsel), wird aber als Diagnosemeldung sichtbar.
- Eine neue `ReturnValidationPending`-Phase beginnt immer bei Evidenz Null;
  es gibt keinen Teilfortschritt ueber einen Abbruch hinweg. Das vermeidet
  eine zweite, parallele Fortschrittszaehlung neben `#20`s
  `recoveryProgressCount`, das ohnehin bei jedem ungueltigen Sample
  zurueckgesetzt wird.
- Nach einem Neustart beginnt jede Rueckkehrvalidierung zwingend bei Null
  (siehe 6.12) - unabhaengig vom Stand vor dem Neustart.

#### 6.4.8 Revisions- und Kapazitaetsgrenzen

- Ein tatsaechlicher Wechsel verbraucht dieselbe Laufrevisions-/
  Kapazitaetspruefung wie bestehende Kommandos
  (`requireRevisionCapacity`); bei erreichter Kapazitaet liefert die
  Entscheidung `CommandStatus::CapacityReached` und **keine**
  Modusaenderung - fail-closed, nicht fail-open.
- `SensorSelectionEvent`s werden nicht in einer eigenen unbegrenzten Liste
  gefuehrt; sie folgen den bestehenden begrenzten Meldungs-
  (`run_command_limits::kMaximumRuntimeMessages`) und
  Revisionsgrenzen (`kMaximumRunRevisions`, wird selbst NICHT fuer
  Sensorwechsel verwendet, siehe 6.11) des Laufsystems.

### 6.5 Vollstaendige Startmatrix

Ersetzt die vorlaeufige Tabelle der ersten Revision. Die Matrix ist
vierdimensional: `SensorPreference` (Programm) × angeforderter
`RunSensorMode` × Produktverfuegbarkeit/-qualitaet × Pflichtqualitaet von
Luft und Kuehlkoerpersensor. Da Luft/Kuehlkoerper fuer **jede** Peltierfreigabe
zwingend `VALID` sein muessen (6.6), wird die vierte Dimension als
Vorbedingung separat gefuehrt statt in jeder Zeile wiederholt.

Vorbedingung fuer jede Zeile: Air und Cooling sind zum Startzeitpunkt
`VALID`, sofern nicht anders vermerkt. Ist Air oder Cooling zum
Startzeitpunkt nicht `VALID`, ist **jeder** Start unabhaengig von
`SensorPreference` und angefordertem Modus mit `Peltier zunaechst gesperrt`
und Status `SafetyRejected`/`InvalidInput` abzulehnen (kein Sonderfall pro
Zeile).

| `SensorPreference` | angeforderter `RunSensorMode` | Produkt | gueltig/abgelehnt | effektiver Startmodus | Bestaetigung | Ereignis/Revision | Peltier initial |
|---|---|---|---|---|---|---|---|
| ProductIfAvailableElseAir | Product | Valid | gueltig | Product | Startzusammenfassung zeigt Product | keines (Erststart) | Allowed |
| ProductIfAvailableElseAir | Product | nicht Valid | gueltig, automatischer Ersatz | Air | Startzusammenfassung zeigt Air + Hinweis "Produkt angefordert, nicht verfuegbar" | ein `SensorSelectionEvent` (Ursache: StartFallback) | Allowed (auf Air) |
| ProductIfAvailableElseAir | Air | beliebig | gueltig | Air | Startzusammenfassung zeigt Air | keines | Allowed |
| AirProductOptional | Product | Valid | gueltig | Product | Startzusammenfassung zeigt Product | keines | Allowed |
| AirProductOptional | Product | nicht Valid | **abgelehnt** | - | `CommandStatus::InvalidInput`, keine stille Umdeutung auf Air | - | - |
| AirProductOptional | Air | beliebig | gueltig | Air | Startzusammenfassung zeigt Air | keines | Allowed |
| ProductRequired | Product | Valid | gueltig | Product | Startzusammenfassung zeigt Product | keines | Allowed |
| ProductRequired | Product | nicht Valid | **abgelehnt** | - | `CommandStatus::InvalidInput`, kein Start ohne Produkt | - | - |
| ProductRequired | Air | beliebig | **abgelehnt** | - | `CommandStatus::InvalidInput`, `AirOnly`-Anfrage widerspricht Programmpflicht | - | - |
| AirOnly | Product | beliebig | **abgelehnt** | - | `CommandStatus::InvalidInput`, Produktanfrage widerspricht `AirOnly` | - | - |
| AirOnly | Air | beliebig | gueltig | Air | Startzusammenfassung zeigt Air, Produkt bleibt reine Anzeige | keines | Allowed |

Zusaetzliche verbindliche Regeln:

- `StartSummary.sensorMode` aendert seine Bedeutung von "angefordert" zu
  "effektiv, bereits gegen Programmpraeferenz validiert". Das ist eine
  konsumentenwirksame Vertragsaenderung (nicht nur intern) und wird als
  eigener Testfall gefuehrt (9.2).
  Die effektive Modusableitung erfolgt **vor** der
  `NotConfirmed`-Rueckgabe in `decideProgramStart`, weil die
  Startzusammenfassung bereits fuer eine unbestaetigte, aber gueltige
  Anfrage zurueckgegeben wird (bestehendes Verhalten, siehe
  `run_commands.cpp` Kommentar bei `decision.startSummary`).
- Keine inkompatible Kombination darf still auf eine andere umgedeutet
  werden; jede Ablehnung liefert einen fuer die aufrufende Schicht
  auswertbaren `CommandStatus`.
- Ein automatischer Ersatz beim Start (Zeile 2) ist der einzige Fall, in dem
  ein Start selbst bereits ein `SensorSelectionEvent` erzeugt; die
  Laufrevision dafuer ist die ohnehin beim Start erzeugte erste Revision
  (`decision.after.runRevision`), keine zusaetzliche zweite Revision.

### 6.6 Produktfehler und Ersatzbetrieb

Im produktgefuehrten Lauf gilt:

```text
Produkt-Snapshot nicht nutzbar
  -> Peltierfreigabe sofort sperren
  -> Air und Cooling pruefen
  -> sichtbares Warn-/Ereignisobjekt erzeugen
  -> programmabhaengige Wartezeit abwarten (Start: 6.4.3)
  -> Strategie pruefen
  -> nur bei gueltigen festen Sensoren auf Air wechseln
```

Verbindliche Regeln:

- `Air` und `Cooling` muessen fuer jede Peltierfreigabe aktuell und `VALID`
  sein; ein gleichzeitiger Ausfall verhindert jeden Ersatzbetrieb
  (entschieden, kein Owner-Gate - siehe 6.13, Regel 4).
- Ein ungueltiger Produktwert allein macht den Luftsensor nicht automatisch
  zum Ersatzsensor. Erst die komplette Entscheidung mit Wartezeit,
  Policy und Sicherheitsbedingungen darf den Modus wechseln.
- `FallbackToAirAfterTimeout` wechselt automatisch nach der programmierten
  Wartezeit, nie frueher und nicht aus einem einzelnen wieder gueltigen
  Produktwert (siehe 6.4.4 fuer den Fall, dass Produkt vor Ablauf wieder
  gueltig wird).
- `WaitForUser` bleibt nach der Wartezeit ohne Luftwechsel, bis eine
  explizite, validierte Benutzerentscheidung vorliegt.
- `StopToSafeState` erzeugt keine Regelmodusfreigabe und bleibt fuer #24 als
  verriegelungs-/Fehlerkandidat sichtbar.
- Ist der Luft- oder Kuehlkoerpersensor nicht sicher gueltig, wird nicht auf
  Luft umgeschaltet. `peltierPermission` bleibt `Blocked`.
- Die Wartezeit ist ein konfigurierter Programwert; der reale Wert bleibt
  `TBD_COMMISSIONING`. Firmwarefeste Obergrenzen und eine fehlende/ungueltige
  Konfiguration fuehren fail-closed zu einer gesperrten Entscheidung.

### 6.7 Rueckkehr zum Produktfuehler

Eine Rueckkehr ist nur erlaubt, wenn:

- die konfigurierte `ReturnStrategy` (6.2) die Rueckkehr erlaubt;
- der Produkt-Snapshot `VALID` ist und einen verwendbaren Filterwert besitzt;
- #20s Wiedererkennungsregeln bereits mehrere plausible Proben und die
  geforderte Stabilitaet belegen;
- der begrenzte `CrossRolePlausibilityContext` (6.10) keine widerspruechliche
  Evidenz liefert;
- Air und Cooling weiterhin gueltig sind;
- keine ungeklaerte konkurrierende Safety-/Fehlerlage besteht.

Ein einzelner gueltiger Messwert reicht nie. Ein grosser Unterschied zwischen
Produkt und Luft beweist allein keinen Fehler; der Selektor darf eine
kurzzeitige thermische Traegheit nicht als Ausfall klassifizieren. Bei
unklarer rollenuebergreifender Evidenz bleibt er konservativ bei `Blocked`
beziehungsweise im Luftmodus und liefert einen Verdachts-/Diagnosestatus,
keine unbelegte Schuldzuweisung.

Die drei Strategien (jetzt Programmschemafeld, 6.2):

```text
remain_on_air_until_end
  -> aktive Luftregelung bleibt bis zum Laufende erhalten; jede
     Rueckkehranfrage wird abgelehnt (Zustand bleibt AirFallbackActive)

manual_return_to_product
  -> nur explizite Benutzeraktion nach vollstaendiger Validierung erlaubt
     (Uebergang AirFallbackActive -> NormalProduct in 6.4.2)

automatic_validated_return_to_product
  -> nach #20-VALID, Stabilitaet und 6.10-Plausibilitaet automatisch auf
     Produkt wechseln (ueber ReturnValidationPending, 6.4.2)
```

Der Wechsel zurueck wird genauso sichtbar, revidiert und protokolliert wie
der Fallback (6.11). Ein Quittieren einer Meldung ist keine
Rueckkehrfreigabe.

### 6.8 Produktgefuehrte manuelle Laeufe

`ManualRunPlanRequest` erlaubt `RunSensorMode::Product`, ist aber nicht an
ein `ProgramDefinition` gebunden und besitzt daher keine Policy, keine
Wartezeit und keine Rueckkehrstrategie. Diese Revision entscheidet das
explizit (bisher offen als P21-Punkt der ersten Revision):

**Entscheidung fuer Release 1: produktgefuehrte manuelle Laeufe werden
unterstuetzt, mit einem festen, nicht konfigurierbaren Vertrag** (KISS-/
Safety-Variante, wie vom Review empfohlen):

```text
Manueller Produktmodus (RunSensorMode::Product bei ManualStartRequest):
- Produktfehler sperrt die Peltierfreigabe sofort (wie 6.6);
- kein automatischer Luftfallback: die programmabhaengige Wartezeit
  entfaellt, weil kein Programm mit konfigurierbarer Wartezeit existiert;
  Verhalten ist fest wie ProductSensorFailurePolicy::WaitForUser;
- Fortsetzung mit Luft nur durch explizite, validierte Benutzeraktion
  (Uebergang UserDecisionRequired -> AirFallbackActive, 6.4.2), ohne
  vorherige Wartezeitphase;
- Rueckkehr zum Produkt nur manuell nach vollstaendiger Validierung; fest
  wie ReturnStrategy::ManualReturnToProduct.
```

Diese Konstanten werden **nicht** in `ManualRunPlanRequest` als neue Felder
eingefuehrt: `ManualRunPlan` wird unveraendert in
`RunPersistenceSnapshot::manual` persistiert (`writeManual`/`readManual`,
`run_persistence_codec.cpp`), und neue Felder dort waeren eine
Wireformataenderung mit Migrationsbedarf fuer eine Fachentscheidung, die
firmwareweit konstant ist. Stattdessen behandelt der Selektor
`RunSensorMode::Product` bei einem manuellen Lauf (erkennbar an
`RunCommandState::activeManualRun.has_value()`) als impliziten,
fest verdrahteten Policy-/Strategie-Kontext - keine zweite Konfigurationsquelle,
kein Wireformat-Zuwachs.

Wird diese Entscheidung vom Owner abgelehnt, ist die Ersatzentscheidung
"Produktmodus fuer manuelle Laeufe bis zu einem eigenen Vertrag ablehnen"
(d. h. `validateManualRunPlan` lehnt `RunSensorMode::Product` grundsaetzlich
ab). Beide Varianten sind mit dem in 6.4-6.7 beschriebenen Automaten
kompatibel; die Auswahl ist P21-M3 (Abschnitt 13).

### 6.9 Phasen, Kuehlen und Halten

Der Wechsel von `FERMENTING` nach `COOLING` oder `COOL_HOLDING` loest keine
automatische Neuauswahl aus. Der wirksame Regelsensor bleibt erhalten:

```text
Produktmodus -> Produktmodus
Luftmodus    -> Luftmodus
```

Ein Sensorfehler waehrend `COOLING` oder `COOL_HOLDING` durchlaeuft jedoch
denselben Sicherheits- und Policyvertrag wie in `FERMENTING`. Dies ist
bereits durch `docs/TEMPERATURE_CONTROL.md` ("Regelsensor beim Kuehlen und
Halten") und `docs/RECOVERY_AND_INTERRUPTION.md` (phasenbezogene
Wiederaufnahme fuer `COOLING`/`COOL_HOLDING`) entschieden und wird **nicht**
laenger als Owner-Gate gefuehrt (siehe 6.13-Bereinigung).

### 6.10 Rollenuebergreifende Plausibilitaetspruefung

`CrossRolePlausibilityContext` ist ein kleiner, begrenzter, rein aus bereits
vorhandenen Daten abgeleiteter Wert - keine zweite Sensorqualitaetsmaschine
und keine unbegrenzte Historie:

```cpp
struct CrossRolePlausibilityContext {
    ProcessState phase;
    std::uint64_t evaluationMonotonicMillis;
    SensorQualitySnapshot air;
    SensorQualitySnapshot product;
    SensorQualitySnapshot cooling;
    // air/product je: filteredCelsius, lastValidSampleAgeMs,
    // changeRateCelsiusPerSecond, recoveryProgressCount - bereits in
    // SensorQualitySnapshot vorhanden, keine Duplizierung.
};
```

In #21 verfuegbar und zu beruecksichtigen:

- Prozessphase (`ProcessState`, bereits im Laufkontext vorhanden);
- Momentanwerte, Alter und Trend aller drei Sensorrollen
  (`filteredCelsius`, `lastValidSampleAgeMs`,
  `changeRateCelsiusPerSecond` aus `SensorQualitySnapshot`);
- Wiedererkennungsfortschritt (`recoveryProgressCount` aus #20).

In #21 **nicht** verfuegbar und ausdruecklich als noch nicht vorhanden
abgegrenzt (Abhaengigkeit von #22/#23):

- aktuelle abstrakte Heiz-/Kuehlrichtung (existiert erst mit der PI-Regelung
  aus #22/#23);
- Zeit seit Beginn der aktuellen Regelanforderung (dieselbe Abhaengigkeit);
- bekannte thermische Totzeit beziehungsweise Commissioning-Profil (folgt
  erst nach realer Vermessung, `TBD_COMMISSIONING`/`TBD_HARDWARE`).

**Aufloesung des daraus entstehenden Zielkonflikts** (Werksdefault ist
`automatic_validated_return_to_product`, aber die vollstaendige Evidenzliste
des Fachvertrags ist erst mit #22/#23 komplett verfuegbar): die automatische
Rueckkehr in #21 validiert ausschliesslich auf der in #21 tatsaechlich
verfuegbaren Teilmenge -

```text
Produkt VALID
UND recoveryProgressCount >= konfigurierte Mindestanzahl (TBD_COMMISSIONING)
UND Produkt bleibt fuer die konfigurierte Stabilitaetszeit VALID
    (TBD_COMMISSIONING)
UND |product.filteredCelsius - air.filteredCelsius| innerhalb der
    konfigurierten Plausibilitaetsgrenze (TBD_COMMISSIONING)
UND kein gleichzeitiger Air-/Cooling-Ausfall
```

- waehrend Richtung, Regelanforderungsdauer und Totzeit als **in #21 nicht
  pruefbare** Kriterien explizit ausgeklammert bleiben und erst in einer
  spaeteren Erweiterung (#22/#23) ergaenzt werden koennen, ohne den Vertrag
  hier zu brechen. Diese Aufloesung selbst ist P21-M4 (Abschnitt 13) zur
  Ownerbestaetigung, weil sie den Umfang der "vollstaendigen Validierung" in
  `docs/TEMPERATURE_CONTROL.md` fuer Release 1 fachlich einschraenkt.
- Ist eine der oben verfuegbaren Bedingungen nicht erfuellbar (z. B. weil
  #20 keine ausreichende `recoveryProgressCount` liefert), bleibt die
  automatische Rueckkehr gesperrt (`ReturnValidationPending ->
  AirFallbackActive`, 6.4.7). Es wird keine unbelegte Rueckkehr erzwungen.

### 6.11 Ereignis-, Meldungs- und Revisionsvertrag

Jeder tatsaechliche Wechsel von `Product` nach `Air` oder von `Air` nach
`Product` erzeugt einen strukturierten `SensorSelectionEvent` mit:

- Lauf-ID und Laufrevision (`RunCommandState::runRevision`, der generische
  Zaehler - **nicht** ein Eintrag in `RunRevision`/`revisions[]`, siehe
  Begruendung unten);
- monotone Zeit und, falls vorhanden, UTC-Anker;
- vorherigem und neuem Modus;
- Ursache: `StartFallback` (6.5), `ProductFailureFallback`,
  `AutomaticValidatedReturn`, `ManualUserFallback`, `ManualUserReturn`;
- angewandter Policy/Strategie und kurzer Diagnosegrund;
- Sensorqualitaet/Alter der drei Rollen als Evidenzreferenz, nicht als
  scheinbar aktueller Wert bei `FAILED`;
- Konfidenz-/Plausibilitaetsstatus aus 6.10.

**Bewusst kein neuer Wert in `RunChangeReason`/`RunRevision`:** diese
Struktur ist fuer Zieltemperatur-/Restdauer-Anpassungen gebaut
(`EffectiveRunValues before/after` kennt keinen Sensormodus). Eine
Wiederverwendung wuerde entweder ein fachfremdes optionales Feld in
`EffectiveRunValues` erzwingen oder Sensordaten in ein Feld pressen, das
dafuer nicht vorgesehen ist - beides eine SRP-Verletzung. `event` ist ein
eigener, schmaler Typ, der ueber die bestehende `runRevision`-Zahl
(nicht die `revisions[]`-Historie) an den Lauf gebunden wird, genau wie
`RuntimeMessage` bereits eine `runRevision`-Referenz traegt statt selbst in
`revisions[]` zu leben.

Die bestehende runtime-seitige Meldungsstruktur wird um einen klaren
Sensorwechselcode beziehungsweise eine gleichwertige strukturierte
Ereignisreferenz ergaenzt (`MessageCode`, `run_commands.hpp`). Die
Entscheidung darf eine `IEventJournal`-Aufzeichnung nicht als Voraussetzung
fuer die fachliche Berechnung machen; ein fehlgeschlagenes Komfort-/
Journal-Write darf keine Aktorfreigabe erzeugen. Die dauerhafte
Aufbewahrung und Exportform des Journals bleibt im Scope von #19. #21 muss
jedoch die Ereignisdaten so liefern, dass #19 sie ohne Parallelvertrag
persistieren kann.

### 6.12 Atomare Laufwirkung und Neustart

Der bestehende `activeRunSensorMode` wird als kanonischer aktiver Modus
weiterverwendet. Ein angewendeter Moduswechsel:

1. wird gegen erwartete Lauf-/Zustandsrevision geprueft;
2. wird zusammen mit neuer Laufrevision und Ereignisreferenz atomar
   vorbereitet und gespeichert;
3. wird erst nach bestaetigtem Commit im RAM angewendet;
4. wird danach fuer Regel- und Safetyentscheidungen verwendet.

**Restart-Vertrag (loest P21-03 der ersten Revision konkret auf):**

Der Auswahlzustand aus 6.4 (`selectionPhase`, Wartezeit-Startzeitpunkt,
Rueckkehrvalidierungsfortschritt) wird **nicht** in `RunPersistenceSnapshot`
persistiert und **kein** neues Feld dort eingefuehrt - `kRunPersistenceSchema`
bleibt `1`. Begruendung: `RunPersistenceSnapshot` hat keinen
Migrationsmechanismus (jede Versionsabweichung wird hart abgelehnt); eine
Erweiterung waere eine materielle, folgenreiche Abweichung fuer eine reine
Restart-Komfortfunktion.

Stattdessen gilt der bereits mit dem Recovery-Vertrag konsistente
fail-closed Ablauf:

```text
Peltier gesperrt
-> Auswahlzustand wird beim Restore IMMER auf RestartRevalidationPending
   gesetzt (RAM-Default, unabhaengig vom vor dem Neustart erreichten
   Zustand)
-> keine unbekannte Wartezeit anrechnen: eine erneut erkannte
   Produktnichtnutzbarkeit startet die Wartezeit bei 0 (6.4.3)
-> vollstaendige Sensor- und Rueckkehrvalidierung erneut durchfuehren
   (ReturnValidationPending beginnt immer bei Evidenz Null, 6.4.7)
-> erst nach atomar persistierter Entscheidung freigeben
```

**Ungeloeste Mehrdeutigkeit und ihre Aufloesung:** `activeRunSensorMode ==
Air` allein sagt nach einem Neustart nicht, ob Air die urspruengliche
Programmwahl war (z. B. `AirProductOptional` mit angefordertem Air) oder
das Ergebnis eines Produktausfall-Fallbacks (das ggf. eine automatische
Rueckkehr erlauben wuerde). Diese Information existierte nur im nicht
persistierten `selectionPhase`-RAM-Zustand vor dem Neustart und ist danach
nicht mehr rekonstruierbar, ohne eines der beiden Felder zu persistieren.

Empfohlene Regel (P21-M2, Abschnitt 13):

```text
Nach Neustart: activeRunSensorMode == Air UND Ursprung unbekannt
  -> automatische und manuelle Rueckkehr fuer den Rest dieses Laufs
     gesperrt (wirkt wie remain_on_air_until_end), unabhaengig von der
     konfigurierten ReturnStrategy.
  -> Diagnosemeldung "Rueckkehrpruefung nach Neustart nicht moeglich"
     sichtbar, kein Fehlerzustand.
```

Diese Regel ist bewusst konservativ (kein falscher automatischer Wechsel
moeglich) auf Kosten von Komfort (ein Stromausfall waehrend eines
Fallbacks verhindert danach die automatische Rueckkehr fuer den Rest des
Laufs). Die Alternative - ein dediziertes persistiertes
"Fallback-Ursprung"-Feld mit `kRunPersistenceSchema`-Bump - wurde verworfen,
weil sie eine deutlich groessere, fuer Release 1 nicht notwendige
Persistenzaenderung waere (KISS). Direkte Konsequenz: `activeMode ==
Product` nach Neustart ist **nicht** mehrdeutig (Produkt ist Produkt,
unabhaengig vom Ursprung) und durchlaeuft normal `ProductFailureDetected`,
falls der Produktwert beim ersten Restart-Check nicht `VALID` ist.

`restoreRunPersistenceSnapshot` liefert weiterhin ein `RunCommandState`; der
neue RAM-only `selectionPhase`/Wartezeit-Zustand wird dort **nicht**
gesetzt (Default-Initialisierung), was genau den obigen
`RestartRevalidationPending`-Einstieg erzwingt. Ein Test bestaetigt: Restore
liefert `selectionPhase == RestartRevalidationPending` und
`peltierPermission == Blocked`, unabhaengig vom gespeicherten
`activeRunSensorMode`.

Direkte GPIO- oder alte Aktorzustaende werden nicht gespeichert. Ein
Neustart setzt keine Produktfehlerquittierung zurueck und darf aus einem
alten Modus keine blinde Aktorfreigabe ableiten.

### 6.13 Zentrale Cross-Field-Validierung

`validateProgram` (bzw. eine neue, von dort aufgerufene Funktion in
`program_model.cpp`, um die Funktion nicht unuebersichtlich wachsen zu
lassen) prueft zusaetzlich zu den bestehenden Einzelfeldregeln folgende
Kombinationen aus `SensorPreference`, `ProductSensorFailurePolicy`,
`ReturnStrategy` und `fallbackDelaySeconds`. Jeder Verstoss erzeugt
`ValidationErrorCode::IncompatibleCombination`:

1. `SensorPreference::ProductRequired` + `ProductSensorFailurePolicy::
   FallbackToAirAfterTimeout` - ein Luftfallback ist bei zwingend
   erforderlichem Produkt nicht zulaessig (uebernimmt P21-02 der ersten
   Revision als entschiedene Regel, kein Owner-Gate mehr).
2. `SensorPreference::AirOnly` + `ReturnStrategy != RemainOnAirUntilEnd` -
   eine Rueckkehr zu einem in diesem Modus nie aktiven Produktsensor ist
   sinnlos und wird abgelehnt, nicht still ignoriert.
3. `SensorPreference::AirOnly` + `ProductSensorFailurePolicy !=
   FallbackToAirAfterTimeout` mit gesetzter `fallbackDelaySeconds` - keine
   Fallback-Konfiguration ohne Wirkung; `AirOnly`-Programme duerfen nur die
   policy-neutrale Kombination fuehren, die von der Katalog-/Migrationsregel
   in 6.2.1 erzeugt wird.
4. `ProductSensorFailurePolicy::FallbackToAirAfterTimeout` ohne gueltige
   `fallbackDelaySeconds` bei `ValidationPurpose::Runnable` - bereits
   bestehende Regel (`program_model.cpp`, Zeilen ~209-216), hier nur
   referenziert, nicht dupliziert.
5. Jeder `ReturnStrategy`- oder `ProductSensorFailurePolicy`-Wert, der nur
   mit einem erfundenen `TBD_COMMISSIONING`-Ersatzwert lauffaehig waere -
   bereits durch `MissingCommissioningValue` bei `ValidationPurpose::
   Runnable` abgedeckt, hier nur referenziert.

Diese Validierung gehoert in die bestehende kanonische
Modell-/Konfigurationskette (`program_model.cpp`, aufgerufen aus
`configuration_documents.cpp`/`configuration_document_codec.cpp` und aus
`decideProgramStart` ueber die bereits bestehende
`ActiveRun::start`-Kette) und entsteht nicht als Parallelvertrag im
Laufselektor. Der Laufselektor (`sensor_selection.*`) **liest** nur ein
bereits valides `ProgramDefinition` und dupliziert keine dieser Regeln.

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
`ReturnStrategy`, `FERMENTING` oder `RunSensorMode`. Der Selektor bleibt in
`fermentation_app`, weil er konkrete Fermentationsrollen, Programme und
Prozessphasen kennt. Er verwendet keine Arduino-, ESP-IDF-, Datei-, Netzwerk-
oder Echtzeit-API.

Die Programmschema-, Codec-, Katalog- und Persistenzaenderungen aus 6.2 und
6.12 bleiben ebenfalls vollstaendig innerhalb `fermentation_app`; keine
Grenze aus ADR-013 wird beruehrt.

Die Composition Roots bleiben unveraendert, solange keine reale
Sensorverkabelung existiert. Eine spaetere Laufzeitintegration ruft den
Selektor ueber schmale Anwendungsports und deterministische monotone Zeit auf;
sie darf keine Logik in `src/main.cpp` oder `main/app_main.cpp` verschieben.

## 8. Voraussichtlicher Datei- und Commit-Schnitt

Die genaue Liste wird nach Planfreigabe nur innerhalb dieses Scopes
umgesetzt. Gegenueber der ersten Revision kommt ein eigener Commit fuer die
Programmschema-Erweiterung hinzu, weil sie fachlich und testtechnisch
unabhaengig vom Auswahlkern ist und breiten, aber mechanischen Diff
verursacht (siehe 11).

### Commit 1 - Programmschema: Rueckkehrstrategie (6.2, 6.13)

Voraussichtliche Dateien:

- `lib/fermentation_app/src/program_model.hpp` (neues `ReturnStrategy`-Enum,
  `ProgramField::ReturnStrategy`, `kSchema5RequiredProgramFields`-Umbenennung,
  neues `kCurrentRequiredProgramFields`, `kCurrentProgramSchemaVersion` 6,
  `kMigratableProgramSchemaVersion` 5, neuer
  `ValidationErrorCode::IncompatibleCombination`);
- `lib/fermentation_app/src/program_model.cpp` (`kRequiredFields`
  17. Eintrag, `validReturnStrategy`, Cross-Field-Regeln aus 6.13,
  Migrationsschritt 5 -> 6 mit der Abbildung aus 6.2.1);
- `lib/fermentation_app/src/configuration_document_codec.cpp` (Wire-ID-Paar
  fuer `ReturnStrategy`, Lese-/Schreibpfad im `productSensorFailure`-Block,
  Payload-Groessenberechnung);
- `lib/fermentation_app/src/standard_program_catalog.cpp` (alle vier
  Werksprogramme: `ReturnStrategy::AutomaticValidatedReturnToProduct`);
- `config/programs.example.yaml` (`schema_version: 6`, `return_strategy` je
  Programm, `allowed_values.return_strategy`);
- `test/test_program_models/test_program_models.cpp` (Validierung, Migration,
  Cross-Field-Regeln);
- `test/test_configuration_codecs/test_configuration_codecs.cpp` (Codec
  Round-Trip inkl. Schema-4-Ablehnung, Schema-5-Migration);
- `test/test_configuration_migration/test_configuration_migration.cpp`
  (Migrationsmatrix fuer alle vier `SensorPreference`-Werte);
- ggf. mechanische Folgeanpassungen an bestehenden Testfixtures, die
  `ProgramDocument`/`ProgramDefinition` mit expliziter `presentFields`-Maske
  von Hand bauen (der `static_assert` in `program_model.cpp` deckt
  vergessene `kRequiredFields`-Eintraege ab, nicht aber Testfixtures
  ausserhalb dieser Datei - diese werden einzeln durchsucht und angepasst).

### Commit 2 - Auswahlkern und direkte native Unit-Tests (6.1, 6.3, 6.4, 6.6, 6.7, 6.10)

Voraussichtliche Dateien:

- neu: `lib/fermentation_app/src/sensor_selection.hpp`;
- neu: `lib/fermentation_app/src/sensor_selection.cpp`;
- neu: `lib/fermentation_app/src/sensor_selection_limits.hpp`, nur falls
  fuer firmwarefeste Validierungsobergrenzen erforderlich;
- neu: `test/test_sensor_selection/test_sensor_selection.cpp`;
- gegebenenfalls `lib/fermentation_app/CMakeLists.txt` nur fuer eine
  notwendige, bestehende Modulregistrierung.

Inhalt: Werttypen, reine Entscheidung, vollstaendiger Zustandsautomat (6.4),
Start-/Fallback-/Rueckkehrmatrix, `CrossRolePlausibilityContext` (6.10),
Sicherheitsvoraussetzungen, Zeitvergleich, Rollenvergleich und
fehlgeschlagene/unklare Eingaben. Keine Persistenz und keine Aktoradapter.

### Commit 3 - Lauf-, Meldungs- und Persistenzanschluss (6.5, 6.8, 6.11, 6.12)

Voraussichtliche Dateien:

- `lib/fermentation_app/src/run_commands.hpp/.cpp`: Startmatrix-Pruefung in
  `decideProgramStart` (vor der `NotConfirmed`-Rueckgabe, siehe 6.5),
  `RunSensorMode::Product`-Vertrag fuer manuelle Laeufe (6.8), schmale
  Sensorwechselereignisse (`SensorSelectionEvent`, `MessageCode`-Erweiterung),
  sichere manuelle Aktionen und atomare Laufrevisionsvorbereitung, RAM-only
  `selectionPhase`/Wartezeit-Zustand in `RunCommandState` (nicht persistiert,
  siehe 6.12);
- `lib/fermentation_app/src/run_persistence_contract.cpp` (kein neues
  persistiertes Feld; `restoreRunPersistenceSnapshot` liefert weiterhin nur
  `activeRunSensorMode`, RAM-only Auswahlzustand bleibt beim Restore auf
  Default = `RestartRevalidationPending`);
- direkt betroffene Tests unter `test/test_run_commands/`,
  `test/test_run_checkpoint_codec/` und
  `test/test_run_persistence_coordinator/`.

`kRunPersistenceSchema` bleibt `1` (6.12). Die bestehende
Schema-1-Kompatibilitaet, der unveraenderliche Programmschnappschuss, die
begrenzten Revisions-/Nachrichtenpuffer und die fail-closed
Persistenzordnung bleiben erhalten.

### Commit 4 - fachliche Dokumentation und Abschlussnachweise

Voraussichtliche Dokumentationsdateien:

- `docs/TEMPERATURE_CONTROL.md` fuer die konkret implementierte
  Auswahl-/Fallback-/Rueckkehrauslegung und das Programmschemafeld;
- `docs/RECOVERY_AND_INTERRUPTION.md` fuer den Restart-Vertrag aus 6.12;
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

### 9.1 Programmschema, Migration, Codec (Commit 1)

- neues `ReturnStrategy`-Feld ist Pflichtfeld ab Schema 6; fehlend ->
  `MissingRequiredField`;
- ungueltiger `ReturnStrategy`-Wert -> `InvalidEnumValue`;
- alle vier Regeln aus 6.13 einzeln als ablehnender Testfall
  (`IncompatibleCombination`);
- Migration Schema 5 -> 6 fuer alle vier `SensorPreference`-Werte: drei
  ergeben `AutomaticValidatedReturnToProduct`, `AirOnly` ergibt
  `RemainOnAirUntilEnd`;
- Migration von Schema 4 direkt (kein Zwischenschritt) ->
  `UnsupportedSourceVersion`, kein stilles Mehrschritt-Upgrade;
- Codec-Round-Trip fuer alle drei `ReturnStrategy`-Werte;
- Payload-Groessenberechnung bleibt fuer alle vier Werksprogramme innerhalb
  der bestehenden Kapazitaetsgrenzen;
- `config/programs.example.yaml` referenziert Schema 6 und validiert gegen
  die aktuelle Feldmaske (sofern ein bestehender Test die Beispieldatei
  bereits einliest; andernfalls wird dieser Punkt als Dokumentationsnachweis
  statt Testzeile gefuehrt).

### 9.2 Unit-Tests des Auswahlkerns (Commit 2)

- jede Zeile der Startmatrix (6.5) einzeln, inklusive aller vier
  Ablehnungsfaelle;
- Produkt `STALE` und `FAILED` sperren Peltier sofort;
- Produktfehler vor Ablauf der Wartezeit bleibt ohne Luftwechsel;
- Produkt wird waehrend der Wartezeit erneut `Valid` -> Rueckkehr zu
  `NormalProduct` ohne Ereignis/Revision (6.4.4);
- `FallbackToAirAfterTimeout` wechselt erst nach der exakten monotone Zeit;
- `WaitForUser` wartet auch nach Timeout auf eine ausdrueckliche Aktion;
- `StopToSafeState` liefert keine Luftfreigabe;
- ungueltige Luft sperrt Ersatzbetrieb; ungueltiger Kuehlkoerpersensor
  sperrt jede Peltierfreigabe; gleichzeitiger Ausfall von Luft und
  Kuehlkoerper liefert keinen Ersatzmodus;
- Produkt allein darf eine feste Sicherheitsrolle nie ersetzen;
- `remain_on_air_until_end` verhindert jede automatische UND jede manuelle
  Rueckkehranfrage;
- `manual_return_to_product` lehnt fehlende, unbestaetigte oder ungueltige
  Benutzeraktionen ab;
- automatische Rueckkehr wird mit nur einem gueltigen Messwert abgelehnt;
- automatische Rueckkehr wird nach den in 6.10 definierten verfuegbaren
  Kriterien zugelassen;
- automatische Rueckkehr bleibt gesperrt, wenn eines der 6.10-Kriterien
  fehlt (z. B. `recoveryProgressCount` unter Mindestwert);
- unplausible Produkt-/Luftdifferenz oder unklare Evidenz verhindert die
  Rueckkehr ohne unbelegte Schuldzuweisung;
- Rueckkehrvalidierung wird abgebrochen und beginnt bei erneutem Versuch bei
  Evidenz Null (6.4.7);
- produktgefuehrter manueller Lauf: Produktfehler sperrt sofort, kein
  automatischer Luftfallback, manuelle Fortsetzung mit Luft moeglich,
  Rueckkehr nur manuell (6.8);
- Wechsel erzeugt genau ein `SensorSelectionEvent` mit altem/neuem Modus,
  Ursache und Laufrevision;
- wiederholte Bewertung ohne neue Modusaenderung erzeugt kein doppeltes
  Wechselereignis (6.4.6);
- `COOLING` und `COOL_HOLDING` aendern den Modus nicht allein durch den
  Phasenwechsel; Sensorfehler dort verwenden denselben Vertrag;
- Restart: `restoreRunPersistenceSnapshot` liefert
  `selectionPhase == RestartRevalidationPending`,
  `peltierPermission == Blocked`, unabhaengig vom gespeicherten
  `activeRunSensorMode` (6.12);
- Restart mit `activeRunSensorMode == Air`: automatische UND manuelle
  Rueckkehr bleiben fuer den Rest des Laufs gesperrt (6.12);
- Restart mit `activeRunSensorMode == Product` und aktuell ungueltigem
  Produkt: Wartezeit beginnt bei 0, kein Wartezeitguthaben aus der Zeit vor
  dem Neustart;
- Kapazitaetsgrenze erreicht -> `CapacityReached`, keine Modusaenderung
  (6.4.8);
- ungueltige Rolle, Enum-, Lauf- oder Snapshotdaten erzeugen keine
  Teilwirkung.

### 9.3 Konsumenten- und Laufvertragstests (Commit 3)

- `decideProgramStart` lehnt jede in 6.5 als ungueltig markierte Kombination
  mit `CommandStatus::InvalidInput` ab, auch fuer eine unbestaetigte
  Anfrage (Startzusammenfassung wird trotzdem korrekt/effektiv gefuellt,
  sofern die Kombination gueltig ist);
- `StartSummary.sensorMode` zeigt den effektiven, nicht den angeforderten
  Modus (Regressionstest fuer die Vertragsaenderung aus 6.5);
- akzeptierte Moduswechsel aktualisieren `activeRunSensorMode` und
  Laufrevision nur atomar;
- abgelehnte oder nicht gespeicherte Entscheidungen veraendern weder RAM
  noch Aktoranforderung;
- der bestehende Programmschnappschuss bleibt bei jedem Wechsel unveraendert;
- Laufpersistenz round-tript den aktiven Modus und die neue Revision;
- ein Laufcheckpoint mit eingebettetem Schema-5-Programm wird beim Restore
  transparent auf Schema 6 migriert (Nutzung des bestehenden
  `readProgram`-Migrationspfads, kein neuer Code in
  `run_persistence_codec.cpp` noetig ausser dem Versionsbump);
- ein Laufcheckpoint mit eingebettetem Schema-4-Programm wird beim Restore
  eindeutig abgelehnt (`InvalidWireValue`/`MigrationFailed`), nicht
  stillschweigend falsch interpretiert;
- Sensorwechsel-/Warnmeldungen bleiben begrenzt, quittierbar und getrennt
  vom Fehlerreset;
- Journal-/Meldungsausfall darf keine neue Peltierfreigabe erzeugen;
- produktgefuehrter manueller Lauf durchlaeuft `decideManualStart` mit dem
  festen Vertrag aus 6.8, ohne neue Felder in `ManualRunPlanRequest`.

### 9.4 Gezielte Ausfuehrung nach Freigabe

Mindestens direkt betroffen:

```bash
pio test -e native --filter test_program_models
pio test -e native --filter test_configuration_codecs
pio test -e native --filter test_configuration_migration
pio test -e native --filter test_sensor_selection
pio test -e native --filter test_run_commands
pio test -e native --filter test_run_checkpoint_codec
pio test -e native --filter test_run_persistence_coordinator
python scripts/check_architecture_boundaries.py
python scripts/check_secrets.py
git diff --check
```

Der exakte Filter wird an die tatsaechlich geaenderten Tests angepasst, und
um jede Testdatei ergaenzt, die wegen der breiteren Feldmaske (Commit 1)
mechanisch angepasst werden musste. Nur ausgefuehrte Befehle werden im PR
als Nachweis genannt. Vollstaendige native und ESP-IDF-Laeufe sind in dieser
Umsetzungsphase nicht doppelt lokal auszufuehren; sie bleiben Owner-/
Remote-CI-Gate gemaess `docs/CI_AND_QUALITY_GATES.md`.

## 10. Safety-, Security-, Recovery- und Hardwaregrenzen

- Bei Boot, Reset, `STALE`, `FAILED`, unklarer Auswahl, ungueltigen festen
  Sensoren, fehlendem Persistenzcommit, unklarer Rollenplausibilitaet oder
  unbekanntem Fallback-Ursprung nach Neustart (6.12) wird keine neue
  Peltierfreigabe erzeugt und keine automatische/manuelle Rueckkehr
  zugelassen.
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
  Stabilitaetsdauer, Differenz-/Trendgrenzen, Mindest-`recoveryProgressCount`
  und thermischer Kompatibilitaet offen. Kein TBD-Wert darf unbemerkt als
  gueltiger Produktivwert dienen.
- Hardwaretests, reales Sensorabziehen, Peltierpulse und thermisches Tuning
  sind nicht Bestandteil dieses Plan-PRs und bleiben in den verknuepften
  Hardware-/Inbetriebnahme-Issues blockiert.

## 11. Ressourcen- und Betriebsbudget

- Der Auswahlkern verwendet feste Werttypen, keine unbounded Historie und
  keine neue Bibliothek.
- Sensorqualitaetswerte werden als begrenzte Snapshots referenziert; keine
  zweite unlimitierte Messhistorie wird angelegt. `CrossRolePlausibilityContext`
  (6.10) buendelt nur drei bereits vorhandene Snapshots plus Phase/Zeit.
- Laufereignisse verwenden die bestehenden begrenzten Lauf-/Meldungs-/Journal-
  grenzen. Der neue `SensorSelectionEvent`-Typ und das RAM-only
  `selectionPhase`-Feld sind pro aktivem Lauf konstant gross.
- Das neue `ReturnStrategy`-Feld ist ein zusaetzliches `uint8` pro Programm
  (RAM und Wire) - vernachlaessigbar gegenueber den bestehenden `optional`-
  Feldern derselben Struktur.
- Commit 1 hat einen mechanisch breiten, aber inhaltlich flachen Diff:
  jede Testfixture, die ein `ProgramDocument` mit expliziter
  `presentFields`-Maske von Hand konstruiert, muss die neue Maske
  uebernehmen. Der `static_assert` in `program_model.cpp` faengt nur
  vergessene `kRequiredFields`-Eintraege ab, nicht betroffene Testdateien
  ausserhalb dieser Datei - diese werden vor Commit 1 vollstaendig
  durchsucht (`grep -rl "kCurrentRequiredProgramFields\|presentFields"
  test/`), damit keine stillschweigend kaputte Fixture uebrig bleibt.
- Erwartete RAM-Wirkung: ein kleiner Auswahlstatus und ein Ereigniswert pro
  aktivem Lauf; reale Byte-/Heapwerte bleiben
  `TBD_IMPLEMENTATION_BUDGET`, bis ein reproduzierbarer Build sie belegt.
- Keine PSRAM-, OTA-, Netzwerk- oder Echtzeitabhaengigkeit.

## 12. SOLID-, DRY- und KISS-Bewertung des geplanten Diffs

- **Single Responsibility:** `SensorQualityPipeline` bleibt Evidenzlieferant;
  `program_model` besitzt weiterhin allein die Programmvalidierung
  einschliesslich der neuen Cross-Field-Regeln (6.13); `SensorSelection`
  entscheidet Rollen und Policy anhand eines bereits validierten Programms;
  Lauf-/Persistenzcode wendet nur atomare Entscheidungen an; Safety-/
  Aktorlogik bleibt in #24/#23.
- **Open/Closed:** neue Sensorstrategien werden ueber den bestehenden
  `RunSensorMode`-/Policyvertrag und ein neues, aber in dieselbe bestehende
  Gruppierung (`ProductSensorFailure`) eingeordnetes Feld erweitert, nicht
  durch Hardware-`if`-Ketten in Composition Roots.
- **Liskov:** alle Mocks liefern weiterhin den kanonischen
  `ITemperatureSource`-/Snapshotvertrag; keine konkrete Hardwareklasse wird
  im Auswahlkern vorausgesetzt.
- **Interface Segregation:** der Auswahlkern erhaelt nur Snapshots,
  Laufkontext, `CrossRolePlausibilityContext` und monotone Zeit;
  Eventjournal, Aktor und UI werden nicht als Universalinterface
  hineingezogen.
- **Dependency Inversion:** Fachentscheidung haengt von
  `SensorQualitySnapshot` und schmalen Anwendungsdaten ab, nicht von
  DS18B20, ESP-IDF, GPIO oder PlatformIO.
- **DRY:** `RunSensorMode`, Programmpolicies, `ReturnStrategy` und #20-
  Qualitaetsstatus werden wiederverwendet. Es entsteht kein zweiter
  Qualitaetsautomat und keine parallele Wire-Darstellung. Die
  Rueckkehrstrategie wird bewusst in `ProductSensorFailure` statt in einem
  neuen Top-Level-Feld gefuehrt, um keine zweite Gruppierung fuer denselben
  Fachzusammenhang zu schaffen. `RunRevision`/`RunChangeReason` werden
  bewusst NICHT fuer Sensorwechsel zweckentfremdet (6.11) - das waere die
  falsche Art von Wiederverwendung (Struktur passt fachlich nicht).
- **KISS:** ein reiner Entscheidungsdienst plus begrenzter Eventwert ist
  ausreichend. Der Restart-Vertrag (6.12) verzichtet bewusst auf ein neues
  persistiertes Feld zugunsten einer einfachen, konservativen
  Fail-closed-Regel. Eine generische Regel-Engine, Plugin-Registry oder
  Universalplattform waere unnoetige Abstraktion.

Bewusste Grenze: Die rollenuebergreifende Plausibilitaet muss in #21 liegen,
obwohl #20 bereits Einzelwerte filtert. Das ist keine DRY-Verletzung, weil
die Rollen- und Prozessbedeutung in `fermentation_app` entsteht und #20
bewusst keine Fermentationsrollen kennt. `CrossRolePlausibilityContext`
selbst dupliziert keine #20-Logik, sondern buendelt nur bereits vorhandene
Snapshotfelder.

## 13. Offene Ownerentscheidungen und Gates

Diese Revision trennt bereits entschiedene Fachvertraege von echten
Ownerentscheidungen. Folgende Punkte der ersten Revision sind aus Code,
ADRs und Fachvertraegen bereits ableitbar und werden **nicht mehr** als
Owner-Gate gefuehrt:

| Ehemalige ID | Punkt | Wo entschieden |
|---|---|---|
| P21-02 | `ProductRequired` + Luftfallback | 6.13 Regel 1, wird bei der Programmvalidierung abgelehnt |
| P21-06 | Automatische Neuauswahl bei `COOLING`/`COOL_HOLDING` | 6.9, bereits durch `docs/TEMPERATURE_CONTROL.md` entschieden |
| P21-07 | Eigener persistenter Mutationspfad fuer Sensorwechsel | 6.11/6.12, erforderlich und im Diff eingeplant, kein Genehmigungsvorbehalt |
| P21-04 | Bindung an `IEventJournal`/#19 | 6.11, strukturierter Event plus bestehender Meldungspfad, dauerhafte Aufbewahrung bleibt #19 |
| P21-05 | Numerische Commissioning-Werte | bleiben `TBD_COMMISSIONING`; kein allgemeines Architektur-Gate, siehe Abschnitt 10 |

Als echte Ownerentscheidungen verbleiben, weil sie durch Code, ADRs,
Fachvertraege und Issue tatsaechlich nicht entschieden sind:

| ID | Offene Entscheidung | Planvorschlag / Stopwirkung |
|---|---|---|
| P21-01 | Vollstaendige Startmatrix (6.5) freigeben | wie in 6.5 tabelliert; ohne Freigabe kein Startvertrag |
| P21-M1 | Migrationsabbildung Schema 5 -> 6 fuer `ReturnStrategy` (6.2.1) | produktfaehige Programme -> `automatic_validated_return_to_product`; `AirOnly` -> `remain_on_air_until_end`. Ohne Freigabe keine Migrationsimplementierung |
| P21-M2 | Restart-Verhalten bei unbekanntem Fallback-Ursprung (6.12) | Vorschlag: `activeRunSensorMode == Air` nach Neustart sperrt jede Rueckkehr fuer den Rest des Laufs, kein neues persistiertes Feld. Alternative (grosser): dediziertes persistiertes Ursprungsfeld mit `kRunPersistenceSchema`-Bump. Ohne Entscheidung keine Restart-Implementierung |
| P21-M3 | Vertrag fuer produktgefuehrte manuelle Laeufe (6.8) | Vorschlag: unterstuetzt mit festem WaitForUser-/ManualReturn-Vertrag. Alternative: Produktmodus fuer manuelle Laeufe ablehnen. Ohne Entscheidung kein `RunSensorMode::Product` fuer `ManualStartRequest` |
| P21-M4 | Umfang der "vollstaendigen Validierung" fuer automatische Rueckkehr in Release 1 (6.10) | Vorschlag: nur die in #21 verfuegbare Teilmenge (VALID, `recoveryProgressCount`, Stabilitaetszeit, Differenzgrenze); Richtung/Regelanforderungsdauer/Totzeit bleiben #22/#23. Ohne Entscheidung bleibt `automatic_validated_return_to_product` implementierbar, aber nicht wie in `TEMPERATURE_CONTROL.md` vollstaendig ausgelegt |

Eine Entscheidung, die Schema, Wireformat, Fehlerklasse, Safetyfreigabe,
Hardwareannahme oder Issue-/PR-Struktur veraendert, ist eine materielle
Planabweichung. Dann wird die Umsetzung angehalten, der Plan aktualisiert,
neu committed und erneut freigegeben.

## 14. Dokumentations- und Abschlussnachweise

Vor `Ready for review` werden im Draft-PR auf dem exakten HEAD dokumentiert:

- freigegebene Plan-SHA und Zuordnung jedes umgesetzten Planpunkts zu
  Commits;
- tatsaechlich geaenderte Dateien und jede Abweichung vom erwarteten Diff,
  einschliesslich mechanisch angepasster Testfixtures aus Commit 1 (11.);
- direkte Testbefehle und Ergebnisse, einschliesslich `BLOCKED`/
  `NOT_RUN` fuer nicht angeordnete Volltests oder Hardware;
- `git diff --check`, Secret- und Architekturpruefung;
- Nachweis, dass #20-Vertraege wiederverwendet und keine #22/#23/#24-
  Verantwortungen vorweggenommen wurden;
- konkrete SOLID-/DRY-/KISS-Pruefung gegen den tatsaechlichen Diff;
- verbleibende `TBD_HARDWARE`, `TBD_COMMISSIONING`,
  `TBD_IMPLEMENTATION_BUDGET` und Owner-Gates (Abschnitt 13);
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
[ ] P21-01, P21-M1 bis P21-M4 aufgeloeste Ownerentscheidungen gegen den Plan abgleichen
[ ] ReturnStrategy-Enum, Feldmaske, Schema 6 und Validierung in program_model implementieren
[ ] Migrationsschritt Schema 5 -> 6 gemaess freigegebener P21-M1-Abbildung implementieren
[ ] Codec-Wire-ID und Payloadgroesse fuer ReturnStrategy implementieren
[ ] Werkskatalog und config/programs.example.yaml auf Schema 6 aktualisieren
[ ] betroffene Testfixtures mit expliziter presentFields-Maske vollstaendig durchsuchen und anpassen
[ ] SensorQualitySnapshot-Inputs ohne Parallelqualitaetsmodell anschliessen
[ ] Auswahlkern mit vollstaendigem Zustandsautomaten (6.4) und getrenntem activeMode-/peltierPermission-Vertrag implementieren
[ ] Startmatrix (6.5) in decideProgramStart vor der NotConfirmed-Rueckgabe durchsetzen
[ ] Produktfehler, Wartezeit und alle drei Rueckkehrstrategien implementieren
[ ] festen Vertrag fuer produktgefuehrte manuelle Laeufe (6.8) gemaess P21-M3 implementieren
[ ] feste Schrankluft-/Kuehlkoerpersensoren in jeder Peltierfreigabe erzwingen
[ ] CrossRolePlausibilityContext (6.10) auf verfuegbaren Snapshotfeldern implementieren
[ ] Rueckkehr erst nach #20-VALID, stabilen Proben und 6.10-Plausibilitaetspruefung zulassen
[ ] Moduserhalt bei Kuehlen und Halten implementieren und testen
[ ] strukturiertes SensorSelectionEvent und sichtbaren Meldungspfad anschliessen, ohne RunRevision/RunChangeReason zu benutzen
[ ] Restart-Vertrag (6.12) ohne neues RunPersistenceSnapshot-Feld gemaess P21-M2 implementieren
[ ] atomare Laufrevision/Persistenzwirkung nur im freigegebenen Vertrag umsetzen
[ ] direkte, gezielte Unit-Tests fuer Schema, Migration, Codec, Auswahl, Fehler, Safetyblock, Rueckkehr und Restart ausfuehren
[ ] gezielte Laufkommand-/Laufpersistenz-/Codec-Konsumententests ausfuehren
[ ] gezielte Architektur-, Secret-, Format- und git diff --check-Pruefungen ausfuehren
[ ] betroffene Fachvertraege und docs/ACCEPTANCE_TESTS.md aktualisieren
[ ] docs/ROADMAP.md nur bei tatsaechlicher Status- oder Gatewirkung synchronisieren
[ ] Ressourcenwirkung, begrenzte Puffer und offene Hardware-/Commissioning-Gates dokumentieren
[ ] Review des vollstaendigen aktuellen Diffs gegen Issue, Plan, ADRs und Fachvertraege durchfuehren
[ ] SOLID-, DRY- und KISS-Bewertung gegen den tatsaechlichen Diff durchfuehren
[ ] keinen unbelegten Hardware-, Safety-, Persistenz- oder Bibliotheksentscheid im Diff belassen
[ ] alle Reviewbefunde fachlich bewerten; Threads nur nach ausdruecklicher Autorisierung bearbeiten
[ ] PR-Beschreibung mit Plan-SHA, aktuellem HEAD, Tests, Abweichungen und Restgates aktualisieren
[ ] Owner setzt Draft erst nach befundleerem Review auf Ready for review
[ ] genau eine vollstaendige Remote-CI fuer den reviewten HEAD abwarten und Ergebnis dokumentieren
[ ] bei CI-Fehler PR-Draft-/Korrektur-/Reviewzyklus gemaess Workflow durchfuehren
[ ] Abschlussnachweise, geaenderte Dateien und offene Gates vollstaendig dokumentieren
[ ] HALTED_FOR_OWNER_REVIEW beziehungsweise Owner-Entscheidung dokumentieren
```

## 16. Stopbedingung

Nach Commit und Push dieser Planrevision sowie der notwendigen
Roadmap-/PR-Aktualisierung wird im Draft-PR der exakte neue Plan-Commit,
der aktuelle HEAD und

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
[x] docs/ROADMAP.md auf PR #98 merged und Issue #21 als aktuelle Planungsarbeit aktualisieren
[x] PR-#99-Reviewbefunde vom 2026-08-06 gegen Code verifiziert (program_model, configuration_document_codec, run_commands, run_snapshot, run_persistence_contract/codec, standard_program_catalog, config/programs.example.yaml)
[x] kanonisches ReturnStrategy-Programmschemafeld inklusive Migration, Codec, Katalog und Beispielkonfiguration geplant
[x] vollstaendigen Auswahlzustandsautomaten inklusive Restart-Vertrag ohne neue Persistenzschema-Erweiterung geplant
[x] Vertrag fuer produktgefuehrte manuelle Laeufe entschieden und geplant
[x] Owner-Gates auf tatsaechlich unentschiedene Punkte bereinigt (P21-01, P21-M1 bis P21-M4)
[x] vollstaendige Startmatrix, CrossRolePlausibilityContext und zentrale Cross-Field-Validierung geplant
[x] ausschliesslich Plan und notwendige Roadmap-/PR-Aktualisierung geaendert
[ ] Plan committen und pushen
[ ] Draft-PR mit exakter neuer Plan-SHA, aktuellem HEAD und aufgeloesten/verbleibenden Ownerentscheidungen aktualisieren
[ ] HALTED_FOR_OWNER_REVIEW
```
