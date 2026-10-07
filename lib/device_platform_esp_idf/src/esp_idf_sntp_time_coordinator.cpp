#include "esp_idf_sntp_time_coordinator.hpp"

#include <algorithm>

#include "esp_netif_sntp.h"
#include "esp_sntp.h"

namespace device_platform_esp_idf {

namespace {

struct SntpActionContext {
    const EspTimerTimeSource* timeSource{nullptr};
    Ds3231SnRtcAdapter* rtc{nullptr};
};

void promoteSntpSystemTrust(void* context) {
    auto* actionContext = static_cast<SntpActionContext*>(context);
    if (actionContext != nullptr && actionContext->timeSource != nullptr)
        static_cast<void>(actionContext->timeSource->markAbsoluteTimeTrusted());
}

std::optional<std::int64_t> readSntpSystemUtc(void* context) {
    auto* actionContext = static_cast<SntpActionContext*>(context);
    if (actionContext == nullptr || actionContext->timeSource == nullptr)
        return std::nullopt;
    return actionContext->timeSource->unixTimeSeconds();
}

bool synchronizeSntpRtc(void* context, const std::int64_t utc) {
    auto* actionContext = static_cast<SntpActionContext*>(context);
    if (actionContext == nullptr || actionContext->rtc == nullptr ||
        !actionContext->rtc->present() || !actionContext->rtc->initialized()) {
        return false;
    }
    return actionContext->rtc->synchronizeFromSystemUtc(utc);
}

sntp_sync_mode_t toEspSyncMode(const internal::SntpSyncMode mode) {
    return mode == internal::SntpSyncMode::Smooth ? SNTP_SYNC_MODE_SMOOTH
                                                  : SNTP_SYNC_MODE_IMMED;
}

internal::SntpSyncMode fromEspSyncMode(const sntp_sync_mode_t mode) {
    return mode == SNTP_SYNC_MODE_SMOOTH ? internal::SntpSyncMode::Smooth
                                         : internal::SntpSyncMode::Immediate;
}

}  // namespace

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
    // The mode is set explicitly from the trust latch in applySyncMode().
    config.smooth_sync = false;
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
        arbitration_.reset();
        applySyncMode();
    }
    return status;
}

void EspIdfSntpTimeCoordinator::applySyncMode() const noexcept {
    sntp_set_sync_mode(toEspSyncMode(
        internal::selectSntpSyncMode(timeSource_.absoluteTimeTrusted())));
}

esp_err_t EspIdfSntpTimeCoordinator::start() noexcept {
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    arbitration_.reset();
    applySyncMode();
    return esp_netif_sntp_start();
}

void EspIdfSntpTimeCoordinator::poll() noexcept {
    if (!initialized_) return;
    const auto status = sntp_get_sync_status();
    internal::SntpSyncObservation observation =
        internal::SntpSyncObservation::Other;
    if (status == SNTP_SYNC_STATUS_IN_PROGRESS) {
        // In smooth mode, the response can be accepted while adjtime is still
        // converging.  RTC synchronization is intentionally deferred until
        // the later COMPLETED observation.
        observation = internal::SntpSyncObservation::InProgress;
    } else if (status == SNTP_SYNC_STATUS_RESET) {
        observation = internal::SntpSyncObservation::Reset;
    } else if (status == SNTP_SYNC_STATUS_COMPLETED) {
        observation = internal::SntpSyncObservation::Completed;
    }
    const auto action =
        arbitration_.observe(observation, fromEspSyncMode(sntp_get_sync_mode()),
                             timeSource_.absoluteTimeTrusted());
    // ESP-IDF has already completed the system-time update.  The action
    // explicitly separates trust promotion from RTC synchronization: a
    // failed RTC write must not revoke the current NTP-backed system time.
    SntpActionContext actionContext{&timeSource_, rtc_};
    internal::consumeSntpArbitrationAction(
        action, {&actionContext, &promoteSntpSystemTrust, &readSntpSystemUtc,
                 &synchronizeSntpRtc});
    // After the first promote the clock is set; follow-up syncs smooth.
    applySyncMode();
}

}  // namespace device_platform_esp_idf
