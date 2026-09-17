#pragma once

#include <cstdint>

namespace device_platform {

// The selected mode is an explicit user decision.  It is deliberately kept
// independent from the presence of home-Wi-Fi credentials: AP_ONLY remains a
// valid mode when no HOME_WIFI credentials exist.
enum class NetworkMode : std::uint8_t {
    AP_ONLY = 1U,
    HOME_WIFI = 2U,
};

[[nodiscard]] constexpr bool isValidNetworkMode(NetworkMode mode) noexcept {
    return mode == NetworkMode::AP_ONLY || mode == NetworkMode::HOME_WIFI;
}

enum class NetworkStartupPath : std::uint8_t {
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
    if (explicitHomeWifiReconfiguration || !validHomeWifiCredentials) {
        return {NetworkStartupPath::HomeWifiSetup};
    }
    return {NetworkStartupPath::HomeWifi};
}

}  // namespace device_platform
