#include <cinttypes>
#include <cstdio>
#include <memory>
#include <new>
#include <string>
#include <utility>

#include "app_config.hpp"
#include "device_platform.hpp"
#include "ds3231_sn_rtc_adapter.hpp"
#include "esp_idf_i2c_subsystem.hpp"
#include "esp_idf_http_server_lifecycle.hpp"
#include "esp_idf_authentication_kdf.hpp"
#include "esp_idf_network_lifecycle.hpp"
#include "esp_idf_secure_random_source.hpp"
#include "esp_idf_sntp_time_coordinator.hpp"
#include "esp_timer_time_source.hpp"
#include "esp_reset_cause_source.hpp"
#include "esp_time_zone_resolver.hpp"
#include "nvs_flash.h"
#include "nvs_state_store.hpp"
#include "fermentation_application.hpp"

#ifdef APP_ISSUE_90_SLICE7_HARNESS
#include "issue_90_slice7_harness.hpp"
#endif

#ifdef APP_ISSUE_29_BRINGUP_PROBE
#include "issue_29_bringup_probe.hpp"
#endif

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef APP_SOURCE_GIT_SHA
#define APP_SOURCE_GIT_SHA "UNKNOWN"
#endif

namespace {

constexpr char kTag[] = "app_main";
#ifdef APP_ISSUE_90_SLICE7_HARNESS
constexpr char kStateStorePartitionLabel[] = "state_store_test";
#else
constexpr char kStateStorePartitionLabel[] = "state_store";
#endif
constexpr uint64_t kHeartbeatIntervalMs = 1000U;
constexpr uint64_t kSecondResourceLogAfterMs = 30000U;
// Mindestens ein Tick Schedulerkooperation je Schleifendurchlauf; siehe
// docs/tasks/issue-73-implementation-plan.md, Abschnitt 12. Bewusst nicht
// pdMS_TO_TICKS(1), da das bei CONFIG_FREERTOS_HZ=100 auf 0 runden koennte.
constexpr TickType_t kCooperativeYieldTicks = 1;
constexpr const char* kNtpServers[] = {"pool.ntp.org"};

// No board profile with verified RTC bus pins exists yet.  The disabled
// profile is therefore intentional and is the supported NTP-only mode; a
// future hardware gate supplies these fields before setting present=true.
constexpr device_platform_esp_idf::Ds3231SnRtcConfig kRtcConfig{};

// The composition root owns the concrete partition lifecycle.  The adapter
// only opens/closes its handle; it never initializes, erases, or deinitializes
// an ESP-IDF partition.  The actor-free application receives a non-owning
// IStateStore reference from this context after the store has been opened.
class NvsOwningContext final {
   public:
    [[nodiscard]] static std::unique_ptr<NvsOwningContext> create() {
        auto config = device_platform_esp_idf::NvsStateStoreConfig::create(
            kStateStorePartitionLabel, "fermentation");
        if (!config.has_value()) {
            ESP_LOGE(kTag, "invalid state-store owning-context configuration");
            return nullptr;
        }

        const auto initStatus =
            nvs_flash_init_partition(config->partitionLabel().c_str());
        if (initStatus != ESP_OK) {
            ESP_LOGE(kTag, "state-store partition init failed: %s",
                     esp_err_to_name(initStatus));
            return nullptr;
        }

        auto opened = device_platform_esp_idf::NvsStateStore::open(*config);
        if (opened.status != ESP_OK || opened.store == nullptr) {
            ESP_LOGE(kTag, "state-store open failed: %s",
                     esp_err_to_name(opened.status));
            static_cast<void>(
                nvs_flash_deinit_partition(config->partitionLabel().c_str()));
            return nullptr;
        }

        auto context = std::unique_ptr<NvsOwningContext>(
            new (std::nothrow)
                NvsOwningContext(std::move(*config), std::move(opened.store)));
        if (context == nullptr) {
            ESP_LOGE(kTag, "state-store owning-context allocation failed");
            opened.store.reset();
            static_cast<void>(
                nvs_flash_deinit_partition(config->partitionLabel().c_str()));
            return nullptr;
        }
        return context;
    }

    ~NvsOwningContext() {
        store_.reset();
        const auto status =
            nvs_flash_deinit_partition(config_.partitionLabel().c_str());
        if (status != ESP_OK) {
            ESP_LOGE(kTag, "state-store partition deinit failed: %s",
                     esp_err_to_name(status));
        }
    }

    NvsOwningContext(const NvsOwningContext&) = delete;
    NvsOwningContext& operator=(const NvsOwningContext&) = delete;
    NvsOwningContext(NvsOwningContext&&) = delete;
    NvsOwningContext& operator=(NvsOwningContext&&) = delete;

    [[nodiscard]] device_platform::IStateStore& store() const noexcept {
        return *store_;
    }

   private:
    NvsOwningContext(
        device_platform_esp_idf::NvsStateStoreConfig config,
        std::unique_ptr<device_platform_esp_idf::NvsStateStore> store)
        : config_(std::move(config)), store_(std::move(store)) {}

    device_platform_esp_idf::NvsStateStoreConfig config_;
    std::unique_ptr<device_platform_esp_idf::NvsStateStore> store_;
};

void logBootSummary(const app_config::ProfilePolicy& policy,
                    bool applicationStarted, bool applicationReady) {
    const char* TAG = kTag;
    ESP_LOGI(TAG, "%s", app_config::kProjectName);
    ESP_LOGI(TAG, "profile: %s", app_config::profileName(policy.profile));
    ESP_LOGI(TAG, "source git sha: %s", APP_SOURCE_GIT_SHA);
    ESP_LOGI(TAG, "hardware state: %s",
             app_config::hardwareStateName(policy.startupHardwareState));
    ESP_LOGI(TAG, "actuator policy: %s",
             app_config::actuatorPolicyName(policy.actuatorPolicy));
    ESP_LOGI(TAG, "real actuators: disabled");
    if (!applicationStarted) {
        ESP_LOGE(TAG, "application: startup failed");
    } else if (applicationReady) {
        ESP_LOGI(TAG, "application: ready");
    } else {
        ESP_LOGW(TAG, "application: service required");
    }
}

void logHeartbeat(uint64_t uptimeMs) {
    ESP_LOGI(kTag, "heartbeat: safe test mode, uptime_ms=%" PRIu64, uptimeMs);
}

void logResources() {
    const uint32_t freeHeapBytes = esp_get_free_heap_size();
    const UBaseType_t stackHighWaterMarkBytes =
        uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(kTag, "resources: free_heap_bytes=%" PRIu32 " stack_hwm_bytes=%u",
             freeHeapBytes, static_cast<unsigned>(stackHighWaterMarkBytes));
}

device_platform_esp_idf::EspIdfNetworkLifecycleConfig makeNetworkConfig(
    device_platform::ISecureRandomSource& randomSource) {
    std::uint8_t mac[6]{};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        return {};
    }
    char suffix[13]{};
    const int written =
        std::snprintf(suffix, sizeof(suffix), "%02X%02X%02X%02X%02X%02X",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (written != 12) {
        return {};
    }
    std::uint8_t randomBytes[16]{};
    if (!randomSource.fill(randomBytes, sizeof(randomBytes))) {
        return {};
    }
    static constexpr char kHex[] = "0123456789abcdef";
    std::string password;
    password.reserve(sizeof(randomBytes) * 2U);
    for (const auto byte : randomBytes) {
        password.push_back(kHex[(byte >> 4U) & 0x0FU]);
        password.push_back(kHex[byte & 0x0FU]);
    }
    // The password is neither derived from the MAC nor exposed through logs
    // or URLs. It exists only in the volatile adapter configuration.
    return {std::string("Fermentation-") + suffix, std::move(password), {}};
}

}  // namespace

extern "C" void app_main(void) {
    const auto stateStoreContext = NvsOwningContext::create();
    if (stateStoreContext == nullptr) {
        // No recovery/application path is started if the owning context
        // cannot initialize and open its persistent store.
        return;
    }
#ifdef APP_ISSUE_90_SLICE7_HARNESS
    ESP_LOGI(kTag,
             "ISSUE90_NVS_PARTITION_INIT=PASS ISSUE90_NVS_STORE_OPEN=PASS");
#endif

    device_platform::DevicePlatform platform;
    const device_platform_esp_idf::EspTimeZoneResolver timeZoneResolver;
    const device_platform_esp_idf::EspTimerTimeSource timeSource;
    device_platform_esp_idf::EspIdfI2cSubsystem i2cSubsystem;
    device_platform_esp_idf::Ds3231SnRtcAdapter rtc(i2cSubsystem);
    if (kRtcConfig.present) {
        if (i2cSubsystem.begin() != ESP_OK ||
            rtc.initialize(kRtcConfig) != ESP_OK) {
            ESP_LOGW(kTag, "DS3231SN RTC unavailable; continuing NTP-only");
        }
    }
    if (const auto rtcUtc = rtc.readTrustedUtc();
        rtcUtc.has_value() &&
        device_platform_esp_idf::EspTimerTimeSource::setSystemTimeUtc(
            *rtcUtc)) {
        static_cast<void>(timeSource.markAbsoluteTimeTrusted());
        ESP_LOGI(kTag, "trusted UTC seeded from DS3231SN RTC");
    }
    device_platform_esp_idf::EspIdfSntpTimeCoordinator sntp(timeSource, &rtc);
    const device_platform_esp_idf::SntpServerConfiguration sntpConfiguration{
        kNtpServers, 1U, false};
    if (sntp.initialize(sntpConfiguration) == ESP_OK) {
        // Starting SNTP is non-blocking.  It depends on the connectivity
        // lifecycle owned by Issue #89 and may be restarted when an IP is
        // available.
        static_cast<void>(sntp.start());
    } else {
        ESP_LOGW(kTag, "SNTP initialization failed; continuing fail-closed");
    }
    fermentation::FermentationApplication application;
    const device_platform_esp_idf::EspResetCauseSource resetCauseSource;
    device_platform_esp_idf::EspIdfSecureRandomSource randomSource;
    device_platform_esp_idf::EspIdfPbkdf2HmacSha256 authenticationKdf;
    const auto networkConfig = makeNetworkConfig(randomSource);
    device_platform_esp_idf::EspIdfNetworkLifecycle networkLifecycle(
        networkConfig);
    device_platform_esp_idf::EspIdfHttpServerLifecycle httpServerLifecycle;

    const device_platform::PlatformStartupContext startupContext{
        app_config::hasSafeDefaults(app_config::kActiveProfilePolicy),
    };
    const bool applicationStarted =
        platform.begin(startupContext) &&
        application.begin(platform, stateStoreContext->store(),
                          timeZoneResolver, timeSource, networkLifecycle,
                          httpServerLifecycle, &resetCauseSource,
                          &randomSource, &authenticationKdf);

    logBootSummary(app_config::kActiveProfilePolicy, applicationStarted,
                   application.ready());

    if (!applicationStarted) {
        // Sicherer Fehlerpfad: keine Hardware, kein Busy-Loop, keine
        // automatische Reboot-Schleife, keine Laufzeit-Zeitquelle. Ein
        // return aus app_main() ist offiziell unterstuetzt (siehe Plan
        // Abschnitt 5/10): die Task wird sauber beendet, ihr Stack
        // freigegeben, das System laeuft mit den uebrigen Tasks normal
        // weiter.
        return;
    }

#ifdef APP_ISSUE_90_SLICE7_HARNESS
    fermentation::issue_90_slice7::Harness issue90Harness(application,
                                                          timeSource);
    issue90Harness.start();
#endif

    logResources();

#ifdef APP_ISSUE_29_BRINGUP_PROBE
    if (!fermentation::issue_29_bringup::run()) {
        ESP_LOGE(kTag,
                 "Issue 29 bring-up probe failed; stopping before the"
                 " heartbeat smoke");
        return;
    }
#endif

    // Die Zeitquelle wird vor dem Application-Boot injiziert, damit die
    // Recovery bereits beim Laden des Current-Records dieselbe monotone und
    // absolute Quelle wie die spaetere Laufzeitschleife verwendet.
    const uint64_t startMs = timeSource.monotonicMillis();
    uint64_t lastHeartbeatMs = startMs;
    bool secondResourceLogDone = false;

    for (;;) {
        platform.update();
        sntp.poll();
        application.update();
#ifdef APP_ISSUE_90_SLICE7_HARNESS
        issue90Harness.update();
#endif

        const uint64_t nowMs = timeSource.monotonicMillis();
        if (nowMs - lastHeartbeatMs >= kHeartbeatIntervalMs) {
            lastHeartbeatMs = nowMs;
            logHeartbeat(nowMs);
        }

        if (!secondResourceLogDone &&
            nowMs - startMs >= kSecondResourceLogAfterMs) {
            secondResourceLogDone = true;
            logResources();
        }

        vTaskDelay(kCooperativeYieldTicks);
    }
}
