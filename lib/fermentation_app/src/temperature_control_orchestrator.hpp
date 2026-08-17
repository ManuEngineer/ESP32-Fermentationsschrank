#pragma once

#include <cstdint>
#include <optional>

#include "actuator_plan_sink_driver.hpp"
#include "actuator_planner.hpp"
#include "process_state_machine.hpp"
#include "run_persistence_coordinator.hpp"
#include "sensor_quality_snapshot.hpp"
#include "target_qualification.hpp"
#include "temperature_control.hpp"

namespace fermentation {

class RunRecoveryCoordinator;
class SafetyCore;

// The only dynamic PI evidence a caller may supply. Target, role, and
// context identity are always derived internally from the live
// RunCommandState via resolveEffectiveControlContext() - never taken from a
// caller-suppliable field.
struct TemperatureControlEvaluationEvidence {
    std::uint64_t sampleTimestampMonotonicMillis{0U};
    device_platform::SensorQualitySnapshot air;
    device_platform::SensorQualitySnapshot product;
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

// Lifecycle-Entscheidungen benoetigen nur die Prozessphase. Der vollstaendige
// RunCommandState bleibt an der Application-Grenze und wird nicht fuer diese
// fluechtige Vorher-Nachher-Pruefung kopiert.
struct TemperatureControlLifecycleSnapshot {
    ProcessState processState{ProcessState::Boot};
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

// Resets the planner and sink output at the same committed lifecycle boundary
// as the #22 runtime. The transient feedback/evaluation slots are terminally
// cleared after the common stop output has been applied.
void resetActuatorPlanAtBoundary(
    ActuatorPlanner& planner, ActuatorPlanSinkDriver& driver,
    TemperatureControlLifecycleBoundary boundary,
    std::uint64_t nowMonotonicMillis,
    std::optional<PreviousControlRequestFeedback>& applicationPendingFeedback,
    std::optional<TemperatureControlResult>& outstandingEvaluation);

// The single application boundary for persisted run mutations. It consumes
// transient post-commit hints and derives full RAM resets from the committed
// before/after lifecycle, so callers cannot accidentally forget the handoff.
class TemperatureControlApplicationOrchestrator {
   public:
    TemperatureControlApplicationOrchestrator(
        RunPersistenceCoordinator& persistence,
        TemperatureController& temperatureController,
        TargetQualificationEvaluator& evaluator) noexcept;
    TemperatureControlApplicationOrchestrator(
        RunPersistenceCoordinator& persistence,
        TemperatureController& temperatureController,
        TargetQualificationEvaluator& evaluator, ActuatorPlanner& planner,
        ActuatorPlanSinkDriver& driver) noexcept;

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
    //
    // Owner-Review F4: an active PI evaluation (one that could produce a
    // Heating/Cooling ControlRequest) requires the planner-/driver-bound
    // 5-argument constructor, because #23's feedback handoff is the only
    // source of anti-windup feedback for #22. Without it, this method
    // unconditionally returns Unavailable/NoCommissioning instead of ever
    // running the PI core - not just on the first call, but on every call -
    // so a caller can never observe a silent "PI running without feedback"
    // mode. The 3-argument constructor remains valid for fixtures that
    // exercise only the persistence/lifecycle boundary and never call this
    // method for an active result.
    [[nodiscard]] TemperatureControlResult evaluateTemperatureControl(
        const RunCommandState& current,
        const TemperatureControlEvaluationEvidence& evidence);

    // The single application-to-planner boundary. The stored #22 evaluation
    // is consumed exactly once; the caller cannot inject an alternative
    // evaluation or feedback value.
    [[nodiscard]] ActuatorPlanTickResult tickActuatorPlan(
        const RunCommandState& current, std::uint64_t nowMonotonicMillis,
        ActuatorSafetyGateInput safetyGate = {});

    // Once bound, the planner consumes the latest SafetyCore decision and
    // ignores caller-supplied gate status. This keeps an Allowed value from
    // becoming a second safety authority at the application boundary.
    void bindSafetyCore(SafetyCore& safetyCore) noexcept {
        safetyCore_ = &safetyCore;
    }

   private:
    [[nodiscard]] RunPersistenceResult complete(
        RunPersistenceResult result,
        const TemperatureControlLifecycleSnapshot& before,
        RunCommandState& current, std::uint64_t nowMonotonicMillis,
        bool recoveryBoundary = false);
    [[nodiscard]] bool needsRuntimeReset(
        const TemperatureControlLifecycleSnapshot& before, ProcessState after,
        bool recoveryBoundary, bool newActiveRun) const;

    RunPersistenceCoordinator& persistence_;
    TemperatureController& temperatureController_;
    TargetQualificationEvaluator& evaluator_;
    ActuatorPlanner* planner_{nullptr};
    ActuatorPlanSinkDriver* actuatorDriver_{nullptr};
    SafetyCore* safetyCore_{nullptr};
    std::optional<TemperatureControlResult> outstandingEvaluation_;
    std::optional<PreviousControlRequestFeedback>
        pendingControlRequestFeedback_;
};

}  // namespace fermentation
