#include <unity.h>

#include "run_persistence_codec.hpp"
#include "standard_program_catalog.hpp"

namespace {

using namespace fermentation;

std::string bytesFromHex(const char* hex) {
    std::string bytes;
    for (std::size_t i = 0U; hex[i] != '\0'; i += 2U) {
        const auto nibble = [](char value) -> unsigned char {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'a' && value <= 'f') return value - 'a' + 10U;
            return 0U;
        };
        bytes.push_back(
            static_cast<char>((nibble(hex[i]) << 4U) | nibble(hex[i + 1U])));
    }
    return bytes;
}

void assertGolden(const std::string& actual, const char* expectedHex) {
    const auto expected = bytesFromHex(expectedHex);
    TEST_ASSERT_EQUAL_UINT32(expected.size(), actual.size());
    TEST_ASSERT_EQUAL_MEMORY(expected.data(), actual.data(), expected.size());
}

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
    state.sensorSelection = PersistedSensorSelectionState{
        SensorSelectionProvenance::InitialSelection,
        SensorSelectionDecisionCause::StartSelection, 1U};
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
    assertGolden(
        encoded,
        "01010000000000000064000500000000000e636865636b706f696e742d72756e01010102000000"
        "010000000101005d00000006000000000001ffff000b77617465722d6b65666972000b57617373"
        "65726b656669720000010101010101000201010000001e03010140430000000000000100000078"
        "013fe0000000000000010000000a01000000b400010000000100010000000a000000b400010000"
        "0078000600000000000000000000000000000000000000000000010000000000000063");
    const auto decoded = decodeRunPersistenceSnapshot(encoded, kCurrentRunPersistenceSchema);
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
    std::string encoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(*tombstone, encoded)));
    assertGolden(encoded,
                 "0302000000000000006400050000000000000300000000000000000000000"
                 "00000000000000000000000");
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
    const auto invalidWire = decodeRunPersistenceSnapshot(bytes, kCurrentRunPersistenceSchema);
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
    state.sensorSelection = PersistedSensorSelectionState{
        SensorSelectionProvenance::InitialSelection,
        SensorSelectionDecisionCause::StartSelection, 1U};
    state.processRunSnapshot = makeProcessRunSnapshot(plan);
    TEST_ASSERT_TRUE(state.processRunSnapshot.has_value());
    state.processState.state = ProcessState::Completed;
    state.processState.stateEnteredAtMillis = 100U;
    state.processState.targetReachStartedAtMillis = 0U;
    std::array<CommandId, kMaximumPersistedRunCommandIds> ids{};
    const auto snapshot = makeRunPersistenceSnapshot(
        state, ids, 0U, RunCheckpointTrigger::Transition,
        RunCheckpointTime{100U, std::nullopt}, 5U);
    TEST_ASSERT_TRUE(snapshot.has_value());
    std::string bytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(*snapshot, bytes)));
    assertGolden(bytes,
                 "0202000000000000006400050000000000116d616e75616c2d636865636b706f696e74020101"
                 "020000000140280000000000000200003fe00000000000000000000a000000b4010000000000"
                 "00000a020200010000000a000000b40000000c00000000000000640000000000000000000000"
                 "00000000");
    const auto decoded = decodeRunPersistenceSnapshot(bytes, kCurrentRunPersistenceSchema);
    TEST_ASSERT_TRUE(decoded.snapshot.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Completed),
        static_cast<int>(decoded.snapshot->processState.state));
    TEST_ASSERT_TRUE(
        restoreRunPersistenceSnapshot(*decoded.snapshot).has_value());
}

void test_manual_snapshot_and_runtime_shape_must_be_canonical() {
    ManualRunPlan plan;
    plan.values.runId = "manual-contract";
    plan.values.targetTemperatureCelsius = 12.0;
    plan.values.sensorMode = RunSensorMode::Air;
    plan.values.qualificationBandCelsius = 0.5;
    plan.values.qualificationDurationMinutes = 10U;
    plan.values.maximumTargetReachMinutes = 180U;
    plan.createdAtMonotonicMillis = 10U;
    RunCommandState state;
    state.activeManualRun = plan;
    state.activeRunId = plan.values.runId;
    state.activeRunSensorMode = plan.values.sensorMode;
    state.sensorSelection = PersistedSensorSelectionState{
        SensorSelectionProvenance::InitialSelection,
        SensorSelectionDecisionCause::StartSelection, 1U};
    state.processRunSnapshot = makeProcessRunSnapshot(plan);
    state.processState.state = ProcessState::ManualHolding;
    state.processState.stateEnteredAtMillis = 100U;
    std::array<CommandId, kMaximumPersistedRunCommandIds> ids{};
    const auto snapshot = makeRunPersistenceSnapshot(
        state, ids, 0U, RunCheckpointTrigger::Transition,
        RunCheckpointTime{100U, std::nullopt}, 5U);
    TEST_ASSERT_TRUE(snapshot.has_value());

    auto differentProcess = *snapshot;
    differentProcess.processRunSnapshot->maximumTargetReachMinutes = 181U;
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(differentProcess));

    auto staleTimer = *snapshot;
    staleTimer.processState.targetReachStartedAtMillis = 1U;
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(staleTimer));

    auto staleWarning = *snapshot;
    staleWarning.processState.targetReachWarningIssued = true;
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(staleWarning));

    auto futureTime = *snapshot;
    futureTime.processState.stateEnteredAtMillis = 101U;
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(futureTime));
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
    assertGolden(
        *encoded,
        "4450524600010008000000020000000000000009000000000000001400000077004b4c6c7c01"
        "0100000000010000000000000009000000000000000a0000000b0000000c0101010000000100"
        "0000000000000900000000000000090000000800000007030100000001000000000000000900"
        "0000000000000b0000000d0000000e0301010000000000000058000000040000000500000006"
        "00000007");
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

void test_head_reference_and_mutation_invariants_reject_invalid_contracts() {
    const RunCheckpointReference current{
        0U, 1U, 9U, 10U, 11U, 12U, RunCheckpointVariant::ProgramRun};
    const RunCheckpointReference target{
        1U, 1U, 9U, 11U, 13U, 14U, RunCheckpointVariant::ManualRun};
    RunPersistenceHead committed;
    committed.state = RunPersistenceHeadState::Committed;
    committed.revision = 22U;
    committed.current = current;
    RunPersistenceHead prepared;
    prepared.state = RunPersistenceHeadState::Prepared;
    prepared.revision = 20U;
    prepared.preparedCurrent = current;
    prepared.target = target;
    prepared.mutationKind = RunPersistenceMutationKind::Command;
    prepared.commandId = 88U;
    prepared.newRunRevision = 1U;
    prepared.newTransitionSequence = 1U;
    TEST_ASSERT_TRUE(
        encodeRunPersistenceHead(prepared, device_platform::StorageEpoch(9U))
            .has_value());

    auto wrongTargetSlot = prepared;
    wrongTargetSlot.target.slot = 0U;
    TEST_ASSERT_FALSE(encodeRunPersistenceHead(
                          wrongTargetSlot, device_platform::StorageEpoch(9U))
                          .has_value());
    auto wrongEpoch = prepared;
    wrongEpoch.target.storageEpoch = 8U;
    TEST_ASSERT_FALSE(
        encodeRunPersistenceHead(wrongEpoch, device_platform::StorageEpoch(9U))
            .has_value());
    auto missingCommandId = prepared;
    missingCommandId.commandId.reset();
    TEST_ASSERT_FALSE(encodeRunPersistenceHead(
                          missingCommandId, device_platform::StorageEpoch(9U))
                          .has_value());

    auto noCurrentTargetRc1 = prepared;
    noCurrentTargetRc1.preparedCurrent.reset();
    noCurrentTargetRc1.preparedFallback.reset();
    noCurrentTargetRc1.target.slot = 1U;
    TEST_ASSERT_FALSE(encodeRunPersistenceHead(
                          noCurrentTargetRc1, device_platform::StorageEpoch(9U))
                          .has_value());

    auto tombstoneCurrent = committed;
    tombstoneCurrent.current = RunCheckpointReference{
        1U, 1U, 9U, 12U, 13U, 14U, RunCheckpointVariant::NoActiveRun};
    tombstoneCurrent.fallback = current;
    TEST_ASSERT_FALSE(encodeRunPersistenceHead(
                          tombstoneCurrent, device_platform::StorageEpoch(9U))
                          .has_value());

    auto tombstonePrepared = prepared;
    tombstonePrepared.preparedCurrent = RunCheckpointReference{
        0U, 1U, 9U, 12U, 13U, 14U, RunCheckpointVariant::NoActiveRun};
    tombstonePrepared.preparedFallback = current;
    tombstonePrepared.target.slot = 1U;
    TEST_ASSERT_FALSE(encodeRunPersistenceHead(
                          tombstonePrepared, device_platform::StorageEpoch(9U))
                          .has_value());

    auto oversizedReference = prepared;
    oversizedReference.target.payloadLength =
        static_cast<std::uint32_t>(kMaximumRunPersistencePayloadBytes + 1U);
    TEST_ASSERT_FALSE(encodeRunPersistenceHead(
                          oversizedReference, device_platform::StorageEpoch(9U))
                          .has_value());

    auto activeWithTombstoneFallback = committed;
    activeWithTombstoneFallback.current = current;
    activeWithTombstoneFallback.fallback = RunCheckpointReference{
        1U, 1U, 9U, 9U, 8U, 7U, RunCheckpointVariant::NoActiveRun};
    const auto committedGolden = encodeRunPersistenceHead(
        activeWithTombstoneFallback, device_platform::StorageEpoch(9U));
    TEST_ASSERT_TRUE(committedGolden.has_value());
    assertGolden(
        *committedGolden,
        "445052460001000800000002000000000000000900000000000000160000003e007a5213fe02"
        "00000000010000000000000009000000000000000a0000000b0000000c010101000000010000"
        "0000000000090000000000000009000000080000000703");

    committed.fallback = current;
    TEST_ASSERT_FALSE(
        encodeRunPersistenceHead(committed, device_platform::StorageEpoch(9U))
            .has_value());
}

void test_payload_bounds_and_truncation_are_strict() {
    const auto source = programSnapshot();
    std::string encoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(source, encoded)));
    const std::array<std::size_t, 4U> cuts{0U, 1U, encoded.size() / 2U,
                                           encoded.size() - 1U};
    for (const std::size_t cut : cuts) {
        const auto truncated =
            decodeRunPersistenceSnapshot(encoded.substr(0U, cut), kCurrentRunPersistenceSchema);
        TEST_ASSERT_NOT_EQUAL(
            static_cast<int>(RunPersistenceCodecStatus::Success),
            static_cast<int>(truncated.status));
    }
    auto withTrailing = encoded;
    withTrailing.push_back('\0');
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::TrailingBytes),
        static_cast<int>(decodeRunPersistenceSnapshot(withTrailing, kCurrentRunPersistenceSchema).status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::InvalidWireValue),
        static_cast<int>(
            decodeRunPersistenceSnapshot(
                std::string(kMaximumRunPersistencePayloadBytes, '\0'),
                kCurrentRunPersistenceSchema)
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::CapacityExceeded),
        static_cast<int>(
            decodeRunPersistenceSnapshot(
                std::string(kMaximumRunPersistencePayloadBytes + 1U, '\0'),
                kCurrentRunPersistenceSchema)
                .status));
}

// #21, 9.3: a genuine schema-1 payload (captured before the sensor-selection
// field existed - no presence tag, no PersistedSensorSelectionState bytes)
// must still decode. Decoding it with schemaVersion 1 must never attempt to
// read the field at all.
void test_schema_one_payload_decodes_without_sensor_selection_field() {
    const auto schemaOnePayload = bytesFromHex(
        "01010000000000000064000500000000000e636865636b706f696e742d72756e0100000001"
        "01005d00000006000000000001ffff000b77617465722d6b65666972000b5761737365726b"
        "656669720000010101010101000201010000001e0301014043000000000000010000007801"
        "3fe0000000000000010000000a01000000b400010000000100010000000a000000b4000100"
        "000078000600000000000000000000000000000000000000000000010000000000000063");
    const auto decoded = decodeRunPersistenceSnapshot(schemaOnePayload, 1U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(decoded.status));
    TEST_ASSERT_TRUE(decoded.snapshot.has_value());
    TEST_ASSERT_EQUAL_STRING("checkpoint-run",
                             decoded.snapshot->activeRunId.c_str());
    TEST_ASSERT_FALSE(decoded.snapshot->sensorSelection.has_value());
    // Decoding the identical bytes as schema 2 misreads the following field
    // boundary (there is no presence tag at this offset in a schema-1
    // payload) and must not silently succeed with the wrong shape.
    const auto misread =
        decodeRunPersistenceSnapshot(schemaOnePayload, kCurrentRunPersistenceSchema);
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(misread.status));
}

// #21, 9.3: readReference/validReference accept exactly the known schema set
// {1U, 2U} and reject anything else, exercised via the public head codec
// (encodeRunPersistenceHead stamps kCurrentRunPersistenceSchema on the head
// envelope itself; the embedded RunCheckpointReference::schemaVersion is
// independently checked by readReference/validReference).
void test_head_reference_accepts_known_schemas_and_rejects_unknown_ones() {
    const auto epoch = device_platform::StorageEpoch(9U);
    for (const std::uint32_t schema : {1U, 2U}) {
        RunPersistenceHead committed;
        committed.state = RunPersistenceHeadState::Committed;
        committed.revision = 5U;
        committed.current = RunCheckpointReference{
            0U, schema, 9U, 10U, 11U, 12U, RunCheckpointVariant::ProgramRun};
        const auto encoded = encodeRunPersistenceHead(committed, epoch);
        TEST_ASSERT_TRUE(encoded.has_value());
        const auto decoded = decodeRunPersistenceHead(*encoded, epoch);
        TEST_ASSERT_TRUE(decoded.has_value());
        TEST_ASSERT_EQUAL_UINT32(schema, decoded->current.schemaVersion);
    }
    RunPersistenceHead unknownSchema;
    unknownSchema.state = RunPersistenceHeadState::Committed;
    unknownSchema.revision = 5U;
    unknownSchema.current = RunCheckpointReference{
        0U, 3U, 9U, 10U, 11U, 12U, RunCheckpointVariant::ProgramRun};
    TEST_ASSERT_FALSE(
        encodeRunPersistenceHead(unknownSchema, epoch).has_value());
}

// #21, 9.3: "Codec-/Contract-Test mit gemischtem Schema-2-Current und
// Schema-1-Fallback" - the two references inside one committed head may
// legitimately carry different schema versions (current freshly written
// under schema 2, fallback still the not-yet-superseded schema-1 checkpoint
// from before the upgrade).
void test_committed_head_accepts_mixed_current_and_fallback_schema() {
    const auto epoch = device_platform::StorageEpoch(9U);
    RunPersistenceHead committed;
    committed.state = RunPersistenceHeadState::Committed;
    committed.revision = 6U;
    committed.current = RunCheckpointReference{
        0U, kCurrentRunPersistenceSchema, 9U,
        11U, 13U, 14U, RunCheckpointVariant::ProgramRun};
    committed.fallback = RunCheckpointReference{
        1U, 1U, 9U, 10U, 11U, 12U, RunCheckpointVariant::ProgramRun};
    const auto encoded = encodeRunPersistenceHead(committed, epoch);
    TEST_ASSERT_TRUE(encoded.has_value());
    const auto decoded = decodeRunPersistenceHead(*encoded, epoch);
    TEST_ASSERT_TRUE(decoded.has_value());
    TEST_ASSERT_EQUAL_UINT32(kCurrentRunPersistenceSchema,
                             decoded->current.schemaVersion);
    TEST_ASSERT_TRUE(decoded->fallback.has_value());
    TEST_ASSERT_EQUAL_UINT32(1U, decoded->fallback->schemaVersion);
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_program_checkpoint_round_trip_restores_active_run);
    RUN_TEST(test_tombstone_has_empty_run_id_and_rejects_active_data);
    RUN_TEST(
        test_projection_rejects_inconsistent_aggregate_instead_of_prioritizing);
    RUN_TEST(test_payload_bounds_and_truncation_are_strict);
    RUN_TEST(test_manual_completed_round_trip_is_a_valid_run_projection);
    RUN_TEST(test_prepared_head_binds_full_transaction_contract);
    RUN_TEST(
        test_head_reference_and_mutation_invariants_reject_invalid_contracts);
    RUN_TEST(test_manual_snapshot_and_runtime_shape_must_be_canonical);
    RUN_TEST(test_schema_one_payload_decodes_without_sensor_selection_field);
    RUN_TEST(test_head_reference_accepts_known_schemas_and_rejects_unknown_ones);
    RUN_TEST(test_committed_head_accepts_mixed_current_and_fallback_schema);
    return UNITY_END();
}
