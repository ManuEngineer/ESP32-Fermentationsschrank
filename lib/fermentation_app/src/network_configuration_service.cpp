#include "network_configuration_service.hpp"

#include <limits>
#include <utility>

#include "configuration_limits.hpp"
#include "connectivity_credential_codec.hpp"

namespace fermentation {

NetworkConfigurationResult NetworkConfigurationService::mapLifecycleResult(
    device_platform::NetworkOperationResult result) const {
    if (result.status == device_platform::NetworkOperationStatus::Applied) {
        return {NetworkConfigurationStatus::Applied};
    }
    if (result.status ==
        device_platform::NetworkOperationStatus::InvalidInput) {
        return {NetworkConfigurationStatus::InvalidCredential};
    }
    return {result.status == device_platform::NetworkOperationStatus::Busy
                ? NetworkConfigurationStatus::TransportFailure
                : NetworkConfigurationStatus::TransportFailure};
}

NetworkConfigurationResult
NetworkConfigurationService::restoreActiveTransport() {
    std::optional<device_platform::NetworkCredentials> credentials;
    if (activeCredential_.has_value()) {
        credentials = activeCredential_->homeWifi;
    }
    return mapLifecycleResult(lifecycle_.start(selectedMode_, credentials));
}

NetworkConfigurationResult NetworkConfigurationService::start(
    device_platform::NetworkMode selectedMode,
    device_platform::StorageEpoch storageEpoch,
    bool explicitHomeWifiReconfiguration) {
    if (!device_platform::isValidNetworkMode(selectedMode) ||
        storageEpoch.value() == 0U) {
        return {NetworkConfigurationStatus::InvalidMode};
    }
    selectedMode_ = selectedMode;
    storageEpoch_ = storageEpoch;
    candidate_.reset();
    recoveryRequired_ = false;
    setupFlowActive_ = false;
    if (selectedMode == device_platform::NetworkMode::UNSELECTED) {
        initialized_ = false;
        return {NetworkConfigurationStatus::SelectionRequired};
    }

    activeCredential_.reset();
    if (selectedMode == device_platform::NetworkMode::HOME_WIFI) {
        const auto loaded = credentialStore_.load(storageEpoch);
        if (loaded.status == ConnectivityCredentialLoadStatus::Available &&
            loaded.record.has_value()) {
            if (loaded.record->credential.homeWifi.has_value()) {
                activeCredential_ = loaded.record->credential;
            }
        } else if (loaded.status !=
                       ConnectivityCredentialLoadStatus::NotFound &&
                   loaded.status !=
                       ConnectivityCredentialLoadStatus::OtherEpoch) {
            initialized_ = false;
            return {loaded.status ==
                            ConnectivityCredentialLoadStatus::CapacityError
                        ? NetworkConfigurationStatus::CredentialUnavailable
                        : NetworkConfigurationStatus::PersistenceFailure};
        }
    }

    const auto decision =
        decideNetworkStartup(selectedMode, activeCredential_.has_value(),
                             explicitHomeWifiReconfiguration);
    if (decision.path == NetworkStartupPath::SelectionRequired) {
        initialized_ = false;
        return {NetworkConfigurationStatus::SelectionRequired};
    }
    setupFlowActive_ = decision.path == NetworkStartupPath::HomeWifiSetup;
    std::optional<device_platform::NetworkCredentials> credentials;
    if (decision.path == NetworkStartupPath::HomeWifi &&
        activeCredential_.has_value()) {
        credentials = activeCredential_->homeWifi;
    }
    const auto result = lifecycle_.start(selectedMode, credentials);
    if (result.status != device_platform::NetworkOperationStatus::Applied) {
        initialized_ = false;
        return mapLifecycleResult(result);
    }
    initialized_ = true;
    return {NetworkConfigurationStatus::Applied};
}

NetworkConfigurationScanResult NetworkConfigurationService::scan() {
    if (!initialized_ || !setupFlowActive_ ||
        selectedMode_ == device_platform::NetworkMode::UNSELECTED) {
        const auto status = initialized_
                                ? NetworkConfigurationStatus::SetupNotAvailable
                                : NetworkConfigurationStatus::NotInitialized;
        return {status, {}};
    }
    const auto result = lifecycle_.scan();
    if (result.status != device_platform::NetworkOperationStatus::Applied) {
        return {NetworkConfigurationStatus::TransportFailure, {}};
    }
    return {NetworkConfigurationStatus::Applied, result.entries};
}

NetworkConfigurationResult NetworkConfigurationService::beginCandidate(
    std::string ssid, std::string password) {
    if (!initialized_ || !setupFlowActive_ ||
        selectedMode_ != device_platform::NetworkMode::HOME_WIFI) {
        return {initialized_ ? NetworkConfigurationStatus::SetupNotAvailable
                             : NetworkConfigurationStatus::NotInitialized};
    }
    ConnectivityCredential candidate;
    candidate.homeWifi = device_platform::NetworkCredentials{
        std::move(ssid), std::move(password)};
    if (validateConnectivityCredential(candidate) !=
        ConnectivityCredentialValidationStatus::Success) {
        return {NetworkConfigurationStatus::InvalidCredential};
    }
    candidate_ = std::move(candidate);
    return {NetworkConfigurationStatus::Applied};
}

NetworkConfigurationResult NetworkConfigurationService::testCandidate() {
    if (!initialized_ || !setupFlowActive_ || !candidate_.has_value() ||
        !candidate_->homeWifi.has_value()) {
        return {NetworkConfigurationStatus::CandidateRejected};
    }
    const auto tested = lifecycle_.testCandidate(*candidate_->homeWifi);
    if (tested.status != device_platform::NetworkOperationStatus::Applied) {
        // The active credential and lifecycle remain untouched by a failed
        // candidate test. The volatile candidate is deliberately discarded.
        static_cast<void>(restoreActiveTransport());
        candidate_.reset();
        return {NetworkConfigurationStatus::CandidateRejected};
    }

    std::uint64_t nextSequence = 1U;
    const auto current = credentialStore_.load(storageEpoch_);
    if (current.status == ConnectivityCredentialLoadStatus::Available &&
        current.record.has_value()) {
        if (current.record->recordSequence ==
            std::numeric_limits<std::uint64_t>::max()) {
            static_cast<void>(restoreActiveTransport());
            candidate_.reset();
            return {NetworkConfigurationStatus::PersistenceFailure};
        }
        nextSequence = current.record->recordSequence + 1U;
    } else if (current.status != ConnectivityCredentialLoadStatus::NotFound &&
               current.status != ConnectivityCredentialLoadStatus::OtherEpoch) {
        static_cast<void>(restoreActiveTransport());
        candidate_.reset();
        return {NetworkConfigurationStatus::PersistenceFailure};
    }

    const auto written =
        credentialStore_.write(*candidate_, storageEpoch_, nextSequence);
    if (written.status == ConnectivityCredentialWriteStatus::Committed) {
        const auto committed = *candidate_;
        const auto applied = lifecycle_.start(
            device_platform::NetworkMode::HOME_WIFI, committed.homeWifi);
        if (applied.status !=
            device_platform::NetworkOperationStatus::Applied) {
            // The persistent winner is known, but the runtime could not adopt
            // it. Stop rather than restoring an older runtime credential; the
            // next explicit start reloads cc0 and resolves the state.
            activeCredential_ = committed;
            candidate_.reset();
            initialized_ = false;
            recoveryRequired_ = true;
            static_cast<void>(lifecycle_.stop());
            return {NetworkConfigurationStatus::RecoveryRequired};
        }
        activeCredential_ = committed;
        candidate_.reset();
        setupFlowActive_ = false;
        return {NetworkConfigurationStatus::Applied};
    }
    const auto status =
        written.status == ConnectivityCredentialWriteStatus::Indeterminate
            ? NetworkConfigurationStatus::CommitIndeterminate
            : NetworkConfigurationStatus::PersistenceFailure;
    if (written.status == ConnectivityCredentialWriteStatus::Indeterminate) {
        // A write followed by an uncertain readback may have changed cc0.
        // Keeping the old runtime would create a runtime/persistence split.
        activeCredential_.reset();
        initialized_ = false;
        recoveryRequired_ = true;
        static_cast<void>(lifecycle_.stop());
    } else {
        static_cast<void>(restoreActiveTransport());
    }
    candidate_.reset();
    return {status};
}

NetworkConfigurationResult
NetworkConfigurationService::beginHomeWifiReconfiguration() {
    if (selectedMode_ != device_platform::NetworkMode::HOME_WIFI ||
        storageEpoch_.value() == 0U) {
        return {NetworkConfigurationStatus::SetupNotAvailable};
    }
    return start(device_platform::NetworkMode::HOME_WIFI, storageEpoch_, true);
}

void NetworkConfigurationService::discardCandidate() noexcept {
    candidate_.reset();
}

}  // namespace fermentation
