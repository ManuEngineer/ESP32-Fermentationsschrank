#include "esp_timer_time_source.hpp"

#include "esp_timer.h"
#include <sys/time.h>

#include <limits>

namespace device_platform_esp_idf {

EspTimerTimeSource::EspTimerTimeSource()
    : baselineMicros_(esp_timer_get_time()) {}

uint64_t EspTimerTimeSource::monotonicMillis() const {
    // esp_timer_get_time() is monotonic, so this difference is never
    // negative for a baseline captured at or before "now".
    const int64_t elapsedMicros = esp_timer_get_time() - baselineMicros_;
    return static_cast<uint64_t>(elapsedMicros) / 1000U;
}

std::optional<int64_t> EspTimerTimeSource::unixTimeSeconds() const {
    const std::lock_guard<std::mutex> lock(trustMutex_);
    if (!publicationGate_.trusted()) {
        return std::nullopt;
    }

    const time_t current = time(nullptr);
    if constexpr (std::numeric_limits<time_t>::is_signed) {
        if (current <
                static_cast<time_t>(std::numeric_limits<int64_t>::min()) ||
            current >
                static_cast<time_t>(std::numeric_limits<int64_t>::max())) {
            return std::nullopt;
        }
    } else if (static_cast<std::uintmax_t>(current) >
               static_cast<std::uintmax_t>(
                   std::numeric_limits<int64_t>::max())) {
        return std::nullopt;
    }

    // This is the boot-local publication gate, not a second clock.  A real
    // retrograde correction is never published as trusted UTC.
    return publicationGate_.publish(static_cast<int64_t>(current));
}

bool EspTimerTimeSource::setSystemTimeUtc(
    const int64_t utcUnixSeconds) noexcept {
    const auto converted = static_cast<time_t>(utcUnixSeconds);
    if (static_cast<int64_t>(converted) != utcUnixSeconds) {
        return false;
    }
    const timeval value{converted, 0};
    return settimeofday(&value, nullptr) == 0;
}

bool EspTimerTimeSource::markAbsoluteTimeTrusted() const noexcept {
    const std::lock_guard<std::mutex> lock(trustMutex_);
    publicationGate_.markTrusted();
    return true;
}

}  // namespace device_platform_esp_idf
