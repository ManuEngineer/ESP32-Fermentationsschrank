#pragma once

#include <cstdint>
#include <optional>

#include "run_persistence_coordinator.hpp"
#include "sensor_quality_snapshot.hpp"
#include "target_qualification.hpp"
#include "temperature_control.hpp"

namespace fermentation {

class RunRecoveryCoordinator;

// The only dynamic PI evidence a caller may supply. Target, role, and
// context identity are always derived internally from the live
// RunCommandState via resolveEffectiveControlContext() - never taken from a
// caller-suppliable field.
struct TemperatureControlEvaluationEvidence {
    std::uint64_t sampleTimestampMonotonicMillis{0U};
    device_platform::SensorQualitySnapshot air;
    device_platform::SensorQualitySnapshot product;
    std::optional<PreviousControlRequestFeedback>
        previousControlRequestFeedback;
};

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

    // The single canonical PI-evaluation boundary (FR1): resolves the
    // effective control context from the live RunCommandState and feeds it,
    // together with the caller's sensor evidence, into the pure
    // TemperatureController. A caller cannot supply its own target, role, or
    // context identity. Fail-closed (no valid ControlRequest) when the
    // current phase/run is not temperature-controlled or is structurally
    // inconsistent.
    [[nodiscard]] TemperatureControlResult evaluateTemperatureControl(
        const RunCommandState& current,
        const TemperatureControlEvaluationEvidence& evidence);

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
