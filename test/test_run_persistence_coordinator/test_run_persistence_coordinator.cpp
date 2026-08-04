#include <unity.h>

#include "run_persistence_coordinator.hpp"
#include "simulated_persistent_state_store.hpp"
#include "standard_program_catalog.hpp"

namespace {

using namespace fermentation;

ProgramDocument runnableProgram() {
    auto document = FactoryProgramCatalog::find("water-kefir");
    TEST_ASSERT_TRUE(document.has_value());
    auto& program = document->program;
    program.productSensorFailure.fallbackDelaySeconds = 30U;
    program.fermentationStages.front().targetTemperatureCelsius = 38.0;
    program.fermentationStages.front().durationMinutes = 120U;
    program.targetQualification.bandCelsius = 0.5;
    program.targetQualification.durationMinutes = 10U;
    program.maximumTargetReachMinutes = 180U;
    if (program.preheat) program.maximumProductWaitMinutes = 30U;
    TEST_ASSERT_TRUE(validateProgram(*document).valid());
    return *document;
}

CommandDecision startDecision(const RunCommandState& state, CommandId id,
                              std::uint64_t monotonicMillis = 100U) {
    ProgramStartRequest request;
    request.envelope = {id,
                        CommandSource::LocalDisplay,
                        monotonicMillis,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true};
    request.runId = "persisted-run";
    request.program = runnableProgram();
    request.sourceKind = ProgramSourceKind::FactoryCatalog;
    request.sourceProgramRevision = 1U;
    request.sensorMode = RunSensorMode::Product;
    request.safetyAllowsStart = true;
    return decideProgramStart(state, request);
}

CommandDecision manualStartDecision(const RunCommandState& state,
                                    CommandId id) {
    ManualStartRequest request;
    request.envelope = {id,
                        CommandSource::LocalDisplay,
                        100U,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true};
    request.plan.runId = "manual-persisted-run";
    request.plan.targetTemperatureCelsius = 12.0;
    request.plan.sensorMode = RunSensorMode::Air;
    request.plan.qualificationBandCelsius = 0.5;
    request.plan.qualificationDurationMinutes = 10U;
    request.plan.maximumTargetReachMinutes = 180U;
    request.safetyAllowsStart = true;
    return decideManualStart(state, request);
}

void test_load_empty_then_commit_and_restore_run_projection() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = coordinator.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::NoPersistedRun),
        static_cast<int>(loaded.status));
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    const auto decision = startDecision(state, 42U);
    TEST_ASSERT_TRUE(decision.proposed());
    const auto result = coordinator.persistCommand(
        state, decision, RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_TRUE(state.activeProgramRun.has_value());
    TEST_ASSERT_EQUAL_UINT32(1U, result.effectCount);

    store.restart();
    RunPersistenceCoordinator restored(store, device_platform::StorageEpoch(1U),
                                       RunCheckpointSchedule{});
    const auto afterBoot = restored.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(afterBoot.status));
    TEST_ASSERT_TRUE(afterBoot.snapshot.has_value());
    const auto runtime = restoreRunPersistenceSnapshot(*afterBoot.snapshot);
    TEST_ASSERT_TRUE(runtime.has_value());
    TEST_ASSERT_EQUAL_STRING("persisted-run", runtime->activeRunId.c_str());
    TEST_ASSERT_TRUE(runtime->activeProgramRun.has_value());
}

void test_unknown_outcome_is_resolved_by_exact_readback_and_duplicate_is_safe() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    const auto decision = startDecision(state, 77U);
    store.setNextWriteFault(
        device_platform_test_support::SimulatedPersistentStateStore::
            WriteFault::PowerCutAfterCommitBeforeReturn);
    const auto result = coordinator.persistCommand(
        state, decision, RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    const auto retry = coordinator.persistCommand(
        state, decision, RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::AlreadyPersisted),
        static_cast<int>(retry.status));
}

void test_mutation_before_initialization_writes_nothing() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    const auto result =
        coordinator.persistCommand(state, startDecision(state, 101U),
                                   RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NotInitialized),
        static_cast<int>(result.status));
}

void test_periodic_non_writes_are_truthful_and_do_not_apply() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState empty;
    empty.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NoActiveRun),
        static_cast<int>(
            coordinator
                .checkpointPeriodic(empty, RunCheckpointTime{10U, std::nullopt})
                .status));

    RunCommandState active;
    active.processState.state = ProcessState::Standby;
    const auto decision = startDecision(active, 202U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(active, decision,
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NotDue),
        static_cast<int>(coordinator
                             .checkpointPeriodic(
                                 active, RunCheckpointTime{101U, std::nullopt})
                             .status));
}

void test_manual_completed_transition_commits_before_releasing_messages() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    const auto start = manualStartDecision(state, 303U);
    TEST_ASSERT_TRUE(start.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, start,
                                RunCheckpointTime{100U, std::nullopt})
                .status));

    // The state machine produces this only after a real manual hold.  The
    // transition still has to pass the persistence gate before its message is
    // exposed to a caller.
    state.processState.state = ProcessState::ManualHolding;
    state.processState.stateEnteredAtMillis = 100U;
    state.processState.targetReachStartedAtMillis = 100U;
    ProcessSignals signals;
    const auto transition = decideProcessTransition(
        state.processState, &*state.processRunSnapshot, signals,
        TransitionRequest{ProcessEvent::FinishHoldConfirmed, std::nullopt},
        200U);
    TEST_ASSERT_TRUE(transition.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::HoldFinishedByUser),
        static_cast<int>(transition.reason));
    store.setNextWriteFault(
        device_platform_test_support::SimulatedPersistentStateStore::
            WriteFault::FailBeforeBegin);
    const auto failed = coordinator.persistTransition(
        state, transition, RunCheckpointTime{200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(failed.status));
    TEST_ASSERT_EQUAL_UINT32(0U, failed.messageCount);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ManualHolding),
                          static_cast<int>(state.processState.state));

    const auto result = coordinator.persistTransition(
        state, transition, RunCheckpointTime{200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_EQUAL_UINT32(1U, result.messageCount);

    store.restart();
    RunPersistenceCoordinator restored(store, device_platform::StorageEpoch(1U),
                                       RunCheckpointSchedule{});
    const auto loaded = restored.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(loaded.status));
    TEST_ASSERT_TRUE(loaded.snapshot.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Completed),
        static_cast<int>(loaded.snapshot->processState.state));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunCheckpointVariant::ManualRun),
                          static_cast<int>(loaded.snapshot->variant));
}

void test_tombstone_boot_resets_schedule_to_the_new_boot_timebase() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    const auto start = startDecision(state, 401U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, start,
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    StopRequest stop;
    stop.envelope = {402U,
                     CommandSource::LocalDisplay,
                     200U,
                     state.processState.transitionSequence,
                     state.runRevision,
                     std::nullopt,
                     std::nullopt,
                     true};
    stop.option = StopOption::AbortAndTurnOff;
    const auto abort = decideStop(state, stop);
    TEST_ASSERT_TRUE(abort.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, abort,
                                RunCheckpointTime{200U, std::nullopt})
                .status));

    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::NoActiveRun),
        static_cast<int>(afterBoot.loadAndInitialize().status));
    RunCommandState newRun;
    newRun.processState.state = ProcessState::Standby;
    const auto nextStart = startDecision(newRun, 403U, 10U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            afterBoot
                .persistCommand(newRun, nextStart,
                                RunCheckpointTime{10U, std::nullopt})
                .status));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_load_empty_then_commit_and_restore_run_projection);
    RUN_TEST(
        test_unknown_outcome_is_resolved_by_exact_readback_and_duplicate_is_safe);
    RUN_TEST(test_mutation_before_initialization_writes_nothing);
    RUN_TEST(test_periodic_non_writes_are_truthful_and_do_not_apply);
    RUN_TEST(
        test_manual_completed_transition_commits_before_releasing_messages);
    RUN_TEST(test_tombstone_boot_resets_schedule_to_the_new_boot_timebase);
    return UNITY_END();
}
