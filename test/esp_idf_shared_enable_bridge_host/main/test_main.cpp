#include <cstdio>
#include <cstdlib>
#include <set>
#include <utility>
#include <vector>

#include "esp_idf_shared_enable_bridge.hpp"

extern "C" {
#include "Mockgpio.h"
#include "unity.h"
}

using device_platform::OutputPolarity;
using device_platform_esp_idf::EspIdfSharedEnableBridge;

namespace {

constexpr int kEnablePin = 25;
constexpr int kForwardPin = 13;  // RPWM
constexpr int kReversePin = 14;  // LPWM

struct Call {
    enum class Kind { SetLevel, Config } kind;
    int pin;
    int level;  // -1 for Config
    gpio_mode_t mode;
    bool pullUp;
    bool pullDown;
};

std::vector<Call> calls;
// (pin, level) pairs for which gpio_set_level fails, and pins for which
// gpio_config fails. Failures are only applied while enabled in a test.
std::set<std::pair<int, int>> failingLevels;
std::set<int> failingConfigPins;

esp_err_t setLevelStub(gpio_num_t pin, uint32_t level, int) {
    calls.push_back({Call::Kind::SetLevel, static_cast<int>(pin),
                     static_cast<int>(level), GPIO_MODE_DISABLE, false, false});
    return failingLevels.count({static_cast<int>(pin), static_cast<int>(level)})
               ? ESP_FAIL
               : ESP_OK;
}

esp_err_t configStub(const gpio_config_t* config, int) {
    int pin = -1;
    for (int bit = 0; bit < 64; ++bit) {
        if ((config->pin_bit_mask >> bit) & 1ULL) {
            pin = bit;
        }
    }
    calls.push_back({Call::Kind::Config, pin, -1, config->mode,
                     config->pull_up_en == GPIO_PULLUP_ENABLE,
                     config->pull_down_en == GPIO_PULLDOWN_ENABLE});
    return failingConfigPins.count(pin) ? ESP_FAIL : ESP_OK;
}

void expectLevel(std::size_t index, int pin, int level) {
    TEST_ASSERT_TRUE(index < calls.size());
    TEST_ASSERT_TRUE(calls[index].kind == Call::Kind::SetLevel);
    TEST_ASSERT_EQUAL(pin, calls[index].pin);
    TEST_ASSERT_EQUAL(level, calls[index].level);
}

void expectConfig(std::size_t index, int pin) {
    TEST_ASSERT_TRUE(index < calls.size());
    TEST_ASSERT_TRUE(calls[index].kind == Call::Kind::Config);
    TEST_ASSERT_EQUAL(pin, calls[index].pin);
    TEST_ASSERT_EQUAL(GPIO_MODE_OUTPUT, calls[index].mode);
    TEST_ASSERT_FALSE(calls[index].pullUp);
    TEST_ASSERT_FALSE(calls[index].pullDown);
}

bool anyLevelHighFrom(std::size_t from) {
    for (std::size_t i = from; i < calls.size(); ++i) {
        if (calls[i].kind == Call::Kind::SetLevel && calls[i].level == 1) {
            return true;
        }
    }
    return false;
}

// Replays every GPIO level write and proves the bridge invariants at the pin
// level: RPWM and LPWM are never high together, and a leg only goes high while
// the shared enable pin is low.
void assertPinInvariants() {
    int enable = 0;
    int forward = 0;
    int reverse = 0;
    for (const auto& call : calls) {
        if (call.kind != Call::Kind::SetLevel) continue;
        if (call.pin == kEnablePin) enable = call.level;
        if (call.pin == kForwardPin) {
            if (call.level == 1) TEST_ASSERT_EQUAL(0, enable);
            forward = call.level;
        }
        if (call.pin == kReversePin) {
            if (call.level == 1) TEST_ASSERT_EQUAL(0, enable);
            reverse = call.level;
        }
        TEST_ASSERT_FALSE(forward == 1 && reverse == 1);
    }
}

}  // namespace

extern "C" void setUp() {
    calls.clear();
    failingLevels.clear();
    failingConfigPins.clear();
    Mockgpio_Init();
    gpio_set_level_Stub(setLevelStub);
    gpio_config_Stub(configStub);
}

extern "C" void tearDown() {
    Mockgpio_Verify();
    Mockgpio_Destroy();
}

#define MAKE_BRIDGE(name)                                                  \
    EspIdfSharedEnableBridge name(kEnablePin, OutputPolarity::ActiveHigh,  \
                                  kForwardPin, OutputPolarity::ActiveHigh, \
                                  kReversePin, OutputPolarity::ActiveHigh)

void test_constructor_and_calls_before_begin_do_not_touch_gpio() {
    MAKE_BRIDGE(bridge);
    bridge.setForward(true);
    bridge.setReverse(true);
    bridge.setForward(false);
    bridge.setReverse(false);
    TEST_ASSERT_EQUAL(0, static_cast<int>(calls.size()));
}

void test_begin_initialises_enable_then_rpwm_then_lpwm_inactive_then_all_off() {
    MAKE_BRIDGE(bridge);
    TEST_ASSERT_TRUE(bridge.begin());
    TEST_ASSERT_EQUAL(9, static_cast<int>(calls.size()));
    // Output initialisation: inactive level first, then output without
    // pulls, in the order enable (GPIO25), RPWM (GPIO13), LPWM (GPIO14).
    expectLevel(0, kEnablePin, 0);
    expectConfig(1, kEnablePin);
    expectLevel(2, kForwardPin, 0);
    expectConfig(3, kForwardPin);
    expectLevel(4, kReversePin, 0);
    expectConfig(5, kReversePin);
    // Only then the bridge verifies the all-off state.
    expectLevel(6, kEnablePin, 0);
    expectLevel(7, kForwardPin, 0);
    expectLevel(8, kReversePin, 0);
    TEST_ASSERT_FALSE(anyLevelHighFrom(0));
}

// A failure at any single output initialisation stage: all three stages are
// still attempted in order, the bridge is never started, every output gets
// exactly one best-effort OFF, and no later command reaches a GPIO.
void begin_stage_failure(int failingPin, bool presetFails) {
    if (presetFails) {
        failingLevels.insert({failingPin, 0});
    } else {
        failingConfigPins.insert(failingPin);
    }
    MAKE_BRIDGE(bridge);
    TEST_ASSERT_FALSE(bridge.begin());

    // Initialisation attempts in the fixed order, then three best-effort OFF.
    const int failedStageCalls = presetFails ? 1 : 2;
    const int initCalls = 2 + 2 + 2 - (2 - failedStageCalls);
    TEST_ASSERT_EQUAL(initCalls + 3, static_cast<int>(calls.size()));
    TEST_ASSERT_FALSE(anyLevelHighFrom(0));
    // The all-off verification of the bridge (which would write 25, 13, 14
    // right after the initialisation) must not have run: the tail is exactly
    // the three best-effort OFF writes of the adapter.
    expectLevel(calls.size() - 3, kEnablePin, 0);
    expectLevel(calls.size() - 2, kForwardPin, 0);
    expectLevel(calls.size() - 1, kReversePin, 0);

    const auto before = calls.size();
    bridge.setForward(true);
    bridge.setReverse(true);
    bridge.setForward(false);
    bridge.setReverse(false);
    TEST_ASSERT_EQUAL(static_cast<int>(before), static_cast<int>(calls.size()));
}

void test_enable_stage_config_failure_keeps_bridge_unstarted() {
    begin_stage_failure(kEnablePin, false);
}

void test_rpwm_stage_config_failure_keeps_bridge_unstarted() {
    begin_stage_failure(kForwardPin, false);
}

void test_lpwm_stage_config_failure_keeps_bridge_unstarted() {
    begin_stage_failure(kReversePin, false);
}

void test_enable_stage_preset_failure_keeps_bridge_unstarted() {
    begin_stage_failure(kEnablePin, true);
}

void test_rpwm_stage_preset_failure_keeps_bridge_unstarted() {
    begin_stage_failure(kForwardPin, true);
}

void test_lpwm_stage_preset_failure_keeps_bridge_unstarted() {
    begin_stage_failure(kReversePin, true);
}

void test_unconfirmed_output_polarity_is_inert_without_touching_that_pin() {
    EspIdfSharedEnableBridge bridge(kEnablePin, OutputPolarity::Unconfirmed,
                                    kForwardPin, OutputPolarity::ActiveHigh,
                                    kReversePin, OutputPolarity::ActiveHigh);
    TEST_ASSERT_FALSE(bridge.begin());
    for (const auto& call : calls) {
        TEST_ASSERT_TRUE(call.pin != kEnablePin);
    }
    TEST_ASSERT_FALSE(anyLevelHighFrom(0));
    const auto before = calls.size();
    bridge.setForward(true);
    TEST_ASSERT_EQUAL(static_cast<int>(before), static_cast<int>(calls.size()));
}

void test_forward_and_reverse_sequences_at_gpio_level() {
    MAKE_BRIDGE(bridge);
    TEST_ASSERT_TRUE(bridge.begin());
    calls.clear();

    bridge.setForward(true);  // RPWM first (enable still low), enable last
    TEST_ASSERT_EQUAL(2, static_cast<int>(calls.size()));
    expectLevel(0, kForwardPin, 1);
    expectLevel(1, kEnablePin, 1);
    bridge.setForward(false);  // enable first
    TEST_ASSERT_EQUAL(4, static_cast<int>(calls.size()));
    expectLevel(2, kEnablePin, 0);
    expectLevel(3, kForwardPin, 0);

    bridge.setReverse(true);
    TEST_ASSERT_EQUAL(6, static_cast<int>(calls.size()));
    expectLevel(4, kReversePin, 1);
    expectLevel(5, kEnablePin, 1);
    bridge.setReverse(false);
    TEST_ASSERT_EQUAL(8, static_cast<int>(calls.size()));
    expectLevel(6, kEnablePin, 0);
    expectLevel(7, kReversePin, 0);
    assertPinInvariants();
}

void test_contradictory_commands_shut_down_all_pins_and_latch() {
    MAKE_BRIDGE(bridge);
    TEST_ASSERT_TRUE(bridge.begin());
    bridge.setForward(true);
    calls.clear();

    bridge.setReverse(true);  // forward still active
    TEST_ASSERT_EQUAL(3, static_cast<int>(calls.size()));
    expectLevel(0, kEnablePin, 0);
    expectLevel(1, kForwardPin, 0);
    expectLevel(2, kReversePin, 0);
    TEST_ASSERT_FALSE(anyLevelHighFrom(0));

    bridge.setForward(true);
    bridge.setReverse(true);
    TEST_ASSERT_EQUAL(3, static_cast<int>(calls.size()));
}

void test_gpio_write_failure_while_enabling_shuts_down_and_latches() {
    MAKE_BRIDGE(bridge);
    TEST_ASSERT_TRUE(bridge.begin());
    calls.clear();
    failingLevels.insert({kEnablePin, 1});  // the final enable write fails

    bridge.setForward(true);
    // RPWM high, failed enable write, then the sink's own best-effort OFF and
    // the bridge's shutdown round.
    TEST_ASSERT_TRUE(calls.size() >= 5U);
    expectLevel(calls.size() - 3, kEnablePin, 0);
    expectLevel(calls.size() - 2, kForwardPin, 0);
    expectLevel(calls.size() - 1, kReversePin, 0);

    failingLevels.clear();
    const auto before = calls.size();
    bridge.setForward(true);
    bridge.setReverse(true);
    TEST_ASSERT_FALSE(anyLevelHighFrom(before));
}

void test_gpio_write_failure_while_disabling_latches_and_blocks_reverse() {
    MAKE_BRIDGE(bridge);
    TEST_ASSERT_TRUE(bridge.begin());
    bridge.setForward(true);
    calls.clear();
    failingLevels.insert({kForwardPin, 0});  // RPWM cannot be switched off

    bridge.setForward(false);
    failingLevels.clear();
    const auto before = calls.size();
    bridge.setReverse(true);  // never released after the failed break
    TEST_ASSERT_FALSE(anyLevelHighFrom(before));
}

extern "C" void app_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_constructor_and_calls_before_begin_do_not_touch_gpio);
    RUN_TEST(
        test_begin_initialises_enable_then_rpwm_then_lpwm_inactive_then_all_off);
    RUN_TEST(test_enable_stage_config_failure_keeps_bridge_unstarted);
    RUN_TEST(test_rpwm_stage_config_failure_keeps_bridge_unstarted);
    RUN_TEST(test_lpwm_stage_config_failure_keeps_bridge_unstarted);
    RUN_TEST(test_enable_stage_preset_failure_keeps_bridge_unstarted);
    RUN_TEST(test_rpwm_stage_preset_failure_keeps_bridge_unstarted);
    RUN_TEST(test_lpwm_stage_preset_failure_keeps_bridge_unstarted);
    RUN_TEST(
        test_unconfirmed_output_polarity_is_inert_without_touching_that_pin);
    RUN_TEST(test_forward_and_reverse_sequences_at_gpio_level);
    RUN_TEST(test_contradictory_commands_shut_down_all_pins_and_latch);
    RUN_TEST(test_gpio_write_failure_while_enabling_shuts_down_and_latches);
    RUN_TEST(
        test_gpio_write_failure_while_disabling_latches_and_blocks_reverse);
    std::exit(UNITY_END() == 0 ? 0 : 1);
}
