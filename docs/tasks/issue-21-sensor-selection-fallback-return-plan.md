# Plan: Issue #21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik

## 1. Metadaten und Status

```text
Issue: #21 [E3.2] Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik
Epic: #5 (E3)
Branch: plan/issue-21-sensor-selection-fallback-return
Baseline main: ff2e66a8c340d61c8c4517f90fd3fba5a8fc3db2
Vorherige, NICHT freigegebene Plan-Commits:
  c505fce6cbd12a02f9c195cdba7bf0dc37d3c8bd (Revision 1)
  aaeefbdf6997bbbbd9359985ed00f9b75ab6283e (Revision 2)
PLAN_ONLY: YES
IMPLEMENTATION_STARTED: NO
PLAN_STATUS: PLAN_DRAFT_REVIEW_REQUIRED
IMPLEMENTATION_STATUS: IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

Dies ist Revision 3. Sie behebt die im PR-#99-Review vom 2026-08-06 zu
Revision 2 benannten materiellen Befunde: fehlender atomarer Persistenzpfad
fuer Sensorentscheidungen, nicht rekonstruierbarer Fallback-Ursprung nach
Neustart, unzulaessig abgeschwaechter Plausibilitaetsvertrag fuer die
automatische Rueckkehr, einstufige statt verketteter Programmschema-Migration
sowie mehrere Praezisierungsluecken im Zustandsautomaten und in den
Cross-Field-Regeln. Produktionscode, produktive Tests, Toolchain,
Buildkonfiguration, Hardwarekonfiguration und Abhaengigkeiten werden
weiterhin in dieser Planungsphase nicht geaendert.

## 2. Live-Issue- und Abhaengigkeitsabgleich

| Quelle | Live-Stand am 2026-08-06 | Bedeutung fuer diesen Plan |
|---|---|---|
| Issue #21 | OPEN, Body-Status `PLANNED_SPEC_PENDING`, keine Kommentare | eigener Plan-first-Draft-PR, keine Implementierungsfreigabe |
| Issue #14 | CLOSED | kanonische Prozesszustaende und Uebergangstopologie stehen zur Verfuegung |
| Issue #20 | CLOSED, Body-Status `READY` ist historisch | `SensorQualitySnapshot` und `SensorQualityPipeline` sind die bestehende Qualitaetsquelle |
| Epic #5 | OPEN | Issue bleibt Teil des E3-Sensor-/Regel-/Safety-Kerns |
| PR #99 | OPEN, Draft, dieser Plan ist die dritte Revision | Reviewbefunde vom 2026-08-06 zu Revision 2 sind Grundlage dieser Ueberarbeitung |

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

Zusaetzlich zu den in Revision 1 und 2 gelesenen Quellen wurden fuer diese
Ueberarbeitung konkret nachvollzogen:

- `docs/CONFIGURATION_PERSISTENCE.md`, Abschnitt "Schema, Revision und
  Migration": der kanonische Vertrag beschreibt Migrationen ausdruecklich als
  **Copy-Migrationen mit einzeln ausgefuehrten Schritten** ("jeden
  Migrationsschritt einzeln auf einer Kopie ausfuehren"), nicht als
  Einzelsprung von genau einer Vorgaengerversion. Revision 2s
  `kMigratableProgramSchemaVersion`-Verschiebung (4 -> 5) widersprach diesem
  Vertrag, weil sie Schema 4 nach dem Bump auf Schema 6 endgueltig
  unsupported gemacht haette, statt es ueber 4 -> 5 -> 6 zu fuehren;
- `lib/fermentation_app/src/run_persistence_coordinator.hpp/.cpp` im Detail:
  `writeSnapshot` (Zeilen ~296-539) ist der gemeinsame Zwei-Phasen-
  Schreibmechanismus (prepared Head -> Slot -> committed Head) fuer sowohl
  periodische Checkpoints als auch Kommando-/Uebergangsmutationen;
  `persistCommand` (~541-606) und `persistTransition` (~608-659) sind duenne
  Wrapper, die eine Kandidatenkopie erzeugen, `makeRunPersistenceSnapshot`
  aufrufen und danach `writeSnapshot(..., periodic=false, ..., commandId)`
  nutzen. **`writeSnapshot` leitet `RunPersistenceMutationKind` bislang
  ausschliesslich aus `commandId.has_value()` ab** (Zeilen 370-372: `Command`
  falls vorhanden, sonst `Transition`) - ein dritter Mutationsgrund
  existiert nicht;
- `lib/fermentation_app/src/run_persistence_codec.cpp`: die Kopf- und
  Checkpoint-Envelopes tragen unabhaengig je einen eigenen
  `kRunPersistenceSchema`-Konstantenwert (identisch `1U`, einmal in
  `run_persistence_codec.cpp` Zeile 22, einmal in
  `run_persistence_coordinator.cpp` Zeile 13 - zwei separate Definitionen
  desselben Werts, ein bestehender kleiner DRY-Verstoss); `runCheckpointReferenceMatches`
  (Zeilen 963-975) und mehrere `!= kRunPersistenceSchema`-Pruefungen
  (Codec ~718, 881, 950; Coordinator beim Laden) vergleichen strikt auf
  Gleichheit mit der aktuellen Konstante, es existiert kein
  Migrationsmechanismus fuer `RunPersistenceSnapshot` selbst;
- `lib/fermentation_app/src/configuration_document_codec.cpp`,
  `readProgramLimitsAndCompletion` (Zeilen ~519-538): die bedingte Lektuere
  von `maximumProductWaitMinutes` prueft `schemaVersion >=
  kCurrentProgramSchemaVersion` - **relativ zur jeweils aktuellen Konstante,
  nicht zur absoluten Einfuehrungsversion 5**. Das funktioniert nur zufaellig
  heute, weil `kCurrentProgramSchemaVersion == 5` ist. Nach einem Bump auf 6
  wuerde ein Schema-5-Dokument (das dieses Feld auf dem Wire tatsaechlich
  besitzt) faelschlich `5 >= 6 == false` auswerten, das Feld ueberspringen und
  jeden nachfolgenden Lesevorgang (Completion-Modus, Kuehlziel,
  Haltedauer) fehlausrichten - ein latenter Silent-Corruption-Fehler, den
  dieser Plan jetzt konkret beheben muss, nicht nur "robuster" gestalten;
- `test/test_configuration_codecs/test_configuration_codecs.cpp`,
  `maximizeProgramPayload` (Zeilen 98-117) und `maximumValidCatalog`
  (119-131, verwendet in `validateProgramCatalog`-Test Zeile 612-613):
  dieses bestehende Payload-Maximierungs-Fixture setzt fuer **jedes**
  Katalogprogramm `SensorPreference::AirOnly` **und** `ProductSensorFailurePolicy::
  FallbackToAirAfterTimeout` **und** `fallbackDelaySeconds = 0U` gleichzeitig
  und erwartet `validateProgramCatalog(...) == Success`. Die in dieser
  Revision praezisierte `AirOnly`-Cross-Field-Regel (6.13) macht genau diese
  Kombination ungueltig - das Fixture muss angepasst werden (8, 9.1);
- `lib/fermentation_app/src/run_commands.hpp`, `RunCommandState`
  (Zeilen 268-298) und `run_persistence_contract.cpp`
  (`makeRunPersistenceSnapshot`/`restoreRunPersistenceSnapshot`): beide
  Richtungen kopieren aktuell nur `activeRunSensorMode` zwischen RAM-Zustand
  und Snapshot; ein Feld fuer den Sensorselektionsursprung existiert in
  keiner der beiden Strukturen.

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
  `automatic_validated_return_to_product` als kanonisches, typisiertes
  Programmfeld umgesetzt werden;
- wie jeder tatsaechliche Wechsel als sichtbares fachliches Laufereignis,
  Laufrevision und spaeter auslesbare Meldungs-/Journalreferenz weitergegeben
  wird, **und dabei so persistiert wird, dass die konfigurierte
  Rueckkehrstrategie auch nach einem Neustart korrekt angewendet werden
  kann** (Korrektur gegenueber Revision 2, siehe 6.12).

Die Entscheidung ist reversibel: Sie mutiert den laufenden Zustand nicht
selbst, sondern liefert eine erwartete Vorher-/Nachher-Entscheidung. Eine
lauf- oder aktorwirksame Anwendung erfolgt erst nach einer **eigenen,
benannten atomaren Persistenztransaktion** (6.14), nicht ueber eine
zweckentfremdete Kommando- oder Uebergangsmutation.

Diese Revision macht zusaetzlich explizit zum Ziel:

- ein kleiner, typisierter, versioniert persistierter
  Sensorselektionszustand ersetzt die in Revision 2 vorgeschlagene
  dauerhafte Rueckkehrsperre nach jedem Neustart; die Sperre bleibt nur
  Rueckfallverhalten fuer tatsaechlich alte/migrierte Datensaetze;
- der vollstaendige rollenuebergreifende Plausibilitaetsvertrag aus
  `docs/TEMPERATURE_CONTROL.md` (Richtung, Regelanforderungsdauer,
  thermischer Kontext) wird als typisierter Vertrag geplant, nicht auf die
  in #21 bereits vorhandene Teilmenge reduziert;
- die Programmschema-Migration folgt der kanonischen
  Copy-Migrationskette (4 -> 5 -> 6) statt eines Einzelsprungs.

### Nicht-Ziele

- keine DS18B20-, 1-Wire-, GPIO-, Display-, Touch-, WLAN- oder
  ESP-IDF-Treiberimplementierung;
- keine konkrete Pin-, Pegel-, Bus- oder Steckerentscheidung;
- keine PI-Regelung, Luftbegrenzungsregel, Aktorplanung, Totzeit oder
  Lueftersteuerung aus #22/#23. #21 definiert den vollstaendigen
  Plausibilitaetsvertrag (6.10) bereits typisiert; die tatsaechliche
  Befuellung von Regelrichtung, Regelanforderungsdauer und thermischem
  Profil bleibt #22/#23 vorbehalten. Bis dahin bleibt
  `automatic_validated_return_to_product` mangels Evidenz praktisch inert
  (13, P21-M4);
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
- keine Wiederverwendung von `RunRevision`/`RunChangeReason` fuer
  Sensorwechsel (6.11) - das waere eine fachfremde Zweckentfremdung einer
  auf Temperatur-/Dauer-Anpassungen zugeschnittenen Struktur;
- keine generische, unbegrenzte Migrationskette fuer beliebig viele
  zukuenftige Schemaversionen - geplant und implementiert werden genau die
  tatsaechlich existierenden Schritte 4 -> 5 und 5 -> 6 (6.2), keine
  vorsorgliche Plugin-/Registry-Abstraktion.

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
  `ProductSensorFailurePolicy` sowie die validierte `fallbackDelaySeconds`,
  aber keine Rueckkehrstrategie (wird in 6.2 ergaenzt).
- `decideProgramStart` prueft den angeforderten `RunSensorMode` bislang
  nicht gegen `program.sensorPreference` (wird in 6.5 ergaenzt).
- `RunCommandState::activeRunSensorMode` wird bei aktiven Programm- und
  manuellen Laeufen gefuehrt; `RunPersistenceSnapshot` speichert diesen
  Modus bereits. Ein Sensorselektionsursprung (Start vs. Fallback) wird
  weder im RAM-Zustand noch im Snapshot gefuehrt.
- `RunPersistenceCoordinator` besitzt genau zwei Mutationspfade,
  `persistCommand` und `persistTransition`, beide auf den gemeinsamen
  Zwei-Phasen-Schreiber `writeSnapshot` aufgesetzt. `writeSnapshot` leitet
  `RunPersistenceMutationKind` derzeit binaer aus der Anwesenheit einer
  `CommandId` ab; ein automatischer Sensorwechsel (kein Benutzerkommando,
  kein Prozessuebergang) haette in diesem Schema faelschlich als
  `Transition` gegolten.
- `RunPersistenceSnapshot` hat kein Migrationskonzept: jede
  Schemaversionsabweichung wird beim Laden hart als `UnsupportedSchema`
  abgelehnt (`loadAndInitialize`).
- `migrateProgramToCurrentSchema` unterstuetzt nur genau eine
  Vorgaengerversion (`kMigratableProgramSchemaVersion`, aktuell exakt 4),
  keine Verkettung mehrerer Schritte - im Widerspruch zum kanonischen
  Copy-Migrationsvertrag aus `docs/CONFIGURATION_PERSISTENCE.md`.
- `readProgramLimitsAndCompletion` verankert die bedingte Feldlektuere an
  `kCurrentProgramSchemaVersion` statt an der absoluten
  Feldeinfuehrungsversion - ein latenter Fehler, der erst bei einem
  weiteren Schema-Bump sichtbar wuerde, wenn er nicht jetzt behoben wird.
- `RunRevision`/`RunChangeReason` (`run_snapshot.hpp`) sind eine
  Anpassungshistorie fuer Zieltemperatur und Restdauer, keine generische
  Ereignishistorie.
- Die Zustandsmaschine kennt die relevanten Phasen und akzeptiert nur
  abstrakte `ProcessSignals`. Sie kennt noch keine Sensorrollen-
  oder Fallbacklogik.
- `FermentationApplication` und beide Composition Roots sind derzeit nur
  Grundgeruest.

### Fehlende Teile

- kein `ReturnStrategy`-Feld im Programmschema;
- keine verkettete Programmschema-Migration ueber mehr als einen Schritt;
- keine Cross-Field-Validierung zwischen `SensorPreference`,
  `ProductSensorFailurePolicy` und `ReturnStrategy`;
- kein `SensorSelection`-Wertmodell oder Entscheidungsdienst;
- kein vollstaendig praezisierter Auswahlzustandsautomat (Zustand pro
  Tabellenzeile: Modus, Peltierfreigabe, Eintritts-/Austrittsbedingung,
  Ereigniswirkung);
- kein eigener, benannter atomarer Persistenzpfad fuer Sensorwechsel;
- kein persistierter, versionierter Sensorselektionszustand fuer den
  Neustartfall;
- kein vollstaendig typisierter `CrossRolePlausibilityContext` mit
  Regelrichtung, Regelanforderungsdauer und thermischem Kontext;
- keine gezielten Tests fuer Ausfall, gleichzeitig ungueltige feste Sensoren,
  Rueckkehr, Moduserhalt in Kuehl-/Haltephasen, Startmatrix, Migration,
  Cross-Field-Validierung, Restart oder atomare Sensorpersistenz.

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

- unveraenderlicher `ProgramDefinition`-/Laufkontext, inklusive
  `ReturnStrategy` (6.2);
- aktueller `RunSensorMode`, RAM-Auswahlphase und persistierter
  Sensorselektionszustand des Laufes (6.4, 6.12);
- aktueller `ProcessState` einschliesslich `COOLING` und `COOL_HOLDING`;
- monotone Zeit des Bewertungsaufrufs;
- explizite Benutzeraktion, falls die konfigurierte Strategie eine verlangt;
- der vollstaendige `CrossRolePlausibilityContext` (6.10).

`SensorQuality::Valid` ist die notwendige Qualitaet fuer einen nutzbaren
Regelwert. Ein Snapshot mit `STALE`, fehlendem Wert, zu hohem Alter oder
`FAILED` ist nicht fuer eine Peltierfreigabe nutzbar. Ein alter Wert darf in
Diagnose und Fehlerbewertung sichtbar bleiben, wird aber nicht als aktuell
verwendet.

### 6.2 Rueckkehrstrategie im kanonischen Programmmodell

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

Die Einordnung in `ProductSensorFailure` folgt der bestehenden fachlichen
Gruppierung: die Rueckkehr ist die Gegenbewegung zum in derselben Struktur
konfigurierten Ersatzverhalten. Ein neues, paralleles Top-Level-Feld waere
eine unnoetige zweite Gruppierung fuer denselben Fachzusammenhang.

`ProgramField` erhaelt ein neues Bit `ReturnStrategy = 1ULL << 16U`.
`kSchema4RequiredProgramFields` bleibt unveraendert. Das bisherige
`kCurrentRequiredProgramFields` (Schema 5) wird in
`kSchema5RequiredProgramFields` umbenannt und bleibt inhaltlich identisch.
Das neue `kCurrentRequiredProgramFields` wird
`kSchema5RequiredProgramFields | fieldMask(ProgramField::ReturnStrategy)`.

```text
kCurrentProgramSchemaVersion:              5 -> 6
kMinimumMigratableProgramSchemaVersion:    4 (unveraendert; ersetzt das
                                            bisherige einstufige
                                            kMigratableProgramSchemaVersion)
```

`kMinimumMigratableProgramSchemaVersion` ersetzt `kMigratableProgramSchemaVersion`
und benennt bewusst eine **untere Grenze eines Bereichs**, nicht "die eine
migrierbare Vorgaengerversion". Sie bleibt bei zukuenftigen Schema-Bumps
unveraendert, solange Schema 4 nicht durch eine eigenstaendige
Ownerentscheidung (siehe unten) abgekuendigt wird. Betroffene Fundstellen
(vollstaendig durchsucht, `grep -rn "kMigratableProgramSchemaVersion"
lib/ test/`):

- `program_model.hpp` (Definition);
- `program_model.cpp` (`migrateProgramToCurrentSchema`, zwei Vergleiche);
- `configuration_document_codec.cpp` Zeilen 462 und 558
  (`readProgramSchema`, `readProgram`-Migrationsauslöser);
- `test/test_configuration_codecs/test_configuration_codecs.cpp` Zeilen 207,
  694;
- `test/test_program_models/test_program_models.cpp` Zeilen 186, 225.

Neue absolute Feldeinfuehrungskonstanten (beheben den in Abschnitt 3/5
verifizierten `readProgramLimitsAndCompletion`-Fehler):

```cpp
inline constexpr std::uint32_t kProductWaitFieldIntroducedInSchema = 5U;
inline constexpr std::uint32_t kReturnStrategyFieldIntroducedInSchema = 6U;
```

#### 6.2.2 Verkettete Migration (Copy-Migration, `docs/CONFIGURATION_PERSISTENCE.md`)

`lib/fermentation_app/src/program_model.cpp`:

- `kRequiredFields` erhaelt einen 17. Eintrag
  `{ProgramField::ReturnStrategy, "defaults.product_sensor_failure.return_strategy"}`.
  Der bestehende `static_assert(kRequiredFields.size() ==
  kCurrentProgramFieldCount)` bleibt die Absicherung gegen einen
  vergessenen Eintrag.
- `validReturnStrategy(ReturnStrategy)` analog zu `validSensorPreference`/
  `validFailurePolicy`.
- `migrateProgramToCurrentSchema` wird von einem Einzelschritt in eine
  **Schrittkette** umgebaut, exakt wie in `CONFIGURATION_PERSISTENCE.md`
  Punkt 4 verlangt ("jeden Migrationsschritt einzeln auf einer Kopie
  ausfuehren"):

  ```cpp
  namespace {
  MigrationResult migrateProgramSchema4To5(const ProgramDocument& source) {
      // bestehende Logik aus Revision 1/2, unveraendert: setzt
      // maximumProductWaitMinutes auf std::nullopt und hebt presentFields
      // auf kSchema5RequiredProgramFields.
  }
  MigrationResult migrateProgramSchema5To6(const ProgramDocument& source) {
      // neu: setzt ReturnStrategy gemaess der Abbildung unten und hebt
      // presentFields auf kCurrentRequiredProgramFields.
  }
  }  // namespace

  MigrationResult migrateProgramToCurrentSchema(const ProgramDocument& source) {
      if (source.schema.version == kCurrentProgramSchemaVersion)
          return {MigrationStatus::NotRequired, source};
      if (source.schema.version < kMinimumMigratableProgramSchemaVersion ||
          source.schema.version > kCurrentProgramSchemaVersion)
          return {MigrationStatus::UnsupportedSourceVersion, std::nullopt};
      ProgramDocument current = source;
      while (current.schema.version < kCurrentProgramSchemaVersion) {
          auto step = current.schema.version == 4U
                          ? migrateProgramSchema4To5(current)
                          : migrateProgramSchema5To6(current);
          if (step.status != MigrationStatus::Migrated ||
              !step.document.has_value())
              return step;
          current = std::move(*step.document);
      }
      return {MigrationStatus::Migrated, std::move(current)};
  }
  ```

  Es wird bewusst kein generischer Schrittregistry-Mechanismus gebaut - die
  Kette hat exakt zwei bekannte Schritte (KISS, siehe Nicht-Ziele).

Migrationsabbildung fuer Schritt 5 -> 6:

```text
program.sensorPreference != AirOnly
  -> returnStrategy = AutomaticValidatedReturnToProduct

program.sensorPreference == AirOnly
  -> returnStrategy = RemainOnAirUntilEnd
  -> UND policy wird auf FallbackToAirAfterTimeout normalisiert
  -> UND fallbackDelaySeconds wird auf std::nullopt gesetzt, falls belegt
```

Die letzten beiden Zeilen sind die im Review konkret geforderte Klaerung:
ein migriertes Schema-4/5-`AirOnly`-Dokument, das (wie vor dieser Revision
technisch zulaessig) eine andere Policy oder einen gesetzten
`fallbackDelaySeconds`-Wert traegt, wird durch den Migrationsschritt selbst
in die einzige nach 6.13 gueltige `AirOnly`-Kombination ueberfuehrt, statt
nach erfolgreicher Migration an der neuen Cross-Field-Regel zu scheitern.
Ein vor diesem Update gueltiges Dokument bleibt dadurch auch nach dem Update
gueltig - keine stille Verschlechterung bestehender Konfigurationen. Diese
Normalisierung ist Teil von P21-M1 (Abschnitt 13) und wird als eigener
Migrationstestfall gefuehrt (9.1).

#### 6.2.3 Validierung

`validateProgram` erhaelt:

- `InvalidEnumValue` fuer `defaults.product_sensor_failure.return_strategy`,
  falls kein gueltiger `ReturnStrategy`-Wert vorliegt;
- die Cross-Field-Regeln aus 6.13.

`ValidationErrorCode` erhaelt einen neuen Wert `IncompatibleCombination` fuer
echte Enum-/Enum-Widersprueche (6.13, Regeln 1-3). Fuer "Feld ist gesetzt,
obwohl im aktuellen Kontext nicht anwendbar" wird der bereits bestehende
`UnexpectedValue`-Code wiederverwendet (6.13, Regel 4), analog zum
bestehenden Muster bei `maximumProductWaitMinutes`/`preheat` und
`coolingTargetCelsius`/`completion.mode` (`program_model.cpp`,
Zeilen ~198-239). Kein neuer Code fuer einen bereits vorhandenen
Regel-Shape.

#### 6.2.4 Codec

`configuration_document_codec.cpp` erhaelt `returnStrategyToWireId`/
`returnStrategyFromWireId` nach demselben Muster wie
`productSensorFailurePolicyToWireId`. Schreiben/Lesen erfolgt innerhalb des
bestehenden `productSensorFailure`-Blocks in
`readProgramIdentityAndFlags`/der zugehoerigen Schreibfunktion (~244-246,
493-496), **bedingt auf `schemaVersion >= kReturnStrategyFieldIntroducedInSchema`**
- nicht auf `kCurrentProgramSchemaVersion`. Die bestehende Bedingung fuer
`maximumProductWaitMinutes` in `readProgramLimitsAndCompletion` (~528) wird
im selben Commit von `schemaVersion >= kCurrentProgramSchemaVersion` auf
`schemaVersion >= kProductWaitFieldIntroducedInSchema` korrigiert - siehe
Abschnitt 3 fuer den konkreten Fehlerfall, den diese Korrektur verhindert.
`readProgramSchema` akzeptiert `candidate.schema.version` im Bereich
`[kMinimumMigratableProgramSchemaVersion, kCurrentProgramSchemaVersion]` und
waehlt `knownFields`/`requiredFields` je nach exakter Version
(`kSchema4RequiredProgramFields`, `kSchema5RequiredProgramFields` oder
`kCurrentRequiredProgramFields`). `readProgram`s Migrationsausloeser wird von
`candidate.schema.version == kMigratableProgramSchemaVersion` auf
`candidate.schema.version < kCurrentProgramSchemaVersion` geaendert, damit
sowohl Schema-4- als auch Schema-5-Dokumente durch die verkettete Migration
(6.2.2) laufen. Die Payload-Groessenberechnung (~355) wird um ein festes
`uint8` erweitert (kein `optional`, das Feld ist immer gesetzt).

#### 6.2.5 Werkskatalog, Beispielkonfiguration, Persistenz

- `standard_program_catalog.cpp`: alle vier Werksprogramme (keines ist
  `AirOnly`) erhalten `ReturnStrategy::AutomaticValidatedReturnToProduct`.
- `config/programs.example.yaml`: `schema_version: 6`, jeder
  `product_sensor_failure`-Block erhaelt `return_strategy:
  automatic_validated_return_to_product`; `allowed_values.return_strategy`
  wird ergaenzt.
- Programmkatalog-/Konfigurationspersistenz benoetigt keine strukturelle
  Aenderung ausser der Feld-/Wire-Erweiterung.

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
notice: optional SensorSelectionNotice (6.11)
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

#### 6.4.1 Zustaende mit vollstaendiger Definition

Jeder Zustand ist unten mit aktivem Modus, Peltierfreigabe,
Eintritts-/Austrittsbedingung und Ereigniswirkung definiert. `FallbackWaitPending`
aus Revision 2 entfaellt ersatzlos - es war ein reiner Alias fuer
`ProductFailureDetected` und wird nicht mehr referenziert.

| Zustand | Modus | `peltierPermission` | Eintritt | Austritt | Ereigniswirkung |
|---|---|---|---|---|---|
| `NormalProduct` | Product | Allowed (bei validem Product/Air/Cooling) | Start, Rueckkehr, Re-Evaluation ohne Aenderung | Produkt wird nicht mehr nutzbar | keines bei reiner Re-Evaluation |
| `NormalAir` | Air | Allowed (bei validem Air/Cooling) | Start mit Luft (`origin = InitialSelection`) | nie automatisch verlassen (kein Ersatzbetrieb aktiv) | keines |
| `ProductFailureDetected` | Product | **Blocked** (sofort) | Produkt-Snapshot wird nicht mehr nutzbar | Wartezeit ablaeuft, Produkt erneut valide, oder Policy = StopToSafeState | `SensorSelectionNotice` (kein Modus-Event, 6.11) |
| `UserDecisionRequired` | Product | Blocked | Wartezeit abgelaufen UND Policy = WaitForUser | explizite Benutzeraktion oder Air/Cooling wird ungueltig | `SensorSelectionNotice` |
| `AirFallbackActive` | Air | Allowed (bei validem Air/Cooling), sonst Blocked | Wartezeit abgelaufen (FallbackToAirAfterTimeout) oder bestaetigte manuelle Aktion oder Startersatz (Zeile 2, 6.5) | Rueckkehrbedingung erfuellt oder Air/Cooling wird ungueltig | `SensorSelectionEvent` beim Eintritt (`origin = Fallback`, siehe 6.11) |
| `ReturnValidationPending` | **Air** (Regelung laeuft unveraendert auf Luft weiter) | **Allowed**, solange Air/Cooling valide sind - die Rueckkehrpruefung selbst blockiert die laufende Luftregelung nicht | `AirFallbackActive` UND `ReturnStrategy = AutomaticValidatedReturnToProduct` UND Produkt wird valide | 6.10-Evidenz vollstaendig positiv (-> `NormalProduct`) oder Evidenz bricht ab (-> `AirFallbackActive`) oder Air/Cooling wird ungueltig (-> `SafeLocked`) | `SensorSelectionEvent` nur beim tatsaechlichen Wechsel nach `NormalProduct`; Abbruch selbst ist `SensorSelectionNotice` |
| `SafeLocked` | unveraendert (letzter Modus wird nicht weiter geregelt) | **Blocked, dauerhaft bis externe Aufloesung** | Policy = StopToSafeState, gleichzeitiger Air-/Cooling-Ausfall, oder unklare 6.10-Evidenz waehrend eines bereits freigegebenen Zustands | nur ueber denselben Pfad wie ein frischer Start dieser Bewertungskette | `SensorSelectionEvent`, falls der Eintritt mit einem Moduswechsel einherging; sonst `SensorSelectionNotice` |
| `RestartRevalidationPending` | letzter persistierter `activeMode` | **Blocked** | unmittelbar nach `restoreRunPersistenceSnapshot` (6.12) | erste vollstaendige Nachbewertung nach dem Neustart abgeschlossen | keines beim Eintritt selbst |

`SafeLocked` ist kein Fehlerzustand im Sinne von #24; er ist der
Selektor-interne "kein Modus ist sicher waehlbar"-Zustand und bleibt fuer
#24 als Verriegelungs-/Fehlerkandidat sichtbar.

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
                                               explizite Benutzerbestaetigung]
UserDecisionRequired -> SafeLocked           [Air/Cooling waehrenddessen
                                               ungueltig]

AirFallbackActive -> ReturnValidationPending [ReturnStrategy =
                                               AutomaticValidatedReturnToProduct
                                               UND Produkt wieder Valid]
AirFallbackActive -> NormalProduct           [ReturnStrategy =
                                               ManualReturnToProduct UND
                                               explizite validierte
                                               Benutzeraktion, nur wenn
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
  vorgesehene Bedingung erfuellt ist;
- jeder Uebergang nach `NormalProduct`/`AirFallbackActive` mit
  `peltierPermission = Allowed`, waehrend `RestartRevalidationPending`
  aktiv ist.

#### 6.4.3 Start der Fallback-Wartezeit

Die Wartezeit beginnt mit der monotonen Zeit der ersten Bewertung, in der
der Produkt-Snapshot als nicht nutzbar erkannt wird (`ProductFailureDetected`
wird betreten). Dieser Zeitpunkt wird nur im RAM als Teil der Auswahlphase
gehalten - nicht in `SensorQualitySnapshot` und nicht im persistierten
Sensorselektionszustand (6.12), der bewusst nur den groben Ursprung, nicht
den laufenden Wartezustand speichert.

#### 6.4.4 Erneut gueltiger Produktwert waehrend der Wartezeit

Wird der Produkt-Snapshot waehrend `ProductFailureDetected` (vor Ablauf der
Wartezeit) wieder `Valid`, kehrt der Zustand direkt zu `NormalProduct`
zurueck. Es fand nie ein tatsaechlicher Moduswechsel statt (`activeMode`
blieb durchgehend `Product`), deshalb entsteht **kein**
`SensorSelectionEvent` und **keine** neue Laufrevision - nur eine
`SensorSelectionNotice` ist zulaessig.

#### 6.4.5 Einmalige Verarbeitung manueller Aktionen

Jede Benutzeraktion traegt dieselbe `CommandEnvelope`-Idempotenzsicherung
wie bestehende Kommandos (`CommandId`, `expectedRunRevision`,
`processedCommandIds`-Fenster aus `RunCommandState`). Eine bereits
verarbeitete Aktion liefert `CommandStatus::AlreadyProcessed`.

#### 6.4.6 Idempotenz und doppelte Wechselereignisse

Eine wiederholte Bewertung ohne tatsaechliche Zustands- oder Modusaenderung
liefert `after == before` mit `event = std::nullopt`. Ein
`SensorSelectionEvent` entsteht ausschliesslich beim tatsaechlichen
Verlassen eines Zustands mit unterschiedlichem `activeMode`. Ein Eintritt in
einen Block- oder Diagnosezustand ohne Modusaenderung (z. B.
`ProductFailureDetected` betreten, `SafeLocked` ohne Moduswechsel betreten,
Rueckkehrvalidierung abgebrochen) erzeugt stattdessen eine
`SensorSelectionNotice` - beide Typen sind bewusst getrennt (6.11).

#### 6.4.7 Abbruch, Neustart und Wiederaufnahme der Rueckkehrvalidierung

- `ReturnValidationPending` wird abgebrochen (zurueck nach
  `AirFallbackActive`), sobald ein einzelnes Kriterium aus 6.10 nicht mehr
  erfuellt ist; der Abbruch selbst ist eine `SensorSelectionNotice`.
- Eine neue `ReturnValidationPending`-Phase beginnt immer bei Evidenz Null.
- Nach einem Neustart beginnt jede laufende Wartezeit- und
  Rueckkehrvalidierung zwingend bei Null (6.12); der **Ursprung** des
  aktuellen Modus (Start vs. Fallback) bleibt dagegen ab Schema 2
  rekonstruierbar und wird nicht verworfen.

#### 6.4.8 Revisions- und Kapazitaetsgrenzen

- Ein tatsaechlicher Wechsel verbraucht dieselbe Laufrevisions-/
  Kapazitaetspruefung wie bestehende Kommandos; bei erreichter Kapazitaet
  liefert die Entscheidung `CommandStatus::CapacityReached` und **keine**
  Modusaenderung.
- `SensorSelectionEvent`s und `-Notice`s werden nicht in einer eigenen
  unbegrenzten Liste gefuehrt; sie folgen den bestehenden begrenzten
  Meldungs- (`run_command_limits::kMaximumRuntimeMessages`) und
  Revisionsgrenzen des Laufsystems.

### 6.5 Vollstaendige Startmatrix

Vierdimensional: `SensorPreference` (Programm) × angeforderter
`RunSensorMode` × Produktverfuegbarkeit/-qualitaet × Pflichtqualitaet von
Luft und Kuehlkoerpersensor. Vorbedingung fuer jede Zeile: Air und Cooling
sind zum Startzeitpunkt `VALID`, sofern nicht anders vermerkt.

**Stabiler Ablehnungsstatus (loest Review-Praezisierungspunkt 4):** Ist Air
oder Cooling zum Startzeitpunkt nicht `VALID`, liefert der Start
unabhaengig von `SensorPreference` und angefordertem Modus immer genau
`CommandStatus::InvalidInput`. `CommandStatus::SafetyRejected` bleibt
ausschliesslich fuer den bereits bestehenden, extern berechneten
`request.safetyAllowsStart`-Vorabgate reserviert (unveraendert seit vor
#21). Sensorqualitaet ist ein #20/#21-Fachkriterium, kein externes
Safety-Gate, und wird deshalb konsequent als `InvalidInput` behandelt -
identisch zu jeder anderen in dieser Tabelle abgelehnten Kombination.

| `SensorPreference` | angeforderter `RunSensorMode` | Produkt | gueltig/abgelehnt | effektiver Startmodus | Ursprung | Bestaetigung | Ereigniswirkung | Peltier initial |
|---|---|---|---|---|---|---|---|---|
| ProductIfAvailableElseAir | Product | Valid | gueltig | Product | InitialSelection | Startzusammenfassung zeigt Product | keine | Allowed |
| ProductIfAvailableElseAir | Product | nicht Valid | gueltig, automatischer Ersatz | Air | **Fallback** | Startzusammenfassung zeigt Air + Hinweis "Produkt angefordert, nicht verfuegbar" | `StartSensorSelectionNotice` (kein `SensorSelectionEvent`, 6.11) | Allowed (auf Air) |
| ProductIfAvailableElseAir | Air | beliebig | gueltig | Air | InitialSelection | Startzusammenfassung zeigt Air | keine | Allowed |
| AirProductOptional | Product | Valid | gueltig | Product | InitialSelection | Startzusammenfassung zeigt Product | keine | Allowed |
| AirProductOptional | Product | nicht Valid | **abgelehnt** | - | - | `CommandStatus::InvalidInput`, keine stille Umdeutung auf Air | - | - |
| AirProductOptional | Air | beliebig | gueltig | Air | InitialSelection | Startzusammenfassung zeigt Air | keine | Allowed |
| ProductRequired | Product | Valid | gueltig | Product | InitialSelection | Startzusammenfassung zeigt Product | keine | Allowed |
| ProductRequired | Product | nicht Valid | **abgelehnt** | - | - | `CommandStatus::InvalidInput`, kein Start ohne Produkt | - | - |
| ProductRequired | Air | beliebig | **abgelehnt** | - | - | `CommandStatus::InvalidInput` | - | - |
| AirOnly | Product | beliebig | **abgelehnt** | - | - | `CommandStatus::InvalidInput` | - | - |
| AirOnly | Air | beliebig | gueltig | Air | InitialSelection | Startzusammenfassung zeigt Air, Produkt bleibt reine Anzeige | keine | Allowed |

Zusaetzliche verbindliche Regeln:

- `StartSummary.sensorMode` aendert seine Bedeutung von "angefordert" zu
  "effektiv, bereits gegen Programmpraeferenz validiert". Die effektive
  Modusableitung erfolgt **vor** der `NotConfirmed`-Rueckgabe in
  `decideProgramStart`.
- Keine inkompatible Kombination darf still auf eine andere umgedeutet
  werden.
- Zeile 2 ("Startersatz") setzt `origin = Fallback` fuer die neue
  `PersistedSensorSelectionState` (6.12), weil Produkt angefordert, aber
  nicht verfuegbar war - fachlich ein Ersatzbetrieb ab dem ersten Moment,
  fuer den die konfigurierte `ReturnStrategy` gilt (6.11). Alle anderen
  gueltigen Zeilen setzen `origin = InitialSelection`: Luft war dort von
  Anfang an der gewaehlte Modus ohne Produktversuch, und `ReturnStrategy`
  ("Rueckkehr **nach** Ersatzbetrieb") wird fuer solche Laeufe nie
  ausgewertet.
- Ein Startersatz (Zeile 2) verwendet die ohnehin beim Start erzeugte erste
  Laufrevision, keine zusaetzliche zweite Revision.

### 6.6 Produktfehler und Ersatzbetrieb

Im produktgefuehrten Lauf gilt:

```text
Produkt-Snapshot nicht nutzbar
  -> Peltierfreigabe sofort sperren
  -> Air und Cooling pruefen
  -> SensorSelectionNotice erzeugen (6.11)
  -> programmabhaengige Wartezeit abwarten (Start: 6.4.3)
  -> Strategie pruefen
  -> nur bei gueltigen festen Sensoren auf Air wechseln
```

Verbindliche Regeln:

- `Air` und `Cooling` muessen fuer jede Peltierfreigabe aktuell und `VALID`
  sein; ein gleichzeitiger Ausfall verhindert jeden Ersatzbetrieb
  (entschieden, kein Owner-Gate).
- Ein ungueltiger Produktwert allein macht den Luftsensor nicht automatisch
  zum Ersatzsensor.
- `FallbackToAirAfterTimeout` wechselt automatisch nach der programmierten
  Wartezeit, nie frueher und nicht aus einem einzelnen wieder gueltigen
  Produktwert (siehe 6.4.4).
- `WaitForUser` bleibt nach der Wartezeit ohne Luftwechsel, bis eine
  explizite, validierte Benutzerentscheidung vorliegt.
- `StopToSafeState` erzeugt keine Regelmodusfreigabe.
- Ist der Luft- oder Kuehlkoerpersensor nicht sicher gueltig, wird nicht auf
  Luft umgeschaltet. `peltierPermission` bleibt `Blocked`.
- Die Wartezeit ist ein konfigurierter Programwert; der reale Wert bleibt
  `TBD_COMMISSIONING`.

### 6.7 Rueckkehr zum Produktfuehler

Eine Rueckkehr ist nur erlaubt, wenn:

- die konfigurierte `ReturnStrategy` (6.2) die Rueckkehr erlaubt;
- der Produkt-Snapshot `VALID` ist und einen verwendbaren Filterwert besitzt;
- #20s Wiedererkennungsregeln bereits mehrere plausible Proben und die
  geforderte Stabilitaet belegen;
- der vollstaendige `CrossRolePlausibilityContext` (6.10) keine
  widerspruechliche oder fehlende Evidenz liefert;
- Air und Cooling weiterhin gueltig sind;
- keine ungeklaerte konkurrierende Safety-/Fehlerlage besteht.

Die drei Strategien (Programmschemafeld, 6.2):

```text
remain_on_air_until_end
  -> aktive Luftregelung bleibt bis zum Laufende erhalten; jede
     Rueckkehranfrage wird abgelehnt

manual_return_to_product
  -> nur explizite Benutzeraktion nach vollstaendiger Validierung erlaubt

automatic_validated_return_to_product
  -> nach #20-VALID, Stabilitaet und vollstaendiger 6.10-Plausibilitaet
     automatisch auf Produkt wechseln
```

### 6.8 Produktgefuehrte manuelle Laeufe

**Entscheidung fuer Release 1: produktgefuehrte manuelle Laeufe werden
unterstuetzt, mit einem festen, nicht konfigurierbaren Vertrag:**

```text
Manueller Produktmodus (RunSensorMode::Product bei ManualStartRequest):
- Produktfehler sperrt die Peltierfreigabe sofort (wie 6.6);
- kein automatischer Luftfallback: fest wie
  ProductSensorFailurePolicy::WaitForUser;
- Fortsetzung mit Luft nur durch explizite, validierte Benutzeraktion, ohne
  vorherige Wartezeitphase;
- Rueckkehr zum Produkt nur manuell nach vollstaendiger Validierung; fest
  wie ReturnStrategy::ManualReturnToProduct.
```

Diese Konstanten werden **nicht** in `ManualRunPlanRequest` als neue Felder
eingefuehrt: `ManualRunPlan` bleibt unveraendert in
`RunPersistenceSnapshot::manual` persistiert. Der Selektor behandelt
`RunSensorMode::Product` bei einem manuellen Lauf
(`RunCommandState::activeManualRun.has_value()`) als impliziten, fest
verdrahteten Policy-/Strategie-Kontext, `origin = InitialSelection` fuer
den manuellen Start selbst.

### 6.9 Phasen, Kuehlen und Halten

Der Wechsel von `FERMENTING` nach `COOLING` oder `COOL_HOLDING` loest keine
automatische Neuauswahl aus (bereits durch `docs/TEMPERATURE_CONTROL.md`
und `docs/RECOVERY_AND_INTERRUPTION.md` entschieden, kein Owner-Gate). Ein
Sensorfehler waehrend `COOLING`/`COOL_HOLDING` durchlaeuft denselben
Sicherheits- und Policyvertrag wie in `FERMENTING`.

### 6.10 Rollenuebergreifende Plausibilitaetspruefung

**Korrektur gegenueber Revision 2:** `CrossRolePlausibilityContext` bildet
den vollstaendigen in `docs/TEMPERATURE_CONTROL.md` und
`docs/RECOVERY_AND_INTERRUPTION.md` geforderten Vertrag typisiert ab, statt
ihn auf die in #21 bereits verfuegbare Teilmenge zu reduzieren:

```cpp
enum class AbstractControlDirection : std::uint8_t {
    Unknown,
    Heating,
    Cooling,
    Idle,
};

struct ThermalCommissioningProfileRef {
    // TBD_HARDWARE/TBD_COMMISSIONING-gated Referenz auf ein spaeter
    // erst nach realer Vermessung vorhandenes Profil; in #21 nie befuellt.
};

struct CrossRolePlausibilityContext {
    ProcessState phase;
    std::uint64_t evaluationMonotonicMillis;
    SensorQualitySnapshot air;
    SensorQualitySnapshot product;
    SensorQualitySnapshot cooling;
    AbstractControlDirection direction{AbstractControlDirection::Unknown};
    std::optional<std::uint64_t> controlDemandAgeMs;
    std::optional<ThermalCommissioningProfileRef> thermalProfile;
};
```

`phase`, die drei `SensorQualitySnapshot`s (mit `filteredCelsius`,
`lastValidSampleAgeMs`, `changeRateCelsiusPerSecond`,
`recoveryProgressCount`) sind bereits heute befuellbar. `direction`,
`controlDemandAgeMs` und `thermalProfile` sind erst mit #22/#23 real
befuellbar; der **Typvertrag** entsteht jedoch vollstaendig in #21, damit
#22/#23 nur noch Produzenten anschliessen, ohne API oder Safety-Semantik
erneut materiell zu aendern.

**Verbindliche Regel (keine Abschwaechung, nicht mehr Owner-Gate):**
`automatic_validated_return_to_product` verlangt zusaetzlich zu den in 6.7
gelisteten Kriterien:

```text
direction != Unknown
UND controlDemandAgeMs vorhanden
UND thermalProfile vorhanden
UND direction/controlDemandAgeMs/thermalProfile mit dem beobachteten
    Produkt-/Luftverhalten vereinbar (Grenzwerte TBD_COMMISSIONING)
```

Ist eine dieser Bedingungen nicht erfuellbar, bleibt die automatische
Rueckkehr gesperrt (`ReturnValidationPending -> AirFallbackActive`, 6.4.7).
Der Auswahlkern wird nativ getestet, indem diese drei Felder synthetisch
befuellt werden (9.2) - ohne #22/#23 zu implementieren.

**Konsequenz, die dem Owner explizit vorgelegt wird statt sie zu
verstecken:** Solange #22/#23 nicht existieren, liefert kein produktiver
Aufrufer `direction`/`controlDemandAgeMs`/`thermalProfile`. Das macht
`automatic_validated_return_to_product` - die Werksvorgabe aller vier
Katalogprogramme (6.2.5) - in Release 1 **praktisch inert**: keine
automatische Rueckkehr findet statt, bis #22/#23 die Produzenten liefern.
Bis dahin ist die manuelle Rueckkehr (Benutzeraktion, wie bei
`manual_return_to_product`) der einzige wirksame Rueckkehrpfad, obwohl das
Programm formal `automatic_validated_return_to_product` konfiguriert hat.
Dies ist eine **Abhaengigkeitsaussage**, keine Ownerfreigabe eines
abgeschwaechten Vertrags (13, P21-M4).

### 6.11 Ereignis-, Meldungs- und Revisionsvertrag

Zwei getrennte, schmale Typen statt eines einzigen:

```cpp
enum class SensorSelectionEventCause : std::uint8_t {
    None,
    ProductFailureFallback,
    AutomaticValidatedReturn,
    ManualUserFallback,
    ManualUserReturn,
};

struct SensorSelectionEvent {
    // Tatsaechlicher Wechsel zwischen zwei bereits aktiven Modi waehrend
    // eines laufenden Prozesses. beforeMode ist IMMER gesetzt.
    RunSensorMode beforeMode;
    RunSensorMode afterMode;
    SensorSelectionEventCause cause;
    std::uint32_t runRevision;
    std::uint64_t monotonicMillis;
    std::optional<std::int64_t> utcUnixSeconds;
    // Sensorqualitaet/Alter der drei Rollen als Evidenzreferenz, nicht als
    // scheinbar aktueller Wert bei FAILED.
};

struct StartSensorSelectionNotice {
    // Effektive Startauswahl - KEIN Wechsel zwischen zwei aktiven Modi,
    // taeuscht deshalb keinen nie aktiven Produktmodus vor (loest
    // Review-Praezisierungspunkt 6).
    RunSensorMode requestedMode;
    RunSensorMode effectiveMode;
    std::uint32_t runRevision;
};

struct SensorSelectionNotice {
    // Diagnose-/Blockmeldung OHNE Moduswechsel (Eintritt in
    // ProductFailureDetected, Eintritt in SafeLocked ohne Moduswechsel,
    // abgebrochene Rueckkehrvalidierung, Produkt waehrend Wartezeit wieder
    // valide). Kein SensorSelectionEvent, keine neue Laufrevision.
};
```

`SensorSelectionEvent`/`StartSensorSelectionNotice`/`SensorSelectionNotice`
werden ueber die bereits bestehende `runRevision`-Zahl an den Lauf
gebunden - **nicht** ueber `RunRevision`/`RunChangeReason`
(`run_snapshot.hpp`), das fuer Zieltemperatur-/Restdauer-Anpassungen gebaut
ist und keinen Sensormodus kennt.

Die bestehende runtime-seitige Meldungsstruktur (`MessageCode`,
`run_commands.hpp`) wird um passende Codes ergaenzt. Ein fehlgeschlagenes
Journal-Write darf keine Aktorfreigabe erzeugen. Die dauerhafte Aufbewahrung
und Exportform des Journals bleibt Scope von #19.

### 6.12 Persistierter Sensorselektionszustand und Neustart-Vertrag

**Vollstaendige Neufassung gegenueber Revision 2.** Revision 2 verzichtete
bewusst auf ein neues persistiertes Feld und sperrte stattdessen nach jedem
Neustart jede Rueckkehr, sobald `activeRunSensorMode == Air`. Der Review hat
das explizit als unzulaessigen Normalvertrag fuer neue Datensaetze
zurueckgewiesen: die konfigurierte `ReturnStrategy` muss nach einem
gewoehnlichen Neustart weiterhin korrekt gelten, nicht nur bei Datensaetzen,
die den neuen Vertrag gar nicht kennen.

#### 6.12.1 Persistierter Zustand

`lib/fermentation_app/src/run_commands.hpp` (bzw. `run_persistence_contract.hpp`,
gemeinsam mit `RunSensorMode` in `run_commands.hpp` definiert, damit sowohl
der Selektor als auch die Persistenzschicht ohne zirkulaere Abhaengigkeit
darauf zugreifen):

```cpp
enum class SensorSelectionOrigin : std::uint8_t {
    InitialSelection,
    Fallback,
};

struct PersistedSensorSelectionState {
    RunSensorMode activeMode;
    SensorSelectionOrigin origin{SensorSelectionOrigin::InitialSelection};
    SensorSelectionEventCause lastEventCause{SensorSelectionEventCause::None};
    std::uint32_t lastEventRunRevision{0U};
};
```

Bewusst **nicht** enthalten: Wartezeit-Startzeitpunkt, laufende
Rueckkehrvalidierungs-Teilergebnisse, GPIO- oder Aktorzustaende. Diese
bleiben RAM-only (6.4.3, 6.4.7) und beginnen nach jedem Neustart bei Null -
das ist unveraendert gegenueber Revision 2 und wird hier nicht
zurueckgenommen. Persistiert wird ausschliesslich, was noetig ist, um nach
dem Neustart zu wissen, **ob ueberhaupt ein Ersatzbetrieb vorliegt und
welche `ReturnStrategy` dafuer gilt** - nicht, wie weit eine laufende
Validierung fortgeschritten war.

`RunCommandState` erhaelt `std::optional<PersistedSensorSelectionState>
sensorSelection` (analog zu `activeRunSensorMode`, `nullopt` fuer
`NoActiveRun`). `RunPersistenceSnapshot` erhaelt dasselbe Feld.
`makeRunPersistenceSnapshot`/`restoreRunPersistenceSnapshot`
(`run_persistence_contract.cpp`) kopieren es wie `activeRunSensorMode`.

#### 6.12.2 Schema-Bump und Migration (`kRunPersistenceSchema`)

```text
kRunPersistenceSchema: 1 -> 2
```

Die beiden bestehenden, unabhaengig definierten Konstanten (`run_persistence_codec.cpp`
Zeile 22 und `run_persistence_coordinator.cpp` Zeile 13, aktuell zufaellig
identisch `1U`) werden im selben Zug zu einer einzigen gemeinsamen
Konstante in `run_persistence_contract.hpp` zusammengefuehrt - eine kleine,
durch diese Aenderung ohnehin erzwungene DRY-Korrektur, kein zusaetzlicher
Scope.

Der Kopf-Datensatz (`RunPersistenceHead`) bleibt strukturell unveraendert -
nur das Checkpoint-Payload (`RunPersistenceSnapshot`) waechst um das neue
Feld. Der Versionsstempel wird trotzdem fuer beide Datensatztypen
synchron auf 2 angehoben, um nicht zwei unabhaengige Versionszaehler fuer
denselben Persistenzgenerationswechsel zu fuehren (DRY). Kopf-Decodierung
fuer Version 1 und Version 2 ist bytegleich; es ist keine echte
Kopf-Migration noetig, nur eine erweiterte Versionsakzeptanz.

Migration (ein Schritt, folgt derselben Copy-Migrationskonvention wie 6.2.2,
hier ohne Schrittkette, weil nur eine Vorgaengerversion existiert):

- **Legacy-Decoder beibehalten:** die bestehende `decodeRunPersistenceSnapshot`-
  Logik (Schema-1-Layout, kein `sensorSelection`-Feld auf dem Wire) bleibt
  als interner Legacy-Pfad erhalten.
- **Neuer Decoder fuer Schema 2:** liest zusaetzlich `sensorSelection`
  (optional-getaggt wie `activeRunSensorMode`).
- **Dispatch:** die Envelope-`schemaVersion` entscheidet, welcher Decoder
  laeuft. Bei Schema 1 wird das Ergebnis mit `sensorSelection = std::nullopt`
  markiert (das ist die alleinige, korrekte Bedeutung von "Ursprung
  unbekannt" - keine separate Markierung noetig).
- Alle strikten `!= kRunPersistenceSchema`-Gleichheitspruefungen
  (`run_persistence_codec.cpp` ~718, 881, 950, `runCheckpointReferenceMatches`
  963-975, sowie die Ladepruefungen in `run_persistence_coordinator.cpp::loadAndInitialize`)
  werden von strikter Gleichheit mit der aktuellen Konstante auf "bekannte
  Version im Bereich [1, 2]" umgestellt, exakt spiegelbildlich zu
  `readProgramSchema`s Umstellung (6.2.4).
- **Alle neuen Schreibvorgaenge stempeln immer Schema 2** - unveraendert
  gegenueber dem bestehenden Muster, `writeSnapshot` kennt ohnehin nur die
  aktuelle Konstante beim Schreiben.

#### 6.12.3 Restart-Vertrag

```text
Peltier gesperrt
-> Auswahlzustand wird beim Restore IMMER auf RestartRevalidationPending
   gesetzt (RAM-Default, unabhaengig vom vor dem Neustart erreichten
   RAM-Zustand)
-> persistierter sensorSelection-Zustand (falls vorhanden) liefert
   activeMode-Ursprung und letzte Ereignisursache
-> keine unbekannte Wartezeit anrechnen: eine erneut erkannte
   Produktnichtnutzbarkeit startet die Wartezeit bei 0 (6.4.3)
-> vollstaendige Sensor- und Rueckkehrvalidierung erneut durchfuehren,
   ReturnValidationPending beginnt immer bei Evidenz Null (6.4.7)
-> erst nach atomar persistierter Entscheidung (6.14) freigeben
```

**Normalfall (Schema-2-Datensatz, `sensorSelection` vorhanden):**
`origin == Fallback` UND die konfigurierte `ReturnStrategy != RemainOnAirUntilEnd`
-> automatische/manuelle Rueckkehr wird nach dem Neustart normal gemaess
6.7/6.10 geprueft, keine pauschale Sperre. `origin == InitialSelection` ->
Rueckkehr war nie relevant und bleibt es (6.5).

**Rueckfall (Legacy-Datensatz, `sensorSelection == std::nullopt` -
ausschliesslich fuer aus Schema 1 migrierte Datensaetze, siehe 6.12.2):**
`activeRunSensorMode == Air` UND Ursprung unbekannt -> automatische UND
manuelle Rueckkehr bleiben fuer den Rest dieses Laufs gesperrt (wirkt wie
`remain_on_air_until_end`), mit sichtbarer Diagnosemeldung. Dieser
Rueckfall gilt **ausschliesslich** fuer migrierte Alt-Datensaetze, nicht
mehr als Normalvertrag fuer neu geschriebene Schema-2-Records - das ist die
konkrete Korrektur des Reviewbefunds gegenueber Revision 2.

`restoreRunPersistenceSnapshot` liefert weiterhin ein `RunCommandState`; ein
Test bestaetigt: Restore eines Schema-2-Datensatzes mit `origin = Fallback`
liefert nach vollstaendiger Nachvalidierung eine normal funktionierende
Rueckkehrpruefung; Restore eines migrierten Schema-1-Datensatzes liefert
dauerhaft gesperrte Rueckkehr mit sichtbarer Diagnose.

Direkte GPIO- oder alte Aktorzustaende werden weiterhin nicht gespeichert.

### 6.13 Zentrale Cross-Field-Validierung

`validateProgram` (bzw. eine neue, von dort aufgerufene Funktion in
`program_model.cpp`) prueft zusaetzlich zu den bestehenden Einzelfeldregeln:

1. `SensorPreference::ProductRequired` + `ProductSensorFailurePolicy::
   FallbackToAirAfterTimeout` -> `IncompatibleCombination` (entschieden,
   kein Owner-Gate).
2. `SensorPreference::AirOnly` + `ReturnStrategy != RemainOnAirUntilEnd` ->
   `IncompatibleCombination`.
3. `SensorPreference::AirOnly` + `ProductSensorFailurePolicy !=
   FallbackToAirAfterTimeout` -> `IncompatibleCombination`. Die einzige
   kanonische Kombination fuer `AirOnly` ist `FallbackToAirAfterTimeout` +
   `ReturnStrategy::RemainOnAirUntilEnd` (semantisch inert, da
   `AirOnly`-Laeufe `ProductFailureDetected`/`AirFallbackActive`/
   `ReturnValidationPending` nie betreten, siehe 6.5).
4. `SensorPreference::AirOnly` + `fallbackDelaySeconds.has_value()` ->
   `UnexpectedValue` (bestehender Code wiederverwendet, gleicher Regel-Shape
   wie die bestehende `maximumProductWaitMinutes`/`preheat`-Regel). Dies
   **aendert eine bereits bestehende Regel**: die heutige
   `ValidationPurpose::Runnable`-Pflicht fuer `fallbackDelaySeconds` bei
   `policy == FallbackToAirAfterTimeout` (`program_model.cpp` ~209-216)
   wird auf `sensorPreference != AirOnly` bedingt, weil der Wert fuer
   `AirOnly` nie ausgewertet wird und deshalb nicht erfunden werden darf.
   Betroffen: das bestehende Payload-Maximierungs-Fixture in
   `test_configuration_codecs.cpp` (`maximizeProgramPayload`, siehe
   Abschnitt 3) muss fuer die `AirOnly`-Dimension einen anderen
   Sensorpraeferenzwert verwenden, um weiterhin einen maximalen
   `fallbackDelaySeconds`-Wert im Payload zu erzeugen.
5. `ProductSensorFailurePolicy::FallbackToAirAfterTimeout` (bei
   `sensorPreference != AirOnly`) ohne gueltige `fallbackDelaySeconds` bei
   `ValidationPurpose::Runnable` - bereits bestehende Regel, hier nur
   referenziert.

Diese Validierung gehoert in die bestehende kanonische
Modell-/Konfigurationskette und entsteht nicht als Parallelvertrag im
Laufselektor.

### 6.14 Atomarer Persistenzpfad fuer Sensorentscheidungen

**Neuer Abschnitt (Review-Befund 1).** `RunPersistenceCoordinator` besitzt
bislang genau zwei Mutationspfade, `persistCommand` (Benutzerkommando) und
`persistTransition` (Prozesszustandsuebergang). Ein automatisch vom
Selektor ausgeloester Sensorwechsel ist keins von beidem und braucht einen
eigenen, gleichwertigen dritten Pfad.

#### 6.14.1 Neue Typen

```cpp
enum class RunPersistenceMutationKind : std::uint8_t {
    Command,
    Transition,
    SensorSelection,   // neu
};

enum class RunCheckpointTrigger : std::uint8_t {
    Command,
    Transition,
    Periodic,
    SensorSelection,   // neu
};
```

Beide sind additive Enum-Erweiterungen mit eigenen Wire-IDs im bestehenden
Schema-1-Payload (analog zum bereits vorhandenen Wire-ID-Muster fuer
`RunChangeReason`) und erzwingen fuer sich genommen **keinen**
Schemabump - der Bump auf Schema 2 (6.12.2) wird ausschliesslich durch das
neue `sensorSelection`-Struktur-Feld in `RunPersistenceSnapshot` ausgeloest.

#### 6.14.2 `writeSnapshot`-Korrektur

Die private Methode
`RunPersistenceCoordinator::writeSnapshot(...)` erhaelt einen expliziten
`RunPersistenceMutationKind`-Parameter statt ihn intern aus
`commandId.has_value()` abzuleiten:

```cpp
RunPersistenceResult writeSnapshot(
    const RunPersistenceSnapshot& snapshot, const RunCheckpointTime& time,
    bool periodic, const RunCommandState& before,
    std::optional<CommandId> commandId,
    RunPersistenceMutationKind mutationKind);  // neu, statt Ableitung
```

`persistCommand` ruft weiterhin mit `RunPersistenceMutationKind::Command`
auf, `persistTransition` mit `Transition`. Der bestehende Kopf-Invarianten-
Check in `run_persistence_codec.cpp` Zeile 789
(`(head.mutationKind == RunPersistenceMutationKind::Command) !=
head.commandId.has_value()`) bleibt unveraendert korrekt: er prueft nur die
Command-Richtung der Aequivalenz, und fuer `SensorSelection` gilt wie fuer
`Transition` "keine `commandId`" - verifiziert, keine Anpassung noetig.

#### 6.14.3 `persistSensorSelection`

```cpp
[[nodiscard]] RunPersistenceResult persistSensorSelection(
    RunCommandState& current, const SensorSelectionDecision& decision,
    const RunCheckpointTime& time);
```

Folgt exakt der bestehenden Transaktionsreihenfolge (identisch zu
`persistCommand`/`persistTransition`, aus `run_persistence_coordinator.cpp`
verifiziert):

```text
Entscheidung validieren (expectedRunRevision, before-Konsistenz)
-> Kandidatenkopie erzeugen, Sensorwechsel darauf anwenden
   (activeRunSensorMode, sensorSelection, runRevision)
-> makeRunPersistenceSnapshot(..., RunCheckpointTrigger::SensorSelection, ...)
-> writeSnapshot(..., periodic=false, ..., commandId=std::nullopt,
   RunPersistenceMutationKind::SensorSelection)
-> bei Erfolg: identische Mutation auf `current` anwenden (RAM-Commit)
-> RunPersistenceResult mit ggf. SensorSelectionEvent/-Notice liefern
```

Ein fehlgeschlagener Schritt (Stale-Revision, Schreibfehler, Kapazitaet)
liefert denselben Status-/Reason-Vokabular wie die bestehenden Pfade
(`RunPersistenceResultStatus`, `RunPersistenceStep`,
`RunPersistenceTechnicalReason`) und aendert weder RAM noch Aktoranforderung
- konsistent mit dem `Entscheidung validieren -> ... -> erst danach neue
Aktorfreigabe ableiten`-Vertrag.

#### 6.14.4 Datei- und Testschnitt

- `run_persistence_contract.hpp/.cpp`: `PersistedSensorSelectionState`,
  `SensorSelectionOrigin`, `SensorSelectionEventCause`, `RunPersistenceSnapshot`-
  Erweiterung, `makeRunPersistenceSnapshot`/`restoreRunPersistenceSnapshot`-
  Anpassung, `validateRunPersistenceSnapshot`-Erweiterung.
- `run_persistence_codec.hpp/.cpp`: gemeinsame `kRunPersistenceSchema`-
  Konstante, Legacy-/Schema-2-Decoder, `RunCheckpointTrigger`/
  `RunPersistenceMutationKind`-Wire-IDs, Bereichspruefungen statt
  Gleichheitspruefungen.
- `run_persistence_coordinator.hpp/.cpp`: `persistSensorSelection`,
  `writeSnapshot`-Signaturaenderung, Ladepruefungen im Bereich [1, 2].
- direkt betroffene Tests unter `test/test_run_persistence_coordinator/`,
  `test/test_run_checkpoint_codec/`.

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
`ReturnStrategy`, `SensorSelectionOrigin`, `FERMENTING` oder `RunSensorMode`.
Der Selektor bleibt in `fermentation_app`. `SensorSelectionEventCause` wird
in `run_commands.hpp` definiert (bereits gemeinsame Abhaengigkeit von
Selektor und Persistenzschicht), damit `run_persistence_contract.hpp`
nicht auf `sensor_selection.hpp` verweisen muss - die Persistenzschicht
bleibt unterhalb des Entscheidungsdienstes, nicht umgekehrt abhaengig.

Alle Programmschema-, Codec-, Katalog- und Persistenzaenderungen bleiben
vollstaendig innerhalb `fermentation_app`; keine Grenze aus ADR-013 wird
beruehrt.

## 8. Voraussichtlicher Datei- und Commit-Schnitt

Gegenueber Revision 2 wird der bisherige "Commit 3" wegen seines
gewachsenen Umfangs in zwei unabhaengig ueberpruefbare und testbare Commits
gesplittet (Persistenzmechanik getrennt von Coordinator-/Kommandointegration).

### Commit 1 - Programmschema: Rueckkehrstrategie und verkettete Migration (6.2, 6.13)

- `lib/fermentation_app/src/program_model.hpp`: `ReturnStrategy`-Enum,
  `ProgramField::ReturnStrategy`, `kSchema5RequiredProgramFields`-
  Umbenennung, neues `kCurrentRequiredProgramFields`,
  `kCurrentProgramSchemaVersion` 6, `kMinimumMigratableProgramSchemaVersion`
  (ersetzt `kMigratableProgramSchemaVersion`), `kProductWaitFieldIntroducedInSchema`,
  `kReturnStrategyFieldIntroducedInSchema`, neuer
  `ValidationErrorCode::IncompatibleCombination`;
- `lib/fermentation_app/src/program_model.cpp`: `kRequiredFields`
  17. Eintrag, `validReturnStrategy`, Cross-Field-Regeln (6.13),
  `migrateProgramSchema4To5`/`migrateProgramSchema5To6`-Schrittfunktionen
  und die verkettete `migrateProgramToCurrentSchema`;
- `lib/fermentation_app/src/configuration_document_codec.cpp`: Wire-ID-Paar
  fuer `ReturnStrategy`, `readProgramSchema`-Bereichsakzeptanz,
  `readProgram`-Migrationsausloeser (`< kCurrentProgramSchemaVersion`),
  Korrektur von `readProgramLimitsAndCompletion` auf absolute
  Feldeinfuehrungskonstanten, Payload-Groessenberechnung;
- `lib/fermentation_app/src/standard_program_catalog.cpp`: alle vier
  Werksprogramme -> `ReturnStrategy::AutomaticValidatedReturnToProduct`;
- `config/programs.example.yaml`: `schema_version: 6`, `return_strategy`
  je Programm, `allowed_values.return_strategy`;
- `test/test_program_models/test_program_models.cpp`: Validierung,
  verkettete Migration (4->5->6 und 5->6), Cross-Field-Regeln,
  `AirOnly`-Migrationsnormalisierung;
- `test/test_configuration_codecs/test_configuration_codecs.cpp`:
  Codec-Round-Trip, Schema-Bereichsakzeptanz/-ablehnung,
  **Korrektur von `maximizeProgramPayload`** (Abschnitt 3, 6.13 Regel 4);
- `test/test_configuration_migration/test_configuration_migration.cpp`:
  Migrationsmatrix fuer alle vier `SensorPreference`-Werte, beide
  Kettenschritte einzeln und end-to-end;
- mechanische Folgeanpassungen an weiteren Testfixtures mit expliziter
  `presentFields`-Maske (vorab vollstaendig durchsucht, siehe 11).

### Commit 2 - Auswahlkern und direkte native Unit-Tests (6.1, 6.3, 6.4, 6.6, 6.7, 6.10)

- neu: `lib/fermentation_app/src/sensor_selection.hpp/.cpp`;
- neu: `lib/fermentation_app/src/sensor_selection_limits.hpp`, nur falls
  fuer firmwarefeste Validierungsobergrenzen erforderlich;
- neu: `test/test_sensor_selection/test_sensor_selection.cpp`;
- gegebenenfalls `lib/fermentation_app/CMakeLists.txt` nur fuer eine
  notwendige, bestehende Modulregistrierung.

Inhalt: Werttypen, reine Entscheidung, vollstaendiger Zustandsautomat (6.4),
Start-/Fallback-/Rueckkehrmatrix, vollstaendiger
`CrossRolePlausibilityContext` (6.10) inklusive synthetisch befuellter
Richtungs-/Zeit-/Thermik-Testfaelle, Sicherheitsvoraussetzungen,
Zeitvergleich, Rollenvergleich und fehlgeschlagene/unklare Eingaben. Keine
Persistenz und keine Aktoradapter.

### Commit 3 - Persistenzmechanik: Schema, Migration, atomarer Sensorpfad (6.12, 6.14)

- `lib/fermentation_app/src/run_persistence_contract.hpp/.cpp`:
  `PersistedSensorSelectionState`, `SensorSelectionOrigin`,
  `RunPersistenceSnapshot`-Erweiterung, `makeRunPersistenceSnapshot`/
  `restoreRunPersistenceSnapshot`-Anpassung;
- `lib/fermentation_app/src/run_commands.hpp`: `SensorSelectionEventCause`,
  `SensorSelectionEvent`, `StartSensorSelectionNotice`,
  `SensorSelectionNotice`, `RunCommandState::sensorSelection`-Feld;
- `lib/fermentation_app/src/run_persistence_codec.hpp/.cpp`: gemeinsame
  `kRunPersistenceSchema`-Konstante (Schema 2), Legacy-/Schema-2-Decoder,
  neue Wire-IDs fuer `RunCheckpointTrigger::SensorSelection`/
  `RunPersistenceMutationKind::SensorSelection`, Bereichspruefungen statt
  Gleichheitspruefungen (6.12.2, 6.14.1);
- `lib/fermentation_app/src/run_persistence_coordinator.hpp/.cpp`:
  `persistSensorSelection`, `writeSnapshot`-Signaturaenderung
  (`RunPersistenceMutationKind`-Parameter), Ladepruefungen im
  Versionsbereich [1, 2];
- `test/test_run_checkpoint_codec/test_run_checkpoint_codec.cpp`:
  Schema-2-Round-Trip, Legacy-Schema-1-Migration beim Decode,
  Ablehnung von Versionen ausserhalb [1, 2];
- `test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp`:
  `persistSensorSelection`-Transaktionsreihenfolge, `mutationKind`-Korrektheit,
  Restart mit Schema-2- und mit migriertem Legacy-Datensatz.

### Commit 4 - Lauf- und Startvertragsanschluss (6.5, 6.8, 6.9, 6.11)

- `lib/fermentation_app/src/run_commands.hpp/.cpp`: Startmatrix-Pruefung in
  `decideProgramStart` (vor der `NotConfirmed`-Rueckgabe), fester
  `RunSensorMode::Product`-Vertrag fuer manuelle Laeufe (6.8), Verdrahtung
  von `sensor_selection` in den Laufkommandopfad (RAM-`selectionPhase`,
  Aufruf von `persistSensorSelection` bei tatsaechlichem Wechsel);
- direkt betroffene Tests unter `test/test_run_commands/`.

### Commit 5 - fachliche Dokumentation und Abschlussnachweise

- `docs/TEMPERATURE_CONTROL.md`, `docs/RECOVERY_AND_INTERRUPTION.md` (inkl.
  Restart-/Migrationsvertrag), `docs/SAFETY_COMPONENT_FAULTS.md`,
  `docs/LOCAL_RUNTIME_UI.md`/`docs/DIAGNOSTICS_AND_MAINTENANCE.md`,
  `docs/ACCEPTANCE_TESTS.md`, `CHANGELOG.md` (knapper Eintrag),
  `docs/ROADMAP.md` (nur bei tatsaechlicher Status-/Gateaenderung).

Keine ADR-, Issue-, Hardware- oder Bibliotheksdatei wird ohne explizite
Ownerfreigabe angelegt oder geaendert.

## 9. Teststrategie und Testmatrix

In der Planungsphase werden keine Builds und keine produktiven Testlaeufe
ausgefuehrt. Nach der Planfreigabe gelten waehrend der Umsetzung nur gezielte
Tests. Die vollstaendige CI startet ausschliesslich durch den Owner auf
`Ready for review`.

### 9.1 Programmschema, verkettete Migration, Codec (Commit 1)

- neues `ReturnStrategy`-Feld ist Pflichtfeld ab Schema 6; fehlend ->
  `MissingRequiredField`; ungueltiger Wert -> `InvalidEnumValue`;
- alle Regeln aus 6.13 einzeln als ablehnender Testfall;
- **Migration 4 -> 5 -> 6 verkettet**: ein Schema-4-Dokument wird ueber
  beide Schritte korrekt migriert (nicht mehr als `UnsupportedSourceVersion`
  abgelehnt);
- Migration 5 -> 6 einzeln fuer alle vier `SensorPreference`-Werte: drei
  ergeben `AutomaticValidatedReturnToProduct`, `AirOnly` ergibt
  `RemainOnAirUntilEnd` **und** normalisiert Policy/`fallbackDelaySeconds`
  (6.2.2);
- ein Schema-4-`AirOnly`-Dokument mit gesetztem `fallbackDelaySeconds` und
  abweichender Policy bleibt nach der Kette gueltig (Normalisierungstest);
- Migration von einer Version ausserhalb [4, 6] -> `UnsupportedSourceVersion`;
- Codec-Round-Trip fuer alle drei `ReturnStrategy`-Werte;
- **Regressionstest fuer den in Abschnitt 3 verifizierten Bug:** ein
  Schema-5-Dokument mit gesetztem `maximumProductWaitMinutes` wird nach dem
  Bump auf Schema 6 weiterhin korrekt decodiert (kein Feld-Versatz);
- korrigiertes `maximizeProgramPayload`-Fixture bleibt unter
  `validateProgramCatalog(...) == Success` und maximiert weiterhin
  `fallbackDelaySeconds` ueber eine nicht-`AirOnly`-Praeferenz.

### 9.2 Unit-Tests des Auswahlkerns (Commit 2)

- jede Zeile der Startmatrix (6.5) einzeln, inklusive Ursprungszuweisung
  (`InitialSelection`/`Fallback`);
- Produkt `STALE`/`FAILED` sperrt Peltier sofort;
- Produktfehler vor Ablauf der Wartezeit bleibt ohne Luftwechsel; Produkt
  wird waehrend der Wartezeit erneut `Valid` -> `NormalProduct` ohne
  Event, nur `SensorSelectionNotice` (6.4.4);
- `FallbackToAirAfterTimeout` wechselt erst nach exakter monotoner Zeit;
  `WaitForUser` wartet auch nach Timeout; `StopToSafeState` liefert keine
  Luftfreigabe;
- ungueltige Luft/Kuehlkoerper sperrt Ersatzbetrieb bzw. jede
  Peltierfreigabe; gleichzeitiger Ausfall liefert keinen Ersatzmodus;
- `remain_on_air_until_end` verhindert jede automatische UND manuelle
  Rueckkehranfrage;
- `manual_return_to_product` lehnt fehlende/unbestaetigte/ungueltige
  Benutzeraktionen ab;
- **automatische Rueckkehr bleibt gesperrt, wenn `direction`,
  `controlDemandAgeMs` oder `thermalProfile` fehlen** (6.10, kein
  abgeschwaechter Ersatzvertrag mehr);
- automatische Rueckkehr wird nur zugelassen, wenn alle 6.7/6.10-Kriterien
  inklusive synthetisch befuellter Richtungs-/Zeit-/Thermikdaten erfuellt
  sind;
- `ReturnValidationPending`: Peltierfreigabe bleibt `Allowed` auf Luft,
  solange Air/Cooling valide sind, waehrend die Rueckkehr geprueft wird
  (Review-Praezisierungspunkt 2);
- Rueckkehrvalidierung wird abgebrochen und beginnt bei erneutem Versuch bei
  Evidenz Null; Abbruch erzeugt nur `SensorSelectionNotice`, kein Event;
- Eintritt in `SafeLocked` ohne Moduswechsel erzeugt nur eine
  `SensorSelectionNotice`, kein `SensorSelectionEvent` (Review-Punkt 3);
- Startersatz (Startmatrix Zeile 2) erzeugt `StartSensorSelectionNotice`
  mit `requestedMode != effectiveMode`, kein `SensorSelectionEvent` mit
  vorgetaeuschtem `beforeMode` (Review-Punkt 6);
- produktgefuehrter manueller Lauf: Produktfehler sperrt sofort, kein
  automatischer Luftfallback, manuelle Fortsetzung mit Luft moeglich,
  Rueckkehr nur manuell (6.8);
- ein tatsaechlicher Wechsel erzeugt genau ein `SensorSelectionEvent` mit
  `beforeMode`/`afterMode`/Ursache/Laufrevision; wiederholte Bewertung ohne
  Aenderung erzeugt kein doppeltes Ereignis;
- `COOLING`/`COOL_HOLDING` aendern den Modus nicht allein durch
  Phasenwechsel; Sensorfehler dort verwenden denselben Vertrag;
- Kapazitaetsgrenze erreicht -> `CapacityReached`, keine Modusaenderung;
- ungueltige Rolle-, Enum-, Lauf- oder Snapshotdaten erzeugen keine
  Teilwirkung.

### 9.3 Persistenzmechanik (Commit 3)

- `persistSensorSelection` folgt exakt der Reihenfolge validieren ->
  vorbereiten -> schreiben -> committen -> RAM anwenden, identisch zu
  `persistCommand`/`persistTransition` getestet;
- `writeSnapshot` erhaelt fuer `persistSensorSelection` den expliziten
  `RunPersistenceMutationKind::SensorSelection` und schreibt ihn korrekt in
  den Kopf-Datensatz (kein `Transition`-Fehletikett mehr);
- Schema-2-Checkpoint mit `sensorSelection`-Feld round-tript verlustfrei;
- ein Schema-1-Checkpoint wird beim Laden ueber den Legacy-Decoder gelesen
  und liefert `sensorSelection == std::nullopt`;
- ein Checkpoint mit Schemaversion ausserhalb [1, 2] wird eindeutig als
  `UnsupportedSchema` abgelehnt, nicht stillschweigend falsch interpretiert;
- Restart mit Schema-2-Datensatz und `origin == Fallback`: Rueckkehrpruefung
  funktioniert nach vollstaendiger Nachvalidierung normal;
- Restart mit migriertem Schema-1-Datensatz: Rueckkehr bleibt fuer den Rest
  des Laufs gesperrt, mit sichtbarer Diagnose (6.12.3);
- fehlgeschlagene `persistSensorSelection`-Transaktion (Stale-Revision,
  Schreibfehler, Kapazitaet) veraendert weder RAM noch Aktoranforderung.

### 9.4 Konsumenten- und Laufvertragstests (Commit 4)

- `decideProgramStart` lehnt jede in 6.5 als ungueltig markierte Kombination
  mit `CommandStatus::InvalidInput` ab (auch bei ungueltigem Air/Cooling,
  auch fuer eine unbestaetigte Anfrage);
- `StartSummary.sensorMode` zeigt den effektiven, nicht den angeforderten
  Modus;
- akzeptierte Moduswechsel aktualisieren `activeRunSensorMode`,
  `sensorSelection` und Laufrevision nur atomar ueber
  `persistSensorSelection`;
- der bestehende Programmschnappschuss bleibt bei jedem Wechsel unveraendert;
- produktgefuehrter manueller Lauf durchlaeuft `decideManualStart` mit dem
  festen Vertrag aus 6.8, ohne neue Felder in `ManualRunPlanRequest`.

### 9.5 Gezielte Ausfuehrung nach Freigabe

```bash
pio test -e native --filter test_program_models
pio test -e native --filter test_configuration_codecs
pio test -e native --filter test_configuration_migration
pio test -e native --filter test_sensor_selection
pio test -e native --filter test_run_checkpoint_codec
pio test -e native --filter test_run_persistence_coordinator
pio test -e native --filter test_run_commands
python scripts/check_architecture_boundaries.py
python scripts/check_secrets.py
git diff --check
```

Der exakte Filter wird an die tatsaechlich geaenderten Tests angepasst, und
um jede Testdatei ergaenzt, die wegen der breiteren Feldmaske (Commit 1)
mechanisch angepasst werden musste. Nur ausgefuehrte Befehle werden im PR
als Nachweis genannt. Vollstaendige native und ESP-IDF-Laeufe bleiben
Owner-/Remote-CI-Gate gemaess `docs/CI_AND_QUALITY_GATES.md`.

## 10. Safety-, Security-, Recovery- und Hardwaregrenzen

- Bei Boot, Reset, `STALE`, `FAILED`, unklarer Auswahl, ungueltigen festen
  Sensoren, fehlendem Persistenzcommit oder unklarer Rollenplausibilitaet
  (inklusive fehlender Richtungs-/Zeit-/Thermikevidenz, 6.10) wird keine
  neue Peltierfreigabe erzeugt und keine automatische/manuelle Rueckkehr
  zugelassen.
- Nach Neustart gilt der abgestufte Vertrag aus 6.12.3: Schema-2-Datensaetze
  mit bekanntem Ursprung werden normal nachvalidiert; nur migrierte
  Alt-Datensaetze mit unbekanntem Ursprung sperren die Rueckkehr dauerhaft.
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
  Stabilitaetsdauer, Differenz-/Trendgrenzen und Plausibilitaetsgrenzen fuer
  Richtung/Regelanforderungsdauer/Thermik offen. Kein TBD-Wert darf
  unbemerkt als gueltiger Produktivwert dienen.
- Hardwaretests, reales Sensorabziehen, Peltierpulse und thermisches Tuning
  sind nicht Bestandteil dieses Plan-PRs.

## 11. Ressourcen- und Betriebsbudget

- Der Auswahlkern verwendet feste Werttypen, keine unbounded Historie und
  keine neue Bibliothek.
- `PersistedSensorSelectionState` ist konstant gross (ein `RunSensorMode`,
  ein `SensorSelectionOrigin`, ein `SensorSelectionEventCause`, ein
  `uint32_t`) - wenige zusaetzliche Bytes im 8-KB-Checkpoint-Budget
  (`kMaximumRunPersistencePayloadBytes = 8192U`,
  `kMaximumCheckpointRecordBytes = 8240U`), das seinerseits Teil des in
  ADR-008 festgelegten 4-MB-Flash-Gesamtbudgets ohne PSRAM ist. Keine der
  Aenderungen naehert sich dieser Grenze messbar an.
- `CrossRolePlausibilityContext` buendelt nur drei bereits vorhandene
  Snapshots plus Phase/Zeit/Richtung/Alter/Profilreferenz - keine zweite
  unlimitierte Messhistorie.
- Commit 1 hat einen mechanisch breiten, aber inhaltlich flachen Diff:
  jede Testfixture, die ein `ProgramDocument` mit expliziter
  `presentFields`-Maske von Hand konstruiert, muss die neue Maske
  uebernehmen; das `maximizeProgramPayload`-Fixture muss zusaetzlich wegen
  der geaenderten `AirOnly`-Regel (6.13 Regel 4) angepasst werden. Beide
  Aenderungsklassen werden vor Commit 1 vollstaendig durchsucht (`grep -rl
  "kCurrentRequiredProgramFields\|presentFields\|AirOnly" test/`).
- Commit 3 fuehrt zwei bislang duplizierte `kRunPersistenceSchema`-
  Konstanten zu einer zusammen (6.12.2) - eine Verkleinerung, nicht
  Vergroesserung der Codebasis.
- Erwartete RAM-Wirkung: ein kleiner Auswahlstatus, ein Ereigniswert und ein
  persistierter Sensorselektionszustand pro aktivem Lauf; reale
  Byte-/Heapwerte bleiben `TBD_IMPLEMENTATION_BUDGET`, bis ein
  reproduzierbarer Build sie belegt.
- Keine PSRAM-, OTA-, Netzwerk- oder Echtzeitabhaengigkeit.

## 12. SOLID-, DRY- und KISS-Bewertung des geplanten Diffs

- **Single Responsibility:** `program_model` bleibt allein fuer
  Programmvalidierung/-migration zustaendig; `SensorSelection` entscheidet
  Rollen und Policy anhand eines bereits validierten Programms;
  `RunPersistenceCoordinator` bekommt fuer die dritte, fachlich eigene
  Mutationsart (Sensorwechsel) einen eigenen, gleichrangigen Pfad statt
  ihn in `persistTransition` zu verstecken; Safety-/Aktorlogik bleibt in
  #24/#23.
- **Open/Closed:** neue Sensorstrategien werden ueber bestehende Vertraege
  und ein neues, in `ProductSensorFailure` eingeordnetes Feld erweitert;
  `RunPersistenceMutationKind`/`RunCheckpointTrigger` werden additiv um
  einen dritten Wert erweitert, ohne bestehende Pfade umzubauen.
- **Liskov:** unveraendert - alle Mocks liefern weiterhin den kanonischen
  `ITemperatureSource`-/Snapshotvertrag.
- **Interface Segregation:** der Auswahlkern erhaelt nur Snapshots,
  Laufkontext, `CrossRolePlausibilityContext` und monotone Zeit.
- **Dependency Inversion:** `run_persistence_contract.hpp` haengt weiterhin
  nur auf `run_commands.hpp` (fuer `RunSensorMode`/`SensorSelectionEventCause`),
  nicht auf `sensor_selection.hpp` - die Persistenzschicht bleibt unterhalb
  des Entscheidungsdienstes.
- **DRY:** die beiden bislang unabhaengig definierten
  `kRunPersistenceSchema`-Konstanten werden zusammengefuehrt (6.12.2);
  `readProgramSchema`s Bereichsakzeptanz und die neuen
  Feldeinfuehrungskonstanten ersetzen eine bislang zufaellig richtige, aber
  fragile Ableitung; `ValidationErrorCode::UnexpectedValue` wird fuer die
  neue `AirOnly`-Regel wiederverwendet statt eines unnoetigen neuen Codes.
  `RunRevision`/`RunChangeReason` werden weiterhin bewusst NICHT fuer
  Sensorwechsel zweckentfremdet.
- **KISS:** die Migrationskette hat exakt zwei konkrete, fest verdrahtete
  Schritte statt eines generischen Registrierungsmechanismus; der
  persistierte Sensorselektionszustand enthaelt nur die vier fuer den
  Restart-Vertrag tatsaechlich noetigen Felder, keine vollstaendige
  Live-Zustandsautomat-Spiegelung.

Bewusste Grenze: `automatic_validated_return_to_product` bleibt in Release 1
ohne #22/#23 praktisch inert (6.10). Das ist keine Vertragsschwaeche, weil
der Typvertrag vollstaendig ist und keine spaetere API-/Safety-Aenderung
noetig macht - nur die Produzenten fehlen noch.

## 13. Offene Ownerentscheidungen und Gates

Folgende Punkte sind aus Code, ADRs und Fachvertraegen bereits ableitbar und
werden **nicht** als Owner-Gate gefuehrt:

| Punkt | Wo entschieden |
|---|---|
| `ProductRequired` + Luftfallback | 6.13 Regel 1 |
| Automatische Neuauswahl bei `COOLING`/`COOL_HOLDING` | 6.9 |
| Eigener persistenter Mutationspfad fuer Sensorwechsel | 6.14, erforderlich und eingeplant |
| Bindung an `IEventJournal`/#19 | 6.11 |
| Numerische Commissioning-Werte | bleiben `TBD_COMMISSIONING`, Abschnitt 10 |
| Stabiler Ablehnungsstatus fuer ungueltige Air/Cooling beim Start | 6.5, `CommandStatus::InvalidInput` |
| `ReturnValidationPending`-Peltierverhalten | 6.4.1, `Allowed` auf Luft |
| `SensorSelectionEvent` vs. `-Notice`-Abgrenzung | 6.4.6, 6.11 |

Als echte Ownerentscheidungen verbleiben:

| ID | Offene Entscheidung | Planvorschlag / Stopwirkung |
|---|---|---|
| P21-01 | Vollstaendige Startmatrix (6.5) freigeben, inklusive stabilem Ablehnungsstatus und `StartSensorSelectionNotice` | wie tabelliert; ohne Freigabe kein Startvertrag |
| P21-M1 | Migrationsabbildung 4->5->6 fuer `ReturnStrategy`, inklusive `AirOnly`-Normalisierung von Policy/`fallbackDelaySeconds` (6.2.2) | produktfaehige Programme -> `automatic_validated_return_to_product`; `AirOnly` -> `remain_on_air_until_end` + normalisierte Policy/Delay. Verkettete Copy-Migration statt Einzelsprung. Ohne Freigabe keine Migrationsimplementierung |
| P21-M2 | Persistierter Sensorselektionszustand (`PersistedSensorSelectionState`, `kRunPersistenceSchema` 1->2) und der abgestufte Restart-Vertrag (6.12.3: normal fuer Schema-2, Rueckkehrsperre nur fuer migrierte Legacy-Datensaetze) | wie in 6.12 spezifiziert. Ohne Entscheidung keine Restart-Implementierung |
| P21-M3 | Vertrag fuer produktgefuehrte manuelle Laeufe (6.8) | Vorschlag: unterstuetzt mit festem WaitForUser-/ManualReturn-Vertrag. Alternative: Produktmodus fuer manuelle Laeufe ablehnen. Ohne Entscheidung kein `RunSensorMode::Product` fuer `ManualStartRequest` |

**Kein Owner-Gate mehr, sondern Abhaengigkeitsaussage:** P21-M4 aus
Revision 2 (Umfang der Plausibilitaetspruefung fuer automatische Rueckkehr)
entfaellt als Gate. Der Vertrag ist in 6.10 vollstaendig und unveraendert
gegenueber `docs/TEMPERATURE_CONTROL.md` geplant. Der Owner erhaelt
stattdessen die explizite Abhaengigkeitsaussage: `automatic_validated_return_to_product`
bleibt ohne #22/#23-Produzenten fuer Regelrichtung, Regelanforderungsdauer
und thermisches Profil praktisch inert; die manuelle Rueckkehr ist bis dahin
der einzige wirksame Pfad, obwohl es die Werksvorgabe aller vier
Katalogprogramme ist (6.2.5, 6.10).

Eine Entscheidung, die Schema, Wireformat, Fehlerklasse, Safetyfreigabe,
Hardwareannahme oder Issue-/PR-Struktur veraendert, ist eine materielle
Planabweichung. Dann wird die Umsetzung angehalten, der Plan aktualisiert,
neu committed und erneut freigegeben.

## 14. Dokumentations- und Abschlussnachweise

Vor `Ready for review` werden im Draft-PR auf dem exakten HEAD dokumentiert:

- freigegebene Plan-SHA und Zuordnung jedes umgesetzten Planpunkts zu
  Commits;
- tatsaechlich geaenderte Dateien und jede Abweichung vom erwarteten Diff,
  einschliesslich mechanisch angepasster Testfixtures aus Commit 1 und der
  `maximizeProgramPayload`-Korrektur;
- direkte Testbefehle und Ergebnisse, einschliesslich `BLOCKED`/
  `NOT_RUN` fuer nicht angeordnete Volltests oder Hardware;
- `git diff --check`, Secret- und Architekturpruefung;
- Nachweis, dass #20-Vertraege wiederverwendet und keine #22/#23/#24-
  Verantwortungen vorweggenommen wurden;
- konkrete SOLID-/DRY-/KISS-Pruefung gegen den tatsaechlichen Diff;
- verbleibende `TBD_HARDWARE`, `TBD_COMMISSIONING`,
  `TBD_IMPLEMENTATION_BUDGET` und Owner-Gates (Abschnitt 13);
- die P21-M4-Abhaengigkeitsaussage (13) bleibt bis #22/#23 unveraendert
  sichtbar dokumentiert, nicht nur einmalig erwaehnt;
- offene Reviewthreads, ohne sie ohne ausdrueckliche Autorisierung zu
  beantworten oder zu schliessen;
- Nachweis, dass PR Draft bleibt und der Owner allein `Ready for review`,
  vollstaendige Remote-CI, Merge oder Branchloeschung steuert.

## 15. Verbindliche `/task`-Taskliste fuer die Umsetzung

```text
/task
[ ] exakten freigegebenen Plan-Commit und Ownerkommentar `PLAN APPROVED` verifizieren
[ ] aktuellen Branch, HEAD, Live-Issue #21, Abhaengigkeiten und Roadmap erneut pruefen
[ ] seit der Planfreigabe geaenderte Quellen, ADRs, Vertraege und lokale Regeln inkrementell lesen
[ ] P21-01, P21-M1 bis P21-M3 aufgeloeste Ownerentscheidungen gegen den Plan abgleichen
[ ] ReturnStrategy-Enum, Feldmaske, Schema 6, Feldeinfuehrungskonstanten und Validierung implementieren
[ ] verkettete Migration 4->5->6 gemaess P21-M1 implementieren, inklusive AirOnly-Normalisierung
[ ] Codec-Wire-ID, Payloadgroesse und Bereichsakzeptanz fuer Programmschema implementieren
[ ] Werkskatalog und config/programs.example.yaml auf Schema 6 aktualisieren
[ ] betroffene Testfixtures (presentFields-Maske und maximizeProgramPayload) vollstaendig durchsuchen und anpassen
[ ] SensorQualitySnapshot-Inputs ohne Parallelqualitaetsmodell anschliessen
[ ] Auswahlkern mit vollstaendigem Zustandsautomaten (6.4) und getrenntem activeMode-/peltierPermission-Vertrag implementieren
[ ] Startmatrix (6.5) in decideProgramStart vor der NotConfirmed-Rueckgabe durchsetzen, stabiler InvalidInput-Status
[ ] Produktfehler, Wartezeit und alle drei Rueckkehrstrategien implementieren
[ ] festen Vertrag fuer produktgefuehrte manuelle Laeufe (6.8) gemaess P21-M3 implementieren
[ ] feste Schrankluft-/Kuehlkoerpersensoren in jeder Peltierfreigabe erzwingen
[ ] vollstaendigen CrossRolePlausibilityContext (6.10) mit Richtung/Zeit/Thermik implementieren, automatische Rueckkehr bei fehlender Evidenz gesperrt
[ ] SensorSelectionEvent/-Notice/StartSensorSelectionNotice sauber getrennt implementieren, ohne RunRevision/RunChangeReason zu benutzen
[ ] PersistedSensorSelectionState, kRunPersistenceSchema-Bump auf 2 und Legacy-Migration gemaess P21-M2 implementieren
[ ] gemeinsame kRunPersistenceSchema-Konstante konsolidieren
[ ] persistSensorSelection und writeSnapshot-Mutationskind-Korrektur implementieren
[ ] Restart-Vertrag (6.12.3) mit abgestufter Rueckkehrsperre nur fuer migrierte Legacy-Datensaetze implementieren
[ ] direkte, gezielte Unit-Tests fuer Schema, Migration, Codec, Auswahl, Fehler, Safetyblock, Rueckkehr, Persistenzmechanik und Restart ausfuehren
[ ] gezielte Laufkommand-/Laufpersistenz-/Codec-Konsumententests ausfuehren
[ ] gezielte Architektur-, Secret-, Format- und git diff --check-Pruefungen ausfuehren
[ ] betroffene Fachvertraege und docs/ACCEPTANCE_TESTS.md aktualisieren
[ ] docs/ROADMAP.md nur bei tatsaechlicher Status- oder Gatewirkung synchronisieren
[ ] Ressourcenwirkung, begrenzte Puffer und offene Hardware-/Commissioning-Gates dokumentieren
[ ] Review des vollstaendigen aktuellen Diffs gegen Issue, Plan, ADRs und Fachvertraege durchfuehren
[ ] SOLID-, DRY- und KISS-Bewertung gegen den tatsaechlichen Diff durchfuehren
[ ] keinen unbelegten Hardware-, Safety-, Persistenz- oder Bibliotheksentscheid im Diff belassen
[ ] P21-M4-Abhaengigkeitsaussage im PR sichtbar dokumentieren, nicht als abgeschwaechten Vertrag behandeln
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
Roadmap-/PR-/Handover-Aktualisierung wird im Draft-PR der exakte neue
Plan-Commit, der aktuelle HEAD und

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
[x] docs/ROADMAP.md auf PR #98 merged und Issue #21 als aktuelle Planungsarbeit aktualisieren (Revision 1)
[x] Plan Revision 1 committen und pushen; Draft-PR aktualisieren (Plan-Commit c505fce6...)
[x] PR-#99-Reviewbefunde zu Revision 1 gegen Code verifiziert; Revision 2 committen und pushen; Draft-PR aktualisieren (Plan-Commit aaeefbdf...)
[x] PR-#99-Reviewbefunde zu Revision 2 gegen Code verifiziert: writeSnapshot-Mutationskind-Ableitung, fehlende Migration fuer RunPersistenceSnapshot, readProgramLimitsAndCompletion-Feldversatz-Bug, maximizeProgramPayload-Konflikt, kanonischer Copy-Migrationsvertrag aus CONFIGURATION_PERSISTENCE.md
[x] atomaren Persistenzpfad (persistSensorSelection, RunPersistenceMutationKind::SensorSelection) geplant
[x] persistierten, versionierten Sensorselektionszustand mit abgestuftem Restart-Vertrag geplant (kein Normalvertrag mehr fuer dauerhafte Rueckkehrsperre)
[x] vollstaendigen CrossRolePlausibilityContext ohne Vertragsabschwaechung geplant; P21-M4 als Abhaengigkeitsaussage statt Gate reklassifiziert
[x] verkettete Programmschema-Migration (4->5->6) statt Einzelsprung geplant
[x] Zustandsautomat und Cross-Field-Regeln praezisiert (FallbackWaitPending-Alias entfernt, Zustandstabelle, Event/Notice-Trennung, stabiler InvalidInput-Status, AirOnly-Normalisierung, StartSensorSelectionNotice)
[x] Owner-Gates aktualisiert (P21-01, P21-M1 bis P21-M3, P21-M4 als Abhaengigkeitsaussage)
[x] ausschliesslich Plan und notwendige Roadmap-/PR-/Handover-Aktualisierung geaendert
[ ] Plan committen und pushen
[ ] SESSION-HANDOVER-Kommentar auf neuen HEAD aktualisieren (ersetzt den bestehenden Kommentar, keinen zweiten anlegen)
[ ] Draft-PR mit exakter neuer Plan-SHA, aktuellem HEAD und aufgeloesten/verbleibenden Ownerentscheidungen aktualisieren
[ ] HALTED_FOR_OWNER_REVIEW
```
