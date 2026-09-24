#include "issue_31_touch_calibration_harness.hpp"

#if defined(APP_ISSUE_31_TOUCH_CALIBRATION_HARNESS)

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdint>
#include <limits>

#include "device_ui_hardware_ports.hpp"
#include "esp_idf_display_touch_adapter.hpp"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "generated/board_profile_r1.hpp"
#include "sdkconfig.h"

namespace fermentation::issue_31_touch_calibration {
namespace {

constexpr char kTag[] = "issue31_capture";
constexpr std::uint16_t kDisplayWidth = 320U;
constexpr std::uint16_t kDisplayHeight = 240U;
constexpr std::uint16_t kCrosshairHalfExtent = 16U;
constexpr std::uint16_t kCrosshairThickness = 2U;
constexpr std::uint16_t kCrosshairCenterExtent = 5U;
constexpr std::uint16_t kBackgroundColor = 0x0000U;
constexpr std::uint16_t kFitColor = 0x07E0U;
constexpr std::uint16_t kValidationColor = 0x001FU;
constexpr std::uint16_t kHoldColor = 0xF800U;
constexpr TickType_t kSamplePeriodTicks = pdMS_TO_TICKS(20U);
constexpr std::uint32_t kIdleBaselineSamples = 50U;
constexpr std::uint32_t kMinimumContactSamples = 15U;
constexpr std::uint8_t kStableReleaseSamples = 5U;

enum class ReferenceRole : std::uint8_t { Fit, Validation, Hold };

struct ReferencePoint {
    const char* id;
    ReferenceRole role;
    std::uint16_t targetX;
    std::uint16_t targetY;
};

// Four corners are the smallest practical overdetermined set for an affine
// fit: any three non-collinear points identify the six coefficients, while a
// fourth point exposes a bad contact or an unstable corner. The two
// validation points are never consumed by that fit. This is deliberately not
// a dogmatic five-point product calibration matrix.
constexpr std::array<ReferencePoint, 6U> kReferencePoints{{
    {"FIT_TOP_LEFT", ReferenceRole::Fit, 32U, 32U},
    {"FIT_TOP_RIGHT", ReferenceRole::Fit, 287U, 32U},
    {"FIT_BOTTOM_LEFT", ReferenceRole::Fit, 32U, 207U},
    {"FIT_BOTTOM_RIGHT", ReferenceRole::Fit, 287U, 207U},
    {"VALIDATION_CENTER", ReferenceRole::Validation, 160U, 120U},
    {"VALIDATION_TOP_MID", ReferenceRole::Validation, 160U, 48U},
}};

struct ContactStats {
    std::uint32_t sampleCount{0U};
    std::uint16_t minRawX{std::numeric_limits<std::uint16_t>::max()};
    std::uint16_t maxRawX{0U};
    std::uint16_t minRawY{std::numeric_limits<std::uint16_t>::max()};
    std::uint16_t maxRawY{0U};
    std::uint16_t minStrength{std::numeric_limits<std::uint16_t>::max()};
    std::uint16_t maxStrength{0U};
    std::uint64_t firstContactUs{0U};
    std::uint64_t lastContactUs{0U};
};

[[nodiscard]] const char* roleName(ReferenceRole role) noexcept {
    switch (role) {
        case ReferenceRole::Fit:
            return "FIT";
        case ReferenceRole::Validation:
            return "VALIDATION";
        case ReferenceRole::Hold:
            return "HOLD";
    }
    return "UNKNOWN";
}

[[nodiscard]] std::uint16_t roleColor(ReferenceRole role) noexcept {
    switch (role) {
        case ReferenceRole::Fit:
            return kFitColor;
        case ReferenceRole::Validation:
            return kValidationColor;
        case ReferenceRole::Hold:
            return kHoldColor;
    }
    return kValidationColor;
}

void resetStats(ContactStats& stats) noexcept { stats = ContactStats{}; }

void observeContact(ContactStats& stats,
                    const device_platform::RawTouchSample& sample) noexcept {
    if (stats.sampleCount == 0U) stats.firstContactUs = sample.monotonicTimeUs;
    ++stats.sampleCount;
    stats.lastContactUs = sample.monotonicTimeUs;
    stats.minRawX = std::min(stats.minRawX, sample.rawX);
    stats.maxRawX = std::max(stats.maxRawX, sample.rawX);
    stats.minRawY = std::min(stats.minRawY, sample.rawY);
    stats.maxRawY = std::max(stats.maxRawY, sample.rawY);
    stats.minStrength = std::min(stats.minStrength, sample.strength);
    stats.maxStrength = std::max(stats.maxStrength, sample.strength);
}

[[nodiscard]] bool drawReference(
    device_platform_esp_idf::EspIdfDisplayTouchAdapter& adapter,
    const ReferencePoint& point) noexcept {
    if (!adapter.fillRect({0U, 0U, kDisplayWidth, kDisplayHeight},
                          kBackgroundColor)) {
        return false;
    }

    const auto color = roleColor(point.role);
    const auto left =
        static_cast<std::uint16_t>(point.targetX - kCrosshairHalfExtent);
    const auto top =
        static_cast<std::uint16_t>(point.targetY - kCrosshairHalfExtent);
    if (!adapter.fillRect(
            {left,
             static_cast<std::uint16_t>(point.targetY -
                                        kCrosshairThickness / 2U),
             static_cast<std::uint16_t>(2U * kCrosshairHalfExtent + 1U),
             kCrosshairThickness},
            color) ||
        !adapter.fillRect(
            {static_cast<std::uint16_t>(point.targetX -
                                        kCrosshairThickness / 2U),
             top, kCrosshairThickness,
             static_cast<std::uint16_t>(2U * kCrosshairHalfExtent + 1U)},
            color) ||
        !adapter.fillRect(
            {static_cast<std::uint16_t>(point.targetX - kCrosshairCenterExtent),
             static_cast<std::uint16_t>(point.targetY - kCrosshairCenterExtent),
             static_cast<std::uint16_t>(2U * kCrosshairCenterExtent + 1U),
             static_cast<std::uint16_t>(2U * kCrosshairCenterExtent + 1U)},
            kHoldColor)) {
        return false;
    }
    return true;
}

void logReady(const ReferencePoint& point) noexcept {
    ESP_LOGI(kTag,
             "CALIBRATION_POINT_READY id=%s role=%s target_x=%u target_y=%u "
             "instruction=touch_center_hold_" PRIu32 "ms_then_release",
             point.id, roleName(point.role),
             static_cast<unsigned>(point.targetX),
             static_cast<unsigned>(point.targetY),
             static_cast<std::uint32_t>(kMinimumContactSamples * 20U));
}

void logContactSample(const ReferencePoint& point,
                      const device_platform::RawTouchSample& sample) noexcept {
    ESP_LOGI(
        kTag,
        "CALIBRATION_SAMPLE id=%s role=%s target_x=%u target_y=%u "
        "status=CONTACT raw_x=%u raw_y=%u strength=%u time_us=%" PRIu64
        " contact=1",
        point.id, roleName(point.role), static_cast<unsigned>(point.targetX),
        static_cast<unsigned>(point.targetY),
        static_cast<unsigned>(sample.rawX), static_cast<unsigned>(sample.rawY),
        static_cast<unsigned>(sample.strength), sample.monotonicTimeUs);
}

void logRelease(const ReferencePoint& point,
                const device_platform::RawTouchSample& sample) noexcept {
    ESP_LOGI(
        kTag,
        "CALIBRATION_SAMPLE id=%s role=%s target_x=%u target_y=%u "
        "status=RELEASE raw_x=%u raw_y=%u strength=%u time_us=%" PRIu64
        " contact=0",
        point.id, roleName(point.role), static_cast<unsigned>(point.targetX),
        static_cast<unsigned>(point.targetY),
        static_cast<unsigned>(sample.rawX), static_cast<unsigned>(sample.rawY),
        static_cast<unsigned>(sample.strength), sample.monotonicTimeUs);
}

void logControllerError(
    const ReferencePoint& point,
    const device_platform::RawTouchSample& sample) noexcept {
    ESP_LOGW(kTag,
             "CALIBRATION_SAMPLE id=%s role=%s target_x=%u target_y=%u "
             "status=CONTROLLER_ERROR time_us=%" PRIu64 " contact=0",
             point.id, roleName(point.role),
             static_cast<unsigned>(point.targetX),
             static_cast<unsigned>(point.targetY), sample.monotonicTimeUs);
}

void logSummary(const ReferencePoint& point,
                const ContactStats& stats) noexcept {
    ESP_LOGI(kTag,
             "CALIBRATION_POINT_SUMMARY id=%s role=%s target_x=%u target_y=%u "
             "samples=%" PRIu32
             " raw_x_min=%u raw_x_max=%u raw_y_min=%u "
             "raw_y_max=%u strength_min=%u strength_max=%u first_us=%" PRIu64
             " last_us=%" PRIu64,
             point.id, roleName(point.role),
             static_cast<unsigned>(point.targetX),
             static_cast<unsigned>(point.targetY), stats.sampleCount,
             static_cast<unsigned>(stats.minRawX),
             static_cast<unsigned>(stats.maxRawX),
             static_cast<unsigned>(stats.minRawY),
             static_cast<unsigned>(stats.maxRawY),
             static_cast<unsigned>(stats.minStrength),
             static_cast<unsigned>(stats.maxStrength), stats.firstContactUs,
             stats.lastContactUs);
}

[[nodiscard]] bool runIdleBaseline(
    device_platform_esp_idf::EspIdfDisplayTouchAdapter& adapter,
    int interruptGpio) noexcept {
    std::uint32_t contactSamples = 0U;
    std::uint32_t controllerErrors = 0U;
    const int initialPenirqLevel =
        gpio_get_level(static_cast<gpio_num_t>(interruptGpio));

    for (std::uint32_t sampleIndex = 0U;
         sampleIndex < kIdleBaselineSamples; ++sampleIndex) {
        const auto sample = adapter.sampleTouch();
        if (sample.status == device_platform::RawTouchSampleStatus::Contact &&
            sample.contact) {
            ++contactSamples;
        } else if (sample.status ==
                   device_platform::RawTouchSampleStatus::ControllerError) {
            ++controllerErrors;
        }
        vTaskDelay(kSamplePeriodTicks);
    }

    const int finalPenirqLevel =
        gpio_get_level(static_cast<gpio_num_t>(interruptGpio));
    if (contactSamples != 0U || controllerErrors != 0U) {
        ESP_LOGE(
            kTag,
            "CALIBRATION_IDLE_BASELINE=FAILED samples=%" PRIu32
            " contacts=%" PRIu32 " controller_errors=%" PRIu32
            " gate=PENIRQ_ACTIVE_LOW gpio=%d penirq_level_initial=%d"
            " penirq_level_final=%d",
            kIdleBaselineSamples, contactSamples, controllerErrors,
            interruptGpio, initialPenirqLevel, finalPenirqLevel);
        ESP_LOGE(kTag,
                 "CALIBRATION_CAPTURE_HARNESS=FAILED "
                 "reason=IDLE_CONTACT_GATE");
        return false;
    }

    ESP_LOGI(
        kTag,
        "CALIBRATION_IDLE_BASELINE=PASS samples=%" PRIu32
        " contacts=%" PRIu32 " controller_errors=%" PRIu32
        " gate=PENIRQ_ACTIVE_LOW gpio=%d penirq_level_initial=%d"
        " penirq_level_final=%d",
        kIdleBaselineSamples, contactSamples, controllerErrors, interruptGpio,
        initialPenirqLevel, finalPenirqLevel);
    return true;
}

void captureContact(device_platform_esp_idf::EspIdfDisplayTouchAdapter& adapter,
                    const ReferencePoint& point) noexcept {
    ContactStats stats;
    bool contactSession = false;
    std::uint8_t stableReleaseSamples = 0U;
    bool releaseLogged = false;

    logReady(point);
    for (;;) {
        const auto sample = adapter.sampleTouch();
        if (sample.status == device_platform::RawTouchSampleStatus::Contact &&
            sample.contact) {
            if (!contactSession) {
                contactSession = true;
                stableReleaseSamples = 0U;
                releaseLogged = false;
                resetStats(stats);
                ESP_LOGI(kTag, "CALIBRATION_CONTACT_BEGIN id=%s role=%s",
                         point.id, roleName(point.role));
            }
            stableReleaseSamples = 0U;
            observeContact(stats, sample);
            logContactSample(point, sample);
        } else if (sample.status ==
                   device_platform::RawTouchSampleStatus::NoContact) {
            if (contactSession) {
                if (!releaseLogged) {
                    logRelease(point, sample);
                    releaseLogged = true;
                }
                if (stableReleaseSamples < kStableReleaseSamples) {
                    ++stableReleaseSamples;
                }
                if (stableReleaseSamples >= kStableReleaseSamples) {
                    if (stats.sampleCount >= kMinimumContactSamples) {
                        logSummary(point, stats);
                        return;
                    }
                    ESP_LOGW(
                        kTag,
                        "CALIBRATION_CONTACT_TOO_SHORT id=%s samples=%" PRIu32
                        " required=%" PRIu32 " restart=YES",
                        point.id, stats.sampleCount, kMinimumContactSamples);
                    contactSession = false;
                    stableReleaseSamples = 0U;
                    releaseLogged = false;
                    resetStats(stats);
                }
            }
        } else {
            logControllerError(point, sample);
            // A controller error never counts as contact or release evidence.
            // Requiring a fresh contact after the error keeps the capture
            // fail-closed instead of stitching two unknown sessions together.
            contactSession = false;
            stableReleaseSamples = 0U;
            releaseLogged = false;
            resetStats(stats);
        }
        vTaskDelay(kSamplePeriodTicks);
    }
}

}  // namespace

void run() noexcept {
    namespace r1_pins = board_profile::esp32_32e_quad_mosfet_r1;
    device_platform_esp_idf::EspIdfDisplayTouchAdapter adapter({
        r1_pins::kSpiSckPin,
        r1_pins::kSpiMosiPin,
        r1_pins::kSpiMisoPin,
        r1_pins::kDisplayChipSelectPin,
        r1_pins::kTouchChipSelectPin,
        r1_pins::kDisplayDataCommandPin,
        r1_pins::kBacklightPin,
        r1_pins::kTouchInterruptPin,
        kDisplayWidth,
        kDisplayHeight,
        r1_pins::kBacklightActiveHigh,
    });

    if (!adapter.initialize() ||
        !adapter.setRotation(device_platform::DisplayRotation::Rotate0) ||
        !adapter.setBacklight(true)) {
        ESP_LOGE(kTag,
                 "CALIBRATION_CAPTURE_HARNESS=FAILED "
                 "reason=DISPLAY_TOUCH_INITIALIZATION");
        return;
    }

    ESP_LOGI(kTag,
             "CALIBRATION_CAPTURE_HARNESS=READY "
             "ACTUATOR_RELEASE=NO CALIBRATION_RECORD_WRITTEN=NO "
             "RAW_SAMPLE_SOURCE=ESP_IDF_XPT2046_NATIVE");
    ESP_LOGI(kTag,
             "CALIBRATION_CAPTURE_PREFILTER=MEASUREMENT_SAFE "
             "XPT2046_Z_THRESHOLD=%d",
             CONFIG_XPT2046_Z_THRESHOLD);
    ESP_LOGI(kTag,
             "CALIBRATION_CAPTURE_PENIRQ_GATE=ENABLED "
             "gpio=%d active_level=LOW",
             r1_pins::kTouchInterruptPin);
    if (!runIdleBaseline(adapter, r1_pins::kTouchInterruptPin)) return;
    ESP_LOGI(kTag,
             "CALIBRATION_CAPTURE_LAYOUT=FIT_4_NONCOLLINEAR_PLUS_"
             "VALIDATION_2_INDEPENDENT");

    for (const auto& point : kReferencePoints) {
        if (!drawReference(adapter, point)) {
            ESP_LOGE(kTag,
                     "CALIBRATION_CAPTURE_HARNESS=FAILED "
                     "reason=REFERENCE_DRAW id=%s",
                     point.id);
            return;
        }
        captureContact(adapter, point);
        vTaskDelay(pdMS_TO_TICKS(250U));
    }

    const ReferencePoint holdPoint{"HOLD_PROBE", ReferenceRole::Hold, 160U,
                                   120U};
    if (!drawReference(adapter, holdPoint)) {
        ESP_LOGE(kTag,
                 "CALIBRATION_CAPTURE_HARNESS=FAILED "
                 "reason=HOLD_REFERENCE_DRAW");
        return;
    }
    ESP_LOGI(kTag,
             "CALIBRATION_HOLD_RELEASE_SEQUENCE=READY "
             "purpose=contact_stability_strength_debounce");
    captureContact(adapter, holdPoint);

    ESP_LOGI(kTag,
             "CALIBRATION_CAPTURE_HARNESS=COMPLETE "
             "CALIBRATION_RECORD_WRITTEN=NO FIT_VALUES_COMPUTED=NO "
             "OWNER_NEXT=offline_fit_then_independent_validation");
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000U));
}

}  // namespace fermentation::issue_31_touch_calibration

#endif
