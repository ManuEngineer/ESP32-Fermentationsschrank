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
    // Per #20, only filteredCelsius is a normal control value; rawCelsius is
    // set alongside as harmless diagnostic evidence, never as the actual
    // read value.
    snapshot.filteredCelsius = celsius;
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
    auto result = evaluator.evaluate(qualificationInput);
    const TargetQualificationCommitContext context{
        {qualificationInput.targetCelsius, qualificationInput.bandCelsius,
         qualificationInput.controlSensorRole},
        qualificationInput.runRevision,
        qualificationInput.processTransitionSequence};
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(result, context));
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
    invalidInput.product.filteredCelsius =
        std::numeric_limits<double>::quiet_NaN();
    const auto invalid = evaluateAndApply(evaluator, invalidInput);
    TEST_ASSERT_TRUE(invalid.progress == QualificationProgress::Invalid);
}

void test_unavailable_and_invalid_return_start_new_credit() {
    TargetQualificationEvaluator evaluator;
    static_cast<void>(evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 100U, 20.0)));

    auto unavailableInput = input(QualificationPhase::Target, 200U, 20.0);
    unavailableInput.product.quality = SensorQuality::Stale;
    static_cast<void>(evaluateAndApply(evaluator, unavailableInput));
    const auto afterUnavailable = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 300U, 20.0));
    TEST_ASSERT_TRUE(afterUnavailable.progress ==
                     QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(0U, afterUnavailable.creditedInBandMillis);

    auto invalidInput = input(QualificationPhase::Target, 400U, 20.0);
    invalidInput.product.filteredCelsius =
        std::numeric_limits<double>::quiet_NaN();
    static_cast<void>(evaluateAndApply(evaluator, invalidInput));
    const auto afterInvalid = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 500U, 20.0));
    TEST_ASSERT_TRUE(afterInvalid.progress == QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(0U, afterInvalid.creditedInBandMillis);
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

void test_all_six_progress_values_have_direct_evaluator_oracles() {
    TargetQualificationEvaluator unavailableEvaluator;
    auto unavailableInput = input(QualificationPhase::Target, 100U, 20.0);
    unavailableInput.product.quality = SensorQuality::Stale;
    TEST_ASSERT_TRUE(unavailableEvaluator.evaluate(unavailableInput).progress ==
                     QualificationProgress::Unavailable);

    TargetQualificationEvaluator invalidEvaluator;
    auto invalidInput = input(QualificationPhase::Target, 100U, 20.0);
    invalidInput.product.filteredCelsius =
        std::numeric_limits<double>::quiet_NaN();
    TEST_ASSERT_TRUE(invalidEvaluator.evaluate(invalidInput).progress ==
                     QualificationProgress::Invalid);

    TargetQualificationEvaluator outsideEvaluator;
    TEST_ASSERT_TRUE(
        outsideEvaluator.evaluate(input(QualificationPhase::Target, 100U, 21.0))
            .progress == QualificationProgress::OutsideBand);

    TargetQualificationEvaluator graceEvaluator;
    static_cast<void>(evaluateAndApply(
        graceEvaluator, input(QualificationPhase::Target, 100U, 20.0)));
    TEST_ASSERT_TRUE(
        evaluateAndApply(graceEvaluator,
                         input(QualificationPhase::Target, 200U, 21.0))
            .progress == QualificationProgress::Grace);

    TargetQualificationEvaluator inBandEvaluator;
    TEST_ASSERT_TRUE(
        inBandEvaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0))
            .progress == QualificationProgress::InBand);

    TargetQualificationEvaluator completeEvaluator;
    static_cast<void>(evaluateAndApply(
        completeEvaluator, input(QualificationPhase::Target, 100U, 20.0)));
    TEST_ASSERT_TRUE(
        evaluateAndApply(completeEvaluator,
                         input(QualificationPhase::Target, 1'100U, 20.0))
            .progress == QualificationProgress::Complete);
}

void test_outside_band_resets_an_episode_when_grace_cannot_continue() {
    TargetQualificationEvaluator evaluator;
    static_cast<void>(evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 100U, 20.0)));
    auto outsideInput = input(QualificationPhase::Target, 200U, 21.0);
    outsideInput.effectiveGraceMillis = 0U;
    const auto outside = evaluateAndApply(evaluator, outsideInput);
    TEST_ASSERT_TRUE(outside.progress == QualificationProgress::OutsideBand);
    TEST_ASSERT_EQUAL_UINT64(0U, outside.creditedInBandMillis);
    TEST_ASSERT_FALSE(evaluator.state().episodeActive);
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

    const auto afterRetrograde = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 200U, 20.0));
    TEST_ASSERT_EQUAL_UINT64(0U, afterRetrograde.creditedInBandMillis);
    const auto gap = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 2'201U, 20.0));
    TEST_ASSERT_TRUE(gap.progress == QualificationProgress::Invalid);
    const auto afterGap = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 2'300U, 20.0));
    TEST_ASSERT_EQUAL_UINT64(0U, afterGap.creditedInBandMillis);
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
    TEST_ASSERT_TRUE(evaluator.evaluate(missingGrace).progress ==
                     QualificationProgress::Unavailable);

    auto missingGap = input(QualificationPhase::Target, 200U, 20.0);
    missingGap.maximumAcceptedSampleGapMillis = std::nullopt;
    TEST_ASSERT_TRUE(evaluator.evaluate(missingGap).progress ==
                     QualificationProgress::Unavailable);
}

void test_candidate_failures_leave_live_state_and_retry_does_not_double_credit() {
    TargetQualificationEvaluator evaluator;
    const auto first =
        evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0));
    auto firstDecision = first;
    const auto firstInput = input(QualificationPhase::Target, 100U, 20.0);
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        firstDecision, {{firstInput.targetCelsius, firstInput.bandCelsius,
                         firstInput.controlSensorRole},
                        firstInput.runRevision,
                        firstInput.processTransitionSequence}));
    const auto before = evaluator.state();

    auto candidate =
        evaluator.evaluate(input(QualificationPhase::Target, 1'100U, 20.0));
    TEST_ASSERT_EQUAL_UINT64(1'000U, candidate.creditedInBandMillis);
    evaluator.discard(candidate);
    TEST_ASSERT_FALSE(evaluator.applyRamOnly(
        candidate, {{20.0, 0.5, ControlSensorRole::Product}, 0U, 0U}));
    TEST_ASSERT_TRUE(evaluator.state().creditedInBandMillis ==
                     before.creditedInBandMillis);
    TEST_ASSERT_TRUE(evaluator.state().lastUsableTimestampMillis ==
                     before.lastUsableTimestampMillis);

    const auto retry =
        evaluator.evaluate(input(QualificationPhase::Target, 1'100U, 20.0));
    TEST_ASSERT_EQUAL_UINT64(1'000U, retry.creditedInBandMillis);
    auto retryDecision = retry;
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        retryDecision, {{20.0, 0.5, ControlSensorRole::Product}, 0U, 0U}));
    TEST_ASSERT_EQUAL_UINT64(1'000U, evaluator.state().creditedInBandMillis);
}

void test_process_apply_failure_leaves_live_state_unchanged() {
    TargetQualificationEvaluator evaluator;
    const auto first =
        evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0));
    auto firstDecision = first;
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        firstDecision, {{20.0, 0.5, ControlSensorRole::Product}, 0U, 0U}));
    const auto before = evaluator.state();
    auto candidate =
        evaluator.evaluate(input(QualificationPhase::Target, 600U, 20.0));
    auto committedElsewhere =
        evaluator.evaluate(input(QualificationPhase::Target, 600U, 20.0));
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        committedElsewhere, {{20.0, 0.5, ControlSensorRole::Product}, 0U, 0U}));
    TEST_ASSERT_FALSE(evaluator.applyAfterPersistedProcessApply(
        candidate, {{20.0, 0.5, ControlSensorRole::Product}, 0U, 0U},
        {{20.0, 0.5, ControlSensorRole::Product}, 0U, 0U}));
    TEST_ASSERT_TRUE(evaluator.state().creditedInBandMillis >
                     before.creditedInBandMillis);
    TEST_ASSERT_TRUE(evaluator.state().lastUsableTimestampMillis == 600U);
}

void test_target_change_is_candidate_only_until_commit() {
    TargetQualificationEvaluator evaluator;
    const auto first =
        evaluator.evaluate(input(QualificationPhase::Target, 100U, 20.0));
    auto firstDecision = first;
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        firstDecision, {{20.0, 0.5, ControlSensorRole::Product}, 0U, 0U}));

    auto changed = input(QualificationPhase::Target, 200U, 21.0);
    changed.targetCelsius = 21.0;
    auto candidate = evaluator.evaluate(changed);
    evaluator.discard(candidate);
    TEST_ASSERT_FALSE(evaluator.applyRamOnly(
        candidate, {{20.0, 0.5, ControlSensorRole::Product}, 0U, 0U}));
    TEST_ASSERT_TRUE(evaluator.state().context.has_value());
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 20.0,
                              evaluator.state().context->targetCelsius);
    const auto retry = evaluator.evaluate(changed);
    auto retryDecision = retry;
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        retryDecision, {{21.0, 0.5, ControlSensorRole::Product}, 0U, 0U}));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 21.0,
                              evaluator.state().context->targetCelsius);
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);
}

void test_grace_direct_return_preserves_credit_only_before_expiry() {
    for (const auto returnTimestamp : {799U, 800U, 801U}) {
        TargetQualificationEvaluator evaluator;
        static_cast<void>(evaluateAndApply(
            evaluator, input(QualificationPhase::Target, 100U, 20.0)));
        static_cast<void>(evaluateAndApply(
            evaluator, input(QualificationPhase::Target, 200U, 20.0)));
        static_cast<void>(evaluateAndApply(
            evaluator, input(QualificationPhase::Target, 300U, 21.0)));
        auto returned = evaluator.evaluate(
            input(QualificationPhase::Target, returnTimestamp, 20.0));
        const auto expectedCredit = returnTimestamp < 800U ? 100U : 0U;
        TEST_ASSERT_EQUAL_UINT64(expectedCredit, returned.creditedInBandMillis);
        TEST_ASSERT_TRUE(returned.progress == QualificationProgress::InBand);
        const auto returnedInput =
            input(QualificationPhase::Target, returnTimestamp, 20.0);
        TEST_ASSERT_TRUE(evaluator.applyRamOnly(
            returned, {{returnedInput.targetCelsius, returnedInput.bandCelsius,
                        returnedInput.controlSensorRole},
                       returnedInput.runRevision,
                       returnedInput.processTransitionSequence}));
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

void test_stale_run_context_rejects_old_candidate_without_resetting_episode() {
    TargetQualificationEvaluator evaluator;
    auto firstInput = input(QualificationPhase::Target, 100U, 20.0);
    firstInput.runRevision = 4U;
    firstInput.processTransitionSequence = 9U;
    auto first = evaluator.evaluate(firstInput);
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        first, {{20.0, 0.5, ControlSensorRole::Product}, 4U, 9U}));

    auto oldCandidateInput = firstInput;
    oldCandidateInput.sampleTimestampMonotonicMillis = 200U;
    auto oldCandidate = evaluator.evaluate(oldCandidateInput);

    auto changedRunInput = firstInput;
    changedRunInput.sampleTimestampMonotonicMillis = 200U;
    auto changedRun = evaluator.evaluate(changedRunInput);
    TEST_ASSERT_TRUE(evaluator.applyAfterPersistedProcessApply(
        changedRun, {{20.0, 0.5, ControlSensorRole::Product}, 4U, 9U},
        {{20.0, 0.5, ControlSensorRole::Product}, 5U, 10U}));
    TEST_ASSERT_EQUAL_UINT64(100U, evaluator.state().creditedInBandMillis);
    TEST_ASSERT_FALSE(evaluator.applyRamOnly(
        oldCandidate, {{20.0, 0.5, ControlSensorRole::Product}, 4U, 9U}));

    auto newRunSample = changedRunInput;
    newRunSample.sampleTimestampMonotonicMillis = 300U;
    newRunSample.runRevision = 5U;
    newRunSample.processTransitionSequence = 10U;
    auto newRunCandidate = evaluator.evaluate(newRunSample);
    TEST_ASSERT_EQUAL_UINT64(200U, newRunCandidate.creditedInBandMillis);
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        newRunCandidate, {{20.0, 0.5, ControlSensorRole::Product}, 5U, 10U}));
    TEST_ASSERT_EQUAL_UINT64(200U, evaluator.state().creditedInBandMillis);
}

void test_stale_process_identity_rejects_same_valued_candidate() {
    TargetQualificationEvaluator evaluator;
    auto firstInput = input(QualificationPhase::Target, 100U, 20.0);
    firstInput.runRevision = 2U;
    firstInput.processTransitionSequence = 10U;
    auto first = evaluator.evaluate(firstInput);
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        first, {{20.0, 0.5, ControlSensorRole::Product}, 2U, 10U}));

    auto candidateInput = firstInput;
    candidateInput.sampleTimestampMonotonicMillis = 200U;
    auto candidate = evaluator.evaluate(candidateInput);
    auto changed = evaluator.evaluate(candidateInput);
    TEST_ASSERT_TRUE(evaluator.applyAfterPersistedProcessApply(
        changed, {{20.0, 0.5, ControlSensorRole::Product}, 2U, 10U},
        {{20.0, 0.5, ControlSensorRole::Product}, 2U, 11U}));
    TEST_ASSERT_FALSE(evaluator.applyRamOnly(
        candidate, {{20.0, 0.5, ControlSensorRole::Product}, 2U, 10U}));
    TEST_ASSERT_EQUAL_UINT64(100U, evaluator.state().creditedInBandMillis);
}

void test_future_candidate_identity_is_stale_against_old_expected_context() {
    TargetQualificationEvaluator evaluator;
    auto beforeInput = input(QualificationPhase::Target, 100U, 20.0);
    beforeInput.runRevision = 4U;
    beforeInput.processTransitionSequence = 9U;
    auto first = evaluator.evaluate(beforeInput);
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        first, {{20.0, 0.5, ControlSensorRole::Product}, 4U, 9U}));

    auto futureInput = beforeInput;
    futureInput.sampleTimestampMonotonicMillis = 200U;
    futureInput.runRevision = 5U;
    futureInput.processTransitionSequence = 10U;
    auto futureCandidate = evaluator.evaluate(futureInput);
    TEST_ASSERT_FALSE(evaluator.applyAfterPersistedProcessApply(
        futureCandidate, {{20.0, 0.5, ControlSensorRole::Product}, 4U, 9U},
        {{20.0, 0.5, ControlSensorRole::Product}, 5U, 10U}));
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);
}

void test_invalid_qualification_phase_is_a_non_mutating_invalid_candidate() {
    TargetQualificationEvaluator evaluator;
    auto validInput = input(QualificationPhase::Target, 100U, 20.0);
    auto validResult = evaluator.evaluate(validInput);
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        validResult, {{20.0, 0.5, ControlSensorRole::Product}, 0U, 0U}));
    const auto before = evaluator.state();

    auto invalidInput = validInput;
    invalidInput.phase = static_cast<QualificationPhase>(0xFFU);
    auto invalid = evaluator.evaluate(invalidInput);
    TEST_ASSERT_TRUE(invalid.progress == QualificationProgress::Invalid);
    TEST_ASSERT_FALSE(evaluator.applyRamOnly(
        invalid, {{20.0, 0.5, ControlSensorRole::Product}, 0U, 0U}));
    evaluator.discard(invalid);
    TEST_ASSERT_TRUE(evaluator.state().creditedInBandMillis ==
                     before.creditedInBandMillis);
    TEST_ASSERT_TRUE(evaluator.state().phase == before.phase);
}

void test_valid_quality_without_filtered_value_is_invalid_not_raw_fallback() {
    // FR2: SensorQuality::Valid with a missing filteredCelsius is structurally
    // inconsistent evidence, even if rawCelsius/correctedCelsius are present.
    // It must never be used as a normal qualification-value fallback.
    TargetQualificationEvaluator evaluator;
    auto rawOnly = input(QualificationPhase::Target, 100U, 20.0);
    rawOnly.product.filteredCelsius = std::nullopt;
    rawOnly.product.rawCelsius = 20.0;
    TEST_ASSERT_TRUE(evaluator.evaluate(rawOnly).progress ==
                     QualificationProgress::Invalid);

    auto correctedOnly = input(QualificationPhase::Target, 100U, 20.0);
    correctedOnly.product.filteredCelsius = std::nullopt;
    correctedOnly.product.rawCelsius = std::nullopt;
    correctedOnly.product.correctedCelsius = 20.0;
    TEST_ASSERT_TRUE(evaluator.evaluate(correctedOnly).progress ==
                     QualificationProgress::Invalid);
}

void test_stale_quality_with_old_filtered_value_is_unavailable() {
    // FR2: quality != Valid is always Unavailable, even if a previously
    // accepted filteredCelsius value is still carried on the snapshot.
    TargetQualificationEvaluator evaluator;
    auto staleInput = input(QualificationPhase::Target, 100U, 20.0);
    staleInput.product.quality = SensorQuality::Stale;
    TEST_ASSERT_TRUE(evaluator.evaluate(staleInput).progress ==
                     QualificationProgress::Unavailable);
}

void test_recovery_after_invalid_evidence_starts_a_clean_credit() {
    // FR2: after Unavailable/Invalid evidence, a later genuine
    // Valid+filteredCelsius sample must not carry over an old qualifier
    // credit.
    TargetQualificationEvaluator evaluator;
    static_cast<void>(evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 100U, 20.0)));
    static_cast<void>(evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 1'000U, 20.0)));

    auto missingFiltered = input(QualificationPhase::Target, 2'000U, 20.0);
    missingFiltered.product.filteredCelsius = std::nullopt;
    missingFiltered.product.rawCelsius = 20.0;
    static_cast<void>(evaluateAndApply(evaluator, missingFiltered));

    const auto recovered = evaluateAndApply(
        evaluator, input(QualificationPhase::Target, 2'100U, 20.0));
    TEST_ASSERT_TRUE(recovered.progress == QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(0U, recovered.creditedInBandMillis);
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
    RUN_TEST(test_unavailable_and_invalid_return_start_new_credit);
    RUN_TEST(test_outside_starts_grace_and_direct_return_does_not_credit_time);
    RUN_TEST(test_all_six_progress_values_have_direct_evaluator_oracles);
    RUN_TEST(test_outside_band_resets_an_episode_when_grace_cannot_continue);
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
    RUN_TEST(
        test_stale_run_context_rejects_old_candidate_without_resetting_episode);
    RUN_TEST(test_stale_process_identity_rejects_same_valued_candidate);
    RUN_TEST(
        test_future_candidate_identity_is_stale_against_old_expected_context);
    RUN_TEST(
        test_invalid_qualification_phase_is_a_non_mutating_invalid_candidate);
    RUN_TEST(
        test_valid_quality_without_filtered_value_is_invalid_not_raw_fallback);
    RUN_TEST(test_stale_quality_with_old_filtered_value_is_unavailable);
    RUN_TEST(test_recovery_after_invalid_evidence_starts_a_clean_credit);
    return UNITY_END();
}
