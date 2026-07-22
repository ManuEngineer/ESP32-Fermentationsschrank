#include "virtual_time_source.hpp"

#include <limits>

namespace device_platform {

uint64_t VirtualTimeSource::monotonicMillis() const { return monotonicMillis_; }

std::optional<int64_t> VirtualTimeSource::unixTimeSeconds() const {
    return unixTimeSeconds_;
}

void VirtualTimeSource::advanceMonotonicMillis(uint64_t deltaMs) {
    // Saturate at UINT64_MAX to keep the monotone-time guarantee even at the
    // boundary: adding any further delta must never wrap the counter back.
    const uint64_t remaining =
        std::numeric_limits<uint64_t>::max() - monotonicMillis_;
    monotonicMillis_ += (deltaMs <= remaining) ? deltaMs : remaining;
}

void VirtualTimeSource::setUnixTimeSeconds(std::optional<int64_t> unixSeconds) {
    unixTimeSeconds_ = unixSeconds;
}

}  // namespace device_platform
