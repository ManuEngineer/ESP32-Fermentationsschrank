# Plan: Issue #106 – Per-Run-Aktorparameter-Snapshot und Recovery-Bindung

## 1. Planstatus und harte Grenzen

- Issue: **#106 – Aktorplaner Per-Run-Parameter-Snapshot und
  Recovery-Bindung**.
- Planbasis: `main@54c80d26416343495b4d9a8c4518e6137dc747c1`.
- Eigener Branch und eigener Draft-PR; dieser Plan baut nicht auf PR #156 auf.
- `IMPLEMENTATION=NOT_STARTED`.
- `DISPLAY_TOUCH_SCOPE=NOT_INCLUDED`.
- `ACTUATOR_RELEASE=NO`.
- Dieser Plan verändert keine produktiven Aktorparameter, Grenzwerte oder
  Defaults. Die fachliche und produktive Werte-/Grenzenfreigabe bleibt das
  Gate von Issue #35.
- Der Plan beschreibt die ausführbare strukturelle Vorbereitung: Producer,
  Schema, Per-Run-Snapshot, Persistenz-/Recoverybindung und sichere
  Planner-Lebenszeit. Er aktiviert keine produktive Konfiguration.
- Nach dem Plan-Commit hält der Builder für den Independent Full Plan Review
  an. Eine Implementierung beginnt erst nach Owner-Freigabe der exakten
  Plan-SHA.

Die bereits vorhandenen Verträge aus #17 (Run-Persistenz), #22/#23
(Temperatur-/Aktorplanung), #24 (Safety-Gate), #35 (produktive Werte und
Grenzen) und ADR-013 bleiben maßgeblich. Dieser Plan ergänzt nur die bisher
fehlende Bindung zwischen einer beim Run-Start festgeschriebenen
Servicekonfiguration und dem ausführenden, langlebigen Aktorplaner.

## 2. Ziel, Nicht-Ziele und Gate-Reihenfolge

### 2.1 Ziel

Jeder neue produktive Run erhält genau einmal einen vollständigen, validierten
`ActuatorPlannerParameters`-Snapshot aus der zu diesem Start gelesenen
Servicekonfiguration. Dieser Snapshot wird vor `NewActiveRun` atomar im
bestehenden Run-Checkpoint gespeichert, nach einem Neustart ausschließlich aus
dem gespeicherten Run wiederhergestellt und für die gesamte Run-Lebenszeit in
den langlebigen `ActuatorPlanner` kopiert. Änderungen der
Servicekonfiguration wirken erst auf einen späteren Run.

Der Ablauf muss für Program- und Manual-Run identisch gelten:

```text
RuntimeConfigurationReadLease erwerben
-> aktuelle ServiceConfiguration als immutable Startquelle lesen
-> vollständigen Run-Kandidaten inklusive Planner-Snapshot bilden
-> Snapshot und Run-Kandidat validieren
-> bestehende #17-Run-Transaktion atomar persistieren/verifizieren
-> erst nach bestätigtem Commit NewActiveRun/RAM-State publizieren
-> erst danach langlebigen ActuatorPlanner an den Snapshot binden
```

Bei Recovery gilt ausschließlich:

```text
Run-Head/Current/Fallback laden
-> Run-Payload und Planner-Snapshot validieren
-> denselben Snapshot in den RAM-Run-State rekonstruieren
-> bestehende Recovery-/Safety-Entscheidung fortsetzen
-> Planner erst nach erfolgreicher Activation wertkopierend binden
```

### 2.2 Nicht-Ziele

- Keine neuen PI-, Dead-Time-, Mindestzeit-, Watchdog- oder Lüfterwerte.
- Keine Defaultwerte und keine produktive Aktivierung von `TBD_*`.
- Keine Änderung der #23-Planungsalgorithmik, Prioritätsleiter oder
  Safety-Semantik.
- Keine neue Safety-Klassifikation und keine direkte Aktor-/GPIO-Persistenz.
- Keine zweite Konfigurations-, Persistenz- oder Recovery-Engine.
- Keine neue allgemeine UI-, Display- oder Touch-Komponente.
- Keine Änderung an PR #156, Issue #31 oder deren Hardware-/Renderer-Scope.
- Kein Arduino-/Zephyr-Wechsel, keine PSRAM-Abhängigkeit und keine
  Hardware-/ESP-IDF-Ausführung in diesem Plan-PR.

Die strukturelle Implementierung darf native Tests mit explizit als Testdaten
gekennzeichneten Parametern verwenden. Solche Werte sind keine
Produktionswerte und dürfen nicht in Composition Root, Service-Seed,
Produktionspersistenz oder Release-Konfiguration gelangen.

## 3. Repository- und Verantwortungsgrundlage

### 3.1 Bestehende SSOTs und Wiederverwendung

- `fermentation_app::ServiceConfiguration` und der vorhandene
  `ConfigurationService`/`RuntimeConfigurationReadLease` bleiben der einzige
  Konfigurations-Producer.
- `ActuatorPlannerParameters` aus `actuator_plan_types.hpp` bleibt der einzige
  Parameterwerttyp. Es wird kein zweites Feldmodell mit denselben zehn
  Aktorparametern eingeführt.
- `RunPersistenceCoordinator`, `RunPersistenceSnapshot`, die vorhandenen
  `RunCheckpointVariant`-Slots und der bestehende vorbereitete/committete
  Head-Mechanismus aus #17 bleiben der einzige Run-Persistenzpfad.
- `RecordTypeId=7` bleibt der Run-Checkpoint-Record, `RecordTypeId=8` bleibt
  der Run-Head-Record. Für den Snapshot werden keine neuen Record-Typen und
  keine neuen Slots eingeführt.
- Die vorhandene `classifyActuatorPlannerParameters()`-Prüfung ist die
  strukturelle Validierung. Ihre Bedeutung und #23-Safetyfolgen werden nicht
  neu erfunden.
- `RuntimeConfigurationReadLease::get()`/`operator->()` ist die einzige
  konsistente Startquelle. Ein Startpfad liest nicht mehrfach ungeschützt aus
  einer veränderlichen Konfiguration.

### 3.2 Ownership

| Verantwortung | Einziger Owner | Vertrag |
|---|---|---|
| Erzeugung der aktuellen Servicekonfiguration | `ConfigurationService` / bestehender Konfigurationsgraph | schema-versionierte `ServiceConfiguration`; kein zweiter Producer |
| Werttyp und strukturelle Parameterprüfung | `fermentation_app` mit bestehendem `ActuatorPlannerParameters` und `classifyActuatorPlannerParameters()` | keine produktiven Werte, keine Fach-/Safety-Neuregel |
| Bildung des immutable Per-Run-Snapshots | bestehender Application-/Startpfad im `fermentation_app` | genau einmal am kanonischen New-Active-Run-Punkt aus einer gültigen Runtime-Lease |
| Run-Record, Schema, Codec, Head/Slot-Transaktion | `RunPersistenceCoordinator` und bestehender #17-Codec | atomar, Active/Fallback gleichwertig, kein Live-Fallback |
| Recovery-Entscheidung und Wiederherstellung | bestehender Run-Recovery-/`RunPersistenceCoordinator`-Pfad | gespeicherten Snapshot validieren und rekonstruieren; kein zweiter Koordinator |
| Planner-Bindung und Lebenszeit | `TemperatureControlApplicationOrchestrator` | wertkopierende Bindung nach Commit/Activation, Reset an jeder kanonischen Boundary |
| Objektkonstruktion und Verdrahtung | Composition Root | nur langlebige Objekte erzeugen und Ports verdrahten; keine Parameter- oder Recoverypolicy |
| produktive Werte/Grenzen | Issue #35 / freigegebene Commissioning-Quelle | außerhalb dieses Plans; ohne #35 keine produktive Freigabe |

## 4. Service-Producer und schema-versionierte Konfiguration

### 4.1 Additive ServiceConfiguration-Schemaänderung

Die bisherige `ServiceConfigurationSchema::Version1` bleibt lesbar. Sie
repräsentiert weiterhin den aktuellen leeren Service-Datensatz und wird bei
der Lesekonvertierung nicht rückwirkend mit erfundenen Parametern aufgefüllt.

Additiv wird festgelegt:

```text
ServiceConfigurationSchema::Version2 = 2
kCurrentServiceConfigurationSchemaVersion = 2
ServiceConfiguration::actuatorPlannerParameters:
    std::optional<ActuatorPlannerParameters>
```

Die vorhandene `ActuatorPlannerParameters`-Struktur ist die einzige
Felddefinition. Die zehn Felder werden in ihrer bestehenden Reihenfolge und
mit den vorhandenen kanonischen `ByteWriter`-/`ByteReader`- und Binary64-
Konventionen kodiert. Schema 2 erhält einen kanonischen Presence-Tag; bei
`absent` folgt kein Parameterblock, bei `present` folgt genau der vollständige
Parameterblock. Zusätzliche oder verkürzte Bytes, nicht-finite Binary64-Werte
und ungültige Presence-Tags werden abgelehnt.

Damit gilt:

- Schema 1 mit leerem Payload bleibt gültig lesbar und ergibt
  `std::nullopt`.
- Neue Writes verwenden ausschließlich Schema 2 und schreiben keinen
  Produktivwert, wenn der Producer keinen freigegebenen Snapshot liefert.
- Unbekanntes neueres Schema, falsche Payloadgröße oder ungültige Parameter
  bleibt fail-closed.
- Der Service-Record bleibt `RecordTypeId=2`; seine zwei vorhandenen Slots,
  Manifest-/Root-Verweise und der bestehende Configuration-Envelope bleiben
  unverändert.
- `configuration_graph` und der Graph-Store akzeptieren beim Lesen Schema 1
  und 2, schreiben nur Schema 2 und bewahren die bestehende
  Canonical-Reencode-/Fail-Closed-Semantik.
- Die Implementierung leitet die zulässige Payloadgröße aus dem Codec ab bzw.
  prüft sie an einer zentralen Stelle; es wird kein willkürliches neues
  Ressourcenbudget erfunden.

### 4.2 Producer-Vertrag ohne Default-Aktivierung

Ein Runtime-Snapshot mit `std::nullopt`, `Unconfigured` oder `Invalid` ist ein
strukturell ehrlicher, nicht produktiv startbarer Zustand. Der Startpfad darf
ihn nicht durch Nullen, lokale Defaults, letzte Livewerte oder eine andere
Servicekonfiguration ersetzen. Erst Issue #35 kann eine gültige produktive
Konfiguration liefern.

Die Konfigurationsrevision und der `RuntimeConfigurationReadLease` halten die
Quelle bis zur Snapshotbildung konsistent. Nach der wertkopierenden
Übernahme ist der Run unabhängig von späteren Serviceänderungen. Der Run
persistiert keine ESP-IDF-Typen, GPIOs oder Hardwareparameter.

## 5. Per-Run-Snapshot und Wire-Vertrag

### 5.1 RAM-Modell

`RunCommandState` und `RunPersistenceSnapshot` erhalten ein einziges
zusätzliches Feld:

```text
std::optional<ActuatorPlannerParameters> actuatorPlannerParametersSnapshot
```

Das Feld verwendet denselben bestehenden Parameterwerttyp und ist semantisch
der Per-Run-Snapshot, nicht eine zweite Parameterquelle. Es wird:

- bei jedem neuen Program- oder Manual-Run vor der Persistenz in den
  Kandidaten kopiert;
- bei jeder normalen laufenden Run-Mutation unverändert mitgeführt;
- beim Stop/NoActiveRun gelöscht;
- bei Recovery aus dem ausgewählten Current- oder Fallback-Payload
  wiederhergestellt;
- niemals während eines aktiven Runs aus `ConfigurationService` aktualisiert.

Ein aktiver Run ohne dieses Feld ist nicht produktiv rekonstruierbar. Ein
`NoActiveRun`-Snapshot darf kein Planner-Snapshot-Feld tragen.

### 5.2 Run-Persistenzschema 6

Die bestehende gemeinsame Run-Schemaidentität wird eingefroren und additiv
fortgeschrieben:

```text
RunCheckpointRecordTypeId = 7
RunHeadRecordTypeId       = 8
kCurrentRunPersistenceSchema = 6
```

Schema 1 bis 5 bleiben technisch decodierbar, soweit ihre jeweiligen bereits
definierten Felder gültig sind. Neue Head- und Checkpoint-Writes verwenden
Schema 6. Kein altes Payload wird in-place umgedeutet.

Im Schema-6-Checkpoint wird am Ende des bestehenden kanonischen
Snapshot-Feldaufbaus ein Planner-Parameterblock angehängt:

- `ProgramRun` und `ManualRun`: vollständiger, nicht optionaler Snapshotblock
  mit den zehn bestehenden Parametern in `ActuatorPlannerParameters`-
  Reihenfolge;
- `NoActiveRun`: kein Snapshotblock;
- fehlender, verkürzter, überlanger oder strukturell ungültiger Block: Decode-
  bzw. Validierungsfehler;
- die Schema-5-Bytefolge bleibt unverändert und erhält beim Legacy-Decode
  `std::nullopt`.

Die bestehende Envelope-Prüfung, CRC, Payloadgrenze, Variantprüfung und
kanonische Re-Encode-Prüfung bleiben aktiv. Die aus dem neuen Block resultierende
Payloadgröße wird in den vorhandenen Capacity-/Ressourcentests nachgewiesen;
die Implementierung darf eine bestehende Kapazitätskonstante nur aus dem
gemessenen/abgeleiteten Codecbedarf anpassen, nicht als Produktivbudget
erfinden.

### 5.3 Legacy- und unbekannte Schemas

- Ein alter `NoActiveRun`-Record aus Schema 1–5 bleibt als solcher verwendbar.
- Ein alter aktiver Schema-1–5-Record ohne erforderlichen Planner-Snapshot
  wird technisch gelesen, aber als `NotReconstructible` mit einem eindeutigen
  Grund `MissingRequiredActuatorPlannerSnapshot` klassifiziert. Es gibt keinen
  Live-Service-Fallback und keine Aktorfreigabe.
- Ein beschädigter oder widersprüchlicher Snapshot, ein Snapshot mit
  `Unconfigured`/`Invalid`-Parametern, ein unbekanntes neueres Schema oder ein
  fremder `StorageEpoch` bleibt fail-closed nach den bestehenden
  Run-Persistence-/Recovery-Verträgen.
- Current und Fallback werden immer als vollständige Run-Kandidaten inklusive
  desselben Planner-Snapshots geprüft. Ein Fallback darf keinen fehlenden
  Snapshot aus Current oder Live-Konfiguration ergänzen.

## 6. Fresh-Start-Transaktion: Write-before-Apply

Der vorhandene `persistFreshStartCommand`-Pfad wird zur einzigen
Anwendungsgrenze für einen neuen produktiven Run erweitert. Die
Implementierung muss dafür diese Lease-gebundene Schnittstelle oder eine
semantisch identische, nicht umgehbare Typgrenze herstellen:

```text
persistFreshStartCommand(
    current,
    startDecision,
    const RuntimeConfigurationReadLease& runtimeLease,
    time,
    liveSensorEvidence)
```

Die Methode muss:

1. ausschließlich `StartProgram` und `StartManualHolding` akzeptieren;
2. eine gültige, nicht abgelaufene Runtime-Lease und den darin enthaltenen
   `ServiceConfiguration`-Snapshot verlangen;
3. den vollständigen Planner-Snapshot wertkopierend in den Run-Kandidaten
   übernehmen;
4. `classifyActuatorPlannerParameters()` und alle bestehenden
   Run-Kandidaten-/Plausibilitätsprüfungen vor dem Schreiben ausführen;
5. die bestehende `RunPersistenceCoordinator`-Transaktion benutzen, wobei
   der Snapshot vor `makeRunPersistenceSnapshot()` im Kandidaten liegt;
6. bei fehlender/ungültiger Quelle, Candidate-Fehler, Slot-/Head-/Verify-
   Fehler oder unbestimmtem Write-Ergebnis weder RAM noch Planner binden;
7. erst nach `Applied` die bestehende `applyRunCommand`-/RAM-Änderung sichtbar
   machen.

Dafür darf die bestehende `RunPersistenceCoordinator`-Klasse einen
start-spezifischen, streng typisierten Snapshot-Handoff an ihre vorhandene
Persistenzroutine erhalten. Das ist keine zweite Transaktion: Candidate-Apply,
`writeSnapshotCore`, Head-Commit, Verify und bestehende Rollback-/Failure-
Semantik bleiben zentral im selben Coordinator. Ein beliebiger Caller darf
nicht direkt ein produktives Parameterobjekt in `persistCommand()` einschleusen.

Die zulässige Reihenfolge ist:

```text
Lease-Snapshot A lesen
-> Start-Kandidat mit A bilden
-> A validieren
-> A zusammen mit Run atomar schreiben/verifizieren
-> Commit bestätigen
-> RunState A anwenden
-> Planner mit A binden
-> normale Evaluierung/Aktorplanung zulassen
```

Eine zwischenzeitliche Änderung der Servicekonfiguration B beeinflusst weder
Run A noch seinen Planner. Der nächste neue Start liest B aus einer neuen
Lease und erzeugt Run B'.

## 7. Recovery und Lebenszeitbindung

### 7.1 Recovery im bestehenden Owner

Der bestehende `RunPersistenceCoordinator` und die vorhandenen
`RunRecoveryCoordinator`-/Boot-Klassifikationspfade bleiben alleinige Owner.
Die Implementierung ergänzt nur:

- Snapshot-Validierung in `validateRunPersistenceSnapshot*()`;
- Snapshot-Übernahme in `makeRunPersistenceSnapshotInto()` und
  `restoreRunPersistenceSnapshotInto()`;
- die Legacy-Klassifikation `MissingRequiredActuatorPlannerSnapshot`;
- die Prüfung, dass Current und Fallback denselben vollständigen
  Snapshot-Vertrag erfüllen.

Recovery fragt die aktuelle Servicekonfiguration nicht ab, um ein fehlendes,
altes oder beschädigtes Feld zu reparieren. Ein solcher Run bleibt ohne
produktive Aktivierung und folgt der bestehenden fail-closed
`NotReconstructible`-/Recovery-Fehlerbehandlung. Es wird kein zweiter
Recovery-Koordinator und keine parallele Run-State-Machine eingeführt.

### 7.2 Langlebiger ActuatorPlanner

`ActuatorPlanner` bleibt ein langlebiges Composition-Root-Objekt. Die
Implementierung ersetzt die bisher boot-session-feste Produktparameterbindung
durch einen wertkopierenden Run-Lifecycle:

```text
construct unconfigured / test-configured only
-> beginRun(validSnapshot) after confirmed persistence or recovery activation
-> tick only while that copied binding is active
-> forceStop at existing boundary
-> endRun / clearRunBinding
-> next beginRun with a new copied snapshot
```

Normative Eigenschaften:

- Der Planner speichert weder Referenzen noch Pointer auf
  `ServiceConfiguration`, `RuntimeConfigurationReadLease` oder
  `RunCommandState`.
- `beginRun()` kopiert alle zehn Parameter, validiert sie und weist einen
  aktiven Rebind zurück. Ein aktiver Run erhält keine Mid-Run-Änderung.
- Ohne Bindung, bei `Unconfigured` oder `Invalid` liefert der bestehende
  Planner fail-closed `Idle`/`Unconfigured`; daraus wird keine
  `ActuatorSafetyGateStatus::Allowed`-Freigabe abgeleitet.
- `TemperatureControlApplicationOrchestrator` ruft `beginRun()` erst nach
  bestätigtem Fresh-Start-Commit bzw. erfolgreicher Recovery-Activation auf.
  Bei fehlendem oder ungültigem Snapshot bleibt der Pfad gesperrt.
- Derselbe Orchestrator räumt Bindung, Feedback und offene Evaluationen an
  allen bereits kanonischen Stop-, Fault-, Standby-, Completion- und
  Recovery-Abbruch-Boundaries auf. `forceStop` erfolgt vor `endRun()`.
- Zwei aufeinanderfolgende Runs müssen mit demselben Planner-Objekt und zwei
  unabhängigen Kopien sicher funktionieren; nach `endRun()` darf kein alter
  Snapshot weiterwirken.

Der Composition Root konstruiert und verdrahtet nur die langlebigen Objekte.
Er entscheidet keine Produktparameter und implementiert keine Recoverypolicy.

## 8. Konkrete Implementierungsslices nach Planfreigabe

Die folgenden Slices sind die zulässige Implementierungsreihenfolge. Jeder
Slice bleibt auf #106 begrenzt und enthält keine Display-/Touch-Änderung.

1. **Service-Modell und Codec**
   - `configuration_documents.*`, `runtime_configuration_snapshot.*`,
     `configuration_document_codec.*`, `configuration_graph.*` und
     `configuration_graph_store.*`.
   - Schema 2, additive V1-Kompatibilität, Planner-Feld, zentrale Validierung
     und korrekte Manifest-/Payloadgrenzen.
2. **Run-RAM-Modell und Run-Codec**
   - `run_commands.*`, `run_persistence_contract.*`,
     `run_persistence_codec.*`, `run_persistence_coordinator.*` sowie die
     bestehende Recovery-/Boot-Klassifikation.
   - Schema 6, Snapshot-Feld, Active/Fallback, Legacy-Klassifikation und
     unveränderte #17-Transaktionssemantik.
3. **Fresh-Start-Handoff**
   - `temperature_control_orchestrator.*` und der bereits existierende
     Application-Startpfad.
   - Leasegebundene Snapshotbildung vor Candidate-Write und keine RAM-/Planner-
     Anwendung vor `Applied`.
4. **Planner-Lebenszeit**
   - `actuator_planner.*`, `actuator_plan_types.*` nur soweit für den
     wertkopierenden Lifecycle erforderlich, sowie der bestehende Orchestrator.
   - `beginRun`/`endRun`, Bindungsstatus und Reset an allen vorhandenen
     Lebenszyklusgrenzen; keine Algorithmusänderung.
5. **Dokumentation und Guard**
   - `docs/RUN_PERSISTENCE.md` und die direkt betroffenen Vertragsabschnitte
     aktualisieren.
   - Den Architektur-/Dependency-Guard so ergänzen, dass nur die bestehende
     App-/Orchestrator-Grenze den Snapshot an den Planner übergibt und keine
     neue Persistenz-/Recoverykomponente entsteht.

Eine neue `lib/fermentation_ui_renderer_*`-, Display-, Touch- oder allgemeine
Parameter-/Persistenzbibliothek ist ausdrücklich nicht Teil der Slices.

## 9. Test- und Nachweisplan nach Implementierung

Es werden nur gezielte native Tests des geänderten Bereichs und direkt
betroffene Vertragskonsumenten ausgeführt. Hardware, ESP-IDF, Display/Touch
und Pre-Ready-Gates sind in diesem Plan-PR `NOT_RUN`.

### 9.1 Konfiguration

- Schema-1-Leerrecord liest weiterhin zu `ServiceConfiguration` ohne Snapshot.
- Schema-2-Present/Absent round-tripped kanonisch.
- Alle zehn Parameter und Binary64-Grenzfälle werden verlustfrei bzw.
  fail-closed nach bestehendem Codecvertrag geprüft.
- Falscher Presence-Tag, falsche Payloadlänge, `NaN`/unendliche Werte,
  unbekanntes neueres Schema und ungültige Parameter werden abgelehnt.
- Graph-/Manifest-Read akzeptiert V1/V2 und schreibt nur die aktuelle Version;
  es entstehen keine Defaults.

### 9.2 Run-Persistenz und Recovery

- Schema-6-Program- und Manual-Run enthalten exakt denselben validierten
  Snapshot in Current und Fallback.
- Schema-5-Active-Run ohne Snapshot wird als nicht rekonstruierbar behandelt;
  Schema-5-NoActiveRun bleibt lesbar.
- Corruption, fehlendes Feld, falsche Variante, fremde Epoch und unbekanntes
  neueres Schema bleiben fail-closed.
- Write-Abbruch vor, während und nach dem Commit lässt keinen neuen RAM-Run
  und keine Planner-Bindung zu; die bestehende Head-/Fallback-Evidence bleibt
  maßgeblich.
- Run A mit Servicewerten A behält A trotz nachfolgender Serviceänderung B;
  der nächste neue Run erhält B; Recovery von A ignoriert Live-B.
- Es wird kein direkter Aktor-/GPIO-Wert persistiert.

### 9.3 Planner-Lebenszeit und Application Boundary

- `beginRun()` erfolgt erst nach bestätigtem `Applied` und nicht nach einem
  bloßen Candidate-/Write-Aufruf.
- Persistenzfehler, fehlender Snapshot und ungültige Parameter lassen Planner
  und Aktorpfad fail-closed.
- Derselbe Planner überlebt zwei Runs, kopiert beide Snapshots unabhängig und
  verliert den alten Snapshot nach `endRun()`.
- Rebind während eines aktiven Runs, Lease-/State-Pointer und Mid-Run-
  Konfigurationsänderung werden zurückgewiesen bzw. bleiben wirkungslos.
- Stop, Fault, Standby, Completion und Recovery-Abbruch führen über die
  bestehende zentrale Boundary zu `forceStop` und `endRun`.
- Bestehende #22/#23-, #17- und #24-Tests bleiben grün; kein Test behauptet
  produktive Wertefreigabe durch #106.

### 9.4 Abschlusschecks

Vor dem Independent Review des späteren Implementierungs-PRs sind mindestens
zu prüfen:

- `git diff --check` und Repository-Secret-/Dependency-/Architecture-Guards;
- gezielte Host-/native Tests gemäß `docs/CI_AND_QUALITY_GATES.md`;
- keine Änderungen außerhalb des genehmigten #106-Scope;
- kein `Ready`, kein Merge, kein Auto-Merge und keine Aktorfreigabe.

Ein vollständiger Pre-Ready-Lauf ist erst nach abgeschlossenem Independent
Full Review mit `OPEN_BLOCKERS=0` und ausdrücklicher Owner-Anweisung zulässig.

## 10. Abnahmekriterien für Issue #106

Issue #106 ist strukturell erst erfüllbar, wenn alle folgenden Aussagen durch
Code und gezielte Tests belegt sind:

1. ServiceConfiguration Schema 2 kann den bestehenden Plannerwerttyp
   versioniert erzeugen, validieren, lesen und schreiben; Schema 1 bleibt
   rückwärts lesbar und erzeugt keine erfundenen Werte.
2. Ein neuer Program- und Manual-Run persistiert genau einen vollständigen
   Planner-Snapshot innerhalb des bestehenden #17-Run-Records, bevor er als
   `NewActiveRun`/RAM-State sichtbar wird.
3. Current und Fallback tragen bzw. validieren denselben Snapshotvertrag;
   fehlender oder nicht validierbarer Snapshot führt zu keiner produktiven
   Recovery.
4. Recovery verwendet ausschließlich den persistierten Snapshot und niemals
   eine inzwischen geänderte Live-Servicekonfiguration.
5. Der langlebige Planner besitzt eine wertkopierende, ausführbare
   `beginRun`/`endRun`-Lebenszeitbindung ohne Dangling- oder Mid-Run-Referenzen.
6. Keine Produktivparameter, Grenzwerte, Defaults, GPIO-/Hardwarewerte oder
   `ActuatorSafetyGateStatus::Allowed`-Freigabe werden durch #106 erfunden oder
   aktiviert; diese bleiben von #35 beziehungsweise den bestehenden Safety-
   und Hardware-Gates abhängig.
7. Keine Display-/Touch-Datei, kein PR-#156-Inhalt und keine neue allgemeine
   Renderer-/Persistenzplattform ist enthalten.

## 11. Plan- und PR-Handover

Nach dem Plan-Commit und der Draft-PR-Synchronisation müssen die finalen
Metadaten exakt so vorgelegt werden:

```text
PLAN_SHA=<exact plan commit>
PR_HEAD=<exact current draft-pr head>
ISSUE=106
PLAN_BASE=main@54c80d26416343495b4d9a8c4518e6137dc747c1
PLAN_SCOPE=STRUCTURAL_PRODUCER_PER_RUN_SNAPSHOT_PERSISTENCE_RECOVERY_LIFETIME
IMPLEMENTATION=NOT_STARTED
PRODUCTIVE_ACTUATOR_PARAMETERS=NOT_SET
DISPLAY_TOUCH_SCOPE=NOT_INCLUDED
ACTUATOR_RELEASE=NO
ROADMAP_SYNC=PASS
GIT_DIFF_CHECK=PASS
NEXT_GATE=INDEPENDENT_FULL_PLAN_REVIEW
```

Der PR bleibt Draft. Nach der Übergabe dieses exakten Plan- und HEAD-Nachweises
wird angehalten; weder Independent Review noch Owner-Freigabe werden durch den
Builder vorweggenommen.
