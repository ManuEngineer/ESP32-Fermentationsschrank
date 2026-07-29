#include <unity.h>

#include <cstring>
#include <cstdio>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "configuration_bootstrap_store.hpp"
#include "configuration_bootstrap_codec.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_limits.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "state_store.hpp"
#include "storage_envelope.hpp"
#include "time_zone_resolver.hpp"

namespace fermentation {
class ConfigurationServiceTestAccess {
   public:
    static void setStateRevision(ConfigurationService& service,
                                 std::uint64_t value) {
        const std::lock_guard<std::mutex> lock(service.stateMutex_);
        service.stateRevision_ = value;
    }
    static void setMode(ConfigurationService& service,
                        ConfigurationServiceMode mode) {
        const std::lock_guard<std::mutex> lock(service.stateMutex_);
        service.mode_ = mode;
    }
};
}  // namespace fermentation

namespace {

class LocalStore final : public device_platform::IStateStore {
   public:
    device_platform::StateStoreWriteStatus write(
        const device_platform::StateStoreKey& key,
        const std::string& value) override {
        ++writeCount_;
        writeKeys_.push_back(key.bytes());
        const auto fault = writeFaults_.find(key.bytes());
        if (fault == writeFaults_.end()) {
            values_[key.bytes()] = value;
            return device_platform::StateStoreWriteStatus::Success;
        }
        if (fault->second.commit) {
            values_[key.bytes()] = value;
        }
        if (fault->second.readbackFault.has_value()) {
            readFaults_[key.bytes()] = *fault->second.readbackFault;
        }
        return fault->second.status;
    }

    device_platform::StateStoreReadResult read(
        const device_platform::StateStoreKey& key,
        std::size_t maxBytes) const override {
        ++readCount_;
        readKeys_.push_back(key.bytes());
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

    void put(std::string key, std::string value) {
        values_[std::move(key)] = std::move(value);
    }
    void erase(const char* key) { values_.erase(key); }
    void faultWrite(std::string key,
                    device_platform::StateStoreWriteStatus status, bool commit,
                    bool armReadError = false) {
        TEST_ASSERT_FALSE(
            commit &&
            (status == device_platform::StateStoreWriteStatus::WriteError ||
             status == device_platform::StateStoreWriteStatus::CapacityError));
        writeFaults_[std::move(key)] = {
            status, commit,
            armReadError
                ? std::optional<
                      device_platform::
                          StateStoreReadStatus>{device_platform::
                                                    StateStoreReadStatus::
                                                        ReadError}
                : std::nullopt};
    }
    void faultWriteReadback(
        std::string key, device_platform::StateStoreWriteStatus status,
        bool commit, device_platform::StateStoreReadStatus readbackFault) {
        TEST_ASSERT_FALSE(
            commit &&
            (status == device_platform::StateStoreWriteStatus::WriteError ||
             status == device_platform::StateStoreWriteStatus::CapacityError));
        writeFaults_[std::move(key)] = {status, commit, readbackFault};
    }
    void faultRead(std::string key,
                   device_platform::StateStoreReadStatus status) {
        readFaults_[std::move(key)] = status;
    }
    void clearFaults() {
        writeFaults_.clear();
        readFaults_.clear();
    }
    [[nodiscard]] std::optional<std::string> value(const char* key) const {
        const auto found = values_.find(key);
        return found == values_.end()
                   ? std::optional<std::string>{}
                   : std::optional<std::string>{found->second};
    }
    [[nodiscard]] std::size_t writeCount() const { return writeCount_; }
    [[nodiscard]] std::size_t readCount() const { return readCount_; }
    [[nodiscard]] const std::vector<std::string>& readKeys() const {
        return readKeys_;
    }
    [[nodiscard]] const std::vector<std::string>& writeKeys() const {
        return writeKeys_;
    }

   private:
    struct WriteFault {
        device_platform::StateStoreWriteStatus status;
        bool commit;
        std::optional<device_platform::StateStoreReadStatus> readbackFault;
    };
    mutable std::map<std::string, std::string> values_;
    std::map<std::string, WriteFault> writeFaults_;
    mutable std::map<std::string, device_platform::StateStoreReadStatus>
        readFaults_;
    mutable std::size_t readCount_{0U};
    std::size_t writeCount_{0U};
    mutable std::vector<std::string> readKeys_;
    std::vector<std::string> writeKeys_;
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

struct Fixture {
    LocalStore store;
    Resolver resolver;
    fermentation::ConfigurationMutationCoordinator coordinator;
    fermentation::ConfigurationBootstrapStore bootstrap{store};
    fermentation::ConfigurationGraphStore graph{store, resolver};
    fermentation::ConfigurationService service{coordinator, graph, resolver};
    std::unique_ptr<fermentation::ConfigurationRecoveryService> recovery =
        fermentation::ConfigurationRecoveryService::create(
            store, bootstrap, graph, service, coordinator);
};

struct FreshBootResult {
    fermentation::ConfigurationRecoveryStatus status;
    std::uint64_t runtimeEpoch{0U};
};

FreshBootResult bootWithFreshServices(LocalStore& store, Resolver& resolver,
                                      bool repeatAuthorizedReset = false) {
    fermentation::ConfigurationMutationCoordinator coordinator;
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    fermentation::ConfigurationGraphStore graph(store, resolver);
    fermentation::ConfigurationService service(coordinator, graph, resolver);
    auto recovery = fermentation::ConfigurationRecoveryService::create(
        store, bootstrap, graph, service, coordinator);
    auto result = recovery->boot();
    for (std::size_t attempt = 0U;
         attempt < 2U &&
         result.status != fermentation::ConfigurationRecoveryStatus::
                              FactoryInitializationCompleted &&
         result.status !=
             fermentation::ConfigurationRecoveryStatus::FactoryResetCompleted &&
         result.status !=
             fermentation::ConfigurationRecoveryStatus::RuntimeReady;
         ++attempt) {
        result = recovery->boot();
    }
    std::uint64_t bootEpoch = 0U;
    if (result.status ==
        fermentation::ConfigurationRecoveryStatus::RuntimeReady) {
        auto bootRuntime = service.acquireRuntime();
        if (bootRuntime.status ==
            fermentation::RuntimeConfigurationReadStatus::RuntimeLeaseGranted) {
            bootEpoch = bootRuntime.lease.get().storageEpoch().value();
        }
    }
    if (repeatAuthorizedReset && bootEpoch == 1U) {
        result = recovery->beginAuthorizedFactoryReset();
        for (std::size_t attempt = 0U;
             attempt < 2U && result.status !=
                                 fermentation::ConfigurationRecoveryStatus::
                                     FactoryResetCompleted;
             ++attempt) {
            result = recovery->boot();
        }
    }
    auto runtime = service.acquireRuntime();
    return {result.status,
            runtime.status == fermentation::RuntimeConfigurationReadStatus::
                                  RuntimeLeaseGranted
                ? runtime.lease.get().storageEpoch().value()
                : 0U};
}

void test_factory_boot_uses_exactly_one_factory_read_per_known_key() {
    Fixture fixture;
    const auto result = fixture.recovery->boot();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             FactoryInitializationCompleted),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationServiceMode::Operational),
        static_cast<int>(fixture.service.mode()));
    auto runtime = fixture.service.acquireRuntime();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::RuntimeConfigurationReadStatus::RuntimeLeaseGranted),
        static_cast<int>(runtime.status));
    TEST_ASSERT_EQUAL_UINT64(1U, runtime.lease.get().storageEpoch().value());
    TEST_ASSERT_EQUAL_UINT32(
        4U, runtime.lease.get().programCatalog().programs.size());

    std::map<std::string, std::size_t> firstReads;
    for (std::size_t index = 0U;
         index < fixture.store.readKeys().size() && firstReads.size() < 19U;
         ++index) {
        ++firstReads[fixture.store.readKeys()[index]];
    }
    TEST_ASSERT_EQUAL_UINT32(19U, firstReads.size());
    for (const auto& [key, count] : firstReads) {
        static_cast<void>(key);
        TEST_ASSERT_EQUAL_UINT32(1U, count);
    }
    for (const auto& key : fixture.store.writeKeys()) {
        TEST_ASSERT_NULL(std::strstr(key.c_str(), "auth"));
        TEST_ASSERT_NULL(std::strstr(key.c_str(), "secret"));
        TEST_ASSERT_NULL(std::strstr(key.c_str(), "credential"));
    }
}

void test_existing_bytes_without_bootstrap_are_not_factory_initialized() {
    Fixture fixture;
    fixture.store.put("pc2", "old-bytes");
    const auto result = fixture.recovery->boot();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             ConfigurationIntegrityFailure),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT32(0U, fixture.store.writeCount());
}

void test_reboot_loads_initialized_graph() {
    LocalStore store;
    Resolver resolver;
    {
        fermentation::ConfigurationMutationCoordinator coordinator;
        fermentation::ConfigurationBootstrapStore bootstrap(store);
        fermentation::ConfigurationGraphStore graph(store, resolver);
        fermentation::ConfigurationService service(coordinator, graph,
                                                   resolver);
        auto recovery = fermentation::ConfigurationRecoveryService::create(
            store, bootstrap, graph, service, coordinator);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                                 FactoryInitializationCompleted),
            static_cast<int>(recovery->boot().status));
    }
    fermentation::ConfigurationMutationCoordinator coordinator;
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    fermentation::ConfigurationGraphStore graph(store, resolver);
    fermentation::ConfigurationService service(coordinator, graph, resolver);
    auto recovery = fermentation::ConfigurationRecoveryService::create(
        store, bootstrap, graph, service, coordinator);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::RuntimeReady),
        static_cast<int>(recovery->boot().status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationServiceMode::Operational),
        static_cast<int>(service.mode()));
}

void test_factory_reset_advances_epoch_and_preserves_touch_key() {
    Fixture fixture;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             FactoryInitializationCompleted),
        static_cast<int>(fixture.recovery->boot().status));
    fixture.store.put("touch-calibration", "sentinel");
    const auto reset = fixture.recovery->beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::FactoryResetCompleted),
        static_cast<int>(reset.status));
    auto runtime = fixture.service.acquireRuntime();
    TEST_ASSERT_EQUAL_UINT64(2U, runtime.lease.get().storageEpoch().value());
    TEST_ASSERT_TRUE(fixture.store.value("touch-calibration").has_value());
    TEST_ASSERT_EQUAL_STRING("sentinel",
                             fixture.store.value("touch-calibration")->c_str());
}

void test_root_unknown_new_is_resolved_without_publishing_early() {
    Fixture fixture;
    fixture.store.faultWrite(
        "cr0", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        true);
    const auto result = fixture.recovery->boot();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             FactoryInitializationCompleted),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationServiceMode::Operational),
        static_cast<int>(fixture.service.mode()));
}

void test_factory_boot_read_error_is_fail_closed() {
    Fixture fixture;
    fixture.store.faultRead("cm1",
                            device_platform::StateStoreReadStatus::ReadError);
    const auto result = fixture.recovery->boot();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::PersistenceReadFailure),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT32(0U, fixture.store.writeCount());
}

void test_initialization_resumes_after_each_definite_write_cut() {
    const char* keys[]{"cb0", "uc0", "sc0", "pc0", "cm0", "cr0", "cb1"};
    for (const auto* cutKey : keys) {
        Fixture fixture;
        fixture.store.faultWrite(
            cutKey, device_platform::StateStoreWriteStatus::WriteError, false);
        TEST_ASSERT_NOT_EQUAL(
            static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                                 FactoryInitializationCompleted),
            static_cast<int>(fixture.recovery->boot().status));
        fixture.store.clearFaults();
        const auto resumed = fixture.recovery->boot();
        TEST_ASSERT_EQUAL_INT_MESSAGE(
            static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                                 FactoryInitializationCompleted),
            static_cast<int>(resumed.status), cutKey);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(
                fermentation::ConfigurationServiceMode::Operational),
            static_cast<int>(fixture.service.mode()));
    }
}

void test_root_unknown_old_requires_bound_resolution_before_retry() {
    Fixture fixture;
    fixture.store.faultWrite(
        "cr0", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        false);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             ConfigurationCommitIndeterminate),
        static_cast<int>(fixture.recovery->boot().status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationServiceMode::CommitIndeterminate),
        static_cast<int>(fixture.service.mode()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             ConfigurationUnavailable),
        static_cast<int>(fixture.recovery->boot().status));
    fixture.store.clearFaults();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             FactoryInitializationCompleted),
        static_cast<int>(fixture.recovery->boot().status));
}

void test_authorized_reset_without_runtime_recovers_missing_graph() {
    LocalStore store;
    Resolver resolver;
    {
        fermentation::ConfigurationMutationCoordinator coordinator;
        fermentation::ConfigurationBootstrapStore bootstrap(store);
        fermentation::ConfigurationGraphStore graph(store, resolver);
        fermentation::ConfigurationService service(coordinator, graph,
                                                   resolver);
        auto recovery = fermentation::ConfigurationRecoveryService::create(
            store, bootstrap, graph, service, coordinator);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                                 FactoryInitializationCompleted),
            static_cast<int>(recovery->boot().status));
    }
    store.erase("cr0");
    fermentation::ConfigurationMutationCoordinator coordinator;
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    fermentation::ConfigurationGraphStore graph(store, resolver);
    fermentation::ConfigurationService service(coordinator, graph, resolver);
    auto recovery = fermentation::ConfigurationRecoveryService::create(
        store, bootstrap, graph, service, coordinator);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             ConfigurationUnavailable),
        static_cast<int>(recovery->boot().status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::FactoryResetCompleted),
        static_cast<int>(recovery->beginAuthorizedFactoryReset().status));
    auto runtime = service.acquireRuntime();
    TEST_ASSERT_EQUAL_UINT64(2U, runtime.lease.get().storageEpoch().value());
}

void test_split_store_and_coordinator_composition_is_rejected() {
    LocalStore store;
    LocalStore otherStore;
    Resolver resolver;
    fermentation::ConfigurationMutationCoordinator coordinator;
    fermentation::ConfigurationMutationCoordinator otherCoordinator;
    fermentation::ConfigurationBootstrapStore bootstrap(otherStore);
    fermentation::ConfigurationGraphStore graph(store, resolver);
    fermentation::ConfigurationService service(coordinator, graph, resolver);
    TEST_ASSERT_NULL(fermentation::ConfigurationRecoveryService::create(
                         store, bootstrap, graph, service, coordinator)
                         .get());
    fermentation::ConfigurationBootstrapStore matchingBootstrap(store);
    TEST_ASSERT_NULL(
        fermentation::ConfigurationRecoveryService::create(
            store, matchingBootstrap, graph, service, otherCoordinator)
            .get());
}

void test_reset_resumes_after_each_definite_write_cut() {
    const char* keys[]{"cb0", "uc0", "sc0", "pc0", "cm0", "cr0", "cb1"};
    for (const auto* cutKey : keys) {
        Fixture fixture;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                                 FactoryInitializationCompleted),
            static_cast<int>(fixture.recovery->boot().status));
        fixture.store.faultWrite(
            cutKey, device_platform::StateStoreWriteStatus::WriteError, false);
        TEST_ASSERT_NOT_EQUAL(
            static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                                 FactoryResetCompleted),
            static_cast<int>(
                fixture.recovery->beginAuthorizedFactoryReset().status));
        fixture.store.clearFaults();
        const auto resumed =
            std::string(cutKey) == "cb0"
                ? fixture.recovery->beginAuthorizedFactoryReset()
                : fixture.recovery->boot();
        TEST_ASSERT_EQUAL_INT_MESSAGE(
            static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                                 FactoryResetCompleted),
            static_cast<int>(resumed.status), cutKey);
        auto runtime = fixture.service.acquireRuntime();
        TEST_ASSERT_EQUAL_UINT64(2U,
                                 runtime.lease.get().storageEpoch().value());
    }
}

void test_reset_keeps_retired_runtime_generation_until_last_reader_releases() {
    Fixture fixture;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             FactoryInitializationCompleted),
        static_cast<int>(fixture.recovery->boot().status));
    std::vector<fermentation::RuntimeConfigurationReadLease> leases;
    for (std::size_t index = 0U; index < 8U; ++index) {
        auto read = fixture.service.acquireRuntime();
        TEST_ASSERT_TRUE(read.lease.valid());
        leases.push_back(std::move(read.lease));
    }
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::FactoryResetCompleted),
        static_cast<int>(
            fixture.recovery->beginAuthorizedFactoryReset().status));
    TEST_ASSERT_EQUAL_UINT32(2U, fixture.service.fullModelGenerationCount());
    leases.clear();
    TEST_ASSERT_EQUAL_UINT32(1U, fixture.service.fullModelGenerationCount());
}

void test_state_revision_headroom_blocks_before_factory_write() {
    Fixture fixture;
    fermentation::ConfigurationServiceTestAccess::setStateRevision(
        fixture.service, std::numeric_limits<std::uint64_t>::max() - 2U);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::CounterOverflow),
        static_cast<int>(fixture.recovery->boot().status));
    TEST_ASSERT_EQUAL_UINT32(0U, fixture.store.writeCount());
}

void test_shared_mutation_lease_blocks_boot_and_reset_without_writes() {
    Fixture fixture;
    auto held = fixture.coordinator.tryAcquire();
    TEST_ASSERT_TRUE(held.lease.valid());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             ConfigurationMutationBusy),
        static_cast<int>(fixture.recovery->boot().status));
    TEST_ASSERT_EQUAL_UINT32(0U, fixture.store.writeCount());
    held.lease = fermentation::ConfigurationMutationLease{};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             FactoryInitializationCompleted),
        static_cast<int>(fixture.recovery->boot().status));
    const auto writes = fixture.store.writeCount();
    auto heldAgain = fixture.coordinator.tryAcquire();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             ConfigurationMutationBusy),
        static_cast<int>(
            fixture.recovery->beginAuthorizedFactoryReset().status));
    TEST_ASSERT_EQUAL_UINT32(writes, fixture.store.writeCount());
}

void test_authorized_reset_recovers_corrupt_old_graph_without_reusing_slot() {
    LocalStore store;
    Resolver resolver;
    {
        fermentation::ConfigurationMutationCoordinator coordinator;
        fermentation::ConfigurationBootstrapStore bootstrap(store);
        fermentation::ConfigurationGraphStore graph(store, resolver);
        fermentation::ConfigurationService service(coordinator, graph,
                                                   resolver);
        auto recovery = fermentation::ConfigurationRecoveryService::create(
            store, bootstrap, graph, service, coordinator);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                                 FactoryInitializationCompleted),
            static_cast<int>(recovery->boot().status));
    }
    store.put("pc0", "corrupt-old-catalog");
    fermentation::ConfigurationMutationCoordinator coordinator;
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    fermentation::ConfigurationGraphStore graph(store, resolver);
    fermentation::ConfigurationService service(coordinator, graph, resolver);
    auto recovery = fermentation::ConfigurationRecoveryService::create(
        store, bootstrap, graph, service, coordinator);
    const auto writesBeforeBoot = store.writeCount();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             ConfigurationIntegrityFailure),
        static_cast<int>(recovery->boot().status));
    TEST_ASSERT_EQUAL_UINT32(writesBeforeBoot, store.writeCount());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::FactoryResetCompleted),
        static_cast<int>(recovery->beginAuthorizedFactoryReset().status));
    TEST_ASSERT_EQUAL_STRING("corrupt-old-catalog",
                             store.value("pc0")->c_str());
    TEST_ASSERT_TRUE(store.value("pc1").has_value());

    fermentation::ConfigurationMutationCoordinator rebootCoordinator;
    fermentation::ConfigurationBootstrapStore rebootBootstrap(store);
    fermentation::ConfigurationGraphStore rebootGraph(store, resolver);
    fermentation::ConfigurationService rebootService(rebootCoordinator,
                                                     rebootGraph, resolver);
    auto rebootRecovery = fermentation::ConfigurationRecoveryService::create(
        store, rebootBootstrap, rebootGraph, rebootService, rebootCoordinator);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::RuntimeReady),
        static_cast<int>(rebootRecovery->boot().status));
    auto runtime = rebootService.acquireRuntime();
    TEST_ASSERT_EQUAL_UINT64(2U, runtime.lease.get().storageEpoch().value());
}

void test_initialization_root_unknown_is_resolved_new_on_later_call() {
    Fixture fixture;
    fixture.store.faultWrite(
        "cr0", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        true, true);
    const auto first = fixture.recovery->boot();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             ConfigurationCommitIndeterminate),
        static_cast<int>(first.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationServiceMode::CommitIndeterminate),
        static_cast<int>(fixture.service.mode()));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::RuntimeConfigurationReadStatus::
                             ConfigurationRuntimeUnavailable),
        static_cast<int>(fixture.service.acquireRuntime().status));
    fixture.store.clearFaults();
    const auto second = fixture.recovery->boot();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             FactoryInitializationCompleted),
        static_cast<int>(second.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationServiceMode::Operational),
        static_cast<int>(fixture.service.mode()));
}

void test_reset_root_unknown_is_resolved_new_on_later_call() {
    Fixture fixture;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             FactoryInitializationCompleted),
        static_cast<int>(fixture.recovery->boot().status));
    fixture.store.faultWrite(
        "cr0", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        true, true);
    const auto first = fixture.recovery->beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             ConfigurationCommitIndeterminate),
        static_cast<int>(first.status));
    fixture.store.clearFaults();
    const auto second = fixture.recovery->boot();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::FactoryResetCompleted),
        static_cast<int>(second.status));
    auto runtime = fixture.service.acquireRuntime();
    TEST_ASSERT_EQUAL_UINT64(2U, runtime.lease.get().storageEpoch().value());
}

void test_internal_runtime_failure_is_not_reset_eligible() {
    Fixture fixture;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             FactoryInitializationCompleted),
        static_cast<int>(fixture.recovery->boot().status));
    fermentation::ConfigurationServiceTestAccess::setMode(
        fixture.service,
        fermentation::ConfigurationServiceMode::RuntimeFailure);
    const auto writes = fixture.store.writeCount();
    const auto reset = fixture.recovery->beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::StateTransitionRejected),
        static_cast<int>(reset.status));
    TEST_ASSERT_EQUAL_UINT32(writes, fixture.store.writeCount());
}

void test_no_runtime_failure_exposes_exactly_one_safety_producer() {
    Fixture fixture;
    fixture.store.put("pc2", "corrupt");
    const auto result = fixture.recovery->boot();
    TEST_ASSERT_TRUE(result.safetyProducer.has_value());
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationSafetyProducer::
                             ConfigurationIntegrityFailure),
        static_cast<int>(*result.safetyProducer));
}

void test_recovery_reserves_model_budget_before_factory_allocation() {
    Fixture fixture;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             FactoryInitializationCompleted),
        static_cast<int>(fixture.recovery->boot().status));
    auto preview = fixture.service.beginPreview();
    TEST_ASSERT_TRUE(preview.lease.valid());
    const auto models = fixture.service.fullModelGenerationCount();
    const auto writes = fixture.store.writeCount();
    const auto reset = fixture.recovery->beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             ConfigurationModelBudgetBusy),
        static_cast<int>(reset.status));
    TEST_ASSERT_FALSE(reset.safetyProducer.has_value());
    TEST_ASSERT_EQUAL_UINT32(models,
                             fixture.service.fullModelGenerationCount());
    TEST_ASSERT_EQUAL_UINT32(writes, fixture.store.writeCount());
}

void test_stale_root_resolution_binding_fails_closed() {
    Fixture fixture;
    fixture.store.faultWrite(
        "cr0", device_platform::StateStoreWriteStatus::CommitOutcomeUnknown,
        true, true);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             ConfigurationCommitIndeterminate),
        static_cast<int>(fixture.recovery->boot().status));
    fermentation::ConfigurationServiceTestAccess::setStateRevision(
        fixture.service, fixture.service.stateRevision() + 1U);
    fixture.store.clearFaults();
    const auto result = fixture.recovery->boot();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             ConfigurationIntegrityFailure),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationServiceMode::RuntimeFailure),
        static_cast<int>(fixture.service.mode()));
}

void test_each_initialization_phase_resumes_after_definite_or_old_outcome() {
    const char* keys[]{"cb0", "uc0", "sc0", "pc0", "cm0", "cr0", "cb1"};
    struct Outcome {
        device_platform::StateStoreWriteStatus status;
        bool commit;
        std::optional<device_platform::StateStoreReadStatus> readbackFault;
    };
    const Outcome outcomes[]{
        {device_platform::StateStoreWriteStatus::WriteError, false,
         std::nullopt},
        {device_platform::StateStoreWriteStatus::CapacityError, false,
         std::nullopt},
        {device_platform::StateStoreWriteStatus::CommitOutcomeUnknown, false,
         std::nullopt},
        {device_platform::StateStoreWriteStatus::CommitOutcomeUnknown, true,
         std::nullopt},
        {device_platform::StateStoreWriteStatus::CommitOutcomeUnknown, true,
         device_platform::StateStoreReadStatus::ReadError},
        {device_platform::StateStoreWriteStatus::Success, true,
         device_platform::StateStoreReadStatus::ReadError},
        {device_platform::StateStoreWriteStatus::Success, true,
         device_platform::StateStoreReadStatus::CapacityError}};
    for (const auto* cutKey : keys) {
        for (const auto outcome : outcomes) {
            Fixture fixture;
            fixture.store.put("touch-calibration", "sentinel");
            if (outcome.readbackFault.has_value()) {
                fixture.store.faultWriteReadback(cutKey, outcome.status,
                                                 outcome.commit,
                                                 *outcome.readbackFault);
            } else {
                fixture.store.faultWrite(cutKey, outcome.status,
                                         outcome.commit);
            }
            const auto first = fixture.recovery->boot();
            if (outcome.status != device_platform::StateStoreWriteStatus::
                                      CommitOutcomeUnknown ||
                !outcome.commit || outcome.readbackFault.has_value()) {
                TEST_ASSERT_NOT_EQUAL(
                    static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                                         FactoryInitializationCompleted),
                    static_cast<int>(first.status));
            }
            fixture.store.clearFaults();
            const auto resumed =
                first.status == fermentation::ConfigurationRecoveryStatus::
                                    FactoryInitializationCompleted
                    ? FreshBootResult{first.status, 1U}
                    : bootWithFreshServices(fixture.store, fixture.resolver);
            TEST_ASSERT_TRUE_MESSAGE(
                resumed.status == fermentation::ConfigurationRecoveryStatus::
                                      FactoryInitializationCompleted ||
                    resumed.status ==
                        fermentation::ConfigurationRecoveryStatus::RuntimeReady,
                cutKey);
            TEST_ASSERT_EQUAL_UINT64(1U, resumed.runtimeEpoch);
            TEST_ASSERT_EQUAL_STRING(
                "sentinel", fixture.store.value("touch-calibration")->c_str());
        }
    }
}

void test_each_reset_phase_resumes_after_definite_or_old_outcome() {
    const char* keys[]{"cb0", "uc0", "sc0", "pc0", "cm0", "cr0", "cb1"};
    struct Outcome {
        device_platform::StateStoreWriteStatus status;
        bool commit;
        std::optional<device_platform::StateStoreReadStatus> readbackFault;
    };
    const Outcome outcomes[]{
        {device_platform::StateStoreWriteStatus::WriteError, false,
         std::nullopt},
        {device_platform::StateStoreWriteStatus::CapacityError, false,
         std::nullopt},
        {device_platform::StateStoreWriteStatus::CommitOutcomeUnknown, false,
         std::nullopt},
        {device_platform::StateStoreWriteStatus::CommitOutcomeUnknown, true,
         std::nullopt},
        {device_platform::StateStoreWriteStatus::CommitOutcomeUnknown, true,
         device_platform::StateStoreReadStatus::ReadError},
        {device_platform::StateStoreWriteStatus::Success, true,
         device_platform::StateStoreReadStatus::ReadError},
        {device_platform::StateStoreWriteStatus::Success, true,
         device_platform::StateStoreReadStatus::CapacityError}};
    for (const auto* cutKey : keys) {
        for (const auto outcome : outcomes) {
            Fixture fixture;
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                                     FactoryInitializationCompleted),
                static_cast<int>(fixture.recovery->boot().status));
            fixture.store.put("touch-calibration", "sentinel");
            if (outcome.readbackFault.has_value()) {
                fixture.store.faultWriteReadback(cutKey, outcome.status,
                                                 outcome.commit,
                                                 *outcome.readbackFault);
            } else {
                fixture.store.faultWrite(cutKey, outcome.status,
                                         outcome.commit);
            }
            const auto first = fixture.recovery->beginAuthorizedFactoryReset();
            if (outcome.status != device_platform::StateStoreWriteStatus::
                                      CommitOutcomeUnknown ||
                !outcome.commit || outcome.readbackFault.has_value()) {
                TEST_ASSERT_NOT_EQUAL(
                    static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                                         FactoryResetCompleted),
                    static_cast<int>(first.status));
            }
            fixture.store.clearFaults();
            const auto resumed =
                first.status == fermentation::ConfigurationRecoveryStatus::
                                    FactoryResetCompleted
                    ? FreshBootResult{first.status, 2U}
                    : bootWithFreshServices(fixture.store, fixture.resolver,
                                            true);
            TEST_ASSERT_TRUE_MESSAGE(
                resumed.status == fermentation::ConfigurationRecoveryStatus::
                                      FactoryResetCompleted ||
                    resumed.status ==
                        fermentation::ConfigurationRecoveryStatus::RuntimeReady,
                cutKey);
            TEST_ASSERT_EQUAL_UINT64(2U, resumed.runtimeEpoch);
            TEST_ASSERT_EQUAL_STRING(
                "sentinel", fixture.store.value("touch-calibration")->c_str());
        }
    }
}

void test_schema1_epoch_overflow_blocks_before_graph_or_factory_model() {
    Fixture fixture;
    const auto epoch = std::numeric_limits<std::uint64_t>::max() / 2U;
    fermentation::ConfigurationBootstrapRecord record{
        fermentation::ConfigurationBootstrapSequence{epoch * 2U},
        fermentation::kConfigurationStorageFormatVersion1,
        device_platform::StorageEpoch{epoch},
        fermentation::ConfigurationBootstrapState::Initialized};
    std::string bytes;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapCodecStatus::Success),
        static_cast<int>(
            fermentation::encodeConfigurationBootstrapRecord(record, bytes)));
    fixture.store.put("cb0", bytes);
    const auto reads = fixture.store.readCount();
    const auto result = fixture.recovery->beginAuthorizedFactoryReset();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::CounterOverflow),
        static_cast<int>(result.status));
    TEST_ASSERT_EQUAL_UINT32(reads + 2U, fixture.store.readCount());
    TEST_ASSERT_EQUAL_UINT32(0U, fixture.store.writeCount());
    TEST_ASSERT_EQUAL_UINT32(0U, fixture.service.fullModelGenerationCount());
}

void test_additive_unknown_records_and_envelope_versions_have_no_partial_effect() {
    for (std::size_t variant = 0U; variant < 2U; ++variant) {
        Fixture fixture;
        std::string bytes;
        if (variant == 0U) {
            TEST_ASSERT_TRUE(
                device_platform::encodeEnvelope(
                    {device_platform::RecordTypeId{65000U}, 1U,
                     device_platform::StorageEpoch{1U}, 1U, std::nullopt, "x"},
                    bytes,
                    128U) == device_platform::EnvelopeEncodeStatus::Success);
        } else {
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(
                    fermentation::ConfigurationBootstrapCodecStatus::Success),
                static_cast<
                    int>(fermentation::encodeConfigurationBootstrapRecord(
                    {fermentation::ConfigurationBootstrapSequence{1U},
                     fermentation::kConfigurationStorageFormatVersion1,
                     device_platform::StorageEpoch{1U},
                     fermentation::ConfigurationBootstrapState::Initializing},
                    bytes)));
            bytes[5] = 2;
        }
        fixture.store.put("cb0", bytes);
        const auto result = fixture.recovery->boot();
        TEST_ASSERT_TRUE(
            result.status == fermentation::ConfigurationRecoveryStatus::
                                 ConfigurationIntegrityFailure ||
            result.status == fermentation::ConfigurationRecoveryStatus::
                                 UnsupportedNewerConfigurationSchema ||
            result.status == fermentation::ConfigurationRecoveryStatus::
                                 ConfigurationUnavailable);
        TEST_ASSERT_EQUAL_UINT32(0U, fixture.store.writeCount());
        TEST_ASSERT_EQUAL_UINT32(0U,
                                 fixture.service.fullModelGenerationCount());
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::RuntimeConfigurationReadStatus::
                                 ConfigurationRuntimeUnavailable),
            static_cast<int>(fixture.service.acquireRuntime().status));
    }
}

void test_recovery_resource_peaks_are_measured_and_bounded() {
    Fixture fixture;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                             FactoryInitializationCompleted),
        static_cast<int>(fixture.recovery->boot().status));
    const auto peaks = fixture.recovery->lastResourcePeaks();
    TEST_ASSERT_TRUE(peaks.has_value());
    TEST_ASSERT_GREATER_THAN(0U, peaks->programPayloadCapacity);
    TEST_ASSERT_GREATER_THAN(0U, peaks->documentEnvelopeCapacity);
    TEST_ASSERT_GREATER_THAN(0U, peaks->storeReadbackCapacity);
    TEST_ASSERT_GREATER_THAN(0U, peaks->smallCanonicalRecordCapacity);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
        fermentation::configuration_limits::
            kMaxDistinctConfigurationModelGenerations,
        peaks->fullModelGenerations);
    std::printf(
        "ISSUE57_INIT_RESOURCE_PEAK payload=%zu envelope=%zu readback=%zu "
        "small=%zu models=%zu\n",
        peaks->programPayloadCapacity, peaks->documentEnvelopeCapacity,
        peaks->storeReadbackCapacity, peaks->smallCanonicalRecordCapacity,
        peaks->fullModelGenerations);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationRecoveryStatus::FactoryResetCompleted),
        static_cast<int>(
            fixture.recovery->beginAuthorizedFactoryReset().status));
    const auto resetPeaks = fixture.recovery->lastResourcePeaks();
    TEST_ASSERT_TRUE(resetPeaks.has_value());
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
        fermentation::configuration_limits::
            kMaxDistinctConfigurationModelGenerations,
        resetPeaks->fullModelGenerations);
    std::printf(
        "ISSUE57_RESET_RESOURCE_PEAK payload=%zu envelope=%zu readback=%zu "
        "small=%zu models=%zu\n",
        resetPeaks->programPayloadCapacity,
        resetPeaks->documentEnvelopeCapacity, resetPeaks->storeReadbackCapacity,
        resetPeaks->smallCanonicalRecordCapacity,
        resetPeaks->fullModelGenerations);
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_factory_boot_uses_exactly_one_factory_read_per_known_key);
    RUN_TEST(test_existing_bytes_without_bootstrap_are_not_factory_initialized);
    RUN_TEST(test_reboot_loads_initialized_graph);
    RUN_TEST(test_factory_reset_advances_epoch_and_preserves_touch_key);
    RUN_TEST(test_root_unknown_new_is_resolved_without_publishing_early);
    RUN_TEST(test_factory_boot_read_error_is_fail_closed);
    RUN_TEST(test_initialization_resumes_after_each_definite_write_cut);
    RUN_TEST(test_root_unknown_old_requires_bound_resolution_before_retry);
    RUN_TEST(test_authorized_reset_without_runtime_recovers_missing_graph);
    RUN_TEST(test_split_store_and_coordinator_composition_is_rejected);
    RUN_TEST(test_reset_resumes_after_each_definite_write_cut);
    RUN_TEST(
        test_reset_keeps_retired_runtime_generation_until_last_reader_releases);
    RUN_TEST(test_state_revision_headroom_blocks_before_factory_write);
    RUN_TEST(test_shared_mutation_lease_blocks_boot_and_reset_without_writes);
    RUN_TEST(
        test_authorized_reset_recovers_corrupt_old_graph_without_reusing_slot);
    RUN_TEST(test_initialization_root_unknown_is_resolved_new_on_later_call);
    RUN_TEST(test_reset_root_unknown_is_resolved_new_on_later_call);
    RUN_TEST(test_internal_runtime_failure_is_not_reset_eligible);
    RUN_TEST(test_no_runtime_failure_exposes_exactly_one_safety_producer);
    RUN_TEST(test_recovery_reserves_model_budget_before_factory_allocation);
    RUN_TEST(test_stale_root_resolution_binding_fails_closed);
    RUN_TEST(
        test_each_initialization_phase_resumes_after_definite_or_old_outcome);
    RUN_TEST(test_each_reset_phase_resumes_after_definite_or_old_outcome);
    RUN_TEST(test_schema1_epoch_overflow_blocks_before_graph_or_factory_model);
    RUN_TEST(
        test_additive_unknown_records_and_envelope_versions_have_no_partial_effect);
    RUN_TEST(test_recovery_resource_peaks_are_measured_and_bounded);
    return UNITY_END();
}
