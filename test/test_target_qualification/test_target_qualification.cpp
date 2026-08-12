#include <unity.h>

#include <limits>

#include "target_qualification.hpp"

namespace {

using namespace fermentation;
using device_platform::SensorQuality;
using device_platform::SensorQualitySnapshot;

SensorQualitySnapshot valid(double celsius) {
    SensorQualitySnapshot snapshot;
    snapshot.quality = SensorQuality::Valid;
    snapshot.rawCelsius = celsius;
    return snapshot;
}

TargetQualificationInput input(QualificationPhase phase,
                               std::uint64_t timestamp, double measured) {
    TargetQualificationInput result;
    result.phase = phase;
    result.sampleTimestampMonotonicMillis = timestamp;
    result.targetCelsius = 20.0;
    result.bandCelsius = 0.5;
    result.qualificationDurationMillis = 1'000U;
    result.effectiveGraceMillis = 500U;
    result.maximumAcceptedSampleGapMillis = 2'000U;
    result.controlSensorRole = ControlSensorRole::Product;
    result.air = valid(measured);
    result.product = valid(measured);
    return result;
}

void test_first_in_band_sample_starts_empty_episode() {
    TargetQualificationEvaluator evaluator;
    const auto result =
        evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0));
    TEST_ASSERT_TRUE(result.progress == QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(0U, result.creditedInBandMillis);
}

void test_duration_completes_only_after_checked_following_time() {
    TargetQualificationEvaluator evaluator;
    static_cast<void>(
        evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0)));
    const auto inBand =
        evaluator.evaluate(input(QualificationPhase::Target, 1'099U, 20.4));
    TEST_ASSERT_TRUE(inBand.progress == QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(999U, inBand.creditedInBandMillis);
    const auto complete =
        evaluator.evaluate(input(QualificationPhase::Target, 1'100U, 20.0));
    TEST_ASSERT_TRUE(complete.progress == QualificationProgress::Complete);
    TEST_ASSERT_EQUAL_UINT64(1'000U, complete.creditedInBandMillis);
}

void test_band_boundary_is_inclusive() {
    TargetQualificationEvaluator evaluator;
    const auto result =
        evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.5));
    TEST_ASSERT_TRUE(result.progress == QualificationProgress::InBand);
}

void test_unavailable_and_invalid_interrupt_episode_differently() {
    TargetQualificationEvaluator evaluator;
    static_cast<void>(
        evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0)));
    auto unavailableInput = input(QualificationPhase::Target, 200U, 20.0);
    unavailableInput.product.quality = SensorQuality::Stale;
    const auto unavailable = evaluator.evaluate(unavailableInput);
    TEST_ASSERT_TRUE(unavailable.progress ==
                     QualificationProgress::Unavailable);
    TEST_ASSERT_EQUAL_UINT64(0U, unavailable.creditedInBandMillis);

    auto invalidInput = input(QualificationPhase::Target, 300U, 20.0);
    invalidInput.product.rawCelsius = std::numeric_limits<double>::quiet_NaN();
    const auto invalid = evaluator.evaluate(invalidInput);
    TEST_ASSERT_TRUE(invalid.progress == QualificationProgress::Invalid);
}

void test_outside_starts_grace_and_direct_return_does_not_credit_time() {
    TargetQualificationEvaluator evaluator;
    static_cast<void>(
        evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0)));
    const auto grace =
        evaluator.evaluate(input(QualificationPhase::Target, 200U, 21.0));
    TEST_ASSERT_TRUE(grace.progress == QualificationProgress::Grace);

    const auto returned =
        evaluator.evaluate(input(QualificationPhase::Target, 400U, 20.0));
    TEST_ASSERT_TRUE(returned.progress == QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(0U, returned.creditedInBandMillis);
}

void test_grace_equality_expires_old_episode_before_return() {
    TargetQualificationEvaluator evaluator;
    static_cast<void>(
        evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0)));
    static_cast<void>(
        evaluator.evaluate(input(QualificationPhase::Target, 200U, 21.0)));
    const auto expired =
        evaluator.evaluate(input(QualificationPhase::Target, 700U, 20.0));
    TEST_ASSERT_TRUE(expired.progress == QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(0U, expired.creditedInBandMillis);
}

void test_gap_and_retrograde_time_reset_as_invalid() {
    TargetQualificationEvaluator evaluator;
    static_cast<void>(
        evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0)));
    const auto retrograde =
        evaluator.evaluate(input(QualificationPhase::Target, 99U, 20.0));
    TEST_ASSERT_TRUE(retrograde.progress == QualificationProgress::Invalid);

    static_cast<void>(
        evaluator.evaluate(input(QualificationPhase::Target, 200U, 20.0)));
    const auto gap =
        evaluator.evaluate(input(QualificationPhase::Target, 2'201U, 20.0));
    TEST_ASSERT_TRUE(gap.progress == QualificationProgress::Invalid);
}

void test_preheating_always_uses_air_and_phase_change_resets_credit() {
    TargetQualificationEvaluator evaluator;
    auto preheat = input(QualificationPhase::Preheating, 100U, 20.0);
    preheat.product = valid(40.0);
    const auto first = evaluator.evaluate(preheat);
    TEST_ASSERT_TRUE(first.progress == QualificationProgress::InBand);

    auto target = input(QualificationPhase::Target, 200U, 40.0);
    target.product = valid(20.0);
    const auto after = evaluator.evaluate(target);
    TEST_ASSERT_TRUE(after.progress == QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(0U, after.creditedInBandMillis);
}

void test_missing_effective_grace_or_gap_is_unavailable() {
    TargetQualificationEvaluator evaluator;
    auto missingGrace = input(QualificationPhase::Target, 100U, 20.0);
    missingGrace.effectiveGraceMillis = std::nullopt;
    TEST_ASSERT_TRUE(evaluator.evaluate(missingGrace).progress ==
                     QualificationProgress::Unavailable);

    auto missingGap = input(QualificationPhase::Target, 200U, 20.0);
    missingGap.maximumAcceptedSampleGapMillis = std::nullopt;
    TEST_ASSERT_TRUE(evaluator.evaluate(missingGap).progress ==
                     QualificationProgress::Unavailable);
}

}  // namespace

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    static_cast<void>(argc);
    static_cast<void>(argv);
    UNITY_BEGIN();
    RUN_TEST(test_first_in_band_sample_starts_empty_episode);
    RUN_TEST(test_duration_completes_only_after_checked_following_time);
    RUN_TEST(test_band_boundary_is_inclusive);
    RUN_TEST(test_unavailable_and_invalid_interrupt_episode_differently);
    RUN_TEST(test_outside_starts_grace_and_direct_return_does_not_credit_time);
    RUN_TEST(test_grace_equality_expires_old_episode_before_return);
    RUN_TEST(test_gap_and_retrograde_time_reset_as_invalid);
    RUN_TEST(test_preheating_always_uses_air_and_phase_change_resets_credit);
    RUN_TEST(test_missing_effective_grace_or_gap_is_unavailable);
    return UNITY_END();
}
