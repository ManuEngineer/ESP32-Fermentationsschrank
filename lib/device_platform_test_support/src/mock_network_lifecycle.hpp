#pragma once

#include <cstddef>
#include <optional>
#include <vector>

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
    [[nodiscard]] device_platform::NetworkOperationResult setHostname(
        const std::string& hostname) override {
        hostname_ = hostname;
        return {device_platform::NetworkOperationStatus::Applied};
    }
    [[nodiscard]] device_platform::NetworkOperationResult
    setAccessPointCredentials(const std::string& ssid,
                              const std::string& password) override;
    [[nodiscard]] device_platform::NetworkStatus status() const override {
        return status_;
    }
    [[nodiscard]] std::optional<device_platform::NetworkAccessPointInfo>
    accessPointInfo() const override {
        return accessPointInfo_;
    }
    [[nodiscard]] std::uint64_t accessPointInfoRevision()
        const noexcept override {
        return accessPointInfoRevision_;
    }
    void poll() override {}

    void setAccessPointAddress(std::uint32_t address) noexcept {
        accessPointAddress_ = address;
    }
    void setStartStatus(
        device_platform::NetworkOperationStatus status) noexcept {
        startStatus_ = status;
    }
    void setStopStatus(
        device_platform::NetworkOperationStatus status) noexcept {
        stopStatus_ = status;
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
    [[nodiscard]] const std::string& hostname() const noexcept {
        return hostname_;
    }
    [[nodiscard]] const std::string& accessPointSsid() const noexcept {
        return accessPointSsid_;
    }
    [[nodiscard]] const std::string& accessPointPassword() const noexcept {
        return accessPointPassword_;
    }
    [[nodiscard]] std::size_t startCallCount() const noexcept {
        return startHistory_.size();
    }
    [[nodiscard]] std::size_t stopCallCount() const noexcept {
        return stopCallCount_;
    }
    [[nodiscard]] const std::vector<
        std::optional<device_platform::NetworkCredentials>>&
    startHistory() const noexcept {
        return startHistory_;
    }

   private:
    void setAccessPointInfo(
        std::optional<device_platform::NetworkAccessPointInfo> info) {
        if (info != accessPointInfo_) {
            accessPointInfo_ = std::move(info);
            ++accessPointInfoRevision_;
        }
    }

    device_platform::NetworkStatus status_;
    std::optional<device_platform::NetworkAccessPointInfo> accessPointInfo_;
    std::uint64_t accessPointInfoRevision_{0U};
    device_platform::NetworkOperationStatus startStatus_{
        device_platform::NetworkOperationStatus::Applied};
    device_platform::NetworkOperationStatus stopStatus_{
        device_platform::NetworkOperationStatus::Applied};
    device_platform::NetworkOperationStatus candidateStatus_{
        device_platform::NetworkOperationStatus::Applied};
    device_platform::NetworkScanResult scanResult_{
        device_platform::NetworkOperationStatus::Applied, {}};
    std::optional<device_platform::NetworkCredentials> lastStartedCredentials_;
    std::optional<device_platform::NetworkCredentials> lastTestedCandidate_;
    std::string hostname_;
    std::uint32_t accessPointAddress_{0x0104A8C0U};
    std::string accessPointSsid_{"mock-setup-ap"};
    std::string accessPointPassword_{"mock-ap-password"};
    std::vector<std::optional<device_platform::NetworkCredentials>>
        startHistory_;
    std::size_t stopCallCount_{0U};
};

}  // namespace device_platform_test_support
