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
- Nach dem Plan-Commit hält der Builder für die Independent Plan Fix
  Verification an. Eine Implementierung beginnt erst nach Owner-Freigabe der
  exakten Plan-SHA.

Die bereits vorhandenen Verträge aus #17 (Run-Persistenz), #22/#23
(Temperatur-/Aktorplanung), #24 (Safety-Gate), #35 (produktive Werte und
Grenzen) und ADR-013 bleiben maßgeblich. Dieser Plan ergänzt nur die bisher
fehlende Bindung zwischen einer beim Run-Start festgeschriebenen
Servicekonfiguration und dem ausführenden, langlebigen Aktorplaner.

## 2. Ziel, Nicht-Ziele und Gate-Reihenfolge

### 2.1 Ziel

Jeder logisch startbare Run erhält an derselben bestehenden Startgrenze
entweder keinen Planner-Snapshot (`absent`) oder genau einen vollständigen,
validierten `ActuatorPlannerParameters`-Snapshot (`present`) aus einer gültigen
`RuntimeConfigurationReadLease`. Diese Snapshot-Provenienz entscheidet nur,
welcher unveränderliche Planerwert in den Run gelangt; sie ist keine Safety-
oder Aktorfreigabe. Ein `absent`-Run darf logisch und persistent ohne
Planner-Bindung starten. Ein `present`-Snapshot wird vor `NewActiveRun`
atomar im bestehenden Run-Checkpoint gespeichert, nach einem Neustart
ausschließlich aus dem gespeicherten Run wiederhergestellt und für die aktive
Run-Lebenszeit in den langlebigen `ActuatorPlanner` kopiert. Änderungen der
Servicekonfiguration wirken erst auf einen späteren Run.

Der Ablauf muss für Program- und Manual-Run identisch gelten:

```text
eine einzige bestehende Fresh-Start-API mit Snapshot-Provenienz
-> absent: Run-Kandidat ohne Planner-Snapshot bilden
-> present: bestehende RuntimeConfigurationReadLease erwerben
-> bei present aktuellen Service-Snapshot lesen und validieren
-> Run-Kandidat mit absent/present Planner-Snapshot bilden
-> bestehende #17-Run-Transaktion atomar persistieren/verifizieren
-> erst nach bestätigtem Commit NewActiveRun/RAM-State publizieren
-> nur bei present Snapshot den Planner binden
-> tatsächliche Aktorfreigabe ausschließlich über #24/ActuationInterlock
```

Bei Recovery gilt ausschließlich:

```text
Run-Head/Current/Fallback laden
-> jeden Kandidaten für sich vollständig validieren
-> present Snapshot rekonstruieren oder absent als actor-free Zustand laden
-> niemals Live-Servicewerte nachladen
-> bestehende Recovery-/Safety-Entscheidung fortsetzen
-> Planner nur bei present Snapshot nach erfolgreicher Activation binden
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
| Bildung des immutable Per-Run-Snapshots | bestehender Application-/Startpfad im `fermentation_app` | genau einmal am kanonischen New-Active-Run-Punkt bei `present` aus einer gültigen Runtime-Lease; `absent` bleibt absent |
| Run-Record, Schema, Codec, Head/Slot-Transaktion | `RunPersistenceCoordinator` und bestehender #17-Codec | atomar; jeder Current-/Fallback-Kandidat ist für sich vollständig; kein Live-Fallback |
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
strukturell ehrlicher, actor-free Zustand ohne Planner-Bindung. Der
logisch/persistent startbare `absent`-Pfad darf ihn nicht durch Nullen, lokale
Defaults, letzte Livewerte oder eine andere Servicekonfiguration ersetzen. Ein
`present`-Handoff wird ohne gültige, nicht abgelaufene Herkunft aus der
bestehenden Runtime-Konfiguration vor `Applied` abgelehnt. Ob Aktoren
tatsächlich freigegeben werden, entscheidet ausschließlich #24 über
`ActuationInterlock`; #35 bleibt alleiniger Owner der produktiven Werte und
Grenzen.

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

- bei einem `present`-Program- oder Manual-Run vor der Persistenz in den
  Kandidaten kopiert;
- bei einem `absent`-Run absent bleibt und keinen Planner bindet;
- bei jeder normalen laufenden Run-Mutation unverändert mitgeführt;
- beim Stop/NoActiveRun gelöscht;
- bei Recovery aus dem ausgewählten Current- oder Fallback-Payload
  wiederhergestellt;
- niemals während eines aktiven Runs aus `ConfigurationService` aktualisiert.

Ein aktiver Run ohne dieses Feld ist ein gültiger, actor-free Run, aber nicht
für produktive Aktorplanung zugänglich. Recovery darf daraus keinen
Planner-Snapshot ergänzen. `NoActiveRun` trägt keinen Planner-Snapshot.

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
Snapshot-Feldaufbaus ein variantspezifischer Planner-Snapshotabschnitt
angehängt:

- `ProgramRun` und `ManualRun`: ein Presence-Tag; `present` trägt den
  vollständigen Snapshotblock mit den zehn bestehenden Parametern in
  `ActuatorPlannerParameters`-Reihenfolge, `absent` ist ausschließlich der
  persistierte actor-free/`actuation-unconfigured`-Zustand;
- `NoActiveRun`: kein Snapshotblock;
- ungültiger Presence-Tag, verkürzter, überlanger oder strukturell ungültiger
  `present`-Block: Decode- bzw. Validierungsfehler;
- die Schema-5-Bytefolge bleibt unverändert und erhält beim Legacy-Decode
  bei aktivem Run `std::nullopt` und damit den actor-free-Zustand.

Die bestehende Envelope-Prüfung, CRC, Payloadgrenze, Variantprüfung und
kanonische Re-Encode-Prüfung bleiben aktiv. Die aus dem neuen Block resultierende
Payloadgröße wird in den vorhandenen Capacity-/Ressourcentests nachgewiesen;
die Implementierung darf eine bestehende Kapazitätskonstante nur aus dem
gemessenen/abgeleiteten Codecbedarf anpassen, nicht als Produktivbudget
erfinden.

### 5.3 Legacy- und unbekannte Schemas

- Ein alter `NoActiveRun`-Record aus Schema 1–5 bleibt als solcher verwendbar.
- Ein alter aktiver Schema-1–5-Record ohne Planner-Snapshot wird technisch
  gelesen und als actor-free/`actuation-unconfigured` rekonstruiert. Es gibt
  keinen Live-Service-Fallback und keine Aktorfreigabe.
- Ein beschädigter oder widersprüchlicher Snapshot, ein Snapshot mit
  `Unconfigured`/`Invalid`-Parametern, ein unbekanntes neueres Schema oder ein
  fremder `StorageEpoch` bleibt fail-closed nach den bestehenden
  Run-Persistence-/Recovery-Verträgen. `absent` ist hiervon als explizit
  erlaubter actor-free Zustand zu unterscheiden.
- Jeder Current-/Fallback-Kandidat wird für sich vollständig geprüft. Gehören
  Current und Fallback zur selben Run-Identität, müssen sie unabhängig von
  unterschiedlichen Run-Revisionen denselben immutable Snapshot tragen
  (oder beide absent sein). `NoActiveRun` trägt keinen Snapshot. Ein Fallback
  eines anderen älteren Runs trägt ausschließlich dessen eigenen Snapshot.
  Kein Kandidat wird aus Current, einem anderen Run oder der Live-Konfiguration
  ergänzt.

## 6. Fresh-Start-Transaktion: Write-before-Apply

Der vorhandene `persistFreshStartCommand`-Pfad bleibt die einzige
Anwendungsgrenze für jeden neuen Program- und Manual-Run. #106 entscheidet an
dieser Grenze ausschließlich über Snapshot-Provenienz: `absent` oder
`present` aus der bestehenden Runtime-Konfiguration. Zuerst sind die
vorhandenen Typen und APIs, insbesondere `RuntimeConfigurationReadLease` und
das bestehende optionale Snapshot-Feld, zu verwenden. Nur wenn ein normaler
`persistCommand()`-Pfad diese Grenze nachweislich umgehen könnte, darf die
kleinste zusätzliche, streng typisierte `FreshStartSnapshotProvenance`
eingeführt werden. Sie trägt keine Safety-Capability und keine
Commissioning-Wahrheit. Eine Bezeichnung wie
`FreshStartActuationAdmission` darf nicht zu einem zweiten Safety-Vertrag
werden:

```text
persistFreshStartCommand(
    current,
    startDecision,
    existing snapshot-provenance handoff,
    time,
    liveSensorEvidence)
```

Der Handoff hat genau zwei Provenienz-Zustände, keine Aktorberechtigung:

```text
absent
present + gültige, nicht abgelaufene RuntimeConfigurationReadLease
```

#106 entscheidet damit nur, ob der Snapshot vorhanden ist und ob seine
Herkunft aus der bestehenden Runtime-Konfiguration gültig ist. `#24` bzw.
`ActuationInterlock` bleibt die alleinige Autorität für die tatsächliche
Aktorfreigabe aus frischer Safety-Evidenz. `#35` bleibt alleiniger Owner der
produktiven Werte und Grenzen. Es gibt keine neue Safety-Capability, keine
parallele Freigabelogik und keine zweite Commissioning-Wahrheit.

Die Methode muss:

1. ausschließlich `StartProgram` und `StartManualHolding` akzeptieren;
2. `absent` ohne Runtime-Lease als logisch/persistent startbar behandeln, den
   Snapshot absent lassen und keine Planner-Bindung vorbereiten;
3. bei `present` eine gültige, nicht abgelaufene Runtime-Lease und den darin
   enthaltenen `ServiceConfiguration`-Snapshot verlangen;
4. bei `present` den vollständigen Planner-Snapshot wertkopierend in den
   Run-Kandidaten übernehmen;
5. `classifyActuatorPlannerParameters()` ausschließlich als bestehende
   strukturelle Parameterprüfung und alle bestehenden
   Run-Kandidaten-/Plausibilitätsprüfungen vor dem Schreiben ausführen;
6. die bestehende `RunPersistenceCoordinator`-Transaktion benutzen, wobei
   der absent/present Snapshot vor `makeRunPersistenceSnapshot()` im
   Kandidaten liegt;
7. einen behaupteten `present`-Snapshot ohne gültige Herkunft vor `Applied`
   ablehnen; dies gilt auch dann, wenn ein Caller versucht, die normale
   `persistCommand()`-Methode zu verwenden;
8. bei Candidate-, Slot-, Head-, Verify-Fehler oder unbestimmtem Write-Ergebnis
   weder RAM noch Planner binden;
9. erst nach `Applied` die bestehende `applyRunCommand`-/RAM-Änderung sichtbar
   machen und nur bei present Snapshot den Planner binden; eine tatsächliche
   Aktorfreigabe bleibt anschließend weiterhin die Entscheidung von
   `ActuationInterlock`.

Dafür darf die bestehende `RunPersistenceCoordinator`-Klasse einen
start-spezifischen, streng typisierten Snapshot-Handoff an ihre vorhandene
Persistenzroutine erhalten, falls die vorhandenen APIs die Provenienz sonst
nicht-umgehbar transportieren. Das ist keine zweite Transaktion:
Candidate-Apply, `writeSnapshotCore`, Head-Commit, Verify und bestehende
Rollback-/Failure-Semantik bleiben zentral im selben Coordinator. Ein
beliebiger Caller darf weder einen Safety-/Commissioning-Status noch ein
produktives Parameterobjekt in `persistCommand()` einschleusen. Es wird keine
zweite Fresh-Start- oder Safety-Logik eingeführt.

Die zulässige Reihenfolge ist:

```text
Snapshot-Provenienz bestimmen
-> absent: Run-Kandidat ohne Snapshot bilden
-> present: Lease-Snapshot A lesen und validieren
-> absent/present Run-Kandidat atomar schreiben/verifizieren
-> Commit bestätigen
-> RunState anwenden
-> nur bei present Snapshot Planner mit A binden
-> actor-free bleibt ohne produktive Aktorplanung
-> aktuelle #24/ActuationInterlock-Evidenz entscheidet separat über Freigabe
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
- die explizite Schema-6-Semantik `absent=actuation-unconfigured` und
  `present=gebundener Snapshot`;
- die Prüfung jedes Current-/Fallback-Kandidaten für sich sowie die
  Identitätsregel für zwei Revisionen desselben Runs.

Recovery fragt die aktuelle Servicekonfiguration nie ab. Ein fehlender
Snapshot wird als actor-free rekonstruiert und bleibt ohne Planner-Bindung;
ein beschädigter oder ungültiger `present`-Snapshot folgt der bestehenden
fail-closed Recovery-Fehlerbehandlung. Es wird kein zweiter Recovery-
Koordinator und keine parallele Run-State-Machine eingeführt.

### 7.2 Langlebiger ActuatorPlanner

`ActuatorPlanner` bleibt ein langlebiges Composition-Root-Objekt. Die
Implementierung ersetzt die bisher boot-session-feste Produktparameterbindung
durch eine wertkopierende aktive Run-Bindung und einen kleinen, im selben
Planner-/Orchestratorzustand geführten Teardown-Kontext:

```text
construct unconfigured / test-configured only
-> beginRun(validSnapshot) after confirmed persistence or recovery activation
-> tick active planning while that copied binding is active
-> at logical Run-Ende: forceStop while A is still available
-> retain only A's required teardown-tail copy and physical anchors
-> endRun clears active A binding, not tail/anchor evidence
-> tail ticks continue actor-free until their own deadlines complete
-> next beginRun(B) may coexist with an unfinished A tail
-> active planning uses B; A affects only the already-created tail
```

Normative Eigenschaften:

- Der Planner speichert weder Referenzen noch Pointer auf
  `ServiceConfiguration`, `RuntimeConfigurationReadLease` oder
  `RunCommandState`.
- `beginRun()` kopiert alle zehn Parameter, validiert sie und weist einen
  aktiven Rebind zurück. Ein aktiver Run erhält keine Mid-Run-Änderung.
- Die kleinste Zustands-Erweiterung ist eine optionale
  `teardownParameters_`-Kopie im bestehenden Planner (oder ein semantisch
  identisches Feld im vorhandenen Runtime-State) plus die vorhandenen Fan-
  Deaktivierungsanker und je betroffenem Lüfter eine erhaltene absolute
  monotone Teardown-Deadline. Bestehende Felder wie
  `lastPhysicalDeactivationDirection`/
  `lastPhysicalDeactivationAtMonotonicMillis` werden wiederverwendet, soweit
  sie dafür ausreichen. Es wird keine zweite Lifecycle-Engine, Deadline-Queue
  oder parallele Run-State-Machine angelegt.
- `forceStop()` beendet die aktive physische Planung und erzeugt bzw. erhält
  die vorhandenen äußeren/inneren Fan-Nachlaufanker. Für jeden betroffenen
  Lüfter wird die daraus berechnete absolute A-Deadline materialisiert oder
  erhalten. Erst danach darf `endRun()` die aktive A-Bindung löschen. Die für
  den laufenden Nachlauf nötige kopierte A-Parameterbasis bleibt bis zum
  Abschluss dieses Nachlaufs verfügbar.
- `endRun()` löscht weder laufende Fan-Nachläufe noch
  `lastPhysicalDeactivationDirection`/-Zeit. Diese Evidenz bleibt erhalten,
  bis die vorhandenen Nachlauf- bzw. Mindest-Auszeit-/Totzeitberechnungen sie
  nicht mehr benötigen; danach wird die Tail-Kopie deterministisch freigegeben.
- Startet Run B während eines A-Nachlaufs, bleiben die betroffenen Lüfter
  kontinuierlich aktiv. Die aktive Planung, neue Fenster und ein späterer
  B-Teardown verwenden B; A wirkt nur auf den bereits entstandenen Tail.
  Ein späteres `forceStop()` von B ersetzt den alten Tail-Kontext nicht. Für
  jeden betroffenen Lüfter gilt die effektive Deadline als Maximum aus jeder
  noch offenen A-Deadline und der neu aus B entstandenen Deadline. B darf eine
  bereits entstandene A-Anforderung daher nie verkürzen; A-Werte dürfen nur
  den bereits entstandenen Rest-Tail verlängern und niemals B aktiv planen.
- Startet B und endet es vor einer noch offenen A-Deadline, bleibt der
  betroffene Lüfter bis zu dieser A-Deadline eingeschaltet. Erst wenn keine
  gespeicherte Deadline und keine bestehende Mindest-Auszeit-/Totzeitbindung
  mehr offen ist, darf die vorhandene Abschaltlogik den Lüfter ausschalten.
- Ohne aktive Bindung und ohne present Snapshot liefert der Planner
  fail-closed `Idle`/`Unconfigured`, darf aber bereits laufende Teardown-
  Ausgänge bis zum Ende ihres gespeicherten Nachlaufs weiter ausgeben. Daraus
  wird keine `ActuatorSafetyGateStatus::Allowed`-Freigabe abgeleitet.
- `TemperatureControlApplicationOrchestrator` ruft `beginRun()` erst nach
  bestätigtem Fresh-Start-Commit mit present Snapshot bzw. erfolgreicher
  Recovery-Activation mit present Snapshot auf. Bei absent bleibt der Planner
  ungebunden.
- Derselbe Orchestrator räumt aktive Bindung, Feedback und offene Evaluationen
  an allen bereits kanonischen Run-Ende-, Stop-, Fault-, Standby-, Completion-
  und Recovery-Abbruch-Boundaries auf. `forceStop` erfolgt am Run-Ende vor
  `endRun()`; reine Teardown-/Safety-Evidenz bleibt davon unberührt.
- Zwei aufeinanderfolgende Runs müssen mit demselben Planner-Objekt und zwei
  unabhängigen Kopien sicher funktionieren; kein alter aktiver Snapshot darf
  B planen, aber ein erforderlicher A-Tail und A-Deaktivierungsanker müssen
  bis zu ihrem Abschluss weiterwirken.

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
   - Snapshotbildung aus der bestehenden Runtime-Lease vor Candidate-Write,
     reine absent/present-Provenienz und keine RAM-/Planner-Anwendung vor
     `Applied`.
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

- Schema-6-Program- und Manual-Run ohne Snapshot dürfen logisch und persistent
  starten; ihre Recovery bleibt actor-free und lädt keine Live-Servicewerte.
- Ein behaupteter present-Snapshot ohne gültige Herkunft aus der bestehenden
  Runtime-Lease wird vor `Applied` abgelehnt; es gibt keinen zweiten
  Startpfad und keinen `persistCommand()`-Bypass.
- Schema-5-Active-Run ohne Snapshot bleibt als actor-free Run lesbar und
  rekonstruierbar; Schema-5-NoActiveRun bleibt lesbar.
- Corruption, fehlendes Feld, falsche Variante, fremde Epoch und unbekanntes
  neueres Schema bleiben fail-closed. Ein fehlender Snapshot ist dabei nur im
  expliziten actor-free Vertrag gültig, nicht als beschädigter `present`-
  Snapshot.
- Write-Abbruch vor, während und nach dem Commit lässt keinen neuen RAM-Run
  und keine Planner-Bindung zu; die bestehende Head-/Fallback-Evidence bleibt
  maßgeblich.
- Run A mit Servicewerten A behält A trotz nachfolgender Serviceänderung B;
  der nächste neue Run erhält B; Recovery von A ignoriert Live-B.
- Fresh-Start mit altem `NoActiveRun`-Fallback trägt im Current den neuen
  present-Snapshot und im Fallback keinen Snapshot.
- Current und Fallback derselben Run-Identität tragen auch bei
  unterschiedlichen Run-Revisionen byte-identisch denselben immutable
  Snapshot; ein Fallback eines anderen älteren Runs trägt dessen eigenen
  Snapshot und `NoActiveRun` trägt keinen Snapshot.
- Es wird kein direkter Aktor-/GPIO-Wert persistiert.

### 9.3 Planner-Lebenszeit und Application Boundary

- `beginRun()` erfolgt bei present Snapshot erst nach bestätigtem `Applied` und
  nicht nach einem bloßen Candidate-/Write-Aufruf. Ein absent-Run bleibt ohne
  aktive Planner-Bindung zulässig.
- Persistenzfehler, ein present-Snapshot ohne gültige Provenienz und ungültige
  Parameter lassen den produktiven Planner-/Aktorpfad fail-closed; ein
  actor-free Run bleibt logisch/persistent verfügbar. Die eigentliche
  Aktorfreigabe prüft weiterhin ausschließlich #24/`ActuationInterlock`.
- Derselbe Planner überlebt zwei Runs, kopiert beide Snapshots unabhängig und
  erhält nach Run A dessen laufenden Fan-Tail sowie
  Deaktivierungszeit/-richtung über `endRun()` hinweg.
- Start ohne Snapshot, present-Start ohne gültige Provenienz und Recovery ohne
  Snapshot werden als getrennte Tests ausgeführt; Recovery lädt niemals
  Live-Servicewerte.
- Rebind während eines aktiven Runs, Lease-/State-Pointer und Mid-Run-
  Konfigurationsänderung werden zurückgewiesen bzw. bleiben wirkungslos.
- Run-Ende führt über die bestehende zentrale Boundary zu `forceStop` und
  danach `endRun`; der Test verwendet einen A-Nachlauf, der länger als der
  B-Nachlauf ist, startet Run B während dieses A-Tails und lässt B vor der noch
  offenen A-Deadline enden. Er prüft kontinuierliche Lüfterausgabe, B-Planung
  sowie, dass der betroffene Lüfter nicht vor der A-Deadline abschaltet. Eine
  B-Deadline darf eine längere offene A-Deadline nicht verkürzen.
- Stop, Fault, Standby, Completion und Recovery-Abbruch behalten ihre
  bestehende zentrale Boundary; nur ein tatsächlich persistiertes Run-Ende
  beendet die aktive Run-Bindung.
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
2. Ein neuer Run ohne Snapshot darf logisch und persistent starten; ein
   present-Start aus gültiger Runtime-Lease persistiert genau einen
   vollständigen, validierten Planner-Snapshot vor `NewActiveRun`/RAM-
   `Applied`. Snapshot-Provenienz erteilt keine Aktorfreigabe.
3. Schema 6 friert `absent=actuation-unconfigured` und
   `present=gebundener Snapshot` ein; kein Start-API- oder
   `persistCommand()`-Bypass kann einen present-Snapshot ohne gültige
   Runtime-Provenienz persistieren.
4. Jeder Current-/Fallback-Kandidat ist für sich vollständig: Current und
   Fallback derselben Run-Identität tragen unabhängig von unterschiedlichen
   Run-Revisionen byte-identisch denselben immutable Snapshot (oder beide
   keinen), `NoActiveRun` trägt keinen Snapshot, und ein anderer älterer Run
   trägt seinen eigenen Snapshot.
5. Recovery verwendet bei present ausschließlich den persistierten Snapshot,
   bei absent niemals eine inzwischen geänderte Live-Servicekonfiguration und
   bleibt dann actor-free.
6. Der langlebige Planner besitzt eine wertkopierende, ausführbare
   `beginRun`/`endRun`-Lebenszeitbindung ohne Dangling- oder Mid-Run-Referenzen;
   Fan-Nachläufe und physische Deaktivierungsanker überleben `endRun()`, und
   überlappende Teardown-Anforderungen verwenden je betroffenem Lüfter die
   spätere noch offene absolute Deadline. Run B kann einen A-Tail nicht
   verkürzen und plant aktiv ausschließlich mit B.
7. Keine Produktivparameter, Grenzwerte, Defaults, GPIO-/Hardwarewerte oder
   `ActuatorSafetyGateStatus::Allowed`-Freigabe werden durch #106 erfunden oder
   aktiviert. #106 entscheidet nur Snapshot-Provenienz; #24/
   `ActuationInterlock` bleibt alleinige Aktorfreigabe und #35 alleiniger Owner
   produktiver Werte und Grenzen.
8. Keine Display-/Touch-Datei, kein PR-#156-Inhalt und keine neue allgemeine
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
NEXT_GATE=INDEPENDENT_PLAN_FIX_VERIFICATION
```

Der PR bleibt Draft. Nach der Übergabe dieses exakten Plan- und HEAD-Nachweises
wird für die Independent Plan Fix Verification angehalten; weder Verification
noch Owner-Freigabe werden durch den Builder vorweggenommen.
