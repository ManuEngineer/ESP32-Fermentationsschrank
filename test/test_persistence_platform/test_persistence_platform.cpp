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
#include "storage_envelope.hpp"
#include "storage_slot_candidates.hpp"
#include "storage_types.hpp"
#include "time_zone_resolver.hpp"

namespace {

using device_platform::EnvelopeEncodeStatus;
using device_platform::RecordTypeId;
using device_platform::StateStoreStatus;
using device_platform::StorageEnvelope;
using device_platform::StorageEpoch;
using device_platform::TimeZoneResolutionStatus;
using device_platform_test_support::MockSecureRandomSource;
using device_platform_test_support::MockTimeZoneResolver;
using device_platform_test_support::SimulatedPersistentStateStore;

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
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(store.write("key", "value")));
    const auto result = store.read("key", kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_STRING("value", result.value.c_str());
}

void test_read_distinguishes_missing_key_from_error() {
    const SimulatedPersistentStateStore store;
    const auto result = store.read("unknown", kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::NotFound),
                          static_cast<int>(result.status));
    TEST_ASSERT_TRUE(result.value.empty());
}

void test_read_enforces_caller_specific_maximum_length() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(store.write("key", "0123456789")));

    const auto tooSmallLimit = store.read("key", 5U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::CapacityError),
                          static_cast<int>(tooSmallLimit.status));
    TEST_ASSERT_TRUE(tooSmallLimit.value.empty());

    const auto exactLimit = store.read("key", 10U);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(exactLimit.status));
}

void test_write_fail_before_begin_leaves_store_unchanged() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(store.write("key", "first")));

    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::FailBeforeBegin);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::WriteError),
                          static_cast<int>(store.write("key", "second")));

    const auto result = store.read("key", kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_STRING("first", result.value.c_str());
}

void test_write_power_cut_before_commit_leaves_store_unchanged() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(store.write("key", "first")));

    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::PowerCutBeforeCommit);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::WriteError),
                          static_cast<int>(store.write("key", "second")));
    store.restart();

    const auto result = store.read("key", kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_STRING("first", result.value.c_str());
}

void test_write_power_cut_after_commit_before_return_is_durable() {
    SimulatedPersistentStateStore store;
    store.setNextWriteFault(SimulatedPersistentStateStore::WriteFault::
                                PowerCutAfterCommitBeforeReturn);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::WriteError),
        static_cast<int>(store.write("key", "committed_despite_error")));

    // Der Aufrufer sah einen Fehler, aber der Wert ist bereits dauerhaft
    // committed und muss einen Neustart ueberleben.
    store.restart();
    const auto result = store.read("key", kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_STRING("committed_despite_error", result.value.c_str());
}

void test_write_capacity_exceeded_leaves_store_unchanged() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(store.write("key", "first")));

    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::CapacityExceeded);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::CapacityError),
                          static_cast<int>(store.write("key", "second")));

    const auto result = store.read("key", kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_STRING("first", result.value.c_str());
}

void test_write_fault_applies_only_to_next_write() {
    SimulatedPersistentStateStore store;
    store.setNextWriteFault(
        SimulatedPersistentStateStore::WriteFault::FailBeforeBegin);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::WriteError),
                          static_cast<int>(store.write("key", "first")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(store.write("key", "second")));
    TEST_ASSERT_EQUAL_STRING("second",
                             store.read("key", kDefaultMaxBytes).value.c_str());
}

void test_read_failure_injection_is_key_specific_and_recoverable() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(store.write("a", "valueA")));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(store.write("b", "valueB")));

    store.injectReadFailure("a", true);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::ReadError),
        static_cast<int>(store.read("a", kDefaultMaxBytes).status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.read("b", kDefaultMaxBytes).status));

    store.injectReadFailure("a", false);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.read("a", kDefaultMaxBytes).status));
}

void test_forced_not_found_overrides_existing_committed_value() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(store.write("key", "value")));

    store.forceNotFound("key", true);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::NotFound),
        static_cast<int>(store.read("key", kDefaultMaxBytes).status));

    store.forceNotFound("key", false);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.read("key", kDefaultMaxBytes).status));
}

void test_corruption_injection_survives_restart() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(store.write("key", "valid")));

    store.injectCorruption("key", std::string("\x00\x01corrupt", 9U));
    store.restart();

    const auto result = store.read("key", kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT32(9U, result.value.size());
    TEST_ASSERT_NOT_EQUAL(0, result.value.compare(std::string("valid")));
}

void test_restart_clears_volatile_fault_injection_but_keeps_committed_data() {
    SimulatedPersistentStateStore store;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(store.write("key", "value")));
    store.injectReadFailure("key", true);
    store.forceNotFound("other", true);

    store.restart();

    const auto result = store.read("key", kDefaultMaxBytes);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(StateStoreStatus::Success),
                          static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_STRING("value", result.value.c_str());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::NotFound),
        static_cast<int>(store.read("other", kDefaultMaxBytes).status));
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

void test_technical_candidates_filtered_and_sorted_descending() {
    SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(9U);
    const auto epoch = StorageEpoch(1U);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            "slot0", encodedEnvelopeOrFail(recordType, 1U, epoch, 5U, "a"))));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            "slot1", encodedEnvelopeOrFail(recordType, 1U, epoch, 9U, "b"))));
    // Falscher RecordType: muss ausgelassen werden.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            "slot2", encodedEnvelopeOrFail(RecordTypeId(99U), 1U, epoch, 20U,
                                           "wrong-type"))));
    // Falsche StorageEpoch: muss ausgelassen werden.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            "slot3", encodedEnvelopeOrFail(recordType, 1U, StorageEpoch(2U),
                                           30U, "wrong-epoch"))));
    // Fehlender Slot.
    // ("slot4" wird nie geschrieben.)
    // Korrupte Bytes: muss ausgelassen werden.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write("slot5", "not-an-envelope")));

    const std::vector<std::string> slotKeys{"slot0", "slot1", "slot2",
                                            "slot3", "slot4", "slot5"};
    const auto candidates = device_platform::technicalCandidatesDescending(
        store, slotKeys, recordType, 1U, epoch, 4096U);

    TEST_ASSERT_EQUAL_UINT32(2U, candidates.size());
    TEST_ASSERT_EQUAL_UINT64(9U, candidates[0].versionValue);
    TEST_ASSERT_EQUAL_UINT32(1U, candidates[0].slot.value());
    TEST_ASSERT_EQUAL_STRING("b", candidates[0].payload.c_str());
    TEST_ASSERT_EQUAL_UINT64(5U, candidates[1].versionValue);
    TEST_ASSERT_EQUAL_UINT32(0U, candidates[1].slot.value());
}

void test_technical_candidates_break_ties_by_slot_id_ascending() {
    SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(9U);
    const auto epoch = StorageEpoch(1U);

    // Gleicher versionValue in mehreren Slots: Sortierung muss deterministisch
    // sein (std::sort ist nicht stabil), Tiebreak per aufsteigender Slot-ID.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            "slot0", encodedEnvelopeOrFail(recordType, 1U, epoch, 7U, "a"))));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            "slot1", encodedEnvelopeOrFail(recordType, 1U, epoch, 7U, "b"))));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            "slot2", encodedEnvelopeOrFail(recordType, 1U, epoch, 7U, "c"))));

    const std::vector<std::string> slotKeys{"slot0", "slot1", "slot2"};
    const auto candidates = device_platform::technicalCandidatesDescending(
        store, slotKeys, recordType, 1U, epoch, 4096U);

    TEST_ASSERT_EQUAL_UINT32(3U, candidates.size());
    TEST_ASSERT_EQUAL_UINT32(0U, candidates[0].slot.value());
    TEST_ASSERT_EQUAL_UINT32(1U, candidates[1].slot.value());
    TEST_ASSERT_EQUAL_UINT32(2U, candidates[2].slot.value());
}

void test_technical_candidates_handle_max_version_value_without_overflow() {
    SimulatedPersistentStateStore store;
    const auto recordType = RecordTypeId(9U);
    const auto epoch = StorageEpoch(1U);

    // Sequenzueberlauf-Schutz beim Erhoehen von versionValue gehoert zur
    // schreibenden Anwendungsschicht (#55/#56); #54 muss lediglich den
    // vollen 64-Bit-Wertebereich verlustfrei kodieren, dekodieren und
    // korrekt einsortieren.
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            "slot0", encodedEnvelopeOrFail(recordType, 1U, epoch,
                                           std::numeric_limits<uint64_t>::max(),
                                           "max"))));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(StateStoreStatus::Success),
        static_cast<int>(store.write(
            "slot1",
            encodedEnvelopeOrFail(recordType, 1U, epoch,
                                  std::numeric_limits<uint64_t>::max() - 1U,
                                  "near-max"))));

    const std::vector<std::string> slotKeys{"slot0", "slot1"};
    const auto candidates = device_platform::technicalCandidatesDescending(
        store, slotKeys, recordType, 1U, epoch, 4096U);

    TEST_ASSERT_EQUAL_UINT32(2U, candidates.size());
    TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<uint64_t>::max(),
                             candidates[0].versionValue);
    TEST_ASSERT_EQUAL_STRING("max", candidates[0].payload.c_str());
    TEST_ASSERT_EQUAL_UINT64(std::numeric_limits<uint64_t>::max() - 1U,
                             candidates[1].versionValue);
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

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_write_and_read_round_trip);
    RUN_TEST(test_read_distinguishes_missing_key_from_error);
    RUN_TEST(test_read_enforces_caller_specific_maximum_length);
    RUN_TEST(test_write_fail_before_begin_leaves_store_unchanged);
    RUN_TEST(test_write_power_cut_before_commit_leaves_store_unchanged);
    RUN_TEST(test_write_power_cut_after_commit_before_return_is_durable);
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
    RUN_TEST(test_technical_candidates_filtered_and_sorted_descending);
    RUN_TEST(test_technical_candidates_break_ties_by_slot_id_ascending);
    RUN_TEST(
        test_technical_candidates_handle_max_version_value_without_overflow);
    RUN_TEST(test_next_slot_round_robin_wraps_and_handles_zero_slots);
    return UNITY_END();
}
