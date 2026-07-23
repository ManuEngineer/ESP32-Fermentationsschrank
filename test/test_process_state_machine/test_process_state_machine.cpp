#include <unity.h>

#include <array>
#include <cstdint>

#include "process_state_machine.hpp"
#include "standard_program_catalog.hpp"
#include "virtual_time_source.hpp"

namespace {

using device_platform::VirtualTimeSource;
using fermentation::ActiveRun;
using fermentation::CompletionMode;
using fermentation::DecisionStatus;
using fermentation::FactoryProgramCatalog;
using fermentation::ProcessEvent;
using fermentation::ProcessKind;
using fermentation::ProcessMessage;
using fermentation::ProcessRunSnapshot;
using fermentation::ProcessRuntimeState;
using fermentation::ProcessSignals;
using fermentation::ProcessState;
using fermentation::ProgramDocument;
using fermentation::ProgramSourceKind;
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

void apply(ProcessRuntimeState& state, const TransitionDecision& decision) {
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(fermentation::applyProcessTransition(state, decision));
}

ProcessRuntimeState standbyState() {
    ProcessRuntimeState state;
    state.state = ProcessState::Standby;
    return state;
}

void enterQualifying(ProcessRuntimeState& state,
                     const ProcessRunSnapshot& snapshot,
                     const VirtualTimeSource& time) {
    apply(state, decide(state, &snapshot, time, {}, ProcessEvent::StartRun));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.state));
    apply(state, decide(state, &snapshot, time, {true, false}));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::QualifyingTarget),
                          static_cast<int>(state.state));
}

void qualify(ProcessRuntimeState& state, const ProcessRunSnapshot& snapshot,
             VirtualTimeSource& time) {
    time.advanceMonotonicMillis(kMinuteMillis);
    apply(state, decide(state, &snapshot, time, {true, false}));
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
    apply(state, decision);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.state));

    const auto applied = state;
    TEST_ASSERT_FALSE(fermentation::applyProcessTransition(state, decision));
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

    TEST_ASSERT_FALSE(fermentation::applyProcessTransition(state, fabricated));
    TEST_ASSERT_TRUE(fermentation::equalProcessRuntimeState(state, before));
}

void test_critical_fault_has_priority_over_user_event() {
    VirtualTimeSource time;
    auto state = standbyState();
    const auto snapshot = makeTimedSnapshot(false);

    const auto decision =
        decide(state, &snapshot, time, {false, true}, ProcessEvent::StartRun);

    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fault),
                          static_cast<int>(decision.after.state));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TransitionReason::CriticalFault),
                          static_cast<int>(decision.reason));
    TEST_ASSERT_TRUE(containsMessage(decision, ProcessMessage::FaultEntered));

    auto invalidSnapshot = snapshot;
    invalidSnapshot.qualificationDurationMinutes = 0U;
    const auto invalidSnapshotDecision =
        decide(state, &invalidSnapshot, time, {false, true});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TransitionReason::CriticalFault),
                          static_cast<int>(invalidSnapshotDecision.reason));
}

void test_preheat_qualification_resets_and_wait_timeout_aborts() {
    VirtualTimeSource time;
    auto state = standbyState();
    const auto snapshot = makeTimedSnapshot(true);

    apply(state, decide(state, &snapshot, time, {}, ProcessEvent::StartRun));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Preheating),
                          static_cast<int>(state.state));

    apply(state, decide(state, &snapshot, time, {true, false}));
    time.advanceMonotonicMillis(30000U);
    apply(state, decide(state, &snapshot, time, {false, false}));
    TEST_ASSERT_FALSE(state.qualificationValidSinceMillis.has_value());

    apply(state, decide(state, &snapshot, time, {true, false}));
    time.advanceMonotonicMillis(kMinuteMillis);
    const auto waiting = decide(state, &snapshot, time, {true, false});
    TEST_ASSERT_TRUE(
        containsMessage(waiting, ProcessMessage::ProductInsertionRequested));
    apply(state, waiting);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::WaitingForProduct),
                          static_cast<int>(state.state));

    time.advanceMonotonicMillis(3U * kMinuteMillis);
    const auto expired = decide(state, &snapshot, time);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::ProductWaitExpired),
        static_cast<int>(expired.reason));
    TEST_ASSERT_TRUE(containsMessage(expired, ProcessMessage::RunAborted));
    apply(state, expired);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                          static_cast<int>(state.state));
}

void test_product_confirmation_starts_target_reach() {
    VirtualTimeSource time;
    auto state = standbyState();
    const auto snapshot = makeTimedSnapshot(true);
    apply(state, decide(state, &snapshot, time, {}, ProcessEvent::StartRun));
    apply(state, decide(state, &snapshot, time, {true, false}));
    time.advanceMonotonicMillis(kMinuteMillis);
    apply(state, decide(state, &snapshot, time, {true, false}));

    time.advanceMonotonicMillis(1000U);
    apply(state, decide(state, &snapshot, time, {},
                        ProcessEvent::ProductInsertedConfirmed));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.state));
    TEST_ASSERT_EQUAL_UINT64(time.monotonicMillis(),
                             state.targetReachStartedAtMillis);
}

void test_product_confirmation_cannot_bypass_expired_wait_limit() {
    VirtualTimeSource time;
    auto state = standbyState();
    const auto snapshot = makeTimedSnapshot(true);
    apply(state, decide(state, &snapshot, time, {}, ProcessEvent::StartRun));
    apply(state, decide(state, &snapshot, time, {true, false}));
    time.advanceMonotonicMillis(kMinuteMillis);
    apply(state, decide(state, &snapshot, time, {true, false}));
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

    time.advanceMonotonicMillis(kMinuteMillis - 1U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DecisionStatus::NoTransition),
        static_cast<int>(decide(state, &snapshot, time, {true, false}).status));

    time.advanceMonotonicMillis(1U);
    apply(state, decide(state, &snapshot, time, {true, false}));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fermenting),
                          static_cast<int>(state.state));
    TEST_ASSERT_EQUAL_UINT64(time.monotonicMillis(),
                             state.stateEnteredAtMillis);

    time.advanceMonotonicMillis(2U * kMinuteMillis - 1U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DecisionStatus::NoTransition),
        static_cast<int>(decide(state, &snapshot, time).status));
    time.advanceMonotonicMillis(1U);
    apply(state, decide(state, &snapshot, time));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                          static_cast<int>(state.state));
}

void test_target_reach_warning_survives_qualification_reset_and_occurs_once() {
    VirtualTimeSource time;
    auto state = standbyState();
    const auto snapshot = makeTimedSnapshot(false);
    enterQualifying(state, snapshot, time);

    time.advanceMonotonicMillis(kMinuteMillis);
    apply(state, decide(state, &snapshot, time, {false, false}));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.state));
    TEST_ASSERT_EQUAL_UINT64(0U, state.targetReachStartedAtMillis);

    time.advanceMonotonicMillis(kMinuteMillis);
    const auto warning = decide(state, &snapshot, time);
    TEST_ASSERT_TRUE(
        containsMessage(warning, ProcessMessage::TargetReachTimeExceeded));
    apply(state, warning);

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
    apply(state, decide(state, &snapshot, time));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Cooling),
                          static_cast<int>(state.state));
    apply(state, decide(state, &snapshot, time, {true, false}));
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
          decide(timedHoldState, &timedHoldSnapshot, timedHoldTime));
    apply(timedHoldState, decide(timedHoldState, &timedHoldSnapshot,
                                 timedHoldTime, {true, false}));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::CoolHolding),
                          static_cast<int>(timedHoldState.state));
    timedHoldTime.advanceMonotonicMillis(2U * kMinuteMillis);
    apply(timedHoldState,
          decide(timedHoldState, &timedHoldSnapshot, timedHoldTime));
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
    apply(state, decide(state, &snapshot, time));
    apply(state, decide(state, &snapshot, time, {true, false}));

    const auto finished =
        decide(state, &snapshot, time, {}, ProcessEvent::FinishHoldConfirmed);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::HoldFinishedByUser),
        static_cast<int>(finished.reason));
    apply(state, finished);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                          static_cast<int>(state.state));
}

void test_manual_holding_uses_qualified_route_with_and_without_preheat() {
    for (const bool preheat : {false, true}) {
        VirtualTimeSource time;
        auto state = standbyState();
        const auto snapshot = makeManualHoldingSnapshot(preheat);
        apply(state,
              decide(state, &snapshot, time, {}, ProcessEvent::StartRun));

        if (preheat) {
            apply(state, decide(state, &snapshot, time, {true, false}));
            time.advanceMonotonicMillis(kMinuteMillis);
            apply(state, decide(state, &snapshot, time, {true, false}));
            apply(state, decide(state, &snapshot, time, {},
                                ProcessEvent::ProductInsertedConfirmed));
        }

        apply(state, decide(state, &snapshot, time, {true, false}));
        qualify(state, snapshot, time);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ManualHolding),
                              static_cast<int>(state.state));

        apply(state, decide(state, &snapshot, time, {},
                            ProcessEvent::FinishHoldConfirmed));
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
          decide(productState, &productSnapshot, productTime, {true, false}));
    apply(airState, decide(airState, &airSnapshot, airTime, {true, false}));

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

    apply(state, decide(state, nullptr, time, {}, ProcessEvent::BootReady));
    apply(state,
          decide(state, nullptr, time, {}, ProcessEvent::EnterServiceMode));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ServiceMode),
                          static_cast<int>(state.state));
    apply(state,
          decide(state, nullptr, time, {}, ProcessEvent::ExitServiceMode));

    state = {};
    apply(state,
          decide(state, nullptr, time, {}, ProcessEvent::BootRecoverRun));
    ProcessRuntimeState recovered;
    recovered.state = ProcessState::Fermenting;
    const auto snapshot = makeTimedSnapshot(false);
    apply(state, decideRecovery(state, &snapshot, time, recovered));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fermenting),
                          static_cast<int>(state.state));

    state = {};
    apply(state,
          decide(state, nullptr, time, {}, ProcessEvent::BootRestoreCompleted));
    apply(state, decide(state, nullptr, time, {},
                        ProcessEvent::AcknowledgeCompletion));
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
    RUN_TEST(test_process_snapshot_validation_rejects_inconsistent_values);
    RUN_TEST(test_decision_does_not_mutate_until_applied_and_stale_is_rejected);
    RUN_TEST(test_apply_rejects_fabricated_forbidden_transition);
    RUN_TEST(test_critical_fault_has_priority_over_user_event);
    RUN_TEST(test_preheat_qualification_resets_and_wait_timeout_aborts);
    RUN_TEST(test_product_confirmation_starts_target_reach);
    RUN_TEST(test_product_confirmation_cannot_bypass_expired_wait_limit);
    RUN_TEST(test_target_qualification_starts_fermentation_timer);
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
    RUN_TEST(test_backward_time_and_invalid_events_do_not_change_state);
    return UNITY_END();
}
