#include "mock_event_journal.hpp"

namespace device_platform {

bool MockEventJournal::record(uint64_t monotonicMillis,
                              const std::string& message) {
    if (writeShouldFail_) {
        return false;
    }
    if (entries_.size() >= kMaxEntries) {
        entries_.erase(entries_.begin());
    }
    entries_.push_back(JournalEntry{monotonicMillis, message});
    return true;
}

const std::vector<JournalEntry>& MockEventJournal::entries() const {
    return entries_;
}

void MockEventJournal::injectWriteFailure(bool shouldFail) {
    writeShouldFail_ = shouldFail;
}

}  // namespace device_platform
