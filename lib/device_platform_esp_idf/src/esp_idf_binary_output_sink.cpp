#include "esp_idf_binary_output_sink.hpp"

#include "driver/gpio.h"

namespace device_platform_esp_idf {

namespace {

// Guards only the 64-bit pin mask shift. Whether a pad can drive an output is
// decided by the ESP-IDF driver, which rejects invalid pins with
// ESP_ERR_INVALID_ARG on every call below.
constexpr int kMaxMaskedGpioNumber = 63;

[[nodiscard]] bool gpioNumberInMaskRange(int gpioNumber) noexcept {
    return gpioNumber >= 0 && gpioNumber <= kMaxMaskedGpioNumber;
}

}  // namespace

EspIdfBinaryOutputSink::EspIdfBinaryOutputSink(
    int gpioNumber, device_platform::OutputPolarity polarity) noexcept
    : gpioNumber_(gpioNumber), polarity_(polarity) {}

int EspIdfBinaryOutputSink::inactiveLevel() const noexcept {
    return polarity_ == device_platform::OutputPolarity::ActiveHigh ? 0 : 1;
}

int EspIdfBinaryOutputSink::levelFor(bool enabled) const noexcept {
    return enabled ? 1 - inactiveLevel() : inactiveLevel();
}

BinaryOutputBeginResult EspIdfBinaryOutputSink::begin() noexcept {
    if (state_ != State::NotStarted) {
        return state_ == State::Ready ? BinaryOutputBeginResult::Ready
               : state_ == State::Unconfirmed
                   ? BinaryOutputBeginResult::PolarityUnconfirmed
                   : BinaryOutputBeginResult::Failed;
    }
    if (polarity_ == device_platform::OutputPolarity::Unconfirmed) {
        state_ = State::Unconfirmed;
        return BinaryOutputBeginResult::PolarityUnconfirmed;
    }
    if (polarity_ != device_platform::OutputPolarity::ActiveHigh &&
        polarity_ != device_platform::OutputPolarity::ActiveLow) {
        state_ = State::Faulted;
        return BinaryOutputBeginResult::Failed;
    }
    const auto pin = static_cast<gpio_num_t>(gpioNumber_);
    if (!gpioNumberInMaskRange(gpioNumber_)) {
        state_ = State::Faulted;
        return BinaryOutputBeginResult::Failed;
    }
    // Inactive level first, then switch the pad to output: the pad never
    // drives a level that was not chosen here.
    if (gpio_set_level(pin, static_cast<std::uint32_t>(inactiveLevel())) !=
        ESP_OK) {
        state_ = State::Faulted;
        return BinaryOutputBeginResult::Failed;
    }
    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << gpioNumber_;
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&config) != ESP_OK) {
        state_ = State::Faulted;
        return BinaryOutputBeginResult::Failed;
    }
    state_ = State::Ready;
    return BinaryOutputBeginResult::Ready;
}

void EspIdfBinaryOutputSink::driveInactiveBestEffort() noexcept {
    if ((polarity_ == device_platform::OutputPolarity::ActiveHigh ||
         polarity_ == device_platform::OutputPolarity::ActiveLow) &&
        gpioNumberInMaskRange(gpioNumber_)) {
        // One bounded software attempt towards the inactive level; the result
        // is not retried and is no guarantee of a physical shutdown.
        static_cast<void>(
            gpio_set_level(static_cast<gpio_num_t>(gpioNumber_),
                           static_cast<std::uint32_t>(inactiveLevel())));
    }
}

void EspIdfBinaryOutputSink::setEnabled(bool enabled) {
    if (state_ == State::Ready) {
        if (gpio_set_level(static_cast<gpio_num_t>(gpioNumber_),
                           static_cast<std::uint32_t>(levelFor(enabled))) !=
            ESP_OK) {
            // A failed write may leave the last effective level standing:
            // latch the fault and immediately try the inactive level once.
            state_ = State::Faulted;
            driveInactiveBestEffort();
        }
        return;
    }
    if (state_ == State::Faulted && !enabled) {
        // Best effort towards the inactive level only; never enables.
        driveInactiveBestEffort();
    }
}

}  // namespace device_platform_esp_idf
