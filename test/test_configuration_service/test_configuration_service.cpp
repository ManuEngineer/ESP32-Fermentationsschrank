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

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

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

struct ConfigurationServiceHookControl {
    std::atomic<int> desiredPoint{-1};
    std::atomic<bool> reached{false};
    std::atomic<bool> released{false};
};

struct RuntimePreparationObservation {
    std::atomic<std::size_t> attemptCount{0U};
};

namespace fermentation {

class ConfigurationServiceTestAccess {
   public:
    static bool initialize(ConfigurationService& service,
                           const LoadedConfigurationGraph& graph) {
        return service.initializeForTest(graph);
    }
    static void installHook(ConfigurationService& service,
                            ConfigurationServiceHookControl& control,
                            int desiredPoint) {
        control.desiredPoint.store(desiredPoint, std::memory_order_release);
        control.reached.store(false, std::memory_order_release);
        control.released.store(false, std::memory_order_release);
        service.testHookContext_ = &control;
        service.testHook_ = &hook;
    }

    static void clearHook(ConfigurationService& service) {
        service.testHook_ = nullptr;
        service.testHookContext_ = nullptr;
    }

    static void observeRuntimePreparation(
        ConfigurationService& service,
        RuntimePreparationObservation& observation) {
        observation.attemptCount.store(0U, std::memory_order_release);
        service.testHookContext_ = &observation;
        service.testHook_ = &runtimePreparationHook;
    }

    static void setStateRevision(ConfigurationService& service,
                                 std::uint64_t revision) {
        const std::lock_guard<std::mutex> lock(service.stateMutex_);
        service.stateRevision_ = revision;
    }

    static void forceRuntimeFailure(ConfigurationService& service) {
        const std::lock_guard<std::mutex> lock(service.stateMutex_);
        service.enterFailClosedLocked(
            ConfigurationServiceMode::RuntimeFailure,
            ConfigurationRuntimeFailureCause::ServiceStateInvariantViolation);
    }

    static void restoreOperationalForReservationTest(
        ConfigurationService& service) {
        const std::lock_guard<std::mutex> lock(service.stateMutex_);
        service.mode_ = ConfigurationServiceMode::Operational;
        service.runtimeFailureCause_.reset();
        TEST_ASSERT_TRUE(service.incrementStateRevisionLocked());
    }

    static std::uint64_t previewBuildReservation(
        ConfigurationService& service) {
        const std::lock_guard<std::mutex> lock(service.stateMutex_);
        return service.previewBuildReservation_.value_or(0U);
    }

    static void releasePreviewBuild(ConfigurationService& service,
                                    std::uint64_t reservationId) {
        service.releasePreviewBuild(reservationId);
    }

    static void invalidateRuntimeBinding(ConfigurationService& service) {
        service.invalidateRuntimePreparationBindingForTest();
    }

    static void rejectRuntimePreparation(ConfigurationService& service,
                                         bool reject) {
        service.rejectRuntimePreparationForTest(reject);
    }

    static bool runtimePreparationRetryConsumed(ConfigurationService& service) {
        return service.runtimePreparationRetryConsumedForTest();
    }

    static void changeCurrentGraphActiveBasis(ConfigurationService& service) {
        const std::lock_guard<std::mutex> lock(service.stateMutex_);
        TEST_ASSERT_NOT_NULL(service.currentGraph_.get());
        const auto generation =
            service.currentGraph_->active.manifestReference.version.value();
        service.currentGraph_->active.manifestReference.version =
            ConfigurationManifestGeneration{generation + 1U};
    }

    static bool completeRuntimeRetirement(ConfigurationService& service,
                                          std::uint64_t generationId) {
        return service.completeRuntimeRetirement(generationId);
    }

   private:
    static void runtimePreparationHook(void* context,
                                       ConfigurationService::TestPoint point) {
        if (point !=
            ConfigurationService::TestPoint::BeforeRuntimePreparation) {
            return;
        }
        auto& observation =
            *static_cast<RuntimePreparationObservation*>(context);
        observation.attemptCount.fetch_add(1U, std::memory_order_relaxed);
    }

    static void hook(void* context, ConfigurationService::TestPoint point) {
        auto& control = *static_cast<ConfigurationServiceHookControl*>(context);
        if (static_cast<int>(point) !=
            control.desiredPoint.load(std::memory_order_acquire)) {
            return;
        }
        control.reached.store(true, std::memory_order_release);
        while (!control.released.load(std::memory_order_acquire)) {
        }
    }
};

}  // namespace fermentation

namespace {

class LocalStore final : public device_platform::IStateStore {
   public:
    using ReadObserver = void (*)(void*);

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
        if (readObserver_ != nullptr) {
            readObserver_(readObserverContext_);
        }
        readCount_.fetch_add(1U, std::memory_order_relaxed);
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
        TEST_ASSERT_FALSE(
            commitValue &&
            (status == device_platform::StateStoreWriteStatus::WriteError ||
             status == device_platform::StateStoreWriteStatus::CapacityError));
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

    void observeReads(ReadObserver observer, void* context) {
        readObserver_ = observer;
        readObserverContext_ = context;
    }

    void clearReadObserver() {
        readObserver_ = nullptr;
        readObserverContext_ = nullptr;
    }

    [[nodiscard]] std::size_t writeCount() const { return writeCount_; }
    [[nodiscard]] std::size_t readCount() const {
        return readCount_.load(std::memory_order_relaxed);
    }

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
    mutable std::atomic<std::size_t> readCount_{0U};
    mutable ReadObserver readObserver_{nullptr};
    mutable void* readObserverContext_{nullptr};
};

struct RetryReadObservation {
    fermentation::ConfigurationService* service{nullptr};
    std::size_t readCount{0U};
};

void observeConsumedRetryAtStoreRead(void* context) {
    auto& observation = *static_cast<RetryReadObservation*>(context);
    TEST_ASSERT_NOT_NULL(observation.service);
    TEST_ASSERT_TRUE(fermentation::ConfigurationServiceTestAccess::
                         runtimePreparationRetryConsumed(*observation.service));
    ++observation.readCount;
}

class Resolver final : public device_platform::ITimeZoneResolver {
   public:
    device_platform::TimeZonePrepareResult prepare(
        const std::string& identifier) const override {
        prepareCount_.fetch_add(1U, std::memory_order_relaxed);
        if (identifier != "Europe/Zurich") {
            return {
                device_platform::TimeZonePrepareStatus::UnsupportedIdentifier,
                std::nullopt};
        }
        return {device_platform::TimeZonePrepareStatus::Success,
                device_platform::PreparedTimeZone{identifier}};
    }

    [[nodiscard]] std::size_t prepareCount() const {
        return prepareCount_.load(std::memory_order_relaxed);
    }

   private:
    mutable std::atomic<std::size_t> prepareCount_{0U};
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
        TEST_ASSERT_TRUE(
            fermentation::ConfigurationServiceTestAccess::initialize(
                service, initialGraph));
    }

    explicit Fixture(const fermentation::ProgramCatalog& catalog)
        : initialGraph(seedGraphWithCatalog(store, resolver, catalog)) {
        TEST_ASSERT_TRUE(
            fermentation::ConfigurationServiceTestAccess::initialize(
                service, initialGraph));
    }
};

fermentation::ConfigurationPreviewView installChangedPreview(Fixture& fixture,
                                                             const char* name) {
    auto build = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(build.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
    build.lease.userConfiguration() = {"de", "Europe/Zurich", name};
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
    // AirOnly ist nach der 6.13-Cross-Field-Regel nur mit einer festen,
    // schlankeren Kombination gueltig (kein fallback_delay_s). Fuer die
    // maximale Katalog-Payload wird stattdessen eine Praeferenz gewaehlt,
    // die fallback_delay_s UND jede ReturnStrategy zulaesst.
    program.sensorPreference =
        fermentation::SensorPreference::ProductIfAvailableElseAir;
    program.productSensorFailure.policy =
        fermentation::ProductSensorFailurePolicy::FallbackToAirAfterTimeout;
    program.productSensorFailure.fallbackDelaySeconds = 0U;
    program.productSensorFailure.returnStrategy =
        fermentation::ReturnStrategy::AutomaticValidatedReturnToProduct;
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
    build.lease.userConfiguration() = {"de", "Europe/Zurich", "Neuer Name"};
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
    invalidBuild.lease.userConfiguration() = {"xx", "Europe/Zurich",
                                              "Ungueltig"};
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
    TEST_ASSERT_TRUE(
        fixture.service.commitIndeterminateCause() ==
        fermentation::ConfigurationCommitIndeterminateCause::RootReadError);
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
    TEST_ASSERT_FALSE(fixture.service.commitIndeterminateCause().has_value());
    auto runtime = fixture.service.acquireRuntime();
    TEST_ASSERT_EQUAL_STRING(
        "Spaeter aufgeloest",
        runtime.lease->userConfiguration().deviceName.c_str());
}

void test_indeterminate_commit_preserves_graph_read_and_capacity_causes() {
    const std::pair<device_platform::StateStoreReadStatus,
                    fermentation::ConfigurationCommitIndeterminateCause>
        cases[]{
            {device_platform::StateStoreReadStatus::ReadError,
             fermentation::ConfigurationCommitIndeterminateCause::
                 GraphReadError},
            {device_platform::StateStoreReadStatus::CapacityError,
             fermentation::ConfigurationCommitIndeterminateCause::
                 GraphCapacityError},
        };
    for (const auto& [storeStatus, expectedCause] : cases) {
        Fixture fixture;
        const auto preview = installChangedPreview(fixture, "Graph unklar");
        fixture.store.failWrite(
            "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
            true);
        fixture.store.failReadsAfterWrite("cr1", {{"uc1", storeStatus}});
        const auto committed = fixture.service.confirmPreview(preview.handle);
        TEST_ASSERT_TRUE(committed.status ==
                         fermentation::ConfigurationCommitStatus::
                             ConfigurationCommitIndeterminate);
        TEST_ASSERT_TRUE(fixture.service.commitIndeterminateCause() ==
                         expectedCause);
        fixture.store.clearReadFault("uc1");
        TEST_ASSERT_TRUE(fixture.service.resolveIndeterminate() ==
                         fermentation::ConfigurationCommitResolutionStatus::
                             ResolutionRecoveredNew);
    }
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

void test_stale_no_change_revision_conflicts_without_write_or_counter_use() {
    Fixture fixture;
    auto build = fixture.service.beginPreview();
    auto installed = fixture.service.installPreview(
        std::move(build.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(installed.preview->noChange);
    fermentation::ConfigurationServiceTestAccess::setStateRevision(
        fixture.service, fixture.service.stateRevision() + 1U);
    const auto revisionBefore = fixture.service.stateRevision();
    const auto writesBefore = fixture.store.writeCount();
    TEST_ASSERT_TRUE(
        fixture.service.confirmPreview(installed.preview->handle).status ==
        fermentation::ConfigurationCommitStatus::ConfigurationConflictFailure);
    TEST_ASSERT_EQUAL_UINT64(revisionBefore, fixture.service.stateRevision());
    TEST_ASSERT_EQUAL_UINT64(writesBefore, fixture.store.writeCount());
    TEST_ASSERT_FALSE(fixture.service.visiblePreview().has_value());
}

void test_stale_no_change_active_basis_conflicts_without_store_access() {
    Fixture fixture;
    auto build = fixture.service.beginPreview();
    auto installed = fixture.service.installPreview(
        std::move(build.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(installed.preview->noChange);
    fermentation::ConfigurationServiceTestAccess::changeCurrentGraphActiveBasis(
        fixture.service);
    const auto writesBefore = fixture.store.writeCount();
    TEST_ASSERT_TRUE(
        fixture.service.confirmPreview(installed.preview->handle).status ==
        fermentation::ConfigurationCommitStatus::ConfigurationConflictFailure);
    TEST_ASSERT_EQUAL_UINT64(writesBefore, fixture.store.writeCount());
    TEST_ASSERT_FALSE(fixture.service.visiblePreview().has_value());
}

void test_maximum_count_catalog_keeps_two_model_limit_through_commit() {
    Fixture fixture;
    auto build = fixture.service.beginPreview();
    build.lease.programCatalog() = maximumCountCatalog();
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
    build.lease.programCatalog() = std::move(candidate);
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
        build.lease.programCatalog() = maximumCountCatalog();
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
    changedBuild.lease.userConfiguration() = {"de", "Europe/Zurich",
                                              "Neuere Vorschau"};
    auto changed = fixture.service.installPreview(
        std::move(changedBuild.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    const auto writesBefore = fixture.store.writeCount();
    TEST_ASSERT_TRUE(
        fixture.service.confirmPreview(noChange.preview->handle).status ==
        fermentation::ConfigurationCommitStatus::PreviewSuperseded);
    TEST_ASSERT_EQUAL_UINT64(writesBefore, fixture.store.writeCount());
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

void test_captured_preview_keeps_model_reservation_until_commit_finishes() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Reserviert A");
    ConfigurationServiceHookControl control;
    fermentation::ConfigurationServiceTestAccess::installHook(fixture.service,
                                                              control, 0);
    fermentation::ConfigurationCommitStatus commitStatus{};
    std::thread commit([&] {
        commitStatus = fixture.service.confirmPreview(preview.handle).status;
    });
    while (!control.reached.load(std::memory_order_acquire)) {
    }
    TEST_ASSERT_TRUE(fixture.service.cancelPreview(preview.handle) ==
                     fermentation::ConfigurationPreviewStatus::PreviewNotFound);
    TEST_ASSERT_TRUE(
        fixture.service.beginPreview().status ==
        fermentation::ConfigurationPreviewStatus::ConfigurationModelBudgetBusy);
    TEST_ASSERT_EQUAL_UINT32(2U, fixture.service.fullModelGenerationCount());
    control.released.store(true, std::memory_order_release);
    commit.join();
    fermentation::ConfigurationServiceTestAccess::clearHook(fixture.service);
    TEST_ASSERT_TRUE(commitStatus ==
                     fermentation::ConfigurationCommitStatus::Activated);
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.service.fullModelGenerationCount());
}

void test_all_old_runtime_leases_hold_retired_generation_until_last_release() {
    Fixture fixture;
    std::vector<fermentation::RuntimeConfigurationReadLease> leases;
    leases.reserve(
        fermentation::configuration_limits::kMaxRuntimeConfigurationReadLeases);
    for (std::size_t index = 0U;
         index <
         fermentation::configuration_limits::kMaxRuntimeConfigurationReadLeases;
         ++index) {
        auto read = fixture.service.acquireRuntime();
        TEST_ASSERT_TRUE(
            read.status ==
            fermentation::RuntimeConfigurationReadStatus::RuntimeLeaseGranted);
        leases.push_back(std::move(read.lease));
    }
    const auto preview = installChangedPreview(fixture, "Neue Generation");
    TEST_ASSERT_TRUE(fixture.service.confirmPreview(preview.handle).status ==
                     fermentation::ConfigurationCommitStatus::Activated);
    TEST_ASSERT_EQUAL_UINT32(2U, fixture.service.fullModelGenerationCount());
    for (std::size_t index = 0U; index + 1U < leases.size(); ++index) {
        leases[index] = {};
        TEST_ASSERT_EQUAL_UINT32(2U,
                                 fixture.service.fullModelGenerationCount());
        TEST_ASSERT_TRUE(fixture.service.beginPreview().status ==
                         fermentation::ConfigurationPreviewStatus::
                             ConfigurationModelBudgetBusy);
    }
    leases.back() = {};
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.service.fullModelGenerationCount());
    auto available = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(available.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
}

void test_publish_retirement_keeps_service_closed_until_local_owners_die() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Retirement");
    ConfigurationServiceHookControl control;
    fermentation::ConfigurationServiceTestAccess::installHook(fixture.service,
                                                              control, 1);
    fermentation::ConfigurationCommitStatus commitStatus{};
    std::thread commit([&] {
        commitStatus = fixture.service.confirmPreview(preview.handle).status;
    });
    while (!control.reached.load(std::memory_order_acquire)) {
    }
    TEST_ASSERT_TRUE(fixture.service.mode() ==
                     fermentation::ConfigurationServiceMode::CommitInProgress);
    TEST_ASSERT_EQUAL_UINT32(2U, fixture.service.fullModelGenerationCount());
    TEST_ASSERT_TRUE(fixture.service.beginPreview().status ==
                     fermentation::ConfigurationPreviewStatus::
                         ConfigurationRuntimeUnavailable);
    control.released.store(true, std::memory_order_release);
    commit.join();
    fermentation::ConfigurationServiceTestAccess::clearHook(fixture.service);
    TEST_ASSERT_TRUE(commitStatus ==
                     fermentation::ConfigurationCommitStatus::Activated);
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.service.fullModelGenerationCount());
}

void test_revoked_preview_build_reservation_is_released_by_identity_only() {
    Fixture fixture;
    auto oldBuild = fixture.service.beginPreview();
    const auto oldId =
        fermentation::ConfigurationServiceTestAccess::previewBuildReservation(
            fixture.service);
    TEST_ASSERT_NOT_EQUAL(0U, oldId);
    fermentation::ConfigurationServiceTestAccess::forceRuntimeFailure(
        fixture.service);
    fermentation::ConfigurationServiceTestAccess::
        restoreOperationalForReservationTest(fixture.service);
    TEST_ASSERT_TRUE(
        fixture.service.beginPreview().status ==
        fermentation::ConfigurationPreviewStatus::ConfigurationModelBudgetBusy);
    oldBuild.lease = {};
    auto newBuild = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(newBuild.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
    fermentation::ConfigurationServiceTestAccess::releasePreviewBuild(
        fixture.service, oldId);
    TEST_ASSERT_TRUE(
        fixture.service.beginPreview().status ==
        fermentation::ConfigurationPreviewStatus::ConfigurationModelBudgetBusy);
}

void test_preview_finishing_after_fail_closed_cannot_install_or_lose_budget() {
    Fixture fixture;
    auto build = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(build.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
    build.lease.userConfiguration().deviceName = "Spaete Vorschau";
    ConfigurationServiceHookControl control;
    fermentation::ConfigurationServiceTestAccess::installHook(fixture.service,
                                                              control, 3);
    fermentation::ConfigurationPreviewStatus installStatus{};
    std::thread installer([&] {
        installStatus =
            fixture.service
                .installPreview(std::move(build.lease),
                                fermentation::decodeChangeOrigin(2U),
                                fermentation::decodeChangeOperation(1U))
                .status;
    });
    while (!control.reached.load(std::memory_order_acquire)) {
    }
    fermentation::ConfigurationServiceTestAccess::forceRuntimeFailure(
        fixture.service);
    TEST_ASSERT_FALSE(fixture.service.visiblePreview().has_value());
    control.released.store(true, std::memory_order_release);
    installer.join();
    fermentation::ConfigurationServiceTestAccess::clearHook(fixture.service);
    TEST_ASSERT_TRUE(installStatus ==
                     fermentation::ConfigurationPreviewStatus::StateChanged);
    TEST_ASSERT_FALSE(fixture.service.visiblePreview().has_value());
    TEST_ASSERT_EQUAL_UINT32(2U, fixture.service.fullModelGenerationCount());
    build.lease = {};
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.service.fullModelGenerationCount());

    fermentation::ConfigurationServiceTestAccess::
        restoreOperationalForReservationTest(fixture.service);
    auto next = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(next.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
}

void test_invalid_runtime_binding_is_rebuilt_before_recovered_new_publish() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Neu gebunden");
    fixture.store.failWrite(
        "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        true);
    fixture.store.failReadsAfterWrite(
        "cr1", {{"cr1", device_platform::StateStoreReadStatus::ReadError}});
    TEST_ASSERT_TRUE(fixture.service.confirmPreview(preview.handle).status ==
                     fermentation::ConfigurationCommitStatus::
                         ConfigurationCommitIndeterminate);
    fixture.store.clearReadFault("cr1");
    fermentation::ConfigurationServiceTestAccess::invalidateRuntimeBinding(
        fixture.service);
    TEST_ASSERT_TRUE(fixture.service.resolveIndeterminate() ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRecoveredNew);
    auto runtime = fixture.service.acquireRuntime();
    TEST_ASSERT_EQUAL_STRING(
        "Neu gebunden", runtime.lease->userConfiguration().deviceName.c_str());
}

void test_failed_runtime_rebuild_stays_closed_and_can_retry_allowed_cause() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Spaeter vorbereitet");
    fixture.store.failWrite(
        "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        true);
    fixture.store.failReadsAfterWrite(
        "cr1", {{"cr1", device_platform::StateStoreReadStatus::ReadError}});
    TEST_ASSERT_TRUE(fixture.service.confirmPreview(preview.handle).status ==
                     fermentation::ConfigurationCommitStatus::
                         ConfigurationCommitIndeterminate);
    fixture.store.clearReadFault("cr1");
    fermentation::ConfigurationServiceTestAccess::invalidateRuntimeBinding(
        fixture.service);
    fermentation::ConfigurationServiceTestAccess::rejectRuntimePreparation(
        fixture.service, true);
    TEST_ASSERT_TRUE(fixture.service.resolveIndeterminate() ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRuntimeFailure);
    TEST_ASSERT_TRUE(fixture.service.runtimeFailureCause().has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRuntimeFailureCause::
                             RuntimePreparationAfterResolutionFailure),
        static_cast<int>(*fixture.service.runtimeFailureCause()));
    RetryReadObservation observation{&fixture.service};
    fixture.store.observeReads(observeConsumedRetryAtStoreRead, &observation);
    RuntimePreparationObservation preparation;
    fermentation::ConfigurationServiceTestAccess::observeRuntimePreparation(
        fixture.service, preparation);
    const auto readsBeforeRetry = fixture.store.readCount();
    fermentation::ConfigurationServiceTestAccess::rejectRuntimePreparation(
        fixture.service, false);
    TEST_ASSERT_TRUE(fixture.service.recoverRuntimeFailure() ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRecoveredNew);
    fixture.store.clearReadObserver();
    fermentation::ConfigurationServiceTestAccess::clearHook(fixture.service);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, observation.readCount);
    TEST_ASSERT_GREATER_THAN_UINT64(readsBeforeRetry,
                                    fixture.store.readCount());
    TEST_ASSERT_EQUAL_UINT64(1U, preparation.attemptCount.load());
    auto runtime = fixture.service.acquireRuntime();
    TEST_ASSERT_TRUE(
        runtime.status ==
        fermentation::RuntimeConfigurationReadStatus::RuntimeLeaseGranted);
    TEST_ASSERT_EQUAL_STRING(
        "Spaeter vorbereitet",
        runtime.lease->userConfiguration().deviceName.c_str());
}

void test_failed_runtime_rebuild_retry_is_consumed_before_preparation() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Retry verbraucht");
    fixture.store.failWrite(
        "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        true);
    fixture.store.failReadsAfterWrite(
        "cr1", {{"cr1", device_platform::StateStoreReadStatus::ReadError}});
    TEST_ASSERT_TRUE(fixture.service.confirmPreview(preview.handle).status ==
                     fermentation::ConfigurationCommitStatus::
                         ConfigurationCommitIndeterminate);
    fixture.store.clearReadFault("cr1");
    fermentation::ConfigurationServiceTestAccess::invalidateRuntimeBinding(
        fixture.service);
    fermentation::ConfigurationServiceTestAccess::rejectRuntimePreparation(
        fixture.service, true);
    TEST_ASSERT_TRUE(fixture.service.resolveIndeterminate() ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRuntimeFailure);

    RetryReadObservation observation{&fixture.service};
    fixture.store.observeReads(observeConsumedRetryAtStoreRead, &observation);
    RuntimePreparationObservation preparation;
    fermentation::ConfigurationServiceTestAccess::observeRuntimePreparation(
        fixture.service, preparation);
    const auto readsBeforeRetry = fixture.store.readCount();
    TEST_ASSERT_TRUE(fixture.service.recoverRuntimeFailure() ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRuntimeFailure);
    fixture.store.clearReadObserver();
    TEST_ASSERT_GREATER_THAN_UINT32(0U, observation.readCount);
    TEST_ASSERT_GREATER_THAN_UINT64(readsBeforeRetry,
                                    fixture.store.readCount());
    TEST_ASSERT_EQUAL_UINT64(1U, preparation.attemptCount.load());
    const auto readsAfterConsumedRetry = fixture.store.readCount();
    fermentation::ConfigurationServiceTestAccess::rejectRuntimePreparation(
        fixture.service, false);
    const auto writesAfterConsumedRetry = fixture.store.writeCount();
    const auto liveAllocationsAfterConsumedRetry = gLiveAllocBytes.load();
    const auto peakAllocationsAfterConsumedRetry = gPeakAllocBytes.load();
    for (std::size_t attempt = 0U; attempt < 3U; ++attempt) {
        TEST_ASSERT_TRUE(fixture.service.recoverRuntimeFailure() ==
                         fermentation::ConfigurationCommitResolutionStatus::
                             ResolutionRuntimeFailure);
    }
    TEST_ASSERT_EQUAL_UINT64(readsAfterConsumedRetry,
                             fixture.store.readCount());
    TEST_ASSERT_EQUAL_UINT64(1U, preparation.attemptCount.load());
    TEST_ASSERT_EQUAL_UINT64(writesAfterConsumedRetry,
                             fixture.store.writeCount());
    TEST_ASSERT_EQUAL_UINT64(liveAllocationsAfterConsumedRetry,
                             gLiveAllocBytes.load());
    TEST_ASSERT_EQUAL_UINT64(peakAllocationsAfterConsumedRetry,
                             gPeakAllocBytes.load());
    TEST_ASSERT_TRUE(fixture.service.mode() ==
                     fermentation::ConfigurationServiceMode::RuntimeFailure);
    TEST_ASSERT_TRUE(fixture.service.runtimeFailureCause() ==
                     fermentation::ConfigurationRuntimeFailureCause::
                         RuntimePreparationAfterResolutionFailure);
    TEST_ASSERT_TRUE(fixture.service.acquireRuntime().status ==
                     fermentation::RuntimeConfigurationReadStatus::
                         ConfigurationRuntimeUnavailable);
    TEST_ASSERT_TRUE(fixture.service.beginPreview().status ==
                     fermentation::ConfigurationPreviewStatus::
                         ConfigurationRuntimeUnavailable);
    TEST_ASSERT_TRUE(
        fixture.service.confirmPreview(preview.handle).status ==
        fermentation::ConfigurationCommitStatus::ConfigurationRuntimeFailure);
    fermentation::ConfigurationServiceTestAccess::clearHook(fixture.service);
}

void test_failed_runtime_rebuild_scan_consumes_retry_before_first_read() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Scan scheitert");
    fixture.store.failWrite(
        "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        true);
    fixture.store.failReadsAfterWrite(
        "cr1", {{"cr1", device_platform::StateStoreReadStatus::ReadError}});
    TEST_ASSERT_TRUE(fixture.service.confirmPreview(preview.handle).status ==
                     fermentation::ConfigurationCommitStatus::
                         ConfigurationCommitIndeterminate);
    fixture.store.clearReadFault("cr1");
    fermentation::ConfigurationServiceTestAccess::invalidateRuntimeBinding(
        fixture.service);
    fermentation::ConfigurationServiceTestAccess::rejectRuntimePreparation(
        fixture.service, true);
    TEST_ASSERT_TRUE(fixture.service.resolveIndeterminate() ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRuntimeFailure);

    fermentation::ConfigurationServiceTestAccess::rejectRuntimePreparation(
        fixture.service, false);
    fixture.store.failRead("cr1",
                           device_platform::StateStoreReadStatus::ReadError);
    RetryReadObservation observation{&fixture.service};
    fixture.store.observeReads(observeConsumedRetryAtStoreRead, &observation);
    RuntimePreparationObservation preparation;
    fermentation::ConfigurationServiceTestAccess::observeRuntimePreparation(
        fixture.service, preparation);
    const auto readsBeforeRetry = fixture.store.readCount();
    TEST_ASSERT_TRUE(fixture.service.recoverRuntimeFailure() ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRuntimeFailure);
    fixture.store.clearReadObserver();
    fixture.store.clearReadFault("cr1");
    TEST_ASSERT_GREATER_THAN_UINT32(0U, observation.readCount);
    TEST_ASSERT_GREATER_THAN_UINT64(readsBeforeRetry,
                                    fixture.store.readCount());
    TEST_ASSERT_EQUAL_UINT64(0U, preparation.attemptCount.load());

    const auto readsAfterConsumedRetry = fixture.store.readCount();
    const auto writesAfterConsumedRetry = fixture.store.writeCount();
    const auto liveAllocationsAfterConsumedRetry = gLiveAllocBytes.load();
    const auto peakAllocationsAfterConsumedRetry = gPeakAllocBytes.load();
    for (std::size_t attempt = 0U; attempt < 3U; ++attempt) {
        TEST_ASSERT_TRUE(fixture.service.recoverRuntimeFailure() ==
                         fermentation::ConfigurationCommitResolutionStatus::
                             ResolutionRuntimeFailure);
    }
    TEST_ASSERT_EQUAL_UINT64(readsAfterConsumedRetry,
                             fixture.store.readCount());
    TEST_ASSERT_EQUAL_UINT64(0U, preparation.attemptCount.load());
    TEST_ASSERT_EQUAL_UINT64(writesAfterConsumedRetry,
                             fixture.store.writeCount());
    TEST_ASSERT_EQUAL_UINT64(liveAllocationsAfterConsumedRetry,
                             gLiveAllocBytes.load());
    TEST_ASSERT_EQUAL_UINT64(peakAllocationsAfterConsumedRetry,
                             gPeakAllocBytes.load());
    TEST_ASSERT_TRUE(fixture.service.mode() ==
                     fermentation::ConfigurationServiceMode::RuntimeFailure);
    TEST_ASSERT_TRUE(fixture.service.runtimeFailureCause() ==
                     fermentation::ConfigurationRuntimeFailureCause::
                         RuntimePreparationAfterResolutionFailure);
    TEST_ASSERT_TRUE(fixture.service.acquireRuntime().status ==
                     fermentation::RuntimeConfigurationReadStatus::
                         ConfigurationRuntimeUnavailable);
    TEST_ASSERT_TRUE(fixture.service.beginPreview().status ==
                     fermentation::ConfigurationPreviewStatus::
                         ConfigurationRuntimeUnavailable);
    fermentation::ConfigurationServiceTestAccess::clearHook(fixture.service);
}

void test_wrong_retirement_generation_is_revisioned_and_never_activated() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Retirementfehler");
    ConfigurationServiceHookControl control;
    fermentation::ConfigurationServiceTestAccess::installHook(fixture.service,
                                                              control, 1);
    fermentation::ConfigurationCommitStatus commitStatus{};
    std::thread commit([&] {
        commitStatus = fixture.service.confirmPreview(preview.handle).status;
    });
    while (!control.reached.load(std::memory_order_acquire)) {
    }
    const auto revisionBefore = fixture.service.stateRevision();
    TEST_ASSERT_FALSE(
        fermentation::ConfigurationServiceTestAccess::completeRuntimeRetirement(
            fixture.service, std::numeric_limits<std::uint64_t>::max()));
    TEST_ASSERT_EQUAL_UINT64(revisionBefore + 1U,
                             fixture.service.stateRevision());
    TEST_ASSERT_TRUE(fixture.service.mode() ==
                     fermentation::ConfigurationServiceMode::RuntimeFailure);
    TEST_ASSERT_TRUE(fixture.service.runtimeFailureCause() ==
                     fermentation::ConfigurationRuntimeFailureCause::
                         ConfigurationModelBudgetInvariantViolation);
    control.released.store(true, std::memory_order_release);
    commit.join();
    fermentation::ConfigurationServiceTestAccess::clearHook(fixture.service);
    TEST_ASSERT_TRUE(
        commitStatus ==
        fermentation::ConfigurationCommitStatus::ConfigurationRuntimeFailure);
    TEST_ASSERT_TRUE(fixture.service.acquireRuntime().status ==
                     fermentation::RuntimeConfigurationReadStatus::
                         ConfigurationRuntimeUnavailable);
    TEST_ASSERT_TRUE(fixture.service.beginPreview().status ==
                     fermentation::ConfigurationPreviewStatus::
                         ConfigurationRuntimeUnavailable);
}

void test_wrong_retirement_generation_never_returns_recovered_new() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Recovery-Retirement");
    fixture.store.failWrite(
        "cr1", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        true);
    fixture.store.failReadsAfterWrite(
        "cr1", {{"cr1", device_platform::StateStoreReadStatus::ReadError}});
    TEST_ASSERT_TRUE(fixture.service.confirmPreview(preview.handle).status ==
                     fermentation::ConfigurationCommitStatus::
                         ConfigurationCommitIndeterminate);
    fixture.store.clearReadFault("cr1");

    ConfigurationServiceHookControl control;
    fermentation::ConfigurationServiceTestAccess::installHook(fixture.service,
                                                              control, 1);
    fermentation::ConfigurationCommitResolutionStatus resolutionStatus{};
    std::thread resolution(
        [&] { resolutionStatus = fixture.service.resolveIndeterminate(); });
    while (!control.reached.load(std::memory_order_acquire)) {
    }
    const auto revisionBefore = fixture.service.stateRevision();
    TEST_ASSERT_FALSE(
        fermentation::ConfigurationServiceTestAccess::completeRuntimeRetirement(
            fixture.service, std::numeric_limits<std::uint64_t>::max()));
    TEST_ASSERT_EQUAL_UINT64(revisionBefore + 1U,
                             fixture.service.stateRevision());
    control.released.store(true, std::memory_order_release);
    resolution.join();
    fermentation::ConfigurationServiceTestAccess::clearHook(fixture.service);
    TEST_ASSERT_TRUE(resolutionStatus ==
                     fermentation::ConfigurationCommitResolutionStatus::
                         ResolutionRuntimeFailure);
    TEST_ASSERT_TRUE(fixture.service.mode() ==
                     fermentation::ConfigurationServiceMode::RuntimeFailure);
    TEST_ASSERT_TRUE(fixture.service.runtimeFailureCause() ==
                     fermentation::ConfigurationRuntimeFailureCause::
                         ConfigurationModelBudgetInvariantViolation);
}

void test_state_revision_headroom_blocks_before_first_write() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Kein Overflow");
    fermentation::ConfigurationServiceTestAccess::setStateRevision(
        fixture.service, std::numeric_limits<std::uint64_t>::max() - 1U);
    const auto beforeWrites = fixture.store.writeCount();
    TEST_ASSERT_TRUE(
        fixture.service.confirmPreview(preview.handle).status ==
        fermentation::ConfigurationCommitStatus::ConfigurationRuntimeFailure);
    TEST_ASSERT_EQUAL_UINT64(beforeWrites, fixture.store.writeCount());
    TEST_ASSERT_TRUE(fixture.service.mode() ==
                     fermentation::ConfigurationServiceMode::RuntimeFailure);
}

void test_publish_contract_violation_never_returns_activated() {
    Fixture fixture;
    const auto preview = installChangedPreview(fixture, "Publish verletzt");
    ConfigurationServiceHookControl control;
    fermentation::ConfigurationServiceTestAccess::installHook(fixture.service,
                                                              control, 4);
    fermentation::ConfigurationCommitStatus status{};
    std::thread commit([&] {
        status = fixture.service.confirmPreview(preview.handle).status;
    });
    while (!control.reached.load(std::memory_order_acquire)) {
    }
    fermentation::ConfigurationServiceTestAccess::forceRuntimeFailure(
        fixture.service);
    control.released.store(true, std::memory_order_release);
    commit.join();
    fermentation::ConfigurationServiceTestAccess::clearHook(fixture.service);
    TEST_ASSERT_TRUE(
        status ==
        fermentation::ConfigurationCommitStatus::ConfigurationRuntimeFailure);
    TEST_ASSERT_TRUE(fixture.service.runtimeFailureCause() ==
                     fermentation::ConfigurationRuntimeFailureCause::
                         PublishContractViolation);
    TEST_ASSERT_TRUE(fixture.service.acquireRuntime().status ==
                     fermentation::RuntimeConfigurationReadStatus::
                         ConfigurationRuntimeUnavailable);
}

void test_state_revision_invariant_after_publish_never_returns_activated() {
    Fixture fixture;
    const auto preview =
        installChangedPreview(fixture, "Revision nach Publish");
    ConfigurationServiceHookControl control;
    fermentation::ConfigurationServiceTestAccess::installHook(fixture.service,
                                                              control, 1);
    fermentation::ConfigurationCommitStatus status{};
    std::thread commit([&] {
        status = fixture.service.confirmPreview(preview.handle).status;
    });
    while (!control.reached.load(std::memory_order_acquire)) {
    }
    fermentation::ConfigurationServiceTestAccess::setStateRevision(
        fixture.service, std::numeric_limits<std::uint64_t>::max());
    control.released.store(true, std::memory_order_release);
    commit.join();
    fermentation::ConfigurationServiceTestAccess::clearHook(fixture.service);
    TEST_ASSERT_TRUE(
        status ==
        fermentation::ConfigurationCommitStatus::ConfigurationRuntimeFailure);
    TEST_ASSERT_TRUE(fixture.service.mode() ==
                     fermentation::ConfigurationServiceMode::RuntimeFailure);
}

void test_preview_reports_schema_bound_integrity_and_redacted_summary() {
    Fixture fixture;
    auto build = fixture.service.beginPreview();
    build.lease.userConfiguration().deviceName = "Zusammenfassung";
    auto extra = build.lease.programCatalog().programs.back();
    extra.program.id = "zusatz";
    extra.program.name = "Zusatz";
    extra.program.builtIn = false;
    extra.program.factoryCatalogEntry = false;
    extra.program.resettable = false;
    extra.program.userDeletable = true;
    build.lease.programCatalog().programs.push_back(std::move(extra));
    const auto installed = fixture.service.installPreview(
        std::move(build.lease), fermentation::decodeChangeOrigin(2U),
        fermentation::decodeChangeOperation(1U));
    TEST_ASSERT_TRUE(installed.status ==
                     fermentation::ConfigurationPreviewStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(1U, installed.preview->integrity.userSchema);
    TEST_ASSERT_EQUAL_UINT32(1U, installed.preview->integrity.serviceSchema);
    TEST_ASSERT_EQUAL_UINT32(1U, installed.preview->integrity.programSchema);
    TEST_ASSERT_TRUE(installed.preview->summary.deviceNameChanged);
    TEST_ASSERT_EQUAL_UINT16(1U, installed.preview->summary.programsAdded);
}

void test_persistent_failure_causes_remain_distinct() {
    {
        Fixture fixture;
        const auto preview = installChangedPreview(fixture, "Kollision");
        const fermentation::UserConfiguration other{
            "de", "Europe/Zurich", "Gleiche Revision, anderer Inhalt"};
        std::string payload;
        TEST_ASSERT_TRUE(fermentation::encodeUserConfigurationPayload(
                             other, fixture.resolver, payload) ==
                         fermentation::ConfigurationCodecStatus::Success);
        fixture.store.put(
            "uc1", envelope(fermentation::configuration_storage_contract::
                                kUserConfigurationRecordType,
                            1U, payload));
        TEST_ASSERT_TRUE(
            fixture.service.confirmPreview(preview.handle).status ==
            fermentation::ConfigurationCommitStatus::
                ConfigurationRuntimeFailure);
        TEST_ASSERT_TRUE(fixture.service.runtimeFailureCause() ==
                         fermentation::ConfigurationRuntimeFailureCause::
                             PersistentConfigurationIdentityCollision);
    }
    {
        Fixture fixture;
        const auto preview = installChangedPreview(fixture, "Integritaet");
        fixture.store.put("cr1", "corrupt-root-record");
        TEST_ASSERT_TRUE(
            fixture.service.confirmPreview(preview.handle).status ==
            fermentation::ConfigurationCommitStatus::
                ConfigurationRuntimeFailure);
        TEST_ASSERT_TRUE(fixture.service.runtimeFailureCause() ==
                         fermentation::ConfigurationRuntimeFailureCause::
                             PersistentGraphIntegrityFailure);
    }
    {
        Fixture fixture;
        const auto preview = installChangedPreview(fixture, "Neues Schema");
        std::string future;
        TEST_ASSERT_TRUE(device_platform::encodeEnvelope(
                             {fermentation::configuration_storage_contract::
                                  kConfigurationRootRecordType,
                              2U, device_platform::StorageEpoch{1U}, 2U,
                              std::nullopt, "future-root"},
                             future, 114U) ==
                         device_platform::EnvelopeEncodeStatus::Success);
        fixture.store.put("cr1", std::move(future));
        TEST_ASSERT_TRUE(
            fixture.service.confirmPreview(preview.handle).status ==
            fermentation::ConfigurationCommitStatus::
                ConfigurationRuntimeFailure);
        TEST_ASSERT_TRUE(fixture.service.runtimeFailureCause() ==
                         fermentation::ConfigurationRuntimeFailureCause::
                             UnsupportedNewerConfigurationSchema);
    }
    {
        Fixture fixture;
        const auto preview = installChangedPreview(fixture, "Lesefehler");
        fixture.store.failRead(
            "pc3", device_platform::StateStoreReadStatus::ReadError);
        TEST_ASSERT_TRUE(
            fixture.service.confirmPreview(preview.handle).status ==
            fermentation::ConfigurationCommitStatus::
                ConfigurationRuntimeFailure);
        TEST_ASSERT_TRUE(fixture.service.runtimeFailureCause() ==
                         fermentation::ConfigurationRuntimeFailureCause::
                             PersistentStoreReadFailure);
    }
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
    RUN_TEST(
        test_indeterminate_commit_preserves_graph_read_and_capacity_causes);
    RUN_TEST(test_post_commit_verification_failure_is_fail_closed_until_rescan);
    RUN_TEST(test_held_old_reader_blocks_third_model_before_any_write);
    RUN_TEST(test_no_change_confirmation_removes_only_its_visible_handle);
    RUN_TEST(
        test_stale_no_change_revision_conflicts_without_write_or_counter_use);
    RUN_TEST(test_stale_no_change_active_basis_conflicts_without_store_access);
    RUN_TEST(test_maximum_count_catalog_keeps_two_model_limit_through_commit);
    RUN_TEST(test_maximum_valid_active_and_preview_never_create_third_model);
    RUN_TEST(test_preview_cancel_cycles_return_to_stable_live_allocation);
    RUN_TEST(test_two_parallel_preview_starts_reserve_exactly_one_full_model);
    RUN_TEST(test_superseded_no_change_handle_cannot_remove_newer_preview);
    RUN_TEST(test_indeterminate_commit_can_resolve_exactly_old);
    RUN_TEST(test_runtime_failure_does_not_recover_to_old_graph_in_process);
    RUN_TEST(test_two_concurrent_confirmations_have_exactly_one_winner);
    RUN_TEST(
        test_captured_preview_keeps_model_reservation_until_commit_finishes);
    RUN_TEST(
        test_all_old_runtime_leases_hold_retired_generation_until_last_release);
    RUN_TEST(
        test_publish_retirement_keeps_service_closed_until_local_owners_die);
    RUN_TEST(
        test_revoked_preview_build_reservation_is_released_by_identity_only);
    RUN_TEST(
        test_preview_finishing_after_fail_closed_cannot_install_or_lose_budget);
    RUN_TEST(
        test_invalid_runtime_binding_is_rebuilt_before_recovered_new_publish);
    RUN_TEST(
        test_failed_runtime_rebuild_stays_closed_and_can_retry_allowed_cause);
    RUN_TEST(test_failed_runtime_rebuild_retry_is_consumed_before_preparation);
    RUN_TEST(test_failed_runtime_rebuild_scan_consumes_retry_before_first_read);
    RUN_TEST(
        test_wrong_retirement_generation_is_revisioned_and_never_activated);
    RUN_TEST(test_wrong_retirement_generation_never_returns_recovered_new);
    RUN_TEST(test_state_revision_headroom_blocks_before_first_write);
    RUN_TEST(test_publish_contract_violation_never_returns_activated);
    RUN_TEST(
        test_state_revision_invariant_after_publish_never_returns_activated);
    RUN_TEST(test_preview_reports_schema_bound_integrity_and_redacted_summary);
    RUN_TEST(test_persistent_failure_causes_remain_distinct);
    return UNITY_END();
}
