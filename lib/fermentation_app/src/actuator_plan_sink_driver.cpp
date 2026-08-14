#include "actuator_plan_sink_driver.hpp"

namespace fermentation {

ActuatorPlanSinkDriver::ActuatorPlanSinkDriver(
    device_platform::IBidirectionalActuatorSink& peltier,
    device_platform::IBinaryOutputSink& outerFan,
    device_platform::IBinaryOutputSink& innerFan) noexcept
    : peltier_(peltier), outerFan_(outerFan), innerFan_(innerFan) {}

void ActuatorPlanSinkDriver::apply(const ActuatorPlanTickResult& result) {
    switch (result.appliedDirection) {
        case AbstractControlDirection::Heating:
            // The opposite H-bridge leg is explicitly disabled before the
            // fan and the requested direction are enabled.
            peltier_.setReverse(false);
            outerFan_.setEnabled(result.outerFanEnabled);
            peltier_.setForward(true);
            innerFan_.setEnabled(result.innerFanEnabled);
            return;
        case AbstractControlDirection::Cooling:
            peltier_.setForward(false);
            outerFan_.setEnabled(result.outerFanEnabled);
            peltier_.setReverse(true);
            innerFan_.setEnabled(result.innerFanEnabled);
            return;
        case AbstractControlDirection::Idle:
            peltier_.setForward(false);
            peltier_.setReverse(false);
            outerFan_.setEnabled(result.outerFanEnabled);
            innerFan_.setEnabled(result.innerFanEnabled);
            return;
        case AbstractControlDirection::Unknown:
            // Unknown output is fail-closed. This branch is defensive because
            // the planner's structural validation rejects Unknown first.
            peltier_.setForward(false);
            peltier_.setReverse(false);
            outerFan_.setEnabled(false);
            innerFan_.setEnabled(false);
            return;
    }
}

}  // namespace fermentation
