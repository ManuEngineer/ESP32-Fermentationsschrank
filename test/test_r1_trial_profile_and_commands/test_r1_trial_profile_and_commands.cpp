#include <unity.h>

#include <string>

#include "../../main/r1_control_trial_profile.hpp"
#include "r1_trial_commands.hpp"
#include "temperature_control.hpp"

// R1 trial plan S5: the compile-time trial profile is technically valid (and
// only a proposal), and the console grammar is strict.

namespace {

using namespace fermentation;

void test_trial_pi_parameters_are_valid_with_positive_ki_everywhere() {
    const auto parameters = r1_trial::trialControlChainParameters();
    TEST_ASSERT_TRUE(classifyTemperatureControlParameters(parameters.pi) ==
                     TemperatureControlParametersValidation::Valid);
    for (const auto* set :
         {&parameters.pi.airHeating, &parameters.pi.airCooling,
          &parameters.pi.productHeating, &parameters.pi.productCooling}) {
        TEST_ASSERT_TRUE(set->integralGainQuotePerCelsiusSecond > 0.0);
        TEST_ASSERT_TRUE(set->maximumQuote > 0.0 && set->maximumQuote <= 1.0);
    }
    // The product default stays unconfigured (no trial value leaks into it).
    TEST_ASSERT_TRUE(classifyTemperatureControlParameters(
                         kControlChainParametersUnconfiguredUntilIssue35.pi) ==
                     TemperatureControlParametersValidation::Unconfigured);
}

void test_trial_quality_configs_are_valid_and_the_cooling_range_is_wider() {
    const auto configs = r1_trial::trialSensorQualityConfigs();
    TEST_ASSERT_TRUE(configs.air.has_value());
    TEST_ASSERT_TRUE(configs.cooling.has_value());
    TEST_ASSERT_TRUE(configs.product.has_value());
    TEST_ASSERT_TRUE(configs.cooling->maxPlausibleCelsius() >
                     configs.air->maxPlausibleCelsius());
}

void test_commands_parse_strictly() {
    TEST_ASSERT_TRUE(parseR1TrialCommand("trial status").kind ==
                     R1TrialCommandKind::Status);
    TEST_ASSERT_TRUE(parseR1TrialCommand("  planner   show ").kind ==
                     R1TrialCommandKind::PlannerShow);
    TEST_ASSERT_TRUE(parseR1TrialCommand("planner clear").kind ==
                     R1TrialCommandKind::PlannerClear);
    TEST_ASSERT_TRUE(parseR1TrialCommand("thermal show").kind ==
                     R1TrialCommandKind::ThermalShow);
    TEST_ASSERT_TRUE(parseR1TrialCommand("thermal clear").kind ==
                     R1TrialCommandKind::ThermalClear);
    for (const char* rejected :
         {"", "   ", "status", "trial", "trial status now", "planner",
          "planner set", "planner show extra", "thermal set 1 2 3",
          "unknown command", "planner  SET 1 2 3 4 5 0.5 6 7 8 9"}) {
        TEST_ASSERT_TRUE(parseR1TrialCommand(rejected).kind ==
                         R1TrialCommandKind::Rejected);
    }
}

void test_planner_set_parses_all_ten_fields() {
    const auto command = parseR1TrialCommand(
        "planner set 30000 3000 2000 5000 20000 0.5 4000 60000 10000 2000");
    TEST_ASSERT_TRUE(command.kind == R1TrialCommandKind::PlannerSet);
    TEST_ASSERT_EQUAL_UINT64(30000U, command.planner.switchingWindowMillis);
    TEST_ASSERT_EQUAL_UINT64(3000U, command.planner.minimumOnMillis);
    TEST_ASSERT_EQUAL_UINT64(2000U, command.planner.minimumOffMillis);
    TEST_ASSERT_EQUAL_UINT64(5000U, command.planner.polarityDeadTimeMillis);
    TEST_ASSERT_EQUAL_UINT64(20000U, command.planner.pulseAccumulatorCapMillis);
    TEST_ASSERT_EQUAL_DOUBLE(
        0.5, command.planner.counterDirectionConfirmationQuoteThreshold);
    TEST_ASSERT_EQUAL_UINT64(
        4000U, command.planner.counterDirectionConfirmationDurationMillis);
    TEST_ASSERT_EQUAL_UINT64(60000U, command.planner.requestWatchdogMillis);
    TEST_ASSERT_EQUAL_UINT64(10000U, command.planner.outerFanPostRunMillis);
    TEST_ASSERT_EQUAL_UINT64(2000U, command.planner.innerFanPostRunMillis);
}

void test_planner_set_rejects_malformed_numbers() {
    for (const char* bad :
         {"planner set 30000 3000 2000 5000 20000 0.5 4000 60000 10000",
          "planner set 30000 3000 2000 5000 20000 0.5 4000 60000 10000 2000 1",
          "planner set -1 3000 2000 5000 20000 0.5 4000 60000 10000 2000",
          "planner set 3e4 3000 2000 5000 20000 0.5 4000 60000 10000 2000",
          "planner set 0x10 3000 2000 5000 20000 0.5 4000 60000 10000 2000",
          "planner set 30000 3000 2000 5000 20000 nan 4000 60000 10000 2000",
          "planner set 30000 3000 2000 5000 20000 inf 4000 60000 10000 2000",
          "planner set 30000 3000 2000 5000 20000 0.5x 4000 60000 10000 2000",
          "planner set 30000 3000 2000 5000 20000 0.5 4000 60000 10000 2.5"}) {
        TEST_ASSERT_TRUE(parseR1TrialCommand(bad).kind ==
                         R1TrialCommandKind::Rejected);
    }
}

void test_thermal_set_parses_all_twelve_fields() {
    const auto command = parseR1TrialCommand(
        "thermal set -10 0 40 50 -20 -10 60 70 6 60000 5000 20000");
    TEST_ASSERT_TRUE(command.kind == R1TrialCommandKind::ThermalSet);
    const auto& l = command.thermal;
    TEST_ASSERT_EQUAL_DOUBLE(-10.0, l.airEmergencyLowerCelsius);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, l.airInterventionLowerCelsius);
    TEST_ASSERT_EQUAL_DOUBLE(40.0, l.airInterventionUpperCelsius);
    TEST_ASSERT_EQUAL_DOUBLE(50.0, l.airEmergencyUpperCelsius);
    TEST_ASSERT_EQUAL_DOUBLE(-20.0, l.coolingEmergencyLowerCelsius);
    TEST_ASSERT_EQUAL_DOUBLE(-10.0, l.coolingInterventionLowerCelsius);
    TEST_ASSERT_EQUAL_DOUBLE(60.0, l.coolingInterventionUpperCelsius);
    TEST_ASSERT_EQUAL_DOUBLE(70.0, l.coolingEmergencyUpperCelsius);
    TEST_ASSERT_EQUAL_DOUBLE(6.0, l.coolingMaxRiseCelsiusPerMinute);
    TEST_ASSERT_EQUAL_UINT64(60000U, l.trendWindowMillis);
    TEST_ASSERT_EQUAL_UINT64(5000U, l.maxSampleAgeMillis);
    TEST_ASSERT_EQUAL_UINT64(20000U, l.invalidLatchMillis);
    TEST_ASSERT_TRUE(validateThermalSafetyLimits(l));
}

void test_thermal_set_rejects_malformed_numbers() {
    for (const char* bad :
         {"thermal set -10 0 40 50 -20 -10 60 70 6 60000 5000",
          "thermal set -10 0 40 50 -20 -10 60 70 6 60000 5000 20000 1",
          "thermal set -10 0 40 50 -20 -10 60 70 6 60.5 5000 20000",
          "thermal set -10 0 40 50 -20 -10 60 70 6 -60000 5000 20000",
          "thermal set -10 0 40 50 -20 -10 60 70 abc 60000 5000 20000",
          "thermal set -10 0 40 50 -20 -10 60 70 1e1 60000 5000 20000",
          "thermal set -10 0 40 50 -20 -10 60 70 -- 60000 5000 20000"}) {
        TEST_ASSERT_TRUE(parseR1TrialCommand(bad).kind ==
                         R1TrialCommandKind::Rejected);
    }
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_trial_pi_parameters_are_valid_with_positive_ki_everywhere);
    RUN_TEST(
        test_trial_quality_configs_are_valid_and_the_cooling_range_is_wider);
    RUN_TEST(test_commands_parse_strictly);
    RUN_TEST(test_planner_set_parses_all_ten_fields);
    RUN_TEST(test_planner_set_rejects_malformed_numbers);
    RUN_TEST(test_thermal_set_parses_all_twelve_fields);
    RUN_TEST(test_thermal_set_rejects_malformed_numbers);
    return UNITY_END();
}
