#include "configuration_graph.hpp"

#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"

namespace fermentation {
namespace {

template <typename Version>
bool validReference(const ConfigurationRecordReference<Version>& reference,
                    device_platform::RecordTypeId expectedRecordType,
                    std::size_t slotCount) {
    return reference.recordType == expectedRecordType &&
           reference.slot.value() < slotCount &&
           reference.version.value() != 0U && reference.schemaVersion != 0U &&
           reference.storageEpoch.value() != 0U;
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
    return validReference(
               manifest.userConfiguration, kUserConfigurationRecordType,
               configuration_limits::kConfigurationDocumentSlotCount) &&
           validReference(
               manifest.serviceConfiguration, kServiceConfigurationRecordType,
               configuration_limits::kConfigurationDocumentSlotCount) &&
           validReference(
               manifest.programCatalog, kProgramCatalogRecordType,
               configuration_limits::kConfigurationDocumentSlotCount) &&
           manifest.userConfiguration.storageEpoch ==
               manifest.serviceConfiguration.storageEpoch &&
           manifest.userConfiguration.storageEpoch ==
               manifest.programCatalog.storageEpoch;
}

bool isPlausible(const ConfigurationRootRecord& root) {
    using namespace configuration_storage_contract;
    if (!validReference(
            root.active, kConfigurationManifestRecordType,
            configuration_limits::kConfigurationManifestSlotCount)) {
        return false;
    }
    if (!root.fallback.has_value()) {
        return true;
    }
    return validReference(
               *root.fallback, kConfigurationManifestRecordType,
               configuration_limits::kConfigurationManifestSlotCount) &&
           root.fallback->storageEpoch == root.active.storageEpoch &&
           *root.fallback != root.active;
}

}  // namespace fermentation
