#include "nvs_state_store.hpp"

#include <utility>

namespace device_platform_esp_idf {
namespace {

device_platform::StateStoreWriteStatus mapSetError(esp_err_t error) {
    // These errors are reported by nvs_set_blob before a durable commit. The
    // v6.0.2 contract describes them as rejected input, a read-only / invalid
    // handle, or insufficient storage; no persistent mutation is possible
    // through this not-yet-committed operation.
    switch (error) {
        case ESP_ERR_NVS_NOT_ENOUGH_SPACE:
        case ESP_ERR_NVS_VALUE_TOO_LONG:
            return device_platform::StateStoreWriteStatus::CapacityError;
        case ESP_ERR_NVS_INVALID_HANDLE:
        case ESP_ERR_NVS_READ_ONLY:
        case ESP_ERR_NVS_INVALID_NAME:
        case ESP_ERR_NVS_KEY_TOO_LONG:
        case ESP_ERR_NVS_INVALID_LENGTH:
        case ESP_ERR_NVS_NOT_INITIALIZED:
        case ESP_ERR_NVS_INVALID_STATE:
        case ESP_ERR_NVS_NO_FREE_PAGES:
        case ESP_ERR_NVS_PART_NOT_FOUND:
        case ESP_ERR_INVALID_ARG:
        case ESP_ERR_NO_MEM:
            return device_platform::StateStoreWriteStatus::WriteError;
        case ESP_ERR_NVS_REMOVE_FAILED:
        case ESP_FAIL:
        default:
            // REMOVE_FAILED explicitly says that the value may already have
            // been written and can finish after reinitialization. Unknown or
            // lower-layer errors receive the same fail-closed treatment.
            return device_platform::StateStoreWriteStatus::CommitOutcomeUnknown;
    }
}

}  // namespace

std::optional<NvsStateStoreConfig> NvsStateStoreConfig::create(
    std::string partitionLabel, std::string namespaceName) {
    // The public v6.0.2 partition-label constant is a 16-byte storage bound,
    // while the actual NVSPartition/BDL admission check requires a label
    // shorter than that bound. Reject the boundary here so a configuration
    // cannot be accepted and then rejected by the selected ESP-IDF backend.
    // Namespace length includes the NUL in NVS_NS_NAME_MAX_SIZE, so the
    // largest accepted namespace is 15 bytes. No character whitelist or
    // silent truncation is introduced.
    if (partitionLabel.empty() ||
        partitionLabel.size() >= NVS_PART_NAME_MAX_SIZE ||
        namespaceName.empty() ||
        namespaceName.size() + 1U > NVS_NS_NAME_MAX_SIZE) {
        return std::nullopt;
    }
    return NvsStateStoreConfig{std::move(partitionLabel),
                               std::move(namespaceName)};
}

NvsStateStoreOpenResult NvsStateStore::open(const NvsStateStoreConfig& config) {
    nvs_handle_t handle = 0U;
    const esp_err_t status = nvs_open_from_partition(
        config.partitionLabel.c_str(), config.namespaceName.c_str(),
        NVS_READWRITE, &handle);
    if (status != ESP_OK) {
        return NvsStateStoreOpenResult{status, nullptr};
    }
    return NvsStateStoreOpenResult{
        ESP_OK, std::unique_ptr<NvsStateStore>(new NvsStateStore(handle))};
}

NvsStateStore::~NvsStateStore() { nvs_close(handle_); }

device_platform::StateStoreReadResult NvsStateStore::read(
    const device_platform::StateStoreKey& key, std::size_t maxBytes) const {
    std::size_t requiredSize = 0U;
    const esp_err_t sizeStatus =
        nvs_get_blob(handle_, key.bytes().c_str(), nullptr, &requiredSize);
    if (sizeStatus == ESP_ERR_NVS_NOT_FOUND) {
        return {device_platform::StateStoreReadStatus::NotFound, {}};
    }
    if (sizeStatus != ESP_OK) {
        return {device_platform::StateStoreReadStatus::ReadError, {}};
    }
    if (requiredSize > maxBytes) {
        return {device_platform::StateStoreReadStatus::CapacityError, {}};
    }
    if (requiredSize == 0U) {
        return {device_platform::StateStoreReadStatus::Success, {}};
    }

    std::string value(requiredSize, '\0');
    std::size_t actualSize = requiredSize;
    const esp_err_t dataStatus =
        nvs_get_blob(handle_, key.bytes().c_str(), value.data(), &actualSize);
    if (dataStatus != ESP_OK || actualSize != requiredSize) {
        // A NOT_FOUND after a successful size query is a read that changed
        // underneath us, not proof that the original key was absent.
        return {device_platform::StateStoreReadStatus::ReadError, {}};
    }
    return {device_platform::StateStoreReadStatus::Success, std::move(value)};
}

device_platform::StateStoreWriteStatus NvsStateStore::write(
    const device_platform::StateStoreKey& key, const std::string& value) {
    const esp_err_t setStatus =
        nvs_set_blob(handle_, key.bytes().c_str(), value.data(), value.size());
    if (setStatus != ESP_OK) {
        return mapSetError(setStatus);
    }

    // Only the pair ESP_OK/ESP_OK is a durable Success. nvs_close() is
    // cleanup only and never substitutes for nvs_commit().
    if (nvs_commit(handle_) == ESP_OK) {
        return device_platform::StateStoreWriteStatus::Success;
    }
    return device_platform::StateStoreWriteStatus::CommitOutcomeUnknown;
}

}  // namespace device_platform_esp_idf
