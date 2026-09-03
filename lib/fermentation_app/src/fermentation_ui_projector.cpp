#include "fermentation_ui_projector.hpp"

namespace fermentation {

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
    output.status.ready = input.application.ready;
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
            output.home.mode = FermentationHomeMode::ActiveRun;
            output.home.effectiveValues =
                state.activeProgramRun->effectiveValues();
        } else if (state.activeManualRun.has_value()) {
            output.home.mode = FermentationHomeMode::ActiveRun;
        } else {
            output.home.mode = state.processState.state == ProcessState::Fault
                                   ? FermentationHomeMode::ServiceRequired
                                   : FermentationHomeMode::Standby;
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
    // Recovery is an owning application state, not a renderer route.  Keep
    // the home projection in Recovery whenever the canonical recovery
    // disposition/evaluation or the selected-fallback gate is active.
    if (input.recoveryDisposition.has_value() ||
        (input.runState != nullptr && input.runState->processState.state ==
                                          ProcessState::RecoveryEvaluation) ||
        output.recovery.mode == RecoveryViewMode::FallbackSelectionRequired) {
        output.home.mode = FermentationHomeMode::Recovery;
    }
    if (input.refreshTracker != nullptr) {
        output.refreshRevision = input.refreshTracker->publish(output);
    }
    return output;
}

}  // namespace fermentation
