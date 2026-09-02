#include "fermentation_ui_commands.hpp"

#include <utility>

namespace fermentation {
namespace {

using Category = device_platform::DeviceUiCommandOutcomeCategory;

FermentationUiCommandResult makeResult(Category category,
                                       FermentationUiCommandDetail detail) {
    return {device_platform::safeOutcomeCategory(category), std::move(detail),
            std::nullopt, std::nullopt};
}

Category categoryFor(CommandStatus status) {
    switch (status) {
        case CommandStatus::Proposed:
        case CommandStatus::Applied:
        case CommandStatus::NoChange:
        case CommandStatus::AlreadyProcessed:
            return Category::Accepted;
        case CommandStatus::NotConfirmed:
            return Category::ConfirmationRequired;
        case CommandStatus::NotAllowedInState:
        case CommandStatus::InvalidInput:
        case CommandStatus::StaleState:
        case CommandStatus::SafetyRejected:
        case CommandStatus::CapacityReached:
        case CommandStatus::ContextMissing:
            return Category::Rejected;
    }
    return Category::Rejected;
}

Category categoryFor(RunPersistenceResultStatus status) {
    switch (status) {
        case RunPersistenceResultStatus::Applied:
        case RunPersistenceResultStatus::CheckpointWritten:
        case RunPersistenceResultStatus::AlreadyProcessed:
        case RunPersistenceResultStatus::AlreadyPersisted:
            return Category::Accepted;
        case RunPersistenceResultStatus::Busy:
            return Category::Busy;
        case RunPersistenceResultStatus::NotInitialized:
        case RunPersistenceResultStatus::RecoveryPending:
        case RunPersistenceResultStatus::PersistenceIndeterminate:
        case RunPersistenceResultStatus::PersistenceCommittedApplyFailed:
        case RunPersistenceResultStatus::Blocked:
            return Category::Unavailable;
        case RunPersistenceResultStatus::NotEligible:
        case RunPersistenceResultStatus::NotAllowedInState:
        case RunPersistenceResultStatus::InvalidDecision:
        case RunPersistenceResultStatus::StaleDecision:
        case RunPersistenceResultStatus::TimeMismatch:
        case RunPersistenceResultStatus::TimeWentBackwards:
        case RunPersistenceResultStatus::CounterOverflow:
        case RunPersistenceResultStatus::WriteFailed:
        case RunPersistenceResultStatus::CapacityExceeded:
        case RunPersistenceResultStatus::NotDue:
        case RunPersistenceResultStatus::NoActiveRun:
            return Category::Rejected;
    }
    return Category::Rejected;
}

Category categoryFor(ConfigurationPreviewStatus status) {
    switch (status) {
        case ConfigurationPreviewStatus::Success:
            return Category::Accepted;
        case ConfigurationPreviewStatus::ConfigurationModelBudgetBusy:
            return Category::Busy;
        case ConfigurationPreviewStatus::ConfigurationRuntimeUnavailable:
            return Category::Unavailable;
        case ConfigurationPreviewStatus::InvalidCandidate:
        case ConfigurationPreviewStatus::StateChanged:
        case ConfigurationPreviewStatus::PreviewNotFound:
        case ConfigurationPreviewStatus::PreviewSuperseded:
            return Category::Rejected;
    }
    return Category::Rejected;
}

Category categoryFor(ConfigurationCommitStatus status) {
    switch (status) {
        case ConfigurationCommitStatus::Activated:
        case ConfigurationCommitStatus::NoChange:
            return Category::Accepted;
        case ConfigurationCommitStatus::ConfigurationMutationBusy:
            return Category::Busy;
        case ConfigurationCommitStatus::ConfigurationCommitIndeterminate:
        case ConfigurationCommitStatus::ConfigurationRuntimeFailure:
            return Category::Unavailable;
        case ConfigurationCommitStatus::PreviewNotFound:
        case ConfigurationCommitStatus::PreviewSuperseded:
        case ConfigurationCommitStatus::ConfigurationConflictFailure:
        case ConfigurationCommitStatus::ConfigurationValidationFailure:
        case ConfigurationCommitStatus::PersistenceFailure:
        case ConfigurationCommitStatus::CapacityFailure:
            return Category::Rejected;
    }
    return Category::Rejected;
}

template <typename Request, typename Decide>
FermentationUiCommandResult decide(
    const RunCommandState& current, Request request,
    const FermentationUiCommandContext& context,
    const std::optional<FermentationUiConfirmationRequest>& confirmation,
    Decide decideCanonical) {
    request.envelope = FermentationUiCommandBridge::makeEnvelope(context);
    const auto decision = decideCanonical(current, request);
    return FermentationUiCommandBridge::fromCommandStatus(decision.status,
                                                          confirmation);
}

}  // namespace

CommandEnvelope FermentationUiCommandBridge::makeEnvelope(
    const FermentationUiCommandContext& context) noexcept {
    return {context.requestId.value,
            context.surface == device_platform::UiSurface::LocalDisplay
                ? CommandSource::LocalDisplay
                : CommandSource::WebInterface,
            context.monotonicMillis,
            context.expected.expectedStateSequence,
            context.expected.expectedRunRevision,
            context.expected.expectedMessageRevision,
            context.expected.expectedFaultRevision,
            context.confirmed,
            context.expected.expectedRecoveryEpisodeRevision};
}

FermentationUiConfirmationRequest
FermentationUiCommandBridge::confirmationRequest(
    const FermentationUiCommandContext& context, FermentationUiAction action,
    device_platform::TextKey title, device_platform::TextKey summary) {
    return {action, std::move(title), std::move(summary), context.expected};
}

FermentationUiCommandResult FermentationUiCommandBridge::fromCommandStatus(
    CommandStatus status,
    const std::optional<FermentationUiConfirmationRequest>& confirmation) {
    auto result = makeResult(categoryFor(status), status);
    if (status == CommandStatus::NotConfirmed && confirmation.has_value()) {
        result.confirmation = confirmation;
    }
    return result;
}

FermentationUiCommandResult
FermentationUiCommandBridge::fromRunPersistenceResult(
    RunPersistenceResultStatus status) {
    return makeResult(categoryFor(status), status);
}

FermentationUiCommandResult
FermentationUiCommandBridge::fromConfigurationPreview(
    ConfigurationPreviewStatus status) {
    return makeResult(categoryFor(status), status);
}

FermentationUiCommandResult
FermentationUiCommandBridge::fromConfigurationCommit(
    ConfigurationCommitStatus status) {
    return makeResult(categoryFor(status), status);
}

FermentationUiCommandResult
FermentationUiCommandBridge::unsupportedAppDetail() {
    return makeResult(Category::Rejected,
                      FermentationUiDetailStatus::UnsupportedAppDetail);
}

FermentationUiCommandResult FermentationUiCommandBridge::decideProgramStart(
    const RunCommandState& current, ProgramStartRequest request,
    const FermentationUiCommandContext& context,
    const std::optional<FermentationUiConfirmationRequest>& confirmation) {
    return decide(current, std::move(request), context, confirmation,
                  [](const RunCommandState& state,
                     const ProgramStartRequest& input) {
                      return ::fermentation::decideProgramStart(state, input);
                  });
}

FermentationUiCommandResult FermentationUiCommandBridge::decideManualStart(
    const RunCommandState& current, ManualStartRequest request,
    const FermentationUiCommandContext& context,
    const std::optional<FermentationUiConfirmationRequest>& confirmation) {
    return decide(current, std::move(request), context, confirmation,
                  [](const RunCommandState& state,
                     const ManualStartRequest& input) {
                      return ::fermentation::decideManualStart(state, input);
                  });
}

FermentationUiCommandResult FermentationUiCommandBridge::decideStop(
    const RunCommandState& current, StopRequest request,
    const FermentationUiCommandContext& context,
    const std::optional<FermentationUiConfirmationRequest>& confirmation) {
    return decide(current, std::move(request), context, confirmation,
                  [](const RunCommandState& state, const StopRequest& input) {
                      return ::fermentation::decideStop(state, input);
                  });
}

FermentationUiCommandResult FermentationUiCommandBridge::decideCompletion(
    const RunCommandState& current, CompletionRequest request,
    const FermentationUiCommandContext& context,
    const std::optional<FermentationUiConfirmationRequest>& confirmation) {
    return decide(current, std::move(request), context, confirmation,
                  [](const RunCommandState& state,
                     const CompletionRequest& input) {
                      return ::fermentation::decideCompletion(state, input);
                  });
}

FermentationUiCommandResult
FermentationUiCommandBridge::decideRunAdjustment(
    const RunCommandState& current, RunAdjustmentCommandRequest request,
    const FermentationUiCommandContext& context,
    const std::optional<FermentationUiConfirmationRequest>& confirmation) {
    return decide(current, std::move(request), context, confirmation,
                  [](const RunCommandState& state,
                     const RunAdjustmentCommandRequest& input) {
                      return ::fermentation::decideRunAdjustment(state, input);
                  });
}

FermentationUiCommandResult
FermentationUiCommandBridge::decideApplyRecoveryTimeCorrection(
    const RunCommandState& current, ApplyRecoveryTimeCorrectionRequest request,
    const FermentationUiCommandContext& context,
    const std::optional<FermentationUiConfirmationRequest>& confirmation) {
    return decide(current, std::move(request), context, confirmation,
                  [](const RunCommandState& state,
                     const ApplyRecoveryTimeCorrectionRequest& input) {
                      return ::fermentation::decideApplyRecoveryTimeCorrection(
                          state, input);
                  });
}

FermentationUiCommandResult
FermentationUiCommandBridge::decideAcknowledgeMessage(
    const RunCommandState& current, MessageCommandRequest request,
    const FermentationUiCommandContext& context,
    const std::optional<FermentationUiConfirmationRequest>& confirmation) {
    return decide(current, std::move(request), context, confirmation,
                  [](const RunCommandState& state,
                     const MessageCommandRequest& input) {
                      return ::fermentation::decideAcknowledgeMessage(state,
                                                                        input);
                  });
}

FermentationUiCommandResult FermentationUiCommandBridge::decideMuteMessage(
    const RunCommandState& current, MessageCommandRequest request,
    const FermentationUiCommandContext& context,
    const std::optional<FermentationUiConfirmationRequest>& confirmation) {
    return decide(current, std::move(request), context, confirmation,
                  [](const RunCommandState& state,
                     const MessageCommandRequest& input) {
                      return ::fermentation::decideMuteMessage(state, input);
                  });
}

FermentationUiCommandResult FermentationUiCommandBridge::decideFaultReset(
    const RunCommandState& current, FaultResetRequest request,
    const FermentationUiCommandContext& context,
    const std::optional<FermentationUiConfirmationRequest>& confirmation) {
    return decide(current, std::move(request), context, confirmation,
                  [](const RunCommandState& state,
                     const FaultResetRequest& input) {
                      return ::fermentation::decideFaultReset(state, input);
                  });
}

}  // namespace fermentation
