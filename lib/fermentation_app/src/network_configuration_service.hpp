#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "connectivity_credentials.hpp"
#include "network_lifecycle.hpp"
#include "network_mode.hpp"
#include "network_startup_policy.hpp"
#include "secure_random_source.hpp"

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
    NetworkConfigurationService(
        ConnectivityCredentialStore& credentialStore,
        device_platform::INetworkLifecycle& lifecycle,
        device_platform::ISecureRandomSource& randomSource)
        : credentialStore_(credentialStore),
          lifecycle_(lifecycle),
          randomSource_(randomSource) {}

    [[nodiscard]] NetworkConfigurationResult start(
        device_platform::NetworkMode selectedMode,
        device_platform::StorageEpoch storageEpoch,
        std::string canonicalDeviceName,
        bool explicitHomeWifiReconfiguration = false);

    [[nodiscard]] NetworkConfigurationScanResult scan();

    [[nodiscard]] NetworkConfigurationResult beginCandidate(
        std::string ssid, std::string password);
    [[nodiscard]] NetworkConfigurationResult testCandidate();
    [[nodiscard]] NetworkConfigurationResult beginHomeWifiReconfiguration(
        std::string canonicalDeviceName);
    void discardCandidate() noexcept;

    [[nodiscard]] device_platform::NetworkStatus status() const {
        return lifecycle_.status();
    }
    [[nodiscard]] std::optional<device_platform::NetworkAccessPointInfo>
    accessPointInfo() const {
        return lifecycle_.accessPointInfo();
    }
    [[nodiscard]] std::uint64_t accessPointInfoRevision() const noexcept {
        return lifecycle_.accessPointInfoRevision();
    }
    [[nodiscard]] device_platform::NetworkMode selectedMode() const noexcept {
        return selectedMode_;
    }
    [[nodiscard]] bool candidatePending() const noexcept {
        return candidate_.has_value();
    }
    [[nodiscard]] bool setupFlowActive() const noexcept {
        return setupFlowActive_;
    }
    [[nodiscard]] bool recoveryRequired() const noexcept {
        return recoveryRequired_;
    }

   private:
    [[nodiscard]] NetworkConfigurationResult mapLifecycleResult(
        device_platform::NetworkOperationResult result) const;
    [[nodiscard]] NetworkConfigurationResult restoreActiveTransport();
    [[nodiscard]] NetworkConfigurationResult ensureCredential(
        device_platform::StorageEpoch storageEpoch,
        const std::string& canonicalDeviceName);
    [[nodiscard]] NetworkConfigurationResult writeNewCredential(
        ConnectivityCredential credential,
        device_platform::StorageEpoch storageEpoch,
        std::uint64_t recordSequence);

    ConnectivityCredentialStore& credentialStore_;
    device_platform::INetworkLifecycle& lifecycle_;
    device_platform::ISecureRandomSource& randomSource_;
    device_platform::NetworkMode selectedMode_{
        device_platform::NetworkMode::UNSELECTED};
    device_platform::StorageEpoch storageEpoch_;
    std::optional<ConnectivityCredential> activeCredential_;
    std::optional<ConnectivityCredential> candidate_;
    std::string activeSoftApSsid_;
    bool initialized_{false};
    bool setupFlowActive_{false};
    bool recoveryRequired_{false};
};

}  // namespace fermentation
