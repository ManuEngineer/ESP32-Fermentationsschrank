#pragma once

#include "state_store.hpp"

namespace device_platform_esp_idf {

// Concrete ESP-IDF adapter for the existing, application-neutral store port.
// Partition initialization and lifetime belong to the owning Composition Root
// (or to a test harness while no productive consumer exists); this class owns
// neither operation.
class NvsStateStore final : public device_platform::IStateStore {
   public:
    NvsStateStore() noexcept = default;
    ~NvsStateStore() override = default;

    NvsStateStore(const NvsStateStore&) = delete;
    NvsStateStore& operator=(const NvsStateStore&) = delete;
    NvsStateStore(NvsStateStore&&) = delete;
    NvsStateStore& operator=(NvsStateStore&&) = delete;

    [[nodiscard]] device_platform::StateStoreWriteStatus write(
        const device_platform::StateStoreKey& key,
        const std::string& value) override;

    [[nodiscard]] device_platform::StateStoreReadResult read(
        const device_platform::StateStoreKey& key,
        std::size_t maxBytes) const override;
};

}  // namespace device_platform_esp_idf
