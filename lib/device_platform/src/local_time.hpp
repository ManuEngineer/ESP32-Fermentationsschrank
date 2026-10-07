#pragma once

#include <cstdint>
#include <optional>

#include "time_zone_resolver.hpp"

namespace device_platform {

// Renderer- und transportneutrale lokale Zeit (reine Werte).
struct LocalTime {
    std::int32_t year{0};
    std::uint8_t month{0};   // 1..12
    std::uint8_t day{0};     // 1..31
    std::uint8_t hour{0};    // 0..23
    std::uint8_t minute{0};  // 0..59
    std::uint8_t second{0};  // 0..59
    // Offset zu UTC inklusive Sommerzeit; unterscheidet den doppelten lokalen
    // Herbst-Bereich eindeutig.
    std::int16_t utcOffsetMinutes{0};
    bool daylightSaving{false};
};

// Einziger UTC->Lokal-Owner. Rein und zustandslos: keine Uhr, keine
// Konfiguration, kein Prozesszustand. Liefert `std::nullopt` ohne trusted UTC,
// fuer eine nicht vorbereitete Zone (Regel `Unavailable`) und fuer nicht
// darstellbare Instanzen; nie eine ersatzweise UTC- oder Lokalzeit.
[[nodiscard]] std::optional<LocalTime> toLocalTime(
    std::optional<std::int64_t> trustedUtc, const TimeZoneRule& rule) noexcept;

// Convenience for callers holding the prepared zone; identical result.
[[nodiscard]] inline std::optional<LocalTime> toLocalTime(
    const std::optional<std::int64_t> trustedUtc,
    const PreparedTimeZone& zone) noexcept {
    return toLocalTime(trustedUtc, zone.rule);
}

}  // namespace device_platform
