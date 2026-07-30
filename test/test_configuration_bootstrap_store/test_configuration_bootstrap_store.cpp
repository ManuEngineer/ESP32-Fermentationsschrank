#include <unity.h>

#include <map>
#include <optional>
#include <string>
#include <utility>

#include "configuration_bootstrap_store.hpp"
#include "configuration_bootstrap_codec.hpp"
#include "configuration_storage_contract.hpp"
#include "state_store.hpp"
#include "storage_envelope.hpp"

namespace fermentation {
class ConfigurationBootstrapStoreTestAccess {
   public:
    static ConfigurationBootstrapWriteResult initialize(
        ConfigurationBootstrapStore& store) {
        return store.writeInitialInitializing();
    }
    static ConfigurationBootstrapWriteResult advance(
        ConfigurationBootstrapStore& store,
        const LoadedConfigurationBootstrap& expected,
        ConfigurationBootstrapState target) {
        return store.writeSuccessor(expected, target);
    }
};
}  // namespace fermentation

namespace {

std::string bootstrapBytes(std::uint64_t epoch, std::uint64_t sequence,
                           fermentation::ConfigurationBootstrapState state) {
    std::string bytes;
    const fermentation::ConfigurationBootstrapRecord record{
        fermentation::ConfigurationBootstrapSequence{sequence},
        fermentation::kConfigurationStorageFormatVersion1,
        device_platform::StorageEpoch{epoch}, state};
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapCodecStatus::Success),
        static_cast<int>(
            fermentation::encodeConfigurationBootstrapRecord(record, bytes)));
    return bytes;
}

class LocalStore final : public device_platform::IStateStore {
   public:
    device_platform::StateStoreWriteStatus write(
        const device_platform::StateStoreKey& key,
        const std::string& value) override {
        if (writeStatus_ ==
                device_platform::StateStoreWriteStatus::CommitOutcomeUnknown &&
            !commitUnknown_) {
            return writeStatus_;
        }
        if ((writeStatus_ == device_platform::StateStoreWriteStatus::Success &&
             commitSuccess_) ||
            writeStatus_ ==
                device_platform::StateStoreWriteStatus::CommitOutcomeUnknown) {
            values_[key.bytes()] = value;
        }
        if (readStatusAfterWrite_.has_value()) {
            readStatus_ = *readStatusAfterWrite_;
        }
        if (foreignReadbackAfterWrite_) {
            values_[key.bytes()] = "foreign-readback";
        }
        return writeStatus_;
    }

    device_platform::StateStoreReadResult read(
        const device_platform::StateStoreKey& key,
        std::size_t maxBytes) const override {
        if (readStatus_ != device_platform::StateStoreReadStatus::Success) {
            return {readStatus_, {}};
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

    void setWriteStatus(device_platform::StateStoreWriteStatus status,
                        bool commitUnknown = false) {
        writeStatus_ = status;
        commitUnknown_ = commitUnknown;
    }
    void setReadStatus(device_platform::StateStoreReadStatus status) {
        readStatus_ = status;
    }
    void setCommitSuccess(bool commit) { commitSuccess_ = commit; }
    void setReadStatusAfterWrite(device_platform::StateStoreReadStatus status) {
        readStatusAfterWrite_ = status;
    }
    void setForeignReadbackAfterWrite(bool enabled) {
        foreignReadbackAfterWrite_ = enabled;
    }
    void put(std::string key, std::string value) {
        values_[std::move(key)] = std::move(value);
    }

   private:
    mutable std::map<std::string, std::string> values_;
    device_platform::StateStoreWriteStatus writeStatus_{
        device_platform::StateStoreWriteStatus::Success};
    device_platform::StateStoreReadStatus readStatus_{
        device_platform::StateStoreReadStatus::Success};
    bool commitUnknown_{false};
    bool commitSuccess_{true};
    std::optional<device_platform::StateStoreReadStatus> readStatusAfterWrite_;
    bool foreignReadbackAfterWrite_{false};
};

void test_empty_initializes_and_progresses_history() {
    LocalStore store;
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationBootstrapScanStatus::Empty),
        static_cast<int>(bootstrap.scan().status));
    auto initializing =
        fermentation::ConfigurationBootstrapStoreTestAccess::initialize(
            bootstrap);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapWriteStatus::Success),
        static_cast<int>(initializing.status));
    TEST_ASSERT_TRUE(initializing.loaded.has_value());
    auto initialized =
        fermentation::ConfigurationBootstrapStoreTestAccess::advance(
            bootstrap, *initializing.loaded,
            fermentation::ConfigurationBootstrapState::Initialized);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapWriteStatus::Success),
        static_cast<int>(initialized.status));
    TEST_ASSERT_EQUAL_UINT64(2U, initialized.loaded->record.sequence.value());
}

void test_unknown_old_and_new_are_distinguished() {
    LocalStore oldStore;
    oldStore.setWriteStatus(
        device_platform::StateStoreWriteStatus::CommitOutcomeUnknown, false);
    fermentation::ConfigurationBootstrapStore oldBootstrap(oldStore);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationBootstrapWriteStatus::
                             CommitNotEffective),
        static_cast<int>(
            fermentation::ConfigurationBootstrapStoreTestAccess::initialize(
                oldBootstrap)
                .status));

    LocalStore newStore;
    newStore.setWriteStatus(
        device_platform::StateStoreWriteStatus::CommitOutcomeUnknown, true);
    fermentation::ConfigurationBootstrapStore newBootstrap(newStore);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapWriteStatus::Success),
        static_cast<int>(
            fermentation::ConfigurationBootstrapStoreTestAccess::initialize(
                newBootstrap)
                .status));
}

void test_read_errors_are_not_empty() {
    LocalStore store;
    store.setReadStatus(device_platform::StateStoreReadStatus::ReadError);
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapScanStatus::ReadError),
        static_cast<int>(bootstrap.scan().status));
}

void test_success_without_new_readback_is_store_contract_violation() {
    LocalStore store;
    store.setCommitSuccess(false);
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    const auto write =
        fermentation::ConfigurationBootstrapStoreTestAccess::initialize(
            bootstrap);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapWriteStatus::IntegrityFailure),
        static_cast<int>(write.status));

    LocalStore foreignStore;
    foreignStore.setForeignReadbackAfterWrite(true);
    fermentation::ConfigurationBootstrapStore foreignBootstrap(foreignStore);
    const auto foreignWrite =
        fermentation::ConfigurationBootstrapStoreTestAccess::initialize(
            foreignBootstrap);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapWriteStatus::IntegrityFailure),
        static_cast<int>(foreignWrite.status));
}

void test_unknown_read_failures_remain_indeterminate() {
    for (const auto readStatus :
         {device_platform::StateStoreReadStatus::ReadError,
          device_platform::StateStoreReadStatus::CapacityError}) {
        LocalStore store;
        store.setWriteStatus(
            device_platform::StateStoreWriteStatus::CommitOutcomeUnknown, true);
        store.setReadStatusAfterWrite(readStatus);
        fermentation::ConfigurationBootstrapStore bootstrap(store);
        const auto write =
            fermentation::ConfigurationBootstrapStoreTestAccess::initialize(
                bootstrap);
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(fermentation::ConfigurationBootstrapWriteStatus::
                                 BootstrapCommitIndeterminate),
            static_cast<int>(write.status));
    }
}

void test_two_slot_history_and_duplicates_are_canonical() {
    LocalStore store;
    store.put(
        "cb0",
        bootstrapBytes(
            1U, 1U, fermentation::ConfigurationBootstrapState::Initializing));
    store.put(
        "cb1",
        bootstrapBytes(1U, 2U,
                       fermentation::ConfigurationBootstrapState::Initialized));
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    auto scan = bootstrap.scan();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapScanStatus::Available),
        static_cast<int>(scan.status));
    TEST_ASSERT_EQUAL_UINT64(2U, scan.loaded->record.sequence.value());

    LocalStore duplicateStore;
    const auto duplicate = bootstrapBytes(
        2U, 4U, fermentation::ConfigurationBootstrapState::Initialized);
    duplicateStore.put("cb0", duplicate);
    duplicateStore.put("cb1", duplicate);
    fermentation::ConfigurationBootstrapStore duplicateBootstrap(
        duplicateStore);
    const auto duplicateScan = duplicateBootstrap.scan();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapScanStatus::Available),
        static_cast<int>(duplicateScan.status));
    TEST_ASSERT_TRUE(duplicateScan.loaded->duplicate);
    TEST_ASSERT_EQUAL_UINT32(0U, duplicateScan.loaded->slot.value());
}

// A caller's "expected" snapshot can only be stale relative to the store if
// something else advanced the canonical bootstrap between the caller's read
// and this write attempt. writeSuccessor() must reject that without writing,
// and callers must never treat it as proof that the caller's own old
// snapshot is still the persisted state (see beginAuthorizedFactoryReset()).
void test_write_successor_rejects_stale_expected_without_write() {
    LocalStore store;
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    auto initializing =
        fermentation::ConfigurationBootstrapStoreTestAccess::initialize(
            bootstrap);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapWriteStatus::Success),
        static_cast<int>(initializing.status));
    auto stale = *initializing.loaded;
    stale.record.sequence = fermentation::ConfigurationBootstrapSequence{99U};
    const auto result =
        fermentation::ConfigurationBootstrapStoreTestAccess::advance(
            bootstrap, stale,
            fermentation::ConfigurationBootstrapState::Initialized);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapWriteStatus::InvalidTransition),
        static_cast<int>(result.status));
    TEST_ASSERT_FALSE(result.loaded.has_value());
}

// A newer bootstrap or storage format discovered by writeSuccessor()'s own
// re-scan must block fail closed and must never be treated as proof that the
// caller's previously observed record is still safely in place.
void test_write_successor_detects_newer_schema_during_rescan() {
    LocalStore store;
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    auto initializing =
        fermentation::ConfigurationBootstrapStoreTestAccess::initialize(
            bootstrap);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapWriteStatus::Success),
        static_cast<int>(initializing.status));
    std::string newerSchemaBytes;
    TEST_ASSERT_TRUE(device_platform::encodeEnvelope(
                         {fermentation::configuration_storage_contract::
                              kConfigurationBootstrapRecordType,
                          2U, device_platform::StorageEpoch{1U}, 1U,
                          std::nullopt, std::string(5U, '\0')},
                         newerSchemaBytes, 42U) ==
                     device_platform::EnvelopeEncodeStatus::Success);
    store.put("cb1", newerSchemaBytes);
    const auto result =
        fermentation::ConfigurationBootstrapStoreTestAccess::advance(
            bootstrap, *initializing.loaded,
            fermentation::ConfigurationBootstrapState::Initialized);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationBootstrapWriteStatus::
                             UnsupportedNewerSchema),
        static_cast<int>(result.status));
    TEST_ASSERT_FALSE(result.loaded.has_value());
}

void test_impossible_history_gap_and_regression_fail_closed() {
    LocalStore store;
    store.put(
        "cb0",
        bootstrapBytes(
            1U, 1U, fermentation::ConfigurationBootstrapState::Initializing));
    store.put(
        "cb1",
        bootstrapBytes(2U, 4U,
                       fermentation::ConfigurationBootstrapState::Initialized));
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapScanStatus::IntegrityFailure),
        static_cast<int>(bootstrap.scan().status));
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_empty_initializes_and_progresses_history);
    RUN_TEST(test_unknown_old_and_new_are_distinguished);
    RUN_TEST(test_read_errors_are_not_empty);
    RUN_TEST(test_success_without_new_readback_is_store_contract_violation);
    RUN_TEST(test_unknown_read_failures_remain_indeterminate);
    RUN_TEST(test_two_slot_history_and_duplicates_are_canonical);
    RUN_TEST(test_write_successor_rejects_stale_expected_without_write);
    RUN_TEST(test_write_successor_detects_newer_schema_during_rescan);
    RUN_TEST(test_impossible_history_gap_and_regression_fail_closed);
    return UNITY_END();
}
