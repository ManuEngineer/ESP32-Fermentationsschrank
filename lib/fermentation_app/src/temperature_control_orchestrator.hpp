#pragma once

#include <cstdint>

#include "run_persistence_coordinator.hpp"
#include "target_qualification.hpp"
#include "temperature_control.hpp"

namespace fermentation {

class RunRecoveryCoordinator;

// These are canonical lifecycle boundaries. Ordinary phase changes inside
// the same run and control context are intentionally not represented here.
enum class TemperatureControlLifecycleBoundary : std::uint8_t {
    NewActiveRun,
    LeaveTemperatureControl,
    Recovery,
    Fault,
    SafeBoot,
    Service,
    Standby,
};

// Consumes one successful persistence/apply handoff. Passing the result by
// reference makes the transient hint single-use without adding a wire or
// persistence field.
[[nodiscard]] bool consumeCommittedControlContextTransition(
    RunPersistenceResult& persisted, TemperatureController& controller);

// Resets both RAM-only control engines at an applied lifecycle boundary.
void resetTemperatureControlAtBoundary(
    TemperatureController& controller, TargetQualificationEvaluator& evaluator,
    TemperatureControlLifecycleBoundary boundary);

// The single application boundary for persisted run mutations. It consumes
// transient post-commit hints and derives full RAM resets from the committed
// before/after lifecycle, so callers cannot accidentally forget the handoff.
class TemperatureControlApplicationOrchestrator {
   public:
    TemperatureControlApplicationOrchestrator(
        RunPersistenceCoordinator& persistence,
        TemperatureController& temperatureController,
        TargetQualificationEvaluator& evaluator) noexcept;

    [[nodiscard]] RunPersistenceResult persistCommand(
        RunCommandState& current, const CommandDecision& decision,
        const RunCheckpointTime& time,
        const CrossRolePlausibilityContext* liveSensorEvidence = nullptr);
    [[nodiscard]] RunPersistenceResult persistTransition(
        RunCommandState& current, const TransitionDecision& decision,
        const RunCheckpointTime& time,
        const CrossRolePlausibilityContext* liveSensorEvidence = nullptr);
    [[nodiscard]] RunPersistenceResult persistSensorSelection(
        RunCommandState& current, const SensorSelectionStateMutation& mutation,
        const RunCheckpointTime& time,
        const CrossRolePlausibilityContext* liveSensorEvidence = nullptr);

    [[nodiscard]] RunPersistenceResult activateRecovery(
        RunRecoveryCoordinator& recovery, RunCommandState& current,
        const RunCheckpointTime& time,
        const CrossRolePlausibilityContext& liveSensorEvidence);
    [[nodiscard]] RunPersistenceResult resolveRecoveryOutcome(
        RunCommandState& current,
        const ResolveRecoveryUncertaintyRequest& request,
        const RunCheckpointTime& time,
        const CrossRolePlausibilityContext& liveSensorEvidence);
    [[nodiscard]] RunPersistenceResult reevaluateRecoveryEvaluation(
        RunCommandState& current, const RunCheckpointTime& time,
        const CrossRolePlausibilityContext* liveSensorEvidence = nullptr);

    [[nodiscard]] TargetQualificationEvaluator&
    qualificationEvaluator() noexcept {
        return evaluator_;
    }

   private:
    [[nodiscard]] RunPersistenceResult complete(RunPersistenceResult result,
                                                const RunCommandState& before,
                                                RunCommandState& current,
                                                bool recoveryBoundary = false);
    [[nodiscard]] bool needsRuntimeReset(const RunCommandState& before,
                                         const RunCommandState& after,
                                         bool recoveryBoundary) const;

    RunPersistenceCoordinator& persistence_;
    TemperatureController& temperatureController_;
    TargetQualificationEvaluator& evaluator_;
};

}  // namespace fermentation
