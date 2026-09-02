#pragma once

#include "time_zone_resolver.hpp"

namespace device_platform_esp_idf {

class EspTimeZoneResolver final : public device_platform::ITimeZoneResolver {
   public:
    EspTimeZoneResolver() = default;

    [[nodiscard]] device_platform::TimeZonePrepareResult prepare(
        const std::string& canonicalIdentifier) const override;
};

}  // namespace device_platform_esp_idf
