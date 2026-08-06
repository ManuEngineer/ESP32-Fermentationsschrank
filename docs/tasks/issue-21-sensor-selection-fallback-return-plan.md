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
  6a85c331cf17673d03ec2a231100e5f3af7b916b (Revision 4)
  286ebacda05a202ea203789421e2398a7a868905 (Revision 5)
PLAN_ONLY: YES
IMPLEMENTATION_STARTED: NO
PLAN_STATUS: PLAN_DRAFT_REVIEW_REQUIRED
IMPLEMENTATION_STATUS: IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
CONTEXT_BASELINE_BRANCH: plan/issue-21-sensor-selection-fallback-return
CONTEXT_BASELINE_SHA: 286ebacda05a202ea203789421e2398a7a868905
CONTEXT_HEAD_SHA: 286ebacda05a202ea203789421e2398a7a868905
CONTEXT_PLAN_SHA: 286ebacda05a202ea203789421e2398a7a868905
CONTEXT_REFRESH_MODE: INCREMENTAL
CONTEXT_DELTA: Revision-6-Planpruefung von Include-Schichtung, Apply-Status,
  Clear-/Startpfaden, Safety-Pending, Ready-only-Persistenz, Re-Arm-Zeit und
  Architektur-/Testguards
SOURCE_OF_TRUTH_CONFLICT: NONE
```

Dies ist Revision 6, die fuenfte und abschliessende Plankorrektur. Sie behebt
die im PR-#99-Review vom 2026-08-06 zu
Revision 4 benannten Luecken bei neu konkretisierten Integrationsstellen:
fehlender Eigentuemer und fehlende kanonische Anwendungsfunktion fuer den
fluechtigen Sensorselektions-Laufzeitzustand, fehlender persistierter
Kommandovertrag fuer manuelle Aktionen, widerspruechliches
Policy-Zeitverhalten (Wartezeit fuer eine Policy ohne Wartezeit),
unbegrenzte Wiederholschleife bei der automatischen Rueckkehrvalidierung,
unvollstaendig angeschlossene Start-Notice, missverstaendliche
Effektbezeichnung, fehlende thermische Evidenz-Invarianten sowie eine an
der #17/#18-Grenze zu optimistische Testbehauptung. Produktionscode,
produktive Tests, Toolchain, Buildkonfiguration, Hardwarekonfiguration und
Abhaengigkeiten werden weiterhin in dieser Planungsphase nicht geaendert.
Revision 6 korrigiert zusaetzlich die daraus entstandene Include-Schichtung,
den typisierten Status des RAM-only-Apply, die vollstaendigen Clear-/Start-
Pfade, das Safety-Gate der drei manuellen Aktionen, den Geltungsbereich von
`persistSensorSelection`, den Re-Arm im Luftbetrieb und den
Release-1-Zeitvertrag.

## 2. Live-Issue- und Abhaengigkeitsabgleich

| Quelle | Live-Stand am 2026-08-06 | Bedeutung fuer diesen Plan |
|---|---|---|
| Issue #21 | OPEN, Body-Status `PLANNED_SPEC_PENDING`, keine Kommentare | eigener Plan-first-Draft-PR, keine Implementierungsfreigabe |
| Issue #14 | CLOSED | kanonische Prozesszustaende und Uebergangstopologie stehen zur Verfuegung |
| Issue #17 | Implementiert (`RunPersistenceCoordinator`) | `LoadedActiveRun`/`RecoveryPending` bestehen bereits; Recoveryaktivierung bleibt #18 |
| Issue #18 | OPEN | zustaendig fuer `LoadedActiveRun -> Ready`; #21 liefert nur Daten und eine reine Entscheidungsfunktion |
| Issue #20 | CLOSED | `SensorQualitySnapshot`/`SensorQualityPipeline` bleiben Qualitaetsquelle |
| Epic #5 | OPEN | Issue bleibt Teil des E3-Sensor-/Regel-/Safety-Kerns |
| PR #99 | OPEN, Draft, dieser Plan ist die sechste Revision | Reviewbefunde vom 2026-08-06 zu Revision 4 sowie die Folgepruefung zu Revision 5 sind Grundlage dieser Ueberarbeitung |

Issue #21 hat weiterhin keine Kommentare mit zusaetzlichen Anforderungen.
Issue #17 und #18 werden nicht veraendert.

## 3. Verbindliche Quellen und Lesematrix

Bei Widerspruechen gilt die in `docs/SPECIFICATION_REVIEW.md` festgelegte
Reihenfolge (unveraendert seit Revision 1).

Zusaetzlich zu den in Revision 1-5 gelesenen Quellen wurden fuer diese
Ueberarbeitung konkret nachvollzogen:

- `lib/fermentation_app/src/run_commands.hpp`, `struct CommandDecision`
  (Zeile 300-315): traegt bereits `before`/`after: RunCommandState`
  (vollstaendiger Zustand, nicht nur ein schmaler Ausschnitt),
  `std::optional<StartSummary> startSummary` als bestehendes Muster fuer
  einen kommandobezogenen Zusatzwert - ein analoges
  `std::optional<StartSensorSelectionNotice>`-Feld fuegt sich ohne
  Strukturbruch ein;
- `lib/fermentation_app/src/run_commands.cpp`, `applyRunCommand`
  (Zeile 848-874): **generisch fuer jeden `CommandKind`** - kein Switch pro
  Kommandotyp, sondern eine gemeinsame Staleness-Pruefung gegen einzeln
  aufgezaehlte Felder von `current` gegen `decision.before`
  (`commandSequence`, `processState`, `runRevision`, `messageRevision`,
  `faultRevision`, `criticalSafetyEventPending`, `activeRunId`,
  `activeRunSensorMode`), gefolgt von `current = decision.after;` als
  Ganzes. **Weder `sensorSelectionRuntime` noch `sensorSelection` sind in
  dieser Liste** - eine neue manuelle Sensorwechsel-Aktion wuerde ohne
  Erweiterung dieser Liste eine zwischenzeitlich (durch eine parallele
  automatische Bewertung) veraenderte Auswahlphase stillschweigend
  ueberschreiben koennen (siehe 6.14.3);
- `isRunComfortCommand` (Zeile 54-70) und die (nicht explizit gefundene,
  aber aus dem Muster ableitbare) Erwartung eines vollstaendigen
  `CommandKind`-Switches: beide sind **erschoepfend** ueber `CommandKind`
  geschrieben - ein neuer `CommandKind`-Wert erzwingt beim Compilieren eine
  bewusste Entscheidung an jeder Stelle, an der ueber `CommandKind`
  geschaltet wird (derselbe Absicherungsmechanismus wie der
  `static_assert` bei `kRequiredFields` in `program_model.cpp`);
- `CommandStatus::NoChange` (bereits bestehender Enumwert, referenziert an
  mehreren Stellen in `run_commands.cpp`, z. B. Zeile 300, 532, 772, 800):
  eine Entscheidung mit `status = NoChange` ist nicht `.proposed()` und
  wird von der aufrufenden Schicht direkt beantwortet, ohne
  `persistCommand` ueberhaupt aufzurufen - das bestehende, bereits an
  mehreren Stellen genutzte Muster fuer eine bestaetigte, aber wirkungslose
  Anfrage passt exakt auf einen wirkungslosen `RecheckProduct`.
- `lib/fermentation_app/src/run_commands.cpp::clearActiveRun` entfernt heute
  nur die bestehenden Laufdaten; `run_persistence_coordinator.cpp::
  clearCandidateRun` wiederholt denselben unvollstaendigen Clear. Beide
  Pfade werden durch den in 6.14.6 beschriebenen gemeinsamen Helfer ersetzt.
- `applyTransition` behandelt `TransitionReason::ProductWaitExpired` heute
  zweimal getrennt (Candidate vor dem Write und current danach); beide
  Stellen muessen denselben neuen Sensor-Clear aufrufen. Der bestehende
  `ReadyEmpty`-Pfad liefert bereits `NoActiveRun` fuer laufbezogene
  Operationen und ist der Guard fuer den korrigierten automatischen
  Sensorpfad.

Roadmaps und historische Plaene werden nicht als implementierter Ist-Stand
behandelt.

Die aktuelle Revision hat ausserdem die Include- und Typgrenzen des geplanten
Diffs gegen den bestehenden Code abzugleichen: `run_commands.hpp` definiert
heute `RunSensorMode` und wird von `run_persistence_contract.hpp` eingebunden;
ein Sensorselektionsvertrag darf deshalb keinen vollstaendigen
`RunCommandState` in `sensor_selection.hpp` zurueckgeben und keine
`PersistedSensorSelectionState` im Persistenzvertrag definieren, wenn sie
zugleich Feldtyp von `RunCommandState` ist.

## 4. Ziel und Nicht-Ziele

### Ziel

Zusaetzlich zu den in Revision 1-5 etablierten Zielen macht diese Revision
explizit:

- ein genau benannter, RAM-only Eigentuemer (`RunCommandState::
  sensorSelectionRuntime`) fuer die flüchtige Auswahlphase, Peltier-
  Permission-Momentaufnahme, Fallback-Wartezeit-Start und den
  Rueckkehrvalidierungs-/Re-Arm-Zustand, angewendet durch **genau eine**
  kanonische Entscheidungsfunktion (`applySensorSelectionDecision`, 6.4.11)
  fuer sowohl automatische als auch manuelle Aenderungen; die Funktion
  liefert ausschliesslich eine schmale Mutation und niemals einen
  vollstaendigen `RunCommandState`;
- manuelle Sensorentscheidungen (mit Luft fortsetzen, zum Produkt
  zurueckkehren, Produkt erneut pruefen) sind echte, persistierte,
  idempotente Kommandos ueber den bestehenden `persistCommand`-Pfad, nicht
  nur eine Randnotiz zu `CommandEnvelope`;
- ein policy-bewusstes, in sich widerspruchsfreies Zeitverhalten: eine
  Policy ohne konfigurierbare Wartezeit (`WaitForUser`) wartet nicht auf
  eine nicht existierende Wartezeit; `StopToSafeState` erzeugt in jedem
  Fall genau eine atomare Entscheidung, nie zwei;
- eine verschleisssichere, terminierende Re-Arm-Regel fuer die automatische
  Rueckkehrvalidierung, die eine Bewertungsschleife pro Messzyklus
  strukturell ausschliesst;
- eine vollstaendig angeschlossene Start-Notice ueber
  `CommandDecision`/`RunPersistenceResult`;
- eine Effektbezeichnung, die nicht als direkte Aktorfreigabe missverstanden
  werden kann;
- ausgewertete, nicht nur deklarierte thermische Evidenz-Invarianten;
- eine Testmatrix, die zwischen dem in #21 tatsaechlich testbaren Teil des
  Zwei-Slot-/Recovery-Vertrags und dem erst in #18 erfuellbaren Teil
  unterscheidet, ohne private Coordinatorzustaende zu umgehen.
- eine kompilierbare Include-Schichtung ohne gegenseitige Abhaengigkeit von
  `run_commands.hpp`, `sensor_selection.hpp` und
  `run_persistence_contract.hpp` sowie mit einer einzigen Definition der
  Sensorselektions-Werttypen;
- einen expliziten Inaktivzustand fuer `NoActiveRun`, vollstaendige
  Abbruch-/Abschluss-/Tombstone-Bereinigung und einen Release-1-Vertrag, in
  dem ein zeitbasierter automatischer Re-Arm vor Commissioning deaktiviert
  bleibt.

### Nicht-Ziele

Unveraendert seit Revision 4, ergaenzt um:

- keine Erweiterung von `applyRunCommand` ueber die in 6.14.3 konkret
  benannten zwei neuen Felder hinaus - keine generelle Ueberarbeitung der
  bestehenden Staleness-Pruefung;
- kein vollstaendiger Aggregatsnapshot als Rueckgabewert der
  Sensorselektionsfunktion und keine gegenseitige Include-Abhaengigkeit;
- keine zweite, #21-eigene Altersarithmetik auf
  `ThermalCompatibilityEvidence::evaluatedAtMonotonicMillis` - die
  inhaltliche Alters-/`Stale`-Bewertung bleibt Sache des Produzenten
  (#22/#23), #21 prueft nur auf einen unmoeglichen Zukunftszeitstempel
  (6.10).

## 5. Befund des aktuellen Codes

Unveraendert seit Revision 4, ergaenzt um die in Abschnitt 3 verifizierten
Fundstellen: `applyRunCommand`s unvollstaendige Staleness-Feldliste,
`CommandDecision`s bereits vorhandenes `optional`-Zusatzfeld-Muster
(`startSummary`), `CommandStatus::NoChange` als bereits etabliertes Muster
fuer wirkungslose, aber bestaetigte Anfragen.

## 6. Fachvertraege

### 6.1-6.3

Unveraendert seit Revision 3/4: Eingaben und Rollen (6.1), Rueckkehrstrategie
im Programmmodell (6.2), Ausgabewert und Zustandsseparation (6.3).

### 6.4 Auswahlzustandsautomat

#### 6.4.1 Zustaende

Zustandsliste und Grundtabelle bleiben seit Revision 4 unveraendert fuer die
aktiven Laufphasen (`NormalProduct`, `NormalAir`, `ProductFailureDetected`,
`UserDecisionRequired`, `AirFallbackActive`, `ReturnValidationPending`,
`SafeLocked`, `RestartRevalidationPending`). Revision 6 ergaenzt
`NoActiveRun` ausschliesslich als expliziten Inaktivzustand ausserhalb des
aktiven Automaten. Es gelten die in 6.4.9-6.4.12 dieser Revision
praezisierten Uebergangs-, Zeit- und Persistenzregeln.

#### 6.4.2-6.4.8

Unveraendert seit Revision 4 (Uebergangstabelle als Grundlage, Start der
Fallback-Wartezeit, erneut gueltiger Produktwert, einmalige Verarbeitung
manueller Aktionen, Idempotenz, Abbruch/Wiederaufnahme der
Rueckkehrvalidierung, Revisions-/Kapazitaetsgrenzen), **praezisiert** durch
die folgenden neuen Unterabschnitte. Wo diese Revision einer frueheren
Aussage widerspricht, gilt die neue Aussage.

#### 6.4.9 Grundregel: was wird atomar persistiert, mit Vorrangregel

Unveraendert die vier Bedingungen aus Revision 4:

```text
(a) peltierPermission aendert sich (Allowed<->Blocked)
(b) activeMode aendert sich
(c) der Zustand SafeLocked wird betreten
(d) eine laufende automatische Rueckkehrvalidierung wird abgebrochen
```

**Neu: Vorrangregel fuer mehrdeutige Zyklen (Review-Befund 3/4).** Erfuellt
ein einzelner Bewertungszyklus mehr als eine dieser Bedingungen
gleichzeitig, wird **genau eine** Entscheidung emittiert, nach folgender
Rangfolge (hoechste zuerst):

```text
1. (c) SafeStateEntry
2. (b) ModeChanged
3. (d) ReturnValidationAborted
4. (a) ProductFailureBlock / RecoveryRevalidation
```

`SafeStateEntry` gewinnt strukturell immer. Damit sind beide im Review
benannten Ueberschneidungsfaelle eindeutig geloest, ohne sie einzeln
kodieren zu muessen:

- `StopToSafeState` + Produktausfall: der Zyklus erfuellt sowohl (a)
  (Permission wuerde ohne Policy-Sonderfall Allowed->Blocked) als auch (c);
  nach der Vorrangregel wird **ausschliesslich** `SafeStateEntry` emittiert.
  Es gibt fuer diese Policy nie einen separaten `ProductFailureBlock`-
  Vorlauf (6.4.10) - `(b)` und `(c)` sind aufgrund von 6.4.1s
  "Modus unveraendert beim Eintritt in `SafeLocked`" ohnehin nie
  gleichzeitig erfuellbar, brauchen also keine Rangfolge untereinander.
- gleichzeitiger Air-/Cooling-Ausfall waehrend `ReturnValidationPending`:
  erfuellt sowohl (d) (Rueckkehrvalidierung waere abzubrechen) als auch (c)
  (Air/Cooling ungueltig); nach der Vorrangregel wird ausschliesslich
  `SafeStateEntry` emittiert, der Uebergang erfolgt direkt
  `ReturnValidationPending -> SafeLocked` (bereits so in der
  Uebergangstabelle vorgesehen), nicht ueber einen Zwischenschritt.

Klassifikationstabelle (ersetzt die Tabelle aus Revision 4):

| Bedingung(en) im Zyklus | Emittierte Cause | Persistenzweg (6.14) |
|---|---|---|
| nur (c), oder (c) zusammen mit (a)/(d) | `SafeStateEntry` | `persistSensorSelection` (automatisch) oder `persistCommand` (falls durch eine Benutzeraktion ausgeloest) |
| (b), automatisch (Timeout/automatische Rueckkehr) | `FallbackToAir` / `AutomaticValidatedReturn` | `persistSensorSelection` |
| (b), manuell (Benutzeraktion) | `ManualUserFallback` / `ManualUserReturn` | `persistCommand` (6.14.3) |
| nur (d) | `ReturnValidationAborted` | `persistSensorSelection` |
| nur (a), Allowed -> Blocked, Policy != StopToSafeState | `ProductFailureBlock` | `persistSensorSelection` |
| nur (a), Blocked -> Allowed | `RecoveryRevalidation` | `persistSensorSelection` (automatisch) oder `persistCommand` (falls durch `RecheckProduct` ausgeloest) |
| keine der obigen | `None` | keine Persistenz, hoechstens `MessageCode`-Diagnose |

#### 6.4.10 Policy-Zeitverhalten (Korrektur, Review-Befund 3)

**Widerspruch in Revision 4 behoben:** `WaitForUser` besitzt nach der
Cross-Field-Regel in 6.13 (Regel 5) niemals ein `fallbackDelaySeconds` -
"nach Ablauf der Wartezeit" war fuer diese Policy nie erfuellbar. Korrigierte,
policy-spezifische Ablaeufe:

```text
FallbackToAirAfterTimeout:
  ProductFailureDetected wird betreten
    -> ProductFailureBlock sofort persistiert (Peltier Blocked)
    -> Zustand bleibt ProductFailureDetected fuer die Dauer der
       konfigurierten Wartezeit
    -> automatische Luftfortsetzung (FallbackToAir) nach Ablauf
    -> ODER bestaetigte manuelle Luftfortsetzung (ManualUserFallback)
       jederzeit bereits waehrend der Wartezeit, sofern SensorPreference
       Luft erlaubt und Air/Cooling gueltig sind (vereinheitlichte
       ContinueWithAir-Regel, siehe unten)
  UserDecisionRequired wird fuer diese Policy nie betreten.

WaitForUser:
  ProductFailureDetected wird betreten
    -> ProductFailureBlock sofort persistiert (Peltier Blocked)
    -> im selben Zyklus fluechtiger Uebergang nach UserDecisionRequired
       (keine eigene Revision, siehe 6.4.9-Tabelle "keine der obigen")
    -> bestaetigte Luftfortsetzung (ManualUserFallback) sofort moeglich,
       sofern SensorPreference Luft erlaubt (bei ProductRequired
       strukturell ausgeschlossen, 6.4.13)
    -> Produkt wird waehrenddessen wieder gueltig -> RecoveryRevalidation,
       automatisch, KEINE explizite Benutzerbestaetigung noetig
       (Korrektur: Revision 4 verlangte hier faelschlich zusaetzlich eine
       Benutzeraktion, obwohl derselbe automatische Ruecklauf bereits fuer
       FallbackToAirAfterTimeout in 6.4.4 gilt - beide Policies werden
       hier vereinheitlicht, keine sachliche Begruendung fuer eine
       Ausnahme bei WaitForUser)

StopToSafeState:
  Produktausfall erkannt -> direkt SafeLocked, genau eine atomare
    SafeStateEntry-Entscheidung (6.4.9-Vorrangregel), kein
    ProductFailureBlock-Zwischenschritt.
```

**Vereinheitlichte `ContinueWithAir`-Regel:** die manuelle Aktion "mit Luft
fortsetzen" ist gueltig, sobald `selectionPhase ∈ {ProductFailureDetected,
UserDecisionRequired}` UND `SensorPreference` Luftbetrieb erlaubt (nicht
`ProductRequired`) UND Air/Cooling `VALID` sind - unabhaengig davon, ob eine
Wartezeit existiert oder bereits abgelaufen ist. Das ersetzt zwei
policy-spezifische Regeln (Revision 4 kannte diese Aktion nur ab
`UserDecisionRequired`) durch eine einzige, fuer beide Policies gueltige
Regel.

Fuer produktgefuehrte manuelle Laeufe (6.8) gilt derselbe `WaitForUser`-
Ablauf ohne erfundene Wartezeit - unveraendert in der Substanz, jetzt am
korrigierten Automaten nachvollziehbar.

#### 6.4.11 Kanonischer Laufzeitzustand, Include-Schichtung und Mutation (Revision 6)

Die Typen werden in einer eigenen, werttypenreinen Schicht definiert. Damit
bleibt der bestehende Persistenzvertrag kompatibel, ohne einen Zyklus zu
erzeugen:

```text
sensor_selection_types.hpp
  - RunSensorMode
  - SensorSelectionPhase (einschliesslich NoActiveRun)
  - SensorPeltierPermission
  - ReturnValidationRuntimeState
  - SensorSelectionRuntimeState
  - SensorSelectionProvenance
  - PersistedSensorSelectionState
  - SensorSelectionDecisionCause, BlockReason, Event, Notice,
    StartSensorSelectionNotice und schmale weitere Werttypen

run_commands.hpp
  -> sensor_selection_types.hpp
  -> besitzt RunCommandState::sensorSelectionRuntime und
     optional<PersistedSensorSelectionState> sensorSelection

sensor_selection.hpp/.cpp
  -> sensor_selection_types.hpp und nur schmale bestehende Fachtypen
  -> enthaelt die fachliche Entscheidung und die kanonische
     applySensorSelectionDecision-Funktion

run_persistence_contract.hpp
  -> run_commands.hpp (bestehende Richtung)
  -> definiert keine Sensorselektions-Werttypen
```

`sensor_selection_types.hpp` darf weder `run_commands.hpp` noch
`run_persistence_contract.hpp` einbinden. `sensor_selection.hpp` darf keinen
vollstaendigen `RunCommandState` und keinen Persistenzkoordinator einbinden.
`RunSensorMode` wird aus `run_commands.hpp` in den Werttyp-Header verschoben;
damit koennen Mutation, Persistenzsnapshot und Kommandozustand denselben
vollstaendigen Typ verwenden. Es gibt genau eine Definition jedes genannten
Werttyps und genau eine kanonische fachliche Apply-Funktion.

Der RAM-Eigentuemer bleibt `RunCommandState::sensorSelectionRuntime`:

```cpp
enum class SensorSelectionPhase : std::uint8_t {
    NoActiveRun,
    NormalProduct, NormalAir, ProductFailureDetected, UserDecisionRequired,
    AirFallbackActive, ReturnValidationPending, SafeLocked,
    RestartRevalidationPending,
};

enum class SensorPeltierPermission : std::uint8_t { Allowed, Blocked };

struct ReturnValidationRuntimeState {
    std::optional<std::uint64_t> enteredAtMonotonicMillis;
    std::optional<std::uint32_t> lastObservedProfileRevision;
    // In Release 1 immer leer; zeitbasierter Re-Arm ist deaktiviert.
    std::optional<std::uint64_t> retryNotBeforeMonotonicMillis;
};

struct SensorSelectionRuntimeState {
    SensorSelectionPhase phase{SensorSelectionPhase::NoActiveRun};
    SensorPeltierPermission permission{SensorPeltierPermission::Blocked};
    std::optional<std::uint64_t> fallbackWaitStartedAtMonotonicMillis;
    std::optional<std::uint64_t> lastAppliedMonotonicMillis;
    ReturnValidationRuntimeState returnValidation;
};
```

Der explizite `NoActiveRun`-Wert ist der einzige Inaktivzustand:
`permission == Blocked`, alle Timer-/Evidenz-/Retry-Felder sind leer,
`activeRunSensorMode == nullopt` und `sensorSelection == nullopt`. Der
Start- und Restore-Pfad setzt fuer einen aktiven Lauf alle Werte vollstaendig
neu; er uebernimmt niemals Runtimezustand eines vorherigen Laufs.

Die Apply-Funktion arbeitet nur auf einem schmalen Sensorselektionssichtwert,
nicht auf dem Gesamtaggregat:

```cpp
struct SensorSelectionStateView {
    std::string activeRunId;
    SensorSelectionRuntimeState runtime;
    std::optional<RunSensorMode> activeMode;
    std::optional<PersistedSensorSelectionState> persisted;
    std::uint32_t runRevision{0U};
};

enum class SensorSelectionApplyStatus : std::uint8_t {
    AppliedPersistentCandidate,
    AppliedRamOnly,
    NoChange,
    StaleDecision,
    InvalidDecision,
    TimeWentBackwards,
    CapacityReached,
    InvalidContext,
};

struct SensorSelectionStateMutation {
    SensorSelectionApplyStatus status{
        SensorSelectionApplyStatus::InvalidDecision};
    SensorSelectionRuntimeState runtime;
    std::optional<RunSensorMode> activeMode;
    std::optional<PersistedSensorSelectionState> persisted;
    std::uint32_t resultingRunRevision{0U};
    std::optional<SensorSelectionEvent> event;
    std::optional<SensorSelectionNotice> notice;
    bool persistWorthy{false};
};

[[nodiscard]] SensorSelectionStateMutation applySensorSelectionDecision(
    const SensorSelectionStateView& current,
    const SensorSelectionDecision& decision,
    std::uint64_t nowMonotonicMillis);
```

`SensorSelectionDecision` traegt eine vollstaendige erwartete
`SensorSelectionStateView` (Lauf-ID, Runtime einschliesslich Phase, Permission
und Provenienz, aktiven Modus, persistierten Selektionswert und
Laufrevision).
Vor jeder Mutation vergleicht die Funktion diese Before-Werte, prueft die
Cross-Field-Invarianten und validiert die monotone Zeit. Ein veralteter
`expectedRunRevision`, eine abweichende Before-Phase, geaenderte
Provenienz/Selektionswerte, eine ungueltige Runtimekombination,
`nowMonotonicMillis < lastAppliedMonotonicMillis` oder eine ueberlaufende
Revision liefert den typisierten Status und laesst alle Werte unveraendert.

`AppliedPersistentCandidate` ist nur fuer eine Mutation mit den Bedingungen
aus 6.4.9 (a)-(d) zulaessig und erhoeht `resultingRunRevision` checked.
`AppliedRamOnly` ist nur fuer eine gueltige fluechtige Unterphasenmutation
ohne Laufrevision und ohne Flashwrite zulaessig; `NoChange` erzeugt keine
Mutation. Bei den ablehnenden Statuswerten wird der Rueckgabewert nie blind
uebernommen.

Der automatische Aufrufer bildet eine `SensorSelectionStateView` aus
`current`, ruft die Funktion auf und uebernimmt bei Erfolg nur die benannten
Mutationsfelder ueber einen mechanischen, gemeinsamen Mutationshelfer. Er
setzt nie `current = result.state` und uebernimmt keinen alten
Gesamtaggregatsnapshot. Der manuelle Kommandoaufbau verwendet dieselbe
Funktion, traegt die Mutation in `CommandDecision::after` ein und wird durch
die erweiterte `applyRunCommand`-Stale-Pruefung abgesichert. Die einzige
fachliche Entscheidungsimplementierung bleibt damit gemeinsam.

#### 6.4.12 Re-Arm-Regel fuer die automatische Rueckkehrvalidierung (Revision 6)

Verschleisssicherer, terminierender Ablauf fuer `ReturnValidationPending`.
Der zeitbasierte automatische Re-Arm ist in Release 1 vor der
Commissioning-Entscheidung deaktiviert:

```text
Eintritt (Produkt wird waehrend AirFallbackActive valide, ReturnStrategy =
AutomaticValidatedReturnToProduct):
  -> returnValidation.enteredAtMonotonicMillis = jetzt
  -> returnValidation.lastObservedProfileRevision =
     thermalCompatibility.profileRevision
  -> Stabilitaets-/Evidenzfortschritt beginnt bei Null (kein
     Teilfortschritt aus einem fruehreren Versuch)

Waehrend ReturnValidationPending, pro Bewertungszyklus:
  Evidenz Unavailable, unvollstaendig oder (noch) nicht stabil, oder Stale:
    -> Zustand bleibt ReturnValidationPending
    -> KEINE Revision, KEINE Abbruchmeldung (6.4.9: keine der (a)-(d)
       Bedingungen ist erfuellt - reine RAM-Bewertung)
  Evidenz Incompatible ODER Produkt wird waehrenddessen erneut ungueltig:
    -> genau EIN ReturnValidationAborted (6.4.9), Uebergang nach
       AirFallbackActive (oder SafeLocked bei gleichzeitigem Air-/
       Cooling-Ausfall, Vorrangregel 6.4.9)
    -> keine zeitbasierte Retry-Sperre in Release 1; der aktive Luftmodus
       und seine Permission bleiben erhalten, sofern Air/Cooling gueltig
       sind
  Evidenz Compatible UND alle uebrigen 6.7/6.10-Kriterien erfuellt:
    -> genau EIN AutomaticValidatedReturn (6.4.9), Uebergang nach
       NormalProduct

Re-Arm - ein NEUER Eintritt in ReturnValidationPending nach einem Abbruch
ist in Release 1 nur zulaessig, wenn MINDESTENS eine Bedingung gilt:
  (i)   thermalCompatibility.profileRevision hat sich gegenueber
        returnValidation.lastObservedProfileRevision geaendert;
  (ii)  eine explizite RecheckProduct-Benutzeraktion (6.14.3) loest eine
        neue Pruefung im selben Kommandodurchlauf aus;
  (iii) das Produkt war waehrend des Luftbetriebs zwischenzeitlich erneut
        ungueltig und ist danach erneut valide geworden - ohne einen
        Ruecksprung nach `ProductFailureDetected`; dieser Fall gilt als
        neue, unabhaengige Gelegenheit und hebt die Sperre sofort auf.

Ohne erfuellte Re-Arm-Bedingung bleibt der Zustand nach einem Abbruch in
AirFallbackActive, auch wenn Produkt weiterhin valide ist - kein
sofortiger erneuter Eintritt. Ein ungueltiger optionaler Produktfuehler
waehrend `AirFallbackActive` ist nur ein beobachtetes
Produkt-Recovery-Ereignis; er setzt weder `ProductFailureDetected` noch
den aktiven Luftmodus oder dessen Permission zurueck. `ProductFailureDetected`
wird ausschliesslich betreten, wenn Product tatsaechlich der aktive Modus ist.
`ReturnValidationPending` wird bei erneut ungueltigem Produkt genau einmal
nach `AirFallbackActive` abgebrochen.

Eine spaetere neue gueltige Produkt-Wiedererkennung ist eine neue Evidenz-
generation und darf den Re-Arm ausloesen. Die Tests pruefen dabei explizit,
dass Air-Regelung und Permission erhalten bleiben, solange Air/Cooling
weiterhin gueltig sind.
```

Es gibt in Release 1 keinen produktiven
`kReturnValidationRetryIntervalMillis` und keinen operativen
`TBD_COMMISSIONING`-Wert. Eine spaetere Aktivierung des zeitbasierten Re-Arms
benoetigt einen eigenen, erneut freizugebenden Plan mit einem konkreten,
begruendeten Factory-Wert sowie firmwarefesten positiven Mindest- und
Hoechstgrenzen. Ein deaktivierter oder nicht freigegebener Wert ist
`NotEligible`/`InvalidContext`, erzeugt keinen Retry und keinen Write.

**`RecheckProduct` und die Reihenfolge:** die Aktion ist gueltig aus
`{ProductFailureDetected, UserDecisionRequired, AirFallbackActive}` - die
ersten beiden fuer eine sofortige Neubewertung des Produktfuehlers, der
dritte fuer eine neue Rueckkehrchance im Luftbetrieb. Die neue Bewertung
erfolgt innerhalb derselben Kandidatenentscheidung. Aus jedem anderen
Zustand liefert `RecheckProduct` `CommandStatus::NotAllowedInState`.

Checked-Zeitvertrag fuer alle Zeitberechnungen:

```text
checkedMillisFromSeconds(fallbackDelaySeconds)
checkedAdd(nowMonotonicMillis, durationMillis)
```

Eine Multiplikations- oder Additionsueberlaufpruefung, ebenso wie
`nowMonotonicMillis < runtime.lastAppliedMonotonicMillis`, liefert
`InvalidContext` beziehungsweise `TimeWentBackwards`, veraendert keinen
Zustand und erzeugt keinen sofortigen Retry. Dieselben Regeln gelten fuer
die Fallback-Wartezeitberechnung und fuer jeden spaeter freigegebenen
Retry-Timer.

#### 6.4.13 `ProductRequired` schliesst jeden Luftfallback strukturell aus

Unveraendert seit Revision 4 (zentrale Validierung + Aktionsbeschraenkung),
jetzt formuliert gegen die vereinheitlichte `ContinueWithAir`-Regel aus
6.4.10: die Aktion bleibt fuer `SensorPreference::ProductRequired`
unabhaengig von Zustand und Policy abgelehnt (`CommandStatus::InvalidInput`).

#### 6.4.14 Vollstaendige Policy-/Aktionsmatrix (Review-Befund 3)

| Policy | Luft erlaubt? | Zustand | Aktion/Ereignis | Air/Cooling | Ergebnis |
|---|---|---|---|---|---|
| FallbackToAirAfterTimeout | ja | `ProductFailureDetected` (vor Timeout) | keine | ja | bleibt `ProductFailureDetected`, Blocked |
| FallbackToAirAfterTimeout | ja | `ProductFailureDetected` (vor Timeout) | `ContinueWithAir` | ja | -> `AirFallbackActive`, `ManualUserFallback` |
| FallbackToAirAfterTimeout | ja | `ProductFailureDetected` (vor Timeout) | `ContinueWithAir` | nein | `CommandStatus::InvalidInput` |
| FallbackToAirAfterTimeout | ja | beliebig | `ContinueWithAir` + `criticalSafetyEventPending` | egal | `SafetyRejected`, keine Mutation |
| FallbackToAirAfterTimeout | ja | beliebig | `ReturnToProduct` + `criticalSafetyEventPending` | egal | `SafetyRejected`, keine Mutation |
| FallbackToAirAfterTimeout | ja | beliebig | `RecheckProduct` + `criticalSafetyEventPending` | egal | reine Pruefung; kein Moduswechsel, keine `PermissionRestored`, kein Write |
| FallbackToAirAfterTimeout | ja | `ProductFailureDetected` | Timeout erreicht | ja | -> `AirFallbackActive`, `FallbackToAir` |
| FallbackToAirAfterTimeout | ja | `ProductFailureDetected` | Timeout erreicht | nein | -> `SafeLocked`, `SafeStateEntry` (Vorrang) |
| FallbackToAirAfterTimeout | nein (`ProductRequired`) | - | - | - | durch 6.13 Regel 1 bereits validierungsseitig ausgeschlossen |
| WaitForUser | ja | `ProductFailureDetected` | sofort (kein Timeout) | - | -> `UserDecisionRequired`, fluechtig |
| WaitForUser | ja | `UserDecisionRequired` | `ContinueWithAir` | ja | -> `AirFallbackActive`, `ManualUserFallback` |
| WaitForUser | ja | `UserDecisionRequired` | `ContinueWithAir` | nein | `CommandStatus::InvalidInput` |
| WaitForUser | ja | beliebig | `ContinueWithAir` + `criticalSafetyEventPending` | egal | `SafetyRejected`, keine Mutation |
| WaitForUser | ja | beliebig | `ReturnToProduct` + `criticalSafetyEventPending` | egal | `SafetyRejected`, keine Mutation |
| WaitForUser | ja | beliebig | `RecheckProduct` + `criticalSafetyEventPending` | egal | reine Pruefung; kein Moduswechsel, keine `PermissionRestored`, kein Write |
| WaitForUser | nein (`ProductRequired`) | `UserDecisionRequired` | `ContinueWithAir` | - | `CommandStatus::InvalidInput` (6.4.13) |
| WaitForUser | egal | `ProductFailureDetected`/`UserDecisionRequired` | Produkt wieder gueltig | - | -> `NormalProduct`, `RecoveryRevalidation`, keine Benutzeraktion noetig |
| WaitForUser | egal | `ProductFailureDetected`/`UserDecisionRequired` | `RecheckProduct`, weiterhin ungueltig | - | `CommandStatus::NoChange` |
| WaitForUser | egal | `AirFallbackActive` | Produkt erneut ungueltig | Air/Cooling gueltig | bleibt `AirFallbackActive`, Air-Modus und Permission erhalten |
| StopToSafeState | egal | `NormalProduct` | Produktausfall erkannt | egal | -> `SafeLocked`, genau eine `SafeStateEntry` (kein `ProductFailureBlock`-Vorlauf) |

### 6.5-6.9

Unveraendert seit Revision 3/4: vollstaendige Startmatrix (6.5), Produktfehler
und Ersatzbetrieb als Ueberblick (6.6, jetzt praezisiert durch 6.4.9-6.4.14),
Rueckkehr zum Produktfuehler (6.7), produktgefuehrte manuelle Laeufe (6.8, mit
6.4.10s korrigiertem `WaitForUser`-Ablauf), Phasen/Kuehlen/Halten (6.9).

### 6.10 Rollenuebergreifende Plausibilitaetspruefung

Struktureller Vertrag (`CrossRolePlausibilityContext`,
`ThermalCompatibilityEvidence`) unveraendert seit Revision 4. **Neu:
Invarianten fuer `ThermalCompatibilityEvidence`** (Review-Befund 7):

```text
status ∈ {Compatible, Incompatible, Stale}  => profileRevision != 0
evaluatedAtMonotonicMillis <= evaluationMonotonicMillis
  (des umschliessenden CrossRolePlausibilityContext)
  -> sonst blockReason = InvalidContext, fail-closed wie Unavailable
Unavailable erzeugt nie eine Freigabe
ungueltige Enum-/Revisions-/Zeitkombinationen (z. B. Compatible mit
  profileRevision == 0) blockieren fail-closed mit InvalidContext
```

**Stale-Eigentuemerschaft (kanonisch entschieden, kein Owner-Gate):** der
Produzent (spaeter #22/#23) setzt `status = Stale` verbindlich selbst. #21
fuehrt **keine eigene Altersarithmetik** auf
`evaluatedAtMonotonicMillis` durch - das Feld ist reine Diagnose- und
Zukunftsvertrauens-Referenz (siehe die `<=`-Pruefung oben), nicht
Grundlage einer zweiten, #21-eigenen `Stale`-Bewertung. Ein
implementierender Agent darf hierauf **keine** eigene Altersschwelle
aufbauen - das wuerde denselben Doppel-Eigentuemer-Fehler reproduzieren,
den diese Regel gerade ausschliesst.

Die bereits in Revision 4 dokumentierte Abhaengigkeitsaussage (P21-M4, kein
Owner-Gate) bleibt unveraendert.

### 6.11 Ereignis-, Meldungs- und Revisionsvertrag

**Erweiterung gegenueber Revision 4:** neue Ursachen fuer manuelle
Aktionen, korrigierte Kardinalitaetsaussage, vollstaendiger Anschluss der
Start-Notice, umbenannte Effekte. Alle schmalen Event-/Notice-Werttypen
liegen in `sensor_selection_types.hpp`; weder diese Typen noch
`sensor_selection.hpp` benoetigen `RunCommandState`.

```cpp
enum class SensorSelectionDecisionCause : std::uint8_t {
    None,
    StartSelection,            // ueber persistCommand (Start)
    ProductFailureBlock,       // ueber persistSensorSelection
    FallbackToAir,             // ueber persistSensorSelection (automatisch)
    ManualUserFallback,        // ueber persistCommand (ContinueWithAir)
    AutomaticValidatedReturn,  // ueber persistSensorSelection (automatisch)
    ManualUserReturn,          // ueber persistCommand (ReturnToProduct)
    RecoveryRevalidation,      // ueber persistSensorSelection ODER
                                // persistCommand (RecheckProduct-Erfolg)
    SafeStateEntry,            // ueber persistSensorSelection ODER
                                // persistCommand
    ReturnValidationAborted,   // ueber persistSensorSelection
};
```

**Korrigierte Zaehlung (Review-Befund 9):** acht Nicht-Start-Ursachen
insgesamt (`ProductFailureBlock`, `FallbackToAir`, `ManualUserFallback`,
`AutomaticValidatedReturn`, `ManualUserReturn`, `RecoveryRevalidation`,
`SafeStateEntry`, `ReturnValidationAborted`) - **nicht** sechs, wie in
Revision 4 an mehreren Stellen faelschlich behauptet. Davon werden **sechs**
ausschliesslich oder primaer ueber `persistSensorSelection` transportiert
(automatische Ursachen) und **zwei** (`ManualUserFallback`,
`ManualUserReturn`) ausschliesslich ueber `persistCommand` (manuelle
Moduswechsel, 6.14.3); `RecoveryRevalidation` und `SafeStateEntry` koennen
je nach Ausloeser (automatischer Zyklus vs. `RecheckProduct`-Kommando bzw.
eine waehrend einer manuellen Aktion erkannte Sicherheitslage) ueber beide
Pfade entstehen - beide Pfade rufen dafuer dieselbe
`applySensorSelectionDecision`-Funktion auf (6.4.11), sodass keine zweite
Regelimplementierung entsteht.

```cpp
struct SensorSelectionEvent {
    RunSensorMode beforeMode;
    RunSensorMode afterMode;
    SensorSelectionDecisionCause cause;
    std::uint32_t runRevision;
    std::uint64_t monotonicMillis;
    std::optional<std::int64_t> utcUnixSeconds;
};

struct SensorSelectionNotice {
    SensorSelectionDecisionCause cause;
    std::uint64_t monotonicMillis;
    std::uint32_t runRevision;
    RunSensorMode activeMode;
    SensorSelectionBlockReason blockReason;
};

struct StartSensorSelectionNotice {
    RunSensorMode requestedMode;
    RunSensorMode effectiveMode;
    std::uint32_t runRevision;
};

enum class SensorSelectionUserAction : std::uint8_t {
    ContinueWithAir,
    ReturnToProduct,
    RecheckProduct,
};

struct SensorSelectionCommandRequest {
    CommandEnvelope envelope;
    SensorSelectionUserAction action;
    bool safetyAllowsChange{false};  // nur Zusatzsignal, kein Safety-Gate
};
```

**Korrigierte Kardinalitaetsaussage (Review-Befund 5):** *nicht* "genau eine
der drei Nutzlasten pro `persistSensorSelection`-Aufruf" (das war falsch,
weil die Start-Notice nie ueber `persistSensorSelection` entsteht) -
richtig: **genau eine Nutzlast pro tatsaechlicher Sensorentscheidung**,
transportiert entweder ueber `persistSensorSelection` (automatische
Ursachen) oder ueber `persistCommand` (Start, manuelle Moduswechsel,
`RecheckProduct`-Erfolg).

**Anschluss der Start-Notice (Review-Befund 5):**

```cpp
struct CommandDecision {
    // ... bestehende Felder unveraendert ...
    std::optional<StartSensorSelectionNotice> startSensorSelectionNotice;
    std::optional<SensorSelectionEvent> sensorSelectionEvent;
    std::optional<SensorSelectionNotice> sensorSelectionNotice;
};

struct RunPersistenceResult {
    // ... bestehende Felder unveraendert ...
    std::optional<StartSensorSelectionNotice> startSensorSelectionNotice;
    std::optional<SensorSelectionEvent> sensorSelectionEvent;
    std::optional<SensorSelectionNotice> sensorSelectionNotice;
};
```

`decideProgramStart`/`decideManualStart` fuellen
`CommandDecision::startSensorSelectionNotice` analog zu `startSummary` -
bereits vor der Bestaetigungspruefung fuer die Vorschau (angeforderter UND
effektiver Modus sichtbar), aber `persistCommand` kopiert das Feld nach
`RunPersistenceResult` **erst nach erfolgreichem Commit**, exakt wie es
bereits heute `result.effects = decision.effects` nach dem RAM-Apply-
Schritt tut - keine neue Reihenfolge, dieselbe bestehende. Bei
Schreibfehler bleibt `RunPersistenceResult::startSensorSelectionNotice`
`std::nullopt`; die Vorschau (`CommandDecision`) bleibt davon unberuehrt,
weil sie unabhaengig von der Persistenz erzeugt wird. Direkter Start ohne
Ersatz (Zeile 1/3/4/6/7/9/11 der Startmatrix) erzeugt keine
`StartSensorSelectionNotice` (nur bei tatsaechlichem `requestedMode !=
effectiveMode`).

Die drei Nutzlasttypen werden weiterhin ueber die bestehende `runRevision`-
Zahl an den Lauf gebunden, nicht ueber `RunRevision`/`RunChangeReason`.

### 6.12 Persistierter Sensorselektionszustand und die #17/#18-Recoverygrenze

Die bereits entschiedene Provenienz- und Zwei-Slot-Regel bleibt bestehen; die
Typen liegen jetzt aber ausschliesslich in `sensor_selection_types.hpp`:

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

`RunCommandState` besitzt `std::optional<PersistedSensorSelectionState>
sensorSelection`; der `RunPersistenceSnapshot` verwendet denselben optionalen
Wert. `RunCommandState::activeRunSensorMode` bleibt die einzige kanonische
Quelle des aktiven Modus; `PersistedSensorSelectionState` dupliziert keinen
Modus. Ein aktiver Lauf ist nur gueltig, wenn `activeRunSensorMode`,
`sensorSelectionRuntime` in einer aktiven Phase und `sensorSelection`
konsistent vorhanden sind. Ein `NoActiveRun`-Snapshot enthaelt keinen dieser
aktiven Sensorselektionswerte; das Validator-/Codec-Orakel prueft diese
Schema-2-Invariante auch fuer den Tombstone.

Bootlokale Timer (`fallbackWaitStartedAtMonotonicMillis`,
`lastAppliedMonotonicMillis` und `returnValidation.*`) sind **nicht** Teil von
`PersistedSensorSelectionState` und ausdruecklich ausserhalb des Wireformats.
`restoreRunPersistenceSnapshot` setzt bei einem aktiven Restore die
persistierte Auswahl, aber `sensorSelectionRuntime` fail-closed auf
`RestartRevalidationPending`/`Blocked` und verwirft alle bootlokalen Timer.
Bei `NoActiveRun` setzt es den expliziten `NoActiveRun`-Default.

Die bestehenden Provenienz-Invarianten bleiben unveraendert: ein neuer
Entscheid darf `LegacyUnknown` nicht erzeugen; die Legacy-Provenienz darf
nach Schema-1-Decode weiterhin fuer unveraenderte Checkpoints codiert werden.
`lastDecisionRunRevision <= runRevision`, `None` und Revision 0 treten genau
gemeinsam auf, und `FallbackActive`/`ReturnedToProduct` sind nur mit dem
jeweils passenden aktiven Modus gueltig. Start, Fallback, Rueckkehr und
Schema-1-Migration setzen die bereits entschiedenen Provenienzwerte.

Die bisherige #17/#18-Abgrenzung und Variante B bleiben unveraendert: #21
liefert die Daten und die reine Entscheidungsfunktion, #18 aktiviert keinen
Recoveryzustand vorweg durch diesen Plan.

### 6.13 Zentrale Cross-Field-Validierung

Unveraendert seit Revision 4 (sechs Regeln inklusive der generellen
`fallbackDelaySeconds`-Regel und der separaten `AirOnly`-Regel).

### 6.14 Atomarer Persistenzpfad fuer laufrelevante Sensorentscheidungen

#### 6.14.1 Neue Typen

Unveraendert seit Revision 3/4: `RunPersistenceMutationKind::SensorSelection`,
`RunCheckpointTrigger::SensorSelection`.

**Neu, fuer den manuellen Pfad (Review-Befund 2):**

```cpp
enum class CommandKind : std::uint8_t {
    // ... bestehende Werte unveraendert ...
    ApplySensorSelectionAction,
};
```

`CommandKind` wird an mehreren Stellen erschoepfend geschaltet
(`isRunComfortCommand`, siehe 3); der neue Wert erzwingt dort eine bewusste
Entscheidung. `isRunComfortCommand(CommandKind::ApplySensorSelectionAction)
= false` bleibt als Klassifikation bestehen, damit der bestehende generische
Command-Dispatcher die Aktion nicht pauschal vor der aktionsspezifischen
Pruefung verwirft. Das ist **kein** Safety-Bypass: `decideApplySensorSelectionAction`
prueft intern den autoritativen
`decision.before.criticalSafetyEventPending`-Zustand.

Aktionsspezifische Matrix bei `criticalSafetyEventPending`:

```text
ContinueWithAir -> SafetyRejected, keine Mutation und kein Write
ReturnToProduct -> SafetyRejected, keine Mutation und kein Write
RecheckProduct -> reine Pruefung darf laufen; keine PermissionRestored-,
                  Permission- oder Modusmutation und kein Write, solange
                  das kritische Safety-Ereignis aktiv ist
```

`request.safetyAllowsChange` ist nur ein zusaetzliches, von aussen geliefertes
Pruefsignal und ersetzt die interne Invariante nicht. Ein Ausweg aus
`UserDecisionRequired` entsteht bei offenem kritischem Safety-Ereignis nicht
durch eine Komfortaktion, sondern erst ueber den vorgesehenen Fault-/Reset-
Pfad oder einen eindeutig sicheren Abbruch.

`isPersistedRunCommand(CommandKind::ApplySensorSelectionAction) = true`
(analog zu allen anderen laufwirksamen Kommandos).

#### 6.14.2 `writeSnapshot`-Korrektur und `applyRunCommand`-Erweiterung

`writeSnapshot`-Korrektur unveraendert seit Revision 3/4 (expliziter
`RunPersistenceMutationKind`-Parameter).

**Neu (Review-Blocking 1, notwendige Korrektur einer bestehenden, gemeinsam
genutzten Funktion):** `applyRunCommand` (`run_commands.cpp` Zeile 848-874)
erhaelt zwei zusaetzliche Felder in seiner Staleness-Vergleichsliste:

```cpp
current.sensorSelectionRuntime != decision.before.sensorSelectionRuntime ||
current.sensorSelection != decision.before.sensorSelection ||
```

**Begruendung und Tragweite:** ohne diese Ergaenzung wuerde ein zwischen
Entscheidung und Anwendung durch eine parallele automatische Bewertung
veraenderter Auswahlzustand (z. B. ein inzwischen eingetretener
`SafeLocked`-Zustand) von einer noch auf dem alten Zustand basierenden
manuellen Kommandoentscheidung stillschweigend ueberschrieben - ein
Sicherheits-Lock koennte verloren gehen. Diese Aenderung betrifft **jeden**
bestehenden Kommandopfad, nicht nur den neuen, weil `applyRunCommand`
generisch fuer alle `CommandKind`-Werte verwendet wird; sie wird deshalb im
Dateischnitt (8) explizit als eigener, isoliert ueberpruefbarer Diff
gefuehrt, nicht als Nebeneffekt des neuen Kommandos.

#### 6.14.3 `persistSensorSelection` (automatisch) und `decideApplySensorSelectionAction` (manuell)

**`persistSensorSelection`** unveraendert im Aufbau seit Revision 4, jetzt
mit korrigiertem Geltungsbereich: transportiert genau die **sechs**
automatischen Ursachen (`ProductFailureBlock`, `FallbackToAir`,
`AutomaticValidatedReturn`, `RecoveryRevalidation`, `SafeStateEntry`,
`ReturnValidationAborted`), nicht die beiden manuellen
(`ManualUserFallback`, `ManualUserReturn`). Der automatische laufrelevante
Pfad ist **nur** aus `RunPersistenceCoordinatorState::Ready` und nur fuer
einen aktiven Programm- oder manuellen Lauf aufrufbar. Vor dem Aufruf muessen
`activeRunId`, `activeRunSensorMode`, `sensorSelectionRuntime` in einer
aktiven Phase und `sensorSelection` konsistent vorhanden sein. Aus
`ReadyEmpty` liefert `persistSensorSelection` stabil
`RunPersistenceResultStatus::NoActiveRun` (alternativ der gleichwertig
typisierte `NotEligible`-Status des Fachvertrags), schreibt nichts und hat
keine RAM-Wirkung. Die bestehenden `ReadyEmpty`-Startwege bleiben
`persistCommand` vorbehalten.

**Neu: `decideApplySensorSelectionAction`** (Review-Befund 2, empfohlene
Loesung):

```cpp
[[nodiscard]] CommandDecision decideApplySensorSelectionAction(
    const RunCommandState& current,
    const SensorSelectionCommandRequest& request);
```

```text
decide command
-> ruft applySensorSelectionDecision(current, decision) auf derselben
   Kandidatenkopie auf wie der automatische Pfad (6.4.11) - keine zweite
   Regelimplementierung
-> bei persistWorthy == false: CommandStatus::NoChange, kein persistCommand-
   Aufruf durch die aufrufende Schicht (bestehendes Muster, siehe 3)
-> bei persistWorthy == true: CommandStatus::Proposed, `after` traegt den
   aktualisierten Kandidaten inklusive sensorSelectionRuntime/
   sensorSelection
-> persistCommand mit CommandId (bestehender Pfad, RunPersistenceMutationKind::
   Command, KEIN neuer Mutationskind)
-> RAM apply ueber applyRunCommand (6.14.2, mit den neuen Vergleichsfeldern)
-> Event/Notice/Permission-Effekt erst nach Commit (unveraendertes Muster)
```

Bei `criticalSafetyEventPending` wird diese Ablaufbeschreibung
aktionsspezifisch eingeschraenkt: `ContinueWithAir` und `ReturnToProduct`
enden vor einer Mutation mit `SafetyRejected`; `RecheckProduct` darf nur die
Pruefung ausfuehren und verwirft jede daraus entstehende Modus- oder
Permissionmutation. Der externe Boolean `safetyAllowsChange` darf diese
Regel nicht ueberschreiben.

Damit landet die `CommandId` regulaer im bestehenden, begrenzten
persistierten Kommando-ID-Fenster (`persistedRunCommandIds`), ein
Wiederholungsversuch liefert `CommandStatus::AlreadyProcessed`
(`applyRunCommand`, `containsProcessedCommand`) - keine zweite
Idempotenzimplementierung.

#### 6.14.4 Ergebnis-, Ereignis- und Aktorwirkungsanschluss

**Umbenennung (Review-Befund 6):** `CommandEffect::SensorSelectionPeltierBlocked`/
`SensorSelectionPeltierReleased` aus Revision 4 werden umbenannt in:

```cpp
enum class CommandEffect : std::uint8_t {
    // ... bestehende Werte unveraendert ...
    SensorSelectionPermissionBlocked,
    SensorSelectionPermissionRestored,
};
```

**`SensorSelectionPermissionRestored != Peltier einschalten`** - der Effekt
transportiert ausschliesslich, dass #21s fachliche Voraussetzungen wieder
erfuellt sind (`peltierPermission = Allowed`). #23/#24 pruefen weiterhin
alle uebrigen Freigaben (Regeltotzeit, Aktorsicherheitslogik,
Verriegelungen), bevor ein Aktor tatsaechlich schaltet. Diese Klarstellung
wird an jeder Stelle uebernommen, an der Revision 4 die alten Namen
verwendete (dieser Abschnitt, Testmatrix 9, PR-Beschreibung).

Ansonsten unveraendert seit Revision 4: Effekte nur nach erfolgreichem
Commit, keine Teilwirkung bei Fehlern, Einzelwerte statt Arrays fuer Event/
Notice.

#### 6.14.5 Datei- und Testschnitt

- `sensor_selection_types.hpp` (neu), `run_persistence_contract.hpp/.cpp`,
  `run_commands.hpp`,
  `run_persistence_codec.hpp/.cpp`, `run_persistence_coordinator.hpp/.cpp`:
  unveraendert seit Revision 4 benannte Dateien, jetzt zusaetzlich mit den
  umbenannten `CommandEffect`-Werten und den neuen
`RunPersistenceResult`-Feldern.
- **neu:** `run_commands.hpp/.cpp`: `CommandKind::ApplySensorSelectionAction`,
  `SensorSelectionUserAction`, `SensorSelectionCommandRequest`,
  `decideApplySensorSelectionAction`, `SensorSelectionRuntimeState`,
  `RunCommandState::sensorSelectionRuntime`, `applyRunCommand`-Erweiterung
  (isoliert ueberpruefbar, siehe 8).
- `sensor_selection.hpp/.cpp`: `applySensorSelectionDecision` als die eine
  kanonische Entscheidungs-/Mutationsfunktion (6.4.11),
  `computeRestartSensorSelection` (6.12.3) unveraendert; kein
  `RunCommandState`-Include.
- direkt betroffene Tests: `test/test_run_commands/`,
  `test/test_run_persistence_coordinator/`, `test/test_run_checkpoint_codec/`,
  `test/test_sensor_selection/`.

#### 6.14.6 Gemeinsamer Clear-/Start-Lebenszyklus (Revision 6)

Die beiden bestehenden Pfade `run_commands.cpp::clearActiveRun` und
`run_persistence_coordinator.cpp::clearCandidateRun` werden zu einem
gemeinsamen, oeffentlich erklaerten fachlichen Helfer zusammengefuehrt, zum
Beispiel `clearActiveRunState(RunCommandState&)`, der genau einmal
implementiert wird und von beiden Schichten verwendet wird. Er entfernt
vollstaendig:

```text
activeProgramRun, activeManualRun, processRunSnapshot, activeRunId,
activeRunSensorMode, sensorSelection
```

und setzt `sensorSelectionRuntime` atomar auf den definierten
`NoActiveRun`-/`Blocked`-Default zurueck. Dabei werden
`fallbackWaitStartedAtMonotonicMillis`, `lastAppliedMonotonicMillis`,
`returnValidation.enteredAtMonotonicMillis`,
`returnValidation.lastObservedProfileRevision` und
`returnValidation.retryNotBeforeMonotonicMillis` geleert. Ein
`NoActiveRun`-Snapshot darf danach keinen aktiven Sensorselektionswert
enthalten.

Die Deklaration liegt bei `RunCommandState` in `run_commands.hpp`, die einzige
Definition in `run_commands.cpp`; der Coordinator verwendet sie ueber seine
bestehende `run_commands`-Abhaengigkeit. `clearCandidateRun` bleibt nicht als
zweite Implementierung bestehen.

Der Helfer wird in allen bestehenden und neuen terminalen Pfaden verwendet:
`AbortAndTurnOff`, `AbortAndCool`, `AcknowledgeCompletion` beziehungsweise
regulaerer Abschluss, jeder Tombstone-Pfad sowie
`TransitionReason::ProductWaitExpired`; Candidate und aktueller RAM-Zustand
werden mit denselben Invarianten bereinigt. Der Test fuer einen neuen Start
unmittelbar nach einem beendeten Lauf belegt, dass kein Modus, keine
Provenienz, keine Phase und kein Timer des vorherigen Laufs uebernommen wird.
Startpfade setzen `activeRunSensorMode`, `sensorSelectionRuntime` und
`sensorSelection` immer als vollstaendige neue Gruppe.

## 7. Modul- und Abhaengigkeitsgrenzen

Die Architekturpruefung folgt ADR-013 und erzwingt diese gerichtete
Abhaengigkeit:

```text
device_platform
  <- fermentation_app Werttypen/Fachlogik

sensor_selection_types.hpp
  <- run_commands.hpp
  <- sensor_selection.hpp
  <- run_persistence_contract.hpp indirekt ueber run_commands.hpp

sensor_selection.hpp/.cpp
  <- run_commands.cpp und der automatische Coordinator-Aufrufer

run_commands.hpp <-> sensor_selection.hpp       NICHT zulaessig
sensor_selection.hpp <-> run_persistence_contract.hpp  NICHT zulaessig
```

`device_platform` bleibt frei von Fermentationsbegriffen. `run_commands.hpp`
enthaelt den Aggregatfeldbesitz, `sensor_selection.hpp` nur Fachentscheidung
und schmale Mutations-/Werttypen, `run_persistence_contract.hpp` nur den
Persistenzvertrag. Es gibt keine Parallelfunktion und keine verdeckte zweite
Apply-Implementierung; mechanisches Feldanwenden nach dem kanonischen
Ergebnis ist kein zweiter Fachautomat.

## 8. Voraussichtlicher Datei- und Commit-Schnitt

Gegenueber Revision 4 wird der bisherige Commit 4 in zwei unabhaengig
ueberpruefbare Commits gesplittet, weil er sonst Startmatrix, manuellen
Laufvertrag, `ProductRequired`-Aktionsgate, den neuen Kommandovertrag und
die `applyRunCommand`-Korrektur gleichzeitig getragen haette.

### Commit 1 - Programmschema (6.2, 6.13)

Unveraendert seit Revision 3/4.

### Commit 2 - Auswahlkern (6.1, 6.3, 6.4, 6.7, 6.10)

- `sensor_selection_types.hpp`: ausschliessliche Definition der gemeinsamen
  Sensorselektions-Werttypen, einschliesslich `RunSensorMode`,
  `PersistedSensorSelectionState` und `SensorSelectionRuntimeState`;
- `sensor_selection.hpp/.cpp`: Fachentscheidung und
  `applySensorSelectionDecision` als schmale Mutation, vollstaendiger
  Zustandsautomat inklusive 6.4.9-6.4.14,
  Re-Arm-Regel (6.4.12), `ThermalCompatibility`-Invarianten (6.10),
  `computeRestartSensorSelection`;
- `test/test_sensor_selection/test_sensor_selection.cpp`.

### Commit 3 - Persistenzmechanik: Schema, Migration, automatischer Sensorpfad (6.12, 6.14.1-6.14.2 Codec-Teil, 6.14.4)

Unveraendert seit Revision 4 im Kern (Schema-Bump, `persistSensorSelection`,
`RunCheckpointTrigger`/
`RunPersistenceMutationKind::SensorSelection`), jetzt mit korrigiertem
Geltungsbereich (sechs statt acht Ursachen) und den umbenannten
`CommandEffect`-Werten sowie den neuen `RunPersistenceResult`-Feldern.
Weiterhin **kein** `LoadedActiveRun`-Mutationspfad. Die Snapshot-/Validator-
Invarianten fuer `sensorSelection` und den `NoActiveRun`-Tombstone werden in
diesem Schnitt mit abgedeckt.

### Commit 4 - Kommandovertrag fuer manuelle Sensorentscheidungen (6.4.11, 6.14.2 `applyRunCommand`-Teil, 6.14.3)

**Neuer, isolierter Commit (Review-Scope-Hinweis).**

- `run_commands.hpp/.cpp`: `RunCommandState::sensorSelectionRuntime`,
  `CommandKind::ApplySensorSelectionAction`, `SensorSelectionCommandRequest`,
  `decideApplySensorSelectionAction`, `isRunComfortCommand`/
  `isPersistedRunCommand`-Erweiterung, **`applyRunCommand`-Staleness-Fix**
  (eigener, kompakter Diff mit eigenem Test gegen jeden bestehenden
  Kommandopfad, nicht nur den neuen) sowie der gemeinsame
  `clearActiveRunState`-Aufruf;
- `test/test_run_commands/test_run_commands.cpp`.

### Commit 5 - Startvertragsanschluss (6.5, 6.8, 6.9, 6.11 Start-Notice)

- `run_commands.hpp/.cpp`: Startmatrix-Pruefung in `decideProgramStart`,
  fester manueller Produktlaufvertrag (6.8), `StartSensorSelectionNotice`-
  Anschluss an `CommandDecision`/`RunPersistenceResult`;
- direkt betroffene Tests unter `test/test_run_commands/`.

### Commit 6 - fachliche Dokumentation und Abschlussnachweise

Unveraendert seit Revision 3/4, ergaenzt um die explizite #18-Handover-Notiz
(siehe 9.3) in `docs/RUN_PERSISTENCE.md`/`docs/RECOVERY_AND_INTERRUPTION.md`.
Zusaetzlich wird ein Compile-/Architektur-Guard fuer die Include-Richtung und
die Schema-/Runtime-Invarianten (9.7) ergaenzt. Dieser Guard darf keine
Produktionsabhaengigkeit aus `device_platform_test_support` erzeugen.

## 9. Teststrategie und Testmatrix

### 9.1 Programmschema, verkettete Migration, Codec (Commit 1)

Unveraendert seit Revision 3/4.

### 9.2 Unit-Tests des Auswahlkerns (Commit 2)

Unveraendert seit Revision 4, ergaenzt um:

- **vollstaendige Policy-/Aktionsmatrix** (6.4.14) als Einzeltestfaelle;
- `applySensorSelectionDecision` ist die einzige Stelle, die
  `sensorSelectionRuntime` veraendert - Testfall belegt, dass eine direkt
  konstruierte abweichende Runtime-Kombination von der Funktion selbst nie
  erzeugt wird (z. B. `phase = SafeLocked` mit `permission = Allowed` ist
  unerreichbar);
- **Vorrangregel** (6.4.9): `StopToSafeState` + Produktausfall erzeugt
  genau eine `SafeStateEntry`, kein `ProductFailureBlock`-Vorlauf;
  gleichzeitiger Air-/Cooling-Ausfall waehrend `ReturnValidationPending`
  erzeugt genau eine `SafeStateEntry`, kein separates
  `ReturnValidationAborted`;
- **Re-Arm-Pflichttests** (6.4.12, wörtlich aus dem Review uebernommen):
  10.000 identische Bewertungen mit `Unavailable` erzeugen keinen
  Flashwrite; wiederholtes `Incompatible` erzeugt hoechstens einen Abort
  pro Versuch; eine neue Evidenzgeneration (`profileRevision`-Aenderung)
  startet genau einen neuen Versuch; `Compatible` nach vollstaendiger
  Stabilitaet erzeugt genau eine Rueckkehr; Produkt faellt waehrend
  `ReturnValidationPending` aus und rearmt erst nach einer neuen gueltigen
  Wiedererkennung;
- `RecheckProduct` hebt die Retry-Sperre innerhalb derselben Entscheidung
  auf, bevor die ausgeloeste Neubewertung erfolgt (Reihenfolge-Testfall);
- `RecheckProduct` aus einem nicht zulaessigen Zustand liefert
  `CommandStatus::NotAllowedInState`;
- **typisierte Apply-Status-/Stale-Tests:** eine vorab erzeugte Entscheidung
  wird nach Aenderung von Runtimephase, Provenienz, aktivem Selektionswert
  oder Laufrevision mit `StaleDecision` abgelehnt; der aktuelle Zustand bleibt
  byteweise unveraendert. Gleiches gilt fuer `InvalidDecision`,
  `InvalidContext`, `TimeWentBackwards` und `CapacityReached`;
- RAM-only-Unterphasenwechsel liefert `AppliedRamOnly`, veraendert keine
  Laufrevision und schreibt nicht; eine persistenzwuerdige Entscheidung
  liefert `AppliedPersistentCandidate`. `NoChange` erzeugt keine Mutation;
- monotone Zeit rueckwaerts sowie checked-Addition/Millisekundenueberlauf
  fuer Fallback- und Retry-Berechnung werden jeweils fail-closed ohne
  Zustandsaenderung und ohne sofortigen Retry belegt;
- `ThermalCompatibilityEvidence`-Invarianten (6.10): `profileRevision == 0`
  bei `Compatible`/`Incompatible`/`Stale` -> `InvalidContext`;
  `evaluatedAtMonotonicMillis` in der Zukunft -> `InvalidContext`; #21
  fuehrt nachweislich keine eigene Altersschwelle auf
  `evaluatedAtMonotonicMillis` aus (Regressionstest gegen eine versehentlich
  hinzugefuegte zweite `Stale`-Bewertung).

### 9.3 Persistenzmechanik: automatischer Pfad (Commit 3)

Unveraendert seit Revision 4, mit korrigierter Ursachenzahl (sechs
automatische statt acht) und den umbenannten Effektwerten.

**Praezisierung der #17/#18-Testgrenze (Review-Befund 8, ersetzt eine zu
optimistische Aussage in Revision 4):**

In #21 testbar (ausschliesslich ueber die bestehende oeffentliche API,
kein Zugriff auf private Coordinatorzustaende):

```text
Codec-/Contract-Test mit gemischtem Schema-2-Current und Schema-1-Fallback
Head-/Referenzvalidierung beider bekannten Versionen ({1U, 2U})
Schema-1 laden endet in LoadedActiveRun / RecoveryPending
persistSensorSelection aus LoadedActiveRun bleibt RecoveryPending
  (Regressionstest, siehe Revision 4)
Schema-2-Schreiben aus einem regulaeren Ready-Fixture (nicht aus einem
  ueber loadAndInitialize() geladenen LoadedActiveRun-Zustand)
```

Nicht in #21 testbar, sondern erst in #18 zu erfuellen (in diesem Plan
**nicht** als #21-Testzeile behauptet):

```text
tatsaechliche Recoveryaktivierung eines geladenen Schema-1-Laufs
erster Schema-2-Commit nach dieser Aktivierung
anschliessender Neustart mit Schema-2-Current und Schema-1-Fallback
Aktorfreigabe erst nach persistierter Recoveryentscheidung
```

Diese Uebergabe wird zusaetzlich zu diesem Plan als benannter Abschnitt in
`docs/RUN_PERSISTENCE.md` oder `docs/RECOVERY_AND_INTERRUPTION.md`
dokumentiert (Commit 6), damit sie bei der #18-Planung nicht erneut
recherchiert werden muss.

Zusaetzliche laufbezogene Pflichttests pruefen den aktiven-only-Vertrag:

```text
persistSensorSelection aus ReadyEmpty -> NoActiveRun/NotEligible,
  kein Store-Write und keine RAM-Aenderung
Ready ohne activeRunId, activeRunSensorMode, sensorSelectionRuntime oder
  sensorSelection -> InvalidDecision/NotEligible, kein Write
NoActiveRun-Schema-2-Tombstone enthaelt keinen aktiven Sensorselektionswert
```

### 9.4 Kommandovertrag fuer manuelle Sensorentscheidungen (Commit 4)

**Neuer Abschnitt.**

- `ContinueWithAir`/`ReturnToProduct`/`RecheckProduct` je aus jedem
  zulaessigen und mindestens einem unzulaessigen Ausgangszustand;
- Wiederholungsversuch derselben `CommandId` liefert `AlreadyProcessed`,
  keine zweite Wirkung;
- **`applyRunCommand`-Staleness-Regressionstest** (Review-Blocking 1):
  `ContinueWithAir` wird gegen `UserDecisionRequired` entschieden; eine
  zwischenzeitliche automatische Bewertung fuehrt zu `SafeLocked`; die
  Anwendung des ersten Kommandos liefert `CommandStatus::StaleState`, der
  `SafeLocked`-Zustand bleibt erhalten;
- derselbe Regressionstest wird gegen mindestens einen weiteren,
  bestehenden `CommandKind` (z. B. `AdjustRun`) wiederholt, um zu
  bestaetigen, dass die erweiterte Vergleichsliste bestehende Pfade nicht
  bricht;
- `RunPersistenceResult`-Anschluss: `sensorSelectionEvent`/
  `sensorSelectionNotice` nur nach erfolgreichem Commit gesetzt;
  `CommandEffect::SensorSelectionPermissionBlocked`/`-Restored` korrekt
  gesetzt, keine Verwechslung mit einer direkten Aktorfreigabe (Testname
  spiegelt das wider).
- **vollstaendige Safety-Pending-Matrix:** `criticalSafetyEventPending` mit
  `ContinueWithAir` und `ReturnToProduct` liefert jeweils
  `SafetyRejected`; `RecheckProduct` darf pruefen, aber weder Modus noch
  Permission restaurieren und nichts persistieren. Der Test setzt den
  externen `safetyAllowsChange`-Wert sowohl `true` als auch `false`, um zu
  belegen, dass er die interne Safety-Invariante nicht ersetzt;
- **Clear-/Tombstone-/Start-Regressionen:** `AbortAndTurnOff`,
  `AbortAndCool`, `AcknowledgeCompletion`/regulaerer Abschluss,
  `ProductWaitExpired` und `NoActiveRun`-Tombstone pruefen alle neuen
  Felder (`sensorSelectionRuntime`, `sensorSelection`,
  `activeRunSensorMode`) sowie Timer, Provenienz und Schema-2-Projektion;
  ein neuer Start unmittelbar danach beginnt mit einer vollstaendigen neuen
  Sensorselektion.

### 9.5 Startvertragsanschluss (Commit 5)

Unveraendert seit Revision 4, ergaenzt um: `StartSensorSelectionNotice`
erscheint in `CommandDecision` bereits vor Bestaetigung, in
`RunPersistenceResult` erst nach Commit; Schreibfehler beim Start erzeugt
keine scheinbar ausgefuehrte Start-Notice; direkter Start ohne Ersatz
erzeugt keine Notice.

### 9.6 Gezielte Ausfuehrung nach Freigabe

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

### 9.7 Architektur-, Compile- und Invariant-Guards (Commit 6)

Der Implementierer ergaenzt einen gezielten Guard, der ohne Produktionslogik
folgendes nachweist:

```text
sensor_selection_types.hpp, sensor_selection.hpp, run_commands.hpp und
run_persistence_contract.hpp lassen sich in isolierten Include-TUs in jeder
vorgesehenen Reihenfolge kompilieren; kein gegenseitiger Include-Zyklus
device_platform enthaelt keinen Fermentationsbegriff
genau eine kanonische applySensorSelectionDecision-Deklaration/Definition;
keine Parallelfunktion und kein vollstaendiger RunCommandState-Rueckgabewert
aktiver Lauf => Runtime, activeRunSensorMode und sensorSelection vorhanden
NoActiveRun => alle drei aktiven Sensorwerte fehlen bzw. sind Inaktiv-Default
persistSensorSelection aus ReadyEmpty schreibt nichts und aendert kein RAM
Safety-Pending-Matrix deckt alle drei Aktionen ab
Clear-/Startpfade und Retry-/Zeitfehlerfaelle sind abgedeckt
```

Dazu werden der bestehende Architektur-Checker, Compile-only-Header-TUs und
die nativen Fachtests verwendet; es wird kein `#include` aus
`device_platform_test_support` in Produktionsheader eingebracht.

## 10. Safety-, Security-, Recovery- und Hardwaregrenzen

Unveraendert seit Revision 4, ergaenzt um:

- die Vorrangregel (6.4.9) garantiert, dass ein sicherheitsrelevanter
  Zustand (`SafeLocked`) nie durch eine gleichzeitig moegliche schwaechere
  Klassifikation verdraengt wird;
- die Re-Arm-Sperre (6.4.12) verhindert eine unbegrenzte automatische
  Wiederholung derselben abgelehnten Rueckkehr; in Release 1 ist der
  zeitbasierte Re-Arm deaktiviert, sodass kein unfreigegebener
  Commissioning-Wert eine Flash-Schreibschleife erzeugen kann;
- `SensorSelectionPermissionRestored` ist ausdruecklich **keine**
  Aktorfreigabe - nachgelagerte Safety-/Regel-/Aktorlogik (#23/#24) prueft
  weiterhin alle uebrigen Bedingungen, bevor ein Aktor schaltet;
- die `applyRunCommand`-Erweiterung (6.14.2) verhindert, dass eine
  zwischenzeitlich eingetretene Sicherheitssperre durch eine veraltete
  manuelle Kommandoentscheidung stillschweigend rueckgaengig gemacht wird.
- `ContinueWithAir` und `ReturnToProduct` sind bei
  `criticalSafetyEventPending` immer `SafetyRejected`; `RecheckProduct` ist
  nur eine Pruefung ohne Modus-/Permissionwirkung. Der externe
  `safetyAllowsChange`-Boolean ist kein Ersatz fuer diese interne Invariante.
- Jede Ablehnung des typisierten Apply-Status ist fail-closed: keine
  Teilmutation, kein alter Aggregatsnapshot und kein Write.
- Alle aktiven Sensorselektionsfelder werden bei Abbruch, Abschluss,
  `ProductWaitExpired` und Tombstone entfernt; ein neuer Lauf setzt sie
  vollstaendig neu.

## 11. Ressourcen- und Betriebsbudget

Unveraendert seit Revision 4, ergaenzt um:

- `SensorSelectionRuntimeState` ist RAM-only und konstant gross (ein Phase-
  Enum, ein Permission-Enum, vier `optional<uint64_t>` und ein
  `optional<uint32_t>`)
  - keine Wireformat-Wirkung, keine Aenderung am 8-KB-Checkpoint-Budget;
  - ein zeitbasierter automatischer Retry ist in Release 1 deaktiviert;
    `TBD_COMMISSIONING` wird nicht als Laufzeitwert eingebaut.

## 12. SOLID-, DRY- und KISS-Bewertung des geplanten Diffs

Unveraendert seit Revision 4, ergaenzt um:

- **Single Responsibility:** `applySensorSelectionDecision` ist die einzige
  Stelle, die Automat-Regeln kennt; `persistSensorSelection` und
  `decideApplySensorSelectionAction` sind duenne Transport-/
  Persistenzwrapper darum (Korrektur ggue. Revision 4, die implizit zwei
  Schreiber zuliess).
- **Dependency inversion:** die Fachfunktion kennt weder `RunCommandState`
  noch den Persistenzkoordinator; beide Aufrufschichten adaptieren die
  schmale Mutation. Der Werttyp-Header verhindert den Include-Zyklus.
- **DRY:** die Vorrangregel (6.4.9) ersetzt mehrere Einzelfall-
  Entscheidungen (`StopToSafeState`, gleichzeitiger Air-/Cooling-Ausfall)
  durch eine einzige Regel; `RecoveryRevalidation`/`SafeStateEntry` teilen
  sich dieselbe Anwendungsfunktion unabhaengig vom Ausloeser.
- **KISS:** die Re-Arm-Regel nutzt ausschliesslich bereits vorhandene
  Bausteine (Evidenzgeneration und bestehendes Kommandovokabular); die
  zeitbasierte Variante bleibt bis zu einem konkreten Commissioning-Wert
  deaktiviert. Checked-Zeitberechnung ist ein gemeinsamer Guard fuer
  Fallback und spaetere Retry-Aktivierung.

## 13. Offene Ownerentscheidungen und Gates

Unveraendert seit Revision 4 (P21-01, P21-M1 bis P21-M3 als echte Gates,
P21-M4 als Abhaengigkeitsaussage). Diese Revision trifft zusaetzlich
folgende, dem Owner zur Bestaetigung (nicht zur offenen Auswahl)
vorgelegte kanonische Entscheidungen innerhalb von P21-01 (Aktions-/
Zeitverhalten) und P21-M2 (Laufzeitzustand/Persistenzweg):

- Vorrangregel `SafeStateEntry` schlaegt alle anderen Klassifikationen
  (6.4.9);
- vereinheitlichte `ContinueWithAir`-Regel unabhaengig von Timeout-Status
  (6.4.10);
- manuelle Moduswechsel ueber `persistCommand`/neuen `CommandKind`, nicht
  ueber `persistSensorSelection` (6.14.3);
- Re-Arm-Bedingungen (i)-(iv) fuer die automatische Rueckkehrvalidierung
  (6.4.12);
- Stale-Eigentuemerschaft beim Produzenten, keine #21-eigene
  Altersarithmetik (6.10).
- Include-Schichtung und schmale Mutation gemaess 6.4.11/7;
- typisierte Apply-Statuswerte mit unveraenderlicher Ablehnung;
- Clear-/Start-/Tombstone-Invarianten und Safety-Pending-Matrix;
- Release-1-Deaktivierung des zeitbasierten Re-Arms sowie checked
  monotone Zeitberechnung.

## 14. Dokumentations- und Abschlussnachweise

Unveraendert seit Revision 4, ergaenzt um: die #18-Handover-Notiz (9.3)
wird als eigener, benannter Abschnitt in einem Fachvertrag nachgewiesen,
nicht nur im Plan-PR erwaehnt. Die neue Revision dokumentiert ausserdem die
Include-/Compile-Guards, den NoActiveRun-Tombstone-Vertrag, die
Safety-Pending-Matrix und den deaktivierten Release-1-Retry im PR-Nachweis.

## 15. Verbindliche `/task`-Taskliste fuer die Umsetzung

```text
/task
[ ] exakten freigegebenen Plan-Commit und Ownerkommentar `PLAN APPROVED` verifizieren
[ ] aktuellen Branch, HEAD, Live-Issue #21, Abhaengigkeiten (inkl. #17/#18-Stand) und Roadmap erneut pruefen
[ ] seit der Planfreigabe geaenderte Quellen, ADRs, Vertraege und lokale Regeln inkrementell lesen
[ ] P21-01, P21-M1 bis P21-M3 aufgeloeste Ownerentscheidungen gegen den Plan abgleichen
[ ] ReturnStrategy-Enum, Feldmaske, Schema 6, verkettete Migration implementieren
[ ] Codec-/Katalog-/Beispielkonfigurationsaenderungen implementieren
[ ] sensor_selection_types.hpp als einzige Werttypquelle ohne Include-Zyklus
    einfuehren und die vier Header in isolierten Compile-TUs absichern
[ ] SensorSelectionRuntimeState, typisierte Apply-Statuswerte,
    applySensorSelectionDecision als schmale Mutation und vollstaendigen
    Zustandsautomaten (6.4.9-6.4.14) implementieren
[ ] Vorrangregel und Re-Arm-Regel implementieren und mit den Pflichttests belegen
[ ] Release-1-Zeit-Re-Arm deaktiviert halten; checked Zeit-/Ueberlauf- und
    TimeWentBackwards-Vertrag fuer Fallback und spaetere Retry-Aktivierung
    implementieren
[ ] ThermalCompatibilityEvidence-Invarianten implementieren, keine eigene Altersarithmetik ergaenzen
[ ] PersistedSensorSelectionState, kRunPersistenceSchema-Bump,
    persistSensorSelection nur aus Ready mit aktivem Lauf fuer die sechs
    automatischen Ursachen implementieren; ReadyEmpty ohne Write/RAM-Wirkung
[ ] CommandKind::ApplySensorSelectionAction, SensorSelectionCommandRequest, decideApplySensorSelectionAction implementieren
[ ] applyRunCommand um sensorSelectionRuntime/sensorSelection erweitern und gegen bestehende Kommandopfade regressionstesten
[ ] gemeinsame clearActiveRunState-Invarianten in allen Abbruch-, Abschluss-,
    ProductWaitExpired- und Tombstonepfaden sowie vollstaendige neue Starts
    implementieren
[ ] Safety-Pending-Matrix intern gegen criticalSafetyEventPending pruefen;
    safetyAllowsChange nicht als Ersatz verwenden
[ ] StartSensorSelectionNotice an CommandDecision/RunPersistenceResult anschliessen
[ ] CommandEffect auf SensorSelectionPermissionBlocked/-Restored umbenennen und dokumentieren, dass dies keine Aktorfreigabe ist
[ ] Startmatrix (6.5) in decideProgramStart durchsetzen, ProductRequired-Aktionsausschluss (6.4.13) implementieren
[ ] festen Vertrag fuer produktgefuehrte manuelle Laeufe (6.8) implementieren
[ ] verifizieren, dass LoadedActiveRun unveraendert RecoveryPending bleibt
[ ] computeRestartSensorSelection als reine Funktion fuer die spaetere #18-Integration implementieren
[ ] direkte, gezielte Unit-Tests fuer alle in Abschnitt 9 gelisteten Faelle ausfuehren
[ ] gezielte Architektur-, Compile-, Invariant-, Secret-, Format- und git diff --check-Pruefungen ausfuehren
[ ] betroffene Fachvertraege inklusive #18-Handover-Notiz und docs/ACCEPTANCE_TESTS.md aktualisieren
[ ] docs/ROADMAP.md nur bei tatsaechlicher Status- oder Gatewirkung synchronisieren
[ ] Review des vollstaendigen aktuellen Diffs gegen Issue, Plan, ADRs und Fachvertraege durchfuehren
[ ] SOLID-, DRY- und KISS-Bewertung gegen den tatsaechlichen Diff durchfuehren
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
[x] Plan Revision 4 committen und pushen; Draft-PR und SESSION HANDOVER aktualisieren (Plan-Commit 6a85c331...)
[x] PR-#99-Reviewbefunde zu Revision 4 gegen Code verifiziert: CommandDecision-Struktur, applyRunCommand-Generizitaet und Staleness-Luecke, CommandStatus::NoChange-Muster
[x] Eigentuemer und kanonische Anwendungsfunktion fuer den fluechtigen Sensorselektions-Laufzeitzustand geplant (SensorSelectionRuntimeState, applySensorSelectionDecision)
[x] manuelle Sensorentscheidungen als echte persistierte Kommandos modelliert (CommandKind::ApplySensorSelectionAction, decideApplySensorSelectionAction)
[x] applyRunCommand-Staleness-Fix als eigenen, isoliert testbaren Diff geplant
[x] Policy-Zeitverhalten korrigiert (WaitForUser ohne Wartezeit, vereinheitlichte ContinueWithAir-Regel, StopToSafeState als eine atomare Entscheidung ueber Vorrangregel)
[x] Re-Arm-Regel gegen Bewertungsschleife der automatischen Rueckkehrvalidierung geplant, inklusive Pflichttests
[x] Start-Notice vollstaendig an CommandDecision/RunPersistenceResult angeschlossen, falsche Kardinalitaetsaussage korrigiert
[x] CommandEffect auf SensorSelectionPermissionBlocked/-Restored umbenannt
[x] ThermalCompatibilityEvidence-Invarianten und Stale-Eigentuemerschaft festgelegt
[x] Testmatrix an die #17/#18-Grenze angepasst (kein End-to-End-Testanspruch in #21)
[x] Ursachenzahl korrigiert (acht Nicht-Start-Ursachen, sechs automatisch/zwei manuell)
[x] Planungs-Taskliste bereinigt (Commit/Push/Handover/PR-Aktualisierung fuer Revision 1-4 als erledigt markiert)
[x] ausschliesslich Plan und notwendige Roadmap-/PR-/Handover-Aktualisierung geaendert
[x] Plan Revision 5 committen und pushen (Plan-Commit 286ebacda05a202ea203789421e2398a7a868905)
[x] SESSION HANDOVER fuer Revision 5 auf diesem Plan-Commit aktualisieren
[x] Draft-PR #99 fuer Revision 5 mit Plan-SHA und aktuellem HEAD aktualisieren
[x] Revision 5 nach PR-#99-Review erneut gegen Include-/Typ-, Apply-,
    Clear-/Start-, Safety-, Persistenz-, Re-Arm-, Zeit- und Testbefunde pruefen
[x] Include-/Typ-Schichtung mit sensor_selection_types.hpp und schmaler
    SensorSelectionStateMutation planen
[x] typisierte Apply-Statuswerte, Before-/Revisionspruefung und
    unveraenderliche Ablehnungspfade planen
[x] vollstaendige Clear-/Tombstone-/Start-Invarianten fuer beide bestehenden
    Laufpfade planen
[x] aktionsspezifische Safety-Pending-Matrix und aktiven-only-
    persistSensorSelection-Vertrag planen
[x] AirFallback-Re-Arm ohne Rueckfall nach ProductFailureDetected sowie
    Release-1-Zeitvertrag mit checked Zeitberechnung planen
[x] Architektur-/Compile-Guards und die vollstaendige neue Testmatrix ergaenzen
[x] Planungs-Taskliste fuer Revision 6 mit eigenen Abschlusszeilen aktualisieren
[x] Plan Revision 6 committen und pushen; exakte SHA im PR/Handover eintragen
[x] Draft-PR #99 mit exakter Plan-SHA, aktuellem HEAD, Tests, offenen Gates
    und `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL` aktualisieren
[x] bestehenden SESSION-HANDOVER-Kommentar in-place auf den neuen HEAD
    aktualisieren (kein zweiter Handover-Kommentar)
[x] HALTED_FOR_OWNER_REVIEW
```
