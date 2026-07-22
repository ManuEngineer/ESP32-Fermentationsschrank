#include "virtual_time_source.hpp"

namespace device_platform {

uint64_t VirtualTimeSource::monotonicMillis() const { return monotonicMillis_; }

std::optional<int64_t> VirtualTimeSource::unixTimeSeconds() const {
    return unixTimeSeconds_;
}

void VirtualTimeSource::advanceMonotonicMillis(uint64_t deltaMs) {
    monotonicMillis_ += deltaMs;
}

void VirtualTimeSource::setUnixTimeSeconds(std::optional<int64_t> unixSeconds) {
    unixTimeSeconds_ = unixSeconds;
}

}  // namespace device_platform
