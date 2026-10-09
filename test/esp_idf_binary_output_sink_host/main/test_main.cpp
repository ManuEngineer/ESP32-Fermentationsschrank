#include <cstdio>
#include <cstdlib>
#include <vector>

#include "esp_idf_binary_output_sink.hpp"

extern "C" {
#include "Mockgpio.h"
#include "unity.h"
}

using device_platform::OutputPolarity;
using device_platform_esp_idf::BinaryOutputBeginResult;
using device_platform_esp_idf::EspIdfBinaryOutputSink;

namespace {

struct Call {
    enum class Kind { SetLevel, Config } kind;
    int pin;
    int level;
    gpio_mode_t mode;
    gpio_pull_mode_t pull;
    bool pullUp;
    bool pullDown;
};

std::vector<Call> calls;
esp_err_t setLevelResult = ESP_OK;
// Results consumed in order by gpio_set_level before falling back to
// setLevelResult.
std::vector<esp_err_t> setLevelResultQueue;
esp_err_t configResult = ESP_OK;

constexpr int kInputOnlyPin = 34;  // the real driver rejects it for output

esp_err_t setLevelStub(gpio_num_t pin, uint32_t level, int) {
    calls.push_back({Call::Kind::SetLevel, static_cast<int>(pin),
                     static_cast<int>(level), GPIO_MODE_DISABLE, GPIO_FLOATING,
                     false, false});
    if (static_cast<int>(pin) == kInputOnlyPin) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!setLevelResultQueue.empty()) {
        const esp_err_t next = setLevelResultQueue.front();
        setLevelResultQueue.erase(setLevelResultQueue.begin());
        return next;
    }
    return setLevelResult;
}

esp_err_t configStub(const gpio_config_t* config, int) {
    int pin = -1;
    for (int bit = 0; bit < 64; ++bit) {
        if ((config->pin_bit_mask >> bit) & 1ULL) {
            pin = bit;
        }
    }
    calls.push_back({Call::Kind::Config, pin, -1, config->mode, GPIO_FLOATING,
                     config->pull_up_en == GPIO_PULLUP_ENABLE,
                     config->pull_down_en == GPIO_PULLDOWN_ENABLE});
    return configResult;
}

}  // namespace

extern "C" void setUp() {
    calls.clear();
    setLevelResult = ESP_OK;
    setLevelResultQueue.clear();
    configResult = ESP_OK;
    Mockgpio_Init();
    gpio_set_level_Stub(setLevelStub);
    gpio_config_Stub(configStub);
}

extern "C" void tearDown() {
    Mockgpio_Verify();
    Mockgpio_Destroy();
}

void test_unconfirmed_never_touches_gpio() {
    EspIdfBinaryOutputSink sink(16, OutputPolarity::Unconfirmed);
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::PolarityUnconfirmed,
                      sink.begin());
    sink.setEnabled(true);
    sink.setEnabled(false);
    sink.setEnabled(true);
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::PolarityUnconfirmed,
                      sink.begin());
    TEST_ASSERT_EQUAL(0, static_cast<int>(calls.size()));
}

void test_constructor_and_enable_before_begin_do_not_touch_gpio() {
    EspIdfBinaryOutputSink sink(16, OutputPolarity::ActiveHigh);
    sink.setEnabled(true);
    sink.setEnabled(false);
    TEST_ASSERT_EQUAL(0, static_cast<int>(calls.size()));
}

void test_begin_presets_inactive_level_before_output_without_pulls(
    OutputPolarity polarity, int expectedInactive) {
    EspIdfBinaryOutputSink sink(17, polarity);
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::Ready, sink.begin());
    TEST_ASSERT_EQUAL(2, static_cast<int>(calls.size()));
    TEST_ASSERT_TRUE(calls[0].kind == Call::Kind::SetLevel);
    TEST_ASSERT_EQUAL(17, calls[0].pin);
    TEST_ASSERT_EQUAL(expectedInactive, calls[0].level);
    TEST_ASSERT_TRUE(calls[1].kind == Call::Kind::Config);
    TEST_ASSERT_EQUAL(17, calls[1].pin);
    TEST_ASSERT_EQUAL(GPIO_MODE_OUTPUT, calls[1].mode);
    TEST_ASSERT_FALSE(calls[1].pullUp);
    TEST_ASSERT_FALSE(calls[1].pullDown);
}

void test_begin_active_high() {
    test_begin_presets_inactive_level_before_output_without_pulls(
        OutputPolarity::ActiveHigh, 0);
}

void test_begin_active_low() {
    test_begin_presets_inactive_level_before_output_without_pulls(
        OutputPolarity::ActiveLow, 1);
}

void test_level_mapping(OutputPolarity polarity, int onLevel, int offLevel) {
    EspIdfBinaryOutputSink sink(26, polarity);
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::Ready, sink.begin());
    calls.clear();
    sink.setEnabled(true);
    sink.setEnabled(false);
    TEST_ASSERT_EQUAL(2, static_cast<int>(calls.size()));
    TEST_ASSERT_EQUAL(26, calls[0].pin);
    TEST_ASSERT_EQUAL(onLevel, calls[0].level);
    TEST_ASSERT_EQUAL(offLevel, calls[1].level);
}

void test_mapping_active_high() {
    test_level_mapping(OutputPolarity::ActiveHigh, 1, 0);
}

void test_mapping_active_low() {
    test_level_mapping(OutputPolarity::ActiveLow, 0, 1);
}

void test_config_failure_is_fail_closed() {
    configResult = ESP_FAIL;
    EspIdfBinaryOutputSink sink(16, OutputPolarity::ActiveHigh);
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::Failed, sink.begin());
    calls.clear();
    sink.setEnabled(true);
    TEST_ASSERT_EQUAL(0, static_cast<int>(calls.size()));
    sink.setEnabled(false);  // best effort towards inactive only
    TEST_ASSERT_EQUAL(1, static_cast<int>(calls.size()));
    TEST_ASSERT_EQUAL(0, calls[0].level);
}

void test_preset_failure_is_fail_closed_without_config() {
    setLevelResult = ESP_FAIL;
    EspIdfBinaryOutputSink sink(16, OutputPolarity::ActiveLow);
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::Failed, sink.begin());
    for (const auto& call : calls) {
        TEST_ASSERT_TRUE(call.kind == Call::Kind::SetLevel);
    }
    calls.clear();
    sink.setEnabled(true);
    TEST_ASSERT_EQUAL(0, static_cast<int>(calls.size()));
}

void test_enable_write_failure_latches_fault_and_tries_inactive_once() {
    EspIdfBinaryOutputSink sink(16, OutputPolarity::ActiveHigh);
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::Ready, sink.begin());
    calls.clear();
    setLevelResultQueue = {ESP_FAIL};  // the EIN write fails
    sink.setEnabled(true);
    // failed EIN attempt + exactly one immediate inactive attempt
    TEST_ASSERT_EQUAL(2, static_cast<int>(calls.size()));
    TEST_ASSERT_EQUAL(1, calls[0].level);
    TEST_ASSERT_EQUAL(0, calls[1].level);
    calls.clear();
    sink.setEnabled(true);  // refused, no GPIO access
    TEST_ASSERT_EQUAL(0, static_cast<int>(calls.size()));
    sink.setEnabled(false);  // keeps the best-effort AUS contract
    TEST_ASSERT_EQUAL(1, static_cast<int>(calls.size()));
    TEST_ASSERT_EQUAL(0, calls[0].level);
}

void test_disable_write_failure_after_enable_tries_inactive_once_and_blocks() {
    EspIdfBinaryOutputSink sink(17, OutputPolarity::ActiveLow);
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::Ready, sink.begin());
    calls.clear();
    sink.setEnabled(true);  // succeeds, drives the active level (LOW)
    TEST_ASSERT_EQUAL(1, static_cast<int>(calls.size()));
    TEST_ASSERT_EQUAL(0, calls[0].level);
    calls.clear();
    setLevelResultQueue = {ESP_FAIL};  // the AUS write fails
    sink.setEnabled(false);
    // failed AUS attempt + exactly one additional inactive attempt (HIGH);
    // no retry loop. This is a software attempt, not a guaranteed shutdown.
    TEST_ASSERT_EQUAL(2, static_cast<int>(calls.size()));
    TEST_ASSERT_EQUAL(1, calls[0].level);
    TEST_ASSERT_EQUAL(1, calls[1].level);
    calls.clear();
    sink.setEnabled(true);  // never re-enabled after the fault
    TEST_ASSERT_EQUAL(0, static_cast<int>(calls.size()));
}

void test_invalid_pin_and_invalid_polarity_fail_closed() {
    EspIdfBinaryOutputSink badPin(kInputOnlyPin, OutputPolarity::ActiveHigh);
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::Failed, badPin.begin());
    EspIdfBinaryOutputSink negative(-1, OutputPolarity::ActiveLow);
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::Failed, negative.begin());
    EspIdfBinaryOutputSink garbage(16, static_cast<OutputPolarity>(99));
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::Failed, garbage.begin());
    garbage.setEnabled(true);
    garbage.setEnabled(false);
    badPin.setEnabled(true);
    badPin.setEnabled(false);
    negative.setEnabled(false);
    // Only the driver-rejected preset and the best-effort inactive write
    // reach the driver for the input-only pin; no config, no active level.
    for (const auto& call : calls) {
        TEST_ASSERT_TRUE(call.kind == Call::Kind::SetLevel);
        TEST_ASSERT_EQUAL(kInputOnlyPin, call.pin);
        TEST_ASSERT_EQUAL(0, call.level);
    }
}

void test_channels_are_independent() {
    EspIdfBinaryOutputSink inner(16, OutputPolarity::ActiveHigh);
    EspIdfBinaryOutputSink outer(17, OutputPolarity::ActiveLow);
    EspIdfBinaryOutputSink buzzer(26, OutputPolarity::Unconfirmed);
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::Ready, inner.begin());
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::Ready, outer.begin());
    TEST_ASSERT_EQUAL(BinaryOutputBeginResult::PolarityUnconfirmed,
                      buzzer.begin());
    calls.clear();
    inner.setEnabled(true);
    buzzer.setEnabled(true);
    TEST_ASSERT_EQUAL(1, static_cast<int>(calls.size()));
    TEST_ASSERT_EQUAL(16, calls[0].pin);
    TEST_ASSERT_EQUAL(1, calls[0].level);
    calls.clear();
    outer.setEnabled(true);
    TEST_ASSERT_EQUAL(1, static_cast<int>(calls.size()));
    TEST_ASSERT_EQUAL(17, calls[0].pin);
    TEST_ASSERT_EQUAL(0, calls[0].level);
}

extern "C" void app_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unconfirmed_never_touches_gpio);
    RUN_TEST(test_constructor_and_enable_before_begin_do_not_touch_gpio);
    RUN_TEST(test_begin_active_high);
    RUN_TEST(test_begin_active_low);
    RUN_TEST(test_mapping_active_high);
    RUN_TEST(test_mapping_active_low);
    RUN_TEST(test_config_failure_is_fail_closed);
    RUN_TEST(test_preset_failure_is_fail_closed_without_config);
    RUN_TEST(test_enable_write_failure_latches_fault_and_tries_inactive_once);
    RUN_TEST(
        test_disable_write_failure_after_enable_tries_inactive_once_and_blocks);
    RUN_TEST(test_invalid_pin_and_invalid_polarity_fail_closed);
    RUN_TEST(test_channels_are_independent);
    std::exit(UNITY_END() == 0 ? 0 : 1);
}
