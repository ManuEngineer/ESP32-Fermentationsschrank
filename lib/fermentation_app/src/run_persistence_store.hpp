#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "state_store.hpp"

namespace fermentation {

// Narrow technical wrapper for Issue #17's three fixed records.  It owns no
// storage and keeps exact old/new/absent readback resolution out of the
// application coordinator.
enum class RunPersistenceStoreWriteResult : std::uint8_t {
    Written,
    NotWritten,
    WriteError,
    CapacityError,
    Indeterminate,
};

class RunPersistenceStore {
   public:
    explicit RunPersistenceStore(device_platform::IStateStore& store) noexcept
        : store_(store) {}

    [[nodiscard]] device_platform::StateStoreReadResult readHead(
        std::size_t maxBytes) const;
    [[nodiscard]] device_platform::StateStoreReadResult readSlot(
        std::size_t slot, std::size_t maxBytes) const;
    [[nodiscard]] RunPersistenceStoreWriteResult writeHeadExact(
        const std::string& bytes, const std::optional<std::string>& old,
        std::size_t maxBytes);
    [[nodiscard]] RunPersistenceStoreWriteResult writeSlotExact(
        std::size_t slot, const std::string& bytes,
        const std::optional<std::string>& old, std::size_t maxBytes);

   private:
    device_platform::IStateStore& store_;
};

}  // namespace fermentation
