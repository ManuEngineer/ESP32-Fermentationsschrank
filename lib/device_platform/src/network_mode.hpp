#pragma once

#include <cstdint>

namespace device_platform {

// UNSELECTED is an internal bootstrap/migration state only. It is deliberately
// kept separate from the two user-selectable modes and from credential
// presence: AP_ONLY remains valid when no HOME_WIFI credentials exist.
enum class NetworkMode : std::uint8_t {
    UNSELECTED = 0U,
    AP_ONLY = 1U,
    HOME_WIFI = 2U,
};

[[nodiscard]] constexpr bool isValidNetworkMode(NetworkMode mode) noexcept {
    return mode == NetworkMode::UNSELECTED || mode == NetworkMode::AP_ONLY ||
           mode == NetworkMode::HOME_WIFI;
}

[[nodiscard]] constexpr bool isSelectableNetworkMode(
    NetworkMode mode) noexcept {
    return mode == NetworkMode::AP_ONLY || mode == NetworkMode::HOME_WIFI;
}

}  // namespace device_platform
