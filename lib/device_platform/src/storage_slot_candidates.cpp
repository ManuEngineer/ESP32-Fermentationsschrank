#include "storage_slot_candidates.hpp"

#include <algorithm>

#include "storage_envelope.hpp"

namespace device_platform {

std::vector<SlotCandidate> technicalCandidatesDescending(
    const IStateStore& store, const std::vector<std::string>& slotKeys,
    RecordTypeId expectedRecordType, uint32_t expectedSchemaVersion,
    StorageEpoch expectedStorageEpoch, std::size_t maxEnvelopeBytes) {
    std::vector<SlotCandidate> candidates;
    for (std::size_t index = 0U; index < slotKeys.size(); ++index) {
        const auto readResult = store.read(slotKeys[index], maxEnvelopeBytes);
        if (readResult.status != StateStoreStatus::Success) {
            continue;
        }
        const auto decoded = decodeEnvelope(readResult.value);
        if (decoded.status != EnvelopeDecodeStatus::Success ||
            !decoded.envelope.has_value()) {
            continue;
        }
        const auto& envelope = *decoded.envelope;
        if (envelope.recordTypeId != expectedRecordType ||
            envelope.schemaVersion != expectedSchemaVersion ||
            envelope.storageEpoch != expectedStorageEpoch) {
            continue;
        }
        candidates.push_back(SlotCandidate{
            SlotId(static_cast<uint32_t>(index)),
            envelope.versionValue,
            envelope.payload,
            envelope.utcUnixSeconds,
        });
    }
    // Bei gleichem VersionValue entscheidet die Slot-ID aufsteigend, damit
    // die Reihenfolge deterministisch bleibt (`std::sort` ist nicht stabil).
    std::sort(candidates.begin(), candidates.end(),
              [](const SlotCandidate& left, const SlotCandidate& right) {
                  if (left.versionValue != right.versionValue) {
                      return left.versionValue > right.versionValue;
                  }
                  return left.slot.value() < right.slot.value();
              });
    return candidates;
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
