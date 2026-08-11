#include <limits>
#include <map>
#include <set>
#include <utility>

#include <unity.h>

#include "run_persistence_coordinator.hpp"
#include "run_persistence_codec.hpp"
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

namespace fermentation {
class RunPersistenceCoordinatorTestAccess {
   public:
    static RunPersistenceResult writeSnapshotCore(
        RunPersistenceCoordinator& coordinator,
        const RunPersistenceSnapshot& snapshot, const RunCheckpointTime& time,
        bool periodic, const RunCommandState& before,
        RunPersistenceMutationKind mutationKind,
        std::optional<CommandId> commandId,
        std::optional<std::size_t> targetSlotOverride,
        std::optional<RunCheckpointReference> fallbackOverride,
        RunPersistenceCoordinatorState rollbackState) {
        return coordinator.writeSnapshotCore(
            snapshot, time, periodic, before, mutationKind, commandId,
            targetSlotOverride, fallbackOverride, rollbackState);
    }

    static RunCheckpointReference currentReference(
        const RunPersistenceCoordinator& coordinator) {
        return coordinator.currentHead_->current;
    }

    static RunCheckpointReference fallbackReference(
        const RunPersistenceCoordinator& coordinator) {
        return *coordinator.currentHead_->fallback;
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

CommandDecision startDecision(
    const RunCommandState& state, CommandId id,
    std::uint64_t monotonicMillis = 100U,
    std::optional<ProgramDocument> program = std::nullopt) {
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
    request.program = program.has_value() ? *program : runnableProgram();
    request.sourceKind = ProgramSourceKind::FactoryCatalog;
    request.sourceProgramRevision = 1U;
    request.sensorMode = RunSensorMode::Product;
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
                        true};
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
                        true};
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
                     true};
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
                     true};
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
                     true};
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
                     true};
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
                std::nullopt, std::nullopt, std::nullopt,
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
                std::nullopt, current.slot, fallback,
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
                std::nullopt, std::nullopt,
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
                current.slot, fallback,
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
        return RunPersistenceCoordinatorTestAccess::writeSnapshotCore(
            coordinator, snapshot, RunCheckpointTime{200U, std::nullopt},
            periodic, before, mutationKind, std::nullopt, current.slot,
            fallback, RunPersistenceCoordinatorState::Ready);
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
            ProcessSignals{true, false}, TransitionRequest{}, 100U);
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
    const auto tracking = decideProcessTransition(
        state.processState, &*state.processRunSnapshot,
        ProcessSignals{true, false}, TransitionRequest{}, 100U);
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
        ProcessSignals{true, false}, TransitionRequest{}, 600100U);
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
    store.faultAt(store.writeCount() + 1U,
                  SequencedWriteStore::WriteFault::FailBeforeBegin);
    const auto failed = coordinator.persistTransition(
        state, inserted, RunCheckpointTime{600200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::WriteFailed),
        static_cast<int>(failed.status));
    TEST_ASSERT_EQUAL_UINT32(0U, failed.messageCount);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::WaitingForProduct),
                          static_cast<int>(state.processState.state));

    const auto committed = coordinator.persistTransition(
        state, inserted, RunCheckpointTime{600200U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(committed.status));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ProcessState::ReachingTarget),
                          static_cast<int>(state.processState.state));

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

CrossRolePlausibilityContext recoveryPlausibility(
    std::uint64_t evaluatedAtMonotonicMillis, bool productValid = true) {
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
    signals.qualificationConditionValid = true;
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
    signals.qualificationConditionValid = true;
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
    signals.qualificationConditionValid = true;
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
    signals.qualificationConditionValid = true;
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

void test_activate_loaded_run_resolved_resume_clears_anchor() {
    device_platform_test_support::SimulatedPersistentStateStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 1002U);

    ProcessSignals signals;
    signals.qualificationConditionValid = true;
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
    signals.qualificationConditionValid = true;
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

    const auto result = coordinator.persistSensorSelection(
        state, mutation, RunCheckpointTime{500U, 1700000600});

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_TRUE(result.sensorSelectionEvent.has_value());
    TEST_ASSERT_FALSE(result.sensorSelectionNotice.has_value());
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

    const auto result = coordinator.persistSensorSelection(
        state, mutation, RunCheckpointTime{500U, std::nullopt});

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(result.status));
    TEST_ASSERT_TRUE(state.activeRunSensorMode.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunSensorMode::Product),
                          static_cast<int>(*state.activeRunSensorMode));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunSensorMode::Product),
        static_cast<int>(state.activeManualRun->values.sensorMode));
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
                        true};
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
                        true};
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
                        true};
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
                        true};
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
        test_load_rejects_a_structurally_valid_but_mismatched_current_reference);
    RUN_TEST(test_loaded_active_run_blocks_all_mutations_after_restart);
    RUN_TEST(test_product_inserted_commits_before_advancing_and_restores);
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
    RUN_TEST(test_activate_loaded_run_resolved_resume_clears_anchor);
    RUN_TEST(test_activate_loaded_run_episode_refreshes_hop_one_anchor);
    RUN_TEST(test_activate_loaded_run_persists_sensor_gate_rejection_as_fault);
    RUN_TEST(test_loaded_gate_rejection_keeps_persistence_cutpoint_contract);
    RUN_TEST(
        test_activate_fallback_recovered_run_replaces_damaged_current_slot);
    RUN_TEST(test_resolve_recovery_outcome_waiting_assume_still_valid_resumes);
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
    RUN_TEST(test_persist_command_applies_and_writes_manual_continue_with_air);
    RUN_TEST(
        test_persist_command_manual_recheck_product_ram_only_is_ram_only_and_idempotent);
    RUN_TEST(
        test_persist_command_manual_recheck_product_ram_only_rejects_stale_decision);
    RUN_TEST(
        test_start_sensor_selection_notice_only_visible_after_successful_commit);
    return UNITY_END();
}
