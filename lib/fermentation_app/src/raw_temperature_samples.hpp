#pragma once

#include <cstdint>
#include <optional>

#include "temperature_source.hpp"

namespace fermentation {

// Technical raw readings of the three DS18B20 roles as delivered by the
// existing abstract temperature sources (no filter, no quality judgement, no
// second measurement pipeline). Published once per main-loop iteration by the
// composition root (#218/R1 trial plan, S1/S6).
//
// Two independent readers exist and never influence each other:
//  * the local UI projection shows a fresh value as "raw, unqualified"
//    (never `SensorQuality::Valid`, never evidence for start or control);
//  * the firmware temperature safety supervisor evaluates the cooling-body and
//    cabinet-air readings fail-closed.
struct RawTemperatureSamples {
    std::optional<device_platform::TemperatureReading> air;
    std::optional<device_platform::TemperatureReading> product;
    std::optional<device_platform::TemperatureReading> cooling;
};

// A raw value is only shown while it is a successful reading that is not older
// than this age (about 2.5 sensor cycles; measured cycle 1.8-2.2 s).
inline constexpr std::uint64_t kRawDisplayMaxAgeMs = 5'000U;

}  // namespace fermentation
