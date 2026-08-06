#include <unity.h>

#include "sensor_identity.hpp"

namespace {

using device_platform::SensorIdentity;
using device_platform::SensorIdentityStatus;

}  // namespace

void test_zero_is_rejected() {
    const auto result = SensorIdentity::create(0U);

    TEST_ASSERT_TRUE(result.status ==
                     SensorIdentityStatus::ZeroIsNotAValidIdentity);
    TEST_ASSERT_FALSE(result.identity.has_value());
}

void test_positive_value_succeeds() {
    const auto result = SensorIdentity::create(42U);

    TEST_ASSERT_TRUE(result.status == SensorIdentityStatus::Success);
    TEST_ASSERT_TRUE(result.identity.has_value());
    TEST_ASSERT_EQUAL_UINT64(42U, result.identity->value());
}

void test_equality_compares_by_value() {
    const auto a = SensorIdentity::create(5U).identity.value();
    const auto b = SensorIdentity::create(5U).identity.value();
    const auto c = SensorIdentity::create(6U).identity.value();

    TEST_ASSERT_TRUE(a == b);
    TEST_ASSERT_FALSE(a != b);
    TEST_ASSERT_TRUE(a != c);
    TEST_ASSERT_FALSE(a == c);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_zero_is_rejected);
    RUN_TEST(test_positive_value_succeeds);
    RUN_TEST(test_equality_compares_by_value);
    return UNITY_END();
}
