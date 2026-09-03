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
    BASE_BRANCH=integration/r1-development
    BASE_SHA=f5aca945c3009408c091a8f03b000e8309af6bcf
    ROADMAP_COMMIT=3ecf9ad9edc223c7af731600d54a857d5e2f8c9f
    PLAN_PATH=docs/tasks/issue-144-run-identity-neutral-provenance-plan.md
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

Die Baseline ist der live verifizierte Merge-HEAD von PR #146. Die Roadmap
wurde in diesem PR als erster Commit auf Issue #144 als aktuellen
fachlichen Pflichtvorgaenger und auf die Blockierung von #26 synchronisiert.
Der Plan ist der zweite inhaltliche Commit dieses PR.

Vor dieser Planerstellung wurden ausserdem live verifiziert:

- Issue #144 ist offen und traegt den Run-Identity-/Provenienzscope;
- PR #147 ist offen und Draft auf `integration/r1-development`;
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
- `RunPersistenceSnapshot::persistedRunCommandIds` ist das bestehende
  begrenzte, kanonisch persistierte Fenster fuer eligible Run-Command-IDs;
  der aktuelle `RunPersistenceCoordinator` schreibt dieses Fenster mit dem
  bestehenden Run-Snapshot und bewahrt es auch bei `NoActiveRun`;
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

Der Identity-Baustein ruft keine `decide*`-Funktion und keinen
`RunPersistenceCoordinator` direkt fuer eine Mutation auf. Er liefert nur die
fehlende Identitaet an den vorhandenen Application-Requestaufbau. Die
mutierende Ausfuehrung bleibt beim bestehenden
`TemperatureControlApplicationOrchestrator`; dessen Handoffs und die
Write-before-Apply-Reihenfolge werden nicht dupliziert.

`FermentationUiCommandBridge::makeEnvelope()` darf weiterhin die vom
Application-Aufrufer gelieferte `UiRequestId` exakt in
`CommandEnvelope::id` abbilden. Die Quelle, der monotone Zeitwert, aktuelle
Evidenz und erwartete Revisionen werden ebenfalls von der bestehenden
Application-Komposition geliefert. Es entsteht kein neuer Dispatcher und
kein zweiter Command-Bus.

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

### 6.2 Codec- und Validierungsregeln

- `kCurrentRunPersistenceSchema` wird auf 4 angehoben;
- `knownRunPersistenceSchema()` akzeptiert 1, 2, 3 und 4;
- Decode verzweigt nur fuer dieses Feld zwischen `readUint32` bei Schema <4
  und `readUint64` bei Schema >=4;
- Encode von Schema 4 verwendet den vorhandenen Big-Endian-Codec und schreibt
  den vollstaendigen 64-Bit-Wert, ohne Cast oder Trunkierung;
- Head-, Slot-, CRC-, Epoch-, Referenz-, Laengen-, Trailing-Byte- und
  Zustandsvalidierungen bleiben die bestehenden Validierungen;
- ein ungueltiger Wert, eine abgeschnittene 64-Bit-Zahl oder ein unbekanntes
  neueres Schema fuehrt zu dem bestehenden fail-closed Ergebnis;
- es gibt keine zweite Run-Persistenz, keinen Paralleldecoder und keine
  Rueckmigration in ein anderes Provenienzregister.

### 6.3 Nachweise

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
   64-Bit-Wert.
2. Ein Schema-4-Golden-Payload schreibt und liest den vollstaendigen
   64-Bit-Wert ohne Veraenderung.
3. Schema-1/2/3- und Schema-4-Roundtrips bewahren Programm-Dokument,
   Source-Kind, Provenienz und alle schemaabhaengigen Recoveryfelder.
4. Trunkierung, Trailing Bytes, unbekanntes Schema 5 und ungueltige
   Referenzen laden fail-closed.
5. Current-, Fallback- und `NoActiveRun`-Recovery mit alten und neuen
   Records erzeugt dieselbe bestehende Run-/Recoveryentscheidung; ein
   Legacywert wird dabei nirgends als echte Katalogrevision ausgewertet.

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
2. der vorhandene `RunPersistenceCoordinator` hat den aktuellen oder
   gueltigen Fallback-/No-Active-Run-Snapshot geladen.

Aus dem geladenen Snapshot wird die sichere High-Water-Basis bestimmt:

- aus `persistedRunCommandIds[0..persistedRunCommandCount)` werden nur die
  bereits vom bestehenden Vertrag als eligible persistierten, von null
  verschiedenen, eindeutigen IDs verwendet;
- der groesste Wert ist der High Water Mark;
- `NoPersistedRun` oder ein gueltiger Snapshot ohne IDs bedeutet
  High Water `0`, die erste neue ID ist `1`;
- Current, NoActiveRun und gueltig geladener Fallback verwenden die IDs des
  tatsaechlich gewaehlt geladenen Snapshots; der bestehende Coordinator
  bewahrt sie bei `NoActiveRun` bereits ueber den Runwechsel;
- `ReadFailed`, `UnsupportedSchema`, `ForeignEpoch`, invalides ID-Fenster,
  unklare beziehungsweise indeterminierte Persistenz oder Safe-Boot ohne
  vertrauenswuerdigen Snapshot initialisieren den Allocator nicht. Neue
  Fachrequests und neue Runs liefern dann typisiert `Unavailable`/fail-closed.

Es wird kein zweiter persistierter Zaehler eingefuehrt. Die bestehende
Persistenz des eligible ID-Fensters ist ausreichend: eligible Run-Command-IDs
werden vom vorhandenen Coordinator erst mit dem owning Write-before-Apply in
den Snapshot aufgenommen, und der aktuelle Pfad bewahrt dieses Fenster auch
ohne aktiven Run. Da neue IDs ab der groessten vorhandenen ID fortlaufend
reserviert werden, ist dessen groesster Wert die sichere Basis fuer die
naechste erfolgreiche persistierte Run-Command-ID. Eine nur reservierte ID,
deren owning Persistenz nie erfolgreich war, beschreibt keinen ausgefuehrten
Run und darf nach Neustart nicht als ausgefuehrte Mutation gelten.

Diese Garantie bezieht sich bewusst auf die bestehende eligible Run-
Persistenz- und Replaysemantik. Nicht eligible Message-/Komfortkommandos
haben nach `docs/RUN_COMMANDS.md` keine sitzungsuebergreifende Replaygarantie;
#144 erweitert dafuer weder das Persistenzfenster noch fuehrt es eine zweite
Historie ein.

### 7.2 Allokationsergebnis und Grenzen

Der Allocator liefert einen kleinen typisierten lokalen Outcome, zum Beispiel
mit den Zustandswerten `Allocated`, `NotInitialized`, `InvalidPersistedBase`,
`Overflow` und `Unavailable`. Dies ist kein neuer `CommandStatus`, kein
zweiter Replayvertrag und keine Fachentscheidung.

Bei `allocate()` gilt:

- `next = highWater + 1` wird vor der Ausgabe checked;
- `0` bleibt ungueltig;
- `UINT64_MAX` als High Water fuehrt zu `Overflow` ohne Ausgabe;
- eine ausgegebene ID wird im RAM als naechste Basis vorgemerkt und innerhalb
  dieses Boots nicht recycelt, auch wenn die Vorpruefung oder der Benutzer
  spaeter abbricht;
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
| `AbortAndCool` | Stopoption und nur editierbare Cooling-/Manual-Werte | nur wenn ein neuer Cooling-Run erzeugt wird: Command-ID allokieren, `runId` ableiten, bestehenden `StopRequest::coolingPlan` vervollstaendigen | bestehender Stop-/Cooling-Applypfad |
| `CoolAfterCompletion` | Abschlussoption und nur editierbare Cooling-/Manual-Werte | nur wenn ein neuer Cooling-Run erzeugt wird: Command-ID allokieren, `runId` ableiten, bestehenden `CompletionRequest::coolingPlan` vervollstaendigen | bestehender Completion-/Cooling-Applypfad |

Wird bei Stop oder Completion kein neuer Cooling-Run angefordert, wird keine
neue Lauf-ID reserviert und kein `coolingPlan` erzeugt. Wird ein neuer Run
angefordert, ist die erzeugende Stop-/Completion-Command-ID zugleich dessen
`StartCommandId` fuer die kanonische Ableitung.

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
   Head-/Slot-/Recoveryvalidierungen unveraendert weiterverwenden.
3. `ApplicationRunIdentity` an der bestehenden Application-Grenze instanzieren
   und mit dem geladenen Snapshot, dem validierten `StorageEpoch` und dem
   bestehenden `persistedRunCommandIds`-Fenster verbinden.
4. Den vorhandenen UI-/Application-Requestaufbau so schliessen, dass
   `UiRequestId` vom Allocator stammt und die Envelope-ID exakt gespiegelt
   wird. Die bestehenden `decide*`-, Persistenz- und Lifecyclepfade bleiben
   die einzigen owning Pfade.
5. Den app-owned Run-ID-Aufbau fuer alle vier neuen Runpfade in die
   bestehenden Program-, Manual-, Stop- und Completion-Requests integrieren.
6. Die identitaetsfreien Adapterwerte fuer Manual-/Cooling-Eingaben auf den
   bestehenden owning Request abbilden, ohne einen zweiten Plan- oder
   Commandvertrag einzufuehren.
7. Die gezielten nativen Regressionen aus Abschnitt 10 ausfuehren und alle
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
| RP-06 | Roundtrip aller vier bekannten Schemas mit ihren bestehenden optionalen Feldern | Dokument, Source-Kind, Provenienz und Recoverydaten bleiben erhalten |
| RP-07 | unbekanntes Schema 5, abgeschnittenes Feld, Trailing Bytes, falsche Referenz/Epoch | bestehender Loadpfad bleibt fail-closed; kein Run und kein Allocator wird freigegeben |
| RP-08 | Current-, Fallback- und `NoActiveRun`-Recovery mit Legacy und Schema 4 | bestehende Recoveryklassifikation bleibt gleich; Legacyprovenienz wird nicht als Katalogrevision verwendet |
| ID-01 | leerer gueltiger Persistenzzustand | Allocator initialisiert mit High Water 0; erste ID ist 1 |
| ID-02 | gueltiger Snapshot mit unsortiertem eligible ID-Fenster | groesstes nichtnulles ID ist Basis; naechste Ausgabe ist exakt `max + 1` |
| ID-03 | `NoActiveRun` nach vorherigem owning Run-Commit | bestehende IDs bleiben Basis; keine Wiederverwendung einer bereits persistierten ID |
| ID-04 | `ReadFailed`, unbekanntes Schema, Foreign Epoch, invalid/dupliziertes Fenster oder unklare Persistenz | typisiertes `Unavailable`/fail-closed; kein Request und kein Run wird erzeugt |
| ID-05 | High Water `UINT64_MAX` | typisiertes `Overflow`; kein Wraparound und keine ID 0 |
| ID-06 | zwei neue Requests ueber denselben Application-Aufrufpfad | verschiedene fortlaufende IDs; `UiRequestId.value == CommandEnvelope::id` je Request |
| ID-07 | Touch und Web als zwei Quellen | beide verwenden denselben Allocator; keine Quellenprioritaet, keine getrennten Zaehler |
| ID-08 | unbestaetigtes bestaetigungspflichtiges Request und Confirmation-Replay | erste Entscheidung und Replay behalten exakt dieselbe Command-ID und denselben vorbereiteten Request; kein zweiter Allocatoraufruf |
| ID-09 | Pre-Apply-Abbruch einer reservierten ID, danach neue Anfrage im selben Boot | neue Anfrage recycelt die ID nicht |
| RUN-01 | `StartProgram` | UI liefert keine Identitaet; Application erzeugt Command-ID und `e<epoch>-c<id>` und ruft bestehenden Startpfad auf |
| RUN-02 | `StartManualHolding` | nur vorhandene Manual-Holding-Werte kommen vom UI; Application erzeugt `runId`; owning Vertrag bleibt ManualHolding |
| RUN-03 | `AbortAndCool` mit neuem Cooling-Run | Cooling-Payload enthaelt UI-seitig keine ID; Application verwendet die Stop-Command-ID als `StartCommandId` |
| RUN-04 | `CoolAfterCompletion` mit neuem Cooling-Run | gleiche Ableitung wie RUN-03; keine zweite Cooling-ID-Logik |
| RUN-05 | Stop/Completion ohne neuen Cooling-Run | keine Run-ID-Reservierung und kein Cooling-Plan |
| RUN-06 | alle vier Pfade mit maximalen Epoch-/ID-Dezimalwerten | Run-ID bleibt innerhalb 48 Bytes; ungueltige Basis/Laenge wird unavailable |
| RUN-07 | spaetere Confirmation oder Replay eines Cooling-Requests | dieselbe Command-ID und derselbe vorbereitete Cooling-`runId`; keine zweite Nebenwirkung |
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
  fuer den bestehenden Schema-/High-Water-Handoff erforderlich

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
- `test/test_fermentation_ui_commands/test_fermentation_ui_commands.cpp`
- `test/test_issue90_product_recovery_oracle/...` soweit der bestehende
  Recoverytest die Run-Provenienz direkt konstruiert
- ein kleiner nativer Identity-Test unter
  `test/test_issue144_run_identity/`, nur falls die vorhandene Teststruktur
  fuer den Application-Allocator keinen passenden bestehenden Einstieg
  besitzt; kein neues Testframework

Keine Firmware-, Hardware-, Display-, Touch-, Netzwerk- oder
Produktionsdatei ausserhalb dieser schmalen Identitaets-/Run-Contractgrenze
wird in #144 benoetigt. `docs/ROADMAP.md` ist bereits im ersten PR-Commit
synchronisiert; weitere Roadmap-Fachanforderungen werden hier nicht kopiert.

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
reviewt werden. Vor dieser Freigabe werden keine Produktionsdateien, Tests,
Issues oder PR-Zustaende veraendert.
