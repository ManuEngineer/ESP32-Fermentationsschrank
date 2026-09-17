#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "network_lifecycle.hpp"

namespace device_platform_esp_idf {

struct EspIdfNetworkLifecycleConfig {
    std::string softApSsid;
    std::string softApPassword;
    std::string hostname;
};

class EspIdfNetworkLifecycle final : public device_platform::INetworkLifecycle {
   public:
    explicit EspIdfNetworkLifecycle(EspIdfNetworkLifecycleConfig config);
    ~EspIdfNetworkLifecycle() override;

    EspIdfNetworkLifecycle(const EspIdfNetworkLifecycle&) = delete;
    EspIdfNetworkLifecycle& operator=(const EspIdfNetworkLifecycle&) = delete;
    EspIdfNetworkLifecycle(EspIdfNetworkLifecycle&&) = delete;
    EspIdfNetworkLifecycle& operator=(EspIdfNetworkLifecycle&&) = delete;

    [[nodiscard]] device_platform::NetworkOperationResult start(
        device_platform::NetworkMode mode,
        const std::optional<device_platform::NetworkCredentials>& credentials)
        override;
    [[nodiscard]] device_platform::NetworkOperationResult stop() override;
    [[nodiscard]] device_platform::NetworkScanResult scan() override;
    [[nodiscard]] device_platform::NetworkOperationResult testCandidate(
        const device_platform::NetworkCredentials& candidate) override;
    [[nodiscard]] device_platform::NetworkOperationResult setHostname(
        const std::string& hostname) override;
    [[nodiscard]] device_platform::NetworkStatus status() const override;
    [[nodiscard]] std::optional<device_platform::NetworkAccessPointInfo>
    accessPointInfo() const override;
    void poll() override;

   private:
    struct Impl;

    [[nodiscard]] bool ensureInitialized();
    void cleanupInitialization() noexcept;
    void destroyDefaultNetifs() noexcept;
    [[nodiscard]] bool configureAccessPoint();
    [[nodiscard]] bool configureStation(
        const device_platform::NetworkCredentials& credentials);
    [[nodiscard]] bool startWifi();
    [[nodiscard]] bool connectStationOnce();
    [[nodiscard]] bool requestIntentionalDisconnect() noexcept;
    void stopWifi() noexcept;
    void unregisterEventHandlers() noexcept;
    [[nodiscard]] static bool validAccessPointConfig(
        const EspIdfNetworkLifecycleConfig& config);

    EspIdfNetworkLifecycleConfig config_;
    device_platform::NetworkStatus status_;
    std::optional<device_platform::NetworkAccessPointInfo> accessPointInfo_;
    bool initialized_{false};
    bool wifiInitialized_{false};
    bool mdnsInitialized_{false};
    bool wifiStarted_{false};
    bool candidateTesting_{false};
    enum class CandidateTestOutcome : std::uint8_t {
        None,
        Connected,
        Failed,
    };
    CandidateTestOutcome candidateTestOutcome_{CandidateTestOutcome::None};
    std::optional<device_platform::NetworkCredentials> activeHomeCredentials_;
    bool reconnectAllowed_{false};
    bool reconnectRequested_{false};
    std::uint32_t intentionalDisconnectsPending_{0U};
    mutable std::mutex operationMutex_;
    mutable std::mutex stateMutex_;
    std::unique_ptr<Impl> impl_;
};

}  // namespace device_platform_esp_idf
