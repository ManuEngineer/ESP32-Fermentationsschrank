#include <unity.h>

#include "process_state_machine.hpp"

namespace {

using namespace fermentation;

ProcessRunSnapshot snapshot() {
    ProcessRunSnapshot result;
    result.kind = ProcessKind::Timed;
    result.preheatEnabled = false;
    result.completionMode = CompletionMode::FinishWithoutCooling;
    result.qualificationDurationMinutes = 1U;
    result.maximumTargetReachMinutes = 30U;
    result.fermentationDurationMinutes = 10U;
    return result;
}

void test_reaching_complete_only_enters_qualifying() {
    const auto run = snapshot();
    ProcessRuntimeState current;
    current.state = ProcessState::ReachingTarget;
    current.targetReachStartedAtMillis = 0U;
    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::Complete;
    const auto decision =
        decideProcessTransition(current, &run, signals, {}, 100U);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(decision.after.state == ProcessState::QualifyingTarget);
    TEST_ASSERT_TRUE(decision.reason ==
                     TransitionReason::QualificationTrackingStarted);
}

void test_qualifying_in_band_does_not_bypass_complete_progress() {
    const auto run = snapshot();
    ProcessRuntimeState current;
    current.state = ProcessState::QualifyingTarget;
    current.targetReachStartedAtMillis = 0U;
    current.qualificationValidSinceMillis = 0U;
    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::InBand;
    const auto decision =
        decideProcessTransition(current, &run, signals, {}, 61'000U);
    TEST_ASSERT_TRUE(decision.status == DecisionStatus::NoTransition);
}

void test_qualifying_complete_enters_fermenting() {
    const auto run = snapshot();
    ProcessRuntimeState current;
    current.state = ProcessState::QualifyingTarget;
    current.targetReachStartedAtMillis = 0U;
    current.qualificationValidSinceMillis = 0U;
    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::Complete;
    const auto decision =
        decideProcessTransition(current, &run, signals, {}, 61'000U);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(decision.after.state == ProcessState::Fermenting);
}

void test_cooling_uses_dedicated_signal_not_qualification_progress() {
    auto run = snapshot();
    run.completionMode = CompletionMode::CoolThenFinish;
    ProcessRuntimeState current;
    current.state = ProcessState::Cooling;
    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::Complete;
    const auto notCooling =
        decideProcessTransition(current, &run, signals, {}, 100U);
    TEST_ASSERT_TRUE(notCooling.status == DecisionStatus::NoTransition);

    signals.coolingTargetConditionValid = true;
    const auto cooling =
        decideProcessTransition(current, &run, signals, {}, 100U);
    TEST_ASSERT_TRUE(cooling.proposed());
    TEST_ASSERT_TRUE(cooling.after.state == ProcessState::Completed);
}

void test_process_apply_decisions_expose_only_fluid_context_commit_notice() {
    auto run = snapshot();
    run.preheatEnabled = true;
    run.maximumProductWaitMinutes = 10U;
    ProcessRuntimeState waiting;
    waiting.state = ProcessState::WaitingForProduct;
    const auto inserted = decideProcessTransition(
        waiting, &run, {},
        {ProcessEvent::ProductInsertedConfirmed, std::nullopt}, 100U);
    TEST_ASSERT_TRUE(inserted.proposed());
    TEST_ASSERT_TRUE(inserted.committedControlContextTransition.has_value());
    TEST_ASSERT_TRUE(*inserted.committedControlContextTransition ==
                     CommittedControlContextTransition::ProductInserted);

    ProcessRuntimeState reaching;
    reaching.state = ProcessState::ReachingTarget;
    reaching.targetReachStartedAtMillis = 0U;
    const auto changed = decideProcessTransition(
        reaching, &run, {}, {ProcessEvent::TargetChanged, std::nullopt}, 100U);
    TEST_ASSERT_TRUE(changed.proposed());
    TEST_ASSERT_TRUE(changed.committedControlContextTransition.has_value());
    TEST_ASSERT_TRUE(*changed.committedControlContextTransition ==
                     CommittedControlContextTransition::TargetContextChange);

    ProcessRuntimeState fermenting;
    fermenting.state = ProcessState::Fermenting;
    fermenting.stateEnteredAtMillis = 0U;
    ProcessRunSnapshot coolingRun = run;
    coolingRun.completionMode = CompletionMode::CoolThenFinish;
    coolingRun.fermentationDurationMinutes = 1U;
    const auto cooling =
        decideProcessTransition(fermenting, &coolingRun, {},
                                {ProcessEvent::None, std::nullopt}, 60'000U);
    TEST_ASSERT_TRUE(cooling.proposed());
    TEST_ASSERT_TRUE(cooling.after.state == ProcessState::Cooling);
    TEST_ASSERT_TRUE(cooling.committedControlContextTransition.has_value());
    TEST_ASSERT_TRUE(
        *cooling.committedControlContextTransition ==
        CommittedControlContextTransition::CoolingTargetContextChange);
}

}  // namespace

void setup() {}
void loop() {}

int main(int argc, char** argv) {
    static_cast<void>(argc);
    static_cast<void>(argv);
    UNITY_BEGIN();
    RUN_TEST(test_reaching_complete_only_enters_qualifying);
    RUN_TEST(test_qualifying_in_band_does_not_bypass_complete_progress);
    RUN_TEST(test_qualifying_complete_enters_fermenting);
    RUN_TEST(test_cooling_uses_dedicated_signal_not_qualification_progress);
    RUN_TEST(
        test_process_apply_decisions_expose_only_fluid_context_commit_notice);
    return UNITY_END();
}
