#pragma once

#include <optional>

#include "network_lifecycle.hpp"

namespace device_platform_test_support {

class MockNetworkLifecycle final : public device_platform::INetworkLifecycle {
   public:
    [[nodiscard]] device_platform::NetworkOperationResult start(
        device_platform::NetworkMode mode,
        const std::optional<device_platform::NetworkCredentials>& credentials)
        override;
    [[nodiscard]] device_platform::NetworkOperationResult stop() override;
    [[nodiscard]] device_platform::NetworkScanResult scan() override;
    [[nodiscard]] device_platform::NetworkOperationResult testCandidate(
        const device_platform::NetworkCredentials& candidate) override;
    [[nodiscard]] device_platform::NetworkStatus status() const override {
        return status_;
    }
    void poll() override {}

    void setStartStatus(
        device_platform::NetworkOperationStatus status) noexcept {
        startStatus_ = status;
    }
    void setScanResult(device_platform::NetworkScanResult result) {
        scanResult_ = std::move(result);
    }
    void setCandidateStatus(
        device_platform::NetworkOperationStatus status) noexcept {
        candidateStatus_ = status;
    }
    [[nodiscard]] const std::optional<device_platform::NetworkCredentials>&
    lastStartedCredentials() const noexcept {
        return lastStartedCredentials_;
    }
    [[nodiscard]] std::optional<device_platform::NetworkCredentials>
    lastTestedCandidate() const {
        return lastTestedCandidate_;
    }

   private:
    device_platform::NetworkStatus status_;
    device_platform::NetworkOperationStatus startStatus_{
        device_platform::NetworkOperationStatus::Applied};
    device_platform::NetworkOperationStatus candidateStatus_{
        device_platform::NetworkOperationStatus::Applied};
    device_platform::NetworkScanResult scanResult_{
        device_platform::NetworkOperationStatus::Applied, {}};
    std::optional<device_platform::NetworkCredentials> lastStartedCredentials_;
    std::optional<device_platform::NetworkCredentials> lastTestedCandidate_;
};

}  // namespace device_platform_test_support
