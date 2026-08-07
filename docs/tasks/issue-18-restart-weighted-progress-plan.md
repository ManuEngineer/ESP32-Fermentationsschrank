# Plan: Issue #18 – Wiederanlauf und temperaturgewichteter Fortschritt

## 1. Metadaten und Status

| Feld | Wert |
|---|---|
| Issue | [#18](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/18) – `[E2.3] Wiederanlauf und temperaturgewichteten Fortschritt implementieren` |
| Epic | [#4](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/4) – Konfiguration, Persistenz und Wiederanlauf |
| Branch | `plan/issue-18-restart-weighted-progress` |
| Basis | `main` @ `082fb3f` (Merge PR #99 / Issue #21) |
| Status dieses Dokuments | Revision 1 – zur Ownerfreigabe vorgelegt |
| Vorherige Plaene zu #18 | keiner; erste Planrevision |

## 2. Live-Issue- und Abhaengigkeitsabgleich

Vor Planbeginn geprueft (2026-08-07):

- Issue #21 (`PR #99`) ist gemergt (`082fb3f`), Issue #21 ist `CLOSED`.
- Abhaengigkeiten von #18 laut Issue: #10, #14, #17, #20 – alle vier `CLOSED`.
- `docs/ROADMAP.md` nannte vor diesem Plan "#22 nach #21" als naechste
  fachliche Arbeit. Der Owner-Auftrag zu diesem Plan weist stattdessen #18
  an; die Roadmap wurde dazu bereits vor diesem Plan-Commit aktualisiert
  (materielle Reihenfolgeaenderung, dokumentiert statt still ersetzt). #22
  bleibt unveraendert die naechste Arbeit innerhalb Epic E3.
- Issue #18 nannte bislang **nicht** #21 als Abhaengigkeit, obwohl
  `docs/RUN_PERSISTENCE.md` (aus #21) einen expliziten Uebergabeabschnitt an
  #18 enthaelt und der Code (`run_persistence_contract.cpp:270–277`,
  Kommentar) #18 namentlich als Konsument von
  `computeRestartSensorSelection` benennt. Die fehlende Abhaengigkeit wurde
  vor diesem Plan-Commit minimal korrigiert (siehe Issue #18, Abschnitt
  "Abhaengigkeiten" und "Scope").
- Kein offener PR referenziert Issue #18 (`gh pr list --search 18` liefert
  keinen Treffer auf dieses Issue).

## 3. Verbindliche Quellen und Lesematrix

Gemaess `AGENTS.md`-Lesematrix (Planung, Safety/Recovery/Persistenz, Module):

- `docs/AGENT_WORKFLOW.md`, `docs/ENGINEERING_PRINCIPLES.md` (Planungsprozess)
- `docs/RECOVERY_AND_INTERRUPTION.md` (Issue-Quelle, Kernvertrag)
- `docs/RUN_PERSISTENCE.md` (Issue-Quelle, insbesondere Abschnitt
  "Wiederanlaufreihenfolge" und "Uebergabe an ein spaeteres Vorhaben:
  Regelsensorauswahl bei Reaktivierung")
- `docs/STATE_MACHINE.md` (Issue-Quelle, Abschnitte `BOOT`, `SAFE_BOOT`,
  `RECOVERY_EVALUATION`, `RECOVERY_TIME_PENDING`, phasenspezifische
  Wiederanlaufhinweise)
- `docs/DECISIONS.md` (ADR-013 Modulgrenzen, ADR-014 deterministischer
  Zustandsautomat, ADR-008 Ressourcenbudget)
- `docs/CI_AND_QUALITY_GATES.md` (Testbefehle, Buildprofile)
- lokale `lib/fermentation_app/AGENTS.md`, `lib/device_platform/AGENTS.md`

Nicht zusaetzlich gelesen: UI-, Web- und Netzwerkdokumente (nicht direkt
referenziert und ausserhalb des Scopes, siehe Abschnitt 4).

## 4. Ziel und Nicht-Ziele

### Ziel

Ein nach vollstaendigem Spannungsverlust gestartetes Geraet mit einem
persistierten aktiven Lauf durchlaeuft nach `BOOT` einen produktiven,
getesteten Wiederanlaufpfad, der:

1. den gespeicherten Laufzustand ueber `RunPersistenceCoordinator` laedt und
   klassifiziert (kein Lauf, `COMPLETED`, unterbrochener aktiver Lauf);
2. fuer einen unterbrochenen aktiven Lauf `RECOVERY_EVALUATION` durchlaeuft,
   phasenbezogen die sichere Wiederanlaufaktion nach
   `docs/RECOVERY_AND_INTERRUPTION.md` bestimmt und dabei nie auf verfuegbare
   NTP-Zeit wartet;
3. den in Issue #21 vorbereiteten, bislang unangewendeten
   Sensorselektions-Reaktivierungszustand (`RestartRevalidationPending`)
   auswertet, endgueltig setzt und **vor** jeder Reglerbewertung fuer diesen
   Lauf persistiert;
4. ein konservatives temperaturgewichtetes Fortschrittsmodell fuehrt, das
   Ausfallzeit als Ober-/Untergrenzen-Intervall behandelt statt als exakten
   Wert, und Korrekturen erst nach verlaesslicher UTC-Zeit nachtraegt, ohne
   den Lauf zu blockieren;
5. bei mehrdeutigem Ergebnis (Intervall ueberschneidet eine Phasen- oder
   Haltezeitgrenze) keinen automatischen Abschluss ausloest, sondern eine
   sichtbare Entscheidungsanforderung erzeugt;
6. jede Wiederanlaufentscheidung, jede Sensorselektions-Reaktivierung und
   jede spaetere Zeitkorrektur als atomare Laufrevision persistiert, bevor
   Aktoren freigegeben werden.

### Nicht-Ziele

- **Kein NTP-Port/-Adapter.** `device_platform::ITimeSource::unixTimeSeconds()`
  existiert bereits als abstrakter Port; `VirtualTimeSource::setUnixTimeSeconds`
  macht "spaetere Korrektur durch absolute Zeit" bereits nativ testbar. Der
  produktive `EspTimerTimeSource` liefert bewusst weiterhin `std::nullopt`
  (unveraendert). Ein echter NTP-Sync-Mechanismus waere eine vorsorgliche
  Netzwerkfunktion ausserhalb des Issue-Scopes und wird hier nicht gebaut.
- **Kein kalibriertes thermisches Aktivitaets-Kennfeld.** Laut
  `docs/RUN_PERSISTENCE.md` ("Fortschrittsmodell") wird "keine biologische
  Aktivitaetskurve ... ohne praktische Grundlage erfunden" und "die
  Gewichtung wird bei der Inbetriebnahme kalibriert". Release 1 behandelt ein
  unsicheres Ausfallintervall deshalb konservativ als **nicht angerechnete**
  Zeit (Fortschritt pausiert waehrend des unsicheren Intervalls); ein echtes
  Temperatur-Aktivitaetsmodell bleibt `TBD_COMMISSIONING` und ausserhalb
  dieses Plans. "Temperaturgewichtet" bedeutet hier: Produktwert hat Vorrang
  vor Luftwert bei der Bewertung der *bekannten* laufenden Zeit; es bedeutet
  nicht, dass eine Ausfallluecke mit einer erfundenen Kurve gefuellt wird.
- **Kein neues Nachrichten-/Journalsystem.** `RuntimeMessage`,
  `MessageClass::DecisionRequired`/`Recovery`, `RunCommandState::messages`
  und `highestPriorityActiveMessage()` existieren bereits
  (`run_commands.hpp`) und werden fuer die Entscheidungsanforderung
  wiederverwendet. Persistente Meldungshistorie über einen Neustart hinweg
  (`docs/RUN_PERSISTENCE.md`, "Meldungen nach Neustart") ist Issue #19 und
  wird hier **nicht** vorgebaut; #18 erzeugt Meldungen nur fuer den laufenden
  Wiederanlauf, ohne eine Historienpersistenz einzufuehren.
- **Keine UI-/Web-Darstellung.** Die in "Anzeige und Export" geforderten
  Werte werden als Datenfelder auf dem bestehenden Diagnose-/Exportpfad
  bereitgestellt; ein UI-Rendering ist nicht Teil dieses Plans.
- **Keine Aenderung an der Fehlerklassen-/Verriegelungslogik aus Issue #24**
  (`SAFE_BOOT`, Bootschleifenzaehler, kritischer Persistenzfehler-Latch).
  Deren bereits dokumentiertes Verhalten (`docs/RUN_PERSISTENCE.md`,
  "Kritischer Persistenzfehler") wird als gegeben vorausgesetzt und nur
  konsumiert (Latch gesetzt -> `SAFE_BOOT`, vor jeder Recovery-Logik).
- **`computeRestartSensorSelection` wird nicht ohne Ownerentscheidung
  erweitert** (siehe Abschnitt 13, Gate A).

## 5. Befund des aktuellen Codes

Vollstaendig vorhanden und wiederzuverwenden:

- `RunPersistenceCoordinator` (`lib/fermentation_app/src/run_persistence_coordinator.{hpp,cpp}`)
  inkl. `loadAndInitialize()`, `persistCommand`/`persistTransition`/
  `persistSensorSelection`/`checkpointPeriodic`. **Kein produktiver
  Aufrufer** – `loadAndInitialize()` wird ausschliesslich aus
  `test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp`
  aufgerufen.
- `process_state_machine.{hpp,cpp}`: `ProcessState::RecoveryEvaluation`
  existiert bereits; `ProcessEvent::BootRecoverRun`, `RecoveryResume`,
  `RecoveryReject`, `BootRestoreCompleted` sind bereits verdrahtet
  (`Boot -> RecoveryEvaluation` bei `BootRecoverRun`; Ausgang bei
  `RecoveryResume`/`RecoveryReject`). **Kein produktiver Treiber** dieser
  Events. `RECOVERY_TIME_PENDING` existiert **nicht** als `ProcessState` –
  `docs/STATE_MACHINE.md` beschreibt es als "Kontext ... setzen" zusaetzlich
  zu einem wieder aufgenommenen normalen Zustand, nicht als eigenen
  Zustandswert.
- `sensor_selection.{hpp,cpp}` (Issue #21): `SensorSelectionRuntimeState`,
  `PersistedSensorSelectionState`, `computeRestartSensorSelection(...)`.
  **Befund (siehe Abschnitt 13, Gate A):** Die Funktion ignoriert aktuell
  ihre Eingaben `persisted` und `program` (`static_cast<void>(...)` in
  `sensor_selection.cpp:895–896`) und liefert unabhaengig vom Input immer
  `RestartRevalidationPending`/`Blocked`/`lastActiveMode`. Der
  Header-Kommentar (`sensor_selection.hpp:92–96`) beschreibt sie dagegen als
  Funktion, die "aus dem persistierten ... Zustand und dem ... Programmkontext
  eine Empfehlung" berechnet. Die drei zugehoerigen Tests
  (`test_sensor_selection.cpp:1193–1250`) heissen bereits
  `..._is_fail_closed_for_...` und pruefen ausschliesslich das
  eingabeunabhaengige Fail-Closed-Ergebnis.
- `run_persistence_contract.cpp:270–277`: Restore setzt
  `sensorSelectionRuntime` bereits fail-closed
  (`RestartRevalidationPending`/`Blocked`) und benennt #18 im Kommentar
  namentlich als Konsument von `computeRestartSensorSelection`.
- `device_platform::ITimeSource`/`VirtualTimeSource`
  (`lib/device_platform/src/time_source.hpp`,
  `virtual_time_source.{hpp,cpp}`) – solide, deterministische Testbasis fuer
  `monotonicMillis()` und optionales `unixTimeSeconds()`.
- `RuntimeMessage`/`MessageClass`/`MessageCode` inkl. `RecoveryPending`,
  `DecisionRequired` (`run_commands.hpp:211–261`,
  `RunCommandState::messages`, `highestPriorityActiveMessage()`).

Vollstaendig neu zu bauen (Gegenstand dieses Plans):

- Ein produktiver Boot-/Wiederanlauf-Koordinator, der `loadAndInitialize()`
  aufruft, klassifiziert und mit der Zustandsmaschine verdrahtet.
- Das Anwenden von `computeRestartSensorSelection` inkl. atomarer
  Persistierung des Ergebnisses vor Reglerfreigabe.
- `RECOVERY_TIME_PENDING` als Kontextflag im Prozess-Laufzeitzustand.
- Das Ausfallintervall-Modell (Ober-/Untergrenze, Konfidenz) als reine
  Funktion.
- Das konservative Fortschrittsmodell inkl. Wireformat-Erweiterung
  (Schema 2 -> 3).
- Phasenbezogene Wiederanlaufregeln je `ProcessState`.
- Die spaetere Korrektur nach Eintreffen einer UTC-Zeit.

`fermentation_application.{hpp,cpp}` ist mit 23 Zeilen `.cpp` der
vorgesehene, aber bislang leere Andockpunkt
("Die Fermentationslogik wird issueweise in diesem Modul implementiert.").

## 6. Fachvertraege

### 6.1 Modul- und Dateizuordnung (ADR-013)

Alle neuen Typen und Funktionen liegen in `lib/fermentation_app/src/` und
haengen ausschliesslich von `device_platform`-Ports
(`ITimeSource`, `IStateStore` ueber den bestehenden `RunPersistenceCoordinator`)
sowie von bereits vorhandenen `fermentation_app`-Modulen ab. Keine
Abhaengigkeit auf `device_platform_esp_idf` oder
`device_platform_test_support` (letzteres bleibt ausschliesslich in `test/`).

Neue Dateien:

- `run_recovery_interval.hpp` / `.cpp` – reine Funktionen fuer
  Ausfallintervall (Ober-/Untergrenze, Konfidenz) nach
  `docs/RUN_PERSISTENCE.md`, Abschnitt "Zeitanker und Ausfallintervall".
- `run_progress.hpp` / `.cpp` – Progress-Datentypen und reine
  Korrekturfunktion nach Abschnitt "Fortschrittsmodell".
- `run_recovery_coordinator.hpp` / `.cpp` – der produktive
  Boot-/Wiederanlauf-Koordinator (Abschnitt 6.4).

Geaenderte Dateien: `run_persistence_contract.{hpp,cpp}`,
`run_persistence_codec.cpp`, `run_persistence_coordinator.{hpp,cpp}`,
`process_state_machine.{hpp,cpp}`, `sensor_selection.{hpp,cpp}` (nur bei
Gate A / Option B, siehe Abschnitt 13), `fermentation_application.{hpp,cpp}`.

### 6.2 Ausfallintervall (reine Funktion, kein neuer Zustand)

Nach `docs/RUN_PERSISTENCE.md`, Abschnitte "Zeitanker und Ausfallintervall"
und "Unterbrechungsdauer ist ein Intervall":

```cpp
struct RecoveryIntervalInput {
    std::uint64_t currentMonotonicMillis;
    std::optional<std::int64_t> currentUnixSeconds;      // ITimeSource::unixTimeSeconds()
    std::optional<std::int64_t> lastReliableCheckpointUnixSeconds;
    std::uint64_t lastReliableCheckpointMonotonicMillis;
    std::uint16_t configuredCheckpointIntervalMinutes;
    std::optional<std::uint64_t> lastEventDrivenSaveMonotonicMillis;
};

enum class RecoveryIntervalConfidence : std::uint8_t {
    Unknown,     // keine verlaessliche UTC-Zeit verfuegbar
    Bounded,     // Ober-/Untergrenze vorhanden, aber weit auseinander
    Reliable,    // beide Grenzen fuehren zur gleichen Entscheidung
};

struct RecoveryIntervalResult {
    RecoveryIntervalConfidence confidence{RecoveryIntervalConfidence::Unknown};
    std::optional<std::uint64_t> lowerBoundSeconds;
    std::optional<std::uint64_t> upperBoundSeconds;
};

[[nodiscard]] RecoveryIntervalResult computeRecoveryInterval(
    const RecoveryIntervalInput& input);
```

`computeRecoveryInterval` ist seiteneffektfrei, speichert nichts und trifft
keine Phasenentscheidung. Fehlt `currentUnixSeconds` oder
`lastReliableCheckpointUnixSeconds`, ist das Ergebnis `Unknown` mit leeren
Grenzen – niemals eine geratene Zeit. Formel wie dokumentiert:
`obere Grenze = currentUnix - lastReliableCheckpointUnixSeconds`,
`untere Grenze = max(0, obere Grenze - maximal moeglicher
Kontrollpunktabstand)`, wobei der maximale Abstand aus
`configuredCheckpointIntervalMinutes` und optional
`lastEventDrivenSaveMonotonicMillis` abgeleitet wird.

### 6.3 Konservatives Fortschrittsmodell

Neue, im Wireformat persistierte Struktur (Schema 3, siehe 6.6):

```cpp
enum class RunTimeQualityStatus : std::uint8_t {
    NoReliableTime,      // vor erstem UTC-Abgleich dieses Laufs
    RecoveryTimePending, // Wiederanlauf erfolgt, Intervall noch nicht ausgewertet
    Evaluated,           // Intervall/Korrektur wurde nach verlaesslicher Zeit angewendet
};

struct RunProgressState {
    std::uint64_t cumulativeWeightedProgressSeconds{0U};
    std::uint64_t appliedCorrectionSeconds{0U};   // Summe bereits angewandter Korrekturen
    std::optional<std::int64_t> lastReliableUtcAnchorSeconds;
    std::uint64_t monotonicMillisAtLastReliableAnchor{0U};
    RunTimeQualityStatus timeQuality{RunTimeQualityStatus::NoReliableTime};
};
```

`cumulativeWeightedProgressSeconds` waechst ausschliesslich waehrend
tatsaechlich beobachteter, monoton gemessener Laufzeit (Produktwert hat
Vorrang, sonst Luftwert, jeweils nur bei gueltigem Sensor – siehe
`sensor_quality_pipeline`). Eine unsichere Ausfalllaufzeit (Intervall mit
`confidence != Reliable`) traegt **nicht** automatisch zum Fortschritt bei;
`appliedCorrectionSeconds` haelt fest, welcher Anteil eines spaeter
aufgeloesten Intervalls nachtraeglich als Fortschritt gutgeschrieben wurde,
damit dieselbe Ausfallzeit nicht doppelt angerechnet wird.

```cpp
struct RunProgressCorrectionInput {
    RunProgressState current;
    RecoveryIntervalResult interval;
    std::uint32_t nominalFermentationDurationMinutes; // aus ProcessRunSnapshot
};

struct RunProgressCorrectionResult {
    RunProgressState updated;
    bool crossesPhaseOrHoldBoundary{false}; // -> DecisionRequired, kein Autoabschluss
};

[[nodiscard]] RunProgressCorrectionResult computeProgressCorrection(
    const RunProgressCorrectionInput& input);
```

Bei `confidence == Unknown`: keine Korrektur, `timeQuality` bleibt/wird
`RecoveryTimePending`. Bei `confidence == Reliable`: konservativer Zuwachs
um die untere Intervallgrenze (nie die obere – "konservativer" im Sinn von
`docs/RECOVERY_AND_INTERRUPTION.md`, "darf die konservativere Variante
automatisch angewendet werden"), `timeQuality = Evaluated`. Bei
`confidence == Bounded` und das Intervall ueberschneidet eine Phasen- oder
Haltezeitgrenze: keine automatische Korrektur,
`crossesPhaseOrHoldBoundary = true`.

### 6.4 Boot-/Wiederanlauf-Koordinator

`RunRecoveryCoordinator` orchestriert exakt die in
`docs/RUN_PERSISTENCE.md` ("Wiederanlaufreihenfolge") und
`docs/STATE_MACHINE.md` (`BOOT`, `RECOVERY_EVALUATION`) bereits
akzeptierte Reihenfolge – hier nur software-technisch verdrahtet, nicht neu
entworfen:

```text
1. RunPersistenceCoordinator::loadAndInitialize() aufrufen
   (Aktoren bleiben bis hierher unbedingt AUS – Kontrakt bereits durch
   den Coordinator und ADR-014 sichergestellt).
2. Ladeergebnis klassifizieren:
   - kein aktiver Lauf, kein COMPLETED -> ProcessEvent::BootReady -> STANDBY
   - COMPLETED persistiert                -> ProcessEvent::BootRestoreCompleted -> COMPLETED
   - geladener aktiver Lauf                -> ProcessEvent::BootRecoverRun -> RECOVERY_EVALUATION
   - persistierte Sperre / unvollstaendige Transaktion / Speicherfehler
     -> bereits durch RunPersistenceCoordinator/#24-Vertrag als SAFE_BOOT-
     Ausloeser behandelt; dieser Plan aendert daran nichts, konsumiert das
     Ergebnis nur (siehe Nicht-Ziele).
3. Nur fuer den RECOVERY_EVALUATION-Zweig, in dieser Reihenfolge:
   a. Sensorselektions-Reaktivierung anwenden (6.5) und persistieren.
   b. Phasenbezogene Wiederanlaufaktion bestimmen (6.7).
   c. Ausfallintervall bewerten (6.2); bei Unknown -> RECOVERY_TIME_PENDING-
      Kontext setzen (6.6), aber sichere Aktion aus (b) trotzdem anwenden,
      sofern eindeutig zulaessig.
   d. Recoveryentscheidung (Zielzustand + RECOVERY_TIME_PENDING-Kontext +
      Sensorselektions-Reaktivierung) als eine atomare Revision persistieren.
   e. Regelung fuer diesen Lauf erst nach erfolgreichem Schritt d freigeben.
4. Netzwerk-/Zeitsynchronisation ist ausserhalb dieses Koordinators (Port
   liefert ohnehin nur std::nullopt bis eine spaetere Instanz
   unixTimeSeconds() aktualisiert) und blockiert Schritt 3 nicht.
```

`FermentationApplication::begin()` ruft `RunRecoveryCoordinator::run()` genau
einmal beim Start auf, bevor `update()` erstmals regelungswirksam wird.

### 6.5 Sensorselektions-Reaktivierung anwenden

Neue Funktion (Anwendungslogik, kein neuer Entscheidungskern – die
Entscheidung selbst bleibt ausschliesslich `computeRestartSensorSelection`,
Owner-Gate A in Abschnitt 13 betrifft nur deren Inhalt, nicht diese
Anwendung):

```cpp
// run_recovery_coordinator.hpp
[[nodiscard]] bool applySensorSelectionReactivation(
    RunCommandState& state, RunPersistenceCoordinator& persistence,
    std::uint64_t nowMonotonicMillis);
```

Ablauf: nur wirksam, wenn
`state.sensorSelectionRuntime.phase == SensorSelectionPhase::RestartRevalidationPending`
und ein aktiver Lauf vorliegt (sonst No-Op, Rueckgabe `true`). Liest
`state.sensorSelection` (persistiert, garantiert vorhanden fuer jeden
aktiven Lauf laut #21, 6.12) und den Programmkontext, ruft
`computeRestartSensorSelection(...)` auf, setzt
`state.sensorSelectionRuntime` auf das Empfehlungsergebnis, und persistiert
ueber den bestehenden `RunPersistenceCoordinator::persistSensorSelection(...)`-
Pfad (kein neuer Persistenzmechanismus – Wiederverwendung von Issue #21).
Die bestehende Schreibvoraussetzung ("ein aktiver Lauf verlangt zwingend ein
vorhandenes `sensorSelection`") bleibt unveraendert erfuellt, da
`sensorSelection` beim Laden bereits vorhanden ist und von jeder Folge-
Persistierung unveraendert mitgefuehrt wird (#21, 6.12).

Erst nach erfolgreicher Persistierung dieses Schritts wertet der Koordinator
(6.4, Schritt 3b) die Regelung fuer diesen Lauf ueberhaupt aus. Schlaegt die
Persistierung fehl, bleibt `SensorPeltierPermission::Blocked` bestehen und
der Lauf verbleibt in `RECOVERY_EVALUATION`.

### 6.6 `RECOVERY_TIME_PENDING` als Kontext

Kein neuer `ProcessState`-Wert (laut `docs/STATE_MACHINE.md` ist es ein
"Kontext", der zusaetzlich zu einem wieder aufgenommenen normalen Zustand
gesetzt wird). Erweiterung von `ProcessRuntimeState`
(`process_state_machine.hpp:52–59`):

```cpp
struct ProcessRuntimeState {
    // ... bestehende Felder unveraendert ...
    RunTimeQualityStatus recoveryTimeQuality{RunTimeQualityStatus::NoReliableTime};
};
```

`recoveryTimeQuality == RecoveryTimePending` zeigt den in
`docs/STATE_MACHINE.md` beschriebenen Kontext an, unabhaengig vom aktuellen
`state`-Wert (z. B. `Fermenting` mit `RecoveryTimePending`). Der Wert wird
Teil des checkpointfaehigen Zustands (`validateProcessRuntimeForCheckpoint`
wird um eine Invariante ergaenzt: `RecoveryTimePending` ist nur ausserhalb
`Boot`/`SafeBoot`/`Standby`/`Completed`/`Fault`/`ServiceMode` zulaessig).

### 6.7 Phasenbezogene Wiederanlaufregeln

Direkte Umsetzung von `docs/RECOVERY_AND_INTERRUPTION.md`, Abschnitt
"Phasenbezogene Wiederaufnahme", als eine Entscheidungsfunktion je
`ProcessState`-Gruppe (kein neuer Fachvertrag, nur Codeabbildung des bereits
akzeptierten Textes):

| `ProcessState` beim Ausfall | Wiederanlaufaktion |
|---|---|
| `Preheating`, `ReachingTarget` | Ziel erneut anfahren; noch nicht gestartete Fermentationszeit nicht anrechnen |
| `WaitingForProduct` | nur fortsetzen, wenn Wartezeit belastbar noch gueltig; sonst keine Fermentation starten (nicht raten) |
| `QualifyingTarget` | Ziel erneut erreichen; Qualifikation vollstaendig neu beginnen |
| `Fermenting` | sichere Regelung nach vollstaendiger Recoverypruefung neu ableiten; bei fehlender Zeit `RecoveryTimePending`; kein automatischer Abschluss aus Schaetzwert |
| `Cooling` | bei gueltigen Pflichtsensoren und ohne Sperre erneut ableiten; keine alte H-Bruecken-Richtung blind wiederherstellen |
| `CoolHolding` | Kuehlregelung erneut ableiten; Haltezeit nur aus belastbarer Zeitbewertung beenden |
| `ManualHolding` | Zieltemperatur nach Recoverypruefung weiter halten; Benutzer sichtbar informieren |
| `Completed` | keine Regelung neu starten; Ergebnis wiederherstellen; erst Quittierung fuehrt nach `Standby` |

Ist die Wartezeit in `WaitingForProduct` bei Boot nicht sicher entscheidbar
(fehlende belastbare Zeit), wird laut
`docs/RECOVERY_AND_INTERRUPTION.md` ("Maximale Wartezeit nach dem
Vorheizen") keine Fermentation automatisch gestartet; die Regelung bleibt
nur in einer eindeutig sicheren Warteaktion oder der Lauf wird beendet – die
konkrete Grenze dafuer ist `TBD_COMMISSIONING` (Wartezeit-Parameter) und
bleibt es; das Verhalten bei Unsicherheit selbst ist keine
Inbetriebnahmegroesse und wird hier implementiert.

### 6.8 Mehrdeutiges Intervall -> Entscheidungsanforderung

Kreuzt das Ausfallintervall eine Phasen- oder Haltezeitgrenze
(`RunProgressCorrectionResult::crossesPhaseOrHoldBoundary == true`), erzeugt
der Koordinator eine `RuntimeMessage` mit
`messageClass = MessageClass::DecisionRequired`,
`code = MessageCode::RecoveryPending`, `decisionRequired = true` in
`RunCommandState::messages` (bestehender Mechanismus, keine neue
Infrastruktur). Der Lauf setzt seine zuletzt eindeutig sichere Regelaktion
fort, ohne automatischen Abschluss, bis eine Benutzerentscheidung ueber den
bestehenden Kommandopfad eintrifft oder eine spaetere verlaessliche
UTC-Zeit die Mehrdeutigkeit aufloest.

### 6.9 Spaetere Korrektur nach verfuegbarer UTC-Zeit

Solange `recoveryTimeQuality == RecoveryTimePending`, prueft
`FermentationApplication::update()` bei jedem Zyklus, ob
`ITimeSource::unixTimeSeconds()` neu einen Wert liefert (vorher
`std::nullopt`). Sobald ja: `computeRecoveryInterval` und
`computeProgressCorrection` erneut ausfuehren, Ergebnis als neue atomare
Revision persistieren (`persistTransition` mit
`RunCheckpointTrigger::Transition`, wiederverwendet – kein neuer Trigger-
Wert noetig), `recoveryTimeQuality = Evaluated` setzen. Dies bildet
"Ausfallintervall und Fortschritt spaeter korrigieren -> korrigierten
Zustand atomar speichern" aus der Wiederanlaufreihenfolge ab.

## 7. Modul- und Abhaengigkeitsgrenzen

- Neue/geaenderte Dateien bleiben unter `lib/fermentation_app/src/`, keine
  Abhaengigkeit auf `device_platform_esp_idf` oder
  `device_platform_test_support`.
- `RunRecoveryCoordinator` haengt gegen `device_platform::ITimeSource` und
  den bestehenden `RunPersistenceCoordinator` ab – keine neuen Ports.
- `python scripts/check_architecture_boundaries.py` bleibt gruener Guard,
  keine Ausnahmeliste noetig.
- Keine neue Bibliothek, kein neues Buildprofil, keine ADR-Aenderung an
  ADR-013.

## 8. Voraussichtlicher Datei- und Commit-Schnitt

### Commit 1 – Ausfallintervall- und Fortschrittsdatenmodell (6.2, 6.3)

- neu: `run_recovery_interval.hpp/.cpp`, `run_progress.hpp/.cpp`
- `run_persistence_contract.hpp/.cpp`: `RunProgressState` in
  `RunPersistenceSnapshot` aufnehmen, `kCurrentRunPersistenceSchema` auf `3U`
- `run_persistence_codec.cpp`: verkettete Migration 2 -> 3 (fehlendes Feld
  wird auf `NoReliableTime`/Nullwerte abgebildet, nie als "0 Sekunden
  Fortschritt" fehlinterpretiert)
- Tests: `test/test_run_recovery_interval/`, `test/test_run_progress/`,
  Codec-/Migrationstests in `test/test_run_checkpoint_codec/`

### Commit 2 – `RECOVERY_TIME_PENDING`-Kontext und phasenbezogene Regeln (6.6, 6.7)

- `process_state_machine.hpp/.cpp`: `RunTimeQualityStatus`-Feld,
  Invariantenpruefung, neue reine Funktion fuer die 6.7-Tabelle
- Tests: `test/test_process_state_machine/`

### Commit 3 – Sensorselektions-Reaktivierung anwenden (6.5, Gate A)

- `run_recovery_coordinator.hpp/.cpp` (nur `applySensorSelectionReactivation`
  und deren direkte Abhaengigkeiten)
- Bei Ownerentscheidung Gate A = Option B zusaetzlich:
  `sensor_selection.cpp` (`computeRestartSensorSelection` differenzieren)
  und `test_sensor_selection.cpp:1193–1250` (Tests anpassen/erweitern) – als
  eigener, klar markierter Teilcommit 3b, der nur nach Ownerfreigabe von
  Option B umgesetzt wird.
- Tests: `test/test_run_recovery_coordinator/`

### Commit 4 – Boot-/Wiederanlauf-Koordinator (6.4)

- `run_recovery_coordinator.hpp/.cpp` vervollstaendigen (Laden,
  Klassifizieren, Verdrahtung mit `process_state_machine`-Events)
- `fermentation_application.hpp/.cpp`: Aufruf aus `begin()`
- Tests: `test/test_run_recovery_coordinator/` (Boot-Simulationen mit
  `VirtualTimeSource`/`SimulatedPersistentStateStore`)

### Commit 5 – Mehrdeutige Intervalle und spaetere Korrektur (6.8, 6.9)

- `run_recovery_coordinator.cpp`: `DecisionRequired`-Meldungserzeugung,
  Nachtrags-Korrekturpfad in `FermentationApplication::update()`
- Tests: `test/test_run_recovery_coordinator/`,
  `test/test_run_commands/` (Meldungserzeugung)

### Commit 6 – Anzeige-/Exportdatenvertrag, Dokumentation, Guards

- Diagnose-/Exportfelder fuer die in `docs/RUN_PERSISTENCE.md`
  ("Anzeige und Export") geforderten Werte (reine Datenprojektion, keine UI)
- `docs/RUN_PERSISTENCE.md`: Uebergabeabschnitt an #18 durch einen kurzen
  Abschlussvermerk ersetzen (Abschnitt bleibt als Herkunftsnachweis lesbar,
  wird aber nicht mehr als offen markiert)
- `docs/RECOVERY_AND_INTERRUPTION.md`, `docs/STATE_MACHINE.md`: Verweise auf
  "noch nicht implementiert" entfernen, sofern durch diesen Plan erledigt
- `python scripts/check_architecture_boundaries.py`,
  `python scripts/selftest_quality_gates.py`

Jeder Commit ist einzeln kompilier- und testbar; Commit 3b ist optional und
nur bei Gate-A-Option B Teil der Umsetzung.

## 9. Teststrategie und Testmatrix

### 9.1 Commit 1 – Intervall und Fortschritt

- `computeRecoveryInterval`: keine UTC -> `Unknown`; beide Grenzen gleich ->
  `Reliable`; grosse Luecke -> `Bounded`; Kontrollpunkt-Intervall-Grenzwerte
  (1/5/60 Minuten) je einmal.
- `computeProgressCorrection`: `Unknown` -> keine Aenderung; `Reliable` ->
  Zuwachs um Untergrenze, `appliedCorrectionSeconds` konsistent; `Bounded`
  mit Phasengrenzenkreuzung -> `crossesPhaseOrHoldBoundary=true`, keine
  Aenderung an `cumulativeWeightedProgressSeconds`.
- Codec: Schema-2-Bestand ohne `RunProgressState` deckodiert auf
  `NoReliableTime`/Nullwerte, nie als abgeschlossenen Fortschritt
  fehlinterpretiert; Round-Trip Schema 3.

### 9.2 Commit 2 – Kontext und Phasenregeln

- Fuer jede der neun Tabellenzeilen aus 6.7: ein Test mit kurzer, einer mit
  mittlerer, einer mit langer (grenzueberschreitender) Unterbrechung –
  entspricht der im Issue geforderten Testmatrix
  ("Kurze, mittlere und lange Unterbrechung in jeder wesentlichen Phase").
- `WaitingForProduct`: Wartezeit sicher noch gueltig / sicher abgelaufen /
  nicht sicher entscheidbar (drei Faelle, letzter -> keine Fermentation).
- Invariante: `RecoveryTimePending` nur in zulaessigen Zustaenden.

### 9.3 Commit 3 – Sensorselektions-Reaktivierung

- Reaktivierung wird nur bei `RestartRevalidationPending` wirksam (No-Op
  sonst).
- Persistierung schlaegt fehl -> `Blocked` bleibt, kein Fortschritt zur
  Reglerfreigabe.
- Reihenfolge-Test: Reglerfreigabe ist erst nach erfolgreicher Persistierung
  der Reaktivierung technisch erreichbar (kein Pfad im Koordinator, der dies
  umgeht).
- Bei Gate-A-Option B: je Provenienzwert (`InitialSelection`,
  `FallbackActive`, `ReturnedToProduct`, `LegacyUnknown`) ein Test mit dem
  dann definierten differenzierten Ergebnis; bestehende drei
  Fail-Closed-Tests werden zu Regressionstests fuer den jeweils weiterhin
  fail-closed bleibenden Teilfall.

### 9.4 Commit 4 – Boot-Koordinator

- Kein Lauf -> `Standby`. `COMPLETED` persistiert -> direkte
  Wiederherstellung ohne Reglerstart. Aktiver Lauf -> `RecoveryEvaluation`
  -> phasenbezogener Zielzustand.
- Aktoren bleiben bis zur ersten erfolgreichen Persistierung der
  Recoveryentscheidung nachweislich AUS (Regressionstest gegen
  versehentliche Vorabfreigabe).
- Persistierte Sperre / unvollstaendige Transaktion -> Koordinator greift
  nicht ein, bestehendes `SAFE_BOOT`-Verhalten bleibt unveraendert
  (Abgrenzungstest, kein neues Verhalten).

### 9.5 Commit 5 – Mehrdeutigkeit und Nachkorrektur

- Grenzueberschreitendes Intervall erzeugt genau eine aktive
  `DecisionRequired`-Meldung, kein automatischer Abschluss.
- `unixTimeSeconds()` wechselt waehrend `update()`-Zyklen von `nullopt` zu
  einem Wert -> genau eine Korrekturrevision, `Evaluated` gesetzt.

### 9.6 Commit 6 – Guards und gezielte Ausfuehrung

```bash
pio test -e native --filter test_run_recovery_interval
pio test -e native --filter test_run_progress
pio test -e native --filter test_process_state_machine
pio test -e native --filter test_run_recovery_coordinator
pio test -e native --filter test_run_commands
pio test -e native --filter test_run_checkpoint_codec
pio test -e native --filter test_run_persistence_coordinator
pio test -e native --filter test_sensor_selection   # nur bei Gate-A-Option B
python scripts/check_architecture_boundaries.py
clang-format --dry-run --Werror <geaenderte Dateien>
```

Ein vollstaendiger Lauf (`pio run -e native`, `pio test -e native` ohne
Filter, `scripts/selftest_quality_gates.py`) erfolgt ausschliesslich nach
abgeschlossenem Review und auf ausdrueckliche Owner-Anweisung, wie in
`docs/CI_AND_QUALITY_GATES.md` festgelegt.

## 10. Safety-, Security-, Recovery- und Hardwaregrenzen

- Kein Schritt dieses Plans hebt die Grundregel auf, dass jeder Wiederanlauf
  mit ausgeschalteten Aktoren beginnt (ADR-014, `docs/STATE_MACHINE.md`
  `BOOT`).
- Die Reglerfreigabe fuer einen reaktivierten Lauf ist strukturell an eine
  erfolgreiche, vorherige Persistierung sowohl der Sensorselektions-
  Reaktivierung als auch der Recoveryentscheidung gebunden (6.4, 6.5) – kein
  Pfad darf beides umgehen.
- Kein automatischer Laufabschluss aus einem unsicheren Zeitintervall (6.3,
  6.8) – deckt Akzeptanzkriterium "unsichere Situationen werden nicht
  geraten" direkt ab.
- Fehlende NTP-Zeit beendet keinen Lauf (6.9 laesst die zuletzt eindeutig
  sichere Aktion laufen) – deckt das zweite Akzeptanzkriterium direkt ab.
- Keine Aenderung an `SAFE_BOOT`-, Bootschleifen- oder
  Latch-Verriegelungslogik; dieser Plan konsumiert deren Ergebnis nur vor
  Schritt 1 des Koordinators.
- Kein Zugriff auf ESP-IDF-, GPIO- oder Hardwaredetails; alles bleibt gegen
  `ITimeSource` und `RunPersistenceCoordinator` (der wiederum gegen
  `IStateStore` arbeitet) abstrahiert.

## 11. Ressourcen- und Betriebsbudget

- Schema-Bump 2 -> 3 vergroessert jeden persistierten Checkpoint um die
  `RunProgressState`-Felder (ein `uint64_t`, ein `uint64_t`, ein
  `optional<int64_t>`, ein `uint64_t`, ein `uint8_t`-Enum – ca. 30–35 Byte
  vor Kodierungsoverhead). `kMaximumRunPersistencePayloadBytes = 8192U`
  bleibt unangetastet ausreichend; keine Anpassung dieser Konstante in
  diesem Plan vorgesehen, wird aber in Commit 1 durch einen Test gegen die
  tatsaechliche kodierte Groesse abgesichert.
- Keine neue dynamische Allokation; `RunProgressState` ist ein reiner
  Werttyp ohne Container.
- Kein zusaetzlicher periodischer Schreibzyklus – die Nachkorrektur (6.9)
  nutzt den bestehenden `persistTransition`-Pfad und -Trigger.

## 12. SOLID-, DRY- und KISS-Bewertung des geplanten Diffs

- **SRP:** Intervallberechnung, Fortschrittskorrektur, Sensorselektions-
  Reaktivierung und Boot-Orchestrierung sind vier getrennte, einzeln
  testbare Einheiten statt einer monolithischen "Recovery"-Klasse.
- **DRY:** Keine zweite Persistenzmechanik – Reaktivierung und
  Nachkorrektur nutzen ausschliesslich bestehende
  `RunPersistenceCoordinator`-Methoden. Keine zweite Nachrichteninfrastruktur
  – `RuntimeMessage` wird wiederverwendet. Die 6.7-Tabelle ist eine direkte
  Codeabbildung des bereits akzeptierten Dokuments, keine Neuerfindung.
- **KISS:** Das Fortschrittsmodell verzichtet bewusst auf ein
  Aktivitaets-Kennfeld (Nicht-Ziel, Abschnitt 4) und bleibt auf die in den
  Dokumenten tatsaechlich verlangte konservative Intervallbehandlung
  beschraenkt.
- **OCP/DIP:** Alle neuen Funktionen sind reine Funktionen oder haengen nur
  gegen bestehende abstrakte Ports/Koordinatoren ab; keine neue Abhaengigkeit
  von einer konkreten Plattform.

## 13. Offene Ownerentscheidungen und Gates

**Gate A – `computeRestartSensorSelection`-Widerspruch (blockierend fuer
Commit 3-Umfang, nicht fuer Commit 1/2/4/5/6).**
`docs/RUN_PERSISTENCE.md` (aus #21) beschreibt die Funktion als
Empfehlung *aus* dem persistierten Zustand und Programmkontext; der
tatsaechliche Code (`sensor_selection.cpp:890–907`) ignoriert beide
Eingaben und liefert immer das fail-closed Ergebnis, was durch die
Testnamen (`..._is_fail_closed_for_...`) bestaetigt wird. Zwei moegliche
Antworten mit materiell unterschiedlichem Umfang:

- **Option A (Vorschlag):** Fail-closed ist die endgueltige fachliche
  Antwort fuer Release 1 – nach jedem Neustart erfordert *jede* Provenienz
  eine vollstaendige Neubewertung, unabhaengig von der letzten
  dokumentierten Ursache. `computeRestartSensorSelection` bleibt
  unveraendert; #18 baut nur die Anwendung (6.5). Die RUN_PERSISTENCE.md-
  Formulierung "eine Empfehlung ... berechnet" wird redaktionell auf die
  tatsaechliche (triviale, aber bewusste) Fail-Closed-Berechnung
  praezisiert.
- **Option B:** Die Empfehlung soll ueber Fail-Closed hinaus nach
  Provenienz differenzieren (z. B. `ReturnedToProduct` mit kurzer
  Unterbrechung anders behandeln als `FallbackActive`). Das erfordert
  Commit 3b: neue Fachregeln, Aenderung von
  `computeRestartSensorSelection`, Anpassung der drei bestehenden #21-Tests.

Ohne Ownerentscheidung wird nach Freigabe dieses Plans mit **Option A**
begonnen (kleinster, mit der vorhandenen Testsuite konsistenter Schnitt);
Commit 3b entfaellt dann ersatzlos. Eine spaetere Owner-Praeferenz fuer
Option B gilt als materielle Abweichung nach Abschnitt 6 des
Agent-Workflows und erfordert eine neue Planrevision.

**Gate B – Zustaendigkeit fuer den produktiven Boot-Koordinator.**
Kein anderes offenes Issue baut `RunRecoveryCoordinator` oder verdrahtet
`FermentationApplication::begin()` mit `loadAndInitialize()`; #17 lieferte
bewusst nur die Persistenzmechanik ohne produktiven Aufrufer. Dieser Plan
geht davon aus, dass dies zu #18 gehoert (das Issue heisst "Wiederanlauf...
implementieren" und ist ohne einen Aufrufer nicht erfuellbar). Falls der
Owner dies stattdessen einem vorgelagerten, noch zu erstellenden Issue
zuordnen will, entfallen Commit 4 sowie die Boot-Klassifizierung aus
Commit 3/5 aus diesem Plan und muessten dort neu geplant werden.

**Gate C – Konservatives Fortschrittsmodell ohne Aktivitaets-Kennfeld
(Bestaetigung).** Abschnitt 4 legt fest, dass Release 1 eine unsichere
Ausfallzeit grundsaetzlich nicht anrechnet (kein erfundenes
Aktivitaets-Kennfeld). Dies ist die einzige mit den Dokumenten konsistente
Lesart, wird aber ausdruecklich als Gate genannt, weil sie "konservative
temperaturgewichtete Fortschrittsbewertung" enger auslegt als eine
Kennfeld-basierte Interpretation.

## 14. Dokumentations- und Abschlussnachweise

- `docs/ROADMAP.md`: bereits vor diesem Plan-Commit aktualisiert (Abschnitt 2).
- Issue #18: Abhaengigkeit auf #21 und Scope-Praezisierung bereits vor
  diesem Plan-Commit vorgenommen (Abschnitt 2).
- `docs/RUN_PERSISTENCE.md`, `docs/RECOVERY_AND_INTERRUPTION.md`,
  `docs/STATE_MACHINE.md`: Aktualisierung in Commit 6, nach tatsaechlicher
  Umsetzung, nicht vorab.
- PR-Beschreibung nennt Planpfad, exakte Plan-Commit-SHA und die drei Gates
  aus Abschnitt 13.

## 15. Verbindliche Taskliste fuer die Umsetzung

1. [ ] Gate A/B/C-Antworten vom Owner einholen (oder Owner-Bestaetigung des
   Default-Pfads Option A/Gate B wie geplant).
2. [ ] Commit 1: Intervall- und Fortschrittsdatenmodell + Schema-3-Migration
   + Tests.
3. [ ] Commit 2: `RECOVERY_TIME_PENDING`-Kontext + Phasenregeln + Tests.
4. [ ] Commit 3 (+3b bei Gate-A-Option B): Sensorselektions-Reaktivierung +
   Tests.
5. [ ] Commit 4: Boot-/Wiederanlauf-Koordinator + Verdrahtung + Tests.
6. [ ] Commit 5: Mehrdeutigkeits-Meldung + Nachkorrektur + Tests.
7. [ ] Commit 6: Anzeige-/Exportfelder, Dokumentationsabschluss, Guards.
8. [ ] Gezielte Tests je Commit (Abschnitt 9.6-Befehle, jeweils passender
   Filter).
9. [ ] Vollstaendiges Review des Gesamtdiffs (Abschnitt 7 Agent-Workflow).
10. [ ] Vollstaendiger lokaler Lauf nur nach Review und auf Owner-Anweisung.
11. [ ] `SESSION HANDOVER` bei Sessionende mit offenem PR.

## 16. Stopbedingung

Nach Committen dieses Plans, Push des Branches und Anlegen des Draft-PR mit
Planpfad und exakter Plan-Commit-SHA wird angehalten. Keine Implementierung,
kein `Ready for review`, kein Merge, bis der Owner den exakten Plan-Commit
und die Gates aus Abschnitt 13 freigibt.
