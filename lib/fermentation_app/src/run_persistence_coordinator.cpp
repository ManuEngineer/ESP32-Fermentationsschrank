#include "run_persistence_coordinator.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "run_persistence_codec.hpp"
#include "storage_envelope.hpp"

namespace fermentation {
namespace {

constexpr std::uint32_t kRunPersistenceSchema = 1U;
constexpr device_platform::RecordTypeId kCheckpointRecordType{7U};
constexpr device_platform::RecordTypeId kHeadRecordType{8U};
constexpr std::size_t kMaximumCheckpointRecordBytes = 8240U;
constexpr std::size_t kMaximumHeadRecordBytes = 256U;

bool eligibleTransition(TransitionReason reason) {
    switch (reason) {
        case TransitionReason::QualificationTrackingStarted:
        case TransitionReason::QualificationReset:
        case TransitionReason::PreheatQualified:
        case TransitionReason::ProductInserted:
        case TransitionReason::ProductWaitExpired:
        case TransitionReason::TargetReachTimeExceeded:
        case TransitionReason::TargetQualified:
        case TransitionReason::FermentationCompleted:
        case TransitionReason::CoolingTargetReached:
        case TransitionReason::HoldDurationCompleted:
        case TransitionReason::HoldFinishedByUser:
            return true;
        default:
            return false;
    }
}

void clearCandidateRun(RunCommandState& state) {
    state.activeProgramRun.reset();
    state.activeManualRun.reset();
    state.processRunSnapshot.reset();
    state.activeRunId.clear();
    state.activeRunSensorMode.reset();
}

}  // namespace

RunPersistenceCoordinator::RunPersistenceCoordinator(
    device_platform::IStateStore& store, device_platform::StorageEpoch epoch,
    RunCheckpointSchedule schedule) noexcept
    : store_(store), epoch_(epoch), schedule_(std::move(schedule)) {}

RunPersistenceResult RunPersistenceCoordinator::result(
    RunPersistenceResultStatus status, RunPersistenceStep step,
    RunPersistenceTechnicalReason reason,
    RunPersistenceDurability durability) const {
    RunPersistenceResult value;
    value.status = status;
    value.step = step;
    value.technicalReason = reason;
    value.durability = durability;
    value.coordinatorState = state_;
    return value;
}

void RunPersistenceCoordinator::enterBlockedIndeterminate() {
    state_ = RunPersistenceCoordinatorState::BlockedIndeterminate;
}

RunPersistenceResult RunPersistenceCoordinator::unavailableResult() const {
    if (state_ == RunPersistenceCoordinatorState::Uninitialized)
        return result(RunPersistenceResultStatus::NotInitialized);
    if (state_ == RunPersistenceCoordinatorState::Busy)
        return result(RunPersistenceResultStatus::Busy);
    if (state_ ==
        RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed)
        return result(
            RunPersistenceResultStatus::PersistenceCommittedApplyFailed);
    if (state_ == RunPersistenceCoordinatorState::LoadedActiveRun)
        return result(RunPersistenceResultStatus::RecoveryPending);
    return result(RunPersistenceResultStatus::Blocked);
}

RunPersistenceLoadResult RunPersistenceCoordinator::loadAndInitialize() {
    if (state_ != RunPersistenceCoordinatorState::Uninitialized) {
        return {RunPersistenceLoadStatus::AlreadyInitialized, std::nullopt};
    }
    currentHead_.reset();
    slots_[0].reset();
    slots_[1].reset();
    persistedIdCount_ = 0U;
    const auto read = store_.readHead(kMaximumHeadRecordBytes);
    if (read.status == device_platform::StateStoreReadStatus::NotFound) {
        bool slotPresent = false;
        for (std::size_t slot = 0U; slot < 2U; ++slot) {
            const auto probe =
                store_.readSlot(slot, kMaximumCheckpointRecordBytes);
            if (probe.status ==
                device_platform::StateStoreReadStatus::Success) {
                slotPresent = true;
                continue;
            }
            if (probe.status ==
                device_platform::StateStoreReadStatus::NotFound) {
                continue;
            }
            enterBlockedIndeterminate();
            return {probe.status ==
                            device_platform::StateStoreReadStatus::CapacityError
                        ? RunPersistenceLoadStatus::CapacityExceeded
                        : RunPersistenceLoadStatus::ReadFailed,
                    std::nullopt};
        }
        if (!slotPresent) {
            state_ = RunPersistenceCoordinatorState::ReadyEmpty;
            return {RunPersistenceLoadStatus::NoPersistedRun, std::nullopt};
        }
        enterBlockedIndeterminate();
        return {RunPersistenceLoadStatus::NotReconstructibleOrphanedState,
                std::nullopt};
    }
    if (read.status == device_platform::StateStoreReadStatus::CapacityError) {
        enterBlockedIndeterminate();
        return {RunPersistenceLoadStatus::CapacityExceeded, std::nullopt};
    }
    if (read.status != device_platform::StateStoreReadStatus::Success) {
        enterBlockedIndeterminate();
        return {RunPersistenceLoadStatus::ReadFailed, std::nullopt};
    }
    const auto headEnvelope = device_platform::decodeEnvelope(read.value);
    if (headEnvelope.envelope.has_value() &&
        headEnvelope.envelope->storageEpoch != epoch_) {
        enterBlockedIndeterminate();
        return {RunPersistenceLoadStatus::ForeignEpoch, std::nullopt};
    }
    if (headEnvelope.envelope.has_value() &&
        headEnvelope.envelope->schemaVersion != kRunPersistenceSchema) {
        enterBlockedIndeterminate();
        return {RunPersistenceLoadStatus::UnsupportedSchema, std::nullopt};
    }
    auto head = decodeRunPersistenceHead(read.value, epoch_);
    if (!head.has_value()) {
        enterBlockedIndeterminate();
        return {RunPersistenceLoadStatus::NotReconstructible, std::nullopt};
    }
    currentHead_ = std::move(*head);
    nextHeadRevision_ =
        currentHead_->revision == std::numeric_limits<std::uint64_t>::max()
            ? 0U
            : currentHead_->revision + 1U;
    if (currentHead_->state == RunPersistenceHeadState::Prepared) {
        enterBlockedIndeterminate();
        return {RunPersistenceLoadStatus::PreparedInterrupted, std::nullopt};
    }
    const auto loadReference = [this](const RunCheckpointReference& reference,
                                      RunPersistenceLoadStatus& status)
        -> std::optional<RunPersistenceRawRecord> {
        const auto slot = reference.slot;
        const auto slotRead =
            store_.readSlot(slot, kMaximumCheckpointRecordBytes);
        if (slotRead.status ==
            device_platform::StateStoreReadStatus::NotFound) {
            status = RunPersistenceLoadStatus::NotReconstructible;
            return std::nullopt;
        }
        if (slotRead.status ==
            device_platform::StateStoreReadStatus::CapacityError) {
            status = RunPersistenceLoadStatus::CapacityExceeded;
            return std::nullopt;
        }
        if (slotRead.status != device_platform::StateStoreReadStatus::Success) {
            status = RunPersistenceLoadStatus::ReadFailed;
            return std::nullopt;
        }
        const auto envelope = device_platform::decodeEnvelope(slotRead.value);
        if (envelope.envelope.has_value() &&
            envelope.envelope->storageEpoch != epoch_) {
            status = RunPersistenceLoadStatus::ForeignEpoch;
            return std::nullopt;
        }
        if (envelope.envelope.has_value() &&
            envelope.envelope->schemaVersion != kRunPersistenceSchema) {
            status = RunPersistenceLoadStatus::UnsupportedSchema;
            return std::nullopt;
        }
        auto record = decodeRunPersistenceRecord(slotRead.value, epoch_);
        if (!record.has_value() ||
            !runCheckpointReferenceMatches(reference, *record, slot)) {
            status = RunPersistenceLoadStatus::NotReconstructible;
            return std::nullopt;
        }
        status = RunPersistenceLoadStatus::Current;
        return record;
    };
    RunPersistenceLoadStatus currentStatus = RunPersistenceLoadStatus::Current;
    const auto currentRecord =
        loadReference(currentHead_->current, currentStatus);
    if (currentRecord.has_value()) {
        slots_[currentHead_->current.slot] = *currentRecord;
        const auto& snap = currentRecord->snapshot;
        nextCheckpointRevision_ =
            currentRecord->checkpointRevision ==
                    std::numeric_limits<std::uint64_t>::max()
                ? 0U
                : currentRecord->checkpointRevision + 1U;
        persistedIds_ = snap.persistedRunCommandIds;
        persistedIdCount_ = snap.persistedRunCommandCount;
        // Monotonic milliseconds are boot-local.  A persisted value is
        // recovery data, never the base for a new boot's schedule.
        schedule_.reset();

        // The head is the only source of the recoverable state.  A valid
        // unreferenced checkpoint is therefore never loaded as current or
        // fallback.  Its envelope revision is nevertheless a physical
        // high-watermark: reusing it could overwrite a known orphan with the
        // same revision after a reboot.
        for (std::size_t slot = 0U; slot < 2U; ++slot) {
            const auto physical =
                store_.readSlot(slot, kMaximumCheckpointRecordBytes);
            if (physical.status !=
                device_platform::StateStoreReadStatus::Success) {
                continue;
            }
            const auto physicalEnvelope =
                device_platform::decodeEnvelope(physical.value);
            if (!physicalEnvelope.envelope.has_value() ||
                physicalEnvelope.envelope->recordTypeId !=
                    kCheckpointRecordType ||
                physicalEnvelope.envelope->schemaVersion !=
                    kRunPersistenceSchema ||
                physicalEnvelope.envelope->storageEpoch != epoch_ ||
                physicalEnvelope.envelope->versionValue == 0U) {
                continue;
            }
            nextCheckpointRevision_ =
                physicalEnvelope.envelope->versionValue ==
                        std::numeric_limits<std::uint64_t>::max()
                    ? 0U
                    : std::max(nextCheckpointRevision_,
                               physicalEnvelope.envelope->versionValue + 1U);
        }
        if (snap.variant == RunCheckpointVariant::NoActiveRun) {
            state_ = RunPersistenceCoordinatorState::ReadyEmpty;
            return {RunPersistenceLoadStatus::NoActiveRun, snap};
        }
        state_ = RunPersistenceCoordinatorState::LoadedActiveRun;
        return {RunPersistenceLoadStatus::Current, snap};
    }
    if (currentHead_->fallback.has_value()) {
        RunPersistenceLoadStatus fallbackStatus =
            RunPersistenceLoadStatus::Current;
        const auto fallbackRecord =
            loadReference(*currentHead_->fallback, fallbackStatus);
        if (fallbackRecord.has_value()) {
            slots_[currentHead_->fallback->slot] = *fallbackRecord;
            enterBlockedIndeterminate();
            return {RunPersistenceLoadStatus::FallbackRecovered,
                    fallbackRecord->snapshot};
        }
        if (fallbackStatus == RunPersistenceLoadStatus::ReadFailed ||
            fallbackStatus == RunPersistenceLoadStatus::CapacityExceeded ||
            fallbackStatus == RunPersistenceLoadStatus::ForeignEpoch ||
            fallbackStatus == RunPersistenceLoadStatus::UnsupportedSchema) {
            enterBlockedIndeterminate();
            return {fallbackStatus, std::nullopt};
        }
    }
    enterBlockedIndeterminate();
    return {currentStatus, std::nullopt};
}

RunPersistenceResult RunPersistenceCoordinator::writeSnapshot(
    const RunPersistenceSnapshot& snapshot, const RunCheckpointTime& time,
    bool periodic, const RunCommandState& before,
    std::optional<CommandId> commandId) {
    if (state_ != RunPersistenceCoordinatorState::Ready &&
        state_ != RunPersistenceCoordinatorState::ReadyEmpty)
        return unavailableResult();
    if (nextCheckpointRevision_ == 0U || nextHeadRevision_ == 0U ||
        (!periodic &&
         nextHeadRevision_ == std::numeric_limits<std::uint64_t>::max()))
        return result(RunPersistenceResultStatus::CounterOverflow);
    const auto timeStatus = schedule_.validate(time.monotonicMillis);
    if (timeStatus == RunCheckpointScheduleStatus::TimeWentBackwards) {
        return result(RunPersistenceResultStatus::TimeWentBackwards);
    }
    if (timeStatus != RunCheckpointScheduleStatus::Success) {
        return result(RunPersistenceResultStatus::InvalidDecision);
    }
    state_ = RunPersistenceCoordinatorState::Busy;
    const std::size_t target =
        currentHead_.has_value() ? 1U - currentHead_->current.slot : 0U;
    std::string payload;
    if (encodeRunPersistenceSnapshot(snapshot, payload) !=
        RunPersistenceCodecStatus::Success) {
        state_ = currentHead_.has_value()
                     ? RunPersistenceCoordinatorState::Ready
                     : RunPersistenceCoordinatorState::ReadyEmpty;
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CheckpointSlot,
                      RunPersistenceTechnicalReason::CodecError);
    }
    device_platform::StorageEnvelope env{
        kCheckpointRecordType,   kRunPersistenceSchema, epoch_,
        nextCheckpointRevision_, time.utcUnixSeconds,   payload};
    std::string targetBytes;
    if (device_platform::encodeEnvelope(env, targetBytes,
                                        kMaximumCheckpointRecordBytes) !=
        device_platform::EnvelopeEncodeStatus::Success) {
        state_ = currentHead_.has_value()
                     ? RunPersistenceCoordinatorState::Ready
                     : RunPersistenceCoordinatorState::ReadyEmpty;
        return result(RunPersistenceResultStatus::CapacityExceeded,
                      RunPersistenceStep::CheckpointSlot,
                      RunPersistenceTechnicalReason::CodecError);
    }
    RunPersistenceRawRecord record{
        targetBytes, snapshot, nextCheckpointRevision_, time.utcUnixSeconds};
    const auto ref = makeRunCheckpointReference(target, record, epoch_);
    RunPersistenceHead prepared;
    RunPersistenceHead committed;
    std::optional<std::string> preparedBytes;
    std::optional<std::string> committedBytes;
    if (periodic) {
        committed.state = RunPersistenceHeadState::Committed;
        committed.revision = nextHeadRevision_;
        committed.current = ref;
        if (snapshot.variant != RunCheckpointVariant::NoActiveRun &&
            currentHead_.has_value()) {
            committed.fallback = currentHead_->current;
        }
        committedBytes = encodeRunPersistenceHead(committed, epoch_);
        if (!committedBytes.has_value()) {
            state_ = currentHead_.has_value()
                         ? RunPersistenceCoordinatorState::Ready
                         : RunPersistenceCoordinatorState::ReadyEmpty;
            return result(RunPersistenceResultStatus::CapacityExceeded,
                          RunPersistenceStep::CommittedHead,
                          RunPersistenceTechnicalReason::CodecError,
                          RunPersistenceDurability::Unchanged);
        }
    } else {
        prepared.state = RunPersistenceHeadState::Prepared;
        prepared.revision = nextHeadRevision_;
        prepared.target = ref;
        prepared.mutationKind = commandId.has_value()
                                    ? RunPersistenceMutationKind::Command
                                    : RunPersistenceMutationKind::Transition;
        prepared.commandId = commandId;
        prepared.oldRunRevision = before.runRevision;
        prepared.newRunRevision = snapshot.runRevision;
        prepared.oldTransitionSequence = before.processState.transitionSequence;
        prepared.newTransitionSequence =
            snapshot.processState.transitionSequence;
        if (currentHead_.has_value()) {
            prepared.preparedCurrent = currentHead_->current;
            prepared.preparedFallback = currentHead_->fallback;
        }
        committed.state = RunPersistenceHeadState::Committed;
        committed.revision = prepared.revision + 1U;
        committed.current = ref;
        if (snapshot.variant != RunCheckpointVariant::NoActiveRun &&
            currentHead_.has_value()) {
            committed.fallback = currentHead_->current;
        }
        preparedBytes = encodeRunPersistenceHead(prepared, epoch_);
        committedBytes = encodeRunPersistenceHead(committed, epoch_);
        if (!preparedBytes.has_value() || !committedBytes.has_value()) {
            state_ = currentHead_.has_value()
                         ? RunPersistenceCoordinatorState::Ready
                         : RunPersistenceCoordinatorState::ReadyEmpty;
            return result(RunPersistenceResultStatus::CapacityExceeded,
                          !preparedBytes.has_value()
                              ? RunPersistenceStep::PreparedHead
                              : RunPersistenceStep::CommittedHead,
                          RunPersistenceTechnicalReason::CodecError,
                          RunPersistenceDurability::Unchanged);
        }
    }
    const auto oldHead = currentHead_.has_value()
                             ? std::optional<std::string>{currentHead_->bytes}
                             : std::nullopt;
    // Read every target before writing.  `slots_` only tracks decoded records;
    // this read preserves an orphaned or otherwise undecodable old byte value
    // as Existing rather than silently treating it as Absent.
    const auto physicalTarget =
        store_.readSlot(target, kMaximumCheckpointRecordBytes);
    std::optional<std::string> oldSlot;
    if (physicalTarget.status ==
        device_platform::StateStoreReadStatus::Success) {
        oldSlot = physicalTarget.value;
    } else if (physicalTarget.status !=
               device_platform::StateStoreReadStatus::NotFound) {
        enterBlockedIndeterminate();
        return result(
            physicalTarget.status ==
                    device_platform::StateStoreReadStatus::CapacityError
                ? RunPersistenceResultStatus::CapacityExceeded
                : RunPersistenceResultStatus::PersistenceIndeterminate,
            RunPersistenceStep::CheckpointSlot,
            RunPersistenceTechnicalReason::StoreReadError,
            RunPersistenceDurability::Unchanged);
    }

    auto readyState = [this]() {
        state_ = currentHead_.has_value()
                     ? RunPersistenceCoordinatorState::Ready
                     : RunPersistenceCoordinatorState::ReadyEmpty;
    };
    auto writeFailure = [this](RunPersistenceStoreWriteResult written,
                               RunPersistenceStep step,
                               RunPersistenceDurability durability) {
        if (written == RunPersistenceStoreWriteResult::Indeterminate) {
            enterBlockedIndeterminate();
            return result(RunPersistenceResultStatus::PersistenceIndeterminate,
                          step,
                          RunPersistenceTechnicalReason::StoreOutcomeUnknown,
                          durability == RunPersistenceDurability::Unchanged
                              ? RunPersistenceDurability::MayHaveChanged
                              : RunPersistenceDurability::Changed);
        }
        if (written == RunPersistenceStoreWriteResult::CapacityError) {
            return result(RunPersistenceResultStatus::CapacityExceeded, step,
                          RunPersistenceTechnicalReason::StoreCapacityError,
                          durability);
        }
        return result(RunPersistenceResultStatus::WriteFailed, step,
                      written == RunPersistenceStoreWriteResult::NotWritten
                          ? RunPersistenceTechnicalReason::StoreNotWritten
                          : RunPersistenceTechnicalReason::StoreWriteError,
                      durability);
    };

    if (periodic) {
        const auto slotWrite = store_.writeSlotExact(
            target, targetBytes, oldSlot, kMaximumCheckpointRecordBytes);
        if (slotWrite != RunPersistenceStoreWriteResult::Written) {
            if (slotWrite != RunPersistenceStoreWriteResult::Indeterminate)
                readyState();
            return writeFailure(slotWrite, RunPersistenceStep::CheckpointSlot,
                                RunPersistenceDurability::Unchanged);
        }
        slots_[target] = record;
        nextCheckpointRevision_ =
            record.checkpointRevision ==
                    std::numeric_limits<std::uint64_t>::max()
                ? 0U
                : record.checkpointRevision + 1U;
        const auto headWrite = store_.writeHeadExact(*committedBytes, oldHead,
                                                     kMaximumHeadRecordBytes);
        if (headWrite != RunPersistenceStoreWriteResult::Written) {
            if (headWrite != RunPersistenceStoreWriteResult::Indeterminate)
                readyState();
            return writeFailure(headWrite, RunPersistenceStep::CommittedHead,
                                RunPersistenceDurability::Changed);
        }
        committed.bytes = *committedBytes;
        currentHead_ = std::move(committed);
    } else {
        const auto preparedWrite = store_.writeHeadExact(
            *preparedBytes, oldHead, kMaximumHeadRecordBytes);
        if (preparedWrite != RunPersistenceStoreWriteResult::Written) {
            if (preparedWrite != RunPersistenceStoreWriteResult::Indeterminate)
                readyState();
            return writeFailure(preparedWrite, RunPersistenceStep::PreparedHead,
                                RunPersistenceDurability::Unchanged);
        }
        prepared.bytes = *preparedBytes;
        const auto slotWrite = store_.writeSlotExact(
            target, targetBytes, oldSlot, kMaximumCheckpointRecordBytes);
        if (slotWrite != RunPersistenceStoreWriteResult::Written) {
            enterBlockedIndeterminate();
            return writeFailure(slotWrite, RunPersistenceStep::CheckpointSlot,
                                RunPersistenceDurability::Changed);
        }
        slots_[target] = record;
        nextCheckpointRevision_ =
            record.checkpointRevision ==
                    std::numeric_limits<std::uint64_t>::max()
                ? 0U
                : record.checkpointRevision + 1U;
        const auto committedWrite = store_.writeHeadExact(
            *committedBytes, std::optional<std::string>{prepared.bytes},
            kMaximumHeadRecordBytes);
        if (committedWrite != RunPersistenceStoreWriteResult::Written) {
            enterBlockedIndeterminate();
            return writeFailure(committedWrite,
                                RunPersistenceStep::CommittedHead,
                                RunPersistenceDurability::Changed);
        }
        committed.bytes = *committedBytes;
        currentHead_ = std::move(committed);
    }
    slots_[target] = std::move(record);
    nextHeadRevision_ =
        currentHead_->revision == std::numeric_limits<std::uint64_t>::max()
            ? 0U
            : currentHead_->revision + 1U;
    state_ = snapshot.variant == RunCheckpointVariant::NoActiveRun
                 ? RunPersistenceCoordinatorState::ReadyEmpty
                 : RunPersistenceCoordinatorState::Ready;
    if (schedule_.confirm(snapshot.checkpointMonotonicMillis) !=
        RunCheckpointScheduleStatus::Success) {
        enterBlockedIndeterminate();
        return result(RunPersistenceResultStatus::TimeWentBackwards,
                      RunPersistenceStep::CommittedHead,
                      RunPersistenceTechnicalReason::InvalidProjection,
                      RunPersistenceDurability::Changed);
    }
    return result(periodic ? RunPersistenceResultStatus::CheckpointWritten
                           : RunPersistenceResultStatus::Applied,
                  RunPersistenceStep::CommittedHead,
                  RunPersistenceTechnicalReason::None,
                  RunPersistenceDurability::Changed);
}

RunPersistenceResult RunPersistenceCoordinator::persistCommand(
    RunCommandState& current, const CommandDecision& decision,
    const RunCheckpointTime& time) {
    if (state_ != RunPersistenceCoordinatorState::Ready &&
        state_ != RunPersistenceCoordinatorState::ReadyEmpty)
        return unavailableResult();
    if (!decision.proposed() || !isPersistedRunCommand(decision.kind))
        return result(decision.proposed()
                          ? RunPersistenceResultStatus::NotEligible
                          : RunPersistenceResultStatus::InvalidDecision);
    if (time.monotonicMillis != decision.envelope.monotonicMillis)
        return result(RunPersistenceResultStatus::TimeMismatch);
    for (std::size_t i = 0U; i < persistedIdCount_; ++i)
        if (persistedIds_[i] == decision.envelope.id)
            return result(RunPersistenceResultStatus::AlreadyPersisted);
    auto candidate = current;
    const auto apply = applyRunCommand(candidate, decision);
    if (apply != CommandStatus::Applied)
        return result(apply == CommandStatus::StaleState
                          ? RunPersistenceResultStatus::StaleDecision
                          : RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply);
    auto ids = persistedIds_;
    auto count = persistedIdCount_;
    if (count < ids.size())
        ids[count++] = decision.envelope.id;
    else {
        for (std::size_t i = 1U; i < ids.size(); ++i) ids[i - 1U] = ids[i];
        ids.back() = decision.envelope.id;
    }
    if (nextCheckpointRevision_ == 0U)
        return result(RunPersistenceResultStatus::CounterOverflow);
    const auto snapshot = makeRunPersistenceSnapshot(
        candidate, ids, count, RunCheckpointTrigger::Command, time,
        schedule_.intervalMinutes());
    if (!snapshot.has_value())
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    const auto persisted =
        writeSnapshot(*snapshot, time, false, current, decision.envelope.id);
    if (persisted.status != RunPersistenceResultStatus::Applied)
        return persisted;
    if (applyRunCommand(current, decision) != CommandStatus::Applied) {
        state_ =
            RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed;
        return result(
            RunPersistenceResultStatus::PersistenceCommittedApplyFailed,
            RunPersistenceStep::RamApply,
            RunPersistenceTechnicalReason::InvalidProjection,
            RunPersistenceDurability::Changed);
    }
    persistedIds_ = ids;
    persistedIdCount_ = count;
    RunPersistenceResult result{RunPersistenceResultStatus::Applied};
    result.step = RunPersistenceStep::RamApply;
    result.durability = RunPersistenceDurability::Changed;
    result.coordinatorState = state_;
    if (decision.effectCount > result.effects.size()) {
        state_ =
            RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed;
        return this->result(
            RunPersistenceResultStatus::PersistenceCommittedApplyFailed,
            RunPersistenceStep::RamApply,
            RunPersistenceTechnicalReason::InvalidProjection,
            RunPersistenceDurability::Changed);
    }
    result.effects = decision.effects;
    result.effectCount = decision.effectCount;
    return result;
}

RunPersistenceResult RunPersistenceCoordinator::persistTransition(
    RunCommandState& current, const TransitionDecision& decision,
    const RunCheckpointTime& time) {
    if (state_ != RunPersistenceCoordinatorState::Ready)
        return unavailableResult();
    if (!decision.proposed() || !eligibleTransition(decision.reason))
        return result(RunPersistenceResultStatus::InvalidDecision);
    if (time.monotonicMillis != decision.monotonicMillis)
        return result(RunPersistenceResultStatus::TimeMismatch);
    auto candidate = current;
    if (!candidate.processRunSnapshot.has_value() ||
        !applyProcessTransition(candidate.processState, decision,
                                &*candidate.processRunSnapshot))
        return result(RunPersistenceResultStatus::StaleDecision,
                      RunPersistenceStep::CandidateApply);
    if (decision.reason == TransitionReason::ProductWaitExpired)
        clearCandidateRun(candidate);
    const auto snapshot = makeRunPersistenceSnapshot(
        candidate, persistedIds_, persistedIdCount_,
        RunCheckpointTrigger::Transition, time, schedule_.intervalMinutes());
    if (!snapshot.has_value())
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    const auto persisted = writeSnapshot(*snapshot, time, false, current);
    if (persisted.status != RunPersistenceResultStatus::Applied)
        return persisted;
    if (!current.processRunSnapshot.has_value() ||
        !applyProcessTransition(current.processState, decision,
                                &*current.processRunSnapshot)) {
        state_ =
            RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed;
        return result(
            RunPersistenceResultStatus::PersistenceCommittedApplyFailed,
            RunPersistenceStep::RamApply,
            RunPersistenceTechnicalReason::InvalidProjection,
            RunPersistenceDurability::Changed);
    }
    if (decision.reason == TransitionReason::ProductWaitExpired)
        clearCandidateRun(current);
    RunPersistenceResult result{RunPersistenceResultStatus::Applied};
    result.step = RunPersistenceStep::RamApply;
    result.durability = RunPersistenceDurability::Changed;
    result.coordinatorState = state_;
    if (decision.messageCount > result.messages.size()) {
        state_ =
            RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed;
        return this->result(
            RunPersistenceResultStatus::PersistenceCommittedApplyFailed,
            RunPersistenceStep::RamApply,
            RunPersistenceTechnicalReason::InvalidProjection,
            RunPersistenceDurability::Changed);
    }
    result.messages = decision.messages;
    result.messageCount = decision.messageCount;
    return result;
}

RunPersistenceResult RunPersistenceCoordinator::checkpointPeriodic(
    const RunCommandState& current, const RunCheckpointTime& time) {
    if (state_ == RunPersistenceCoordinatorState::ReadyEmpty) {
        return result(RunPersistenceResultStatus::NoActiveRun);
    }
    if (state_ != RunPersistenceCoordinatorState::Ready)
        return unavailableResult();
    if (!current.activeProgramRun.has_value() &&
        !current.activeManualRun.has_value())
        return result(RunPersistenceResultStatus::NoActiveRun);
    if (!currentHead_.has_value() ||
        !slots_[currentHead_->current.slot].has_value()) {
        return result(RunPersistenceResultStatus::StaleDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    const auto& confirmed = slots_[currentHead_->current.slot]->snapshot;
    const auto expected = makeRunPersistenceSnapshot(
        current, persistedIds_, persistedIdCount_, confirmed.trigger,
        RunCheckpointTime{confirmed.checkpointMonotonicMillis, std::nullopt},
        confirmed.intervalMinutes);
    std::string expectedBytes;
    std::string confirmedBytes;
    if (!expected.has_value() ||
        encodeRunPersistenceSnapshot(*expected, expectedBytes) !=
            RunPersistenceCodecStatus::Success ||
        encodeRunPersistenceSnapshot(confirmed, confirmedBytes) !=
            RunPersistenceCodecStatus::Success ||
        expectedBytes != confirmedBytes) {
        return result(RunPersistenceResultStatus::StaleDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    const auto due = schedule_.due(time.monotonicMillis);
    if (due == RunCheckpointScheduleStatus::NotDue)
        return result(RunPersistenceResultStatus::NotDue);
    if (due != RunCheckpointScheduleStatus::Success)
        return result(RunPersistenceResultStatus::TimeWentBackwards);
    const auto snapshot = makeRunPersistenceSnapshot(
        current, persistedIds_, persistedIdCount_,
        RunCheckpointTrigger::Periodic, time, schedule_.intervalMinutes());
    return snapshot.has_value()
               ? writeSnapshot(*snapshot, time, true, current)
               : result(RunPersistenceResultStatus::InvalidDecision,
                        RunPersistenceStep::CandidateApply,
                        RunPersistenceTechnicalReason::InvalidProjection);
}

}  // namespace fermentation
