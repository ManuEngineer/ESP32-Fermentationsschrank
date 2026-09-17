#include "mock_network_lifecycle.hpp"

#include <utility>

namespace device_platform_test_support {

device_platform::NetworkOperationResult MockNetworkLifecycle::start(
    device_platform::NetworkMode mode,
    const std::optional<device_platform::NetworkCredentials>& credentials) {
    if (startStatus_ != device_platform::NetworkOperationStatus::Applied) {
        return {startStatus_};
    }
    startHistory_.push_back(credentials);
    status_.selectedMode = mode;
    status_.state =
        mode == device_platform::NetworkMode::AP_ONLY
            ? device_platform::NetworkLifecycleState::AccessPointOnly
        : credentials.has_value()
            ? device_platform::NetworkLifecycleState::HomeConnected
            : device_platform::NetworkLifecycleState::SetupAccessPoint;
    lastStartedCredentials_ = credentials;
    return {device_platform::NetworkOperationStatus::Applied};
}

device_platform::NetworkOperationResult MockNetworkLifecycle::stop() {
    ++stopCallCount_;
    status_.state = device_platform::NetworkLifecycleState::Stopped;
    status_.httpReady = false;
    status_.ipv4Address.reset();
    return {device_platform::NetworkOperationStatus::Applied};
}

device_platform::NetworkScanResult MockNetworkLifecycle::scan() {
    return scanResult_;
}

device_platform::NetworkOperationResult MockNetworkLifecycle::testCandidate(
    const device_platform::NetworkCredentials& candidate) {
    lastTestedCandidate_ = candidate;
    if (candidateStatus_ == device_platform::NetworkOperationStatus::Applied) {
        status_.state = device_platform::NetworkLifecycleState::HomeConnected;
    }
    return {candidateStatus_};
}

}  // namespace device_platform_test_support
