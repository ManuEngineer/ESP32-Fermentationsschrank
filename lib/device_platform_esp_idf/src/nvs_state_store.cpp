#include "nvs_state_store.hpp"

#include <cstdint>
#include <string>
#include <utility>

#include "nvs.h"

namespace device_platform_esp_idf {
namespace {

constexpr char kPartitionName[] = "state_store";
constexpr char kNamespaceName[] = "fermentation";

// ESP-IDF v6.0.2 stores blob chunks in 125 data entries of 32 bytes. The
// BLOB_IDX chunk index uses two 7-bit ranges, so the implementation's largest
// representable blob is 127 * 125 * 32 bytes. This is a compatibility bound,
// not a replacement for the selected partition capacity.
constexpr std::size_t kNvsMaximumBlobBytes = 508000U;

// nvs_set_blob requires a non-null value pointer even for a zero-length blob.
constexpr std::uint8_t kEmptyBlobSentinel = 0U;

using device_platform::StateStoreReadResult;
using device_platform::StateStoreReadStatus;
using device_platform::StateStoreWriteStatus;

StateStoreWriteStatus mapSetError(esp_err_t error) {
    switch (error) {
        case ESP_ERR_NVS_VALUE_TOO_LONG:
            return StateStoreWriteStatus::CapacityError;
        case ESP_ERR_NVS_INVALID_HANDLE:
        case ESP_ERR_NVS_READ_ONLY:
        case ESP_ERR_INVALID_ARG:
        case ESP_ERR_NVS_INVALID_NAME:
        case ESP_ERR_NVS_KEY_TOO_LONG:
            // StateStoreKey is valid-by-construction and the adapter supplies
            // an RW handle and a non-null value pointer. These are therefore
            // pre-mutation failures in the controlled production path.
            return StateStoreWriteStatus::WriteError;
        default:
            // nvs_set_blob mutates while writing chunks, the blob index and
            // removing the old value. No other returned error is safely
            // classifiable as unchanged after the call has begun.
            return StateStoreWriteStatus::CommitOutcomeUnknown;
    }
}

StateStoreReadResult readError() {
    return StateStoreReadResult{StateStoreReadStatus::ReadError, {}};
}

}  // namespace

device_platform::StateStoreWriteStatus NvsStateStore::write(
    const device_platform::StateStoreKey& key, const std::string& value) {
    if (value.size() > kNvsMaximumBlobBytes) {
        return StateStoreWriteStatus::CapacityError;
    }

    nvs_handle_t handle = 0;
    const esp_err_t openError = nvs_open_from_partition(
        kPartitionName, kNamespaceName, NVS_READWRITE, &handle);
    if (openError != ESP_OK) {
        return StateStoreWriteStatus::WriteError;
    }

    const void* data = value.empty()
                           ? static_cast<const void*>(&kEmptyBlobSentinel)
                           : static_cast<const void*>(value.data());
    const esp_err_t setError =
        nvs_set_blob(handle, key.bytes().c_str(), data, value.size());
    if (setError != ESP_OK) {
        nvs_close(handle);
        return mapSetError(setError);
    }

    const esp_err_t commitError = nvs_commit(handle);
    nvs_close(handle);
    if (commitError != ESP_OK) {
        return StateStoreWriteStatus::CommitOutcomeUnknown;
    }
    return StateStoreWriteStatus::Success;
}

device_platform::StateStoreReadResult NvsStateStore::read(
    const device_platform::StateStoreKey& key, std::size_t maxBytes) const {
    nvs_handle_t handle = 0;
    const esp_err_t openError = nvs_open_from_partition(
        kPartitionName, kNamespaceName, NVS_READONLY, &handle);
    if (openError == ESP_ERR_NVS_NOT_FOUND) {
        return StateStoreReadResult{StateStoreReadStatus::NotFound, {}};
    }
    if (openError != ESP_OK) {
        return readError();
    }

    std::size_t requiredBytes = 0U;
    const esp_err_t sizeError =
        nvs_get_blob(handle, key.bytes().c_str(), nullptr, &requiredBytes);
    if (sizeError == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return StateStoreReadResult{StateStoreReadStatus::NotFound, {}};
    }
    if (sizeError != ESP_OK) {
        nvs_close(handle);
        return readError();
    }
    if (requiredBytes > maxBytes) {
        nvs_close(handle);
        return StateStoreReadResult{StateStoreReadStatus::CapacityError, {}};
    }

    std::string value(requiredBytes, '\0');
    const void* sentinel = static_cast<const void*>(&kEmptyBlobSentinel);
    void* output = requiredBytes == 0U ? const_cast<void*>(sentinel)
                                       : static_cast<void*>(value.data());
    std::size_t readBytes = requiredBytes;
    const esp_err_t readErrorCode =
        nvs_get_blob(handle, key.bytes().c_str(), output, &readBytes);
    nvs_close(handle);

    if (readErrorCode == ESP_ERR_NVS_NOT_FOUND) {
        return StateStoreReadResult{StateStoreReadStatus::NotFound, {}};
    }
    if (readErrorCode == ESP_ERR_NVS_INVALID_LENGTH && readBytes > maxBytes) {
        return StateStoreReadResult{StateStoreReadStatus::CapacityError, {}};
    }
    if (readErrorCode != ESP_OK || readBytes != requiredBytes) {
        return readError();
    }
    return StateStoreReadResult{StateStoreReadStatus::Success,
                                std::move(value)};
}

}  // namespace device_platform_esp_idf
