#pragma once

#include <cstdint>

#include "run_persistence_coordinator.hpp"
#include "target_qualification.hpp"
#include "temperature_control.hpp"

namespace fermentation {

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

}  // namespace fermentation
