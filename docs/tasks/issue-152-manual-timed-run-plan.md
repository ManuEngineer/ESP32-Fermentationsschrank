# Plan – Issue #152: Owning-Vertrag für den manuellen Zeit-/Temperaturlauf

## Planstatus und harte Basis

Dieser Plan ist ausschließlich ein Plan-only-Artefakt. Er startet keine
Firmware-Implementation und erteilt keine Ready-, Merge- oder
Aktorfreigabe.

```text
ISSUE=152
BASE_SHA=0b8b4cc1673f40296a510fdc0d79440c616ffeb8
DOWNSTREAM_ISSUE=26
IMPLEMENTATION=NOT_STARTED
OWNER_PLAN_APPROVAL_REQUIRED=YES
ACTUATOR_RELEASE=NO
```

Verifiziert am Planbeginn:

```text
CONTEXT_BASELINE_BRANCH=feature/issue-152-manual-timed-run-plan
CONTEXT_BASELINE_SHA=0b8b4cc1673f40296a510fdc0d79440c616ffeb8
CONTEXT_HEAD_SHA=0b8b4cc1673f40296a510fdc0d79440c616ffeb8
CONTEXT_PLAN_SHA=NONE
CONTEXT_REFRESH_MODE=FULL
CONTEXT_DELTA=main baseline, live Issue #152, Issue #144, PR #147, Roadmap, existing contracts
SOURCE_OF_TRUTH_CONFLICT=NONE
```

Issue #144 ist live `CLOSED`; PR #147 ist live `MERGED` mit Source-HEAD
`81bb985146d2ad926dfc156ab1136f8fefe2b3cb` und Merge-/Main-Commit
`0b8b4cc1673f40296a510fdc0d79440c616ffeb8`. Issue #152 ist live `OPEN` und
`PLAN_FIRST`. PR #143 bleibt ein separater Downstream-Scope und wird auf
diesem Branch nicht verändert.

## 1. Ziel, Ergebnis und Grenzen

Issue #152 definiert den kleinsten owning, rendererunabhängigen Vertrag für
den in `REQUIREMENTS.md`, `PRODUCT_VISION.md` und `STATE_MACHINE.md`
verbindlichen manuellen Zeit-/Temperaturlauf. Die fachliche Laufsemantik ist:

- Zieltemperatur und Dauer;
- Air- oder Product-Sensorbetrieb;
- optionales Vorheizen mit bestehender ProductInserted-Grenze;
- dieselbe Zielqualifikation, Regelung, Safety und Persistenz wie bei einem
  zeitgesteuerten Programmlauf;
- eines der bestehenden R1-Abschlussverhalten;
- ein unveränderlicher aktiver Snapshot;
- keine direkte Aktorsteuerung.

Das Ergebnis ist ein kanonischer Application-Eingang, der die fachliche
Absicht bis zum bestehenden `ProgramStart`-/`ActiveRun`-/Orchestrator- und
Persistenzpfad trägt. Issue #26 mappt später nur einen UI-Intent auf diesen
Vertrag. #26 übernimmt weder Allocator-, Codec-, Handoff-, Recovery- noch
Persistence-Ownership von #152 oder #144.

Ausgeschlossen bleiben ausdrücklich:

- Touch-Workspace, Screens, Renderer/LVGL, Display- und Touchhardware;
- Web-API, WLAN und Netzwerkverträge;
- neue Aktorsteuerung, Safety-Regeln, Sensorqualitätslogik oder Recoverypolicy;
- ein dritter Run-Typ, ein zweiter Dispatcher oder ein zweiter
  Persistenzpfad;
- eine allgemeine Programmbibliothek, ein Workflow-/Recipe-/Job-Framework,
  UUID-/Registry-/globaler-ID-Service;
- persistierte temporäre Katalogeinträge;
- Implementation vor Ownerfreigabe dieses exakten Plan-Commits.

## 2. Verifizierte Baseline und direkte Lücke

### 2.1 Kanonische Fachquellen

Der Implementierungsplan verwendet nur die direkt betroffenen Verträge:

- `docs/REQUIREMENTS.md`, Abschnitt Programme und Prozess;
- `docs/PRODUCT_VISION.md`, Abschnitt Manueller Betrieb;
- `docs/STATE_MACHINE.md`, insbesondere STANDBY, PREHEATING,
  WAITING_FOR_PRODUCT, REACHING_TARGET, QUALIFYING_TARGET, FERMENTING,
  COOLING, COOL_HOLDING, COMPLETED, RECOVERY_EVALUATION und die beiden
  manuellen Betriebsarten;
- `docs/RUN_COMMANDS.md` für Decide-/Confirmation-/Apply-/Replay-Semantik,
  den bestehenden `ProgramStartRequest`, `ManualRunPlan` und
  `RunAdjustment`;
- `docs/RUN_PERSISTENCE.md` sowie `docs/RECOVERY_AND_INTERRUPTION.md` für
  Write-before-Apply, Schema-Fail-Closed, Current/Fallback und
  `WaitingForTrustedTime`;
- `docs/DECISIONS.md`, ADR-013 und die lokale
  `lib/fermentation_app/AGENTS.md` für Modulgrenzen;
- der gemergte Vertrag aus
  `docs/tasks/issue-144-run-identity-neutral-provenance-plan.md` und die
  aktuelle Implementierung auf `BASE_SHA`.

### 2.2 Bestehende Start-, Run- und Persistenzverträge

Der Baseline-Abgleich ergibt:

1. `ProcessKind::Timed` und `ProcessRunSnapshot` tragen bereits Dauer,
   Vorheizen, Product-Wartezeit, Zielqualifikation, Zielerreichungsgrenze,
   CompletionMode und Cooling-Haltezeit.
2. `ActiveRun` besitzt den unveränderlichen `RunProgramSnapshot`, die
   effektiven Zielwerte und die bestehende append-only-`RunRevision`-Logik.
   Der vorhandene `RunAdjustment`-Pfad ist auf `activeProgramRun`
   ausgerichtet.
3. `decideProgramStart()` erzeugt aus einem bereits vertrauenswürdig
   aufgelösten Startkandidaten einen `Timed`-Prozess, prüft State-, Lauf-,
   Safety- und Sensorbedingungen und bleibt bis zur Bestätigung
   `DecisionOnly`.
4. `TemperatureControlApplicationOrchestrator` und
   `RunPersistenceCoordinator::persistCommand()` besitzen den einzigen
   Write-before-Apply-Handoff. Ein Fresh Start wird dort vor Candidate-Apply
   ohne trusted UTC mit `TrustedAbsoluteTimeRequired` blockiert.
5. `RunPersistenceSnapshot::ProgramRun`, die Existing-Program-Recovery und
   die `ProgramRun`-Recoverymatrix tragen alle zeitgesteuerten Phasen bereits.
6. `RunCheckpointVariant::ManualRun` und `ManualRunPlan` sind dagegen heute
   ausdrücklich auf `ProcessKind::ManualHolding` festgelegt. Der Plan enthält
   keine Fermentationsdauer und kein Completion-Verhalten. Die Persistenz-
   und Recoveryvalidierung setzt für `ManualRun` denselben Zusammenhang
   voraus.
7. `RunCommandState::activeManualRun`, `control_context.cpp`, die
   Sensorselektions-Projektion und die Recovery-Hilfen unterscheiden heute
   zwischen Programmlauf und ManualHolding. `decideRunAdjustment()` akzeptiert
   heute nur `activeProgramRun`.
8. Der gemergte #144-Vertrag stellt `ApplicationRunIdentity` bereit:
   `CommandId` wird an der Application-Grenze vergeben, `runId` wird aus
   `StorageEpoch + StartCommandId` erzeugt, und der committed
   `commandIdHighWater` bleibt Eigentum des bestehenden Run-Coordinators.
9. Der aktuelle Run-Persistenzstand ist Schema 4. Schemas 1–4 sind bekannt;
   Schema 4 trägt die 64-Bit-neutrale `RunProgramSourceRevision` und den
   HWM-Vertrag. Die bestehende ProgramRun-Quelle kennt nur
   `FactoryCatalog` und `UserProgram` und verlangt eine nicht-null
   `sourceProgramRevision`.

Damit ist die fachliche Lücke nicht eine neue Regelmaschine, sondern die
fehlende, wahrheitsgemäße Repräsentation eines temporären Timed-Run-Quelltyps
an der vorhandenen ProgramRun-Grenze.

## 3. Vergleich der zwei zulässigen Wiederverwendungsrichtungen

### Variante A – temporärer ProgramRun über den bestehenden ProgramStart-Pfad

```text
manuelle Zeit-/Temperaturabsicht
    -> FermentationApplication
    -> typisierter temporärer ManualTimed-Run-Source
    -> bestehender ProgramStartRequest / decideProgramStart()
    -> ActiveRun + ProcessKind::Timed
    -> bestehender Orchestrator / RunPersistenceCoordinator
```

Die Variante behält `RunCheckpointVariant::ProgramRun`,
`activeProgramRun`, die bestehende Timed-FSM, `ProcessRunSnapshot`,
RunAdjustment, Completion und die vorhandene ProgramRun-Recovery. Sie benötigt
eine kleine diskriminierte Quellrepräsentation für den temporären Inhalt und
wegen der neuen Wire-Bedeutung ein neues Run-Schema. Sie verwendet weder eine
Katalogrevision noch einen Fake-Katalogeintrag.

### Variante B – ManualRunPlan auf einen zeitbegrenzten Lauf erweitern

```text
manuelle Zeit-/Temperaturabsicht
    -> FermentationApplication
    -> erweiterter ManualRunPlan
    -> bestehender Manual-Start-/Orchestratorpfad
    -> ProcessKind::Timed
```

Diese Variante würde `ManualRunPlan` von `ManualHolding` auf zwei
semantische Prozessarten erweitern. Dafür müssten `activeManualRun`,
`stateMatchesRunSnapshot`, `validStateFor`, die Manual-Codecfelder, die
Recoverymatrix und die Control-Context-Projektion gleichzeitig für
`FERMENTING`, `COOLING`, `COOL_HOLDING` und Completion erweitert werden. Der
vorhandene `RunAdjustment`-Pfad akzeptiert nur `ActiveRun`; entweder müsste
die bestehende Adjustment-Ownership dupliziert oder der ManualPlan in ein
zweites, paralleles Revisionsmodell umgebaut werden. Das ist größer und
weniger DRY als die vorhandene ProgramRun-Ownership zu verwenden.

### Auswahl

Gewählt wird **Variante A**. Sie hat über Commandmodell, Snapshot, Application,
Persistence, Recovery und Tests den kleineren korrekten Gesamtumfang, weil der
manuelle Zeitlauf fachlich bereits als temporäres Programm definiert ist und
alle Timed-Semantiken im bestehenden ProgramRun liegen. Variante B wird nicht
teilweise vorbereitet.

## 4. Gewählter Quell-, Snapshot- und Application-Vertrag

### 4.1 Wahrheitsgemäße `ManualTimed`-Quelle

`ProgramSourceKind` erhält die explizite Ausprägung `ManualTimed`. Sie ist
keine dritte `ProcessKind`-Ausprägung und kein dritter Persistenzvariantentyp;
der aktive Lauf bleibt ein `ProgramRun` mit `ProcessKind::Timed`.

Der bestehende `RunProgramSnapshot` wird zu einem diskriminierten Source-
Snapshot erweitert:

- `ProgramDocument` bleibt unverändert die Quelle für `FactoryCatalog` und
  `UserProgram`;
- ein neuer schmaler `ManualTimedRunSource` trägt nur die immutable, für den
  konkreten temporären Run notwendigen Werte:
  - Zieltemperatur und Dauer;
  - Vorheizen und optionale maximale Product-Wartezeit;
  - Zielband, Qualifikationsdauer und maximale Zielerreichungszeit;
  - `CompletionMode`, Cooling-Ziel und optionale Cooling-Haltezeit;
  - keinen frei konfigurierbaren Sensor-Policywert; der bestehende feste
    manuelle Sensorselektionskontext wird source-aware abgeleitet;
- `RunProgramSnapshot` trägt genau eine dieser Quellen und prüft die
  Übereinstimmung zwischen Source-Variante und `ProgramSourceKind`.

Die Source enthält keine Programm-ID, keinen Programmnamen als Identität,
keinen Katalogeintrag und keine `ProgramCatalogRevision`. Ein stabiler Text
für eine Startzusammenfassung ist reine Darstellung und keine Quellidentität.
Der aktive Run-Snapshot ist die einzige dauerhafte Kopie der eingegebenen
manuellen Werte; der `ProgramCatalog` wird nicht verändert.

Die geplante Shape ist bewusst explizit und klein:

```cpp
struct ManualTimedRunSource {
    FermentationStage stage;          // target + duration, no program id
    bool preheatEnabled;
    std::optional<std::uint32_t> maximumProductWaitMinutes;
    TargetQualification targetQualification;
    std::optional<std::uint32_t> maximumTargetReachMinutes;
    ProgramCompletion completion;
    // Manual sensor policy is derived, not a user/configuration field:
    // ProductIfAvailableElseAir, WaitForUser, ManualReturnToProduct,
    // no fallback delay.
};

using ProgramRunSource =
    std::variant<ProgramDocument, ManualTimedRunSource>;

struct RunProgramSnapshot {
    ProgramRunSource source;
    ProgramSourceKind sourceKind;
    std::optional<RunProgramSourceRevision> sourceProgramRevision;
};
```

Die konkrete C++-Anordnung folgt den vorhandenen Include- und Strong-ID-
Grenzen. `ManualTimedRunSource` ist ein konkreter Run-Source-Wert, keine neue
allgemeine Programmbibliothek. Für `FactoryCatalog` und `UserProgram` gilt
weiterhin: ProgramDocument und nicht-null `RunProgramSourceRevision`. Für
`ManualTimed` gilt: ManualTimedRunSource und `nullopt`. Jede andere
Kombination wird vor Start, Restore und Encode abgelehnt. Der
`ManualTimedRunSource` enthält keine `runId`; diese bleibt ausschließlich im
umgebenden Run-/Command-Vertrag.

Der source-aware Sensor-Kontext-Accessor liefert für `ManualTimed` stets den
kanonischen festen manuellen Vertrag
`ProductIfAvailableElseAir` / `WaitForUser` / `ManualReturnToProduct` ohne
Fallback-Delay. Diese Werte sind keine neuen Benutzerparameter, keine zweite
Policy und keine zusätzliche persistierte Quelle. Der Accessor wird sowohl
für die Startauflösung als auch für die späteren manuellen Recheck-/Continue-
Entscheidungen verwendet.

Die vorhandene `ActiveRun`-Revision- und Adjustment-Logik wird durch schmale
Source-Accessors wiederverwendet: initiale Ziel-/Dauerwerte, Qualifikation,
Completion/Cooling und Sensorselektionskontext werden abhängig vom
Source-Kind aus derselben Snapshot-Quelle gelesen. Es wird kein zweites
Revisionsmodell gebaut.

### 4.2 Application-Eingang

Die bestehende Application-Grenze erhält eine rendererunabhängige
`ManualTimedRunValues`-Absicht. Der Aufrufer liefert ausschließlich:

- Zieltemperatur;
- Dauer;
- `RunSensorMode`;
- Vorheizen und, falls Vorheizen aktiv ist, die bereits etablierte maximale
  Product-Wartezeit;
- die bestehenden Qualifikations- und Zielerreichungswerte, soweit der
  vorhandene Fachvertrag sie für einen Start verlangt;
- bestehendes Completion-Verhalten einschließlich Cooling-Ziel und
  Cooling-Haltezeit.

Der Aufrufer liefert ausdrücklich nicht:

- `ProgramDocument`, Programm-ID, Katalogeintrag oder
  `ProgramCatalogRevision`;
- `CommandId`, `runId` oder einen Ersatz dafür;
- Safety-, Sensor-, Planner-, Persistenz- oder Recovery-Evidenz.

`FermentationApplication::prepareStartManualTimed()` validiert die
fachlichen Werte über den neuen schmalen Source-Validator, mintet danach
über den bestehenden `ApplicationRunIdentity` genau eine Application-owned
Command-ID und erzeugt den `runId` ausschließlich aus
`StorageEpoch + StartCommandId`. Die bestehende `FermentationUiCommandContext`
bleibt die erwartete State-/Run-/Message-/Fault-/Recovery-Revisionsquelle;
neue ManualTimed-Payloads duplizieren keine Expected-Revisionsfelder.

Das vorbereitete Ergebnis verwendet den bestehenden
`ProgramStartRequest`-Vertrag mit einer `ManualTimed`-Source und dem
bestehenden `CommandKind::StartProgram`. Dadurch bleiben
`persistFreshStartCommand()` und die Fresh-Start-UTC-Schranke unverändert
semantisch zuständig. Es gibt keinen neuen Startservice und keine zweite
Confirmation-Engine. #26 ergänzt später nur seine UI-Abbildung auf diese
Application-Methode.

Dafür wird `ProgramStartRequest::program` nicht durch ein zweites paralleles
Programmfeld ergänzt, sondern auf denselben `ProgramRunSource`-Discriminator
umgestellt. Der SourceKind-/Revision-Abgleich ist eine einzige Invariante;
ein normaler ProgramStart und ein ManualTimed-Start gelangen danach durch
dieselbe `decideProgramStart()`-Funktion.

### 4.3 Sensor- und Safety-Semantik

Für den ManualTimed-Source wird die bestehende manuelle Sensorsemantik
wiederverwendet, nicht eine neue Policy erfunden:

- Air- und Cooling-Sensor bleiben für jeden Start Pflicht;
- die angeforderte Air-/Product-Rolle wird nicht aus UI-Werten als Safety-
  Evidenz abgeleitet;
- gespeicherte Factory-/User-Programme behalten unverändert die bestehende
  `resolveProgramStartSensorMode()`-Semantik;
- `ManualTimed` erhält innerhalb derselben bestehenden
  `decideProgramStart()`-Auflösung einen source-aware Zweig: angefordert
  `Air` wird effektiv `Air`, angefordert `Product` wird effektiv `Product`;
- für `ManualTimed` wird bei angefordertem `Product` niemals die generische
  `ProductIfAvailableElseAir`-Substitution verwendet und der Start wird nicht
  wegen der Programmpraeferenz vorab abgelehnt. Stattdessen wird die bereits
  bestehende `startSensorSelectionOutcome()`-Logik mit
  `substitutedFromProduct = false` aufgerufen. Bei aktuell ungültigem Product
  entsteht damit die kanonische `UserDecisionRequired`-/`Blocked`-Semantik,
  ohne Product-zu-Air-Fallback und ohne Aktorfreigabe;
- der feste source-aware Kontext (`ProductIfAvailableElseAir`, `WaitForUser`,
  `ManualReturnToProduct`, kein Fallback-Delay) wird über den gemeinsamen
  Accessor an die automatische und manuelle Recheck-/Continue-Logik gegeben;
- frische Safety-/Sensor-/Planner-Evidenz kommt weiterhin ausschließlich von
  der owning Application-/Orchestrator-Grenze.

### 4.4 Source-aware StartSummary

Der bestehende Startentscheid muss auch für `ManualTimed` eine wahrheitsgemäße
Zusammenfassung vor der Bestätigung liefern. Der bisher zwingende
`StartSummary::programName`-String wird deshalb source-aware und darf keinen
leeren Sentinel oder Fake-Namen verwenden:

```cpp
struct StartSummary {
    std::string runId;
    ProgramSourceKind sourceKind;
    std::optional<std::string> programName;
    // existing target, duration, sensor, preheat, completion and kind fields
};
```

Für `FactoryCatalog` und `UserProgram` enthält `programName` weiterhin den
echten Namen aus dem `ProgramDocument`. Für `ManualTimed` gilt
`sourceKind == ProgramSourceKind::ManualTimed` und
`programName == std::nullopt`; die spätere UI-/Web-Darstellung verwendet den
bereits vorhandenen lokalisierten TextKey für „manueller
Zeit-/Temperaturlauf“. Der sichtbare Text wird nicht in der Fachlogik
hartcodiert und nicht persistiert. Die Summary ist bereits bei
`DecisionOnly` vor der Bestätigung vorhanden; Confirmation-Replay bewahrt
SourceKind, optionalen Namen und die vorbereitete Identität unverändert.

### 4.5 Decision-/Apply-Grenze und Identität

Der unveränderte Handoff bleibt:

```text
semantic ManualTimed values
    -> Application prepares canonical ProgramStartRequest
       (one CommandId, derived runId, owning evidence)
    -> decideProgramStart() = DecisionOnly until confirmed
    -> confirmation reuses the same prepared envelope and runId
    -> TemperatureControlApplicationOrchestrator
    -> RunPersistenceCoordinator write-before-Apply
    -> OwningOutcome / committed FSM handoff
```

Verbindliche Eigenschaften:

- ein gültiger neuer Request erhält genau eine Command-ID;
- eine Confirmation-Replay-Operation erzeugt keine neue ID und keinen neuen
  Run;
- eine bereits verarbeitete ID bleibt im bestehenden Replayfenster
  `AlreadyProcessed`;
- Persistenzfehler vor Apply verändern weder aktiven RAM-/FSM-Zustand noch
  geben sie Aktoren frei;
- UI/Web greifen nicht direkt auf den `RunPersistenceCoordinator` zu;
- HWM-, Replay- und Epoch-Handoff-Ownership aus #144 bleibt unverändert.

## 5. Fachliche Start- und Laufsemantik

Die Umsetzung verwendet die vorhandene FSM und den vorhandenen
`TemperatureControlApplicationOrchestrator`:

1. Start ist nur aus kanonischem `STANDBY` zulässig, mit gültigen
   Revisions-, Safety- und Sensorbedingungen.
2. Ein Fresh Start wird durch den bestehenden Persistenzpfad ohne trusted UTC
   vor Candidate-Apply blockiert. Die Application behauptet keine eigene
   Zeitfreigabe.
3. Mit Vorheizen bleibt der bestehende Weg
   `PREHEATING -> WAITING_FOR_PRODUCT -> REACHING_TARGET` erhalten.
   `ProductInserted` bleibt ein Prozessereignis; die Rollen-/Evidenzmutation
   erfolgt nur über die bestehende committed Sensorselektionsgrenze.
4. Ohne Vorheizen startet der vorhandene Timed-Pfad direkt in
   `REACHING_TARGET`.
5. Zielqualifikation und Sensorqualität benutzen die bestehenden Evaluatoren
   und Sensorverträge. Die Fermentationsdauer beginnt erst nach erfolgreicher
   Qualifikation in `FERMENTING`.
6. `CompletionMode` benutzt ausschließlich den bestehenden Vertrag:
   `FinishWithoutCooling -> COMPLETED`, die drei Cooling-Modi über
   `COOLING` und gegebenenfalls `COOL_HOLDING`.
7. Der aktive `RunProgramSnapshot` bleibt unveränderlich. Zieltemperatur und
   verbleibende Dauer ändern sich nur über den vorhandenen validierten
   `RunAdjustment`-Pfad, inklusive `RunRevision`, Requalifikationswirkung und
   Write-before-Apply.
8. Stop, Abort, Completion und Cooling verwenden die vorhandenen
   Command-/FSM-/Orchestratorpfade. Ein `AbortAndCool`-Ersatzlauf bleibt der
   bestehende manuelle Cooling-Plan; #152 macht daraus keinen zweiten
   Timed-/Cooling-Dispatcher.
9. Es wird keine neue FSM, keine zweite Temperaturregelung und keine direkte
   Aktoraktion eingeführt.

## 6. Provenienz und Wire-/Schemaentscheidung

### 6.1 #144 bleibt SSOT für Identität

Für ManualTimed gilt exakt der gemergte #144-Identitätsvertrag:

- `CommandId` wird nur von `ApplicationRunIdentity` vergeben;
- `runId` wird nur aus `StorageEpoch + StartCommandId` erzeugt;
- UI/Web liefern weder `CommandId` noch `runId`;
- `commandIdHighWater`, Replayfenster, Epoch-Bindung und Recovery-Handoff
  bleiben Eigentum der vorhandenen Application-/Run-Persistence-Owner.

`RunProgramSourceRevision` bleibt die neutrale Katalog-/Quellstandsprovenienz
für tatsächlich aus `FactoryCatalog` oder `UserProgram` aufgelöste Quellen.
Für `ProgramSourceKind::ManualTimed` ist die Revision **nicht vorhanden**.
Sie wird weder aus der aktuellen `ProgramCatalogRevision` kopiert noch aus
der Command-ID, dem `runId`, einem Hash oder einer erfundenen Zahl abgeleitet.
Damit wird keine Katalogherkunft behauptet, die den manuellen Inhalt nicht
erzeugt hat.

### 6.2 Bewusster Schemawechsel

Die neue Source-Kind-Bedeutung und die optionale Quellrevision sind eine neue
Wire-Semantik. Sie werden nicht still in Schema 4 gepresst. Die Umsetzung
hebt den bestehenden Run-Vertrag auf Schema 5:

- `kCurrentRunPersistenceSchema = 5`;
- `knownRunPersistenceSchema()` akzeptiert 1, 2, 3, 4 und 5;
- neue Head-, Current-, Checkpoint- und Fallback-Writes verwenden Schema 5;
- Schema 1–4 bleiben lesbar; ihr historischer ProgramRun-Layout wird mit dem
  bestehenden Decoderpfad gelesen und erhält eine vorhandene, nicht-null
  `RunProgramSourceRevision` als neutralen Legacywert;
- Schema 5 schreibt eine explizite Source-Discriminierung. Stored-Program-
  Quellen tragen eine nicht-null `RunProgramSourceRevision` und weiterhin den
  gemeinsamen ProgramDocument-Codec. ManualTimed trägt die explizite
  `ManualTimedRunSource` und ein absent-Revision-Tag;
- `RunCheckpointVariant::ProgramRun`, `RunCheckpointVariant::ManualRun`,
  `RunPersistenceHead::commandIdHighWater`, `StorageEpoch`, HWM-Monotonie,
  Slot-/Head-/CRC-/Length-/Trailing-Byte-Validierung und
  Write-before-Apply bleiben unverändert in ihrer Ownership;
- Schema 4 und ältere ManualHolding-Wirewerte werden nicht nachträglich als
  ManualTimed interpretiert. Alte Records benötigen keine Migration;
- unbekannte neuere Schemas, unbekannte Source-Kinds, fehlende oder
  widersprüchliche Sourcefelder und untrusted Records bleiben fail-closed;
- der bestehende Payload-/Recordgrößenvertrag wird erneut geprüft. Falls die
  konkrete ManualTimed-Source die bestehende Obergrenze überschreiten würde,
  ist das ein Planblocker; es gibt keine ungeprüfte Budget- oder
  Speicherreserve.

Der #26-Plan muss nach einem späteren Merge von #152 seinen angenommenen
aktuellen Write-Schema- und Source-Vertrag auf Schema 5 synchronisieren. Das
ist ein legitimes Downstream-Delta, kein Grund, #152 in eine falsche Schema-4
Semantik zu zwingen.

## 7. Recovery- und Persistenzmatrix

ManualTimed ist ein `ProgramRun` mit `ProcessKind::Timed`. Daher verwendet es
die vorhandenen R1-Klassifikationen, nicht eine neue Recoverypolicy:

| Persistierte Phase | bestehende R1-Behandlung für den ManualTimed-ProgramRun |
|---|---|
| `PREHEATING` | technisch vertrauenswürdiges Resume-Angebot; keine automatische Aktorfreigabe; bewusste Bestätigung, frische Evidenz und bestehender Write-before-Apply-Pfad |
| `WAITING_FOR_PRODUCT` | alte Product-Wartezeit wird nach Neustart nicht blind fortgesetzt; bestehender `NoActiveRun`-/Abbruchpfad, niemals Produkt als eingesetzt annehmen |
| `REACHING_TARGET` | keine automatische R1-Fortsetzung; bestehender `NoActiveRun`-Pfad für die nicht rekonstruierbare Zielerreichungsphase |
| `QUALIFYING_TARGET` | keine Wiederaufnahme angefangener Qualifikation; bestehender `NoActiveRun`-Pfad ohne Qualifier-Kredit |
| `FERMENTING` | vollständiger Current-Fall über den bestehenden #124-Exact-Time-Recoverykern; mit trusted UTC logische Fortsetzung oder normale Completion-Semantik, ohne trusted UTC `RecoveryEvaluation/WaitingForTrustedTime` |
| `COOLING` | Resume-Angebot gemäß bestehender Cooling-Regel; keine automatische Fortsetzung und keine Aktorfreigabe aus dem Record allein |
| `COOL_HOLDING` | bestehender `NoActiveRun`-Pfad ohne automatische Aktivierung; keine erfundene Hold-Zeit oder Cooling-Zielerreichung |
| `COMPLETED` | bestehendes Completed-Restore; kein Neustart und kein stiller Wechsel nach Standby |
| `RecoveryEvaluation` | bestehende Recovery-Evidenz-/UTC-Entscheidung; keine neue ManualTimed-Policy |
| untrusted, unbekanntes oder widersprüchliches Persistenzrecord | `SAFE_BOOT` beziehungsweise bestehender fail-closed Load-Status; keine Tombstone-Umetikettierung und keine Aktorfreigabe |

Sensorselektions-Revalidation nach Boot bleibt ebenfalls unverändert: die
persistierte Source liefert den bestehenden manuellen Sensorselektionskontext,
frische Live-Evidenz entscheidet über Permission, und kein vorheriger
Peltier-Zustand wird wiederhergestellt.

## 8. Konkreter Implementierungs- und Commit-Schnitt nach Freigabe

Die Umsetzung erfolgt erst nach Ownerfreigabe dieses Plan-Commits und in
kleinen, gezielt testbaren Schnitten:

1. **Source-/Snapshot-Vertrag:** `ManualTimedRunSource`,
   `ProgramSourceKind::ManualTimed`, optionales
   `RunProgramSourceRevision`, Source-Invarianten, Accessors und die
   `ActiveRun`-Initial-/Restore-/Adjustment-Projektion.
2. **Application-Handoff:** rendererunabhängige ManualTimed-Werte,
   `prepareStartManualTimed()`, Source-Building, app-owned Identity und
   Prepared-Confirmation; keine UI-/Touch-Typen.
3. **Bestehende Konsumenten:** `decideProgramStart`, Process-Snapshot,
   Control-Context, Sensorselektions-Projektion und Recovery-Sourcezugriff
   auf den neuen Source-Accessor umstellen. Keine zweite Regelimplementierung.
4. **Schema-/Codecvertrag:** Schema 5, Source-Discriminator,
   ManualTimed-Source-Codec, Legacy-Leser, Unknown-Newer-Fail-Closed und die
   bestehenden Head-/HWM-/Epoch-Invarianten.
5. **Gezielte Tests und normative Dokumentation:** Akzeptanzmatrix aus
   Abschnitt 9, bestehende Testsuiten erweitern, danach PR-/Roadmap-/Issue-
   Provenienz synchronisieren. Kein vollständiger Pre-Ready-Lauf im Draft.

## 9. Test- und Akzeptanzmatrix

Die Tests erweitern die bestehenden nativen Suites; es wird kein neues
Testframework und kein Test-Dispatcher eingeführt.

### Command/Application/Run

- gültiger Air-geführter ManualTimed-Start;
- gültiger Product-geführter Start mit erforderlicher Produktevidenz;
- ManualTimed + Air startet über den bestehenden ProgramStart-Pfad;
- ManualTimed + Product mit gültiger Produktevidenz bleibt Product;
- ManualTimed + Product mit ungültigem Product erzeugt
  `UserDecisionRequired`/`Blocked`, ohne Product-zu-Air-Fallback und ohne
  Aktorfreigabe;
- spätere Recheck-, `ContinueWithAir`- und `ReturnToProduct`-Aktionen folgen
  der bestehenden manuellen #21-Semantik;
- ungültige Zieltemperatur, 0/ungültige Dauer, ungültige Qualifikations- oder
  Coolingwerte;
- fehlende trusted UTC blockiert vor Candidate-Apply und vor RAM-/FSM-Mutation;
- fehlende Air-/Cooling-/Safety-Evidenz bleibt fail-closed;
- Vorheizen und `ProductInserted` verwenden den bestehenden Übergang;
- Timerstart erst nach erfolgreicher Zielqualifikation;
- jedes der vier bestehenden R1-Abschlussverhalten;
- immutable aktiver Snapshot; spätere Catalogänderung verändert ihn nicht;
- `RunAdjustment` für Zieltemperatur und verbleibende Dauer über den
  vorhandenen `ActiveRun`-Pfad;
- Stop, Abort, Completion und Cooling über bestehende Pfade;
- genau eine Command-ID und `runId = StorageEpoch + StartCommandId`;
- Confirmation-Replay mit derselben vorbereiteten Identität;
- Duplicate ohne zweite Mutation oder zweite Nebenwirkung;
- UI-/Anwendungswerte können keine IDs oder Evidence einschleusen.
- Stored ProgramStart behält `sourceKind` und den echten Programmnamen;
- ManualTimed-Startsummary enthält `sourceKind=ManualTimed` und keinen
  erfundenen Programmnamen;
- die Summary bleibt vor der Bestätigung verfügbar und Confirmation-Replay
  verändert weder Summary-Quelle noch Identität.

### Persistenz/Codec/Recovery

- Write-before-Apply bei Fehler vor Slot-/Head-Commit;
- Schema-5-ManualTimed-Roundtrip ohne Katalogrevision;
- Schema-4- und Legacy-ProgramRun-Lesbarkeit mit neutraler Revision;
- Schema-1/2/3/4-Reads und neuer Schema-5-Writepfad;
- unknown newer schema, unknown source kind, fehlende Revision für Stored
  Program oder falsche Revision für ManualTimed fail-closed;
- HWM, StorageEpoch, Replayfenster und Command-ID-Commit bleiben nach
  ProgramRun- und ManualTimed-Transitions monoton und unverändert;
- Recoverymatrix für PREHEATING, WAITING_FOR_PRODUCT, REACHING_TARGET,
  QUALIFYING_TARGET, FERMENTING, COOLING, COOL_HOLDING, COMPLETED und
  RecoveryEvaluation;
- Current-FERMENTING mit trusted UTC, ohne trusted UTC (`WaitingForTrustedTime`)
  sowie untrusted Persistenz;
- frische Sensor-/Safety-Revalidation nach Recovery;
- kein gespeicherter ProgramCatalogeintrag und keine Mutation des
  `ProgramCatalog` durch einen ManualTimed-Start;
- Payload-/Recordgrenzen, CRC, Trailing Bytes und bestehende Head-/Slot-
  Konsistenz bleiben geprüft.

### Geplante bestehende Testdateien

- `test/test_run_commands/test_run_commands.cpp`;
- `test/test_run_snapshots/test_run_snapshots.cpp`;
- `test/test_process_state_machine/test_process_state_machine.cpp`;
- `test/test_control_context/test_control_context.cpp`;
- `test/test_run_checkpoint_codec/test_run_checkpoint_codec.cpp`;
- `test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp`;
- `test/test_boot_classification/test_boot_classification.cpp`;
- `test/test_issue144_run_identity/test_issue144_run_identity.cpp`;
- `test/test_fermentation_ui_commands/test_fermentation_ui_commands.cpp`,
  ausschließlich für die bestehende Application-Prepared-/Confirmation-
  Regression und ohne neue #26-UI-Typen.

Gezielte native Tests werden erst nach Planfreigabe ausgeführt. Im aktuellen
Planauftrag sind Firmwaretests, Build, ESP-IDF und Hardwaretests `NOT_RUN`.

## 10. Tatsächlicher Dateiscope nach Freigabe

Der erwartete minimale Produktionsscope ist:

- `lib/fermentation_app/src/run_snapshot.hpp/.cpp` – Source-Variant,
  Provenienz-Optionalität, Validierung, ActiveRun-Projektion und
  RunAdjustment-Accessors;
- `lib/fermentation_app/src/run_commands.hpp/.cpp` – canonicaler
  `ProgramStartRequest`, ManualTimed-Application-Handoff im vorhandenen
  Startentscheid, Sensor-/Startzusammenfassung und keine neue Command-Engine;
- `lib/fermentation_app/src/fermentation_application.hpp/.cpp` – schmaler
  rendererunabhängiger Application-Eingang und vorhandener Identity-Handoff;
- `lib/fermentation_app/src/process_state_machine.cpp` – Timed-
  `ProcessRunSnapshot` aus beiden bestehenden Run-Sourceformen;
- `lib/fermentation_app/src/control_context.cpp` – bestehende Timed-Control-
  und Qualification-Projektion über den Source-Accessor;
- `lib/fermentation_app/src/run_persistence_contract.hpp/.cpp` – Schema-5-
  Konstante, bekannte Schemas und Source-/Snapshot-Invarianten;
- `lib/fermentation_app/src/run_persistence_codec.cpp` – versionierter
  ManualTimed-Source-Codec und Legacy-/Unknown-Schema-Grenze;
- `lib/fermentation_app/src/run_persistence_coordinator.cpp` – nur die
  bestehenden Recovery-/Sensor-Sourcezugriffe auf den neuen Accessor, falls
  der Baseline-Abgleich dies wie erwartet erfordert. Keine neue Coordinator-
  Ownership;
- die in Abschnitt 9 genannten bestehenden gezielten Tests;
- nur die direkt betroffenen normativen Vertragsstellen in
  `docs/RUN_COMMANDS.md`, `docs/RUN_PERSISTENCE.md` und
  `docs/STATE_MACHINE.md`.

Nicht Teil des Dateiscopes sind `application_run_identity.*`,
`temperature_control_orchestrator.*`, `configuration_service.*`,
`configuration_graph_*`, `program_document_codec.*`, UI-/Touch-/Renderer-
Dateien und Hardwareadapter. Sie bleiben bestehende Owner bzw. reine
Regressionstests. Sollte der Detailabgleich nach Planfreigabe einen
zusätzlichen materiellen Vertragsscope erfordern, wird vor Implementation
angehalten und dieser Plan neu freigegeben.

## 11. Owner-Gates und Abschluss

Vor Implementation müssen alle folgenden Bedingungen erfüllt sein:

- unabhängiger vollständiger Review dieses exakten Plan-Commits;
- `OPEN_BLOCKERS=0` und ausdrückliche Ownerfreigabe der exakten Plan-SHA;
- erneute Prüfung von Branch, Base, HEAD, Issue #152, PR-Status und Roadmap;
- danach Umsetzung in kleinen Slices mit gezielten Tests;
- Builder-Self-Check, unabhängiger Implementation-Review und die normalen
  Owner-Gates für Pre-Ready, Ready, CI und Merge.

Der Abschluss dieses Auftrags besteht nur aus dem Plan-/Provenienzcommit und
der Synchronisierung von Issue #152, Draft-PR, Roadmap und genau einem
aktuellen `SESSION HANDOVER`. Danach hält der Builder an.

```text
ISSUE=152
BASE_SHA=0b8b4cc1673f40296a510fdc0d79440c616ffeb8
IMPLEMENTATION=NOT_STARTED
OWNER_PLAN_APPROVAL_REQUIRED=YES
```
