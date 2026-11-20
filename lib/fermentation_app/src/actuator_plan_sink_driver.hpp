#pragma once

#include "actuator_plan_types.hpp"
#include "bidirectional_actuator_sink.hpp"
#include "binary_output_sink.hpp"

namespace fermentation {

// The three abstract sinks the driver translates into. Composition roots hand
// them over as one value so that no other fermentation_app type has to name a
// sink interface (the driver stays the only sink user, see SIM-19-S1-03).
struct ActuatorPlanSinkPorts {
    device_platform::IBidirectionalActuatorSink* peltier{nullptr};
    device_platform::IBinaryOutputSink* outerFan{nullptr};
    device_platform::IBinaryOutputSink* innerFan{nullptr};

    [[nodiscard]] bool complete() const noexcept {
        return peltier != nullptr && outerFan != nullptr && innerFan != nullptr;
    }
};

// Translates one already-decided application result into the existing,
// application-neutral platform sinks. The driver owns no planning state and
// contains no GPIO or ESP-IDF knowledge.
class ActuatorPlanSinkDriver final {
   public:
    ActuatorPlanSinkDriver(
        device_platform::IBidirectionalActuatorSink& peltier,
        device_platform::IBinaryOutputSink& outerFan,
        device_platform::IBinaryOutputSink& innerFan) noexcept;
    // Precondition: ports.complete().
    explicit ActuatorPlanSinkDriver(const ActuatorPlanSinkPorts& ports) noexcept
        : ActuatorPlanSinkDriver(*ports.peltier, *ports.outerFan,
                                 *ports.innerFan) {}

    void apply(const ActuatorPlanTickResult& result);

   private:
    device_platform::IBidirectionalActuatorSink& peltier_;
    device_platform::IBinaryOutputSink& outerFan_;
    device_platform::IBinaryOutputSink& innerFan_;
};

}  // namespace fermentation
