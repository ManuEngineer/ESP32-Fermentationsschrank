#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "network_mode.hpp"

namespace device_platform {

inline constexpr std::size_t kMaximumNetworkSsidBytes = 32U;
inline constexpr std::size_t kMaximumNetworkScanEntries = 16U;
inline constexpr std::size_t kMaximumNetworkScanResponseBytes =
    kMaximumNetworkScanEntries * (kMaximumNetworkSsidBytes + 1U);

struct NetworkCredentials {
    std::string ssid;
    std::string password;

    friend bool operator==(const NetworkCredentials& left,
                           const NetworkCredentials& right) {
        return left.ssid == right.ssid && left.password == right.password;
    }
    friend bool operator!=(const NetworkCredentials& left,
                           const NetworkCredentials& right) {
        return !(left == right);
    }
};

struct NetworkScanEntry {
    std::string ssid;
    std::int8_t rssi{0};
    bool protectedNetwork{false};
};

// Renderer-independent access data for the currently active local SoftAP.
// This is intentionally a concrete setup contract, not a general secret
// store. It is available only while the transport exposes that SoftAP.
struct NetworkAccessPointInfo {
    std::string ssid;
    std::string password;
    std::optional<std::uint32_t> ipv4Address;

    friend bool operator==(const NetworkAccessPointInfo& left,
                           const NetworkAccessPointInfo& right) {
        return left.ssid == right.ssid && left.password == right.password &&
               left.ipv4Address == right.ipv4Address;
    }
    friend bool operator!=(const NetworkAccessPointInfo& left,
                           const NetworkAccessPointInfo& right) {
        return !(left == right);
    }
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
    NetworkMode selectedMode{NetworkMode::UNSELECTED};
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

// An application-neutral lifecycle port. It owns volatile transport state
// only; persistence, preview, commit, and credential redaction remain outside
// this transport boundary.
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
    // Supplies the canonical application device name before startup. The
    // transport derives its advertised hostname/mDNS identity from it.
    [[nodiscard]] virtual NetworkOperationResult setHostname(
        const std::string& hostname) = 0;
    // Supplies the volatile SoftAP identity before the next transport start.
    // The application owns persistence and derives the value; the lifecycle
    // only retains it in RAM for the concrete Wi-Fi adapter.
    [[nodiscard]] virtual NetworkOperationResult setAccessPointCredentials(
        const std::string& ssid, const std::string& password) = 0;
    [[nodiscard]] virtual NetworkStatus status() const = 0;
    // Deliberately separate sensitive local SoftAP setup data from the
    // generic status/diagnostic contract.
    [[nodiscard]] virtual std::optional<NetworkAccessPointInfo>
    accessPointInfo() const = 0;
    // Monotonic change identity of accessPointInfo(), readable without
    // copying any SSID or password: it changes exactly when the semantic
    // NetworkAccessPointInfo changes (SSID, password or IPv4 address) or the
    // data is set or cleared, and never otherwise. It carries no secret and
    // lets a renderer decide whether to fetch the data at all. It is not a
    // second source of the access data.
    [[nodiscard]] virtual std::uint64_t accessPointInfoRevision()
        const noexcept = 0;
    virtual void poll() = 0;
};

}  // namespace device_platform
