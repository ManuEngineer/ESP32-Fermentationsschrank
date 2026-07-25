#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace device_platform {

enum class TimeZonePrepareStatus : std::uint8_t {
    Success,
    UnsupportedIdentifier,
    PreparationFailed,
};

struct PreparedTimeZone {
    std::string canonicalIdentifier;
};

struct TimeZonePrepareResult {
    TimeZonePrepareStatus status{TimeZonePrepareStatus::PreparationFailed};
    std::optional<PreparedTimeZone> prepared;
};

// Schmaler, anwendungsneutraler Port fuer die Vorbereitung eines bereits
// strukturell und katalogseitig validierten kanonischen IANA-Bezeichners.
// Er kennt weder UserConfiguration noch lokale Terminplanung. Eine reale
// ESP32-Zeitzonendatenbank ist nicht Bestandteil dieses Ports.
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
