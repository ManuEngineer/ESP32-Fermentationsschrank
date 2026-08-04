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
    program.preheat = true;
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
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::RecoveryPending),
        static_cast<int>(
            afterBoot
                .persistCommand(state, startDecision(state, 780U),
                                RunCheckpointTime{100U, std::nullopt})
                .status));
    TransitionDecision transition;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::RecoveryPending),
        static_cast<int>(
            afterBoot
                .persistTransition(state, transition,
                                   RunCheckpointTime{101U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(RunPersistenceResultStatus::RecoveryPending),
        static_cast<int>(
            afterBoot
                .checkpointPeriodic(state,
                                    RunCheckpointTime{400000U, std::nullopt})
                .status));
    TEST_ASSERT_EQUAL_UINT(static_cast<unsigned>(writes),
                           static_cast<unsigned>(store.writeCount()));
    TEST_ASSERT_TRUE(loaded.snapshot.has_value());
    TEST_ASSERT_EQUAL_UINT32(1U, loaded.snapshot->persistedRunCommandCount);
    TEST_ASSERT_EQUAL_UINT64(780U, loaded.snapshot->persistedRunCommandIds[0]);
}

void test_product_inserted_and_wait_expired_persist_atomically() {
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
                .persistCommand(state, startDecision(state, 790U),
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
    auto waiting = decideProcessTransition(
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
        decodeRunPersistenceSnapshot(insertedEnvelope.envelope->payload);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceCodecStatus::Success),
                          static_cast<int>(insertedSnapshot.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(ProcessState::ReachingTarget),
        static_cast<int>(insertedSnapshot.snapshot->processState.state));

    // Let the state machine produce its automatic ProductWaitExpired decision.
    auto expired = TransitionDecision{};
    state.processState.state = ProcessState::WaitingForProduct;
    state.processState.stateEnteredAtMillis = 600200U;
    state.processState.targetReachStartedAtMillis = 0U;
    state.processState.targetReachWarningIssued = false;
    expired = decideProcessTransition(
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
    const auto expiredCommit = coordinator.persistTransition(
        state, expired, RunCheckpointTime{2400201U, std::nullopt});
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RunPersistenceResultStatus::Applied),
                          static_cast<int>(expiredCommit.status));
    TEST_ASSERT_FALSE(state.activeProgramRun.has_value());
    TEST_ASSERT_TRUE(state.activeRunId.empty());
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
    RUN_TEST(test_load_fallback_orphan_and_schema_epoch_matrix);
    RUN_TEST(test_loaded_active_run_blocks_all_mutations_after_restart);
    RUN_TEST(test_product_inserted_and_wait_expired_persist_atomically);
    RUN_TEST(test_periodic_unknown_outcomes_at_slot_and_head_are_unresolvable);
    RUN_TEST(test_stale_invalid_and_time_mismatched_decisions_write_nothing);
    RUN_TEST(test_restart_after_prepared_or_slot_cut_is_interrupted);
    RUN_TEST(test_periodic_slot_and_head_faults_preserve_cutpoint_truth);
    RUN_TEST(test_invalid_effect_and_message_counts_are_rejected_before_writes);
    return UNITY_END();
}
