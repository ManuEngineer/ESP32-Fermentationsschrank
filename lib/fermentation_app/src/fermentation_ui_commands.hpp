#pragma once

#include <optional>
#include <string>
#include <variant>

#include "application_run_identity.hpp"
#include "configuration_service.hpp"
#include "fermentation_ui_models.hpp"
#include "sensor_selection.hpp"

namespace fermentation {

class FermentationApplication;

enum class FermentationUiAction : std::uint8_t {
    StartProgram,
    StartManualHolding,
    StopRun,
    CompleteRun,
    AdjustRun,
    ApplyRecoveryTimeCorrection,
    AcknowledgeMessage,
    MuteMessage,
    ResetFault,
    ApplySensorSelection,
    CommitConfiguration,
    ResumeFallback,
};

struct FermentationUiCommandContext {
    device_platform::UiSurface surface{
        device_platform::UiSurface::LocalDisplay};
    std::uint64_t monotonicMillis{0U};
    FermentationUiExpectedRevisions expected;
    bool confirmed{false};
};

struct FermentationUiConfirmationRequest {
    FermentationUiAction action{FermentationUiAction::StartProgram};
    device_platform::TextKey title;
    device_platform::TextKey summary;
    FermentationUiExpectedRevisions expected;
};

// App-owned command payload.  The variant is deliberately closed: each
// mutation enters its existing owning contract, while pure shell/web
// navigation is not represented here at all.
struct FermentationUiConfigurationCommitCommand {
    std::uint64_t previewHandle{0U};
    UserConfigurationRevision expectedUserConfigurationRevision;
    bool confirmed{false};
};

struct FermentationUiResumeFallbackCommand {
    FermentationUiExpectedRevisions expected;
    bool confirmed{false};
};

// The renderer-independent UI contract carries intent only.  In particular,
// these payloads never carry a ProgramDocument, safety/sensor/planner
// evidence, or an owning decision object.  The later application boundary
// resolves the IDs and supplies its own current evidence before entering the
// existing canonical command path.
struct FermentationUiStartProgramIntent {
    std::string programId;
    RunSensorMode sensorMode{RunSensorMode::Air};
};

// Identity-free UI values. The application adds the owning run identity once
// at the existing request boundary; adapters cannot inject a run or command
// identifier through this candidate.
struct FermentationUiManualRunPlanValues {
    double targetTemperatureCelsius{0.0};
    RunSensorMode sensorMode{RunSensorMode::Air};
    bool preheatEnabled{false};
    std::optional<std::uint32_t> maximumProductWaitMinutes;
    double qualificationBandCelsius{0.0};
    std::uint32_t qualificationDurationMinutes{0U};
    std::uint32_t maximumTargetReachMinutes{0U};
};

struct FermentationUiStartManualHoldingIntent {
    FermentationUiManualRunPlanValues plan;
};

struct FermentationUiStopRunIntent {
    StopOption option{StopOption::Back};
    std::optional<FermentationUiManualRunPlanValues> coolingPlan;
};

struct FermentationUiCompleteRunIntent {
    bool startCooling{false};
    std::optional<FermentationUiManualRunPlanValues> coolingPlan;
};

enum class FermentationApplicationRequestStatus : std::uint8_t {
    Prepared,
    NotInitialized,
    Unavailable,
    Overflow,
    StaleProgramCatalog,
    ProgramUnavailable,
    InvalidInput,
};

using FermentationApplicationPreparedRequest =
    std::variant<ProgramStartRequest, ManualStartRequest, StopRequest,
                 CompletionRequest, RunAdjustmentCommandRequest,
                 ApplyRecoveryTimeCorrectionRequest, MessageCommandRequest,
                 FaultResetRequest, SensorSelectionCommandRequest>;

// The application returns an already identity-bound request. Confirmation
// reuses this value; a renderer never supplies a replacement ID or run ID.
struct FermentationApplicationRequestResult {
    FermentationApplicationRequestStatus status{
        FermentationApplicationRequestStatus::Unavailable};
    std::optional<FermentationApplicationPreparedRequest> request;
    // Retained with the prepared request so confirmation/replay can reuse the
    // exact same envelope and any runId derived from it. Adapters cannot
    // construct a replacement identity.
    std::optional<ApplicationCommandIdentity> identity;
    // Sensor-selection preparation also retains the owning evidence supplied
    // by the application boundary. It is never transport-controlled.
    std::optional<CrossRolePlausibilityContext> owningPlausibility;
};

struct FermentationUiAdjustRunIntent {
    std::optional<double> targetTemperatureCelsius;
    std::optional<std::uint32_t> remainingDurationMinutes;
};

struct FermentationUiRecoveryTimeCorrectionIntent {
    std::uint32_t secondsDelta{0U};
};

struct FermentationUiAcknowledgeMessageIntent {
    std::uint32_t messageId{0U};
};

struct FermentationUiMuteMessageIntent {
    std::uint32_t messageId{0U};
};

struct FermentationUiResetFaultIntent {};

struct FermentationUiSensorSelectionIntent {
    SensorSelectionUserAction action{SensorSelectionUserAction::RecheckProduct};
};

// CommandEnvelope-owned operations are tagged by their payload type. There is
// deliberately no second free-standing action tag that could disagree with
// the payload. Configuration and fallback operations remain outside the
// envelope because their owning contracts have no CommandEnvelope/idempotency
// semantics.
using FermentationUiEnvelopePayload = std::variant<
    FermentationUiStartProgramIntent, FermentationUiStartManualHoldingIntent,
    FermentationUiStopRunIntent, FermentationUiCompleteRunIntent,
    FermentationUiAdjustRunIntent, FermentationUiRecoveryTimeCorrectionIntent,
    FermentationUiAcknowledgeMessageIntent, FermentationUiMuteMessageIntent,
    FermentationUiResetFaultIntent, FermentationUiSensorSelectionIntent>;

struct FermentationUiEnvelopeCommand {
    FermentationUiExpectedRevisions expected;
    bool confirmed{false};
    FermentationUiEnvelopePayload payload;
};

struct FermentationUiCommand {
    device_platform::UiSurface surface{
        device_platform::UiSurface::LocalDisplay};
    std::uint64_t monotonicMillis{0U};
    std::variant<FermentationUiEnvelopeCommand,
                 FermentationUiConfigurationCommitCommand,
                 FermentationUiResumeFallbackCommand>
        operation;
};

enum class FermentationUiDetailStatus : std::uint8_t {
    UnsupportedAppDetail,
};

// A canonical Proposed decision is not an owning apply/persist outcome.  The
// phase makes that distinction explicit to every renderer without introducing
// another execution engine.
enum class FermentationUiCommandPhase : std::uint8_t {
    OwningOutcome,
    DecisionOnly,
};

using FermentationUiCommandDetail =
    std::variant<CommandStatus, RunPersistenceResultStatus,
                 ConfigurationPreviewStatus, ConfigurationCommitStatus,
                 FermentationUiDetailStatus>;

struct FermentationUiCommandResult {
    device_platform::DeviceUiCommandOutcomeCategory category{
        device_platform::DeviceUiCommandOutcomeCategory::Unavailable};
    FermentationUiCommandDetail detail{
        FermentationUiDetailStatus::UnsupportedAppDetail};
    FermentationUiCommandPhase phase{FermentationUiCommandPhase::DecisionOnly};
    std::optional<FermentationUiConfirmationRequest> confirmation;
    std::optional<device_platform::UiRefreshRevision> refreshRevision;
};

class FermentationUiCommandBridge {
   public:
    [[nodiscard]] static CommandEnvelope makeEnvelope(
        const FermentationUiCommandContext& context,
        const ApplicationCommandIdentity& identity) noexcept;
    [[nodiscard]] static FermentationUiConfirmationRequest confirmationRequest(
        const FermentationUiCommandContext& context,
        FermentationUiAction action, device_platform::TextKey title,
        device_platform::TextKey summary);

    [[nodiscard]] static FermentationUiCommandResult fromCommandStatus(
        CommandStatus status,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult fromRunPersistenceResult(
        RunPersistenceResultStatus status);
    [[nodiscard]] static FermentationUiCommandResult fromConfigurationPreview(
        ConfigurationPreviewStatus status);
    [[nodiscard]] static FermentationUiCommandResult fromConfigurationCommit(
        ConfigurationCommitStatus status,
        FermentationUiCommandPhase phase =
            FermentationUiCommandPhase::DecisionOnly);
    [[nodiscard]] static FermentationUiCommandResult unsupportedAppDetail();

    [[nodiscard]] static FermentationUiCommandResult decideProgramStart(
        const RunCommandState& current, ProgramStartRequest request,
        const FermentationUiCommandContext& context,
        const ApplicationCommandIdentity& identity,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideManualStart(
        const RunCommandState& current, ManualStartRequest request,
        const FermentationUiCommandContext& context,
        const ApplicationCommandIdentity& identity,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideStop(
        const RunCommandState& current, StopRequest request,
        const FermentationUiCommandContext& context,
        const ApplicationCommandIdentity& identity,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideCompletion(
        const RunCommandState& current, CompletionRequest request,
        const FermentationUiCommandContext& context,
        const ApplicationCommandIdentity& identity,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideRunAdjustment(
        const RunCommandState& current, RunAdjustmentCommandRequest request,
        const FermentationUiCommandContext& context,
        const ApplicationCommandIdentity& identity,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult
    decideApplyRecoveryTimeCorrection(
        const RunCommandState& current,
        ApplyRecoveryTimeCorrectionRequest request,
        const FermentationUiCommandContext& context,
        const ApplicationCommandIdentity& identity,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideAcknowledgeMessage(
        const RunCommandState& current, MessageCommandRequest request,
        const FermentationUiCommandContext& context,
        const ApplicationCommandIdentity& identity,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideMuteMessage(
        const RunCommandState& current, MessageCommandRequest request,
        const FermentationUiCommandContext& context,
        const ApplicationCommandIdentity& identity,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideFaultReset(
        const RunCommandState& current, FaultResetRequest request,
        const FermentationUiCommandContext& context,
        const ApplicationCommandIdentity& identity,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideSensorSelection(
        const RunCommandState& current, SensorSelectionCommandRequest request,
        const FermentationUiCommandContext& context,
        const ApplicationCommandIdentity& identity,
        const CrossRolePlausibilityContext& owningPlausibility,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult fromFallbackResult(
        RunPersistenceResultStatus status);

    // Owning configuration path: expected user revision is checked before
    // confirmation is surfaced, and the existing service performs the
    // authoritative preview/commit validation.
    [[nodiscard]] static FermentationUiCommandResult commitConfiguration(
        ConfigurationService& service,
        const FermentationUiConfigurationCommitCommand& command,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult resumeFallback(
        FermentationApplication& application,
        const FermentationUiResumeFallbackCommand& command,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
};

}  // namespace fermentation
