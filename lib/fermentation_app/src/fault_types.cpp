#include "fault_types.hpp"

#include <limits>

namespace fermentation {
namespace {

struct FaultCodeInfo {
    FaultCode code;
    FaultClass faultClass;
    std::uint8_t priority;
    const char* text;
};

constexpr FaultCodeInfo kCodeInfo[] = {
    {FaultCode::P1_001, FaultClass::ProcessWarning, 1U, "P1-001"},
    {FaultCode::P1_002, FaultClass::ProcessWarning, 2U, "P1-002"},
    {FaultCode::P1_003, FaultClass::ProcessWarning, 3U, "P1-003"},
    {FaultCode::O2_001, FaultClass::OperatingFault, 1U, "O2-001"},
    {FaultCode::O2_002, FaultClass::OperatingFault, 2U, "O2-002"},
    {FaultCode::O2_003, FaultClass::OperatingFault, 3U, "O2-003"},
    {FaultCode::O2_004, FaultClass::OperatingFault, 4U, "O2-004"},
    {FaultCode::S3_001, FaultClass::LatchedSafetyFault, 1U, "S3-001"},
    {FaultCode::S3_002, FaultClass::LatchedSafetyFault, 2U, "S3-002"},
    {FaultCode::S3_003, FaultClass::LatchedSafetyFault, 3U, "S3-003"},
    {FaultCode::S3_004, FaultClass::LatchedSafetyFault, 4U, "S3-004"},
    {FaultCode::S3_005, FaultClass::LatchedSafetyFault, 5U, "S3-005"},
    {FaultCode::S3_006, FaultClass::LatchedSafetyFault, 6U, "S3-006"},
    {FaultCode::S3_007, FaultClass::LatchedSafetyFault, 7U, "S3-007"},
    {FaultCode::S3_008, FaultClass::LatchedSafetyFault, 8U, "S3-008"},
    {FaultCode::S3_009, FaultClass::LatchedSafetyFault, 9U, "S3-009"},
    {FaultCode::Y4_001, FaultClass::LatchedSystemFault, 1U, "Y4-001"},
    {FaultCode::Y4_002, FaultClass::LatchedSystemFault, 2U, "Y4-002"},
    {FaultCode::Y4_003, FaultClass::LatchedSystemFault, 3U, "Y4-003"},
    {FaultCode::Y4_004, FaultClass::LatchedSystemFault, 4U, "Y4-004"},
    {FaultCode::Y4_005, FaultClass::LatchedSystemFault, 5U, "Y4-005"},
    {FaultCode::Y4_006, FaultClass::LatchedSystemFault, 6U, "Y4-006"},
    {FaultCode::Y4_007, FaultClass::LatchedSystemFault, 7U, "Y4-007"},
    {FaultCode::Y4_008, FaultClass::LatchedSystemFault, 8U, "Y4-008"},
    {FaultCode::Y4_009, FaultClass::LatchedSystemFault, 9U, "Y4-009"},
    {FaultCode::Y4_011, FaultClass::LatchedSystemFault, 10U, "Y4-011"},
};

const FaultCodeInfo* info(FaultCode code) {
    for (const auto& entry : kCodeInfo) {
        if (entry.code == code) return &entry;
    }
    return nullptr;
}

bool statusActive(const FaultRecord& record) {
    return record.status != FaultStatus::Cleared;
}

}  // namespace

bool isKnownFaultClass(FaultClass value) {
    switch (value) {
        case FaultClass::ProcessWarning:
        case FaultClass::OperatingFault:
        case FaultClass::LatchedSafetyFault:
        case FaultClass::LatchedSystemFault:
            return true;
        case FaultClass::Unknown:
            return false;
    }
    return false;
}

bool isKnownFaultCode(FaultCode value) { return info(value) != nullptr; }

FaultCode normalizeFaultCode(FaultCode value) {
    return isKnownFaultCode(value) ? value : FaultCode::Y4_011;
}

FaultClass faultClassForCode(FaultCode value) {
    const auto* entry = info(normalizeFaultCode(value));
    return entry == nullptr ? FaultClass::LatchedSystemFault
                            : entry->faultClass;
}

std::uint8_t faultCodePriority(FaultCode value) {
    const auto* entry = info(normalizeFaultCode(value));
    return entry == nullptr ? 0U : entry->priority;
}

const char* faultCodeText(FaultCode value) {
    const auto* entry = info(normalizeFaultCode(value));
    return entry == nullptr ? "Y4-011" : entry->text;
}

bool isLatchedFaultClass(FaultClass value) {
    return value == FaultClass::LatchedSafetyFault ||
           value == FaultClass::LatchedSystemFault;
}

bool isBlockingFault(const FaultRecord& record) {
    return statusActive(record) &&
           (isLatchedFaultClass(record.faultClass) ||
            (record.faultClass == FaultClass::OperatingFault &&
             record.causeActive));
}

bool equalFaultCoreSnapshot(const FaultCoreSnapshot& left,
                            const FaultCoreSnapshot& right) {
    if (left.count != right.count || left.revision != right.revision ||
        left.instanceSequenceHighWatermark !=
            right.instanceSequenceHighWatermark ||
        left.criticalSafetyEventPending != right.criticalSafetyEventPending) {
        return false;
    }
    for (std::size_t index = 0U; index < left.count; ++index) {
        const auto& a = left.records[index];
        const auto& b = right.records[index];
        if (a.instanceId != b.instanceId || a.code != b.code ||
            a.faultClass != b.faultClass || a.sourceKey != b.sourceKey ||
            a.correlationKey != b.correlationKey ||
            a.creationSequence != b.creationSequence ||
            a.createdAtMonotonicMillis != b.createdAtMonotonicMillis ||
            a.status != b.status || a.disposition != b.disposition ||
            a.causeActive != b.causeActive || a.latched != b.latched ||
            a.controlledRestartUsed != b.controlledRestartUsed ||
            a.faultRevision != b.faultRevision ||
            a.primaryFaultId != b.primaryFaultId ||
            a.diagnosticSequenceHighWatermark !=
                b.diagnosticSequenceHighWatermark) {
            return false;
        }
    }
    return true;
}

bool FaultCore::incrementRevision() {
    if (state_.revision == std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    ++state_.revision;
    return true;
}

FaultRecord* FaultCore::findMutable(FaultInstanceId id) {
    if (!id.valid()) return nullptr;
    for (std::size_t index = 0U; index < state_.count; ++index) {
        if (state_.records[index].instanceId == id) {
            return &state_.records[index];
        }
    }
    return nullptr;
}

const FaultRecord* FaultCore::find(FaultInstanceId id) const {
    if (!id.valid()) return nullptr;
    for (std::size_t index = 0U; index < state_.count; ++index) {
        if (state_.records[index].instanceId == id) {
            return &state_.records[index];
        }
    }
    return nullptr;
}

FaultRecord* FaultCore::findCorrelation(const FaultRaiseRequest& request) {
    const auto code = normalizeFaultCode(request.code);
    for (std::size_t index = 0U; index < state_.count; ++index) {
        auto& record = state_.records[index];
        if (statusActive(record) && record.code == code &&
            record.sourceKey == request.sourceKey &&
            record.correlationKey == request.correlationKey &&
            record.diagnosticSequenceHighWatermark ==
                request.diagnosticSequenceHighWatermark) {
            return &record;
        }
    }
    return nullptr;
}

FaultRaiseResult FaultCore::raise(const FaultRaiseRequest& request) {
    if (request.primaryFaultId.has_value() &&
        find(*request.primaryFaultId) == nullptr) {
        return {FaultRaiseStatus::InvalidInput, {}};
    }
    if (auto* existing = findCorrelation(request); existing != nullptr) {
        if (!existing->causeActive ||
            existing->status == FaultStatus::CauseClearedLocked) {
            if (!incrementRevision()) {
                return {FaultRaiseStatus::RevisionOverflow, {}};
            }
            existing->causeActive = true;
            existing->status = FaultStatus::ActiveUnacknowledged;
            existing->faultRevision = state_.revision;
            recomputeProjection();
            return {FaultRaiseStatus::Reactivated, existing->instanceId};
        }
        return {FaultRaiseStatus::Existing, existing->instanceId};
    }
    if (state_.count >= state_.records.size() || nextInstanceId_ == 0U) {
        installUnknownPersistenceFault();
        return {FaultRaiseStatus::CapacityReached, {}};
    }
    if (state_.revision == std::numeric_limits<std::uint32_t>::max() ||
        nextInstanceId_ == std::numeric_limits<std::uint32_t>::max()) {
        installUnknownPersistenceFault();
        return {FaultRaiseStatus::RevisionOverflow, {}};
    }

    const auto code = normalizeFaultCode(request.code);
    const FaultInstanceId id{nextInstanceId_++};
    FaultRecord record;
    record.instanceId = id;
    record.code = code;
    record.faultClass = faultClassForCode(code);
    record.sourceKey = request.sourceKey;
    record.correlationKey = request.correlationKey;
    record.creationSequence = id.value;
    record.createdAtMonotonicMillis = request.monotonicMillis;
    record.status = FaultStatus::ActiveUnacknowledged;
    record.disposition = record.faultClass == FaultClass::ProcessWarning
                             ? SafetyDisposition::Allowed
                             : SafetyDisposition::ImmediateStop;
    record.causeActive = true;
    record.latched = isLatchedFaultClass(record.faultClass);
    record.faultRevision = state_.revision + 1U;
    record.primaryFaultId = request.primaryFaultId;
    record.diagnosticSequenceHighWatermark =
        request.diagnosticSequenceHighWatermark;
    state_.records[state_.count++] = record;
    state_.instanceSequenceHighWatermark = id.value;
    ++state_.revision;
    recomputeProjection();
    return {FaultRaiseStatus::Created, id};
}

bool FaultCore::acknowledge(FaultInstanceId id,
                            std::uint32_t expectedRevision) {
    auto* record = findMutable(id);
    if (record == nullptr || record->status == FaultStatus::Cleared ||
        record->faultRevision != expectedRevision || !incrementRevision()) {
        return false;
    }
    record->status = record->causeActive
                         ? FaultStatus::ActiveAcknowledged
                         : (record->latched ? FaultStatus::CauseClearedLocked
                                            : FaultStatus::Cleared);
    record->faultRevision = state_.revision;
    recomputeProjection();
    return true;
}

bool FaultCore::markCauseCleared(FaultInstanceId id,
                                 std::uint32_t expectedRevision) {
    auto* record = findMutable(id);
    if (record == nullptr || record->status == FaultStatus::Cleared ||
        record->faultRevision != expectedRevision || !incrementRevision()) {
        return false;
    }
    record->causeActive = false;
    // P1/O2 are explicitly non-latched in the R2 code policy.  Once their
    // cause is gone they are cleared immediately; leaving them in
    // CauseClearedLocked would create an unresolvable nonpersistent latch.
    record->status = record->latched ? FaultStatus::CauseClearedLocked
                                     : FaultStatus::Cleared;
    record->faultRevision = state_.revision;
    recomputeProjection();
    return true;
}

bool FaultCore::markControlledRestartUsed(FaultInstanceId id,
                                          std::uint32_t expectedRevision) {
    auto* record = findMutable(id);
    if (record == nullptr || record->status == FaultStatus::Cleared ||
        record->faultRevision != expectedRevision ||
        record->controlledRestartUsed || !incrementRevision()) {
        return false;
    }
    record->controlledRestartUsed = true;
    record->faultRevision = state_.revision;
    recomputeProjection();
    return true;
}

bool FaultCore::clearAfterVerifiedReset(FaultInstanceId id,
                                        std::uint32_t expectedRevision) {
    auto* record = findMutable(id);
    if (record == nullptr || record->status == FaultStatus::Cleared ||
        record->causeActive || !record->latched ||
        record->faultRevision != expectedRevision || !incrementRevision()) {
        return false;
    }
    record->status = FaultStatus::Cleared;
    record->faultRevision = state_.revision;
    recomputeProjection();
    return true;
}

bool FaultCore::restoreSnapshot(const FaultCoreSnapshot& snapshot) {
    if (snapshot.count > snapshot.records.size()) return false;
    if (snapshot.instanceSequenceHighWatermark ==
        std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    std::uint32_t maximumId = 0U;
    for (std::size_t index = 0U; index < snapshot.count; ++index) {
        const auto& record = snapshot.records[index];
        if (!record.instanceId.valid() || !isKnownFaultCode(record.code) ||
            !isKnownFaultClass(record.faultClass) ||
            record.faultClass != faultClassForCode(record.code) ||
            record.faultRevision == 0U ||
            record.disposition !=
                (record.faultClass == FaultClass::ProcessWarning
                     ? SafetyDisposition::Allowed
                     : SafetyDisposition::ImmediateStop) ||
            (record.primaryFaultId.has_value() &&
             record.primaryFaultId->value == record.instanceId.value)) {
            return false;
        }
        for (std::size_t otherIndex = 0U; otherIndex < index; ++otherIndex) {
            if (snapshot.records[otherIndex].instanceId == record.instanceId) {
                return false;
            }
        }
        if (record.primaryFaultId.has_value()) {
            bool primaryFound = false;
            for (std::size_t primaryIndex = 0U; primaryIndex < snapshot.count;
                 ++primaryIndex) {
                if (snapshot.records[primaryIndex].instanceId ==
                    *record.primaryFaultId) {
                    primaryFound = true;
                    break;
                }
            }
            if (!primaryFound) return false;
        }
        if (record.instanceId.value > snapshot.instanceSequenceHighWatermark) {
            return false;
        }
        if (record.instanceId.value > maximumId) {
            maximumId = record.instanceId.value;
        }
    }
    if (maximumId > snapshot.instanceSequenceHighWatermark) return false;
    state_ = snapshot;
    state_.instanceSequenceHighWatermark =
        snapshot.instanceSequenceHighWatermark;
    nextInstanceId_ = snapshot.instanceSequenceHighWatermark + 1U;
    recomputeProjection();
    return true;
}

const FaultRecord* FaultCore::dominant() const {
    const FaultRecord* result = nullptr;
    for (std::size_t index = 0U; index < state_.count; ++index) {
        const auto& candidate = state_.records[index];
        if (!statusActive(candidate)) continue;
        if (result == nullptr ||
            static_cast<std::uint8_t>(candidate.faultClass) >
                static_cast<std::uint8_t>(result->faultClass) ||
            (candidate.faultClass == result->faultClass &&
             (faultCodePriority(candidate.code) <
                  faultCodePriority(result->code) ||
              (faultCodePriority(candidate.code) ==
                   faultCodePriority(result->code) &&
               candidate.creationSequence < result->creationSequence)))) {
            result = &candidate;
        }
    }
    return result;
}

SafetyDisposition FaultCore::disposition() const {
    const auto* record = dominant();
    if (record == nullptr) return SafetyDisposition::Allowed;
    return record->disposition;
}

bool FaultCore::hasBlockingFault() const {
    for (std::size_t index = 0U; index < state_.count; ++index) {
        if (isBlockingFault(state_.records[index])) return true;
    }
    return false;
}

FaultCoreSnapshot FaultCore::snapshot() const { return state_; }

void FaultCore::recomputeProjection() {
    state_.criticalSafetyEventPending = hasBlockingFault();
}

void FaultCore::installUnknownPersistenceFault() {
    if (state_.count >= state_.records.size() ||
        state_.revision == std::numeric_limits<std::uint32_t>::max() ||
        nextInstanceId_ == 0U ||
        nextInstanceId_ == std::numeric_limits<std::uint32_t>::max()) {
        state_.criticalSafetyEventPending = true;
        return;
    }
    FaultRecord record;
    record.instanceId = {nextInstanceId_ == 0U ? 1U : nextInstanceId_++};
    record.code = FaultCode::Y4_005;
    record.faultClass = FaultClass::LatchedSystemFault;
    record.creationSequence = record.instanceId.value;
    record.latched = true;
    record.faultRevision = ++state_.revision;
    state_.records[state_.count++] = record;
    state_.instanceSequenceHighWatermark = record.instanceId.value;
    state_.criticalSafetyEventPending = true;
}

const char* faultEventTypeText(FaultEventType type) {
    switch (type) {
        case FaultEventType::FaultCreated:
            return "FaultCreated";
        case FaultEventType::FaultEscalated:
            return "FaultEscalated";
        case FaultEventType::FaultCauseCleared:
            return "FaultCauseCleared";
        case FaultEventType::FaultAcknowledged:
            return "FaultAcknowledged";
        case FaultEventType::FaultResetCommitted:
            return "FaultResetCommitted";
        case FaultEventType::FaultResetRejected:
            return "FaultResetRejected";
        case FaultEventType::RestartEpisodeAdvanced:
            return "RestartEpisodeAdvanced";
        case FaultEventType::RestartEpisodeClosed:
            return "RestartEpisodeClosed";
        case FaultEventType::SafeBootEntered:
            return "SafeBootEntered";
        case FaultEventType::SafeBootExitDecided:
            return "SafeBootExitDecided";
        case FaultEventType::SafeBootExitRejected:
            return "SafeBootExitRejected";
        case FaultEventType::SafetyRecoveryAttempted:
            return "SafetyRecoveryAttempted";
        case FaultEventType::SafetyRecoveryAborted:
            return "SafetyRecoveryAborted";
        case FaultEventType::SafetyRecoverySucceeded:
            return "SafetyRecoverySucceeded";
    }
    return "SafeBootExitRejected";
}

std::string serializeFaultEvent(const FaultEventProjection& projection) {
    std::string result = "type=";
    result += faultEventTypeText(projection.type);
    result += ";code=";
    result += faultCodeText(projection.code);
    result += ";fault=" + std::to_string(projection.faultInstanceId.value);
    result += ";primary=";
    result += projection.primaryFaultId.has_value()
                  ? std::to_string(projection.primaryFaultId->value)
                  : "0";
    result += ";faultRevision=" + std::to_string(projection.faultRevision);
    result += ";episode=" + std::to_string(projection.episodeId);
    result += ";evidence=" + std::to_string(projection.restartEvidenceId);
    result += ";diagnosticSequence=" +
              std::to_string(projection.diagnosticSequenceHighWatermark);
    result += ";accepted=" + std::to_string(projection.accepted ? 1 : 0);
    return result;
}

bool recordFaultEvent(device_platform::IEventJournal* journal,
                      std::uint64_t monotonicMillis,
                      const FaultEventProjection& projection) {
    if (journal == nullptr) return false;
    return journal->record(monotonicMillis, serializeFaultEvent(projection));
}

}  // namespace fermentation
