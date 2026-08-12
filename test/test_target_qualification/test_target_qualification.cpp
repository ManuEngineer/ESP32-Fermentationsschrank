#include <unity.h>

#include <limits>

#include "program_limits.hpp"
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

TargetQualificationResult evaluateAndApply(
    TargetQualificationEvaluator& evaluator,
    const TargetQualificationInput& qualificationInput) {
    const auto result = evaluator.evaluate(qualificationInput);
    static_cast<void>(evaluator.apply(
        result, TargetQualificationApplyStatus::AppliedRamOnly));
    return result;
}

void test_first_in_band_sample_starts_empty_episode() {
    TargetQualificationEvaluator evaluator;
    const auto result = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 100U, 20.0));
    TEST_ASSERT_TRUE(result.progress == QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(0U, result.creditedInBandMillis);
}

void test_duration_completes_only_after_checked_following_time() {
    TargetQualificationEvaluator evaluator;
    static_cast<void>(evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 100U, 20.0)));
    const auto inBand = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 1'099U, 20.4));
    TEST_ASSERT_TRUE(inBand.progress == QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(999U, inBand.creditedInBandMillis);
    const auto complete = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 1'100U, 20.0));
    TEST_ASSERT_TRUE(complete.progress == QualificationProgress::Complete);
    TEST_ASSERT_EQUAL_UINT64(1'000U, complete.creditedInBandMillis);
}

void test_band_boundary_is_inclusive() {
    TargetQualificationEvaluator evaluator;
    const auto result = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 100U, 20.5));
    TEST_ASSERT_TRUE(result.progress == QualificationProgress::InBand);
}

void test_unavailable_and_invalid_interrupt_episode_differently() {
    TargetQualificationEvaluator evaluator;
    static_cast<void>(evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 100U, 20.0)));
    auto unavailableInput = input(QualificationPhase::Target, 200U, 20.0);
    unavailableInput.product.quality = SensorQuality::Stale;
    const auto unavailable = evaluateAndApply(evaluator, unavailableInput);
    TEST_ASSERT_TRUE(unavailable.progress ==
                     QualificationProgress::Unavailable);
    TEST_ASSERT_EQUAL_UINT64(0U, unavailable.creditedInBandMillis);

    auto invalidInput = input(QualificationPhase::Target, 300U, 20.0);
    invalidInput.product.rawCelsius = std::numeric_limits<double>::quiet_NaN();
    const auto invalid = evaluateAndApply(evaluator, invalidInput);
    TEST_ASSERT_TRUE(invalid.progress == QualificationProgress::Invalid);
}

void test_outside_starts_grace_and_direct_return_does_not_credit_time() {
    TargetQualificationEvaluator evaluator;
    static_cast<void>(evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 100U, 20.0)));
    const auto grace = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 200U, 21.0));
    TEST_ASSERT_TRUE(grace.progress == QualificationProgress::Grace);

    const auto returned = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 400U, 20.0));
    TEST_ASSERT_TRUE(returned.progress == QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(0U, returned.creditedInBandMillis);
}

void test_grace_equality_expires_old_episode_before_return() {
    TargetQualificationEvaluator evaluator;
    static_cast<void>(evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 100U, 20.0)));
    static_cast<void>(evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 200U, 21.0)));
    const auto expired = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 700U, 20.0));
    TEST_ASSERT_TRUE(expired.progress == QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(0U, expired.creditedInBandMillis);
}

void test_gap_and_retrograde_time_reset_as_invalid() {
    TargetQualificationEvaluator evaluator;
    static_cast<void>(evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 100U, 20.0)));
    const auto retrograde = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 99U, 20.0));
    TEST_ASSERT_TRUE(retrograde.progress == QualificationProgress::Invalid);

    static_cast<void>(evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 200U, 20.0)));
    const auto gap = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 2'201U, 20.0));
    TEST_ASSERT_TRUE(gap.progress == QualificationProgress::Invalid);
}

void test_preheating_always_uses_air_and_phase_change_resets_credit() {
    TargetQualificationEvaluator evaluator;
    auto preheat = input(QualificationPhase::Preheating, 100U, 20.0);
    preheat.product = valid(40.0);
    const auto first = evaluateAndApply(evaluator, preheat);
    TEST_ASSERT_TRUE(first.progress == QualificationProgress::InBand);

    auto target = input(QualificationPhase::Target, 200U, 40.0);
    target.product = valid(20.0);
    const auto after = evaluateAndApply(evaluator, target);
    TEST_ASSERT_TRUE(after.progress == QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(0U, after.creditedInBandMillis);
}

void test_missing_effective_grace_or_gap_is_unavailable() {
    TargetQualificationEvaluator evaluator;
    auto missingGrace = input(QualificationPhase::Target, 100U, 20.0);
    missingGrace.effectiveGraceMillis = std::nullopt;
    TEST_ASSERT_TRUE(evaluateAndApply(evaluator, missingGrace).progress ==
                     QualificationProgress::Unavailable);

    auto missingGap = input(QualificationPhase::Target, 200U, 20.0);
    missingGap.maximumAcceptedSampleGapMillis = std::nullopt;
    TEST_ASSERT_TRUE(evaluateAndApply(evaluator, missingGap).progress ==
                     QualificationProgress::Unavailable);
}

void test_candidate_failures_leave_live_state_and_retry_does_not_double_credit() {
    TargetQualificationEvaluator evaluator;
    const auto first =
        evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0));
    TEST_ASSERT_TRUE(
        evaluator.apply(first, TargetQualificationApplyStatus::AppliedRamOnly));
    const auto before = evaluator.state();

    const auto candidate =
        evaluator.evaluate(input(QualificationPhase::Target, 1'100U, 20.0));
    TEST_ASSERT_EQUAL_UINT64(1'000U, candidate.creditedInBandMillis);
    TEST_ASSERT_FALSE(evaluator.apply(
        candidate, TargetQualificationApplyStatus::PersistenceFailed));
    TEST_ASSERT_TRUE(evaluator.state().creditedInBandMillis ==
                     before.creditedInBandMillis);
    TEST_ASSERT_TRUE(evaluator.state().lastUsableTimestampMillis ==
                     before.lastUsableTimestampMillis);

    const auto retry =
        evaluator.evaluate(input(QualificationPhase::Target, 1'100U, 20.0));
    TEST_ASSERT_EQUAL_UINT64(1'000U, retry.creditedInBandMillis);
    TEST_ASSERT_TRUE(evaluator.apply(
        retry, TargetQualificationApplyStatus::PersistedAndProcessApplied));
    TEST_ASSERT_EQUAL_UINT64(1'000U, evaluator.state().creditedInBandMillis);
}

void test_process_apply_failure_leaves_live_state_unchanged() {
    TargetQualificationEvaluator evaluator;
    const auto first =
        evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0));
    TEST_ASSERT_TRUE(
        evaluator.apply(first, TargetQualificationApplyStatus::AppliedRamOnly));
    const auto before = evaluator.state();
    const auto candidate =
        evaluator.evaluate(input(QualificationPhase::Target, 600U, 20.0));
    TEST_ASSERT_FALSE(evaluator.apply(
        candidate, TargetQualificationApplyStatus::ProcessApplyFailed));
    TEST_ASSERT_TRUE(evaluator.state().creditedInBandMillis ==
                     before.creditedInBandMillis);
    TEST_ASSERT_TRUE(evaluator.state().lastUsableTimestampMillis ==
                     before.lastUsableTimestampMillis);
}

void test_target_change_is_candidate_only_until_commit() {
    TargetQualificationEvaluator evaluator;
    const auto first =
        evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0));
    TEST_ASSERT_TRUE(
        evaluator.apply(first, TargetQualificationApplyStatus::AppliedRamOnly));

    auto changed = input(QualificationPhase::Target, 200U, 21.0);
    changed.targetCelsius = 21.0;
    const auto candidate = evaluator.evaluate(changed);
    TEST_ASSERT_FALSE(evaluator.apply(
        candidate, TargetQualificationApplyStatus::ProcessApplyFailed));
    TEST_ASSERT_TRUE(evaluator.state().context.has_value());
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 20.0,
                              evaluator.state().context->targetCelsius);
    TEST_ASSERT_TRUE(evaluator.apply(
        candidate, TargetQualificationApplyStatus::AppliedRamOnly));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 21.0,
                              evaluator.state().context->targetCelsius);
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);
}

void test_grace_direct_return_preserves_credit_only_before_expiry() {
    for (const auto returnTimestamp : {799U, 800U, 801U}) {
        TargetQualificationEvaluator evaluator;
        TEST_ASSERT_TRUE(evaluator.apply(
            evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0)),
            TargetQualificationApplyStatus::AppliedRamOnly));
        TEST_ASSERT_TRUE(evaluator.apply(
            evaluator.evaluate(input(QualificationPhase::Target, 200U, 20.0)),
            TargetQualificationApplyStatus::AppliedRamOnly));
        TEST_ASSERT_TRUE(evaluator.apply(
            evaluator.evaluate(input(QualificationPhase::Target, 300U, 21.0)),
            TargetQualificationApplyStatus::AppliedRamOnly));
        const auto returned = evaluator.evaluate(
            input(QualificationPhase::Target, returnTimestamp, 20.0));
        const auto expectedCredit = returnTimestamp < 800U ? 100U : 0U;
        TEST_ASSERT_EQUAL_UINT64(expectedCredit, returned.creditedInBandMillis);
        TEST_ASSERT_TRUE(returned.progress == QualificationProgress::InBand);
        TEST_ASSERT_TRUE(evaluator.apply(
            returned, TargetQualificationApplyStatus::AppliedRamOnly));
    }
}

void test_band_limits_are_inclusive_and_outside_is_invalid() {
    const double minimum = program_limits::kMinimumQualificationBandCelsius;
    const double maximum = program_limits::kMaximumQualificationBandCelsius;
    for (const auto band : {minimum, maximum}) {
        auto qualificationInput = input(QualificationPhase::Target, 100U, 20.0);
        qualificationInput.bandCelsius = band;
        TargetQualificationEvaluator evaluator;
        const auto result = evaluator.evaluate(qualificationInput);
        TEST_ASSERT_TRUE(result.progress == QualificationProgress::InBand);
    }
    for (const auto band : {minimum - 0.001, maximum + 0.001}) {
        auto qualificationInput = input(QualificationPhase::Target, 100U, 20.0);
        qualificationInput.bandCelsius = band;
        TargetQualificationEvaluator evaluator;
        TEST_ASSERT_TRUE(evaluator.evaluate(qualificationInput).progress ==
                         QualificationProgress::Invalid);
    }
}

void test_invalid_and_unavailable_qualifier_parameters_are_distinct() {
    TargetQualificationEvaluator evaluator;
    auto invalidDuration = input(QualificationPhase::Target, 100U, 20.0);
    invalidDuration.qualificationDurationMillis = 0U;
    TEST_ASSERT_TRUE(evaluator.evaluate(invalidDuration).progress ==
                     QualificationProgress::Invalid);

    auto nonfiniteTarget = input(QualificationPhase::Target, 100U, 20.0);
    nonfiniteTarget.targetCelsius = std::numeric_limits<double>::quiet_NaN();
    TEST_ASSERT_TRUE(evaluator.evaluate(nonfiniteTarget).progress ==
                     QualificationProgress::Invalid);

    auto nonfiniteBand = input(QualificationPhase::Target, 100U, 20.0);
    nonfiniteBand.bandCelsius = std::numeric_limits<double>::infinity();
    TEST_ASSERT_TRUE(evaluator.evaluate(nonfiniteBand).progress ==
                     QualificationProgress::Invalid);

    auto invalidRole = input(QualificationPhase::Target, 100U, 20.0);
    invalidRole.controlSensorRole = static_cast<ControlSensorRole>(0xFFU);
    TEST_ASSERT_TRUE(evaluator.evaluate(invalidRole).progress ==
                     QualificationProgress::Invalid);

    auto missingGrace = input(QualificationPhase::Target, 100U, 20.0);
    missingGrace.effectiveGraceMillis.reset();
    TEST_ASSERT_TRUE(evaluator.evaluate(missingGrace).progress ==
                     QualificationProgress::Unavailable);

    auto invalidGap = input(QualificationPhase::Target, 100U, 20.0);
    invalidGap.maximumAcceptedSampleGapMillis = 0U;
    TEST_ASSERT_TRUE(evaluator.evaluate(invalidGap).progress ==
                     QualificationProgress::Invalid);
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
    RUN_TEST(
        test_candidate_failures_leave_live_state_and_retry_does_not_double_credit);
    RUN_TEST(test_process_apply_failure_leaves_live_state_unchanged);
    RUN_TEST(test_target_change_is_candidate_only_until_commit);
    RUN_TEST(test_grace_direct_return_preserves_credit_only_before_expiry);
    RUN_TEST(test_band_limits_are_inclusive_and_outside_is_invalid);
    RUN_TEST(test_invalid_and_unavailable_qualifier_parameters_are_distinct);
    return UNITY_END();
}
