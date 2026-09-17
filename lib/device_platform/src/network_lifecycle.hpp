#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "network_mode.hpp"

namespace device_platform {

struct NetworkCredentials {
    std::string ssid;
    std::string password;
};

struct NetworkScanEntry {
    std::string ssid;
    std::int8_t rssi{0};
    bool protectedNetwork{false};
};

enum class NetworkLifecycleState : std::uint8_t {
    Stopped,
    SetupAccessPoint,
    AccessPointOnly,
    ConnectingHome,
    HomeConnected,
    CandidateTesting,
    Failed,
};

struct NetworkStatus {
    NetworkMode selectedMode{NetworkMode::HOME_WIFI};
    NetworkLifecycleState state{NetworkLifecycleState::Stopped};
    bool httpReady{false};
    std::optional<std::uint32_t> ipv4Address;
};

enum class NetworkOperationStatus : std::uint8_t {
    Applied,
    InvalidInput,
    Busy,
    Failed,
};

struct NetworkOperationResult {
    NetworkOperationStatus status{NetworkOperationStatus::Failed};
};

struct NetworkScanResult {
    NetworkOperationStatus status{NetworkOperationStatus::Failed};
    std::vector<NetworkScanEntry> entries;
};

// An application-neutral lifecycle port.  It owns volatile transport state
// only; persistence, preview, commit, and credential redaction remain owned by
// fermentation_app.
class INetworkLifecycle {
   public:
    INetworkLifecycle() = default;
    virtual ~INetworkLifecycle() = default;

    INetworkLifecycle(const INetworkLifecycle&) = delete;
    INetworkLifecycle& operator=(const INetworkLifecycle&) = delete;
    INetworkLifecycle(INetworkLifecycle&&) = delete;
    INetworkLifecycle& operator=(INetworkLifecycle&&) = delete;

    [[nodiscard]] virtual NetworkOperationResult start(
        NetworkMode mode,
        const std::optional<NetworkCredentials>& homeCredentials) = 0;
    [[nodiscard]] virtual NetworkOperationResult stop() = 0;
    [[nodiscard]] virtual NetworkScanResult scan() = 0;
    [[nodiscard]] virtual NetworkOperationResult testCandidate(
        const NetworkCredentials& candidate) = 0;
    [[nodiscard]] virtual NetworkStatus status() const = 0;
    virtual void poll() = 0;
};

}  // namespace device_platform
