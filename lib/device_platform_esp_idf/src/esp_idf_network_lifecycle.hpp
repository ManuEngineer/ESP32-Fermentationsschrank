#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "network_lifecycle.hpp"

namespace device_platform_esp_idf {

struct EspIdfNetworkLifecycleConfig {
    std::string softApSsid;
    std::string softApPassword;
    std::string hostname;
};

class EspIdfNetworkLifecycle final : public device_platform::INetworkLifecycle {
   public:
    explicit EspIdfNetworkLifecycle(EspIdfNetworkLifecycleConfig config)
        : config_(std::move(config)) {}
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
    void poll() override;

   private:
    [[nodiscard]] bool ensureInitialized();
    [[nodiscard]] bool configureAccessPoint();
    [[nodiscard]] bool configureStation(
        const device_platform::NetworkCredentials& credentials);
    [[nodiscard]] bool startWifi();
    [[nodiscard]] bool connectStationOnce();
    void stopWifi() noexcept;
    static void handleEvent(void* context, esp_event_base_t eventBase,
                            std::int32_t eventId, void* eventData);
    void unregisterEventHandlers() noexcept;
    [[nodiscard]] static bool validAccessPointConfig(
        const EspIdfNetworkLifecycleConfig& config);

    EspIdfNetworkLifecycleConfig config_;
    esp_netif_t* stationNetif_{nullptr};
    esp_netif_t* accessPointNetif_{nullptr};
    device_platform::NetworkStatus status_;
    bool initialized_{false};
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
    mutable std::mutex operationMutex_;
    mutable std::mutex stateMutex_;
    esp_event_handler_instance_t wifiEventHandler_{nullptr};
    esp_event_handler_instance_t ipEventHandler_{nullptr};
};

}  // namespace device_platform_esp_idf
