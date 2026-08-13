#include "process_state_machine.hpp"

#include <limits>

#include "program_limits.hpp"

namespace fermentation {
namespace {

constexpr std::uint64_t kMillisecondsPerMinute = 60000U;

bool validProcessState(ProcessState state) {
    switch (state) {
        case ProcessState::Boot:
        case ProcessState::SafeBoot:
        case ProcessState::Standby:
        case ProcessState::Preheating:
        case ProcessState::WaitingForProduct:
        case ProcessState::ReachingTarget:
        case ProcessState::QualifyingTarget:
        case ProcessState::Fermenting:
        case ProcessState::Cooling:
        case ProcessState::CoolHolding:
        case ProcessState::ManualHolding:
        case ProcessState::Completed:
        case ProcessState::RecoveryEvaluation:
        case ProcessState::Fault:
        case ProcessState::ServiceMode:
            return true;
    }
    return false;
}

bool validProcessKind(ProcessKind kind) {
    switch (kind) {
        case ProcessKind::Timed:
        case ProcessKind::ManualHolding:
            return true;
    }
    return false;
}

bool validCompletionModeForStateMachine(CompletionMode mode) {
    switch (mode) {
        case CompletionMode::FinishWithoutCooling:
        case CompletionMode::CoolThenFinish:
        case CompletionMode::CoolAndHoldForDuration:
        case CompletionMode::CoolAndHoldUntilManualStop:
            return true;
    }
    return false;
}

bool inRange(std::uint32_t value, std::uint32_t minimum,
             std::uint32_t maximum) {
    return value >= minimum && value <= maximum;
}

std::uint64_t minutesToMillis(std::uint32_t minutes) {
    return static_cast<std::uint64_t>(minutes) * kMillisecondsPerMinute;
}

bool elapsed(std::uint64_t now, std::uint64_t startedAt,
             std::uint32_t durationMinutes) {
    return now - startedAt >= minutesToMillis(durationMinutes);
}

bool stateHasTargetReachTimer(ProcessState state) {
    return state == ProcessState::ReachingTarget ||
           state == ProcessState::QualifyingTarget;
}

bool stateCanAbort(ProcessState state) {
    switch (state) {
        case ProcessState::Preheating:
        case ProcessState::WaitingForProduct:
        case ProcessState::ReachingTarget:
        case ProcessState::QualifyingTarget:
        case ProcessState::Fermenting:
        case ProcessState::Cooling:
        case ProcessState::CoolHolding:
        case ProcessState::ManualHolding:
            return true;
        case ProcessState::Boot:
        case ProcessState::SafeBoot:
        case ProcessState::Standby:
        case ProcessState::Completed:
        case ProcessState::RecoveryEvaluation:
        case ProcessState::Fault:
        case ProcessState::ServiceMode:
            return false;
    }
    return false;
}

bool stateCanEnterFault(ProcessState state) {
    return state != ProcessState::SafeBoot && state != ProcessState::Fault;
}

bool validRecoveryTarget(ProcessState state) {
    switch (state) {
        case ProcessState::Preheating:
        case ProcessState::WaitingForProduct:
        case ProcessState::ReachingTarget:
        case ProcessState::QualifyingTarget:
        case ProcessState::Fermenting:
        case ProcessState::Cooling:
        case ProcessState::CoolHolding:
        case ProcessState::ManualHolding:
            return true;
        case ProcessState::Boot:
        case ProcessState::SafeBoot:
        case ProcessState::Standby:
        case ProcessState::Completed:
        case ProcessState::RecoveryEvaluation:
        case ProcessState::Fault:
        case ProcessState::ServiceMode:
            return false;
    }
    return false;
}

bool runtimeTimeIsValid(const ProcessRuntimeState& state,
                        std::uint64_t monotonicMillis) {
    if (state.stateEnteredAtMillis > monotonicMillis) {
        return false;
    }
    if (stateHasTargetReachTimer(state.state) &&
        state.targetReachStartedAtMillis > monotonicMillis) {
        return false;
    }
    return !state.qualificationValidSinceMillis.has_value() ||
           *state.qualificationValidSinceMillis <= monotonicMillis;
}

bool runtimeShapeIsValid(const ProcessRuntimeState& state) {
    if (!validProcessState(state.state)) {
        return false;
    }
    const bool mayTrackQualification =
        state.state == ProcessState::Preheating ||
        state.state == ProcessState::QualifyingTarget;
    if (!mayTrackQualification &&
        state.qualificationValidSinceMillis.has_value()) {
        return false;
    }
    if (!stateHasTargetReachTimer(state.state)) {
        return state.targetReachStartedAtMillis == 0U &&
               !state.targetReachWarningIssued;
    }
    return true;
}

bool validBootTopology(const TransitionDecision& decision) {
    const auto from = decision.before.state;
    const auto to = decision.after.state;
    switch (decision.reason) {
        case TransitionReason::BootCompleted:
            return from == ProcessState::Boot && to == ProcessState::Standby;
        case TransitionReason::SafeBootRequired:
            return from == ProcessState::Boot && to == ProcessState::SafeBoot;
        case TransitionReason::CompletedRunRestored:
            return from == ProcessState::Boot && to == ProcessState::Completed;
        case TransitionReason::RecoveryRequired:
            return from == ProcessState::Boot &&
                   to == ProcessState::RecoveryEvaluation;
        default:
            return false;
    }
}

bool validTargetChangedTopology(ProcessState from, ProcessState to) {
    switch (from) {
        case ProcessState::Preheating:
            return to == ProcessState::Preheating;
        case ProcessState::ReachingTarget:
        case ProcessState::QualifyingTarget:
            return to == ProcessState::ReachingTarget;
        default:
            return false;
    }
}

bool validPhaseTopology(const TransitionDecision& decision) {
    const auto from = decision.before.state;
    const auto to = decision.after.state;
    switch (decision.reason) {
        case TransitionReason::RunStarted:
            return from == ProcessState::Standby &&
                   (to == ProcessState::Preheating ||
                    to == ProcessState::ReachingTarget);
        case TransitionReason::QualificationTrackingStarted:
            return (from == ProcessState::Preheating &&
                    to == ProcessState::Preheating) ||
                   (from == ProcessState::ReachingTarget &&
                    to == ProcessState::QualifyingTarget);
        case TransitionReason::QualificationReset:
            return (from == ProcessState::Preheating &&
                    to == ProcessState::Preheating) ||
                   (from == ProcessState::QualifyingTarget &&
                    to == ProcessState::ReachingTarget);
        case TransitionReason::PreheatQualified:
            return from == ProcessState::Preheating &&
                   to == ProcessState::WaitingForProduct;
        case TransitionReason::ProductInserted:
            return from == ProcessState::WaitingForProduct &&
                   to == ProcessState::ReachingTarget;
        case TransitionReason::TargetChangedReevaluation:
            return validTargetChangedTopology(from, to);
        case TransitionReason::ProductWaitExpired:
            return from == ProcessState::WaitingForProduct &&
                   to == ProcessState::Standby;
        case TransitionReason::TargetReachTimeExceeded:
            return (from == ProcessState::ReachingTarget &&
                    to == ProcessState::ReachingTarget) ||
                   (from == ProcessState::QualifyingTarget &&
                    to == ProcessState::QualifyingTarget);
        case TransitionReason::TargetQualified:
            return from == ProcessState::QualifyingTarget &&
                   (to == ProcessState::Fermenting ||
                    to == ProcessState::ManualHolding);
        case TransitionReason::FermentationCompleted:
            return from == ProcessState::Fermenting &&
                   (to == ProcessState::Cooling ||
                    to == ProcessState::Completed);
        case TransitionReason::CoolingTargetReached:
            return from == ProcessState::Cooling &&
                   (to == ProcessState::CoolHolding ||
                    to == ProcessState::Completed);
        case TransitionReason::HoldDurationCompleted:
            return from == ProcessState::CoolHolding &&
                   to == ProcessState::Completed;
        case TransitionReason::HoldFinishedByUser:
            return (from == ProcessState::CoolHolding ||
                    from == ProcessState::ManualHolding) &&
                   to == ProcessState::Completed;
        default:
            return false;
    }
}

bool validControlTopology(const TransitionDecision& decision) {
    const auto from = decision.before.state;
    const auto to = decision.after.state;
    switch (decision.reason) {
        case TransitionReason::RunAborted:
            return stateCanAbort(from) && to == ProcessState::Standby;
        case TransitionReason::CompletionAcknowledged:
            return from == ProcessState::Completed &&
                   to == ProcessState::Standby;
        case TransitionReason::CriticalFault:
            return stateCanEnterFault(from) && to == ProcessState::Fault;
        case TransitionReason::ServiceModeEntered:
            return from == ProcessState::Standby &&
                   to == ProcessState::ServiceMode;
        case TransitionReason::ServiceModeExited:
            return from == ProcessState::ServiceMode &&
                   to == ProcessState::Standby;
        case TransitionReason::RecoveryResumed:
            return from == ProcessState::RecoveryEvaluation &&
                   validRecoveryTarget(to);
        case TransitionReason::RecoveryRejected:
            return from == ProcessState::RecoveryEvaluation &&
                   to == ProcessState::Fault;
        case TransitionReason::RecoveryReentryRequired:
            return stateUsesRunSnapshot(from) &&
                   to == ProcessState::RecoveryEvaluation;
        case TransitionReason::RecoveryEndedByExpiredWait:
            return from == ProcessState::RecoveryEvaluation &&
                   to == ProcessState::Standby;
        default:
            return false;
    }
}

bool validProposedTopology(const TransitionDecision& decision) {
    return validBootTopology(decision) || validPhaseTopology(decision) ||
           validControlTopology(decision);
}

bool transitionMatchesRunSnapshot(const TransitionDecision& decision,
                                  const ProcessRunSnapshot* runSnapshot) {
    // Eine Sicherheitsabschaltung darf nie an fehlendem oder beschaedigtem
    // Laufkontext scheitern.
    if (decision.reason == TransitionReason::CriticalFault) {
        return true;
    }

    if (runSnapshot != nullptr && !validateProcessRunSnapshot(*runSnapshot)) {
        return false;
    }

    const bool needsRunSnapshot = stateUsesRunSnapshot(decision.before.state) ||
                                  stateUsesRunSnapshot(decision.after.state);
    if (needsRunSnapshot && runSnapshot == nullptr) {
        return false;
    }
    if (!needsRunSnapshot) {
        return true;
    }
    if ((stateUsesRunSnapshot(decision.before.state) &&
         !stateMatchesRunSnapshot(decision.before.state, *runSnapshot)) ||
        (stateUsesRunSnapshot(decision.after.state) &&
         !stateMatchesRunSnapshot(decision.after.state, *runSnapshot))) {
        return false;
    }

    switch (decision.reason) {
        case TransitionReason::RunStarted:
            return decision.after.state == (runSnapshot->preheatEnabled
                                                ? ProcessState::Preheating
                                                : ProcessState::ReachingTarget);
        case TransitionReason::TargetQualified:
            return decision.after.state ==
                   (runSnapshot->kind == ProcessKind::ManualHolding
                        ? ProcessState::ManualHolding
                        : ProcessState::Fermenting);
        case TransitionReason::FermentationCompleted:
            return decision.after.state ==
                   (runSnapshot->completionMode ==
                            CompletionMode::FinishWithoutCooling
                        ? ProcessState::Completed
                        : ProcessState::Cooling);
        case TransitionReason::CoolingTargetReached:
            return decision.after.state ==
                   (runSnapshot->completionMode ==
                            CompletionMode::CoolThenFinish
                        ? ProcessState::Completed
                        : ProcessState::CoolHolding);
        case TransitionReason::HoldDurationCompleted:
            return runSnapshot->completionMode ==
                   CompletionMode::CoolAndHoldForDuration;
        default:
            return true;
    }
}

TransitionDecision result(DecisionStatus status,
                          const ProcessRuntimeState& current,
                          std::uint64_t monotonicMillis) {
    TransitionDecision decision;
    decision.status = status;
    decision.before = current;
    decision.after = current;
    decision.monotonicMillis = monotonicMillis;
    return decision;
}

bool addMessage(TransitionDecision& decision, ProcessMessage message) {
    if (decision.messageCount >= decision.messages.size()) {
        return false;
    }
    decision.messages[decision.messageCount] = message;
    ++decision.messageCount;
    return true;
}

TransitionDecision proposePhaseDataUpdate(const ProcessRuntimeState& current,
                                          TransitionReason reason,
                                          std::uint64_t monotonicMillis) {
    auto decision = result(DecisionStatus::Proposed, current, monotonicMillis);
    decision.reason = reason;
    decision.after.transitionSequence = current.transitionSequence + 1U;
    return decision;
}

void initializeTargetReach(TransitionDecision& decision,
                           std::uint64_t monotonicMillis) {
    decision.after.targetReachStartedAtMillis = monotonicMillis;
    decision.after.targetReachWarningIssued = false;
}

bool targetReachWarningDue(const ProcessRuntimeState& current,
                           const ProcessRunSnapshot& snapshot,
                           std::uint64_t monotonicMillis) {
    return stateHasTargetReachTimer(current.state) &&
           !current.targetReachWarningIssued &&
           elapsed(monotonicMillis, current.targetReachStartedAtMillis,
                   snapshot.maximumTargetReachMinutes);
}

void includeTargetReachWarning(TransitionDecision& decision) {
    if (stateHasTargetReachTimer(decision.after.state)) {
        decision.after.targetReachWarningIssued = true;
    }
    static_cast<void>(
        addMessage(decision, ProcessMessage::TargetReachTimeExceeded));
}

TransitionDecision decideQualification(const ProcessRuntimeState& current,
                                       const ProcessRunSnapshot& snapshot,
                                       const ProcessSignals& signals,
                                       std::uint64_t monotonicMillis,
                                       bool preheating) {
    if (qualificationIsInterrupted(signals)) {
        if (!current.qualificationValidSinceMillis.has_value()) {
            return result(DecisionStatus::NoTransition, current,
                          monotonicMillis);
        }
        if (preheating) {
            auto decision = proposePhaseDataUpdate(
                current, TransitionReason::QualificationReset, monotonicMillis);
            decision.after.qualificationValidSinceMillis = std::nullopt;
            return decision;
        }
        auto decision =
            propose(current, ProcessState::ReachingTarget,
                    TransitionReason::QualificationReset, monotonicMillis);
        decision.after.targetReachStartedAtMillis =
            current.targetReachStartedAtMillis;
        decision.after.targetReachWarningIssued =
            current.targetReachWarningIssued;
        return decision;
    }

    if (signals.qualificationProgress == QualificationProgress::Complete &&
        preheating) {
        auto decision =
            propose(current, ProcessState::WaitingForProduct,
                    TransitionReason::PreheatQualified, monotonicMillis);
        static_cast<void>(
            addMessage(decision, ProcessMessage::ProductInsertionRequested));
        return decision;
    }

    if (signals.qualificationProgress == QualificationProgress::Complete) {
        const auto nextState = snapshot.kind == ProcessKind::ManualHolding
                                   ? ProcessState::ManualHolding
                                   : ProcessState::Fermenting;
        return propose(current, nextState, TransitionReason::TargetQualified,
                       monotonicMillis);
    }

    if (!current.qualificationValidSinceMillis.has_value()) {
        auto decision = proposePhaseDataUpdate(
            current, TransitionReason::QualificationTrackingStarted,
            monotonicMillis);
        decision.after.qualificationValidSinceMillis = monotonicMillis;
        return decision;
    }
    if (!qualificationIsComplete(signals)) {
        return result(DecisionStatus::NoTransition, current, monotonicMillis);
    }

    return result(DecisionStatus::NoTransition, current, monotonicMillis);
}

TransitionDecision decideWaitingForProduct(
    const ProcessRuntimeState& current, const ProcessRunSnapshot& snapshot,
    std::uint64_t monotonicMillis, PriorBootPhaseElapsed priorElapsed = {}) {
    if (!snapshot.maximumProductWaitMinutes.has_value() ||
        !elapsedWithPrior(monotonicMillis, current.stateEnteredAtMillis,
                          *snapshot.maximumProductWaitMinutes,
                          priorElapsed.lowerBoundSeconds)) {
        return result(DecisionStatus::NoTransition, current, monotonicMillis);
    }
    auto decision =
        propose(current, ProcessState::Standby,
                TransitionReason::ProductWaitExpired, monotonicMillis);
    static_cast<void>(addMessage(decision, ProcessMessage::RunAborted));
    return decision;
}

TransitionDecision decideReachingTarget(const ProcessRuntimeState& current,
                                        const ProcessRunSnapshot& snapshot,
                                        const ProcessSignals& signals,
                                        std::uint64_t monotonicMillis) {
    if (qualificationHasPositiveEvidence(signals)) {
        auto decision = propose(current, ProcessState::QualifyingTarget,
                                TransitionReason::QualificationTrackingStarted,
                                monotonicMillis);
        decision.after.targetReachStartedAtMillis =
            current.targetReachStartedAtMillis;
        decision.after.targetReachWarningIssued =
            current.targetReachWarningIssued;
        decision.after.qualificationValidSinceMillis = monotonicMillis;
        if (targetReachWarningDue(current, snapshot, monotonicMillis)) {
            includeTargetReachWarning(decision);
        }
        return decision;
    }
    if (targetReachWarningDue(current, snapshot, monotonicMillis)) {
        auto decision = proposePhaseDataUpdate(
            current, TransitionReason::TargetReachTimeExceeded,
            monotonicMillis);
        includeTargetReachWarning(decision);
        return decision;
    }
    return result(DecisionStatus::NoTransition, current, monotonicMillis);
}

TransitionDecision decideQualifyingTarget(const ProcessRuntimeState& current,
                                          const ProcessRunSnapshot& snapshot,
                                          const ProcessSignals& signals,
                                          std::uint64_t monotonicMillis) {
    auto decision =
        decideQualification(current, snapshot, signals, monotonicMillis, false);
    if (targetReachWarningDue(current, snapshot, monotonicMillis)) {
        if (!decision.proposed()) {
            decision = proposePhaseDataUpdate(
                current, TransitionReason::TargetReachTimeExceeded,
                monotonicMillis);
        }
        includeTargetReachWarning(decision);
    }
    return decision;
}

TransitionDecision decideFermenting(const ProcessRuntimeState& current,
                                    const ProcessRunSnapshot& snapshot,
                                    std::uint64_t monotonicMillis,
                                    PriorBootPhaseElapsed priorElapsed = {}) {
    if (!snapshot.fermentationDurationMinutes.has_value() ||
        !elapsedWithPrior(monotonicMillis, current.stateEnteredAtMillis,
                          *snapshot.fermentationDurationMinutes,
                          priorElapsed.lowerBoundSeconds)) {
        return result(DecisionStatus::NoTransition, current, monotonicMillis);
    }
    return completeTimedRun(current, snapshot, monotonicMillis);
}

TransitionDecision decideCooling(const ProcessRuntimeState& current,
                                 const ProcessRunSnapshot& snapshot,
                                 const ProcessSignals& signals,
                                 std::uint64_t monotonicMillis) {
    if (!signals.coolingTargetConditionValid) {
        return result(DecisionStatus::NoTransition, current, monotonicMillis);
    }
    if (snapshot.completionMode == CompletionMode::CoolThenFinish) {
        auto decision =
            propose(current, ProcessState::Completed,
                    TransitionReason::CoolingTargetReached, monotonicMillis);
        static_cast<void>(addMessage(decision, ProcessMessage::RunCompleted));
        return decision;
    }
    return propose(current, ProcessState::CoolHolding,
                   TransitionReason::CoolingTargetReached, monotonicMillis);
}

TransitionDecision decideCoolHolding(const ProcessRuntimeState& current,
                                     const ProcessRunSnapshot& snapshot,
                                     std::uint64_t monotonicMillis,
                                     PriorBootPhaseElapsed priorElapsed = {}) {
    if (snapshot.completionMode != CompletionMode::CoolAndHoldForDuration ||
        !snapshot.holdDurationMinutes.has_value() ||
        !elapsedWithPrior(monotonicMillis, current.stateEnteredAtMillis,
                          *snapshot.holdDurationMinutes,
                          priorElapsed.lowerBoundSeconds)) {
        return result(DecisionStatus::NoTransition, current, monotonicMillis);
    }
    return completeHoldDuration(current, monotonicMillis);
}

TransitionDecision decideAutomatic(
    const ProcessRuntimeState& current, const ProcessRunSnapshot* snapshot,
    const ProcessSignals& signals, std::uint64_t monotonicMillis,
    const PriorBootPhaseElapsed& priorElapsed = {}) {
    if (snapshot == nullptr && stateUsesRunSnapshot(current.state)) {
        return result(DecisionStatus::InvalidInput, current, monotonicMillis);
    }

    switch (current.state) {
        case ProcessState::Preheating:
            return decideQualification(current, *snapshot, signals,
                                       monotonicMillis, true);
        case ProcessState::WaitingForProduct:
            return decideWaitingForProduct(current, *snapshot, monotonicMillis,
                                           priorElapsed);
        case ProcessState::ReachingTarget:
            return decideReachingTarget(current, *snapshot, signals,
                                        monotonicMillis);
        case ProcessState::QualifyingTarget:
            return decideQualifyingTarget(current, *snapshot, signals,
                                          monotonicMillis);
        case ProcessState::Fermenting:
            return decideFermenting(current, *snapshot, monotonicMillis,
                                    priorElapsed);
        case ProcessState::Cooling:
            return decideCooling(current, *snapshot, signals, monotonicMillis);
        case ProcessState::CoolHolding:
            return decideCoolHolding(current, *snapshot, monotonicMillis,
                                     priorElapsed);
        case ProcessState::Boot:
        case ProcessState::SafeBoot:
        case ProcessState::Standby:
        case ProcessState::ManualHolding:
        case ProcessState::Completed:
        case ProcessState::RecoveryEvaluation:
        case ProcessState::Fault:
        case ProcessState::ServiceMode:
            return result(DecisionStatus::NoTransition, current,
                          monotonicMillis);
    }
    return result(DecisionStatus::InvalidInput, current, monotonicMillis);
}

TransitionDecision rejected(const ProcessRuntimeState& current,
                            std::uint64_t monotonicMillis) {
    return result(DecisionStatus::Rejected, current, monotonicMillis);
}

TransitionDecision decideBootEvent(const ProcessRuntimeState& current,
                                   ProcessEvent event,
                                   std::uint64_t monotonicMillis) {
    switch (event) {
        case ProcessEvent::BootReady:
            return propose(current, ProcessState::Standby,
                           TransitionReason::BootCompleted, monotonicMillis);
        case ProcessEvent::BootSafe:
            return propose(current, ProcessState::SafeBoot,
                           TransitionReason::SafeBootRequired, monotonicMillis);
        case ProcessEvent::BootRestoreCompleted:
            return propose(current, ProcessState::Completed,
                           TransitionReason::CompletedRunRestored,
                           monotonicMillis);
        case ProcessEvent::BootRecoverRun:
            return propose(current, ProcessState::RecoveryEvaluation,
                           TransitionReason::RecoveryRequired, monotonicMillis);
        default:
            return rejected(current, monotonicMillis);
    }
}

TransitionDecision decideStandbyEvent(const ProcessRuntimeState& current,
                                      const ProcessRunSnapshot* snapshot,
                                      ProcessEvent event,
                                      std::uint64_t monotonicMillis) {
    if (event == ProcessEvent::EnterServiceMode) {
        return propose(current, ProcessState::ServiceMode,
                       TransitionReason::ServiceModeEntered, monotonicMillis);
    }
    if (event != ProcessEvent::StartRun || snapshot == nullptr) {
        return rejected(current, monotonicMillis);
    }
    const auto nextState = snapshot->preheatEnabled
                               ? ProcessState::Preheating
                               : ProcessState::ReachingTarget;
    auto decision = propose(current, nextState, TransitionReason::RunStarted,
                            monotonicMillis);
    if (nextState == ProcessState::ReachingTarget) {
        initializeTargetReach(decision, monotonicMillis);
    }
    return decision;
}

TransitionDecision decideHoldEvent(const ProcessRuntimeState& current,
                                   ProcessEvent event,
                                   std::uint64_t monotonicMillis) {
    if (event != ProcessEvent::FinishHoldConfirmed) {
        return rejected(current, monotonicMillis);
    }
    auto decision =
        propose(current, ProcessState::Completed,
                TransitionReason::HoldFinishedByUser, monotonicMillis);
    static_cast<void>(addMessage(decision, ProcessMessage::RunCompleted));
    return decision;
}

TransitionDecision decideRecoveryEvent(
    const ProcessRuntimeState& current, const ProcessRunSnapshot* snapshot,
    const TransitionRequest& request, std::uint64_t monotonicMillis,
    const PriorBootPhaseElapsed& priorElapsed = {}) {
    if (request.event == ProcessEvent::RecoveryReject) {
        auto decision =
            propose(current, ProcessState::Fault,
                    TransitionReason::RecoveryRejected, monotonicMillis);
        static_cast<void>(addMessage(decision, ProcessMessage::FaultEntered));
        return decision;
    }
    if (request.event != ProcessEvent::RecoveryResume ||
        !request.recoveredState.has_value() || snapshot == nullptr) {
        return rejected(current, monotonicMillis);
    }

    const auto& recovered = request.recoveredState.value();
    if (!validRecoveryTarget(recovered.state) ||
        !runtimeShapeIsValid(recovered) ||
        !runtimeTimeIsValid(recovered, monotonicMillis) ||
        !stateMatchesRunSnapshot(recovered.state, *snapshot) ||
        (recovered.state == ProcessState::WaitingForProduct &&
         snapshot->maximumProductWaitMinutes.has_value() &&
         elapsedWithPrior(monotonicMillis, recovered.stateEnteredAtMillis,
                          *snapshot->maximumProductWaitMinutes,
                          priorElapsed.lowerBoundSeconds))) {
        return rejected(current, monotonicMillis);
    }

    auto decision = result(DecisionStatus::Proposed, current, monotonicMillis);
    decision.reason = TransitionReason::RecoveryResumed;
    decision.after = recovered;
    if (decision.after.state == ProcessState::Preheating) {
        decision.after.qualificationValidSinceMillis.reset();
    } else if (decision.after.state == ProcessState::QualifyingTarget) {
        decision.after.state = ProcessState::ReachingTarget;
        decision.after.stateEnteredAtMillis = monotonicMillis;
        decision.after.qualificationValidSinceMillis.reset();
    }
    decision.after.transitionSequence = current.transitionSequence + 1U;
    return decision;
}

TransitionDecision decideExplicitEvent(
    const ProcessRuntimeState& current, const ProcessRunSnapshot* snapshot,
    const TransitionRequest& request, std::uint64_t monotonicMillis,
    const PriorBootPhaseElapsed& priorElapsed = {}) {
    if (request.event == ProcessEvent::Abort) {
        if (!stateCanAbort(current.state)) {
            return rejected(current, monotonicMillis);
        }
        auto decision = propose(current, ProcessState::Standby,
                                TransitionReason::RunAborted, monotonicMillis);
        static_cast<void>(addMessage(decision, ProcessMessage::RunAborted));
        return decision;
    }

    if (request.event == ProcessEvent::TargetChanged) {
        if (current.state == ProcessState::Preheating) {
            auto decision = propose(current, ProcessState::Preheating,
                                    TransitionReason::TargetChangedReevaluation,
                                    monotonicMillis);
            decision.after.qualificationValidSinceMillis.reset();
            decision.committedControlContextTransition =
                CommittedControlContextTransition::TargetContextChange;
            return decision;
        }
        if (current.state == ProcessState::ReachingTarget ||
            current.state == ProcessState::QualifyingTarget) {
            auto decision = propose(current, ProcessState::ReachingTarget,
                                    TransitionReason::TargetChangedReevaluation,
                                    monotonicMillis);
            initializeTargetReach(decision, monotonicMillis);
            decision.committedControlContextTransition =
                CommittedControlContextTransition::TargetContextChange;
            return decision;
        }
        return rejected(current, monotonicMillis);
    }

    switch (current.state) {
        case ProcessState::Boot:
            return decideBootEvent(current, request.event, monotonicMillis);
        case ProcessState::Standby:
            return decideStandbyEvent(current, snapshot, request.event,
                                      monotonicMillis);
        case ProcessState::WaitingForProduct:
            if (request.event == ProcessEvent::ProductInsertedConfirmed) {
                auto decision =
                    propose(current, ProcessState::ReachingTarget,
                            TransitionReason::ProductInserted, monotonicMillis);
                initializeTargetReach(decision, monotonicMillis);
                return decision;
            }
            return rejected(current, monotonicMillis);
        case ProcessState::CoolHolding:
        case ProcessState::ManualHolding:
            return decideHoldEvent(current, request.event, monotonicMillis);
        case ProcessState::Completed:
            if (request.event == ProcessEvent::AcknowledgeCompletion) {
                return propose(current, ProcessState::Standby,
                               TransitionReason::CompletionAcknowledged,
                               monotonicMillis);
            }
            return rejected(current, monotonicMillis);
        case ProcessState::RecoveryEvaluation:
            return decideRecoveryEvent(current, snapshot, request,
                                       monotonicMillis, priorElapsed);
        case ProcessState::ServiceMode:
            if (request.event == ProcessEvent::ExitServiceMode) {
                return propose(current, ProcessState::Standby,
                               TransitionReason::ServiceModeExited,
                               monotonicMillis);
            }
            return rejected(current, monotonicMillis);
        case ProcessState::SafeBoot:
        case ProcessState::Preheating:
        case ProcessState::ReachingTarget:
        case ProcessState::QualifyingTarget:
        case ProcessState::Fermenting:
        case ProcessState::Cooling:
        case ProcessState::Fault:
            return rejected(current, monotonicMillis);
    }
    return rejected(current, monotonicMillis);
}

}  // namespace

bool validateProcessRunSnapshot(const ProcessRunSnapshot& snapshot) {
    if (!validProcessKind(snapshot.kind) ||
        !validCompletionModeForStateMachine(snapshot.completionMode) ||
        !inRange(snapshot.qualificationDurationMinutes,
                 program_limits::kMinimumQualificationDurationMinutes,
                 program_limits::kMaximumQualificationDurationMinutes) ||
        !inRange(snapshot.maximumTargetReachMinutes,
                 program_limits::kMinimumTargetReachMinutes,
                 program_limits::kMaximumTargetReachMinutes)) {
        return false;
    }
    if (snapshot.preheatEnabled !=
        snapshot.maximumProductWaitMinutes.has_value()) {
        return false;
    }
    if (snapshot.maximumProductWaitMinutes.has_value() &&
        !inRange(*snapshot.maximumProductWaitMinutes,
                 program_limits::kMinimumProductWaitMinutes,
                 program_limits::kMaximumProductWaitMinutes)) {
        return false;
    }
    if (snapshot.kind == ProcessKind::Timed) {
        if (!snapshot.fermentationDurationMinutes.has_value() ||
            *snapshot.fermentationDurationMinutes >
                program_limits::kMaximumFermentationDurationMinutes) {
            return false;
        }
    } else if (snapshot.fermentationDurationMinutes.has_value() ||
               snapshot.completionMode !=
                   CompletionMode::FinishWithoutCooling) {
        return false;
    }

    const bool needsTimedHold =
        snapshot.completionMode == CompletionMode::CoolAndHoldForDuration;
    if (needsTimedHold != snapshot.holdDurationMinutes.has_value()) {
        return false;
    }
    return !snapshot.holdDurationMinutes.has_value() ||
           inRange(*snapshot.holdDurationMinutes,
                   program_limits::kMinimumHoldDurationMinutes,
                   program_limits::kMaximumHoldDurationMinutes);
}

std::optional<ProcessRunSnapshot> makeProcessRunSnapshot(const ActiveRun& run) {
    const auto& program = run.snapshot().sourceProgram.program;
    if (!program.targetQualification.durationMinutes.has_value() ||
        !program.maximumTargetReachMinutes.has_value()) {
        return std::nullopt;
    }

    ProcessRunSnapshot snapshot;
    snapshot.kind = ProcessKind::Timed;
    snapshot.preheatEnabled = program.preheat;
    snapshot.completionMode = program.completion.mode;
    snapshot.qualificationDurationMinutes =
        *program.targetQualification.durationMinutes;
    snapshot.maximumTargetReachMinutes = *program.maximumTargetReachMinutes;
    snapshot.maximumProductWaitMinutes = program.maximumProductWaitMinutes;
    snapshot.fermentationDurationMinutes =
        run.effectiveValues().remainingDurationMinutes;
    snapshot.holdDurationMinutes = program.completion.holdDurationMinutes;
    return validateProcessRunSnapshot(snapshot)
               ? std::optional<ProcessRunSnapshot>{snapshot}
               : std::nullopt;
}

bool equalProcessRunSnapshot(const ProcessRunSnapshot& left,
                             const ProcessRunSnapshot& right) {
    return left.kind == right.kind &&
           left.preheatEnabled == right.preheatEnabled &&
           left.completionMode == right.completionMode &&
           left.qualificationDurationMinutes ==
               right.qualificationDurationMinutes &&
           left.maximumTargetReachMinutes == right.maximumTargetReachMinutes &&
           left.maximumProductWaitMinutes == right.maximumProductWaitMinutes &&
           left.fermentationDurationMinutes ==
               right.fermentationDurationMinutes &&
           left.holdDurationMinutes == right.holdDurationMinutes;
}

bool equalProcessRuntimeState(const ProcessRuntimeState& left,
                              const ProcessRuntimeState& right) {
    return left.state == right.state &&
           left.stateEnteredAtMillis == right.stateEnteredAtMillis &&
           left.targetReachStartedAtMillis ==
               right.targetReachStartedAtMillis &&
           left.qualificationValidSinceMillis ==
               right.qualificationValidSinceMillis &&
           left.targetReachWarningIssued == right.targetReachWarningIssued &&
           left.transitionSequence == right.transitionSequence;
}

bool validateProcessRuntimeForCheckpoint(
    const ProcessRuntimeState& state, const ProcessRunSnapshot* runSnapshot,
    std::uint64_t checkpointMonotonicMillis) {
    if (!runtimeShapeIsValid(state) ||
        !runtimeTimeIsValid(state, checkpointMonotonicMillis)) {
        return false;
    }
    if (stateUsesRunSnapshot(state.state)) {
        return runSnapshot != nullptr &&
               validateProcessRunSnapshot(*runSnapshot) &&
               stateMatchesRunSnapshot(state.state, *runSnapshot);
    }
    return runSnapshot == nullptr || validateProcessRunSnapshot(*runSnapshot);
}

bool stateUsesRunSnapshot(ProcessState state) {
    switch (state) {
        case ProcessState::Preheating:
        case ProcessState::WaitingForProduct:
        case ProcessState::ReachingTarget:
        case ProcessState::QualifyingTarget:
        case ProcessState::Fermenting:
        case ProcessState::Cooling:
        case ProcessState::CoolHolding:
        case ProcessState::ManualHolding:
            return true;
        case ProcessState::Boot:
        case ProcessState::SafeBoot:
        case ProcessState::Standby:
        case ProcessState::Completed:
        case ProcessState::RecoveryEvaluation:
        case ProcessState::Fault:
        case ProcessState::ServiceMode:
            return false;
    }
    return false;
}

bool stateMatchesRunSnapshot(ProcessState state,
                             const ProcessRunSnapshot& snapshot) {
    switch (state) {
        case ProcessState::Preheating:
            return snapshot.preheatEnabled;
        case ProcessState::WaitingForProduct:
            return snapshot.preheatEnabled &&
                   snapshot.maximumProductWaitMinutes.has_value();
        case ProcessState::ReachingTarget:
        case ProcessState::QualifyingTarget:
            return true;
        case ProcessState::Fermenting:
            return snapshot.kind == ProcessKind::Timed &&
                   snapshot.fermentationDurationMinutes.has_value();
        case ProcessState::Cooling:
            return snapshot.kind == ProcessKind::Timed &&
                   snapshot.completionMode !=
                       CompletionMode::FinishWithoutCooling;
        case ProcessState::CoolHolding:
            return snapshot.kind == ProcessKind::Timed &&
                   (snapshot.completionMode ==
                        CompletionMode::CoolAndHoldForDuration ||
                    snapshot.completionMode ==
                        CompletionMode::CoolAndHoldUntilManualStop);
        case ProcessState::ManualHolding:
            return snapshot.kind == ProcessKind::ManualHolding;
        case ProcessState::Boot:
        case ProcessState::SafeBoot:
        case ProcessState::Standby:
        case ProcessState::Completed:
        case ProcessState::RecoveryEvaluation:
        case ProcessState::Fault:
        case ProcessState::ServiceMode:
            return true;
    }
    return false;
}

TransitionDecision propose(const ProcessRuntimeState& current,
                           ProcessState nextState, TransitionReason reason,
                           std::uint64_t monotonicMillis) {
    // Kein gueltiger Transitionspfad darf UINT32_MAX -> 0 ueberlaufen; dieser
    // Schutz lag bisher nur in decideProcessTransition() und griff damit
    // nicht fuer den seit Commit 1 direkten propose()-Aufrufpfad (Recovery-
    // Orchestrierung ausserhalb dieser Uebersetzungseinheit, 5.9/5.11/5.17).
    if (current.transitionSequence ==
        std::numeric_limits<std::uint32_t>::max()) {
        return result(DecisionStatus::InvalidInput, current, monotonicMillis);
    }
    auto decision = result(DecisionStatus::Proposed, current, monotonicMillis);
    decision.reason = reason;
    decision.after.state = nextState;
    decision.after.stateEnteredAtMillis = monotonicMillis;
    decision.after.qualificationValidSinceMillis = std::nullopt;
    if (!stateHasTargetReachTimer(nextState)) {
        decision.after.targetReachStartedAtMillis = 0U;
        decision.after.targetReachWarningIssued = false;
    }
    decision.after.transitionSequence = current.transitionSequence + 1U;
    return decision;
}

TransitionDecision completeTimedRun(const ProcessRuntimeState& current,
                                    const ProcessRunSnapshot& snapshot,
                                    std::uint64_t monotonicMillis) {
    if (snapshot.completionMode == CompletionMode::FinishWithoutCooling) {
        auto decision =
            propose(current, ProcessState::Completed,
                    TransitionReason::FermentationCompleted, monotonicMillis);
        static_cast<void>(addMessage(decision, ProcessMessage::RunCompleted));
        return decision;
    }
    auto decision =
        propose(current, ProcessState::Cooling,
                TransitionReason::FermentationCompleted, monotonicMillis);
    decision.committedControlContextTransition =
        CommittedControlContextTransition::CoolingTargetContextChange;
    return decision;
}

TransitionDecision completeHoldDuration(const ProcessRuntimeState& current,
                                        std::uint64_t monotonicMillis) {
    auto decision =
        propose(current, ProcessState::Completed,
                TransitionReason::HoldDurationCompleted, monotonicMillis);
    static_cast<void>(addMessage(decision, ProcessMessage::RunCompleted));
    return decision;
}

TransitionDecision decideProcessTransition(
    const ProcessRuntimeState& current, const ProcessRunSnapshot* runSnapshot,
    const ProcessSignals& signals, const TransitionRequest& request,
    std::uint64_t monotonicMillis, const PriorBootPhaseElapsed& priorElapsed) {
    if (!runtimeShapeIsValid(current)) {
        return result(DecisionStatus::InvalidInput, current, monotonicMillis);
    }
    if (!runtimeTimeIsValid(current, monotonicMillis)) {
        return result(DecisionStatus::TimeWentBackwards, current,
                      monotonicMillis);
    }
    if (current.transitionSequence ==
        std::numeric_limits<std::uint32_t>::max()) {
        return result(DecisionStatus::InvalidInput, current, monotonicMillis);
    }

    if (signals.criticalFault && stateCanEnterFault(current.state)) {
        auto decision =
            propose(current, ProcessState::Fault,
                    TransitionReason::CriticalFault, monotonicMillis);
        static_cast<void>(addMessage(decision, ProcessMessage::FaultEntered));
        return decision;
    }

    if (runSnapshot != nullptr && !validateProcessRunSnapshot(*runSnapshot)) {
        return result(DecisionStatus::InvalidInput, current, monotonicMillis);
    }
    if (stateUsesRunSnapshot(current.state) && runSnapshot == nullptr) {
        return result(DecisionStatus::InvalidInput, current, monotonicMillis);
    }
    if (stateUsesRunSnapshot(current.state) &&
        !stateMatchesRunSnapshot(current.state, *runSnapshot)) {
        return result(DecisionStatus::InvalidInput, current, monotonicMillis);
    }

    if (current.state == ProcessState::WaitingForProduct &&
        runSnapshot->maximumProductWaitMinutes.has_value() &&
        elapsedWithPrior(monotonicMillis, current.stateEnteredAtMillis,
                         *runSnapshot->maximumProductWaitMinutes,
                         priorElapsed.lowerBoundSeconds)) {
        return decideAutomatic(current, runSnapshot, signals, monotonicMillis,
                               priorElapsed);
    }

    if (request.event != ProcessEvent::None) {
        return decideExplicitEvent(current, runSnapshot, request,
                                   monotonicMillis, priorElapsed);
    }
    return decideAutomatic(current, runSnapshot, signals, monotonicMillis,
                           priorElapsed);
}

bool applyProcessTransition(ProcessRuntimeState& current,
                            const TransitionDecision& decision,
                            const ProcessRunSnapshot* runSnapshot) {
    if (!decision.proposed() ||
        !equalProcessRuntimeState(current, decision.before) ||
        // Letzte Schutzschicht: bei before.transitionSequence == UINT32_MAX
        // wraeppt sowohl ein manuell konstruiertes after.transitionSequence
        // als auch der Vergleichsausdruck before+1U identisch auf 0, sodass
        // der folgende Gleichheitscheck einen Wrap-Around faelschlich
        // akzeptieren wuerde. Explizit vorab ausschliessen statt sich auf den
        // (dadurch selbst ueberlaufenden) Vergleich zu verlassen.
        decision.before.transitionSequence ==
            std::numeric_limits<std::uint32_t>::max() ||
        decision.after.transitionSequence !=
            decision.before.transitionSequence + 1U ||
        !validProposedTopology(decision) ||
        !transitionMatchesRunSnapshot(decision, runSnapshot) ||
        !runtimeShapeIsValid(decision.after) ||
        !runtimeTimeIsValid(decision.after, decision.monotonicMillis)) {
        return false;
    }
    current = decision.after;
    return true;
}

}  // namespace fermentation
