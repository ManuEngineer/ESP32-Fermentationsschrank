#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "connectivity_credentials.hpp"
#include "network_lifecycle.hpp"
#include "network_mode.hpp"

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
    CommitIndeterminate,
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
    void discardCandidate() noexcept;

    [[nodiscard]] device_platform::NetworkStatus status() const {
        return lifecycle_.status();
    }
    [[nodiscard]] device_platform::NetworkMode selectedMode() const noexcept {
        return selectedMode_;
    }
    [[nodiscard]] bool candidatePending() const noexcept {
        return candidate_.has_value();
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
};

}  // namespace fermentation
