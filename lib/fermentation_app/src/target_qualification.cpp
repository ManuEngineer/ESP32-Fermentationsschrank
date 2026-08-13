#include "target_qualification.hpp"

#include <cmath>
#include <limits>

#include "control_sensor_value.hpp"
#include "program_limits.hpp"

namespace fermentation {
namespace {

bool finite(double value) { return std::isfinite(value); }

void clearEpisode(TargetQualificationRuntimeState& state) {
    state.episodeActive = false;
    state.creditedInBandMillis = 0U;
    state.lastUsableTimestampMillis = std::nullopt;
    state.graceStartedAtMillis = std::nullopt;
}

bool validRole(ControlSensorRole role) {
    return role == ControlSensorRole::Air || role == ControlSensorRole::Product;
}

bool sameState(const TargetQualificationRuntimeState& left,
               const TargetQualificationRuntimeState& right) {
    return left.phase == right.phase && left.context == right.context &&
           left.commitContext == right.commitContext &&
           left.episodeActive == right.episodeActive &&
           left.creditedInBandMillis == right.creditedInBandMillis &&
           left.lastUsableTimestampMillis == right.lastUsableTimestampMillis &&
           left.graceStartedAtMillis == right.graceStartedAtMillis;
}

TargetQualificationResult result(
    QualificationProgress progress,
    const TargetQualificationRuntimeState& expected,
    const TargetQualificationRuntimeState& candidate,
    bool candidateApplicable = true) {
    return {progress,
            candidate.creditedInBandMillis,
            expected,
            candidate,
            TargetQualificationDecisionLifecycle::Pending,
            candidateApplicable};
}

bool checkedAdd(std::uint64_t left, std::uint64_t right, std::uint64_t& sum) {
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        return false;
    }
    sum = left + right;
    return true;
}

}  // namespace

void TargetQualificationEvaluator::reset() {
    const auto phase = state_.phase;
    state_ = {};
    state_.phase = phase;
}

bool TargetQualificationEvaluator::applyRamOnly(
    TargetQualificationResult& decision,
    const TargetQualificationCommitContext& currentContext) {
    if (decision.lifecycle != TargetQualificationDecisionLifecycle::Pending ||
        !decision.candidateApplicable) {
        return false;
    }
    decision.lifecycle = TargetQualificationDecisionLifecycle::Discarded;
    if (!sameState(state_, decision.expectedEvaluatorState) ||
        !decision.candidateEvaluatorState.commitContext.has_value() ||
        *decision.candidateEvaluatorState.commitContext != currentContext) {
        return false;
    }
    state_ = decision.candidateEvaluatorState;
    decision.lifecycle = TargetQualificationDecisionLifecycle::Applied;
    return true;
}

bool TargetQualificationEvaluator::applyAfterPersistedProcessApply(
    TargetQualificationResult& decision,
    const TargetQualificationCommitContext& expectedBeforeContext,
    const TargetQualificationCommitContext& committedContext) {
    if (decision.lifecycle != TargetQualificationDecisionLifecycle::Pending ||
        !decision.candidateApplicable) {
        return false;
    }
    decision.lifecycle = TargetQualificationDecisionLifecycle::Discarded;
    if (!sameState(state_, decision.expectedEvaluatorState) ||
        !decision.candidateEvaluatorState.commitContext.has_value() ||
        *decision.candidateEvaluatorState.commitContext !=
            expectedBeforeContext ||
        committedContext.qualification != expectedBeforeContext.qualification) {
        return false;
    }
    auto candidate = decision.candidateEvaluatorState;
    candidate.commitContext = committedContext;
    state_ = candidate;
    decision.lifecycle = TargetQualificationDecisionLifecycle::Applied;
    return true;
}

void TargetQualificationEvaluator::discard(
    TargetQualificationResult& decision) {
    if (decision.lifecycle == TargetQualificationDecisionLifecycle::Pending) {
        decision.lifecycle = TargetQualificationDecisionLifecycle::Discarded;
    }
}

TargetQualificationResult TargetQualificationEvaluator::evaluate(
    const TargetQualificationInput& input) {
    const auto expected = state_;
    auto candidate = state_;
    if (!validQualificationPhase(input.phase)) {
        clearEpisode(candidate);
        return result(QualificationProgress::Invalid, expected, candidate,
                      false);
    }
    if (input.phase != candidate.phase) {
        clearEpisode(candidate);
        candidate.phase = input.phase;
    }

    if (!finite(input.targetCelsius) || !finite(input.bandCelsius) ||
        input.bandCelsius < program_limits::kMinimumQualificationBandCelsius ||
        input.bandCelsius > program_limits::kMaximumQualificationBandCelsius ||
        input.qualificationDurationMillis == 0U ||
        !validRole(input.controlSensorRole)) {
        clearEpisode(candidate);
        return result(QualificationProgress::Invalid, expected, candidate);
    }
    if (!input.effectiveGraceMillis.has_value() ||
        !input.maximumAcceptedSampleGapMillis.has_value()) {
        clearEpisode(candidate);
        return result(QualificationProgress::Unavailable, expected, candidate);
    }
    if (*input.maximumAcceptedSampleGapMillis == 0U) {
        clearEpisode(candidate);
        return result(QualificationProgress::Invalid, expected, candidate);
    }

    const TargetQualificationContext context{
        input.targetCelsius, input.bandCelsius, input.controlSensorRole};
    if (!candidate.context.has_value() || !(*candidate.context == context)) {
        clearEpisode(candidate);
        candidate.context = context;
    }
    candidate.commitContext = TargetQualificationCommitContext{
        context, input.runRevision, input.processTransitionSequence};

    const auto& selectedSnapshot =
        input.phase == QualificationPhase::Preheating ||
                input.controlSensorRole == ControlSensorRole::Air
            ? input.air
            : input.product;
    double measuredCelsius = 0.0;
    const auto sampleStatus =
        readControlSensorValue(selectedSnapshot, measuredCelsius);
    if (sampleStatus == ControlSensorValueStatus::Unavailable) {
        clearEpisode(candidate);
        return result(QualificationProgress::Unavailable, expected, candidate);
    }
    if (sampleStatus == ControlSensorValueStatus::Invalid ||
        !finite(measuredCelsius)) {
        clearEpisode(candidate);
        return result(QualificationProgress::Invalid, expected, candidate);
    }

    const auto now = input.sampleTimestampMonotonicMillis;
    if (candidate.lastUsableTimestampMillis.has_value()) {
        const auto previous = *candidate.lastUsableTimestampMillis;
        if (now < previous ||
            now - previous > *input.maximumAcceptedSampleGapMillis) {
            clearEpisode(candidate);
            return result(QualificationProgress::Invalid, expected, candidate);
        }
    }
    if (candidate.graceStartedAtMillis.has_value()) {
        if (now < *candidate.graceStartedAtMillis) {
            clearEpisode(candidate);
            return result(QualificationProgress::Invalid, expected, candidate);
        }
        const auto outsideElapsed = now - *candidate.graceStartedAtMillis;
        if (outsideElapsed >= *input.effectiveGraceMillis) {
            clearEpisode(candidate);
            const bool inBand =
                std::fabs(measuredCelsius - input.targetCelsius) <=
                input.bandCelsius;
            if (!inBand) {
                return result(QualificationProgress::OutsideBand, expected,
                              candidate);
            }
            candidate.episodeActive = true;
            candidate.lastUsableTimestampMillis = now;
            return result(QualificationProgress::InBand, expected, candidate);
        }

        const bool inBand = std::fabs(measuredCelsius - input.targetCelsius) <=
                            input.bandCelsius;
        if (inBand) {
            candidate.graceStartedAtMillis = std::nullopt;
            candidate.lastUsableTimestampMillis = now;
            return result(QualificationProgress::InBand, expected, candidate);
        }
        candidate.lastUsableTimestampMillis = now;
        return result(QualificationProgress::Grace, expected, candidate);
    }

    const double deviation = std::fabs(measuredCelsius - input.targetCelsius);
    if (!finite(deviation)) {
        clearEpisode(candidate);
        return result(QualificationProgress::Invalid, expected, candidate);
    }
    const bool inBand = deviation <= input.bandCelsius;
    if (!inBand) {
        if (!candidate.episodeActive || *input.effectiveGraceMillis == 0U) {
            clearEpisode(candidate);
            return result(QualificationProgress::OutsideBand, expected,
                          candidate);
        }
        candidate.graceStartedAtMillis = now;
        candidate.lastUsableTimestampMillis = now;
        return result(QualificationProgress::Grace, expected, candidate);
    }

    if (!candidate.episodeActive) {
        candidate.episodeActive = true;
        candidate.creditedInBandMillis = 0U;
        candidate.lastUsableTimestampMillis = now;
        return result(QualificationProgress::InBand, expected, candidate);
    }

    const auto previous = candidate.lastUsableTimestampMillis.value_or(now);
    const auto delta = now - previous;
    std::uint64_t credited = 0U;
    if (!checkedAdd(candidate.creditedInBandMillis, delta, credited)) {
        clearEpisode(candidate);
        return result(QualificationProgress::Invalid, expected, candidate);
    }
    candidate.creditedInBandMillis =
        credited >= input.qualificationDurationMillis
            ? input.qualificationDurationMillis
            : credited;
    candidate.lastUsableTimestampMillis = now;
    if (candidate.creditedInBandMillis >= input.qualificationDurationMillis) {
        return result(QualificationProgress::Complete, expected, candidate);
    }
    return result(QualificationProgress::InBand, expected, candidate);
}

}  // namespace fermentation
