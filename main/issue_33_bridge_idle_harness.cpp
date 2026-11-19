#include "issue_33_bridge_idle_harness.hpp"

#if defined(APP_ISSUE_33_BRIDGE_IDLE_TEST)

#include <string_view>

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace fermentation::issue_33_bridge_idle {
namespace {

constexpr char kTag[] = "issue33_bridge_idle";
// Muss die UART-Hardware-FIFO (128 Byte) uebersteigen (uart_driver_install).
constexpr int kUartRxBufferBytes = 256;
// Feste Hoechstdauer je manueller Richtung; die Rueckkehr auf AUS erzwingt
// update(). Kein autonomer Hardware-Watchdog.
constexpr std::int64_t kDirectionMicros = 10'000'000;

}  // namespace

void Harness::start() noexcept {
    if (!uart_is_driver_installed(UART_NUM_0)) {
        const esp_err_t installed = uart_driver_install(
            UART_NUM_0, kUartRxBufferBytes, 0, 0, nullptr, 0);
        if (installed != ESP_OK) {
            ESP_LOGE(kTag, "ISSUE33_UART_RX_READY result=FAIL error=%s",
                     esp_err_to_name(installed));
            return;
        }
    }
    ESP_LOGI(kTag, "ISSUE33_UART_RX_READY result=PASS");
    ESP_LOGI(kTag,
             "ISSUE33_READY bring_up_only=YES real_actuators_enabled=NO "
             "bridge_ready=%s commands=\"bridge forward|bridge reverse|bridge "
             "off\" direction_ms=10000 note=no_automatic_on_at_boot_no_load",
             bridgeReady_ ? "YES" : "NO");
}

void Harness::commandAllOff() noexcept {
    // Beide Richtungen explizit AUS; die Bruecke schaltet Enable zuerst aus.
    bridge_.setForward(false);
    bridge_.setReverse(false);
}

void Harness::beginDirection(Direction direction, const char* name) noexcept {
    if (!bridgeReady_) {
        ESP_LOGE(kTag,
                 "ISSUE33_BRIDGE direction=%s result=FAIL "
                 "reason=BRIDGE_NOT_READY",
                 name);
        return;
    }
    if (active_ != Direction::None) {
        ESP_LOGE(kTag, "ISSUE33_BRIDGE direction=%s result=FAIL reason=BUSY",
                 name);
        return;
    }
    // Nur aus dem All-off-Zustand: beide Richtungen zuerst AUS befehlen,
    // danach genau eine Richtung EIN; die Gegenrichtung bleibt AUS.
    commandAllOff();
    if (direction == Direction::Forward) {
        bridge_.setForward(true);
    } else {
        bridge_.setReverse(true);
    }
    active_ = direction;
    activeName_ = name;
    deadlineUs_ = esp_timer_get_time() + kDirectionMicros;
    ESP_LOGI(kTag, "ISSUE33_BRIDGE direction=%s state=ON_COMMANDED", name);
}

void Harness::finishDirection(const char* reason) noexcept {
    if (active_ == Direction::None) return;
    commandAllOff();
    ESP_LOGI(kTag, "ISSUE33_BRIDGE direction=%s state=OFF_COMMANDED reason=%s",
             activeName_, reason);
    active_ = Direction::None;
    activeName_ = nullptr;
}

void Harness::update() noexcept {
    if (active_ != Direction::None && esp_timer_get_time() >= deadlineUs_) {
        finishDirection("TIMEOUT");
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
    if (!lineOverflow_ && text == "bridge off") {
        if (active_ == Direction::None) {
            // Auch im Leerlauf ein sicherer AUS-Befehl; kein Schaltvorgang EIN.
            commandAllOff();
            ESP_LOGI(kTag, "ISSUE33_OFF result=PASS reason=ALREADY_OFF");
        } else {
            finishDirection("OFF");
        }
        return;
    }
    if (active_ != Direction::None) {
        // Jede andere Eingabe waehrend einer aktiven Richtung bricht ab.
        finishDirection("UNEXPECTED_INPUT");
        return;
    }
    if (!lineOverflow_ && text == "bridge forward") {
        beginDirection(Direction::Forward, "forward");
    } else if (!lineOverflow_ && text == "bridge reverse") {
        beginDirection(Direction::Reverse, "reverse");
    } else {
        ESP_LOGE(kTag, "ISSUE33_COMMAND result=FAIL reason=REJECTED");
    }
}

}  // namespace fermentation::issue_33_bridge_idle

#endif
