#pragma once

#include "actuator_plan_types.hpp"
#include "bidirectional_actuator_sink.hpp"
#include "binary_output_sink.hpp"

namespace fermentation {

// Translates one already-decided application result into the existing,
// application-neutral platform sinks. The driver owns no planning state and
// contains no GPIO or ESP-IDF knowledge.
class ActuatorPlanSinkDriver final {
   public:
    ActuatorPlanSinkDriver(
        device_platform::IBidirectionalActuatorSink& peltier,
        device_platform::IBinaryOutputSink& outerFan,
        device_platform::IBinaryOutputSink& innerFan) noexcept;

    void apply(const ActuatorPlanTickResult& result);

   private:
    device_platform::IBidirectionalActuatorSink& peltier_;
    device_platform::IBinaryOutputSink& outerFan_;
    device_platform::IBinaryOutputSink& innerFan_;
};

}  // namespace fermentation
