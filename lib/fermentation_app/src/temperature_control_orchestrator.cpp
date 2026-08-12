#include "temperature_control_orchestrator.hpp"

namespace fermentation {

bool consumeCommittedControlContextTransition(
    RunPersistenceResult& persisted, TemperatureController& controller) {
    if (persisted.status != RunPersistenceResultStatus::Applied ||
        !persisted.committedControlContextTransition.has_value()) {
        return false;
    }
    const auto transition = *persisted.committedControlContextTransition;
    // A failed mark is also consumed: a committed hint must never be
    // reinterpreted later as a second successful handoff.
    persisted.committedControlContextTransition.reset();
    return controller.markCommittedControlContextTransitionPending(transition);
}

void resetTemperatureControlAtBoundary(
    TemperatureController& controller, TargetQualificationEvaluator& evaluator,
    TemperatureControlLifecycleBoundary boundary) {
    switch (boundary) {
        case TemperatureControlLifecycleBoundary::NewActiveRun:
        case TemperatureControlLifecycleBoundary::LeaveTemperatureControl:
        case TemperatureControlLifecycleBoundary::Recovery:
        case TemperatureControlLifecycleBoundary::Fault:
        case TemperatureControlLifecycleBoundary::SafeBoot:
        case TemperatureControlLifecycleBoundary::Service:
        case TemperatureControlLifecycleBoundary::Standby:
            controller.resetRuntime();
            evaluator.reset();
            return;
    }
}

}  // namespace fermentation
