#include <unity.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "mock_secure_random_source.hpp"
#include "mock_time_zone_resolver.hpp"
#include "secure_random_source.hpp"
#include "simulated_persistent_state_store.hpp"
#include "state_store.hpp"
#include "state_store_key.hpp"
#include "storage_envelope.hpp"
#include "storage_slot_candidates.hpp"
#include "storage_types.hpp"
#include "time_zone_resolver.hpp"

namespace {

using device_platform::EnvelopeEncodeStatus;
using device_platform::RecordTypeId;
using device_platform::SlotIssueKind;
using device_platform::StateStoreKey;
using device_platform::StateStoreKeyStatus;
using device_platform::StateStoreStatus;
using device_platform::StorageEnvelope;
using device_platform::StorageEpoch;
using device_platform::TimeZoneResolutionStatus;
using device_platform_test_support::MockSecureRandomSource;
using device_platform_test_support::MockTimeZoneResolver;
using device_platform_test_support::SimulatedPersistentStateStore;

StateStoreKey keyOrFail(const std::string& raw) {
    StateStoreKey key;
    TEST_ASSERT_TRUE(StateStoreKey::create(raw, key) ==
                     StateStoreKeyStatus::Success);
    return key;
}

std::string encodedEnvelopeOrFail(RecordTypeId recordType,
                                  uint32_t schemaVersion, StorageEpoch epoch,
                                  uint64_t versionValue,
                                  const std::string& payload) {
    StorageEnvelope envelope;
    envelope.recordTypeId = recordType;
    envelope.schemaVersion = schemaVersion;
    envelope.storageEpoch = epoch;
    envelope.versionValue = versionValue;
    envelope.payload = payload;
    std::string encoded;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(envelope, encoded, 4096U) ==
        EnvelopeEncodeStatus::Success);
    return encoded;
}

constexpr std::size_t kDefaultMaxBytes = 4096U;

void test_write_and_read_round_trip() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("key"), "value")));
    const auto result = store.read(keyOrFail("key"), kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_STRING("value", result.value.c_str());
}

void test_read_distinguishes_missing_key_from_error() {
    const SimulatedPersistentStateStore store;
    const auto result = store.read(keyOrFail("unknown"), kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::NotFound),
                          static_cast<int>(result.status));
    TEST_ASSERT_TRUE(result.value.empty());
}

void test_read_enforces_caller_specific_maximum_length() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("key"), "0123456789")));

    const auto tooSmallLimit = store.read(keyOrFail("key"), 5U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::CapacityError),
                          static_cast<int>(tooSmallLimit.status));
    TEST_ASSERT_TRUE(tooSmallLimit.value.empty());

    const auto exactLimit = store.read(keyOrFail("key"), 10U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(exactLimit.status));
}

void test_write_fail_before_begin_leaves_store_unchanged() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("key"), "first")));

    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::FailBeforeBegin);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::WriteError),
        static_cast<int>(store.write(keyOrFail("key"), "second")));

    const auto result = store.read(keyOrFail("key"), kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_STRING("first", result.value.c_str());
}

void test_write_power_cut_before_commit_leaves_store_unchanged() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("key"), "first")));

    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::PowerCutBeforeCommit);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::WriteError),
        static_cast<int>(store.write(keyOrFail("key"), "second")));
    store.restart();

    const auto result = store.read(keyOrFail("key"), kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_STRING("first", result.value.c_str());
}

// `CommitOutcomeUnknown` ist bewusst kein `WriteError`: der neue Wert kann
// bereits dauerhaft gespeichert sein. Der Aufrufer erkennt dies nur durch
// Ruecklesen - genau das beweist dieser Test.
void test_write_power_cut_after_commit_before_return_yields_commit_outcome_unknown_and_value_is_durable() {
    SimulatedPersistentStateStore store;
    store.setNextWriteFault(SimulatedPersistentStateStore::WriteFault::
                                PowerCutAfterCommitBeforeReturn);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::CommitOutcomeUnknown),
        static_cast<int>(
            store.write(keyOrFail("key"), "committed_despite_unknown")));

    // Der Aufrufer sah einen unbekannten Commit-Ausgang; erst das Ruecklesen
    // (auch nach einem simulierten Neustart) zeigt, dass der neue Wert
    // bereits dauerhaft gespeichert wurde.
    store.restart();
    const auto result = store.read(keyOrFail("key"), kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_STRING("committed_despite_unknown", result.value.c_str());
}

void test_write_capacity_exceeded_leaves_store_unchanged() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("key"), "first")));

    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::CapacityExceeded);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::CapacityError),
        static_cast<int>(store.write(keyOrFail("key"), "second")));

    const auto result = store.read(keyOrFail("key"), kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_STRING("first", result.value.c_str());
}

void test_write_fault_applies_only_to_next_write() {
    SimulatedPersistentStateStore store;
    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::FailBeforeBegin);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::WriteError),
        static_cast<int>(store.write(keyOrFail("key"), "first")));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("key"), "second")));
    TEST_ASSERT_EQUAL_STRING(
        "second", store.read(keyOrFail("key"), kDefaultMaxBytes).value.c_str());
}

void test_read_failure_injection_is_key_specific_and_recoverable() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("a"), "valueA")));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("b"), "valueB")));

    store.injectReadFailure(keyOrFail("a"), true);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::ReadError),
        static_cast<int>(store.read(keyOrFail("a"), kDefaultMaxBytes).status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.read(keyOrFail("b"), kDefaultMaxBytes).status));

    store.injectReadFailure(keyOrFail("a"), false);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.read(keyOrFail("a"), kDefaultMaxBytes).status));
}

void test_forced_not_found_overrides_existing_committed_value() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("key"), "value")));

    store.forceNotFound(keyOrFail("key"), true);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::NotFound),
        static_cast<int>(
            store.read(keyOrFail("key"), kDefaultMaxBytes).status));

    store.forceNotFound(keyOrFail("key"), false);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(
            store.read(keyOrFail("key"), kDefaultMaxBytes).status));
}

void test_corruption_injection_survives_restart() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("key"), "valid")));

    store.injectCorruption(keyOrFail("key"),
                           std::string("\x00\x01corrupt", 9U));
    store.restart();

    const auto result = store.read(keyOrFail("key"), kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT32(9U, result.value.size());
    TEST_ASSERT_NOT_EQUAL(0, result.value.compare(std::string("valid")));
}

void test_restart_clears_volatile_fault_injection_but_keeps_committed_data() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("key"), "value")));
    store.injectReadFailure(keyOrFail("key"), true);
    store.forceNotFound(keyOrFail("other"), true);

    store.restart();

    const auto result = store.read(keyOrFail("key"), kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_STRING("value", result.value.c_str());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::NotFound),
        static_cast<int>(
            store.read(keyOrFail("other"), kDefaultMaxBytes).status));
}

void test_secure_random_source_fills_requested_length_deterministically() {
    MockSecureRandomSource source(42U);
    uint8_t bufferA[16] = {0};
    uint8_t bufferB[16] = {0};
    TEST_ASSERT_TRUE(source.fill(bufferA, sizeof(bufferA)));

    MockSecureRandomSource sameSeed(42U);
    TEST_ASSERT_TRUE(sameSeed.fill(bufferB, sizeof(bufferB)));
    TEST_ASSERT_EQUAL_MEMORY(bufferA, bufferB, sizeof(bufferA));
}

void test_secure_random_source_next_bytes_override_and_failure_injection() {
    MockSecureRandomSource source;
    source.setNextBytes(std::string("\x01\x02\x03\x04", 4U));
    uint8_t buffer[4] = {0};
    TEST_ASSERT_TRUE(source.fill(buffer, sizeof(buffer)));
    const uint8_t expected[4] = {0x01U, 0x02U, 0x03U, 0x04U};
    TEST_ASSERT_EQUAL_MEMORY(expected, buffer, sizeof(buffer));

    // Der Override gilt nur fuer genau einen Aufruf.
    uint8_t nextBuffer[4] = {0xAAU, 0xAAU, 0xAAU, 0xAAU};
    TEST_ASSERT_TRUE(source.fill(nextBuffer, sizeof(nextBuffer)));
    TEST_ASSERT_TRUE(nextBuffer[0] != 0xAAU || nextBuffer[1] != 0xAAU ||
                     nextBuffer[2] != 0xAAU || nextBuffer[3] != 0xAAU);

    source.injectFailure(true);
    uint8_t untouched[4] = {0x99U, 0x99U, 0x99U, 0x99U};
    TEST_ASSERT_FALSE(source.fill(untouched, sizeof(untouched)));
    const uint8_t stillUntouched[4] = {0x99U, 0x99U, 0x99U, 0x99U};
    TEST_ASSERT_EQUAL_MEMORY(stillUntouched, untouched, sizeof(untouched));
}

void test_time_zone_resolver_known_unknown_and_failure() {
    MockTimeZoneResolver resolver;
    resolver.addKnownZone("Europe/Zurich");

    TEST_ASSERT_EQUAL_INT(static_cast<int>(TimeZoneResolutionStatus::Success),
                          static_cast<int>(resolver.prepare("Europe/Zurich")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TimeZoneResolutionStatus::Unknown),
                          static_cast<int>(resolver.prepare("Etc/Unknown")));

    resolver.injectFailure(true);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TimeZoneResolutionStatus::Error),
                          static_cast<int>(resolver.prepare("Europe/Zurich")));
}

void test_scan_filters_and_sorts_candidates_and_preserves_issues() {
    SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(9U);
    const auto epoch = StorageEpoch(1U);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            keyOrFail("slot0"),
            encodedEnvelopeOrFail(recordType, 1U, epoch, 5U, "a"))));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            keyOrFail("slot1"),
            encodedEnvelopeOrFail(recordType, 1U, epoch, 9U, "b"))));
    // Falscher RecordType: technisch gueltig, aber nicht passend.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(
            store.write(keyOrFail("slot2"),
                        encodedEnvelopeOrFail(RecordTypeId(99U), 1U, epoch, 20U,
                                              "wrong-type"))));
    // Falsche StorageEpoch: technisch gueltig, aber nicht passend.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(
            store.write(keyOrFail("slot3"),
                        encodedEnvelopeOrFail(recordType, 1U, StorageEpoch(2U),
                                              30U, "wrong-epoch"))));
    // Fehlender Slot.
    // ("slot4" wird nie geschrieben.)
    // Envelope strukturell ungueltig: richtige Laenge, falsches Magic.
    auto badMagic = encodedEnvelopeOrFail(recordType, 1U, epoch, 40U, "bad");
    badMagic[0] = 'X';
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("slot5"), badMagic)));

    const std::vector<StateStoreKey> slotKeys{
        keyOrFail("slot0"), keyOrFail("slot1"), keyOrFail("slot2"),
        keyOrFail("slot3"), keyOrFail("slot4"), keyOrFail("slot5"),
    };
    const auto scan = device_platform::scanTechnicalSlotCandidates(
        store, slotKeys, recordType, 1U, epoch, 4096U);

    TEST_ASSERT_EQUAL_UINT32(2U, scan.candidates.size());
    TEST_ASSERT_EQUAL_UINT64(9U, scan.candidates[0].versionValue);
    TEST_ASSERT_EQUAL_UINT32(1U, scan.candidates[0].slot.value());
    TEST_ASSERT_EQUAL_STRING("b", scan.candidates[0].payload.c_str());
    TEST_ASSERT_EQUAL_UINT64(5U, scan.candidates[1].versionValue);
    TEST_ASSERT_EQUAL_UINT32(0U, scan.candidates[1].slot.value());

    // Kein uebersprungener Slot geht verloren; jeder erscheint mit der
    // spezifischen Ursache, nicht als undifferenzierte leere Luecke.
    TEST_ASSERT_EQUAL_UINT32(4U, scan.issues.size());
    TEST_ASSERT_EQUAL_UINT32(2U, scan.issues[0].slot.value());  // slot2
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SlotIssueKind::RecordIdentityMismatch),
        static_cast<int>(scan.issues[0].kind));
    TEST_ASSERT_EQUAL_UINT32(3U, scan.issues[1].slot.value());  // slot3
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SlotIssueKind::RecordIdentityMismatch),
        static_cast<int>(scan.issues[1].kind));
    TEST_ASSERT_EQUAL_UINT32(4U, scan.issues[2].slot.value());  // slot4
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SlotIssueKind::NotFound),
                          static_cast<int>(scan.issues[2].kind));
    TEST_ASSERT_EQUAL_UINT32(5U, scan.issues[3].slot.value());  // slot5
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SlotIssueKind::InvalidMagic),
                          static_cast<int>(scan.issues[3].kind));
}

void test_scan_reports_not_found_for_every_slot_on_factory_empty_storage() {
    const SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(1U);
    const auto epoch = StorageEpoch(1U);
    const std::vector<StateStoreKey> slotKeys{
        keyOrFail("slot0"), keyOrFail("slot1"), keyOrFail("slot2")};

    const auto scan = device_platform::scanTechnicalSlotCandidates(
        store, slotKeys, recordType, 1U, epoch, 4096U);

    TEST_ASSERT_TRUE(scan.candidates.empty());
    TEST_ASSERT_EQUAL_UINT32(3U, scan.issues.size());
    for (const auto& issue : scan.issues) {
        TEST_ASSERT_EQUAL_INT(static_cast<int>(SlotIssueKind::NotFound),
                              static_cast<int>(issue.kind));
    }
}

// `ReadError` darf niemals wie `NotFound` behandelt werden: ein
// beschaedigter, unlesbarer Slot ist kein fabrikleerer Speicher. Ein Scan
// mit beiden Faellen nebeneinander muss sie unterscheidbar halten.
void test_scan_never_conflates_read_error_with_not_found() {
    SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(1U);
    const auto epoch = StorageEpoch(1U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            keyOrFail("slot0"),
            encodedEnvelopeOrFail(recordType, 1U, epoch, 1U, "ok"))));
    store.injectReadFailure(keyOrFail("slot1"), true);
    // "slot2" wird nie geschrieben (NotFound).

    const std::vector<StateStoreKey> slotKeys{
        keyOrFail("slot0"), keyOrFail("slot1"), keyOrFail("slot2")};
    const auto scan = device_platform::scanTechnicalSlotCandidates(
        store, slotKeys, recordType, 1U, epoch, 4096U);

    TEST_ASSERT_EQUAL_UINT32(1U, scan.candidates.size());
    TEST_ASSERT_EQUAL_UINT32(2U, scan.issues.size());
    TEST_ASSERT_EQUAL_UINT32(1U, scan.issues[0].slot.value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SlotIssueKind::ReadError),
                          static_cast<int>(scan.issues[0].kind));
    TEST_ASSERT_EQUAL_UINT32(2U, scan.issues[1].slot.value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SlotIssueKind::NotFound),
                          static_cast<int>(scan.issues[1].kind));
}

void test_scan_preserves_capacity_error_on_read_limit() {
    SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(1U);
    const auto epoch = StorageEpoch(1U);
    const auto oversized = encodedEnvelopeOrFail(recordType, 1U, epoch, 1U,
                                                 std::string(100U, 'x'));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("slot0"), oversized)));

    const std::vector<StateStoreKey> slotKeys{keyOrFail("slot0")};
    // `maxEnvelopeBytes` kleiner als der tatsaechlich gespeicherte Envelope:
    // der Speicherport liefert `CapacityError`, kein `NotFound`.
    const auto scan = device_platform::scanTechnicalSlotCandidates(
        store, slotKeys, recordType, 1U, epoch, 10U);

    TEST_ASSERT_TRUE(scan.candidates.empty());
    TEST_ASSERT_EQUAL_UINT32(1U, scan.issues.size());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SlotIssueKind::CapacityError),
                          static_cast<int>(scan.issues[0].kind));
}

void test_scan_preserves_crc_mismatch_issue() {
    SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(1U);
    const auto epoch = StorageEpoch(1U);
    auto corrupted =
        encodedEnvelopeOrFail(recordType, 1U, epoch, 1U, "payload");
    corrupted.back() = static_cast<char>(corrupted.back() ^ 0x01);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("slot0"), corrupted)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            keyOrFail("slot1"),
            encodedEnvelopeOrFail(recordType, 1U, epoch, 2U, "ok"))));

    const std::vector<StateStoreKey> slotKeys{keyOrFail("slot0"),
                                              keyOrFail("slot1")};
    const auto scan = device_platform::scanTechnicalSlotCandidates(
        store, slotKeys, recordType, 1U, epoch, 4096U);

    TEST_ASSERT_EQUAL_UINT32(1U, scan.candidates.size());
    TEST_ASSERT_EQUAL_UINT32(1U, scan.candidates[0].slot.value());
    TEST_ASSERT_EQUAL_UINT32(1U, scan.issues.size());
    TEST_ASSERT_EQUAL_UINT32(0U, scan.issues[0].slot.value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SlotIssueKind::CrcMismatch),
                          static_cast<int>(scan.issues[0].kind));
}

void test_scan_break_ties_by_slot_id_ascending() {
    SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(9U);
    const auto epoch = StorageEpoch(1U);

    // Gleicher versionValue in mehreren Slots: Sortierung muss deterministisch
    // sein (std::sort ist nicht stabil), Tiebreak per aufsteigender Slot-ID.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            keyOrFail("slot0"),
            encodedEnvelopeOrFail(recordType, 1U, epoch, 7U, "a"))));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            keyOrFail("slot1"),
            encodedEnvelopeOrFail(recordType, 1U, epoch, 7U, "b"))));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            keyOrFail("slot2"),
            encodedEnvelopeOrFail(recordType, 1U, epoch, 7U, "c"))));

    const std::vector<StateStoreKey> slotKeys{
        keyOrFail("slot0"), keyOrFail("slot1"), keyOrFail("slot2")};
    const auto scan = device_platform::scanTechnicalSlotCandidates(
        store, slotKeys, recordType, 1U, epoch, 4096U);

    TEST_ASSERT_EQUAL_UINT32(3U, scan.candidates.size());
    TEST_ASSERT_EQUAL_UINT32(0U, scan.candidates[0].slot.value());
    TEST_ASSERT_EQUAL_UINT32(1U, scan.candidates[1].slot.value());
    TEST_ASSERT_EQUAL_UINT32(2U, scan.candidates[2].slot.value());
}

void test_scan_handles_max_version_value_without_overflow() {
    SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(9U);
    const auto epoch = StorageEpoch(1U);

    // Sequenzueberlauf-Schutz beim Erhoehen von versionValue gehoert zur
    // schreibenden Anwendungsschicht (#55/#56, siehe `checkedIncrement` in
    // storage_types.hpp); #54 muss lediglich den vollen 64-Bit-Wertebereich
    // verlustfrei kodieren, dekodieren und korrekt einsortieren.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            keyOrFail("slot0"),
            encodedEnvelopeOrFail(recordType, 1U, epoch,
                                  std::numeric_limits<uint64_t>::max(),
                                  "max"))));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            keyOrFail("slot1"),
            encodedEnvelopeOrFail(recordType, 1U, epoch,
                                  std::numeric_limits<uint64_t>::max() - 1U,
                                  "near-max"))));

    const std::vector<StateStoreKey> slotKeys{keyOrFail("slot0"),
                                              keyOrFail("slot1")};
    const auto scan = device_platform::scanTechnicalSlotCandidates(
        store, slotKeys, recordType, 1U, epoch, 4096U);

    TEST_ASSERT_EQUAL_UINT32(2U, scan.candidates.size());
    TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<uint64_t>::max(),
                             scan.candidates[0].versionValue);
    TEST_ASSERT_EQUAL_STRING("max", scan.candidates[0].payload.c_str());
    TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<uint64_t>::max() - 1U,
                             scan.candidates[1].versionValue);
}

void test_next_slot_round_robin_wraps_and_handles_zero_slots() {
    TEST_ASSERT_EQUAL_UINT32(
        1U, device_platform::nextSlotRoundRobin(device_platform::SlotId(0U), 4U)
                .value());
    TEST_ASSERT_EQUAL_UINT32(
        0U, device_platform::nextSlotRoundRobin(device_platform::SlotId(3U), 4U)
                .value());
    TEST_ASSERT_EQUAL_UINT32(
        0U, device_platform::nextSlotRoundRobin(device_platform::SlotId(0U), 0U)
                .value());
}

void test_state_store_key_accepts_valid_key() {
    StateStoreKey key;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreKeyStatus::Success),
        static_cast<int>(StateStoreKey::create("manifest", key)));
    TEST_ASSERT_EQUAL_STRING("manifest", key.bytes().c_str());
    TEST_ASSERT_EQUAL_UINT32(8U, key.size());
}

void test_state_store_key_accepts_empty_key() {
    StateStoreKey key;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreKeyStatus::Success),
                          static_cast<int>(StateStoreKey::create("", key)));
    TEST_ASSERT_EQUAL_UINT32(0U, key.size());
}

void test_state_store_key_accepts_exact_max_length() {
    const std::string atMax(StateStoreKey::kMaxLength, 'k');
    StateStoreKey key;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreKeyStatus::Success),
                          static_cast<int>(StateStoreKey::create(atMax, key)));
    TEST_ASSERT_EQUAL_UINT32(StateStoreKey::kMaxLength, key.size());
}

void test_state_store_key_rejects_one_byte_over_max_length() {
    const std::string overMax(StateStoreKey::kMaxLength + 1U, 'k');
    StateStoreKey key;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreKeyStatus::TooLong),
        static_cast<int>(StateStoreKey::create(overMax, key)));
    // Bei Ablehnung bleibt `key` der unveraenderte Default (leer).
    TEST_ASSERT_EQUAL_UINT32(0U, key.size());
}

void test_state_store_key_is_binary_safe_with_embedded_nul() {
    const std::string withNul("a\0b", 3U);
    StateStoreKey key;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreKeyStatus::Success),
        static_cast<int>(StateStoreKey::create(withNul, key)));
    TEST_ASSERT_EQUAL_UINT32(3U, key.size());
    TEST_ASSERT_EQUAL_MEMORY(withNul.data(), key.bytes().data(), 3U);
}

// Der Simulator verwendet den Schluessel wertbasiert (nicht
// identitaetsbasiert): zwei unabhaengig erzeugte, aber bytegleiche Schluessel
// adressieren denselben Eintrag.
void test_state_store_key_comparison_and_deterministic_simulator_use() {
    TEST_ASSERT_TRUE(keyOrFail("same") == keyOrFail("same"));
    TEST_ASSERT_TRUE(keyOrFail("a") != keyOrFail("b"));
    TEST_ASSERT_TRUE(keyOrFail("a") < keyOrFail("b"));

    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(keyOrFail("same"), "value")));
    const auto result = store.read(keyOrFail("same"), kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_STRING("value", result.value.c_str());
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_write_and_read_round_trip);
    RUN_TEST(test_read_distinguishes_missing_key_from_error);
    RUN_TEST(test_read_enforces_caller_specific_maximum_length);
    RUN_TEST(test_write_fail_before_begin_leaves_store_unchanged);
    RUN_TEST(test_write_power_cut_before_commit_leaves_store_unchanged);
    RUN_TEST(
        test_write_power_cut_after_commit_before_return_yields_commit_outcome_unknown_and_value_is_durable);
    RUN_TEST(test_write_capacity_exceeded_leaves_store_unchanged);
    RUN_TEST(test_write_fault_applies_only_to_next_write);
    RUN_TEST(test_read_failure_injection_is_key_specific_and_recoverable);
    RUN_TEST(test_forced_not_found_overrides_existing_committed_value);
    RUN_TEST(test_corruption_injection_survives_restart);
    RUN_TEST(
        test_restart_clears_volatile_fault_injection_but_keeps_committed_data);
    RUN_TEST(
        test_secure_random_source_fills_requested_length_deterministically);
    RUN_TEST(
        test_secure_random_source_next_bytes_override_and_failure_injection);
    RUN_TEST(test_time_zone_resolver_known_unknown_and_failure);
    RUN_TEST(test_scan_filters_and_sorts_candidates_and_preserves_issues);
    RUN_TEST(
        test_scan_reports_not_found_for_every_slot_on_factory_empty_storage);
    RUN_TEST(test_scan_never_conflates_read_error_with_not_found);
    RUN_TEST(test_scan_preserves_capacity_error_on_read_limit);
    RUN_TEST(test_scan_preserves_crc_mismatch_issue);
    RUN_TEST(test_scan_break_ties_by_slot_id_ascending);
    RUN_TEST(test_scan_handles_max_version_value_without_overflow);
    RUN_TEST(test_next_slot_round_robin_wraps_and_handles_zero_slots);
    RUN_TEST(test_state_store_key_accepts_valid_key);
    RUN_TEST(test_state_store_key_accepts_empty_key);
    RUN_TEST(test_state_store_key_accepts_exact_max_length);
    RUN_TEST(test_state_store_key_rejects_one_byte_over_max_length);
    RUN_TEST(test_state_store_key_is_binary_safe_with_embedded_nul);
    RUN_TEST(test_state_store_key_comparison_and_deterministic_simulator_use);
    return UNITY_END();
}
