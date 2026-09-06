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
    StartManualTimed,
    ProductInsertedConfirmed,
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
struct FermentationUiStartCandidate {
    std::string programId;
    std::optional<double> targetTemperatureCelsius;
    std::optional<std::uint32_t> fermentationDurationMinutes;
    std::optional<bool> preheatEnabled;
    std::optional<RunSensorMode> sensorMode;
    std::optional<CompletionMode> completionMode;
    std::optional<double> coolingTargetCelsius;
    std::optional<std::uint32_t> holdDurationMinutes;
};

struct FermentationUiStartProgramIntent {
    std::string programId;
    RunSensorMode sensorMode{RunSensorMode::Air};
    // Optional values are next-run-only overrides. The Application resolves
    // and validates them on a transient ProgramDocument copy.
    std::optional<double> targetTemperatureCelsius;
    std::optional<std::uint32_t> fermentationDurationMinutes;
    std::optional<bool> preheatEnabled;
    std::optional<CompletionMode> completionMode;
    std::optional<double> coolingTargetCelsius;
    std::optional<std::uint32_t> holdDurationMinutes;
    std::optional<FermentationUiStartCandidate> candidate;
};

struct FermentationUiStartManualTimedIntent {
    ManualTimedRunValues values;
};

struct FermentationUiProductInsertedConfirmedIntent {};

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

// A prepared request is an opaque, application-bound command.  The raw
// request variant is intentionally private: a transport may replay this
// object, but cannot replace its envelope, owning evidence, or action tag.
class FermentationApplicationPreparedRequest {
   public:
    FermentationApplicationPreparedRequest(
        const FermentationApplicationPreparedRequest&) = default;
    FermentationApplicationPreparedRequest& operator=(
        const FermentationApplicationPreparedRequest&) = default;
    FermentationApplicationPreparedRequest(
        FermentationApplicationPreparedRequest&&) noexcept = default;
    FermentationApplicationPreparedRequest& operator=(
        FermentationApplicationPreparedRequest&&) noexcept = default;
    ~FermentationApplicationPreparedRequest() = default;

    [[nodiscard]] CommandId commandId() const noexcept;
    [[nodiscard]] const CommandEnvelope& commandEnvelope() const noexcept;
    [[nodiscard]] std::optional<std::string> runId() const;

   private:
    struct PreparedAcknowledgeMessage {
        MessageCommandRequest request;
    };
    struct PreparedMuteMessage {
        MessageCommandRequest request;
    };
    using Storage =
        std::variant<ProgramStartRequest, ManualStartRequest, StopRequest,
                     CompletionRequest, RunAdjustmentCommandRequest,
                     ApplyRecoveryTimeCorrectionRequest,
                     PreparedAcknowledgeMessage, PreparedMuteMessage,
                     FaultResetRequest, SensorSelectionCommandRequest>;

    FermentationApplicationPreparedRequest(
        Storage storage,
        std::optional<CrossRolePlausibilityContext> owningPlausibility);
    void confirm() noexcept;
    [[nodiscard]] const Storage& storage() const noexcept { return storage_; }
    [[nodiscard]] const std::optional<CrossRolePlausibilityContext>&
    owningPlausibility() const noexcept {
        return owningPlausibility_;
    }

    friend class FermentationApplication;
    friend class FermentationUiCommandBridge;

    Storage storage_;
    std::optional<CrossRolePlausibilityContext> owningPlausibility_;
};

// The application returns an already identity-bound request. Confirmation
// reuses this value; a renderer never supplies a replacement ID or run ID.
struct FermentationApplicationRequestResult {
    FermentationApplicationRequestStatus status{
        FermentationApplicationRequestStatus::Unavailable};
    std::optional<FermentationApplicationPreparedRequest> request;
    // Scalar correlation is an application output, never a capability that
    // can be supplied back to bind a different request.
    std::optional<device_platform::UiRequestId> uiRequestId;
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
    FermentationUiStartManualTimedIntent, FermentationUiStopRunIntent,
    FermentationUiCompleteRunIntent, FermentationUiAdjustRunIntent,
    FermentationUiRecoveryTimeCorrectionIntent,
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
    std::variant<CommandStatus, DecisionStatus, RunPersistenceResultStatus,
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
    [[nodiscard]] static FermentationUiConfirmationRequest confirmationRequest(
        const FermentationUiCommandContext& context,
        FermentationUiAction action, device_platform::TextKey title,
        device_platform::TextKey summary);

    [[nodiscard]] static FermentationUiCommandResult fromCommandStatus(
        CommandStatus status,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult fromTransitionDecision(
        DecisionStatus status);
    [[nodiscard]] static FermentationUiCommandResult fromRunPersistenceResult(
        RunPersistenceResultStatus status);
    [[nodiscard]] static FermentationUiCommandResult fromConfigurationPreview(
        ConfigurationPreviewStatus status);
    [[nodiscard]] static FermentationUiCommandResult fromConfigurationCommit(
        ConfigurationCommitStatus status,
        FermentationUiCommandPhase phase =
            FermentationUiCommandPhase::DecisionOnly);
    [[nodiscard]] static FermentationUiCommandResult unsupportedAppDetail();
    [[nodiscard]] static FermentationUiCommandResult
    decideProductInsertedConfirmed(const RunCommandState& current,
                                   const ProcessRunSnapshot* runSnapshot,
                                   const FermentationUiCommandContext& context,
                                   const ProcessSignals& signals,
                                   std::uint64_t monotonicMillis);

    [[nodiscard]] static FermentationUiCommandResult decidePrepared(
        const RunCommandState& current,
        const FermentationApplicationPreparedRequest& request,
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

   private:
    friend class FermentationApplication;
    [[nodiscard]] static CommandEnvelope makeEnvelope(
        const FermentationUiCommandContext& context,
        const ApplicationCommandIdentity& identity) noexcept;
};

}  // namespace fermentation
