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

Jeder logisch startbare Run wird als `ActorFree` oder als
`ProductiveActorRun` über dieselbe bestehende Startgrenze klassifiziert. Ein
`ActorFree`-Run darf ohne Planner-Snapshot logisch und persistent starten; er
bleibt aktor-unberechtigt. Ein `ProductiveActorRun` erhält genau einmal einen
vollständigen, validierten `ActuatorPlannerParameters`-Snapshot aus der zu
diesem Start gelesenen Servicekonfiguration. Dieser Snapshot wird vor
`NewActiveRun` atomar im bestehenden Run-Checkpoint gespeichert, nach einem
Neustart ausschließlich aus dem gespeicherten Run wiederhergestellt und für
die aktive Run-Lebenszeit in den langlebigen `ActuatorPlanner` kopiert.
Änderungen der Servicekonfiguration wirken erst auf einen späteren Run.

Der Ablauf muss für Program- und Manual-Run identisch gelten:

```text
eine einzige Fresh-Start-API mit owner-erzeugter ActuationAdmission
-> ActorFree: Run-Kandidat ohne Planner-Snapshot bilden
-> ProductiveActorRun: RuntimeConfigurationReadLease erwerben
-> bei ProductiveActorRun aktuellen Service-Snapshot lesen und validieren
-> Run-Kandidat mit absent/present Planner-Snapshot bilden
-> bestehende #17-Run-Transaktion atomar persistieren/verifizieren
-> erst nach bestätigtem Commit NewActiveRun/RAM-State publizieren
-> nur bei present Snapshot und ProductiveActorRun den Planner binden
```

Bei Recovery gilt ausschließlich:

```text
Run-Head/Current/Fallback laden
-> jeden Kandidaten für sich vollständig validieren
-> present Snapshot rekonstruieren oder absent als ActorFree klassifizieren
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
| Bildung des immutable Per-Run-Snapshots | bestehender Application-/Startpfad im `fermentation_app` | genau einmal am kanonischen New-Active-Run-Punkt für `ProductiveActorRun` aus einer gültigen Runtime-Lease; `ActorFree` bleibt absent |
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
strukturell ehrlicher, actor-free Zustand. Der logisch/persistent startbare
`ActorFree`-Pfad darf ihn nicht durch Nullen, lokale Defaults, letzte Livewerte
oder eine andere Servicekonfiguration ersetzen. Ein als
`ProductiveActorRun` zugelassener Start wird ohne gültigen Snapshot vor
`Applied` abgelehnt. Erst Issue #35 kann eine gültige produktive Konfiguration
liefern.

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

- bei einem als `ProductiveActorRun` zugelassenen Program- oder Manual-Run vor
  der Persistenz in den Kandidaten kopiert;
- bei einem `ActorFree`-Run absent bleibt und keinen Planner bindet;
- bei jeder normalen laufenden Run-Mutation unverändert mitgeführt;
- beim Stop/NoActiveRun gelöscht;
- bei Recovery aus dem ausgewählten Current- oder Fallback-Payload
  wiederhergestellt;
- niemals während eines aktiven Runs aus `ConfigurationService` aktualisiert.

Ein aktiver Run ohne dieses Feld ist ein gültiger, actor-free Run, aber nicht
produktiver Aktorplanung zugänglich. Recovery darf daraus keinen
Planner-Snapshot ergänzen. Ein `NoActiveRun`-Snapshot darf kein
Planner-Snapshot-Feld tragen.

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
  persistierte `ActorFree`-/`actuation-unconfigured`-Zustand;
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
- Jeder Current-/Fallback-Kandidat wird für sich vollständig geprüft:
  dieselbe aktive Run-Identität und dieselben Run-Revisionen müssen denselben
  immutable Snapshot tragen; ein `NoActiveRun`-Fallback trägt keinen
  Snapshot; ein Fallback eines anderen älteren Runs trägt dessen eigenen
  Snapshot. Kein Kandidat wird aus Current oder Live-Konfiguration ergänzt.

## 6. Fresh-Start-Transaktion: Write-before-Apply

Der vorhandene `persistFreshStartCommand`-Pfad bleibt die einzige
Anwendungsgrenze für jeden neuen Program- und Manual-Run. Die Implementierung
muss dafür eine owner-erzeugte, nicht caller-fälschbare
`FreshStartActuationAdmission` oder eine semantisch identische Typgrenze
herstellen:

```text
persistFreshStartCommand(
    current,
    startDecision,
    FreshStartActuationAdmission admission,
    time,
    liveSensorEvidence)
```

Die Admission ist kein zweiter Lifecycle-State und keine zweite Start-API. Sie
hat genau zwei Ergebnisse:

```text
ActorFree
ProductiveActorRun + gültige RuntimeConfigurationReadLease
```

`ActorFree` wird durch die bestehende Application-/Safety-Komposition für
uncommissioned bzw. nicht aktorberechtigte Runs erzeugt. `ProductiveActorRun`
ist eine owner-erzeugte, bereits gegen die vorhandene #35-/Safety-Evidenz
geprüfte Capability; ein frei gesetztes Caller-Bool genügt nicht. Die
Admission enthält beim produktiven Ergebnis die konsistente Runtime-Lease
oder den daraus unveränderlich abgeleiteten Start-Snapshot.

Die Methode muss:

1. ausschließlich `StartProgram` und `StartManualHolding` akzeptieren;
2. `ActorFree` ohne Runtime-Lease als logisch/persistent startbar behandeln,
   den Snapshot absent lassen und keine Planner-Bindung vorbereiten;
3. bei `ProductiveActorRun` eine gültige, nicht abgelaufene Runtime-Lease und
   den darin enthaltenen `ServiceConfiguration`-Snapshot verlangen;
4. bei `ProductiveActorRun` den vollständigen Planner-Snapshot wertkopierend
   in den Run-Kandidaten übernehmen;
5. `classifyActuatorPlannerParameters()` und alle bestehenden
   Run-Kandidaten-/Plausibilitätsprüfungen vor dem Schreiben ausführen;
6. die bestehende `RunPersistenceCoordinator`-Transaktion benutzen, wobei
   der absent/present Snapshot vor `makeRunPersistenceSnapshot()` im
   Kandidaten liegt;
7. `ProductiveActorRun` ohne gültigen present Snapshot vor `Applied`
   ablehnen; dies gilt auch dann, wenn ein Caller versucht, die normale
   `persistCommand()`-Methode zu verwenden;
8. bei Candidate-, Slot-, Head-, Verify-Fehler oder unbestimmtem Write-Ergebnis
   weder RAM noch Planner binden;
9. erst nach `Applied` die bestehende `applyRunCommand`-/RAM-Änderung sichtbar
   machen und nur bei present Snapshot den Planner binden.

Dafür darf die bestehende `RunPersistenceCoordinator`-Klasse einen
start-spezifischen, streng typisierten Admission-/Snapshot-Handoff an ihre
vorhandene Persistenzroutine erhalten. Das ist keine zweite Transaktion:
Candidate-Apply, `writeSnapshotCore`, Head-Commit, Verify und bestehende
Rollback-/Failure-Semantik bleiben zentral im selben Coordinator. Ein
beliebiger Caller darf weder einen Productive-Admission-Status noch ein
produktives Parameterobjekt in `persistCommand()` einschleusen. Alle
Fresh-Start-Kommandos außerhalb dieser einen API werden abgewiesen.

Die zulässige Reihenfolge ist:

```text
Admission bestimmen
-> ActorFree: Run-Kandidat ohne Snapshot bilden
-> ProductiveActorRun: Lease-Snapshot A lesen und validieren
-> absent/present Run-Kandidat atomar schreiben/verifizieren
-> Commit bestätigen
-> RunState anwenden
-> nur bei present Snapshot Planner mit A binden
-> actor-free bleibt ohne produktive Aktorplanung
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
  Deaktivierungszeitpunkte und `lastPhysicalDeactivationDirection`/
  `lastPhysicalDeactivationAtMonotonicMillis`. Es wird keine zweite
  Lifecycle-Architektur angelegt.
- `forceStop()` beendet die aktive physische Planung und erzeugt bzw. erhält
  die vorhandenen äußeren/inneren Fan-Nachlaufanker. Erst danach darf
  `endRun()` die aktive A-Bindung löschen. Die für den laufenden Nachlauf
  nötige kopierte A-Parameterbasis bleibt bis zum Abschluss dieses
  Nachlaufs verfügbar.
- `endRun()` löscht weder laufende Fan-Nachläufe noch
  `lastPhysicalDeactivationDirection`/-Zeit. Diese Evidenz bleibt erhalten,
  bis die vorhandenen Nachlauf- bzw. Mindest-Auszeit-/Totzeitberechnungen sie
  nicht mehr benötigen; danach wird die Tail-Kopie deterministisch freigegeben.
- Startet Run B während eines A-Nachlaufs, bleiben die betroffenen Lüfter
  kontinuierlich aktiv. Die aktive Planung, neue Fenster und ein späterer
  B-Teardown verwenden B; A wirkt nur auf den bereits entstandenen Tail.
  Ein späteres `forceStop()` von B ersetzt den alten Tail-Kontext durch die
  neue B-Teardownbasis und aktualisiert den physischen Deaktivierungsanker.
- Ohne aktive Bindung und ohne present Snapshot liefert der Planner
  fail-closed `Idle`/`Unconfigured`, darf aber bereits laufende Teardown-
  Ausgänge bis zum Ende ihres gespeicherten Nachlaufs weiter ausgeben. Daraus
  wird keine `ActuatorSafetyGateStatus::Allowed`-Freigabe abgeleitet.
- `TemperatureControlApplicationOrchestrator` ruft `beginRun()` erst nach
  bestätigtem Fresh-Start-Commit eines `ProductiveActorRun` bzw.
  erfolgreicher Recovery-Activation mit present Snapshot auf. Bei
  `ActorFree`/absent bleibt der Planner ungebunden.
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
   - Leasegebundene Snapshotbildung vor Candidate-Write für
     `ProductiveActorRun`, absent/present-Admission und keine RAM-/Planner-
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

- Actor-free Schema-6-Program- und Manual-Run ohne Snapshot dürfen logisch und
  persistent starten; ihre Recovery bleibt actor-free und lädt keine
  Live-Servicewerte.
- Ein als `ProductiveActorRun` zugelassener Start ohne present Snapshot wird
  vor `Applied` abgelehnt; es gibt keinen zweiten Startpfad und keinen
  `persistCommand()`-Bypass.
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
- Productive Fresh-Start mit altem `NoActiveRun`-Fallback trägt im Current den
  neuen Snapshot und im Fallback keinen Snapshot.
- Zwei Revisionen derselben Run-Identität tragen denselben immutable Snapshot;
  ein Fallback eines anderen älteren Runs trägt dessen eigenen Snapshot.
- Es wird kein direkter Aktor-/GPIO-Wert persistiert.

### 9.3 Planner-Lebenszeit und Application Boundary

- `beginRun()` erfolgt für `ProductiveActorRun` erst nach bestätigtem
  `Applied` und nicht nach einem bloßen Candidate-/Write-Aufruf. Ein
  `ActorFree`-Run bleibt ohne aktive Planner-Bindung zulässig.
- Persistenzfehler, ein fehlender Snapshot beim produktiven Admission und
  ungültige Parameter lassen den produktiven Planner-/Aktorpfad fail-closed;
  ein actor-free Run bleibt logisch/persistent verfügbar.
- Derselbe Planner überlebt zwei Runs, kopiert beide Snapshots unabhängig und
  erhält nach Run A dessen laufenden Fan-Tail sowie
  Deaktivierungszeit/-richtung über `endRun()` hinweg.
- Actor-free Start ohne Snapshot, produktiver Start ohne Snapshot und
  Recovery ohne Snapshot werden als getrennte Tests ausgeführt; Recovery lädt
  niemals Live-Servicewerte.
- Rebind während eines aktiven Runs, Lease-/State-Pointer und Mid-Run-
  Konfigurationsänderung werden zurückgewiesen bzw. bleiben wirkungslos.
- Run-Ende führt über die bestehende zentrale Boundary zu `forceStop` und
  danach `endRun`; der Test startet Run B während A-Nachlauf und prüft
  kontinuierliche Lüfterausgabe, B-Planung und A-only-Tailsemantik.
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
2. Ein neuer actor-free Program- oder Manual-Run darf ohne Snapshot logisch
   und persistent starten; ein als `ProductiveActorRun` zugelassener Start
   persistiert genau einen vollständigen, validierten Planner-Snapshot vor
   `NewActiveRun`/RAM-`Applied`.
3. Schema 6 friert `absent=actuation-unconfigured` und
   `present=gebundener Snapshot` ein; kein Start-API- oder
   `persistCommand()`-Bypass kann einen produktiven Run ohne Snapshot
   persistieren.
4. Jeder Current-/Fallback-Kandidat ist für sich vollständig: gleiche Run-
   Identität/Revisionen bedeuten denselben Snapshot, `NoActiveRun` bedeutet
   keinen Snapshot, und ein anderer älterer Run trägt seinen eigenen Snapshot.
5. Recovery verwendet bei present ausschließlich den persistierten Snapshot,
   bei absent niemals eine inzwischen geänderte Live-Servicekonfiguration und
   bleibt dann actor-free.
6. Der langlebige Planner besitzt eine wertkopierende, ausführbare
   `beginRun`/`endRun`-Lebenszeitbindung ohne Dangling- oder Mid-Run-Referenzen;
   Fan-Nachläufe und physische Deaktivierungsanker überleben `endRun()`.
7. Keine Produktivparameter, Grenzwerte, Defaults, GPIO-/Hardwarewerte oder
   `ActuatorSafetyGateStatus::Allowed`-Freigabe werden durch #106 erfunden oder
   aktiviert; diese bleiben von #35 beziehungsweise den bestehenden Safety-
   und Hardware-Gates abhängig.
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
