#include "raw_touch_recovery_detector.hpp"

namespace fermentation {

bool RawTouchRecoveryDetector::isHeldContact(
    const device_platform::RawTouchSample& sample) const noexcept {
    return sample.status == device_platform::RawTouchSampleStatus::Contact &&
           sample.contact && sample.strength >= minimumStrength_;
}

bool RawTouchRecoveryDetector::observe(
    const device_platform::RawTouchSample& sample) noexcept {
    if (triggered_) {
        return false;
    }
    if (!isHeldContact(sample)) {
        holdStartUs_.reset();
        return false;
    }
    if (!holdStartUs_.has_value()) {
        holdStartUs_ = sample.monotonicTimeUs;
        return false;
    }
    if (sample.monotonicTimeUs < *holdStartUs_) {
        // A monotonic source must never go backwards; treat this as a new
        // contact instead of silently accepting an inconsistent duration.
        holdStartUs_ = sample.monotonicTimeUs;
        return false;
    }
    if (sample.monotonicTimeUs - *holdStartUs_ >= requiredHoldMicros_) {
        triggered_ = true;
        return true;
    }
    return false;
}

void RawTouchRecoveryDetector::reset() noexcept {
    holdStartUs_.reset();
    triggered_ = false;
}

}  // namespace fermentation
