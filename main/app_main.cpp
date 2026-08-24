#include <cinttypes>
#include <memory>
#include <optional>
#include <utility>

#include "app_config.hpp"
#include "configuration_bootstrap_store.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "device_platform.hpp"
#include "esp_reset_cause_source.hpp"
#include "esp_time_zone_resolver.hpp"
#include "esp_timer_time_source.hpp"
#include "nvs_flash.h"
#include "nvs_state_store.hpp"
#include "fermentation_application.hpp"
#include "run_checkpoint_schedule.hpp"
#include "run_persistence_coordinator.hpp"

#if defined(APP_ISSUE_29_BRINGUP_PROBE)
#include "issue_29_bringup_probe.hpp"
#endif

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kTag[] = "app_main";
constexpr uint64_t kHeartbeatIntervalMs = 1000U;
constexpr uint64_t kSecondResourceLogAfterMs = 30000U;
// Mindestens ein Tick Schedulerkooperation je Schleifendurchlauf; siehe
// docs/tasks/issue-73-implementation-plan.md, Abschnitt 12. Bewusst nicht
// pdMS_TO_TICKS(1), da das bei CONFIG_FREERTOS_HZ=100 auf 0 runden koennte.
constexpr TickType_t kCooperativeYieldTicks = 1;

// The composition root owns the concrete partition lifecycle. The adapter
// only opens/closes its handle; it never initializes, erases, or deinitializes
// an ESP-IDF partition. The store accessor below is deliberately non-owning;
// this context remains alive for every consumer constructed from it.
class NvsOwningContext final {
   public:
    [[nodiscard]] static std::unique_ptr<NvsOwningContext> create() {
        auto config = device_platform_esp_idf::NvsStateStoreConfig::create(
            "state_store", "fermentation");
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

        return std::unique_ptr<NvsOwningContext>(
            new NvsOwningContext(std::move(*config), std::move(opened.store)));
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
                    bool applicationStarted) {
    const char* TAG = kTag;
    ESP_LOGI(TAG, "%s", app_config::kProjectName);
    ESP_LOGI(TAG, "profile: %s", app_config::profileName(policy.profile));
    ESP_LOGI(TAG, "hardware state: %s",
             app_config::hardwareStateName(policy.startupHardwareState));
    ESP_LOGI(TAG, "actuator policy: %s",
             app_config::actuatorPolicyName(policy.actuatorPolicy));
    ESP_LOGI(TAG, "real actuators: disabled");
    if (applicationStarted) {
        ESP_LOGI(TAG, "application: ready");
    } else {
        ESP_LOGE(TAG, "application: startup failed");
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

}  // namespace

extern "C" void app_main(void) {
    const auto stateStoreContext = NvsOwningContext::create();
    if (stateStoreContext == nullptr) {
        // No recovery/application path is started if the owning context
        // cannot initialize and open its persistent store.
        return;
    }

    device_platform::DevicePlatform platform;
    fermentation::FermentationApplication application;
    const device_platform_esp_idf::EspResetCauseSource resetCauseSource;

    const device_platform::PlatformStartupContext startupContext{
        app_config::hasSafeDefaults(app_config::kActiveProfilePolicy),
    };
    if (!platform.begin(startupContext)) {
        logBootSummary(app_config::kActiveProfilePolicy, false);
        return;
    }

    auto& store = stateStoreContext->store();
    device_platform_esp_idf::EspTimeZoneResolver timeZoneResolver;
    fermentation::ConfigurationMutationCoordinator mutationCoordinator;
    fermentation::ConfigurationBootstrapStore bootstrapStore(store);
    fermentation::ConfigurationGraphStore graphStore(store, timeZoneResolver);
    fermentation::ConfigurationService configurationService(
        mutationCoordinator, graphStore, timeZoneResolver);
    auto configurationRecoveryService =
        fermentation::ConfigurationRecoveryService::create(
            store, bootstrapStore, graphStore, configurationService,
            mutationCoordinator);
    if (configurationRecoveryService == nullptr) {
        ESP_LOGE(kTag, "configuration recovery composition failed");
        logBootSummary(app_config::kActiveProfilePolicy, false);
        return;
    }

    const auto configurationRecoveryResult =
        configurationRecoveryService->boot();
    std::optional<fermentation::RunPersistenceCoordinator>
        runPersistenceCoordinator;
    if (configurationRecoveryResult.status ==
            fermentation::ConfigurationRecoveryStatus::RuntimeReady ||
        configurationRecoveryResult.status ==
            fermentation::ConfigurationRecoveryStatus::
                FactoryInitializationCompleted ||
        configurationRecoveryResult.status ==
            fermentation::ConfigurationRecoveryStatus::FactoryResetCompleted) {
        const auto runtimeRead = configurationService.acquireRuntime();
        if (runtimeRead.status ==
            fermentation::RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
            runPersistenceCoordinator.emplace(
                store, runtimeRead.lease.get().storageEpoch(),
                fermentation::RunCheckpointSchedule{});
        }
    }

    std::optional<fermentation::RunPersistenceLoadResult>
        runPersistenceLoadResult;
    if (runPersistenceCoordinator.has_value()) {
        runPersistenceLoadResult.emplace(
            runPersistenceCoordinator->loadAndInitialize());
    }

    const bool applicationStarted = application.begin(
        platform, configurationService, configurationRecoveryResult,
        runPersistenceCoordinator.has_value() ? &*runPersistenceCoordinator
                                              : nullptr,
        runPersistenceLoadResult.has_value() ? &*runPersistenceLoadResult
                                             : nullptr,
        &resetCauseSource);

    logBootSummary(app_config::kActiveProfilePolicy, applicationStarted);

    if (!applicationStarted) {
        // Sicherer Fehlerpfad: keine Hardware, kein Busy-Loop, keine
        // automatische Reboot-Schleife, keine Laufzeit-Zeitquelle. Ein
        // return aus app_main() ist offiziell unterstuetzt (siehe Plan
        // Abschnitt 5/10): die Task wird sauber beendet, ihr Stack
        // freigegeben, das System laeuft mit den uebrigen Tasks normal
        // weiter.
        return;
    }

    logResources();

#if defined(APP_ISSUE_29_BRINGUP_PROBE)
    if (!fermentation::issue_29_bringup::run()) {
        ESP_LOGE(kTag,
                 "Issue 29 bring-up probe failed; stopping before the"
                 " heartbeat smoke");
        return;
    }
#endif

    // Erst hier, unmittelbar vor der Laufzeitschleife, konstruiert: die
    // geloggte Uptime bedeutet damit eindeutig "Laufzeit seit
    // Schleifenstart" und schliesst Startpruefung, Bootlogging und die
    // erste Ressourcenmessung nicht mit ein.
    const device_platform_esp_idf::EspTimerTimeSource timeSource;
    const uint64_t startMs = timeSource.monotonicMillis();
    uint64_t lastHeartbeatMs = startMs;
    bool secondResourceLogDone = false;

    for (;;) {
        platform.update();
        application.update();

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
