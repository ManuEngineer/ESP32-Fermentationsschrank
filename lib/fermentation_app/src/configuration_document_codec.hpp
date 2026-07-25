#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "configuration_documents.hpp"

namespace fermentation {

enum class ConfigurationCodecStatus : std::uint8_t {
    Success,
    InvalidDocument,
    UnsupportedSchema,
    CapacityExceeded,
    Truncated,
    TrailingBytes,
    InvalidWireValue,
    MigrationFailed,
};

template <typename Document>
struct ConfigurationDecodeResult {
    ConfigurationCodecStatus status{ConfigurationCodecStatus::Truncated};
    std::optional<Document> document;
};

[[nodiscard]] ConfigurationCodecStatus encodeUserConfigurationPayload(
    const UserConfiguration& configuration,
    const device_platform::ITimeZoneResolver& resolver, std::string& out);

[[nodiscard]] ConfigurationDecodeResult<UserConfiguration>
decodeUserConfigurationPayload(
    std::uint32_t schemaVersion, const std::string& payload,
    const device_platform::ITimeZoneResolver& resolver);

[[nodiscard]] ConfigurationCodecStatus encodeServiceConfigurationPayload(
    const ServiceConfiguration& configuration, std::string& out);

[[nodiscard]] ConfigurationDecodeResult<ServiceConfiguration>
decodeServiceConfigurationPayload(std::uint32_t schemaVersion,
                                  const std::string& payload);

[[nodiscard]] ConfigurationCodecStatus encodeProgramCatalogPayload(
    const ProgramCatalog& catalog, std::string& out);

[[nodiscard]] ConfigurationDecodeResult<ProgramCatalog>
decodeProgramCatalogPayload(std::uint32_t schemaVersion,
                            const std::string& payload);

}  // namespace fermentation
