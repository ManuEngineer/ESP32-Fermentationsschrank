#include "time_zone_rule.hpp"

namespace device_platform {

std::optional<TimeZoneRule> findTimeZoneRule(
    const std::string& canonicalIdentifier) {
    for (const auto& zone : kSupportedTimeZones) {
        if (canonicalIdentifier == zone.canonicalIdentifier) return zone.rule;
    }
    return std::nullopt;
}

}  // namespace device_platform
