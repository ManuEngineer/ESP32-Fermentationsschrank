#pragma once

#include <optional>

#include "run_recovery_time.hpp"

namespace fermentation {

struct RecoveryProgressWeightingInput {
    ProcessState phase{ProcessState::Boot};
    RecoveryEpisodeEvidence episodeEvidence;
    std::optional<RecoveryOutageBounds> outage;
    std::optional<RunSensorMode> usableSensorRole;
};

struct WeightedProgressContribution {
    WeightedProgressBounds delta;
    RunSensorMode sourceRole{RunSensorMode::Product};
    WeightedProgressConfidence confidence{
        WeightedProgressConfidence::ProductPreferred};
    std::uint32_t modelRevision{0U};
};

class RecoveryProgressWeightingModel {
   public:
    virtual ~RecoveryProgressWeightingModel() = default;
    [[nodiscard]] virtual std::optional<WeightedProgressContribution> evaluate(
        const RecoveryProgressWeightingInput& input) const = 0;
};

// Gate C: Release 1 besitzt kein freigegebenes Commissioning-/Biologiemodell.
// Dieser Provider ist deshalb absichtlich immer unavailable.
class UnavailableRecoveryProgressWeightingModel final
    : public RecoveryProgressWeightingModel {
   public:
    [[nodiscard]] std::optional<WeightedProgressContribution> evaluate(
        const RecoveryProgressWeightingInput& input) const override;
};

[[nodiscard]] bool hasUsableRecoveryProgressEvidence(
    const RecoveryProgressWeightingInput& input);
[[nodiscard]] bool isValidWeightedProgressContribution(
    const RecoveryProgressWeightingInput& input,
    const WeightedProgressContribution& contribution);

// Markiert ein offenes, noch nicht gebuchtes Segment als unbekannt. Ein
// bereits gebuchtes Segment bleibt unveraendert; nur Coverage und die
// Gesamt-Obergrenze werden konservativ angepasst.
void supersedeUnbookedWeightedSegment(
    RunProgressState& progress, std::optional<std::uint32_t> oldSegmentId);

}  // namespace fermentation
