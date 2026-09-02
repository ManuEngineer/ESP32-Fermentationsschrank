#include "fermentation_ui_projector.hpp"

namespace fermentation {

FermentationUiSnapshot FermentationUiProjector::project(
    const FermentationUiProjectionInput& input) {
    FermentationUiSnapshot output;
    output.revisions = input.revisions;
    output.temperatures = input.temperatures;
    output.messages = input.messages;
    output.navigation.semanticActions = input.semanticActions;
    output.status = input.status;
    output.service = input.service;
    output.home.primaryAction = input.primaryAction.value_or(
        device_platform::TextKey{});

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
                output.recovery.mode = RecoveryViewMode::RecoveryRejectedOrFailClosed;
                break;
        }
    }
    if (input.persistenceLoadStatus.has_value() &&
        *input.persistenceLoadStatus == RunPersistenceLoadStatus::FallbackRecovered &&
        input.coordinatorState.has_value() &&
        *input.coordinatorState == RunPersistenceCoordinatorState::FallbackRecoveryPending) {
        output.recovery.mode = RecoveryViewMode::FallbackSelectionRequired;
    }
    if (input.refreshTracker != nullptr) {
        output.refreshRevision = input.refreshTracker->publish(
            input.semanticPublicationRevision);
    }
    return output;
}

}  // namespace fermentation
