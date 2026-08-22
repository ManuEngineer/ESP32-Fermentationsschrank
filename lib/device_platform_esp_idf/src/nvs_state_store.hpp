#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "esp_err.h"
#include "nvs.h"
#include "state_store.hpp"

namespace device_platform_esp_idf {

// The owning context supplies both values. The adapter deliberately has no
// application-specific defaults; in particular, "state_store" and
// "fermentation" belong to the R1 composition root, not to this component.
struct NvsStateStoreConfig {
    std::string partitionLabel;
    std::string namespaceName;

    [[nodiscard]] static std::optional<NvsStateStoreConfig> create(
        std::string partitionLabel, std::string namespaceName);
};

struct NvsStateStoreOpenResult;

class NvsStateStore final : public device_platform::IStateStore {
   public:
    [[nodiscard]] static NvsStateStoreOpenResult open(
        const NvsStateStoreConfig& config);

    ~NvsStateStore() override;

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
    explicit NvsStateStore(nvs_handle_t handle) : handle_(handle) {}

    nvs_handle_t handle_;
};

struct NvsStateStoreOpenResult {
    esp_err_t status{ESP_FAIL};
    std::unique_ptr<NvsStateStore> store;
};

}  // namespace device_platform_esp_idf
