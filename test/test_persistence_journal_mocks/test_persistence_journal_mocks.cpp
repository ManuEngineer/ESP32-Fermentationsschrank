#include <unity.h>

#include "mock_event_journal.hpp"
#include "mock_state_store.hpp"

void test_state_store_round_trips_a_value() {
    device_platform::MockStateStore store;

    TEST_ASSERT_TRUE(store.write("example_key", "0.5"));

    const auto result = store.read("example_key");
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_STRING("0.5", result.value.c_str());
}

void test_state_store_distinguishes_missing_key_from_error() {
    const device_platform::MockStateStore store;

    const auto result = store.read("unknown_key");
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::NotFound),
        static_cast<int>(result.status));
    TEST_ASSERT_TRUE(result.value.empty());
}

void test_state_store_write_failure_is_injectable_and_preserves_old_value() {
    device_platform::MockStateStore store;
    TEST_ASSERT_TRUE(store.write("key", "first"));

    store.injectWriteFailure(true);
    TEST_ASSERT_FALSE(store.write("key", "second"));

    const auto result = store.read("key");
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_STRING("first", result.value.c_str());
}

void test_state_store_read_failure_is_injectable() {
    device_platform::MockStateStore store;
    TEST_ASSERT_TRUE(store.write("key", "value"));

    store.injectReadFailure(true);
    const auto failedResult = store.read("key");
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Error),
        static_cast<int>(failedResult.status));
    TEST_ASSERT_TRUE(failedResult.value.empty());

    store.injectReadFailure(false);
    const auto recoveredResult = store.read("key");
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(recoveredResult.status));
    TEST_ASSERT_EQUAL_STRING("value", recoveredResult.value.c_str());
}

void test_state_store_recovers_after_fault_injection_is_cleared() {
    device_platform::MockStateStore store;

    store.injectWriteFailure(true);
    TEST_ASSERT_FALSE(store.write("key", "value"));

    store.injectWriteFailure(false);
    TEST_ASSERT_TRUE(store.write("key", "value"));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::StateStoreReadStatus::Success),
        static_cast<int>(store.read("key").status));
}

void test_event_journal_records_entries_in_order() {
    device_platform::MockEventJournal journal;

    TEST_ASSERT_TRUE(journal.record(100, "boot"));
    TEST_ASSERT_TRUE(journal.record(200, "standby"));

    const auto& entries = journal.entries();
    TEST_ASSERT_EQUAL_UINT32(2U, entries.size());
    TEST_ASSERT_EQUAL_UINT64(100U, entries[0].monotonicMillis);
    TEST_ASSERT_EQUAL_STRING("boot", entries[0].message.c_str());
    TEST_ASSERT_EQUAL_UINT64(200U, entries[1].monotonicMillis);
    TEST_ASSERT_EQUAL_STRING("standby", entries[1].message.c_str());
}

void test_event_journal_is_bounded_and_drops_oldest_entries() {
    device_platform::MockEventJournal journal;

    const std::size_t entriesToWrite =
        device_platform::MockEventJournal::kMaxEntries + 10;
    for (std::size_t i = 0; i < entriesToWrite; ++i) {
        TEST_ASSERT_TRUE(
            journal.record(static_cast<uint64_t>(i), "event"));
    }

    const auto& entries = journal.entries();
    TEST_ASSERT_EQUAL_UINT32(
        static_cast<unsigned>(device_platform::MockEventJournal::kMaxEntries),
        entries.size());
    // Die aeltesten zehn Eintraege (Zeitstempel 0..9) muessen verworfen sein.
    TEST_ASSERT_EQUAL_UINT64(10U, entries.front().monotonicMillis);
}

void test_event_journal_write_failure_is_reported_and_preserves_entries() {
    device_platform::MockEventJournal journal;
    TEST_ASSERT_TRUE(journal.record(100, "boot"));

    journal.injectWriteFailure(true);
    TEST_ASSERT_FALSE(journal.record(200, "must_not_be_stored"));

    const auto& entries = journal.entries();
    TEST_ASSERT_EQUAL_UINT32(1U, entries.size());
    TEST_ASSERT_EQUAL_UINT64(100U, entries.front().monotonicMillis);
    TEST_ASSERT_EQUAL_STRING("boot", entries.front().message.c_str());

    journal.injectWriteFailure(false);
    TEST_ASSERT_TRUE(journal.record(300, "recovered"));
    TEST_ASSERT_EQUAL_UINT32(2U, journal.entries().size());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_state_store_round_trips_a_value);
    RUN_TEST(test_state_store_distinguishes_missing_key_from_error);
    RUN_TEST(
        test_state_store_write_failure_is_injectable_and_preserves_old_value);
    RUN_TEST(test_state_store_read_failure_is_injectable);
    RUN_TEST(test_state_store_recovers_after_fault_injection_is_cleared);
    RUN_TEST(test_event_journal_records_entries_in_order);
    RUN_TEST(test_event_journal_is_bounded_and_drops_oldest_entries);
    RUN_TEST(
        test_event_journal_write_failure_is_reported_and_preserves_entries);
    return UNITY_END();
}
