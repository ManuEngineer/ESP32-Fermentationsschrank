#include <unity.h>

#include <limits>

#include "sensor_calibration.hpp"
#include "sensor_identity.hpp"
#include "sensor_limits.hpp"
#include "sensor_offset.hpp"

namespace {

using device_platform::SensorCalibration;
using device_platform::SensorIdentity;
using device_platform::SensorOffset;
using device_platform::SensorOffsetStatus;

}  // namespace

void test_offset_inside_firmware_range_succeeds() {
    const auto result = SensorOffset::create(2.5);

    TEST_ASSERT_TRUE(result.status == SensorOffsetStatus::Success);
    TEST_ASSERT_TRUE(result.offset.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(2.5, result.offset->celsius());
}

void test_explicit_zero_offset_is_a_valid_value() {
    const auto result = SensorOffset::create(0.0);

    TEST_ASSERT_TRUE(result.status == SensorOffsetStatus::Success);
    TEST_ASSERT_TRUE(result.offset.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(0.0, result.offset->celsius());
}

void test_offset_at_firmware_boundary_succeeds() {
    const auto result = SensorOffset::create(
        device_platform::sensor_limits::kMaxAbsoluteOffsetCelsius);

    TEST_ASSERT_TRUE(result.status == SensorOffsetStatus::Success);
    TEST_ASSERT_TRUE(result.offset.has_value());
}

void test_nonfinite_offset_is_rejected_before_range_check() {
    const auto nanResult =
        SensorOffset::create(std::numeric_limits<double>::quiet_NaN());
    const auto infResult =
        SensorOffset::create(std::numeric_limits<double>::infinity());

    TEST_ASSERT_TRUE(nanResult.status == SensorOffsetStatus::NonFinite);
    TEST_ASSERT_TRUE(infResult.status == SensorOffsetStatus::NonFinite);
    TEST_ASSERT_FALSE(nanResult.offset.has_value());
    TEST_ASSERT_FALSE(infResult.offset.has_value());
}

void test_offset_outside_firmware_range_is_rejected() {
    const auto result = SensorOffset::create(
        device_platform::sensor_limits::kMaxAbsoluteOffsetCelsius + 0.1);

    TEST_ASSERT_TRUE(result.status == SensorOffsetStatus::OutOfFirmwareRange);
    TEST_ASSERT_FALSE(result.offset.has_value());
}

void test_sensor_calibration_roundtrips_identity_and_offset() {
    const auto identity = SensorIdentity::create(42U).identity.value();
    const auto offset = SensorOffset::create(-1.25).offset.value();
    const SensorCalibration calibration(identity, offset);

    TEST_ASSERT_TRUE(calibration.identity() == identity);
    TEST_ASSERT_EQUAL_DOUBLE(offset.celsius(), calibration.offset().celsius());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_offset_inside_firmware_range_succeeds);
    RUN_TEST(test_explicit_zero_offset_is_a_valid_value);
    RUN_TEST(test_offset_at_firmware_boundary_succeeds);
    RUN_TEST(test_nonfinite_offset_is_rejected_before_range_check);
    RUN_TEST(test_offset_outside_firmware_range_is_rejected);
    RUN_TEST(test_sensor_calibration_roundtrips_identity_and_offset);
    return UNITY_END();
}
