#include <limits>
#include <map>
#include <set>
#include <utility>

#include <unity.h>

#include "run_persistence_coordinator.hpp"
#include "run_persistence_codec.hpp"
#include "run_recovery.hpp"
#include "qualification_orchestrator.hpp"
#include "control_context.hpp"
#include "mock_bidirectional_actuator_sink.hpp"
#include "mock_binary_output_sink.hpp"
// PR-#99-Abschlussreview-Korrektur: der manuelle Transportvertrag-
// Integrationstest (persistCommand fuer ApplySensorSelectionAction) braucht
// CrossRolePlausibilityContext/decideApplySensorSelectionAction - beides nur
// ueber sensor_selection.hpp vollstaendig sichtbar, da run_commands.hpp den
// Typ laut Plan Abschnitt 7 nur vorwaertsdeklariert.
#include "sensor_selection.hpp"
#include "simulated_persistent_state_store.hpp"
#include "state_store.hpp"
#include "state_store_key.hpp"
#include "standard_program_catalog.hpp"
#include "storage_envelope.hpp"
#include "temperature_control_orchestrator.hpp"

namespace fermentation {

class FixedRecoveryProgressModel final : public RecoveryProgressWeightingModel {
   public:
    mutable std::uint32_t calls{0U};
    RunSensorMode sourceRole{RunSensorMode::Product};
    WeightedProgressConfidence confidence{
        WeightedProgressConfidence::ProductPreferred};

    [[nodiscard]] std::optional<WeightedProgressContribution> evaluate(
        const RecoveryProgressWeightingInput& input) const override {
        ++calls;
        if (!hasUsableRecoveryProgressEvidence(input)) return std::nullopt;
        return WeightedProgressContribution{WeightedProgressBounds{10U, 20U},
                                            sourceRole, confidence, 7U};
    }
};

class RunPersistenceCoordinatorTestAccess {
   public:
    static RunPersistenceResult writeSnapshotCore(
        RunPersistenceCoordinator& coordinator,
        const RunPersistenceSnapshot& snapshot, const RunCheckpointTime& time,
        bool periodic, const RunCommandState& before,
        RunPersistenceMutationKind mutationKind,
        std::optional<CommandId> commandId,
        std::optional<std::size_t> targetSlotOverride,
        RunPersistenceFallbackDirective fallbackDirective,
        RunPersistenceCoordinatorState rollbackState) {
        return coordinator.writeSnapshotCore(
            snapshot, time, periodic, before, mutationKind, commandId,
            targetSlotOverride, fallbackDirective, rollbackState);
    }

    static RunCheckpointReference currentReference(
        const RunPersistenceCoordinator& coordinator) {
        return coordinator.currentHead_->current;
    }

    static RunCheckpointReference fallbackReference(
        const RunPersistenceCoordinator& coordinator) {
        return *coordinator.currentHead_->fallback;
    }

    static std::optional<RunCheckpointReference> fallbackReferenceOptional(
        const RunPersistenceCoordinator& coordinator) {
        return coordinator.currentHead_->fallback;
    }

    static std::uint64_t nextCheckpointRevision(
        const RunPersistenceCoordinator& coordinator) {
        return coordinator.nextCheckpointRevision_;
    }

    static std::size_t persistedIdCount(
        const RunPersistenceCoordinator& coordinator) {
        return coordinator.persistedIdCount_;
    }

    static CommandId persistedId(const RunPersistenceCoordinator& coordinator,
                                 std::size_t index) {
        return coordinator.persistedIds_[index];
    }
};
}  // namespace fermentation

namespace {

using namespace fermentation;

class SequencedWriteStore final : public device_platform::IStateStore {
   public:
    using WriteFault =
        device_platform_test_support::SimulatedPersistentStateStore::WriteFault;

    enum class ReadFault {
        None,
        NotFound,
        ReadError,
        CapacityError,
        ForeignBytes
    };

    device_platform::StateStoreWriteStatus write(
        const device_platform::StateStoreKey& key,
        const std::string& value) override {
        ++writeCount_;
        const auto readFault = readFaults_.find(writeCount_);
        if (readFault != readFaults_.end())
            readFaultByKey_[key] = readFault->second;
        if (unknownWithoutCommit_.find(writeCount_) !=
            unknownWithoutCommit_.end())
            return device_platform::StateStoreWriteStatus::CommitOutcomeUnknown;
        const auto fault = faults_.find(writeCount_);
        if (fault != faults_.end()) backing_.setNextWriteFault(fault->second);
        return backing_.write(key, value);
    }

    device_platform::StateStoreReadResult read(
        const device_platform::StateStoreKey& key,
        std::size_t maxBytes) const override {
        const auto fault = readFaultByKey_.find(key);
        if (fault != readFaultByKey_.end()) {
            switch (fault->second) {
                case ReadFault::NotFound:
                    return {device_platform::StateStoreReadStatus::NotFound,
                            {}};
                case ReadFault::ReadError:
                    return {device_platform::StateStoreReadStatus::ReadError,
                            {}};
                case ReadFault::CapacityError:
                    return {
                        device_platform::StateStoreReadStatus::CapacityError,
                        {}};
                case ReadFault::ForeignBytes:
                    return {device_platform::StateStoreReadStatus::Success,
                            "foreign-readback"};
                case ReadFault::None:
                    break;
            }
        }
        return backing_.read(key, maxBytes);
    }

    void faultAt(std::size_t writeNumber, WriteFault fault) {
        faults_[writeNumber] = fault;
    }

    void clearFaults() { faults_.clear(); }

    void unknownWithoutCommitAt(std::size_t writeNumber) {
        unknownWithoutCommit_.insert(writeNumber);
    }

    void readFaultAt(std::size_t writeNumber, ReadFault fault) {
        readFaults_[writeNumber] = fault;
    }

    [[nodiscard]] std::size_t writeCount() const { return writeCount_; }

    void restart() {
        backing_.restart();
        writeCount_ = 0U;
        faults_.clear();
        unknownWithoutCommit_.clear();
        readFaults_.clear();
        readFaultByKey_.clear();
    }

    void forceNotFound(const device_platform::StateStoreKey& key, bool force) {
        backing_.forceNotFound(key, force);
    }

    void injectReadFailure(const device_platform::StateStoreKey& key,
                           bool fail) {
        backing_.injectReadFailure(key, fail);
    }

    [[nodiscard]] auto& backing() { return backing_; }

   private:
    device_platform_test_support::SimulatedPersistentStateStore backing_;
    std::map<std::size_t, WriteFault> faults_;
    std::set<std::size_t> unknownWithoutCommit_;
    std::map<std::size_t, ReadFault> readFaults_;
    mutable std::map<device_platform::StateStoreKey, ReadFault> readFaultByKey_;
    std::size_t writeCount_{0U};
};

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
    TEST_ASSERT_TRUE(validateProgram(*document).valid());
    return *document;
}

// Only the product-wait tests need a preheat-qualified program; every other
// coordinator test keeps using the plain runnableProgram() fixture above.
ProgramDocument preheatProgram() {
    auto document = runnableProgram();
    document.program.preheat = true;
    document.program.maximumProductWaitMinutes = 30U;
    TEST_ASSERT_TRUE(validateProgram(document).valid());
    return document;
}

TemperatureControlParameters bridgeTemperatureParameters() {
    TemperatureControlParameters result;
    result.airHeating = {0.10, 0.01, 1.0, 0.8};
    result.airCooling = {0.10, 0.01, 1.0, 0.8};
    result.productHeating = {0.20, 0.02, 1.0, 0.8};
    result.productCooling = {0.20, 0.02, 1.0, 0.8};
    result.maximumIntegrationStepMillis = 10'000U;
    result.airLimitLowerBlockCelsius = 5.0;
    result.airLimitLowerReduceStartCelsius = 10.0;
    result.airLimitUpperReduceStartCelsius = 30.0;
    result.airLimitUpperBlockCelsius = 35.0;
    return result;
}

IntegratorTransitionPolicy bridgeTemperaturePolicy() {
    return {IntegratorTransitionAction::Reset,
            IntegratorTransitionAction::Reset,
            IntegratorTransitionAction::Reset, 0.2};
}

ActuatorPlannerParameters bridgeActuatorPlannerParameters() {
    // Test-only timing values; no hardware or production commissioning value
    // is inferred from this integration fixture.
    ActuatorPlannerParameters result;
    result.switchingWindowMillis = 10'000U;
    result.minimumOnMillis = 2'000U;
    result.minimumOffMillis = 1'000U;
    result.polarityDeadTimeMillis = 3'000U;
    result.pulseAccumulatorCapMillis = 10'000U;
    result.counterDirectionConfirmationQuoteThreshold = 0.5;
    result.counterDirectionConfirmationDurationMillis = 2'000U;
    result.requestWatchdogMillis = 60'000U;
    result.outerFanPostRunMillis = 1'000U;
    result.innerFanPostRunMillis = 500U;
    return result;
}

// Owner-Review F4: evaluateTemperatureControl() fails closed without a
// planner (Abschnitt 6.1/9.4); any fixture exercising it as the public
// Application API therefore needs the full planner-/driver-bound
// construction, not just the persistence-only 3-argument orchestrator.
// Bundles one instance of every collaborator so evaluateTemperatureControl()
// call sites do not need to hand-repeat this wiring.
struct ActuatorHandoffFixture {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator;
    TargetQualificationEvaluator evaluator;
    TemperatureController controller;
    ActuatorPlanner planner;
    device_platform_test_support::MockBidirectionalActuatorSink peltier;
    device_platform_test_support::MockBinaryOutputSink outerFan;
    device_platform_test_support::MockBinaryOutputSink innerFan;
    ActuatorPlanSinkDriver driver;
    TemperatureControlApplicationOrchestrator application;

    ActuatorHandoffFixture()
        : coordinator(store, device_platform::StorageEpoch(1U),
                      RunCheckpointSchedule{}),
          controller(bridgeTemperatureParameters(), bridgeTemperaturePolicy()),
          planner(bridgeActuatorPlannerParameters()),
          driver(peltier, outerFan, innerFan),
          application(coordinator, controller, evaluator, planner, driver) {
        static_cast<void>(coordinator.loadAndInitialize());
    }
};

TemperatureControlInput bridgeAirInput(std::uint64_t timestamp, double target,
                                       double measured) {
    TemperatureControlInput input;
    input.sampleTimestampMonotonicMillis = timestamp;
    input.targetCelsius = target;
    input.controlSensorRole = ControlSensorRole::Air;
    input.air.quality = device_platform::SensorQuality::Valid;
    // Per #20/FR2, only filteredCelsius is a normal control value.
    input.air.filteredCelsius = measured;
    input.air.rawCelsius = measured;
    return input;
}

void buildQualifierCredit(TargetQualificationEvaluator& evaluator) {
    TargetQualificationInput first;
    first.phase = QualificationPhase::Preheating;
    first.sampleTimestampMonotonicMillis = 100U;
    first.targetCelsius = 20.0;
    first.bandCelsius = 0.5;
    first.qualificationDurationMillis = 1'000U;
    first.effectiveGraceMillis = 500U;
    first.maximumAcceptedSampleGapMillis = 2'000U;
    first.controlSensorRole = ControlSensorRole::Air;
    first.air.quality = device_platform::SensorQuality::Valid;
    // Per #20/FR2, only filteredCelsius is a normal control value.
    first.air.filteredCelsius = 20.0;
    first.air.rawCelsius = 20.0;
    auto firstDecision = evaluator.evaluate(first);
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        firstDecision, {{20.0, 0.5, ControlSensorRole::Air}, 0U, 0U}));

    first.sampleTimestampMonotonicMillis = 1'100U;
    auto credited = evaluator.evaluate(first);
    TEST_ASSERT_TRUE(credited.creditedInBandMillis > 0U);
    TEST_ASSERT_TRUE(evaluator.applyRamOnly(
        credited, {{20.0, 0.5, ControlSensorRole::Air}, 0U, 0U}));
}

RunCommandState readyActiveRunWithSensorSelection(
    RunPersistenceCoordinator& coordinator, CommandId startId);
RunCommandState persistedFermentingRun(RunPersistenceCoordinator& coordinator,
                                       CommandId startId);
RunCommandState persistedCoolHoldingRun(RunPersistenceCoordinator& coordinator,
                                        CommandId startId);
RunCommandState readyActiveManualRunWithSensorSelection(
    RunPersistenceCoordinator& coordinator, CommandId startId);
CommandDecision continueWithAirDecision(const RunCommandState& state,
                                        CommandId id,
                                        std::uint64_t monotonicMillis);
CrossRolePlausibilityContext recoveryPlausibility(
    std::uint64_t evaluatedAtMonotonicMillis, bool productValid = true);

CommandDecision startDecision(
    const RunCommandState& state, CommandId id,
    std::uint64_t monotonicMillis = 100U,
    std::optional<ProgramDocument> program = std::nullopt,
    RunSensorMode sensorMode = RunSensorMode::Product) {
    ProgramStartRequest request;
    request.envelope = {id,
                        CommandSource::LocalDisplay,
                        monotonicMillis,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true,
                        std::nullopt};
    request.runId = "persisted-run";
    request.program = program.has_value() ? *program : runnableProgram();
    request.sourceKind = ProgramSourceKind::FactoryCatalog;
    request.sourceProgramRevision = 1U;
    request.sensorMode = sensorMode;
    request.safetyAllowsStart = true;
    request.airSensorValid = true;
    request.coolingSensorValid = true;
    request.productSensorValid = true;
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
                        true,
                        std::nullopt};
    request.plan.runId = "manual-persisted-run";
    request.plan.targetTemperatureCelsius = 12.0;
    request.plan.sensorMode = RunSensorMode::Air;
    request.plan.qualificationBandCelsius = 0.5;
    request.plan.qualificationDurationMinutes = 10U;
    request.plan.maximumTargetReachMinutes = 180U;
    request.safetyAllowsStart = true;
    request.airSensorValid = true;
    request.coolingSensorValid = true;
    request.productSensorValid = true;
    return decideManualStart(state, request);
}

CommandDecision stopDecision(const RunCommandState& state, CommandId id,
                             std::uint64_t monotonicMillis = 200U) {
    StopRequest request;
    request.envelope = {id,
                        CommandSource::LocalDisplay,
                        monotonicMillis,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true,
                        std::nullopt};
    request.option = StopOption::AbortAndTurnOff;
    return decideStop(state, request);
}

device_platform::StateStoreKey slotKey(const char* name) {
    const auto created = device_platform::StateStoreKey::create(name);
    TEST_ASSERT_TRUE(created.key.has_value());
    return *created.key;
}

void commitTombstone(
    device_platform_test_support::SimulatedPersistentStateStore& store) {
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 601U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    StopRequest stop;
    stop.envelope = {602U,
                     CommandSource::LocalDisplay,
                     200U,
                     state.processState.transitionSequence,
                     state.runRevision,
                     std::nullopt,
                     std::nullopt,
                     true,
                     std::nullopt};
    stop.option = StopOption::AbortAndTurnOff;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, decideStop(state, stop),
                                RunCheckpointTime{200U, std::nullopt})
                .status));
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
    // #21, 6.12: die persistierte Auswahl (sensorSelection) wird
    // uebernommen, der RAM-only-Laufzeitzustand (sensorSelectionRuntime)
    // dagegen fail-closed neu gesetzt - kein Wireformat traegt ihn, und
    // bootlokale Timer sind ueber einen Boot hinweg nicht gueltig. #18 muss
    // diesen Zustand vor jeder Peltier-Freigabe explizit neu bewerten.
    TEST_ASSERT_TRUE(runtime->sensorSelection.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionPhase::RestartRevalidationPending),
        static_cast<int>(runtime->sensorSelectionRuntime.phase));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorPeltierPermission::Blocked),
        static_cast<int>(runtime->sensorSelectionRuntime.permission));
    TEST_ASSERT_FALSE(runtime->sensorSelectionRuntime
                          .fallbackWaitStartedAtMonotonicMillis.has_value());
    TEST_ASSERT_FALSE(
        runtime->sensorSelectionRuntime.lastAppliedMonotonicMillis.has_value());
}

void test_apply_recovery_time_correction_uses_persist_command_atomically() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());

    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 43U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    state.processState.state = ProcessState::Fermenting;
    state.processState.stateEnteredAtMillis = 100U;
    state.processState.targetReachStartedAtMillis = 0U;
    state.processState.targetReachWarningIssued = false;
    state.processState.qualificationValidSinceMillis.reset();
    state.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
        ProcessState::Fermenting, PriorBootPhaseElapsed{100U, 300U}};
    state.recoveryEpisodeRevision = 2U;

    ApplyRecoveryTimeCorrectionRequest request;
    request.envelope.id = 44U;
    request.envelope.source = CommandSource::LocalDisplay;
    request.envelope.monotonicMillis = 200U;
    request.envelope.expectedStateSequence =
        state.processState.transitionSequence;
    request.envelope.expectedRunRevision = state.runRevision;
    request.envelope.expectedRecoveryEpisodeRevision =
        state.recoveryEpisodeRevision;
    request.envelope.confirmed = true;
    request.secondsDelta = 50U;
    const auto decision = decideApplyRecoveryTimeCorrection(state, request);
    TEST_ASSERT_TRUE(decision.proposed());

    const auto persisted = coordinator.persistCommand(
        state, decision, RunCheckpointTime{200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(persisted.status));
    TEST_ASSERT_TRUE(state.nominalRecoveryAdjustment.has_value());
    TEST_ASSERT_EQUAL_UINT32(
        50U, state.nominalRecoveryAdjustment->cumulativeAppliedSeconds);

    store.restart();
    RunPersistenceCoordinator restored(store, device_platform::StorageEpoch(1U),
                                       RunCheckpointSchedule{});
    const auto loaded = restored.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(loaded.status));
    TEST_ASSERT_TRUE(loaded.snapshot.has_value());
    TEST_ASSERT_TRUE(loaded.snapshot->nominalRecoveryAdjustment.has_value());
    TEST_ASSERT_EQUAL_UINT32(
        50U,
        loaded.snapshot->nominalRecoveryAdjustment->cumulativeAppliedSeconds);
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

void test_load_is_single_use_and_does_not_reinitialize_live_state() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::NoPersistedRun),
        static_cast<int>(coordinator.loadAndInitialize().status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::AlreadyInitialized),
        static_cast<int>(coordinator.loadAndInitialize().status));
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 150U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
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
    state.processState.targetReachStartedAtMillis = 0U;
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
                     true,
                     std::nullopt};
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

// Proves idempotency after a real restart without #18: the persisted
// command-ID window survives a tombstone load (loadAndInitialize populates
// persistedIds_ before the NoActiveRun/ReadyEmpty return), so replaying the
// same start command ID against a freshly booted coordinator is rejected
// without any write, RAM apply or effect -- no recovery API required.
void test_already_persisted_command_id_is_rejected_after_restart_via_tombstone() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    const CommandId startId = 910U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, startId),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    StopRequest stop;
    stop.envelope = {911U,
                     CommandSource::LocalDisplay,
                     200U,
                     state.processState.transitionSequence,
                     state.runRevision,
                     std::nullopt,
                     std::nullopt,
                     true,
                     std::nullopt};
    stop.option = StopOption::AbortAndTurnOff;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, decideStop(state, stop),
                                RunCheckpointTime{200U, std::nullopt})
                .status));

    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::NoActiveRun),
        static_cast<int>(afterBoot.loadAndInitialize().status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::ReadyEmpty),
        static_cast<int>(afterBoot.state()));

    RunCommandState newStandby;
    newStandby.processState.state = ProcessState::Standby;
    const auto writesBeforeRetry = store.writeCount();
    const auto retried = afterBoot.persistCommand(
        newStandby, startDecision(newStandby, startId, 10U),
        RunCheckpointTime{10U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::AlreadyPersisted),
        static_cast<int>(retried.status));
    TEST_ASSERT_EQUAL_UINT32(0U, retried.effectCount);
    TEST_ASSERT_FALSE(newStandby.activeProgramRun.has_value());
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBeforeRetry),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::ReadyEmpty),
        static_cast<int>(afterBoot.state()));

    // The stop command's ID is likewise remembered on the same level.
    const auto retriedStop = afterBoot.persistCommand(
        newStandby, startDecision(newStandby, 911U, 11U),
        RunCheckpointTime{11U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::AlreadyPersisted),
        static_cast<int>(retriedStop.status));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBeforeRetry),
                           static_cast<unsigned>(store.writeCount()));
}

void test_orphan_checkpoint_revision_is_never_reused_after_restart() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 501U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    StopRequest stop;
    stop.envelope = {502U,
                     CommandSource::LocalDisplay,
                     200U,
                     state.processState.transitionSequence,
                     state.runRevision,
                     std::nullopt,
                     std::nullopt,
                     true,
                     std::nullopt};
    stop.option = StopOption::AbortAndTurnOff;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, decideStop(state, stop),
                                RunCheckpointTime{200U, std::nullopt})
                .status));

    // rc1 is the committed tombstone.  rc0 is no longer referenced; inject a
    // valid physical orphan with a deliberately higher envelope revision.
    const auto tombstoneBytes = store.read(slotKey("rc1"), 8240U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(tombstoneBytes.status));
    const auto tombstoneEnvelope =
        device_platform::decodeEnvelope(tombstoneBytes.value);
    TEST_ASSERT_TRUE(tombstoneEnvelope.envelope.has_value());
    auto orphanEnvelope = *tombstoneEnvelope.envelope;
    orphanEnvelope.versionValue = 100U;
    std::string orphanBytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeEncodeStatus::Success),
        static_cast<int>(device_platform::encodeEnvelope(orphanEnvelope,
                                                         orphanBytes, 8240U)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreWriteStatus::Success),
        static_cast<int>(store.write(slotKey("rc0"), orphanBytes)));

    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::NoActiveRun),
        static_cast<int>(afterBoot.loadAndInitialize().status));
    RunCommandState next;
    next.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            afterBoot
                .persistCommand(next, startDecision(next, 503U, 10U),
                                RunCheckpointTime{10U, std::nullopt})
                .status));
    const auto replacement = store.read(slotKey("rc0"), 8240U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(replacement.status));
    const auto replacementEnvelope =
        device_platform::decodeEnvelope(replacement.value);
    TEST_ASSERT_TRUE(replacementEnvelope.envelope.has_value());
    TEST_ASSERT_EQUAL_UINT64(101U, replacementEnvelope.envelope->versionValue);
}

void test_orphan_max_revision_is_sticky_and_blocks_new_writes() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    commitTombstone(store);
    const auto existing = store.read(slotKey("rc0"), 8240U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(existing.status));
    const auto decoded = device_platform::decodeEnvelope(existing.value);
    TEST_ASSERT_TRUE(decoded.envelope.has_value());
    auto maxEnvelope = *decoded.envelope;
    maxEnvelope.versionValue = std::numeric_limits<std::uint64_t>::max();
    std::string maxBytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeEncodeStatus::Success),
        static_cast<int>(
            device_platform::encodeEnvelope(maxEnvelope, maxBytes, 8240U)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreWriteStatus::Success),
        static_cast<int>(store.write(slotKey("rc0"), maxBytes)));
    store.restart();

    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::NoActiveRun),
        static_cast<int>(afterBoot.loadAndInitialize().status));
    RunCommandState next;
    next.processState.state = ProcessState::Standby;
    const auto result =
        afterBoot.persistCommand(next, startDecision(next, 603U, 10U),
                                 RunCheckpointTime{10U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CounterOverflow),
        static_cast<int>(result.status));
}

void test_unknown_orphan_high_watermark_blocks_mutation() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    commitTombstone(store);
    store.restart();
    store.injectReadFailure(slotKey("rc0"), true);
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::ReadFailed),
        static_cast<int>(afterBoot.loadAndInitialize().status));
    RunCommandState next;
    next.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Blocked),
        static_cast<int>(
            afterBoot
                .persistCommand(next, startDecision(next, 604U, 10U),
                                RunCheckpointTime{10U, std::nullopt})
                .status));
}

void test_empty_boot_always_disarms_injected_schedule() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunCheckpointSchedule schedule{};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunCheckpointScheduleStatus::Success),
        static_cast<int>(schedule.confirm(100U)));
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), std::move(schedule));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::NoPersistedRun),
        static_cast<int>(coordinator.loadAndInitialize().status));
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 605U, 10U),
                                RunCheckpointTime{10U, std::nullopt})
                .status));
}

void test_mutation_write_faults_at_each_cutpoint_are_classified() {
    using Fault = SequencedWriteStore::WriteFault;
    for (std::size_t writeNumber = 1U; writeNumber <= 3U; ++writeNumber) {
        SequencedWriteStore store;
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceLoadStatus::NoPersistedRun),
            static_cast<int>(coordinator.loadAndInitialize().status));
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        store.faultAt(writeNumber, Fault::FailBeforeBegin);
        const auto result = coordinator.persistCommand(
            state, startDecision(state, 700U + writeNumber),
            RunCheckpointTime{100U, std::nullopt});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::WriteFailed),
            static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_UINT32(0U, result.effectCount);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                              static_cast<int>(state.processState.state));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                writeNumber == 1U
                    ? RunPersistenceCoordinatorState::ReadyEmpty
                    : RunPersistenceCoordinatorState::BlockedIndeterminate),
            static_cast<int>(coordinator.state()));
    }
}

void test_unknown_outcome_at_each_mutation_write_is_resolved() {
    using Fault = SequencedWriteStore::WriteFault;
    for (std::size_t writeNumber = 1U; writeNumber <= 3U; ++writeNumber) {
        SequencedWriteStore store;
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        static_cast<void>(coordinator.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        store.faultAt(writeNumber, Fault::PowerCutAfterCommitBeforeReturn);
        const auto result = coordinator.persistCommand(
            state, startDecision(state, 710U + writeNumber),
            RunCheckpointTime{100U, std::nullopt});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(result.status));
        TEST_ASSERT_TRUE(state.activeProgramRun.has_value());
        TEST_ASSERT_EQUAL_UINT32(1U, result.effectCount);
    }
}

void test_unknown_outcome_with_exact_old_bytes_is_not_written() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 715U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    // The sixth write is the next Committed-Head write.  Its old value is the
    // already committed Prepared-Head bytes, so this is an Existing+old-bytes
    // readback rather than an absent-key NotFound case.
    store.unknownWithoutCommitAt(6U);
    const auto result =
        coordinator.persistCommand(state, stopDecision(state, 716U),
                                   RunCheckpointTime{200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceDurability::Changed),
                          static_cast<int>(result.durability));
    TEST_ASSERT_EQUAL_UINT32(0U, result.effectCount);
    TEST_ASSERT_TRUE(state.activeProgramRun.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(coordinator.state()));
}

void test_capacity_faults_at_each_mutation_cutpoint_are_classified() {
    using Fault = SequencedWriteStore::WriteFault;
    for (std::size_t writeNumber = 1U; writeNumber <= 3U; ++writeNumber) {
        SequencedWriteStore store;
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        static_cast<void>(coordinator.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        store.faultAt(writeNumber, Fault::CapacityExceeded);
        const auto result = coordinator.persistCommand(
            state, startDecision(state, 718U + writeNumber),
            RunCheckpointTime{100U, std::nullopt});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::CapacityExceeded),
            static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                              static_cast<int>(state.processState.state));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                writeNumber == 1U
                    ? RunPersistenceCoordinatorState::ReadyEmpty
                    : RunPersistenceCoordinatorState::BlockedIndeterminate),
            static_cast<int>(coordinator.state()));
    }
}

void test_unknown_outcome_not_found_distinguishes_absent_and_existing_head() {
    using Fault = SequencedWriteStore::WriteFault;
    const auto head = slotKey("rh0");

    SequencedWriteStore absent;
    RunPersistenceCoordinator emptyCoordinator(
        absent, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(emptyCoordinator.loadAndInitialize());
    absent.forceNotFound(head, true);
    absent.faultAt(1U, Fault::PowerCutAfterCommitBeforeReturn);
    RunCommandState emptyState;
    emptyState.processState.state = ProcessState::Standby;
    const auto absentResult = emptyCoordinator.persistCommand(
        emptyState, startDecision(emptyState, 720U),
        RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(absentResult.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceDurability::Unchanged),
                          static_cast<int>(absentResult.durability));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::ReadyEmpty),
        static_cast<int>(emptyCoordinator.state()));

    SequencedWriteStore existing;
    RunPersistenceCoordinator existingCoordinator(
        existing, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(existingCoordinator.loadAndInitialize());
    RunCommandState active;
    active.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            existingCoordinator
                .persistCommand(active, startDecision(active, 721U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    existing.forceNotFound(head, true);
    existing.faultAt(existing.writeCount() + 1U,
                     Fault::PowerCutAfterCommitBeforeReturn);
    const auto existingResult = existingCoordinator.persistCommand(
        active, stopDecision(active, 722U),
        RunCheckpointTime{200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::PersistenceIndeterminate),
        static_cast<int>(existingResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceDurability::MayHaveChanged),
        static_cast<int>(existingResult.durability));
    TEST_ASSERT_EQUAL_UINT32(0U, existingResult.effectCount);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(existingCoordinator.state()));
}

void test_unknown_outcome_unresolvable_readbacks_block_every_mutation_step() {
    using Fault = SequencedWriteStore::WriteFault;
    using ReadFault = SequencedWriteStore::ReadFault;
    for (const auto readFault : {ReadFault::ForeignBytes, ReadFault::ReadError,
                                 ReadFault::CapacityError}) {
        for (std::size_t writeNumber = 1U; writeNumber <= 3U; ++writeNumber) {
            SequencedWriteStore store;
            RunPersistenceCoordinator coordinator(
                store, device_platform::StorageEpoch(1U),
                RunCheckpointSchedule{});
            static_cast<void>(coordinator.loadAndInitialize());
            RunCommandState state;
            state.processState.state = ProcessState::Standby;
            store.faultAt(writeNumber, Fault::PowerCutAfterCommitBeforeReturn);
            store.readFaultAt(writeNumber, readFault);
            const auto result = coordinator.persistCommand(
                state, startDecision(state, 760U + writeNumber),
                RunCheckpointTime{100U, std::nullopt});
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(
                    RunPersistenceResultStatus::PersistenceIndeterminate),
                static_cast<int>(result.status));
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(
                    RunPersistenceCoordinatorState::BlockedIndeterminate),
                static_cast<int>(coordinator.state()));
            TEST_ASSERT_EQUAL_UINT32(0U, result.effectCount);
            TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                                  static_cast<int>(state.processState.state));
        }
    }
}

void test_unknown_outcome_absent_slot_not_found_is_not_indeterminate() {
    using Fault = SequencedWriteStore::WriteFault;
    using ReadFault = SequencedWriteStore::ReadFault;
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    store.faultAt(2U, Fault::PowerCutAfterCommitBeforeReturn);
    store.readFaultAt(2U, ReadFault::NotFound);
    const auto result =
        coordinator.persistCommand(state, startDecision(state, 764U),
                                   RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceDurability::Changed),
                          static_cast<int>(result.durability));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(coordinator.state()));
}

// Closes the remaining resolvable-outcome cells the review flagged as
// missing: exact old bytes at a Prepared-Head that already had a committed
// predecessor, and exact old bytes at a target checkpoint slot that a prior
// command already occupied (the target slot alternates 0 -> 1 -> 0, so a
// third command's target lands back on the first command's real content).
void test_unknown_outcome_old_bytes_at_prepared_head_and_occupied_target_slot() {
    // Prepared-Head: the second command's Prepared-Head write always reads
    // its `old` argument from the coordinator's in-memory currentHead_, so
    // it is real/Existing (the first command's committed head) here.
    {
        SequencedWriteStore store;
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        static_cast<void>(coordinator.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                coordinator
                    .persistCommand(state, startDecision(state, 870U),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));
        store.unknownWithoutCommitAt(4U);
        const auto result =
            coordinator.persistCommand(state, stopDecision(state, 871U),
                                       RunCheckpointTime{200U, std::nullopt});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::WriteFailed),
            static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceStep::PreparedHead),
            static_cast<int>(result.step));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceDurability::Unchanged),
            static_cast<int>(result.durability));
        TEST_ASSERT_EQUAL_UINT32(0U, result.effectCount);
        TEST_ASSERT_TRUE(state.activeProgramRun.has_value());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceCoordinatorState::Ready),
            static_cast<int>(coordinator.state()));
        TEST_ASSERT_EQUAL_UINT(4U, static_cast<unsigned>(store.writeCount()));
    }

    // Target checkpoint slot: a fresh read precedes every slot write, so a
    // third command's target (back at the first command's physical slot)
    // genuinely observes real old content. By this point the Prepared-Head
    // for this command is already durably staged, so any slot-step outcome
    // -- old bytes or not -- leaves the coordinator blocked pending reboot.
    {
        SequencedWriteStore store;
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        static_cast<void>(coordinator.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                coordinator
                    .persistCommand(state, startDecision(state, 872U),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                coordinator
                    .persistCommand(state, stopDecision(state, 873U),
                                    RunCheckpointTime{200U, std::nullopt})
                    .status));
        store.unknownWithoutCommitAt(8U);
        const auto result =
            coordinator.persistCommand(state, startDecision(state, 874U, 300U),
                                       RunCheckpointTime{300U, std::nullopt});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::WriteFailed),
            static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceStep::CheckpointSlot),
            static_cast<int>(result.step));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceDurability::Changed),
            static_cast<int>(result.durability));
        TEST_ASSERT_EQUAL_UINT32(0U, result.effectCount);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                RunPersistenceCoordinatorState::BlockedIndeterminate),
            static_cast<int>(coordinator.state()));
        TEST_ASSERT_EQUAL_UINT(8U, static_cast<unsigned>(store.writeCount()));
        // The third command's RAM apply never ran: the run stopped by the
        // second command stays stopped, no new run was installed.
        TEST_ASSERT_FALSE(state.activeProgramRun.has_value());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                              static_cast<int>(state.processState.state));
    }
}

// A NotFound readback at a target checkpoint slot that a prior command
// already occupied is not equivalent to the never-written (Absent) case --
// only the Absent+NotFound combination resolves cleanly. Existing+NotFound
// stays unresolvable.
void test_unknown_outcome_not_found_at_a_previously_occupied_target_slot() {
    using Fault = SequencedWriteStore::WriteFault;
    using ReadFault = SequencedWriteStore::ReadFault;
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 880U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, stopDecision(state, 881U),
                                RunCheckpointTime{200U, std::nullopt})
                .status));
    store.faultAt(8U, Fault::PowerCutAfterCommitBeforeReturn);
    store.readFaultAt(8U, ReadFault::NotFound);
    const auto result =
        coordinator.persistCommand(state, startDecision(state, 882U, 300U),
                                   RunCheckpointTime{300U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::PersistenceIndeterminate),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceStep::CheckpointSlot),
                          static_cast<int>(result.step));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceDurability::Changed),
                          static_cast<int>(result.durability));
    TEST_ASSERT_EQUAL_UINT32(0U, result.effectCount);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(coordinator.state()));
    TEST_ASSERT_EQUAL_UINT(8U, static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_FALSE(state.activeProgramRun.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                          static_cast<int>(state.processState.state));
}

// The periodic new-bytes cells the review flagged as missing: an ambiguous
// status that actually did commit resolves via exact-new-bytes readback to
// a successful CheckpointWritten, for both the periodic slot and the
// periodic Committed-Head.
void test_periodic_unknown_outcome_resolves_to_written_for_slot_and_head() {
    using Fault = SequencedWriteStore::WriteFault;
    {
        SequencedWriteStore store;
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        static_cast<void>(coordinator.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                coordinator
                    .persistCommand(state, startDecision(state, 890U),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));
        store.faultAt(4U, Fault::PowerCutAfterCommitBeforeReturn);
        const auto result = coordinator.checkpointPeriodic(
            state, RunCheckpointTime{300100U, std::nullopt});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
            static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceStep::CommittedHead),
            static_cast<int>(result.step));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceDurability::Changed),
            static_cast<int>(result.durability));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceCoordinatorState::Ready),
            static_cast<int>(coordinator.state()));
        // Pins the exact write numbering the fault targets: 3 command writes
        // plus periodic slot(4) and head(5). A status-only assertion could
        // not distinguish "the ambiguous slot write resolved via exact-new-
        // bytes readback" from "the fault never fired at all".
        TEST_ASSERT_EQUAL_UINT(5U, static_cast<unsigned>(store.writeCount()));
    }
    {
        SequencedWriteStore store;
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        static_cast<void>(coordinator.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                coordinator
                    .persistCommand(state, startDecision(state, 891U),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));
        store.faultAt(5U, Fault::PowerCutAfterCommitBeforeReturn);
        const auto result = coordinator.checkpointPeriodic(
            state, RunCheckpointTime{300100U, std::nullopt});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
            static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceStep::CommittedHead),
            static_cast<int>(result.step));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceDurability::Changed),
            static_cast<int>(result.durability));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceCoordinatorState::Ready),
            static_cast<int>(coordinator.state()));
        TEST_ASSERT_EQUAL_UINT(5U, static_cast<unsigned>(store.writeCount()));
    }
}

// The periodic old-bytes cells the review flagged as missing: a second
// periodic checkpoint's slot target cycles back to the slot the first
// command originally wrote, and its Committed-Head old value is the first
// periodic checkpoint's real committed head -- both genuinely Existing. An
// ambiguous status that never actually wrote resolves to that untouched old
// content, and (unlike the non-periodic Prepared/Committed transaction)
// resolves cleanly back to Ready rather than blocking.
void test_periodic_unknown_outcome_old_bytes_preserve_existing_slot_and_head() {
    {
        SequencedWriteStore store;
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        static_cast<void>(coordinator.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                coordinator
                    .persistCommand(state, startDecision(state, 900U),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
            static_cast<int>(
                coordinator
                    .checkpointPeriodic(
                        state, RunCheckpointTime{300100U, std::nullopt})
                    .status));
        store.unknownWithoutCommitAt(6U);
        const auto result = coordinator.checkpointPeriodic(
            state, RunCheckpointTime{600200U, std::nullopt});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::WriteFailed),
            static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceStep::CheckpointSlot),
            static_cast<int>(result.step));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceDurability::Unchanged),
            static_cast<int>(result.durability));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceCoordinatorState::Ready),
            static_cast<int>(coordinator.state()));
        TEST_ASSERT_EQUAL_UINT32(0U, result.effectCount);
        TEST_ASSERT_EQUAL_UINT32(0U, result.messageCount);
        TEST_ASSERT_TRUE(state.activeProgramRun.has_value());
        TEST_ASSERT_EQUAL_UINT(6U, static_cast<unsigned>(store.writeCount()));
    }
    {
        SequencedWriteStore store;
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        static_cast<void>(coordinator.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                coordinator
                    .persistCommand(state, startDecision(state, 901U),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
            static_cast<int>(
                coordinator
                    .checkpointPeriodic(
                        state, RunCheckpointTime{300100U, std::nullopt})
                    .status));
        store.unknownWithoutCommitAt(7U);
        const auto result = coordinator.checkpointPeriodic(
            state, RunCheckpointTime{600200U, std::nullopt});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::WriteFailed),
            static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceStep::CommittedHead),
            static_cast<int>(result.step));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceDurability::Changed),
            static_cast<int>(result.durability));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceCoordinatorState::Ready),
            static_cast<int>(coordinator.state()));
        TEST_ASSERT_EQUAL_UINT32(0U, result.effectCount);
        TEST_ASSERT_EQUAL_UINT32(0U, result.messageCount);
        TEST_ASSERT_TRUE(state.activeProgramRun.has_value());
        TEST_ASSERT_EQUAL_UINT(7U, static_cast<unsigned>(store.writeCount()));
    }
}

// Periodic target slot was never written (Absent): a fresh read precedes the
// write, so the ambiguous status resolves via the NotFound+Absent branch to
// a clean, fully recoverable rejection.
void test_periodic_target_slot_absent_not_found_resolves_cleanly() {
    using Fault = SequencedWriteStore::WriteFault;
    using ReadFault = SequencedWriteStore::ReadFault;
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 910U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    store.faultAt(4U, Fault::PowerCutAfterCommitBeforeReturn);
    store.readFaultAt(4U, ReadFault::NotFound);
    const auto result = coordinator.checkpointPeriodic(
        state, RunCheckpointTime{300100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceStep::CheckpointSlot),
                          static_cast<int>(result.step));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceDurability::Unchanged),
                          static_cast<int>(result.durability));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(coordinator.state()));
    TEST_ASSERT_EQUAL_UINT32(0U, result.effectCount);
    TEST_ASSERT_EQUAL_UINT32(0U, result.messageCount);
    TEST_ASSERT_TRUE(state.activeProgramRun.has_value());
    // Proves the head write never even started: WriteFailed at the slot
    // cutpoint is exactly one write, not two.
    TEST_ASSERT_EQUAL_UINT(4U, static_cast<unsigned>(store.writeCount()));
}

// Periodic Committed-Head old value is always Existing once currentHead_ is
// set (it is read from memory, never re-read live): an ambiguous status
// resolved by a NotFound readback is a genuine, unresolvable mismatch.
void test_periodic_committed_head_existing_not_found_is_indeterminate() {
    using Fault = SequencedWriteStore::WriteFault;
    using ReadFault = SequencedWriteStore::ReadFault;
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 911U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    store.faultAt(5U, Fault::PowerCutAfterCommitBeforeReturn);
    store.readFaultAt(5U, ReadFault::NotFound);
    const auto result = coordinator.checkpointPeriodic(
        state, RunCheckpointTime{300100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::PersistenceIndeterminate),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceStep::CommittedHead),
                          static_cast<int>(result.step));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceDurability::Changed),
                          static_cast<int>(result.durability));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(coordinator.state()));
    TEST_ASSERT_EQUAL_UINT32(0U, result.effectCount);
    TEST_ASSERT_EQUAL_UINT32(0U, result.messageCount);
    TEST_ASSERT_TRUE(state.activeProgramRun.has_value());
    TEST_ASSERT_EQUAL_UINT(5U, static_cast<unsigned>(store.writeCount()));
}

// Periodic target slot was already occupied by an earlier periodic
// checkpoint's counterpart (slot rotation 0 -> 1 -> 0): a fresh read
// precedes the write and genuinely observes real old content, so a
// NotFound readback after the ambiguous write is an unresolvable mismatch,
// not a clean rollback -- unlike the Absent case above.
void test_periodic_target_slot_existing_not_found_is_indeterminate() {
    using Fault = SequencedWriteStore::WriteFault;
    using ReadFault = SequencedWriteStore::ReadFault;
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 912U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            coordinator
                .checkpointPeriodic(state,
                                    RunCheckpointTime{300100U, std::nullopt})
                .status));
    store.faultAt(6U, Fault::PowerCutAfterCommitBeforeReturn);
    store.readFaultAt(6U, ReadFault::NotFound);
    const auto result = coordinator.checkpointPeriodic(
        state, RunCheckpointTime{600200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::PersistenceIndeterminate),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceStep::CheckpointSlot),
                          static_cast<int>(result.step));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceDurability::MayHaveChanged),
        static_cast<int>(result.durability));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(coordinator.state()));
    TEST_ASSERT_EQUAL_UINT32(0U, result.effectCount);
    TEST_ASSERT_EQUAL_UINT32(0U, result.messageCount);
    TEST_ASSERT_TRUE(state.activeProgramRun.has_value());
    TEST_ASSERT_EQUAL_UINT(6U, static_cast<unsigned>(store.writeCount()));
}

void test_load_fallback_orphan_and_schema_epoch_matrix() {
    SequencedWriteStore orphan;
    RunPersistenceCoordinator seed(orphan, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            seed.persistCommand(state, startDecision(state, 770U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    orphan.restart();
    orphan.forceNotFound(slotKey("rh0"), true);
    RunPersistenceCoordinator orphanBoot(
        orphan, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            RunPersistenceLoadStatus::NotReconstructibleOrphanedState),
        static_cast<int>(orphanBoot.loadAndInitialize().status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(orphanBoot.state()));

    SequencedWriteStore fallback;
    RunPersistenceCoordinator running(
        fallback, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(running.loadAndInitialize());
    RunCommandState active;
    active.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            running
                .persistCommand(active, startDecision(active, 771U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            running
                .checkpointPeriodic(active,
                                    RunCheckpointTime{300100U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            running
                .checkpointPeriodic(active,
                                    RunCheckpointTime{600200U, std::nullopt})
                .status));
    // The current reference is now rc0/revision 3; the fallback is
    // rc1/revision 2, so the fallback-side reconstruction cannot accidentally
    // keep its initial checkpoint revision of 1.
    fallback.backing().injectCorruption(slotKey("rc0"), "damaged-current");
    fallback.restart();
    RunPersistenceCoordinator recovered(
        fallback, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::FallbackRecovered),
        static_cast<int>(recovered.loadAndInitialize().status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            RunPersistenceCoordinatorState::FallbackRecoveryPending),
        static_cast<int>(recovered.state()));
    TEST_ASSERT_EQUAL_UINT64(
        4U,
        RunPersistenceCoordinatorTestAccess::nextCheckpointRevision(recovered));
    TEST_ASSERT_EQUAL_UINT(
        1U,
        static_cast<unsigned>(
            RunPersistenceCoordinatorTestAccess::persistedIdCount(recovered)));
    TEST_ASSERT_EQUAL_UINT64(
        771U, RunPersistenceCoordinatorTestAccess::persistedId(recovered, 0U));
    // The first externally writing FallbackRecoveryPending API is Commit 7;
    // its end-to-end write must assert that this reconstructed high-watermark
    // is used and that this persisted command ID is deduplicated.
    RunCommandState blockedState;
    blockedState.processState.state = ProcessState::Standby;
    const auto writesBeforeRecovery = fallback.writeCount();
    const auto blockedMutation = recovered.persistCommand(
        blockedState, startDecision(blockedState, 773U),
        RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::RecoveryPending),
        static_cast<int>(blockedMutation.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            RunPersistenceCoordinatorState::FallbackRecoveryPending),
        static_cast<int>(blockedMutation.coordinatorState));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBeforeRecovery),
                           static_cast<unsigned>(fallback.writeCount()));

    fallback.backing().injectCorruption(slotKey("rc1"), "damaged-fallback");
    fallback.restart();
    RunPersistenceCoordinator unrecoverable(
        fallback, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::NotReconstructible),
        static_cast<int>(unrecoverable.loadAndInitialize().status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(unrecoverable.state()));

    SequencedWriteStore foreign;
    RunPersistenceCoordinator foreignSeed(
        foreign, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(foreignSeed.loadAndInitialize());
    RunCommandState foreignState;
    foreignState.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            foreignSeed
                .persistCommand(foreignState, startDecision(foreignState, 772U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    const auto bytes = foreign.read(slotKey("rc0"), 8240U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(bytes.status));
    const auto decoded = device_platform::decodeEnvelope(bytes.value);
    TEST_ASSERT_TRUE(decoded.envelope.has_value());
    auto foreignEnvelope = *decoded.envelope;
    foreignEnvelope.storageEpoch = device_platform::StorageEpoch(99U);
    std::string foreignBytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeEncodeStatus::Success),
        static_cast<int>(device_platform::encodeEnvelope(foreignEnvelope,
                                                         foreignBytes, 8240U)));
    foreign.backing().injectCorruption(slotKey("rc0"), foreignBytes);
    foreign.restart();
    RunPersistenceCoordinator foreignBoot(
        foreign, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::ForeignEpoch),
        static_cast<int>(foreignBoot.loadAndInitialize().status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(foreignBoot.state()));

    auto unsupportedEnvelope = *decoded.envelope;
    unsupportedEnvelope.storageEpoch = device_platform::StorageEpoch(1U);
    unsupportedEnvelope.schemaVersion = 99U;
    std::string unsupportedBytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::EnvelopeEncodeStatus::Success),
        static_cast<int>(device_platform::encodeEnvelope(
            unsupportedEnvelope, unsupportedBytes, 8240U)));
    foreign.backing().injectCorruption(slotKey("rc0"), unsupportedBytes);
    foreign.restart();
    RunPersistenceCoordinator unsupportedBoot(
        foreign, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::UnsupportedSchema),
        static_cast<int>(unsupportedBoot.loadAndInitialize().status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(unsupportedBoot.state()));
}

void test_tombstone_fallback_does_not_enter_recovery_pending() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());

    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 990U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));

    StopRequest stop;
    stop.envelope = {991U,
                     CommandSource::LocalDisplay,
                     200U,
                     state.processState.transitionSequence,
                     state.runRevision,
                     std::nullopt,
                     std::nullopt,
                     true,
                     std::nullopt};
    stop.option = StopOption::AbortAndTurnOff;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, decideStop(state, stop),
                                RunCheckpointTime{200U, std::nullopt})
                .status));

    // Start another run so the valid active Current carries the valid
    // NoActiveRun tombstone as its fallback.
    RunCommandState nextRun;
    nextRun.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(nextRun, startDecision(nextRun, 992U, 300U),
                                RunCheckpointTime{300U, std::nullopt})
                .status));
    const auto current =
        RunPersistenceCoordinatorTestAccess::currentReference(coordinator);
    const auto fallback =
        RunPersistenceCoordinatorTestAccess::fallbackReference(coordinator);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunCheckpointVariant::ProgramRun),
                          static_cast<int>(current.variant));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunCheckpointVariant::NoActiveRun),
                          static_cast<int>(fallback.variant));

    store.backing().injectCorruption(
        current.slot == 0U ? slotKey("rc0") : slotKey("rc1"),
        "damaged-current");
    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = afterBoot.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::NotReconstructible),
        static_cast<int>(loaded.status));
    TEST_ASSERT_FALSE(loaded.snapshot.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(afterBoot.state()));
}

enum class CorePreCommitFailure {
    Codec,
    Write,
    Capacity,
    NotWritten,
};

void test_write_snapshot_core_rolls_back_loaded_active_run_before_commit() {
    using Fault = SequencedWriteStore::WriteFault;
    for (const auto failure :
         {CorePreCommitFailure::Codec, CorePreCommitFailure::Write,
          CorePreCommitFailure::Capacity, CorePreCommitFailure::NotWritten}) {
        SequencedWriteStore store;
        RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                       RunCheckpointSchedule{});
        static_cast<void>(seed.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                seed.persistCommand(state, startDecision(state, 993U),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));

        store.restart();
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        const auto loaded = coordinator.loadAndInitialize();
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceLoadStatus::Current),
            static_cast<int>(loaded.status));
        TEST_ASSERT_TRUE(loaded.snapshot.has_value());
        const auto before = restoreRunPersistenceSnapshot(*loaded.snapshot);
        TEST_ASSERT_TRUE(before.has_value());
        auto snapshot = *loaded.snapshot;
        if (failure == CorePreCommitFailure::Codec) {
            snapshot = RunPersistenceSnapshot{};
        } else if (failure == CorePreCommitFailure::Write) {
            store.faultAt(1U, Fault::FailBeforeBegin);
        } else if (failure == CorePreCommitFailure::Capacity) {
            store.faultAt(1U, Fault::CapacityExceeded);
        } else {
            store.unknownWithoutCommitAt(1U);
        }

        const auto result =
            RunPersistenceCoordinatorTestAccess::writeSnapshotCore(
                coordinator, snapshot, RunCheckpointTime{200U, std::nullopt},
                false, *before, RunPersistenceMutationKind::Recovery,
                std::nullopt, std::nullopt, RunPersistenceFallbackDirective{},
                RunPersistenceCoordinatorState::LoadedActiveRun);
        const auto expectedStatus =
            failure == CorePreCommitFailure::Codec
                ? RunPersistenceResultStatus::InvalidDecision
            : failure == CorePreCommitFailure::Capacity
                ? RunPersistenceResultStatus::CapacityExceeded
                : RunPersistenceResultStatus::WriteFailed;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(expectedStatus),
                              static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceCoordinatorState::LoadedActiveRun),
            static_cast<int>(coordinator.state()));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceCoordinatorState::LoadedActiveRun),
            static_cast<int>(result.coordinatorState));
        TEST_ASSERT_EQUAL_UINT(failure == CorePreCommitFailure::Codec ? 0U : 1U,
                               static_cast<unsigned>(store.writeCount()));
    }
}

void test_write_snapshot_core_rolls_back_fallback_recovery_before_commit() {
    using Fault = SequencedWriteStore::WriteFault;
    for (const auto failure :
         {CorePreCommitFailure::Codec, CorePreCommitFailure::Write,
          CorePreCommitFailure::Capacity, CorePreCommitFailure::NotWritten}) {
        SequencedWriteStore store;
        RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                       RunCheckpointSchedule{});
        static_cast<void>(seed.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                seed.persistCommand(state, startDecision(state, 994U),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
            static_cast<int>(
                seed.checkpointPeriodic(
                        state, RunCheckpointTime{300100U, std::nullopt})
                    .status));

        store.backing().injectCorruption(slotKey("rc1"), "damaged-current");
        store.restart();
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        const auto loaded = coordinator.loadAndInitialize();
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceLoadStatus::FallbackRecovered),
            static_cast<int>(loaded.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                RunPersistenceCoordinatorState::FallbackRecoveryPending),
            static_cast<int>(coordinator.state()));
        TEST_ASSERT_TRUE(loaded.snapshot.has_value());
        const auto before = restoreRunPersistenceSnapshot(*loaded.snapshot);
        TEST_ASSERT_TRUE(before.has_value());
        const auto current =
            RunPersistenceCoordinatorTestAccess::currentReference(coordinator);
        const auto fallback =
            RunPersistenceCoordinatorTestAccess::fallbackReference(coordinator);
        auto snapshot = *loaded.snapshot;
        if (failure == CorePreCommitFailure::Codec) {
            snapshot = RunPersistenceSnapshot{};
        } else if (failure == CorePreCommitFailure::Write) {
            store.faultAt(1U, Fault::FailBeforeBegin);
        } else if (failure == CorePreCommitFailure::Capacity) {
            store.faultAt(1U, Fault::CapacityExceeded);
        } else {
            store.unknownWithoutCommitAt(1U);
        }

        const auto result =
            RunPersistenceCoordinatorTestAccess::writeSnapshotCore(
                coordinator, snapshot, RunCheckpointTime{200U, std::nullopt},
                false, *before, RunPersistenceMutationKind::Recovery,
                std::nullopt, current.slot,
                RunPersistenceFallbackDirective{
                    RunPersistenceFallbackMode::SetExplicitReference, fallback},
                RunPersistenceCoordinatorState::FallbackRecoveryPending);
        const auto expectedStatus =
            failure == CorePreCommitFailure::Codec
                ? RunPersistenceResultStatus::InvalidDecision
            : failure == CorePreCommitFailure::Capacity
                ? RunPersistenceResultStatus::CapacityExceeded
                : RunPersistenceResultStatus::WriteFailed;
        TEST_ASSERT_EQUAL_INT(static_cast<int>(expectedStatus),
                              static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                RunPersistenceCoordinatorState::FallbackRecoveryPending),
            static_cast<int>(coordinator.state()));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                RunPersistenceCoordinatorState::FallbackRecoveryPending),
            static_cast<int>(result.coordinatorState));
        TEST_ASSERT_EQUAL_UINT(failure == CorePreCommitFailure::Codec ? 0U : 1U,
                               static_cast<unsigned>(store.writeCount()));
    }
}

void test_write_snapshot_core_indeterminate_always_blocks_loaded_and_fallback() {
    using Fault = SequencedWriteStore::WriteFault;
    using ReadFault = SequencedWriteStore::ReadFault;

    {
        SequencedWriteStore store;
        RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                       RunCheckpointSchedule{});
        static_cast<void>(seed.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                seed.persistCommand(state, startDecision(state, 995U),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));
        store.restart();
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        const auto loaded = coordinator.loadAndInitialize();
        TEST_ASSERT_TRUE(loaded.snapshot.has_value());
        const auto before = restoreRunPersistenceSnapshot(*loaded.snapshot);
        TEST_ASSERT_TRUE(before.has_value());
        store.faultAt(1U, Fault::PowerCutAfterCommitBeforeReturn);
        store.readFaultAt(1U, ReadFault::ReadError);
        const auto result =
            RunPersistenceCoordinatorTestAccess::writeSnapshotCore(
                coordinator, *loaded.snapshot,
                RunCheckpointTime{200U, std::nullopt}, false, *before,
                RunPersistenceMutationKind::Recovery, std::nullopt,
                std::nullopt, RunPersistenceFallbackDirective{},
                RunPersistenceCoordinatorState::LoadedActiveRun);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                RunPersistenceResultStatus::PersistenceIndeterminate),
            static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                RunPersistenceCoordinatorState::BlockedIndeterminate),
            static_cast<int>(coordinator.state()));
    }

    {
        SequencedWriteStore store;
        RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                       RunCheckpointSchedule{});
        static_cast<void>(seed.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                seed.persistCommand(state, startDecision(state, 996U),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
            static_cast<int>(
                seed.checkpointPeriodic(
                        state, RunCheckpointTime{300100U, std::nullopt})
                    .status));
        store.backing().injectCorruption(slotKey("rc1"), "damaged-current");
        store.restart();
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        const auto loaded = coordinator.loadAndInitialize();
        TEST_ASSERT_TRUE(loaded.snapshot.has_value());
        const auto before = restoreRunPersistenceSnapshot(*loaded.snapshot);
        TEST_ASSERT_TRUE(before.has_value());
        const auto current =
            RunPersistenceCoordinatorTestAccess::currentReference(coordinator);
        const auto fallback =
            RunPersistenceCoordinatorTestAccess::fallbackReference(coordinator);
        store.faultAt(1U, Fault::PowerCutAfterCommitBeforeReturn);
        store.readFaultAt(1U, ReadFault::ReadError);
        const auto result =
            RunPersistenceCoordinatorTestAccess::writeSnapshotCore(
                coordinator, *loaded.snapshot,
                RunCheckpointTime{200U, std::nullopt}, false, *before,
                RunPersistenceMutationKind::Recovery, std::nullopt,
                current.slot,
                RunPersistenceFallbackDirective{
                    RunPersistenceFallbackMode::SetExplicitReference, fallback},
                RunPersistenceCoordinatorState::FallbackRecoveryPending);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                RunPersistenceResultStatus::PersistenceIndeterminate),
            static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                RunPersistenceCoordinatorState::BlockedIndeterminate),
            static_cast<int>(coordinator.state()));
    }
}

void test_same_slot_overrides_fail_closed_before_any_write() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 980U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));

    const auto current =
        RunPersistenceCoordinatorTestAccess::currentReference(coordinator);
    const auto writesBefore = store.writeCount();
    const RunPersistenceSnapshot snapshot;
    const RunCommandState before;
    const auto reject = [&](bool periodic,
                            RunPersistenceMutationKind mutationKind,
                            std::optional<RunCheckpointReference> fallback) {
        const auto fallbackDirective =
            fallback.has_value()
                ? RunPersistenceFallbackDirective{RunPersistenceFallbackMode::
                                                      SetExplicitReference,
                                                  fallback}
                : RunPersistenceFallbackDirective{};
        return RunPersistenceCoordinatorTestAccess::writeSnapshotCore(
            coordinator, snapshot, RunCheckpointTime{200U, std::nullopt},
            periodic, before, mutationKind, std::nullopt, current.slot,
            fallbackDirective, RunPersistenceCoordinatorState::Ready);
    };

    const auto recoveryWithoutFallback =
        reject(false, RunPersistenceMutationKind::Recovery, std::nullopt);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::InvalidDecision),
        static_cast<int>(recoveryWithoutFallback.status));

    const auto recoveryWithSameSlotFallback =
        reject(false, RunPersistenceMutationKind::Recovery, current);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::InvalidDecision),
        static_cast<int>(recoveryWithSameSlotFallback.status));

    const auto periodicSameSlot =
        reject(true, RunPersistenceMutationKind::Recovery, std::nullopt);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::InvalidDecision),
        static_cast<int>(periodicSameSlot.status));

    const auto nonRecoverySameSlot =
        reject(false, RunPersistenceMutationKind::Command, std::nullopt);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::InvalidDecision),
        static_cast<int>(nonRecoverySameSlot.status));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBefore),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(coordinator.state()));
}

void test_fallback_directive_rejects_invalid_mode_reference_combinations() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 997U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));

    const auto current =
        RunPersistenceCoordinatorTestAccess::currentReference(coordinator);
    const RunPersistenceSnapshot snapshot;
    const auto writesBefore = store.writeCount();
    const auto reject = [&](RunPersistenceFallbackDirective directive) {
        return RunPersistenceCoordinatorTestAccess::writeSnapshotCore(
            coordinator, snapshot, RunCheckpointTime{200U, std::nullopt}, false,
            state, RunPersistenceMutationKind::Recovery, std::nullopt,
            1U - current.slot, directive,
            RunPersistenceCoordinatorState::Ready);
    };

    for (const auto directive :
         {RunPersistenceFallbackDirective{
              RunPersistenceFallbackMode::UseStandardFallback, current},
          RunPersistenceFallbackDirective{
              RunPersistenceFallbackMode::ClearFallback, current},
          RunPersistenceFallbackDirective{
              RunPersistenceFallbackMode::SetExplicitReference, std::nullopt},
          RunPersistenceFallbackDirective{
              RunPersistenceFallbackMode::SetExplicitReference, current},
          RunPersistenceFallbackDirective{
              static_cast<RunPersistenceFallbackMode>(99U), std::nullopt}}) {
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::InvalidDecision),
            static_cast<int>(reject(directive).status));
    }
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBefore),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(coordinator.state()));
}

// The head-vs-slot reference binding (slot, revision, length, CRC, variant)
// is only checked at load time (runCheckpointReferenceMatches). A
// structurally valid but mis-bound reference -- one that passes
// encodeRunPersistenceHead()'s own invariants yet no longer describes the
// physically stored record -- must be rejected as both Current and
// Fallback, not silently accepted. The codec-level encode-rejection tests
// only prove the encoder refuses inconsistent inputs; they never exercise
// this load-time binding check against a real store.
void test_load_rejects_a_structurally_valid_but_mismatched_current_reference() {
    // Case 1: `current` and `fallback` have their slot fields swapped. Both
    // physical slots hold real, valid, distinct checkpoints, and the
    // structural head contract (two distinct slots) still holds -- but each
    // reference's revision/length/CRC now describes the slot it no longer
    // points at. Neither the corrupted current nor the corrupted fallback
    // reference may be accepted.
    {
        SequencedWriteStore store;
        RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                       RunCheckpointSchedule{});
        static_cast<void>(seed.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                seed.persistCommand(state, startDecision(state, 950U),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));
        const auto tracking = decideProcessTransition(
            state.processState, &*state.processRunSnapshot,
            ProcessSignals{QualificationProgress::InBand, false, false},
            TransitionRequest{}, 100U);
        TEST_ASSERT_TRUE(tracking.proposed());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                seed.persistTransition(state, tracking,
                                       RunCheckpointTime{100U, std::nullopt})
                    .status));

        const auto headBytes = store.read(slotKey("rh0"), 256U);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(device_platform::StateStoreReadStatus::Success),
            static_cast<int>(headBytes.status));
        auto head = decodeRunPersistenceHead(headBytes.value,
                                             device_platform::StorageEpoch(1U));
        TEST_ASSERT_TRUE(head.has_value());
        TEST_ASSERT_TRUE(head->fallback.has_value());
        // Swap only the slot fields: `current` and `fallback` remain
        // structurally distinct (required by validCommittedHead), but each
        // now names the physical slot holding the OTHER reference's
        // content -- both revision/length/CRC pairs go stale at once.
        std::swap(head->current.slot, head->fallback->slot);
        const auto corruptedHead =
            encodeRunPersistenceHead(*head, device_platform::StorageEpoch(1U));
        TEST_ASSERT_TRUE(corruptedHead.has_value());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(device_platform::StateStoreWriteStatus::Success),
            static_cast<int>(
                store.backing().write(slotKey("rh0"), *corruptedHead)));

        store.restart();
        RunPersistenceCoordinator afterBoot(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        const auto loaded = afterBoot.loadAndInitialize();
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceLoadStatus::NotReconstructible),
            static_cast<int>(loaded.status));
        TEST_ASSERT_FALSE(loaded.snapshot.has_value());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                RunPersistenceCoordinatorState::BlockedIndeterminate),
            static_cast<int>(afterBoot.state()));
    }

    // Case 2: the reference's variant lies about the physically stored
    // record's variant (slot/revision/length/CRC otherwise correct).
    {
        SequencedWriteStore store;
        RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                       RunCheckpointSchedule{});
        static_cast<void>(seed.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                seed.persistCommand(state, manualStartDecision(state, 951U),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));

        const auto headBytes = store.read(slotKey("rh0"), 256U);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(device_platform::StateStoreReadStatus::Success),
            static_cast<int>(headBytes.status));
        auto head = decodeRunPersistenceHead(headBytes.value,
                                             device_platform::StorageEpoch(1U));
        TEST_ASSERT_TRUE(head.has_value());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(RunCheckpointVariant::ManualRun),
                              static_cast<int>(head->current.variant));
        head->current.variant = RunCheckpointVariant::ProgramRun;
        const auto corruptedHead =
            encodeRunPersistenceHead(*head, device_platform::StorageEpoch(1U));
        TEST_ASSERT_TRUE(corruptedHead.has_value());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(device_platform::StateStoreWriteStatus::Success),
            static_cast<int>(
                store.backing().write(slotKey("rh0"), *corruptedHead)));

        store.restart();
        RunPersistenceCoordinator afterBoot(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        const auto loaded = afterBoot.loadAndInitialize();
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceLoadStatus::NotReconstructible),
            static_cast<int>(loaded.status));
        TEST_ASSERT_FALSE(loaded.snapshot.has_value());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                RunPersistenceCoordinatorState::BlockedIndeterminate),
            static_cast<int>(afterBoot.state()));
    }
}

void test_loaded_active_run_blocks_all_mutations_after_restart() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 780U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = afterBoot.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(loaded.status));
    const auto writes = store.writeCount();
    const auto commandResult =
        afterBoot.persistCommand(state, startDecision(state, 780U),
                                 RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::RecoveryPending),
        static_cast<int>(commandResult.status));
    TEST_ASSERT_EQUAL_UINT32(0U, commandResult.effectCount);
    TEST_ASSERT_EQUAL_UINT32(0U, commandResult.messageCount);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::LoadedActiveRun),
        static_cast<int>(commandResult.coordinatorState));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::LoadedActiveRun),
        static_cast<int>(afterBoot.state()));

    TransitionDecision transition;
    const auto transitionResult = afterBoot.persistTransition(
        state, transition, RunCheckpointTime{101U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::RecoveryPending),
        static_cast<int>(transitionResult.status));
    TEST_ASSERT_EQUAL_UINT32(0U, transitionResult.effectCount);
    TEST_ASSERT_EQUAL_UINT32(0U, transitionResult.messageCount);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::LoadedActiveRun),
        static_cast<int>(transitionResult.coordinatorState));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::LoadedActiveRun),
        static_cast<int>(afterBoot.state()));

    const auto periodicResult = afterBoot.checkpointPeriodic(
        state, RunCheckpointTime{400000U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::RecoveryPending),
        static_cast<int>(periodicResult.status));
    TEST_ASSERT_EQUAL_UINT32(0U, periodicResult.effectCount);
    TEST_ASSERT_EQUAL_UINT32(0U, periodicResult.messageCount);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::LoadedActiveRun),
        static_cast<int>(periodicResult.coordinatorState));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::LoadedActiveRun),
        static_cast<int>(afterBoot.state()));

    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writes),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_TRUE(loaded.snapshot.has_value());
    TEST_ASSERT_EQUAL_UINT32(1U, loaded.snapshot->persistedRunCommandCount);
    TEST_ASSERT_EQUAL_UINT64(780U, loaded.snapshot->persistedRunCommandIds[0]);
}

// Shared prefix for both product-path scenarios below: a preheat-qualified
// program run driven by real decisions to a durably confirmed
// WaitingForProduct. Each scenario starts its own store/coordinator and
// calls this once; neither scenario resets the other's already-advanced RAM
// state.
RunCommandState reachDurablyWaitingForProduct(
    RunPersistenceCoordinator& coordinator, CommandId startId,
    RunSensorMode sensorMode = RunSensorMode::Product) {
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state,
                                startDecision(state, startId, 100U,
                                              preheatProgram(), sensorMode),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    const auto tracking = decideProcessTransition(
        state.processState, &*state.processRunSnapshot,
        ProcessSignals{QualificationProgress::InBand, false, false},
        TransitionRequest{}, 100U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::QualificationTrackingStarted),
        static_cast<int>(tracking.reason));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, tracking,
                                   RunCheckpointTime{100U, std::nullopt})
                .status));
    const auto waiting = decideProcessTransition(
        state.processState, &*state.processRunSnapshot,
        ProcessSignals{QualificationProgress::Complete, false, false},
        TransitionRequest{}, 600100U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TransitionReason::PreheatQualified),
                          static_cast<int>(waiting.reason));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, waiting,
                                   RunCheckpointTime{600100U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::WaitingForProduct),
                          static_cast<int>(state.processState.state));
    return state;
}

TargetQualificationInput preheatQualificationInput(const RunCommandState& state,
                                                   std::uint64_t timestamp) {
    TargetQualificationInput input;
    input.phase = QualificationPhase::Preheating;
    input.sampleTimestampMonotonicMillis = timestamp;
    input.runRevision = state.runRevision;
    input.processTransitionSequence = state.processState.transitionSequence;
    input.targetCelsius = 38.0;
    // Must match the canonical run contract (preheatProgram():
    // targetQualification.bandCelsius/durationMinutes) so
    // evaluateAndApplyTargetQualification()'s canonical-context check
    // (FR4) accepts it; effectiveGraceMillis/maximumAcceptedSampleGapMillis
    // stay free Commissioning-style test values (not canonically bound).
    input.bandCelsius = 0.5;
    input.qualificationDurationMillis = 600'000U;
    input.effectiveGraceMillis = 500U;
    input.maximumAcceptedSampleGapMillis = 700'000U;
    input.controlSensorRole = ControlSensorRole::Air;
    input.air.quality = device_platform::SensorQuality::Valid;
    // Per #20/FR2, only filteredCelsius is a normal control value.
    input.air.filteredCelsius = 38.0;
    input.air.rawCelsius = 38.0;
    input.product = input.air;
    return input;
}

RunAdjustmentCommandRequest targetAdjustmentRequest(
    const RunCommandState& state, CommandId id, std::uint64_t timestamp,
    double target) {
    RunAdjustmentCommandRequest request;
    request.envelope = {id,
                        CommandSource::LocalDisplay,
                        timestamp,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true,
                        std::nullopt};
    request.targetTemperatureCelsius = target;
    request.safetyAllowsChange = true;
    return request;
}

void test_persist_command_hands_off_target_context_only_after_apply() {
    for (const auto phase :
         {ProcessState::ReachingTarget, ProcessState::Fermenting}) {
        SequencedWriteStore store;
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        static_cast<void>(coordinator.loadAndInitialize());
        TargetQualificationEvaluator evaluator;
        TemperatureController controller(
            {}, {IntegratorTransitionAction::Reset,
                 IntegratorTransitionAction::Reset,
                 IntegratorTransitionAction::Reset, 0.2});
        TemperatureControlApplicationOrchestrator application(
            coordinator, controller, evaluator);
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                application
                    .persistCommand(state, startDecision(state, 1210U),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));
        state.processState.state = phase;
        state.processState.stateEnteredAtMillis = 100U;
        if (phase == ProcessState::Fermenting) {
            state.processState.targetReachStartedAtMillis = 0U;
            state.processState.targetReachWarningIssued = false;
        }
        const auto decision = decideRunAdjustment(
            state, targetAdjustmentRequest(state, 1211U, 200U, 37.5));
        TEST_ASSERT_TRUE(decision.proposed());

        const auto oldTarget =
            state.activeProgramRun->effectiveValues().targetTemperatureCelsius;
        store.faultAt(store.writeCount() + 1U,
                      SequencedWriteStore::WriteFault::FailBeforeBegin);
        const auto failed = application.persistCommand(
            state, decision, RunCheckpointTime{200U, std::nullopt});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::WriteFailed),
            static_cast<int>(failed.status));
        TEST_ASSERT_FALSE(failed.committedControlContextTransition.has_value());
        TEST_ASSERT_DOUBLE_WITHIN(
            0.0001, oldTarget,
            state.activeProgramRun->effectiveValues().targetTemperatureCelsius);
        TEST_ASSERT_FALSE(
            controller.state().pendingContextTransition.has_value());

        store.clearFaults();
        auto committed = application.persistCommand(
            state, decision, RunCheckpointTime{200U, std::nullopt});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(committed.status));
        TEST_ASSERT_FALSE(
            committed.committedControlContextTransition.has_value());
        TEST_ASSERT_TRUE(
            controller.state().pendingContextTransition.has_value());
        TEST_ASSERT_TRUE(
            *controller.state().pendingContextTransition ==
            CommittedControlContextTransition::TargetContextChange);
    }
}

void test_qualification_orchestrator_discards_failed_candidate_and_retries() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(
                    state, startDecision(state, 1200U, 100U, preheatProgram()),
                    RunCheckpointTime{100U, std::nullopt})
                .status));
    TargetQualificationEvaluator evaluator;
    TemperatureController temperatureController({}, {});
    TemperatureControlApplicationOrchestrator application(
        coordinator, temperatureController, evaluator);

    const auto writesBeforeMismatchedInput = store.writeCount();
    auto mismatchedInput = preheatQualificationInput(state, 100U);
    mismatchedInput.targetCelsius = 37.0;
    const auto mismatched = evaluateAndApplyTargetQualification(
        application, state, mismatchedInput,
        RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_TRUE(mismatched.status ==
                     TargetQualificationOrchestrationStatus::InvalidDecision);
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBeforeMismatchedInput),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Preheating),
                          static_cast<int>(state.processState.state));

    auto mismatchedRole = preheatQualificationInput(state, 100U);
    mismatchedRole.controlSensorRole = ControlSensorRole::Product;
    const auto roleMismatch = evaluateAndApplyTargetQualification(
        application, state, mismatchedRole,
        RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_TRUE(roleMismatch.status ==
                     TargetQualificationOrchestrationStatus::InvalidDecision);
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBeforeMismatchedInput),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Preheating),
                          static_cast<int>(state.processState.state));

    const auto tracking = evaluateAndApplyTargetQualification(
        application, state, preheatQualificationInput(state, 100U),
        RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_TRUE(tracking.status ==
                     TargetQualificationOrchestrationStatus::AppliedPersisted);
    TEST_ASSERT_TRUE(tracking.qualification.lifecycle ==
                     TargetQualificationDecisionLifecycle::Applied);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Preheating),
                          static_cast<int>(state.processState.state));
    const auto beforeFailure = evaluator.state();

    store.faultAt(store.writeCount() + 1U,
                  SequencedWriteStore::WriteFault::FailBeforeBegin);
    // FR4: the canonical duration is the real 10-minute program contract
    // (600'000ms), so completion needs a matching elapsed gap, not the
    // previous arbitrary short one.
    const auto failed = evaluateAndApplyTargetQualification(
        application, state, preheatQualificationInput(state, 600'100U),
        RunCheckpointTime{600'100U, std::nullopt});
    TEST_ASSERT_TRUE(failed.status ==
                     TargetQualificationOrchestrationStatus::PersistenceFailed);
    TEST_ASSERT_TRUE(failed.qualification.lifecycle ==
                     TargetQualificationDecisionLifecycle::Discarded);
    TEST_ASSERT_TRUE(evaluator.state().lastUsableTimestampMillis ==
                     beforeFailure.lastUsableTimestampMillis);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Preheating),
                          static_cast<int>(state.processState.state));

    store.clearFaults();
    const auto retry = evaluateAndApplyTargetQualification(
        application, state, preheatQualificationInput(state, 600'100U),
        RunCheckpointTime{600'100U, std::nullopt});
    TEST_ASSERT_TRUE(retry.status ==
                     TargetQualificationOrchestrationStatus::AppliedPersisted);
    TEST_ASSERT_TRUE(retry.qualification.progress ==
                     QualificationProgress::Complete);
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::WaitingForProduct),
                          static_cast<int>(state.processState.state));

    const auto inserted = decideProcessTransition(
        state.processState, &*state.processRunSnapshot, ProcessSignals{},
        TransitionRequest{ProcessEvent::ProductInsertedConfirmed, std::nullopt},
        600'200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, inserted,
                                   RunCheckpointTime{600'200U, std::nullopt})
                .status));
    auto targetInput = preheatQualificationInput(state, 600'300U);
    targetInput.phase = QualificationPhase::Target;
    targetInput.controlSensorRole = ControlSensorRole::Product;
    targetInput.product.filteredCelsius = 38.0;
    targetInput.product.rawCelsius = 38.0;
    const auto targetTracking = evaluateAndApplyTargetQualification(
        application, state, targetInput,
        RunCheckpointTime{600'300U, std::nullopt});
    TEST_ASSERT_TRUE(targetTracking.status ==
                     TargetQualificationOrchestrationStatus::AppliedPersisted);
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);

    targetInput.processTransitionSequence =
        state.processState.transitionSequence;
    targetInput.sampleTimestampMonotonicMillis = 1'200'300U;
    const auto targetComplete = evaluateAndApplyTargetQualification(
        application, state, targetInput,
        RunCheckpointTime{1'200'300U, std::nullopt});
    TEST_ASSERT_TRUE(targetComplete.qualification.progress ==
                     QualificationProgress::Complete);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fermenting),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);
}

device_platform::SensorQualitySnapshot bridgeSensorSample(double celsius) {
    device_platform::SensorQualitySnapshot snapshot;
    snapshot.quality = device_platform::SensorQuality::Valid;
    snapshot.filteredCelsius = celsius;
    return snapshot;
}

TargetQualificationInput manualQualificationInput(const RunCommandState& state,
                                                  std::uint64_t timestamp) {
    const auto& values = state.activeManualRun->values;
    TargetQualificationInput input;
    input.phase = QualificationPhase::Target;
    input.sampleTimestampMonotonicMillis = timestamp;
    input.runRevision = state.runRevision;
    input.processTransitionSequence = state.processState.transitionSequence;
    input.targetCelsius = values.targetTemperatureCelsius;
    input.bandCelsius = values.qualificationBandCelsius;
    input.qualificationDurationMillis =
        static_cast<std::uint64_t>(values.qualificationDurationMinutes) *
        60'000U;
    input.effectiveGraceMillis = 500U;
    input.maximumAcceptedSampleGapMillis = 700'000U;
    input.controlSensorRole = values.sensorMode == RunSensorMode::Product
                                  ? ControlSensorRole::Product
                                  : ControlSensorRole::Air;
    input.air = bridgeSensorSample(values.targetTemperatureCelsius);
    input.product = input.air;
    return input;
}

TargetQualificationInput programTargetQualificationInput(
    const RunCommandState& state, std::uint64_t timestamp) {
    const auto values = state.activeProgramRun->effectiveValues();
    const auto& program =
        state.activeProgramRun->snapshot().sourceProgram.program;
    TargetQualificationInput input;
    input.phase = QualificationPhase::Target;
    input.sampleTimestampMonotonicMillis = timestamp;
    input.runRevision = state.runRevision;
    input.processTransitionSequence = state.processState.transitionSequence;
    input.targetCelsius = values.targetTemperatureCelsius;
    input.bandCelsius = *program.targetQualification.bandCelsius;
    input.qualificationDurationMillis =
        static_cast<std::uint64_t>(
            *program.targetQualification.durationMinutes) *
        60'000U;
    input.effectiveGraceMillis = 500U;
    input.maximumAcceptedSampleGapMillis = 700'000U;
    input.controlSensorRole =
        *state.activeRunSensorMode == RunSensorMode::Product
            ? ControlSensorRole::Product
            : ControlSensorRole::Air;
    input.air = bridgeSensorSample(20.0);
    input.product = bridgeSensorSample(input.targetCelsius);
    return input;
}

// FR1: the only canonical PI-evaluation boundary. It resolves target/role
// from the live RunCommandState via resolveEffectiveControlContext() - a
// caller cannot supply a freely-chosen target/role/context, closing the gap
// where TemperatureController::evaluate() alone could still be fed a stale
// or fabricated source-program target.
void test_evaluate_temperature_control_uses_canonical_context_per_phase() {
    ActuatorHandoffFixture fixture;
    auto& application = fixture.application;

    // Programme run, ReachingTarget: canonical target 38.0, role Product.
    auto state = readyActiveRunWithSensorSelection(fixture.coordinator, 900U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.processState.state));
    TemperatureControlEvaluationEvidence evidence;
    evidence.sampleTimestampMonotonicMillis = 100U;
    evidence.air = bridgeSensorSample(20.0);
    evidence.product = bridgeSensorSample(20.0);
    const auto reaching =
        application.evaluateTemperatureControl(state, evidence);
    TEST_ASSERT_TRUE(reaching.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(reaching.direction == AbstractControlDirection::Heating);
    TEST_ASSERT_TRUE(reaching.controlRequest.has_value());
    TEST_ASSERT_TRUE(reaching.controlRequest->context.controlSensorRole ==
                     ControlSensorRole::Product);
    TEST_ASSERT_EQUAL_UINT32(state.runRevision,
                             reaching.controlRequest->context.runRevision);
    TEST_ASSERT_EQUAL_UINT32(
        state.processState.transitionSequence,
        reaching.controlRequest->context.processTransitionSequence);
}

void test_evaluate_temperature_control_target_changed_uses_new_value_only() {
    // FR1/#20-consistent: after a live TargetChanged commit, only the new
    // effective target may drive the PI evaluation - no leftover old
    // source-program target.
    ActuatorHandoffFixture fixture;
    auto& application = fixture.application;
    auto state = readyActiveRunWithSensorSelection(fixture.coordinator, 901U);

    TemperatureControlEvaluationEvidence evidence;
    evidence.sampleTimestampMonotonicMillis = 100U;
    evidence.air = bridgeSensorSample(37.9);
    evidence.product = bridgeSensorSample(37.9);
    const auto beforeChange =
        application.evaluateTemperatureControl(state, evidence);
    TEST_ASSERT_TRUE(beforeChange.status == TemperatureControlStatus::Off);
    // Consume the outstanding evaluation so the second public
    // evaluateTemperatureControl() call below is not fail-closed by the
    // single-outstanding-evaluation guard (Abschnitt 6.1).
    static_cast<void>(application.tickActuatorPlan(
        state, 100U,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed}));

    const auto decision = decideRunAdjustment(
        state, targetAdjustmentRequest(state, 902U, 200U, 20.0));
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            application
                .persistCommand(state, decision,
                                RunCheckpointTime{200U, std::nullopt})
                .status));

    evidence.sampleTimestampMonotonicMillis = 300U;
    const auto afterChange =
        application.evaluateTemperatureControl(state, evidence);
    TEST_ASSERT_TRUE(afterChange.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(afterChange.direction ==
                     AbstractControlDirection::Cooling);
}

void test_evaluate_temperature_control_cooling_uses_completion_target_only() {
    ActuatorHandoffFixture fixture;
    auto& application = fixture.application;
    auto state = persistedCoolHoldingRun(fixture.coordinator, 903U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::CoolHolding),
                          static_cast<int>(state.processState.state));

    TemperatureControlEvaluationEvidence evidence;
    evidence.sampleTimestampMonotonicMillis = 700'000U;
    // Would be Heating against the 38.0 fermentation target, but Cooling
    // must exclusively use the completion target (20.0) - Idle at 25.0.
    evidence.air = bridgeSensorSample(25.0);
    evidence.product = bridgeSensorSample(25.0);
    const auto result = application.evaluateTemperatureControl(state, evidence);
    TEST_ASSERT_TRUE(result.direction == AbstractControlDirection::Cooling ||
                     result.status == TemperatureControlStatus::Off);
    TEST_ASSERT_TRUE(result.direction != AbstractControlDirection::Heating);
}

void test_evaluate_temperature_control_manual_run_uses_manual_target() {
    ActuatorHandoffFixture fixture;
    auto& application = fixture.application;
    auto state =
        readyActiveManualRunWithSensorSelection(fixture.coordinator, 904U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.processState.state));

    TemperatureControlEvaluationEvidence evidence;
    evidence.sampleTimestampMonotonicMillis = 100U;
    // ManualRun target is 12.0 (manualStartDecision()); 20.0 must read as
    // Cooling against it, not as Heating against a nonexistent fermentation
    // source.
    evidence.air = bridgeSensorSample(20.0);
    const auto result = application.evaluateTemperatureControl(state, evidence);
    TEST_ASSERT_TRUE(result.status == TemperatureControlStatus::Demand);
    TEST_ASSERT_TRUE(result.direction == AbstractControlDirection::Cooling);
}

void test_evaluate_temperature_control_fails_closed_outside_temperature_control() {
    ActuatorHandoffFixture fixture;
    auto& application = fixture.application;
    RunCommandState standby;
    standby.processState.state = ProcessState::Standby;

    TemperatureControlEvaluationEvidence evidence;
    evidence.sampleTimestampMonotonicMillis = 100U;
    evidence.air = bridgeSensorSample(20.0);
    const auto notControlled =
        application.evaluateTemperatureControl(standby, evidence);
    TEST_ASSERT_TRUE(notControlled.status ==
                     TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_FALSE(notControlled.controlRequest.has_value());

    // Run-/Snapshot-Widerspruch: a temperature-controlled phase without a
    // matching run/snapshot is structurally inconsistent, not merely
    // "not yet configured".
    RunCommandState contradictory;
    contradictory.processState.state = ProcessState::ReachingTarget;
    const auto contradictoryResult =
        application.evaluateTemperatureControl(contradictory, evidence);
    TEST_ASSERT_TRUE(contradictoryResult.status ==
                     TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_FALSE(contradictoryResult.controlRequest.has_value());
}

void test_evaluate_temperature_control_invalid_context_resets_runtime() {
    ActuatorHandoffFixture fixture;
    auto& application = fixture.application;
    auto& controller = fixture.controller;
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            application
                .persistCommand(state,
                                startDecision(state, 905U, 100U, std::nullopt,
                                              RunSensorMode::Air),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.processState.state));

    TemperatureControlEvaluationEvidence firstEvidence;
    firstEvidence.sampleTimestampMonotonicMillis = 100U;
    firstEvidence.air = bridgeSensorSample(35.0);
    firstEvidence.product = bridgeSensorSample(35.0);
    const auto first =
        application.evaluateTemperatureControl(state, firstEvidence);
    TEST_ASSERT_TRUE(first.controlRequest.has_value());
    // Consume the outstanding evaluation so the next public
    // evaluateTemperatureControl() call below is not fail-closed by the
    // single-outstanding-evaluation guard (Abschnitt 6.1); this test targets
    // context-validity resets, not the planner's own physical output.
    static_cast<void>(application.tickActuatorPlan(
        state, 100U,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed}));

    TemperatureControlEvaluationEvidence secondEvidence = firstEvidence;
    secondEvidence.sampleTimestampMonotonicMillis = 1'100U;
    const auto context = resolveEffectiveControlContext(state);
    TemperatureControlInput secondInput;
    secondInput.sampleTimestampMonotonicMillis =
        secondEvidence.sampleTimestampMonotonicMillis;
    secondInput.targetCelsius = context.target.targetCelsius;
    secondInput.controlSensorRole = context.controlSensorRole;
    secondInput.air = secondEvidence.air;
    secondInput.product = secondEvidence.product;
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    secondInput.processTransitionSequence =
        context.requestContext.processTransitionSequence;
    secondInput.runRevision = context.requestContext.runRevision;
    const auto second = controller.evaluate(secondInput);
    TEST_ASSERT_TRUE(second.integralContributionQuote > 0.0);
    TEST_ASSERT_TRUE(controller.state().lastActiveDirection.has_value());
    TEST_ASSERT_TRUE(controller.state().feedbackWindow.has_value());
    TEST_ASSERT_TRUE(controller.markCommittedControlContextTransitionPending(
        CommittedControlContextTransition::TargetContextChange));

    // Keep the phase temperature-controlled while making the otherwise
    // valid snapshot stale relative to the active program run.
    state.processRunSnapshot->qualificationDurationMinutes = 5U;
    TemperatureControlEvaluationEvidence invalidEvidence = firstEvidence;
    invalidEvidence.sampleTimestampMonotonicMillis = 2'100U;
    const auto invalid =
        application.evaluateTemperatureControl(state, invalidEvidence);
    TEST_ASSERT_TRUE(invalid.status == TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_FALSE(invalid.controlRequest.has_value());
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              controller.state().integralContributionQuote);
    TEST_ASSERT_FALSE(controller.state().lastActiveDirection.has_value());
    TEST_ASSERT_FALSE(controller.state().feedbackWindow.has_value());
    TEST_ASSERT_FALSE(controller.state().pendingContextTransition.has_value());
    TEST_ASSERT_FALSE(
        controller.state().lastSampleTimestampMonotonicMillis.has_value());
    // Consume this outstanding (fail-closed) evaluation too, for the same
    // reason as above.
    static_cast<void>(application.tickActuatorPlan(
        state, 2'100U,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed}));

    state.processRunSnapshot = makeProcessRunSnapshot(*state.activeProgramRun);
    TEST_ASSERT_TRUE(state.processRunSnapshot.has_value());
    TemperatureControlEvaluationEvidence restoredEvidence = firstEvidence;
    restoredEvidence.sampleTimestampMonotonicMillis = 3'100U;
    const auto restored =
        application.evaluateTemperatureControl(state, restoredEvidence);
    TEST_ASSERT_TRUE(restored.controlRequest.has_value());
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0, restored.integralContributionQuote);
    TEST_ASSERT_EQUAL_UINT64(first.controlRequest->identity.sequence + 2U,
                             restored.controlRequest->identity.sequence);
}

void test_qualification_orchestrator_rejects_band_and_duration_mismatch() {
    SequencedWriteStore programStore;
    RunPersistenceCoordinator programCoordinator(
        programStore, device_platform::StorageEpoch(1U),
        RunCheckpointSchedule{});
    static_cast<void>(programCoordinator.loadAndInitialize());
    TargetQualificationEvaluator programEvaluator;
    TemperatureController programController({}, {});
    TemperatureControlApplicationOrchestrator programApplication(
        programCoordinator, programController, programEvaluator);
    RunCommandState programState;
    programState.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            programApplication
                .persistCommand(
                    programState,
                    startDecision(programState, 906U, 100U, preheatProgram()),
                    RunCheckpointTime{100U, std::nullopt})
                .status));

    const auto writesBeforeProgram = programStore.writeCount();
    auto wrongBand = preheatQualificationInput(programState, 100U);
    wrongBand.bandCelsius = 0.6;
    const auto rejectedBand = evaluateAndApplyTargetQualification(
        programApplication, programState, wrongBand,
        RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_TRUE(rejectedBand.status ==
                     TargetQualificationOrchestrationStatus::InvalidDecision);
    TEST_ASSERT_TRUE(rejectedBand.qualification.lifecycle ==
                     TargetQualificationDecisionLifecycle::Discarded);
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBeforeProgram),
                           static_cast<unsigned>(programStore.writeCount()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Preheating),
                          static_cast<int>(programState.processState.state));
    TEST_ASSERT_FALSE(programEvaluator.state().episodeActive);
    TEST_ASSERT_EQUAL_UINT64(0U, programEvaluator.state().creditedInBandMillis);

    auto wrongDuration = preheatQualificationInput(programState, 100U);
    wrongDuration.qualificationDurationMillis = 300'000U;
    const auto rejectedDuration = evaluateAndApplyTargetQualification(
        programApplication, programState, wrongDuration,
        RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_TRUE(rejectedDuration.status ==
                     TargetQualificationOrchestrationStatus::InvalidDecision);
    TEST_ASSERT_TRUE(rejectedDuration.qualification.lifecycle ==
                     TargetQualificationDecisionLifecycle::Discarded);
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBeforeProgram),
                           static_cast<unsigned>(programStore.writeCount()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Preheating),
                          static_cast<int>(programState.processState.state));
    TEST_ASSERT_FALSE(programEvaluator.state().episodeActive);
    TEST_ASSERT_EQUAL_UINT64(0U, programEvaluator.state().creditedInBandMillis);

    SequencedWriteStore manualStore;
    RunPersistenceCoordinator manualCoordinator(
        manualStore, device_platform::StorageEpoch(2U),
        RunCheckpointSchedule{});
    static_cast<void>(manualCoordinator.loadAndInitialize());
    TargetQualificationEvaluator manualEvaluator;
    TemperatureController manualController({}, {});
    TemperatureControlApplicationOrchestrator manualApplication(
        manualCoordinator, manualController, manualEvaluator);
    auto manualState =
        readyActiveManualRunWithSensorSelection(manualCoordinator, 907U);
    const auto writesBeforeManual = manualStore.writeCount();

    auto manualWrongBand = manualQualificationInput(manualState, 100U);
    manualWrongBand.bandCelsius = 0.6;
    const auto manualRejectedBand = evaluateAndApplyTargetQualification(
        manualApplication, manualState, manualWrongBand,
        RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_TRUE(manualRejectedBand.status ==
                     TargetQualificationOrchestrationStatus::InvalidDecision);
    TEST_ASSERT_TRUE(manualRejectedBand.qualification.lifecycle ==
                     TargetQualificationDecisionLifecycle::Discarded);
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBeforeManual),
                           static_cast<unsigned>(manualStore.writeCount()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(manualState.processState.state));
    TEST_ASSERT_FALSE(manualEvaluator.state().episodeActive);
    TEST_ASSERT_EQUAL_UINT64(0U, manualEvaluator.state().creditedInBandMillis);

    auto manualWrongDuration = manualQualificationInput(manualState, 100U);
    manualWrongDuration.qualificationDurationMillis = 300'000U;
    const auto manualRejectedDuration = evaluateAndApplyTargetQualification(
        manualApplication, manualState, manualWrongDuration,
        RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_TRUE(manualRejectedDuration.status ==
                     TargetQualificationOrchestrationStatus::InvalidDecision);
    TEST_ASSERT_TRUE(manualRejectedDuration.qualification.lifecycle ==
                     TargetQualificationDecisionLifecycle::Discarded);
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBeforeManual),
                           static_cast<unsigned>(manualStore.writeCount()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(manualState.processState.state));
    TEST_ASSERT_FALSE(manualEvaluator.state().episodeActive);
    TEST_ASSERT_EQUAL_UINT64(0U, manualEvaluator.state().creditedInBandMillis);
}

void test_manual_run_qualification_reaches_holding_via_application_path() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(3U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    TargetQualificationEvaluator evaluator;
    TemperatureController controller({}, {});
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);
    auto state = readyActiveManualRunWithSensorSelection(coordinator, 908U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.processState.state));

    const auto tracking = evaluateAndApplyTargetQualification(
        application, state, manualQualificationInput(state, 100U),
        RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_TRUE(tracking.status ==
                     TargetQualificationOrchestrationStatus::AppliedPersisted);
    TEST_ASSERT_TRUE(tracking.processDecision.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::QualifyingTarget),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_TRUE(state.activeManualRun.has_value());
    TEST_ASSERT_DOUBLE_WITHIN(
        0.0001, state.activeManualRun->values.targetTemperatureCelsius,
        tracking.qualification.candidateEvaluatorState.context->targetCelsius);

    const auto complete = evaluateAndApplyTargetQualification(
        application, state, manualQualificationInput(state, 600'100U),
        RunCheckpointTime{600'100U, std::nullopt});
    TEST_ASSERT_TRUE(complete.status ==
                     TargetQualificationOrchestrationStatus::AppliedPersisted);
    TEST_ASSERT_TRUE(complete.processDecision.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ManualHolding),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_TRUE(state.activeManualRun.has_value());
    TEST_ASSERT_TRUE(state.processRunSnapshot.has_value());
    TEST_ASSERT_EQUAL_UINT32(
        state.activeManualRun->values.qualificationDurationMinutes,
        state.processRunSnapshot->qualificationDurationMinutes);
}

void test_product_inserted_commits_before_advancing_and_restores() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = reachDurablyWaitingForProduct(coordinator, 790U);

    auto inserted = decideProcessTransition(
        state.processState, &*state.processRunSnapshot, ProcessSignals{},
        TransitionRequest{ProcessEvent::ProductInsertedConfirmed, std::nullopt},
        600200U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TransitionReason::ProductInserted),
                          static_cast<int>(inserted.reason));
    TargetQualificationEvaluator evaluator;
    TemperatureController controller(
        {},
        {IntegratorTransitionAction::Reset, IntegratorTransitionAction::Reset,
         IntegratorTransitionAction::Reset, 0.2});
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);
    store.faultAt(store.writeCount() + 1U,
                  SequencedWriteStore::WriteFault::FailBeforeBegin);
    const auto failed = application.persistTransition(
        state, inserted, RunCheckpointTime{600200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(failed.status));
    TEST_ASSERT_EQUAL_UINT32(0U, failed.messageCount);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::WaitingForProduct),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_FALSE(controller.state().pendingContextTransition.has_value());

    auto committed = application.persistTransition(
        state, inserted, RunCheckpointTime{600200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(committed.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_FALSE(committed.committedControlContextTransition.has_value());
    TEST_ASSERT_TRUE(controller.state().pendingContextTransition.has_value());
    TEST_ASSERT_TRUE(*controller.state().pendingContextTransition ==
                     CommittedControlContextTransition::ProductInserted);

    const auto insertedRecord = store.read(slotKey("rc1"), 8240U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(insertedRecord.status));
    const auto insertedEnvelope =
        device_platform::decodeEnvelope(insertedRecord.value);
    TEST_ASSERT_TRUE(insertedEnvelope.envelope.has_value());
    const auto insertedSnapshot =
        decodeRunPersistenceSnapshot(insertedEnvelope.envelope->payload,
                                     insertedEnvelope.envelope->schemaVersion);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(insertedSnapshot.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::ReachingTarget),
        static_cast<int>(insertedSnapshot.snapshot->processState.state));

    const auto restored =
        restoreRunPersistenceSnapshot(*insertedSnapshot.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(restored->processState.state));
}

void test_air_run_product_inserted_is_air_to_air_without_pi_transition() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state =
        reachDurablyWaitingForProduct(coordinator, 792U, RunSensorMode::Air);
    const auto inserted = decideProcessTransition(
        state.processState, &*state.processRunSnapshot, ProcessSignals{},
        TransitionRequest{ProcessEvent::ProductInsertedConfirmed, std::nullopt},
        600200U);
    TargetQualificationEvaluator evaluator;
    TemperatureController controller({}, {});
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);
    const auto committed = application.persistTransition(
        state, inserted, RunCheckpointTime{600200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(committed.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_FALSE(committed.committedControlContextTransition.has_value());
    TEST_ASSERT_FALSE(controller.state().pendingContextTransition.has_value());
}

void test_application_bridge_hands_off_cooling_context_once() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto program = runnableProgram();
    program.program.completion.mode = CompletionMode::CoolThenFinish;
    program.program.completion.coolingTargetCelsius = 20.0;
    TEST_ASSERT_TRUE(validateProgram(program).valid());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state,
                                startDecision(state, 793U, 100U, program),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    state.processState.state = ProcessState::Fermenting;
    state.processState.stateEnteredAtMillis = 100U;

    const auto transition = completeTimedRun(
        state.processState, *state.processRunSnapshot, 600'000U);
    TEST_ASSERT_TRUE(transition.proposed());
    TEST_ASSERT_TRUE(transition.committedControlContextTransition.has_value());

    TargetQualificationEvaluator evaluator;
    TemperatureController controller({}, bridgeTemperaturePolicy());
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);
    store.faultAt(store.writeCount() + 1U,
                  SequencedWriteStore::WriteFault::FailBeforeBegin);
    const auto failed = application.persistTransition(
        state, transition, RunCheckpointTime{600'000U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(failed.status));
    TEST_ASSERT_FALSE(controller.state().pendingContextTransition.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fermenting),
                          static_cast<int>(state.processState.state));

    store.clearFaults();
    const auto committed = application.persistTransition(
        state, transition, RunCheckpointTime{600'000U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(committed.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Cooling),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_FALSE(committed.committedControlContextTransition.has_value());
    TEST_ASSERT_TRUE(controller.state().pendingContextTransition.has_value());
    TEST_ASSERT_TRUE(
        *controller.state().pendingContextTransition ==
        CommittedControlContextTransition::CoolingTargetContextChange);
}

void test_application_actuator_handoff_and_lifecycle_boundary() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    TargetQualificationEvaluator evaluator;
    TemperatureController controller(bridgeTemperatureParameters(),
                                     bridgeTemperaturePolicy());
    ActuatorPlanner planner(bridgeActuatorPlannerParameters());
    device_platform_test_support::MockBidirectionalActuatorSink peltier;
    device_platform_test_support::MockBinaryOutputSink outerFan;
    device_platform_test_support::MockBinaryOutputSink innerFan;
    ActuatorPlanSinkDriver driver(peltier, outerFan, innerFan);
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator, planner, driver);

    auto state = readyActiveRunWithSensorSelection(coordinator, 797U);
    TemperatureControlEvaluationEvidence evidence;
    evidence.sampleTimestampMonotonicMillis = 100U;
    evidence.air = bridgeSensorSample(20.0);
    evidence.product = bridgeSensorSample(36.0);

    const auto first = application.evaluateTemperatureControl(state, evidence);
    TEST_ASSERT_TRUE(first.controlRequest.has_value());
    const auto repeatedBeforeTick =
        application.evaluateTemperatureControl(state, evidence);
    TEST_ASSERT_TRUE(repeatedBeforeTick.status ==
                     TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_FALSE(repeatedBeforeTick.controlRequest.has_value());

    const auto firstPlan = application.tickActuatorPlan(
        state, 100U,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed});
    TEST_ASSERT_TRUE(firstPlan.acceptedCommandSequence.has_value());
    TEST_ASSERT_EQUAL_UINT64(first.controlRequest->identity.sequence,
                             *firstPlan.acceptedCommandSequence);
    TEST_ASSERT_TRUE(firstPlan.appliedDirection ==
                     AbstractControlDirection::Heating);
    TEST_ASSERT_TRUE(peltier.forward());
    TEST_ASSERT_FALSE(peltier.reverse());
    TEST_ASSERT_TRUE(outerFan.enabled());
    TEST_ASSERT_TRUE(innerFan.enabled());

    evidence.sampleTimestampMonotonicMillis = 2'100U;
    const auto second = application.evaluateTemperatureControl(state, evidence);
    TEST_ASSERT_TRUE(second.controlRequest.has_value());
    TEST_ASSERT_EQUAL_UINT64(first.controlRequest->identity.sequence + 1U,
                             second.controlRequest->identity.sequence);
    TEST_ASSERT_TRUE(second.integralContributionQuote > 0.0);
    const auto secondPlan = application.tickActuatorPlan(
        state, 2'100U,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed});
    TEST_ASSERT_EQUAL_UINT64(second.controlRequest->identity.sequence,
                             *secondPlan.acceptedCommandSequence);

    const auto stop = stopDecision(state, 798U, 2'200U);
    TEST_ASSERT_TRUE(stop.proposed());
    const auto stopped = application.persistCommand(
        state, stop, RunCheckpointTime{2'200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(stopped.status));
    TEST_ASSERT_FALSE(planner.state().acceptedCommand.has_value());
    TEST_ASSERT_FALSE(peltier.forward());
    TEST_ASSERT_FALSE(peltier.reverse());
    // The lifecycle boundary applies the fail-closed Peltier stop immediately;
    // both fan outputs may remain in their configured post-run windows.
    TEST_ASSERT_TRUE(outerFan.enabled());
    TEST_ASSERT_TRUE(innerFan.enabled());
}

// Owner-Review R1 test 6: a stale-on-arrival B closes A's episode and hands
// {B, Rejected} to the Application. A further planner tick driven by an
// unconditional fail-closed safety event, with no new #22 evaluation in
// between, must not resurrect A's sequence into the Application's feedback
// slot - #22 itself hard-fails with InvalidSample on any sequence mismatch
// against its own last-emitted request (temperature_control.cpp), so a
// corrupted slot would not merely be a wrong value but would break the next
// #22 evaluation outright.
//
// Timing is deliberately chosen so B's own admission-staleness
// (now - B.createdAt) and the running H-heartbeat staleness (now - H) are
// two genuinely different gaps, not the same one observed twice: A and B
// share the same #22 evidence timestamp (0; repeated timestamps are valid),
// while A's planner tick - and thus H - lands close to the watchdog
// boundary (999 of 1000ms). This is the only way to trip B's own
// stale-on-arrival admission at t=1000 without the running watchdog (I-5)
// also tripping in the very same tick, which would otherwise call
// rejectToIdle() through its own correct freshTrustedActiveSequence path
// and mask the acceptedCommand-fallback bug this test targets.
void test_application_repeated_i_tick_after_stale_b_does_not_corrupt_next_evaluation() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    TargetQualificationEvaluator evaluator;
    TemperatureController controller(bridgeTemperatureParameters(),
                                     bridgeTemperaturePolicy());
    auto parameters = bridgeActuatorPlannerParameters();
    parameters.requestWatchdogMillis = 1'000U;
    ActuatorPlanner planner(parameters);
    device_platform_test_support::MockBidirectionalActuatorSink peltier;
    device_platform_test_support::MockBinaryOutputSink outerFan;
    device_platform_test_support::MockBinaryOutputSink innerFan;
    ActuatorPlanSinkDriver driver(peltier, outerFan, innerFan);
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator, planner, driver);
    auto state = readyActiveRunWithSensorSelection(coordinator, 950U);

    TemperatureControlEvaluationEvidence evidence;
    evidence.sampleTimestampMonotonicMillis = 0U;
    evidence.air = bridgeSensorSample(20.0);
    evidence.product = bridgeSensorSample(36.0);
    const auto a = application.evaluateTemperatureControl(state, evidence);
    TEST_ASSERT_TRUE(a.controlRequest.has_value());
    const auto aPlan = application.tickActuatorPlan(
        state, 999U,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed});
    TEST_ASSERT_TRUE(aPlan.appliedDirection ==
                     AbstractControlDirection::Heating);

    const auto b = application.evaluateTemperatureControl(state, evidence);
    TEST_ASSERT_TRUE(b.controlRequest.has_value());
    TEST_ASSERT_EQUAL_UINT64(a.controlRequest->identity.sequence + 1U,
                             b.controlRequest->identity.sequence);

    // B is only observed by the planner exactly at its own watchdog boundary
    // (createdAt 0, now 1000) - stale on arrival - while the running
    // heartbeat (H = 999, from A's own admission tick) is not yet due.
    const auto bPlan = application.tickActuatorPlan(
        state, 1'000U,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed});
    TEST_ASSERT_TRUE(bPlan.admissionOutcome ==
                     ActuatorAdmissionOutcome::StaleOnArrivalWatchdog);

    // A second planner tick, still well before H's own boundary (999+1000 =
    // 1999), with no new #22 evaluation in between.
    static_cast<void>(application.tickActuatorPlan(
        state, 1'500U,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::ImmediateStop}));

    evidence.sampleTimestampMonotonicMillis = 1'500U;
    const auto next = application.evaluateTemperatureControl(state, evidence);
    TEST_ASSERT_FALSE(next.status == TemperatureControlStatus::InvalidInput);
    TEST_ASSERT_FALSE(next.reason == TemperatureControlReason::InvalidSample);
}

void test_application_multi_rate_windows_and_downstream_counter_probe() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    TargetQualificationEvaluator evaluator;
    // Owner-Review F6: this fixture keeps a constant, unmoving sensor error
    // for 45 cycles (the test evidence never simulates the temperature
    // actually approaching target); with the shared
    // bridgeTemperatureParameters() gain, that sustained error would clamp
    // the integrator to its headroom within a handful of cycles, well
    // before the required three switching windows have elapsed. Only this
    // fixture's own copy gets a much smaller integral gain so genuine
    // multi-window progression can be observed without a future parameter
    // drift silently making the strict progression assertion vacuous
    // through early saturation; bridgeTemperatureParameters() itself stays
    // untouched for every other fixture.
    auto temperatureParameters = bridgeTemperatureParameters();
    temperatureParameters.productHeating.integralGainQuotePerCelsiusSecond =
        0.001;
    TemperatureController controller(temperatureParameters,
                                     bridgeTemperaturePolicy());
    auto parameters = bridgeActuatorPlannerParameters();
    parameters.switchingWindowMillis = 30'000U;
    parameters.minimumOnMillis = 2'000U;
    parameters.minimumOffMillis = 1'000U;
    parameters.polarityDeadTimeMillis = 3'000U;
    parameters.pulseAccumulatorCapMillis = 30'000U;
    ActuatorPlanner planner(parameters);
    device_platform_test_support::MockBidirectionalActuatorSink peltier;
    device_platform_test_support::MockBinaryOutputSink outerFan;
    device_platform_test_support::MockBinaryOutputSink innerFan;
    ActuatorPlanSinkDriver driver(peltier, outerFan, innerFan);
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator, planner, driver);
    auto state = readyActiveRunWithSensorSelection(coordinator, 799U);

    std::uint64_t windowSourceSequence = 0U;
    double previousIntegral = 0.0;
    for (std::uint32_t sample = 0U; sample <= 45U; ++sample) {
        const std::uint64_t timestamp =
            100U + static_cast<std::uint64_t>(sample) * 2'000U;
        TemperatureControlEvaluationEvidence evidence;
        evidence.sampleTimestampMonotonicMillis = timestamp;
        evidence.air = bridgeSensorSample(20.0);
        evidence.product = bridgeSensorSample(36.0);
        const auto evaluation =
            application.evaluateTemperatureControl(state, evidence);
        TEST_ASSERT_TRUE(evaluation.controlRequest.has_value());
        if (sample > 0U) {
            // Owner-Review F6: strict positive progression at every one of
            // these 45 non-saturated cycles (spanning three 30s switching
            // windows and two window boundaries at sample 15/30) - a stalled
            // sequence like 0.01, 0.01, ... would pass a ">=" check but is
            // exactly the integrator-freezing class Revision 8 exists to
            // rule out. Non-saturation is asserted explicitly so the
            // parameters cannot silently drift into a state where clamping,
            // not real progression, makes this pass.
            TEST_ASSERT_TRUE(evaluation.integralContributionQuote >
                             previousIntegral);
            TEST_ASSERT_TRUE(evaluation.reason !=
                             TemperatureControlReason::Saturated);
        }
        previousIntegral = evaluation.integralContributionQuote;

        const auto plan = application.tickActuatorPlan(
            state, timestamp,
            ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed});
        TEST_ASSERT_TRUE(plan.acceptedCommandSequence.has_value());
        TEST_ASSERT_EQUAL_UINT64(evaluation.controlRequest->identity.sequence,
                                 *plan.acceptedCommandSequence);
        TEST_ASSERT_TRUE(planner.state().activeWindow.has_value());
        if (sample == 0U || sample % 15U == 0U) {
            windowSourceSequence = evaluation.controlRequest->identity.sequence;
        }
        TEST_ASSERT_EQUAL_UINT64(
            windowSourceSequence,
            planner.state().activeWindow->sourceRequestSequence);
        TEST_ASSERT_TRUE(plan.reason !=
                         ActuatorPlanReason::CounterDirectionConfirming);
        TEST_ASSERT_TRUE(plan.reason != ActuatorPlanReason::MinimumOffTimeHeld);
        TEST_ASSERT_TRUE(plan.reason !=
                         ActuatorPlanReason::PolarityDeadTimeHeld);
        TEST_ASSERT_FALSE(peltier.simultaneousActivationObserved());
    }

    // A separate run makes the real counter-direction and dead-time gates
    // visible through the same Application -> Planner -> Sink path.
    SequencedWriteStore counterStore;
    RunPersistenceCoordinator counterCoordinator(
        counterStore, device_platform::StorageEpoch(2U),
        RunCheckpointSchedule{});
    static_cast<void>(counterCoordinator.loadAndInitialize());
    TargetQualificationEvaluator counterEvaluator;
    TemperatureController counterController(bridgeTemperatureParameters(),
                                            bridgeTemperaturePolicy());
    auto counterParameters = bridgeActuatorPlannerParameters();
    counterParameters.minimumOffMillis = 2'000U;
    counterParameters.polarityDeadTimeMillis = 3'000U;
    ActuatorPlanner counterPlanner(counterParameters);
    device_platform_test_support::MockBidirectionalActuatorSink counterPeltier;
    device_platform_test_support::MockBinaryOutputSink counterOuterFan;
    device_platform_test_support::MockBinaryOutputSink counterInnerFan;
    ActuatorPlanSinkDriver counterDriver(counterPeltier, counterOuterFan,
                                         counterInnerFan);
    TemperatureControlApplicationOrchestrator counterApplication(
        counterCoordinator, counterController, counterEvaluator, counterPlanner,
        counterDriver);
    auto counterState =
        readyActiveRunWithSensorSelection(counterCoordinator, 800U);
    TemperatureControlEvaluationEvidence counterEvidence;
    counterEvidence.air = bridgeSensorSample(20.0);
    counterEvidence.product = bridgeSensorSample(20.0);
    counterEvidence.sampleTimestampMonotonicMillis = 100U;
    const auto heating = counterApplication.evaluateTemperatureControl(
        counterState, counterEvidence);
    TEST_ASSERT_TRUE(heating.controlRequest.has_value());
    const auto initialCounterPlan = counterApplication.tickActuatorPlan(
        counterState, 100U,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed});
    TEST_ASSERT_TRUE(initialCounterPlan.appliedDirection ==
                     AbstractControlDirection::Heating);

    counterEvidence.product = bridgeSensorSample(45.0);
    counterEvidence.sampleTimestampMonotonicMillis = 2'100U;
    const auto cooling = counterApplication.evaluateTemperatureControl(
        counterState, counterEvidence);
    TEST_ASSERT_TRUE(cooling.direction == AbstractControlDirection::Cooling);
    // Owner-Review F7: capture the integral before the real downstream gate
    // takes effect.
    const double coolingIntegralBeforeGate = cooling.integralContributionQuote;
    auto counterPlan = counterApplication.tickActuatorPlan(
        counterState, 2'100U,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed});
    TEST_ASSERT_TRUE(counterPlan.reason ==
                     ActuatorPlanReason::CounterDirectionConfirming);
    TEST_ASSERT_TRUE(counterPlan.appliedDirection ==
                     AbstractControlDirection::Heating);

    counterEvidence.sampleTimestampMonotonicMillis = 4'100U;
    const auto coolingContinuation =
        counterApplication.evaluateTemperatureControl(counterState,
                                                      counterEvidence);
    // Owner-Review F7: the DeferredOrLimited disposition produced above by
    // the real CounterDirectionConfirming gate must be consumed by exactly
    // this next #22 evaluation and must hold the PI integrator - not merely
    // be a planner Reason label with no effect on #22's own state.
    TEST_ASSERT_TRUE(coolingContinuation.integralContributionQuote <=
                     coolingIntegralBeforeGate);
    counterPlan = counterApplication.tickActuatorPlan(
        counterState, 4'100U,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed});
    TEST_ASSERT_TRUE(coolingContinuation.controlRequest.has_value());
    TEST_ASSERT_TRUE(counterPlan.reason ==
                     ActuatorPlanReason::DirectionChangeApplied);
    TEST_ASSERT_TRUE(counterPlan.appliedDirection ==
                     AbstractControlDirection::Idle);

    counterEvidence.sampleTimestampMonotonicMillis = 6'100U;
    static_cast<void>(counterApplication.evaluateTemperatureControl(
        counterState, counterEvidence));
    counterPlan = counterApplication.tickActuatorPlan(
        counterState, 6'100U,
        ActuatorSafetyGateInput{ActuatorSafetyGateStatus::Allowed});
    TEST_ASSERT_TRUE(counterPlan.reason ==
                     ActuatorPlanReason::PolarityDeadTimeHeld);
    TEST_ASSERT_TRUE(counterPlan.appliedDirection ==
                     AbstractControlDirection::Idle);
}

void test_application_bridge_resets_runtime_at_committed_lifecycle_boundaries() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    TargetQualificationEvaluator evaluator;
    TemperatureController controller(bridgeTemperatureParameters(),
                                     bridgeTemperaturePolicy());
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);

    const auto first = controller.evaluate(bridgeAirInput(100U, 25.0, 20.0));
    auto secondInput = bridgeAirInput(1'100U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    TEST_ASSERT_TRUE(
        controller.evaluate(secondInput).integralContributionQuote > 0.0);
    buildQualifierCredit(evaluator);

    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            application
                .persistCommand(state, startDecision(state, 794U, 2'000U),
                                RunCheckpointTime{2'000U, std::nullopt})
                .status));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              controller.state().integralContributionQuote);
    TEST_ASSERT_FALSE(controller.state().lastActiveDirection.has_value());
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);

    const auto stop = stopDecision(state, 795U, 2'100U);
    TEST_ASSERT_TRUE(stop.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            application
                .persistCommand(state, stop,
                                RunCheckpointTime{2'100U, std::nullopt})
                .status));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              controller.state().integralContributionQuote);
    TEST_ASSERT_FALSE(controller.state().lastActiveDirection.has_value());

    auto firstAfterAbort = bridgeAirInput(2'200U, 25.0, 20.0);
    const auto firstAfter = controller.evaluate(firstAfterAbort);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              firstAfter.integralContributionQuote);
}

void test_abort_and_cool_is_a_new_active_run_boundary() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(23U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    TargetQualificationEvaluator evaluator;
    TemperatureController controller(bridgeTemperatureParameters(),
                                     bridgeTemperaturePolicy());
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);
    auto state = readyActiveRunWithSensorSelection(coordinator, 1250U);

    const auto first = controller.evaluate(bridgeAirInput(100U, 25.0, 20.0));
    auto secondInput = bridgeAirInput(1'100U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    TEST_ASSERT_TRUE(
        controller.evaluate(secondInput).integralContributionQuote > 0.0);
    TEST_ASSERT_TRUE(controller.state().feedbackWindow.has_value());
    TEST_ASSERT_TRUE(controller.markCommittedControlContextTransitionPending(
        CommittedControlContextTransition::TargetContextChange));
    buildQualifierCredit(evaluator);

    ManualRunPlanRequest coolingPlan;
    coolingPlan.runId = "abort-cooling-run";
    coolingPlan.targetTemperatureCelsius = 12.0;
    coolingPlan.sensorMode = RunSensorMode::Air;
    coolingPlan.qualificationBandCelsius = 0.5;
    coolingPlan.qualificationDurationMinutes = 10U;
    coolingPlan.maximumTargetReachMinutes = 180U;
    StopRequest request;
    request.envelope = {1251U,
                        CommandSource::LocalDisplay,
                        2'000U,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true,
                        std::nullopt};
    request.option = StopOption::AbortAndCool;
    request.coolingPlan = coolingPlan;
    request.safetyAllowsCooling = true;
    request.airSensorValid = true;
    request.coolingSensorValid = true;
    const auto decision = decideStop(state, request);
    TEST_ASSERT_TRUE(decision.proposed());

    const auto result = application.persistCommand(
        state, decision, RunCheckpointTime{2'000U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    bool hasRunStarted = false;
    for (std::size_t index = 0U; index < result.effectCount; ++index) {
        hasRunStarted =
            hasRunStarted || result.effects[index] == CommandEffect::RunStarted;
    }
    TEST_ASSERT_TRUE(hasRunStarted);
    TEST_ASSERT_TRUE(state.activeManualRun.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              controller.state().integralContributionQuote);
    TEST_ASSERT_FALSE(controller.state().lastActiveDirection.has_value());
    TEST_ASSERT_FALSE(controller.state().feedbackWindow.has_value());
    TEST_ASSERT_FALSE(controller.state().pendingContextTransition.has_value());
    TEST_ASSERT_FALSE(evaluator.state().episodeActive);
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);

    const auto firstAfter =
        controller.evaluate(bridgeAirInput(2'100U, 25.0, 20.0));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              firstAfter.integralContributionQuote);
}

void test_application_bridge_resets_runtime_on_recovery_activation() {
    SequencedWriteStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    static_cast<void>(persistedFermentingRun(seed, 796U));

    store.restart();
    RunPersistenceCoordinator persistence(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = persistence.loadAndInitialize();
    TEST_ASSERT_TRUE(loaded.snapshot.has_value());
    auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());

    TargetQualificationEvaluator evaluator;
    TemperatureController controller(bridgeTemperatureParameters(),
                                     bridgeTemperaturePolicy());
    const auto first = controller.evaluate(bridgeAirInput(100U, 25.0, 20.0));
    auto secondInput = bridgeAirInput(1'100U, 25.0, 20.0);
    secondInput.previousControlRequestFeedback = PreviousControlRequestFeedback{
        first.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    TEST_ASSERT_TRUE(
        controller.evaluate(secondInput).integralContributionQuote > 0.0);
    buildQualifierCredit(evaluator);

    TemperatureControlApplicationOrchestrator application(
        persistence, controller, evaluator);
    RunRecoveryCoordinator recovery;
    RunCommandState current = *restored;
    const auto result = application.activateRecovery(
        recovery, current, RunCheckpointTime{700'000U, std::nullopt},
        recoveryPlausibility(700'000U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              controller.state().integralContributionQuote);
    TEST_ASSERT_FALSE(controller.state().lastActiveDirection.has_value());
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);
}

void test_qualification_orchestrator_preserves_fault_signal_and_binds_sample_time() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(
                    state, startDecision(state, 1201U, 100U, preheatProgram()),
                    RunCheckpointTime{100U, std::nullopt})
                .status));

    TargetQualificationEvaluator faultEvaluator;
    TemperatureController faultController(bridgeTemperatureParameters(),
                                          bridgeTemperaturePolicy());
    TemperatureControlApplicationOrchestrator faultApplication(
        coordinator, faultController, faultEvaluator);
    const auto faultFirst =
        faultController.evaluate(bridgeAirInput(100U, 25.0, 20.0));
    auto faultSecondInput = bridgeAirInput(1'100U, 25.0, 20.0);
    faultSecondInput
        .previousControlRequestFeedback = PreviousControlRequestFeedback{
        faultFirst.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    TEST_ASSERT_TRUE(
        faultController.evaluate(faultSecondInput).integralContributionQuote >
        0.0);
    buildQualifierCredit(faultEvaluator);
    ProcessSignals baseline;
    baseline.criticalFault = true;
    const auto fault = evaluateAndApplyTargetQualification(
        faultApplication, state, preheatQualificationInput(state, 200U),
        RunCheckpointTime{200U, std::nullopt}, baseline);
    TEST_ASSERT_TRUE(fault.status ==
                     TargetQualificationOrchestrationStatus::AppliedPersisted);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fault),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TransitionReason::CriticalFault),
                          static_cast<int>(fault.processDecision->reason));
    TEST_ASSERT_FALSE(faultEvaluator.state().episodeActive);
    TEST_ASSERT_EQUAL_UINT64(0U, faultEvaluator.state().creditedInBandMillis);
    TEST_ASSERT_DOUBLE_WITHIN(
        0.0001, 0.0, faultController.state().integralContributionQuote);
    TEST_ASSERT_FALSE(faultController.state().lastActiveDirection.has_value());

    RunCommandState timeState;
    timeState.processState.state = ProcessState::Standby;
    SequencedWriteStore timeStore;
    RunPersistenceCoordinator timeCoordinator(
        timeStore, device_platform::StorageEpoch(2U), RunCheckpointSchedule{});
    static_cast<void>(timeCoordinator.loadAndInitialize());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            timeCoordinator
                .persistCommand(
                    timeState,
                    startDecision(timeState, 1202U, 300U, preheatProgram()),
                    RunCheckpointTime{300U, std::nullopt})
                .status));
    TargetQualificationEvaluator timeEvaluator;
    TemperatureController timeController({}, {});
    TemperatureControlApplicationOrchestrator timeApplication(
        timeCoordinator, timeController, timeEvaluator);
    const auto beforeWrites = timeStore.writeCount();
    const auto mismatch = evaluateAndApplyTargetQualification(
        timeApplication, timeState, preheatQualificationInput(timeState, 400U),
        RunCheckpointTime{401U, std::nullopt});
    TEST_ASSERT_TRUE(mismatch.status ==
                     TargetQualificationOrchestrationStatus::StaleDecision);
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(beforeWrites),
                           static_cast<unsigned>(timeStore.writeCount()));
    TEST_ASSERT_FALSE(timeEvaluator.state().episodeActive);

    const auto futureSample = evaluateAndApplyTargetQualification(
        timeApplication, timeState, preheatQualificationInput(timeState, 500U),
        RunCheckpointTime{499U, std::nullopt});
    TEST_ASSERT_TRUE(futureSample.status ==
                     TargetQualificationOrchestrationStatus::StaleDecision);
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(beforeWrites),
                           static_cast<unsigned>(timeStore.writeCount()));
}

void test_critical_fault_precedes_invalid_qualification_evidence() {
    for (int faultCase = 0; faultCase < 9; ++faultCase) {
        SequencedWriteStore store;
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(10U + faultCase),
            RunCheckpointSchedule{});
        static_cast<void>(coordinator.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                coordinator
                    .persistCommand(state,
                                    startDecision(state, 1210U + faultCase,
                                                  100U, preheatProgram()),
                                    RunCheckpointTime{100U, std::nullopt})
                    .status));

        TargetQualificationEvaluator evaluator;
        TemperatureController controller({}, {});
        TemperatureControlApplicationOrchestrator application(
            coordinator, controller, evaluator);
        auto input = preheatQualificationInput(state, 200U);
        ProcessSignals baseline;
        baseline.criticalFault = true;
        switch (faultCase) {
            case 0:
                input.targetCelsius = 37.0;
                break;
            case 1:
                input.controlSensorRole = ControlSensorRole::Product;
                break;
            case 2:
                input.bandCelsius = 0.6;
                break;
            case 3:
                input.qualificationDurationMillis = 1U;
                break;
            case 4:
                ++input.runRevision;
                break;
            case 5:
                ++input.processTransitionSequence;
                break;
            case 6:
                input.sampleTimestampMonotonicMillis = 201U;
                break;
            case 7:
                input.air.quality = device_platform::SensorQuality::Stale;
                input.air.filteredCelsius.reset();
                break;
            case 8:
                input.air.filteredCelsius =
                    std::numeric_limits<double>::quiet_NaN();
                break;
        }

        const auto fault = evaluateAndApplyTargetQualification(
            application, state, input, RunCheckpointTime{200U, std::nullopt},
            baseline);
        TEST_ASSERT_TRUE(fault.processDecision.has_value());
        TEST_ASSERT_EQUAL_INT(static_cast<int>(TransitionReason::CriticalFault),
                              static_cast<int>(fault.processDecision->reason));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                TargetQualificationOrchestrationStatus::AppliedPersisted),
            static_cast<int>(fault.status));
        TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fault),
                              static_cast<int>(state.processState.state));
        TEST_ASSERT_FALSE(fault.signals.qualificationProgress ==
                          QualificationProgress::Complete);
        TEST_ASSERT_FALSE(evaluator.state().episodeActive);
        TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);
    }
}

void test_critical_fault_persistence_failure_does_not_apply_process_fault() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(20U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(
                    state, startDecision(state, 1220U, 100U, preheatProgram()),
                    RunCheckpointTime{100U, std::nullopt})
                .status));
    TargetQualificationEvaluator evaluator;
    TemperatureController controller({}, {});
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);
    ProcessSignals baseline;
    baseline.criticalFault = true;
    auto input = preheatQualificationInput(state, 200U);
    store.faultAt(store.writeCount() + 1U,
                  SequencedWriteStore::WriteFault::FailBeforeBegin);
    const auto failed = evaluateAndApplyTargetQualification(
        application, state, input, RunCheckpointTime{200U, std::nullopt},
        baseline);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            TargetQualificationOrchestrationStatus::PersistenceFailed),
        static_cast<int>(failed.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Preheating),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_FALSE(evaluator.state().episodeActive);
}

void test_committed_target_changes_reset_qualification_without_a_sample() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(21U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    TargetQualificationEvaluator evaluator;
    TemperatureController controller({}, bridgeTemperaturePolicy());
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);
    auto state = readyActiveRunWithSensorSelection(coordinator, 1230U);
    buildQualifierCredit(evaluator);
    TEST_ASSERT_TRUE(evaluator.state().creditedInBandMillis > 0U);

    auto targetB = decideRunAdjustment(
        state, targetAdjustmentRequest(state, 1231U, 200U, 37.5));
    TEST_ASSERT_TRUE(targetB.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            application
                .persistCommand(state, targetB,
                                RunCheckpointTime{200U, std::nullopt})
                .status));
    TEST_ASSERT_DOUBLE_WITHIN(
        0.0001, 0.0,
        static_cast<double>(evaluator.state().creditedInBandMillis));

    auto targetA = decideRunAdjustment(
        state, targetAdjustmentRequest(state, 1232U, 300U, 38.0));
    TEST_ASSERT_TRUE(targetA.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            application
                .persistCommand(state, targetA,
                                RunCheckpointTime{300U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);

    const auto next = evaluateAndApplyTargetQualification(
        application, state, programTargetQualificationInput(state, 400U),
        RunCheckpointTime{400U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(QualificationProgress::InBand),
                          static_cast<int>(next.qualification.progress));
    TEST_ASSERT_EQUAL_UINT64(0U, next.qualification.creditedInBandMillis);
}

void test_failed_target_context_commit_keeps_qualification_credit() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(22U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    TargetQualificationEvaluator evaluator;
    TemperatureController controller({}, bridgeTemperaturePolicy());
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);
    auto state = readyActiveRunWithSensorSelection(coordinator, 1240U);
    buildQualifierCredit(evaluator);
    const auto beforeCredit = evaluator.state().creditedInBandMillis;
    const auto beforeTarget =
        state.activeProgramRun->effectiveValues().targetTemperatureCelsius;
    const auto target = decideRunAdjustment(
        state, targetAdjustmentRequest(state, 1241U, 200U, 37.5));
    store.faultAt(store.writeCount() + 1U,
                  SequencedWriteStore::WriteFault::FailBeforeBegin);
    const auto failed = application.persistCommand(
        state, target, RunCheckpointTime{200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(failed.status));
    TEST_ASSERT_EQUAL_UINT64(beforeCredit,
                             evaluator.state().creditedInBandMillis);
    TEST_ASSERT_DOUBLE_WITHIN(
        0.0001, beforeTarget,
        state.activeProgramRun->effectiveValues().targetTemperatureCelsius);
    TEST_ASSERT_FALSE(controller.state().pendingContextTransition.has_value());
}

void test_product_wait_expired_tombstones_and_does_not_revive_after_restart() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = reachDurablyWaitingForProduct(coordinator, 791U);
    // #21, 6.14.6: clearActiveRunState must reset these too - populate them
    // first so the assertions below are not a vacuous no-op check.
    state.sensorSelectionRuntime.phase = SensorSelectionPhase::NormalProduct;
    state.sensorSelectionRuntime.permission = SensorPeltierPermission::Allowed;
    state.sensorSelection = PersistedSensorSelectionState{
        SensorSelectionProvenance::InitialSelection,
        SensorSelectionDecisionCause::StartSelection, state.runRevision};

    // Let the state machine produce its automatic ProductWaitExpired
    // decision from the genuinely reached WaitingForProduct state -- no
    // manual state reset.
    const auto expired = decideProcessTransition(
        state.processState, &*state.processRunSnapshot, ProcessSignals{},
        TransitionRequest{}, 2400201U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::ProductWaitExpired),
        static_cast<int>(expired.reason));

    store.faultAt(store.writeCount() + 1U,
                  SequencedWriteStore::WriteFault::FailBeforeBegin);
    const auto expiredFailure = coordinator.persistTransition(
        state, expired, RunCheckpointTime{2400201U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(expiredFailure.status));
    TEST_ASSERT_TRUE(state.activeProgramRun.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::WaitingForProduct),
                          static_cast<int>(state.processState.state));

    const auto expiredCommit = coordinator.persistTransition(
        state, expired, RunCheckpointTime{2400201U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(expiredCommit.status));
    TEST_ASSERT_FALSE(state.activeProgramRun.has_value());
    TEST_ASSERT_TRUE(state.activeRunId.empty());
    // #21, 6.14.6: clearActiveRunState resets the sensor-selection fields on
    // this terminal path too, not just on the command-layer abort/complete
    // paths (test_run_commands.cpp already covers those).
    TEST_ASSERT_FALSE(state.sensorSelection.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SensorSelectionPhase::NoActiveRun),
                          static_cast<int>(state.sensorSelectionRuntime.phase));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorPeltierPermission::Blocked),
        static_cast<int>(state.sensorSelectionRuntime.permission));

    store.restart();
    RunPersistenceCoordinator tombstoneBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::NoActiveRun),
        static_cast<int>(tombstoneBoot.loadAndInitialize().status));
}

void test_periodic_unknown_outcomes_at_slot_and_head_are_unresolvable() {
    using Fault = SequencedWriteStore::WriteFault;
    using ReadFault = SequencedWriteStore::ReadFault;
    for (const auto readFault : {ReadFault::ForeignBytes, ReadFault::ReadError,
                                 ReadFault::CapacityError}) {
        for (const std::size_t writeNumber : {4U, 5U}) {
            SequencedWriteStore store;
            RunPersistenceCoordinator coordinator(
                store, device_platform::StorageEpoch(1U),
                RunCheckpointSchedule{});
            static_cast<void>(coordinator.loadAndInitialize());
            RunCommandState state;
            state.processState.state = ProcessState::Standby;
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(RunPersistenceResultStatus::Applied),
                static_cast<int>(
                    coordinator
                        .persistCommand(state, startDecision(state, 810U),
                                        RunCheckpointTime{100U, std::nullopt})
                        .status));
            store.faultAt(writeNumber, Fault::PowerCutAfterCommitBeforeReturn);
            store.readFaultAt(writeNumber, readFault);
            const auto result = coordinator.checkpointPeriodic(
                state, RunCheckpointTime{300100U, std::nullopt});
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(
                    RunPersistenceResultStatus::PersistenceIndeterminate),
                static_cast<int>(result.status));
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(
                    RunPersistenceCoordinatorState::BlockedIndeterminate),
                static_cast<int>(coordinator.state()));
        }
    }
}

void test_stale_invalid_and_time_mismatched_decisions_write_nothing() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    const auto beforeWrites = store.writeCount();
    auto stale = startDecision(state, 800U);
    state.runRevision = 1U;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::StaleDecision),
        static_cast<int>(
            coordinator
                .persistCommand(state, stale,
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    CommandDecision invalid;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::InvalidDecision),
        static_cast<int>(
            coordinator
                .persistCommand(state, invalid,
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    state.runRevision = 0U;
    auto mismatch = startDecision(state, 801U, 100U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::TimeMismatch),
        static_cast<int>(
            coordinator
                .persistCommand(state, mismatch,
                                RunCheckpointTime{101U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(beforeWrites),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_FALSE(state.activeProgramRun.has_value());
}

void test_stale_invalid_and_time_mismatched_transitions_write_nothing() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, manualStartDecision(state, 850U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    // The state machine produces this only after a real manual hold.
    state.processState.state = ProcessState::ManualHolding;
    state.processState.stateEnteredAtMillis = 100U;
    state.processState.targetReachStartedAtMillis = 0U;
    ProcessSignals signals;
    const auto beforeWrites = store.writeCount();

    const auto legitimate = decideProcessTransition(
        state.processState, &*state.processRunSnapshot, signals,
        TransitionRequest{ProcessEvent::FinishHoldConfirmed, std::nullopt},
        200U);
    TEST_ASSERT_TRUE(legitimate.proposed());

    const auto mismatched = coordinator.persistTransition(
        state, legitimate, RunCheckpointTime{201U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::TimeMismatch),
        static_cast<int>(mismatched.status));
    TEST_ASSERT_EQUAL_UINT32(0U, mismatched.messageCount);

    TransitionDecision invalid;
    const auto rejectedInvalid = coordinator.persistTransition(
        state, invalid, RunCheckpointTime{200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::InvalidDecision),
        static_cast<int>(rejectedInvalid.status));
    TEST_ASSERT_EQUAL_UINT32(0U, rejectedInvalid.messageCount);

    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(beforeWrites),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ManualHolding),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(coordinator.state()));

    // Genuinely advance the run with a second, later-computed decision so
    // `legitimate` (captured against the earlier `before`) becomes stale
    // relative to the now-current process state -- a real race between two
    // decisions, not a manual state edit.
    const auto second = decideProcessTransition(
        state.processState, &*state.processRunSnapshot, signals,
        TransitionRequest{ProcessEvent::FinishHoldConfirmed, std::nullopt},
        201U);
    TEST_ASSERT_TRUE(second.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, second,
                                   RunCheckpointTime{201U, std::nullopt})
                .status));
    const auto afterSecondWrites = store.writeCount();
    // applyProcessTransition increments transitionSequence by exactly one on
    // a real RAM apply and nothing else does; capturing it here proves the
    // stale attempt below does not apply, regardless of which ProcessState
    // `second` actually landed on.
    const auto sequenceAfterSecond = state.processState.transitionSequence;

    const auto stale = coordinator.persistTransition(
        state, legitimate, RunCheckpointTime{200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::StaleDecision),
        static_cast<int>(stale.status));
    TEST_ASSERT_EQUAL_UINT32(0U, stale.messageCount);
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(afterSecondWrites),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(coordinator.state()));
    TEST_ASSERT_EQUAL_UINT32(sequenceAfterSecond,
                             state.processState.transitionSequence);
}

void test_restart_after_prepared_or_slot_cut_is_interrupted() {
    using Fault = SequencedWriteStore::WriteFault;
    for (std::size_t writeNumber : {2U, 3U}) {
        SequencedWriteStore store;
        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        static_cast<void>(coordinator.loadAndInitialize());
        RunCommandState state;
        state.processState.state = ProcessState::Standby;
        store.faultAt(writeNumber, Fault::FailBeforeBegin);
        const auto result = coordinator.persistCommand(
            state, startDecision(state, 730U + writeNumber),
            RunCheckpointTime{100U, std::nullopt});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::WriteFailed),
            static_cast<int>(result.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                RunPersistenceCoordinatorState::BlockedIndeterminate),
            static_cast<int>(coordinator.state()));
        store.restart();
        RunPersistenceCoordinator afterBoot(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceLoadStatus::PreparedInterrupted),
            static_cast<int>(afterBoot.loadAndInitialize().status));
    }
}

void test_periodic_slot_and_head_faults_preserve_cutpoint_truth() {
    using Fault = SequencedWriteStore::WriteFault;

    SequencedWriteStore slotFailure;
    RunPersistenceCoordinator slotCoordinator(slotFailure,
                                              device_platform::StorageEpoch(1U),
                                              RunCheckpointSchedule{});
    static_cast<void>(slotCoordinator.loadAndInitialize());
    RunCommandState slotState;
    slotState.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            slotCoordinator
                .persistCommand(slotState, startDecision(slotState, 740U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    slotFailure.faultAt(4U, Fault::FailBeforeBegin);
    const auto slotResult = slotCoordinator.checkpointPeriodic(
        slotState, RunCheckpointTime{300100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(slotResult.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceDurability::Unchanged),
                          static_cast<int>(slotResult.durability));

    SequencedWriteStore headFailure;
    RunPersistenceCoordinator headCoordinator(headFailure,
                                              device_platform::StorageEpoch(1U),
                                              RunCheckpointSchedule{});
    static_cast<void>(headCoordinator.loadAndInitialize());
    RunCommandState headState;
    headState.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            headCoordinator
                .persistCommand(headState, startDecision(headState, 741U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    headFailure.faultAt(5U, Fault::FailBeforeBegin);
    const auto headResult = headCoordinator.checkpointPeriodic(
        headState, RunCheckpointTime{300100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(headResult.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceDurability::Changed),
                          static_cast<int>(headResult.durability));
    const auto orphan = headFailure.read(slotKey("rc1"), 8240U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(orphan.status));
    const auto orphanEnvelope = device_platform::decodeEnvelope(orphan.value);
    TEST_ASSERT_TRUE(orphanEnvelope.envelope.has_value());
    const auto orphanRevision = orphanEnvelope.envelope->versionValue;
    const auto retry = headCoordinator.checkpointPeriodic(
        headState, RunCheckpointTime{300200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(retry.status));
    const auto replacement = headFailure.read(slotKey("rc1"), 8240U);
    const auto replacementEnvelope =
        device_platform::decodeEnvelope(replacement.value);
    TEST_ASSERT_TRUE(replacementEnvelope.envelope.has_value());
    TEST_ASSERT_TRUE(replacementEnvelope.envelope->versionValue >
                     orphanRevision);
}

void test_invalid_effect_and_message_counts_are_rejected_before_writes() {
    device_platform_test_support::SimulatedPersistentStateStore commandStore;
    RunPersistenceCoordinator commandCoordinator(
        commandStore, device_platform::StorageEpoch(1U),
        RunCheckpointSchedule{});
    static_cast<void>(commandCoordinator.loadAndInitialize());
    RunCommandState commandState;
    commandState.processState.state = ProcessState::Standby;
    auto command = startDecision(commandState, 750U);
    command.effectCount = command.effects.size() + 1U;
    const auto commandResult = commandCoordinator.persistCommand(
        commandState, command, RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::InvalidDecision),
        static_cast<int>(commandResult.status));
    TEST_ASSERT_EQUAL_UINT32(0U, commandResult.effectCount);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::ReadyEmpty),
        static_cast<int>(commandCoordinator.state()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::NotFound),
        static_cast<int>(commandStore.read(slotKey("rh0"), 256U).status));

    device_platform_test_support::SimulatedPersistentStateStore transitionStore;
    RunPersistenceCoordinator transitionCoordinator(
        transitionStore, device_platform::StorageEpoch(1U),
        RunCheckpointSchedule{});
    static_cast<void>(transitionCoordinator.loadAndInitialize());
    RunCommandState transitionState;
    transitionState.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            transitionCoordinator
                .persistCommand(transitionState,
                                manualStartDecision(transitionState, 751U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    transitionState.processState.state = ProcessState::ManualHolding;
    transitionState.processState.stateEnteredAtMillis = 100U;
    transitionState.processState.targetReachStartedAtMillis = 0U;
    ProcessSignals signals;
    auto transition = decideProcessTransition(
        transitionState.processState, &*transitionState.processRunSnapshot,
        signals,
        TransitionRequest{ProcessEvent::FinishHoldConfirmed, std::nullopt},
        200U);
    TEST_ASSERT_TRUE(transition.proposed());
    transition.messageCount = transition.messages.size() + 1U;
    const auto transitionResult = transitionCoordinator.persistTransition(
        transitionState, transition, RunCheckpointTime{200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::InvalidDecision),
        static_cast<int>(transitionResult.status));
    TEST_ASSERT_EQUAL_UINT32(0U, transitionResult.messageCount);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ManualHolding),
                          static_cast<int>(transitionState.processState.state));
}

// ---------------------------------------------------------------------------
// #21, 9.3: persistSensorSelection (automatic path)
// ---------------------------------------------------------------------------

RunCommandState readyActiveRunWithSensorSelection(
    RunPersistenceCoordinator& coordinator, CommandId startId) {
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, startId),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    TEST_ASSERT_TRUE(state.sensorSelection.has_value());
    TEST_ASSERT_TRUE(
        state.sensorSelection ==
        (PersistedSensorSelectionState{
            SensorSelectionProvenance::InitialSelection,
            SensorSelectionDecisionCause::StartSelection, state.runRevision}));
    return state;
}

RunCommandState persistedPreheatingRun(RunPersistenceCoordinator& coordinator,
                                       CommandId startId) {
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(
                    state,
                    startDecision(state, startId, 100U, preheatProgram()),
                    RunCheckpointTime{100U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Preheating),
                          static_cast<int>(state.processState.state));
    return state;
}

CrossRolePlausibilityContext recoveryPlausibility(
    std::uint64_t evaluatedAtMonotonicMillis, bool productValid) {
    CrossRolePlausibilityContext plausibility;
    plausibility.phase = ProcessState::RecoveryEvaluation;
    plausibility.evaluationMonotonicMillis = evaluatedAtMonotonicMillis;
    const auto valid = [](double value) {
        device_platform::SensorQualitySnapshot snapshot;
        snapshot.quality = device_platform::SensorQuality::Valid;
        snapshot.filteredCelsius = value;
        return snapshot;
    };
    plausibility.air = valid(20.0);
    plausibility.product = valid(21.0);
    plausibility.cooling = valid(19.0);
    if (!productValid) {
        plausibility.product.quality = device_platform::SensorQuality::Failed;
        plausibility.product.filteredCelsius.reset();
        plausibility.product.lastFaultReason =
            device_platform::SensorFaultReason::MissingSample;
    }
    return plausibility;
}

RunCommandState persistedWaitingForProductRun(
    RunPersistenceCoordinator& coordinator, CommandId startId) {
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(
                    state,
                    startDecision(state, startId, 100U, preheatProgram()),
                    RunCheckpointTime{100U, std::nullopt})
                .status));
    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::InBand;
    auto transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, transition,
                                   RunCheckpointTime{200U, std::nullopt})
                .status));
    signals.qualificationProgress = QualificationProgress::Complete;
    transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 600300U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, transition,
                                   RunCheckpointTime{600300U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::WaitingForProduct),
                          static_cast<int>(state.processState.state));
    return state;
}

RunCommandState persistedWaitingWithUtcCheckpoint(
    RunPersistenceCoordinator& coordinator, CommandId startId) {
    auto state = persistedWaitingForProductRun(coordinator, startId);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(coordinator
                             .checkpointPeriodic(
                                 state, RunCheckpointTime{900300U, 1700000000})
                             .status));
    return state;
}

RunCommandState persistedFermentingRun(RunPersistenceCoordinator& coordinator,
                                       CommandId startId) {
    auto state = readyActiveRunWithSensorSelection(coordinator, startId);
    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::Complete;
    auto transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(coordinator
                             .persistTransition(state, transition,
                                                RunCheckpointTime{200U, 0})
                             .status));
    transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 600300U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(coordinator
                             .persistTransition(state, transition,
                                                RunCheckpointTime{600300U, 0})
                             .status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fermenting),
                          static_cast<int>(state.processState.state));
    return state;
}

RunCommandState persistedCoolHoldingRun(RunPersistenceCoordinator& coordinator,
                                        CommandId startId) {
    auto program = runnableProgram();
    program.program.completion.mode = CompletionMode::CoolAndHoldForDuration;
    program.program.completion.coolingTargetCelsius = 20.0;
    program.program.completion.holdDurationMinutes = 30U;
    TEST_ASSERT_TRUE(validateProgram(program).valid());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state,
                                startDecision(state, startId, 100U, program),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::Complete;
    auto transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, transition,
                                   RunCheckpointTime{200U, std::nullopt})
                .status));
    signals.qualificationProgress = QualificationProgress::Complete;
    transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 600300U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, transition,
                                   RunCheckpointTime{600300U, std::nullopt})
                .status));
    transition = decideProcessTransition(
        state.processState, &*state.processRunSnapshot, ProcessSignals{},
        TransitionRequest{}, 7800300U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, transition,
                                   RunCheckpointTime{7800300U, std::nullopt})
                .status));
    signals.qualificationProgress = QualificationProgress::Unavailable;
    signals.coolingTargetConditionValid = true;
    transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 7800400U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::CoolHolding),
                          static_cast<int>(transition.after.state));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, transition,
                                   RunCheckpointTime{7800400U, std::nullopt})
                .status));
    return state;
}

RunCommandState persistedQualifyingTargetRun(
    RunPersistenceCoordinator& coordinator, CommandId startId,
    bool issueTargetReachWarning) {
    TargetQualificationEvaluator evaluator;
    TemperatureController controller({}, {});
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);
    auto state = readyActiveRunWithSensorSelection(coordinator, startId);
    const auto tracking = evaluateAndApplyTargetQualification(
        application, state, programTargetQualificationInput(state, 100U),
        RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_TRUE(tracking.status ==
                     TargetQualificationOrchestrationStatus::AppliedPersisted);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::QualifyingTarget),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_TRUE(
        state.processState.qualificationValidSinceMillis.has_value());

    if (issueTargetReachWarning) {
        ProcessSignals signals;
        signals.qualificationProgress = QualificationProgress::InBand;
        const auto warning = decideProcessTransition(
            state.processState, &*state.processRunSnapshot, signals,
            TransitionRequest{}, 10'800'100U);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(TransitionReason::TargetReachTimeExceeded),
            static_cast<int>(warning.reason));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::Applied),
            static_cast<int>(
                coordinator
                    .persistTransition(
                        state, warning,
                        RunCheckpointTime{10'800'100U, std::nullopt})
                    .status));
        TEST_ASSERT_TRUE(state.processState.targetReachWarningIssued);
    }
    return state;
}

using RecoveryPhaseSeed = RunCommandState (*)(RunPersistenceCoordinator&,
                                              CommandId);

void assertLoadedRecoveryPreservesPhase(RecoveryPhaseSeed seedPhase,
                                        ProcessState expectedState,
                                        CommandId startId) {
    SequencedWriteStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    static_cast<void>(seedPhase(seed, startId));

    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = afterBoot.loadAndInitialize();
    TEST_ASSERT_TRUE(loaded.snapshot.has_value());
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto outcome = afterBoot.activateLoadedRun(
        *restored, RunCheckpointTime{700'000U, std::nullopt},
        recoveryPlausibility(700'000U));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(outcome.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(expectedState),
        static_cast<int>(outcome.resultingState.processState.state));
    TEST_ASSERT_EQUAL_UINT64(
        700'000U, outcome.resultingState.processState.stateEnteredAtMillis);
    TEST_ASSERT_FALSE(outcome.resultingState.processState
                          .qualificationValidSinceMillis.has_value());
    TEST_ASSERT_FALSE(
        outcome.resultingState.processState.targetReachWarningIssued);
    TEST_ASSERT_EQUAL_UINT64(
        expectedState == ProcessState::ReachingTarget ? 700'000U : 0U,
        outcome.resultingState.processState.targetReachStartedAtMillis);
}

void test_loaded_qualifying_recovery_rebases_and_restarts_qualification() {
    SequencedWriteStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    const auto seeded = persistedQualifyingTargetRun(seed, 1275U, true);
    TEST_ASSERT_TRUE(
        seeded.processState.qualificationValidSinceMillis.has_value());
    TEST_ASSERT_TRUE(seeded.processState.targetReachWarningIssued);

    store.restart();
    RunPersistenceCoordinator persistence(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = persistence.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(loaded.status));
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::QualifyingTarget),
                          static_cast<int>(restored->processState.state));
    TEST_ASSERT_TRUE(
        restored->processState.qualificationValidSinceMillis.has_value());

    TargetQualificationEvaluator evaluator;
    TemperatureController controller(bridgeTemperatureParameters(),
                                     bridgeTemperaturePolicy());
    const auto firstControl =
        controller.evaluate(bridgeAirInput(100U, 25.0, 20.0));
    auto secondControl = bridgeAirInput(1'100U, 25.0, 20.0);
    secondControl
        .previousControlRequestFeedback = PreviousControlRequestFeedback{
        firstControl.controlRequest->identity.sequence,
        PreviousControlRequestFeedback::Disposition::NoIntegratorConstraint};
    TEST_ASSERT_TRUE(
        controller.evaluate(secondControl).integralContributionQuote > 0.0);
    buildQualifierCredit(evaluator);

    TemperatureControlApplicationOrchestrator application(
        persistence, controller, evaluator);
    RunCommandState current = *restored;
    RunRecoveryCoordinator recovery;
    const auto recovered = application.activateRecovery(
        recovery, current, RunCheckpointTime{700'000U, std::nullopt},
        recoveryPlausibility(700'000U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(recovered.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(current.processState.state));
    TEST_ASSERT_EQUAL_UINT64(700'000U,
                             current.processState.stateEnteredAtMillis);
    TEST_ASSERT_EQUAL_UINT64(700'000U,
                             current.processState.targetReachStartedAtMillis);
    TEST_ASSERT_FALSE(
        current.processState.qualificationValidSinceMillis.has_value());
    TEST_ASSERT_FALSE(current.processState.targetReachWarningIssued);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.0,
                              controller.state().integralContributionQuote);
    TEST_ASSERT_FALSE(controller.state().lastActiveDirection.has_value());
    TEST_ASSERT_FALSE(controller.state().feedbackWindow.has_value());
    TEST_ASSERT_FALSE(controller.state().pendingContextTransition.has_value());
    TEST_ASSERT_FALSE(evaluator.state().episodeActive);
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);

    const auto firstAfterRecovery = evaluateAndApplyTargetQualification(
        application, current,
        programTargetQualificationInput(current, 700'100U),
        RunCheckpointTime{700'100U, std::nullopt});
    TEST_ASSERT_TRUE(firstAfterRecovery.status ==
                     TargetQualificationOrchestrationStatus::AppliedPersisted);
    TEST_ASSERT_TRUE(firstAfterRecovery.qualification.progress ==
                     QualificationProgress::InBand);
    TEST_ASSERT_EQUAL_UINT64(
        0U, firstAfterRecovery.qualification.creditedInBandMillis);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::QualifyingTarget),
                          static_cast<int>(current.processState.state));
    TEST_ASSERT_TRUE(
        current.processState.qualificationValidSinceMillis.has_value());
    TEST_ASSERT_EQUAL_UINT64(
        700'100U, *current.processState.qualificationValidSinceMillis);
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);
    TEST_ASSERT_TRUE(evaluator.state().episodeActive);
}

void test_fallback_qualifying_recovery_rebases_through_common_helper() {
    SequencedWriteStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{1U});
    static_cast<void>(seed.loadAndInitialize());
    const auto seeded = persistedQualifyingTargetRun(seed, 1276U, false);
    TEST_ASSERT_TRUE(
        seeded.processState.qualificationValidSinceMillis.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            seed.checkpointPeriodic(seeded,
                                    RunCheckpointTime{60'100U, std::nullopt})
                .status));

    const auto currentReference =
        RunPersistenceCoordinatorTestAccess::currentReference(seed);
    store.backing().injectCorruption(
        slotKey(currentReference.slot == 0U ? "rc0" : "rc1"),
        "damaged-qualifying-current");
    store.restart();

    RunPersistenceCoordinator persistence(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{1U});
    const auto loaded = persistence.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::FallbackRecovered),
        static_cast<int>(loaded.status));
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::QualifyingTarget),
                          static_cast<int>(restored->processState.state));
    TEST_ASSERT_TRUE(
        restored->processState.qualificationValidSinceMillis.has_value());

    TargetQualificationEvaluator evaluator;
    TemperatureController controller({}, {});
    TemperatureControlApplicationOrchestrator application(
        persistence, controller, evaluator);
    RunRecoveryCoordinator recovery;
    RunCommandState current = *restored;
    const auto recovered = application.activateRecovery(
        recovery, current, RunCheckpointTime{80'000U, std::nullopt},
        recoveryPlausibility(80'000U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(recovered.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(current.processState.state));
    TEST_ASSERT_FALSE(
        current.processState.qualificationValidSinceMillis.has_value());
    TEST_ASSERT_EQUAL_UINT64(80'000U,
                             current.processState.targetReachStartedAtMillis);
    TEST_ASSERT_FALSE(current.processState.targetReachWarningIssued);
}

void test_loaded_recovery_rebase_preserves_reaching_preheating_and_fermenting() {
    assertLoadedRecoveryPreservesPhase(readyActiveRunWithSensorSelection,
                                       ProcessState::ReachingTarget, 1277U);
    assertLoadedRecoveryPreservesPhase(persistedPreheatingRun,
                                       ProcessState::Preheating, 1278U);
    assertLoadedRecoveryPreservesPhase(persistedFermentingRun,
                                       ProcessState::Fermenting, 1279U);
}

void test_resolve_recovery_outcome_waiting_assume_still_valid_resumes() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    static_cast<void>(persistedWaitingForProductRun(seed, 1006U));

    store.restart();
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = coordinator.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto activated = coordinator.activateLoadedRun(
        *restored, RunCheckpointTime{700000U, std::nullopt},
        recoveryPlausibility(700000U));
    RunCommandState current = activated.resultingState;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::RecoveryEvaluation),
                          static_cast<int>(current.processState.state));

    const ResolveRecoveryUncertaintyRequest request{
        1007U, current.runRevision, current.recoveryEpisodeRevision,
        RecoveryUncertaintyDecision::AssumeStillValid};
    const auto result = coordinator.resolveRecoveryOutcome(
        current, request, RunCheckpointTime{700100U, std::nullopt},
        recoveryPlausibility(700100U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::WaitingForProduct),
                          static_cast<int>(current.processState.state));
    TEST_ASSERT_TRUE(current.priorBootPhaseElapsed.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::WaitingForProduct),
        static_cast<int>(current.priorBootPhaseElapsed->taggedState));
    TEST_ASSERT_TRUE(current.pendingRecoveryAnchor.has_value());
}

void test_resolve_recovery_outcome_rejects_gate_and_deduplicates() {
    SequencedWriteStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    static_cast<void>(persistedWaitingForProductRun(seed, 1018U));
    store.restart();

    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = coordinator.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto activated = coordinator.activateLoadedRun(
        *restored, RunCheckpointTime{700000U, std::nullopt},
        recoveryPlausibility(700000U));
    RunCommandState current = activated.resultingState;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::RecoveryEvaluation),
                          static_cast<int>(current.processState.state));

    const auto writesBeforeStale = store.writeCount();
    const ResolveRecoveryUncertaintyRequest staleRequest{
        1019U, current.runRevision, current.recoveryEpisodeRevision - 1U,
        RecoveryUncertaintyDecision::AssumeStillValid};
    const auto stale = coordinator.resolveRecoveryOutcome(
        current, staleRequest, RunCheckpointTime{700100U, std::nullopt},
        recoveryPlausibility(700100U, false));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::StaleDecision),
        static_cast<int>(stale.status));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBeforeStale),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::RecoveryEvaluation),
                          static_cast<int>(current.processState.state));

    const ResolveRecoveryUncertaintyRequest request{
        1020U, current.runRevision, current.recoveryEpisodeRevision,
        RecoveryUncertaintyDecision::AssumeStillValid};
    const auto rejected = coordinator.resolveRecoveryOutcome(
        current, request, RunCheckpointTime{700100U, std::nullopt},
        recoveryPlausibility(700100U, false));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(rejected.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fault),
                          static_cast<int>(current.processState.state));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(coordinator.state()));
    TEST_ASSERT_FALSE(current.pendingRecoveryAnchor.has_value());

    const auto duplicate = coordinator.resolveRecoveryOutcome(
        current, request, RunCheckpointTime{700200U, std::nullopt},
        recoveryPlausibility(700200U, false));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::AlreadyPersisted),
        static_cast<int>(duplicate.status));

    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto rebooted = afterBoot.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(rebooted.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Fault),
        static_cast<int>(rebooted.snapshot->processState.state));
}

void test_waiting_definitely_expired_tombstones_before_sensor_gate() {
    SequencedWriteStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    auto state = persistedWaitingForProductRun(seed, 1021U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            seed.checkpointPeriodic(state,
                                    RunCheckpointTime{2400300U, std::nullopt})
                .status));

    store.restart();
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = coordinator.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto outcome = coordinator.activateLoadedRun(
        *restored, RunCheckpointTime{2500000U, std::nullopt},
        recoveryPlausibility(2500000U, false));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(outcome.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Standby),
        static_cast<int>(outcome.resultingState.processState.state));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::ReadyEmpty),
        static_cast<int>(coordinator.state()));
    TEST_ASSERT_FALSE(outcome.resultingState.activeProgramRun.has_value());

    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::NoActiveRun),
        static_cast<int>(afterBoot.loadAndInitialize().status));
}

void test_resolve_recovery_outcome_waiting_threshold_crossed_tombstones() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    static_cast<void>(persistedWaitingForProductRun(seed, 1008U));
    store.restart();
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = coordinator.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto activated = coordinator.activateLoadedRun(
        *restored, RunCheckpointTime{700000U, std::nullopt},
        recoveryPlausibility(700000U));
    RunCommandState current = activated.resultingState;
    const ResolveRecoveryUncertaintyRequest request{
        1009U, current.runRevision, current.recoveryEpisodeRevision,
        RecoveryUncertaintyDecision::AssumeThresholdCrossed};
    const auto result = coordinator.resolveRecoveryOutcome(
        current, request, RunCheckpointTime{700100U, std::nullopt},
        recoveryPlausibility(700100U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                          static_cast<int>(current.processState.state));
    TEST_ASSERT_FALSE(current.activeProgramRun.has_value());
    TEST_ASSERT_FALSE(current.pendingRecoveryAnchor.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::ReadyEmpty),
        static_cast<int>(coordinator.state()));
}

void test_resolve_recovery_outcome_fermenting_bounds_gate_and_completion() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(seed, 1010U);
    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::Complete;
    auto transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            seed.persistTransition(state, transition,
                                   RunCheckpointTime{200U, std::nullopt})
                .status));
    transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 600300U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            seed.persistTransition(state, transition,
                                   RunCheckpointTime{600300U, std::nullopt})
                .status));

    store.restart();
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = coordinator.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto activated = coordinator.activateLoadedRun(
        *restored, RunCheckpointTime{700000U, std::nullopt},
        recoveryPlausibility(700000U));
    RunCommandState current = activated.resultingState;
    const ResolveRecoveryUncertaintyRequest rejectedRequest{
        1011U, current.runRevision, current.recoveryEpisodeRevision,
        RecoveryUncertaintyDecision::AssumeThresholdCrossed};
    const auto rejected = coordinator.resolveRecoveryOutcome(
        current, rejectedRequest, RunCheckpointTime{700100U, std::nullopt},
        recoveryPlausibility(700100U));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NotAllowedInState),
        static_cast<int>(rejected.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fermenting),
                          static_cast<int>(current.processState.state));

    current.priorBootPhaseElapsed->elapsed.upperBoundSeconds = 7201U;
    const ResolveRecoveryUncertaintyRequest acceptedRequest{
        1012U, current.runRevision, current.recoveryEpisodeRevision,
        RecoveryUncertaintyDecision::AssumeThresholdCrossed};
    const auto accepted = coordinator.resolveRecoveryOutcome(
        current, acceptedRequest, RunCheckpointTime{700200U, std::nullopt},
        recoveryPlausibility(700200U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(accepted.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                          static_cast<int>(current.processState.state));
    TEST_ASSERT_FALSE(current.pendingRecoveryAnchor.has_value());
    TEST_ASSERT_FALSE(current.priorBootPhaseElapsed.has_value());
    TEST_ASSERT_EQUAL_UINT32(1U, accepted.messageCount);
}

void test_resolve_recovery_outcome_cool_holding_bounds_gate_completes_hold() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    static_cast<void>(persistedCoolHoldingRun(seed, 1014U));
    store.restart();
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = coordinator.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto activated = coordinator.activateLoadedRun(
        *restored, RunCheckpointTime{8000000U, std::nullopt},
        recoveryPlausibility(8000000U));
    RunCommandState current = activated.resultingState;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::CoolHolding),
                          static_cast<int>(current.processState.state));
    TEST_ASSERT_TRUE(current.priorBootPhaseElapsed.has_value());
    const ResolveRecoveryUncertaintyRequest request{
        1015U, current.runRevision, current.recoveryEpisodeRevision,
        RecoveryUncertaintyDecision::AssumeThresholdCrossed};
    const auto rejected = coordinator.resolveRecoveryOutcome(
        current, request, RunCheckpointTime{8000100U, std::nullopt},
        recoveryPlausibility(8000100U));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NotAllowedInState),
        static_cast<int>(rejected.status));

    current.priorBootPhaseElapsed->elapsed.upperBoundSeconds = 1801U;
    const auto accepted = coordinator.resolveRecoveryOutcome(
        current, request, RunCheckpointTime{8000200U, std::nullopt},
        recoveryPlausibility(8000200U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(accepted.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                          static_cast<int>(current.processState.state));
    TEST_ASSERT_EQUAL_UINT32(1U, accepted.messageCount);
}

RunCommandState readyActiveManualRunWithSensorSelection(
    RunPersistenceCoordinator& coordinator, CommandId startId);

void test_activate_loaded_completed_run_refreshes_boot_time_without_transition() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveManualRunWithSensorSelection(coordinator, 1013U);
    state.processState.state = ProcessState::ManualHolding;
    state.processState.stateEnteredAtMillis = 100U;
    state.processState.targetReachStartedAtMillis = 0U;
    const auto transition = decideProcessTransition(
        state.processState, &*state.processRunSnapshot, ProcessSignals{},
        TransitionRequest{ProcessEvent::FinishHoldConfirmed, std::nullopt},
        200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, transition,
                                   RunCheckpointTime{200U, std::nullopt})
                .status));

    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = afterBoot.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto outcome = afterBoot.activateLoadedRun(
        *restored, RunCheckpointTime{500U, std::nullopt},
        recoveryPlausibility(500U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(outcome.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Completed),
        static_cast<int>(outcome.resultingState.processState.state));
    TEST_ASSERT_EQUAL_UINT64(
        500U, outcome.resultingState.processState.stateEnteredAtMillis);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceDurability::Unchanged),
        static_cast<int>(outcome.persistenceResult.durability));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(afterBoot.state()));
}

void test_apply_live_recovery_evidence_requires_a_value_to_latch() {
    RunCommandState state;
    state.processState.state = ProcessState::RecoveryEvaluation;
    state.pendingRecoveryAnchor = PendingRecoveryAnchor{};
    state.lastRecoveryEpisodeEvidence = RecoveryEpisodeEvidence{};

    auto plausibility = recoveryPlausibility(100U);
    plausibility.air.filteredCelsius.reset();
    applyLiveRecoveryEvidence(state, plausibility);
    TEST_ASSERT_FALSE(
        state.lastRecoveryEpisodeEvidence->firstAfterRestart.air.has_value());
    TEST_ASSERT_TRUE(state.lastRecoveryEpisodeEvidence->firstAfterRestart
                         .product.has_value());

    plausibility.air.filteredCelsius = 20.5;
    applyLiveRecoveryEvidence(state, plausibility);
    TEST_ASSERT_TRUE(
        state.lastRecoveryEpisodeEvidence->firstAfterRestart.air.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(
        20.5, *state.lastRecoveryEpisodeEvidence->firstAfterRestart.air
                   ->filteredCelsius);
}

void test_activate_loaded_run_resumes_and_retains_unresolved_anchor() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 1001U);

    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::Complete;
    auto transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::QualificationTrackingStarted),
        static_cast<int>(transition.reason));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, transition,
                                   RunCheckpointTime{200U, std::nullopt})
                .status));

    transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 600300U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TransitionReason::TargetQualified),
                          static_cast<int>(transition.reason));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, transition,
                                   RunCheckpointTime{600300U, std::nullopt})
                .status));

    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = afterBoot.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(loaded.status));
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());

    const auto live = recoveryPlausibility(700000U);
    const auto outcome = afterBoot.activateLoadedRun(
        *restored, RunCheckpointTime{700000U, std::nullopt}, live);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(outcome.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(afterBoot.state()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Fermenting),
        static_cast<int>(outcome.resultingState.processState.state));
    TEST_ASSERT_TRUE(outcome.resultingState.pendingRecoveryAnchor.has_value());
    TEST_ASSERT_TRUE(
        outcome.resultingState.recoveryBootAnchorMonotonicMillis.has_value());
    TEST_ASSERT_TRUE(outcome.resultingState.priorBootPhaseElapsed.has_value());
    TEST_ASSERT_FALSE(outcome.resultingState.priorBootPhaseElapsed->elapsed
                          .upperBoundSeconds.has_value());
    TEST_ASSERT_TRUE(outcome.resultingState.lastRecoveryEpisodeEvidence
                         ->firstAfterRestart.air.has_value());
    TEST_ASSERT_TRUE(outcome.resultingState.lastRecoveryEpisodeEvidence
                         ->firstAfterRestart.product.has_value());
    TEST_ASSERT_TRUE(outcome.resultingState.lastRecoveryEpisodeEvidence
                         ->firstAfterRestart.cooling.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::SensorQuality::Valid),
        static_cast<int>(outcome.resultingState.recoveryTemperatureEvidence
                             .lastKnown.product.quality));
}

void test_run_recovery_coordinator_delegates_loaded_activation() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(seed, 1020U);

    store.restart();
    RunPersistenceCoordinator persistence(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = persistence.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());

    RunCommandState current = *restored;
    RunRecoveryCoordinator recovery;
    const auto result = recovery.activate(
        persistence, current, RunCheckpointTime{700000U, std::nullopt},
        recoveryPlausibility(700000U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(current.processState.state));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(persistence.state()));
}

void test_live_fermenting_transition_folds_observed_time_once() {
    using Fault = SequencedWriteStore::WriteFault;
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 1030U);
    state.processState.state = ProcessState::Fermenting;
    state.processState.stateEnteredAtMillis = 1000U;
    state.runProgress.observedRunSeconds = 7U;

    const auto transition =
        propose(state.processState, ProcessState::Completed,
                TransitionReason::FermentationCompleted, 5000U);
    TEST_ASSERT_TRUE(transition.proposed());

    store.faultAt(store.writeCount() + 1U, Fault::FailBeforeBegin);
    const auto failed = coordinator.persistTransition(
        state, transition, RunCheckpointTime{5000U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(failed.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fermenting),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_EQUAL_UINT32(7U, state.runProgress.observedRunSeconds);

    const auto committed = coordinator.persistTransition(
        state, transition, RunCheckpointTime{5000U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(committed.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_EQUAL_UINT32(11U, state.runProgress.observedRunSeconds);

    store.restart();
    RunPersistenceCoordinator restoredPersistence(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = restoredPersistence.loadAndInitialize();
    TEST_ASSERT_TRUE(loaded.snapshot.has_value());
    TEST_ASSERT_EQUAL_UINT32(11U,
                             loaded.snapshot->runProgress.observedRunSeconds);
}

void test_real_fermenting_hop_one_folds_only_this_boot_local_seconds() {
    SequencedWriteStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    auto state = persistedFermentingRun(seed, 1031U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            seed.checkpointPeriodic(state,
                                    RunCheckpointTime{900300U, std::nullopt})
                .status));

    store.restart();
    RunPersistenceCoordinator firstBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = firstBoot.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto activated = firstBoot.activateLoadedRun(
        *restored, RunCheckpointTime{1000000U, std::nullopt},
        recoveryPlausibility(1000000U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(activated.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Fermenting),
        static_cast<int>(activated.resultingState.processState.state));
    TEST_ASSERT_EQUAL_UINT32(
        300U, activated.resultingState.runProgress.observedRunSeconds);

    // A persisted Hop-1-only episode is refreshed, not re-entered. It must
    // not fold the old N1 a second time.
    store.restart();
    RunPersistenceCoordinator refreshedBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto refreshedLoaded = refreshedBoot.loadAndInitialize();
    const auto refreshedState =
        restoreRunPersistenceSnapshot(*refreshedLoaded.snapshot);
    TEST_ASSERT_TRUE(refreshedState.has_value());
    const auto refreshed = refreshedBoot.activateLoadedRun(
        *refreshedState, RunCheckpointTime{1100000U, std::nullopt},
        recoveryPlausibility(1100000U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(refreshed.persistenceResult.status));
    TEST_ASSERT_EQUAL_UINT32(
        300U, refreshed.resultingState.runProgress.observedRunSeconds);
}

void test_three_real_fermenting_reboots_fold_exactly_n1_plus_n2_plus_n3() {
    SequencedWriteStore store;
    RunPersistenceCoordinator firstSeed(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(firstSeed.loadAndInitialize());
    auto state = persistedFermentingRun(firstSeed, 1032U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(firstSeed
                             .checkpointPeriodic(
                                 state, RunCheckpointTime{900300U, 1700000000})
                             .status));

    // Keep each coordinator alive in the explicit sequence below; the
    // checkpoints are the three concrete N1/N2/N3 fold points.
    store.restart();
    RunPersistenceCoordinator boot1(store, device_platform::StorageEpoch(1U),
                                    RunCheckpointSchedule{});
    const auto loaded1 = boot1.loadAndInitialize();
    const auto restored1 = restoreRunPersistenceSnapshot(*loaded1.snapshot);
    TEST_ASSERT_TRUE(restored1.has_value());
    auto outcome1 = boot1.activateLoadedRun(
        *restored1, RunCheckpointTime{1000000U, 1700000400},
        recoveryPlausibility(1000000U));
    TEST_ASSERT_EQUAL_UINT32(
        300U, outcome1.resultingState.runProgress.observedRunSeconds);
    auto current = outcome1.resultingState;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            boot1
                .checkpointPeriodic(current,
                                    RunCheckpointTime{1350300U, 1700000400})
                .status));

    store.restart();
    RunPersistenceCoordinator boot2(store, device_platform::StorageEpoch(1U),
                                    RunCheckpointSchedule{});
    const auto loaded2 = boot2.loadAndInitialize();
    const auto restored2 = restoreRunPersistenceSnapshot(*loaded2.snapshot);
    TEST_ASSERT_TRUE(restored2.has_value());
    auto outcome2 = boot2.activateLoadedRun(
        *restored2, RunCheckpointTime{1450000U, 1700000850},
        recoveryPlausibility(1450000U));
    TEST_ASSERT_EQUAL_UINT32(
        650U, outcome2.resultingState.runProgress.observedRunSeconds);
    current = outcome2.resultingState;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            boot2
                .checkpointPeriodic(current,
                                    RunCheckpointTime{1800100U, 1700000850})
                .status));

    store.restart();
    RunPersistenceCoordinator boot3(store, device_platform::StorageEpoch(1U),
                                    RunCheckpointSchedule{});
    const auto loaded3 = boot3.loadAndInitialize();
    const auto restored3 = restoreRunPersistenceSnapshot(*loaded3.snapshot);
    TEST_ASSERT_TRUE(restored3.has_value());
    const auto outcome3 = boot3.activateLoadedRun(
        *restored3, RunCheckpointTime{1900000U, 1700001400},
        recoveryPlausibility(1900000U));
    TEST_ASSERT_EQUAL_UINT32(
        1000U, outcome3.resultingState.runProgress.observedRunSeconds);
}

void test_hop_one_waiting_utc_reevaluation_has_single_gate_and_tombstone_rules() {
    // Same-boot definite expiry: no Gate-A context is required.
    SequencedWriteStore expiredStore;
    RunPersistenceCoordinator expiredSeed(expiredStore,
                                          device_platform::StorageEpoch(1U),
                                          RunCheckpointSchedule{});
    static_cast<void>(expiredSeed.loadAndInitialize());
    static_cast<void>(persistedWaitingWithUtcCheckpoint(expiredSeed, 1033U));
    expiredStore.restart();
    RunPersistenceCoordinator expiredBoot(expiredStore,
                                          device_platform::StorageEpoch(1U),
                                          RunCheckpointSchedule{});
    const auto expiredLoaded = expiredBoot.loadAndInitialize();
    const auto expiredRestored =
        restoreRunPersistenceSnapshot(*expiredLoaded.snapshot);
    TEST_ASSERT_TRUE(expiredRestored.has_value());
    const auto expiredActivation = expiredBoot.activateLoadedRun(
        *expiredRestored, RunCheckpointTime{1000000U, std::nullopt},
        recoveryPlausibility(1000000U));
    RunCommandState expiredCurrent = expiredActivation.resultingState;
    TEST_ASSERT_TRUE(expiredCurrent.pendingRecoveryAnchor.has_value());
    expiredCurrent.pendingRecoveryAnchor->accumulatedBeforeEpisode =
        PriorBootPhaseElapsed{1800U, 1800U};
    const auto writesBeforeExpiry = expiredStore.writeCount();
    const auto expired =
        RunRecoveryCoordinator(expiredBoot)
            .reevaluateRecoveryTime(expiredCurrent,
                                    RunCheckpointTime{1000100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(expired.status));
    TEST_ASSERT_TRUE(expiredStore.writeCount() > writesBeforeExpiry);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Standby),
                          static_cast<int>(expiredCurrent.processState.state));
    TEST_ASSERT_FALSE(expiredCurrent.activeProgramRun.has_value());
    TEST_ASSERT_FALSE(
        RunPersistenceCoordinatorTestAccess::fallbackReferenceOptional(
            expiredBoot)
            .has_value());

    // Same-boot valid result: no-context remains fail-closed; fresh context
    // alone enables the existing Gate-A resume path.
    SequencedWriteStore validStore;
    RunPersistenceCoordinator validSeed(
        validStore, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(validSeed.loadAndInitialize());
    static_cast<void>(persistedWaitingWithUtcCheckpoint(validSeed, 1034U));
    validStore.restart();
    RunPersistenceCoordinator validBoot(
        validStore, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto validLoaded = validBoot.loadAndInitialize();
    const auto validRestored =
        restoreRunPersistenceSnapshot(*validLoaded.snapshot);
    TEST_ASSERT_TRUE(validRestored.has_value());
    const auto validActivation = validBoot.activateLoadedRun(
        *validRestored, RunCheckpointTime{1000000U, std::nullopt},
        recoveryPlausibility(1000000U));
    RunCommandState validCurrent = validActivation.resultingState;
    const auto revisionBefore = validCurrent.runRevision;
    const auto writesBeforeNoContext = validStore.writeCount();
    auto validRecovery = RunRecoveryCoordinator(validBoot);
    const auto noContext = validRecovery.reevaluateRecoveryTime(
        validCurrent, RunCheckpointTime{1000100U, 1700000500});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NotAllowedInState),
        static_cast<int>(noContext.status));
    TEST_ASSERT_EQUAL_UINT32(revisionBefore, validCurrent.runRevision);
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBeforeNoContext),
                           static_cast<unsigned>(validStore.writeCount()));
    const auto resumed = validRecovery.reevaluateRecoveryTime(
        validCurrent, RunCheckpointTime{1000100U, 1700000500},
        recoveryPlausibility(1000100U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(resumed.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::WaitingForProduct),
                          static_cast<int>(validCurrent.processState.state));
    TEST_ASSERT_FALSE(validCurrent.pendingRecoveryAnchor.has_value());
}

void test_run_recovery_coordinator_reevaluates_resumed_time_without_biological_fold() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(seed, 1021U);
    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::Complete;
    auto transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            seed.persistTransition(state, transition,
                                   RunCheckpointTime{200U, 1700000200})
                .status));
    transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 600300U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            seed.persistTransition(state, transition,
                                   RunCheckpointTime{600300U, 1700000600})
                .status));

    store.restart();
    RunPersistenceCoordinator persistence(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = persistence.loadAndInitialize();
    auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto activated = persistence.activateLoadedRun(
        *restored, RunCheckpointTime{700000U, std::nullopt},
        recoveryPlausibility(700000U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(activated.persistenceResult.status));
    RunCommandState current = activated.resultingState;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fermenting),
                          static_cast<int>(current.processState.state));
    TEST_ASSERT_TRUE(current.pendingRecoveryAnchor.has_value());
    const auto observedBefore = current.runProgress.observedRunSeconds;
    const auto revisionBefore = current.runRevision;

    RunRecoveryCoordinator recovery(persistence);
    const auto unresolved = recovery.reevaluateRecoveryTime(
        current, RunCheckpointTime{700050U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::NotDue),
                          static_cast<int>(unresolved.status));
    TEST_ASSERT_EQUAL_UINT32(revisionBefore, current.runRevision);

    const auto resolved = recovery.reevaluateRecoveryTime(
        current, RunCheckpointTime{700100U, 1700000700});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(resolved.status));
    TEST_ASSERT_EQUAL_UINT32(revisionBefore + 1U, current.runRevision);
    TEST_ASSERT_EQUAL_UINT32(observedBefore,
                             current.runProgress.observedRunSeconds);
    TEST_ASSERT_TRUE(current.priorBootPhaseElapsed.has_value());
    TEST_ASSERT_TRUE(
        current.priorBootPhaseElapsed->elapsed.upperBoundSeconds.has_value());
    TEST_ASSERT_FALSE(current.pendingRecoveryAnchor.has_value());
    TEST_ASSERT_FALSE(current.recoveryBootAnchorMonotonicMillis.has_value());

    store.restart();
    RunPersistenceCoordinator restoredPersistence(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto restoredLoad = restoredPersistence.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(restoredLoad.status));
    TEST_ASSERT_TRUE(restoredLoad.snapshot.has_value());
    TEST_ASSERT_TRUE(restoredLoad.snapshot->priorBootPhaseElapsed.has_value());
    TEST_ASSERT_TRUE(restoredLoad.snapshot->priorBootPhaseElapsed->elapsed
                         .upperBoundSeconds.has_value());
    TEST_ASSERT_FALSE(restoredLoad.snapshot->pendingRecoveryAnchor.has_value());
}

void test_run_recovery_coordinator_books_weighting_atomically_once() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(seed, 1022U);
    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::Complete;
    auto transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            seed.persistTransition(state, transition,
                                   RunCheckpointTime{200U, std::nullopt})
                .status));
    transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 600300U);
    auto beforeEvidence = recoveryPlausibility(600300U);
    state.recoveryTemperatureEvidence.lastKnown.air = {
        beforeEvidence.air.filteredCelsius, beforeEvidence.air.quality};
    state.recoveryTemperatureEvidence.lastKnown.product = {
        beforeEvidence.product.filteredCelsius, beforeEvidence.product.quality};
    state.recoveryTemperatureEvidence.lastKnown.cooling = {
        beforeEvidence.cooling.filteredCelsius, beforeEvidence.cooling.quality};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            seed.persistTransition(state, transition,
                                   RunCheckpointTime{600300U, std::nullopt},
                                   &beforeEvidence)
                .status));

    store.restart();
    RunPersistenceCoordinator persistence(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = persistence.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto activated = persistence.activateLoadedRun(
        *restored, RunCheckpointTime{700000U, std::nullopt},
        recoveryPlausibility(700000U));
    RunCommandState current = activated.resultingState;
    TEST_ASSERT_TRUE(current.lastRecoveryEpisodeEvidence.has_value());
    TEST_ASSERT_TRUE(current.lastRecoveryEpisodeEvidence
                         ->weightedProgressSegmentId.has_value());
    current.runProgress.observedRunSeconds = 12U;
    current.nominalRecoveryAdjustment =
        NominalRecoveryAdjustmentState{5U, 1U, 5U};
    const auto observedBefore = current.runProgress.observedRunSeconds;
    const auto nominalBefore = *current.nominalRecoveryAdjustment;
    const auto segmentId =
        *current.lastRecoveryEpisodeEvidence->weightedProgressSegmentId;

    RecoveryProgressWeightingInput input;
    input.phase = ProcessState::Fermenting;
    input.outage = RecoveryOutageBounds{200U, 100U};
    input.usableSensorRole = RunSensorMode::Product;
    FixedRecoveryProgressModel model;
    RunRecoveryCoordinator recovery(persistence);
    const auto booked = recovery.applyRecoveryProgressWeighting(
        current, current.runRevision, current.recoveryEpisodeRevision,
        segmentId, RunCheckpointTime{700100U, std::nullopt}, input, model);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(booked.status));
    TEST_ASSERT_EQUAL_UINT32(1U, model.calls);
    TEST_ASSERT_EQUAL_UINT32(observedBefore,
                             current.runProgress.observedRunSeconds);
    TEST_ASSERT_TRUE(current.nominalRecoveryAdjustment.has_value());
    TEST_ASSERT_EQUAL_UINT32(
        nominalBefore.cumulativeAppliedSeconds,
        current.nominalRecoveryAdjustment->cumulativeAppliedSeconds);
    TEST_ASSERT_TRUE(current.runProgress.weightedProgress.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WeightedProgressCoverage::Complete),
        static_cast<int>(current.runProgress.weightedProgress->coverage));
    TEST_ASSERT_EQUAL_UINT64(
        10U,
        current.runProgress.weightedProgress->cumulative.lowerBoundSeconds);
    TEST_ASSERT_EQUAL_UINT64(
        20U,
        *current.runProgress.weightedProgress->cumulative.upperBoundSeconds);
    TEST_ASSERT_EQUAL_UINT32(segmentId,
                             current.runProgress.weightedProgress->lastApplied
                                 ->lastAppliedSegmentId);

    const auto duplicate = recovery.applyRecoveryProgressWeighting(
        current, current.runRevision, current.recoveryEpisodeRevision,
        segmentId, RunCheckpointTime{700200U, std::nullopt}, input, model);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::AlreadyProcessed),
        static_cast<int>(duplicate.status));
    TEST_ASSERT_EQUAL_UINT32(2U, model.calls);

    const UnavailableRecoveryProgressWeightingModel unavailable;
    const auto unavailableResult = recovery.applyRecoveryProgressWeighting(
        current, current.runRevision, current.recoveryEpisodeRevision,
        segmentId, RunCheckpointTime{700300U, std::nullopt}, input,
        unavailable);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NotEligible),
        static_cast<int>(unavailableResult.status));
}

void test_activate_loaded_run_resolved_resume_clears_anchor() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 1002U);

    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::Complete;
    auto transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, transition,
                                   RunCheckpointTime{200U, 1700000200})
                .status));
    transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 600300U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, transition,
                                   RunCheckpointTime{600300U, 1700000600})
                .status));

    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = afterBoot.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto outcome = afterBoot.activateLoadedRun(
        *restored, RunCheckpointTime{700000U, 1700000700},
        recoveryPlausibility(700000U));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(outcome.persistenceResult.status));
    TEST_ASSERT_TRUE(outcome.resultingState.priorBootPhaseElapsed.has_value());
    TEST_ASSERT_TRUE(outcome.resultingState.priorBootPhaseElapsed->elapsed
                         .upperBoundSeconds.has_value());
    TEST_ASSERT_FALSE(outcome.resultingState.pendingRecoveryAnchor.has_value());
    TEST_ASSERT_FALSE(
        outcome.resultingState.recoveryBootAnchorMonotonicMillis.has_value());
}

void test_activate_loaded_run_episode_refreshes_hop_one_anchor() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(
                    state, startDecision(state, 1003U, 100U, preheatProgram()),
                    RunCheckpointTime{100U, std::nullopt})
                .status));

    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::InBand;
    auto transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(TransitionReason::QualificationTrackingStarted),
        static_cast<int>(transition.reason));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, transition,
                                   RunCheckpointTime{200U, std::nullopt})
                .status));
    signals.qualificationProgress = QualificationProgress::Complete;
    transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 600300U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TransitionReason::PreheatQualified),
                          static_cast<int>(transition.reason));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistTransition(state, transition,
                                   RunCheckpointTime{600300U, std::nullopt})
                .status));

    store.restart();
    RunPersistenceCoordinator firstBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto firstLoaded = firstBoot.loadAndInitialize();
    const auto firstRestored =
        restoreRunPersistenceSnapshot(*firstLoaded.snapshot);
    TEST_ASSERT_TRUE(firstRestored.has_value());
    const auto live = recoveryPlausibility(700000U);
    const auto first = firstBoot.activateLoadedRun(
        *firstRestored, RunCheckpointTime{700000U, std::nullopt}, live);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(first.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::RecoveryEvaluation),
        static_cast<int>(first.resultingState.processState.state));
    TEST_ASSERT_TRUE(first.resultingState.pendingRecoveryAnchor.has_value());
    const auto firstRevision = first.resultingState.recoveryEpisodeRevision;
    const auto firstAnchor = *first.resultingState.pendingRecoveryAnchor;

    store.restart();
    RunPersistenceCoordinator secondBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto secondLoaded = secondBoot.loadAndInitialize();
    const auto secondRestored =
        restoreRunPersistenceSnapshot(*secondLoaded.snapshot);
    TEST_ASSERT_TRUE(secondRestored.has_value());
    const auto second = secondBoot.activateLoadedRun(
        *secondRestored, RunCheckpointTime{800000U, std::nullopt},
        recoveryPlausibility(800000U));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(second.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::RecoveryEvaluation),
        static_cast<int>(second.resultingState.processState.state));
    TEST_ASSERT_EQUAL_UINT32(firstRevision + 1U,
                             second.resultingState.recoveryEpisodeRevision);
    TEST_ASSERT_TRUE(
        second.resultingState.recoveryBootAnchorMonotonicMillis.has_value());
    TEST_ASSERT_EQUAL_UINT64(
        800000U, *second.resultingState.recoveryBootAnchorMonotonicMillis);
    const auto& secondAnchor = *second.resultingState.pendingRecoveryAnchor;
    TEST_ASSERT_TRUE(equalProcessRuntimeState(
        firstAnchor.originalProcessState, secondAnchor.originalProcessState));
    TEST_ASSERT_EQUAL_UINT64(
        firstAnchor.knownPhaseSecondsAtOriginalCheckpoint,
        secondAnchor.knownPhaseSecondsAtOriginalCheckpoint);
    TEST_ASSERT_EQUAL_UINT64(firstAnchor.knownSecondsSinceOriginalCheckpoint,
                             secondAnchor.knownSecondsSinceOriginalCheckpoint);
}

void test_activate_loaded_run_persists_sensor_gate_rejection_as_fault() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    const auto state = readyActiveRunWithSensorSelection(coordinator, 1004U);
    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = afterBoot.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    auto invalidEvidence = recoveryPlausibility(700000U, false);
    const auto writesBefore = store.writeCount();

    const auto outcome = afterBoot.activateLoadedRun(
        *restored, RunCheckpointTime{700000U, std::nullopt}, invalidEvidence);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(outcome.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Fault),
        static_cast<int>(outcome.resultingState.processState.state));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(afterBoot.state()));
    TEST_ASSERT_TRUE(store.writeCount() > writesBefore);

    store.restart();
    RunPersistenceCoordinator rebooted(store, device_platform::StorageEpoch(1U),
                                       RunCheckpointSchedule{});
    const auto rebootedLoad = rebooted.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(rebootedLoad.status));
    const auto rebootedState =
        restoreRunPersistenceSnapshot(*rebootedLoad.snapshot);
    TEST_ASSERT_TRUE(rebootedState.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Fault),
                          static_cast<int>(rebootedState->processState.state));

    const auto writesBeforeRestore = store.writeCount();
    const auto activation = rebooted.activateLoadedRun(
        *rebootedState, RunCheckpointTime{700100U, std::nullopt},
        recoveryPlausibility(700100U, true));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(activation.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceStep::RamApply),
                          static_cast<int>(activation.persistenceResult.step));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceDurability::Unchanged),
        static_cast<int>(activation.persistenceResult.durability));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Fault),
        static_cast<int>(activation.resultingState.processState.state));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(rebooted.state()));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBeforeRestore),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorPeltierPermission::Blocked),
        static_cast<int>(
            activation.resultingState.sensorSelectionRuntime.permission));
}

void test_loaded_gate_rejection_keeps_persistence_cutpoint_contract() {
    using Fault = SequencedWriteStore::WriteFault;
    for (const auto offset : {1U, 2U, 3U}) {
        SequencedWriteStore store;
        RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                       RunCheckpointSchedule{});
        static_cast<void>(seed.loadAndInitialize());
        static_cast<void>(
            readyActiveRunWithSensorSelection(seed, 1023U + offset));
        store.restart();

        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        const auto loaded = coordinator.loadAndInitialize();
        const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
        TEST_ASSERT_TRUE(restored.has_value());
        store.faultAt(offset, Fault::FailBeforeBegin);
        const auto outcome = coordinator.activateLoadedRun(
            *restored, RunCheckpointTime{700000U, std::nullopt},
            recoveryPlausibility(700000U, false));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::WriteFailed),
            static_cast<int>(outcome.persistenceResult.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                offset == 1U
                    ? RunPersistenceCoordinatorState::LoadedActiveRun
                    : RunPersistenceCoordinatorState::BlockedIndeterminate),
            static_cast<int>(coordinator.state()));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(offset == 1U ? RunPersistenceDurability::Unchanged
                                          : RunPersistenceDurability::Changed),
            static_cast<int>(outcome.persistenceResult.durability));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(ProcessState::ReachingTarget),
            static_cast<int>(outcome.resultingState.processState.state));
    }

    SequencedWriteStore indeterminateStore;
    RunPersistenceCoordinator seed(indeterminateStore,
                                   device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    static_cast<void>(readyActiveRunWithSensorSelection(seed, 1027U));
    indeterminateStore.restart();
    RunPersistenceCoordinator coordinator(indeterminateStore,
                                          device_platform::StorageEpoch(1U),
                                          RunCheckpointSchedule{});
    const auto loaded = coordinator.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    indeterminateStore.unknownWithoutCommitAt(1U);
    indeterminateStore.readFaultAt(1U,
                                   SequencedWriteStore::ReadFault::ReadError);
    const auto outcome = coordinator.activateLoadedRun(
        *restored, RunCheckpointTime{700000U, std::nullopt},
        recoveryPlausibility(700000U, false));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::PersistenceIndeterminate),
        static_cast<int>(outcome.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(coordinator.state()));
}

void test_activate_fallback_recovered_run_replaces_damaged_current_slot() {
    SequencedWriteStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            seed.persistCommand(state, startDecision(state, 1005U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            seed.checkpointPeriodic(state,
                                    RunCheckpointTime{300100U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            seed.checkpointPeriodic(state,
                                    RunCheckpointTime{600200U, std::nullopt})
                .status));

    store.backing().injectCorruption(slotKey("rc0"), "damaged-current");
    store.restart();
    RunPersistenceCoordinator recovered(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = recovered.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::FallbackRecovered),
        static_cast<int>(loaded.status));
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto outcome = recovered.activateFallbackRecoveredRun(
        *restored, RunCheckpointTime{700000U, std::nullopt},
        recoveryPlausibility(700000U));

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(outcome.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(recovered.state()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::ReachingTarget),
        static_cast<int>(outcome.resultingState.processState.state));
    TEST_ASSERT_FALSE(outcome.resultingState.pendingRecoveryAnchor.has_value());

    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto afterRecovery = afterBoot.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(afterRecovery.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::ReachingTarget),
        static_cast<int>(afterRecovery.snapshot->processState.state));

    const auto recoveredCurrent =
        RunPersistenceCoordinatorTestAccess::currentReference(afterBoot);
    const auto recoveredFallback =
        RunPersistenceCoordinatorTestAccess::fallbackReference(afterBoot);
    TEST_ASSERT_NOT_EQUAL(recoveredCurrent.slot, recoveredFallback.slot);
    const auto recoveredCurrentKey =
        recoveredCurrent.slot == 0U ? slotKey("rc0") : slotKey("rc1");
    store.backing().injectCorruption(recoveredCurrentKey,
                                     "damaged-current-again");
    store.restart();

    RunPersistenceCoordinator repeated(store, device_platform::StorageEpoch(1U),
                                       RunCheckpointSchedule{});
    const auto repeatedLoaded = repeated.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::FallbackRecovered),
        static_cast<int>(repeatedLoaded.status));
    const auto repeatedRestored =
        restoreRunPersistenceSnapshot(*repeatedLoaded.snapshot);
    TEST_ASSERT_TRUE(repeatedRestored.has_value());
    const auto repeatedOutcome = repeated.activateFallbackRecoveredRun(
        *repeatedRestored, RunCheckpointTime{800000U, std::nullopt},
        recoveryPlausibility(800000U));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(repeatedOutcome.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(repeated.state()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::ReachingTarget),
        static_cast<int>(repeatedOutcome.resultingState.processState.state));

    store.restart();
    RunPersistenceCoordinator repeatedReboot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto repeatedCurrent = repeatedReboot.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(repeatedCurrent.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::ReachingTarget),
        static_cast<int>(repeatedCurrent.snapshot->processState.state));
    const auto finalFallback =
        RunPersistenceCoordinatorTestAccess::fallbackReference(repeatedReboot);
    TEST_ASSERT_EQUAL_UINT8(recoveredFallback.slot, finalFallback.slot);
    TEST_ASSERT_EQUAL_UINT64(recoveredFallback.checkpointRevision,
                             finalFallback.checkpointRevision);
    TEST_ASSERT_EQUAL_UINT32(recoveredFallback.payloadCrc,
                             finalFallback.payloadCrc);
}

void test_activate_fallback_run_persists_sensor_gate_rejection_as_fault() {
    SequencedWriteStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            seed.persistCommand(state, startDecision(state, 1016U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            seed.checkpointPeriodic(state,
                                    RunCheckpointTime{300200U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            seed.checkpointPeriodic(state,
                                    RunCheckpointTime{600200U, std::nullopt})
                .status));

    store.backing().injectCorruption(slotKey("rc0"), "damaged-current");
    store.restart();
    RunPersistenceCoordinator recovered(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = recovered.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::FallbackRecovered),
        static_cast<int>(loaded.status));
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto outcome = recovered.activateFallbackRecoveredRun(
        *restored, RunCheckpointTime{700000U, std::nullopt},
        recoveryPlausibility(700000U, false));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(outcome.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Fault),
        static_cast<int>(outcome.resultingState.processState.state));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(recovered.state()));

    TEST_ASSERT_FALSE(
        RunPersistenceCoordinatorTestAccess::fallbackReferenceOptional(
            recovered)
            .has_value());

    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto rebooted = afterBoot.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(rebooted.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Fault),
        static_cast<int>(rebooted.snapshot->processState.state));
    TEST_ASSERT_FALSE(
        RunPersistenceCoordinatorTestAccess::fallbackReferenceOptional(
            afterBoot)
            .has_value());

    const auto faultCurrent =
        RunPersistenceCoordinatorTestAccess::currentReference(afterBoot);
    const auto faultCurrentKey =
        faultCurrent.slot == 0U ? slotKey("rc0") : slotKey("rc1");
    store.backing().injectCorruption(faultCurrentKey, "damaged-fault-current");
    store.restart();
    RunPersistenceCoordinator failClosed(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto failedLoad = failClosed.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::NotReconstructible),
        static_cast<int>(failedLoad.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(failClosed.state()));
}

void test_fallback_completed_recovery_repairs_current_and_repeats() {
    SequencedWriteStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    auto state = readyActiveManualRunWithSensorSelection(seed, 1017U);
    state.processState.state = ProcessState::ManualHolding;
    state.processState.stateEnteredAtMillis = 100U;
    state.processState.targetReachStartedAtMillis = 0U;
    const auto completed = decideProcessTransition(
        state.processState, &*state.processRunSnapshot, ProcessSignals{},
        TransitionRequest{ProcessEvent::FinishHoldConfirmed, std::nullopt},
        200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            seed.persistTransition(state, completed,
                                   RunCheckpointTime{200U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::Completed),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            seed.checkpointPeriodic(state,
                                    RunCheckpointTime{300200U, std::nullopt})
                .status));

    store.backing().injectCorruption(slotKey("rc0"), "damaged-completed");
    store.restart();
    RunPersistenceCoordinator recovered(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = recovered.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::FallbackRecovered),
        static_cast<int>(loaded.status));
    const auto fallbackBefore =
        RunPersistenceCoordinatorTestAccess::fallbackReference(recovered);
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    const auto sequenceBefore = restored->processState.transitionSequence;
    const auto outcome = recovered.activateFallbackRecoveredRun(
        *restored, RunCheckpointTime{400U, std::nullopt},
        recoveryPlausibility(400U, false));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(outcome.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Completed),
        static_cast<int>(outcome.resultingState.processState.state));
    TEST_ASSERT_EQUAL_UINT32(
        sequenceBefore, outcome.resultingState.processState.transitionSequence);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(recovered.state()));

    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto currentLoad = afterBoot.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(currentLoad.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Completed),
        static_cast<int>(currentLoad.snapshot->processState.state));
    const auto currentReference =
        RunPersistenceCoordinatorTestAccess::currentReference(afterBoot);
    const auto currentKey =
        currentReference.slot == 0U ? slotKey("rc0") : slotKey("rc1");
    store.backing().injectCorruption(currentKey, "damaged-completed-again");
    store.restart();

    RunPersistenceCoordinator repeated(store, device_platform::StorageEpoch(1U),
                                       RunCheckpointSchedule{});
    const auto repeatedLoad = repeated.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::FallbackRecovered),
        static_cast<int>(repeatedLoad.status));
    const auto repeatedRestored =
        restoreRunPersistenceSnapshot(*repeatedLoad.snapshot);
    TEST_ASSERT_TRUE(repeatedRestored.has_value());
    const auto repeatedOutcome = repeated.activateFallbackRecoveredRun(
        *repeatedRestored, RunCheckpointTime{500U, std::nullopt},
        recoveryPlausibility(500U, false));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(repeatedOutcome.persistenceResult.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::Completed),
        static_cast<int>(repeatedOutcome.resultingState.processState.state));
    const auto finalFallback =
        RunPersistenceCoordinatorTestAccess::fallbackReference(repeated);
    TEST_ASSERT_EQUAL_UINT8(fallbackBefore.slot, finalFallback.slot);
    TEST_ASSERT_EQUAL_UINT64(fallbackBefore.checkpointRevision,
                             finalFallback.checkpointRevision);
    TEST_ASSERT_EQUAL_UINT32(fallbackBefore.payloadCrc,
                             finalFallback.payloadCrc);
}

void seed_completed_fallback(SequencedWriteStore& store, CommandId startId) {
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    auto state = readyActiveManualRunWithSensorSelection(seed, startId);
    state.processState.state = ProcessState::ManualHolding;
    state.processState.stateEnteredAtMillis = 100U;
    state.processState.targetReachStartedAtMillis = 0U;
    const auto completed = decideProcessTransition(
        state.processState, &*state.processRunSnapshot, ProcessSignals{},
        TransitionRequest{ProcessEvent::FinishHoldConfirmed, std::nullopt},
        200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            seed.persistTransition(state, completed,
                                   RunCheckpointTime{200U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::CheckpointWritten),
        static_cast<int>(
            seed.checkpointPeriodic(state,
                                    RunCheckpointTime{300200U, std::nullopt})
                .status));
    store.backing().injectCorruption(slotKey("rc0"), "damaged-completed");
}

void test_fallback_completed_storage_recovery_cutpoints_remain_fail_closed() {
    using Fault = SequencedWriteStore::WriteFault;
    for (const auto offset : {1U, 2U, 3U}) {
        SequencedWriteStore store;
        seed_completed_fallback(store, 1022U + offset);
        store.restart();

        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        const auto loaded = coordinator.loadAndInitialize();
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceLoadStatus::FallbackRecovered),
            static_cast<int>(loaded.status));
        const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
        TEST_ASSERT_TRUE(restored.has_value());
        const auto writeNumber = store.writeCount() + offset;
        store.faultAt(writeNumber, Fault::FailBeforeBegin);
        const auto outcome = coordinator.activateFallbackRecoveredRun(
            *restored, RunCheckpointTime{400U, std::nullopt},
            recoveryPlausibility(400U, false));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::WriteFailed),
            static_cast<int>(outcome.persistenceResult.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                offset == 1U
                    ? RunPersistenceCoordinatorState::FallbackRecoveryPending
                    : RunPersistenceCoordinatorState::BlockedIndeterminate),
            static_cast<int>(coordinator.state()));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(offset == 1U ? RunPersistenceDurability::Unchanged
                                          : RunPersistenceDurability::Changed),
            static_cast<int>(outcome.persistenceResult.durability));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(offset == 1U ? RunPersistenceStep::PreparedHead
                             : offset == 2U
                                 ? RunPersistenceStep::CheckpointSlot
                                 : RunPersistenceStep::CommittedHead),
            static_cast<int>(outcome.persistenceResult.step));
    }

    for (const auto offset : {1U, 2U, 3U}) {
        SequencedWriteStore store;
        seed_completed_fallback(store, 1030U + offset);
        store.restart();

        RunPersistenceCoordinator coordinator(
            store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
        const auto loaded = coordinator.loadAndInitialize();
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceLoadStatus::FallbackRecovered),
            static_cast<int>(loaded.status));
        const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
        TEST_ASSERT_TRUE(restored.has_value());
        const auto writeNumber = store.writeCount() + offset;
        store.unknownWithoutCommitAt(writeNumber);
        store.readFaultAt(writeNumber,
                          SequencedWriteStore::ReadFault::ReadError);
        const auto outcome = coordinator.activateFallbackRecoveredRun(
            *restored, RunCheckpointTime{400U, std::nullopt},
            recoveryPlausibility(400U, false));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                RunPersistenceResultStatus::PersistenceIndeterminate),
            static_cast<int>(outcome.persistenceResult.status));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                RunPersistenceCoordinatorState::BlockedIndeterminate),
            static_cast<int>(coordinator.state()));
    }
}

RunCommandState readyActiveManualRunWithSensorSelection(
    RunPersistenceCoordinator& coordinator, CommandId startId) {
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, manualStartDecision(state, startId),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    TEST_ASSERT_TRUE(state.sensorSelection.has_value());
    TEST_ASSERT_TRUE(
        state.sensorSelection ==
        (PersistedSensorSelectionState{
            SensorSelectionProvenance::InitialSelection,
            SensorSelectionDecisionCause::StartSelection, state.runRevision}));
    return state;
}

SensorSelectionStateMutation modeChangeMutation(
    const RunCommandState& state, RunSensorMode newMode,
    SensorSelectionDecisionCause cause, std::uint64_t nowMonotonicMillis) {
    SensorSelectionStateMutation mutation;
    mutation.status = SensorSelectionApplyStatus::AppliedPersistentCandidate;
    mutation.runtime.phase = SensorSelectionPhase::NormalProduct;
    mutation.runtime.permission = SensorPeltierPermission::Allowed;
    mutation.runtime.lastAppliedMonotonicMillis = nowMonotonicMillis;
    mutation.activeMode = newMode;
    mutation.resultingRunRevision = state.runRevision + 1U;
    mutation.persisted = PersistedSensorSelectionState{
        SensorSelectionProvenance::InitialSelection, cause,
        mutation.resultingRunRevision};
    mutation.event = SensorSelectionEvent{*state.activeRunSensorMode,
                                          newMode,
                                          cause,
                                          mutation.resultingRunRevision,
                                          nowMonotonicMillis,
                                          std::nullopt};
    return mutation;
}

void test_effective_sensor_role_change_controls_pi_handoff() {
    TEST_ASSERT_FALSE(resolveControlSensorRoleTransition(
        ProcessState::Preheating, RunSensorMode::Product,
        ProcessState::Preheating, RunSensorMode::Air));
    TEST_ASSERT_FALSE(resolveControlSensorRoleTransition(
        ProcessState::WaitingForProduct, RunSensorMode::Product,
        ProcessState::WaitingForProduct, RunSensorMode::Air));
    TEST_ASSERT_TRUE(resolveControlSensorRoleTransition(
        ProcessState::ReachingTarget, RunSensorMode::Product,
        ProcessState::ReachingTarget, RunSensorMode::Air));
    TEST_ASSERT_TRUE(resolveControlSensorRoleTransition(
        ProcessState::QualifyingTarget, RunSensorMode::Air,
        ProcessState::QualifyingTarget, RunSensorMode::Product));

    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(24U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    TargetQualificationEvaluator evaluator;
    TemperatureController controller({}, bridgeTemperaturePolicy());
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);

    RunCommandState preheating;
    preheating.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            application
                .persistCommand(
                    preheating,
                    startDecision(preheating, 1260U, 100U, preheatProgram(),
                                  RunSensorMode::Product),
                    RunCheckpointTime{100U, std::nullopt})
                .status));
    auto preheatMutation =
        modeChangeMutation(preheating, RunSensorMode::Air,
                           SensorSelectionDecisionCause::FallbackToAir, 200U);
    preheatMutation.runtime.phase = SensorSelectionPhase::NormalAir;
    const auto preheatResult = application.persistSensorSelection(
        preheating, preheatMutation, RunCheckpointTime{200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(preheatResult.status));
    TEST_ASSERT_FALSE(controller.state().pendingContextTransition.has_value());

    SequencedWriteStore waitingStore;
    RunPersistenceCoordinator waitingCoordinator(
        waitingStore, device_platform::StorageEpoch(25U),
        RunCheckpointSchedule{});
    static_cast<void>(waitingCoordinator.loadAndInitialize());
    TargetQualificationEvaluator waitingEvaluator;
    TemperatureController waitingController({}, bridgeTemperaturePolicy());
    TemperatureControlApplicationOrchestrator waitingApplication(
        waitingCoordinator, waitingController, waitingEvaluator);
    auto waiting = reachDurablyWaitingForProduct(waitingCoordinator, 1261U,
                                                 RunSensorMode::Product);
    auto waitingMutation = modeChangeMutation(
        waiting, RunSensorMode::Air,
        SensorSelectionDecisionCause::FallbackToAir, 600200U);
    waitingMutation.runtime.phase = SensorSelectionPhase::NormalAir;
    const auto waitingResult = waitingApplication.persistSensorSelection(
        waiting, waitingMutation, RunCheckpointTime{600200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(waitingResult.status));
    TEST_ASSERT_FALSE(
        waitingController.state().pendingContextTransition.has_value());

    SequencedWriteStore manualStore;
    RunPersistenceCoordinator manualCoordinator(
        manualStore, device_platform::StorageEpoch(26U),
        RunCheckpointSchedule{});
    static_cast<void>(manualCoordinator.loadAndInitialize());
    TargetQualificationEvaluator manualEvaluator;
    TemperatureController manualController({}, bridgeTemperaturePolicy());
    TemperatureControlApplicationOrchestrator manualApplication(
        manualCoordinator, manualController, manualEvaluator);
    auto manual =
        readyActiveManualRunWithSensorSelection(manualCoordinator, 1262U);
    manual.activeRunSensorMode = RunSensorMode::Product;
    manual.activeManualRun->values.sensorMode = RunSensorMode::Product;
    manual.sensorSelectionRuntime.phase =
        SensorSelectionPhase::UserDecisionRequired;
    manual.sensorSelectionRuntime.permission = SensorPeltierPermission::Blocked;
    const auto manualDecision = continueWithAirDecision(manual, 1263U, 700U);
    TEST_ASSERT_TRUE(manualDecision.proposed());
    const auto manualResult = manualApplication.persistCommand(
        manual, manualDecision, RunCheckpointTime{700U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(manualResult.status));
    TEST_ASSERT_TRUE(
        manualController.state().pendingContextTransition.has_value());
    TEST_ASSERT_TRUE(*manualController.state().pendingContextTransition ==
                     CommittedControlContextTransition::SensorRoleChange);
}

void test_qualifying_sensor_role_change_resets_state_marker_and_qualifier_atomically() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(25U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    TargetQualificationEvaluator evaluator;
    TemperatureController controller({}, bridgeTemperaturePolicy());
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);
    auto state = readyActiveRunWithSensorSelection(coordinator, 1270U);
    state.processState.state = ProcessState::QualifyingTarget;
    state.processState.stateEnteredAtMillis = 100U;
    state.processState.targetReachStartedAtMillis = 100U;
    state.processState.qualificationValidSinceMillis = 150U;
    buildQualifierCredit(evaluator);
    const auto beforeState = state.processState;
    const auto beforeEvaluator = evaluator.state();
    auto mutation =
        modeChangeMutation(state, RunSensorMode::Air,
                           SensorSelectionDecisionCause::FallbackToAir, 200U);
    mutation.runtime.phase = SensorSelectionPhase::NormalAir;

    store.faultAt(store.writeCount() + 1U,
                  SequencedWriteStore::WriteFault::FailBeforeBegin);
    const auto failed = application.persistSensorSelection(
        state, mutation, RunCheckpointTime{200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(failed.status));
    TEST_ASSERT_TRUE(equalProcessRuntimeState(state.processState, beforeState));
    TEST_ASSERT_TRUE(evaluator.state().creditedInBandMillis ==
                     beforeEvaluator.creditedInBandMillis);
    TEST_ASSERT_FALSE(controller.state().pendingContextTransition.has_value());

    store.clearFaults();
    const auto committed = application.persistSensorSelection(
        state, mutation, RunCheckpointTime{200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(committed.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.processState.state));
    TEST_ASSERT_FALSE(
        state.processState.qualificationValidSinceMillis.has_value());
    TEST_ASSERT_EQUAL_UINT64(0U, evaluator.state().creditedInBandMillis);
    TEST_ASSERT_FALSE(evaluator.state().episodeActive);
    TEST_ASSERT_TRUE(controller.state().pendingContextTransition.has_value());
    TEST_ASSERT_TRUE(*controller.state().pendingContextTransition ==
                     CommittedControlContextTransition::SensorRoleChange);
}

void test_weighted_progress_keeps_cumulative_confidence_conservative() {
    SequencedWriteStore store;
    RunPersistenceCoordinator seed(store, device_platform::StorageEpoch(1U),
                                   RunCheckpointSchedule{});
    static_cast<void>(seed.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(seed, 1035U);
    ProcessSignals signals;
    signals.qualificationProgress = QualificationProgress::Complete;
    auto transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 200U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            seed.persistTransition(state, transition,
                                   RunCheckpointTime{200U, 1700000200})
                .status));
    transition =
        decideProcessTransition(state.processState, &*state.processRunSnapshot,
                                signals, TransitionRequest{}, 600300U);
    const auto evidence = recoveryPlausibility(600300U);
    state.recoveryTemperatureEvidence.lastKnown = CrossRoleEvidence{
        {evidence.air.filteredCelsius, evidence.air.quality},
        {evidence.product.filteredCelsius, evidence.product.quality},
        {evidence.cooling.filteredCelsius, evidence.cooling.quality}};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            seed.persistTransition(state, transition,
                                   RunCheckpointTime{600300U, std::nullopt},
                                   &evidence)
                .status));

    store.restart();
    RunPersistenceCoordinator persistence(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = persistence.loadAndInitialize();
    const auto restored = restoreRunPersistenceSnapshot(*loaded.snapshot);
    TEST_ASSERT_TRUE(restored.has_value());
    auto activation = persistence.activateLoadedRun(
        *restored, RunCheckpointTime{700000U, std::nullopt},
        recoveryPlausibility(700000U));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(activation.persistenceResult.status));
    auto current = activation.resultingState;
    TEST_ASSERT_TRUE(current.lastRecoveryEpisodeEvidence.has_value());
    const auto initialSegment =
        *current.lastRecoveryEpisodeEvidence->weightedProgressSegmentId;
    RecoveryProgressWeightingInput input;
    input.phase = ProcessState::Fermenting;
    input.outage = RecoveryOutageBounds{200U, 100U};
    input.usableSensorRole = RunSensorMode::Product;
    FixedRecoveryProgressModel model;
    RunRecoveryCoordinator recovery(persistence);

    const auto productOne = recovery.applyRecoveryProgressWeighting(
        current, current.runRevision, current.recoveryEpisodeRevision,
        initialSegment, RunCheckpointTime{700100U, std::nullopt}, input, model);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(productOne.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WeightedProgressConfidence::ProductPreferred),
        static_cast<int>(
            current.runProgress.weightedProgress->lastApplied->confidence));

    auto nextSegment = [&]() {
        ++current.lastRecoveryEpisodeEvidence->weightedProgressSegmentId
              .value();
        return *current.lastRecoveryEpisodeEvidence->weightedProgressSegmentId;
    };
    const auto productTwo = recovery.applyRecoveryProgressWeighting(
        current, current.runRevision, current.recoveryEpisodeRevision,
        nextSegment(), RunCheckpointTime{700200U, std::nullopt}, input, model);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(productTwo.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WeightedProgressConfidence::ProductPreferred),
        static_cast<int>(
            current.runProgress.weightedProgress->lastApplied->confidence));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            persistence
                .persistSensorSelection(
                    current,
                    modeChangeMutation(
                        current, RunSensorMode::Air,
                        SensorSelectionDecisionCause::FallbackToAir, 700300U),
                    RunCheckpointTime{700300U, std::nullopt})
                .status));
    model.sourceRole = RunSensorMode::Air;
    model.confidence = WeightedProgressConfidence::AirReduced;
    input.usableSensorRole = RunSensorMode::Air;
    const auto airOne = recovery.applyRecoveryProgressWeighting(
        current, current.runRevision, current.recoveryEpisodeRevision,
        nextSegment(), RunCheckpointTime{700400U, std::nullopt}, input, model);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(airOne.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunSensorMode::Air),
        static_cast<int>(
            current.runProgress.weightedProgress->lastApplied->lastSourceRole));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WeightedProgressConfidence::AirReduced),
        static_cast<int>(
            current.runProgress.weightedProgress->lastApplied->confidence));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            persistence
                .persistSensorSelection(
                    current,
                    modeChangeMutation(
                        current, RunSensorMode::Product,
                        SensorSelectionDecisionCause::AutomaticValidatedReturn,
                        700500U),
                    RunCheckpointTime{700500U, std::nullopt})
                .status));
    model.sourceRole = RunSensorMode::Product;
    model.confidence = WeightedProgressConfidence::ProductPreferred;
    input.usableSensorRole = RunSensorMode::Product;
    const auto productAfterAir = recovery.applyRecoveryProgressWeighting(
        current, current.runRevision, current.recoveryEpisodeRevision,
        nextSegment(), RunCheckpointTime{700600U, std::nullopt}, input, model);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(productAfterAir.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunSensorMode::Product),
        static_cast<int>(
            current.runProgress.weightedProgress->lastApplied->lastSourceRole));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WeightedProgressConfidence::AirReduced),
        static_cast<int>(
            current.runProgress.weightedProgress->lastApplied->confidence));

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            persistence
                .persistSensorSelection(
                    current,
                    modeChangeMutation(
                        current, RunSensorMode::Air,
                        SensorSelectionDecisionCause::FallbackToAir, 700700U),
                    RunCheckpointTime{700700U, std::nullopt})
                .status));
    model.sourceRole = RunSensorMode::Air;
    model.confidence = WeightedProgressConfidence::AirReduced;
    input.usableSensorRole = RunSensorMode::Air;
    const auto airAfterAir = recovery.applyRecoveryProgressWeighting(
        current, current.runRevision, current.recoveryEpisodeRevision,
        nextSegment(), RunCheckpointTime{700800U, std::nullopt}, input, model);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(airAfterAir.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(WeightedProgressConfidence::AirReduced),
        static_cast<int>(
            current.runProgress.weightedProgress->lastApplied->confidence));
}

SensorSelectionStateMutation recoveryRevalidationMutation(
    const RunCommandState& state, std::uint64_t nowMonotonicMillis) {
    SensorSelectionStateMutation mutation;
    mutation.status = SensorSelectionApplyStatus::AppliedPersistentCandidate;
    mutation.runtime.phase = SensorSelectionPhase::NormalProduct;
    mutation.runtime.permission = SensorPeltierPermission::Allowed;
    mutation.runtime.lastAppliedMonotonicMillis = nowMonotonicMillis;
    mutation.activeMode = *state.activeRunSensorMode;
    mutation.resultingRunRevision = state.runRevision + 1U;
    mutation.persisted = PersistedSensorSelectionState{
        SensorSelectionProvenance::InitialSelection,
        SensorSelectionDecisionCause::RecoveryRevalidation,
        mutation.resultingRunRevision};
    mutation.notice = SensorSelectionNotice{
        SensorSelectionDecisionCause::RecoveryRevalidation, nowMonotonicMillis,
        mutation.resultingRunRevision, *state.activeRunSensorMode,
        SensorSelectionBlockReason::None};
    return mutation;
}

SensorSelectionStateMutation productFailureBlockMutation(
    const RunCommandState& state, std::uint64_t nowMonotonicMillis) {
    SensorSelectionStateMutation mutation;
    mutation.status = SensorSelectionApplyStatus::AppliedPersistentCandidate;
    mutation.runtime.phase = SensorSelectionPhase::ProductFailureDetected;
    mutation.runtime.permission = SensorPeltierPermission::Blocked;
    mutation.runtime.fallbackWaitStartedAtMonotonicMillis = nowMonotonicMillis;
    mutation.runtime.lastAppliedMonotonicMillis = nowMonotonicMillis;
    mutation.activeMode = *state.activeRunSensorMode;
    mutation.resultingRunRevision = state.runRevision + 1U;
    mutation.persisted = PersistedSensorSelectionState{
        SensorSelectionProvenance::InitialSelection,
        SensorSelectionDecisionCause::ProductFailureBlock,
        mutation.resultingRunRevision};
    mutation.notice = SensorSelectionNotice{
        SensorSelectionDecisionCause::ProductFailureBlock, nowMonotonicMillis,
        mutation.resultingRunRevision, *state.activeRunSensorMode,
        SensorSelectionBlockReason::ProductSensorUnusable};
    return mutation;
}

void test_persist_sensor_selection_writes_schema_two_and_reports_permission_blocked() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 900U);
    const auto mutation = productFailureBlockMutation(state, 500U);

    const auto result = coordinator.persistSensorSelection(
        state, mutation, RunCheckpointTime{500U, 1700000500});

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT32(1U, result.effectCount);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandEffect::SensorSelectionPermissionBlocked),
        static_cast<int>(result.effects[0]));
    TEST_ASSERT_FALSE(result.sensorSelectionEvent.has_value());
    TEST_ASSERT_TRUE(result.sensorSelectionNotice.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionDecisionCause::ProductFailureBlock),
        static_cast<int>(result.sensorSelectionNotice->cause));
    TEST_ASSERT_TRUE(state.sensorSelection.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionDecisionCause::ProductFailureBlock),
        static_cast<int>(state.sensorSelection->lastDecisionCause));
    TEST_ASSERT_EQUAL_UINT32(mutation.resultingRunRevision, state.runRevision);
    // Korrekturauftrag Befund 1, Pflichttest "Kernentscheidung ->
    // persistSensorSelection -> aktueller RAM-Zustand": vor der Korrektur
    // uebernahm persistSensorSelection mutation.runtime nie in current -
    // sensorSelectionRuntime blieb auf dem alten Wert stehen.
    TEST_ASSERT_TRUE(state.sensorSelectionRuntime == mutation.runtime);

    const auto reference = coordinator.state();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::Ready),
        static_cast<int>(reference));

    const auto currentSlotRead = store.read(slotKey("rc1"), 8240U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(currentSlotRead.status));
    const auto envelope =
        device_platform::decodeEnvelope(currentSlotRead.value);
    TEST_ASSERT_TRUE(envelope.envelope.has_value());
    TEST_ASSERT_EQUAL_UINT32(kCurrentRunPersistenceSchema,
                             envelope.envelope->schemaVersion);
    const auto decoded = decodeRunPersistenceSnapshot(
        envelope.envelope->payload, envelope.envelope->schemaVersion);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(decoded.status));
    TEST_ASSERT_TRUE(decoded.snapshot->sensorSelection.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionDecisionCause::ProductFailureBlock),
        static_cast<int>(decoded.snapshot->sensorSelection->lastDecisionCause));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunCheckpointTrigger::SensorSelection),
        static_cast<int>(decoded.snapshot->trigger));
}

void test_persist_sensor_selection_mode_change_fills_event_and_updates_mode() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 905U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunSensorMode::Product),
                          static_cast<int>(*state.activeRunSensorMode));
    const auto mutation =
        modeChangeMutation(state, RunSensorMode::Air,
                           SensorSelectionDecisionCause::FallbackToAir, 500U);

    TargetQualificationEvaluator evaluator;
    TemperatureController controller({}, {});
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);
    store.faultAt(store.writeCount() + 1U,
                  SequencedWriteStore::WriteFault::FailBeforeBegin);
    const auto failed = application.persistSensorSelection(
        state, mutation, RunCheckpointTime{500U, 1700000600});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(failed.status));
    TEST_ASSERT_FALSE(controller.state().pendingContextTransition.has_value());
    store.clearFaults();
    const auto result = application.persistSensorSelection(
        state, mutation, RunCheckpointTime{500U, 1700000600});

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_TRUE(result.sensorSelectionEvent.has_value());
    TEST_ASSERT_FALSE(result.sensorSelectionNotice.has_value());
    TEST_ASSERT_FALSE(result.committedControlContextTransition.has_value());
    TEST_ASSERT_TRUE(controller.state().pendingContextTransition.has_value());
    TEST_ASSERT_TRUE(*controller.state().pendingContextTransition ==
                     CommittedControlContextTransition::SensorRoleChange);
    TEST_ASSERT_TRUE(result.sensorSelectionEvent->utcUnixSeconds.has_value());
    TEST_ASSERT_EQUAL_INT64(1700000600,
                            *result.sensorSelectionEvent->utcUnixSeconds);
    TEST_ASSERT_TRUE(state.activeRunSensorMode.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunSensorMode::Air),
                          static_cast<int>(*state.activeRunSensorMode));
}

void test_persist_sensor_selection_mode_change_on_manual_run() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveManualRunWithSensorSelection(coordinator, 906U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunSensorMode::Air),
                          static_cast<int>(*state.activeRunSensorMode));
    const auto mutation = modeChangeMutation(
        state, RunSensorMode::Product,
        SensorSelectionDecisionCause::AutomaticValidatedReturn, 500U);

    TargetQualificationEvaluator evaluator;
    TemperatureController controller({}, {});
    TemperatureControlApplicationOrchestrator application(
        coordinator, controller, evaluator);
    store.faultAt(store.writeCount() + 1U,
                  SequencedWriteStore::WriteFault::FailBeforeBegin);
    const auto failed = application.persistSensorSelection(
        state, mutation, RunCheckpointTime{500U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(failed.status));
    TEST_ASSERT_FALSE(controller.state().pendingContextTransition.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunSensorMode::Air),
                          static_cast<int>(*state.activeRunSensorMode));
    store.clearFaults();
    auto result = application.persistSensorSelection(
        state, mutation, RunCheckpointTime{500U, std::nullopt});

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_TRUE(state.activeRunSensorMode.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunSensorMode::Product),
                          static_cast<int>(*state.activeRunSensorMode));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunSensorMode::Product),
        static_cast<int>(state.activeManualRun->values.sensorMode));
    TEST_ASSERT_FALSE(result.committedControlContextTransition.has_value());
    TEST_ASSERT_TRUE(controller.state().pendingContextTransition.has_value());
    TEST_ASSERT_TRUE(*controller.state().pendingContextTransition ==
                     CommittedControlContextTransition::SensorRoleChange);
}

void test_persist_sensor_selection_recovery_revalidation_restores_permission() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 907U);
    // Korrekturauftrag Befund 1: der Effekt wird jetzt aus dem tatsaechlichen
    // Before/After-Permission-Uebergang abgeleitet statt aus der Ursache -
    // die Fixture muss die "vorher Blocked" Ausgangslage einer echten
    // Wiederherstellung deshalb explizit abbilden.
    state.sensorSelectionRuntime.permission = SensorPeltierPermission::Blocked;
    const auto mutation = recoveryRevalidationMutation(state, 500U);

    const auto result = coordinator.persistSensorSelection(
        state, mutation, RunCheckpointTime{500U, std::nullopt});

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT32(1U, result.effectCount);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CommandEffect::SensorSelectionPermissionRestored),
        static_cast<int>(result.effects[0]));
    TEST_ASSERT_TRUE(result.sensorSelectionNotice.has_value());
    TEST_ASSERT_FALSE(result.sensorSelectionEvent.has_value());
}

void test_persist_sensor_selection_from_ready_empty_reports_no_active_run() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = coordinator.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::NoPersistedRun),
        static_cast<int>(loaded.status));
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    SensorSelectionStateMutation mutation;
    mutation.status = SensorSelectionApplyStatus::AppliedPersistentCandidate;

    const auto result = coordinator.persistSensorSelection(
        state, mutation, RunCheckpointTime{100U, std::nullopt});

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NoActiveRun),
        static_cast<int>(result.status));
    TEST_ASSERT_FALSE(state.sensorSelection.has_value());
}

// Korrekturauftrag Befund 1, Pflichttest "keine Wiederholung desselben
// automatischen Writes": die zweite Anwendung derselben, bereits
// verarbeiteten Mutation auf denselben Coordinator wird durch die
// Revisionsfolgepruefung als StaleDecision abgelehnt statt den Write zu
// wiederholen - `mutation.resultingRunRevision` passt nach dem ersten Write
// nicht mehr zu `current.runRevision + 1`.
void test_persist_sensor_selection_same_mutation_is_not_replayed() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 908U);
    const auto mutation = productFailureBlockMutation(state, 500U);

    const auto first = coordinator.persistSensorSelection(
        state, mutation, RunCheckpointTime{500U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(first.status));
    const auto writesAfterFirst = store.writeCount();

    const auto second = coordinator.persistSensorSelection(
        state, mutation, RunCheckpointTime{500U, std::nullopt});

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::StaleDecision),
        static_cast<int>(second.status));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesAfterFirst),
                           static_cast<unsigned>(store.writeCount()));
}

void test_persist_sensor_selection_requires_active_run_fields() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::Applied),
        static_cast<int>(
            coordinator
                .persistCommand(state, startDecision(state, 901U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    // #21 Commit 5: decideProgramStart now always populates sensorSelection,
    // so this eligibility gate is exercised directly against a state that
    // deliberately lacks it (e.g. a not-yet-#18-reactivated restore) rather
    // than relying on it being absent by default.
    state.sensorSelection.reset();
    const auto mutation = productFailureBlockMutation(state, 500U);
    const auto writesBefore = store.writeCount();

    const auto result = coordinator.persistSensorSelection(
        state, mutation, RunCheckpointTime{500U, std::nullopt});

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NotEligible),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBefore),
                           static_cast<unsigned>(store.writeCount()));
}

void test_persist_sensor_selection_from_loaded_active_run_stays_recovery_pending() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 902U);
    store.restart();
    RunPersistenceCoordinator afterBoot(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto loaded = afterBoot.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(loaded.status));
    const auto writesBefore = store.writeCount();
    const auto mutation = productFailureBlockMutation(state, 500U);

    const auto result = afterBoot.persistSensorSelection(
        state, mutation, RunCheckpointTime{500U, std::nullopt});

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::RecoveryPending),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBefore),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::LoadedActiveRun),
        static_cast<int>(afterBoot.state()));
}

void test_persist_sensor_selection_rejects_manual_causes() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 903U);
    auto mutation = productFailureBlockMutation(state, 500U);
    mutation.persisted->lastDecisionCause =
        SensorSelectionDecisionCause::ManualUserFallback;
    const auto writesBefore = store.writeCount();

    const auto result = coordinator.persistSensorSelection(
        state, mutation, RunCheckpointTime{500U, std::nullopt});

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::NotEligible),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBefore),
                           static_cast<unsigned>(store.writeCount()));
}

void test_persist_sensor_selection_rejects_non_persistent_status() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 904U);
    const auto writesBefore = store.writeCount();
    for (const auto status : {SensorSelectionApplyStatus::NoChange,
                              SensorSelectionApplyStatus::AppliedRamOnly}) {
        auto mutation = productFailureBlockMutation(state, 500U);
        mutation.status = status;

        const auto result = coordinator.persistSensorSelection(
            state, mutation, RunCheckpointTime{500U, std::nullopt});

        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(RunPersistenceResultStatus::InvalidDecision),
            static_cast<int>(result.status));
    }
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBefore),
                           static_cast<unsigned>(store.writeCount()));
}

// ---------------------------------------------------------------------------
// PR-#99-Abschlussreview-Korrektur, Plan 9.7: manueller Transportvertrag -
// AppliedPersistentCandidate -> persistCommand. Die obigen
// test_persist_sensor_selection_*-Faelle pruefen ausschliesslich den
// automatischen Pfad (RunPersistenceCoordinator::persistSensorSelection);
// bis hierhin gab es keinen Coordinator-Integrationstest fuer den manuellen
// Pfad (decideApplySensorSelectionAction -> persistCommand). Die folgenden
// beiden Tests schliessen diese Luecke direkt am Coordinator statt nur auf
// RunCommandState-Ebene (wie in test_run_commands.cpp).
// ---------------------------------------------------------------------------

device_platform::SensorQualitySnapshot coordinatorValidSensorSnapshot() {
    device_platform::SensorQualitySnapshot snapshot;
    snapshot.quality = device_platform::SensorQuality::Valid;
    return snapshot;
}

device_platform::SensorQualitySnapshot coordinatorFailedSensorSnapshot() {
    device_platform::SensorQualitySnapshot snapshot;
    snapshot.quality = device_platform::SensorQuality::Failed;
    snapshot.lastFaultReason =
        device_platform::SensorFaultReason::MissingSample;
    return snapshot;
}

CommandDecision continueWithAirDecision(const RunCommandState& state,
                                        CommandId id,
                                        std::uint64_t monotonicMillis) {
    SensorSelectionCommandRequest request;
    request.envelope = {id,
                        CommandSource::LocalDisplay,
                        monotonicMillis,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true,
                        std::nullopt};
    request.action = SensorSelectionUserAction::ContinueWithAir;
    request.safetyAllowsChange = true;
    CrossRolePlausibilityContext plausibility;
    plausibility.air = coordinatorValidSensorSnapshot();
    plausibility.product = coordinatorFailedSensorSnapshot();
    plausibility.cooling = coordinatorValidSensorSnapshot();
    plausibility.evaluationMonotonicMillis = monotonicMillis;
    return decideApplySensorSelectionAction(state, request, plausibility);
}

void test_persist_command_applies_and_writes_manual_continue_with_air() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 910U);
    // Vorbedingung fuer ContinueWithAir: Produkt ausgefallen, Bediener wird
    // um Entscheidung gebeten, Permission gesperrt.
    state.sensorSelectionRuntime.phase =
        SensorSelectionPhase::UserDecisionRequired;
    state.sensorSelectionRuntime.permission = SensorPeltierPermission::Blocked;

    const auto decision = continueWithAirDecision(state, 911U, 600U);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(decision.sensorSelectionApplyStatus.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            SensorSelectionApplyStatus::AppliedPersistentCandidate),
        static_cast<int>(*decision.sensorSelectionApplyStatus));

    const auto writesBefore = store.writeCount();
    const auto result = coordinator.persistCommand(
        state, decision, RunCheckpointTime{600U, std::nullopt});

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_TRUE(store.writeCount() > writesBefore);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionPhase::AirFallbackActive),
        static_cast<int>(state.sensorSelectionRuntime.phase));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorPeltierPermission::Allowed),
        static_cast<int>(state.sensorSelectionRuntime.permission));
    TEST_ASSERT_TRUE(state.activeRunSensorMode.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunSensorMode::Air),
                          static_cast<int>(*state.activeRunSensorMode));

    // Ueberlebt einen Neustart - Beweis fuer den tatsaechlichen Store-Write,
    // nicht nur die RAM-Mutation.
    RunPersistenceCoordinator restarted(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto afterBoot = restarted.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(afterBoot.status));
    TEST_ASSERT_TRUE(afterBoot.snapshot.has_value());
    TEST_ASSERT_TRUE(afterBoot.snapshot->sensorSelection.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionDecisionCause::ManualUserFallback),
        static_cast<int>(
            afterBoot.snapshot->sensorSelection->lastDecisionCause));
}

// PR-#99-letzter-Abschlussblocker: RecheckProduct in AirFallbackActive mit
// unvollstaendiger/veralteter thermischer Evidenz ist der einzige manuelle
// Pfad, der AppliedRamOnly erreicht (test_recheck_product_from_air_fallback_
// with_incomplete_evidence_is_ram_only in test_sensor_selection.cpp deckt
// das auf reiner Kernfunktionsebene ab). Dieser Coordinator-Integrationstest
// belegt den korrigierten Transportvertrag (Plan 9.7): AppliedRamOnly wird
// stale-geprueft genau einmal nur im RAM angewendet - kein Store-Write,
// keine persistierte CommandId, keine Laufrevisionserhoehung; dieselbe
// CommandId bleibt innerhalb desselben Boots fluechtig idempotent
// (AlreadyProcessed statt AlreadyPersisted) und ueberlebt keinen Neustart.
void test_persist_command_manual_recheck_product_ram_only_is_ram_only_and_idempotent() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 920U);
    state.sensorSelectionRuntime.phase =
        SensorSelectionPhase::AirFallbackActive;
    state.sensorSelectionRuntime.permission = SensorPeltierPermission::Allowed;
    state.activeRunSensorMode = RunSensorMode::Air;
    if (state.activeManualRun.has_value()) {
        state.activeManualRun->values.sensorMode = RunSensorMode::Air;
    }

    SensorSelectionCommandRequest request;
    request.envelope = {921U,
                        CommandSource::LocalDisplay,
                        700U,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true,
                        std::nullopt};
    request.action = SensorSelectionUserAction::RecheckProduct;
    request.safetyAllowsChange = true;
    CrossRolePlausibilityContext plausibility;
    plausibility.air = coordinatorValidSensorSnapshot();
    plausibility.product = coordinatorValidSensorSnapshot();
    plausibility.cooling = coordinatorValidSensorSnapshot();
    plausibility.evaluationMonotonicMillis = 700U;
    plausibility.thermalCompatibility.status = ThermalCompatibility::Stale;
    plausibility.thermalCompatibility.profileRevision = 9U;

    const auto decision =
        decideApplySensorSelectionAction(state, request, plausibility);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(decision.sensorSelectionApplyStatus.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionApplyStatus::AppliedRamOnly),
        static_cast<int>(*decision.sensorSelectionApplyStatus));

    const auto sensorSelectionBefore = state.sensorSelection;
    const auto runRevisionBefore = state.runRevision;
    const auto writesBefore = store.writeCount();

    const auto result = coordinator.persistCommand(
        state, decision, RunCheckpointTime{700U, std::nullopt});

    // Genau einmal im RAM angewendet, kein Store-Write.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceDurability::Unchanged),
                          static_cast<int>(result.durability));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBefore),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionPhase::ReturnValidationPending),
        static_cast<int>(state.sensorSelectionRuntime.phase));

    // Fachinhalt (persistierbares Feld und Laufrevision) bleibt unveraendert
    // - AppliedRamOnly aendert per Definition weder sensorSelection noch
    // runRevision.
    TEST_ASSERT_TRUE(state.sensorSelection == sensorSelectionBefore);
    TEST_ASSERT_EQUAL_UINT32(runRevisionBefore, state.runRevision);

    // Dieselbe CommandId bleibt innerhalb desselben Boots fluechtig
    // idempotent: der zweite Versuch mit identischer Entscheidung liefert
    // AlreadyProcessed (RAM-only, ueber RunCommandState::
    // processedCommandIds), nicht AlreadyPersisted (das wuerde einen
    // Eintrag in der dauerhaften persistedIds_-Liste voraussetzen, die hier
    // nie beruehrt wurde).
    const auto replay = coordinator.persistCommand(
        state, decision, RunCheckpointTime{700U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::AlreadyProcessed),
        static_cast<int>(replay.status));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBefore),
                           static_cast<unsigned>(store.writeCount()));

    // Nach einem Neustart gilt die CommandId nicht als dauerhaft
    // persistiert: die zuletzt tatsaechlich geschriebene Kommando-CommandId
    // ist weiterhin die des Laufstarts (920U aus
    // readyActiveRunWithSensorSelection), nicht die des RAM-only-Versuchs
    // (921U) - und der laufzeitseitige Zustand ist wie geplant fail-closed
    // verworfen (RestartRevalidationPending/Blocked statt
    // ReturnValidationPending/Allowed).
    RunPersistenceCoordinator restarted(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    const auto afterBoot = restarted.loadAndInitialize();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceLoadStatus::Current),
                          static_cast<int>(afterBoot.status));
    TEST_ASSERT_TRUE(afterBoot.snapshot.has_value());
    TEST_ASSERT_EQUAL_UINT32(1U, afterBoot.snapshot->persistedRunCommandCount);
    TEST_ASSERT_EQUAL_UINT32(920U,
                             afterBoot.snapshot->persistedRunCommandIds[0]);
    const auto restoredState =
        restoreRunPersistenceSnapshot(*afterBoot.snapshot);
    TEST_ASSERT_TRUE(restoredState.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionPhase::RestartRevalidationPending),
        static_cast<int>(restoredState->sensorSelectionRuntime.phase));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorPeltierPermission::Blocked),
        static_cast<int>(restoredState->sensorSelectionRuntime.permission));
}

// PR-#99-letzter-Abschlussblocker: "stale-geprueft" ist ein eigenstaendiger
// Teil des Auftrags, nicht nur eine Folge des once-only-Verhaltens oben -
// dieser Test belegt ihn direkt. Zwischen Entscheidung und Anwendung
// veraendert sich der laufzeitseitige Zustand (z. B. durch eine
// zwischenzeitliche automatische Bewertung); applyRunCommand's bestehende
// before/after-Pruefung muss das erkennen und ablehnen, genau wie fuer jeden
// anderen Kommandotyp.
void test_persist_command_manual_recheck_product_ram_only_rejects_stale_decision() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 930U);
    state.sensorSelectionRuntime.phase =
        SensorSelectionPhase::AirFallbackActive;
    state.sensorSelectionRuntime.permission = SensorPeltierPermission::Allowed;
    state.activeRunSensorMode = RunSensorMode::Air;
    if (state.activeManualRun.has_value()) {
        state.activeManualRun->values.sensorMode = RunSensorMode::Air;
    }

    SensorSelectionCommandRequest request;
    request.envelope = {931U,
                        CommandSource::LocalDisplay,
                        700U,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true,
                        std::nullopt};
    request.action = SensorSelectionUserAction::RecheckProduct;
    request.safetyAllowsChange = true;
    CrossRolePlausibilityContext plausibility;
    plausibility.air = coordinatorValidSensorSnapshot();
    plausibility.product = coordinatorValidSensorSnapshot();
    plausibility.cooling = coordinatorValidSensorSnapshot();
    plausibility.evaluationMonotonicMillis = 700U;
    plausibility.thermalCompatibility.status = ThermalCompatibility::Stale;
    plausibility.thermalCompatibility.profileRevision = 9U;

    const auto decision =
        decideApplySensorSelectionAction(state, request, plausibility);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorSelectionApplyStatus::AppliedRamOnly),
        static_cast<int>(*decision.sensorSelectionApplyStatus));

    // Zwischenzeitliche Aenderung nach der Entscheidung, vor der Anwendung -
    // macht `decision` stale gegenueber dem jetzigen `state`.
    state.sensorSelectionRuntime.permission = SensorPeltierPermission::Blocked;
    const auto writesBefore = store.writeCount();

    const auto result = coordinator.persistCommand(
        state, decision, RunCheckpointTime{700U, std::nullopt});

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::StaleDecision),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writesBefore),
                           static_cast<unsigned>(store.writeCount()));
    // Die zwischenzeitliche Aenderung bleibt bestehen - eine abgelehnte
    // stale Entscheidung darf sie nicht ueberschreiben.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorPeltierPermission::Blocked),
        static_cast<int>(state.sensorSelectionRuntime.permission));
}

// #21, 6.5 Zeile 2/6.11: decideProgramStart's automatischer Ersatz auf Luft.
CommandDecision substitutedStartDecision(const RunCommandState& state,
                                         CommandId id) {
    auto program = runnableProgram();
    program.program.sensorPreference =
        SensorPreference::ProductIfAvailableElseAir;
    ProgramStartRequest request;
    request.envelope = {id,
                        CommandSource::LocalDisplay,
                        100U,
                        state.processState.transitionSequence,
                        state.runRevision,
                        std::nullopt,
                        std::nullopt,
                        true,
                        std::nullopt};
    request.runId = "persisted-run";
    request.program = program;
    request.sourceKind = ProgramSourceKind::FactoryCatalog;
    request.sourceProgramRevision = 1U;
    request.sensorMode = RunSensorMode::Product;
    request.safetyAllowsStart = true;
    request.airSensorValid = true;
    request.coolingSensorValid = true;
    request.productSensorValid = false;
    return decideProgramStart(state, request);
}

// #21, 6.11: RunPersistenceResult::startSensorSelectionNotice nur nach
// erfolgreichem Commit sichtbar - ein Schreibfehler beim Start darf keine
// scheinbar ausgefuehrte Start-Notice erzeugen.
void test_start_sensor_selection_notice_only_visible_after_successful_commit() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    RunCommandState state;
    state.processState.state = ProcessState::Standby;

    const auto decision = substitutedStartDecision(state, 910U);
    TEST_ASSERT_TRUE(decision.proposed());
    TEST_ASSERT_TRUE(decision.startSensorSelectionNotice.has_value());

    store.faultAt(store.writeCount() + 1U,
                  SequencedWriteStore::WriteFault::FailBeforeBegin);
    const auto failed = coordinator.persistCommand(
        state, decision, RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(failed.status));
    TEST_ASSERT_FALSE(failed.startSensorSelectionNotice.has_value());
    TEST_ASSERT_FALSE(state.activeProgramRun.has_value());

    const auto retryDecision = substitutedStartDecision(state, 910U);
    const auto committed = coordinator.persistCommand(
        state, retryDecision, RunCheckpointTime{100U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(committed.status));
    TEST_ASSERT_TRUE(committed.startSensorSelectionNotice.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunSensorMode::Product),
        static_cast<int>(committed.startSensorSelectionNotice->requestedMode));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunSensorMode::Air),
        static_cast<int>(committed.startSensorSelectionNotice->effectiveMode));
    TEST_ASSERT_EQUAL_UINT32(state.runRevision,
                             committed.startSensorSelectionNotice->runRevision);
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_load_empty_then_commit_and_restore_run_projection);
    RUN_TEST(
        test_apply_recovery_time_correction_uses_persist_command_atomically);
    RUN_TEST(
        test_unknown_outcome_is_resolved_by_exact_readback_and_duplicate_is_safe);
    RUN_TEST(test_mutation_before_initialization_writes_nothing);
    RUN_TEST(test_load_is_single_use_and_does_not_reinitialize_live_state);
    RUN_TEST(test_periodic_non_writes_are_truthful_and_do_not_apply);
    RUN_TEST(
        test_manual_completed_transition_commits_before_releasing_messages);
    RUN_TEST(test_tombstone_boot_resets_schedule_to_the_new_boot_timebase);
    RUN_TEST(
        test_already_persisted_command_id_is_rejected_after_restart_via_tombstone);
    RUN_TEST(test_orphan_checkpoint_revision_is_never_reused_after_restart);
    RUN_TEST(test_orphan_max_revision_is_sticky_and_blocks_new_writes);
    RUN_TEST(test_unknown_orphan_high_watermark_blocks_mutation);
    RUN_TEST(test_empty_boot_always_disarms_injected_schedule);
    RUN_TEST(test_mutation_write_faults_at_each_cutpoint_are_classified);
    RUN_TEST(test_unknown_outcome_at_each_mutation_write_is_resolved);
    RUN_TEST(test_unknown_outcome_with_exact_old_bytes_is_not_written);
    RUN_TEST(test_capacity_faults_at_each_mutation_cutpoint_are_classified);
    RUN_TEST(
        test_unknown_outcome_not_found_distinguishes_absent_and_existing_head);
    RUN_TEST(
        test_unknown_outcome_unresolvable_readbacks_block_every_mutation_step);
    RUN_TEST(test_unknown_outcome_absent_slot_not_found_is_not_indeterminate);
    RUN_TEST(
        test_unknown_outcome_old_bytes_at_prepared_head_and_occupied_target_slot);
    RUN_TEST(
        test_unknown_outcome_not_found_at_a_previously_occupied_target_slot);
    RUN_TEST(
        test_periodic_unknown_outcome_resolves_to_written_for_slot_and_head);
    RUN_TEST(
        test_periodic_unknown_outcome_old_bytes_preserve_existing_slot_and_head);
    RUN_TEST(test_periodic_target_slot_absent_not_found_resolves_cleanly);
    RUN_TEST(test_periodic_committed_head_existing_not_found_is_indeterminate);
    RUN_TEST(test_periodic_target_slot_existing_not_found_is_indeterminate);
    RUN_TEST(test_load_fallback_orphan_and_schema_epoch_matrix);
    RUN_TEST(test_tombstone_fallback_does_not_enter_recovery_pending);
    RUN_TEST(
        test_write_snapshot_core_rolls_back_loaded_active_run_before_commit);
    RUN_TEST(
        test_write_snapshot_core_rolls_back_fallback_recovery_before_commit);
    RUN_TEST(
        test_write_snapshot_core_indeterminate_always_blocks_loaded_and_fallback);
    RUN_TEST(test_same_slot_overrides_fail_closed_before_any_write);
    RUN_TEST(
        test_fallback_directive_rejects_invalid_mode_reference_combinations);
    RUN_TEST(
        test_load_rejects_a_structurally_valid_but_mismatched_current_reference);
    RUN_TEST(test_loaded_active_run_blocks_all_mutations_after_restart);
    RUN_TEST(test_persist_command_hands_off_target_context_only_after_apply);
    RUN_TEST(
        test_qualification_orchestrator_discards_failed_candidate_and_retries);
    RUN_TEST(
        test_qualification_orchestrator_preserves_fault_signal_and_binds_sample_time);
    RUN_TEST(test_critical_fault_precedes_invalid_qualification_evidence);
    RUN_TEST(
        test_critical_fault_persistence_failure_does_not_apply_process_fault);
    RUN_TEST(
        test_committed_target_changes_reset_qualification_without_a_sample);
    RUN_TEST(test_failed_target_context_commit_keeps_qualification_credit);
    RUN_TEST(
        test_evaluate_temperature_control_uses_canonical_context_per_phase);
    RUN_TEST(
        test_evaluate_temperature_control_target_changed_uses_new_value_only);
    RUN_TEST(
        test_evaluate_temperature_control_cooling_uses_completion_target_only);
    RUN_TEST(test_evaluate_temperature_control_manual_run_uses_manual_target);
    RUN_TEST(
        test_evaluate_temperature_control_fails_closed_outside_temperature_control);
    RUN_TEST(test_evaluate_temperature_control_invalid_context_resets_runtime);
    RUN_TEST(
        test_qualification_orchestrator_rejects_band_and_duration_mismatch);
    RUN_TEST(
        test_manual_run_qualification_reaches_holding_via_application_path);
    RUN_TEST(test_product_inserted_commits_before_advancing_and_restores);
    RUN_TEST(test_air_run_product_inserted_is_air_to_air_without_pi_transition);
    RUN_TEST(test_application_bridge_hands_off_cooling_context_once);
    RUN_TEST(test_application_actuator_handoff_and_lifecycle_boundary);
    RUN_TEST(
        test_application_repeated_i_tick_after_stale_b_does_not_corrupt_next_evaluation);
    RUN_TEST(test_application_multi_rate_windows_and_downstream_counter_probe);
    RUN_TEST(
        test_application_bridge_resets_runtime_at_committed_lifecycle_boundaries);
    RUN_TEST(test_abort_and_cool_is_a_new_active_run_boundary);
    RUN_TEST(test_application_bridge_resets_runtime_on_recovery_activation);
    RUN_TEST(
        test_product_wait_expired_tombstones_and_does_not_revive_after_restart);
    RUN_TEST(test_periodic_unknown_outcomes_at_slot_and_head_are_unresolvable);
    RUN_TEST(test_stale_invalid_and_time_mismatched_decisions_write_nothing);
    RUN_TEST(test_stale_invalid_and_time_mismatched_transitions_write_nothing);
    RUN_TEST(test_restart_after_prepared_or_slot_cut_is_interrupted);
    RUN_TEST(test_periodic_slot_and_head_faults_preserve_cutpoint_truth);
    RUN_TEST(test_invalid_effect_and_message_counts_are_rejected_before_writes);
    RUN_TEST(
        test_persist_sensor_selection_writes_schema_two_and_reports_permission_blocked);
    RUN_TEST(
        test_persist_sensor_selection_mode_change_fills_event_and_updates_mode);
    RUN_TEST(test_persist_sensor_selection_mode_change_on_manual_run);
    RUN_TEST(
        test_persist_sensor_selection_recovery_revalidation_restores_permission);
    RUN_TEST(
        test_persist_sensor_selection_from_ready_empty_reports_no_active_run);
    RUN_TEST(test_persist_sensor_selection_same_mutation_is_not_replayed);
    RUN_TEST(test_persist_sensor_selection_requires_active_run_fields);
    RUN_TEST(test_apply_live_recovery_evidence_requires_a_value_to_latch);
    RUN_TEST(test_activate_loaded_run_resumes_and_retains_unresolved_anchor);
    RUN_TEST(
        test_loaded_qualifying_recovery_rebases_and_restarts_qualification);
    RUN_TEST(test_fallback_qualifying_recovery_rebases_through_common_helper);
    RUN_TEST(
        test_loaded_recovery_rebase_preserves_reaching_preheating_and_fermenting);
    RUN_TEST(test_run_recovery_coordinator_delegates_loaded_activation);
    RUN_TEST(test_live_fermenting_transition_folds_observed_time_once);
    RUN_TEST(test_real_fermenting_hop_one_folds_only_this_boot_local_seconds);
    RUN_TEST(
        test_three_real_fermenting_reboots_fold_exactly_n1_plus_n2_plus_n3);
    RUN_TEST(
        test_hop_one_waiting_utc_reevaluation_has_single_gate_and_tombstone_rules);
    RUN_TEST(
        test_run_recovery_coordinator_reevaluates_resumed_time_without_biological_fold);
    RUN_TEST(test_run_recovery_coordinator_books_weighting_atomically_once);
    RUN_TEST(test_weighted_progress_keeps_cumulative_confidence_conservative);
    RUN_TEST(test_activate_loaded_run_resolved_resume_clears_anchor);
    RUN_TEST(test_activate_loaded_run_episode_refreshes_hop_one_anchor);
    RUN_TEST(test_activate_loaded_run_persists_sensor_gate_rejection_as_fault);
    RUN_TEST(test_loaded_gate_rejection_keeps_persistence_cutpoint_contract);
    RUN_TEST(
        test_activate_fallback_recovered_run_replaces_damaged_current_slot);
    RUN_TEST(
        test_activate_fallback_run_persists_sensor_gate_rejection_as_fault);
    RUN_TEST(test_fallback_completed_recovery_repairs_current_and_repeats);
    RUN_TEST(
        test_fallback_completed_storage_recovery_cutpoints_remain_fail_closed);
    RUN_TEST(test_resolve_recovery_outcome_waiting_assume_still_valid_resumes);
    RUN_TEST(test_resolve_recovery_outcome_rejects_gate_and_deduplicates);
    RUN_TEST(test_waiting_definitely_expired_tombstones_before_sensor_gate);
    RUN_TEST(
        test_resolve_recovery_outcome_waiting_threshold_crossed_tombstones);
    RUN_TEST(
        test_resolve_recovery_outcome_fermenting_bounds_gate_and_completion);
    RUN_TEST(
        test_activate_loaded_completed_run_refreshes_boot_time_without_transition);
    RUN_TEST(
        test_resolve_recovery_outcome_cool_holding_bounds_gate_completes_hold);
    RUN_TEST(
        test_persist_sensor_selection_from_loaded_active_run_stays_recovery_pending);
    RUN_TEST(test_persist_sensor_selection_rejects_manual_causes);
    RUN_TEST(test_persist_sensor_selection_rejects_non_persistent_status);
    RUN_TEST(test_effective_sensor_role_change_controls_pi_handoff);
    RUN_TEST(
        test_qualifying_sensor_role_change_resets_state_marker_and_qualifier_atomically);
    RUN_TEST(test_persist_command_applies_and_writes_manual_continue_with_air);
    RUN_TEST(
        test_persist_command_manual_recheck_product_ram_only_is_ram_only_and_idempotent);
    RUN_TEST(
        test_persist_command_manual_recheck_product_ram_only_rejects_stale_decision);
    RUN_TEST(
        test_start_sensor_selection_notice_only_visible_after_successful_commit);
    return UNITY_END();
}
