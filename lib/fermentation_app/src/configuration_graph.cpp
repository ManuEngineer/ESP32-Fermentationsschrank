#include "configuration_graph.hpp"

#include "configuration_graph_codec.hpp"
#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"

namespace fermentation {
namespace {

template <typename Version>
bool validReference(
    const ConfigurationRecordReference<Version>& reference,
    device_platform::RecordTypeId expectedRecordType, std::size_t slotCount,
    std::uint32_t expectedSchema, std::size_t maximumPayloadBytes,
    std::optional<std::size_t> exactPayloadBytes = std::nullopt) {
    return reference.recordType == expectedRecordType &&
           reference.slot.value() < slotCount &&
           reference.version.value() != 0U &&
           reference.schemaVersion == expectedSchema &&
           reference.payloadLength <= maximumPayloadBytes &&
           (!exactPayloadBytes.has_value() ||
            reference.payloadLength == *exactPayloadBytes) &&
           reference.storageEpoch.value() != 0U;
}

bool validChangeOrigin(const ChangeOrigin& origin) {
    switch (origin.kind) {
        case ChangeOriginKind::InternalSystem:
            return origin.wireValue == 1U;
        case ChangeOriginKind::LocalDisplay:
            return origin.wireValue == 2U;
        case ChangeOriginKind::WebInterface:
            return origin.wireValue == 3U;
        case ChangeOriginKind::Unknown:
            return origin.wireValue != 1U && origin.wireValue != 2U &&
                   origin.wireValue != 3U;
    }
    return false;
}

bool validChangeOperation(const ChangeOperation& operation) {
    switch (operation.kind) {
        case ChangeOperationKind::NormalEdit:
            return operation.wireValue == 1U;
        case ChangeOperationKind::FactoryInitialization:
            return operation.wireValue == 2U;
        case ChangeOperationKind::BackupImport:
            return operation.wireValue == 3U;
        case ChangeOperationKind::SchemaMigration:
            return operation.wireValue == 4U;
        case ChangeOperationKind::FactoryReset:
            return operation.wireValue == 5U;
        case ChangeOperationKind::StandardProgramReset:
            return operation.wireValue == 6U;
        case ChangeOperationKind::Unknown:
            return operation.wireValue < 1U || operation.wireValue > 6U;
    }
    return false;
}

}  // namespace

bool operator==(const ChangeOrigin& left, const ChangeOrigin& right) {
    return left.kind == right.kind && left.wireValue == right.wireValue;
}

bool operator==(const ChangeOperation& left, const ChangeOperation& right) {
    return left.kind == right.kind && left.wireValue == right.wireValue;
}

bool operator==(const ConfigurationManifest& left,
                const ConfigurationManifest& right) {
    return left.origin == right.origin && left.operation == right.operation &&
           left.userConfiguration == right.userConfiguration &&
           left.serviceConfiguration == right.serviceConfiguration &&
           left.programCatalog == right.programCatalog;
}

bool operator==(const ConfigurationRootRecord& left,
                const ConfigurationRootRecord& right) {
    return left.active == right.active && left.fallback == right.fallback;
}

ChangeOrigin decodeChangeOrigin(std::uint8_t wireValue) {
    switch (wireValue) {
        case 1U:
            return {ChangeOriginKind::InternalSystem, wireValue};
        case 2U:
            return {ChangeOriginKind::LocalDisplay, wireValue};
        case 3U:
            return {ChangeOriginKind::WebInterface, wireValue};
        default:
            return {ChangeOriginKind::Unknown, wireValue};
    }
}

ChangeOperation decodeChangeOperation(std::uint8_t wireValue) {
    switch (wireValue) {
        case 1U:
            return {ChangeOperationKind::NormalEdit, wireValue};
        case 2U:
            return {ChangeOperationKind::FactoryInitialization, wireValue};
        case 3U:
            return {ChangeOperationKind::BackupImport, wireValue};
        case 4U:
            return {ChangeOperationKind::SchemaMigration, wireValue};
        case 5U:
            return {ChangeOperationKind::FactoryReset, wireValue};
        case 6U:
            return {ChangeOperationKind::StandardProgramReset, wireValue};
        default:
            return {ChangeOperationKind::Unknown, wireValue};
    }
}

bool isPlausible(const ConfigurationManifest& manifest) {
    using namespace configuration_storage_contract;
    return validChangeOrigin(manifest.origin) &&
           validChangeOperation(manifest.operation) &&
           validReference(
               manifest.userConfiguration, kUserConfigurationRecordType,
               configuration_limits::kConfigurationDocumentSlotCount, 1U,
               configuration_limits::kMaximumUserConfigurationPayloadBytes) &&
           validReference(manifest.serviceConfiguration,
                          kServiceConfigurationRecordType,
                          configuration_limits::kConfigurationDocumentSlotCount,
                          1U, 0U, 0U) &&
           validReference(
               manifest.programCatalog, kProgramCatalogRecordType,
               configuration_limits::kConfigurationDocumentSlotCount, 1U,
               configuration_limits::kMaximumProgramCatalogPayloadBytes) &&
           manifest.userConfiguration.storageEpoch ==
               manifest.serviceConfiguration.storageEpoch &&
           manifest.userConfiguration.storageEpoch ==
               manifest.programCatalog.storageEpoch;
}

bool isPlausible(const ConfigurationRootRecord& root) {
    using namespace configuration_storage_contract;
    if (!validReference(
            root.active, kConfigurationManifestRecordType,
            configuration_limits::kConfigurationManifestSlotCount,
            kConfigurationManifestSchemaVersion1,
            configuration_limits::kConfigurationManifestPayloadBytes,
            configuration_limits::kConfigurationManifestPayloadBytes)) {
        return false;
    }
    if (!root.fallback.has_value()) {
        return true;
    }
    return validReference(
               *root.fallback, kConfigurationManifestRecordType,
               configuration_limits::kConfigurationManifestSlotCount,
               kConfigurationManifestSchemaVersion1,
               configuration_limits::kConfigurationManifestPayloadBytes,
               configuration_limits::kConfigurationManifestPayloadBytes) &&
           root.fallback->storageEpoch == root.active.storageEpoch &&
           root.fallback->version != root.active.version;
}

}  // namespace fermentation
