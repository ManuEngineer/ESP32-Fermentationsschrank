# Issue #144 – Run-Identity und neutrale Run-Provenienz

## 1. Planstatus und Baseline

Dies ist der vollstaendige, eigenstaendig ausfuehrbare Plan fuer Issue #144.
Er beschreibt den kleinen vorgelagerten Application-/Run-Identity- und
Run-Provenienzscope. Er implementiert weder die lokale Touch-Shell noch den
Fermentations-Workspace aus Issue #26.

    ISSUE=144
    TITLE=[E4.2] Run-Identity und neutrale Run-Provenienz vor Issue #26
    PR=147_DRAFT
    BRANCH=feature/issue-144-run-identity-provenance
    WORKFLOW=PLAN_FIRST_SINGLE_PR
    BASE_BRANCH=main
    BASE_SHA=e84dfa8abf220220a33e6e21b95dbd0d7bd9ac90
    ROADMAP_COMMIT=3ecf9ad9edc223c7af731600d54a857d5e2f8c9f
    PLAN_PATH=docs/tasks/issue-144-run-identity-neutral-provenance-plan.md
    PLAN_REVISION=BLOCKER_CORRECTION_3
    SUPERSEDES_PLAN_COMMIT=d014a33f40ee1968bf28811df878b39931ba75b2
    PLAN_COMMIT=THIS_COMMIT
    IMPLEMENTATION=NOT_STARTED
    NATIVE_TESTS=NOT_RUN_PLANNING_ONLY
    ESP_IDF_BUILD=NOT_RUN
    HARDWARE_TEST=NOT_RUN
    OWNER_PLAN_APPROVAL_REQUIRED=YES
    ACTUATOR_RELEASE=NO
    BLOCKS_ISSUE26=YES
    DOWNSTREAM_ISSUE=26
    DOWNSTREAM_PR=143
    DOWNSTREAM_APPROVED_PLAN=aea6acb2e51147c6452d728a5a45840236ab1fdf

    CONTEXT_BASELINE_BRANCH=main
    CONTEXT_BASELINE_SHA=e84dfa8abf220220a33e6e21b95dbd0d7bd9ac90
    CONTEXT_HEAD_SHA=d014a33f40ee1968bf28811df878b39931ba75b2
    CONTEXT_PLAN_SHA=d014a33f40ee1968bf28811df878b39931ba75b2
    CONTEXT_REFRESH_MODE=INCREMENTAL
    CONTEXT_DELTA=Planrevision 3: Bootstrap-Schema-2-Sequence, Successor-, Wire- und Zwei-Slot-Crashvertrag sowie Dateiscope
    SOURCE_OF_TRUTH_CONFLICT=NONE

Die aktuelle PR-Basis ist `main@e84dfa8abf220220a33e6e21b95dbd0d7bd9ac90`.
PR #149 / Issue #148 hat die bereits reviewten PRs #142 und #146 von der
frueheren Integrationsbasis unveraendert nach `main` uebernommen und `main`
als normale Entwicklungsbasis wiederhergestellt. PR #147 ist auf diese
kanonische `main`-Basis retargetet; fuer diese Dokumentationskorrektur wird
keine Ancestry durch Rebase oder zusaetzlichen Mergecommit veraendert.

Die Roadmap wurde in diesem PR als erster Commit auf Issue #144 als aktuellen
fachlichen Pflichtvorgaenger und auf die Blockierung von #26 synchronisiert.
Dieser Korrekturstand praezisiert den bereits persistierten Handoff um den
vollstaendigen Schema-2-Sequence-, Successor-, Wire- und Zwei-Slot-Vertrag;
die Roadmap-Reihenfolge bleibt unveraendert.

Vor dieser Planerstellung wurden ausserdem live verifiziert:

- Issue #144 ist offen und traegt den Run-Identity-/Provenienzscope;
- PR #147 ist offen und Draft auf `main`;
- PR #143 ist weiterhin offen und Draft, mit dem freigegebenen #26-Plan auf
  `aea6acb2e51147c6452d728a5a45840236ab1fdf`;
- PR #143 wird weder gemergt noch geschlossen, und seine #26-Implementation
  beginnt nicht in diesem Scope;
- PR #142 / Issue #25 und PR #146 / Issue #145 sind in der Baseline
  abgeschlossen.

## 2. Ziel und Abgrenzung

Issue #144 liefert die von #26 konsumierte Application-Grenze fuer:

1. eine semantisch neutrale, starke 64-Bit-Run-Provenienz;
2. abwaertskompatible Run-Persistenz ab Schema 4;
3. einen gemeinsamen app-owned Command-ID-Allocator fuer Touch und spaeter
   Web;
4. app-erzeugte Lauf-IDs fuer jeden Pfad, der einen neuen aktiven Lauf
   erzeugt.

Nach erfolgreichem Merge garantiert #144 fuer #26 die folgenden Invarianten:

- ein neues Programm nimmt seine Run-Quellprovenienz exakt aus der bereits
  validierten `ProgramCatalogRevision` des aufgeloesten
  `RuntimeConfigurationSnapshot`;
- die gespeicherte `ProgramDocument`-Kopie ist die tatsaechlich verwendete,
  unveraenderliche Laufkopie;
- ein neues Fachrequest erhaelt seine `CommandId` an der
  Application-Grenze, und `UiRequestId.value == CommandEnvelope::id`;
- die Neustartbasis fuer diese ID ist im normalen Fall der committed
  `commandIdHighWater` im bestehenden kanonischen `RunPersistenceHead`, nicht
  das begrenzte Replayfenster; ein vollstaendig bewiesener
  `NoPersistedRun`-Store bildet die headlose Ausnahme mit logischem HWM `0`;
- ein autorisierter Konfigurations-Epochwechsel macht den neuen
  Run-Persistenzraum erst nach dem bestehenden Application-Handoff mit beiden
  neuen Slots und Committed-HWM `0` benutzbar; ein blosser `ForeignEpoch`-
  Befund wird niemals als leer behandelt;
- ein neuer aktiver Lauf erhaelt seine `runId` nur an dieser Grenze;
- Touch- und spaetere Web-Adapter liefern keine Lauf- oder Command-Identitaet
  als Benutzereingabe;
- eine Bestaetigungswiederholung verwendet dieselbe Command-ID und denselben
  bereits vorbereiteten Lauf-Identifier.

Nicht Teil dieses Plans sind:

    UI_SHELL_IMPLEMENTATION=NO
    LOCAL_TOUCH_WORKSPACE=NO
    DISPLAY_DRIVER=NO
    REAL_TOUCH=NO
    TOUCH_CALIBRATION=NO
    RENDERER_OR_WIDGET_FRAMEWORK=NO
    GPIO_OR_BACKLIGHT=NO
    WEB_TRANSPORT=NO
    NEW_COMMAND_BUS=NO
    NEW_DISPATCHER=NO
    SECOND_PERSISTENCE_COORDINATOR=NO
    SECOND_RECOVERY_LOGIC=NO
    NEW_SAFETY_OR_LIFECYCLE_GATE=NO
    PER_PROGRAM_REVISION=NO
    PROVENANCE_HASH=NO
    SECOND_PROVENANCE_REGISTRY=NO
    SECOND_PERSISTED_COMMAND_COUNTER=NO
    GLOBAL_ID_REGISTRY=NO
    UUID_LIBRARY=NO
    MANUAL_TIME_TEMPERATURE_RUN_CONTRACT=NO

Die fachlichen Run-, Safety-, Recovery-, Sensor-, FSM-, Configuration- und
Persistenzentscheidungen bleiben bei ihren bestehenden Ownern. #144 macht
deren Identitaets- und Codecgrenze eindeutig; es leitet keine neuen
Fachentscheidungen aus UI-Daten ab.

## 3. Verbindliche Quellen und aktueller Befund

Der Implementierung liegt mindestens dieser direkte Vertragsbestand zugrunde:

- `docs/RUN_COMMANDS.md` fuer `CommandEnvelope`, Decide-/Apply-Semantik,
  Bestaetigung, Replay und die bestehende Begrenzung des
  `processedCommandIds`-Fensters;
- `docs/RUN_PERSISTENCE.md` fuer Write-before-Apply, Head/Checkpoint/
  Fallback, Schema-Fail-Closed und `RunPersistenceCoordinator`;
- `docs/CONFIGURATION_PERSISTENCE.md` fuer
  `RuntimeConfigurationSnapshot`, `ProgramCatalogRevision`, `StorageEpoch`
  und die vorhandene atomare Configuration-Revision;
- `docs/SPECIFICATION_REVIEW.md`, `docs/ENGINEERING_PRINCIPLES.md`,
  `docs/DECISIONS.md` mit ADR-013 und den lokalen `AGENTS.md` fuer R1- und
  Modulgrenzen;
- der gemergte #25-Vertrag aus
  `docs/tasks/issue-25-device-ui-contracts-plan.md` und der freigegebene
  Downstream-Plan
  `docs/tasks/issue-26-local-touch-shell-plan.md` auf
  `aea6acb2e51147c6452d728a5a45840236ab1fdf`.

Der relevante Codebefund auf `BASE_SHA` ist:

- `RunProgramSnapshot::sourceProgramRevision` und die zugehoerigen
  Start-Requests verwenden heute einen frei interpretierbaren `uint32_t`;
- der Run-Codec schreibt dieses Feld aktuell in Schema 3 als 32-Bit-Wert;
  bekannte Schemas sind 1, 2 und 3;
- `CommandId` ist bereits ein `uint64_t`, aber es gibt noch keinen
  produktiven app-owned Erzeuger;
- `RunPersistenceSnapshot::persistedRunCommandIds` ist nur das bestehende
  begrenzte FIFO-/Replayfenster fuer eligible Run-Command-IDs. Bei voller
  Kapazitaet wird die aelteste ID entfernt; der historische Vertrag beweist
  weder globale Monotonie noch, dass das Fenstermaximum alle frueheren IDs
  uebersteigt;
- `RunPersistenceHead` wird mit dem bestehenden Run-Commit persistiert, hat
  aber noch keinen committed High-Water-Wert. Der bestehende Coordinator
  bewahrt das ID-Fenster bei `NoActiveRun`, was allein keine sichere
  Identitaetsbasis ist;
- `RunPersistenceCoordinator::loadAndInitializeInto()` klassifiziert einen
  Head- und slotfreien Store kanonisch als `NoPersistedRun` und setzt seinen
  Zustand auf `ReadyEmpty`. Ein solcher nachweislich fabrikneuer leerer
  Store hat noch keinen Head, ist aber kein unklarer Persistenzzustand;
- `ConfigurationRecoveryService::beginAuthorizedFactoryReset()` aendert im
  bestehenden Resetpfad die Konfigurations-`StorageEpoch`, besitzt aber
  keinen Run-Coordinator und retiriert dessen Head/Slots nicht. Ein
  `ForeignEpoch` darf deshalb nur durch einen expliziten, vom Application-
  Owner uebergebenen Resetabschlussnachweis behandelt werden;
- der aktuelle Run-Store-Port besitzt nur die bestehenden Head-/Slot-Reads
  und exacten Writes fuer `rh0`, `rc0` und `rc1`. Es gibt keinen Delete-Port;
  ein autorisierter Epoch-Handoff muss vorhandene alte Slots daher vor dem
  neuen Committed-Head mit gueltigen neuen Epoch-Records ueberschreiben;
- `ProgramStartRequest`, `ManualRunPlanRequest`, `StopRequest` und
  `CompletionRequest` enthalten heute owning Run- beziehungsweise Cooling-
  Felder, darunter `runId`; diese Felder bleiben fuer den Fachowner
  notwendig, werden aber an der Application-Grenze befuellt;
- der #25-UI-Vertrag darf diese owning Identitaetsfelder deshalb nicht als
  frei erzeugte Adaptereingabe weiterreichen.

## 4. Architektur- und Ownergrenze

Die Identity-Komposition liegt in `fermentation_app` an der bestehenden
Application-Grenze. Sie wird als kleine, instanzgebundene Komponente geplant,
beispielsweise `ApplicationRunIdentity` mit Header/Implementierung in
`application_run_identity.hpp/.cpp`. Der Name ist kein neuer Fachlayer; die
Komponente besitzt nur die Erzeugung und Pruefung von Command- und Run-
Identitaet.

Die Grenze verwendet die bestehenden Owner in dieser Reihenfolge:

```text
UI-/Web-Adapterwerte ohne Identitaet
    -> bestehende Application-Grenze
       -> ApplicationRunIdentity: CommandId reservieren, runId erzeugen
       -> bestehende Program-/Manual-/Stop-/Completion-Request-Struktur
       -> bestehende decide*-Funktion
       -> bestehender TemperatureControlApplicationOrchestrator
          -> persistCommand / persistFreshStartCommand /
             persistTransition / persistSensorSelection
       -> bestehender RunPersistenceCoordinator und seine Lifecycle-Handoffs
```

Der bereits autorisierte Konfigurations-Epochwechsel hat einen getrennten,
einmaligen Handoff an derselben Application-Kompositionsgrenze:

```text
ConfigurationRecoveryService::beginAuthorizedFactoryReset() /
ConfigurationRecoveryService::boot()
    -> persistenter Pending-Handoff-Zustand mit neuem Runtime-StorageEpoch
    -> FermentationApplication
       -> private Proof/Continuation aus kanonischer Configuration-Evidenz
       -> bestehender RunPersistenceCoordinator: Slots validieren/schreiben
       -> ConfigurationRecoveryService: Pending -> Committed persistieren
       -> Coordinator: exakten Committed-Head schreiben
       -> ConfigurationRecoveryService: Committed -> Consumed persistieren
       -> loadAndInitialize() / ReadyEmpty oder Ready
```

`FactoryResetCompleted` ist dabei nur der Configuration-Erfolgsausgang, der
den persistierten `Pending`-Zustand bindet; er ist kein allgemeines
`ForeignEpoch`-Ignore-Signal. Nach `Consumed` mintet die Configuration-
Recovery bei spaeteren Boots keinen Proof mehr, auch wenn die letzte
Manifestoperation weiterhin `FactoryReset` lautet. Der Coordinator prueft
zusaetzlich den exakt vorherigen Epochwert und den vollstaendigen alten
Run-Recordgraphen. Dieser schmale Handoff ist kein neuer Resetdienst und keine
zweite Recovery- oder Persistenzlogik.

Der Identity-Baustein ruft keine `decide*`-Funktion und keinen
`RunPersistenceCoordinator` direkt fuer eine Mutation auf. Er liefert nur die
fehlende Identitaet an den vorhandenen Application-Requestaufbau. Die
mutierende Ausfuehrung bleibt beim bestehenden
`TemperatureControlApplicationOrchestrator`; dessen Handoffs und die
Write-before-Apply-Reihenfolge werden nicht dupliziert.

`FermentationUiCommandBridge::makeEnvelope()` erhaelt ausschliesslich die von
der Application gemintete, nicht frei konstruierbare
`ApplicationCommandIdentity` und spiegelt deren `CommandId` in
`CommandEnvelope::id`. Adapterwerte koennen weder `UiRequestId` noch
`CommandId` ersetzen. Die Quelle, der monotone Zeitwert, aktuelle Evidenz und
erwartete Revisionen werden ebenfalls von der bestehenden Application-
Komposition geliefert. Es entsteht kein neuer Dispatcher und kein zweiter
Command-Bus.

## 5. Neutrale Run-Provenienz

### 5.1 Typ und Bedeutung

Das bisherige Feld bleibt als fachliche Position erhalten, bekommt aber einen
starken, eindeutig neutralen 64-Bit-Typ:

    RunProgramSourceRevisionTag
    using RunProgramSourceRevision =
        device_platform::StrongId<RunProgramSourceRevisionTag, std::uint64_t>

Die konkrete vorhandene Strong-ID-Syntax ist zu verwenden; es wird kein
zweites Strong-ID-System gebaut. Der Typ bedeutet ausschliesslich:

> numerische Provenienz des Katalog-/Quellstands, aus dem die Laufkopie
> erzeugt wurde.

Er bedeutet insbesondere nicht:

- eine Revision eines einzelnen Programms;
- eine historische Behauptung ueber Legacy-Daten;
- einen Hash des `ProgramDocument`;
- eine UI-Refresh- oder Editor-Revision.

`RunProgramSnapshot::sourceProgram` bleibt der vollstaendige, validierte
`ProgramDocument` und damit die unveraenderliche tatsaechliche Laufkopie.
`sourceProgramRevision` wird auf den starken 64-Bit-Typ umgestellt. Die
bestehende Validierung verlangt weiterhin einen gueltigen, von null
verschiedenen Wert.

### 5.2 Erzeugung eines neuen Programmstarts

Die Application-Grenze baut einen Programmstart nur in dieser Reihenfolge:

1. den aktuellen, vertrauenswuerdigen
   `RuntimeConfigurationSnapshot` laden;
2. eine vom Adapter gelieferte erwartete `ProgramCatalogRevision` gegen den
   aktuellen Snapshot pruefen;
3. die Programm-ID aus dem aktuellen `ProgramCatalog` aufloesen;
4. das vollstaendige aufgeloeste Dokument mit der bestehenden
   Runnable-Validierung pruefen;
5. `RunProgramSourceRevision` explizit aus dem bereits validierten Wert von
   `RuntimeConfigurationSnapshot::programCatalogRevision()` erzeugen;
6. den unveraenderten bestehenden Start-/Orchestratorpfad mit Dokument,
   Source-Kind und neutraler Provenienz aufrufen.

Die Umwandlung erfolgt nur ueber eine benannte, schmale Funktion wie
`makeRunProgramSourceRevision(ProgramCatalogRevision)`. Sie prueft die
Gueltigkeit der Katalogrevision und erlaubt keine implizite oder
verlustbehaftete Konvertierung. Ein stale Katalog wird vor jeder Startmutation
abgelehnt. Die UI-/Editor-Staleness bleibt ausschliesslich an
`ProgramCatalogRevision` gebunden; `RunProgramSourceRevision` ist kein
nachtraegliches Stale-Gate.

### 5.3 Start- und Snapshot-Invariante

Nach einem erfolgreichen neuen Programmstart gilt:

    run.sourceProgramRevision.value()
        == validatedRuntimeSnapshot.programCatalogRevision().value()

Die Gleichheit ist eine Herkunftsabbildung zum Startzeitpunkt, nicht eine
per-program Revision. Spaetere Katalogbearbeitungen koennen den aktiven
`ProgramDocument`-Snapshot nicht veraendern. Manual-Holding-Laeufe haben
keine Programmquelle und erhalten daher keine erfundene Programmprovenienz.

## 6. Abwaertskompatible Run-Persistenz

### 6.1 Schema-Vertrag

Die bestehende Run-Persistenz wird schmal von Schema 3 auf Schema 4 erweitert.
Es wird keine zweite Codec- oder Migrationslogik angelegt.

| Schema | Wirewert fuer Programm-Provenienz | Decode-Ziel | Semantik |
|---|---|---|---|
| 1 | historischer `uint32_t` | `RunProgramSourceRevision{value}` | nur numerisch erweitert; keine Katalogbedeutung |
| 2 | historischer `uint32_t` | `RunProgramSourceRevision{value}` | nur numerisch erweitert; keine Katalogbedeutung |
| 3 | historischer `uint32_t` | `RunProgramSourceRevision{value}` | nur numerisch erweitert; keine Katalogbedeutung |
| 4 | `uint64_t` Big Endian | `RunProgramSourceRevision{value}` | neue verlustfreie Run-Provenienz |

Fuer Schema 1 bis 3 wird der vorhandene 32-Bit-Wert lediglich numerisch und
verlustfrei auf 64 Bit erweitert. Ein Legacy-Wert `7` wird also zu neutraler
Provenienz `7`, aber niemals nachtraeglich als
`ProgramCatalogRevision(7)` etikettiert. Der bisherige Null-/Invalidbefund
bleibt ein Invalidbefund; Decode darf ihn nicht durch Umbenennung gueltig
machen.

Neue Writes von Head, Current- und Checkpoint-/Fallback-Records verwenden
Schema 4. Die bereits vorhandenen schemaabhaengigen Felder von Schema 1 bis 3
bleiben an ihren bestehenden Einfuehrungsschemas. Nur die Breite des
Provenienzfelds aendert sich in Schema 4. Ein alter, gelesener Snapshot darf
bei einer spaeteren regulaeren bestehenden Persistenzaktion als Schema 4
geschrieben werden; dafuer entsteht kein Migrationsjob und kein neuer Record.

Die bestehende Referenz- und Slotlogik darf gueltige alte und neue Schemas in
derselben Recoverykette lesen, soweit dies der vorhandene Codecvertrag
zuliesst. Schema 4 ist die einzige neue bekannte Version. Ein unbekanntes
neueres Schema bleibt im bestehenden Load-/Decodepfad fail-closed und darf
weder einen Run noch einen Allocator-Hochwasserstand freigeben.

### 6.2 Bootstrap-Schema 2, Sequenz und Handoff-Historie

Das bestehende Bootstrap-Schema 1 bleibt ein unveraenderter Legacyvertrag.
Schema 1 behaelt seine 5-Byte-Payload, die Zustandswerte
`Initializing=1`, `Initialized=2` und `Resetting=3`, die geschlossene
Relation

    Initializing: E=1, S=1
    Initialized:  S=2*E
    Resetting:    E>=2, S=2*E-1

und die ausschliessliche Successorfolge mit `next.S = previous.S + 1`.
Schema-1-Goldens werden bytegenau dekodiert und plausibilisiert. Es wird kein
Schema-1-Writer eingefuehrt; ein neuer Bootstrap-Write verwendet immer
Schema 2.

Das In-Memory-Modell erhaelt fuer Schema 2 eine kleine, optionale
Handoff-Bindung, ohne einen zweiten Record einzufuehren:

    RunEpochHandoffState = None | Pending | Committed | Consumed
    None       = 0
    Pending    = 1
    Committed  = 2
    Consumed   = 3

`Initializing` und `Resetting` duerfen nur `HandoffState::None` tragen.
`Initialized` darf `None`, `Pending`, `Committed` oder `Consumed` tragen.
Bei `Pending`, `Committed` und `Consumed` sind `previousEpoch` und
`currentEpoch` Teil derselben gebundenen In-Memory-Struktur. Bei `None` sind
beide nicht vorhanden. Der Decoder beziehungsweise der geladene
Bootstrapkandidat fuehrt die Envelope-Schemaversion mit, damit Plausibilitaet
und Successorpruefung Schema 1 und Schema 2 nicht vermischen; sie wird nicht
als zusaetzliches Payloadfeld dupliziert.

#### Geschlossene Schema-2-Einzelrecordrelation

Schema 2 verwendet weiterhin die bestehende `ConfigurationBootstrapSequence`
als globale, streng fortgeschriebene Historiennummer. Damit die drei
gueltigen Schema-1->2-Einstiege auch bei einer bereits hoeheren alten Epoche
ohne Sonderzaehler moeglich bleiben, bildet Schema 2 eine geschlossene
Familie moeglicher historischer Anker. Fuer `E>=2` wird checked berechnet:

    B(E) = 2*E - 1

Fuer die vier zusaetzlichen Schema-2-Zustaende gilt der feste Phasenoffset
`p`:

| Schema-2-Zustand | `p` | Einzelrecordbedingung |
|---|---:|---|
| `Resetting/None` | 0 | `S >= B(E)`, `S-B(E)-p` gerade, `k=(S-B(E)-p)/2`, `0 <= k <= E-2` |
| `Initialized/Pending` | 1 | dieselbe Bedingung mit `p=1` |
| `Initialized/Committed` | 2 | dieselbe Bedingung mit `p=2` |
| `Initialized/Consumed` | 3 | dieselbe Bedingung mit `p=3` |

Die Subtraktionen, die Halbierung und die Schranke werden nur nach checked
Vorbedingungen ausgefuehrt. Damit beschreibt `k` ausschliesslich die Anzahl
moeglicher bereits abgeschlossener Schema-2-Phasen seit dem letzten
Schema-1-Anker; es ist kein zusaetzliches persistiertes Feld. Jeder einzelne
Schema-2-Record muss diese Relation erfuellen und eine gueltige lokale
Handoffbindung besitzen. Ein CRC-gueltiger, aber mathematisch nicht moeglicher
Record ist `IntegrityFailure`.

Die beiden Schema-2-Zustaende ohne zusaetzliche Handoffbindung sind nur der
fabrikneue Einstieg `Initializing/None (E=1,S=1)` und sein direkter Nachfolger
`Initialized/None (E=1,S=2)`. Ein unveraendert vorgefundener Schema-1-
`Initialized`-Record wird nicht als gleichsequenter Schema-2-Record
umgeschrieben; sein erster neuer Successor ist der nachfolgend definierte
Schema-2-`Resetting`-Record.

Fuer gebundene Records gilt zusaetzlich:

- `currentEpoch` ist exakt gleich dem Envelope-`StorageEpoch`;
- `previousEpoch` und `currentEpoch` sind vorhanden, beide nicht null und
  `previousEpoch + 1 == currentEpoch` ist checked beweisbar;
- `currentEpoch >= 2`, die Envelope- und Payloadwerte sind nicht frei
  voneinander abweichend;
- `Pending`, `Committed` und `Consumed` sind nur mit
  `ConfigurationBootstrapState::Initialized` gueltig;
- bei `None` fehlen die beiden Epochenfelder vollstaendig.

#### Versionierte Successor-Matrix

Jeder neue Successor erhoeht `ConfigurationBootstrapSequence` checked exakt
um eins. Die Matrix ist versionsbewusst; ein Schema-2-Record kann keinen
Schema-1-Record als seinen Nachfolger ausgeben, und eine Schema-1-Abfolge wird
nicht mit Schema-2-Regeln plausibilisiert:

| Ausgang | Ziel | Schema/State/Handoff | Epochenbindung | Sequence |
|---|---|---|---|---|
| fabrikneu | `Initializing` | 2 / `Initializing/None` | `E=1` | `S=1` |
| `Initializing/None` | `Initialized` | 1->1 oder 2->2 / `Initialized/None` | gleiche Epoche | `S+1` |
| `Initialized/None` | `Resetting` | 1->2 oder 2->2 / `Resetting/None` | `E+1` checked | `S+1` |
| `Resetting/None` | `Initialized/Pending` | 1->2 oder 2->2 | `previous=E-1`, `current=E` | `S+1` |
| `Initialized/Pending` | `Initialized/Committed` | 2->2 | unveraendert | `S+1` |
| `Initialized/Committed` | `Initialized/Consumed` | 2->2 | unveraendert | `S+1` |
| `Initialized/Consumed` | `Resetting/None` | 2->2 | `E+1` checked; alte Bindung entfällt | `S+1` |

Die drei Cross-Schema-1->2-Einstiege sind damit explizit:

- Schema-1 `Initializing(E=1,S=1)` -> Schema-2
  `Initialized/None(E=1,S=2)`;
- Schema-1 `Initialized(E,S=2*E)` -> Schema-2
  `Resetting/None(E+1,S+1)`;
- Schema-1 `Resetting(E,S=2*E-1)` -> Schema-2
  `Initialized/Pending(E,S+1)`.

Die Schema-2-Einzelrecordrelation ist so bemessen, dass jeder dieser
Successoren mit `k=0` gueltig ist. Nach einem Schema-2-`Consumed` folgt beim
naechsten Reset `Resetting/None` mit `S+1`; dadurch steigt `k` checked um eins
und die Folge bleibt auch nach mehreren Epochen geschlossen. Der Scanner
ordnet zwei unterschiedliche Records nur dann als Historie, wenn die
Sequence-Differenz exakt eins ist und die passende versionierte Zeile gilt.
Schema 1 -> Schema 2 ist nur fuer die drei genannten Zeilen zulaessig,
Schema 2 -> Schema 1 sowie alle anderen Cross-Schema-/State-Kombinationen
bleiben `IntegrityFailure`. Gleichsequente Bytes sind nur als byteidentisches
Duplikat zulaessig.

`Pending` und `Committed` sind keine Reset-Ausgangszustaende. Ein
Resetversuch waehrend einer offenen dieser Phasen wird vor jedem Write
abgelehnt. `Initialized/Consumed` darf beim naechsten autorisierten Reset
durch `Resetting/None` ersetzt werden; die alte `previousEpoch` /
`currentEpoch`-Bindung wird dabei nicht weitergetragen.

#### Exaktes Schema-2-Wireformat

Der generische Envelope bleibt Version 1 und traegt weiterhin
`RecordTypeId=6`, die `StorageEpoch` im Envelope-`uint64` und die
`ConfigurationBootstrapSequence` im Envelope-`versionValue` als `uint64`.
Alle Mehrbytefelder sind Big Endian. UTC ist fuer Bootstrap ausgeschlossen.
Die Schema-2-Payload ist:

| Reihenfolge | Feld | Wirebreite |
|---:|---|---:|
| 1 | `ConfigurationStorageFormatVersion` | `uint32` Big Endian |
| 2 | `ConfigurationBootstrapState` | `uint8` |
| 3 | `RunEpochHandoffState` | `uint8` |
| 4 | `previousEpoch` bei gebundener Phase | `uint64` Big Endian |
| 5 | `currentEpoch` bei gebundener Phase | `uint64` Big Endian |

Die Reihenfolge ist strikt. `None` hat exakt 6 Payloadbytes und ein
37+6=43-Byte-Envelope. `Pending`, `Committed` und `Consumed` haben jeweils
exakt 22 Payloadbytes und ein 37+22=59-Byte-Envelope. Die neue maximale
Bootstrap-Envelopegroesse ist daher exakt 59 Bytes; das bestehende
Schema-1-Limit 42 bleibt als historische Mindest-/Decodegrenze bekannt.
`configuration_limits.hpp` fuehrt dafuer getrennte Konstanten fuer die
Schema-1-Payload (5), die Schema-2-`None`-Payload (6), die gebundene
Schema-2-Payload (22) und das neue Maximum (59); kein Writepfad verwendet
mehr das alte 5/42-Limit fuer Schema 2.
`ConfigurationStorageFormatVersion` bleibt exakt 1. Payloads mit UTC-Tag,
fehlenden oder zusaetzlichen Bytes, unbekannten Zustandswerten oder
abweichender Laenge werden abgelehnt. Ein unbekanntes Bootstrap-Schema 3 oder
hoeher bleibt `UnsupportedNewerSchema`/fail-closed.

`previousEpoch`/`currentEpoch` werden nur bei einer gebundenen Phase
serialisiert. Beim Decode muessen sie die oben definierte checked Relation
erfuellen, `currentEpoch` muss dem Envelope entsprechen, und die
Schema-2-Recordrelation muss ebenfalls erfuellt sein. Die Payload wiederholt
keine UTC-, Zeit-, Manifest-, Run- oder Commandmetadaten.

#### Zwei-Slot- und Crashvertrag

Der bestehende Zwei-Slot-Store schreibt jeden Successor einzeln in den jeweils
nicht kanonischen Slot und bestaetigt ihn per exaktem Readback. Die
Schema-1-/Schema-2-Historie wird erst nach Einzelrecord- und
Successorpruefung akzeptiert. Daraus folgen fuer den Handoff:

- Nach erfolgreich persistiertem `Consumed` liegt im verbleibenden aelteren
  kanonischen Slot exakt der vorherige `Initialized/Committed`-Record, nicht
  `Pending`. Das ist eine Folge der vier aufeinanderfolgenden Handoff-
  Successoren und der Zwei-Slot-Rotation.
- Fehlt der neueste `Consumed`-Record, bleibt ein einzelner aelterer
  `Committed`-Record hoechstens die bereits gebundene exakte
  Target-Fortsetzung. Er ist keine neue freie Reset-Autorisierung. Ist der
  neueste Slot unlesbar oder widerspruechlich, blockiert der vollstaendige
  Scan fail-closed; er darf den alten Slot nicht als neuen Resetbeweis
  auswaehlen.
- Ein `Committed`-Record darf nur den exakt vorbereiteten Target-Head der
  gebundenen Handoff-Epoche finalisieren. Ein alter Previous-Epoch-Graph,
  ein beliebiger Orphan oder ein neuer Reset ist damit niemals zulaessig.
- Erst nach bestaetigtem Target-Head wird `Committed -> Consumed` geschrieben.
  Ein Stromausfall davor laesst `Committed` als eng gebundene
  Fortsetzungserlaubnis bestehen; ein Stromausfall danach laesst die
  dauerhaft verbrauchte `Consumed`-Tatsache zurueck. Die verlorene oder
  unlesbare neueste `Consumed`-Kopie erzeugt daher weder einen neuen freien
  Proof noch einen HWM-/Run-ID-Rollback.
- Beim naechsten autorisierten Reset wird die alte `Consumed`-Bindung durch
  den neuen `Resetting/None`-Successor abgeloest. Sie wird nicht in
  `Pending`/`Committed` des neuen Epochenzyklus kopiert.

Jeder unklare Slot-Write, Readback, CRC-, Sequence-, Schema- oder
Epochenbefund bleibt `IntegrityFailure`, `Unavailable` oder der bestehende
indeterminierte Writezustand. Keine Phase wird aus einem einzelnen CRC-
gueltigen, aber historisch unmoeglichen Record geraten.

### 6.3 Kanonische Command-High-Water-Ankerung

Das vorhandene 32er-Fenster ist kein High-Water-Vertrag und wird nicht zu
einem solchen umgedeutet. Die kleinste korrekte Erweiterung ist ein einziges
neues Feld im bestehenden `RunPersistenceHead`:

    std::optional<CommandId> commandIdHighWater

Fuer einen etablierten neuen Schema-4-Identitaetsraum schreibt Schema 4 dieses
Feld sowohl in den `Prepared`- als auch in den `Committed`-Head. Bei einem
neuen Schema-4-Commit wird der Wert atomar mit dem bereits bestehenden
Head-/Snapshot-Commit fortgeschrieben. Ein regulaeres Re-Write eines aus
Schema 1 bis 3 dekodierten Legacy-Zustands darf das Presence-Feld dagegen
bewusst als `Unknown`/nicht vorhanden erhalten; dadurch wird kein historischer
High Water erfunden.

Die historische Nicht-Ruecklaeufigkeit gehoert nicht in den isolierten Codec,
sondern in `RunPersistenceCoordinator::writeSnapshotCore()` und dessen
Head-Konstruktion. Der Coordinator haelt den bisher kanonischen committed
High Water und bildet vor beiden Head-Write-Formen einen checked Kandidaten:

- bei einer neuen eligible Run-Command-ID muss der Kandidat mindestens diese
  `CommandId` enthalten und darf den bisherigen committed High Water nicht
  unterschreiten;
- bei Transition-, Sensor- und Recovery-Commits ohne neue Command-ID wird
  der bisherige High Water unveraendert weitergegeben;
- der periodische Direktpfad `Slot -> CommittedHead` uebernimmt den bisherigen
  High Water ebenfalls unveraendert; er darf ihn weder aus dem Snapshotfenster
  neu berechnen noch auf null setzen;
- der nichtperiodische Pfad schreibt denselben Kandidaten in den
  `Prepared`-Head und danach in den `Committed`-Head;
- ein frisch etablierter Schema-4-Identitaetsraum darf den Wert `0` als
  explizit vorhandenen Anfangswert tragen;
- ein Schema-4-Head eines etablierten Identitaetsraums muss das Feld tragen,
  und kein Coordinator-Write darf einen niedrigeren Wert konstruieren;
- ein aus Legacy stammender `Unknown`-Wert bleibt auch bei passivem Rewrite
  oder periodischem Checkpoint `Unknown`; ein solcher Write wird niemals zu
  `0` oder einem erfundenen bekannten Wert.

Der `Prepared`-Head ist nie Neustartautoritaet. Erst der erfolgreich
geschriebene und als `Committed` bestaetigte Head ist die Allocatorbasis. Ein
deterministisch nicht geschriebener Prepared-/Slot-/Committed-Schritt laesst
den letzten committed High Water unveraendert. Bei einem
`CommitOutcomeUnknown` oder sonst indeterminierten Write bleibt der
Coordinator blockiert; aus dem Prepared-Record wird keine neue ID freigegeben.
Ein bereits committed High Water wird weder zurueckgesetzt noch aus einem
Snapshotfenster rekonstruiert.

`commandIdHighWater` im committed Head ist damit die einzige kanonische
Neustartbasis innerhalb einer `StorageEpoch`. Es ist kein zweiter Zaehler,
keine zweite Registry und kein zusaetzliches Persistenzrecord. Das weiterhin
persistierte `persistedRunCommandIds`-Array bleibt unveraendert das begrenzte
Replay-/AlreadyProcessed-Fenster. Sein Maximum wird niemals als High Water
verwendet.

Bei Schema 1 bis 3 existiert dieses Head-Feld historisch nicht. Der Decoder
setzt deshalb den High Water auf `Unknown`/nicht vorhanden. Die Legacy-ID-
Folge wird nicht als monoton angenommen, und auch ein hoher Wert im alten
Fenster wird nicht als Untergrenze oder High Water verwendet. Legacy-Records
bleiben lesbar und fuer die bestehende passive beziehungsweise fail-closed
Recovery auswertbar; ein neuer eligible Command oder ein neuer Run darf in
dieser alten `StorageEpoch` jedoch nicht allokiert werden.

#### Persistenter einmaliger Epoch-Handoff

Ein Head- und slotfreier Store wird nicht wie ein fehlender/unklarer Head
behandelt. Wenn der bestehende Coordinator vollstaendig
`NoPersistedRun`/`ReadyEmpty` festgestellt hat und die aktuelle
`StorageEpoch` gueltig ist, bildet dies den logischen neuen Identitaetsraum
mit High Water `0`. Es wird kein kuenstlicher `NoActiveRun`-Head vor der
ersten Anfrage persistiert. Die erste eligible Anfrage kann ID `1` erhalten;
erst ihr erfolgreicher neuer Schema-4-Commit schreibt den Committed-Head mit
High Water `1`.

Ein alter oder Legacy-Head derselben Epoche bleibt dagegen `Unknown` und fuer
neue Vergabe unavailable. Der bestehende
`ConfigurationRecoveryService::beginAuthorizedFactoryReset()` ist allein
noch kein Run-Handoff: Er aendert die Konfigurations-Epoche und besitzt
keinen Run-Coordinator. Der Handoff wird deshalb nur ueber die bestehende
Configuration-/Application-/Run-Persistence-Komposition fortgesetzt.

Die Reset-Autorisierung wird als kleiner, additiver Zustand im bestehenden
kanonischen `ConfigurationBootstrapRecord` persistiert; es entsteht weder
ein zweites Reset-Record noch eine Registry. Der Bootstrap-Datensatz erhaelt
eine versionierte, streng validierte Bindung:

    RunEpochHandoffState = None | Pending | Committed | Consumed
    previousEpoch
    currentEpoch

Fuer diesen additiven Bootstrap-Vertrag wird die Envelope-Schemaversion 2
verwendet. Schema 1 bleibt lesbar und wird beim Decode als `None` ohne
Handoff-Bindung behandelt; neue Bootstrap-Writes verwenden ausschliesslich
Schema 2. Ein unbekanntes neueres Bootstrap-Schema bleibt wie bisher
fail-closed. Bei `None` sind die beiden Epochfelder nicht vorhanden. Bei
`Pending`, `Committed` und `Consumed` muessen beide Felder vorhanden,
ungueltige Nullwerte ausgeschlossen und die checked Epochbeziehung nachweisbar
sein. Die Configuration-Storage-Formatversion bleibt davon getrennt und
unveraendert.

`Pending` wird zusammen mit dem kanonischen `Initialized`-Nachfolger des
Factory-Reset-Graphen geschrieben. Das gilt sowohl fuer den direkten
`FactoryResetCompleted`-Ausgang als auch fuer die spaetere
`boot()`-Finalisierung nach einem zuvor indeterminierten Configuration-Reset.
Die Bindung ist nur gueltig, wenn `currentEpoch` exakt die Bootstrap-Epoche,
`previousEpoch + 1 == currentEpoch` ohne Overflow und der Configuration-
Resetpfad selbst kanonisch abgeschlossen ist. Die blosse letzte
Manifestoperation `FactoryReset`, ein `ForeignEpoch`-Befund oder
`old + 1 == current` erzeugen keinen Proof. Alte Bootstrap-Schemas ohne
diese Bindung bleiben lesbar, liefern aber keine Handoff-Autorisierung.

Die Zustandsuebergaenge sind der persistente Linearisierungsvertrag:

1. `Pending` ist die einmalige, offen rekonstruierbare Autorisierung fuer
   genau die gebundene Epoche. Die ConfigurationRecoveryService erzeugt
   daraus eine nicht frei konstruierbare
   `AuthorizedRunEpochHandoffProof`. RAM-Verbrauch des Proofs veraendert den
   persistenten Zustand nicht.
2. Der bestehende `RunPersistenceCoordinator` liest Head und beide Slots und
   validiert Epochbeziehung, alten Committed-Graphen beziehungsweise den
   vollstaendig leeren Store sowie die exakten Targetbytes selbst. Er schreibt
   die beiden neuen Schema-4-`NoActiveRun`-Slots. Erst wenn beide Slotwrites
   sicher erfolgreich waren, fordert die Application beim Configuration-
   Owner den persistenten Uebergang `Pending -> Committed` an.
3. `Committed` bedeutet: beide Target-Slots sind sicher vorhanden, der neue
   Committed-Head ist aber noch nicht als final geschrieben bestaetigt. Dieser
   Zustand ist keine neue Autorisierung und niemals ein Grund, einen
   beliebigen alten oder fremden Graphen zu ueberschreiben. Der Coordinator
   darf daraus nur den exakt vorbereiteten Target-Head finalisieren.
4. Danach schreibt der Coordinator den neuen Committed-Head der aktuellen
   Epoche mit HWM `0`. Erst bei sicher bestaetigtem Target-Head persistiert
   der Configuration-Owner `Committed -> Consumed`. `Consumed` bleibt als
   gebundene historische Tatsache erhalten und erzeugt bei spaeteren Boots
   weder Proof noch Fortsetzungsfreigabe.

Die Phasen sind bewusst so geordnet, dass ein einzelner per-Key-Write keine
erneute Autorisierung ermoeglicht: Ein Stromausfall nach Slot 0 oder Slot 1,
aber vor `Committed`, laesst `Pending` bestehen und erlaubt die exakte
Fortsetzung. Ein Stromausfall nach `Pending -> Committed` laesst nur die
Finalisierung des exakt belegten Target-Graphs zu. Ein Stromausfall nach
dem neuen Head, aber vor `Consumed`, findet weiterhin `Committed` vor und
verifiziert den exakten Target-Head, bevor `Consumed` wiederholt wird. Ein
alter Epoch-Graph mit `Committed` wird nicht erneut ueberschrieben, sondern
fail-closed.

Ein indeterminierter Write des Configuration-Markers oder des Run-Heads
loest dieselbe konservative Regel aus: Readback muss den vollstaendigen,
gebundenen alten beziehungsweise neuen Zustand eindeutig bestaetigen; ein
gemischter, alter, korrupt/unlesbarer oder nicht entscheidbarer Zustand
bleibt unavailable. Insbesondere darf ein `Pending`-Marker nach einem
bereits persistierten `Consumed`-Zustand nicht durch Rollback der Run-Slots
rekonstruiert werden; `Consumed` in der kanonischen Configuration-Evidenz
verhindert jede neue Autorisierung innerhalb dieser `StorageEpoch`.

Der kleine Proof beziehungsweise die Fortsetzungscapability traegt nur die
gebundenen Epochen und den persistenten Handoff-Zustand; sie enthaelt keine
Command-ID, Lauf-ID, Persistenzdaten oder frei waehlbare Foreign-Epoch. Nur
der ConfigurationRecoveryService mintet diese privaten Typen aus seiner
kanonischen Bootstrap-Evidenz. Die Application komponiert die Phasen mit dem
bestehenden Coordinator; der Coordinator prueft Epoch-, Slot-, Head- und
Targetbytes nochmals selbst. Es gibt keinen zweiten Resetdienst,
Persistenzkoordinator, allgemeinen Transaction- oder ForeignEpoch-Repair-
Pfad.

#### Bootdelegation fuer leere und partielle Run-Stores

Die Application interpretiert keinen `RunPersistenceLoadStatus` selbst als
Legitimation. Liegt eine gueltige offene `Pending`- oder
`Committed`-Handoff-Bindung vor, darf sie den bestehenden Coordinator jedoch
auch bei den genau dafuer zulaessigen Loadformen aufrufen:

- vollstaendig leerer Store / `NoPersistedRun` / `ReadyEmpty`;
- `ForeignEpoch` eines vollstaendig validierbaren alten Graphen;
- `NotReconstructibleOrphanedState`, wenn die Run-Persistenz nur den exakten
  waehrend dieses Handoffs geschriebenen Target-Teilzustand enthaelt.

Die Application reicht dabei nur die vom Configuration-Owner ausgegebene
private Capability weiter. Der Coordinator ist alleiniger Owner der
Entscheidung, ob alter Graph, leerer Store, exakte Targetbytes,
PreparedInterrupted, Orphan, Korruption oder Mixed-Epoch vorliegen. Ein
allgemeines `NotReconstructibleOrphanedState -> reset`-Mapping existiert
nicht. Ein Handoff mit `Committed` darf nur einen exakten vorbereiteten
Target-Teilzustand beziehungsweise den exakt gebundenen Target-Head
finalisieren; ein alter formal gueltiger Previous-Epoch-Graph bleibt auch
dann fail-closed.

Nach erfolgreichem `Consumed`-Commit laedt die Application den normalen
aktuellen Coordinatorstand. Erst dann wird `Ready` mit HWM `0` beziehungsweise
dem daraus folgenden HWM veroeffentlicht. Die erste neue ID im neuen Raum ist
`1`.

Ohne `Pending`-/`Committed`-Nachweis bleibt jeder `ForeignEpoch`-Befund
fail-closed. Ein alter oder Legacy-Head, ein nicht autorisierter leerer
Teilstore, ein abgebrochener/indeterminierter Handoff oder eine bereits
`Consumed` gebundene Autorisierung gibt keine neue ID frei.

### 6.4 Current-, NoActiveRun- und Fallback-Auswahl

Die High-Water-Auswahl folgt der kanonischen committed Head-Information und
nicht der Auswahl eines moeglicherweise aelteren Snapshots:

1. Ein gueltiger Schema-4-`Committed`-Head mit vorhandenem High Water ist
   die normale Basis fuer den Allocator. Der referenzierte Current-Snapshot
   kann `ProgramRun`, `ManualRun` oder `NoActiveRun` sein.
2. Ein vom Coordinator vollstaendig als `NoPersistedRun`/`ReadyEmpty`
   bewiesener Head- und slotfreier Store ist die einzige Headlose Ausnahme:
   die aktuelle gueltige Epoche bildet dort den neuen Raum mit HWM `0`.
3. Ein `NoActiveRun`-Snapshot veraendert den High Water nicht. Ein neuer
   Start setzt deshalb nach `commandIdHighWater + 1` fort.
4. Ist Current unbrauchbar, aber ein Fallback formal gueltig, wird der
   Allocator niemals aus dem aelteren Fallback-Fenster initialisiert. Der
   bestehende Recoverypfad darf den Fallback weiterhin nach seiner eigenen
   kanonischen Entscheidung anzeigen beziehungsweise recovern; solange kein
   gueltiger committed Head mit etabliertem High Water fuer die Epoche
   vorliegt, bleiben neue Commands und Runs `Unavailable`.
5. Ist der committed Head gueltig und traegt er einen etablierten High Water,
   bleibt dieser Wert auch bei einer Fallback-/Recoveryansicht als obere
   Identitaetsgrenze erhalten. Eine neue Mutation wird trotzdem nur nach den
   bestehenden Recovery-/Process-Zulassungen weitergereicht; die
   Fallbackauswahl darf den High Water nicht aelter machen. Der normale
   Startpfad bleibt waehrend einer nicht abgeschlossenen Recoveryansicht
   gesperrt; die reine HWM-Verfuegbarkeit ersetzt keine bestehende
   Recovery-/Standby-Freigabe.
6. Ein `FactoryResetCompleted` ohne gebundene persistente
   `Pending`-/`Committed`-/`Consumed`-Evidenz und jeder fehlende,
   Legacy-Unknown, fremde, inkonsistente oder indeterminierte Nachweis wird
   fail-closed angehalten. `Pending` und `Committed` duerfen nur den exakt
   gebundenen Handoff fortsetzen; `Consumed` bezeichnet den bereits
   abgeschlossenen Handoff und mintet keinen neuen Proof. Kein aelterer
   Fallback und kein Replayfenster darf diese Bindung ersetzen.

Der bestehende `RunPersistenceCoordinator` beziehungsweise sein vorhandener
Application-Handoff reicht dem Allocator deshalb neben dem bestehenden
`RunPersistenceLoadStatus` genau den optionalen High-Water-Wert aus dem
validierten committed Head weiter. Dieser Handoff exponiert keinen zweiten
Store und keine neue Recoveryentscheidung; er macht nur die bereits
kanonisch entschiedene Head-Identitaet fuer die Application sichtbar.

Damit kann ein spaeterer Neustart weder vom Current- auf den Fallback-
Snapshot zurueckspringen noch eine bereits relevante persistierte
Command-/Run-Identitaet erneut ausgeben. Die Kombination aus
`StorageEpoch` und dem nicht ruecklaeufigen committed High Water ist die
beweisbare Identitaetsbasis; der FIFO-Replaybestand bleibt davon getrennt.

### 6.5 Codec- und Validierungsregeln

- `kCurrentRunPersistenceSchema` wird auf 4 angehoben;
- `knownRunPersistenceSchema()` akzeptiert 1, 2, 3 und 4;
- Decode verzweigt nur fuer dieses Feld zwischen `readUint32` bei Schema <4
  und `readUint64` bei Schema >=4;
- Encode von Schema 4 verwendet den vorhandenen Big-Endian-Codec und schreibt
  den vollstaendigen 64-Bit-Wert, ohne Cast oder Trunkierung;
- Schema-4-Head-Encode/Decode schreibt beziehungsweise liest
  `commandIdHighWater` mit explizitem Presence-Tag. Schema 1 bis 3
  dekodieren dieses Feld als `Unknown`; es wird kein alter Head-Writer
  erzeugt;
- der Codec prueft am einzelnen Record nur Schema, Presence/Format,
  Wertebereich, Envelope-/Record-/Epochbindung und die bestehenden lokalen
  Head-Invarianten. Die historische HWM-Monotonie gegen einen frueheren
  Committed-Head ist ausschliesslich Aufgabe des Coordinators und seines
  Writepfads;
- Head-, Slot-, CRC-, Epoch-, Referenz-, Laengen-, Trailing-Byte- und
  Zustandsvalidierungen bleiben die bestehenden Validierungen;
- ein ungueltiger Wert, eine abgeschnittene 64-Bit-Zahl oder ein unbekanntes
  neueres Schema fuehrt zu dem bestehenden fail-closed Ergebnis;
- es gibt keine zweite Run-Persistenz, keinen Paralleldecoder und keine
  Rueckmigration in ein anderes Provenienzregister.

### 6.6 Nachweise

Die Implementation ergaenzt die bestehenden nativen Suites, statt ein neues
Testframework aufzubauen:

- `test/test_run_snapshots/test_run_snapshots.cpp`: starker 64-Bit-Typ,
  Gueltigkeitsgrenzen und die exakte Uebernahme in den Snapshot;
- `test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp`:
  Schema-4-Write, altes Decode, Snapshot-/Recoverypfad und
  Write-before-Apply;
- `test/test_issue90_product_recovery_oracle`: Legacy- und Schema-4-
  Recovery-Fallback ohne neue Katalogsemantik;
- bestehende Smoke-/UI-Command-Tests: aktualisierte Request-Typen und die
  unveraenderte Envelope-ID-Gleichheit.

Mindestens folgende feste Nachweise sind erforderlich:

1. Golden-Payloads fuer mindestens je einen aktiven Programmrun der Schemas
   1, 2 und 3 dekodieren bytegetreu den alten numerischen Wert als neutralen
   64-Bit-Wert und ergeben einen Legacy-Snapshot ohne beweisbaren High Water.
2. Ein Schema-4-Golden-Payload schreibt und liest den vollstaendigen
   64-Bit-Provenienzwert sowie den expliziten High Water ohne Veraenderung.
3. Fuer Legacy gibt es keinen Schema-1/2/3-Writer: Der Nachweis lautet
   `Legacy decode -> kanonischer In-Memory-Snapshot -> Schema-4 encode ->
   Schema-4 decode -> semantische Gleichheit der unterstuetzten Felder`.
   Der Legacy-High-Water bleibt dabei `Unknown`, bis ein neuer
   `StorageEpoch` etabliert ist.
4. Schema 4 erhaelt den eigentlichen Encode-/Decode-Roundtrip in seinem
   eigenen Wireformat; ein alter Snapshot wird bei regulaerer erneuter
   Persistenz ausschliesslich als Schema 4 geschrieben.
5. Trunkierung, Trailing Bytes, unbekanntes Schema 5 und ungueltige
   Referenzen laden fail-closed.
6. Current-, Fallback- und `NoActiveRun`-Recovery mit alten und neuen
   Records erzeugt dieselbe bestehende Run-/Recoveryentscheidung; ein
   Legacywert wird dabei nirgends als echte Katalogrevision ausgewertet und
   ein Fallback erzeugt keinen neuen High Water.

## 7. Gemeinsame Application-Command-Identitaet

### 7.1 Owner und Initialisierung

`ApplicationRunIdentity` ist eine einzelne Instanz an der
`FermentationApplication`-Grenze. Touch und spaeter Web greifen auf dieselbe
Instanz beziehungsweise denselben bestehenden Application-Aufrufpfad zu.
Die UI besitzt keinen Counter.

Die Initialisierung erfolgt erst, wenn beide bestehenden Grundlagen
vertrauenswuerdig vorliegen:

1. der Configuration-Recoverypfad liefert einen gueltigen
   `RuntimeConfigurationSnapshot` mit gueltigem `StorageEpoch`;
2. der vorhandene `RunPersistenceCoordinator` hat den kanonischen
   committed Head beziehungsweise den vollstaendig bewiesenen
   `NoPersistedRun`-Leerstand und den aktuellen beziehungsweise bestehenden
   Recoverystatus geladen.

Aus dem kanonischen committed Head wird die sichere High-Water-Basis
bestimmt:

- ein etablierter Schema-4-Head liefert seinen explizit persistierten
  `commandIdHighWater`; `0` ist der gueltige Anfangswert eines neuen
  Identitaetsraums;
- das bestehende `persistedRunCommandIds`-Fenster wird nur fuer Replay-
  beziehungsweise AlreadyProcessed-Semantik verwendet und niemals fuer die
  High-Water-Berechnung;
- ein gueltiger Schema-4-`NoActiveRun`-Head behaelt denselben High Water; die
  erste neue ID eines leeren neuen Raums ist `1`, ansonsten folgt sie auf
  `commandIdHighWater`;
- `NoPersistedRun`/`ReadyEmpty` bei Head- und slotfreiem Store ist ein
  vertrauenswuerdiger leerer neuer Raum mit logischem HWM `0`; er benoetigt
  keinen vorab persistierten Head und vergibt die erste ID `1`;
- ein gueltiger Fallback wird fuer Recovery nach bestehenden Regeln gelesen,
  aber nicht als High-Water-Quelle verwendet. Der committed Head bleibt die
  obere Identitaetsgrenze;
- `ReadFailed`, `UnsupportedSchema`, `ForeignEpoch`, ein nicht etablierter
  Legacy-High-Water, ein inkonsistenter Head, indeterminierte Persistenz oder
  Safe-Boot ohne vertrauenswuerdigen Head initialisieren den Allocator nicht.
  Ein fehlender Head ist nur dann zulaessig, wenn der Coordinator
  ausschliesslich `NoPersistedRun`/`ReadyEmpty` bewiesen hat oder der
  autorisierte Epoch-Handoff den persistierten `Consumed`-Zustand erreicht
  hat. Bei `Pending`/`Committed` wird der Handoff zuerst fortgesetzt; neue
  Fachrequests und neue Runs liefern sonst typisiert
  `Unavailable`/fail-closed.

Es wird kein zweiter persistierter Zaehler eingefuehrt. Das neue Feld ist die
einzige High-Water-Information im bereits vorhandenen Head-/Commitvertrag.
Bei jedem erfolgreichen eligible Command-Commit sowie bei jedem anderen
erfolgreichen Head-Commit wird der HWM-Kandidat zusammen mit dem Head atomar
geschrieben; eine nur reservierte ID, deren owning Persistenz nie erfolgreich
war, wird nicht als ausgefuehrte Mutation dargestellt. Sobald eine
Command-ID im bestehenden owning Commit relevant geworden ist, ist sie im
committed High Water enthalten und kann nach Neustart nicht erneut
ausgegeben werden, auch wenn sie spaeter aus dem 32er-Replayfenster faellt.

Diese Garantie bezieht sich bewusst auf die bestehende eligible Run-
Persistenz- und Replaysemantik. Nicht eligible Message-/Komfortkommandos
haben nach `docs/RUN_COMMANDS.md` keine sitzungsuebergreifende Replaygarantie;
#144 erweitert dafuer weder das Persistenzfenster noch fuehrt es eine zweite
Historie ein.

### 7.2 Allokationsergebnis und Grenzen

Der Allocator liefert einen kleinen typisierten lokalen Outcome, zum Beispiel
mit den Zustandswerten `Allocated`, `NotInitialized`, `InvalidPersistedBase`,
`LegacyHighWaterUnknown`, `CurrentStateNotCanonical`, `Overflow` und
`Unavailable`. Dies ist kein neuer `CommandStatus`, kein
zweiter Replayvertrag und keine Fachentscheidung.

Bei `allocate()` gilt:

- `next = highWater + 1` wird vor der Ausgabe checked;
- `0` bleibt ungueltig;
- `UINT64_MAX` als High Water fuehrt zu `Overflow` ohne Ausgabe;
- eine ausgegebene ID wird im RAM als naechste Basis vorgemerkt und innerhalb
  dieses Boots nicht recycelt, auch wenn die Vorpruefung oder der Benutzer
  spaeter abbricht;
- der Allocator liest keinen High Water aus dem Fallback-Snapshot, dem
  `persistedRunCommandIds`-Fenster oder einem Legacy-32-Bit-Wert;
- ein bestehender bestaetigungspflichtiger Request besitzt nur eine
  Reservierung; seine Wiederholung ruft `allocate()` nicht nochmals auf;
- ein uninitialisierter oder untrusted Allocator blockiert den Aufbau des
  Fachrequests vor jeder Mutation.

Eine normale neue Fachanfrage erhaelt genau einmal:

    UiRequestId.value == CommandEnvelope::id == allocated CommandId

Touch und Web erhalten nicht zwei identische IDs fuer verschiedene Requests.
Vielmehr verwenden beide denselben Application-Allocator: verschiedene neu
erzeugte Fachrequests erhalten verschiedene fortlaufende IDs. Ein
Confirmation-Replay desselben Requests verwendet exakt dieselbe ID und
dasselbe `CommandEnvelope`.

Die App setzt Commandquelle, monotone Zeit, erwartete Zustands-/Run-/Message-
Revisionen und die aktuelle owning Evidenz beim Requestaufbau. Ein Adapter
darf davon keine Identitaet ersetzen oder vorgeben.

## 8. Application-erzeugte Lauf-ID fuer alle neuen Runs

### 8.1 Kanonische Ableitung

Jede Erzeugung eines neuen aktiven Runs verwendet denselben bereits validierten
`StorageEpoch` und die Command-ID des Fachrequests, der diesen neuen Run
erzeugt:

    runId = "e" + decimal(StorageEpoch.value())
            + "-c" + decimal(StartCommandId)

Die maximale Dezimaldarstellung beider `uint64_t`-Werte hat je 20 Stellen.
Das Format hat damit maximal `1 + 20 + 2 + 20 = 43` Bytes und bleibt unter
dem bestehenden 1..48-Byte-Limit. Epoch und Command-ID muessen gueltig und
von null verschieden sein; jede Format-/Laengenverletzung ist ein typisiertes
`Unavailable`/fail-closed Ergebnis. Es gibt keine UUID, keinen Zaehler, keine
zweite Cooling-ID und keinen UI-Fallback.

### 8.2 Vier vollstaendige Erzeugungspfade

| Fachpfad | UI-/Web-Eingabe | Application-Aufbau | Owning Weitergabe |
|---|---|---|---|
| `StartProgram` | Programm-ID, erwartete Katalogrevision und erlaubte Startwerte | Command-ID allokieren, `runId` ableiten, Dokument aufloesen, neutrale Provenienz setzen, aktuellen Command-/Zeit-/Evidenzkontext ergaenzen | bestehender Program-Start-/Orchestratorpfad |
| `StartManualHolding` | nur die editierbaren Manual-Holding-Werte | Command-ID allokieren, `runId` ableiten, bestehendes `ManualStartRequest` vervollstaendigen | bestehender Manual-Holding-Startpfad |
| `AbortAndCool` | Stopoption und nur editierbare Cooling-/Manual-Werte | fuer jedes neue Stop-Request genau eine Command-ID allokieren; nur im Cooling-Zweig daraus `runId` ableiten und den bestehenden `StopRequest::coolingPlan` vervollstaendigen | bestehender Stop-/Cooling-Applypfad |
| `CoolAfterCompletion` | Abschlussoption und nur editierbare Cooling-/Manual-Werte | fuer jedes neue Completion-Request genau eine Command-ID allokieren; nur im Cooling-Zweig daraus `runId` ableiten und den bestehenden `CompletionRequest::coolingPlan` vervollstaendigen | bestehender Completion-/Cooling-Applypfad |

Wird bei Stop oder Completion kein neuer Cooling-Run angefordert, wird die
neue Stop-/Completion-Command-ID trotzdem genau einmal allokiert und fuer das
Fachrequest verwendet; lediglich `runId` und `coolingPlan` fehlen. Wird ein
neuer Run angefordert, ist dieselbe bereits allokierte Stop-/Completion-
Command-ID zugleich dessen `StartCommandId` fuer die kanonische Ableitung.
Innerhalb des Cooling-Zweigs findet kein zweiter `allocate()`-Aufruf statt.

Die bestehende owning `ManualRunPlanRequest` darf im Domain-/Persistence-
Request weiterhin ihr notwendiges `runId` tragen. An der UI-/Adaptergrenze
wird dafuer ein kleiner identitaetsfreier Wertebehaelter verwendet, der nur
die vorhandenen editierbaren Manual-/Cooling-Werte traegt und genau einmal an
der Application-Grenze in `ManualRunPlanRequest` ueberfuehrt wird. Dies ist
kein zweites Programmmodell, kein Formularframework und kein neuer
Persistencevertrag. `ProgramStartRequest::runId` sowie die Cooling-
`runId`-Felder werden ebenfalls ausschliesslich von dieser Grenze gesetzt.

Damit tragen `FermentationUiStartProgramIntent`,
`FermentationUiStartManualHoldingIntent`, Stop- und Completion-Intents weder
`runId` noch `CommandId`. Ein frei geliefertes `ProgramDocument` oder ein
vollstaendiger frei gelieferter `ManualRunPlanRequest` ist kein gueltiger
UI-Startvertrag.

### 8.3 Bestaetigung und Replay

Bei einem bestaetigungspflichtigen Request werden Envelope, Command-ID und
der daraus bereits abgeleitete vorbereitete `runId` an der Application-Grenze
zusammengehalten. `confirmed == false` kann eine reine Bestaetigungsantwort
liefern, erzeugt aber keine neue ID bei der spaeteren Bestaetigung. Ein Replay
mit derselben Command-ID verwendet genau denselben vorbereiteten Request.

Eine abgelehnte oder abgebrochene Vorbereitung verbraucht die reservierte
ID innerhalb des Boots; sie wird nicht auf eine andere Fachanfrage
uebertragen. Eine erneute Benutzeraktion ist ein neues Fachrequest und
erhaelt eine neue ID. Ein bereits owning persistiertes Request darf durch
Replay weder einen zweiten Run noch eine zweite Nebenwirkung erzeugen.

## 9. Konkrete Integrationsarbeiten nach Planfreigabe

Die spaetere Umsetzung erfolgt in diesem engen Scope:

1. `RunProgramSourceRevision` in den bestehenden Run-Snapshot- und
   Startvertraegen verankern. Alle impliziten `uint32_t`-Annahmen und
   verlustbehafteten Casts entfernen.
2. Den bestehenden Run-Codec auf Schema 4 erweitern und die vorhandenen
   Head-/Slot-/Recoveryvalidierungen unveraendert weiterverwenden. Das
   `commandIdHighWater` wird im bestehenden Head-Commit atomar mitgefuehrt;
   Schema-1/2/3 erhalten keinen neuen Writer. Der Codec behauptet keine
   historische Monotonie, die nur der Coordinator beweisen kann.
3. Den bestehenden Configuration-Bootstrap-Codec und -Store um die kleine
   gebundene `Pending`/`Committed`/`Consumed`-Handoff-Evidenz ergaenzen.
   Schema 1 bleibt mit seiner 5-Byte-Payload und geschlossenen Historien-
   relation lesbar, Schema 2 bekommt die geschlossene Phasen-/Sequence-
   relation, Cross-Schema-Successormatrix und das exakt 6/22-Byte-Payload-
   format. Alte Records ohne Bindung minten keinen Proof. Der
   ConfigurationRecoveryService gibt nur private, phasenbezogene
   Proof-/Fortsetzungscapabilities aus und persistiert `Consumed` erst nach
   bestaetigtem Run-Head.
4. Den bestehenden `RunPersistenceCoordinator` um den schmalen, phasengeteilten
   Handoff-Pfad ergaenzen. Die Application ruft ihn nur mit der Configuration-
   Capability auf; der Pfad validiert den alten Graphen, den leeren Store oder
   den exakten Target-Teilzustand, schreibt die beiden neuen Schema-4-
   `NoActiveRun`-Records und meldet erst danach `Pending -> Committed` an den
   Configuration-Owner. Anschliessend wird der exakt vorbereitete
   Committed-Head mit HWM `0` geschrieben. Jede Unsicherheit bleibt
   fail-closed. Ein wirklich leerer `NoPersistedRun`-Store darf ohne
   Vorab-Head logisch `ReadyEmpty` bilden, muss bei offenem Reset-Handoff aber
   dennoch den gebundenen Handoff vollstaendig konsumieren.
5. `ApplicationRunIdentity` an der bestehenden Application-Grenze instanzieren
   und mit dem validierten committed Head, dem leeren Store-Urteil oder dem
   erfolgreichen Epoch-Handoff, dem `StorageEpoch` und dem bestehenden
   Recoverystatus verbinden. Das `persistedRunCommandIds`-Fenster bleibt
   ausschliesslich Replayzustand.
6. Den vorhandenen UI-/Application-Requestaufbau so schliessen, dass
   `UiRequestId` vom Allocator stammt und die Envelope-ID exakt gespiegelt
   wird. Die bestehenden `decide*`-, Persistenz- und Lifecyclepfade bleiben
   die einzigen owning Pfade.
7. Den app-owned Run-ID-Aufbau fuer alle vier neuen Runpfade in die
   bestehenden Program-, Manual-, Stop- und Completion-Requests integrieren.
   Stop und Completion allokieren auch ohne Cooling genau einmal; der
   Cooling-Zweig verwendet dieselbe Command-ID als `StartCommandId`.
8. Die identitaetsfreien Adapterwerte fuer Manual-/Cooling-Eingaben auf den
   bestehenden owning Request abbilden, ohne einen zweiten Plan- oder
   Commandvertrag einzufuehren.
9. Die gezielten nativen Regressionen aus Abschnitt 10 ausfuehren und alle
   neuen/angepassten Dateien gegen den exakten Planumfang pruefen.

Die konkrete Persistenzmutation bleibt beim bestehenden
`TemperatureControlApplicationOrchestrator` und
`RunPersistenceCoordinator`. Der Identity-Scope darf diese Owner nur mit
den benoetigten Typ- und Inputgrenzen integrieren, nicht deren Fachlogik,
Safetylogik, Recoverylogik oder Lifecycle-Handoff neu implementieren.

## 10. Akzeptanz- und Regressionsmatrix

Alle Tests laufen im bestehenden nativen Testprofil. In diesem Plan werden
keine Tests ausgefuehrt; nach Ownerfreigabe sind mindestens diese Faelle
beobachtbar nachzuweisen:

| ID | Nachweis | Erwartetes Ergebnis |
|---|---|---|
| RP-01 | neuer Programmstart aus validiertem Katalogstand `P` | `sourceProgramRevision` ist neutraler 64-Bit-Wert mit exakt `P.value()`; vollstaendiger `ProgramDocument` bleibt unveraendert im Run-Snapshot |
| RP-02 | stale UI-Katalogrevision vor Aufloesung/Start | typisierter Stale-/Conflict-Ausgang; keine Startmutation und keine Run-Provenienz aus einem fremden Stand |
| RP-03 | Katalogrevision `UINT64_MAX` beziehungsweise null/invalid | keine implizite Konvertierung; gueltige/invaliden Grenzen werden fail-closed behandelt |
| RP-04 | Schema-1-, Schema-2- und Schema-3-Golden mit freiem Legacy-`uint32_t` | Decode liefert numerisch denselben neutralen 64-Bit-Wert; keine `ProgramCatalogRevision`-Behauptung |
| RP-05 | Schema-4-Golden mit hohem 64-Bit-Wert | Encode/Decode ist verlustfrei; kein Cast, keine Trunkierung |
| RP-06 | Legacy decode -> kanonischer In-Memory-Snapshot -> Schema-4 encode -> Schema-4 decode | semantische Gleichheit der unterstuetzten Felder; kein Schema-1/2/3-Writer und Legacy-High-Water bleibt `Unknown` |
| RP-06b | Schema-4 encode -> Schema-4 decode im eigenen Wireformat | Dokument, Source-Kind, neutrale Provenienz, `commandIdHighWater` und Recoverydaten bleiben erhalten |
| RP-07 | unbekanntes Schema 5, abgeschnittenes Feld, Trailing Bytes, falsche Referenz/Epoch | bestehender Loadpfad bleibt fail-closed; kein Run und kein Allocator wird freigegeben |
| RP-08 | Current-, Fallback- und `NoActiveRun`-Recovery mit Legacy und Schema 4 | bestehende Recoveryklassifikation bleibt gleich; High Water kommt nur aus dem committed Head, nie aus dem aelteren Fallbackfenster |
| BOOT-01 | Schema-1-Goldens und Schema-1-Einzel-/Zwei-Slot-Historie | 5-Byte-Payloads bleiben bytegenau lesbar; die alte `2*E`-/`2*E-1`-Relation bleibt unveraendert; kein Schema-1-Writer entsteht |
| BOOT-02 | Schema-2-Goldens fuer `None`, `Pending`, `Committed` und `Consumed` | exakt 6 beziehungsweise 22 Payloadbytes, 43 beziehungsweise 59 Gesamtbytes, checked Sequence-/Epoch-/Handoff-Plausibilitaet |
| BOOT-03 | Schema-1 `Initializing`, `Initialized` und `Resetting` als erste Schema-2-Successoren | nur die drei definierten Cross-Schema-Zeilen mit `S+1` werden akzeptiert; alle anderen Mischhistorien bleiben `IntegrityFailure` |
| BOOT-04 | Schema-2 `Initializing/None -> Initialized/None -> Resetting/None -> Pending -> Committed -> Consumed` ueber alternierende `cb0`/`cb1`-Writes | jeder Successor ist versions- und zustandsgenau, Sequence steigt checked exakt um eins |
| BOOT-05 | `Consumed -> naechster Reset -> neues Pending` | neue `Resetting/None`-Phase traegt die neue Epoche und keine alte Bindung; danach entsteht ein neues gebundenes `Pending` |
| BOOT-06 | Resetversuch waehrend `Pending` oder `Committed` | vor jedem Write typisiert abgelehnt; keine neue Epoche und keine neue Handoff-Autorisierung |
| BOOT-07 | unzulaessige Cross-Schema-, Sequence-, State- oder Epochkombination | Zwei-Slot-Scan ordnet nichts still um und liefert `IntegrityFailure`/fail-closed |
| BOOT-08 | Sequence-/Epoch-Overflow an allen `2*E`, `E+1` und `S+1`-Stellen | checked Ablehnung vor Write; kein Wraparound und kein gueltiger Nachfolger |
| BOOT-09 | unbekanntes Bootstrap-Schema 3, abgeschnittene/zusaetzliche Payload, UTC-Tag oder falsche Payloadlaenge | `UnsupportedNewerSchema` beziehungsweise `InvalidModel`; keine partielle Modellrueckgabe und keine Mutation |
| BOOT-10 | beide Slots: neuester `Consumed`, aelterer `Committed`; Verlust/Unlesbarkeit des neuesten Slots | aelterer Record ist nur exakte gebundene Fortsetzung, niemals freie Resetautorisierung; bei unlesbarem/mixed Scan fail-closed |
| BOOT-11 | `Committed` mit altem Previous-Epoch-Graph oder beliebigem Target-Head | keine Finalisierung; nur der exakt vorbereitete Target-Head darf fortgesetzt werden |
| ID-01 | fabrikneuer Head- und slotfreier Store, Coordinator-Ergebnis `NoPersistedRun`/`ReadyEmpty` | gueltige aktuelle Epoche bildet logisch HWM `0`; keine Vorabpersistenz; erste ID ist `1` |
| ID-02 | erster erfolgreicher eligible Command-Commit aus `ReadyEmpty` | Prepared und Committed tragen den Kandidaten; der erfolgreich geladene Schema-4-Head traegt committed HWM `1` |
| ID-03 | Neustart nach ID-01/ID-02 | Committed-HWM `1` ist Neustartbasis; naechste ID ist `2` |
| ID-04 | Legacy-Head derselben Epoche mit frueher hoeherer, aus dem 32er-Fenster verdraengter ID | Legacy-HWM bleibt `Unknown`; keine neue ID in derselben Epoche und keine Wiederverwendung |
| ID-05 | mehr als 32 eligible Commands mit dazwischenliegenden periodischen Checkpoints und Neustart | der committed HWM bleibt die einzige Basis; keine Wiederverwendung einer verdraengten ID |
| ID-06 | gueltiger Schema-4-Head mit `NoActiveRun` nach vorherigem owning Run-Commit | Transition zu `NoActiveRun` setzt den HWM nicht zurueck; neue ID ist HWM + 1 |
| ID-07 | Current unbrauchbar, aelterer Fallback formal gueltig | Allocator springt nicht auf das Fallbackfenster zurueck; nur committed Head-HWM oder `Unavailable` |
| ID-08 | Current unbrauchbar, Fallback gueltig, aber kein gueltiger committed HWM | Fallback bleibt passiv beziehungsweise Recovery-pending; neue ID und neuer Run bleiben unavailable |
| ID-09 | eligible Command hebt den HWM an | Head-Konstruktion enthaelt mindestens die betreffende `CommandId`; kein niedrigerer Committed-HWM |
| ID-10 | Transition-Commit ohne neue CommandId | bisheriger committed HWM wird unveraendert in Prepared/Committed uebernommen |
| ID-11 | Sensor-Commit ohne neue CommandId | bisheriger committed HWM wird unveraendert uebernommen |
| ID-12 | Recovery-/Fallback-Commit ohne neue CommandId | bisheriger committed HWM wird unveraendert uebernommen |
| ID-13 | periodischer Direkt-Checkpoint ohne neue CommandId | direkter `CommittedHead` uebernimmt den bisherigen HWM; kein Reset auf `0` und keine Fensterberechnung |
| ID-14 | Fehler vor Slot-Write, Fehler nach Prepared, Fehler beim Committed-Write und `CommitOutcomeUnknown` | letzter Committed-HWM bleibt autoritativ; Prepared/Teilzustand vergibt keine neue ID und blockiert fail-closed |
| ID-15 | `PreparedInterrupted` beim Neustart | Prepared-HWM wird nicht als Allocatorbasis verwendet; keine ID-Ausgabe |
| ID-16 | High Water `UINT64_MAX` | typisiertes `Overflow`; kein Wraparound und keine ID `0` |
| EPOCH-01 | autorisierter Factory-Reset mit vollstaendigem alten Committed-Graphen in `previousEpoch` | Application uebergibt `FactoryResetCompleted`; Handoff ueberschreibt beide Slots und Committed-Head der neuen Epoche mit HWM `0`; danach erste ID `1` |
| EPOCH-02 | Slot-/Head-Write des Handoffs sicher fehlgeschlagen oder indeterminiert | neuer Epochraum bleibt unavailable; kein Allocator und keine teilweise als leer behandelte Foreign-Epoch |
| EPOCH-03 | abgebrochener/indeterminierter Epochwechsel beim naechsten Boot | ohne vollstaendigen autorisierten Handoff bleibt `ForeignEpoch`/Prepared/Mixed-State fail-closed |
| EPOCH-04 | fremde Epoche ohne passenden autorisierten `FactoryResetCompleted`-Nachweis | nicht als leer akzeptiert; keine neue ID und kein neuer Run |
| EPOCH-05 | Configuration-Reset erfolgreich, Stromausfall vor dem ersten Run-Write | persistierter `Pending`-Marker rekonstruiert exakt dieselbe Epoche; kein Proof aus der letzten Manifestoperation allein |
| EPOCH-06 | Stromausfall nach Slot 0 oder Slot 1, aber vor `Pending -> Committed` | `Pending` bleibt offen; nur der exakte Target-Teilzustand wird fortgesetzt, kein beliebiger Orphan wird geloescht |
| EPOCH-07 | beide Run-Slots sicher geschrieben, `Pending -> Committed`, danach Stromausfall vor dem neuen Head | `Committed` sperrt eine neue Autorisierung; der exakt vorbereitete Target-Head darf fertiggeschrieben werden |
| EPOCH-08 | neuer Head sicher geschrieben, Stromausfall vor `Committed -> Consumed` | `Committed` wird gegen den exakten Target-Head bestaetigt und danach dauerhaft `Consumed`; keine zweite Handoff-Mutation |
| EPOCH-09 | nach `Consumed` HWM/Run-Identitaet der neuen Epoche und spaeter formal gueltiger Previous-Epoch-Graph | kein Proof und kein Handoff; `ForeignEpoch`/Rollback bleibt fail-closed, HWM und `e<epoch>-c<id>` werden nicht wiederverwendet |
| EPOCH-10 | `Pending`/`Committed`-Marker, Run-Head oder Slot-Read/Write indeterminiert beziehungsweise gemischt | Readback muss die gebundene Phase und den exakten Graphen bestaetigen; andernfalls unavailable/fail-closed |
| EPOCH-11 | vollstaendig leerer alter Run-Store, Reset offen, erster neuer Slot vorhanden | normaler Loader darf `NotReconstructibleOrphanedState` liefern; Application delegiert nur mit Capability, Coordinator beendet den exakten Handoff bei HWM `0`, erste ID `1` |
| RUN-01 | `StartProgram` | UI liefert keine Identitaet; Application erzeugt Command-ID und `e<epoch>-c<id>` und ruft bestehenden Startpfad auf |
| RUN-02 | `StartManualHolding` | nur vorhandene Manual-Holding-Werte kommen vom UI; Application erzeugt `runId`; owning Vertrag bleibt ManualHolding |
| RUN-03 | `AbortAndCool` mit neuem Cooling-Run | ein neues Stop-Request allokiert genau eine Command-ID; Cooling-`runId` nutzt exakt diese ID als `StartCommandId`; kein zweiter Allocate-Aufruf |
| RUN-04 | `CoolAfterCompletion` mit neuem Cooling-Run | ein neues Completion-Request allokiert genau eine Command-ID; Cooling-`runId` nutzt exakt diese ID; keine zweite Cooling-ID-Logik |
| RUN-05 | Stop/Completion ohne neuen Cooling-Run | neues Stop-/Completion-Request besitzt genau eine Command-ID, aber keine `runId` und keinen `coolingPlan` |
| RUN-06 | alle vier Pfade mit maximalen Epoch-/ID-Dezimalwerten | Run-ID bleibt innerhalb 48 Bytes; ungueltige Basis/Laenge wird unavailable |
| RUN-07 | spaetere Confirmation oder Replay eines Cooling-Requests | dieselbe Command-ID und derselbe vorbereitete Cooling-`runId`; keine zweite Nebenwirkung |
| RUN-08 | neuer Programmstart nach Neustart mit vorherigem Run `e<epoch>-c<id>` und zwischenzeitlich verdraengtem Replayfenster | committed Head-HWM liefert `id + 1`; der neue Run ist `e<epoch>-c<id+1>` und der alte Run-Identifier wird nicht wieder ausgegeben |
| CMD-01 | zwei neue Requests ueber denselben Application-Aufrufpfad | verschiedene fortlaufende IDs; `UiRequestId.value == CommandEnvelope::id` je Request |
| CMD-02 | Touch und Web als zwei Quellen | beide verwenden denselben Allocator; keine Quellenprioritaet, keine getrennten Zaehler |
| CMD-03 | unbestaetigtes bestaetigungspflichtiges Request und Confirmation-Replay | erste Entscheidung und Replay behalten exakt dieselbe Command-ID und denselben vorbereiteten Request; kein zweiter Allocatoraufruf |
| CMD-04 | Stop/Completion-Confirmation mit und ohne Cooling | pro Fachrequest genau ein Allocate-Aufruf; Replay verwendet dieselbe Command-ID und vorbereitete `runId` |
| CMD-05 | frei erfundener UI-Korrelationswert oder zurueckgereichter Wert ohne Application-Vorbereitung | Wert kann keine neue Command-ID bestimmen; nur Application-owned Echo eines vorbereiteten Requests wird akzeptiert |
| CMD-06 | Pre-Apply-Abbruch einer reservierten ID, danach neue Anfrage im selben Boot | neue Anfrage recycelt die ID nicht |
| OWN-01 | Startentscheid und anschliessender Apply | `decide*` bleibt reine Entscheidung; owning Persistenz geht ausschliesslich ueber den vorhandenen Application-/Orchestratorpfad |
| OWN-02 | Persistenzfehler nach Proposed/Startvorbereitung | kein RAM-/Run-Apply und keine neue Aktorfreigabe; Identitaetsreservierung wird nicht als ausgefuehrte Mutation dargestellt |
| OWN-03 | statische Scopepruefung | kein neuer Dispatcher, Command-Bus, Persistence-Coordinator, Recovery-/Safety-/Lifecyclepfad oder UI-/Renderer-Scope |

Die bestehende #25-Phasensemantik bleibt dabei unveraendert: eine reine
Entscheidung oder Vorpruefung ist `DecisionOnly`; `OwningOutcome` entsteht
erst beim tatsaechlich ausgefuehrten bestehenden Apply-/Persistenz-/Commit-
Pfad. #144 fuehrt dafuer keine zweite Ergebnisfamilie ein.

## 11. Geplanter Dateiscope

### Bestehende Dateien mit notwendiger Vertragsanpassung

- `lib/fermentation_app/src/run_snapshot.hpp`
- `lib/fermentation_app/src/run_snapshot.cpp`
- `lib/fermentation_app/src/run_commands.hpp`
- `lib/fermentation_app/src/fermentation_ui_commands.hpp`
- `lib/fermentation_app/src/fermentation_ui_commands.cpp`
- `lib/fermentation_app/src/fermentation_application.hpp`
- `lib/fermentation_app/src/fermentation_application.cpp`
- `lib/fermentation_app/src/run_persistence_codec.hpp`
- `lib/fermentation_app/src/run_persistence_codec.cpp`
- `lib/fermentation_app/src/run_persistence_contract.hpp`
- `lib/fermentation_app/src/run_persistence_contract.cpp`
- `lib/fermentation_app/src/run_persistence_coordinator.hpp`
- `lib/fermentation_app/src/run_persistence_coordinator.cpp`, nur soweit
  fuer den bestehenden Schema-/High-Water-Handoff und den schmalen
  autorisierten Epoch-Handoff erforderlich
- `lib/fermentation_app/src/configuration_bootstrap.hpp`
- `lib/fermentation_app/src/configuration_bootstrap.cpp`
- `lib/fermentation_app/src/configuration_limits.hpp`
- `lib/fermentation_app/src/configuration_bootstrap_codec.hpp`
- `lib/fermentation_app/src/configuration_bootstrap_codec.cpp`
- `lib/fermentation_app/src/configuration_bootstrap_store.hpp`
- `lib/fermentation_app/src/configuration_bootstrap_store.cpp`
- `lib/fermentation_app/src/configuration_recovery_service.hpp`
- `lib/fermentation_app/src/configuration_recovery_service.cpp`, nur fuer
  die persistente Handoff-Phasenbindung und ihre autorisierte Konsumierung
- `lib/fermentation_app/src/actuation_interlock.cpp`, nur fuer die bereits
  bestehende Projektion des Configuration-Recovery-Status
- `lib/fermentation_app/src/fermentation_ui_models.hpp`, nur fuer
  `expectedProgramCatalogRevision`
- `lib/fermentation_app/src/fermentation_ui_models.cpp`, nur fuer die
  semantische Snapshot-Gleichheit und den bestehenden Refresh-Tracker
- `main/issue_29_bringup_probe.cpp`, nur fuer die Strong-Type-Kompatibilitaet
  von `RunProgramSourceRevision`

`ConfigurationRecoveryService` bleibt der Owner fuer die kanonische
FactoryReset-Autorisierung, die persistente `Pending`/`Committed`/`Consumed`-
Evidenz und die gebundenen privaten Proof-/Fortsetzungscapabilities. Der
Handoff wird nicht als zweiter Resetpfad in diesem Service implementiert,
sondern von der bereits uebergeordneten `FermentationApplication` mit dem
bestehenden `RunPersistenceCoordinator` komponiert.

Der `AuthorizedRunEpochHandoffProof` wird als kleiner Typ im bestehenden
Run-Persistence-Vertragsbereich verankert. Er enthaelt ausschliesslich alte
und neue `StorageEpoch`; eine generische Reset-, Token- oder
Provenienzregistry entsteht nicht.

### Kleiner neuer Identity-Baustein

- `lib/fermentation_app/src/application_run_identity.hpp`
- `lib/fermentation_app/src/application_run_identity.cpp`

Der Baustein bleibt auf `fermentation_app` beschraenkt und benoetigt keine
ESP-IDF- oder device_platform-Produktionsabhaengigkeit ausser den bereits
verwendeten starken Plattformtypen. Falls der bestehende Build die
`SRC_DIRS`-Aufnahme nutzt, ist kein paralleles Buildsystem erforderlich.

### Bestehende Tests mit gezielter Erweiterung

- `test/test_run_snapshots/test_run_snapshots.cpp`
- `test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp`
- `test/test_configuration_bootstrap_codec/test_configuration_bootstrap_codec.cpp`
- `test/test_configuration_bootstrap_store/test_configuration_bootstrap_store.cpp`
- `test/test_fermentation_ui_commands/test_fermentation_ui_commands.cpp`
- `test/test_issue90_product_recovery_oracle/...` fuer den bestehenden
  Application-/Factory-Reset-/Recovery-Handoff, soweit dieser Harness den
  Pfad bereits abbildet
- ein kleiner nativer Identity-Test unter
  `test/test_issue144_run_identity/`, nur falls die vorhandene Teststruktur
  fuer den Application-Allocator keinen passenden bestehenden Einstieg
  besitzt; kein neues Testframework
- `test/test_configuration_recovery_service/test_configuration_recovery_service.cpp`
  fuer die persistente Handoff-Phasenfolge und die Rekonstruktion nach
  Reset-/Boot-Cuts;
- `test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp`
  fuer exakte Pending-/Committed-Fortsetzung, leeren partiellen Store und
  die fail-closed-Abgrenzung gegen einen spaeteren Previous-Epoch-Graphen
- `test/test_fermentation_ui_models/test_fermentation_ui_models.cpp` fuer
  die semantische Gleichheit von `expectedProgramCatalogRevision` und den
  daraus folgenden Refresh-Revisionsnachweis

Keine Firmware-, Hardware-, Display-, Touch- oder Netzwerkdatei ausserhalb
dieses schmalen Identity-/Run-/Configuration-Contractumfangs wird in #144
benoetigt. Die genannten bestehenden Interlock-, UI-Modell- und Bring-up-
Dateien sind nur wegen bereits nachgewiesener Folgeanpassungen enthalten;
ihre Fach- und Hardwaregrenzen werden nicht erweitert. `docs/ROADMAP.md` ist
bereits im ersten PR-Commit synchronisiert; weitere Roadmap-Fachanforderungen
werden hier nicht kopiert.

## 12. Downstream-Vertrag fuer Issue #26

Nach dem Merge von #144 muss PR #143 seinen Plan und seine Branchbasis auf den
exakten #144-Merge-HEAD aktualisieren. #26 konsumiert dann nur:

- `RunProgramSourceRevision` als neutralen 64-Bit-Run-Provenienztyp;
- Schema-4-Writes sowie das Lesen der Schemas 1 bis 3 ohne nachtraegliche
  Legacy-Katalogsemantik;
- die echte `ProgramCatalogRevision` als unabhaengige UI-/Editor-Stale-Grenze;
- den gemeinsamen Application-Allocator und
  `UiRequestId.value == CommandEnvelope::id`;
- die Application-Erzeugung der `runId` fuer Programm-, ManualHolding-,
  AbortAndCool- und CoolAfterCompletion-Runs;
- die Garantie, dass UI-/Workspace-Payloads keine Command- oder Lauf-ID
  einschleusen;
- die bestehenden Application-/Orchestrator-/Persistence-Owner fuer
  Entscheidung, Apply, Persistenz und Lifecycle-Handoff.

Der #26-Plan dupliziert weder Schemaaenderung, Golden-/Recoverytests,
Allocatorimplementation noch die Run-Request-Komposition. Seine lokale
Simulation darf die fertig vorhandene Application-Grenze aufrufen, aber
keinen Ersatzdispatcher oder Persistenzpfad bauen. Bis #144 gemergt ist, bleibt
#26 gemaess Roadmap blockiert und seine Implementation `NOT_STARTED`.

## 13. Ergebnis und Freigabepunkt

Dieser PR liefert zunaechst nur:

1. den Roadmap-Sync in `ROADMAP_COMMIT`;
2. diesen versionierten, vollstaendigen #144-Plan.

Der aktuelle Planstatus bleibt `IMPLEMENTATION=NOT_STARTED`; Tests und
ESP-IDF-/Hardwarelaeufe sind `NOT_RUN`. Nach Ownerfreigabe des exakten
Plan-Commits darf der enge Identity-/Codecscope umgesetzt und danach separat
reviewt werden. Vor dieser Freigabe werden keine Produktionsdateien und keine
Tests ausgefuehrt; Issue-/PR-Metadaten duerfen fuer die exakte Plan- und
Handover-Provenienz synchronisiert werden. Issue-/PR-Status, Merge und
Issue-Schluss bleiben beim Owner.
