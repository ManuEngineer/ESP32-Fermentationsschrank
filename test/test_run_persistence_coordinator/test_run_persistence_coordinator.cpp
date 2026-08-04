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

CommandDecision startDecision(const RunCommandState& state, CommandId id) {
    ProgramStartRequest request;
    request.envelope = {id, CommandSource::LocalDisplay, 100U,
                        state.processState.transitionSequence,
                        state.runRevision, std::nullopt, std::nullopt, true};
    request.runId = "persisted-run";
    request.program = runnableProgram();
    request.sourceKind = ProgramSourceKind::FactoryCatalog;
    request.sourceProgramRevision = 1U;
    request.sensorMode = RunSensorMode::Product;
    request.safetyAllowsStart = true;
    return decideProgramStart(state, request);
}

void test_load_empty_then_commit_and_restore_run_projection() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(store, device_platform::StorageEpoch(1U),
                                          RunCheckpointSchedule{});
    const auto loaded = coordinator.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::NoPersistedRun),
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
    RunPersistenceCoordinator coordinator(store, device_platform::StorageEpoch(1U),
                                          RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    const auto decision = startDecision(state, 77U);
    store.setNextWriteFault(
        device_platform_test_support::SimulatedPersistentStateStore::WriteFault::PowerCutAfterCommitBeforeReturn);
    const auto result = coordinator.persistCommand(
        state, decision, RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    const auto retry = coordinator.persistCommand(
        state, decision, RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::AlreadyPersisted),
                          static_cast<int>(retry.status));
}

void test_mutation_before_initialization_writes_nothing() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(store, device_platform::StorageEpoch(1U),
                                          RunCheckpointSchedule{});
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    const auto result = coordinator.persistCommand(
        state, startDecision(state, 101U), RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::NotInitialized),
                          static_cast<int>(result.status));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_load_empty_then_commit_and_restore_run_projection);
    RUN_TEST(test_unknown_outcome_is_resolved_by_exact_readback_and_duplicate_is_safe);
    RUN_TEST(test_mutation_before_initialization_writes_nothing);
    return UNITY_END();
}
