#include "run_persistence_coordinator.hpp"

#include <limits>
#include <utility>

#include "big_endian_codec.hpp"
#include "byte_buffer.hpp"
#include "crc32.hpp"
#include "run_persistence_codec.hpp"
#include "storage_envelope.hpp"

namespace fermentation {
namespace {

using device_platform::ByteReader;
using device_platform::ByteWriter;
namespace be = device_platform::big_endian;

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

namespace {

bool writeReference(ByteWriter& writer, const RunCheckpointReference& ref) {
    return be::writeUint8(writer, ref.slot) &&
           be::writeUint64(writer, ref.checkpointRevision) &&
           be::writeUint32(writer, ref.payloadLength) &&
           be::writeUint32(writer, ref.payloadCrc) &&
           be::writeUint8(writer, static_cast<std::uint8_t>(ref.variant));
}

bool readReference(ByteReader& reader, RunCheckpointReference& ref) {
    std::uint8_t variant = 0U;
    if (!be::readUint8(reader, ref.slot) || ref.slot > 1U ||
        !be::readUint64(reader, ref.checkpointRevision) ||
        !be::readUint32(reader, ref.payloadLength) ||
        !be::readUint32(reader, ref.payloadCrc) ||
        !be::readUint8(reader, variant) || ref.checkpointRevision == 0U)
        return false;
    switch (variant) {
        case 1U:
            ref.variant = RunCheckpointVariant::ProgramRun;
            return true;
        case 2U:
            ref.variant = RunCheckpointVariant::ManualRun;
            return true;
        case 3U:
            ref.variant = RunCheckpointVariant::NoActiveRun;
            return true;
        default:
            return false;
    }
}

std::optional<std::string> encodeHead(const RunPersistenceHead& head,
                                      device_platform::StorageEpoch epoch) {
    ByteWriter payload(80U);
    bool ok =
        be::writeUint8(payload, static_cast<std::uint8_t>(head.state)) &&
        writeReference(payload, head.current) &&
        be::writeOptionalTag(payload, head.fallback.has_value()) &&
        (!head.fallback.has_value() || writeReference(payload, *head.fallback));
    if (!ok) return std::nullopt;
    device_platform::StorageEnvelope envelope{
        kHeadRecordType, kRunPersistenceSchema, epoch,
        head.revision,   std::nullopt,          payload.takeBytes()};
    std::string bytes;
    if (device_platform::encodeEnvelope(envelope, bytes,
                                        kMaximumHeadRecordBytes) !=
        device_platform::EnvelopeEncodeStatus::Success)
        return std::nullopt;
    return bytes;
}

std::optional<RunPersistenceHead> decodeHead(
    const std::string& bytes, device_platform::StorageEpoch epoch) {
    const auto envelope = device_platform::decodeEnvelope(bytes);
    if (!envelope.envelope.has_value() ||
        envelope.envelope->recordTypeId != kHeadRecordType ||
        envelope.envelope->schemaVersion != kRunPersistenceSchema ||
        envelope.envelope->storageEpoch != epoch ||
        envelope.envelope->versionValue == 0U)
        return std::nullopt;
    ByteReader reader(envelope.envelope->payload);
    std::uint8_t state = 0U;
    bool fallback = false;
    RunPersistenceHead h;
    h.revision = envelope.envelope->versionValue;
    if (!be::readUint8(reader, state) || !readReference(reader, h.current) ||
        !be::readOptionalTag(reader, fallback))
        return std::nullopt;
    if (state == 1U)
        h.state = RunPersistenceHeadState::Prepared;
    else if (state == 2U)
        h.state = RunPersistenceHeadState::Committed;
    else
        return std::nullopt;
    if (fallback) {
        RunCheckpointReference ref;
        if (!readReference(reader, ref)) return std::nullopt;
        h.fallback = ref;
    }
    if (reader.remaining() != 0U) return std::nullopt;
    h.bytes = bytes;
    return h;
}

std::optional<RunPersistenceRawRecord> decodeRecord(
    const std::string& bytes, device_platform::StorageEpoch epoch) {
    const auto envelope = device_platform::decodeEnvelope(bytes);
    if (!envelope.envelope.has_value() ||
        envelope.envelope->recordTypeId != kCheckpointRecordType ||
        envelope.envelope->schemaVersion != kRunPersistenceSchema ||
        envelope.envelope->storageEpoch != epoch ||
        envelope.envelope->versionValue == 0U)
        return std::nullopt;
    const auto snapshot =
        decodeRunPersistenceSnapshot(envelope.envelope->payload);
    if (!snapshot.snapshot.has_value()) return std::nullopt;
    return RunPersistenceRawRecord{bytes, *snapshot.snapshot,
                                   envelope.envelope->versionValue,
                                   envelope.envelope->utcUnixSeconds};
}

bool matches(const RunCheckpointReference& ref,
             const RunPersistenceRawRecord& record, std::size_t slot) {
    const auto envelope = device_platform::decodeEnvelope(record.bytes);
    return envelope.envelope.has_value() && ref.slot == slot &&
           ref.checkpointRevision == record.checkpointRevision &&
           ref.payloadLength == envelope.envelope->payload.size() &&
           ref.payloadCrc == device_platform::computeCrc32IsoHdlc(
                                 envelope.envelope->payload) &&
           ref.variant == record.snapshot.variant;
}

RunCheckpointReference referenceFor(std::size_t slot,
                                    const RunPersistenceRawRecord& record) {
    const auto envelope = device_platform::decodeEnvelope(record.bytes);
    return {static_cast<std::uint8_t>(slot), record.checkpointRevision,
            static_cast<std::uint32_t>(envelope.envelope->payload.size()),
            device_platform::computeCrc32IsoHdlc(envelope.envelope->payload),
            record.snapshot.variant};
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
    state_ = RunPersistenceCoordinatorState::Uninitialized;
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
    auto head = decodeHead(read.value, epoch_);
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
        auto record = decodeRecord(slotRead.value, epoch_);
        if (!record.has_value() || !matches(reference, *record, slot)) {
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
        if (schedule_.confirm(snap.checkpointMonotonicMillis) !=
            RunCheckpointScheduleStatus::Success) {
            enterBlockedIndeterminate();
            return {RunPersistenceLoadStatus::NotReconstructible, std::nullopt};
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
    bool periodic) {
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
    const auto ref = referenceFor(target, record);
    const auto oldHead = currentHead_.has_value()
                             ? std::optional<std::string>{currentHead_->bytes}
                             : std::nullopt;
    const auto oldSlot = slots_[target].has_value()
                             ? std::optional<std::string>{slots_[target]->bytes}
                             : std::nullopt;

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
                          RunPersistenceDurability::MayHaveChanged);
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
        RunPersistenceHead committed;
        committed.state = RunPersistenceHeadState::Committed;
        committed.revision = nextHeadRevision_;
        committed.current = ref;
        if (snapshot.variant != RunCheckpointVariant::NoActiveRun &&
            currentHead_.has_value()) {
            committed.fallback = currentHead_->current;
        }
        const auto bytes = encodeHead(committed, epoch_);
        if (!bytes.has_value()) {
            readyState();
            return result(RunPersistenceResultStatus::CapacityExceeded,
                          RunPersistenceStep::CommittedHead,
                          RunPersistenceTechnicalReason::CodecError,
                          RunPersistenceDurability::Unchanged);
        }
        const auto headWrite =
            store_.writeHeadExact(*bytes, oldHead, kMaximumHeadRecordBytes);
        if (headWrite != RunPersistenceStoreWriteResult::Written) {
            if (headWrite != RunPersistenceStoreWriteResult::Indeterminate)
                readyState();
            return writeFailure(headWrite, RunPersistenceStep::CommittedHead,
                                RunPersistenceDurability::Changed);
        }
        committed.bytes = *bytes;
        currentHead_ = std::move(committed);
    } else {
        RunPersistenceHead prepared;
        prepared.state = RunPersistenceHeadState::Prepared;
        prepared.revision = nextHeadRevision_;
        prepared.current = ref;
        if (snapshot.variant != RunCheckpointVariant::NoActiveRun &&
            currentHead_.has_value()) {
            prepared.fallback = currentHead_->current;
        }
        const auto preparedBytes = encodeHead(prepared, epoch_);
        if (!preparedBytes.has_value()) {
            readyState();
            return result(RunPersistenceResultStatus::CapacityExceeded,
                          RunPersistenceStep::PreparedHead,
                          RunPersistenceTechnicalReason::CodecError);
        }
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
        RunPersistenceHead committed = prepared;
        committed.state = RunPersistenceHeadState::Committed;
        ++committed.revision;
        if (snapshot.variant == RunCheckpointVariant::NoActiveRun) {
            committed.fallback.reset();
        }
        const auto committedBytes = encodeHead(committed, epoch_);
        if (!committedBytes.has_value()) {
            enterBlockedIndeterminate();
            return result(RunPersistenceResultStatus::PersistenceIndeterminate,
                          RunPersistenceStep::CommittedHead,
                          RunPersistenceTechnicalReason::CodecError,
                          RunPersistenceDurability::MayHaveChanged);
        }
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
    nextCheckpointRevision_ =
        record.checkpointRevision == std::numeric_limits<std::uint64_t>::max()
            ? 0U
            : record.checkpointRevision + 1U;
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
    const auto persisted = writeSnapshot(*snapshot, time, false);
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
    const auto persisted = writeSnapshot(*snapshot, time, false);
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
        current.activeRunId !=
            slots_[currentHead_->current.slot]->snapshot.activeRunId ||
        current.runRevision !=
            slots_[currentHead_->current.slot]->snapshot.runRevision) {
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
               ? writeSnapshot(*snapshot, time, true)
               : result(RunPersistenceResultStatus::InvalidDecision,
                        RunPersistenceStep::CandidateApply,
                        RunPersistenceTechnicalReason::InvalidProjection);
}

}  // namespace fermentation
