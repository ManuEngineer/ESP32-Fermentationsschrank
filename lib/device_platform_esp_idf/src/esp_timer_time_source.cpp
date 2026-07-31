#include "esp_timer_time_source.hpp"

#include "esp_timer.h"

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
    return std::nullopt;
}

}  // namespace device_platform_esp_idf
