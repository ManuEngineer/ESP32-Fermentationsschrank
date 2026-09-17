#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "network_lifecycle.hpp"
#include "state_store.hpp"
#include "storage_types.hpp"

namespace fermentation {

// The single productive R1 credential domain. It intentionally does not
// belong to UserConfiguration: the latter contains network-mode selection
// only. An empty record is valid for AP_ONLY and for HOME_WIFI before setup.
struct ConnectivityCredential {
    std::optional<device_platform::NetworkCredentials> homeWifi;

    friend bool operator==(const ConnectivityCredential& left,
                           const ConnectivityCredential& right) {
        return left.homeWifi == right.homeWifi;
    }
    friend bool operator!=(const ConnectivityCredential& left,
                           const ConnectivityCredential& right) {
        return !(left == right);
    }
};

enum class ConnectivityCredentialValidationStatus : std::uint8_t {
    Success,
    InvalidSsid,
    InvalidPassword,
};

[[nodiscard]] ConnectivityCredentialValidationStatus
validateConnectivityCredential(const ConnectivityCredential& credential);

enum class ConnectivityCredentialLoadStatus : std::uint8_t {
    Available,
    NotFound,
    OtherEpoch,
    ReadError,
    CapacityError,
    InvalidRecord,
};

struct ConnectivityCredentialRecord {
    ConnectivityCredential credential;
    device_platform::StorageEpoch storageEpoch;
    std::uint64_t recordSequence{0U};
};

struct ConnectivityCredentialLoadResult {
    ConnectivityCredentialLoadStatus status{
        ConnectivityCredentialLoadStatus::ReadError};
    std::optional<ConnectivityCredentialRecord> record;
};

enum class ConnectivityCredentialWriteStatus : std::uint8_t {
    Committed,
    WriteFailure,
    CapacityFailure,
    Indeterminate,
};

struct ConnectivityCredentialWriteResult {
    ConnectivityCredentialWriteStatus status{
        ConnectivityCredentialWriteStatus::WriteFailure};
    std::uint64_t recordSequence{0U};
};

class ConnectivityCredentialStore final {
   public:
    explicit ConnectivityCredentialStore(device_platform::IStateStore& store)
        : store_(store) {}

    [[nodiscard]] ConnectivityCredentialLoadResult load(
        device_platform::StorageEpoch expectedEpoch) const;

    [[nodiscard]] ConnectivityCredentialWriteResult write(
        const ConnectivityCredential& credential,
        device_platform::StorageEpoch epoch, std::uint64_t recordSequence);

    [[nodiscard]] static const device_platform::StateStoreKey& key() noexcept;

   private:
    device_platform::IStateStore& store_;
};

}  // namespace fermentation
