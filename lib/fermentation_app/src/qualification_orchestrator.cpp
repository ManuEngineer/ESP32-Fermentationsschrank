#include "qualification_orchestrator.hpp"

#include "control_context.hpp"
#include "temperature_control_orchestrator.hpp"

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

bool matchesCanonicalControlContext(const RunCommandState& current,
                                    const TargetQualificationInput& input) {
    const auto context = resolveEffectiveQualificationContext(current);
    return context.valid && context.targetCelsius == input.targetCelsius &&
           context.controlSensorRole == input.controlSensorRole &&
           context.bandCelsius == input.bandCelsius &&
           context.qualificationDurationMillis ==
               input.qualificationDurationMillis;
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
    TemperatureControlApplicationOrchestrator& application,
    RunCommandState& current, const TargetQualificationInput& input,
    const RunCheckpointTime& time, const ProcessSignals& baselineSignals,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    auto& evaluator = application.qualificationEvaluator();
    auto qualification = evaluator.evaluate(input);
    const auto context = commitContext(input);
    if (!validQualificationPhase(input.phase) ||
        !phaseMatchesProcessState(input.phase, current.processState.state) ||
        !matchesCanonicalControlContext(current, input)) {
        return discarded(
            evaluator, qualification,
            TargetQualificationOrchestrationStatus::InvalidDecision);
    }
    if (input.sampleTimestampMonotonicMillis != time.monotonicMillis) {
        return discarded(evaluator, qualification,
                         TargetQualificationOrchestrationStatus::StaleDecision);
    }
    if (input.runRevision != current.runRevision ||
        input.processTransitionSequence !=
            current.processState.transitionSequence) {
        return discarded(evaluator, qualification,
                         TargetQualificationOrchestrationStatus::StaleDecision);
    }

    TargetQualificationOrchestrationResult result;
    result.qualification = qualification;
    result.signals = baselineSignals;
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

    auto persisted = application.persistTransition(current, processDecision,
                                                   time, liveSensorEvidence);
    result.persistenceStatus = persisted.status;
    if (persisted.status != RunPersistenceResultStatus::Applied) {
        evaluator.discard(qualification);
        result.qualification = qualification;
        result.status =
            TargetQualificationOrchestrationStatus::PersistenceFailed;
        return result;
    }
    if (processDecision.reason == TransitionReason::CriticalFault) {
        // The application boundary has already fail-closed the evaluator at
        // the committed fault boundary. The candidate must not be applied
        // back into that freshly reset runtime.
        evaluator.discard(qualification);
        result.qualification = qualification;
        result.status =
            TargetQualificationOrchestrationStatus::AppliedPersisted;
        return result;
    }
    const auto committedContext = TargetQualificationCommitContext{
        context.qualification, current.runRevision,
        current.processState.transitionSequence};
    const bool applied = evaluator.applyAfterPersistedProcessApply(
        qualification, context, committedContext);
    const bool leavesQualificationDomain =
        processDecision.reason == TransitionReason::PreheatQualified ||
        processDecision.reason == TransitionReason::TargetQualified ||
        processDecision.reason == TransitionReason::CriticalFault;
    if (!applied) {
        if (leavesQualificationDomain) evaluator.reset();
        result.qualification = qualification;
        result.status = TargetQualificationOrchestrationStatus::StaleDecision;
        return result;
    }
    if (leavesQualificationDomain) {
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
