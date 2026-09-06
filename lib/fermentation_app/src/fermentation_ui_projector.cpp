#include "fermentation_ui_projector.hpp"

#include "application_lifecycle.hpp"

namespace fermentation {

namespace {

bool hasCanonicalDecisionRequiredMessage(const RunCommandState& state) {
    for (std::size_t i = 0U; i < state.messageCount; ++i) {
        const auto& message = state.messages[i];
        if (!message.active || message.resolved || !message.decisionRequired ||
            message.messageClass != MessageClass::DecisionRequired) {
            continue;
        }
        if (message.code == MessageCode::UserDecisionRequired ||
            message.code == MessageCode::ProductInsertionRequested) {
            return true;
        }
    }
    return false;
}

}  // namespace

FermentationUiSnapshot FermentationUiProjector::project(
    const FermentationUiProjectionInput& input) {
    FermentationUiSnapshot output;
    output.revisions = input.revisions;
    output.temperatures.reserve(input.temperatures.size());
    for (const auto& source : input.temperatures) {
        output.temperatures.push_back(
            TemperatureView{source.role, source.valueCelsius, source.quality});
    }
    if (input.runState != nullptr) {
        output.messages.reserve(input.runState->messageCount);
        for (std::size_t i = 0U; i < input.runState->messageCount; ++i) {
            output.messages.push_back(MessageView{input.runState->messages[i]});
        }
    }
    output.navigation.semanticActions = input.semanticActions;
    output.status.presentation = input.application.presentation;
    output.status.ready =
        input.application.lifecycleState == ApplicationLifecycleState::Ready;
    output.service.available = input.service.available;
    output.service.confirmationRequired = input.service.confirmationRequired;
    output.service.serviceAuthorizationRequired =
        input.service.serviceAuthorizationRequired;
    output.service.unavailableReason = input.service.unavailableReason;
    output.home.primaryAction =
        input.primaryAction.value_or(device_platform::TextKey{});

    if (input.runState != nullptr) {
        const auto& state = *input.runState;
        output.home.processState = state.processState.state;
        output.home.activeRunId = state.activeRunId;
        if (state.activeProgramRun.has_value()) {
            output.home.effectiveValues =
                state.activeProgramRun->effectiveValues();
        } else if (state.activeManualRun.has_value()) {
        }
    }
    output.recovery.canonicalRecoveryDisposition = input.recoveryDisposition;
    output.recovery.persistenceLoadStatus = input.persistenceLoadStatus;
    output.recovery.coordinatorState = input.coordinatorState;
    if (input.recoveryDisposition.has_value()) {
        switch (*input.recoveryDisposition) {
            case RecoveryDisposition::WaitingForTrustedTime:
                output.recovery.mode = RecoveryViewMode::WaitingForTrustedTime;
                break;
            case RecoveryDisposition::CurrentRunRecoverable:
                output.recovery.mode = RecoveryViewMode::CurrentRunRecovered;
                break;
            case RecoveryDisposition::RecoveryRejectedOrFailClosed:
                output.recovery.mode =
                    RecoveryViewMode::RecoveryRejectedOrFailClosed;
                break;
        }
    }
    if (input.runState != nullptr) {
        switch (input.runState->processState.state) {
            case ProcessState::Completed:
                output.recovery.mode = RecoveryViewMode::Completed;
                break;
            case ProcessState::Cooling:
            case ProcessState::CoolHolding:
                output.recovery.mode = RecoveryViewMode::Cooling;
                break;
            default:
                break;
        }
    }
    if (input.persistenceLoadStatus.has_value() &&
        *input.persistenceLoadStatus ==
            RunPersistenceLoadStatus::FallbackRecovered &&
        input.coordinatorState.has_value() &&
        *input.coordinatorState ==
            RunPersistenceCoordinatorState::FallbackRecoveryPending) {
        output.recovery.mode = RecoveryViewMode::FallbackSelectionRequired;
    }
    const auto lifecycle = input.application.lifecycleState;
    if (lifecycle == ApplicationLifecycleState::Initializing) {
        output.home.mode = FermentationHomeMode::Unavailable;
    } else if (lifecycle == ApplicationLifecycleState::ServiceRequired) {
        output.home.mode = FermentationHomeMode::Restricted;
    } else if (input.runState == nullptr) {
        output.home.mode = FermentationHomeMode::Unavailable;
    } else {
        const auto processState = input.runState->processState.state;
        const bool recoveryActive =
            input.recoveryDisposition.has_value() ||
            processState == ProcessState::RecoveryEvaluation ||
            output.recovery.mode == RecoveryViewMode::FallbackSelectionRequired;
        if (recoveryActive) {
            output.home.mode = FermentationHomeMode::Recovery;
        } else {
            const bool decisionRequired =
                hasCanonicalDecisionRequiredMessage(*input.runState);
            switch (processState) {
                case ProcessState::Boot:
                case ProcessState::SafeBoot:
                case ProcessState::Fault:
                case ProcessState::ServiceMode:
                    output.home.mode = FermentationHomeMode::Restricted;
                    break;
                case ProcessState::Completed:
                    output.home.mode = FermentationHomeMode::Completed;
                    break;
                case ProcessState::WaitingForProduct:
                    output.home.mode = FermentationHomeMode::Waiting;
                    break;
                case ProcessState::Preheating:
                case ProcessState::ReachingTarget:
                case ProcessState::QualifyingTarget:
                case ProcessState::Fermenting:
                case ProcessState::Cooling:
                case ProcessState::CoolHolding:
                    output.home.mode = decisionRequired
                                           ? FermentationHomeMode::Waiting
                                           : FermentationHomeMode::ActiveRun;
                    break;
                case ProcessState::ManualHolding:
                    output.home.mode = decisionRequired
                                           ? FermentationHomeMode::Waiting
                                           : FermentationHomeMode::ActiveRun;
                    break;
                case ProcessState::Standby: {
                    output.home.mode = decisionRequired
                                           ? FermentationHomeMode::Waiting
                                           : FermentationHomeMode::Standby;
                    break;
                }
                case ProcessState::RecoveryEvaluation:
                    output.home.mode = FermentationHomeMode::Recovery;
                    break;
            }
        }
    }
    if (input.refreshTracker != nullptr) {
        output.refreshRevision = input.refreshTracker->publish(output);
    }
    return output;
}

}  // namespace fermentation
