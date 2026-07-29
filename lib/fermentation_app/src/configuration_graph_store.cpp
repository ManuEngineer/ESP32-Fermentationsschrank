#include "configuration_graph_store.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "configuration_document_codec.hpp"
#include "configuration_graph_codec.hpp"
#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"
#include "crc32.hpp"
#include "state_store_key.hpp"
#include "storage_envelope.hpp"

namespace fermentation {
namespace {

struct StoredRecord {
    device_platform::SlotId slot;
    std::string bytes;
    device_platform::StorageEnvelope envelope;
};

struct MetadataScanResult {
    ConfigurationScanStatus status{ConfigurationScanStatus::Success};
    struct RecordDescriptor {
        device_platform::SlotId slot;
        std::uint64_t versionValue;
        std::uint32_t schemaVersion;
        std::string canonicalRootBytes;
    };
    std::vector<RecordDescriptor> records;
    std::uint64_t highWater{0U};
    bool newerSchema{false};
};

device_platform::StateStoreKey key(const char* value) {
    auto result = device_platform::StateStoreKey::create(value);
    // All call sites use compile-time keys from the validated storage contract.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    return std::move(*result.key);
}

ConfigurationScanStatus mapReadStatus(
    device_platform::StateStoreReadStatus status) {
    return status == device_platform::StateStoreReadStatus::CapacityError
               ? ConfigurationScanStatus::CapacityError
               : ConfigurationScanStatus::ReadError;
}

template <std::size_t N>
MetadataScanResult scanGroupMetadata(
    const device_platform::IStateStore& store,
    const std::array<const char*, N>& keys,
    device_platform::RecordTypeId expectedRecordType,
    std::uint32_t currentSchema, device_platform::StorageEpoch epoch,
    std::size_t maxBytes, ConfigurationGraphDiagnostics& diagnostics) {
    MetadataScanResult result;
    result.records.reserve(N);
    for (std::size_t index = 0U; index < N; ++index) {
        auto read = store.read(key(keys[index]), maxBytes);
        if (read.status == device_platform::StateStoreReadStatus::NotFound) {
            ++diagnostics.notFoundSlots;
            continue;
        }
        if (read.status != device_platform::StateStoreReadStatus::Success) {
            result.status = mapReadStatus(read.status);
            return result;
        }
        const auto decoded =
            device_platform::decodeEnvelopeMetadata(read.value);
        if (!decoded.metadata.has_value()) {
            ++diagnostics.invalidCandidates;
            if (expectedRecordType ==
                configuration_storage_contract::kConfigurationRootRecordType) {
                ++diagnostics.corruptRootSlots;
            }
            continue;
        }
        const auto& metadata = *decoded.metadata;
        if (metadata.storageEpoch != epoch) {
            ++diagnostics.otherEpochSlots;
            continue;
        }
        if (metadata.recordTypeId != expectedRecordType) {
            ++diagnostics.invalidCandidates;
            if (expectedRecordType ==
                configuration_storage_contract::kConfigurationRootRecordType) {
                ++diagnostics.corruptRootSlots;
            }
            continue;
        }
        result.highWater = std::max(result.highWater, metadata.versionValue);
        result.newerSchema =
            result.newerSchema || metadata.schemaVersion > currentSchema;
        if (metadata.schemaVersion > currentSchema) {
            ++diagnostics.unsupportedNewerSchemaSlots;
        }
        const auto existing = std::find_if(
            result.records.begin(), result.records.end(),
            [&metadata](const MetadataScanResult::RecordDescriptor& record) {
                return record.versionValue == metadata.versionValue;
            });
        if (existing != result.records.end()) {
            const auto previous =
                store.read(key(keys[existing->slot.value()]), maxBytes);
            if (previous.status !=
                device_platform::StateStoreReadStatus::Success) {
                result.status = mapReadStatus(previous.status);
                return result;
            }
            if (previous.value != read.value) {
                result.status = ConfigurationScanStatus::
                    PersistentConfigurationIdentityCollision;
                return result;
            }
            ++diagnostics.exactDuplicateRecords;
        }
        result.records.push_back(
            {device_platform::SlotId(static_cast<std::uint32_t>(index)),
             metadata.versionValue, metadata.schemaVersion,
             expectedRecordType == configuration_storage_contract::
                                       kConfigurationRootRecordType
                 ? read.value
                 : std::string{}});
    }
    return result;
}

template <typename Version>
bool referenceMatches(const ConfigurationRecordReference<Version>& reference,
                      const StoredRecord& record) {
    return record.envelope.recordTypeId == reference.recordType &&
           record.slot == reference.slot &&
           record.envelope.versionValue == reference.version.value() &&
           record.envelope.schemaVersion == reference.schemaVersion &&
           record.envelope.storageEpoch == reference.storageEpoch &&
           record.envelope.payload.size() == reference.payloadLength &&
           device_platform::computeCrc32IsoHdlc(record.envelope.payload) ==
               reference.payloadCrc;
}

template <typename Version, std::size_t N>
ConfigurationScanStatus validateStoredReference(
    const device_platform::IStateStore& store,
    const std::array<const char*, N>& keys,
    const ConfigurationRecordReference<Version>& reference,
    std::size_t maxBytes, const std::string* exactBytes = nullptr) {
    if (reference.slot.value() >= N) {
        return ConfigurationScanStatus::ConfigurationGraphReferenceFailure;
    }
    auto read = store.read(key(keys[reference.slot.value()]), maxBytes);
    if (read.status == device_platform::StateStoreReadStatus::NotFound) {
        return ConfigurationScanStatus::ConfigurationGraphReferenceFailure;
    }
    if (read.status != device_platform::StateStoreReadStatus::Success) {
        return mapReadStatus(read.status);
    }
    auto decoded = device_platform::decodeEnvelope(read.value);
    if (!decoded.envelope.has_value()) {
        return ConfigurationScanStatus::ConfigurationGraphEnvelopeOrCrcFailure;
    }
    const StoredRecord record{reference.slot, read.value,
                              std::move(*decoded.envelope)};
    if (!referenceMatches(reference, record) ||
        (exactBytes != nullptr && *exactBytes != read.value)) {
        return ConfigurationScanStatus::ConfigurationGraphReferenceFailure;
    }
    return ConfigurationScanStatus::Success;
}

struct StoredRecordLoadResult {
    ConfigurationScanStatus status{
        ConfigurationScanStatus::ConfigurationGraphReferenceFailure};
    std::optional<StoredRecord> record;
};

template <typename Version, std::size_t N>
StoredRecordLoadResult loadReferencedRecord(
    const device_platform::IStateStore& store,
    const std::array<const char*, N>& keys,
    const ConfigurationRecordReference<Version>& reference,
    std::size_t maxBytes) {
    if (reference.slot.value() >= N) {
        return {ConfigurationScanStatus::ConfigurationGraphReferenceFailure,
                std::nullopt};
    }
    auto read = store.read(key(keys[reference.slot.value()]), maxBytes);
    if (read.status == device_platform::StateStoreReadStatus::NotFound) {
        return {ConfigurationScanStatus::ConfigurationGraphReferenceFailure,
                std::nullopt};
    }
    if (read.status != device_platform::StateStoreReadStatus::Success) {
        return {mapReadStatus(read.status), std::nullopt};
    }
    auto decoded = device_platform::decodeEnvelope(read.value);
    if (!decoded.envelope.has_value()) {
        return {ConfigurationScanStatus::ConfigurationGraphEnvelopeOrCrcFailure,
                std::nullopt};
    }
    StoredRecord record{reference.slot, std::move(read.value),
                        std::move(*decoded.envelope)};
    if (!referenceMatches(reference, record)) {
        return {ConfigurationScanStatus::ConfigurationGraphReferenceFailure,
                std::nullopt};
    }
    return {ConfigurationScanStatus::Success, std::move(record)};
}

ConfigurationScanStatus validateManifestBinding(
    const device_platform::IStateStore& store,
    const ConfigurationManifestReference& reference,
    const ConfigurationManifest& expected,
    const std::string& expectedCanonicalBytes) {
    auto loaded = loadReferencedRecord(
        store, configuration_storage_contract::kConfigurationManifestSlotKeys,
        reference,
        configuration_limits::kMaximumConfigurationManifestEnvelopeBytes);
    if (!loaded.record.has_value()) {
        return loaded.status;
    }
    if (loaded.record->bytes != expectedCanonicalBytes ||
        loaded.record->envelope.schemaVersion !=
            kConfigurationManifestSchemaVersion1) {
        return ConfigurationScanStatus::ConfigurationGraphReferenceFailure;
    }
    const auto decoded =
        decodeConfigurationManifestPayload(loaded.record->envelope.payload);
    if (!decoded.value.has_value() || !(*decoded.value == expected)) {
        return ConfigurationScanStatus::ConfigurationGraphSemanticFailure;
    }
    return ConfigurationScanStatus::Success;
}

ConfigurationScanStatus validateUserReferenceSemantically(
    const device_platform::IStateStore& store,
    const UserConfigurationReference& reference,
    const device_platform::ITimeZoneResolver& resolver,
    const UserConfiguration* expected = nullptr) {
    auto loaded = loadReferencedRecord(
        store, configuration_storage_contract::kUserConfigurationSlotKeys,
        reference,
        configuration_limits::kMaximumUserConfigurationPayloadBytes + 45U);
    if (!loaded.record.has_value()) {
        return loaded.status;
    }
    const auto decoded = decodeUserConfigurationPayload(
        loaded.record->envelope.schemaVersion, loaded.record->envelope.payload,
        resolver);
    if (!decoded.document.has_value()) {
        return ConfigurationScanStatus::ConfigurationGraphSemanticFailure;
    }
    if (expected != nullptr) {
        std::string canonical;
        if (encodeUserConfigurationPayload(*expected, resolver, canonical) !=
                ConfigurationCodecStatus::Success ||
            canonical != loaded.record->envelope.payload) {
            return ConfigurationScanStatus::ConfigurationGraphReferenceFailure;
        }
    }
    return ConfigurationScanStatus::Success;
}

ConfigurationScanStatus validateServiceReferenceSemantically(
    const device_platform::IStateStore& store,
    const ServiceConfigurationReference& reference,
    const ServiceConfiguration* expected = nullptr) {
    auto loaded = loadReferencedRecord(
        store, configuration_storage_contract::kServiceConfigurationSlotKeys,
        reference, 45U);
    if (!loaded.record.has_value()) {
        return loaded.status;
    }
    const auto decoded = decodeServiceConfigurationPayload(
        loaded.record->envelope.schemaVersion, loaded.record->envelope.payload);
    if (!decoded.document.has_value()) {
        return ConfigurationScanStatus::ConfigurationGraphSemanticFailure;
    }
    if (expected != nullptr) {
        std::string canonical;
        if (encodeServiceConfigurationPayload(*expected, canonical) !=
                ConfigurationCodecStatus::Success ||
            canonical != loaded.record->envelope.payload) {
            return ConfigurationScanStatus::ConfigurationGraphReferenceFailure;
        }
    }
    return ConfigurationScanStatus::Success;
}

ConfigurationScanStatus validateProgramReferenceSemantically(
    const device_platform::IStateStore& store,
    const ProgramCatalogReference& reference,
    const ProgramCatalog* expected = nullptr) {
    auto loaded = loadReferencedRecord(
        store, configuration_storage_contract::kProgramCatalogSlotKeys,
        reference,
        configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U);
    if (!loaded.record.has_value()) {
        return loaded.status;
    }
    std::string{}.swap(loaded.record->bytes);
    if (validateProgramCatalogPayload(loaded.record->envelope.schemaVersion,
                                      loaded.record->envelope.payload,
                                      expected) !=
        ConfigurationCodecStatus::Success) {
        return ConfigurationScanStatus::ConfigurationGraphSemanticFailure;
    }
    return ConfigurationScanStatus::Success;
}

ConfigurationScanStatus validateBranchSemantically(
    const device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& resolver,
    const ConfigurationGraphMetadataBranch& branch) {
    auto status = validateManifestBinding(store, branch.manifestReference,
                                          branch.manifest,
                                          branch.canonicalManifestRecordBytes);
    if (status != ConfigurationScanStatus::Success) {
        return status;
    }
    status = validateUserReferenceSemantically(
        store, branch.manifest.userConfiguration, resolver);
    if (status != ConfigurationScanStatus::Success) {
        return status;
    }
    status = validateServiceReferenceSemantically(
        store, branch.manifest.serviceConfiguration);
    if (status != ConfigurationScanStatus::Success) {
        return status;
    }
    return validateProgramReferenceSemantically(store,
                                                branch.manifest.programCatalog);
}

ConfigurationScanStatus validateBranchSemantically(
    const device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& resolver,
    const ConfigurationGraphBranch& branch) {
    auto status = validateManifestBinding(store, branch.manifestReference,
                                          branch.manifest,
                                          branch.canonicalManifestRecordBytes);
    if (status != ConfigurationScanStatus::Success) {
        return status;
    }
    status = validateUserReferenceSemantically(
        store, branch.manifest.userConfiguration, resolver,
        branch.userConfiguration.get());
    if (status != ConfigurationScanStatus::Success) {
        return status;
    }
    status = validateServiceReferenceSemantically(
        store, branch.manifest.serviceConfiguration,
        branch.serviceConfiguration.get());
    if (status != ConfigurationScanStatus::Success) {
        return status;
    }
    return validateProgramReferenceSemantically(
        store, branch.manifest.programCatalog, branch.programCatalog.get());
}

struct SemanticallyLoadedMetadataBranchResult {
    ConfigurationScanStatus status{
        ConfigurationScanStatus::ConfigurationGraphIntegrityFailure};
    std::optional<ConfigurationGraphMetadataBranch> branch;
};

SemanticallyLoadedMetadataBranchResult loadMetadataBranchSemantically(
    const ConfigurationManifestReference& reference,
    const device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& resolver) {
    auto manifestRecord = loadReferencedRecord(
        store, configuration_storage_contract::kConfigurationManifestSlotKeys,
        reference,
        configuration_limits::kMaximumConfigurationManifestEnvelopeBytes);
    if (!manifestRecord.record.has_value()) {
        return {manifestRecord.status, std::nullopt};
    }
    if (manifestRecord.record->envelope.schemaVersion !=
        kConfigurationManifestSchemaVersion1) {
        return {};
    }
    const auto decoded = decodeConfigurationManifestPayload(
        manifestRecord.record->envelope.payload);
    if (!decoded.value.has_value()) {
        return {};
    }
    ConfigurationGraphMetadataBranch branch{reference, *decoded.value,
                                            manifestRecord.record->bytes};
    const auto status = validateBranchSemantically(store, resolver, branch);
    if (status != ConfigurationScanStatus::Success) {
        return {status, std::nullopt};
    }
    return {ConfigurationScanStatus::Success, std::move(branch)};
}

struct BoundRootReadResult {
    ConfigurationScanStatus status{
        ConfigurationScanStatus::ConfigurationGraphIntegrityFailure};
    std::optional<ConfigurationRootRecord> root;
    std::string canonicalBytes;
    bool descriptorStable{false};
};

BoundRootReadResult readBoundRootDescriptor(
    const device_platform::IStateStore& store,
    const MetadataScanResult::RecordDescriptor& descriptor,
    device_platform::StorageEpoch epoch) {
    const auto read = store.read(
        key(configuration_storage_contract::kConfigurationRootSlotKeys
                [descriptor.slot.value()]),
        configuration_limits::kMaximumConfigurationRootEnvelopeBytes);
    if (read.status != device_platform::StateStoreReadStatus::Success) {
        return {mapReadStatus(read.status), std::nullopt, {}, false};
    }
    if (!descriptor.canonicalRootBytes.empty() &&
        read.value != descriptor.canonicalRootBytes) {
        return {ConfigurationScanStatus::ConfigurationGraphIntegrityFailure,
                std::nullopt,
                {},
                false};
    }
    const auto decodedEnvelope = device_platform::decodeEnvelope(read.value);
    if (!decodedEnvelope.envelope.has_value()) {
        return {ConfigurationScanStatus::ConfigurationGraphIntegrityFailure,
                std::nullopt,
                {},
                true};
    }
    const auto& envelope = *decodedEnvelope.envelope;
    if (envelope.recordTypeId !=
            configuration_storage_contract::kConfigurationRootRecordType ||
        envelope.storageEpoch != epoch ||
        envelope.versionValue != descriptor.versionValue ||
        envelope.schemaVersion != descriptor.schemaVersion) {
        return {};
    }
    if (descriptor.schemaVersion != kConfigurationRootSchemaVersion1) {
        return {ConfigurationScanStatus::UnsupportedNewerConfigurationSchema,
                std::nullopt,
                {},
                true};
    }
    const auto decodedRoot = decodeConfigurationRootPayload(envelope.payload);
    if (!decodedRoot.value.has_value()) {
        return {ConfigurationScanStatus::ConfigurationGraphIntegrityFailure,
                std::nullopt,
                {},
                true};
    }
    return {ConfigurationScanStatus::Success, *decodedRoot.value, read.value,
            true};
}

struct LoadedBranchParts {
    ConfigurationManifest manifest;
    std::string canonicalManifestRecordBytes;
    std::shared_ptr<const UserConfiguration> userConfiguration;
    std::shared_ptr<const ServiceConfiguration> serviceConfiguration;
    std::shared_ptr<const ProgramCatalog> programCatalog;
};

struct LoadedBranchPartsResult {
    ConfigurationScanStatus status{
        ConfigurationScanStatus::ConfigurationGraphIntegrityFailure};
    std::optional<LoadedBranchParts> parts;
};

LoadedBranchPartsResult loadBranchParts(
    const ConfigurationManifestReference& reference,
    const device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& resolver) {
    auto manifestRecord = loadReferencedRecord(
        store, configuration_storage_contract::kConfigurationManifestSlotKeys,
        reference,
        configuration_limits::kMaximumConfigurationManifestEnvelopeBytes);
    if (!manifestRecord.record.has_value()) {
        return {manifestRecord.status, std::nullopt};
    }
    if (manifestRecord.record->envelope.schemaVersion !=
        kConfigurationManifestSchemaVersion1) {
        return {};
    }
    auto decodedManifest = decodeConfigurationManifestPayload(
        manifestRecord.record->envelope.payload);
    if (!decodedManifest.value.has_value()) {
        return {};
    }
    const auto manifest = *decodedManifest.value;
    auto manifestBytes = std::move(manifestRecord.record->bytes);
    manifestRecord.record.reset();

    std::shared_ptr<const UserConfiguration> user;
    {
        auto record = loadReferencedRecord(
            store, configuration_storage_contract::kUserConfigurationSlotKeys,
            manifest.userConfiguration,
            configuration_limits::kMaximumUserConfigurationPayloadBytes + 45U);
        if (!record.record.has_value()) {
            return {record.status, std::nullopt};
        }
        auto decoded = decodeUserConfigurationPayload(
            record.record->envelope.schemaVersion,
            record.record->envelope.payload, resolver);
        if (!decoded.document.has_value()) {
            return {};
        }
        user = std::make_shared<const UserConfiguration>(
            std::move(*decoded.document));
    }
    std::shared_ptr<const ServiceConfiguration> service;
    {
        auto record = loadReferencedRecord(
            store,
            configuration_storage_contract::kServiceConfigurationSlotKeys,
            manifest.serviceConfiguration, 45U);
        if (!record.record.has_value()) {
            return {record.status, std::nullopt};
        }
        auto decoded = decodeServiceConfigurationPayload(
            record.record->envelope.schemaVersion,
            record.record->envelope.payload);
        if (!decoded.document.has_value()) {
            return {};
        }
        service =
            std::make_shared<const ServiceConfiguration>(*decoded.document);
    }
    std::shared_ptr<const ProgramCatalog> catalog;
    {
        auto record = loadReferencedRecord(
            store, configuration_storage_contract::kProgramCatalogSlotKeys,
            manifest.programCatalog,
            configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U);
        if (!record.record.has_value()) {
            return {record.status, std::nullopt};
        }
        auto decoded =
            decodeProgramCatalogPayload(record.record->envelope.schemaVersion,
                                        record.record->envelope.payload);
        if (!decoded.document.has_value()) {
            return {};
        }
        catalog = std::make_shared<const ProgramCatalog>(
            std::move(*decoded.document));
    }
    return {
        ConfigurationScanStatus::Success,
        LoadedBranchParts{manifest, std::move(manifestBytes), std::move(user),
                          std::move(service), std::move(catalog)}};
}

struct LoadedBranchResult {
    ConfigurationScanStatus status{
        ConfigurationScanStatus::ConfigurationGraphIntegrityFailure};
    std::optional<ConfigurationGraphBranch> branch;
};

LoadedBranchResult loadBranch(
    const ConfigurationManifestReference& reference,
    const device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& resolver) {
    auto parts = loadBranchParts(reference, store, resolver);
    if (!parts.parts.has_value()) {
        return {parts.status, std::nullopt};
    }
    return {ConfigurationScanStatus::Success,
            ConfigurationGraphBranch{
                reference, parts.parts->manifest,
                std::move(parts.parts->userConfiguration),
                std::move(parts.parts->serviceConfiguration),
                std::move(parts.parts->programCatalog),
                std::move(parts.parts->canonicalManifestRecordBytes)}};
}

struct MetadataBranchResult {
    ConfigurationScanStatus status{
        ConfigurationScanStatus::ConfigurationGraphIntegrityFailure};
    std::optional<ConfigurationGraphMetadataBranch> branch;
};

MetadataBranchResult validateMetadataBranch(
    const ConfigurationManifestReference& reference,
    const device_platform::IStateStore& store,
    const device_platform::ITimeZoneResolver& resolver) {
    auto parts = loadBranchParts(reference, store, resolver);
    if (!parts.parts.has_value()) {
        return {parts.status, std::nullopt};
    }
    return {ConfigurationScanStatus::Success,
            ConfigurationGraphMetadataBranch{
                reference, parts.parts->manifest,
                std::move(parts.parts->canonicalManifestRecordBytes)}};
}

ConfigurationGraphLoadStatus toLoadStatus(ConfigurationScanStatus status,
                                          bool rootGroup) {
    if (status == ConfigurationScanStatus::CapacityError) {
        return rootGroup ? ConfigurationGraphLoadStatus::RootCapacityError
                         : ConfigurationGraphLoadStatus::RecordCapacityError;
    }
    if (status == ConfigurationScanStatus::ReadError) {
        return rootGroup ? ConfigurationGraphLoadStatus::RootReadError
                         : ConfigurationGraphLoadStatus::RecordReadError;
    }
    return ConfigurationGraphLoadStatus::ConfigurationGraphIntegrityFailure;
}

template <typename Version>
bool checkedNext(Version current, Version& out) {
    return device_platform::checkedIncrement(current, out) ==
           device_platform::CheckedIncrementStatus::Success;
}

template <typename T>
const T& requiredPlannedValue(const std::optional<T>& value) {
    // planSlots() establishes this precondition for every enabled change bit.
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    return *value;
}

template <std::size_t N>
std::optional<device_platform::SlotId> chooseSlot(
    device_platform::SlotId after, const std::array<bool, N>& protectedSlots) {
    for (std::size_t offset = 1U; offset <= N; ++offset) {
        const std::size_t candidate = (after.value() + offset) % N;
        if (!protectedSlots[candidate]) {
            return device_platform::SlotId(
                static_cast<std::uint32_t>(candidate));
        }
    }
    return std::nullopt;
}

template <typename Branch>
void protectBranch(const Branch& branch, std::array<bool, 4>& users,
                   std::array<bool, 4>& services, std::array<bool, 4>& catalogs,
                   std::array<bool, 3>& manifests) {
    users[branch.manifest.userConfiguration.slot.value()] = true;
    services[branch.manifest.serviceConfiguration.slot.value()] = true;
    catalogs[branch.manifest.programCatalog.slot.value()] = true;
    manifests[branch.manifestReference.slot.value()] = true;
}

template <typename Version>
bool encodeDocumentRecord(device_platform::RecordTypeId recordType,
                          Version version, device_platform::StorageEpoch epoch,
                          const std::string& payload, std::size_t maxBytes,
                          std::string& out) {
    return device_platform::encodeEnvelope(
               {recordType, 1U, epoch, version.value(), std::nullopt, payload},
               out, maxBytes) == device_platform::EnvelopeEncodeStatus::Success;
}

enum class WriteReadbackStatus : std::uint8_t {
    NewValue,
    OldValue,
    WriteFailure,
    CapacityFailure,
    ReadbackFailure,
};

WriteReadbackStatus writeAndReadBack(device_platform::IStateStore& store,
                                     const char* keyValue,
                                     const std::string& bytes,
                                     std::size_t maxBytes) {
    const auto write = store.write(key(keyValue), bytes);
    if (write == device_platform::StateStoreWriteStatus::WriteError) {
        return WriteReadbackStatus::WriteFailure;
    }
    if (write == device_platform::StateStoreWriteStatus::CapacityError) {
        return WriteReadbackStatus::CapacityFailure;
    }
    const auto read = store.read(key(keyValue), maxBytes);
    if (read.status != device_platform::StateStoreReadStatus::Success) {
        return read.status ==
                       device_platform::StateStoreReadStatus::CapacityError
                   ? WriteReadbackStatus::CapacityFailure
                   : WriteReadbackStatus::ReadbackFailure;
    }
    if (read.value == bytes) {
        return WriteReadbackStatus::NewValue;
    }
    return write == device_platform::StateStoreWriteStatus::CommitOutcomeUnknown
               ? WriteReadbackStatus::OldValue
               : WriteReadbackStatus::ReadbackFailure;
}

ConfigurationCommitExecutionResult mapPreRootWrite(
    WriteReadbackStatus status, ConfigurationCommitFailurePhase phase) {
    if (status == WriteReadbackStatus::CapacityFailure) {
        return {ConfigurationCommitExecutionStatus::CapacityFailure, phase};
    }
    return {ConfigurationCommitExecutionStatus::WriteFailure, phase};
}

ConfigurationCommitResolutionCause mapResolutionCause(
    ConfigurationScanStatus status) {
    if (status == ConfigurationScanStatus::ReadError) {
        return ConfigurationCommitResolutionCause::GraphReadError;
    }
    if (status == ConfigurationScanStatus::CapacityError) {
        return ConfigurationCommitResolutionCause::GraphCapacityError;
    }
    if (status ==
        ConfigurationScanStatus::ConfigurationGraphEnvelopeOrCrcFailure) {
        return ConfigurationCommitResolutionCause::GraphEnvelopeOrCrcFailure;
    }
    if (status == ConfigurationScanStatus::ConfigurationGraphReferenceFailure) {
        return ConfigurationCommitResolutionCause::GraphReferenceFailure;
    }
    if (status == ConfigurationScanStatus::ConfigurationGraphSemanticFailure) {
        return ConfigurationCommitResolutionCause::GraphSemanticFailure;
    }
    if (status == ConfigurationScanStatus::ActiveBasisMismatch) {
        return ConfigurationCommitResolutionCause::AmbiguousRootOutcome;
    }
    if (status ==
        ConfigurationScanStatus::PersistentConfigurationIdentityCollision) {
        return ConfigurationCommitResolutionCause::IdentityCollision;
    }
    if (status ==
        ConfigurationScanStatus::UnsupportedNewerConfigurationSchema) {
        return ConfigurationCommitResolutionCause::UnsupportedNewerSchema;
    }
    return ConfigurationCommitResolutionCause::GraphIntegrityFailure;
}

struct InitialSlotSelection {
    InitialConfigurationPrepareStatus status{
        InitialConfigurationPrepareStatus::NoSafeSlotAvailable};
    std::optional<device_platform::SlotId> slot;
    bool writeRequired{true};
    std::optional<std::string> previousBytes;
};

template <std::size_t N>
InitialSlotSelection selectInitialSlot(
    const device_platform::IStateStore& store,
    const std::array<const char*, N>& keys, device_platform::StorageEpoch epoch,
    device_platform::RecordTypeId recordType, std::uint32_t schemaVersion,
    std::uint64_t versionValue, const std::string& expectedBytes,
    std::size_t maxBytes) {
    std::optional<std::size_t> safeSlot;
    std::optional<std::string> safePrevious;
    for (std::size_t index = 0U; index < N; ++index) {
        auto read = store.read(key(keys[index]), maxBytes);
        if (read.status == device_platform::StateStoreReadStatus::ReadError) {
            return {InitialConfigurationPrepareStatus::PersistenceFailure,
                    std::nullopt, true, std::nullopt};
        }
        if (read.status ==
            device_platform::StateStoreReadStatus::CapacityError) {
            return {InitialConfigurationPrepareStatus::CapacityFailure,
                    std::nullopt, true, std::nullopt};
        }
        if (read.status == device_platform::StateStoreReadStatus::NotFound) {
            if (!safeSlot.has_value()) {
                safeSlot = index;
                safePrevious = std::nullopt;
            }
            continue;
        }
        if (read.value == expectedBytes) {
            return {InitialConfigurationPrepareStatus::Success,
                    device_platform::SlotId{static_cast<std::uint32_t>(index)},
                    false, read.value};
        }
        const auto metadata =
            device_platform::decodeEnvelopeMetadata(read.value);
        if (!metadata.metadata.has_value()) {
            continue;
        }
        if (metadata.metadata->storageEpoch != epoch) {
            if (!safeSlot.has_value()) {
                safeSlot = index;
                safePrevious = std::move(read.value);
            }
            continue;
        }
        if (metadata.metadata->recordTypeId == recordType &&
            metadata.metadata->schemaVersion > schemaVersion) {
            return {InitialConfigurationPrepareStatus::UnsupportedNewerSchema,
                    std::nullopt, true, std::nullopt};
        }
        if (metadata.metadata->recordTypeId == recordType &&
            metadata.metadata->schemaVersion == schemaVersion &&
            metadata.metadata->versionValue == versionValue) {
            return {InitialConfigurationPrepareStatus::IntegrityFailure,
                    std::nullopt, true, std::nullopt};
        }
    }
    if (!safeSlot.has_value()) {
        return {InitialConfigurationPrepareStatus::NoSafeSlotAvailable,
                std::nullopt, true, std::nullopt};
    }
    return {InitialConfigurationPrepareStatus::Success,
            device_platform::SlotId{static_cast<std::uint32_t>(*safeSlot)},
            true, std::move(safePrevious)};
}

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
ConfigurationGraphLoadResult ConfigurationGraphStore::loadCanonicalGraph(
    device_platform::StorageEpoch storageEpoch) const {
    ConfigurationGraphLoadResult result;
    auto roots = scanGroupMetadata(
        store_, configuration_storage_contract::kConfigurationRootSlotKeys,
        configuration_storage_contract::kConfigurationRootRecordType,
        kConfigurationRootSchemaVersion1, storageEpoch,
        configuration_limits::kMaximumConfigurationRootEnvelopeBytes,
        result.diagnostics);
    const auto rootOtherEpochSlots = result.diagnostics.otherEpochSlots;
    const auto corruptRootSlots = result.diagnostics.corruptRootSlots;
    if (roots.status != ConfigurationScanStatus::Success) {
        result.status = toLoadStatus(roots.status, true);
        return result;
    }
    auto users = scanGroupMetadata(
        store_, configuration_storage_contract::kUserConfigurationSlotKeys,
        configuration_storage_contract::kUserConfigurationRecordType, 1U,
        storageEpoch,
        configuration_limits::kMaximumUserConfigurationPayloadBytes + 45U,
        result.diagnostics);
    auto services = scanGroupMetadata(
        store_, configuration_storage_contract::kServiceConfigurationSlotKeys,
        configuration_storage_contract::kServiceConfigurationRecordType, 1U,
        storageEpoch, 45U, result.diagnostics);
    auto catalogs = scanGroupMetadata(
        store_, configuration_storage_contract::kProgramCatalogSlotKeys,
        configuration_storage_contract::kProgramCatalogRecordType, 1U,
        storageEpoch,
        configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U,
        result.diagnostics);
    auto manifests = scanGroupMetadata(
        store_, configuration_storage_contract::kConfigurationManifestSlotKeys,
        configuration_storage_contract::kConfigurationManifestRecordType,
        kConfigurationManifestSchemaVersion1, storageEpoch,
        configuration_limits::kMaximumConfigurationManifestEnvelopeBytes,
        result.diagnostics);
    for (const auto* group : {&users, &services, &catalogs, &manifests}) {
        if (group->status != ConfigurationScanStatus::Success) {
            result.status = toLoadStatus(group->status, false);
            return result;
        }
    }
    if (roots.records.empty()) {
        if (corruptRootSlots != 0U) {
            result.status = ConfigurationGraphLoadStatus::
                ConfigurationGraphIntegrityFailure;
        } else if (rootOtherEpochSlots != 0U) {
            result.status = ConfigurationGraphLoadStatus::
                ConfigurationGraphUnavailableOtherEpoch;
        } else {
            result.status =
                ConfigurationGraphLoadStatus::ConfigurationGraphUnavailable;
        }
        return result;
    }
    std::sort(roots.records.begin(), roots.records.end(),
              [](const MetadataScanResult::RecordDescriptor& left,
                 const MetadataScanResult::RecordDescriptor& right) {
                  if (left.versionValue != right.versionValue) {
                      return left.versionValue > right.versionValue;
                  }
                  return left.slot.value() < right.slot.value();
              });
    if (roots.records.size() > 1U &&
        roots.records[0].versionValue == roots.records[1].versionValue) {
        result.diagnostics.identicalRootTie = true;
    }

    for (const auto& rootDescriptor : roots.records) {
        if (rootDescriptor.schemaVersion != kConfigurationRootSchemaVersion1) {
            ++result.diagnostics.invalidCandidates;
            continue;
        }
        auto rootRead =
            readBoundRootDescriptor(store_, rootDescriptor, storageEpoch);
        if (rootRead.status == ConfigurationScanStatus::ReadError ||
            rootRead.status == ConfigurationScanStatus::CapacityError) {
            result.status = toLoadStatus(rootRead.status, true);
            return result;
        }
        if (!rootRead.descriptorStable) {
            result.status = ConfigurationGraphLoadStatus::
                ConfigurationGraphIntegrityFailure;
            return result;
        }
        if (rootRead.status ==
            ConfigurationScanStatus::UnsupportedNewerConfigurationSchema) {
            ++result.diagnostics.invalidCandidates;
            continue;
        }
        if (!rootRead.root.has_value()) {
            ++result.diagnostics.invalidCandidates;
            continue;
        }
        const auto root = *rootRead.root;
        auto canonicalRootBytes = std::move(rootRead.canonicalBytes);
        auto activeResult = loadBranch(root.active, store_, timeZoneResolver_);
        if (activeResult.status == ConfigurationScanStatus::ReadError ||
            activeResult.status == ConfigurationScanStatus::CapacityError) {
            result.status = toLoadStatus(activeResult.status, false);
            return result;
        }
        auto active = std::move(activeResult.branch);
        std::optional<ConfigurationGraphMetadataBranch> fallback;
        if (root.fallback.has_value()) {
            if (active.has_value()) {
                auto fallbackResult = validateMetadataBranch(
                    *root.fallback, store_, timeZoneResolver_);
                if (fallbackResult.status ==
                        ConfigurationScanStatus::ReadError ||
                    fallbackResult.status ==
                        ConfigurationScanStatus::CapacityError) {
                    result.status = toLoadStatus(fallbackResult.status, false);
                    return result;
                }
                fallback = std::move(fallbackResult.branch);
            } else {
                auto promotedResult =
                    loadBranch(*root.fallback, store_, timeZoneResolver_);
                if (promotedResult.status ==
                        ConfigurationScanStatus::ReadError ||
                    promotedResult.status ==
                        ConfigurationScanStatus::CapacityError) {
                    result.status = toLoadStatus(promotedResult.status, false);
                    return result;
                }
                active = std::move(promotedResult.branch);
            }
        }
        bool selectedFallback = false;
        if (active.has_value() && !fallback.has_value() &&
            root.fallback.has_value() &&
            active->manifestReference == *root.fallback) {
            selectedFallback = true;
            result.diagnostics.fallbackUsed = true;
        } else if (active.has_value() && root.fallback.has_value() &&
                   !fallback.has_value()) {
            result.diagnostics.unusableFallback = true;
        }
        if (!active.has_value()) {
            ++result.diagnostics.invalidCandidates;
            ++result.diagnostics.skippedHigherRoots;
            continue;
        }
        result.status =
            ConfigurationGraphLoadStatus::ConfigurationGraphAvailable;
        result.graph = LoadedConfigurationGraph{
            rootDescriptor.slot,
            ConfigurationRootSequence(rootDescriptor.versionValue),
            root,
            std::move(canonicalRootBytes),
            std::move(*active),
            std::move(fallback),
            selectedFallback};
        return result;
    }
    result.status =
        roots.newerSchema
            ? ConfigurationGraphLoadStatus::UnsupportedNewerConfigurationSchema
            : (result.diagnostics.invalidCandidates == 0U
                   ? ConfigurationGraphLoadStatus::ConfigurationGraphUnavailable
                   : ConfigurationGraphLoadStatus::
                         ConfigurationGraphIntegrityFailure);
    return result;
}

ConfigurationValidationScanResult ConfigurationGraphStore::validationScan(
    const LoadedConfigurationGraph& expectedActive) const {
    ConfigurationValidationScanResult result;
    const auto epoch = expectedActive.active.manifestReference.storageEpoch;
    auto roots = scanGroupMetadata(
        store_, configuration_storage_contract::kConfigurationRootSlotKeys,
        configuration_storage_contract::kConfigurationRootRecordType,
        kConfigurationRootSchemaVersion1, epoch,
        configuration_limits::kMaximumConfigurationRootEnvelopeBytes,
        result.diagnostics);
    auto users = scanGroupMetadata(
        store_, configuration_storage_contract::kUserConfigurationSlotKeys,
        configuration_storage_contract::kUserConfigurationRecordType, 1U, epoch,
        configuration_limits::kMaximumUserConfigurationPayloadBytes + 45U,
        result.diagnostics);
    auto services = scanGroupMetadata(
        store_, configuration_storage_contract::kServiceConfigurationSlotKeys,
        configuration_storage_contract::kServiceConfigurationRecordType, 1U,
        epoch, 45U, result.diagnostics);
    auto catalogs = scanGroupMetadata(
        store_, configuration_storage_contract::kProgramCatalogSlotKeys,
        configuration_storage_contract::kProgramCatalogRecordType, 1U, epoch,
        configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U,
        result.diagnostics);
    auto manifests = scanGroupMetadata(
        store_, configuration_storage_contract::kConfigurationManifestSlotKeys,
        configuration_storage_contract::kConfigurationManifestRecordType,
        kConfigurationManifestSchemaVersion1, epoch,
        configuration_limits::kMaximumConfigurationManifestEnvelopeBytes,
        result.diagnostics);
    for (const auto* group :
         {&roots, &users, &services, &catalogs, &manifests}) {
        if (group->status != ConfigurationScanStatus::Success) {
            result.status = group->status;
            return result;
        }
        if (group->newerSchema) {
            result.status =
                ConfigurationScanStatus::UnsupportedNewerConfigurationSchema;
            return result;
        }
    }
    if (result.diagnostics.corruptRootSlots != 0U) {
        result.status =
            ConfigurationScanStatus::ConfigurationGraphIntegrityFailure;
        return result;
    }
    std::sort(roots.records.begin(), roots.records.end(),
              [](const MetadataScanResult::RecordDescriptor& left,
                 const MetadataScanResult::RecordDescriptor& right) {
                  if (left.versionValue != right.versionValue) {
                      return left.versionValue > right.versionValue;
                  }
                  return left.slot.value() < right.slot.value();
              });

    bool expectedRootFound = false;
    for (const auto& descriptor : roots.records) {
        const bool isExpected =
            descriptor.slot == expectedActive.rootSlot &&
            descriptor.versionValue == expectedActive.rootSequence.value();
        auto bound = readBoundRootDescriptor(store_, descriptor, epoch);
        if (bound.status == ConfigurationScanStatus::ReadError ||
            bound.status == ConfigurationScanStatus::CapacityError) {
            result.status = bound.status;
            return result;
        }
        if (descriptor.versionValue > expectedActive.rootSequence.value()) {
            // A higher technically and structurally valid Root is enough to
            // invalidate the captured basis. Loading its graph here would
            // create an unbudgeted third full model.
            if (!bound.descriptorStable || bound.root.has_value()) {
                result.status = ConfigurationScanStatus::ActiveBasisMismatch;
                return result;
            }
            continue;
        }
        if (!isExpected) {
            if (descriptor.versionValue ==
                    expectedActive.rootSequence.value() &&
                bound.root.has_value() &&
                bound.canonicalBytes !=
                    expectedActive.canonicalRootRecordBytes) {
                result.status = ConfigurationScanStatus::
                    PersistentConfigurationIdentityCollision;
                return result;
            }
            continue;
        }
        expectedRootFound = true;
        if (!bound.root.has_value() ||
            bound.canonicalBytes != expectedActive.canonicalRootRecordBytes ||
            !(*bound.root == expectedActive.root)) {
            result.status = ConfigurationScanStatus::ActiveBasisMismatch;
            return result;
        }
    }
    if (!expectedRootFound) {
        result.status = ConfigurationScanStatus::ActiveBasisMismatch;
        return result;
    }

    if (expectedActive.selectedFallback) {
        if (!expectedActive.root.fallback.has_value() ||
            *expectedActive.root.fallback !=
                expectedActive.active.manifestReference) {
            result.status =
                ConfigurationScanStatus::ConfigurationGraphIntegrityFailure;
            return result;
        }
        auto formerActive = loadMetadataBranchSemantically(
            expectedActive.root.active, store_, timeZoneResolver_);
        if (formerActive.status == ConfigurationScanStatus::ReadError ||
            formerActive.status == ConfigurationScanStatus::CapacityError) {
            result.status = formerActive.status;
            return result;
        }
        if (formerActive.branch.has_value()) {
            // The formerly unusable Active branch has become usable and is
            // now selected by the canonical algorithm.
            result.status = ConfigurationScanStatus::ActiveBasisMismatch;
            return result;
        }
    } else if (expectedActive.root.active !=
               expectedActive.active.manifestReference) {
        result.status = ConfigurationScanStatus::ActiveBasisMismatch;
        return result;
    }

    result.status = validateBranchSemantically(store_, timeZoneResolver_,
                                               expectedActive.active);
    if (result.status != ConfigurationScanStatus::Success) {
        return result;
    }
    if (expectedActive.root.fallback.has_value() &&
        !expectedActive.selectedFallback) {
        if (!expectedActive.fallback.has_value() ||
            expectedActive.fallback->manifestReference !=
                *expectedActive.root.fallback) {
            result.status =
                ConfigurationScanStatus::ConfigurationGraphIntegrityFailure;
            return result;
        }
        result.status = validateBranchSemantically(store_, timeZoneResolver_,
                                                   *expectedActive.fallback);
        if (result.status != ConfigurationScanStatus::Success) {
            return result;
        }
    } else if (expectedActive.fallback.has_value()) {
        result.status =
            ConfigurationScanStatus::ConfigurationGraphIntegrityFailure;
        return result;
    }
    result.highWater = {UserConfigurationRevision(users.highWater),
                        ServiceConfigurationRevision(services.highWater),
                        ProgramCatalogRevision(catalogs.highWater),
                        ConfigurationManifestGeneration(manifests.highWater),
                        ConfigurationRootSequence(roots.highWater)};
    result.status = ConfigurationScanStatus::Success;
    return result;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
ConfigurationSlotPlanResult ConfigurationGraphStore::planSlots(
    const LoadedConfigurationGraph& current,
    const ConfigurationHighWaterMarks& highWater,
    ConfigurationChangeMask changes) {
    std::array<bool, 4> users{};
    std::array<bool, 4> services{};
    std::array<bool, 4> catalogs{};
    std::array<bool, 3> manifests{};
    protectBranch(current.active, users, services, catalogs, manifests);
    if (current.fallback.has_value()) {
        protectBranch(*current.fallback, users, services, catalogs, manifests);
    }

    ConfigurationSlotPlan plan;
    if (changes.userConfiguration) {
        plan.userConfigurationSlot =
            chooseSlot(current.active.manifest.userConfiguration.slot, users);
        UserConfigurationRevision next;
        if (!plan.userConfigurationSlot.has_value() ||
            !checkedNext(highWater.userConfiguration, next)) {
            return {
                plan.userConfigurationSlot.has_value()
                    ? ConfigurationSlotPlanStatus::HighWaterOverflow
                    : ConfigurationSlotPlanStatus::NoUnreferencedSlotAvailable,
                std::nullopt};
        }
        plan.userConfigurationRevision = next;
        users[plan.userConfigurationSlot->value()] = true;
    }
    if (changes.serviceConfiguration) {
        plan.serviceConfigurationSlot = chooseSlot(
            current.active.manifest.serviceConfiguration.slot, services);
        ServiceConfigurationRevision next;
        if (!plan.serviceConfigurationSlot.has_value() ||
            !checkedNext(highWater.serviceConfiguration, next)) {
            return {
                plan.serviceConfigurationSlot.has_value()
                    ? ConfigurationSlotPlanStatus::HighWaterOverflow
                    : ConfigurationSlotPlanStatus::NoUnreferencedSlotAvailable,
                std::nullopt};
        }
        plan.serviceConfigurationRevision = next;
        services[plan.serviceConfigurationSlot->value()] = true;
    }
    if (changes.programCatalog) {
        plan.programCatalogSlot =
            chooseSlot(current.active.manifest.programCatalog.slot, catalogs);
        ProgramCatalogRevision next;
        if (!plan.programCatalogSlot.has_value() ||
            !checkedNext(highWater.programCatalog, next)) {
            return {
                plan.programCatalogSlot.has_value()
                    ? ConfigurationSlotPlanStatus::HighWaterOverflow
                    : ConfigurationSlotPlanStatus::NoUnreferencedSlotAvailable,
                std::nullopt};
        }
        plan.programCatalogRevision = next;
        catalogs[plan.programCatalogSlot->value()] = true;
    }
    auto manifestSlot =
        chooseSlot(current.active.manifestReference.slot, manifests);
    ConfigurationManifestGeneration nextManifest;
    ConfigurationRootSequence nextRoot;
    if (!manifestSlot.has_value()) {
        return {ConfigurationSlotPlanStatus::NoUnreferencedSlotAvailable,
                std::nullopt};
    }
    if (!checkedNext(highWater.manifest, nextManifest) ||
        !checkedNext(highWater.root, nextRoot)) {
        return {ConfigurationSlotPlanStatus::HighWaterOverflow, std::nullopt};
    }
    plan.manifestSlot = *manifestSlot;
    plan.rootSlot =
        device_platform::SlotId(current.rootSlot.value() == 0U ? 1U : 0U);
    plan.manifestGeneration = nextManifest;
    plan.rootSequence = nextRoot;
    return {ConfigurationSlotPlanStatus::Success, plan};
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
ConfigurationCommitPrepareResult ConfigurationGraphStore::prepareCommit(
    const LoadedConfigurationGraph& current,
    const ConfigurationCommitCandidate& candidate, ChangeOrigin origin,
    ChangeOperation operation) const {
    if (!candidate.userConfiguration || !candidate.serviceConfiguration ||
        !candidate.programCatalog) {
        return {ConfigurationCommitPrepareStatus::InvalidCandidate,
                std::nullopt};
    }
    auto userValidation = validateUserConfiguration(
        *candidate.userConfiguration, timeZoneResolver_);
    if (userValidation.status != UserConfigurationStatus::Success ||
        validateProgramCatalog(*candidate.programCatalog) !=
            ProgramCatalogStatus::Success) {
        return {ConfigurationCommitPrepareStatus::InvalidCandidate,
                std::nullopt};
    }
    const ConfigurationChangeMask changes{
        !configurationContentEquals(*candidate.userConfiguration,
                                    *current.active.userConfiguration),
        !configurationContentEquals(*candidate.serviceConfiguration,
                                    *current.active.serviceConfiguration),
        !configurationContentEquals(*candidate.programCatalog,
                                    *current.active.programCatalog)};
    if (!changes.userConfiguration && !changes.serviceConfiguration &&
        !changes.programCatalog) {
        return {ConfigurationCommitPrepareStatus::InvalidCandidate,
                std::nullopt};
    }
    const auto scan = validationScan(current);
    if (scan.status != ConfigurationScanStatus::Success) {
        ConfigurationCommitPrepareStatus status =
            ConfigurationCommitPrepareStatus::PersistenceFailure;
        if (scan.status == ConfigurationScanStatus::ActiveBasisMismatch) {
            status = ConfigurationCommitPrepareStatus::Conflict;
        } else if (scan.status == ConfigurationScanStatus::
                                      UnsupportedNewerConfigurationSchema) {
            status = ConfigurationCommitPrepareStatus::UnsupportedNewerSchema;
        } else if (scan.status == ConfigurationScanStatus::HighWaterOverflow) {
            status = ConfigurationCommitPrepareStatus::HighWaterOverflow;
        } else if (
            scan.status ==
                ConfigurationScanStatus::ConfigurationGraphIntegrityFailure ||
            scan.status == ConfigurationScanStatus::
                               ConfigurationGraphEnvelopeOrCrcFailure ||
            scan.status ==
                ConfigurationScanStatus::ConfigurationGraphReferenceFailure ||
            scan.status ==
                ConfigurationScanStatus::ConfigurationGraphSemanticFailure) {
            status = ConfigurationCommitPrepareStatus::IntegrityFailure;
        } else if (scan.status ==
                   ConfigurationScanStatus::
                       PersistentConfigurationIdentityCollision) {
            status = ConfigurationCommitPrepareStatus::IdentityCollision;
        } else if (scan.status == ConfigurationScanStatus::CapacityError) {
            status = ConfigurationCommitPrepareStatus::CapacityFailure;
        }
        return {status, std::nullopt};
    }
    auto slotPlan = planSlots(current, scan.highWater, changes);
    if (!slotPlan.plan.has_value()) {
        return {
            slotPlan.status == ConfigurationSlotPlanStatus::HighWaterOverflow
                ? ConfigurationCommitPrepareStatus::HighWaterOverflow
                : ConfigurationCommitPrepareStatus::NoUnreferencedSlotAvailable,
            std::nullopt};
    }
    auto plan = *slotPlan.plan;
    const auto epoch = current.active.manifestReference.storageEpoch;
    auto userReference = current.active.manifest.userConfiguration;
    auto serviceReference = current.active.manifest.serviceConfiguration;
    auto programReference = current.active.manifest.programCatalog;
    std::string payload;
    if (changes.userConfiguration) {
        if (encodeUserConfigurationPayload(*candidate.userConfiguration,
                                           timeZoneResolver_, payload) !=
            ConfigurationCodecStatus::Success) {
            return {ConfigurationCommitPrepareStatus::InvalidCandidate,
                    std::nullopt};
        }
        userReference = {
            configuration_storage_contract::kUserConfigurationRecordType,
            requiredPlannedValue(plan.userConfigurationSlot),
            requiredPlannedValue(plan.userConfigurationRevision),
            1U,
            static_cast<std::uint32_t>(payload.size()),
            device_platform::computeCrc32IsoHdlc(payload),
            epoch};
        payload.clear();
    }
    if (changes.serviceConfiguration) {
        if (encodeServiceConfigurationPayload(*candidate.serviceConfiguration,
                                              payload) !=
            ConfigurationCodecStatus::Success) {
            return {ConfigurationCommitPrepareStatus::InvalidCandidate,
                    std::nullopt};
        }
        serviceReference = {
            configuration_storage_contract::kServiceConfigurationRecordType,
            requiredPlannedValue(plan.serviceConfigurationSlot),
            requiredPlannedValue(plan.serviceConfigurationRevision),
            1U,
            static_cast<std::uint32_t>(payload.size()),
            device_platform::computeCrc32IsoHdlc(payload),
            epoch};
        payload.clear();
    }
    if (changes.programCatalog) {
        if (encodeProgramCatalogPayload(*candidate.programCatalog, payload) !=
            ConfigurationCodecStatus::Success) {
            return {ConfigurationCommitPrepareStatus::InvalidCandidate,
                    std::nullopt};
        }
        programReference = {
            configuration_storage_contract::kProgramCatalogRecordType,
            requiredPlannedValue(plan.programCatalogSlot),
            requiredPlannedValue(plan.programCatalogRevision),
            1U,
            static_cast<std::uint32_t>(payload.size()),
            device_platform::computeCrc32IsoHdlc(payload),
            epoch};
        payload.clear();
    }
    const ConfigurationManifest manifest{origin, operation, userReference,
                                         serviceReference, programReference};
    std::string manifestPayload;
    if (encodeConfigurationManifestPayload(manifest, manifestPayload) !=
        ConfigurationGraphCodecStatus::Success) {
        return {ConfigurationCommitPrepareStatus::InvalidCandidate,
                std::nullopt};
    }
    const ConfigurationManifestReference manifestReference{
        configuration_storage_contract::kConfigurationManifestRecordType,
        plan.manifestSlot,
        plan.manifestGeneration,
        kConfigurationManifestSchemaVersion1,
        static_cast<std::uint32_t>(manifestPayload.size()),
        device_platform::computeCrc32IsoHdlc(manifestPayload),
        epoch};
    std::string manifestRecord;
    if (encodeConfigurationManifestRecord(
            manifest, plan.manifestGeneration, epoch, std::nullopt,
            manifestRecord) != ConfigurationGraphCodecStatus::Success) {
        return {ConfigurationCommitPrepareStatus::CapacityFailure,
                std::nullopt};
    }
    const ConfigurationRootRecord root{manifestReference,
                                       current.active.manifestReference};
    std::string rootRecord;
    if (encodeConfigurationRootRecord(root, plan.rootSequence, epoch,
                                      std::nullopt, rootRecord) !=
        ConfigurationGraphCodecStatus::Success) {
        return {ConfigurationCommitPrepareStatus::CapacityFailure,
                std::nullopt};
    }
    const auto previousTarget = store_.read(
        key(configuration_storage_contract::kConfigurationRootSlotKeys
                [plan.rootSlot.value()]),
        configuration_limits::kMaximumConfigurationRootEnvelopeBytes);
    std::optional<std::string> previousTargetBytes;
    if (previousTarget.status ==
        device_platform::StateStoreReadStatus::Success) {
        previousTargetBytes = previousTarget.value;
    } else if (previousTarget.status !=
               device_platform::StateStoreReadStatus::NotFound) {
        return {previousTarget.status ==
                        device_platform::StateStoreReadStatus::CapacityError
                    ? ConfigurationCommitPrepareStatus::CapacityFailure
                    : ConfigurationCommitPrepareStatus::PersistenceFailure,
                std::nullopt};
    }
    ConfigurationGraphBranch newActive{
        manifestReference,           manifest,
        candidate.userConfiguration, candidate.serviceConfiguration,
        candidate.programCatalog,    manifestRecord};
    ConfigurationGraphMetadataBranch previousActive{
        current.active.manifestReference, current.active.manifest,
        current.active.canonicalManifestRecordBytes};
    LoadedConfigurationGraph newGraph{
        plan.rootSlot,        plan.rootSequence,         root, rootRecord,
        std::move(newActive), std::move(previousActive), false};
    return {ConfigurationCommitPrepareStatus::Success,
            PreparedConfigurationCommit{current, std::move(newGraph), changes,
                                        plan, std::move(manifestRecord),
                                        std::move(rootRecord),
                                        std::move(previousTargetBytes)}};
}

ConfigurationCommitExecutionResult ConfigurationGraphStore::
    executePreparedCommit(  // NOLINT(readability-function-cognitive-complexity)
        PreparedConfigurationCommit& prepared) {
    std::string payload;
    std::string record;
    const auto writeRecord = [this, &record](
                                 const char* keyValue, std::size_t maxBytes,
                                 ConfigurationCommitFailurePhase phase)
        -> std::optional<ConfigurationCommitExecutionResult> {
        const auto status =
            writeAndReadBack(store_, keyValue, record, maxBytes);
        if (status != WriteReadbackStatus::NewValue) {
            return mapPreRootWrite(status, phase);
        }
        record.clear();
        return std::nullopt;
    };
    const auto epoch = prepared.newGraph.active.manifestReference.storageEpoch;
    if (prepared.changes.userConfiguration) {
        if (encodeUserConfigurationPayload(
                *prepared.newGraph.active.userConfiguration, timeZoneResolver_,
                payload) != ConfigurationCodecStatus::Success ||
            !encodeDocumentRecord(
                configuration_storage_contract::kUserConfigurationRecordType,
                requiredPlannedValue(
                    prepared.slotPlan.userConfigurationRevision),
                epoch, payload,
                configuration_limits::kMaximumUserConfigurationPayloadBytes +
                    45U,
                record)) {
            return {ConfigurationCommitExecutionStatus::CapacityFailure,
                    ConfigurationCommitFailurePhase::UserDocument};
        }
        payload.clear();
        const auto failure = writeRecord(
            configuration_storage_contract::kUserConfigurationSlotKeys
                [requiredPlannedValue(prepared.slotPlan.userConfigurationSlot)
                     .value()],
            configuration_limits::kMaximumUserConfigurationPayloadBytes + 45U,
            ConfigurationCommitFailurePhase::UserDocument);
        if (failure.has_value()) {
            return *failure;
        }
    }
    if (prepared.changes.serviceConfiguration) {
        if (encodeServiceConfigurationPayload(
                *prepared.newGraph.active.serviceConfiguration, payload) !=
                ConfigurationCodecStatus::Success ||
            !encodeDocumentRecord(
                configuration_storage_contract::kServiceConfigurationRecordType,
                requiredPlannedValue(
                    prepared.slotPlan.serviceConfigurationRevision),
                epoch, payload, 45U, record)) {
            return {ConfigurationCommitExecutionStatus::CapacityFailure,
                    ConfigurationCommitFailurePhase::ServiceDocument};
        }
        payload.clear();
        const auto failure = writeRecord(
            configuration_storage_contract::kServiceConfigurationSlotKeys
                [requiredPlannedValue(
                     prepared.slotPlan.serviceConfigurationSlot)
                     .value()],
            45U, ConfigurationCommitFailurePhase::ServiceDocument);
        if (failure.has_value()) {
            return *failure;
        }
    }
    if (prepared.changes.programCatalog) {
        if (encodeProgramCatalogPayload(
                *prepared.newGraph.active.programCatalog, payload) !=
                ConfigurationCodecStatus::Success ||
            !encodeDocumentRecord(
                configuration_storage_contract::kProgramCatalogRecordType,
                requiredPlannedValue(prepared.slotPlan.programCatalogRevision),
                epoch, payload,
                configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U,
                record)) {
            return {ConfigurationCommitExecutionStatus::CapacityFailure,
                    ConfigurationCommitFailurePhase::ProgramDocument};
        }
        payload.clear();
        const auto failure = writeRecord(
            configuration_storage_contract::kProgramCatalogSlotKeys
                [requiredPlannedValue(prepared.slotPlan.programCatalogSlot)
                     .value()],
            configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U,
            ConfigurationCommitFailurePhase::ProgramDocument);
        if (failure.has_value()) {
            return *failure;
        }
    }
    const auto manifestStatus = writeAndReadBack(
        store_,
        configuration_storage_contract::kConfigurationManifestSlotKeys
            [prepared.slotPlan.manifestSlot.value()],
        prepared.manifestRecordBytes,
        configuration_limits::kMaximumConfigurationManifestEnvelopeBytes);
    if (manifestStatus != WriteReadbackStatus::NewValue) {
        return mapPreRootWrite(manifestStatus,
                               ConfigurationCommitFailurePhase::Manifest);
    }
    auto targetStatus = validateBranchSemantically(store_, timeZoneResolver_,
                                                   prepared.newGraph.active);
    if (targetStatus == ConfigurationScanStatus::Success &&
        prepared.newGraph.fallback.has_value()) {
        targetStatus = validateBranchSemantically(store_, timeZoneResolver_,
                                                  *prepared.newGraph.fallback);
    }
    if (targetStatus != ConfigurationScanStatus::Success) {
        return {ConfigurationCommitExecutionStatus::RuntimeFailure,
                ConfigurationCommitFailurePhase::TargetGraphVerification,
                mapResolutionCause(targetStatus)};
    }
    const auto* const rootKey = configuration_storage_contract::
        kConfigurationRootSlotKeys[prepared.slotPlan.rootSlot.value()];
    const auto write = store_.write(key(rootKey), prepared.rootRecordBytes);
    if (write == device_platform::StateStoreWriteStatus::WriteError) {
        return {ConfigurationCommitExecutionStatus::WriteFailure,
                ConfigurationCommitFailurePhase::Root};
    }
    if (write == device_platform::StateStoreWriteStatus::CapacityError) {
        return {ConfigurationCommitExecutionStatus::CapacityFailure,
                ConfigurationCommitFailurePhase::Root};
    }
    if (write == device_platform::StateStoreWriteStatus::CommitOutcomeUnknown) {
        const auto resolution = resolveCommitDetailed(prepared);
        if (resolution.status ==
            ConfigurationCommitResolutionStatus::ResolutionRecoveredNew) {
            return {ConfigurationCommitExecutionStatus::Activated,
                    ConfigurationCommitFailurePhase::Root,
                    ConfigurationCommitResolutionCause::None};
        }
        if (resolution.status ==
            ConfigurationCommitResolutionStatus::ResolutionRecoveredOld) {
            return {ConfigurationCommitExecutionStatus::WriteFailure,
                    ConfigurationCommitFailurePhase::Root, resolution.cause};
        }
        return {resolution.status == ConfigurationCommitResolutionStatus::
                                         ResolutionStillIndeterminate
                    ? ConfigurationCommitExecutionStatus::CommitIndeterminate
                    : ConfigurationCommitExecutionStatus::RuntimeFailure,
                ConfigurationCommitFailurePhase::RootVerification,
                resolution.cause};
    }
    const auto verified = validationScan(prepared.newGraph);
    if (verified.status != ConfigurationScanStatus::Success) {
        return {ConfigurationCommitExecutionStatus::RuntimeFailure,
                ConfigurationCommitFailurePhase::RootVerification,
                mapResolutionCause(verified.status)};
    }
    return {ConfigurationCommitExecutionStatus::Activated,
            ConfigurationCommitFailurePhase::Root};
}

ConfigurationCommitResolutionStatus ConfigurationGraphStore::resolveCommit(
    const PreparedConfigurationCommit& prepared) const {
    return resolveCommitDetailed(prepared).status;
}

ConfigurationCommitResolutionResult
ConfigurationGraphStore::resolveCommitDetailed(
    const PreparedConfigurationCommit& prepared) const {
    const auto target = store_.read(
        key(configuration_storage_contract::kConfigurationRootSlotKeys
                [prepared.slotPlan.rootSlot.value()]),
        configuration_limits::kMaximumConfigurationRootEnvelopeBytes);
    if (target.status == device_platform::StateStoreReadStatus::ReadError ||
        target.status == device_platform::StateStoreReadStatus::CapacityError) {
        return {
            ConfigurationCommitResolutionStatus::ResolutionStillIndeterminate,
            target.status ==
                    device_platform::StateStoreReadStatus::CapacityError
                ? ConfigurationCommitResolutionCause::RootCapacityError
                : ConfigurationCommitResolutionCause::RootReadError};
    }

    if (target.status == device_platform::StateStoreReadStatus::Success &&
        target.value == prepared.rootRecordBytes) {
        const auto newResult = validationScan(prepared.newGraph);
        if (newResult.status == ConfigurationScanStatus::Success) {
            return {ConfigurationCommitResolutionStatus::ResolutionRecoveredNew,
                    ConfigurationCommitResolutionCause::None};
        }
        if (newResult.status == ConfigurationScanStatus::ReadError ||
            newResult.status == ConfigurationScanStatus::CapacityError ||
            newResult.status == ConfigurationScanStatus::ActiveBasisMismatch) {
            return {ConfigurationCommitResolutionStatus::
                        ResolutionStillIndeterminate,
                    mapResolutionCause(newResult.status)};
        }
        return {ConfigurationCommitResolutionStatus::ResolutionRuntimeFailure,
                mapResolutionCause(newResult.status)};
    }

    const bool priorTargetStillPresent =
        (target.status == device_platform::StateStoreReadStatus::NotFound &&
         !prepared.previousTargetRootRecordBytes.has_value()) ||
        (target.status == device_platform::StateStoreReadStatus::Success &&
         prepared.previousTargetRootRecordBytes.has_value() &&
         target.value == *prepared.previousTargetRootRecordBytes);
    if (!priorTargetStillPresent) {
        return {
            ConfigurationCommitResolutionStatus::ResolutionStillIndeterminate,
            ConfigurationCommitResolutionCause::AmbiguousRootOutcome};
    }

    const auto oldResult = validationScan(prepared.oldGraph);
    if (oldResult.status == ConfigurationScanStatus::Success) {
        return {ConfigurationCommitResolutionStatus::ResolutionRecoveredOld,
                ConfigurationCommitResolutionCause::None};
    }
    return {
        oldResult.status == ConfigurationScanStatus::ReadError ||
                oldResult.status == ConfigurationScanStatus::CapacityError ||
                oldResult.status == ConfigurationScanStatus::ActiveBasisMismatch
            ? ConfigurationCommitResolutionStatus::ResolutionStillIndeterminate
            : ConfigurationCommitResolutionStatus::ResolutionRuntimeFailure,
        mapResolutionCause(oldResult.status)};
}

InitialConfigurationPrepareResult ConfigurationGraphStore::prepareInitialGraph(
    device_platform::StorageEpoch epoch, ChangeOperation operation) const {
    if (epoch.value() == 0U ||
        (operation.kind != ChangeOperationKind::FactoryInitialization &&
         operation.kind != ChangeOperationKind::FactoryReset)) {
        return {InitialConfigurationPrepareStatus::InvalidCandidate,
                std::nullopt};
    }
    auto user = std::make_shared<const UserConfiguration>(
        UserConfiguration{"de", "Europe/Zurich", "Fermentationsschrank"});
    auto service = std::make_shared<const ServiceConfiguration>();
    auto catalog =
        std::make_shared<const ProgramCatalog>(makeFactoryProgramCatalog());
    if (validateUserConfiguration(*user, timeZoneResolver_).status !=
            UserConfigurationStatus::Success ||
        validateProgramCatalog(*catalog) != ProgramCatalogStatus::Success) {
        return {InitialConfigurationPrepareStatus::InvalidCandidate,
                std::nullopt};
    }

    std::string payload;
    std::string record;
    if (encodeUserConfigurationPayload(*user, timeZoneResolver_, payload) !=
            ConfigurationCodecStatus::Success ||
        !encodeDocumentRecord(
            configuration_storage_contract::kUserConfigurationRecordType,
            UserConfigurationRevision{1U}, epoch, payload,
            configuration_limits::kMaximumUserConfigurationPayloadBytes + 45U,
            record)) {
        return {InitialConfigurationPrepareStatus::InvalidCandidate,
                std::nullopt};
    }
    const auto userSlot = selectInitialSlot(
        store_, configuration_storage_contract::kUserConfigurationSlotKeys,
        epoch, configuration_storage_contract::kUserConfigurationRecordType, 1U,
        1U, record,
        configuration_limits::kMaximumUserConfigurationPayloadBytes + 45U);
    if (userSlot.status != InitialConfigurationPrepareStatus::Success) {
        return {userSlot.status, std::nullopt};
    }
    const UserConfigurationReference userRef{
        configuration_storage_contract::kUserConfigurationRecordType,
        *userSlot.slot,
        UserConfigurationRevision{1U},
        1U,
        static_cast<std::uint32_t>(payload.size()),
        device_platform::computeCrc32IsoHdlc(payload),
        epoch};
    std::string{}.swap(payload);
    std::string{}.swap(record);

    if (encodeServiceConfigurationPayload(*service, payload) !=
            ConfigurationCodecStatus::Success ||
        !encodeDocumentRecord(
            configuration_storage_contract::kServiceConfigurationRecordType,
            ServiceConfigurationRevision{1U}, epoch, payload, 45U, record)) {
        return {InitialConfigurationPrepareStatus::InvalidCandidate,
                std::nullopt};
    }
    const auto serviceSlot = selectInitialSlot(
        store_, configuration_storage_contract::kServiceConfigurationSlotKeys,
        epoch, configuration_storage_contract::kServiceConfigurationRecordType,
        1U, 1U, record, 45U);
    if (serviceSlot.status != InitialConfigurationPrepareStatus::Success) {
        return {serviceSlot.status, std::nullopt};
    }
    const ServiceConfigurationReference serviceRef{
        configuration_storage_contract::kServiceConfigurationRecordType,
        *serviceSlot.slot,
        ServiceConfigurationRevision{1U},
        1U,
        static_cast<std::uint32_t>(payload.size()),
        device_platform::computeCrc32IsoHdlc(payload),
        epoch};
    std::string{}.swap(payload);
    std::string{}.swap(record);

    if (encodeProgramCatalogPayload(*catalog, payload) !=
            ConfigurationCodecStatus::Success ||
        !encodeDocumentRecord(
            configuration_storage_contract::kProgramCatalogRecordType,
            ProgramCatalogRevision{1U}, epoch, payload,
            configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U,
            record)) {
        return {InitialConfigurationPrepareStatus::InvalidCandidate,
                std::nullopt};
    }
    const auto catalogSlot = selectInitialSlot(
        store_, configuration_storage_contract::kProgramCatalogSlotKeys, epoch,
        configuration_storage_contract::kProgramCatalogRecordType, 1U, 1U,
        record, configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U);
    if (catalogSlot.status != InitialConfigurationPrepareStatus::Success) {
        return {catalogSlot.status, std::nullopt};
    }
    const ProgramCatalogReference catalogRef{
        configuration_storage_contract::kProgramCatalogRecordType,
        *catalogSlot.slot,
        ProgramCatalogRevision{1U},
        1U,
        static_cast<std::uint32_t>(payload.size()),
        device_platform::computeCrc32IsoHdlc(payload),
        epoch};
    std::string{}.swap(payload);
    std::string{}.swap(record);

    const ConfigurationManifest manifest{decodeChangeOrigin(1U), operation,
                                         userRef, serviceRef, catalogRef};
    std::string manifestPayload;
    if (encodeConfigurationManifestPayload(manifest, manifestPayload) !=
        ConfigurationGraphCodecStatus::Success) {
        return {InitialConfigurationPrepareStatus::InvalidCandidate,
                std::nullopt};
    }
    const ConfigurationManifestReference manifestRef{
        configuration_storage_contract::kConfigurationManifestRecordType,
        device_platform::SlotId{0U},
        ConfigurationManifestGeneration{1U},
        kConfigurationManifestSchemaVersion1,
        static_cast<std::uint32_t>(manifestPayload.size()),
        device_platform::computeCrc32IsoHdlc(manifestPayload),
        epoch};
    std::string manifestRecord;
    if (encodeConfigurationManifestRecord(
            manifest, ConfigurationManifestGeneration{1U}, epoch, std::nullopt,
            manifestRecord) != ConfigurationGraphCodecStatus::Success) {
        return {InitialConfigurationPrepareStatus::CapacityFailure,
                std::nullopt};
    }
    const auto manifestSlot = selectInitialSlot(
        store_, configuration_storage_contract::kConfigurationManifestSlotKeys,
        epoch, configuration_storage_contract::kConfigurationManifestRecordType,
        kConfigurationManifestSchemaVersion1, 1U, manifestRecord,
        configuration_limits::kMaximumConfigurationManifestEnvelopeBytes);
    if (manifestSlot.status != InitialConfigurationPrepareStatus::Success) {
        return {manifestSlot.status, std::nullopt};
    }
    auto boundManifestRef = manifestRef;
    boundManifestRef.slot = *manifestSlot.slot;
    const ConfigurationRootRecord root{boundManifestRef, std::nullopt};
    std::string rootRecord;
    if (encodeConfigurationRootRecord(root, ConfigurationRootSequence{1U},
                                      epoch, std::nullopt, rootRecord) !=
        ConfigurationGraphCodecStatus::Success) {
        return {InitialConfigurationPrepareStatus::CapacityFailure,
                std::nullopt};
    }
    const auto rootSlot = selectInitialSlot(
        store_, configuration_storage_contract::kConfigurationRootSlotKeys,
        epoch, configuration_storage_contract::kConfigurationRootRecordType,
        kConfigurationRootSchemaVersion1, 1U, rootRecord,
        configuration_limits::kMaximumConfigurationRootEnvelopeBytes);
    if (rootSlot.status != InitialConfigurationPrepareStatus::Success) {
        return {rootSlot.status, std::nullopt};
    }
    ConfigurationGraphBranch branch{boundManifestRef, manifest, user,
                                    service,          catalog,  manifestRecord};
    LoadedConfigurationGraph graph{*rootSlot.slot,
                                   ConfigurationRootSequence{1U},
                                   root,
                                   rootRecord,
                                   std::move(branch),
                                   std::nullopt,
                                   false};
    ConfigurationSlotPlan plan;
    plan.userConfigurationSlot = *userSlot.slot;
    plan.serviceConfigurationSlot = *serviceSlot.slot;
    plan.programCatalogSlot = *catalogSlot.slot;
    plan.userConfigurationRevision = UserConfigurationRevision{1U};
    plan.serviceConfigurationRevision = ServiceConfigurationRevision{1U};
    plan.programCatalogRevision = ProgramCatalogRevision{1U};
    plan.manifestSlot = *manifestSlot.slot;
    plan.rootSlot = *rootSlot.slot;
    plan.manifestGeneration = ConfigurationManifestGeneration{1U};
    plan.rootSequence = ConfigurationRootSequence{1U};
    const auto planIdentity = device_platform::computeCrc32IsoHdlc(rootRecord);
    return {InitialConfigurationPrepareStatus::Success,
            PreparedInitialConfigurationGraph{
                std::move(graph), plan, std::move(manifestRecord),
                std::move(rootRecord), rootSlot.previousBytes,
                userSlot.writeRequired, serviceSlot.writeRequired,
                catalogSlot.writeRequired, manifestSlot.writeRequired,
                rootSlot.writeRequired, planIdentity}};
}

ConfigurationCommitExecutionResult ConfigurationGraphStore::executeInitialGraph(
    PreparedInitialConfigurationGraph& prepared,
    ConfigurationEpochGraphWriteCapability& capability) {
    if (capability.consumed_ ||
        capability.epoch_ !=
            prepared.graph.active.manifestReference.storageEpoch ||
        capability.planIdentity_ != prepared.planIdentity ||
        capability.mutationLease_ == nullptr ||
        !capability.mutationLease_->valid() ||
        (capability.bootstrapState_ !=
             ConfigurationBootstrapState::Initializing &&
         capability.bootstrapState_ !=
             ConfigurationBootstrapState::Resetting)) {
        return {ConfigurationCommitExecutionStatus::RuntimeFailure,
                ConfigurationCommitFailurePhase::TargetGraphVerification,
                ConfigurationCommitResolutionCause::GraphIntegrityFailure};
    }
    capability.consumed_ = true;
    std::string payload;
    std::string record;
    const auto epoch = capability.epoch_;
    const auto writeDocument = [this, &record](
                                   const char* keyValue, std::size_t maxBytes,
                                   ConfigurationCommitFailurePhase phase) {
        const auto status =
            writeAndReadBack(store_, keyValue, record, maxBytes);
        std::string{}.swap(record);
        return status == WriteReadbackStatus::NewValue
                   ? std::optional<ConfigurationCommitExecutionResult>{}
                   : std::optional<ConfigurationCommitExecutionResult>{
                         mapPreRootWrite(status, phase)};
    };
    if (prepared.userWriteRequired &&
        (encodeUserConfigurationPayload(
             *prepared.graph.active.userConfiguration, timeZoneResolver_,
             payload) != ConfigurationCodecStatus::Success ||
         !encodeDocumentRecord(
             configuration_storage_contract::kUserConfigurationRecordType,
             UserConfigurationRevision{1U}, epoch, payload,
             configuration_limits::kMaximumUserConfigurationPayloadBytes + 45U,
             record))) {
        return {ConfigurationCommitExecutionStatus::CapacityFailure,
                ConfigurationCommitFailurePhase::UserDocument};
    }
    std::string{}.swap(payload);
    if (prepared.userWriteRequired) {
        if (auto failure = writeDocument(
                configuration_storage_contract::kUserConfigurationSlotKeys
                    [prepared.slotPlan.userConfigurationSlot->value()],
                configuration_limits::kMaximumUserConfigurationPayloadBytes +
                    45U,
                ConfigurationCommitFailurePhase::UserDocument)) {
            return *failure;
        }
    }
    if (prepared.serviceWriteRequired &&
        (encodeServiceConfigurationPayload(
             *prepared.graph.active.serviceConfiguration, payload) !=
             ConfigurationCodecStatus::Success ||
         !encodeDocumentRecord(
             configuration_storage_contract::kServiceConfigurationRecordType,
             ServiceConfigurationRevision{1U}, epoch, payload, 45U, record))) {
        return {ConfigurationCommitExecutionStatus::CapacityFailure,
                ConfigurationCommitFailurePhase::ServiceDocument};
    }
    std::string{}.swap(payload);
    if (prepared.serviceWriteRequired) {
        if (auto failure = writeDocument(
                configuration_storage_contract::kServiceConfigurationSlotKeys
                    [prepared.slotPlan.serviceConfigurationSlot->value()],
                45U, ConfigurationCommitFailurePhase::ServiceDocument)) {
            return *failure;
        }
    }
    if (prepared.programWriteRequired &&
        (encodeProgramCatalogPayload(*prepared.graph.active.programCatalog,
                                     payload) !=
             ConfigurationCodecStatus::Success ||
         !encodeDocumentRecord(
             configuration_storage_contract::kProgramCatalogRecordType,
             ProgramCatalogRevision{1U}, epoch, payload,
             configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U,
             record))) {
        return {ConfigurationCommitExecutionStatus::CapacityFailure,
                ConfigurationCommitFailurePhase::ProgramDocument};
    }
    std::string{}.swap(payload);
    if (prepared.programWriteRequired) {
        if (auto failure = writeDocument(
                configuration_storage_contract::kProgramCatalogSlotKeys
                    [prepared.slotPlan.programCatalogSlot->value()],
                configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U,
                ConfigurationCommitFailurePhase::ProgramDocument)) {
            return *failure;
        }
    }
    if (prepared.manifestWriteRequired) {
        const auto manifest = writeAndReadBack(
            store_,
            configuration_storage_contract::kConfigurationManifestSlotKeys
                [prepared.slotPlan.manifestSlot.value()],
            prepared.manifestRecordBytes,
            configuration_limits::kMaximumConfigurationManifestEnvelopeBytes);
        if (manifest != WriteReadbackStatus::NewValue) {
            return mapPreRootWrite(manifest,
                                   ConfigurationCommitFailurePhase::Manifest);
        }
    }
    const auto target = validateBranchSemantically(store_, timeZoneResolver_,
                                                   prepared.graph.active);
    if (target != ConfigurationScanStatus::Success) {
        return {ConfigurationCommitExecutionStatus::RuntimeFailure,
                ConfigurationCommitFailurePhase::TargetGraphVerification,
                mapResolutionCause(target)};
    }
    if (!prepared.rootWriteRequired) {
        auto existing = loadCanonicalGraph(epoch);
        if (existing.status !=
                ConfigurationGraphLoadStatus::ConfigurationGraphAvailable ||
            !existing.graph.has_value() ||
            existing.graph->canonicalRootRecordBytes !=
                prepared.rootRecordBytes) {
            return {ConfigurationCommitExecutionStatus::RuntimeFailure,
                    ConfigurationCommitFailurePhase::RootVerification,
                    ConfigurationCommitResolutionCause::GraphIntegrityFailure};
        }
        prepared.graph = std::move(*existing.graph);
        return {ConfigurationCommitExecutionStatus::Activated,
                ConfigurationCommitFailurePhase::Root};
    }
    const auto rootWrite = store_.write(
        key(configuration_storage_contract::kConfigurationRootSlotKeys
                [prepared.slotPlan.rootSlot.value()]),
        prepared.rootRecordBytes);
    if (rootWrite == device_platform::StateStoreWriteStatus::WriteError) {
        return {ConfigurationCommitExecutionStatus::WriteFailure,
                ConfigurationCommitFailurePhase::Root};
    }
    if (rootWrite == device_platform::StateStoreWriteStatus::CapacityError) {
        return {ConfigurationCommitExecutionStatus::CapacityFailure,
                ConfigurationCommitFailurePhase::Root};
    }
    const auto loaded = loadCanonicalGraph(epoch);
    if (loaded.status !=
            ConfigurationGraphLoadStatus::ConfigurationGraphAvailable ||
        !loaded.graph.has_value() ||
        loaded.graph->canonicalRootRecordBytes != prepared.rootRecordBytes) {
        return {
            rootWrite ==
                    device_platform::StateStoreWriteStatus::CommitOutcomeUnknown
                ? ConfigurationCommitExecutionStatus::CommitIndeterminate
                : ConfigurationCommitExecutionStatus::RuntimeFailure,
            ConfigurationCommitFailurePhase::RootVerification,
            ConfigurationCommitResolutionCause::AmbiguousRootOutcome};
    }
    prepared.graph = *loaded.graph;
    return {ConfigurationCommitExecutionStatus::Activated,
            ConfigurationCommitFailurePhase::Root};
}

ConfigurationCommitResolutionResult
ConfigurationGraphStore::resolveInitialGraph(
    const PreparedInitialConfigurationGraph& prepared) const {
    const auto target = store_.read(
        key(configuration_storage_contract::kConfigurationRootSlotKeys
                [prepared.slotPlan.rootSlot.value()]),
        configuration_limits::kMaximumConfigurationRootEnvelopeBytes);
    if (target.status == device_platform::StateStoreReadStatus::ReadError) {
        return {
            ConfigurationCommitResolutionStatus::ResolutionStillIndeterminate,
            ConfigurationCommitResolutionCause::RootReadError};
    }
    if (target.status == device_platform::StateStoreReadStatus::CapacityError) {
        return {
            ConfigurationCommitResolutionStatus::ResolutionStillIndeterminate,
            ConfigurationCommitResolutionCause::RootCapacityError};
    }
    if (target.status == device_platform::StateStoreReadStatus::Success &&
        target.value == prepared.rootRecordBytes) {
        const auto loaded = loadCanonicalGraph(
            prepared.graph.active.manifestReference.storageEpoch);
        if (loaded.status ==
                ConfigurationGraphLoadStatus::ConfigurationGraphAvailable &&
            loaded.graph.has_value() &&
            loaded.graph->canonicalRootRecordBytes ==
                prepared.rootRecordBytes) {
            return {ConfigurationCommitResolutionStatus::ResolutionRecoveredNew,
                    ConfigurationCommitResolutionCause::None};
        }
        if (loaded.status == ConfigurationGraphLoadStatus::RootReadError ||
            loaded.status == ConfigurationGraphLoadStatus::RecordReadError) {
            return {ConfigurationCommitResolutionStatus::
                        ResolutionStillIndeterminate,
                    ConfigurationCommitResolutionCause::GraphReadError};
        }
        if (loaded.status == ConfigurationGraphLoadStatus::RootCapacityError ||
            loaded.status ==
                ConfigurationGraphLoadStatus::RecordCapacityError) {
            return {ConfigurationCommitResolutionStatus::
                        ResolutionStillIndeterminate,
                    ConfigurationCommitResolutionCause::GraphCapacityError};
        }
        return {
            ConfigurationCommitResolutionStatus::ResolutionRuntimeFailure,
            loaded.status == ConfigurationGraphLoadStatus::
                                 UnsupportedNewerConfigurationSchema
                ? ConfigurationCommitResolutionCause::UnsupportedNewerSchema
                : ConfigurationCommitResolutionCause::GraphIntegrityFailure};
    }
    const bool oldConfirmed =
        target.status == device_platform::StateStoreReadStatus::NotFound ||
        (target.status == device_platform::StateStoreReadStatus::Success &&
         prepared.previousTargetRootRecordBytes.has_value() &&
         target.value == *prepared.previousTargetRootRecordBytes);
    if (oldConfirmed) {
        return {ConfigurationCommitResolutionStatus::ResolutionRecoveredOld,
                ConfigurationCommitResolutionCause::None};
    }
    return {ConfigurationCommitResolutionStatus::ResolutionStillIndeterminate,
            ConfigurationCommitResolutionCause::AmbiguousRootOutcome};
}

}  // namespace fermentation
