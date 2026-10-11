#include "shared_enable_bridge_sink.hpp"

namespace device_platform {

SharedEnableBridgeSink::SharedEnableBridgeSink(
    IBinaryOutputSink& forwardLeg, IBinaryOutputSink& reverseLeg,
    IBinaryOutputSink& enable) noexcept
    : forwardLeg_(forwardLeg), reverseLeg_(reverseLeg), enable_(enable) {}

bool SharedEnableBridgeSink::allOff() {
    // Every output is attempted exactly once, independent of earlier results.
    const bool enableOff = enable_.setEnabled(false);
    const bool forwardOff = forwardLeg_.setEnabled(false);
    const bool reverseOff = reverseLeg_.setEnabled(false);
    return enableOff && forwardOff && reverseOff;
}

void SharedEnableBridgeSink::shutdown() {
    static_cast<void>(allOff());
    forwardOn_ = false;
    reverseOn_ = false;
    state_ = State::Faulted;
}

bool SharedEnableBridgeSink::begin() {
    if (state_ != State::NotStarted) {
        return state_ == State::Ready;
    }
    if (!allOff()) {
        shutdown();
        return false;
    }
    state_ = State::Ready;
    return true;
}

void SharedEnableBridgeSink::setLeg(IBinaryOutputSink& leg, bool& legOn,
                                    bool otherLegOn, bool enabled) {
    if (state_ == State::NotStarted) {
        return;
    }
    if (state_ == State::Faulted) {
        if (!enabled) {
            // Best effort only; never enables and never lifts the latch.
            shutdown();
        }
        return;
    }
    if (enabled) {
        if (otherLegOn) {
            // Contradictory command: both legs are never requested together.
            shutdown();
            return;
        }
        if (legOn) {
            return;
        }
        // Leg first while the shared enable is still off, enable last.
        if (!leg.setEnabled(true)) {
            shutdown();
            return;
        }
        legOn = true;
        if (!enable_.setEnabled(true)) {
            shutdown();
        }
        return;
    }
    if (!legOn) {
        return;
    }
    // Master gate first, then the leg; both are always attempted.
    const bool enableOff = enable_.setEnabled(false);
    const bool legOff = leg.setEnabled(false);
    legOn = false;
    if (!enableOff || !legOff) {
        shutdown();
    }
}

void SharedEnableBridgeSink::setForward(bool enabled) {
    setLeg(forwardLeg_, forwardOn_, reverseOn_, enabled);
}

void SharedEnableBridgeSink::setReverse(bool enabled) {
    setLeg(reverseLeg_, reverseOn_, forwardOn_, enabled);
}

}  // namespace device_platform
