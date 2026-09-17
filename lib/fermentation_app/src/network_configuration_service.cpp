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

    const auto decision = device_platform::decideNetworkStartup(
        selectedMode, activeCredential_.has_value(),
        explicitHomeWifiReconfiguration);
    if (decision.path ==
        device_platform::NetworkStartupPath::SelectionRequired) {
        initialized_ = false;
        return {NetworkConfigurationStatus::SelectionRequired};
    }
    std::optional<device_platform::NetworkCredentials> credentials;
    if (decision.path == device_platform::NetworkStartupPath::HomeWifi &&
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
    if (!initialized_ ||
        selectedMode_ == device_platform::NetworkMode::UNSELECTED) {
        return {NetworkConfigurationStatus::NotInitialized, {}};
    }
    const auto result = lifecycle_.scan();
    if (result.status != device_platform::NetworkOperationStatus::Applied) {
        return {NetworkConfigurationStatus::TransportFailure, {}};
    }
    return {NetworkConfigurationStatus::Applied, result.entries};
}

NetworkConfigurationResult NetworkConfigurationService::beginCandidate(
    std::string ssid, std::string password) {
    if (!initialized_ ||
        selectedMode_ != device_platform::NetworkMode::HOME_WIFI) {
        return {NetworkConfigurationStatus::NotInitialized};
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
    if (!initialized_ || !candidate_.has_value() ||
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

    // Apply the candidate to the transport before persisting it. A transport
    // failure therefore cannot leave a newly persisted credential behind.
    const auto applied = lifecycle_.start(
        device_platform::NetworkMode::HOME_WIFI, candidate_->homeWifi);
    if (applied.status != device_platform::NetworkOperationStatus::Applied) {
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
        activeCredential_ = *candidate_;
        candidate_.reset();
        return {NetworkConfigurationStatus::Applied};
    }
    const auto status =
        written.status == ConnectivityCredentialWriteStatus::Indeterminate
            ? NetworkConfigurationStatus::CommitIndeterminate
            : NetworkConfigurationStatus::PersistenceFailure;
    static_cast<void>(restoreActiveTransport());
    candidate_.reset();
    return {status};
}

void NetworkConfigurationService::discardCandidate() noexcept {
    candidate_.reset();
}

}  // namespace fermentation
