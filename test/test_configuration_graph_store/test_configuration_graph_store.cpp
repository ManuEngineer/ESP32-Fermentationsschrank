#include <unity.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "configuration_document_codec.hpp"
#include "configuration_documents.hpp"
#include "configuration_graph_codec.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"
#include "crc32.hpp"
#include "state_store.hpp"
#include "state_store_key.hpp"
#include "standard_program_catalog.hpp"
#include "storage_envelope.hpp"

namespace {

using device_platform::RecordTypeId;
using device_platform::SlotId;
using device_platform::StateStoreKey;
using device_platform::StorageEpoch;

class LocalStore final : public device_platform::IStateStore {
   public:
    device_platform::StateStoreWriteStatus write(
        const StateStoreKey& key, const std::string& value) override {
        ++writeCount_;
        const auto fault = writeFaults_.find(key.bytes());
        if (fault == writeFaults_.end() || fault->second.commitValue) {
            values_[key.bytes()] = value;
        }
        const auto readFaults = readFaultsAfterWrite_.find(key.bytes());
        if (readFaults != readFaultsAfterWrite_.end()) {
            for (const auto& [readKey, status] : readFaults->second) {
                faults_[readKey] = status;
            }
        }
        const auto replacements = replacementsAfterWrite_.find(key.bytes());
        if (replacements != replacementsAfterWrite_.end()) {
            for (const auto& [targetKey, replacement] : replacements->second) {
                values_[targetKey] = replacement;
            }
        }
        if (fault != writeFaults_.end()) {
            return fault->second.status;
        }
        return device_platform::StateStoreWriteStatus::Success;
    }

    device_platform::StateStoreReadResult read(
        const StateStoreKey& key, std::size_t maxBytes) const override {
        const auto fault = faults_.find(key.bytes());
        if (fault != faults_.end()) {
            return {fault->second, {}};
        }
        const auto found = values_.find(key.bytes());
        if (found == values_.end()) {
            return {device_platform::StateStoreReadStatus::NotFound, {}};
        }
        if (found->second.size() > maxBytes) {
            return {device_platform::StateStoreReadStatus::CapacityError, {}};
        }
        const auto value = found->second;
        if (replacementAfterRead_.has_value() &&
            replacementAfterRead_->triggerKey == key.bytes()) {
            values_[replacementAfterRead_->targetKey] =
                replacementAfterRead_->replacement;
            replacementAfterRead_.reset();
        }
        return {device_platform::StateStoreReadStatus::Success, value};
    }

    void put(const char* keyValue, std::string value) {
        values_[keyValue] = std::move(value);
    }

    void erase(const char* keyValue) { values_.erase(keyValue); }

    void failRead(const char* keyValue,
                  device_platform::StateStoreReadStatus status) {
        faults_[keyValue] = status;
    }

    void clearReadFault(const char* keyValue) { faults_.erase(keyValue); }

    void failWrite(const char* keyValue,
                   device_platform::StateStoreWriteStatus status,
                   bool commitValue) {
        TEST_ASSERT_FALSE(
            commitValue &&
            (status == device_platform::StateStoreWriteStatus::WriteError ||
             status == device_platform::StateStoreWriteStatus::CapacityError));
        writeFaults_[keyValue] = {status, commitValue};
    }

    void failReadsAfterWrite(
        const char* writeKey,
        std::vector<
            std::pair<std::string, device_platform::StateStoreReadStatus>>
            faults) {
        readFaultsAfterWrite_[writeKey] = std::move(faults);
    }

    void replaceAfterWrite(
        const char* writeKey,
        std::vector<std::pair<std::string, std::string>> replacements) {
        replacementsAfterWrite_[writeKey] = std::move(replacements);
    }

    void replaceAfterNextRead(const char* readKey, const char* targetKey,
                              std::string replacement) {
        replacementAfterRead_ =
            ReplacementAfterRead{readKey, targetKey, std::move(replacement)};
    }

    [[nodiscard]] std::size_t writeCount() const { return writeCount_; }

   private:
    struct WriteFault {
        device_platform::StateStoreWriteStatus status;
        bool commitValue;
    };
    struct ReplacementAfterRead {
        std::string triggerKey;
        std::string targetKey;
        std::string replacement;
    };
    mutable std::map<std::string, std::string> values_;
    mutable std::map<std::string, device_platform::StateStoreReadStatus>
        faults_;
    std::map<std::string, WriteFault> writeFaults_;
    std::map<std::string,
             std::vector<
                 std::pair<std::string, device_platform::StateStoreReadStatus>>>
        readFaultsAfterWrite_;
    std::map<std::string, std::vector<std::pair<std::string, std::string>>>
        replacementsAfterWrite_;
    mutable std::optional<ReplacementAfterRead> replacementAfterRead_;
    std::size_t writeCount_{0U};
};

class LocalTimeZoneResolver final : public device_platform::ITimeZoneResolver {
   public:
    device_platform::TimeZonePrepareResult prepare(
        const std::string& identifier) const override {
        if (identifier != "Europe/Zurich") {
            return {
                device_platform::TimeZonePrepareStatus::UnsupportedIdentifier,
                std::nullopt};
        }
        return {device_platform::TimeZonePrepareStatus::Success,
                device_platform::PreparedTimeZone{identifier}};
    }
};

StateStoreKey key(const char* value) {
    auto result = StateStoreKey::create(value);
    return std::move(*result.key);
}

std::string envelope(RecordTypeId type, std::uint32_t schema,
                     std::uint64_t version, const std::string& payload,
                     std::optional<std::int64_t> utc = std::nullopt,
                     StorageEpoch epoch = StorageEpoch{1U}) {
    std::string bytes;
    TEST_ASSERT_TRUE(device_platform::encodeEnvelope(
                         {type, schema, epoch, version, utc, payload}, bytes,
                         payload.size() + (utc.has_value() ? 53U : 45U)) ==
                     device_platform::EnvelopeEncodeStatus::Success);
    return bytes;
}

template <typename Version>
fermentation::ConfigurationRecordReference<Version> reference(
    RecordTypeId type, std::uint32_t slot, Version version,
    const std::string& payload) {
    return {type,
            SlotId{slot},
            version,
            1U,
            static_cast<std::uint32_t>(payload.size()),
            device_platform::computeCrc32IsoHdlc(payload),
            StorageEpoch{1U}};
}

struct SeededGraph {
    fermentation::ConfigurationManifestReference manifestReference;
    fermentation::ConfigurationRootRecord root;
};

SeededGraph seedGraph(LocalStore& store, std::uint64_t generation = 1U,
                      std::uint64_t sequence = 1U,
                      std::uint32_t manifestSlot = 0U,
                      std::uint32_t rootSlot = 0U) {
    const fermentation::UserConfiguration user{"de", "Europe/Zurich",
                                               "Fermentationsschrank"};
    const fermentation::ServiceConfiguration service;
    const auto catalog = fermentation::makeFactoryProgramCatalog();
    LocalTimeZoneResolver resolver;
    std::string userPayload;
    std::string servicePayload;
    std::string catalogPayload;
    TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                         user, resolver, userPayload) ==
                     fermentation::ConfigurationCodecStatus::Success);
    TEST_ASSERT_TRUE(fermentation::encodeServiceConfigurationPayload(
                         service, servicePayload) ==
                     fermentation::ConfigurationCodecStatus::Success);
    TEST_ASSERT_TRUE(
        fermentation::encodeProgramCatalogPayload(catalog, catalogPayload) ==
        fermentation::ConfigurationCodecStatus::Success);
    store.put("uc0", envelope(fermentation::configuration_storage_contract::
                                  kUserConfigurationRecordType,
                              1U, 1U, userPayload));
    store.put("sc0", envelope(fermentation::configuration_storage_contract::
                                  kServiceConfigurationRecordType,
                              1U, 1U, servicePayload));
    store.put("pc0", envelope(fermentation::configuration_storage_contract::
                                  kProgramCatalogRecordType,
                              1U, 1U, catalogPayload));

    fermentation::ConfigurationManifest manifest{
        fermentation::decodeChangeOrigin(1U),
        fermentation::decodeChangeOperation(2U),
        reference(fermentation::configuration_storage_contract::
                      kUserConfigurationRecordType,
                  0U, fermentation::UserConfigurationRevision{1U}, userPayload),
        reference(fermentation::configuration_storage_contract::
                      kServiceConfigurationRecordType,
                  0U, fermentation::ServiceConfigurationRevision{1U},
                  servicePayload),
        reference(fermentation::configuration_storage_contract::
                      kProgramCatalogRecordType,
                  0U, fermentation::ProgramCatalogRevision{1U},
                  catalogPayload)};
    std::string manifestPayload;
    TEST_ASSERT_TRUE(fermentation::encodeConfigurationManifestPayload(
                         manifest, manifestPayload) ==
                     fermentation::ConfigurationGraphCodecStatus::Success);
    const auto manifestReference = reference(
        fermentation::configuration_storage_contract::
            kConfigurationManifestRecordType,
        manifestSlot, fermentation::ConfigurationManifestGeneration{generation},
        manifestPayload);
    store.put(fermentation::configuration_storage_contract::
                  kConfigurationManifestSlotKeys[manifestSlot],
              envelope(fermentation::configuration_storage_contract::
                           kConfigurationManifestRecordType,
                       1U, generation, manifestPayload));
    fermentation::ConfigurationRootRecord root{manifestReference, std::nullopt};
    std::string rootPayload;
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationRootPayload(root, rootPayload) ==
        fermentation::ConfigurationGraphCodecStatus::Success);
    store.put(fermentation::configuration_storage_contract::
                  kConfigurationRootSlotKeys[rootSlot],
              envelope(fermentation::configuration_storage_contract::
                           kConfigurationRootRecordType,
                       1U, sequence, rootPayload));
    return {manifestReference, root};
}

void test_loads_complete_active_graph() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    const auto writesBefore = store.writeCount();
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(loaded.status ==
                     fermentation::ConfigurationGraphLoadStatus::
                         ConfigurationGraphAvailable);
    TEST_ASSERT_TRUE(loaded.graph.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, loaded.graph->rootSequence.value());
    TEST_ASSERT_EQUAL_STRING(
        "Fermentationsschrank",
        loaded.graph->active.userConfiguration->deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING(
        "manuengineer-dark",
        loaded.graph->active.userConfiguration->activeThemeId.c_str());
    TEST_ASSERT_EQUAL_UINT32(
        4U, loaded.graph->active.programCatalog->programs.size());
    const auto scan = graphStore.validationScan(*loaded.graph);
    TEST_ASSERT_TRUE(scan.status ==
                     fermentation::ConfigurationScanStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(writesBefore, store.writeCount());
}

void test_root_read_error_has_priority_over_valid_older_root() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    store.failRead("cr1", device_platform::StateStoreReadStatus::ReadError);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(loaded.status ==
                     fermentation::ConfigurationGraphLoadStatus::RootReadError);
    TEST_ASSERT_FALSE(loaded.graph.has_value());
}

void test_root_capacity_error_has_priority_over_valid_older_root() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    store.failRead("cr1", device_platform::StateStoreReadStatus::CapacityError);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(
        loaded.status ==
        fermentation::ConfigurationGraphLoadStatus::RootCapacityError);
    TEST_ASSERT_FALSE(loaded.graph.has_value());
}

void test_referenced_record_read_error_does_not_fall_back_to_older_root() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    store.failRead("pc0", device_platform::StateStoreReadStatus::ReadError);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(
        loaded.status ==
        fermentation::ConfigurationGraphLoadStatus::RecordReadError);
    TEST_ASSERT_FALSE(loaded.graph.has_value());
}

void test_invalid_active_branch_promotes_complete_fallback() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    const auto seeded = seedGraph(store);
    auto missingActive = seeded.manifestReference;
    missingActive.slot = SlotId{1U};
    missingActive.version = fermentation::ConfigurationManifestGeneration{2U};
    fermentation::ConfigurationRootRecord root{missingActive,
                                               seeded.manifestReference};
    std::string rootPayload;
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationRootPayload(root, rootPayload) ==
        fermentation::ConfigurationGraphCodecStatus::Success);
    store.put("cr1", envelope(fermentation::configuration_storage_contract::
                                  kConfigurationRootRecordType,
                              1U, 2U, rootPayload));
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(loaded.status ==
                     fermentation::ConfigurationGraphLoadStatus::
                         ConfigurationGraphAvailable);
    TEST_ASSERT_TRUE(loaded.graph->selectedFallback);
    TEST_ASSERT_FALSE(loaded.graph->fallback.has_value());
    TEST_ASSERT_TRUE(loaded.graph->active.manifestReference ==
                     seeded.manifestReference);
    TEST_ASSERT_EQUAL_UINT32(
        static_cast<std::uint32_t>(fermentation::UserConfigurationSchema::Version1),
        loaded.graph->active.manifest.userConfiguration.schemaVersion);
    TEST_ASSERT_EQUAL_STRING(
        "manuengineer-dark",
        loaded.graph->active.userConfiguration->activeThemeId.c_str());
}

void test_mixed_v1_v2_user_generations_remain_structurally_valid() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    const auto seeded = seedGraph(store);  // canonical V1 branch
    const fermentation::UserConfiguration v2{
        "en", "Europe/Zurich", "V2-Active", "manuengineer-dark"};
    std::string userPayload;
    TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                         v2,
                         static_cast<std::uint32_t>(
                             fermentation::UserConfigurationSchema::Version2),
                         resolver, userPayload) ==
                     fermentation::ConfigurationCodecStatus::Success);
    store.put("uc1", envelope(fermentation::configuration_storage_contract::
                                  kUserConfigurationRecordType,
                              2U, 2U, userPayload));

    // The newer manifest is a real active branch, while the older manifest
    // remains the root's valid fallback.  Both references therefore
    // participate in one graph; the V2 record is not an orphan high-water
    // artifact.
    // Reuse the exact service/catalog reference bytes from the V1 manifest;
    // only the user reference changes schema and generation.
    const auto loadedSeed = fermentation::ConfigurationGraphStore(
        store, resolver).loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(loadedSeed.graph.has_value());
    fermentation::ConfigurationManifest v2Manifest{
        fermentation::decodeChangeOrigin(1U),
        fermentation::decodeChangeOperation(2U),
        reference(fermentation::configuration_storage_contract::
                      kUserConfigurationRecordType,
                  1U, fermentation::UserConfigurationRevision{2U},
                  userPayload),
        loadedSeed.graph->active.manifest.serviceConfiguration,
        loadedSeed.graph->active.manifest.programCatalog};
    v2Manifest.userConfiguration.schemaVersion = 2U;
    std::string manifestPayload;
    TEST_ASSERT_TRUE(fermentation::encodeConfigurationManifestPayload(
                         v2Manifest, manifestPayload) ==
                     fermentation::ConfigurationGraphCodecStatus::Success);
    const auto v2ManifestReference = reference(
        fermentation::configuration_storage_contract::
            kConfigurationManifestRecordType,
        1U, fermentation::ConfigurationManifestGeneration{2U},
        manifestPayload);
    store.put(fermentation::configuration_storage_contract::
                  kConfigurationManifestSlotKeys[1U],
              envelope(fermentation::configuration_storage_contract::
                           kConfigurationManifestRecordType,
                       1U, 2U, manifestPayload));
    fermentation::ConfigurationRootRecord mixedRoot{
        v2ManifestReference, seeded.root.active};
    std::string rootPayload;
    TEST_ASSERT_TRUE(fermentation::encodeConfigurationRootPayload(
                         mixedRoot, rootPayload) ==
                     fermentation::ConfigurationGraphCodecStatus::Success);
    store.put(fermentation::configuration_storage_contract::
                  kConfigurationRootSlotKeys[1U],
              envelope(fermentation::configuration_storage_contract::
                           kConfigurationRootRecordType,
                       1U, 2U, rootPayload));

    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(loaded.graph.has_value());
    TEST_ASSERT_EQUAL_UINT32(2U, loaded.graph->active.manifest.userConfiguration.schemaVersion);
    TEST_ASSERT_TRUE(loaded.graph->fallback.has_value());
    TEST_ASSERT_EQUAL_UINT32(1U,
                             loaded.graph->fallback->manifest.userConfiguration.schemaVersion);
    TEST_ASSERT_EQUAL_STRING("V2-Active",
                             loaded.graph->active.userConfiguration->deviceName.c_str());
    const auto scan = graphStore.validationScan(*loaded.graph);
    TEST_ASSERT_TRUE(scan.status == fermentation::ConfigurationScanStatus::Success);
    TEST_ASSERT_EQUAL_UINT64(2U, scan.highWater.userConfiguration.value());
}

void test_different_bytes_under_same_document_revision_fail_closed() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    const fermentation::UserConfiguration other{"de", "Europe/Zurich",
                                                "Anderer Schrank"};
    std::string payload;
    TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                         other, resolver, payload) ==
                     fermentation::ConfigurationCodecStatus::Success);
    store.put("uc1", envelope(fermentation::configuration_storage_contract::
                                  kUserConfigurationRecordType,
                              1U, 1U, payload));
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(loaded.status ==
                     fermentation::ConfigurationGraphLoadStatus::
                         ConfigurationGraphIntegrityFailure);
    TEST_ASSERT_TRUE(loaded.diagnostics.persistentIdentityCollision);
    TEST_ASSERT_TRUE(loaded.diagnostics.globalScanBlocker);
}

void test_equal_root_sequence_with_different_bytes_has_no_tiebreak() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    auto seeded = seedGraph(store);
    auto different = seeded.root;
    different.active.payloadCrc ^= 1U;
    std::string payload;
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationRootPayload(different, payload) ==
        fermentation::ConfigurationGraphCodecStatus::Success);
    store.put("cr1", envelope(fermentation::configuration_storage_contract::
                                  kConfigurationRootRecordType,
                              1U, 1U, payload));
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(loaded.status ==
                     fermentation::ConfigurationGraphLoadStatus::
                         ConfigurationGraphIntegrityFailure);
    TEST_ASSERT_TRUE(loaded.diagnostics.persistentIdentityCollision);
    TEST_ASSERT_TRUE(loaded.diagnostics.globalScanBlocker);
}

void test_orphan_high_water_is_used_for_next_revision() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    const fermentation::UserConfiguration orphan{"de", "Europe/Zurich",
                                                 "Verwaister Inhalt"};
    std::string payload;
    TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                         orphan, resolver, payload) ==
                     fermentation::ConfigurationCodecStatus::Success);
    store.put("uc2", envelope(fermentation::configuration_storage_contract::
                                  kUserConfigurationRecordType,
                              1U, 9U, payload));
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(loaded.graph.has_value());
    auto scan = graphStore.validationScan(*loaded.graph);
    TEST_ASSERT_TRUE(scan.status ==
                     fermentation::ConfigurationScanStatus::Success);
    TEST_ASSERT_EQUAL_UINT64(9U, scan.highWater.userConfiguration.value());
    auto plan = graphStore.planSlots(*loaded.graph, scan.highWater,
                                     {true, false, false});
    TEST_ASSERT_TRUE(plan.status ==
                     fermentation::ConfigurationSlotPlanStatus::Success);
    TEST_ASSERT_EQUAL_UINT64(10U,
                             plan.plan->userConfigurationRevision->value());
    TEST_ASSERT_EQUAL_UINT64(2U, plan.plan->manifestGeneration.value());
    TEST_ASSERT_EQUAL_UINT64(2U, plan.plan->rootSequence.value());
}

void test_unknown_newer_schema_blocks_mutation_before_plan() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    store.put("uc2", envelope(fermentation::configuration_storage_contract::
                                  kUserConfigurationRecordType,
                              3U, 9U, "future"));
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(loaded.graph.has_value());
    const auto scan = graphStore.validationScan(*loaded.graph);
    TEST_ASSERT_TRUE(scan.status == fermentation::ConfigurationScanStatus::
                                        UnsupportedNewerConfigurationSchema);
}

void test_identical_root_duplicate_is_diagnostic_and_loadable() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    const auto cr0 = store.read(key("cr0"), 114U);
    store.put("cr1", cr0.value);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(loaded.graph.has_value());
    TEST_ASSERT_TRUE(loaded.diagnostics.identicalRootTie);
    TEST_ASSERT_EQUAL_UINT32(1U, loaded.diagnostics.exactDuplicateRecords);
}

fermentation::ConfigurationCommitCandidate changedCandidate(
    const fermentation::LoadedConfigurationGraph& current, const char* name) {
    return {std::make_shared<const fermentation::UserConfiguration>(
                fermentation::UserConfiguration{"de", "Europe/Zurich", name}),
            current.active.serviceConfiguration, current.active.programCatalog};
}

fermentation::ConfigurationCommitCandidate programChangedCandidate(
    const fermentation::LoadedConfigurationGraph& current) {
    auto catalog = std::make_shared<fermentation::ProgramCatalog>(
        *current.active.programCatalog);
    auto userCopy = fermentation::FactoryProgramCatalog::makeUserCopy(
        "water-kefir", "review-user-program", "Reviewprogramm");
    TEST_ASSERT_TRUE(userCopy.has_value());
    catalog->programs.push_back(std::move(*userCopy));
    return {current.active.userConfiguration,
            current.active.serviceConfiguration, std::move(catalog)};
}

std::string sameCrcDifferentBytes(std::string original) {
    TEST_ASSERT_GREATER_THAN_UINT32(8U, original.size());
    const auto target = device_platform::computeCrc32IsoHdlc(original);
    original[3] ^= static_cast<char>(0x5AU);
    const auto patchOffset = original.size() - 4U;
    for (std::size_t byte = patchOffset; byte < original.size(); ++byte) {
        original[byte] = 0;
    }
    const auto base = device_platform::computeCrc32IsoHdlc(original);
    std::array<std::uint32_t, 32U> basis{};
    std::array<std::uint32_t, 32U> basisMask{};
    for (std::uint32_t inputBit = 0U; inputBit < 32U; ++inputBit) {
        auto candidate = original;
        candidate[patchOffset + inputBit / 8U] = static_cast<char>(
            static_cast<std::uint8_t>(candidate[patchOffset + inputBit / 8U]) ^
            static_cast<std::uint8_t>(1U << (inputBit % 8U)));
        auto value = device_platform::computeCrc32IsoHdlc(candidate) ^ base;
        auto mask = static_cast<std::uint32_t>(1U << inputBit);
        for (int outputBit = 31; outputBit >= 0; --outputBit) {
            const auto outputMask = static_cast<std::uint32_t>(1U << outputBit);
            if ((value & outputMask) == 0U) {
                continue;
            }
            if (basis[static_cast<std::size_t>(outputBit)] != 0U) {
                value ^= basis[static_cast<std::size_t>(outputBit)];
                mask ^= basisMask[static_cast<std::size_t>(outputBit)];
                continue;
            }
            basis[static_cast<std::size_t>(outputBit)] = value;
            basisMask[static_cast<std::size_t>(outputBit)] = mask;
            break;
        }
    }
    auto remaining = target ^ base;
    std::uint32_t solution = 0U;
    for (int outputBit = 31; outputBit >= 0; --outputBit) {
        const auto outputMask = static_cast<std::uint32_t>(1U << outputBit);
        if ((remaining & outputMask) == 0U) {
            continue;
        }
        TEST_ASSERT_NOT_EQUAL_UINT32(
            0U, basis[static_cast<std::size_t>(outputBit)]);
        remaining ^= basis[static_cast<std::size_t>(outputBit)];
        solution ^= basisMask[static_cast<std::size_t>(outputBit)];
    }
    TEST_ASSERT_EQUAL_UINT32(0U, remaining);
    for (std::uint32_t inputBit = 0U; inputBit < 32U; ++inputBit) {
        if ((solution & static_cast<std::uint32_t>(1U << inputBit)) != 0U) {
            original[patchOffset + inputBit / 8U] = static_cast<char>(
                static_cast<std::uint8_t>(
                    original[patchOffset + inputBit / 8U]) ^
                static_cast<std::uint8_t>(1U << (inputBit % 8U)));
        }
    }
    TEST_ASSERT_EQUAL_UINT32(target,
                             device_platform::computeCrc32IsoHdlc(original));
    return original;
}

void enableSchemaOneServiceWriteForTest(
    fermentation::PreparedConfigurationCommit& prepared) {
    const auto epoch = prepared.newGraph.active.manifestReference.storageEpoch;
    prepared.changes.serviceConfiguration = true;
    prepared.slotPlan.serviceConfigurationSlot = SlotId{1U};
    prepared.slotPlan.serviceConfigurationRevision =
        fermentation::ServiceConfigurationRevision{2U};
    const std::string emptyPayload;
    prepared.newGraph.active.manifest.serviceConfiguration = reference(
        fermentation::configuration_storage_contract::
            kServiceConfigurationRecordType,
        1U, fermentation::ServiceConfigurationRevision{2U}, emptyPayload);

    std::string manifestPayload;
    TEST_ASSERT_TRUE(fermentation::encodeConfigurationManifestPayload(
                         prepared.newGraph.active.manifest, manifestPayload) ==
                     fermentation::ConfigurationGraphCodecStatus::Success);
    prepared.newGraph.active.manifestReference =
        reference(fermentation::configuration_storage_contract::
                      kConfigurationManifestRecordType,
                  prepared.slotPlan.manifestSlot.value(),
                  prepared.slotPlan.manifestGeneration, manifestPayload);
    TEST_ASSERT_TRUE(fermentation::encodeConfigurationManifestRecord(
                         prepared.newGraph.active.manifest,
                         prepared.slotPlan.manifestGeneration, epoch,
                         std::nullopt, prepared.manifestRecordBytes) ==
                     fermentation::ConfigurationGraphCodecStatus::Success);
    prepared.newGraph.active.canonicalManifestRecordBytes =
        prepared.manifestRecordBytes;
    prepared.newGraph.root.active = prepared.newGraph.active.manifestReference;
    TEST_ASSERT_TRUE(fermentation::encodeConfigurationRootRecord(
                         prepared.newGraph.root, prepared.slotPlan.rootSequence,
                         epoch, std::nullopt, prepared.rootRecordBytes) ==
                     fermentation::ConfigurationGraphCodecStatus::Success);
    prepared.newGraph.canonicalRootRecordBytes = prepared.rootRecordBytes;
}

void test_prepares_high_water_values_and_exact_fallback_before_writes() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    const auto writesBefore = store.writeCount();
    auto prepared = graphStore.prepareCommit(
        *loaded.graph, changedCandidate(*loaded.graph, "Neu"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(prepared.status ==
                     fermentation::ConfigurationCommitPrepareStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(writesBefore, store.writeCount());
    TEST_ASSERT_EQUAL_UINT64(
        2U, prepared.prepared->slotPlan.userConfigurationRevision->value());
    TEST_ASSERT_EQUAL_UINT64(
        2U, prepared.prepared->slotPlan.manifestGeneration.value());
    TEST_ASSERT_EQUAL_UINT64(2U,
                             prepared.prepared->slotPlan.rootSequence.value());
    TEST_ASSERT_TRUE(prepared.prepared->newGraph.fallback.has_value());
    TEST_ASSERT_TRUE(prepared.prepared->newGraph.fallback->manifestReference ==
                     loaded.graph->active.manifestReference);
}

void test_document_write_failure_leaves_old_graph_canonical() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    auto prepared = graphStore.prepareCommit(
        *loaded.graph, changedCandidate(*loaded.graph, "Nicht aktiv"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    store.failWrite("uc1", device_platform::StateStoreWriteStatus::WriteError,
                    false);
    const auto execution = graphStore.executePreparedCommit(*prepared.prepared);
    TEST_ASSERT_TRUE(
        execution.status ==
        fermentation::ConfigurationCommitExecutionStatus::WriteFailure);
    auto after = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_EQUAL_STRING(
        "Fermentationsschrank",
        after.graph->active.userConfiguration->deviceName.c_str());
}

void test_unknown_root_outcome_resolves_exactly_old_or_new() {
    {
        LocalStore store;
        LocalTimeZoneResolver resolver;
        seedGraph(store);
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
        auto prepared = graphStore.prepareCommit(
            *loaded.graph, changedCandidate(*loaded.graph, "Bleibt alt"),
            fermentation::decodeChangeOrigin(2U),
            fermentation::decodeChangeOperation(1U));
        store.failWrite(
            "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
            false);
        const auto execution =
            graphStore.executePreparedCommit(*prepared.prepared);
        TEST_ASSERT_TRUE(
            execution.status ==
            fermentation::ConfigurationCommitExecutionStatus::WriteFailure);
        TEST_ASSERT_TRUE(graphStore.resolveCommit(*prepared.prepared) ==
                         fermentation::ConfigurationCommitResolutionStatus::
                             ResolutionRecoveredOld);
    }
    {
        LocalStore store;
        LocalTimeZoneResolver resolver;
        seedGraph(store);
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
        auto prepared = graphStore.prepareCommit(
            *loaded.graph, changedCandidate(*loaded.graph, "Wird neu"),
            fermentation::decodeChangeOrigin(2U),
            fermentation::decodeChangeOperation(1U));
        store.failWrite(
            "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
            true);
        const auto execution =
            graphStore.executePreparedCommit(*prepared.prepared);
        TEST_ASSERT_TRUE(
            execution.status ==
            fermentation::ConfigurationCommitExecutionStatus::Activated);
        TEST_ASSERT_TRUE(graphStore.resolveCommit(*prepared.prepared) ==
                         fermentation::ConfigurationCommitResolutionStatus::
                             ResolutionRecoveredNew);
    }
}

void test_root_write_outcome_matrix_obeys_state_store_contract() {
    struct Scenario {
        device_platform::StateStoreWriteStatus writeStatus;
        bool commitValue;
        fermentation::ConfigurationCommitExecutionStatus expectedExecution;
        fermentation::ConfigurationCommitResolutionStatus expectedResolution;
    };
    constexpr std::array scenarios{
        Scenario{device_platform::StateStoreWriteStatus::WriteError, false,
                 fermentation::ConfigurationCommitExecutionStatus::WriteFailure,
                 fermentation::ConfigurationCommitResolutionStatus::
                     ResolutionRecoveredOld},
        Scenario{
            device_platform::StateStoreWriteStatus::CapacityError, false,
            fermentation::ConfigurationCommitExecutionStatus::CapacityFailure,
            fermentation::ConfigurationCommitResolutionStatus::
                ResolutionRecoveredOld},
        Scenario{device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
                 false,
                 fermentation::ConfigurationCommitExecutionStatus::WriteFailure,
                 fermentation::ConfigurationCommitResolutionStatus::
                     ResolutionRecoveredOld},
        Scenario{device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
                 true,
                 fermentation::ConfigurationCommitExecutionStatus::Activated,
                 fermentation::ConfigurationCommitResolutionStatus::
                     ResolutionRecoveredNew},
    };
    for (const auto& scenario : scenarios) {
        LocalStore store;
        LocalTimeZoneResolver resolver;
        seedGraph(store);
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
        auto prepared = graphStore.prepareCommit(
            *loaded.graph, changedCandidate(*loaded.graph, "Rootmatrix"),
            fermentation::decodeChangeOrigin(2U),
            fermentation::decodeChangeOperation(1U));
        store.failWrite("cr1", scenario.writeStatus, scenario.commitValue);
        const auto execution =
            graphStore.executePreparedCommit(*prepared.prepared);
        TEST_ASSERT_TRUE(execution.status == scenario.expectedExecution);
        TEST_ASSERT_TRUE(graphStore.resolveCommit(*prepared.prepared) ==
                         scenario.expectedResolution);
    }
}

void test_unknown_root_with_unreadable_scan_stays_indeterminate() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    auto prepared = graphStore.prepareCommit(
        *loaded.graph, changedCandidate(*loaded.graph, "Unklar"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    store.failWrite(
        "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        true);
    store.failReadsAfterWrite(
        "cr1", {{"cr0", device_platform::StateStoreReadStatus::ReadError},
                {"cr1", device_platform::StateStoreReadStatus::CapacityError}});
    const auto execution = graphStore.executePreparedCommit(*prepared.prepared);
    TEST_ASSERT_TRUE(
        execution.status ==
        fermentation::ConfigurationCommitExecutionStatus::CommitIndeterminate);
    TEST_ASSERT_TRUE(graphStore.resolveCommit(*prepared.prepared) ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionStillIndeterminate);
    store.clearReadFault("cr0");
    store.clearReadFault("cr1");
    TEST_ASSERT_TRUE(graphStore.resolveCommit(*prepared.prepared) ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRecoveredNew);
}

void test_orphaned_document_and_manifest_values_are_never_reused() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    auto abandoned = graphStore.prepareCommit(
        *loaded.graph, changedCandidate(*loaded.graph, "Verwaist"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    store.failWrite("cr1", device_platform::StateStoreWriteStatus::WriteError,
                    false);
    TEST_ASSERT_TRUE(
        graphStore.executePreparedCommit(*abandoned.prepared).status ==
        fermentation::ConfigurationCommitExecutionStatus::WriteFailure);
    auto next = graphStore.prepareCommit(
        *loaded.graph, changedCandidate(*loaded.graph, "Anderer Inhalt"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_EQUAL_UINT64(
        3U, next.prepared->slotPlan.userConfigurationRevision->value());
    TEST_ASSERT_EQUAL_UINT64(
        3U, next.prepared->slotPlan.manifestGeneration.value());
}

void test_five_commits_rotate_active_and_exact_previous_fallback() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    for (std::uint64_t index = 0U; index < 5U; ++index) {
        const auto previous = loaded.graph->active.manifestReference;
        const auto name = std::string("Schrank ") + std::to_string(index);
        auto prepared = graphStore.prepareCommit(
            *loaded.graph, changedCandidate(*loaded.graph, name.c_str()),
            fermentation::decodeChangeOrigin(2U),
            fermentation::decodeChangeOperation(1U));
        TEST_ASSERT_TRUE(prepared.prepared.has_value());
        TEST_ASSERT_TRUE(
            graphStore.executePreparedCommit(*prepared.prepared).status ==
            fermentation::ConfigurationCommitExecutionStatus::Activated);
        loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
        TEST_ASSERT_TRUE(loaded.graph.has_value());
        TEST_ASSERT_TRUE(loaded.graph->fallback.has_value());
        TEST_ASSERT_TRUE(loaded.graph->fallback->manifestReference == previous);
        TEST_ASSERT_EQUAL_UINT32(
            static_cast<std::uint32_t>(
                fermentation::UserConfigurationSchema::Version2),
            loaded.graph->active.manifest.userConfiguration.schemaVersion);
        TEST_ASSERT_EQUAL_UINT64(index + 2U,
                                 loaded.graph->rootSequence.value());
    }
}

void test_document_and_manifest_identity_collisions_all_fail_closed() {
    LocalTimeZoneResolver resolver;
    {
        LocalStore store;
        seedGraph(store);
        store.put("sc1", envelope(fermentation::configuration_storage_contract::
                                      kServiceConfigurationRecordType,
                                  1U, 1U, "different"));
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        TEST_ASSERT_TRUE(
            graphStore.loadCanonicalGraph(StorageEpoch{1U}).status ==
            fermentation::ConfigurationGraphLoadStatus::RecordCapacityError);
    }
    {
        LocalStore store;
        seedGraph(store);
        store.put("pc1", envelope(fermentation::configuration_storage_contract::
                                      kProgramCatalogRecordType,
                                  1U, 1U, "different"));
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        TEST_ASSERT_TRUE(
            graphStore.loadCanonicalGraph(StorageEpoch{1U}).status ==
            fermentation::ConfigurationGraphLoadStatus::
                ConfigurationGraphIntegrityFailure);
    }
    {
        LocalStore store;
        seedGraph(store);
        fermentation::ConfigurationManifest manifest{
            fermentation::decodeChangeOrigin(3U),
            fermentation::decodeChangeOperation(2U),
            reference(fermentation::configuration_storage_contract::
                          kUserConfigurationRecordType,
                      0U, fermentation::UserConfigurationRevision{1U}, ""),
            reference(fermentation::configuration_storage_contract::
                          kServiceConfigurationRecordType,
                      0U, fermentation::ServiceConfigurationRevision{1U}, ""),
            reference(fermentation::configuration_storage_contract::
                          kProgramCatalogRecordType,
                      0U, fermentation::ProgramCatalogRevision{1U}, "")};
        std::string manifestPayload;
        TEST_ASSERT_TRUE(fermentation::encodeConfigurationManifestPayload(
                             manifest, manifestPayload) ==
                         fermentation::ConfigurationGraphCodecStatus::Success);
        store.put("cm1", envelope(fermentation::configuration_storage_contract::
                                      kConfigurationManifestRecordType,
                                  1U, 1U, manifestPayload));
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        TEST_ASSERT_TRUE(
            graphStore.loadCanonicalGraph(StorageEpoch{1U}).status ==
            fermentation::ConfigurationGraphLoadStatus::
                ConfigurationGraphIntegrityFailure);
    }
}

void test_valid_envelope_identity_collisions_fail_closed_for_every_group() {
    LocalTimeZoneResolver resolver;
    for (const char* slot : {"uc1", "sc1", "pc1"}) {
        LocalStore store;
        seedGraph(store);
        const char* source = slot[0] == 'u'   ? "uc0"
                             : slot[0] == 's' ? "sc0"
                                              : "pc0";
        const auto original = store.read(key(source), 32813U);
        const auto decoded = device_platform::decodeEnvelope(original.value);
        TEST_ASSERT_TRUE(decoded.envelope.has_value());
        store.put(slot, envelope(decoded.envelope->recordTypeId,
                                 decoded.envelope->schemaVersion,
                                 decoded.envelope->versionValue,
                                 decoded.envelope->payload, 1234));
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        TEST_ASSERT_TRUE(
            graphStore.loadCanonicalGraph(StorageEpoch{1U}).status ==
            fermentation::ConfigurationGraphLoadStatus::
                ConfigurationGraphIntegrityFailure);
    }

    LocalStore store;
    seedGraph(store);
    const auto original = store.read(key("cm0"), 149U);
    const auto decoded = device_platform::decodeEnvelope(original.value);
    TEST_ASSERT_TRUE(decoded.envelope.has_value());
    store.put("cm1", envelope(decoded.envelope->recordTypeId,
                              decoded.envelope->schemaVersion,
                              decoded.envelope->versionValue,
                              decoded.envelope->payload, 1234));
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    TEST_ASSERT_TRUE(graphStore.loadCanonicalGraph(StorageEpoch{1U}).status ==
                     fermentation::ConfigurationGraphLoadStatus::
                         ConfigurationGraphIntegrityFailure);
}

void test_exact_record_duplicates_are_diagnostic_and_loadable() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    for (const auto& pair : {std::pair{"uc0", "uc1"}, std::pair{"sc0", "sc1"},
                             std::pair{"pc0", "pc1"}, std::pair{"cm0", "cm1"},
                             std::pair{"cr0", "cr1"}}) {
        const auto original = store.read(key(pair.first), 32813U);
        TEST_ASSERT_TRUE(original.status ==
                         device_platform::StateStoreReadStatus::Success);
        store.put(pair.second, original.value);
    }
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(loaded.status ==
                     fermentation::ConfigurationGraphLoadStatus::
                         ConfigurationGraphAvailable);
    TEST_ASSERT_TRUE(loaded.diagnostics.exactDuplicateRecords >= 5U);
    TEST_ASSERT_TRUE(loaded.diagnostics.identicalRootTie);
}

void test_high_water_overflow_blocks_before_any_write() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    const fermentation::UserConfiguration orphan{"de", "Europe/Zurich",
                                                 "Maximale Revision"};
    std::string payload;
    TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                         orphan, resolver, payload) ==
                     fermentation::ConfigurationCodecStatus::Success);
    store.put("uc2",
              envelope(fermentation::configuration_storage_contract::
                           kUserConfigurationRecordType,
                       1U, std::numeric_limits<std::uint64_t>::max(), payload));
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    const auto writesBefore = store.writeCount();
    auto prepared = graphStore.prepareCommit(
        *loaded.graph, changedCandidate(*loaded.graph, "Blockiert"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(
        prepared.status ==
        fermentation::ConfigurationCommitPrepareStatus::HighWaterOverflow);
    TEST_ASSERT_EQUAL_UINT32(writesBefore, store.writeCount());
}

void test_unusable_higher_root_advances_high_water_without_activation() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    store.put("cr1", envelope(fermentation::configuration_storage_contract::
                                  kConfigurationRootRecordType,
                              1U, 9U, "invalid-root-payload"));
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(loaded.graph.has_value());
    TEST_ASSERT_EQUAL_UINT64(1U, loaded.graph->rootSequence.value());
    auto prepared = graphStore.prepareCommit(
        *loaded.graph, changedCandidate(*loaded.graph, "Nach Rootluecke"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(prepared.prepared.has_value());
    TEST_ASSERT_EQUAL_UINT64(10U,
                             prepared.prepared->slotPlan.rootSequence.value());
}

void test_high_water_read_failures_block_before_any_write() {
    for (const auto status :
         {device_platform::StateStoreReadStatus::ReadError,
          device_platform::StateStoreReadStatus::CapacityError}) {
        LocalStore store;
        LocalTimeZoneResolver resolver;
        seedGraph(store);
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
        store.failRead("pc3", status);
        const auto writesBefore = store.writeCount();
        auto prepared = graphStore.prepareCommit(
            *loaded.graph, changedCandidate(*loaded.graph, "Blockiert"),
            fermentation::decodeChangeOrigin(2U),
            fermentation::decodeChangeOperation(1U));
        TEST_ASSERT_FALSE(prepared.prepared.has_value());
        TEST_ASSERT_EQUAL_UINT32(writesBefore, store.writeCount());
    }
}

void test_validation_scan_rejects_newer_canonical_root_before_any_write() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    const auto captured = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    seedGraph(store, 2U, 2U, 1U, 1U);
    const auto beforeWrites = store.writeCount();
    const auto scan = graphStore.validationScan(*captured.graph);
    TEST_ASSERT_TRUE(
        scan.status ==
        fermentation::ConfigurationScanStatus::ActiveBasisMismatch);
    TEST_ASSERT_EQUAL_UINT64(beforeWrites, store.writeCount());
}

void test_root_replacement_between_metadata_and_bound_read_fails_closed() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    const auto captured = graphStore.loadCanonicalGraph(StorageEpoch{1U});

    store.put("cr1", envelope(fermentation::configuration_storage_contract::
                                  kConfigurationRootRecordType,
                              1U, 2U, "invalid-root-payload"));
    LocalStore foreignStore;
    seedGraph(foreignStore, 2U, 2U, 1U, 1U);
    const auto foreign = foreignStore.read(key("cr1"), 114U);
    store.replaceAfterNextRead("cr1", "cr1", foreign.value);

    const auto scan = graphStore.validationScan(*captured.graph);
    TEST_ASSERT_TRUE(
        scan.status ==
        fermentation::ConfigurationScanStatus::ActiveBasisMismatch);
}

void test_exact_new_root_with_unreadable_new_graph_never_recovers_old() {
    for (const auto status :
         {device_platform::StateStoreReadStatus::ReadError,
          device_platform::StateStoreReadStatus::CapacityError}) {
        LocalStore store;
        LocalTimeZoneResolver resolver;
        seedGraph(store);
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
        auto prepared = graphStore.prepareCommit(
            *loaded.graph, changedCandidate(*loaded.graph, "Neu aber unlesbar"),
            fermentation::decodeChangeOrigin(2U),
            fermentation::decodeChangeOperation(1U));
        store.failWrite(
            "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
            true);
        store.failReadsAfterWrite("cr1", {{"uc1", status}});
        const auto result =
            graphStore.executePreparedCommit(*prepared.prepared);
        TEST_ASSERT_TRUE(result.status ==
                         fermentation::ConfigurationCommitExecutionStatus::
                             CommitIndeterminate);
        TEST_ASSERT_TRUE(graphStore.resolveCommit(*prepared.prepared) ==
                         fermentation::ConfigurationCommitResolutionStatus::
                             ResolutionStillIndeterminate);
    }
}

void test_exact_new_root_with_invalid_target_graph_never_recovers_old() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    auto prepared = graphStore.prepareCommit(
        *loaded.graph, changedCandidate(*loaded.graph, "Semantikkollision"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    std::string newPayload;
    TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                         *prepared.prepared->newGraph.active.userConfiguration,
                         2U, resolver, newPayload) ==
                     fermentation::ConfigurationCodecStatus::Success);
    const auto collision = sameCrcDifferentBytes(newPayload);
    store.failWrite(
        "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        true);
    store.replaceAfterWrite(
        "cr1", {{"uc1", envelope(fermentation::configuration_storage_contract::
                                     kUserConfigurationRecordType,
                                 2U, 2U, collision)}});
    const auto result = graphStore.executePreparedCommit(*prepared.prepared);
    TEST_ASSERT_TRUE(
        result.status ==
        fermentation::ConfigurationCommitExecutionStatus::RuntimeFailure);
    TEST_ASSERT_TRUE(graphStore.resolveCommit(*prepared.prepared) ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRuntimeFailure);
    TEST_ASSERT_TRUE(
        graphStore.resolveCommitDetailed(*prepared.prepared).cause ==
        fermentation::ConfigurationCommitResolutionCause::GraphSemanticFailure);
}

void test_foreign_target_root_is_neither_recovered_old_nor_new() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    auto prepared = graphStore.prepareCommit(
        *loaded.graph, changedCandidate(*loaded.graph, "Fremder Root"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    LocalStore foreign;
    seedGraph(foreign, 9U, 9U, 1U, 1U);
    const auto foreignRoot = foreign.read(key("cr1"), 114U);
    store.failWrite(
        "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        false);
    store.replaceAfterWrite("cr1", {{"cr1", foreignRoot.value}});
    const auto result = graphStore.executePreparedCommit(*prepared.prepared);
    TEST_ASSERT_TRUE(
        result.status ==
        fermentation::ConfigurationCommitExecutionStatus::CommitIndeterminate);
    TEST_ASSERT_TRUE(graphStore.resolveCommit(*prepared.prepared) ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionStillIndeterminate);
}

void test_empty_other_epoch_and_corrupt_current_root_are_distinct() {
    LocalTimeZoneResolver resolver;
    LocalStore empty;
    fermentation::ConfigurationGraphStore emptyGraphStore(empty, resolver);
    const auto emptyResult =
        emptyGraphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(emptyResult.status ==
                     fermentation::ConfigurationGraphLoadStatus::
                         ConfigurationGraphUnavailable);

    LocalStore otherEpoch;
    std::string bytes;
    TEST_ASSERT_TRUE(device_platform::encodeEnvelope(
                         {fermentation::configuration_storage_contract::
                              kConfigurationRootRecordType,
                          1U, StorageEpoch{2U}, 1U, std::nullopt, "old"},
                         bytes, 114U) ==
                     device_platform::EnvelopeEncodeStatus::Success);
    otherEpoch.put("cr0", bytes);
    fermentation::ConfigurationGraphStore otherGraphStore(otherEpoch, resolver);
    const auto otherResult =
        otherGraphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(otherResult.status ==
                     fermentation::ConfigurationGraphLoadStatus::
                         ConfigurationGraphUnavailableOtherEpoch);

    LocalStore corrupt;
    corrupt.put("cr0", "not-an-envelope");
    fermentation::ConfigurationGraphStore corruptGraphStore(corrupt, resolver);
    const auto corruptResult =
        corruptGraphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(corruptResult.status ==
                     fermentation::ConfigurationGraphLoadStatus::
                         ConfigurationGraphIntegrityFailure);

    LocalStore orphanedGeneration;
    seedGraph(orphanedGeneration);
    orphanedGeneration.erase("cr0");
    orphanedGeneration.erase("cr1");
    fermentation::ConfigurationGraphStore orphanedGraphStore(orphanedGeneration,
                                                             resolver);
    const auto orphanedResult =
        orphanedGraphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(orphanedResult.status ==
                     fermentation::ConfigurationGraphLoadStatus::
                         ConfigurationGraphIntegrityFailure);
}

void test_unmodified_document_high_water_maximum_does_not_block() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    const auto catalog = fermentation::makeFactoryProgramCatalog();
    std::string payload;
    TEST_ASSERT_TRUE(
        fermentation::encodeProgramCatalogPayload(catalog, payload) ==
        fermentation::ConfigurationCodecStatus::Success);
    store.put("pc3",
              envelope(fermentation::configuration_storage_contract::
                           kProgramCatalogRecordType,
                       1U, std::numeric_limits<std::uint64_t>::max(), payload));
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    auto prepared = graphStore.prepareCommit(
        *loaded.graph, changedCandidate(*loaded.graph, "Nur Benutzer"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(prepared.status ==
                     fermentation::ConfigurationCommitPrepareStatus::Success);
    TEST_ASSERT_FALSE(prepared.prepared->changes.programCatalog);
    TEST_ASSERT_FALSE(
        prepared.prepared->slotPlan.programCatalogRevision.has_value());
}

void test_only_changed_document_high_water_is_checked_for_overflow() {
    LocalTimeZoneResolver resolver;
    {
        LocalStore store;
        seedGraph(store);
        const fermentation::UserConfiguration user{
            "de", "Europe/Zurich", "Verwaiste maximale Userrevision"};
        std::string payload;
        TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                             user, resolver, payload) ==
                         fermentation::ConfigurationCodecStatus::Success);
        store.put("uc3", envelope(fermentation::configuration_storage_contract::
                                      kUserConfigurationRecordType,
                                  1U, std::numeric_limits<std::uint64_t>::max(),
                                  payload));
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
        const auto prepared = graphStore.prepareCommit(
            *loaded.graph, programChangedCandidate(*loaded.graph),
            fermentation::decodeChangeOrigin(2U),
            fermentation::decodeChangeOperation(1U));
        TEST_ASSERT_TRUE(prepared.prepared.has_value());
        TEST_ASSERT_FALSE(
            prepared.prepared->slotPlan.userConfigurationRevision.has_value());
    }
    {
        LocalStore store;
        seedGraph(store);
        store.put("sc3",
                  envelope(fermentation::configuration_storage_contract::
                               kServiceConfigurationRecordType,
                           1U, std::numeric_limits<std::uint64_t>::max(), ""));
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
        const auto prepared = graphStore.prepareCommit(
            *loaded.graph,
            changedCandidate(*loaded.graph, "User trotz Service"),
            fermentation::decodeChangeOrigin(2U),
            fermentation::decodeChangeOperation(1U));
        TEST_ASSERT_TRUE(prepared.prepared.has_value());
        TEST_ASSERT_FALSE(prepared.prepared->slotPlan
                              .serviceConfigurationRevision.has_value());
    }
    {
        LocalStore store;
        seedGraph(store);
        const auto catalog = fermentation::makeFactoryProgramCatalog();
        std::string payload;
        TEST_ASSERT_TRUE(
            fermentation::encodeProgramCatalogPayload(catalog, payload) ==
            fermentation::ConfigurationCodecStatus::Success);
        store.put("pc3", envelope(fermentation::configuration_storage_contract::
                                      kProgramCatalogRecordType,
                                  1U, std::numeric_limits<std::uint64_t>::max(),
                                  payload));
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
        const auto beforeWrites = store.writeCount();
        const auto prepared = graphStore.prepareCommit(
            *loaded.graph, programChangedCandidate(*loaded.graph),
            fermentation::decodeChangeOrigin(2U),
            fermentation::decodeChangeOperation(1U));
        TEST_ASSERT_TRUE(
            prepared.status ==
            fermentation::ConfigurationCommitPrepareStatus::HighWaterOverflow);
        TEST_ASSERT_EQUAL_UINT32(beforeWrites, store.writeCount());
    }
}

void test_fallback_change_between_preview_and_commit_blocks_before_write() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto first = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    auto firstCommit = graphStore.prepareCommit(
        *first.graph, changedCandidate(*first.graph, "Generation zwei"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(
        graphStore.executePreparedCommit(*firstCommit.prepared).status ==
        fermentation::ConfigurationCommitExecutionStatus::Activated);
    const auto captured = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(captured.graph->fallback.has_value());
    store.put("pc0", "corrupt-fallback-record");
    const auto beforeWrites = store.writeCount();
    const auto prepared = graphStore.prepareCommit(
        *captured.graph, changedCandidate(*captured.graph, "Generation drei"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_FALSE(prepared.prepared.has_value());
    TEST_ASSERT_TRUE(
        prepared.status ==
        fermentation::ConfigurationCommitPrepareStatus::IntegrityFailure);
    TEST_ASSERT_EQUAL_UINT32(beforeWrites, store.writeCount());
}

void test_complete_target_graph_is_verified_before_root_write() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    auto prepared = graphStore.prepareCommit(
        *loaded.graph, changedCandidate(*loaded.graph, "Zielpruefung"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    store.failReadsAfterWrite(
        "cm1", {{"pc0", device_platform::StateStoreReadStatus::ReadError}});
    const auto result = graphStore.executePreparedCommit(*prepared.prepared);
    TEST_ASSERT_TRUE(
        result.status ==
        fermentation::ConfigurationCommitExecutionStatus::RuntimeFailure);
    TEST_ASSERT_TRUE(
        result.phase ==
        fermentation::ConfigurationCommitFailurePhase::TargetGraphVerification);
    TEST_ASSERT_TRUE(
        result.resolutionCause ==
        fermentation::ConfigurationCommitResolutionCause::GraphReadError);
    TEST_ASSERT_TRUE(store.read(key("cr1"), 114U).status ==
                     device_platform::StateStoreReadStatus::NotFound);
}

void test_target_graph_failure_causes_distinguish_envelope_and_reference() {
    {
        LocalStore store;
        LocalTimeZoneResolver resolver;
        seedGraph(store);
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
        auto prepared = graphStore.prepareCommit(
            *loaded.graph, changedCandidate(*loaded.graph, "Envelopefehler"),
            fermentation::decodeChangeOrigin(2U),
            fermentation::decodeChangeOperation(1U));
        store.replaceAfterWrite("cm1", {{"uc1", "corrupt-envelope"}});
        const auto result =
            graphStore.executePreparedCommit(*prepared.prepared);
        TEST_ASSERT_TRUE(result.resolutionCause ==
                         fermentation::ConfigurationCommitResolutionCause::
                             GraphEnvelopeOrCrcFailure);
        TEST_ASSERT_TRUE(store.read(key("cr1"), 114U).status ==
                         device_platform::StateStoreReadStatus::NotFound);
    }
    {
        LocalStore store;
        LocalTimeZoneResolver resolver;
        seedGraph(store);
        fermentation::ConfigurationGraphStore graphStore(store, resolver);
        const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
        auto prepared = graphStore.prepareCommit(
            *loaded.graph, changedCandidate(*loaded.graph, "Referenzfehler"),
            fermentation::decodeChangeOrigin(2U),
            fermentation::decodeChangeOperation(1U));
        const fermentation::UserConfiguration other{
            "de", "Europe/Zurich", "Nicht referenzierte Payload"};
        std::string payload;
        TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                             other, resolver, payload) ==
                         fermentation::ConfigurationCodecStatus::Success);
        store.replaceAfterWrite(
            "cm1",
            {{"uc1", envelope(fermentation::configuration_storage_contract::
                                  kUserConfigurationRecordType,
                              1U, 2U, payload)}});
        const auto result =
            graphStore.executePreparedCommit(*prepared.prepared);
        TEST_ASSERT_TRUE(result.resolutionCause ==
                         fermentation::ConfigurationCommitResolutionCause::
                             GraphReferenceFailure);
        TEST_ASSERT_TRUE(store.read(key("cr1"), 114U).status ==
                         device_platform::StateStoreReadStatus::NotFound);
    }
}

void test_change_mask_is_derived_and_committed_graph_reloads_identically() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});

    auto candidate = programChangedCandidate(*loaded.graph);
    const auto expectedCatalog = candidate.programCatalog;
    auto prepared = graphStore.prepareCommit(
        *loaded.graph, candidate, fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(prepared.prepared.has_value());
    TEST_ASSERT_FALSE(prepared.prepared->changes.userConfiguration);
    TEST_ASSERT_FALSE(prepared.prepared->changes.serviceConfiguration);
    TEST_ASSERT_TRUE(prepared.prepared->changes.programCatalog);
    TEST_ASSERT_TRUE(
        graphStore.executePreparedCommit(*prepared.prepared).status ==
        fermentation::ConfigurationCommitExecutionStatus::Activated);
    const auto reloaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    TEST_ASSERT_TRUE(reloaded.graph.has_value());
    TEST_ASSERT_TRUE(fermentation::configurationContentEquals(
        *expectedCatalog, *reloaded.graph->active.programCatalog));
}

void test_crc_collision_cannot_replace_bound_typed_document_before_root() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    seedGraph(store);
    fermentation::ConfigurationGraphStore graphStore(store, resolver);
    auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
    auto prepared = graphStore.prepareCommit(
        *loaded.graph, changedCandidate(*loaded.graph, "CRC-Bindung"),
        fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    const auto currentProgram = store.read(
        key("pc0"),
        fermentation::configuration_limits::kMaximumProgramCatalogPayloadBytes +
            45U);
    const auto decoded = device_platform::decodeEnvelope(currentProgram.value);
    TEST_ASSERT_TRUE(decoded.envelope.has_value());
    const auto collision = sameCrcDifferentBytes(decoded.envelope->payload);
    TEST_ASSERT_FALSE(decoded.envelope->payload == collision);
    std::vector<std::pair<std::string, std::string>> replacements;
    replacements.emplace_back(
        "pc0", envelope(decoded.envelope->recordTypeId,
                        decoded.envelope->schemaVersion,
                        decoded.envelope->versionValue, collision));
    store.replaceAfterWrite("cm1", std::move(replacements));
    const auto result = graphStore.executePreparedCommit(*prepared.prepared);
    TEST_ASSERT_TRUE(
        result.status ==
        fermentation::ConfigurationCommitExecutionStatus::RuntimeFailure);
    TEST_ASSERT_TRUE(
        result.phase ==
        fermentation::ConfigurationCommitFailurePhase::TargetGraphVerification);
    TEST_ASSERT_TRUE(
        result.resolutionCause ==
        fermentation::ConfigurationCommitResolutionCause::GraphSemanticFailure);
    TEST_ASSERT_TRUE(store.read(key("cr1"), 114U).status ==
                     device_platform::StateStoreReadStatus::NotFound);
}

void test_each_pre_root_write_phase_obeys_state_store_outcome_contract() {
    enum class Phase : std::uint8_t { User, Service, Program, Manifest };
    struct Scenario {
        device_platform::StateStoreWriteStatus writeStatus;
        bool commitValue;
        fermentation::ConfigurationCommitExecutionStatus expected;
    };
    constexpr std::array scenarios{
        Scenario{
            device_platform::StateStoreWriteStatus::WriteError, false,
            fermentation::ConfigurationCommitExecutionStatus::WriteFailure},
        Scenario{
            device_platform::StateStoreWriteStatus::CapacityError, false,
            fermentation::ConfigurationCommitExecutionStatus::CapacityFailure},
        Scenario{
            device_platform::StateStoreWriteStatus::CommitOutcomeUnknown, false,
            fermentation::ConfigurationCommitExecutionStatus::WriteFailure},
        Scenario{device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
                 true,
                 fermentation::ConfigurationCommitExecutionStatus::Activated},
    };
    for (const auto phase :
         {Phase::User, Phase::Service, Phase::Program, Phase::Manifest}) {
        for (const auto& scenario : scenarios) {
            LocalStore store;
            LocalTimeZoneResolver resolver;
            seedGraph(store);
            fermentation::ConfigurationGraphStore graphStore(store, resolver);
            const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
            const auto candidate =
                phase == Phase::Program
                    ? programChangedCandidate(*loaded.graph)
                    : changedCandidate(*loaded.graph, "Fehlermatrix");
            auto prepared = graphStore.prepareCommit(
                *loaded.graph, candidate, fermentation::decodeChangeOrigin(2U),
                fermentation::decodeChangeOperation(1U));
            TEST_ASSERT_TRUE(prepared.prepared.has_value());
            if (phase == Phase::Service) {
                enableSchemaOneServiceWriteForTest(*prepared.prepared);
            }
            const char* target = "cm1";
            if (phase == Phase::User || phase == Phase::Service) {
                target = phase == Phase::User ? "uc1" : "sc1";
            } else if (phase == Phase::Program) {
                target = "pc1";
            }
            store.failWrite(target, scenario.writeStatus, scenario.commitValue);
            const auto execution =
                graphStore.executePreparedCommit(*prepared.prepared);
            TEST_ASSERT_TRUE(execution.status == scenario.expected);
            if (scenario.expected !=
                fermentation::ConfigurationCommitExecutionStatus::Activated) {
                TEST_ASSERT_TRUE(
                    store.read(key("cr1"), 114U).status ==
                    device_platform::StateStoreReadStatus::NotFound);
            }
        }
    }
}

void test_unknown_pre_root_readback_failures_never_reach_root_write() {
    enum class Phase : std::uint8_t { User, Service, Program, Manifest };
    for (const auto readStatus :
         {device_platform::StateStoreReadStatus::ReadError,
          device_platform::StateStoreReadStatus::CapacityError}) {
        for (const auto phase :
             {Phase::User, Phase::Service, Phase::Program, Phase::Manifest}) {
            LocalStore store;
            LocalTimeZoneResolver resolver;
            seedGraph(store);
            fermentation::ConfigurationGraphStore graphStore(store, resolver);
            const auto loaded = graphStore.loadCanonicalGraph(StorageEpoch{1U});
            auto prepared = graphStore.prepareCommit(
                *loaded.graph,
                phase == Phase::Program
                    ? programChangedCandidate(*loaded.graph)
                    : changedCandidate(*loaded.graph, "Readbackmatrix"),
                fermentation::decodeChangeOrigin(2U),
                fermentation::decodeChangeOperation(1U));
            if (phase == Phase::Service) {
                enableSchemaOneServiceWriteForTest(*prepared.prepared);
            }
            const char* target = "cm1";
            if (phase == Phase::User || phase == Phase::Service) {
                target = phase == Phase::User ? "uc1" : "sc1";
            } else if (phase == Phase::Program) {
                target = "pc1";
            }
            store.failWrite(
                target,
                device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
                true);
            store.failReadsAfterWrite(target, {{target, readStatus}});
            const auto execution =
                graphStore.executePreparedCommit(*prepared.prepared);
            TEST_ASSERT_TRUE(
                execution.status ==
                    fermentation::ConfigurationCommitExecutionStatus::
                        WriteFailure ||
                execution.status ==
                    fermentation::ConfigurationCommitExecutionStatus::
                        CapacityFailure);
            TEST_ASSERT_TRUE(store.read(key("cr1"), 114U).status ==
                             device_platform::StateStoreReadStatus::NotFound);
        }
    }
}

void test_initial_graph_plan_uses_safe_slots_and_fixed_epoch_identities() {
    LocalStore empty;
    LocalTimeZoneResolver resolver;
    fermentation::ConfigurationGraphStore emptyGraph(empty, resolver);
    auto initial = emptyGraph.prepareInitialGraph(
        StorageEpoch{1U}, fermentation::decodeChangeOperation(2U));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::InitialConfigurationPrepareStatus::Success),
        static_cast<int>(initial.status));
    TEST_ASSERT_TRUE(initial.prepared.has_value());
    TEST_ASSERT_EQUAL_UINT32(
        fermentation::kCurrentUserConfigurationSchemaVersion,
        initial.prepared->graph.active.manifest.userConfiguration.schemaVersion);
    TEST_ASSERT_EQUAL_UINT32(0U, initial.prepared->slotPlan.rootSlot.value());
    TEST_ASSERT_EQUAL_UINT64(1U,
                             initial.prepared->slotPlan.rootSequence.value());
    TEST_ASSERT_FALSE(initial.prepared->graph.root.fallback.has_value());

    LocalStore priorEpoch;
    static_cast<void>(seedGraph(priorEpoch));
    fermentation::ConfigurationGraphStore resetGraph(priorEpoch, resolver);
    auto reset = resetGraph.prepareInitialGraph(
        StorageEpoch{2U}, fermentation::decodeChangeOperation(5U));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::InitialConfigurationPrepareStatus::Success),
        static_cast<int>(reset.status));
    TEST_ASSERT_TRUE(reset.prepared.has_value());
    TEST_ASSERT_EQUAL_UINT64(
        2U,
        reset.prepared->graph.active.manifestReference.storageEpoch.value());
    TEST_ASSERT_EQUAL_UINT64(
        1U, reset.prepared->slotPlan.manifestGeneration.value());
}

void test_initial_graph_plan_rejects_same_epoch_identity_collision() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    store.put("uc0",
              envelope(fermentation::configuration_storage_contract::
                           kUserConfigurationRecordType,
                       1U, 1U, "different", std::nullopt, StorageEpoch{2U}));
    fermentation::ConfigurationGraphStore graph(store, resolver);
    const auto prepared = graph.prepareInitialGraph(
        StorageEpoch{2U}, fermentation::decodeChangeOperation(5U));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::InitialConfigurationPrepareStatus::IntegrityFailure),
        static_cast<int>(prepared.status));
}

void test_initial_graph_keeps_maximum_old_catalog_as_descriptor_only() {
    LocalStore store;
    LocalTimeZoneResolver resolver;
    const std::string maximumPayload(
        fermentation::configuration_limits::kMaximumProgramCatalogPayloadBytes,
        'x');
    store.put("pc0",
              envelope(fermentation::configuration_storage_contract::
                           kProgramCatalogRecordType,
                       1U, 1U, maximumPayload, std::nullopt, StorageEpoch{1U}));
    fermentation::ConfigurationGraphStore graph(store, resolver);
    const auto prepared = graph.prepareInitialGraph(
        StorageEpoch{2U}, fermentation::decodeChangeOperation(5U));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::InitialConfigurationPrepareStatus::Success),
        static_cast<int>(prepared.status));
    TEST_ASSERT_TRUE(prepared.prepared.has_value());
    TEST_ASSERT_FALSE(
        prepared.prepared->previousTargetProgramRecord.wasNotFound);
    TEST_ASSERT_EQUAL_UINT32(
        fermentation::configuration_limits::kMaximumProgramCatalogPayloadBytes +
            37U,
        prepared.prepared->previousTargetProgramRecord.recordLength);
    TEST_ASSERT_LESS_THAN(
        fermentation::configuration_limits::kMaximumProgramCatalogPayloadBytes,
        prepared.prepared->peakDocumentEnvelopeCapacity);
    // The old 32805-byte envelope (32768-byte payload plus the 37-byte
    // non-UTC envelope) is genuinely read in full while scanning for a safe
    // slot, even though only its small descriptor survives afterwards; the
    // resource report must show that real transient buffer, not just the
    // small kept descriptor.
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
        fermentation::configuration_limits::kMaximumProgramCatalogPayloadBytes +
            37U,
        prepared.prepared->peakSlotScanReadCapacity);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
        fermentation::configuration_limits::kMaximumProgramCatalogPayloadBytes +
            45U,
        prepared.prepared->peakSlotScanReadCapacity);
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_loads_complete_active_graph);
    RUN_TEST(test_root_read_error_has_priority_over_valid_older_root);
    RUN_TEST(test_root_capacity_error_has_priority_over_valid_older_root);
    RUN_TEST(
        test_referenced_record_read_error_does_not_fall_back_to_older_root);
    RUN_TEST(test_invalid_active_branch_promotes_complete_fallback);
    RUN_TEST(test_mixed_v1_v2_user_generations_remain_structurally_valid);
    RUN_TEST(test_different_bytes_under_same_document_revision_fail_closed);
    RUN_TEST(test_equal_root_sequence_with_different_bytes_has_no_tiebreak);
    RUN_TEST(test_orphan_high_water_is_used_for_next_revision);
    RUN_TEST(test_unknown_newer_schema_blocks_mutation_before_plan);
    RUN_TEST(test_identical_root_duplicate_is_diagnostic_and_loadable);
    RUN_TEST(test_prepares_high_water_values_and_exact_fallback_before_writes);
    RUN_TEST(test_document_write_failure_leaves_old_graph_canonical);
    RUN_TEST(test_unknown_root_outcome_resolves_exactly_old_or_new);
    RUN_TEST(test_root_write_outcome_matrix_obeys_state_store_contract);
    RUN_TEST(test_unknown_root_with_unreadable_scan_stays_indeterminate);
    RUN_TEST(test_orphaned_document_and_manifest_values_are_never_reused);
    RUN_TEST(test_five_commits_rotate_active_and_exact_previous_fallback);
    RUN_TEST(test_document_and_manifest_identity_collisions_all_fail_closed);
    RUN_TEST(
        test_valid_envelope_identity_collisions_fail_closed_for_every_group);
    RUN_TEST(test_exact_record_duplicates_are_diagnostic_and_loadable);
    RUN_TEST(test_high_water_overflow_blocks_before_any_write);
    RUN_TEST(test_unusable_higher_root_advances_high_water_without_activation);
    RUN_TEST(test_high_water_read_failures_block_before_any_write);
    RUN_TEST(
        test_validation_scan_rejects_newer_canonical_root_before_any_write);
    RUN_TEST(
        test_root_replacement_between_metadata_and_bound_read_fails_closed);
    RUN_TEST(test_exact_new_root_with_unreadable_new_graph_never_recovers_old);
    RUN_TEST(test_exact_new_root_with_invalid_target_graph_never_recovers_old);
    RUN_TEST(test_foreign_target_root_is_neither_recovered_old_nor_new);
    RUN_TEST(test_empty_other_epoch_and_corrupt_current_root_are_distinct);
    RUN_TEST(test_unmodified_document_high_water_maximum_does_not_block);
    RUN_TEST(test_only_changed_document_high_water_is_checked_for_overflow);
    RUN_TEST(
        test_fallback_change_between_preview_and_commit_blocks_before_write);
    RUN_TEST(test_complete_target_graph_is_verified_before_root_write);
    RUN_TEST(
        test_change_mask_is_derived_and_committed_graph_reloads_identically);
    RUN_TEST(
        test_crc_collision_cannot_replace_bound_typed_document_before_root);
    RUN_TEST(
        test_target_graph_failure_causes_distinguish_envelope_and_reference);
    RUN_TEST(test_each_pre_root_write_phase_obeys_state_store_outcome_contract);
    RUN_TEST(test_unknown_pre_root_readback_failures_never_reach_root_write);
    RUN_TEST(
        test_initial_graph_plan_uses_safe_slots_and_fixed_epoch_identities);
    RUN_TEST(test_initial_graph_plan_rejects_same_epoch_identity_collision);
    RUN_TEST(test_initial_graph_keeps_maximum_old_catalog_as_descriptor_only);
    return UNITY_END();
}
