#include <unity.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "sensor_limits.hpp"
#include "sensor_quality_config.hpp"

namespace {

using device_platform::SensorQualityConfig;
using device_platform::SensorQualityConfigStatus;

// Ein vollstaendiger, gueltiger Parametersatz, den jeder Testfall gezielt an
// genau einer Stelle abweichen laesst (KISS - keine 20 fast identischen
// direkten create()-Aufrufe).
struct Args {
    std::size_t medianWindowSize{3U};
    double lowPassTauSeconds{5.0};
    double minPlausibleCelsius{-20.0};
    double maxPlausibleCelsius{80.0};
    double maxRateOfChangeCelsiusPerSecond{5.0};
    uint64_t maxStaleAgeMs{10'000U};
    uint16_t maxConsecutiveInvalid{3U};
    uint16_t minConsecutiveValidSamples{2U};
    uint64_t minRecoveryStabilityDurationMs{2'000U};
};

device_platform::SensorQualityConfigCreateResult createFrom(const Args& a) {
    return SensorQualityConfig::create(
        a.medianWindowSize, a.lowPassTauSeconds, a.minPlausibleCelsius,
        a.maxPlausibleCelsius, a.maxRateOfChangeCelsiusPerSecond,
        a.maxStaleAgeMs, a.maxConsecutiveInvalid, a.minConsecutiveValidSamples,
        a.minRecoveryStabilityDurationMs);
}

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

}  // namespace

void test_valid_arguments_succeed_and_roundtrip() {
    const Args a{};
    const auto result = createFrom(a);

    TEST_ASSERT_TRUE(result.status == SensorQualityConfigStatus::Success);
    TEST_ASSERT_TRUE(result.config.has_value());
    TEST_ASSERT_EQUAL_size_t(a.medianWindowSize,
                             result.config->medianWindowSize());
    TEST_ASSERT_EQUAL_DOUBLE(a.lowPassTauSeconds,
                             result.config->lowPassTauSeconds());
    TEST_ASSERT_EQUAL_DOUBLE(a.minPlausibleCelsius,
                             result.config->minPlausibleCelsius());
    TEST_ASSERT_EQUAL_DOUBLE(a.maxPlausibleCelsius,
                             result.config->maxPlausibleCelsius());
    TEST_ASSERT_EQUAL_DOUBLE(a.maxRateOfChangeCelsiusPerSecond,
                             result.config->maxRateOfChangeCelsiusPerSecond());
    TEST_ASSERT_EQUAL_UINT64(a.maxStaleAgeMs, result.config->maxStaleAgeMs());
    TEST_ASSERT_EQUAL_UINT16(a.maxConsecutiveInvalid,
                             result.config->maxConsecutiveInvalid());
    TEST_ASSERT_EQUAL_UINT16(a.minConsecutiveValidSamples,
                             result.config->minConsecutiveValidSamples());
    TEST_ASSERT_EQUAL_UINT64(a.minRecoveryStabilityDurationMs,
                             result.config->minRecoveryStabilityDurationMs());
}

void test_nonfinite_lowpass_tau_is_rejected() {
    Args a{};
    a.lowPassTauSeconds = kNan;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::NonFiniteParameter);
}

void test_nonfinite_min_plausible_celsius_is_rejected() {
    Args a{};
    a.minPlausibleCelsius = kInf;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::NonFiniteParameter);
}

void test_nonfinite_max_plausible_celsius_is_rejected() {
    Args a{};
    a.maxPlausibleCelsius = kNan;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::NonFiniteParameter);
}

void test_nonfinite_rate_of_change_limit_is_rejected() {
    Args a{};
    a.maxRateOfChangeCelsiusPerSecond = -kInf;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::NonFiniteParameter);
}

void test_zero_median_window_size_is_rejected() {
    Args a{};
    a.medianWindowSize = 0U;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidMedianWindowSize);
}

void test_even_median_window_size_is_rejected() {
    Args a{};
    a.medianWindowSize = 4U;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidMedianWindowSize);
}

void test_median_window_size_above_ceiling_is_rejected() {
    Args a{};
    a.medianWindowSize = device_platform::sensor_limits::kMaxMedianWindowSize +
                          2U;  // naechster ungerader Wert oberhalb der Grenze
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidMedianWindowSize);
}

void test_nonpositive_lowpass_tau_is_rejected() {
    Args a{};
    a.lowPassTauSeconds = 0.0;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidLowPassTimeConstant);
}

void test_inverted_plausible_range_is_rejected() {
    Args a{};
    a.minPlausibleCelsius = 10.0;
    a.maxPlausibleCelsius = 10.0;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidPlausibleRange);
}

void test_plausible_range_below_firmware_outer_bound_is_rejected() {
    Args a{};
    a.minPlausibleCelsius = device_platform::sensor_limits::kAbsoluteMinCelsius - 1.0;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidPlausibleRange);
}

void test_plausible_range_above_firmware_outer_bound_is_rejected() {
    Args a{};
    a.maxPlausibleCelsius = device_platform::sensor_limits::kAbsoluteMaxCelsius + 1.0;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidPlausibleRange);
}

void test_nonpositive_rate_of_change_limit_is_rejected() {
    Args a{};
    a.maxRateOfChangeCelsiusPerSecond = 0.0;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidRateOfChangeLimit);
}

void test_zero_stale_age_threshold_is_rejected() {
    Args a{};
    a.maxStaleAgeMs = 0U;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidStaleAgeThreshold);
}

void test_stale_age_threshold_above_ceiling_is_rejected() {
    Args a{};
    a.maxStaleAgeMs =
        device_platform::sensor_limits::kMaxStaleAgeCeilingMs + 1U;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidStaleAgeThreshold);
}

void test_zero_consecutive_invalid_limit_is_rejected() {
    Args a{};
    a.maxConsecutiveInvalid = 0U;
    TEST_ASSERT_TRUE(
        createFrom(a).status ==
        SensorQualityConfigStatus::InvalidConsecutiveInvalidLimit);
}

void test_consecutive_invalid_limit_above_ceiling_is_rejected() {
    Args a{};
    a.maxConsecutiveInvalid =
        device_platform::sensor_limits::kMaxConsecutiveInvalidCeiling + 1U;
    TEST_ASSERT_TRUE(
        createFrom(a).status ==
        SensorQualityConfigStatus::InvalidConsecutiveInvalidLimit);
}

void test_zero_min_consecutive_valid_samples_is_rejected() {
    Args a{};
    a.minConsecutiveValidSamples = 0U;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidRecoveryThresholds);
}

void test_single_min_consecutive_valid_sample_is_rejected() {
    Args a{};
    a.minConsecutiveValidSamples = 1U;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidRecoveryThresholds);
}

void test_two_min_consecutive_valid_samples_succeeds() {
    Args a{};
    a.minConsecutiveValidSamples = 2U;
    TEST_ASSERT_TRUE(createFrom(a).status == SensorQualityConfigStatus::Success);
}

void test_zero_recovery_stability_duration_is_rejected() {
    Args a{};
    a.minRecoveryStabilityDurationMs = 0U;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidRecoveryThresholds);
}

void test_recovery_stability_duration_above_ceiling_is_rejected() {
    Args a{};
    a.minRecoveryStabilityDurationMs =
        device_platform::sensor_limits::kMaxRecoveryStabilityDurationCeilingMs +
        1U;
    TEST_ASSERT_TRUE(createFrom(a).status ==
                      SensorQualityConfigStatus::InvalidRecoveryThresholds);
}

void test_recovery_stability_duration_at_ceiling_succeeds() {
    Args a{};
    a.minRecoveryStabilityDurationMs =
        device_platform::sensor_limits::kMaxRecoveryStabilityDurationCeilingMs;
    TEST_ASSERT_TRUE(createFrom(a).status == SensorQualityConfigStatus::Success);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_valid_arguments_succeed_and_roundtrip);
    RUN_TEST(test_nonfinite_lowpass_tau_is_rejected);
    RUN_TEST(test_nonfinite_min_plausible_celsius_is_rejected);
    RUN_TEST(test_nonfinite_max_plausible_celsius_is_rejected);
    RUN_TEST(test_nonfinite_rate_of_change_limit_is_rejected);
    RUN_TEST(test_zero_median_window_size_is_rejected);
    RUN_TEST(test_even_median_window_size_is_rejected);
    RUN_TEST(test_median_window_size_above_ceiling_is_rejected);
    RUN_TEST(test_nonpositive_lowpass_tau_is_rejected);
    RUN_TEST(test_inverted_plausible_range_is_rejected);
    RUN_TEST(test_plausible_range_below_firmware_outer_bound_is_rejected);
    RUN_TEST(test_plausible_range_above_firmware_outer_bound_is_rejected);
    RUN_TEST(test_nonpositive_rate_of_change_limit_is_rejected);
    RUN_TEST(test_zero_stale_age_threshold_is_rejected);
    RUN_TEST(test_stale_age_threshold_above_ceiling_is_rejected);
    RUN_TEST(test_zero_consecutive_invalid_limit_is_rejected);
    RUN_TEST(test_consecutive_invalid_limit_above_ceiling_is_rejected);
    RUN_TEST(test_zero_min_consecutive_valid_samples_is_rejected);
    RUN_TEST(test_single_min_consecutive_valid_sample_is_rejected);
    RUN_TEST(test_two_min_consecutive_valid_samples_succeeds);
    RUN_TEST(test_zero_recovery_stability_duration_is_rejected);
    RUN_TEST(test_recovery_stability_duration_above_ceiling_is_rejected);
    RUN_TEST(test_recovery_stability_duration_at_ceiling_succeeds);
    return UNITY_END();
}
