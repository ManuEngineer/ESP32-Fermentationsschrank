#include "run_progress_weighting.hpp"

#include <limits>

namespace fermentation {
namespace {

const RoleTemperatureEvidence* beforeEvidenceFor(
    const RecoveryProgressWeightingInput& input) {
    if (!input.usableSensorRole.has_value()) return nullptr;
    return *input.usableSensorRole == RunSensorMode::Product
               ? &input.episodeEvidence.beforeOutage.product
               : &input.episodeEvidence.beforeOutage.air;
}

const RoleTemperatureEvidence* firstEvidenceFor(
    const RecoveryProgressWeightingInput& input) {
    if (!input.usableSensorRole.has_value()) return nullptr;
    const auto& first = input.episodeEvidence.firstAfterRestart;
    const auto& selected = *input.usableSensorRole == RunSensorMode::Product
                               ? first.product
                               : first.air;
    return selected.has_value() ? &*selected : nullptr;
}

bool validTemperatureEvidence(const RoleTemperatureEvidence& evidence) {
    return evidence.quality == device_platform::SensorQuality::Valid &&
           evidence.filteredCelsius.has_value();
}

}  // namespace

std::optional<WeightedProgressContribution>
UnavailableRecoveryProgressWeightingModel::evaluate(
    const RecoveryProgressWeightingInput& input) const {
    static_cast<void>(input);
    return std::nullopt;
}

bool hasUsableRecoveryProgressEvidence(
    const RecoveryProgressWeightingInput& input) {
    if (input.phase != ProcessState::Fermenting || !input.outage.has_value() ||
        !input.usableSensorRole.has_value()) {
        return false;
    }
    const auto* before = beforeEvidenceFor(input);
    const auto* first = firstEvidenceFor(input);
    return before != nullptr && first != nullptr &&
           validTemperatureEvidence(*before) &&
           validTemperatureEvidence(*first);
}

bool isValidWeightedProgressContribution(
    const RecoveryProgressWeightingInput& input,
    const WeightedProgressContribution& contribution) {
    if (!hasUsableRecoveryProgressEvidence(input) ||
        !input.usableSensorRole.has_value() ||
        contribution.sourceRole != *input.usableSensorRole ||
        contribution.modelRevision == 0U ||
        !contribution.delta.upperBoundSeconds.has_value() ||
        contribution.delta.lowerBoundSeconds >
            *contribution.delta.upperBoundSeconds) {
        return false;
    }
    const auto expectedConfidence =
        contribution.sourceRole == RunSensorMode::Air
            ? WeightedProgressConfidence::AirReduced
            : WeightedProgressConfidence::ProductPreferred;
    return contribution.confidence == expectedConfidence;
}

void supersedeUnbookedWeightedSegment(
    RunProgressState& progress, std::optional<std::uint32_t> oldSegmentId) {
    if (!oldSegmentId.has_value() || *oldSegmentId == 0U) return;
    if (progress.weightedProgress.has_value() &&
        progress.weightedProgress->lastApplied.has_value() &&
        progress.weightedProgress->lastApplied->lastAppliedSegmentId ==
            *oldSegmentId) {
        return;
    }

    if (!progress.weightedProgress.has_value()) {
        progress.weightedProgress = WeightedProgressState{};
    }
    auto& weighted = *progress.weightedProgress;
    weighted.coverage = WeightedProgressCoverage::PartialUnknown;
    weighted.cumulative.upperBoundSeconds.reset();
}

}  // namespace fermentation
