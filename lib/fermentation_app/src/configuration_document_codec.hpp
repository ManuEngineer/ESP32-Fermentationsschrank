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
    const UserConfiguration& configuration, std::uint32_t schemaVersion,
    const device_platform::ITimeZoneResolver& resolver, std::string& out);

// Compatibility helper for explicit V1 fixtures and V1 semantic
// revalidation. New production writes pass
// kCurrentUserConfigurationSchemaVersion explicitly.
[[nodiscard]] inline ConfigurationCodecStatus encodeUserConfigurationPayload(
    const UserConfiguration& configuration,
    const device_platform::ITimeZoneResolver& resolver, std::string& out) {
    return encodeUserConfigurationPayload(
        configuration,
        static_cast<std::uint32_t>(UserConfigurationSchema::Version1),
        resolver, out);
}

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

// Validates a canonical catalog payload one program at a time. Unlike the
// decoding API this does not retain a second complete ProgramCatalog model and
// is therefore suitable for ValidationOnly graph scans while active and
// preview model generations are already resident.
[[nodiscard]] ConfigurationCodecStatus validateProgramCatalogPayload(
    std::uint32_t schemaVersion, const std::string& payload,
    const ProgramCatalog* expected = nullptr);

}  // namespace fermentation
