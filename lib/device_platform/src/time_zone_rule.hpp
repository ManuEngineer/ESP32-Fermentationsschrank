#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace device_platform {

// Regelwerk einer Zone. Keine Zeitzonendatenbank: ein Standardoffset und
// hoechstens eine Sommerzeitregel. Der Default ist fail-closed.
enum class DaylightSavingRule : std::uint8_t {
    Unavailable,
    // Letzter Sonntag im Maerz 01:00 UTC bis letzter Sonntag im Oktober
    // 01:00 UTC, +60 Minuten.
    EuropeanUnion,
};

struct TimeZoneRule {
    DaylightSavingRule dst{DaylightSavingRule::Unavailable};
    std::int16_t standardOffsetMinutes{0};
};

struct SupportedTimeZone {
    const char* canonicalIdentifier;
    TimeZoneRule rule;
};

// Einzige kanonische Tabelle der in diesem Build unterstuetzten Zonen
// (Issue #178). Resolver und Firmwarekatalog leiten davon ab.
inline constexpr SupportedTimeZone kSupportedTimeZones[] = {
    {"Europe/Zurich", {DaylightSavingRule::EuropeanUnion, 60}},
};

// Exakter Treffer auf den kanonischen Bezeichner, sonst `std::nullopt`.
[[nodiscard]] std::optional<TimeZoneRule> findTimeZoneRule(
    const std::string& canonicalIdentifier);

}  // namespace device_platform
