#include <unity.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include "raw_temperature_samples.hpp"
#include "sensor_identity.hpp"
#include "temperature_source.hpp"
#include "thermal_safety_supervisor.hpp"

// R1 trial plan S6: firmware temperature safety on the raw cooling-body and
// cabinet-air readings. All numbers are TEST fixtures, no product value.

namespace {

using namespace fermentation;
using device_platform::TemperatureReading;
using device_platform::TemperatureSampleStatus;

ThermalSafetyLimits testLimits() {
    ThermalSafetyLimits limits;
    limits.airEmergencyLowerCelsius = -10.0;
    limits.airInterventionLowerCelsius = 0.0;
    limits.airInterventionUpperCelsius = 40.0;
    limits.airEmergencyUpperCelsius = 50.0;
    limits.coolingEmergencyLowerCelsius = -20.0;
    limits.coolingInterventionLowerCelsius = -10.0;
    limits.coolingInterventionUpperCelsius = 60.0;
    limits.coolingEmergencyUpperCelsius = 70.0;
    limits.coolingMaxRiseCelsiusPerMinute = 6.0;
    limits.trendWindowMillis = 60'000U;
    limits.maxSampleAgeMillis = 5'000U;
    limits.invalidLatchMillis = 20'000U;
    return limits;
}

std::optional<TemperatureReading> reading(
    std::uint64_t stamp, double celsius,
    TemperatureSampleStatus status = TemperatureSampleStatus::Ok) {
    const bool ok = status == TemperatureSampleStatus::Ok;
    return TemperatureReading::create(
               device_platform::SensorIdentity::create(0x28A6ULL).identity,
               stamp, status,
               ok ? std::optional<double>{celsius} : std::nullopt)
        .reading;
}

RawTemperatureSamples samples(std::uint64_t stamp, double air, double cooling) {
    RawTemperatureSamples result;
    result.air = reading(stamp, air);
    result.cooling = reading(stamp, cooling);
    return result;
}

void test_unconfigured_is_the_default_and_denies() {
    ThermalSafetySupervisor supervisor;
    const auto result =
        supervisor.evaluate(samples(1'000U, 20.0, 20.0), 1'000U);
    TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Unconfigured);
    TEST_ASSERT_FALSE(supervisor.configured());
}

void test_limit_validation_rejects_every_malformed_set() {
    auto expectRejected = [](ThermalSafetyLimits limits) {
        ThermalSafetySupervisor supervisor;
        TEST_ASSERT_TRUE(supervisor.configure(limits) ==
                         ThermalSafetyConfigureResult::InvalidLimits);
        TEST_ASSERT_FALSE(supervisor.configured());
    };
    ThermalSafetySupervisor ok;
    TEST_ASSERT_TRUE(ok.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    auto l = testLimits();
    l.airInterventionUpperCelsius = l.airEmergencyUpperCelsius;  // not ordered
    expectRejected(l);
    l = testLimits();
    l.coolingEmergencyLowerCelsius = l.coolingInterventionLowerCelsius;
    expectRejected(l);
    l = testLimits();
    l.coolingEmergencyUpperCelsius = 151.0;  // beyond the measuring range
    expectRejected(l);
    l = testLimits();
    l.airEmergencyLowerCelsius = -41.0;
    expectRejected(l);
    l = testLimits();
    l.coolingInterventionUpperCelsius =
        std::numeric_limits<double>::quiet_NaN();
    expectRejected(l);
    l = testLimits();
    l.coolingMaxRiseCelsiusPerMinute = 0.0;
    expectRejected(l);
    l = testLimits();
    l.coolingMaxRiseCelsiusPerMinute =
        kThermalSafetyMaxRiseCelsiusPerMinute + 1.0;
    expectRejected(l);
    l = testLimits();
    l.trendWindowMillis = kThermalSafetyMinTrendWindowMillis - 1U;
    expectRejected(l);
    l = testLimits();
    l.maxSampleAgeMillis = kThermalSafetyMaxSampleAgeMillis + 1U;
    expectRejected(l);
    l = testLimits();
    l.invalidLatchMillis = l.maxSampleAgeMillis - 1U;
    expectRejected(l);
    l = testLimits();
    l.invalidLatchMillis = kThermalSafetyMaxInvalidLatchMillis + 1U;
    expectRejected(l);
}

void test_blocked_until_first_valid_evaluation_then_ok() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    TEST_ASSERT_TRUE(supervisor.last().status == ThermalSafetyStatus::Blocked);
    const auto result =
        supervisor.evaluate(samples(10'000U, 25.0, 30.0), 10'000U);
    TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Ok);
    TEST_ASSERT_TRUE(result.reason == ThermalSafetyReason::None);
}

void test_every_limit_level_trips_and_latches() {
    struct Case {
        double air, cooling;
        ThermalSafetyReason reason;
    };
    const Case cases[] = {
        {-10.0, 30.0, ThermalSafetyReason::AirLowerEmergency},
        {0.0, 30.0, ThermalSafetyReason::AirLowerIntervention},
        {40.0, 30.0, ThermalSafetyReason::AirUpperIntervention},
        {50.0, 30.0, ThermalSafetyReason::AirUpperEmergency},
        {25.0, -20.0, ThermalSafetyReason::CoolingLowerEmergency},
        {25.0, -10.0, ThermalSafetyReason::CoolingLowerIntervention},
        {25.0, 60.0, ThermalSafetyReason::CoolingUpperIntervention},
        {25.0, 70.0, ThermalSafetyReason::CoolingUpperEmergency},
    };
    for (const auto& c : cases) {
        ThermalSafetySupervisor supervisor;
        TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                         ThermalSafetyConfigureResult::Accepted);
        const auto result =
            supervisor.evaluate(samples(10'000U, c.air, c.cooling), 10'000U);
        TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Tripped);
        TEST_ASSERT_TRUE(result.reason == c.reason);
        // The latch holds although the values return to normal.
        const auto later =
            supervisor.evaluate(samples(12'000U, 25.0, 30.0), 12'000U);
        TEST_ASSERT_TRUE(later.status == ThermalSafetyStatus::Tripped);
        TEST_ASSERT_TRUE(later.reason == c.reason);
        TEST_ASSERT_TRUE(supervisor.latched());
    }
}

void test_emergency_level_wins_over_intervention() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    const auto result =
        supervisor.evaluate(samples(10'000U, 25.0, 75.0), 10'000U);
    TEST_ASSERT_TRUE(result.reason ==
                     ThermalSafetyReason::CoolingUpperEmergency);
}

void test_invalid_readings_block_and_latch_after_the_configured_time() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    RawTemperatureSamples bad = samples(10'000U, 25.0, 30.0);
    bad.cooling = reading(10'000U, 0.0, TemperatureSampleStatus::CrcFault);
    auto result = supervisor.evaluate(bad, 10'000U);
    TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Blocked);
    TEST_ASSERT_TRUE(result.reason == ThermalSafetyReason::SampleFault);
    result = supervisor.evaluate(bad, 29'999U);
    TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Blocked);
    result = supervisor.evaluate(bad, 30'000U);
    TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Tripped);
    TEST_ASSERT_TRUE(result.reason == ThermalSafetyReason::SampleLoss);
}

void test_recovered_readings_clear_the_invalid_timer_but_not_a_trip() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    RawTemperatureSamples bad = samples(10'000U, 25.0, 30.0);
    bad.air.reset();
    TEST_ASSERT_TRUE(supervisor.evaluate(bad, 10'000U).status ==
                     ThermalSafetyStatus::Blocked);
    TEST_ASSERT_TRUE(
        supervisor.evaluate(samples(18'000U, 25.0, 30.0), 18'000U).status ==
        ThermalSafetyStatus::Ok);
    // A new invalid episode starts a fresh timer.
    TEST_ASSERT_TRUE(supervisor.evaluate(bad, 19'000U).status ==
                     ThermalSafetyStatus::Blocked);
    TEST_ASSERT_TRUE(supervisor.evaluate(bad, 38'999U).status ==
                     ThermalSafetyStatus::Blocked);
}

void test_each_invalid_kind_is_blocked_with_its_reason() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    RawTemperatureSamples s = samples(10'000U, 25.0, 30.0);
    s.cooling.reset();
    TEST_ASSERT_TRUE(supervisor.evaluate(s, 10'000U).reason ==
                     ThermalSafetyReason::SampleMissing);
    s = samples(10'000U, 25.0, 30.0);
    s.air = reading(10'000U, 0.0, TemperatureSampleStatus::BusFault);
    TEST_ASSERT_TRUE(supervisor.evaluate(s, 10'000U).reason ==
                     ThermalSafetyReason::SampleFault);
    // Stale (older than the maximum age) and future-dated readings.
    s = samples(1'000U, 25.0, 30.0);
    TEST_ASSERT_TRUE(supervisor.evaluate(s, 6'001U).reason ==
                     ThermalSafetyReason::SampleStale);
    s = samples(9'000U, 25.0, 30.0);
    TEST_ASSERT_TRUE(supervisor.evaluate(s, 8'000U).reason ==
                     ThermalSafetyReason::SampleStale);
    // 85 degrees C is not special; a value outside the measuring range is.
    s = samples(10'000U, 25.0, 30.0);
    s.cooling = reading(10'000U, 151.0);
    TEST_ASSERT_TRUE(supervisor.evaluate(s, 10'000U).reason ==
                     ThermalSafetyReason::OutOfMeasuringRange);
    s.cooling = reading(10'000U, std::numeric_limits<double>::infinity());
    TEST_ASSERT_TRUE(supervisor.evaluate(s, 10'000U).status ==
                     ThermalSafetyStatus::Blocked);
}

void test_a_limit_excursion_trips_even_if_the_other_sensor_is_invalid() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    RawTemperatureSamples s = samples(10'000U, 25.0, 65.0);
    s.air.reset();
    const auto result = supervisor.evaluate(s, 10'000U);
    TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Tripped);
    TEST_ASSERT_TRUE(result.reason ==
                     ThermalSafetyReason::CoolingUpperIntervention);
}

void test_sustained_rise_trips_but_a_short_step_does_not() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    // 2 s cycle; +10 K/min would trip over a long enough span.
    std::uint64_t now = 10'000U;
    double cooling = 20.0;
    ThermalSafetyEvaluation result;
    // A first 20 s of +10 K/min is below half of the window: no decision yet.
    for (int i = 0; i < 10; ++i) {
        result = supervisor.evaluate(samples(now, 25.0, cooling), now);
        TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Ok);
        now += 2'000U;
        cooling += 10.0 / 30.0;
    }
    bool tripped = false;
    for (int i = 0; i < 20 && !tripped; ++i) {
        result = supervisor.evaluate(samples(now, 25.0, cooling), now);
        tripped = result.status == ThermalSafetyStatus::Tripped;
        now += 2'000U;
        cooling += 10.0 / 30.0;
    }
    TEST_ASSERT_TRUE(tripped);
    TEST_ASSERT_TRUE(result.reason == ThermalSafetyReason::CoolingRiseRate);
}

void test_a_slow_rise_never_trips() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    std::uint64_t now = 10'000U;
    double cooling = 20.0;
    for (int i = 0; i < 80; ++i) {
        const auto result =
            supervisor.evaluate(samples(now, 25.0, cooling), now);
        TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Ok);
        now += 2'000U;
        cooling += 2.0 / 30.0;  // +2 K/min < 6 K/min
    }
}

void test_evaluation_is_idempotent_for_identical_input() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    for (int i = 0; i < 200; ++i) {
        TEST_ASSERT_TRUE(
            supervisor.evaluate(samples(10'000U, 25.0, 30.0), 10'000U).status ==
            ThermalSafetyStatus::Ok);
    }
}

void test_latch_refuses_configuration_and_survives_clearing() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    TEST_ASSERT_TRUE(
        supervisor.evaluate(samples(10'000U, 25.0, 80.0), 10'000U).status ==
        ThermalSafetyStatus::Tripped);
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Latched);
    supervisor.clearConfiguration();
    TEST_ASSERT_TRUE(
        supervisor.evaluate(samples(12'000U, 25.0, 30.0), 12'000U).status ==
        ThermalSafetyStatus::Tripped);
}

void test_clearing_the_configuration_returns_to_unconfigured() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    supervisor.clearConfiguration();
    TEST_ASSERT_TRUE(
        supervisor.evaluate(samples(10'000U, 25.0, 30.0), 10'000U).status ==
        ThermalSafetyStatus::Unconfigured);
}

void test_names_are_stable() {
    TEST_ASSERT_EQUAL_STRING(
        "tripped", thermalSafetyStatusName(ThermalSafetyStatus::Tripped));
    TEST_ASSERT_EQUAL_STRING(
        "cooling_rise_rate",
        thermalSafetyReasonName(ThermalSafetyReason::CoolingRiseRate));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_unconfigured_is_the_default_and_denies);
    RUN_TEST(test_limit_validation_rejects_every_malformed_set);
    RUN_TEST(test_blocked_until_first_valid_evaluation_then_ok);
    RUN_TEST(test_every_limit_level_trips_and_latches);
    RUN_TEST(test_emergency_level_wins_over_intervention);
    RUN_TEST(test_invalid_readings_block_and_latch_after_the_configured_time);
    RUN_TEST(test_recovered_readings_clear_the_invalid_timer_but_not_a_trip);
    RUN_TEST(test_each_invalid_kind_is_blocked_with_its_reason);
    RUN_TEST(test_a_limit_excursion_trips_even_if_the_other_sensor_is_invalid);
    RUN_TEST(test_sustained_rise_trips_but_a_short_step_does_not);
    RUN_TEST(test_a_slow_rise_never_trips);
    RUN_TEST(test_evaluation_is_idempotent_for_identical_input);
    RUN_TEST(test_latch_refuses_configuration_and_survives_clearing);
    RUN_TEST(test_clearing_the_configuration_returns_to_unconfigured);
    RUN_TEST(test_names_are_stable);
    return UNITY_END();
}
