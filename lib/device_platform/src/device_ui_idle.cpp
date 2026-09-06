#include "device_ui_idle.hpp"

#include <limits>

namespace device_platform {

void DeviceUiIdleController::observeUserActivity(
    std::uint64_t nowMillis) noexcept {
    lastActivityMillis_ = nowMillis;
    hasActivityTimestamp_ = true;
    state_ = DeviceUiIdleState::Awake;
}

void DeviceUiIdleController::observeSystemWake() noexcept {
    state_ = DeviceUiIdleState::Awake;
}

void DeviceUiIdleController::tick(std::uint64_t nowMillis) noexcept {
    if (!hasActivityTimestamp_ || inactivityTimeoutMillis_ == 0U ||
        nowMillis < lastActivityMillis_) {
        return;
    }
    const auto elapsed = nowMillis - lastActivityMillis_;
    if (elapsed < inactivityTimeoutMillis_) return;
    state_ =
        supportsSleep_ ? DeviceUiIdleState::Asleep : DeviceUiIdleState::Dimmed;
}

}  // namespace device_platform
