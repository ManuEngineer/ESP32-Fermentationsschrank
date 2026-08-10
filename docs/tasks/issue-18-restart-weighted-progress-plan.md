# Plan: Issue #18 – Wiederanlauf und temperaturgewichteter Fortschritt

## 1. Status

- Revision: **10** (ersetzt Revision 9 vollstaendig; diese Datei ist die
  einzige normative Planquelle fuer die Umsetzung und setzt keine fruehere
  Planrevision voraus).
- Draft-PR: #102 (`plan/issue-18-restart-weighted-progress` -> `main`).
- Live-Issue: #18.
- Plan-Basis: `main` = `17ab3f5399a066465298ac6871b965d176a38d32`;
  Remote-HEAD vor dieser Korrektur =
  `0cad66e2cc7e59a0171487ef2ed55e6131e2f2a7`. Branch ist 0 Commits hinter
  `main`.
- Anlass: Der bestehende Vertrag fuer eine bestaetigte Restdaueranpassung
  waehrend `Fermenting` setzt den Timer bereits auf den Kommandozeitpunkt,
  faltet aber die sicher beobachtete Vor-Anpassungszeit noch nicht vollstaendig
  und neutralisiert die alten Recovery-Zeitbeitraege fuer den neuen Timer
  nicht. Revision 10 schliesst deshalb den gesamten Vertrag: die historische
  `observedRunSeconds`-Zeit wird genau einmal vor der Neusetzung gefaltet;
  `priorBootPhaseElapsed`, aktive nominale Recoverykorrektur und ein offener
  Recovery-Anker werden fuer die neue Restdauer-Baseline superseded; reine
  Zieltemperaturaenderungen behalten ihre ununterbrochene Zeitbasis; neue
  Stromausfaelle nach der Baseline beginnen wieder mit deren aktuellem
  `stateEnteredAtMillis`. Die Testmatrix und der atomare Persistenznachweis
  werden entsprechend erweitert. `docs/ROADMAP.md` wird minimal auf Revision
  10 synchronisiert.
- Nach Commit dieser Revision: `git diff --check` **ungescoped** (bare,
  ohne Pfadangabe, s. Auftrag "fuer alle geaenderten Dateien") ausfuehren,
  `git push`, danach frischer `git fetch` und Abgleich `git rev-parse
  origin/plan/issue-18-restart-weighted-progress` == lokaler `HEAD` sowie
  `gh api repos/ManuEngineer/ESP32-Fermentationsschrank/pulls/102`
  (`head.sha`) und `gh pr view 102 --json headRefOid`, bevor PR-Beschreibung/
  SESSION HANDOVER als aktuell gemeldet werden.
- Dieser Plan implementiert noch nichts.

## 2. Live-Status-Pruefung

- Live-Issue #18: weiterhin offen mit Status `PLANNED_SPEC_PENDING`; Scope,
  Akzeptanzkriterien und Quellen bleiben unveraendert.
- `gh pr view 102`: Draft, Base `main`.
- `docs/ROADMAP.md:13` wurde in dieser Session direkt gelesen und zeigt
  weiterhin `Revision 9 in Arbeit`. Diese Revision aktualisiert Zeile 13 auf
  `Revision 10` sowie Zeile 3 (`Stand:`) auf das Datum dieses Commits (5.29
  unten).
- Der aktuelle Planstand wurde vollstaendig gegen den neuen Ownerauftrag
  geprueft. Die bestehende Recovery-/Carry-Forward-Semantik bleibt erhalten;
  die neue Restdauer-Baseline wird in 5.22, 5.23, 5.12, 5.16, 5.17 und der
  Testmatrix konsistent als ein atomarer `AdjustRun`-Pfad beschrieben.
- `lib/fermentation_app/src/run_persistence_contract.hpp:40-43`
  (`RunCheckpointTime{monotonicMillis; std::optional<std::int64_t>
  utcUnixSeconds}`) direkt erneut gelesen: die UTC ist bereits im
  bestehenden Vertrag `std::optional`, "noch keine NTP-Synchronisation"
  wird also durch `nullopt` ausgedrueckt, nicht durch einen Sentinelwert –
  `deriveUtcAtRecoveryBootAnchor` (5.12) wird in dieser Revision auf genau
  diese bereits vorhandene Optionalitaet umgestellt, statt sie stillschweigend
  vorauszusetzen.
- Alle Codezeilenangaben in dieser Revision wurden in dieser Session erneut
  direkt am Dateiinhalt verifiziert, u. a.:
  - `grep -rn "checkpointPeriodic"` ueber das gesamte Repository: ausserhalb
    von `run_persistence_coordinator.hpp/.cpp` selbst und dessen eigenem
    Testverzeichnis existiert **kein** Aufrufer – es gibt keine produktive
    periodische Aufrufcadence, aus der eine Kontrollpunktabstands-Garantie
    abgeleitet werden koennte.
  - `run_persistence_coordinator.cpp` vollstaendig, `process_state_machine.cpp`
    an allen zitierten Stellen, `run_persistence_contract.hpp/.cpp`,
    `run_checkpoint_schedule.hpp/.cpp`, `sensor_selection.hpp`,
    `sensor_quality_snapshot.hpp` (`SensorQuality{Valid,Stale,Failed}`),
    `run_commands.cpp:995-1029`.
  - `run_persistence_codec.cpp:901-929` (`writeMutationKind`/
    `readMutationKind`, aktuell exakt drei Werte 1/2/3) und
    `run_persistence_codec.cpp:958-973` (`validCommittedHead`, erzwingt dort
    bereits strukturell `head.fallback->slot != head.current.slot`).
  - `run_commands.hpp`/`run_persistence_coordinator.hpp`/
    `process_state_machine.hpp/.cpp`: `ResolveRecoveryUncertainty`/
    `ResolveRecoveryUncertaintyRequest`/`resolveRecoveryOutcome`/
    `ApplyRecoveryTimeCorrection`/`AssumeStillValid`/`AssumeThresholdCrossed`/
    `completeHoldDuration`/`reevaluateRecoveryTime` existieren dort nirgends
    – reine Planungsbegriffe dieser Datei, keine bestehenden Bezeichner, die
    kollidieren koennten.

## 3. Owner-Entscheidungen (Gates)

- **Gate A:** Restart-Sensorauswahl wird real angewandt (5.26).
- **Gate B:** kein neuer allgemeiner Prozesszyklus; kein produktiver
  Aufrufer wird behauptet, der nicht existiert (5.27). Gilt explizit auch
  fuer `applyLiveRecoveryEvidence` (5.20) und `reevaluateRecoveryTime`
  (5.17, als `reevaluateRecoveryTime` bewusst schmal benannt, da
  die Funktion ab dieser Revision auch bereits resumte
  `Fermenting`/`CoolHolding`/`WaitingForProduct`-Laeufe auswertet, also
  nicht mehr nur "pending" im Sinn von "noch in `RecoveryEvaluation`"
  ist): beide sind native, vollstaendig getestete APIs ohne produktive
  Verdrahtung innerhalb von #18 – insbesondere wird kein produktiver
  NTP-Verfuegbarkeits-Aufrufer komponiert, der `reevaluateRecoveryTime`
  automatisch ausloest.
- **Gate C:** keine unkalibrierte biologische Aktivitaetskurve; unsichere
  Ausfallzeit wird ohne freigegebenes Modell nicht automatisch als
  Fortschritt gutgeschrieben (5.22); nicht beobachtete Ausfallzeit wird
  niemals als `observedRunSeconds` gebucht (5.22). Eine automatische
  Prozessentscheidung wird ausschliesslich aus bewiesenen Fakten (Alt-Boot-
  lokal bekannte Sekunden, bestaetigte Benutzerkorrektur) getroffen, niemals
  aus einer unbewiesenen Ausfall-Untergrenzen-Annahme (5.13).

## 4. Ziel und Nicht-Ziele

**Ziel:** Nach einem Neustart wird ein geladener aktiver Lauf sicher
bewertet, korrekt fortgesetzt oder korrekt beendet, mit nachvollziehbarem,
ehrlich gekennzeichnetem Fortschritt und ohne jede Aktorfreigabe vor
abgeschlossener Bewertung.

**Nicht-Ziele (Release 1):** reale Hardware-/NVS-Anbindung (#29/#90);
Fault-Klassen/SAFE_BOOT-Feinausbau (#24); Web-/Anzeige-UI-Implementierung;
kalibrierte biologische Temperatur-Aktivitaets-Gewichtung (Gate C); ein
neuer periodischer Anwendungszyklus (Gate B); produktive Verdrahtung eines
automatischen UTC-Reevaluationsaufrufers oder einer produktiven
Live-Sensor-Update-Schleife (Gate B).

## 5. Bindende Fachvertraege

### 5.1 Modul- und Dateizuordnung

| Bereich | Datei(en) | Aenderungsart |
|---|---|---|
| Ausfallintervall, Boot-Anker-Ableitung, Konfidenz (rein) | `lib/fermentation_app/src/run_recovery_time.hpp/.cpp` | neu |
| Recovery-Orchestrierung, `applyLiveRecoveryEvidence`, `RunRecoveryCoordinator::reevaluateRecoveryTime` (Verdicts, Zeit-Nachtragskorrektur fuer bereits resumte Laeufe) | `lib/fermentation_app/src/run_recovery.hpp/.cpp` | neu |
| Zustandsautomat: neue Reasons, Topologie, `PriorBootPhaseElapsed`-Parameter | `lib/fermentation_app/src/process_state_machine.hpp/.cpp` | erweitert |
| Persistenzvertrag: Schema 3 (`PendingRecoveryAnchor`, `RunProgressState`, `RecoveryEpisodeEvidence`, `TaggedPriorBootPhaseElapsed`, `NominalRecoveryAdjustmentState`) | `lib/fermentation_app/src/run_persistence_contract.hpp/.cpp` | erweitert |
| Codec: Schema-3-Gate, Legacy-Migration, `RunPersistenceMutationKind::Recovery` | `lib/fermentation_app/src/run_persistence_codec.cpp` | erweitert |
| Coordinator: Low-Level-Schreibkern mit Rollbackzustand, `FallbackRecoveryPending`, `activateLoadedRun`, `activateFallbackRecoveredRun`, `resolveRecoveryOutcome` (Gate-A-gekoppelter Resume fuer `AssumeStillValid`, Bounds-Gate fuer `AssumeThresholdCrossed`), Episode-Refresh | `lib/fermentation_app/src/run_persistence_coordinator.hpp/.cpp` | erweitert |
| Kommandos: `ApplyRecoveryTimeCorrection` (echtes `persistCommand`, mutiert nur Daten); `AdjustRun`-Zeitfaltung und manuelle Restdauer-Baseline; `completeTimedRun`-Wiederverwendung fuer `resolveRecoveryOutcome` | `lib/fermentation_app/src/run_commands.hpp/.cpp` | erweitert |
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

"Aktuelle UTC" ist niemals eine roh zum Abfragezeitpunkt gelesene `utcNow`,
wenn seit dem Neustart bereits Zeit vergangen ist – sonst wird die seit dem
Neustart verstrichene *Laufzeit* faelschlich als Teil der *Ausfallzeit*
gezaehlt (Ableitung: 5.12). `computeRecoveryOutageBounds` selbst ist eine
reine, boot-unabhaengige Vergleichsfunktion auf bereits fertig abgeleiteten
UTC-Werten:

```cpp
// run_recovery_time.hpp
struct RecoveryOutageBoundsInput {
    std::optional<std::int64_t> utcAtLastCheckpoint;   // PendingRecoveryAnchor.originalCheckpointUtc (5.12) – ueber die gesamte offene Episode unveraendert
    std::optional<std::int64_t> utcAtRestartBoundary;  // deriveUtcAtRecoveryBootAnchor() (5.12) – UTC-Aequivalent des Boot-Zeitpunkts dieser Recovery-Bewertung, NICHT die aktuelle Abfrage-UTC
    std::optional<std::uint32_t> maxCheckpointGapSeconds; // 5.13 – NUR gesetzt, wenn eine tatsaechlich beweisbare Grenze existiert; `nullopt`, solange keine solche Garantie besteht (heute IMMER `nullopt`, s. 5.13)
    RunCheckpointTrigger lastCheckpointTrigger;         // reine Anzeigeinformation (5.5), keine Grenzenrolle
};

struct RecoveryOutageBounds {
    std::uint64_t outageSecondsUpperBound;  // = utcAtRestartBoundary - utcAtLastCheckpoint (checked, s.u.)
    std::uint64_t outageSecondsLowerBound;  // = maxCheckpointGapSeconds.has_value() ? saturating_sub(upperBound, *maxCheckpointGapSeconds, 0) : 0
};

// nullopt genau dann, wenn ein UTC-Anker fehlt oder utcAtRestartBoundary <
// utcAtLastCheckpoint (Uhr ging zurueck – als unbekannt behandelt, nicht als
// negative Dauer). Keine erfundene Ausfalldauer ohne beide Anker.
[[nodiscard]] std::optional<RecoveryOutageBounds> computeRecoveryOutageBounds(
    const RecoveryOutageBoundsInput& input);
```

**Ohne beweisbaren `maxCheckpointGapSeconds` (der Regelfall, 5.13) ist
`outageSecondsLowerBound` immer `0`** – die Ausfallzeit traegt dann nichts
zur unteren Grenze bei; nur der sicher bekannte Alt-Boot-Anteil
(`knownSecondsBeforeCheckpoint`, 5.3) bleibt untergrenzenwirksam. Das ist
beabsichtigt (5.4, 5.13) und kein Regressionsrisiko: eine kleinere
Untergrenze kann nur dazu fuehren, dass ein `Uncertain`-Verdikt laenger
bestehen bleibt, niemals dazu, dass eine tatsaechlich noch laufende Phase
faelschlich als abgelaufen erkannt wird.

**Checked Arithmetic:** Die Subtraktion `utcAtRestartBoundary -
utcAtLastCheckpoint` erfolgt in `std::int64_t` (beide Operanden sind bereits
vorzeichenbehaftete UTC-Sekunden) und wird vor der Umwandlung nach
`std::uint64_t` explizit auf `< 0` geprueft (-> `nullopt`, keine impliziten
Wrap-Around-Werte). `saturating_sub(upperBound, maxCheckpointGapSeconds, 0)`
ist eine eigene kleine Hilfsfunktion (`a >= b ? a - b : 0`), keine rohe
`unsigned`-Subtraktion. Testfaelle: `maxCheckpointGapSeconds == nullopt` ->
`outageSecondsLowerBound == 0` unabhaengig von `upperBound`; exakt gleiche
Werte bei gesetztem Gap (Ergebnis 0); `maxCheckpointGapSeconds > upperBound`;
`std::uint64_t`-nahe Grenzwerte fuer `upperBound`.

Fehlt ein UTC-Anker: `computeRecoveryOutageBounds` liefert `nullopt`.

### 5.3 `RecoveredPhaseElapsed` – Phasenlaufzeit, strukturell getrennt (Vertrag B)

```cpp
struct RecoveredPhaseElapsedInput {
    std::uint64_t knownSecondsBeforeCheckpoint;  // bereits fertig berechnet uebergeben (5.12) – NIEMALS aus zwei rohen Boot-Millis-Werten an dieser Stelle neu subtrahiert
    std::optional<RecoveryOutageBounds> outage;  // aus 5.2, kann nullopt sein
};

struct RecoveredPhaseElapsed {
    std::uint64_t knownSecondsBeforeCheckpoint;          // Durchreichung des Inputs
    std::uint64_t totalSecondsLowerBound;                // = checked_add(knownSecondsBeforeCheckpoint, outage ? outage->outageSecondsLowerBound : 0)
    std::optional<std::uint64_t> totalSecondsUpperBound; // = checked_add(knownSecondsBeforeCheckpoint, outage->outageSecondsUpperBound), nur wenn outage vorhanden
};

// nullopt bedeutet ausschliesslich einen arithmetischen Fehler
// (Additionsueberlauf, praktisch unerreichbar). Das ist etwas GRUNDSAETZLICH
// ANDERES als `totalSecondsUpperBound == nullopt` INNERHALB des
// zurueckgegebenen Werts, das eine fachlich unbekannte Obergrenze bedeutet
// (z. B. weil kein UTC-Anker vorliegt). Ein Ergebnis kann also entweder
// - gar nicht existieren (nullopt auf Funktionsebene: Rechenfehler), oder
// - existieren, aber mit `totalSecondsUpperBound == nullopt` (fachlich
//   unbekannt, kein Fehler)
// sein – niemals wird ein Rechenfehler still als "Obergrenze unbekannt"
// verkleidet oder umgekehrt.
[[nodiscard]] std::optional<RecoveredPhaseElapsed> computeRecoveredPhaseElapsed(
    const RecoveredPhaseElapsedInput& input);
```

Die Funktion nimmt keine zwei Boot-Millis-Werte entgegen, aus denen sie
selbst eine Differenz bilden koennte. Damit ist es strukturell unmoeglich,
dass diese Funktion Werte aus zwei verschiedenen Boots voneinander
subtrahiert. Die einzige Stelle, an der
`(oldCheckpointMonotonicMillis - oldStateEnteredAtMillis) / 1000` tatsaechlich
berechnet wird, ist die einmalige Konstruktion von
`PendingRecoveryAnchor.knownPhaseSecondsAtOriginalCheckpoint` bei Hop 1
(5.12) – dort sind beide Werte garantiert aus demselben (alten) Boot, weil
sie direkt aus dem soeben geladenen Datensatz stammen, bevor irgendein neuer
Boot-Wert einfliesst.

**Checked Arithmetic:** `checked_add` prueft vor der Addition
`std::numeric_limits<std::uint64_t>::max() - a < b`; im (praktisch
unerreichbaren) Ueberlauffall liefert `computeRecoveredPhaseElapsed`
insgesamt `std::nullopt` (Funktionsebene) statt eines Ergebnisses mit
irgendeinem der beiden Grenzwerte auf einen Ersatzwert gesetzt. Jeder
Aufrufer behandelt dieses `nullopt` wie jeden anderen technischen
Rechenfehler (analog `InvalidProjection`), niemals wie ein fachliches
`Uncertain`. Test: beide Operanden nahe `UINT64_MAX` -> Funktionsergebnis
`nullopt`, nicht ein Wert mit geklemmter Obergrenze.

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
steuert Hop-2-Ausgang), `Fermenting` (`fermentationDurationMinutes*60`),
`CoolHolding` mit `CompletionMode::CoolAndHoldForDuration`
(`holdDurationMinutes*60`); alle anderen recovery-faehigen Phasen ohne
snapshot-getragene Grenze.

**`Uncertain` ist der zu erwartende Regelfall, kein Sonderfall:** Da
`maxCheckpointGapSeconds` heute grundsaetzlich nicht beweisbar ist (5.13),
ist `outageSecondsLowerBound` praktisch immer `0`, sodass
`totalSecondsLowerBound` nur den Alt-Boot-lokalen Anteil enthaelt. Damit
liefert `DefinitelyExpired` ausschliesslich dann, wenn bereits dieser
Alt-Boot-lokale Anteil allein die Grenze erreicht – eine tatsaechlich
bewiesene, boot-lokale Tatsache, unabhaengig von jeder Ausfallzeitschaetzung.
In allen anderen Faellen bleibt das Verdikt `Uncertain`, bis entweder eine
schaerfere UTC-Grenze vorliegt (`DefinitelyStillValid`/`DefinitelyExpired`
ueber `totalSecondsUpperBound`) oder der Benutzer entscheidet (5.17). Genau
deshalb braucht #18 einen echten Benutzerpfad fuer alle drei
grenztragenden Phasen (5.17), einen unabhaengig von Kontrollpunkt-Zeitpunkten
funktionierenden Sensor-Latch (5.20) und eine ehrliche, sichtbare
Konfidenzanzeige (5.5) statt einer stillschweigend angenommenen, nicht
belegbaren Praezision.

### 5.5 `RecoveryConfidence` – abgeleiteter, nicht persistierter Konfidenzvertrag

Issue #18 und `docs/RECOVERY_AND_INTERRUPTION.md` verlangen sichtbare
Zeitqualitaet/Konfidenz. Ein einzelnes, nur beilaeufig erwaehntes Feld
(`lastCheckpointTrigger`, reine Anzeigeinformation, 5.2) definiert keine
kanonische Konfidenzsemantik.

```cpp
// run_recovery_time.hpp – rein, nicht persistiert
enum class RecoveryConfidence : std::uint8_t {
    Unknown,  // Verdict Uncertain UND kein UTC-Anker (outage == nullopt): keine belastbare numerische Aussage moeglich
    Bounded,  // Verdict Uncertain, aber UTC-Anker vorhanden: eine numerische Ober-/Untergrenze ist anzeigbar
    Strong,   // Verdict DefinitelyStillValid ODER DefinitelyExpired: die Entscheidung ist bewiesen, unabhaengig davon, ob dafuer ein UTC-Anker noetig war
};

[[nodiscard]] RecoveryConfidence deriveRecoveryConfidence(
    RecoveryTimeVerdict verdict, bool outageBoundsKnown) {
    if (verdict != RecoveryTimeVerdict::Uncertain) {
        return RecoveryConfidence::Strong;
    }
    return outageBoundsKnown ? RecoveryConfidence::Bounded
                             : RecoveryConfidence::Unknown;
}
```

**Alle drei Werte sind unter dem in 5.13 festgelegten, heute grundsaetzlich
unbeweisbaren `maxCheckpointGapSeconds` tatsaechlich erreichbar** (anders als
ein hypothetischer, an die Gap-Beweisbarkeit gekoppelter vierter/dritter
Pegel es waere, der niemals erreicht wuerde, s. 5.25):

- `Strong`: entweder der Alt-Boot-lokale Anteil allein beweist bereits das
  Ergebnis (kein UTC-Anker noetig), oder ein vorhandener UTC-Anker liefert
  eine `totalSecondsUpperBound`, die eindeutig unter der Grenze liegt.
- `Bounded`: ein UTC-Anker existiert (`outage.has_value()`), das Intervall
  ueberschneidet die Grenze aber weiterhin – anzeigbar als "zwischen X und Y
  Sekunden", ohne definitive Aussage.
- `Unknown`: kein UTC-Anker vorhanden und der Alt-Boot-lokale Anteil allein
  reicht nicht zur Entscheidung.

**Bewusst nicht in `RecoveryConfidence` eingemischt:** `RunProgressBasis`
(5.21, `KnownTotal`/`PartialUnknownHistory`) ist ein orthogonales, bereits
eigenstaendig definiertes Signal ueber die **gesamte** historische
Fermentationszeit dieses Laufs, nicht ueber die aktuelle
Recovery-Episode. Anzeige/Export zeigt beide Werte nebeneinander, nicht
zusammengefuehrt – konsistent mit der durchgaengigen Trennung fachlich
verschiedener Groessen in diesem Plan (5.2/5.3, 5.22).

**Nicht persistiert:** `RecoveryConfidence` wird bei jeder Anzeige/jedem
Export aus dem jeweils aktuellen `RecoveryTimeVerdict` und der Praesenz von
`RecoveryOutageBounds` frisch abgeleitet – dieselbe Ableitungsfunktion fuer
alle Aufrufer, kein zweiter, potenziell abweichender gespeicherter Wert.

**Tests:** `Strong` ueber Alt-Boot-lokalen Beweis ohne UTC-Anker; `Strong`
ueber UTC-Obergrenze; `Bounded` bei ueberschneidendem UTC-Intervall;
`Unknown` ohne jeden UTC-Anker; alle drei Faelle nachweisbar erreichbar
(kein toter Enumwert, Regressionstest gegen den in `WeightedProgressStatus`,
5.25, bereits vermiedenen Fehler).

### 5.6 Boot-unabhaengiger Phasenfortschritt – kein Rebasing-Unterlauf

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
    // Hop-1-Konstruktion garantiert (5.9) - keine Unterlaufgefahr, da hier
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
`RunCommandState`/`RunPersistenceSnapshot` (5.23, vollstaendiger
Persistenzvertrag). Der Aufrufer (Recovery-Orchestrierung bzw. der
automatische Live-Dispatch) liest ihn nur, wenn `current.processState.state`
mit dem Tag uebereinstimmt – sonst wird `{}` (kein Vor-Boot-Anteil)
verwendet. Fuer `Fermenting` wird der tatsaechlich verwendete Wert zusaetzlich
um eine bestaetigte nominale Korrektur ergaenzt (5.22) – Herkunft und
Kombination dort verbindlich festgelegt. Keine Boot-Epochen-Fiktion; keine
Subtraktion von `now`.

**Manuelle Restdauer-Baseline:** Bei einem bestaetigten `AdjustRun` mit
tatsaechlich geaendertem `remainingDurationMinutes` waehrend `Fermenting` gilt
der Kommandozeitpunkt als neuer Timer-Baselinepunkt. Der anschliessende normale
`elapsedWithPrior`-Aufruf verwendet fuer diese neue Restdauer weder den alten
`priorBootPhaseElapsed`-Beitrag noch eine alte nominale Korrektur; beide werden
im selben Kandidaten auf den in 5.22/5.23 beschriebenen neuen Nullstand
gebracht. `observedRunSeconds` ist eine historische Metrik und wird nicht als
Timer-Vorlauf verwendet. Eine reine Zieltemperaturaenderung durchlaeuft diese
Baseline-Regel nicht. Eine kombinierte Ziel- und Restdaueranpassung folgt fuer
die Zeitbasis der Restdauerregel.

### 5.7 Vollstaendige phasenbezogene Timertabelle fuer Hop 2

| Phase | `stateEnteredAtMillis` | `targetReachStartedAtMillis` | `targetReachWarningIssued` | `qualificationValidSinceMillis` | boot-unabhaengiger Anteil |
|---|---|---|---|---|---|
| `Preheating` | `now` | `0` (kein Timer) | `false` | `nullopt` (bestehende Logik, `process_state_machine.cpp:771-772`) | keiner |
| `WaitingForProduct` | `now` | `0` | `false` | n/a | `priorElapsed` (Ober- oder Untergrenze, siehe 5.10) |
| `ReachingTarget` | `now` | `now` (Timer bewusst neu gestartet – konservativ, verzoegert hoechstens eine Warnung) | `false` | n/a | keiner |
| `QualifyingTarget` | wird von `decideRecoveryEvent` selbst zu `ReachingTarget` umgeleitet, `stateEnteredAtMillis = now` (bestehende Logik, `process_state_machine.cpp:773-777`) | `now` | `false` | `nullopt` | keiner – beginnt vollstaendig neu |
| `Fermenting` | `now` | `0` | `false` | n/a | `priorElapsed` (Untergrenze, ggf. + bestaetigte nominale Korrektur, 5.22) + separat `observedRunSeconds` (5.22, Geschaeftsmetrik, getrennt von der reinen Grenzbewertung); nach manueller Restdauer-Baseline sind beide aktiven Vorlaufbeitraege fuer den neuen Timer `0` |
| `Cooling` | `now` | `0` | `false` | n/a | keiner (kein Dauer-Timer, signalbasiert) |
| `CoolHolding` | `now` | `0` | `false` | n/a | `priorElapsed` (Untergrenze) |
| `ManualHolding` | `now` | `0` | `false` | n/a | keiner (kein automatisches Limit) |

Kein Feld wird roh aus dem alten Boot uebernommen; jedes Feld ist entweder
explizit auf `now`/`0`/`nullopt` gesetzt oder ueber `priorElapsed` separat
gefuehrt. Der `now`-Wert bei einer manuellen Restdauer-Baseline ist dabei der
Kommandozeitpunkt, nicht ein Recovery-Bootzeitpunkt; die alte Recovery-Zeitbasis
wird nicht zusaetzlich in die neue Dauerentscheidung eingerechnet.

### 5.8 Architekturgrenze: `RunPersistenceCoordinator` vs. `RunRecoveryCoordinator`

Der geladene Vor-Ausfall-Datensatz (`RunPersistenceRawRecord`: `bytes`,
`snapshot`, `checkpointRevision`, `utcUnixSeconds`) ist **nicht** Teil der
oeffentlichen `loadAndInitialize()`-Schnittstelle:

```cpp
struct RunPersistenceLoadResult {
    RunPersistenceLoadStatus status;
    std::optional<RunPersistenceSnapshot> snapshot;  // KEIN RawRecord
};
```

Der RawRecord liegt ausschliesslich intern in `RunPersistenceCoordinator::slots_`
(`run_persistence_coordinator.hpp:188`, `std::optional<RunPersistenceRawRecord>
slots_[2]`) – befuellt bei `loadAndInitialize()` sowohl fuer den
Current-Erfolgsfall (`slots_[currentHead_->current.slot]`,
`run_persistence_coordinator.cpp:193`) als auch fuer den
Fallback-Erfolgsfall (`slots_[currentHead_->fallback->slot]`,
`run_persistence_coordinator.cpp:270`) und bis zum naechsten Schreibvorgang
unveraendert gueltig.

**Vertrag – kein Leaken des technischen RawRecord:**

- `activateLoadedRun(...)` und `activateFallbackRecoveredRun(...)` sind
  Methoden von `RunPersistenceCoordinator` selbst (bereits so in 5.1
  zugeordnet), nicht von `RunRecoveryCoordinator`. Sie konstruieren
  `PendingRecoveryAnchor` (5.12) direkt aus dem fuer den jeweiligen Fall
  autoritativen internen Datensatz: `activateLoadedRun` aus
  `slots_[currentHead_->current.slot]`, `activateFallbackRecoveredRun` aus
  `slots_[currentHead_->fallback->slot]`. Kein zweiter Transportweg, keine
  Kopie ueber die oeffentliche API. **Beide** wenden dabei identisch die in
  5.12 festgelegte Fallunterscheidung (frisch/Carry-Forward, Auftragspunkt
  1) auf ihren jeweiligen Datensatz an – `activateFallbackRecoveredRun`
  traegt dieselbe Carry-Forward-Pflicht wie `activateLoadedRun`, da auch
  ein aus dem Fallback-Slot geladener Datensatz einen noch offenen
  `pendingRecoveryAnchor` tragen kann (derselbe Schema-3-Snapshot, nur aus
  dem anderen Slot gelesen).
- Beide Methoden fuehren atomar Ankerkonstruktion, Gate-A-Auswertung (Punkt
  weiter unten), Hop-1-Aufbau und – falls erfolgreich – den Hop-2-Versuch
  sowie den Commit ueber `writeSnapshotCore` (5.16) in einem einzigen
  Methodenaufruf durch und liefern ein schmales Ergebnis:

  ```cpp
  struct RecoveryActivationOutcome {
      RunPersistenceResult persistenceResult;
      RunCommandState resultingState;  // vollstaendig aktualisierter RAM-Zustand
  };
  [[nodiscard]] RecoveryActivationOutcome
  RunPersistenceCoordinator::activateLoadedRun(
      const RunCommandState& current, const RunCheckpointTime& time,
      const CrossRolePlausibilityContext& liveSensorEvidence);
  [[nodiscard]] RecoveryActivationOutcome
  RunPersistenceCoordinator::activateFallbackRecoveredRun(
      const RunCommandState& current, const RunCheckpointTime& time,
      const CrossRolePlausibilityContext& liveSensorEvidence);
  ```

  `liveSensorEvidence` wird von diesen Methoden selbst sowohl fuer Gate A
  (5.26) als auch fuer die Sensorevidenz-Erstbefuellung (5.20) verwendet –
  ein einziger Parameter, eine einzige Verwendung, keine zweite
  Sensorpipeline.
- `RunRecoveryCoordinator::activate(RunPersistenceCoordinator& persistence,
  RunCommandState& current, const RunCheckpointTime& time, const
  CrossRolePlausibilityContext& liveSensorEvidence)` ist die duenne
  Orchestrierungsschicht: sie liest `persistence.state()` (oeffentlich,
  bestehend), waehlt anhand dessen zwischen `activateLoadedRun`,
  `activateFallbackRecoveredRun`, dem Episode-Refresh-Pfad (5.15) und dem
  `Completed`-Sonderpfad (5.24), ruft die gewaehlte Methode auf und
  uebernimmt `resultingState` in `current`. Sie erhaelt zu keinem Zeitpunkt
  Zugriff auf `slots_`, `currentHead_` oder einen `RunPersistenceRawRecord`.
- `reevaluateRecoveryTime` (5.12) benoetigt keinen RawRecord: sie liest
  ausschliesslich `current.pendingRecoveryAnchor` und
  `current.recoveryBootAnchorMonotonicMillis` (beide bereits im RAM-Zustand
  vorhanden, 5.12).

Current- und Fallback-Fall werden getrennt getestet: `activateLoadedRun`
konstruiert den Anker korrekt aus `slots_[currentHead_->current.slot]`;
`activateFallbackRecoveredRun` konstruiert ihn korrekt aus
`slots_[currentHead_->fallback->slot]`; `RunRecoveryCoordinator` selbst hat
in beiden Fallgruppen keinen Zugriff auf den jeweiligen RawRecord
(Compile-/API-Test: die Typsignatur von `RunRecoveryCoordinator::activate`
enthaelt keinen `RunPersistenceRawRecord`-Parameter).

### 5.9 Hop 1 – Recovery-Eintrittsvertrag

`TransitionReason::RecoveryReentryRequired`, `validControlTopology`-Zweig
`stateUsesRunSnapshot(from) && to == RecoveryEvaluation`
(`stateUsesRunSnapshot`, `process_state_machine.cpp:76-97`: `Preheating`,
`WaitingForProduct`, `ReachingTarget`, `QualifyingTarget`, `Fermenting`,
`Cooling`, `CoolHolding`, `ManualHolding` – **nicht** `Completed`, dazu
5.24). Ablauf, ausgefuehrt innerhalb von
`RunPersistenceCoordinator::activateLoadedRun` (5.8): `candidate =
restoredState`; `originalRestoredProcessState` als unveraenderte Kopie fuer
den Recovery-Kontext (5.12); `hop1 = propose(candidate.processState,
RecoveryEvaluation, RecoveryReentryRequired, monotonicMillis)`;
`applyProcessTransition(candidate.processState, hop1,
&*candidate.processRunSnapshot)`. `ProcessSignals::criticalFault = false`.
Bei Fehlschlag: kein Schreiben, Coordinator bleibt im Ausgangszustand,
`InvalidDecision`.

**Zusaetzlich, vor dem Commit dieses Hop 1 (5.12):** aus dem intern
autoritativen Vor-Ausfall-Datensatz (5.8) wird `PendingRecoveryAnchor`
konstruiert und `candidate.pendingRecoveryAnchor` gesetzt – **frisch**
(einmalig) oder **carry-forward** (fortgesetzt), je nachdem, ob der
geladene Datensatz selbst bereits einen noch offenen
`pendingRecoveryAnchor` traegt (5.12 legt die genaue Fallunterscheidung
und Feldkonstruktion fest, Auftragspunkt 1);
`candidate.recoveryBootAnchorMonotonicMillis = monotonicMillis` wird in
beiden Faellen gesetzt (5.12). Die geordnete Reihenfolge fuer Sensorevidenz
(`beforeOutage` einfrieren, `lastKnown` aktualisieren, `firstAfterRestart`
latchen, erst danach ggf. den Anker loeschen) ist in 5.20 verbindlich
festgelegt und gilt fuer Hop 1 unveraendert.

### 5.10 Hop 2 – Wiederverwendung der bestehenden `decideRecoveryEvent`-API

`request.recoveredState` wird aus `pendingRecoveryAnchor.originalProcessState`
gemaess 5.7 aufgebaut (alle Felder explizit gesetzt, kein roher Altwert).
Aufruf, weiterhin innerhalb derselben `activateLoadedRun`/
`activateFallbackRecoveredRun`-Methode (5.8):

```cpp
TransitionRequest request;
request.event = ProcessEvent::RecoveryResume;  // oder RecoveryReject, siehe 5.26
request.recoveredState = rebasedRecoveredState;
const auto hop2 = decideProcessTransition(
    candidate.processState /* == RecoveryEvaluation */,
    &*candidate.processRunSnapshot, ProcessSignals{/* criticalFault=false */},
    request, monotonicMillis, priorElapsedForOldPhase /* 5.6 */);
```

Rebasing-Richtung fuer `priorElapsedForOldPhase`, das an `decideRecoveryEvent`s
eingebauten `WaitingForProduct`-Check (`elapsedWithPrior` statt
`elapsedOptional`, 5.6) sowie an die Snapshot-Struktur selbst
weitergereicht wird:

- **Alle Phasen ausser `WaitingForProduct`:** `RecoveredPhaseElapsed.totalSecondsLowerBound`
  – nie mehr Ausfallzeit kreditiert, als Alt-Boot-lokal belegt ist.
- **`WaitingForProduct` mit `DefinitelyStillValid` (automatischer Pfad):**
  `totalSecondsUpperBound` (in diesem Fall garantiert vorhanden) – die fuer
  diesen **automatischen, unbeaufsichtigten** Pfad konservative Richtung:
  ohne menschliche Pruefung soll selbst die pessimistischste (laengste)
  moegliche Wartezeit noch sicher unter der Grenze liegen, sonst wird kein
  Resume erzwungen. Unter-Kreditieren waere hier die falsche Richtung, weil
  es einen in Wirklichkeit bereits abgelaufenen Wartezustand automatisch
  als "noch gueltig" fortsetzen wuerde.
- **`WaitingForProduct` mit `Uncertain` und bestaetigendem Benutzerentscheid
  (`resolveRecoveryOutcome` + `AssumeStillValid`, 5.17, neu in dieser
  Revision):** `totalSecondsLowerBound` – **bewusst die entgegengesetzte
  Richtung zum automatischen Pfad**, kein Widerspruch: die manuelle
  Attestierung des Benutzers ersetzt hier bereits den Sicherheitsnachweis
  ("selbst im schlechtesten Fall noch gueltig"), den der automatische Pfad
  erst ueber `totalSecondsUpperBound` erbringen muss. Ausserdem ist
  `totalSecondsUpperBound` unter `Uncertain` entweder `nullopt` oder
  `>= limit` (5.4) – als Kreditwert hier also entweder undefiniert oder
  selbstwidersprechend (er wuerde die soeben bestaetigte Guelitgkeit
  sofort wieder infrage stellen). `totalSecondsLowerBound` ist unter jeder
  `Uncertain`-Konstellation wohldefiniert und erfindet keine
  Restwartezeit, die nicht Alt-Boot-lokal bewiesen ist (Auftragspunkt 2:
  "keine still erfundene Restwartezeit").
- **`WaitingForProduct` mit `DefinitelyExpired`, oder `Uncertain` ohne
  bestaetigenden Benutzerentscheid:** kein Resume-Versuch (5.11/5.17).

Liefert `hop2.status != Proposed` (z. B. weil das eingebaute
`WaitingForProduct`-Sicherheitsnetz trotz eigener Vorpruefung ablehnt), wird
kein Resume erzwungen; Rueckfall auf Hop-1-only (5.17).

**Gate-A-Kopplung:** Innerhalb derselben Methode wird zwischen Hop 1 und
Hop 2 die reale Restart-Sensorauswahl (5.26) anhand des uebergebenen
`liveSensorEvidence` (5.8) ausgewertet; negatives Ergebnis ->
`request.event = ProcessEvent::RecoveryReject` (bestehende, bereits
implementierte `RecoveryEvaluation -> Fault`-Logik).

### 5.11 WaitingForProduct: definitiv abgelaufene Wartezeit – Tombstone

`TransitionReason::RecoveryEndedByExpiredWait`, `validControlTopology`-Zweig
`from == RecoveryEvaluation && to == Standby`, direkt ueber `propose()`
konstruiert (keine wiederverwendbare bestehende Funktion dafuer vorhanden),
`clearActiveRunState(candidate)` vor der Persistierung (dieser Helfer setzt
`pendingRecoveryAnchor`, `recoveryBootAnchorMonotonicMillis`,
`lastRecoveryEpisodeEvidence` (5.20), `priorBootPhaseElapsed` (5.23) und
`nominalRecoveryAdjustment` (5.22) auf `nullopt`), Coordinator erreicht
`ReadyEmpty`.

**Automatische Erreichbarkeit (verbindlich, 5.4):** Der automatische
Tombstone-Pfad (ueber `evaluateRecoveryTimeVerdict ==
DefinitelyExpired`) ist ohne beweisbaren `maxCheckpointGapSeconds` (5.13)
nur erreichbar, wenn bereits `knownSecondsBeforeCheckpoint` allein (die
Alt-Boot-lokale, sicher bekannte Wartezeit vor dem Ausfall) die
`maximumProductWaitMinutes*60`-Grenze erreicht – eine bewiesene, rein
boot-lokale Tatsache. Der ueberwiegende Teil der Faelle mit tatsaechlich
abgelaufener, aber nicht allein Alt-Boot-lokal bewiesener Wartezeit bleibt
`Uncertain` und erfordert den Benutzerpfad (5.17) – **keine geratene
automatische Tombstone-Entscheidung aus einer unbewiesenen
Ausfall-Untergrenze**.

### 5.12 `PendingRecoveryAnchor` – unveraenderlicher Ursprungsanker ueber Hop-1-Commit und mehrere Reboots

Ein einzelnes `pendingRecoveryOriginalState` (nur der restaurierte
`ProcessRuntimeState`) reicht nach dem ersten Hop-1-Commit nicht mehr aus:
der neue Current-Snapshot traegt `checkpointMonotonicMillis` des
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
    ProcessRuntimeState originalProcessState;             // 5.9 originalRestoredProcessState – rein strukturell (Phasen-/Formmatching), niemals fuer Boot-uebergreifende Arithmetik verwendet
    std::uint64_t knownPhaseSecondsAtOriginalCheckpoint;   // = (oldCheckpointMonotonicMillis - oldStateEnteredAtMillis) / 1000, EINMALIG bei Hop 1 berechnet, danach nur noch gelesen
    std::optional<std::int64_t> originalCheckpointUtc;     // RunPersistenceRawRecord.utcUnixSeconds DES VOR-AUSFALL-DATENSATZES, eingefroren bei Hop 1
    RunCheckpointTrigger originalCheckpointTrigger;        // Trigger desselben Vor-Ausfall-Datensatzes (reine Anzeigeinformation)
    std::uint32_t originalCheckpointIntervalMinutes;       // intervalMinutes desselben Vor-Ausfall-Datensatzes – Soll-Cadence-Anzeige und Eingabe fuer eine kuenftige belastbare Gap-Herleitung (5.13); dient NICHT als harter maximaler Gap
    PriorBootPhaseElapsed accumulatedBeforeEpisode{};   // 5.23s "alt"-Wert (Default {0U, nullopt}, identisch zu 5.23s eigener Konvention fuer eine fehlende/nicht passende Vor-Tag-Situation). Bedeutung gemaess Auftragspunkt 1: EINMALIG beim ERSTEN Hop 1 einer noch offenen Kette aus dem zu diesem Zeitpunkt bereits geladenen candidate.priorBootPhaseElapsed uebernommen (falls Tag passend vorhanden, sonst Default), BEVOR diese Kette ihre erste Episode faltet; bei jedem WEITEREN Carry-Forward-Hop-1 derselben, weiterhin offenen Kette (s. u., "Carry-Forward") byte-identisch unveraendert uebernommen, NICHT erneut aus candidate.priorBootPhaseElapsed neu erfasst (das wuerde bereits diese Kette selbst wieder einlesen und deren eigenen Beitrag doppelt zaehlen). Ohne dieses Feld wuerde eine spaetere reevaluateRecoveryTime-Faltung (s. u.) den vor dieser Kette akkumulierten Stand nicht mehr kennen, da der unmittelbar bei Hop 1 gefaltete candidate.priorBootPhaseElapsed selbst bereits nullopt ist, sobald die Ausfallzeit dieser Episode zum Hop-1-Zeitpunkt unbekannt war.
    std::uint64_t knownSecondsSinceOriginalCheckpoint{0U};  // NEU in dieser Revision (Auftragspunkt 1): kumulierte, sicher bekannte eingeschaltete Sekunden zwischen originalCheckpointUtc und dem Beginn der aktuellen, noch offenen Episode. 0 beim ERSTEN Hop 1 einer Kette; bei jedem Carry-Forward-Hop-1 (s. u.) um den Alt-Boot-lokalen Beitrag der soeben beendeten, bereits resumten Episode erhoeht (dieselbe Formel wie knownPhaseSecondsAtOriginalCheckpoint, nur gegen den zuletzt geladenen Datensatz statt gegen den urspruenglichen). originalCheckpointUtc/originalCheckpointTrigger/originalCheckpointIntervalMinutes bleiben dabei bewusst unveraendert (sie beschreiben weiterhin wahrheitsgemaess DEN einen Checkpoint, den sie ausweisen); dieses Feld traegt stattdessen den seither vergangenen, bekannten Anteil separat.
};
```

Zusaetzlich, **ausserhalb** dieses Ankers, ein zweites, bewusst getrenntes
Schema-3-Feld auf `RunPersistenceSnapshot`:

```cpp
std::optional<std::uint64_t> recoveryBootAnchorMonotonicMillis;
// Monotonic-Zeitpunkt DES AKTUELLEN BOOTS, an dem diese Recovery-Bewertung
// begann (bei Hop 1: der Hop-1-Zeitpunkt selbst; bei jedem Episode-Refresh,
// 5.15: neu auf den Episode-Refresh-Zeitpunkt DIESES Boots gesetzt). Lebt
// bewusst nicht im Anker: der Anker beschreibt ausschliesslich den
// urspruenglichen Vor-Ausfall-Kontext und darf durch keinen spaeteren Reboot
// veraendert werden; dieses Feld beschreibt dagegen exakt das Gegenteil –
// den je-Boot-lokalen Bezugspunkt – und MUSS bei jedem neuen Boot neu
// gesetzt werden, weil ein alter Boot-Monotonic-Wert in einem neuen Boot
// bedeutungslos ist.
```

**Ableitung des korrekten "aktuelle UTC"-Werts fuer 5.2 (reine Funktion,
ausschliesslich Boot-lokale Werte, run_recovery_time.hpp), vollstaendig
checked:**

```cpp
[[nodiscard]] std::optional<std::int64_t> deriveUtcAtRecoveryBootAnchor(
    std::optional<std::int64_t> utcNow, std::uint64_t nowMonotonicMillis,
    std::uint64_t recoveryBootAnchorMonotonicMillis) {
    if (!utcNow.has_value()) return std::nullopt;  // (noch) keine NTP-Synchronisation dieses Boots – kein Fehler, fachlich unbekannt
    if (*utcNow < 0) return std::nullopt;  // keine negative UTC – niemals nach uint64_t konvertieren
    if (nowMonotonicMillis < recoveryBootAnchorMonotonicMillis) {
        return std::nullopt;  // niemals in diesem Boot, Vorbedingung verletzt
    }
    const std::uint64_t elapsedMillis =
        nowMonotonicMillis - recoveryBootAnchorMonotonicMillis;
    const std::uint64_t elapsedSeconds = elapsedMillis / 1000U;
    if (elapsedSeconds >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;  // wuerde als int64_t nicht mehr darstellbar sein
    }
    const auto elapsedSecondsSigned = static_cast<std::int64_t>(elapsedSeconds);
    if (*utcNow < elapsedSecondsSigned) return std::nullopt;  // Subtraktion in int64_t, keine vorzeitige uint64_t-Konvertierung von utcNow
    return *utcNow - elapsedSecondsSigned;
}
```

**Signatur bewusst auf `std::optional<std::int64_t> utcNow` umgestellt
(gegenueber dem frueheren rohen `std::int64_t utcNow`):** der einzige
produktive Aufrufkontext, `RunCheckpointTime.utcUnixSeconds`
(`run_persistence_contract.hpp:42`), ist bereits heute
`std::optional<std::int64_t>` – "noch keine NTP-Synchronisation seit
diesem Boot" ist im bestehenden Vertrag also strukturell `nullopt`, kein
erfundener Sentinelwert (z. B. `-1`) und keine Sonderbehandlung an der
Aufrufstelle. Eine rohe Signatur haette jeden Aufrufer gezwungen, dieses
"noch unbekannt" selbst irgendwie in einen rohen `int64_t` zu kodieren,
bevor die Funktion ueberhaupt entscheiden koennte – das waere entweder ein
zweiter, hier erfundener Sentinelwert oder eine stillschweigende Annahme
gewesen, dass NTP zum Aufrufzeitpunkt immer schon verfuegbar ist (beides
mit genau dem Auftrags-Befund 1 unvereinbar). `!utcNow.has_value()` wird
identisch zu jedem anderen "UTC-Anker fehlt"-Fall behandelt (5.2: fehlender
UTC-Anker -> `computeRecoveryOutageBounds` liefert `nullopt` ->
`RecoveredPhaseElapsed.totalSecondsUpperBound == nullopt`, fachlich
unbekannt, kein Rechenfehler).

Reihenfolge verbindlich: zuerst `!utcNow.has_value()` ablehnen, danach
(erst nach dem Entpacken) `*utcNow < 0` ablehnen, danach die Boot-lokale
Monotonic-Differenz bilden, danach deren Darstellbarkeit als `int64_t`
pruefen, erst danach die eigentliche Subtraktion – ausschliesslich in
`std::int64_t`, nie ueber eine Zwischenkonvertierung von `*utcNow` nach
`std::uint64_t` (eine solche Konvertierung wuerde ein bereits negatives
`*utcNow` in einen riesigen positiven Wert verwandeln und die nachfolgende
Pruefung unbrauchbar machen).

Beide Monotonic-Eingaben stammen garantiert aus demselben Boot (der
Aufrufer liest `recoveryBootAnchorMonotonicMillis` aus dem gerade aktiven,
in diesem Boot geladenen `RunCommandState`) – die Subtraktion ist damit
immer boot-lokal, niemals boot-uebergreifend. Das Ergebnis ersetzt in 5.2
`RecoveryOutageBoundsInput.utcAtRestartBoundary`, niemals eine roh zum
Abfragezeitpunkt gelesene `utcNow`.

**Verbindlicher Vertrag – Hop 1 konstruiert den Anker bedingt (frisch oder
Carry-Forward, Auftragspunkt 1):**

Bei Hop 1 (5.9), sowohl in `activateLoadedRun` als auch identisch in
`activateFallbackRecoveredRun` (5.8 – beide konstruieren den Anker aus dem
fuer ihren jeweiligen Fall autoritativen internen Datensatz, dieselbe
Fallunterscheidung gilt fuer beide unveraendert), wird zuerst geprueft, ob
der geladene Datensatz selbst bereits einen `pendingRecoveryAnchor` traegt
(`loadedRecord.snapshot.pendingRecoveryAnchor.has_value()`). Das ist
strukturell nur moeglich, wenn `loadedRecord.snapshot.processState.state !=
RecoveryEvaluation` (sonst waere dies gemaess 5.15 ein Episode-Refresh, kein
Hop 1) **und** damit gemaess 5.14 Punkt 3 zwingend `state in
{WaitingForProduct, Fermenting, CoolHolding}` mit weiterhin offener
Zeitbewertung – ein frueherer Resume dieser Kette war also bereits
erfolgreich, ihre Zeitfrage aber noch nicht abgeschlossen. Diese Praesenz
allein ist damit hinreichend, um zwischen zwei Faellen zu unterscheiden,
ohne zusaetzlich die Zielphase separat zu vergleichen:

1. **Frischer Fall** (`loadedRecord.snapshot.pendingRecoveryAnchor ==
   nullopt` – kein vorheriger, noch offener Zeitkontext fuer diese Phase):
   `PendingRecoveryAnchor` wird **einmalig**, vollstaendig neu konstruiert:
   `knownPhaseSecondsAtOriginalCheckpoint = thisHopAltBootLocalSeconds =
   (loadedRecord.snapshot.checkpointMonotonicMillis
   - originalRestoredProcessState.stateEnteredAtMillis) / 1000` (Definition
   von `thisHopAltBootLocalSeconds` s. u. "Carry-Forward-Fall" – hier
   identisch mit dem neu gesetzten Feld selbst),
   `originalCheckpointUtc = loadedRecord.utcUnixSeconds`,
   `originalCheckpointTrigger = loadedRecord.snapshot.trigger`,
   `originalCheckpointIntervalMinutes = loadedRecord.snapshot.intervalMinutes`,
   `accumulatedBeforeEpisode = (candidate.priorBootPhaseElapsed.has_value()
   && candidate.priorBootPhaseElapsed->taggedState == Zielphase) ?
   candidate.priorBootPhaseElapsed->elapsed : PriorBootPhaseElapsed{}`
   (derselbe "alt"-Wert, den 5.23s Akkumulationsregel ohnehin fuer diese
   Episode verwendet), `knownSecondsSinceOriginalCheckpoint = 0`.
2. **Carry-Forward-Fall** (`loadedRecord.snapshot.pendingRecoveryAnchor.has_value()`
   – die Zeitfrage einer frueheren Episode derselben Phase ist noch offen,
   neuer Auftragspunkt 1): `originalProcessState`,
   `originalCheckpointUtc`, `originalCheckpointTrigger`,
   `originalCheckpointIntervalMinutes` und `accumulatedBeforeEpisode`
   werden **byte-identisch** aus `loadedRecord.snapshot.pendingRecoveryAnchor`
   uebernommen, **nicht** neu konstruiert; `knownPhaseSecondsAtOriginalCheckpoint`
   ebenso byte-identisch uebernommen (bleibt der Alt-Boot-lokale Anteil
   **vor** `originalCheckpointUtc`, s. o.). Einzige Aenderung:
   `knownSecondsSinceOriginalCheckpoint = checked_add(loadedRecord.snapshot.pendingRecoveryAnchor->knownSecondsSinceOriginalCheckpoint,
   thisHopAltBootLocalSeconds)`, wobei

   ```text
   thisHopAltBootLocalSeconds = (loadedRecord.snapshot.checkpointMonotonicMillis
       - originalRestoredProcessState.stateEnteredAtMillis) / 1000
   ```

   **derselbe Alt-Boot-lokale Beitrag der soeben beendeten, bereits
   resumten Episode, den der frische Fall als
   `knownPhaseSecondsAtOriginalCheckpoint` speichern wuerde** (dieselbe
   Formel, hier nur auf den bestehenden Carry-Forward-Zaehler addiert
   statt den Ursprungscheckpoint zu ersetzen; `checked_add`, Ueberlauf
   praktisch unerreichbar; im Theoriefall behandelt wie jeder andere
   Rechenfehler dieses Plans, s. u. "Checked Arithmetic fuer den
   Carry-Forward-Zaehler"). `thisHopAltBootLocalSeconds` ist damit **der
   einzige neue, von diesem konkreten Hop 1 selbst beigetragene Anteil** –
   im frischen Fall identisch mit dem neu gesetzten
   `knownPhaseSecondsAtOriginalCheckpoint` selbst, im Carry-Forward-Fall
   identisch mit dem Zuwachs von `knownSecondsSinceOriginalCheckpoint`.
   Diese einheitliche Grosse wird von 5.22s `observedRunSeconds`-Fold
   wiederverwendet (s. dort) – niemals die **gesamte**, bereits akkumulierte
   `knownSecondsSinceOriginalCheckpoint`, die auch fruehere Episoden dieser
   Kette enthaelt und sonst bei jedem weiteren Carry-Forward erneut
   mitgezaehlt wuerde.

In beiden Faellen: `recoveryBootAnchorMonotonicMillis = monotonicMillis`
(der Hop-1-Zeitpunkt **dieses** Boots, wie bisher).

- Episode-Refresh (5.15) uebernimmt `pendingRecoveryAnchor` **byte-identisch**
  unveraendert (inklusive `knownSecondsSinceOriginalCheckpoint`), setzt aber
  `recoveryBootAnchorMonotonicMillis` **neu** auf den
  Episode-Refresh-Zeitpunkt **dieses** Boots.
- Automatische Reevaluation (`reevaluateRecoveryTime`, s. u.) und der
  Benutzerpfad (`resolveRecoveryOutcome`, 5.17) lesen **ausschliesslich**
  `pendingRecoveryAnchor` und `recoveryBootAnchorMonotonicMillis` aus dem
  aktuell geladenen `RunCommandState` – niemals aus dem zuletzt physisch
  geschriebenen Datensatz direkt.
- Ein Hop-1-/Episode-Refresh-Checkpoint ersetzt den Anker selbst nicht;
  lediglich das separate `recoveryBootAnchorMonotonicMillis` wird bei jedem
  Episode-Refresh und beim initialen Hop 1 neu gesetzt (s. o.).
- Ein zweiter/dritter Reboot waehrend `RecoveryEvaluation` bleibt korrekt
  berechenbar, weil `pendingRecoveryAnchor` als Teil jedes persistierten
  Snapshots unveraendert mitgefuehrt wird und `recoveryBootAnchorMonotonicMillis`
  bei jedem dieser Reboots ueber Episode-Refresh neu und korrekt (boot-lokal)
  gesetzt wird.

**Wirksame Zeitbasis – ein einziger, an allen drei Auswertungsstellen
wiederverwendeter Ableitungsschritt (DRY, neu in dieser Revision):** Hop 1s
eigene Verdikt-/Bounds-Auswertung (5.4, fuer den automatischen
Hop-1-only-Pfad von `WaitingForProduct`, 5.11), `reevaluateRecoveryTime`
(s. u.) **und** `resolveRecoveryOutcome`s Verdikt-Neuberechnung fuer
`WaitingForProduct` (5.17 – auf einer carry-forwarded Kette ueber
`AssumeStillValid` + erneutem zweiten Ausfall genauso erreichbar wie die
anderen beiden) verwenden **nicht** `anchor.originalCheckpointUtc` und
`anchor.knownPhaseSecondsAtOriginalCheckpoint` direkt, sondern eine
kleine, reine Ableitung:

```cpp
// run_recovery_time.hpp
struct EffectiveAnchorTimeBasis {
    std::optional<std::int64_t> effectiveCheckpointUtc;
    std::uint64_t effectiveKnownSecondsBeforeCheckpoint;
};

[[nodiscard]] EffectiveAnchorTimeBasis deriveEffectiveAnchorTimeBasis(
    const PendingRecoveryAnchor& anchor) {
    return EffectiveAnchorTimeBasis{
        .effectiveCheckpointUtc = anchor.originalCheckpointUtc.has_value()
            ? checked_add_signed_unsigned(*anchor.originalCheckpointUtc,
                                           anchor.knownSecondsSinceOriginalCheckpoint)
            : std::nullopt,  // kein Ursprungsanker -> bleibt unbekannt, unabhaengig vom Carry-Forward-Zaehler
        .effectiveKnownSecondsBeforeCheckpoint =
            checked_add(anchor.knownPhaseSecondsAtOriginalCheckpoint,
                        anchor.knownSecondsSinceOriginalCheckpoint),
    };
}
```

`effectiveCheckpointUtc` ersetzt `anchor.originalCheckpointUtc` als
`RecoveryOutageBoundsInput.utcAtLastCheckpoint` (5.2);
`effectiveKnownSecondsBeforeCheckpoint` ersetzt
`anchor.knownPhaseSecondsAtOriginalCheckpoint` als
`RecoveredPhaseElapsedInput.knownSecondsBeforeCheckpoint` (5.3) – **an
jeder Stelle**, an der bislang die rohen Anker-Felder gelesen wurden. Fuer
einen frischen Anker (`knownSecondsSinceOriginalCheckpoint == 0`) ist das
Ergebnis identisch zum frischen Ankerfall (`effectiveCheckpointUtc ==
originalCheckpointUtc`, `effectiveKnownSecondsBeforeCheckpoint ==
knownPhaseSecondsAtOriginalCheckpoint`) – keine Verhaltensaenderung fuer
den Einzelausfall-Fall.

**Checked Arithmetic fuer den Carry-Forward-Zaehler:** `checked_add`
(`std::uint64_t`) folgt derselben Konvention wie 5.3
(`computeRecoveredPhaseElapsed`); `checked_add_signed_unsigned`
(`std::int64_t + std::uint64_t`) prueft vor der Addition, dass das Ergebnis
als `std::int64_t` darstellbar bleibt (analog zu
`deriveUtcAtRecoveryBootAnchor`s bereits bestehenden Grenzwertpruefungen,
s. o.). Im praktisch unerreichbaren Ueberlauffall liefert
`deriveEffectiveAnchorTimeBasis` `effectiveCheckpointUtc == nullopt`
(degradiert fail-safe auf "Zeitfrage unbeweisbar", niemals ein
Programmabbruch oder eine geratene Zahl) bzw. behandelt einen Ueberlauf von
`effectiveKnownSecondsBeforeCheckpoint` wie jeden anderen technischen
Additionsfehler dieses Plans (5.3: Funktionsebene `nullopt`, niemals ein
fachliches `Uncertain`).

**Zwei getrennte, aber zusammenwirkende Effekte des Carry-Forward, beide
ausdruecklich benannt statt eines einzigen ueberzeichneten Vorteils:**

1. **Erhalt der Aufloesbarkeit (der eigentliche Auftragspunkt):** ein
   bereits bekanntes `originalCheckpointUtc` (`t0`, z. B. weil NTP vor dem
   ersten Ausfall der Kette verfuegbar war) bleibt ueber beliebig viele
   weitere Reboots erhalten, statt beim naechsten Ausfall verworfen zu
   werden. Sobald spaeter (auf einem beliebigen Boot dieser Kette) NTP
   verfuegbar wird, ist `computeRecoveryOutageBounds` fuer die **gesamte**
   Kette auswertbar, nicht nur fuer deren letzte Episode.
2. **Zusaetzliche Praezisierung der Untergrenze durch
   `knownSecondsSinceOriginalCheckpoint` (Nebeneffekt, nicht der
   Kernpunkt):** dieser Zaehler ist nur dann `> 0`, wenn zwischen dem
   Resume der vorigen Episode und dem naechsten Ausfall tatsaechlich ein
   weiterer Checkpoint geschrieben wurde (z. B. ein Benutzerkommando, eine
   Sensorselektion oder eine erfolgreiche `reevaluateRecoveryTime`-Korrektur
   – **nicht** durch periodisches Checkpointing, s. 5.13:
   `checkpointPeriodic()` hat keinen produktiven Aufrufer). Im
   Regelfall ohne zwischenzeitliche Aktivitaet ist
   `loadedRecord.snapshot.checkpointMonotonicMillis ==
   originalRestoredProcessState.stateEnteredAtMillis` (derselbe
   Resume-Commit ist der zuletzt geschriebene Datensatz), also
   `knownSecondsSinceOriginalCheckpoint`-Zuwachs `== 0` – Punkt 1 (Erhalt
   der Aufloesbarkeit) bleibt davon vollstaendig unberuehrt und ist der
   tatsaechlich verlangte, immer wirksame Teil dieser Korrektur.

**Anker-Lebenszyklus bei Resume – bedingt, nicht unbedingt:** Ein
unbedingtes Loeschen von `pendingRecoveryAnchor`/
`recoveryBootAnchorMonotonicMillis` bei jedem erfolgreichen Resume, mit der
Begruendung, `priorBootPhaseElapsed`
(5.23) trage die Entscheidungsgrundlage bereits vollstaendig weiter. Das
gilt nur, wenn die Ausfallzeit **bereits** UTC-seitig aufgeloest war – ist
sie das nicht (keine NTP-Synchronisation zum Resume-Zeitpunkt, der
Regelfall bei einem Kaltstart ohne WLAN/Internet), ist der Anker die
**einzige** Quelle, aus der eine spaetere, praezisere Bewertung ueberhaupt
noch abgeleitet werden koennte: `priorBootPhaseElapsed` enthaelt nur bereits
verrechnete Sekundenwerte, nicht den `originalCheckpointUtc`, aus dem eine
spaetere `computeRecoveryOutageBounds`-Berechnung eine bislang unbekannte
Obergrenze gewinnen koennte. Ein unbedingtes Loeschen macht diese spaetere
Aufloesung **strukturell unmoeglich** – exakt der vom Owner benannte Fehler.

Der Vertrag wird deshalb **an jeder Stelle, an der ein Resume(-artiger)
Uebergang stattfindet**, auf eine einzige, einheitliche Bedingung
umgestellt (Aktor- und Zeitfrage bewusst entkoppelt, s. u. "Zwei getrennte
Fragen"):

```cpp
// run_recovery.hpp – reine Abfrage, keine Mutation.
[[nodiscard]] bool recoveryTimeResolvedAtResume(
    const std::optional<TaggedPriorBootPhaseElapsed>& accumulated) {
    if (!accumulated.has_value()) {
        return true;  // Phase traegt keine Grenze/keinen Vor-Boot-Anteil (5.7:
                       // "keiner" fuer Preheating/ReachingTarget/Cooling/
                       // ManualHolding) – keine Zeitfrage, sofort loeschen,
                       // fuer jede Phase ohne eigene Zeitfrage.
    }
    return accumulated->elapsed.upperBoundSeconds.has_value();
}
```

**Wichtig, Abgrenzung gegen einen naheliegenden Fehler:** `!accumulated.has_value()`
bedeutet **nicht** "Zeitfrage unbekannt", sondern "diese Phase hat gar keine
Zeitfrage" (5.23 "Setzen" taggt ausschliesslich `WaitingForProduct`/
`Fermenting`/`CoolHolding` – jede andere Zielphase eines Resumes bleibt
`nullopt`). Eine fruehere Fassung dieser Funktion behandelte "kein
`priorBootPhaseElapsed`" faelschlich identisch zu "Obergrenze unbekannt"
und haette den Anker damit auch nach einem Resume nach `Cooling`/
`Preheating`/`ReachingTarget`/`ManualHolding` faelschlich bestehen lassen
– ein gemaess 5.14 Punkt 3 (dort auf genau die drei grenztragenden Phasen
beschraenkt) strukturell ungueltiger Snapshot. Die obige Fassung liefert
fuer diese vier Phasen immer `true` (sofortige Loeschung); fuer die drei
grenztragenden Phasen bleibt der Anker bei unbekannter Obergrenze erhalten.

- Ist `recoveryTimeResolvedAtResume(candidate.priorBootPhaseElapsed)`
  **nach** der Akkumulation dieser Episode (5.23) `true` (die
  **akkumulierte** Obergrenze ueber alle bisherigen Episoden dieser Phase
  ist bekannt – nicht nur diese eine Episode fuer sich), werden
  `pendingRecoveryAnchor`/`recoveryBootAnchorMonotonicMillis` im selben
  Commit auf `nullopt` gesetzt (**nach** der geordneten
  Sensorevidenz-Latch-Sequenz aus 5.20, niemals davor) – identisch zu
  Das bestehende Verhalten fuer den Fall, dass die Zeit von Anfang an
  aufgeloest war.
- Ist sie `false` (Obergrenze weiterhin unbekannt), bleiben
  `pendingRecoveryAnchor`/`recoveryBootAnchorMonotonicMillis` **unveraendert
  bestehen** – **auch dann, wenn der Resume selbst bereits erfolgreich war**
  (`Fermenting`/`CoolHolding` unbedingt, `WaitingForProduct` bei
  `DefinitelyStillValid` oder bei einem bestaetigenden Benutzerentscheid,
  5.17). Dies ist die neue "Recovery-Zeitbewertung noch offen"-Situation:
  der Aktor laeuft bereits regulaer (5.17 unten, "Zwei getrennte Fragen"),
  aber der fuer diese Episode zustaendige Zeitkontext bleibt persistent
  erhalten, bis entweder (a) `reevaluateRecoveryTime` (s. u.) ihn spaeter
  aufloest, (b) die Phase real wechselt/abschliesst (5.23, dieselbe
  `RecoveryReentryRequired`/`RecoveryResumed`-Ausnahme gilt fuer diesen
  Anker analog, s. dort) oder (c) ein Tombstone/`clearActiveRunState()`
  ihn fachlich obsolet macht.
- Diese Regel gilt **einheitlich** an jeder Stelle, die einen Resume
  durchfuehrt: Hop 2 (automatisch, 5.10), `resolveRecoveryOutcome` mit
  `AssumeStillValid` fuer `WaitingForProduct` und jeder zukuenftige
  gleichartige Pfad – eine einzige Regel statt eines an jeder Stelle erneut
  zu treffenden Sonderfalls (DRY).

**Manuelle Restdauer-Neubaseline – Recovery-Zeit der alten Baseline ist
fachlich superseded:** Eine bestaetigte `AdjustRun`-Entscheidung mit
`durationChanged == true` waehrend `Fermenting` beginnt eine neue
Restdauerentscheidung ab `request.envelope.monotonicMillis`. Das gilt auch
bei einer kombinierten Zieltemperatur- und Restdaueranpassung. Es gilt nicht
bei `durationChanged == false`, insbesondere nicht bei einer reinen
Zieltemperaturaenderung.

Der bestehende `decideRunAdjustment()`-Pfad bleibt die einzige
Laufanpassungsentscheidung. In seiner Kandidatenmutation ist die Reihenfolge
verbindlich:

1. Vor jeder Kandidatenmutation werden erwartete Lauf-/Kommando-Revision,
   monotoner Zeitbezug und `observedRunSeconds`-Kapazitaet geprueft. Der
   Kommandozeitpunkt darf nicht vor dem bisherigen
   `processState.stateEnteredAtMillis` liegen. Der seit diesem alten
   Timerstart sicher beobachtete Fermenting-Anteil wird checked als
   `deriveFermentingSecondsDelta(before, command.monotonicMillis)` berechnet;
   die Addition zu `runProgress.observedRunSeconds` muss ohne
   `std::uint32_t`-Ueberlauf moeglich sein. Stale-, Zeit- oder
   Kapazitaetsfehler verlassen den Pfad ohne irgendeine Feldmutation.
2. Dieser Deltawert wird in der lokalen Kandidatenkopie genau einmal in
   `runProgress.observedRunSeconds` gefaltet. Die historische Basis
   (`KnownTotal` oder `PartialUnknownHistory`) bleibt unveraendert.
3. Die bereits bestehende `ActiveRun`-Entscheidung wird auf dem Kandidaten
   angewandt; `makeProcessRunSnapshot(ActiveRun)` projiziert weiterhin exakt
   das neue `EffectiveRunValues::remainingDurationMinutes` auf
   `ProcessRunSnapshot::fermentationDurationMinutes`.
4. Der Kandidat setzt `processState.stateEnteredAtMillis` auf den
   Kommandozeitpunkt und ersetzt die aktive Zeitbasis fuer die neue
   Restdauer. `priorBootPhaseElapsed` wird dabei getaggt fuer `Fermenting`
   auf `lowerBoundSeconds == 0` und `upperBoundSeconds == 0` gesetzt. Damit
   ist kein alter priorer Zeitbeitrag mehr wirksam, waehrend der neue
   Baselinepunkt ausdruecklich und nicht als unbekannter Zustand modelliert
   ist.
5. `nominalRecoveryAdjustment` wird auf `nullopt` gesetzt. Falls die
   Implementierung das Feld im Kandidaten behaelt, bedeutet es ab diesem
   Kommando ausschliesslich eine seit dieser manuellen Restdauer-Baseline
   bestaetigte Korrektur; eine unbegrenzte Historie wird nicht eingefuehrt.
6. `pendingRecoveryAnchor` und `recoveryBootAnchorMonotonicMillis` werden
   atomar auf `nullopt` gesetzt. Damit ist die alte offene
   Recovery-Zeitfrage geschlossen und ein spaeterer NTP-Abgleich kann diese
   alte Baseline nicht mehr in die neue Restdauerentscheidung einbringen.
   `lastRecoveryEpisodeEvidence` darf gemaess seinem bestehenden
   Diagnoselebenszyklus im Kandidaten erhalten bleiben; es ist kein
   Zeitgeber und wird von keiner Dauerentscheidung gelesen.
7. Erst der vollstaendige Kandidat wird ueber die bestehende
   Write-before-Apply-/`persistCommand`-Kette persistiert. Nach bestaetigtem
   Commit werden neue Restdauer, Snapshot, Timer-Baseline und die
   supersedeten Recovery-Felder gemeinsam im RAM angewandt. Der bestehende
   `RunRevision`-Eintrag mit altem/neuem `remainingDurationMinutes`,
   `remainingDurationChanged` und dem Kommandozeitbezug ist der kanonische
   Nachweis der manuellen Neufestlegung; kein zweites Baseline- oder
   Journalfeld wird eingefuehrt.

Die Folge ist absichtlich schmal: `observedRunSeconds` behaelt die gesamte
tatsaechlich beobachtete Historie, ist aber kein Restdauer-Timer. Der neue
Timer misst nur ab dem neuen `stateEnteredAtMillis`; `priorBootPhaseElapsed`
und `nominalRecoveryAdjustment` tragen keinen alten Kredit hinueber. Ein
`remainingDurationMinutes == 0` bleibt deshalb unmittelbar zulaessig, sobald
die bestehende Fermenting-Abschlusspruefung nach dem Commit laeuft. Ein
spaeterer Stromausfall konstruiert seinen Hop-1-Kontext wieder aus diesem
neuen Baselinepunkt; nur seit dieser Baseline tatsaechlich bekannte Zeit wird
in den neuen Recovery-Kontext aufgenommen.

**Reine Zieltemperaturaenderung:** Bei `durationChanged == false` bleiben
`stateEnteredAtMillis`, `priorBootPhaseElapsed`,
`nominalRecoveryAdjustment`, `pendingRecoveryAnchor` und
`recoveryBootAnchorMonotonicMillis` unveraendert. Der bestehende Vertrag
"verbleibende Dauer laeuft ohne Unterbrechung weiter" wird damit nicht
versehentlich in eine Restdauer-Neubaseline umgedeutet.

**Reboot waehrend noch offener Zeitbewertung eines bereits resumten Laufs –
Carry-Forward statt Verlust (Auftragspunkt 1):** laedt `loadAndInitialize()`
einen Snapshot mit
`processState.state in {Fermenting, CoolHolding, WaitingForProduct}`
**und** weiterhin gesetztem `pendingRecoveryAnchor` (Zeitbewertung war beim
vorigen Reboot noch offen), ist das gemaess 5.15 strukturell ein
regulaerer `LoadedActiveRun` (`state != RecoveryEvaluation`) – **kein**
Episode-Refresh, sondern ein **frischer Hop 1**. Das ist zwingend, nicht
optional: der Aktor war zwischen dem letzten Resume und diesem neuen
Reboot tatsaechlich freigegeben und lief; ein erneuter physischer Ausfall
verlangt dieselbe Aktor-Sicherheits-Neubewertung (Gate A,
Sensor-Reselektion) wie jeder andere Hop 1 – eine "vereinfachte"
Wiederaufnahme ohne diese Neubewertung waere eine unzulaessig
vorausgesetzte Aktorfreigabe ueber einen Reboot hinweg. **Diese
Aktor-Neubewertung ist von der Zeitfrage vollstaendig unabhaengig** (Punkt
1, "Zwei getrennte Fragen") – der folgende Absatz betrifft ausschliesslich
die zweite Frage.

Dieser frische Hop 1 ist per Definition der oben beschriebene
**Carry-Forward-Fall**: `loadedRecord.snapshot.pendingRecoveryAnchor` ist
gesetzt, also werden `originalCheckpointUtc`,
`knownPhaseSecondsAtOriginalCheckpoint`, `originalCheckpointTrigger`,
`originalCheckpointIntervalMinutes` und `accumulatedBeforeEpisode`
byte-identisch aus dem alten Anker uebernommen (Episode 1s Ursprungskontext
bleibt erhalten, wird **nicht** ueberschrieben) und lediglich
`knownSecondsSinceOriginalCheckpoint` um Episode 1s eigenen, seit dem
Resume bekannt gewordenen Alt-Boot-lokalen Beitrag erhoeht. Damit bleibt
ein **vor** Episode 1 bekanntes `originalCheckpointUtc` fuer die **gesamte**
Kette (Episode 1 + Episode 2 + ggf. weitere) auswertbar, sobald irgendwann
auf irgendeinem Boot dieser Kette NTP verfuegbar wird – nicht nur fuer die
jeweils letzte Episode.

Die 5.23-Akkumulationsregel selbst aendert sich dabei nicht in ihrer Form,
nur in ihrer Eingabe: `alt` bleibt `pendingRecoveryAnchor->accumulatedBeforeEpisode`
(der VOR der gesamten Kette eingefrorene Stand, **nicht**
`candidate.priorBootPhaseElapsed` – dieses wuerde bereits Episode 1s
eigenen, in der Kette selbst enthaltenen Beitrag zurueckspiegeln und ihn
bei erneuter Addition doppelt zaehlen, s. u. "Kein doppeltes Zaehlen");
`knownSecondsBeforeCheckpoint`/`utcAtLastCheckpoint` sind die ueber
`deriveEffectiveAnchorTimeBasis` (s. o.) aus dem carry-forwarded Anker
abgeleiteten wirksamen Werte, nicht die rohen Einzelfelder. Ist
`originalCheckpointUtc` bereits vor Episode 1 unbekannt gewesen (kein NTP
je vor Beginn dieser Kette verfuegbar), bleibt `effectiveCheckpointUtc`
unabhaengig vom Carry-Forward `nullopt` – dieser Fall bleibt unveraendert
unaufloesbar (keine erfundene Zeit, Gate C), aber das ist eine **andere**
Ursache als den zuvor ausdruecklich ausgeschlossenen Informationsverlust, hier
korrigierte Ueberschreiben eines **bereits bekannten** Ursprungscheckpoints.

**Kein doppeltes Zaehlen (verbindliche Invariante, Auftrag woertlich):**
nach einer Kette aus **drei** Episoden (zwei Carry-Forwards) mit
Alt-Boot-lokalen Beitraegen `N1` (Episode 1, vor `originalCheckpointUtc`),
`N2` (Episode 2, carry-forward-akkumuliert) und `N3` (Episode 3,
carry-forward-akkumuliert) gilt fuer die akkumulierte Untergrenze
(sofern die vorherige `accumulatedBeforeEpisode` selbst `0` war):
`neu.lowerBoundSeconds == accumulatedBeforeEpisode.lowerBoundSeconds + N1
+ N2 + N3` – jedes `N_k` erscheint in dieser Summe **genau einmal**, unabhaengig
davon, ob es ueber `knownPhaseSecondsAtOriginalCheckpoint` (nur `N1`) oder
ueber `knownSecondsSinceOriginalCheckpoint` (`N2+N3`, kumulativ) gefuehrt
wird. Der Test in Abschnitt 8 prueft diese Summe explizit fuer eine
Drei-Reboot-Kette.

**Sicher bekannte eingeschaltete Zeit zwischen Ausfaellen zaehlt nicht als
Ausfallzeit (Auftrag woertlich):** `knownSecondsSinceOriginalCheckpoint`
wird ausschliesslich aus boot-lokalen, bereits checkpoint-belegten
Zeitwerten gebildet (wie `knownPhaseSecondsAtOriginalCheckpoint` selbst,
5.3) und fliesst ueber `effectiveKnownSecondsBeforeCheckpoint` in **beide**
Grenzen von `RecoveredPhaseElapsed` symmetrisch und ungeschmaelert ein
(anders als eine unbewiesene Ausfallzeit, die nach 5.13 nur die Obergrenze
erreicht) – sie wird an keiner Stelle als potenzielle Ausfalldauer
behandelt.

**Untergrenzen-Vertrag fuer einen carry-forwarded Anker (Anpassung an
5.13, verbindlich, sicherheitsrelevant):** 5.13s Kontrollpunktabstands-Luecke
(`outageSecondsLowerBound = saturating_sub(upperBound, maxCheckpointGapSeconds, 0)`)
kompensiert genau **eine** Checkpoint-zu-Ausfall-Verzoegerung. Eine
carry-forwarded Kette durchlaeuft **mehrere** solcher Luecken (eine je
Episode), sodass dieselbe Formel unveraendert auf die **kumulierte**
Ausfallspanne angewandt bei einem kuenftig tatsaechlich bewiesenen
`maxCheckpointGapSeconds` die Untergrenze um bis zu `(Episodenzahl-1) *
maxCheckpointGapSeconds` **ueberhoehen** wuerde – eine faelschlich zu
grosse Untergrenze ist unter 5.13 der sicherheitsrelevante Fehlerfall
(falsches `DefinitelyExpired`, ungerechtfertigter Tombstone). Deshalb gilt
zusaetzlich zu 5.2/5.13: **ist `anchor.knownSecondsSinceOriginalCheckpoint
> 0`** (die Kette wurde mindestens einmal carry-forwarded), **bleibt
`outageSecondsLowerBound` unbedingt `0`**, unabhaengig vom Wert eines
kuenftig gesetzten `maxCheckpointGapSeconds` – die Luecken-Kompensation
gilt ausschliesslich fuer einen Anker mit genau einer Episode. Heute
(`maxCheckpointGapSeconds` immer `nullopt`, 5.13) ist dieser Zusatz
beobachtungsgleich zum bestehenden Verhalten; er wird normativ, sobald
5.13s offener Folgepunkt (ein bewiesener Gap) tatsaechlich umgesetzt wird.
Eine kuenftige, engere Untergrenze fuer den Mehrfach-Episoden-Fall (z. B.
ueber einen zusaetzlichen Episodenzaehler) ist ausdruecklich **nicht** Teil
von #18 (mehr Feld, mehr Testflaeche, KISS).

**Phasenwechsel/Abschluss vor Zeitaufloesung macht die offene Frage
fachlich obsolet (Praezisierung, Auftragspunkt 1):** wechselt die Phase vor
einer Zeitaufloesung real (z. B. `Fermenting -> Cooling` bei signalbasiertem
Abschluss, oder ein Tombstone/`Completed`), wird der Anker gemaess der
bestehenden `RecoveryReentryRequired`/`RecoveryResumed`-Ausnahme (5.23) und
der Tombstone-/Abschluss-Loeschregel (5.11/5.17) geloescht, **ohne** dass
die Zeitfrage jemals explizit aufgeloest wurde. Das ist kein Sonderfall:
die akkumulierten Bounds haben ausschliesslich der Dauer-/Abschlussgrenze
**dieser** Phase gedient (5.4/5.17/5.22); ein echter Phasenwechsel
beendet genau diese Entscheidung selbst – der jetzt unbeantwortbare Teil
der Zeitfrage ("wie lange dauerte Ausfall X genau") wird damit fachlich
gegenstandslos, nicht heimlich als geloest behandelt. Der bereits
carry-forward-erhaltene, sicher bekannte Anteil
(`accumulatedBeforeEpisode`/`knownPhaseSecondsAtOriginalCheckpoint`/
`knownSecondsSinceOriginalCheckpoint`) ist zu diesem Zeitpunkt ohnehin
bereits vollstaendig in `priorBootPhaseElapsed`/`observedRunSeconds`
eingeflossen (5.22/5.23) und geht durch die Anker-Loeschung nicht
verloren – nur der ANKER (die noch offene ZeitFRAGE selbst) wird
geloescht, nicht die bereits gesicherten Sekundenwerte.

**Zwei getrennte Fragen, ausdruecklich entkoppelt (Kern von
Auftragspunkt 1):**

1. **Aktor-/Phasen-Recovery:** "Darf die Regelung fuer diese Phase sicher
   wieder laufen?" – beantwortet durch Hop 1 (Gate A, Sensor-Reselektion,
   Topologie) und Hop 2 (Resume-Entscheidung), unveraendert nach 5.9/5.10.
   Diese Frage ist **unabhaengig** von NTP/absoluter Zeit: `Fermenting`/
   `CoolHolding` resumen unveraendert unbedingt (5.17); `WaitingForProduct`
   resumt bei `DefinitelyStillValid` oder einem bestaetigenden
   Benutzerentscheid (5.17).
2. **Recovery-Zeitbewertung:** "Wie lange dauerte der Ausfall tatsaechlich,
   und was folgt daraus fuer Fortschritt/Abschlussgrenze/Konfidenz?" –
   bleibt offen, bis ein UTC-Anker verfuegbar wird oder die Phase sie
   obsolet macht. Diese Frage sperrt **niemals** die unter Punkt 1 bereits
   erteilte Aktorfreigabe erneut (verbindlich, s. Tests unten und 5.17)
   – kein Code-Pfad prueft `pendingRecoveryAnchor.has_value()` als
   Vorbedingung fuer eine normale, laufende Regelentscheidung
   (`decideFermenting`/`decideCoolHolding`/`decideWaitingForProduct`
   bleiben davon vollstaendig unberuehrt).

**`reevaluateRecoveryTime` – gemeinsame Zeit-Reevaluations-API, erweitert
auf bereits resumte Laeufe (bewusst als
`reevaluatePendingRecovery`):**

```cpp
[[nodiscard]] RunPersistenceResult
RunRecoveryCoordinator::reevaluateRecoveryTime(
    RunCommandState& current, const RunCheckpointTime& time);
```

Vorbedingung: `current.pendingRecoveryAnchor.has_value()` –
**unabhaengig** von `current.processState.state` (sonst
`unavailableResult()`). Der Vertrag liest **denselben** Anker unabhaengig davon, ob
er zu einer noch offenen `RecoveryEvaluation` (Hop-1-only,
`WaitingForProduct`) oder zu einem bereits resumten, aktorfreigegebenen
Lauf (`Fermenting`/`CoolHolding`/`WaitingForProduct` nach Resume) gehoert.

Ablauf:

1. Neuberechnung von 5.2-5.3 mit dem (weiterhin bestehenden) Anker – **ueber
   `deriveEffectiveAnchorTimeBasis(pendingRecoveryAnchor)` (s. o.), nicht
   ueber die rohen Anker-Felder direkt**, damit ein ggf. carry-forwarded
   `knownSecondsSinceOriginalCheckpoint` korrekt einfliesst – und der
   frisch uebergebenen `time` (`deriveUtcAtRecoveryBootAnchor` mit
   `time.utcUnixSeconds`, s. o.).
2. **Fall `current.processState.state == RecoveryEvaluation`
   (Hop-1-only):** identisch zum bestehenden Vertrag – nur `WaitingForProduct`
   erreicht diesen Zustand; ergibt sich aus Unter- **und** Obergrenze
   bereits dieselbe fachliche Entscheidung (`DefinitelyExpired`/
   `DefinitelyStillValid`), wird automatisch Tombstone/Resume ausgeloest
   (5.11/5.10). Schreibt **niemals** `observedRunSeconds` oder
   `NominalRecoveryAdjustmentState` (Gate C, unveraendert).
3. **Fall `current.processState.state in {WaitingForProduct, Fermenting,
   CoolHolding}` mit passendem `priorBootPhaseElapsed`-Tag (bereits
   resumt, Zeit noch offen):** die Neuberechnung wendet 5.23s normative
   Akkumulationsformel (dort einzige Quelle, hier nicht wiederholt) mit
   folgenden Abweichungen und feldweisen Konsequenzen gegenueber ihrer
   regulaeren Hop-1-Anwendung an:
   - **Basis ist nicht** das bereits gefaltete (und deshalb bereits
     `nullopt`) `current.priorBootPhaseElapsed`, sondern
     `pendingRecoveryAnchor->accumulatedBeforeEpisode` (der VOR der
     gesamten, ggf. bereits carry-forwarded Kette eingefrorene "alt"-Wert,
     s. o.) – nur so bleibt ein vor dieser Kette bereits akkumulierter,
     bekannter Stand erreichbar, ohne die Kette selbst doppelt zu zaehlen.
   - **Es wird keine neue Episode addiert**, sondern die gesamte,
     ggf. bereits carry-forwarded Kette mit dem jetzt bekannten `outage`
     (aus Schritt 1, gegen die **wirksame** Zeitbasis berechnet) statt des
     zum Hop-1-Zeitpunkt unbekannten neu gefaltet.
   - `neu.lowerBoundSeconds` wird ebenfalls nach 5.23s Formel neu gefaltet
     (`accumulatedBeforeEpisode.lowerBoundSeconds +
     effectiveKnownSecondsBeforeCheckpoint + outage.outageSecondsLowerBound`)
     – nicht uebersprungen. Solange `maxCheckpointGapSeconds == nullopt`
     ist (heute der einzige implementierte Zustand, 5.13), liefert das
     **exakt denselben** Wert, den der letzte Commit dieser Kette bereits
     enthielt, da `outageSecondsLowerBound` dann strukturell immer `0` ist;
     die Neufaltung ist in diesem Fall ein beobachtbar wirkungsloser, aber
     korrekt hergeleiteter Leerlauf. Wird `maxCheckpointGapSeconds`
     kuenftig als bewiesene Konstante gesetzt (5.13s ausdruecklich offener
     Folgepunkt), liefert dieselbe Faltung dann echte, groessere
     `lowerBoundSeconds`-Werte fuer eine Kette aus **genau einer** Episode
     – fuer eine bereits carry-forwarded Kette (`knownSecondsSinceOriginalCheckpoint
     > 0`) bleibt `outageSecondsLowerBound` gemaess dem oben festgelegten
     Untergrenzen-Vertrag unbedingt `0`, unabhaengig von
     `maxCheckpointGapSeconds` – dieser Schritt selbst muss dafuer nicht
     geaendert werden, die Einschraenkung liegt bereits in der
     Outage-Bounds-Berechnung selbst. Eine **reine**
     `lowerBoundSeconds`-Verbesserung (ohne begleitende
     `upperBoundSeconds`-Aenderung, also ein Wert-zu-Wert- statt
     `nullopt`-zu-Wert-Uebergang) ist ebenfalls ein gueltiger
     Commit-Ausloeser fuer den fuenften `RunPersistenceMutationKind::Recovery`-
     Trigger (5.16, dort entsprechend ergaenzt) – heute unerreichbar, da
     `outageSecondsLowerBound` ohne gesetztes `maxCheckpointGapSeconds`
     nie einen von Hop 1 abweichenden Wert liefert, aber ab produktivem,
     auf eine Ein-Episoden-Kette beschraenktem `maxCheckpointGapSeconds`
     faellig.
   - Nur `neu.upperBoundSeconds` kann sich durch diesen Aufruf ueberhaupt
     aendern (`outage` ist jetzt bekannt, `accumulatedBeforeEpisode` ist
     unveraendert seit dem ersten Hop 1 dieser Kette). Nur wenn dieser
     **neu berechnete** Wert von `nullopt` auf einen Wert wechselt, wird
     `candidate.priorBootPhaseElapsed->elapsed.upperBoundSeconds` auf ihn
     gesetzt und **committet** (`recoveryTimeResolvedAtResume(...)` ist
     danach zwangslaeufig wahr, da `priorBootPhaseElapsed` bereits
     vorhanden ist und die Obergrenze nun bekannt ist -> Anker im selben
     Commit geloescht). War `accumulatedBeforeEpisode.upperBoundSeconds`
     bereits vor dieser Kette `nullopt` (kein UTC-Anker war jemals vor
     Beginn dieser Kette fuer diese Phase bekannt, oder eine fruehere,
     bereits **abgeschlossene** Kette derselben Phase wurde nie aufgeloest,
     bevor die Phase real wechselte, 5.23), bleibt `neu.upperBoundSeconds`
     **unabhaengig vom jetzt bekannten `outage`** bei `nullopt` – dieser
     Fall committet **nichts**, exakt wie ein Aufruf ohne jede
     Verbesserung. Diese Uebereinstimmung des Schreibkriteriums (5.16: nur
     bei echter akkumulierter `nullopt`->Wert-Aenderung) mit der
     tatsaechlichen Akkumulationsformel ist die einzige Bedingung, unter
     der **ueberhaupt** committet wird – ein
     episode-lokal berechenbares `outage` allein reicht dafuer **nicht**,
     wenn die akkumulierte Basis bereits dauerhaft `nullopt` ist. In jedem
     Fall ohne Commit wird `unavailableResult()`/ein expliziter "keine
     Verbesserung"-Status zurueckgegeben.
4. Commit (nur im Erfolgsfall aus Punkt 3, oder bei automatischer
   Tombstone-/Resume-Aufloesung aus Punkt 2) ueber den gemeinsamen
   Commit-Kern (5.16), `mutationKind = Recovery` (fuenfter Ausloeser,
   s. 5.16), `rollbackState` gemaess aktuellem `state_`.

**Tests:**
- `maxCheckpointGapSeconds` gesetzt (5.13-Testaufbau, kein produktiver
  Setter noetig), `outage` erst bei Reevaluation bekannt, akkumulierte
  `lowerBoundSeconds` waechst gegenueber dem zuletzt committeten Stand
  waehrend `upperBoundSeconds` weiterhin `nullopt` bleibt -> Commit ueber
  den fuenften Ausloeser (5.16) feuert allein wegen der Untergrenze;
  identischer Aufbau ohne Wachstum -> kein Commit.
- Hop1-only -> Commit -> UTC wird spaeter im selben Boot verfuegbar:
  `deriveUtcAtRecoveryBootAnchor` liefert die UTC am Hop-1-Zeitpunkt, nicht
  die spaetere Abfrage-UTC.
- Hop1-only -> Reboot -> UTC wird im zweiten Boot verfuegbar:
  `recoveryBootAnchorMonotonicMillis` wurde beim Episode-Refresh auf den
  zweiten Boot neu gesetzt; dieselbe Ableitung bleibt korrekt.
- Hop1-only -> mehrere Reboots -> `pendingRecoveryAnchor` bleibt
  byte-identisch, nur `recoveryBootAnchorMonotonicMillis` aendert sich je
  Boot.
- `utcNow == nullopt` (keine NTP-Synchronisation): `deriveUtcAtRecoveryBootAnchor`
  liefert `nullopt`, keine Sonderbehandlung an der Aufrufstelle noetig.
- Negative `*utcNow`: `deriveUtcAtRecoveryBootAnchor` liefert `nullopt`,
  keine Konvertierung nach `uint64_t`.
- `elapsedSeconds` an der `int64_t`-Obergrenze: `nullopt`, kein Wrap-Around.
- Spaetere UTC-Reevaluation verwendet **nicht** den Hop-1-/Episode-Refresh-
  Commit als urspruenglichen Ausfallanker.
- **`Fermenting`: Reboot ohne NTP -> sichere Recovery/Resume -> NTP
  spaeter -> Bounds/Konfidenz werden neu bewertet und persistiert**
  (Auftragspunkt 1): Hop 1 mit `time.utcUnixSeconds == nullopt`;
  `RecoveredPhaseElapsed.totalSecondsUpperBound == nullopt`; Hop 2 resumt
  dennoch unbedingt (Aktorfreigabe sofort); `pendingRecoveryAnchor` bleibt
  nach dem Resume-Commit **bestehen** (`recoveryTimeResolvedAtResume ==
  false`); anschliessender `reevaluateRecoveryTime`-Aufruf mit jetzt
  gesetztem `time.utcUnixSeconds` faltet eine neue, endliche Obergrenze in
  `priorBootPhaseElapsed`, loescht danach den Anker, `runRevision`
  erhoeht sich um genau einen `RunPersistenceMutationKind::Recovery`-Commit;
  `RecoveryConfidence` (5.5) wechselt fuer diese Episode von `Unknown`/
  `Bounded` auf einen praeziseren Wert.
- Dasselbe fuer zeitbegrenztes `CoolHolding`
  (`CompletionMode::CoolAndHoldForDuration`).
- **Kein erneutes Aktor-Blocking allein wegen offener Zeitbewertung:**
  `decideFermenting`/`decideCoolHolding`/`decideWaitingForProduct` liefern
  fuer einen Snapshot mit **und** ohne gesetzten `pendingRecoveryAnchor`
  identische `TransitionDecision`/`NoTransition`-Ergebnisse (Regressionstest:
  `pendingRecoveryAnchor.has_value()` ist an keiner Stelle dieser
  Entscheidungsfunktionen eine Eingabe).
- `reevaluateRecoveryTime` ohne verbesserte Obergrenze (NTP weiterhin nicht
  verfuegbar): kein Commit, `priorBootPhaseElapsed`/`runRevision`
  unveraendert (Negativtest gegen unnoetigen NVS-Schreibverschleiss).
- **Reboot zwischen Resume und spaeterer NTP-Reevaluation – Carry-Forward
  erhaelt die Aufloesbarkeit (Auftragspunkt 1; der Test ersetzt den zuvor
  widerspruechlichen Ablauf, der den hier behobenen
  Informationsverlust noch als Sollverhalten festschrieb):** `Fermenting`
  resumt ohne NTP (`originalCheckpointUtc` **bekannt**, Anker bleibt offen)
  -> weiterer Reboot **ohne** zwischenzeitliche Aufloesung ->
  `loadAndInitialize()` klassifiziert dies gemaess 5.15 als regulaeren
  `LoadedActiveRun` (nicht Episode-Refresh) -> frischer Hop 1 erkennt
  `loadedRecord.snapshot.pendingRecoveryAnchor.has_value()` und konstruiert
  den Carry-Forward-Fall: `originalCheckpointUtc`/
  `knownPhaseSecondsAtOriginalCheckpoint`/`accumulatedBeforeEpisode`
  byte-identisch uebernommen, `knownSecondsSinceOriginalCheckpoint` um
  Episode 1s Alt-Boot-lokalen Beitrag erhoeht -> Hop 2 resumt erneut
  unbedingt -> der Anker bleibt aus demselben Grund wie zuvor bestehen ->
  NTP wird **danach** (auf diesem oder einem weiteren Boot) verfuegbar ->
  `reevaluateRecoveryTime` faltet ueber `deriveEffectiveAnchorTimeBasis`
  eine endliche, **beide** Ausfaelle (Episode 1 + Episode 2) korrekt
  umfassende Obergrenze und committet sie, loescht danach den Anker –
  Episode 1s urspruengliches `originalCheckpointUtc` war zu keinem
  Zeitpunkt verloren.
- **Drei-Reboot-Kette (Auftragspunkt 3, KISS-Grenztest):** drei
  aufeinanderfolgende Ausfaelle derselben Phase, alle vor jeder
  NTP-Verfuegbarkeit, jeweils mit erfolgreichem Zwischen-Resume; NTP wird
  erst nach dem dritten Reboot verfuegbar -> `reevaluateRecoveryTime`
  loest die **gesamte** Kette auf einen Schlag auf (keine Ober-/Untergrenze
  bleibt wegen einer der drei Episoden dauerhaft unbekannt); dasselbe fuer
  zeitbegrenztes `CoolHolding`.
- **Kein doppeltes Zaehlen ueber eine Drei-Reboot-Kette (expliziter
  Assertion-Test):** bei bekannten Alt-Boot-lokalen Beitraegen `N1`
  (Episode 1), `N2`, `N3` (jeweils carry-forward-akkumuliert) und
  `accumulatedBeforeEpisode.lowerBoundSeconds == 0` gilt nach Aufloesung
  `priorBootPhaseElapsed->elapsed.lowerBoundSeconds == N1 + N2 + N3` exakt
  (nicht mehr, nicht weniger) – deckt sowohl `knownPhaseSecondsAtOriginalCheckpoint`
  (nur `N1`) als auch die kumulative Rolle von
  `knownSecondsSinceOriginalCheckpoint` (`N2+N3`) ab.
- **Sicher bekannte eingeschaltete Zeit zwischen Ausfaellen zaehlt nicht
  als Ausfallzeit:** ist zwischen Episode 1s Resume und Episode 2s Ausfall
  ein weiterer Checkpoint geschrieben worden (`knownSecondsSinceOriginalCheckpoint`-Zuwachs
  `> 0`, z. B. durch ein zwischenzeitliches Benutzerkommando), erscheint
  dieser Anteil in der akkumulierten Untergrenze, **bevor** NTP jemals
  verfuegbar wird (unabhaengig von `outage`); ohne einen solchen
  zwischenzeitlichen Checkpoint ist der Zuwachs `0` (Regelfall, 5.13) und
  die Aufloesbarkeit aus Punkt 1 bleibt trotzdem unveraendert erhalten.
- **Carry-Forward-Untergrenzen-Vertrag:** mit gesetztem
  `maxCheckpointGapSeconds` (5.13-Testaufbau) bleibt
  `outageSecondsLowerBound` fuer eine carry-forwarded Kette
  (`knownSecondsSinceOriginalCheckpoint > 0`) unbedingt `0`, obwohl
  dieselbe Konstellation fuer eine Ein-Episoden-Kette (`== 0`) eine
  `> 0`-Untergrenze liefern wuerde (Negativtest gegen eine Ueberhoehung
  der Untergrenze durch mehrfach angewandte Kontrollpunktabstands-Kompensation).
- **Phasenwechsel vor Zeitaufloesung macht die Zeitfrage obsolet, ohne
  bereits gesicherte Sekunden zu verlieren:** ein echter Phasenwechsel
  (z. B. signalbasierter `Fermenting -> Cooling`-Uebergang) waehrend noch
  offener Zeitbewertung loescht `pendingRecoveryAnchor`/
  `priorBootPhaseElapsed` atomar (5.23); `observedRunSeconds`/bereits
  gefaltete `priorBootPhaseElapsed`-Werte **vor** diesem Wechsel bleiben in
  der zu diesem Zeitpunkt bereits committeten Historie unveraendert
  (Regressionstest: kein rueckwirkender Verlust bereits gesicherter
  Sekunden durch die Anker-Loeschung selbst).
- **Kein UTC-Anker jemals vor Beginn der Kette bekannt:** `originalCheckpointUtc
  == nullopt` bereits beim ersten Hop 1 -> bleibt ueber beliebig viele
  Carry-Forwards `nullopt` (`effectiveCheckpointUtc` bleibt `nullopt`
  unabhaengig von `knownSecondsSinceOriginalCheckpoint`) – Abgrenzung
  gegen den oben behobenen Fall (dort war `originalCheckpointUtc` bekannt
  und wurde faelschlich verworfen).
- `activateFallbackRecoveredRun` konstruiert den Carry-Forward-Fall
  identisch zu `activateLoadedRun` (5.8) – derselbe Test einmal je
  Aktivierungspfad.
- `recoveryTimeResolvedAtResume`: `true` bei fehlendem `priorBootPhaseElapsed`
  insgesamt (Resume nach `Preheating`/`ReachingTarget`/`Cooling`/
  `ManualHolding` – keine Zeitfrage, sofortige Loeschung, Regressionstest
  gegen eine faelschliche Gleichsetzung von "keine Zeitfrage" mit
  "Zeitfrage unbekannt"); `false` bei vorhandenem `priorBootPhaseElapsed`
  mit unbekannter akkumulierter Obergrenze; `true` bei vorhandenem
  `priorBootPhaseElapsed` mit bekannter akkumulierter Obergrenze.

### 5.13 Kein harter maximaler Kontrollpunktabstand ohne belastbare Garantie

Ein maximaler Kontrollpunktabstand `maxCheckpointGapSeconds =
originalCheckpointIntervalMinutes * 60`, begruendet mit der Annahme, ein
externer periodischer Aufrufer fuehre mindestens einmal pro Intervall
`checkpointPeriodic()` aus, waere eine unbelegte Annahme: `grep -rn
"checkpointPeriodic"` ueber das gesamte Repository (2.) zeigt, dass
`checkpointPeriodic()` ausserhalb seiner eigenen Deklaration/Definition und
seines eigenen Testverzeichnisses **keinen** Aufrufer hat. Es existiert
keine produktive Schreibcadence, aus der ein maximaler Abstand zwischen zwei
tatsaechlich persistierten Kontrollpunkten bewiesen werden koennte.

Ein zu klein angesetzter Gap ist dabei **nicht** nur "konservativ": er
vergroessert `outageSecondsLowerBound` und damit `totalSecondsLowerBound`
und kann bei `WaitingForProduct` faelschlich `DefinitelyExpired` erzeugen
und den Lauf tombstonen, obwohl die reale untere Grenze tatsaechlich
kleiner ist – eine geratene, nicht bewiesene Zahl mit sicherheitsrelevanter
Wirkung. Das verletzt "unsichere Situationen werden nicht geraten".

**Vertrag:**

- `maxCheckpointGapSeconds` (5.2) ist `std::optional<std::uint32_t>` und
  wird **nur** gesetzt, wenn eine tatsaechlich beweisbare Scheduling-/
  Persistenzgarantie existiert. Eine solche Garantie existiert im heutigen
  Code nicht – der Wert ist deshalb heute **immer** `nullopt`.
- Fehlt die Garantie, entsteht daraus keine kuenstlich hohe Untergrenze:
  `outageSecondsLowerBound` ist dann `0` (5.2), die einzige beweisbare
  untere Grenze bleibt `knownSecondsBeforeCheckpoint` (Alt-Boot-lokal,
  5.3).
- Ereignisbezogene Speicherung, verspaetete oder ausgefallene Kontrollpunkte
  und Zeitqualitaet fliessen nur so weit ein, wie tatsaechlich vorhandene
  Daten sie beweisen: `RunCheckpointSchedule::due()`
  (`run_checkpoint_schedule.cpp:33-51`) laesst zwar einen periodischen
  Checkpoint erst nach mindestens `intervalMinutes` seit dem zuletzt
  **beliebigen** bestaetigten Checkpoint zu (`confirm()`, jeder
  Triggertyp, `run_persistence_coordinator.cpp:516`) – das ist aber
  ausschliesslich eine **Mindestabstands**-Garantie (kein Checkpoint
  kommt frueher), keine **Maximalabstands**-Garantie (ohne einen
  tatsaechlich aufrufenden periodischen Prozess kann beliebig viel Zeit
  bis zum naechsten Checkpoint vergehen). `RunCheckpointTrigger`
  (`lastCheckpointTrigger`, 5.2) bleibt deshalb reine, unveraenderte
  Anzeigeinformation ohne Einfluss auf einen Grenzwert.
- Keine automatische Tombstone- oder Phasenabschlussentscheidung leitet
  sich aus einer unbewiesenen Untergrenzen-Annahme ab (5.11, 5.4): jede
  automatische `DefinitelyExpired`-Entscheidung stuetzt sich unter diesem
  Vertrag ausschliesslich auf den Alt-Boot-lokal bewiesenen Anteil.
- `originalCheckpointIntervalMinutes` (5.12) bleibt als Soll-Cadence-
  Anzeige und Konfidenz-Eingabe (5.5) erhalten; es dient nicht mehr als
  Ersatz fuer eine nicht vorhandene Gap-Garantie. Eine spaetere, dann
  eigenstaendig zu planende Arbeit kann einen tatsaechlich belegbaren
  maximalen Gap einfuehren (z. B. wenn ein produktiver periodischer
  Aufrufer mit nachweisbarer Cadence komponiert wird) – #18 erfindet
  diesen Bound nicht vorab.

**Konsequenz (verbindlich, s. 5.4/5.5/5.11/5.17/5.20):** `Uncertain` wird
unter diesem Vertrag der ueberwiegende Fall fuer eine tatsaechlich knapp vor
oder nach einer Grenze liegende Wartezeit. Das ist beabsichtigt und wird
durch einen echten, typisierten Benutzerpfad (5.17), einen von
Kontrollpunkt-Zeitpunkten unabhaengigen Sensor-Latch (5.20) und eine
ehrliche Konfidenzanzeige (5.5) aufgefangen – nicht durch eine geratene
Zahl.

**Tests:** gleicher UTC-Upper-Bound, aber `maxCheckpointGapSeconds ==
nullopt` (Standardfall) -> `outageSecondsLowerBound == 0`, kein falsches
`DefinitelyExpired`; `WaitingForProduct` mit tatsaechlich abgelaufener, aber
nur ueber die Ausfallzeit (nicht Alt-Boot-lokal) beweisbarer Wartezeit
bleibt `Uncertain`, kein automatischer Tombstone.

### 5.14 Schema-3-Gueltigkeitsvertrag fuer `RecoveryEvaluation` bei aktivem Run

`validStateFor()` (`run_persistence_contract.cpp:10-42`) erlaubt
`RecoveryEvaluation` fuer `RunCheckpointVariant::ProgramRun`/`ManualRun`
**nicht** (weder in der `ProgramRun`- noch der `ManualRun`-Fallliste).
`makeRunPersistenceSnapshot()` wuerde einen Hop-1-only-Kandidaten (5.17)
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
   - `snapshot.pendingRecoveryAnchor.has_value()` (5.12, in diesem Fall
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
3. **Konverser Fall, neu in dieser Revision (5.12, "Anker-Lebenszyklus bei
   Resume"):** `snapshot.pendingRecoveryAnchor.has_value()` **und**
   `snapshot.processState.state != RecoveryEvaluation` ist ab dieser
   Revision **ebenfalls** ein gueltiger Fall (der fruehere Vertrag liess ihn
   strukturell nicht zu, da der Anker dort immer spaetestens beim Resume
   geloescht wurde) – die "Recovery-Zeitbewertung noch offen"-Situation
   eines bereits aktorfreigegebenen Laufs. Gueltig nur, wenn zusaetzlich
   gilt:
   - `snapshot.processState.state in {WaitingForProduct, Fermenting,
     CoolHolding}` (genau die drei grenztragenden, snapshot-getragenen
     Phasen, 5.4/5.7);
   - `snapshot.recoveryBootAnchorMonotonicMillis.has_value()` (identisch
     zum `RecoveryEvaluation`-Fall – der Anker traegt immer beide Felder
     gemeinsam);
   - `snapshot.priorBootPhaseElapsed.has_value()` **und**
     `priorBootPhaseElapsed->taggedState == snapshot.processState.state`
     (Normalfall aus Punkt 4 unten – die Phase ist bereits regulaer
     getaggt, kein Sonderfall wie beim Hop-1-only-Tag);
   - `!priorBootPhaseElapsed->elapsed.upperBoundSeconds.has_value()`
     (`recoveryTimeResolvedAtResume(...) == false`, 5.12 – ist die
     Obergrenze bereits bekannt, muesste der Anker gemaess 5.12 laengst
     geloescht sein; ein Snapshot mit bekannter Obergrenze **und**
     weiterhin gesetztem Anker ist strukturell inkonsistent und damit
     ungueltig).

   Fehlt eine dieser Bedingungen, ist der Snapshot ungueltig.
4. **Zusaetzliche Invariante fuer `priorBootPhaseElapsed` (5.23):** ist
   `snapshot.priorBootPhaseElapsed.has_value()`, muss **genau eine** der
   folgenden zwei Bedingungen gelten, sonst ist der Snapshot ungueltig:
   - **Normalfall** (`snapshot.processState.state != RecoveryEvaluation`):
     `priorBootPhaseElapsed->taggedState == snapshot.processState.state`.
   - **Hop-1-only-Fall** (`snapshot.processState.state ==
     RecoveryEvaluation`, nur fuer `WaitingForProduct` erreichbar, 5.17):
     `priorBootPhaseElapsed->taggedState ==
     pendingRecoveryAnchor->originalProcessState.state`. `RecoveryEvaluation`
     selbst ist eine transiente Buchhaltungsphase und nie ein gueltiger
     `taggedState`-Wert (5.23 "Setzen" taggt ausschliesslich
     `WaitingForProduct`/`Fermenting`/`CoolHolding`); waehrend eines
     zweiten Ausfalls einer bereits einmal resumten `WaitingForProduct`-
     Phase bleibt der Tag deshalb auf der urspruenglichen Phase aus dem
     Anker stehen, nicht auf `RecoveryEvaluation`.
5. **Keine stille Liberalisierung von Schema 1/2:** ein nach altem Schema
   (vor #18) geschriebener Datensatz konnte `RecoveryEvaluation` fuer einen
   aktiven Run strukturell nie enthalten (die alte `validStateFor()`-Liste
   liess das nicht zu) – die Erweiterung macht eine vorher **immer**
   ungueltige Kombination unter den engen, oben genannten zusaetzlichen
   Bedingungen gueltig; bereits existierende Alt-Daten sind von dieser
   Erweiterung nicht betroffen, da sie diese Kombination nie erzeugen
   konnten. Kein expliziter Schema-Versionscheck an dieser Stelle noetig.
6. **`NoActiveRun` traegt keine Recovery-Diagnosedaten eines beendeten
   Laufs:** ist `snapshot.variant == NoActiveRun`, muessen
   `pendingRecoveryAnchor`, `recoveryBootAnchorMonotonicMillis`,
   `lastRecoveryEpisodeEvidence` (5.20), `priorBootPhaseElapsed` (5.23) und
   `nominalRecoveryAdjustment` (5.22) alle `nullopt` sein, sonst ist der
   Snapshot ungueltig – konsistent mit `clearActiveRunState()` (5.11), das
   alle fuenf Felder im selben Commit loescht.

Contract-/Codec-Tests: aktiver Schema-3-Snapshot mit `RecoveryEvaluation`
und vollstaendigem, konsistentem Pending-Kontext -> gueltig; ohne
Pending-Kontext -> ungueltig; mit inkonsistentem `pendingRecoveryAnchor`
(falsche Phase fuer den Snapshot) -> ungueltig; mit `priorBootPhaseElapsed`,
dessen Tag nicht zur aktuellen Phase passt -> ungueltig; `NoActiveRun` mit
noch gesetztem `pendingRecoveryAnchor`/`lastRecoveryEpisodeEvidence`/
`nominalRecoveryAdjustment` -> ungueltig; Schema-1/2-Decodierung kann diese
Kombinationen gar nicht erzeugen (Regressionstest gegen bestehende
Migrationsvektoren). Zusaetzlich (Punkt 3, neu): Snapshot mit
`processState.state == Fermenting`, gesetztem `PendingRecoveryAnchor`,
regulaer getaggtem `priorBootPhaseElapsed` ohne bekannte Obergrenze ->
gueltig ("Zeitbewertung noch offen"); derselbe Snapshot, aber mit
bekannter Obergrenze -> ungueltig; `processState.state == Cooling` mit
gesetztem Anker -> ungueltig (keine der drei erlaubten Phasen).

### 5.15 Reboot waehrend bereits persistiertem `RecoveryEvaluation` (Episode-Refresh)

`RecoveryReentryRequired`s Topologie erlaubt als Quelle ausschliesslich
`stateUsesRunSnapshot(from)`-Phasen; `RecoveryEvaluation` selbst gehoert
nicht dazu (`stateUsesRunSnapshot(RecoveryEvaluation) == false`). Ein
zweiter Reboot waehrend einer noch offenen Hop-1-only-Recovery (5.17) laedt
aber genau `processState.state == RecoveryEvaluation` – Hop 1 ist auf
dieses geladene Ergebnis nicht anwendbar.

**Vertrag – Ladeklassifikation um einen dritten Fall erweitert:**

```text
loadAndInitialize():
  variant == NoActiveRun                                -> ReadyEmpty (unveraendert)
  variant != NoActiveRun && state != RecoveryEvaluation  -> LoadedActiveRun (Hop-1-faehig, 5.9)
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
`RecoveryEvaluation` ohne Pending-Kontext ist gemaess 5.14 strukturell
ungueltig und waere schon beim Laden abgelehnt worden).

**Episode-Refresh statt Transition:** Da kein Zustandsuebergang stattfindet,
aber die Zeitbasis (UTC-jetzt, ggf. zwischenzeitlich neuere periodische
Checkpoints) sich seit dem letzten Boot geaendert haben kann, fuehrt der
Coordinator in diesem Fall einen eigenen, einfacheren Commit aus:
`candidate = current` (keine Transition), `candidate.recoveryEpisodeRevision
+= 1U`, `candidate.pendingRecoveryAnchor` bleibt **byte-identisch**
unveraendert (5.12), `candidate.recoveryBootAnchorMonotonicMillis =
monotonicMillis` (dieser Boot, 5.12), `candidate.nominalRecoveryAdjustment`
bleibt unveraendert (5.22 – eine bereits bestaetigte nominale Korrektur wird
durch einen Reboot nicht beeinflusst). Geschrieben ueber denselben
Commit-Kern (5.16), mit `rollbackState == LoadedActiveRun` (5.16),
`mutationKind == Recovery` (5.16).

**Latch-Reset pro Sensorrolle (5.20):** Bei jedem Anstieg von
`recoveryEpisodeRevision` – ob durch den initialen Hop 1 oder durch ein
Episode-Refresh – werden die drei `firstAfterRestart`-Latches
(`lastRecoveryEpisodeEvidence.firstAfterRestart.air/product/cooling`) auf
`nullopt` zurueckgesetzt, **bevor** derselbe Commit versucht, sie ueber
`applyLiveRecoveryEvidence` (5.20) aus dem bei dieser Gelegenheit ohnehin
verfuegbaren `CrossRolePlausibilityContext` (Gate A, 5.26) sofort neu zu
befuellen. `beforeOutage` bleibt davon unberuehrt und wird bei jedem
echten Hop 1 (frisch oder carry-forward, 5.12) neu eingefroren, nicht bei
einem Episode-Refresh (5.20).

Dadurch: **jeder** Reboot, der eine Recovery-Bewertung neu beginnt – ob via
echtem Hop 1 oder via Episode-Refresh –, erhoeht `recoveryEpisodeRevision`
genau einmal; ein zwischen zwei Boots eingereichter, jetzt veralteter
Korrektur-/Entscheidungsversuch wird ueber den bestehenden
`expectedRecoveryEpisodeRevision`-Abgleich (5.22) als `StaleState` erkannt.

`state_` bleibt `LoadedActiveRun` bis zum erfolgreichen Episode-Refresh-
Commit und geht danach auf `Ready` ueber (identisches Ergebnis zur
regulaeren Hop-1-only-Situation, 5.17); `RecoveryEvaluation` traegt
weiterhin keine Aktorfreigabe; die Aufloesungswege (automatisch/Benutzer,
5.17) bleiben unveraendert erreichbar, jetzt gegen die aufgefrischte
Episode.

Test: Reboot -> Hop-1-only -> erneuter Reboot -> `recoveryEpisodeRevision`
erhoeht sich erneut, `pendingRecoveryAnchor` und `nominalRecoveryAdjustment`
bleiben identisch, `recoveryBootAnchorMonotonicMillis` wird neu gesetzt,
`firstAfterRestart` wird zurueckgesetzt und ggf. sofort neu befuellt, beide
Aufloesungswege bleiben funktionsfaehig.

### 5.16 Gemeinsamer Commit-Kern – Rollbackzustand und `RunPersistenceMutationKind::Recovery`

`writeSnapshot()` (`run_persistence_coordinator.cpp:287-...`) akzeptiert
heute ausschliesslich `state_ == Ready || state_ == ReadyEmpty` (Zeilen
292-294) und stellt bei jedem Vor-Commit-Fehler exakt diesen aus
`currentHead_.has_value()` neu berechneten Ausgangszustand wieder her.
Wird derselbe Kern auch aus `LoadedActiveRun` bzw. `FallbackRecoveryPending`
(5.18) aufgerufen, waere eine pauschale Rueckkehr zu einem aus
`currentHead_.has_value()` berechneten `Ready`/`ReadyEmpty` sicherheitskritisch
falsch: ein Codec-/Capacity-/NotWritten-Fehler vor sicherem Commit koennte
den Coordinator auf `Ready` setzen, obwohl die Recovery nie erfolgreich
persistiert wurde und `RunCommandState& current` im RAM weiterhin den
unbewerteten Alt-Zustand traegt.

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
    std::optional<RunCheckpointReference> fallbackOverride,  // 5.18
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
noch nicht evaluierten Run) und `FallbackRecoveryPending` (5.18,
ausschliesslich fuer den Fallback-Recovery-Sonderfall).

**Ersetzungsregel innerhalb von `writeSnapshotCore`:** Jede der sieben
Stellen, an denen der heutige Code bei einem Fehler **vor** vollstaendigem
Commit den Coordinator wieder "betriebsbereit" macht, verwendet
**ausschliesslich** `state_ = rollbackState` statt einer Neuberechnung aus
`currentHead_.has_value()`: Snapshot-Encode-Fehler, Envelope-Encode-Fehler
(Ziel), Committed-Head-Encode-Fehler (periodisch), Prepared-/Committed-
Head-Encode-Fehler (nicht-periodisch), periodischer Slot-Schreibfehler
(nicht `Indeterminate`), periodischer Head-Schreibfehler nach erfolgreichem
Slot-Schreiben (nicht `Indeterminate`), Prepared-Head-Schreibfehler
(nicht-periodisch, nicht `Indeterminate`). Fuer den bestehenden Standardpfad
(`rollbackState` == exakt der Zustand, den `currentHead_.has_value()`
ohnehin ergeben haette) ist das Ergebnis **byte-identisch** zum heutigen
Verhalten.

**Unveraendert bleiben, ausdruecklich ohne `rollbackState`-Bezug:**

- Der Lesefehler auf `physicalTarget` vor jedem Schreibversuch
  (Zeilen 403-417) geht bereits heute **unbedingt** in
  `BlockedIndeterminate` – unabhaengig vom Aufrufer korrekt.
- Ein Slot-Schreibfehler nach bereits erfolgreich geschriebenem Prepared-
  Head (nicht-periodisch, Zeilen 485-489) sowie ein Head-Commit-Fehler nach
  bereits erfolgreich geschriebenem Slot (Zeilen 499-503) gehen bereits
  heute **unbedingt** in `BlockedIndeterminate` (bestehender generischer
  `PreparedInterrupted`-Mechanismus, keine recovery-spezifische
  Sonderbehandlung noetig).
- Jeder `RunPersistenceStoreWriteResult::Indeterminate`-Fall geht weiterhin
  unbedingt in `BlockedIndeterminate` (`writeFailure`-Lambda), unabhaengig
  von `rollbackState`.
- Der Schedule-Bestaetigungsfehler nach vollstaendigem Commit (Zeilen
  516-523) geht weiterhin unbedingt in `BlockedIndeterminate`.
- `CounterOverflow` und `TimeWentBackwards`/`InvalidDecision` aus der
  Schedule-`validate()`-Pruefung (Zeilen 295-305) treten **vor** dem Setzen
  von `state_ = Busy` auf; `state_` ist zu diesem Zeitpunkt bereits der
  spaetere `rollbackState`-Wert und braucht keine explizite Wiederherstellung.
- Ein bestaetigt durabler Commit mit anschliessendem RAM-Apply-Fehler auf
  dem echten `RunCommandState& current` fuehrt weiterhin zu
  `PersistenceCommittedApplyFailed` – das setzt **nicht**
  `writeSnapshotCore` selbst, sondern der jeweilige Aufrufer (wie heute bei
  `persistCommand`/`persistTransition`) nach einem `Applied`/
  `CheckpointWritten`-Ergebnis, wenn sein eigener zweiter Anwendungsschritt
  auf `current` fehlschlaegt. `activateLoadedRun`/`activateFallbackRecoveredRun`
  (5.8) folgen demselben Muster.

Alle vier bestehenden oeffentlichen Standardpfade (`persistCommand`,
`persistTransition`, `persistSensorSelection`, `checkpointPeriodic`) rufen
weiterhin ausschliesslich das unveraenderte `writeSnapshot()` mit seinem
bestehenden Guard auf – **kein** Verhaltensunterschied fuer diese Pfade.

**Atomarer `AdjustRun`-Pfad fuer die manuelle Restdauer-Baseline:** Eine
Restdaueranpassung bleibt ein einzelnes `Command`-Commit. Die lokale
Entscheidung darf zwar den vollstaendigen Kandidaten aus
`observedRunSeconds`-Fold, neuer `ActiveRun`-/`ProcessRunSnapshot`-Projektion,
neuem `stateEnteredAtMillis`, `priorBootPhaseElapsed == 0/0`, leerer
`nominalRecoveryAdjustment` und geloeschtem altem Recovery-Anker aufbauen;
`persistCommand` schreibt diesen Kandidaten aber als eine unteilbare
Revision, bevor `current` angewandt wird. Der Kandidat wird nicht aus
mehreren Schreibvorgaengen zusammengesetzt.

Vor-Commit-Fehler (`Write`, `Capacity`, `Stale`, `Apply` vor bestaetigtem
Commit) lassen den alten Restdauer-Timer, die alte `observedRunSeconds` und
den alten Recovery-Kontext unveraendert. Ein `CommitOutcomeUnknown` bleibt
ueber den bestehenden Indeterminate-/Fail-Closed-Vertrag behandelt; es gibt
keine nachtraegliche Teilanwendung oder erneuten Foldversuch in derselben
Entscheidung. Erst ein bestaetigter Commit erlaubt die einmalige RAM-
Anwendung. Ein bestaetigter Commit mit anschliessendem Apply-Fehler bleibt
`PersistenceCommittedApplyFailed` gemaess dem bestehenden Vertrag und wird
nicht in einen behaupteten Rollback umgedeutet.

**`RunPersistenceMutationKind::Recovery` – eigener Mutationstyp fuer
Recovery-Commits:**

Der bestehende `RunPersistenceMutationKind` kennt heute `Command`,
`Transition`, `SensorSelection` (`run_persistence_codec.cpp:901-929`, Werte
1/2/3). Ein Episode-Refresh ist strukturell weder ein `Command` (keine
`CommandId`, kein Benutzerkommando) noch eine gewoehnliche `Transition`
(kein `TransitionDecision`, keine Zustandsaenderung) – dieselbe
Unschaerfe gilt fuer Hop 1, Hop 1+Hop 2 und den Tombstone-Pfad.

```cpp
enum class RunPersistenceMutationKind : std::uint8_t {
    Command = 1U,
    Transition = 2U,
    SensorSelection = 3U,
    Recovery = 4U,  // Hop 1, Hop 1+Hop 2, Episode-Refresh, Tombstone, Zeit-Nachtragskorrektur
};
```

- Codec: `writeMutationKind`/`readMutationKind` erhalten den vierten Wert
  (`4U`); `validPreparedHead` (`run_persistence_codec.cpp:931-939`,
  `(mutationKind == Command) != commandId.has_value()`) bleibt unveraendert
  korrekt, da `Recovery` – wie `Transition` und `SensorSelection` bereits
  heute – **keine** `CommandId` traegt.
- **Fuenf** Ausloeser: die von
  `activateLoadedRun`, `activateFallbackRecoveredRun`, dem
  Episode-Refresh-Pfad (5.15) und dem Tombstone-Pfad (5.11) ausgeloesten
  Commits rufen `writeSnapshotCore` mit `mutationKind =
  RunPersistenceMutationKind::Recovery`, `commandId = std::nullopt` auf –
  **sowie, neu in dieser Revision,** ein erfolgreicher
  `reevaluateRecoveryTime`-Aufruf (5.12), der die akkumulierte
  `priorBootPhaseElapsed`-Obergrenze tatsaechlich von `nullopt` auf einen
  Wert aendert, **oder** – sobald `maxCheckpointGapSeconds` produktiv
  gesetzt ist (5.13, heute nicht der Fall) **und** die betroffene Kette aus
  genau einer Episode besteht (`knownSecondsSinceOriginalCheckpoint == 0`,
  5.12s Untergrenzen-Vertrag fuer carry-forwarded Anker – fuer eine bereits
  carry-forwarded Kette bleibt `outageSecondsLowerBound` unbedingt `0` und
  liefert daher nie eine Verbesserung ueber diesen Zweig) – die
  akkumulierte `lowerBoundSeconds` gegenueber dem zuletzt committeten
  Stand tatsaechlich vergroessert (5. Ausloeser: Zeit-Nachtragskorrektur,
  s. 5.12 Schritt 3).
  Dieser fuenfte Ausloeser committet **ausschliesslich** bei einer dieser
  beiden echten Verbesserungen; ein Aufruf ohne jede Verbesserung (NTP
  weiterhin nicht verfuegbar, und ohne gesetztes `maxCheckpointGapSeconds`)
  schreibt **nichts** – ohne diese Einschraenkung koennte ein wiederholt
  erfolgloser Reevaluationsversuch unbegrenzt NVS-Schreibverschleiss
  erzeugen, ohne dass ein produktiver, ratenbegrenzender Aufrufer existiert
  (Gate B – kein solcher Aufrufer wird in #18 komponiert).
- Beide Benutzerpfade bleiben `Command`-klassifiziert mit echter
  `CommandId` – keine falsche Metadatenklassifikation fuer sie:
  `ApplyRecoveryTimeCorrection` (5.22, reine Datenmutation) laeuft ueber
  die bestehende `persistCommand`-Infrastruktur; `resolveRecoveryOutcome`
  (5.17, kann eine `TransitionDecision` erzeugen, was `persistCommand`
  nicht ausdruecken kann) ist eine eigene Coordinator-Methode mit
  denselben Envelope-/Stale-/Dedup-Halbfunktionen, committet aber ueber
  das normale `writeSnapshot()` ebenfalls mit `mutationKind = Command`.
- Normale Prozessuebergaenge (`persistTransition`) bleiben `Transition`.
- Kompatibilitaet: ein vor #18 geschriebener Schema-1/2-Datensatz kann den
  Wert `4U` fuer `mutationKind` nie enthalten (das Feld existiert dort nur
  im Prepared-Head-Wireformat mit den historisch gueltigen Werten 1-3); ein
  hypothetischer, aus anderem Grund korrupt gesetzter Wert `4U` in einem
  Schema-1/2-Kontext wuerde ohnehin bereits vor Erreichen der
  `mutationKind`-Decodierung durch die Schema-Gate-Pruefung
  (`knownRunPersistenceSchema`, vor jeder Payload-Decodierung) abgefangen
  – `readMutationKind`s eigener `default: return false`-Zweig ist daher
  nicht die einzige Verteidigungslinie, sondern eine zusaetzliche.

`resolveRecoveryOutcome` (5.17, phasenabhaengige Vorbedingung – fuer
`WaitingForProduct` `current.processState.state == RecoveryEvaluation`,
fuer `Fermenting`/`CoolHolding` bereits resumt, s. dort; in jedem Fall
bereits `state_ == Ready`) verwendet das **normale**, guard-behaftete
`writeSnapshot()` mit `mutationKind = Command` (echtes Benutzerkommando) –
der Bypass wird nicht benoetigt, da `state_` zu diesem Zeitpunkt bereits
`Ready` ist.

**Tests fuer jeden relevanten Cutpoint**, je aus `LoadedActiveRun`,
`FallbackRecoveryPending` und regulaerem `Ready`: Codec-/Capacity-/
NotWritten-Fehler vor Commit stellen exakt `rollbackState` wieder her; ein
`Indeterminate`-Ausgang fuehrt unabhaengig vom Aufrufer zu
`BlockedIndeterminate`; ein bestaetigt durabler Commit mit RAM-Apply-Fehler
fuehrt zu `PersistenceCommittedApplyFailed`; Standardpfade zeigen exakt das
heutige Verhalten (Regressionstest). Zusaetzlich: `RunPersistenceMutationKind::Recovery`
Codec-Roundtrip; alle **fuenf** Recovery-Commit-Ausloeser (Hop 1, Hop
1+Hop 2, Episode-Refresh, Tombstone, erfolgreiche Zeit-Nachtragskorrektur
via `reevaluateRecoveryTime`) erzeugen einen Prepared-Head mit
`mutationKind == Recovery` und `commandId == std::nullopt`; ein
`reevaluateRecoveryTime`-Aufruf ohne Verbesserung erzeugt **keinen**
Prepared-Head (Negativtest); ein Cut nach dem Prepared-Head-Schreiben
eines Recovery-Commits durchlaeuft denselben bestehenden generischen
`PreparedInterrupted`-Mechanismus wie jeder andere Mutationstyp.

### 5.17 Unsichere Recovery – typisierter Benutzervertrag fuer alle drei grenztragenden Phasen

**Resume ist nicht dasselbe wie Abschluss – nur `WaitingForProduct` blockiert
bei Unsicherheit den Hop 2:** `evaluateRecoveryTimeVerdict == Uncertain`
bedeutet fuer `Fermenting` und `CoolHolding` mit
`CompletionMode::CoolAndHoldForDuration` **nicht**, dass nur Hop 1
committet wird. Fuer beide Phasen erfolgt Hop 2/Resume unveraendert nach
5.10 (`totalSecondsLowerBound` als `priorElapsed`, Aktorfreigabe wie bei
jedem anderen Resume) – ein Fermentationslauf oder ein Kuehl-Halten wird
nach einem Stromausfall wieder aktiviert und laeuft normal weiter,
unabhaengig vom Verdikt. `Uncertain` betrifft fuer diese beiden Phasen
ausschliesslich die **Abschlussentscheidung**: die normale automatische
Dauerpruefung (`decideFermenting`/`decideCoolHolding`) schliesst den Lauf
noch nicht ab, solange die bewiesene Zeit die Grenze nicht erreicht. Nur
`WaitingForProduct` blockiert Hop 2 bei `DefinitelyExpired`/`Uncertain`
automatisch (5.10, eingebauter Check in `decideRecoveryEvent`,
unveraendert) – dort bedeutet "Weiterwarten" anders als bei den beiden
anderen Phasen eine fortgesetzte Ambiguitaet ueber den Lauf selbst, nicht
nur ueber dessen Abschlusszeitpunkt. Neu in dieser Revision: fuer
`WaitingForProduct` unter `Uncertain` kann der Benutzer diese Ambiguitaet
ueber `resolveRecoveryOutcome` + `AssumeStillValid` explizit aufloesen und
damit einen Resume erzwingen, den der automatische Pfad mangels Beweis
nicht erzwingen darf (s. u.).

`docs/RECOVERY_AND_INTERRUPTION.md` verlangt kanonisch: ueberschneidet das
Zeitintervall einen moeglichen Abschluss oder eine Haltezeitgrenze, erfolgt
**kein automatischer Abschluss aus der unbekannten Zeit**, und der Benutzer
kann Fortschritt bzw. Abschluss innerhalb der erlaubten Grenzen
bestaetigen/anpassen. Da `Uncertain` unter 5.13 der Regelfall ist, muss
dieser Benutzerpfad fuer **alle drei** grenztragenden Phasen tatsaechlich
existieren.

**`resolveRecoveryOutcome` – eigene Coordinator-Methode, kein
`persistCommand`:** Eine Abschlussentscheidung ist eine
`TransitionDecision` (Zustandswechsel), keine reine `CommandDecision`
(Datenmutation) – `persistCommand`/`applyRunCommand` koennen keinen
Zustandswechsel ausdruecken (dasselbe Formproblem, das 5.11 fuer den
Tombstone bereits durch eine direkte `propose()`-Konstruktion loest, statt
eine nicht vorhandene wiederverwendbare Funktion anzunehmen). Der
Benutzerpfad laeuft deshalb ueber eine eigene, schmale Coordinator-Methode:

```cpp
struct ResolveRecoveryUncertaintyRequest {
    CommandId commandId;
    std::uint32_t expectedRunRevision;
    std::uint32_t expectedRecoveryEpisodeRevision;
    RecoveryUncertaintyDecision decision;  // AssumeStillValid | AssumeThresholdCrossed
};
[[nodiscard]] RunPersistenceResult RunPersistenceCoordinator::resolveRecoveryOutcome(
    RunCommandState& current, const ResolveRecoveryUncertaintyRequest& request,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext& liveSensorEvidence);
```

**Neuer Parameter `liveSensorEvidence` (Gate-A-Kopplung, Auftragspunkt 2,
"implied but not stated"):** ausschliesslich fuer den einen Zweig
verwendet, der tatsaechlich einen Resume durchfuehrt
(`WaitingForProduct` + `AssumeStillValid`, s. u.) – jeder Resume in eine
snapshot-getragene, sensorabhaengige Phase durchlaeuft dieselbe
Restart-Sensorauswahl (5.26) wie Hop 2, unabhaengig davon, ob er
automatisch (Hop 1+Hop 2, sofort) oder verzoegert ueber einen spaeteren
Benutzerentscheid erfolgt – ein Resume ohne erneute Sensorpruefung waere
eine ueber den Reboot hinaus vorausgesetzte Aktorfreigabe (AGENTS.md,
unzulaessig). Fuer alle anderen Zweige (Tombstone, `AssumeStillValid` fuer
`WaitingForProduct` mit `NotAllowedInState`-Ablehnung, jede
`AssumeThresholdCrossed`-Abschlussentscheidung fuer `Fermenting`/
`CoolHolding`) bleibt der Parameter ungenutzt, da dort kein neuer
Resume/keine neue Aktorfreigabe stattfindet – ein bereits laufender,
laengst freigegebener Prozess wird nur beendet, nicht neu aktiviert.

**Vorbedingung – zwei unterschiedliche Datenquellen, weil Hop 2 fuer
`Fermenting`/`CoolHolding` bereits abgeschlossen ist, bevor dieser Pfad
ueberhaupt aufgerufen werden kann (5.12):** Ein Resume fuer diese beiden
Phasen ist bereits erfolgt, sobald ein aktiver Run ueberhaupt wieder in
`Ready` beobachtbar ist – `current.processState.state` ist zu diesem
Zeitpunkt bereits `Fermenting`/`CoolHolding`, nicht mehr
`RecoveryEvaluation`. **Praezisierung fuer diesen Vertrag
(Auftragspunkt 2):** `pendingRecoveryAnchor`
ist zu diesem Zeitpunkt **nicht** notwendigerweise bereits `nullopt` – er
kann gemaess 5.12s bedingter Loeschregel weiterhin bestehen, solange die
Recovery-Zeitbewertung noch offen ist ("Zeitbewertung noch offen", 5.12).
Das ist fuer die Vorbedingung dieses Pfades unerheblich: sie liest
`pendingRecoveryAnchor` an keiner Stelle (s. u.), sondern ausschliesslich
`current.processState.state` und `current.priorBootPhaseElapsed`. Die
Vorbedingung ist deshalb **phasenabhaengig**, nicht einheitlich:

- **`WaitingForProduct`:** `state_ == Ready && current.processState.state
  == RecoveryEvaluation && current.pendingRecoveryAnchor.has_value()`
  (sonst `unavailableResult()`/`NotAllowedInState`) – dieser Pfad ist nur
  erreichbar, solange Hop 2 noch nicht erfolgreich war (Hop-1-only, 5.17
  oben).
- **`Fermenting`/`CoolHolding`:** `state_ == Ready &&
  (current.processState.state == ProcessState::Fermenting ||
  current.processState.state == ProcessState::CoolHolding) &&
  current.priorBootPhaseElapsed.has_value() &&
  current.priorBootPhaseElapsed->taggedState ==
  current.processState.state` (sonst `unavailableResult()`/
  `NotAllowedInState`) – der Lauf ist zu diesem Zeitpunkt bereits resumt;
  `pendingRecoveryAnchor` wird fuer diese beiden Phasen an keiner Stelle
  dieses Pfades gelesen.

Stale-Pruefung fuer beide Faelle identisch: `request.expectedRunRevision
== current.runRevision && request.expectedRecoveryEpisodeRevision ==
current.recoveryEpisodeRevision` (sonst `StaleState`) – dieser Zaehler
bleibt ueber Hop 2 hinweg gueltig, da er unabhaengig vom Anker gefuehrt
wird (5.14). Dedup ueber `persistedIds_`/`request.commandId` (dieselbe
Technik wie `persistCommand`, sonst `AlreadyPersisted`/
`AlreadyProcessed`). Anschliessend Neuberechnung des Verdikts (5.4) – fuer
`WaitingForProduct` aus 5.2-5.3 gegen `deriveEffectiveAnchorTimeBasis(pendingRecoveryAnchor)`
(5.12 – **nicht** gegen die rohen Anker-Felder direkt, damit ein bereits
carry-forwarded `knownSecondsSinceOriginalCheckpoint` korrekt einfliesst,
z. B. wenn diese Phase nach einem frueheren `AssumeStillValid`-Resume
einen zweiten Ausfall erlebt hat), fuer
`Fermenting`/`CoolHolding` durch direktes Anwenden von
`evaluateRecoveryTimeVerdict`s Vergleichslogik auf
`current.priorBootPhaseElapsed->elapsed.lowerBoundSeconds`/
`.upperBoundSeconds` (die bereits ueber alle bisherigen Episoden dieser
Phase akkumulierten, dauerhaften Grenzen, 5.23) gegen die jeweilige
Phasengrenze, **ohne** frische UTC-Neuberechnung ueber den (bereits
geloeschten) Anker – nur bei `Uncertain` wird fortgefahren, sonst
`NotAllowedInState`. Verzweigung nach Phase und `decision`:

- **`WaitingForProduct` + `AssumeThresholdCrossed`:** identisch zu 5.11 –
  `propose(candidate.processState, Standby, RecoveryEndedByExpiredWait,
  monotonicMillis)`, `clearActiveRunState(candidate)`.
- **`WaitingForProduct` + `AssumeStillValid` – echter Resume
  (Auftragspunkt 2):** Diese Revision fuehrt hier denselben Resume aus,
  den der automatische Pfad bei `DefinitelyStillValid` ausfuehren wuerde,
  jetzt durch die manuelle Attestierung des Benutzers anstelle des dortigen
  UTC-Beweises getragen:
  1. Gate A (5.26) wird gegen `liveSensorEvidence` ausgewertet; negatives
     Ergebnis -> `request.event = ProcessEvent::RecoveryReject`
     (identisch zu Hop 2s Gate-A-Kopplung, 5.10) statt eines Resume –
     `propose(candidate.processState, Fault, RecoveryRejected,
     monotonicMillis)`.
  2. Bei positivem Gate A: `request.event = ProcessEvent::RecoveryResume`,
     `request.recoveredState` aus `pendingRecoveryAnchor.originalProcessState`
     aufgebaut (identisch zu Hop 2, 5.10), `priorElapsedForOldPhase =
     RecoveredPhaseElapsed.totalSecondsLowerBound` (5.10s neue,
     benutzerpfadspezifische Zeile – **nicht** `totalSecondsUpperBound`,
     Begruendung dort). Aufruf ueber dieselbe `decideProcessTransition`-API
     wie Hop 2.
  3. Das Ergebnis wird ueber `applyProcessTransition` angewandt;
     `priorBootPhaseElapsed` (getaggt `WaitingForProduct`) wird gemaess
     5.23 "Setzen" gesetzt/akkumuliert (dort bereits als "Hop 1 oder Hop 2
     mit Resume" gefasst – dieser Resume ist fachlich ein Hop-2-Resume,
     nur zeitlich entkoppelt von Hop 1 ausgeloest).
  4. Anker-Loeschung/-Erhalt folgt der einheitlichen Regel aus 5.12
     (`recoveryTimeResolvedAtResume`) – **nicht** mehr "bleibt unveraendert
     bestehen" wie im frueheren Pfad, da nun tatsaechlich ein Resume
     stattfindet, der dieselbe Bedingung wie jeder andere Resume
     durchlaeuft.
- **`Fermenting` + `AssumeThresholdCrossed` – nur innerhalb belastbarer
  Bounds (Bounds-Gate, Auftragspunkt 3):** zusaetzlich zur
  `Uncertain`-Vorbedingung wird verlangt, dass die **akkumulierte**
  Obergrenze bekannt ist: `current.priorBootPhaseElapsed->elapsed.upperBoundSeconds.has_value()`
  – sonst `NotAllowedInState` (dieselbe Ablehnung wie ausserhalb
  `Uncertain`, s. u.). **Beweis, dass diese Bedingung bereits die
  eigentlich verlangte Intervallpruefung einschliesst:** `Uncertain`
  bedeutet (5.4) `lowerBound < limit` **und** `(upperBound == nullopt ODER
  upperBound >= limit)`. Ist zusaetzlich `upperBound.has_value()`
  bewiesen, folgt daraus zwingend `upperBound >= limit` (da der
  `upperBound == nullopt`-Zweig durch die Praesenzpruefung bereits
  ausgeschlossen ist) – zusammen mit `lowerBound < limit` ergibt das exakt
  `lowerBound < limit <= upperBound`, also `limit in (lowerBound,
  upperBound]`: die Abschlussgrenze liegt bewiesenermassen **innerhalb**
  des ausgewiesenen Intervalls. Eine zusaetzliche, separate
  Intervall-Vergleichspruefung ist damit **redundant** und wird bewusst
  nicht zusaetzlich kodiert (KISS) – die Praesenzpruefung allein ist
  aequivalent. Ist `upperBound == nullopt` (NTP nie verfuegbar geworden
  oder eine fruehere Episode dieser Phase unaufloesbar verloren, 5.12):
  **keine** qualitative Abschlussbestaetigung; die sichere Regelung laeuft
  unveraendert weiter (Punkt 1, "Zwei getrennte Fragen"), der Abschluss
  wartet auf eine bessere Zeitinformation (`reevaluateRecoveryTime`) oder
  einen spaeter, eigenstaendig zu planenden manuellen Override-Vertrag
  (ausdruecklich **nicht** Teil von #18). Bei erfuellter Bedingung: ruft
  die bereits bestehende, eigenstaendige Funktion
  `completeTimedRun(candidate.processState, *candidate.processRunSnapshot,
  monotonicMillis)` (`process_state_machine.cpp:469`, von
  `decideFermenting` fuer den Nicht-Recovery-Fall bereits verwendet)
  direkt auf, statt auf `elapsedWithPrior` zu warten, und wendet das
  Ergebnis ueber `applyProcessTransition` an. `completeTimedRun` selbst
  entscheidet anhand von `completionMode`, ob das Ziel `Cooling` oder
  `Completed` ist (`FermentationCompleted`, 5.7-Tabelle) –
  `resolveRecoveryOutcome` erzwingt kein festes Zielergebnis, sondern
  uebernimmt unveraendert, was die bereits bestehende Funktion fuer den
  konkreten Snapshot entscheidet. Ist zu diesem Zeitpunkt noch ein
  lingernder `pendingRecoveryAnchor` vorhanden (5.12,
  "Zeitbewertung noch offen"), wird er **hier** geloescht (Abschluss macht
  die Zeitfrage fachlich obsolet, s. 5.12 Punkt (c)) – anders als in
  dem frueheren Pfad, wo diese Felder an dieser Stelle bereits unbedingt `nullopt`
  waren.
- **`CoolHolding` + `AssumeThresholdCrossed`:** dasselbe Bounds-Gate wie
  fuer `Fermenting` (`priorBootPhaseElapsed->elapsed.upperBoundSeconds.has_value()`,
  identischer Beweis); ruft bei erfuellter Bedingung eine neu extrahierte,
  ebenso eigenstaendige Funktion `completeHoldDuration(const
  ProcessRuntimeState&, std::uint64_t monotonicMillis) ->
  TransitionDecision` auf (`propose(current, Completed,
  HoldDurationCompleted, monotonicMillis)` + `RunCompleted`-Nachricht –
  exakt die bisher in `decideCoolHolding` inline konstruierte Entscheidung,
  jetzt als eigene Funktion fuer beide Aufrufer nutzbar, DRY analog zu
  `completeTimedRun`), wendet das Ergebnis ueber `applyProcessTransition`
  an, loescht einen lingernden Anker analog zu `Fermenting` oben.
  `decideCoolHolding` selbst ruft diese Funktion nach erfolgreicher
  Dauerpruefung ebenfalls auf statt die Konstruktion weiterhin zu
  duplizieren.
- **`Fermenting`/`CoolHolding` + `AssumeStillValid`:** **nicht angeboten**
  – ohne Zustandswechsel und ohne Wirkung auf eine bereits laufende,
  aktorfreigegebene Phase waere diese Aktion ein reiner, wirkungsloser
  `CommandId`-Verbrauch (derselbe Erreichbarkeitsmassstab wie 5.5/5.25:
  keine Aktion ohne tatsaechliche Wirkung). Der Lauf faehrt ohnehin normal
  fort, solange keine explizite Abschlussentscheidung getroffen wird; fuer
  `Fermenting` steht zusaetzlich die **quantitative**
  `ApplyRecoveryTimeCorrection` (5.22) zur Verfuegung, semantisch getrennt
  von dieser qualitativen Abschlussentscheidung (5.22 verwendet
  dieselbe, gemeinsame Bounds-Grundlage, s. dort).
- **Ausserhalb `Uncertain`, oder `Uncertain` ohne bekannte akkumulierte
  Obergrenze (Fermenting/CoolHolding + AssumeThresholdCrossed):**
  Ablehnung (`NotAllowedInState`) fuer jede Kombination – der Benutzer kann
  weder eine Entscheidung erzwingen, die dem berechneten Verdikt
  widerspricht, noch eine, fuer die keine belastbare Obergrenze
  nachgewiesen ist.

Commit ueber das normale, guard-behaftete `writeSnapshot()` (5.16,
`mutationKind = Command`, echte `commandId` aus `request.commandId`) – der
Bypass aus 5.16 wird nicht benoetigt, da `state_` bereits `Ready` ist.
`pendingRecoveryAnchor`/`recoveryBootAnchorMonotonicMillis` werden **nicht
pauschal phasenabhaengig** behandelt, sondern folgen einheitlich der Regel
aus 5.12:

- **`WaitingForProduct` + `AssumeThresholdCrossed`:** immer auf `nullopt`
  gesetzt (Tombstone macht die Zeitfrage obsolet, ueber
  `clearActiveRunState`, 5.11, nach der Latch-Sequenz aus 5.20) –
  unabhaengig vom `recoveryTimeResolvedAtResume`-Stand.
- **`WaitingForProduct` + `AssumeStillValid` (echter Resume, s. o.):**
  `recoveryTimeResolvedAtResume`-Regel aus 5.12 (auf `nullopt`, wenn die
  akkumulierte Obergrenze bereits bekannt ist; sonst bleibt der Anker
  bestehen).
- **`Fermenting`/`CoolHolding` + `AssumeThresholdCrossed`:** an dieser
  Stelle explizit geloescht, falls noch vorhanden (Abschluss macht die
  Zeitfrage obsolet, s. o.) – unabhaengig vom
  `recoveryTimeResolvedAtResume`-Stand.
- **`Fermenting`/`CoolHolding` sind hier sonst nicht erreichbar** (kein
  weiterer Zweig veraendert den Anker fuer diese beiden Phasen, da
  `AssumeStillValid` fuer sie nicht angeboten wird).

**Getrennte Frage, dieselbe Grundlage – Verdikt vs. Dauerentscheidung
(verbindliche Klarstellung):** `RecoveryTimeVerdict` (5.4) beantwortet
ausschliesslich "ist genuegend Zeit bewiesen?", basierend auf
`RecoveredPhaseElapsed` allein (Alt-Boot-lokal + Ausfallintervall, ohne
jede nominale Korrektur). Die tatsaechliche `Fermenting`-Dauerentscheidung
(`elapsedWithPrior`, 5.22) verwendet dagegen `priorBootPhaseElapsed.lowerBoundSeconds
+ nominalRecoveryAdjustment.cumulativeAppliedSeconds` – bewiesene Zeit
**plus** bereits bestaetigte nominale Korrektur. Diese beiden Werte duerfen
legitim auseinanderfallen: das Verdikt bleibt so lange `Uncertain` (und
erlaubt damit weitere Korrekturen innerhalb der in 5.22 festgelegten
akkumulierten Obergrenze), bis entweder eine schaerfere UTC-Grenze oder der
`Completed`-Uebergang selbst die Frage beendet. Keine der beiden Groessen
ersetzt die andere; sie beantworten unterschiedliche Fragen (bewiesen? vs.
fuer die Dauerentscheidung als vergangen zu behandeln?).

Kein UI-Screen ist Teil von #18; `resolveRecoveryOutcome` sowie
vollstaendige native Tests sind es.

**Zwei gleichwertige Aufloesungswege bleiben bestehen, beide ueber
denselben Commit-Kern – jetzt fuer alle drei Phasen erreichbar, nicht nur
fuer `WaitingForProduct` (Erweiterung durch 5.12s
`reevaluateRecoveryTime`):**

1. **Automatisch:** `RunRecoveryCoordinator::reevaluateRecoveryTime(RunCommandState&,
   const RunCheckpointTime&)` (5.12, nativ testbar; produktive Verdrahtung
   eines Aufrufers ist nicht Teil von #18, Gate B). Fuer `WaitingForProduct`
   im Hop-1-only-Zustand identisch zum bestehenden Vertrag: ergibt sich aus Unter-
   **und** Obergrenze dieselbe fachliche Entscheidung
   (`DefinitelyExpired`/`DefinitelyStillValid`), wird automatisch
   Tombstone/Resume ausgeloest. Fuer alle drei Phasen **nach** bereits
   erfolgtem Resume mit weiterhin offenem Zeitkontext (5.12, neu):
   aktualisiert bei Verfuegbarkeit einer bislang unbekannten Obergrenze
   `priorBootPhaseElapsed` und loescht danach ggf. den Anker – niemals
   eine Zustandstransition. Schreibt in beiden Faellen **niemals**
   `RunCommandState.runProgress.observedRunSeconds` und **niemals**
   `NominalRecoveryAdjustmentState` (5.22, Gate C) – eine praezisere
   UTC-Grenze beweist Zeit, aber keine biologische Aktivitaet, und ist kein
   Ersatz fuer eine Benutzerbestaetigung.
2. **Benutzerpfad:** siehe oben, `resolveRecoveryOutcome` – fuer
   `WaitingForProduct` sowohl qualitativ (`AssumeStillValid`: Resume,
   `AssumeThresholdCrossed`: Tombstone) als auch fuer `Fermenting`
   zusaetzlich quantitativ (`ApplyRecoveryTimeCorrection`, 5.22).

Anker-Loeschung/-Erhalt nach jeder dieser Aufloesungen folgt einheitlich
5.12 (`recoveryTimeResolvedAtResume`) bzw. der expliziten
Obsoleszenz-Loeschung bei Tombstone/Abschluss (s. o.) – kein weiterer,
hier zusaetzlich zu beschreibender Sonderfall.

**Tests:** `AssumeStillValid` fuer `WaitingForProduct` innerhalb
`Uncertain` -> echter Resume nach `WaitingForProduct` (Gate A positiv),
`priorBootPhaseElapsed` mit Tag `WaitingForProduct` gesetzt,
`priorElapsedForOldPhase == totalSecondsLowerBound`; Gate A negativ ->
`RecoveryRejected`/`Fault` statt Resume; Anker bleibt nach diesem Resume
bestehen, solange die akkumulierte Obergrenze `nullopt` ist, sonst
geloescht; Wiederholung derselben `commandId` idempotent
(`AlreadyPersisted`/`AlreadyProcessed`); Stale-Episode ->
`StaleState`; kein automatischer Uebergang zu `ReachingTarget`/
`Fermenting` (Resume landet exakt in `WaitingForProduct`, wartet weiter
auf `ProductInserted`). `AssumeThresholdCrossed` fuer `Fermenting`
innerhalb `Uncertain` **mit** bekannter akkumulierter Obergrenze ->
`completeTimedRun`-Abschluss, identisch zum automatischen Pfad, lingernder
Anker wird dabei geloescht; **mit unbekannter** Obergrenze (`Uncertain`,
`upperBoundSeconds == nullopt`) -> `NotAllowedInState`, sichere Regelung
laeuft unveraendert weiter (Negativtest fuer das Bounds-Gate); ausserhalb
`Uncertain` (z. B. `DefinitelyStillValid`) -> `NotAllowedInState`;
`AssumeThresholdCrossed` fuer `CoolHolding` -> `completeHoldDuration`/
`HoldDurationCompleted`, identisches Bounds-Gate; `decideCoolHolding` und
`resolveRecoveryOutcome` erzeugen fuer denselben Ausgangszustand
identische Transitionsdaten (Regressionstest gegen die Extraktion);
`AssumeStillValid` fuer `Fermenting`/`CoolHolding` ist kein anbietbarer
Wert (Compile-/API-Test); Hop 2/Resume erfolgt fuer `Fermenting`/
`CoolHolding` unveraendert bei `Uncertain` (Aktorfreigabe, kein
Hop-1-only fuer diese beiden Phasen); Stale-/Episode-/CommandId-Schutz
identisch zum bestehenden
`WaitingForProduct`-Pfad; Write-before-Apply.

### 5.18 `FallbackRecovered`/`FallbackRecoveryPending` – kein Dead-End, echter gueltiger Fallback nach Commit

`loadAndInitialize()` setzt beim erfolgreichen Laden des Fallback-
Datensatzes (`run_persistence_coordinator.cpp:264-274`, dort heute
`enterBlockedIndeterminate()`) stattdessen den eigenstaendigen Zustand
`RunPersistenceCoordinatorState::FallbackRecoveryPending`:

```cpp
enum class RunPersistenceCoordinatorState : std::uint8_t {
    Uninitialized, ReadyEmpty, LoadedActiveRun, Ready, Busy,
    BlockedIndeterminate, FallbackRecoveryPending,
    PersistenceCommittedApplyFailed,
};
```

Das ist eine **beobachtbare Vertragsaenderung**: der bestehende Test
`test_run_persistence_coordinator.cpp:1454-1456` erwartet nach einem
`FallbackRecovered`-Load `state() == BlockedIndeterminate`; er wird auf
`state() == FallbackRecoveryPending` aktualisiert. `RunPersistenceCoordinatorState`
ist reine Laufzeit-/RAM-Kategorie ohne Wire-Format-Bezug.

`unavailableResult()` erhaelt einen Zweig fuer `FallbackRecoveryPending`,
der wie `LoadedActiveRun` `RunPersistenceResultStatus::RecoveryPending`
liefert. Alle anderen, echten `BlockedIndeterminate`-Faelle (Store-Ausgang
unbestimmt, nicht rekonstruierbar, fremde Epoche, unbekanntes Schema,
`PreparedInterrupted`) bleiben strikt in `BlockedIndeterminate` und sind
fuer `activateLoadedRun`/`activateFallbackRecoveredRun` (5.8) **nicht**
zulaessig (Vorbedingung exakt `LoadedActiveRun || FallbackRecoveryPending`)
– kein Recovery-Write aus einem beliebigen `BlockedIndeterminate`.

**Kein Dead-End:** Hop 1 wird **immer** versucht, unabhaengig davon, ob die
Quelle `Current` oder `FallbackRecovered` war, und immer atomar committet
(`mutationKind = Recovery`, 5.16), sobald er lokal erfolgreich aufgebaut
werden konnte. Fuer den `FallbackRecoveryPending`-Fall (Ankerkonstruktion
aus `slots_[currentHead_->fallback->slot]`, 5.8) gilt zusaetzlich:

- **Zielslot:** `targetSlotOverride = currentHead_->current.slot` (der
  bekannt defekte Slot – der gueltige Fallback-Slot bleibt bis zum Commit
  physisch unangetastet).
- **Fallback-Referenz nach Commit:** `fallbackOverride =
  currentHead_->fallback` (die zum Ladezeitpunkt erfolgreich gelesene,
  weiterhin gueltige Fallback-Referenz) wird an `writeSnapshotCore`
  durchgereicht und dort fuer `committed.fallback` verwendet **statt** der
  bestehenden Standardregel `committed.fallback = currentHead_->current`
  (die im Fallback-Fall exakt die bekannt defekte Referenz waere).
  `currentHead_->fallback` ist zwischen Laden und diesem – dem einzigen aus
  `FallbackRecoveryPending` moeglichen – Commit unveraendert, da
  `FallbackRecoveryPending` ausser diesem einen Commit keinerlei
  Schreibzugriff zulaesst.
- **Fail-closed-Guard:** vor dem Commit wird geprueft
  `targetSlotOverride != fallbackOverride->slot`; sind beide Slots
  identisch (nur bei bereits inkonsistenten Head-Metadaten moeglich),
  wird der Commit abgelehnt (`InvalidDecision`, kein Schreibversuch). Der
  Codec selbst erzwingt dieselbe Trennung zusaetzlich strukturell fuer
  jeden erfolgreich committeten Head (`validCommittedHead`,
  `run_persistence_codec.cpp:970-972`: `head.fallback->slot !=
  head.current.slot`) – der explizite Guard liefert dabei den spezifischeren
  Fehler `InvalidDecision` statt eines generischen Codec-Fehlers.
- **Ergebnis nach vollstaendigem Commit:** `committed.current` = neuer
  Recovery-Snapshot im ehemals defekten Slot; `committed.fallback` = die
  unveraendert gueltige alte Fallback-Referenz.

**Cutpoint-Verhalten (nicht-periodischer Commit, Prepared-Head -> Slot ->
Committed-Head):**

- Cut **vor** dem Prepared-Head-Schreiben: `state_` kehrt exakt zu
  `FallbackRecoveryPending` zurueck (5.16, `rollbackState`); Current bleibt
  defekt, Fallback bleibt unveraendert gueltig und ladbar; ein erneuter
  Hop-1-Versuch bleibt moeglich.
- Cut **nach** dem Prepared-Head-Schreiben, vor oder nach dem
  Slot-Schreiben: bestehender generischer `PreparedInterrupted`-Mechanismus
  greift unveraendert.
- Cut **nach** vollstaendigem Committed-Head-Schreiben: Erfolg wie oben;
  ein anschliessend erneut beschaedigter neuer Current fuehrt beim
  naechsten Boot wieder zu `FallbackRecovered` -> `FallbackRecoveryPending`
  – derselbe Mechanismus greift beliebig oft.

**Tests:** korrupter Current + gueltiger Fallback -> Recoverycommit -> neuer
Current gueltig **und** alter gueltiger Fallback weiterhin ladbar;
anschliessend neuen Current beschaedigen -> Fallback-Recovery funktioniert
erneut; `targetSlotOverride == fallbackOverride->slot` -> Ablehnung ohne
Schreibversuch; Cut vor/nach Prepared-/Head-Commit mit erwartetem Zustand
je Cutpoint; `state()` nach `FallbackRecovered`-Load ist
`FallbackRecoveryPending` (aktualisierter bestehender Test).

### 5.19 `BlockedIndeterminate` und `PersistenceCommittedApplyFailed` – getrennt

Store-Schreibausgang unbestimmt (`RunPersistenceStoreWriteResult::Indeterminate`,
`run_persistence_coordinator.cpp:427-431,452,466,477,486,500,518`) ->
`BlockedIndeterminate`/`StoreOutcomeUnknown`; bestaetigter Commit mit
fehlgeschlagenem RAM-Apply (`run_persistence_coordinator.cpp:617-625,676-685`)
-> `PersistenceCommittedApplyFailed`. `writeSnapshotCore` (5.16) und die
`ApplyRecoveryTimeCorrection`-Persistierung (5.22) uebernehmen beide
Zustaende unveraendert und getrennt. `FallbackRecoveryPending` (5.18) ist
ein eigener, von `BlockedIndeterminate` disjunkter Zustand und wird von
keiner dieser beiden Kategorien beruehrt.

### 5.20 Sensorevidenz – strukturell getrennt, echter Latch unabhaengig von Kontrollpunkt-Zeitpunkten

Ein einzelnes `RecoveryTemperatureEvidence{air,product,cooling}` kann nicht
gleichzeitig die laufend fortgeschriebene "letzte gueltige"-Evidenz und die
eingefrorene Vor-/Nach-Ausfall-Diagnose einer bestimmten Recoveryepisode
tragen.

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
    CrossRoleEvidence beforeOutage;              // eingefroren, bei jedem echten Hop 1 (frisch oder carry-forward, 5.12) neu gesetzt, nicht bei Episode-Refresh
    FirstAfterRestartEvidence firstAfterRestart; // gemischte Lebensdauer, siehe unten
};
```

**Das Problem eines rein schreibpfad-gebundenen Latches:** wird `Product`
z. B. um 12:00:05 erstmals gueltig, der naechste Checkpoint aber erst um
12:05 geschrieben, erfasst ein Latch, der ausschliesslich innerhalb eines
Persistenzschreibpfads ausgefuehrt wird, nicht den tatsaechlich ersten
gueltigen Wert – `SensorQualitySnapshot` selbst enthaelt keine Historie, aus
der der erste gueltige Wert spaeter rekonstruiert werden koennte.

**Vertrag – RAM-Latch, entkoppelt vom Persistenzschreib-Zeitpunkt:**

```cpp
// run_recovery.hpp – reine Mutatorfunktion auf bereits im RAM gehaltenem
// Zustand, unabhaengig davon, ob im selben Moment ein Checkpoint
// geschrieben wird.
bool recoveryEvidenceWindowOpen(const RunCommandState& current) {
    if (current.pendingRecoveryAnchor.has_value() &&
        current.processState.state == ProcessState::RecoveryEvaluation) {
        return true;  // Hop-1-only, Entscheidung noch offen (5.12)
    }
    // Fermenting/CoolHolding nach Resume: der Anker mag bereits nullopt
    // sein (Zeitfrage geloest) ODER weiterhin bestehen ("Zeitbewertung
    // noch offen", 5.12) - in BEIDEN Faellen bleibt das Fenster ueber die
    // dauerhafte PriorBootPhaseElapsed-Tag-Bindung offen, bis die Phase
    // real wechselt (5.23) - nicht ueber den Anker-Zustand. Der
    // zusaetzliche Zustandscheck oben schliesst das Fenster defensiv auch
    // dann, wenn ein spaeterer Aufrufer den Anker in einem Fault-Pfad
    // (Reject, #24) noch nicht geloescht haben sollte.
    return current.priorBootPhaseElapsed.has_value() &&
           current.priorBootPhaseElapsed->taggedState ==
               current.processState.state;
}

void applyLiveRecoveryEvidence(RunCommandState& current,
                               const CrossRolePlausibilityContext& live) {
    if (!recoveryEvidenceWindowOpen(current)) return;
    if (!current.lastRecoveryEpisodeEvidence.has_value()) return;  // defensiv
    auto& episode = *current.lastRecoveryEpisodeEvidence;
    auto latch = [](std::optional<RoleTemperatureEvidence>& slot,
                    const device_platform::SensorQualitySnapshot& liveRole) {
        if (slot.has_value()) return;  // pro Rolle genau einmal je Episode
        if (liveRole.quality != device_platform::SensorQuality::Valid) return;
        if (!liveRole.filteredCelsius.has_value()) return;  // Valid ohne Messwert verbraucht den Latch NICHT
        slot = RoleTemperatureEvidence{liveRole.filteredCelsius, liveRole.quality};
    };
    latch(episode.firstAfterRestart.air, live.air);
    latch(episode.firstAfterRestart.product, live.product);
    latch(episode.firstAfterRestart.cooling, live.cooling);
}
```

Die Latch-Bedingung ist `Valid && filteredCelsius.has_value()` – nicht
`Valid` allein: ein `Valid`-Status ohne begleitenden gefilterten Messwert
darf den Latch nicht dauerhaft mit einem inhaltsleeren Wert "verbrauchen"
(die Rolle bleibt fuer den naechsten, tatsaechlich werttragenden `Valid`-
Aufruf latchbar).

**Entkopplung von Persistenzschreibvorgaengen (Kern des Vertrags):** diese
Funktion mutiert `RunCommandState` direkt im RAM und ist unabhaengig davon
aufrufbar, ob im selben Moment ein Checkpoint geschrieben wird. Die
Persistenzschreibpfade (die vier bestehenden Checkpoint-Pfade sowie
`activateLoadedRun`/`activateFallbackRecoveredRun`/Episode-Refresh)
serialisieren anschliessend nur noch den zu diesem Zeitpunkt bereits
gelatchten Zustand – sie fuehren keine eigene Latch-Logik mehr aus. Innerhalb
von #18 wird `applyLiveRecoveryEvidence` an genau den Stellen aufgerufen, an
denen bereits ein `CrossRolePlausibilityContext` vorliegt (die fuenf
bisherigen Gelegenheiten: vier Checkpoint-Schreibpfade ueber ihren
bestehenden optionalen `liveSensorEvidence`-Parameter, sowie
`activateLoadedRun`/`activateFallbackRecoveredRun`/Episode-Refresh ueber
ihren `liveSensorEvidence`-Parameter, 5.8) – **zusaetzlich** aber
unabhaengig davon direkt (native Tests rufen die Funktion mit
zwischenzeitlichen, nicht mit einem Checkpoint zusammenfallenden
Evidenzwerten auf und weisen nach, dass korrekt gelatcht wird). Eine
produktive, hoeherfrequente Verdrahtung (z. B. jeden Regelzyklus) ist
ausdruecklich **nicht** Teil von #18 (Gate B, wie
`reevaluateRecoveryTime` bereits heute) – die Funktion selbst ist jedoch
schon jetzt korrekt fuer eine solche kuenftige Verdrahtung, ohne dass sich
ihr Verhalten dafuer aendern muesste.

**Nicht persistierte Zwischenzustaende:** ein `applyLiveRecoveryEvidence`-
Aufruf ausserhalb eines Commits aendert nur den RAM-Zustand; stuerzt das
Geraet vor dem naechsten Commit ab, geht dieser Zwischenzustand verloren
(kein Aktorgating haengt daran, ausschliesslich Diagnosedaten – ein
Datenverlust hier ist folgenlos, wird aber ausdruecklich benannt statt
stillschweigend vorausgesetzt).

**Geordnete Reihenfolge fuer `beforeOutage` (verbindlich, bei Hop 1 wie bei
jedem sonstigen Zeitpunkt, an dem `lastRecoveryEpisodeEvidence` neu
angelegt oder `firstAfterRestart` zurueckgesetzt wird):**

1. `beforeOutage` wird aus dem **soeben aus dem persistierten Datensatz
   restaurierten** `current.recoveryTemperatureEvidence.lastKnown` kopiert
   – bevor irgendeine Aktualisierung mit Post-Restart-Evidenz stattfindet.
2. **Erst danach** wird `current.recoveryTemperatureEvidence.lastKnown`
   ueber das bestehende `updateRoleEvidence(...)` mit der aktuellen,
   tatsaechlich nach dem Neustart gemessenen `CrossRolePlausibilityContext`
   aktualisiert.
3. **Erst danach** wird `applyLiveRecoveryEvidence` (s. o.) fuer
   `firstAfterRestart` mit derselben Evidenz aufgerufen.
4. **Nur falls** dieser Aufbau unmittelbar zu einem erfolgreichen
   Hop-1+Hop-2-Resume fuehrt (5.10), erfolgt Schritt 3 **bevor**
   `pendingRecoveryAnchor`/`recoveryBootAnchorMonotonicMillis` im finalen
   Kandidaten geloescht werden (5.12/5.17) – niemals danach, sonst wuerde
   die Latch-Bedingung `pendingRecoveryAnchor.has_value()` bereits
   verletzt sein, bevor der Latch ueberhaupt versucht wurde.

Wird diese Reihenfolge verletzt (z. B. `lastKnown` zuerst aktualisiert),
koennte `beforeOutage` versehentlich bereits einen Wert **nach** dem
Neustart enthalten.

`firstAfterRestart` (alle drei Rollenfelder) wird bei **jedem** Anstieg von
`recoveryEpisodeRevision` – Hop 1 **und** jedes Episode-Refresh –
vollstaendig auf `{nullopt, nullopt, nullopt}` zurueckgesetzt, unmittelbar
vor Schritt 1-3 oben. `beforeOutage` wird dagegen bei **jedem echten Hop 1**
neu gesetzt (frisch **und** carry-forward, 5.12 – jeder physische Ausfall
hat seine eigenen, tatsaechlich vor **diesem** Ausfall gemessenen
Sensorwerte, unabhaengig davon, ob 5.12s Zeitkontext fuer diesen Ausfall
frisch beginnt oder eine fortgesetzte Kette carry-forwarded; "Kette" meint
hier **nicht** 5.12s Zeitkontext-Kette, sondern schlicht "ein echter Hop 1
fand statt"); ein Episode-Refresh (kein echter Hop 1, derselbe Ausfall wird
nur erneut bewertet) laesst es unveraendert.

`lastRecoveryEpisodeEvidence: std::optional<RecoveryEpisodeEvidence>` selbst
wird bei Hop 1 neu angelegt. Nach **Resume** bleibt es als Diagnosefeld
bestehen, latcht aber **weiterhin** nach: `recoveryEvidenceWindowOpen`
(s. o.) bleibt fuer `Fermenting`/`CoolHolding` ueber den Resume-Zeitpunkt
hinaus offen, solange `priorBootPhaseElapsed` mit passendem Tag besteht
(bis zum naechsten echten Phasenwechsel, 5.23); fuer `WaitingForProduct`
nach Resume (`DefinitelyStillValid`) ebenso, ueber dasselbe
`priorBootPhaseElapsed`-Kriterium statt ueber den (dort ebenfalls bereits
geloeschten) Anker. Nach **Reject** (Fault, #24) schliesst der
Zustandscheck in `recoveryEvidenceWindowOpen` das Fenster in jedem Fall
(weder `RecoveryEvaluation` noch ein passender `priorBootPhaseElapsed`-Tag
fuer `Fault`). Nach **Tombstone** (5.11, `clearActiveRunState()`) wird
`lastRecoveryEpisodeEvidence` im selben Commit auf `nullopt` gesetzt wie
`pendingRecoveryAnchor`, `recoveryBootAnchorMonotonicMillis`,
`priorBootPhaseElapsed` (5.23) und `nominalRecoveryAdjustment` (5.22).

**Tests:**
- Echter First-valid-Latch: `applyLiveRecoveryEvidence` wird mit einer
  Zwischenevidenz aufgerufen, die zeitlich **zwischen** zwei simulierten
  Checkpoints liegt – der Latch erfasst den Wert trotzdem, unabhaengig
  davon, dass kein Checkpoint zu diesem Zeitpunkt geschrieben wird.
- `Valid` ohne `filteredCelsius`: Latch bleibt unverbraucht; ein spaeterer
  `Valid`-Aufruf mit Messwert latcht erfolgreich.
- Air beim Hop 1 bereits gueltig, Product/Cooling erst spaeter (ueber
  `applyLiveRecoveryEvidence`, unabhaengig vom naechsten Checkpoint):
  alle drei Latches korrekt und genau einmal gesetzt.
- `beforeOutage` enthaelt nachweisbar den Vor-Ausfall-Wert, nicht einen nach
  Schritt 2 bereits aktualisierten Wert (Test mit deutlich unterschiedlichen
  Vor-/Nach-Temperaturen).
- Unmittelbarer Hop1+Hop2-Erfolgspfad: `firstAfterRestart` ist korrekt
  gesetzt, obwohl `pendingRecoveryAnchor` im selben finalen Commit
  anschliessend geloescht wird.
- Episode-Refresh setzt Latches zurueck, `beforeOutage` bleibt unveraendert.
- `Fermenting`/`CoolHolding` nach Resume: `applyLiveRecoveryEvidence`
  latcht einen Wert, der erst **nach** dem Resume-Commit eintrifft
  (`pendingRecoveryAnchor` bereits `nullopt`, `priorBootPhaseElapsed` mit
  passendem Tag traegt das Fenster).
  `WaitingForProduct` nach Resume (`DefinitelyStillValid`) analog.
- Nach Reject (Fault) latcht `applyLiveRecoveryEvidence` nicht mehr
  (`recoveryEvidenceWindowOpen` liefert `false`, weder `RecoveryEvaluation`
  noch passender `priorBootPhaseElapsed`-Tag).
- Realer Phasenwechsel (z. B. `Fermenting -> Completed`) schliesst das
  Fenster: ein danach eintreffender Wert latcht nicht mehr.
- Nach Tombstone ist `lastRecoveryEpisodeEvidence` auf `nullopt` gesetzt.

### 5.21 `RunProgressState` – ehrliche Basis bleibt nach Migration und Folds bestehen

`RunProgressState` existierte vor Schema 3 nicht. Ein Fortschreiben auf
"Known" bereits nach dem ersten Fold nach einer Schema-1/2-Migration ist
semantisch falsch: ist die gesamte Vor-Schema-3-Fermentationszeit
unbekannt, macht ein spaeter beobachteter Abschnitt den zuvor unbekannten
Anteil nicht rueckwirkend bekannt.

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
  Decodierung eines Schema-1- oder Schema-2-Datensatzes fuer diesen
  aktiven Run. `basis` wird danach **nie mehr veraendert** – weder durch
  einen Fold-Punkt (5.22) noch durch `ApplyRecoveryTimeCorrection` (5.22)
  noch durch eine automatische UTC-Reevaluation (5.17).
- Bekannte neue Sekunden werden trotzdem **kumuliert**: `observedRunSeconds`
  steigt bei jedem Fold-Ereignis (5.22) unabhaengig vom Wert von `basis` um
  genau das dort tatsaechlich beobachtete Delta.
- Anzeige/Export rendert bei `PartialUnknownHistory` explizit "mindestens
  `observedRunSeconds` Sekunden bekannt, aelterer Anteil unbekannt", bei
  `KnownTotal` schlicht `observedRunSeconds`.

**Tests:**
- Schema 1 -> 3 und Schema 2 -> 3 fuer einen aktiven Run: `basis ==
  PartialUnknownHistory` sofort nach Migration.
- Mehrere Folds nach einer solchen Migration: `basis` bleibt
  `PartialUnknownHistory`, `observedRunSeconds` waechst um jedes Delta.
- `ApplyRecoveryTimeCorrection` nach einer solchen Migration: `basis`
  bleibt `PartialUnknownHistory`.
- Neuer, vollstaendig unter Schema 3 gestarteter Run: `basis == KnownTotal`,
  bleibt es fuer die gesamte Laufzeit.
- Gemischte Current/Fallback-Matrix 3/2 und 3/1; korrupter Schema-3-Current
  mit gueltigem Schema-2/1-Fallback; Prepared-Head-Unterbrechung ueber den
  Versionswechsel hinweg.

### 5.22 `observedRunSeconds` vs. `NominalRecoveryAdjustmentState` – strukturell getrennt, kumulativ, tatsaechlich wirksam

**Fortschreibungspunkte fuer `observedRunSeconds` (Fold,
`deriveFermentingSecondsDelta(before, atMillis)`):** angewandt (1) bei jedem
Live-Phasenwechsel **aus** `Fermenting`, (2) in `decideRunAdjustment()`
unmittelbar **vor** der bestehenden `stateEnteredAtMillis`-Neusetzung bei
tatsaechlicher Daueraenderung (`run_commands.cpp:1019-1023`), (3) bei Hop 1, wenn
`originalRestoredProcessState.state == Fermenting`, ausschliesslich aus
`thisHopAltBootLocalSeconds` (5.12 – dem Alt-Boot-lokalen, sicher bekannten
Beitrag, den **dieser konkrete** Hop 1 neu beisteuert; **niemals** aus dem
unsicheren Ausfallanteil). **Carry-Forward-Vertrag gemaess Auftragspunkt 1:**
vor diesem Vertrag
war jeder Hop 1 zwangslaeufig frisch, sodass
`pendingRecoveryAnchor.knownPhaseSecondsAtOriginalCheckpoint` selbst genau
diesen Beitrag enthielt. Bei einem Carry-Forward-Hop-1 (5.12) ist dieses
Feld dagegen byte-identisch aus der vorigen Episode uebernommen und wuerde,
ungeaendert wiederverwendet, `N1` ein zweites Mal falten (statt des
tatsaechlich neuen `N2`) – ein Verstoss gegen "streng beobachtete
Laufzeit" (s. u.). `thisHopAltBootLocalSeconds` ist in beiden Faellen
(frisch/Carry-Forward) exakt der neue, von diesem Hop 1 selbst
beigetragene Anteil und schliesst diese Luecke strukturell.

Bei Punkt (2) ist die Faltung kein unabhaengiger Schreibvorgang, sondern Teil
der einen `AdjustRun`-Kandidatenmutation: Der checked Delta-Wert wird vor der
Timer-Neusetzung genau einmal addiert; danach werden neue
`ProcessRunSnapshot`-Daten und die neue `stateEnteredAtMillis`-Baseline
gebildet und die alten aktiven Recovery-Zeitbeitraege gemaess 5.12/5.23
superseded. Bei `durationChanged == false` findet dieser Fold und diese
Neubaseline nicht statt. Ein Commit darf daher nie nur die Restdauer, nur die
historische Metrik oder nur die Recovery-Felder uebernehmen.

**Harte Trennung:** `observedRunSeconds` bedeutet ausschliesslich
tatsaechlich beobachtete, eingeschaltete Fermentationszeit. Es wird **unter
keinen Umstaenden** durch eine Stromausfallkorrektur veraendert – weder
durch den automatischen Hop-1-Fold, noch durch `ApplyRecoveryTimeCorrection`,
noch durch eine automatische UTC-Reevaluation.

Eine genauere UTC-Grenze beweist **Zeit**, aber nicht **biologische
Aktivitaet** (Gate C). Der kanonische Recovery-Vertrag verlangt: fehlende
Messzeit wird nur mit einem validierten thermischen Modell biologisch
bewertet; ohne Modell wird kein scheinbar exakter temperaturgewichteter
Fortschritt erfunden.

**`NominalRecoveryAdjustmentState` – kumulative, tatsaechlich wirksame
nominale Korrektur:**

Ein einzelnes optionales `{appliedAtEpisodeRevision; appliedSecondsDelta}`
reicht fuer mehrere Stromausfaelle desselben Runs nicht aus: nach einer
zweiten Recoveryepisode muesste entweder der erste Wert ueberschrieben
werden, oder die zweite Korrektur waere nicht speicherbar. Ausserdem muss
verbindlich festgelegt sein, wie eine gespeicherte Korrektur die
tatsaechliche Fermentations-/Haltezeitentscheidung ueberhaupt beeinflusst –
ein reiner Anzeigewert ohne Wirkung erfuellt den kanonischen Vertrag
("Benutzer kann Fortschritt bestaetigen") nicht.

```cpp
// Schema 3, optional, Teil von RunPersistenceSnapshot
struct NominalRecoveryAdjustmentState {
    std::uint32_t cumulativeAppliedSeconds{0U};    // Summe aller bisher bestaetigten Korrekturen dieses Runs, ueber beliebig viele Episoden
    std::uint32_t lastAppliedEpisodeRevision{0U};  // fuer Idempotenz-/Stale-Schutz der jeweils letzten Korrektur
    std::uint32_t lastAppliedEpisodeDelta{0U};     // das zuletzt in dieser Episode angewandte Delta (fuer den Idempotenzvergleich)
};
```

**Geltungsbereich:** ausschliesslich `Fermenting` (die einzige Phase mit
einer kumulativen Geschaeftsprogress-Metrik). `CoolHolding` hat keine
solche Metrik (5.17).

**Wirkung – tatsaechlich wirksam, nicht nur gespeichert:** Der Wert von
`nominalRecoveryAdjustment.cumulativeAppliedSeconds` wird bei jeder
`Fermenting`-Dauerentscheidung (`decideFermenting`,
`elapsedWithPrior`-Aufruf, 5.6) **zusaetzlich** zum strukturellen,
automatischen `priorBootPhaseElapsed.lowerBoundSeconds` (5.23) addiert:

```text
effectivePriorSecondsForFermenting =
    priorBootPhaseElapsed.lowerBoundSeconds + nominalRecoveryAdjustment.cumulativeAppliedSeconds
```

Dieser kombinierte Wert – nicht `priorBootPhaseElapsed.lowerBoundSeconds`
allein – wird als `priorSeconds` an `elapsedWithPrior` uebergeben. Erreicht
dieser kombinierte Wert die Fermentationsdauer, schliesst der naechste
normale, ohnehin bestehende `decideFermenting`-Aufruf den Lauf automatisch
ab (`completeTimedRun`) – **derselbe** Mechanismus wie bei rein Alt-Boot-
lokal bewiesener Zeit, kein zweiter Abschlussmechanismus. Das ist zulaessig
und kein Verstoss gegen Gate C: der automatische Pfad wird ausschliesslich
durch bewiesene Alt-Boot-lokale Zeit **und** durch explizit vom Benutzer
bestaetigte nominale Sekunden gespeist, niemals durch eine unbewiesene
Ausfall-Untergrenze (die ist unter 5.13 ohnehin `0`).

Diese Kombination gilt fuer den jeweils bestehenden Recovery-Kontext. Nach
einer manuellen Restdauer-Neubaseline waehrend `Fermenting` ist der aktive
`priorBootPhaseElapsed`-Wert auf `0/0` gesetzt und
`nominalRecoveryAdjustment` auf `nullopt`; der neue
`stateEnteredAtMillis`-Punkt ist die alleinige Zeitbasis fuer den neuen
Restdauer-Timer. `observedRunSeconds` wird dabei nicht in diesen
`priorSeconds`-Wert eingemischt und bleibt ausschliesslich historische,
tatsaechlich beobachtete Laufzeit.

Fuer eine **sofortige, qualitative** Abschlussentscheidung ohne exakte
Sekundenangabe steht zusaetzlich `resolveRecoveryOutcome` mit
`AssumeThresholdCrossed` zur Verfuegung (5.17) – beide Wege sind zulaessig,
unabhaengig voneinander nutzbar und fuehren zum selben Transitionstyp
(`completeTimedRun`).

**Kumulations- und Bounds-Invariante (verbindlich, verhindert doppeltes
Zaehlen und unbewiesene Kredite):** vor der Annahme einer neuen Korrektur
mit Delta `d` fuer die aktuelle Episode wird geprueft:

```text
priorBootPhaseElapsed.upperBoundSeconds bekannt
  UND priorBootPhaseElapsed.lowerBoundSeconds
      + (nominalRecoveryAdjustment.cumulativeAppliedSeconds + d)
      <= priorBootPhaseElapsed.upperBoundSeconds
```

`priorBootPhaseElapsed.lowerBoundSeconds`/`.upperBoundSeconds` sind dabei
die gemaess 5.23 bereits **ueber alle bisherigen Episoden dieser Phase
akkumulierten** strukturellen Grenzen – die Pruefung nutzt damit denselben,
bereits an anderer Stelle korrekt gefuehrten kumulativen Rahmen, statt eine
zweite, parallele Buchfuehrung einzufuehren. Ist
`priorBootPhaseElapsed.upperBoundSeconds` `nullopt` (mindestens eine
Episode ohne bekannte Obergrenze, 5.23), ist **keine** Korrektur gegen eine
bewiesene Obergrenze pruefbar -> jede Korrektur wird abgelehnt
(`InvalidInput`), **kein** Saturieren auf einen Ersatzwert. Additionsueberlauf
von `d` beim Aufaddieren auf `cumulativeAppliedSeconds` (uint32) wird vor
dem Schreiben geprueft und fuehrt zu `InvalidInput`.

**Gemeinsamer Bounds-Grundsatz mit `resolveRecoveryOutcome`
(Auftragspunkt 3, explizit gemacht statt nur implizit gueltig):** sowohl
diese Invariante als auch `resolveRecoveryOutcome`s Bounds-Gate fuer
`AssumeThresholdCrossed` (5.17) verlangen dieselbe Vorbedingung – eine
**bekannte, akkumulierte** `priorBootPhaseElapsed.upperBoundSeconds` – und
lehnen bei `nullopt` **beide** unbedingt ab, statt auf einen Ersatzwert
zu saturieren. Das ist keine zufaellige Uebereinstimmung, sondern derselbe
Mechanismus fuer zwei verschiedene Aktionsformen (quantitative Korrektur
vs. qualitative Abschlussbestaetigung): keine der beiden Formen darf einen
Abschluss ueber eine unbewiesene Obergrenze hinaus bewirken. Ein Effekt
davon (bereits strukturell garantiert, keine zusaetzliche Pruefung noetig):
`ApplyRecoveryTimeCorrection` kann `effectivePriorSecondsForFermenting`
niemals ueber die bewiesene `priorBootPhaseElapsed.upperBoundSeconds`
hinaus anheben – jedes `d`, das dies versuchen wuerde, verletzt bereits
die obige Kumulations-Invariante und wird abgelehnt. Wollte ein Aufrufer
also ueber eine Korrektur einen Abschluss ausloesen, dessen tatsaechliche
Grenze (`fermentationDurationMinutes*60`) oberhalb der bewiesenen
Obergrenze liegt, waere kein zulaessiges `d` dafuer waehlbar – dieselbe
Garantie, die 5.17s explizite Intervallpruefung fuer die qualitative
Abschlussbestaetigung liefert, ergibt sich hier bereits aus der
bestehenden Bounds-Pruefung selbst.

**Episoden-/Staleness-Vertrag:** `CommandKind::ApplyRecoveryTimeCorrection`;
`CommandEnvelope.expectedRunRevision` **und**
`expectedRecoveryEpisodeRevision` muessen beide mit `current`
uebereinstimmen, sonst `StaleState`. Innerhalb **derselben** Episode
(`lastAppliedEpisodeRevision == aktuelle Episode`): identisches `d` wie
zuvor -> `AlreadyProcessed`; abweichendes `d` -> `NotAllowedInState` (eine
bereits committete Korrektur dieser Episode wird nicht revidiert). Bei
einer **neuen** Episode (`lastAppliedEpisodeRevision != aktuelle Episode`):
die obige Bounds-Invariante wird gegen das NEUE `d` geprueft; bei Erfolg
`cumulativeAppliedSeconds += d`, `lastAppliedEpisodeRevision = aktuelle
Episode`, `lastAppliedEpisodeDelta = d`. Write-before-apply; Fehlerfaelle
getrennt nach 5.19.

**Reset:** `nominalRecoveryAdjustment` wird bei Start eines neuen Runs, bei
einer manuellen Restdauer-Neubaseline in `Fermenting` sowie in
`clearActiveRunState()` (5.11) vollstaendig auf `nullopt`
zurueckgesetzt – derselbe Helfer, der bereits `pendingRecoveryAnchor`/
`recoveryBootAnchorMonotonicMillis`/`lastRecoveryEpisodeEvidence`/
`priorBootPhaseElapsed` zuruecksetzt. Ausfuehrliche Journal-/Historienfuehrung
ueber alle einzelnen Korrekturen hinweg bleibt #19 vorbehalten; dieser
Vertrag haelt nur die aktuell wirksame kumulative Summe sowie die zuletzt
angewandte Episode fuer Idempotenz. Nach einer manuellen Restdauer-Neubaseline
bedeutet ein erneut vorhandenes Feld ausschliesslich "seit der letzten
manuellen Restdauer-Baseline"; ein alter Wert darf nicht weiterwirken.

**Anzeige/Export:** `observedRunSeconds` und
`nominalRecoveryAdjustment.cumulativeAppliedSeconds` werden **getrennt**
ausgewiesen (z. B. "X Sekunden beobachtet, zusaetzlich Y Sekunden manuell
als Ausfallzeit angerechnet"); keine stille Zusammenfuehrung in ein
einzelnes, ununterscheidbares Feld.

**Tests:**
- `ApplyRecoveryTimeCorrection` innerhalb der (akkumulierten) Grenzen
  erfolgreich, treibt bei Erreichen der Fermentationsdauer den naechsten
  `decideFermenting`-Aufruf tatsaechlich zum Abschluss.
- Ausserhalb der Grenzen -> Ablehnung ohne Saturierung; ohne bekannte
  (akkumulierte) Obergrenze -> Ablehnung; Overflow-Schutz.
- Zwei Recoveryepisoden mit zwei Korrekturen desselben Runs: beide bleiben
  in `cumulativeAppliedSeconds` erhalten (Summe, keine Ueberschreibung).
- Dieselbe Episode, identisches `d` -> `AlreadyProcessed`; abweichendes `d`
  -> `NotAllowedInState`.
- `observedRunSeconds` bleibt bei jeder Ausfallkorrektur (automatisch und
  manuell) unveraendert.
- Restdaueranpassung waehrend `Fermenting`: der seit der alten
  `stateEnteredAtMillis`-Baseline sicher beobachtete Anteil wird vor der
  Timer-Neusetzung genau einmal gefaltet; neue Restdauer und neuer Timer
  werden ab Kommandozeitpunkt angewandt; `priorBootPhaseElapsed` ist danach
  `0/0`, `nominalRecoveryAdjustment` ist leer, und ein alter offener Anker ist
  geschlossen.
- Reine Zieltemperaturaenderung waehrend `Fermenting`: keine Faltung, kein
  Reset von Prior-/Nominalzeit und kein Schliessen eines Recovery-Kontexts;
  `stateEnteredAtMillis` bleibt unveraendert. Kombinierte Ziel- und
  Restdaueranpassung folgt der Restdauerregel.
- `remainingDurationMinutes == 0`: bestehende unmittelbare
  Abschlusszulaessigkeit bleibt nach der neuen Baseline erhalten.
- Restdaueranpassung mit stale Erwartungsrevision, monotone Zeit vor der
  bisherigen Baseline, Additionsueberlauf oder Persistenzfehler: keine
  teilweise neue Dauer, kein teilweise geloeschter Recovery-Kontext und kein
  doppelter Fold.
- **Carry-Forward-Kette, kein doppeltes/verlorenes Zaehlen bei
  `observedRunSeconds` (Auftragspunkt 1):** eine
  Drei-Reboot-Kette in `Fermenting` mit Alt-Boot-lokalen Beitraegen `N1`,
  `N2`, `N3` (frischer Hop 1, dann zwei Carry-Forward-Hop-1s) faltet
  `observedRunSeconds` um genau `N1 + N2 + N3` – `N1` erscheint dabei
  **kein zweites Mal** (Regressionstest gegen ein Wiederaufgreifen von
  `pendingRecoveryAnchor.knownPhaseSecondsAtOriginalCheckpoint` bei einem
  Carry-Forward-Hop-1, das `N1` erneut statt des tatsaechlich neuen `N2`
  falten wuerde).
- Automatische UTC-Reevaluation erzeugt keine Aenderung an
  `NominalRecoveryAdjustmentState` oder `observedRunSeconds`.
- Neuer Run/`clearActiveRunState()` setzt `nominalRecoveryAdjustment`
  vollstaendig zurueck.
- Anzeige/Export weist beide Werte nachweisbar getrennt aus.

### 5.23 `PriorBootPhaseElapsed` – vollstaendiger Persistenz- und Lebenszyklusvertrag

**Wire-/RAM-Feld:**

```cpp
// run_persistence_contract.hpp, Schema 3, Teil von RunPersistenceSnapshot
struct TaggedPriorBootPhaseElapsed {
    ProcessState taggedState;             // Phase, fuer die dieser Vor-Boot-Anteil gilt
    PriorBootPhaseElapsed elapsed;        // 5.6: lowerBoundSeconds + optional upperBoundSeconds
};
// RunPersistenceSnapshot: std::optional<TaggedPriorBootPhaseElapsed> priorBootPhaseElapsed;
```

**Codec/Migration:** Schema-3-Feld; eine Schema-1- oder Schema-2-Decodierung
liefert immer `nullopt`.

**Invariante:** die einzige normative Fassung steht in
`validateRunPersistenceSnapshot()`, 5.14 Punkt 4 (Normalfall
`taggedState == processState.state`; Hop-1-only-Ausnahme fuer
`RecoveryEvaluation` gegen `pendingRecoveryAnchor->originalProcessState.state`;
sonst `nullopt`) – diese Sektion wiederholt sie nicht eigenstaendig, um
genau die Zwei-Fassungen-Falle zu vermeiden, die die Hop-1-only-Ausnahme in
5.14 erst noetig gemacht hat.

**Leserseitige defensive Absicherung (5.6), explizit abgegrenzt von der
Akkumulationsregel unten:** jeder **live-Phasentimer-Leser**
(`elapsedWithPrior`, waehrend `WaitingForProduct`/`Fermenting`/
`CoolHolding` laeuft) liest `priorBootPhaseElapsed` nur, wenn
`current.processState.state` mit dem Tag uebereinstimmt, sonst wird `{}`
verwendet. Diese Regel gilt **nicht** fuer die Akkumulationsregel unten:
Hop 1 vergleicht dort bewusst gegen `originalRestoredProcessState.state`
(5.9 – dieselbe Phase wie `pendingRecoveryAnchor->originalProcessState.state`,
5.12, aber bereits als lokale Kopie vorhanden, **bevor** der Anker selbst
konstruiert wird), nicht gegen `current.processState.state` (das durch
`applyProcessTransition(hop1)` bereits auf `RecoveryEvaluation` gesetzt
sein kann).

**Setzen:** bei einer Recovery-Aktivierung (Hop 1 oder Hop 2 mit Resume,
5.9/5.10), wenn die resultierende Phase `WaitingForProduct`, `Fermenting`
oder `CoolHolding` ist (5.7), getaggt mit genau dieser resultierenden Phase.
Zusaetzlich setzt eine manuelle Restdauer-Neubaseline in `Fermenting` gemaess
5.12/5.22 das Feld explizit auf
`TaggedPriorBootPhaseElapsed{Fermenting, {0U, std::optional<uint32_t>{0U}}}`.
Dieser Nullstand ist die neue aktive Baseline: die historische
`observedRunSeconds`-Metrik bleibt davon unberuehrt, wird aber von keiner
Fermenting-Dauerentscheidung als priorer Timeranteil gelesen.

**Akkumulationsregel bei wiederholter Recovery innerhalb derselben, noch
nicht gewechselten Phase (kein doppeltes Zaehlen, kein stilles
Weglassen):** existiert beim Aufbau eines neuen Hop 1 bereits ein
`priorBootPhaseElapsed` mit `taggedState == originalRestoredProcessState.state`
(der Phase, aus der recovert wird – nicht `current.processState.state`),
wird der neue Wert **addiert**, nicht ersetzt. Da `originalRestoredProcessState`
(5.9) bereits als unveraenderte Kopie **vor jeder weiteren Hop-1-Aktion**
vorliegt (insbesondere vor der Konstruktion des Ankers selbst und vor
`applyProcessTransition(hop1)`), ist dieses Lesen von `alt` und die
Berechnung von `neu` an **keine** Reihenfolge relativ zu
`applyProcessTransition(hop1)` gebunden – einzige Anforderung ist, dass
`candidate.priorBootPhaseElapsed = neu` vor dem Commit dieses Hop 1
gesetzt wird (5.9). Die weiter oben (Abschnitt "Erhalt/Loeschen")
beschriebene Ausnahme fuer `RecoveryReentryRequired`/`RecoveryResumed`
von der Phasenwechsel-Loeschregel bleibt unabhaengig davon verbindlich –
sie schuetzt gegen eine generische, `applyProcessTransition`-gekoppelte
Loeschimplementierung, die unabhaengig vom Lesezeitpunkt greifen koennte.
**`outage` und der Known-Seconds-Term werden hier – wie ueberall in diesem
Plan (5.12) – aus `deriveEffectiveAnchorTimeBasis(pendingRecoveryAnchor)`
gebildet, nicht aus den rohen Anker-Feldern direkt:**

```text
basis = deriveEffectiveAnchorTimeBasis(pendingRecoveryAnchor)  // 5.12
outage = computeRecoveryOutageBounds({utcAtLastCheckpoint: basis.effectiveCheckpointUtc, ...})  // 5.2

neu.lowerBoundSeconds = alt.lowerBoundSeconds
    + basis.effectiveKnownSecondsBeforeCheckpoint
    + outage.outageSecondsLowerBound
neu.upperBoundSeconds =
    (alt.upperBoundSeconds.has_value() && outage.outageSecondsUpperBound.has_value())
        ? alt.upperBoundSeconds.value()
            + basis.effectiveKnownSecondsBeforeCheckpoint
            + outage.outageSecondsUpperBound.value()
        : std::nullopt  // ein einziges unbekanntes Teilintervall macht die gesamte akkumulierte Obergrenze unbekannt
```

Fuer einen frischen Anker (`knownSecondsSinceOriginalCheckpoint == 0`) ist
`basis.effectiveKnownSecondsBeforeCheckpoint ==
pendingRecoveryAnchor.knownPhaseSecondsAtOriginalCheckpoint` und
`basis.effectiveCheckpointUtc == pendingRecoveryAnchor.originalCheckpointUtc`
– byte-identisch zur Formel fuer den frischen Einzelausfall-Fall. Fuer
einen carry-forwarded Anker schliesst dieselbe Formel automatisch den
gesamten, bereits mehrfach fortgesetzten Zeitkontext ein, ohne gesondert
behandelt werden zu muessen.

Diese akkumulierte Obergrenze ist dieselbe, die 5.22 fuer die
Bounds-Invariante der nominalen Korrektur wiederverwendet – **eine**
kumulative Buchfuehrung fuer beide Zwecke, keine zweite parallele.

Stimmt `taggedState` **nicht** mit der Zielphase ueberein, beginnt die
Akkumulation bei `lowerBoundSeconds = 0`, `upperBoundSeconds = nullopt`.
Welcher der beiden akkumulierten Werte an `elapsedWithPrior` weitergereicht
wird, folgt der Phasenregel aus 5.10 (Untergrenze fuer alle Phasen ausser
`WaitingForProduct` mit `DefinitelyStillValid`, dort Obergrenze); fuer
`Fermenting` wird die Untergrenze zusaetzlich um
`nominalRecoveryAdjustment.cumulativeAppliedSeconds` ergaenzt (5.22).

**Erhalt/Loeschen:**

- Bleibt fuer die laufende, zeitbegrenzte Phase erhalten.
- Wird bei einer bestaetigten Restdauer-Neubaseline waehrend `Fermenting`
  auf den getaggten `0/0`-Nullstand zurueckgesetzt; der alte
  `priorBootPhaseElapsed`-Beitrag wird nicht in die neue Restdauer
  uebertragen. Das ist eine explizite Baseline-Mutation und kein echter
  Phasenwechsel.
- Wird bei **jedem** echten Phasenwechsel atomar im selben Commit geloescht
  (`nullopt`) – **mit Ausnahme** von Transitionen mit
  `TransitionReason::RecoveryReentryRequired` (Hop 1, in
  `RecoveryEvaluation` hinein) und `TransitionReason::RecoveryResumed`
  (Hop 2, aus `RecoveryEvaluation` zurueck in dieselbe getaggte Phase).
  Beide sind Recovery-Buchhaltungs-Uebergaenge, kein fachlicher
  Phasenwechsel: waere `RecoveryReentryRequired` selbst
  loeschungsausloesend, wuerde die Akkumulationsregel (s. o.) den
  vorhandenen `alt`-Wert bereits verlieren, bevor Hop 1 ihn addieren kann.
  Eine generische, an `applyProcessTransition` gekoppelte
  Loeschimplementierung **muss** diese beiden `TransitionReason`-Werte
  explizit von ihrer Phasenwechsel-Erkennung ausnehmen; ein Test unten
  sichert das ab.
- Wird bei Start eines neuen Runs sowie in `clearActiveRunState()`
  ebenfalls geloescht.
- **Dieselbe Ausnahme gilt identisch fuer `pendingRecoveryAnchor`/
  `recoveryBootAnchorMonotonicMillis` (neu in dieser Revision, 5.12):**
  seit diese beiden Felder auch nach einem Resume erhalten bleiben koennen
  ("Zeitbewertung noch offen"), sind sie demselben generischen
  Phasenwechsel-Loeschrisiko ausgesetzt wie `priorBootPhaseElapsed` –
  `RecoveryReentryRequired`/`RecoveryResumed` duerfen sie ebenso wenig
  ueber eine generische, an `applyProcessTransition` gekoppelte
  Loeschimplementierung veraendern; ihre tatsaechliche Loeschung folgt
  ausschliesslich der expliziten Regel aus 5.12
  (`recoveryTimeResolvedAtResume`, Tombstone/Abschluss, oder ein echter,
  **anderer** Phasenwechsel).
- **Zweiter Ausfall waehrend bereits resumtem Lauf, ggf. mit weiterhin
  offenem Zeitkontext (5.12, dort im Detail; Praezisierung zu
  Auftragspunkt 1):** ein erneuter Reboot waehrend
  `Fermenting`/`CoolHolding`/`WaitingForProduct` (nach einem frueheren
  erfolgreichen Resume) laedt `state != RecoveryEvaluation` als
  regulaeren `LoadedActiveRun` (5.15) und loest einen **frischen** Hop 1
  aus. War der vorige Zeitkontext bereits geloescht (Zeitfrage der ersten
  Episode war bereits aufgeloest), konstruiert dieser Hop 1 einen
  vollstaendig neuen `PendingRecoveryAnchor` (5.12 "Frischer Fall"); die
  Akkumulationsregel liest dabei den unveraendert bestehenden `alt`-Wert
  dieses `priorBootPhaseElapsed` (der die vorangegangene, bereits
  aufgeloeste Episode enthaelt) und addiert die neue Episode hinzu –
  unveraendert zum bestehenden Einzelausfallvertrag. War der vorige Zeitkontext dagegen noch
  **offen** (Zeitfrage der ersten Episode war beim Ausfall noch nicht
  geloest), ist dieser Hop 1 gemaess 5.12 der **Carry-Forward-Fall**: der
  bestehende Anker (inklusive seines urspruenglichen
  `originalCheckpointUtc`, sofern damals bekannt) wird **fortgesetzt**,
  nicht ersetzt – die Aufloesbarkeit der ersten Episode geht **nicht**
  mehr verloren, sobald spaeter (auf einem beliebigen Boot dieser Kette)
  NTP verfuegbar wird (5.12 fuehrt den vollstaendigen Mechanismus und die
  zugehoerigen Tests). Ein `upperBoundSeconds == nullopt` nach der ersten
  Episode bleibt in diesem Fall **vorlaeufig** offen, nicht **dauerhaft**
  unaufloesbar – "dauerhaft `nullopt`" bleibt ausschliesslich dem Fall
  vorbehalten, dass niemals vor Beginn der gesamten Kette ein UTC-Anker
  bekannt war (5.12).

**Neue Recovery-Kette nach manueller Restdauer-Baseline:** Ein spaeterer
Stromausfall behandelt den Kommandozeitpunkt als neuen
`stateEnteredAtMillis`-Ausgangspunkt. `knownPhaseSecondsAtOriginalCheckpoint`
und – falls die Kette spaeter fortgesetzt wird –
`knownSecondsSinceOriginalCheckpoint` enthalten dann nur seit diesem Punkt
im jeweiligen Boot sicher bekannte Sekunden. `observedRunSeconds` darf die
gesamte Vor-Anpassungs-Historie weiter ausweisen, wird aber weder als
`priorBootPhaseElapsed` in den neuen Timer eingefaltet noch durch einen
Carry-Forward erneut als Recoveryzeit gelesen. Fuer eine neue offene Kette
werden `accumulatedBeforeEpisode` aus dem neuen `0/0`-Stand und die Nominal-
Korrektur aus dem leeren Zustand aufgebaut. Der bestehende Carry-Forward-
Vertrag fuer danach entstehende Reboots bleibt ansonsten unveraendert.

**Reine Zieltemperaturaenderung:** Sie ist kein Setzen oder Loeschen einer
Restdauer-Baseline. Solange `durationChanged == false`, bleiben
`priorBootPhaseElapsed`, `nominalRecoveryAdjustment`,
`pendingRecoveryAnchor`, `recoveryBootAnchorMonotonicMillis` und
`stateEnteredAtMillis` byte- beziehungsweise wertgleich im Kandidaten; die
neue Zieltemperatur wird nach dem bestehenden Vertrag angewandt.

**Tests:**
- Roundtrip fuer `WaitingForProduct`, `Fermenting`, `CoolHolding` je
  einzeln.
- Phasenwechsel loescht das Feld atomar im selben Commit.
- Neuer Run und `clearActiveRunState()` loeschen das Feld.
- Zwei aufeinanderfolgende Recovery-Episoden innerhalb derselben Phase:
  Akkumulation korrekt (Unter- **und** Obergrenze).
- Eine der beiden Episoden ohne bekannte Obergrenze: akkumulierte
  Obergrenze wird `nullopt`.
- Snapshot mit nicht zur aktuellen Phase passendem Tag ist gemaess 5.14
  strukturell ungueltig.
- `RecoveryReentryRequired` (Hop 1) und `RecoveryResumed` (Hop 2) loeschen
  `priorBootPhaseElapsed` **nicht** – Negativtest gegen eine generische,
  faelschlich auf jeden `processState.state`-Wechsel reagierende
  Loeschimplementierung. Derselbe Negativtest zusaetzlich fuer
  `pendingRecoveryAnchor`/`recoveryBootAnchorMonotonicMillis` (neu, s. o.).
- Zweiter Ausfall waehrend bereits resumtem `Fermenting`
  (`Fermenting -> RecoveryEvaluation -> Fermenting`, zwei vollstaendige
  Hop-1/Hop-2-Zyklen ohne dazwischenliegenden echten Phasenwechsel):
  `priorBootPhaseElapsed` akkumuliert beide Ausfaelle korrekt, identisch
  zum Fall ohne zwischenzeitlichen Resume; ist die Obergrenze nach Episode 1
  bereits bekannt (Zeitfrage dort geloest), ist der Anker vor Episode 2s
  Hop 1 bereits `nullopt` (Regressionstest);
  ist sie bei Episode 1 noch `nullopt` (Zeitfrage dort offen), konstruiert
  Episode 2s Hop 1 den Carry-Forward-Fall (5.12) und die Zeitfrage bleibt
  **weiterhin aufloesbar**, sobald spaeter NTP verfuegbar wird – **nicht**
  mehr **dauerhaft** `nullopt` (Regressionstest gegen den hier
  behobenen Informationsverlust, Details und die Drei-Reboot-Erweiterung
  in 5.12).

### 5.24 `Completed` – expliziter, schmaler Sonderpfad

`docs/RECOVERY_AND_INTERRUPTION.md:163-174` verlangt fuer `COMPLETED`: keine
Temperaturregelung neu starten, Ergebniszustand wiederherstellen, erst
Benutzerquittierung fuehrt nach `STANDBY`. `RecoveryReentryRequired` deckt
`Completed` bewusst nicht ab (`stateUsesRunSnapshot(Completed) == false`).

**Vertrag:** `RunPersistenceCoordinator`/`RunRecoveryCoordinator` behandeln
`processState.state == Completed` als eigenen, fruehen Sonderfall, **vor**
jedem Hop-1-Versuch:

- Keine `TransitionDecision`/`applyProcessTransition` noetig.
- Direkte Uebernahme: `current = restoredState`, mit **einer** expliziten
  technischen Korrektur: `current.processState.stateEnteredAtMillis =
  monotonicMillis` (aktueller Boot) – noetig, da `decideProcessTransition`
  bei der spaeteren `CompletionAcknowledged`-Entscheidung sonst
  `runtimeTimeIsValid` gegen einen Alt-Boot-Wert prueft und faelschlich
  `TimeWentBackwards` liefern koennte.
- Keine Aktorfreigabe.
- `state_ = Ready` direkt, kein `LoadedActiveRun`-Zwischenschritt.
- Bestehender `CompletedRunRestored`-Reason bleibt unangetastet.

Test: persistierter `Completed`-Snapshot bleibt nach Reboot `Completed` bis
zur Quittierung; `stateEnteredAtMillis` liegt im aktuellen Boot; kein
`InvalidDecision`.

### 5.25 `WeightedProgressStatus` – entfernt (KISS)

Ein persistiertes Enum mit exakt einem heute erreichbaren Wert traegt
keinen Zustand, der sich innerhalb von Release 1 aendern kann.

**Vertrag:** Kein `WeightedProgressStatus`-Feld im Schema-3-Persistenzvertrag.
"Nicht kalibriert" ist eine statische, aus dem Firmware-/Commissioning-Stand
ableitbare Anzeige-/Exportkonstante (nicht pro Lauf persistiert). Die reale
Aktivitaetsgewichtung bleibt ohne validiertes Modell ausdruecklich
unimplementiert; #34 liefert die Messgrundlage (verifiziert per `gh issue
view 34`), ist aber nicht der stille Modelleigentuemer. Derselbe
Erreichbarkeitsmassstab gilt fuer jeden neu eingefuehrten Enum in diesem
Plan (`RecoveryConfidence`, 5.5): kein Wert wird eingefuehrt, der unter den
hier festgelegten Vertraegen praktisch nie erreichbar waere.

### 5.26 Restart-Sensorauswahl (Gate A)

`SensorSelectionPhase::RestartRevalidationPending` wird zwischen Hop 1 und
Hop 2 real bewertet; `computeRestartSensorSelection`
(`sensor_selection.cpp:890-907`, Stub) wertet den persistierten
Sensorselektionszustand gegen die aktuelle `CrossRolePlausibilityContext`
aus; negatives Ergebnis -> `RecoveryReject`. Dieselbe
`CrossRolePlausibilityContext`, an `activateLoadedRun`/
`activateFallbackRecoveredRun` als `liveSensorEvidence` uebergeben (5.8),
liefert auch den Kontext fuer `applyLiveRecoveryEvidence` (5.20).

### 5.27 Komposition/DI – kein erfundener Aufrufer

`RunRecoveryCoordinator::activate(...)`, `reevaluateRecoveryTime(...)`
(5.12) und `applyLiveRecoveryEvidence(...)` (5.20) sind nativ testbare
APIs; kein bestehender produktiver Aufrufer wird behauptet – insbesondere
kein produktiver NTP-Verfuegbarkeits-Aufrufer, der `reevaluateRecoveryTime`
automatisch nach erfolgreicher Zeitsynchronisation ausloest; produktive
Verdrahtung bleibt dem zustaendigen Composition-Issue vorbehalten
(Gate B).

### 5.28 Schema-Versionierung

`kCurrentRunPersistenceSchema` (aktuell `2U`,
`run_persistence_contract.hpp:24`) wird auf `3U` angehoben.
`knownRunPersistenceSchema()` akzeptiert nach dieser Aenderung `{1U, 2U,
3U}` (bisher `{1U, 2U}`). Alle in diesem Plan neu eingefuehrten Felder
(`PendingRecoveryAnchor`, `recoveryBootAnchorMonotonicMillis`,
`RunProgressState`, `RecoveryEpisodeEvidence`, `NominalRecoveryAdjustmentState`,
`TaggedPriorBootPhaseElapsed`, `recoveryEpisodeRevision`) sind
Schema-3-exklusiv; eine Schema-1/2-Decodierung liefert fuer jedes davon den
jeweiligen Leerwert (`nullopt`/`0`, `basis` wird `PartialUnknownHistory`
statt `KnownTotal`, nach 5.21-Migrationsregel). `RunPersistenceMutationKind::Recovery`
(5.16, Wert `4U`) ist ein Codec-Wert innerhalb des Prepared-Head-
Wireformats, kein Schema-3-Feld im engeren Sinn, aber ebenso erst ab
Schema 3 erzeugt.

### 5.29 ROADMAP-Konsistenz

Revision 10 aktualisiert `docs/ROADMAP.md` minimal: `docs/ROADMAP.md:13`
wechselt von `Revision 9 in Arbeit, Freigabe steht aus` auf `Revision 10 in
Arbeit, Freigabe steht aus`; `docs/ROADMAP.md:3` (`Stand:`) wird auf das
Datum des Plan-Commits gesetzt. Inhaltlich unveraendert bleiben #18/PR #102
als aktuelle Arbeit (Prioritaet 1), das Ressourcen-Gate aus PR #103 ueber
#29/`OPEN_POINTS.md` (Prioritaet 2) und die naechste fachliche Arbeit nach
#18. Kein weiterer ROADMAP-Aenderungsbedarf ist vorgesehen.

### 5.30 #24-Abgrenzung

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
| 1 | `feat(process-state-machine): RecoveryReentryRequired-/RecoveryEndedByExpiredWait-Topologie, PriorBootPhaseElapsed-Parameter, elapsedWithPrior, completeHoldDuration-Extraktion (von decideCoolHolding wiederverwendet)` | 5.6-5.11, 5.17 |
| 2 | `feat(persistence): Schema 3 – PendingRecoveryAnchor (inkl. knownSecondsSinceOriginalCheckpoint fuer Carry-Forward-Ketten), recoveryBootAnchorMonotonicMillis, RunProgressState, RecoveryEpisodeEvidence, NominalRecoveryAdjustmentState, TaggedPriorBootPhaseElapsed, recoveryEpisodeRevision, validStateFor-Erweiterung, Schema-Bump auf 3` | 5.12, 5.14, 5.20, 5.21, 5.22, 5.23, 5.28; Migrationstests |
| 3 | `feat(recovery): computeRecoveryOutageBounds, computeRecoveredPhaseElapsed, evaluateRecoveryTimeVerdict, deriveUtcAtRecoveryBootAnchor, deriveRecoveryConfidence, deriveEffectiveAnchorTimeBasis` | `run_recovery_time.hpp/.cpp` (5.2-5.5, 5.12, 5.13) |
| 4 | `feat(sensor-selection): reale Restart-Reaktivierung` | Gate A / 5.26 |
| 5 | `feat(persistence-coordinator): writeSnapshotCore mit explizitem Rollbackzustand, Fallback-Override-Parameter, FallbackRecoveryPending-Zustand, RunPersistenceMutationKind::Recovery, Signaturerweiterung der vier Checkpoint-Schreibpfade um liveSensorEvidence; bestehenden FallbackRecovered-state()-Test (`test_run_persistence_coordinator.cpp:1454-1456`) und `readMutationKind`-Test auf den neuen Wert aktualisiert` | 5.16, 5.18, 5.19, 5.20 |
| 6 | `feat(persistence-coordinator): activateLoadedRun (Hop 1 + bedingt Hop 2, frisch/Carry-Forward-Ankerkonstruktion), applyLiveRecoveryEvidence, Episode-Refresh-Pfad, bedingte Anker-Loeschung ueber recoveryTimeResolvedAtResume` | 5.8-5.10, 5.12, 5.15, 5.20 |
| 7 | `feat(persistence-coordinator): activateFallbackRecoveredRun (identische frisch/Carry-Forward-Fallunterscheidung wie activateLoadedRun), Slot-/Fallback-Override, Slot-Distinctness-Guard` | 5.8, 5.18 |
| 8 | `feat(persistence-coordinator): resolveRecoveryOutcome (ResolveRecoveryUncertaintyRequest, phasenuebergreifend WaitingForProduct/Fermenting/CoolHolding, liveSensorEvidence-Parameter fuer Gate-A-Kopplung, echter Resume fuer WaitingForProduct+AssumeStillValid, Bounds-Gate fuer AssumeThresholdCrossed), Completed-Sonderpfad` | 5.17, 5.24 |
| 9 | `feat(run-commands): ApplyRecoveryTimeCorrection (kumulativ, wirksam, gemeinsamer Bounds-Grundsatz mit resolveRecoveryOutcome), AdjustRun-Zeitfaltung und manuelle Restdauer-Baseline` | 5.12, 5.22, 5.23 |
| 10 | `feat(recovery): RunRecoveryCoordinator (activate, reevaluateRecoveryTime – Verdicts und Zeit-Nachtragskorrektur fuer bereits resumte Laeufe ueber deriveEffectiveAnchorTimeBasis via RunPersistenceMutationKind::Recovery)` | 5.12, 5.17, 5.22, 5.27 |
| 11 | `docs: Anzeigevertrag (getrennte Ausweisung observedRunSeconds/kumulativer nominaler Korrektur, LegacyUnknown-Anzeige, RecoveryConfidence), Ressourcenbudget` | Abschnitt 10 |

`docs/ROADMAP.md` (Statuszeile, Stand-Datum) wird **nicht** ueber diese
Commit-Liste behandelt – die Korrektur ist bereits Teil **dieses**
Plan-Revision-Commits (5.29), nicht der spaeteren Umsetzung.

## 8. Testmatrix

1. Hop 1 mit echter geladener Altphase; Negativtest gegen vorgetaeuschten
   `Boot`-Quellzustand.
2. `computeRecoveryOutageBounds`: `maxCheckpointGapSeconds == nullopt` ->
   `outageSecondsLowerBound == 0`; exakte UTC-Bruecke bleibt bei gesetztem
   Gap ein Intervall; fehlender UTC-Anker -> `nullopt`; checked Arithmetic.
3. `computeRecoveredPhaseElapsed`: liefert `std::optional`-Ergebnisebene
   (Rechenfehler) getrennt von `totalSecondsUpperBound == nullopt`
   (fachlich unbekannt); Additionsueberlauf -> Funktionsergebnis `nullopt`.
4. Frischer Boot mit `now` deutlich kleiner als bereits bekannte
   Phasenzeit: **kein** unsigned Rebasing-Unterlauf.
5. `WaitingForProduct`/`Fermenting`/`CoolHolding` mit Vor-Boot-Phasenanteil
   ueber `priorElapsed` korrekt beruecksichtigt.
6. Target-Reach-/Qualification-Timer aus altem Boot werden niemals roh
   weiterverwendet (volle Tabelle 5.7, alle acht Phasen).
7. `WaitingForProduct` `DefinitelyExpired` **nur** erreichbar, wenn der
   Alt-Boot-lokale Anteil allein die Grenze erreicht; kein falsches
   `DefinitelyExpired` aus einer unbewiesenen Ausfall-Untergrenze; Tombstone
   -> `ReadyEmpty` -> Reboot `NoActiveRun`.
8. Aktiver Schema-3-Snapshot mit `RecoveryEvaluation` nur mit gueltigem
   `PendingRecoveryAnchor` gueltig; ohne/mit inkonsistentem Kontext
   ungueltig (5.14 Punkt 2). Ohne `priorBootPhaseElapsed` weiterhin
   gueltig; mit `priorBootPhaseElapsed` nur, wenn dessen `taggedState`
   `pendingRecoveryAnchor->originalProcessState.state` entspricht (nicht
   `RecoveryEvaluation` selbst) – Positivtest fuer den zweiten Ausfall
   waehrend bereits einmal resumtem `WaitingForProduct`; Negativtest mit
   `taggedState == RecoveryEvaluation` (5.14 Punkt 4). Snapshot mit
   `processState.state in {WaitingForProduct, Fermenting, CoolHolding}`
   **und** gesetztem `PendingRecoveryAnchor` gueltig, wenn
   `priorBootPhaseElapsed` regulaer getaggt ist **und** dessen Obergrenze
   `nullopt` ist (5.14 Punkt 3, "Recovery-Zeitbewertung noch offen");
   Negativtest mit **bekannter** Obergrenze bei weiterhin gesetztem Anker
   -> ungueltig (strukturell inkonsistent gemaess 5.12).
9. `PendingRecoveryAnchor` ueber Hop-1-Commit und mehrere Reboots (5.12):
   UTC spaeter im selben Boot bzw. im zweiten Boot verfuegbar; Anker
   byte-identisch ueber mehrere Reboots, **explizit einschliesslich**
   `accumulatedBeforeEpisode` (Codec-Roundtrip-Test: bei Hop 1 mit sowohl
   `alt.lowerBoundSeconds` als auch `alt.upperBoundSeconds` bekannt
   gesetzt, ueber Persistenz-Schreiben und -Lesen sowie einen zweiten
   Reboot hinweg fuer **beide** Felder unveraendert nachweisen) **und**
   `knownSecondsSinceOriginalCheckpoint` (Codec-Roundtrip-Test: nach
   mindestens einem Carry-Forward auf einen von `0` verschiedenen Wert
   gesetzt, ueber Persistenz-Schreiben und -Lesen sowie einen weiteren
   Reboot hinweg unveraendert nachweisen – ein Test mit `0` koennte eine
   fehlende Codierung dieses Feldes nicht von einem frischen Anker
   unterscheiden);
   `utcNow == nullopt` und negative `*utcNow` sowie Integer-Grenzwerte fuer
   `deriveUtcAtRecoveryBootAnchor` (5.12); spaetere UTC-Reevaluation
   verwendet nicht den Hop-1-/Episode-Refresh-Commit als Ausfallanker.
10. **Recovery-Zeitbewertung noch offen nach Resume, entkoppelt von der
    Aktorfreigabe (5.12, Auftragspunkt 1 – Detailtests dort):**
    `Fermenting`/`CoolHolding` resumen ohne NTP unbedingt, Anker bleibt
    bestehen (`recoveryTimeResolvedAtResume == false`); spaeterer
    `reevaluateRecoveryTime`-Aufruf mit jetzt verfuegbarer UTC faltet die
    Obergrenze nach, loescht danach den Anker, committet als fuenften
    `RunPersistenceMutationKind::Recovery`-Ausloeser; ein
    `reevaluateRecoveryTime`-Aufruf ohne Verbesserung committet nichts;
    `decideFermenting`/`decideCoolHolding`/`decideWaitingForProduct`
    liefern unabhaengig von `pendingRecoveryAnchor.has_value()` identische
    Ergebnisse (kein erneutes Aktor-Blocking). **Weiterer Reboot waehrend
    noch offener Zeitbewertung – Carry-Forward statt Verlust (5.12,
    Carry-Forward-Fall):** durchlaeuft einen frischen Hop 1
    (nicht Episode-Refresh), der `loadedRecord.snapshot.pendingRecoveryAnchor.has_value()`
    erkennt und den Anker fortsetzt statt ihn zu ersetzen
    (`originalCheckpointUtc`/`knownPhaseSecondsAtOriginalCheckpoint`/
    `accumulatedBeforeEpisode` byte-identisch, `knownSecondsSinceOriginalCheckpoint`
    erhoeht); ein urspruenglich bekanntes `originalCheckpointUtc` bleibt
    ueber diesen zweiten Ausfall hinweg aufloesbar, sobald NTP spaeter
    verfuegbar wird; dasselbe fuer zeitbegrenztes `CoolHolding`; **drei
    aufeinanderfolgende Ausfaelle** vor jeder NTP-Verfuegbarkeit verlieren
    die Aufloesbarkeit ebenfalls nicht; explizite Summenpruefung gegen
    doppeltes Zaehlen (`priorBootPhaseElapsed->elapsed.lowerBoundSeconds ==
    accumulatedBeforeEpisode.lowerBoundSeconds + N1 + N2 + N3` fuer eine
    Drei-Episoden-Kette mit Alt-Boot-lokalen Beitraegen `N1`/`N2`/`N3`);
    ein zwischenzeitlich tatsaechlich geschriebener Checkpoint zwischen
    zwei Ausfaellen erhoeht `knownSecondsSinceOriginalCheckpoint` und
    zaehlt nicht als Ausfallzeit; bei gesetztem `maxCheckpointGapSeconds`
    (5.13-Testaufbau) bleibt `outageSecondsLowerBound` fuer eine
    carry-forwarded Kette unbedingt `0` (Negativtest gegen mehrfach
    angewandte Kontrollpunktabstands-Kompensation); ein echter
    Phasenwechsel vor Zeitaufloesung loescht Anker und
    `priorBootPhaseElapsed` atomar, ohne bereits zuvor gesicherte
    `observedRunSeconds`/`priorBootPhaseElapsed`-Werte rueckwirkend zu
    veraendern; war `originalCheckpointUtc` bereits vor der gesamten Kette
    unbekannt, bleibt die Kette unabhaengig von Carry-Forward-Reboots
    unaufloesbar (Abgrenzung); `activateFallbackRecoveredRun` konstruiert
    den Carry-Forward-Fall identisch zu `activateLoadedRun`.
11. Reboot waehrend bereits persistiertem `RecoveryEvaluation`:
    Episode-Refresh statt Hop 1, `recoveryEpisodeRevision` erhoeht sich,
    `nominalRecoveryAdjustment` bleibt unveraendert, beide Aufloesungswege
    bleiben erreichbar.
12. `writeSnapshotCore`-Rollbackvertrag (5.16): alle Vor-Commit-Fehlerpfade
    stellen aus `LoadedActiveRun`, `FallbackRecoveryPending` und regulaerem
    `Ready` exakt den uebergebenen `rollbackState` wieder her;
    `Indeterminate` fuehrt unabhaengig vom Aufrufer zu
    `BlockedIndeterminate`; bestaetigter Commit mit RAM-Apply-Fehler fuehrt
    zu `PersistenceCommittedApplyFailed`; Standardpfade bleiben
    byte-identisch zum bisherigen Verhalten.
13. Fallback-Recovery-Fallback-Korrektheit (5.18): korrupter Current +
    gueltiger Fallback -> Recoverycommit -> neuer Current gueltig **und**
    alter gueltiger Fallback weiterhin ladbar; zweiter Zyklus; Slot-
    Distinctness-Guard; Cut vor/nach Prepared-/Head-Commit.
14. `state()` nach `FallbackRecovered`-Load ist `FallbackRecoveryPending`;
    kein Recovery-Write aus einem beliebigen `BlockedIndeterminate`.
15. Legacy-Unknown-Historie (5.21): Schema 1/2 -> mehrere Folds -> `basis`
    bleibt `PartialUnknownHistory`; Recoverykorrektur -> `basis` bleibt
    `PartialUnknownHistory`; neuer Schema-3-Run -> `basis == KnownTotal`.
16. `observedRunSeconds` strikt getrennt (5.22): bleibt bei jeder
    automatischen und manuellen Ausfallkorrektur unveraendert; automatische
    UTC-Reevaluation erzeugt keine Aenderung an `NominalRecoveryAdjustmentState`;
    `ApplyRecoveryTimeCorrection` innerhalb der akkumulierten Grenzen
    erfolgreich und treibt tatsaechlich den Fermentationsabschluss;
    ausserhalb -> Ablehnung ohne Saturierung; ohne bekannte akkumulierte
    Obergrenze -> Ablehnung (gemeinsamer Bounds-Grundsatz mit
    `resolveRecoveryOutcome`, s. Punkt 19); zwei Episoden mit zwei
    Korrekturen -> kumulative Summe korrekt; Overflow-Schutz; getrennte
    Ausweisung in Anzeige/Export.
17. Echter, von Kontrollpunkt-Zeitpunkten unabhaengiger First-after-restart-
    Latch (5.20): Zwischenevidenz zwischen zwei Checkpoints wird erfasst;
    `Valid` ohne `filteredCelsius` verbraucht den Latch nicht; unmittelbarer
    Hop1+Hop2-Erfolg erhaelt korrektes First-after vor Ankerloeschung;
    `beforeOutage` nachweisbar Vor-Ausfall-Wert (nicht nach
    `lastKnown`-Aktualisierung kopiert); Episode-Refresh-Reset;
    `Fermenting`/`CoolHolding`/`WaitingForProduct` latchen ueber den
    Resume-Zeitpunkt hinaus weiter (`priorBootPhaseElapsed`-Tag traegt das
    Fenster, unabhaengig davon, ob der Anker zu diesem Zeitpunkt bereits
    `nullopt` ist oder wegen offener Zeitbewertung noch besteht); Reject
    (Fault) schliesst das Fenster sofort; echter Phasenwechsel schliesst
    das Fenster; Tombstone loescht das Feld.
18. `PriorBootPhaseElapsed` (5.23): Roundtrip je Phase; Phasenwechsel-Clear;
    `RecoveryReentryRequired`/`RecoveryResumed` loeschen weder
    `priorBootPhaseElapsed` noch `pendingRecoveryAnchor`/
    `recoveryBootAnchorMonotonicMillis`; neuer Run/`clearActiveRunState()`-
    Clear (alle fuenf Felder); Akkumulation ueber zwei Recovery-Episoden
    ohne Resume dazwischen (Unter- und Obergrenze); Akkumulation ueber zwei
    Recovery-Episoden **mit** zwischenzeitlichem erfolgreichem Resume,
    sowohl mit bereits geloeschtem als auch mit noch offenem Anker der
    ersten Episode (letzterer Fall: Carry-Forward, 5.12); eine Episode
    ohne bekannte Obergrenze macht die akkumulierte Obergrenze `nullopt`
    (nur dauerhaft, wenn niemals vor Beginn der Kette ein UTC-Anker bekannt
    war – ein carry-forwarded, urspruenglich bekannter Anker bleibt
    aufloesbar, 5.12); Tag-Mismatch ist strukturell ungueltig.
19. Benutzerpfad Fermenting/CoolHolding-Grenzueberschneidung (5.17):
    Hop 2/Resume erfolgt fuer beide Phasen unveraendert bei `Uncertain`
    (kein Hop-1-only); `resolveRecoveryOutcome`s Vorbedingung fuer diese
    beiden Phasen prueft `processState.state`+`priorBootPhaseElapsed`,
    unabhaengig vom (ggf. noch offenen) Anker-Zustand; **Bounds-Gate**:
    `AssumeThresholdCrossed` innerhalb `Uncertain` **mit** bekannter
    akkumulierter Obergrenze treibt ueber `resolveRecoveryOutcome`
    denselben Abschlusspfad wie die jeweilige automatische
    Dauerentscheidung (`completeTimedRun`/`completeHoldDuration`) und
    loescht einen lingernden Anker; **mit unbekannter** Obergrenze (auch
    innerhalb `Uncertain`) -> `NotAllowedInState`, sichere Regelung laeuft
    unveraendert weiter; ausserhalb `Uncertain` -> `NotAllowedInState`;
    `AssumeStillValid` fuer diese beiden Phasen ist kein anbietbarer Wert;
    Stale-/Episode-/CommandId-Schutz.
20. **`WaitingForProduct` + `AssumeStillValid` – echter Resume
    (Auftragspunkt 2, Detailtests unter 5.17):** fuehrt einen tatsaechlich
    fortsetzbaren Resume nach `WaitingForProduct` herbei (kein reiner
    `CommandId`-Verbrauch wie im frueheren wirkungslosen Pfad); Gate-A-Kopplung ueber den
    neuen `liveSensorEvidence`-Parameter (positiv -> Resume, negativ ->
    `RecoveryRejected`/`Fault`); `priorElapsedForOldPhase ==
    RecoveredPhaseElapsed.totalSecondsLowerBound` (nicht `totalSecondsUpperBound`,
    5.10-Reconciliation); Idempotenz derselben `commandId`; Stale-Episode
    -> `StaleState`; kein automatischer Uebergang zu `ReachingTarget`/
    `Fermenting`; Anker-Erhalt/-Loeschung nach diesem Resume folgt derselben
    `recoveryTimeResolvedAtResume`-Regel wie jeder andere Resume.
21. `RecoveryConfidence` (5.5): `Strong` ueber Alt-Boot-lokalen Beweis ohne
    UTC-Anker; `Strong` ueber UTC-Obergrenze; `Bounded` bei
    ueberschneidendem UTC-Intervall; `Unknown` ohne jeden UTC-Anker; alle
    drei Werte nachweisbar erreichbar.
22. `RunPersistenceMutationKind::Recovery` (5.16): Codec-Roundtrip; alle
    **fuenf** Recovery-Commit-Ausloeser (Hop 1, Hop 1+Hop 2, Episode-Refresh,
    Tombstone, erfolgreiche Zeit-Nachtragskorrektur) erzeugen
    `mutationKind == Recovery`, `commandId == std::nullopt`; eine
    erfolglose Zeit-Nachtragskorrektur erzeugt **keinen** Commit; Cut nach
    Prepared-Head-Schreiben eines Recovery-Commits durchlaeuft den
    bestehenden generischen `PreparedInterrupted`-Mechanismus.
23. Architekturgrenze (5.8): `activateLoadedRun`/`activateFallbackRecoveredRun`
    konstruieren den Anker korrekt aus dem jeweils zutreffenden internen
    `slots_`-Eintrag; `RunRecoveryCoordinator` hat keinen Zugriff auf einen
    `RunPersistenceRawRecord`.
24. `Completed`-Restore: bleibt `Completed` bis Quittierung, kein
    `InvalidDecision`, `stateEnteredAtMillis` im aktuellen Boot.
25. `StoreOutcomeUnknown` vs. bestaetigter Commit + RAM-Apply-Fehler
    weiterhin getrennt.
26. Schema-1/2/3-Current/Fallback-Matrix vollstaendig (Regression);
    `knownRunPersistenceSchema` akzeptiert `{1,2,3}`.
27. Alle bestehenden #20/#21-Sensor-/Sicherheitsregressionen bleiben gruen.
28. `git diff --check` **ungescoped** (bare, ohne Pfadangabe) fuer alle
    geaenderten Dateien (Plan **und** `docs/ROADMAP.md`).
29. Recovered `Fermenting` mit
    `priorBootPhaseElapsed.lowerBoundSeconds == 3600`, danach bestaetigte
    Restdauer `remainingDurationMinutes == 120`: `priorBootPhaseElapsed` und
    `nominalRecoveryAdjustment` sind nach dem Commit auf dem neuen Nullstand;
    nach 119 neuen Minuten erfolgt **kein** Abschluss, nach 120 neuen Minuten
    der bestehende Abschluss.
30. Derselbe Fall mit bereits gesetztem
    `nominalRecoveryAdjustment.cumulativeAppliedSeconds`: die alte nominale
    Korrektur verkuerzt die neu gesetzte Restdauer nicht; die neue
    Entscheidung zaehlt ausschliesslich ab dem Kommando-Baselinepunkt.
31. Restdaueranpassung waehrend noch offenem `pendingRecoveryAnchor` und
    gesetztem `recoveryBootAnchorMonotonicMillis`: beide werden im selben
    Commit geloescht; ein spaeterer `reevaluateRecoveryTime()`-Aufruf liefert
    keinen dauerwirksamen Pfad und kann die neue Restdauer nicht verkuerzen.
32. Reine Zieltemperaturaenderung waehrend `Fermenting`: keine Fold- oder
    Baseline-Mutation; `stateEnteredAtMillis`, Prior-/Nominalzeit und offener
    Recovery-Kontext bleiben vollstaendig erhalten, die Restdauer laeuft
    ununterbrochen weiter.
33. Kombinierte Zieltemperatur- und Restdaueranpassung waehrend `Fermenting`:
    die neue Zieltemperatur und die neue Restdauer werden gemeinsam
    angewandt, aber genau eine neue Restdauer-Baseline wird gesetzt; der
    Temperaturteil loest keinen zweiten Reset aus.
34. `remainingDurationMinutes == 0`: bestehende Semantik eines unmittelbar
    zulaessigen Fermenting-Abschlusses bleibt trotz neuer Baseline erhalten.
35. Neuer Stromausfall **nach** manueller Restdauer-Baseline: Hop 1 beginnt
    mit dem neuen `stateEnteredAtMillis`; `knownPhaseSecondsAtOriginalCheckpoint`
    enthaelt keine Vor-Baseline-Zeit; neue Carry-Forward-Ketten funktionieren
    normal und Vor-Baseline-Zeit wird nicht erneut in `priorBootPhaseElapsed`
    eingefaltet.
36. `observedRunSeconds`: die vor der Anpassung sicher beobachtete Zeit wird
    genau einmal gefaltet und ueber die Baseline hinweg historisch erhalten;
    sie wird weder geloescht noch als neuer Restdauer-Vorlauf verwendet.
37. Persistenz-Cutpoints der Restdaueranpassung: kein Commit -> weder neue
    Restdauer noch neuer Snapshot, Timer-Baseline, Fold oder geloeschter
    Recovery-Kontext ist teilweise sichtbar; `CommitOutcomeUnknown` bleibt
    fail-closed; bestaetigter Commit plus Apply-Fehler bleibt der bestehende
    `PersistenceCommittedApplyFailed`-Vertrag.
38. Alle Tests 1-28 dieser Matrix, insbesondere die Carry-Forward-/Mehrfach-
    Reboot-/Kein-doppeltes-Zaehlen-Tests, bleiben als Regression erhalten;
    die neuen Baseline-Tests veraendern deren Semantik fuer neue
    Recovery-Ketten nicht.

## 9. Safety-/Security-/Recovery-/Hardwaregrenzen

Keine Aktorfreigabe vor abgeschlossenem Hop 2 oder vor `Completed`-
Quittierung. Kein Schreiben vor vollstaendigem lokalem Kandidatenaufbau.
Kein Aktorpfad direkt aus `FallbackRecoveryPending` vor Hop 1. Kein
Recovery-Commit aus einem beliebigen `BlockedIndeterminate`. Keine
automatische Prozessentscheidung aus einer unbewiesenen Ausfall-
Untergrenze (5.13). Keine biologische oder "observed" Zeitgutschrift ohne
validiertes Modell bzw. ohne explizite, gesondert ausgewiesene
Benutzerentscheidung (5.22). Reale Hardware-/NVS-Anbindung bleibt #29/#90
vorbehalten. **Neu in dieser Revision (5.12/5.17):** eine noch offene
Recovery-Zeitbewertung sperrt niemals eine bereits erteilte Aktorfreigabe
erneut (Aktor-/Zeitfrage strukturell entkoppelt); eine qualitative
(`AssumeThresholdCrossed`) oder quantitative (`ApplyRecoveryTimeCorrection`)
Abschlussbestaetigung fuer `Fermenting`/`CoolHolding` erfolgt niemals ohne
eine bewiesene, endliche akkumulierte Obergrenze; ein Resume ueber
`resolveRecoveryOutcome` durchlaeuft dieselbe Gate-A-Sensorpruefung wie
jeder andere Resume. **Fuer die manuelle Restdauer-Baseline gilt zusaetzlich:**
der neue Timer darf nur nach einem bestaetigten atomaren `AdjustRun`-Commit
wirksam werden; alte Recovery-Zeit darf weder ueber
`priorBootPhaseElapsed`/`nominalRecoveryAdjustment` noch ueber einen spaeteren
NTP-Reevaluationspfad erneut angerechnet werden. Die historische
`observedRunSeconds`-Metrik bleibt erhalten und wird nie als unbewiesene
biologische Zeitgutschrift behandelt.

## 10. Ressourcen-/Betriebsbudget

Schema-3-Zuwachs gegenueber Schema 2: `PendingRecoveryAnchor` (~68-78 Byte,
optional, waehrend offener Episode **oder** waehrend
einer noch offenen Recovery-Zeitbewertung eines bereits resumten Laufs
belegt, s. 5.12; das Feld `accumulatedBeforeEpisode`
(`PriorBootPhaseElapsed`, uint32 + optional uint32, ~9 Byte) sowie das Feld
`knownSecondsSinceOriginalCheckpoint`
(`std::uint64_t`, 8 Byte, neu in dieser Revision fuer Carry-Forward-Ketten,
5.12) vergroessern das Struct-Layout um diese
Betraege; struct-intern kein zusaetzlicher Alignment-Zuschlag ueber die
uebliche 8-Byte-Ausrichtung von `std::uint64_t`/`std::optional<std::int64_t>`
hinaus),
`recoveryBootAnchorMonotonicMillis`
(9 Byte, optional), `RunProgressState` (5 Byte inkl. Basis-Tag),
`RecoveryTemperatureEvidence.lastKnown` (~30 Byte), `RecoveryEpisodeEvidence`
(optional, `beforeOutage` ~30 Byte + `firstAfterRestart` mit drei
optionalen ~15-Byte-Feldern, zusammen ~75-90 Byte, nur waehrend offener
Episode belegt), `NominalRecoveryAdjustmentState` (12 Byte, optional, groesser
als der vorherige 8-Byte-Einzelrecord wegen der kumulativen Buchfuehrung),
`recoveryEpisodeRevision` (4 Byte), `TaggedPriorBootPhaseElapsed` (~10 Byte,
optional, nur waehrend zeitbegrenzter Phase belegt). In Summe deutlich
unter dem bestehenden `kMaximumCheckpointRecordBytes`-Budget (8240 Byte)
bzw. `kMaximumRunPersistencePayloadBytes` (8192 Byte); im Rahmen der
Schema-3-Schreibtests (Testmatrix 24) abgedeckt – ein tatsaechliches
Ueberschreiten des Budgets wuerde dort als `CapacityExceeded` auffallen.
Die manuelle Restdauer-Baseline fuehrt kein zusaetzliches persistentes
Timerfeld und kein unbegrenztes Recoveryjournal ein: sie verwendet den
bestehenden `stateEnteredAtMillis`-Punkt sowie die vorhandenen
`priorBootPhaseElapsed`-/`nominalRecoveryAdjustment`-/Ankerfelder mit deren
beschriebenem Null-/Leerstand. Detaillierte historische Korrekturen bleiben
#19.

## 11. SOLID/DRY/KISS

`RecoveryOutageBounds`/`RecoveredPhaseElapsed` sind zwei kleine, einzeln
testbare, nicht ueberladene Typen statt eines vermischten Objekts;
`RecoveredPhaseElapsedInput` nimmt bewusst nur einen bereits fertig
abgeleiteten Sekundenwert entgegen, wodurch eine Boot-uebergreifende
Subtraktion strukturell unmoeglich wird. `elapsedWithPrior` ist eine
einzige neue Vergleichsfunktion, an drei bestehenden Stellen sowie am
eingebauten `WaitingForProduct`-Check wiederverwendet. `writeSnapshotCore`
ist eine einzige Extraktion, von allen Schreibpfaden gemeinsam genutzt, mit
exakt einem zusaetzlichen Parameter (`rollbackState`) statt einer
impliziten, neu-berechneten Vorbedingung. `FallbackRecoveryPending` ist ein
eigenstaendiger, disjunkter Zustand statt eines versteckten Grundes.
`WeightedProgressStatus` entfaellt vollstaendig statt eines
Ein-Wert-Enums; `RecoveryConfidence` (5.5) wird an demselben Massstab
(jeder Wert tatsaechlich erreichbar) geprueft. Der phasenuebergreifende
`resolveRecoveryOutcome`-Vertrag (5.17) ist eine einzige, an drei Phasen
wiederverwendete Entscheidungs-/Stale-/Dedup-Infrastruktur statt dreier
separater Mechanismen; `completeTimedRun`/`completeHoldDuration` werden
dabei jeweils direkt wiederverwendet statt eines zweiten
Abschluss-Entscheidungspfads. `NominalRecoveryAdjustmentState` und
`priorBootPhaseElapsed` (5.23) teilen sich dieselbe kumulative
Obergrenzen-Buchfuehrung statt einer zweiten, parallelen.
`applyLiveRecoveryEvidence` (5.20) ist eine einzige, reine
Mutatorfunktion, sowohl von den bestehenden Schreibpfaden als auch
unabhaengig davon aufrufbar – keine zweite Sensorpipeline.
`recoveryTimeResolvedAtResume` (5.12) ist eine einzige, kleine
Praedikatsfunktion, an jeder Resume-Stelle (Hop 2 automatisch,
`resolveRecoveryOutcome` + `AssumeStillValid`) identisch verwendet, statt
eines an jeder Stelle erneut zu treffenden Sonderfalls.
`deriveEffectiveAnchorTimeBasis` (5.12, neu in dieser Revision) ist eine
einzige, reine Ableitung, sowohl von Hop 1s eigener Verdikt-/Bounds-Auswertung
als auch von `reevaluateRecoveryTime` identisch verwendet – ohne sie
muesste die Carry-Forward-Verschiebung (`originalCheckpointUtc`/
`knownPhaseSecondsAtOriginalCheckpoint` gegen `knownSecondsSinceOriginalCheckpoint`)
an beiden Stellen getrennt und potenziell abweichend implementiert werden.
Die frisch/Carry-Forward-Fallunterscheidung selbst ist identisch fuer
`activateLoadedRun` und `activateFallbackRecoveredRun` spezifiziert (5.8),
keine zweite, abweichende Ankerkonstruktionsregel je Aktivierungspfad. Die
Gate-A-Kopplung (5.10/5.26) wird fuer den neuen `AssumeStillValid`-Resume
wiederverwendet, statt eine zweite Sensorpruefung zu erfinden. Der
Bounds-Grundsatz fuer eine bewiesene, endliche Obergrenze (5.17/5.22) ist
eine einzige logische Bedingung
(`priorBootPhaseElapsed->elapsed.upperBoundSeconds.has_value()`), von
`resolveRecoveryOutcome` und `ApplyRecoveryTimeCorrection` identisch
gelesen statt zweimal getrennt implementiert. Die manuelle
Restdauer-Neubaseline verwendet keine zweite Timerwahrheit: der bestehende
`stateEnteredAtMillis`-Wert wird einmal auf den Kommandozeitpunkt gesetzt,
der bestehende prior-/nominale Recoverystand auf `0/0` beziehungsweise leer
gebracht, und `observedRunSeconds` bleibt als einzige historische
Beobachtungsmetrik erhalten. `reevaluateRecoveryTime` kann nur einen noch
vorhandenen Anker lesen und ist damit strukturell von der neuen Baseline
entkoppelt.

## 12. Dokumentations-/Abschlussnachweis

- `docs/ROADMAP.md`: **in diesem Commit korrigiert** (5.29) – Zeile 13
  (Statuszeile) und Zeile 3 (`Stand:`), s. dort fuer den Befund; kein
  weiterer Aenderungsbedarf ueber diese beiden Zeilen hinaus.
- `docs/RUN_PERSISTENCE.md`/`docs/RECOVERY_AND_INTERRUPTION.md`: werden im
  Umsetzungscommit (Nr. 11) um die in 5.2-5.24 vertraglich fixierten
  Punkte ergaenzt, insbesondere die getrennte, kumulative Ausweisung von
  `observedRunSeconds` und der nominalen Ausfallzeitkorrektur (5.22), die
  ehrliche Legacy-Historie (5.21) und den Konfidenzvertrag (5.5).
- `git diff --check`: nach Commit **ungescoped (bare), fuer alle
  geaenderten Dateien** (Plan **und** `docs/ROADMAP.md`) ausgefuehrt und im
  SESSION-HANDOVER-Kommentar mit dem tatsaechlichen Befehlsergebnis
  dokumentiert (nicht nur als geplanter Schritt).
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

Revision 10 ist ein vollstaendiger, eigenstaendiger Plan. Nach Commit dieser
Datei **und** `docs/ROADMAP.md`: **anhalten**, `git diff --check`
**ungescoped (bare)** fuer beide geaenderten Dateien ausfuehren, `git push`,
Remote-SHA verifizieren (Abschnitt 12), PR-Beschreibung und SESSION
HANDOVER aktualisieren. Keine Implementierung. Kein `Ready for review`.
Keine Remote-CI. Kein Merge. Keine Branchloeschung.
