#include "mock_binary_output_sink.hpp"

namespace device_platform_test_support {

void MockBinaryOutputSink::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (journal_.size() >= kMaxJournalEntries) {
        journal_.erase(journal_.begin());
    }
    journal_.push_back(BinaryOutputCommand{enabled});
}

bool MockBinaryOutputSink::enabled() const { return enabled_; }

const std::vector<BinaryOutputCommand>& MockBinaryOutputSink::commandJournal()
    const {
    return journal_;
}

}  // namespace device_platform_test_support
