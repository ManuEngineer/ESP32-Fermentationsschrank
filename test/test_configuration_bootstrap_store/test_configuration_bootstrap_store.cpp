#include <unity.h>

#include <map>
#include <string>
#include <utility>

#include "configuration_bootstrap_store.hpp"
#include "state_store.hpp"

namespace {

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
        if (writeStatus_ == device_platform::StateStoreWriteStatus::Success ||
            writeStatus_ ==
                device_platform::StateStoreWriteStatus::CommitOutcomeUnknown) {
            values_[key.bytes()] = value;
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
};

void test_empty_initializes_and_progresses_history() {
    LocalStore store;
    fermentation::ConfigurationBootstrapStore bootstrap(store);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(fermentation::ConfigurationBootstrapScanStatus::Empty),
        static_cast<int>(bootstrap.scan().status));
    auto initializing = bootstrap.writeInitialInitializing();
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapWriteStatus::Success),
        static_cast<int>(initializing.status));
    TEST_ASSERT_TRUE(initializing.loaded.has_value());
    auto initialized = bootstrap.writeSuccessor(
        *initializing.loaded,
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
        static_cast<int>(oldBootstrap.writeInitialInitializing().status));

    LocalStore newStore;
    newStore.setWriteStatus(
        device_platform::StateStoreWriteStatus::CommitOutcomeUnknown, true);
    fermentation::ConfigurationBootstrapStore newBootstrap(newStore);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(
            fermentation::ConfigurationBootstrapWriteStatus::Success),
        static_cast<int>(newBootstrap.writeInitialInitializing().status));
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

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_empty_initializes_and_progresses_history);
    RUN_TEST(test_unknown_old_and_new_are_distinguished);
    RUN_TEST(test_read_errors_are_not_empty);
    return UNITY_END();
}
