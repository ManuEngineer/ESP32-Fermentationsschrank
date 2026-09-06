#pragma once

#include <cstdint>

namespace device_platform {

enum class DeviceUiIdleState : std::uint8_t {
    Awake,
    Dimmed,
    Asleep,
};

class DeviceUiIdleController {
   public:
    explicit DeviceUiIdleController(std::uint64_t inactivityTimeoutMillis,
                                    bool supportsSleep = true) noexcept
        : inactivityTimeoutMillis_(inactivityTimeoutMillis),
          supportsSleep_(supportsSleep) {}

    void observeUserActivity(std::uint64_t nowMillis) noexcept;
    void observeSystemWake() noexcept;
    void tick(std::uint64_t nowMillis) noexcept;
    [[nodiscard]] DeviceUiIdleState state() const noexcept { return state_; }
    [[nodiscard]] bool isDimmed() const noexcept {
        return state_ == DeviceUiIdleState::Dimmed;
    }
    [[nodiscard]] bool isSleeping() const noexcept {
        return state_ == DeviceUiIdleState::Asleep;
    }
    [[nodiscard]] std::uint64_t lastActivityMillis() const noexcept {
        return lastActivityMillis_;
    }

   private:
    std::uint64_t inactivityTimeoutMillis_{0U};
    std::uint64_t lastActivityMillis_{0U};
    bool hasActivityTimestamp_{false};
    bool supportsSleep_{true};
    DeviceUiIdleState state_{DeviceUiIdleState::Awake};
};

}  // namespace device_platform
