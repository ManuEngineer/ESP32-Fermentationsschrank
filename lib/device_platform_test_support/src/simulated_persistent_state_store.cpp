#include "simulated_persistent_state_store.hpp"

#include <utility>

namespace device_platform_test_support {

using device_platform::StateStoreKey;
using device_platform::StateStoreReadResult;
using device_platform::StateStoreReadStatus;
using device_platform::StateStoreWriteStatus;

StateStoreWriteStatus SimulatedPersistentStateStore::write(
    const StateStoreKey& key, const std::string& value) {
    const WriteFault fault = nextWriteFault_;
    nextWriteFault_ = WriteFault::None;

    if (fault == WriteFault::FailBeforeBegin) {
        // Kein Staging entsteht; committed_ bleibt unberuehrt.
        return StateStoreWriteStatus::WriteError;
    }
    if (fault == WriteFault::CapacityExceeded) {
        // Kein Staging, kein Commit; committed_ bleibt unberuehrt.
        return StateStoreWriteStatus::CapacityError;
    }

    // Ab hier ist der neue Wert vollstaendig gestagt (bildet die reale
    // Reihenfolge "vollstaendig schreiben, dann atomar committen" nach).
    pendingWrite_ = PendingWrite{key, value};

    if (fault == WriteFault::PowerCutBeforeCommit) {
        // Staging existiert, wird aber nie committet. Nur restart() (ein
        // simulierter Neustart) verwirft es; bis dahin bleibt es als
        // laufende Operation sichtbar (hasPendingWriteForTesting()). Danach
        // ist ausschliesslich der alte committed Wert sichtbar.
        return StateStoreWriteStatus::WriteError;
    }

    // Commit: der gestagte Wert wird atomar in committed_ uebernommen, kein
    // Teil- oder Mischwert ist jemals sichtbar.
    committed_[pendingWrite_->key] = pendingWrite_->value;
    pendingWrite_.reset();

    if (fault == WriteFault::PowerCutAfterCommitBeforeReturn) {
        return StateStoreWriteStatus::CommitOutcomeUnknown;
    }
    return StateStoreWriteStatus::Success;
}

StateStoreReadResult SimulatedPersistentStateStore::read(
    const StateStoreKey& key, std::size_t maxBytes) const {
    const auto forcedIterator = forceNotFound_.find(key);
    if (forcedIterator != forceNotFound_.end() && forcedIterator->second) {
        return {StateStoreReadStatus::NotFound, {}};
    }
    const auto failIterator = readShouldFail_.find(key);
    if (failIterator != readShouldFail_.end() && failIterator->second) {
        return {StateStoreReadStatus::ReadError, {}};
    }
    const auto valueIterator = committed_.find(key);
    if (valueIterator == committed_.end()) {
        return {StateStoreReadStatus::NotFound, {}};
    }
    if (valueIterator->second.size() > maxBytes) {
        return {StateStoreReadStatus::CapacityError, {}};
    }
    return {StateStoreReadStatus::Success, valueIterator->second};
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
    pendingWrite_.reset();
    nextWriteFault_ = WriteFault::None;
    readShouldFail_.clear();
    forceNotFound_.clear();
}

}  // namespace device_platform_test_support
