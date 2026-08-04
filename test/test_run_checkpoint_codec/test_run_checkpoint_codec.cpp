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
    const auto run = ActiveRun::start(*document, ProgramSourceKind::FactoryCatalog, 1U);
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
        state, ids, 1U, RunCheckpointTrigger::Command, 1U,
        RunCheckpointTime{100U, 1700000000}, 5U);
    TEST_ASSERT_TRUE(snapshot.has_value());
    return *snapshot;
}

void test_program_checkpoint_round_trip_restores_active_run() {
    const auto source = programSnapshot();
    std::string encoded;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(encodeRunPersistenceSnapshot(source, encoded)));
    const auto decoded = decodeRunPersistenceSnapshot(encoded);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(decoded.status));
    TEST_ASSERT_TRUE(decoded.snapshot.has_value());
    TEST_ASSERT_EQUAL_STRING("checkpoint-run", decoded.snapshot->activeRunId.c_str());
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
        state, ids, 0U, RunCheckpointTrigger::Transition, 1U,
        RunCheckpointTime{100U, std::nullopt}, 5U);
    TEST_ASSERT_TRUE(tombstone.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunCheckpointVariant::NoActiveRun),
                          static_cast<int>(tombstone->variant));
    TEST_ASSERT_EQUAL_UINT32(0U, tombstone->activeRunId.size());
    auto invalid = *tombstone;
    invalid.activeRunId = "stale";
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(invalid));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_program_checkpoint_round_trip_restores_active_run);
    RUN_TEST(test_tombstone_has_empty_run_id_and_rejects_active_data);
    return UNITY_END();
}
