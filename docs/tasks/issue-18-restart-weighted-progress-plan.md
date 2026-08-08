# Plan: Issue #18 – Wiederanlauf und temperaturgewichteter Fortschritt

## 1. Status

- Revision: **5** (ersetzt Revision 1-4 vollstaendig; kein Abschnitt dieser
  Datei verweist auf eine fruehere Revision als weiterhin gueltig).
- Draft-PR: #102 (`plan/issue-18-restart-weighted-progress` -> `main`).
- Live-Issue: #18.
- Plan-Basis: `main` = `17ab3f5399a066465298ac6871b965d176a38d32`. Branch ist
  0 Commits hinter `main`.
- Nach Commit dieser Revision: `git push`, danach frischer `git fetch` und
  Abgleich `git rev-parse origin/plan/issue-18-restart-weighted-progress`
  == lokaler `HEAD` sowie `gh api repos/ManuEngineer/ESP32-Fermentationsschrank/pulls/102`
  (`head.sha`) und `gh pr view 102 --json headRefOid`, bevor PR-Beschreibung/
  SESSION HANDOVER als aktuell gemeldet werden.
- Dieser Plan implementiert noch nichts.

## 2. Live-Status-Pruefung

- `gh issue view 18`: weiterhin offen, Scope/Akzeptanzkriterien unveraendert.
- `gh pr view 102`: Draft, Base `main`.
- Alle Codezeilenangaben in dieser Revision wurden in dieser Session erneut
  direkt am Dateiinhalt verifiziert.

## 3. Owner-Entscheidungen (Gates)

- **Gate A:** Restart-Sensorauswahl wird real angewandt (5.20).
- **Gate B:** kein neuer allgemeiner Prozesszyklus; kein produktiver
  Aufrufer wird behauptet, der nicht existiert (5.21).
- **Gate C:** keine unkalibrierte biologische Aktivitaetskurve; unsichere
  Ausfallzeit wird ohne freigegebenes Modell nicht automatisch als
  Fortschritt gutgeschrieben (5.16, 5.17).

## 4. Ziel und Nicht-Ziele

**Ziel:** Nach einem Neustart wird ein geladener aktiver Lauf sicher
bewertet, korrekt fortgesetzt oder korrekt beendet, mit nachvollziehbarem,
ehrlich gekennzeichnetem Fortschritt und ohne jede Aktorfreigabe vor
abgeschlossener Bewertung.

**Nicht-Ziele (Release 1):** reale Hardware-/NVS-Anbindung (#29/#90);
Fault-Klassen/SAFE_BOOT-Feinausbau (#24); Web-/Anzeige-UI-Implementierung;
kalibrierte biologische Temperatur-Aktivitaets-Gewichtung (Gate C); ein
neuer periodischer Anwendungszyklus (Gate B); produktive Verdrahtung eines
automatischen UTC-Reevaluationsaufrufers (Gate B).

## 5. Bindende Fachvertraege

### 5.1 Modul- und Dateizuordnung

| Bereich | Datei(en) | Aenderungsart |
|---|---|---|
| Ausfallintervall (rein) | `lib/fermentation_app/src/run_recovery_time.hpp/.cpp` | neu |
| Recovery-Orchestrierung | `lib/fermentation_app/src/run_recovery.hpp/.cpp` | neu |
| Zustandsautomat: neue Reasons, Topologie, `PriorBootPhaseElapsed`-Parameter | `lib/fermentation_app/src/process_state_machine.hpp/.cpp` | erweitert |
| Persistenzvertrag: Schema 3 | `lib/fermentation_app/src/run_persistence_contract.hpp/.cpp` | erweitert |
| Codec: Schema-3-Gate, Legacy-Migration | `lib/fermentation_app/src/run_persistence_codec.cpp` | erweitert |
| Coordinator: Low-Level-Schreibkern, `activateLoadedRun`, `activateFallbackRecoveredRun`, `resolveRecoveryOutcome`, Episode-Refresh | `lib/fermentation_app/src/run_persistence_coordinator.hpp/.cpp` | erweitert |
| Kommandos: `ResolveRecoveryUncertainty`, `ApplyRecoveryTimeCorrection` | `lib/fermentation_app/src/run_commands.hpp/.cpp` | erweitert |
| Restart-Sensorauswahl (Gate A) | `lib/fermentation_app/src/sensor_selection.hpp/.cpp` | erweitert |
| Tests | `test/test_run_recovery_time/`, `test/test_run_recovery/`, `test/test_process_state_machine/`, `test/test_run_persistence_coordinator/`, `test/test_run_commands/`, `test/test_sensor_selection/` | neu/erweitert |

### 5.2 `RecoveryOutageBounds` – kanonische Ausfalldauer (Vertrag A)

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 4):**
`docs/RUN_PERSISTENCE.md:131-136` legt die Ausfalldauer verbindlich fest als

```text
obere Grenze = aktuelle UTC - letzter verlaesslicher UTC-Kontrollpunkt
untere Grenze = max(0, obere Grenze - maximal moeglicher Kontrollpunktabstand)
```

**auch mit exaktem UTC-Anker auf beiden Seiten bleibt die Ausfalldauer ein
Intervall**, weil das Geraet nach dem letzten Kontrollpunkt noch bis zum
naechsten planmaessigen Speicherzeitpunkt weitergelaufen sein kann
(`docs/RUN_PERSISTENCE.md:119-121`). Revision 4 behandelte eine exakte
UTC-Differenz faelschlich als exakte Ausfalldauer und vermischte sie mit der
Alt-Boot-internen Phasenlaufzeit in einem einzigen Objekt.

```cpp
// run_recovery_time.hpp
struct RecoveryOutageBoundsInput {
    std::optional<std::int64_t> utcAtLastCheckpoint;  // RunPersistenceRawRecord.utcUnixSeconds
    std::optional<std::int64_t> utcNowAfterRestart;   // aktuelle Zeitquelle, einmalig zum Recoveryzeitpunkt abgefragt
    std::uint32_t maxCheckpointGapSeconds;             // aus konfiguriertem Intervall (intervalMinutes) + firmwarefester Toleranz
    RunCheckpointTrigger lastCheckpointTrigger;        // fuer Konfidenzanzeige, keine Grenzenrolle
};

struct RecoveryOutageBounds {
    std::uint64_t outageSecondsUpperBound;  // = utcNowAfterRestart - utcAtLastCheckpoint
    std::uint64_t outageSecondsLowerBound;  // = max(0, upperBound - maxCheckpointGapSeconds)
};

// nullopt genau dann, wenn ein UTC-Anker fehlt oder utcNow < utcAtCheckpoint
// (Uhr ging zurueck – als unbekannt behandelt, nicht als negative Dauer).
// Keine erfundene Ausfalldauer ohne beide Anker.
[[nodiscard]] std::optional<RecoveryOutageBounds> computeRecoveryOutageBounds(
    const RecoveryOutageBoundsInput& input);
```

Fehlt ein UTC-Anker: `computeRecoveryOutageBounds` liefert `nullopt` – die
Ausfalldauer ist dann vollstaendig unbekannt (kein Fallback auf eine
geratene Zahl).

### 5.3 `RecoveredPhaseElapsed` – Phasenlaufzeit, strukturell getrennt (Vertrag B)

```cpp
struct RecoveredPhaseElapsedInput {
    std::uint64_t oldStateEnteredAtMillis;       // restaurierter processState, alter Boot
    std::uint64_t oldCheckpointMonotonicMillis;  // aus Snapshot, alter Boot, >= oldStateEnteredAtMillis
    std::optional<RecoveryOutageBounds> outage;  // aus 5.2, kann nullopt sein
};

struct RecoveredPhaseElapsed {
    std::uint64_t knownSecondsBeforeCheckpoint;       // = (oldCheckpointMonotonicMillis - oldStateEnteredAtMillis) / 1000, immer bekannt, rein Alt-Boot-intern
    std::uint64_t totalSecondsLowerBound;             // = knownSecondsBeforeCheckpoint + (outage ? outage->outageSecondsLowerBound : 0)
    std::optional<std::uint64_t> totalSecondsUpperBound; // = knownSecondsBeforeCheckpoint + outage->outageSecondsUpperBound, nur wenn outage vorhanden
};

[[nodiscard]] RecoveredPhaseElapsed computeRecoveredPhaseElapsed(
    const RecoveredPhaseElapsedInput& input);
```

Keine Variable bedeutet gleichzeitig "Ausfalldauer" und "gesamte
Phasenlaufzeit": `RecoveryOutageBounds` beschreibt ausschliesslich die
Unterbrechung selbst (Vertrag A, kanonisch nach `RUN_PERSISTENCE.md`);
`RecoveredPhaseElapsed` beschreibt die daraus abgeleitete **gesamte**
Zeit seit Phaseneintritt (Vertrag B, `Alt-Boot-Anteil + Ausfallintervall`),
verwendet fuer die fachliche Grenzbewertung (5.4).

### 5.4 `evaluateRecoveryTimeVerdict` – unveraendert als reine Vergleichsfunktion

```cpp
enum class RecoveryTimeVerdict : std::uint8_t {
    DefinitelyStillValid, DefinitelyExpired, Uncertain,
};

[[nodiscard]] RecoveryTimeVerdict evaluateRecoveryTimeVerdict(
    const RecoveredPhaseElapsed& elapsed, std::uint32_t limitSeconds) {
    if (elapsed.totalSecondsLowerBound >= limitSeconds) {
        return RecoveryTimeVerdict::DefinitelyExpired;
    }
    if (elapsed.totalSecondsUpperBound.has_value() &&
        *elapsed.totalSecondsUpperBound < limitSeconds) {
        return RecoveryTimeVerdict::DefinitelyStillValid;
    }
    return RecoveryTimeVerdict::Uncertain;
}
```

Anwendung je Phase (Tabelle unveraendert aus Revision 4, jetzt auf korrekt
komponierten `RecoveredPhaseElapsed`-Bounds statt eines vermischten
Objekts): `WaitingForProduct` (`maximumProductWaitMinutes*60`, steuert
Hop-2-Ausgang), `Fermenting` (`fermentationDurationMinutes*60`, rein
informativ), `CoolHolding` mit `CompletionMode::CoolAndHoldForDuration`
(`holdDurationMinutes*60`, rein informativ); alle anderen recovery-faehigen
Phasen ohne snapshot-getragene Grenze.

### 5.5 Boot-unabhaengiger Phasenfortschritt – kein Rebasing-Unterlauf

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 4):**
`newBootMonotonicMillisNow - elapsedSeconds * 1000` unterlaeuft (unsigned),
wenn bereits mehr Phasenzeit bekannt ist, als der aktuelle Boot ueberhaupt
laeuft (z. B. zwei Stunden bekannte Fermenting-Zeit, Boot laeuft erst fuenf
Sekunden) – das Ergebnis waere ein riesiger Wert, den
`runtimeTimeIsValid()` als "in der Zukunft" ablehnt. Dieser Fehler wurde in
einer frueheren Revisionsrunde bereits benannt (kein Boot-uebergreifender
Rohvergleich monotoner Werte) und ist mit der Rebasing-Subtraktion in
Revision 4 unbeabsichtigt zurueckgekehrt.

**Vertrag:** `stateEnteredAtMillis` wird bei Recovery **immer** auf den
aktuellen Boot gesetzt (`= monotonicMillis`, niemals zurueckgerechnet).
Der bereits bekannte Vor-Boot-Anteil wird **separat**, boot-unabhaengig,
additiv gefuehrt und von den Dauerentscheidungsfunktionen explizit
hinzugerechnet – niemals durch Subtraktion von `now`:

```cpp
// process_state_machine.hpp
struct PriorBootPhaseElapsed {
    std::uint32_t lowerBoundSeconds{0U};
    std::optional<std::uint32_t> upperBoundSeconds;
};

bool elapsedWithPrior(std::uint64_t now, std::uint64_t startedAt,
                      std::uint32_t durationMinutes,
                      std::uint32_t priorSeconds) {
    // now >= startedAt ist innerhalb desselben Boots durch propose()/
    // Hop-1-Konstruktion garantiert (5.7) - keine Unterlaufgefahr, da hier
    // ausschliesslich addiert, nie von now subtrahiert wird.
    return (now - startedAt) / 1000U + priorSeconds >=
           static_cast<std::uint64_t>(durationMinutes) * 60U;
}
```

`decideWaitingForProduct`, `decideFermenting`, `decideCoolHolding`
(`process_state_machine.cpp:537-544,596-604,624-637`) erhalten je einen
neuen Parameter `PriorBootPhaseElapsed priorElapsed = {}` (Default = kein
Vor-Boot-Anteil, bestehendes Verhalten fuer alle Nicht-Recovery-Aufrufe
unveraendert) und verwenden `elapsedWithPrior` statt `elapsedOptional` fuer
ihre jeweilige Grenze. Der bestehende Vorab-Check in
`decideProcessTransition` (`process_state_machine.cpp:993-996`,
`WaitingForProduct`) sowie der eingebaute Check in `decideRecoveryEvent`
(`process_state_machine.cpp:762-764`) werden ebenso auf `elapsedWithPrior`
umgestellt. `decideProcessTransition`, `decideAutomatic` und
`decideExplicitEvent` erhalten dazu einen zusaetzlichen Parameter
`const PriorBootPhaseElapsed& priorElapsed = {}` (Default erhaelt alle
bestehenden Aufrufstellen unveraendert lauffaehig) und reichen ihn an die
drei Funktionen durch.

**Herkunft des Werts:** `priorElapsed` wird **nicht** Teil von
`ProcessRuntimeState` (das bliebe damit phasen-generisch und unveraendert
in seinen bestehenden Invarianten/Vergleichsfunktionen), sondern lebt als
`std::optional<TaggedPriorBootPhaseElapsed>` (mit Phasen-Tag) auf Ebene von
`RunCommandState`/`RunPersistenceSnapshot` (5.10). Der Aufrufer (Recovery-
Orchestrierung bzw. der automatische Live-Dispatch) liest ihn nur, wenn
`current.processState.state` mit dem Tag uebereinstimmt – sonst wird `{}`
(kein Vor-Boot-Anteil) verwendet. Keine Boot-Epochen-Fiktion; keine
Subtraktion von `now`.

### 5.6 Vollstaendige phasenbezogene Timertabelle fuer Hop 2

| Phase | `stateEnteredAtMillis` | `targetReachStartedAtMillis` | `targetReachWarningIssued` | `qualificationValidSinceMillis` | boot-unabhaengiger Anteil |
|---|---|---|---|---|---|
| `Preheating` | `now` | `0` (kein Timer) | `false` | `nullopt` (bestehende Logik, `process_state_machine.cpp:771-772`) | keiner |
| `WaitingForProduct` | `now` | `0` | `false` | n/a | `priorElapsed` (Ober- oder Untergrenze, siehe 5.8) |
| `ReachingTarget` | `now` | `now` (Timer bewusst neu gestartet – konservativ, verzoegert hoechstens eine Warnung) | `false` | n/a | keiner |
| `QualifyingTarget` | wird von `decideRecoveryEvent` selbst zu `ReachingTarget` umgeleitet, `stateEnteredAtMillis = now` (bestehende Logik, `process_state_machine.cpp:773-777`) | `now` | `false` | `nullopt` | keiner – beginnt vollstaendig neu |
| `Fermenting` | `now` | `0` | `false` | n/a | `priorElapsed` (Untergrenze) + separat `observedRunSeconds` (5.16, Geschaeftsmetrik, getrennt von der reinen Grenzbewertung) |
| `Cooling` | `now` | `0` | `false` | n/a | keiner (kein Dauer-Timer, signalbasiert) |
| `CoolHolding` | `now` | `0` | `false` | n/a | `priorElapsed` (Untergrenze) |
| `ManualHolding` | `now` | `0` | `false` | n/a | keiner (kein automatisches Limit) |

Kein Feld wird roh aus dem alten Boot uebernommen; jedes Feld ist entweder
explizit auf `now`/`0`/`nullopt` gesetzt oder ueber `priorElapsed` separat
gefuehrt.

### 5.7 Hop 1 – Recovery-Eintrittsvertrag (unveraendert aus Revision 4, weiterhin verifiziert korrekt)

`TransitionReason::RecoveryReentryRequired`, `validControlTopology`-Zweig
`stateUsesRunSnapshot(from) && to == RecoveryEvaluation`
(`stateUsesRunSnapshot`, `process_state_machine.cpp:76-97`: `Preheating`,
`WaitingForProduct`, `ReachingTarget`, `QualifyingTarget`, `Fermenting`,
`Cooling`, `CoolHolding`, `ManualHolding` – **nicht** `Completed`, dazu
5.18). Ablauf: `candidate = restoredState`; `originalRestoredProcessState`
als unveraenderte Kopie fuer den spaeteren Recovery-Kontext (5.10);
`hop1 = propose(candidate.processState, RecoveryEvaluation,
RecoveryReentryRequired, monotonicMillis)`; `applyProcessTransition(candidate.processState,
hop1, &*candidate.processRunSnapshot)`. `ProcessSignals::criticalFault =
false`. Bei Fehlschlag: kein Schreiben, Coordinator bleibt im
Ausgangszustand, `InvalidDecision`.

### 5.8 Hop 2 – Wiederverwendung der bestehenden `decideRecoveryEvent`-API

`request.recoveredState` wird aus `originalRestoredProcessState` gemaess
5.6 aufgebaut (alle Felder explizit gesetzt, kein roher Altwert). Aufruf:

```cpp
TransitionRequest request;
request.event = ProcessEvent::RecoveryResume;  // oder RecoveryReject, siehe 5.20
request.recoveredState = rebasedRecoveredState;
const auto hop2 = decideProcessTransition(
    candidate.processState /* == RecoveryEvaluation */,
    &*candidate.processRunSnapshot, ProcessSignals{/* criticalFault=false */},
    request, monotonicMillis, priorElapsedForOldPhase /* 5.5 */);
```

Rebasing-Richtung fuer `priorElapsedForOldPhase`, das an `decideRecoveryEvent`s
eingebauten `WaitingForProduct`-Check (`elapsedWithPrior` statt
`elapsedOptional`, 5.5) sowie an die Snapshot-Struktur selbst
weitergereicht wird:

- **Alle Phasen ausser `WaitingForProduct`:** `RecoveredPhaseElapsed.totalSecondsLowerBound`
  – nie mehr Ausfallzeit kreditiert, als Alt-Boot-lokal belegt ist.
- **`WaitingForProduct` mit `DefinitelyStillValid`:** `totalSecondsUpperBound`
  (in diesem Fall garantiert vorhanden) – die fuer diese Phase konservative
  Richtung.
- **`WaitingForProduct` mit `DefinitelyExpired`/`Uncertain`:** kein Resume-
  Versuch (5.9/5.13).

Liefert `hop2.status != Proposed` (z. B. weil das eingebaute
`WaitingForProduct`-Sicherheitsnetz trotz eigener Vorpruefung ablehnt), wird
kein Resume erzwungen; Rueckfall auf Hop-1-only (5.13).

**Gate-A-Kopplung:** Zwischen Hop 1 und Hop 2 wird die reale
Restart-Sensorauswahl (5.20) ausgewertet; negatives Ergebnis ->
`request.event = ProcessEvent::RecoveryReject` (bestehende, bereits
implementierte `RecoveryEvaluation -> Fault`-Logik).

### 5.9 WaitingForProduct: definitiv abgelaufene Wartezeit – Tombstone

Unveraendert aus Revision 4: `TransitionReason::RecoveryEndedByExpiredWait`,
`validControlTopology`-Zweig `from == RecoveryEvaluation && to == Standby`,
direkt ueber `propose()` konstruiert (keine wiederverwendbare bestehende
Funktion dafuer vorhanden), `clearActiveRunState(candidate)` vor der
Persistierung, Coordinator erreicht `ReadyEmpty`.

### 5.10 Schema-3-Gueltigkeitsvertrag fuer `RecoveryEvaluation` bei aktivem Run

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 4):**
`validStateFor()` (`run_persistence_contract.cpp:10-42`) erlaubt
`RecoveryEvaluation` fuer `RunCheckpointVariant::ProgramRun`/`ManualRun`
**nicht** (weder in der `ProgramRun`- noch der `ManualRun`-Fallliste).
`makeRunPersistenceSnapshot()` wuerde einen Hop-1-only-Kandidaten (5.13)
damit ablehnen – nicht speicherbar.

**Vertrag:**

1. `validStateFor()` wird um `RecoveryEvaluation` in beiden Fallisten
   (`ProgramRun`, `ManualRun`) ergaenzt.
2. **Zusaetzliche, engere Konsistenzpruefung** in
   `validateRunPersistenceSnapshot()` (nicht in `validStateFor()` selbst,
   um dessen einfache Struktur zu erhalten): ist
   `snapshot.processState.state == RecoveryEvaluation` **und**
   `snapshot.variant != NoActiveRun`, dann ist der Snapshot nur gueltig,
   wenn zusaetzlich gilt:
   - `snapshot.pendingRecoveryOriginalState.has_value()` (5.13, neues
     Schema-3-Feld, in diesem Fall verpflichtend);
   - `pendingRecoveryOriginalState->state != ProcessState::RecoveryEvaluation`
     (die urspruengliche Phase ist eine echte Altphase, keine
     Selbstreferenz);
   - `stateMatchesRunSnapshot(pendingRecoveryOriginalState->state,
     *snapshot.processRunSnapshot)` (die urspruengliche Phase passt zum
     mitgelieferten Programm-/manuellen Schnappschuss).

   Fehlt eine dieser Bedingungen, ist der Snapshot ungueltig – kein aktiver
   Run darf ausserhalb dieses eng begrenzten Pending-Recovery-Falls in
   `RecoveryEvaluation` stehen.
3. **Keine stille Liberalisierung von Schema 1/2:** ein nach altem Schema
   (vor #18) geschriebener Datensatz konnte `RecoveryEvaluation` fuer einen
   aktiven Run strukturell nie enthalten (die alte `validStateFor()`-Liste
   liess das nicht zu) – die Erweiterung macht eine vorher **immer**
   ungueltige Kombination unter den engen, oben genannten zusaetzlichen
   Bedingungen gueltig; bereits existierende Alt-Daten sind von dieser
   Erweiterung nicht betroffen, da sie diese Kombination nie erzeugen
   konnten. Kein expliziter Schema-Versionscheck an dieser Stelle noetig.

Contract-/Codec-Tests: aktiver Schema-3-Snapshot mit `RecoveryEvaluation`
und vollstaendigem, konsistentem Pending-Kontext -> gueltig; ohne
Pending-Kontext -> ungueltig; mit inkonsistentem
`pendingRecoveryOriginalState` (falsche Phase fuer den Snapshot) ->
ungueltig; Schema-1/2-Decodierung kann diese Kombination gar nicht
erzeugen (Regressionstest gegen bestehende Migrationsvektoren).

### 5.11 Reboot waehrend bereits persistiertem `RecoveryEvaluation`

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 4):**
`RecoveryReentryRequired`s Topologie erlaubt als Quelle ausschliesslich
`stateUsesRunSnapshot(from)`-Phasen; `RecoveryEvaluation` selbst gehoert
nicht dazu (`stateUsesRunSnapshot(RecoveryEvaluation) == false`). Ein
zweiter Reboot waehrend einer noch offenen Hop-1-only-Recovery (5.13) laedt
aber genau `processState.state == RecoveryEvaluation` – Hop 1 ist auf
dieses geladene Ergebnis nicht anwendbar.

**Vertrag – Ladeklassifikation um einen dritten Fall erweitert:**

```text
loadAndInitialize():
  variant == NoActiveRun                                -> ReadyEmpty (unveraendert)
  variant != NoActiveRun && state != RecoveryEvaluation  -> LoadedActiveRun (Hop-1-faehig, 5.7)
  variant != NoActiveRun && state == RecoveryEvaluation  -> LoadedActiveRun
      (bereits im Recoveryzustand befindlicher Run; kein Hop 1 noetig,
       erkannt an processState.state selbst, keine neue
       RunPersistenceCoordinatorState-Auspraegung)
```

Kein zweiter kuenstlicher `RecoveryReentryRequired`-Transition-Hack:
`RunRecoveryCoordinator::activate()` prueft `current.processState.state`
selbst und ueberspringt Hop 1 vollstaendig, wenn bereits
`RecoveryEvaluation` vorliegt (Voraussetzung: `pendingRecoveryOriginalState`
ist vorhanden, sonst `NotReconstructible` – ein aktiver Snapshot in
`RecoveryEvaluation` ohne Pending-Kontext ist gemaess 5.10 strukturell
ungueltig und waere schon beim Laden abgelehnt worden).

**Episode-Refresh statt Transition:** Da kein Zustandsuebergang stattfindet,
aber die Zeitbasis (UTC-jetzt, ggf. zwischenzeitlich neuere periodische
Checkpoints, siehe unten) sich seit dem letzten Boot geaendert haben kann,
fuehrt `activate()` in diesem Fall einen eigenen, einfacheren Commit aus:
`candidate = current` (keine Transition), `candidate.recoveryEpisodeRevision
+= 1U` (5.16), `pendingRecoveryOriginalState` bleibt unveraendert (beschreibt
weiterhin die **urspruengliche** Altphase vor dem allerersten Ausfall, nicht
den Zwischenzustand). Geschrieben ueber denselben Commit-Kern (5.12).
Dadurch: **jeder** Reboot, der eine Recovery-Bewertung neu beginnt – ob via
echtem Hop 1 oder via Episode-Refresh –, erhoeht `recoveryEpisodeRevision`
genau einmal; ein zwischen zwei Boots eingereichter, jetzt veralteter
Korrektur-/Entscheidungsversuch wird ueber den bestehenden
`expectedRecoveryEpisodeRevision`-Abgleich (5.17) als `StaleState` erkannt.

`state_` bleibt `Ready` (kein Unterschied zur regulaeren Hop-1-only-Situation);
`RecoveryEvaluation` traegt weiterhin keine Aktorfreigabe; die
Aufloesungswege (automatisch/Benutzer, 5.13) bleiben unveraendert erreichbar,
jetzt gegen die aufgefrischte Episode.

Test: Reboot -> Hop-1-only -> erneuter Reboot -> `recoveryEpisodeRevision`
erhoeht sich erneut, `pendingRecoveryOriginalState` bleibt identisch, beide
Aufloesungswege bleiben funktionsfaehig.

### 5.12 Gemeinsamer Commit-Kern – korrekter Transaktionsschnitt

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 4):**
`writeSnapshot()` (`run_persistence_coordinator.cpp:287-...`) akzeptiert
ausschliesslich `state_ == Ready || state_ == ReadyEmpty` (Zeilen 292-294).
Revision 4 rief den gemeinsamen Recovery-Commit-Kern jedoch aus
`LoadedActiveRun` und aus `BlockedIndeterminate/FallbackRecovery` auf und
behauptete gleichzeitig, die bestehende Vorbedingung bleibe unveraendert –
widersspruechlich und nicht ausfuehrbar.

**Vertrag:** Der bestehende Koerper von `writeSnapshot()` wird in eine
private, guard-lose Funktion extrahiert:

```cpp
// privat, KEINE state_-Vorbedingung – Aufrufer traegt die Verantwortung
RunPersistenceResult RunPersistenceCoordinator::writeSnapshotCore(
    const RunPersistenceSnapshot& snapshot, const RunCheckpointTime& time,
    bool periodic, const RunCommandState& before,
    RunPersistenceMutationKind mutationKind, std::optional<CommandId> commandId,
    std::optional<std::size_t> targetSlotOverride);

RunPersistenceResult RunPersistenceCoordinator::writeSnapshot(
    const RunPersistenceSnapshot& snapshot, const RunCheckpointTime& time,
    bool periodic, const RunCommandState& before,
    RunPersistenceMutationKind mutationKind, std::optional<CommandId> commandId) {
    if (state_ != RunPersistenceCoordinatorState::Ready &&
        state_ != RunPersistenceCoordinatorState::ReadyEmpty) {
        return unavailableResult();
    }
    return writeSnapshotCore(snapshot, time, periodic, before, mutationKind,
                             commandId, /*targetSlotOverride=*/std::nullopt);
}
```

Alle vier bestehenden oeffentlichen Standardpfade (`persistCommand`,
`persistTransition`, `persistSensorSelection`, `checkpointPeriodic`) rufen
weiterhin ausschliesslich das unveraenderte `writeSnapshot()` mit seinem
bestehenden Guard auf – **kein** Verhaltensunterschied fuer diese Pfade.

Der Recovery-Commit-Kern `commitRecoveryOutcome(...)` (5.7-5.9, 5.13, 5.14)
ruft **ausschliesslich** `writeSnapshotCore()` direkt auf, mit einer
eigenen, engen Vorbedingung: `state_ == LoadedActiveRun ||
(state_ == BlockedIndeterminate && blockedIndeterminateReason_ ==
FallbackRecovery)`. Kein temporaeres Umsetzen von `state_` auf `Ready` vor
dem Commit. Write-before-apply sowie alle bestehenden Prepared-/Slot-/
Head-Fehlersemantiken (5.15) bleiben identisch, da `writeSnapshotCore` exakt
derselbe Code ist wie zuvor `writeSnapshot`, nur ohne die eine Guard-Zeile.
Der Fallback-Slot-Override (5.14) wird als Parameter durchgereicht.

`resolveRecoveryOutcome`/`ResolveRecoveryUncertainty` (5.13, Hop-2-only,
nur relevant wenn `current.processState.state == RecoveryEvaluation` und
`state_` bereits `Ready` ist) verwenden weiterhin das **normale**,
guard-behaftete `writeSnapshot()` – sie benoetigen den Bypass nicht, da
`state_` zu diesem Zeitpunkt bereits `Ready` ist.

### 5.13 Unsichere Recovery – Hop-1-only, zwei gleichwertige Aufloesungswege

Bei `RecoveryTimeVerdict::Uncertain` (5.4, nur `WaitingForProduct`) oder
fehlendem Gate-A-Ergebnis ohne sichere `RecoveryReject`-Anzeige wird nur
Hop 1 committet (`commitRecoveryOutcome`, 5.12). `pendingRecoveryOriginalState
= originalRestoredProcessState` wird dabei gesetzt (5.10). Coordinator-
Ergebnis: `Ready`.

**Zwei gleichwertige Aufloesungswege**, beide ueber denselben Commit-Kern:

1. **Automatisch:** `RunRecoveryCoordinator::reevaluatePendingRecovery(RunCommandState&,
   const RunCheckpointTime&)` (nativ testbar; produktive Verdrahtung eines
   Aufrufers ist nicht Teil von #18, 5.21/Gate B). Wiederholt 5.2-5.8 gegen
   `current.processState.state == RecoveryEvaluation` und
   `pendingRecoveryOriginalState`.
2. **Benutzerpfad:** `CommandKind::ResolveRecoveryUncertainty`
   (`AssumeStillValid`/`AssumeThresholdCrossed`), ueber die bestehende
   `persistCommand`-Infrastruktur (`expectedRunRevision`,
   `expectedRecoveryEpisodeRevision`, 5.16; `persistedIds_`-Dedup,
   `StaleState`/`AlreadyProcessed`). Nur zulaessig innerhalb der
   ausgewiesenen Unsicherheit. `AcknowledgeMessage` bleibt reine
   Quittierung, nicht zweckentfremdet.

Bei erfolgreicher Aufloesung wird `pendingRecoveryOriginalState` auf
`nullopt` gesetzt (Pending-Kontext beendet); Ergebnis-`state_` gemaess 5.9
(`ReadyEmpty`) oder unveraendert `Ready` (Resume/Reject).

### 5.14 `FallbackRecovered` – kein Dead-End

Unveraendert aus Revision 4: Hop 1 wird **immer** versucht, unabhaengig
davon, ob die Quelle `Current` oder `FallbackRecovered` war, und immer
atomar committet (ueber `writeSnapshotCore` mit Slot-Override, 5.12), sobald
er lokal erfolgreich aufgebaut werden konnte. Der Ursprung ist danach
irrelevant – beide konvergieren auf `Ready`+`RecoveryEvaluation`, aufloesbar
ueber 5.13. `BlockedIndeterminate` bleibt ausschliesslich reserviert fuer
den Fall, dass Hop 1 selbst lokal nicht aufgebaut/angewendet werden kann.

### 5.15 `BlockedIndeterminate` und `PersistenceCommittedApplyFailed` – getrennt

Unveraendert aus Revision 4 (verifiziert weiterhin korrekt): Store-
Schreibausgang unbestimmt (`RunPersistenceStoreWriteResult::Indeterminate`,
`run_persistence_coordinator.cpp:427-431,452,466,477,486,500,518`) ->
`BlockedIndeterminate`/`StoreOutcomeUnknown`; bestaetigter Commit mit
fehlgeschlagenem RAM-Apply (`run_persistence_coordinator.cpp:617-625,676-685`)
-> `PersistenceCommittedApplyFailed`. `writeSnapshotCore` (5.12) und die
`ApplyRecoveryTimeCorrection`-Persistierung (5.17) uebernehmen beide
Zustaende unveraendert und getrennt.

### 5.16 Sensorevidenz – Vor-/Nach-Ausfall strukturell getrennt

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 4):** Ein
einzelnes `RecoveryTemperatureEvidence{air,product,cooling}` kann nicht
gleichzeitig die laufend fortgeschriebene "letzte gueltige"-Evidenz und die
eingefrorene Vor-/Nach-Ausfall-Diagnose einer bestimmten Recoveryepisode
tragen, ohne dass spaetere normale Checkpoints die Diagnosedaten
ueberschreiben.

```cpp
struct RoleTemperatureEvidence {
    std::optional<double> filteredCelsius;
    device_platform::SensorQuality quality{device_platform::SensorQuality::Stale};
};
struct CrossRoleEvidence {
    RoleTemperatureEvidence air;
    RoleTemperatureEvidence product;
    RoleTemperatureEvidence cooling;
};

// Schema 3, Teil von RunPersistenceSnapshot:
struct RecoveryTemperatureEvidence {
    CrossRoleEvidence lastKnown;  // laufend, von jedem normalen Checkpoint aktualisiert
};
struct RecoveryEpisodeEvidence {
    CrossRoleEvidence beforeOutage;             // Kopie von lastKnown, eingefroren bei Hop 1/Episode-Refresh
    std::optional<CrossRoleEvidence> afterRestart;  // genau einmal gesetzt, bei Hop 1/Episode-Refresh aus der fuer Gate A ohnehin benoetigten CrossRolePlausibilityContext
};
```

`lastKnown` wird ausschliesslich von der bestehenden, in allen vier
Checkpoint-Schreibpfaden neu hinzugefuegten
`updateRoleEvidence(RoleTemperatureEvidence&, const
device_platform::SensorQualitySnapshot&)` (unveraendert aus Revision 4:
aktualisiert `filteredCelsius` nur bei `SensorQuality::Valid`, behaelt den
letzten gueltigen Wert bei `Stale`/`Failed`) fortgeschrieben – ueber einen
neuen, optionalen Parameter `const CrossRolePlausibilityContext*
liveSensorEvidence` an allen vier Pfaden (unveraendert aus Revision 4,
`nullptr` zulaessig).

`lastRecoveryEpisodeEvidence: std::optional<RecoveryEpisodeEvidence>`
(separates Schema-3-Feld) wird bei Hop 1/Episode-Refresh **einmalig**
befuellt (`beforeOutage = candidate.recoveryTemperatureEvidence.lastKnown`
zum Zeitpunkt des Commits) und danach von keinem spaeteren, regulaeren
Checkpoint-Schreibvorgang mehr veraendert – nur eine erneute Hop-1-
Ausfuehrung (naechste Recoveryepisode) ueberschreibt es wieder vollstaendig.
Kein "before"/"after" geht verloren, keine Diagnosedaten werden
versehentlich durch spaetere Live-Checkpoints ueberschrieben.

### 5.17 `RunProgressState`, Fortschrittsfortschreibung, Schema-1/2-Migration

**Fortschreibungspunkte** (unveraendert aus Revision 4, weiterhin
verifiziert korrekt): `deriveFermentingSecondsDelta(before, atMillis)`,
angewandt (1) bei jedem Live-Phasenwechsel **aus** `Fermenting`, (2) in
`decideRunAdjustment()` unmittelbar **vor** der bestehenden
`stateEnteredAtMillis`-Neusetzung bei Daueraenderung
(`run_commands.cpp:1019-1023`), (3) bei Hop 1, wenn
`originalRestoredProcessState.state == Fermenting`, ausschliesslich aus
`checkpointMonotonicMillis` (dem Alt-Boot-lokalen, sicher bekannten
Anteil – **nicht** aus dem unsicheren Ausfallanteil, siehe naechster
Absatz). Kein `RunProgressAccountingRuntime`-Typ; `stateEnteredAtMillis`
selbst erfuellt bereits die Rolle der Buchfuehrungsreferenz.

**Trennung von Fortschritts-Gutschrift und Grenzbewertung (Klarstellung
gegenueber Revision 4):** Nur der sicher bekannte Alt-Boot-Anteil
(`RecoveredPhaseElapsed.knownSecondsBeforeCheckpoint`) wird automatisch in
`observedRunSeconds` gefaltet. Der unsichere Ausfallanteil
(`RecoveryOutageBounds`) fliesst **nicht** automatisch in
`observedRunSeconds` ein – er dient ausschliesslich der Grenzbewertung
(5.4-5.5, ueber `priorElapsed`, konservativ mit der Untergrenze). Eine
explizite Gutschrift des Ausfallanteils in `observedRunSeconds` erfolgt
ausschliesslich ueber die kontrollierte, begrenzte Korrektur (5.17
`ApplyRecoveryTimeCorrection`) – niemals automatisch ohne diese Kontrolle
(Gate C).

**Schema-1/2-Migration (neu, Korrektur gegenueber Revision 4):**
`RunProgressState` existierte vor Schema 3 nicht. `observedRunSeconds{0U}`
als stiller Default wuerde "sicher null Sekunden" und "bei altem Schema
unbekannt" nicht unterscheidbar machen.

```cpp
enum class RunProgressBasis : std::uint8_t {
    Known,          // observedRunSeconds ist eine belastbare Zaehlung
    LegacyUnknown,   // aus Schema 1/2 migriert, kein Vorwert bekannt
};
struct RunProgressState {
    RunProgressBasis basis{RunProgressBasis::Known};
    std::uint32_t observedRunSeconds{0U};  // nur bedeutsam, wenn basis == Known
};
```

Codec: Decodiert ein Schema-1- oder Schema-2-Datensatz (kein
`RunProgressState`-Feld im Wireformat), wird `basis = LegacyUnknown`
gesetzt, **nicht** `Known` mit `observedRunSeconds = 0`. Der erste
Fold-Punkt (oben) **nach** einer solchen Migration schaltet
`basis: LegacyUnknown -> Known` um und beginnt die Zaehlung ab genau diesem
Delta (kein rueckwirkendes Erfinden von Sekunden vor der Migration). Anzeige/
Export rendert `LegacyUnknown` explizit als "unbekannt", nicht als "0
Sekunden". Migrationstests: Schema 1 -> 3 und Schema 2 -> 3 fuer einen
aktiven Run (`basis == LegacyUnknown` nach Migration, `Known` nach dem
ersten Fold); gemischte Current/Fallback-Matrix 3/2 und 3/1; korrupter
Schema-3-Current mit gueltigem Schema-2/1-Fallback; Prepared-Head-
Unterbrechung ueber den Versionswechsel hinweg (Erweiterung der
bestehenden Migrationstestreihe).

### 5.18 `ApplyRecoveryTimeCorrection` – fachliche Wirkung und harte Grenzen

**Geltungsbereich:** ausschliesslich fuer `Fermenting` (die einzige Phase
mit einer Geschaeftsprogress-Metrik, `observedRunSeconds`). Fuer
`WaitingForProduct` ist `ResolveRecoveryUncertainty` (5.13) der zustaendige,
semantisch andere Vertrag (Phasengrenzentscheidung: fortsetzen/beenden,
keine Sekundenkorrektur) – beide Kommandos bleiben getrennt.

**Wirkung:** mutiert ausschliesslich
`RunCommandState.runProgress.observedRunSeconds` (und setzt `basis = Known`,
falls zuvor `LegacyUnknown`).

**Harte Grenzen, aus dem tatsaechlichen Ausfallintervall abgeleitet:**
`appliedSecondsDelta: std::uint32_t` (keine negativen Korrekturen – der
automatische Hop-1-Fold verwendet immer die Untergrenze, eine gueltige
Korrektur bewegt sich daher ausschliesslich aufwaerts in Richtung der
Obergrenze, nie darunter). Gueltiger Bereich:
`[0, totalSecondsUpperBound - totalSecondsLowerBound]`
(`RecoveredPhaseElapsed` fuer `Fermenting` zum Episodenzeitpunkt). Ist
`totalSecondsUpperBound` nicht bekannt (keine UTC-Bruecke, 5.2), ist **kein**
Korrekturwert gegen eine obere Grenze pruefbar -> jede Korrektur wird
abgelehnt (`InvalidInput`), **kein** Saturieren auf einen Ersatzwert.
Ein Wert ausserhalb `[0, maxDelta]` wird ebenso abgelehnt, nicht auf die
Grenze gekappt. Additionsueberlauf von `observedRunSeconds` (uint32) wird
vor dem Schreiben geprueft und fuehrt zu `InvalidInput` (praktisch nicht
erreichbar, aber explizit abgesichert).

**Automatischer Korrekturpfad:** sobald `reevaluatePendingRecovery`/eine
verbesserte UTC-Lage eine praezisere Grenze liefert als die urspruenglich
bei Hop 1 verwendete Untergrenze, wird **dieselbe** begrenzte Mutation
(gleiche Ober-/Untergrenzen-Pruefung) automatisch mit der Differenz
angewendet – bleibt damit ebenso konservativ und erfindet keine
biologische Gutschrift ausserhalb des belegten Intervalls.

**Episoden-/Staleness-Vertrag** (unveraendert aus Revision 4):
`CommandKind::ApplyRecoveryTimeCorrection`; `RecoveryTimeCorrectionRecord
{ appliedAtEpisodeRevision: uint32; appliedSecondsDelta: uint32; }`
(Schema 3, optional); zweiter Versuch mit identischem Inhalt fuer dieselbe
Episode -> `AlreadyProcessed`; mit abweichendem Inhalt -> `NotAllowedInState`;
`CommandEnvelope.expectedRunRevision` **und** neues
`expectedRecoveryEpisodeRevision` muessen beide mit `current` uebereinstimmen,
sonst `StaleState`; Write-before-apply; Fehlerfaelle getrennt nach 5.15.

### 5.19 `Completed` – expliziter, schmaler Sonderpfad

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 4):**
`docs/RECOVERY_AND_INTERRUPTION.md:163-174` verlangt fuer `COMPLETED`: keine
Temperaturregelung neu starten, Ergebniszustand wiederherstellen, erst
Benutzerquittierung fuehrt nach `STANDBY`. `RecoveryReentryRequired` deckt
`Completed` bewusst nicht ab (`stateUsesRunSnapshot(Completed) == false`).
Ein pauschaler `RunRecoveryCoordinator::activate()`-Aufruf auf jeden
geladenen Snapshot wuerde fuer `Completed` `InvalidDecision` liefern.

**Vertrag:** `RunPersistenceCoordinator`/`RunRecoveryCoordinator` behandeln
`processState.state == Completed` als eigenen, fruehen Sonderfall, **vor**
jedem Hop-1-Versuch:

- Keine `TransitionDecision`/`applyProcessTransition` noetig (keine
  Entscheidung wird getroffen – die Phase aendert sich nicht, es gibt keine
  Ambiguitaet, kein Sicherheitsgate).
- Direkte Uebernahme: `current = restoredState`, mit **einer** expliziten
  technischen Korrektur: `current.processState.stateEnteredAtMillis =
  monotonicMillis` (aktueller Boot) – noetig, da `decideProcessTransition`
  bei der spaeteren `CompletionAcknowledged`-Entscheidung sonst
  `runtimeTimeIsValid` gegen einen Alt-Boot-Wert prueft und faelschlich
  `TimeWentBackwards` liefern koennte. Keine sonstige Feldaenderung.
- Keine Aktorfreigabe (unveraendert, `Completed` traegt ohnehin keine).
- `state_ = Ready` direkt, kein `LoadedActiveRun`-Zwischenschritt fuer
  diesen Fall (kein Hop 1 vorgesehen, keine Recovery-Bewertung noetig).
- Bestehender `CompletedRunRestored`-Reason bleibt fuer den originalen,
  echten Boot-Zeitpunkt (unveraendert, ausserhalb dieses Sonderpfads)
  unangetastet.

Test: persistierter `Completed`-Snapshot bleibt nach Reboot `Completed` bis
zur Quittierung; `stateEnteredAtMillis` liegt im aktuellen Boot; kein
`InvalidDecision`.

### 5.20 `WeightedProgressStatus` – entfernt (KISS)

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 4):** Ein
persistiertes Enum mit exakt einem heute erreichbaren Wert traegt keinen
Zustand, der sich innerhalb von Release 1 aendern kann – ein konstantes
Wire-Feld ohne fachliche Funktion.

**Vertrag:** Kein `WeightedProgressStatus`-Feld im Schema-3-Persistenzvertrag.
"Nicht kalibriert" ist eine statische, aus dem Firmware-/Commissioning-Stand
ableitbare Anzeige-/Exportkonstante (nicht pro Lauf persistiert). Die reale
Aktivitaetsgewichtung bleibt ohne validiertes Modell ausdruecklich
unimplementiert; #34 liefert die Messgrundlage (verifiziert per `gh issue
view 34`: Referenzmessungen, Offsets, thermische Reaktion – kein
Aktivitaetskennfeld als eigenes Akzeptanzkriterium), ist aber nicht der
stille Modelleigentuemer. Ein spaeteres, dann eigenstaendig zu planendes
Vorhaben fuehrt bei Bedarf ein echtes, mehrwertiges Statusfeld ein.

### 5.21 Restart-Sensorauswahl (Gate A)

Unveraendert: `SensorSelectionPhase::RestartRevalidationPending` wird
zwischen Hop 1 und Hop 2 real bewertet; `computeRestartSensorSelection`
(`sensor_selection.cpp:890-907`, Stub) wertet den persistierten
Sensorselektionszustand gegen die aktuelle `CrossRolePlausibilityContext`
aus; negatives Ergebnis -> `RecoveryReject`.

### 5.22 Komposition/DI – kein erfundener Aufrufer

Unveraendert aus Revision 4: `RunRecoveryCoordinator::activate(...)` und
`reevaluatePendingRecovery(...)` sind nativ testbare APIs; kein bestehender
produktiver Aufrufer wird behauptet; produktive Verdrahtung bleibt dem
zustaendigen Composition-Issue vorbehalten (Gate B).

### 5.23 ROADMAP-Konsistenz

**Ausgangsbefund (verifiziert):** `docs/ROADMAP.md:3` zeigt weiterhin
`Stand: 2026-08-07` trotz Aktualisierung in dieser Session; Zeile 32
("offene Ownerentscheidungen stehen im Plan unter `docs/tasks/`") liest
sich so, als bestuenden aktuell offene Ownerentscheidungen, obwohl Abschnitt
3 dieses Plans keine offenen Gates mehr ausweist.

**Vertrag (in diesem Plan-Commit umgesetzt, Abschnitt 12):** Standdatum auf
das tatsaechliche Aenderungsdatum aktualisiert; Zeile 32 praezisiert, dass
Details und Abhaengigkeitsstand im Plan stehen, ohne offene
Ownerentscheidungen zu behaupten, wo keine bestehen; #18/PR #102 bleibt
aktuelle Arbeit; Ressourcen-Gate ueber #29/`OPEN_POINTS.md` weiterhin
sichtbar; #22 bleibt naechste fachliche Arbeit nach #18.

### 5.24 #24-Abgrenzung

Unveraendert: Fault-Klassen, SAFE_BOOT-Feinausbau und Fault-Reset-Ablauf
bleiben #24 zugeordnet. #18 nutzt `Fault` ausschliesslich ueber die
bestehende `RecoveryReject`-Logik.

## 6. Modul- und Abhaengigkeitsgrenzen

Alle neuen/geaenderten Dateien liegen in `lib/fermentation_app/src/` und
haengen ausschliesslich von bestehenden `device_platform`-Ports und
bestehenden `fermentation_app`-Modulen ab (ADR-013 unveraendert
eingehalten). Kein neuer `SensorRole`-Typ (feste `air`/`product`/`cooling`-
Felder, konsistent mit `CrossRolePlausibilityContext`).

## 7. Datei-/Commit-Aufschluesselung

| # | Commit | Inhalt |
|---|---|---|
| 1 | `feat(process-state-machine): RecoveryReentryRequired-/RecoveryEndedByExpiredWait-Topologie, PriorBootPhaseElapsed-Parameter, elapsedWithPrior` | 5.5-5.9 |
| 2 | `feat(persistence): Schema 3 – RunProgressState (mit Legacy-Basis), RecoveryTemperatureEvidence/RecoveryEpisodeEvidence, RecoveryTimeCorrectionRecord, recoveryEpisodeRevision, pendingRecoveryOriginalState, validStateFor-Erweiterung` | 5.10, 5.16, 5.17, 5.18; Migrationstests 1/2/3 |
| 3 | `feat(recovery): computeRecoveryOutageBounds, computeRecoveredPhaseElapsed, evaluateRecoveryTimeVerdict` | `run_recovery_time.hpp/.cpp` (5.2-5.4) |
| 4 | `feat(sensor-selection): reale Restart-Reaktivierung` | Gate A / 5.21 |
| 5 | `feat(persistence-coordinator): writeSnapshotCore-Extraktion, Signaturerweiterung der vier Checkpoint-Schreibpfade um liveSensorEvidence` | 5.12, 5.16 |
| 6 | `feat(persistence-coordinator): commitRecoveryOutcome, activateLoadedRun (Hop 1 + bedingt Hop 2), Episode-Refresh-Pfad` | 5.7-5.9, 5.11-5.13 |
| 7 | `feat(persistence-coordinator): activateFallbackRecoveredRun, Slot-Override` | 5.14 |
| 8 | `feat(persistence-coordinator): resolveRecoveryOutcome, ResolveRecoveryUncertainty, Completed-Sonderpfad` | 5.13, 5.19 |
| 9 | `feat(run-commands): ApplyRecoveryTimeCorrection, AdjustRun-Zeitfaltung` | 5.17, 5.18 |
| 10 | `feat(recovery): RunRecoveryCoordinator (activate, reevaluatePendingRecovery)` | 5.22 |
| 11 | `docs: Anzeigevertrag, Ressourcenbudget, ROADMAP-Korrektur` | 5.23, Abschnitt 10 |

## 8. Testmatrix

1. Hop 1 mit echter geladener Altphase; Negativtest gegen vorgetaeuschten
   `Boot`-Quellzustand.
2. `computeRecoveryOutageBounds`: exakte UTC-Bruecke bleibt ein Intervall
   (Ober-/Untergrenze via Kontrollpunktabstand), fehlender Anker -> `nullopt`.
3. `computeRecoveredPhaseElapsed`: korrekte Komposition aus Alt-Boot-Anteil
   und Ausfallintervall, getrennt testbar von `RecoveryOutageBounds`.
4. Frischer Boot mit `now` deutlich kleiner als bereits bekannte
   Phasenzeit: **kein** unsigned Rebasing-Unterlauf (`elapsedWithPrior`
   statt Subtraktion von `now`).
5. `WaitingForProduct`/`Fermenting`/`CoolHolding` mit Vor-Boot-Phasenanteil
   ueber `priorElapsed` korrekt beruecksichtigt.
6. Target-Reach-/Qualification-Timer aus altem Boot werden niemals roh
   weiterverwendet (volle Tabelle 5.6, alle acht Phasen).
7. `WaitingForProduct` `DefinitelyExpired` -> Tombstone -> `ReadyEmpty` ->
   Reboot `NoActiveRun`.
8. Aktiver Schema-3-Snapshot mit `RecoveryEvaluation` nur mit gueltigem
   Pending-Kontext gueltig; ohne/mit inkonsistentem Kontext ungueltig.
9. Reboot waehrend bereits persistiertem `RecoveryEvaluation`:
   Episode-Refresh statt Hop 1, `recoveryEpisodeRevision` erhoeht sich,
   beide Aufloesungswege bleiben erreichbar.
10. Recovery-Commit aus `LoadedActiveRun` und aus
    `BlockedIndeterminate/FallbackRecovery` gelingt trotz unveraendertem
    oeffentlichem `writeSnapshot`-Guard (ueber `writeSnapshotCore`).
11. Fallback-Slot-Override: Store-Schnitt zwischen Slot- und Kopfschreiben.
12. Getrennte Vor-/Nach-Ausfall-Sensorevidenz; ein spaeterer normaler
    Checkpoint ueberschreibt `lastRecoveryEpisodeEvidence` **nicht**.
13. Schema-1/2-Legacy-Progress: `basis == LegacyUnknown` nach Migration,
    `Known` erst nach dem ersten Fold-Ereignis; Anzeige unterscheidet
    "unbekannt" von "0 Sekunden".
14. `ApplyRecoveryTimeCorrection` innerhalb der Grenzen erfolgreich; ausserhalb
    -> Ablehnung ohne Saturierung; ohne bekannte Obergrenze -> Ablehnung;
    Overflow-Schutz.
15. `Completed`-Restore: bleibt `Completed` bis Quittierung, kein
    `InvalidDecision`, `stateEnteredAtMillis` im aktuellen Boot.
16. `StoreOutcomeUnknown` vs. bestaetigter Commit + RAM-Apply-Fehler
    weiterhin getrennt.
17. Schema-1/2/3-Current/Fallback-Matrix vollstaendig (Regression).
18. Alle bestehenden #20/#21-Sensor-/Sicherheitsregressionen bleiben gruen.
19. `git diff --check`.

## 9. Safety-/Security-/Recovery-/Hardwaregrenzen

Keine Aktorfreigabe vor abgeschlossenem Hop 2 oder vor `Completed`-
Quittierung. Kein Schreiben vor vollstaendigem lokalem Kandidatenaufbau.
Kein Aktorpfad direkt aus `FallbackRecovered` vor Hop 1. Reale Hardware-/
NVS-Anbindung bleibt #29/#90 vorbehalten.

## 10. Ressourcen-/Betriebsbudget

Schema-3-Zuwachs: `RunProgressState` (5 Byte inkl. Basis-Tag),
`RecoveryTemperatureEvidence.lastKnown` (~30 Byte), `RecoveryEpisodeEvidence`
(optional, ~60 Byte waehrend offener Episode), `RecoveryTimeCorrectionRecord`
(8 Byte, optional), `recoveryEpisodeRevision` (4 Byte),
`pendingRecoveryOriginalState` (Groesse von `ProcessRuntimeState`, optional,
nur waehrend offener Episode belegt). Deutlich unter dem bestehenden
`kMaximumCheckpointRecordBytes`-Budget, durch Migrationstest 8.17 belegt.

## 11. SOLID/DRY/KISS

`RecoveryOutageBounds`/`RecoveredPhaseElapsed` sind zwei kleine, einzeln
testbare, nicht ueberladene Typen statt eines vermischten Objekts.
`elapsedWithPrior` ist eine einzige neue Vergleichsfunktion, an drei
bestehenden Stellen sowie am eingebauten `WaitingForProduct`-Check
wiederverwendet. `writeSnapshotCore` ist eine einzige Extraktion, von allen
Schreibpfaden (Standard und Recovery) gemeinsam genutzt, mit exakt einer
zusaetzlichen Guard-Variante fuer Recovery. `WeightedProgressStatus` entfaellt
vollstaendig statt eines Ein-Wert-Enums.

## 12. Dokumentations-/Abschlussnachweis

- `docs/ROADMAP.md`: in diesem Plan-Commit korrigiert (5.23).
- `docs/RUN_PERSISTENCE.md`/`docs/RECOVERY_AND_INTERRUPTION.md`: werden im
  Umsetzungscommit (Nr. 11) um die in 5.2-5.19 vertraglich fixierten Punkte
  ergaenzt.
- `git diff --check`: nach Commit dieser Datei auszufuehren.
- **Remote-Verifikation (Pflicht):** nach dem Push wird
  `origin/plan/issue-18-restart-weighted-progress` per frischem `git fetch`
  gelesen und mit dem lokalen `HEAD` sowie mit `gh api
  repos/ManuEngineer/ESP32-Fermentationsschrank/pulls/102` (`head.sha`) und
  `gh pr view 102 --json headRefOid` abgeglichen, bevor PR-Beschreibung/
  SESSION HANDOVER als aktuell gemeldet werden.

## 13. Pflichtaufgabenliste (fuer die Umsetzung, nicht Teil dieser Planungssession)

1. Commit 1-11 gemaess Abschnitt 7, je mit gezielten lokalen Tests.
2. Testmatrix Abschnitt 8 vollstaendig.
3. `docs/RUN_PERSISTENCE.md`/`docs/RECOVERY_AND_INTERRUPTION.md` nachfuehren.
4. SESSION HANDOVER vor Sessionende bei offenem PR, inkl. verifiziertem
   Remote-SHA.

## 14. Stop-Bedingung

Diese Revision 5 ist ein vollstaendiger, eigenstaendiger Plan. Nach Commit
dieser Datei: **anhalten**, `git push`, Remote-SHA verifizieren
(Abschnitt 12), PR-Beschreibung und SESSION HANDOVER aktualisieren. Keine
Implementierung. Kein `Ready for review`. Keine Remote-CI. Kein Merge.
Keine Branchloeschung.
