#pragma once

#include <cstddef>
#include <string>

#include "time_zone_resolver.hpp"

namespace device_platform_test_support {

class MockTimeZoneResolver final : public device_platform::ITimeZoneResolver {
   public:
    void setStatus(device_platform::TimeZonePrepareStatus status);

    [[nodiscard]] device_platform::TimeZonePrepareResult prepare(
        const std::string& canonicalIdentifier) const override;

    [[nodiscard]] std::size_t callCount() const;
    [[nodiscard]] const std::string& lastIdentifier() const;

   private:
    device_platform::TimeZonePrepareStatus status_{
        device_platform::TimeZonePrepareStatus::Success};
    mutable std::size_t callCount_{0U};
    mutable std::string lastIdentifier_;
};

}  // namespace device_platform_test_support
