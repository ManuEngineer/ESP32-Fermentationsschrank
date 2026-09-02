#include "esp_time_zone_resolver.hpp"

namespace device_platform_esp_idf {
namespace {

constexpr char kSupportedIdentifier[] = "Europe/Zurich";

}  // namespace

device_platform::TimeZonePrepareResult EspTimeZoneResolver::prepare(
    const std::string& canonicalIdentifier) const {
    if (canonicalIdentifier == kSupportedIdentifier) {
        return {device_platform::TimeZonePrepareStatus::Success,
                device_platform::PreparedTimeZone{canonicalIdentifier}};
    }
    return {device_platform::TimeZonePrepareStatus::UnsupportedIdentifier,
            std::nullopt};
}

}  // namespace device_platform_esp_idf
