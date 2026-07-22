#include "mock_state_store.hpp"

namespace device_platform_test_support {

bool MockStateStore::write(const std::string& key, const std::string& value) {
    if (writeShouldFail_) {
        return false;
    }
    values_[key] = value;
    return true;
}

device_platform::StateStoreReadResult MockStateStore::read(
    const std::string& key) const {
    if (readShouldFail_) {
        return {device_platform::StateStoreReadStatus::Error, {}};
    }
    const auto iterator = values_.find(key);
    if (iterator == values_.end()) {
        return {device_platform::StateStoreReadStatus::NotFound, {}};
    }
    return {device_platform::StateStoreReadStatus::Success, iterator->second};
}

void MockStateStore::injectWriteFailure(bool shouldFail) {
    writeShouldFail_ = shouldFail;
}

void MockStateStore::injectReadFailure(bool shouldFail) {
    readShouldFail_ = shouldFail;
}

}  // namespace device_platform_test_support
