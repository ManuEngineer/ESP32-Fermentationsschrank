#include <unity.h>

#include "run_persistence_codec.hpp"
#include "standard_program_catalog.hpp"

namespace {

using namespace fermentation;

RunPersistenceSnapshot programSnapshot() {
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
    const auto run =
        ActiveRun::start(*document, ProgramSourceKind::FactoryCatalog, 1U);
    TEST_ASSERT_TRUE(run.has_value());
    RunCommandState state;
    state.processState.state = program.preheat ? ProcessState::Preheating
                                               : ProcessState::ReachingTarget;
    state.activeProgramRun = *run;
    state.activeRunId = "checkpoint-run";
    state.activeRunSensorMode = RunSensorMode::Product;
    state.processRunSnapshot = makeProcessRunSnapshot(*run);
    TEST_ASSERT_TRUE(state.processRunSnapshot.has_value());
    std::array<CommandId, kMaximumPersistedRunCommandIds> ids{};
    ids[0] = 99U;
    const auto snapshot = makeRunPersistenceSnapshot(
        state, ids, 1U, RunCheckpointTrigger::Command,
        RunCheckpointTime{100U, 1700000000}, 5U);
    TEST_ASSERT_TRUE(snapshot.has_value());
    return *snapshot;
}

void test_program_checkpoint_round_trip_restores_active_run() {
    const auto source = programSnapshot();
    std::string encoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(source, encoded)));
    const auto decoded = decodeRunPersistenceSnapshot(encoded);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(decoded.status));
    TEST_ASSERT_TRUE(decoded.snapshot.has_value());
    TEST_ASSERT_EQUAL_STRING("checkpoint-run",
                             decoded.snapshot->activeRunId.c_str());
    TEST_ASSERT_EQUAL_UINT64(99U, decoded.snapshot->persistedRunCommandIds[0]);
    const auto restored = restoreRunPersistenceSnapshot(*decoded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    TEST_ASSERT_TRUE(restored->activeProgramRun.has_value());
}

void test_tombstone_has_empty_run_id_and_rejects_active_data() {
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    std::array<CommandId, kMaximumPersistedRunCommandIds> ids{};
    const auto tombstone = makeRunPersistenceSnapshot(
        state, ids, 0U, RunCheckpointTrigger::Transition,
        RunCheckpointTime{100U, std::nullopt}, 5U);
    TEST_ASSERT_TRUE(tombstone.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunCheckpointVariant::NoActiveRun),
                          static_cast<int>(tombstone->variant));
    TEST_ASSERT_EQUAL_UINT32(0U, tombstone->activeRunId.size());
    auto invalid = *tombstone;
    invalid.activeRunId = "stale";
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(invalid));
}

void test_projection_rejects_inconsistent_aggregate_instead_of_prioritizing() {
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    state.activeRunId = "stale";
    std::array<CommandId, kMaximumPersistedRunCommandIds> ids{};
    TEST_ASSERT_FALSE(makeRunPersistenceSnapshot(
                          state, ids, 0U, RunCheckpointTrigger::Command,
                          RunCheckpointTime{1U, std::nullopt}, 5U)
                          .has_value());

    auto valid = programSnapshot();
    std::string bytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(valid, bytes)));
    bytes[0] = '\0';
    const auto invalidWire = decodeRunPersistenceSnapshot(bytes);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::InvalidWireValue),
        static_cast<int>(invalidWire.status));
}

void test_manual_completed_round_trip_is_a_valid_run_projection() {
    ManualRunPlan plan;
    plan.values.runId = "manual-checkpoint";
    plan.values.targetTemperatureCelsius = 12.0;
    plan.values.sensorMode = RunSensorMode::Air;
    plan.values.qualificationBandCelsius = 0.5;
    plan.values.qualificationDurationMinutes = 10U;
    plan.values.maximumTargetReachMinutes = 180U;
    plan.createdAtMonotonicMillis = 10U;
    TEST_ASSERT_TRUE(validateManualRunPlan(plan));
    RunCommandState state;
    state.activeManualRun = plan;
    state.activeRunId = plan.values.runId;
    state.activeRunSensorMode = plan.values.sensorMode;
    state.processRunSnapshot = makeProcessRunSnapshot(plan);
    TEST_ASSERT_TRUE(state.processRunSnapshot.has_value());
    state.processState.state = ProcessState::Completed;
    state.processState.stateEnteredAtMillis = 100U;
    state.processState.targetReachStartedAtMillis = 100U;
    std::array<CommandId, kMaximumPersistedRunCommandIds> ids{};
    const auto snapshot = makeRunPersistenceSnapshot(
        state, ids, 0U, RunCheckpointTrigger::Transition,
        RunCheckpointTime{100U, std::nullopt}, 5U);
    TEST_ASSERT_TRUE(snapshot.has_value());
    std::string bytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(*snapshot, bytes)));
    const auto decoded = decodeRunPersistenceSnapshot(bytes);
    TEST_ASSERT_TRUE(decoded.snapshot.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Completed),
        static_cast<int>(decoded.snapshot->processState.state));
    TEST_ASSERT_TRUE(
        restoreRunPersistenceSnapshot(*decoded.snapshot).has_value());
}

void test_prepared_head_binds_full_transaction_contract() {
    const RunCheckpointReference current{
        0U, 1U, 9U, 10U, 11U, 12U, RunCheckpointVariant::ProgramRun};
    const RunCheckpointReference fallback{
        1U, 1U, 9U, 9U, 8U, 7U, RunCheckpointVariant::NoActiveRun};
    const RunCheckpointReference target{
        1U, 1U, 9U, 11U, 13U, 14U, RunCheckpointVariant::NoActiveRun};
    RunPersistenceHead prepared;
    prepared.state = RunPersistenceHeadState::Prepared;
    prepared.revision = 20U;
    prepared.preparedCurrent = current;
    prepared.preparedFallback = fallback;
    prepared.target = target;
    prepared.mutationKind = RunPersistenceMutationKind::Command;
    prepared.commandId = 88U;
    prepared.oldRunRevision = 4U;
    prepared.newRunRevision = 5U;
    prepared.oldTransitionSequence = 6U;
    prepared.newTransitionSequence = 7U;
    const auto encoded =
        encodeRunPersistenceHead(prepared, device_platform::StorageEpoch(9U));
    TEST_ASSERT_TRUE(encoded.has_value());
    const auto decoded =
        decodeRunPersistenceHead(*encoded, device_platform::StorageEpoch(9U));
    TEST_ASSERT_TRUE(decoded.has_value());
    TEST_ASSERT_TRUE(decoded->preparedCurrent.has_value());
    TEST_ASSERT_TRUE(decoded->preparedFallback.has_value());
    TEST_ASSERT_EQUAL_UINT64(88U, *decoded->commandId);
    TEST_ASSERT_EQUAL_UINT32(5U, decoded->newRunRevision);
    TEST_ASSERT_EQUAL_UINT32(7U, decoded->newTransitionSequence);
    TEST_ASSERT_EQUAL_UINT32(1U, decoded->target.schemaVersion);
    TEST_ASSERT_EQUAL_UINT64(9U, decoded->target.storageEpoch);
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_program_checkpoint_round_trip_restores_active_run);
    RUN_TEST(test_tombstone_has_empty_run_id_and_rejects_active_data);
    RUN_TEST(
        test_projection_rejects_inconsistent_aggregate_instead_of_prioritizing);
    RUN_TEST(test_manual_completed_round_trip_is_a_valid_run_projection);
    RUN_TEST(test_prepared_head_binds_full_transaction_contract);
    return UNITY_END();
}
