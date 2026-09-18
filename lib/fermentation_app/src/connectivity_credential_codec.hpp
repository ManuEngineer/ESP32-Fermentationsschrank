#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "connectivity_credentials.hpp"

namespace fermentation {

enum class ConnectivityCredentialCodecStatus : std::uint8_t {
    Success,
    InvalidCredential,
    UnsupportedSchema,
    CapacityExceeded,
    Truncated,
    TrailingBytes,
    InvalidWireValue,
};

[[nodiscard]] ConnectivityCredentialCodecStatus
encodeConnectivityCredentialPayload(const ConnectivityCredential& credential,
                                    std::string& out);

struct ConnectivityCredentialDecodeResult {
    ConnectivityCredentialCodecStatus status{
        ConnectivityCredentialCodecStatus::Truncated};
    std::optional<ConnectivityCredential> credential;
};

[[nodiscard]] ConnectivityCredentialDecodeResult
decodeConnectivityCredentialPayload(std::uint32_t schemaVersion,
                                    const std::string& payload);

}  // namespace fermentation
