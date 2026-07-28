#include "configuration_graph_store.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <map>
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

struct GroupReadResult {
    ConfigurationScanStatus status{ConfigurationScanStatus::Success};
    std::vector<StoredRecord> records;
    std::uint64_t highWater{0U};
    bool newerSchema{false};
};

struct MetadataScanResult {
    ConfigurationScanStatus status{ConfigurationScanStatus::Success};
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
GroupReadResult readGroup(const device_platform::IStateStore& store,
                          const std::array<const char*, N>& keys,
                          device_platform::RecordTypeId expectedRecordType,
                          std::uint32_t currentSchema,
                          device_platform::StorageEpoch epoch,
                          std::size_t maxBytes,
                          ConfigurationGraphDiagnostics& diagnostics) {
    GroupReadResult result;
    std::map<std::uint64_t, std::string> identities;
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
        auto decoded = device_platform::decodeEnvelope(read.value);
        if (!decoded.envelope.has_value()) {
            ++diagnostics.invalidCandidates;
            continue;
        }
        auto& envelope = *decoded.envelope;
        if (envelope.recordTypeId != expectedRecordType ||
            envelope.storageEpoch != epoch) {
            ++diagnostics.invalidCandidates;
            continue;
        }
        result.highWater = std::max(result.highWater, envelope.versionValue);
        if (envelope.schemaVersion > currentSchema) {
            result.newerSchema = true;
        }
        const auto existing = identities.find(envelope.versionValue);
        if (existing != identities.end()) {
            if (existing->second != read.value) {
                result.status =
                    ConfigurationScanStatus::ConfigurationGraphIntegrityFailure;
                return result;
            }
            ++diagnostics.exactDuplicateRecords;
        } else {
            identities.emplace(envelope.versionValue, read.value);
        }
        result.records.push_back(
            {device_platform::SlotId(static_cast<std::uint32_t>(index)),
             std::move(read.value), std::move(envelope)});
    }
    return result;
}

template <std::size_t N>
MetadataScanResult scanGroupMetadata(
    const device_platform::IStateStore& store,
    const std::array<const char*, N>& keys,
    device_platform::RecordTypeId expectedRecordType,
    std::uint32_t currentSchema, device_platform::StorageEpoch epoch,
    std::size_t maxBytes, ConfigurationGraphDiagnostics& diagnostics) {
    MetadataScanResult result;
    std::map<std::uint64_t, std::size_t> identities;
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
            continue;
        }
        const auto& metadata = *decoded.metadata;
        if (metadata.recordTypeId != expectedRecordType ||
            metadata.storageEpoch != epoch) {
            ++diagnostics.invalidCandidates;
            continue;
        }
        result.highWater = std::max(result.highWater, metadata.versionValue);
        result.newerSchema =
            result.newerSchema || metadata.schemaVersion > currentSchema;
        const auto existing = identities.find(metadata.versionValue);
        if (existing == identities.end()) {
            identities.emplace(metadata.versionValue, index);
            continue;
        }
        const auto previous = store.read(key(keys[existing->second]), maxBytes);
        if (previous.status != device_platform::StateStoreReadStatus::Success) {
            result.status = mapReadStatus(previous.status);
            return result;
        }
        if (previous.value != read.value) {
            result.status =
                ConfigurationScanStatus::ConfigurationGraphIntegrityFailure;
            return result;
        }
        ++diagnostics.exactDuplicateRecords;
    }
    return result;
}

const StoredRecord* findRecord(const std::vector<StoredRecord>& records,
                               device_platform::SlotId slot) {
    const auto found = std::find_if(
        records.begin(), records.end(),
        [slot](const StoredRecord& record) { return record.slot == slot; });
    return found == records.end() ? nullptr : &*found;
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
        return ConfigurationScanStatus::ConfigurationGraphIntegrityFailure;
    }
    auto read = store.read(key(keys[reference.slot.value()]), maxBytes);
    if (read.status == device_platform::StateStoreReadStatus::NotFound) {
        return ConfigurationScanStatus::ConfigurationGraphIntegrityFailure;
    }
    if (read.status != device_platform::StateStoreReadStatus::Success) {
        return mapReadStatus(read.status);
    }
    auto decoded = device_platform::decodeEnvelope(read.value);
    if (!decoded.envelope.has_value()) {
        return ConfigurationScanStatus::ConfigurationGraphIntegrityFailure;
    }
    const StoredRecord record{reference.slot, read.value,
                              std::move(*decoded.envelope)};
    if (!referenceMatches(reference, record) ||
        (exactBytes != nullptr && *exactBytes != read.value)) {
        return ConfigurationScanStatus::ConfigurationGraphIntegrityFailure;
    }
    return ConfigurationScanStatus::Success;
}

ConfigurationScanStatus validateExpectedBranch(
    const device_platform::IStateStore& store,
    const ConfigurationGraphBranch& branch) {
    auto status = validateStoredReference(
        store, configuration_storage_contract::kConfigurationManifestSlotKeys,
        branch.manifestReference,
        configuration_limits::kMaximumConfigurationManifestEnvelopeBytes,
        &branch.canonicalManifestRecordBytes);
    if (status != ConfigurationScanStatus::Success) {
        return status;
    }
    status = validateStoredReference(
        store, configuration_storage_contract::kUserConfigurationSlotKeys,
        branch.manifest.userConfiguration,
        configuration_limits::kMaximumUserConfigurationPayloadBytes + 45U);
    if (status != ConfigurationScanStatus::Success) {
        return status;
    }
    status = validateStoredReference(
        store, configuration_storage_contract::kServiceConfigurationSlotKeys,
        branch.manifest.serviceConfiguration, 45U);
    if (status != ConfigurationScanStatus::Success) {
        return status;
    }
    return validateStoredReference(
        store, configuration_storage_contract::kProgramCatalogSlotKeys,
        branch.manifest.programCatalog,
        configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U);
}

std::optional<ConfigurationGraphBranch> loadBranch(
    const ConfigurationManifestReference& reference,
    const std::vector<StoredRecord>& manifests,
    const std::vector<StoredRecord>& users,
    const std::vector<StoredRecord>& services,
    const std::vector<StoredRecord>& catalogs,
    const device_platform::ITimeZoneResolver& resolver) {
    const auto* manifestRecord = findRecord(manifests, reference.slot);
    if (manifestRecord == nullptr ||
        !referenceMatches(reference, *manifestRecord) ||
        manifestRecord->envelope.schemaVersion !=
            kConfigurationManifestSchemaVersion1) {
        return std::nullopt;
    }
    auto decodedManifest =
        decodeConfigurationManifestPayload(manifestRecord->envelope.payload);
    if (!decodedManifest.value.has_value()) {
        return std::nullopt;
    }
    const auto manifest = *decodedManifest.value;

    const auto* userRecord = findRecord(users, manifest.userConfiguration.slot);
    const auto* serviceRecord =
        findRecord(services, manifest.serviceConfiguration.slot);
    const auto* catalogRecord =
        findRecord(catalogs, manifest.programCatalog.slot);
    if (userRecord == nullptr || serviceRecord == nullptr ||
        catalogRecord == nullptr ||
        !referenceMatches(manifest.userConfiguration, *userRecord) ||
        !referenceMatches(manifest.serviceConfiguration, *serviceRecord) ||
        !referenceMatches(manifest.programCatalog, *catalogRecord)) {
        return std::nullopt;
    }
    auto user =
        decodeUserConfigurationPayload(userRecord->envelope.schemaVersion,
                                       userRecord->envelope.payload, resolver);
    auto service = decodeServiceConfigurationPayload(
        serviceRecord->envelope.schemaVersion, serviceRecord->envelope.payload);
    auto catalog = decodeProgramCatalogPayload(
        catalogRecord->envelope.schemaVersion, catalogRecord->envelope.payload);
    if (!user.document.has_value() || !service.document.has_value() ||
        !catalog.document.has_value()) {
        return std::nullopt;
    }
    return ConfigurationGraphBranch{
        reference,
        manifest,
        std::make_shared<const UserConfiguration>(std::move(*user.document)),
        std::make_shared<const ServiceConfiguration>(*service.document),
        std::make_shared<const ProgramCatalog>(std::move(*catalog.document)),
        manifestRecord->bytes};
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

void protectBranch(const ConfigurationGraphBranch& branch,
                   std::array<bool, 4>& users, std::array<bool, 4>& services,
                   std::array<bool, 4>& catalogs,
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

}  // namespace

ConfigurationGraphLoadResult ConfigurationGraphStore::loadCanonicalGraph(
    device_platform::StorageEpoch storageEpoch) const {
    ConfigurationGraphLoadResult result;
    auto roots = readGroup(
        store_, configuration_storage_contract::kConfigurationRootSlotKeys,
        configuration_storage_contract::kConfigurationRootRecordType,
        kConfigurationRootSchemaVersion1, storageEpoch,
        configuration_limits::kMaximumConfigurationRootEnvelopeBytes,
        result.diagnostics);
    if (roots.status != ConfigurationScanStatus::Success) {
        result.status = toLoadStatus(roots.status, true);
        return result;
    }
    auto users = readGroup(
        store_, configuration_storage_contract::kUserConfigurationSlotKeys,
        configuration_storage_contract::kUserConfigurationRecordType, 1U,
        storageEpoch,
        configuration_limits::kMaximumUserConfigurationPayloadBytes + 45U,
        result.diagnostics);
    auto services = readGroup(
        store_, configuration_storage_contract::kServiceConfigurationSlotKeys,
        configuration_storage_contract::kServiceConfigurationRecordType, 1U,
        storageEpoch, 45U, result.diagnostics);
    auto catalogs = readGroup(
        store_, configuration_storage_contract::kProgramCatalogSlotKeys,
        configuration_storage_contract::kProgramCatalogRecordType, 1U,
        storageEpoch,
        configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U,
        result.diagnostics);
    auto manifests = readGroup(
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
        result.status =
            ConfigurationGraphLoadStatus::ConfigurationGraphUnavailable;
        return result;
    }
    std::sort(
        roots.records.begin(), roots.records.end(),
        [](const StoredRecord& left, const StoredRecord& right) {
            if (left.envelope.versionValue != right.envelope.versionValue) {
                return left.envelope.versionValue > right.envelope.versionValue;
            }
            return left.slot.value() < right.slot.value();
        });
    if (roots.records.size() > 1U &&
        roots.records[0].envelope.versionValue ==
            roots.records[1].envelope.versionValue &&
        roots.records[0].bytes == roots.records[1].bytes) {
        result.diagnostics.identicalRootTie = true;
    }

    for (const auto& rootRecord : roots.records) {
        if (rootRecord.envelope.schemaVersion !=
            kConfigurationRootSchemaVersion1) {
            ++result.diagnostics.invalidCandidates;
            continue;
        }
        auto decodedRoot =
            decodeConfigurationRootPayload(rootRecord.envelope.payload);
        if (!decodedRoot.value.has_value()) {
            ++result.diagnostics.invalidCandidates;
            continue;
        }
        const auto root = *decodedRoot.value;
        auto active =
            loadBranch(root.active, manifests.records, users.records,
                       services.records, catalogs.records, timeZoneResolver_);
        std::optional<ConfigurationGraphBranch> fallback;
        if (root.fallback.has_value()) {
            fallback = loadBranch(*root.fallback, manifests.records,
                                  users.records, services.records,
                                  catalogs.records, timeZoneResolver_);
        }
        bool selectedFallback = false;
        if (!active.has_value() && fallback.has_value()) {
            active = *fallback;
            fallback.reset();
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
            rootRecord.slot,
            ConfigurationRootSequence(rootRecord.envelope.versionValue),
            root,
            rootRecord.bytes,
            std::move(*active),
            std::move(fallback),
            selectedFallback};
        return result;
    }
    result.status =
        result.diagnostics.invalidCandidates == 0U
            ? ConfigurationGraphLoadStatus::ConfigurationGraphUnavailable
            : ConfigurationGraphLoadStatus::ConfigurationGraphIntegrityFailure;
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
    const auto expectedRoot = store_.read(
        key(configuration_storage_contract::kConfigurationRootSlotKeys
                [expectedActive.rootSlot.value()]),
        configuration_limits::kMaximumConfigurationRootEnvelopeBytes);
    if (expectedRoot.status != device_platform::StateStoreReadStatus::Success) {
        result.status = mapReadStatus(expectedRoot.status);
        return result;
    }
    if (expectedRoot.value != expectedActive.canonicalRootRecordBytes) {
        result.status = ConfigurationScanStatus::ActiveBasisMismatch;
        return result;
    }
    result.status = validateExpectedBranch(store_, expectedActive.active);
    if (result.status != ConfigurationScanStatus::Success) {
        return result;
    }
    if (expectedActive.fallback.has_value()) {
        result.status =
            validateExpectedBranch(store_, *expectedActive.fallback);
        if (result.status != ConfigurationScanStatus::Success) {
            return result;
        }
    }
    result.highWater = {UserConfigurationRevision(users.highWater),
                        ServiceConfigurationRevision(services.highWater),
                        ProgramCatalogRevision(catalogs.highWater),
                        ConfigurationManifestGeneration(manifests.highWater),
                        ConfigurationRootSequence(roots.highWater)};
    if (users.highWater == std::numeric_limits<std::uint64_t>::max() ||
        services.highWater == std::numeric_limits<std::uint64_t>::max() ||
        catalogs.highWater == std::numeric_limits<std::uint64_t>::max() ||
        manifests.highWater == std::numeric_limits<std::uint64_t>::max() ||
        roots.highWater == std::numeric_limits<std::uint64_t>::max()) {
        result.status = ConfigurationScanStatus::HighWaterOverflow;
        return result;
    }
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
    const auto changes = candidate.changes;
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
        } else if (scan.status == ConfigurationScanStatus::
                                      ConfigurationGraphIntegrityFailure) {
            status = ConfigurationCommitPrepareStatus::IntegrityFailure;
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
    ConfigurationGraphBranch newActive{
        manifestReference,           manifest,
        candidate.userConfiguration, candidate.serviceConfiguration,
        candidate.programCatalog,    manifestRecord};
    LoadedConfigurationGraph newGraph{
        plan.rootSlot,        plan.rootSequence, root, rootRecord,
        std::move(newActive), current.active,    false};
    return {ConfigurationCommitPrepareStatus::Success,
            PreparedConfigurationCommit{current, std::move(newGraph), changes,
                                        plan, std::move(manifestRecord),
                                        std::move(rootRecord)}};
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
        const auto resolution = resolveCommit(prepared);
        if (resolution ==
            ConfigurationCommitResolutionStatus::ResolutionRecoveredNew) {
            return {ConfigurationCommitExecutionStatus::Activated,
                    ConfigurationCommitFailurePhase::Root};
        }
        if (resolution ==
            ConfigurationCommitResolutionStatus::ResolutionRecoveredOld) {
            return {ConfigurationCommitExecutionStatus::WriteFailure,
                    ConfigurationCommitFailurePhase::Root};
        }
        return {resolution == ConfigurationCommitResolutionStatus::
                                  ResolutionStillIndeterminate
                    ? ConfigurationCommitExecutionStatus::CommitIndeterminate
                    : ConfigurationCommitExecutionStatus::RuntimeFailure,
                ConfigurationCommitFailurePhase::RootVerification};
    }
    const auto verified = validationScan(prepared.newGraph);
    if (verified.status != ConfigurationScanStatus::Success) {
        return {ConfigurationCommitExecutionStatus::RuntimeFailure,
                ConfigurationCommitFailurePhase::RootVerification};
    }
    return {ConfigurationCommitExecutionStatus::Activated,
            ConfigurationCommitFailurePhase::Root};
}

ConfigurationCommitResolutionStatus ConfigurationGraphStore::resolveCommit(
    const PreparedConfigurationCommit& prepared) const {
    const auto newResult = validationScan(prepared.newGraph);
    if (newResult.status == ConfigurationScanStatus::Success) {
        return ConfigurationCommitResolutionStatus::ResolutionRecoveredNew;
    }
    const auto oldResult = validationScan(prepared.oldGraph);
    if (oldResult.status == ConfigurationScanStatus::Success) {
        return ConfigurationCommitResolutionStatus::ResolutionRecoveredOld;
    }
    return ConfigurationCommitResolutionStatus::ResolutionStillIndeterminate;
}

}  // namespace fermentation
