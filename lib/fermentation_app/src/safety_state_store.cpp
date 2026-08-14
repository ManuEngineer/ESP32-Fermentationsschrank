#include "safety_state_store.hpp"

#include <cstring>
#include <limits>

#include "storage_envelope.hpp"

namespace fermentation {
namespace {

constexpr std::uint16_t kSafetyRecordType = 0x24U;
constexpr std::size_t kPayloadBytes = 80U + kMaximumPersistedLatches * 48U;

void appendU8(std::string& bytes, std::uint8_t value) {
    bytes.push_back(static_cast<char>(value));
}

void appendU16(std::string& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<char>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<char>(value & 0xFFU));
}

void appendU32(std::string& bytes, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<char>((value >> shift) & 0xFFU));
    }
}

void appendU64(std::string& bytes, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<char>((value >> shift) & 0xFFU));
    }
}

bool readU8(const std::string& bytes, std::size_t& offset, std::uint8_t& out) {
    if (offset >= bytes.size()) return false;
    out = static_cast<std::uint8_t>(bytes[offset++]);
    return true;
}

bool readU16(const std::string& bytes, std::size_t& offset,
             std::uint16_t& out) {
    if (bytes.size() - offset < 2U) return false;
    out = (static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes[offset]))
           << 8U) |
          static_cast<std::uint16_t>(
              static_cast<std::uint8_t>(bytes[offset + 1U]));
    offset += 2U;
    return true;
}

bool readU32(const std::string& bytes, std::size_t& offset,
             std::uint32_t& out) {
    if (bytes.size() - offset < 4U) return false;
    out = 0U;
    for (std::size_t i = 0U; i < 4U; ++i) {
        out = (out << 8U) | static_cast<std::uint32_t>(
                                static_cast<std::uint8_t>(bytes[offset + i]));
    }
    offset += 4U;
    return true;
}

bool readU64(const std::string& bytes, std::size_t& offset,
             std::uint64_t& out) {
    if (bytes.size() - offset < 8U) return false;
    out = 0U;
    for (std::size_t i = 0U; i < 8U; ++i) {
        out = (out << 8U) | static_cast<std::uint64_t>(
                                static_cast<std::uint8_t>(bytes[offset + i]));
    }
    offset += 8U;
    return true;
}

void encodeFault(std::string& bytes, const FaultRecord& fault) {
    const std::size_t start = bytes.size();
    appendU32(bytes, fault.instanceId.value);
    appendU16(bytes, static_cast<std::uint16_t>(fault.code));
    appendU8(bytes, static_cast<std::uint8_t>(fault.faultClass));
    appendU8(bytes, static_cast<std::uint8_t>(fault.status));
    appendU8(bytes, static_cast<std::uint8_t>(fault.disposition));
    appendU8(bytes,
             static_cast<std::uint8_t>(
                 (fault.causeActive ? 1U : 0U) | (fault.latched ? 2U : 0U) |
                 (fault.controlledRestartUsed ? 4U : 0U)));
    appendU32(bytes, fault.sourceKey);
    appendU32(bytes, fault.correlationKey);
    appendU32(bytes, fault.creationSequence);
    appendU64(bytes, fault.createdAtMonotonicMillis);
    appendU32(bytes, fault.faultRevision);
    appendU32(bytes, fault.primaryFaultId.has_value()
                         ? fault.primaryFaultId->value
                         : 0U);
    appendU64(bytes, fault.diagnosticSequenceHighWatermark);
    // Fixed-size record: retain room for future bounded metadata without
    // introducing strings or vectors into the persistent authority.
    while (bytes.size() - start < 48U) appendU8(bytes, 0U);
}

bool decodeFault(const std::string& bytes, std::size_t& offset,
                 FaultRecord& fault) {
    std::uint32_t instance = 0U;
    std::uint16_t code = 0U;
    std::uint8_t faultClass = 0U;
    std::uint8_t status = 0U;
    std::uint8_t disposition = 0U;
    std::uint8_t flags = 0U;
    std::uint32_t primary = 0U;
    if (!readU32(bytes, offset, instance) || !readU16(bytes, offset, code) ||
        !readU8(bytes, offset, faultClass) || !readU8(bytes, offset, status) ||
        !readU8(bytes, offset, disposition) || !readU8(bytes, offset, flags) ||
        !readU32(bytes, offset, fault.sourceKey) ||
        !readU32(bytes, offset, fault.correlationKey) ||
        !readU32(bytes, offset, fault.creationSequence) ||
        !readU64(bytes, offset, fault.createdAtMonotonicMillis) ||
        !readU32(bytes, offset, fault.faultRevision) ||
        !readU32(bytes, offset, primary) ||
        !readU64(bytes, offset, fault.diagnosticSequenceHighWatermark)) {
        return false;
    }
    if (bytes.size() - offset < 2U) return false;
    offset += 2U;
    fault.instanceId = {instance};
    fault.code = static_cast<FaultCode>(code);
    fault.faultClass = static_cast<FaultClass>(faultClass);
    fault.status = static_cast<FaultStatus>(status);
    fault.disposition = static_cast<SafetyDisposition>(disposition);
    fault.causeActive = (flags & 1U) != 0U;
    fault.latched = (flags & 2U) != 0U;
    fault.controlledRestartUsed = (flags & 4U) != 0U;
    fault.primaryFaultId = primary == 0U
                               ? std::nullopt
                               : std::optional<FaultInstanceId>{{primary}};
    return true;
}

std::string encodePayload(const SafetyStateRecord& record) {
    std::string bytes;
    bytes.reserve(kPayloadBytes);
    appendU32(bytes, record.faultRevision);
    appendU32(bytes, record.faultInstanceSequence);
    appendU8(bytes, record.safeBootRequired ? 1U : 0U);
    appendU16(bytes, static_cast<std::uint16_t>(record.dominantCode));
    appendU32(bytes, record.storageEpoch.value());
    appendU32(bytes, static_cast<std::uint32_t>(record.latchCount));
    appendU32(bytes, record.restartEpisode.episodeId);
    appendU32(bytes, record.restartEpisode.abnormalRestartCount);
    appendU32(bytes, record.restartEpisode.lastRestartEvidenceId);
    appendU32(bytes, record.restartEpisode.nextRestartEvidenceId);
    appendU8(bytes, record.restartEpisode.open ? 1U : 0U);
    appendU8(bytes, record.restartEpisode.stableWindowRunning ? 1U : 0U);
    appendU64(bytes, record.restartEpisode.stableWindowStartedAtMillis);
    appendU32(bytes, record.restartEvidence.evidenceId);
    appendU8(bytes, static_cast<std::uint8_t>(record.restartEvidence.cause));
    appendU8(bytes, static_cast<std::uint8_t>(record.restartEvidence.state));
    appendU32(bytes, record.restartEvidence.faultInstanceId.value);
    appendU32(bytes, record.faultResetBootIntent.targetFault.value);
    appendU32(bytes, record.faultResetBootIntent.expectedFaultRevision);
    appendU32(bytes, record.faultResetBootIntent.intentRevision);
    appendU8(bytes, record.faultResetBootIntent.pending ? 1U : 0U);
    appendU8(bytes, static_cast<std::uint8_t>(record.lastResetCause));
    appendU64(bytes, record.lastResetObservationId);
    while (bytes.size() < 80U) appendU8(bytes, 0U);
    for (std::size_t index = 0U; index < kMaximumPersistedLatches; ++index) {
        encodeFault(bytes, index < record.latchCount ? record.latches[index]
                                                     : FaultRecord{});
    }
    return bytes;
}

bool decodePayload(const std::string& bytes, SafetyStateRecord& record) {
    if (bytes.size() != kPayloadBytes) {
        return false;
    }
    std::size_t offset = 0U;
    std::uint8_t safeBoot = 0U;
    std::uint16_t dominant = 0U;
    std::uint32_t epoch = 0U;
    std::uint32_t latchCount = 0U;
    std::uint8_t open = 0U;
    std::uint8_t stable = 0U;
    std::uint8_t evidenceCause = 0U;
    std::uint8_t evidenceState = 0U;
    std::uint32_t evidenceFault = 0U;
    std::uint32_t intentFault = 0U;
    std::uint8_t intentPending = 0U;
    std::uint8_t resetCause = 0U;
    if (!readU32(bytes, offset, record.faultRevision) ||
        !readU32(bytes, offset, record.faultInstanceSequence) ||
        !readU8(bytes, offset, safeBoot) || !readU16(bytes, offset, dominant) ||
        !readU32(bytes, offset, epoch) || !readU32(bytes, offset, latchCount) ||
        !readU32(bytes, offset, record.restartEpisode.episodeId) ||
        !readU32(bytes, offset, record.restartEpisode.abnormalRestartCount) ||
        !readU32(bytes, offset, record.restartEpisode.lastRestartEvidenceId) ||
        !readU32(bytes, offset, record.restartEpisode.nextRestartEvidenceId) ||
        !readU8(bytes, offset, open) || !readU8(bytes, offset, stable) ||
        !readU64(bytes, offset,
                 record.restartEpisode.stableWindowStartedAtMillis) ||
        !readU32(bytes, offset, record.restartEvidence.evidenceId) ||
        !readU8(bytes, offset, evidenceCause) ||
        !readU8(bytes, offset, evidenceState) ||
        !readU32(bytes, offset, evidenceFault) ||
        !readU32(bytes, offset, intentFault) ||
        !readU32(bytes, offset,
                 record.faultResetBootIntent.expectedFaultRevision) ||
        !readU32(bytes, offset, record.faultResetBootIntent.intentRevision) ||
        !readU8(bytes, offset, intentPending) ||
        !readU8(bytes, offset, resetCause) ||
        !readU64(bytes, offset, record.lastResetObservationId)) {
        return false;
    }
    if (bytes.size() < 80U) return false;
    offset = 80U;
    record.safeBootRequired = safeBoot != 0U;
    record.dominantCode = static_cast<FaultCode>(dominant);
    record.storageEpoch = device_platform::StorageEpoch{epoch};
    record.latchCount = latchCount;
    record.restartEpisode.open = open != 0U;
    record.restartEpisode.stableWindowRunning = stable != 0U;
    record.restartEvidence.cause =
        static_cast<RestartCauseEvent>(evidenceCause);
    record.restartEvidence.state =
        static_cast<RestartEvidenceState>(evidenceState);
    record.restartEvidence.faultInstanceId = {evidenceFault};
    record.faultResetBootIntent.targetFault = {intentFault};
    record.faultResetBootIntent.pending = intentPending != 0U;
    record.lastResetCause =
        static_cast<device_platform::ResetCause>(resetCause);
    for (std::size_t index = 0U; index < kMaximumPersistedLatches; ++index) {
        if (!decodeFault(bytes, offset, record.latches[index])) return false;
    }
    return validateSafetyStateRecord(record) == SafetyRecordValidation::Valid;
}

}  // namespace

bool knownFaultStatus(FaultStatus status) {
    switch (status) {
        case FaultStatus::ActiveUnacknowledged:
        case FaultStatus::ActiveAcknowledged:
        case FaultStatus::CauseClearedLocked:
        case FaultStatus::Cleared:
            return true;
    }
    return false;
}

bool knownDisposition(SafetyDisposition disposition) {
    switch (disposition) {
        case SafetyDisposition::Allowed:
        case SafetyDisposition::ImmediateStop:
        case SafetyDisposition::SafetyRecovery:
            return true;
    }
    return false;
}

bool knownRestartCause(RestartCauseEvent cause) {
    switch (cause) {
        case RestartCauseEvent::ControlledSafety:
        case RestartCauseEvent::WatchdogOrPanic:
        case RestartCauseEvent::Brownout:
        case RestartCauseEvent::PowerOn:
        case RestartCauseEvent::Authorized:
        case RestartCauseEvent::Unknown:
            return true;
    }
    return false;
}

bool knownRestartEvidenceState(RestartEvidenceState state) {
    switch (state) {
        case RestartEvidenceState::None:
        case RestartEvidenceState::Pending:
        case RestartEvidenceState::Committed:
        case RestartEvidenceState::Consumed:
            return true;
    }
    return false;
}

bool knownResetCause(device_platform::ResetCause cause) {
    switch (cause) {
        case device_platform::ResetCause::PowerOn:
        case device_platform::ResetCause::AuthorizedRestart:
        case device_platform::ResetCause::ControlledSafetyRestart:
        case device_platform::ResetCause::WatchdogOrPanic:
        case device_platform::ResetCause::Brownout:
        case device_platform::ResetCause::Unknown:
            return true;
    }
    return false;
}

SafetyRecordValidation validateSafetyStateRecord(
    const SafetyStateRecord& record) {
    if (record.schemaVersion != kSafetyStateRecordSchema ||
        record.recordRevision == 0U || record.storageEpoch.value() == 0U ||
        record.latchCount > kMaximumPersistedLatches ||
        record.restartEpisode.nextRestartEvidenceId == 0U) {
        return SafetyRecordValidation::InvalidField;
    }
    if (!knownRestartCause(record.restartEvidence.cause) ||
        !knownRestartEvidenceState(record.restartEvidence.state) ||
        !knownResetCause(record.lastResetCause) ||
        (record.restartEpisode.open && record.restartEpisode.episodeId == 0U) ||
        (record.restartEpisode.stableWindowRunning &&
         !record.restartEpisode.open)) {
        return SafetyRecordValidation::InvalidField;
    }
    if (record.restartEvidence.state == RestartEvidenceState::None &&
        (record.restartEvidence.evidenceId != 0U ||
         record.restartEvidence.faultInstanceId.valid())) {
        return SafetyRecordValidation::InvalidRelationship;
    }
    if (record.restartEvidence.state != RestartEvidenceState::None &&
        record.restartEvidence.evidenceId == 0U) {
        return SafetyRecordValidation::InvalidRelationship;
    }
    if (!isKnownFaultCode(record.dominantCode) &&
        record.dominantCode != FaultCode::Unknown) {
        return SafetyRecordValidation::InvalidField;
    }
    for (std::size_t index = 0U; index < record.latchCount; ++index) {
        const auto& fault = record.latches[index];
        if (!fault.instanceId.valid() || !isKnownFaultCode(fault.code) ||
            !isKnownFaultClass(fault.faultClass) ||
            !isLatchedFaultClass(fault.faultClass) ||
            fault.faultClass != faultClassForCode(fault.code) ||
            !knownFaultStatus(fault.status) ||
            !knownDisposition(fault.disposition) || fault.faultRevision == 0U ||
            fault.disposition != SafetyDisposition::ImmediateStop ||
            (fault.primaryFaultId.has_value() &&
             fault.primaryFaultId->value == fault.instanceId.value)) {
            return SafetyRecordValidation::InvalidRelationship;
        }
        for (std::size_t otherIndex = 0U; otherIndex < index; ++otherIndex) {
            if (record.latches[otherIndex].instanceId == fault.instanceId) {
                return SafetyRecordValidation::InvalidRelationship;
            }
        }
        if (fault.primaryFaultId.has_value()) {
            bool primaryFound = false;
            for (std::size_t primaryIndex = 0U;
                 primaryIndex < record.latchCount; ++primaryIndex) {
                if (record.latches[primaryIndex].instanceId ==
                    *fault.primaryFaultId) {
                    primaryFound = true;
                    break;
                }
            }
            if (!primaryFound)
                return SafetyRecordValidation::InvalidRelationship;
        }
        if (fault.instanceId.value > record.faultInstanceSequence) {
            return SafetyRecordValidation::InvalidRelationship;
        }
    }
    if (record.dominantCode != FaultCode::Unknown) {
        bool dominantFound = false;
        for (std::size_t index = 0U; index < record.latchCount; ++index) {
            if (record.latches[index].code == record.dominantCode &&
                record.latches[index].status != FaultStatus::Cleared) {
                dominantFound = true;
                break;
            }
        }
        if (!dominantFound) return SafetyRecordValidation::InvalidRelationship;
    }
    if (record.restartEvidence.state != RestartEvidenceState::None) {
        if (record.restartEvidence.cause ==
                RestartCauseEvent::ControlledSafety &&
            !record.restartEvidence.faultInstanceId.valid()) {
            return SafetyRecordValidation::InvalidRelationship;
        }
        if (record.restartEvidence.faultInstanceId.valid()) {
            bool evidenceFaultFound = false;
            for (std::size_t index = 0U; index < record.latchCount; ++index) {
                if (record.latches[index].instanceId ==
                    record.restartEvidence.faultInstanceId) {
                    evidenceFaultFound = true;
                    break;
                }
            }
            if (!evidenceFaultFound) {
                return SafetyRecordValidation::InvalidRelationship;
            }
        }
    }
    if (record.faultResetBootIntent.pending &&
        (!record.faultResetBootIntent.targetFault.valid() ||
         record.faultResetBootIntent.intentRevision == 0U ||
         record.faultResetBootIntent.expectedFaultRevision == 0U)) {
        return SafetyRecordValidation::InvalidRelationship;
    }
    if (record.faultResetBootIntent.pending) {
        bool targetFound = false;
        for (std::size_t index = 0U; index < record.latchCount; ++index) {
            if (record.latches[index].instanceId ==
                record.faultResetBootIntent.targetFault) {
                targetFound = true;
                break;
            }
        }
        if (!targetFound) return SafetyRecordValidation::InvalidRelationship;
    }
    if (record.lastResetCause != device_platform::ResetCause::Unknown &&
        record.lastResetObservationId == 0U) {
        return SafetyRecordValidation::InvalidRelationship;
    }
    return SafetyRecordValidation::Valid;
}

SafetyRecordEncodeStatus encodeSafetyStateRecord(
    const SafetyStateRecord& record, std::string& outBytes,
    std::size_t maxBytes) {
    if (validateSafetyStateRecord(record) != SafetyRecordValidation::Valid) {
        return SafetyRecordEncodeStatus::InvalidRecord;
    }
    const auto payload = encodePayload(record);
    device_platform::StorageEnvelope envelope{
        device_platform::RecordTypeId{kSafetyRecordType},
        kSafetyStateRecordSchema,
        record.storageEpoch,
        record.recordRevision,
        std::nullopt,
        payload};
    const auto status =
        device_platform::encodeEnvelope(envelope, outBytes, maxBytes);
    if (status == device_platform::EnvelopeEncodeStatus::CapacityExceeded) {
        return SafetyRecordEncodeStatus::CapacityExceeded;
    }
    return status == device_platform::EnvelopeEncodeStatus::Success
               ? SafetyRecordEncodeStatus::Success
               : SafetyRecordEncodeStatus::InvalidRecord;
}

SafetyRecordDecodeStatus decodeSafetyStateRecord(const std::string& bytes,
                                                 SafetyStateRecord& outRecord) {
    const auto decoded = device_platform::decodeEnvelope(bytes);
    if (decoded.status != device_platform::EnvelopeDecodeStatus::Success ||
        !decoded.envelope.has_value() ||
        decoded.envelope->recordTypeId.value() != kSafetyRecordType ||
        decoded.envelope->schemaVersion != kSafetyStateRecordSchema ||
        decoded.envelope->versionValue == 0U) {
        return SafetyRecordDecodeStatus::InvalidEnvelope;
    }
    outRecord = SafetyStateRecord{};
    outRecord.schemaVersion = decoded.envelope->schemaVersion;
    outRecord.recordRevision =
        static_cast<std::uint32_t>(decoded.envelope->versionValue);
    outRecord.storageEpoch = decoded.envelope->storageEpoch;
    if (decoded.envelope->versionValue >
            std::numeric_limits<std::uint32_t>::max() ||
        !decodePayload(decoded.envelope->payload, outRecord)) {
        return SafetyRecordDecodeStatus::InvalidRecord;
    }
    return SafetyRecordDecodeStatus::Success;
}

SafetyStateStore::SafetyStateStore(device_platform::IStateStore& store)
    : store_(store),
      key_(*device_platform::StateStoreKey::create("safety24").key) {}

SafetyRecordLoadResult SafetyStateStore::load(
    const FactoryNewSafetyProof& factoryProof) {
    const auto read = store_.read(key_, kMaximumSafetyRecordBytes);
    if (read.status == device_platform::StateStoreReadStatus::NotFound) {
        if (!factoryProof.valid()) {
            return {SafetyRecordLoadStatus::NotFoundOutsideFactoryBootstrap,
                    {}};
        }
        SafetyStateRecord initial;
        const auto commitResult = commit(initial);
        if (commitResult.status != SafetyRecordCommitStatus::Committed) {
            return {SafetyRecordLoadStatus::ReadError, {}};
        }
        return {SafetyRecordLoadStatus::FactoryInitialized, initial};
    }
    if (read.status == device_platform::StateStoreReadStatus::ReadError) {
        return {SafetyRecordLoadStatus::ReadError, {}};
    }
    if (read.status == device_platform::StateStoreReadStatus::CapacityError) {
        return {SafetyRecordLoadStatus::CapacityError, {}};
    }
    SafetyStateRecord record;
    if (decodeSafetyStateRecord(read.value, record) !=
        SafetyRecordDecodeStatus::Success) {
        return {SafetyRecordLoadStatus::Corrupt, {}};
    }
    return {SafetyRecordLoadStatus::Loaded, record};
}

SafetyRecordCommitResult SafetyStateStore::commit(
    const SafetyStateRecord& record) {
    std::string encoded;
    const auto encodeStatus =
        encodeSafetyStateRecord(record, encoded, kMaximumSafetyRecordBytes);
    if (encodeStatus == SafetyRecordEncodeStatus::InvalidRecord) {
        return {SafetyRecordCommitStatus::InvalidRecord};
    }
    if (encodeStatus == SafetyRecordEncodeStatus::CapacityExceeded) {
        return {SafetyRecordCommitStatus::CapacityError};
    }
    const auto writeStatus = store_.write(key_, encoded);
    if (writeStatus == device_platform::StateStoreWriteStatus::WriteError) {
        return {SafetyRecordCommitStatus::WriteError};
    }
    if (writeStatus == device_platform::StateStoreWriteStatus::CapacityError) {
        return {SafetyRecordCommitStatus::CapacityError};
    }
    const auto readback = store_.read(key_, kMaximumSafetyRecordBytes);
    if (readback.status != device_platform::StateStoreReadStatus::Success) {
        return {
            writeStatus ==
                    device_platform::StateStoreWriteStatus::CommitOutcomeUnknown
                ? SafetyRecordCommitStatus::CommitOutcomeUnknown
                : SafetyRecordCommitStatus::ReadbackError};
    }
    if (readback.value != encoded) {
        return {
            writeStatus ==
                    device_platform::StateStoreWriteStatus::CommitOutcomeUnknown
                ? SafetyRecordCommitStatus::CommitOutcomeUnknown
                : SafetyRecordCommitStatus::ReadbackMismatch};
    }
    return {SafetyRecordCommitStatus::Committed};
}

RestartCauseEvent classifyRestartCause(device_platform::ResetCause cause) {
    switch (cause) {
        case device_platform::ResetCause::PowerOn:
            return RestartCauseEvent::PowerOn;
        case device_platform::ResetCause::AuthorizedRestart:
            return RestartCauseEvent::Authorized;
        case device_platform::ResetCause::ControlledSafetyRestart:
            return RestartCauseEvent::ControlledSafety;
        case device_platform::ResetCause::WatchdogOrPanic:
            return RestartCauseEvent::WatchdogOrPanic;
        case device_platform::ResetCause::Brownout:
            return RestartCauseEvent::Brownout;
        case device_platform::ResetCause::Unknown:
            return RestartCauseEvent::Unknown;
    }
    return RestartCauseEvent::Unknown;
}

}  // namespace fermentation
