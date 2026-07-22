#include "mock_state_store.hpp"

namespace device_platform {

bool MockStateStore::write(const std::string& key, const std::string& value) {
    if (writeShouldFail_) {
        return false;
    }
    values_[key] = value;
    return true;
}

std::optional<std::string> MockStateStore::read(const std::string& key) const {
    if (readShouldFail_) {
        return std::nullopt;
    }
    const auto iterator = values_.find(key);
    if (iterator == values_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

void MockStateStore::injectWriteFailure(bool shouldFail) {
    writeShouldFail_ = shouldFail;
}

void MockStateStore::injectReadFailure(bool shouldFail) {
    readShouldFail_ = shouldFail;
}

}  // namespace device_platform
