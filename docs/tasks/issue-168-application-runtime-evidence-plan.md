# Issue #168 – Application-owned Runtime-Evidence und UI-/Command-Projection

## Planstatus und Baseline

```text
ISSUE=168
SCOPE=APPLICATION_RUNTIME_EVIDENCE_AND_UI_COMMAND_PROJECTION
BASE_BRANCH=main
BASE_SHA=1f1755e5e706fb668472920545b5302fcef1df16
PREVIOUS_REVIEWED_PLAN_SHA=2aca6ce5ce8eb1c7ba6e37cf6a1ebd27a4e4cad1
PREVIOUS_INDEPENDENT_PLAN_REVIEW=REVISE
PRIOR_FIX_VERIFICATION_PLAN_SHA=ef875d8e22a72135fed25a8629949cfdfc762ace
PRIOR_FIX_VERIFICATION_RESULT=4_OF_5_CLOSED_1_BLOCKER_OPEN
PLAN_STATUS=DRAFT_OWNER_APPROVAL_REQUIRED
IMPLEMENTATION=NOT_STARTED
IMPLEMENTATION_AUTHORIZATION=NO
OWNER_SAFETY_SEMANTICS_DECISION=PASS
OWNER_DECISIONS_PENDING=0
CONSUMER=ISSUE_27_PR167_AFTER_ISSUE168_MERGE
ACTUATOR_RELEASE=NO
```

Dieser Plan ist der vorgelagerte, kleine Application-/Composition-Scope fuer
Issue #27. Er wird auf dem aktuellen kanonischen `main` erstellt
(`BASE_SHA` == aktueller `origin/main`). Die Umsetzung endet nach dem
Plan-Commit bis zur ausdruecklichen Freigabe genau dieses Plan-Commits.

Diese Revision behebt vier der fuenf Blocker aus dem Independent Plan Review
von `PREVIOUS_REVIEWED_PLAN_SHA=2aca6ce5ce8eb1c7ba6e37cf6a1ebd27a4e4cad1` und
integriert die vier verbindlichen Ownerentscheidungen aus dem
Provenienzkommentar `5772310484` in den verbleibenden Rest-Blocker aus der
gezielten Fix Verification auf
`PRIOR_FIX_VERIFICATION_PLAN_SHA=ef875d8e22a72135fed25a8629949cfdfc762ace`
(konkrete Pre-Command-Safety-Semantik). Der Scope selbst (Application als
alleiniger Runtime-Evidence-Owner vor #27) ist unveraendert richtig und bleibt
bestehen. `OWNER_SAFETY_SEMANTICS_DECISION=PASS` und
`OWNER_DECISIONS_PENDING=0` schliessen die vier Planentscheidungen; die
separate Ownerfreigabe dieser neuen exakten Plan-SHA bleibt erforderlich,
bevor Implementation beginnen darf.

## Anlass und Problem

`FermentationApplicationOwningEvidence`
(`lib/fermentation_app/src/fermentation_application.hpp:46-55`) ist im
aktuellen Stand ein reiner Funktionsparameter, der an alle `prepare*`-Methoden
uebergeben wird (`fermentation_application.hpp:118-142`,
`fermentation_application.cpp:223,292,321,357,394,431,504`). Er wird also
heute vom Caller befuellt, nicht application-intern aufgeloest. Diese
externen Aufrufer (Renderer, Touch-Client, Web-Adapter) duerfen weder Safety-
noch Sensor-, Planner- oder Recovery-Evidence behaupten oder zusammensetzen.

Der aktuelle `main` besitzt bereits die rendererunabhaengigen UI-Modelle und
den reinen `FermentationUiProjector`, aber noch keinen vollstaendigen
produktiven Runtime-Producer, der die Live-Sensor- und Safety-Evidence in der
`FermentationApplication` zusammenfuehrt. Insbesondere ist im aktuellen
`main` kein laufender DS18B20-Producer in `app_main` oder der Application-
Composition vorhanden; `device_platform::SensorQualityPipeline` hat aktuell
keinen produktiven Owner (Bestandspruefung: kein Treffer ausserhalb von
Tests). Deshalb muss der software-only Zustand ohne #30 echte Sensorwerte
sichtbar als nicht verfuegbar/ungueltig behandeln. Nullwerte, erfundene
Temperaturen und implizite Sensorfreigaben sind unzulaessig.

Ziel ist die kleinste Application-owned Composition/Handoff-Erweiterung, mit
der `FermentationApplication` die aktuelle Evidence selbst aufloest und
`FermentationUiSnapshot` sowie UI-/Web-Kommandos aus dieser kanonischen
Application-Quelle bedient. #27 soll diese Grenze nach dem Merge dieses
Vorgaengers konsumieren; #27 wird in diesem Scope nicht weiterimplementiert.

## Repository-first-Bestandsaufnahme

### Bereits vorhandene Owner und Vertraege

| Bereich | Kanonische Quelle auf `BASE_SHA` | Verwendung in diesem Scope |
|---|---|---|
| Run-/Prozess-/Recoveryzustand | `FermentationApplication::runtimeRunState_`, `RunPersistenceCoordinator`, `ConfigurationRecoveryService`, `ConfigurationService`, `PresentationState` und Lifecycle-Zustand | Application bleibt Owner der aktuellen Run-, Recovery-, Konfigurations- und Lebenszyklus-Sicht; dient auch als Quelle der Pre-Command-Evidence (siehe Zielvertrag). |
| Sensorwerte und Qualitaet | `device_platform::TemperatureReading` (`temperature_source.hpp:41-70`) -> `device_platform::ITemperatureSource::read()` (`temperature_source.hpp:94-107`) -> `device_platform::SensorQualityPipeline::ingest()/snapshot()` (`sensor_quality_pipeline.hpp:27,44,53`) -> `device_platform::SensorQualitySnapshot` (`sensor_quality_snapshot.hpp:27`) mit `SensorQuality::{Valid,Stale,Failed}` (`sensor_quality.hpp:13-17`) | Einzige produktive Raw-/Processed-Grenze; `fermentation_app` konsumiert ausschliesslich `SensorQualitySnapshot`, niemals `TemperatureReading` direkt (Bestandspruefung: kein Treffer). Kein paralleler Handoff. |
| Rollen und Plausibilitaet | `CrossRolePlausibilityContext` (`lib/fermentation_app/src/sensor_selection.hpp:34-42`) mit je einem `device_platform::SensorQualitySnapshot` fuer `air`/`product`/`cooling` (Z.37-39) | Bereits die kanonische, application-seitige Zusammenfuehrung der drei Rollen-Snapshots; wird in diesem Scope wiederverwendet, nicht neu erfunden. |
| Safety und post-commit Aktorfreigabe | `ActuationInterlock::evaluate()` (`actuation_interlock.hpp:74-75`, `actuation_interlock.cpp:221-230`), `ActuationEvidence` (`actuation_interlock.hpp:39-65`) | Bleibt alleinige tatsaechliche Aktorfreigabe (`ActuatorSafetyGateStatus`); wird strikt post-commit nach Persistenz-/Apply-Handoff ausgewertet (siehe Zielvertrag, Blocker 2). |
| Pre-Command-Safety-Evidence | Bestehende, von `ActuationEvidence` unabhaengige Felder `safetyAllowsStart`/`safetyAllowsCooling` und Sensorvaliditaet in `ProgramStartRequest`, `ManualStartRequest`, `StopRequest` und `CompletionRequest` (`run_commands.hpp:156-235`); die bisherigen Change-Signale in `RunAdjustmentCommandRequest`/`SensorSelectionCommandRequest` werden abgebaut | Start-/Cooling-Readiness wird application-intern aus den kanonischen Application-/Konfigurations-/Persistenzzustaenden aufgeloest; Sensorvaliditaet bleibt #20/#21; die Change-Entscheidungen verwenden ihre bestehenden State-/Revisions-/Fachregeln ohne Ersatz-Boolean. |
| Konfiguration und Revisionen | `ConfigurationService`, User-Configuration-/Lease-Vertraege, `RunCommandState` und vorhandene Revisionen | Snapshot und Kommandos lesen die vorhandenen Revisionen aus dem Application-Owner. |
| UI-Projektion | `FermentationUiProjectionInput`, `FermentationUiProjector`, `FermentationUiSnapshot` und `TemperatureView` | Projector bleibt pure Projektion; die Application liefert den kanonisch zusammengesetzten Input. |
| Fachkommandos und Confirmation | `FermentationApplication::prepareStartProgram`, `prepareStartManualHolding`, `prepareStartManualTimed`, `prepareStop`, `prepareCompletion`, `prepareEnvelope`, `FermentationApplicationPreparedRequest` (`fermentation_ui_commands.hpp:142-187`), `confirmPrepared()` (`fermentation_application.hpp:146-147`, `.cpp:534-544`) | Caller liefern nur Intent, Werte, Bestaetigung und erwartete Revisionen; Evidence wird intern aufgeloest, bei Confirm erneut (siehe Blocker 3). |
| Recovery-Evidence | `publishOwningRecoveryEvidence(const CrossRolePlausibilityContext&)` (`fermentation_application.cpp:1029-1031`), `owningRecoveryEvidence_` (Header Z.235), `resumeFallback()` (`.cpp:1034ff`, Verbrauch Z.1067-1078) | Bereits `CrossRolePlausibilityContext`-typisiert; wird konsolidiert, nicht dupliziert (siehe Blocker 4). |

### Abgrenzung zu abgeschlossenen oder spaeteren Scopes

Die bestehenden #20/#21-, #24-, #25/#26- und #144/#152-Vertraege werden
konsumiert, nicht neu entschieden. #119 bleibt der geschlossene historische
und nicht wiederzubelebende Composition-/StateStore-Pfad.

Der Scope enthaelt weder DS18B20-Treiber, GPIO-/ROM-/CRC-/Hot-Plug-
Nachweise noch reale Sensorhardware aus #30. Er enthaelt auch keinen
Display-/Touch-/Rendererpfad aus #31, keine Diagnose-/Service-/Exportlogik aus
#28, keine Werte-/Safety-Commissioning-Entscheidungen aus #35 und keine
produktive Per-Run-Aktoraktivierung aus #106.

## Zielvertrag

### BLOCKER 1 – Eine Application-seitige Runtime-Evidence-Wahrheit

Die einzige Application-seitige Runtime-Evidence-Wahrheit ist
`CrossRolePlausibilityContext`
(`lib/fermentation_app/src/sensor_selection.hpp:34-42`). Sie enthaelt
ausschliesslich bereits ausgewertete `device_platform::SensorQualitySnapshot`
je Rolle (`air`/`product`/`cooling`). Ein `TemperatureReading`-Handoff bleibt
vollstaendig innerhalb der `device_platform`-Producer-Grenze
(`ITemperatureSource -> SensorQualityPipeline`); `fermentation_app` und die
neue Application-owned Composition erhalten und verarbeiten niemals ein
rohes `TemperatureReading`. Es entsteht damit kein paralleler
`TemperatureReading`-Handoff neben dem `SensorQualitySnapshot`-Handoff; beide
sind dieselbe Kette an verschiedenen Stellen, nicht zwei Produktwahrheiten.

Erzeugender Owner: Auf `BASE_SHA` existiert **kein produktiver** Owner, der
`SensorQualityPipeline` instanziiert und `CrossRolePlausibilityContext`
befuellt (Bestandspruefung ausserhalb von Tests: kein Treffer). Das ist
korrekt und beabsichtigt, solange #30 nicht produktiv liefert.

`CrossRolePlausibilityContext` haelt `air`/`product`/`cooling` bereits als
`device_platform::SensorQualitySnapshot` **by value**, nicht optional
(`sensor_selection.hpp:37-39`); ein fehlender Producer wird also nicht durch
Abwesenheit des Felds dargestellt, sondern durch den bestehenden
Default-/Fail-safe-Zustand des Snapshots selbst: `identity == std::nullopt`
und `quality == SensorQuality::Stale` (Default-Member-Initialisierung,
`sensor_quality_snapshot.hpp:30,32`), alle Wertfelder `std::nullopt`. Dieselbe
by-value-`SensorQualitySnapshot`-Darstellung traegt bereits
`TemperatureView::quality` (`fermentation_ui_models.hpp:59`) und
`FermentationUiTemperatureInput::quality` (Z.129) — kein neuer UI-Zustand.
"Kein Producer vorhanden" und "Producer aktuell `Stale`" sind damit auf
Typebene bewusst identisch (fail-safe by construction); die
Application-owned Composition erfindet keinen dritten Zustand und keinen
Wert, sondern gibt genau diesen bestehenden Default unveraendert weiter, wenn
keine kanonische Producer-Quelle geliefert hat.

Auf der aeusseren Handoff-Ebene (Confirmation, Recovery) bleibt zusaetzlich
das bereits bestehende Muster erhalten, dass die gesamte zusammengesetzte
Evidence als `std::optional<CrossRolePlausibilityContext>` fehlen kann, bevor
ueberhaupt ein Aufloesungsversuch stattgefunden hat — analog zu
`owningPlausibility_` (`fermentation_ui_commands.hpp:186`) und
`owningRecoveryEvidence_` (`fermentation_application.hpp:235`).

Freshness/Stale/Failed wird direkt aus dem bestehenden
`SensorQuality`-Zustand `{Valid, Stale, Failed}`
(`sensor_quality.hpp:13-17`, Folge `VALID -> STALE -> FAILED`) je
`SensorQualitySnapshot` in `CrossRolePlausibilityContext` uebernommen; es
entsteht keine zweite Freshness-Klassifikation.

Evidence wird ungueltig und darf nicht mehr fuer einen spaeteren Command
verwendet werden, sobald: (a) eine neue `CrossRolePlausibilityContext`-Instanz
fuer denselben Aufruf aufgeloest wird (jeder Snapshot-/Command-Aufruf bildet
einen frischen aktuellen Kontext, siehe Einleitungssatz der bisherigen
Zielvertrag-Beschreibung), oder (b) `SensorQuality` fuer die betroffene Rolle
zwischenzeitlich auf `Stale`/`Failed` uebergegangen ist. Es gibt keinen
dauerhaft gespeicherten positiven `safetyAllowsStart`, `safetyAllowsCooling`
oder Sensorvaliditaets-Flag; diese Felder werden bei jeder Aufloesung neu
berechnet, nie zwischengespeichert und wiederverwendet. Das entfernte
`safetyAllowsChange` wird nicht durch einen gespeicherten Ersatz ersetzt.

### BLOCKER 2 – Pre-Command-Evidence getrennt von post-commit `ActuationInterlock::Allowed`

`ActuationInterlock::evaluate()` kann `ActuatorSafetyGateStatus::Allowed` erst
setzen, wenn `activationEvidenceComplete(...)` erfuellt ist
(`actuation_interlock.cpp:221-230,412-429`); das prueft am Ende
`evidence.processActivationApplied` (Z.429) – ein Feld, das laut Kommentar
(`actuation_interlock.hpp:37`) bewusst keinen persistierten Zustand vor dem
Apply-Handoff hat. `Allowed` ist damit strukturell erst nach erfolgreichem
Persistence-/Apply-Handoff erreichbar und fuer `ProgramStartRequest::
safetyAllowsStart` (`run_commands.hpp:162`), das bereits vor
`decideProgramStart()` benoetigt wird, zirkulaer. Dasselbe gilt fuer
`ManualStartRequest.safetyAllowsStart` (Z.171), `StopRequest/
CompletionRequest.safetyAllowsCooling` (Z.187,200) und
`RunAdjustmentCommandRequest/SensorSelectionCommandRequest.
safetyAllowsChange` (Z.210,235).

Getrennt werden:

1. **Pre-Command-/Decision-Evidence** fuer `safetyAllowsStart`,
   `safetyAllowsCooling` und Sensorselection. Sie entsteht ausschliesslich
   application-intern; sie darf
   `activationPersistenceResult` und `processActivationApplied`
   (`actuation_interlock.hpp:53-54`, nur gelesen in
   `activationEvidenceComplete()` Z.412-429, in Produktionscode nirgends
   gesetzt) nicht konsultieren, da diese Felder strukturell erst nach dem
   Apply-/Persistenz-Handoff sinnvoll befuellbar sind.
2. **Post-commit `ActuationInterlock`** bleibt unveraendert die alleinige
   tatsaechliche Aktorfreigabe (`ActuatorSafetyGateStatus`). Sie wird strikt
   nach dem Persistenz-/Apply-Handoff ausgewertet und niemals durch
   Pre-Command-Evidence ersetzt oder vorweggenommen.

#### Bereits durch bestehende Vertraege geklaertes Teilstueck: Sensorvaliditaet

Fuer `ProgramStartRequest` ist die Sensorrollen-Vorbedingung bereits
vollstaendig entschieden und **nicht** Teil von `safetyAllowsStart`:
`decideProgramStartInto()` prueft `!request.safetyAllowsStart`
(`run_commands.cpp:717-720`) als eigenes, unabhaengiges Gate und **separat**
`!request.airSensorValid || !request.coolingSensorValid`
(`run_commands.cpp:729-735`, Kommentar woertlich: *"#21, 6.5: Vorbedingung
fuer jede Zeile der Startmatrix - gilt unabhaengig von SensorPreference und
angefordertem Modus, kein Sonderfall pro Zeile."*); `productSensorValid`
fliesst separat in `resolveProgramStartSensorMode()` ein. Diese drei
`*SensorValid`-Felder werden in diesem Scope application-intern aus der
aktuell aufgeloesten `CrossRolePlausibilityContext` (Blocker 1) bestimmt:
`airSensorValid`/`coolingSensorValid`/`productSensorValid` = `quality ==
device_platform::SensorQuality::Valid` fuer die jeweilige Rolle
(`air`/`cooling`/`product`). Das ist eine reine Uebernahme der bereits
akzeptierten #21-Semantik in die interne Aufloesung, keine neue Regel, und
schliesst dieses Teilstueck vollstaendig.

#### Ownerentscheidungen integriert: Readiness, Cooling, Change und Fault-Reset

Die verbindliche Ownerentscheidung aus
`OWNER_DECISION_PROVENANCE=PR-169-COMMENT-5772310484` wird in diesem Plan
konkret umgesetzt. Sie ersetzt keine bestehende Safety-State-Machine und
vermischt keine Pre-Command-Evidence mit der post-commit-
`ActuationInterlock`-Freigabe.

##### `safetyAllowsStart`: schmales Application-Readiness-Gate

`safetyAllowsStart == true` darf die Application nur ableiten, wenn alle
folgenden bereits vorhandenen Application-/Boot-/Lifecycle-Zustaende fuer den
aktuellen Aufruf gueltig sind:

1. `FermentationApplication::lifecycleState_` ist
   `ApplicationLifecycleState::Ready`; ein Initializing- oder
   `ServiceRequired`-Zustand ist fail-closed.
2. Der bestehende Konfigurationsowner ist vertrauenswuerdig: eine aktuelle
   `ConfigurationService::acquireRuntime()`-Abfrage liefert
   `RuntimeConfigurationReadStatus::RuntimeLeaseGranted`, und
   `runtime.lease.get().storageEpoch()` entspricht der aktuellen
   Application-`storageEpoch_`. Der Plan fuehrt dafuer keinen neuen
   dauerhaft gespeicherten `ConfigurationRecoveryStatus` und keine neue
   Safety-Kopie des Bootresultats ein. Andere Ergebnisse von
   `acquireRuntime()` (`RuntimeReadLeaseBusy`,
   `ConfigurationRuntimeUnavailable`) sind fail-closed.
3. Die vorhandenen Application-Kopien `persistenceLoadStatus_` und
   `loadDisposition_` sowie `RunPersistenceCoordinator::state()` muessen
   einen bereits bestehenden, vertrauenswuerdigen Nicht-Safe-Boot-Zustand
   darstellen. Akzeptierte Load-Status sind exakt
   `NoPersistedRun`, `NoActiveRun`, `Current` und `FallbackRecovered`, jedoch
   nur wenn die bestehende `boot_classification::classifyRunLoad()`-Auswertung
   nicht `RunLoadDisposition::SafeBoot` ergibt. Abgelehnt werden exakt
   `PreparedInterrupted`, `NotReconstructible`,
   `NotReconstructibleOrphanedState`, `ReadFailed`, `CapacityExceeded`,
   `UnsupportedSchema`, `ForeignEpoch` und `AlreadyInitialized` sowie jeder
   unbekannte/unklare Wert. Akzeptierte Coordinator-Zustaende sind exakt
   `ReadyEmpty`, `Ready` und `LoadedActiveRun`; `Uninitialized`, `Busy`,
   `BlockedIndeterminate`, `FallbackRecoveryPending` und
   `PersistenceCommittedApplyFailed` werden abgelehnt.
4. Auf der Run-Command-Ebene ist die bestehende
   `RunCommandState::criticalSafetyEventPending`-Invariante massgeblich. Fuer
   den normalen Start-/Cooling-Readinesspfad muss sie `false` sein; es wird
   keine zusaetzliche Safety-Projektion aus einem anderen Owner konsultiert.

Dieses Readiness-Gate enthaelt ausschliesslich Application-, Boot-,
Konfigurations-, Run-Persistenz- und kritische Safety-Bereitschaft. Es
enthaelt **nicht** Sensorrollen/-qualitaet (#20/#21), Planner-/Aktorfreigabe,
`activationPersistenceResult`, `processActivationApplied`, konkrete
thermische/#35-Grenzen oder produktives
`ActuatorSafetyGateStatus::Allowed` aus #106. Die bestehenden
`airSensorValid`, `coolingSensorValid` und `productSensorValid` werden
separat aus `CrossRolePlausibilityContext` geprueft.

Die vorhandenen Zustands- und Lease-Owner werden fuer die Ableitung
verwendet. `PresentationState`/Diagnoseprojektionen sind keine neue
Safety-Entscheidungsquelle; ein direkter `ActuatorPlanner`-/Watchdog-Latch
ist kein Bestandteil dieses Gates; `ActuationInterlock::evaluate()` wird
nicht fuer das Pre-Command-Gate aufgerufen. Watchdog-/Aktor-Safety bleibt am
bestehenden #24-Ownerpfad und wird post-commit durch den Interlock
durchgesetzt. Es werden keine neuen globalen Flags, keine parallele
Readiness-Wahrheit und keine neue Readiness-/Safety-State-Machine eingefuehrt.

##### `safetyAllowsCooling`: gleiche Grundbereitschaft, nur fuer Cooling-Start

`safetyAllowsCooling` verwendet exakt dieselbe grundlegende Application-
Readiness aus dem vorherigen Abschnitt und wird nur ausgewertet, wenn die
angeforderte Mutation tatsaechlich einen neuen Cooling-Run startet:

- `StopOption::AbortAndCool` konsultiert das Gate;
- Completion mit `startCooling=true` konsultiert das Gate;
- `StopOption::AbortAndTurnOff` darf niemals von diesem Gate abhaengen;
- Completion ohne Cooling darf niemals von diesem Gate abhaengen.

Air-/Cooling-Sensorvaliditaet bleibt separat #20/#21. Konkrete thermische
Grenzwerte bleiben #35; eine produktive Aktorfreigabe bleibt #106. Das Gate
ist nur Pre-Command-Readiness und kein Ersatz fuer den post-commit-
`ActuationInterlock`.

##### `safetyAllowsChange`: vollstaendiger Abbau des generischen Signals

Das generische externe Zusatzsignal `safetyAllowsChange` wird entfernt; es
gibt keine Ersatz-Boolean und keine neue generische Change-Safety-Policy.
Massgeblich bleiben die bereits bestehenden Regeln:

- `RunAdjustment`-State-, Revisions- und Fachregeln in
  `ActiveRun::decideAdjustment()`;
- die #21-Sensorselection-Matrix und ihr
  `criticalSafetyEventPending`-Verhalten;
- aktuelle Plausibilitaets-/Sensorevidenz;
- Confirmation-, Persistenz- und Exactly-once-Semantik.

Repository-first direkt betroffene Produktionsstellen fuer den gezielten
Vertragsabbau sind `FermentationApplicationOwningEvidence` und die
Passthrough-Zeilen in `fermentation_application.{hpp,cpp}`,
`RunAdjustmentCommandRequest`/`SensorSelectionCommandRequest` und die
zugehoerigen Entscheidungen in `run_commands.{hpp,cpp}` sowie der
`FermentationUiCommandBridge` in `fermentation_ui_commands.{hpp,cpp}`.
`RunAdjustmentContext::safetyAllowsChange` in `run_snapshot.hpp/.cpp` ist
ebenfalls kein neuer Owner; der bestehende interne Aufruf wird auf die
kanonischen Run-Adjustment-State-/Fachregeln reduziert, ohne eine neue
Boolean einzufuehren. Die bestehende `criticalSafetyEventPending`-Matrix
wird nicht abgeschwaecht.

Direkt betroffene native Tests/Harnessstellen sind die
`safetyAllowsChange`-Fixtures und Erwartungen in
`test/test_run_commands/test_run_commands.cpp`,
`test/test_run_snapshots/test_run_snapshots.cpp`,
`test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp`,
`test/test_issue144_run_identity/test_issue144_run_identity.cpp`,
`test/test_local_touch_ui/test_local_touch_ui.cpp` und
`test/test_fermentation_ui_commands/test_fermentation_ui_commands.cpp`.
Weitere Treffer in den bestehenden NVS-/Smoke-Harnesses werden nur
angepasst, wenn sie die geaenderte Produktionssignatur direkt kompilieren;
keine Testhilfe darf ein externes Safety-Signal als neue Wahrheit behalten.

##### Fault-Reset: kein generischer R1-Fault-Clearer

Der bestehende generische `FaultResetEvaluation`-/`FaultResetRequest`-
Vertrag ist fuer R1 redundant: Im Repository wird keine kanonische
Berechnung seiner Felder vorgenommen. Er wird daher aus der
Application-/UI-Command-Grenze und dem generischen Run-Command-Vertrag
entfernt, statt `allowed`, `authorizationSatisfied` oder eine neue
PIN-/Berechtigungslogik zu erfinden. Betroffen sind
`fermentation_application.{hpp,cpp}`, `run_commands.{hpp,cpp}`,
`fermentation_ui_commands.{hpp,cpp}` sowie der bestehende
`FermentationUiResetFaultIntent`-/Touch-Workspace-Bridgepfad und die direkt
zugehoerigen nativen Tests.

Der eine rendererunabhaengige R1-Bedienpfad bleibt der bestehende
`FermentationUiResetFaultIntent` mit
`FermentationUiAction::ResetFault`. Sein interner Application-Bridgevertrag
wird auf einen schmalen, nicht generischen Watchdog-Reset-Request reduziert;
`FaultResetEvaluation` und `FaultResetRequest` werden nicht ersetzt. Die
Repository-Bestandspruefung ergibt jedoch, dass auf `BASE_SHA` weder
`FermentationApplication` noch `main/app_main.cpp` einen produktiven
`ActuatorPlanner`-/Watchdog-Owner binden. Solange dieser Owner im aktuellen
Compositionpfad fehlt, liefert die Application fuer diesen Intent bereits bei
Prepare typisiert `FermentationApplicationRequestStatus::Unavailable`.
Prepare/Confirm erzeugen dann weder `ActuationEvidence` noch irgendeine
Planner-/Aktor-Composition. Ist ein kanonisch gebundener Owner spaeter
vorhanden, prueft die Application die vorhandenen
`expectedStateSequence`-/`expectedFaultRevision`-Werte des bestehenden
`CommandEnvelope` gegen den aktuellen `RunCommandState`-Owner und loest erst
dann fuer Confirm frische `ActuationEvidence` auf. Eine fehlende
Bestaetigung oder stale Revision wird mit den bestehenden typisierten
UI-/Command-Ergebnissen abgelehnt.

`#168` konstruiert dafuer keinen `ActuatorPlanner`, keinen
`TemperatureControlApplicationOrchestrator`, keinen Aktorsink und keine
produktiven Plannerparameter. Es fuehrt auch keinen neuen Planner-Port,
Provider, Service-Locator oder zweiten Safety-Owner ein.

Bei bestaetigtem, aktuellem Intent und bereits vorhandenem kanonischem
Planner-Owner ruft `FermentationApplication` ausschliesslich den bestehenden
#24-Ownerpfad
`ActuationInterlock::resetRequestWatchdog(...)` auf. Dieser prueft frische
gueltige `ActuationEvidence` und ruft ausschliesslich
`ActuatorPlanner::applyExternalWatchdogFaultReset(...)` auf. Dessen `true`
ist der einzige mutierende Reset-Erfolg und wird als typisiertes
`OwningOutcome` mit bestehendem `CommandStatus::Applied` projiziert. Dessen
`false` wird fail-closed als bestehendes typisiertes
`CommandStatus::SafetyRejected` projiziert; ist der Application-Owner vor dem
Aufruf nicht verfuegbar, wird bereits bei Prepare das bestehende
`FermentationApplicationRequestStatus::Unavailable` geliefert. Es gibt keine
positive Resetannahme aus UI-Daten.

Der Watchdog-Reset erzeugt keine Run-Persistence-Mutation: kein Aufruf von
`RunPersistenceCoordinator`, kein Fortschreiben von `faultRevision`, kein
`criticalSafetyEventPending`-Clear durch den UI-Bridge und kein zweiter
Watchdog-Latch. Die vorhandene CommandId-/Confirmation-/Revision-Semantik
dient nur der aktuellen UI-Operation und ihrer erneuten Pruefung; der Erfolg
ist ausschliesslich die Rueckgabe des Watchdog-Ownerpfads. Ack/Mute bleiben
vollstaendig getrennt.

Nur dieser Ownerpfad darf den Watchdog-Fault mutieren. Configuration-,
Persistence- und Sensorfehler werden ausschliesslich durch ihre
Producer-/Revalidierungspfade gesund. Kein Service-PIN und keine neue
Authorization-Schicht werden
eingefuehrt; `authorizationSatisfied` wird nicht als PIN-/Berechtigungs-
plattform interpretiert.

### Downstream-Abhaengigkeit fuer spaetere produktive Aktivierung

Die spaetere Bindung desselben `FermentationUiResetFaultIntent` an einen
produktiven Planner-Owner ist ausdruecklich an den bestehenden offenen
Composition-Scope von #106 gebunden. #106 bleibt das Gate fuer produktive
Plannerbindung, Per-Run-Parameterbindung und produktives
`ActuatorSafetyGateStatus::Allowed`; #35 bleibt Owner der dafuer benoetigten
realen Parameter und Grenzen. Sobald #106 den kanonischen Planner-Owner im
Composition-Root tatsaechlich bereitstellt, darf der bestehende Intent an
genau diesen Owner angeschlossen werden. Die Mutation bleibt dann
ausschliesslich
`ActuationInterlock::resetRequestWatchdog(...)` ->
`ActuatorPlanner::applyExternalWatchdogFaultReset(...)`.

Das ist keine neue Resetsemantik in #106 und keine Umsetzung von #106 in
PR #169. Falls #106 seinen Scope dafuer nur dokumentarisch ergaenzen muss,
bleibt dies eine explizite Downstream-Abhaengigkeit dieses Plans.

Die bestehenden Watchdog-Tests in
`test/test_actuation_interlock/test_actuation_interlock.cpp` und
`test/test_actuator_planner/test_actuator_planner.cpp` bleiben die Owner-
Regressionen fuer den tatsaechlich erfolgreichen Watchdog-Reset. Im
`#168`-Scope pruefen Application-/UI-/Touch-Tests den ungebundenen
Compositionpfad: `ResetFault` liefert `Unavailable`, ohne Mutation und ohne
Planner-/Aktor-Composition; Ack/Mute bleiben unabhaengig. Generische
`decideFaultReset()`-Tests und die generische RunCommand-Variante werden
entfernt. Ein erfolgreicher Application-End-to-End-Watchdogpfad wird erst im
produktiven Composition-Scope gebunden, nicht in #168 vorgezogen.

Damit sind die vier Ownerentscheidungen vollständig in den Plan übersetzt;
die betroffenen `prepare*`-Pfade sind nicht mehr wegen ungeklärter
Owner-Semantik blockiert. Ihre Umsetzung bleibt dennoch bis zur Freigabe
dieser neuen exakten Plan-SHA ausgeschlossen.

Direkter Testvertrag (soweit bereits durch bestehende Semantik entschieden):

- Ein fachlich zulaessiger Fresh Start darf vorbereitet und entschieden
  werden (`decideProgramStart()` liefert eine positive Entscheidung), wenn
  das neue Readiness-Gate positiv ist, waehrend die post-commit
  `ActuationInterlock`-Aktorfreigabe noch `Unresolved` ist. Reale Freigabe
  entsteht erst nach Persistenz/Apply/Interlock. `ACTUATOR_RELEASE=NO` bleibt
  davon unberuehrt, da dieser Scope keine Aktorfreigabe einfuehrt.
- Positive und negative Application-Readiness werden fuer Start und einen
  tatsaechlichen Cooling-Start geprueft. `AbortAndCool` und Completion mit
  `startCooling=true` weisen bei fehlender Readiness ab.
- Ein sicherer Stop/Completion ohne Cooling (`StopOption` ungleich
  `AbortAndCool` bzw. ohne `startCooling`) konsultiert `safetyAllowsCooling`
  nicht und wird dadurch nicht blockiert (`decideStop()`,
  `run_commands.cpp:970-973`, und der analoge Completion-Pfad
  `run_commands.cpp:1061`).
- `airSensorValid`/`coolingSensorValid`/`productSensorValid` je Rolle positiv
  und einzeln negativ (`SensorQuality::Valid` vs. `Stale`/`Failed`) fuer
  Start; `air`/`cooling` sind Pflicht, `product` beeinflusst nur
  `resolveProgramStartSensorMode()`.
- Nach Entfernung von `safetyAllowsChange` bleiben Run-Adjustment-State-/
  Revisionsregeln und die #21-Sensorselection-Matrix mit
  `criticalSafetyEventPending` aktiv; kein entfernter Boolwert darf eine
  Mutation wieder freigeben.
- Der generische Fault-Reset-Command ist nicht mehr Bestandteil der
  Application-/UI-Grenze. Der bestehende Watchdog-Reset wird mit frischer
  gueltiger Evidence positiv und mit fehlender/staler/inkompatibler Evidence
  fail-closed geprueft.

### BLOCKER 3 – Confirmation validiert aktuelle Evidence erneut

`FermentationApplicationPreparedRequest` speichert Command-Envelope,
`commandId()`, `commandEnvelope()`, `runId()` und den vorbereiteten Kandidaten
in einem `std::variant` sowie optional `owningPlausibility_`
(`fermentation_ui_commands.hpp:142-187`). `confirm()` (privat, nur
`FermentationApplication` als `friend`) setzt heute ausschliesslich
`envelope.confirmed = true`
(`fermentation_ui_commands.cpp:179ff`) — keine Evidence-Neubewertung.
`FermentationApplication::confirmPrepared()` ist aktuell `static`
(`fermentation_application.hpp:146-147`, `.cpp:534-544`) und hat dadurch
keinen Zugriff auf aktuellen Instanzzustand.

Die Revision legt fest:

- Command-ID, `runId`, Benutzerintent und der vorbereitete Kandidat bleiben
  beim Confirm unveraendert stabil; es wird keine neue Command-ID oder
  `runId` erzeugt.
- `confirmPrepared()` wird zu einer Instanzmethode der
  `FermentationApplication` (materielle, im Rahmen dieses Blockers
  ausdruecklich autorisierte Signaturaenderung), damit sie bei der finalen
  Bestaetigung Zugriff auf die aktuellen Application-eigenen Quellen hat und
  die fuer den gespeicherten Kandidaten relevante Pre-Command-Evidence
  (Blocker 2) sowie die zugehoerige `CrossRolePlausibilityContext` (Blocker 1)
  application-intern erneut aufloest — nicht die bei `prepare*()` vorher
  aufgeloeste Evidence wiederverwendet.
- Ist die erneut aufgeloeste Evidence stale, fehlend oder gegenueber dem
  Vorbereitungszeitpunkt verschlechtert (z. B. `SensorQuality` fuer eine
  beteiligte Rolle von `Valid` auf `Stale`/`Failed` gewechselt, ein zuvor
   erfuellbares `safetyAllowsStart`/`safetyAllowsCooling` nicht mehr
   erfuellbar, oder der betroffene Konfigurations-/
  Persistenzzustand nicht mehr gueltig), wird die Confirmation typisiert
  abgelehnt, ohne den vorbereiteten Kandidaten, die Command-ID oder `runId`
  zu mutieren. Fruehere positive Evidence aus der Vorbereitungsphase ist
  keine Capability und begruendet keine spaetere Freigabe.

Pflichttest: `Prepare gueltig -> Evidence wird zwischen Prepare und Confirm
stale/failed -> Confirm -> typisierte Ablehnung, keine Mutation, Command-ID/
runId unveraendert`.

### BLOCKER 4 – Bestehender Recovery-Evidence-Pfad konsolidiert

`publishOwningRecoveryEvidence(const CrossRolePlausibilityContext&)`
(`fermentation_application.cpp:1029-1031`) setzt bereits
`owningRecoveryEvidence_` (`std::optional<CrossRolePlausibilityContext>`,
Header Z.235); `resumeFallback()` (`.cpp:1034ff`) verbraucht sie single-use
durch Kopie gefolgt von `owningRecoveryEvidence_.reset()`
(Z.1077-1078).

Entscheidung: Dieser Pfad ist bereits `CrossRolePlausibilityContext`-typisiert
— exakt derselbe Typ, der in Blocker 1 als einzige Application-seitige
Runtime-Evidence-Wahrheit festgelegt wird. Der neue Runtime-Handoff **ersetzt
diesen Pfad nicht**, sondern beide lesen denselben Typ aus derselben
kanonischen Quelle (Rollen-`SensorQualitySnapshot`s). `publishOwningRecoveryEvidence`/
`owningRecoveryEvidence_`/`resumeFallback()` bleiben als eigener, klar
begrenzter Recovery-Einspeisepfad bestehen; es entsteht keine zweite
Plausibilitaetswahrheit, weil beide denselben `CrossRolePlausibilityContext`-
Vertrag verwenden. Die bestehende Semantik bleibt unveraendert erhalten:
frische Evidence pro Verbrauch, kein Rendererinput, kein Wiederverwenden nach
einem Mutationsversuch (Reset bei Verbrauch, Z.1077-1078, sowie bestehende
weitere `.reset()`-Stellen Z.761,1213 fuer Invalidierung ohne Verbrauch), keine
parallele Plausibilitaetswahrheit gegenueber Blocker 1.

Tests: fehlende (`owningRecoveryEvidence_` leer), gueltige, bereits
verbrauchte (zweiter `resumeFallback()`-Aufruf nach Reset) und inzwischen
veraltete (Rolle zwischenzeitlich `Stale`/`Failed`) Recovery-Evidence.

### Snapshot und Commands

Die Application stellt eine ownerseitige Snapshot-Methode
`FermentationApplication::uiSnapshot()` (oder nur dann einen gleichwertigen
bereits vorhandenen Namen, wenn die Bestandspruefung dies zwingend nahelegt)
bereit. Sie erzeugt keinen zweiten Snapshotvertrag, sondern fuellt den
bestehenden `FermentationUiProjectionInput` und ruft den vorhandenen reinen
`FermentationUiProjector` auf.

Die bestehenden `prepare*`-Pfade werden so angepasst, dass externe Aufrufer
nicht mehr `FermentationApplicationOwningEvidence` injizieren koennen. Sie
uebergeben ausschliesslich den typisierten UI-/Command-Intent, benoetigte
fachliche Werte, Bestaetigungen und erwartete Revisionen. Die Application
loest daraus die aktuelle Pre-Command-Evidence (Blocker 2) intern auf und
delegiert die Mutation an die bestehenden Run-/Persistenz-Owner.
`confirmPrepared()` loest sie bei der finalen Bestaetigung erneut auf
(Blocker 3). Command-Identitaet, erwartete Revisionen, Exactly-once-/
Persistenzsemantik und bestehende Recovery-Ergebnisse bleiben unveraendert.

Damit kann ein Renderer oder Web-Adapter keine Safety-, Sensor-, Planner-
oder Recovery-Evidence mehr als Eingabe der Application erzeugen. Er liefert
nur Absicht, Nutzdaten, Bestaetigung und Revisionserwartung.

### Zukuenftiger #30-Handoff

Wenn #30 spaeter reale DS18B20-Werte liefert, werden seine bereits
kanonisierten `TemperatureReading`-/Qualitaetsdaten ueber die bestehende
`device_platform::SensorQualityPipeline` in denselben Application-owned
Runtime-Handoff (`CrossRolePlausibilityContext`, Blocker 1) eingespeist. #30
wird dadurch nicht zum UI- oder Safety-Owner und erzeugt keinen zweiten
Sensorstatus. In diesem Scope wird nur der softwareseitige Vertrag fuer
diesen Handoff und eine native Testquelle festgelegt; DS18B20-Adapter, GPIO,
ROM, CRC, Bus- und Hot-Plug-Arbeit bleiben ausgeschlossen.

## Aktive Dokumentationsgrenzen

Die folgenden aktiven kanonischen Dokumente wurden repository-first gegen
den bestehenden Vertrag geprueft. Die spaetere Implementation synchronisiert
nur tatsaechlich widerspruechliche Aussagen; historische versionierte
Task-Plaene werden nicht rueckwirkend umgeschrieben:

- `docs/RUN_COMMANDS.md` ist direkt betroffen: der aktive Command-Vertrag
  muss `safetyAllowsChange` und den generischen
  `FaultResetEvaluation`-/`FaultResetRequest`-Pfad entfernen und den schmalen
  `FermentationUiResetFaultIntent`-/Watchdog-Ownerpfad ohne Service-PIN
  beschreiben.
- `docs/SAFETY_AND_FAULTS.md` enthaelt bereits im vorrangigen R1-Abschnitt
  den stateless `ActuationInterlock`, `ActuatorRequestWatchdog` als einzigen
  expliziten #23-Reset mit frischer Evidence sowie keine Service-PIN-Pflicht.
  Die nachfolgenden C2-/Legacy-/Future-Abschnitte sind ausdruecklich nicht
  #24-R1 und bleiben unveraendert.
- `docs/STATE_MACHINE.md` beschreibt die Service-PIN bereits als spaeteres
  Service-/Hardware-Gate ausserhalb des #24-R1-Interlocks; nur ein direkter
  Widerspruch zur neuen Reset-Bridge waere zu korrigieren.
- `docs/REQUIREMENTS.md` fuehrt Service-PIN-/Hardware-Servicefunktionen als
  spaetere Gates und fuehrt keinen generischen R1-Fault-Clearer; kein
  Rueckwirkungsdelta ist derzeit begruendet.
- `docs/ACCEPTANCE_TESTS.md` grenzt Service-PIN-/Vollreset-Tests bereits von
  der #24-R1-Abnahme ab und nennt den R1-Watchdog-/Ack-Vertrag; nur direkt
  betroffene Command-/Resettestverweise werden synchronisiert.

Damit ist der aktive Dokumentationsumfang konkret festgelegt, ohne
historische Aussagen in einen falschen aktuellen R1-Vertrag umzudeuten.

## Erlaubter Implementierungsumfang

Nur die kleinste direkt betroffene Application-/Composition-/Projection-
Aenderung ist zulaessig:

1. Application-interne Runtime-Evidence-Zusammenfuehrung in
   `lib/fermentation_app`, vorzugsweise in den bestehenden
   `fermentation_application.{hpp,cpp}`-Dateien, basierend auf
   `CrossRolePlausibilityContext` (Blocker 1).
2. Ownerseitige `uiSnapshot()`-Projektion gegen den bestehenden
   `FermentationUiProjector`; keine neue UI-State-Machine und kein paralleles
   View-Modell.
3. Anpassung der bestehenden Application-Command-Grenze, damit externe
   Aufrufer keine `FermentationApplicationOwningEvidence` mehr liefern, sowie
   Umwandlung von `confirmPrepared()` in eine Instanzmethode mit erneuter
   Evidence-Aufloesung (Blocker 3).
4. Konsolidierung des bestehenden Recovery-Evidence-Pfads
   (`publishOwningRecoveryEvidence`/`owningRecoveryEvidence_`/
   `resumeFallback()`) auf denselben `CrossRolePlausibilityContext`-Vertrag
   (Blocker 4), ohne dessen bestehende Semantik zu aendern.
5. Schmaler `FermentationUiResetFaultIntent`-/Application-Bridgepfad, der
   ohne bereits kanonisch gebundenen Planner-Owner typisiert `Unavailable`
   liefert und keine Composition erzeugt. Die spaetere Ownerbindung an
   `ActuationInterlock::resetRequestWatchdog(...)` bleibt an #106/#35
   downstream gebunden, mit bestehender Confirmation-/Revisionspruefung und
   ohne Run-Persistence-Mutation; kein generischer Fault-Dispatcher.
6. Synchronisierung der in "Aktive Dokumentationsgrenzen" genannten aktiven
   Aussagen, nur soweit der direkte Produktions-/Command-Diff sie
   widerspricht.
7. Nur direkt betroffene native Tests fuer Snapshot, Evidence-Aufloesung,
   Pre-Command-/post-commit-Trennung, Confirmation-Revalidierung,
   Recovery-Konsolidierung, Fail-closed-Verhalten, Revisionen, den
   Watchdog-Reset-Bridgepfad und Command-Aufrufe.

Ein neues kleines application-internes Struct ist nur zulaessig, wenn die
bestehenden Typen die Komposition nicht ausdruecken koennen. Es darf keine
allgemeine Runtime-Provider-, Eventbus-, Service-Locator- oder zweite
State-Machine-Abstraktion entstehen. Aenderungen an `device_platform` sind
nur zulaessig, wenn ein bereits bestehender abstrakter Temperatur-/Qualitaets-
Vertrag konkret in den Application-Handoff eingebunden werden muss; neue
fermentation-spezifische Ports gehoeren nicht dorthin.

Die konkrete Berechnung von `safetyAllowsStart` und
`safetyAllowsCooling` folgt verbindlich dem Readiness-Vertrag dieses Plans;
`safetyAllowsChange` und der generische `FaultResetEvaluation`-Vertrag werden
entfernt. Die Application-interne Evidence-Aufloesung — einschliesslich der
bereits entschiedenen Sensorvaliditaet
(`airSensorValid`/`coolingSensorValid`/`productSensorValid` aus
`CrossRolePlausibilityContext`) — ist damit vollstaendig spezifiziert. Die
post-commit-Aktorfreigabe bleibt trotzdem ausserhalb dieses Scopes.

## Nicht im Scope

- #27 Web/API/Auth, HTTP, Sessions, Auth, Webassets oder KDF;
- #30 DS18B20-Hardware, Adapter, GPIO, ROM-/CRC-/Hot-Plug- und reale
  Sensorpruefungen;
- #31 Display, Touch, Renderer, Bibliothek und Kalibrierung;
- #28 Diagnose, Service, Exporte, historische Laufdaten oder Diagramme;
- #35 PI-, Luft-, Aktor- und Safety-Parameter sowie Commissioning-Werte;
- #106 produktive Per-Run-Bindung oder Aktoraktivierung;
- neue Safety-Logik, neue Sensorqualitaets-/Rollenklassifikation oder neue
  Recovery-Policy;
- zweite Persistenz-, Connectivity-, Auth-, Sensor- oder UI-Wahrheit;
- neue generische Provider-/Event-/Service-Locator-Infrastruktur;
- ESP-IDF-, HTTP-, Hardware-, Aktor-, Renderer- oder Bibliotheksarbeit;
- eine post-commit `ActuationInterlock`-Aktorfreigabe als Ersatz fuer
  Pre-Command-Evidence oder umgekehrt (Blocker 2 bleibt eine strikte
  Trennung, keine Vereinheitlichung);
- erfundene Produktionswerte, Default-Temperaturen oder simulierte
  Hardware-/Client-Evidence.

`ACTUATOR_RELEASE=NO` bleibt waehrend des gesamten Scopes unveraendert.

## Gezielte native Regressionen

Die Tests muessen die bestehende Semantik pruefen, nicht einen neuen
Parallelvertrag einfuehren:

1. Fehlende Producerquelle bleibt im Snapshot als der bestehende
   Default-/Fail-safe-`SensorQualitySnapshot` (`identity=nullopt`,
   `quality=Stale`) sichtbar; es gibt keinen impliziten Nullwert, keine
   Persistenzmutation und keine Sensor-Safety-Freigabe.
2. Eine native kanonische Testquelle kann einen
   `SensorQualitySnapshot`-Handoff je Rolle speisen; Snapshot, Rollen- und
   `CrossRolePlausibilityContext` verwenden danach exakt diese Quelle
   (Blocker 1).
3. Valid, stale und failed Sensorqualitaet (`SensorQuality::{Valid,Stale,
   Failed}`) werden entsprechend den bestehenden #20/#21-Vertraegen
   behandelt; fehlende oder widerspruechliche Evidence bleibt fail-closed.
4. Ein fachlich zulaessiger Fresh Start wird vorbereitet/entschieden, waehrend
   die post-commit `ActuationInterlock`-Aktorfreigabe noch `Unresolved` ist;
   reale Freigabe entsteht nachweislich erst nach Persistenz/Apply/Interlock
   (Blocker 2). `airSensorValid`/`coolingSensorValid`/`productSensorValid`
   werden je Rolle positiv und einzeln negativ geprueft (`air`/`cooling`
   Pflicht, `product` nur fuer `resolveProgramStartSensorMode()`). Ein
   sicherer Stop/Completion ohne Cooling konsultiert `safetyAllowsCooling`
   nicht und wird dadurch nicht blockiert. Positive und negative
   Application-Readiness fuer Start und Cooling werden getrennt geprueft;
   `AbortAndTurnOff` und Completion ohne Cooling bleiben bei fehlender
   Cooling-Readiness zulaessig.
   Jede Readiness-Bedingung wird einzeln negativ getestet: Lifecycle nicht
   `Ready`; Runtime-Lease busy/unavailable oder falsche `StorageEpoch`; jeder
   abgelehnte `RunPersistenceLoadStatus`; `RunLoadDisposition::SafeBoot`; jeder
   abgelehnte `RunPersistenceCoordinatorState`; sowie
   `criticalSafetyEventPending=true`. Ein post-commit Watchdog-/Interlock-
   Fault wird separat nachgewiesen und darf keine neue Pre-Command-
   Parallelregel erzeugen.
5. `Prepare gueltig -> Evidence stale/failed vor Confirm -> Confirm ->
   typisierte Ablehnung ohne Mutation, Command-ID/`runId` unveraendert`
   (Blocker 3).
6. Recovery-Evidence: fehlend, gueltig, bereits verbraucht und inzwischen
   veraltet, konsolidiert auf denselben `CrossRolePlausibilityContext`-Vertrag
   (Blocker 4).
7. Start-, Cooling-, Aenderungs-, Completion-, Stop- und Recovery-Kommandos
   loesen die aktuelle Pre-Command-Evidence in der Application auf. Der
   `FermentationUiResetFaultIntent` liefert im aktuellen #168-
   Compositionpfad typisiert `Unavailable`, solange kein kanonischer
   Planner-Owner gebunden ist; er erzeugt keine Mutation. Die spaetere
   `true`-Ownermutation bleibt an #106/#35 gebunden und erfolgt ohne
   Run-Persistence-Mutation. Kein oeffentlicher Command-/UI-Aufruf kann
   `FermentationApplicationOwningEvidence`, `FaultResetEvaluation` oder
   `FaultResetRequest` einsetzen.
8. Bestehende erwartete Revisionen, Run-Identity, Persistenz- und
   Exactly-once-Semantik bleiben erhalten; stale Commands werden wie bisher
   abgelehnt.
9. Snapshot-Projektion enthaelt aktuelle Application-/Service-/Network-
   Quellen und Refresh-/Revisionswerte ohne doppelte Projection-Logik.
10. Ein nativer #30-Handoff-Test prueft nur die kuenftige abstrakte
    Einspeisegrenze ueber `SensorQualityPipeline`; kein DS18B20-Code und
    keine Hardwareannahme.
11. Direkt betroffene native Tests sowie
    `scripts/check_architecture_boundaries.py` und `git diff --check` bleiben
    PASS. ESP-IDF-, Hardware- und Web-CI-Laeufe gehoeren nicht in die
    Planphase.

## Umsetzung und Nachweis

Die spaetere Umsetzung kann in kleinen Builder-Commits erfolgen:

1. Application-owned Evidence-Komposition (`CrossRolePlausibilityContext`,
   Blocker 1) und `uiSnapshot()` gegen die bestehenden Projektionstypen;
2. Pre-Command-/post-commit-Trennung (Blocker 2), Anpassung der
   Command-Grenze, Confirmation-Revalidierung (Blocker 3) und direkte native
   Regressionen;
3. Konsolidierung des Recovery-Evidence-Pfads (Blocker 4), gezielte
   Konsistenz-/Architekturpruefungen und Handover-Evidence.

Jeder Commit bleibt innerhalb dieses Plans. Bei einer notwendigen materiellen
Abweichung an Architektur, Persistenz, Wireformat, Recovery, Safety,
Abhaengigkeiten oder Teststrategie ist vor weiterer Umsetzung eine neue
Planrevision erforderlich.

Akzeptiert ist der Scope erst, wenn die Application alleinige Quelle der
aktuellen Runtime-Evidence ist, Renderer/Touch/Web keine solche Evidence mehr
einspeisen koennen, fehlende Producer sichtbar unavailable bleiben, die
bestehenden UI-/Command-/Safety-Vertraege nicht dupliziert werden, der
Recovery-Evidence-Pfad konsolidiert statt verdoppelt ist und #30 spaeter
denselben Handoff verwenden kann.

## Roadmap-Synchronisation (`docs/ROADMAP.md`)

`docs/ROADMAP.md` wird mit genau diesem Plan-Commit synchronisiert (Zeilen
#164/PR #165, #89 und #31 in "Aktuelle Arbeit"), nicht erst bei der spaeteren
#168-Umsetzung — Blocker 5 verlangt die aktuelle SSOT, nicht eine
Absichtserklaerung. PR #165 ist gemergt
(`mergedAt=2026-09-18T07:31:12Z`, `mergeCommit=1f1755e5e706fb668472920545b5302fcef1df16`
== aktueller `main`); Independent Review, Pre-Ready und GitHub-CI sind fuer
PR #165 abgeschlossen, offen bleibt reale Hardware-/Client-Evidence. #89 ist
`EVALUATION_COMPLETE`; die produktive Integration liegt bereits gemergt in
#164/PR #165, nicht mehr `IN_PROGRESS`. #31 ist nicht mehr
`BLOCKED_HARDWARE_NOT_CONNECTED`: Hardware ist verbunden, Stage 0/1 PASS,
Stage 2 `FAILED` (`COLDSTART_ROOT_CAUSE=UNDETERMINED`, Touch-Funktion PASS),
Stage 3/4 `NOT_RUN`. Die konkreten Zeilenaenderungen sind Teil dieses
Plan-Commits (siehe Diff auf `docs/ROADMAP.md`), keine Issue-Inhalte werden
kopiert.

## Metadatenbereinigung

Issue #168 und PR #169 verwenden `[E4.4]`, obwohl Issue #28 bereits `[E4.4]`
traegt. `[E4.1]` (#25), `[E4.2]` (#144, #152, #26) und `[E4.3]` (#27) sind
ebenfalls bereits vergeben; `[E4.5]` ist frei (Bestandspruefung:
`gh issue list --search "E4.5" --state all` liefert keinen Treffer). Issue
#168 wird auf `[E4.5]` umbenannt (Titel und PR-Titel) — eine tatsaechlich
eindeutige, bisher unbenutzte Kennung. Technischer Scope bleibt unveraendert.

## Reihenfolge und Owner-Gates

1. Issue #168 wird auf aktuellem `main` gefuehrt; dieser Draft-PR enthaelt
   nur Plan-/Roadmap-Artefakte.
2. Der Owner muss die exakte Plan-Commit-SHA dieser Revision freigeben.
3. Erst danach darf Issue #168 implementiert werden; bis dahin gilt
   `IMPLEMENTATION_AUTHORIZATION=NO`.
4. Nach Merge von #168 wird PR #167 auf den neuen kanonischen `main` gebracht.
   Der #27-Plan wird dann als Verbraucher dieses Application-Scopes revalidiert
   und bei Bedarf als neue Planrevision freigegeben. Die #27-Implementierung
   wird bis dahin nicht fortgesetzt.
5. Danach gelten fuer #27 wieder dessen eigene unabhängige Review-,
   Pre-Ready-, CI- und Merge-Gates.

## Bedingungen vor Umsetzung

- Die vier Ownerentscheidungen sind durch
  `OWNER_SAFETY_SEMANTICS_DECISION=PASS`,
  `OWNER_DECISIONS_PENDING=0` und Provenienzkommentar `5772310484` in dieser
  Planrevision geschlossen. Die exakte neue Plan-SHA benoetigt weiterhin die
  separate Owner-Planfreigabe.
- Die Implementierung muss zuerst bestaetigen, dass
  `FermentationApplication::uiSnapshot()` und die bestehende interne
  Application-Komposition ohne neue allgemeine Provider-/Event-Infrastruktur
  ausreichen.
- Falls der aktuelle Quellstand fuer eine sichere Application-owned
  Sensor-/Safety-Komposition einen materiell neuen Producer, eine neue
  Persistenzwahrheit oder eine andere Architekturgrenze benoetigt, ist hier
  anzuhalten und eine Planrevision vorzulegen. Es darf kein Fallback-Owner,
  kein Web-/Renderer-Owner und keine erfundene Produktionsquelle eingefuehrt
  werden.

## Abschluss dieser Planrevision

Nach dieser Planrevision:

- nur Plan-/Roadmap-/Issue-/PR-Metadaten geaendert;
- keine Produktimplementation;
- neue exakte Plan-SHA in Issue, PR und bestehendem SESSION HANDOVER
  konsistent ausweisen;
- fuer Independent Plan Fix Verification anhalten.

```text
PLAN_STATUS=DRAFT_OWNER_APPROVAL_REQUIRED
IMPLEMENTATION_AUTHORIZATION=NO
OWNER_SAFETY_SEMANTICS_DECISION=PASS
OWNER_DECISIONS_PENDING=0
OWNER_PLAN_APPROVAL=NO
ACTUATOR_RELEASE=NO
OWNER_PLAN_APPROVAL_REQUIRED=YES
```
