#include <unity.h>

#include <cmath>

#include "sensor_lowpass_filter.hpp"

namespace {

using device_platform::LowPassFilter;

}  // namespace

void test_first_value_initializes_filter_without_lag() {
    LowPassFilter filter(5.0);

    const auto value = filter.update(20.0, 1000U);

    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(20.0, value.value());
}

void test_sample_timestamp_delta_controls_response() {
    LowPassFilter filter(1.0);
    (void)filter.update(0.0, 0U);

    const auto value = filter.update(10.0, 1000U);
    const double expected = 10.0 * (1.0 - std::exp(-1.0));

    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, expected, value.value());
}

void test_different_tau_values_have_different_dynamics() {
    LowPassFilter fast(1.0);
    LowPassFilter slow(10.0);
    (void)fast.update(0.0, 0U);
    (void)slow.update(0.0, 0U);

    const auto fastValue = fast.update(10.0, 1000U).value();
    const auto slowValue = slow.update(10.0, 1000U).value();

    TEST_ASSERT_TRUE(fastValue > slowValue);
}

void test_large_sample_gap_converges_faster() {
    LowPassFilter filter(5.0);
    (void)filter.update(0.0, 0U);

    const double shortGap = filter.update(10.0, 1000U).value();
    filter.reset();
    (void)filter.update(0.0, 0U);
    const double longGap = filter.update(10.0, 10'000U).value();

    TEST_ASSERT_TRUE(longGap > shortGap);
    TEST_ASSERT_TRUE(longGap < 10.0);
}

void test_shift_moves_existing_state_without_resetting_timestamp() {
    LowPassFilter filter(5.0);
    (void)filter.update(20.0, 1000U);

    filter.shiftState(2.0);

    TEST_ASSERT_EQUAL_DOUBLE(22.0, filter.value().value());
    TEST_ASSERT_EQUAL_DOUBLE(22.0, filter.update(22.0, 2000U).value());
}

void test_reset_discards_value_and_timebase() {
    LowPassFilter filter(5.0);
    (void)filter.update(20.0, 1000U);
    filter.reset();

    TEST_ASSERT_FALSE(filter.value().has_value());
    TEST_ASSERT_EQUAL_DOUBLE(40.0, filter.update(40.0, 0U).value());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_first_value_initializes_filter_without_lag);
    RUN_TEST(test_sample_timestamp_delta_controls_response);
    RUN_TEST(test_different_tau_values_have_different_dynamics);
    RUN_TEST(test_large_sample_gap_converges_faster);
    RUN_TEST(test_shift_moves_existing_state_without_resetting_timestamp);
    RUN_TEST(test_reset_discards_value_and_timebase);
    return UNITY_END();
}
