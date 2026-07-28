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

device_platform::StateStoreKey key(const char* value) {
    auto result = device_platform::StateStoreKey::create(value);
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
    auto manifest = std::move(*decodedManifest.value);

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
        std::move(manifest),
        std::make_shared<const UserConfiguration>(std::move(*user.document)),
        std::make_shared<const ServiceConfiguration>(
            std::move(*service.document)),
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
        auto root = std::move(*decodedRoot.value);
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
            active = std::move(fallback);
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
            std::move(root),
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
    auto roots = readGroup(
        store_, configuration_storage_contract::kConfigurationRootSlotKeys,
        configuration_storage_contract::kConfigurationRootRecordType,
        kConfigurationRootSchemaVersion1, epoch,
        configuration_limits::kMaximumConfigurationRootEnvelopeBytes,
        result.diagnostics);
    auto users = readGroup(
        store_, configuration_storage_contract::kUserConfigurationSlotKeys,
        configuration_storage_contract::kUserConfigurationRecordType, 1U, epoch,
        configuration_limits::kMaximumUserConfigurationPayloadBytes + 45U,
        result.diagnostics);
    auto services = readGroup(
        store_, configuration_storage_contract::kServiceConfigurationSlotKeys,
        configuration_storage_contract::kServiceConfigurationRecordType, 1U,
        epoch, 45U, result.diagnostics);
    auto catalogs = readGroup(
        store_, configuration_storage_contract::kProgramCatalogSlotKeys,
        configuration_storage_contract::kProgramCatalogRecordType, 1U, epoch,
        configuration_limits::kMaximumProgramCatalogPayloadBytes + 45U,
        result.diagnostics);
    auto manifests = readGroup(
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
    const auto* root = findRecord(roots.records, expectedActive.rootSlot);
    if (root == nullptr ||
        root->bytes != expectedActive.canonicalRootRecordBytes ||
        roots.highWater > expectedActive.rootSequence.value()) {
        result.status = ConfigurationScanStatus::ActiveBasisMismatch;
        return result;
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

ConfigurationSlotPlanResult ConfigurationGraphStore::planSlots(
    const LoadedConfigurationGraph& current,
    const ConfigurationHighWaterMarks& highWater,
    ConfigurationChangeMask changes) const {
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
    return {ConfigurationSlotPlanStatus::Success, std::move(plan)};
}

}  // namespace fermentation
