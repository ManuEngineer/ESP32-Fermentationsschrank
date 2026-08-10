#include <unity.h>

#include <limits>

#include "run_recovery_time.hpp"

namespace {

using namespace fermentation;

RecoveryOutageBoundsInput outageInput(
    std::optional<std::int64_t> checkpoint, std::optional<std::int64_t> restart,
    std::optional<std::uint32_t> maximumGap = std::nullopt) {
    RecoveryOutageBoundsInput input;
    input.utcAtLastCheckpoint = checkpoint;
    input.utcAtRestartBoundary = restart;
    input.maxCheckpointGapSeconds = maximumGap;
    return input;
}

void test_outage_bounds_require_both_utc_anchors() {
    TEST_ASSERT_FALSE(
        computeRecoveryOutageBounds(outageInput(std::nullopt, 110U))
            .has_value());
    TEST_ASSERT_FALSE(
        computeRecoveryOutageBounds(outageInput(100U, std::nullopt))
            .has_value());
    TEST_ASSERT_FALSE(
        computeRecoveryOutageBounds(outageInput(110U, 100U)).has_value());
}

void test_outage_bounds_keep_unknown_gap_conservative() {
    const auto bounds = computeRecoveryOutageBounds(outageInput(100U, 1000U));
    TEST_ASSERT_TRUE(bounds.has_value());
    TEST_ASSERT_EQUAL_UINT64(900U, bounds->outageSecondsUpperBound);
    TEST_ASSERT_EQUAL_UINT64(0U, bounds->outageSecondsLowerBound);
}

void test_outage_bounds_apply_only_the_explicit_gap() {
    const auto exact =
        computeRecoveryOutageBounds(outageInput(100U, 1000U, 900U));
    TEST_ASSERT_TRUE(exact.has_value());
    TEST_ASSERT_EQUAL_UINT64(0U, exact->outageSecondsLowerBound);

    const auto wider =
        computeRecoveryOutageBounds(outageInput(100U, 1000U, 1200U));
    TEST_ASSERT_TRUE(wider.has_value());
    TEST_ASSERT_EQUAL_UINT64(0U, wider->outageSecondsLowerBound);

    const auto interval =
        computeRecoveryOutageBounds(outageInput(100U, 1000U, 300U));
    TEST_ASSERT_TRUE(interval.has_value());
    TEST_ASSERT_EQUAL_UINT64(600U, interval->outageSecondsLowerBound);
}

void test_outage_bounds_support_the_full_signed_utc_difference() {
    const auto bounds = computeRecoveryOutageBounds(
        outageInput(std::numeric_limits<std::int64_t>::min(),
                    std::numeric_limits<std::int64_t>::max()));
    TEST_ASSERT_TRUE(bounds.has_value());
    TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<std::uint64_t>::max(),
                             bounds->outageSecondsUpperBound);
}

void test_recovered_elapsed_keeps_unknown_upper_bound_distinct_from_error() {
    const auto elapsed = computeRecoveredPhaseElapsed(
        RecoveredPhaseElapsedInput{50U, std::nullopt});
    TEST_ASSERT_TRUE(elapsed.has_value());
    TEST_ASSERT_EQUAL_UINT64(50U, elapsed->knownSecondsBeforeCheckpoint);
    TEST_ASSERT_EQUAL_UINT64(50U, elapsed->totalSecondsLowerBound);
    TEST_ASSERT_FALSE(elapsed->totalSecondsUpperBound.has_value());
}

void test_recovered_elapsed_adds_checked_bounds() {
    const auto elapsed = computeRecoveredPhaseElapsed(
        RecoveredPhaseElapsedInput{50U, RecoveryOutageBounds{100U, 20U}});
    TEST_ASSERT_TRUE(elapsed.has_value());
    TEST_ASSERT_EQUAL_UINT64(70U, elapsed->totalSecondsLowerBound);
    TEST_ASSERT_TRUE(elapsed->totalSecondsUpperBound.has_value());
    TEST_ASSERT_EQUAL_UINT64(150U, *elapsed->totalSecondsUpperBound);
}

void test_recovered_elapsed_reports_addition_overflow_as_error() {
    const auto max = std::numeric_limits<std::uint64_t>::max();
    TEST_ASSERT_FALSE(
        computeRecoveredPhaseElapsed(
            RecoveredPhaseElapsedInput{max, RecoveryOutageBounds{1U, 0U}})
            .has_value());
    TEST_ASSERT_FALSE(
        computeRecoveredPhaseElapsed(
            RecoveredPhaseElapsedInput{max, RecoveryOutageBounds{0U, 1U}})
            .has_value());
}

void test_recovery_time_verdict_uses_lower_bound_before_upper_bound() {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RecoveryTimeVerdict::DefinitelyExpired),
        static_cast<int>(evaluateRecoveryTimeVerdict(
            RecoveredPhaseElapsed{100U, 100U, 100U}, 100U)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RecoveryTimeVerdict::DefinitelyStillValid),
        static_cast<int>(evaluateRecoveryTimeVerdict(
            RecoveredPhaseElapsed{10U, 10U, 99U}, 100U)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RecoveryTimeVerdict::Uncertain),
                          static_cast<int>(evaluateRecoveryTimeVerdict(
                              RecoveredPhaseElapsed{10U, 10U, 100U}, 100U)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RecoveryTimeVerdict::Uncertain),
        static_cast<int>(evaluateRecoveryTimeVerdict(
            RecoveredPhaseElapsed{10U, 10U, std::nullopt}, 100U)));
}

void test_recovery_confidence_is_derived_from_verdict_and_bounds() {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RecoveryConfidence::Strong),
                          static_cast<int>(deriveRecoveryConfidence(
                              RecoveryTimeVerdict::DefinitelyExpired, false)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RecoveryConfidence::Strong),
        static_cast<int>(deriveRecoveryConfidence(
            RecoveryTimeVerdict::DefinitelyStillValid, true)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RecoveryConfidence::Bounded),
                          static_cast<int>(deriveRecoveryConfidence(
                              RecoveryTimeVerdict::Uncertain, true)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RecoveryConfidence::Unknown),
                          static_cast<int>(deriveRecoveryConfidence(
                              RecoveryTimeVerdict::Uncertain, false)));
}

void test_utc_boot_anchor_is_boot_local_and_fail_closed() {
    TEST_ASSERT_FALSE(
        deriveUtcAtRecoveryBootAnchor(std::nullopt, 1000U, 0U).has_value());
    TEST_ASSERT_FALSE(deriveUtcAtRecoveryBootAnchor(-1, 1000U, 0U).has_value());
    TEST_ASSERT_FALSE(
        deriveUtcAtRecoveryBootAnchor(1000, 999U, 1000U).has_value());
    TEST_ASSERT_FALSE(deriveUtcAtRecoveryBootAnchor(1, 2000U, 0U).has_value());

    const auto derived = deriveUtcAtRecoveryBootAnchor(5000, 123456U, 23456U);
    TEST_ASSERT_TRUE(derived.has_value());
    TEST_ASSERT_EQUAL_INT64(4900, *derived);
}

void test_utc_boot_anchor_preserves_signed_boundary_without_wrap() {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto at_anchor = deriveUtcAtRecoveryBootAnchor(maximum, 0U, 0U);
    TEST_ASSERT_TRUE(at_anchor.has_value());
    TEST_ASSERT_EQUAL_INT64(maximum, *at_anchor);

    const auto after_boot = deriveUtcAtRecoveryBootAnchor(
        maximum, std::numeric_limits<std::uint64_t>::max(), 0U);
    TEST_ASSERT_TRUE(after_boot.has_value());
    TEST_ASSERT_EQUAL_INT64(
        maximum - static_cast<std::int64_t>(
                      std::numeric_limits<std::uint64_t>::max() / 1000U),
        *after_boot);
}

void test_effective_anchor_basis_folds_carry_forward_once() {
    PendingRecoveryAnchor anchor;
    anchor.originalCheckpointUtc = 1000;
    anchor.knownPhaseSecondsAtOriginalCheckpoint = 40U;
    anchor.knownSecondsSinceOriginalCheckpoint = 25U;

    const auto basis = deriveEffectiveAnchorTimeBasis(anchor);
    TEST_ASSERT_TRUE(basis.has_value());
    TEST_ASSERT_TRUE(basis->effectiveCheckpointUtc.has_value());
    TEST_ASSERT_EQUAL_INT64(1025, *basis->effectiveCheckpointUtc);
    TEST_ASSERT_EQUAL_UINT64(65U, basis->effectiveKnownSecondsBeforeCheckpoint);
}

void test_effective_anchor_basis_keeps_missing_utc_unresolved() {
    PendingRecoveryAnchor anchor;
    anchor.knownPhaseSecondsAtOriginalCheckpoint = 40U;
    anchor.knownSecondsSinceOriginalCheckpoint = 25U;

    const auto basis = deriveEffectiveAnchorTimeBasis(anchor);
    TEST_ASSERT_TRUE(basis.has_value());
    TEST_ASSERT_FALSE(basis->effectiveCheckpointUtc.has_value());
    TEST_ASSERT_EQUAL_UINT64(65U, basis->effectiveKnownSecondsBeforeCheckpoint);
}

void test_effective_anchor_basis_rejects_checked_overflow() {
    PendingRecoveryAnchor unsignedOverflow;
    unsignedOverflow.knownPhaseSecondsAtOriginalCheckpoint =
        std::numeric_limits<std::uint64_t>::max();
    unsignedOverflow.knownSecondsSinceOriginalCheckpoint = 1U;
    TEST_ASSERT_FALSE(
        deriveEffectiveAnchorTimeBasis(unsignedOverflow).has_value());

    PendingRecoveryAnchor signedOverflow;
    signedOverflow.originalCheckpointUtc =
        std::numeric_limits<std::int64_t>::max();
    signedOverflow.knownSecondsSinceOriginalCheckpoint = 1U;
    const auto basis = deriveEffectiveAnchorTimeBasis(signedOverflow);
    TEST_ASSERT_TRUE(basis.has_value());
    TEST_ASSERT_FALSE(basis->effectiveCheckpointUtc.has_value());
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_outage_bounds_require_both_utc_anchors);
    RUN_TEST(test_outage_bounds_keep_unknown_gap_conservative);
    RUN_TEST(test_outage_bounds_apply_only_the_explicit_gap);
    RUN_TEST(test_outage_bounds_support_the_full_signed_utc_difference);
    RUN_TEST(
        test_recovered_elapsed_keeps_unknown_upper_bound_distinct_from_error);
    RUN_TEST(test_recovered_elapsed_adds_checked_bounds);
    RUN_TEST(test_recovered_elapsed_reports_addition_overflow_as_error);
    RUN_TEST(test_recovery_time_verdict_uses_lower_bound_before_upper_bound);
    RUN_TEST(test_recovery_confidence_is_derived_from_verdict_and_bounds);
    RUN_TEST(test_utc_boot_anchor_is_boot_local_and_fail_closed);
    RUN_TEST(test_utc_boot_anchor_preserves_signed_boundary_without_wrap);
    RUN_TEST(test_effective_anchor_basis_folds_carry_forward_once);
    RUN_TEST(test_effective_anchor_basis_keeps_missing_utc_unresolved);
    RUN_TEST(test_effective_anchor_basis_rejects_checked_overflow);
    return UNITY_END();
}
