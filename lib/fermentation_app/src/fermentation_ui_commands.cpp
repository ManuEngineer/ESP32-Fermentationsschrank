#include "fermentation_ui_commands.hpp"

#include <type_traits>
#include <utility>

#include "fermentation_application.hpp"

namespace fermentation {
namespace {

using Category = device_platform::DeviceUiCommandOutcomeCategory;

FermentationUiCommandResult makeResult(
    Category category, FermentationUiCommandDetail detail,
    FermentationUiCommandPhase phase =
        FermentationUiCommandPhase::DecisionOnly) {
    return {device_platform::safeOutcomeCategory(category), std::move(detail),
            phase, std::nullopt, std::nullopt};
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
        case ConfigurationPreviewStatus::NotAllowed:
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
        case ConfigurationCommitStatus::ReadyForConfirmation:
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

Category categoryFor(NetworkConfigurationStatus status) {
    switch (status) {
        case NetworkConfigurationStatus::Applied:
            return Category::Accepted;
        case NetworkConfigurationStatus::SelectionRequired:
        case NetworkConfigurationStatus::InvalidMode:
        case NetworkConfigurationStatus::InvalidCredential:
        case NetworkConfigurationStatus::CandidateRejected:
        case NetworkConfigurationStatus::PersistenceFailure:
            return Category::Rejected;
        case NetworkConfigurationStatus::CredentialUnavailable:
        case NetworkConfigurationStatus::TransportFailure:
        case NetworkConfigurationStatus::CommitIndeterminate:
        case NetworkConfigurationStatus::RecoveryRequired:
        case NetworkConfigurationStatus::SetupNotAvailable:
        case NetworkConfigurationStatus::NotInitialized:
            return Category::Unavailable;
    }
    return Category::Rejected;
}

}  // namespace

CommandId FermentationApplicationPreparedRequest::commandId() const noexcept {
    return commandEnvelope().id;
}

const CommandEnvelope& FermentationApplicationPreparedRequest::commandEnvelope()
    const noexcept {
    return std::visit(
        [](const auto& request) -> const CommandEnvelope& {
            using Request = std::decay_t<decltype(request)>;
            if constexpr (std::is_same_v<
                              Request, FermentationApplicationPreparedRequest::
                                           PreparedAcknowledgeMessage> ||
                          std::is_same_v<
                              Request, FermentationApplicationPreparedRequest::
                                           PreparedMuteMessage>) {
                return request.request.envelope;
            } else {
                return request.envelope;
            }
        },
        storage_);
}

std::optional<std::string> FermentationApplicationPreparedRequest::runId()
    const {
    return std::visit(
        [](const auto& request) -> std::optional<std::string> {
            using Request = std::decay_t<decltype(request)>;
            if constexpr (std::is_same_v<Request, ProgramStartRequest>) {
                return request.runId;
            } else if constexpr (std::is_same_v<Request, ManualStartRequest>) {
                return request.plan.runId;
            } else if constexpr (std::is_same_v<Request, StopRequest>) {
                if (request.coolingPlan.has_value())
                    return request.coolingPlan->runId;
            } else if constexpr (std::is_same_v<Request, CompletionRequest>) {
                if (request.coolingPlan.has_value())
                    return request.coolingPlan->runId;
            }
            return std::nullopt;
        },
        storage_);
}

void FermentationApplicationPreparedRequest::confirm() noexcept {
    std::visit(
        [](auto& request) {
            using Request = std::decay_t<decltype(request)>;
            if constexpr (std::is_same_v<
                              Request, FermentationApplicationPreparedRequest::
                                           PreparedAcknowledgeMessage> ||
                          std::is_same_v<
                              Request, FermentationApplicationPreparedRequest::
                                           PreparedMuteMessage>) {
                request.request.envelope.confirmed = true;
            } else {
                request.envelope.confirmed = true;
            }
        },
        storage_);
}

FermentationApplicationPreparedRequest::FermentationApplicationPreparedRequest(
    Storage storage,
    std::optional<CrossRolePlausibilityContext> owningPlausibility)
    : storage_(std::move(storage)),
      owningPlausibility_(std::move(owningPlausibility)) {}

CommandEnvelope FermentationUiCommandBridge::makeEnvelope(
    const FermentationUiCommandContext& context,
    const ApplicationCommandIdentity& identity) noexcept {
    return {identity.commandId(),
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
    auto result = makeResult(categoryFor(status), status,
                             FermentationUiCommandPhase::DecisionOnly);
    if (status == CommandStatus::NotConfirmed && confirmation.has_value()) {
        result.confirmation = confirmation;
    }
    return result;
}

FermentationUiCommandResult
FermentationUiCommandBridge::fromOwningCommandApplyStatus(
    CommandStatus status) {
    return makeResult(categoryFor(status), status,
                      FermentationUiCommandPhase::OwningOutcome);
}

FermentationUiCommandResult FermentationUiCommandBridge::fromTransitionDecision(
    DecisionStatus status) {
    const auto category = status == DecisionStatus::Proposed
                              ? Category::Accepted
                              : Category::Rejected;
    return makeResult(category, status,
                      FermentationUiCommandPhase::DecisionOnly);
}

FermentationUiCommandResult
FermentationUiCommandBridge::fromRunPersistenceResult(
    RunPersistenceResultStatus status) {
    return makeResult(categoryFor(status), status,
                      FermentationUiCommandPhase::OwningOutcome);
}

FermentationUiCommandResult
FermentationUiCommandBridge::fromConfigurationPreview(
    ConfigurationPreviewStatus status) {
    return makeResult(categoryFor(status), status,
                      FermentationUiCommandPhase::OwningOutcome);
}

FermentationUiCommandResult
FermentationUiCommandBridge::fromConfigurationCommit(
    ConfigurationCommitStatus status, FermentationUiCommandPhase phase) {
    return makeResult(categoryFor(status), status, phase);
}

FermentationUiCommandResult
FermentationUiCommandBridge::fromNetworkConfigurationResult(
    NetworkConfigurationStatus status) {
    return makeResult(categoryFor(status), status,
                      FermentationUiCommandPhase::OwningOutcome);
}

FermentationUiCommandResult FermentationUiCommandBridge::fromFallbackResult(
    RunPersistenceResultStatus status) {
    return fromRunPersistenceResult(status);
}

FermentationUiCommandResult FermentationUiCommandBridge::commitConfiguration(
    ConfigurationService& service,
    const FermentationUiConfigurationCommitCommand& command,
    const std::optional<FermentationUiConfirmationRequest>& confirmation) {
    // ConfigurationService is the sole owner of preview basis, active
    // binding, current revision, and conflict validation.  It must run before
    // a UI confirmation response so stale state cannot be masked by an
    // unconfirmed request.
    const auto validation = service.validatePreviewForConfirmation(
        command.previewHandle, command.expectedUserConfigurationRevision);
    if (validation.status != ConfigurationCommitStatus::ReadyForConfirmation) {
        return fromConfigurationCommit(
            validation.status, FermentationUiCommandPhase::DecisionOnly);
    }
    if (!command.confirmed) {
        auto result =
            makeResult(Category::ConfirmationRequired,
                       ConfigurationCommitStatus::ReadyForConfirmation);
        result.confirmation = confirmation;
        return result;
    }
    return fromConfigurationCommit(
        service.confirmPreview(command.previewHandle).status,
        FermentationUiCommandPhase::OwningOutcome);
}

FermentationUiCommandResult FermentationUiCommandBridge::resumeFallback(
    FermentationApplication& application,
    const FermentationUiResumeFallbackCommand& command,
    const std::optional<FermentationUiConfirmationRequest>& confirmation) {
    const auto outcome = application.resumeFallback(command);
    // RecoveryPending is returned before activateFallbackRecoveredRun when
    // confirmation or the owning evidence is still missing.  It is therefore
    // a bridge/application pre-apply result even when the adapter sent a
    // confirmed command without the required evidence.  Do not infer an
    // owning outcome from the persistence status family.
    if (outcome.status == RunPersistenceResultStatus::RecoveryPending) {
        auto result = makeResult(Category::ConfirmationRequired, outcome.status,
                                 FermentationUiCommandPhase::DecisionOnly);
        if (!command.confirmed) result.confirmation = confirmation;
        return result;
    }
    if (outcome.status == RunPersistenceResultStatus::NotInitialized ||
        outcome.status == RunPersistenceResultStatus::StaleDecision ||
        outcome.status == RunPersistenceResultStatus::InvalidDecision) {
        return makeResult(categoryFor(outcome.status), outcome.status,
                          FermentationUiCommandPhase::DecisionOnly);
    }
    return fromFallbackResult(outcome.status);
}

FermentationUiCommandResult FermentationUiCommandBridge::applyNetworkMode(
    FermentationApplication& application,
    const FermentationUiApplyNetworkModeCommand& command) {
    return fromNetworkConfigurationResult(
        application.applyNetworkMode(command.selectedMode).status);
}

FermentationUiCommandResult
FermentationUiCommandBridge::beginHomeWifiReconfiguration(
    FermentationApplication& application,
    const FermentationUiBeginHomeWifiReconfigurationCommand&) {
    return fromNetworkConfigurationResult(
        application.beginHomeWifiReconfiguration().status);
}

FermentationUiCommandResult
FermentationUiCommandBridge::openWebProvisioningWindow(
    FermentationApplication& application,
    const FermentationUiOpenWebProvisioningWindowCommand&) {
    // Only the Application owner decides; "not opened" covers an already open
    // window as well as every state in which the release is not allowed.
    const bool opened = application.openWebProvisioningWindow();
    return makeResult(
        opened ? Category::Accepted : Category::Rejected,
        opened ? FermentationUiDetailStatus::WebProvisioningWindowOpened
               : FermentationUiDetailStatus::WebProvisioningWindowNotOpened,
        FermentationUiCommandPhase::OwningOutcome);
}

FermentationUiCommandResult FermentationUiCommandBridge::setDisplayLanguage(
    FermentationApplication& application,
    const FermentationUiSetDisplayLanguageCommand& command) {
    // The Application returns the owning preview status and, only if the
    // preview was accepted, the owning commit status; both map through the
    // existing configuration projections (no new detail type).
    const auto outcome = application.applyDisplayLanguage(
        command.languageId, command.expectedUserConfigurationRevision);
    if (outcome.preview != ConfigurationPreviewStatus::Success) {
        return fromConfigurationPreview(outcome.preview);
    }
    return fromConfigurationCommit(outcome.commit,
                                   FermentationUiCommandPhase::OwningOutcome);
}

FermentationUiCommandResult FermentationUiCommandBridge::setDeviceName(
    FermentationApplication& application,
    const FermentationUiSetDeviceNameCommand& command) {
    // Same coarse projection as the language change.
    const auto outcome = application.applyUserSettings(
        {command.deviceName}, command.expectedUserConfigurationRevision);
    if (outcome.preview != ConfigurationPreviewStatus::Success) {
        return fromConfigurationPreview(outcome.preview);
    }
    return fromConfigurationCommit(outcome.commit,
                                   FermentationUiCommandPhase::OwningOutcome);
}

FermentationUiCommandResult FermentationUiCommandBridge::applyProgramEdit(
    FermentationApplication& application,
    const FermentationUiProgramEditRequest& request,
    const std::optional<ProgramCatalogRevision>& expectedProgramCatalogRevision,
    const std::optional<UserConfigurationRevision>&
        expectedUserConfigurationRevision) {
    // Same coarse projection as the language change: the owning preview
    // status, and only if the preview was accepted, the owning commit status.
    const auto outcome =
        application.applyProgramEdit(request, expectedProgramCatalogRevision,
                                     expectedUserConfigurationRevision);
    if (outcome.preview != ConfigurationPreviewStatus::Success) {
        return fromConfigurationPreview(outcome.preview);
    }
    return fromConfigurationCommit(outcome.commit,
                                   FermentationUiCommandPhase::OwningOutcome);
}

FermentationUiCommandResult
FermentationUiCommandBridge::unsupportedAppDetail() {
    return makeResult(Category::Rejected,
                      FermentationUiDetailStatus::UnsupportedAppDetail,
                      FermentationUiCommandPhase::DecisionOnly);
}

FermentationUiCommandResult
FermentationUiCommandBridge::decideProductInsertedConfirmed(
    const RunCommandState& current, const ProcessRunSnapshot* runSnapshot,
    const FermentationUiCommandContext& context, const ProcessSignals& signals,
    std::uint64_t monotonicMillis, TransitionDecision* decisionOut) {
    if (context.expected.expectedStateSequence !=
        current.processState.transitionSequence) {
        return fromCommandStatus(CommandStatus::StaleState);
    }
    const TransitionRequest request{ProcessEvent::ProductInsertedConfirmed,
                                    std::nullopt};
    const auto decision = decideProcessTransition(
        current.processState, runSnapshot, signals, request, monotonicMillis);
    if (decisionOut != nullptr) *decisionOut = decision;
    return fromTransitionDecision(decision.status);
}

FermentationUiCommandResult FermentationUiCommandBridge::decidePrepared(
    const RunCommandState& current,
    const FermentationApplicationPreparedRequest& request,
    const std::optional<FermentationUiConfirmationRequest>& confirmation,
    CommandDecision* decisionOut) {
    return std::visit(
        [&current, &request, &confirmation, decisionOut](const auto& prepared) {
            using Request = std::decay_t<decltype(prepared)>;
            CommandDecision decision;
            if constexpr (std::is_same_v<Request, ProgramStartRequest>) {
                decision =
                    ::fermentation::decideProgramStart(current, prepared);
            } else if constexpr (std::is_same_v<Request, ManualStartRequest>) {
                decision = ::fermentation::decideManualStart(current, prepared);
            } else if constexpr (std::is_same_v<Request, StopRequest>) {
                decision = ::fermentation::decideStop(current, prepared);
            } else if constexpr (std::is_same_v<Request, CompletionRequest>) {
                decision = ::fermentation::decideCompletion(current, prepared);
            } else if constexpr (std::is_same_v<Request,
                                                RunAdjustmentCommandRequest>) {
                decision =
                    ::fermentation::decideRunAdjustment(current, prepared);
            } else if constexpr (std::is_same_v<
                                     Request,
                                     ApplyRecoveryTimeCorrectionRequest>) {
                decision = ::fermentation::decideApplyRecoveryTimeCorrection(
                    current, prepared);
            } else if constexpr (std::is_same_v<
                                     Request,
                                     FermentationApplicationPreparedRequest::
                                         PreparedAcknowledgeMessage>) {
                decision = ::fermentation::decideAcknowledgeMessage(
                    current, prepared.request);
            } else if constexpr (std::is_same_v<
                                     Request,
                                     FermentationApplicationPreparedRequest::
                                         PreparedMuteMessage>) {
                decision = ::fermentation::decideMuteMessage(current,
                                                             prepared.request);
            } else {
                if (!request.owningPlausibility().has_value())
                    return FermentationUiCommandBridge::unsupportedAppDetail();
                decision = decideApplySensorSelectionAction(
                    current, prepared, *request.owningPlausibility());
            }
            if (decisionOut != nullptr) *decisionOut = decision;
            return FermentationUiCommandBridge::fromCommandStatus(
                decision.status, confirmation);
        },
        request.storage());
}

bool hasStartCandidateOverride(
    const FermentationUiStartCandidate& candidate) noexcept {
    return candidate.targetTemperatureCelsius.has_value() ||
           candidate.fermentationDurationMinutes.has_value() ||
           candidate.preheatEnabled.has_value() ||
           candidate.completionMode.has_value() ||
           candidate.coolingTargetCelsius.has_value() ||
           candidate.holdDurationMinutes.has_value();
}

void applyStartCandidateOverrides(
    ProgramDocument& program, const FermentationUiStartCandidate& candidate) {
    auto& definition = program.program;
    if (candidate.targetTemperatureCelsius.has_value() &&
        !definition.fermentationStages.empty()) {
        definition.fermentationStages.front().targetTemperatureCelsius =
            candidate.targetTemperatureCelsius;
    }
    if (candidate.fermentationDurationMinutes.has_value() &&
        !definition.fermentationStages.empty()) {
        definition.fermentationStages.front().durationMinutes =
            candidate.fermentationDurationMinutes;
    }
    if (candidate.preheatEnabled.has_value()) {
        definition.preheat = *candidate.preheatEnabled;
    }
    if (candidate.completionMode.has_value()) {
        definition.completion.mode = *candidate.completionMode;
        if (*candidate.completionMode == CompletionMode::FinishWithoutCooling) {
            definition.completion.coolingTargetCelsius.reset();
            definition.completion.holdDurationMinutes.reset();
        } else if (*candidate.completionMode ==
                       CompletionMode::CoolThenFinish ||
                   *candidate.completionMode ==
                       CompletionMode::CoolAndHoldUntilManualStop) {
            definition.completion.holdDurationMinutes.reset();
        }
    }
    if (candidate.coolingTargetCelsius.has_value()) {
        definition.completion.coolingTargetCelsius =
            candidate.coolingTargetCelsius;
    }
    if (candidate.holdDurationMinutes.has_value()) {
        definition.completion.holdDurationMinutes =
            candidate.holdDurationMinutes;
    }
}

}  // namespace fermentation
