#include "mock_time_zone_resolver.hpp"

namespace device_platform_test_support {

void MockTimeZoneResolver::setStatus(
    device_platform::TimeZonePrepareStatus status) {
    status_ = status;
}

device_platform::TimeZonePrepareResult MockTimeZoneResolver::prepare(
    const std::string& canonicalIdentifier) const {
    ++callCount_;
    lastIdentifier_ = canonicalIdentifier;
    if (status_ != device_platform::TimeZonePrepareStatus::Success) {
        return {status_, std::nullopt};
    }
    return {status_, device_platform::PreparedTimeZone{canonicalIdentifier}};
}

std::size_t MockTimeZoneResolver::callCount() const { return callCount_; }

const std::string& MockTimeZoneResolver::lastIdentifier() const {
    return lastIdentifier_;
}

}  // namespace device_platform_test_support
