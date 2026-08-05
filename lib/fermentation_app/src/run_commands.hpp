#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "process_state_machine.hpp"
#include "run_command_limits.hpp"
#include "run_snapshot.hpp"

namespace fermentation {

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
    AcknowledgeMessage,
    MuteMessage,
    ResetFault,
};

enum class RunSensorMode : std::uint8_t {
    Product,
    Air,
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
struct ProgramStartRequest {
    CommandEnvelope envelope;
    std::string runId;
    ProgramDocument program;
    ProgramSourceKind sourceKind{ProgramSourceKind::UserProgram};
    std::uint32_t sourceProgramRevision{0U};
    RunSensorMode sensorMode{RunSensorMode::Air};
    bool safetyAllowsStart{false};
};

struct ManualStartRequest {
    CommandEnvelope envelope;
    ManualRunPlanRequest plan;
    bool safetyAllowsStart{false};
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
};

struct CompletionRequest {
    CommandEnvelope envelope;
    bool startCooling{false};
    std::optional<ManualRunPlanRequest> coolingPlan;
    bool safetyAllowsCooling{false};
};

struct RunAdjustmentCommandRequest {
    CommandEnvelope envelope;
    std::optional<double> targetTemperatureCelsius;
    std::optional<std::uint32_t> remainingDurationMinutes;
    bool safetyAllowsChange{false};
};

struct RunAdjustmentPreview {
    EffectiveRunValues before;
    EffectiveRunValues after;
    ProcessState phase{ProcessState::Standby};
    bool targetRequalificationRequired{false};
    bool timerContinuesWithoutBiologicalCorrection{false};
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

struct FaultResetEvaluation {
    bool allowed{false};
    bool causeStillActive{true};
    bool safetyChecksPassed{false};
    bool authorizationSatisfied{false};
    bool otherBlockingFaultActive{false};
    std::uint32_t faultRevision{0U};
    FaultResetRejection rejection{FaultResetRejection::CauseStillActive};
};

struct FaultResetRequest {
    CommandEnvelope envelope;
    FaultResetEvaluation evaluation;
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
    std::uint32_t runRevision{0U};
    std::array<RuntimeMessage, run_command_limits::kMaximumRuntimeMessages>
        messages{};
    std::size_t messageCount{0U};
    std::uint32_t messageRevision{0U};
    std::uint32_t faultRevision{0U};
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
};

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
[[nodiscard]] CommandDecision decideAcknowledgeMessage(
    const RunCommandState& current, const MessageCommandRequest& request);
[[nodiscard]] CommandDecision decideMuteMessage(
    const RunCommandState& current, const MessageCommandRequest& request);
[[nodiscard]] CommandDecision decideFaultReset(
    const RunCommandState& current, const FaultResetRequest& request);

[[nodiscard]] CommandStatus applyRunCommand(RunCommandState& current,
                                            const CommandDecision& decision);
[[nodiscard]] const RuntimeMessage* highestPriorityActiveMessage(
    const RunCommandState& state);

}  // namespace fermentation
