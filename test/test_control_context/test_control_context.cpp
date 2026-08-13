#include <unity.h>

#include "control_context.hpp"
#include "run_commands.hpp"
#include "standard_program_catalog.hpp"

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

RunCommandState projectedProgramState(ProcessState phase) {
    auto document = FactoryProgramCatalog::find("water-kefir");
    TEST_ASSERT_TRUE(document.has_value());
    document->program.fermentationStages.front().targetTemperatureCelsius =
        38.0;
    document->program.fermentationStages.front().durationMinutes = 120U;
    document->program.productSensorFailure.fallbackDelaySeconds = 30U;
    document->program.targetQualification.bandCelsius = 0.5;
    document->program.targetQualification.durationMinutes = 10U;
    document->program.maximumTargetReachMinutes = 180U;
    document->program.completion.mode = CompletionMode::CoolThenFinish;
    document->program.completion.coolingTargetCelsius = 9.0;
    // Keep the snapshot structurally consistent with the requested phase
    // (stateMatchesRunSnapshot()/validateProcessRunSnapshot()): Preheating
    // requires preheatEnabled with a matching product-wait configuration.
    document->program.preheat = phase == ProcessState::Preheating;
    document->program.maximumProductWaitMinutes =
        phase == ProcessState::Preheating ? std::optional<std::uint32_t>{30U}
                                          : std::nullopt;
    TEST_ASSERT_TRUE(validateProgram(*document).valid());
    const auto run =
        ActiveRun::start(*document, ProgramSourceKind::FactoryCatalog, 1U);
    TEST_ASSERT_TRUE(run.has_value());

    RunCommandState state;
    state.processState.state = phase;
    state.activeProgramRun = *run;
    state.activeRunId = "projected-program";
    state.activeRunSensorMode = RunSensorMode::Product;
    state.processRunSnapshot = makeProcessRunSnapshot(*state.activeProgramRun);
    TEST_ASSERT_TRUE(state.processRunSnapshot.has_value());
    state.runRevision = 4U;
    state.processState.transitionSequence = 8U;
    return state;
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

void test_live_state_projection_uses_effective_targets_and_completion_target() {
    auto state = projectedProgramState(ProcessState::Preheating);
    auto context = resolveEffectiveControlContext(state);
    TEST_ASSERT_TRUE(context.valid);
    TEST_ASSERT_TRUE(context.controlSensorRole == ControlSensorRole::Air);
    TEST_ASSERT_TRUE(context.target.targetKind ==
                     ControlTargetKind::FermentationRun);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 38.0, context.target.targetCelsius);

    RunAdjustmentRequest adjustment;
    adjustment.targetTemperatureCelsius = 39.0;
    adjustment.confirmed = true;
    adjustment.timestamp = {100U, std::nullopt};
    const auto decision = state.activeProgramRun->decideAdjustment(
        adjustment,
        RunAdjustmentContext{true, true, 0U, 0U,
                             RunAdjustmentPhaseContext::BeforeFermentation});
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(
        state.activeProgramRun->applyAdjustment(decision).applied());
    context = resolveEffectiveControlContext(state);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 39.0, context.target.targetCelsius);

    state.processState.state = ProcessState::ReachingTarget;
    context = resolveEffectiveControlContext(state);
    TEST_ASSERT_TRUE(context.controlSensorRole == ControlSensorRole::Product);

    state.processState.state = ProcessState::Cooling;
    context = resolveEffectiveControlContext(state);
    TEST_ASSERT_TRUE(context.valid);
    TEST_ASSERT_TRUE(context.target.targetKind ==
                     ControlTargetKind::CoolingCompletion);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 9.0, context.target.targetCelsius);

    RunCommandState manual;
    manual.processState.state = ProcessState::ManualHolding;
    manual.activeRunId = "projected-manual";
    manual.activeRunSensorMode = RunSensorMode::Air;
    ManualRunPlan plan;
    plan.values.runId = manual.activeRunId;
    plan.values.targetTemperatureCelsius = 12.0;
    plan.values.sensorMode = RunSensorMode::Air;
    plan.values.qualificationBandCelsius = 0.5;
    plan.values.qualificationDurationMinutes = 10U;
    plan.values.maximumTargetReachMinutes = 180U;
    manual.activeManualRun = plan;
    manual.processRunSnapshot =
        ProcessRunSnapshot{ProcessKind::ManualHolding,
                           false,
                           CompletionMode::FinishWithoutCooling,
                           10U,
                           180U,
                           std::nullopt,
                           std::nullopt,
                           std::nullopt};
    context = resolveEffectiveControlContext(manual);
    TEST_ASSERT_TRUE(context.valid);
    TEST_ASSERT_TRUE(context.target.targetKind == ControlTargetKind::ManualRun);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 12.0, context.target.targetCelsius);
}

RunCommandState manualRunState(ProcessState phase, bool preheatEnabled) {
    RunCommandState state;
    state.processState.state = phase;
    state.activeRunId = "manual-run";
    state.activeRunSensorMode = RunSensorMode::Product;
    ManualRunPlan plan;
    plan.values.runId = state.activeRunId;
    plan.values.targetTemperatureCelsius = 18.0;
    plan.values.sensorMode = RunSensorMode::Product;
    plan.values.preheatEnabled = preheatEnabled;
    plan.values.maximumProductWaitMinutes =
        preheatEnabled ? std::optional<std::uint32_t>{30U} : std::nullopt;
    plan.values.qualificationBandCelsius = 0.5;
    plan.values.qualificationDurationMinutes = 10U;
    plan.values.maximumTargetReachMinutes = 180U;
    state.activeManualRun = plan;
    state.processRunSnapshot = makeProcessRunSnapshot(plan);
    state.runRevision = 2U;
    state.processState.transitionSequence = 5U;
    return state;
}

void test_manual_run_target_resolves_before_manual_holding() {
    // FR5: PREHEATING/WAITING_FOR_PRODUCT/REACHING_TARGET/QUALIFYING_TARGET
    // must resolve the ManualRunPlan target, not
    // effectiveFermentationTargetCelsius (which a ManualRun never sets).
    for (const auto phase :
         {ProcessState::WaitingForProduct, ProcessState::ReachingTarget,
          ProcessState::QualifyingTarget}) {
        const auto state = manualRunState(phase, true);
        const auto context = resolveEffectiveControlContext(state);
        TEST_ASSERT_TRUE(context.valid);
        TEST_ASSERT_TRUE(context.target.targetKind ==
                         ControlTargetKind::ManualRun);
        TEST_ASSERT_DOUBLE_WITHIN(0.0001, 18.0, context.target.targetCelsius);
    }

    const auto preheating = manualRunState(ProcessState::Preheating, true);
    const auto preheatingContext = resolveEffectiveControlContext(preheating);
    TEST_ASSERT_TRUE(preheatingContext.valid);
    TEST_ASSERT_TRUE(preheatingContext.controlSensorRole ==
                     ControlSensorRole::Air);
    TEST_ASSERT_TRUE(preheatingContext.target.targetKind ==
                     ControlTargetKind::ManualRun);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 18.0,
                              preheatingContext.target.targetCelsius);

    const auto waiting = manualRunState(ProcessState::WaitingForProduct, true);
    const auto waitingContext = resolveEffectiveControlContext(waiting);
    TEST_ASSERT_TRUE(waitingContext.valid);
    TEST_ASSERT_TRUE(waitingContext.controlSensorRole ==
                     ControlSensorRole::Air);

    const auto reaching = manualRunState(ProcessState::ReachingTarget, false);
    const auto reachingContext = resolveEffectiveControlContext(reaching);
    TEST_ASSERT_TRUE(reachingContext.valid);
    TEST_ASSERT_TRUE(reachingContext.controlSensorRole ==
                     ControlSensorRole::Product);
}

void test_manual_run_in_fermenting_is_structurally_rejected() {
    // FR5: a ManualRun snapshot can never legitimately reach FERMENTING; the
    // corrected phase-based target resolution must not legitimize it.
    auto input = baseInput(ProcessState::Fermenting);
    input.manualRun = true;
    TEST_ASSERT_FALSE(resolveEffectiveControlContext(input).valid);
}

void test_qualification_context_binds_band_and_duration_to_active_run() {
    // FR4: band and duration come from the immutable run/qualification
    // contract (program or ManualRunPlan), not a freely suppliable value.
    auto programState = projectedProgramState(ProcessState::Preheating);
    const auto programContext =
        resolveEffectiveQualificationContext(programState);
    TEST_ASSERT_TRUE(programContext.valid);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.5, programContext.bandCelsius);
    TEST_ASSERT_EQUAL_UINT64(600'000U,
                             programContext.qualificationDurationMillis);

    const auto manualState =
        manualRunState(ProcessState::ReachingTarget, false);
    const auto manualContext =
        resolveEffectiveQualificationContext(manualState);
    TEST_ASSERT_TRUE(manualContext.valid);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.5, manualContext.bandCelsius);
    TEST_ASSERT_EQUAL_UINT64(600'000U,
                             manualContext.qualificationDurationMillis);

    // FERMENTING/COOLING/etc. have no active qualification episode.
    auto fermenting = programState;
    fermenting.processState.state = ProcessState::Fermenting;
    TEST_ASSERT_FALSE(resolveEffectiveQualificationContext(fermenting).valid);
}

void test_live_state_projection_rejects_stale_snapshot_and_manual_identity() {
    auto program = projectedProgramState(ProcessState::Preheating);
    TEST_ASSERT_TRUE(resolveEffectiveControlContext(program).valid);
    TEST_ASSERT_TRUE(resolveEffectiveQualificationContext(program).valid);
    program.processRunSnapshot->qualificationDurationMinutes = 5U;
    TEST_ASSERT_FALSE(resolveEffectiveControlContext(program).valid);
    TEST_ASSERT_FALSE(resolveEffectiveQualificationContext(program).valid);

    auto manual = manualRunState(ProcessState::ReachingTarget, false);
    TEST_ASSERT_TRUE(resolveEffectiveControlContext(manual).valid);
    TEST_ASSERT_TRUE(resolveEffectiveQualificationContext(manual).valid);
    manual.processRunSnapshot->qualificationDurationMinutes = 5U;
    TEST_ASSERT_FALSE(resolveEffectiveControlContext(manual).valid);
    TEST_ASSERT_FALSE(resolveEffectiveQualificationContext(manual).valid);

    manual = manualRunState(ProcessState::ReachingTarget, false);
    manual.activeRunSensorMode = RunSensorMode::Air;
    TEST_ASSERT_FALSE(resolveEffectiveControlContext(manual).valid);

    manual = manualRunState(ProcessState::ReachingTarget, false);
    manual.activeRunId = "foreign-manual-run";
    TEST_ASSERT_FALSE(resolveEffectiveControlContext(manual).valid);

    const auto canonical = manualRunState(ProcessState::ReachingTarget, false);
    TEST_ASSERT_TRUE(resolveEffectiveControlContext(canonical).valid);
    TEST_ASSERT_TRUE(resolveEffectiveQualificationContext(canonical).valid);
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
    RUN_TEST(test_manual_run_target_resolves_before_manual_holding);
    RUN_TEST(test_manual_run_in_fermenting_is_structurally_rejected);
    RUN_TEST(test_qualification_context_binds_band_and_duration_to_active_run);
    RUN_TEST(
        test_live_state_projection_rejects_stale_snapshot_and_manual_identity);
    RUN_TEST(
        test_live_state_projection_uses_effective_targets_and_completion_target);
    return UNITY_END();
}
