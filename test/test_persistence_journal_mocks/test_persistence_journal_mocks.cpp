#include <unity.h>

#include "mock_event_journal.hpp"

void test_event_journal_records_entries_in_order() {
    device_platform_test_support::MockEventJournal journal;

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
    device_platform_test_support::MockEventJournal journal;

    const std::size_t entriesToWrite =
        device_platform_test_support::MockEventJournal::kMaxEntries + 10;
    for (std::size_t i = 0; i < entriesToWrite; ++i) {
        TEST_ASSERT_TRUE(journal.record(static_cast<uint64_t>(i), "event"));
    }

    const auto& entries = journal.entries();
    TEST_ASSERT_EQUAL_UINT32(
        static_cast<unsigned>(
            device_platform_test_support::MockEventJournal::kMaxEntries),
        entries.size());
    // Die aeltesten zehn Eintraege (Zeitstempel 0..9) muessen verworfen sein.
    TEST_ASSERT_EQUAL_UINT64(10U, entries.front().monotonicMillis);
}

void test_event_journal_write_failure_is_reported_and_preserves_entries() {
    device_platform_test_support::MockEventJournal journal;
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
    RUN_TEST(test_event_journal_records_entries_in_order);
    RUN_TEST(test_event_journal_is_bounded_and_drops_oldest_entries);
    RUN_TEST(
        test_event_journal_write_failure_is_reported_and_preserves_entries);
    return UNITY_END();
}
