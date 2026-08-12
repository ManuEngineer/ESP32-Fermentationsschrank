#include "qualification_orchestrator.hpp"

namespace fermentation {
namespace {

TargetQualificationCommitContext commitContext(
    const TargetQualificationInput& input) {
    return {{input.targetCelsius, input.bandCelsius, input.controlSensorRole},
            input.runRevision,
            input.processTransitionSequence};
}

bool phaseMatchesProcessState(QualificationPhase phase, ProcessState state) {
    if (phase == QualificationPhase::Preheating)
        return state == ProcessState::Preheating;
    if (phase == QualificationPhase::Target)
        return state == ProcessState::ReachingTarget ||
               state == ProcessState::QualifyingTarget;
    return false;
}

TargetQualificationOrchestrationResult discarded(
    TargetQualificationEvaluator& evaluator,
    TargetQualificationResult& qualification,
    TargetQualificationOrchestrationStatus status) {
    evaluator.discard(qualification);
    TargetQualificationOrchestrationResult result;
    result.qualification = qualification;
    result.status = status;
    return result;
}

}  // namespace

TargetQualificationOrchestrationResult evaluateAndApplyTargetQualification(
    TargetQualificationEvaluator& evaluator, RunCommandState& current,
    RunPersistenceCoordinator& persistence,
    const TargetQualificationInput& input, const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    auto qualification = evaluator.evaluate(input);
    const auto context = commitContext(input);
    if (!validQualificationPhase(input.phase) ||
        !phaseMatchesProcessState(input.phase, current.processState.state)) {
        return discarded(
            evaluator, qualification,
            TargetQualificationOrchestrationStatus::InvalidDecision);
    }
    if (input.runRevision != current.runRevision ||
        input.processTransitionSequence !=
            current.processState.transitionSequence) {
        return discarded(evaluator, qualification,
                         TargetQualificationOrchestrationStatus::StaleDecision);
    }

    TargetQualificationOrchestrationResult result;
    result.qualification = qualification;
    result.signals.qualificationProgress = qualification.progress;
    const auto processDecision = decideProcessTransition(
        current.processState,
        current.processRunSnapshot.has_value() ? &*current.processRunSnapshot
                                               : nullptr,
        result.signals, TransitionRequest{}, time.monotonicMillis);
    result.processDecision = processDecision;
    if (!processDecision.proposed()) {
        if (processDecision.status != DecisionStatus::NoTransition) {
            return discarded(
                evaluator, qualification,
                TargetQualificationOrchestrationStatus::InvalidDecision);
        }
        if (!evaluator.applyRamOnly(qualification, context)) {
            result.qualification = qualification;
            result.status =
                TargetQualificationOrchestrationStatus::StaleDecision;
            return result;
        }
        result.qualification = qualification;
        result.status = TargetQualificationOrchestrationStatus::AppliedRamOnly;
        return result;
    }

    const auto persisted = persistence.persistTransition(
        current, processDecision, time, liveSensorEvidence);
    result.persistenceStatus = persisted.status;
    if (persisted.status != RunPersistenceResultStatus::Applied) {
        evaluator.discard(qualification);
        result.qualification = qualification;
        result.status =
            TargetQualificationOrchestrationStatus::PersistenceFailed;
        return result;
    }

    const auto committedContext = TargetQualificationCommitContext{
        context.qualification, current.runRevision,
        current.processState.transitionSequence};
    if (!evaluator.applyAfterPersistedProcessApply(qualification, context,
                                                   committedContext)) {
        result.qualification = qualification;
        result.status = TargetQualificationOrchestrationStatus::StaleDecision;
        return result;
    }
    if (processDecision.reason == TransitionReason::PreheatQualified ||
        processDecision.reason == TransitionReason::TargetQualified) {
        // Leaving the qualification domain ends the current episode. This is
        // deliberately RAM-only; the process marker remains owned by the
        // state machine and no evaluator state is persisted.
        evaluator.reset();
    }
    result.qualification = qualification;
    result.status = TargetQualificationOrchestrationStatus::AppliedPersisted;
    return result;
}

}  // namespace fermentation
