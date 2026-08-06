#include <unity.h>

#include <limits>

#include "sensor_median_filter.hpp"

namespace {

using device_platform::MedianFilter;

}  // namespace

void test_empty_filter_has_no_median() {
    const MedianFilter filter(3U);

    TEST_ASSERT_FALSE(filter.median().has_value());
    TEST_ASSERT_EQUAL_size_t(0U, filter.size());
}

void test_partial_window_uses_existing_measured_values() {
    MedianFilter filter(3U);

    TEST_ASSERT_TRUE(filter.add(9.0));
    TEST_ASSERT_TRUE(filter.add(1.0));

    TEST_ASSERT_EQUAL_DOUBLE(9.0, filter.median().value());
    TEST_ASSERT_EQUAL_size_t(2U, filter.size());
}

void test_single_spike_is_removed_after_window_is_full() {
    MedianFilter filter(3U);

    TEST_ASSERT_TRUE(filter.add(20.0));
    TEST_ASSERT_TRUE(filter.add(21.0));
    TEST_ASSERT_TRUE(filter.add(40.0));

    TEST_ASSERT_EQUAL_DOUBLE(21.0, filter.median().value());
}

void test_real_trend_remains_visible() {
    MedianFilter filter(3U);

    TEST_ASSERT_TRUE(filter.add(20.0));
    TEST_ASSERT_TRUE(filter.add(21.0));
    TEST_ASSERT_TRUE(filter.add(22.0));
    TEST_ASSERT_EQUAL_DOUBLE(21.0, filter.median().value());

    TEST_ASSERT_TRUE(filter.add(23.0));
    TEST_ASSERT_EQUAL_DOUBLE(22.0, filter.median().value());
}

void test_invalid_value_does_not_change_window() {
    MedianFilter filter(3U);
    TEST_ASSERT_TRUE(filter.add(20.0));
    TEST_ASSERT_TRUE(filter.add(21.0));
    const double before = filter.median().value();

    TEST_ASSERT_FALSE(filter.add(std::numeric_limits<double>::quiet_NaN()));

    TEST_ASSERT_EQUAL_DOUBLE(before, filter.median().value());
    TEST_ASSERT_EQUAL_size_t(2U, filter.size());
}

void test_even_capacity_is_rejected_defensively() {
    MedianFilter filter(4U);

    TEST_ASSERT_FALSE(filter.add(20.0));
    TEST_ASSERT_FALSE(filter.median().has_value());
    TEST_ASSERT_EQUAL_size_t(0U, filter.size());
}

void test_reset_discards_all_values() {
    MedianFilter filter(3U);
    TEST_ASSERT_TRUE(filter.add(20.0));
    TEST_ASSERT_TRUE(filter.add(21.0));

    filter.reset();

    TEST_ASSERT_FALSE(filter.median().has_value());
    TEST_ASSERT_EQUAL_size_t(0U, filter.size());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_empty_filter_has_no_median);
    RUN_TEST(test_partial_window_uses_existing_measured_values);
    RUN_TEST(test_single_spike_is_removed_after_window_is_full);
    RUN_TEST(test_real_trend_remains_visible);
    RUN_TEST(test_invalid_value_does_not_change_window);
    RUN_TEST(test_even_capacity_is_rejected_defensively);
    RUN_TEST(test_reset_discards_all_values);
    return UNITY_END();
}
