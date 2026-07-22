#include <unity.h>

#include "virtual_time_source.hpp"

void test_monotonic_time_starts_at_zero() {
    const device_platform::VirtualTimeSource timeSource;

    TEST_ASSERT_EQUAL_UINT64(0U, timeSource.monotonicMillis());
}

void test_monotonic_time_advances_by_requested_delta() {
    device_platform::VirtualTimeSource timeSource;

    timeSource.advanceMonotonicMillis(250);
    TEST_ASSERT_EQUAL_UINT64(250U, timeSource.monotonicMillis());

    timeSource.advanceMonotonicMillis(750);
    TEST_ASSERT_EQUAL_UINT64(1000U, timeSource.monotonicMillis());
}

void test_absolute_time_is_absent_by_default() {
    const device_platform::VirtualTimeSource timeSource;

    TEST_ASSERT_FALSE(timeSource.unixTimeSeconds().has_value());
}

void test_absolute_time_can_be_set_and_cleared() {
    device_platform::VirtualTimeSource timeSource;

    timeSource.setUnixTimeSeconds(1700000000);
    TEST_ASSERT_TRUE(timeSource.unixTimeSeconds().has_value());
    TEST_ASSERT_EQUAL_INT64(1700000000, timeSource.unixTimeSeconds().value());

    timeSource.setUnixTimeSeconds(std::nullopt);
    TEST_ASSERT_FALSE(timeSource.unixTimeSeconds().has_value());
}

void test_absolute_time_change_does_not_affect_monotonic_time() {
    device_platform::VirtualTimeSource timeSource;

    timeSource.advanceMonotonicMillis(5000);
    timeSource.setUnixTimeSeconds(1700000000);

    TEST_ASSERT_EQUAL_UINT64(5000U, timeSource.monotonicMillis());
}

void test_new_instance_after_restart_resets_monotonic_time() {
    device_platform::VirtualTimeSource beforeRestart;
    beforeRestart.advanceMonotonicMillis(123456);

    const device_platform::VirtualTimeSource afterRestart;

    TEST_ASSERT_EQUAL_UINT64(123456U, beforeRestart.monotonicMillis());
    TEST_ASSERT_EQUAL_UINT64(0U, afterRestart.monotonicMillis());
}

void test_forward_time_jump_is_reflected_immediately() {
    device_platform::VirtualTimeSource timeSource;

    timeSource.advanceMonotonicMillis(1000);
    // Simuliert einen grossen Zeitvorwaertssprung, z. B. nach langer
    // Inaktivitaet oder einer nachtraeglichen Zeitkorrektur.
    timeSource.advanceMonotonicMillis(86400000ULL);

    TEST_ASSERT_EQUAL_UINT64(86401000ULL, timeSource.monotonicMillis());
}

void test_two_instances_are_reproducible_given_the_same_advances() {
    // Beweist, dass das Verhalten ausschliesslich von expliziten Vorschaltungen
    // abhaengt und nicht von der realen Uhrzeit oder Ausfuehrungsreihenfolge
    // beeinflusst wird.
    device_platform::VirtualTimeSource first;
    device_platform::VirtualTimeSource second;

    for (const uint64_t deltaMs : {100U, 900U, 3000U}) {
        first.advanceMonotonicMillis(deltaMs);
        second.advanceMonotonicMillis(deltaMs);
    }

    TEST_ASSERT_EQUAL_UINT64(first.monotonicMillis(), second.monotonicMillis());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_monotonic_time_starts_at_zero);
    RUN_TEST(test_monotonic_time_advances_by_requested_delta);
    RUN_TEST(test_absolute_time_is_absent_by_default);
    RUN_TEST(test_absolute_time_can_be_set_and_cleared);
    RUN_TEST(test_absolute_time_change_does_not_affect_monotonic_time);
    RUN_TEST(test_new_instance_after_restart_resets_monotonic_time);
    RUN_TEST(test_forward_time_jump_is_reflected_immediately);
    RUN_TEST(test_two_instances_are_reproducible_given_the_same_advances);
    return UNITY_END();
}
