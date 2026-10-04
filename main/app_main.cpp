#include <cinttypes>
#include <memory>
#include <new>
#include <string>
#include <utility>

#include "app_config.hpp"
#include "device_platform.hpp"
#include "ds3231_sn_rtc_adapter.hpp"
#include "esp_idf_i2c_subsystem.hpp"
#include "esp_idf_http_server_lifecycle.hpp"
#include "esp_idf_network_lifecycle.hpp"
#include "esp_idf_secure_random_source.hpp"
#include "esp_idf_sntp_time_coordinator.hpp"
#include "esp_timer_time_source.hpp"
#include "esp_reset_cause_source.hpp"
#include "esp_time_zone_resolver.hpp"
#include "nvs_flash.h"
#include "nvs_state_store.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_lvgl_renderer.hpp"
#include "fermentation_ui_press_dispatcher.hpp"
#include "fermentation_ui_text.hpp"
#include "generated/board_profile_r1.hpp"
#include "touch_calibration.hpp"

#ifdef APP_ISSUE_90_SLICE7_HARNESS
#include "issue_90_slice7_harness.hpp"
#endif

#ifdef APP_ISSUE_31_TOUCH_CALIBRATION_HARNESS
#include "issue_31_touch_calibration_harness.hpp"
#endif

#ifdef APP_ISSUE_31_TOUCH_CALIBRATION_PROVISIONER
#include "issue_31_touch_calibration_provision.hpp"
#endif

#ifdef APP_ISSUE_29_BRINGUP_PROBE
#include "issue_29_bringup_probe.hpp"
#endif

#include "esp_log.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef APP_SOURCE_GIT_SHA
#define APP_SOURCE_GIT_SHA "UNKNOWN"
#endif

namespace {

constexpr char kTag[] = "app_main";
// Tag fuer den Failed-Allocation-Hook; muss in DRAM liegen (ESP_DRAM_LOGE).
DRAM_ATTR const char kHeapAllocFailedTag[] = "heap_alloc_failed";
#ifdef APP_ISSUE_90_SLICE7_HARNESS
constexpr char kStateStorePartitionLabel[] = "state_store_test";
#else
constexpr char kStateStorePartitionLabel[] = "state_store";
#endif
constexpr uint64_t kHeartbeatIntervalMs = 1000U;
constexpr uint64_t kSecondResourceLogAfterMs = 30000U;
// Idle-Messpunkt: 120 s ohne Bedienung (seit Boot bzw. seit dem letzten
// Touch-Press); siehe docs/tasks/memory-platform-course-plan.md, Abschnitt 3.
constexpr uint64_t kIdleResourceLogAfterMs = 120000U;
// Mindestens ein Tick Schedulerkooperation je Schleifendurchlauf; siehe
// docs/tasks/issue-73-implementation-plan.md, Abschnitt 12. Bewusst nicht
// pdMS_TO_TICKS(1), da das bei CONFIG_FREERTOS_HZ=100 auf 0 runden koennte.
constexpr TickType_t kCooperativeYieldTicks = 1;
constexpr const char* kNtpServers[] = {"pool.ntp.org"};

// The R1 board/controller identity a persisted touch calibration record
// must match to be trusted (Stage-2 evidence confirmed
// DISPLAY_CONTROLLER_IDENTITY=FUNCTIONAL_VISUAL_PASS and
// TOUCH_CONTROLLER_IDENTITY=FUNCTIONAL_RAW_TOUCH_PASS for exactly this
// combination; see
// docs/tasks/issue-31-renderer-display-touch-calibration-plan.md). A record
// written for a different board or controller is never silently accepted.
constexpr char kBoardControllerId[] =
    "esp32_32e_quad_mosfet_r1+ili9341+xpt2046";

[[nodiscard]] const char* touchCalibrationLoadStatusName(
    device_platform::TouchCalibrationLoadStatus status) noexcept {
    using device_platform::TouchCalibrationLoadStatus;
    switch (status) {
        case TouchCalibrationLoadStatus::Available:
            return "Available";
        case TouchCalibrationLoadStatus::NotFound:
            return "NotFound";
        case TouchCalibrationLoadStatus::OtherEpoch:
            return "OtherEpoch";
        case TouchCalibrationLoadStatus::UnsupportedSchema:
            return "UnsupportedSchema";
        case TouchCalibrationLoadStatus::InvalidRecord:
            return "InvalidRecord";
        case TouchCalibrationLoadStatus::ReadError:
            return "ReadError";
        case TouchCalibrationLoadStatus::CapacityError:
            return "CapacityError";
    }
    return "Unknown";
}

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

[[nodiscard]] const char* networkModeName(
    device_platform::NetworkMode mode) noexcept {
    switch (mode) {
        case device_platform::NetworkMode::UNSELECTED:
            return "UNSELECTED";
        case device_platform::NetworkMode::AP_ONLY:
            return "AP_ONLY";
        case device_platform::NetworkMode::HOME_WIFI:
            return "HOME_WIFI";
    }
    return "UNKNOWN";
}

[[nodiscard]] const char* networkLifecycleStateName(
    device_platform::NetworkLifecycleState state) noexcept {
    switch (state) {
        case device_platform::NetworkLifecycleState::Stopped:
            return "Stopped";
        case device_platform::NetworkLifecycleState::SetupAccessPoint:
            return "SetupAccessPoint";
        case device_platform::NetworkLifecycleState::AccessPointOnly:
            return "AccessPointOnly";
        case device_platform::NetworkLifecycleState::ConnectingHome:
            return "ConnectingHome";
        case device_platform::NetworkLifecycleState::HomeConnected:
            return "HomeConnected";
        case device_platform::NetworkLifecycleState::CandidateTesting:
            return "CandidateTesting";
        case device_platform::NetworkLifecycleState::Failed:
            return "Failed";
    }
    return "Unknown";
}

// Diagnosehook fuer fehlgeschlagene Heap-Allokationen. Er kann aus jedem
// Kontext der Allokation aufgerufen werden und darf deshalb weder allokieren
// noch ESP_LOGx verwenden; nur ROM-Ausgabe mit DRAM-Formatstrings. Auch
// erwartete Fehlschlaege (nothrow-new, Capability-Fallbacks in ESP-IDF)
// loesen ihn aus. Reine Diagnose: der Hook aendert kein Verhalten.
void IRAM_ATTR logFailedHeapAllocation(size_t size, uint32_t caps,
                                       const char* functionName) {
    ESP_DRAM_LOGE(kHeapAllocFailedTag, "size=%u caps=0x%08x function=%s",
                  static_cast<unsigned>(size), static_cast<unsigned>(caps),
                  functionName != nullptr ? functionName : "?");
}

void logResources(const char* samplePoint,
                  device_platform::NetworkMode selectedMode,
                  device_platform::NetworkLifecycleState lifecycleState,
                  const fermentation::main_ui::ProductiveLvglRenderer*
                      displayRenderer = nullptr) {
    const uint32_t freeHeapBytes = esp_get_free_heap_size();
    const uint32_t minimumFreeHeapBytes = esp_get_minimum_free_heap_size();
    const size_t largestFreeBlockBytes =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const UBaseType_t stackHighWaterMarkBytes =
        uxTaskGetStackHighWaterMark(nullptr);
    // Getrennte, sich ueberlappende Capability-Sichten; nicht addieren.
    constexpr uint32_t kInternal8Bit = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    ESP_LOGI(
        kTag,
        "resources: point=%s network_mode=%s network_state=%s "
        "free_heap_bytes=%" PRIu32 " minimum_free_heap_bytes=%" PRIu32
        " largest_free_block_8bit_bytes=%zu stack_hwm_bytes=%u"
        " internal_8bit_free_bytes=%zu internal_8bit_minimum_free_bytes=%zu"
        " internal_8bit_largest_free_block_bytes=%zu"
        " dma_free_bytes=%zu dma_minimum_free_bytes=%zu"
        " dma_largest_free_block_bytes=%zu",
        samplePoint, networkModeName(selectedMode),
        networkLifecycleStateName(lifecycleState), freeHeapBytes,
        minimumFreeHeapBytes, largestFreeBlockBytes,
        static_cast<unsigned>(stackHighWaterMarkBytes),
        heap_caps_get_free_size(kInternal8Bit),
        heap_caps_get_minimum_free_size(kInternal8Bit),
        heap_caps_get_largest_free_block(kInternal8Bit),
        heap_caps_get_free_size(MALLOC_CAP_DMA),
        heap_caps_get_minimum_free_size(MALLOC_CAP_DMA),
        heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    if (displayRenderer == nullptr) {
        return;
    }
    // Nur bei aktivem eingebautem LVGL-Allocator und initialisiertem LVGL.
    const auto pool = displayRenderer->lvglPoolStats();
    if (pool.has_value()) {
        ESP_LOGI(kTag,
                 "resources_lvgl: point=%s pool_total_bytes=%" PRIu32
                 " pool_free_bytes=%" PRIu32 " pool_largest_free_bytes=%" PRIu32
                 " pool_max_used_bytes=%" PRIu32
                 " pool_used_pct=%u pool_frag_pct=%u",
                 samplePoint, pool->totalBytes, pool->freeBytes,
                 pool->largestFreeBytes, pool->maxUsedBytes,
                 static_cast<unsigned>(pool->usedPercent),
                 static_cast<unsigned>(pool->fragmentationPercent));
    } else {
        ESP_LOGI(kTag, "resources_lvgl: point=%s pool=unavailable",
                 samplePoint);
    }
}

struct NetworkResourceSamplingState {
    device_platform::NetworkMode lastObservedMode;
    device_platform::NetworkLifecycleState lastObservedState;
    bool hasObservedState{false};
};

void sampleStableNetworkResources(
    device_platform::NetworkMode selectedNetworkMode,
    device_platform::NetworkLifecycleState networkState,
    NetworkResourceSamplingState& samplingState,
    const fermentation::main_ui::ProductiveLvglRenderer* displayRenderer) {
    const bool stableApOnly =
        networkState == device_platform::NetworkLifecycleState::AccessPointOnly;
    const bool stableHomeWifi =
        selectedNetworkMode == device_platform::NetworkMode::HOME_WIFI &&
        (networkState ==
             device_platform::NetworkLifecycleState::HomeConnected ||
         networkState ==
             device_platform::NetworkLifecycleState::SetupAccessPoint);
    const bool networkStatusChanged =
        !samplingState.hasObservedState ||
        selectedNetworkMode != samplingState.lastObservedMode ||
        networkState != samplingState.lastObservedState;
    if (networkStatusChanged && (stableApOnly || stableHomeWifi)) {
        const char* samplePoint = nullptr;
        if (stableApOnly) {
            samplePoint = "stable_ap_only";
        } else if (networkState ==
                   device_platform::NetworkLifecycleState::HomeConnected) {
            samplePoint = "stable_home_wifi";
        } else {
            samplePoint = "stable_home_wifi_setup_access_point";
        }
        logResources(samplePoint, selectedNetworkMode, networkState,
                     displayRenderer);
    }
    samplingState.lastObservedMode = selectedNetworkMode;
    samplingState.lastObservedState = networkState;
    samplingState.hasObservedState = true;
}

// Maps the existing renderer-independent #164 network lifecycle state to the
// existing renderer-independent header status contract. No new network
// state is introduced; SetupAccessPoint/ConnectingHome/CandidateTesting are
// presented as Disconnected (a transition is in progress, not yet usable).
device_platform::DeviceUiNetworkStatus toDeviceUiNetworkStatus(
    device_platform::NetworkLifecycleState state) noexcept {
    switch (state) {
        case device_platform::NetworkLifecycleState::AccessPointOnly:
        case device_platform::NetworkLifecycleState::HomeConnected:
            return device_platform::DeviceUiNetworkStatus::Connected;
        case device_platform::NetworkLifecycleState::SetupAccessPoint:
        case device_platform::NetworkLifecycleState::ConnectingHome:
        case device_platform::NetworkLifecycleState::CandidateTesting:
            return device_platform::DeviceUiNetworkStatus::Disconnected;
        case device_platform::NetworkLifecycleState::Stopped:
        case device_platform::NetworkLifecycleState::Failed:
            return device_platform::DeviceUiNetworkStatus::Unavailable;
    }
    return device_platform::DeviceUiNetworkStatus::Unavailable;
}

void loadTouchCalibration(
    device_platform::IStateStore& stateStore,
    fermentation::main_ui::ProductiveLvglRenderer& displayRenderer) {
    // Boot-time load/classify only (Schnitt 9): calibration never changes at
    // runtime without an explicit future calibration workflow writing a new
    // record, so this is not re-read per loop tick. The fallback slot is
    // loaded and classified for diagnostics only; it never replaces an
    // invalid active record here (see the session handover for that open
    // policy question).
    const device_platform::TouchCalibrationStore touchCalibrationStore(
        stateStore);
    const auto activeCalibration = touchCalibrationStore.load(
        device_platform::TouchCalibrationSlot::Active, kBoardControllerId);
    const auto fallbackCalibration = touchCalibrationStore.load(
        device_platform::TouchCalibrationSlot::Fallback, kBoardControllerId);
    ESP_LOGI(kTag, "touch calibration: active_status=%s fallback_status=%s",
             touchCalibrationLoadStatusName(activeCalibration.status),
             touchCalibrationLoadStatusName(fallbackCalibration.status));

    if (activeCalibration.status ==
            device_platform::TouchCalibrationLoadStatus::Available &&
        activeCalibration.record.has_value()) {
        displayRenderer.setTouchCalibration(activeCalibration.record->model);
        return;
    }

    if (activeCalibration.status ==
        device_platform::TouchCalibrationLoadStatus::Available) {
        ESP_LOGE(kTag,
                 "touch calibration: active record missing; keeping touch "
                 "fail-closed");
    }
    // Every non-Available result, including an inconsistent Available result,
    // must leave the renderer without a calibration model. No fallback or
    // guessed interpretation is permitted.
    displayRenderer.setTouchCalibration(std::nullopt);
}

void initializeProductUi(
    fermentation::main_ui::ProductiveLvglRenderer* displayRenderer,
    device_platform::IStateStore& stateStore,
    fermentation::FermentationApplication& application,
    fermentation::FermentationTouchWorkspace& uiWorkspace,
    const std::vector<device_platform::TextPackManifest>& uiTextPacks,
    const fermentation::FermentationUiPresentationSource& uiPresentation,
    device_platform::DeviceUiNetworkStatus uiNetworkStatus,
    const device_platform::ClockViewInput& uiClock) {
    if (displayRenderer == nullptr || !displayRenderer->initialize()) {
        ESP_LOGW(kTag,
                 "productive LVGL display unavailable; UI remains fail-closed");
        return;
    }

    loadTouchCalibration(stateStore, *displayRenderer);
    if (!displayRenderer->render(application.uiSnapshot(), uiWorkspace,
                                 uiTextPacks, uiPresentation.displayLocale,
                                 std::nullopt, &uiPresentation.programCatalog,
                                 uiNetworkStatus, uiClock,
                                 application.networkAccessPointInfo())) {
        ESP_LOGW(kTag, "productive LVGL initial projection failed");
    }
}

// Returns true iff this iteration consumed a fresh touch press edge.
bool updateProductUi(
    fermentation::FermentationApplication& application,
    fermentation::main_ui::ProductiveLvglRenderer* displayRenderer,
    fermentation::FermentationTouchWorkspace& uiWorkspace,
    fermentation::main_ui::UiRenderGate& renderGate,
    const std::vector<device_platform::TextPackManifest>& uiTextPacks,
    const device_platform::LocaleId& initialDisplayLocale,
    const device_platform::TimeZoneId& initialTimeZoneId,
    device_platform::INetworkLifecycle& networkLifecycle,
    const device_platform::ITimeSource& timeSource) {
    if (displayRenderer == nullptr || !displayRenderer->initialized()) {
        return false;
    }

    // Allocation-free steady state (R1 RAM plan, S4): the recycled snapshot,
    // the one presentation copy (S3) and the pre-built render key come from
    // the shared UiRenderGate. HeaderNetwork does not consume ProgramCatalog:
    // the copy is evicted before a network-mode touch is committed and
    // refilled after leaving the page.
    const bool networkPageBeforeTouch =
        uiWorkspace.page() == fermentation::FermentationUiPage::HeaderNetwork;
    renderGate.beginStep(application, networkPageBeforeTouch);
    const auto& loopSnapshot = renderGate.snapshot();
    const auto loopNetworkStatus =
        toDeviceUiNetworkStatus(networkLifecycle.status().state);

    // The existing #26 target/interaction path (calibrated touch
    // -> targetAt()/Workspace::press() -> existing typed
    // FermentationApplication/FermentationUiCommandBridge entry points) is
    // owned entirely by this one app-specific adapter; main/app_main.cpp
    // never builds a RepresentativeScreen or calls targetAt()/routePress()
    // itself. No second event/command state machine is introduced.
    const auto touchPoll = displayRenderer->pollTouch();
    const bool sampleNetworkPagePress =
        touchPoll.freshPressEdge &&
        uiWorkspace.page() == fermentation::FermentationUiPage::HeaderNetwork;
    if (sampleNetworkPagePress) {
        logResources("network_page_press_before", application.networkMode(),
                     networkLifecycle.status().state, displayRenderer);
    }
    fermentation::main_ui::WorkspaceTouchTickResult touchTick;
    if (touchPoll.contactHeld) {
        // Locale, time zone and catalog for the touch path; the clock input
        // (which copies the time zone id) is only built while a contact is
        // held, so the idle loop does not allocate for it.
        const auto& loopPresentation = renderGate.presentation().get();
        const auto& touchDisplayLocale = networkPageBeforeTouch
                                             ? initialDisplayLocale
                                             : loopPresentation.displayLocale;
        const auto& touchTimeZoneId =
            networkPageBeforeTouch ? initialTimeZoneId
                                   : loopPresentation.canonicalTimeZoneId;
        const auto* touchProgramCatalog =
            networkPageBeforeTouch ? nullptr : &loopPresentation.programCatalog;
        const device_platform::ClockViewInput loopClock{
            timeSource.unixTimeSeconds(), touchTimeZoneId};
        const auto touchPoint = touchPoll.point.value_or(
            fermentation::main_ui::ProductiveLvglRenderer::TouchPoint{});
        touchTick = fermentation::main_ui::processWorkspaceTouch(
            application, uiWorkspace, loopSnapshot, uiTextPacks,
            touchDisplayLocale, touchProgramCatalog, loopNetworkStatus,
            loopClock, touchPoll.contactHeld, touchPoint.x, touchPoint.y,
            touchPoll.freshPressEdge, timeSource.monotonicMillis());
    }
    if (sampleNetworkPagePress) {
        logResources("network_page_press_after", application.networkMode(),
                     networkLifecycle.status().state, displayRenderer);
    }
    if (touchTick.dispatch.outcome !=
        fermentation::main_ui::WorkspacePressDispatchOutcome::NoTypedPayload) {
        ESP_LOGI(kTag, "touch press dispatch: outcome=%d",
                 static_cast<int>(touchTick.dispatch.outcome));
    }

    const auto trustedUtc = timeSource.unixTimeSeconds();
    const bool redrawRequired = renderGate.renderRequired(
        application, uiWorkspace, initialDisplayLocale, touchTick.pressedTarget,
        loopNetworkStatus, trustedUtc);
    if (redrawRequired) {
        // Only a real visible change reaches the screen model and the LVGL
        // update; ProductiveLvglRenderer::render() builds both.
        const bool networkPageAfterTouch =
            uiWorkspace.page() ==
            fermentation::FermentationUiPage::HeaderNetwork;
        const auto& renderPresentation = renderGate.presentation().get();
        const auto& renderDisplayLocale =
            networkPageAfterTouch ? initialDisplayLocale
                                  : renderPresentation.displayLocale;
        const auto* renderProgramCatalog =
            networkPageAfterTouch ? nullptr
                                  : &renderPresentation.programCatalog;
        const device_platform::ClockViewInput renderClock{
            trustedUtc, networkPageAfterTouch
                            ? initialTimeZoneId
                            : renderPresentation.canonicalTimeZoneId};
        if (displayRenderer->render(
                loopSnapshot, uiWorkspace, uiTextPacks, renderDisplayLocale,
                touchTick.pressedTarget, renderProgramCatalog,
                loopNetworkStatus, renderClock,
                application.networkAccessPointInfo())) {
            renderGate.markRendered();
        }
    }
    return touchPoll.freshPressEdge;
}

}  // namespace

extern "C" void app_main(void) {
#ifdef APP_ISSUE_31_TOUCH_CALIBRATION_PROVISIONER
    fermentation::issue_31_touch_calibration_provision::run();
    return;
#endif

#ifdef APP_ISSUE_31_TOUCH_CALIBRATION_HARNESS
    fermentation::issue_31_touch_calibration::run();
    return;
#endif

    // Erste Anweisung: auch Allokationsfehler waehrend des Boots erfassen.
    static_cast<void>(
        heap_caps_register_failed_alloc_callback(logFailedHeapAllocation));

    const esp_err_t defaultNvsStatus = nvs_flash_init();
    if (defaultNvsStatus != ESP_OK) {
        ESP_LOGE(kTag,
                 "default NVS initialization for ESP-IDF PHY data failed: "
                 "err=0x%x; Wi-Fi startup is stopped",
                 static_cast<unsigned>(defaultNvsStatus));
        return;
    }
    ESP_LOGI(kTag, "default NVS initialized for ESP-IDF PHY system data");

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
    device_platform_esp_idf::EspIdfNetworkLifecycle networkLifecycle({});
    device_platform_esp_idf::EspIdfHttpServerLifecycle httpServerLifecycle;

    const device_platform::PlatformStartupContext startupContext{
        app_config::hasSafeDefaults(app_config::kActiveProfilePolicy),
    };
    // Kurzschlussverhalten von `platform.begin() && application.begin()` bleibt
    // erhalten: application.begin() laeuft nur nach erfolgreichem
    // platform.begin().
    bool applicationStarted = platform.begin(startupContext);
    logResources("after_platform_begin", application.networkMode(),
                 networkLifecycle.status().state);
    if (applicationStarted) {
        applicationStarted = application.begin(
            platform, stateStoreContext->store(), timeZoneResolver, timeSource,
            networkLifecycle, httpServerLifecycle, randomSource,
            &resetCauseSource);
        logResources("after_application_begin", application.networkMode(),
                     networkLifecycle.status().state);
    }

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

#ifdef APP_ISSUE_29_BRINGUP_PROBE
    if (!fermentation::issue_29_bringup::run()) {
        ESP_LOGE(kTag,
                 "Issue 29 bring-up probe failed; stopping before the"
                 " heartbeat smoke");
        return;
    }
#endif

    // Issue #31's selected product renderer is composed here, at the
    // application boundary. The pin numbers are the single deterministic
    // build-time derivation from config/board_profiles/
    // esp32_32e_quad_mosfet_r1.yaml (see main/generated/board_profile_r1.hpp
    // and scripts/generate_board_profile_header.py) - no second
    // hand-maintained pin list. Width/height are a panel property, not a
    // GPIO assignment, and stay a composition-root constant. No display,
    // touch, LVGL or command policy enters the application component.
    namespace r1_pins = board_profile::esp32_32e_quad_mosfet_r1;
    auto displayRenderer = fermentation::main_ui::makeProductiveUiRenderer(
        {r1_pins::kSpiSckPin, r1_pins::kSpiMosiPin, r1_pins::kSpiMisoPin,
         r1_pins::kDisplayChipSelectPin, r1_pins::kTouchChipSelectPin,
         r1_pins::kDisplayDataCommandPin, r1_pins::kBacklightPin,
         r1_pins::kTouchInterruptPin, 320U, 240U, r1_pins::kR1DisplayRotation,
         r1_pins::kBacklightActiveHigh});
    fermentation::FermentationTouchWorkspace uiWorkspace;
    fermentation::main_ui::UiRenderGate uiRenderGate;
    const auto uiTextPacks = fermentation::makeFermentationUiTextPacks();
    device_platform::LocaleId uiDisplayLocale{"en"};
    device_platform::TimeZoneId uiTimeZoneId;
    // The single renderer-independent source for locale, program catalog and
    // canonical prepared time zone is needed here only for initial UI setup.
    {
        // One-time initial setup: an unavailable source falls back to the
        // safe defaults (English, empty catalog), as before.
        auto uiPresentation = application.uiPresentationSource().value_or(
            fermentation::FermentationUiPresentationSource{});
        const auto uiNetworkStatus =
            toDeviceUiNetworkStatus(networkLifecycle.status().state);
        const device_platform::ClockViewInput uiClock{
            timeSource.unixTimeSeconds(), uiPresentation.canonicalTimeZoneId};
        initializeProductUi(displayRenderer.get(), stateStoreContext->store(),
                            application, uiWorkspace, uiTextPacks,
                            uiPresentation, uiNetworkStatus, uiClock);
        uiDisplayLocale = std::move(uiPresentation.displayLocale);
        uiTimeZoneId = std::move(uiPresentation.canonicalTimeZoneId);
    }

#ifdef APP_ISSUE_90_SLICE7_HARNESS
    fermentation::issue_90_slice7::Harness issue90Harness(application,
                                                          timeSource);
    issue90Harness.start();
#endif

    logResources("after_ui_init", application.networkMode(),
                 networkLifecycle.status().state, displayRenderer.get());

    // Die Zeitquelle wird vor dem Application-Boot injiziert, damit die
    // Recovery bereits beim Laden des Current-Records dieselbe monotone und
    // absolute Quelle wie die spaetere Laufzeitschleife verwendet.
    const uint64_t startMs = timeSource.monotonicMillis();
    uint64_t lastHeartbeatMs = startMs;
    bool secondResourceLogDone = false;
    uint64_t lastActivityMs = startMs;
    bool idleResourceLogDone = false;
    NetworkResourceSamplingState networkResourceSamplingState{
        application.networkMode(), networkLifecycle.status().state};

    for (;;) {
        platform.update();
        sntp.poll();
        application.update();
        const bool touchPressObserved =
            updateProductUi(application, displayRenderer.get(), uiWorkspace,
                            uiRenderGate, uiTextPacks, uiDisplayLocale,
                            uiTimeZoneId, networkLifecycle, timeSource);

        const auto networkStatus = networkLifecycle.status();
        const auto selectedNetworkMode = application.networkMode();
        sampleStableNetworkResources(selectedNetworkMode, networkStatus.state,
                                     networkResourceSamplingState,
                                     displayRenderer.get());
#ifdef APP_ISSUE_90_SLICE7_HARNESS
        issue90Harness.update();
#endif

        const uint64_t nowMs = timeSource.monotonicMillis();
        if (touchPressObserved) {
            lastActivityMs = nowMs;
            idleResourceLogDone = false;
        }
        if (nowMs - lastHeartbeatMs >= kHeartbeatIntervalMs) {
            lastHeartbeatMs = nowMs;
            logHeartbeat(nowMs);
        }

        if (!secondResourceLogDone &&
            nowMs - startMs >= kSecondResourceLogAfterMs) {
            secondResourceLogDone = true;
            const auto currentNetworkStatus = networkLifecycle.status();
            logResources("periodic_30s", application.networkMode(),
                         currentNetworkStatus.state, displayRenderer.get());
        }

        if (!idleResourceLogDone &&
            nowMs - lastActivityMs >= kIdleResourceLogAfterMs) {
            idleResourceLogDone = true;
            logResources("idle_120s", application.networkMode(),
                         networkLifecycle.status().state,
                         displayRenderer.get());
        }

        vTaskDelay(kCooperativeYieldTicks);
    }
}
