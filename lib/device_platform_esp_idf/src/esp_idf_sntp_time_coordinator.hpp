#pragma once

#include <cstddef>

#include "esp_err.h"

#include "ds3231_sn_rtc_adapter.hpp"
#include "esp_timer_time_source.hpp"

namespace device_platform_esp_idf {

struct SntpServerConfiguration {
    const char* const* servers{nullptr};
    std::size_t serverCount{0U};
    bool serverFromDhcp{false};
};

// Owns the app-neutral time-platform side of ESP-IDF SNTP.  Connectivity and
// its network interface remain owned by Issue #89.  `poll()` is deliberately
// non-blocking so smooth synchronization can be observed without blocking
// application boot or the application task.
class EspIdfSntpTimeCoordinator final {
   public:
    EspIdfSntpTimeCoordinator(const EspTimerTimeSource& timeSource,
                              Ds3231SnRtcAdapter* rtc = nullptr) noexcept;
    ~EspIdfSntpTimeCoordinator();

    EspIdfSntpTimeCoordinator(const EspIdfSntpTimeCoordinator&) = delete;
    EspIdfSntpTimeCoordinator& operator=(const EspIdfSntpTimeCoordinator&) =
        delete;
    EspIdfSntpTimeCoordinator(EspIdfSntpTimeCoordinator&&) = delete;
    EspIdfSntpTimeCoordinator& operator=(EspIdfSntpTimeCoordinator&&) = delete;

    [[nodiscard]] esp_err_t initialize(
        const SntpServerConfiguration& configuration) noexcept;
    // Issue #89 may call this again after connectivity has become available.
    [[nodiscard]] esp_err_t start() noexcept;
    // Non-blocking completion observer.  Call from the platform update loop.
    void poll() noexcept;
    [[nodiscard]] bool initialized() const noexcept { return initialized_; }

   private:
    const EspTimerTimeSource& timeSource_;
    Ds3231SnRtcAdapter* rtc_{nullptr};
    bool initialized_{false};
    bool completionHandled_{false};
};

}  // namespace device_platform_esp_idf
