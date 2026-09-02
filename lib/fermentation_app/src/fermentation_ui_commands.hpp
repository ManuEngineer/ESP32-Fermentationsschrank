#pragma once

#include <optional>
#include <string>
#include <variant>

#include "configuration_service.hpp"
#include "fermentation_ui_models.hpp"
#include "sensor_selection.hpp"

namespace fermentation {

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
    device_platform::UiSurface surface{device_platform::UiSurface::LocalDisplay};
    device_platform::UiRequestId requestId;
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

using FermentationUiCommandPayload = std::variant<
    ProgramStartRequest, ManualStartRequest, StopRequest, CompletionRequest,
    RunAdjustmentCommandRequest, ApplyRecoveryTimeCorrectionRequest,
    MessageCommandRequest, FaultResetRequest, SensorSelectionCommandRequest,
    FermentationUiConfigurationCommitCommand,
    FermentationUiResumeFallbackCommand>;

struct FermentationUiCommand {
    device_platform::UiSurface surface{device_platform::UiSurface::LocalDisplay};
    device_platform::UiRequestId requestId;
    std::uint64_t monotonicMillis{0U};
    FermentationUiAction action{FermentationUiAction::StartProgram};
    FermentationUiCommandPayload payload;
};

enum class FermentationUiDetailStatus : std::uint8_t {
    UnsupportedAppDetail,
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
    std::optional<FermentationUiConfirmationRequest> confirmation;
    std::optional<device_platform::UiRefreshRevision> refreshRevision;
};

class FermentationUiCommandBridge {
   public:
    [[nodiscard]] static CommandEnvelope makeEnvelope(
        const FermentationUiCommandContext& context) noexcept;
    [[nodiscard]] static FermentationUiConfirmationRequest
    confirmationRequest(const FermentationUiCommandContext& context,
                        FermentationUiAction action,
                        device_platform::TextKey title,
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
        ConfigurationCommitStatus status);
    [[nodiscard]] static FermentationUiCommandResult unsupportedAppDetail();

    [[nodiscard]] static FermentationUiCommandResult decideProgramStart(
        const RunCommandState& current, ProgramStartRequest request,
        const FermentationUiCommandContext& context,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideManualStart(
        const RunCommandState& current, ManualStartRequest request,
        const FermentationUiCommandContext& context,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideStop(
        const RunCommandState& current, StopRequest request,
        const FermentationUiCommandContext& context,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideCompletion(
        const RunCommandState& current, CompletionRequest request,
        const FermentationUiCommandContext& context,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideRunAdjustment(
        const RunCommandState& current, RunAdjustmentCommandRequest request,
        const FermentationUiCommandContext& context,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult
    decideApplyRecoveryTimeCorrection(
        const RunCommandState& current, ApplyRecoveryTimeCorrectionRequest request,
        const FermentationUiCommandContext& context,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideAcknowledgeMessage(
        const RunCommandState& current, MessageCommandRequest request,
        const FermentationUiCommandContext& context,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideMuteMessage(
        const RunCommandState& current, MessageCommandRequest request,
        const FermentationUiCommandContext& context,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideFaultReset(
        const RunCommandState& current, FaultResetRequest request,
        const FermentationUiCommandContext& context,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult decideSensorSelection(
        const RunCommandState& current,
        SensorSelectionCommandRequest request,
        const FermentationUiCommandContext& context,
        const CrossRolePlausibilityContext& owningPlausibility,
        const std::optional<FermentationUiConfirmationRequest>& confirmation =
            std::nullopt);
    [[nodiscard]] static FermentationUiCommandResult fromFallbackResult(
        RunPersistenceResultStatus status);
};

}  // namespace fermentation
