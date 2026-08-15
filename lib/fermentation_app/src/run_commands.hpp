#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "process_state_machine.hpp"
#include "run_command_limits.hpp"
#include "run_recovery_types.hpp"
#include "run_snapshot.hpp"
#include "sensor_selection_types.hpp"
#include "control_context_types.hpp"
#include "fault_types.hpp"

namespace fermentation {

class SafetyFaultService;

// Vorwaertsdeklaration statt Include: `run_commands.hpp <->
// sensor_selection.hpp` ist per Plan Abschnitt 7 NICHT zulaessig.
// `decideApplySensorSelectionAction` braucht die externe Sensorevidenz dennoch
// als Parameter (weder `RunCommandState` noch `SensorSelectionCommandRequest`
// koennen sie tragen - #21, 6.14.3). Die vollstaendige Definition ist
// ausschliesslich in `run_commands.cpp` sichtbar, das `sensor_selection.hpp`
// regulaer einbindet.
struct CrossRolePlausibilityContext;

using CommandId = std::uint64_t;

enum class CommandSource : std::uint8_t {
    LocalDisplay,
    WebInterface,
};

struct CommandEnvelope {
    CommandId id{0U};
    CommandSource source{CommandSource::LocalDisplay};
    std::uint64_t monotonicMillis{0U};
    std::uint32_t expectedStateSequence{0U};
    std::optional<std::uint32_t> expectedRunRevision;
    std::optional<std::uint32_t> expectedMessageRevision;
    std::optional<std::uint32_t> expectedFaultRevision;
    bool confirmed{false};
    std::optional<std::uint32_t> expectedRecoveryEpisodeRevision;
};

enum class CommandStatus : std::uint8_t {
    Proposed,
    Applied,
    NoChange,
    NotConfirmed,
    NotAllowedInState,
    InvalidInput,
    StaleState,
    // Nur innerhalb des begrenzten, gleitenden In-Memory-Fensters
    // `RunCommandState::processedCommandIds` (siehe dort) garantiert. Eine
    // laengerfristige, sitzungsuebergreifende Wiederholungserkennung ist kein
    // Bestandteil von Issue #15 und folgt mit #27; dauerhafte
    // Kommandoatomaritaet folgt mit #17.
    AlreadyProcessed,
    SafetyRejected,
    CapacityReached,
    ContextMissing,
};

enum class CommandKind : std::uint8_t {
    StartProgram,
    StartManualHolding,
    AbortAndTurnOff,
    AbortAndCool,
    AcknowledgeCompletion,
    CoolAfterCompletion,
    AdjustRun,
    ApplyRecoveryTimeCorrection,
    AcknowledgeMessage,
    MuteMessage,
    ResetFault,
    ApplySensorSelectionAction,
};

struct ManualRunPlanRequest {
    std::string runId;
    double targetTemperatureCelsius{0.0};
    RunSensorMode sensorMode{RunSensorMode::Air};
    bool preheatEnabled{false};
    std::optional<std::uint32_t> maximumProductWaitMinutes;
    double qualificationBandCelsius{0.0};
    std::uint32_t qualificationDurationMinutes{0U};
    std::uint32_t maximumTargetReachMinutes{0U};
};

struct ManualRunPlan {
    ManualRunPlanRequest values;
    CommandSource source{CommandSource::LocalDisplay};
    std::uint64_t createdAtMonotonicMillis{0U};
    ProcessKind kind{ProcessKind::ManualHolding};
};

[[nodiscard]] bool validateManualRunPlan(const ManualRunPlan& plan);
[[nodiscard]] std::optional<ProcessRunSnapshot> makeProcessRunSnapshot(
    const ManualRunPlan& plan);

struct StartSummary {
    std::string runId;
    std::string programName;
    double targetTemperatureCelsius{0.0};
    std::optional<std::uint32_t> durationMinutes;
    RunSensorMode sensorMode{RunSensorMode::Air};
    bool preheatEnabled{false};
    CompletionMode completionMode{CompletionMode::FinishWithoutCooling};
    ProcessKind kind{ProcessKind::Timed};
};

// `program` ist ein bereits fachlich aufgeloestes, vertrauenswuerdiges
// Startprogramm, kein von aussen frei mitgeliefertes Programmdokument. Diese
// Kommandoschicht prueft nur noch Lauf-, Sicherheits- und Werteregeln, nicht
// aber, ob `program` tatsaechlich noch dem aktuellen Katalog- oder
// Programmstand entspricht.
//
// Vor einer echten UI-/Web-API-Anbindung (#16, #27) darf ein Aufrufer ein
// `ProgramDocument` nicht ungeprueft als Startvertrag einliefern. Stattdessen
// muss die aufrufende Schicht die `ProgramDefinition` ueber Programm-ID und
// erwartete Katalog-/Programmrevision aus der aktuellen
// `RuntimeConfigurationSnapshot` (#16) aufloesen. Ist das Programm seither
// geaendert oder geloescht worden, ist das Ergebnis `CommandStatus::
// StaleState` oder ein typisierter Aufloesungsfehler der aufrufenden Schicht
// - kein stiller Fallback auf einen veralteten oder erfundenen Stand. Der
// aktive Lauf erhaelt danach weiterhin seinen eigenen unveraenderlichen
// Schnappschuss (`RunProgramSnapshot`), unabhaengig von spaeteren
// Katalogaenderungen.
//
// Diese PR fuehrt weder `ConfigurationService` noch eine provisorische
// Katalogpersistenz ein; das bleibt Scope von #16.
// #21, 6.5: Vorbedingung fuer jede Zeile der Startmatrix - Luft und
// Kuehlkoerpersensor muessen zum Startzeitpunkt gueltig sein, unabhaengig von
// `SensorPreference` und angefordertem Modus. `productSensorValid` wird nur
// konsultiert, wenn `sensorMode == RunSensorMode::Product` angefordert wird.
struct ProgramStartRequest {
    CommandEnvelope envelope;
    std::string runId;
    ProgramDocument program;
    ProgramSourceKind sourceKind{ProgramSourceKind::UserProgram};
    std::uint32_t sourceProgramRevision{0U};
    RunSensorMode sensorMode{RunSensorMode::Air};
    bool safetyAllowsStart{false};
    bool airSensorValid{false};
    bool coolingSensorValid{false};
    bool productSensorValid{false};
};

struct ManualStartRequest {
    CommandEnvelope envelope;
    ManualRunPlanRequest plan;
    bool safetyAllowsStart{false};
    bool airSensorValid{false};
    bool coolingSensorValid{false};
    bool productSensorValid{false};
};

enum class StopOption : std::uint8_t {
    Back,
    AbortAndTurnOff,
    AbortAndCool,
};

struct StopRequest {
    CommandEnvelope envelope;
    StopOption option{StopOption::Back};
    std::optional<ManualRunPlanRequest> coolingPlan;
    bool safetyAllowsCooling{false};
    // Korrekturauftrag Befund 2: der Kuehl-Ersatzlauf darf NormalAir/Allowed
    // nicht mehr per Konvention annehmen (siehe startSensorSelectionOutcome
    // in run_commands.cpp) - explizite Air-/Cooling-Evidenz wie bei jedem
    // anderen Start (6.5), sonst bleibt die Erstbefuellung fail-closed Blocked.
    bool airSensorValid{false};
    bool coolingSensorValid{false};
};

struct CompletionRequest {
    CommandEnvelope envelope;
    bool startCooling{false};
    std::optional<ManualRunPlanRequest> coolingPlan;
    bool safetyAllowsCooling{false};
    // Siehe StopRequest.
    bool airSensorValid{false};
    bool coolingSensorValid{false};
};

struct RunAdjustmentCommandRequest {
    CommandEnvelope envelope;
    std::optional<double> targetTemperatureCelsius;
    std::optional<std::uint32_t> remainingDurationMinutes;
    bool safetyAllowsChange{false};
};

struct ApplyRecoveryTimeCorrectionRequest {
    CommandEnvelope envelope;
    std::uint32_t secondsDelta{0U};
};

struct RunAdjustmentPreview {
    EffectiveRunValues before;
    EffectiveRunValues after;
    ProcessState phase{ProcessState::Standby};
    bool targetRequalificationRequired{false};
    bool timerContinuesWithoutBiologicalCorrection{false};
};

// Manuelle Sensoraktion (#21, 6.11/6.14.3). `action` liegt in
// sensor_selection_types.hpp (dependency-frei); dieser Vertrag bindet
// zusaetzlich CommandEnvelope und gehoert deshalb hierher, nicht dorthin.
// `safetyAllowsChange` ist wie `ProgramStartRequest::safetyAllowsStart` ein
// zusaetzliches externes Pruefsignal - es ersetzt weder die interne
// `criticalSafetyEventPending`-Invariante noch wird es von ihr ersetzt.
struct SensorSelectionCommandRequest {
    CommandEnvelope envelope;
    SensorSelectionUserAction action{SensorSelectionUserAction::RecheckProduct};
    bool safetyAllowsChange{false};
};

enum class MessageCode : std::uint8_t {
    ProductInsertionRequested,
    TargetReachTimeExceeded,
    UserDecisionRequired,
    RunCompleted,
    RunAborted,
    RecoveryPending,
    SafetyFault,
};

enum class MessageClass : std::uint8_t {
    Information,
    ProcessWarning,
    Recovery,
    DecisionRequired,
    SafetyFault,
};

enum class MessageTrigger : std::uint8_t {
    Process,
    UserCommand,
    Recovery,
    Safety,
};

enum class AcousticIntent : std::uint8_t {
    None,
    SinglePattern,
    RepeatWarning,
    RepeatDecision,
    RepeatFault,
};

struct RuntimeMessage {
    std::uint32_t id{0U};
    MessageCode code{MessageCode::RunCompleted};
    MessageClass messageClass{MessageClass::Information};
    std::uint8_t priority{0U};
    MessageTrigger trigger{MessageTrigger::Process};
    std::uint64_t monotonicMillis{0U};
    bool active{true};
    bool acknowledged{false};
    bool resolved{false};
    bool decisionRequired{false};
    bool acousticMuted{false};
    AcousticIntent acousticIntent{AcousticIntent::None};
    std::optional<std::uint32_t> runRevision;
    std::optional<std::uint32_t> stateSequence;
    std::optional<std::uint32_t> faultRevision;
    std::uint32_t revision{0U};
};

struct MessageCommandRequest {
    CommandEnvelope envelope;
    std::uint32_t messageId{0U};
};

enum class FaultResetRejection : std::uint8_t {
    None,
    CauseStillActive,
    SafetyChecksFailed,
    AuthorizationMissing,
    OtherActiveFault,
    StaleEvaluation,
};

// Typed evidence from the future local authorization producer. The PIN
// verification itself is outside Issue #24; a missing, expired, or
// insufficient value is never interpreted as authorization.
enum class FaultResetAuthorizationLevel : std::uint8_t {
    None,
    Operator,
    Service,
    Technical,
};

struct FaultResetAuthorizationEvidence {
    CommandId evidenceId{0U};
    FaultResetAuthorizationLevel level{FaultResetAuthorizationLevel::None};
    std::uint64_t issuedAtMonotonicMillis{0U};
    std::uint64_t expiresAtMonotonicMillis{0U};
    FaultInstanceId targetFault;
    std::uint32_t targetFaultRevision{0U};
};

enum class FaultResetCheckStatus : std::uint8_t {
    Unknown,
    Passed,
    Failed,
};

struct FaultResetSafetyEvidence {
    FaultResetCheckStatus sensor{FaultResetCheckStatus::Unknown};
    FaultResetCheckStatus actuator{FaultResetCheckStatus::Unknown};
    FaultResetCheckStatus persistence{FaultResetCheckStatus::Unknown};
    FaultResetCheckStatus integrity{FaultResetCheckStatus::Unknown};

    [[nodiscard]] bool allPassed() const {
        return sensor == FaultResetCheckStatus::Passed &&
               actuator == FaultResetCheckStatus::Passed &&
               persistence == FaultResetCheckStatus::Passed &&
               integrity == FaultResetCheckStatus::Passed;
    }
};

struct FaultResetEvaluation {
    bool allowed{false};
    bool causeStillActive{true};
    bool safetyChecksPassed{false};
    bool authorizationSatisfied{false};
    bool safetyEvidenceComplete{false};
    bool codePolicyAllowsReset{false};
    bool otherBlockingFaultActive{false};
    std::uint32_t faultRevision{0U};
    FaultResetAuthorizationLevel requiredAuthorization{
        FaultResetAuthorizationLevel::None};
    FaultResetAuthorizationLevel presentedAuthorization{
        FaultResetAuthorizationLevel::None};
    FaultResetRejection rejection{FaultResetRejection::CauseStillActive};
};

struct FaultResetRequest {
    CommandEnvelope envelope;
    FaultInstanceId targetFault;
};

enum class CommandEffect : std::uint8_t {
    RunStarted,
    RunAborted,
    SafePeltierStopRequested,
    FanRunOnRequired,
    ManualRunStarted,
    RunAdjusted,
    CompletionAcknowledged,
    MessageAcknowledged,
    AcousticMuted,
    FaultResetAuthorized,
    // #21, 6.14.4: transports only that #21's factual precondition for
    // Peltier control changed (peltierPermission). It is NOT a direct actor
    // release - #23/#24 still evaluate every remaining actor safety gate
    // (control dead time, interlocks) before actually switching anything.
    SensorSelectionPermissionBlocked,
    SensorSelectionPermissionRestored,
};

struct RunCommandState {
    ProcessRuntimeState processState;
    std::optional<ActiveRun> activeProgramRun;
    std::optional<ManualRunPlan> activeManualRun;
    std::optional<ProcessRunSnapshot> processRunSnapshot;
    std::string activeRunId;
    // Der Modus gehoert zum aktiven Laufvertrag und wird zusammen mit dem
    // Programmschnappschuss beziehungsweise dem manuellen Plan gesetzt. Er
    // ist keine Sensorqualitaets- oder Hardwareentscheidung.
    std::optional<RunSensorMode> activeRunSensorMode;
    // Persisted sensor-selection provenance (#21, 6.12). RAM-only fields
    // (SensorSelectionRuntimeState) are added by #21 Commit 4 - the wire
    // format never carries them (6.12: "ausdruecklich ausserhalb des
    // Wireformats").
    std::optional<PersistedSensorSelectionState> sensorSelection;
    // RAM-only Laufzeitzustand der Sensorselektion (#21, 6.4.11/6.14.6). Der
    // Default entspricht bereits dem einzigen NoActiveRun-Inaktivzustand
    // (siehe sensor_selection_types.hpp), deshalb kein weiterer expliziter
    // Reset noetig ausser durch clearActiveRunState().
    SensorSelectionRuntimeState sensorSelectionRuntime;
    std::uint32_t runRevision{0U};
    std::array<RuntimeMessage, run_command_limits::kMaximumRuntimeMessages>
        messages{};
    std::size_t messageCount{0U};
    std::uint32_t messageRevision{0U};
    std::uint32_t faultRevision{0U};
    FaultCoreSnapshot faultSnapshot;
    bool criticalSafetyEventPending{false};
    std::uint32_t commandSequence{0U};
    std::uint64_t lastCommandMonotonicMillis{0U};
    // Begrenztes, gleitendes In-Memory-Fenster der zuletzt verarbeiteten
    // Kommando-IDs (siehe `run_command_limits::kMaximumProcessedCommandIds`).
    // `CommandStatus::AlreadyProcessed` erkennt eine Wiederholung nur, solange
    // deren ID noch in diesem Fenster steht; eine verdraengte, aeltere ID kann
    // von dieser Struktur nicht mehr als Wiederholung erkannt werden. Es gibt
    // absichtlich keine unbegrenzte ID-Historie und keine Sitzungslogik in
    // dieser Struktur - eine sitzungsgebundene Replay-/Transportsemantik
    // folgt mit #27, persistierte Kommandoatomaritaet mit #17.
    std::array<CommandId, run_command_limits::kMaximumProcessedCommandIds>
        processedCommandIds{};
    std::size_t processedCommandCount{0U};
    // Schema 3 (#18), gespiegelt aus RunPersistenceSnapshot (siehe dortigen
    // Kommentar): RAM-Zustand fuer die Recovery-Orchestrierung
    // (RunPersistenceCoordinator/RunRecoveryCoordinator). clearActiveRunState()
    // setzt alle bis auf recoveryTemperatureEvidence und
    // recoveryEpisodeRevision zurueck (analog zu runRevision bleibt Letzteres
    // ein monoton gefuehrter Zaehler ueber Laufgrenzen hinweg).
    std::optional<PendingRecoveryAnchor> pendingRecoveryAnchor;
    std::optional<std::uint64_t> recoveryBootAnchorMonotonicMillis;
    RecoveryTemperatureEvidence recoveryTemperatureEvidence;
    std::optional<RecoveryEpisodeEvidence> lastRecoveryEpisodeEvidence;
    std::optional<TaggedPriorBootPhaseElapsed> priorBootPhaseElapsed;
    std::optional<NominalRecoveryAdjustmentState> nominalRecoveryAdjustment;
    std::uint32_t recoveryEpisodeRevision{0U};
    RunProgressState runProgress;
};

// Liefert die fuer die Fermenting-Dauerentscheidung wirksame Vorzeitbasis.
// Die bestaetigte nominale Korrektur bleibt dabei strukturell getrennt von
// `priorBootPhaseElapsed` und wird nur fuer diese fachliche Auswertung
// addiert. Ein inkonsistenter Kandidat wird fail-closed als `nullopt`
// zurueckgegeben.
[[nodiscard]] std::optional<PriorBootPhaseElapsed>
effectivePriorElapsedForFermenting(const RunCommandState& current);

// Checked, gemeinsame Fortschreibung der ausschliesslich sicher beobachteten
// Fermenting-Sekunden. Der Aufrufer berechnet den fachlichen Delta-Punkt
// (Live-Transition, AdjustRun oder konkreter Hop 1); diese Funktion ist die
// einzige uint32-Fold-Grenze und mutiert bei Overflow nicht.
[[nodiscard]] bool foldObservedRunSeconds(RunCommandState& candidate,
                                          std::uint64_t deltaSeconds);

[[nodiscard]] std::optional<std::uint32_t> deriveFermentingSecondsDelta(
    const RunCommandState& before, std::uint64_t atMillis);

struct CommandDecision {
    CommandStatus status{CommandStatus::InvalidInput};
    CommandKind kind{CommandKind::StartProgram};
    CommandEnvelope envelope;
    RunCommandState before;
    RunCommandState after;
    std::optional<StartSummary> startSummary;
    std::optional<RunAdjustmentPreview> adjustmentPreview;
    std::array<CommandEffect, run_command_limits::kMaximumCommandEffects>
        effects{};
    std::size_t effectCount{0U};
    // #21, 6.11/6.14.3: bei ApplySensorSelectionAction immer gesetzt, sonst
    // immer std::nullopt. Alleinige Transportquelle des manuellen Pfads;
    // `status`/`after` duerfen diese Entscheidung nicht ersetzen.
    std::optional<SensorSelectionApplyStatus> sensorSelectionApplyStatus;
    std::optional<SensorSelectionEvent> sensorSelectionEvent;
    std::optional<SensorSelectionNotice> sensorSelectionNotice;
    // #21, 6.5/6.11: von decideProgramStart/decideManualStart bereits vor
    // der Bestaetigungspruefung gefuellt (analog startSummary) - nur bei
    // tatsaechlichem requestedMode != effectiveMode (Startmatrix Zeile 2).
    std::optional<StartSensorSelectionNotice> startSensorSelectionNotice;
    // Fluessige #22-Commit-Information. Sie wird nur nach erfolgreichem
    // persistCommand/applyRunCommand an den PI-Kern weitergereicht und wird
    // weder persistiert noch als zweiter Lauf-/Prozessstatus verwendet.
    std::optional<CommittedControlContextTransition>
        committedControlContextTransition;

    [[nodiscard]] bool proposed() const {
        return status == CommandStatus::Proposed;
    }
};

[[nodiscard]] CommandDecision decideProgramStart(
    const RunCommandState& current, const ProgramStartRequest& request);
[[nodiscard]] CommandDecision decideManualStart(
    const RunCommandState& current, const ManualStartRequest& request);
[[nodiscard]] CommandDecision decideStop(const RunCommandState& current,
                                         const StopRequest& request);
[[nodiscard]] CommandDecision decideCompletion(
    const RunCommandState& current, const CompletionRequest& request);
[[nodiscard]] CommandDecision decideRunAdjustment(
    const RunCommandState& current, const RunAdjustmentCommandRequest& request);
[[nodiscard]] CommandDecision decideApplyRecoveryTimeCorrection(
    const RunCommandState& current,
    const ApplyRecoveryTimeCorrectionRequest& request);
[[nodiscard]] CommandDecision decideAcknowledgeMessage(
    const RunCommandState& current, const MessageCommandRequest& request);
[[nodiscard]] CommandDecision decideMuteMessage(
    const RunCommandState& current, const MessageCommandRequest& request);
[[nodiscard]] CommandDecision decideFaultReset(
    const RunCommandState& current, const FaultResetRequest& request);
// #21, 6.14.3: ruft applySensorSelectionDecision (sensor_selection.hpp) auf
// derselben Kandidatenkopie wie der automatische Coordinator-Pfad auf - keine
// zweite Regelimplementierung. `plausibility` ist ein eigener Parameter statt
// eines Felds von SensorSelectionCommandRequest, weil weder RunCommandState
// noch dieser Vertrag den Typ (sensor_selection.hpp) ohne einen nach 7
// unzulaessigen Include tragen koennen.
[[nodiscard]] CommandDecision decideApplySensorSelectionAction(
    const RunCommandState& current,
    const SensorSelectionCommandRequest& request,
    const CrossRolePlausibilityContext& plausibility);

[[nodiscard]] CommandStatus applyRunCommand(RunCommandState& current,
                                            const CommandDecision& decision);
[[nodiscard]] const RuntimeMessage* highestPriorityActiveMessage(
    const RunCommandState& state);
// #21, 6.14.6: einzige Implementierung fuer jeden terminalen Laufpfad
// (Abort/Complete/Tombstone/ProductWaitExpired); ersetzt die vormals
// getrennten run_commands.cpp::clearActiveRun und
// run_persistence_coordinator.cpp::clearCandidateRun.
void clearActiveRunState(RunCommandState& state);

// Korrekturauftrag Befund 1: gemeinsamer mechanischer Mutationshelfer fuer
// den manuellen Pfad (decideApplySensorSelectionAction) und den
// automatischen Pfad (RunPersistenceCoordinator::persistSensorSelection,
// run_persistence_coordinator.cpp) - verhindert, dass Laufzeitzustand,
// aktiver Modus, Persistenzzustand, Laufrevision und der in
// ManualRunPlan::values.sensorMode duplizierte Modus auseinanderlaufen. Rein
// mechanisch: trifft keine Fachentscheidung, die bleibt vollstaendig in
// sensor_selection.cpp/applySensorSelectionDecision.
// SensorSelectionStateMutation ist bereits ueber sensor_selection_types.hpp
// sichtbar (RunCommandState braucht SensorSelectionRuntimeState ohnehin), kein
// zusaetzlicher Include.
void applySensorSelectionMutation(RunCommandState& state,
                                  const SensorSelectionStateMutation& mutation);

// Einziger Projektionseinstieg aus dem kanonischen #24-Faultkern in den
// bestehenden Command-/Messagekonsumenten.
void applyFaultCoreProjection(RunCommandState& state,
                              const FaultCore& faultCore);

// Reuses the existing QualificationReset -> REACHING_TARGET topology for an
// effective sensor-role change committed while target qualification is active.
// Returns false without mutation when the current process context cannot
// produce that validated state-machine transition.
[[nodiscard]] bool applySensorRoleChangeQualificationReset(
    RunCommandState& state, std::uint64_t monotonicMillis);

}  // namespace fermentation
