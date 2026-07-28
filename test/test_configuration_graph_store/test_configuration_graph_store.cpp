#include <unity.h>

#include <cstddef>
#include <cstdint>
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
        return {device_platform::StateStoreReadStatus::Success, found->second};
    }

    void put(const char* keyValue, std::string value) {
        values_[keyValue] = std::move(value);
    }

    void failRead(const char* keyValue,
                  device_platform::StateStoreReadStatus status) {
        faults_[keyValue] = status;
    }

    void clearReadFault(const char* keyValue) { faults_.erase(keyValue); }

    void failWrite(const char* keyValue,
                   device_platform::StateStoreWriteStatus status,
                   bool commitValue) {
        writeFaults_[keyValue] = {status, commitValue};
    }

    void failReadsAfterWrite(
        const char* writeKey,
        std::vector<
            std::pair<std::string, device_platform::StateStoreReadStatus>>
            faults) {
        readFaultsAfterWrite_[writeKey] = std::move(faults);
    }

    [[nodiscard]] std::size_t writeCount() const { return writeCount_; }

   private:
    struct WriteFault {
        device_platform::StateStoreWriteStatus status;
        bool commitValue;
    };
    std::map<std::string, std::string> values_;
    mutable std::map<std::string, device_platform::StateStoreReadStatus>
        faults_;
    std::map<std::string, WriteFault> writeFaults_;
    std::map<std::string,
             std::vector<
                 std::pair<std::string, device_platform::StateStoreReadStatus>>>
        readFaultsAfterWrite_;
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
                     std::uint64_t version, const std::string& payload) {
    std::string bytes;
    TEST_ASSERT_TRUE(
        device_platform::encodeEnvelope(
            {type, schema, StorageEpoch{1U}, version, std::nullopt, payload},
            bytes, payload.size() + 45U) ==
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
    TEST_ASSERT_EQUAL_UINT32(
        4U, loaded.graph->active.programCatalog->programs.size());
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
                              2U, 9U, "future"));
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
            current.active.serviceConfiguration,
            current.active.programCatalog,
            {true, false, false}};
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
    store.failWrite("cm1", device_platform::StateStoreWriteStatus::WriteError,
                    true);
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

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_loads_complete_active_graph);
    RUN_TEST(test_root_read_error_has_priority_over_valid_older_root);
    RUN_TEST(test_different_bytes_under_same_document_revision_fail_closed);
    RUN_TEST(test_equal_root_sequence_with_different_bytes_has_no_tiebreak);
    RUN_TEST(test_orphan_high_water_is_used_for_next_revision);
    RUN_TEST(test_unknown_newer_schema_blocks_mutation_before_plan);
    RUN_TEST(test_identical_root_duplicate_is_diagnostic_and_loadable);
    RUN_TEST(test_prepares_high_water_values_and_exact_fallback_before_writes);
    RUN_TEST(test_document_write_failure_leaves_old_graph_canonical);
    RUN_TEST(test_unknown_root_outcome_resolves_exactly_old_or_new);
    RUN_TEST(test_unknown_root_with_unreadable_scan_stays_indeterminate);
    RUN_TEST(test_orphaned_document_and_manifest_values_are_never_reused);
    RUN_TEST(test_five_commits_rotate_active_and_exact_previous_fallback);
    RUN_TEST(test_document_and_manifest_identity_collisions_all_fail_closed);
    RUN_TEST(test_unusable_higher_root_advances_high_water_without_activation);
    RUN_TEST(test_high_water_read_failures_block_before_any_write);
    return UNITY_END();
}
