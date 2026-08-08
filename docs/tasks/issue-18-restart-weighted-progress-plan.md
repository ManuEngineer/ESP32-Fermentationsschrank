# Plan: Issue #18 – Wiederanlauf und temperaturgewichteter Fortschritt

## 1. Status

- Revision: **4** (ersetzt Revision 1, 2 und 3 vollstaendig; kein Abschnitt
  dieser Datei verweist auf eine fruehere Revision als weiterhin gueltig –
  jeder Vertrag steht hier eigenstaendig).
- Draft-PR: #102 (`plan/issue-18-restart-weighted-progress` -> `main`).
- Live-Issue: #18 ("[E2.3] Wiederanlauf und temperaturgewichteten Fortschritt
  implementieren").
- Plan-Basis: `main` = `17ab3f5399a066465298ac6871b965d176a38d32` (enthaelt
  PR #103, bereits gemergt). Branch ist 0 Commits hinter `main`.
- Remote-Verifikation (Pflicht seit dem Vorfall in dieser Session, siehe
  Abschnitt 13): Nach dem Commit dieser Revision wird `origin/plan/issue-18-restart-weighted-progress`
  per frischem `git fetch` gegen den lokalen `HEAD` geprueft, bevor die
  Session als abgeschlossen gemeldet wird.
- Dieser Plan implementiert noch nichts. Umsetzung beginnt erst nach
  Freigabe des exakten Commits dieser Datei durch den Owner.

## 2. Live-Status-Pruefung (Basis dieser Revision)

- `gh issue view 18`: weiterhin offen, Scope/Akzeptanzkriterien unveraendert.
- `gh pr view 102`: Draft, Base `main`, Head
  `plan/issue-18-restart-weighted-progress`.
- PR #103 ist gemergt; `docs/ROADMAP.md` wird in dieser Revision erneut
  korrigiert (Abschnitt 5.17), da sie den bereits gemergten PR #103
  faelschlich noch als zu mergende aktive Arbeit auswies.
- Alle folgenden Codezeilenangaben wurden in dieser Session erneut direkt am
  Dateiinhalt verifiziert (nicht aus Revision 3 uebernommen).

## 3. Owner-Entscheidungen (Gates)

- **Gate A – Restart-Sensorauswahl wird real angewandt.** Der in #21
  vorbereitete, aktuell durch `computeRestartSensorSelection` (Stub,
  `sensor_selection.cpp:890-907`) fail-closed blockierte Uebergabepunkt wird
  in #18 tatsaechlich implementiert.
- **Gate B – kein neuer allgemeiner Prozesszyklus.** #18 liefert die nativ
  testbare Recovery-/Reevaluationslogik vollstaendig; produktive
  ESP-/NVS-Composition inkl. Verdrahtung eines etwaigen periodischen
  Aufrufers bleibt dem zustaendigen Composition-Issue (nicht #18)
  vorbehalten. #18 behauptet keinen bestehenden produktiven Aufrufer, den es
  nicht gibt (siehe 5.16).
- **Gate C – keine erfundene Aktivitaetskurve.** Es wird keine unkalibrierte
  biologische Aktivitaetskurve eingefuehrt; kein normaler Sekundenwert wird
  als "temperaturgewichtet" fehlbezeichnet; unsichere Ausfallzeit wird ohne
  freigegebenes Modell nicht biologisch gutgeschrieben.

## 4. Ziel und Nicht-Ziele

**Ziel:** Nach einem Neustart wird ein geladener aktiver Lauf sicher
bewertet, korrekt fortgesetzt oder korrekt beendet, mit nachvollziehbarem,
ehrlich gekennzeichnetem Fortschritt und ohne jede Aktorfreigabe vor
abgeschlossener Bewertung.

**Nicht-Ziele (Release 1):** reale Hardware-/NVS-Anbindung (#29/#90);
Fault-Klassen/SAFE_BOOT-Feinausbau (#24); Web-/Anzeige-UI-Implementierung
(nur Domain-/Application-Vertrag); kalibrierte biologische
Temperatur-Aktivitaets-Gewichtung (Gate C); ein neuer periodischer
Anwendungszyklus (Gate B); produktive Verdrahtung eines automatischen
UTC-Reevaluationsaufrufers (Gate B, 5.16).

## 5. Bindende Fachvertraege

### 5.1 Modul- und Dateizuordnung

| Bereich | Datei(en) | Aenderungsart |
|---|---|---|
| Recovery-Zeitfenster (reine Daten) | `lib/fermentation_app/src/run_recovery_time.hpp/.cpp` | neu |
| Recovery-Orchestrierung (Hop 1/2, Evidenz, Episoden) | `lib/fermentation_app/src/run_recovery.hpp/.cpp` | neu |
| Zustandsautomat: neue Reasons, Topologie | `lib/fermentation_app/src/process_state_machine.hpp/.cpp` | erweitert |
| Persistenzvertrag: Schema 3 | `lib/fermentation_app/src/run_persistence_contract.hpp/.cpp` | erweitert |
| Codec: Schema-3-Gate | `lib/fermentation_app/src/run_persistence_codec.cpp` | erweitert |
| Coordinator: Commit-Kern, `activateLoadedRun`, `activateFallbackRecoveredRun`, `resolveRecoveryOutcome` | `lib/fermentation_app/src/run_persistence_coordinator.hpp/.cpp` | erweitert (inkl. Signaturaenderung der vier Checkpoint-Schreibpfade, 5.12) |
| Kommandos: `ResolveRecoveryUncertainty`, `ApplyRecoveryTimeCorrection` | `lib/fermentation_app/src/run_commands.hpp/.cpp` | erweitert |
| Restart-Sensorauswahl (Gate A) | `lib/fermentation_app/src/sensor_selection.hpp/.cpp` | erweitert |
| Tests | `test/test_run_recovery_time/`, `test/test_run_recovery/`, `test/test_process_state_machine/`, `test/test_run_persistence_coordinator/`, `test/test_run_commands/`, `test/test_sensor_selection/` | neu/erweitert |

### 5.2 Recovery-Eintrittsvertrag (Hop 1) – tatsaechlich gueltige Topologie

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 3):**
`validBootTopology()` (`process_state_machine.cpp:223-239`) erlaubt
`TransitionReason::RecoveryRequired` ausschliesslich fuer
`from == ProcessState::Boot`. `applyProcessTransition()`
(`process_state_machine.cpp:1006-1021`) verlangt
`equalProcessRuntimeState(current, decision.before)` – `decision.before`
muss exakt dem tatsaechlich uebergebenen `current` entsprechen. Eine
geladene Altphase wie `Fermenting` kann daher **nicht** durch blosses
Umdeuten ("fachlich wie Boot behandelt") nach `RecoveryEvaluation`
wechseln; ein `before`, dessen `state`-Feld tatsaechlich `Fermenting` ist,
besteht die `from == Boot`-Pruefung nicht. Revision 3 hat genau das
versucht und ist damit nicht ausfuehrbar.

**Vertrag:** Ein neuer, eng begrenzter Reason mit einer eigenen,
tatsaechlich zu den restaurierten Altphasen passenden Topologieregel:

```cpp
// process_state_machine.hpp
enum class TransitionReason : std::uint8_t {
    ..., RecoveryReentryRequired,
};
```

```cpp
// validControlTopology(), neuer Zweig – erlaubte Quellzustaende sind exakt
// die recovery-faehigen aktiven Phasen, wiederverwendet aus der bereits
// bestehenden Praedikatfunktion stateUsesRunSnapshot() (Preheating,
// WaitingForProduct, ReachingTarget, QualifyingTarget, Fermenting, Cooling,
// CoolHolding, ManualHolding; verifiziert process_state_machine.cpp:76-97).
case TransitionReason::RecoveryReentryRequired:
    return stateUsesRunSnapshot(from) && to == ProcessState::RecoveryEvaluation;
```

**Ablauf (lokal, auf einer Kopie, keine RAM-Mutation vor Commit):**

1. `auto candidate = restoredState;` (aus `restoreRunPersistenceSnapshot()`,
   `candidate.processState.state` ist die echte alte Phase, z. B.
   `Fermenting`, mit dem echten `processRunSnapshot`).
2. `auto originalRestoredProcessState = candidate.processState;` (unveraenderte
   Kopie, dient in Abschnitt 5.5 als Grundlage fuer `request.recoveredState` –
   getrennt von `candidate.processState`, das durch Hop 1 mutiert wird).
3. `hop1.before = candidate.processState` (identisch zu `candidate.processState`,
   erfuellt `equalProcessRuntimeState` trivial). `hop1 = propose(candidate.processState,
   ProcessState::RecoveryEvaluation, TransitionReason::RecoveryReentryRequired,
   monotonicMillis)`. `propose()` setzt `after.stateEnteredAtMillis = monotonicMillis`
   (aktueller Boot), zeroed `targetReachStartedAtMillis`/`targetReachWarningIssued`
   (da `RecoveryEvaluation` keinen Zielfenster-Timer hat,
   `stateHasTargetReachTimer` liefert `false`, `process_state_machine.cpp:429-432`),
   und resettet `qualificationValidSinceMillis` (immer, unabhaengig vom
   Zielzustand, `process_state_machine.cpp:428`) – `runtimeShapeIsValid(after)`
   ist damit garantiert erfuellt.
4. `applyProcessTransition(candidate.processState, hop1, &*candidate.processRunSnapshot)`.
   `transitionMatchesRunSnapshot`: `stateUsesRunSnapshot(before)==true` (alte
   Phase), verlangt `stateMatchesRunSnapshot(before.state, *runSnapshot)` –
   erfuellt, da der echte restaurierte Snapshot verwendet wird;
   `stateUsesRunSnapshot(RecoveryEvaluation)==false`, der `after`-Abgleich
   entfaellt (`process_state_machine.cpp:356-367`). `ProcessSignals::criticalFault`
   ist `false` zu setzen (sonst Kurzschluss nach `Fault`,
   `process_state_machine.cpp:974`). Bei Fehlschlag: Abbruch, kein Schreiben,
   Coordinator bleibt in `LoadedActiveRun`/`BlockedIndeterminate`
   (Ausgangszustand), Ergebnis `RunPersistenceResultStatus::InvalidDecision`.
5. Ab hier: `candidate.processState.state == RecoveryEvaluation`,
   `transitionSequence == restoredState.processState.transitionSequence + 1U`.

Kein Vorwand, die Altphase sei `Boot`; die alte Phase bleibt ausschliesslich
ueber `originalRestoredProcessState` als expliziter Recovery-Kontext
verfuegbar und wird in Abschnitt 5.5 in `request.recoveredState` eingebracht
– kein direkter Zugriff auf `RunPersistenceCoordinator::current` vor Commit.

Test: `applyProcessTransition()` wird mit der **echten** geladenen Altphase
(nicht einem synthetischen Sollzustand) fuer jede recovery-faehige Phase
sowie negativ gegen einen vorgetaeuschten `Boot`-Quellzustand geprueft
(Testmatrix 8.1/8.2).

### 5.3 Recovery-Zeitfenster – berechenbarer Datenvertrag mit echten UTC-Ankern

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 3):** Ein
Zeitfenster laesst sich nicht aus zwei Bool-Werten berechnen; gleichzeitig
verbietet der Boot-Epochen-Befund (weiterhin gueltig: `runtimeTimeIsValid()`
kennt keine Boot-Epoche, ein roh kopierter alter Zeitstempel wuerde nach
Neustart als "Zukunft" erkannt) jeden direkten Vergleich zweier
`monotonicMillis`-Werte aus unterschiedlichen Boots. Die einzige sichere
Bruecke ueber die Bootgrenze ist eine **echte UTC-Differenz**, wenn beide
Seiten sie liefern.

```cpp
// run_recovery_time.hpp
struct RecoveryTimeBoundsInput {
    std::uint64_t oldStateEnteredAtMillis;      // restaurierter processState, alter Boot
    std::uint64_t oldCheckpointMonotonicMillis; // aus Snapshot, alter Boot, >= oldStateEnteredAtMillis
    std::optional<std::int64_t> utcAtLastCheckpoint;  // RunPersistenceRawRecord.utcUnixSeconds
    std::optional<std::int64_t> utcNowAfterRestart;   // aktuelle Zeitquelle, einmalig zum Recoveryzeitpunkt abgefragt
};

struct RecoveryTimeBounds {
    std::uint64_t elapsedSecondsLowerBound;
    std::optional<std::uint64_t> elapsedSecondsUpperBound;  // nur gesetzt, wenn beide UTC-Anker vorhanden und nowUtc >= lastUtc
    bool exact;  // true genau dann, wenn upperBound gesetzt und == lowerBound
};

[[nodiscard]] RecoveryTimeBounds computeRecoveryTimeBounds(
    const RecoveryTimeBoundsInput& input);
```

**Berechnung:**
- `elapsedSecondsLowerBound = (oldCheckpointMonotonicMillis - oldStateEnteredAtMillis) / 1000`
  – reine **Alt-Boot-interne** Subtraktion (beide Werte aus demselben Boot),
  kein Boot-uebergreifender Vergleich. Dies ist die mindestens verstrichene
  Zeit, unabhaengig von der Ausfalldauer (Ausfall traegt niemals negativ
  bei).
- Sind `utcAtLastCheckpoint` und `utcNowAfterRestart` **beide** vorhanden und
  `utcNowAfterRestart >= utcAtLastCheckpoint`: `outageSeconds =
  utcNowAfterRestart - utcAtLastCheckpoint`; die Gesamtzeit ist dann exakt
  bekannt: `elapsedSecondsLowerBound = elapsedSecondsUpperBound =
  elapsedSecondsLowerBound(oben) + outageSeconds`, `exact = true`.
- Fehlt ein UTC-Anker oder ist `utcNowAfterRestart < utcAtLastCheckpoint`
  (Uhr ging zurueck – als unbekannt behandelt, nicht als negative Dauer):
  `elapsedSecondsUpperBound = nullopt`, `exact = false`. **Keine erfundene
  Ausfalldauer ohne Anker.**
- Keine „rebased monotonic“-Fiktion zwischen zwei Boots; die einzige
  Bootgrenzen-ueberbrueckende Groesse ist die UTC-Differenz.

Ausgefallene/verspaetete Kontrollpunkte werden **nicht** als eigener,
separat gespeicherter Fakt behandelt, sondern folgen direkt aus
`elapsedSecondsLowerBound` im Verhaeltnis zum konfigurierten
Checkpoint-Intervall (`RunPersistenceSnapshot.intervalMinutes`,
`RunPersistenceSnapshot.trigger`) – z. B. als Konfidenzhinweis fuer die
Anzeige (5.11), nie als harte Grenze und nie als erfundene Tatsache ohne
Datengrundlage.

### 5.4 Getrennte fachliche Bewertung, mehrere zeitabhaengige Recoverygrenzen

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 3):** Eine
Funktion, die sowohl das Zeitfenster berechnet als auch bereits ein
fachliches Verdikt (inkl. eines `configuredWaitingForProductLimitMinutes`-
Parameters) liefert, vermischt Datenermittlung und Geschaeftsentscheidung
wieder. Ausserdem deckt ein reiner `WaitingForProduct`-Fokus nicht alle
zeitabhaengigen Recoverygrenzen ab, die `ProcessRunSnapshot` tatsaechlich
traegt (`fermentationDurationMinutes`, `holdDurationMinutes` bei
`CompletionMode::CoolAndHoldForDuration`).

**Vertrag:** `computeRecoveryTimeBounds()` (5.3) liefert ausschliesslich
Zeitdaten. Eine separate, reine Vergleichsfunktion:

```cpp
enum class RecoveryTimeVerdict : std::uint8_t {
    DefinitelyStillValid,
    DefinitelyExpired,
    Uncertain,
};

[[nodiscard]] RecoveryTimeVerdict evaluateRecoveryTimeVerdict(
    const RecoveryTimeBounds& bounds, std::uint32_t limitSeconds) {
    if (bounds.elapsedSecondsLowerBound >= limitSeconds) {
        return RecoveryTimeVerdict::DefinitelyExpired;
    }
    if (bounds.elapsedSecondsUpperBound.has_value() &&
        *bounds.elapsedSecondsUpperBound < limitSeconds) {
        return RecoveryTimeVerdict::DefinitelyStillValid;
    }
    return RecoveryTimeVerdict::Uncertain;
}
```

Liegen beide Bounds auf derselben Seite der Grenze, ist das Ergebnis
eindeutig, auch ohne exakten UTC-Anker (`lowerBound >= limit` allein
reicht fuer `DefinitelyExpired`; ein bekannter `upperBound < limit` allein
reicht fuer `DefinitelyStillValid`). Nur wenn die Grenze zwischen
`lowerBound` und einem ggf. unbekannten `upperBound` liegt, ist das
Ergebnis `Uncertain`.

**Anwendung je Phase (fachliche Zuordnung, nicht Teil der reinen
Vergleichsfunktion):**

| Phase | Grenze | Verwendung des Verdikts |
|---|---|---|
| `WaitingForProduct` | `maximumProductWaitMinutes * 60` | steuert Hop-2-Ausgang direkt: `DefinitelyExpired` -> Tombstone (5.6), `DefinitelyStillValid`/hinreichend fuer Resume -> `RecoveryResume`, `Uncertain` -> Hop-1-only (5.7) |
| `Fermenting` | `fermentationDurationMinutes * 60` | rein informativ fuer die Rebasing-Richtung (5.5); blockiert **nicht** den Resume – ein ueberfaelliger Fermenting-Lauf resumiert und wird durch die bereits bestehende `decideFermenting`-Logik beim naechsten automatischen Aufruf reguär abgeschlossen |
| `CoolHolding` (nur `CompletionMode::CoolAndHoldForDuration`) | `holdDurationMinutes * 60` | wie `Fermenting`, rein informativ |
| alle anderen recovery-faehigen Phasen (`Preheating`, `ReachingTarget`, `QualifyingTarget`, `Cooling`, `ManualHolding`) | keine snapshot-getragene Grenze | keine Verdikt-Anwendung; einheitliches konservatives Rebasing (5.5) |

`WaitingForProduct` ist die einzige Phase, bei der ein `DefinitelyExpired`-
Verdikt den Resume verhindert (Sicherheits-/Produktrelevanz: ein
faelschlich fortgesetztes Warten auf ein Produkt ist die eigentliche Gefahr,
die #18 schliesst). Fuer `Fermenting`/`CoolHolding` ist ein ueberfaelliger,
aber fortgesetzter Lauf unkritisch, da die bereits bestehende
`decideAutomatic`-Logik (`decideFermenting`/`decideCoolHolding`,
`process_state_machine.cpp:639-677`) ihn beim naechsten regulaeren Aufruf
korrekt abschliesst – #18 dupliziert diese Geschaeftslogik nicht.

### 5.5 Hop 2 – Wiederverwendung der bestehenden `decideRecoveryEvent`/`decideProcessTransition`-API

**Wichtiger Befund (in dieser Session neu verifiziert, in Revision 3 nicht
erkannt):** `decideRecoveryEvent()` (`process_state_machine.cpp:741-780`)
existiert bereits produktiv und ist bereits **oeffentlich ueber
`decideProcessTransition()` erreichbar**, sobald `current.state ==
RecoveryEvaluation` gilt (Dispatch in `decideExplicitEvent()`,
`process_state_machine.cpp:840-842`). Sie nimmt `request.recoveredState`
(ein vollstaendiges `ProcessRuntimeState`) entgegen, validiert es
vollstaendig (`validRecoveryTarget`, `runtimeShapeIsValid`,
`runtimeTimeIsValid`, `stateMatchesRunSnapshot`, sowie einen eingebauten
`WaitingForProduct`-Ablaufcheck) und setzt bei Erfolg `decision.after =
recovered`. `RecoveryReject` (`request.event ==
ProcessEvent::RecoveryReject`) ist bereits vollstaendig implementiert
(`RecoveryEvaluation -> Fault`). Hop 2 muss diese Logik daher **nicht**
neu erfinden, sondern korrekt aufrufen.

**Rebasing von `request.recoveredState` (einzige noetige Vorarbeit):**
Ausgehend von `originalRestoredProcessState` (5.2, Schritt 2) wird **nur**
`stateEnteredAtMillis` neu gesetzt, alle anderen Felder (`state`,
`qualificationValidSinceMillis`, `targetReachStartedAtMillis`,
`targetReachWarningIssued`) bleiben unveraendert aus der Restaurierung
(`transitionSequence` wird von `decideRecoveryEvent` selbst auf
`current.transitionSequence + 1U` gesetzt, `process_state_machine.cpp:778`
– bezogen auf den **Hop-1-Kandidaten**, also insgesamt `alt + 2`, wie in
5.2 hergeleitet):

- **Alle Phasen ausser `WaitingForProduct`:** konservatives Rebasing mit
  `elapsedSecondsLowerBound` (5.3/5.4): `rebasedStateEnteredAtMillis =
  newBootMonotonicMillisNow - bounds.elapsedSecondsLowerBound * 1000`.
  Diese Richtung kreditiert nie mehr Ausfallzeit, als durch Alt-Boot-lokale
  Daten allein belegt ist – konsistent mit Gate C (keine ungesicherte
  Zeitgutschrift) und sicher fuer `Fermenting`/`CoolHolding` (verzoegert
  hoechstens eine ohnehin unkritische automatische Nachbewertung).
- **`WaitingForProduct` mit Verdikt `DefinitelyStillValid`:** Rebasing mit
  `elapsedSecondsUpperBound` (in diesem Fall garantiert vorhanden, da die
  Verdikt-Funktion `DefinitelyStillValid` nur liefert, wenn `upperBound`
  gesetzt ist) – die konservativere Richtung fuer **diese** Phase, da ein zu
  niedrig angesetztes `stateEnteredAtMillis` eine bereits abgelaufene
  Wartezeit verschleiern koennte.
- **`WaitingForProduct` mit `DefinitelyExpired`/`Uncertain`:** kein Rebasing,
  kein `RecoveryResume`-Versuch (siehe 5.6/5.7).

**Ausfuehrung:**

```cpp
TransitionRequest request;
request.event = ProcessEvent::RecoveryResume;
request.recoveredState = rebasedRecoveredState;
const auto hop2 = decideProcessTransition(
    candidate.processState /* == RecoveryEvaluation, nach Hop 1 */,
    &*candidate.processRunSnapshot,
    ProcessSignals{/* criticalFault = false */}, request, monotonicMillis);
```

Dies durchlaeuft `decideExplicitEvent -> decideRecoveryEvent` unveraendert;
liefert `hop2.status != Proposed` (z. B. weil die eingebaute
`WaitingForProduct`-Pruefung – als zusaetzliches, unveraendertes
Sicherheitsnetz – dennoch ablehnt, obwohl die eigene Vorpruefung
`DefinitelyStillValid` ergab), wird **kein** Resume erzwungen; Hop 2 gilt
als nicht durchfuehrbar und der Ablauf faellt auf die Hop-1-only-Behandlung
(5.7) mit einer Diagnosenachricht zurueck (fail-closed, kein Bypass des
bestehenden eingebauten Schutzes).

Bei Erfolg: `applyProcessTransition(candidate.processState, hop2, &*candidate.processRunSnapshot)`.

**Gate A-Kopplung:** Zwischen Hop 1 und dem Aufruf von Hop 2 wird die reale
Restart-Sensorauswahl (5.14) ausgewertet. Liefert sie kein bestaetigtes
Ergebnis, wird `request.event = ProcessEvent::RecoveryReject` statt
`RecoveryResume` verwendet – ebenfalls ueber die bestehende,
bereits implementierte `decideRecoveryEvent`-Logik (`RecoveryEvaluation ->
Fault`), keine eigene Fault-Konstruktion.

### 5.6 WaitingForProduct: definitiv abgelaufene Wartezeit – Tombstone

**Vertrag (Reason und Topologie unveraendert gegenueber Revision 3, hier
im Kontext des korrigierten Ablaufs neu verankert):** Da kein bestehender
Reason `RecoveryEvaluation -> Standby` erlaubt (`RecoveryRejected` ist hart
auf `Fault` fixiert, `process_state_machine.cpp:319-321`), bleibt ein
eigener Reason noetig:

```cpp
case TransitionReason::RecoveryEndedByExpiredWait:
    return from == ProcessState::RecoveryEvaluation &&
           to == ProcessState::Standby;
```

Anders als Hop 2 fuer `RecoveryResume`/`RecoveryReject` existiert fuer
diesen Ausgang **keine** wiederverwendbare bestehende Entscheidungsfunktion
– er wird direkt ueber `propose(candidate.processState, ProcessState::Standby,
TransitionReason::RecoveryEndedByExpiredWait, monotonicMillis)` konstruiert
und lokal angewendet, danach `clearActiveRunState(candidate)` **vor** der
einmaligen Persistierung (5.8). Ausloeser: alte Phase war
`WaitingForProduct` **und** `evaluateRecoveryTimeVerdict(bounds,
maximumProductWaitMinutes*60) == DefinitelyExpired` (5.4). Ergebnis:
`variant == NoActiveRun`; Coordinator erreicht `ReadyEmpty`, nicht `Ready`
(Commit-Kern, 5.8).

Test: Neustart nach definitiv abgelaufener Frist -> Reboot danach laedt
`NoActiveRun` (Testmatrix 8.3).

### 5.7 Unsichere Recovery – Hop-1-only, kein Dead-End

**Vertrag:** Bei `RecoveryTimeVerdict::Uncertain` (nur relevant fuer
`WaitingForProduct`, 5.4) oder wenn Gate A kein bestaetigtes Sensorergebnis
liefert, ohne dass eine sichere `RecoveryReject`-Entscheidung fachlich
angezeigt ist, wird **nur Hop 1** committet (`candidate.processState.state ==
RecoveryEvaluation`, kein Hop-2-Ergebnis). Dies ist eine vollstaendige,
fuer sich gueltige atomare Revision (5.8) – kein Zwischenschritt, der auf
einen weiteren, in derselben Operation folgenden Schritt angewiesen waere.
Coordinator-Ergebnis: `Ready` (ein Lauf mit gueltigem, aber unentschiedenem
Zustand existiert; `RecoveryEvaluation` traegt keine Aktorfreigabe).

**Zwei gleichwertige, spaetere Aufloesungswege (beide ueber denselben
Commit-Kern, 5.8):**

1. **Automatisch, sobald UTC verfuegbar wird:** `RunRecoveryCoordinator::reevaluatePendingRecovery(RunCommandState&, const RunCheckpointTime&)`
   (neu, nativ testbar; produktive Verdrahtung eines Aufrufers ist **nicht**
   Teil von #18, siehe 5.16/Gate B). Wiederholt 5.3-5.5 gegen
   `current.processState.state == RecoveryEvaluation` und `originalRestoredProcessState`
   (muss dafuer weiterhin verfuegbar sein – wird deshalb Teil des
   persistierten Recovery-Kontexts, 5.9).
2. **Benutzerpfad:** `CommandKind::ResolveRecoveryUncertainty` (unveraendert
   aus Revision 3 im Grundprinzip): typisierte Entscheidung
   (`AssumeStillValid`/`AssumeThresholdCrossed`), laeuft ueber die
   bestehende `persistCommand`-Infrastruktur (`expectedRunRevision`,
   `persistedIds_`-Dedup, `StaleState`/`AlreadyProcessed`), darf das
   berechnete Fenster nur innerhalb der ausgewiesenen Unsicherheit
   auflegen (Ablehnung, wenn `bounds`-Verdikt bereits definitiv ist).
   `AcknowledgeMessage` bleibt reine Quittierung.

Beide Wege fuehren **denselben** Hop-2-Aufbau (5.5/5.6) aus und persistieren
ueber denselben Commit-Kern (5.8) – keine zweite Entscheidungslogik.

**Kein Dead-End (Korrektur gegenueber Revision 3):** Revision 3 liess
`FallbackRecovered` bei `Uncertain` im Zustand
`BlockedIndeterminate/FallbackRecovery`, aus dem weder der automatische
noch der Benutzerpfad erreichbar waren, da beide einen bereits committeten
`RecoveryEvaluation`-Zustand voraussetzen. Die Korrektur: **Hop 1 wird
immer versucht**, unabhaengig davon, ob die Quelle `Current` oder
`FallbackRecovered` war, und **immer** atomar committet, sobald er lokal
erfolgreich aufgebaut werden konnte (5.8). Der Ursprung (`Current` vs.
`FallbackRecovered`) hat danach keine Bedeutung mehr fuer den weiteren
Ablauf – beide konvergieren auf denselben `Ready`+`RecoveryEvaluation`-
Zustand, aufloesbar ueber dieselben zwei Wege oben. `BlockedIndeterminate`
bleibt ausschliesslich reserviert fuer den Fall, dass **Hop 1 selbst** lokal
nicht aufgebaut/angewendet werden kann (z. B. eine strukturell defekte
Snapshot-Form) – das ist eine echte Datenintegritaetsfrage, keine
Zeitunsicherheit, und rechtfertigt weiterhin Fail-Closed (5.10).

### 5.8 Gemeinsamer Commit-Kern

Ein privater Coordinator-Helfer `commitRecoveryOutcome(RunCommandState& current,
const RunCommandState& candidateAfterHops, std::optional<std::size_t> targetSlotOverride,
const RunCheckpointTime& time)` kapselt: Projektion via
`makeRunPersistenceSnapshot`, Schreiben (siehe 5.10 fuer die
Fehlerfallunterscheidung), Bestimmung von `state_` (`ReadyEmpty` wenn
`variant == NoActiveRun`, sonst `Ready`), Uebernahme in `current` erst nach
bestaetigtem Schreiben. Aufrufer: `activateLoadedRun` (Quelle: direkt
geladen), `activateFallbackRecoveredRun` (Quelle: Fallback, mit
`targetSlotOverride = currentHead_->current.slot` – die bestehende
Standardableitung `target = 1 - currentHead_->current.slot`
(`run_persistence_coordinator.cpp:307-308`) wuerde sonst den einzigen
gueltigen Fallback-Datensatz ueberschreiben, bevor der neue Datensatz
sicher committet ist; Test mit Store-Schnitt zwischen Slot- und
Kopfschreiben, Erweiterung von
`test_restart_after_prepared_or_slot_cut_is_interrupted`),
`resolveRecoveryOutcome` (automatischer UTC-Pfad, nur Hop-2-Ergebnis, kein
erneutes Hop 1) sowie der `ResolveRecoveryUncertainty`-Kommandozweig in
`persistCommand`. `writeSnapshot`s bestehende `Ready`/`ReadyEmpty`-
Vorbedingung bleibt fuer die Standard-Zielslot-Ableitung unveraendert; der
Override betrifft ausschliesslich den Fallback-Fall.

### 5.9 Persistierter Recovery-Kontext (fuer Hop-1-only-Faelle)

Damit der automatische Wiederholungspfad (5.7, Weg 1) und der
Benutzerpfad (5.7, Weg 2) nach einem Hop-1-only-Commit weiterhin Zugriff
auf `originalRestoredProcessState` und den zugehoerigen
`ProcessRunSnapshot` haben (auch nach einem zwischenzeitlichen Reboot,
falls die Aufloesung erst nach einem weiteren Neustart erfolgt), bleibt der
urspruengliche `ProcessRunSnapshot` unveraendert Teil von
`RunPersistenceSnapshot.processRunSnapshot` (bereits bestehendes Feld,
unveraendert). Zusaetzlich wird `originalRestoredProcessState` als
kompakter, optionaler Teil des Schema-3-Vertrags gefuehrt
(`pendingRecoveryOriginalState: std::optional<ProcessRuntimeState>`),
gesetzt bei jedem Hop-1-only-Commit, geloescht (auf `nullopt`) sobald Hop 2
erfolgreich committet. Kein Duplikat einer bereits vorhandenen kanonischen
Quelle – dieser Wert existiert nirgends sonst, sobald `candidate.processState`
bereits `RecoveryEvaluation` ist.

### 5.10 `BlockedIndeterminate` und `PersistenceCommittedApplyFailed` – getrennt behandelt

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 3):** Der
bestehende Coordinator unterscheidet bereits zwei verschiedene
Sicherheitszustaende, die Revision 3 faelschlich zusammenlegte:

- **Store-Schreibausgang unbestimmt** (`RunPersistenceStoreWriteResult::Indeterminate`
  an beliebiger Phase des Prepared-/Slot-/Head-Schreibens,
  `run_persistence_coordinator.cpp:427-431,452,466,477,486,500,518`):
  `enterBlockedIndeterminate()` -> Coordinator-Zustand `BlockedIndeterminate`,
  Ergebnis `RunPersistenceResultStatus::PersistenceIndeterminate` mit
  `RunPersistenceTechnicalReason::StoreOutcomeUnknown`. Wir wissen nicht, ob
  die Daten geschrieben wurden.
- **Commit bestaetigt, RAM-Apply schlaegt danach fehl**
  (`run_persistence_coordinator.cpp:617-625,676-685`): Coordinator-Zustand
  `PersistenceCommittedApplyFailed`, gleichnamiger Ergebnisstatus. Die
  durable Daten sind bekannt korrekt; nur die RAM-Projektion ist inkonsistent.

Der in 5.8 beschriebene Commit-Kern (und die
`ApplyRecoveryTimeCorrection`-Persistierung, 5.13) uebernehmen **beide**
bestehenden Zustaende unveraendert und getrennt: ein unbestimmter
Schreibausgang fuehrt zu `BlockedIndeterminate`/`StoreOutcomeUnknown`; ein
bestaetigter Commit mit fehlgeschlagenem RAM-Apply fuehrt zu
`PersistenceCommittedApplyFailed`. Beide Faelle werden in der Testmatrix
(8.15) einzeln, nicht gemeinsam, abgedeckt.

### 5.11 `RunProgressState`, Fortschrittsfortschreibung – realer Eigentuemer

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 3):**

1. `decideRunAdjustment()` (`run_commands.cpp:947-1033`) setzt bei einer
   Daueraenderung waehrend `Fermenting`
   (`durationChanged && current.processState.state == Fermenting`,
   Zeilen 1019-1023) `candidate.processState.stateEnteredAtMillis =
   request.envelope.monotonicMillis` – **ohne** die bis dahin verstrichene
   Zeit vorher in einen Fortschrittszaehler zu falten. Ohne Korrektur geht
   diese Zeit fuer jede spaetere Ableitung verloren.
2. `RunCommandState::runRevision` (`run_commands.hpp:331`) wird
   ausschliesslich durch **fachliche Kommando-Entscheidungen** erhoeht
   (`decideProgramStart`, `decideManualStart`, `decideStop`,
   `decideCompletion`, `decideRunAdjustment` – verifiziert exakt an den
   `++decision.after.runRevision`-Stellen `run_commands.cpp:673,744,841,927,1027`).
   `writeSnapshot()` erhoeht ausschliesslich seine **technischen**
   Kopf-/Checkpoint-Zaehler (`nextHeadRevision_`, `nextCheckpointRevision_`),
   **nicht** `RunCommandState::runRevision`. Die Behauptung aus Revision 3,
   jeder neue Boot erzeuge zwangslaeufig eine neue Recovery-Episode ueber
   `runRevision`, war falsch – Hop 1/Hop 2 (Transitionsbasiert, nicht
   kommandobasiert) beruehren `runRevision` gar nicht.

**Vertrag – ein neues, explizites Episodenfeld statt einer falschen
Annahme:**

```cpp
// run_commands.hpp (RunCommandState) und run_persistence_contract.hpp
// (RunPersistenceSnapshot, Schema 3) – dieselbe Semantik in RAM und
// persistiert, ueberlebt Restore.
std::uint32_t recoveryEpisodeRevision{0U};
```

Erhoeht **ausschliesslich** durch Hop 1 (`candidate.recoveryEpisodeRevision =
restoredState.recoveryEpisodeRevision + 1U`, als Teil des Hop-1-Kandidaten,
5.2). Kein anderer Pfad (weder gewoehnliche Kommandos noch Hop 2, noch der
Korrekturbefehl selbst) veraendert dieses Feld. Damit:

- **Neuer Reboot -> neue Episode:** jede neue Ausfuehrung von Hop 1
  erhoeht garantiert um genau 1, unabhaengig von `runRevision`.
- **Stabil ueber Hop-1-only (`Uncertain`) und spaeteren Hop 2:** Hop 2
  aendert `recoveryEpisodeRevision` nicht; die Episode bleibt bis zur
  naechsten Hop-1-Ausfuehrung (also bis zum naechsten Reboot, der erneut
  Recovery erfordert) identisch.
- **Fortschrittskorrektur (5.13):** traegt sowohl das bestehende
  `CommandEnvelope.expectedRunRevision` (schuetzt vor zwischenzeitlichen
  fachlichen Mutationen seit Berechnung der Korrektur) **als auch** ein
  neues `expectedRecoveryEpisodeRevision` (schuetzt vor einem
  zwischenzeitlichen weiteren Reboot/neuer Recovery-Episode) – beide
  Pruefungen zusammen, nicht nur eine.

**Fortschrittsfortschreibung – reale Aufrufpunkte, keine neue Schleife:**
Eine reine Funktion `deriveFermentingSecondsDelta(before: ProcessRuntimeState,
atMillis: uint64) -> uint32` (`before.state == Fermenting ? (atMillis -
before.stateEnteredAtMillis) / 1000 : 0`) wird an **drei** bestehenden
Punkten angewandt, jeweils unmittelbar bevor `stateEnteredAtMillis`
(fachlich) neu gesetzt wird:

1. Live-Phasenwechsel **aus** `Fermenting` (`persistTransition`, jeder
   Aufruf mit `decision.before.state == Fermenting`).
2. `decideRunAdjustment()`, unmittelbar **vor** Zeile 1021
   (`run_commands.cpp:1019-1023`): `if (durationChanged && current.processState.state
   == Fermenting) { candidate.runProgress.observedRunSeconds +=
   deriveFermentingSecondsDelta(current.processState, request.envelope.monotonicMillis);
   candidate.processState.stateEnteredAtMillis = request.envelope.monotonicMillis; }`
   – **vor** dem bestehenden Reset, damit keine Zeit verloren geht; keine
   Doppelzaehlung, da `stateEnteredAtMillis` danach sofort neu gesetzt wird
   und ein zweiter Aufruf desselben Deltas ab dem neuen Wert null ergaebe.
   Overflow-Schutz: `observedRunSeconds` ist `uint32_t`
   (`>136` Jahre Kapazitaet bei Sekundenaufloesung, praktisch nicht
   erreichbar; sattigende Addition dennoch als Grenzfallschutz vorgesehen).
3. Hop 1 (5.2), wenn `originalRestoredProcessState.state == Fermenting`:
   `deriveFermentingSecondsDelta(originalRestoredProcessState,
   restoredSnapshot.checkpointMonotonicMillis)` – ausschliesslich aus
   bereits persistierten Checkpointdaten, keine unterbrochene Buchfuehrung.

Keine separate RAM-only-Buchfuehrungsstruktur noetig: `stateEnteredAtMillis`
innerhalb von `ProcessRuntimeState` ist bereits die einzige Quelle fuer "seit
wann in dieser Phase", wird bereits an jedem Fold-Punkt korrekt neu gesetzt
und ist bereits boot-lokal korrekt (uebliche `propose()`-Semantik bzw. das in
5.2/5.5 definierte Rebasing). Der in Revision 3 geplante
`RunProgressAccountingRuntime`-Typ hatte keinen fachlich benoetigten
Aufrufpunkt und entfaellt ersatzlos.

`RunProgressState` (persistiert, Schema 3) bleibt entsprechend einfach:

```cpp
struct RunProgressState {
    std::uint32_t observedRunSeconds{0U};
    WeightedProgressStatus weightedStatus{WeightedProgressStatus::NotCalibrated};  // 5.15
};
```

Kein boot-lokales Feld ist je Teil dieser Struktur, weil kein
boot-lokaler Begleitwert mehr existiert, der eine Trennung erforderte.

### 5.12 Recovery-Sensorevidenz – expliziter Datenfluss, keine erfundene Quelle

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 3):** Die vier
bestehenden Checkpoint-Schreibpfade (`persistCommand`, `persistTransition`,
`persistSensorSelection`, `checkpointPeriodic`) erhalten aktuell **keine**
Sensor-Snapshots als Parameter; die Behauptung, es fliesse "bereits" eine
`SensorQualitySnapshot`-Instanz mit, war falsch (verifiziert: keiner der
vier Funktionskoepfe nimmt einen entsprechenden Parameter oder liest ihn
aus `RunCommandState`). Ausserdem existiert **kein** `SensorRole`-Typ im
Repository (verifiziert: keine Fundstelle); das etablierte Muster ist
`CrossRolePlausibilityContext { air, product, cooling: SensorQualitySnapshot }`
(`sensor_selection.hpp:45-54`) – drei feste, benannte Felder statt eines
Enum-/Array-Typs.

**Vertrag:**

```cpp
// run_persistence_contract.hpp, Schema 3 – KISS-gerecht, dem bestehenden
// Muster folgend, kein neuer SensorRole-Typ.
struct RoleTemperatureEvidence {
    std::optional<double> filteredCelsius;  // letzter fachlich gueltiger, gefilterter Wert
    device_platform::SensorQuality quality{device_platform::SensorQuality::Stale};
};
struct RecoveryTemperatureEvidence {
    RoleTemperatureEvidence air;
    RoleTemperatureEvidence product;
    RoleTemperatureEvidence cooling;
};
```

**Aktualisierungsregel (ein Owner, eine Funktion, keine zweite
Sensorqualitaetslogik):**

```cpp
void updateRoleEvidence(RoleTemperatureEvidence& evidence,
                        const device_platform::SensorQualitySnapshot& live) {
    evidence.quality = live.quality;
    if (live.quality == device_platform::SensorQuality::Valid &&
        live.filteredCelsius.has_value()) {
        evidence.filteredCelsius = live.filteredCelsius;
    }
    // Sonst: vorheriger filteredCelsius bleibt erhalten. Ein
    // Stale/Failed-Sensor verliert seinen letzten gueltigen Wert nicht
    // (RUN_PERSISTENCE.md: der letzte gueltige Wert wird fuer
    // Recovery/Diagnose benoetigt).
}
```

**Datenfluss:** Die vier bestehenden Checkpoint-Schreibpfade erhalten einen
neuen, optionalen Parameter `const CrossRolePlausibilityContext*
liveSensorEvidence` (nullptr zulaessig – bedeutet "keine frischere Evidenz
in diesem Zyklus verfuegbar", vorheriger Stand bleibt erhalten, kein
Zwang zu einer neuen Aufrufkette). Ist er gesetzt, wird
`updateRoleEvidence` fuer `air`/`product`/`cooling` auf den Kandidaten vor
dem Schreiben angewandt. Alle vier Pfade verwenden dieselbe Funktion – kein
Parallelpfad. Aufrufer, die bereits ueber eine aktuelle
`CrossRolePlausibilityContext` verfuegen (z. B. der Gate-A-Aufruf bei
Restart-Sensorauswahl, 5.14), uebergeben sie explizit; alle bestehenden
Testaufrufe der vier Funktionen werden auf die neue Signatur angepasst
(Commit-Umfang, Abschnitt 7).

`afterRestart`-Evidenz (Vor-/Nach-Vergleich fuer die Anzeige, 5.16) wird
**einmalig** beim Hop-1/Hop-2-Aufbau aus der zu diesem Zeitpunkt ohnehin
fuer Gate A benoetigten `CrossRolePlausibilityContext` uebernommen – kein
zweiter Erhebungsweg.

### 5.13 `ApplyRecoveryTimeCorrection` – episoden-/staleness-fest

- **Episodenidentitaet:** `recoveryEpisodeRevision` (5.11), nicht
  `runRevision`.
- **Genau eine Korrektur pro Episode:** neues Kommando
  `CommandKind::ApplyRecoveryTimeCorrection`; `RecoveryTimeCorrectionRecord
  { std::uint32_t appliedAtEpisodeRevision; std::int32_t appliedSecondsDelta; }`
  (Schema 3, optional). Zweiter Versuch mit identischem Inhalt fuer
  dieselbe Episode -> `AlreadyProcessed` (bestehende Dedup ueber
  `persistedIds_`); mit abweichendem Inhalt -> `NotAllowedInState`.
- **Kein Einfluss auf einen fortgeschrittenen/beendeten Lauf:**
  `CommandEnvelope.expectedRunRevision` **und** ein neues
  `expectedRecoveryEpisodeRevision`-Feld muessen beide mit `current`
  uebereinstimmen; sonst `StaleState`.
- **Write-before-apply:** exakt das bestehende `persistCommand`-Muster.
- **Fehlerfaelle:** getrennt nach 5.10 (`BlockedIndeterminate`/
  `StoreOutcomeUnknown` bei unbestimmtem Schreibausgang,
  `PersistenceCommittedApplyFailed` bei bestaetigtem Commit mit
  fehlgeschlagenem RAM-Apply).
- **Reboot-Idempotenz:** folgt aus Write-before-apply ohne Sonderfallcode.

### 5.14 Restart-Sensorauswahl-Aktivierung (Gate A)

Zwischen Hop 1 und Hop 2 (5.5) wird `SensorSelectionPhase::RestartRevalidationPending`
real bewertet: `evaluatePhase` erhaelt einen echten Fall statt
`reject(InvalidContext)`; `computeRestartSensorSelection`
(`sensor_selection.cpp:890-907`, aktuell Stub) wertet den persistierten
Sensorselektionszustand (`sensorSelection`, #21) gegen die aktuelle
`CrossRolePlausibilityContext` aus. Ein negatives Ergebnis fuehrt zu
`RecoveryReject` statt `RecoveryResume` (5.5) – bereits vor #18 als offener
Uebergabepunkt in `docs/RUN_PERSISTENCE.md` dokumentiert; #18 schliesst ihn.

### 5.15 `WeightedProgressStatus` – nur heute unterscheidbare Zustaende

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 3):** Ein
zweiwertiges Enum (`NotCalibrated`/`Unavailable`), dessen Testmatrix
zugleich verlangt, dass Release 1 durchgehend `NotCalibrated` bleibt, macht
`Unavailable` zu einem unbenutzten Zukunftswert – die vermiedene
vorsorgliche Modellierung in neuer Form. `docs/SENSOR_TUNING_COMMISSIONING.md`/
Issue #34 (verifiziert per `gh issue view 34`) nennt heute **kein**
Aktivitaetskennfeld als eigenes Akzeptanzkriterium; #34 liefert die
**Messgrundlage** (Referenzmessungen, Offsets, thermische Reaktion), nicht
bereits ein Gewichtungsmodell.

**Vertrag:** Ein einwertiges Enum, das genau die heute tatsaechlich
erreichbare Aussage traegt und explizit als Erweiterungspunkt fuer eine
spaetere, dann eigenstaendig zu planende Modellierung dokumentiert ist:

```cpp
enum class WeightedProgressStatus : std::uint8_t {
    NotCalibrated,  // einziger heute erreichbarer Wert: kein freigegebenes Modell
};
```

Ein spaeterer, tatsaechlich unterscheidbarer Wert (z. B. sobald ein
konkretes Commissioning-Ergebnis eine reale Berechnung ermoeglicht) ist
eine neue, eigenstaendig zu begruendende Erweiterung dieses Enums zu
gegebener Zeit – kein heute vorab reservierter toter Wert. Die reale
Aktivitaetsgewichtung bleibt #34 als Messgrundlagen-Voraussetzung
zugeordnet, ohne #34 faelschlich als bereits definierten Modelleigentuemer
darzustellen; das eigentliche Gewichtungsmodell (falls spaeter benoetigt)
gehoert einem dann zu benennenden eigenen Vorhaben.

### 5.16 Komposition/DI – kein erfundener Aufrufer

**Ausgangsbefund (verifiziert, korrigiert gegenueber Revision 3):** Der
aktuelle Produktionscode enthaelt keinen produktiven Aufrufer von
`RunPersistenceCoordinator::loadAndInitialize()` und keine bestehende
Verdrahtung eines UTC-Verfuegbarkeitssignals als Recovery-Trigger
(verifiziert: kein Fund ausserhalb von Zeitquellen-Implementierungen/Tests).
Revision 3s Behauptung eines "bestehenden Aufrufers" war falsch.

**Vertrag:** `RunRecoveryCoordinator` (`run_recovery.hpp/.cpp`) liefert
ausschliesslich eine nativ testbare API:
`activate(RunCommandState&, const RunCheckpointTime&) -> RunPersistenceResult`
(Hop 1 + bedingt Hop 2, initialer Aufruf nach `loadAndInitialize()`) und
`reevaluatePendingRecovery(RunCommandState&, const RunCheckpointTime&) ->
RunPersistenceResult` (spaetere Aufloesung bei zuvor `Uncertain`, 5.7 Weg 1).
Beide sind ueber Unit-Tests direkt aufrufbar. Die produktive Verdrahtung
(wann/durch wen `loadAndInitialize()` und `reevaluatePendingRecovery()`
tatsaechlich im laufenden Betrieb aufgerufen werden) ist ausdruecklich
**nicht** Teil von #18 und bleibt dem zustaendigen Composition-Issue
vorbehalten (Gate B). Kein Verweis auf einen nicht existierenden
bestehenden Caller.

### 5.17 ROADMAP-Korrektur (PR #103 bereits gemergt)

`docs/ROADMAP.md` fuehrte PR #103 nach dessen Merge weiterhin als aktive,
zu mergende Arbeit ("Owner reviewt und mergt den separaten
Markdown-only-PR"). Da PR #103 bereits gemergt ist, ist das falsch – die
Roadmap ist die einzige aktuelle Statusuebersicht und darf keinen bereits
abgeschlossenen PR als offen darstellen. Korrigiert (als Teil dieses
Plan-Commits, siehe Abschnitt 12): Zeile 1 bleibt #18/PR #102 als aktuelle
fachliche Arbeit; Zeile 2 stellt PR #103 als abgeschlossen dar und verweist
auf das weiterhin reale, ueber #29/`OPEN_POINTS.md` sichtbare
Ressourcen-Gate; #22 bleibt als naechste fachliche Arbeit nach #18 im
Abschnitt "Naechste fachliche Arbeit" unveraendert benannt.

### 5.18 #24-Abgrenzung

Fault-Klassen, SAFE_BOOT-Feinausbau und Fault-Reset-Ablauf bleiben #24
zugeordnet. #18 nutzt den bestehenden `Fault`-Zustand ausschliesslich ueber
die bereits implementierte `RecoveryReject`-Logik (5.5); es fuegt keine
neue Fault-Unterklassifikation hinzu.

## 6. Modul- und Abhaengigkeitsgrenzen

Alle neuen/aenderten Dateien liegen in `lib/fermentation_app/src/` und
haengen ausschliesslich von bestehenden `device_platform`-Ports und
bestehenden `fermentation_app`-Modulen ab (ADR-013 unveraendert
eingehalten). `device_platform::SensorQuality`/`SensorQualitySnapshot`
werden wiederverwendet (bereits bestehende, portable Typen); kein neuer
`SensorRole`-Typ.

## 7. Datei-/Commit-Aufschluesselung

| # | Commit | Inhalt |
|---|---|---|
| 1 | `feat(process-state-machine): RecoveryReentryRequired- und RecoveryEndedByExpiredWait-Topologie` | 5.2, 5.6 |
| 2 | `feat(persistence): Schema 3 – RunProgressState, RecoveryTemperatureEvidence, RecoveryTimeCorrectionRecord, recoveryEpisodeRevision, pendingRecoveryOriginalState` | 5.9, 5.11, 5.12, 5.13, 5.15; Migrationstests 1/2/3 |
| 3 | `feat(recovery): computeRecoveryTimeBounds, evaluateRecoveryTimeVerdict` | `run_recovery_time.hpp/.cpp` (5.3, 5.4) |
| 4 | `feat(sensor-selection): reale Restart-Reaktivierung` | Gate A / 5.14 |
| 5 | `feat(persistence-coordinator): Signaturerweiterung der vier Checkpoint-Schreibpfade um liveSensorEvidence` | 5.12 (inkl. Anpassung aller bestehenden Testaufrufe) |
| 6 | `feat(persistence-coordinator): commitRecoveryOutcome, activateLoadedRun (Hop 1 + bedingt Hop 2)` | 5.2, 5.5, 5.8, 5.10 |
| 7 | `feat(persistence-coordinator): activateFallbackRecoveredRun, Slot-Override` | 5.7 (Dead-End-Fix), 5.8 |
| 8 | `feat(persistence-coordinator): resolveRecoveryOutcome, ResolveRecoveryUncertainty` | 5.7 |
| 9 | `feat(run-commands): ApplyRecoveryTimeCorrection, AdjustRun-Zeitfaltung` | 5.11, 5.13 |
| 10 | `feat(recovery): RunRecoveryCoordinator (activate, reevaluatePendingRecovery)` | 5.16 |
| 11 | `docs: Anzeigevertrag, Ressourcenbudget, ROADMAP-Korrektur` | 5.17, Abschnitt 10 |

Jeder Commit ist einzeln kompilier- und testbar; vollstaendiger Lauf erst
nach Owner-Freigabe gemaess `docs/AGENT_WORKFLOW.md`.

## 8. Testmatrix

1. Hop 1 mit echter geladener Altphase (`Fermenting` u. a.) ->
   `applyProcessTransition` erfolgreich, `RecoveryEvaluation` erreicht.
2. Negativtest: Hop 1 mit vorgetaeuschtem `Boot`-Quellzustand (statt der
   echten Altphase) wird von der Topologiepruefung abgelehnt.
3. `computeRecoveryTimeBounds`: exakte UTC-Bruecke, nur unterer Bound ohne
   UTC, `Uncertain` bei grenzwertiger Lage.
4. `evaluateRecoveryTimeVerdict`: beide Bounds auf derselben Seite ->
   definitives Ergebnis auch ohne exakten UTC-Anker.
5. `WaitingForProduct` `DefinitelyExpired` -> Tombstone -> `ReadyEmpty` ->
   Reboot `NoActiveRun`.
6. `WaitingForProduct` `DefinitelyStillValid` -> Resume ueber
   `decideRecoveryEvent`, Rebasing mit oberer Grenze.
7. `WaitingForProduct` `Uncertain` -> Hop-1-only -> automatischer Pfad
   **und** Benutzerpfad loesen unabhaengig auf; `StaleState` bei
   zwischenzeitlichem `runRevision`- oder `recoveryEpisodeRevision`-Wechsel;
   `AlreadyProcessed` bei Wiederholung.
8. `Fermenting`/`CoolHolding` resumieren trotz `DefinitelyExpired`-Verdikt
   (kein Blockieren); anschliessende automatische Bewertung schliesst sie
   regulaer ab.
9. Gueltiger `FallbackRecovered`-Snapshot mit `Uncertain`-Zeitverdikt
   erreicht **keinen** Dead-End: Hop 1 committet, `Ready`+`RecoveryEvaluation`,
   danach ueber beide Wege (automatisch/Benutzer) aufloesbar.
10. Slot-Override: Store-Schnitt zwischen Slot- und Kopfschreiben im
    Fallback-Fall – urspruenglicher Fallback-Datensatz bleibt bis Commit
    erhalten.
11. `AdjustRun` mit Daueraenderung waehrend `Fermenting` faltet die
    Vor-Anpassungszeit exakt einmal in `observedRunSeconds`; kein
    Datenverlust, keine Doppelzaehlung bei nachfolgendem Phasenwechsel/
    Recovery.
12. Recovery-Episode: Hop 1 -> `recoveryEpisodeRevision` erhoeht; ein
    zwischenzeitlicher weiterer Reboot (neues Hop 1) erhoeht erneut; eine
    Korrektur mit altem `expectedRecoveryEpisodeRevision` wird `StaleState`,
    unabhaengig vom aktuellen `runRevision`.
13. `StoreOutcomeUnknown` (`BlockedIndeterminate`) und bestaetigter Commit
    mit RAM-Apply-Fehler (`PersistenceCommittedApplyFailed`) werden als
    zwei getrennte, nicht austauschbare Faelle getestet.
14. Sensorevidenz: `liveSensorEvidence == nullptr` laesst vorherigen Stand
    unveraendert; ein `Stale`/`Failed`-Update ueberschreibt `filteredCelsius`
    nicht; ein `Valid`-Update aktualisiert Wert und Qualitaet. Roundtrip
    ueber Schema 3 und Recovery.
15. `WeightedProgressStatus` bleibt in Release 1 durchgaengig
    `NotCalibrated`, Codec-Roundtrip getestet.
16. Schema-1/2/3-Current/Fallback-Matrix bleibt vollstaendig (Regression).
17. Alle bestehenden #20/#21-Sensor-/Sicherheitsregressionen bleiben gruen.
18. `docs/ROADMAP.md`/`git diff --check` nach Fertigstellung.

## 9. Safety-/Security-/Recovery-/Hardwaregrenzen

Keine Aktorfreigabe vor abgeschlossenem Hop 2 (`RecoveryEvaluation` traegt
keine Aktorfreigabe, unveraendert). Kein Schreiben vor vollstaendigem
lokalem Kandidatenaufbau. Kein Aktorpfad direkt aus `FallbackRecovered` vor
Hop 1. Reale Hardware-/NVS-Anbindung bleibt #29/#90 vorbehalten.

## 10. Ressourcen-/Betriebsbudget

Schema-3-Zuwachs: `RunProgressState` (5 Byte), `RecoveryTemperatureEvidence`
(3 Rollen a ~10 Byte = ~30 Byte, optional), `RecoveryTimeCorrectionRecord`
(8 Byte, optional), `recoveryEpisodeRevision` (4 Byte),
`pendingRecoveryOriginalState` (Groesse von `ProcessRuntimeState`, optional,
nur waehrend einer offenen `Uncertain`-Episode belegt). Gesamtzuwachs pro
aktivem Lauf-Snapshot deutlich unter dem bestehenden
`kMaximumCheckpointRecordBytes`-Budget, durch Migrationstest 8.16
abgesichert. Keine unbeschraenkten Logs, kein Rohmesswertverlauf.

## 11. SOLID/DRY/KISS

Hop 2 nutzt die bestehende, bereits implementierte `decideRecoveryEvent`/
`decideProcessTransition`-API vollstaendig wieder, statt sie zu duplizieren
– die groesste DRY-Verbesserung gegenueber Revision 3. `evaluateRecoveryTimeVerdict`
ist eine einzige, phasenunabhaengige Vergleichsfunktion, dreifach
angewandt (WaitingForProduct/Fermenting/CoolHolding) statt dreier
Spezialimplementierungen. `updateRoleEvidence` ist die einzige
Sensorevidenz-Aktualisierungsfunktion, von allen vier Checkpoint-Pfaden
gemeinsam genutzt. `RunProgressAccountingRuntime` (Revision 3) entfaellt
ersatzlos, da `stateEnteredAtMillis` bereits dieselbe Rolle erfuellt – ein
Typ weniger statt eines toten Typs mit nachtraeglich gesuchtem Eigentuemer.

## 12. Dokumentations-/Abschlussnachweis

- `docs/ROADMAP.md`: in diesem Plan-Commit korrigiert (5.17).
- `docs/RUN_PERSISTENCE.md`/`docs/RECOVERY_AND_INTERRUPTION.md`: werden im
  Umsetzungscommit (Nr. 11) um die in 5.3-5.15 vertraglich fixierten Punkte
  ergaenzt.
- `git diff --check`: nach Commit dieser Datei auszufuehren.
- **Remote-Verifikation (neu, Pflicht):** nach dem Push wird
  `origin/plan/issue-18-restart-weighted-progress` per frischem `git fetch`
  gelesen und mit dem lokalen `HEAD` sowie mit `gh api
  repos/ManuEngineer/ESP32-Fermentationsschrank/pulls/102` (`head.sha`)
  abgeglichen, bevor PR-Beschreibung/SESSION HANDOVER als aktuell gemeldet
  werden.

## 13. Pflichtaufgabenliste (fuer die Umsetzung, nicht Teil dieser Planungssession)

1. Commit 1-11 gemaess Abschnitt 7, je mit gezielten lokalen Tests.
2. Testmatrix Abschnitt 8 vollstaendig.
3. `docs/RUN_PERSISTENCE.md`/`docs/RECOVERY_AND_INTERRUPTION.md` nachfuehren.
4. SESSION HANDOVER vor Sessionende bei offenem PR, inkl. verifiziertem
   Remote-SHA.

## 14. Stop-Bedingung

Diese Revision 4 ist ein vollstaendiger, eigenstaendiger Plan. Nach Commit
dieser Datei: **anhalten**, `git push`, Remote-SHA verifizieren
(Abschnitt 12), PR-Beschreibung und SESSION HANDOVER aktualisieren. Keine
Implementierung. Kein `Ready for review`. Keine Remote-CI. Kein Merge.
Keine Branchloeschung.
