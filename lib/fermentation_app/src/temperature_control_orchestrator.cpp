#include "temperature_control_orchestrator.hpp"

#include "control_context.hpp"
#include "run_recovery.hpp"

namespace fermentation {

bool consumeCommittedControlContextTransition(
    RunPersistenceResult& persisted, TemperatureController& controller) {
    if (persisted.status != RunPersistenceResultStatus::Applied ||
        !persisted.committedControlContextTransition.has_value()) {
        return false;
    }
    const auto transition = *persisted.committedControlContextTransition;
    // A failed mark is also consumed: a committed hint must never be
    // reinterpreted later as a second successful handoff.
    persisted.committedControlContextTransition.reset();
    return controller.markCommittedControlContextTransitionPending(transition);
}

void resetTemperatureControlAtBoundary(
    TemperatureController& controller, TargetQualificationEvaluator& evaluator,
    TemperatureControlLifecycleBoundary boundary) {
    switch (boundary) {
        case TemperatureControlLifecycleBoundary::NewActiveRun:
        case TemperatureControlLifecycleBoundary::LeaveTemperatureControl:
        case TemperatureControlLifecycleBoundary::Recovery:
        case TemperatureControlLifecycleBoundary::Fault:
        case TemperatureControlLifecycleBoundary::SafeBoot:
        case TemperatureControlLifecycleBoundary::Service:
        case TemperatureControlLifecycleBoundary::Standby:
            controller.resetRuntime();
            evaluator.reset();
            return;
    }
}

TemperatureControlApplicationOrchestrator::
    TemperatureControlApplicationOrchestrator(
        RunPersistenceCoordinator& persistence,
        TemperatureController& temperatureController,
        TargetQualificationEvaluator& evaluator) noexcept
    : persistence_(persistence),
      temperatureController_(temperatureController),
      evaluator_(evaluator) {}

RunPersistenceResult TemperatureControlApplicationOrchestrator::persistCommand(
    RunCommandState& current, const CommandDecision& decision,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    const auto before = current;
    return complete(persistence_.persistCommand(current, decision, time,
                                                liveSensorEvidence),
                    before, current);
}

RunPersistenceResult
TemperatureControlApplicationOrchestrator::persistTransition(
    RunCommandState& current, const TransitionDecision& decision,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    const auto before = current;
    return complete(persistence_.persistTransition(current, decision, time,
                                                   liveSensorEvidence),
                    before, current);
}

RunPersistenceResult
TemperatureControlApplicationOrchestrator::persistSensorSelection(
    RunCommandState& current, const SensorSelectionStateMutation& mutation,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    const auto before = current;
    return complete(persistence_.persistSensorSelection(current, mutation, time,
                                                        liveSensorEvidence),
                    before, current);
}

RunPersistenceResult
TemperatureControlApplicationOrchestrator::activateRecovery(
    RunRecoveryCoordinator& recovery, RunCommandState& current,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    const auto before = current;
    return complete(
        recovery.activate(persistence_, current, time, liveSensorEvidence),
        before, current, true);
}

RunPersistenceResult
TemperatureControlApplicationOrchestrator::resolveRecoveryOutcome(
    RunCommandState& current, const ResolveRecoveryUncertaintyRequest& request,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    const auto before = current;
    return complete(persistence_.resolveRecoveryOutcome(current, request, time,
                                                        liveSensorEvidence),
                    before, current);
}

RunPersistenceResult
TemperatureControlApplicationOrchestrator::reevaluateRecoveryEvaluation(
    RunCommandState& current, const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    const auto before = current;
    return complete(persistence_.reevaluateRecoveryEvaluation(
                        current, time, liveSensorEvidence),
                    before, current);
}

RunPersistenceResult TemperatureControlApplicationOrchestrator::complete(
    RunPersistenceResult result, const RunCommandState& before,
    RunCommandState& current, bool recoveryBoundary) {
    if (result.status != RunPersistenceResultStatus::Applied) return result;

    // This is the sole production handoff boundary. The persistence
    // coordinator never owns PI state, and a failed result cannot reach it.
    static_cast<void>(consumeCommittedControlContextTransition(
        result, temperatureController_));

    if (needsRuntimeReset(before, current, recoveryBoundary)) {
        TemperatureControlLifecycleBoundary boundary =
            TemperatureControlLifecycleBoundary::LeaveTemperatureControl;
        if (recoveryBoundary ||
            before.processState.state == ProcessState::RecoveryEvaluation ||
            current.processState.state == ProcessState::RecoveryEvaluation) {
            boundary = TemperatureControlLifecycleBoundary::Recovery;
        } else if (!isTemperatureControlledProcessState(
                       before.processState.state) &&
                   isTemperatureControlledProcessState(
                       current.processState.state)) {
            boundary = TemperatureControlLifecycleBoundary::NewActiveRun;
        }
        resetTemperatureControlAtBoundary(temperatureController_, evaluator_,
                                          boundary);
    }
    return result;
}

TemperatureControlResult
TemperatureControlApplicationOrchestrator::evaluateTemperatureControl(
    const RunCommandState& current,
    const TemperatureControlEvaluationEvidence& evidence) {
    const auto context = resolveEffectiveControlContext(current);
    if (!context.valid) {
        // Not temperature-controlled or structurally inconsistent (Run-/
        // Snapshot-Widerspruch): fail-closed, no ControlRequest. The
        // TemperatureController runtime itself was already reset at the
        // lifecycle boundary that left temperature control, so it is left
        // untouched here.
        TemperatureControlResult result;
        result.status = TemperatureControlStatus::InvalidInput;
        result.reason = TemperatureControlReason::InvalidConfiguration;
        result.airLimitState = AirLimitState::Unavailable;
        result.direction = AbstractControlDirection::Idle;
        return result;
    }

    TemperatureControlInput input;
    input.sampleTimestampMonotonicMillis =
        evidence.sampleTimestampMonotonicMillis;
    input.targetCelsius = context.target.targetCelsius;
    input.controlSensorRole = context.controlSensorRole;
    input.air = evidence.air;
    input.product = evidence.product;
    input.previousControlRequestFeedback =
        evidence.previousControlRequestFeedback;
    input.processTransitionSequence =
        context.requestContext.processTransitionSequence;
    input.runRevision = context.requestContext.runRevision;
    return temperatureController_.evaluate(input);
}

bool TemperatureControlApplicationOrchestrator::needsRuntimeReset(
    const RunCommandState& before, const RunCommandState& after,
    bool recoveryBoundary) const {
    if (recoveryBoundary) return true;
    if (before.processState.state == ProcessState::RecoveryEvaluation ||
        after.processState.state == ProcessState::RecoveryEvaluation) {
        return true;
    }

    const bool beforeControlled =
        isTemperatureControlledProcessState(before.processState.state);
    const bool afterControlled =
        isTemperatureControlledProcessState(after.processState.state);
    if (!beforeControlled && afterControlled) return true;
    if (beforeControlled && !afterControlled) return true;

    switch (after.processState.state) {
        case ProcessState::Fault:
        case ProcessState::SafeBoot:
        case ProcessState::ServiceMode:
        case ProcessState::Standby:
            return before.processState.state != after.processState.state;
        default:
            return false;
    }
}

}  // namespace fermentation
