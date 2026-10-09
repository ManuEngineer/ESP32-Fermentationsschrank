#include "mock_binary_output_sink.hpp"

namespace device_platform_test_support {

bool MockBinaryOutputSink::setEnabled(bool enabled) {
    if (!accepting_) {
        ++rejectedCommandCount_;
        return false;
    }
    enabled_ = enabled;
    if (journal_.size() >= kMaxJournalEntries) {
        journal_.erase(journal_.begin());
    }
    journal_.push_back(BinaryOutputCommand{enabled});
    return true;
}

void MockBinaryOutputSink::setAcceptingCommands(bool accepting) {
    accepting_ = accepting;
}

std::size_t MockBinaryOutputSink::rejectedCommandCount() const {
    return rejectedCommandCount_;
}

bool MockBinaryOutputSink::enabled() const { return enabled_; }

const std::vector<BinaryOutputCommand>& MockBinaryOutputSink::commandJournal()
    const {
    return journal_;
}

}  // namespace device_platform_test_support
