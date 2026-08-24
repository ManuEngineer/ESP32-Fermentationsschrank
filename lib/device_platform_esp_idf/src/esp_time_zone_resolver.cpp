#include "esp_time_zone_resolver.hpp"

#include <algorithm>
#include <array>

namespace device_platform_esp_idf {
namespace {

constexpr std::array<const char*, 1U> kSupportedTimeZoneIds{
    "Europe/Zurich",
};

}  // namespace

device_platform::TimeZonePrepareResult EspTimeZoneResolver::prepare(
    const std::string& canonicalIdentifier) const {
    const auto supported =
        std::find(kSupportedTimeZoneIds.begin(), kSupportedTimeZoneIds.end(),
                  canonicalIdentifier);
    if (supported == kSupportedTimeZoneIds.end()) {
        return {device_platform::TimeZonePrepareStatus::UnsupportedIdentifier,
                std::nullopt};
    }

    return {device_platform::TimeZonePrepareStatus::Success,
            device_platform::PreparedTimeZone{canonicalIdentifier}};
}

}  // namespace device_platform_esp_idf
