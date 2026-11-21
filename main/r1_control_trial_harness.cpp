#include "r1_control_trial_harness.hpp"

#if defined(APP_R1_CONTROL_TRIAL)

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "driver/uart.h"
#include "esp_log.h"
#include "r1_control_trial_profile.hpp"
#include "r1_trial_commands.hpp"
#include "temperature_control.hpp"

namespace fermentation::r1_trial {
namespace {

constexpr char kTag[] = "r1_control_trial";
// Must exceed the UART hardware FIFO (128 bytes) (uart_driver_install).
constexpr int kUartRxBufferBytes = 256;
constexpr uint64_t kStatusPeriodMs = 5000U;

const char* statusName(device_platform::TemperatureSampleStatus status) {
    using S = device_platform::TemperatureSampleStatus;
    switch (status) {
        case S::Ok:
            return "Ok";
        case S::BusFault:
            return "BusFault";
        case S::CrcFault:
            return "CrcFault";
        case S::MissingSample:
            return "MissingSample";
        case S::KnownInvalidMeasurement:
            return "KnownInvalidMeasurement";
    }
    return "Unknown";
}

void logRaw(const char* role,
            const std::optional<device_platform::TemperatureReading>& reading,
            uint64_t nowMs) {
    if (!reading.has_value()) {
        ESP_LOGI(kTag, "R1_TRIAL_RAW role=%s status=Absent", role);
        return;
    }
    char celsius[24] = "-";
    if (reading->celsius().has_value()) {
        std::snprintf(celsius, sizeof(celsius), "%.3f", *reading->celsius());
    }
    const uint64_t stamp = reading->monotonicTimestampMs();
    ESP_LOGI(kTag,
             "R1_TRIAL_RAW role=%s status=%s celsius=%s timestamp_ms=%" PRIu64
             " age_ms=%" PRIu64,
             role, statusName(reading->status()), celsius, stamp,
             nowMs >= stamp ? nowMs - stamp : 0U);
}

void logResult(const char* command, bool pass, int detailA, int detailB) {
    ESP_LOGI(kTag, "R1_TRIAL_COMMAND_RESULT command=%s result=%s a=%d b=%d",
             command, pass ? "PASS" : "FAIL", detailA, detailB);
}

}  // namespace

void Harness::start() noexcept {
    ESP_LOGW(kTag,
             "R1_TRIAL TRIAL_NOT_COMMISSIONED bring_up_only=YES "
             "release_profile=NO control_chain_attached=YES "
             "initial_outputs=OFF real_gpio_bts7960_outputs_possible=YES "
             "(explicit run + permission only)");
    ESP_LOGW(kTag,
             "R1_TRIAL peltier_power_path_must_be_physically_disconnected_for_"
             "C0_C1=EXTERNAL_PRECONDITION (APP_REAL_ACTUATORS_ENABLED=0 is a "
             "build policy flag, NOT a physical inhibit)");
    ESP_LOGW(kTag,
             "R1_TRIAL compile-time trial values are PROPOSAL_NOT_APPROVED; "
             "planner and thermal limits are set only at the gates");
    const auto parameters = trialControlChainParameters();
    const auto configs = trialSensorQualityConfigs();
    ESP_LOGI(kTag,
             "R1_TRIAL_PROFILE pi_valid=%d quality_air=%d quality_cooling=%d "
             "quality_product=%d",
             classifyTemperatureControlParameters(parameters.pi) ==
                     TemperatureControlParametersValidation::Valid
                 ? 1
                 : 0,
             configs.air.has_value() ? 1 : 0,
             configs.cooling.has_value() ? 1 : 0,
             configs.product.has_value() ? 1 : 0);
    if (!uart_is_driver_installed(UART_NUM_0)) {
        const esp_err_t installed = uart_driver_install(
            UART_NUM_0, kUartRxBufferBytes, 0, 0, nullptr, 0);
        if (installed != ESP_OK) {
            ESP_LOGE(kTag, "R1_TRIAL_UART_RX_READY result=FAIL error=%s",
                     esp_err_to_name(installed));
            return;
        }
    }
    ESP_LOGI(kTag, "R1_TRIAL_UART_RX_READY result=PASS");
    ESP_LOGI(kTag,
             "R1_TRIAL_READY commands=\"trial status|planner show|planner "
             "set <10 values>|planner clear|thermal show|thermal set <12 "
             "values>|thermal clear\"");
}

void Harness::showPlanner() noexcept {
    const auto snapshot = application_.actuatorPlannerParameters();
    if (!snapshot.has_value() || !snapshot->parameters.has_value()) {
        ESP_LOGI(kTag, "R1_TRIAL_PLANNER state=absent");
        return;
    }
    const auto& p = *snapshot->parameters;
    ESP_LOGI(kTag,
             "R1_TRIAL_PLANNER state=present window_ms=%" PRIu64
             " min_on_ms=%" PRIu64 " min_off_ms=%" PRIu64 " dead_ms=%" PRIu64
             " cap_ms=%" PRIu64 " counter_quote=%.3f counter_ms=%" PRIu64
             " watchdog_ms=%" PRIu64 " outer_postrun_ms=%" PRIu64
             " inner_postrun_ms=%" PRIu64,
             p.switchingWindowMillis, p.minimumOnMillis, p.minimumOffMillis,
             p.polarityDeadTimeMillis, p.pulseAccumulatorCapMillis,
             p.counterDirectionConfirmationQuoteThreshold,
             p.counterDirectionConfirmationDurationMillis,
             p.requestWatchdogMillis, p.outerFanPostRunMillis,
             p.innerFanPostRunMillis);
}

void Harness::showThermal() noexcept {
    const auto evaluation = application_.thermalSafetyEvaluation();
    const auto l = application_.thermalSafetyLimits();
    ESP_LOGI(kTag,
             "R1_TRIAL_THERMAL status=%s reason=%s air=[%.1f %.1f %.1f %.1f] "
             "cooling=[%.1f %.1f %.1f %.1f] rise_c_per_min=%.2f "
             "trend_window_ms=%" PRIu64 " max_age_ms=%" PRIu64
             " invalid_latch_ms=%" PRIu64,
             thermalSafetyStatusName(evaluation.status),
             thermalSafetyReasonName(evaluation.reason),
             l.airEmergencyLowerCelsius, l.airInterventionLowerCelsius,
             l.airInterventionUpperCelsius, l.airEmergencyUpperCelsius,
             l.coolingEmergencyLowerCelsius, l.coolingInterventionLowerCelsius,
             l.coolingInterventionUpperCelsius, l.coolingEmergencyUpperCelsius,
             l.coolingMaxRiseCelsiusPerMinute, l.trendWindowMillis,
             l.maxSampleAgeMillis, l.invalidLatchMillis);
}

void Harness::logStatus() noexcept {
    const uint64_t nowMs = time_.monotonicMillis();
    const auto raw = producer_.readRawSamples();
    logRaw("air", raw.air, nowMs);
    logRaw("cooling", raw.cooling, nowMs);
    logRaw("product", raw.product, nowMs);
    showThermal();
    showPlanner();
    const auto& presentation = application_.presentationState();
    ESP_LOGI(
        kTag,
        "R1_TRIAL_STATUS control_chain=attached real_outputs_possible=1 "
        "lifecycle=%d fault_code=0x%04X run_active=%d",
        static_cast<int>(application_.lifecycleState()),
        static_cast<unsigned>(presentation.faultCode),
        application_.uiSnapshot().home.mode == FermentationHomeMode::ActiveRun
            ? 1
            : 0);
}

void Harness::update() noexcept {
    const uint64_t nowMs = time_.monotonicMillis();
    if (nowMs >= nextStatusMs_) {
        nextStatusMs_ = nowMs + kStatusPeriodMs;
        logStatus();
    }
    for (std::size_t attempts = 0U; attempts < 64U; ++attempts) {
        size_t buffered = 0U;
        if (uart_get_buffered_data_len(UART_NUM_0, &buffered) != ESP_OK ||
            buffered == 0U) {
            return;
        }
        uint8_t byte = 0U;
        if (uart_read_bytes(UART_NUM_0, &byte, 1U, 0U) != 1) return;
        if (byte == '\r') continue;
        if (byte == '\n') {
            processLine();
            lineLength_ = 0U;
            lineOverflow_ = false;
            continue;
        }
        if (lineLength_ + 1U >= line_.size()) {
            lineOverflow_ = true;
            continue;
        }
        line_[lineLength_++] = static_cast<char>(byte);
    }
}

void Harness::processLine() noexcept {
    if (lineOverflow_) {
        logResult("line", false, 0, 0);
        return;
    }
    const auto command =
        parseR1TrialCommand(std::string_view(line_.data(), lineLength_));
    switch (command.kind) {
        case R1TrialCommandKind::Rejected:
            logResult("parse", false, 0, 0);
            return;
        case R1TrialCommandKind::Status:
            logStatus();
            return;
        case R1TrialCommandKind::PlannerShow:
            showPlanner();
            return;
        case R1TrialCommandKind::ThermalShow:
            showThermal();
            return;
        case R1TrialCommandKind::ThermalClear:
            application_.clearThermalSafetyConfiguration();
            logResult("thermal_clear", true, 0, 0);
            return;
        case R1TrialCommandKind::ThermalSet: {
            const auto result =
                application_.configureThermalSafety(command.thermal);
            logResult("thermal_set",
                      result == ThermalSafetyConfigureResult::Accepted,
                      static_cast<int>(result), 0);
            return;
        }
        case R1TrialCommandKind::PlannerSet:
        case R1TrialCommandKind::PlannerClear: {
            const auto snapshot = application_.actuatorPlannerParameters();
            if (!snapshot.has_value()) {
                logResult("planner", false, -1, -1);
                return;
            }
            const bool clear = command.kind == R1TrialCommandKind::PlannerClear;
            const auto result = application_.applyActuatorPlannerParameters(
                clear
                    ? std::nullopt
                    : std::optional<ActuatorPlannerParameters>(command.planner),
                snapshot->revision);
            const bool pass =
                result.commit == ConfigurationCommitStatus::Activated ||
                result.commit == ConfigurationCommitStatus::NoChange;
            logResult(clear ? "planner_clear" : "planner_set", pass,
                      static_cast<int>(result.preview),
                      static_cast<int>(result.commit));
            showPlanner();
            return;
        }
    }
}

}  // namespace fermentation::r1_trial

#endif
