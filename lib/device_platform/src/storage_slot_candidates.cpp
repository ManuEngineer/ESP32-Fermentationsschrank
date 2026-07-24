#include "storage_slot_candidates.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "storage_envelope.hpp"

namespace device_platform {

namespace {

// Nur intern nach `IStateStore::read()` aufgerufen, und dort ausschliesslich
// fuer `status != Success` (siehe Aufrufstelle unten). `read()` liefert laut
// eigenem Vertrag (state_store.hpp) niemals `WriteError` oder
// `CommitOutcomeUnknown` - die beiden Faelle sind an dieser Aufrufstelle
// nachweislich unerreichbar, nicht nur zufaellig ungenutzt. Der Fallback
// existiert ausschliesslich, damit der `switch` ohne `default` vollstaendig
// bleibt (jeder neue `StateStoreStatus`-Wert erzeugt eine
// `-Wswitch`-Warnung, die als Fehler behandelt wird).
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
    return SlotIssueKind::ReadError;
}

// Nur intern nach `decodeEnvelope()` aufgerufen, und dort ausschliesslich
// fuer `status != Success` (siehe Aufrufstelle unten) - `Success` ist an
// dieser Aufrufstelle nachweislich unerreichbar. Der Fallback existiert
// ausschliesslich, damit der `switch` ohne `default` vollstaendig bleibt.
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
        // Nicht `const`: die Payload wird bei einem technisch gueltigen und
        // passenden Kandidaten unten in `SlotCandidate` verschoben statt
        // kopiert (siehe docs/CONFIGURATION_PERSISTENCE.md, Abschnitt
        // "Ressourcenvertrag").
        auto decoded = decodeEnvelope(readResult.value);
        if (decoded.status != EnvelopeDecodeStatus::Success ||
            !decoded.envelope.has_value()) {
            result.issues.push_back(
                SlotIssue{slot, toSlotIssueKind(decoded.status)});
            continue;
        }
        auto& envelope = *decoded.envelope;
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
            std::move(envelope.payload),
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
    if (slotCount == 0U ||
        slotCount >
            static_cast<std::size_t>(std::numeric_limits<uint32_t>::max())) {
        return SlotId(0U);
    }
    const auto slotCount32 = static_cast<uint32_t>(slotCount);
    // Zuerst modulo reduzieren, dann eins addieren: `normalized` ist immer
    // `< slotCount32 <= UINT32_MAX`, daher kann `normalized + 1` nie
    // ueberlaufen - unabhaengig davon, ob `size_t` auf der Zielplattform 32
    // oder 64 Bit breit ist.
    const uint32_t normalized = lastWrittenSlot.value() % slotCount32;
    const uint32_t next =
        (normalized + 1U == slotCount32) ? 0U : normalized + 1U;
    return SlotId(next);
}

}  // namespace device_platform
