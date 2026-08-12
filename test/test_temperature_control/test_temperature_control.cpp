#include <unity.h>

#include <cmath>

#include "temperature_control.hpp"

namespace {

using namespace fermentation;
using device_platform::SensorQuality;
using device_platform::SensorQualitySnapshot;

SensorQualitySnapshot validSample(double celsius) {
    SensorQualitySnapshot snapshot;
    snapshot.quality = SensorQuality::Valid;
    snapshot.rawCelsius = celsius;
    return snapshot;
}

SensorQualitySnapshot unavailableSample(SensorQuality quality) {
    SensorQualitySnapshot snapshot;
    snapshot.quality = quality;
    return snapshot;
}

TemperatureControlParameters parameters() {
    TemperatureControlParameters result;
    result.airHeating = {0.10, 0.01, 1.0, 0.8};
    result.airCooling = {0.10, 0.01, 1.0, 0.8};
    result.productHeating = {0.20, 0.02, 1.0, 0.8};
    result.productCooling = {0.20, 0.02, 1.0, 0.8};
    result.maximumIntegrationStepMillis = 10'000U;
    result.airLimitLowerBlockCelsius = 5.0;
    result.airLimitLowerReduceStartCelsius = 10.0;
    result.airLimitUpperReduceStartCelsius = 30.0;
    result.airLimitUpperBlockCelsius = 35.0;
    return result;
}

IntegratorTransitionPolicy policy() {
    return {IntegratorTransitionAction::Reset, IntegratorTransitionAction::Reset,
            IntegratorTransitionAction::Reset, 0.2};
}

TemperatureControlInput airInput(std::uint64_t timestamp, double target,
                                 double measured) {
    TemperatureControlInput input;
    input.sampleTimestampMonotonicMillis = timestamp;
    input.targetCelsius = target;
    input.controlSensorRole = ControlSensorRole::Air;
    input.air = validSample(measured);
    return input;
}

TemperatureControlInput productInput(std::uint64_t timestamp, double target,
                                     double measured, double air) {
    auto input = airInput(timestamp, target, measured);
    input.controlSensorRole = ControlSensorRole::Product;
    input.air = validSample(air);
    input.product = validSample(measured);
    return input;
}

void test_parameters_require_all_four_profiles() {
    auto configured = parameters();
    TEST_ASSERT_TRUE(validateTemperatureControlParameters(configured));
    configured.productCooling.integralGainQuotePerCelsiusSecond = 0.0;
    TEST_ASSERT_FALSE(validateTemperatureControlParameters(configured));
}

void test_neutral_band_emits_identified_off_request() {
    TemperatureController controller(parameters(), policy());
    const auto result = controller.evaluate(airInput(100U, 20.0, 20.5));
    TEST_ASSERT_TRUE(result.status == TemperatureControlStatus::Off);
    TEST_ASSERT_TRUE(result.reason == TemperatureControlReason::NeutralBand);
    TEST_ASSERT_TRUE(result.controlRequest.has_value());
    TEST_ASSERT_TRUE(result.controlRequest->direction ==
                     AbstractControlDirection::Idle);
    TEST_ASSERT_EQUAL_UINT64(1U, result.controlRequest->identity.sequence);
    TEST_ASSERT_EQUAL_UINT64(100U,
                             result.controlRequest->identity.createdAtMonotonicMillis);
}

void test_pi_uses_active_error_and_integrates_only_after_first_sample() {
    TemperatureController controller(parameters(), policy());
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    TEST_ASSERT_TRUE(first.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.4, first.rawProportionalQuote);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              first.integralContributionQuote);

    auto secondInput = airInput(1100U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback =
        PreviousControlRequestFeedback{
            first.controlRequest->identity.sequence,
            PreviousControlRequestFeedback::Disposition::
                NoIntegratorConstraint};
    const auto second = controller.evaluate(secondInput);
    TEST_ASSERT_TRUE(second.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(second.integralContributionQuote > 0.0);
    TEST_ASSERT_TRUE(second.integralContributionQuote <= 0.4);
    TEST_ASSERT_TRUE(second.controlRequest->identity.sequence >
                     first.controlRequest->identity.sequence);
}

void test_feedback_disposition_freezes_integrator_and_missing_feedback_is_safe() {
    TemperatureController controller(parameters(), policy());
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));

    auto deferredInput = airInput(1100U, 25.0, 20.0);
    deferredInput.previousControlRequestFeedback =
        PreviousControlRequestFeedback{
            first.controlRequest->identity.sequence,
            PreviousControlRequestFeedback::Disposition::DeferredOrLimited};
    const auto deferred = controller.evaluate(deferredInput);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              deferred.integralContributionQuote);

    const auto missing = controller.evaluate(airInput(2100U, 25.0, 20.0));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              missing.integralContributionQuote);
}

void test_feedback_for_off_or_wrong_request_is_invalid() {
    TemperatureController controller(parameters(), policy());
    const auto off = controller.evaluate(airInput(100U, 20.0, 20.0));
    TEST_ASSERT_TRUE(off.controlRequest.has_value());
    auto next = airInput(200U, 25.0, 20.0);
    next.previousControlRequestFeedback =
        PreviousControlRequestFeedback{
            off.controlRequest->identity.sequence,
            PreviousControlRequestFeedback::Disposition::
                NoIntegratorConstraint};
    const auto result = controller.evaluate(next);
    TEST_ASSERT_TRUE(result.status == TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_FALSE(result.controlRequest.has_value());
}

void test_raw_p_above_maximum_is_checked_and_saturated_without_p_preclamp() {
    auto configured = parameters();
    configured.airHeating.proportionalGainQuotePerCelsius = 10.0;
    TemperatureController controller(configured, policy());
    const auto result = controller.evaluate(airInput(100U, 25.0, 0.0));
    TEST_ASSERT_TRUE(result.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(result.reason == TemperatureControlReason::Saturated);
    TEST_ASSERT_TRUE(result.rawProportionalQuote > 1.0);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.8, result.maximumLimitedQuote);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              result.integralContributionQuote);
}

void test_product_air_limit_reduces_and_blocks_normal_demand() {
    TemperatureController controller(parameters(), policy());
    const auto reduced =
        controller.evaluate(productInput(100U, 25.0, 20.0, 32.5));
    TEST_ASSERT_TRUE(reduced.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(reduced.reason == TemperatureControlReason::AirLimitReduced);
    TEST_ASSERT_TRUE(reduced.airLimitState == AirLimitState::Reduced);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.4, reduced.timeQuote);

    const auto blocked =
        controller.evaluate(productInput(200U, 25.0, 20.0, 35.0));
    TEST_ASSERT_TRUE(blocked.status == TemperatureControlStatus::Off);
    TEST_ASSERT_TRUE(blocked.reason == TemperatureControlReason::AirLimitBlocked);
    TEST_ASSERT_TRUE(blocked.airLimitState == AirLimitState::Blocked);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, blocked.timeQuote);
}

void test_air_limit_does_not_charge_integrator() {
    TemperatureController controller(parameters(), policy());
    const auto first =
        controller.evaluate(productInput(100U, 25.0, 20.0, 32.0));
    auto next = productInput(1100U, 25.0, 20.0, 32.0);
    next.previousControlRequestFeedback =
        PreviousControlRequestFeedback{
            first.controlRequest->identity.sequence,
            PreviousControlRequestFeedback::Disposition::
                NoIntegratorConstraint};
    const auto result = controller.evaluate(next);
    TEST_ASSERT_TRUE(result.airLimitState == AirLimitState::Reduced);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              result.integralContributionQuote);
}

void test_committed_transition_applies_once_and_idle_does_not_consume_it() {
    auto configured = parameters();
    TemperatureController controller(
        configured,
        {IntegratorTransitionAction::BoundedCarry,
         IntegratorTransitionAction::BoundedCarry,
         IntegratorTransitionAction::BoundedCarry, 0.1});
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    auto secondInput = airInput(1100U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback =
        PreviousControlRequestFeedback{
            first.controlRequest->identity.sequence,
            PreviousControlRequestFeedback::Disposition::
                NoIntegratorConstraint};
    const auto second = controller.evaluate(secondInput);
    TEST_ASSERT_TRUE(second.integralContributionQuote > 0.0);
    TEST_ASSERT_TRUE(controller.markCommittedControlContextTransitionPending(
        CommittedControlContextTransition::TargetContextChange));

    auto idleInput = airInput(1200U, 20.0, 20.0);
    const auto idle = controller.evaluate(idleInput);
    TEST_ASSERT_TRUE(idle.status == TemperatureControlStatus::Off);
    TEST_ASSERT_TRUE(controller.state().pendingContextTransition.has_value());

    auto activeInput = airInput(1300U, 25.0, 20.0);
    const auto active = controller.evaluate(activeInput);
    TEST_ASSERT_TRUE(active.controlRequest.has_value());
    TEST_ASSERT_FALSE(controller.state().pendingContextTransition.has_value());
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, second.integralContributionQuote,
                              active.integralContributionQuote);
}

void test_product_mode_requires_air_and_product_without_role_fallback() {
    TemperatureController controller(parameters(), policy());
    auto input = productInput(100U, 25.0, 20.0, 32.0);
    input.air = unavailableSample(SensorQuality::Stale);
    const auto unavailable = controller.evaluate(input);
    TEST_ASSERT_TRUE(unavailable.status == TemperatureControlStatus::Unavailable);
    TEST_ASSERT_TRUE(unavailable.reason ==
                     TemperatureControlReason::SensorUnavailable);
    TEST_ASSERT_TRUE(unavailable.airLimitState == AirLimitState::Unavailable);
    TEST_ASSERT_FALSE(unavailable.controlRequest.has_value());

    input = productInput(200U, 25.0, 20.0, 32.0);
    input.air.quality = SensorQuality::Valid;
    input.air.rawCelsius = NAN;
    const auto invalid = controller.evaluate(input);
    TEST_ASSERT_TRUE(invalid.status == TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_TRUE(invalid.reason == TemperatureControlReason::InvalidSample);
    TEST_ASSERT_TRUE(invalid.airLimitState == AirLimitState::Unavailable);
    TEST_ASSERT_FALSE(invalid.controlRequest.has_value());
}

void test_air_mode_does_not_require_product() {
    TemperatureController controller(parameters(), policy());
    auto input = airInput(100U, 25.0, 20.0);
    input.product = unavailableSample(SensorQuality::Failed);
    const auto result = controller.evaluate(input);
    TEST_ASSERT_TRUE(result.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(result.airLimitState == AirLimitState::NotApplied);
}

void test_large_gap_resets_and_uses_current_timestamp_as_new_anchor() {
    TemperatureController controller(parameters(), policy());
    static_cast<void>(controller.evaluate(airInput(100U, 25.0, 20.0)));
    const auto gap = controller.evaluate(airInput(20'100U, 25.0, 20.0));
    TEST_ASSERT_TRUE(gap.status == TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_TRUE(gap.reason == TemperatureControlReason::TimeInvalid);
    TEST_ASSERT_FALSE(gap.controlRequest.has_value());
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              controller.state().integralContributionQuote);

    const auto next = controller.evaluate(airInput(21'100U, 25.0, 20.0));
    TEST_ASSERT_TRUE(next.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              next.integralContributionQuote);
}

void test_timestamp_can_repeat_but_request_identity_cannot() {
    TemperatureController controller(parameters(), policy());
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    const auto second = controller.evaluate(airInput(100U, 25.0, 20.0));
    TEST_ASSERT_EQUAL_UINT64(100U, second.controlRequest->identity.createdAtMonotonicMillis);
    TEST_ASSERT_EQUAL_UINT64(first.controlRequest->identity.sequence + 1U,
                             second.controlRequest->identity.sequence);
}

void test_invalid_parameters_are_unavailable_without_a_request() {
    TemperatureControlParameters unconfigured;
    TemperatureController controller(unconfigured, policy());
    const auto result = controller.evaluate(airInput(100U, 25.0, 20.0));
    TEST_ASSERT_TRUE(result.status == TemperatureControlStatus::Unavailable);
    TEST_ASSERT_TRUE(result.reason == TemperatureControlReason::NoCommissioning);
    TEST_ASSERT_FALSE(result.controlRequest.has_value());
}

}  // namespace

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    static_cast<void>(argc);
    static_cast<void>(argv);
    UNITY_BEGIN();
    RUN_TEST(test_parameters_require_all_four_profiles);
    RUN_TEST(test_neutral_band_emits_identified_off_request);
    RUN_TEST(test_pi_uses_active_error_and_integrates_only_after_first_sample);
    RUN_TEST(test_feedback_disposition_freezes_integrator_and_missing_feedback_is_safe);
    RUN_TEST(test_feedback_for_off_or_wrong_request_is_invalid);
    RUN_TEST(test_raw_p_above_maximum_is_checked_and_saturated_without_p_preclamp);
    RUN_TEST(test_product_air_limit_reduces_and_blocks_normal_demand);
    RUN_TEST(test_air_limit_does_not_charge_integrator);
    RUN_TEST(test_committed_transition_applies_once_and_idle_does_not_consume_it);
    RUN_TEST(test_product_mode_requires_air_and_product_without_role_fallback);
    RUN_TEST(test_air_mode_does_not_require_product);
    RUN_TEST(test_large_gap_resets_and_uses_current_timestamp_as_new_anchor);
    RUN_TEST(test_timestamp_can_repeat_but_request_identity_cannot);
    RUN_TEST(test_invalid_parameters_are_unavailable_without_a_request);
    return UNITY_END();
}
