#include "issue_32_output_pulse_harness.hpp"

#if defined(APP_ISSUE_32_OUTPUT_PULSE)

#include <string_view>

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace fermentation::issue_32_output_pulse {
namespace {

constexpr char kTag[] = "issue32_output_pulse";
// Muss die UART-Hardware-FIFO (128 Byte) uebersteigen (uart_driver_install).
constexpr int kUartRxBufferBytes = 256;
// Hoechstens ein kurzer EIN-Zyklus; die Rueckkehr auf LOW erzwingt update().
constexpr std::int64_t kPulseMicros = 3'000'000;

}  // namespace

void Harness::start() noexcept {
    if (!uart_is_driver_installed(UART_NUM_0)) {
        const esp_err_t installed = uart_driver_install(
            UART_NUM_0, kUartRxBufferBytes, 0, 0, nullptr, 0);
        if (installed != ESP_OK) {
            ESP_LOGE(kTag, "ISSUE32_UART_RX_READY result=FAIL error=%s",
                     esp_err_to_name(installed));
            return;
        }
    }
    ESP_LOGI(kTag, "ISSUE32_UART_RX_READY result=PASS");
    ESP_LOGI(kTag,
             "ISSUE32_READY bring_up_only=YES real_actuators_enabled=NO "
             "commands=\"pulse out1|pulse out4|off\" pulse_ms=3000 "
             "note=no_automatic_on_at_boot");
}

void Harness::beginPulse(device_platform::IBinaryOutputSink& sink,
                         const char* name) noexcept {
    if (active_ != nullptr) {
        ESP_LOGE(kTag, "ISSUE32_PULSE channel=%s result=FAIL reason=BUSY",
                 name);
        return;
    }
    if (!sink.setEnabled(true)) {
        // Fail-closed: sofort LOW, kein Zyklus.
        static_cast<void>(sink.setEnabled(false));
        ESP_LOGE(kTag,
                 "ISSUE32_PULSE channel=%s result=FAIL reason=ENABLE_REFUSED",
                 name);
        return;
    }
    active_ = &sink;
    activeName_ = name;
    deadlineUs_ = esp_timer_get_time() + kPulseMicros;
    ESP_LOGI(kTag, "ISSUE32_PULSE channel=%s state=ON", name);
}

void Harness::finishPulse(const char* result, const char* reason) noexcept {
    if (active_ == nullptr) return;
    const bool off = active_->setEnabled(false);
    ESP_LOGI(kTag, "ISSUE32_PULSE channel=%s state=OFF result=%s reason=%s",
             activeName_, off ? result : "FAIL", off ? reason : "DISABLE_FAILED");
    active_ = nullptr;
    activeName_ = nullptr;
}

void Harness::update() noexcept {
    if (active_ != nullptr && esp_timer_get_time() >= deadlineUs_) {
        finishPulse("PASS", "TIMEOUT_OFF");
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
    const std::string_view text(line_.data(), lineLength_);
    if (!lineOverflow_ && text == "off") {
        if (active_ == nullptr) {
            ESP_LOGI(kTag, "ISSUE32_OFF result=PASS reason=ALREADY_OFF");
        } else {
            finishPulse("PASS", "ABORTED");
        }
        return;
    }
    if (active_ != nullptr) {
        // Jede andere Eingabe waehrend eines Zyklus bricht ab: sofort LOW.
        finishPulse("ABORT", "UNEXPECTED_INPUT");
        return;
    }
    if (!lineOverflow_ && text == "pulse out1") {
        beginPulse(out1_, "out1");
    } else if (!lineOverflow_ && text == "pulse out4") {
        beginPulse(out4_, "out4");
    } else {
        ESP_LOGE(kTag, "ISSUE32_COMMAND result=FAIL reason=REJECTED");
    }
}

}  // namespace fermentation::issue_32_output_pulse

#endif
