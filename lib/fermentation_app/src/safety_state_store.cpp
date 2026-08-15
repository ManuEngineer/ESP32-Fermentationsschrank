#include "safety_state_store.hpp"

#include <limits>

#include "storage_envelope.hpp"

namespace fermentation {
namespace {

constexpr std::uint16_t kSafetyRecordType = 0x24U;

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
    out =
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes[offset]))
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
    for (std::size_t index = 0U; index < 4U; ++index) {
        out =
            (out << 8U) | static_cast<std::uint32_t>(
                              static_cast<std::uint8_t>(bytes[offset + index]));
    }
    offset += 4U;
    return true;
}

bool readU64(const std::string& bytes, std::size_t& offset,
             std::uint64_t& out) {
    if (bytes.size() - offset < 8U) return false;
    out = 0U;
    for (std::size_t index = 0U; index < 8U; ++index) {
        out =
            (out << 8U) | static_cast<std::uint64_t>(
                              static_cast<std::uint8_t>(bytes[offset + index]));
    }
    offset += 8U;
    return true;
}

void padTo(std::string& bytes, std::size_t target) {
    while (bytes.size() < target) appendU8(bytes, 0U);
}

void encodeFault(std::string& bytes, const FaultRecord& fault) {
    const auto start = bytes.size();
    appendU32(bytes, fault.instanceId.value);
    appendU16(bytes, static_cast<std::uint16_t>(fault.code));
    appendU8(bytes, static_cast<std::uint8_t>(fault.faultClass));
    appendU8(bytes, static_cast<std::uint8_t>(fault.status));
    appendU8(bytes, static_cast<std::uint8_t>(fault.disposition));
    appendU8(bytes,
             static_cast<std::uint8_t>(
                 (fault.causeActive ? 1U : 0U) | (fault.latched ? 2U : 0U) |
                 (fault.automaticRecoveryRestartUsed ? 4U : 0U)));
    appendU32(bytes, fault.sourceKey);
    appendU32(bytes, fault.correlationKey);
    appendU32(bytes, fault.creationSequence);
    appendU64(bytes, fault.createdAtMonotonicMillis);
    appendU32(bytes, fault.faultRevision);
    appendU32(bytes, fault.primaryFaultId.has_value()
                         ? fault.primaryFaultId->value
                         : 0U);
    appendU64(bytes, fault.diagnosticSequenceHighWatermark);
    padTo(bytes, start + kSafetyRecordSlotPayloadBytes);
}

bool decodeFault(const std::string& bytes, std::size_t& offset,
                 FaultRecord& fault) {
    const auto start = offset;
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
    if (bytes.size() - offset <
        start + kSafetyRecordSlotPayloadBytes - offset) {
        return false;
    }
    offset = start + kSafetyRecordSlotPayloadBytes;
    fault.instanceId = {instance};
    fault.code = static_cast<FaultCode>(code);
    fault.faultClass = static_cast<FaultClass>(faultClass);
    fault.status = static_cast<FaultStatus>(status);
    fault.disposition = static_cast<SafetyDisposition>(disposition);
    fault.causeActive = (flags & 1U) != 0U;
    fault.latched = (flags & 2U) != 0U;
    fault.automaticRecoveryRestartUsed = (flags & 4U) != 0U;
    fault.primaryFaultId = primary == 0U
                               ? std::nullopt
                               : std::optional<FaultInstanceId>{{primary}};
    return true;
}

std::string encodePayload(const SafetyStateRecord& record) {
    std::string bytes;
    bytes.reserve(kSafetyRecordPayloadBytes);
    appendU32(bytes, record.schemaVersion);
    appendU32(bytes, record.recordRevision);
    appendU32(bytes, record.faultRevision);
    appendU32(bytes, record.faultInstanceSequence);
    appendU64(bytes, record.storageEpoch.value());
    appendU32(bytes, static_cast<std::uint32_t>(record.latchCount));
    appendU8(bytes, record.safeBootRequired ? 1U : 0U);
    appendU8(bytes, record.capacityFailureLatched ? 1U : 0U);
    appendU32(bytes, record.capacityFailureRevision);
    appendU32(bytes, record.capacityFailureSourceKey);
    appendU32(bytes, record.capacityFailureCorrelationKey);
    appendU16(bytes, static_cast<std::uint16_t>(record.dominantCode));
    appendU8(bytes, static_cast<std::uint8_t>(record.lastResetCause));
    appendU64(bytes, record.lastResetObservationId);
    appendU32(bytes, record.restartEpisode.episodeId);
    appendU32(bytes, record.restartEpisode.abnormalRestartCount);
    appendU32(bytes, record.restartEpisode.lastRestartEvidenceId);
    appendU32(bytes, record.restartEpisode.nextRestartEvidenceId);
    appendU8(bytes, record.restartEpisode.open ? 1U : 0U);
    appendU8(bytes, record.restartEpisode.stableWindowRunning ? 1U : 0U);
    appendU64(bytes, record.restartEpisode.stableWindowStartedAtMillis);
    appendU32(bytes, record.restartEvidence.evidenceId);
    appendU64(bytes, record.restartEvidence.authorizationEvidenceId);
    appendU8(bytes, static_cast<std::uint8_t>(record.restartEvidence.cause));
    appendU8(bytes, static_cast<std::uint8_t>(record.restartEvidence.state));
    appendU8(bytes, static_cast<std::uint8_t>(record.restartEvidence.intent));
    appendU32(bytes, record.restartEvidence.targetFault.value);
    appendU32(bytes, record.restartEvidence.targetFaultRevision);
    appendU32(bytes, record.restartEvidence.episodeId);
    appendU32(bytes, record.restartEvidence.evidenceRevision);
    appendU8(bytes, static_cast<std::uint8_t>(record.capacityFailureKind));
    padTo(bytes, kSafetyRecordBasePayloadBytes);
    for (std::size_t index = 0U; index < kMaximumPersistedLatches; ++index) {
        encodeFault(bytes, index < record.latchCount ? record.latches[index]
                                                     : FaultRecord{});
    }
    return bytes;
}

bool decodePayload(const std::string& bytes, SafetyStateRecord& record) {
    if (bytes.size() != kSafetyRecordPayloadBytes) return false;
    std::size_t offset = 0U;
    std::uint64_t epoch = 0U;
    std::uint32_t latchCount = 0U;
    std::uint8_t safeBoot = 0U;
    std::uint8_t capacityFailure = 0U;
    std::uint8_t capacityFailureKind = 0U;
    std::uint16_t dominant = 0U;
    std::uint8_t resetCause = 0U;
    std::uint8_t open = 0U;
    std::uint8_t stable = 0U;
    std::uint8_t evidenceCause = 0U;
    std::uint8_t evidenceState = 0U;
    std::uint8_t evidenceIntent = 0U;
    std::uint32_t targetFault = 0U;
    if (!readU32(bytes, offset, record.schemaVersion) ||
        !readU32(bytes, offset, record.recordRevision) ||
        !readU32(bytes, offset, record.faultRevision) ||
        !readU32(bytes, offset, record.faultInstanceSequence) ||
        !readU64(bytes, offset, epoch) || !readU32(bytes, offset, latchCount) ||
        !readU8(bytes, offset, safeBoot) ||
        !readU8(bytes, offset, capacityFailure) ||
        !readU32(bytes, offset, record.capacityFailureRevision) ||
        !readU32(bytes, offset, record.capacityFailureSourceKey) ||
        !readU32(bytes, offset, record.capacityFailureCorrelationKey) ||
        !readU16(bytes, offset, dominant) ||
        !readU8(bytes, offset, resetCause) ||
        !readU64(bytes, offset, record.lastResetObservationId) ||
        !readU32(bytes, offset, record.restartEpisode.episodeId) ||
        !readU32(bytes, offset, record.restartEpisode.abnormalRestartCount) ||
        !readU32(bytes, offset, record.restartEpisode.lastRestartEvidenceId) ||
        !readU32(bytes, offset, record.restartEpisode.nextRestartEvidenceId) ||
        !readU8(bytes, offset, open) || !readU8(bytes, offset, stable) ||
        !readU64(bytes, offset,
                 record.restartEpisode.stableWindowStartedAtMillis) ||
        !readU32(bytes, offset, record.restartEvidence.evidenceId) ||
        !readU64(bytes, offset,
                 record.restartEvidence.authorizationEvidenceId) ||
        !readU8(bytes, offset, evidenceCause) ||
        !readU8(bytes, offset, evidenceState) ||
        !readU8(bytes, offset, evidenceIntent) ||
        !readU32(bytes, offset, targetFault) ||
        !readU32(bytes, offset, record.restartEvidence.targetFaultRevision) ||
        !readU32(bytes, offset, record.restartEvidence.episodeId) ||
        !readU32(bytes, offset, record.restartEvidence.evidenceRevision) ||
        !readU8(bytes, offset, capacityFailureKind)) {
        return false;
    }
    offset = kSafetyRecordBasePayloadBytes;
    record.storageEpoch = device_platform::StorageEpoch{epoch};
    record.latchCount = latchCount;
    record.safeBootRequired = safeBoot != 0U;
    record.capacityFailureLatched = capacityFailure != 0U;
    record.capacityFailureKind =
        static_cast<SafetyMarkerErrorKind>(capacityFailureKind);
    record.dominantCode = static_cast<FaultCode>(dominant);
    record.lastResetCause =
        static_cast<device_platform::ResetCause>(resetCause);
    record.restartEpisode.open = open != 0U;
    record.restartEpisode.stableWindowRunning = stable != 0U;
    record.restartEvidence.cause =
        static_cast<RestartCauseEvent>(evidenceCause);
    record.restartEvidence.state =
        static_cast<RestartEvidenceState>(evidenceState);
    record.restartEvidence.intent =
        static_cast<RestartIntentType>(evidenceIntent);
    record.restartEvidence.targetFault = {targetFault};
    for (std::size_t index = 0U; index < kMaximumPersistedLatches; ++index) {
        if (!decodeFault(bytes, offset, record.latches[index])) return false;
    }
    return validateSafetyStateRecord(record) == SafetyRecordValidation::Valid;
}

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
    return disposition == SafetyDisposition::ImmediateStop;
}

bool knownRestartCause(RestartCauseEvent cause) {
    switch (cause) {
        case RestartCauseEvent::SoftwareRestart:
        case RestartCauseEvent::WatchdogOrPanic:
        case RestartCauseEvent::Brownout:
        case RestartCauseEvent::PowerOn:
        case RestartCauseEvent::ExternalOrOther:
        case RestartCauseEvent::Unknown:
            return true;
    }
    return false;
}

bool knownRestartIntent(RestartIntentType intent) {
    switch (intent) {
        case RestartIntentType::None:
        case RestartIntentType::AutomaticSafetyRecovery:
        case RestartIntentType::AuthorizedTechnicalRestart:
        case RestartIntentType::AuthorizedSafeBootExit:
        case RestartIntentType::Unknown:
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
        case device_platform::ResetCause::SoftwareRestart:
        case device_platform::ResetCause::WatchdogOrPanic:
        case device_platform::ResetCause::Brownout:
        case device_platform::ResetCause::ExternalOrOther:
        case device_platform::ResetCause::Unknown:
            return true;
    }
    return false;
}

bool knownSafetyMarkerErrorKind(SafetyMarkerErrorKind kind) {
    switch (kind) {
        case SafetyMarkerErrorKind::None:
        case SafetyMarkerErrorKind::Read:
        case SafetyMarkerErrorKind::Write:
        case SafetyMarkerErrorKind::Capacity:
        case SafetyMarkerErrorKind::Integrity:
        case SafetyMarkerErrorKind::ReadbackMismatch:
        case SafetyMarkerErrorKind::CommitOutcomeUnknown:
        case SafetyMarkerErrorKind::Unknown:
            return true;
    }
    return false;
}

}  // namespace

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
        !knownRestartIntent(record.restartEvidence.intent) ||
        !knownResetCause(record.lastResetCause) ||
        !knownSafetyMarkerErrorKind(record.capacityFailureKind) ||
        (record.restartEpisode.open && record.restartEpisode.episodeId == 0U) ||
        (record.restartEpisode.stableWindowRunning &&
         !record.restartEpisode.open)) {
        return SafetyRecordValidation::InvalidField;
    }
    if (record.capacityFailureLatched &&
        (record.capacityFailureRevision == 0U || !record.safeBootRequired ||
         record.capacityFailureKind == SafetyMarkerErrorKind::None)) {
        return SafetyRecordValidation::InvalidRelationship;
    }
    if (!record.capacityFailureLatched &&
        (record.capacityFailureRevision != 0U ||
         record.capacityFailureSourceKey != 0U ||
         record.capacityFailureCorrelationKey != 0U ||
         record.capacityFailureKind != SafetyMarkerErrorKind::None)) {
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
            !knownDisposition(fault.disposition) ||
            fault.disposition != SafetyDisposition::ImmediateStop ||
            fault.status == FaultStatus::Cleared || fault.faultRevision == 0U ||
            // D3: a latch's own revision can never be newer than the
            // global FaultCore revision it was last mutated under.
            fault.faultRevision > record.faultRevision ||
            (fault.primaryFaultId.has_value() &&
             fault.primaryFaultId->value == fault.instanceId.value) ||
            fault.instanceId.value > record.faultInstanceSequence) {
            return SafetyRecordValidation::InvalidRelationship;
        }
        for (std::size_t other = 0U; other < index; ++other) {
            if (record.latches[other].instanceId == fault.instanceId) {
                return SafetyRecordValidation::InvalidRelationship;
            }
            // D3: S3-008 and Y4-008 mechanically normalize to one fixed
            // bounded identity (normalizeBoundedFaultIdentity()), so two
            // persisted latches can never legitimately share either code.
            // Every other code's "at most one active instance" bound is a
            // producer-discipline assumption feeding the worst-case
            // capacity analysis, not a FaultCore-level identity constraint;
            // enforcing it here would reject legitimate independent
            // instances (e.g. distinct sensor roles/correlations).
            if ((fault.code == FaultCode::S3_008 ||
                 fault.code == FaultCode::Y4_008) &&
                record.latches[other].code == fault.code) {
                return SafetyRecordValidation::InvalidRelationship;
            }
        }
        if (fault.primaryFaultId.has_value()) {
            bool found = false;
            for (std::size_t primary = 0U; primary < record.latchCount;
                 ++primary) {
                if (record.latches[primary].instanceId ==
                    *fault.primaryFaultId) {
                    found = true;
                    break;
                }
            }
            if (!found) return SafetyRecordValidation::InvalidRelationship;
        }
    }
    // D3: dominantCode must equal the actually reconstructed dominant among
    // the persisted latches (highest class, then lowest code priority, then
    // earliest creation), not merely occur somewhere in the record. Every
    // persisted latch is already a latched class by the per-fault check
    // above, so a persisted latched fault always outranks the non-persisted
    // P1/O2 domain.
    {
        const FaultRecord* reconstructedDominant = nullptr;
        for (std::size_t index = 0U; index < record.latchCount; ++index) {
            const auto& candidate = record.latches[index];
            if (reconstructedDominant == nullptr ||
                static_cast<std::uint8_t>(candidate.faultClass) >
                    static_cast<std::uint8_t>(
                        reconstructedDominant->faultClass) ||
                (candidate.faultClass == reconstructedDominant->faultClass &&
                 (faultCodePriority(candidate.code) <
                      faultCodePriority(reconstructedDominant->code) ||
                  (faultCodePriority(candidate.code) ==
                       faultCodePriority(reconstructedDominant->code) &&
                   candidate.creationSequence <
                       reconstructedDominant->creationSequence)))) {
                reconstructedDominant = &candidate;
            }
        }
        const auto expectedDominantCode = reconstructedDominant == nullptr
                                              ? FaultCode::Unknown
                                              : reconstructedDominant->code;
        if (record.dominantCode != expectedDominantCode) {
            return SafetyRecordValidation::InvalidRelationship;
        }
    }
    if (record.restartEvidence.state == RestartEvidenceState::None) {
        if (record.restartEvidence.evidenceId != 0U ||
            record.restartEvidence.authorizationEvidenceId != 0U ||
            record.restartEvidence.intent != RestartIntentType::None ||
            record.restartEvidence.targetFault.valid() ||
            record.restartEvidence.targetFaultRevision != 0U ||
            record.restartEvidence.episodeId != 0U ||
            record.restartEvidence.evidenceRevision != 0U) {
            return SafetyRecordValidation::InvalidRelationship;
        }
    } else if (record.restartEvidence.evidenceId == 0U ||
               record.restartEvidence.intent == RestartIntentType::None ||
               record.restartEvidence.intent == RestartIntentType::Unknown ||
               record.restartEvidence.evidenceRevision == 0U ||
               record.restartEvidence.episodeId == 0U) {
        return SafetyRecordValidation::InvalidRelationship;
    }
    if ((record.restartEvidence.intent ==
             RestartIntentType::AuthorizedTechnicalRestart ||
         record.restartEvidence.intent ==
             RestartIntentType::AuthorizedSafeBootExit) !=
        (record.restartEvidence.authorizationEvidenceId != 0U)) {
        return SafetyRecordValidation::InvalidRelationship;
    }
    if (record.restartEvidence.intent ==
            RestartIntentType::AutomaticSafetyRecovery &&
        record.restartEvidence.authorizationEvidenceId != 0U) {
        return SafetyRecordValidation::InvalidRelationship;
    }
    if (record.restartEvidence.intent ==
            RestartIntentType::AutomaticSafetyRecovery &&
        (!record.restartEvidence.targetFault.valid() ||
         record.restartEvidence.targetFaultRevision == 0U)) {
        return SafetyRecordValidation::InvalidRelationship;
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
    if (decoded.envelope->versionValue >
        std::numeric_limits<std::uint32_t>::max()) {
        return SafetyRecordDecodeStatus::InvalidRecord;
    }
    if (!decodePayload(decoded.envelope->payload, outRecord)) {
        return SafetyRecordDecodeStatus::InvalidRecord;
    }
    // D3: the envelope carries the authoritative schema/version/epoch; the
    // payload carries its own redundant copy. decodePayload() sets these
    // fields from the payload bytes alone, so a splice or partial
    // corruption that changes one without the other must be caught here
    // instead of letting whichever value decoded last silently win.
    const auto expectedRevision =
        static_cast<std::uint32_t>(decoded.envelope->versionValue);
    if (outRecord.recordRevision != expectedRevision ||
        outRecord.storageEpoch.value() !=
            decoded.envelope->storageEpoch.value() ||
        outRecord.schemaVersion != decoded.envelope->schemaVersion) {
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
        if (commit(initial).status != SafetyRecordCommitStatus::Committed) {
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
        case device_platform::ResetCause::SoftwareRestart:
            return RestartCauseEvent::SoftwareRestart;
        case device_platform::ResetCause::WatchdogOrPanic:
            return RestartCauseEvent::WatchdogOrPanic;
        case device_platform::ResetCause::Brownout:
            return RestartCauseEvent::Brownout;
        case device_platform::ResetCause::ExternalOrOther:
            return RestartCauseEvent::ExternalOrOther;
        case device_platform::ResetCause::Unknown:
            return RestartCauseEvent::Unknown;
    }
    return RestartCauseEvent::Unknown;
}

}  // namespace fermentation
