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
OWNER_SAFETY_SEMANTICS_DECISION_REQUIRED=YES
CONSUMER=ISSUE_27_PR167_AFTER_ISSUE168_MERGE
ACTUATOR_RELEASE=NO
```

Dieser Plan ist der vorgelagerte, kleine Application-/Composition-Scope fuer
Issue #27. Er wird auf dem aktuellen kanonischen `main` erstellt
(`BASE_SHA` == aktueller `origin/main`). Die Umsetzung endet nach dem
Plan-Commit bis zur ausdruecklichen Freigabe genau dieses Plan-Commits.

Diese Revision behebt vier der fuenf Blocker aus dem Independent Plan Review
von `PREVIOUS_REVIEWED_PLAN_SHA=2aca6ce5ce8eb1c7ba6e37cf6a1ebd27a4e4cad1` und
praezisiert den verbleibenden Rest-Blocker aus der gezielten Fix
Verification auf `PRIOR_FIX_VERIFICATION_PLAN_SHA=
ef875d8e22a72135fed25a8629949cfdfc762ace` (konkrete Pre-Command-Safety-
Semantik). Der Scope selbst (Application als alleiniger Runtime-Evidence-
Owner vor #27) ist unveraendert richtig und bleibt bestehen.
`OWNER_SAFETY_SEMANTICS_DECISION_REQUIRED=YES` markiert, dass vier konkrete
Safety-Formeln (Blocker 2) noch eine Ownerentscheidung benoetigen, bevor
`IMPLEMENTATION_AUTHORIZATION=YES` fuer die betroffenen Commands moeglich
ist (siehe Blocker 2, Abschnitt "Offene Owner-Entscheidungen").

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
| Pre-Command-Safety-Evidence | Bereits vorhandene, von `ActuationEvidence` unabhaengig benoetigte Felder `safetyAllowsStart`/`safetyAllowsCooling`/`safetyAllowsChange`/Sensorvaliditaet in `ProgramStartRequest`, `ManualStartRequest`, `StopRequest`, `CompletionRequest`, `RunAdjustmentCommandRequest`, `SensorSelectionCommandRequest` (`run_commands.hpp:156-235`) | Werden aktuell extern injiziert; werden in diesem Scope stattdessen application-intern aus denselben kanonischen Quellen aufgeloest, die die Application ohnehin schon besitzt (siehe Blocker 2). |
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
dauerhaft gespeicherten positiven `safetyAllowsStart`, `safetyAllowsCooling`,
`safetyAllowsChange` oder Sensorvaliditaets-Flag; diese Felder werden bei
jeder Aufloesung neu berechnet, nie zwischengespeichert und wiederverwendet.

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
   `safetyAllowsCooling`, `safetyAllowsChange`, Fault-Reset und
   Sensorselection. Sie entsteht ausschliesslich application-intern; sie darf
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

#### Offene Owner-Entscheidungen: `safetyAllowsStart`/`Cooling`/`Change` und Fault-Reset

Fuer die verbleibenden vier Evidence-Arten existiert **keine** bereits
beschlossene kanonische Berechnungsformel im Repository oder in den
#20-/#21-/#24-Planvertraegen. Bestandspruefung (repository-first, vor
Planfreigabe durchgefuehrt, nicht in die Implementierung verschoben):

- Alle vier Felder werden aktuell ausschliesslich als reine externe
  Passthrough-Parameter gefuehrt: `fermentation_application.cpp:281,310,347,
  375,411,459,495` kopiert `request.safetyAllowsX = evidence.safetyAllowsX`
  unveraendert aus dem heute extern injizierten
  `FermentationApplicationOwningEvidence`. Es gibt keinen weiteren
  Produktionsaufrufer, der diese Felder aus einer Formel berechnet;
  ausserhalb davon setzen nur Diagnose-/Testharnesse
  (`main/issue_90_slice7_harness.cpp:542,582`,
  `main/issue_29_bringup_probe.cpp:206`) hartcodierte Werte.
- `ActuationEvidence`s eigene Vorbedingungsfelder
  `bootValidationComplete`, `persistenceValidated`, `plannerEvidenceValidated`
  (`actuation_interlock.hpp:41-46`) haben **keinen einzigen** Produktions-Setter
  im gesamten Repository; sie koennen daher aktuell keine Formel fuer
  Boot-/Persistenz-Bereitschaft liefern, ohne eine neue Regel zu erfinden.
- Zwei verwandte, bereits bestehende reine Praedikate existieren, sind aber
  **nicht** unveraendert fuer den Pre-Command-Zweck ratifiziert:
  `hasFreshConfigurationEvidence(evidence)` (`actuation_interlock.cpp:36-58`)
  und `hasFreshSensorEvidence(evidence)` (`actuation_interlock.cpp:60-69`),
  beide anonymous namespace. Beide sind aktuell ausschliesslich
  Teilausdruecke der post-commit `ActuationInterlock::evaluate()`-Berechnung
  (`activationEvidenceComplete()`, Z.412-429). Sie unveraendert auf einem
  Pre-Command-Pfad erneut aufzurufen, wuerde demselben Praedikat zwei
  Aufrufer mit zwei unterschiedlichen Bedeutungen geben — exakt das
  Parallelwahrheit-Muster, das Blocker 1 bereits ausschliesst. Diese
  Praedikate sind daher nur **Kandidaten**, keine bereits entschiedene
  Wiederverwendung.
- `hasFreshSensorEvidence` ist zudem `peltierSensor`-/`SensorPeltierPermission`-
  skaliert (ein einzelner Aktorsensor), nicht rollenbezogen im Sinne von
  `air`/`product`/`cooling` — sie kann eine cooling-spezifische Pruefung fuer
  `safetyAllowsCooling` nicht ohne Weiteres begruenden.
- `SensorSelectionCommandRequest.safetyAllowsChange`
  (`run_commands.hpp:229-235`) hat einen woertlichen Kommentar, der das Feld
  als reines additives externes Signal festlegt: *"`safetyAllowsChange` ist
  wie `ProgramStartRequest::safetyAllowsStart` ein zusaetzliches externes
  Pruefsignal - es ersetzt weder die interne
  `criticalSafetyEventPending`-Invariante noch wird es von ihr ersetzt."*
  Das legt die Rolle des Felds fest, nicht seine Berechnung.
- `FaultResetEvaluation` (`run_commands.hpp:304-312`, Felder `allowed`,
  `causeStillActive`, `safetyChecksPassed`, `authorizationSatisfied`,
  `otherBlockingFaultActive`, `faultRevision`, `rejection`, konsumiert in
  `decideFaultReset()` Z.1373-1378) wird im gesamten Repository **nirgends**
  berechnet — nur in `fermentation_application.cpp:433,488` unveraendert
  durchgereicht. Vollstaendige Luecke, keine Kandidaten vorhanden.

Gemaess Auftragsvorgabe wird hier **nicht geraten**. Diese vier Punkte sind
explizite, vor Ownerfreigabe zu klaerende Planentscheidungen und blockieren
`IMPLEMENTATION_AUTHORIZATION=YES` fuer die betroffenen Commands, bis der
Owner entscheidet:

1. **`safetyAllowsStart`**: Ist `hasFreshConfigurationEvidence(evidence)`
   (`actuation_interlock.cpp:36-58`) als Pre-Command-Formel ratifiziert? Die
   Funktion prueft exakt: `configurationValidated == true` ist zwingend
   erforderlich, UND zusaetzlich mindestens eine der drei Bedingungen
   `configurationRecoveryStatus ∈ {RuntimeReady,
   FactoryInitializationCompleted, FactoryResetCompleted}` ODER
   `configurationServiceMode == Operational` ODER `configurationCommitStatus
   ∈ {Activated, NoChange}`. Falls ratifiziert: reicht diese Formel allein
   fuer `safetyAllowsStart`, oder ist zusaetzlich eine Persistenz-/
   Boot-Klassifikationsbedingung vorzugeben (fuer die aktuell — siehe oben —
   keine `ActuationEvidence`-Vorbedingung produktiv befuellt wird)?
2. **`safetyAllowsCooling`**: Welche kanonische Formel gilt, wenn das Feld
   tatsaechlich konsultiert wird (nur bei `StopOption::AbortAndCool`
   bzw. `CompletionRequest`-Cooling-Pfad, siehe Testvertrag unten)?
3. **`safetyAllowsChange`**: Welche kanonische Formel gilt fuer
   `RunAdjustmentCommandRequest`/`SensorSelectionCommandRequest`, zusaetzlich
   zur bereits bestehenden internen `criticalSafetyEventPending`-Invariante?
4. **`FaultResetEvaluation`**: Aus welchen bestehenden kanonischen Zustaenden
   werden `allowed`, `causeStillActive`, `safetyChecksPassed`,
   `authorizationSatisfied`, `otherBlockingFaultActive` und `rejection`
   application-intern bestimmt?

Bis diese vier Punkte geklaert sind, bleibt die Umsetzung der betroffenen
`prepare*`-Pfade (Start, Cooling-Aktivierung, Change, Fault-Reset,
Sensorselection) angehalten; die uebrigen Teile dieses Scopes (Blocker 1,
3, 4, Sensorvaliditaet oben) sind davon unabhaengig umsetzbar.

Direkter Testvertrag (soweit bereits durch bestehende Semantik entschieden):

- Ein fachlich zulaessiger Fresh Start darf vorbereitet und entschieden
  werden (`decideProgramStart()` liefert eine positive Entscheidung),
  waehrend die post-commit `ActuationInterlock`-Aktorfreigabe noch
  `Unresolved` ist. Reale Freigabe entsteht erst nach Persistenz/Apply/
  Interlock. `ACTUATOR_RELEASE=NO` bleibt davon unberuehrt, da dieser Scope
  keine Aktorfreigabe einfuehrt.
- Ein sicherer Stop/Completion ohne Cooling (`StopOption` ungleich
  `AbortAndCool` bzw. ohne `startCooling`) konsultiert `safetyAllowsCooling`
  nicht und wird dadurch nicht blockiert (`decideStop()`,
  `run_commands.cpp:970-973`: `safetyAllowsCooling` wird ausschliesslich
  innerhalb `if (request.option == StopOption::AbortAndCool)` geprueft;
  analoger Completion-Pfad `run_commands.cpp:1061`) — dieser Testfall ist
  unabhaengig von den vier offenen Owner-Entscheidungen bereits heute
  pruefbar.
- `airSensorValid`/`coolingSensorValid`/`productSensorValid` je Rolle positiv
  und einzeln negativ (`SensorQuality::Valid` vs. `Stale`/`Failed`) fuer
  Start; `air`/`cooling` sind Pflicht, `product` beeinflusst nur
  `resolveProgramStartSensorMode()`.
- Zeilen fuer `safetyAllowsStart`/`Cooling`/`Change`/`FaultResetEvaluation`
  selbst werden erst nach der jeweiligen Owner-Entscheidung ergaenzt, nicht
  als Platzhalter vorab spezifiziert.

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
  erfuellbares `safetyAllowsStart`/`safetyAllowsCooling`/`safetyAllowsChange`
  nicht mehr erfuellbar, oder der betroffene Konfigurations-/
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
5. Nur direkt betroffene native Tests fuer Snapshot, Evidence-Aufloesung,
   Pre-Command-/post-commit-Trennung, Confirmation-Revalidierung,
   Recovery-Konsolidierung, Fail-closed-Verhalten, Revisionen und
   Command-Aufrufe.

Ein neues kleines application-internes Struct ist nur zulaessig, wenn die
bestehenden Typen die Komposition nicht ausdruecken koennen. Es darf keine
allgemeine Runtime-Provider-, Eventbus-, Service-Locator- oder zweite
State-Machine-Abstraktion entstehen. Aenderungen an `device_platform` sind
nur zulaessig, wenn ein bereits bestehender abstrakter Temperatur-/Qualitaets-
Vertrag konkret in den Application-Handoff eingebunden werden muss; neue
fermentation-spezifische Ports gehoeren nicht dorthin.

Die konkrete Berechnung von `safetyAllowsStart`, `safetyAllowsCooling`,
`safetyAllowsChange` und `FaultResetEvaluation` (Blocker 2, "Offene
Owner-Entscheidungen") ist **nicht** Teil des mit dieser Plan-SHA
freigebbaren Implementierungsumfangs, solange die zugehoerige
Ownerentscheidung fehlt. Die uebrige Application-interne
Evidence-Aufloesung — einschliesslich der bereits entschiedenen
Sensorvaliditaet (`airSensorValid`/`coolingSensorValid`/`productSensorValid`
aus `CrossRolePlausibilityContext`) — ist davon unabhaengig umsetzbar.

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
   nicht und wird dadurch nicht blockiert. Testzeilen fuer die konkrete
   Formel von `safetyAllowsStart`/`Cooling`/`Change`/`FaultResetEvaluation`
   folgen erst nach den vier Owner-Entscheidungen aus Blocker 2 und sind
   hier bewusst nicht vorab spezifiziert.
5. `Prepare gueltig -> Evidence stale/failed vor Confirm -> Confirm ->
   typisierte Ablehnung ohne Mutation, Command-ID/`runId` unveraendert`
   (Blocker 3).
6. Recovery-Evidence: fehlend, gueltig, bereits verbraucht und inzwischen
   veraltet, konsolidiert auf denselben `CrossRolePlausibilityContext`-Vertrag
   (Blocker 4).
7. Start-, Cooling-, Aenderungs-, Completion-, Stop- und Recovery-Kommandos
   loesen die aktuelle Pre-Command-Evidence in der Application auf. Kein
   oeffentlicher Command-/UI-Aufruf kann `FermentationApplicationOwningEvidence`
   einsetzen.
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

## Offene Entscheidungen vor Umsetzung

- Die vier in Blocker 2 unter "Offene Owner-Entscheidungen" benannten Punkte
  (`safetyAllowsStart`, `safetyAllowsCooling`, `safetyAllowsChange`,
  `FaultResetEvaluation`) sind vor Ownerfreigabe zu klaeren; sie sind nicht
  Implementierungsentscheidungen und werden nicht in die Umsetzung
  verschoben (siehe `OWNER_SAFETY_SEMANTICS_DECISION_REQUIRED` unten).
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
OWNER_SAFETY_SEMANTICS_DECISION_REQUIRED=YES
ACTUATOR_RELEASE=NO
OWNER_PLAN_APPROVAL_REQUIRED=YES
```
