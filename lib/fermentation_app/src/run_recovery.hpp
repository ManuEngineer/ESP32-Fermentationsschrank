#pragma once

#include "run_persistence_coordinator.hpp"
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

   private:
    RunPersistenceCoordinator* persistence_{nullptr};
};

}  // namespace fermentation
