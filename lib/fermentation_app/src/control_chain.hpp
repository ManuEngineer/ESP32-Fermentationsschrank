#pragma once

#include <cstdint>

#include "actuator_plan_sink_driver.hpp"
#include "actuator_planner.hpp"
#include "run_persistence_coordinator.hpp"
#include "target_qualification.hpp"
#include "temperature_control.hpp"
#include "temperature_control_orchestrator.hpp"
#include "temperature_control_types.hpp"

namespace fermentation {

// The only injection point for the PI regulator configuration of the R1
// control chain (#218). The product passes the named unconfigured constant
// below until #35 releases real values; the existing parameter
// classification then reports Unconfigured/NoCommissioning and no
// ControlRequest is ever produced (fail-closed).
struct ControlChainParameters {
    TemperatureControlParameters pi;
    IntegratorTransitionPolicy policy;
};

inline const ControlChainParameters
    kControlChainParametersUnconfiguredUntilIssue35{};

// Private regulator/planner/driver composition of FermentationApplication.
// Holds no persistence of its own: the orchestrator references the
// application's coordinator, so the chain must be destroyed before it.
class ControlChain final {
   public:
    ControlChain(RunPersistenceCoordinator& persistence,
                 const ActuatorPlanSinkPorts& ports,
                 const ControlChainParameters& parameters);
    ControlChain(const ControlChain&) = delete;
    ControlChain& operator=(const ControlChain&) = delete;

    // Final teardown, distinct from the normal run STOP: the running control
    // is stopped first, then an explicit Idle result with both fans off is
    // applied through the existing driver, because the planner (and with it
    // any permitted fan run-on) is about to be destroyed.
    void shutdownOutputs(std::uint64_t nowMonotonicMillis);

    TemperatureController controller;
    TargetQualificationEvaluator evaluator;
    ActuatorPlanner planner;
    ActuatorPlanSinkDriver driver;
    TemperatureControlApplicationOrchestrator orchestrator;
};

}  // namespace fermentation
