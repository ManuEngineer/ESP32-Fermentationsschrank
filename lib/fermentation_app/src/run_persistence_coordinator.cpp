#include "run_persistence_coordinator.hpp"

#include <limits>
#include <utility>

#include "big_endian_codec.hpp"
#include "byte_buffer.hpp"
#include "crc32.hpp"
#include "run_persistence_codec.hpp"
#include "state_store_key.hpp"
#include "storage_envelope.hpp"

namespace fermentation {
namespace {

using device_platform::ByteReader;
using device_platform::ByteWriter;
namespace be = device_platform::big_endian;

constexpr std::uint32_t kRunPersistenceSchema = 1U;
constexpr device_platform::RecordTypeId kCheckpointRecordType{7U};
constexpr device_platform::RecordTypeId kHeadRecordType{8U};
constexpr std::size_t kMaximumCheckpointRecordBytes = 65581U;
constexpr std::size_t kMaximumHeadRecordBytes = 256U;

const device_platform::StateStoreKey& slotKey(std::size_t slot) {
    static const auto rc0 = *device_platform::StateStoreKey::create("rc0").key;
    static const auto rc1 = *device_platform::StateStoreKey::create("rc1").key;
    return slot == 0U ? rc0 : rc1;
}
const device_platform::StateStoreKey& headKey() {
    static const auto rh0 = *device_platform::StateStoreKey::create("rh0").key;
    return rh0;
}

enum class ExactWrite { Written, NotWritten, WriteFailed, CapacityExceeded, Indeterminate };

ExactWrite writeExact(device_platform::IStateStore& store,
                      const device_platform::StateStoreKey& key,
                      const std::string& bytes,
                      const std::optional<std::string>& old,
                      std::size_t limit) {
    const auto status = store.write(key, bytes);
    if (status == device_platform::StateStoreWriteStatus::Success) return ExactWrite::Written;
    if (status == device_platform::StateStoreWriteStatus::WriteError) return ExactWrite::WriteFailed;
    if (status == device_platform::StateStoreWriteStatus::CapacityError) return ExactWrite::CapacityExceeded;
    const auto read = store.read(key, limit);
    if (read.status == device_platform::StateStoreReadStatus::Success) {
        if (read.value == bytes) return ExactWrite::Written;
        if (old.has_value() && read.value == *old) return ExactWrite::NotWritten;
        return ExactWrite::Indeterminate;
    }
    if (read.status == device_platform::StateStoreReadStatus::NotFound && !old.has_value()) return ExactWrite::NotWritten;
    return ExactWrite::Indeterminate;
}

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

bool writeReference(ByteWriter& writer,
                    const RunPersistenceCoordinator::RecordReference& ref) {
    return be::writeUint8(writer, ref.slot) &&
           be::writeUint64(writer, ref.checkpointRevision) &&
           be::writeUint32(writer, ref.payloadLength) &&
           be::writeUint32(writer, ref.payloadCrc) &&
           be::writeUint8(writer, static_cast<std::uint8_t>(ref.variant));
}

bool readReference(ByteReader& reader,
                   RunPersistenceCoordinator::RecordReference& ref) {
    std::uint8_t variant = 0U;
    if (!be::readUint8(reader, ref.slot) || ref.slot > 1U ||
        !be::readUint64(reader, ref.checkpointRevision) ||
        !be::readUint32(reader, ref.payloadLength) ||
        !be::readUint32(reader, ref.payloadCrc) ||
        !be::readUint8(reader, variant) || ref.checkpointRevision == 0U) return false;
    switch (variant) {
        case 1U: ref.variant = RunCheckpointVariant::ProgramRun; return true;
        case 2U: ref.variant = RunCheckpointVariant::ManualRun; return true;
        case 3U: ref.variant = RunCheckpointVariant::NoActiveRun; return true;
        default: return false;
    }
}

std::optional<std::string> encodeHead(const RunPersistenceCoordinator::Head& head,
                                      device_platform::StorageEpoch epoch) {
    ByteWriter payload(80U);
    bool ok = be::writeUint8(payload, static_cast<std::uint8_t>(head.state)) &&
              writeReference(payload, head.current) &&
              be::writeOptionalTag(payload, head.fallback.has_value()) &&
              (!head.fallback.has_value() || writeReference(payload, *head.fallback));
    if (!ok) return std::nullopt;
    device_platform::StorageEnvelope envelope{kHeadRecordType, kRunPersistenceSchema,
                                               epoch, head.revision, std::nullopt,
                                               payload.takeBytes()};
    std::string bytes;
    if (device_platform::encodeEnvelope(envelope, bytes, kMaximumHeadRecordBytes) !=
        device_platform::EnvelopeEncodeStatus::Success) return std::nullopt;
    return bytes;
}

std::optional<RunPersistenceCoordinator::Head> decodeHead(
    const std::string& bytes, device_platform::StorageEpoch epoch) {
    const auto envelope = device_platform::decodeEnvelope(bytes);
    if (!envelope.envelope.has_value() ||
        envelope.envelope->recordTypeId != kHeadRecordType ||
        envelope.envelope->schemaVersion != kRunPersistenceSchema ||
        envelope.envelope->storageEpoch != epoch || envelope.envelope->versionValue == 0U) return std::nullopt;
    ByteReader reader(envelope.envelope->payload);
    std::uint8_t state=0U; bool fallback=false;
    RunPersistenceCoordinator::Head h;
    h.revision = envelope.envelope->versionValue;
    if (!be::readUint8(reader,state) || !readReference(reader,h.current) ||
        !be::readOptionalTag(reader,fallback)) return std::nullopt;
    if (state == 1U) h.state = RunPersistenceCoordinator::Head::State::Prepared;
    else if (state == 2U) h.state = RunPersistenceCoordinator::Head::State::Committed;
    else return std::nullopt;
    if (fallback) { RunPersistenceCoordinator::RecordReference ref; if(!readReference(reader,ref)) return std::nullopt; h.fallback=ref; }
    if (reader.remaining()!=0U) return std::nullopt;
    h.bytes=bytes; return h;
}

std::optional<RunPersistenceCoordinator::RawRecord> decodeRecord(
    const std::string& bytes, device_platform::StorageEpoch epoch) {
    const auto envelope=device_platform::decodeEnvelope(bytes);
    if (!envelope.envelope.has_value() || envelope.envelope->recordTypeId != kCheckpointRecordType ||
        envelope.envelope->schemaVersion != kRunPersistenceSchema || envelope.envelope->storageEpoch != epoch ||
        envelope.envelope->versionValue == 0U) return std::nullopt;
    const auto snapshot=decodeRunPersistenceSnapshot(envelope.envelope->payload);
    if (!snapshot.snapshot.has_value() || snapshot.snapshot->checkpointRevision != envelope.envelope->versionValue ||
        snapshot.snapshot->checkpointUtcUnixSeconds != envelope.envelope->utcUnixSeconds) return std::nullopt;
    return RunPersistenceCoordinator::RawRecord{bytes,*snapshot.snapshot};
}

bool matches(const RunPersistenceCoordinator::RecordReference& ref,
             const RunPersistenceCoordinator::RawRecord& record,
             std::size_t slot) {
    const auto envelope=device_platform::decodeEnvelope(record.bytes);
    return envelope.envelope.has_value() && ref.slot==slot &&
           ref.checkpointRevision==record.snapshot.checkpointRevision &&
           ref.payloadLength==envelope.envelope->payload.size() &&
           ref.payloadCrc==device_platform::computeCrc32IsoHdlc(envelope.envelope->payload) &&
           ref.variant==record.snapshot.variant;
}

RunPersistenceCoordinator::RecordReference referenceFor(
    std::size_t slot, const RunPersistenceCoordinator::RawRecord& record) {
    const auto envelope=device_platform::decodeEnvelope(record.bytes);
    return {static_cast<std::uint8_t>(slot), record.snapshot.checkpointRevision,
            static_cast<std::uint32_t>(envelope.envelope->payload.size()),
            device_platform::computeCrc32IsoHdlc(envelope.envelope->payload),
            record.snapshot.variant};
}

}  // namespace

RunPersistenceCoordinator::RunPersistenceCoordinator(
    device_platform::IStateStore& store, device_platform::StorageEpoch epoch,
    RunCheckpointSchedule schedule) noexcept
    : store_(store), epoch_(epoch), schedule_(std::move(schedule)) {}

void RunPersistenceCoordinator::enterBlockedIndeterminate() {
    state_ = RunPersistenceCoordinatorState::BlockedIndeterminate;
}

RunPersistenceResult RunPersistenceCoordinator::unavailableResult() const {
    if (state_ == RunPersistenceCoordinatorState::Uninitialized) return {RunPersistenceResultStatus::NotInitialized};
    if (state_ == RunPersistenceCoordinatorState::Busy) return {RunPersistenceResultStatus::Busy};
    if (state_ == RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed) return {RunPersistenceResultStatus::PersistenceCommittedApplyFailed};
    if (state_ == RunPersistenceCoordinatorState::LoadedActiveRun) return {RunPersistenceResultStatus::RecoveryPending};
    return {RunPersistenceResultStatus::Blocked};
}

RunPersistenceLoadResult RunPersistenceCoordinator::loadAndInitialize() {
    state_ = RunPersistenceCoordinatorState::Uninitialized;
    currentHead_.reset(); slots_[0].reset(); slots_[1].reset(); persistedIdCount_=0U;
    const auto readSlot = [this](std::size_t slot) -> RunPersistenceLoadStatus {
        const auto read=store_.read(slotKey(slot),kMaximumCheckpointRecordBytes);
        if(read.status==device_platform::StateStoreReadStatus::NotFound) return RunPersistenceLoadStatus::NoPersistedRun;
        if(read.status==device_platform::StateStoreReadStatus::CapacityError) return RunPersistenceLoadStatus::CapacityExceeded;
        if(read.status!=device_platform::StateStoreReadStatus::Success) return RunPersistenceLoadStatus::ReadFailed;
        auto decoded=decodeRecord(read.value,epoch_); if(!decoded.has_value()) return RunPersistenceLoadStatus::NotReconstructible;
        slots_[slot]=std::move(*decoded); return RunPersistenceLoadStatus::Current;
    };
    const auto s0=readSlot(0U), s1=readSlot(1U);
    if (s0==RunPersistenceLoadStatus::ReadFailed || s1==RunPersistenceLoadStatus::ReadFailed) { enterBlockedIndeterminate(); return {RunPersistenceLoadStatus::ReadFailed,std::nullopt}; }
    if (s0==RunPersistenceLoadStatus::CapacityExceeded || s1==RunPersistenceLoadStatus::CapacityExceeded) { enterBlockedIndeterminate(); return {RunPersistenceLoadStatus::CapacityExceeded,std::nullopt}; }
    const auto read=store_.read(headKey(),kMaximumHeadRecordBytes);
    if(read.status==device_platform::StateStoreReadStatus::NotFound) {
        if(!slots_[0].has_value() && !slots_[1].has_value()) { state_=RunPersistenceCoordinatorState::ReadyEmpty; return {RunPersistenceLoadStatus::NoPersistedRun,std::nullopt}; }
        enterBlockedIndeterminate(); return {RunPersistenceLoadStatus::NotReconstructibleOrphanedState,std::nullopt};
    }
    if(read.status==device_platform::StateStoreReadStatus::CapacityError) { enterBlockedIndeterminate(); return {RunPersistenceLoadStatus::CapacityExceeded,std::nullopt}; }
    if(read.status!=device_platform::StateStoreReadStatus::Success) { enterBlockedIndeterminate(); return {RunPersistenceLoadStatus::ReadFailed,std::nullopt}; }
    auto head=decodeHead(read.value,epoch_); if(!head.has_value()) { enterBlockedIndeterminate(); return {RunPersistenceLoadStatus::NotReconstructible,std::nullopt}; }
    currentHead_=std::move(*head); nextHeadRevision_=currentHead_->revision==std::numeric_limits<std::uint64_t>::max()?0U:currentHead_->revision+1U;
    if(currentHead_->state==Head::State::Prepared) { enterBlockedIndeterminate(); return {RunPersistenceLoadStatus::PreparedInterrupted,std::nullopt}; }
    const auto currentSlot=currentHead_->current.slot;
    if(slots_[currentSlot].has_value() && matches(currentHead_->current,*slots_[currentSlot],currentSlot)) {
        const auto& snap=slots_[currentSlot]->snapshot;
        nextCheckpointRevision_=snap.checkpointRevision==std::numeric_limits<std::uint64_t>::max()?0U:snap.checkpointRevision+1U;
        persistedIds_=snap.persistedRunCommandIds; persistedIdCount_=snap.persistedRunCommandCount;
        if(snap.variant==RunCheckpointVariant::NoActiveRun) { state_=RunPersistenceCoordinatorState::ReadyEmpty; return {RunPersistenceLoadStatus::NoActiveRun,snap}; }
        state_=RunPersistenceCoordinatorState::LoadedActiveRun; return {RunPersistenceLoadStatus::Current,snap};
    }
    if(currentHead_->fallback.has_value()) {
        const auto slot=currentHead_->fallback->slot;
        if(slots_[slot].has_value() && matches(*currentHead_->fallback,*slots_[slot],slot)) { enterBlockedIndeterminate(); return {RunPersistenceLoadStatus::FallbackRecovered,slots_[slot]->snapshot}; }
    }
    enterBlockedIndeterminate(); return {RunPersistenceLoadStatus::NotReconstructible,std::nullopt};
}

RunPersistenceResult RunPersistenceCoordinator::writeSnapshot(const RunPersistenceSnapshot& snapshot, bool periodic) {
    if (state_!=RunPersistenceCoordinatorState::Ready && state_!=RunPersistenceCoordinatorState::ReadyEmpty) return unavailableResult();
    if(nextCheckpointRevision_==0U || nextHeadRevision_==0U || nextHeadRevision_==std::numeric_limits<std::uint64_t>::max()) return {RunPersistenceResultStatus::CounterOverflow};
    state_=RunPersistenceCoordinatorState::Busy;
    const std::size_t target = currentHead_.has_value() ? 1U-currentHead_->current.slot : 0U;
    std::string payload;
    if(encodeRunPersistenceSnapshot(snapshot,payload)!=RunPersistenceCodecStatus::Success) { state_=currentHead_.has_value()?RunPersistenceCoordinatorState::Ready:RunPersistenceCoordinatorState::ReadyEmpty; return {RunPersistenceResultStatus::InvalidDecision}; }
    device_platform::StorageEnvelope env{kCheckpointRecordType,kRunPersistenceSchema,epoch_,snapshot.checkpointRevision,snapshot.checkpointUtcUnixSeconds,payload};
    std::string targetBytes;
    if(device_platform::encodeEnvelope(env,targetBytes,kMaximumCheckpointRecordBytes)!=device_platform::EnvelopeEncodeStatus::Success) { state_=currentHead_.has_value()?RunPersistenceCoordinatorState::Ready:RunPersistenceCoordinatorState::ReadyEmpty; return {RunPersistenceResultStatus::CapacityExceeded}; }
    RawRecord record{targetBytes,snapshot};
    const auto ref=referenceFor(target,record);
    Head prepared; prepared.state=Head::State::Prepared; prepared.revision=nextHeadRevision_; prepared.current=ref;
    if(currentHead_.has_value()) prepared.fallback=currentHead_->current;
    const auto preparedBytes=encodeHead(prepared,epoch_); if(!preparedBytes.has_value()) { state_=RunPersistenceCoordinatorState::Ready; return {RunPersistenceResultStatus::CapacityExceeded}; }
    const auto oldHead=currentHead_.has_value()?std::optional<std::string>{currentHead_->bytes}:std::nullopt;
    auto written=writeExact(store_,headKey(),*preparedBytes,oldHead,kMaximumHeadRecordBytes);
    if(written==ExactWrite::Indeterminate) { enterBlockedIndeterminate(); return {RunPersistenceResultStatus::PersistenceIndeterminate}; }
    if(written==ExactWrite::CapacityExceeded) { state_=currentHead_.has_value()?RunPersistenceCoordinatorState::Ready:RunPersistenceCoordinatorState::ReadyEmpty; return {RunPersistenceResultStatus::CapacityExceeded}; }
    if(written!=ExactWrite::Written) { state_=currentHead_.has_value()?RunPersistenceCoordinatorState::Ready:RunPersistenceCoordinatorState::ReadyEmpty; return {RunPersistenceResultStatus::WriteFailed}; }
    prepared.bytes=*preparedBytes;
    const auto oldSlot=slots_[target].has_value()?std::optional<std::string>{slots_[target]->bytes}:std::nullopt;
    written=writeExact(store_,slotKey(target),targetBytes,oldSlot,kMaximumCheckpointRecordBytes);
    if(written==ExactWrite::Indeterminate) { enterBlockedIndeterminate(); return {RunPersistenceResultStatus::PersistenceIndeterminate}; }
    if(written==ExactWrite::CapacityExceeded) { state_=RunPersistenceCoordinatorState::Ready; return {RunPersistenceResultStatus::CapacityExceeded}; }
    if(written!=ExactWrite::Written) { state_=RunPersistenceCoordinatorState::Ready; return {RunPersistenceResultStatus::WriteFailed}; }
    Head committed=prepared; committed.state=Head::State::Committed; ++committed.revision;
    const auto committedBytes=encodeHead(committed,epoch_); if(!committedBytes.has_value()) { enterBlockedIndeterminate(); return {RunPersistenceResultStatus::PersistenceIndeterminate}; }
    written=writeExact(store_,headKey(),*committedBytes,std::optional<std::string>{prepared.bytes},kMaximumHeadRecordBytes);
    if(written==ExactWrite::Indeterminate) { enterBlockedIndeterminate(); return {RunPersistenceResultStatus::PersistenceIndeterminate}; }
    if(written==ExactWrite::CapacityExceeded) { state_=RunPersistenceCoordinatorState::Ready; return {RunPersistenceResultStatus::CapacityExceeded}; }
    if(written!=ExactWrite::Written) { state_=RunPersistenceCoordinatorState::Ready; return {RunPersistenceResultStatus::WriteFailed}; }
    committed.bytes=*committedBytes; slots_[target]=std::move(record); currentHead_=std::move(committed);
    nextCheckpointRevision_ = snapshot.checkpointRevision==std::numeric_limits<std::uint64_t>::max()?0U:snapshot.checkpointRevision+1U;
    nextHeadRevision_ = currentHead_->revision==std::numeric_limits<std::uint64_t>::max()?0U:currentHead_->revision+1U;
    state_=snapshot.variant==RunCheckpointVariant::NoActiveRun?RunPersistenceCoordinatorState::ReadyEmpty:RunPersistenceCoordinatorState::Ready;
    if(schedule_.confirm(snapshot.checkpointMonotonicMillis)!=RunCheckpointScheduleStatus::Success) { enterBlockedIndeterminate(); return {RunPersistenceResultStatus::TimeWentBackwards}; }
    return {periodic?RunPersistenceResultStatus::CheckpointWritten:RunPersistenceResultStatus::Applied};
}

RunPersistenceResult RunPersistenceCoordinator::persistCommand(
    RunCommandState& current,const CommandDecision& decision,const RunCheckpointTime& time) {
    if(state_!=RunPersistenceCoordinatorState::Ready && state_!=RunPersistenceCoordinatorState::ReadyEmpty) return unavailableResult();
    if(!decision.proposed() || !isPersistedRunCommand(decision.kind)) return {decision.proposed()?RunPersistenceResultStatus::NotEligible:RunPersistenceResultStatus::InvalidDecision};
    if(time.monotonicMillis!=decision.envelope.monotonicMillis) return {RunPersistenceResultStatus::TimeMismatch};
    for(std::size_t i=0U;i<persistedIdCount_;++i) if(persistedIds_[i]==decision.envelope.id) return {RunPersistenceResultStatus::AlreadyPersisted};
    auto candidate=current; const auto apply=applyRunCommand(candidate,decision);
    if(apply!=CommandStatus::Applied) return {apply==CommandStatus::StaleState?RunPersistenceResultStatus::StaleDecision:RunPersistenceResultStatus::InvalidDecision};
    auto ids=persistedIds_; auto count=persistedIdCount_;
    if(count<ids.size()) ids[count++]=decision.envelope.id; else { for(std::size_t i=1U;i<ids.size();++i) ids[i-1U]=ids[i]; ids.back()=decision.envelope.id; }
    if(nextCheckpointRevision_==0U) return {RunPersistenceResultStatus::CounterOverflow};
    const auto snapshot=makeRunPersistenceSnapshot(candidate,ids,count,RunCheckpointTrigger::Command,nextCheckpointRevision_,time,schedule_.intervalMinutes());
    if(!snapshot.has_value()) return {RunPersistenceResultStatus::InvalidDecision};
    const auto persisted=writeSnapshot(*snapshot,false); if(persisted.status!=RunPersistenceResultStatus::Applied) return persisted;
    if(applyRunCommand(current,decision)!=CommandStatus::Applied) { state_=RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed; return {RunPersistenceResultStatus::PersistenceCommittedApplyFailed}; }
    persistedIds_=ids; persistedIdCount_=count; RunPersistenceResult result{RunPersistenceResultStatus::Applied}; result.effects=decision.effects; result.effectCount=decision.effectCount; return result;
}

RunPersistenceResult RunPersistenceCoordinator::persistTransition(
    RunCommandState& current,const TransitionDecision& decision,const RunCheckpointTime& time) {
    if(state_!=RunPersistenceCoordinatorState::Ready) return unavailableResult();
    if(!decision.proposed() || !eligibleTransition(decision.reason)) return {RunPersistenceResultStatus::InvalidDecision};
    if(time.monotonicMillis!=decision.monotonicMillis) return {RunPersistenceResultStatus::TimeMismatch};
    auto candidate=current;
    if(!candidate.processRunSnapshot.has_value() || !applyProcessTransition(candidate.processState,decision,&*candidate.processRunSnapshot)) return {RunPersistenceResultStatus::StaleDecision};
    if(decision.reason==TransitionReason::ProductWaitExpired) clearCandidateRun(candidate);
    const auto snapshot=makeRunPersistenceSnapshot(candidate,persistedIds_,persistedIdCount_,RunCheckpointTrigger::Transition,nextCheckpointRevision_,time,schedule_.intervalMinutes());
    if(!snapshot.has_value()) return {RunPersistenceResultStatus::InvalidDecision};
    const auto persisted=writeSnapshot(*snapshot,false); if(persisted.status!=RunPersistenceResultStatus::Applied) return persisted;
    if(!current.processRunSnapshot.has_value() || !applyProcessTransition(current.processState,decision,&*current.processRunSnapshot)) { state_=RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed; return {RunPersistenceResultStatus::PersistenceCommittedApplyFailed}; }
    if(decision.reason==TransitionReason::ProductWaitExpired) clearCandidateRun(current);
    RunPersistenceResult result{RunPersistenceResultStatus::Applied}; result.messages=decision.messages; result.messageCount=decision.messageCount; return result;
}

RunPersistenceResult RunPersistenceCoordinator::checkpointPeriodic(
    const RunCommandState& current,const RunCheckpointTime& time) {
    if(state_!=RunPersistenceCoordinatorState::Ready) return unavailableResult();
    if(!current.activeProgramRun.has_value() && !current.activeManualRun.has_value()) return {RunPersistenceResultStatus::CheckpointWritten};
    const auto due=schedule_.due(time.monotonicMillis);
    if(due==RunCheckpointScheduleStatus::NotDue) return {RunPersistenceResultStatus::CheckpointWritten};
    if(due!=RunCheckpointScheduleStatus::Success) return {RunPersistenceResultStatus::TimeWentBackwards};
    const auto snapshot=makeRunPersistenceSnapshot(current,persistedIds_,persistedIdCount_,RunCheckpointTrigger::Periodic,nextCheckpointRevision_,time,schedule_.intervalMinutes());
    return snapshot.has_value()?writeSnapshot(*snapshot,true):RunPersistenceResult{RunPersistenceResultStatus::InvalidDecision};
}

}  // namespace fermentation
