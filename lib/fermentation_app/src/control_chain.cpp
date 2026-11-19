#include "control_chain.hpp"

namespace fermentation {

ControlChain::ControlChain(RunPersistenceCoordinator& persistence,
                           const ActuatorPlanSinkPorts& ports,
                           const ControlChainParameters& parameters)
    : controller(parameters.pi, parameters.policy),
      evaluator(),
      planner(),
      driver(ports),
      orchestrator(persistence, controller, evaluator, planner, driver) {}

void ControlChain::shutdownOutputs(std::uint64_t nowMonotonicMillis) {
    const ActuatorPlanTickResult stopped = planner.forceStop(
        nowMonotonicMillis, ActuatorFeedbackEpisodeAtStop::ExistingEpisodeOpen);
    driver.apply(stopped);
    static_cast<void>(planner.takeFeedbackUpdate());

    ActuatorPlanTickResult finalIdle;
    finalIdle.status = ActuatorPlanStatus::Idle;
    finalIdle.reason = ActuatorPlanReason::NeutralIdle;
    finalIdle.appliedDirection = AbstractControlDirection::Idle;
    finalIdle.outerFanEnabled = false;
    finalIdle.innerFanEnabled = false;
    driver.apply(finalIdle);
}

}  // namespace fermentation
