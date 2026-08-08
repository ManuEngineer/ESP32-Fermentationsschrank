# Plan: Issue #18 – Wiederanlauf und temperaturgewichteter Fortschritt

## 1. Status

- Revision: **6** (ersetzt alle frueheren Revisionen vollstaendig; kein
  Abschnitt dieser Datei verweist auf eine fruehere Revision als weiterhin
  gueltige Quelle).
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
  direkt am Dateiinhalt verifiziert (u. a. `run_persistence_coordinator.cpp`
  vollstaendig, `process_state_machine.cpp` an allen zitierten Stellen,
  `run_persistence_contract.hpp/.cpp`, `run_checkpoint_schedule.hpp/.cpp`,
  `sensor_selection.hpp`, `sensor_quality_snapshot.hpp`,
  `run_commands.cpp:995-1029`).

## 3. Owner-Entscheidungen (Gates)

- **Gate A:** Restart-Sensorauswahl wird real angewandt (5.23).
- **Gate B:** kein neuer allgemeiner Prozesszyklus; kein produktiver
  Aufrufer wird behauptet, der nicht existiert (5.24).
- **Gate C:** keine unkalibrierte biologische Aktivitaetskurve; unsichere
  Ausfallzeit wird ohne freigegebenes Modell nicht automatisch als
  Fortschritt gutgeschrieben (5.19); nicht beobachtete Ausfallzeit wird
  niemals als `observedRunSeconds` gebucht (5.19).

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
| Ausfallintervall, Boot-Anker-Ableitung (rein) | `lib/fermentation_app/src/run_recovery_time.hpp/.cpp` | neu |
| Recovery-Orchestrierung | `lib/fermentation_app/src/run_recovery.hpp/.cpp` | neu |
| Zustandsautomat: neue Reasons, Topologie, `PriorBootPhaseElapsed`-Parameter | `lib/fermentation_app/src/process_state_machine.hpp/.cpp` | erweitert |
| Persistenzvertrag: Schema 3 (`PendingRecoveryAnchor`, `RunProgressState`, `RecoveryEpisodeEvidence`, `TaggedPriorBootPhaseElapsed`, `RecoveryTimeCorrectionRecord`) | `lib/fermentation_app/src/run_persistence_contract.hpp/.cpp` | erweitert |
| Codec: Schema-3-Gate, Legacy-Migration | `lib/fermentation_app/src/run_persistence_codec.cpp` | erweitert |
| Coordinator: Low-Level-Schreibkern mit Rollbackzustand, `FallbackRecoveryPending`, `activateLoadedRun`, `activateFallbackRecoveredRun`, `resolveRecoveryOutcome`, Episode-Refresh | `lib/fermentation_app/src/run_persistence_coordinator.hpp/.cpp` | erweitert |
| Kommandos: `ResolveRecoveryUncertainty`, `ApplyRecoveryTimeCorrection` | `lib/fermentation_app/src/run_commands.hpp/.cpp` | erweitert |
| Restart-Sensorauswahl (Gate A) | `lib/fermentation_app/src/sensor_selection.hpp/.cpp` | erweitert |
| Tests | `test/test_run_recovery_time/`, `test/test_run_recovery/`, `test/test_process_state_machine/`, `test/test_run_persistence_coordinator/`, `test/test_run_commands/`, `test/test_sensor_selection/` | neu/erweitert |

### 5.2 `RecoveryOutageBounds` – kanonische Ausfalldauer (Vertrag A)

`docs/RUN_PERSISTENCE.md:131-136` legt die Ausfalldauer verbindlich fest als

```text
obere Grenze = aktuelle UTC - letzter verlaesslicher UTC-Kontrollpunkt
untere Grenze = max(0, obere Grenze - maximal moeglicher Kontrollpunktabstand)
```

**auch mit exaktem UTC-Anker auf beiden Seiten bleibt die Ausfalldauer ein
Intervall**, weil das Geraet nach dem letzten Kontrollpunkt noch bis zum
naechsten planmaessigen Speicherzeitpunkt weitergelaufen sein kann
(`docs/RUN_PERSISTENCE.md:119-121`).

**Kritischer Punkt (in dieser Revision korrigiert):** "aktuelle UTC" darf
niemals eine roh zum Abfragezeitpunkt gelesene `utcNow` sein, wenn seit dem
Neustart bereits Zeit vergangen ist – sonst wird die seit dem Neustart
verstrichene *Laufzeit* faelschlich als Teil der *Ausfallzeit* gezaehlt (siehe
5.10 fuer die Ableitung des korrekten Werts). `computeRecoveryOutageBounds`
selbst bleibt dennoch eine reine, boot-unabhaengige Vergleichsfunktion auf
bereits fertig abgeleiteten UTC-Werten:

```cpp
// run_recovery_time.hpp
struct RecoveryOutageBoundsInput {
    std::optional<std::int64_t> utcAtLastCheckpoint;   // PendingRecoveryAnchor.originalCheckpointUtc (5.10) – ueber die gesamte offene Episode unveraendert
    std::optional<std::int64_t> utcAtRestartBoundary;  // deriveUtcAtRecoveryBootAnchor() (5.10) – UTC-Aequivalent des Boot-Zeitpunkts dieser Recovery-Bewertung, NICHT die aktuelle Abfrage-UTC
    std::uint32_t maxCheckpointGapSeconds;              // 5.10a – abgeleitet aus PendingRecoveryAnchor.originalCheckpointIntervalMinutes, keine zusaetzliche Toleranz
    RunCheckpointTrigger lastCheckpointTrigger;         // fuer Konfidenzanzeige, keine Grenzenrolle (Ableitung siehe 5.10a)
};

struct RecoveryOutageBounds {
    std::uint64_t outageSecondsUpperBound;  // = utcAtRestartBoundary - utcAtLastCheckpoint (checked, s.u.)
    std::uint64_t outageSecondsLowerBound;  // = saturating_sub(upperBound, maxCheckpointGapSeconds, 0)
};

// nullopt genau dann, wenn ein UTC-Anker fehlt oder utcAtRestartBoundary <
// utcAtLastCheckpoint (Uhr ging zurueck – als unbekannt behandelt, nicht als
// negative Dauer). Keine erfundene Ausfalldauer ohne beide Anker.
[[nodiscard]] std::optional<RecoveryOutageBounds> computeRecoveryOutageBounds(
    const RecoveryOutageBoundsInput& input);
```

**Checked Arithmetic (Punkt 9 der Auftragsvorgabe):** Die Subtraktion
`utcAtRestartBoundary - utcAtLastCheckpoint` erfolgt in `std::int64_t`
(beide Operanden sind bereits vorzeichenbehaftete UTC-Sekunden) und wird vor
der Umwandlung nach `std::uint64_t` explizit auf `< 0` geprueft (-> `nullopt`,
keine impliziten Wrap-Around-Werte). `saturating_sub(upperBound,
maxCheckpointGapSeconds, 0)` ist eine eigene kleine Hilfsfunktion
(`a >= b ? a - b : 0`), keine rohe `unsigned`-Subtraktion. Testfaelle: exakt
gleiche Werte (Ergebnis 0), `maxCheckpointGapSeconds > upperBound`,
`std::uint64_t`-nahe Grenzwerte fuer `upperBound`.

Fehlt ein UTC-Anker: `computeRecoveryOutageBounds` liefert `nullopt`.

### 5.3 `RecoveredPhaseElapsed` – Phasenlaufzeit, strukturell getrennt (Vertrag B)

```cpp
struct RecoveredPhaseElapsedInput {
    std::uint64_t knownSecondsBeforeCheckpoint;  // bereits fertig berechnet uebergeben (5.10) – NIEMALS aus zwei rohen Boot-Millis-Werten an dieser Stelle neu subtrahiert
    std::optional<RecoveryOutageBounds> outage;  // aus 5.2, kann nullopt sein
};

struct RecoveredPhaseElapsed {
    std::uint64_t knownSecondsBeforeCheckpoint;          // Durchreichung des Inputs
    std::uint64_t totalSecondsLowerBound;                // = checked_add(knownSecondsBeforeCheckpoint, outage ? outage->outageSecondsLowerBound : 0)
    std::optional<std::uint64_t> totalSecondsUpperBound; // = checked_add(knownSecondsBeforeCheckpoint, outage->outageSecondsUpperBound), nur wenn outage vorhanden
};

[[nodiscard]] RecoveredPhaseElapsed computeRecoveredPhaseElapsed(
    const RecoveredPhaseElapsedInput& input);
```

**Verbindliche Eigenschaft:** Die Funktion nimmt
keine zwei Boot-Millis-Werte entgegen, aus denen sie selbst eine
Differenz bilden koennte. Damit ist es strukturell unmoeglich, dass diese
Funktion Werte aus zwei verschiedenen Boots voneinander subtrahiert – das war
der zentrale Fehlermodus, den ein Ausfallanker ueber mehrere Reboots hinweg
sonst reproduzieren wuerde (5.10). Die einzige Stelle, an der
`(oldCheckpointMonotonicMillis - oldStateEnteredAtMillis) / 1000` tatsaechlich
berechnet wird, ist die einmalige Konstruktion von
`PendingRecoveryAnchor.knownPhaseSecondsAtOriginalCheckpoint` bei Hop 1
(5.10) – dort sind beide Werte garantiert aus demselben (alten) Boot, weil
sie direkt aus dem soeben geladenen Datensatz stammen, bevor irgendein neuer
Boot-Wert einfliesst.

**Checked Arithmetic:** `checked_add` prueft vor der Addition
`std::numeric_limits<std::uint64_t>::max() - a < b` und liefert in diesem
(praktisch unerreichbaren) Fall keinen Wert zurueck, sondern erzwingt
`nullopt` fuer den betroffenen Grenzwert (Verhalten analog 5.19). Test:
beide Operanden nahe `UINT64_MAX`.

Keine Variable bedeutet gleichzeitig "Ausfalldauer" und "gesamte
Phasenlaufzeit": `RecoveryOutageBounds` beschreibt ausschliesslich die
Unterbrechung selbst (Vertrag A, kanonisch nach `RUN_PERSISTENCE.md`);
`RecoveredPhaseElapsed` beschreibt die daraus abgeleitete **gesamte**
Zeit seit Phaseneintritt (Vertrag B, `Alt-Boot-Anteil + Ausfallintervall`),
verwendet fuer die fachliche Grenzbewertung (5.4).

### 5.4 `evaluateRecoveryTimeVerdict` – reine Vergleichsfunktion

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

Anwendung je Phase: `WaitingForProduct` (`maximumProductWaitMinutes*60`,
steuert Hop-2-Ausgang), `Fermenting` (`fermentationDurationMinutes*60`, rein
informativ), `CoolHolding` mit `CompletionMode::CoolAndHoldForDuration`
(`holdDurationMinutes*60`, rein informativ); alle anderen recovery-faehigen
Phasen ohne snapshot-getragene Grenze.

**Konservativitaetsrichtung bei einer knapp bemessenen
`maxCheckpointGapSeconds` (5.10a):** Ein zu klein angesetzter
`maxCheckpointGapSeconds`-Wert vergroessert `outageSecondsLowerBound` und
damit `totalSecondsLowerBound` – das Ergebnis verschiebt sich hoechstens in
Richtung `DefinitelyExpired`, niemals in Richtung `DefinitelyStillValid`
(das haengt ausschliesslich von `totalSecondsUpperBound` ab, das von
`maxCheckpointGapSeconds` unberuehrt bleibt). Fuer die einzige Phase mit
automatischer Aktion (`WaitingForProduct`, Tombstone bei
`DefinitelyExpired`) bedeutet das: ein knapper Wert fuehrt hoechstens zu
einem frueheren Lauf-Abbruch ohne jede Aktorfreigabe – die sichere Seite,
kein stilles Laenger-als-tatsaechlich-Fortsetzen. Deshalb ist die in 5.10a
festgelegte, ausschliesslich aus vorhandenen Feldern abgeleitete
`maxCheckpointGapSeconds` ohne zusaetzliche Toleranz vertretbar.

### 5.5 Boot-unabhaengiger Phasenfortschritt – kein Rebasing-Unterlauf

`newBootMonotonicMillisNow - elapsedSeconds * 1000` unterlaeuft (unsigned),
wenn bereits mehr Phasenzeit bekannt ist, als der aktuelle Boot ueberhaupt
laeuft (z. B. zwei Stunden bekannte Fermenting-Zeit, Boot laeuft erst fuenf
Sekunden) – das Ergebnis waere ein riesiger Wert, den
`runtimeTimeIsValid()` als "in der Zukunft" ablehnt.

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
`RunCommandState`/`RunPersistenceSnapshot` (5.20, vollstaendiger
Persistenzvertrag). Der Aufrufer (Recovery-Orchestrierung bzw. der
automatische Live-Dispatch) liest ihn nur, wenn `current.processState.state`
mit dem Tag uebereinstimmt – sonst wird `{}` (kein Vor-Boot-Anteil)
verwendet. Keine Boot-Epochen-Fiktion; keine Subtraktion von `now`.

### 5.6 Vollstaendige phasenbezogene Timertabelle fuer Hop 2

| Phase | `stateEnteredAtMillis` | `targetReachStartedAtMillis` | `targetReachWarningIssued` | `qualificationValidSinceMillis` | boot-unabhaengiger Anteil |
|---|---|---|---|---|---|
| `Preheating` | `now` | `0` (kein Timer) | `false` | `nullopt` (bestehende Logik, `process_state_machine.cpp:771-772`) | keiner |
| `WaitingForProduct` | `now` | `0` | `false` | n/a | `priorElapsed` (Ober- oder Untergrenze, siehe 5.8) |
| `ReachingTarget` | `now` | `now` (Timer bewusst neu gestartet – konservativ, verzoegert hoechstens eine Warnung) | `false` | n/a | keiner |
| `QualifyingTarget` | wird von `decideRecoveryEvent` selbst zu `ReachingTarget` umgeleitet, `stateEnteredAtMillis = now` (bestehende Logik, `process_state_machine.cpp:773-777`) | `now` | `false` | `nullopt` | keiner – beginnt vollstaendig neu |
| `Fermenting` | `now` | `0` | `false` | n/a | `priorElapsed` (Untergrenze) + separat `observedRunSeconds` (5.19, Geschaeftsmetrik, getrennt von der reinen Grenzbewertung) |
| `Cooling` | `now` | `0` | `false` | n/a | keiner (kein Dauer-Timer, signalbasiert) |
| `CoolHolding` | `now` | `0` | `false` | n/a | `priorElapsed` (Untergrenze) |
| `ManualHolding` | `now` | `0` | `false` | n/a | keiner (kein automatisches Limit) |

Kein Feld wird roh aus dem alten Boot uebernommen; jedes Feld ist entweder
explizit auf `now`/`0`/`nullopt` gesetzt oder ueber `priorElapsed` separat
gefuehrt.

### 5.7 Hop 1 – Recovery-Eintrittsvertrag

`TransitionReason::RecoveryReentryRequired`, `validControlTopology`-Zweig
`stateUsesRunSnapshot(from) && to == RecoveryEvaluation`
(`stateUsesRunSnapshot`, `process_state_machine.cpp:76-97`: `Preheating`,
`WaitingForProduct`, `ReachingTarget`, `QualifyingTarget`, `Fermenting`,
`Cooling`, `CoolHolding`, `ManualHolding` – **nicht** `Completed`, dazu
5.21). Ablauf: `candidate = restoredState`; `originalRestoredProcessState`
als unveraenderte Kopie fuer den spaeteren Recovery-Kontext (5.10);
`hop1 = propose(candidate.processState, RecoveryEvaluation,
RecoveryReentryRequired, monotonicMillis)`; `applyProcessTransition(candidate.processState,
hop1, &*candidate.processRunSnapshot)`. `ProcessSignals::criticalFault =
false`. Bei Fehlschlag: kein Schreiben, Coordinator bleibt im
Ausgangszustand, `InvalidDecision`.

**Zusaetzlich, vor dem Commit dieses Hop 1 (5.10):** aus dem soeben geladenen
Vor-Ausfall-Datensatz (`RunPersistenceRawRecord`, der Datensatz, den
`loadAndInitialize()` als `LoadedActiveRun` geliefert hat) wird
`PendingRecoveryAnchor` einmalig konstruiert und
`candidate.pendingRecoveryAnchor` gesetzt; `candidate.recoveryBootAnchorMonotonicMillis
= monotonicMillis` wird ebenfalls gesetzt (5.10).

### 5.8 Hop 2 – Wiederverwendung der bestehenden `decideRecoveryEvent`-API

`request.recoveredState` wird aus `pendingRecoveryAnchor.originalProcessState`
gemaess 5.6 aufgebaut (alle Felder explizit gesetzt, kein roher Altwert).
Aufruf:

```cpp
TransitionRequest request;
request.event = ProcessEvent::RecoveryResume;  // oder RecoveryReject, siehe 5.23
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
  Versuch (5.9/5.14).

Liefert `hop2.status != Proposed` (z. B. weil das eingebaute
`WaitingForProduct`-Sicherheitsnetz trotz eigener Vorpruefung ablehnt), wird
kein Resume erzwungen; Rueckfall auf Hop-1-only (5.14).

**Gate-A-Kopplung:** Zwischen Hop 1 und Hop 2 wird die reale
Restart-Sensorauswahl (5.23) ausgewertet; negatives Ergebnis ->
`request.event = ProcessEvent::RecoveryReject` (bestehende, bereits
implementierte `RecoveryEvaluation -> Fault`-Logik).

### 5.9 WaitingForProduct: definitiv abgelaufene Wartezeit – Tombstone

`TransitionReason::RecoveryEndedByExpiredWait`, `validControlTopology`-Zweig
`from == RecoveryEvaluation && to == Standby`, direkt ueber `propose()`
konstruiert (keine wiederverwendbare bestehende Funktion dafuer vorhanden),
`clearActiveRunState(candidate)` vor der Persistierung (dieser Helfer setzt
zusaetzlich `pendingRecoveryAnchor`, `recoveryBootAnchorMonotonicMillis`,
`lastRecoveryEpisodeEvidence` (5.17) und `priorBootPhaseElapsed` (5.20) auf
`nullopt`), Coordinator erreicht `ReadyEmpty`.

### 5.10 `PendingRecoveryAnchor` – unveraenderlicher Ursprungsanker ueber Hop-1-Commit und mehrere Reboots

**Ausgangsproblem:** Ein einzelnes `pendingRecoveryOriginalState` (nur der
restaurierte `ProcessRuntimeState`) reicht nach dem ersten Hop-1-Commit nicht
mehr aus: der neue Current-Snapshot traegt `checkpointMonotonicMillis` des
Recovery-Boots, waehrend die urspruengliche Vor-Ausfall-Phasenzeit aus dem
urspruenglichen Boot stammt. Wuerde `computeRecoveredPhaseElapsed` beide
Werte direkt gegeneinander verrechnen, entstuende eine Boot-uebergreifende
Subtraktion. Ebenso darf die urspruengliche `utcAtLastCheckpoint` nicht still
durch die UTC eines spaeteren Hop-1-/Episode-Refresh-Commits ersetzt werden,
sonst berechnet eine spaetere UTC-Reevaluation nicht mehr das Intervall des
urspruenglichen Stromausfalls, sondern nur noch die Zeit seit dem letzten
Reboot.

**Struktur, Schema 3, Teil von `RunPersistenceSnapshot`:**

```cpp
// run_persistence_contract.hpp
struct PendingRecoveryAnchor {
    ProcessRuntimeState originalProcessState;             // 5.7 originalRestoredProcessState – rein strukturell (Phasen-/Formmatching), niemals fuer Boot-uebergreifende Arithmetik verwendet
    std::uint64_t knownPhaseSecondsAtOriginalCheckpoint;   // = (oldCheckpointMonotonicMillis - oldStateEnteredAtMillis) / 1000, EINMALIG bei Hop 1 berechnet, danach nur noch gelesen
    std::optional<std::int64_t> originalCheckpointUtc;     // RunPersistenceRawRecord.utcUnixSeconds DES VOR-AUSFALL-DATENSATZES, eingefroren bei Hop 1
    RunCheckpointTrigger originalCheckpointTrigger;        // Trigger desselben Vor-Ausfall-Datensatzes (Konfidenzanzeige, 5.10a)
    std::uint32_t originalCheckpointIntervalMinutes;       // intervalMinutes desselben Vor-Ausfall-Datensatzes (5.10a) – NICHT die evtl. spaeter geaenderte laufende Konfiguration
};
```

Zusaetzlich, **ausserhalb** dieses Ankers, ein zweites, bewusst getrenntes
Schema-3-Feld auf `RunPersistenceSnapshot`:

```cpp
std::optional<std::uint64_t> recoveryBootAnchorMonotonicMillis;
// Monotonic-Zeitpunkt DES AKTUELLEN BOOTS, an dem diese Recovery-Bewertung
// begann (bei Hop 1: der Hop-1-Zeitpunkt selbst; bei jedem Episode-Refresh,
// 5.12: neu auf den Episode-Refresh-Zeitpunkt DIESES Boots gesetzt). Lebt
// bewusst nicht im Anker: der Anker beschreibt ausschliesslich den
// urspruenglichen Vor-Ausfall-Kontext und darf durch keinen spaeteren Reboot
// veraendert werden; dieses Feld beschreibt dagegen exakt das Gegenteil –
// den je-Boot-lokalen Bezugspunkt – und MUSS bei jedem neuen Boot neu
// gesetzt werden, weil ein alter Boot-Monotonic-Wert in einem neuen Boot
// bedeutungslos ist.
```

**Ableitung des korrekten "aktuelle UTC"-Werts fuer 5.2 (reine Funktion,
ausschliesslich Boot-lokale Werte, run_recovery_time.hpp):**

```cpp
[[nodiscard]] std::optional<std::int64_t> deriveUtcAtRecoveryBootAnchor(
    std::int64_t utcNow, std::uint64_t nowMonotonicMillis,
    std::uint64_t recoveryBootAnchorMonotonicMillis) {
    if (nowMonotonicMillis < recoveryBootAnchorMonotonicMillis) return std::nullopt;  // niemals in diesem Boot, Vorbedingung verletzt
    const auto elapsedSeconds =
        (nowMonotonicMillis - recoveryBootAnchorMonotonicMillis) / 1000U;
    // checked: utcNow - elapsedSeconds duerfte in der Praxis nie unterlaufen
    // (elapsedSeconds ist die Boot-Laufzeit seit dem Recovery-Eintritt,
    // typischerweise Sekunden bis wenige Stunden); dennoch explizit geprueft.
    if (static_cast<std::uint64_t>(utcNow) < elapsedSeconds) return std::nullopt;
    return utcNow - static_cast<std::int64_t>(elapsedSeconds);
}
```

Beide Eingaben (`nowMonotonicMillis`, `recoveryBootAnchorMonotonicMillis`)
stammen garantiert aus demselben Boot (der Aufrufer liest
`recoveryBootAnchorMonotonicMillis` aus dem gerade aktiven, in diesem Boot
geladenen `RunCommandState`) – die Subtraktion ist damit immer boot-lokal,
niemals boot-uebergreifend. Das Ergebnis ersetzt in 5.2
`RecoveryOutageBoundsInput.utcAtRestartBoundary`, niemals eine roh zum
Abfragezeitpunkt gelesene `utcNow`.

**Verbindlicher Vertrag:**

- Bei Hop 1 (5.7) wird `PendingRecoveryAnchor` **einmalig** aus dem soeben
  geladenen Vor-Ausfall-Datensatz konstruiert:
  `knownPhaseSecondsAtOriginalCheckpoint = (loadedRecord.snapshot.checkpointMonotonicMillis
  - originalRestoredProcessState.stateEnteredAtMillis) / 1000`,
  `originalCheckpointUtc = loadedRecord.utcUnixSeconds`,
  `originalCheckpointTrigger = loadedRecord.snapshot.trigger`,
  `originalCheckpointIntervalMinutes = loadedRecord.snapshot.intervalMinutes`.
  `recoveryBootAnchorMonotonicMillis = monotonicMillis` (der Hop-1-Zeitpunkt).
- Episode-Refresh (5.12) uebernimmt `pendingRecoveryAnchor` **byte-identisch**
  unveraendert, setzt aber `recoveryBootAnchorMonotonicMillis` **neu** auf den
  Episode-Refresh-Zeitpunkt **dieses** Boots.
- Automatische Reevaluation (`reevaluatePendingRecovery`) und der
  Benutzerpfad (`ResolveRecoveryUncertainty`, 5.14) lesen **ausschliesslich**
  `pendingRecoveryAnchor` und `recoveryBootAnchorMonotonicMillis` aus dem
  aktuell geladenen `RunCommandState` – niemals aus dem zuletzt physisch
  geschriebenen Datensatz direkt (dessen `utcUnixSeconds`/
  `checkpointMonotonicMillis` gehoeren zum jeweils letzten Commit, nicht zum
  urspruenglichen Ausfall).
- Ein Hop-1-/Episode-Refresh-Checkpoint ersetzt den Anker selbst nicht;
  lediglich das separate `recoveryBootAnchorMonotonicMillis` wird bei jedem
  Episode-Refresh und beim initialen Hop 1 neu gesetzt (s. o.).
- Ein zweiter/dritter Reboot waehrend `RecoveryEvaluation` bleibt korrekt
  berechenbar, weil `pendingRecoveryAnchor` als Teil jedes persistierten
  Snapshots unveraendert mitgefuehrt wird und `recoveryBootAnchorMonotonicMillis`
  bei jedem dieser Reboots ueber Episode-Refresh neu und korrekt (boot-lokal)
  gesetzt wird.
- Nach erfolgreichem Resume/Tombstone/Reject (5.14) werden
  `pendingRecoveryAnchor` und `recoveryBootAnchorMonotonicMillis` atomar im
  selben Commit auf `nullopt` gesetzt.

**Tests:**
- Hop1-only -> Commit -> UTC wird spaeter im selben Boot verfuegbar:
  `deriveUtcAtRecoveryBootAnchor` liefert die UTC am Hop-1-Zeitpunkt, nicht
  die spaetere Abfrage-UTC; `computeRecoveryOutageBounds` bleibt gegenueber
  dem tatsaechlichen Ausfall korrekt (schliesst die seit Hop 1 vergangene
  Laufzeit aus).
- Hop1-only -> Reboot -> UTC wird im zweiten Boot verfuegbar:
  `recoveryBootAnchorMonotonicMillis` wurde beim Episode-Refresh auf den
  zweiten Boot neu gesetzt; dieselbe Ableitung bleibt korrekt.
- Hop1-only -> mehrere Reboots -> `pendingRecoveryAnchor` bleibt
  byte-identisch, nur `recoveryBootAnchorMonotonicMillis` aendert sich je
  Boot.
- Niemals Subtraktion von Monotonic-Werten verschiedener Boots (Negativtest:
  ein absichtlich aus einem anderen "Boot" stammender
  `recoveryBootAnchorMonotonicMillis`-Wert wird von
  `deriveUtcAtRecoveryBootAnchor` nicht plausibilisiert – die Funktion selbst
  kann das nicht erkennen, daher liegt die Garantie beim Aufrufer, der
  ausschliesslich den im aktuellen Boot geladenen Wert verwendet; Testabdeckung
  auf Ebene der Orchestrierung, nicht der reinen Funktion).
- Spaetere UTC-Reevaluation verwendet **nicht** den Hop-1-/Episode-Refresh-
  Commit als urspruenglichen Ausfallanker (Regressionstest gegen genau den in
  dieser Revision behobenen Fehler).

#### 5.10a `maxCheckpointGapSeconds` – ausschliesslich aus vorhandenen Daten

**Vertrag:** `maxCheckpointGapSeconds =
anchor.originalCheckpointIntervalMinutes * 60`. Keine zusaetzliche
firmwarefeste Toleranz.

**Begruendung, warum das belastbar ist:** `RunCheckpointSchedule::due()`
(`run_checkpoint_schedule.cpp:33-51`) laesst einen periodischen Checkpoint
erst zu, wenn seit dem zuletzt **beliebigen** bestaetigten Checkpoint
(`confirm()`, aufgerufen nach jedem erfolgreichen Schreiben, gleich welchen
Triggers – `run_persistence_coordinator.cpp:516`) mindestens
`intervalMinutes` vergangen sind. Ein Command-/Transition-/
SensorSelection-Checkpoint verschiebt den naechsten faelligen periodischen
Checkpoint deshalb genauso wie ein periodischer selbst – das bestehende
`RunCheckpointTrigger`-Feld hat damit auf die GROESSE der maximalen Luecke
keinen Einfluss (bleibt reine Konfidenzanzeige); es liefert keinen
zusaetzlichen, engeren Bound. Die einzige
zusaetzliche, nicht von diesem Modul beweisbare Annahme ist, dass der
periodische Aufrufer (ausserhalb dieses Moduls, bestehende Voraussetzung
bereits vor #18) tatsaechlich mindestens einmal pro `intervalMinutes`
aufruft, waehrend das Geraet laeuft und `state_ == Ready` ist – das ist keine
von #18 neu erfundene Zahl, sondern die bereits bestehende Bedeutung des
konfigurierten Checkpoint-Intervalls selbst.

`originalCheckpointIntervalMinutes` (nicht die aktuell laufende Konfiguration)
wird verwendet, weil sich das konfigurierte Intervall zwischen dem
Vor-Ausfall-Boot und dem Recovery-Boot geaendert haben kann (z. B. durch eine
Einstellungsaenderung nach einem Firmwareupdate) – die Luecke vor dem
Ausfall wurde aber unter dem damals geltenden Intervall erzeugt.

Konservativitaetsrichtung bei fehlender zusaetzlicher Belegbarkeit: siehe
5.4.

### 5.11 Schema-3-Gueltigkeitsvertrag fuer `RecoveryEvaluation` bei aktivem Run

`validStateFor()` (`run_persistence_contract.cpp:10-42`) erlaubt
`RecoveryEvaluation` fuer `RunCheckpointVariant::ProgramRun`/`ManualRun`
**nicht** (weder in der `ProgramRun`- noch der `ManualRun`-Fallliste).
`makeRunPersistenceSnapshot()` wuerde einen Hop-1-only-Kandidaten (5.14)
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
   - `snapshot.pendingRecoveryAnchor.has_value()` (5.10, in diesem Fall
     verpflichtend) **und** `snapshot.recoveryBootAnchorMonotonicMillis.has_value()`;
   - `pendingRecoveryAnchor->originalProcessState.state !=
     ProcessState::RecoveryEvaluation` (die urspruengliche Phase ist eine
     echte Altphase, keine Selbstreferenz);
   - `stateMatchesRunSnapshot(pendingRecoveryAnchor->originalProcessState.state,
     *snapshot.processRunSnapshot)` (die urspruengliche Phase passt zum
     mitgelieferten Programm-/manuellen Schnappschuss).

   Fehlt eine dieser Bedingungen, ist der Snapshot ungueltig – kein aktiver
   Run darf ausserhalb dieses eng begrenzten Pending-Recovery-Falls in
   `RecoveryEvaluation` stehen.
3. **Zusaetzliche Invariante fuer `priorBootPhaseElapsed` (5.20):** ist
   `snapshot.priorBootPhaseElapsed.has_value()`, muss
   `priorBootPhaseElapsed->taggedState == snapshot.processState.state`
   gelten – sonst ist der Snapshot ungueltig. Ein getaggter Vor-Boot-Anteil,
   der nicht zur aktuellen Phase passt, darf nie persistiert werden (siehe
   5.20 fuer die zusaetzliche, defensive Lesersicherung).
4. **Keine stille Liberalisierung von Schema 1/2:** ein nach altem Schema
   (vor #18) geschriebener Datensatz konnte `RecoveryEvaluation` fuer einen
   aktiven Run strukturell nie enthalten (die alte `validStateFor()`-Liste
   liess das nicht zu) – die Erweiterung macht eine vorher **immer**
   ungueltige Kombination unter den engen, oben genannten zusaetzlichen
   Bedingungen gueltig; bereits existierende Alt-Daten sind von dieser
   Erweiterung nicht betroffen, da sie diese Kombination nie erzeugen
   konnten. Kein expliziter Schema-Versionscheck an dieser Stelle noetig.
5. **`NoActiveRun` traegt keine Recovery-Diagnosedaten eines beendeten
   Laufs:** ist `snapshot.variant == NoActiveRun`, muessen
   `pendingRecoveryAnchor`, `recoveryBootAnchorMonotonicMillis`,
   `lastRecoveryEpisodeEvidence` (5.17) und `priorBootPhaseElapsed` (5.20)
   alle `nullopt` sein, sonst ist der Snapshot ungueltig – konsistent mit
   `clearActiveRunState()` (5.9), das alle vier Felder im selben Commit
   loescht.

Contract-/Codec-Tests: aktiver Schema-3-Snapshot mit `RecoveryEvaluation`
und vollstaendigem, konsistentem Pending-Kontext -> gueltig; ohne
Pending-Kontext -> ungueltig; mit inkonsistentem `pendingRecoveryAnchor`
(falsche Phase fuer den Snapshot) -> ungueltig; mit `priorBootPhaseElapsed`,
dessen Tag nicht zur aktuellen Phase passt -> ungueltig; `NoActiveRun` mit
noch gesetztem `pendingRecoveryAnchor`/`lastRecoveryEpisodeEvidence` ->
ungueltig; Schema-1/2-
Decodierung kann diese Kombinationen gar nicht erzeugen (Regressionstest
gegen bestehende Migrationsvektoren).

### 5.12 Reboot waehrend bereits persistiertem `RecoveryEvaluation` (Episode-Refresh)

`RecoveryReentryRequired`s Topologie erlaubt als Quelle ausschliesslich
`stateUsesRunSnapshot(from)`-Phasen; `RecoveryEvaluation` selbst gehoert
nicht dazu (`stateUsesRunSnapshot(RecoveryEvaluation) == false`). Ein
zweiter Reboot waehrend einer noch offenen Hop-1-only-Recovery (5.14) laedt
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
`RecoveryEvaluation` vorliegt (Voraussetzung: `pendingRecoveryAnchor` ist
vorhanden, sonst `NotReconstructible` – ein aktiver Snapshot in
`RecoveryEvaluation` ohne Pending-Kontext ist gemaess 5.11 strukturell
ungueltig und waere schon beim Laden abgelehnt worden).

**Episode-Refresh statt Transition:** Da kein Zustandsuebergang stattfindet,
aber die Zeitbasis (UTC-jetzt, ggf. zwischenzeitlich neuere periodische
Checkpoints) sich seit dem letzten Boot geaendert haben kann, fuehrt
`activate()` in diesem Fall einen eigenen, einfacheren Commit aus:
`candidate = current` (keine Transition), `candidate.recoveryEpisodeRevision
+= 1U`, `candidate.pendingRecoveryAnchor` bleibt **byte-identisch**
unveraendert (5.10), `candidate.recoveryBootAnchorMonotonicMillis =
monotonicMillis` (dieser Boot, 5.10). Geschrieben ueber denselben
Commit-Kern (5.13), mit `rollbackState == LoadedActiveRun` (5.13).

**Latch-Reset pro Sensorrolle (5.17):** Bei jedem Anstieg von
`recoveryEpisodeRevision` – ob durch den initialen Hop 1 oder durch ein
Episode-Refresh – werden die drei `firstAfterRestart`-Latches
(`lastRecoveryEpisodeEvidence.firstAfterRestart.air/product/cooling`) auf
`nullopt` zurueckgesetzt, **bevor** derselbe Commit versucht, sie aus dem
bei dieser Gelegenheit ohnehin verfuegbaren `CrossRolePlausibilityContext`
(Gate A, 5.23) sofort neu zu befuellen (5.17). `beforeOutage` bleibt davon
unberuehrt und wird ausschliesslich beim initialen Hop 1 einer
Recoveryepisode-Kette eingefroren (5.17).

Dadurch: **jeder** Reboot, der eine Recovery-Bewertung neu beginnt – ob via
echtem Hop 1 oder via Episode-Refresh –, erhoeht `recoveryEpisodeRevision`
genau einmal; ein zwischen zwei Boots eingereichter, jetzt veralteter
Korrektur-/Entscheidungsversuch wird ueber den bestehenden
`expectedRecoveryEpisodeRevision`-Abgleich (5.19) als `StaleState` erkannt.

`state_` bleibt `LoadedActiveRun` bis zum erfolgreichen Episode-Refresh-
Commit und geht danach auf `Ready` ueber (identisches Ergebnis zur
regulaeren Hop-1-only-Situation, 5.14); `RecoveryEvaluation` traegt
weiterhin keine Aktorfreigabe; die Aufloesungswege (automatisch/Benutzer,
5.14) bleiben unveraendert erreichbar, jetzt gegen die aufgefrischte
Episode.

Test: Reboot -> Hop-1-only -> erneuter Reboot -> `recoveryEpisodeRevision`
erhoeht sich erneut, `pendingRecoveryAnchor` bleibt identisch,
`recoveryBootAnchorMonotonicMillis` wird neu gesetzt, `firstAfterRestart`
wird zurueckgesetzt und ggf. sofort neu befuellt, beide Aufloesungswege
bleiben funktionsfaehig.

### 5.13 Gemeinsamer Commit-Kern – Rollbackzustand statt implizitem Guard

`writeSnapshot()` (`run_persistence_coordinator.cpp:287-...`) akzeptiert
heute ausschliesslich `state_ == Ready || state_ == ReadyEmpty` (Zeilen
292-294) und stellt bei jedem Vor-Commit-Fehler exakt diesen aus
`currentHead_.has_value()` neu berechneten Ausgangszustand wieder her
(`readyState()`-Lambda, Zeilen 419-423, sowie vier strukturell identische
Inline-Stellen bei Codec-Fehlern: Zeilen 312-314, 326-328, 350-352,
383-385). Wird derselbe Kern nun auch aus `LoadedActiveRun` bzw. dem neuen
`FallbackRecoveryPending`-Zustand (5.15) aufgerufen, waere eine pauschale
Rueckkehr zu einem aus `currentHead_.has_value()` berechneten `Ready`/
`ReadyEmpty` sicherheitskritisch falsch: ein Codec-/Capacity-/
NotWritten-Fehler vor sicherem Commit koennte den Coordinator auf `Ready`
setzen, obwohl die Recovery nie erfolgreich persistiert wurde und
`RunCommandState& current` im RAM weiterhin den unbewerteten Alt-Zustand
traegt.

**Vertrag:** Der bestehende Koerper von `writeSnapshot()` wird in eine
private, guard-lose Funktion extrahiert, die den erlaubten
Rueckfallzustand explizit als Parameter erhaelt statt ihn selbst zu
erraten:

```cpp
// privat – KEINE state_-Vorbedingung, der Aufrufer liefert stattdessen den
// exakten Zustand, in den bei einem Vor-Commit-Fehler zurueckgekehrt wird.
RunPersistenceResult RunPersistenceCoordinator::writeSnapshotCore(
    const RunPersistenceSnapshot& snapshot, const RunCheckpointTime& time,
    bool periodic, const RunCommandState& before,
    RunPersistenceMutationKind mutationKind, std::optional<CommandId> commandId,
    std::optional<std::size_t> targetSlotOverride,
    std::optional<RunCheckpointReference> fallbackOverride,  // 5.15
    RunPersistenceCoordinatorState rollbackState);

RunPersistenceResult RunPersistenceCoordinator::writeSnapshot(
    const RunPersistenceSnapshot& snapshot, const RunCheckpointTime& time,
    bool periodic, const RunCommandState& before,
    RunPersistenceMutationKind mutationKind, std::optional<CommandId> commandId) {
    if (state_ != RunPersistenceCoordinatorState::Ready &&
        state_ != RunPersistenceCoordinatorState::ReadyEmpty) {
        return unavailableResult();
    }
    const auto rollbackState = state_;  // exakt Ready oder ReadyEmpty, nicht neu berechnet
    return writeSnapshotCore(snapshot, time, periodic, before, mutationKind,
                             commandId, /*targetSlotOverride=*/std::nullopt,
                             /*fallbackOverride=*/std::nullopt, rollbackState);
}
```

`rollbackState` ist auf genau vier Werte beschraenkt (durch die beiden
einzigen Aufrufer erzwungen, nicht durch eine zusaetzliche Laufzeitpruefung):
`Ready`, `ReadyEmpty` (Standardpfad, s. o.), `LoadedActiveRun`
(Hop 1/Episode-Refresh/Tombstone/Resume-Vorbereitung aus einem geladenen,
noch nicht evaluierten Run) und `FallbackRecoveryPending` (5.15,
ausschliesslich fuer den Fallback-Recovery-Sonderfall).

**Ersetzungsregel innerhalb von `writeSnapshotCore`:** Jede der sieben
Stellen, an denen der heutige Code bei einem Fehler **vor** vollstaendigem
Commit (`durability == Unchanged` bzw. ein durch einen fehlgeschlagenen,
noch nicht vom Head referenzierten Slot-Schreibversuch verursachtes
`Changed`, das den Head nicht veraendert hat) den Coordinator wieder
"betriebsbereit" macht, verwendet **ausschliesslich** `state_ =
rollbackState` statt einer Neuberechnung aus `currentHead_.has_value()`:
Snapshot-Encode-Fehler, Envelope-Encode-Fehler (Ziel), Committed-Head-Encode
-Fehler (periodisch), Prepared-/Committed-Head-Encode-Fehler
(nicht-periodisch), periodischer Slot-Schreibfehler (nicht `Indeterminate`),
periodischer Head-Schreibfehler nach erfolgreichem Slot-Schreiben (nicht
`Indeterminate`), Prepared-Head-Schreibfehler (nicht-periodisch, nicht
`Indeterminate`). Fuer den bestehenden Standardpfad (`rollbackState` ==
exakt der Zustand, den `currentHead_.has_value()` ohnehin ergeben haette)
ist das Ergebnis **byte-identisch** zum heutigen Verhalten.

**Unveraendert bleiben, ausdruecklich ohne `rollbackState`-Bezug (keine
dieser Stellen wird durch diese Revision veraendert):**

- Der Lesefehler auf `physicalTarget` vor jedem Schreibversuch
  (Zeilen 403-417) geht bereits heute **unbedingt** in
  `BlockedIndeterminate` – unabhaengig vom Aufrufer korrekt, da ein
  unklarer physischer Lesezustand maximal fail-closed behandelt werden muss.
- Ein Slot-Schreibfehler nach bereits erfolgreich geschriebenem Prepared-
  Head (nicht-periodisch, Zeilen 485-489) sowie ein Head-Commit-Fehler nach
  bereits erfolgreich geschriebenem Slot (Zeilen 499-503) gehen bereits
  heute **unbedingt** in `BlockedIndeterminate` (ein inkonsistenter,
  bereits committeter `Prepared`-Head verlangt beim naechsten Boot den
  bestehenden generischen `PreparedInterrupted`-Mechanismus, unabhaengig
  vom urspruenglichen Aufrufer – keine recovery-spezifische
  Sonderbehandlung noetig, dieser Cutpoint war bereits vor #18 fail-closed
  abgedeckt).
- Jeder `RunPersistenceStoreWriteResult::Indeterminate`-Fall geht weiterhin
  unbedingt in `BlockedIndeterminate` (`writeFailure`-Lambda), unabhaengig
  von `rollbackState` – ein physisch unklarer Schreibausgang ist immer
  maximal konservativ zu behandeln.
- Der Schedule-Bestaetigungsfehler nach vollstaendigem Commit (Zeilen
  516-523) geht weiterhin unbedingt in `BlockedIndeterminate`.
- `CounterOverflow` und `TimeWentBackwards`/`InvalidDecision` aus der
  Schedule-`validate()`-Pruefung (Zeilen 295-305) treten **vor** dem Setzen
  von `state_ = Busy` auf; `state_` ist zu diesem Zeitpunkt bereits der
  spaetere `rollbackState`-Wert und braucht keine explizite Wiederherstellung.
- Ein bestaetigt durabler Commit mit anschliessendem RAM-Apply-Fehler auf
  dem echten `RunCommandState& current` fuehrt weiterhin zu
  `PersistenceCommittedApplyFailed` – das setzt **nicht**
  `writeSnapshotCore` selbst, sondern (wie heute bei `persistCommand`/
  `persistTransition`) der jeweilige Aufrufer nach einem `Applied`/
  `CheckpointWritten`-Ergebnis, wenn sein eigener zweiter Anwendungsschritt
  auf `current` fehlschlaegt. Der Recovery-Commit-Kern
  `commitRecoveryOutcome(...)` (5.7-5.9, 5.14, 5.15) folgt demselben Muster.

Alle vier bestehenden oeffentlichen Standardpfade (`persistCommand`,
`persistTransition`, `persistSensorSelection`, `checkpointPeriodic`) rufen
weiterhin ausschliesslich das unveraenderte `writeSnapshot()` mit seinem
bestehenden Guard auf – **kein** Verhaltensunterschied fuer diese Pfade.

Der Recovery-Commit-Kern `commitRecoveryOutcome(...)` ruft **ausschliesslich**
`writeSnapshotCore()` direkt auf, mit einer eigenen, engen Vorbedingung:
`state_ == LoadedActiveRun || state_ == FallbackRecoveryPending` (5.15). Kein
temporaeres Umsetzen von `state_` auf `Ready` vor dem Commit. Write-before-
apply sowie alle bestehenden Prepared-/Slot-/Head-Fehlersemantiken bleiben
identisch, da `writeSnapshotCore` exakt derselbe Code ist wie zuvor
`writeSnapshot`, nur mit `rollbackState` statt einer Neuberechnung.

`resolveRecoveryOutcome`/`ResolveRecoveryUncertainty` (5.14, Hop-2-only,
nur relevant wenn `current.processState.state == RecoveryEvaluation` und
`state_` bereits `Ready` ist) verwenden weiterhin das **normale**,
guard-behaftete `writeSnapshot()` – sie benoetigen den Bypass nicht, da
`state_` zu diesem Zeitpunkt bereits `Ready` ist.

**Tests fuer jeden relevanten Cutpoint**, je aus `LoadedActiveRun`,
`FallbackRecoveryPending` und regulaerem `Ready`: Codec-/Capacity-/
NotWritten-Fehler vor Commit stellen exakt `rollbackState` wieder her (nicht
ein aus `currentHead_` neu berechnetes `Ready`/`ReadyEmpty`); ein
`Indeterminate`-Ausgang fuehrt unabhaengig vom Aufrufer zu
`BlockedIndeterminate`; ein bestaetigt durabler Commit mit RAM-Apply-Fehler
fuehrt zu `PersistenceCommittedApplyFailed`; Standardpfade (`persistCommand`
usw.) zeigen exakt das heutige Verhalten (Regressionstest, Ergebnis
byte-identisch zu bestehenden Tests).

### 5.14 Unsichere Recovery – Hop-1-only, zwei gleichwertige Aufloesungswege

Bei `RecoveryTimeVerdict::Uncertain` (5.4, nur `WaitingForProduct`) oder
fehlendem Gate-A-Ergebnis ohne sichere `RecoveryReject`-Anzeige wird nur
Hop 1 committet (`commitRecoveryOutcome`, 5.13, `rollbackState ==
LoadedActiveRun` bzw. `FallbackRecoveryPending`, 5.15).
`pendingRecoveryAnchor` und `recoveryBootAnchorMonotonicMillis` werden dabei
gesetzt (5.10). Coordinator-Ergebnis: `Ready`.

**Zwei gleichwertige Aufloesungswege**, beide ueber denselben Commit-Kern:

1. **Automatisch:** `RunRecoveryCoordinator::reevaluatePendingRecovery(RunCommandState&,
   const RunCheckpointTime&)` (nativ testbar; produktive Verdrahtung eines
   Aufrufers ist nicht Teil von #18, 5.24/Gate B). Wiederholt 5.2-5.8 gegen
   `current.processState.state == RecoveryEvaluation` und
   `pendingRecoveryAnchor`. **Erzeugt ausschliesslich** aktualisierte
   `RecoveryOutageBounds`/`RecoveredPhaseElapsed`-Werte sowie – nur fuer
   `WaitingForProduct` – ggf. eine automatische Tombstone-/Resume-Aufloesung
   ueber denselben Mechanismus wie 5.9/5.8, wenn Unter- und Obergrenze
   dieselbe fachliche Entscheidung ergeben (`DefinitelyExpired`/
   `DefinitelyStillValid`). Schreibt **niemals**
   `RunCommandState.runProgress.observedRunSeconds` und **niemals**
   `RecoveryTimeCorrectionRecord.appliedSecondsDelta` (5.19, Gate C) – eine
   praezisere UTC-Grenze beweist Zeit, aber keine biologische Aktivitaet.
2. **Benutzerpfad:** `CommandKind::ResolveRecoveryUncertainty`
   (`AssumeStillValid`/`AssumeThresholdCrossed`), ueber die bestehende
   `persistCommand`-Infrastruktur (`expectedRunRevision`,
   `expectedRecoveryEpisodeRevision`, 5.19; `persistedIds_`-Dedup,
   `StaleState`/`AlreadyProcessed`). Nur zulaessig innerhalb der
   ausgewiesenen Unsicherheit. `AcknowledgeMessage` bleibt reine
   Quittierung, nicht zweckentfremdet.

Bei erfolgreicher Aufloesung werden `pendingRecoveryAnchor` und
`recoveryBootAnchorMonotonicMillis` auf `nullopt` gesetzt (Pending-Kontext
beendet); Ergebnis-`state_` gemaess 5.9 (`ReadyEmpty`) oder unveraendert
`Ready` (Resume/Reject).

### 5.15 `FallbackRecovered`/`FallbackRecoveryPending` – kein Dead-End, echter gueltiger Fallback nach Commit

**Typisierung (vormals implizit ueber `BlockedIndeterminate` mit
verstecktem Grund):** `loadAndInitialize()` setzt beim erfolgreichen Laden
des Fallback-Datensatzes (`run_persistence_coordinator.cpp:264-274`, dort
heute `enterBlockedIndeterminate()`) stattdessen den neuen, eigenstaendigen
Zustand `RunPersistenceCoordinatorState::FallbackRecoveryPending`:

```cpp
enum class RunPersistenceCoordinatorState : std::uint8_t {
    Uninitialized, ReadyEmpty, LoadedActiveRun, Ready, Busy,
    BlockedIndeterminate, FallbackRecoveryPending,  // neu
    PersistenceCommittedApplyFailed,
};
```

Das ist eine **beobachtbare Vertragsaenderung**: der bestehende Test
`test_run_persistence_coordinator.cpp:1454-1456` erwartet nach einem
`FallbackRecovered`-Load `state() == BlockedIndeterminate`; er wird auf
`state() == FallbackRecoveryPending` aktualisiert. `RunPersistenceCoordinatorState`
ist reine Laufzeit-/RAM-Kategorie ohne Wire-Format-Bezug – keine
Codec-/Schema-Auswirkung.

`unavailableResult()` erhaelt einen Zweig fuer `FallbackRecoveryPending`,
der wie `LoadedActiveRun` `RunPersistenceResultStatus::RecoveryPending`
liefert (aus Sicht jedes Standardpfad-Aufrufers identische Semantik: der
Run muss erst evaluiert werden). Alle anderen, echten
`BlockedIndeterminate`-Faelle (Store-Ausgang unbestimmt, nicht
rekonstruierbar, fremde Epoche, unbekanntes Schema, `PreparedInterrupted`)
bleiben strikt in `BlockedIndeterminate` und sind fuer
`commitRecoveryOutcome()` **nicht** zulaessig (5.13: Vorbedingung exakt
`LoadedActiveRun || FallbackRecoveryPending`) – kein Recovery-Write aus
einem beliebigen `BlockedIndeterminate`.

**Kein Dead-End:** Hop 1 wird **immer** versucht, unabhaengig davon, ob die
Quelle `Current` oder `FallbackRecovered` war, und immer atomar committet,
sobald er lokal erfolgreich aufgebaut werden konnte. Fuer den
`FallbackRecoveryPending`-Fall gilt zusaetzlich:

- **Zielslot:** `targetSlotOverride = currentHead_->current.slot` (der
  bekannt defekte Slot – der gueltige Fallback-Slot bleibt bis zum Commit
  physisch unangetastet).
- **Fallback-Referenz nach Commit (Kernkorrektur dieser Revision):**
  `fallbackOverride = currentHead_->fallback` (die zum Ladezeitpunkt
  erfolgreich gelesene, weiterhin gueltige Fallback-Referenz) wird an
  `writeSnapshotCore` durchgereicht und dort fuer `committed.fallback`
  verwendet **statt** der bestehenden Standardregel `committed.fallback =
  currentHead_->current` (die im Fallback-Fall exakt die bekannt defekte
  Referenz waere und nach dem Ueberschreiben ihres Slots zusaetzlich mit
  veralteten CRC-/Revisionsdaten auf den neuen Inhalt zeigen wuerde – kein
  gueltiger Fallback). `currentHead_->fallback` ist zwischen Laden und
  diesem – dem einzigen aus `FallbackRecoveryPending` moeglichen – Commit
  unveraendert, da `FallbackRecoveryPending` ausser diesem einen Commit
  keinerlei Schreibzugriff zulaesst (`unavailableResult()`-Guard, s. o.).
- **Fail-closed-Guard:** vor dem Commit wird geprueft
  `targetSlotOverride != fallbackOverride->slot`; sind beide Slots
  identisch (nur bei bereits inkonsistenten Head-Metadaten moeglich),
  wird der Commit abgelehnt (`InvalidDecision`, kein Schreibversuch) statt
  den einzigen gueltigen Datensatz zu ueberschreiben.
- **Ergebnis nach vollstaendigem Commit:** `committed.current` = neuer
  Recovery-Snapshot im ehemals defekten Slot; `committed.fallback` = die
  unveraendert gueltige alte Fallback-Referenz. Current und Fallback zeigen
  auf zwei verschiedene, beide strukturell gueltige Slots/Records.

**Cutpoint-Verhalten (nicht-periodischer Commit, Prepared-Head -> Slot ->
Committed-Head, wie jeder Hop-1-Commit):**

- Cut **vor** dem Prepared-Head-Schreiben (z. B. Codec-Fehler,
  Capacity-Fehler): `state_` kehrt exakt zu `FallbackRecoveryPending`
  zurueck (5.13, `rollbackState`); Current bleibt defekt, Fallback bleibt
  unveraendert gueltig und ladbar; ein erneuter Hop-1-Versuch bleibt
  moeglich.
- Cut **nach** dem Prepared-Head-Schreiben, vor oder nach dem
  Slot-Schreiben: bestehender generischer `PreparedInterrupted`-Mechanismus
  greift unveraendert (`BlockedIndeterminate` beim naechsten Boot, kein
  automatischer Retry) – kein recovery-spezifischer Sonderfall (5.13).
- Cut **nach** vollstaendigem Committed-Head-Schreiben: Erfolg, wie oben
  beschrieben; ein anschliessend erneut beschaedigter neuer Current fuehrt
  beim naechsten Boot wieder zu `FallbackRecovered` -> `FallbackRecoveryPending`
  gegen den (nun ggf. aelteren, aber weiterhin strukturell gueltigen)
  Fallback-Datensatz – derselbe Mechanismus greift beliebig oft.
- Der `oldSlot`-Compare-and-Swap (`writeSlotExact`) erfolgt unveraendert
  gegen die physischen Bytes des **defekten** Zielslots – kein zusaetzlicher
  Sonderfall noetig, da `writeSlotExact` bereits heute jeden physischen
  Vorwert (auch einen strukturell nicht decodierbaren) korrekt behandelt.

**Tests:**
- Korrupter Current + gueltiger Fallback -> Recoverycommit -> neuer Current
  gueltig **und** alter gueltiger Fallback weiterhin ladbar.
- Anschliessend neuen Current beschaedigen -> Fallback-Recovery funktioniert
  erneut (zweiter Zyklus, kein "Slot-Poisoning").
- `targetSlotOverride == fallbackOverride->slot` (simulierte inkonsistente
  Head-Metadaten) -> Ablehnung ohne Schreibversuch.
- Cut vor Prepared-Head-Commit (Rueckkehr zu `FallbackRecoveryPending`), Cut
  nach Prepared-Head-Commit (generisch `PreparedInterrupted`), Cut nach
  vollstaendigem Commit (Erfolg mit korrektem Fallback).
- `state()` nach `FallbackRecovered`-Load ist `FallbackRecoveryPending`,
  nicht `BlockedIndeterminate` (aktualisierter bestehender Test, s. o.).

### 5.16 `BlockedIndeterminate` und `PersistenceCommittedApplyFailed` – getrennt

Store-Schreibausgang unbestimmt (`RunPersistenceStoreWriteResult::Indeterminate`,
`run_persistence_coordinator.cpp:427-431,452,466,477,486,500,518`) ->
`BlockedIndeterminate`/`StoreOutcomeUnknown`; bestaetigter Commit mit
fehlgeschlagenem RAM-Apply (`run_persistence_coordinator.cpp:617-625,676-685`)
-> `PersistenceCommittedApplyFailed`. `writeSnapshotCore` (5.13) und die
`ApplyRecoveryTimeCorrection`-Persistierung (5.19) uebernehmen beide
Zustaende unveraendert und getrennt. `FallbackRecoveryPending` (5.15) ist
ein eigener, von `BlockedIndeterminate` disjunkter Zustand und wird von
keiner dieser beiden Kategorien beruehrt.

### 5.17 Sensorevidenz – Vor-/Nach-Ausfall strukturell getrennt, latch-once pro Rolle und Episode

Ein einzelnes `RecoveryTemperatureEvidence{air,product,cooling}` kann nicht
gleichzeitig die laufend fortgeschriebene "letzte gueltige"-Evidenz und die
eingefrorene Vor-/Nach-Ausfall-Diagnose einer bestimmten Recoveryepisode
tragen, ohne dass spaetere normale Checkpoints die Diagnosedaten
ueberschreiben. Zusaetzlich reicht ein einmaliger **Gesamt**-Snapshot fuer
"erster gueltiger Wert nach Neustart" nicht aus: werden Air, Product und
Cooling zu unterschiedlichen Zeitpunkten nach dem Neustart erstmals
gueltig, erfasst ein einzelner gemeinsamer Snapshot nur die zu diesem einen
Zeitpunkt bereits gueltigen Rollen – spaeter erstmals gueltig werdende
Rollen wuerden nie erfasst.

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
struct FirstAfterRestartEvidence {
    std::optional<RoleTemperatureEvidence> air;
    std::optional<RoleTemperatureEvidence> product;
    std::optional<RoleTemperatureEvidence> cooling;
};

// Schema 3, Teil von RunPersistenceSnapshot:
struct RecoveryTemperatureEvidence {
    CrossRoleEvidence lastKnown;  // laufend, von jedem normalen Checkpoint aktualisiert
};
struct RecoveryEpisodeEvidence {
    CrossRoleEvidence beforeOutage;              // eingefroren, ausschliesslich beim initialen Hop 1 einer Recoveryepisode-Kette gesetzt
    FirstAfterRestartEvidence firstAfterRestart; // gemischte Lebensdauer, siehe unten
};
```

`lastKnown` wird ausschliesslich von der bestehenden, in allen vier
Checkpoint-Schreibpfaden sowie im Recovery-Commit-Kern (Hop 1/
Episode-Refresh) vorhandenen `updateRoleEvidence(RoleTemperatureEvidence&,
const device_platform::SensorQualitySnapshot&)` (aktualisiert
`filteredCelsius` nur bei `SensorQuality::Valid`, behaelt den letzten
gueltigen Wert bei `Stale`/`Failed`) fortgeschrieben – ueber einen
optionalen Parameter `const CrossRolePlausibilityContext* liveSensorEvidence`
an allen fuenf Schreibstellen (`nullptr` zulaessig).

**Gemischte Lebensdauer innerhalb von `RecoveryEpisodeEvidence` (Kern der
Korrektur):**

- `beforeOutage`: eingefroren **ausschliesslich** beim allerersten Hop 1
  einer Recoveryepisode-Kette (`beforeOutage =
  candidate.recoveryTemperatureEvidence.lastKnown` zum Commit-Zeitpunkt);
  ein Episode-Refresh (5.12) laesst `beforeOutage` unveraendert. Nur eine
  erneute, echte Hop-1-Ausfuehrung (naechste, unabhaengige
  Recoveryepisode nach vollstaendiger Aufloesung der vorherigen)
  ueberschreibt `beforeOutage` wieder vollstaendig.
- `firstAfterRestart` (alle drei Rollenfelder): wird bei **jedem** Anstieg
  von `recoveryEpisodeRevision` – Hop 1 **und** jedes Episode-Refresh –
  vollstaendig auf `{nullopt, nullopt, nullopt}` zurueckgesetzt ("erster
  gueltiger Wert nach *diesem* Neustart" ist pro Boot diagnostisch
  sinnvoll; Latches einer vorherigen, bereits abgeschlossenen Episode
  werden dabei akzeptiert ueberschrieben, da nur die aktuell offene
  Episode diagnostisch relevant ist).

**Latch-Regel, in derselben Reihenfolge wie der Reset ausgefuehrt:** direkt
im Anschluss an einen Reset (Hop 1/Episode-Refresh) sowie bei jedem der vier
bestehenden Checkpoint-Schreibpfade waehrend einer **offenen** Episode –
definiert als `candidate.pendingRecoveryAnchor.has_value()` (5.10), **nicht**
als `lastRecoveryEpisodeEvidence.has_value()`: Letzteres bleibt gemaess der
Loeschregel unten laenger bestehen als der Anker und darf deshalb nicht als
Latch-Bedingung dienen, sonst wuerde nach Aufloesung der Episode aus
unzusammenhaengenden spaeteren Live-Sensorwerten weiterlatcht. Bedingung
also `candidate.pendingRecoveryAnchor.has_value() && liveSensorEvidence !=
nullptr`:

```cpp
void latchFirstAfterRestart(RecoveryEpisodeEvidence& episode,
                            const CrossRolePlausibilityContext& live) {
    auto latch = [](std::optional<RoleTemperatureEvidence>& slot,
                    const device_platform::SensorQualitySnapshot& liveRole) {
        if (slot.has_value()) return;  // pro Rolle genau einmal je Episode
        if (liveRole.quality != device_platform::SensorQuality::Valid) return;
        slot = RoleTemperatureEvidence{liveRole.filteredCelsius, liveRole.quality};
    };
    latch(episode.firstAfterRestart.air, live.air);
    latch(episode.firstAfterRestart.product, live.product);
    latch(episode.firstAfterRestart.cooling, live.cooling);
}
```

Ist Air beim Hop 1 bereits gueltig, wird `firstAfterRestart.air` sofort im
selben Commit gesetzt; werden Product/Cooling erst Sekunden oder Minuten
spaeter gueltig, latcht sie der naechste beliebige der fuenf Schreibpfade
(Hop-1-Commit selbst nur einmalig, danach jeder normale Checkpoint oder ein
spaeteres Episode-Refresh), sobald `liveSensorEvidence` das erstmals meldet
– unabhaengig davon, ob dieser Checkpoint durch ein Kommando, eine
Transition, eine Sensorselektionsaenderung oder periodisch ausgeloest wurde.

`lastRecoveryEpisodeEvidence: std::optional<RecoveryEpisodeEvidence>` selbst
wird bei Hop 1 neu angelegt (nie zuvor `nullopt` -> jetzt belegt). Nach
**Resume/Reject** (Run bleibt aktiv, 5.14) bleibt es unveraendert als reines
Diagnosefeld der zuletzt abgeschlossenen Episode bestehen – harmlos, weil
die Latch-Regel oben ausschliesslich an `pendingRecoveryAnchor` haengt, nicht
an dieses Feld. Nach **Tombstone** (5.9, `clearActiveRunState()`,
`variant` wird `NoActiveRun`) wird es dagegen **im selben Commit auf
`nullopt` gesetzt** wie `pendingRecoveryAnchor`,
`recoveryBootAnchorMonotonicMillis` und `priorBootPhaseElapsed` (5.20) – ein
`NoActiveRun`-Snapshot traegt keinerlei Recovery-Diagnosedaten eines
beendeten Laufs mehr.

**Tests:**
- Air beim Hop 1 bereits gueltig, Product/Cooling erst bei einem spaeteren
  Checkpoint (persistCommand, persistTransition, persistSensorSelection,
  checkpointPeriodic – je einmal) gueltig: alle drei Latches korrekt und
  genau einmal gesetzt, spaetere Sensorwerte veraendern sie nicht mehr.
- Episode-Refresh (zweiter Reboot) waehrend Product/Cooling noch nicht
  gelatcht waren: Latches werden zurueckgesetzt und im neuen Boot erneut
  unabhaengig befuellt; `beforeOutage` bleibt unveraendert.
- Ein spaeterer regulaerer Checkpoint ueberschreibt weder `beforeOutage`
  noch bereits gesetzte `firstAfterRestart`-Werte.
- Nach Resume/Reject bleibt `lastRecoveryEpisodeEvidence` als Diagnosedaten
  bestehen; ein danach eintreffender, mit dem Run inhaltlich nicht mehr
  zusammenhaengender Live-Sensorwert latcht **nicht** nach (Regressionstest
  gegen den in dieser Revision behobenen Fehler: Latch-Bedingung haengt an
  `pendingRecoveryAnchor`, nicht an `lastRecoveryEpisodeEvidence`).
- Nach Tombstone (5.9) ist `lastRecoveryEpisodeEvidence` im selben Commit
  wie `pendingRecoveryAnchor` auf `nullopt` gesetzt.

### 5.18 `RunProgressState` – ehrliche Basis bleibt nach Migration und Folds bestehen

`RunProgressState` existierte vor Schema 3 nicht. Ein Fortschreiben auf
"Known" bereits nach dem ersten Fold nach einer Schema-1/2-Migration ist
semantisch falsch: ist die gesamte Vor-Schema-3-Fermentationszeit
unbekannt, macht ein spaeter beobachteter Abschnitt (z. B. 10 Minuten) den
zuvor unbekannten Anteil nicht rueckwirkend bekannt. Dasselbe gilt fuer
`ApplyRecoveryTimeCorrection` (5.19): eine neue Korrektur kann unbekannte
historische Zeit nicht rekonstruieren.

```cpp
enum class RunProgressBasis : std::uint8_t {
    KnownTotal,             // die gesamte bisherige Laufzeit dieses Runs ist lueckenlos bekannt (Run vollstaendig unter Schema 3 gestartet)
    PartialUnknownHistory,  // ein permanent unbekannter Alt-Anteil (Schema-1/2-Migration) besteht fuer den Rest dieses Runs, unabhaengig von spaeter bekannten Deltas
};
struct RunProgressState {
    RunProgressBasis basis{RunProgressBasis::KnownTotal};
    std::uint32_t observedRunSeconds{0U};  // ausschliesslich der BEKANNTE, tatsaechlich beobachtete Anteil, kumulativ
};
```

**Verbindlich, fuer die gesamte Lebensdauer eines Runs:**

- `basis` wird **genau einmal** gesetzt: `KnownTotal` bei einem unter
  Schema 3 frisch gestarteten Run; `PartialUnknownHistory` bei der
  Decodierung eines Schema-1- oder Schema-2-Datensatzes (kein
  `RunProgressState`-Feld im Wireformat) fuer diesen aktiven Run. `basis`
  wird danach **nie mehr veraendert** – weder durch einen Fold-Punkt
  (5.19) noch durch `ApplyRecoveryTimeCorrection` (5.19) noch durch eine
  automatische UTC-Reevaluation (5.14).
- Bekannte neue Sekunden werden trotzdem **kumuliert**:
  `observedRunSeconds` steigt bei jedem Fold-Ereignis (5.19) unabhaengig
  vom Wert von `basis` um genau das dort tatsaechlich beobachtete Delta.
  `PartialUnknownHistory` bedeutet "zusaetzlich zu `observedRunSeconds`
  existiert ein permanent unbekannter Alt-Anteil", nicht "0 Sekunden
  bekannt".
- Anzeige/Export rendert bei `PartialUnknownHistory` explizit "mindestens
  `observedRunSeconds` Sekunden bekannt, aelterer Anteil unbekannt", bei
  `KnownTotal` schlicht `observedRunSeconds`.

**Tests:**
- Schema 1 -> 3 und Schema 2 -> 3 fuer einen aktiven Run: `basis ==
  PartialUnknownHistory` sofort nach Migration.
- Mehrere Folds nach einer solchen Migration: `basis` bleibt
  `PartialUnknownHistory`, `observedRunSeconds` waechst um jedes Delta.
- `ApplyRecoveryTimeCorrection` nach einer solchen Migration: `basis`
  bleibt `PartialUnknownHistory` (Regressionstest gegen den in dieser
  Revision behobenen Fehler).
- Neuer, vollstaendig unter Schema 3 gestarteter Run: `basis == KnownTotal`,
  bleibt es fuer die gesamte Laufzeit.
- Gemischte Current/Fallback-Matrix 3/2 und 3/1; korrupter Schema-3-Current
  mit gueltigem Schema-2/1-Fallback; Prepared-Head-Unterbrechung ueber den
  Versionswechsel hinweg (Erweiterung der bestehenden Migrationstestreihe).

### 5.19 `observedRunSeconds` vs. Recovery-/Nominalzeitkorrektur – strukturell getrennt

**Fortschreibungspunkte fuer `observedRunSeconds` (Fold,
`deriveFermentingSecondsDelta(before, atMillis)`):** angewandt (1) bei jedem
Live-Phasenwechsel **aus** `Fermenting`, (2) in `decideRunAdjustment()`
unmittelbar **vor** der bestehenden `stateEnteredAtMillis`-Neusetzung bei
Daueraenderung (`run_commands.cpp:1019-1023`), (3) bei Hop 1, wenn
`pendingRecoveryAnchor.originalProcessState.state == Fermenting`,
ausschliesslich aus `pendingRecoveryAnchor.knownPhaseSecondsAtOriginalCheckpoint`
(dem Alt-Boot-lokalen, sicher bekannten Anteil – **niemals** aus dem
unsicheren Ausfallanteil). Kein `RunProgressAccountingRuntime`-Typ;
`stateEnteredAtMillis` selbst erfuellt bereits die Rolle der
Buchfuehrungsreferenz.

**Harte Trennung (Kern der Korrektur dieser Revision):**
`observedRunSeconds` bedeutet ausschliesslich tatsaechlich beobachtete,
eingeschaltete Fermentationszeit. Es wird **unter keinen Umstaenden** durch
eine Stromausfallkorrektur veraendert – weder durch den automatischen
Hop-1-Fold (der ausschliesslich `knownSecondsBeforeCheckpoint` verwendet,
niemals `RecoveryOutageBounds`), noch durch `ApplyRecoveryTimeCorrection`,
noch durch eine automatische UTC-Reevaluation.

Eine genauere UTC-Grenze beweist **Zeit**, aber nicht **biologische
Aktivitaet** (Gate C). Der kanonische Recovery-Vertrag verlangt: fehlende
Messzeit wird nur mit einem validierten thermischen Modell biologisch
bewertet; ohne Modell wird kein scheinbar exakter temperaturgewichteter
Fortschritt erfunden.

**`ApplyRecoveryTimeCorrection` – ausschliesslich nominale, benutzerseitig
gekennzeichnete Korrektur, kein automatischer Schreibpfad:**

**Geltungsbereich:** ausschliesslich `Fermenting`. Fuer `WaitingForProduct`
ist `ResolveRecoveryUncertainty` (5.14) der zustaendige, semantisch andere
Vertrag (Phasengrenzentscheidung: fortsetzen/beenden, keine
Sekundenkorrektur) – beide Kommandos bleiben getrennt.

**Wirkung:** mutiert ausschliesslich
`RecoveryTimeCorrectionRecord.appliedSecondsDelta` (unten). Mutiert
**niemals** `RunCommandState.runProgress.observedRunSeconds` und **niemals**
`RunProgressState.basis`.

```cpp
struct RecoveryTimeCorrectionRecord {
    std::uint32_t appliedAtEpisodeRevision{0U};
    std::uint32_t appliedSecondsDelta{0U};  // NOMINALE Ausfallzeitkorrektur, getrennt von observedRunSeconds, in Anzeige/Export gesondert ausgewiesen
};
```

**Harte Grenzen, aus dem tatsaechlichen Ausfallintervall abgeleitet:**
`appliedSecondsDelta: std::uint32_t` (keine negativen Korrekturen). Gueltiger
Bereich: `[0, totalSecondsUpperBound - totalSecondsLowerBound]`
(`RecoveredPhaseElapsed` fuer `Fermenting` zum Episodenzeitpunkt). Ist
`totalSecondsUpperBound` nicht bekannt (keine UTC-Bruecke, 5.2), ist **kein**
Korrekturwert gegen eine obere Grenze pruefbar -> jede Korrektur wird
abgelehnt (`InvalidInput`), **kein** Saturieren auf einen Ersatzwert. Ein
Wert ausserhalb `[0, maxDelta]` wird ebenso abgelehnt, nicht auf die Grenze
gekappt. Additionsueberlauf von `appliedSecondsDelta` (uint32, bei erneuter
Anwendung innerhalb derselben Episode) wird vor dem Schreiben geprueft und
fuehrt zu `InvalidInput` (praktisch nicht erreichbar, aber explizit
abgesichert).

**Kein automatischer Korrekturpfad:** Die automatische UTC-Reevaluation
(`reevaluatePendingRecovery`, 5.14) erzeugt ausschliesslich aktualisierte
Grenzwerte/Verdicts, niemals einen `appliedSecondsDelta`-Wert.
`RecoveryTimeCorrectionRecord` wird **ausschliesslich** durch das explizite
Kommando `CommandKind::ApplyRecoveryTimeCorrection` geschrieben – dadurch
bleibt jede darin gespeicherte Korrektur per Konstruktion als manuelle
Benutzerentscheidung erkennbar, es gibt keinen zweiten Schreiber, dessen
Herkunft nachtraeglich unterschieden werden muesste.

**Verwendung bei Fortschritts-/Abschlussentscheidungen:** Anzeige/Export
und jede explizite Prozessentscheidung, die eine nominale
Ausfallzeitgutschrift beruecksichtigen soll, lesen `observedRunSeconds` und
`appliedSecondsDelta` **getrennt** und weisen sie getrennt aus (z. B. "X
Sekunden beobachtet, zusaetzlich Y Sekunden manuell als Ausfallzeit
angerechnet"); keine stille Zusammenfuehrung in ein einzelnes,
ununterscheidbares Feld. `ResolveRecoveryUncertainty` und
`ApplyRecoveryTimeCorrection` bleiben semantisch getrennte Kommandos.

**Episoden-/Staleness-Vertrag:** `CommandKind::ApplyRecoveryTimeCorrection`;
zweiter Versuch mit identischem Inhalt fuer dieselbe Episode ->
`AlreadyProcessed`; mit abweichendem Inhalt -> `NotAllowedInState`;
`CommandEnvelope.expectedRunRevision` **und** `expectedRecoveryEpisodeRevision`
muessen beide mit `current` uebereinstimmen, sonst `StaleState`;
Write-before-apply; Fehlerfaelle getrennt nach 5.16.

**Tests:**
- `ApplyRecoveryTimeCorrection` innerhalb der Grenzen erfolgreich; ausserhalb
  -> Ablehnung ohne Saturierung; ohne bekannte Obergrenze -> Ablehnung;
  Overflow-Schutz.
- `observedRunSeconds` bleibt nach jeder Ausfallkorrektur (automatisch und
  manuell) unveraendert – Regressionstest gegen den in dieser Revision
  behobenen Fehler.
- Automatische UTC-Reevaluation erzeugt keinen `appliedSecondsDelta`-Eintrag
  und keine Aenderung an `observedRunSeconds`.
- Anzeige/Export weist `observedRunSeconds` und `appliedSecondsDelta`
  nachweisbar getrennt aus (kein gemeinsames, ununterscheidbares Feld).

### 5.20 `PriorBootPhaseElapsed` – vollstaendiger Persistenz- und Lebenszyklusvertrag

**Wire-/RAM-Feld:**

```cpp
// run_persistence_contract.hpp, Schema 3, Teil von RunPersistenceSnapshot
struct TaggedPriorBootPhaseElapsed {
    ProcessState taggedState;             // Phase, fuer die dieser Vor-Boot-Anteil gilt
    PriorBootPhaseElapsed elapsed;        // 5.5: lowerBoundSeconds + optional upperBoundSeconds
};
// RunPersistenceSnapshot: std::optional<TaggedPriorBootPhaseElapsed> priorBootPhaseElapsed;
```

**Codec/Migration:** Schema-3-Feld; eine Schema-1- oder Schema-2-Decodierung
liefert immer `nullopt` (das Feld existierte nicht).

**Invariante (auch als harte Konsistenzpruefung in
`validateRunPersistenceSnapshot()`, 5.11 Punkt 3):**
`priorBootPhaseElapsed->taggedState == processState.state` oder das Feld ist
`nullopt` – ein Snapshot mit einem nicht zur aktuellen Phase passenden Tag
ist strukturell ungueltig und wird nie persistiert. **Zusaetzlich**, als
defensive Absicherung auf Leserseite (5.5): jeder Aufrufer liest den Wert
nur, wenn `current.processState.state` mit dem Tag uebereinstimmt, sonst
wird `{}` verwendet – doppelte Absicherung gegen eine versehentlich auf die
falsche Phase angewandte stale Zeit.

**Setzen:** ausschliesslich bei einer Recovery-Aktivierung (Hop 1 oder
Hop 2 mit Resume, 5.7/5.8), wenn die resultierende Phase
`WaitingForProduct`, `Fermenting` oder `CoolHolding` ist (5.6). Getaggt mit
genau dieser resultierenden Phase.

**Akkumulationsregel bei wiederholter Recovery innerhalb derselben, noch
nicht gewechselten Phase (kein doppeltes Zaehlen, kein stilles
Weglassen):** existiert beim Aufbau eines neuen Hop 1 bereits ein
`priorBootPhaseElapsed` mit `taggedState` identisch zur Phase, in die
recovert wird (d. h. die Phase hat seit dem vorherigen getaggten Wert nicht
gewechselt), wird der neue Wert **addiert**, nicht ersetzt:

```text
neu.lowerBoundSeconds = alt.lowerBoundSeconds
    + pendingRecoveryAnchor.knownPhaseSecondsAtOriginalCheckpoint
    + outage.outageSecondsLowerBound
neu.upperBoundSeconds =
    (alt.upperBoundSeconds.has_value() && outage.outageSecondsUpperBound.has_value())
        ? alt.upperBoundSeconds.value()
            + pendingRecoveryAnchor.knownPhaseSecondsAtOriginalCheckpoint
            + outage.outageSecondsUpperBound.value()
        : std::nullopt  // ein einziges unbekanntes Teilintervall macht die gesamte Obergrenze unbekannt, kein stilles Weglassen dieses Anteils
```

Stimmt `taggedState` **nicht** mit der Zielphase ueberein (kein Tag
vorhanden oder ein zwischenzeitlicher echter Phasenwechsel hat ihn bereits
geloescht, s. u.), beginnt die Akkumulation bei `lowerBoundSeconds = 0`,
`upperBoundSeconds = nullopt` und wird ausschliesslich aus dieser einen
Recovery-Episode gebildet. Welcher der beiden akkumulierten Werte
(`lowerBoundSeconds` vs. `upperBoundSeconds`) an `elapsedWithPrior`
weitergereicht wird, folgt unveraendert der Phasenregel aus 5.8 (Untergrenze
fuer alle Phasen ausser `WaitingForProduct` mit `DefinitelyStillValid`, dort
Obergrenze).

**Erhalt/Loeschen:**

- Bleibt fuer die laufende, zeitbegrenzte Phase erhalten (jeder weitere
  normale Checkpoint innerhalb derselben Phase fuehrt denselben Wert fort,
  unveraendert).
- Wird bei **jedem** echten Phasenwechsel (jede erfolgreiche
  `applyProcessTransition`, die `processState.state` tatsaechlich
  aendert) atomar im selben Commit geloescht (`nullopt`).
- Wird bei Start eines neuen Runs sowie in `clearActiveRunState()`
  ebenfalls geloescht (derselbe Helfer, der bereits `pendingRecoveryAnchor`/
  `recoveryBootAnchorMonotonicMillis` zuruecksetzt, 5.9/5.10).

**Tests:**
- Roundtrip (Persistieren/Laden) fuer `WaitingForProduct`, `Fermenting`,
  `CoolHolding` je einzeln.
- Phasenwechsel loescht das Feld atomar im selben Commit.
- Neuer Run und `clearActiveRunState()` loeschen das Feld.
- Zwei aufeinanderfolgende Recovery-Episoden innerhalb derselben Phase:
  Akkumulation korrekt (Unter- **und** Obergrenze), keine doppelte Zaehlung.
- Eine der beiden Episoden ohne bekannte Obergrenze: akkumulierte
  Obergrenze wird `nullopt`, nicht die unbekannte Episode stillschweigend
  ignoriert.
- Snapshot mit nicht zur aktuellen Phase passendem Tag ist gemaess 5.11
  strukturell ungueltig (Contract-Test).

### 5.21 `Completed` – expliziter, schmaler Sonderpfad

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
  echten Boot-Zeitpunkt unangetastet.

Test: persistierter `Completed`-Snapshot bleibt nach Reboot `Completed` bis
zur Quittierung; `stateEnteredAtMillis` liegt im aktuellen Boot; kein
`InvalidDecision`.

### 5.22 `WeightedProgressStatus` – entfernt (KISS)

Ein persistiertes Enum mit exakt einem heute erreichbaren Wert traegt
keinen Zustand, der sich innerhalb von Release 1 aendern kann – ein
konstantes Wire-Feld ohne fachliche Funktion.

**Vertrag:** Kein `WeightedProgressStatus`-Feld im Schema-3-Persistenzvertrag.
"Nicht kalibriert" ist eine statische, aus dem Firmware-/Commissioning-Stand
ableitbare Anzeige-/Exportkonstante (nicht pro Lauf persistiert). Die reale
Aktivitaetsgewichtung bleibt ohne validiertes Modell ausdruecklich
unimplementiert; #34 liefert die Messgrundlage (verifiziert per `gh issue
view 34`: Referenzmessungen, Offsets, thermische Reaktion – kein
Aktivitaetskennfeld als eigenes Akzeptanzkriterium), ist aber nicht der
stille Modelleigentuemer. Ein spaeteres, dann eigenstaendig zu planendes
Vorhaben fuehrt bei Bedarf ein echtes, mehrwertiges Statusfeld ein.

### 5.23 Restart-Sensorauswahl (Gate A)

`SensorSelectionPhase::RestartRevalidationPending` wird zwischen Hop 1 und
Hop 2 real bewertet; `computeRestartSensorSelection`
(`sensor_selection.cpp:890-907`, Stub) wertet den persistierten
Sensorselektionszustand gegen die aktuelle `CrossRolePlausibilityContext`
aus; negatives Ergebnis -> `RecoveryReject`. Dieselbe
`CrossRolePlausibilityContext`-Auswertung liefert den `liveSensorEvidence`-
Kontext fuer den Latch-Mechanismus aus 5.17.

### 5.24 Komposition/DI – kein erfundener Aufrufer

`RunRecoveryCoordinator::activate(...)` und `reevaluatePendingRecovery(...)`
sind nativ testbare APIs; kein bestehender produktiver Aufrufer wird
behauptet; produktive Verdrahtung bleibt dem zustaendigen
Composition-Issue vorbehalten (Gate B).

### 5.25 Schema-Versionierung

`kCurrentRunPersistenceSchema` (aktuell `2U`,
`run_persistence_contract.hpp:24`) wird auf `3U` angehoben.
`knownRunPersistenceSchema()` akzeptiert nach dieser Aenderung `{1U, 2U,
3U}` (bisher `{1U, 2U}`), damit ein vor #18 geschriebener Schema-2-Slot
weiterhin decodierbar bleibt. Alle in dieser Revision neu eingefuehrten
Felder (`PendingRecoveryAnchor`, `recoveryBootAnchorMonotonicMillis`,
`RunProgressState`, `RecoveryEpisodeEvidence`, `RecoveryTimeCorrectionRecord`,
`TaggedPriorBootPhaseElapsed`, `recoveryEpisodeRevision`) sind
Schema-3-exklusiv; eine Schema-1/2-Decodierung liefert fuer jedes davon den
jeweiligen Leerwert (`nullopt`/`0`, `basis` wird `PartialUnknownHistory`
statt `KnownTotal`, nach 5.18-Migrationsregel).

### 5.26 ROADMAP-Konsistenz

`docs/ROADMAP.md:3` zeigt `Stand: 2026-08-08`; Zeile 32-33 ist bereits so
formuliert, dass Details/Abhaengigkeitsstand im Plan stehen und aktuell
**keine** offenen Ownerentscheidungen behauptet werden. Dieser Zustand wurde
in dieser Session direkt am aktuellen Dateiinhalt verifiziert – **kein**
weiterer ROADMAP-Aenderungsbedarf durch diesen Plan-Commit. #18/PR #102
bleibt aktuelle Arbeit; Ressourcen-Gate ueber #29/`OPEN_POINTS.md` weiterhin
sichtbar; #22 bleibt naechste fachliche Arbeit nach #18.

### 5.27 #24-Abgrenzung

Fault-Klassen, SAFE_BOOT-Feinausbau und Fault-Reset-Ablauf bleiben #24
zugeordnet. #18 nutzt `Fault` ausschliesslich ueber die bestehende
`RecoveryReject`-Logik.

## 6. Modul- und Abhaengigkeitsgrenzen

Alle neuen/geaenderten Dateien liegen in `lib/fermentation_app/src/` und
haengen ausschliesslich von bestehenden `device_platform`-Ports und
bestehenden `fermentation_app`-Modulen ab (ADR-013 eingehalten). Kein neuer
`SensorRole`-Typ (feste `air`/`product`/`cooling`-Felder, konsistent mit
`CrossRolePlausibilityContext`).

## 7. Datei-/Commit-Aufschluesselung

| # | Commit | Inhalt |
|---|---|---|
| 1 | `feat(process-state-machine): RecoveryReentryRequired-/RecoveryEndedByExpiredWait-Topologie, PriorBootPhaseElapsed-Parameter, elapsedWithPrior` | 5.5-5.9 |
| 2 | `feat(persistence): Schema 3 – PendingRecoveryAnchor, recoveryBootAnchorMonotonicMillis, RunProgressState (mit ehrlicher Basis), RecoveryTemperatureEvidence/RecoveryEpisodeEvidence (mit Rollen-Latches), RecoveryTimeCorrectionRecord, TaggedPriorBootPhaseElapsed, recoveryEpisodeRevision, validStateFor-Erweiterung, Schema-Bump auf 3` | 5.10, 5.11, 5.17, 5.18, 5.19, 5.20, 5.25; Migrationstests 1/2/3 |
| 3 | `feat(recovery): computeRecoveryOutageBounds, computeRecoveredPhaseElapsed, evaluateRecoveryTimeVerdict, deriveUtcAtRecoveryBootAnchor` | `run_recovery_time.hpp/.cpp` (5.2-5.4, 5.10) |
| 4 | `feat(sensor-selection): reale Restart-Reaktivierung` | Gate A / 5.23 |
| 5 | `feat(persistence-coordinator): writeSnapshotCore mit explizitem Rollbackzustand, Fallback-Override-Parameter, FallbackRecoveryPending-Zustand, Signaturerweiterung der vier Checkpoint-Schreibpfade um liveSensorEvidence; bestehenden FallbackRecovered-state()-Test (`test_run_persistence_coordinator.cpp:1454-1456`) auf `FallbackRecoveryPending` aktualisiert` | 5.13, 5.15, 5.16, 5.17 |
| 6 | `feat(persistence-coordinator): commitRecoveryOutcome, activateLoadedRun (Hop 1 + bedingt Hop 2), Episode-Refresh-Pfad` | 5.7-5.9, 5.12-5.14 |
| 7 | `feat(persistence-coordinator): activateFallbackRecoveredRun, Slot-/Fallback-Override, Slot-Distinctness-Guard` | 5.15 |
| 8 | `feat(persistence-coordinator): resolveRecoveryOutcome, ResolveRecoveryUncertainty, Completed-Sonderpfad` | 5.14, 5.21 |
| 9 | `feat(run-commands): ApplyRecoveryTimeCorrection (nominale Korrektur, getrennt von observedRunSeconds), AdjustRun-Zeitfaltung` | 5.19 |
| 10 | `feat(recovery): RunRecoveryCoordinator (activate, reevaluatePendingRecovery – ausschliesslich Verdicts, keine Sekundenschreibung)` | 5.14, 5.19, 5.24 |
| 11 | `docs: Anzeigevertrag (getrennte Ausweisung observedRunSeconds/appliedSecondsDelta, LegacyUnknown-Anzeige), Ressourcenbudget` | Abschnitt 10 |

## 8. Testmatrix

1. Hop 1 mit echter geladener Altphase; Negativtest gegen vorgetaeuschten
   `Boot`-Quellzustand.
2. `computeRecoveryOutageBounds`: exakte UTC-Bruecke bleibt ein Intervall
   (Ober-/Untergrenze via Kontrollpunktabstand), fehlender Anker -> `nullopt`,
   checked Arithmetic (5.2).
3. `computeRecoveredPhaseElapsed`: korrekte Komposition aus bereits fertig
   berechnetem `knownSecondsBeforeCheckpoint` und Ausfallintervall, checked
   Arithmetic (5.3).
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
   `PendingRecoveryAnchor` und passendem `priorBootPhaseElapsed`-Tag
   gueltig; ohne/mit inkonsistentem Kontext ungueltig (5.11).
9. **`PendingRecoveryAnchor` ueber Hop-1-Commit und mehrere Reboots (5.10):**
   Hop1-only -> Commit -> UTC spaeter im selben Boot verfuegbar; Hop1-only ->
   Reboot -> UTC im zweiten Boot verfuegbar; mehrere Reboots -> Anker
   byte-identisch, nur `recoveryBootAnchorMonotonicMillis` aendert sich;
   niemals Subtraktion von Monotonic-Werten verschiedener Boots; spaetere
   UTC-Reevaluation verwendet nicht den Hop-1-/Episode-Refresh-Commit als
   Ausfallanker.
10. Reboot waehrend bereits persistiertem `RecoveryEvaluation`:
    Episode-Refresh statt Hop 1, `recoveryEpisodeRevision` erhoeht sich,
    beide Aufloesungswege bleiben erreichbar.
11. **`writeSnapshotCore`-Rollbackvertrag (5.13):** alle Vor-Commit-
    Fehlerpfade (Codec, Capacity, NotWritten) stellen aus
    `LoadedActiveRun`, `FallbackRecoveryPending` und regulaerem `Ready`
    exakt den uebergebenen `rollbackState` wieder her, niemals ein aus
    `currentHead_` neu berechnetes `Ready`/`ReadyEmpty`; `Indeterminate`
    fuehrt unabhaengig vom Aufrufer zu `BlockedIndeterminate`; bestaetigter
    Commit mit RAM-Apply-Fehler fuehrt zu `PersistenceCommittedApplyFailed`;
    Standardpfade bleiben byte-identisch zum bisherigen Verhalten.
12. **Fallback-Recovery-Fallback-Korrektheit (5.15):** korrupter Current +
    gueltiger Fallback -> Recoverycommit -> neuer Current gueltig **und**
    alter gueltiger Fallback weiterhin ladbar; anschliessend neuen Current
    beschaedigen -> Fallback-Recovery funktioniert erneut;
    Slot-Distinctness-Guard; Cut vor/nach Prepared-/Head-Commit mit
    erwartetem Zustand je Cutpoint.
13. `state()` nach `FallbackRecovered`-Load ist `FallbackRecoveryPending`
    (aktualisierter bestehender Test `test_run_persistence_coordinator.cpp:1454-1456`);
    kein Recovery-Write aus einem beliebigen `BlockedIndeterminate`.
14. **Legacy-Unknown-Historie (5.18):** Schema 1/2 -> mehrere Folds ->
    `basis` bleibt `PartialUnknownHistory`; Recoverykorrektur ->
    `basis` bleibt `PartialUnknownHistory`; neuer Schema-3-Run -> `basis ==
    KnownTotal`; Anzeige unterscheidet "unbekannt" von "0 Sekunden".
15. **`observedRunSeconds` strikt getrennt (5.19):** bleibt bei jeder
    automatischen und manuellen Ausfallkorrektur unveraendert; automatische
    UTC-Reevaluation erzeugt keinen `appliedSecondsDelta`-Eintrag;
    `ApplyRecoveryTimeCorrection` innerhalb der Grenzen erfolgreich, ausserhalb
    -> Ablehnung ohne Saturierung, ohne bekannte Obergrenze -> Ablehnung,
    Overflow-Schutz; getrennte Ausweisung in Anzeige/Export.
16. **First-after-restart latch-once pro Rolle (5.17):** unterschiedlich
    spaet gueltig werdende Air/Product/Cooling-Sensoren, je einzeln
    gelatcht ueber alle fuenf Schreibpfade hinweg; Episode-Refresh setzt
    Latches zurueck, `beforeOutage` bleibt unveraendert; spaeterer normaler
    Checkpoint ueberschreibt weder `beforeOutage` noch gesetzte Latches;
    Latch-Bedingung haengt an `pendingRecoveryAnchor`, nicht an
    `lastRecoveryEpisodeEvidence` (kein Nachlatchen nach Resume/Reject);
    Tombstone loescht `lastRecoveryEpisodeEvidence` im selben Commit wie
    `pendingRecoveryAnchor`.
17. **`PriorBootPhaseElapsed` (5.20):** Roundtrip je Phase; Phasenwechsel-
    Clear; neuer Run/`clearActiveRunState()`-Clear; Akkumulation ueber zwei
    Recovery-Episoden innerhalb derselben Phase (Unter- und Obergrenze);
    eine Episode ohne bekannte Obergrenze macht die akkumulierte Obergrenze
    `nullopt`; Tag-Mismatch ist strukturell ungueltig.
18. `Completed`-Restore: bleibt `Completed` bis Quittierung, kein
    `InvalidDecision`, `stateEnteredAtMillis` im aktuellen Boot.
19. `StoreOutcomeUnknown` vs. bestaetigter Commit + RAM-Apply-Fehler
    weiterhin getrennt.
20. Schema-1/2/3-Current/Fallback-Matrix vollstaendig (Regression);
    `knownRunPersistenceSchema` akzeptiert `{1,2,3}`.
21. Alle bestehenden #20/#21-Sensor-/Sicherheitsregressionen bleiben gruen.
22. `git diff --check`.

## 9. Safety-/Security-/Recovery-/Hardwaregrenzen

Keine Aktorfreigabe vor abgeschlossenem Hop 2 oder vor `Completed`-
Quittierung. Kein Schreiben vor vollstaendigem lokalem Kandidatenaufbau.
Kein Aktorpfad direkt aus `FallbackRecoveryPending` vor Hop 1. Kein
Recovery-Commit aus einem beliebigen `BlockedIndeterminate`. Keine
biologische oder "observed" Zeitgutschrift ohne validiertes Modell bzw.
ohne explizite, gesondert ausgewiesene Benutzerentscheidung. Reale
Hardware-/NVS-Anbindung bleibt #29/#90 vorbehalten.

## 10. Ressourcen-/Betriebsbudget

Schema-3-Zuwachs gegenueber Schema 2: `PendingRecoveryAnchor` (~50-60 Byte:
`ProcessRuntimeState`-Kopie + 5 skalare Felder, optional, nur waehrend
offener Episode belegt), `recoveryBootAnchorMonotonicMillis` (9 Byte,
optional), `RunProgressState` (5 Byte inkl. Basis-Tag),
`RecoveryTemperatureEvidence.lastKnown` (~30 Byte),
`RecoveryEpisodeEvidence` (optional, `beforeOutage` ~30 Byte +
`firstAfterRestart` mit drei optionalen ~15-Byte-Feldern, zusammen ~75-90
Byte, nur waehrend offener Episode belegt), `RecoveryTimeCorrectionRecord`
(8 Byte, optional), `recoveryEpisodeRevision` (4 Byte),
`TaggedPriorBootPhaseElapsed` (~10 Byte, optional, nur waehrend
zeitbegrenzter Phase belegt). In Summe deutlich unter dem bestehenden
`kMaximumCheckpointRecordBytes`-Budget (8240 Byte) bzw.
`kMaximumRunPersistencePayloadBytes` (8192 Byte); im Rahmen der
Schema-3-Schreibtests (Testmatrix 20) abgedeckt – ein tatsaechliches
Ueberschreiten des Budgets wuerde dort als `CapacityExceeded` auffallen,
ohne dass ein gesonderter Groessen-Assert noetig ist.

## 11. SOLID/DRY/KISS

`RecoveryOutageBounds`/`RecoveredPhaseElapsed` sind zwei kleine, einzeln
testbare, nicht ueberladene Typen statt eines vermischten Objekts;
`RecoveredPhaseElapsedInput` nimmt bewusst nur noch einen bereits fertig
abgeleiteten Sekundenwert entgegen, wodurch eine Boot-uebergreifende
Subtraktion strukturell unmoeglich wird, statt sie per Disziplin zu
vermeiden. `elapsedWithPrior` ist eine einzige neue Vergleichsfunktion, an
drei bestehenden Stellen sowie am eingebauten `WaitingForProduct`-Check
wiederverwendet. `writeSnapshotCore` ist eine einzige Extraktion, von allen
Schreibpfaden (Standard und Recovery) gemeinsam genutzt, mit exakt einem
zusaetzlichen Parameter (`rollbackState`) statt einer impliziten,
neu-berechneten Vorbedingung. `FallbackRecoveryPending` ist ein
eigenstaendiger, disjunkter Zustand statt eines versteckten Grundes auf
`BlockedIndeterminate` – der Typ selbst verhindert eine versehentliche
Wiederverwendung fuer andere `BlockedIndeterminate`-Faelle.
`WeightedProgressStatus` entfaellt vollstaendig statt eines
Ein-Wert-Enums. `RunProgressBasis`/`RecoveryTimeCorrectionRecord` trennen
zwei fachlich verschiedene Groessen (beobachtete vs. nominale Zeit) in zwei
Felder statt eines ueberladenen, mehrdeutigen Feldes.

## 12. Dokumentations-/Abschlussnachweis

- `docs/ROADMAP.md`: bereits aktuell (5.26), kein weiterer Aenderungsbedarf
  durch diese Revision.
- `docs/RUN_PERSISTENCE.md`/`docs/RECOVERY_AND_INTERRUPTION.md`: werden im
  Umsetzungscommit (Nr. 11) um die in 5.2-5.21 vertraglich fixierten Punkte
  ergaenzt, insbesondere die getrennte Ausweisung von `observedRunSeconds`
  und der nominalen Ausfallzeitkorrektur (5.19) sowie die ehrliche
  Legacy-Historie (5.18).
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

Revision 6 ist ein vollstaendiger, eigenstaendiger Plan. Nach Commit dieser
Datei: **anhalten**, `git push`, Remote-SHA verifizieren (Abschnitt 12),
PR-Beschreibung und SESSION HANDOVER aktualisieren. Keine Implementierung.
Kein `Ready for review`. Keine Remote-CI. Kein Merge. Keine Branchloeschung.
