#include "storage_slot_candidates.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "storage_envelope.hpp"

namespace device_platform {

namespace {

// Bildet einen Lesefehlerstatus auf die passende `SlotIssueKind` ab. Wird nur
// fuer `status != Success` aufgerufen; der `Success`-Zweig ist an dieser
// Aufrufstelle nicht erreichbar und liefert `UnexpectedStatus` (nie mit einem
// echten `ReadError` zu verwechseln), damit der `switch` ohne `default`
// vollstaendig bleibt. `WriteError`/`CommitOutcomeUnknown` sind durch den
// Parametertyp `StateStoreReadStatus` ausgeschlossen.
SlotIssueKind toSlotIssueKind(StateStoreReadStatus status) {
    switch (status) {
        case StateStoreReadStatus::NotFound:
            return SlotIssueKind::NotFound;
        case StateStoreReadStatus::ReadError:
            return SlotIssueKind::ReadError;
        case StateStoreReadStatus::CapacityError:
            return SlotIssueKind::CapacityError;
        case StateStoreReadStatus::Success:
            break;
    }
    return SlotIssueKind::UnexpectedStatus;
}

// Bildet einen Envelope-Dekodierfehler auf die passende `SlotIssueKind` ab.
// Wird nur fuer `status != Success` aufgerufen; der `Success`-Zweig ist an
// dieser Aufrufstelle nicht erreichbar und liefert `UnexpectedStatus` (nie mit
// einem echten `LengthMismatch` zu verwechseln), damit der `switch` ohne
// `default` vollstaendig bleibt.
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
    return SlotIssueKind::UnexpectedStatus;
}

}  // namespace

SlotScanResult scanTechnicalSlotCandidates(
    const IStateStore& store, const std::vector<StateStoreKey>& slotKeys,
    RecordTypeId expectedRecordType, uint32_t expectedSchemaVersion,
    StorageEpoch expectedStorageEpoch, std::size_t maxEnvelopeBytes) {
    if (slotKeys.size() > storage_slot_limits::kMaximumTechnicalSlotsPerScan) {
        return SlotScanResult{SlotScanStatus::SlotLimitExceeded, {}, {}};
    }

    SlotScanResult result{SlotScanStatus::Success, {}, {}};
    for (std::size_t index = 0U; index < slotKeys.size(); ++index) {
        const auto slot = SlotId(static_cast<uint32_t>(index));
        const auto readResult = store.read(slotKeys[index], maxEnvelopeBytes);
        if (readResult.status != StateStoreReadStatus::Success) {
            result.issues.push_back(
                SlotIssue{slot, toSlotIssueKind(readResult.status)});
            continue;
        }
        // Metadaten-Dekodierung: validiert inklusive CRC ueber die Payload,
        // materialisiert die Payload aber nicht. Nur `readResult.value` (ein
        // Recordpuffer) liegt waehrend dieser Iteration im Speicher und wird
        // am Iterationsende freigegeben.
        const auto decoded = decodeEnvelopeMetadata(readResult.value);
        if (decoded.status != EnvelopeDecodeStatus::Success ||
            !decoded.metadata.has_value()) {
            result.issues.push_back(
                SlotIssue{slot, toSlotIssueKind(decoded.status)});
            continue;
        }
        const auto& meta = *decoded.metadata;
        if (meta.recordTypeId != expectedRecordType ||
            meta.schemaVersion != expectedSchemaVersion ||
            meta.storageEpoch != expectedStorageEpoch) {
            result.issues.push_back(
                SlotIssue{slot, SlotIssueKind::RecordIdentityMismatch});
            continue;
        }
        result.candidates.push_back(SlotCandidate{
            slot,
            meta.versionValue,
            meta.utcUnixSeconds,
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

SlotPayloadResult loadSlotPayload(const IStateStore& store,
                                  const StateStoreKey& slotKey,
                                  RecordTypeId expectedRecordType,
                                  uint32_t expectedSchemaVersion,
                                  StorageEpoch expectedStorageEpoch,
                                  uint64_t expectedVersionValue,
                                  std::size_t maxEnvelopeBytes) {
    const auto readResult = store.read(slotKey, maxEnvelopeBytes);
    switch (readResult.status) {
        case StateStoreReadStatus::Success:
            break;
        case StateStoreReadStatus::NotFound:
            return {SlotPayloadLoadStatus::NotFound, {}, {}};
        case StateStoreReadStatus::ReadError:
            return {SlotPayloadLoadStatus::ReadError, {}, {}};
        case StateStoreReadStatus::CapacityError:
            return {SlotPayloadLoadStatus::CapacityError, {}, {}};
    }

    // Einziger bewusst benannter Uebergang mit mehr als einer Payload:
    // `readResult.value` (voller Record inkl. Payload) und die von
    // `decodeEnvelope()` materialisierte Kopie bestehen bis zum `std::move`
    // unten gleichzeitig. Der Scan selbst hat diesen Uebergang nicht.
    auto decoded = decodeEnvelope(readResult.value);
    if (decoded.status != EnvelopeDecodeStatus::Success ||
        !decoded.envelope.has_value()) {
        return {SlotPayloadLoadStatus::InvalidEnvelope, {}, {}};
    }
    auto& envelope = *decoded.envelope;
    if (envelope.recordTypeId != expectedRecordType ||
        envelope.schemaVersion != expectedSchemaVersion ||
        envelope.storageEpoch != expectedStorageEpoch) {
        return {SlotPayloadLoadStatus::RecordIdentityMismatch, {}, {}};
    }
    if (envelope.versionValue != expectedVersionValue) {
        return {SlotPayloadLoadStatus::VersionValueMismatch, {}, {}};
    }
    return {SlotPayloadLoadStatus::Success, std::move(envelope.payload),
            envelope.utcUnixSeconds};
}

NextSlotResult nextSlotRoundRobin(SlotId lastWrittenSlot,
                                  std::size_t slotCount) {
    if (slotCount == 0U ||
        slotCount >
            static_cast<std::size_t>(std::numeric_limits<uint32_t>::max())) {
        return NextSlotResult{NextSlotStatus::InvalidSlotCount, std::nullopt};
    }
    const auto slotCount32 = static_cast<uint32_t>(slotCount);
    // Ein `lastWrittenSlot` ausserhalb des gueltigen Bereichs (>= slotCount,
    // z. B. aus korruptem Speicher) wird beobachtbar abgelehnt statt still per
    // Modulo in einen gueltigen Wert verwandelt - konsistent mit
    // `checkedIncrement`, das den reservierten Wert 0 bewusst ablehnt.
    if (lastWrittenSlot.value() >= slotCount32) {
        return NextSlotResult{NextSlotStatus::InvalidLastSlot, std::nullopt};
    }
    // Hier gilt `lastWrittenSlot.value() < slotCount32 <= UINT32_MAX`, daher
    // ueberlaeuft `lastWrittenSlot.value() + 1` nie - unabhaengig von der
    // `size_t`-Breite der Zielplattform.
    const uint32_t next = (lastWrittenSlot.value() + 1U == slotCount32)
                              ? 0U
                              : lastWrittenSlot.value() + 1U;
    return NextSlotResult{NextSlotStatus::Success, SlotId(next)};
}

}  // namespace device_platform
