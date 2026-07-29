#include <unity.h>

#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "configuration_bootstrap_store.hpp"
#include "configuration_graph_store.hpp"
#include "configuration_mutation_coordinator.hpp"
#include "configuration_recovery_service.hpp"
#include "configuration_service.hpp"
#include "state_store.hpp"
#include "time_zone_resolver.hpp"

namespace fermentation {
class ConfigurationServiceTestAccess {
   public:
    static void setStateRevision(ConfigurationService& service,
                                 std::uint64_t value) {
        const std::lock_guard<std::mutex> lock(service.stateMutex_);
        service.stateRevision_ = value;
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
                    device_platform::StateStoreWriteStatus status,
                    bool commit) {
        TEST_ASSERT_FALSE(
            commit &&
            (status == device_platform::StateStoreWriteStatus::WriteError ||
             status == device_platform::StateStoreWriteStatus::CapacityError));
        writeFaults_[std::move(key)] = {status, commit};
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
            fermentation::ConfigurationRecoveryStatus::PersistenceFailure),
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
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                                 FactoryInitializationCompleted),
            static_cast<int>(resumed.status));
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
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::ConfigurationRecoveryStatus::
                                 FactoryResetCompleted),
            static_cast<int>(resumed.status));
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
    return UNITY_END();
}
