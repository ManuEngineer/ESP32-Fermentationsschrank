#pragma once

#include <cstdint>

#include "network_mode.hpp"

namespace fermentation {

// The startup path is an application decision. The platform lifecycle only
// receives the resulting transport mode and volatile credentials.
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
    device_platform::NetworkMode mode, bool validHomeWifiCredentials,
    bool explicitHomeWifiReconfiguration) noexcept {
    if (mode == device_platform::NetworkMode::AP_ONLY) {
        return {NetworkStartupPath::AccessPointOnly};
    }
    if (mode == device_platform::NetworkMode::UNSELECTED) {
        return {NetworkStartupPath::SelectionRequired};
    }
    if (explicitHomeWifiReconfiguration || !validHomeWifiCredentials) {
        return {NetworkStartupPath::HomeWifiSetup};
    }
    return {NetworkStartupPath::HomeWifi};
}

}  // namespace fermentation
