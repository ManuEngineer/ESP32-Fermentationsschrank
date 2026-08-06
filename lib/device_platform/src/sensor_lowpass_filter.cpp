#include "sensor_lowpass_filter.hpp"

#include <cmath>

namespace device_platform {

LowPassFilter::LowPassFilter(double tauSeconds) noexcept
    : tauSeconds_(tauSeconds) {}

std::optional<double> LowPassFilter::update(double input,
                                            uint64_t timestampMs) noexcept {
    if (!std::isfinite(input) || !std::isfinite(tauSeconds_) ||
        tauSeconds_ <= 0.0) {
        return std::nullopt;
    }

    if (!filteredValue_.has_value()) {
        filteredValue_ = input;
        lastTimestampMs_ = timestampMs;
        return filteredValue_;
    }

    if (!lastTimestampMs_.has_value() || timestampMs < *lastTimestampMs_) {
        return std::nullopt;
    }

    const double dtSeconds =
        static_cast<double>(timestampMs - *lastTimestampMs_) / 1000.0;
    const double alpha = 1.0 - std::exp(-dtSeconds / tauSeconds_);
    *filteredValue_ += (input - *filteredValue_) * alpha;
    lastTimestampMs_ = timestampMs;
    return filteredValue_;
}

void LowPassFilter::shiftState(double delta) noexcept {
    if (filteredValue_.has_value() && std::isfinite(delta)) {
        *filteredValue_ += delta;
    }
}

void LowPassFilter::reset() noexcept {
    filteredValue_ = std::nullopt;
    lastTimestampMs_ = std::nullopt;
}

}  // namespace device_platform
