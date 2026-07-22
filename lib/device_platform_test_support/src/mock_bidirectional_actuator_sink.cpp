#include "mock_bidirectional_actuator_sink.hpp"

namespace device_platform_test_support {

void MockBidirectionalActuatorSink::setForward(bool enabled) {
    forward_ = enabled;
    record(BidirectionalActuatorCommandKind::Forward, enabled);
    checkSimultaneousActivation();
}

void MockBidirectionalActuatorSink::setReverse(bool enabled) {
    reverse_ = enabled;
    record(BidirectionalActuatorCommandKind::Reverse, enabled);
    checkSimultaneousActivation();
}

bool MockBidirectionalActuatorSink::forward() const { return forward_; }

bool MockBidirectionalActuatorSink::reverse() const { return reverse_; }

const std::vector<BidirectionalActuatorCommand>&
MockBidirectionalActuatorSink::commandJournal() const {
    return journal_;
}

bool MockBidirectionalActuatorSink::simultaneousActivationObserved() const {
    return simultaneousActivationObserved_;
}

void MockBidirectionalActuatorSink::record(
    BidirectionalActuatorCommandKind kind, bool enabled) {
    if (journal_.size() >= kMaxJournalEntries) {
        journal_.erase(journal_.begin());
    }
    journal_.push_back(BidirectionalActuatorCommand{kind, enabled});
}

void MockBidirectionalActuatorSink::checkSimultaneousActivation() {
    if (forward_ && reverse_) {
        simultaneousActivationObserved_ = true;
    }
}

}  // namespace device_platform_test_support
