#include <unity.h>

#include "civil_calendar.hpp"

namespace {

using device_platform::daysFromCivil;

void test_epoch_is_day_zero() {
    TEST_ASSERT_EQUAL_INT64(0, daysFromCivil(1970, 1U, 1U));
}

void test_known_dates() {
    TEST_ASSERT_EQUAL_INT64(1LL, daysFromCivil(1970, 1U, 2U));
    TEST_ASSERT_EQUAL_INT64(-1LL, daysFromCivil(1969, 12U, 31U));
    // 2026-03-29 00:00Z = 1774742400 s
    TEST_ASSERT_EQUAL_INT64(1774742400LL / 86400LL,
                            daysFromCivil(2026, 3U, 29U));
    // 2038-01-19 00:00Z, around the 32-bit time boundary.
    TEST_ASSERT_EQUAL_INT64(2147472000LL / 86400LL,
                            daysFromCivil(2038, 1U, 19U));
}

void test_leap_day_handling() {
    TEST_ASSERT_EQUAL_INT64(1LL, daysFromCivil(2024, 3U, 1U) -
                                     daysFromCivil(2024, 2U, 29U));
    TEST_ASSERT_EQUAL_INT64(1LL, daysFromCivil(2100, 3U, 1U) -
                                     daysFromCivil(2100, 2U, 28U));
    TEST_ASSERT_EQUAL_INT64(366LL, daysFromCivil(2025, 1U, 1U) -
                                       daysFromCivil(2024, 1U, 1U));
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_epoch_is_day_zero);
    RUN_TEST(test_known_dates);
    RUN_TEST(test_leap_day_handling);
    return UNITY_END();
}
