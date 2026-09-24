#pragma once

#include <cstdint>
#include <optional>
#include <mutex>
#include <string>
#include <vector>

#include "connectivity_credentials.hpp"
#include "network_lifecycle.hpp"
#include "network_mode.hpp"
#include "network_startup_policy.hpp"

namespace fermentation {

enum class NetworkConfigurationStatus : std::uint8_t {
    Applied,
    SelectionRequired,
    InvalidMode,
    InvalidCredential,
    CredentialUnavailable,
    TransportFailure,
    CandidateRejected,
    PersistenceFailure,
    StateChanged,
    CommitIndeterminate,
    RecoveryRequired,
    SetupNotAvailable,
    NotInitialized,
};

struct NetworkConfigurationResult {
    NetworkConfigurationStatus status{
        NetworkConfigurationStatus::NotInitialized};
};

struct NetworkConfigurationScanResult {
    NetworkConfigurationStatus status{
        NetworkConfigurationStatus::NotInitialized};
    std::vector<device_platform::NetworkScanEntry> entries;
};

// Fachlicher Netzwerk-Workflow. Er besitzt keine UserConfiguration und gibt
// Secrets nicht als Status-/View-Wert heraus. Der Kandidat lebt nur bis zum
// Test-/Commit-Ergebnis im RAM; der einzige produktive Credential-Write geht
// ueber den bestehenden ConnectivityCredentialStore.
class NetworkConfigurationService final {
   public:
    NetworkConfigurationService(ConnectivityCredentialStore& credentialStore,
                                device_platform::INetworkLifecycle& lifecycle)
        : credentialStore_(credentialStore), lifecycle_(lifecycle) {}

    [[nodiscard]] NetworkConfigurationResult start(
        device_platform::NetworkMode selectedMode,
        device_platform::StorageEpoch storageEpoch,
        bool explicitHomeWifiReconfiguration = false);

    [[nodiscard]] NetworkConfigurationScanResult scan();

    [[nodiscard]] NetworkConfigurationResult beginCandidate(
        std::string ssid, std::string password);
    [[nodiscard]] NetworkConfigurationResult testCandidate();
    [[nodiscard]] NetworkConfigurationResult beginHomeWifiReconfiguration();
    void discardCandidate() noexcept {
        const std::lock_guard<std::recursive_mutex> lock(mutex_);
        candidate_.reset();
    }

    [[nodiscard]] device_platform::NetworkStatus status() const {
        const std::lock_guard<std::recursive_mutex> lock(mutex_);
        return lifecycle_.status();
    }
    [[nodiscard]] std::optional<device_platform::NetworkAccessPointInfo>
    accessPointInfo() const {
        const std::lock_guard<std::recursive_mutex> lock(mutex_);
        return lifecycle_.accessPointInfo();
    }
    [[nodiscard]] device_platform::NetworkMode selectedMode() const noexcept {
        const std::lock_guard<std::recursive_mutex> lock(mutex_);
        return selectedMode_;
    }
    [[nodiscard]] bool candidatePending() const noexcept {
        const std::lock_guard<std::recursive_mutex> lock(mutex_);
        return candidate_.has_value();
    }
    [[nodiscard]] bool setupFlowActive() const noexcept {
        const std::lock_guard<std::recursive_mutex> lock(mutex_);
        return setupFlowActive_;
    }
    [[nodiscard]] bool recoveryRequired() const noexcept {
        const std::lock_guard<std::recursive_mutex> lock(mutex_);
        return recoveryRequired_;
    }

   private:
    [[nodiscard]] NetworkConfigurationResult mapLifecycleResult(
        device_platform::NetworkOperationResult result) const;
    [[nodiscard]] NetworkConfigurationResult restoreActiveTransport();

    ConnectivityCredentialStore& credentialStore_;
    device_platform::INetworkLifecycle& lifecycle_;
    device_platform::NetworkMode selectedMode_{
        device_platform::NetworkMode::UNSELECTED};
    device_platform::StorageEpoch storageEpoch_;
    std::optional<ConnectivityCredential> activeCredential_;
    std::optional<ConnectivityCredential> candidate_;
    bool initialized_{false};
    bool setupFlowActive_{false};
    bool recoveryRequired_{false};
    mutable std::recursive_mutex mutex_;
};

}  // namespace fermentation
