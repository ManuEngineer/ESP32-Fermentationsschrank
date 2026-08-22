#include "bdl_ramdisk.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace {

struct Context {
    std::byte* bytes;
    std::size_t totalSize;
    std::size_t eraseSize;
};

Context& context(esp_blockdev_handle_t handle) {
    return *static_cast<Context*>(handle->ctx);
}

esp_err_t read(esp_blockdev_handle_t handle, uint8_t* destination,
               std::size_t destinationSize, uint64_t sourceAddress,
               std::size_t length) {
    if (handle == nullptr || destination == nullptr ||
        destinationSize < length ||
        sourceAddress + length > context(handle).totalSize ||
        sourceAddress % handle->geometry.read_size != 0U ||
        length % handle->geometry.read_size != 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    std::memcpy(destination, context(handle).bytes + sourceAddress, length);
    return ESP_OK;
}

esp_err_t write(esp_blockdev_handle_t handle, const uint8_t* source,
                uint64_t destinationAddress, std::size_t length) {
    if (handle == nullptr || source == nullptr ||
        handle->device_flags.read_only ||
        destinationAddress + length > context(handle).totalSize ||
        destinationAddress % handle->geometry.write_size != 0U ||
        length % handle->geometry.write_size != 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    std::memcpy(context(handle).bytes + destinationAddress, source, length);
    return ESP_OK;
}

esp_err_t erase(esp_blockdev_handle_t handle, uint64_t startAddress,
                std::size_t length) {
    if (handle == nullptr || handle->device_flags.read_only ||
        startAddress + length > context(handle).totalSize ||
        startAddress % context(handle).eraseSize != 0U ||
        length % context(handle).eraseSize != 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    std::memset(context(handle).bytes + startAddress, 0xFF, length);
    return ESP_OK;
}

esp_err_t sync(esp_blockdev_handle_t) { return ESP_OK; }

esp_err_t ioctl(esp_blockdev_handle_t handle, uint8_t command, void* argument) {
    if (handle == nullptr || argument == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (command != ESP_BLOCKDEV_CMD_MARK_DELETED &&
        command != ESP_BLOCKDEV_CMD_ERASE_CONTENTS) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    const auto* eraseArgument =
        static_cast<const esp_blockdev_cmd_arg_erase_t*>(argument);
    return erase(handle, eraseArgument->start_addr, eraseArgument->erase_len);
}

esp_err_t release(esp_blockdev_handle_t handle) {
    if (handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    std::free(context(handle).bytes);
    std::free(handle->ctx);
    std::free(handle);
    return ESP_OK;
}

const esp_blockdev_ops_t kOps{
    .read = read,
    .write = write,
    .erase = erase,
    .sync = sync,
    .ioctl = ioctl,
    .release = release,
};

}  // namespace

TestRamDisk::TestRamDisk(std::size_t totalSize, std::size_t eraseSize) {
    if (totalSize == 0U || eraseSize == 0U || totalSize % eraseSize != 0U) {
        return;
    }
    auto* device = static_cast<esp_blockdev_handle_t>(
        std::calloc(1U, sizeof(esp_blockdev_t)));
    auto* diskContext = static_cast<Context*>(std::calloc(1U, sizeof(Context)));
    auto* bytes = static_cast<std::byte*>(std::malloc(totalSize));
    if (device == nullptr || diskContext == nullptr || bytes == nullptr) {
        std::free(bytes);
        std::free(diskContext);
        std::free(device);
        return;
    }
    std::memset(bytes, 0xFF, totalSize);
    diskContext->bytes = bytes;
    diskContext->totalSize = totalSize;
    diskContext->eraseSize = eraseSize;
    device->ctx = diskContext;
    device->ops = &kOps;
    ESP_BLOCKDEV_FLAGS_INST_CONFIG_DEFAULT(device->device_flags);
    device->geometry.disk_size = totalSize;
    device->geometry.read_size = 1U;
    device->geometry.write_size = 1U;
    device->geometry.erase_size = eraseSize;
    device->geometry.recommended_read_size = 32U;
    device->geometry.recommended_write_size = 32U;
    device->geometry.recommended_erase_size = eraseSize;
    handle_ = device;
}

TestRamDisk::~TestRamDisk() {
    if (handle_ != nullptr) {
        handle_->ops->release(handle_);
    }
}
