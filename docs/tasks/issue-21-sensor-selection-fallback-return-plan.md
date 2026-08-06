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
  2e3a041131996d73cb0ce342f256f06f79f694bd (Revision 3)
PLAN_ONLY: YES
IMPLEMENTATION_STARTED: NO
PLAN_STATUS: PLAN_DRAFT_REVIEW_REQUIRED
IMPLEMENTATION_STATUS: IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

Dies ist Revision 4. Sie behebt die im PR-#99-Review vom 2026-08-06 zu
Revision 3 benannten materiellen Befunde: eine verbleibende Luftfallback-
Luecke fuer `ProductRequired` im Zustandsautomaten, ein unaufgeloester
Recovery-Lebenszyklus-Widerspruch zum bestehenden `RunPersistenceCoordinator`
(#17/#18-Grenze), eine zu enge Definition dessen, was atomar persistiert
wird (nur Moduswechsel statt aller laufrelevanten Sensorentscheidungen),
ein widerspruechliches Provenienzmodell, ein leerer, nicht auswertbarer
Thermik-Typ sowie ein unvollstaendig spezifizierter Ergebnis-/Ereignis-
Anschluss. Produktionscode, produktive Tests, Toolchain, Buildkonfiguration,
Hardwarekonfiguration und Abhaengigkeiten werden weiterhin in dieser
Planungsphase nicht geaendert.

## 2. Live-Issue- und Abhaengigkeitsabgleich

| Quelle | Live-Stand am 2026-08-06 | Bedeutung fuer diesen Plan |
|---|---|---|
| Issue #21 | OPEN, Body-Status `PLANNED_SPEC_PENDING`, keine Kommentare | eigener Plan-first-Draft-PR, keine Implementierungsfreigabe |
| Issue #14 | CLOSED | kanonische Prozesszustaende und Uebergangstopologie stehen zur Verfuegung |
| Issue #17 | Implementiert (`RunPersistenceCoordinator`, siehe `docs/tasks/issue-17-implementation-plan.md`) | definiert `LoadedActiveRun`/`RecoveryPending` und weist Recoveryaktivierung ausdruecklich #18 zu; #21 haelt sich an diese Grenze (6.12) |
| Issue #18 | OPEN, `[E2.3] Wiederanlauf und temperaturgewichteten Fortschritt implementieren` | zustaendig fuer die tatsaechliche Aktivierung von `LoadedActiveRun -> Ready`; #21 liefert nur Daten und eine reine Entscheidungsfunktion dafuer, keine Aktivierung |
| Issue #20 | CLOSED, Body-Status `READY` ist historisch | `SensorQualitySnapshot` und `SensorQualityPipeline` sind die bestehende Qualitaetsquelle |
| Epic #5 | OPEN | Issue bleibt Teil des E3-Sensor-/Regel-/Safety-Kerns |
| PR #99 | OPEN, Draft, dieser Plan ist die vierte Revision | Reviewbefunde vom 2026-08-06 zu Revision 3 sind Grundlage dieser Ueberarbeitung |

Issue #21 hat weiterhin keine Kommentare, die zusaetzliche Anforderungen oder
Ownerentscheidungen enthalten. Issue #17 und #18 werden nicht veraendert.

## 3. Verbindliche Quellen und Lesematrix

Bei Widerspruechen gilt die in `docs/SPECIFICATION_REVIEW.md` festgelegte
Reihenfolge:

1. akzeptierte ADRs im Register `docs/DECISIONS.md`;
2. `docs/SPECIFICATION_REVIEW.md` fuer Release-Scope, Quellenrollen und TBDs;
3. der fachlich zustaendige spezialisierte Vertrag;
4. `REQUIREMENTS.md`, `ARCHITECTURE.md` und `HARDWARE.md`;
5. Beispielkonfigurationen;
6. historische Audits, Plaene und Revisionsdokumente.

Zusaetzlich zu den in Revision 1-3 gelesenen Quellen wurden fuer diese
Ueberarbeitung konkret nachvollzogen:

- `docs/tasks/issue-17-implementation-plan.md`, Zeile 203-205 und 528:
  `LoadedActiveRun` "gibt die technische Projektion an #18 weiter, erlaubt
  aber [keine Mutation]... Recoveryaktivierung wird erst mit #18 ergaenzt;
  #17 erfindet sie nicht." `RecoveryPending` ist der bestehende, bereits
  implementierte fail-closed-Zustand fuer einen geladenen aktiven Lauf.
  Issue #18 (OPEN) ist explizit fuer "phasenbezogener Wiederanlauf",
  "Betrieb ohne sofort verfuegbare NTP-Zeit" und
  "konservative temperaturgewichtete Fortschrittsbewertung" zustaendig -
  keine dieser Verantwortungen darf #21 sich stillschweigend aneignen
  (Root-`AGENTS.md`: "Parallelvertraege und stille Neuerfindungen sind
  unzulaessig");
- `lib/fermentation_app/src/run_persistence_coordinator.cpp`, verifiziert:
  `loadAndInitialize()` setzt bei einem geladenen aktiven Lauf
  `state_ = LoadedActiveRun` (Zeile 270) und liefert `{Current, snap}`. Es
  gibt in dieser Datei **keinen** Pfad, der `state_` von `LoadedActiveRun`
  wieder nach `Ready` fuehrt. `persistCommand`, `persistTransition`,
  `checkpointPeriodic` und `writeSnapshot` verlangen alle `state_ == Ready`
  (bzw. zusaetzlich `ReadyEmpty` fuer die ersten beiden) und liefern sonst
  `unavailableResult()`, welches fuer `LoadedActiveRun` `RecoveryPending`
  zurueckgibt (Zeile 79-80);
- `lib/fermentation_app/src/run_persistence_codec.cpp`, Detailpruefung der
  Referenzierungspfade: `readReference` (Zeile 701-713) und `validReference`
  (715-731) vergleichen `schemaVersion` **direkt gegen die globale
  Konstante** `kRunPersistenceSchema` - diese beiden Funktionen muessen fuer
  eine Zwei-Slot-Koexistenz von Schema 1 und Schema 2 auf einen bekannten
  Wertebereich erweitert werden. `runCheckpointReferenceMatches`
  (963-975) vergleicht dagegen `reference.schemaVersion` gegen
  `envelope.envelope->schemaVersion` **derselben Aufzeichnung** - ein
  bereits versionsunabhaengiger Selbstkonsistenzcheck, der **keine**
  Aenderung braucht (Korrektur einer ungenauen Aussage in Revision 3).
  `validPreparedHead`/`validCommittedHead` verlangen fuer die jeweils
  ungenutzte Referenz (`head.current` im Prepared-Zustand,
  `head.target` im Committed-Zustand) separat exakt `schemaVersion == 0U` -
  diese Pruefung bleibt unveraendert und ist von der Bereichserweiterung auf
  `{1, 2}` fuer **genutzte** Referenzen zu unterscheiden;
- `CommandEffect` (`run_commands.hpp`) kommt in `run_persistence_codec.cpp`
  an keiner Stelle vor - der Enum wird nie auf die Wire-Bytes geschrieben,
  sondern ausschliesslich RAM-seitig in `RunPersistenceResult`/
  `CommandDecision` gefuehrt. Neue Werte sind deshalb ohne Schemabump
  moeglich;
- `run_commands.hpp`, `MessageCode` (Zeile 177): `UserDecisionRequired`
  existiert bereits als eigener Meldungscode - der Uebergang
  `ProductFailureDetected -> UserDecisionRequired` hat damit bereits einen
  passenden, bestehenden Laufzeitmeldungspfad und braucht keine neue
  Laufrevision, um sichtbar zu sein.

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
- wie ein Produktfuehlerfehler zuerst sicher zur Peltierabschaltung fuehrt,
  und zwar fuer **jede** `ProductSensorFailurePolicy` einschliesslich
  `ProductRequired`, wo kein Luftfallback jemals - weder automatisch noch
  manuell - erreichbar sein darf (6.4, 6.13);
- wann ein programmabhaengiger Luft-Ersatzbetrieb zulaessig ist;
- wie `remain_on_air_until_end`, `manual_return_to_product` und
  `automatic_validated_return_to_product` als kanonisches, typisiertes
  Programmfeld umgesetzt werden;
- wie **jede laufrelevante Sensorentscheidung** - nicht nur ein tatsaechlicher
  Moduswechsel - als sichtbares fachliches Laufereignis, Laufrevision und
  spaeter auslesbare Meldungs-/Journalreferenz weitergegeben wird (6.14,
  Korrektur gegenueber Revision 3);
- wie der dafuer persistierte Zustand nach einem Neustart innerhalb der
  bestehenden, bereits implementierten #17-Grenze (`LoadedActiveRun`,
  Aktivierung erst mit #18) korrekt bereitsteht, ohne diese Grenze
  vorwegzunehmen (6.12).

Die Entscheidung ist reversibel: Sie mutiert den laufenden Zustand nicht
selbst, sondern liefert eine erwartete Vorher-/Nachher-Entscheidung. Eine
lauf- oder aktorwirksame Anwendung erfolgt erst nach einer eigenen,
benannten atomaren Persistenztransaktion (6.14).

### Nicht-Ziele

- keine DS18B20-, 1-Wire-, GPIO-, Display-, Touch-, WLAN- oder
  ESP-IDF-Treiberimplementierung;
- keine konkrete Pin-, Pegel-, Bus- oder Steckerentscheidung;
- keine PI-Regelung, Luftbegrenzungsregel, Aktorplanung, Totzeit oder
  Lueftersteuerung aus #22/#23. #21 definiert den vollstaendigen
  Plausibilitaetsvertrag (6.10) bereits typisiert und auswertbar; die
  tatsaechliche Befuellung von Regelrichtung, Regelanforderungsdauer und
  thermischer Kompatibilitaetsevidenz bleibt #22/#23 vorbehalten. Bis dahin
  bleibt `automatic_validated_return_to_product` mangels Evidenz praktisch
  inert (13, P21-M4);
- **keine Recoveryaktivierung, kein Uebergang von `LoadedActiveRun` nach
  `Ready` und keine Wiederanlauf-Aktorfreigabe** - das bleibt vollstaendig
  #18 vorbehalten (6.12, Korrektur gegenueber Revision 3, die dies
  faelschlich implizit dem #21-Scope zugeschlagen hatte);
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
- keine Wiederverwendung von `RunRevision`/`RunChangeReason` oder
  `ProcessMessage` fuer Sensorentscheidungen - beide sind auf andere,
  fachlich verschiedene Inhalte zugeschnitten;
- keine generische, unbegrenzte Migrationskette fuer beliebig viele
  zukuenftige Schemaversionen - geplant und implementiert werden genau die
  tatsaechlich existierenden Schritte 4 -> 5 und 5 -> 6 (6.2).

## 5. Befund des aktuellen Codes

### Bereits vorhanden

- `device_platform::SensorQualityPipeline` verarbeitet eine einzelne Quelle
  und liefert `VALID`, `STALE`, `FAILED`, Roh-/Korrektur-/Filterwerte,
  Messalter, Trend, Fehlerursache und Wiedererkennungsfortschritt.
- `ProgramDefinition` kennt `SensorPreference` und
  `ProductSensorFailurePolicy` sowie die validierte `fallbackDelaySeconds`,
  aber keine Rueckkehrstrategie (wird in 6.2 ergaenzt - unveraendert seit
  Revision 3).
- `decideProgramStart` prueft den angeforderten `RunSensorMode` bislang
  nicht gegen `program.sensorPreference` (wird in 6.5 ergaenzt).
- `RunCommandState::activeRunSensorMode` bleibt die alleinige kanonische
  Quelle des aktiven Modus; ein Sensorselektionsursprung existiert weder im
  RAM-Zustand noch im Snapshot.
- `RunPersistenceCoordinator` besitzt genau zwei Mutationspfade,
  `persistCommand` und `persistTransition`, beide auf den gemeinsamen
  Zwei-Phasen-Schreiber `writeSnapshot` aufgesetzt, beide nur aus `Ready`
  (bzw. zusaetzlich `ReadyEmpty`) heraus nutzbar. **Ein geladener aktiver
  Lauf bleibt nach `loadAndInitialize()` in `LoadedActiveRun` und erlaubt
  keine Mutation, bis eine externe Aktivierung (#18) den Coordinator nach
  `Ready` ueberfuehrt** - dieser Mechanismus existiert bereits vollstaendig
  und fail-closed, unabhaengig von #21.
- `RunPersistenceSnapshot` hat kein Migrationskonzept: jede
  Schemaversionsabweichung wird beim Laden hart als `UnsupportedSchema`
  abgelehnt.
- `migrateProgramToCurrentSchema` unterstuetzt nur genau eine
  Vorgaengerversion, keine Verkettung (wird in 6.2 auf eine Kette
  umgebaut - unveraendert seit Revision 3).
- `RunRevision`/`RunChangeReason` sind eine Anpassungshistorie fuer
  Zieltemperatur und Restdauer; `ProcessMessage` ist ein schmaler, auf
  Prozessuebergaenge zugeschnittener Fuenf-Werte-Enum - beide fachlich
  ungeeignet fuer Sensorentscheidungen.
- Die Zustandsmaschine kennt die relevanten Phasen und akzeptiert nur
  abstrakte `ProcessSignals`. Sie kennt noch keine Sensorrollen- oder
  Fallbacklogik.

### Fehlende Teile

- kein `ReturnStrategy`-Feld im Programmschema;
- keine verkettete Programmschema-Migration ueber mehr als einen Schritt;
- keine Cross-Field-Validierung zwischen `SensorPreference`,
  `ProductSensorFailurePolicy` und `ReturnStrategy`;
- **kein struktureller Ausschluss eines manuellen Luftfallbacks fuer
  `ProductRequired`** - der bisherige Zustandsautomat liesse
  `UserDecisionRequired -> AirFallbackActive` fuer jede Praeferenz zu;
- kein `SensorSelection`-Wertmodell oder Entscheidungsdienst;
- **keine Unterscheidung zwischen "Moduswechsel" und "laufrelevante
  Sensorentscheidung ohne Moduswechsel"** bei der Frage, was atomar
  persistiert wird;
- kein auswertbarer Typ fuer thermische Plausibilitaet (Revision 3s
  `ThermalCommissioningProfileRef` war ein leerer Platzhalter);
- keine widerspruchsfreie Provenienzmodellierung fuer den persistierten
  Sensorselektionszustand;
- kein konkreter Anschluss von `persistSensorSelection`s Ergebnis an
  `RunPersistenceResult`;
- kein eigener, benannter atomarer Persistenzpfad fuer laufrelevante
  Sensorentscheidungen;
- keine gezielten Tests fuer Ausfall, gleichzeitig ungueltige feste Sensoren,
  Rueckkehr, Moduserhalt, Startmatrix, Migration, Cross-Field-Validierung,
  Restart, atomare Sensorpersistenz, Zwei-Slot-Koexistenz oder die
  vollstaendige `ProductRequired`-Aktionsmatrix.

## 6. Fachvertraege

Die folgenden Typen und Regeln sind der umsetzbare Planvorschlag. Ihre
materielle Freigabe erfolgt erst mit `PLAN APPROVED` fuer diesen Plan-Commit.

### 6.1 Eingaben und Rollen

Unveraendert seit Revision 3:

```text
air      = fester Schrankluftfuehler
product  = abnehmbarer Produktfuehler
cooling  = Kuehlkoerper-/Aussenwaermetauscherfuehler
```

Der Selektor kennt die `SensorQualitySnapshot`-Felder, aber keine
`ITemperatureSource` und keinen Bus. Eingaben: unveraenderlicher
`ProgramDefinition`-/Laufkontext inklusive `ReturnStrategy` (6.2); aktueller
`RunSensorMode`, RAM-Auswahlphase und persistierter
`PersistedSensorSelectionState` (6.12); aktueller `ProcessState`; monotone
Zeit; explizite Benutzeraktion; vollstaendiger
`CrossRolePlausibilityContext` (6.10).

### 6.2 Rueckkehrstrategie im kanonischen Programmmodell

Unveraendert seit Revision 3 (siehe dortige Herleitung 6.2.1-6.2.5):
`ReturnStrategy`-Enum in `ProductSensorFailure`, neues `ProgramField`-Bit,
`kCurrentProgramSchemaVersion` 6, `kMinimumMigratableProgramSchemaVersion`
4, verkettete Migration `migrateProgramSchema4To5`/
`migrateProgramSchema5To6`, absolute Feldeinfuehrungskonstanten
`kProductWaitFieldIntroducedInSchema`/`kReturnStrategyFieldIntroducedInSchema`,
Migrationsabbildung 5 -> 6 (produktfaehig -> `AutomaticValidatedReturnToProduct`,
`AirOnly` -> `RemainOnAirUntilEnd` + Policy-/Delay-Normalisierung).

### 6.3 Ausgabewert und Zustandsseparation

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
decision: SensorSelectionDecision (6.11) - traegt Ursache, Klassifikation
          und ggf. Event-/Notice-Nutzlast
expectedRunRevision
```

### 6.4 Auswahlzustandsautomat

#### 6.4.1 Zustaende mit vollstaendiger Definition

| Zustand | Modus | `peltierPermission` | Eintritt | Austritt | Persistenzwirkung |
|---|---|---|---|---|---|
| `NormalProduct` | Product | Allowed (bei validem Product/Air/Cooling) | Start, Rueckkehr, Re-Evaluation ohne Aenderung | Produkt wird nicht mehr nutzbar | keine bei reiner Re-Evaluation |
| `NormalAir` | Air | Allowed (bei validem Air/Cooling) | Start mit Luft (`provenance = InitialSelection`) | nie automatisch verlassen | keine |
| `ProductFailureDetected` | Product | **Allowed -> Blocked** (sofort) | Produkt-Snapshot wird nicht mehr nutzbar | Wartezeit ablaeuft, Produkt erneut valide, oder Policy = StopToSafeState | **atomare Revision, Ursache `ProductFailureBlock`** (Korrektur ggue. Revision 3, siehe 6.4.9) |
| `UserDecisionRequired` | Product | Blocked (unveraendert) | Wartezeit abgelaufen UND Policy = WaitForUser | explizite Benutzeraktion oder Air/Cooling wird ungueltig | keine eigene Revision - reiner RAM-Unterphasenwechsel mit bestehendem `MessageCode::UserDecisionRequired` (6.4.9) |
| `AirFallbackActive` | Air | Allowed (bei validem Air/Cooling), sonst Blocked | Wartezeit abgelaufen (nur falls `SensorPreference` Luft erlaubt, siehe 6.4.10) oder bestaetigte manuelle Aktion (nur falls `SensorPreference != ProductRequired`) oder Startersatz | Rueckkehrbedingung erfuellt oder Air/Cooling wird ungueltig | atomare Revision, `SensorSelectionEvent`, `provenance = FallbackActive` |
| `ReturnValidationPending` | Air (Regelung laeuft unveraendert weiter) | Allowed, solange Air/Cooling valide | `AirFallbackActive` UND `ReturnStrategy = AutomaticValidatedReturnToProduct` UND Produkt valide | 6.10-Evidenz vollstaendig positiv (-> `NormalProduct`) oder Evidenz bricht ab (-> `AirFallbackActive`) oder Air/Cooling wird ungueltig (-> `SafeLocked`) | Eintritt/laufende Pruefung: keine; Abbruch: **atomare Revision, Ursache `ReturnValidationAborted`** (6.4.9); positiver Abschluss: atomare Revision, `SensorSelectionEvent`, `provenance = ReturnedToProduct` |
| `SafeLocked` | unveraendert | **Blocked, dauerhaft bis externe Aufloesung** | Policy = StopToSafeState, gleichzeitiger Air-/Cooling-Ausfall, oder unklare 6.10-Evidenz | nur ueber denselben Pfad wie ein frischer Start dieser Bewertungskette | **atomare Revision, Ursache `SafeStateEntry`, immer - unabhaengig davon, ob `peltierPermission` bereits vorher `Blocked` war** (6.4.9) |
| `RestartRevalidationPending` | letzter persistierter `activeMode` | Blocked | siehe 6.12 - liegt ausserhalb des #21-Mutationspfads, solange der Coordinator in `LoadedActiveRun` verbleibt | erste vollstaendige Nachbewertung, sobald #18 den Coordinator aktiviert hat | keine eigene #21-Persistenzwirkung; #18 persistiert die daraus folgende Entscheidung ueber `persistSensorSelection` (6.14) |

`SafeLocked` ist kein Fehlerzustand im Sinne von #24; er ist der
Selektor-interne "kein Modus ist sicher waehlbar"-Zustand.

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
                                               Air+Cooling VALID -
                                               strukturell nur erreichbar,
                                               wenn SensorPreference
                                               ueberhaupt Luft erlaubt, siehe
                                               6.4.10; FallbackToAirAfterTimeout
                                               ist fuer ProductRequired durch
                                               6.13 Regel 1 bereits
                                               validierungsseitig
                                               ausgeschlossen]
ProductFailureDetected -> SafeLocked         [Policy StopToSafeState, oder
                                               Air/Cooling gleichzeitig
                                               ungueltig]

UserDecisionRequired -> AirFallbackActive    [explizite, validierte
                                               Benutzeraktion "mit Luft
                                               fortsetzen" UND
                                               SensorPreference != ProductRequired
                                               (6.4.10); sonst
                                               CommandStatus::InvalidInput,
                                               kein Zustandswechsel]
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

RestartRevalidationPending -> *              [ausserhalb des #21-Mutations-
                                               pfads, solange LoadedActiveRun
                                               anhaelt; siehe 6.12]

SafeLocked -> *                              [nur ueber denselben Pfad wie
                                               ein frischer Start dieser
                                               Bewertungskette; kein
                                               impliziter Austritt]
```

Explizit verboten:

- **`* -> AirFallbackActive` fuer einen Lauf mit `SensorPreference::
  ProductRequired`, unabhaengig davon, ob der Wechsel automatisch oder
  manuell angefordert wird** (schliesst die in Revision 3 verbliebene
  Luecke, siehe 6.4.10);
- jeder Uebergang, der `activeMode` ohne ein zugehoeriges
  `SensorSelectionEvent` (6.11) aendert;
- ein Uebergang von `ProductFailureDetected`/`UserDecisionRequired` direkt
  nach `AirFallbackActive`, ohne dass die fuer die jeweilige Policy
  vorgesehene Bedingung erfuellt ist;
- jeder Uebergang nach `NormalProduct`/`AirFallbackActive` mit
  `peltierPermission = Allowed`, waehrend `RestartRevalidationPending`
  aktiv ist.

#### 6.4.3 Start der Fallback-Wartezeit

Unveraendert seit Revision 3: RAM-only, monotone Zeit des ersten
`ProductFailureDetected`-Eintritts.

#### 6.4.4 Erneut gueltiger Produktwert waehrend der Wartezeit

Wird der Produkt-Snapshot waehrend `ProductFailureDetected` (vor Ablauf der
Wartezeit) wieder `Valid`, kehrt der Zustand zu `NormalProduct` zurueck.
`peltierPermission` wechselt dabei `Blocked -> Allowed`, `activeMode` bleibt
`Product`. Das ist per 6.4.9-Grundregel eine **atomare Revision mit Ursache
`RecoveryRevalidation`** (Korrektur ggue. Revision 3, die dies faelschlich
als rein fluechtig einstufte - jede Rueckkehr von `Blocked` nach `Allowed`
ist laufrelevant, unabhaengig vom Ausloeser).

#### 6.4.5 Einmalige Verarbeitung manueller Aktionen

Unveraendert: `CommandEnvelope`-Idempotenzsicherung wie bei bestehenden
Kommandos.

#### 6.4.6 Idempotenz

Eine wiederholte Bewertung ohne eine der in 6.4.9 genannten Wirkungen
liefert `after == before` ohne neue Revision.

#### 6.4.7 Abbruch, Neustart und Wiederaufnahme der Rueckkehrvalidierung

- `ReturnValidationPending` wird abgebrochen (zurueck nach
  `AirFallbackActive`), sobald ein Kriterium aus 6.10 nicht mehr erfuellt
  ist; siehe 6.4.9 fuer die Persistenzwirkung.
- Eine neue `ReturnValidationPending`-Phase beginnt immer bei Evidenz Null.
- Nach einer Aktivierung durch #18 beginnt jede laufende Wartezeit- und
  Rueckkehrvalidierung zwingend bei Null; die Provenienz des aktuellen
  Modus bleibt dagegen ab Schema 2 rekonstruierbar (6.12).

#### 6.4.8 Revisions- und Kapazitaetsgrenzen

Unveraendert: dieselbe Laufrevisions-/Kapazitaetspruefung wie bestehende
Kommandos; bei erreichter Kapazitaet `CommandStatus::CapacityReached` und
keine Wirkung.

#### 6.4.9 Grundregel: was wird atomar persistiert?

Eine Sensorentscheidung wird ueber `persistSensorSelection` (6.14) atomar
persistiert, wenn mindestens eine der folgenden Bedingungen gilt:

```text
(a) peltierPermission aendert sich (Allowed<->Blocked)
(b) activeMode aendert sich
(c) der Zustand SafeLocked wird betreten
(d) eine laufende automatische Rueckkehrvalidierung wird abgebrochen
```

Klassifikation ueber `SensorSelectionDecisionCause` (6.11, ersetzt Revision
3s `SensorSelectionEventCause` und die dortige starre Event-/Notice-
Zweiteilung):

| Bedingung | Cause | Persistenzwirkung |
|---|---|---|
| (a) Allowed -> Blocked | `ProductFailureBlock` | `SensorSelectionNotice` |
| (b) | `FallbackToAir` / `AutomaticValidatedReturn` / `ManualUserFallback` / `ManualUserReturn` | `SensorSelectionEvent` |
| (a) Blocked -> Allowed | `RecoveryRevalidation` | `SensorSelectionNotice` |
| (c) | `SafeStateEntry` | `SensorSelectionNotice` (oder `SensorSelectionEvent`, falls (c) gleichzeitig mit (b) eintritt) |
| (d) | `ReturnValidationAborted` | `SensorSelectionNotice` |
| keine der obigen | `None` | keine Persistenzwirkung, hoechstens RAM-/`MessageCode`-Diagnose |

Reine RAM-Unterphasenwechsel ohne (a)-(d), insbesondere
`ProductFailureDetected -> UserDecisionRequired`, bleiben fluechtig: dieser
spezifische Uebergang hat mit `MessageCode::UserDecisionRequired`
(`run_commands.hpp` Zeile 177) bereits einen passenden bestehenden
Laufzeitmeldungspfad (siehe 3), sodass keine neue Struktur noetig ist.
`(c)` und `(d)` sind bewusste, im kanonischen Laufpersistenzvertrag
("Warnung oder Fehler mit Laufwirkung") begruendete Ergaenzungen der
Grundregel, keine willkuerlichen Sonderfaelle: der Eintritt in einen
dauerhaft gesperrten Zustand beziehungsweise der Abbruch einer bereits
sichtbar angekuendigten Rueckkehr sind beide fuer sich genommen
laufrelevante Warnungen, auch wenn `peltierPermission` dabei zufaellig
unveraendert bleibt.

#### 6.4.10 `ProductRequired` schliesst jeden Luftfallback strukturell aus

**Neuer Abschnitt (Review-Befund 1).** Zwei sich ergaenzende Mechanismen,
keine Mischloesung:

1. **Zentral (Validierung, unveraendert seit Revision 3):** 6.13 Regel 1
   lehnt `SensorPreference::ProductRequired` +
   `ProductSensorFailurePolicy::FallbackToAirAfterTimeout` als
   `IncompatibleCombination` ab. Der automatische Pfad
   `ProductFailureDetected -> AirFallbackActive` ist damit fuer
   `ProductRequired` bereits strukturell unerreichbar, weil die dafuer
   noetige Policy nie konfigurierbar ist.
2. **Im Zustandsautomaten (neu):** `ProductSensorFailurePolicy::WaitForUser`
   bleibt fuer `ProductRequired` **gueltig** (kein Migrations- oder
   Bestandsschutzproblem, siehe Abwaegung unten). Die Aktion "mit Luft
   fortsetzen" (`UserDecisionRequired -> AirFallbackActive`) wird jedoch
   zusaetzlich zur bestehenden Bestaetigungspruefung gegen
   `SensorPreference != ProductRequired` geprueft. Fuer `ProductRequired`
   bleiben in `UserDecisionRequired` nur Aktionen verfuegbar, die nicht auf
   Luftregelung wechseln: erneut pruefen (automatisch bei jedem
   Bewertungsaufruf), auf wieder gueltigen Produktfuehler warten (impliziter
   Verbleib in `UserDecisionRequired`), Lauf abbrechen oder sicher
   ausschalten (bestehende `StopRequest`-Pfade, ausserhalb des
   #21-Kommandovokabulars). Eine dennoch eingereichte "mit Luft
   fortsetzen"-Aktion liefert `CommandStatus::InvalidInput` und bewirkt
   keine Zustandsaenderung - konsistent mit dem in 6.5 etablierten Muster
   fuer strukturell unmoegliche Kombinationen.

**Abwaegung der Alternative (zentrale Regel `ProductRequired` erlaubt nur
`StopToSafeState`), explizit verworfen:** Eine rein zentrale Loesung waere
kuerzer, haette aber eine Migrationsfolge, die die AirOnly-Normalisierung
(6.2.1) nicht hat: `ProductRequired + WaitForUser` ist heute (Schema 4/5)
gueltig und in Benutzung; `WaitForUser` durch die 5->6-Migration still auf
`StopToSafeState` zu normalisieren waere **keine inerte Normalisierung wie
bei `AirOnly`**, sondern eine echte Verhaltensaenderung - ein
Produktfuehlerausfall, der bisher auf den Benutzer wartete, wuerde nach dem
Update sofort und ohne Wartefenster in `SafeLocked` enden. Die hier gewaehlte
Loesung (Policy bleibt gueltig, nur die eine strukturell unzulaessige Aktion
wird abgelehnt) vermeidet diese stille Verhaltensaenderung vollstaendig und
nutzt dieselbe `SensorPreference`-Bewusstheit, die der Selektor ohnehin schon
fuer die Startmatrix (6.5) und die Fallback-/Rueckkehrregeln (6.6/6.7)
braucht - keine neue Art von Abhaengigkeit, nur eine weitere Anwendung
derselben bereits vorhandenen.

**Symmetrische Korrektur der `fallbackDelaySeconds`-Regel (6.13):** analog zu
`AirOnly` ist `fallbackDelaySeconds` auch bei jeder anderen Kombination mit
`policy != FallbackToAirAfterTimeout` (also auch `ProductRequired +
WaitForUser`) ein toter Wert. 6.13 erhaelt dafuer eine eigene, von der
`AirOnly`-Regel unabhaengige generelle Regel (siehe dort - **keine**
Zusammenlegung zu einer einzigen Regel, weil `AirOnly` den Wert selbst bei
passender Policy nie auswertet, waehrend die generelle Regel nur bei
nicht-passender Policy greift; eine Zusammenlegung wuerde die
`AirOnly`-Einschraenkung faelschlich wieder aufheben).

### 6.5 Vollstaendige Startmatrix

Unveraendert seit Revision 3 (Tabelle, stabiler `CommandStatus::InvalidInput`-
Status, `StartSensorSelectionNotice`, Provenienzzuordnung
`InitialSelection`/`FallbackActive` je Zeile).

### 6.6 Produktfehler und Ersatzbetrieb

Unveraendert seit Revision 3, ergaenzt um 6.4.10: bei `ProductRequired` ist
der beschriebene Ablauf fuer keine konfigurierbare Policy in der Lage,
`AirFallbackActive` zu erreichen.

### 6.7 Rueckkehr zum Produktfuehler

Unveraendert seit Revision 3.

### 6.8 Produktgefuehrte manuelle Laeufe

Unveraendert seit Revision 3: fester WaitForUser-/ManualReturn-Vertrag,
keine neuen Felder in `ManualRunPlanRequest`.

### 6.9 Phasen, Kuehlen und Halten

Unveraendert seit Revision 3.

### 6.10 Rollenuebergreifende Plausibilitaetspruefung

`CrossRolePlausibilityContext` bleibt strukturell wie in Revision 3
(Prozessphase, drei `SensorQualitySnapshot`s, `AbstractControlDirection`,
`controlDemandAgeMs`), mit einer Korrektur an der thermischen Evidenz:

**Korrektur gegenueber Revision 3:** `ThermalCommissioningProfileRef` war ein
leerer Platzhaltertyp ohne Profilidentitaet, Grenzwert oder berechnetes
Ergebnis - der Auswahlkern haette damit keine Kompatibilitaet pruefen
koennen. Ersetzt durch einen tatsaechlich auswertbaren Vertrag:

```cpp
enum class ThermalCompatibility : std::uint8_t {
    Unavailable,
    Compatible,
    Incompatible,
    Stale,
};

struct ThermalCompatibilityEvidence {
    ThermalCompatibility status{ThermalCompatibility::Unavailable};
    std::uint32_t profileRevision{0U};
    std::uint64_t evaluatedAtMonotonicMillis{0U};
};

struct CrossRolePlausibilityContext {
    ProcessState phase;
    std::uint64_t evaluationMonotonicMillis;
    SensorQualitySnapshot air;
    SensorQualitySnapshot product;
    SensorQualitySnapshot cooling;
    AbstractControlDirection direction{AbstractControlDirection::Unknown};
    std::optional<std::uint64_t> controlDemandAgeMs;
    ThermalCompatibilityEvidence thermalCompatibility;
};
```

`thermalCompatibility` ist bewusst **nicht** `optional` - `Unavailable` ist
selbst der explizite "noch nicht befuellt"-Wert, analog zur Vermeidung von
`std::nullopt` als Doppelbedeutung in 6.12. Der Produzent (spaeter #22/#23)
setzt `status`; #21 wertet ihn nur aus, berechnet ihn nicht selbst.

Verbindliche Regel (unveraendert in der Substanz, jetzt am konkreten Typ
formuliert): `automatic_validated_return_to_product` verlangt zusaetzlich zu
den 6.7-Kriterien `direction != Unknown`, `controlDemandAgeMs` vorhanden
**und** `thermalCompatibility.status == Compatible`. `Unavailable`,
`Incompatible` und `Stale` blockieren die automatische Rueckkehr gleichermassen
(`ReturnValidationPending -> AirFallbackActive`, 6.4.7). Der Auswahlkern wird
nativ getestet, indem alle vier `ThermalCompatibility`-Zustaende synthetisch
injiziert werden (9.2).

Die bereits in Revision 3 dokumentierte Abhaengigkeitsaussage (kein
Owner-Gate, P21-M4) bleibt unveraendert: ohne #22/#23-Produzenten bleibt
`automatic_validated_return_to_product` praktisch inert.

### 6.11 Ereignis-, Meldungs- und Revisionsvertrag

**Vollstaendige Neufassung gegenueber Revision 3**, ersetzt die dortige
starre Zweiteilung "Event bei Moduswechsel, sonst nur fluechtige Notice"
durch die in 6.4.9 hergeleitete Grundregel.

```cpp
enum class SensorSelectionDecisionCause : std::uint8_t {
    None,
    StartSelection,           // Start direkt oder Startersatz (persistCommand)
    ProductFailureBlock,      // (a) Allowed -> Blocked
    FallbackToAir,            // (b) Product -> Air, automatisch
    ManualUserFallback,       // (b) Product -> Air, manuell
    AutomaticValidatedReturn, // (b) Air -> Product, automatisch
    ManualUserReturn,         // (b) Air -> Product, manuell
    RecoveryRevalidation,     // (a) Blocked -> Allowed, gleicher Modus
    SafeStateEntry,           // (c)
    ReturnValidationAborted,  // (d)
};

struct SensorSelectionEvent {
    // Tatsaechlicher Wechsel zwischen zwei bereits aktiven Modi. beforeMode
    // ist IMMER gesetzt - dies ist ein Wechsel waehrend eines laufenden
    // Prozesses, kein Startwert (siehe StartSensorSelectionNotice).
    RunSensorMode beforeMode;
    RunSensorMode afterMode;
    SensorSelectionDecisionCause cause;
    std::uint32_t runRevision;
    std::uint64_t monotonicMillis;
    std::optional<std::int64_t> utcUnixSeconds;
    // begrenzte Evidenzreferenz: Qualitaet/Alter der drei Rollen, kein
    // Rohwert bei FAILED.
};

struct SensorSelectionNotice {
    // Laufrelevante Sensorentscheidung OHNE Moduswechsel: (a)
    // Permission-Aenderung, (c) SafeLocked-Eintritt, (d) abgebrochene
    // Rueckkehrvalidierung. Kein leerer Platzhalter (Korrektur ggue.
    // Revision 3).
    SensorSelectionDecisionCause cause;
    std::uint64_t monotonicMillis;
    std::uint32_t runRevision;
    RunSensorMode activeMode;
    // entspricht dem blockReason-Vokabular aus 6.3
    SensorSelectionBlockReason blockReason;
    // begrenzte Evidenzreferenz wie bei SensorSelectionEvent
};

struct StartSensorSelectionNotice {
    // Effektive Startauswahl - kein Wechsel zwischen zwei aktiven Modi,
    // haengt an der ohnehin beim Start erzeugten ersten Revision
    // (persistCommand, nicht persistSensorSelection).
    RunSensorMode requestedMode;
    RunSensorMode effectiveMode;
    std::uint32_t runRevision;
};
```

Genau eine der drei Nutzlasten entsteht pro `persistSensorSelection`-Aufruf
(kein Array noetig, siehe 6.14.5). Reine Diagnosen ohne (a)-(d) aus 6.4.9
(z. B. `ProductFailureDetected -> UserDecisionRequired`) verwenden
ausschliesslich den bestehenden `MessageCode`/`RuntimeMessage`-Pfad, ohne
eigene Struktur und ohne neue Laufrevision.

`SensorSelectionEvent`/`SensorSelectionNotice`/`StartSensorSelectionNotice`
werden ueber die bestehende `runRevision`-Zahl an den Lauf gebunden - nicht
ueber `RunRevision`/`RunChangeReason`. Ein fehlgeschlagenes Journal-Write
darf keine Aktorfreigabe erzeugen. Die dauerhafte Aufbewahrung und
Exportform des Journals bleibt Scope von #19.

### 6.12 Persistierter Sensorselektionszustand und die #17/#18-Recoverygrenze

**Vollstaendige Neufassung gegenueber Revision 3.**

#### 6.12.1 Provenienzmodell (widerspruchsfrei, ersetzt `SensorSelectionOrigin`)

```cpp
enum class SensorSelectionProvenance : std::uint8_t {
    InitialSelection,
    FallbackActive,
    ReturnedToProduct,
    LegacyUnknown,
};

struct PersistedSensorSelectionState {
    SensorSelectionProvenance provenance;
    SensorSelectionDecisionCause lastDecisionCause;
    std::uint32_t lastDecisionRunRevision;
};
```

`RunCommandState::activeRunSensorMode` bleibt die **einzige** kanonische
Quelle des aktiven Modus (keine zweite Quelle, Korrektur ggue. Revision 3s
`PersistedSensorSelectionState::activeMode`-Duplikat).
`PersistedSensorSelectionState` traegt ausschliesslich die Provenienz und
die letzte Entscheidungsursache/-revision - keinen eigenen Modus.

Provenienzuebergaenge (uebernommen wie vorgegeben):

```text
Start direkt Product/Air       -> InitialSelection   (cause = StartSelection)
Startersatz auf Air            -> FallbackActive      (cause = StartSelection)
Product -> Air                 -> FallbackActive      (cause = FallbackToAir | ManualUserFallback)
Air -> Product                 -> ReturnedToProduct    (cause = AutomaticValidatedReturn | ManualUserReturn)
Schema-1-Migration             -> LegacyUnknown        (cause = None, lastDecisionRunRevision = 0)
```

`ProductFailureBlock`, `RecoveryRevalidation`, `SafeStateEntry` und
`ReturnValidationAborted` aktualisieren `lastDecisionCause`/
`lastDecisionRunRevision`, lassen `provenance` aber unveraendert - nur die
vier oben gelisteten Ursachen setzen einen neuen Provenienzwert.

**Invarianten** (durchgesetzt in `validateRunPersistenceSnapshot` und beim
Decode):

- ein aktiver Lauf (`variant != NoActiveRun`) besitzt immer einen gueltigen
  `sensorSelection`-Wert; `RunPersistenceSnapshot::sensorSelection` ist
  `std::optional`, aber `nullopt` ist **ausschliesslich** fuer
  `variant == NoActiveRun` zulaessig - keine zweite Bedeutung fuer
  "Feld fehlt bei aktivem Lauf" (loest die in Revision 3 fehlende
  Abgrenzung zwischen "kein Lauf" und "Ursprung unbekannt");
- `provenance == LegacyUnknown` entsteht ausschliesslich beim
  Schema-1-Legacy-Decode. Kein von `persistSensorSelection` erzeugter
  **neuer Entscheid** darf `LegacyUnknown` setzen (Schreibpfad-Regel).
  `LegacyUnknown` darf jedoch weiterhin **codiert** werden, solange kein
  neuer Entscheid es ueberschrieben hat - ein migrierter Lauf muss ohne
  Moduswechsel weiterhin periodisch checkpointen koennen
  (`checkpointPeriodic` ruft `makeRunPersistenceSnapshot` mit dem
  unveraenderten aktuellen `RunCommandState` auf und muss dabei ein
  `sensorSelection` mit `LegacyUnknown` weiterhin erfolgreich codieren
  koennen). Ein Encode-seitiges Verbot von `LegacyUnknown` waere falsch und
  wuerde jeden periodischen Checkpoint eines migrierten Laufs zum Scheitern
  bringen - **korrigierte Aussage gegenueber einer frueheren Zwischenfassung
  dieses Plans**;
- `lastDecisionRunRevision <= runRevision`;
- `lastDecisionCause == None` genau dann, wenn `lastDecisionRunRevision == 0`
  (beide Richtungen);
- `provenance == FallbackActive` impliziert `activeRunSensorMode == Air`;
  `provenance == ReturnedToProduct` impliziert `activeRunSensorMode ==
  Product` - beide Kombinationen mit dem jeweils anderen Modus sind
  strukturell unmoeglich und werden abgelehnt;
- `InitialSelection` und `LegacyUnknown` sind mit beiden Modi kombinierbar.

#### 6.12.2 Schema-Bump und Migration (`kRunPersistenceSchema`)

Unveraendert in der Substanz seit Revision 3: `kRunPersistenceSchema` 1 ->
2, eine gemeinsame Konstante statt zweier unabhaengiger Definitionen,
Kopf-Datensatz strukturell unveraendert, Legacy-Decoder fuer Schema 1
bleibt erhalten, neuer Decoder fuer Schema 2 mit `sensorSelection`-Feld.

**Praezisierung der Wire-Aenderungen (Review-Befund 7, Zwei-Slot-Vertrag):**

- `readReference` (`run_persistence_codec.cpp` Zeile 711) und
  `validReference` (Zeile 718) vergleichen `schemaVersion` direkt gegen die
  globale Konstante; beide werden auf einen bekannten Wertebereich `{1U,
  2U}` erweitert - **nicht** `{0U, 1U, 2U}`. Der Wert `0U` bleibt
  ausschliesslich der separaten, unveraenderten Pruefung "diese Referenz
  ist strukturell ungenutzt" vorbehalten (`validPreparedHead`:
  `head.current.schemaVersion != 0U`; `validCommittedHead`:
  `head.target.schemaVersion != 0U`) - eine pauschale Erweiterung auf `0U`
  wuerde diese Pruefungen unbeabsichtigt aufweichen;
- `runCheckpointReferenceMatches` (963-975) vergleicht bereits die
  Referenz gegen die physische Envelope **derselben Aufzeichnung**, nicht
  gegen die globale Konstante, und braucht **keine** Aenderung
  (Korrektur einer ungenauen Aussage in Revision 3, siehe 3);
- alle uebrigen direkten `!= kRunPersistenceSchema`-Envelope-Pruefungen
  (Codec ~881, 950; Coordinator `loadAndInitialize`) werden ebenso auf den
  Bereich `{1U, 2U}` erweitert;
- **gemischte bekannte Versionen in `current` (Schema 2) und `fallback`
  (Schema 1) sind der erwartete, nicht abzulehnende Normalzustand
  unmittelbar nach dem ersten Schema-2-Write nach einem Schema-1-Load**:
  `writeSnapshot`s bestehende Fallback-Uebernahmelogik
  (`committed.fallback = currentHead_->current`) traegt beim ersten
  Schema-2-Write automatisch die vorherige Schema-1-`current`-Referenz als
  neue `fallback`-Referenz fort. `validCommittedHead` validiert `current`
  und `fallback` bereits unabhaengig voneinander (kein Cross-Check auf
  gleiche Version) - nach der Bereichserweiterung ist dieser Zustand ohne
  weitere Sonderbehandlung gueltig. Ein Integritaetsfehler liegt nur vor,
  wenn eine einzelne Referenz **nicht** mit ihrer eigenen physischen
  Aufzeichnung uebereinstimmt (`runCheckpointReferenceMatches`), nicht wenn
  `current` und `fallback` unterschiedliche, aber je gueltige Versionen
  tragen.

#### 6.12.3 Abgrenzung zu #17/#18 (Review-Befund 2, ersetzt den Restart-Vertrag aus Revision 3)

**Variante B wird gewaehlt: die Recoveryaktivierung bleibt vollstaendig bei
#18.** Der in Revision 3 skizzierte Ablauf "Restore ->
RestartRevalidationPending -> Sensorentscheidung validieren ->
persistSensorSelection(...) -> Regelung freigeben" setzte stillschweigend
voraus, dass `persistSensorSelection` aus `LoadedActiveRun` heraus schreiben
darf. Das widerspricht dem bereits implementierten #17-Vertrag
(`RecoveryPending`, kein Mutationspfad aus `LoadedActiveRun`) und der
expliziten Zuweisung der Recoveryaktivierung an #18
(`docs/tasks/issue-17-implementation-plan.md`).

#21 liefert dafuer ausschliesslich:

1. den unter 6.12.1 spezifizierten, versioniert persistierten
   `sensorSelection`-Zustand in der von `loadAndInitialize()` bereits heute
   gelieferten `RunPersistenceLoadResult::snapshot`-Projektion (keine
   Aenderung an `loadAndInitialize()` selbst noetig ausser der
   Schema-Bereichserweiterung aus 6.12.2);
2. eine reine, seiteneffektfreie Funktion im `sensor_selection`-Kern
   (Arbeitsname `computeRestartSensorSelection`), die aus
   `PersistedSensorSelectionState`, dem konfigurierten
   `ReturnStrategy`/`SensorPreference` und (sobald verfuegbar) aktuellen
   Sensordaten eine Empfehlung berechnet - ohne selbst zu persistieren oder
   den Coordinator zu mutieren;
3. `persistSensorSelection` (6.14) selbst, aufrufbar mit Ursache
   `RecoveryRevalidation`, aber **ausschliesslich aus dem Zustand `Ready`**
   - identisch zu jedem anderen Aufruf, keine Sondervariante fuer
   `LoadedActiveRun`.

Der tatsaechliche Aufruf dieser Funktion und von `persistSensorSelection`
mit `RecoveryRevalidation`-Ursache sowie der Uebergang des Coordinators von
`LoadedActiveRun` nach `Ready` sind **#18**, nicht #21. Ein Testfall in
diesem Plan kann deshalb nur pruefen "`persistSensorSelection` akzeptiert
eine `RecoveryRevalidation`-Ursache aus `Ready`" - nicht "ein Neustart setzt
end-to-end automatisch die Freigabe fort" (9.3, korrigierte Wortwahl).

**Warum #21 `kRunPersistenceSchema` dennoch anhebt, obwohl die
Restart-Aktivierung #18 bleibt:** #21 selbst schreibt `provenance` waehrend
des normalen `Ready`-Betriebs bei jedem Fallback und jeder Rueckkehr (6.4);
#18 liest diesen Zustand beim Wiederanlauf nur, schreibt ihn in Release 1
nicht selbst. Der Schemabump ist also durch #21s eigene, normale
Schreibpfade begruendet, nicht durch eine vorweggenommene
Recoveryaktivierung.

**Gegenueber Revision 3 entfaellt** der dort skizzierte
`persistRecoverySensorSelection`/`LoadedActiveRun -> Ready`-Pfad ersatzlos
aus Commit 3 (8) - diese Revision reduziert an dieser Stelle Scope, sie
erweitert ihn nicht nur.

Direkte GPIO- oder alte Aktorzustaende werden weiterhin nicht gespeichert.

### 6.13 Zentrale Cross-Field-Validierung

1. `SensorPreference::ProductRequired` + `ProductSensorFailurePolicy::
   FallbackToAirAfterTimeout` -> `IncompatibleCombination` (unveraendert,
   Grundlage von 6.4.10 Mechanismus 1).
2. `SensorPreference::AirOnly` + `ReturnStrategy != RemainOnAirUntilEnd` ->
   `IncompatibleCombination`.
3. `SensorPreference::AirOnly` + `ProductSensorFailurePolicy !=
   FallbackToAirAfterTimeout` -> `IncompatibleCombination`. Die einzige
   kanonische Kombination fuer `AirOnly` ist `FallbackToAirAfterTimeout` +
   `ReturnStrategy::RemainOnAirUntilEnd` (semantisch inert).
4. `SensorPreference::AirOnly` + `fallbackDelaySeconds.has_value()` ->
   `UnexpectedValue`. **Bleibt eine eigene, `AirOnly`-spezifische Regel**
   (keine Zusammenlegung mit Regel 5, siehe Begruendung in 6.4.10): sie
   greift unabhaengig von der konfigurierten Policy, weil `AirOnly`
   `ProductFailureDetected` nie betritt.
5. `ProductSensorFailurePolicy != FallbackToAirAfterTimeout` +
   `fallbackDelaySeconds.has_value()` -> `UnexpectedValue`. **Neue,
   generelle Regel** (schliesst die in Revision 3 aufgezeigte Asymmetrie:
   `fallbackDelaySeconds` ist bei jeder nicht-`FallbackToAirAfterTimeout`-
   Policy tot, nicht nur bei `AirOnly` - trifft insbesondere auch
   `ProductRequired + WaitForUser + fallbackDelaySeconds`).
6. `ProductSensorFailurePolicy::FallbackToAirAfterTimeout` (bei
   `sensorPreference != AirOnly`) ohne gueltige `fallbackDelaySeconds` bei
   `ValidationPurpose::Runnable` - bereits bestehende Regel, hier nur
   referenziert.

Diese Validierung gehoert in die bestehende kanonische
Modell-/Konfigurationskette. Die "mit Luft fortsetzen"-Aktionsbeschraenkung
fuer `ProductRequired` (6.4.10 Mechanismus 2) ist bewusst **kein**
Validierungsfall, weil sie von der zur Kommandozeit aktuellen
`SensorPreference` und einer konkreten Benutzeraktion abhaengt, nicht von
statischen Programmfeldkombinationen.

### 6.14 Atomarer Persistenzpfad fuer laufrelevante Sensorentscheidungen

#### 6.14.1 Neue Typen

Unveraendert seit Revision 3:

```cpp
enum class RunPersistenceMutationKind : std::uint8_t {
    Command,
    Transition,
    SensorSelection,
};

enum class RunCheckpointTrigger : std::uint8_t {
    Command,
    Transition,
    Periodic,
    SensorSelection,
};
```

Beide additive, wire-id-kodierte Erweiterungen ohne Schemabump-Zwang fuer
sich genommen; der Schemabump auf 2 kommt ausschliesslich vom neuen
`sensorSelection`-Strukturfeld (6.12.2).

#### 6.14.2 `writeSnapshot`-Korrektur

Unveraendert seit Revision 3: expliziter `RunPersistenceMutationKind`-
Parameter statt Ableitung aus `commandId.has_value()`. Der Kopf-Invarianten-
Check in `run_persistence_codec.cpp` Zeile 789 bleibt unveraendert korrekt
(verifiziert: prueft nur die `Command`-Aequivalenz, `SensorSelection`
verhaelt sich wie `Transition` bezueglich fehlender `commandId`).

#### 6.14.3 `persistSensorSelection`

```cpp
[[nodiscard]] RunPersistenceResult persistSensorSelection(
    RunCommandState& current, const SensorSelectionDecision& decision,
    const RunCheckpointTime& time);
```

**Erweiterter Geltungsbereich (Review-Befund 3):** anders als in Revision 3
persistiert dieser Pfad nicht mehr nur `ModeChanged`-Entscheidungen, sondern
jede Entscheidung mit einer Ursache `!= None` gemaess 6.4.9 - also auch
`ProductFailureBlock`, `RecoveryRevalidation`, `SafeStateEntry` und
`ReturnValidationAborted`, bei denen `activeRunSensorMode` unveraendert
bleibt, aber `PersistedSensorSelectionState::lastDecisionCause`/
`lastDecisionRunRevision` (und ggf. `peltierPermission`-relevante Ableitung
fuer #23/#24) sich aendern.

Reihenfolge (identisch zu `persistCommand`/`persistTransition`, verifiziert
aus `run_persistence_coordinator.cpp`):

```text
Entscheidung validieren (expectedRunRevision, before-Konsistenz,
  Ursache != None)
-> Kandidatenkopie erzeugen, Sensorentscheidung darauf anwenden
   (ggf. activeRunSensorMode, immer sensorSelection, ggf. runRevision)
-> makeRunPersistenceSnapshot(..., RunCheckpointTrigger::SensorSelection, ...)
-> writeSnapshot(..., periodic=false, ..., commandId=std::nullopt,
   RunPersistenceMutationKind::SensorSelection)
-> bei Erfolg: identische Mutation auf `current` anwenden (RAM-Commit)
-> RunPersistenceResult mit Event-/Notice-Nutzlast und Peltier-Effekt liefern
```

Nur aus `Ready`/`ReadyEmpty` aufrufbar - **keine** Sonderbehandlung fuer
`LoadedActiveRun` (6.12.3).

#### 6.14.4 Ergebnis-, Ereignis- und Aktorwirkungsanschluss (Review-Befund 6)

**Neuer Abschnitt.** `RunPersistenceResult` erhaelt zwei neue, einzelne
(nicht array-foermige) Felder:

```cpp
struct RunPersistenceResult {
    // ... bestehende Felder unveraendert ...
    std::optional<SensorSelectionEvent> sensorSelectionEvent;
    std::optional<SensorSelectionNotice> sensorSelectionNotice;
};
```

Einzelwerte statt der bestehenden begrenzten Arrays (`effects`, `messages`):
ein `persistSensorSelection`-Aufruf erzeugt hoechstens genau eine
Event- **oder** Notice-Nutzlast (nie beides, nie mehrere) - ein Array waere
hier eine unpassende Kapazitaetsabstraktion fuer eine Kardinalitaet von
maximal 1; kein zusaetzliches Kapazitaetslimit noetig.

`CommandEffect` (bereits RAM-only, nie auf dem Wire, siehe 3) erhaelt zwei
neue Werte fuer den nachgelagerten Aktorbezug:

```cpp
enum class CommandEffect : std::uint8_t {
    // ... bestehende Werte unveraendert ...
    SensorSelectionPeltierBlocked,
    SensorSelectionPeltierReleased,
};
```

Verbindliche Regeln:

- `SensorSelectionPeltierBlocked` wird gesetzt, wenn die Entscheidung
  `peltierPermission` von `Allowed` nach `Blocked` aendert
  (`ProductFailureBlock`, `SafeStateEntry`); `SensorSelectionPeltierReleased`,
  wenn sie von `Blocked` nach `Allowed` aendert (`RecoveryRevalidation`,
  sowie ein `ModeChanged`, das gleichzeitig eine Freigabe herstellt).
- Beide Effekte werden **ausschliesslich nach erfolgreichem
  Persistenzcommit** (`writeSnapshot`-Status `Applied`) in `result.effects`
  eingetragen - identisch zum bestehenden Muster in `persistCommand`/
  `persistTransition`, wo `result.effects`/`result.messages` erst nach dem
  erfolgreichen RAM-Apply-Schritt gesetzt werden.
- Bei Schreibfehler, `StaleDecision` oder `CapacityReached` liefert
  `persistSensorSelection` fruehzeitig zurueck, **bevor** `current` oder
  `result.effects`/`sensorSelectionEvent`/`sensorSelectionNotice` gesetzt
  werden - keine Teilwirkung, identisch zum bestehenden Fehlerpfadmuster.

#### 6.14.5 Datei- und Testschnitt

- `run_persistence_contract.hpp/.cpp`: `PersistedSensorSelectionState`,
  `SensorSelectionProvenance`, `RunPersistenceSnapshot`-Erweiterung,
  `makeRunPersistenceSnapshot`/`restoreRunPersistenceSnapshot`-Anpassung,
  erweiterte `validateRunPersistenceSnapshot`-Invarianten (6.12.1).
- `lib/fermentation_app/src/run_commands.hpp`: `SensorSelectionDecisionCause`,
  `SensorSelectionEvent`, `StartSensorSelectionNotice`,
  `SensorSelectionNotice`, neue `CommandEffect`-Werte.
- `run_persistence_codec.hpp/.cpp`: gemeinsame `kRunPersistenceSchema`-
  Konstante (Schema 2), Legacy-/Schema-2-Decoder, neue Wire-IDs, Bereichs-
  pruefungen `{1U, 2U}` in `readReference`/`validReference` (nicht `0U`,
  6.12.2), unveraendertes `runCheckpointReferenceMatches`.
- `run_persistence_coordinator.hpp/.cpp`: `persistSensorSelection`,
  `writeSnapshot`-Signaturaenderung, Ladepruefungen im Bereich `{1, 2}`.
  **Kein** `LoadedActiveRun`-Mutationspfad (6.12.3).
- `sensor_selection.hpp/.cpp`: reine `computeRestartSensorSelection`-
  Funktion fuer die spaetere #18-Integration (6.12.3), ohne Seiteneffekt.
- direkt betroffene Tests unter `test/test_run_persistence_coordinator/`,
  `test/test_run_checkpoint_codec/`.

## 7. Modul- und Abhaengigkeitsgrenzen

Unveraendert seit Revision 3. `SensorSelectionDecisionCause` wird in
`run_commands.hpp` definiert (gemeinsame Abhaengigkeit von Selektor und
Persistenzschicht), damit `run_persistence_contract.hpp` nicht auf
`sensor_selection.hpp` verweisen muss.

Zusaetzliche Grenzpraezisierung dieser Revision: `computeRestartSensorSelection`
(6.12.3) ist Teil des `sensor_selection`-Kerns und ruft weder
`RunPersistenceCoordinator` noch einen Composition-Root-Typ auf - sie ist
eine reine Funktion ueber bereits geladene Daten, aufrufbar sowohl aus #21s
eigenen Tests als auch spaeter aus #18s Aktivierungscode, ohne dass #21
selbst eine Abhaengigkeit auf #18-Code eingeht.

## 8. Voraussichtlicher Datei- und Commit-Schnitt

Gegenueber Revision 3 wird Commit 3 **reduziert** (kein
`LoadedActiveRun`-Aktivierungspfad mehr, siehe 6.12.3) und Commit 4 um die
`ProductRequired`-Aktionsbeschraenkung ergaenzt.

### Commit 1 - Programmschema: Rueckkehrstrategie und verkettete Migration (6.2, 6.13)

Unveraendert seit Revision 3.

### Commit 2 - Auswahlkern und direkte native Unit-Tests (6.1, 6.3, 6.4, 6.6, 6.7, 6.10)

- neu: `lib/fermentation_app/src/sensor_selection.hpp/.cpp` (inklusive
  `computeRestartSensorSelection`, 6.12.3);
- neu: `lib/fermentation_app/src/sensor_selection_limits.hpp`, nur falls
  fuer firmwarefeste Validierungsobergrenzen erforderlich;
- neu: `test/test_sensor_selection/test_sensor_selection.cpp`.

Inhalt: Werttypen, reine Entscheidung, vollstaendiger Zustandsautomat (6.4)
inklusive `ProductRequired`-Aktionsausschluss, `ThermalCompatibility`-
Auswertung mit allen vier Zustaenden, `computeRestartSensorSelection` als
reine Funktion. Keine Persistenz und keine Aktoradapter.

### Commit 3 - Persistenzmechanik: Schema, Migration, atomarer Sensorpfad (6.12, 6.14)

**Reduziert gegenueber Revision 3:** kein
`persistRecoverySensorSelection`/`LoadedActiveRun -> Ready`-Pfad mehr.

- `run_persistence_contract.hpp/.cpp`: `PersistedSensorSelectionState`,
  `SensorSelectionProvenance`, `RunPersistenceSnapshot`-Erweiterung,
  Invarianten (6.12.1);
- `run_commands.hpp`: `SensorSelectionDecisionCause`, `SensorSelectionEvent`,
  `StartSensorSelectionNotice`, `SensorSelectionNotice`, neue
  `CommandEffect`-Werte;
- `run_persistence_codec.hpp/.cpp`: gemeinsame `kRunPersistenceSchema`-
  Konstante, Legacy-/Schema-2-Decoder, `RunCheckpointTrigger::SensorSelection`/
  `RunPersistenceMutationKind::SensorSelection`-Wire-IDs, Bereichspruefungen
  `{1U, 2U}` in `readReference`/`validReference`;
- `run_persistence_coordinator.hpp/.cpp`: `persistSensorSelection` (nur
  `Ready`/`ReadyEmpty`), `writeSnapshot`-Signaturaenderung, `RunPersistenceResult`-
  Erweiterung (6.14.4);
- `test/test_run_checkpoint_codec/test_run_checkpoint_codec.cpp`:
  Schema-2-Round-Trip, Legacy-Migration, Zwei-Slot-Koexistenztests (9.3, aus
  Review-Befund 7);
- `test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp`:
  `persistSensorSelection` fuer alle sechs Ursachenklassen, `mutationKind`-
  Korrektheit, Ergebnis-/Effektanschluss, `LoadedActiveRun` bleibt
  unveraendert `RecoveryPending` (Regressionstest gegen versehentliche
  #21-seitige Aktivierung).

### Commit 4 - Lauf- und Startvertragsanschluss (6.4.10, 6.5, 6.8, 6.9, 6.11)

- `run_commands.hpp/.cpp`: Startmatrix-Pruefung in `decideProgramStart`,
  fester `RunSensorMode::Product`-Vertrag fuer manuelle Laeufe (6.8),
  `ProductRequired`-Aktionsausschluss fuer "mit Luft fortsetzen" (6.4.10),
  Verdrahtung von `sensor_selection` in den Laufkommandopfad;
- direkt betroffene Tests unter `test/test_run_commands/`.

### Commit 5 - fachliche Dokumentation und Abschlussnachweise

Unveraendert seit Revision 3, ergaenzt um die #17/#18-Abgrenzung in
`docs/RECOVERY_AND_INTERRUPTION.md`/`docs/RUN_PERSISTENCE.md`.

## 9. Teststrategie und Testmatrix

In der Planungsphase werden keine Builds und keine produktiven Testlaeufe
ausgefuehrt.

### 9.1 Programmschema, verkettete Migration, Codec (Commit 1)

Unveraendert seit Revision 3.

### 9.2 Unit-Tests des Auswahlkerns (Commit 2)

Unveraendert seit Revision 3, ergaenzt um:

- **vollstaendige `ProductRequired`-Aktionsmatrix** (Review-Befund 1):

  ```text
  ProductRequired × FallbackToAirAfterTimeout
    -> Validierung lehnt Kombination ab (IncompatibleCombination)
  ProductRequired × WaitForUser × Benutzeraktion "mit Luft fortsetzen"
    -> CommandStatus::InvalidInput, kein Zustandswechsel
  ProductRequired × WaitForUser × Produkt wieder gueltig
    -> UserDecisionRequired -> NormalProduct, RecoveryRevalidation
  ProductRequired × WaitForUser × Abbruch (StopRequest)
    -> regulaerer bestehender Stop-Pfad, kein #21-Sonderfall
  ProductRequired × StopToSafeState × Produktausfall
    -> SafeLocked, atomare Revision (SafeStateEntry), kein Air-Pfad je
       erreichbar
  ProductRequired × StopToSafeState × Produkt waehrenddessen wieder gueltig
    -> bleibt SafeLocked (kein impliziter Austritt)
  ```

- **Persistenzwirkung je Ursachenklasse** (Review-Befund 3): `ProductFailureBlock`,
  `RecoveryRevalidation`, `SafeStateEntry`, `ReturnValidationAborted`
  erzeugen je eine atomare Revision auch ohne Moduswechsel;
  `ProductFailureDetected -> UserDecisionRequired` und identische
  wiederholte Bewertungen erzeugen keine;
- `ThermalCompatibility`: alle vier Zustaende (`Unavailable`, `Compatible`,
  `Incompatible`, `Stale`) einzeln getestet; nur `Compatible` traegt
  gemeinsam mit den uebrigen 6.7/6.10-Kriterien zur Freigabe bei;
- `computeRestartSensorSelection`: reine Funktion, getestet mit
  `LegacyUnknown`- und mit konkreten `FallbackActive`/`ReturnedToProduct`-
  Provenienzen als Eingabe, liefert die erwartete Empfehlung ohne
  Seiteneffekt;
- alle uebrigen Tests aus Revision 3 unveraendert (Startmatrix, Produktfehler-
  /Wartezeit-/Rueckkehr-Verhalten, Idempotenz, Kapazitaetsgrenzen).

### 9.3 Persistenzmechanik (Commit 3)

Unveraendert seit Revision 3, ergaenzt um:

- **`persistSensorSelection` fuer jede der sechs Ursachenklassen** (nicht
  nur `ModeChanged`), inklusive korrektem `RunPersistenceResult`-Anschluss
  (`sensorSelectionEvent` **xor** `sensorSelectionNotice`,
  `CommandEffect::SensorSelectionPeltierBlocked`/`-Released` nur nach
  erfolgreichem Commit);
- **`LoadedActiveRun` bleibt unter `persistSensorSelection` unveraendert
  `RecoveryPending`** - expliziter Regressionstest, dass #21 keinen
  Mutationspfad aus diesem Zustand eroeffnet (Review-Befund 2);
- **Zwei-Slot-/Head-Migrationsvertrag** (Review-Befund 7), mindestens:

  ```text
  Schema-1 Head + Schema-1 Current laden
  Schema-1 Current + Schema-1 Fallback laden
  erster Schema-2-Write nach erfolgreichem Schema-1-Laden
  Schema-2 Head referenziert aktuellen Schema-2-Slot und aelteren
    Schema-1-Fallback -> gueltig, kein Integritaetsfehler
  Neustart nach diesem ersten Schema-2-Write
  beschaedigter aktueller Schema-2-Slot -> gueltiger Schema-1-Fallback
  Prepared-Head-Unterbrechung beim Wechsel 1 -> 2
  unbekannte Version < 1 oder > 2 -> UnsupportedSchema
  Schema-2 aktiver Lauf ohne sensorSelection-Feld -> InvalidWireValue
  widerspruechliche Provenienz/Ursache/Revision -> InvalidWireValue
  Rueckkehr Product nach vorherigem Fallback, anschliessender Neustart:
    provenance bleibt ReturnedToProduct rekonstruierbar
  migrierter Lauf (LegacyUnknown) uebersteht periodischen Checkpoint ohne
    Moduswechsel (Regressionstest gegen ein Encode-seitiges
    LegacyUnknown-Verbot, siehe 6.12.1)
  ```

- Reference-Bereichspruefung `{1U, 2U}` in `readReference`/`validReference`
  lehnt `0U` weiterhin an den strukturell vorgesehenen Stellen ab
  (`validPreparedHead`/`validCommittedHead`-Nullpruefungen bleiben scharf).

### 9.4 Konsumenten- und Laufvertragstests (Commit 4)

Unveraendert seit Revision 3, ergaenzt um: `decideManualStart`/
`UserDecisionRequired`-Kommandopfad lehnt "mit Luft fortsetzen" fuer
`ProductRequired`-Laeufe mit `CommandStatus::InvalidInput` ab.

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

## 10. Safety-, Security-, Recovery- und Hardwaregrenzen

- Bei Boot, Reset, `STALE`, `FAILED`, unklarer Auswahl, ungueltigen festen
  Sensoren, fehlendem Persistenzcommit oder unklarer Rollenplausibilitaet
  wird keine neue Peltierfreigabe erzeugt und keine automatische/manuelle
  Rueckkehr zugelassen.
- **Nach einem Neustart bleibt die Peltierfreigabe vollstaendig durch den
  bereits implementierten #17-Vertrag gesperrt** (`LoadedActiveRun` /
  `RecoveryPending`, kein Mutationspfad), bis #18 die Aktivierung liefert -
  #21 traegt dazu keine neue Sperrmechanik bei, sondern stellt sicher, dass
  die dabei gelesene `PersistedSensorSelectionState`-Projektion ausreicht,
  um die konfigurierte `ReturnStrategy` korrekt anzuwenden, sobald #18 diese
  Aktivierung liefert (Korrektur gegenueber Revision 3, die #21 faelschlich
  eine eigene Restart-Freigabewirkung zuschrieb).
- `ProductRequired` erreicht `AirFallbackActive` unter keiner Policy- und
  keiner Benutzeraktionskombination (6.4.10) - strukturell durch
  Validierung (automatischer Pfad) und Kommandoschicht (manueller Pfad)
  doppelt abgesichert.
- Der Produktfuehler ist niemals Ersatz fuer Schrankluft oder Kuehlkoerper.
- Heizen/Kuehlen bleibt ausschliesslich #22/#23 vorbehalten; #21 setzt keine
  Richtung und keinen GPIO.
- Eine Sicherheitsabschaltung ueberstimmt Warte-, Mindest- oder
  Rueckkehrzeiten.
- Quittierung ist keine Rueckkehr- oder Fehlerresetfreigabe; Neustart ist
  kein Fehlerreset.
- Programmschnappschuss, Laufrevision und Sensorentscheidung muessen
  konsistent sein; bei Persistenzunsicherheit bleibt der bisherige fachliche
  Zustand wirksam.
- Ereignisse enthalten keine Secrets, keine WLANdaten, keine Service-PIN,
  keine Rohkonfiguration und keine unbestaetigten Hardwarewerte.
- `TBD_HARDWARE` bleibt bei Sensorbus, ROM-/Rollenverdrahtung und realer
  Quelle offen. `TBD_COMMISSIONING` bleibt bei Fallbackwartezeit,
  Stabilitaetsdauer, Differenz-/Trendgrenzen und Plausibilitaetsgrenzen fuer
  Richtung/Regelanforderungsdauer/Thermik offen.
- Hardwaretests, reales Sensorabziehen, Peltierpulse und thermisches Tuning
  sind nicht Bestandteil dieses Plan-PRs.

## 11. Ressourcen- und Betriebsbudget

- `PersistedSensorSelectionState` bleibt konstant gross (ein
  `SensorSelectionProvenance`, ein `SensorSelectionDecisionCause`, ein
  `uint32_t`) - kleiner als Revision 3s Variante (kein zweiter
  `RunSensorMode`), wenige zusaetzliche Bytes im 8-KB-Checkpoint-Budget
  (`kMaximumRunPersistencePayloadBytes = 8192U`), Teil des ADR-008
  4-MB-Flash-Gesamtbudgets.
- `RunPersistenceResult` waechst um zwei `std::optional`-Felder statt eines
  weiteren begrenzten Arrays - kleinere RAM-Wirkung als eine
  Array-basierte Alternative.
- `ThermalCompatibilityEvidence` ist konstant gross (ein Enum, zwei feste
  Ganzzahlen) - keine unbegrenzte Historie.
- Commit 3 ist gegenueber Revision 3 **kleiner** (kein
  `LoadedActiveRun`-Aktivierungspfad).
- Erwartete RAM-Wirkung: unveraendert klein pro aktivem Lauf; reale
  Byte-/Heapwerte bleiben `TBD_IMPLEMENTATION_BUDGET`.
- Keine PSRAM-, OTA-, Netzwerk- oder Echtzeitabhaengigkeit.

## 12. SOLID-, DRY- und KISS-Bewertung des geplanten Diffs

- **Single Responsibility:** `RunPersistenceCoordinator` bekommt fuer
  Sensorentscheidungen einen eigenen, gleichrangigen Mutationspfad;
  Recoveryaktivierung bleibt vollstaendig #18 zugewiesen, nicht heimlich in
  #21 miterledigt (Korrektur ggue. Revision 3).
- **Open/Closed:** additive Enum-Erweiterungen (`RunPersistenceMutationKind`,
  `RunCheckpointTrigger`, `CommandEffect`) ohne Umbau bestehender Pfade.
- **Liskov:** unveraendert.
- **Interface Segregation:** `RunPersistenceResult` bekommt zwei schmale
  optionale Felder statt eines weiteren generischen Arrays, das inhaltlich
  nicht gepasst haette.
- **Dependency Inversion:** `sensor_selection` haengt nicht auf
  `RunPersistenceCoordinator`; `computeRestartSensorSelection` ist eine
  reine Funktion, die #18 spaeter aufruft, statt dass #21 auf #18 wartet
  oder dessen Verantwortung vorwegnimmt.
- **DRY:** `activeRunSensorMode` bleibt einzige Modus-Quelle, kein Duplikat
  in `PersistedSensorSelectionState` (Korrektur ggue. Revision 3); die
  `AirOnly`- und die generelle `fallbackDelaySeconds`-Regel bleiben bewusst
  getrennt, weil eine Zusammenlegung die `AirOnly`-Einschraenkung faelschlich
  aufheben wuerde (6.4.10) - hier ist Nicht-Zusammenlegen die korrekte
  DRY-Abwaegung, nicht deren Verletzung.
- **KISS:** die `ProductRequired`-Loesung nutzt dieselbe bereits vorhandene
  `SensorPreference`-Bewusstheit des Selektors ein weiteres Mal, statt eine
  zusaetzliche Migrationsnormalisierung mit echter Verhaltensaenderung
  einzufuehren; die Persistenzklassifikation nutzt eine einzige Grundregel
  (6.4.9) statt sechs unabhaengiger Spezialfaelle.

## 13. Offene Ownerentscheidungen und Gates

Zusaetzlich zu den in Revision 3 bereits nicht mehr als Gate gefuehrten
Punkten gilt: die Wahl von Variante B (6.12.3) und die Wahl der
Aktionsbeschraenkung statt Migrationsnormalisierung fuer `ProductRequired`
(6.4.10) sind in diesem Plan bereits als kanonische Loesung getroffen (wie
vom Review verlangt: "Entscheide dich im Plan fuer genau eine Loesung") und
werden dem Owner zur Bestaetigung, nicht zur offenen Auswahl vorgelegt.

| ID | Offene Entscheidung | Planvorschlag / Stopwirkung |
|---|---|---|
| P21-01 | Vollstaendige Startmatrix (6.5) und `ProductRequired`-Aktionsmatrix (6.4.10) freigeben | wie tabelliert; ohne Freigabe kein Startvertrag |
| P21-M1 | Migrationsabbildung 4->5->6 fuer `ReturnStrategy`, inklusive `AirOnly`-Normalisierung (6.2.1) | unveraendert seit Revision 3 |
| P21-M2 | Persistierter Sensorselektionszustand (`PersistedSensorSelectionState`, `kRunPersistenceSchema` 1->2, Provenienzmodell 6.12.1) und die #17/#18-Abgrenzung (Variante B, 6.12.3) | wie in 6.12 spezifiziert. Ohne Entscheidung keine Implementierung |
| P21-M3 | Vertrag fuer produktgefuehrte manuelle Laeufe (6.8) | unveraendert seit Revision 3 |

P21-M4 (Plausibilitaetsvertrag) bleibt wie in Revision 3 kein Gate, sondern
eine Abhaengigkeitsaussage zu #22/#23 (6.10).

Eine Entscheidung, die Schema, Wireformat, Fehlerklasse, Safetyfreigabe,
Hardwareannahme, Issue-/PR-Struktur oder die #17/#18-Grenze veraendert, ist
eine materielle Planabweichung.

## 14. Dokumentations- und Abschlussnachweise

Vor `Ready for review` werden im Draft-PR auf dem exakten HEAD dokumentiert:

- freigegebene Plan-SHA und Zuordnung jedes umgesetzten Planpunkts zu
  Commits;
- tatsaechlich geaenderte Dateien und jede Abweichung vom erwarteten Diff;
- direkte Testbefehle und Ergebnisse, einschliesslich `BLOCKED`/`NOT_RUN`
  fuer nicht angeordnete Volltests oder Hardware;
- `git diff --check`, Secret- und Architekturpruefung;
- Nachweis, dass #20-Vertraege wiederverwendet und keine #17/#18/#22/#23/#24-
  Verantwortungen vorweggenommen wurden - insbesondere, dass
  `LoadedActiveRun -> Ready` weiterhin ausschliesslich #18 zugewiesen bleibt;
- konkrete SOLID-/DRY-/KISS-Pruefung gegen den tatsaechlichen Diff;
- verbleibende `TBD_HARDWARE`, `TBD_COMMISSIONING`,
  `TBD_IMPLEMENTATION_BUDGET` und Owner-Gates (13);
- die P21-M4-Abhaengigkeitsaussage bleibt sichtbar dokumentiert;
- offene Reviewthreads, ohne sie ohne ausdrueckliche Autorisierung zu
  beantworten oder zu schliessen;
- Nachweis, dass PR Draft bleibt und der Owner allein `Ready for review`,
  vollstaendige Remote-CI, Merge oder Branchloeschung steuert.

## 15. Verbindliche `/task`-Taskliste fuer die Umsetzung

```text
/task
[ ] exakten freigegebenen Plan-Commit und Ownerkommentar `PLAN APPROVED` verifizieren
[ ] aktuellen Branch, HEAD, Live-Issue #21, Abhaengigkeiten (inkl. #17/#18-Stand) und Roadmap erneut pruefen
[ ] seit der Planfreigabe geaenderte Quellen, ADRs, Vertraege und lokale Regeln inkrementell lesen
[ ] P21-01, P21-M1 bis P21-M3 aufgeloeste Ownerentscheidungen gegen den Plan abgleichen
[ ] ReturnStrategy-Enum, Feldmaske, Schema 6, Feldeinfuehrungskonstanten und Validierung implementieren
[ ] verkettete Migration 4->5->6 implementieren, inklusive AirOnly-Normalisierung
[ ] Codec-Wire-ID, Payloadgroesse und Bereichsakzeptanz fuer Programmschema implementieren
[ ] Werkskatalog und config/programs.example.yaml auf Schema 6 aktualisieren
[ ] betroffene Testfixtures vollstaendig durchsuchen und anpassen
[ ] Auswahlkern mit vollstaendigem Zustandsautomaten (6.4) implementieren, inklusive ProductRequired-Aktionsausschluss (6.4.10)
[ ] Startmatrix (6.5) in decideProgramStart vor der NotConfirmed-Rueckgabe durchsetzen
[ ] Produktfehler, Wartezeit und alle drei Rueckkehrstrategien implementieren
[ ] festen Vertrag fuer produktgefuehrte manuelle Laeufe (6.8) implementieren
[ ] feste Schrankluft-/Kuehlkoerpersensoren in jeder Peltierfreigabe erzwingen
[ ] vollstaendigen CrossRolePlausibilityContext mit ThermalCompatibilityEvidence implementieren
[ ] SensorSelectionDecisionCause, -Event, -Notice, StartSensorSelectionNotice gemaess Grundregel 6.4.9 implementieren
[ ] PersistedSensorSelectionState mit widerspruchsfreiem Provenienzmodell (6.12.1) implementieren
[ ] kRunPersistenceSchema-Bump auf 2, Legacy-Decoder, Zwei-Slot-Bereichspruefungen implementieren
[ ] persistSensorSelection fuer alle sechs Ursachenklassen implementieren, RunPersistenceResult-Anschluss (6.14.4)
[ ] verifizieren, dass LoadedActiveRun unveraendert RecoveryPending bleibt (keine #21-seitige Aktivierung)
[ ] computeRestartSensorSelection als reine Funktion fuer die spaetere #18-Integration implementieren
[ ] direkte, gezielte Unit-Tests fuer Schema, Migration, Codec, Auswahl, ProductRequired-Matrix, Persistenzmechanik, Zwei-Slot-Koexistenz ausfuehren
[ ] gezielte Laufkommand-/Laufpersistenz-/Codec-Konsumententests ausfuehren
[ ] gezielte Architektur-, Secret-, Format- und git diff --check-Pruefungen ausfuehren
[ ] betroffene Fachvertraege (inkl. #17/#18-Abgrenzung) und docs/ACCEPTANCE_TESTS.md aktualisieren
[ ] docs/ROADMAP.md nur bei tatsaechlicher Status- oder Gatewirkung synchronisieren
[ ] Ressourcenwirkung, begrenzte Puffer und offene Hardware-/Commissioning-Gates dokumentieren
[ ] Review des vollstaendigen aktuellen Diffs gegen Issue, Plan, ADRs und Fachvertraege durchfuehren
[ ] SOLID-, DRY- und KISS-Bewertung gegen den tatsaechlichen Diff durchfuehren
[ ] keinen unbelegten Hardware-, Safety-, Persistenz- oder Bibliotheksentscheid im Diff belassen
[ ] P21-M4-Abhaengigkeitsaussage im PR sichtbar dokumentieren
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
[x] Abhaengigkeiten #14, #17, #18, #20 live verifizieren
[x] Root- und lokale AGENTS-Regeln lesen
[x] Dokumentationsprioritaet und relevante Fachvertraege lesen
[x] bestehenden #20-Sensorqualitaetskern und Lauf-/Persistenzmodelle inventarisieren
[x] fehlende Auswahl-, Fallback-, Rueckkehr- und Ereignisvertraege abgrenzen
[x] offene Ownerentscheidungen und materielle Planabweichungen dokumentieren
[x] SOLID-, DRY- und KISS-Bewertung des geplanten Diffs erstellen
[x] vollstaendige Umsetzung-, Test-, Dokumentations-, Review- und Abschluss-Taskliste erstellen
[x] Plan Revision 1 committen und pushen; Draft-PR aktualisieren (Plan-Commit c505fce6...)
[x] Plan Revision 2 committen und pushen; Draft-PR aktualisieren (Plan-Commit aaeefbdf...)
[x] Plan Revision 3 committen und pushen; Draft-PR und SESSION HANDOVER aktualisieren (Plan-Commit 2e3a0411...)
[x] PR-#99-Reviewbefunde zu Revision 3 gegen Code verifiziert: LoadedActiveRun/RecoveryPending-Grenze (#17/#18), ProductRequired-Luftfallback-Luecke, Persistenzklassifikation, Provenienzmodell, ThermalCompatibility, RunPersistenceResult-Anschluss, Zwei-Slot-Wire-Vertrag
[x] ProductRequired-Loesung entschieden (Aktionsbeschraenkung statt Migrationsnormalisierung, wegen Verhaltensaenderungsrisiko bei Bestandskonfigurationen)
[x] Recovery-Lebenszyklus aufgeloest: Variante B (Recoveryaktivierung bleibt vollstaendig #18), Commit 3 entsprechend reduziert
[x] Grundregel fuer atomar zu persistierende Sensorentscheidungen hergeleitet und auf alle sechs Ursachenklassen angewendet
[x] widerspruchsfreies Provenienzmodell mit expliziten Invarianten geplant, LegacyUnknown-Encode-Korrektur vorgenommen
[x] ThermalCompatibilityEvidence als auswertbaren Ersatz fuer den leeren Platzhaltertyp geplant
[x] RunPersistenceResult-Erweiterung und CommandEffect-Ergaenzung fuer den Ergebnisanschluss geplant
[x] Zwei-Slot-/Head-Migrationsvertrag praezisiert und vollstaendige Testmatrix ergaenzt
[x] Planungs-Taskliste bereinigt (Commit/Push/Handover/PR-Aktualisierung fuer Revision 1-3 als erledigt markiert)
[x] ausschliesslich Plan und notwendige Roadmap-/PR-/Handover-Aktualisierung geaendert
[ ] Plan committen und pushen
[ ] SESSION-HANDOVER-Kommentar auf neuen HEAD aktualisieren (ersetzt den bestehenden Kommentar, keinen zweiten anlegen)
[ ] Draft-PR mit exakter neuer Plan-SHA, aktuellem HEAD und aufgeloesten/verbleibenden Ownerentscheidungen aktualisieren
[ ] HALTED_FOR_OWNER_REVIEW
```
