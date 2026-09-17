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

enum class NetworkStartupPath : std::uint8_t {
    SelectionRequired,
    AccessPointOnly,
    HomeWifi,
    HomeWifiSetup,
};

struct NetworkStartupDecision {
    NetworkStartupPath path{NetworkStartupPath::HomeWifiSetup};
};

[[nodiscard]] constexpr NetworkStartupDecision decideNetworkStartup(
    NetworkMode mode, bool validHomeWifiCredentials,
    bool explicitHomeWifiReconfiguration) noexcept {
    if (mode == NetworkMode::AP_ONLY) {
        return {NetworkStartupPath::AccessPointOnly};
    }
    if (mode == NetworkMode::UNSELECTED) {
        return {NetworkStartupPath::SelectionRequired};
    }
    if (explicitHomeWifiReconfiguration || !validHomeWifiCredentials) {
        return {NetworkStartupPath::HomeWifiSetup};
    }
    return {NetworkStartupPath::HomeWifi};
}

}  // namespace device_platform
