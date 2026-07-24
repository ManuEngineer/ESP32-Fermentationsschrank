#include "storage_slot_candidates.hpp"

#include <algorithm>

#include "storage_envelope.hpp"

namespace device_platform {

namespace {

SlotIssueKind toSlotIssueKind(StateStoreStatus status) {
    switch (status) {
        case StateStoreStatus::NotFound:
            return SlotIssueKind::NotFound;
        case StateStoreStatus::ReadError:
            return SlotIssueKind::ReadError;
        case StateStoreStatus::CapacityError:
            return SlotIssueKind::CapacityError;
        case StateStoreStatus::Success:
        case StateStoreStatus::WriteError:
        case StateStoreStatus::CommitOutcomeUnknown:
            break;
    }
    // Nur Lesestatus werden hier abgebildet; die aufrufenden Codepfade rufen
    // dies ausschliesslich fuer `status != Success` und nur nach `read()` auf.
    return SlotIssueKind::ReadError;
}

SlotIssueKind toSlotIssueKind(EnvelopeDecodeStatus status) {
    switch (status) {
        case EnvelopeDecodeStatus::InvalidMagic:
            return SlotIssueKind::InvalidMagic;
        case EnvelopeDecodeStatus::UnknownEnvelopeVersion:
            return SlotIssueKind::UnknownEnvelopeVersion;
        case EnvelopeDecodeStatus::InvalidRecordType:
            return SlotIssueKind::InvalidRecordType;
        case EnvelopeDecodeStatus::InvalidSchemaVersion:
            return SlotIssueKind::InvalidSchemaVersion;
        case EnvelopeDecodeStatus::InvalidStorageEpoch:
            return SlotIssueKind::InvalidStorageEpoch;
        case EnvelopeDecodeStatus::InvalidVersionValue:
            return SlotIssueKind::InvalidVersionValue;
        case EnvelopeDecodeStatus::InvalidUtcTag:
            return SlotIssueKind::InvalidUtcTag;
        case EnvelopeDecodeStatus::LengthMismatch:
            return SlotIssueKind::LengthMismatch;
        case EnvelopeDecodeStatus::CrcMismatch:
            return SlotIssueKind::CrcMismatch;
        case EnvelopeDecodeStatus::Success:
            break;
    }
    return SlotIssueKind::LengthMismatch;
}

}  // namespace

SlotScanResult scanTechnicalSlotCandidates(
    const IStateStore& store, const std::vector<StateStoreKey>& slotKeys,
    RecordTypeId expectedRecordType, uint32_t expectedSchemaVersion,
    StorageEpoch expectedStorageEpoch, std::size_t maxEnvelopeBytes) {
    SlotScanResult result;
    for (std::size_t index = 0U; index < slotKeys.size(); ++index) {
        const auto slot = SlotId(static_cast<uint32_t>(index));
        const auto readResult = store.read(slotKeys[index], maxEnvelopeBytes);
        if (readResult.status != StateStoreStatus::Success) {
            result.issues.push_back(
                SlotIssue{slot, toSlotIssueKind(readResult.status)});
            continue;
        }
        const auto decoded = decodeEnvelope(readResult.value);
        if (decoded.status != EnvelopeDecodeStatus::Success ||
            !decoded.envelope.has_value()) {
            result.issues.push_back(
                SlotIssue{slot, toSlotIssueKind(decoded.status)});
            continue;
        }
        const auto& envelope = *decoded.envelope;
        if (envelope.recordTypeId != expectedRecordType ||
            envelope.schemaVersion != expectedSchemaVersion ||
            envelope.storageEpoch != expectedStorageEpoch) {
            result.issues.push_back(
                SlotIssue{slot, SlotIssueKind::RecordIdentityMismatch});
            continue;
        }
        result.candidates.push_back(SlotCandidate{
            slot,
            envelope.versionValue,
            envelope.payload,
            envelope.utcUnixSeconds,
        });
    }
    // Bei gleichem VersionValue entscheidet die Slot-ID aufsteigend, damit
    // die Reihenfolge deterministisch bleibt (`std::sort` ist nicht stabil).
    std::sort(result.candidates.begin(), result.candidates.end(),
              [](const SlotCandidate& left, const SlotCandidate& right) {
                  if (left.versionValue != right.versionValue) {
                      return left.versionValue > right.versionValue;
                  }
                  return left.slot.value() < right.slot.value();
              });
    return result;
}

SlotId nextSlotRoundRobin(SlotId lastWrittenSlot, std::size_t slotCount) {
    if (slotCount == 0U) {
        return SlotId(0U);
    }
    const auto next = static_cast<uint32_t>(
        (static_cast<std::size_t>(lastWrittenSlot.value()) + 1U) % slotCount);
    return SlotId(next);
}

}  // namespace device_platform
