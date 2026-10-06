#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "time_zone_rule.hpp"

namespace device_platform {

enum class TimeZonePrepareStatus : std::uint8_t {
    Success,
    UnsupportedIdentifier,
    PreparationFailed,
};

struct PreparedTimeZone {
    std::string canonicalIdentifier;
    // Einzige Regelquelle fuer die lokale Zeitumrechnung (siehe
    // `toLocalTime`); der Default `Unavailable` ist fail-closed.
    TimeZoneRule rule;
};

struct TimeZonePrepareResult {
    TimeZonePrepareStatus status{TimeZonePrepareStatus::PreparationFailed};
    std::optional<PreparedTimeZone> prepared;
};

// Schmaler, anwendungsneutraler Port fuer die Vorbereitung eines bereits
// strukturell und katalogseitig validierten kanonischen IANA-Bezeichners.
// Er kennt weder UserConfiguration noch lokale Terminplanung. Eine reale
// ESP32-Zeitzonendatenbank ist nicht Bestandteil dieses Ports; die Regel einer
// unterstuetzten Zone stammt aus der kanonischen Tabelle `kSupportedTimeZones`.
class ITimeZoneResolver {
   public:
    ITimeZoneResolver() = default;
    virtual ~ITimeZoneResolver() = default;

    ITimeZoneResolver(const ITimeZoneResolver&) = delete;
    ITimeZoneResolver& operator=(const ITimeZoneResolver&) = delete;
    ITimeZoneResolver(ITimeZoneResolver&&) = delete;
    ITimeZoneResolver& operator=(ITimeZoneResolver&&) = delete;

    [[nodiscard]] virtual TimeZonePrepareResult prepare(
        const std::string& canonicalIdentifier) const = 0;
};

}  // namespace device_platform
