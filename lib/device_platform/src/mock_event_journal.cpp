#include "mock_event_journal.hpp"

namespace device_platform {

void MockEventJournal::record(uint64_t monotonicMillis,
                              const std::string& message) {
    if (entries_.size() >= kMaxEntries) {
        entries_.erase(entries_.begin());
    }
    entries_.push_back(JournalEntry{monotonicMillis, message});
}

const std::vector<JournalEntry>& MockEventJournal::entries() const {
    return entries_;
}

}  // namespace device_platform
