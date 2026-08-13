#include "control_sensor_value.hpp"

#include <cmath>

namespace fermentation {

ControlSensorValueStatus readControlSensorValue(
    const device_platform::SensorQualitySnapshot& snapshot, double& value) {
    if (snapshot.quality != device_platform::SensorQuality::Valid) {
        return ControlSensorValueStatus::Unavailable;
    }
    if (!snapshot.filteredCelsius.has_value() ||
        !std::isfinite(*snapshot.filteredCelsius)) {
        return ControlSensorValueStatus::Invalid;
    }
    value = *snapshot.filteredCelsius;
    return ControlSensorValueStatus::Valid;
}

}  // namespace fermentation
