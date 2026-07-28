#include <unity.h>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "configuration_documents.hpp"
#include "configuration_document_codec.hpp"
#include "configuration_graph.hpp"
#include "configuration_graph_codec.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_limits.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_service.hpp"
#include "configuration_storage_contract.hpp"
#include "crc32.hpp"
#include "state_store.hpp"
#include "state_store_key.hpp"
#include "storage_envelope.hpp"

namespace {

std::atomic<std::size_t> gLiveAllocBytes{0U};
std::atomic<std::size_t> gPeakAllocBytes{0U};
constexpr std::size_t kAllocHeader = alignof(std::max_align_t);

void recordAllocation(std::size_t size) {
    const auto live = gLiveAllocBytes.fetch_add(size) + size;
    auto peak = gPeakAllocBytes.load();
    while (peak < live && !gPeakAllocBytes.compare_exchange_weak(peak, live)) {
    }
}

}  // namespace

void* operator new(std::size_t size) {
    void* raw = std::malloc(size + kAllocHeader);
    if (raw == nullptr) {
        throw std::bad_alloc();
    }
    *static_cast<std::size_t*>(raw) = size;
    recordAllocation(size);
    return static_cast<char*>(raw) + kAllocHeader;
}

void operator delete(void* ptr) noexcept {
    if (ptr == nullptr) {
        return;
    }
    void* raw = static_cast<char*>(ptr) - kAllocHeader;
    gLiveAllocBytes.fetch_sub(*static_cast<std::size_t*>(raw));
    std::free(raw);
}

void operator delete(void* ptr, std::size_t) noexcept { operator delete(ptr); }

namespace {

class LocalStore final : public device_platform::IStateStore {
   public:
    device_platform::StateStoreWriteStatus write(
        const device_platform::StateStoreKey& key,
        const std::string& value) override {
        ++writeCount_;
        const auto fault = writeFaults_.find(key.bytes());
        if (fault == writeFaults_.end() || fault->second.commitValue) {
            values_[key.bytes()] = value;
        }
        const auto readFaults = readFaultsAfterWrite_.find(key.bytes());
        if (readFaults != readFaultsAfterWrite_.end()) {
            for (const auto& [readKey, status] : readFaults->second) {
                readFaults_[readKey] = status;
            }
        }
        if (fault != writeFaults_.end()) {
            return fault->second.status;
        }
        return device_platform::StateStoreWriteStatus::Success;
    }
    device_platform::StateStoreReadResult read(
        const device_platform::StateStoreKey& key,
        std::size_t maxBytes) const override {
        const auto fault = readFaults_.find(key.bytes());
        if (fault != readFaults_.end()) {
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

    void put(const char* key, std::string value) {
        values_[key] = std::move(value);
    }

    void failWrite(const char* key,
                   device_platform::StateStoreWriteStatus status,
                   bool commitValue) {
        writeFaults_[key] = {status, commitValue};
    }

    void clearWriteFault(const char* key) { writeFaults_.erase(key); }

    void failReadsAfterWrite(
        const char* writeKey,
        std::vector<
            std::pair<std::string, device_platform::StateStoreReadStatus>>
            faults) {
        readFaultsAfterWrite_[writeKey] = std::move(faults);
    }

    void failRead(const char* key,
                  device_platform::StateStoreReadStatus status) {
        readFaults_[key] = status;
    }

    void clearReadFault(const char* key) { readFaults_.erase(key); }

    [[nodiscard]] std::size_t writeCount() const { return writeCount_; }

   private:
    struct WriteFault {
        device_platform::StateStoreWriteStatus status;
        bool commitValue;
    };
    std::map<std::string, std::string> values_;
    std::map<std::string, WriteFault> writeFaults_;
    mutable std::map<std::string, device_platform::StateStoreReadStatus>
        readFaults_;
    std::map<std::string,
             std::vector<
                 std::pair<std::string, device_platform::StateStoreReadStatus>>>
        readFaultsAfterWrite_;
    std::size_t writeCount_{0U};
};

class Resolver final : public device_platform::ITimeZoneResolver {
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

std::string envelope(device_platform::RecordTypeId type, std::uint64_t version,
                     const std::string& payload) {
    std::string bytes;
    TEST_ASSERT_TRUE(device_platform::encodeEnvelope(
                         {type, 1U, device_platform::StorageEpoch{1U}, version,
                          std::nullopt, payload},
                         bytes, payload.size() + 45U) ==
                     device_platform::EnvelopeEncodeStatus::Success);
    return bytes;
}

template <typename Version>
fermentation::ConfigurationRecordReference<Version> reference(
    device_platform::RecordTypeId type, std::uint32_t slot, Version version,
    const std::string& payload) {
    return {type,
            device_platform::SlotId{slot},
            version,
            1U,
            static_cast<std::uint32_t>(payload.size()),
            device_platform::computeCrc32IsoHdlc(payload),
            device_platform::StorageEpoch{1U}};
}

fermentation::LoadedConfigurationGraph seedGraphWithCatalog(
    LocalStore& store, const Resolver& resolver,
    const fermentation::ProgramCatalog& catalog) {
    const fermentation::UserConfiguration user{"de", "Europe/Zurich",
                                               "Fermentationsschrank"};
    const fermentation::ServiceConfiguration service;
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
                              1U, userPayload));
    store.put("sc0", envelope(fermentation::configuration_storage_contract::
                                  kServiceConfigurationRecordType,
                              1U, servicePayload));
    store.put("pc0", envelope(fermentation::configuration_storage_contract::
                                  kProgramCatalogRecordType,
                              1U, catalogPayload));
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
    auto manifestReference = reference(
        fermentation::configuration_storage_contract::
            kConfigurationManifestRecordType,
        0U, fermentation::ConfigurationManifestGeneration{1U}, manifestPayload);
    const auto manifestBytes =
        envelope(fermentation::configuration_storage_contract::
                     kConfigurationManifestRecordType,
                 1U, manifestPayload);
    store.put("cm0", manifestBytes);
    fermentation::ConfigurationGraphBranch branch{
        manifestReference,
        manifest,
        std::make_shared<const fermentation::UserConfiguration>(user),
        std::make_shared<const fermentation::ServiceConfiguration>(service),
        std::make_shared<const fermentation::ProgramCatalog>(catalog),
        manifestBytes};
    fermentation::ConfigurationRootRecord root{manifestReference, std::nullopt};
    std::string rootPayload;
    TEST_ASSERT_TRUE(
        fermentation::encodeConfigurationRootPayload(root, rootPayload) ==
        fermentation::ConfigurationGraphCodecStatus::Success);
    const auto rootBytes =
        envelope(fermentation::configuration_storage_contract::
                     kConfigurationRootRecordType,
                 1U, rootPayload);
    store.put("cr0", rootBytes);
    return {device_platform::SlotId{0U},
            fermentation::ConfigurationRootSequence{1U},
            root,
            rootBytes,
            std::move(branch),
            std::nullopt,
            false};
}

fermentation::LoadedConfigurationGraph seedGraph(LocalStore& store,
                                                 const Resolver& resolver) {
    return seedGraphWithCatalog(store, resolver,
                                fermentation::makeFactoryProgramCatalog());
}

struct Fixture {
    LocalStore store;
    Resolver resolver;
    fermentation::ConfigurationGraphStore graphStore{store, resolver};
    fermentation::ConfigurationMutationCoordinator coordinator;
    fermentation::ConfigurationService service{coordinator, graphStore,
                                               resolver};
    fermentation::LoadedConfigurationGraph initialGraph;

    Fixture() : initialGraph(seedGraph(store, resolver)) {
        TEST_ASSERT_TRUE(service.initialize(initialGraph));
    }

    explicit Fixture(const fermentation::ProgramCatalog& catalog)
        : initialGraph(seedGraphWithCatalog(store, resolver, catalog)) {
        TEST_ASSERT_TRUE(service.initialize(initialGraph));
    }
};

fermentation::ConfigurationPreviewView installChangedPreview(Fixture& fixture,
                                                             const char* name) {
    auto build = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(build.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
    TEST_ASSERT_TRUE(
        build.lease.replaceUserConfiguration({"de", "Europe/Zurich", name}));
    auto installed = fixture.service.installPreview(
        std::move(build.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(installed.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
    TEST_ASSERT_TRUE(installed.preview.has_value());
    return *installed.preview;
}

std::string repeatedUmlaut(std::size_t scalarCount) {
    std::string value;
    value.reserve(scalarCount * 2U);
    for (std::size_t index = 0U; index < scalarCount; ++index) {
        value += "\xC3\xA4";
    }
    return value;
}

void maximizeProgramPayload(fermentation::ProgramDocument& document) {
    auto& program = document.program;
    program.name = repeatedUmlaut(48U);
    program.notes = repeatedUmlaut(512U);
    program.preheat = true;
    program.sensorPreference = fermentation::SensorPreference::AirOnly;
    program.productSensorFailure.policy =
        fermentation::ProductSensorFailurePolicy::FallbackToAirAfterTimeout;
    program.productSensorFailure.fallbackDelaySeconds = 0U;
    program.fermentationStages.front().targetTemperatureCelsius = 20.0;
    program.fermentationStages.front().durationMinutes = 60U;
    program.targetQualification.bandCelsius = 0.5;
    program.targetQualification.durationMinutes = 10U;
    program.maximumTargetReachMinutes = 60U;
    program.maximumProductWaitMinutes = 60U;
    program.completion.mode =
        fermentation::CompletionMode::CoolAndHoldForDuration;
    program.completion.coolingTargetCelsius = 4.0;
    program.completion.holdDurationMinutes = 60U;
}

fermentation::ProgramCatalog maximumCountCatalog() {
    auto catalog = fermentation::makeFactoryProgramCatalog();
    for (auto& document : catalog.programs) {
        maximizeProgramPayload(document);
    }
    const auto prototype = catalog.programs.front();
    for (std::size_t index = 0U;
         index < fermentation::configuration_limits::kMaximumUserProgramCount;
         ++index) {
        auto program = prototype;
        const auto suffix = std::to_string(index);
        const std::size_t padding = 48U - 5U - suffix.size();
        program.program.id = "user-" + std::string(padding, 'a') + suffix;
        program.program.builtIn = false;
        program.program.factoryCatalogEntry = false;
        program.program.resettable = false;
        program.program.userDeletable = true;
        program.program.installed = true;
        catalog.programs.push_back(std::move(program));
    }
    TEST_ASSERT_TRUE(fermentation::validateProgramCatalog(catalog) ==
                     fermentation::ProgramCatalogStatus::Success);
    return catalog;
}

void test_initial_runtime_is_available_through_move_only_lease() {
    Fixture fixture;
    auto result = fixture.service.acquireRuntime();
    TEST_ASSERT_TRUE(
        result.status ==
        fermentation::RuntimeConfigurationReadStatus::RuntimeLeaseGranted);
    TEST_ASSERT_TRUE(result.lease.valid());
    TEST_ASSERT_EQUAL_STRING(
        "Fermentationsschrank",
        result.lease->userConfiguration().deviceName.c_str());
    TEST_ASSERT_EQUAL_UINT64(1U, result.lease->volatileGenerationId());
}

void test_runtime_reader_limit_is_enforced_and_released() {
    Fixture fixture;
    std::vector<fermentation::RuntimeConfigurationReadLease> leases;
    for (std::size_t index = 0U;
         index <
         fermentation::configuration_limits::kMaxRuntimeConfigurationReadLeases;
         ++index) {
        auto result = fixture.service.acquireRuntime();
        TEST_ASSERT_TRUE(
            result.status ==
            fermentation::RuntimeConfigurationReadStatus::RuntimeLeaseGranted);
        leases.push_back(std::move(result.lease));
    }
    auto ninth = fixture.service.acquireRuntime();
    TEST_ASSERT_TRUE(
        ninth.status ==
        fermentation::RuntimeConfigurationReadStatus::RuntimeReadLeaseBusy);
    leases.pop_back();
    auto available = fixture.service.acquireRuntime();
    TEST_ASSERT_TRUE(
        available.status ==
        fermentation::RuntimeConfigurationReadStatus::RuntimeLeaseGranted);
}

void test_changed_preview_owns_the_only_second_model_generation() {
    Fixture fixture;
    auto build = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(build.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
    TEST_ASSERT_TRUE(build.lease.replaceUserConfiguration(
        {"de", "Europe/Zurich", "Neuer Name"}));
    auto second = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(
        second.status ==
        fermentation::ConfigurationPreviewStatus::ConfigurationModelBudgetBusy);
    auto installed = fixture.service.installPreview(
        std::move(build.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(installed.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
    TEST_ASSERT_TRUE(installed.preview.has_value());
    TEST_ASSERT_FALSE(installed.preview->noChange);
    TEST_ASSERT_TRUE(installed.preview->changes.userConfiguration);
    TEST_ASSERT_EQUAL_UINT32(2U, fixture.service.fullModelGenerationCount());
    auto blocked = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(
        blocked.status ==
        fermentation::ConfigurationPreviewStatus::ConfigurationModelBudgetBusy);
    TEST_ASSERT_TRUE(fixture.service.cancelPreview(installed.preview->handle) ==
                     fermentation::ConfigurationPreviewStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.service.fullModelGenerationCount());
}

void test_no_change_preview_is_lightweight_and_identity_bound() {
    Fixture fixture;
    auto build = fixture.service.beginPreview();
    auto installed = fixture.service.installPreview(
        std::move(build.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(installed.preview->noChange);
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.service.fullModelGenerationCount());
    TEST_ASSERT_TRUE(
        fixture.service.cancelPreview(installed.preview->handle + 1U) ==
        fermentation::ConfigurationPreviewStatus::PreviewSuperseded);
    TEST_ASSERT_TRUE(fixture.service.visiblePreview().has_value());
    TEST_ASSERT_TRUE(fixture.service.cancelPreview(installed.preview->handle) ==
                     fermentation::ConfigurationPreviewStatus::Success);
    TEST_ASSERT_FALSE(fixture.service.visiblePreview().has_value());
}

void test_invalid_new_request_does_not_replace_visible_no_change_preview() {
    Fixture fixture;
    auto firstBuild = fixture.service.beginPreview();
    auto first = fixture.service.installPreview(
        std::move(firstBuild.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    auto invalidBuild = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(invalidBuild.lease.replaceUserConfiguration(
        {"xx", "Europe/Zurich", "Ungueltig"}));
    auto invalid = fixture.service.installPreview(
        std::move(invalidBuild.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(
        invalid.status ==
        fermentation::ConfigurationPreviewStatus::InvalidCandidate);
    TEST_ASSERT_EQUAL_UINT64(first.preview->handle,
                             fixture.service.visiblePreview()->handle);
}

void test_abandoned_build_lease_releases_model_budget() {
    Fixture fixture;
    {
        auto abandoned = fixture.service.beginPreview();
        TEST_ASSERT_TRUE(abandoned.lease.valid());
    }
    auto next = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(next.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
}

void test_confirmed_preview_commits_root_then_publishes_runtime() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Neuer Schrank");
    const auto committed = fixture.service.confirmPreview(preview.handle);
    TEST_ASSERT_TRUE(committed.status ==
                     fermentation::ConfigurationCommitStatus::Activated);
    TEST_ASSERT_TRUE(fixture.service.mode() ==
                     fermentation::ConfigurationServiceMode::Operational);
    TEST_ASSERT_FALSE(fixture.service.visiblePreview().has_value());
    auto runtime = fixture.service.acquireRuntime();
    TEST_ASSERT_TRUE(
        runtime.status ==
        fermentation::RuntimeConfigurationReadStatus::RuntimeLeaseGranted);
    TEST_ASSERT_EQUAL_STRING(
        "Neuer Schrank", runtime.lease->userConfiguration().deviceName.c_str());
    TEST_ASSERT_EQUAL_UINT64(
        2U, runtime.lease->manifestReference().version.value());
}

void test_pre_root_write_failure_keeps_old_runtime_and_no_partial_publish() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Nicht aktiv");
    fixture.store.failWrite(
        "uc1", device_platform::StateStoreWriteStatus::WriteError, false);
    const auto committed = fixture.service.confirmPreview(preview.handle);
    TEST_ASSERT_TRUE(
        committed.status ==
        fermentation::ConfigurationCommitStatus::PersistenceFailure);
    TEST_ASSERT_TRUE(fixture.service.mode() ==
                     fermentation::ConfigurationServiceMode::Operational);
    auto runtime = fixture.service.acquireRuntime();
    TEST_ASSERT_EQUAL_STRING(
        "Fermentationsschrank",
        runtime.lease->userConfiguration().deviceName.c_str());
}

void test_unknown_root_commit_with_verified_new_graph_activates() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Unbekannt aber neu");
    fixture.store.failWrite(
        "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        true);
    const auto committed = fixture.service.confirmPreview(preview.handle);
    TEST_ASSERT_TRUE(committed.status ==
                     fermentation::ConfigurationCommitStatus::Activated);
    auto runtime = fixture.service.acquireRuntime();
    TEST_ASSERT_EQUAL_STRING(
        "Unbekannt aber neu",
        runtime.lease->userConfiguration().deviceName.c_str());
}

void test_indeterminate_commit_blocks_runtime_until_exact_new_resolution() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Spaeter aufgeloest");
    fixture.store.failWrite(
        "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        true);
    fixture.store.failReadsAfterWrite(
        "cr1", {{"cr0", device_platform::StateStoreReadStatus::ReadError},
                {"cr1", device_platform::StateStoreReadStatus::ReadError}});
    const auto committed = fixture.service.confirmPreview(preview.handle);
    TEST_ASSERT_TRUE(committed.status ==
                     fermentation::ConfigurationCommitStatus::
                         ConfigurationCommitIndeterminate);
    TEST_ASSERT_TRUE(
        fixture.service.mode() ==
        fermentation::ConfigurationServiceMode::CommitIndeterminate);
    TEST_ASSERT_FALSE(fixture.service.visiblePreview().has_value());
    TEST_ASSERT_TRUE(fixture.service.acquireRuntime().status ==
                     fermentation::RuntimeConfigurationReadStatus::
                         ConfigurationRuntimeUnavailable);
    TEST_ASSERT_TRUE(fixture.service.beginPreview().status ==
                     fermentation::ConfigurationPreviewStatus::
                         ConfigurationRuntimeUnavailable);
    fixture.store.clearReadFault("cr0");
    fixture.store.clearReadFault("cr1");
    TEST_ASSERT_TRUE(fixture.service.resolveIndeterminate() ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRecoveredNew);
    auto runtime = fixture.service.acquireRuntime();
    TEST_ASSERT_EQUAL_STRING(
        "Spaeter aufgeloest",
        runtime.lease->userConfiguration().deviceName.c_str());
}

void test_post_commit_verification_failure_is_fail_closed_until_rescan() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Nachpruefung");
    fixture.store.failWrite(
        "cr1", device_platform::StateStoreWriteStatus::Success, true);
    fixture.store.failReadsAfterWrite(
        "cr1", {{"cr0", device_platform::StateStoreReadStatus::ReadError},
                {"cr1", device_platform::StateStoreReadStatus::ReadError}});
    const auto committed = fixture.service.confirmPreview(preview.handle);
    TEST_ASSERT_TRUE(
        committed.status ==
        fermentation::ConfigurationCommitStatus::ConfigurationRuntimeFailure);
    TEST_ASSERT_TRUE(fixture.service.mode() ==
                     fermentation::ConfigurationServiceMode::RuntimeFailure);
    TEST_ASSERT_TRUE(fixture.service.runtimeFailureCause() ==
                     fermentation::ConfigurationRuntimeFailureCause::
                         PostCommitVerificationFailure);
    TEST_ASSERT_TRUE(fixture.service.acquireRuntime().status ==
                     fermentation::RuntimeConfigurationReadStatus::
                         ConfigurationRuntimeUnavailable);
    fixture.store.clearReadFault("cr0");
    fixture.store.clearReadFault("cr1");
    TEST_ASSERT_TRUE(fixture.service.recoverRuntimeFailure() ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRecoveredNew);
    auto runtime = fixture.service.acquireRuntime();
    TEST_ASSERT_EQUAL_STRING(
        "Nachpruefung", runtime.lease->userConfiguration().deviceName.c_str());
}

void test_held_old_reader_blocks_third_model_before_any_write() {
    Fixture fixture;
    auto oldReader = fixture.service.acquireRuntime();
    const auto first = installChangedPreview(fixture, "Generation zwei");
    TEST_ASSERT_TRUE(fixture.service.confirmPreview(first.handle).status ==
                     fermentation::ConfigurationCommitStatus::Activated);
    TEST_ASSERT_EQUAL_STRING(
        "Fermentationsschrank",
        oldReader.lease->userConfiguration().deviceName.c_str());
    const auto writesBefore = fixture.store.writeCount();
    TEST_ASSERT_TRUE(
        fixture.service.beginPreview().status ==
        fermentation::ConfigurationPreviewStatus::ConfigurationModelBudgetBusy);
    TEST_ASSERT_EQUAL_UINT32(writesBefore, fixture.store.writeCount());
    oldReader.lease = {};
    TEST_ASSERT_TRUE(fixture.service.beginPreview().status ==
                     fermentation::ConfigurationPreviewStatus::Success);
}

void test_no_change_confirmation_removes_only_its_visible_handle() {
    Fixture fixture;
    auto build = fixture.service.beginPreview();
    auto installed = fixture.service.installPreview(
        std::move(build.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(installed.preview->noChange);
    TEST_ASSERT_TRUE(
        fixture.service.confirmPreview(installed.preview->handle).status ==
        fermentation::ConfigurationCommitStatus::NoChange);
    TEST_ASSERT_FALSE(fixture.service.visiblePreview().has_value());
    TEST_ASSERT_EQUAL_UINT32(0U, fixture.store.writeCount());
}

void test_maximum_count_catalog_keeps_two_model_limit_through_commit() {
    Fixture fixture;
    auto build = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(build.lease.replaceProgramCatalog(maximumCountCatalog()));
    TEST_ASSERT_TRUE(
        fixture.service.beginPreview().status ==
        fermentation::ConfigurationPreviewStatus::ConfigurationModelBudgetBusy);
    auto installed = fixture.service.installPreview(
        std::move(build.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_EQUAL_UINT32(2U, fixture.service.fullModelGenerationCount());
    TEST_ASSERT_TRUE(
        fixture.service.confirmPreview(installed.preview->handle).status ==
        fermentation::ConfigurationCommitStatus::Activated);
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.service.fullModelGenerationCount());
    auto runtime = fixture.service.acquireRuntime();
    TEST_ASSERT_EQUAL_UINT32(
        fermentation::configuration_limits::kMaximumProgramCount,
        runtime.lease->programCatalog().programs.size());
}

void test_maximum_valid_active_and_preview_never_create_third_model() {
    const auto maximumActive = maximumCountCatalog();
    Fixture fixture(maximumActive);
    auto build = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(build.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
    auto candidate = maximumCountCatalog();
    candidate.programs.back().program.notes.back() = 'x';
    TEST_ASSERT_TRUE(build.lease.replaceProgramCatalog(std::move(candidate)));
    TEST_ASSERT_EQUAL_UINT32(2U, fixture.service.fullModelGenerationCount());
    TEST_ASSERT_TRUE(
        fixture.service.beginPreview().status ==
        fermentation::ConfigurationPreviewStatus::ConfigurationModelBudgetBusy);
    TEST_ASSERT_EQUAL_UINT32(2U, fixture.service.fullModelGenerationCount());
}

void test_preview_cancel_cycles_return_to_stable_live_allocation() {
    Fixture fixture;
    const auto runCycle = [&fixture]() {
        auto build = fixture.service.beginPreview();
        TEST_ASSERT_TRUE(
            build.lease.replaceProgramCatalog(maximumCountCatalog()));
        auto installed = fixture.service.installPreview(
            std::move(build.lease), fermentation::decodeChangeOrigin(2U),
            fermentation::decodeChangeOperation(1U));
        TEST_ASSERT_TRUE(installed.preview.has_value());
        TEST_ASSERT_TRUE(
            fixture.service.cancelPreview(installed.preview->handle) ==
            fermentation::ConfigurationPreviewStatus::Success);
    };
    runCycle();
    const auto baseline = gLiveAllocBytes.load();
    gPeakAllocBytes.store(baseline);
    for (std::size_t index = 0U; index < 8U; ++index) {
        runCycle();
        TEST_ASSERT_EQUAL_UINT64(baseline, gLiveAllocBytes.load());
    }
    const auto peakDelta = gPeakAllocBytes.load() - baseline;
    TEST_ASSERT_TRUE(peakDelta >= 19916U);
    TEST_ASSERT_TRUE(peakDelta < 160000U);
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.service.fullModelGenerationCount());
}

void test_two_parallel_preview_starts_reserve_exactly_one_full_model() {
    Fixture fixture;
    std::atomic<bool> start{false};
    std::atomic<unsigned int> completed{0U};
    fermentation::ConfigurationPreviewStatus first{};
    fermentation::ConfigurationPreviewStatus second{};
    const auto begin = [&](fermentation::ConfigurationPreviewStatus& status) {
        while (!start.load(std::memory_order_acquire)) {
        }
        auto result = fixture.service.beginPreview();
        status = result.status;
        completed.fetch_add(1U, std::memory_order_release);
        if (result.lease.valid()) {
            while (start.load(std::memory_order_acquire)) {
            }
        }
    };
    std::thread firstThread(begin, std::ref(first));
    std::thread secondThread(begin, std::ref(second));
    start.store(true, std::memory_order_release);
    while (completed.load(std::memory_order_acquire) < 2U) {
    }
    start.store(false, std::memory_order_release);
    firstThread.join();
    secondThread.join();
    const auto successes =
        static_cast<unsigned int>(
            first == fermentation::ConfigurationPreviewStatus::Success) +
        static_cast<unsigned int>(
            second == fermentation::ConfigurationPreviewStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(1U, successes);
    TEST_ASSERT_TRUE(first ==
                         fermentation::ConfigurationPreviewStatus::Success ||
                     first == fermentation::ConfigurationPreviewStatus::
                                  ConfigurationModelBudgetBusy);
    TEST_ASSERT_TRUE(second ==
                         fermentation::ConfigurationPreviewStatus::Success ||
                     second == fermentation::ConfigurationPreviewStatus::
                                   ConfigurationModelBudgetBusy);
}

void test_superseded_no_change_handle_cannot_remove_newer_preview() {
    Fixture fixture;
    auto noChangeBuild = fixture.service.beginPreview();
    auto noChange = fixture.service.installPreview(
        std::move(noChangeBuild.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    auto changedBuild = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(changedBuild.lease.replaceUserConfiguration(
        {"de", "Europe/Zurich", "Neuere Vorschau"}));
    auto changed = fixture.service.installPreview(
        std::move(changedBuild.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(
        fixture.service.cancelPreview(noChange.preview->handle) ==
        fermentation::ConfigurationPreviewStatus::PreviewSuperseded);
    TEST_ASSERT_EQUAL_UINT64(changed.preview->handle,
                             fixture.service.visiblePreview()->handle);
}

void test_indeterminate_commit_can_resolve_exactly_old() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Bleibt alt");
    fixture.store.failWrite(
        "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        false);
    fixture.store.failReadsAfterWrite(
        "cr1", {{"cr0", device_platform::StateStoreReadStatus::ReadError},
                {"cr1", device_platform::StateStoreReadStatus::ReadError}});
    TEST_ASSERT_TRUE(fixture.service.confirmPreview(preview.handle).status ==
                     fermentation::ConfigurationCommitStatus::
                         ConfigurationCommitIndeterminate);
    fixture.store.clearReadFault("cr0");
    fixture.store.clearReadFault("cr1");
    TEST_ASSERT_TRUE(fixture.service.resolveIndeterminate() ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRecoveredOld);
    auto runtime = fixture.service.acquireRuntime();
    TEST_ASSERT_EQUAL_STRING(
        "Fermentationsschrank",
        runtime.lease->userConfiguration().deviceName.c_str());
    TEST_ASSERT_FALSE(fixture.service.visiblePreview().has_value());
}

void test_runtime_failure_does_not_recover_to_old_graph_in_process() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Commit war neu");
    fixture.store.failWrite(
        "cr1", device_platform::StateStoreWriteStatus::Success, true);
    fixture.store.failReadsAfterWrite(
        "cr1", {{"cr0", device_platform::StateStoreReadStatus::ReadError},
                {"cr1", device_platform::StateStoreReadStatus::ReadError}});
    TEST_ASSERT_TRUE(
        fixture.service.confirmPreview(preview.handle).status ==
        fermentation::ConfigurationCommitStatus::ConfigurationRuntimeFailure);
    fixture.store.clearReadFault("cr0");
    fixture.store.clearReadFault("cr1");
    const auto oldRoot = fixture.store.read(
        *device_platform::StateStoreKey::create("cr0").key, 114U);
    fixture.store.put("cr1", oldRoot.value);
    TEST_ASSERT_TRUE(fixture.service.recoverRuntimeFailure() ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRuntimeFailure);
    TEST_ASSERT_TRUE(fixture.service.mode() ==
                     fermentation::ConfigurationServiceMode::RuntimeFailure);
    TEST_ASSERT_TRUE(fixture.service.acquireRuntime().status ==
                     fermentation::RuntimeConfigurationReadStatus::
                         ConfigurationRuntimeUnavailable);
}

void test_two_concurrent_confirmations_have_exactly_one_winner() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Ein Gewinner");
    std::atomic<bool> start{false};
    fermentation::ConfigurationCommitStatus first{};
    fermentation::ConfigurationCommitStatus second{};
    auto confirm = [&](fermentation::ConfigurationCommitStatus& status) {
        while (!start.load(std::memory_order_acquire)) {
        }
        status = fixture.service.confirmPreview(preview.handle).status;
    };
    std::thread firstThread(confirm, std::ref(first));
    std::thread secondThread(confirm, std::ref(second));
    start.store(true, std::memory_order_release);
    firstThread.join();
    secondThread.join();
    const auto activated =
        static_cast<unsigned int>(
            first == fermentation::ConfigurationCommitStatus::Activated) +
        static_cast<unsigned int>(
            second == fermentation::ConfigurationCommitStatus::Activated);
    TEST_ASSERT_EQUAL_UINT32(1U, activated);
    TEST_ASSERT_TRUE(
        first == fermentation::ConfigurationCommitStatus::Activated ||
        first == fermentation::ConfigurationCommitStatus::
                     ConfigurationMutationBusy ||
        first == fermentation::ConfigurationCommitStatus::PreviewNotFound);
    TEST_ASSERT_TRUE(
        second == fermentation::ConfigurationCommitStatus::Activated ||
        second == fermentation::ConfigurationCommitStatus::
                      ConfigurationMutationBusy ||
        second == fermentation::ConfigurationCommitStatus::PreviewNotFound);
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_initial_runtime_is_available_through_move_only_lease);
    RUN_TEST(test_runtime_reader_limit_is_enforced_and_released);
    RUN_TEST(test_changed_preview_owns_the_only_second_model_generation);
    RUN_TEST(test_no_change_preview_is_lightweight_and_identity_bound);
    RUN_TEST(
        test_invalid_new_request_does_not_replace_visible_no_change_preview);
    RUN_TEST(test_abandoned_build_lease_releases_model_budget);
    RUN_TEST(test_confirmed_preview_commits_root_then_publishes_runtime);
    RUN_TEST(
        test_pre_root_write_failure_keeps_old_runtime_and_no_partial_publish);
    RUN_TEST(test_unknown_root_commit_with_verified_new_graph_activates);
    RUN_TEST(
        test_indeterminate_commit_blocks_runtime_until_exact_new_resolution);
    RUN_TEST(test_post_commit_verification_failure_is_fail_closed_until_rescan);
    RUN_TEST(test_held_old_reader_blocks_third_model_before_any_write);
    RUN_TEST(test_no_change_confirmation_removes_only_its_visible_handle);
    RUN_TEST(test_maximum_count_catalog_keeps_two_model_limit_through_commit);
    RUN_TEST(test_maximum_valid_active_and_preview_never_create_third_model);
    RUN_TEST(test_preview_cancel_cycles_return_to_stable_live_allocation);
    RUN_TEST(test_two_parallel_preview_starts_reserve_exactly_one_full_model);
    RUN_TEST(test_superseded_no_change_handle_cannot_remove_newer_preview);
    RUN_TEST(test_indeterminate_commit_can_resolve_exactly_old);
    RUN_TEST(test_runtime_failure_does_not_recover_to_old_graph_in_process);
    RUN_TEST(test_two_concurrent_confirmations_have_exactly_one_winner);
    return UNITY_END();
}
