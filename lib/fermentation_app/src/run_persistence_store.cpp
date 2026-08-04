#include "run_persistence_store.hpp"

#include "state_store_key.hpp"

namespace fermentation {
namespace {

const device_platform::StateStoreKey& slotKey(std::size_t slot) {
    static const auto rc0 = *device_platform::StateStoreKey::create("rc0").key;
    static const auto rc1 = *device_platform::StateStoreKey::create("rc1").key;
    return slot == 0U ? rc0 : rc1;
}

const device_platform::StateStoreKey& headKey() {
    static const auto rh0 = *device_platform::StateStoreKey::create("rh0").key;
    return rh0;
}

RunPersistenceStoreWriteResult writeExact(
    device_platform::IStateStore& store,
    const device_platform::StateStoreKey& key, const std::string& bytes,
    const std::optional<std::string>& old, std::size_t maxBytes) {
    const auto status = store.write(key, bytes);
    if (status == device_platform::StateStoreWriteStatus::Success) {
        return RunPersistenceStoreWriteResult::Written;
    }
    if (status == device_platform::StateStoreWriteStatus::WriteError) {
        return RunPersistenceStoreWriteResult::WriteError;
    }
    if (status == device_platform::StateStoreWriteStatus::CapacityError) {
        return RunPersistenceStoreWriteResult::CapacityError;
    }
    const auto read = store.read(key, maxBytes);
    if (read.status == device_platform::StateStoreReadStatus::Success) {
        if (read.value == bytes) return RunPersistenceStoreWriteResult::Written;
        if (old.has_value() && read.value == *old) {
            return RunPersistenceStoreWriteResult::NotWritten;
        }
        return RunPersistenceStoreWriteResult::Indeterminate;
    }
    if (read.status == device_platform::StateStoreReadStatus::NotFound &&
        !old.has_value()) {
        return RunPersistenceStoreWriteResult::NotWritten;
    }
    return RunPersistenceStoreWriteResult::Indeterminate;
}

}  // namespace

device_platform::StateStoreReadResult RunPersistenceStore::readHead(
    std::size_t maxBytes) const {
    return store_.read(headKey(), maxBytes);
}

device_platform::StateStoreReadResult RunPersistenceStore::readSlot(
    std::size_t slot, std::size_t maxBytes) const {
    return store_.read(slotKey(slot), maxBytes);
}

RunPersistenceStoreWriteResult RunPersistenceStore::writeHeadExact(
    const std::string& bytes, const std::optional<std::string>& old,
    std::size_t maxBytes) {
    return writeExact(store_, headKey(), bytes, old, maxBytes);
}

RunPersistenceStoreWriteResult RunPersistenceStore::writeSlotExact(
    std::size_t slot, const std::string& bytes,
    const std::optional<std::string>& old, std::size_t maxBytes) {
    return writeExact(store_, slotKey(slot), bytes, old, maxBytes);
}

}  // namespace fermentation
