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

// Feeds constant readings every `stepMs` until the observation covers at least
// half of the trend window; returns the time of the Ok evaluation. Only then is
// `Ok` possible (no judgement without enough observation).
std::uint64_t primeToOk(ThermalSafetySupervisor& supervisor,
                        std::uint64_t start, double air = 25.0,
                        double cooling = 30.0, std::uint64_t stepMs = 2'000U) {
    const auto window = supervisor.limits().trendWindowMillis;
    std::uint64_t now = start;
    for (;;) {
        const auto result =
            supervisor.evaluate(samples(now, air, cooling), now);
        if (result.status == ThermalSafetyStatus::Ok) return now;
        TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Blocked);
        TEST_ASSERT_TRUE(result.reason ==
                         ThermalSafetyReason::TrendObservationPending);
        TEST_ASSERT_TRUE(now - start <= window);
        now += stepMs;
    }
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

void test_blocked_until_enough_observation_then_ok() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    TEST_ASSERT_TRUE(supervisor.last().status == ThermalSafetyStatus::Blocked);
    // The first valid reading cannot prove a trend: never Ok. Readings keep
    // coming every 2 s; pending until half of the 60 s window is observed.
    ThermalSafetyEvaluation result;
    for (std::uint64_t now = 10'000U; now < 40'000U; now += 2'000U) {
        result = supervisor.evaluate(samples(now, 25.0, 30.0), now);
        TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Blocked);
        TEST_ASSERT_TRUE(result.reason ==
                         ThermalSafetyReason::TrendObservationPending);
    }
    // Half of the window observed (30 s): judged and Ok.
    result = supervisor.evaluate(samples(40'000U, 25.0, 30.0), 40'000U);
    TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Ok);
    TEST_ASSERT_TRUE(result.reason == ThermalSafetyReason::None);
}

void test_a_gap_breaks_the_observation_and_never_yields_an_ok() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    const auto ok = primeToOk(supervisor, 10'000U);
    TEST_ASSERT_TRUE(supervisor.evaluate(samples(ok, 25.0, 30.0), ok).status ==
                     ThermalSafetyStatus::Ok);
    // Readings stop for longer than the sample-age limit, then resume: the old
    // history is discarded and the observation starts again.
    const auto resume = ok + 12'000U;
    const auto result =
        supervisor.evaluate(samples(resume, 25.0, 30.0), resume);
    TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Blocked);
    TEST_ASSERT_TRUE(result.reason ==
                     ThermalSafetyReason::TrendObservationPending);
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
    const auto ok = primeToOk(supervisor, 18'000U);
    // A new invalid episode starts a fresh timer.
    TEST_ASSERT_TRUE(supervisor.evaluate(bad, ok + 1'000U).status ==
                     ThermalSafetyStatus::Blocked);
    TEST_ASSERT_TRUE(supervisor.evaluate(bad, ok + 19'999U).status ==
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
    // 2 s cycle; +10 K/min (limit 6 K/min) over a 60 s window.
    std::uint64_t now = 10'000U;
    double cooling = 20.0;
    ThermalSafetyEvaluation result;
    bool tripped = false;
    for (int i = 0; i < 60 && !tripped; ++i) {
        result = supervisor.evaluate(samples(now, 25.0, cooling), now);
        // Until half the window is observed there is no Ok (pending).
        if (now - 10'000U < 30'000U) {
            TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Blocked);
        }
        tripped = result.status == ThermalSafetyStatus::Tripped;
        now += 2'000U;
        cooling += 10.0 / 30.0;
    }
    TEST_ASSERT_TRUE(tripped);
    TEST_ASSERT_TRUE(result.reason == ThermalSafetyReason::CoolingRiseRate);
    // It trips as soon as half a window of a dangerous rise is observable.
    TEST_ASSERT_TRUE(now - 2'000U - 10'000U <= 36'000U);
}

// R221-1: the longest permitted window (600 s) at the real ~2 s sensor cycle
// must be evaluable. The former 64-point ring covered only ~126 s, so the rise
// alarm could never fire.
void test_the_longest_window_trips_on_a_dangerous_rise_at_two_second_samples() {
    ThermalSafetyLimits limits = testLimits();
    limits.trendWindowMillis = kThermalSafetyMaxTrendWindowMillis;  // 600 s
    limits.coolingInterventionUpperCelsius = 90.0;
    limits.coolingEmergencyUpperCelsius = 100.0;
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(limits) ==
                     ThermalSafetyConfigureResult::Accepted);
    std::uint64_t now = 10'000U;
    double cooling = 20.0;
    bool tripped = false;
    ThermalSafetyEvaluation result;
    for (int i = 0; i < 400 && !tripped; ++i) {  // up to 800 s
        result = supervisor.evaluate(samples(now, 25.0, cooling), now);
        tripped = result.status == ThermalSafetyStatus::Tripped;
        now += 2'000U;
        cooling += 10.0 / 30.0;  // +10 K/min > 6 K/min
    }
    TEST_ASSERT_TRUE(tripped);
    TEST_ASSERT_TRUE(result.reason == ThermalSafetyReason::CoolingRiseRate);
    // Within half a window plus margin of observation, not after minutes more.
    TEST_ASSERT_TRUE(now - 2'000U - 10'000U <= 330'000U);
}

void test_the_longest_window_stays_ok_for_a_slow_warming() {
    ThermalSafetyLimits limits = testLimits();
    limits.trendWindowMillis = kThermalSafetyMaxTrendWindowMillis;
    limits.coolingInterventionUpperCelsius = 90.0;
    limits.coolingEmergencyUpperCelsius = 100.0;
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(limits) ==
                     ThermalSafetyConfigureResult::Accepted);
    std::uint64_t now = 10'000U;
    double cooling = 20.0;
    for (int i = 0; i < 600; ++i) {  // 1200 s at +2 K/min < 6 K/min
        const auto result =
            supervisor.evaluate(samples(now, 25.0, cooling), now);
        TEST_ASSERT_TRUE(result.status != ThermalSafetyStatus::Tripped);
        if (now - 10'000U >= 300'000U) {
            TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Ok);
        } else {
            TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Blocked);
        }
        now += 2'000U;
        cooling += 2.0 / 30.0;
    }
}

// Irregular sample intervals (1 s ... 4 s, deterministic pseudo-random) must
// not hide the same dangerous rise.
void test_irregular_sample_intervals_still_trip_on_a_dangerous_rise() {
    ThermalSafetyLimits limits = testLimits();
    limits.trendWindowMillis = 300'000U;
    limits.coolingInterventionUpperCelsius = 90.0;
    limits.coolingEmergencyUpperCelsius = 100.0;
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(limits) ==
                     ThermalSafetyConfigureResult::Accepted);
    std::uint64_t now = 10'000U;
    std::uint32_t seed = 12345U;
    bool tripped = false;
    ThermalSafetyEvaluation result;
    for (int i = 0; i < 600 && !tripped; ++i) {
        const double minutes = static_cast<double>(now - 10'000U) / 60'000.0;
        result =
            supervisor.evaluate(samples(now, 25.0, 20.0 + 10.0 * minutes), now);
        tripped = result.status == ThermalSafetyStatus::Tripped;
        seed = seed * 1664525U + 1013904223U;
        now += 1'000U + (seed >> 16U) % 3'001U;  // 1..4 s
    }
    TEST_ASSERT_TRUE(tripped);
    TEST_ASSERT_TRUE(result.reason == ThermalSafetyReason::CoolingRiseRate);
}

// A rise that starts late is caught against a recent baseline, not only the
// oldest one: 40 K within the last 200 s of a 600 s window.
void test_a_late_fast_rise_is_caught_against_a_recent_baseline() {
    ThermalSafetyLimits limits = testLimits();
    limits.trendWindowMillis = kThermalSafetyMaxTrendWindowMillis;
    limits.coolingInterventionUpperCelsius = 100.0;
    limits.coolingEmergencyUpperCelsius = 110.0;
    limits.coolingMaxRiseCelsiusPerMinute = 6.0;
    // Needs the measuring range: use a wide emergency band.
    limits.coolingEmergencyUpperCelsius = 120.0;
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(limits) ==
                     ThermalSafetyConfigureResult::Accepted);
    std::uint64_t now = 10'000U;
    // 400 s flat at 20 degrees C, then +20 K/min.
    for (int i = 0; i < 200; ++i, now += 2'000U) {
        TEST_ASSERT_TRUE(
            supervisor.evaluate(samples(now, 25.0, 20.0), now).status !=
            ThermalSafetyStatus::Tripped);
    }
    double cooling = 20.0;
    bool tripped = false;
    for (int i = 0; i < 120 && !tripped; ++i) {
        cooling += 20.0 / 30.0;
        tripped =
            supervisor.evaluate(samples(now, 25.0, cooling), now).status ==
            ThermalSafetyStatus::Tripped;
        now += 2'000U;
    }
    TEST_ASSERT_TRUE(tripped);
}

// R221-4: `Ok` needs a comparison reading inside the prescribed window. Trend
// window 10 s, maximum sample age 60 s, two readings 55 s apart: the older one
// is no valid baseline (older than the window), so nothing was judged.
void test_two_readings_further_apart_than_the_window_never_yield_an_ok() {
    ThermalSafetyLimits limits = testLimits();
    limits.trendWindowMillis = 10'000U;
    limits.maxSampleAgeMillis = 60'000U;
    limits.invalidLatchMillis = 120'000U;
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(limits) ==
                     ThermalSafetyConfigureResult::Accepted);
    auto result = supervisor.evaluate(samples(100'000U, 25.0, 30.0), 100'000U);
    TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Blocked);
    result = supervisor.evaluate(samples(155'000U, 25.0, 30.0), 155'000U);
    TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Blocked);
    TEST_ASSERT_TRUE(result.reason ==
                     ThermalSafetyReason::TrendObservationPending);
    // Readings that stay too sparse for the window never become Ok ...
    for (std::uint64_t now = 210'000U; now < 600'000U; now += 55'000U) {
        result = supervisor.evaluate(samples(now, 25.0, 30.0), now);
        TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Blocked);
        TEST_ASSERT_TRUE(result.reason ==
                         ThermalSafetyReason::TrendObservationPending);
    }
    // ... while regular readings provide a baseline within the window.
    std::uint64_t now = 600'000U;
    bool ok = false;
    for (int i = 0; i < 10 && !ok; ++i, now += 2'000U) {
        ok = supervisor.evaluate(samples(now, 25.0, 30.0), now).status ==
             ThermalSafetyStatus::Ok;
    }
    TEST_ASSERT_TRUE(ok);
}

// Backwards or out-of-order measurement timestamps are fail-closed: the
// observation restarts and no `Ok` is given until it is rebuilt.
void test_backwards_or_unordered_timestamps_are_fail_closed() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    const auto ok = primeToOk(supervisor, 10'000U);
    TEST_ASSERT_TRUE(supervisor.evaluate(samples(ok, 25.0, 30.0), ok).status ==
                     ThermalSafetyStatus::Ok);
    // The next reading carries an older timestamp than the newest recorded one.
    auto result = supervisor.evaluate(samples(ok - 2'000U, 25.0, 30.0), ok);
    TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Blocked);
    TEST_ASSERT_TRUE(result.reason ==
                     ThermalSafetyReason::TrendObservationPending);
    // Out-of-order arrival: 2 s, 4 s, 3 s later; the late one resets again.
    ThermalSafetySupervisor other;
    TEST_ASSERT_TRUE(other.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    const auto start = primeToOk(other, 10'000U);
    TEST_ASSERT_TRUE(
        other.evaluate(samples(start + 2'000U, 25.0, 30.0), start + 2'000U)
            .status == ThermalSafetyStatus::Ok);
    TEST_ASSERT_TRUE(
        other.evaluate(samples(start + 4'000U, 25.0, 30.0), start + 4'000U)
            .status == ThermalSafetyStatus::Ok);
    result =
        other.evaluate(samples(start + 3'000U, 25.0, 30.0), start + 4'000U);
    TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Blocked);
    TEST_ASSERT_TRUE(result.reason ==
                     ThermalSafetyReason::TrendObservationPending);
    // It recovers only after a fresh, sufficient observation.
    std::uint64_t now = start + 6'000U;
    bool recovered = false;
    for (int i = 0; i < 40 && !recovered; ++i, now += 2'000U) {
        recovered = other.evaluate(samples(now, 25.0, 30.0), now).status ==
                    ThermalSafetyStatus::Ok;
    }
    TEST_ASSERT_TRUE(recovered);
}

// Memory stays small: no per-sample history.
void test_the_supervisor_keeps_a_small_fixed_history() {
    TEST_ASSERT_TRUE(sizeof(ThermalSafetySupervisor) < 1'600U);
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
        TEST_ASSERT_TRUE(result.status != ThermalSafetyStatus::Tripped);
        if (now - 10'000U >= 30'000U) {
            TEST_ASSERT_TRUE(result.status == ThermalSafetyStatus::Ok);
        }
        now += 2'000U;
        cooling += 2.0 / 30.0;  // +2 K/min < 6 K/min
    }
}

void test_evaluation_is_idempotent_for_identical_input() {
    ThermalSafetySupervisor supervisor;
    TEST_ASSERT_TRUE(supervisor.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    const auto ok = primeToOk(supervisor, 10'000U);
    for (int i = 0; i < 200; ++i) {
        TEST_ASSERT_TRUE(
            supervisor.evaluate(samples(ok, 25.0, 30.0), ok).status ==
            ThermalSafetyStatus::Ok);
    }
    // Repeating a pending evaluation never advances the observation.
    ThermalSafetySupervisor pending;
    TEST_ASSERT_TRUE(pending.configure(testLimits()) ==
                     ThermalSafetyConfigureResult::Accepted);
    for (int i = 0; i < 200; ++i) {
        TEST_ASSERT_TRUE(
            pending.evaluate(samples(10'000U, 25.0, 30.0), 10'000U).status ==
            ThermalSafetyStatus::Blocked);
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
    TEST_ASSERT_EQUAL_STRING(
        "trend_observation_pending",
        thermalSafetyReasonName(ThermalSafetyReason::TrendObservationPending));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_unconfigured_is_the_default_and_denies);
    RUN_TEST(test_limit_validation_rejects_every_malformed_set);
    RUN_TEST(test_blocked_until_enough_observation_then_ok);
    RUN_TEST(test_a_gap_breaks_the_observation_and_never_yields_an_ok);
    RUN_TEST(test_every_limit_level_trips_and_latches);
    RUN_TEST(test_emergency_level_wins_over_intervention);
    RUN_TEST(test_invalid_readings_block_and_latch_after_the_configured_time);
    RUN_TEST(test_recovered_readings_clear_the_invalid_timer_but_not_a_trip);
    RUN_TEST(test_each_invalid_kind_is_blocked_with_its_reason);
    RUN_TEST(test_a_limit_excursion_trips_even_if_the_other_sensor_is_invalid);
    RUN_TEST(test_sustained_rise_trips_but_a_short_step_does_not);
    RUN_TEST(test_a_slow_rise_never_trips);
    RUN_TEST(
        test_the_longest_window_trips_on_a_dangerous_rise_at_two_second_samples);
    RUN_TEST(test_the_longest_window_stays_ok_for_a_slow_warming);
    RUN_TEST(test_irregular_sample_intervals_still_trip_on_a_dangerous_rise);
    RUN_TEST(test_a_late_fast_rise_is_caught_against_a_recent_baseline);
    RUN_TEST(test_two_readings_further_apart_than_the_window_never_yield_an_ok);
    RUN_TEST(test_backwards_or_unordered_timestamps_are_fail_closed);
    RUN_TEST(test_the_supervisor_keeps_a_small_fixed_history);
    RUN_TEST(test_evaluation_is_idempotent_for_identical_input);
    RUN_TEST(test_latch_refuses_configuration_and_survives_clearing);
    RUN_TEST(test_clearing_the_configuration_returns_to_unconfigured);
    RUN_TEST(test_names_are_stable);
    return UNITY_END();
}
