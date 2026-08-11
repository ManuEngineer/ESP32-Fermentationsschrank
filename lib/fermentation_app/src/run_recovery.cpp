#include "run_recovery.hpp"

#include <limits>

#include "sensor_selection.hpp"

namespace fermentation {
namespace {

RunPersistenceResult coordinatorResult(
    RunPersistenceResultStatus status,
    RunPersistenceCoordinatorState coordinatorState) {
    RunPersistenceResult result;
    result.status = status;
    result.coordinatorState = coordinatorState;
    return result;
}

std::optional<std::uint32_t> checkedToUint32(std::uint64_t value) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(value);
}

std::optional<PriorBootPhaseElapsed> reevaluatedPriorElapsed(
    const PendingRecoveryAnchor& anchor, const RecoveryTimeContext& context) {
    const auto lower = context.elapsed.totalSecondsLowerBound;
    const auto lower32 = checkedToUint32(lower);
    if (!lower32.has_value()) return std::nullopt;
    std::optional<std::uint32_t> upper32;
    if (context.elapsed.totalSecondsUpperBound.has_value()) {
        upper32 = checkedToUint32(*context.elapsed.totalSecondsUpperBound);
        if (!upper32.has_value()) return std::nullopt;
    }
    static_cast<void>(anchor);
    return PriorBootPhaseElapsed{*lower32, upper32};
}

RunPersistenceResult activateWith(
    RunPersistenceCoordinator& persistence, RunCommandState& current,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    switch (persistence.state()) {
        case RunPersistenceCoordinatorState::LoadedActiveRun: {
            const auto outcome = persistence.activateLoadedRun(
                current, time, liveSensorEvidence);
            current = outcome.resultingState;
            return outcome.persistenceResult;
        }
        case RunPersistenceCoordinatorState::FallbackRecoveryPending: {
            const auto outcome = persistence.activateFallbackRecoveredRun(
                current, time, liveSensorEvidence);
            current = outcome.resultingState;
            return outcome.persistenceResult;
        }
        case RunPersistenceCoordinatorState::ReadyEmpty:
            return coordinatorResult(RunPersistenceResultStatus::NoActiveRun,
                                     persistence.state());
        case RunPersistenceCoordinatorState::Uninitialized:
            return coordinatorResult(RunPersistenceResultStatus::NotInitialized,
                                     persistence.state());
        case RunPersistenceCoordinatorState::Ready:
            return coordinatorResult(
                RunPersistenceResultStatus::NotAllowedInState,
                persistence.state());
        case RunPersistenceCoordinatorState::Busy:
            return coordinatorResult(RunPersistenceResultStatus::Busy,
                                     persistence.state());
        case RunPersistenceCoordinatorState::BlockedIndeterminate:
            return coordinatorResult(RunPersistenceResultStatus::Blocked,
                                     persistence.state());
        case RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed:
            return coordinatorResult(
                RunPersistenceResultStatus::PersistenceCommittedApplyFailed,
                persistence.state());
    }
    return coordinatorResult(RunPersistenceResultStatus::Blocked,
                             persistence.state());
}

RunPersistenceResult reevaluateWith(
    RunPersistenceCoordinator& persistence, RunCommandState& current,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    if (!current.pendingRecoveryAnchor.has_value() ||
        !current.recoveryBootAnchorMonotonicMillis.has_value()) {
        return coordinatorResult(RunPersistenceResultStatus::NotAllowedInState,
                                 persistence.state());
    }

    // The Hop-1-only WaitingForProduct path needs a fresh Gate-A sensor
    // context. Keep the no-context API fail-closed; the existing persistence
    // coordinator remains the sole owner of the Gate-A/Resume rules.
    if (current.processState.state == ProcessState::RecoveryEvaluation) {
        if (liveSensorEvidence == nullptr) {
            return coordinatorResult(
                RunPersistenceResultStatus::NotAllowedInState,
                persistence.state());
        }
        return coordinatorResult(RunPersistenceResultStatus::NotAllowedInState,
                                 persistence.state());
    }

    const auto phase = current.processState.state;
    if (phase != ProcessState::WaitingForProduct &&
        phase != ProcessState::Fermenting &&
        phase != ProcessState::CoolHolding) {
        return coordinatorResult(RunPersistenceResultStatus::NotAllowedInState,
                                 persistence.state());
    }
    if (!current.priorBootPhaseElapsed.has_value() ||
        current.priorBootPhaseElapsed->taggedState != phase) {
        return coordinatorResult(RunPersistenceResultStatus::NotAllowedInState,
                                 persistence.state());
    }

    const auto context = deriveRecoveryTimeContext(
        *current.pendingRecoveryAnchor, time.utcUnixSeconds,
        time.monotonicMillis, *current.recoveryBootAnchorMonotonicMillis);
    if (!context.has_value()) {
        return coordinatorResult(RunPersistenceResultStatus::NotAllowedInState,
                                 persistence.state());
    }
    const auto reevaluated =
        reevaluatedPriorElapsed(*current.pendingRecoveryAnchor, *context);
    if (!reevaluated.has_value()) {
        return coordinatorResult(RunPersistenceResultStatus::InvalidDecision,
                                 persistence.state());
    }

    const auto& previous = current.priorBootPhaseElapsed->elapsed;
    const bool lowerImproved =
        reevaluated->lowerBoundSeconds > previous.lowerBoundSeconds;
    const bool upperResolved = !previous.upperBoundSeconds.has_value() &&
                               reevaluated->upperBoundSeconds.has_value();
    if (!lowerImproved && !upperResolved) {
        return coordinatorResult(RunPersistenceResultStatus::NotDue,
                                 persistence.state());
    }

    if (current.runRevision == std::numeric_limits<std::uint32_t>::max()) {
        return coordinatorResult(RunPersistenceResultStatus::CounterOverflow,
                                 persistence.state());
    }
    auto candidate = current;
    candidate.priorBootPhaseElapsed =
        TaggedPriorBootPhaseElapsed{phase, *reevaluated};
    if (reevaluated->upperBoundSeconds.has_value()) {
        candidate.pendingRecoveryAnchor.reset();
        candidate.recoveryBootAnchorMonotonicMillis.reset();
    }
    ++candidate.runRevision;
    return persistence.persistRecoveryCandidate(current, candidate, time);
}

RunPersistenceResult applyWeightingWith(
    RunPersistenceCoordinator& persistence, RunCommandState& current,
    std::uint32_t expectedRunRevision,
    std::uint32_t expectedRecoveryEpisodeRevision,
    std::uint32_t weightedProgressSegmentId, const RunCheckpointTime& time,
    const RecoveryProgressWeightingInput& suppliedInput,
    const RecoveryProgressWeightingModel& model) {
    if (expectedRunRevision != current.runRevision ||
        expectedRecoveryEpisodeRevision != current.recoveryEpisodeRevision) {
        return coordinatorResult(RunPersistenceResultStatus::StaleDecision,
                                 persistence.state());
    }
    if (weightedProgressSegmentId == 0U ||
        !current.lastRecoveryEpisodeEvidence.has_value() ||
        !current.lastRecoveryEpisodeEvidence->weightedProgressSegmentId
             .has_value() ||
        *current.lastRecoveryEpisodeEvidence->weightedProgressSegmentId !=
            weightedProgressSegmentId) {
        return coordinatorResult(RunPersistenceResultStatus::NotAllowedInState,
                                 persistence.state());
    }
    if (!current.activeRunSensorMode.has_value() ||
        !suppliedInput.usableSensorRole.has_value() ||
        *suppliedInput.usableSensorRole != *current.activeRunSensorMode ||
        current.sensorSelectionRuntime.permission !=
            SensorPeltierPermission::Allowed) {
        return coordinatorResult(RunPersistenceResultStatus::NotAllowedInState,
                                 persistence.state());
    }

    auto input = suppliedInput;
    input.phase = current.processState.state;
    input.episodeEvidence = *current.lastRecoveryEpisodeEvidence;
    const auto contribution = model.evaluate(input);
    if (!contribution.has_value()) {
        return coordinatorResult(RunPersistenceResultStatus::NotEligible,
                                 persistence.state());
    }
    if (!isValidWeightedProgressContribution(input, *contribution)) {
        return coordinatorResult(RunPersistenceResultStatus::NotEligible,
                                 persistence.state());
    }

    if (current.runProgress.weightedProgress.has_value() &&
        current.runProgress.weightedProgress->lastApplied.has_value()) {
        const auto& last = *current.runProgress.weightedProgress->lastApplied;
        if (last.lastAppliedSegmentId == weightedProgressSegmentId) {
            return coordinatorResult(
                RunPersistenceResultStatus::AlreadyProcessed,
                persistence.state());
        }
        if (last.modelRevision != contribution->modelRevision) {
            return coordinatorResult(
                RunPersistenceResultStatus::NotAllowedInState,
                persistence.state());
        }
    }

    WeightedProgressState next;
    if (current.runProgress.weightedProgress.has_value()) {
        next = *current.runProgress.weightedProgress;
        if (contribution->delta.lowerBoundSeconds >
            std::numeric_limits<std::uint64_t>::max() -
                next.cumulative.lowerBoundSeconds) {
            return coordinatorResult(
                RunPersistenceResultStatus::CounterOverflow,
                persistence.state());
        }
        next.cumulative.lowerBoundSeconds +=
            contribution->delta.lowerBoundSeconds;
        if (next.coverage == WeightedProgressCoverage::PartialUnknown) {
            next.cumulative.upperBoundSeconds.reset();
        } else {
            if (!next.cumulative.upperBoundSeconds.has_value() ||
                *contribution->delta.upperBoundSeconds >
                    std::numeric_limits<std::uint64_t>::max() -
                        *next.cumulative.upperBoundSeconds) {
                return coordinatorResult(
                    RunPersistenceResultStatus::CounterOverflow,
                    persistence.state());
            }
            *next.cumulative.upperBoundSeconds +=
                *contribution->delta.upperBoundSeconds;
        }
    } else {
        next.coverage = WeightedProgressCoverage::Complete;
        next.cumulative = contribution->delta;
    }
    next.lastApplied = WeightedProgressProvenance{
        contribution->sourceRole, contribution->confidence,
        contribution->modelRevision, weightedProgressSegmentId};

    if (current.runRevision == std::numeric_limits<std::uint32_t>::max()) {
        return coordinatorResult(RunPersistenceResultStatus::CounterOverflow,
                                 persistence.state());
    }
    auto candidateState = current;
    candidateState.runProgress.weightedProgress = next;
    ++candidateState.runRevision;
    return persistence.persistRecoveryCandidate(current, candidateState, time);
}

}  // namespace

RunPersistenceResult RunRecoveryCoordinator::activate(
    RunPersistenceCoordinator& persistence, RunCommandState& current,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    persistence_ = &persistence;
    return activateWith(persistence, current, time, liveSensorEvidence);
}

RunPersistenceResult RunRecoveryCoordinator::activate(
    RunCommandState& current, const RunCheckpointTime& time,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    if (persistence_ == nullptr) {
        return coordinatorResult(RunPersistenceResultStatus::NotInitialized,
                                 RunPersistenceCoordinatorState::Uninitialized);
    }
    return activateWith(*persistence_, current, time, liveSensorEvidence);
}

RunPersistenceResult RunRecoveryCoordinator::reevaluateRecoveryTime(
    RunPersistenceCoordinator& persistence, RunCommandState& current,
    const RunCheckpointTime& time) {
    persistence_ = &persistence;
    return reevaluateWith(persistence, current, time, nullptr);
}

RunPersistenceResult RunRecoveryCoordinator::reevaluateRecoveryTime(
    RunPersistenceCoordinator& persistence, RunCommandState& current,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    persistence_ = &persistence;
    return reevaluateWith(persistence, current, time, &liveSensorEvidence);
}

RunPersistenceResult RunRecoveryCoordinator::reevaluateRecoveryTime(
    RunCommandState& current, const RunCheckpointTime& time) {
    if (persistence_ == nullptr) {
        return coordinatorResult(RunPersistenceResultStatus::NotInitialized,
                                 RunPersistenceCoordinatorState::Uninitialized);
    }
    return reevaluateWith(*persistence_, current, time, nullptr);
}

RunPersistenceResult RunRecoveryCoordinator::reevaluateRecoveryTime(
    RunCommandState& current, const RunCheckpointTime& time,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    if (persistence_ == nullptr) {
        return coordinatorResult(RunPersistenceResultStatus::NotInitialized,
                                 RunPersistenceCoordinatorState::Uninitialized);
    }
    return reevaluateWith(*persistence_, current, time, &liveSensorEvidence);
}

RunPersistenceResult RunRecoveryCoordinator::applyRecoveryProgressWeighting(
    RunPersistenceCoordinator& persistence, RunCommandState& current,
    std::uint32_t expectedRunRevision,
    std::uint32_t expectedRecoveryEpisodeRevision,
    std::uint32_t weightedProgressSegmentId, const RunCheckpointTime& time,
    const RecoveryProgressWeightingInput& input,
    const RecoveryProgressWeightingModel& model) {
    persistence_ = &persistence;
    return applyWeightingWith(persistence, current, expectedRunRevision,
                              expectedRecoveryEpisodeRevision,
                              weightedProgressSegmentId, time, input, model);
}

RunPersistenceResult RunRecoveryCoordinator::applyRecoveryProgressWeighting(
    RunCommandState& current, std::uint32_t expectedRunRevision,
    std::uint32_t expectedRecoveryEpisodeRevision,
    std::uint32_t weightedProgressSegmentId, const RunCheckpointTime& time,
    const RecoveryProgressWeightingInput& input,
    const RecoveryProgressWeightingModel& model) {
    if (persistence_ == nullptr) {
        return coordinatorResult(RunPersistenceResultStatus::NotInitialized,
                                 RunPersistenceCoordinatorState::Uninitialized);
    }
    return applyWeightingWith(*persistence_, current, expectedRunRevision,
                              expectedRecoveryEpisodeRevision,
                              weightedProgressSegmentId, time, input, model);
}

}  // namespace fermentation
