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
    [[nodiscard]] device_platform::NetworkOperationResult
    setAccessPointCredentials(const std::string& ssid,
                              const std::string& password) override;
    [[nodiscard]] device_platform::NetworkStatus status() const override;
    [[nodiscard]] std::optional<device_platform::NetworkAccessPointInfo>
    accessPointInfo() const override;
    [[nodiscard]] std::uint64_t accessPointInfoRevision()
        const noexcept override;
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
    // True if the Wi-Fi driver is stopped afterwards: it was never started,
    // `esp_wifi_stop()` returned ESP_OK, or the driver is not initialized
    // (ESP_ERR_WIFI_NOT_INIT, the only other documented result). Any other
    // result for a started driver is NOT a stop; `wifiStarted_` stays true.
    [[nodiscard]] bool stopWifi() noexcept;
    void unregisterEventHandlers() noexcept;
    [[nodiscard]] static bool validAccessPointConfig(
        const EspIdfNetworkLifecycleConfig& config);

    EspIdfNetworkLifecycleConfig config_;
    device_platform::NetworkStatus status_;
    // Both are guarded by stateMutex_; every write goes through
    // setAccessPointInfoLocked() so the revision tracks the data exactly.
    std::optional<device_platform::NetworkAccessPointInfo> accessPointInfo_;
    std::uint64_t accessPointInfoRevision_{0U};
    void setAccessPointInfoLocked(
        std::optional<device_platform::NetworkAccessPointInfo> info);
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
