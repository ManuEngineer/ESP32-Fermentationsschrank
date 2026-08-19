#pragma once

#include <string>

#include "state_store.hpp"

namespace device_platform_esp_idf {

// Owning context supplied configuration. The adapter owns a copy so the
// caller does not have to keep the source strings alive.
struct NvsStateStoreConfig final {
    std::string partition;
    std::string nameSpace;

    NvsStateStoreConfig(std::string partitionName, std::string namespaceName);

    [[nodiscard]] bool isValid() const noexcept;
};

// Concrete ESP-IDF adapter for the existing, application-neutral store port.
// Partition initialization and lifetime belong to the owning Composition Root
// (or to a test harness while no productive consumer exists); this class owns
// neither operation.
class NvsStateStore final : public device_platform::IStateStore {
   public:
    explicit NvsStateStore(NvsStateStoreConfig config);
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

   private:
    NvsStateStoreConfig config_;
};

}  // namespace device_platform_esp_idf
