#include "target_qualification.hpp"

#include <cmath>
#include <limits>

#include "sensor_quality_snapshot.hpp"

namespace fermentation {
namespace {

using device_platform::SensorQuality;
using device_platform::SensorQualitySnapshot;

bool finite(double value) { return std::isfinite(value); }

void clearEpisode(TargetQualificationRuntimeState& state) {
    state.episodeActive = false;
    state.creditedInBandMillis = 0U;
    state.lastUsableTimestampMillis = std::nullopt;
    state.graceStartedAtMillis = std::nullopt;
}

std::optional<double> snapshotValue(const SensorQualitySnapshot& snapshot) {
    const std::optional<double>* value = &snapshot.filteredCelsius;
    if (!value->has_value()) {
        value = &snapshot.correctedCelsius;
    }
    if (!value->has_value()) {
        value = &snapshot.rawCelsius;
    }
    if (!value->has_value() || !finite(**value)) {
        return std::nullopt;
    }
    return **value;
}

enum class SampleStatus : std::uint8_t {
    Valid,
    Unavailable,
    Invalid,
};

SampleStatus readSnapshot(const SensorQualitySnapshot& snapshot,
                          double& value) {
    if (snapshot.quality != SensorQuality::Valid) {
        return SampleStatus::Unavailable;
    }
    const auto candidate = snapshotValue(snapshot);
    if (!candidate.has_value()) {
        return SampleStatus::Invalid;
    }
    value = *candidate;
    return SampleStatus::Valid;
}

bool validRole(ControlSensorRole role) {
    return role == ControlSensorRole::Air ||
           role == ControlSensorRole::Product;
}

TargetQualificationResult result(QualificationProgress progress,
                                 const TargetQualificationRuntimeState& state) {
    return {progress, state.creditedInBandMillis,
            state.episodeActive && state.lastUsableTimestampMillis.has_value()
                ? state.lastUsableTimestampMillis
                : std::nullopt};
}

bool checkedAdd(std::uint64_t left, std::uint64_t right,
                std::uint64_t& sum) {
    if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        return false;
    }
    sum = left + right;
    return true;
}

}  // namespace

void TargetQualificationEvaluator::reset() { clearEpisode(state_); }

TargetQualificationResult TargetQualificationEvaluator::evaluate(
    const TargetQualificationInput& input) {
    if (input.phase != state_.phase) {
        clearEpisode(state_);
        state_.phase = input.phase;
    }

    if (!finite(input.targetCelsius) || !finite(input.bandCelsius) ||
        input.bandCelsius <= 0.0 || input.qualificationDurationMillis == 0U ||
        !validRole(input.controlSensorRole) ||
        !input.effectiveGraceMillis.has_value() ||
        !input.maximumAcceptedSampleGapMillis.has_value()) {
        clearEpisode(state_);
        return result(QualificationProgress::Unavailable, state_);
    }
    if (*input.maximumAcceptedSampleGapMillis == 0U) {
        clearEpisode(state_);
        return result(QualificationProgress::Invalid, state_);
    }

    const auto& selectedSnapshot =
        input.phase == QualificationPhase::Preheating ||
                input.controlSensorRole == ControlSensorRole::Air
            ? input.air
            : input.product;
    double measuredCelsius = 0.0;
    const auto sampleStatus = readSnapshot(selectedSnapshot, measuredCelsius);
    if (sampleStatus == SampleStatus::Unavailable) {
        clearEpisode(state_);
        return result(QualificationProgress::Unavailable, state_);
    }
    if (sampleStatus == SampleStatus::Invalid || !finite(measuredCelsius)) {
        clearEpisode(state_);
        return result(QualificationProgress::Invalid, state_);
    }

    const auto now = input.sampleTimestampMonotonicMillis;
    if (state_.lastUsableTimestampMillis.has_value()) {
        const auto previous = *state_.lastUsableTimestampMillis;
        if (now < previous ||
            now - previous > *input.maximumAcceptedSampleGapMillis) {
            clearEpisode(state_);
            return result(QualificationProgress::Invalid, state_);
        }
    }
    if (state_.graceStartedAtMillis.has_value()) {
        if (now < *state_.graceStartedAtMillis) {
            clearEpisode(state_);
            return result(QualificationProgress::Invalid, state_);
        }
        const auto outsideElapsed = now - *state_.graceStartedAtMillis;
        if (outsideElapsed >= *input.effectiveGraceMillis) {
            clearEpisode(state_);
            const bool inBand =
                std::fabs(measuredCelsius - input.targetCelsius) <=
                input.bandCelsius;
            if (!inBand) {
                return result(QualificationProgress::OutsideBand, state_);
            }
            state_.episodeActive = true;
            state_.lastUsableTimestampMillis = now;
            return result(QualificationProgress::InBand, state_);
        }

        const bool inBand =
            std::fabs(measuredCelsius - input.targetCelsius) <=
            input.bandCelsius;
        if (inBand) {
            state_.graceStartedAtMillis = std::nullopt;
            state_.lastUsableTimestampMillis = now;
            return result(QualificationProgress::InBand, state_);
        }
        state_.lastUsableTimestampMillis = now;
        return result(QualificationProgress::Grace, state_);
    }

    if (state_.lastUsableTimestampMillis.has_value()) {
        const auto previous = *state_.lastUsableTimestampMillis;
        if (now < previous) {
            clearEpisode(state_);
            return result(QualificationProgress::Invalid, state_);
        }
        const auto gap = now - previous;
        if (gap > *input.maximumAcceptedSampleGapMillis) {
            clearEpisode(state_);
            return result(QualificationProgress::Invalid, state_);
        }
    }

    const double deviation = std::fabs(measuredCelsius - input.targetCelsius);
    if (!finite(deviation)) {
        clearEpisode(state_);
        return result(QualificationProgress::Invalid, state_);
    }
    const bool inBand = deviation <= input.bandCelsius;
    if (!inBand) {
        if (!state_.episodeActive || *input.effectiveGraceMillis == 0U) {
            clearEpisode(state_);
            return result(QualificationProgress::OutsideBand, state_);
        }
        state_.graceStartedAtMillis = now;
        state_.lastUsableTimestampMillis = now;
        return result(QualificationProgress::Grace, state_);
    }

    if (!state_.episodeActive) {
        state_.episodeActive = true;
        state_.creditedInBandMillis = 0U;
        state_.lastUsableTimestampMillis = now;
        return result(QualificationProgress::InBand, state_);
    }

    const auto previous = state_.lastUsableTimestampMillis.value_or(now);
    const auto delta = now - previous;
    std::uint64_t credited = 0U;
    if (!checkedAdd(state_.creditedInBandMillis, delta, credited)) {
        clearEpisode(state_);
        return result(QualificationProgress::Invalid, state_);
    }
    state_.creditedInBandMillis =
        credited >= input.qualificationDurationMillis
            ? input.qualificationDurationMillis
            : credited;
    state_.lastUsableTimestampMillis = now;
    if (state_.creditedInBandMillis >= input.qualificationDurationMillis) {
        return result(QualificationProgress::Complete, state_);
    }
    return result(QualificationProgress::InBand, state_);
}

}  // namespace fermentation
