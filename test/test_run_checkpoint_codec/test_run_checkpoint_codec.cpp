#include <unity.h>

#include "run_persistence_codec.hpp"
#include "standard_program_catalog.hpp"
#include "storage_envelope.hpp"

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

// Genuine schema-1 active payload from before sensorSelection existed. The
// runtime state byte is at offset 152; the following schema-2 helper inserts
// only the field introduced by schema 2 and therefore keeps this legacy layout
// otherwise byte-identical.
constexpr std::size_t kSchemaOneRuntimeStateOffset = 152U;
constexpr std::size_t kSchemaTwoRuntimeStateOffset = 159U;

std::string schemaOneActivePayload() {
    return bytesFromHex(
        "01010000000000000064000500000000000e636865636b706f696e742d72756e010000"
        "0001"
        "01005d00000006000000000001ffff000b77617465722d6b65666972000b5761737365"
        "726b"
        "656669720000010101010101000201010000001e030101404300000000000001000000"
        "7801"
        "3fe0000000000000010000000a01000000b400010000000100010000000a000000b400"
        "0100"
        "0000780006000000000000000000000000000000000000000000000100000000000000"
        "63");
}

std::string schemaTwoActivePayload() {
    auto payload = schemaOneActivePayload();
    // present=true, LegacyUnknown, None, lastDecisionRunRevision=0
    payload.insert(33U, std::string("\x01\x04\x01\0\0\0\0", 7U));
    return payload;
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
    // Korrekturauftrag Befund 4: lastDecisionRunRevision <= runRevision.
    state.runRevision = 1U;
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

RunPersistenceSnapshot recoveryEvaluationPendingSnapshot();

RunCommandState manualCommandState() {
    ManualRunPlan plan;
    plan.values.runId = "manual-into";
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
    state.runRevision = 1U;
    state.sensorSelection = PersistedSensorSelectionState{
        SensorSelectionProvenance::InitialSelection,
        SensorSelectionDecisionCause::StartSelection, 1U};
    state.processRunSnapshot = makeProcessRunSnapshot(plan);
    TEST_ASSERT_TRUE(state.processRunSnapshot.has_value());
    state.processState.state = ProcessState::ManualHolding;
    state.processState.stateEnteredAtMillis = 100U;
    return state;
}

void test_program_checkpoint_round_trip_restores_active_run() {
    const auto source = programSnapshot();
    std::string encoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(source, encoded)));
    assertGolden(
        encoded,
        "01010000000000000064000500000001000e636865636b706f696e742d72756e0"
        "1010102000000010000000101005d00000006000000000001ffff000b77617465"
        "722d6b65666972000b5761737365726b656669720000010101010101000201010"
        "000001e03010140430000000000000100000078013fe000000000000001000000"
        "0a01000000b400010000000100010000000a000000b4000100000078000600000"
        "00000000000000000000000000000000000000001000000000000006300000002"
        "0002000200000000000000010000000000");
    const auto decoded =
        decodeRunPersistenceSnapshot(encoded, kCurrentRunPersistenceSchema);
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

void test_inplace_projection_and_restore_match_legacy_semantics() {
    const auto source = programSnapshot();
    RunCommandState restored;
    TEST_ASSERT_TRUE(restoreRunPersistenceSnapshotInto(source, restored));
    std::array<CommandId, kMaximumPersistedRunCommandIds> ids{};
    ids[0] = 99U;

    RunPersistenceSnapshot projected;
    TEST_ASSERT_TRUE(makeRunPersistenceSnapshotInto(
        restored, ids, 1U, source.trigger,
        RunCheckpointTime{source.checkpointMonotonicMillis, 1700000000},
        source.intervalMinutes, projected));
    std::string expectedBytes;
    std::string actualBytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(source, expectedBytes)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(projected, actualBytes)));
    TEST_ASSERT_EQUAL_UINT32(expectedBytes.size(), actualBytes.size());
    TEST_ASSERT_EQUAL_MEMORY(expectedBytes.data(), actualBytes.data(),
                             expectedBytes.size());

    RunPersistenceSnapshot decodedDestination =
        recoveryEvaluationPendingSnapshot();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(decodeRunPersistenceSnapshotInto(
            expectedBytes, kCurrentRunPersistenceSchema, decodedDestination)));
    RunCommandState restoredInto;
    TEST_ASSERT_TRUE(
        restoreRunPersistenceSnapshotInto(decodedDestination, restoredInto));
    TEST_ASSERT_TRUE(restoredInto.activeProgramRun.has_value());
    TEST_ASSERT_EQUAL_STRING(restored.activeRunId.c_str(),
                             restoredInto.activeRunId.c_str());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(restored.processState.state),
                          static_cast<int>(restoredInto.processState.state));
}

void test_snapshot_into_variant_reuse_clears_stale_fields() {
    const auto programState = *restoreRunPersistenceSnapshot(programSnapshot());
    const auto manualState = manualCommandState();
    RunCommandState emptyState;
    emptyState.processState.state = ProcessState::Standby;
    std::array<CommandId, kMaximumPersistedRunCommandIds> ids{};
    RunPersistenceSnapshot destination = programSnapshot();

    TEST_ASSERT_TRUE(makeRunPersistenceSnapshotInto(
        programState, ids, 0U, RunCheckpointTrigger::Command,
        RunCheckpointTime{200U, std::nullopt}, 5U, destination));
    TEST_ASSERT_TRUE(destination.program.has_value());
    TEST_ASSERT_FALSE(destination.manual.has_value());

    TEST_ASSERT_TRUE(makeRunPersistenceSnapshotInto(
        emptyState, ids, 0U, RunCheckpointTrigger::Transition,
        RunCheckpointTime{201U, std::nullopt}, 5U, destination));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunCheckpointVariant::NoActiveRun),
                          static_cast<int>(destination.variant));
    TEST_ASSERT_FALSE(destination.program.has_value());
    TEST_ASSERT_FALSE(destination.manual.has_value());
    TEST_ASSERT_FALSE(destination.sensorSelection.has_value());
    TEST_ASSERT_FALSE(destination.processRunSnapshot.has_value());
    TEST_ASSERT_EQUAL_UINT32(0U, destination.revisionCount);
    TEST_ASSERT_FALSE(destination.pendingRecoveryAnchor.has_value());
    TEST_ASSERT_FALSE(destination.lastRecoveryEpisodeEvidence.has_value());
    TEST_ASSERT_FALSE(destination.priorBootPhaseElapsed.has_value());
    TEST_ASSERT_FALSE(destination.nominalRecoveryAdjustment.has_value());
    TEST_ASSERT_EQUAL_UINT32(0U, destination.runProgress.observedRunSeconds);
    TEST_ASSERT_FALSE(destination.runProgress.weightedProgress.has_value());

    emptyState.activeRunId = "stale-invalid-projection";
    TEST_ASSERT_FALSE(makeRunPersistenceSnapshotInto(
        emptyState, ids, 0U, RunCheckpointTrigger::Command,
        RunCheckpointTime{201U, std::nullopt}, 5U, destination));
    emptyState.activeRunId.clear();

    TEST_ASSERT_TRUE(makeRunPersistenceSnapshotInto(
        manualState, ids, 0U, RunCheckpointTrigger::Transition,
        RunCheckpointTime{202U, std::nullopt}, 5U, destination));
    TEST_ASSERT_FALSE(destination.program.has_value());
    TEST_ASSERT_TRUE(destination.manual.has_value());

    TEST_ASSERT_TRUE(makeRunPersistenceSnapshotInto(
        programState, ids, 0U, RunCheckpointTrigger::Command,
        RunCheckpointTime{203U, std::nullopt}, 5U, destination));
    TEST_ASSERT_TRUE(destination.program.has_value());
    TEST_ASSERT_FALSE(destination.manual.has_value());
}

void test_active_recovery_fault_requires_schema_three() {
    const auto schemaOne = schemaOneActivePayload();
    const auto schemaOneDecoded = decodeRunPersistenceSnapshot(schemaOne, 1U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(schemaOneDecoded.status));
    TEST_ASSERT_TRUE(schemaOneDecoded.snapshot.has_value());
    TEST_ASSERT_EQUAL_UINT8(
        6U, static_cast<std::uint8_t>(schemaOne[kSchemaOneRuntimeStateOffset]));
    auto schemaOneFault = schemaOne;
    schemaOneFault[kSchemaOneRuntimeStateOffset] = static_cast<char>(14U);
    TEST_ASSERT_EQUAL_UINT32(schemaOne.size(), schemaOneFault.size());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::InvalidWireValue),
        static_cast<int>(
            decodeRunPersistenceSnapshot(schemaOneFault, 1U).status));

    const auto schemaTwo = schemaTwoActivePayload();
    const auto schemaTwoDecoded = decodeRunPersistenceSnapshot(schemaTwo, 2U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(schemaTwoDecoded.status));
    TEST_ASSERT_TRUE(schemaTwoDecoded.snapshot.has_value());
    TEST_ASSERT_EQUAL_UINT8(
        6U, static_cast<std::uint8_t>(schemaTwo[kSchemaTwoRuntimeStateOffset]));
    auto schemaTwoFault = schemaTwo;
    schemaTwoFault[kSchemaTwoRuntimeStateOffset] = static_cast<char>(14U);
    TEST_ASSERT_EQUAL_UINT32(schemaTwo.size(), schemaTwoFault.size());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::InvalidWireValue),
        static_cast<int>(
            decodeRunPersistenceSnapshot(schemaTwoFault, 2U).status));

    auto fault = programSnapshot();
    fault.processState.state = ProcessState::Fault;
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshotForSchema(fault, 1U));
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshotForSchema(fault, 2U));
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshotForSchema(
        fault, kCurrentRunPersistenceSchema));
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshotForSchema(fault, 4U));

    std::string encoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(fault, encoded)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::InvalidWireValue),
        static_cast<int>(decodeRunPersistenceSnapshot(encoded, 4U).status));
    for (const std::uint32_t legacySchema : {1U, 2U}) {
        TEST_ASSERT_NOT_EQUAL(
            static_cast<int>(RunPersistenceCodecStatus::Success),
            static_cast<int>(
                decodeRunPersistenceSnapshot(encoded, legacySchema).status));
    }
    const auto decoded =
        decodeRunPersistenceSnapshot(encoded, kCurrentRunPersistenceSchema);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(decoded.status));
    TEST_ASSERT_TRUE(decoded.snapshot.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Fault),
        static_cast<int>(decoded.snapshot->processState.state));
    const auto restored = restoreRunPersistenceSnapshot(*decoded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fault),
                          static_cast<int>(restored->processState.state));
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
    assertGolden(
        encoded,
        "03020000000000000064000500000000000003000000000000000000000000000"
        "0000000000000000000000000020002000200000000000000010000000000");
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
    const auto invalidWire =
        decodeRunPersistenceSnapshot(bytes, kCurrentRunPersistenceSchema);
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
    // Korrekturauftrag Befund 4: lastDecisionRunRevision <= runRevision.
    state.runRevision = 1U;
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
    assertGolden(
        bytes,
        "0202000000000000006400050000000100116d616e75616c2d636865636b706f6"
        "96e74020101020000000140280000000000000200003fe0000000000000000000"
        "0a000000b401000000000000000a020200010000000a000000b40000000c00000"
        "00000000064000000000000000000000000000000000000020002000200000000"
        "000000010000000000");
    const auto decoded =
        decodeRunPersistenceSnapshot(bytes, kCurrentRunPersistenceSchema);
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
    // Korrekturauftrag Befund 4: lastDecisionRunRevision <= runRevision.
    state.runRevision = 1U;
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

void test_sensor_selection_cross_field_invariants_are_enforced() {
    // Korrekturauftrag Befund 4, Pflichttest "Schema-1-Restore und
    // Schema-2-Pflichtfeld/Invarianten": lastDecisionRunRevision <=
    // runRevision, cause == None genau mit Revision 0, sowie
    // Provenienz/aktiver-Modus-Bindung fuer FallbackActive/ReturnedToProduct.
    // LegacyUnknown (Schema-1-Restore) schraenkt den Modus bewusst nicht ein.
    const auto base = programSnapshot();
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(base));

    auto revisionTooHigh = base;
    revisionTooHigh.sensorSelection->lastDecisionRunRevision =
        base.runRevision + 1U;
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(revisionTooHigh));

    auto causeNoneWithNonzeroRevision = base;
    causeNoneWithNonzeroRevision.sensorSelection->lastDecisionCause =
        SensorSelectionDecisionCause::None;
    TEST_ASSERT_FALSE(
        validateRunPersistenceSnapshot(causeNoneWithNonzeroRevision));

    auto nonNoneCauseWithZeroRevision = base;
    nonNoneCauseWithZeroRevision.sensorSelection->lastDecisionRunRevision = 0U;
    TEST_ASSERT_FALSE(
        validateRunPersistenceSnapshot(nonNoneCauseWithZeroRevision));

    auto fallbackActiveWithProductMode = base;
    fallbackActiveWithProductMode.sensorSelection->provenance =
        SensorSelectionProvenance::FallbackActive;
    fallbackActiveWithProductMode.activeRunSensorMode = RunSensorMode::Product;
    TEST_ASSERT_FALSE(
        validateRunPersistenceSnapshot(fallbackActiveWithProductMode));

    auto returnedToProductWithAirMode = base;
    returnedToProductWithAirMode.sensorSelection->provenance =
        SensorSelectionProvenance::ReturnedToProduct;
    returnedToProductWithAirMode.activeRunSensorMode = RunSensorMode::Air;
    TEST_ASSERT_FALSE(
        validateRunPersistenceSnapshot(returnedToProductWithAirMode));

    auto legacyUnknownAnyMode = base;
    legacyUnknownAnyMode.sensorSelection->provenance =
        SensorSelectionProvenance::LegacyUnknown;
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(legacyUnknownAnyMode));
}

// 5.14 Punkt 2: RecoveryEvaluation eines aktiven Runs ist nur mit
// vollstaendigem, konsistentem PendingRecoveryAnchor-Kontext gueltig. Alle
// sechs neuen Schema-3-Felder werden hier bewusst auf tatsaechlich von Null
// verschiedene Werte gesetzt (Testmatrix 9: ein Nullwert-Roundtrip koennte
// eine fehlende Codierung nicht von einem frischen/leeren Zustand
// unterscheiden).
RunPersistenceSnapshot recoveryEvaluationPendingSnapshot() {
    auto snapshot = programSnapshot();
    snapshot.processState.state = ProcessState::RecoveryEvaluation;

    PendingRecoveryAnchor anchor;
    anchor.originalProcessState.state = ProcessState::Fermenting;
    anchor.originalProcessState.stateEnteredAtMillis = 40U;
    anchor.originalProcessState.transitionSequence = 6U;
    anchor.knownPhaseSecondsAtOriginalCheckpoint = 123U;
    anchor.originalCheckpointUtc = 1700000000;
    anchor.originalCheckpointTrigger = RunCheckpointTrigger::Periodic;
    anchor.originalCheckpointIntervalMinutes = 7U;
    anchor.accumulatedBeforeEpisode = PriorBootPhaseElapsed{50U, 200U};
    anchor.knownSecondsSinceOriginalCheckpoint = 30U;
    snapshot.pendingRecoveryAnchor = anchor;
    snapshot.recoveryBootAnchorMonotonicMillis = 555U;

    const RoleTemperatureEvidence airEvidence{
        4.5, device_platform::SensorQuality::Valid};
    const RoleTemperatureEvidence productEvidence{
        5.5, device_platform::SensorQuality::Valid};
    const RoleTemperatureEvidence coolingEvidence{
        -2.5, device_platform::SensorQuality::Stale};
    snapshot.recoveryTemperatureEvidence.lastKnown =
        CrossRoleEvidence{airEvidence, productEvidence, coolingEvidence};

    RecoveryEpisodeEvidence episode;
    episode.beforeOutage =
        CrossRoleEvidence{airEvidence, productEvidence, coolingEvidence};
    episode.firstAfterRestart.product = productEvidence;
    episode.weightedProgressSegmentId = 7U;
    snapshot.lastRecoveryEpisodeEvidence = episode;

    TaggedPriorBootPhaseElapsed tagged;
    tagged.taggedState = ProcessState::Fermenting;
    tagged.elapsed = PriorBootPhaseElapsed{10U, std::nullopt};
    snapshot.priorBootPhaseElapsed = tagged;

    snapshot.nominalRecoveryAdjustment =
        NominalRecoveryAdjustmentState{40U, 3U, 15U};
    snapshot.recoveryEpisodeRevision = 12U;

    WeightedProgressProvenance provenance;
    provenance.lastSourceRole = RunSensorMode::Product;
    provenance.confidence = WeightedProgressConfidence::ProductPreferred;
    provenance.modelRevision = 5U;
    provenance.lastAppliedSegmentId = 7U;
    WeightedProgressState weighted;
    weighted.cumulative = WeightedProgressBounds{100U, 300U};
    weighted.coverage = WeightedProgressCoverage::Complete;
    weighted.lastApplied = provenance;
    snapshot.runProgress.basis = RunProgressBasis::KnownTotal;
    snapshot.runProgress.observedRunSeconds = 999U;
    snapshot.runProgress.weightedProgress = weighted;

    return snapshot;
}

void test_recovery_evaluation_pending_context_round_trips_with_nonzero_values() {
    const auto snapshot = recoveryEvaluationPendingSnapshot();
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(snapshot));

    std::string bytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(snapshot, bytes)));
    const auto decoded =
        decodeRunPersistenceSnapshot(bytes, kCurrentRunPersistenceSchema);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(decoded.status));
    TEST_ASSERT_TRUE(decoded.snapshot.has_value());
    const auto& d = *decoded.snapshot;

    TEST_ASSERT_TRUE(d.pendingRecoveryAnchor.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Fermenting),
        static_cast<int>(d.pendingRecoveryAnchor->originalProcessState.state));
    TEST_ASSERT_EQUAL_UINT64(
        123U, d.pendingRecoveryAnchor->knownPhaseSecondsAtOriginalCheckpoint);
    TEST_ASSERT_TRUE(
        d.pendingRecoveryAnchor->originalCheckpointUtc.has_value());
    TEST_ASSERT_EQUAL_INT64(1700000000,
                            *d.pendingRecoveryAnchor->originalCheckpointUtc);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunCheckpointTrigger::Periodic),
        static_cast<int>(d.pendingRecoveryAnchor->originalCheckpointTrigger));
    TEST_ASSERT_EQUAL_UINT32(
        7U, d.pendingRecoveryAnchor->originalCheckpointIntervalMinutes);
    TEST_ASSERT_EQUAL_UINT32(
        50U,
        d.pendingRecoveryAnchor->accumulatedBeforeEpisode.lowerBoundSeconds);
    TEST_ASSERT_TRUE(d.pendingRecoveryAnchor->accumulatedBeforeEpisode
                         .upperBoundSeconds.has_value());
    TEST_ASSERT_EQUAL_UINT32(
        200U,
        *d.pendingRecoveryAnchor->accumulatedBeforeEpisode.upperBoundSeconds);
    TEST_ASSERT_EQUAL_UINT64(
        30U, d.pendingRecoveryAnchor->knownSecondsSinceOriginalCheckpoint);

    TEST_ASSERT_TRUE(d.recoveryBootAnchorMonotonicMillis.has_value());
    TEST_ASSERT_EQUAL_UINT64(555U, *d.recoveryBootAnchorMonotonicMillis);

    TEST_ASSERT_TRUE(d.recoveryTemperatureEvidence.lastKnown.air.filteredCelsius
                         .has_value());
    TEST_ASSERT_EQUAL_DOUBLE(
        4.5, *d.recoveryTemperatureEvidence.lastKnown.air.filteredCelsius);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::SensorQuality::Stale),
        static_cast<int>(
            d.recoveryTemperatureEvidence.lastKnown.cooling.quality));

    TEST_ASSERT_TRUE(d.lastRecoveryEpisodeEvidence.has_value());
    TEST_ASSERT_TRUE(
        d.lastRecoveryEpisodeEvidence->firstAfterRestart.product.has_value());
    TEST_ASSERT_FALSE(
        d.lastRecoveryEpisodeEvidence->firstAfterRestart.air.has_value());
    TEST_ASSERT_TRUE(
        d.lastRecoveryEpisodeEvidence->weightedProgressSegmentId.has_value());
    TEST_ASSERT_EQUAL_UINT32(
        7U, *d.lastRecoveryEpisodeEvidence->weightedProgressSegmentId);

    TEST_ASSERT_TRUE(d.priorBootPhaseElapsed.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Fermenting),
        static_cast<int>(d.priorBootPhaseElapsed->taggedState));
    TEST_ASSERT_EQUAL_UINT32(
        10U, d.priorBootPhaseElapsed->elapsed.lowerBoundSeconds);
    TEST_ASSERT_FALSE(
        d.priorBootPhaseElapsed->elapsed.upperBoundSeconds.has_value());

    TEST_ASSERT_TRUE(d.nominalRecoveryAdjustment.has_value());
    TEST_ASSERT_EQUAL_UINT32(
        40U, d.nominalRecoveryAdjustment->cumulativeAppliedSeconds);
    TEST_ASSERT_EQUAL_UINT32(
        3U, d.nominalRecoveryAdjustment->lastAppliedEpisodeRevision);
    TEST_ASSERT_EQUAL_UINT32(
        15U, d.nominalRecoveryAdjustment->lastAppliedEpisodeDelta);

    TEST_ASSERT_EQUAL_UINT32(12U, d.recoveryEpisodeRevision);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunProgressBasis::KnownTotal),
                          static_cast<int>(d.runProgress.basis));
    TEST_ASSERT_EQUAL_UINT32(999U, d.runProgress.observedRunSeconds);
    TEST_ASSERT_TRUE(d.runProgress.weightedProgress.has_value());
    TEST_ASSERT_EQUAL_UINT64(
        100U, d.runProgress.weightedProgress->cumulative.lowerBoundSeconds);
    TEST_ASSERT_TRUE(d.runProgress.weightedProgress->cumulative
                         .upperBoundSeconds.has_value());
    TEST_ASSERT_EQUAL_UINT64(
        300U, *d.runProgress.weightedProgress->cumulative.upperBoundSeconds);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WeightedProgressCoverage::Complete),
        static_cast<int>(d.runProgress.weightedProgress->coverage));
    TEST_ASSERT_TRUE(d.runProgress.weightedProgress->lastApplied.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunSensorMode::Product),
        static_cast<int>(
            d.runProgress.weightedProgress->lastApplied->lastSourceRole));
    TEST_ASSERT_EQUAL_UINT32(
        5U, d.runProgress.weightedProgress->lastApplied->modelRevision);
    TEST_ASSERT_EQUAL_UINT32(
        7U, d.runProgress.weightedProgress->lastApplied->lastAppliedSegmentId);

    // Ueber mehrere Reboots byte-identisch (5.12, Testmatrix 9): ein erneuter
    // Roundtrip auf dem bereits decodierten Snapshot darf keine Feldwerte
    // veraendern.
    std::string reencoded;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(d, reencoded)));
    TEST_ASSERT_TRUE(bytes == reencoded);
}

void test_recovery_evaluation_without_pending_context_is_invalid() {
    auto snapshot = programSnapshot();
    snapshot.processState.state = ProcessState::RecoveryEvaluation;
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(snapshot));
}

void test_pending_recovery_anchor_with_wrong_original_phase_is_invalid() {
    auto snapshot = recoveryEvaluationPendingSnapshot();
    // "Falsche Phase fuer den Snapshot" (5.14 Punkt 2): originalProcessState
    // passt strukturell nicht mehr zum mitgelieferten Programm-Snapshot
    // (kind == Timed, kein ManualHolding).
    snapshot.pendingRecoveryAnchor->originalProcessState.state =
        ProcessState::ManualHolding;
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(snapshot));
}

void test_prior_boot_phase_elapsed_tag_mismatch_is_invalid() {
    auto snapshot = recoveryEvaluationPendingSnapshot();
    snapshot.priorBootPhaseElapsed->taggedState = ProcessState::CoolHolding;
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(snapshot));
}

void test_no_active_run_rejects_lingering_recovery_and_progress_fields() {
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    std::array<CommandId, kMaximumPersistedRunCommandIds> ids{};
    const auto tombstone = makeRunPersistenceSnapshot(
        state, ids, 0U, RunCheckpointTrigger::Transition,
        RunCheckpointTime{100U, std::nullopt}, 5U);
    TEST_ASSERT_TRUE(tombstone.has_value());
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(*tombstone));

    auto withAnchor = *tombstone;
    withAnchor.pendingRecoveryAnchor = PendingRecoveryAnchor{};
    withAnchor.recoveryBootAnchorMonotonicMillis = 1U;
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(withAnchor));

    auto withEpisodeEvidence = *tombstone;
    withEpisodeEvidence.lastRecoveryEpisodeEvidence = RecoveryEpisodeEvidence{};
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(withEpisodeEvidence));

    auto withPriorBootPhaseElapsed = *tombstone;
    withPriorBootPhaseElapsed.priorBootPhaseElapsed =
        TaggedPriorBootPhaseElapsed{};
    TEST_ASSERT_FALSE(
        validateRunPersistenceSnapshot(withPriorBootPhaseElapsed));

    auto withNominalAdjustment = *tombstone;
    withNominalAdjustment.nominalRecoveryAdjustment =
        NominalRecoveryAdjustmentState{};
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(withNominalAdjustment));

    auto withWeightedProgress = *tombstone;
    withWeightedProgress.runProgress.weightedProgress = WeightedProgressState{};
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(withWeightedProgress));
}

void test_pending_recovery_anchor_survives_resume_while_time_question_stays_open() {
    // 5.14 Punkt 3 (neu, "Zeitbewertung noch offen"): ein bereits resumter
    // Lauf (Fermenting) darf einen weiterhin gesetzten PendingRecoveryAnchor
    // tragen, solange priorBootPhaseElapsed regulaer getaggt ist und dessen
    // akkumulierte Obergrenze unbekannt bleibt.
    auto snapshot = programSnapshot();
    snapshot.processState.state = ProcessState::Fermenting;

    PendingRecoveryAnchor anchor;
    anchor.originalProcessState.state = ProcessState::Fermenting;
    anchor.knownPhaseSecondsAtOriginalCheckpoint = 90U;
    anchor.originalCheckpointUtc = 1700000000;
    snapshot.pendingRecoveryAnchor = anchor;
    snapshot.recoveryBootAnchorMonotonicMillis = 200U;

    TaggedPriorBootPhaseElapsed tagged;
    tagged.taggedState = ProcessState::Fermenting;
    tagged.elapsed = PriorBootPhaseElapsed{90U, std::nullopt};
    snapshot.priorBootPhaseElapsed = tagged;

    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(snapshot));

    // Dieselbe Konstellation, aber mit bekannter akkumulierter Obergrenze:
    // der Anker muesste dann laengst geloescht sein
    // (recoveryTimeResolvedAtResume), ein weiterhin gesetzter Anker ist
    // strukturell inkonsistent.
    auto withKnownUpperBound = snapshot;
    withKnownUpperBound.priorBootPhaseElapsed->elapsed.upperBoundSeconds = 300U;
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(withKnownUpperBound));

    // Keine der drei erlaubten Phasen (WaitingForProduct/Fermenting/
    // CoolHolding): ReachingTarget statt Cooling, damit die Ablehnung
    // nachweisbar aus der Anker-Phaseninvariante kommt, nicht aus einer
    // unabhaengigen completionMode-/stateMatchesRunSnapshot-Inkompatibilitaet
    // (water-kefir verwendet FinishWithoutCooling und waere fuer Cooling
    // ohnehin strukturell inkompatibel).
    auto reachingTargetWithAnchor = snapshot;
    reachingTargetWithAnchor.processState.state = ProcessState::ReachingTarget;
    reachingTargetWithAnchor.priorBootPhaseElapsed.reset();
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(reachingTargetWithAnchor));
}

// Korrekturauftrag Befund 1: stateMatchesRunSnapshot() liefert fuer nicht
// run-gebundene Zustaende (Boot/Standby/Completed/RecoveryEvaluation/Fault/
// ServiceMode) absichtlich true - eine echte Recovery-Altphase muss
// zusaetzlich zu den acht tatsaechlich snapshot-gebundenen Phasen gehoeren
// (stateUsesRunSnapshot()).
void test_pending_recovery_anchor_valid_for_manual_run_manual_holding_origin() {
    ManualRunPlan plan;
    plan.values.runId = "manual-recovery";
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
    state.runRevision = 1U;
    state.sensorSelection = PersistedSensorSelectionState{
        SensorSelectionProvenance::InitialSelection,
        SensorSelectionDecisionCause::StartSelection, 1U};
    state.processRunSnapshot = makeProcessRunSnapshot(plan);
    state.processState.state = ProcessState::ManualHolding;
    state.processState.stateEnteredAtMillis = 100U;
    std::array<CommandId, kMaximumPersistedRunCommandIds> ids{};
    auto snapshot = makeRunPersistenceSnapshot(
        state, ids, 0U, RunCheckpointTrigger::Transition,
        RunCheckpointTime{100U, std::nullopt}, 5U);
    TEST_ASSERT_TRUE(snapshot.has_value());

    snapshot->processState.state = ProcessState::RecoveryEvaluation;
    PendingRecoveryAnchor anchor;
    anchor.originalProcessState.state = ProcessState::ManualHolding;
    anchor.knownPhaseSecondsAtOriginalCheckpoint = 12U;
    snapshot->pendingRecoveryAnchor = anchor;
    snapshot->recoveryBootAnchorMonotonicMillis = 20U;
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(*snapshot));
}

void test_pending_recovery_anchor_with_non_recovery_original_phase_is_invalid() {
    // Completed sowie vier weitere, strukturell nie snapshot-gebundene
    // Zustaende: keiner davon kann eine echte Vor-Ausfall-Recoveryphase sein.
    for (const auto originalState :
         {ProcessState::Completed, ProcessState::Boot, ProcessState::Standby,
          ProcessState::ServiceMode, ProcessState::Fault}) {
        auto snapshot = recoveryEvaluationPendingSnapshot();
        snapshot.pendingRecoveryAnchor->originalProcessState.state =
            originalState;
        TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(snapshot));
    }
}

void test_prior_boot_phase_elapsed_bounds_are_validated() {
    // Pending-Anker: geordnete Grenzen (Testmatrix "geordnet gueltig"/"lower
    // == upper gueltig") bereits durch den Roundtrip-Test bewiesen
    // (accumulatedBeforeEpisode = {50, 200}); hier die Grenzfaelle.
    auto anchorEqual = recoveryEvaluationPendingSnapshot();
    anchorEqual.pendingRecoveryAnchor->accumulatedBeforeEpisode =
        PriorBootPhaseElapsed{100U, 100U};
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(anchorEqual));

    auto anchorInverted = recoveryEvaluationPendingSnapshot();
    anchorInverted.pendingRecoveryAnchor->accumulatedBeforeEpisode =
        PriorBootPhaseElapsed{200U, 50U};
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(anchorInverted));

    // Getaggtes Feld: dieselbe Invariante.
    auto taggedEqual = recoveryEvaluationPendingSnapshot();
    taggedEqual.priorBootPhaseElapsed->elapsed =
        PriorBootPhaseElapsed{100U, 100U};
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(taggedEqual));

    auto taggedInverted = recoveryEvaluationPendingSnapshot();
    taggedInverted.priorBootPhaseElapsed->elapsed =
        PriorBootPhaseElapsed{200U, 50U};
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(taggedInverted));
}

void test_weighted_progress_provenance_role_and_confidence_must_match() {
    auto validProduct = recoveryEvaluationPendingSnapshot();
    validProduct.runProgress.weightedProgress->lastApplied->lastSourceRole =
        RunSensorMode::Product;
    validProduct.runProgress.weightedProgress->lastApplied->confidence =
        WeightedProgressConfidence::ProductPreferred;
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(validProduct));

    auto validAir = recoveryEvaluationPendingSnapshot();
    validAir.runProgress.weightedProgress->lastApplied->lastSourceRole =
        RunSensorMode::Air;
    validAir.runProgress.weightedProgress->lastApplied->confidence =
        WeightedProgressConfidence::AirReduced;
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(validAir));

    auto productWithAirReduced = recoveryEvaluationPendingSnapshot();
    productWithAirReduced.runProgress.weightedProgress->lastApplied
        ->lastSourceRole = RunSensorMode::Product;
    productWithAirReduced.runProgress.weightedProgress->lastApplied
        ->confidence = WeightedProgressConfidence::AirReduced;
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(productWithAirReduced));
    std::string productWithAirReducedBytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(
            productWithAirReduced, productWithAirReducedBytes)));
    const auto productWithAirReducedDecoded = decodeRunPersistenceSnapshot(
        productWithAirReducedBytes, kCurrentRunPersistenceSchema);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(productWithAirReducedDecoded.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WeightedProgressConfidence::AirReduced),
        static_cast<int>(productWithAirReducedDecoded.snapshot->runProgress
                             .weightedProgress->lastApplied->confidence));

    auto airWithProductPreferred = recoveryEvaluationPendingSnapshot();
    airWithProductPreferred.runProgress.weightedProgress->lastApplied
        ->lastSourceRole = RunSensorMode::Air;
    airWithProductPreferred.runProgress.weightedProgress->lastApplied
        ->confidence = WeightedProgressConfidence::ProductPreferred;
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(airWithProductPreferred));
}

void test_first_after_restart_evidence_requires_canonical_latch_form() {
    // Gesetzt + Valid + Wert: gueltig (Positivfall bereits durch den
    // bestehenden Roundtrip-Test bewiesen, hier zusaetzlich direkt geprueft).
    auto validWithValue = recoveryEvaluationPendingSnapshot();
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(validWithValue));

    // Nicht gesetzt: weiterhin gueltig/latchbar.
    auto notSet = recoveryEvaluationPendingSnapshot();
    notSet.lastRecoveryEpisodeEvidence->firstAfterRestart.air.reset();
    notSet.lastRecoveryEpisodeEvidence->firstAfterRestart.product.reset();
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(notSet));

    // Gesetzt + Valid ohne Wert: strukturell unmoeglich (5.20-Latch verlangt
    // beides gemeinsam) - ungueltig.
    auto validWithoutValue = recoveryEvaluationPendingSnapshot();
    validWithoutValue.lastRecoveryEpisodeEvidence->firstAfterRestart.air =
        RoleTemperatureEvidence{std::nullopt,
                                device_platform::SensorQuality::Valid};
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(validWithoutValue));

    // Gesetzt + Stale/Failed: ebenso unmoeglich, der Latch traegt
    // ausschliesslich Valid-Werte.
    auto staleWithValue = recoveryEvaluationPendingSnapshot();
    staleWithValue.lastRecoveryEpisodeEvidence->firstAfterRestart.air =
        RoleTemperatureEvidence{3.0, device_platform::SensorQuality::Stale};
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(staleWithValue));

    auto failedWithValue = recoveryEvaluationPendingSnapshot();
    failedWithValue.lastRecoveryEpisodeEvidence->firstAfterRestart.air =
        RoleTemperatureEvidence{3.0, device_platform::SensorQuality::Failed};
    TEST_ASSERT_FALSE(validateRunPersistenceSnapshot(failedWithValue));
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
        "44505246000100080000000300000000000000090000000000000014000000770"
        "0b42ede08010100000000010000000000000009000000000000000a0000000b00"
        "00000c01010100000001000000000000000900000000000000090000000800000"
        "0070301000000010000000000000009000000000000000b0000000d0000000e03"
        "0101000000000000005800000004000000050000000600000007");
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

void test_prepared_head_round_trips_recovery_mutation_kind() {
    const RunCheckpointReference current{
        0U, 1U, 9U, 10U, 11U, 12U, RunCheckpointVariant::ProgramRun};
    const RunCheckpointReference target{
        1U, 1U, 9U, 11U, 13U, 14U, RunCheckpointVariant::ProgramRun};
    RunPersistenceHead recovery;
    recovery.state = RunPersistenceHeadState::Prepared;
    recovery.revision = 20U;
    recovery.preparedCurrent = current;
    recovery.target = target;
    recovery.mutationKind = RunPersistenceMutationKind::Recovery;
    recovery.newRunRevision = 5U;
    recovery.newTransitionSequence = 7U;

    const auto encoded =
        encodeRunPersistenceHead(recovery, device_platform::StorageEpoch(9U));
    TEST_ASSERT_TRUE(encoded.has_value());
    const auto decoded =
        decodeRunPersistenceHead(*encoded, device_platform::StorageEpoch(9U));
    TEST_ASSERT_TRUE(decoded.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceMutationKind::Recovery),
        static_cast<int>(decoded->mutationKind));
    TEST_ASSERT_FALSE(decoded->commandId.has_value());
}

void test_recovery_mutation_kind_requires_schema_three() {
    const auto epoch = device_platform::StorageEpoch(9U);
    const RunCheckpointReference current{
        0U, 1U, 9U, 10U, 11U, 12U, RunCheckpointVariant::ProgramRun};
    const RunCheckpointReference target{
        1U, 1U, 9U, 11U, 13U, 14U, RunCheckpointVariant::ProgramRun};
    RunPersistenceHead recovery;
    recovery.state = RunPersistenceHeadState::Prepared;
    recovery.revision = 20U;
    recovery.preparedCurrent = current;
    recovery.target = target;
    recovery.mutationKind = RunPersistenceMutationKind::Recovery;
    recovery.newRunRevision = 5U;
    recovery.newTransitionSequence = 7U;

    const auto encoded = encodeRunPersistenceHead(recovery, epoch);
    TEST_ASSERT_TRUE(encoded.has_value());
    for (const std::uint32_t schema : {1U, 2U}) {
        const auto envelope = device_platform::decodeEnvelope(*encoded);
        TEST_ASSERT_TRUE(envelope.envelope.has_value());
        auto legacyEnvelope = *envelope.envelope;
        legacyEnvelope.schemaVersion = schema;
        std::string legacyBytes;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(device_platform::EnvelopeEncodeStatus::Success),
            static_cast<int>(device_platform::encodeEnvelope(
                legacyEnvelope, legacyBytes, 256U)));
        TEST_ASSERT_FALSE(
            decodeRunPersistenceHead(legacyBytes, epoch).has_value());
    }

    TEST_ASSERT_TRUE(decodeRunPersistenceHead(*encoded, epoch).has_value());
}

void test_same_slot_recovery_requires_a_separate_prepared_fallback() {
    const RunCheckpointReference current{
        0U, 1U, 9U, 10U, 11U, 12U, RunCheckpointVariant::ProgramRun};
    const RunCheckpointReference fallback{
        1U, 1U, 9U, 9U, 8U, 7U, RunCheckpointVariant::ProgramRun};

    RunPersistenceHead recovery;
    recovery.state = RunPersistenceHeadState::Prepared;
    recovery.revision = 20U;
    recovery.preparedCurrent = current;
    recovery.target = current;
    recovery.mutationKind = RunPersistenceMutationKind::Recovery;
    recovery.newRunRevision = 5U;
    recovery.newTransitionSequence = 7U;

    // Recovery alone does not authorize overwriting the only current slot.
    TEST_ASSERT_FALSE(
        encodeRunPersistenceHead(recovery, device_platform::StorageEpoch(9U))
            .has_value());

    // A fallback on the same slot is not a fallback at all.
    auto sameSlotFallback = recovery;
    sameSlotFallback.preparedFallback = current;
    TEST_ASSERT_FALSE(encodeRunPersistenceHead(
                          sameSlotFallback, device_platform::StorageEpoch(9U))
                          .has_value());

    // Non-Recovery mutations cannot use the same-slot exception.
    auto nonRecovery = recovery;
    nonRecovery.mutationKind = RunPersistenceMutationKind::Command;
    nonRecovery.preparedFallback = fallback;
    nonRecovery.commandId = 88U;
    TEST_ASSERT_FALSE(
        encodeRunPersistenceHead(nonRecovery, device_platform::StorageEpoch(9U))
            .has_value());

    // The sole allowed wire shape remains encodable: Recovery, same target
    // slot, and a valid fallback on the other slot.
    auto validRecovery = recovery;
    validRecovery.preparedFallback = fallback;
    TEST_ASSERT_TRUE(encodeRunPersistenceHead(validRecovery,
                                              device_platform::StorageEpoch(9U))
                         .has_value());
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
        "445052460001000800000003000000000000000900000000000000160000003e0"
        "02051b3310200000000010000000000000009000000000000000a0000000b0000"
        "000c0101010000000100000000000000090000000000000009000000080000000"
        "703");

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
        const auto truncated = decodeRunPersistenceSnapshot(
            encoded.substr(0U, cut), kCurrentRunPersistenceSchema);
        TEST_ASSERT_NOT_EQUAL(
            static_cast<int>(RunPersistenceCodecStatus::Success),
            static_cast<int>(truncated.status));
    }
    auto withTrailing = encoded;
    withTrailing.push_back('\0');
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::TrailingBytes),
        static_cast<int>(decodeRunPersistenceSnapshot(
                             withTrailing, kCurrentRunPersistenceSchema)
                             .status));
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
    const auto schemaOnePayload = schemaOneActivePayload();
    const auto decoded = decodeRunPersistenceSnapshot(schemaOnePayload, 1U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(decoded.status));
    TEST_ASSERT_TRUE(decoded.snapshot.has_value());
    TEST_ASSERT_EQUAL_STRING("checkpoint-run",
                             decoded.snapshot->activeRunId.c_str());
    // Korrekturauftrag Befund 4: ein Schema-1-Restore wird auf den expliziten
    // LegacyUnknown/None/0-Sentinelwert abgebildet statt das Feld leer zu
    // lassen - das macht die Pflichtfeldregel in validateRunPersistenceSnapshot
    // unbedingt statt schema-abhaengig.
    TEST_ASSERT_TRUE(decoded.snapshot->sensorSelection.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionProvenance::LegacyUnknown),
        static_cast<int>(decoded.snapshot->sensorSelection->provenance));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionDecisionCause::None),
        static_cast<int>(decoded.snapshot->sensorSelection->lastDecisionCause));
    TEST_ASSERT_EQUAL_UINT32(
        0U, decoded.snapshot->sensorSelection->lastDecisionRunRevision);
    // 5.28: eine Schema-1-Decodierung kann keine der neuen Schema-3-
    // Pending-/Evidenz-/Korrekturkombinationen erzeugen (sie existierten
    // damals nicht) - Regressionstest gegen bestehende Migrationsvektoren.
    // basis startet ehrlich mit PartialUnknownHistory statt eines erfundenen
    // KnownTotal-Altbestands; weightedProgress wird gesetzt, aber ohne
    // erfundenen Beitrag (0/0, PartialUnknown, kein lastApplied).
    TEST_ASSERT_FALSE(decoded.snapshot->pendingRecoveryAnchor.has_value());
    TEST_ASSERT_FALSE(
        decoded.snapshot->recoveryBootAnchorMonotonicMillis.has_value());
    TEST_ASSERT_FALSE(
        decoded.snapshot->lastRecoveryEpisodeEvidence.has_value());
    TEST_ASSERT_FALSE(decoded.snapshot->priorBootPhaseElapsed.has_value());
    TEST_ASSERT_FALSE(decoded.snapshot->nominalRecoveryAdjustment.has_value());
    TEST_ASSERT_EQUAL_UINT32(0U, decoded.snapshot->recoveryEpisodeRevision);
    TEST_ASSERT_FALSE(decoded.snapshot->recoveryTemperatureEvidence.lastKnown
                          .air.filteredCelsius.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunProgressBasis::PartialUnknownHistory),
        static_cast<int>(decoded.snapshot->runProgress.basis));
    TEST_ASSERT_EQUAL_UINT32(0U,
                             decoded.snapshot->runProgress.observedRunSeconds);
    TEST_ASSERT_TRUE(
        decoded.snapshot->runProgress.weightedProgress.has_value());
    TEST_ASSERT_EQUAL_UINT64(0U, decoded.snapshot->runProgress.weightedProgress
                                     ->cumulative.lowerBoundSeconds);
    TEST_ASSERT_FALSE(decoded.snapshot->runProgress.weightedProgress->cumulative
                          .upperBoundSeconds.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WeightedProgressCoverage::PartialUnknown),
        static_cast<int>(
            decoded.snapshot->runProgress.weightedProgress->coverage));
    TEST_ASSERT_FALSE(decoded.snapshot->runProgress.weightedProgress
                          ->lastApplied.has_value());
    // Decoding the identical bytes as schema 2 misreads the following field
    // boundary (there is no presence tag at this offset in a schema-1
    // payload) and must not silently succeed with the wrong shape.
    const auto misread = decodeRunPersistenceSnapshot(
        schemaOnePayload, kCurrentRunPersistenceSchema);
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(misread.status));
}

void test_legacy_decode_reuse_clears_schema_three_only_fields() {
    const auto modern = recoveryEvaluationPendingSnapshot();
    std::string modernBytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(encodeRunPersistenceSnapshot(modern, modernBytes)));
    RunPersistenceSnapshot destination;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCodecStatus::Success),
        static_cast<int>(decodeRunPersistenceSnapshotInto(
            modernBytes, kCurrentRunPersistenceSchema, destination)));
    TEST_ASSERT_EQUAL_UINT32(12U, destination.recoveryEpisodeRevision);
    TEST_ASSERT_TRUE(destination.recoveryTemperatureEvidence.lastKnown.air
                         .filteredCelsius.has_value());
    TEST_ASSERT_EQUAL_UINT32(999U, destination.runProgress.observedRunSeconds);

    const auto legacyBytes = schemaOneActivePayload();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(decodeRunPersistenceSnapshotInto(
                              legacyBytes, 1U, destination)));
    TEST_ASSERT_EQUAL_UINT32(0U, destination.recoveryEpisodeRevision);
    TEST_ASSERT_FALSE(destination.recoveryTemperatureEvidence.lastKnown.air
                          .filteredCelsius.has_value());
    TEST_ASSERT_EQUAL_UINT32(0U, destination.runProgress.observedRunSeconds);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunProgressBasis::PartialUnknownHistory),
        static_cast<int>(destination.runProgress.basis));
    TEST_ASSERT_TRUE(destination.runProgress.weightedProgress.has_value());
    TEST_ASSERT_FALSE(
        destination.runProgress.weightedProgress->lastApplied.has_value());
}

void test_no_active_run_migration_from_schema_one_keeps_default_progress() {
    // 5.28/5.14 Punkt 6: NoActiveRun-Migration erfindet keinen
    // PartialUnknownHistory-Zustand fuer einen nicht existierenden Run - die
    // Migrationsregel gilt ausdruecklich nur fuer variant != NoActiveRun. Ein
    // genuiner Schema-1-Payload (identisch zur bisherigen NoActiveRun-
    // Kodierung vor #18 - dieser Bereich schrieb nie ein schema-abhaengiges
    // Feld) traegt keinen angehaengten Schema-3-Block.
    const auto schemaOnePayload = bytesFromHex(
        "03020000000000000064000500000000000003000000000000000000000000000"
        "0000000000000000000");
    const auto decoded = decodeRunPersistenceSnapshot(schemaOnePayload, 1U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(decoded.status));
    TEST_ASSERT_TRUE(decoded.snapshot.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunProgressBasis::KnownTotal),
        static_cast<int>(decoded.snapshot->runProgress.basis));
    TEST_ASSERT_FALSE(
        decoded.snapshot->runProgress.weightedProgress.has_value());
    TEST_ASSERT_TRUE(validateRunPersistenceSnapshot(*decoded.snapshot));
}

// #21, 9.3: readReference/validReference accept exactly the known schema set
// {1U, 2U} and reject anything else, exercised via the public head codec
// (encodeRunPersistenceHead stamps kCurrentRunPersistenceSchema on the head
// envelope itself; the embedded RunCheckpointReference::schemaVersion is
// independently checked by readReference/validReference).
void test_head_reference_accepts_known_schemas_and_rejects_unknown_ones() {
    const auto epoch = device_platform::StorageEpoch(9U);
    for (const std::uint32_t schema : {1U, 2U, 3U}) {
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
        0U, 4U, 9U, 10U, 11U, 12U, RunCheckpointVariant::ProgramRun};
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
        0U,  kCurrentRunPersistenceSchema,    9U, 11U, 13U,
        14U, RunCheckpointVariant::ProgramRun};
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
    RUN_TEST(test_inplace_projection_and_restore_match_legacy_semantics);
    RUN_TEST(test_snapshot_into_variant_reuse_clears_stale_fields);
    RUN_TEST(test_active_recovery_fault_requires_schema_three);
    RUN_TEST(test_tombstone_has_empty_run_id_and_rejects_active_data);
    RUN_TEST(
        test_projection_rejects_inconsistent_aggregate_instead_of_prioritizing);
    RUN_TEST(test_payload_bounds_and_truncation_are_strict);
    RUN_TEST(test_manual_completed_round_trip_is_a_valid_run_projection);
    RUN_TEST(test_prepared_head_binds_full_transaction_contract);
    RUN_TEST(test_prepared_head_round_trips_recovery_mutation_kind);
    RUN_TEST(test_recovery_mutation_kind_requires_schema_three);
    RUN_TEST(test_same_slot_recovery_requires_a_separate_prepared_fallback);
    RUN_TEST(
        test_head_reference_and_mutation_invariants_reject_invalid_contracts);
    RUN_TEST(test_manual_snapshot_and_runtime_shape_must_be_canonical);
    RUN_TEST(test_sensor_selection_cross_field_invariants_are_enforced);
    RUN_TEST(
        test_recovery_evaluation_pending_context_round_trips_with_nonzero_values);
    RUN_TEST(test_recovery_evaluation_without_pending_context_is_invalid);
    RUN_TEST(test_pending_recovery_anchor_with_wrong_original_phase_is_invalid);
    RUN_TEST(test_prior_boot_phase_elapsed_tag_mismatch_is_invalid);
    RUN_TEST(test_no_active_run_rejects_lingering_recovery_and_progress_fields);
    RUN_TEST(
        test_pending_recovery_anchor_survives_resume_while_time_question_stays_open);
    RUN_TEST(
        test_pending_recovery_anchor_valid_for_manual_run_manual_holding_origin);
    RUN_TEST(
        test_pending_recovery_anchor_with_non_recovery_original_phase_is_invalid);
    RUN_TEST(test_prior_boot_phase_elapsed_bounds_are_validated);
    RUN_TEST(test_weighted_progress_provenance_role_and_confidence_must_match);
    RUN_TEST(test_first_after_restart_evidence_requires_canonical_latch_form);
    RUN_TEST(test_schema_one_payload_decodes_without_sensor_selection_field);
    RUN_TEST(test_legacy_decode_reuse_clears_schema_three_only_fields);
    RUN_TEST(
        test_no_active_run_migration_from_schema_one_keeps_default_progress);
    RUN_TEST(
        test_head_reference_accepts_known_schemas_and_rejects_unknown_ones);
    RUN_TEST(test_committed_head_accepts_mixed_current_and_fallback_schema);
    return UNITY_END();
}
