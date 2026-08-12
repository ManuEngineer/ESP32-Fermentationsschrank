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
    const auto decision = decideProcessTransition(
        current, &run, signals, {}, 100U);
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
    const auto decision = decideProcessTransition(
        current, &run, signals, {}, 61'000U);
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
    const auto decision = decideProcessTransition(
        current, &run, signals, {}, 61'000U);
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
    const auto notCooling = decideProcessTransition(
        current, &run, signals, {}, 100U);
    TEST_ASSERT_TRUE(notCooling.status == DecisionStatus::NoTransition);

    signals.coolingTargetConditionValid = true;
    const auto cooling = decideProcessTransition(
        current, &run, signals, {}, 100U);
    TEST_ASSERT_TRUE(cooling.proposed());
    TEST_ASSERT_TRUE(cooling.after.state == ProcessState::Completed);
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
    return UNITY_END();
}
