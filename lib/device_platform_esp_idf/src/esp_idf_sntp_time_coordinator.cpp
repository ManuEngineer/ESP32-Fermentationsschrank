#include "esp_idf_sntp_time_coordinator.hpp"

#include <algorithm>

#include "esp_netif_sntp.h"
#include "esp_sntp.h"

namespace device_platform_esp_idf {

EspIdfSntpTimeCoordinator::EspIdfSntpTimeCoordinator(
    const EspTimerTimeSource& timeSource, Ds3231SnRtcAdapter* rtc) noexcept
    : timeSource_(timeSource), rtc_(rtc) {}

EspIdfSntpTimeCoordinator::~EspIdfSntpTimeCoordinator() {
    if (initialized_) esp_netif_sntp_deinit();
}

esp_err_t EspIdfSntpTimeCoordinator::initialize(
    const SntpServerConfiguration& configuration) noexcept {
    if (initialized_) return ESP_ERR_INVALID_STATE;
    if (configuration.serverCount > 0U && configuration.servers == nullptr)
        return ESP_ERR_INVALID_ARG;

    esp_sntp_config_t config{};
    config.smooth_sync = true;
    config.server_from_dhcp = configuration.serverFromDhcp;
    config.wait_for_sync = false;
    config.start = false;
    config.renew_servers_after_new_IP = false;
    config.index_of_first_server = 0U;
    config.num_of_servers =
        std::min(configuration.serverCount,
                 static_cast<std::size_t>(CONFIG_LWIP_SNTP_MAX_SERVERS));
    for (std::size_t index = 0U; index < config.num_of_servers; ++index)
        config.servers[index] = configuration.servers[index];

    const auto status = esp_netif_sntp_init(&config);
    if (status == ESP_OK) {
        initialized_ = true;
        completionHandled_ = false;
    }
    return status;
}

esp_err_t EspIdfSntpTimeCoordinator::start() noexcept {
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    completionHandled_ = false;
    return esp_netif_sntp_start();
}

void EspIdfSntpTimeCoordinator::poll() noexcept {
    if (!initialized_) return;
    const auto status = sntp_get_sync_status();
    if (status == SNTP_SYNC_STATUS_IN_PROGRESS) {
        // In smooth mode, the response can be accepted while adjtime is still
        // converging.  RTC synchronization is intentionally deferred until
        // the later COMPLETED observation.
        completionHandled_ = false;
        return;
    }
    if (status == SNTP_SYNC_STATUS_RESET) {
        completionHandled_ = false;
        return;
    }
    if (status != SNTP_SYNC_STATUS_COMPLETED || completionHandled_) return;

    // ESP-IDF has already completed the system-time update.  Marking the
    // local clock trusted does not bypass EspTimerTimeSource's retrograde
    // publication gate.
    static_cast<void>(timeSource_.markAbsoluteTimeTrusted());
    const auto currentUtc = timeSource_.unixTimeSeconds();
    if (currentUtc.has_value() && rtc_ != nullptr && rtc_->present() &&
        rtc_->initialized()) {
        // A failed RTC write does not revoke the current NTP-backed system
        // time; the next COMPLETED sync retries the write.
        static_cast<void>(rtc_->synchronizeFromSystemUtc(*currentUtc));
    }
    completionHandled_ = true;
}

}  // namespace device_platform_esp_idf
