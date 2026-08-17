#include "temperature_control_orchestrator.hpp"

#include <utility>

#include "control_context.hpp"
#include "run_recovery.hpp"
#include "safety_core.hpp"

namespace fermentation {

namespace {

bool hasEffect(const RunPersistenceResult& result, CommandEffect effect) {
    if (result.effectCount > result.effects.size()) return false;
    for (std::size_t index = 0U; index < result.effectCount; ++index) {
        if (result.effects[index] == effect) return true;
    }
    return false;
}

bool qualificationEpisodeDomain(ProcessState state) {
    return state == ProcessState::Preheating ||
           state == ProcessState::ReachingTarget ||
           state == ProcessState::QualifyingTarget;
}

}  // namespace

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

void resetActuatorPlanAtBoundary(
    ActuatorPlanner& planner, ActuatorPlanSinkDriver& driver,
    TemperatureControlLifecycleBoundary /*boundary*/,
    std::uint64_t nowMonotonicMillis,
    std::optional<PreviousControlRequestFeedback>& applicationPendingFeedback,
    std::optional<TemperatureControlResult>& outstandingEvaluation) {
    const auto episodeAtStop =
        outstandingEvaluation.has_value()
            ? ActuatorFeedbackEpisodeAtStop::ClosedByOutstandingEvaluation
            : ActuatorFeedbackEpisodeAtStop::ExistingEpisodeOpen;
    const ActuatorPlanTickResult result =
        planner.forceStop(nowMonotonicMillis, episodeAtStop);
    driver.apply(result);
    static_cast<void>(planner.takeFeedbackUpdate());
    applicationPendingFeedback.reset();
    outstandingEvaluation.reset();
}

TemperatureControlApplicationOrchestrator::
    TemperatureControlApplicationOrchestrator(
        RunPersistenceCoordinator& persistence,
        TemperatureController& temperatureController,
        TargetQualificationEvaluator& evaluator) noexcept
    : persistence_(persistence),
      temperatureController_(temperatureController),
      evaluator_(evaluator) {}

TemperatureControlApplicationOrchestrator::
    TemperatureControlApplicationOrchestrator(
        RunPersistenceCoordinator& persistence,
        TemperatureController& temperatureController,
        TargetQualificationEvaluator& evaluator, ActuatorPlanner& planner,
        ActuatorPlanSinkDriver& driver, SafetyCore& safetyCore) noexcept
    : persistence_(persistence),
      temperatureController_(temperatureController),
      evaluator_(evaluator),
      planner_(&planner),
      actuatorDriver_(&driver),
      safetyCore_(&safetyCore) {}

RunPersistenceResult TemperatureControlApplicationOrchestrator::persistCommand(
    RunCommandState& current, const CommandDecision& decision,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    const TemperatureControlLifecycleSnapshot before{
        current.processState.state};
    return complete(persistence_.persistCommand(current, decision, time,
                                                liveSensorEvidence),
                    before, current, time.monotonicMillis);
}

RunPersistenceResult
TemperatureControlApplicationOrchestrator::persistTransition(
    RunCommandState& current, const TransitionDecision& decision,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    const TemperatureControlLifecycleSnapshot before{
        current.processState.state};
    return complete(persistence_.persistTransition(current, decision, time,
                                                   liveSensorEvidence),
                    before, current, time.monotonicMillis);
}

RunPersistenceResult
TemperatureControlApplicationOrchestrator::persistSensorSelection(
    RunCommandState& current, const SensorSelectionStateMutation& mutation,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    const TemperatureControlLifecycleSnapshot before{
        current.processState.state};
    return complete(persistence_.persistSensorSelection(current, mutation, time,
                                                        liveSensorEvidence),
                    before, current, time.monotonicMillis);
}

RunPersistenceResult
TemperatureControlApplicationOrchestrator::activateRecovery(
    RunRecoveryCoordinator& recovery, RunCommandState& current,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    const TemperatureControlLifecycleSnapshot before{
        current.processState.state};
    return complete(
        recovery.activate(persistence_, current, time, liveSensorEvidence),
        before, current, time.monotonicMillis, true);
}

RunPersistenceResult
TemperatureControlApplicationOrchestrator::resolveRecoveryOutcome(
    RunCommandState& current, const ResolveRecoveryUncertaintyRequest& request,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    const TemperatureControlLifecycleSnapshot before{
        current.processState.state};
    return complete(persistence_.resolveRecoveryOutcome(current, request, time,
                                                        liveSensorEvidence),
                    before, current, time.monotonicMillis);
}

RunPersistenceResult
TemperatureControlApplicationOrchestrator::reevaluateRecoveryEvaluation(
    RunCommandState& current, const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    const TemperatureControlLifecycleSnapshot before{
        current.processState.state};
    return complete(persistence_.reevaluateRecoveryEvaluation(
                        current, time, liveSensorEvidence),
                    before, current, time.monotonicMillis);
}

RunPersistenceResult TemperatureControlApplicationOrchestrator::complete(
    RunPersistenceResult result,
    const TemperatureControlLifecycleSnapshot& before, RunCommandState& current,
    std::uint64_t nowMonotonicMillis, bool recoveryBoundary) {
    if (result.status != RunPersistenceResultStatus::Applied) return result;

    const bool newActiveRun = hasEffect(result, CommandEffect::RunStarted);
    std::optional<CommittedControlContextTransition> committedTransition;
    if (result.committedControlContextTransition.has_value()) {
        committedTransition = *result.committedControlContextTransition;
    }

    // This is the sole production handoff boundary. The persistence
    // coordinator never owns PI state, and a failed result cannot reach it.
    static_cast<void>(consumeCommittedControlContextTransition(
        result, temperatureController_));

    if (needsRuntimeReset(before, current.processState.state, recoveryBoundary,
                          newActiveRun)) {
        TemperatureControlLifecycleBoundary boundary =
            TemperatureControlLifecycleBoundary::LeaveTemperatureControl;
        if (newActiveRun) {
            boundary = TemperatureControlLifecycleBoundary::NewActiveRun;
        } else if (recoveryBoundary ||
                   before.processState == ProcessState::RecoveryEvaluation ||
                   current.processState.state ==
                       ProcessState::RecoveryEvaluation) {
            boundary = TemperatureControlLifecycleBoundary::Recovery;
        } else if (!isTemperatureControlledProcessState(before.processState) &&
                   isTemperatureControlledProcessState(
                       current.processState.state)) {
            boundary = TemperatureControlLifecycleBoundary::NewActiveRun;
        }
        resetTemperatureControlAtBoundary(temperatureController_, evaluator_,
                                          boundary);
        if (planner_ != nullptr && actuatorDriver_ != nullptr) {
            resetActuatorPlanAtBoundary(
                *planner_, *actuatorDriver_, boundary, nowMonotonicMillis,
                pendingControlRequestFeedback_, outstandingEvaluation_);
        }
    }
    if (committedTransition.has_value() && !newActiveRun &&
        (committedTransition ==
             CommittedControlContextTransition::TargetContextChange ||
         committedTransition ==
             CommittedControlContextTransition::SensorRoleChange ||
         committedTransition ==
             CommittedControlContextTransition::ProductInserted) &&
        (qualificationEpisodeDomain(before.processState) ||
         qualificationEpisodeDomain(current.processState.state))) {
        // A successful target/role/product context commit ends the old
        // target episode immediately. The process marker is owned by the
        // state-machine transition; evaluator RAM is reset here at the same
        // successful application boundary.
        evaluator_.reset();
    }
    return result;
}

TemperatureControlResult
TemperatureControlApplicationOrchestrator::evaluateTemperatureControl(
    const RunCommandState& current,
    const TemperatureControlEvaluationEvidence& evidence) {
    if (planner_ != nullptr && outstandingEvaluation_.has_value()) {
        // A second evaluation before planner consumption is not a new input;
        // fail closed without disturbing the pending result or feedback slot.
        TemperatureControlResult result;
        result.status = TemperatureControlStatus::InvalidInput;
        result.reason = TemperatureControlReason::InvalidConfiguration;
        result.airLimitState = AirLimitState::Unavailable;
        result.direction = AbstractControlDirection::Idle;
        return result;
    }

    if (planner_ == nullptr) {
        // Owner-Review F4: without a planner there is no #23 feedback
        // handoff, ever - not just on the first call. Running #22's PI
        // core here would silently freeze its integrator on every active
        // evaluation after the first, indistinguishable from a real
        // downstream limitation. Mirrors tickActuatorPlan()'s plannerless
        // Unconfigured/NoCommissioning response; no active ControlRequest is
        // produced. The 3-argument constructor remains valid for fixtures
        // that never call this method.
        TemperatureControlResult result;
        result.status = TemperatureControlStatus::Unavailable;
        result.reason = TemperatureControlReason::NoCommissioning;
        result.airLimitState = AirLimitState::Unavailable;
        result.direction = AbstractControlDirection::Idle;
        return result;
    }

    const auto context = resolveEffectiveControlContext(current);
    TemperatureControlResult result;
    if (!context.valid) {
        // Not temperature-controlled or structurally inconsistent (Run-/
        // Snapshot-Widerspruch): fail-closed, no ControlRequest. A
        // structurally inconsistent state may still remain in a
        // temperature-controlled phase, so clear all volatile PI state here
        // as well. TemperatureController::resetRuntime() retains the request
        // sequence high-watermark.
        temperatureController_.resetRuntime();
        result.status = TemperatureControlStatus::InvalidInput;
        result.reason = TemperatureControlReason::InvalidConfiguration;
        result.airLimitState = AirLimitState::Unavailable;
        result.direction = AbstractControlDirection::Idle;
    } else {
        TemperatureControlInput input;
        input.sampleTimestampMonotonicMillis =
            evidence.sampleTimestampMonotonicMillis;
        input.targetCelsius = context.target.targetCelsius;
        input.controlSensorRole = context.controlSensorRole;
        input.air = evidence.air;
        input.product = evidence.product;
        input.previousControlRequestFeedback =
            std::exchange(pendingControlRequestFeedback_, std::nullopt);
        input.processTransitionSequence =
            context.requestContext.processTransitionSequence;
        input.runRevision = context.requestContext.runRevision;
        result = temperatureController_.evaluate(input);
    }

    outstandingEvaluation_ = result;
    // Closing the prior feedback episode happens when the result becomes
    // outstanding, before a lifecycle stop can inspect acceptedCommand.
    planner_->closeFeedbackEpisodeForOutstandingEvaluation();
    return result;
}

ActuatorPlanTickResult
TemperatureControlApplicationOrchestrator::tickActuatorPlan(
    const RunCommandState& current, std::uint64_t nowMonotonicMillis) {
    if (planner_ == nullptr || actuatorDriver_ == nullptr ||
        safetyCore_ == nullptr) {
        ActuatorPlanTickResult result;
        result.status = ActuatorPlanStatus::Unconfigured;
        result.reason = ActuatorPlanReason::NoCommissioning;
        result.appliedDirection = AbstractControlDirection::Idle;
        return result;
    }

    const ActuatorSafetyGateInput safetyGate =
        safetyCore_->lastEvaluation().gate;

    const EffectiveControlContext context =
        resolveEffectiveControlContext(current);
    ActuatorPlanTickInput input;
    input.nowMonotonicMillis = nowMonotonicMillis;
    input.currentCanonicalContext = context.requestContext;
    input.temperatureControlledPhase =
        isTemperatureControlledProcessState(current.processState.state);
    input.safetyGate = safetyGate;
    if (!context.valid) {
        // No valid effective context may accidentally keep the physical gate
        // open when a caller omitted a fresh #22 evaluation.
        input.safetyGate.status = ActuatorSafetyGateStatus::Unresolved;
    }
    if (outstandingEvaluation_.has_value()) {
        input.newEvaluation = std::move(outstandingEvaluation_);
        outstandingEvaluation_.reset();
    }

    ActuatorPlanTickResult result = planner_->tick(input);
    actuatorDriver_->apply(result);
    const PendingControlRequestFeedbackUpdate update =
        planner_->takeFeedbackUpdate();
    if (update.changed) pendingControlRequestFeedback_ = update.feedback;
    return result;
}

bool TemperatureControlApplicationOrchestrator::needsRuntimeReset(
    const TemperatureControlLifecycleSnapshot& before, ProcessState after,
    bool recoveryBoundary, bool newActiveRun) const {
    if (newActiveRun) return true;
    if (recoveryBoundary) return true;
    if (before.processState == ProcessState::RecoveryEvaluation ||
        after == ProcessState::RecoveryEvaluation) {
        return true;
    }

    const bool beforeControlled =
        isTemperatureControlledProcessState(before.processState);
    const bool afterControlled = isTemperatureControlledProcessState(after);
    if (!beforeControlled && afterControlled) return true;
    if (beforeControlled && !afterControlled) return true;

    switch (after) {
        case ProcessState::Fault:
        case ProcessState::SafeBoot:
        case ProcessState::ServiceMode:
        case ProcessState::Standby:
            return before.processState != after;
        default:
            return false;
    }
}

}  // namespace fermentation
