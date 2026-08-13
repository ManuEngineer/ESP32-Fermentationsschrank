#pragma once

#include <cstdint>

#include "sensor_quality_snapshot.hpp"

namespace fermentation {

enum class ControlSensorValueStatus : std::uint8_t {
    Valid,
    Unavailable,
    Invalid,
};

// The single #22 control-/qualification-value reader. Per issue #20,
// `filteredCelsius` is the only normal control value; `rawCelsius` (raw,
// possibly implausible/extreme) and `correctedCelsius` (median/offset
// intermediate) remain diagnostic and must never be used as a fallback here.
//
// quality != Valid                        -> Unavailable
// quality == Valid, filteredCelsius absent -> Invalid (structurally
//                                             inconsistent evidence, no raw
//                                             fallback)
// quality == Valid, filteredCelsius non-finite -> Invalid
// quality == Valid, filteredCelsius finite -> Valid, `value` set
[[nodiscard]] ControlSensorValueStatus readControlSensorValue(
    const device_platform::SensorQualitySnapshot& snapshot, double& value);

}  // namespace fermentation
