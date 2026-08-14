#include "actuator_plan_sink_driver.hpp"

namespace fermentation {

namespace {

// Owner-Review F2: validated exhaustively ahead of the switch below (rather
// than via a switch `default:`) so the compiler's covered-switch-default
// warning gate stays clean while a structurally corrupt value (e.g. an
// invalid cast) is still recognized and fail-closed, not silently dropped.
[[nodiscard]] bool isKnownDirection(AbstractControlDirection direction) {
    switch (direction) {
        case AbstractControlDirection::Heating:
        case AbstractControlDirection::Cooling:
        case AbstractControlDirection::Idle:
        case AbstractControlDirection::Unknown:
            return true;
    }
    return false;
}

}  // namespace

ActuatorPlanSinkDriver::ActuatorPlanSinkDriver(
    device_platform::IBidirectionalActuatorSink& peltier,
    device_platform::IBinaryOutputSink& outerFan,
    device_platform::IBinaryOutputSink& innerFan) noexcept
    : peltier_(peltier), outerFan_(outerFan), innerFan_(innerFan) {}

void ActuatorPlanSinkDriver::apply(const ActuatorPlanTickResult& result) {
    if (!isKnownDirection(result.appliedDirection)) {
        // Owner-Review F2: a direction outside the four defined enumerators
        // (e.g. a corrupted cast bypassing the planner's own structural
        // validation) is fail-closed identically to Unknown; it must never
        // leave a previously applied H-bridge state standing.
        peltier_.setForward(false);
        peltier_.setReverse(false);
        outerFan_.setEnabled(false);
        innerFan_.setEnabled(false);
        return;
    }

    switch (result.appliedDirection) {
        case AbstractControlDirection::Heating:
            // The opposite H-bridge leg is explicitly disabled before the
            // fan and the requested direction are enabled. Owner-Review F3:
            // the outer fan is unconditionally enabled here, never taken
            // from result.outerFanEnabled - Peltier power must never be
            // released without it, regardless of an inconsistent result.
            peltier_.setReverse(false);
            outerFan_.setEnabled(true);
            peltier_.setForward(true);
            innerFan_.setEnabled(result.innerFanEnabled);
            return;
        case AbstractControlDirection::Cooling:
            peltier_.setForward(false);
            outerFan_.setEnabled(true);
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
