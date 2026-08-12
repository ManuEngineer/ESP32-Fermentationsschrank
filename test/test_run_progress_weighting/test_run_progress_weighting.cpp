#include <unity.h>

#include "run_progress_weighting.hpp"

namespace {

using namespace fermentation;
using device_platform::SensorQuality;

RoleTemperatureEvidence validTemperature(double value) {
    return {value, SensorQuality::Valid};
}

RecoveryProgressWeightingInput validProductInput() {
    RecoveryProgressWeightingInput input;
    input.phase = ProcessState::Fermenting;
    input.episodeEvidence.beforeOutage.product = validTemperature(21.0);
    input.episodeEvidence.firstAfterRestart.product = validTemperature(22.0);
    input.outage = RecoveryOutageBounds{100U, 50U};
    input.usableSensorRole = RunSensorMode::Product;
    return input;
}

void test_unavailable_provider_never_invents_progress() {
    const auto input = validProductInput();
    const UnavailableRecoveryProgressWeightingModel model;
    TEST_ASSERT_FALSE(model.evaluate(input).has_value());
}

void test_weighting_input_requires_complete_valid_selected_evidence() {
    auto input = validProductInput();
    TEST_ASSERT_TRUE(hasUsableRecoveryProgressEvidence(input));

    input.phase = ProcessState::CoolHolding;
    TEST_ASSERT_FALSE(hasUsableRecoveryProgressEvidence(input));
    input.phase = ProcessState::Fermenting;
    input.episodeEvidence.firstAfterRestart.product->quality =
        SensorQuality::Stale;
    TEST_ASSERT_FALSE(hasUsableRecoveryProgressEvidence(input));
    input.episodeEvidence.firstAfterRestart.product->quality =
        SensorQuality::Valid;
    input.episodeEvidence.firstAfterRestart.product->filteredCelsius.reset();
    TEST_ASSERT_FALSE(hasUsableRecoveryProgressEvidence(input));
    input.episodeEvidence.firstAfterRestart.product = std::nullopt;
    TEST_ASSERT_FALSE(hasUsableRecoveryProgressEvidence(input));
    input.episodeEvidence.firstAfterRestart.product = validTemperature(22.0);
    input.outage.reset();
    TEST_ASSERT_FALSE(hasUsableRecoveryProgressEvidence(input));
}

void test_weighting_contribution_keeps_role_bounds_and_revision_contract() {
    const auto input = validProductInput();
    const WeightedProgressContribution valid{
        WeightedProgressBounds{10U, 20U}, RunSensorMode::Product,
        WeightedProgressConfidence::ProductPreferred, 7U};
    TEST_ASSERT_TRUE(isValidWeightedProgressContribution(input, valid));

    auto air = valid;
    air.sourceRole = RunSensorMode::Air;
    TEST_ASSERT_FALSE(isValidWeightedProgressContribution(input, air));
    auto noRevision = valid;
    noRevision.modelRevision = 0U;
    TEST_ASSERT_FALSE(isValidWeightedProgressContribution(input, noRevision));
    auto noUpper = valid;
    noUpper.delta.upperBoundSeconds.reset();
    TEST_ASSERT_FALSE(isValidWeightedProgressContribution(input, noUpper));
    auto reversed = valid;
    reversed.delta.lowerBoundSeconds = 21U;
    TEST_ASSERT_FALSE(isValidWeightedProgressContribution(input, reversed));
}

void test_supersede_marks_only_unbooked_segment_as_partial_unknown() {
    RunProgressState progress;
    supersedeUnbookedWeightedSegment(progress, 9U);
    TEST_ASSERT_TRUE(progress.weightedProgress.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WeightedProgressCoverage::PartialUnknown),
        static_cast<int>(progress.weightedProgress->coverage));
    TEST_ASSERT_FALSE(
        progress.weightedProgress->cumulative.upperBoundSeconds.has_value());

    progress.weightedProgress->coverage = WeightedProgressCoverage::Complete;
    progress.weightedProgress->cumulative = WeightedProgressBounds{10U, 20U};
    progress.weightedProgress->lastApplied = WeightedProgressProvenance{
        RunSensorMode::Product, WeightedProgressConfidence::ProductPreferred,
        7U, 9U};
    supersedeUnbookedWeightedSegment(progress, 9U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WeightedProgressCoverage::Complete),
        static_cast<int>(progress.weightedProgress->coverage));
    TEST_ASSERT_EQUAL_UINT64(
        20U, *progress.weightedProgress->cumulative.upperBoundSeconds);

    supersedeUnbookedWeightedSegment(progress, 10U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WeightedProgressCoverage::PartialUnknown),
        static_cast<int>(progress.weightedProgress->coverage));
    TEST_ASSERT_FALSE(
        progress.weightedProgress->cumulative.upperBoundSeconds.has_value());
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_unavailable_provider_never_invents_progress);
    RUN_TEST(test_weighting_input_requires_complete_valid_selected_evidence);
    RUN_TEST(
        test_weighting_contribution_keeps_role_bounds_and_revision_contract);
    RUN_TEST(test_supersede_marks_only_unbooked_segment_as_partial_unknown);
    return UNITY_END();
}
