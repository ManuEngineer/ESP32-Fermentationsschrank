#include "sensor_median_filter.hpp"

#include <algorithm>
#include <cmath>

namespace device_platform {

MedianFilter::MedianFilter(std::size_t capacity) noexcept {
    if (capacity != 0U && capacity <= values_.size() && (capacity % 2U) != 0U) {
        capacity_ = capacity;
    }
}

bool MedianFilter::add(double value) noexcept {
    if (capacity_ == 0U || !std::isfinite(value)) {
        return false;
    }

    values_[nextIndex_] = value;
    nextIndex_ = (nextIndex_ + 1U) % capacity_;
    if (size_ < capacity_) {
        ++size_;
    }
    return true;
}

std::optional<double> MedianFilter::median() const noexcept {
    if (size_ == 0U) {
        return std::nullopt;
    }

    std::array<double, sensor_limits::kMaxMedianWindowSize> sorted{};
    std::copy_n(values_.begin(), size_, sorted.begin());
    std::sort(sorted.begin(), sorted.begin() + size_);
    // Bei einem teilweise gefuellten Fenster wird bewusst ein vorhandener
    // Messwert gewaehlt; es gibt keinen kuenstlichen Mittelwert zweier Werte.
    return sorted[size_ / 2U];
}

void MedianFilter::reset() noexcept {
    size_ = 0U;
    nextIndex_ = 0U;
}

}  // namespace device_platform
