#pragma once

#include <cstdint>
#include <optional>
#include <memory>
#include <string>

#include "configuration_documents.hpp"
#include "storage_types.hpp"

namespace fermentation {

namespace detail {
struct ConfigurationManifestGenerationTag {};
struct ConfigurationRootSequenceTag {};
}  // namespace detail

using ConfigurationManifestGeneration =
    device_platform::StrongId<detail::ConfigurationManifestGenerationTag,
                              std::uint64_t>;
using ConfigurationRootSequence =
    device_platform::StrongId<detail::ConfigurationRootSequenceTag,
                              std::uint64_t>;

enum class ChangeOriginKind : std::uint8_t {
    InternalSystem,
    LocalDisplay,
    WebInterface,
    Unknown,
};

struct ChangeOrigin {
    ChangeOriginKind kind{ChangeOriginKind::Unknown};
    std::uint8_t wireValue{0U};
};

enum class ChangeOperationKind : std::uint8_t {
    NormalEdit,
    FactoryInitialization,
    BackupImport,
    SchemaMigration,
    FactoryReset,
    StandardProgramReset,
    Unknown,
};

struct ChangeOperation {
    ChangeOperationKind kind{ChangeOperationKind::Unknown};
    std::uint8_t wireValue{0U};
};

template <typename Version>
struct ConfigurationRecordReference {
    device_platform::RecordTypeId recordType;
    device_platform::SlotId slot;
    Version version;
    std::uint32_t schemaVersion{0U};
    std::uint32_t payloadLength{0U};
    std::uint32_t payloadCrc{0U};
    device_platform::StorageEpoch storageEpoch;
};

using UserConfigurationReference =
    ConfigurationRecordReference<UserConfigurationRevision>;
using ServiceConfigurationReference =
    ConfigurationRecordReference<ServiceConfigurationRevision>;
using ProgramCatalogReference =
    ConfigurationRecordReference<ProgramCatalogRevision>;
using ConfigurationManifestReference =
    ConfigurationRecordReference<ConfigurationManifestGeneration>;

struct ConfigurationManifest {
    ChangeOrigin origin;
    ChangeOperation operation;
    UserConfigurationReference userConfiguration;
    ServiceConfigurationReference serviceConfiguration;
    ProgramCatalogReference programCatalog;
};

struct ConfigurationRootRecord {
    ConfigurationManifestReference active;
    std::optional<ConfigurationManifestReference> fallback;
};

struct ConfigurationGraphBranch {
    ConfigurationManifestReference manifestReference;
    ConfigurationManifest manifest;
    std::shared_ptr<const UserConfiguration> userConfiguration;
    std::shared_ptr<const ServiceConfiguration> serviceConfiguration;
    std::shared_ptr<const ProgramCatalog> programCatalog;
    std::string canonicalManifestRecordBytes;
};

struct ConfigurationGraphMetadataBranch {
    ConfigurationManifestReference manifestReference;
    ConfigurationManifest manifest;
    std::string canonicalManifestRecordBytes;
};

struct LoadedConfigurationGraph {
    device_platform::SlotId rootSlot;
    ConfigurationRootSequence rootSequence;
    ConfigurationRootRecord root;
    std::string canonicalRootRecordBytes;
    ConfigurationGraphBranch active;
    std::optional<ConfigurationGraphMetadataBranch> fallback;
    bool selectedFallback{false};
};

[[nodiscard]] bool operator==(const ChangeOrigin& left,
                              const ChangeOrigin& right);
[[nodiscard]] bool operator==(const ChangeOperation& left,
                              const ChangeOperation& right);

template <typename Version>
[[nodiscard]] bool operator==(
    const ConfigurationRecordReference<Version>& left,
    const ConfigurationRecordReference<Version>& right) {
    return left.recordType == right.recordType && left.slot == right.slot &&
           left.version == right.version &&
           left.schemaVersion == right.schemaVersion &&
           left.payloadLength == right.payloadLength &&
           left.payloadCrc == right.payloadCrc &&
           left.storageEpoch == right.storageEpoch;
}

template <typename Version>
[[nodiscard]] bool operator!=(
    const ConfigurationRecordReference<Version>& left,
    const ConfigurationRecordReference<Version>& right) {
    return !(left == right);
}

[[nodiscard]] bool operator==(const ConfigurationManifest& left,
                              const ConfigurationManifest& right);
[[nodiscard]] bool operator==(const ConfigurationRootRecord& left,
                              const ConfigurationRootRecord& right);

[[nodiscard]] ChangeOrigin decodeChangeOrigin(std::uint8_t wireValue);
[[nodiscard]] ChangeOperation decodeChangeOperation(std::uint8_t wireValue);
[[nodiscard]] bool isPlausible(const ConfigurationManifest& manifest);
[[nodiscard]] bool isPlausible(const ConfigurationRootRecord& root);

}  // namespace fermentation
