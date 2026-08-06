#include <limits>
#include <map>
#include <set>
#include <utility>

#include <unity.h>

#include "run_persistence_coordinator.hpp"
#include "run_persistence_codec.hpp"
#include "simulated_persistent_state_store.hpp"
#include "state_store.hpp"
#include "state_store_key.hpp"
#include "standard_program_catalog.hpp"
#include "storage_envelope.hpp"

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
    fallback.backing().injectCorruption(slotKey("rc1"), "damaged-current");
    fallback.restart();
    RunPersistenceCoordinator recovered(
        fallback, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceLoadStatus::FallbackRecovered),
        static_cast<int>(recovered.loadAndInitialize().status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceCoordinatorState::BlockedIndeterminate),
        static_cast<int>(recovered.state()));

    fallback.backing().injectCorruption(slotKey("rc0"), "damaged-fallback");
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
    const auto insertedSnapshot = decodeRunPersistenceSnapshot(
        insertedEnvelope.envelope->payload, insertedEnvelope.envelope->schemaVersion);
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

// Ready fixture with an active program run, matching what #21 Commit 5's
// start-path wiring will eventually populate on its own: `state.
// sensorSelection` is set here by hand because that wiring does not exist
// yet (out of Commit 3's scope) - decideProgramStart itself still leaves it
// unset, exercised separately below.
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
    state.sensorSelection = PersistedSensorSelectionState{
        SensorSelectionProvenance::InitialSelection,
        SensorSelectionDecisionCause::StartSelection, state.runRevision};
    return state;
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
    state.sensorSelection = PersistedSensorSelectionState{
        SensorSelectionProvenance::InitialSelection,
        SensorSelectionDecisionCause::StartSelection, state.runRevision};
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
    mutation.event = SensorSelectionEvent{
        *state.activeRunSensorMode, newMode,           cause,
        mutation.resultingRunRevision, nowMonotonicMillis, std::nullopt};
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
        mutation.resultingRunRevision,      *state.activeRunSensorMode,
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

    const auto reference = coordinator.state();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCoordinatorState::Ready),
                          static_cast<int>(reference));

    const auto currentSlotRead = store.read(slotKey("rc1"), 8240U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(currentSlotRead.status));
    const auto envelope = device_platform::decodeEnvelope(currentSlotRead.value);
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
    const auto mutation = modeChangeMutation(
        state, RunSensorMode::Air, SensorSelectionDecisionCause::FallbackToAir,
        500U);

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
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunSensorMode::Product),
                          static_cast<int>(state.activeManualRun->values.sensorMode));
}

void test_persist_sensor_selection_recovery_revalidation_restores_permission() {
    SequencedWriteStore store;
    RunPersistenceCoordinator coordinator(
        store, device_platform::StorageEpoch(1U), RunCheckpointSchedule{});
    static_cast<void>(coordinator.loadAndInitialize());
    auto state = readyActiveRunWithSensorSelection(coordinator, 907U);
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

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::NotEligible),
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

    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::NotEligible),
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

// #21, 6.5 Zeile 2/6.11: decideProgramStart's automatischer Ersatz auf Luft.
CommandDecision substitutedStartDecision(const RunCommandState& state,
                                         CommandId id) {
    auto program = runnableProgram();
    program.program.sensorPreference = SensorPreference::ProductIfAvailableElseAir;
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
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::WriteFailed),
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
    TEST_ASSERT_EQUAL_UINT32(
        state.runRevision, committed.startSensorSelectionNotice->runRevision);
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
    RUN_TEST(test_persist_sensor_selection_mode_change_fills_event_and_updates_mode);
    RUN_TEST(test_persist_sensor_selection_mode_change_on_manual_run);
    RUN_TEST(test_persist_sensor_selection_recovery_revalidation_restores_permission);
    RUN_TEST(test_persist_sensor_selection_from_ready_empty_reports_no_active_run);
    RUN_TEST(test_persist_sensor_selection_requires_active_run_fields);
    RUN_TEST(
        test_persist_sensor_selection_from_loaded_active_run_stays_recovery_pending);
    RUN_TEST(test_persist_sensor_selection_rejects_manual_causes);
    RUN_TEST(test_persist_sensor_selection_rejects_non_persistent_status);
    RUN_TEST(test_start_sensor_selection_notice_only_visible_after_successful_commit);
    return UNITY_END();
}
