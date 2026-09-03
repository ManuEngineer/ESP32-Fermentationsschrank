#include "run_persistence_coordinator.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "run_persistence_codec.hpp"
#include "run_progress_weighting.hpp"
#include "run_recovery_time.hpp"
#include "boot_classification.hpp"
#include "control_context.hpp"
#include "sensor_selection.hpp"
#include "storage_envelope.hpp"

#if defined(APP_ISSUE_29_BRINGUP_PROBE)
#include "issue_29_bringup_fault_seam.hpp"
#endif

namespace fermentation {

#if defined(APP_ISSUE_29_BRINGUP_PROBE)
namespace issue_29_bringup {

CommandStatus applyCandidateForResourceProbe(
    RunCommandState& candidate, const CommandDecision& decision) noexcept {
    return applyRunCommand(candidate, decision);
}

}  // namespace issue_29_bringup
#endif

namespace {

constexpr device_platform::RecordTypeId kCheckpointRecordType{7U};
constexpr device_platform::RecordTypeId kHeadRecordType{8U};
constexpr std::size_t kMaximumCheckpointRecordBytes = 8240U;
constexpr std::size_t kMaximumHeadRecordBytes = 256U;

std::optional<std::uint64_t> checkedAdd(std::uint64_t left,
                                        std::uint64_t right) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return std::nullopt;
    }
    return left + right;
}

std::optional<std::uint32_t> checkedToUint32(std::uint64_t value) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(value);
}

std::optional<std::uint64_t> checkedUtcDelta(std::int64_t current,
                                             std::int64_t checkpoint) {
    if (current < checkpoint) return std::nullopt;
    if (checkpoint >= 0 || current < 0) {
        // In this branch the signed subtraction cannot overflow: either both
        // values are non-negative or both are negative and current >=
        // checkpoint.
        return static_cast<std::uint64_t>(current - checkpoint);
    }

    // current is non-negative and checkpoint is negative. Build the distance
    // without negating INT64_MIN and keep the final addition checked.
    const auto negativeMagnitude =
        static_cast<std::uint64_t>(-(checkpoint + 1)) + 1U;
    return checkedAdd(static_cast<std::uint64_t>(current), negativeMagnitude);
}

std::optional<std::uint32_t> exactPriorFermentingSeconds(
    const RunCommandState& current) {
    if (!current.priorBootPhaseElapsed.has_value()) return 0U;
    const auto& tagged = *current.priorBootPhaseElapsed;
    if (tagged.taggedState != ProcessState::Fermenting ||
        !tagged.elapsed.upperBoundSeconds.has_value() ||
        *tagged.elapsed.upperBoundSeconds != tagged.elapsed.lowerBoundSeconds) {
        return std::nullopt;
    }
    return tagged.elapsed.lowerBoundSeconds;
}

bool sameCheckpointReference(const RunCheckpointReference& left,
                             const RunCheckpointReference& right) {
    return left.slot == right.slot &&
           left.schemaVersion == right.schemaVersion &&
           left.storageEpoch == right.storageEpoch &&
           left.checkpointRevision == right.checkpointRevision &&
           left.payloadLength == right.payloadLength &&
           left.payloadCrc == right.payloadCrc && left.variant == right.variant;
}

bool currentMatchesLoadedRecord(
    const RunCommandState& current, const RunPersistenceRawRecord& record,
    const std::array<CommandId, kMaximumPersistedRunCommandIds>& ids,
    std::size_t idCount) {
    RunPersistenceSnapshot projection;
    const RunCheckpointTime checkpointTime{
        record.snapshot.checkpointMonotonicMillis, record.utcUnixSeconds};
    if (!makeRunPersistenceSnapshotInto(
            current, ids, idCount, record.snapshot.trigger, checkpointTime,
            record.snapshot.intervalMinutes, projection)) {
        return false;
    }
    std::string projectedBytes;
    std::string loadedBytes;
    return encodeRunPersistenceSnapshot(projection, projectedBytes) ==
               RunPersistenceCodecStatus::Success &&
           encodeRunPersistenceSnapshot(record.snapshot, loadedBytes) ==
               RunPersistenceCodecStatus::Success &&
           projectedBytes == loadedBytes;
}

bool isActiveFermentingSnapshot(const RunPersistenceSnapshot& snapshot) {
    const bool activeVariant =
        snapshot.variant == RunCheckpointVariant::ProgramRun ||
        snapshot.variant == RunCheckpointVariant::ManualRun;
    return activeVariant &&
           snapshot.processState.state == ProcessState::Fermenting;
}

bool isActiveFermentingCurrent(const RunCommandState& current) {
    return (current.activeProgramRun.has_value() ||
            current.activeManualRun.has_value()) &&
           current.processState.state == ProcessState::Fermenting;
}

RoleTemperatureEvidence toRoleTemperatureEvidence(
    const device_platform::SensorQualitySnapshot& source) {
    return {source.filteredCelsius, source.quality};
}

void updateLastKnownEvidence(
    RecoveryTemperatureEvidence& target,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    target.lastKnown.air = toRoleTemperatureEvidence(liveSensorEvidence.air);
    target.lastKnown.product =
        toRoleTemperatureEvidence(liveSensorEvidence.product);
    target.lastKnown.cooling =
        toRoleTemperatureEvidence(liveSensorEvidence.cooling);
}

bool recoveryEvidenceWindowOpen(const RunCommandState& current) {
    if (current.pendingRecoveryAnchor.has_value() &&
        current.processState.state == ProcessState::RecoveryEvaluation) {
        return true;
    }
    return current.priorBootPhaseElapsed.has_value() &&
           current.priorBootPhaseElapsed->taggedState ==
               current.processState.state;
}

SensorSelectionProgramContext recoverySensorSelectionProgramContext(
    const RunCommandState& current) {
    if (current.activeProgramRun.has_value()) {
        const auto& program =
            current.activeProgramRun->snapshot().sourceProgram.program;
        SensorSelectionProgramContext context;
        context.sensorPreference = program.sensorPreference;
        context.policy = program.productSensorFailure.policy;
        context.returnStrategy = program.productSensorFailure.returnStrategy;
        context.fallbackDelaySeconds =
            program.productSensorFailure.fallbackDelaySeconds;
        return context;
    }
    SensorSelectionProgramContext context;
    context.sensorPreference = SensorPreference::ProductIfAvailableElseAir;
    context.policy = ProductSensorFailurePolicy::WaitForUser;
    context.returnStrategy = ReturnStrategy::ManualReturnToProduct;
    return context;
}

bool recoveryTimeResolvedAtResume(
    const std::optional<TaggedPriorBootPhaseElapsed>& accumulated) {
    return !accumulated.has_value() ||
           accumulated->elapsed.upperBoundSeconds.has_value();
}

struct PendingRecoveryAnchorConstruction {
    PendingRecoveryAnchor anchor;
    std::uint32_t thisHopAltBootLocalSeconds{0U};
};

std::optional<PendingRecoveryAnchorConstruction> makePendingRecoveryAnchor(
    const RunCommandState& candidate,
    const RunPersistenceRawRecord& loadedRecord,
    const ProcessRuntimeState& originalProcessState) {
    if (!candidate.processRunSnapshot.has_value() ||
        !stateUsesRunSnapshot(originalProcessState.state) ||
        !stateMatchesRunSnapshot(originalProcessState.state,
                                 *candidate.processRunSnapshot) ||
        loadedRecord.snapshot.checkpointMonotonicMillis <
            originalProcessState.stateEnteredAtMillis) {
        return std::nullopt;
    }

    const auto thisHopAltBootLocalSeconds64 =
        (loadedRecord.snapshot.checkpointMonotonicMillis -
         originalProcessState.stateEnteredAtMillis) /
        1000U;
    const auto thisHopAltBootLocalSeconds =
        checkedToUint32(thisHopAltBootLocalSeconds64);
    if (!thisHopAltBootLocalSeconds.has_value()) return std::nullopt;
    if (loadedRecord.snapshot.pendingRecoveryAnchor.has_value()) {
        auto anchor = *loadedRecord.snapshot.pendingRecoveryAnchor;
        const auto knownSince =
            checkedAdd(anchor.knownSecondsSinceOriginalCheckpoint,
                       *thisHopAltBootLocalSeconds);
        if (!knownSince.has_value()) return std::nullopt;
        anchor.knownSecondsSinceOriginalCheckpoint = *knownSince;
        return PendingRecoveryAnchorConstruction{anchor,
                                                 *thisHopAltBootLocalSeconds};
    }

    PendingRecoveryAnchor anchor;
    anchor.originalProcessState = originalProcessState;
    anchor.knownPhaseSecondsAtOriginalCheckpoint = *thisHopAltBootLocalSeconds;
    anchor.originalCheckpointUtc = loadedRecord.utcUnixSeconds;
    anchor.originalCheckpointTrigger = loadedRecord.snapshot.trigger;
    anchor.originalCheckpointIntervalMinutes =
        loadedRecord.snapshot.intervalMinutes;
    // No prior elapsed field means that this is the first recovery episode
    // for the phase, not an unknown amount of earlier phase time. Represent
    // that empty prefix as the exact zero interval so known UTC outage
    // evidence can produce an upper bound as well.
    anchor.accumulatedBeforeEpisode = PriorBootPhaseElapsed{0U, 0U};
    if (candidate.priorBootPhaseElapsed.has_value() &&
        candidate.priorBootPhaseElapsed->taggedState ==
            originalProcessState.state) {
        anchor.accumulatedBeforeEpisode =
            candidate.priorBootPhaseElapsed->elapsed;
    }
    return PendingRecoveryAnchorConstruction{anchor,
                                             *thisHopAltBootLocalSeconds};
}

std::optional<PriorBootPhaseElapsed> accumulatedPriorForResume(
    const PendingRecoveryAnchor& anchor, const RecoveryTimeContext& context) {
    static_cast<void>(anchor);
    const auto lower32 =
        checkedToUint32(context.elapsed.totalSecondsLowerBound);
    if (!lower32.has_value()) return std::nullopt;

    std::optional<std::uint32_t> upper32;
    if (context.elapsed.totalSecondsUpperBound.has_value()) {
        upper32 = checkedToUint32(*context.elapsed.totalSecondsUpperBound);
        if (!upper32.has_value()) return std::nullopt;
    }
    return PriorBootPhaseElapsed{*lower32, upper32};
}

ProcessRuntimeState rebasedRecoveredState(const ProcessRuntimeState& original,
                                          std::uint64_t monotonicMillis) {
    auto recovered = original;
    if (original.state == ProcessState::QualifyingTarget) {
        recovered.state = ProcessState::ReachingTarget;
    }
    recovered.stateEnteredAtMillis = monotonicMillis;
    recovered.qualificationValidSinceMillis.reset();
    recovered.targetReachWarningIssued = false;
    recovered.targetReachStartedAtMillis =
        recovered.state == ProcessState::ReachingTarget ? monotonicMillis : 0U;
    return recovered;
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
        case TransitionReason::CriticalFault:
            return true;
        default:
            return false;
    }
}

}  // namespace

RunPersistenceCoordinator::RunPersistenceCoordinator(
    device_platform::IStateStore& store, device_platform::StorageEpoch epoch,
    RunCheckpointSchedule schedule) noexcept
    : store_(store), epoch_(epoch), schedule_(std::move(schedule)) {}

std::optional<CommandId> RunPersistenceCoordinator::commandIdHighWater()
    const noexcept {
    const bool committedState =
        state_ == RunPersistenceCoordinatorState::Ready ||
        state_ == RunPersistenceCoordinatorState::ReadyEmpty ||
        state_ == RunPersistenceCoordinatorState::LoadedActiveRun ||
        state_ == RunPersistenceCoordinatorState::FallbackRecoveryPending;
    if (committedState && currentHead_.has_value() &&
        currentHead_->state == RunPersistenceHeadState::Committed) {
        return currentHead_->commandIdHighWater;
    }
    if (state_ == RunPersistenceCoordinatorState::ReadyEmpty &&
        !currentHead_.has_value()) {
        return CommandId{0U};
    }
    return std::nullopt;
}

RunPersistenceResult RunPersistenceCoordinator::completeAuthorizedEpochHandoff(
    const AuthorizedRunEpochHandoffProof& proof) {
    const auto reject = [this](RunPersistenceStep step,
                               RunPersistenceTechnicalReason reason) {
        return result(RunPersistenceResultStatus::Blocked, step, reason,
                      RunPersistenceDurability::Unchanged);
    };
    if (state_ != RunPersistenceCoordinatorState::Uninitialized ||
        epoch_.value() == 0U || proof.previousEpoch.value() == 0U ||
        proof.currentEpoch != epoch_ ||
        proof.previousEpoch.value() ==
            std::numeric_limits<std::uint64_t>::max() ||
        proof.previousEpoch.value() + 1U != proof.currentEpoch.value()) {
        return reject(RunPersistenceStep::LoadHead,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }

    const auto headRead = store_.readHead(kMaximumHeadRecordBytes);
    if (headRead.status == device_platform::StateStoreReadStatus::CapacityError)
        return reject(RunPersistenceStep::LoadHead,
                      RunPersistenceTechnicalReason::StoreReadError);
    if (headRead.status == device_platform::StateStoreReadStatus::ReadError)
        return reject(RunPersistenceStep::LoadHead,
                      RunPersistenceTechnicalReason::StoreReadError);
    if (headRead.status != device_platform::StateStoreReadStatus::Success &&
        headRead.status != device_platform::StateStoreReadStatus::NotFound)
        return reject(RunPersistenceStep::LoadHead,
                      RunPersistenceTechnicalReason::StoreReadError);

    std::array<device_platform::StateStoreReadResult, 2U> slotReads{};
    for (std::size_t slot = 0U; slot < slotReads.size(); ++slot) {
        slotReads[slot] = store_.readSlot(slot, kMaximumCheckpointRecordBytes);
        if (slotReads[slot].status ==
                device_platform::StateStoreReadStatus::CapacityError ||
            slotReads[slot].status ==
                device_platform::StateStoreReadStatus::ReadError) {
            return reject(RunPersistenceStep::LoadCurrent,
                          RunPersistenceTechnicalReason::StoreReadError);
        }
    }

    const bool emptyStore =
        headRead.status == device_platform::StateStoreReadStatus::NotFound &&
        slotReads[0].status ==
            device_platform::StateStoreReadStatus::NotFound &&
        slotReads[1].status == device_platform::StateStoreReadStatus::NotFound;

    // A completed handoff may be observed again after an interrupted caller
    // path. Recognize only the exact new-epoch, schema-4, two-slot empty
    // shape; no arbitrary ForeignEpoch record is treated as empty.
    if (!emptyStore &&
        headRead.status == device_platform::StateStoreReadStatus::Success) {
        const auto envelope = device_platform::decodeEnvelope(headRead.value);
        if (!envelope.envelope.has_value()) {
            return reject(RunPersistenceStep::LoadHead,
                          RunPersistenceTechnicalReason::CodecError);
        }
        if (envelope.envelope->storageEpoch == proof.currentEpoch) {
            const auto head = decodeRunPersistenceHead(headRead.value, epoch_);
            if (!head.has_value() ||
                head->state != RunPersistenceHeadState::Committed ||
                head->current.variant != RunCheckpointVariant::NoActiveRun ||
                head->current.schemaVersion != kCurrentRunPersistenceSchema ||
                !head->commandIdHighWater.has_value() ||
                *head->commandIdHighWater != 0U || head->fallback.has_value()) {
                return reject(RunPersistenceStep::LoadHead,
                              RunPersistenceTechnicalReason::InvalidProjection);
            }
            for (std::size_t slot = 0U; slot < slotReads.size(); ++slot) {
                if (slotReads[slot].status !=
                    device_platform::StateStoreReadStatus::Success) {
                    return reject(
                        RunPersistenceStep::LoadCurrent,
                        RunPersistenceTechnicalReason::InvalidProjection);
                }
                RunPersistenceRawRecord record;
                if (!decodeRunPersistenceRecordInto(slotReads[slot].value,
                                                    epoch_, record) ||
                    record.snapshot.variant !=
                        RunCheckpointVariant::NoActiveRun ||
                    device_platform::decodeEnvelope(slotReads[slot].value)
                            .envelope->schemaVersion !=
                        kCurrentRunPersistenceSchema) {
                    return reject(RunPersistenceStep::LoadCurrent,
                                  RunPersistenceTechnicalReason::CodecError);
                }
                if (slot == head->current.slot &&
                    !runCheckpointReferenceMatches(head->current, record,
                                                   slot)) {
                    return reject(
                        RunPersistenceStep::LoadCurrent,
                        RunPersistenceTechnicalReason::InvalidProjection);
                }
            }
            return result(RunPersistenceResultStatus::Applied,
                          RunPersistenceStep::CommittedHead,
                          RunPersistenceTechnicalReason::None,
                          RunPersistenceDurability::Unchanged);
        }
        if (envelope.envelope->storageEpoch != proof.previousEpoch) {
            return reject(RunPersistenceStep::LoadHead,
                          RunPersistenceTechnicalReason::InvalidProjection);
        }
    } else if (!emptyStore) {
        // A headless store with a slot is an orphan, not proof of an empty
        // epoch. The old committed graph is required for an authorized reset.
        return reject(RunPersistenceStep::LoadHead,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }

    if (!emptyStore) {
        const auto oldHead =
            decodeRunPersistenceHead(headRead.value, proof.previousEpoch);
        if (!oldHead.has_value() ||
            oldHead->state != RunPersistenceHeadState::Committed) {
            return reject(RunPersistenceStep::LoadHead,
                          RunPersistenceTechnicalReason::CodecError);
        }
        const auto validateReferenced = [&](const RunCheckpointReference& ref) {
            if (ref.slot > 1U ||
                slotReads[ref.slot].status !=
                    device_platform::StateStoreReadStatus::Success) {
                return false;
            }
            RunPersistenceRawRecord record;
            return decodeRunPersistenceRecordInto(slotReads[ref.slot].value,
                                                  proof.previousEpoch,
                                                  record) &&
                   runCheckpointReferenceMatches(ref, record, ref.slot);
        };
        if (!validateReferenced(oldHead->current) ||
            (oldHead->fallback.has_value() &&
             !validateReferenced(*oldHead->fallback))) {
            return reject(RunPersistenceStep::LoadCurrent,
                          RunPersistenceTechnicalReason::CodecError);
        }
        for (std::size_t slot = 0U; slot < slotReads.size(); ++slot) {
            if (slotReads[slot].status ==
                device_platform::StateStoreReadStatus::Success) {
                RunPersistenceRawRecord record;
                if (!decodeRunPersistenceRecordInto(
                        slotReads[slot].value, proof.previousEpoch, record)) {
                    return reject(RunPersistenceStep::LoadCurrent,
                                  RunPersistenceTechnicalReason::CodecError);
                }
            }
        }
    }

    RunCommandState emptyState;
    if (!establishBootCompletedStandby(emptyState.processState, 0U)) {
        return reject(RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    RunPersistenceSnapshot emptySnapshot;
    std::array<CommandId, kMaximumPersistedRunCommandIds> noIds{};
    if (!makeRunPersistenceSnapshotInto(
            emptyState, noIds, 0U, RunCheckpointTrigger::Command,
            RunCheckpointTime{0U, std::nullopt}, schedule_.intervalMinutes(),
            emptySnapshot)) {
        return reject(RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    std::string payload;
    if (encodeRunPersistenceSnapshot(emptySnapshot, payload) !=
        RunPersistenceCodecStatus::Success) {
        return reject(RunPersistenceStep::CheckpointSlot,
                      RunPersistenceTechnicalReason::CodecError);
    }

    std::array<RunPersistenceRawRecord, 2U> newRecords{};
    for (std::size_t slot = 0U; slot < newRecords.size(); ++slot) {
        device_platform::StorageEnvelope envelope{
            kCheckpointRecordType,
            kCurrentRunPersistenceSchema,
            epoch_,
            static_cast<std::uint64_t>(slot + 1U),
            std::nullopt,
            payload};
        if (device_platform::encodeEnvelope(envelope, newRecords[slot].bytes,
                                            kMaximumCheckpointRecordBytes) !=
            device_platform::EnvelopeEncodeStatus::Success) {
            return reject(RunPersistenceStep::CheckpointSlot,
                          RunPersistenceTechnicalReason::CodecError);
        }
        newRecords[slot].snapshot = emptySnapshot;
        newRecords[slot].checkpointRevision = slot + 1U;
        newRecords[slot].utcUnixSeconds = std::nullopt;
    }
    RunPersistenceHead newHead;
    newHead.state = RunPersistenceHeadState::Committed;
    newHead.revision = 1U;
    newHead.current = makeRunCheckpointReference(0U, newRecords[0], epoch_);
    newHead.commandIdHighWater = CommandId{0U};
    const auto encodedHead = encodeRunPersistenceHead(newHead, epoch_);
    if (!encodedHead.has_value()) {
        return reject(RunPersistenceStep::CommittedHead,
                      RunPersistenceTechnicalReason::CodecError);
    }

    const auto oldHeadBytes =
        headRead.status == device_platform::StateStoreReadStatus::Success
            ? std::optional<std::string>{headRead.value}
            : std::nullopt;
    for (std::size_t slot = 0U; slot < newRecords.size(); ++slot) {
        const auto oldSlot =
            slotReads[slot].status ==
                    device_platform::StateStoreReadStatus::Success
                ? std::optional<std::string>{slotReads[slot].value}
                : std::nullopt;
        const auto written =
            store_.writeSlotExact(slot, newRecords[slot].bytes, oldSlot,
                                  kMaximumCheckpointRecordBytes);
        if (written != RunPersistenceStoreWriteResult::Written) {
            enterBlockedIndeterminate();
            return reject(
                RunPersistenceStep::CheckpointSlot,
                written == RunPersistenceStoreWriteResult::Indeterminate
                    ? RunPersistenceTechnicalReason::StoreOutcomeUnknown
                    : RunPersistenceTechnicalReason::StoreWriteError);
        }
    }
    const auto headWritten = store_.writeHeadExact(*encodedHead, oldHeadBytes,
                                                   kMaximumHeadRecordBytes);
    if (headWritten != RunPersistenceStoreWriteResult::Written) {
        enterBlockedIndeterminate();
        return reject(
            RunPersistenceStep::CommittedHead,
            headWritten == RunPersistenceStoreWriteResult::Indeterminate
                ? RunPersistenceTechnicalReason::StoreOutcomeUnknown
                : RunPersistenceTechnicalReason::StoreWriteError);
    }
    return result(
        RunPersistenceResultStatus::Applied, RunPersistenceStep::CommittedHead,
        RunPersistenceTechnicalReason::None, RunPersistenceDurability::Changed);
}

void applyLiveRecoveryEvidence(
    RunCommandState& current,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    if (!recoveryEvidenceWindowOpen(current) ||
        !current.lastRecoveryEpisodeEvidence.has_value()) {
        return;
    }

    auto latch = [](std::optional<RoleTemperatureEvidence>& target,
                    const device_platform::SensorQualitySnapshot& live) {
        if (target.has_value() ||
            live.quality != device_platform::SensorQuality::Valid ||
            !live.filteredCelsius.has_value()) {
            return;
        }
        target = toRoleTemperatureEvidence(live);
    };
    auto& firstAfter = current.lastRecoveryEpisodeEvidence->firstAfterRestart;
    latch(firstAfter.air, liveSensorEvidence.air);
    latch(firstAfter.product, liveSensorEvidence.product);
    latch(firstAfter.cooling, liveSensorEvidence.cooling);
}

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
    if (state_ == RunPersistenceCoordinatorState::LoadedActiveRun ||
        state_ == RunPersistenceCoordinatorState::FallbackRecoveryPending)
        return result(RunPersistenceResultStatus::RecoveryPending);
    return result(RunPersistenceResultStatus::Blocked);
}

void RunPersistenceCoordinator::loadAndInitializeInto(
    RunPersistenceLoadResult& destination) {
    destination.status = RunPersistenceLoadStatus::ReadFailed;
    destination.snapshot.reset();
    pendingR1CheckpointRevision_.reset();
    pendingR1RunRevision_.reset();
    if (state_ != RunPersistenceCoordinatorState::Uninitialized) {
        destination.status = RunPersistenceLoadStatus::AlreadyInitialized;
        return;
    }
    // Monotonic time is boot-local.  An injected schedule may already be
    // armed by its caller, but that state must never cross the boot boundary.
    schedule_.reset();
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
            destination.status =
                probe.status ==
                        device_platform::StateStoreReadStatus::CapacityError
                    ? RunPersistenceLoadStatus::CapacityExceeded
                    : RunPersistenceLoadStatus::ReadFailed;
            return;
        }
        if (!slotPresent) {
            state_ = RunPersistenceCoordinatorState::ReadyEmpty;
            destination.status = RunPersistenceLoadStatus::NoPersistedRun;
            return;
        }
        enterBlockedIndeterminate();
        destination.status =
            RunPersistenceLoadStatus::NotReconstructibleOrphanedState;
        return;
    }
    if (read.status == device_platform::StateStoreReadStatus::CapacityError) {
        enterBlockedIndeterminate();
        destination.status = RunPersistenceLoadStatus::CapacityExceeded;
        return;
    }
    if (read.status != device_platform::StateStoreReadStatus::Success) {
        enterBlockedIndeterminate();
        destination.status = RunPersistenceLoadStatus::ReadFailed;
        return;
    }
    const auto headEnvelope = device_platform::decodeEnvelope(read.value);
    if (headEnvelope.envelope.has_value() &&
        headEnvelope.envelope->storageEpoch != epoch_) {
        enterBlockedIndeterminate();
        destination.status = RunPersistenceLoadStatus::ForeignEpoch;
        return;
    }
    if (headEnvelope.envelope.has_value() &&
        !knownRunPersistenceSchema(headEnvelope.envelope->schemaVersion)) {
        enterBlockedIndeterminate();
        destination.status = RunPersistenceLoadStatus::UnsupportedSchema;
        return;
    }
    auto head = decodeRunPersistenceHead(read.value, epoch_);
    if (!head.has_value()) {
        enterBlockedIndeterminate();
        destination.status = RunPersistenceLoadStatus::NotReconstructible;
        return;
    }
    currentHead_ = std::move(*head);
    nextHeadRevision_ =
        currentHead_->revision == std::numeric_limits<std::uint64_t>::max()
            ? 0U
            : currentHead_->revision + 1U;
    if (currentHead_->state == RunPersistenceHeadState::Prepared) {
        enterBlockedIndeterminate();
        destination.status = RunPersistenceLoadStatus::PreparedInterrupted;
        return;
    }
    const auto loadReference = [this](const RunCheckpointReference& reference,
                                      RunPersistenceLoadStatus& status) {
        const auto slot = reference.slot;
        const auto slotRead =
            store_.readSlot(slot, kMaximumCheckpointRecordBytes);
        if (slotRead.status ==
            device_platform::StateStoreReadStatus::NotFound) {
            status = RunPersistenceLoadStatus::NotReconstructible;
            return false;
        }
        if (slotRead.status ==
            device_platform::StateStoreReadStatus::CapacityError) {
            status = RunPersistenceLoadStatus::CapacityExceeded;
            return false;
        }
        if (slotRead.status != device_platform::StateStoreReadStatus::Success) {
            status = RunPersistenceLoadStatus::ReadFailed;
            return false;
        }
        const auto envelope = device_platform::decodeEnvelope(slotRead.value);
        if (envelope.envelope.has_value() &&
            envelope.envelope->storageEpoch != epoch_) {
            status = RunPersistenceLoadStatus::ForeignEpoch;
            return false;
        }
        if (envelope.envelope.has_value() &&
            !knownRunPersistenceSchema(envelope.envelope->schemaVersion)) {
            status = RunPersistenceLoadStatus::UnsupportedSchema;
            return false;
        }
        if (!decodeRunPersistenceRecordInto(slotRead.value, epoch_,
                                            workingSet_.record) ||
            !runCheckpointReferenceMatches(reference, workingSet_.record,
                                           slot)) {
            status = RunPersistenceLoadStatus::NotReconstructible;
            return false;
        }
        status = RunPersistenceLoadStatus::Current;
        return true;
    };
    RunPersistenceLoadStatus currentStatus = RunPersistenceLoadStatus::Current;
    const bool currentRecord =
        loadReference(currentHead_->current, currentStatus);
    if (currentRecord) {
        slots_[currentHead_->current.slot] = workingSet_.record;
        const auto& snap = workingSet_.record.snapshot;
        nextCheckpointRevision_ =
            workingSet_.record.checkpointRevision ==
                    std::numeric_limits<std::uint64_t>::max()
                ? 0U
                : workingSet_.record.checkpointRevision + 1U;
        persistedIds_ = snap.persistedRunCommandIds;
        persistedIdCount_ = snap.persistedRunCommandCount;
        // The head is the only source of the recoverable state.  A valid
        // unreferenced checkpoint is therefore never loaded as current or
        // fallback.  Its envelope revision is nevertheless a physical
        // high-watermark: reusing it could overwrite a known orphan with the
        // same revision after a reboot.
        bool checkpointRevisionOverflow = false;
        for (std::size_t slot = 0U; slot < 2U; ++slot) {
            std::string physicalBytes;
            if (slot == currentHead_->current.slot) {
                physicalBytes = workingSet_.record.bytes;
            } else {
                const auto physical =
                    store_.readSlot(slot, kMaximumCheckpointRecordBytes);
                if (physical.status ==
                    device_platform::StateStoreReadStatus::NotFound) {
                    continue;
                }
                if (physical.status ==
                    device_platform::StateStoreReadStatus::CapacityError) {
                    enterBlockedIndeterminate();
                    destination.status =
                        RunPersistenceLoadStatus::CapacityExceeded;
                    return;
                }
                if (physical.status !=
                    device_platform::StateStoreReadStatus::Success) {
                    enterBlockedIndeterminate();
                    destination.status = RunPersistenceLoadStatus::ReadFailed;
                    return;
                }
                physicalBytes = physical.value;
            }
            const auto physicalEnvelope =
                device_platform::decodeEnvelope(physicalBytes);
            if (!physicalEnvelope.envelope.has_value() ||
                physicalEnvelope.envelope->recordTypeId !=
                    kCheckpointRecordType ||
                !knownRunPersistenceSchema(
                    physicalEnvelope.envelope->schemaVersion) ||
                physicalEnvelope.envelope->storageEpoch != epoch_ ||
                physicalEnvelope.envelope->versionValue == 0U) {
                enterBlockedIndeterminate();
                destination.status =
                    RunPersistenceLoadStatus::NotReconstructible;
                return;
            }
            if (physicalEnvelope.envelope->versionValue ==
                std::numeric_limits<std::uint64_t>::max()) {
                checkpointRevisionOverflow = true;
            } else if (!checkpointRevisionOverflow) {
                nextCheckpointRevision_ =
                    std::max(nextCheckpointRevision_,
                             physicalEnvelope.envelope->versionValue + 1U);
            }
        }
        if (checkpointRevisionOverflow) {
            nextCheckpointRevision_ = 0U;
        }
        if (snap.variant == RunCheckpointVariant::NoActiveRun) {
            state_ = RunPersistenceCoordinatorState::ReadyEmpty;
            destination.status = RunPersistenceLoadStatus::NoActiveRun;
            destination.snapshot = snap;
            return;
        }
        state_ = RunPersistenceCoordinatorState::LoadedActiveRun;
        destination.status = RunPersistenceLoadStatus::Current;
        destination.snapshot = snap;
        return;
    }
    if (currentHead_->fallback.has_value()) {
        RunPersistenceLoadStatus fallbackStatus =
            RunPersistenceLoadStatus::Current;
        const bool fallbackRecord =
            loadReference(*currentHead_->fallback, fallbackStatus);
        if (fallbackRecord) {
            if (workingSet_.record.snapshot.variant ==
                RunCheckpointVariant::NoActiveRun) {
                // A tombstone is a valid fallback reference for an active
                // current, but it cannot reconstruct that current after the
                // active slot is damaged.  Keep this integrity failure
                // fail-closed instead of exposing a non-resumable snapshot as
                // FallbackRecoveryPending to the later recovery API.
                enterBlockedIndeterminate();
                destination.status =
                    RunPersistenceLoadStatus::NotReconstructible;
                return;
            }
            slots_[currentHead_->fallback->slot] = workingSet_.record;
            const auto raiseCheckpointHighWatermark =
                [this](std::uint64_t checkpointRevision) {
                    if (nextCheckpointRevision_ == 0U ||
                        checkpointRevision ==
                            std::numeric_limits<std::uint64_t>::max()) {
                        nextCheckpointRevision_ = 0U;
                        return;
                    }
                    nextCheckpointRevision_ = std::max(nextCheckpointRevision_,
                                                       checkpointRevision + 1U);
                };
            raiseCheckpointHighWatermark(workingSet_.record.checkpointRevision);
            raiseCheckpointHighWatermark(
                currentHead_->current.checkpointRevision);
            raiseCheckpointHighWatermark(
                currentHead_->fallback->checkpointRevision);
            persistedIds_ = workingSet_.record.snapshot.persistedRunCommandIds;
            persistedIdCount_ =
                workingSet_.record.snapshot.persistedRunCommandCount;
            state_ = RunPersistenceCoordinatorState::FallbackRecoveryPending;
            destination.status = RunPersistenceLoadStatus::FallbackRecovered;
            destination.snapshot = workingSet_.record.snapshot;
            return;
        }
        if (fallbackStatus == RunPersistenceLoadStatus::ReadFailed ||
            fallbackStatus == RunPersistenceLoadStatus::CapacityExceeded ||
            fallbackStatus == RunPersistenceLoadStatus::ForeignEpoch ||
            fallbackStatus == RunPersistenceLoadStatus::UnsupportedSchema) {
            enterBlockedIndeterminate();
            destination.status = fallbackStatus;
            return;
        }
    }
    enterBlockedIndeterminate();
    destination.status = currentStatus;
    destination.snapshot.reset();
}

RunPersistenceLoadResult RunPersistenceCoordinator::loadAndInitialize() {
    RunPersistenceLoadResult destination;
    loadAndInitializeInto(destination);
    return destination;
}

RecoveryActivationOutcome RunPersistenceCoordinator::activateLoadedRun(
    const RunCommandState& current, const RunCheckpointTime& time,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    const auto invalid = [this](const RunCommandState& state) {
        return RecoveryActivationOutcome{
            result(RunPersistenceResultStatus::InvalidDecision,
                   RunPersistenceStep::CandidateApply,
                   RunPersistenceTechnicalReason::InvalidProjection),
            state};
    };
    if (state_ != RunPersistenceCoordinatorState::LoadedActiveRun ||
        !currentHead_.has_value() ||
        !slots_[currentHead_->current.slot].has_value()) {
        return {unavailableResult(), current};
    }

    // A persisted historical Fault is terminal for this recovery episode.
    // Restore it as-is and leave the recovery load state without re-entering
    // the process state machine or touching persistence.
    if (current.processState.state == ProcessState::Fault) {
        state_ = RunPersistenceCoordinatorState::Ready;
        auto restored = result(RunPersistenceResultStatus::Applied,
                               RunPersistenceStep::RamApply,
                               RunPersistenceTechnicalReason::None,
                               RunPersistenceDurability::Unchanged);
        restored.coordinatorState = state_;
        return {restored, current};
    }

    // Completed is a restored result, not a run phase that may be re-entered.
    // Refresh its boot-local timestamp in RAM so the later acknowledgement
    // can pass the normal monotonic-time validation.
    if (current.processState.state == ProcessState::Completed) {
        auto candidate = current;
        candidate.processState.stateEnteredAtMillis = time.monotonicMillis;
        state_ = RunPersistenceCoordinatorState::Ready;
        auto persisted = result(RunPersistenceResultStatus::Applied,
                                RunPersistenceStep::RamApply,
                                RunPersistenceTechnicalReason::None,
                                RunPersistenceDurability::Unchanged);
        persisted.coordinatorState = state_;
        return {persisted, candidate};
    }

    const auto& loadedRecord = *slots_[currentHead_->current.slot];
    if (!current.processRunSnapshot.has_value()) {
        return invalid(current);
    }

    // A reboot while Hop-1-only RecoveryEvaluation is already persisted is an
    // episode refresh, not a second RecoveryReentryRequired transition.
    if (current.processState.state == ProcessState::RecoveryEvaluation) {
        if (!current.pendingRecoveryAnchor.has_value() ||
            !current.recoveryBootAnchorMonotonicMillis.has_value() ||
            current.recoveryEpisodeRevision ==
                std::numeric_limits<std::uint32_t>::max()) {
            return invalid(current);
        }
        auto candidate = current;
        updateLastKnownEvidence(candidate.recoveryTemperatureEvidence,
                                liveSensorEvidence);
        ++candidate.recoveryEpisodeRevision;
        candidate.recoveryBootAnchorMonotonicMillis = time.monotonicMillis;
        applyLiveRecoveryEvidence(candidate, liveSensorEvidence);
        const auto snapshot = makeRunPersistenceSnapshot(
            candidate, persistedIds_, persistedIdCount_,
            RunCheckpointTrigger::Transition, time,
            schedule_.intervalMinutes());
        if (!snapshot.has_value()) return invalid(current);
        const auto persisted = writeSnapshotCore(
            *snapshot, time, false, current,
            RunPersistenceMutationKind::Recovery, std::nullopt, std::nullopt,
            RunPersistenceFallbackDirective{},
            RunPersistenceCoordinatorState::LoadedActiveRun);
        if (persisted.status != RunPersistenceResultStatus::Applied) {
            return {persisted, current};
        }
        return {persisted, candidate};
    }

    const auto originalProcessState = current.processState;
    if (!stateUsesRunSnapshot(originalProcessState.state) ||
        current.recoveryEpisodeRevision ==
            std::numeric_limits<std::uint32_t>::max()) {
        return invalid(current);
    }

    auto candidate = current;
    const auto anchor = makePendingRecoveryAnchor(candidate, loadedRecord,
                                                  originalProcessState);
    if (!anchor.has_value()) return invalid(current);
    candidate.pendingRecoveryAnchor = anchor->anchor;
    if (originalProcessState.state == ProcessState::Fermenting &&
        !foldObservedRunSeconds(candidate,
                                anchor->thisHopAltBootLocalSeconds)) {
        return invalid(current);
    }
    candidate.recoveryBootAnchorMonotonicMillis = time.monotonicMillis;
    ++candidate.recoveryEpisodeRevision;
    if (candidate.lastRecoveryEpisodeEvidence.has_value()) {
        supersedeUnbookedWeightedSegment(
            candidate.runProgress,
            candidate.lastRecoveryEpisodeEvidence->weightedProgressSegmentId);
    }
    candidate.lastRecoveryEpisodeEvidence = RecoveryEpisodeEvidence{
        current.recoveryTemperatureEvidence.lastKnown,
        FirstAfterRestartEvidence{}, candidate.recoveryEpisodeRevision};
    // The frozen beforeOutage copy must precede every post-restart update.
    updateLastKnownEvidence(candidate.recoveryTemperatureEvidence,
                            liveSensorEvidence);

    const auto hop1 = propose(
        originalProcessState, ProcessState::RecoveryEvaluation,
        TransitionReason::RecoveryReentryRequired, time.monotonicMillis);
    if (!hop1.proposed() ||
        !applyProcessTransition(candidate.processState, hop1,
                                &*candidate.processRunSnapshot)) {
        return invalid(current);
    }
    applyLiveRecoveryEvidence(candidate, liveSensorEvidence);

    const auto commitCandidate =
        [&](const RunCommandState& toCommit,
            RunPersistenceFallbackDirective fallbackDirective = {})
        -> RecoveryActivationOutcome {
        const auto snapshot = makeRunPersistenceSnapshot(
            toCommit, persistedIds_, persistedIdCount_,
            RunCheckpointTrigger::Transition, time,
            schedule_.intervalMinutes());
        if (!snapshot.has_value()) return invalid(current);
        const auto persisted = writeSnapshotCore(
            *snapshot, time, false, current,
            RunPersistenceMutationKind::Recovery, std::nullopt, std::nullopt,
            fallbackDirective, RunPersistenceCoordinatorState::LoadedActiveRun);
        if (persisted.status != RunPersistenceResultStatus::Applied) {
            return {persisted, current};
        }
        return {persisted, toCommit};
    };

    const auto timeContext = deriveRecoveryTimeContext(
        *candidate.pendingRecoveryAnchor, time.utcUnixSeconds,
        time.monotonicMillis, *candidate.recoveryBootAnchorMonotonicMillis);
    if (!timeContext.has_value()) return invalid(current);

    if (originalProcessState.state == ProcessState::WaitingForProduct) {
        const auto verdict = evaluateRecoveryTimeVerdict(
            timeContext->elapsed,
            *candidate.processRunSnapshot->maximumProductWaitMinutes * 60U);
        if (verdict == RecoveryTimeVerdict::DefinitelyExpired) {
            const auto tombstone =
                propose(candidate.processState, ProcessState::Standby,
                        TransitionReason::RecoveryEndedByExpiredWait,
                        time.monotonicMillis);
            if (!tombstone.proposed() ||
                !applyProcessTransition(candidate.processState, tombstone,
                                        &*candidate.processRunSnapshot)) {
                return invalid(current);
            }
            applyLiveRecoveryEvidence(candidate, liveSensorEvidence);
            clearActiveRunState(candidate);
            return commitCandidate(candidate);
        }
        if (verdict == RecoveryTimeVerdict::Uncertain) {
            return commitCandidate(candidate);
        }
    }

    if (!candidate.sensorSelection.has_value() ||
        !candidate.activeRunSensorMode.has_value()) {
        return invalid(current);
    }
    const auto recommendation = computeRestartSensorSelection(
        *candidate.sensorSelection, *candidate.activeRunSensorMode,
        recoverySensorSelectionProgramContext(candidate), liveSensorEvidence);
    candidate.sensorSelectionRuntime = recommendation.runtime;
    candidate.activeRunSensorMode = recommendation.activeMode;
    if (candidate.activeManualRun.has_value()) {
        candidate.activeManualRun->values.sensorMode =
            recommendation.activeMode;
    }
    if (recommendation.runtime.permission != SensorPeltierPermission::Allowed) {
        const auto rejected = decideProcessTransition(
            candidate.processState, &*candidate.processRunSnapshot,
            ProcessSignals{},
            TransitionRequest{ProcessEvent::RecoveryReject, std::nullopt},
            time.monotonicMillis);
        if (rejected.proposed()) {
            if (!applyProcessTransition(candidate.processState, rejected,
                                        &*candidate.processRunSnapshot)) {
                return invalid(current);
            }
        } else {
            return invalid(current);
        }
        if (candidate.lastRecoveryEpisodeEvidence.has_value()) {
            supersedeUnbookedWeightedSegment(
                candidate.runProgress, candidate.lastRecoveryEpisodeEvidence
                                           ->weightedProgressSegmentId);
            candidate.lastRecoveryEpisodeEvidence->weightedProgressSegmentId
                .reset();
        }
        candidate.pendingRecoveryAnchor.reset();
        candidate.recoveryBootAnchorMonotonicMillis.reset();
        candidate.priorBootPhaseElapsed.reset();
        clearActiveRunState(candidate);
        return commitCandidate(
            candidate,
            RunPersistenceFallbackDirective{
                RunPersistenceFallbackMode::ClearFallback, std::nullopt});
    }

    if (!candidate.pendingRecoveryAnchor.has_value()) return invalid(current);
    const auto accumulated = accumulatedPriorForResume(
        *candidate.pendingRecoveryAnchor, *timeContext);
    if (!accumulated.has_value()) return invalid(current);

    PriorBootPhaseElapsed priorForDecision = *accumulated;
    if (originalProcessState.state == ProcessState::WaitingForProduct) {
        if (!timeContext->elapsed.totalSecondsUpperBound.has_value()) {
            return invalid(current);
        }
        const auto upper =
            checkedToUint32(*timeContext->elapsed.totalSecondsUpperBound);
        if (!upper.has_value()) return invalid(current);
        priorForDecision = PriorBootPhaseElapsed{*upper, *upper};
    }

    const auto recovered =
        rebasedRecoveredState(originalProcessState, time.monotonicMillis);
    const auto hop2 = decideProcessTransition(
        candidate.processState, &*candidate.processRunSnapshot,
        ProcessSignals{},
        TransitionRequest{ProcessEvent::RecoveryResume, recovered},
        time.monotonicMillis, priorForDecision);
    if (!hop2.proposed() ||
        !applyProcessTransition(candidate.processState, hop2,
                                &*candidate.processRunSnapshot)) {
        return originalProcessState.state == ProcessState::WaitingForProduct
                   ? commitCandidate(candidate)
                   : invalid(current);
    }

    if (candidate.processState.state == ProcessState::WaitingForProduct ||
        candidate.processState.state == ProcessState::Fermenting ||
        candidate.processState.state == ProcessState::CoolHolding) {
        candidate.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
            candidate.processState.state, *accumulated};
    } else {
        candidate.priorBootPhaseElapsed.reset();
    }
    // Latching must happen before this conditional deletion. A resolved
    // resume closes the time question; an unresolved one retains both fields.
    applyLiveRecoveryEvidence(candidate, liveSensorEvidence);
    if (recoveryTimeResolvedAtResume(candidate.priorBootPhaseElapsed)) {
        candidate.pendingRecoveryAnchor.reset();
        candidate.recoveryBootAnchorMonotonicMillis.reset();
    }
    return commitCandidate(candidate);
}

RunPersistenceResult RunPersistenceCoordinator::activateR1EligibleRun(
    RunCommandState& current, const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    if (state_ != RunPersistenceCoordinatorState::LoadedActiveRun ||
        !currentHead_.has_value() ||
        !slots_[currentHead_->current.slot].has_value()) {
        return unavailableResult();
    }

    // These terminal states follow the exact RAM-only restoration precedent
    // of activateLoadedRun(). They do not require live sensor evidence and do
    // not create a process transition.
    if (current.processState.state == ProcessState::Fault) {
        state_ = RunPersistenceCoordinatorState::Ready;
        auto restored = result(RunPersistenceResultStatus::Applied,
                               RunPersistenceStep::RamApply,
                               RunPersistenceTechnicalReason::None,
                               RunPersistenceDurability::Unchanged);
        restored.coordinatorState = state_;
        return restored;
    }

    if (current.processState.state == ProcessState::Completed) {
        current.processState.stateEnteredAtMillis = time.monotonicMillis;
        state_ = RunPersistenceCoordinatorState::Ready;
        auto restored = result(RunPersistenceResultStatus::Applied,
                               RunPersistenceStep::RamApply,
                               RunPersistenceTechnicalReason::None,
                               RunPersistenceDurability::Unchanged);
        restored.coordinatorState = state_;
        return restored;
    }

    const auto& loadedSnapshot = slots_[currentHead_->current.slot]->snapshot;
    if (!boot_classification::isR1ResumeEligible(loadedSnapshot) ||
        current.processState.state != loadedSnapshot.processState.state ||
        liveSensorEvidence == nullptr) {
        return result(RunPersistenceResultStatus::NotEligible,
                      RunPersistenceStep::CandidateApply);
    }

    workingSet_.candidate = current;
    workingSet_.candidate.processState =
        rebasedRecoveredState(current.processState, time.monotonicMillis);

    if (!workingSet_.candidate.sensorSelection.has_value() ||
        !workingSet_.candidate.activeRunSensorMode.has_value()) {
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    const auto recommendation = computeRestartSensorSelection(
        *workingSet_.candidate.sensorSelection,
        *workingSet_.candidate.activeRunSensorMode,
        recoverySensorSelectionProgramContext(workingSet_.candidate),
        *liveSensorEvidence);
    if (recommendation.runtime.permission != SensorPeltierPermission::Allowed) {
        return result(RunPersistenceResultStatus::NotEligible,
                      RunPersistenceStep::CandidateApply);
    }
    workingSet_.candidate.sensorSelectionRuntime = recommendation.runtime;
    workingSet_.candidate.activeRunSensorMode = recommendation.activeMode;
    if (workingSet_.candidate.activeManualRun.has_value()) {
        workingSet_.candidate.activeManualRun->values.sensorMode =
            recommendation.activeMode;
    }

    if (!makeRunPersistenceSnapshotInto(
            workingSet_.candidate, persistedIds_, persistedIdCount_,
            RunCheckpointTrigger::Transition, time, schedule_.intervalMinutes(),
            workingSet_.snapshot)) {
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }

    const auto persisted =
        writeSnapshotCore(workingSet_.snapshot, time, false, current,
                          RunPersistenceMutationKind::Recovery, std::nullopt,
                          std::nullopt, RunPersistenceFallbackDirective{},
                          RunPersistenceCoordinatorState::LoadedActiveRun);
    if (persisted.status != RunPersistenceResultStatus::Applied) {
        return persisted;
    }

    current = workingSet_.candidate;
    return persisted;
}

RecoveryActivationOutcome
RunPersistenceCoordinator::activateR1ExactFermentingCore(
    const RunCommandState& current, const RunPersistenceRawRecord& loadedRecord,
    const RunCheckpointTime& time, std::optional<std::size_t> targetSlot,
    RunPersistenceFallbackDirective fallbackDirective,
    RunPersistenceCoordinatorState rollbackState,
    bool selectedFallbackCompletionToNoActiveRun) {
    const auto invalid =
        [this, &current](RunPersistenceResultStatus status =
                             RunPersistenceResultStatus::InvalidDecision) {
            return RecoveryActivationOutcome{
                result(status, RunPersistenceStep::CandidateApply,
                       RunPersistenceTechnicalReason::InvalidProjection),
                current};
        };
    if (current.processState.state != ProcessState::Fermenting ||
        current.runProgress.basis != RunProgressBasis::KnownTotal ||
        !current.processRunSnapshot.has_value() ||
        !current.processRunSnapshot->fermentationDurationMinutes.has_value() ||
        !loadedRecord.utcUnixSeconds.has_value() ||
        !time.utcUnixSeconds.has_value()) {
        return invalid();
    }
    const auto prior = exactPriorFermentingSeconds(current);
    if (!prior.has_value() || loadedRecord.snapshot.checkpointMonotonicMillis <
                                  current.processState.stateEnteredAtMillis) {
        return invalid();
    }
    const auto liveSegment =
        checkedToUint32((loadedRecord.snapshot.checkpointMonotonicMillis -
                         current.processState.stateEnteredAtMillis) /
                        1000U);
    if (!liveSegment.has_value())
        return invalid(RunPersistenceResultStatus::CounterOverflow);
    const auto phaseAtCheckpoint = checkedAdd(*prior, *liveSegment);
    const auto wallClockSinceCheckpoint =
        checkedUtcDelta(*time.utcUnixSeconds, *loadedRecord.utcUnixSeconds);
    if (!phaseAtCheckpoint.has_value() || !wallClockSinceCheckpoint.has_value())
        return invalid(RunPersistenceResultStatus::CounterOverflow);
    const auto recoveredPhase =
        checkedAdd(*phaseAtCheckpoint, *wallClockSinceCheckpoint);
    const auto duration = checkedToUint32(
        static_cast<std::uint64_t>(
            *current.processRunSnapshot->fermentationDurationMinutes) *
        60U);
    const auto recoveredPrior = recoveredPhase.has_value()
                                    ? checkedToUint32(*recoveredPhase)
                                    : std::optional<std::uint32_t>{};
    if (!recoveredPhase.has_value() || !duration.has_value() ||
        !recoveredPrior.has_value())
        return invalid(RunPersistenceResultStatus::CounterOverflow);

    auto candidate = current;
    if (!foldObservedRunSeconds(candidate, *liveSegment) ||
        candidate.runRevision == std::numeric_limits<std::uint32_t>::max())
        return invalid(RunPersistenceResultStatus::CounterOverflow);
    if (*recoveredPhase < *duration) {
        const auto hop1 = propose(
            candidate.processState, ProcessState::RecoveryEvaluation,
            TransitionReason::RecoveryReentryRequired, time.monotonicMillis);
        if (!hop1.proposed() ||
            !applyProcessTransition(candidate.processState, hop1,
                                    &*candidate.processRunSnapshot))
            return invalid();
        auto recoveredState = current.processState;
        recoveredState.state = ProcessState::Fermenting;
        recoveredState.stateEnteredAtMillis = time.monotonicMillis;
        recoveredState.qualificationValidSinceMillis.reset();
        recoveredState.targetReachStartedAtMillis = 0U;
        recoveredState.targetReachWarningIssued = false;
        const auto hop = decideProcessTransition(
            candidate.processState, &*candidate.processRunSnapshot,
            ProcessSignals{},
            TransitionRequest{ProcessEvent::RecoveryResume, recoveredState},
            time.monotonicMillis,
            PriorBootPhaseElapsed{*recoveredPrior, *recoveredPrior});
        if (!hop.proposed() ||
            !applyProcessTransition(candidate.processState, hop,
                                    &*candidate.processRunSnapshot))
            return invalid();
        candidate.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
            ProcessState::Fermenting,
            PriorBootPhaseElapsed{*recoveredPrior, *recoveredPrior}};
    } else {
        const auto completion = completeTimedRun(candidate.processState,
                                                 *candidate.processRunSnapshot,
                                                 time.monotonicMillis);
        if (!completion.proposed() ||
            !applyProcessTransition(candidate.processState, completion,
                                    &*candidate.processRunSnapshot))
            return invalid();
        // A selected fallback that was already complete during the outage is
        // committed as the existing acknowledged-terminal/no-active shape.
        // This uses the canonical completion acknowledgement transition and
        // the shared clear helper; it does not add an interlock allow rule or
        // a second recovery policy.
        if (selectedFallbackCompletionToNoActiveRun &&
            candidate.processState.state == ProcessState::Completed) {
            const auto acknowledgement = decideProcessTransition(
                candidate.processState, &*candidate.processRunSnapshot,
                ProcessSignals{},
                TransitionRequest{ProcessEvent::AcknowledgeCompletion,
                                  std::nullopt},
                time.monotonicMillis, PriorBootPhaseElapsed{});
            if (!acknowledgement.proposed() ||
                !applyProcessTransition(candidate.processState, acknowledgement,
                                        &*candidate.processRunSnapshot)) {
                return invalid();
            }
            clearActiveRunState(candidate);
        }
        candidate.priorBootPhaseElapsed.reset();
    }
    ++candidate.runRevision;
    RunPersistenceSnapshot snapshot;
    if (!makeRunPersistenceSnapshotInto(candidate, persistedIds_,
                                        persistedIdCount_,
                                        RunCheckpointTrigger::Transition, time,
                                        schedule_.intervalMinutes(), snapshot))
        return invalid();
    const auto persisted = writeSnapshotCore(
        snapshot, time, false, current, RunPersistenceMutationKind::Recovery,
        std::nullopt, targetSlot, fallbackDirective, rollbackState);
    if (persisted.status != RunPersistenceResultStatus::Applied)
        return {persisted, current};
    auto applied = persisted;
    applied.step = RunPersistenceStep::RamApply;
    applied.coordinatorState = state_;
    return {applied, std::move(candidate)};
}

R1RecoveryEvaluation
RunPersistenceCoordinator::evaluateCurrentFermentingRecovery(
    RunCommandState& current, const RunCheckpointTime& time) {
    const auto rejected =
        [this](RunPersistenceResultStatus status,
               RunPersistenceStep step = RunPersistenceStep::CandidateApply,
               RunPersistenceTechnicalReason reason =
                   RunPersistenceTechnicalReason::InvalidProjection) {
            pendingR1CheckpointRevision_.reset();
            pendingR1RunRevision_.reset();
            return R1RecoveryEvaluation{
                RecoveryDisposition::RecoveryRejectedOrFailClosed,
                result(status, step, reason)};
        };

    if (state_ != RunPersistenceCoordinatorState::LoadedActiveRun ||
        !currentHead_.has_value() ||
        !slots_[currentHead_->current.slot].has_value()) {
        return rejected(state_ == RunPersistenceCoordinatorState::Ready
                            ? RunPersistenceResultStatus::StaleDecision
                            : RunPersistenceResultStatus::NotEligible);
    }

    const auto& loadedRecord = *slots_[currentHead_->current.slot];
    // Re-read the head and current slot before each evaluation. Waiting for
    // UTC must not turn an externally advanced persistence revision into a
    // decision based on stale in-memory evidence.
    const auto liveHeadRead = store_.readHead(kMaximumHeadRecordBytes);
    if (liveHeadRead.status != device_platform::StateStoreReadStatus::Success) {
        enterBlockedIndeterminate();
        return rejected(RunPersistenceResultStatus::PersistenceIndeterminate,
                        RunPersistenceStep::LoadHead,
                        RunPersistenceTechnicalReason::StoreReadError);
    }
    const auto liveHead = decodeRunPersistenceHead(liveHeadRead.value, epoch_);
    if (!liveHead.has_value() ||
        !sameCheckpointReference(liveHead->current, currentHead_->current)) {
        return rejected(RunPersistenceResultStatus::StaleDecision,
                        RunPersistenceStep::LoadHead);
    }
    const auto liveSlotRead = store_.readSlot(currentHead_->current.slot,
                                              kMaximumCheckpointRecordBytes);
    if (liveSlotRead.status != device_platform::StateStoreReadStatus::Success) {
        enterBlockedIndeterminate();
        return rejected(RunPersistenceResultStatus::PersistenceIndeterminate,
                        RunPersistenceStep::LoadCurrent,
                        RunPersistenceTechnicalReason::StoreReadError);
    }
    RunPersistenceRawRecord liveRecord;
    if (!decodeRunPersistenceRecordInto(liveSlotRead.value, epoch_,
                                        liveRecord) ||
        !runCheckpointReferenceMatches(currentHead_->current, liveRecord,
                                       currentHead_->current.slot) ||
        !currentMatchesLoadedRecord(current, liveRecord, persistedIds_,
                                    persistedIdCount_)) {
        return rejected(RunPersistenceResultStatus::StaleDecision,
                        RunPersistenceStep::LoadCurrent);
    }

    if (current.processState.state != ProcessState::Fermenting ||
        current.runProgress.basis != RunProgressBasis::KnownTotal ||
        !current.processRunSnapshot.has_value() ||
        !current.processRunSnapshot->fermentationDurationMinutes.has_value() ||
        loadedRecord.checkpointRevision !=
            currentHead_->current.checkpointRevision ||
        !currentMatchesLoadedRecord(current, loadedRecord, persistedIds_,
                                    persistedIdCount_)) {
        return rejected(RunPersistenceResultStatus::InvalidDecision);
    }

    if (pendingR1CheckpointRevision_.has_value() &&
        (*pendingR1CheckpointRevision_ != loadedRecord.checkpointRevision ||
         !pendingR1RunRevision_.has_value() ||
         *pendingR1RunRevision_ != current.runRevision)) {
        return rejected(RunPersistenceResultStatus::StaleDecision);
    }
    pendingR1CheckpointRevision_ = loadedRecord.checkpointRevision;
    pendingR1RunRevision_ = current.runRevision;

    if (!loadedRecord.utcUnixSeconds.has_value()) {
        return rejected(RunPersistenceResultStatus::InvalidDecision);
    }
    if (!time.utcUnixSeconds.has_value()) {
        return R1RecoveryEvaluation{
            RecoveryDisposition::WaitingForTrustedTime,
            result(RunPersistenceResultStatus::RecoveryPending,
                   RunPersistenceStep::CandidateApply)};
    }
    // Missing current UTC is a non-terminal asynchronous state only after
    // the persisted checkpoint UTC has been proven present. Without that
    // record anchor, a later RTC/NTP value cannot make the wall-clock term
    // reconstructible. The loaded checkpoint remains untouched and can be
    // evaluated again by the same coordinator when trusted UTC is available.
    const auto outcome = activateR1ExactFermentingCore(
        current, loadedRecord, time, std::nullopt,
        RunPersistenceFallbackDirective{},
        RunPersistenceCoordinatorState::LoadedActiveRun);
    const auto persisted = outcome.persistenceResult;
    if (persisted.status != RunPersistenceResultStatus::Applied) {
        pendingR1CheckpointRevision_.reset();
        pendingR1RunRevision_.reset();
        return R1RecoveryEvaluation{
            RecoveryDisposition::RecoveryRejectedOrFailClosed, persisted};
    }
    current = outcome.resultingState;
    pendingR1CheckpointRevision_.reset();
    pendingR1RunRevision_.reset();
    return R1RecoveryEvaluation{RecoveryDisposition::CurrentRunRecoverable,
                                persisted};
}

bool RunPersistenceCoordinator::prepareRecoveryEvaluationState(
    RunCommandState& current, std::uint64_t monotonicMillis) const {
    auto candidate = current;
    const auto transition =
        propose(candidate.processState, ProcessState::RecoveryEvaluation,
                TransitionReason::RecoveryReentryRequired, monotonicMillis);
    if (!transition.proposed() ||
        !applyProcessTransition(candidate.processState, transition,
                                candidate.processRunSnapshot.has_value()
                                    ? &*candidate.processRunSnapshot
                                    : nullptr)) {
        return false;
    }
    current = std::move(candidate);
    return true;
}

RecoveryActivationOutcome
RunPersistenceCoordinator::activateFallbackRecoveredRun(
    const RunCommandState& current, const RunCheckpointTime& time,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    const auto invalid = [this](const RunCommandState& state) {
        return RecoveryActivationOutcome{
            result(RunPersistenceResultStatus::InvalidDecision,
                   RunPersistenceStep::CandidateApply,
                   RunPersistenceTechnicalReason::InvalidProjection),
            state};
    };
    if (state_ != RunPersistenceCoordinatorState::FallbackRecoveryPending ||
        !currentHead_.has_value() || !currentHead_->fallback.has_value()) {
        return {unavailableResult(), current};
    }
    const auto fallbackReference = *currentHead_->fallback;
    const auto targetSlot = currentHead_->current.slot;
    if (targetSlot > 1U || fallbackReference.slot > 1U ||
        targetSlot == fallbackReference.slot ||
        !slots_[fallbackReference.slot].has_value() ||
        !current.processRunSnapshot.has_value()) {
        return invalid(current);
    }
    // The retained load copy is only an offer. Re-read both the head and the
    // physical fallback slot immediately before any selected mutation so a
    // changed/corrupt source can never be written through stale RAM evidence.
    const auto liveHeadRead = store_.readHead(kMaximumHeadRecordBytes);
    if (liveHeadRead.status != device_platform::StateStoreReadStatus::Success) {
        return invalid(current);
    }
    const auto liveHead = decodeRunPersistenceHead(liveHeadRead.value, epoch_);
    if (!liveHead.has_value() || liveHead->revision != currentHead_->revision ||
        !liveHead->fallback.has_value() ||
        !sameCheckpointReference(liveHead->current, currentHead_->current) ||
        !sameCheckpointReference(*liveHead->fallback, fallbackReference)) {
        return invalid(current);
    }
    const auto liveFallbackRead =
        store_.readSlot(fallbackReference.slot, kMaximumCheckpointRecordBytes);
    if (liveFallbackRead.status !=
        device_platform::StateStoreReadStatus::Success) {
        return invalid(current);
    }
    RunPersistenceRawRecord liveRecord;
    if (!decodeRunPersistenceRecordInto(liveFallbackRead.value, epoch_,
                                        liveRecord) ||
        !runCheckpointReferenceMatches(fallbackReference, liveRecord,
                                       fallbackReference.slot) ||
        !currentMatchesLoadedRecord(current, liveRecord, persistedIds_,
                                    persistedIdCount_) ||
        liveRecord.bytes != slots_[fallbackReference.slot]->bytes) {
        return invalid(current);
    }
    const auto& loadedRecord = liveRecord;
    if (current.processState.state == ProcessState::Fermenting) {
        if (!loadedRecord.utcUnixSeconds.has_value()) {
            return invalid(current);
        }
        if (!time.utcUnixSeconds.has_value()) {
            return {result(RunPersistenceResultStatus::RecoveryPending,
                           RunPersistenceStep::CandidateApply),
                    current};
        }
        return activateR1ExactFermentingCore(
            current, loadedRecord, time, targetSlot,
            RunPersistenceFallbackDirective{
                RunPersistenceFallbackMode::SetExplicitReference,
                fallbackReference},
            RunPersistenceCoordinatorState::FallbackRecoveryPending, true);
    }
    if (!boot_classification::isR1ResumeEligible(loadedRecord.snapshot)) {
        return invalid(current);
    }
    auto candidate = current;
    applyLiveRecoveryEvidence(candidate, liveSensorEvidence);
    RunPersistenceSnapshot snapshot;
    if (!makeRunPersistenceSnapshotInto(
            candidate, persistedIds_, persistedIdCount_,
            RunCheckpointTrigger::Transition, time, schedule_.intervalMinutes(),
            snapshot)) {
        return invalid(current);
    }
    const auto persisted = writeSnapshotCore(
        snapshot, time, false, current, RunPersistenceMutationKind::Recovery,
        std::nullopt, targetSlot,
        RunPersistenceFallbackDirective{
            RunPersistenceFallbackMode::SetExplicitReference,
            fallbackReference},
        RunPersistenceCoordinatorState::FallbackRecoveryPending);
    if (persisted.status != RunPersistenceResultStatus::Applied) {
        return {persisted, current};
    }
    auto applied = persisted;
    applied.step = RunPersistenceStep::RamApply;
    applied.coordinatorState = state_;
    return {applied, std::move(candidate)};
}

RunPersistenceResult RunPersistenceCoordinator::reevaluateRecoveryEvaluation(
    RunCommandState& current, const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    const auto notAllowed = [this]() {
        return result(RunPersistenceResultStatus::NotAllowedInState,
                      RunPersistenceStep::CandidateApply);
    };
    if (state_ != RunPersistenceCoordinatorState::Ready) {
        return unavailableResult();
    }
    if (current.processState.state != ProcessState::RecoveryEvaluation ||
        !current.pendingRecoveryAnchor.has_value() ||
        !current.recoveryBootAnchorMonotonicMillis.has_value() ||
        current.pendingRecoveryAnchor->originalProcessState.state !=
            ProcessState::WaitingForProduct ||
        !current.processRunSnapshot.has_value() ||
        !current.processRunSnapshot->maximumProductWaitMinutes.has_value()) {
        return notAllowed();
    }

    const auto context = deriveRecoveryTimeContext(
        *current.pendingRecoveryAnchor, time.utcUnixSeconds,
        time.monotonicMillis, *current.recoveryBootAnchorMonotonicMillis);
    if (!context.has_value()) return notAllowed();
    const auto verdict = evaluateRecoveryTimeVerdict(
        context->elapsed,
        *current.processRunSnapshot->maximumProductWaitMinutes * 60U);
    if (verdict == RecoveryTimeVerdict::Uncertain) {
        return result(RunPersistenceResultStatus::NotDue,
                      RunPersistenceStep::CandidateApply);
    }
    if (current.runRevision == std::numeric_limits<std::uint32_t>::max()) {
        return result(RunPersistenceResultStatus::CounterOverflow,
                      RunPersistenceStep::CandidateApply);
    }

    auto candidate = current;
    std::array<ProcessMessage, kMaximumTransitionMessages> messages{};
    std::size_t messageCount = 0U;
    RunPersistenceFallbackDirective fallbackDirective{};
    if (verdict == RecoveryTimeVerdict::DefinitelyExpired) {
        const auto tombstone = propose(
            candidate.processState, ProcessState::Standby,
            TransitionReason::RecoveryEndedByExpiredWait, time.monotonicMillis);
        if (!tombstone.proposed() ||
            !applyProcessTransition(candidate.processState, tombstone,
                                    &*candidate.processRunSnapshot)) {
            return notAllowed();
        }
        clearActiveRunState(candidate);
        fallbackDirective = RunPersistenceFallbackDirective{
            RunPersistenceFallbackMode::ClearFallback, std::nullopt};
    } else {
        // A still-valid resume is the existing restart sensor-selection/Gate-A
        // decision. The no-context overload intentionally remains fail-closed.
        if (liveSensorEvidence == nullptr ||
            !candidate.sensorSelection.has_value() ||
            !candidate.activeRunSensorMode.has_value()) {
            return notAllowed();
        }
        updateLastKnownEvidence(candidate.recoveryTemperatureEvidence,
                                *liveSensorEvidence);
        const auto recommendation = computeRestartSensorSelection(
            *candidate.sensorSelection, *candidate.activeRunSensorMode,
            recoverySensorSelectionProgramContext(candidate),
            *liveSensorEvidence);
        candidate.sensorSelectionRuntime = recommendation.runtime;
        candidate.activeRunSensorMode = recommendation.activeMode;
        if (candidate.activeManualRun.has_value()) {
            candidate.activeManualRun->values.sensorMode =
                recommendation.activeMode;
        }
        if (recommendation.runtime.permission !=
            SensorPeltierPermission::Allowed) {
            const auto rejected = decideProcessTransition(
                candidate.processState, &*candidate.processRunSnapshot,
                ProcessSignals{},
                TransitionRequest{ProcessEvent::RecoveryReject, std::nullopt},
                time.monotonicMillis);
            if (!rejected.proposed() ||
                !applyProcessTransition(candidate.processState, rejected,
                                        &*candidate.processRunSnapshot)) {
                return notAllowed();
            }
            if (candidate.lastRecoveryEpisodeEvidence.has_value()) {
                supersedeUnbookedWeightedSegment(
                    candidate.runProgress,
                    candidate.lastRecoveryEpisodeEvidence
                        ->weightedProgressSegmentId);
                candidate.lastRecoveryEpisodeEvidence->weightedProgressSegmentId
                    .reset();
            }
            candidate.pendingRecoveryAnchor.reset();
            candidate.recoveryBootAnchorMonotonicMillis.reset();
            candidate.priorBootPhaseElapsed.reset();
            clearActiveRunState(candidate);
            fallbackDirective = RunPersistenceFallbackDirective{
                RunPersistenceFallbackMode::ClearFallback, std::nullopt};
            messages = rejected.messages;
            messageCount = rejected.messageCount;
        } else {
            const auto accumulated = accumulatedPriorForResume(
                *candidate.pendingRecoveryAnchor, *context);
            if (!accumulated.has_value() ||
                !context->elapsed.totalSecondsUpperBound.has_value()) {
                return notAllowed();
            }
            const auto upper =
                checkedToUint32(*context->elapsed.totalSecondsUpperBound);
            if (!upper.has_value()) return notAllowed();
            const auto recovered = rebasedRecoveredState(
                candidate.pendingRecoveryAnchor->originalProcessState,
                time.monotonicMillis);
            const auto hop2 = decideProcessTransition(
                candidate.processState, &*candidate.processRunSnapshot,
                ProcessSignals{},
                TransitionRequest{ProcessEvent::RecoveryResume, recovered},
                time.monotonicMillis, PriorBootPhaseElapsed{*upper, *upper});
            if (!hop2.proposed() ||
                !applyProcessTransition(candidate.processState, hop2,
                                        &*candidate.processRunSnapshot)) {
                return notAllowed();
            }
            candidate.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
                ProcessState::WaitingForProduct, *accumulated};
            applyLiveRecoveryEvidence(candidate, *liveSensorEvidence);
            if (recoveryTimeResolvedAtResume(candidate.priorBootPhaseElapsed)) {
                candidate.pendingRecoveryAnchor.reset();
                candidate.recoveryBootAnchorMonotonicMillis.reset();
            }
        }
    }

    ++candidate.runRevision;
    const auto persisted =
        persistRecoveryCandidate(current, candidate, time, fallbackDirective);
    if (persisted.status == RunPersistenceResultStatus::Applied) {
        auto applied = persisted;
        applied.messages = messages;
        applied.messageCount = messageCount;
        return applied;
    }
    return persisted;
}

RunPersistenceResult RunPersistenceCoordinator::resolveRecoveryOutcome(
    RunCommandState& current, const ResolveRecoveryUncertaintyRequest& request,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext& liveSensorEvidence) {
    const auto notAllowed = [this]() {
        return result(RunPersistenceResultStatus::NotAllowedInState,
                      RunPersistenceStep::CandidateApply);
    };
    if (state_ != RunPersistenceCoordinatorState::Ready) {
        return unavailableResult();
    }
    if (request.commandId == 0U) {
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    if (request.expectedRunRevision != current.runRevision ||
        request.expectedRecoveryEpisodeRevision !=
            current.recoveryEpisodeRevision) {
        return result(RunPersistenceResultStatus::StaleDecision,
                      RunPersistenceStep::CandidateApply);
    }
    for (std::size_t i = 0U; i < persistedIdCount_; ++i) {
        if (persistedIds_[i] == request.commandId) {
            return result(RunPersistenceResultStatus::AlreadyPersisted);
        }
    }
    if (!current.processRunSnapshot.has_value()) return notAllowed();

    ProcessState phase = ProcessState::RecoveryEvaluation;
    std::optional<RecoveryTimeContext> timeContext;
    if (current.processState.state == ProcessState::RecoveryEvaluation) {
        if (!current.pendingRecoveryAnchor.has_value() ||
            !current.recoveryBootAnchorMonotonicMillis.has_value() ||
            current.pendingRecoveryAnchor->originalProcessState.state !=
                ProcessState::WaitingForProduct ||
            !current.processRunSnapshot->maximumProductWaitMinutes
                 .has_value()) {
            return notAllowed();
        }
        phase = ProcessState::WaitingForProduct;
        timeContext = deriveRecoveryTimeContext(
            *current.pendingRecoveryAnchor, time.utcUnixSeconds,
            time.monotonicMillis, *current.recoveryBootAnchorMonotonicMillis);
        if (!timeContext.has_value()) return notAllowed();
    } else if (current.processState.state == ProcessState::Fermenting ||
               current.processState.state == ProcessState::CoolHolding) {
        phase = current.processState.state;
        if (!current.priorBootPhaseElapsed.has_value() ||
            current.priorBootPhaseElapsed->taggedState != phase) {
            return notAllowed();
        }
        timeContext = RecoveryTimeContext{
            EffectiveAnchorTimeBasis{}, std::nullopt,
            RecoveredPhaseElapsed{
                0U, current.priorBootPhaseElapsed->elapsed.lowerBoundSeconds,
                current.priorBootPhaseElapsed->elapsed.upperBoundSeconds}};
    } else {
        return notAllowed();
    }

    std::uint32_t limitSeconds = 0U;
    if (phase == ProcessState::WaitingForProduct) {
        limitSeconds =
            *current.processRunSnapshot->maximumProductWaitMinutes * 60U;
    } else if (phase == ProcessState::Fermenting) {
        if (!current.processRunSnapshot->fermentationDurationMinutes
                 .has_value()) {
            return notAllowed();
        }
        limitSeconds =
            *current.processRunSnapshot->fermentationDurationMinutes * 60U;
    } else {
        if (!current.processRunSnapshot->holdDurationMinutes.has_value()) {
            return notAllowed();
        }
        limitSeconds = *current.processRunSnapshot->holdDurationMinutes * 60U;
    }
    const auto verdict =
        evaluateRecoveryTimeVerdict(timeContext->elapsed, limitSeconds);
    if (verdict != RecoveryTimeVerdict::Uncertain) return notAllowed();

    auto ids = persistedIds_;
    auto count = persistedIdCount_;
    if (count < ids.size()) {
        ids[count++] = request.commandId;
    } else {
        for (std::size_t i = 1U; i < ids.size(); ++i) ids[i - 1U] = ids[i];
        ids.back() = request.commandId;
    }

    auto candidate = current;
    std::array<ProcessMessage, kMaximumTransitionMessages> messages{};
    std::size_t messageCount = 0U;
    RunPersistenceFallbackDirective fallbackDirective{};
    if (phase == ProcessState::WaitingForProduct &&
        request.decision == RecoveryUncertaintyDecision::AssumeStillValid) {
        if (!candidate.sensorSelection.has_value() ||
            !candidate.activeRunSensorMode.has_value()) {
            return notAllowed();
        }
        updateLastKnownEvidence(candidate.recoveryTemperatureEvidence,
                                liveSensorEvidence);
        const auto recommendation = computeRestartSensorSelection(
            *candidate.sensorSelection, *candidate.activeRunSensorMode,
            recoverySensorSelectionProgramContext(candidate),
            liveSensorEvidence);
        candidate.sensorSelectionRuntime = recommendation.runtime;
        candidate.activeRunSensorMode = recommendation.activeMode;
        if (candidate.activeManualRun.has_value()) {
            candidate.activeManualRun->values.sensorMode =
                recommendation.activeMode;
        }
        if (recommendation.runtime.permission !=
            SensorPeltierPermission::Allowed) {
            const auto rejected = decideProcessTransition(
                candidate.processState, &*candidate.processRunSnapshot,
                ProcessSignals{},
                TransitionRequest{ProcessEvent::RecoveryReject, std::nullopt},
                time.monotonicMillis);
            if (!rejected.proposed() ||
                !applyProcessTransition(candidate.processState, rejected,
                                        &*candidate.processRunSnapshot)) {
                return notAllowed();
            }
            candidate.pendingRecoveryAnchor.reset();
            candidate.recoveryBootAnchorMonotonicMillis.reset();
            candidate.priorBootPhaseElapsed.reset();
            clearActiveRunState(candidate);
            fallbackDirective = RunPersistenceFallbackDirective{
                RunPersistenceFallbackMode::ClearFallback, std::nullopt};
            messages = rejected.messages;
            messageCount = rejected.messageCount;
        } else {
            applyLiveRecoveryEvidence(candidate, liveSensorEvidence);
            const auto accumulated = accumulatedPriorForResume(
                *candidate.pendingRecoveryAnchor, *timeContext);
            if (!accumulated.has_value()) return notAllowed();
            const auto recovered = rebasedRecoveredState(
                candidate.pendingRecoveryAnchor->originalProcessState,
                time.monotonicMillis);
            const auto hop2 = decideProcessTransition(
                candidate.processState, &*candidate.processRunSnapshot,
                ProcessSignals{},
                TransitionRequest{ProcessEvent::RecoveryResume, recovered},
                time.monotonicMillis, *accumulated);
            if (!hop2.proposed() ||
                !applyProcessTransition(candidate.processState, hop2,
                                        &*candidate.processRunSnapshot)) {
                return notAllowed();
            }
            candidate.priorBootPhaseElapsed = TaggedPriorBootPhaseElapsed{
                ProcessState::WaitingForProduct, *accumulated};
            applyLiveRecoveryEvidence(candidate, liveSensorEvidence);
            if (recoveryTimeResolvedAtResume(candidate.priorBootPhaseElapsed)) {
                candidate.pendingRecoveryAnchor.reset();
                candidate.recoveryBootAnchorMonotonicMillis.reset();
            }
        }
    } else if (phase == ProcessState::WaitingForProduct &&
               request.decision ==
                   RecoveryUncertaintyDecision::AssumeThresholdCrossed) {
        const auto tombstone = propose(
            candidate.processState, ProcessState::Standby,
            TransitionReason::RecoveryEndedByExpiredWait, time.monotonicMillis);
        if (!tombstone.proposed() ||
            !applyProcessTransition(candidate.processState, tombstone,
                                    &*candidate.processRunSnapshot)) {
            return notAllowed();
        }
        clearActiveRunState(candidate);
    } else if (request.decision ==
                   RecoveryUncertaintyDecision::AssumeThresholdCrossed &&
               (phase == ProcessState::Fermenting ||
                phase == ProcessState::CoolHolding)) {
        if (!current.priorBootPhaseElapsed->elapsed.upperBoundSeconds
                 .has_value()) {
            return notAllowed();
        }
        const auto completion =
            phase == ProcessState::Fermenting
                ? completeTimedRun(candidate.processState,
                                   *candidate.processRunSnapshot,
                                   time.monotonicMillis)
                : completeHoldDuration(candidate.processState,
                                       time.monotonicMillis);
        if (!completion.proposed() ||
            !applyProcessTransition(candidate.processState, completion,
                                    &*candidate.processRunSnapshot)) {
            return notAllowed();
        }
        messages = completion.messages;
        messageCount = completion.messageCount;
        if (candidate.lastRecoveryEpisodeEvidence.has_value()) {
            supersedeUnbookedWeightedSegment(
                candidate.runProgress, candidate.lastRecoveryEpisodeEvidence
                                           ->weightedProgressSegmentId);
            candidate.lastRecoveryEpisodeEvidence->weightedProgressSegmentId
                .reset();
        }
        candidate.pendingRecoveryAnchor.reset();
        candidate.recoveryBootAnchorMonotonicMillis.reset();
        candidate.priorBootPhaseElapsed.reset();
    } else {
        return notAllowed();
    }

    const auto snapshot = makeRunPersistenceSnapshot(
        candidate, ids, count, RunCheckpointTrigger::Command, time,
        schedule_.intervalMinutes());
    if (!snapshot.has_value()) {
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    const auto persisted =
        fallbackDirective.mode == RunPersistenceFallbackMode::ClearFallback
            ? writeSnapshotCore(*snapshot, time, false, current,
                                RunPersistenceMutationKind::Command,
                                request.commandId, std::nullopt,
                                fallbackDirective,
                                RunPersistenceCoordinatorState::Ready)
            : writeSnapshot(*snapshot, time, false, current,
                            RunPersistenceMutationKind::Command,
                            request.commandId);
    if (persisted.status != RunPersistenceResultStatus::Applied) {
        return persisted;
    }
    current = candidate;
    persistedIds_ = ids;
    persistedIdCount_ = count;
    auto applied = persisted;
    applied.step = RunPersistenceStep::RamApply;
    applied.durability = RunPersistenceDurability::Changed;
    applied.coordinatorState = state_;
    applied.messages = messages;
    applied.messageCount = messageCount;
    return applied;
}

RunPersistenceResult RunPersistenceCoordinator::writeSnapshot(
    const RunPersistenceSnapshot& snapshot, const RunCheckpointTime& time,
    bool periodic, const RunCommandState& before,
    RunPersistenceMutationKind mutationKind,
    std::optional<CommandId> commandId) {
    if (state_ != RunPersistenceCoordinatorState::Ready &&
        state_ != RunPersistenceCoordinatorState::ReadyEmpty)
        return unavailableResult();
    const auto rollbackState = state_;
    return writeSnapshotCore(snapshot, time, periodic, before, mutationKind,
                             commandId, std::nullopt,
                             RunPersistenceFallbackDirective{}, rollbackState);
}

RunPersistenceResult RunPersistenceCoordinator::discardAsNoActiveRun(
    RunCommandState& current, const RunCheckpointTime& time) {
    if (state_ != RunPersistenceCoordinatorState::Ready &&
        state_ != RunPersistenceCoordinatorState::LoadedActiveRun) {
        return unavailableResult();
    }
    if (!current.activeProgramRun.has_value() &&
        !current.activeManualRun.has_value()) {
        return result(RunPersistenceResultStatus::NoActiveRun);
    }
    if (current.runRevision == std::numeric_limits<std::uint32_t>::max()) {
        return result(RunPersistenceResultStatus::CounterOverflow,
                      RunPersistenceStep::CandidateApply);
    }

    workingSet_.candidate = current;
    const auto discard =
        propose(workingSet_.candidate.processState, ProcessState::Standby,
                TransitionReason::RecoveryRejected, time.monotonicMillis);
    if (!discard.proposed() ||
        !applyProcessTransition(
            workingSet_.candidate.processState, discard,
            workingSet_.candidate.processRunSnapshot.has_value()
                ? &*workingSet_.candidate.processRunSnapshot
                : nullptr)) {
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    clearActiveRunState(workingSet_.candidate);
    ++workingSet_.candidate.runRevision;

    if (!makeRunPersistenceSnapshotInto(
            workingSet_.candidate, persistedIds_, persistedIdCount_,
            RunCheckpointTrigger::Transition, time, schedule_.intervalMinutes(),
            workingSet_.snapshot)) {
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    const auto rollbackState = state_;
    const auto persisted = writeSnapshotCore(
        workingSet_.snapshot, time, false, current,
        RunPersistenceMutationKind::Recovery, std::nullopt, std::nullopt,
        RunPersistenceFallbackDirective{
            RunPersistenceFallbackMode::ClearFallback, std::nullopt},
        rollbackState);
    if (persisted.status != RunPersistenceResultStatus::Applied) {
        return persisted;
    }
    current = workingSet_.candidate;
    return persisted;
}

RunPersistenceResult RunPersistenceCoordinator::writeSnapshotCore(
    const RunPersistenceSnapshot& snapshot, const RunCheckpointTime& time,
    bool periodic, const RunCommandState& before,
    RunPersistenceMutationKind mutationKind, std::optional<CommandId> commandId,
    std::optional<std::size_t> targetSlotOverride,
    RunPersistenceFallbackDirective fallbackDirective,
    RunPersistenceCoordinatorState rollbackState) {
    // R1 write invariant: an active FERMENTING Current is the recovery anchor
    // consumed by #124 and therefore must carry trusted UTC.  This check is
    // deliberately before schedule/counter/state mutation and is shared by
    // command, transition, sensor-selection, periodic, and recovery writes.
    if (isActiveFermentingSnapshot(snapshot) &&
        !time.utcUnixSeconds.has_value()) {
        return result(
            RunPersistenceResultStatus::Blocked,
            RunPersistenceStep::CandidateApply,
            RunPersistenceTechnicalReason::TrustedAbsoluteTimeRequired);
    }
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
    std::optional<CommandId> previousCommandIdHighWater;
    if (currentHead_.has_value()) {
        previousCommandIdHighWater = currentHead_->commandIdHighWater;
    } else if (state_ == RunPersistenceCoordinatorState::ReadyEmpty) {
        previousCommandIdHighWater = CommandId{0U};
    }
    if (commandId.has_value()) {
        if (!previousCommandIdHighWater.has_value() || *commandId == 0U) {
            return result(RunPersistenceResultStatus::Blocked,
                          RunPersistenceStep::CandidateApply,
                          RunPersistenceTechnicalReason::InvalidProjection);
        }
        if (*previousCommandIdHighWater ==
            std::numeric_limits<CommandId>::max()) {
            return result(RunPersistenceResultStatus::CounterOverflow,
                          RunPersistenceStep::CandidateApply);
        }
        if (*previousCommandIdHighWater != 0U &&
            *commandId <= *previousCommandIdHighWater) {
            return result(RunPersistenceResultStatus::Blocked,
                          RunPersistenceStep::CandidateApply,
                          RunPersistenceTechnicalReason::InvalidProjection);
        }
        previousCommandIdHighWater =
            std::max(*previousCommandIdHighWater, *commandId);
    }
    state_ = RunPersistenceCoordinatorState::Busy;
    const std::size_t target =
        targetSlotOverride.has_value()
            ? *targetSlotOverride
            : (currentHead_.has_value() ? 1U - currentHead_->current.slot : 0U);
    const bool sameCurrentSlot =
        currentHead_.has_value() && target == currentHead_->current.slot;
    const auto sameReference = [](const RunCheckpointReference& left,
                                  const RunCheckpointReference& right) {
        return left.slot == right.slot &&
               left.schemaVersion == right.schemaVersion &&
               left.storageEpoch == right.storageEpoch &&
               left.checkpointRevision == right.checkpointRevision &&
               left.payloadLength == right.payloadLength &&
               left.payloadCrc == right.payloadCrc &&
               left.variant == right.variant;
    };
    const bool clearFallbackAllowed =
        snapshot.variant == RunCheckpointVariant::NoActiveRun ||
        (snapshot.variant != RunCheckpointVariant::NoActiveRun &&
         snapshot.processState.state == ProcessState::Fault);
    const bool explicitFallbackValid =
        fallbackDirective.mode ==
            RunPersistenceFallbackMode::SetExplicitReference &&
        fallbackDirective.reference.has_value() && currentHead_.has_value() &&
        currentHead_->fallback.has_value() &&
        fallbackDirective.reference->slot != target &&
        sameReference(*fallbackDirective.reference, *currentHead_->fallback);
    const bool directiveValid =
        (fallbackDirective.mode ==
             RunPersistenceFallbackMode::UseStandardFallback &&
         !fallbackDirective.reference.has_value()) ||
        (fallbackDirective.mode == RunPersistenceFallbackMode::ClearFallback &&
         !fallbackDirective.reference.has_value() && clearFallbackAllowed) ||
        explicitFallbackValid;
    const bool validFallbackRecoverySameSlot =
        !periodic &&
        rollbackState ==
            RunPersistenceCoordinatorState::FallbackRecoveryPending &&
        mutationKind == RunPersistenceMutationKind::Recovery &&
        sameCurrentSlot && currentHead_->fallback.has_value() &&
        ((fallbackDirective.mode == RunPersistenceFallbackMode::ClearFallback &&
          clearFallbackAllowed) ||
         explicitFallbackValid);
    if (target > 1U || !directiveValid) {
        state_ = rollbackState;
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    if (sameCurrentSlot && !validFallbackRecoverySameSlot) {
        state_ = rollbackState;
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    std::string payload;
    if (encodeRunPersistenceSnapshot(snapshot, payload) !=
        RunPersistenceCodecStatus::Success) {
        state_ = rollbackState;
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CheckpointSlot,
                      RunPersistenceTechnicalReason::CodecError);
    }
    device_platform::StorageEnvelope env{
        kCheckpointRecordType,   kCurrentRunPersistenceSchema, epoch_,
        nextCheckpointRevision_, time.utcUnixSeconds,          payload};
    std::string targetBytes;
    if (device_platform::encodeEnvelope(env, targetBytes,
                                        kMaximumCheckpointRecordBytes) !=
        device_platform::EnvelopeEncodeStatus::Success) {
        state_ = rollbackState;
        return result(RunPersistenceResultStatus::CapacityExceeded,
                      RunPersistenceStep::CheckpointSlot,
                      RunPersistenceTechnicalReason::CodecError);
    }
    workingSet_.record.bytes = targetBytes;
    workingSet_.record.snapshot = snapshot;
    workingSet_.record.checkpointRevision = nextCheckpointRevision_;
    workingSet_.record.utcUnixSeconds = time.utcUnixSeconds;
    const auto ref =
        makeRunCheckpointReference(target, workingSet_.record, epoch_);
    RunPersistenceHead prepared;
    RunPersistenceHead committed;
    std::optional<std::string> preparedBytes;
    std::optional<std::string> committedBytes;
    if (periodic) {
        committed.state = RunPersistenceHeadState::Committed;
        committed.revision = nextHeadRevision_;
        committed.current = ref;
        committed.commandIdHighWater = previousCommandIdHighWater;
        if (snapshot.variant != RunCheckpointVariant::NoActiveRun &&
            currentHead_.has_value()) {
            if (fallbackDirective.mode ==
                RunPersistenceFallbackMode::SetExplicitReference) {
                committed.fallback = fallbackDirective.reference;
            } else if (fallbackDirective.mode ==
                       RunPersistenceFallbackMode::UseStandardFallback) {
                committed.fallback = currentHead_->current;
            }
        }
        committedBytes = encodeRunPersistenceHead(committed, epoch_);
        if (!committedBytes.has_value()) {
            state_ = rollbackState;
            return result(RunPersistenceResultStatus::CapacityExceeded,
                          RunPersistenceStep::CommittedHead,
                          RunPersistenceTechnicalReason::CodecError,
                          RunPersistenceDurability::Unchanged);
        }
    } else {
        prepared.state = RunPersistenceHeadState::Prepared;
        prepared.revision = nextHeadRevision_;
        prepared.target = ref;
        prepared.mutationKind = mutationKind;
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
        prepared.commandIdHighWater = previousCommandIdHighWater;
        committed.commandIdHighWater = previousCommandIdHighWater;
        if (snapshot.variant != RunCheckpointVariant::NoActiveRun &&
            currentHead_.has_value()) {
            if (fallbackDirective.mode ==
                RunPersistenceFallbackMode::SetExplicitReference) {
                committed.fallback = fallbackDirective.reference;
            } else if (fallbackDirective.mode ==
                       RunPersistenceFallbackMode::UseStandardFallback) {
                committed.fallback = currentHead_->current;
            }
        }
        preparedBytes = encodeRunPersistenceHead(prepared, epoch_);
        committedBytes = encodeRunPersistenceHead(committed, epoch_);
        if (!preparedBytes.has_value() || !committedBytes.has_value()) {
            state_ = rollbackState;
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

    auto readyState = [this, rollbackState]() { state_ = rollbackState; };
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
        slots_[target] = workingSet_.record;
        nextCheckpointRevision_ =
            workingSet_.record.checkpointRevision ==
                    std::numeric_limits<std::uint64_t>::max()
                ? 0U
                : workingSet_.record.checkpointRevision + 1U;
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
        slots_[target] = workingSet_.record;
        nextCheckpointRevision_ =
            workingSet_.record.checkpointRevision ==
                    std::numeric_limits<std::uint64_t>::max()
                ? 0U
                : workingSet_.record.checkpointRevision + 1U;
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
    slots_[target] = workingSet_.record;
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
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    if (state_ != RunPersistenceCoordinatorState::Ready &&
        state_ != RunPersistenceCoordinatorState::ReadyEmpty)
        return unavailableResult();
    if (!decision.proposed() || !isPersistedRunCommand(decision.kind))
        return result(decision.proposed()
                          ? RunPersistenceResultStatus::NotEligible
                          : RunPersistenceResultStatus::InvalidDecision);
    if (decision.effectCount > decision.effects.size())
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    if (time.monotonicMillis != decision.envelope.monotonicMillis)
        return result(RunPersistenceResultStatus::TimeMismatch);
    // Ein bereits durabel persistiertes CommandId bleibt gesperrt, auch wenn
    // die neu eintreffende Entscheidung (z. B. nach einer erneuten
    // Bewertung) diesmal AppliedRamOnly waere - persistedIds_ hat Vorrang
    // vor der folgenden RAM-only-Sonderbehandlung.
    for (std::size_t i = 0U; i < persistedIdCount_; ++i)
        if (persistedIds_[i] == decision.envelope.id)
            return result(RunPersistenceResultStatus::AlreadyPersisted);
    if (!commandIdHighWater().has_value()) {
        return result(RunPersistenceResultStatus::Blocked,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    const bool freshProductiveStart =
        decision.kind == CommandKind::StartProgram ||
        decision.kind == CommandKind::StartManualHolding;
    if (freshProductiveStart && !time.utcUnixSeconds.has_value()) {
        // The application/domain boundary uses the existing honest Blocked
        // result.  No candidate Apply, durable write, or RAM Apply is allowed
        // to happen before trusted absolute time exists.
        return result(
            RunPersistenceResultStatus::Blocked,
            RunPersistenceStep::CandidateApply,
            RunPersistenceTechnicalReason::TrustedAbsoluteTimeRequired);
    }
    // #21, 9.7-Korrektur (letzter Abschlussblocker): AppliedRamOnly ist per
    // Plan-Vertrag nicht persistierbar - der manuelle Sensorselektionspfad
    // wendet diese Entscheidung ausschliesslich im RAM an, stale-geprueft
    // ueber applyRunCommand's bestehende before/after-Pruefung (identisch
    // zu jedem anderen Kommando), ohne Store-Write, ohne persistierte
    // CommandId (persistedIds_ bleibt unberuehrt) und ohne
    // Laufrevisionsaenderung (AppliedRamOnly haelt resultingRunRevision
    // unveraendert). Dieselbe CommandId bleibt dabei nur innerhalb des
    // laufenden Boots ueber RunCommandState::processedCommandIds fluechtig
    // idempotent (AlreadyProcessed) - dieses Feld ist RAM-only und wird bei
    // keinem Neustart wiederhergestellt, die CommandId gilt danach nicht als
    // verbraucht. sensorSelectionEvent/-Notice/startSensorSelectionNotice
    // bleiben in diesem Ergebnis bewusst leer: 6.11 macht sie erst nach
    // einem erfolgreichen Commit sichtbar, den dieser Pfad nie durchfuehrt,
    // und AppliedRamOnly fuellt sie ohnehin nie.
    if (decision.kind == CommandKind::ApplySensorSelectionAction &&
        decision.sensorSelectionApplyStatus.has_value() &&
        *decision.sensorSelectionApplyStatus ==
            SensorSelectionApplyStatus::AppliedRamOnly) {
        const auto ramApply = applyRunCommand(current, decision);
        if (ramApply == CommandStatus::AlreadyProcessed)
            return result(RunPersistenceResultStatus::AlreadyProcessed);
        if (ramApply != CommandStatus::Applied)
            return result(ramApply == CommandStatus::StaleState
                              ? RunPersistenceResultStatus::StaleDecision
                              : RunPersistenceResultStatus::InvalidDecision,
                          RunPersistenceStep::CandidateApply);
        if (liveSensorEvidence != nullptr)
            applyLiveRecoveryEvidence(current, *liveSensorEvidence);
        RunPersistenceResult ramResult{};
        ramResult.status = RunPersistenceResultStatus::Applied;
        ramResult.step = RunPersistenceStep::RamApply;
        ramResult.durability = RunPersistenceDurability::Unchanged;
        ramResult.coordinatorState = state_;
        ramResult.effects = decision.effects;
        ramResult.effectCount = decision.effectCount;
        return ramResult;
    }
#if defined(APP_ISSUE_29_BRINGUP_PROBE)
    // The probe names this existing candidate-copy boundary as its
    // allocation-failure seam. No production allocator or public error
    // contract is introduced; release/native builds do not compile this
    // branch at all.
    if (issue_29_bringup::consumeCandidateAllocationFailure()) {
        return result(RunPersistenceResultStatus::Blocked,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection,
                      RunPersistenceDurability::Unchanged);
    }
#endif
    workingSet_.candidate = current;
    const auto apply = applyRunCommand(workingSet_.candidate, decision);
    if (apply != CommandStatus::Applied)
        return result(apply == CommandStatus::StaleState
                          ? RunPersistenceResultStatus::StaleDecision
                          : RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply);
    if (liveSensorEvidence != nullptr)
        applyLiveRecoveryEvidence(workingSet_.candidate, *liveSensorEvidence);
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
    if (!makeRunPersistenceSnapshotInto(
            workingSet_.candidate, ids, count, RunCheckpointTrigger::Command,
            time, schedule_.intervalMinutes(), workingSet_.snapshot))
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    const auto persisted = writeSnapshot(
        workingSet_.snapshot, time, false, current,
        RunPersistenceMutationKind::Command, decision.envelope.id);
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
    if (liveSensorEvidence != nullptr)
        applyLiveRecoveryEvidence(current, *liveSensorEvidence);
    persistedIds_ = ids;
    persistedIdCount_ = count;
    RunPersistenceResult result{};
    result.status = RunPersistenceResultStatus::Applied;
    result.step = RunPersistenceStep::RamApply;
    result.durability = RunPersistenceDurability::Changed;
    result.coordinatorState = state_;
    result.effects = decision.effects;
    result.effectCount = decision.effectCount;
    // #21, 6.11: Event/Notice erst nach erfolgreichem Commit sichtbar -
    // dasselbe bestehende Muster wie result.effects oben.
    result.sensorSelectionEvent = decision.sensorSelectionEvent;
    result.sensorSelectionNotice = decision.sensorSelectionNotice;
    result.startSensorSelectionNotice = decision.startSensorSelectionNotice;
    result.committedControlContextTransition =
        decision.committedControlContextTransition;
    return result;
}

RunPersistenceResult RunPersistenceCoordinator::persistRecoveryCandidate(
    RunCommandState& current, const RunCommandState& candidate,
    const RunCheckpointTime& time,
    RunPersistenceFallbackDirective fallbackDirective) {
    if (state_ != RunPersistenceCoordinatorState::Ready &&
        state_ != RunPersistenceCoordinatorState::LoadedActiveRun) {
        return unavailableResult();
    }
    if (current.runRevision == std::numeric_limits<std::uint32_t>::max() ||
        candidate.runRevision != current.runRevision + 1U) {
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    if (!makeRunPersistenceSnapshotInto(
            candidate, persistedIds_, persistedIdCount_,
            RunCheckpointTrigger::Transition, time, schedule_.intervalMinutes(),
            workingSet_.snapshot)) {
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    const auto rollbackState = state_;
    const auto persisted =
        writeSnapshotCore(workingSet_.snapshot, time, false, current,
                          RunPersistenceMutationKind::Recovery, std::nullopt,
                          std::nullopt, fallbackDirective, rollbackState);
    if (persisted.status != RunPersistenceResultStatus::Applied) {
        return persisted;
    }
    current = candidate;
    pendingR1CheckpointRevision_.reset();
    pendingR1RunRevision_.reset();
    auto applied = persisted;
    applied.step = RunPersistenceStep::RamApply;
    applied.durability = RunPersistenceDurability::Changed;
    applied.coordinatorState = state_;
    return applied;
}

RunPersistenceResult RunPersistenceCoordinator::persistTransition(
    RunCommandState& current, const TransitionDecision& decision,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    if (state_ != RunPersistenceCoordinatorState::Ready)
        return unavailableResult();
    if (!decision.proposed() || !eligibleTransition(decision.reason))
        return result(RunPersistenceResultStatus::InvalidDecision);
    if (decision.messageCount > decision.messages.size())
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    if (time.monotonicMillis != decision.monotonicMillis)
        return result(RunPersistenceResultStatus::TimeMismatch);
    workingSet_.candidate = current;
    std::optional<std::uint32_t> observedFermentingDelta;
    const bool foldsObservedFermentingTime =
        current.processState.state == ProcessState::Fermenting &&
        decision.after.state != ProcessState::Fermenting;
    if (foldsObservedFermentingTime) {
        observedFermentingDelta =
            deriveFermentingSecondsDelta(current, decision.monotonicMillis);
        if (!observedFermentingDelta.has_value()) {
            return result(RunPersistenceResultStatus::InvalidDecision,
                          RunPersistenceStep::CandidateApply);
        }
        if (!foldObservedRunSeconds(workingSet_.candidate,
                                    *observedFermentingDelta)) {
            return result(RunPersistenceResultStatus::CounterOverflow,
                          RunPersistenceStep::CandidateApply);
        }
    }
    if (!workingSet_.candidate.processRunSnapshot.has_value() ||
        !applyProcessTransition(workingSet_.candidate.processState, decision,
                                &*workingSet_.candidate.processRunSnapshot))
        return result(RunPersistenceResultStatus::StaleDecision,
                      RunPersistenceStep::CandidateApply);
    if (decision.reason == TransitionReason::ProductWaitExpired)
        clearActiveRunState(workingSet_.candidate);
    if (workingSet_.candidate.lastRecoveryEpisodeEvidence.has_value() &&
        workingSet_.candidate.processState.state !=
            current.processState.state) {
        supersedeUnbookedWeightedSegment(
            workingSet_.candidate.runProgress,
            workingSet_.candidate.lastRecoveryEpisodeEvidence
                ->weightedProgressSegmentId);
        workingSet_.candidate.lastRecoveryEpisodeEvidence
            ->weightedProgressSegmentId.reset();
    }
    if (liveSensorEvidence != nullptr)
        applyLiveRecoveryEvidence(workingSet_.candidate, *liveSensorEvidence);
    if (!makeRunPersistenceSnapshotInto(
            workingSet_.candidate, persistedIds_, persistedIdCount_,
            RunCheckpointTrigger::Transition, time, schedule_.intervalMinutes(),
            workingSet_.snapshot))
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    const auto persisted =
        writeSnapshot(workingSet_.snapshot, time, false, current,
                      RunPersistenceMutationKind::Transition);
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
    if (foldsObservedFermentingTime) {
        // `current` has already transitioned, so use the exact checked delta
        // derived from the pre-transition state.
        if (!observedFermentingDelta.has_value() ||
            !foldObservedRunSeconds(current, *observedFermentingDelta)) {
            state_ =
                RunPersistenceCoordinatorState::PersistenceCommittedApplyFailed;
            return result(
                RunPersistenceResultStatus::PersistenceCommittedApplyFailed,
                RunPersistenceStep::RamApply,
                RunPersistenceTechnicalReason::InvalidProjection,
                RunPersistenceDurability::Changed);
        }
    }
    if (decision.reason == TransitionReason::ProductWaitExpired)
        clearActiveRunState(current);
    if (liveSensorEvidence != nullptr)
        applyLiveRecoveryEvidence(current, *liveSensorEvidence);
    RunPersistenceResult result{};
    result.status = RunPersistenceResultStatus::Applied;
    result.step = RunPersistenceStep::RamApply;
    result.durability = RunPersistenceDurability::Changed;
    result.coordinatorState = state_;
    result.messages = decision.messages;
    result.messageCount = decision.messageCount;
    if (decision.reason == TransitionReason::ProductInserted) {
        const auto beforeRole = resolveEffectiveControlSensorRole(
            decision.before.state, current.activeRunSensorMode);
        const auto afterRole = resolveEffectiveControlSensorRole(
            current.processState.state, current.activeRunSensorMode);
        result.committedControlContextTransition =
            resolveProductInsertedControlContextTransition(beforeRole,
                                                           afterRole);
    } else {
        result.committedControlContextTransition =
            decision.committedControlContextTransition;
    }
    return result;
}

RunPersistenceResult RunPersistenceCoordinator::checkpointPeriodic(
    const RunCommandState& current, const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
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
    RunPersistenceSnapshot expectedSnapshot;
    const bool expectedValid = makeRunPersistenceSnapshotInto(
        current, persistedIds_, persistedIdCount_, confirmed.trigger,
        RunCheckpointTime{confirmed.checkpointMonotonicMillis, std::nullopt},
        confirmed.intervalMinutes, expectedSnapshot);
    std::string expectedBytes;
    std::string confirmedBytes;
    if (!expectedValid ||
        encodeRunPersistenceSnapshot(expectedSnapshot, expectedBytes) !=
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
    if (isActiveFermentingCurrent(current) &&
        !time.utcUnixSeconds.has_value()) {
        // No copy, schedule confirmation, or RAM change occurs for this
        // skipped checkpoint.  The existing Current remains the last valid
        // UTC-bearing recovery anchor.
        return result(
            RunPersistenceResultStatus::Blocked,
            RunPersistenceStep::CandidateApply,
            RunPersistenceTechnicalReason::TrustedAbsoluteTimeRequired);
    }
    workingSet_.candidate = current;
    if (liveSensorEvidence != nullptr)
        applyLiveRecoveryEvidence(workingSet_.candidate, *liveSensorEvidence);
    const bool snapshotValid = makeRunPersistenceSnapshotInto(
        workingSet_.candidate, persistedIds_, persistedIdCount_,
        RunCheckpointTrigger::Periodic, time, schedule_.intervalMinutes(),
        workingSet_.snapshot);
    // mutationKind is inert here: a periodic write only ever produces a
    // Committed head, which carries no mutation-kind field
    // (validCommittedHead).
    return snapshotValid
               ? writeSnapshot(workingSet_.snapshot, time, true, current,
                               RunPersistenceMutationKind::Transition)
               : result(RunPersistenceResultStatus::InvalidDecision,
                        RunPersistenceStep::CandidateApply,
                        RunPersistenceTechnicalReason::InvalidProjection);
}

namespace {

// 6.14.3: exactly the six automatic causes. ManualUserFallback/
// ManualUserReturn are excluded on purpose - those route through
// persistCommand (#21 Commit 4), never through this automatic path.
bool automaticSensorSelectionCause(SensorSelectionDecisionCause cause) {
    switch (cause) {
        case SensorSelectionDecisionCause::ProductFailureBlock:
        case SensorSelectionDecisionCause::FallbackToAir:
        case SensorSelectionDecisionCause::AutomaticValidatedReturn:
        case SensorSelectionDecisionCause::RecoveryRevalidation:
        case SensorSelectionDecisionCause::SafeStateEntry:
        case SensorSelectionDecisionCause::ReturnValidationAborted:
            return true;
        case SensorSelectionDecisionCause::None:
        case SensorSelectionDecisionCause::StartSelection:
        case SensorSelectionDecisionCause::ManualUserFallback:
        case SensorSelectionDecisionCause::ManualUserReturn:
            return false;
    }
    return false;
}

}  // namespace

RunPersistenceResult RunPersistenceCoordinator::persistSensorSelection(
    RunCommandState& current, const SensorSelectionStateMutation& mutation,
    const RunCheckpointTime& time,
    const CrossRolePlausibilityContext* liveSensorEvidence) {
    if (state_ == RunPersistenceCoordinatorState::ReadyEmpty)
        return result(RunPersistenceResultStatus::NoActiveRun);
    if (state_ != RunPersistenceCoordinatorState::Ready)
        return unavailableResult();
    if (!current.activeProgramRun.has_value() &&
        !current.activeManualRun.has_value())
        return result(RunPersistenceResultStatus::NoActiveRun);
    if (current.activeRunId.empty() ||
        !current.activeRunSensorMode.has_value() ||
        !current.sensorSelection.has_value())
        return result(RunPersistenceResultStatus::NotEligible);
    if (mutation.status !=
        SensorSelectionApplyStatus::AppliedPersistentCandidate)
        return result(RunPersistenceResultStatus::InvalidDecision);
    if (!mutation.persisted.has_value() || !mutation.activeMode.has_value())
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    const auto cause = mutation.persisted->lastDecisionCause;
    if (!automaticSensorSelectionCause(cause))
        return result(RunPersistenceResultStatus::NotEligible);
    if (mutation.runtime.lastAppliedMonotonicMillis.has_value() &&
        *mutation.runtime.lastAppliedMonotonicMillis != time.monotonicMillis)
        return result(RunPersistenceResultStatus::TimeMismatch);
    // Korrekturauftrag Befund 1 (dritter Punkt): `mutation` ist eine bereits
    // ausserhalb berechnete Entscheidung (applySensorSelectionDecision auf
    // einer fruehreren Kopie von `current`) - vor jedem Write/RAM-Apply wird
    // die Revisionsfolge stale-sicher gegen den tatsaechlich aktuellen
    // `current`-Zustand geprueft. Nur AppliedPersistentCandidate erreicht
    // diesen Punkt (siehe Filter oben), die erwartete Folge ist deshalb immer
    // genau +1.
    if (mutation.resultingRunRevision != current.runRevision + 1U)
        return result(RunPersistenceResultStatus::StaleDecision,
                      RunPersistenceStep::CandidateApply);
    if (nextCheckpointRevision_ == 0U)
        return result(RunPersistenceResultStatus::CounterOverflow);
    // Korrekturauftrag Befund 1 (vierter Punkt): der Effekt wird unten aus
    // dem tatsaechlichen Before/After-Permission-Uebergang abgeleitet, nicht
    // aus der Ursache - `beforePermission` wird deshalb festgehalten, bevor
    // der gemeinsame Mutationshelfer `current`/`candidate` veraendert.
    const auto beforePermission = current.sensorSelectionRuntime.permission;
    const auto beforeMode = *current.activeRunSensorMode;
    const auto beforeProcessState = current.processState.state;
    workingSet_.candidate = current;
    // Korrekturauftrag Befund 1 (zweiter Punkt): gemeinsamer mechanischer
    // Mutationshelfer mit dem manuellen Pfad (run_commands.cpp::
    // decideApplySensorSelectionAction) - das schliesst die zuvor fehlende
    // Uebernahme von mutation.runtime (sensorSelectionRuntime blieb bislang
    // auf dem alten Wert stehen) und haelt den in ManualRunPlan::values
    // duplizierten Sensormodus konsistent.
    applySensorSelectionMutation(workingSet_.candidate, mutation);
    if (mutation.event.has_value() &&
        resolveControlSensorRoleTransition(
            beforeProcessState, current.activeRunSensorMode,
            workingSet_.candidate.processState.state,
            workingSet_.candidate.activeRunSensorMode)
            .has_value() &&
        !applySensorRoleChangeQualificationReset(workingSet_.candidate,
                                                 time.monotonicMillis)) {
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    }
    if (liveSensorEvidence != nullptr)
        applyLiveRecoveryEvidence(workingSet_.candidate, *liveSensorEvidence);
    if (!makeRunPersistenceSnapshotInto(
            workingSet_.candidate, persistedIds_, persistedIdCount_,
            RunCheckpointTrigger::SensorSelection, time,
            schedule_.intervalMinutes(), workingSet_.snapshot))
        return result(RunPersistenceResultStatus::InvalidDecision,
                      RunPersistenceStep::CandidateApply,
                      RunPersistenceTechnicalReason::InvalidProjection);
    const auto persisted =
        writeSnapshot(workingSet_.snapshot, time, false, current,
                      RunPersistenceMutationKind::SensorSelection);
    if (persisted.status != RunPersistenceResultStatus::Applied)
        return persisted;
    applySensorSelectionMutation(current, mutation);
    current.processState = workingSet_.candidate.processState;
    if (liveSensorEvidence != nullptr)
        applyLiveRecoveryEvidence(current, *liveSensorEvidence);
    RunPersistenceResult out{};
    out.status = RunPersistenceResultStatus::Applied;
    out.step = RunPersistenceStep::RamApply;
    out.durability = RunPersistenceDurability::Changed;
    out.coordinatorState = state_;
    // 6.14.4: SensorSelectionPermissionRestored != direct actor release -
    // only that #21's own precondition (peltierPermission) is satisfied
    // again. Korrekturauftrag Befund 1: derived from the actual before/after
    // transition (mirrors the manual path in run_commands.cpp), not from an
    // enumerated cause list - the cause list was correct only as long as the
    // six known automatic causes stayed exhaustive and never changed
    // behavior; the transition itself is the single source of truth either
    // way.
    if (beforePermission != mutation.runtime.permission) {
        out.effects[0] =
            mutation.runtime.permission == SensorPeltierPermission::Blocked
                ? CommandEffect::SensorSelectionPermissionBlocked
                : CommandEffect::SensorSelectionPermissionRestored;
        out.effectCount = 1U;
    }
    if (mutation.event.has_value()) {
        auto event = *mutation.event;
        event.utcUnixSeconds = time.utcUnixSeconds;
        out.sensorSelectionEvent = event;
        out.committedControlContextTransition =
            resolveControlSensorRoleTransition(beforeProcessState, beforeMode,
                                               current.processState.state,
                                               mutation.activeMode);
    } else if (mutation.notice.has_value()) {
        out.sensorSelectionNotice = mutation.notice;
    }
    return out;
}

}  // namespace fermentation
