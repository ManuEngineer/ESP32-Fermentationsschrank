#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "configuration_graph.hpp"

namespace fermentation {

inline constexpr std::uint32_t kConfigurationManifestSchemaVersion1 = 1U;
inline constexpr std::uint32_t kConfigurationRootSchemaVersion1 = 1U;

enum class ConfigurationGraphCodecStatus : std::uint8_t {
    Success,
    InvalidModel,
    CapacityExceeded,
    Truncated,
    TrailingBytes,
    InvalidOptionalTag,
    InvalidEnvelope,
    RecordIdentityMismatch,
    PayloadCrcMismatch,
};

template <typename Model>
struct ConfigurationGraphDecodeResult {
    ConfigurationGraphCodecStatus status{
        ConfigurationGraphCodecStatus::Truncated};
    std::optional<Model> value;
};

[[nodiscard]] ConfigurationGraphCodecStatus encodeConfigurationManifestPayload(
    const ConfigurationManifest& manifest, std::string& out);
[[nodiscard]] ConfigurationGraphDecodeResult<ConfigurationManifest>
decodeConfigurationManifestPayload(const std::string& payload);

[[nodiscard]] ConfigurationGraphCodecStatus encodeConfigurationRootPayload(
    const ConfigurationRootRecord& root, std::string& out);
[[nodiscard]] ConfigurationGraphDecodeResult<ConfigurationRootRecord>
decodeConfigurationRootPayload(const std::string& payload);

[[nodiscard]] ConfigurationGraphCodecStatus encodeConfigurationManifestRecord(
    const ConfigurationManifest& manifest,
    ConfigurationManifestGeneration generation,
    device_platform::StorageEpoch storageEpoch,
    std::optional<std::int64_t> utcUnixSeconds, std::string& out);

[[nodiscard]] ConfigurationGraphCodecStatus encodeConfigurationRootRecord(
    const ConfigurationRootRecord& root, ConfigurationRootSequence sequence,
    device_platform::StorageEpoch storageEpoch,
    std::optional<std::int64_t> utcUnixSeconds, std::string& out);

}  // namespace fermentation
