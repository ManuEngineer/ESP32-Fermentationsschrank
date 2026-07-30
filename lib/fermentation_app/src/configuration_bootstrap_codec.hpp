#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "configuration_bootstrap.hpp"

namespace fermentation {

inline constexpr std::uint32_t kConfigurationBootstrapSchemaVersion1 = 1U;

enum class ConfigurationBootstrapCodecStatus : std::uint8_t {
    Success,
    InvalidModel,
    CapacityExceeded,
    InvalidEnvelope,
    UnsupportedNewerSchema,
    RecordIdentityMismatch,
};

struct ConfigurationBootstrapDecodeResult {
    ConfigurationBootstrapCodecStatus status{
        ConfigurationBootstrapCodecStatus::InvalidEnvelope};
    std::optional<ConfigurationBootstrapRecord> value;
};

[[nodiscard]] ConfigurationBootstrapCodecStatus
encodeConfigurationBootstrapRecord(const ConfigurationBootstrapRecord& record,
                                   std::string& out);
[[nodiscard]] ConfigurationBootstrapDecodeResult
decodeConfigurationBootstrapRecord(const std::string& bytes);

}  // namespace fermentation
