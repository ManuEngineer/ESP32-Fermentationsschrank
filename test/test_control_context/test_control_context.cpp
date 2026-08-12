#include <unity.h>

#include "control_context.hpp"

namespace {

using namespace fermentation;

EffectiveControlContextInput baseInput(ProcessState phase) {
    EffectiveControlContextInput input;
    input.phase = phase;
    input.activeRunSensorMode = RunSensorMode::Product;
    input.effectiveFermentationTargetCelsius = 24.0;
    input.completionCoolingTargetCelsius = 8.0;
    input.manualTargetCelsius = 22.0;
    input.completionMode = CompletionMode::CoolThenFinish;
    input.processTransitionSequence = 7U;
    input.runRevision = 3U;
    return input;
}

void test_product_preheating_uses_air_without_changing_run_mode() {
    const auto context =
        resolveEffectiveControlContext(baseInput(ProcessState::Preheating));
    TEST_ASSERT_TRUE(context.valid);
    TEST_ASSERT_TRUE(context.controlSensorRole == ControlSensorRole::Air);
    TEST_ASSERT_TRUE(context.target.targetKind ==
                     ControlTargetKind::FermentationRun);
    TEST_ASSERT_EQUAL_UINT32(3U, context.target.sourceRevision);
}

void test_product_waiting_uses_air_and_product_after_insertion() {
    const auto waiting = resolveEffectiveControlContext(
        baseInput(ProcessState::WaitingForProduct));
    TEST_ASSERT_TRUE(waiting.valid);
    TEST_ASSERT_TRUE(waiting.controlSensorRole == ControlSensorRole::Air);

    const auto reaching =
        resolveEffectiveControlContext(baseInput(ProcessState::ReachingTarget));
    TEST_ASSERT_TRUE(reaching.valid);
    TEST_ASSERT_TRUE(reaching.controlSensorRole == ControlSensorRole::Product);
}

void test_air_run_stays_air_in_all_run_phases() {
    for (const auto phase :
         {ProcessState::Preheating, ProcessState::WaitingForProduct,
          ProcessState::ReachingTarget, ProcessState::QualifyingTarget,
          ProcessState::Fermenting, ProcessState::Cooling,
          ProcessState::CoolHolding, ProcessState::ManualHolding}) {
        auto input = baseInput(phase);
        input.activeRunSensorMode = RunSensorMode::Air;
        if (phase == ProcessState::ManualHolding) input.manualRun = true;
        const auto context = resolveEffectiveControlContext(input);
        TEST_ASSERT_TRUE(context.valid);
        TEST_ASSERT_TRUE(context.controlSensorRole == ControlSensorRole::Air);
    }
}

void test_product_run_switches_from_air_to_product_only_after_waiting() {
    for (const auto phase :
         {ProcessState::Preheating, ProcessState::WaitingForProduct}) {
        const auto context = resolveEffectiveControlContext(baseInput(phase));
        TEST_ASSERT_TRUE(context.valid);
        TEST_ASSERT_TRUE(context.controlSensorRole == ControlSensorRole::Air);
    }
    for (const auto phase :
         {ProcessState::ReachingTarget, ProcessState::QualifyingTarget,
          ProcessState::Fermenting, ProcessState::Cooling,
          ProcessState::CoolHolding, ProcessState::ManualHolding}) {
        auto input = baseInput(phase);
        if (phase == ProcessState::ManualHolding) input.manualRun = true;
        const auto context = resolveEffectiveControlContext(input);
        TEST_ASSERT_TRUE(context.valid);
        TEST_ASSERT_TRUE(context.controlSensorRole ==
                         ControlSensorRole::Product);
    }
}

void test_product_inserted_transition_depends_on_resolved_roles() {
    TEST_ASSERT_TRUE(resolveProductInsertedControlContextTransition(
                         ControlSensorRole::Air, ControlSensorRole::Product)
                         .has_value());
    TEST_ASSERT_FALSE(resolveProductInsertedControlContextTransition(
                          ControlSensorRole::Air, ControlSensorRole::Air)
                          .has_value());
}

void test_cooling_uses_completion_target_only() {
    auto input = baseInput(ProcessState::Cooling);
    input.effectiveFermentationTargetCelsius = 25.0;
    const auto context = resolveEffectiveControlContext(input);
    TEST_ASSERT_TRUE(context.valid);
    TEST_ASSERT_TRUE(context.target.targetKind ==
                     ControlTargetKind::CoolingCompletion);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 8.0, context.target.targetCelsius);

    input.completionMode = CompletionMode::FinishWithoutCooling;
    TEST_ASSERT_FALSE(resolveEffectiveControlContext(input).valid);
}

void test_manual_holding_uses_manual_target() {
    auto input = baseInput(ProcessState::ManualHolding);
    input.manualRun = true;
    const auto context = resolveEffectiveControlContext(input);
    TEST_ASSERT_TRUE(context.valid);
    TEST_ASSERT_TRUE(context.target.targetKind == ControlTargetKind::ManualRun);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 22.0, context.target.targetCelsius);
}

void test_non_control_phase_has_no_context() {
    auto input = baseInput(ProcessState::Standby);
    TEST_ASSERT_FALSE(resolveEffectiveControlContext(input).valid);
}

void test_invalid_run_sensor_mode_does_not_fallback_to_air() {
    auto input = baseInput(ProcessState::Fermenting);
    input.activeRunSensorMode = static_cast<RunSensorMode>(0xFFU);
    const auto context = resolveEffectiveControlContext(input);
    TEST_ASSERT_FALSE(context.valid);
}

}  // namespace

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    static_cast<void>(argc);
    static_cast<void>(argv);
    UNITY_BEGIN();
    RUN_TEST(test_product_preheating_uses_air_without_changing_run_mode);
    RUN_TEST(test_product_waiting_uses_air_and_product_after_insertion);
    RUN_TEST(test_air_run_stays_air_in_all_run_phases);
    RUN_TEST(test_product_run_switches_from_air_to_product_only_after_waiting);
    RUN_TEST(test_product_inserted_transition_depends_on_resolved_roles);
    RUN_TEST(test_cooling_uses_completion_target_only);
    RUN_TEST(test_manual_holding_uses_manual_target);
    RUN_TEST(test_non_control_phase_has_no_context);
    RUN_TEST(test_invalid_run_sensor_mode_does_not_fallback_to_air);
    return UNITY_END();
}
