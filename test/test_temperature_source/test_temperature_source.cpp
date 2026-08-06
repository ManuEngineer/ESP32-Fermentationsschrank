#include <unity.h>

#include <optional>

#include "sensor_identity.hpp"
#include "temperature_source.hpp"

namespace {

using device_platform::SensorIdentity;
using device_platform::TemperatureReading;
using device_platform::TemperatureReadingStatus;
using device_platform::TemperatureSampleStatus;

}  // namespace

void test_ok_status_without_celsius_is_rejected() {
    const auto result = TemperatureReading::create(
        /*identity=*/std::nullopt, /*monotonicTimestampMs=*/0U,
        TemperatureSampleStatus::Ok, /*celsius=*/std::nullopt);

    TEST_ASSERT_TRUE(result.status ==
                     TemperatureReadingStatus::InconsistentValuePresence);
    TEST_ASSERT_FALSE(result.reading.has_value());
}

void test_fault_status_with_celsius_is_rejected() {
    const auto result = TemperatureReading::create(
        /*identity=*/std::nullopt, /*monotonicTimestampMs=*/0U,
        TemperatureSampleStatus::BusFault, /*celsius=*/4.0);

    TEST_ASSERT_TRUE(result.status ==
                     TemperatureReadingStatus::InconsistentValuePresence);
    TEST_ASSERT_FALSE(result.reading.has_value());
}

void test_ok_status_with_celsius_and_unknown_identity_succeeds() {
    const auto result = TemperatureReading::create(
        /*identity=*/std::nullopt, /*monotonicTimestampMs=*/1234U,
        TemperatureSampleStatus::Ok, /*celsius=*/4.5);

    TEST_ASSERT_TRUE(result.status == TemperatureReadingStatus::Success);
    TEST_ASSERT_TRUE(result.reading.has_value());
    TEST_ASSERT_FALSE(result.reading->identity().has_value());
    TEST_ASSERT_EQUAL_UINT64(1234U, result.reading->monotonicTimestampMs());
    TEST_ASSERT_TRUE(result.reading->status() == TemperatureSampleStatus::Ok);
    TEST_ASSERT_TRUE(result.reading->celsius().has_value());
    TEST_ASSERT_EQUAL_DOUBLE(4.5, result.reading->celsius().value());
}

void test_ok_status_with_known_identity_succeeds() {
    const auto identity = SensorIdentity::create(7U).identity;
    TEST_ASSERT_TRUE(identity.has_value());

    const auto result = TemperatureReading::create(
        identity, /*monotonicTimestampMs=*/0U, TemperatureSampleStatus::Ok,
        /*celsius=*/1.0);

    TEST_ASSERT_TRUE(result.reading.has_value());
    TEST_ASSERT_TRUE(result.reading->identity().has_value());
    TEST_ASSERT_TRUE(result.reading->identity().value() == identity.value());
}

void test_fault_status_without_celsius_succeeds() {
    const auto result = TemperatureReading::create(
        /*identity=*/std::nullopt, /*monotonicTimestampMs=*/0U,
        TemperatureSampleStatus::CrcFault, /*celsius=*/std::nullopt);

    TEST_ASSERT_TRUE(result.status == TemperatureReadingStatus::Success);
    TEST_ASSERT_TRUE(result.reading.has_value());
    TEST_ASSERT_TRUE(result.reading->status() ==
                     TemperatureSampleStatus::CrcFault);
    TEST_ASSERT_FALSE(result.reading->celsius().has_value());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ok_status_without_celsius_is_rejected);
    RUN_TEST(test_fault_status_with_celsius_is_rejected);
    RUN_TEST(test_ok_status_with_celsius_and_unknown_identity_succeeds);
    RUN_TEST(test_ok_status_with_known_identity_succeeds);
    RUN_TEST(test_fault_status_without_celsius_succeeds);
    return UNITY_END();
}
