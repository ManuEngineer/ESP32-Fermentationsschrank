#include "mock_time_zone_resolver.hpp"

namespace device_platform_test_support {

device_platform::TimeZoneResolutionStatus MockTimeZoneResolver::prepare(
    const std::string& ianaId) {
    if (shouldFail_) {
        return device_platform::TimeZoneResolutionStatus::Error;
    }
    if (knownZones_.find(ianaId) == knownZones_.end()) {
        return device_platform::TimeZoneResolutionStatus::Unknown;
    }
    return device_platform::TimeZoneResolutionStatus::Success;
}

void MockTimeZoneResolver::addKnownZone(const std::string& ianaId) {
    knownZones_.insert(ianaId);
}

void MockTimeZoneResolver::injectFailure(bool shouldFail) {
    shouldFail_ = shouldFail;
}

}  // namespace device_platform_test_support
