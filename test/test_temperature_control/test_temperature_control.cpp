#include <unity.h>

#include <array>
#include <cmath>
#include <limits>

#include "target_qualification.hpp"
#include "temperature_control.hpp"
#include "temperature_control_orchestrator.hpp"

namespace {

using namespace fermentation;
using device_platform::SensorQuality;
using device_platform::SensorQualitySnapshot;

SensorQualitySnapshot validSample(double celsius) {
    SensorQualitySnapshot snapshot;
    snapshot.quality = SensorQuality::Valid;
    // Per #20, only filteredCelsius is a normal control value; rawCelsius is
    // set alongside as harmless diagnostic evidence, never as the actual
    // read value.
    snapshot.filteredCelsius = celsius;
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
    return {IntegratorTransitionAction::Reset,
            IntegratorTransitionAction::Reset,
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

TargetQualificationInput qualificationInput(std::uint64_t timestamp,
                                            double measured) {
    TargetQualificationInput input;
    input.phase = QualificationPhase::Target;
    input.sampleTimestampMonotonicMillis = timestamp;
    input.targetCelsius = 20.0;
    input.bandCelsius = 0.5;
    input.qualificationDurationMillis = 1'000U;
    input.effectiveGraceMillis = 500U;
    input.maximumAcceptedSampleGapMillis = 2'000U;
    input.controlSensorRole = ControlSensorRole::Product;
    input.air = validSample(measured);
    input.product = validSample(measured);
    return input;
}

void test_parameters_require_all_four_profiles() {
    auto configured = parameters();
    TEST_ASSERT_TRUE(validateTemperatureControlParameters(configured));
    configured.productCooling.integralGainQuotePerCelsiusSecond = 0.0;
    TEST_ASSERT_FALSE(validateTemperatureControlParameters(configured));
}

void test_parameter_validation_distinguishes_unconfigured_from_invalid() {
    TemperatureControlParameters unconfigured;
    TEST_ASSERT_TRUE(classifyTemperatureControlParameters(unconfigured) ==
                     TemperatureControlParametersValidation::Unconfigured);
    TEST_ASSERT_TRUE(validateTemperatureControlParameters(parameters()));

    auto nanKp = parameters();
    nanKp.airHeating.proportionalGainQuotePerCelsius = NAN;
    TEST_ASSERT_TRUE(classifyTemperatureControlParameters(nanKp) ==
                     TemperatureControlParametersValidation::Invalid);
    TEST_ASSERT_TRUE(TemperatureController(nanKp, policy())
                         .evaluate(airInput(100U, 25.0, 20.0))
                         .reason ==
                     TemperatureControlReason::InvalidConfiguration);

    auto negativeKi = parameters();
    negativeKi.airCooling.integralGainQuotePerCelsiusSecond = -0.1;
    TEST_ASSERT_TRUE(classifyTemperatureControlParameters(negativeKi) ==
                     TemperatureControlParametersValidation::Invalid);

    auto negativeKp = parameters();
    negativeKp.airHeating.proportionalGainQuotePerCelsius = -0.1;
    TEST_ASSERT_TRUE(classifyTemperatureControlParameters(negativeKp) ==
                     TemperatureControlParametersValidation::Invalid);

    auto invalidQuote = parameters();
    invalidQuote.productHeating.maximumQuote = 1.1;
    TEST_ASSERT_TRUE(classifyTemperatureControlParameters(invalidQuote) ==
                     TemperatureControlParametersValidation::Invalid);

    auto zeroStep = parameters();
    zeroStep.maximumIntegrationStepMillis = 0U;
    TEST_ASSERT_TRUE(classifyTemperatureControlParameters(zeroStep) ==
                     TemperatureControlParametersValidation::Invalid);

    auto wrongAirOrder = parameters();
    wrongAirOrder.airLimitLowerBlockCelsius = 12.0;
    wrongAirOrder.airLimitLowerReduceStartCelsius = 10.0;
    TEST_ASSERT_TRUE(classifyTemperatureControlParameters(wrongAirOrder) ==
                     TemperatureControlParametersValidation::Invalid);

    const auto unconfiguredResult =
        TemperatureController(unconfigured, policy())
            .evaluate(airInput(100U, 25.0, 20.0));
    TEST_ASSERT_TRUE(unconfiguredResult.status ==
                     TemperatureControlStatus::Unavailable);
    TEST_ASSERT_TRUE(unconfiguredResult.reason ==
                     TemperatureControlReason::NoCommissioning);
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
    TEST_ASSERT_EQUAL_UINT64(
        100U, result.controlRequest->identity.createdAtMonotonicMillis);
}

void test_pi_uses_active_error_and_integrates_only_after_first_sample() {
    TemperatureController controller(parameters(), policy());
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    TEST_ASSERT_TRUE(first.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.4, first.rawProportionalQuote);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, first.integralContributionQuote);

    auto secondInput = airInput(1100U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
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
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, deferred.integralContributionQuote);

    const auto missing = controller.evaluate(airInput(2100U, 25.0, 20.0));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, missing.integralContributionQuote);
}

void test_feedback_for_off_or_wrong_request_is_invalid() {
    TemperatureController controller(parameters(), policy());
    const auto off = controller.evaluate(airInput(100U, 20.0, 20.0));
    TEST_ASSERT_TRUE(off.controlRequest.has_value());
    auto next = airInput(200U, 25.0, 20.0);
    next.previousControlRequestFeedback = PreviousControlRequestFeedback{
        off.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
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
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, result.integralContributionQuote);
}

void test_raw_p_equal_to_maximum_has_zero_headroom() {
    auto configured = parameters();
    configured.airHeating.proportionalGainQuotePerCelsius = 0.2;
    TemperatureController controller(configured, policy());
    const auto result = controller.evaluate(airInput(100U, 25.0, 20.0));
    TEST_ASSERT_TRUE(result.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(result.reason == TemperatureControlReason::Saturated);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.8, result.rawProportionalQuote);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, result.integralContributionQuote);
}

void test_second_raw_p_saturation_skips_overflowing_positive_delta_i() {
    auto configured = parameters();
    configured.airHeating.proportionalGainQuotePerCelsius = 0.2;
    configured.airHeating.integralGainQuotePerCelsiusSecond =
        std::numeric_limits<double>::max();
    TemperatureController controller(configured, policy());
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    auto secondInput = airInput(110U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto second = controller.evaluate(secondInput);
    TEST_ASSERT_TRUE(second.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(second.reason == TemperatureControlReason::Saturated);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, second.integralContributionQuote);
}

void test_current_integral_headroom_saturation_skips_overflowing_positive_delta_i() {
    auto configured = parameters();
    configured.airHeating.proportionalGainQuotePerCelsius = 0.2;
    configured.airHeating.integralGainQuotePerCelsiusSecond = 0.05;
    configured.airHeating.neutralBandWidthCelsius = 0.8;
    configured.airCooling.proportionalGainQuotePerCelsius = 0.2;
    configured.airCooling.integralGainQuotePerCelsiusSecond =
        std::numeric_limits<double>::max();
    configured.airCooling.neutralBandWidthCelsius = 0.8;
    const IntegratorTransitionPolicy boundedCarry{
        IntegratorTransitionAction::BoundedCarry,
        IntegratorTransitionAction::BoundedCarry,
        IntegratorTransitionAction::BoundedCarry, 0.16};
    TemperatureController controller(configured, boundedCarry);

    const auto first = controller.evaluate(airInput(100U, 25.0, 21.0));
    auto exactHeadroomInput = airInput(1'100U, 25.0, 21.0);
    exactHeadroomInput
        .previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto exactHeadroom = controller.evaluate(exactHeadroomInput);
    TEST_ASSERT_TRUE(exactHeadroom.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(exactHeadroom.reason ==
                     TemperatureControlReason::Saturated);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.16,
                              exactHeadroom.integralContributionQuote);
    TEST_ASSERT_DOUBLE_WITHIN(
        0.0001, 0.16,
        exactHeadroom.maximumLimitedQuote - exactHeadroom.rawProportionalQuote);

    // The bounded direction change carries I exactly to the cooling profile's
    // headroom. The next valid feedback would overflow if deltaI were formed.
    auto transitionInput = airInput(2'100U, 20.0, 24.0);
    transitionInput
        .previousControlRequestFeedback = PreviousControlRequestFeedback{
        exactHeadroom.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto transition = controller.evaluate(transitionInput);
    TEST_ASSERT_TRUE(transition.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(transition.reason == TemperatureControlReason::Saturated);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.16,
                              transition.integralContributionQuote);

    auto saturatedInput = airInput(3'100U, 20.0, 24.0);
    saturatedInput
        .previousControlRequestFeedback = PreviousControlRequestFeedback{
        transition.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto saturated = controller.evaluate(saturatedInput);
    TEST_ASSERT_TRUE(saturated.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(saturated.reason == TemperatureControlReason::Saturated);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.16,
                              saturated.integralContributionQuote);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.8, saturated.unboundedQuote);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.8, saturated.maximumLimitedQuote);
    TEST_ASSERT_TRUE(saturated.controlRequest.has_value());
    TEST_ASSERT_EQUAL_UINT64(transition.controlRequest->identity.sequence + 1U,
                             saturated.controlRequest->identity.sequence);
}

void test_integral_headroom_caps_exact_and_oversized_steps_and_reduces_existing_i() {
    auto exactParameters = parameters();
    exactParameters.airHeating.proportionalGainQuotePerCelsius = 0.2;
    exactParameters.airHeating.integralGainQuotePerCelsiusSecond = 0.05;
    exactParameters.airHeating.neutralBandWidthCelsius = 0.8;
    TemperatureController exact(exactParameters, policy());
    const auto exactFirst = exact.evaluate(airInput(100U, 25.0, 21.0));
    auto exactSecondInput = airInput(1'100U, 25.0, 21.0);
    exactSecondInput
        .previousControlRequestFeedback = PreviousControlRequestFeedback{
        exactFirst.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto exactSecond = exact.evaluate(exactSecondInput);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.16,
                              exactSecond.integralContributionQuote);
    TEST_ASSERT_TRUE(exactSecond.integralContributionQuote <=
                     exactParameters.airHeating.maximumQuote -
                         exactSecond.rawProportionalQuote);

    auto oversizedParameters = exactParameters;
    oversizedParameters.airHeating.integralGainQuotePerCelsiusSecond = 0.2;
    TemperatureController oversized(oversizedParameters, policy());
    const auto oversizedFirst = oversized.evaluate(airInput(100U, 25.0, 21.0));
    auto oversizedSecondInput = airInput(1'100U, 25.0, 21.0);
    oversizedSecondInput
        .previousControlRequestFeedback = PreviousControlRequestFeedback{
        oversizedFirst.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto oversizedSecond = oversized.evaluate(oversizedSecondInput);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.16,
                              oversizedSecond.integralContributionQuote);

    auto reductionParameters = parameters();
    reductionParameters.airHeating.integralGainQuotePerCelsiusSecond = 0.1;
    reductionParameters.airHeating.neutralBandWidthCelsius = 0.8;
    TemperatureController reduction(reductionParameters, policy());
    const auto reductionFirst = reduction.evaluate(airInput(100U, 25.0, 20.0));
    auto reductionSecondInput = airInput(1'100U, 25.0, 20.0);
    reductionSecondInput
        .previousControlRequestFeedback = PreviousControlRequestFeedback{
        reductionFirst.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto reductionSecond = reduction.evaluate(reductionSecondInput);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.38,
                              reductionSecond.integralContributionQuote);
    auto reductionThirdInput = airInput(2'100U, 25.0, 19.0);
    reductionThirdInput
        .previousControlRequestFeedback = PreviousControlRequestFeedback{
        reductionSecond.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto reductionThird = reduction.evaluate(reductionThirdInput);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.28,
                              reductionThird.integralContributionQuote);
    TEST_ASSERT_TRUE(reductionThird.integralContributionQuote <=
                     reductionParameters.airHeating.maximumQuote -
                         reductionThird.rawProportionalQuote);
}

void test_lifecycle_boundary_resets_pi_and_qualification_ram() {
    TemperatureController controller(parameters(), policy());
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    auto secondInput = airInput(1'100U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto second = controller.evaluate(secondInput);
    TEST_ASSERT_TRUE(second.integralContributionQuote > 0.0);

    TargetQualificationEvaluator evaluator;
    auto qualificationFirst =
        evaluator.evaluate(qualificationInput(100U, 20.0));
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        qualificationFirst, {{20.0, 0.5, ControlSensorRole::Product}, 0U, 0U}));
    auto qualificationSecond =
        evaluator.evaluate(qualificationInput(1'100U, 20.0));
    TEST_ASSERT_TRUE(qualificationSecond.creditedInBandMillis > 0U);
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        qualificationSecond,
        {{20.0, 0.5, ControlSensorRole::Product}, 0U, 0U}));

    resetTemperatureControlAtBoundary(
        controller, evaluator, TemperatureControlLifecycleBoundary::Recovery);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              controller.state().integralContributionQuote);
    TEST_ASSERT_FALSE(controller.state().lastActiveDirection.has_value());
    TEST_ASSERT_FALSE(controller.state().feedbackWindow.has_value());
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);
    TEST_ASSERT_FALSE(evaluator.state().episodeActive);

    const auto firstAfterReset =
        controller.evaluate(airInput(2'000U, 25.0, 20.0));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              firstAfterReset.integralContributionQuote);
    TEST_ASSERT_TRUE(firstAfterReset.controlRequest->identity.sequence >
                     second.controlRequest->identity.sequence);
}

void test_product_air_limit_reduces_and_blocks_normal_demand() {
    TemperatureController controller(parameters(), policy());
    const auto reduced =
        controller.evaluate(productInput(100U, 25.0, 20.0, 32.5));
    TEST_ASSERT_TRUE(reduced.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(reduced.reason ==
                     TemperatureControlReason::AirLimitReduced);
    TEST_ASSERT_TRUE(reduced.airLimitState == AirLimitState::Reduced);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.4, reduced.timeQuote);

    const auto blocked =
        controller.evaluate(productInput(200U, 25.0, 20.0, 35.0));
    TEST_ASSERT_TRUE(blocked.status == TemperatureControlStatus::Off);
    TEST_ASSERT_TRUE(blocked.reason ==
                     TemperatureControlReason::AirLimitBlocked);
    TEST_ASSERT_TRUE(blocked.airLimitState == AirLimitState::Blocked);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, blocked.timeQuote);
}

void test_product_air_limit_blocking_returns_idle_for_both_pi_directions() {
    struct BlockingCase {
        bool cooling;
        double airCelsius;
        AbstractControlDirection activeDirection;
    };
    const std::array cases = {
        BlockingCase{false, 35.0, AbstractControlDirection::Heating},
        BlockingCase{true, 5.0, AbstractControlDirection::Cooling},
    };

    for (const auto& testCase : cases) {
        TemperatureController controller(parameters(), policy());
        const auto blocked = testCase.cooling
                                 ? controller.evaluate(productInput(
                                       100U, 20.0, 25.0, testCase.airCelsius))
                                 : controller.evaluate(productInput(
                                       100U, 25.0, 20.0, testCase.airCelsius));
        TEST_ASSERT_TRUE(blocked.status == TemperatureControlStatus::Off);
        TEST_ASSERT_TRUE(blocked.reason ==
                         TemperatureControlReason::AirLimitBlocked);
        TEST_ASSERT_TRUE(blocked.airLimitState == AirLimitState::Blocked);
        TEST_ASSERT_TRUE(blocked.direction == AbstractControlDirection::Idle);
        TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, blocked.timeQuote);
        TEST_ASSERT_TRUE(blocked.controlRequest.has_value());
        TEST_ASSERT_TRUE(blocked.controlRequest->direction ==
                         AbstractControlDirection::Idle);
        TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                                  blocked.controlRequest->timeQuote);
        TEST_ASSERT_EQUAL_UINT64(1U, blocked.controlRequest->identity.sequence);
        TEST_ASSERT_EQUAL_UINT64(
            100U, blocked.controlRequest->identity.createdAtMonotonicMillis);
        TEST_ASSERT_TRUE(controller.state().lastActiveDirection.has_value());
        TEST_ASSERT_TRUE(*controller.state().lastActiveDirection ==
                         testCase.activeDirection);
    }
}

void test_product_air_limit_boundaries_cover_both_directions() {
    struct BoundaryCase {
        bool cooling;
        double airCelsius;
        AirLimitState expectedAirLimitState;
        TemperatureControlStatus expectedStatus;
        TemperatureControlReason expectedReason;
        AbstractControlDirection expectedDirection;
        double expectedQuote;
    };
    const std::array cases = {
        BoundaryCase{false, 30.0, AirLimitState::Unrestricted,
                     TemperatureControlStatus::Demand,
                     TemperatureControlReason::Saturated,
                     AbstractControlDirection::Heating, 0.8},
        BoundaryCase{false, 32.5, AirLimitState::Reduced,
                     TemperatureControlStatus::Demand,
                     TemperatureControlReason::AirLimitReduced,
                     AbstractControlDirection::Heating, 0.4},
        BoundaryCase{false, 35.0, AirLimitState::Blocked,
                     TemperatureControlStatus::Off,
                     TemperatureControlReason::AirLimitBlocked,
                     AbstractControlDirection::Idle, 0.0},
        BoundaryCase{true, 10.0, AirLimitState::Unrestricted,
                     TemperatureControlStatus::Demand,
                     TemperatureControlReason::Saturated,
                     AbstractControlDirection::Cooling, 0.8},
        BoundaryCase{true, 7.5, AirLimitState::Reduced,
                     TemperatureControlStatus::Demand,
                     TemperatureControlReason::AirLimitReduced,
                     AbstractControlDirection::Cooling, 0.4},
        BoundaryCase{true, 5.0, AirLimitState::Blocked,
                     TemperatureControlStatus::Off,
                     TemperatureControlReason::AirLimitBlocked,
                     AbstractControlDirection::Idle, 0.0},
    };

    for (const auto& testCase : cases) {
        TemperatureController controller(parameters(), policy());
        const auto result = testCase.cooling
                                ? controller.evaluate(productInput(
                                      100U, 20.0, 25.0, testCase.airCelsius))
                                : controller.evaluate(productInput(
                                      100U, 25.0, 20.0, testCase.airCelsius));
        TEST_ASSERT_TRUE(result.airLimitState ==
                         testCase.expectedAirLimitState);
        TEST_ASSERT_TRUE(result.status == testCase.expectedStatus);
        TEST_ASSERT_TRUE(result.reason == testCase.expectedReason);
        TEST_ASSERT_TRUE(result.direction == testCase.expectedDirection);
        TEST_ASSERT_DOUBLE_WITHIN(0.0001, testCase.expectedQuote,
                                  result.timeQuote);
    }
}

void test_air_limit_reduced_and_blocked_never_charge_integrator() {
    struct LimitCase {
        bool cooling;
        double airCelsius;
        AirLimitState expectedAirLimitState;
    };
    const std::array cases = {
        LimitCase{false, 32.0, AirLimitState::Reduced},
        LimitCase{false, 35.0, AirLimitState::Blocked},
        LimitCase{true, 8.0, AirLimitState::Reduced},
        LimitCase{true, 5.0, AirLimitState::Blocked},
    };

    for (const auto& testCase : cases) {
        TemperatureController controller(parameters(), policy());
        const auto first = testCase.cooling
                               ? controller.evaluate(productInput(
                                     100U, 20.0, 25.0, testCase.airCelsius))
                               : controller.evaluate(productInput(
                                     100U, 25.0, 20.0, testCase.airCelsius));
        auto nextInput =
            testCase.cooling
                ? productInput(1100U, 20.0, 25.0, testCase.airCelsius)
                : productInput(1100U, 25.0, 20.0, testCase.airCelsius);
        if (testCase.expectedAirLimitState == AirLimitState::Reduced) {
            nextInput.previousControlRequestFeedback =
                PreviousControlRequestFeedback{
                    first.controlRequest->identity.sequence,
                    PreviousControlRequestFeedback::Disposition::
                        NoIntegratorConstraint};
        }
        const auto result = controller.evaluate(nextInput);
        TEST_ASSERT_TRUE(result.airLimitState ==
                         testCase.expectedAirLimitState);
        TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                                  result.integralContributionQuote);
    }
}

void test_air_limit_does_not_charge_integrator() {
    TemperatureController controller(parameters(), policy());
    const auto first =
        controller.evaluate(productInput(100U, 25.0, 20.0, 32.0));
    auto next = productInput(1100U, 25.0, 20.0, 32.0);
    next.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto result = controller.evaluate(next);
    TEST_ASSERT_TRUE(result.airLimitState == AirLimitState::Reduced);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, result.integralContributionQuote);
}

void test_committed_transition_applies_once_and_idle_does_not_consume_it() {
    auto configured = parameters();
    TemperatureController controller(
        configured, {IntegratorTransitionAction::BoundedCarry,
                     IntegratorTransitionAction::BoundedCarry,
                     IntegratorTransitionAction::BoundedCarry, 0.1});
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    auto secondInput = airInput(1100U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
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
    TEST_ASSERT_TRUE(unavailable.status ==
                     TemperatureControlStatus::Unavailable);
    TEST_ASSERT_TRUE(unavailable.reason ==
                     TemperatureControlReason::SensorUnavailable);
    TEST_ASSERT_TRUE(unavailable.airLimitState == AirLimitState::Unavailable);
    TEST_ASSERT_FALSE(unavailable.controlRequest.has_value());

    input = productInput(200U, 25.0, 20.0, 32.0);
    input.air.quality = SensorQuality::Valid;
    input.air.filteredCelsius = NAN;
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

void test_air_mode_sensor_failures_are_not_applied_air_limits() {
    for (const auto quality : {SensorQuality::Stale, SensorQuality::Failed}) {
        TemperatureController controller(parameters(), policy());
        auto input = airInput(100U, 25.0, 20.0);
        input.air = unavailableSample(quality);
        const auto result = controller.evaluate(input);
        TEST_ASSERT_TRUE(result.status ==
                         TemperatureControlStatus::Unavailable);
        TEST_ASSERT_TRUE(result.reason ==
                         TemperatureControlReason::SensorUnavailable);
        TEST_ASSERT_TRUE(result.airLimitState == AirLimitState::NotApplied);
        TEST_ASSERT_FALSE(result.controlRequest.has_value());
    }

    TemperatureController controller(parameters(), policy());
    auto invalidInput = airInput(100U, 25.0, 20.0);
    invalidInput.air.filteredCelsius = NAN;
    const auto invalid = controller.evaluate(invalidInput);
    TEST_ASSERT_TRUE(invalid.status == TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_TRUE(invalid.reason == TemperatureControlReason::InvalidSample);
    TEST_ASSERT_TRUE(invalid.airLimitState == AirLimitState::NotApplied);
    TEST_ASSERT_FALSE(invalid.controlRequest.has_value());
}

void test_air_mode_early_diagnostics_keep_air_limit_not_applied() {
    TemperatureControlParameters unconfigured;
    const auto noCommissioning = TemperatureController(unconfigured, policy())
                                     .evaluate(airInput(100U, 25.0, 20.0));
    TEST_ASSERT_TRUE(noCommissioning.reason ==
                     TemperatureControlReason::NoCommissioning);
    TEST_ASSERT_TRUE(noCommissioning.airLimitState ==
                     AirLimitState::NotApplied);

    auto malformed = parameters();
    malformed.airHeating.proportionalGainQuotePerCelsius = NAN;
    const auto invalidConfiguration = TemperatureController(malformed, policy())
                                          .evaluate(airInput(100U, 25.0, 20.0));
    TEST_ASSERT_TRUE(invalidConfiguration.reason ==
                     TemperatureControlReason::InvalidConfiguration);
    TEST_ASSERT_TRUE(invalidConfiguration.airLimitState ==
                     AirLimitState::NotApplied);

    auto invalidTarget = airInput(100U, 25.0, 20.0);
    invalidTarget.targetCelsius = NAN;
    const auto invalid =
        TemperatureController(parameters(), policy()).evaluate(invalidTarget);
    TEST_ASSERT_TRUE(invalid.reason ==
                     TemperatureControlReason::InvalidConfiguration);
    TEST_ASSERT_TRUE(invalid.airLimitState == AirLimitState::NotApplied);
}

void test_sensor_role_transition_resets_integral_on_first_product_sample() {
    TemperatureController controller(parameters(), policy());
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    auto secondInput = airInput(1100U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto second = controller.evaluate(secondInput);
    TEST_ASSERT_TRUE(second.integralContributionQuote > 0.0);
    TEST_ASSERT_TRUE(controller.markCommittedControlContextTransitionPending(
        CommittedControlContextTransition::SensorRoleChange));

    const auto product =
        controller.evaluate(productInput(1200U, 25.0, 20.0, 32.0));
    TEST_ASSERT_TRUE(product.controlRequest.has_value());
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, product.integralContributionQuote);
    TEST_ASSERT_FALSE(controller.state().pendingContextTransition.has_value());
}

void test_invalid_sensor_sample_clears_old_pi_state_before_recovery() {
    TemperatureController controller(parameters(), policy());
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    auto secondInput = airInput(1100U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto second = controller.evaluate(secondInput);
    TEST_ASSERT_TRUE(second.integralContributionQuote > 0.0);

    auto invalidInput = airInput(1200U, 25.0, 20.0);
    invalidInput.air.filteredCelsius = NAN;
    TEST_ASSERT_TRUE(controller.evaluate(invalidInput).status ==
                     TemperatureControlStatus::InvalidInput);
    const auto recovered = controller.evaluate(airInput(1300U, 25.0, 20.0));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, recovered.integralContributionQuote);
    TEST_ASSERT_TRUE(recovered.controlRequest.has_value());
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
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, next.integralContributionQuote);
}

void test_timestamp_can_repeat_but_request_identity_cannot() {
    TemperatureController controller(parameters(), policy());
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    const auto second = controller.evaluate(airInput(100U, 25.0, 20.0));
    TEST_ASSERT_EQUAL_UINT64(
        100U, second.controlRequest->identity.createdAtMonotonicMillis);
    TEST_ASSERT_EQUAL_UINT64(first.controlRequest->identity.sequence + 1U,
                             second.controlRequest->identity.sequence);
}

void test_near_overflow_pi_timestamps_use_checked_delta() {
    TemperatureController controller(parameters(), policy());
    const auto first = controller.evaluate(
        airInput(std::numeric_limits<std::uint64_t>::max() - 100U, 25.0, 20.0));
    auto secondInput =
        airInput(std::numeric_limits<std::uint64_t>::max(), 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto second = controller.evaluate(secondInput);
    TEST_ASSERT_TRUE(second.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(second.integralContributionQuote > 0.0);
}

void test_backward_pi_timestamp_is_invalid_and_clears_state() {
    TemperatureController controller(parameters(), policy());
    const auto first = controller.evaluate(airInput(1'000U, 25.0, 20.0));
    auto backward = airInput(999U, 25.0, 20.0);
    backward.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto result = controller.evaluate(backward);
    TEST_ASSERT_TRUE(result.reason == TemperatureControlReason::TimeInvalid);
    TEST_ASSERT_FALSE(result.controlRequest.has_value());
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              controller.state().integralContributionQuote);
}

void test_feedback_replay_and_all_dispositions_fail_closed() {
    for (const auto disposition :
         {PreviousControlRequestFeedback::Disposition::DeferredOrLimited,
          PreviousControlRequestFeedback::Disposition::Rejected}) {
        TemperatureController controller(parameters(), policy());
        const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
        auto nextInput = airInput(1100U, 25.0, 20.0);
        nextInput.previousControlRequestFeedback =
            PreviousControlRequestFeedback{
                first.controlRequest->identity.sequence, disposition};
        const auto next = controller.evaluate(nextInput);
        TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, next.integralContributionQuote);
    }

    TemperatureController oldController(parameters(), policy());
    const auto oldFirst = oldController.evaluate(airInput(100U, 25.0, 20.0));
    auto secondInput = airInput(200U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        oldFirst.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto second = oldController.evaluate(secondInput);
    auto oldFeedbackInput = airInput(300U, 25.0, 20.0);
    oldFeedbackInput
        .previousControlRequestFeedback = PreviousControlRequestFeedback{
        oldFirst.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    TEST_ASSERT_TRUE(oldController.evaluate(oldFeedbackInput).status ==
                     TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_TRUE(second.controlRequest.has_value());

    TemperatureController foreignController(parameters(), policy());
    const auto foreignFirst =
        foreignController.evaluate(airInput(100U, 25.0, 20.0));
    auto foreignInput = airInput(200U, 25.0, 20.0);
    foreignInput
        .previousControlRequestFeedback = PreviousControlRequestFeedback{
        foreignFirst.controlRequest->identity.sequence + 1U,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    TEST_ASSERT_TRUE(foreignController.evaluate(foreignInput).status ==
                     TemperatureControlStatus::InvalidInput);

    TemperatureController duplicateController(parameters(), policy());
    const auto duplicateFirst =
        duplicateController.evaluate(airInput(100U, 25.0, 20.0));
    auto duplicateInput = airInput(200U, 25.0, 20.0);
    duplicateInput
        .previousControlRequestFeedback = PreviousControlRequestFeedback{
        duplicateFirst.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    static_cast<void>(duplicateController.evaluate(duplicateInput));
    duplicateInput.sampleTimestampMonotonicMillis = 300U;
    TEST_ASSERT_TRUE(duplicateController.evaluate(duplicateInput).status ==
                     TemperatureControlStatus::InvalidInput);
}

void test_unknown_feedback_disposition_is_fail_closed() {
    TemperatureController controller(parameters(), policy());
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    auto invalidInput = airInput(1100U, 25.0, 20.0);
    invalidInput.previousControlRequestFeedback =
        PreviousControlRequestFeedback{
            first.controlRequest->identity.sequence,
            static_cast<PreviousControlRequestFeedback::Disposition>(0xFFU)};

    const auto invalid = controller.evaluate(invalidInput);
    TEST_ASSERT_TRUE(invalid.status == TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_TRUE(invalid.reason == TemperatureControlReason::InvalidSample);
    TEST_ASSERT_FALSE(invalid.controlRequest.has_value());
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              controller.state().integralContributionQuote);
    TEST_ASSERT_FALSE(controller.state().feedbackWindow.has_value());
    TEST_ASSERT_FALSE(controller.state().lastActiveDirection.has_value());
}

void test_direction_sequences_cover_idle_and_opposite_demand() {
    TemperatureController controller(parameters(), policy());
    const auto heating = controller.evaluate(airInput(100U, 25.0, 20.0));
    TEST_ASSERT_TRUE(heating.direction == AbstractControlDirection::Heating);
    const auto idle = controller.evaluate(airInput(200U, 20.0, 20.0));
    TEST_ASSERT_TRUE(idle.direction == AbstractControlDirection::Idle);
    const auto cooling = controller.evaluate(airInput(300U, 20.0, 25.0));
    TEST_ASSERT_TRUE(cooling.direction == AbstractControlDirection::Cooling);
    const auto idleAgain = controller.evaluate(airInput(400U, 20.0, 20.0));
    TEST_ASSERT_TRUE(idleAgain.direction == AbstractControlDirection::Idle);
    const auto heatingAgain = controller.evaluate(airInput(500U, 25.0, 20.0));
    TEST_ASSERT_TRUE(heatingAgain.direction ==
                     AbstractControlDirection::Heating);
}

void test_direction_changes_reset_or_bound_carry_exactly_once() {
    TemperatureController resetController(parameters(), policy());
    const auto heating = resetController.evaluate(airInput(100U, 25.0, 20.0));
    auto heatingNext = airInput(1100U, 25.0, 20.0);
    heatingNext.previousControlRequestFeedback = PreviousControlRequestFeedback{
        heating.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto integrated = resetController.evaluate(heatingNext);
    TEST_ASSERT_TRUE(integrated.integralContributionQuote > 0.0);
    static_cast<void>(resetController.evaluate(airInput(1200U, 20.0, 20.0)));
    const auto cooling = resetController.evaluate(airInput(1300U, 20.0, 25.0));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, cooling.integralContributionQuote);
    auto coolingNext = airInput(1400U, 20.0, 25.0);
    coolingNext.previousControlRequestFeedback = PreviousControlRequestFeedback{
        cooling.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto coolingIntegrated = resetController.evaluate(coolingNext);
    TEST_ASSERT_TRUE(coolingIntegrated.integralContributionQuote > 0.0);
    static_cast<void>(resetController.evaluate(airInput(1500U, 20.0, 20.0)));
    const auto heatingAgain =
        resetController.evaluate(airInput(1600U, 25.0, 20.0));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              heatingAgain.integralContributionQuote);

    auto bounded = parameters();
    TemperatureController boundedController(
        bounded, {IntegratorTransitionAction::BoundedCarry,
                  IntegratorTransitionAction::BoundedCarry,
                  IntegratorTransitionAction::BoundedCarry, 0.1});
    const auto boundedHeating =
        boundedController.evaluate(airInput(100U, 25.0, 20.0));
    auto boundedNext = airInput(10'000U, 25.0, 20.0);
    boundedNext.previousControlRequestFeedback = PreviousControlRequestFeedback{
        boundedHeating.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto boundedIntegrated = boundedController.evaluate(boundedNext);
    TEST_ASSERT_TRUE(boundedIntegrated.integralContributionQuote > 0.1);
    static_cast<void>(
        boundedController.evaluate(airInput(10'100U, 20.0, 20.0)));
    const auto boundedCooling =
        boundedController.evaluate(airInput(10'200U, 20.0, 25.0));
    TEST_ASSERT_TRUE(boundedCooling.integralContributionQuote <= 0.1);
    TEST_ASSERT_TRUE(boundedController.state().pendingTransitionAction ==
                     std::nullopt);
}

void test_invalid_parameters_are_unavailable_without_a_request() {
    TemperatureControlParameters unconfigured;
    TemperatureController controller(unconfigured, policy());
    const auto result = controller.evaluate(airInput(100U, 25.0, 20.0));
    TEST_ASSERT_TRUE(result.status == TemperatureControlStatus::Unavailable);
    TEST_ASSERT_TRUE(result.reason ==
                     TemperatureControlReason::NoCommissioning);
    TEST_ASSERT_FALSE(result.controlRequest.has_value());
}

void test_request_identity_does_not_wrap_at_uint64_max() {
    TemperatureController controller(parameters(), policy(),
                                     std::numeric_limits<std::uint64_t>::max());
    const auto last = controller.evaluate(airInput(100U, 20.0, 20.0));
    TEST_ASSERT_TRUE(last.controlRequest.has_value());
    TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<std::uint64_t>::max(),
                             last.controlRequest->identity.sequence);

    const auto exhausted = controller.evaluate(airInput(200U, 20.0, 20.0));
    TEST_ASSERT_TRUE(exhausted.status ==
                     TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_TRUE(exhausted.reason ==
                     TemperatureControlReason::RequestIdentityExhausted);
    TEST_ASSERT_FALSE(exhausted.controlRequest.has_value());
}

void test_request_identity_exhaustion_clears_nonempty_idle_state() {
    TemperatureController controller(
        parameters(), policy(), std::numeric_limits<std::uint64_t>::max() - 1U);
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    auto secondInput = airInput(1100U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto second = controller.evaluate(secondInput);
    TEST_ASSERT_TRUE(second.controlRequest.has_value());
    TEST_ASSERT_TRUE(controller.state().integralContributionQuote > 0.0);

    auto activeExhaustionInput = airInput(1200U, 25.0, 20.0);
    activeExhaustionInput
        .previousControlRequestFeedback = PreviousControlRequestFeedback{
        second.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto activeExhausted = controller.evaluate(activeExhaustionInput);
    TEST_ASSERT_TRUE(activeExhausted.reason ==
                     TemperatureControlReason::RequestIdentityExhausted);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              controller.state().integralContributionQuote);
    TEST_ASSERT_FALSE(controller.state().lastActiveDirection.has_value());
    TEST_ASSERT_FALSE(controller.state().feedbackWindow.has_value());

    const auto idleExhausted = controller.evaluate(airInput(1300U, 20.0, 20.0));
    TEST_ASSERT_TRUE(idleExhausted.reason ==
                     TemperatureControlReason::RequestIdentityExhausted);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              controller.state().integralContributionQuote);
    TEST_ASSERT_FALSE(controller.state().lastActiveDirection.has_value());
    TEST_ASSERT_FALSE(controller.state().feedbackWindow.has_value());
}

void test_request_identity_exhaustion_idle_path_clears_nonempty_pi_directly() {
    TemperatureController controller(
        parameters(), policy(), std::numeric_limits<std::uint64_t>::max() - 1U);
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    auto secondInput = airInput(1100U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto second = controller.evaluate(secondInput);
    TEST_ASSERT_TRUE(second.reason !=
                     TemperatureControlReason::RequestIdentityExhausted);
    TEST_ASSERT_TRUE(controller.state().integralContributionQuote > 0.0);

    const auto idle = controller.evaluate(airInput(1200U, 20.0, 20.0));
    TEST_ASSERT_TRUE(idle.reason ==
                     TemperatureControlReason::RequestIdentityExhausted);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              controller.state().integralContributionQuote);
    TEST_ASSERT_FALSE(controller.state().lastActiveDirection.has_value());
    TEST_ASSERT_FALSE(controller.state().feedbackWindow.has_value());
}

void test_valid_quality_without_filtered_value_is_fail_closed_not_raw_fallback() {
    // FR2: SensorQuality::Valid with a missing filteredCelsius is structurally
    // inconsistent evidence, even if rawCelsius/correctedCelsius are present.
    // It must never be used as a normal control value fallback.
    TemperatureController rawOnly(parameters(), policy());
    auto rawOnlyInput = airInput(100U, 25.0, 20.0);
    rawOnlyInput.air.filteredCelsius = std::nullopt;
    rawOnlyInput.air.rawCelsius = 20.0;
    const auto rawResult = rawOnly.evaluate(rawOnlyInput);
    TEST_ASSERT_TRUE(rawResult.status ==
                     TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_FALSE(rawResult.controlRequest.has_value());

    TemperatureController correctedOnly(parameters(), policy());
    auto correctedOnlyInput = airInput(100U, 25.0, 20.0);
    correctedOnlyInput.air.filteredCelsius = std::nullopt;
    correctedOnlyInput.air.rawCelsius = std::nullopt;
    correctedOnlyInput.air.correctedCelsius = 20.0;
    const auto correctedResult = correctedOnly.evaluate(correctedOnlyInput);
    TEST_ASSERT_TRUE(correctedResult.status ==
                     TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_FALSE(correctedResult.controlRequest.has_value());
}

void test_stale_quality_with_old_filtered_value_is_unavailable() {
    // FR2: quality != Valid is always Unavailable, even if a previously
    // accepted filteredCelsius value is still carried on the snapshot.
    TemperatureController controller(parameters(), policy());
    auto input = airInput(100U, 25.0, 20.0);
    input.air.quality = SensorQuality::Stale;
    const auto result = controller.evaluate(input);
    TEST_ASSERT_TRUE(result.status == TemperatureControlStatus::Unavailable);
    TEST_ASSERT_TRUE(result.reason ==
                     TemperatureControlReason::SensorUnavailable);
    TEST_ASSERT_FALSE(result.controlRequest.has_value());
}

void test_recovery_after_invalid_evidence_starts_a_clean_pi_run() {
    // FR2: after Unavailable/Invalid evidence, a later genuine
    // Valid+filteredCelsius sample must not carry over an old PI impulse.
    TemperatureController controller(parameters(), policy());
    const auto first = controller.evaluate(airInput(100U, 25.0, 20.0));
    auto secondInput = airInput(1100U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    const auto second = controller.evaluate(secondInput);
    TEST_ASSERT_TRUE(second.integralContributionQuote > 0.0);

    auto missingFiltered = airInput(1200U, 25.0, 20.0);
    missingFiltered.air.filteredCelsius = std::nullopt;
    missingFiltered.air.rawCelsius = 20.0;
    TEST_ASSERT_TRUE(controller.evaluate(missingFiltered).status ==
                     TemperatureControlStatus::InvalidInput);
    const auto recovered = controller.evaluate(airInput(1300U, 25.0, 20.0));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, recovered.integralContributionQuote);
    TEST_ASSERT_TRUE(recovered.controlRequest.has_value());
}

void test_initial_request_sequence_zero_is_normalized_to_never_appear() {
    // FR13: sequence 0 is reserved as "no request identity" by the feedback
    // contract; constructing with initialRequestSequence == 0 must never
    // produce a ControlRequest carrying sequence 0.
    TemperatureController controller(parameters(), policy(), 0U);
    const auto off = controller.evaluate(airInput(100U, 20.0, 20.0));
    TEST_ASSERT_TRUE(off.controlRequest.has_value());
    TEST_ASSERT_TRUE(off.controlRequest->identity.sequence != 0U);
    TEST_ASSERT_EQUAL_UINT64(1U, off.controlRequest->identity.sequence);

    const auto demand = controller.evaluate(airInput(1100U, 25.0, 20.0));
    TEST_ASSERT_TRUE(demand.controlRequest.has_value());
    TEST_ASSERT_TRUE(demand.controlRequest->identity.sequence != 0U);
}

}  // namespace

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    static_cast<void>(argc);
    static_cast<void>(argv);
    UNITY_BEGIN();
    RUN_TEST(test_parameters_require_all_four_profiles);
    RUN_TEST(test_parameter_validation_distinguishes_unconfigured_from_invalid);
    RUN_TEST(test_neutral_band_emits_identified_off_request);
    RUN_TEST(test_pi_uses_active_error_and_integrates_only_after_first_sample);
    RUN_TEST(
        test_feedback_disposition_freezes_integrator_and_missing_feedback_is_safe);
    RUN_TEST(test_feedback_for_off_or_wrong_request_is_invalid);
    RUN_TEST(
        test_raw_p_above_maximum_is_checked_and_saturated_without_p_preclamp);
    RUN_TEST(test_raw_p_equal_to_maximum_has_zero_headroom);
    RUN_TEST(test_second_raw_p_saturation_skips_overflowing_positive_delta_i);
    RUN_TEST(
        test_current_integral_headroom_saturation_skips_overflowing_positive_delta_i);
    RUN_TEST(
        test_integral_headroom_caps_exact_and_oversized_steps_and_reduces_existing_i);
    RUN_TEST(test_lifecycle_boundary_resets_pi_and_qualification_ram);
    RUN_TEST(test_product_air_limit_reduces_and_blocks_normal_demand);
    RUN_TEST(
        test_product_air_limit_blocking_returns_idle_for_both_pi_directions);
    RUN_TEST(test_product_air_limit_boundaries_cover_both_directions);
    RUN_TEST(test_air_limit_reduced_and_blocked_never_charge_integrator);
    RUN_TEST(test_air_limit_does_not_charge_integrator);
    RUN_TEST(
        test_committed_transition_applies_once_and_idle_does_not_consume_it);
    RUN_TEST(test_product_mode_requires_air_and_product_without_role_fallback);
    RUN_TEST(test_air_mode_does_not_require_product);
    RUN_TEST(test_air_mode_sensor_failures_are_not_applied_air_limits);
    RUN_TEST(test_air_mode_early_diagnostics_keep_air_limit_not_applied);
    RUN_TEST(
        test_sensor_role_transition_resets_integral_on_first_product_sample);
    RUN_TEST(test_invalid_sensor_sample_clears_old_pi_state_before_recovery);
    RUN_TEST(test_large_gap_resets_and_uses_current_timestamp_as_new_anchor);
    RUN_TEST(test_timestamp_can_repeat_but_request_identity_cannot);
    RUN_TEST(test_near_overflow_pi_timestamps_use_checked_delta);
    RUN_TEST(test_backward_pi_timestamp_is_invalid_and_clears_state);
    RUN_TEST(test_feedback_replay_and_all_dispositions_fail_closed);
    RUN_TEST(test_unknown_feedback_disposition_is_fail_closed);
    RUN_TEST(test_direction_sequences_cover_idle_and_opposite_demand);
    RUN_TEST(test_direction_changes_reset_or_bound_carry_exactly_once);
    RUN_TEST(test_invalid_parameters_are_unavailable_without_a_request);
    RUN_TEST(test_request_identity_does_not_wrap_at_uint64_max);
    RUN_TEST(test_request_identity_exhaustion_clears_nonempty_idle_state);
    RUN_TEST(
        test_request_identity_exhaustion_idle_path_clears_nonempty_pi_directly);
    RUN_TEST(
        test_valid_quality_without_filtered_value_is_fail_closed_not_raw_fallback);
    RUN_TEST(test_stale_quality_with_old_filtered_value_is_unavailable);
    RUN_TEST(test_recovery_after_invalid_evidence_starts_a_clean_pi_run);
    RUN_TEST(test_initial_request_sequence_zero_is_normalized_to_never_appear);
    return UNITY_END();
}
