#include <unity.h>

#include <array>
#include <cstdint>
#include <limits>

#include "process_state_machine.hpp"
#include "standard_program_catalog.hpp"
#include "virtual_time_source.hpp"

namespace {

using device_platform::VirtualTimeSource;
using fermentation::ActiveRun;
using fermentation::CompletionMode;
using fermentation::DecisionStatus;
using fermentation::FactoryProgramCatalog;
using fermentation::PriorBootPhaseElapsed;
using fermentation::ProcessEvent;
using fermentation::ProcessKind;
using fermentation::ProcessMessage;
using fermentation::ProcessRunSnapshot;
using fermentation::ProcessRuntimeState;
using fermentation::ProcessSignals;
using fermentation::ProcessState;
using fermentation::ProgramDocument;
using fermentation::ProgramSourceKind;
using fermentation::QualificationProgress;
using fermentation::TransitionDecision;
using fermentation::TransitionReason;
using fermentation::TransitionRequest;

constexpr std::uint64_t kMinuteMillis = 60000U;

ProgramDocument commissionFactoryProgram(const char* id) {
    auto document = FactoryProgramCatalog::find(id);
    TEST_ASSERT_TRUE(document.has_value());
    auto& program = document->program;
    program.productSensorFailure.fallbackDelaySeconds = 30U;
    program.fermentationStages.front().targetTemperatureCelsius = 38.0;
    program.fermentationStages.front().durationMinutes = 2U;
    program.targetQualification.bandCelsius = 0.5;
    program.targetQualification.durationMinutes = 1U;
    program.maximumTargetReachMinutes = 2U;
    if (program.preheat) {
        program.maximumProductWaitMinutes = 3U;
    }
    if (program.completion.mode != CompletionMode::FinishWithoutCooling) {
        program.completion.coolingTargetCelsius = 8.0;
    }
    TEST_ASSERT_TRUE(validateProgram(*document).valid());
    return *document;
}

ProcessRunSnapshot makeFactorySnapshot(const char* id) {
    const auto document = commissionFactoryProgram(id);
    const auto run =
        ActiveRun::start(document, ProgramSourceKind::FactoryCatalog, 1U);
    TEST_ASSERT_TRUE(run.has_value());
    const auto snapshot = fermentation::makeProcessRunSnapshot(*run);
    TEST_ASSERT_TRUE(snapshot.has_value());
    return *snapshot;
}

ProcessRunSnapshot makeTimedSnapshot(
    bool preheat,
    CompletionMode completionMode = CompletionMode::FinishWithoutCooling) {
    ProcessRunSnapshot snapshot;
    snapshot.kind = ProcessKind::Timed;
    snapshot.preheatEnabled = preheat;
    snapshot.completionMode = completionMode;
    snapshot.qualificationDurationMinutes = 1U;
    snapshot.maximumTargetReachMinutes = 2U;
    if (preheat) {
        snapshot.maximumProductWaitMinutes = 3U;
    }
    snapshot.fermentationDurationMinutes = 2U;
    if (completionMode == CompletionMode::CoolAndHoldForDuration) {
        snapshot.holdDurationMinutes = 2U;
    }
    TEST_ASSERT_TRUE(fermentation::validateProcessRunSnapshot(snapshot));
    return snapshot;
}

ProcessRunSnapshot makeManualHoldingSnapshot(bool preheat) {
    auto snapshot = makeTimedSnapshot(preheat);
    snapshot.kind = ProcessKind::ManualHolding;
    snapshot.fermentationDurationMinutes = std::nullopt;
    TEST_ASSERT_TRUE(fermentation::validateProcessRunSnapshot(snapshot));
    return snapshot;
}

bool containsMessage(const TransitionDecision& decision,
                     ProcessMessage message) {
    for (std::size_t index = 0U; index < decision.messageCount; ++index) {
        if (decision.messages[index] == message) {
            return true;
        }
    }
    return false;
}

TransitionDecision decide(const ProcessRuntimeState& state,
                          const ProcessRunSnapshot* snapshot,
                          const VirtualTimeSource& time,
                          ProcessSignals signals = {},
                          ProcessEvent event = ProcessEvent::None) {
    return fermentation::decideProcessTransition(state, snapshot, signals,
                                                 {event, std::nullopt},
                                                 time.monotonicMillis());
}

TransitionDecision decideRecovery(const ProcessRuntimeState& state,
                                  const ProcessRunSnapshot* snapshot,
                                  const VirtualTimeSource& time,
                                  const ProcessRuntimeState& recoveredState) {
    TransitionRequest request;
    request.event = ProcessEvent::RecoveryResume;
    request.recoveredState = recoveredState;
    return fermentation::decideProcessTransition(state, snapshot, {}, request,
                                                 time.monotonicMillis());
}

void apply(ProcessRuntimeState& state, const TransitionDecision& decision,
           const ProcessRunSnapshot* snapshot) {
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(
        fermentation::applyProcessTransition(state, decision, snapshot));
}

ProcessRuntimeState standbyState() {
    ProcessRuntimeState state;
    state.state = ProcessState::Standby;
    return state;
}

ProcessSignals qualificationInBandSignals() {
    return {QualificationProgress::InBand, false, false};
}

ProcessSignals qualificationCompleteSignals() {
    return {QualificationProgress::Complete, false, false};
}

ProcessSignals coolingReachedSignals() {
    return {QualificationProgress::Unavailable, true, false};
}

ProcessSignals interruptedSignals() {
    return {QualificationProgress::Unavailable, false, false};
}

ProcessSignals criticalFaultSignals() {
    return {QualificationProgress::Unavailable, false, true};
}

void enterQualifying(ProcessRuntimeState& state,
                     const ProcessRunSnapshot& snapshot,
                     const VirtualTimeSource& time) {
    apply(state, decide(state, &snapshot, time, {}, ProcessEvent::StartRun),
          &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.state));
    apply(state, decide(state, &snapshot, time, qualificationInBandSignals()),
          &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::QualifyingTarget),
                          static_cast<int>(state.state));
}

void qualify(ProcessRuntimeState& state, const ProcessRunSnapshot& snapshot,
             VirtualTimeSource& time) {
    time.advanceMonotonicMillis(kMinuteMillis);
    apply(state, decide(state, &snapshot, time, qualificationCompleteSignals()),
          &snapshot);
}

void test_all_factory_programs_produce_expected_process_snapshots() {
    const auto yogurtMild = makeFactorySnapshot("yogurt-mild");
    const auto yogurtFirm = makeFactorySnapshot("yogurt-firm");
    const auto milkKefir = makeFactorySnapshot("milk-kefir");
    const auto waterKefir = makeFactorySnapshot("water-kefir");

    TEST_ASSERT_TRUE(yogurtMild.preheatEnabled);
    TEST_ASSERT_TRUE(yogurtFirm.preheatEnabled);
    TEST_ASSERT_FALSE(milkKefir.preheatEnabled);
    TEST_ASSERT_FALSE(waterKefir.preheatEnabled);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CompletionMode::CoolAndHoldUntilManualStop),
        static_cast<int>(yogurtMild.completionMode));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CompletionMode::CoolAndHoldUntilManualStop),
        static_cast<int>(milkKefir.completionMode));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CompletionMode::FinishWithoutCooling),
        static_cast<int>(waterKefir.completionMode));
}

void test_all_factory_programs_complete_their_full_process_flow() {
    const std::array<const char*, 4U> programIds{{
        "yogurt-mild",
        "yogurt-firm",
        "milk-kefir",
        "water-kefir",
    }};

    for (const auto* programId : programIds) {
        VirtualTimeSource time;
        auto state = standbyState();
        const auto snapshot = makeFactorySnapshot(programId);

        apply(state, decide(state, &snapshot, time, {}, ProcessEvent::StartRun),
              &snapshot);
        if (snapshot.preheatEnabled) {
            apply(state,
                  decide(state, &snapshot, time, qualificationInBandSignals()),
                  &snapshot);
            time.advanceMonotonicMillis(kMinuteMillis);
            apply(
                state,
                decide(state, &snapshot, time, qualificationCompleteSignals()),
                &snapshot);
            apply(state,
                  decide(state, &snapshot, time, {},
                         ProcessEvent::ProductInsertedConfirmed),
                  &snapshot);
        }

        apply(state,
              decide(state, &snapshot, time, qualificationInBandSignals()),
              &snapshot);
        time.advanceMonotonicMillis(kMinuteMillis);
        apply(state,
              decide(state, &snapshot, time, qualificationCompleteSignals()),
              &snapshot);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fermenting),
                              static_cast<int>(state.state));

        time.advanceMonotonicMillis(2U * kMinuteMillis);
        apply(state, decide(state, &snapshot, time), &snapshot);
        if (snapshot.completionMode == CompletionMode::FinishWithoutCooling) {
            TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                                  static_cast<int>(state.state));
            continue;
        }

        TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Cooling),
                              static_cast<int>(state.state));
        apply(state, decide(state, &snapshot, time, coolingReachedSignals()),
              &snapshot);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::CoolHolding),
                              static_cast<int>(state.state));
        apply(state,
              decide(state, &snapshot, time, {},
                     ProcessEvent::FinishHoldConfirmed),
              &snapshot);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                              static_cast<int>(state.state));
    }
}

void test_process_snapshot_validation_rejects_inconsistent_values() {
    auto snapshot = makeTimedSnapshot(true);

    snapshot.maximumProductWaitMinutes = std::nullopt;
    TEST_ASSERT_FALSE(fermentation::validateProcessRunSnapshot(snapshot));

    snapshot = makeTimedSnapshot(false);
    snapshot.maximumProductWaitMinutes = 1U;
    TEST_ASSERT_FALSE(fermentation::validateProcessRunSnapshot(snapshot));

    snapshot = makeTimedSnapshot(false);
    snapshot.completionMode = CompletionMode::CoolAndHoldForDuration;
    TEST_ASSERT_FALSE(fermentation::validateProcessRunSnapshot(snapshot));

    snapshot = makeManualHoldingSnapshot(false);
    snapshot.completionMode = CompletionMode::CoolThenFinish;
    TEST_ASSERT_FALSE(fermentation::validateProcessRunSnapshot(snapshot));
}

void test_decision_does_not_mutate_until_applied_and_stale_is_rejected() {
    VirtualTimeSource time;
    auto state = standbyState();
    const auto snapshot = makeTimedSnapshot(false);
    const auto before = state;

    const auto decision =
        decide(state, &snapshot, time, {}, ProcessEvent::StartRun);

    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(fermentation::equalProcessRuntimeState(state, before));
    apply(state, decision, &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.state));

    const auto applied = state;
    TEST_ASSERT_FALSE(
        fermentation::applyProcessTransition(state, decision, &snapshot));
    TEST_ASSERT_TRUE(fermentation::equalProcessRuntimeState(state, applied));
}

void test_apply_rejects_fabricated_forbidden_transition() {
    auto state = standbyState();
    const auto before = state;
    TransitionDecision fabricated;
    fabricated.status = DecisionStatus::Proposed;
    fabricated.before = state;
    fabricated.after = state;
    fabricated.after.state = ProcessState::Fermenting;
    fabricated.after.transitionSequence = 1U;
    fabricated.reason = TransitionReason::TargetQualified;

    TEST_ASSERT_FALSE(
        fermentation::applyProcessTransition(state, fabricated, nullptr));
    TEST_ASSERT_TRUE(fermentation::equalProcessRuntimeState(state, before));
}

void test_apply_rejects_snapshot_incompatible_fabricated_transitions() {
    auto noPreheatState = standbyState();
    const auto noPreheatBefore = noPreheatState;
    const auto noPreheatSnapshot = makeTimedSnapshot(false);
    TransitionDecision wrongStart;
    wrongStart.status = DecisionStatus::Proposed;
    wrongStart.before = noPreheatState;
    wrongStart.after = noPreheatState;
    wrongStart.after.state = ProcessState::Preheating;
    wrongStart.after.transitionSequence = 1U;
    wrongStart.reason = TransitionReason::RunStarted;
    TEST_ASSERT_FALSE(fermentation::applyProcessTransition(
        noPreheatState, wrongStart, &noPreheatSnapshot));
    TEST_ASSERT_TRUE(fermentation::equalProcessRuntimeState(noPreheatState,
                                                            noPreheatBefore));

    ProcessRuntimeState fermentingState;
    fermentingState.state = ProcessState::Fermenting;
    const auto fermentingBefore = fermentingState;
    const auto finishWithoutCooling = makeTimedSnapshot(false);
    TransitionDecision wrongCompletion;
    wrongCompletion.status = DecisionStatus::Proposed;
    wrongCompletion.before = fermentingState;
    wrongCompletion.after = fermentingState;
    wrongCompletion.after.state = ProcessState::Cooling;
    wrongCompletion.after.transitionSequence = 1U;
    wrongCompletion.reason = TransitionReason::FermentationCompleted;
    TEST_ASSERT_FALSE(fermentation::applyProcessTransition(
        fermentingState, wrongCompletion, &finishWithoutCooling));
    TEST_ASSERT_TRUE(fermentation::equalProcessRuntimeState(fermentingState,
                                                            fermentingBefore));

    ProcessRuntimeState qualifyingState;
    qualifyingState.state = ProcessState::QualifyingTarget;
    const auto qualifyingBefore = qualifyingState;
    const auto manualSnapshot = makeManualHoldingSnapshot(false);
    TransitionDecision wrongQualifiedTarget;
    wrongQualifiedTarget.status = DecisionStatus::Proposed;
    wrongQualifiedTarget.before = qualifyingState;
    wrongQualifiedTarget.after = qualifyingState;
    wrongQualifiedTarget.after.state = ProcessState::Fermenting;
    wrongQualifiedTarget.after.transitionSequence = 1U;
    wrongQualifiedTarget.reason = TransitionReason::TargetQualified;
    TEST_ASSERT_FALSE(fermentation::applyProcessTransition(
        qualifyingState, wrongQualifiedTarget, &manualSnapshot));
    TEST_ASSERT_TRUE(fermentation::equalProcessRuntimeState(qualifyingState,
                                                            qualifyingBefore));
}

void test_critical_fault_has_priority_over_user_event() {
    VirtualTimeSource time;
    auto state = standbyState();
    const auto snapshot = makeTimedSnapshot(false);

    const auto decision = decide(state, &snapshot, time, criticalFaultSignals(),
                                 ProcessEvent::StartRun);

    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fault),
                          static_cast<int>(decision.after.state));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TransitionReason::CriticalFault),
                          static_cast<int>(decision.reason));
    TEST_ASSERT_TRUE(containsMessage(decision, ProcessMessage::FaultEntered));

    auto invalidSnapshot = snapshot;
    invalidSnapshot.qualificationDurationMinutes = 0U;
    const auto invalidSnapshotDecision =
        decide(state, &invalidSnapshot, time, criticalFaultSignals());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TransitionReason::CriticalFault),
                          static_cast<int>(invalidSnapshotDecision.reason));
    apply(state, invalidSnapshotDecision, &invalidSnapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fault),
                          static_cast<int>(state.state));
}

void test_preheat_qualification_resets_and_wait_timeout_aborts() {
    VirtualTimeSource time;
    auto state = standbyState();
    const auto snapshot = makeTimedSnapshot(true);

    apply(state, decide(state, &snapshot, time, {}, ProcessEvent::StartRun),
          &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Preheating),
                          static_cast<int>(state.state));

    apply(state, decide(state, &snapshot, time, qualificationInBandSignals()),
          &snapshot);
    time.advanceMonotonicMillis(30000U);
    apply(state, decide(state, &snapshot, time, interruptedSignals()),
          &snapshot);
    TEST_ASSERT_FALSE(state.qualificationValidSinceMillis.has_value());

    apply(state, decide(state, &snapshot, time, qualificationInBandSignals()),
          &snapshot);
    time.advanceMonotonicMillis(kMinuteMillis);
    const auto waiting =
        decide(state, &snapshot, time, qualificationCompleteSignals());
    TEST_ASSERT_TRUE(
        containsMessage(waiting, ProcessMessage::ProductInsertionRequested));
    apply(state, waiting, &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::WaitingForProduct),
                          static_cast<int>(state.state));

    time.advanceMonotonicMillis(3U * kMinuteMillis);
    const auto expired = decide(state, &snapshot, time);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::ProductWaitExpired),
        static_cast<int>(expired.reason));
    TEST_ASSERT_TRUE(containsMessage(expired, ProcessMessage::RunAborted));
    apply(state, expired, &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                          static_cast<int>(state.state));
}

void test_product_confirmation_starts_target_reach() {
    VirtualTimeSource time;
    auto state = standbyState();
    const auto snapshot = makeTimedSnapshot(true);
    apply(state, decide(state, &snapshot, time, {}, ProcessEvent::StartRun),
          &snapshot);
    apply(state, decide(state, &snapshot, time, qualificationInBandSignals()),
          &snapshot);
    time.advanceMonotonicMillis(kMinuteMillis);
    apply(state, decide(state, &snapshot, time, qualificationCompleteSignals()),
          &snapshot);

    time.advanceMonotonicMillis(1000U);
    apply(state,
          decide(state, &snapshot, time, {},
                 ProcessEvent::ProductInsertedConfirmed),
          &snapshot);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.state));
    TEST_ASSERT_EQUAL_UINT64(time.monotonicMillis(),
                             state.targetReachStartedAtMillis);
}

void test_product_confirmation_cannot_bypass_expired_wait_limit() {
    VirtualTimeSource time;
    auto state = standbyState();
    const auto snapshot = makeTimedSnapshot(true);
    apply(state, decide(state, &snapshot, time, {}, ProcessEvent::StartRun),
          &snapshot);
    apply(state, decide(state, &snapshot, time, qualificationInBandSignals()),
          &snapshot);
    time.advanceMonotonicMillis(kMinuteMillis);
    apply(state, decide(state, &snapshot, time, qualificationCompleteSignals()),
          &snapshot);
    time.advanceMonotonicMillis(3U * kMinuteMillis);

    const auto decision = decide(state, &snapshot, time, {},
                                 ProcessEvent::ProductInsertedConfirmed);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::ProductWaitExpired),
        static_cast<int>(decision.reason));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                          static_cast<int>(decision.after.state));
}

void test_target_qualification_starts_fermentation_timer() {
    VirtualTimeSource time;
    auto state = standbyState();
    const auto snapshot = makeTimedSnapshot(false);
    enterQualifying(state, snapshot, time);

    // Complete is the evaluator's checked-duration signal. The old process
    // marker is not a second qualification clock.
    time.advanceMonotonicMillis(kMinuteMillis - 1U);
    const auto qualified =
        decide(state, &snapshot, time, qualificationCompleteSignals());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TransitionReason::TargetQualified),
                          static_cast<int>(qualified.reason));
    apply(state, qualified, &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fermenting),
                          static_cast<int>(state.state));
    TEST_ASSERT_EQUAL_UINT64(time.monotonicMillis(),
                             state.stateEnteredAtMillis);

    time.advanceMonotonicMillis(2U * kMinuteMillis - 1U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DecisionStatus::NoTransition),
        static_cast<int>(decide(state, &snapshot, time).status));
    time.advanceMonotonicMillis(1U);
    apply(state, decide(state, &snapshot, time), &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                          static_cast<int>(state.state));
}

void test_in_band_signal_never_uses_old_marker_as_qualification_time() {
    VirtualTimeSource time;
    auto state = ProcessRuntimeState{};
    state.state = ProcessState::QualifyingTarget;
    state.stateEnteredAtMillis = 0U;
    state.targetReachStartedAtMillis = 0U;
    state.qualificationValidSinceMillis = 0U;
    auto snapshot = makeTimedSnapshot(false);
    snapshot.maximumTargetReachMinutes = 100U;
    time.advanceMonotonicMillis(10U * kMinuteMillis);

    const auto decision =
        decide(state, &snapshot, time, qualificationInBandSignals());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DecisionStatus::NoTransition),
                          static_cast<int>(decision.status));
    TEST_ASSERT_TRUE(state.qualificationValidSinceMillis.has_value());
}

void test_zero_remaining_duration_completes_immediately_after_qualification() {
    VirtualTimeSource time;
    auto state = standbyState();
    auto snapshot = makeTimedSnapshot(false);
    snapshot.fermentationDurationMinutes = 0U;
    TEST_ASSERT_TRUE(fermentation::validateProcessRunSnapshot(snapshot));

    enterQualifying(state, snapshot, time);
    qualify(state, snapshot, time);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fermenting),
                          static_cast<int>(state.state));

    const auto completed = decide(state, &snapshot, time);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::FermentationCompleted),
        static_cast<int>(completed.reason));
    apply(state, completed, &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                          static_cast<int>(state.state));
}

void test_target_reach_warning_survives_qualification_reset_and_occurs_once() {
    VirtualTimeSource time;
    auto state = standbyState();
    const auto snapshot = makeTimedSnapshot(false);
    enterQualifying(state, snapshot, time);

    time.advanceMonotonicMillis(kMinuteMillis);
    apply(state, decide(state, &snapshot, time, interruptedSignals()),
          &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.state));
    TEST_ASSERT_EQUAL_UINT64(0U, state.targetReachStartedAtMillis);

    time.advanceMonotonicMillis(kMinuteMillis);
    const auto warning = decide(state, &snapshot, time);
    TEST_ASSERT_TRUE(
        containsMessage(warning, ProcessMessage::TargetReachTimeExceeded));
    apply(state, warning, &snapshot);

    time.advanceMonotonicMillis(kMinuteMillis);
    const auto noSecondWarning = decide(state, &snapshot, time);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DecisionStatus::NoTransition),
                          static_cast<int>(noSecondWarning.status));
}

void test_completion_modes_reach_the_specified_states() {
    VirtualTimeSource time;
    auto state = standbyState();
    auto snapshot = makeTimedSnapshot(false, CompletionMode::CoolThenFinish);
    enterQualifying(state, snapshot, time);
    qualify(state, snapshot, time);
    time.advanceMonotonicMillis(2U * kMinuteMillis);
    apply(state, decide(state, &snapshot, time), &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Cooling),
                          static_cast<int>(state.state));
    apply(state, decide(state, &snapshot, time, coolingReachedSignals()),
          &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                          static_cast<int>(state.state));

    VirtualTimeSource timedHoldTime;
    auto timedHoldState = standbyState();
    const auto timedHoldSnapshot =
        makeTimedSnapshot(false, CompletionMode::CoolAndHoldForDuration);
    enterQualifying(timedHoldState, timedHoldSnapshot, timedHoldTime);
    qualify(timedHoldState, timedHoldSnapshot, timedHoldTime);
    timedHoldTime.advanceMonotonicMillis(2U * kMinuteMillis);
    apply(timedHoldState,
          decide(timedHoldState, &timedHoldSnapshot, timedHoldTime),
          &timedHoldSnapshot);
    apply(timedHoldState,
          decide(timedHoldState, &timedHoldSnapshot, timedHoldTime,
                 coolingReachedSignals()),
          &timedHoldSnapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::CoolHolding),
                          static_cast<int>(timedHoldState.state));
    timedHoldTime.advanceMonotonicMillis(2U * kMinuteMillis);
    apply(timedHoldState,
          decide(timedHoldState, &timedHoldSnapshot, timedHoldTime),
          &timedHoldSnapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                          static_cast<int>(timedHoldState.state));
}

void test_manual_cool_hold_finishes_normally() {
    VirtualTimeSource time;
    auto state = standbyState();
    const auto snapshot =
        makeTimedSnapshot(false, CompletionMode::CoolAndHoldUntilManualStop);
    enterQualifying(state, snapshot, time);
    qualify(state, snapshot, time);
    time.advanceMonotonicMillis(2U * kMinuteMillis);
    apply(state, decide(state, &snapshot, time), &snapshot);
    apply(state, decide(state, &snapshot, time, coolingReachedSignals()),
          &snapshot);

    const auto finished =
        decide(state, &snapshot, time, {}, ProcessEvent::FinishHoldConfirmed);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::HoldFinishedByUser),
        static_cast<int>(finished.reason));
    apply(state, finished, &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                          static_cast<int>(state.state));
}

void test_manual_holding_uses_qualified_route_with_and_without_preheat() {
    for (const bool preheat : {false, true}) {
        VirtualTimeSource time;
        auto state = standbyState();
        const auto snapshot = makeManualHoldingSnapshot(preheat);
        apply(state, decide(state, &snapshot, time, {}, ProcessEvent::StartRun),
              &snapshot);

        if (preheat) {
            apply(state,
                  decide(state, &snapshot, time, qualificationInBandSignals()),
                  &snapshot);
            time.advanceMonotonicMillis(kMinuteMillis);
            apply(
                state,
                decide(state, &snapshot, time, qualificationCompleteSignals()),
                &snapshot);
            apply(state,
                  decide(state, &snapshot, time, {},
                         ProcessEvent::ProductInsertedConfirmed),
                  &snapshot);
        }

        apply(state,
              decide(state, &snapshot, time, qualificationInBandSignals()),
              &snapshot);
        qualify(state, snapshot, time);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ManualHolding),
                              static_cast<int>(state.state));

        apply(state,
              decide(state, &snapshot, time, {},
                     ProcessEvent::FinishHoldConfirmed),
              &snapshot);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                              static_cast<int>(state.state));
    }
}

void test_product_and_air_guided_programs_use_same_abstract_signal_path() {
    VirtualTimeSource productTime;
    VirtualTimeSource airTime;
    auto productState = standbyState();
    auto airState = standbyState();
    const auto productSnapshot = makeFactorySnapshot("yogurt-mild");
    const auto airSnapshot = makeFactorySnapshot("milk-kefir");

    // Vorheizen ist eine Programmeigenschaft; ab REACHING_TARGET ist das
    // qualitaetsgepruefte Signal fuer beide Sensorbetriebsarten identisch.
    productState.state = ProcessState::ReachingTarget;
    airState.state = ProcessState::ReachingTarget;
    apply(productState,
          decide(productState, &productSnapshot, productTime,
                 qualificationInBandSignals()),
          &productSnapshot);
    apply(airState,
          decide(airState, &airSnapshot, airTime, qualificationInBandSignals()),
          &airSnapshot);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(productState.state),
                          static_cast<int>(airState.state));
}

void test_abort_is_explicit_for_every_active_state() {
    const std::array<ProcessState, 8U> activeStates{{
        ProcessState::Preheating,
        ProcessState::WaitingForProduct,
        ProcessState::ReachingTarget,
        ProcessState::QualifyingTarget,
        ProcessState::Fermenting,
        ProcessState::Cooling,
        ProcessState::CoolHolding,
        ProcessState::ManualHolding,
    }};
    const auto preheatSnapshot = makeTimedSnapshot(true);
    const auto timedSnapshot = makeTimedSnapshot(false);
    const auto coolingSnapshot =
        makeTimedSnapshot(false, CompletionMode::CoolAndHoldUntilManualStop);
    const auto manualSnapshot = makeManualHoldingSnapshot(false);
    VirtualTimeSource time;

    for (const auto activeState : activeStates) {
        ProcessRuntimeState state;
        state.state = activeState;
        if (activeState == ProcessState::QualifyingTarget) {
            state.qualificationValidSinceMillis = 0U;
        }
        const ProcessRunSnapshot* snapshot = &timedSnapshot;
        if (activeState == ProcessState::Preheating ||
            activeState == ProcessState::WaitingForProduct) {
            snapshot = &preheatSnapshot;
        } else if (activeState == ProcessState::Cooling ||
                   activeState == ProcessState::CoolHolding) {
            snapshot = &coolingSnapshot;
        } else if (activeState == ProcessState::ManualHolding) {
            snapshot = &manualSnapshot;
        }
        const auto decision =
            decide(state, snapshot, time, {}, ProcessEvent::Abort);
        TEST_ASSERT_TRUE(decision.proposed());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                              static_cast<int>(decision.after.state));
    }

    auto standby = standbyState();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DecisionStatus::Rejected),
                          static_cast<int>(decide(standby, &timedSnapshot, time,
                                                  {}, ProcessEvent::Abort)
                                               .status));
}

void test_boot_service_recovery_and_completion_topology_is_explicit() {
    VirtualTimeSource time;
    ProcessRuntimeState state;

    apply(state, decide(state, nullptr, time, {}, ProcessEvent::BootReady),
          nullptr);
    apply(state,
          decide(state, nullptr, time, {}, ProcessEvent::EnterServiceMode),
          nullptr);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ServiceMode),
                          static_cast<int>(state.state));
    apply(state,
          decide(state, nullptr, time, {}, ProcessEvent::ExitServiceMode),
          nullptr);

    state = {};
    apply(state, decide(state, nullptr, time, {}, ProcessEvent::BootRecoverRun),
          nullptr);
    ProcessRuntimeState recovered;
    recovered.state = ProcessState::Fermenting;
    const auto snapshot = makeTimedSnapshot(false);
    apply(state, decideRecovery(state, &snapshot, time, recovered), &snapshot);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fermenting),
                          static_cast<int>(state.state));

    state = {};
    apply(state,
          decide(state, nullptr, time, {}, ProcessEvent::BootRestoreCompleted),
          nullptr);
    apply(state,
          decide(state, nullptr, time, {}, ProcessEvent::AcknowledgeCompletion),
          nullptr);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                          static_cast<int>(state.state));
}

void test_recovery_rejects_state_and_snapshot_mismatches() {
    VirtualTimeSource time;
    ProcessRuntimeState recovery;
    recovery.state = ProcessState::RecoveryEvaluation;

    ProcessRuntimeState waiting;
    waiting.state = ProcessState::WaitingForProduct;
    const auto noPreheat = makeTimedSnapshot(false);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DecisionStatus::Rejected),
        static_cast<int>(
            decideRecovery(recovery, &noPreheat, time, waiting).status));

    ProcessRuntimeState fermenting;
    fermenting.state = ProcessState::Fermenting;
    const auto manual = makeManualHoldingSnapshot(false);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DecisionStatus::Rejected),
        static_cast<int>(
            decideRecovery(recovery, &manual, time, fermenting).status));

    const auto preheat = makeTimedSnapshot(true);
    time.advanceMonotonicMillis(3U * kMinuteMillis);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DecisionStatus::Rejected),
        static_cast<int>(
            decideRecovery(recovery, &preheat, time, waiting).status));

    ProcessRuntimeState qualifying;
    qualifying.state = ProcessState::QualifyingTarget;
    qualifying.qualificationValidSinceMillis = 0U;
    const auto restartedQualification =
        decideRecovery(recovery, &noPreheat, time, qualifying);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(restartedQualification.after.state));
    TEST_ASSERT_FALSE(
        restartedQualification.after.qualificationValidSinceMillis.has_value());

    ProcessRuntimeState preheating;
    preheating.state = ProcessState::Preheating;
    preheating.qualificationValidSinceMillis = 0U;
    const auto restartedPreheat =
        decideRecovery(recovery, &preheat, time, preheating);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Preheating),
                          static_cast<int>(restartedPreheat.after.state));
    TEST_ASSERT_FALSE(
        restartedPreheat.after.qualificationValidSinceMillis.has_value());

    const auto incompatibleCurrent = decide(waiting, &noPreheat, time);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DecisionStatus::InvalidInput),
                          static_cast<int>(incompatibleCurrent.status));
}

void test_recovery_reentry_and_tombstone_topology_use_exported_propose() {
    const auto snapshot = makeTimedSnapshot(false);
    ProcessRuntimeState fermenting;
    fermenting.state = ProcessState::Fermenting;
    fermenting.stateEnteredAtMillis = 5000U;
    fermenting.transitionSequence = 3U;

    const auto hop1 =
        fermentation::propose(fermenting, ProcessState::RecoveryEvaluation,
                              TransitionReason::RecoveryReentryRequired, 5000U);
    TEST_ASSERT_TRUE(hop1.proposed());
    auto candidate = fermenting;
    TEST_ASSERT_TRUE(
        fermentation::applyProcessTransition(candidate, hop1, &snapshot));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::RecoveryEvaluation),
                          static_cast<int>(candidate.state));

    // Boot benutzt keinen Run-Snapshot: Hop-1-Reentry ist dort topologisch
    // unzulaessig, auch wenn propose() selbst keine Topologie prueft.
    ProcessRuntimeState boot;
    boot.state = ProcessState::Boot;
    const auto invalidHop1 =
        fermentation::propose(boot, ProcessState::RecoveryEvaluation,
                              TransitionReason::RecoveryReentryRequired, 0U);
    auto bootCandidate = boot;
    TEST_ASSERT_FALSE(fermentation::applyProcessTransition(
        bootCandidate, invalidHop1, nullptr));

    ProcessRuntimeState recoveryEval;
    recoveryEval.state = ProcessState::RecoveryEvaluation;
    recoveryEval.transitionSequence = 1U;
    const auto tombstone = fermentation::propose(
        recoveryEval, ProcessState::Standby,
        TransitionReason::RecoveryEndedByExpiredWait, 9000U);
    TEST_ASSERT_TRUE(tombstone.proposed());
    auto tombstoneCandidate = recoveryEval;
    TEST_ASSERT_TRUE(fermentation::applyProcessTransition(tombstoneCandidate,
                                                          tombstone, nullptr));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                          static_cast<int>(tombstoneCandidate.state));

    // Tombstone-Reason ausserhalb RecoveryEvaluation ist topologisch
    // unzulaessig.
    auto standby = standbyState();
    const auto invalidTombstone =
        fermentation::propose(standby, ProcessState::Standby,
                              TransitionReason::RecoveryEndedByExpiredWait, 0U);
    auto standbyCandidate = standby;
    TEST_ASSERT_FALSE(fermentation::applyProcessTransition(
        standbyCandidate, invalidTombstone, nullptr));
}

void test_prior_boot_phase_elapsed_completes_fermentation_without_rebasing_underflow() {
    VirtualTimeSource time;
    ProcessRuntimeState state;
    state.state = ProcessState::Fermenting;
    state.stateEnteredAtMillis = 0U;
    const auto snapshot = makeTimedSnapshot(false);  // 2 Minuten (120s) Grenze

    time.advanceMonotonicMillis(5000U);  // dieser Boot laeuft erst 5 Sekunden

    // Ohne Vor-Boot-Anteil bleibt die Phase innerhalb der Grenze.
    const auto withoutPrior = fermentation::decideProcessTransition(
        state, &snapshot, {}, {}, time.monotonicMillis());
    TEST_ASSERT_FALSE(withoutPrior.proposed());

    // Ein bereits bekannter Vor-Boot-Anteil (200s), der allein schon ueber der
    // Grenze liegt, wird additiv beruecksichtigt - ohne dass `now - startedAt`
    // dafuer unterlaufen muesste (now ist hier deutlich kleiner als die
    // bereits bekannte Phasenzeit).
    PriorBootPhaseElapsed prior;
    prior.lowerBoundSeconds = 200U;
    const auto withPrior = fermentation::decideProcessTransition(
        state, &snapshot, {}, {}, time.monotonicMillis(), prior);
    TEST_ASSERT_TRUE(withPrior.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::FermentationCompleted),
        static_cast<int>(withPrior.reason));
}

void test_recovery_resume_waiting_for_product_honors_prior_boot_phase_elapsed() {
    ProcessRuntimeState recoveryEval;
    recoveryEval.state = ProcessState::RecoveryEvaluation;

    ProcessRuntimeState waiting;
    waiting.state = ProcessState::WaitingForProduct;
    waiting.stateEnteredAtMillis = 0U;
    const auto preheat = makeTimedSnapshot(true);  // 3 Minuten (180s) Grenze

    TransitionRequest request;
    request.event = ProcessEvent::RecoveryResume;
    request.recoveredState = waiting;

    PriorBootPhaseElapsed exceeded;
    exceeded.lowerBoundSeconds =
        200U;  // > 180s, obwohl dieser Boot bei 0ms steht
    const auto rejectedByPrior = fermentation::decideProcessTransition(
        recoveryEval, &preheat, {}, request, 0U, exceeded);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DecisionStatus::Rejected),
                          static_cast<int>(rejectedByPrior.status));

    PriorBootPhaseElapsed withinLimit;
    withinLimit.lowerBoundSeconds = 100U;
    const auto resumed = fermentation::decideProcessTransition(
        recoveryEval, &preheat, {}, request, 0U, withinLimit);
    TEST_ASSERT_TRUE(resumed.proposed());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::WaitingForProduct),
                          static_cast<int>(resumed.after.state));
}

void test_complete_hold_duration_extraction_matches_automatic_cool_holding_completion() {
    VirtualTimeSource time;
    ProcessRuntimeState state;
    state.state = ProcessState::CoolHolding;
    state.stateEnteredAtMillis = 0U;
    const auto snapshot =
        makeTimedSnapshot(false, CompletionMode::CoolAndHoldForDuration);
    time.advanceMonotonicMillis(2U * kMinuteMillis);

    const auto automatic = fermentation::decideProcessTransition(
        state, &snapshot, {}, {}, time.monotonicMillis());
    const auto direct =
        fermentation::completeHoldDuration(state, time.monotonicMillis());

    TEST_ASSERT_TRUE(automatic.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::HoldDurationCompleted),
        static_cast<int>(automatic.reason));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(direct.reason),
                          static_cast<int>(automatic.reason));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(direct.after.state),
                          static_cast<int>(automatic.after.state));
    TEST_ASSERT_EQUAL_UINT32(direct.messageCount, automatic.messageCount);
}

void test_propose_rejects_transition_sequence_overflow() {
    ProcessRuntimeState state;
    state.state = ProcessState::Fermenting;
    state.transitionSequence = std::numeric_limits<std::uint32_t>::max();

    const auto decision =
        fermentation::propose(state, ProcessState::Cooling,
                              TransitionReason::FermentationCompleted, 1000U);
    TEST_ASSERT_FALSE(decision.proposed());

    // completeTimedRun()/completeHoldDuration() rufen propose() intern auf
    // und erben denselben Schutz, ohne ihn erneut zu implementieren.
    const auto snapshot = makeTimedSnapshot(false);
    TEST_ASSERT_FALSE(
        fermentation::completeTimedRun(state, snapshot, 1000U).proposed());
    TEST_ASSERT_FALSE(
        fermentation::completeHoldDuration(state, 1000U).proposed());
}

void test_apply_process_transition_rejects_manually_constructed_sequence_wrap() {
    ProcessRuntimeState before;
    before.state = ProcessState::RecoveryEvaluation;
    before.transitionSequence = std::numeric_limits<std::uint32_t>::max();

    // Ein manuell konstruierter Kandidat, dessen after.transitionSequence
    // bereits auf 0 "gewrapped" ist - genau der Fall, in dem
    // `after != before + 1U` denselben Ueberlauf durchlaeuft und den Wrap
    // faelschlich als gueltigen Nachfolger akzeptieren wuerde, wenn
    // applyProcessTransition() before.transitionSequence nicht zusaetzlich
    // explizit gegen UINT32_MAX prueft.
    TransitionDecision forgedWrap;
    forgedWrap.status = DecisionStatus::Proposed;
    forgedWrap.before = before;
    forgedWrap.after = before;
    forgedWrap.after.state = ProcessState::Standby;
    forgedWrap.after.stateEnteredAtMillis = 1000U;
    forgedWrap.after.transitionSequence = 0U;
    forgedWrap.reason = TransitionReason::RecoveryEndedByExpiredWait;
    forgedWrap.monotonicMillis = 1000U;

    auto candidate = before;
    TEST_ASSERT_FALSE(
        fermentation::applyProcessTransition(candidate, forgedWrap, nullptr));
    TEST_ASSERT_TRUE(fermentation::equalProcessRuntimeState(candidate, before));
}

void test_recovery_reentry_at_ordinary_sequence_is_unaffected_by_overflow_guard() {
    const auto snapshot = makeTimedSnapshot(false);
    ProcessRuntimeState fermenting;
    fermenting.state = ProcessState::Fermenting;
    fermenting.transitionSequence = 41U;

    const auto hop1 =
        fermentation::propose(fermenting, ProcessState::RecoveryEvaluation,
                              TransitionReason::RecoveryReentryRequired, 1000U);
    TEST_ASSERT_TRUE(hop1.proposed());
    auto candidate = fermenting;
    TEST_ASSERT_TRUE(
        fermentation::applyProcessTransition(candidate, hop1, &snapshot));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::RecoveryEvaluation),
                          static_cast<int>(candidate.state));
    TEST_ASSERT_EQUAL_UINT32(42U, candidate.transitionSequence);
}

void test_prior_boot_phase_elapsed_waiting_for_product_boundary() {
    ProcessRuntimeState state;
    state.state = ProcessState::WaitingForProduct;
    state.stateEnteredAtMillis = 0U;
    const auto snapshot = makeTimedSnapshot(true);  // 3 Minuten (180s) Grenze

    PriorBootPhaseElapsed justUnder;
    justUnder.lowerBoundSeconds = 179U;
    TEST_ASSERT_FALSE(fermentation::decideProcessTransition(
                          state, &snapshot, {}, {}, 0U, justUnder)
                          .proposed());

    PriorBootPhaseElapsed atLimit;
    atLimit.lowerBoundSeconds = 180U;
    const auto atLimitDecision = fermentation::decideProcessTransition(
        state, &snapshot, {}, {}, 0U, atLimit);
    TEST_ASSERT_TRUE(atLimitDecision.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::ProductWaitExpired),
        static_cast<int>(atLimitDecision.reason));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                          static_cast<int>(atLimitDecision.after.state));
}

void test_prior_boot_phase_elapsed_cool_holding_boundary() {
    ProcessRuntimeState state;
    state.state = ProcessState::CoolHolding;
    state.stateEnteredAtMillis = 0U;
    const auto snapshot =
        makeTimedSnapshot(false, CompletionMode::CoolAndHoldForDuration);
    // 2 Minuten (120s) Grenze.

    PriorBootPhaseElapsed justUnder;
    justUnder.lowerBoundSeconds = 119U;
    TEST_ASSERT_FALSE(fermentation::decideProcessTransition(
                          state, &snapshot, {}, {}, 0U, justUnder)
                          .proposed());

    PriorBootPhaseElapsed atLimit;
    atLimit.lowerBoundSeconds = 120U;
    const auto atLimitDecision = fermentation::decideProcessTransition(
        state, &snapshot, {}, {}, 0U, atLimit);
    TEST_ASSERT_TRUE(atLimitDecision.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::HoldDurationCompleted),
        static_cast<int>(atLimitDecision.reason));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                          static_cast<int>(atLimitDecision.after.state));
}

void test_backward_time_and_invalid_events_do_not_change_state() {
    VirtualTimeSource time;
    auto state = standbyState();
    state.stateEnteredAtMillis = 10U;
    const auto before = state;

    const auto backwards = decide(state, nullptr, time);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DecisionStatus::TimeWentBackwards),
                          static_cast<int>(backwards.status));
    TEST_ASSERT_TRUE(fermentation::equalProcessRuntimeState(state, before));

    state = standbyState();
    const auto rejected = decide(state, nullptr, time, {},
                                 ProcessEvent::ProductInsertedConfirmed);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DecisionStatus::Rejected),
                          static_cast<int>(rejected.status));
    TEST_ASSERT_TRUE(
        fermentation::equalProcessRuntimeState(state, standbyState()));
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_all_factory_programs_produce_expected_process_snapshots);
    RUN_TEST(test_all_factory_programs_complete_their_full_process_flow);
    RUN_TEST(test_process_snapshot_validation_rejects_inconsistent_values);
    RUN_TEST(test_decision_does_not_mutate_until_applied_and_stale_is_rejected);
    RUN_TEST(test_apply_rejects_fabricated_forbidden_transition);
    RUN_TEST(test_apply_rejects_snapshot_incompatible_fabricated_transitions);
    RUN_TEST(test_critical_fault_has_priority_over_user_event);
    RUN_TEST(test_preheat_qualification_resets_and_wait_timeout_aborts);
    RUN_TEST(test_product_confirmation_starts_target_reach);
    RUN_TEST(test_product_confirmation_cannot_bypass_expired_wait_limit);
    RUN_TEST(test_target_qualification_starts_fermentation_timer);
    RUN_TEST(test_in_band_signal_never_uses_old_marker_as_qualification_time);
    RUN_TEST(
        test_zero_remaining_duration_completes_immediately_after_qualification);
    RUN_TEST(
        test_target_reach_warning_survives_qualification_reset_and_occurs_once);
    RUN_TEST(test_completion_modes_reach_the_specified_states);
    RUN_TEST(test_manual_cool_hold_finishes_normally);
    RUN_TEST(test_manual_holding_uses_qualified_route_with_and_without_preheat);
    RUN_TEST(
        test_product_and_air_guided_programs_use_same_abstract_signal_path);
    RUN_TEST(test_abort_is_explicit_for_every_active_state);
    RUN_TEST(test_boot_service_recovery_and_completion_topology_is_explicit);
    RUN_TEST(test_recovery_rejects_state_and_snapshot_mismatches);
    RUN_TEST(test_recovery_reentry_and_tombstone_topology_use_exported_propose);
    RUN_TEST(
        test_prior_boot_phase_elapsed_completes_fermentation_without_rebasing_underflow);
    RUN_TEST(
        test_recovery_resume_waiting_for_product_honors_prior_boot_phase_elapsed);
    RUN_TEST(
        test_complete_hold_duration_extraction_matches_automatic_cool_holding_completion);
    RUN_TEST(test_propose_rejects_transition_sequence_overflow);
    RUN_TEST(
        test_apply_process_transition_rejects_manually_constructed_sequence_wrap);
    RUN_TEST(
        test_recovery_reentry_at_ordinary_sequence_is_unaffected_by_overflow_guard);
    RUN_TEST(test_prior_boot_phase_elapsed_waiting_for_product_boundary);
    RUN_TEST(test_prior_boot_phase_elapsed_cool_holding_boundary);
    RUN_TEST(test_backward_time_and_invalid_events_do_not_change_state);
    return UNITY_END();
}
