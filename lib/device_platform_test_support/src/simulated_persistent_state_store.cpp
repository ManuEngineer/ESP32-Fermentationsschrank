#include "simulated_persistent_state_store.hpp"

#include <utility>

namespace device_platform_test_support {

using device_platform::StateStoreKey;
using device_platform::StateStoreReadResult;
using device_platform::StateStoreStatus;

StateStoreStatus SimulatedPersistentStateStore::write(
    const StateStoreKey& key, const std::string& value) {
    const WriteFault fault = nextWriteFault_;
    nextWriteFault_ = WriteFault::None;

    switch (fault) {
        case WriteFault::FailBeforeBegin:
        case WriteFault::PowerCutBeforeCommit:
            return StateStoreStatus::WriteError;
        case WriteFault::CapacityExceeded:
            return StateStoreStatus::CapacityError;
        case WriteFault::PowerCutAfterCommitBeforeReturn:
            committed_[key] = value;
            return StateStoreStatus::CommitOutcomeUnknown;
        case WriteFault::None:
            break;
    }
    committed_[key] = value;
    return StateStoreStatus::Success;
}

StateStoreReadResult SimulatedPersistentStateStore::read(
    const StateStoreKey& key, std::size_t maxBytes) const {
    const auto forcedIterator = forceNotFound_.find(key);
    if (forcedIterator != forceNotFound_.end() && forcedIterator->second) {
        return {StateStoreStatus::NotFound, {}};
    }
    const auto failIterator = readShouldFail_.find(key);
    if (failIterator != readShouldFail_.end() && failIterator->second) {
        return {StateStoreStatus::ReadError, {}};
    }
    const auto valueIterator = committed_.find(key);
    if (valueIterator == committed_.end()) {
        return {StateStoreStatus::NotFound, {}};
    }
    if (valueIterator->second.size() > maxBytes) {
        return {StateStoreStatus::CapacityError, {}};
    }
    return {StateStoreStatus::Success, valueIterator->second};
}

void SimulatedPersistentStateStore::setNextWriteFault(WriteFault fault) {
    nextWriteFault_ = fault;
}

void SimulatedPersistentStateStore::injectReadFailure(const StateStoreKey& key,
                                                      bool shouldFail) {
    readShouldFail_[key] = shouldFail;
}

void SimulatedPersistentStateStore::forceNotFound(const StateStoreKey& key,
                                                  bool shouldForce) {
    forceNotFound_[key] = shouldForce;
}

void SimulatedPersistentStateStore::injectCorruption(
    const StateStoreKey& key, std::string corruptedBytes) {
    committed_[key] = std::move(corruptedBytes);
}

void SimulatedPersistentStateStore::restart() {
    nextWriteFault_ = WriteFault::None;
    readShouldFail_.clear();
    forceNotFound_.clear();
}

}  // namespace device_platform_test_support
