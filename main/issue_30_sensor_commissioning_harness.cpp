#include "issue_30_sensor_commissioning_harness.hpp"

#if defined(APP_ISSUE_30_SENSOR_COMMISSIONING)

#include <cinttypes>
#include <string_view>

#include "driver/uart.h"
#include "esp_log.h"
#include "sensor_commissioning.hpp"

namespace fermentation::issue_30_commissioning {
namespace {

constexpr char kTag[] = "issue30_commissioning";
// Muss die UART-Hardware-FIFO (128 Byte) uebersteigen (uart_driver_install).
constexpr int kUartRxBufferBytes = 256;

void logRom(const char* label, device_platform::OneWireRom rom) {
    ESP_LOGI(kTag, "ISSUE30_ROM %s=%016" PRIX64, label,
             static_cast<uint64_t>(rom));
}

void logBus(const char* name, const device_platform::Ds18b20BusReport& bus) {
    ESP_LOGI(kTag,
             "ISSUE30_BUS %s presence=%d enumeration_result=%d count=%u "
             "overflow=%d binding_conflict=%d",
             name, static_cast<int>(bus.presence),
             static_cast<int>(bus.enumeration.result),
             static_cast<unsigned>(bus.enumeration.count),
             bus.enumeration.overflow ? 1 : 0, bus.bindingConflict ? 1 : 0);
    for (uint8_t i = 0U; i < bus.enumeration.count; ++i) {
        logRom(name, bus.enumeration.roms[i]);
    }
}

}  // namespace

void Harness::start() noexcept {
    // uart_read_bytes() verlangt den installierten UART-Treiber; der
    // Standard-Konsolenpfad installiert ihn nicht (Muster Issue-90-Harness).
    if (!uart_is_driver_installed(UART_NUM_0)) {
        const esp_err_t installed = uart_driver_install(
            UART_NUM_0, kUartRxBufferBytes, 0, 0, nullptr, 0);
        if (installed != ESP_OK) {
            ESP_LOGE(kTag, "ISSUE30_UART_RX_READY result=FAIL error=%s",
                     esp_err_to_name(installed));
            return;
        }
    }
    ESP_LOGI(kTag, "ISSUE30_UART_RX_READY result=PASS");
    ESP_LOGI(kTag,
             "ISSUE30_READY bring_up_only=YES real_actuators_enabled=NO "
             "commands=\"ds18b20 report|bind air=<16hex> heatsink=<16hex>|"
             "offset air|heatsink|product=<16hex> <milli-C>|clear\" "
             "note=binding_takes_effect_after_restart");
}

void Harness::update() noexcept {
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

void Harness::report() noexcept {
    const auto snapshot = application_.sensorCommissioning();
    ESP_LOGI(kTag, "ISSUE30_SAMPLER running=%d", sampler_.running() ? 1 : 0);
    const auto engine = sampler_.report();
    ESP_LOGI(kTag, "ISSUE30_BINDING engine_binding_valid=%d record_present=%d",
             engine.bindingValid ? 1 : 0,
             (snapshot.has_value() && snapshot->record.has_value()) ? 1 : 0);
    logBus("expected_rom_bus", engine.expectedRomBus);
    logBus("single_device_bus", engine.singleDeviceBus);
    if (engine.singleDeviceLastSeen.has_value()) {
        logRom("single_device_last_seen", *engine.singleDeviceLastSeen);
    }
    if (snapshot.has_value() && snapshot->record.has_value()) {
        const auto& record = *snapshot->record;
        logRom("record_air", record.chamberAir->rom);
        logRom("record_heatsink", record.heatsink->rom);
        for (const auto& probe : record.productProbes) {
            logRom("record_product", probe.rom);
        }
    }
}

void Harness::processLine() noexcept {
    if (lineOverflow_) {
        ESP_LOGE(kTag,
                 "ISSUE30_COMMAND_RESULT command=LINE result=FAIL "
                 "reason=TOO_LONG");
        return;
    }
    const auto command = parseSensorCommissioningCommand(
        std::string_view(line_.data(), lineLength_));
    if (command.kind == SensorCommissioningCommandKind::Rejected) {
        ESP_LOGE(kTag,
                 "ISSUE30_COMMAND_RESULT command=PARSE result=FAIL "
                 "reason=REJECTED");
        return;
    }
    if (command.kind == SensorCommissioningCommandKind::Report) {
        report();
        return;
    }
    const auto snapshot = application_.sensorCommissioning();
    if (!snapshot.has_value()) {
        ESP_LOGE(kTag,
                 "ISSUE30_COMMAND_RESULT result=FAIL "
                 "reason=CONFIGURATION_UNAVAILABLE");
        return;
    }
    const auto applied =
        applySensorCommissioningCommand(snapshot->record, command);
    if (!applied.accepted) {
        ESP_LOGE(kTag, "ISSUE30_COMMAND_RESULT result=FAIL reason=INVALID");
        return;
    }
    const auto result = application_.applySensorCommissioning(
        applied.hasRecord
            ? std::optional<SensorCommissioningRecord>(applied.record)
            : std::nullopt,
        snapshot->revision);
    ESP_LOGI(kTag,
             "ISSUE30_COMMAND_RESULT result=%s preview=%d commit=%d "
             "restart_required=YES",
             result.commit == ConfigurationCommitStatus::Activated ||
                     result.commit == ConfigurationCommitStatus::NoChange
                 ? "PASS"
                 : "FAIL",
             static_cast<int>(result.preview), static_cast<int>(result.commit));
}

}  // namespace fermentation::issue_30_commissioning

#endif
