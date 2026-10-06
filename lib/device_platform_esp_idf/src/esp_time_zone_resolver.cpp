#include "esp_time_zone_resolver.hpp"

namespace device_platform_esp_idf {

device_platform::TimeZonePrepareResult EspTimeZoneResolver::prepare(
    const std::string& canonicalIdentifier) const {
    const auto rule = device_platform::findTimeZoneRule(canonicalIdentifier);
    if (rule.has_value()) {
        return {device_platform::TimeZonePrepareStatus::Success,
                device_platform::PreparedTimeZone{canonicalIdentifier,
                                                  *rule}};
    }
    return {device_platform::TimeZonePrepareStatus::UnsupportedIdentifier,
            std::nullopt};
}

}  // namespace device_platform_esp_idf
