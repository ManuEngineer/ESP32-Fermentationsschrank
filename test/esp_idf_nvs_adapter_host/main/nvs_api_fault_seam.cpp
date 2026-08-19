#include "nvs_api_fault_seam.hpp"

#include <optional>
#include <cstdint>
#include <utility>

#include "nvs.h"

namespace issue90_host {
namespace {

struct FaultState {
    std::optional<esp_err_t> open;
    std::optional<esp_err_t> sizeQuery;
    std::optional<esp_err_t> read;
    std::optional<std::pair<esp_err_t, bool>> set;
    std::optional<esp_err_t> commit;
    std::optional<std::string> raceReplacement;
    bool raceUsed{false};
};

FaultState& state() {
    static FaultState instance;
    return instance;
}

}  // namespace

void NvsApiFaultSeam::reset() { state() = FaultState{}; }

void NvsApiFaultSeam::failOpen(esp_err_t error) { state().open = error; }

void NvsApiFaultSeam::failSizeQuery(esp_err_t error) {
    state().sizeQuery = error;
}

void NvsApiFaultSeam::failRead(esp_err_t error) { state().read = error; }

void NvsApiFaultSeam::failSet(esp_err_t error, bool afterRealMutation) {
    state().set = std::make_pair(error, afterRealMutation);
}

void NvsApiFaultSeam::failCommit(esp_err_t error) { state().commit = error; }

void NvsApiFaultSeam::raceAfterSizeQuery(std::string replacement) {
    state().raceReplacement = std::move(replacement);
    state().raceUsed = false;
}

}  // namespace issue90_host

extern "C" esp_err_t __real_nvs_open_from_partition(const char* partName,
                                                    const char* namespaceName,
                                                    nvs_open_mode_t mode,
                                                    nvs_handle_t* outHandle);
extern "C" esp_err_t __real_nvs_get_blob(nvs_handle_t handle, const char* key,
                                         void* output, size_t* length);
extern "C" esp_err_t __real_nvs_set_blob(nvs_handle_t handle, const char* key,
                                         const void* value, size_t length);
extern "C" esp_err_t __real_nvs_commit(nvs_handle_t handle);

extern "C" esp_err_t __wrap_nvs_open_from_partition(const char* partName,
                                                    const char* namespaceName,
                                                    nvs_open_mode_t mode,
                                                    nvs_handle_t* outHandle) {
    const auto& fault = issue90_host::state();
    if (fault.open.has_value()) return *fault.open;
    return __real_nvs_open_from_partition(partName, namespaceName, mode,
                                          outHandle);
}

extern "C" esp_err_t __wrap_nvs_get_blob(nvs_handle_t handle, const char* key,
                                         void* output, size_t* length) {
    auto& fault = issue90_host::state();
    if (output == nullptr && fault.sizeQuery.has_value()) {
        return *fault.sizeQuery;
    }
    if (output != nullptr && fault.read.has_value()) return *fault.read;
    if (output != nullptr && fault.raceReplacement.has_value() &&
        fault.raceUsed) {
        *length = fault.raceReplacement->size();
        return ESP_ERR_NVS_INVALID_LENGTH;
    }

    const esp_err_t result = __real_nvs_get_blob(handle, key, output, length);
    if (output == nullptr && result == ESP_OK &&
        fault.raceReplacement.has_value() && !fault.raceUsed) {
        fault.raceUsed = true;
        nvs_handle_t writeHandle = 0;
        if (__real_nvs_open_from_partition("state_store", "fermentation",
                                           NVS_READWRITE,
                                           &writeHandle) == ESP_OK) {
            const auto& replacement = *fault.raceReplacement;
            const std::uint8_t sentinel = 0U;
            const void* value =
                replacement.empty()
                    ? static_cast<const void*>(&sentinel)
                    : static_cast<const void*>(replacement.data());
            static_cast<void>(__real_nvs_set_blob(writeHandle, key, value,
                                                  replacement.size()));
            static_cast<void>(__real_nvs_commit(writeHandle));
            nvs_close(writeHandle);
        }
    }
    return result;
}

extern "C" esp_err_t __wrap_nvs_set_blob(nvs_handle_t handle, const char* key,
                                         const void* value, size_t length) {
    auto& fault = issue90_host::state();
    if (!fault.set.has_value()) {
        return __real_nvs_set_blob(handle, key, value, length);
    }
    const auto [error, afterRealMutation] = *fault.set;
    if (afterRealMutation) {
        static_cast<void>(__real_nvs_set_blob(handle, key, value, length));
    }
    return error;
}

extern "C" esp_err_t __wrap_nvs_commit(nvs_handle_t handle) {
    const auto& fault = issue90_host::state();
    if (fault.commit.has_value()) return *fault.commit;
    return __real_nvs_commit(handle);
}
