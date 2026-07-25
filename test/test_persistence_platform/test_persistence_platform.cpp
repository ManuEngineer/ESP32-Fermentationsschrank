#include <unity.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>
#include <string>
#include <vector>

#include "mock_secure_random_source.hpp"
#include "secure_random_source.hpp"
#include "simulated_persistent_state_store.hpp"
#include "state_store.hpp"
#include "state_store_key.hpp"
#include "storage_envelope.hpp"
#include "storage_slot_candidates.hpp"
#include "storage_types.hpp"

// Groessenpraefixierte globale Allokationszaehler fuer den
// Spitzenspeicher-Test: jede Allokation traegt ihre Groesse in einem
// ausrichtungssicheren Kopf, damit `delete` die Live-Bytes exakt zurueckrechnet
// (unabhaengig davon, ob sized-delete aufgerufen wird).
// Array-`new[]`/`delete[]` bleiben unveraendert und werden nicht mitgezaehlt.
namespace {
std::size_t gLiveAllocBytes = 0U;
std::size_t gPeakAllocBytes = 0U;
constexpr std::size_t kAllocHeader = alignof(std::max_align_t);
}  // namespace

void* operator new(std::size_t size) {
    void* raw = std::malloc(size + kAllocHeader);
    if (raw == nullptr) {
        throw std::bad_alloc();
    }
    *static_cast<std::size_t*>(raw) = size;
    gLiveAllocBytes += size;
    if (gLiveAllocBytes > gPeakAllocBytes) {
        gPeakAllocBytes = gLiveAllocBytes;
    }
    return static_cast<char*>(raw) + kAllocHeader;
}

void operator delete(void* ptr) noexcept {
    if (ptr == nullptr) {
        return;
    }
    void* raw = static_cast<char*>(ptr) - kAllocHeader;
    gLiveAllocBytes -= *static_cast<std::size_t*>(raw);
    std::free(raw);
}

void operator delete(void* ptr, std::size_t) noexcept { operator delete(ptr); }

namespace {

using device_platform::EnvelopeEncodeStatus;
using device_platform::RecordTypeId;
using device_platform::SlotIssueKind;
using device_platform::StateStoreKey;
using device_platform::StateStoreKeyStatus;
using device_platform::StateStoreReadStatus;
using device_platform::StateStoreWriteStatus;
using device_platform::StorageEnvelope;
using device_platform::StorageEpoch;
using device_platform_test_support::MockSecureRandomSource;
using device_platform_test_support::SimulatedPersistentStateStore;

StateStoreKey keyOrFail(const std::string& raw) {
    auto result = StateStoreKey::create(raw);
    TEST_ASSERT_TRUE(result.status == StateStoreKeyStatus::Success);
    TEST_ASSERT_TRUE(result.key.has_value());
    return *result.key;
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

// SlotCandidate traegt keine Payload - das Scan-Ergebnis waechst nur mit der
// Slotanzahl (kleine Metadaten), nie mit den Payloadgroessen.
static_assert(sizeof(device_platform::SlotCandidate) <= 32U,
              "SlotCandidate darf keine Payload einbetten.");

void test_write_and_read_round_trip() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_TRUE(store.write(keyOrFail("key"), "value") ==
                     StateStoreWriteStatus::Success);
    const auto result = store.read(keyOrFail("key"), kDefaultMaxBytes);
    TEST_ASSERT_TRUE(result.status == StateStoreReadStatus::Success);
    TEST_ASSERT_EQUAL_STRING("value", result.value.c_str());
}

void test_read_distinguishes_missing_key_from_error() {
    const SimulatedPersistentStateStore store;
    const auto result = store.read(keyOrFail("unknown"), kDefaultMaxBytes);
    TEST_ASSERT_TRUE(result.status == StateStoreReadStatus::NotFound);
    TEST_ASSERT_TRUE(result.value.empty());
}

void test_read_enforces_caller_specific_maximum_length() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_TRUE(store.write(keyOrFail("key"), "0123456789") ==
                     StateStoreWriteStatus::Success);

    const auto tooSmallLimit = store.read(keyOrFail("key"), 5U);
    TEST_ASSERT_TRUE(tooSmallLimit.status ==
                     StateStoreReadStatus::CapacityError);
    TEST_ASSERT_TRUE(tooSmallLimit.value.empty());

    const auto exactLimit = store.read(keyOrFail("key"), 10U);
    TEST_ASSERT_TRUE(exactLimit.status == StateStoreReadStatus::Success);
}

void test_write_fail_before_begin_leaves_store_unchanged() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_TRUE(store.write(keyOrFail("key"), "first") ==
                     StateStoreWriteStatus::Success);
    TEST_ASSERT_FALSE(store.hasPendingWriteForTesting());

    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::FailBeforeBegin);
    TEST_ASSERT_TRUE(store.write(keyOrFail("key"), "second") ==
                     StateStoreWriteStatus::WriteError);
    // Fehler vor Beginn: es entsteht kein Staging.
    TEST_ASSERT_FALSE(store.hasPendingWriteForTesting());

    const auto result = store.read(keyOrFail("key"), kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_STRING("first", result.value.c_str());
}

void test_write_power_cut_before_commit_leaves_store_unchanged() {
    SimulatedPersistentStateStore store;
    const auto key = keyOrFail("key");
    TEST_ASSERT_TRUE(store.write(key, "first") ==
                     StateStoreWriteStatus::Success);

    const std::string stagedBinary{
        'n', 'e', 'w', '\0', 'v', 'a', 'l', 'u', 'e', static_cast<char>(0xFF)};

    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::PowerCutBeforeCommit);
    TEST_ASSERT_TRUE(store.write(key, stagedBinary) ==
                     StateStoreWriteStatus::WriteError);
    // Der vollstaendige neue Wert ist gestagt, aber nicht committet.
    TEST_ASSERT_TRUE(store.hasPendingWriteForTesting());
    TEST_ASSERT_TRUE(store.pendingWriteMatchesForTesting(key, stagedBinary));
    // Vor dem Neustart bleibt ausschliesslich der alte committed Wert
    // sichtbar - das Staging existiert nur intern, nicht als Leseergebnis.
    TEST_ASSERT_EQUAL_STRING("first",
                             store.read(key, kDefaultMaxBytes).value.c_str());

    store.restart();
    // Der simulierte Neustart verwirft das Staging.
    TEST_ASSERT_FALSE(store.hasPendingWriteForTesting());

    const auto result = store.read(key, kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_STRING("first", result.value.c_str());
}

// `CommitOutcomeUnknown` ist bewusst kein `WriteError`: der neue Wert kann
// bereits dauerhaft gespeichert sein. Der Aufrufer erkennt dies nur durch
// Ruecklesen - genau das beweist dieser Test.
void test_write_power_cut_after_commit_before_return_yields_commit_outcome_unknown_and_value_is_durable() {
    SimulatedPersistentStateStore store;
    store.setNextWriteFault(SimulatedPersistentStateStore::WriteFault::
                                PowerCutAfterCommitBeforeReturn);
    TEST_ASSERT_TRUE(
        store.write(keyOrFail("key"), "committed_despite_unknown") ==
        StateStoreWriteStatus::CommitOutcomeUnknown);
    // Der gestagte Wert wurde bereits atomar committet; kein Staging bleibt
    // zurueck, obwohl der Aufrufer nur einen unbekannten Ausgang sah.
    TEST_ASSERT_FALSE(store.hasPendingWriteForTesting());

    // Der Aufrufer sah einen unbekannten Commit-Ausgang; erst das Ruecklesen
    // (auch nach einem simulierten Neustart) zeigt, dass der neue Wert
    // bereits dauerhaft gespeichert wurde.
    store.restart();
    const auto result = store.read(keyOrFail("key"), kDefaultMaxBytes);
    TEST_ASSERT_TRUE(result.status == StateStoreReadStatus::Success);
    TEST_ASSERT_EQUAL_STRING("committed_despite_unknown", result.value.c_str());
}

void test_write_capacity_exceeded_leaves_store_unchanged() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_TRUE(store.write(keyOrFail("key"), "first") ==
                     StateStoreWriteStatus::Success);

    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::CapacityExceeded);
    TEST_ASSERT_TRUE(store.write(keyOrFail("key"), "second") ==
                     StateStoreWriteStatus::CapacityError);
    // Kapazitaetsfehler: kein Commit, kein verbleibendes Staging.
    TEST_ASSERT_FALSE(store.hasPendingWriteForTesting());

    const auto result = store.read(keyOrFail("key"), kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_STRING("first", result.value.c_str());
}

void test_successful_write_stages_then_commits_and_clears_pending() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_TRUE(store.write(keyOrFail("key"), "value") ==
                     StateStoreWriteStatus::Success);
    // Erfolg committet vollstaendig; kein Staging bleibt zurueck.
    TEST_ASSERT_FALSE(store.hasPendingWriteForTesting());
}

// Unterschiedlich lange sowie binaere Werte mit eingebetteten NUL-Bytes
// ersetzen den alten Wert vollstaendig - nie ein Praefix, Suffix oder eine
// Mischung aus altem und neuem Wert, unabhaengig davon, ob der neue Wert
// kuerzer oder laenger als der alte ist.
void test_write_replaces_different_length_and_binary_values_without_residual_bytes() {
    SimulatedPersistentStateStore store;
    const auto key = keyOrFail("key");

    TEST_ASSERT_TRUE(store.write(key, "short") ==
                     StateStoreWriteStatus::Success);
    TEST_ASSERT_EQUAL_STRING("short",
                             store.read(key, kDefaultMaxBytes).value.c_str());

    const std::string longerBinary{'\x00', '\x01', '\x02', 'l', 'o',    'n',
                                   'g',    'e',    'r',    '-', 'b',    'i',
                                   'n',    'a',    'r',    'y', '-',    'v',
                                   'a',    'l',    'u',    'e', '\x00', '\xFF'};
    TEST_ASSERT_TRUE(store.write(key, longerBinary) ==
                     StateStoreWriteStatus::Success);
    const auto afterLonger = store.read(key, kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_UINT32(longerBinary.size(), afterLonger.value.size());
    TEST_ASSERT_EQUAL_MEMORY(longerBinary.data(), afterLonger.value.data(),
                             longerBinary.size());

    // Zurueck zu einem kuerzeren Wert: keine Restbytes des laengeren alten
    // Werts duerfen im gelesenen Ergebnis erscheinen.
    TEST_ASSERT_TRUE(store.write(key, "again-short") ==
                     StateStoreWriteStatus::Success);
    const auto afterShorter = store.read(key, kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_UINT32(11U, afterShorter.value.size());
    TEST_ASSERT_EQUAL_STRING("again-short", afterShorter.value.c_str());
}

void test_write_fault_applies_only_to_next_write() {
    SimulatedPersistentStateStore store;
    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::FailBeforeBegin);
    TEST_ASSERT_TRUE(store.write(keyOrFail("key"), "first") ==
                     StateStoreWriteStatus::WriteError);
    TEST_ASSERT_TRUE(store.write(keyOrFail("key"), "second") ==
                     StateStoreWriteStatus::Success);
    TEST_ASSERT_EQUAL_STRING(
        "second", store.read(keyOrFail("key"), kDefaultMaxBytes).value.c_str());
}

void test_read_failure_injection_is_key_specific_and_recoverable() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_TRUE(store.write(keyOrFail("a"), "valueA") ==
                     StateStoreWriteStatus::Success);
    TEST_ASSERT_TRUE(store.write(keyOrFail("b"), "valueB") ==
                     StateStoreWriteStatus::Success);

    store.injectReadFailure(keyOrFail("a"), true);
    TEST_ASSERT_TRUE(store.read(keyOrFail("a"), kDefaultMaxBytes).status ==
                     StateStoreReadStatus::ReadError);
    TEST_ASSERT_TRUE(store.read(keyOrFail("b"), kDefaultMaxBytes).status ==
                     StateStoreReadStatus::Success);

    store.injectReadFailure(keyOrFail("a"), false);
    TEST_ASSERT_TRUE(store.read(keyOrFail("a"), kDefaultMaxBytes).status ==
                     StateStoreReadStatus::Success);
}

void test_forced_not_found_overrides_existing_committed_value() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_TRUE(store.write(keyOrFail("key"), "value") ==
                     StateStoreWriteStatus::Success);

    store.forceNotFound(keyOrFail("key"), true);
    TEST_ASSERT_TRUE(store.read(keyOrFail("key"), kDefaultMaxBytes).status ==
                     StateStoreReadStatus::NotFound);

    store.forceNotFound(keyOrFail("key"), false);
    TEST_ASSERT_TRUE(store.read(keyOrFail("key"), kDefaultMaxBytes).status ==
                     StateStoreReadStatus::Success);
}

void test_corruption_injection_survives_restart() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_TRUE(store.write(keyOrFail("key"), "valid") ==
                     StateStoreWriteStatus::Success);

    store.injectCorruption(keyOrFail("key"),
                           std::string("\x00\x01corrupt", 9U));
    store.restart();

    const auto result = store.read(keyOrFail("key"), kDefaultMaxBytes);
    TEST_ASSERT_TRUE(result.status == StateStoreReadStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(9U, result.value.size());
    TEST_ASSERT_NOT_EQUAL(0, result.value.compare(std::string("valid")));
}

void test_restart_clears_volatile_fault_injection_but_keeps_committed_data() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_TRUE(store.write(keyOrFail("key"), "value") ==
                     StateStoreWriteStatus::Success);
    store.injectReadFailure(keyOrFail("key"), true);
    store.forceNotFound(keyOrFail("other"), true);

    store.restart();

    const auto result = store.read(keyOrFail("key"), kDefaultMaxBytes);
    TEST_ASSERT_TRUE(result.status == StateStoreReadStatus::Success);
    TEST_ASSERT_EQUAL_STRING("value", result.value.c_str());
    TEST_ASSERT_TRUE(store.read(keyOrFail("other"), kDefaultMaxBytes).status ==
                     StateStoreReadStatus::NotFound);
}

void test_restart_discards_pending_write_and_every_volatile_fault_flag() {
    SimulatedPersistentStateStore store;
    const auto key = keyOrFail("key");
    TEST_ASSERT_TRUE(store.write(key, "committed") ==
                     StateStoreWriteStatus::Success);

    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::PowerCutBeforeCommit);
    TEST_ASSERT_TRUE(store.write(key, "pending") ==
                     StateStoreWriteStatus::WriteError);
    TEST_ASSERT_TRUE(store.hasPendingWriteForTesting());

    store.injectReadFailure(key, true);
    store.forceNotFound(key, true);
    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::FailBeforeBegin);

    store.restart();

    TEST_ASSERT_FALSE(store.hasPendingWriteForTesting());
    const auto afterRestart = store.read(key, kDefaultMaxBytes);
    TEST_ASSERT_TRUE(afterRestart.status == StateStoreReadStatus::Success);
    TEST_ASSERT_EQUAL_STRING("committed", afterRestart.value.c_str());
    TEST_ASSERT_TRUE(store.write(key, "after-restart") ==
                     StateStoreWriteStatus::Success);
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

// Laenge 0 ist immer ein erfolgreicher No-op, unabhaengig von einem
// vorbereiteten Override: `nullptr` ist zulaessig, kein Zeiger wird
// dereferenziert, der Override bleibt fuer den naechsten Aufruf erhalten.
void test_secure_random_source_zero_length_fill_with_null_pointer_is_safe_and_does_not_consume_override() {
    MockSecureRandomSource source;
    source.setNextBytes(std::string("\x01\x02\x03\x04", 4U));
    TEST_ASSERT_TRUE(source.fill(nullptr, 0U));

    uint8_t buffer[4] = {0};
    TEST_ASSERT_TRUE(source.fill(buffer, sizeof(buffer)));
    const uint8_t expected[4] = {0x01U, 0x02U, 0x03U, 0x04U};
    TEST_ASSERT_EQUAL_MEMORY(expected, buffer, sizeof(buffer));
}

// `nullptr` mit positiver Laenge wird beobachtbar abgelehnt, bevor Override
// oder Generatorzustand beruehrt werden: ein vorbereiteter Override bleibt
// fuer den naechsten gueltigen Aufruf erhalten, und der seedbare
// Generatorzustand wird nicht fortgeschaltet.
void test_secure_random_source_rejects_null_pointer_with_positive_length_and_preserves_override() {
    MockSecureRandomSource source;
    source.setNextBytes(std::string("\x01\x02\x03\x04", 4U));
    TEST_ASSERT_FALSE(source.fill(nullptr, 4U));

    uint8_t buffer[4] = {0};
    TEST_ASSERT_TRUE(source.fill(buffer, sizeof(buffer)));
    const uint8_t expected[4] = {0x01U, 0x02U, 0x03U, 0x04U};
    TEST_ASSERT_EQUAL_MEMORY(expected, buffer, sizeof(buffer));
}

void test_secure_random_source_rejects_null_pointer_with_positive_length_and_does_not_advance_generator() {
    MockSecureRandomSource source(42U);
    TEST_ASSERT_FALSE(source.fill(nullptr, 4U));

    uint8_t afterRejected[16] = {0};
    TEST_ASSERT_TRUE(source.fill(afterRejected, sizeof(afterRejected)));

    MockSecureRandomSource neverRejected(42U);
    uint8_t reference[16] = {0};
    TEST_ASSERT_TRUE(neverRejected.fill(reference, sizeof(reference)));

    TEST_ASSERT_EQUAL_MEMORY(reference, afterRejected, sizeof(reference));
}

void test_scan_filters_and_sorts_candidates_and_preserves_issues() {
    SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(9U);
    const auto epoch = StorageEpoch(1U);

    TEST_ASSERT_TRUE(
        store.write(keyOrFail("slot0"),
                    encodedEnvelopeOrFail(recordType, 1U, epoch, 5U, "a")) ==
        StateStoreWriteStatus::Success);
    TEST_ASSERT_TRUE(
        store.write(keyOrFail("slot1"),
                    encodedEnvelopeOrFail(recordType, 1U, epoch, 9U, "b")) ==
        StateStoreWriteStatus::Success);
    // Falscher RecordType: technisch gueltig, aber nicht passend.
    TEST_ASSERT_TRUE(
        store.write(keyOrFail("slot2"),
                    encodedEnvelopeOrFail(RecordTypeId(99U), 1U, epoch, 20U,
                                          "wrong-type")) ==
        StateStoreWriteStatus::Success);
    // Falsche StorageEpoch: technisch gueltig, aber nicht passend.
    TEST_ASSERT_TRUE(
        store.write(keyOrFail("slot3"),
                    encodedEnvelopeOrFail(recordType, 1U, StorageEpoch(2U), 30U,
                                          "wrong-epoch")) ==
        StateStoreWriteStatus::Success);
    // Fehlender Slot.
    // ("slot4" wird nie geschrieben.)
    // Envelope strukturell ungueltig: richtige Laenge, falsches Magic.
    auto badMagic = encodedEnvelopeOrFail(recordType, 1U, epoch, 40U, "bad");
    badMagic[0] = 'X';
    TEST_ASSERT_TRUE(store.write(keyOrFail("slot5"), badMagic) ==
                     StateStoreWriteStatus::Success);

    const std::vector<StateStoreKey> slotKeys{
        keyOrFail("slot0"), keyOrFail("slot1"), keyOrFail("slot2"),
        keyOrFail("slot3"), keyOrFail("slot4"), keyOrFail("slot5"),
    };
    const auto scan = device_platform::scanTechnicalSlotCandidates(
        store, slotKeys, recordType, 1U, epoch, 4096U);

    TEST_ASSERT_EQUAL_UINT32(2U, scan.candidates.size());
    TEST_ASSERT_EQUAL_UINT64(9U, scan.candidates[0].versionValue);
    TEST_ASSERT_EQUAL_UINT32(1U, scan.candidates[0].slot.value());
    // Die Payload steht nicht mehr im Scan-Ergebnis, sondern wird gezielt und
    // vollstaendig neu validiert geladen.
    const auto loaded = device_platform::loadSlotPayload(
        store, keyOrFail("slot1"), recordType, 1U, epoch,
        scan.candidates[0].versionValue, 4096U);
    TEST_ASSERT_TRUE(loaded.status ==
                     device_platform::SlotPayloadLoadStatus::Success);
    TEST_ASSERT_EQUAL_STRING("b", loaded.payload.c_str());
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
    TEST_ASSERT_TRUE(
        store.write(keyOrFail("slot0"),
                    encodedEnvelopeOrFail(recordType, 1U, epoch, 1U, "ok")) ==
        StateStoreWriteStatus::Success);
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
    TEST_ASSERT_TRUE(store.write(keyOrFail("slot0"), oversized) ==
                     StateStoreWriteStatus::Success);

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
    TEST_ASSERT_TRUE(store.write(keyOrFail("slot0"), corrupted) ==
                     StateStoreWriteStatus::Success);
    TEST_ASSERT_TRUE(
        store.write(keyOrFail("slot1"),
                    encodedEnvelopeOrFail(recordType, 1U, epoch, 2U, "ok")) ==
        StateStoreWriteStatus::Success);

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
    TEST_ASSERT_TRUE(
        store.write(keyOrFail("slot0"),
                    encodedEnvelopeOrFail(recordType, 1U, epoch, 7U, "a")) ==
        StateStoreWriteStatus::Success);
    TEST_ASSERT_TRUE(
        store.write(keyOrFail("slot1"),
                    encodedEnvelopeOrFail(recordType, 1U, epoch, 7U, "b")) ==
        StateStoreWriteStatus::Success);
    TEST_ASSERT_TRUE(
        store.write(keyOrFail("slot2"),
                    encodedEnvelopeOrFail(recordType, 1U, epoch, 7U, "c")) ==
        StateStoreWriteStatus::Success);

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
    TEST_ASSERT_TRUE(store.write(keyOrFail("slot0"),
                                 encodedEnvelopeOrFail(
                                     recordType, 1U, epoch,
                                     std::numeric_limits<uint64_t>::max(),
                                     "max")) == StateStoreWriteStatus::Success);
    TEST_ASSERT_TRUE(store.write(keyOrFail("slot1"),
                                 encodedEnvelopeOrFail(
                                     recordType, 1U, epoch,
                                     std::numeric_limits<uint64_t>::max() - 1U,
                                     "near-max")) ==
                     StateStoreWriteStatus::Success);

    const std::vector<StateStoreKey> slotKeys{keyOrFail("slot0"),
                                              keyOrFail("slot1")};
    const auto scan = device_platform::scanTechnicalSlotCandidates(
        store, slotKeys, recordType, 1U, epoch, 4096U);

    TEST_ASSERT_EQUAL_UINT32(2U, scan.candidates.size());
    TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<uint64_t>::max(),
                             scan.candidates[0].versionValue);
    const auto loaded = device_platform::loadSlotPayload(
        store, keyOrFail("slot0"), recordType, 1U, epoch,
        scan.candidates[0].versionValue, 4096U);
    TEST_ASSERT_TRUE(loaded.status ==
                     device_platform::SlotPayloadLoadStatus::Success);
    TEST_ASSERT_EQUAL_STRING("max", loaded.payload.c_str());
    TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<uint64_t>::max() - 1U,
                             scan.candidates[1].versionValue);
}

void test_scan_peak_memory_is_independent_of_slot_count() {
    constexpr std::size_t payloadSize = 2048U;
    constexpr std::size_t maxEnvelope = 4096U;
    const auto recordType = RecordTypeId(9U);
    const auto epoch = StorageEpoch(1U);

    const auto measurePeak = [&](std::size_t slotCount) {
        SimulatedPersistentStateStore store;
        std::vector<StateStoreKey> slotKeys;
        for (std::size_t i = 0U; i < slotCount; ++i) {
            StorageEnvelope envelope;
            envelope.recordTypeId = recordType;
            envelope.schemaVersion = 1U;
            envelope.storageEpoch = epoch;
            envelope.versionValue = i + 1U;
            envelope.payload =
                std::string(payloadSize, static_cast<char>('a' + (i % 26U)));
            std::string encoded;
            TEST_ASSERT_TRUE(device_platform::encodeEnvelope(envelope, encoded,
                                                             maxEnvelope) ==
                             EnvelopeEncodeStatus::Success);
            const auto key = keyOrFail("slot" + std::to_string(i));
            TEST_ASSERT_TRUE(store.write(key, encoded) ==
                             StateStoreWriteStatus::Success);
            slotKeys.push_back(key);
        }
        const std::size_t baseline = gLiveAllocBytes;
        gPeakAllocBytes = gLiveAllocBytes;
        const auto scan = device_platform::scanTechnicalSlotCandidates(
            store, slotKeys, recordType, 1U, epoch, maxEnvelope);
        TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(slotCount),
                                 scan.candidates.size());
        return gPeakAllocBytes - baseline;
    };

    const std::size_t peakOneSlot = measurePeak(1U);
    const std::size_t peakManySlots = measurePeak(8U);

    // Der fruehere Scan hielt fuer jeden gueltigen Kandidaten die volle Payload
    // gleichzeitig (~slotCount * payloadSize). Jetzt liegt zu keinem Zeitpunkt
    // mehr als ein Recordpuffer im Speicher: der Spitzenbedarf bei acht Slots
    // bleibt unter zwei Payloadgroessen und unterscheidet sich vom
    // Ein-Slot-Fall um deutlich weniger als eine Payload.
    TEST_ASSERT_TRUE(peakManySlots < 2U * payloadSize);
    TEST_ASSERT_TRUE(peakManySlots <= peakOneSlot + payloadSize);
}

void test_load_slot_payload_reports_not_found_for_missing_slot() {
    SimulatedPersistentStateStore store;
    const auto result = device_platform::loadSlotPayload(
        store, keyOrFail("slot0"), RecordTypeId(9U), 1U, StorageEpoch(1U), 1U,
        kDefaultMaxBytes);
    TEST_ASSERT_TRUE(result.status ==
                     device_platform::SlotPayloadLoadStatus::NotFound);
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_load_slot_payload_rejects_record_identity_mismatch() {
    SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(9U);
    const auto epoch = StorageEpoch(1U);
    TEST_ASSERT_TRUE(store.write(keyOrFail("slot0"),
                                 encodedEnvelopeOrFail(recordType, 1U, epoch,
                                                       5U, "value")) ==
                     StateStoreWriteStatus::Success);

    const auto result = device_platform::loadSlotPayload(
        store, keyOrFail("slot0"), RecordTypeId(99U), 1U, epoch, 5U,
        kDefaultMaxBytes);
    TEST_ASSERT_TRUE(
        result.status ==
        device_platform::SlotPayloadLoadStatus::RecordIdentityMismatch);
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_load_slot_payload_rejects_version_value_changed_since_scan() {
    SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(9U);
    const auto epoch = StorageEpoch(1U);
    TEST_ASSERT_TRUE(
        store.write(keyOrFail("slot0"),
                    encodedEnvelopeOrFail(recordType, 1U, epoch, 5U, "old")) ==
        StateStoreWriteStatus::Success);
    // Der Slot wird nach dem Scan mit einem anderen versionValue
    // ueberschrieben.
    TEST_ASSERT_TRUE(
        store.write(keyOrFail("slot0"),
                    encodedEnvelopeOrFail(recordType, 1U, epoch, 9U, "new")) ==
        StateStoreWriteStatus::Success);

    const auto result = device_platform::loadSlotPayload(
        store, keyOrFail("slot0"), recordType, 1U, epoch, 5U, kDefaultMaxBytes);
    TEST_ASSERT_TRUE(
        result.status ==
        device_platform::SlotPayloadLoadStatus::VersionValueMismatch);
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_load_slot_payload_rejects_crc_corruption() {
    SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(9U);
    const auto epoch = StorageEpoch(1U);
    auto tampered = encodedEnvelopeOrFail(recordType, 1U, epoch, 5U, "value");
    // Ein Payloadbyte kippen, ohne den CRC anzupassen.
    tampered.back() = static_cast<char>(tampered.back() ^ 0x01);
    TEST_ASSERT_TRUE(store.write(keyOrFail("slot0"), tampered) ==
                     StateStoreWriteStatus::Success);

    const auto result = device_platform::loadSlotPayload(
        store, keyOrFail("slot0"), recordType, 1U, epoch, 5U, kDefaultMaxBytes);
    TEST_ASSERT_TRUE(result.status ==
                     device_platform::SlotPayloadLoadStatus::InvalidEnvelope);
    TEST_ASSERT_TRUE(result.payload.empty());
}

void test_next_slot_round_robin_wraps_and_handles_zero_slots() {
    using device_platform::NextSlotStatus;

    const auto wrap1 =
        device_platform::nextSlotRoundRobin(device_platform::SlotId(0U), 4U);
    TEST_ASSERT_TRUE(wrap1.status == NextSlotStatus::Success);
    TEST_ASSERT_TRUE(wrap1.slot.has_value());
    TEST_ASSERT_EQUAL_UINT32(1U, wrap1.slot->value());

    const auto wrap2 =
        device_platform::nextSlotRoundRobin(device_platform::SlotId(3U), 4U);
    TEST_ASSERT_TRUE(wrap2.status == NextSlotStatus::Success);
    TEST_ASSERT_TRUE(wrap2.slot.has_value());
    TEST_ASSERT_EQUAL_UINT32(0U, wrap2.slot->value());

    // `slotCount == 0` ist ungueltig und liefert keinen Slot - insbesondere
    // nicht den scheinbar gueltigen `SlotId(0)`.
    const auto zeroSlots =
        device_platform::nextSlotRoundRobin(device_platform::SlotId(0U), 0U);
    TEST_ASSERT_TRUE(zeroSlots.status == NextSlotStatus::InvalidSlotCount);
    TEST_ASSERT_FALSE(zeroSlots.slot.has_value());
}

// Bei maximalem gueltigem `slotCount` darf der letzte gueltige
// `lastWrittenSlot` (`slotCount - 1`) `lastWrittenSlot.value() + 1` nicht
// ueberlaufen lassen (relevant auf 32-Bit-`size_t`-Zielplattformen wie dem
// ESP32; auf dem 64-Bit-Testhost waere ein naiver Cast zufaellig unauffaellig
// gewesen).
void test_next_slot_round_robin_does_not_overflow_near_uint32_max() {
    using device_platform::NextSlotStatus;

    const auto slotCount =
        static_cast<std::size_t>(std::numeric_limits<uint32_t>::max());

    // Letzter gueltiger Index (`slotCount - 1`); naechster Slot rotiert ohne
    // Ueberlauf von `lastWrittenSlot.value() + 1` zu 0.
    const auto lastValid =
        device_platform::SlotId(std::numeric_limits<uint32_t>::max() - 1U);
    const auto wrap = device_platform::nextSlotRoundRobin(lastValid, slotCount);
    TEST_ASSERT_TRUE(wrap.status == NextSlotStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(0U, wrap.slot->value());

    // Ein Index zwei unter dem Maximum rotiert regulaer zu `Maximum - 1`.
    const auto lastValidMinusOne =
        device_platform::SlotId(std::numeric_limits<uint32_t>::max() - 2U);
    const auto step =
        device_platform::nextSlotRoundRobin(lastValidMinusOne, slotCount);
    TEST_ASSERT_TRUE(step.status == NextSlotStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(std::numeric_limits<uint32_t>::max() - 1U,
                             step.slot->value());
}

// Ein `lastWrittenSlot` ausserhalb des gueltigen Bereichs (`>= slotCount`,
// z. B. aus korruptem Speicher) wird als `InvalidLastSlot` abgelehnt statt
// still per Modulo normalisiert - und ist von `InvalidSlotCount`
// unterscheidbar.
void test_next_slot_round_robin_rejects_last_slot_out_of_range() {
    using device_platform::NextSlotStatus;

    // Gleich der Slotanzahl ist bereits ausserhalb (gueltig sind 0..count-1).
    const auto equalToCount =
        device_platform::nextSlotRoundRobin(device_platform::SlotId(4U), 4U);
    TEST_ASSERT_TRUE(equalToCount.status == NextSlotStatus::InvalidLastSlot);
    TEST_ASSERT_FALSE(equalToCount.slot.has_value());

    // Deutlich ausserhalb.
    const auto farOutside = device_platform::nextSlotRoundRobin(
        device_platform::SlotId(std::numeric_limits<uint32_t>::max()), 4U);
    TEST_ASSERT_TRUE(farOutside.status == NextSlotStatus::InvalidLastSlot);
    TEST_ASSERT_FALSE(farOutside.slot.has_value());

    // Von einer ungueltigen Slotanzahl unterscheidbar.
    const auto invalidCount =
        device_platform::nextSlotRoundRobin(device_platform::SlotId(0U), 0U);
    TEST_ASSERT_TRUE(invalidCount.status == NextSlotStatus::InvalidSlotCount);
    TEST_ASSERT_TRUE(equalToCount.status != invalidCount.status);
}

// Ein technisch nicht darstellbares `slotCount` (> UINT32_MAX) wird als
// `InvalidSlotCount` abgelehnt statt einen scheinbar gueltigen Slot 0 zu
// liefern.
void test_next_slot_round_robin_rejects_slot_count_above_uint32_max() {
    using device_platform::NextSlotStatus;

    const std::size_t tooLarge =
        static_cast<std::size_t>(std::numeric_limits<uint32_t>::max()) + 1U;
    const auto result = device_platform::nextSlotRoundRobin(
        device_platform::SlotId(0U), tooLarge);
    TEST_ASSERT_TRUE(result.status == NextSlotStatus::InvalidSlotCount);
    TEST_ASSERT_FALSE(result.slot.has_value());
}

// Ein erfolgreicher Slot 0 (gueltige Rotation) und ein ungueltiger `slotCount`
// sind durch `status` eindeutig unterscheidbar, obwohl beide als Slotwert 0
// naheliegen.
void test_next_slot_round_robin_success_at_slot_zero_is_not_confusable_with_invalid_slot_count() {
    using device_platform::NextSlotStatus;

    const auto validRotationToZero =
        device_platform::nextSlotRoundRobin(device_platform::SlotId(3U), 4U);
    const auto invalidSlotCount =
        device_platform::nextSlotRoundRobin(device_platform::SlotId(0U), 0U);

    TEST_ASSERT_TRUE(validRotationToZero.status == NextSlotStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(0U, validRotationToZero.slot->value());
    TEST_ASSERT_TRUE(invalidSlotCount.status ==
                     NextSlotStatus::InvalidSlotCount);
    TEST_ASSERT_FALSE(invalidSlotCount.slot.has_value());
    TEST_ASSERT_TRUE(validRotationToZero.status != invalidSlotCount.status);
}

void test_state_store_key_accepts_valid_key() {
    const auto result = StateStoreKey::create("manifest");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreKeyStatus::Success),
                          static_cast<int>(result.status));
    TEST_ASSERT_TRUE(result.key.has_value());
    TEST_ASSERT_EQUAL_STRING("manifest", result.key->bytes().c_str());
    TEST_ASSERT_EQUAL_UINT32(8U, result.key->size());
}

void test_state_store_key_accepts_single_byte_key() {
    const auto result = StateStoreKey::create("k");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreKeyStatus::Success),
                          static_cast<int>(result.status));
    TEST_ASSERT_TRUE(result.key.has_value());
    TEST_ASSERT_EQUAL_UINT32(1U, result.key->size());
}

// Ein leerer Schluessel ist kein gueltiger technischer Schluessel: `create()`
// liefert keinen Wert, ein leerer Default-Zustand ist ohnehin nicht
// erzeugbar (siehe `static_assert` in state_store_key.hpp).
void test_state_store_key_rejects_empty_key() {
    const auto result = StateStoreKey::create("");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreKeyStatus::Empty),
                          static_cast<int>(result.status));
    TEST_ASSERT_FALSE(result.key.has_value());
}

void test_state_store_key_accepts_exact_max_length() {
    const std::string atMax(StateStoreKey::kMaxLength, 'k');
    const auto result = StateStoreKey::create(atMax);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreKeyStatus::Success),
                          static_cast<int>(result.status));
    TEST_ASSERT_TRUE(result.key.has_value());
    TEST_ASSERT_EQUAL_UINT32(StateStoreKey::kMaxLength, result.key->size());
}

void test_state_store_key_rejects_one_byte_over_max_length() {
    const std::string overMax(StateStoreKey::kMaxLength + 1U, 'k');
    const auto result = StateStoreKey::create(overMax);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreKeyStatus::TooLong),
                          static_cast<int>(result.status));
    // Fehlgeschlagene Erzeugung liefert keinen verwendbaren Schluessel.
    TEST_ASSERT_FALSE(result.key.has_value());
}

// Lehnt `raw` mit `InvalidCharacter` ab und liefert keinen Schluessel.
void assertRejectedAsInvalidCharacter(const std::string& raw) {
    const auto result = StateStoreKey::create(raw);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreKeyStatus::InvalidCharacter),
        static_cast<int>(result.status));
    TEST_ASSERT_FALSE(result.key.has_value());
}

// Alle erlaubten Zeichensatzgrenzen (`A`,`Z`,`a`,`z`,`0`,`9`,`_`,`.`,`-`)
// werden akzeptiert.
void test_state_store_key_accepts_charset_boundary_characters() {
    const auto result = StateStoreKey::create("AZaz09_.-");
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreKeyStatus::Success),
                          static_cast<int>(result.status));
    TEST_ASSERT_TRUE(result.key.has_value());
    TEST_ASSERT_EQUAL_UINT32(9U, result.key->size());
}

// Direkt an die erlaubten Bereiche angrenzende Zeichen sind der jeweils erste
// ungueltige Fall: '/' (vor '0'), ':' (nach '9'), '@' (vor 'A'), '[' (nach
// 'Z'), '`' (vor 'a'), '{' (nach 'z'); ebenso jedes Byte ab 0x80.
void test_state_store_key_rejects_characters_just_outside_allowed_ranges() {
    const char invalids[] = {'/', ':', '@', '[', '`', '{'};
    for (const char invalid : invalids) {
        assertRejectedAsInvalidCharacter(std::string("a") + invalid + "b");
    }
    assertRejectedAsInvalidCharacter(std::string(1U, static_cast<char>(0x80U)));
    assertRejectedAsInvalidCharacter(std::string(1U, static_cast<char>(0xFFU)));
}

// NUL ist unzulaessig (kein binaersicherer Schluesselraum): der Schluessel wird
// abgelehnt statt still aufgenommen.
void test_state_store_key_rejects_embedded_nul() {
    assertRejectedAsInvalidCharacter(std::string("a\0b", 3U));
}

void test_state_store_key_rejects_space() {
    assertRejectedAsInvalidCharacter("a b");
}

void test_state_store_key_rejects_path_separators() {
    assertRejectedAsInvalidCharacter("a/b");
    assertRejectedAsInvalidCharacter("a\\b");
}

// Deterministische Ordnung nach unsigned Bytewert innerhalb des erlaubten
// Zeichensatzes: '-' (0x2D) < '.' (0x2E) < '0' (0x30) < 'A' (0x41) <
// '_' (0x5F) < 'a' (0x61).
void test_state_store_key_ordering_follows_unsigned_byte_value() {
    TEST_ASSERT_TRUE(keyOrFail("-") < keyOrFail("."));
    TEST_ASSERT_TRUE(keyOrFail(".") < keyOrFail("0"));
    TEST_ASSERT_TRUE(keyOrFail("0") < keyOrFail("A"));
    TEST_ASSERT_TRUE(keyOrFail("A") < keyOrFail("_"));
    TEST_ASSERT_TRUE(keyOrFail("_") < keyOrFail("a"));
}

// Der Simulator verwendet den Schluessel wertbasiert (nicht
// identitaetsbasiert): zwei unabhaengig erzeugte, aber bytegleiche Schluessel
// adressieren denselben Eintrag.
void test_state_store_key_comparison_and_deterministic_simulator_use() {
    TEST_ASSERT_TRUE(keyOrFail("same") == keyOrFail("same"));
    TEST_ASSERT_TRUE(keyOrFail("a") != keyOrFail("b"));
    TEST_ASSERT_TRUE(keyOrFail("a") < keyOrFail("b"));

    SimulatedPersistentStateStore store;
    TEST_ASSERT_TRUE(store.write(keyOrFail("same"), "value") ==
                     StateStoreWriteStatus::Success);
    const auto result = store.read(keyOrFail("same"), kDefaultMaxBytes);
    TEST_ASSERT_TRUE(result.status == StateStoreReadStatus::Success);
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
    RUN_TEST(test_successful_write_stages_then_commits_and_clears_pending);
    RUN_TEST(
        test_write_replaces_different_length_and_binary_values_without_residual_bytes);
    RUN_TEST(test_write_fault_applies_only_to_next_write);
    RUN_TEST(test_read_failure_injection_is_key_specific_and_recoverable);
    RUN_TEST(test_forced_not_found_overrides_existing_committed_value);
    RUN_TEST(test_corruption_injection_survives_restart);
    RUN_TEST(
        test_restart_clears_volatile_fault_injection_but_keeps_committed_data);
    RUN_TEST(test_restart_discards_pending_write_and_every_volatile_fault_flag);
    RUN_TEST(
        test_secure_random_source_fills_requested_length_deterministically);
    RUN_TEST(
        test_secure_random_source_next_bytes_override_and_failure_injection);
    RUN_TEST(
        test_secure_random_source_zero_length_fill_with_null_pointer_is_safe_and_does_not_consume_override);
    RUN_TEST(
        test_secure_random_source_rejects_null_pointer_with_positive_length_and_preserves_override);
    RUN_TEST(
        test_secure_random_source_rejects_null_pointer_with_positive_length_and_does_not_advance_generator);
    RUN_TEST(test_scan_filters_and_sorts_candidates_and_preserves_issues);
    RUN_TEST(
        test_scan_reports_not_found_for_every_slot_on_factory_empty_storage);
    RUN_TEST(test_scan_never_conflates_read_error_with_not_found);
    RUN_TEST(test_scan_preserves_capacity_error_on_read_limit);
    RUN_TEST(test_scan_preserves_crc_mismatch_issue);
    RUN_TEST(test_scan_break_ties_by_slot_id_ascending);
    RUN_TEST(test_scan_handles_max_version_value_without_overflow);
    RUN_TEST(test_scan_peak_memory_is_independent_of_slot_count);
    RUN_TEST(test_load_slot_payload_reports_not_found_for_missing_slot);
    RUN_TEST(test_load_slot_payload_rejects_record_identity_mismatch);
    RUN_TEST(test_load_slot_payload_rejects_version_value_changed_since_scan);
    RUN_TEST(test_load_slot_payload_rejects_crc_corruption);
    RUN_TEST(test_next_slot_round_robin_wraps_and_handles_zero_slots);
    RUN_TEST(test_next_slot_round_robin_does_not_overflow_near_uint32_max);
    RUN_TEST(test_next_slot_round_robin_rejects_last_slot_out_of_range);
    RUN_TEST(test_next_slot_round_robin_rejects_slot_count_above_uint32_max);
    RUN_TEST(
        test_next_slot_round_robin_success_at_slot_zero_is_not_confusable_with_invalid_slot_count);
    RUN_TEST(test_state_store_key_accepts_valid_key);
    RUN_TEST(test_state_store_key_accepts_single_byte_key);
    RUN_TEST(test_state_store_key_rejects_empty_key);
    RUN_TEST(test_state_store_key_accepts_exact_max_length);
    RUN_TEST(test_state_store_key_rejects_one_byte_over_max_length);
    RUN_TEST(test_state_store_key_accepts_charset_boundary_characters);
    RUN_TEST(
        test_state_store_key_rejects_characters_just_outside_allowed_ranges);
    RUN_TEST(test_state_store_key_rejects_embedded_nul);
    RUN_TEST(test_state_store_key_rejects_space);
    RUN_TEST(test_state_store_key_rejects_path_separators);
    RUN_TEST(test_state_store_key_ordering_follows_unsigned_byte_value);
    RUN_TEST(test_state_store_key_comparison_and_deterministic_simulator_use);
    return UNITY_END();
}
