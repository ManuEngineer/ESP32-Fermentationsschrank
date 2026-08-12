#pragma once

#include "run_persistence_coordinator.hpp"
#include "run_progress_weighting.hpp"
#include "run_recovery_time.hpp"

namespace fermentation {

class RunRecoveryCoordinator {
   public:
    RunRecoveryCoordinator() = default;
    explicit RunRecoveryCoordinator(RunPersistenceCoordinator& persistence)
        : persistence_(&persistence) {}

    [[nodiscard]] RunPersistenceResult activate(
        RunPersistenceCoordinator& persistence, RunCommandState& current,
        const RunCheckpointTime& time,
        const CrossRolePlausibilityContext& liveSensorEvidence);

    [[nodiscard]] RunPersistenceResult activate(
        RunCommandState& current, const RunCheckpointTime& time,
        const CrossRolePlausibilityContext& liveSensorEvidence);

    [[nodiscard]] RunPersistenceResult reevaluateRecoveryTime(
        RunPersistenceCoordinator& persistence, RunCommandState& current,
        const RunCheckpointTime& time);

    [[nodiscard]] RunPersistenceResult reevaluateRecoveryTime(
        RunPersistenceCoordinator& persistence, RunCommandState& current,
        const RunCheckpointTime& time,
        const CrossRolePlausibilityContext& liveSensorEvidence);

    [[nodiscard]] RunPersistenceResult reevaluateRecoveryTime(
        RunCommandState& current, const RunCheckpointTime& time);

    [[nodiscard]] RunPersistenceResult reevaluateRecoveryTime(
        RunCommandState& current, const RunCheckpointTime& time,
        const CrossRolePlausibilityContext& liveSensorEvidence);

    [[nodiscard]] RunPersistenceResult applyRecoveryProgressWeighting(
        RunPersistenceCoordinator& persistence, RunCommandState& current,
        std::uint32_t expectedRunRevision,
        std::uint32_t expectedRecoveryEpisodeRevision,
        std::uint32_t weightedProgressSegmentId, const RunCheckpointTime& time,
        const RecoveryProgressWeightingInput& input,
        const RecoveryProgressWeightingModel& model);

    [[nodiscard]] RunPersistenceResult applyRecoveryProgressWeighting(
        RunCommandState& current, std::uint32_t expectedRunRevision,
        std::uint32_t expectedRecoveryEpisodeRevision,
        std::uint32_t weightedProgressSegmentId, const RunCheckpointTime& time,
        const RecoveryProgressWeightingInput& input,
        const RecoveryProgressWeightingModel& model);

   private:
    RunPersistenceCoordinator* persistence_{nullptr};
};

}  // namespace fermentation
