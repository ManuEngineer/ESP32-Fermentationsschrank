# Plan: Issue #18 – Wiederanlauf und temperaturgewichteter Fortschritt

## 1. Status

- Revision: **3** (ersetzt Revision 1 und Revision 2 vollstaendig; kein
  Abschnitt dieser Datei verweist auf eine fruehere Revision als weiterhin
  gueltig – jeder Vertrag steht hier eigenstaendig).
- Draft-PR: #102 (`plan/issue-18-restart-weighted-progress` -> `main`).
- Live-Issue: #18 ("[E2.3] Wiederanlauf und temperaturgewichteten Fortschritt
  implementieren").
- Plan-Basis: `main` = `17ab3f5399a066465298ac6871b965d176a38d32` (enthaelt
  PR #103, `docs/OPEN_POINTS.md`/`docs/ROADMAP.md`-Nachfuehrung). Der Branch
  wurde per regulaerem Merge (kein Rebase, kein Force-Push) auf diesen Stand
  gebracht; Merge-Commit `fc908d765dabf5c6d9bce4f1528899b12a72bec6`. Der
  Konflikt in `docs/ROADMAP.md` wurde kanonisch aufgeloest: die #18/PR-102-
  Statuszeile und die Epic-1-Ressourcengate-Zeile aus PR #103 sind beide
  erhalten. Der Branch ist danach 0 Commits hinter `main`
  (`git rev-list --left-right --count origin/main...HEAD` -> `0 4`).
- Dieser Plan implementiert noch nichts. Umsetzung beginnt erst nach
  Freigabe des exakten Commits dieser Datei durch den Owner.

## 2. Live-Status-Pruefung (Basis dieser Revision)

- `gh issue view 18`: Status weiterhin offen, Scope/Akzeptanzkriterien wie in
  Abschnitt 3 zitiert (Quelle: Issue-Body, unveraendert seit Revision 1/2).
- `gh pr view 102`: Draft, Base `main`, Head
  `plan/issue-18-restart-weighted-progress`, jetzt 0 Commits hinter `main`.
- `docs/ROADMAP.md`: aktualisiert (Abschnitt "Aktuelle Arbeit", Zeile 1) auf
  Revision-3-Stand; Zeile 2 (Epic-1-Ressourcengate aus PR #103) unveraendert
  uebernommen.
- `docs/OPEN_POINTS.md`: durch PR-#103-Merge automatisch konfliktfrei
  uebernommen; fuer #18 nicht inhaltlich relevant (betrifft Epic #3/#29).
- Abhaengigkeiten (aus Issue-Body): #10 (virtuelle Zeitquelle, gemergt),
  #14, #17 (Laufpersistenz/Kontrollpunkte, gemergt, PR #84), #20
  (Sensorqualitaet, gemergt, PR #95), #21 (Regelsensorauswahl/Ersatzbetrieb/
  Rueckkehrlogik, gemergt, PR #99). Alle Abhaengigkeiten sind auf `main`
  vorhanden; der vorliegende Code wurde fuer diese Revision erneut am
  tatsaechlichen `HEAD` verifiziert (Zeilenangaben unten beziehen sich auf
  `main` inkl. PR #103, unveraendert durch den Merge in diesem Bereich).

## 3. Owner-Entscheidungen (Gates)

Diese drei Punkte sind vom Owner bereits entschieden und werden in dieser
Revision nicht erneut aufgeworfen:

- **Gate A – Restart-Sensorauswahl wird real angewandt.** Der in #21
  vorbereitete, aktuell durch `computeRestartSensorSelection` (Stub,
  `sensor_selection.cpp:890-907`) fail-closed blockierte Uebergabepunkt wird
  in #18 tatsaechlich implementiert: ein geladener aktiver Lauf durchlaeuft
  bei Reaktivierung eine echte `evaluatePhase`-Bewertung mit
  `SensorSelectionPhase::RestartRevalidationPending` als gueltigem
  Dispatch-Fall (nicht laenger `reject(InvalidContext)`), bevor Regelung
  freigegeben wird.
- **Gate B – kein neuer allgemeiner Prozesszyklus.** #18 fuegt keine
  periodische Anwendungsschleife parallel zu bestehenden Aufrufpunkten
  hinzu. Jede in dieser Revision geplante Fortschreibung nutzt ausschliesslich
  bereits bestehende Aufrufpunkte (Transition, Checkpoint, Recovery).
- **Gate C – keine erfundene Aktivitaetskurve.** Es wird keine unkalibrierte
  biologische Aktivitaetskurve eingefuehrt; kein normaler Sekundenwert wird
  als "temperaturgewichtet" fehlbezeichnet; unsichere Ausfallzeit wird ohne
  freigegebenes Modell nicht biologisch gutgeschrieben
  (`docs/RECOVERY_AND_INTERRUPTION.md:270,275`,
  `docs/RUN_PERSISTENCE.md:109`).

## 4. Ziel und Nicht-Ziele

**Ziel:** Nach einem Neustart (Stromunterbrechung, Reset, Firmware-Update)
wird ein geladener aktiver Lauf sicher bewertet, korrekt fortgesetzt oder
korrekt beendet, mit nachvollziehbarem, ehrlich gekennzeichnetem Fortschritt
und ohne jede Aktorfreigabe vor abgeschlossener Bewertung.

**Nicht-Ziele (Release 1):** reale Hardware-/NVS-Anbindung (#29/#90); Fault-
Klassen/SAFE_BOOT-Feinausbau (#24); Web-/Anzeige-UI-Implementierung (nur
Domain-/Application-Vertrag); kalibrierte biologische
Temperatur-Aktivitaets-Gewichtung (Gate C, siehe Abschnitt 5.9); ein neuer
periodischer Anwendungszyklus (Gate B).

## 5. Bindende Fachvertraege

### 5.1 Modul- und Dateizuordnung

| Bereich | Datei(en) | Aenderungsart |
|---|---|---|
| Recovery-Zeitfenster | `lib/fermentation_app/src/run_recovery_time.hpp/.cpp` | neu |
| Recovery-Orchestrierung, Zwei-Hop-Aufbau, Fortschritts-/Evidenzableitung | `lib/fermentation_app/src/run_recovery.hpp/.cpp` | neu |
| Zustandsautomat: neuer Reason, Topologie | `lib/fermentation_app/src/process_state_machine.hpp/.cpp` | erweitert |
| Persistenzvertrag: Schema 3, `RunProgressState`, `RecoveryTemperatureEvidence`, `RecoveryTimeCorrectionRecord` | `lib/fermentation_app/src/run_persistence_contract.hpp/.cpp` | erweitert |
| Codec: Schema-3-Gate | `lib/fermentation_app/src/run_persistence_codec.cpp` | erweitert |
| Coordinator: `activateLoadedRun`, `activateFallbackRecoveredRun`, `resolveRecoveryOutcome`, `blockedIndeterminateReason_` | `lib/fermentation_app/src/run_persistence_coordinator.hpp/.cpp` | erweitert |
| Kommandos: `ResolveRecoveryUncertainty`, `ApplyRecoveryTimeCorrection` | `lib/fermentation_app/src/run_commands.hpp/.cpp` | erweitert |
| Restart-Sensorauswahl (Gate A) | `lib/fermentation_app/src/sensor_selection.hpp/.cpp` | erweitert |
| Tests | `test/test_run_recovery_time/`, `test/test_run_recovery/`, `test/test_process_state_machine/`, `test/test_run_persistence_coordinator/`, `test/test_run_commands/`, `test/test_sensor_selection/` | neu/erweitert |

### 5.2 Lokale Recovery-Evaluation-Basis und Zwei-Hop-Replay (Punkt 2)

**Ausgangsbefund (verifiziert):**
`restoreRunPersistenceSnapshot()` (`run_persistence_contract.cpp`) liefert
ein `RunCommandState`, dessen `processState.state` die **alte**,
gespeicherte Prozessphase ist (z. B. `Fermenting`), nicht
`RecoveryEvaluation`. `decideRecoveryEvent()` ist nur korrekt adressierbar,
wenn `current.processState.state == ProcessState::RecoveryEvaluation` gilt
(vgl. `validControlTopology`, `process_state_machine.cpp:311-337`:
`RecoveryResumed`/`RecoveryRejected` verlangen beide
`from == ProcessState::RecoveryEvaluation`). `RecoveryEvaluation` ist gemaess
`validBootTopology()` (`process_state_machine.cpp:223-239`) ausschliesslich
ueber `TransitionReason::RecoveryRequired` mit `from == ProcessState::Boot`
gueltig erreichbar. Ein direkter Aufruf von `decideRecoveryEvent` gegen die
geladene Altphase wuerde daher in jedem Fall an der Topologiepruefung in
`applyProcessTransition` (`process_state_machine.cpp:1006-1021`) scheitern.

**Vertrag:** Der geladene Altzustand wird nicht direkt manipuliert. Der
Aufbau erfolgt in zwei lokalen, nacheinander auf eine Kopie angewendeten
Transitionen ("Hops"), beide ueber die bestehende Funktion `propose()` und
`applyProcessTransition()` konstruiert – **keine** Handmutation eines
Zielzustands ausserhalb der bestehenden Zustandsautomat-Vertraege.

1. **Lokale Kopie:** `auto candidate = restoredState;` (kein Zugriff auf
   `RunPersistenceCoordinator::current` oder sonstiges RAM ausserhalb dieser
   lokalen Variable).
2. **Hop 1 (immer, unbedingt):** Eine `TransitionDecision` mit
   `reason = TransitionReason::RecoveryRequired`,
   `before = candidate.processState` (dessen `state`-Feld fachlich als der
   Boot-Vorzustand behandelt wird – die geladene Altphase ist der Zustand
   *vor* der Neubewertung, funktional aequivalent zum bisherigen
   `Boot`-Fall, mit dem einzigen Unterschied, dass hier ein Lauf existiert),
   `after` via `propose(candidate.processState, ProcessState::RecoveryEvaluation, monotonicMillis, ...)`.
   Dies ist die einzige nach `validBootTopology` gueltige Art, in
   `RecoveryEvaluation` zu gelangen; sie wird hier bewusst fuer den
   Wiederanlauf-mit-Lauf-Fall wiederverwendet statt eine Parallelregel
   einzufuehren. `applyProcessTransition(candidate.processState, hop1, &*candidate.processRunSnapshot)`
   wird lokal ausgefuehrt und muss `true` liefern (sonst: Abbruch, siehe
   Fehlerpfad unten). `ProcessSignals::criticalFault` ist fuer beide Hops
   `false` zu setzen; `decideProcessTransition` wuerde sonst vor Erreichen
   des Dispatches nach `Fault` kurzschliessen
   (`process_state_machine.cpp:974`).
3. **Recovery-Zeitfenster bestimmen:** `computeRecoveryTimeBounds(...)`
   (Abschnitt 5.6) wird gegen den Zustand **vor** Hop 1 (den geladenen
   Altzustand) sowie die aktuelle Boot-Zeitbasis ausgewertet. Das Ergebnis
   ist reine Dateninterpretation, keine Zustandsmutation.
4. **Hop 2 (bedingt, siehe Abschnitt 5.3/5.4):** `decideRecoveryEvent(candidate.processState, bounds, ...)`
   wird jetzt korrekt gegen `candidate.processState.state == RecoveryEvaluation`
   (das Ergebnis von Hop 1) ausgewertet und liefert eine zweite
   `TransitionDecision` mit `reason` in
   `{RecoveryResumed, RecoveryRejected, RecoveryEndedByExpiredWait}` (neu,
   Abschnitt 5.3) oder liefert **keine** Entscheidung (Uncertain,
   Abschnitt 5.4). Falls vorhanden, wird sie sofort lokal auf `candidate`
   angewendet (`applyProcessTransition`), bevor irgendetwas geschrieben
   wird.
5. **Persistenz als eine atomare Revision:** Nur der vollstaendige, lokal
   fertig aufgebaute `candidate` wird ueber `makeRunPersistenceSnapshot(...)`
   projiziert und **einmalig** ueber den in Abschnitt 5.5/5.5.1
   beschriebenen neuen Schreibpfad (`writeSnapshot`, ein einziger Aufruf)
   persistiert. Der Kopfeintrag speichert
   `oldTransitionSequence = restoredState.processState.transitionSequence`
   und `newTransitionSequence = candidate.processState.transitionSequence`
   (nach zwei Hops: `alt + 2`). Verifiziert: `run_persistence_codec.cpp:936,1001,1067`
   prueft ausschliesslich `newTransitionSequence >= oldTransitionSequence`,
   keine exakte `+1`-Regel – ein `+2`-Sprung besteht die Kopfvalidierung
   unveraendert. Ein Test belegt diesen Sprung explizit im geschriebenen
   Kopf (Abschnitt 8, Testfall 1).
6. **Erst nach erfolgreichem Commit** wird `RunPersistenceCoordinator::current`
   auf `candidate` gesetzt und `state_` entsprechend des Ergebnisses
   (`Ready` oder `ReadyEmpty`, Abschnitt 5.3) uebergefuehrt – niemals vorher.
   Schlaegt Hop 1 oder Hop 2 lokal fehl (Schritt 2/4 liefert `false`), bricht
   der gesamte Aufbau ab, ohne dass RAM oder Persistenz veraendert werden;
   der Coordinator bleibt in `LoadedActiveRun`, das Ergebnis wird als
   `RunPersistenceResultStatus::InvalidDecision` gemeldet (fail-closed, kein
   stiller Blockierzustand ohne Diagnose).

Dieser Ablauf ersetzt jede Vorstellung eines direkten
`decideProcessTransition(..., RecoveryResume, ...)`-Aufrufs gegen die
Altphase vollstaendig.

### 5.3 WaitingForProduct: definitiv abgelaufene Wartezeit (Punkt 3)

**Ausgangsbefund (verifiziert):** `validPhaseTopology()`
(`process_state_machine.cpp:260-309`) erlaubt `ProductWaitExpired` nur als
`WaitingForProduct -> Standby` (Live-Pfad). `validControlTopology()`
(`process_state_machine.cpp:311-337`) erlaubt fuer `RecoveryRejected`
ausschliesslich `RecoveryEvaluation -> Fault`; kein bestehender Reason
erlaubt `RecoveryEvaluation -> Standby`. Der bestehende Live-Pfad in
`RunPersistenceCoordinator::persistTransition`
(`run_persistence_coordinator.cpp:643-696`) behandelt `ProductWaitExpired`
bereits mit `clearActiveRunState(candidate/current)` (Zeilen 662-663,
687-688), setzt dabei aber `state_` **nicht** auf `ReadyEmpty` um – diese
bestehende Live-Pfad-Konvention (Coordinator bleibt `Ready`, Aktivstatus wird
ueber `activeProgramRun`/`activeManualRun` geprueft, vgl.
`checkpointPeriodic`, `run_persistence_coordinator.cpp:705-707`) ist
etabliert, ausserhalb des #18-Scopes und wird **nicht** veraendert.

**Neuer Reason:** `TransitionReason::RecoveryEndedByExpiredWait` (neuer
Enum-Wert). Begruendung fuer einen neuen statt eines wiederverwendeten
Reasons: `RecoveryRejected` ist im bestehenden Vertrag semantisch "Fortsetzung
nicht sicher moeglich -> Fault" (Diagnosepflicht, #24-Anschluss); ein
definitiv abgelaufener Produktwarte-Zustand ist dagegen ein **regulaeres,
sicheres Laufende**, kein Fehler. `ProductWaitExpired` selbst kann nicht
wiederverwendet werden, da seine Topologie hart auf
`from == WaitingForProduct` fixiert ist (Live-Pfad) und ein weiterer
`from == RecoveryEvaluation`-Fall unter demselben Reason die bestehende,
bereits getestete Live-Pfad-Bedeutung verwaesserte.

**Neue Topologieregel** (`validControlTopology`, ergaenzter `switch`-Zweig):

```cpp
case TransitionReason::RecoveryEndedByExpiredWait:
    return from == ProcessState::RecoveryEvaluation &&
           to == ProcessState::Standby;
```

**Vorbedingung fuer diesen Hop-2-Ausgang:** Die geladene Altphase (vor Hop 1)
war `WaitingForProduct`, **und** `computeRecoveryTimeBounds` (Abschnitt 5.6)
liefert `RecoveryTimeVerdict::DefinitelyExpired` fuer die konfigurierte
Produktwarte-Frist. Andere Phasen erreichen diesen Ausgang nicht (keine
Topologieerlaubnis fuer andere `from`-Kombinationen unter diesem Reason).

**Atomare Tombstone-Bedingung:** Innerhalb desselben lokalen Hop-2-Schritts
(Abschnitt 5.2, Schritt 4) wird `clearActiveRunState(candidate)`
**vor** der einmaligen Persistierung (Schritt 5) angewendet – nicht als
separater zweiter Schreibvorgang. Das persistierte Ergebnis hat
`variant == RunCheckpointVariant::NoActiveRun`; es ueberlebt keine
`Standby`-Revision mit anhaengendem Laufschnappschuss.

**Coordinator-Zielzustand:** Anders als der bestehende Live-Pfad (der
`Ready` beibehaelt) setzt der in Abschnitt 5.5.1 beschriebene neue
Commit-Kern `state_ = ReadyEmpty`, wenn das Ergebnis
`variant == NoActiveRun` ist. Dies ist konsistent mit
`loadAndInitialize()`s eigener Konvention, `ReadyEmpty` immer dann zu
setzen, wenn zum Zeitpunkt der Klassifikation kein aktiver Lauf vorliegt
(`run_persistence_coordinator.cpp:257-259`), unabhaengig davon, ob das schon
beim Laden oder erst durch die Recovery-Entscheidung feststeht. Der Live-Pfad
(`persistTransition`) wird nicht angepasst; die abweichende
Boot-Zeit-Konvention ist beabsichtigt und wird in Abschnitt 12 (SOLID/DRY)
begruendet.

Test: Neustart nach definitiv abgelaufener `WaitingForProduct`-Frist ->
ein Reboot danach laedt `RunPersistenceLoadStatus::NoActiveRun` (kein
Laufschnappschuss ueberlebt), siehe Testmatrix 8.3.

### 5.4 Unsichere Recovery – typisierter Benutzerentscheidungsvertrag (Punkt 4)

**Ausgangsbefund:** Wenn `computeRecoveryTimeBounds` weder
`DefinitelyStillValid` noch `DefinitelyExpired` liefern kann (fehlende UTC,
Zeitfenster ueberlappt die Unsicherheitsgrenze), darf Hop 2 nicht
selbststaendig entschieden werden. Eine reine `DecisionRequired`-Nachricht
ohne spaeteren Aufloesungsmechanismus ist keine Recovery-Entscheidung.

**Vertrag:** Bei `RecoveryTimeVerdict::Uncertain` wird **nur Hop 1**
persistiert (Abschnitt 5.2, Schritt 5 mit `candidate.processState.state ==
RecoveryEvaluation`, keine Hop-2-`TransitionDecision` vorhanden). Das ist
weiterhin "eine atomare Recovery-Revision" im Sinne von Abschnitt 5.2 –
lediglich eine einhoppige statt einer zweihoppigen. `state_` wird auf
`Ready` gesetzt (ein Lauf mit gueltigem, wenn auch unentschiedenem, Zustand
existiert; `RecoveryEvaluation` traegt in der bestehenden
Aktorvertrag-Zuordnung keine Aktorfreigabe – dieselbe Eigenschaft, die
bereits vor #18 fuer `RecoveryEvaluation` gilt und hier unveraendert
genutzt, nicht neu eingefuehrt wird). Eine `DecisionRequired`-Nachricht wird
weiterhin erzeugt (Diagnose/Anzeige), ersetzt aber nicht den unten
beschriebenen Aufloesungsmechanismus.

**Automatischer Aufloesungspfad (bestehend, weiterhin gueltig):** Sobald die
Zeitquelle waehrend des laufenden Betriebs UTC-Verfuegbarkeit meldet (das
bestehende, bereits vor #18 vorhandene Zeitqualitaets-/UTC-Verfuegbarkeits-
Signal aus der virtuellen Zeitquelle, #10), wertet der bestehende Aufrufer
dieses Signals `computeRecoveryTimeBounds`/`decideRecoveryEvent` erneut
gegen `current.processState.state == RecoveryEvaluation` aus. Ergibt sich
jetzt ein definitives Verdikt, wird die resultierende Hop-2-
`TransitionDecision` ueber `RunPersistenceCoordinator::resolveRecoveryOutcome(...)`
(Abschnitt 5.5.1) als eigene, zweite atomare Revision committet. Dieser Pfad
existierte konzeptionell bereits in Revision 2 und wird hier nicht versteckt,
sondern als eine von zwei gleichwertigen Aufloesungsarten neben dem
Benutzerpfad benannt.

**Neuer Benutzerpfad (schmal, typisiert, testbar):**

```cpp
enum class RecoveryUncertaintyResolution : std::uint8_t {
    AssumeStillValid,
    AssumeThresholdCrossed,
};

// run_commands.hpp – neuer CommandKind-Wert (nicht ApplySensorSelectionAction,
// nicht AcknowledgeMessage; ein reines Recovery-Entscheidungskommando).
enum class CommandKind : std::uint8_t {
    ..., ResolveRecoveryUncertainty,
};

struct ResolveRecoveryUncertaintyInput {
    RecoveryUncertaintyResolution resolution;
};
```

`decideResolveRecoveryUncertainty(current, envelope, input, bounds)` (neue
Funktion, `run_commands.cpp`, gleicher Aufrufstil wie die bestehenden
`decide...`-Funktionen fuer andere `CommandKind`-Faelle):

- Nur gueltig, wenn `current.processState.state == RecoveryEvaluation` **und**
  `bounds.verdict == RecoveryTimeVerdict::Uncertain` (kein Ausweichen der
  berechneten sicheren Grenzen: `AssumeStillValid` wird abgelehnt, wenn
  `bounds.verdict == DefinitelyExpired`, und umgekehrt – die Entscheidung
  darf das berechnete Fenster nur innerhalb der als plausibel markierten
  Spanne aufloesen, nicht ausserhalb).
- Lauft ueber die **bestehende** `persistCommand`-Infrastruktur
  (`CommandEnvelope.expectedRunRevision`, `persistedIds_`-Dedup,
  `CommandStatus::StaleState`/`AlreadyProcessed`) – das liefert
  Wiederholungs-Idempotenz und Veraltungspruefung ohne neue Mechanik.
- Erzeugt intern dieselbe Hop-2-`TransitionDecision`
  (`RecoveryResumed`/`RecoveryEndedByExpiredWait`/`RecoveryRejected`) wie der
  automatische Pfad – **eine** Entscheidungsfunktion
  (`decideRecoveryEvent`), zwei Aufrufer. Persistiert wird ueber denselben
  in Abschnitt 5.5.1 beschriebenen Commit-Kern wie der automatische Pfad.
- `AcknowledgeMessage` bleibt unveraendert reine Quittierung einer
  Nachricht; es wird an keiner Stelle fuer diese Entscheidung
  zweckentfremdet.

### 5.5 FallbackRecovered – differenzierter Pfad (Punkt 5)

**Ausgangsbefund (verifiziert):** In `loadAndInitialize()`
(`run_persistence_coordinator.cpp:264-284`) wird beim erfolgreichen Lesen
eines gueltigen Fallback-Datensatzes `enterBlockedIndeterminate()` **vor**
der Rueckgabe von `{FallbackRecovered, fallbackRecord->snapshot}`
aufgerufen (Zeile 271-273) – identisch zum Pfad fuer echte, nicht
rekonstruierbare Zustaende (Zeile 279-280, 283-284). Der #17-Vertrag
unterscheidet eine gueltige, aber degradierte Fallback-Quelle explizit von
tatsaechlich unrekonstruierbaren Daten; die aktuelle Implementierung
verwischt diesen Unterschied auf Coordinator-Zustandsebene.

**Vertrag:**

1. `enterBlockedIndeterminate()` erhaelt einen neuen Parameter mit
   Default-Wert, sodass alle bestehenden Aufrufstellen unveraendert
   kompilieren:
   ```cpp
   enum class RunPersistenceIndeterminateReason : std::uint8_t {
       GenuinelyBroken,
       FallbackRecovery,
   };
   void enterBlockedIndeterminate(
       RunPersistenceIndeterminateReason reason =
           RunPersistenceIndeterminateReason::GenuinelyBroken);
   ```
   Neues privates Feld `blockedIndeterminateReason_`. Nur die eine
   Aufrufstelle im Erfolgspfad des Fallback-Lesens
   (`run_persistence_coordinator.cpp:271`) uebergibt
   `RunPersistenceIndeterminateReason::FallbackRecovery`; alle anderen
   Aufrufstellen (Zeilen 241, 279, 283 und weitere) bleiben unveraendert
   und melden weiterhin `GenuinelyBroken`.
2. **Kein automatischer Aktorfreigabepfad direkt aus dem Fallback.** Der
   Zustand bleibt `BlockedIndeterminate`; `blockedIndeterminateReason_ ==
   FallbackRecovery` ist ein zusaetzliches, oeffentlich abfragbares Merkmal
   (`state()`/ein neuer Accessor `blockedIndeterminateReason()`), kein
   eigener `RunPersistenceCoordinatorState`-Wert (keine Aufblaehung des
   Zustandsautomaten fuer einen Unterfall, der sich vollstaendig durch einen
   Grund plus die bestehende Zwei-Hop-Maschinerie abbilden laesst).
3. **Neue Coordinator-Methode `activateFallbackRecoveredRun(RunCommandState& current, const RecoveryActivationInput& input, const RunCheckpointTime& time)`.**
   Vorbedingung: `state_ == BlockedIndeterminate &&
   blockedIndeterminateReason_ == FallbackRecovery`. Fuehrt exakt denselben
   Zwei-Hop-Aufbau wie `activateLoadedRun` (Abschnitt 5.2-5.4) auf dem
   Fallback-Snapshot aus – **dieselbe** Zwei-Hop-Logik, kein Parallelpfad.
4. **Korrigierte Zielslot-Ableitung (kritisch fuer Fail-Closed-Sicherheit).**
   `writeSnapshot()`s bestehende Standardableitung
   `target = currentHead_.has_value() ? 1U - currentHead_->current.slot : 0U`
   (`run_persistence_coordinator.cpp:307-308`) wuerde im Fallback-Fall
   `currentHead_->fallback->slot` ergeben, da `currentHead_->current` weiterhin
   auf den **defekten** Slot zeigt und `currentHead_->fallback` bereits den
   **einzigen gueltigen** Datensatz belegt (`run_persistence_coordinator.cpp:264-273`).
   Ein Schreiben mit dieser Standardableitung wuerde den einzigen gueltigen
   Datensatz ueberschreiben, bevor der neue Datensatz sicher committet ist –
   ein Datenverlustpfad. `activateFallbackRecoveredRun` (und der davon
   genutzte Commit-Kern, Abschnitt 5.5.1) uebergibt daher explizit
   `target = currentHead_->current.slot` (den defekten Slot) an die interne
   Schreibfunktion, statt der Standardableitung zu folgen. Ein Test schneidet
   den Store exakt zwischen Slot-Schreiben und committiertem Kopf
   (Erweiterung des bestehenden Musters
   `test_restart_after_prepared_or_slot_cut_is_interrupted`) und belegt, dass
   der urspruengliche Fallback-Datensatz im unveraenderten Slot erhalten
   bleibt, bis der Kopf committet ist.
5. **Erfolg:** neue Current-Revision atomar geschrieben (wie 5.2, Schritt 5),
   `blockedIndeterminateReason_` zurueckgesetzt, `state_` auf `Ready`/
   `ReadyEmpty` wie in 5.3/5.4 hergeleitet.
6. **Verbleibende Unklarheit bleibt fail-closed:** Schlaegt der Zwei-Hop-Aufbau
   lokal fehl oder bleibt das Ergebnis `Uncertain` ohne dass ein Benutzer-
   oder automatischer Pfad (5.4) es aufloest, bleibt der Coordinator in
   `BlockedIndeterminate` mit `blockedIndeterminateReason_ ==
   FallbackRecovery` – kein neuer, staerkerer Zustand wird vorgetaeuscht.
7. Echte blockierende Zustaende
   (`NotReconstructible`, `NotReconstructibleOrphanedState`, `UnsupportedSchema`,
   `ForeignEpoch`, `PersistenceCommittedApplyFailed`) bleiben unveraendert
   blockierend; #18 eroeffnet fuer sie keinen Ausweg.

Tests: beschaedigter Current-Datensatz + gueltiger Fallback in Schema 1/2/3,
erfolgreiche Aktivierung ueber `activateFallbackRecoveredRun`; sowie die
bestehenden echten Fail-Closed-Faelle bleiben unveraendert blockierend
(Testmatrix 8.6).

#### 5.5.1 Gemeinsamer Commit-Kern (Wiederverwendung, keine Parallellogik)

Ein neuer privater Coordinator-Helfer
`commitRecoveryOutcome(RunCommandState& current, const RunCommandState& candidateAfterHops, std::optional<std::size_t> targetSlotOverride, const RunCheckpointTime& time)`
kapselt: Projektion via `makeRunPersistenceSnapshot`, Schreiben via
`writeSnapshot` (ggf. mit `targetSlotOverride`, Abschnitt 5.5 Punkt 4),
Bestimmung von `state_` (`ReadyEmpty` wenn
`candidateAfterHops`-Snapshot `variant == NoActiveRun`, sonst `Ready`),
Uebernahme in `current` erst nach erfolgreichem Schreiben. Er wird von vier
Stellen aufgerufen: `activateLoadedRun` (Zwei-Hop, definite Ausgaenge und
Uncertain-Ein-Hop-Fall), `activateFallbackRecoveredRun` (mit Slot-Override),
sowie – fuer die **abweichende** Situation "Hop 1 ist bereits `current`,
nur Hop 2 folgt spaeter" – `resolveRecoveryOutcome` (automatischer
UTC-Pfad) und der `ResolveRecoveryUncertainty`-Kommandozweig in
`persistCommand`. `writeSnapshot`s bestehende `Ready`/`ReadyEmpty`-Vorbedingung
(`run_persistence_coordinator.cpp:292-294`) bleibt fuer die
Standard-Zielslot-Ableitung unveraendert; der Override betrifft
ausschliesslich den in Abschnitt 5.5 Punkt 4 begruendeten Fallback-Fall.

### 5.6 Recovery-Zeitfenster-Datenvertrag (Punkt 6)

**Verifikation gegen den bestehenden #17-Vertrag statt neuer Annahmen:**
`RunPersistenceSnapshot` (`run_persistence_contract.hpp:88-110`) enthaelt
bereits: `trigger` (`RunCheckpointTrigger`: `Command`/`Transition`/
`Periodic`/`SensorSelection`), `checkpointMonotonicMillis`,
`intervalMinutes`. `RunPersistenceRawRecord`
(`run_persistence_contract.hpp:112-117`) enthaelt zusaetzlich
`utcUnixSeconds` (optional – Praesenz zeigt, ob beim letzten Checkpoint eine
UTC-Zeitquelle verfuegbar war).

**Ergebnis der Verifikation:** Alle vom Owner geforderten Mindestfaktoren
sind bereits kanonisch vorhanden, **keiner erfordert ein neues persistiertes
Feld**:

| Geforderter Faktor | Kanonische Quelle |
|---|---|
| Konfiguriertes Checkpoint-Intervall | `RunPersistenceSnapshot.intervalMinutes` |
| Letzter bekannter monotoner Laufzeitstand am Checkpoint | `RunPersistenceSnapshot.checkpointMonotonicMillis` |
| Zeitpunkt der letzten ereignisbezogenen Speicherung | `checkpointMonotonicMillis` (jeder Schreibvorgang – ob `Command`, `Transition`, `Periodic` oder `SensorSelection` ausgeloest – aktualisiert dieses Feld gemeinsam mit `trigger`; der zuletzt geschriebene Datensatz *ist* per Konstruktion die letzte Speicherung, unabhaengig vom Ausloeser) |
| Art der letzten Speicherung | `RunPersistenceSnapshot.trigger` |
| Ausgefallene/verspaetete Kontrollpunkte, soweit belegbar | abgeleitet, nicht separat gespeichert: die Differenz zwischen aktuellem Boot-Zeitpunkt und `checkpointMonotonicMillis` (siehe Rebasing, unten) ist bereits die belastbare obere Schranke der Ausfallzeit unabhaengig vom Ausloeser des letzten Checkpoints; `intervalMinutes` dient nur zur **Einordnung** (z. B. "Luecke > 3x Intervall" fuer die Vertrauensklassifikation in Abschnitt 5.6.1), nicht als harte Grenze |
| Zeitqualitaet vor der Unterbrechung | `RunPersistenceRawRecord.utcUnixSeconds` des zuletzt gelesenen Datensatzes (`has_value()` = UTC war beim letzten Checkpoint verfuegbar) |
| Zeitqualitaet nach der Unterbrechung | die beim aktuellen Boot bereits vorhandene Zeitquellenabfrage (dieselbe, die `RunCheckpointTime.utcUnixSeconds` fuer den laufenden Boot befuellt) – keine neue Abfragestelle |

Diese Tabelle ist die verbindliche Grundlage; sie ersetzt jede vorherige
Fassung, die das Zeitfenster allein aus `intervalMinutes` herleitete.

#### 5.6.1 `computeRecoveryTimeBounds` – finale Eingabestruktur

```cpp
// run_recovery_time.hpp
enum class RecoveryTimeVerdict : std::uint8_t {
    DefinitelyStillValid,
    DefinitelyExpired,
    Uncertain,
};

struct RecoveryTimeBoundsInput {
    std::uint64_t checkpointMonotonicMillis;   // aus Snapshot
    RunCheckpointTrigger lastTrigger;          // aus Snapshot
    std::uint16_t intervalMinutes;             // aus Snapshot
    bool utcAvailableBeforeOutage;             // aus RawRecord.utcUnixSeconds.has_value()
    bool utcAvailableAfterRestart;             // aus aktueller Zeitquelle
    std::uint64_t bootRebasedNowMillis;        // siehe 5.6.2, kein Boot-uebergreifender Rohvergleich
    std::optional<std::uint32_t> configuredWaitingForProductLimitMinutes; // Fachparameter, nur fuer 5.3 relevant
};

struct RecoveryTimeBounds {
    RecoveryTimeVerdict verdict;
    std::uint64_t elapsedSecondsLowerBound;
    std::uint64_t elapsedSecondsUpperBound;
    bool coveredByRegularCheckpoints;  // Luecke <= ~konfiguriertes Intervall (Einordnung, keine harte Grenze)
};

[[nodiscard]] RecoveryTimeBounds computeRecoveryTimeBounds(
    const RecoveryTimeBoundsInput& input);
```

`verdict` wird `Uncertain`, wenn entweder `utcAvailableBeforeOutage` oder
`utcAvailableAfterRestart` `false` ist **und** die elapsed-Spanne die fuer
die jeweilige Fachentscheidung (z. B. `configuredWaitingForProductLimitMinutes`
in 5.3) relevante Schwelle ueberlappt; ansonsten liefert rein
monotonie-basierte Betrachtung bereits ein definitives Verdikt (kurze,
eindeutig unter der Schwelle liegende Ausfallzeiten benoetigen keine UTC).
Diese Funktion trifft **keine** Geschaeftsentscheidung (kein `Standby`, kein
`Fault`) – sie liefert ausschliesslich das Zeitfenster; die Entscheidung
selbst bleibt in `decideRecoveryEvent` (Trennung Datenermittlung vs.
Geschaeftsentscheidung, wie vom Owner gefordert).

#### 5.6.2 Boot-lokale monotone Zeit – kein Boot-uebergreifender Rohvergleich

**Ausgangsbefund:** `runtimeTimeIsValid()` prueft nur, dass Zeitstempel
nicht in der Zukunft relativ zum uebergebenen `monotonicMillis` liegen; es
gibt kein Boot-Epochen-Konzept. Ein roh kopierter alter Zeitstempel
(`stateEnteredAtMillis` aus einem fruehen Boot) wuerde nach einem Neustart
(kleiner `monotonicMillis`) fast immer als "in der Zukunft" erkannt und
`decideProcessTransition` mit `TimeWentBackwards` scheitern lassen.

**Vertrag:** Beim Rueckschreiben des Zwei-Hop-Ergebnisses (Abschnitt 5.2)
werden alle Prozesszeit-Felder, die auf dem alten Boot beruhen
(`stateEnteredAtMillis`, `targetReachStartedAtMillis` sofern nach 5.7/5.8
relevant), im Rahmen von `propose()` **neu gesetzt** auf den aktuellen Boot
(`monotonicMillis` des laufenden Boots) statt roh uebernommen zu werden –
das entspricht exakt dem bestehenden Verhalten von `propose()`
(`process_state_machine.cpp`, setzt `after.stateEnteredAtMillis =
monotonicMillis` bei jedem Hop). Fuer `targetReachStartedAtMillis`/
`targetReachWarningIssued` gilt zusaetzlich die bestehende
`runtimeShapeIsValid`-Invariante: fuer Zustaende, in denen dieses Zeitfenster
nicht laeuft (u. a. `Fermenting`, `Cooling`, `CoolHolding`,
`ManualHolding`), muss `targetReachStartedAtMillis` exakt `0U` und
`targetReachWarningIssued` `false` sein (`process_state_machine.cpp:216-219`)
– ein roh uebernommener Altwert wuerde `decideRecoveryEvent`/
`applyProcessTransition` ablehnen; `propose()` setzt diese Felder bereits
korrekt zurueck, sofern die Zielzustandsklasse das verlangt.

Fuer die reine **Zeitfensterberechnung** (5.6.1) wird niemals ein alter
`monotonicMillis`-Wert direkt mit dem neuen Boot-`monotonicMillis`
verglichen; die einzige Boot-uebergreifende Groesse, die einfliesst, ist die
**Dauer seit dem letzten Checkpoint**, die nicht durch Subtraktion zweier
`monotonicMillis`-Werte unterschiedlicher Boots gebildet wird, sondern
ausschliesslich ueber die optionale UTC-Differenz (falls beidseitig
verfuegbar) oder – falls UTC fehlt – als "unbekannt, mindestens die seit
Prozessstart dieses Boots vergangene Zeit" in `Uncertain` muendet. Damit
entsteht kein Boot-uebergreifender Rohvergleich monotoner Werte.

### 5.7 `RunProgressState` – Trennung persistierbar/RAM-only (Punkt 7)

**Vertrag:** Zwei getrennte Strukturen statt einer:

```cpp
// run_persistence_contract.hpp – Teil des Wire-Formats (Schema 3),
// enthaelt ausschliesslich boot-unabhaengige Geschaeftswerte.
struct RunProgressState {
    std::uint32_t observedRunSeconds{0U};
    WeightedProgressStatus weightedStatus{
        WeightedProgressStatus::NotCalibrated};  // Abschnitt 5.9
};

// run_recovery.hpp – NICHT Teil des Wire-Formats, niemals serialisiert.
struct RunProgressAccountingRuntime {
    std::uint64_t lastAccountedMonotonicMillis{0U};
};
```

`RunProgressAccountingRuntime` lebt ausschliesslich im RAM neben
`RunCommandState` (nicht darin, um eine versehentliche Aufnahme in
`makeRunPersistenceSnapshot`s Projektion strukturell auszuschliessen) und
wird nach jedem Boot bzw. jeder Recovery-Aktivierung neu initialisiert
(`lastAccountedMonotonicMillis = candidate.processState.stateEnteredAtMillis`
nach Abschluss der Zwei-Hop-Aktivierung). Kein boot-lokaler Wert erreicht
jemals den Codec.

### 5.8 Vor-Ausfall-Laufzeit – realer Eigentuemer statt totem Feld (Punkt 8)

**Ausgangsbefund:** Eine Funktion, die in keinem Produktionszyklus
aufgerufen wird, ist keine implementierte Fortschreibung. Gate B verbietet
einen neuen periodischen Zyklus.

**Vertrag – zwei bestehende, bereits vorhandene Aufrufpunkte, keine neue
Schleife:**

1. **Live-Fortschreibung bei Phasenwechsel (bestehender Aufrufpunkt):**
   Ueberall dort, wo bereits heute `RunPersistenceCoordinator::persistTransition`
   fuer einen Phasenwechsel **aus** `ProcessState::Fermenting` heraus
   aufgerufen wird, wird vor der bestehenden Projektion zusaetzlich
   `observedRunSeconds += deriveFermentingSecondsDelta(decision.before, decision.monotonicMillis)`
   auf den Kandidaten angewandt. `deriveFermentingSecondsDelta(before, atMillis)`
   ist eine reine, ungebundene Funktion:
   `return before.state == ProcessState::Fermenting ? (atMillis - before.stateEnteredAtMillis) / 1000U : 0U;`
   Dies ist keine neue Schleife, sondern eine zusaetzliche Berechnung
   innerhalb eines bereits stattfindenden Commits, unabhaengig davon, ob der
   Phasenwechsel regulaer (Live) oder als Hop 2 im Recovery-Aufbau
   stattfindet.
2. **Recovery-Fortschreibung (Zwei-Hop-Aufbau, Abschnitt 5.2):** Wenn die
   geladene Altphase `ProcessState::Fermenting` war, wird beim Aufbau von
   Hop 1 dieselbe Funktion
   `deriveFermentingSecondsDelta(restoredState.processState, restoredSnapshot.checkpointMonotonicMillis)`
   angewandt – die Vor-Ausfall-Laufzeit wird ausschliesslich aus bereits
   persistierten Checkpointdaten hergeleitet (`checkpointMonotonicMillis`,
   Abschnitt 5.6), nicht aus einer waehrend des Ausfalls unterbrochenen
   Buchfuehrung.

Dieselbe Funktion an zwei bestehenden Aufrufpunkten – keine unbenutzte
Hilfsfunktion, kein neuer Zyklus. `observedRunSeconds` ist explizit als
"waehrend `Fermenting` beobachtete Sekunden, an jedem Phasenwechsel-Commit
und bei Recovery fortgeschrieben" skopiert (nicht als bruchstueckhafte,
nur-bei-Ausfall gepflegte Groesse); dies wird im Anzeigevertrag (5.10)
entsprechend benannt.

### 5.9 Temperaturgewichtung – ehrlicher, testbarer Status (Punkt 9, Gate C)

**Vertrag:** Kein `weightedProgressSeconds`-Zahlenfeld, das dauerhaft
`nullopt` bliebe. Stattdessen ein Status-Enum mit heutiger, testbarer
Bedeutung:

```cpp
enum class WeightedProgressStatus : std::uint8_t {
    NotCalibrated,  // heute immer dieser Wert: kein freigegebenes Modell
    Unavailable,    // Sensorlage liess auch eine konservative Bewertung nicht zu
};
```

`NotCalibrated` unterscheidet sich fachlich von "kein Fortschritt bekannt"
(`observedRunSeconds == 0`): es sagt aus, dass sichere, unkorrigierte
Sekunden bekannt sind, aber keine Gewichtung erfolgt ist. Das ist heute
bereits eine testbare, anzeigerelevante Aussage ("nicht kalibriert" statt
eines Zahlenwerts, Abschnitt 5.10) und rechtfertigt das Feld ohne Vorgriff
auf ein zukuenftiges Modell. Ein spaeterer numerischer Gewichtswert
erfordert bei Einfuehrung einen eigenen, dann fachlich begruendeten
Schema-Schritt (#34) – das ist eine normale zukuenftige
Schemaerweiterung fuer neue Funktionalitaet, kein vorsorgliches Freihalten
ohne heutige Bedeutung.

**Owner-Zuordnung:** Die reale Aktivitaetsgewichtung (kalibrierte
Temperatur-Aktivitaets-Kurve) wird #34 ("[E6.1] Sensorvergleich, Offsets
und thermische Grundvermessung") als vorgelagertem Messgrundlage-Gate
zugeordnet; #35 ("[E6.2] PI-Parameter ...") ist als angrenzendes
Sicherheitsgrenzen-Gate relevant, aber nicht der primaere Modelleigentuemer.
#18 stellt hierzu keine kalibrierte Aussage dar und behauptet das an keiner
Stelle.

### 5.10 Kanonische Recovery-Anzeige-/Persistenzdaten (Punkt 10)

Vollstaendige Zuordnung jedes von `docs/RUN_PERSISTENCE.md` geforderten
Recovery-Datums:

| Datum | Quelle/Owner |
|---|---|
| Verwendeter Regelsensor | `RunPersistenceSnapshot.activeRunSensorMode` (bestehend) |
| Sensor-Selektionsherkunft | `RunPersistenceSnapshot.sensorSelection` (bestehend, #21) |
| Zeitqualitaet vor/nach Ausfall | Abschnitt 5.6-Tabelle (bestehend, keine neuen Felder) |
| Letzte relevante Zustandsaenderung | `processState.stateEnteredAtMillis` + `transitionSequence` (bestehend) |
| Intervallgrenzen, Vertrauen (`coveredByRegularCheckpoints`) | `RecoveryTimeBounds` (Abschnitt 5.6.1, neu berechnet, nicht separat persistiert – reproduzierbar aus bereits persistierten Daten) |
| Fortschrittskorrekturen (angewandt/ausstehend) | `RecoveryTimeCorrectionRecord` (Abschnitt 5.11, **neu**, Schema 3) |
| Letzte gueltige Temperaturen/Qualitaetszustaende Air/Product/Cooling vor Ausfall | **neu**, Schema 3: `RecoveryTemperatureEvidence` (unten) |
| Erster gueltiger Temperaturwert nach Neustart | **neu**, Schema 3: derselbe `RecoveryTemperatureEvidence`-Datensatz, Feld `afterRestart` |

**Neue kompakte Schema-3-Struktur (kein unbeschraenktes Log, kein
Rohmesswertverlauf):**

```cpp
struct RoleTemperatureEvidence {
    SensorRole role;              // Air / Product / Cooling
    std::optional<double> celsius;
    SensorQuality quality{SensorQuality::Stale};
};

struct RecoveryTemperatureEvidence {
    std::array<RoleTemperatureEvidence, 3> beforeOutage{};  // aus letztem Checkpoint
    std::array<RoleTemperatureEvidence, 3> afterRestart{};  // aus der Recovery-Bewertung selbst
};
```

`beforeOutage` wird bei **jedem** Checkpoint-Schreibvorgang (alle
bestehenden `writeSnapshot`-Aufrufer: `persistCommand`, `persistTransition`,
`persistSensorSelection`, `checkpointPeriodic`) aus der bereits im System
fliessenden `SensorQualitySnapshot`-Instanz (#20,
`device_platform/src/sensor_quality_snapshot.hpp`) uebernommen – kein neuer
Messpfad, nur eine kompakte Projektion des bereits vorhandenen letzten
gueltigen Werts je Rolle. `afterRestart` wird **einmalig** beim
Zwei-Hop-Aufbau (Abschnitt 5.2) aus der zu diesem Zeitpunkt vorliegenden
`CrossRolePlausibilityContext`/`SensorQualitySnapshot`-Lage befuellt und
Teil derselben atomaren Recovery-Revision. Ein Sensor, der zu einem der
beiden Zeitpunkte keinen gueltigen Wert hatte, bleibt mit
`celsius = nullopt`/`quality = Stale` (kein erfundener Wert, keine
Ausnahmebehandlung). Das Feld wird bei jeder Recovery tatsaechlich befuellt
(kein dauerhaft leeres Schema-Feld).

Schema-3-Migration und Ressourcenbudget: Abschnitt 6, Abschnitt 10.

### 5.11 `persistRecoveryTimeCorrection` – episoden-/staleness-fest (Punkt 11)

**Vertrag, vollstaendig verbindlich (kein Implementierungsdetail):**

- **Episodenidentitaet:** Es wird kein neuer Zaehler eingefuehrt. Die
  Episode ist identisch mit `runRevision` der durch den Zwei-Hop-Aufbau
  (5.2) erzeugten Recovery-Ausgangsrevision. `writeSnapshot` erhoeht
  `nextHeadRevision_`/`runRevision` bei jedem Commit ohnehin
  monoton; ein neuer Boot erzeugt zwangslaeufig eine neue
  Recovery-Ausgangsrevision mit neuer `runRevision` – "ein neuer Reboot
  erzeugt eine neue Episode" folgt damit aus einem bereits bestehenden
  Mechanismus, ohne zusaetzliche Buchfuehrung.
- **Genau eine Korrektur pro Episode:** neues Kommando
  `CommandKind::ApplyRecoveryTimeCorrection` (nicht `AdjustRun` – ein
  eigener, schmaler, typisierter Fall statt Zweckentfremdung einer
  bestehenden, semantisch anderen Fachhandlung). Neue Struktur
  `RecoveryTimeCorrectionRecord { std::uint32_t appliedAtRunRevision;
  std::int32_t appliedSecondsDelta; }` (Schema 3, Teil von
  `RunPersistenceSnapshot`, optional). Ein zweiter Korrekturversuch fuer
  dieselbe `appliedAtRunRevision` mit identischem Inhalt liefert
  `CommandStatus::AlreadyProcessed` (bestehender Wert, Dedup ueber
  `persistedIds_` wie jedes andere Kommando); mit abweichendem Inhalt wird
  abgelehnt (`NotAllowedInState`).
- **Kein Einfluss auf einen inzwischen fortgeschrittenen Lauf:** Das
  Kommando traegt `CommandEnvelope.expectedRunRevision` (bestehendes Feld,
  `run_commands.hpp`). Weicht `current.runRevision` beim Anwenden vom
  erwarteten Wert ab (weil zwischenzeitlich irgendein weiterer Commit
  stattfand), liefert die bestehende `persistCommand`-Pruefung
  `CommandStatus::StaleState` – keine neue Vergleichslogik noetig.
- **Write-before-apply:** folgt exakt dem bestehenden
  `persistCommand`-Muster (Kandidat bauen, `writeSnapshot`, erst danach
  `current` uebernehmen), keine Abweichung.
- **`CommitOutcomeUnknown` fail-closed:** folgt dem bestehenden
  `PersistenceCommittedApplyFailed`-Zustand, unveraendert wiederverwendet.
- **Reboot-Idempotenz vor/nach Korrektur:** ergibt sich aus dem
  Write-before-apply-Muster ohne Sonderfallcode – vor Commit sichtbar keine
  Korrektur (erneut anwendbar), nach Commit ist die Korrektur Teil von
  `current` (erneuter Versuch -> `AlreadyProcessed`).

### 5.12 Restart-Sensorauswahl-Aktivierung (Gate A, konkretisiert im neuen Ablauf)

Innerhalb des Zwei-Hop-Aufbaus (Abschnitt 5.2), zwischen Hop 1 und Hop 2, wird
`SensorSelectionPhase::RestartRevalidationPending` real bewertet: der
`evaluatePhase`-Dispatch (`sensor_selection.cpp`) erhaelt einen echten Fall
statt `reject(InvalidContext)`; `computeRestartSensorSelection`
(aktuell Stub, `sensor_selection.cpp:890-907`, ignoriert `persisted` und
`program` via `static_cast<void>`) wird durch eine echte Bewertung ersetzt,
die den persistierten Sensorselektionszustand (`sensorSelection`-Feld,
#21) gegen die aktuelle `CrossRolePlausibilityContext` prueft, bevor Hop 2
`RecoveryResumed` liefern darf. Ein negatives Ergebnis dieser Bewertung
fliesst als zusaetzliche Bedingung in `decideRecoveryEvent` ein (kein
`RecoveryResumed` ohne bestaetigte Sensorlage) – bereits vor #18 in
`docs/RUN_PERSISTENCE.md`, Abschnitt "Uebergabe an ein spaeteres Vorhaben:
Regelsensorauswahl bei Reaktivierung", als offener Uebergabepunkt
dokumentiert; #18 schliesst ihn.

### 5.13 Komposition/DI

`RunRecoveryCoordinator` (neu, `run_recovery.hpp/.cpp`) buendelt den in 5.2
beschriebenen Ablauf als eine Klasse mit expliziten, injizierten
Abhaengigkeiten (Zeitquelle, `RunPersistenceCoordinator&`,
`CrossRolePlausibilityContext`-Provider) – keine neuen ESP-IDF-, Arduino-
oder WLAN-Abhaengigkeiten, ausschliesslich gegen bestehende
`device_platform`-Ports. Instanziierung erfolgt an der bestehenden
Boot-Kompositionsstelle (dort, wo `RunPersistenceCoordinator::loadAndInitialize()`
bereits heute aufgerufen wird), nicht als neue globale Singleton-Struktur.

### 5.14 #24-Abgrenzung

Fault-Klassen, SAFE_BOOT-Feinausbau und Fault-Reset-Ablauf bleiben #24
zugeordnet. #18 nutzt den bestehenden `Fault`-Zustand ausschliesslich als
bereits vorhandenes, unveraendertes Transitionsziel (`RecoveryRejected`);
es fuegt keine neue Fault-Unterklassifikation hinzu.

## 6. Modul- und Abhaengigkeitsgrenzen

Alle neuen/aenderten Dateien liegen in `lib/fermentation_app/src/` und
haengen ausschliesslich von bestehenden `device_platform`-Ports und
bestehenden `fermentation_app`-Modulen ab (keine neue Abhaengigkeit auf
`device_platform_esp_idf` oder `device_platform_test_support`, ADR-013
unveraendert eingehalten). `SensorRole`/`SensorQuality` werden aus
`device_platform` importiert (bereits bestehende, portable Typen, kein
neuer konkreter Adapterbezug).

## 7. Datei-/Commit-Aufschluesselung

| # | Commit | Inhalt |
|---|---|---|
| 1 | `feat(persistence): Schema 3 – RunProgressState, RecoveryTemperatureEvidence, RecoveryTimeCorrectionRecord` | Contract/Codec-Erweiterung (5.7, 5.9, 5.10, 5.11), Migrationstests 1/2/3 |
| 2 | `feat(process-state-machine): RecoveryEndedByExpiredWait-Topologie` | neuer Reason + `validControlTopology`-Zweig (5.3) |
| 3 | `feat(recovery): computeRecoveryTimeBounds` | `run_recovery_time.hpp/.cpp` (5.6) |
| 4 | `feat(sensor-selection): reale Restart-Reaktivierung` | Gate A / 5.12 |
| 5 | `feat(persistence-coordinator): Zwei-Hop-Aktivierung, commitRecoveryOutcome, activateLoadedRun` | 5.2, 5.5.1 |
| 6 | `feat(persistence-coordinator): FallbackRecovered differenziert, Slot-Override` | 5.5 |
| 7 | `feat(persistence-coordinator): resolveRecoveryOutcome, ResolveRecoveryUncertainty` | 5.4 |
| 8 | `feat(persistence-coordinator): ApplyRecoveryTimeCorrection` | 5.11 |
| 9 | `feat(recovery): RunRecoveryCoordinator, observedRunSeconds an beiden Aufrufpunkten` | 5.8, 5.13 |
| 10 | `docs: Anzeige-/Exportvertrag, Ressourcenbudget, Roadmap-Abschluss` | 5.10, Abschnitt 10 |

Jeder Commit ist einzeln kompilier- und testbar (gezielte lokale Tests des
geaenderten Bereichs je Commit, vollstaendiger Lauf erst nach Owner-Freigabe
gemaess `docs/AGENT_WORKFLOW.md`).

## 8. Testmatrix (Punkt 12)

1. Echte Wiederherstellung der alten Prozessphase -> lokale
   `RecoveryEvaluation`-Basis (Hop 1) -> gueltige `RecoveryResumed`-Transition
   (Hop 2); Kopf belegt `newTransitionSequence == oldTransitionSequence + 2`.
2. Kein Apply gegen den falschen `decision.before` (mutierter/veralteter
   Kandidat wird von `applyProcessTransition` abgelehnt).
3. `WaitingForProduct` definitiv abgelaufen -> atomarer Tombstone ->
   `state_ == ReadyEmpty`; Reboot danach laedt `NoActiveRun`.
4. `WaitingForProduct` unklar -> `Uncertain` -> Hop-1-only-Revision -> echter
   Benutzerentscheidungspfad (`ResolveRecoveryUncertainty`), inkl.
   `StaleState` bei zwischenzeitlichem Revisionswechsel und
   `AlreadyProcessed` bei Wiederholung.
5. Automatischer UTC-Aufloesungspfad fuer `Uncertain` (unabhaengig vom
   Benutzerpfad, ueber `resolveRecoveryOutcome`).
6. Gueltiger `FallbackRecovered`-Snapshot vs. echtes `NotReconstructible*`;
   Slot-Override-Test mit Store-Schnitt zwischen Slot- und Kopf-Schreiben
   (Fallback-Datensatz bleibt bis Commit erhalten).
7. Zeitfenster mit ereignisgetriebenem Save plus verzoegertem/ausgefallenem
   periodischem Checkpoint (`coveredByRegularCheckpoints == false`).
8. RAM-only monotone Buchfuehrung (`RunProgressAccountingRuntime`) ueber
   einen echten Reboot – kein boot-lokaler Wert im Wire-Format nachweisbar
   (Codec-Test: Feld existiert nicht in Schema-3-Payload).
9. Bekannte Vor-Ausfall-Fermenting-Laufzeit bleibt rekonstruierbar
   (Ableitung aus Checkpointdaten, kein aktiver Zyklus noetig).
10. Live-Fermenting-Exit-Transition faltet Laufzeit korrekt in
    `observedRunSeconds` (bestehender Aufrufpunkt, kein neuer Zyklus).
11. Letzte/erste Sensorwerte und Qualitaetszustaende ueber Schema-3-Roundtrip
    und Recovery (fehlender Sensor an einer Seite -> `nullopt`/`Stale`, kein
    erfundener Wert).
12. Fortschrittskorrektur einer alten Episode nach zwischenzeitlichem
    Revisionswechsel -> `StaleState`, kein Schreiben, kein Apply.
13. Schema-1/2/3-Current/Fallback-Matrix bleibt vollstaendig (Regression).
14. Alle bestehenden #20/#21-Sensor-/Sicherheitsregressionen bleiben gruen.
15. `WeightedProgressStatus` bleibt in Release 1 durchgaengig
    `NotCalibrated`; Codec-Roundtrip und Anzeigeableitung testen das
    explizit (kein stiller `Unavailable`-Sonderfall ohne Testfall).
16. `targetReachStartedAtMillis`/`targetReachWarningIssued` werden beim
    Zwei-Hop-Aufbau fuer Zustaende ohne laufendes Zielfenster korrekt auf
    `0U`/`false` zurueckgesetzt (5.6.2); ein roh uebernommener Altwert wird
    von `runtimeShapeIsValid` abgelehnt und dieser Fall ist als Regression
    abgedeckt.

## 9. Safety-/Security-/Recovery-/Hardwaregrenzen

Keine Aktorfreigabe vor abgeschlossenem Hop 2 (`RecoveryEvaluation` traegt
keine Aktorfreigabe, unveraendert aus dem bestehenden Vertrag). Kein
Schreiben vor vollstaendigem lokalem Kandidatenaufbau. Kein Aktorpfad direkt
aus `FallbackRecovered`. Reale Hardware-/NVS-Anbindung bleibt #29/#90
vorbehalten; diese Revision aendert nichts an der Simulations-/Test-Adapter-
Grenze (ADR-013, `device_platform_test_support` bleibt testonly).

## 10. Ressourcen-/Betriebsbudget

Schema-3-Zuwachs: `RunProgressState` (5 Byte), `RecoveryTemperatureEvidence`
(6 Rolleneintraege a ~10 Byte = ~60 Byte, optional), `RecoveryTimeCorrectionRecord`
(8 Byte, optional). Gesamtzuwachs pro aktivem Lauf-Snapshot < 100 Byte,
innerhalb des bestehenden `kMaximumCheckpointRecordBytes`-Budgets (durch
Migrationstest 8.13 mit Kapazitaetsgrenzwert abgesichert). Keine
unbeschraenkten Logs, kein Rohmesswertverlauf (5.10).

## 11. SOLID/DRY/KISS

Eine einzige Entscheidungsfunktion (`decideRecoveryEvent`) fuer Hop 2,
zwei Aufrufer (automatisch/Benutzer) – kein Parallelvertrag (Punkt 4).
Ein gemeinsamer Commit-Kern (`commitRecoveryOutcome`, 5.5.1) fuer alle vier
Aktivierungspfade. `deriveFermentingSecondsDelta` an zwei bestehenden
Aufrufpunkten statt einer unbenutzten Hilfsfunktion (Punkt 8). Die bewusst
**nicht** vereinheitlichte `Ready`/`ReadyEmpty`-Konvention zwischen dem
bestehenden Live-`ProductWaitExpired`-Pfad und dem neuen
Recovery-Tombstone-Pfad ist kein Bruch von DRY: beide behandeln denselben
Datenzustand (`clearActiveRunState`) korrekt, unterscheiden sich nur in der
Boot-Zeit-spezifischen Zustandsklassifikation, die bereits vor #18 fuer den
Ladepfad etabliert ist (`loadAndInitialize`) und hier konsistent fortgesetzt,
nicht neu erfunden wird.

## 12. Dokumentations-/Abschlussnachweis

- `docs/ROADMAP.md`: aktualisiert (Abschnitt 1 dieser Datei).
- `docs/RUN_PERSISTENCE.md`/`docs/RECOVERY_AND_INTERRUPTION.md`: werden im
  Umsetzungscommit 1/9/10 um die in 5.6-5.11 vertraglich fixierten Punkte
  ergaenzt (keine Doku-Aenderung in dieser Planrevision selbst).
- `git diff --check`: auszufuehren nach Committen dieser Datei.

## 13. Pflichtaufgabenliste (fuer die Umsetzung, nicht Teil dieser Planungssession)

1. Commit 1-10 gemaess Abschnitt 7, je mit gezielten lokalen Tests.
2. Testmatrix Abschnitt 8 vollstaendig.
3. `docs/RUN_PERSISTENCE.md`/`docs/RECOVERY_AND_INTERRUPTION.md` gemaess 5.6-5.11 nachfuehren.
4. `docs/ROADMAP.md` bei Merge/Reihenfolgeaenderung erneut pruefen.
5. SESSION HANDOVER vor Sessionende bei offenem PR.

## 14. Stop-Bedingung

Diese Revision 3 ist ein vollstaendiger, eigenstaendiger Plan. Kein
materieller Punkt bleibt als "Implementierungsdetail" offen (Punkte 2-11 des
Owner-Auftrags sind je in einem eigenen, verbindlichen Abschnitt geloest).
Nach Commit dieser Datei: **anhalten**. Keine Implementierung. Kein
`Ready for review`. Keine Remote-CI. Kein Merge. Keine Branchloeschung.
