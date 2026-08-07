# Plan: Issue #18 – Wiederanlauf und temperaturgewichteter Fortschritt

## 1. Metadaten und Status

| Feld | Wert |
|---|---|
| Issue | [#18](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/18) – `[E2.3] Wiederanlauf und temperaturgewichteten Fortschritt implementieren` |
| Epic | [#4](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/4) – Konfiguration, Persistenz und Wiederanlauf |
| PR | [#102](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/102) (Draft) |
| Branch | `plan/issue-18-restart-weighted-progress` |
| Basis | `main` @ `082fb3f` (Merge PR #99 / Issue #21) |
| Status dieses Dokuments | **Revision 2** – vollstaendig ueberarbeitet nach Ownerreview von Revision 1 (Plan-Commit `67804c2a97c752a0b4429d04b56f22c8b97bedc3`) |
| Vorherige Revision | Revision 1 wurde vom Owner mit elf materiellen Befunden zurueckgewiesen (siehe Auftrag "PR #102 Planrevision nach vollstaendigem Review"). Jeder Befund ist unten mit Fundstelle referenziert und durch eine konkrete Vertragsaenderung beantwortet, nicht durch eine Ownerentscheidung vertagt. |

## 2. Live-Issue- und Abhaengigkeitsabgleich

Unveraendert seit Revision 1: PR #99/Issue #21 gemergt (`082fb3f`), Issue #21
`CLOSED`, alle Abhaengigkeiten von #18 (#10, #14, #17, #20, #21) `CLOSED`.
`docs/ROADMAP.md` und Issue #18 wurden bereits vor Revision 1 minimal-
konsistent synchronisiert (Commit `5bbe7b6`, Issue-#18-Body-Edit) und
benoetigen fuer Revision 2 keine weitere Aenderung.

## 3. Ownerentscheidungen (nicht erneut offen)

Der Owner hat alle drei Gates aus Revision 1 entschieden. Diese Revision
stellt sie nicht erneut zur Auswahl:

- **Gate A:** Der #21-Handover-Vertrag bleibt gueltig. `docs/RUN_PERSISTENCE.md`
  wird **nicht** auf den heutigen Stub zurueckgestuft. #18 vervollstaendigt
  die echte Restart-Sensorentscheidung (Abschnitt 6.3).
- **Gate B:** #18 besitzt die hardwareunabhaengige Recovery-Orchestrierung
  vollstaendig nativ. Produktive ESP-NVS-/Hardware-Composition bleibt
  ausdruecklich bei #29/#90 (Abschnitt 6.9, Nicht-Ziel in Abschnitt 4).
- **Gate C:** Kein unkalibriertes Aktivitaetskennfeld. Unsichere Ausfallzeit
  wird ohne validiertes Modell nicht automatisch gutgeschrieben; normaler
  Zeitablauf wird nicht als "temperaturgewichtet" umetikettiert (Abschnitt 6.7).

Es verbleiben **keine offenen Gates** aus dieser Revision. Verbleibende
Unsicherheiten sind Implementierungsdetails, keine Owner-Entscheidungen, und
sind als solche in Abschnitt 13 benannt.

## 4. Ziel und Nicht-Ziele

### Ziel

Unveraendert in der Substanz gegenueber Revision 1 (Abschnitt 4), aber mit
praezisierten Mechanismen:

1. Ein nach Spannungsverlust gestarteter, ueber `RunPersistenceCoordinator::
   loadAndInitialize()` geladener aktiver Lauf durchlaeuft `RECOVERY_EVALUATION`
   und wird ueber eine **einzige atomare** Recoveryaktivierung
   (`RunPersistenceCoordinator::activateLoadedRun`, Abschnitt 6.4) nach
   `Ready` ueberfuehrt, bevor irgendeine Reglerfreigabe fuer diesen Lauf
   moeglich ist.
2. Die Sensorselektions-Reaktivierung aus #21 wird ueber eine **einzige
   kanonische Restart-Sensorentscheidung** vervollstaendigt, die intern
   ausschliesslich die bereits bestehende, kanonische
   `applySensorSelectionDecision`-Funktion aufruft – keine zweite
   Regelimplementierung (Abschnitt 6.3).
3. Boot-lokale Prozesszeitstempel werden nie unveraendert ueber einen
   Neustart hinweg verglichen; jede Wiederaufnahme lauft ueber die bereits
   bestehende, getestete `decideProcessTransition(..., RecoveryResume, ...)`
   mit einem korrekt neu berechneten `recoveredState` (Abschnitt 6.5).
4. Ein konservatives, ehrlich benanntes Beobachtungs-/Fortschrittsmodell
   (kein erfundenes Aktivitaetskennfeld) haelt sicher beobachtete Laufzeit,
   Quelle/Konfidenz und optional einen erst mit Inbetriebnahme verfuegbaren
   temperaturgewichteten Anteil getrennt (Abschnitt 6.7).
5. Ein Ausfallintervall wird als reine Zeitgrenzenberechnung von der
   fachlichen Bewertung gegen eine konkrete Phasen-/Haltegrenze getrennt
   (Abschnitt 6.6). Nur bei uebereinstimmender Bewertung beider Grenzen
   erfolgt eine automatische Entscheidung; sonst `DecisionRequired`.
6. Jede Recoveryaktivierung und jede spaetere Zeitkorrektur ist eindeutig
   und idempotent, ohne dass RAM vor einem erfolgreichen Commit mutiert wird
   (Abschnitt 6.4, 6.8).

### Nicht-Ziele

- **Kein NTP-Port/-Adapter** (unveraendert aus Revision 1; `ITimeSource`
  bleibt der einzige Zeitport, `EspTimerTimeSource::unixTimeSeconds()` bleibt
  `std::nullopt`).
- **Kein kalibriertes Temperatur-Aktivitaetskennfeld** (Gate C, Abschnitt 6.7).
- **Kein neues Nachrichten-/Journalsystem**; `RuntimeMessage`/
  `MessageClass::DecisionRequired` (`run_commands.hpp:211-261`) wird
  wiederverwendet.
- **Keine UI-/Web-Darstellung.**
- **Keine Aenderung an #24** (Fehlerklassen, `SAFE_BOOT`, Latch). #18
  konsumiert ausschliesslich die bereits vorhandenen
  `RunPersistenceLoadStatus`-Werte und `BlockedIndeterminate`
  (Abschnitt 6.10).
- **Keine produktive Composition-Root-Verdrahtung.** `src/main.cpp` (29
  Zeilen, instanziiert nur `DevicePlatform`/`FermentationApplication`) und
  `main/app_main.cpp` (105 Zeilen, instanziiert zusaetzlich nur
  `EspTimerTimeSource` fuer Heartbeat-Logging) werden von diesem Plan
  **nicht** geaendert. Kein produktiver `IStateStore`-Adapter existiert
  (`lib/device_platform_esp_idf/src/` enthaelt nur
  `esp_timer_time_source.*`); dessen Bau ist #29/#90 (Abschnitt 6.9).
- **Keine Erweiterung von `IPlatformServices`** (aktuell nur `ready()`,
  `lib/device_platform/src/platform_services.hpp:5-16`) um `IStateStore`
  oder `ITimeSource` – das waere genau die vorsorgliche Hardware-
  Vorwegnahme, die AGENTS.md verbietet und die zu #29/#90 gehoert.

## 5. Befund des aktuellen Codes (verifiziert, Revision 2)

Zusaetzlich zu Revision 1 (Abschnitt 5, weiterhin gueltig) wurden folgende,
fuer die Korrektur der elf Befunde entscheidende Tatsachen verifiziert:

**5.1 `RunPersistenceCoordinator::writeSnapshot` ist an `Ready`/`ReadyEmpty`
gebunden.** `run_persistence_coordinator.cpp:292-294`: jeder Schreibpfad
(`persistCommand`, `persistTransition`, `persistSensorSelection`,
`checkpointPeriodic`) beginnt mit `if (state_ != Ready && state_ !=
ReadyEmpty) return unavailableResult();`. `unavailableResult()`
(`:61-73`) liefert fuer `LoadedActiveRun` den Status `RecoveryPending` –
**keine** dieser vier Methoden ist aus `LoadedActiveRun` aufrufbar. Es gibt
aktuell **keinen** Uebergang `LoadedActiveRun -> Ready` im gesamten
Coordinator; `state_` wird nur bei `NoActiveRun`-Restore auf `ReadyEmpty`
(`:257-259`) und bei jedem aktiven Lauf unveraendert auf `LoadedActiveRun`
(`:261`) gesetzt.

**5.2 `persistTransition` akzeptiert `RecoveryResumed`/`RecoveryRejected`
nicht als eligible Grund.** `run_persistence_coordinator.cpp:18-35`
(`eligibleTransition`) listet ausschliesslich elf prozessinterne Gruende
(`QualificationTrackingStarted` ... `HoldFinishedByUser`) – `RecoveryResumed`
und `RecoveryRejected` (definiert in `process_state_machine.hpp`, erzeugt in
`decideRecoveryEvent`) sind **nicht** enthalten. Selbst mit einem gelockerten
Zustands-Guard waere `persistTransition` fuer die Recoveryaktivierung damit
technisch die falsche Methode, unabhaengig vom Ausgangszustand.

**5.3 `decideProcessTransition(..., ProcessEvent::RecoveryResume, ...)`
existiert bereits vollstaendig und ist bereits getestet.**
`process_state_machine.cpp:741-780` (`decideRecoveryEvent`):
- prueft `validRecoveryTarget(recovered.state)` (`:131-152`, exakt die acht
  Zustaende aus `docs/RECOVERY_AND_INTERRUPTION.md`, "Phasenbezogene
  Wiederaufnahme"),
- prueft `runtimeShapeIsValid`, `runtimeTimeIsValid(recovered,
  monotonicMillis)` (`:192-203` – **prueft nur, dass kein Zeitstempel in der
  Zukunft relativ zu `monotonicMillis` liegt; kennt keine Boot-Epoche**),
- prueft `stateMatchesRunSnapshot`,
- prueft fuer `WaitingForProduct` bereits `elapsedOptional(monotonicMillis,
  recovered.stateEnteredAtMillis, snapshot->maximumProductWaitMinutes)`
  (`:762-764`) und lehnt bei bereits abgelaufener Wartezeit ab,
- setzt bei `Preheating` `qualificationValidSinceMillis.reset()` (`:772`),
- setzt bei `QualifyingTarget` **bereits automatisch** den Uebergang zu
  `ReachingTarget` mit `stateEnteredAtMillis = monotonicMillis` und
  zurueckgesetzter Qualifikation (`:773-776`) – "QUALIFYING_TARGET beginnt
  neu" ist damit **bereits implementiert**.

Diese Funktion ist der **einzige** Ort, an dem die phasenbezogene
Wiederanlaufregel (Revision-1-Abschnitt 6.7) entschieden wird. #18 baut sie
**nicht neu**, sondern liefert ihr ein korrekt vorbereitetes `recoveredState`
(Abschnitt 6.5) und behandelt die Faelle, die diese Funktion strukturell
nicht selbst entscheiden kann (Ambiguitaet, Abschnitt 6.6).

**5.4 `evaluatePhase` lehnt `RestartRevalidationPending` strukturell ab.**
`sensor_selection.cpp:768-770`: `case NoActiveRun: case
RestartRevalidationPending: return reject(InvalidContext);`. Ein aus dem
Restore geladener Zustand kann deshalb **nicht** unveraendert an
`applySensorSelectionDecision` uebergeben werden.

**5.5 `evaluateNormalProduct`/`evaluateNormalAir` haben bereits einen
Blocked->Allowed-Pfad ueber `RecoveryRevalidation`,
`evaluateAirFallbackActive` nicht.** `sensor_selection.cpp:295-298`
(NormalProduct) und `:328-332` (NormalAir): bei gueltiger Evidenz und
`permission == Blocked` wird `Allowed` mit Ursache `RecoveryRevalidation`
vorgeschlagen. `evaluateAirFallbackActive` (`:541-674`) hat **keinen**
solchen Pfad – jeder Eintritt setzt `permission` entweder unveraendert
(`Allowed`, Normalfall) oder auf `Blocked` beim Eintritt in `SafeLocked`
(`:564`); es existiert keine Stelle, die `Blocked -> Allowed` innerhalb
dieser Phase vorschlaegt. `validRuntimeCombination` (`:110-152`) verbietet
die Kombination `AirFallbackActive` + `Blocked` **nicht** strukturell
(`mustBeBlocked` an `:120-124` enthaelt `AirFallbackActive` nicht) – die
Kombination ist erlaubt, nur die Regelfunktion behandelt sie noch nicht.

**5.6 `computeRestartSensorSelection` ist isoliert, kein Produktionsaufrufer,
ignoriert beide Eingaben.** Bestaetigt (Revision 1, weiterhin gueltig):
`sensor_selection.cpp:890-907`, `static_cast<void>(persisted)`,
`static_cast<void>(program)`.

**5.7 Der einzige Produktionsaufrufer von `applySensorSelectionDecision` ist
der manuelle Kommandopfad, nicht ein "automatischer Bewertungszyklus".**
`run_commands.cpp:1183` (`decideApplySensorSelectionAction`,
`run_commands.hpp:402-405`). `RunPersistenceCoordinator::
persistSensorSelection` (`run_persistence_coordinator.cpp:775-839`) nimmt
eine bereits fertige `SensorSelectionStateMutation` entgegen und wird
ebenfalls nirgends produktiv aufgerufen.

**5.8 `RunCommandState` hat kein Fortschritts-/Dauerfeld.**
`run_commands.hpp:311-351`, vollstaendige Feldliste verifiziert – kein
Fortschritts-, Prozent- oder gewichteter Wert. `ActiveRun` (`run_snapshot.hpp`)
traegt ebenfalls keine nominale/verstrichene Dauer; `fermentationDurationMinutes`
existiert ausschliesslich in `ProcessRunSnapshot` (Konfigurationswert, keine
Laufzeitgroesse).

**5.9 `clearActiveRunState` ist die einzige Implementierung fuer jeden
terminalen Laufpfad.** `run_commands.cpp:494-502`, setzt
`activeProgramRun`, `activeManualRun`, `processRunSnapshot`, `activeRunId`,
`activeRunSensorMode`, `sensorSelection` zurueck und `sensorSelectionRuntime
= SensorSelectionRuntimeState{}` – **nicht** `processState` oder
`runRevision`.

**5.10 Schema-1->2-Migration ist ein einziger, versionsgegateter Decoder,
kein verkettetes Zwischenformat.** `run_persistence_codec.cpp:768-784`:
genau ein `if (schemaVersion >= kSensorSelectionFieldIntroducedInSchema)`
(Konstante `= 2U`, `:23`) in `decodeRunPersistenceSnapshot(payload,
schemaVersion)`; der `else`-Zweig bildet fehlendes `sensorSelection` explizit
auf `LegacyUnknown` ab. Etablierte Tests fuer gemischte Schema-Kombinationen:
`test_head_reference_accepts_known_schemas_and_rejects_unknown_ones`,
`test_committed_head_accepts_mixed_current_and_fallback_schema`
(`test/test_run_checkpoint_codec/`),
`test_load_fallback_orphan_and_schema_epoch_matrix`
(`test/test_run_persistence_coordinator/`).

**5.11 Der UTC-Anker fuer den letzten Kontrollpunkt existiert bereits,
ist aber nicht oeffentlich zugaenglich.** `RunPersistenceRawRecord`
(`run_persistence_contract.hpp`) traegt bereits `std::optional<std::int64_t>
utcUnixSeconds` neben `checkpointRevision`; jeder Schreibpfad
(`writeSnapshot:319-321`) kodiert `time.utcUnixSeconds` unbedingt in die
`StorageEnvelope`. Dieser Wert ist jedoch coordinator-intern (`slots_[]`)
und wird von `RunPersistenceLoadResult` (nur `status`, `snapshot`) heute
**nicht** nach aussen gereicht. Ein neues persistiertes
`lastReliableUtcAnchorSeconds`-Feld im Snapshot-Payload ist deshalb
**nicht** noetig – es genuegt, den bereits erfassten Envelope-Wert ueber die
Coordinator-API sichtbar zu machen (Abschnitt 6.6.1).

**5.12 `IPlatformServices` stellt weder `IStateStore` noch `ITimeSource`
bereit.** `lib/device_platform/src/platform_services.hpp:5-16`: einzige
Methode `ready()`. Kein produktiver `IStateStore`-Adapter existiert;
`RunPersistenceCoordinator` wird nirgends produktiv instanziiert (Grep
ausserhalb `test/` und der eigenen Implementierungsdateien ergibt keinen
Treffer).

## 6. Fachvertraege

### 6.1 Modul- und Dateizuordnung (ADR-013, unveraendert)

Neue/geaenderte Dateien bleiben unter `lib/fermentation_app/src/`, keine
Abhaengigkeit auf `device_platform_esp_idf` oder
`device_platform_test_support` ausserhalb von `test/`.

Neue Dateien:

- `run_recovery_time.hpp/.cpp` – reine Zeitgrenzenberechnung und getrennte
  fachliche Bewertungsfunktionen (6.6).
- `run_progress.hpp/.cpp` – `RunProgressState` und die Beobachtungs-
  /Korrekturlogik (6.7).
- `run_recovery_coordinator.hpp/.cpp` – die produktive, aber ausschliesslich
  nativ verdrahtete Wiederanlauf-Orchestrierung (6.8).

Geaenderte Dateien: `sensor_selection.hpp/.cpp` (6.3),
`run_persistence_contract.hpp/.cpp` (6.4, 6.7 Wireformat),
`run_persistence_codec.cpp` (Schema-3-Migration),
`run_persistence_coordinator.hpp/.cpp` (6.4, neue Methoden/Enums),
`run_commands.hpp/.cpp` (`RunCommandState::runProgress`,
`clearActiveRunState`, Laufinitialisierung), `process_state_machine.hpp/.cpp`
(**eine gezielte Vertragsaenderung**: `TransitionRequest::
recoveredWaitTimeDefinitelyExpired`, dritter Ausgang in
`decideRecoveryEvent`, 6.5).
**Nicht geaendert:** `fermentation_application.hpp/.cpp`, `src/main.cpp`,
`main/app_main.cpp` (6.9).

### 6.2 Einheitlicher Eigentuemer fuer Fortschritts-/Zeitqualitaetszustand (Befund 4)

Genau **eine** Quelle: `RunCommandState::runProgress : RunProgressState`
(neues Feld, analog zu `sensorSelection`/`sensorSelectionRuntime`, **nicht**
in `ProcessRuntimeState` – dessen checkpointfaehige Form bleibt unveraendert
und ist ein anderer fachlicher Belang, die reine Prozess-Zeitstempel-/
Sequenzform). `ProcessRuntimeState` erhaelt **kein** zusaetzliches
`recoveryTimeQuality`-Feld; die Zeitqualitaet lebt ausschliesslich in
`RunProgressState.timeQuality`.

```cpp
enum class RunTimeQualityStatus : std::uint8_t {
    NoReliableTime,       // seit Laufstart noch kein UTC-Abgleich
    RecoveryTimePending,  // Wiederanlauf erfolgt, Intervall noch unentschieden
    Evaluated,            // Intervall abschliessend bewertet (certain oder
                           // ausdruecklich als endgueltig unentscheidbar markiert)
};
```

Vollstaendiger Lebenszyklus:

- **Initialisierung bei neuem Lauf:** in `decideProgramStart`/
  `decideManualStart` (`run_commands.cpp:661-679`, wo bereits
  `decision.after.sensorSelectionRuntime`/`sensorSelection` gesetzt werden)
  wird zusaetzlich `decision.after.runProgress = RunProgressState{}` (Default:
  `NoReliableTime`, `observedRunSeconds=0`) gesetzt.
- **Restore/Migration:** `restoreRunPersistenceSnapshot`
  (`run_persistence_contract.cpp`) kopiert `snapshot.runProgress`
  unveraendert in `restored.runProgress` – **kein** Rebasing noetig, da
  `RunProgressState` ausschliesslich Dauer-/Ankerwerte, keine boot-lokalen
  Zeitstempel traegt (Abgrenzung zu `ProcessRuntimeState`, siehe 6.5).
  Schema 1/2 (kein `runProgress`) -> expliziter `NoReliableTime`-Default,
  niemals "0 Sekunden beobachtet" als impliziter Fortschritt (Befund 8).
- **Persistenz-Mutation:** ausschliesslich ueber
  `RunPersistenceCoordinator::activateLoadedRun` (Erstaktivierung, 6.4) und
  `RunPersistenceCoordinator::persistRecoveryTimeCorrection` (spaetere
  Korrektur, 6.8) – keine dritte Persistenz-Schreibstelle. Die reine
  RAM-Hilfsfunktion `accountObservedRunSeconds` (6.7) mutiert
  `RunCommandState::runProgress` **im Arbeitsspeicher**, bevor der naechste
  ohnehin bestehende Kontrollpunkt (`checkpointPeriodic`, unveraendert) den
  dann aktuellen `RunCommandState` persistiert – das ist keine zusaetzliche
  Persistenz-Schreibstelle, sondern derselbe bereits bestehende Kontrollpunkt-
  Mechanismus, der jetzt zusaetzlich dieses Feld mitfuehrt.
- **Abbruch/Abschluss/Tombstone:** `clearActiveRunState`
  (`run_commands.cpp:494-502`) erhaelt eine zusaetzliche Zeile
  `state.runProgress = RunProgressState{};`, konsistent mit der bestehenden
  `sensorSelectionRuntime`-Behandlung dort.
- **Snapshot-/Codec-Invariante:** `RunPersistenceSnapshot::runProgress :
  std::optional<RunProgressState>` ist vorhanden genau dann, wenn
  `variant != NoActiveRun` – identische Invariante wie fuer
  `sensorSelection` (`run_persistence_contract.cpp:220`), im selben
  Validator (`validateRunPersistenceSnapshot`) um eine Pruefzeile ergaenzt.
- **Keine zweite Kopie:** `ProcessRuntimeState`, `SensorSelectionRuntimeState`
  und `RunProgressState` bleiben drei disjunkte, klar zustaendige Felder auf
  `RunCommandState`; keines dupliziert eine Groesse eines anderen.

### 6.3 Kanonische Restart-Sensorentscheidung (Befund 1, Gate A)

`computeRestartSensorSelection` wird **erweitert**, nicht durch eine zweite
Regelimplementierung ersetzt. Neue Signatur:

```cpp
[[nodiscard]] SensorSelectionStateMutation computeRestartSensorSelection(
    const PersistedSensorSelectionState& persisted,
    RunSensorMode lastActiveMode,
    const SensorSelectionProgramContext& program,
    const CrossRolePlausibilityContext& plausibility,
    std::uint64_t nowMonotonicMillis);
```

Zwei-Schritt-Ablauf, **kein** zweiter Regelkern:

1. **Reine, deterministische Phasenrekonstruktion** (kein Sensorbezug):
   - `provenance == FallbackActive` -> Zielphase `AirFallbackActive`
     (Modus ist per Invariante immer `Air`).
   - jede andere Provenienz (`InitialSelection`, `ReturnedToProduct`,
     `LegacyUnknown`) -> `NormalProduct` falls `lastActiveMode == Product`,
     sonst `NormalAir`. Begruendung: laut #21 (Abschnitt 6.4.12,
     "Re-Arm-Regel") wird jede laufende Wartezeit oder Rueckkehrvalidierung
     nach einem Neustart verworfen – nur **abgeschlossene** Provenienzwerte
     sind ueberhaupt persistiert (`PersistedSensorSelectionState.provenance`
     kennt keinen "mitten in der Validierung"-Wert); die Rekonstruktion
     trifft deshalb nie eine Phase, die eine laufende Sub-Validierung
     voraussetzt.
   - Konstruiert `SensorSelectionStateView{activeMode=lastActiveMode,
     persisted=persisted, runtime={phase=Zielphase,
     permission=Blocked, alle vier boot-lokalen Laufzeitfelder auf ihren
     Default (kein Wert)}}`. `permission=Blocked` ist der verbindliche
     Boden aus #21 und bleibt es, bis Schritt 2 ihn ausdruecklich aendert.
2. **Delegation an die bestehende kanonische Funktion:**
   `applySensorSelectionDecision(view, {expected=view, program,
   plausibility, userAction=std::nullopt}, nowMonotonicMillis)`. Das
   Ergebnis (inklusive `SensorSelectionApplyStatus`, ggf. `cause =
   RecoveryRevalidation`) ist die einzige Quelle der eigentlichen
   Freigabeentscheidung.

**Notwendige, minimale Erweiterung von `evaluateAirFallbackActive`**
(Befund 5.5): direkt nach der bestehenden SafeLocked-Eintrittspruefung
(`sensor_selection.cpp:562-570`) und vor der `userAction`-Verzweigung wird,
symmetrisch zu `evaluateNormalProduct`/`evaluateNormalAir`
(`:295-298`/`:328-332`), ergaenzt:

```cpp
if (airValid && coolingValid &&
    current.runtime.permission == SensorPeltierPermission::Blocked) {
    proposal.runtime.permission = SensorPeltierPermission::Allowed;
    proposal.cause = SensorSelectionDecisionCause::RecoveryRevalidation;
    return proposal;
}
```

Das ist dieselbe bereits akzeptierte Regel (Blocked -> Allowed bei gueltiger
Evidenz, Ursache `RecoveryRevalidation`), nur auf die eine Phase erweitert,
die sie vorher nicht brauchte (Produkt ist fuer `AirFallbackActive`
strukturell nicht erforderlich, konsistent mit der bestehenden
SafeLocked-Bedingung derselben Funktion, die ebenfalls nur `air`/`cooling`
prueft). Keine neue Ursache, keine neue Phase, keine zweite Funktion.

`RestartRevalidationPending`/`Blocked` bleibt der verbindliche Restore-
Default (#21, unveraendert) und wird ausschliesslich durch diesen
Zwei-Schritt-Ablauf – atomar committet ueber `activateLoadedRun` (6.4) –
verlassen.

**Bestehende #21-Tests:** Die drei Tests
`test_restart_recommendation_is_fail_closed_for_*`
(`test_sensor_selection.cpp:1193-1250`) pruefen aktuell nur die
eingabeunabhaengige Fail-Closed-Antwort der alten Stub-Signatur. Sie werden
auf die neue Signatur umgestellt und um Faelle mit **gueltiger** Evidenz
ergaenzt, die zeigen, dass `RecoveryRevalidation` tatsaechlich Permission
`Allowed` liefert – nicht nur, dass Blocked der Default bleibt (9.3).

### 6.4 `RunPersistenceCoordinator::activateLoadedRun` (Befund 2)

**Geteilter Zwei-Phasen-Schreibhelper.** `writeSnapshot`
(`run_persistence_coordinator.cpp:287-529`) wird intern in eine
parametrisierte private Hilfsfunktion zerlegt, die als Argumente zusaetzlich
zum bisherigen Verhalten entgegennimmt:

- den erforderlichen Ausgangszustand (bisher implizit `Ready`/`ReadyEmpty`),
- den Zielzustand bei wiederholbarem Fehler (bisher immer zurueck zu
  `Ready`/`ReadyEmpty` – fuer die neue Methode muss das `LoadedActiveRun`
  sein, **nicht** `Ready`, da der Lauf sonst faelschlich als aktivierbar
  erschiene, obwohl die Aktivierung fehlgeschlagen ist),
- den Zielzustand bei Erfolg.

`writeSnapshot` selbst behaelt sein bisheriges Verhalten fuer die vier
bestehenden Aufrufer exakt bei (keine Verhaltensaenderung an
`persistCommand`/`persistTransition`/`persistSensorSelection`/
`checkpointPeriodic`).

**Neue Methode:**

```cpp
[[nodiscard]] RunPersistenceResult activateLoadedRun(
    RunCommandState& current,
    const CrossRolePlausibilityContext& plausibility,
    const TransitionDecision& recoveryDecision,
    const RunProgressState& progressInit,
    const RunCheckpointTime& time);
```

Ablauf:

1. Vorbedingung `state_ == LoadedActiveRun`; sonst `NotEligible`.
2. Baut eine **lokale Kandidatenkopie** von `current` (RAM bleibt
   unveraendert bis Schritt 5 – direkte Antwort auf Befund 2, "vor dem
   Persistenzcommit den aktuellen RAM-Zustand nicht mutieren").
3. Wendet auf die Kandidatenkopie in dieser Reihenfolge an:
   a. `computeRestartSensorSelection(...)` (6.3) mit der uebergebenen
      `plausibility` -> setzt `sensorSelectionRuntime`, ggf.
      `sensorSelection.provenance`/`lastDecisionCause`. `plausibility.phase`
      entspricht dabei der **rekonstruierten Zielphase** aus 6.3 Schritt 1
      (z. B. `Fermenting`), nicht `RecoveryEvaluation` – die Plausibilitaets-
      bewertung bezieht sich fachlich auf den Zustand, in den der Lauf
      zurueckkehrt, nicht auf den transienten Boot-Zwischenzustand.
      `sensorSelection.lastDecisionRunRevision` wird **nicht** hier,
      sondern erst in Schritt 3d final gesetzt (s. u.), um eine
      Revisionsnummer aus einem noch nicht committeten Zwischenschritt zu
      vermeiden.
   b. `recoveryDecision` (bereits von `RunRecoveryCoordinator` ueber
      `decideProcessTransition(..., RecoveryResume, ...)` berechnet, 6.5) ->
      setzt `processState`.
   c. `progressInit` (von `RunRecoveryCoordinator` ueber 6.6/6.7 berechnet)
      -> setzt `runProgress`.
   d. **Eigentuemerschaft der Revisionsnummer (Befund 4/Praezisierung):**
      `activateLoadedRun` erhoeht `candidate.runRevision` genau **einmal**
      (`++candidate.runRevision`, identisches Muster zu
      `decideProgramStart`, `run_commands.cpp:668`) – unabhaengig davon, ob
      Schritt 3a eine persistenzwuerdige Sensoraenderung
      (`RecoveryRevalidation`) ergab oder nicht, da eine
      Recoveryaktivierung als Bootereignis immer persistenzwuerdig ist.
      Der von `applySensorSelectionDecision` in Schritt 3a intern
      berechnete `resultingRunRevision`-Wert wird **verworfen**;
      stattdessen setzt `activateLoadedRun` nach der Erhoehung
      `candidate.sensorSelection->lastDecisionRunRevision =
      candidate.runRevision`, sofern Schritt 3a eine Ursache ungleich
      `None` lieferte. Damit gibt es genau eine Inkrementquelle, und die
      #21-Invariante `lastDecisionRunRevision <= runRevision` gilt
      trivial (Gleichheit) statt durch zwei unabhaengige Zaehlungen
      zufaellig verletzt zu werden. Die von Schritt 3a gelieferte
      `SensorSelectionEvent`/`SensorSelectionNotice` (deren
      `resultingRunRevision`-Feld intern ebenfalls den verworfenen Wert
      traegt, `sensor_selection.cpp:877-885`) wird **vor** der Rueckgabe
      des Ergebnisses an den Aufrufer mit derselben restamped
      `candidate.runRevision` ueberschrieben – Ereignis/Notice und der
      tatsaechlich committete `runRevision`-Wert weichen nie voneinander
      ab.
4. Validiert die vollstaendige Kandidatenkopie ueber die bestehende
   `makeRunPersistenceSnapshot`/`validateRunPersistenceSnapshot`-Kette
   (wiederverwendet, keine zweite Validierung).
5. Schreibt **eine einzige** atomare Prepared/Committed-Head-Transaktion
   ueber den geteilten Helfer aus Schritt "Geteilter
   Zwei-Phasen-Schreibhelper" mit `mutationKind =
   RunPersistenceMutationKind::RecoveryActivation` (neuer Wert `4U`) und
   `snapshot.trigger = RunCheckpointTrigger::RecoveryActivation` (neuer
   Wert `5U`). Das ist die direkte Antwort auf "keine zwei
   widerspruechlichen Writes": Sensorreaktivierung, Prozess-Recovery-
   Entscheidung und Progress-Initialisierung sind **eine** Revision, nicht
   zwei getrennte Persistierungen wie im verworfenen Revision-1-Entwurf
   (6.4 Schritt 3a + 3d).
6. **Nur bei erfolgreichem Commit:** `current = candidate` (RAM erst jetzt
   uebernommen), `state_ = Ready`.
7. Bei `WriteFailed`/`CapacityExceeded` (wiederholbar): `current`
   unveraendert, `state_` faellt zurueck auf `LoadedActiveRun` (fail-closed
   – der Lauf bleibt im Wiederanlauf-Wartezustand, keine Freigabe).
8. Bei `PersistenceIndeterminate`/`CommitOutcomeUnknown`: bestehendes
   Verhalten wiederverwendet – `enterBlockedIndeterminate()`, `state_ =
   BlockedIndeterminate` (identisch zum bestehenden Muster in
   `writeSnapshot`).

**Idempotenz bei Wiederholung/Neustart** (Befund 2, letzter Punkt) – ohne
neue Buchfuehrung, aus drei bereits bestehenden Tatsachen ableitbar:

1. `sensorSelectionRuntime` ist nie Teil des Wireformats
   (`run_persistence_contract.cpp:270-277`, Kommentar "kein Wireformat
   traegt ihn").
2. `loadAndInitialize()` setzt `state_ = LoadedActiveRun` unbedingt fuer
   **jeden** aktiven Lauf (`:261`), unabhaengig davon, ob eine vorherige
   Aktivierung bereits committet wurde.
3. Restore setzt `sensorSelectionRuntime` unbedingt fail-closed auf
   `RestartRevalidationPending`/`Blocked` (`:277-281`), auch fuer einen
   bereits einmal aktivierten, dann erneut unterbrochenen Lauf.

Daraus folgt: jeder Neustart durchlaeuft den vollstaendigen
Reaktivierungsablauf zwangslaeufig erneut, unabhaengig vom Ergebnis eines
vorherigen Boots – kein Wiederholungs-/Dedup-Token noetig, kein Risiko einer
doppelten Aktivierung.

### 6.5 Boot-lokale Prozesszeitstempel (Befund 5)

Kein generisches "Phasen-Dauer"-Objekt. Zwei getrennte, bereits im Code
angelegte Mechanismen:

**Mechanismus A – boot-lokale Zeitstempel ueberleben nie roh.**
`ProcessRuntimeState.stateEnteredAtMillis`, `.targetReachStartedAtMillis`,
`.qualificationValidSinceMillis` werden **nie** unveraendert aus dem alten
Boot in `decideProcessTransition(..., RecoveryResume, recoveredState)`
uebergeben. `RunRecoveryCoordinator` (6.8) berechnet `recoveredState` vor
jedem Aufruf explizit:

| Zielphase (aus `validRecoveryTarget`) | `stateEnteredAtMillis` | `targetReachStartedAtMillis` | `qualificationValidSinceMillis` |
|---|---|---|---|
| `Preheating`, `ReachingTarget` | `now` (Ziel wird per Doku "erneut angefahren", keine alte Dauer zaehlt) | `now` | `nullopt` (bereits von `decideRecoveryEvent:772` fuer `Preheating` gesetzt; hier fuer `ReachingTarget` konsistent vorgegeben) |
| `WaitingForProduct` | **Sonderfall**, siehe unten (einzige Phase mit fachlich relevanter grenzwertiger Dauer) | unbenutzt (`stateHasTargetReachTimer` = false) | unbenutzt |
| `QualifyingTarget` | `now` (wird von `decideRecoveryEvent:773-776` ohnehin automatisch zu `ReachingTarget` mit `stateEnteredAtMillis=monotonicMillis` konvertiert – Coordinator liefert nur den Eingangswert, die Doku-Regel "QUALIFYING_TARGET beginnt neu" wird durch die **bereits bestehende** Funktion erfuellt) | `now` | `nullopt` |
| `Fermenting`, `Cooling`, `CoolHolding`, `ManualHolding` | `now` (reines Diagnose-/"seit wann in diesem Zustand"-Feld; die fachlich relevante verstrichene Dauer liegt in `RunProgressState`, nicht hier, 6.2/6.7) | unbenutzt | unbenutzt |

Begruendung fuer die einheitliche `now`-Regel ausserhalb von
`WaitingForProduct`: `runtimeTimeIsValid` (`process_state_machine.cpp:
192-203`) prueft ausschliesslich "nicht in der Zukunft relativ zur
uebergebenen `monotonicMillis`" und kennt keine Boot-Epoche – ein roh
kopierter alter Wert wuerde bei einem frischen Boot (kleine
`monotonicMillis`) fast immer als "in der Zukunft" erscheinen und **jede**
nachfolgende `decideProcessTransition`-Auswertung mit
`TimeWentBackwards` scheitern lassen. Das ist ein realer, durch Byte-Kopie
(`run_persistence_contract.cpp:261`, `restored.processState =
snapshot.processState;`) bereits heute vorhandener Fehlerpfad, den dieser
Plan durch die obenstehende Tabelle schliesst.

**`WaitingForProduct`-Sonderfall:** die einzige Phase, in der eine ueber den
Ausfall hinweg **korrekt fortgefuehrte** Wartedauer fachlich noetig ist
(maximale Wartezeit, Akzeptanzkriterium).

**Warum `stateEnteredAtMillis` hierfuer strukturell ungeeignet ist:** eine
naheliegende Idee waere, `recoveredState.stateEnteredAtMillis = now -
priorElapsedMillis` zu setzen, wobei `priorElapsedMillis` die vor dem
Ausfall bereits verstrichene Wartezeit ist. Das ist auf `std::uint64_t`
**nicht darstellbar**: `now` ist bei einem frischen Boot eine kleine Zahl
(Millisekunden seit Systemstart), waehrend `priorElapsedMillis` typischerweise
Minuten bis Stunden betraegt – die Subtraktion liefe auf einen riesigen
Wrap-around-Wert, `runtimeTimeIsValid` (`:194`) wuerde
`stateEnteredAtMillis > monotonicMillis` erkennen und **jede**
`decideProcessTransition`-Auswertung mit `TimeWentBackwards` verwerfen. Ein
Clamping auf `0` vermeidet den Overflow, aber `elapsedOptional(now, 0,
maxWait)` ist dann bei einem frischen Boot immer `false` – die Wartezeit
wuerde faktisch lautlos auf einen vollen neuen `maximumProductWaitMinutes`-
Zyklus zurueckgesetzt, was das Akzeptanzkriterium unterlaeuft.

**Deshalb wird die Bootgrenze fuer diese eine Entscheidung vollstaendig von
`stateEnteredAtMillis`-Arithmetik entkoppelt.** Die Ausfallzeit kommt
ausschliesslich aus der in 6.6 getrennten Intervallbewertung; das Ergebnis
ist eine der drei dort definierten Fachentscheidungen
(`DefinitelyWithinBound`, `DefinitelyExceedsBound`, `Uncertain`),
angewandt auf `priorElapsedSeconds = (snapshot.checkpointMonotonicMillis -
snapshot.processState.stateEnteredAtMillis) / 1000` (reine Alt-Boot-
Arithmetik, beide Operanden aus demselben Boot, immer gueltig – kein
Vergleich monotoner Werte ueber die Bootgrenze hinweg, Befund 6) als
`priorElapsedSeconds`-Parameter von `evaluateRecoveryTimeDecision` (6.6.2)
gegen `maximumProductWaitMinutes` als Schwelle:

- `DefinitelyWithinBound` -> `RunRecoveryCoordinator` setzt
  `recoveredState.stateEnteredAtMillis = now` (**immer sicher, kein
  Underflow-Risiko** – der Zeitpunkt des Wiederanlaufs selbst, nicht
  rekonstruiert) und ruft `decideProcessTransition(..., RecoveryResume,
  ...)` auf; die bereits bestehende `elapsedOptional`-Pruefung dort
  (`:762-764`) wirkt fuer diesen einen Aufruf nur noch als triviale
  Sicherheitspruefung (mit `stateEnteredAtMillis == now` immer `false`),
  **nicht** als die eigentliche Grenzentscheidung – die wurde bereits vom
  Coordinator getroffen.
- `DefinitelyExceedsBound` -> **Praezisierung (nicht der urspruenglich
  angenommene bestehende Pfad):** `decideWaitingForProduct`s automatischer
  Ablauf (`:993-996`) ist aus `RecoveryEvaluation` heraus **nicht**
  erreichbar – der Top-Level-Dispatch in `decideProcessTransition`
  verzweigt dort ueber `current.state == WaitingForProduct`, waehrend der
  Coordinator sich zu diesem Zeitpunkt in `current.state ==
  RecoveryEvaluation` befindet (`current` ist hier die frische, in Schritt
  3a mit `stateEnteredAtMillis = now` proposte `RecoveryEvaluation`-Instanz,
  **nicht** der geladene Altzustand – der Alt-Zustand existiert an dieser
  Stelle nur als `request.recoveredState`). Ein direkter Aufruf des
  Live-Pfads wuerde daher nie ausgefuehrt.
  Stattdessen wird `decideRecoveryEvent`
  (`process_state_machine.cpp:741-780`) um ein drittes, praezise begrenztes
  Ergebnis erweitert: `TransitionRequest` erhaelt ein neues Feld
  `bool recoveredWaitTimeDefinitelyExpired{false}`. Ist es gesetzt (nur
  zulaessig, wenn `request.recoveredState->state ==
  ProcessState::WaitingForProduct`), schlaegt `decideRecoveryEvent` **statt**
  des bisherigen unbedingten `Fault`-Uebergangs im `RecoveryReject`-Zweig
  einen Uebergang nach `Standby` mit dem bereits bestehenden
  `TransitionReason::ProductWaitExpired` vor – exakt dieselbe Einordnung
  (kein Fault, "Lauf als nicht gestartet protokollieren") wie der reguläre
  Live-Pfad, nur ueber eine neue, minimale Fallunterscheidung innerhalb der
  bereits bestehenden Funktion statt eines unerreichbaren Aufrufs des
  Live-Dispatches. Der Coordinator setzt dieses Feld ausschliesslich fuer
  den hier behandelten `DefinitelyExceedsBound`-Fall und ruft
  `decideProcessTransition` mit `ProcessEvent::RecoveryReject`,
  `recoveredState` (unveraendert aus der Rekonstruktion) und
  `recoveredWaitTimeDefinitelyExpired = true` auf.
- `Uncertain` -> **kein** automatischer Aufruf von `RecoveryResume` oder
  `ProductWaitExpired` in diesem Zyklus. Der Lauf bleibt in
  `RECOVERY_EVALUATION` mit `runProgress.timeQuality =
  RecoveryTimePending`; eine `DecisionRequired`-Meldung wird erzeugt
  (6.6.2). Kein automatischer Fortschritt, bis eine Benutzerentscheidung
  oder eine spaeter verlaessliche UTC-Zeit die Ambiguitaet aufloest (6.8,
  spaetere Korrektur).

**Bewusst dokumentierte Restgrenze (kein stiller Luecke, sondern explizit
benannt):** die obige Entscheidung ist korrekt fuer den Wiederanlaufmoment
selbst. Setzt sich der Lauf danach im selben Boot laenger als geplant in
`WaitingForProduct` fort (kein weiterer Neustart), misst die unveraendert
bestehende `decideWaitingForProduct`-Live-Auswertung die Wartezeit ab dem
neuen `stateEnteredAtMillis = now` – die vor dem Ausfall bereits
verstrichene Wartezeit fliesst in diese *weiteren*, nicht mehr
recovery-bezogenen Live-Zyklen nicht erneut ein. Eine Ruecktragung dieser
Information in die laufende Live-Auswertung wuerde entweder
`ProcessRuntimeState` um einen weiteren, ebenfalls boot-uebergreifend
gedachten Zeitwert erweitern (derselbe strukturelle Konflikt wie oben) oder
`processRunSnapshot.maximumProductWaitMinutes` nachtraeglich mutieren
(aktuell keine etablierte Mutierbarkeit dieses Feldes, staette einen neuen
Vertrag ein). Beides wird hier bewusst **nicht** eingefuehrt: Effekt ist,
dass ein wiederholt unterbrochener `WaitingForProduct`-Lauf im
Extremfall laenger warten kann, als `maximumProductWaitMinutes` einmalig
vorsieht – niemals kuerzer, niemals unsicher automatisch beendet. Das ist
eine bewusste, dokumentierte Restunschaerfe zugunsten von "nie faelschlich
zu frueh abbrechen", keine verdeckte Abweichung, und beruehrt keine
Aktorsicherheit (die Phase haelt nur eine Temperatur, keine
Fermentationszeit laeuft). Eine engere Nachverfolgung ueber mehrere
Neustarts hinweg waere eine materielle Erweiterung und wird nicht
nachtraeglich in diese Revision gezogen.

**Boot-lokale Sensorselektions-Timer** (`fallbackWaitStartedAtMonotonicMillis`,
`lastAppliedMonotonicMillis`, `returnValidation.*`,
`sensor_selection_types.hpp:58-70`) sind bereits durch #21 korrekt behandelt
(Restore setzt eine frische `SensorSelectionRuntimeState{}`,
`run_persistence_contract.cpp:277`) und werden durch die
Phasenrekonstruktion in 6.3 Schritt 1 erneut frisch konstruiert – keine
weitere Aenderung noetig, nur Konsistenzbestaetigung.

### 6.6 Ausfallintervall: Zeitgrenzen getrennt von fachlicher Bewertung (Befund 6)

**6.6.1 Reine Zeitgrenzenberechnung** (`run_recovery_time.hpp`):

```cpp
struct RecoveryTimeBoundsInput {
    std::optional<std::int64_t> currentUnixSeconds;             // ITimeSource::unixTimeSeconds()
    std::optional<std::int64_t> lastReliableCheckpointUnixSeconds; // aus dem Envelope, s. u.
    std::uint16_t configuredCheckpointIntervalMinutes;
};

struct RecoveryTimeBounds {
    bool hasReliableAnchor{false};
    std::uint64_t lowerBoundSeconds{0U};
    std::uint64_t upperBoundSeconds{0U};
};

[[nodiscard]] RecoveryTimeBounds computeRecoveryTimeBounds(
    const RecoveryTimeBoundsInput& input);
```

Regeln: fehlt `currentUnixSeconds` oder `lastReliableCheckpointUnixSeconds`
-> `hasReliableAnchor=false`, Grenzen `0`. Ist `currentUnixSeconds <
lastReliableCheckpointUnixSeconds` (rueckwaerts laufende UTC) ->
`hasReliableAnchor=false` (eine rueckwaerts laufende Uhr ist nie
vertrauenswuerdig fuer eine Vorwaerts-Dauer). Die Subtraktion ist
ueberlaufgeprueft (saettigt auf `hasReliableAnchor=false` statt zu
umlaufen). `obere Grenze = currentUnixSeconds -
lastReliableCheckpointUnixSeconds`; `untere Grenze = max(0, obere Grenze -
maximal moeglicher Kontrollpunktabstand)`, wobei der maximale Abstand aus
`configuredCheckpointIntervalMinutes` abgeleitet wird. **Keine** monotonen
Werte aus unterschiedlichen Boots werden verglichen – die Funktion nimmt
ausschliesslich UTC-Domaenenwerte entgegen; keinerlei `monotonicMillis`-Wert
irgendeines Boots ist Teil ihrer Eingabe. Die separate, ausschliesslich
Alt-Boot-interne Berechnung von `priorElapsedSeconds` (aus
`snapshot.checkpointMonotonicMillis - snapshot.processState.
stateEnteredAtMillis`, beide Werte garantiert aus demselben Boot) erfolgt
in `RunRecoveryCoordinator` (6.5), nicht in dieser Funktion.

**Bezug des UTC-Ankers:** `RunPersistenceLoadResult` wird um ein Feld
`std::optional<std::int64_t> lastReliableUtcAnchorSeconds` ergaenzt, direkt
aus dem bereits erfassten `RunPersistenceRawRecord::utcUnixSeconds` des
geladenen Slots gespeist (5.11) – **keine** Schema-/Payload-Aenderung, nur
eine bereits vorhandene, bislang nicht oeffentliche Information wird
sichtbar gemacht.

**6.6.2 Getrennte fachliche Bewertung, pro Aufrufstelle parametrisiert:**

```cpp
enum class RecoveryTimeDecisionOutcome : std::uint8_t {
    DefinitelyWithinBound,
    DefinitelyExceedsBound,
    Uncertain,
};

[[nodiscard]] RecoveryTimeDecisionOutcome evaluateRecoveryTimeDecision(
    const RecoveryTimeBounds& bounds, std::uint64_t priorElapsedSeconds,
    std::uint64_t thresholdSeconds);
```

Wertet **beide** Grenzen (`priorElapsedSeconds + lowerBoundSeconds` und
`priorElapsedSeconds + upperBoundSeconds`) gegen denselben
`thresholdSeconds` aus. Stimmen beide Bewertungen ueberein (beide
`< threshold` oder beide `>= threshold`), ist das Ergebnis eindeutig –
**auch wenn die Grenzen selbst ungleich sind** (direkte Antwort auf "auch
ungleiche Grenzen koennen dieselbe fachliche Entscheidung ergeben").
Stimmen sie nicht ueberein oder ist `bounds.hasReliableAnchor == false`,
ist das Ergebnis `Uncertain`. Diese eine Funktion wird an drei
Aufrufstellen mit ihrem jeweils eigenen `thresholdSeconds` verwendet:
`WaitingForProduct` (`maximumProductWaitMinutes`), `CoolHolding`
(verbleibende Haltezeit), `Fermenting`/Fortschrittsgrenzen
(nominelle Dauer, 6.7) – keine der drei Aufrufstellen dupliziert die
Grenzlogik selbst.

### 6.7 Beobachtungs- und Fortschrittsmodell (Befund 7, Gate C)

```cpp
enum class RunTimeQualityStatus : std::uint8_t { /* siehe 6.2 */ };

struct RunProgressState {
    std::uint32_t observedRunSeconds{0U};   // sicher beobachtete Laufzeit,
                                             // NICHT "gewichtet" genannt
    RunSensorMode observedSource{RunSensorMode::Product}; // Produkt vor Luft
    RunTimeQualityStatus timeQuality{RunTimeQualityStatus::NoReliableTime};
    std::optional<std::uint32_t> weightedProgressSeconds; // s.u.
    // Boot-lokale Referenz fuer die naechste Delta-Berechnung (6.7-Schreib-
    // pfad). Wird bei jeder Aktivierung/Korrektur auf den dortigen `now`
    // gesetzt, ist selbst kein fachlicher Wert und wird nie ueber die
    // Bootgrenze hinweg verglichen (identisches Muster zu
    // `RunCheckpointSchedule::reset()`).
    std::uint64_t lastAccountedMonotonicMillis{0U};
};

// Reine Hilfsfunktion (run_progress.hpp): addiert die seit
// `state.lastAccountedMonotonicMillis` innerhalb DESSELBEN Boots
// verstrichene Zeit zu `observedRunSeconds` und aktualisiert die Referenz.
// Ausschliesslich fuer denselben Boot gueltig (Aufrufer darf `now` nie mit
// einem Wert aus einem anderen Boot aufrufen).
[[nodiscard]] RunProgressState accountObservedRunSeconds(
    RunProgressState state, RunSensorMode source, std::uint64_t now);
```

- `observedRunSeconds`: ehrlich benannter, ungewichteter Sekundenzaehler fuer
  tatsaechlich beobachtete, valide Regelzeit (Produktwert hat Vorrang vor
  Luftwert nur als **Quellen-/Konfidenzregel** fuer `observedSource`, nicht
  als Temperaturgewichtung).
  **Schreibpfade, praezise (Befund "kein Schreibort"):**
  1. `RunPersistenceCoordinator::activateLoadedRun` (6.4) setzt bei jeder
     Aktivierung `lastAccountedMonotonicMillis = now` (frischer, sicherer
     Bezugspunkt fuer diesen Boot) und laesst `observedRunSeconds`
     unveraendert (Wert ueberlebt den Neustart per Byte-Kopie, 6.2).
  2. `RunPersistenceCoordinator::persistRecoveryTimeCorrection` (6.8) darf
     einmalig, episodengebunden, die untere Ausfallintervallgrenze
     addieren (s. u.).
  3. `accountObservedRunSeconds(...)` ist eine reine, vollstaendig
     unit-getestete Funktion fuer den **laufenden** Zyklus waehrend
     `Fermenting`, aber **#18 verdrahtet sie in keinen produktiven
     Aufrufzyklus** – `FermentationApplication::update()` ist heute leer und
     es existiert im gesamten Repository noch keine allgemeine
     "jeden Zyklus den Prozess neu bewerten"-Orchestrierung (dieselbe
     Gate-B-Grenze wie in 6.9: #18 liefert die Bausteine, die produktive
     Verdrahtung einer laufenden Zyklusschleife ist kein Bestandteil dieses
     Issues und keiner seiner Abhaengigkeiten). Der Plan behauptet an
     keiner Stelle, `observedRunSeconds` wachse bereits waehrend
     ununterbrochenen Live-Betriebs – nur, dass der Mechanismus dafuer
     bereitsteht, getestet ist und an den beiden oben genannten
     Recovery-Stellen korrekt funktioniert.
- `weightedProgressSeconds`: bleibt in Release 1 strukturell **immer**
  `std::nullopt`. Das Feld existiert im Vertrag/Wireformat ausschliesslich,
  damit eine spaetere Inbetriebnahme-Kalibrierung keinen weiteren
  Schema-Bump braucht; #18 fuellt es **nicht**. Jede Stelle, die einen
  "temperaturgewichteten Fortschritt" im Sinn des Issue-Titels berechnen
  wuerde, bleibt `TBD_COMMISSIONING` und ausserhalb dieses Plans (Nicht-Ziel,
  Abschnitt 4) – der Plan behauptet an keiner Stelle, `observedRunSeconds`
  sei bereits die geforderte Temperaturgewichtung.
- **Ausfallzeit-Gutschrift:** wird **nicht** an
  `evaluateRecoveryTimeDecision`s Ergebnis gekoppelt – jene Funktion
  beantwortet ausschliesslich "wurde eine konkrete Phasen-/Haltegrenze
  ueberschritten", nicht "ist dieses Zeitintervall vertrauenswuerdig genug
  fuer eine Gutschrift" (Vermischung von Datenqualitaet und fachlicher
  Grenzbewertung, dieselbe Trennung wie in 6.6 gefordert). Stattdessen prueft
  `run_progress.cpp` direkt `bounds.hasReliableAnchor == true` **und** eine
  eigene, enge Bandbreitenschwelle (`upperBoundSeconds - lowerBoundSeconds`
  unterhalb eines konfigurierten Grenzwerts, analog zur bestehenden
  Kontrollpunktintervall-Grenze 1-60 Minuten). Nur wenn beides zutrifft, wird
  die **untere** Intervallgrenze (nie die obere) einmalig zu
  `observedRunSeconds` addiert. Ist der Anker unzuverlaessig oder die
  Bandbreite zu gross, erfolgt **keine** Gutschrift, unabhaengig davon, ob
  eine konkrete Phasen-/Haltegrenze ueberschritten wurde oder nicht
  ("keine erfundene Gutschrift ohne freigegebenes Modell", Gate C).
- **Episoden-Idempotenz statt kumulativer Korrektursumme** (Befund 7, letzter
  Punkt): jede Aktivierungsrevision (`activateLoadedRun`, 6.4) ist durch die
  dabei neu vergebene `runRevision` eindeutig identifiziert. Die spaetere
  Korrektur (6.8) prueft vor jeder Anwendung `runProgress.timeQuality !=
  Evaluated` (statt eine Summe fortzuschreiben); sobald einmal auf
  `Evaluated` gesetzt, wird dieselbe Episode nicht erneut korrigiert. Eine
  neue Unterbrechung erzeugt zwangslaeufig eine neue Aktivierungsrevision
  mit frischem `timeQuality = RecoveryTimePending` – keine
  Doppelanwendung ueber Episodengrenzen hinweg moeglich, ohne dass ein
  zusaetzlicher Zaehler gefuehrt werden muss.

### 6.8 `RunRecoveryCoordinator` (produktive, nativ getestete Orchestrierung)

Neue Klasse in `lib/fermentation_app/src/run_recovery_coordinator.hpp/.cpp`.
Konstruktion:

```cpp
RunRecoveryCoordinator(RunPersistenceCoordinator& persistence,
                       device_platform::ITimeSource& time);
```

Ablauf (`run(...)`), synthetisiert aus der bereits akzeptierten
`docs/RUN_PERSISTENCE.md`-"Wiederanlaufreihenfolge" und
`docs/STATE_MACHINE.md`-`BOOT`/`RECOVERY_EVALUATION` – hier nur
softwaretechnisch verdrahtet, fachlich nicht neu entworfen:

```text
1. persistence.loadAndInitialize() aufrufen.
2. Ladeergebnis klassifizieren (5.x-Statuswerte wiederverwendet):
   - NoPersistedRun/NoActiveRun -> kein Recovery-Bedarf.
   - Current mit variant == NoActiveRun -> kein Recovery-Bedarf.
   - Current mit aktivem Lauf -> weiter mit Schritt 3.
   - CapacityExceeded/ReadFailed/NotReconstructible(OrphanedState)/
     UnsupportedSchema/ForeignEpoch/FallbackRecovered/PreparedInterrupted
     -> "nicht rekonstruierbar" (6.10), keine Recovery- oder
     Aktorfreigabe, Rueckgabe eines typisierten Ergebnisses an den Aufrufer
     (Composition Root, ausserhalb #18).
3. Nur fuer einen geladenen aktiven Lauf:
   a. current.processState.state ist per Definition unterbrochen (jeder
      ueber Boot geladene aktive Lauf gilt als unterbrochen) ->
      RECOVERY_EVALUATION-Kontext.
   b. `plausibility` (aktuelle Sensor-/Plausibilitaetsevidenz) wird dem
      Coordinator vom Aufrufer als Parameter uebergeben, nicht selbst
      gelesen (6.9 – #18 liest keine Sensoren direkt).
   c. `recoveredState` gemaess 6.5 berechnen (inkl. WaitingForProduct-
      Sonderfall, ggf. `Uncertain` -> weiter bei Schritt 3e statt 3d).
   d. `decideProcessTransition(current.processState, {RecoveryResume,
      recoveredState=recoveredState}, snapshot, now)` aufrufen (bestehende
      Funktion, 5.3). Bei Ablehnung (`Rejected`): fuer WaitingForProduct
      -> `ProductWaitExpired`-Pfad (6.5); fuer jede andere Phase ->
      `RecoveryReject`-Ergebnis (bestehender `Fault`-Uebergang,
      `decideRecoveryEvent:745-750`).
   e. Bei `Uncertain` (nur WaitingForProduct erreichbar, 6.5): keine
      Transition, `DecisionRequired`-Meldung (6.6.2/RuntimeMessage),
      Rueckkehr mit "wartet auf Entscheidung"-Ergebnis, kein Aufruf von
      `activateLoadedRun` in diesem Zyklus.
   f. Bei erfolgreicher `RecoveryResume`-Entscheidung:
      `progressInit` gemaess 6.7 berechnen (Default `RunProgressState{}` bei
      Legacy-/fehlendem Wert, sonst der aus dem Snapshot restaurierte Wert
      mit `timeQuality` entsprechend 6.6.2/6.7 gesetzt) und
      `persistence.activateLoadedRun(current, plausibility, recoveryDecision,
      progressInit, time)` aufrufen (6.4).
   g. Nur bei `RunPersistenceResultStatus::Applied` ist der Lauf
      betriebsbereit; jedes andere Ergebnis bleibt fail-closed
      (`current` unveraendert, keine Freigabe).
4. Ergebnis (betriebsbereit / kein Recovery-Bedarf / nicht rekonstruierbar /
   wartet auf Entscheidung) wird typisiert zurueckgegeben.
```

### 6.9 Composition/DI-Vertrag (Befund 9)

`RunRecoveryCoordinator` haengt ausschliesslich von bereits injizierten
Referenzen (`RunPersistenceCoordinator&`, `device_platform::ITimeSource&`)
ab und ist vollstaendig nativ testbar mit
`device_platform_test_support::SimulatedPersistentStateStore` und
`device_platform::VirtualTimeSource`. **#18 verdrahtet diese Klasse nicht
in `src/main.cpp` oder `main/app_main.cpp`** und aendert
`FermentationApplication`/`IPlatformServices` nicht. Diese Verdrahtung setzt
einen produktiven `IStateStore`-Adapter voraus, der nicht existiert
(5.12) und dessen Bau explizit #29/#90 zugeordnet ist (Gate B). Der
Datei-/Commit-Schnitt (Abschnitt 8) enthaelt deshalb **keinen** Commit, der
`fermentation_application.cpp`, `src/main.cpp` oder `main/app_main.cpp`
aendert.

### 6.10 #24-Abgrenzung (Befund 10)

`RunRecoveryCoordinator` konsumiert ausschliesslich bereits vorhandene
Ergebniswerte (`RunPersistenceLoadStatus`, `RunPersistenceCoordinatorState::
BlockedIndeterminate`, beide bereits im Code definiert und von
`loadAndInitialize` bereits korrekt gesetzt) und erzeugt selbst **keine**
neue Fehlerklasse, keinen neuen persistenten Latch und keine
`SAFE_BOOT`-Logik. Jeder als "nicht rekonstruierbar" oder "unbestimmt"
klassifizierte Ladezustand fuehrt zu genau einem Ergebnis: keine Recovery,
keine Aktorfreigabe, typisierte Weitergabe an den zustaendigen
Safety-/Bootpfad (dessen konkrete Implementierung #24 ist und bleibt).

## 7. Modul- und Abhaengigkeitsgrenzen

Unveraendert aus Revision 1 (Abschnitt 7), praezisiert um 6.9: keine
Abhaengigkeit von `RunRecoveryCoordinator` auf `device_platform_esp_idf`
oder `device_platform_test_support` ausserhalb `test/`.
`python scripts/check_architecture_boundaries.py` bleibt gruener Guard.

## 8. Voraussichtlicher Datei- und Commit-Schnitt

### Commit 1 – `RunProgressState`, Schema-3-Migration, Laufinitialisierung/-abbau (6.2, 6.7)

- neu: `run_progress.hpp/.cpp` (Typ + reine Beobachtungs-Hilfsfunktionen)
- `run_persistence_contract.hpp/.cpp`: `RunProgressState`-Feld in
  `RunPersistenceSnapshot`, `kCurrentRunPersistenceSchema = 3U`,
  **`knownRunPersistenceSchema` um `3U` erweitert** (ohne diese Aenderung
  waere jeder neu geschriebene Schema-3-Bestand beim naechsten Boot ueber
  `run_persistence_coordinator.cpp:237-238` als `NotReconstructible`
  unlesbar – jeder Lesepfad gatet darauf),
  `RunPersistenceLoadResult::lastReliableUtcAnchorSeconds`
- `run_persistence_codec.cpp`: `kRunProgressFieldIntroducedInSchema = 3U`,
  ein zusaetzliches `if (schemaVersion >= ...)`-Gate im bestehenden
  `decodeRunPersistenceSnapshot` (5.10-Muster, keine neue Funktion), Legacy
  -> `NoReliableTime`
- `run_persistence_contract.hpp`: `RunPersistenceMutationKind::
  RecoveryActivation = 4U`, `RecoveryTimeCorrection = 5U`;
  `RunCheckpointTrigger::RecoveryActivation = 5U`, `RecoveryTimeCorrection
  = 6U`
- `run_commands.cpp`: `decideProgramStart`/`decideManualStart` setzen
  `decision.after.runProgress = RunProgressState{}`; `clearActiveRunState`
  setzt `state.runProgress = RunProgressState{}`
- Tests: `test/test_run_progress/`, Codec-/Migrationstests in
  `test/test_run_checkpoint_codec/` (vollstaendige Matrix, siehe 9.1)

### Commit 2 – Ausfallintervall: Zeitgrenzen und fachliche Bewertung getrennt (6.6)

- neu: `run_recovery_time.hpp/.cpp`
- Tests: `test/test_run_recovery_time/`

### Commit 3 – Kanonische Restart-Sensorentscheidung (6.3, Gate A)

- `sensor_selection.hpp/.cpp`: `computeRestartSensorSelection`-Signatur
  erweitert (Phasenrekonstruktion + Delegation), `evaluateAirFallbackActive`
  um Blocked->Allowed-Zweig ergaenzt
- `test_sensor_selection.cpp`: drei bestehende Tests umgestellt, neue Tests
  fuer differenziertes Ergebnis bei gueltiger Evidenz (siehe 9.3)

### Commit 4 – `RunPersistenceCoordinator::activateLoadedRun` (6.4)

- `run_persistence_coordinator.hpp/.cpp`: geteilter Zwei-Phasen-
  Schreibhelfer (Refactor von `writeSnapshot`, bestehende vier Aufrufer
  unveraendert im Verhalten), neue Methode `activateLoadedRun`
- Tests: `test/test_run_persistence_coordinator/` (neue Faelle, siehe 9.4)

### Commit 5 – `RunRecoveryCoordinator`: Boot-Klassifikation, Timer-Rebasing, Orchestrierung (6.5, 6.8)

- neu: `run_recovery_coordinator.hpp/.cpp`
- `process_state_machine.hpp/.cpp`: `TransitionRequest::
  recoveredWaitTimeDefinitelyExpired`, dritter Ausgang in
  `decideRecoveryEvent` fuer den `WaitingForProduct`-Sonderfall (6.5)
- Tests: `test/test_run_recovery_coordinator/` (Boot-Simulationen mit
  `VirtualTimeSource`/`SimulatedPersistentStateStore`, siehe 9.5),
  `test/test_process_state_machine/` (neuer Ausgang von `decideRecoveryEvent`)

### Commit 6 – Mehrdeutigkeit und spaetere Korrektur (6.6.2 `Uncertain`, 6.7 Episoden-Idempotenz)

- `run_persistence_coordinator.hpp/.cpp`: neue Methode
  `persistRecoveryTimeCorrection` (eigener, schmaler typisierter Pfad, nicht
  `persistTransition`, direkte Antwort auf Befund 3)
- `run_recovery_coordinator.cpp`: `DecisionRequired`-Meldungserzeugung bei
  `Uncertain`, periodischer Nachtragsversuch sobald `unixTimeSeconds()`
  verfuegbar wird
- Tests: `test/test_run_recovery_coordinator/`,
  `test/test_run_persistence_coordinator/`

### Commit 7 – Anzeige-/Exportdatenvertrag, Dokumentation, Guards

- Diagnose-/Exportfelder fuer `docs/RUN_PERSISTENCE.md` ("Anzeige und
  Export") als reine Datenprojektion
- `docs/RUN_PERSISTENCE.md`: Abschnitt "Uebergabe an ein spaeteres Vorhaben"
  wird **nicht** auf den Stub zurueckgestuft, sondern durch einen
  Abschlussvermerk ersetzt, der beschreibt, wie #18 die Uebergabe tatsaechlich
  erfuellt hat (Verweis auf `activateLoadedRun`/`computeRestartSensorSelection`)
- `docs/RECOVERY_AND_INTERRUPTION.md`, `docs/STATE_MACHINE.md`: Verweise auf
  "noch nicht implementiert" entfernen, sofern durch diesen Plan erledigt
- `python scripts/check_architecture_boundaries.py`,
  `python scripts/selftest_quality_gates.py`

Jeder Commit ist einzeln kompilier- und testbar. Commits 1-4 haben keine
Abhaengigkeit von Commit 5; Commit 5 haengt von 1-4 ab; Commit 6 haengt von
2, 4 und 5 ab.

## 9. Teststrategie und Testmatrix

### 9.1 Commit 1 – Progress, Migration, Lebenszyklus

- `RunProgressState`-Default, `accountObservedRunSeconds` (reine Funktion):
  Inkrement innerhalb desselben Boots korrekt, `lastAccountedMonotonicMillis`
  wird aktualisiert, `weightedProgressSeconds` bleibt in jedem Testfall
  `nullopt` (Regressionstest gegen versehentliche Befuellung).
- Laufinitialisierung setzt `runProgress` frisch; `clearActiveRunState`
  setzt es zurueck (Regressionstest analog zu bestehenden
  `sensorSelectionRuntime`-Tests).
- `knownRunPersistenceSchema(3U) == true`, `knownRunPersistenceSchema(4U) ==
  false` (direkter Test der in Abschnitt 8/Commit 1 ergaenzten Zeile).
- **Vollstaendige Schema-Matrix** (Befund 8, nicht nur 2->3):
  - Schema 1 laden (kein `sensorSelection`, kein `runProgress`) ->
    `LegacyUnknown`/`NoReliableTime`, **nicht** "0 Sekunden = bereits
    bewerteter Fortschritt".
  - Schema 2 laden (kein `runProgress`) -> `NoReliableTime`.
  - Schema 3 schreiben/lesen (Round-Trip).
  - gemischte `current`/`fallback`: 3/2, 3/1 (Erweiterung von
    `test_committed_head_accepts_mixed_current_and_fallback_schema`).
  - korrupter neuer `current` -> gueltiger aelterer Fallback (Schema 2 oder
    1) bleibt korrekt ladbar.
  - Prepared-Head-Unterbrechung waehrend eines Schema-3-Schreibvorgangs
    (Erweiterung von `test_restart_after_prepared_or_slot_cut_is_interrupted`).
  - unbekannte Schemaversion (`4`) wird abgelehnt (Erweiterung von
    `test_head_reference_accepts_known_schemas_and_rejects_unknown_ones`).
  - Legacy-Sensorprovenienz bleibt bei Schema-3-Schreibvorgang auf einem
    zuvor Schema-1-geladenen Bestand korrekt `LegacyUnknown` (kein
    versehentliches Umschreiben beim reinen Progress-Feld-Hinzufuegen).

### 9.2 Commit 2 – Zeitgrenzen und Bewertung

- `computeRecoveryTimeBounds`: fehlender Anker -> `hasReliableAnchor=false`;
  rueckwaerts laufende UTC -> `hasReliableAnchor=false`, keine negativen
  Grenzen; Ueberlaufwerte (nahe `INT64_MAX`/`UINT64_MAX`) -> definiert
  behandelt, kein Wrap; Kontrollpunkt-Intervall-Grenzwerte (1/5/60 Minuten).
- `evaluateRecoveryTimeDecision`: uebereinstimmende **ungleiche** Grenzen
  (z. B. untere Grenze 10 Min., obere Grenze 40 Min., Schwelle 5 Min. ->
  beide klar ueberschritten -> `DefinitelyExceedsBound` trotz Ungleichheit);
  widerspruechliche Grenzen -> `Uncertain`; `hasReliableAnchor=false` ->
  immer `Uncertain`.

### 9.3 Commit 3 – Restart-Sensorentscheidung

- Phasenrekonstruktion: `FallbackActive` -> `AirFallbackActive`;
  `InitialSelection`/`ReturnedToProduct`/`LegacyUnknown` x
  `{Product, Air}` -> `NormalProduct`/`NormalAir` (sechs Faelle).
- Delegation liefert bei ungueltiger Evidenz weiterhin `Blocked` (Ersatz der
  drei alten Fail-Closed-Tests, jetzt mit explizit ungueltiger Evidenz statt
  implizit ignorierter Eingabe).
- Delegation liefert bei gueltiger Evidenz `Allowed` mit
  `RecoveryRevalidation` fuer alle drei erreichbaren Zielphasen
  (`NormalProduct`, `NormalAir`, `AirFallbackActive` – letzteres deckt die
  neue Erweiterung ab).
- `evaluateAirFallbackActive` Regressionstest: bestehendes Verhalten fuer
  `permission == Allowed` (Normalfall) bleibt durch die Erweiterung
  unveraendert.

### 9.4 Commit 4 – `activateLoadedRun`

- Erfolgreiche Aktivierung: `current` erst nach Commit mutiert (RAM-Snapshot
  vor dem Aufruf mit dem Zustand nach fehlgeschlagenem Commit vergleichen).
- `WriteFailed`/`CapacityExceeded`: `state_` faellt auf `LoadedActiveRun`
  zurueck (nicht `Ready`), `current` unveraendert, keine Aktorfreigabe
  moeglich (struktureller Test: kein weiterer Aufruf von
  `persistCommand`/`persistTransition` ist danach erfolgreich, da `state_`
  weiterhin `LoadedActiveRun` ist).
- `CommitOutcomeUnknown`/indeterminate: `state_ = BlockedIndeterminate`
  (bestehendes Muster wiederverwendet).
- Eine einzige Revision enthaelt Sensorselektion + Prozess-Recovery +
  Progress-Init (Test liest die geschriebene Revision und prueft alle drei
  Anteile in dem einen `RunPersistenceRawRecord`).
- `runRevision` wird genau einmal erhoeht; `sensorSelection.
  lastDecisionRunRevision` und die `resultingRunRevision` in einem
  zurueckgegebenen `SensorSelectionEvent`/`SensorSelectionNotice` stimmen
  exakt mit dem committeten `runRevision` ueberein (Test gegen die in 6.4
  beschriebene Restamping-Regel).
- Reboot direkt nach erfolgreicher Aktivierung: zweiter
  `loadAndInitialize()`-Lauf liefert wieder `LoadedActiveRun` mit
  `sensorSelectionRuntime = RestartRevalidationPending`/`Blocked` (Beleg der
  Idempotenzargumentation aus 6.4).
- Vorhandene vier Bestandsmethoden bleiben unveraendert getestet (keine
  Regressionen aus dem Schreibhelfer-Refactor) – bestehende Testsuiten fuer
  `persistCommand`/`persistTransition`/`persistSensorSelection`/
  `checkpointPeriodic` laufen unveraendert gruen.

### 9.5 Commit 5 – `RunRecoveryCoordinator`

- Fuer jede der acht `validRecoveryTarget`-Phasen: kurze (sicher innerhalb),
  mittlere und lange (sicher ausserhalb, wo zutreffend) Unterbrechung –
  deckt die im Issue geforderte Matrix ab.
- `WaitingForProduct`: `DefinitelyWithinBound` -> `RecoveryResume` mit
  `recoveredState.stateEnteredAtMillis == now` (kein Underflow, siehe 6.5);
  `DefinitelyExceedsBound` -> `decideRecoveryEvent` mit
  `recoveredWaitTimeDefinitelyExpired = true` liefert `Standby`/
  `ProductWaitExpired`, **nicht** `Fault` (Test gegen den neuen dritten
  Ausgang aus 6.5/Commit 5, inklusive Regressionstest, dass der
  unveraenderte `RecoveryReject`-Pfad ohne dieses Flag weiterhin `Fault`
  liefert); `Uncertain` -> `DecisionRequired`, kein automatischer Uebergang.
  Zusaetzlich: bei `DefinitelyWithinBound` mit sehr kleinem `now` (frischer
  Boot, Millisekundenbereich) und grossem `priorElapsedSeconds` (Stunden)
  laeuft der Aufruf fehlerfrei durch (Regressionstest exakt gegen den in
  6.5 beschriebenen, urspruenglich uebersehenen Overflow-Fall).
- Aktoren bleiben bis zum ersten erfolgreichen `activateLoadedRun`-Commit
  nachweislich ohne Freigabepfad (kein Codepfad liefert vorher
  `Applied`/`Ready`).
- Nicht rekonstruierbarer/unbestimmter Ladezustand (6.10): Coordinator
  liefert das typisierte "keine Recovery"-Ergebnis, ruft `activateLoadedRun`
  nicht auf.
- Alle Prozess-Timer nach echtem Reboot mit zurueckgesetzter Monotonic-Zeit
  (`VirtualTimeSource` auf 0 zurueckgesetzt zwischen zwei
  `loadAndInitialize()`-Aufrufen): `recoveredState`-Zeitstempel liegen nie
  in der Zukunft relativ zur neuen Zeitbasis (Regressionstest gegen den in
  5.3/6.5 beschriebenen Fehlerpfad).

### 9.6 Commit 6 – Mehrdeutigkeit und Nachkorrektur

- `Uncertain` erzeugt genau eine aktive `DecisionRequired`-Meldung.
- `unixTimeSeconds()` wechselt zwischen zwei `update()`-Zyklen von
  `nullopt` zu einem Wert -> genau eine `persistRecoveryTimeCorrection`-
  Anwendung, `timeQuality -> Evaluated`.
- Wiederholte `update()`-Zyklen nach `Evaluated` wenden die Korrektur
  **nicht** erneut an (Episoden-Idempotenz-Test).
- Eine neue Unterbrechung nach einer bereits `Evaluated`-Episode oeffnet
  eine neue Episode mit frischem `RecoveryTimePending` (kein Uebertrag der
  alten Episode).
- Intervall ueber Fermentations-, Produktwarte- und Haltegrenzen je einmal
  mit `evaluateRecoveryTimeDecision` gegen die jeweils passende Schwelle.
- Ausfallzeit-Gutschrift (6.7): zuverlaessiger Anker mit enger Bandbreite ->
  Gutschrift der unteren Grenze; zuverlaessiger Anker mit weiter Bandbreite
  -> keine Gutschrift, auch wenn `evaluateRecoveryTimeDecision`
  `DefinitelyWithinBound` liefert (Regressionstest gegen die in Befund 4
  korrigierte Verwechslung von Grenzentscheidung und Datenqualitaet).

### 9.7 Gezielte Ausfuehrung nach Freigabe

```bash
pio test -e native --filter test_run_progress
pio test -e native --filter test_run_recovery_time
pio test -e native --filter test_sensor_selection
pio test -e native --filter test_run_persistence_coordinator
pio test -e native --filter test_run_recovery_coordinator
pio test -e native --filter test_run_checkpoint_codec
pio test -e native --filter test_run_commands
pio test -e native --filter test_process_state_machine
python scripts/check_architecture_boundaries.py
python scripts/check_secrets.py
clang-format --dry-run --Werror <geaenderte Dateien>
git diff --check
```

Ein vollstaendiger Lauf (`pio run -e native`, `pio test -e native` ohne
Filter, `scripts/selftest_quality_gates.py`,
`scripts/check_ci_artifact_scan_coverage.py`) erfolgt ausschliesslich nach
abgeschlossenem Review und auf ausdrueckliche Owner-Anweisung
(`docs/CI_AND_QUALITY_GATES.md`).

## 10. Safety-, Security-, Recovery- und Hardwaregrenzen

- Jeder Wiederanlauf beginnt weiterhin mit ausgeschalteten Aktoren
  (unveraendert, ADR-014, `docs/STATE_MACHINE.md` `BOOT`) – dieser Plan
  fuegt keinen Pfad hinzu, der das umgeht.
- Die Reglerfreigabe fuer einen reaktivierten Lauf ist strukturell an genau
  einen erfolgreichen `activateLoadedRun`-Commit gebunden (6.4); RAM wird
  nachweislich nicht vor diesem Commit mutiert (9.4).
- Kein automatischer Laufabschluss oder Fortschrittsgutschrift bei
  `Uncertain` (6.6.2, 6.7, 9.6) – deckt "unsichere Situationen werden nicht
  geraten" direkt ab.
- Fehlende NTP-Zeit beendet keinen Lauf: `hasReliableAnchor=false` fuehrt zu
  `Uncertain` nur fuer die grenzwertige `WaitingForProduct`-Entscheidung;
  alle anderen Phasen werden ueber `RecoveryResume` unveraendert freigegeben,
  sofern eindeutig zulaessig (6.5-Tabelle, keine Abhaengigkeit von UTC fuer
  die reine Phasen-Wiederaufnahme).
- Keine Aenderung an `SAFE_BOOT`-, Bootschleifen- oder Latch-
  Verriegelungslogik (6.10); #18 konsumiert deren Ergebnis nur vor Schritt 3
  des Coordinators.
- Kein Zugriff auf ESP-IDF-, GPIO- oder Hardwaredetails; alles bleibt gegen
  `ITimeSource` und `RunPersistenceCoordinator` (der gegen `IStateStore`
  arbeitet) abstrahiert; keine Verdrahtung in einen Composition Root (6.9).

## 11. Ressourcen- und Betriebsbudget

- Schema-Bump 2 -> 3 fuegt `RunProgressState` hinzu:
  `observedRunSeconds` (`uint32_t`), `observedSource` (`uint8_t`-Enum),
  `timeQuality` (`uint8_t`-Enum), `weightedProgressSeconds`
  (`optional<uint32_t>`) – ca. 10-14 Byte vor Kodierungsoverhead, deutlich
  kleiner als der in Revision 1 geplante Umfang (kein
  `lastReliableUtcAnchorSeconds`/`monotonicMillisAtLastReliableAnchor` mehr
  im Payload, da bereits im Envelope vorhanden, 5.11).
  `kMaximumRunPersistencePayloadBytes = 8192U` bleibt unangetastet
  ausreichend; Commit 1 sichert das mit einem Test gegen die tatsaechliche
  kodierte Groesse ab.
- Zwei neue `RunPersistenceMutationKind`/`RunCheckpointTrigger`-Werte sind
  reine Enum-Erweiterungen ohne Speicherwirkung.
- Kein zusaetzlicher periodischer Schreibzyklus; `persistRecoveryTimeCorrection`
  (Commit 6) wird hoechstens einmal pro Recovery-Episode aufgerufen.
- Keine neue dynamische Allokation.

## 12. SOLID-, DRY- und KISS-Bewertung des geplanten Diffs

- **SRP:** `run_recovery_time.hpp` (reine Zeitgrenzen), `run_progress.hpp`
  (Beobachtungszustand), `sensor_selection.cpp`-Erweiterung
  (Phasenrekonstruktion + Delegation), `activateLoadedRun`
  (Persistenzuebergang) und `RunRecoveryCoordinator` (Orchestrierung) sind
  fuenf disjunkte, einzeln testbare Verantwortlichkeiten statt einer
  monolithischen Recovery-Klasse.
- **DRY:** Kein zweiter Sensorregelkern (6.3 delegiert vollstaendig an
  `applySensorSelectionDecision`); kein zweiter Zwei-Phasen-Schreibpfad
  (6.4 teilt den bestehenden Helfer mit den vier Bestandsmethoden); kein
  zweites Persistenzformat fuer den UTC-Anker (5.11, bereits vorhandener
  Envelope-Wert wird nur sichtbar gemacht statt dupliziert); die
  6.5-Zeitstempeltabelle nutzt die bereits bestehende, getestete
  `decideRecoveryEvent` statt einer neuen Phasentabelle.
- **KISS:** kein generisches `PhaseDurationState`-Objekt (verworfen
  zugunsten der zwei klar getrennten, minimalen Mechanismen A/B in 6.5);
  `weightedProgressSeconds` bleibt strukturell leer statt eines
  unbenutzten Berechnungsgeruests fuer ein nicht vorhandenes Modell.
- **OCP/DIP:** `RunRecoveryCoordinator` haengt nur gegen bereits
  bestehende abstrakte Schnittstellen (`ITimeSource`,
  `RunPersistenceCoordinator`) ab; keine neue Abhaengigkeit von einer
  konkreten Plattform, keine Erweiterung von `IPlatformServices`.

## 13. Offene Punkte (Implementierungsdetails, keine Owner-Gates)

Diese Punkte sind bei der Umsetzung zu klaeren, aendern aber den Vertrag
dieses Plans nicht materiell und erfordern keine erneute Ownerfreigabe,
sofern sie innerhalb der hier beschriebenen Grenzen geloest werden:

- Exakte Konstanten fuer den "maximal moeglichen Kontrollpunktabstand" in
  `computeRecoveryTimeBounds` (abgeleitet aus
  `configuredCheckpointIntervalMinutes`, bereits firmwarefest begrenzt
  1-60 Minuten, `docs/RUN_PERSISTENCE.md`).
- Exakte Feldbreite/Kodierung von `RunProgressState` im Codec (Big-/Little-
  Endian-Konvention folgt dem bestehenden `be::`-Namensraum-Muster aus
  `run_persistence_codec.cpp`, keine neue Konvention).
- Ob `persistRecoveryTimeCorrection` (Commit 6) zusaetzlich einen
  `runRevision`-Erwartungswert zur Stale-Erkennung entgegennimmt (analog
  zu `persistCommand`s `decision.expected`-Muster) – funktional notwendig,
  aber keine Vertragsentscheidung, die der Plan vorab treffen muss.

## 14. Dokumentations- und Abschlussnachweise

- `docs/ROADMAP.md`, Issue #18: bereits vor Revision 1 synchronisiert,
  keine weitere Aenderung fuer Revision 2 noetig.
- `docs/RUN_PERSISTENCE.md`, `docs/RECOVERY_AND_INTERRUPTION.md`,
  `docs/STATE_MACHINE.md`: Aktualisierung in Commit 7, nach tatsaechlicher
  Umsetzung – der #21-Uebergabeabschnitt wird als **erfuellt**
  dokumentiert, nicht durch eine Beschreibung des Stub-Verhaltens ersetzt
  (direkte Antwort auf Gate A).
- PR-Beschreibung wird auf Planpfad, exakte neue Plan-Commit-SHA und "keine
  offenen Gates" aktualisiert (Abschnitt 3).

## 15. Verbindliche Taskliste fuer die Umsetzung

1. [ ] Commit 1: `RunProgressState`, Schema-3-Migration (vollstaendige
   Matrix), Lebenszyklus-Anschluss + Tests.
2. [ ] Commit 2: `run_recovery_time` (Zeitgrenzen + fachliche Bewertung
   getrennt) + Tests.
3. [ ] Commit 3: kanonische Restart-Sensorentscheidung + `evaluateAirFallbackActive`-
   Erweiterung + Tests (inkl. Umstellung der drei #21-Tests).
4. [ ] Commit 4: geteilter Schreibhelfer + `activateLoadedRun` + Tests
   (inkl. Regressionstests fuer die vier Bestandsmethoden).
5. [ ] Commit 5: `RunRecoveryCoordinator` (Klassifikation, Timer-Rebasing,
   Orchestrierung) + Tests.
6. [ ] Commit 6: `persistRecoveryTimeCorrection`, `Uncertain`-Meldung,
   Episoden-Idempotenz + Tests.
7. [ ] Commit 7: Anzeige-/Exportfelder, Dokumentationsabschluss (ohne
   Ruecksstufung auf Stub), Guards.
8. [ ] Gezielte Tests je Commit (9.7-Befehle, jeweils passender Filter).
9. [ ] Vollstaendiges Review des Gesamtdiffs (Agent-Workflow Abschnitt 7).
10. [ ] Vollstaendiger lokaler Lauf nur nach Review und auf Owner-Anweisung.
11. [ ] `SESSION HANDOVER` bei Sessionende mit offenem PR.

## 16. Stopbedingung

Nach Committen dieser Revision wird angehalten. Keine Implementierung, kein
`Ready for review`, keine Remote-CI, kein Merge, bis der Owner den exakten
neuen Plan-Commit freigibt.
