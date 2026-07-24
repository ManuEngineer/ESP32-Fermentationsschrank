#pragma once

#include <cstdint>
#include <string>

namespace device_platform {

enum class TimeZoneResolutionStatus : uint8_t {
    Success,
    Unknown,
    Error,
};

// Anwendungsneutraler Port zur plattformseitigen Vorbereitung einer
// kanonischen IANA-Zeitzone (z. B. Laden der Zonenregeln). Kennt keinen
// Zeitzonenkatalog und keine fachliche Bedeutung einer Zeitzone; die
// Katalogpruefung liegt bei der aufrufenden Anwendung (siehe
// docs/CONFIGURATION_PERSISTENCE.md, Abschnitt "Gespeicherte Zeitzone").
class ITimeZoneResolver {
   public:
    ITimeZoneResolver() = default;
    virtual ~ITimeZoneResolver() = default;

    ITimeZoneResolver(const ITimeZoneResolver&) = delete;
    ITimeZoneResolver& operator=(const ITimeZoneResolver&) = delete;
    ITimeZoneResolver(ITimeZoneResolver&&) = delete;
    ITimeZoneResolver& operator=(ITimeZoneResolver&&) = delete;

    [[nodiscard]] virtual TimeZoneResolutionStatus prepare(
        const std::string& ianaId) = 0;
};

}  // namespace device_platform
