#include "stateful_block_device.hpp"

#include <algorithm>
#include <cstring>

#include "spi_flash_mmap.h"

namespace issue90_host {
namespace {

struct Context {
    std::vector<std::uint8_t>* bytes;
    std::vector<BlockTraceEvent>* trace;
    std::optional<std::size_t>* failFrom;
    std::size_t sequence{0U};
};

Context& context(esp_blockdev_handle_t handle) {
    return *static_cast<Context*>(handle->ctx);
}

bool record(Context& ctx, BlockOperation operation, std::uint64_t address,
            std::size_t length) {
    const std::size_t sequence = ++ctx.sequence;
    ctx.trace->push_back(BlockTraceEvent{operation, address, length, sequence});
    return ctx.failFrom->has_value() && sequence >= **ctx.failFrom;
}

esp_err_t read(esp_blockdev_handle_t handle, std::uint8_t* destination,
               std::size_t destinationSize, std::uint64_t sourceAddress,
               std::size_t length) {
    if (handle == nullptr || destination == nullptr ||
        destinationSize < length || sourceAddress > SIZE_MAX - length) {
        return ESP_ERR_INVALID_ARG;
    }
    auto& ctx = context(handle);
    if (sourceAddress + length > ctx.bytes->size()) {
        return ESP_ERR_INVALID_SIZE;
    }
    const bool powerCut =
        record(ctx, BlockOperation::Read, sourceAddress, length);
    std::copy_n(ctx.bytes->data() + sourceAddress, length, destination);
    if (powerCut) return ESP_ERR_FLASH_OP_FAIL;
    return ESP_OK;
}

esp_err_t write(esp_blockdev_handle_t handle, const std::uint8_t* source,
                std::uint64_t destinationAddress, std::size_t length) {
    if (handle == nullptr || source == nullptr ||
        destinationAddress > SIZE_MAX - length) {
        return ESP_ERR_INVALID_ARG;
    }
    auto& ctx = context(handle);
    if (destinationAddress + length > ctx.bytes->size()) {
        return ESP_ERR_INVALID_SIZE;
    }
    const bool powerCut =
        record(ctx, BlockOperation::Write, destinationAddress, length);
    for (std::size_t index = 0U; index < length; ++index) {
        ctx.bytes->at(destinationAddress + index) &= source[index];
    }
    if (powerCut) return ESP_ERR_FLASH_OP_FAIL;
    return ESP_OK;
}

esp_err_t erase(esp_blockdev_handle_t handle, std::uint64_t startAddress,
                std::size_t length) {
    if (handle == nullptr || startAddress > SIZE_MAX - length) {
        return ESP_ERR_INVALID_ARG;
    }
    auto& ctx = context(handle);
    if (startAddress + length > ctx.bytes->size()) {
        return ESP_ERR_INVALID_SIZE;
    }
    const bool powerCut =
        record(ctx, BlockOperation::Erase, startAddress, length);
    std::fill_n(ctx.bytes->data() + startAddress, length, 0xffU);
    if (powerCut) return ESP_ERR_FLASH_OP_FAIL;
    return ESP_OK;
}

esp_err_t sync(esp_blockdev_handle_t handle) {
    if (handle == nullptr) return ESP_ERR_INVALID_ARG;
    auto& ctx = context(handle);
    const bool powerCut = record(ctx, BlockOperation::Sync, 0U, 0U);
    if (powerCut) return ESP_ERR_FLASH_OP_FAIL;
    return ESP_OK;
}

esp_err_t ioctl(esp_blockdev_handle_t handle, std::uint8_t command,
                void* arguments) {
    if (handle == nullptr || arguments == nullptr) return ESP_ERR_INVALID_ARG;
    auto& ctx = context(handle);
    const bool powerCut = record(ctx, BlockOperation::Ioctl, 0U, 0U);
    if (powerCut) return ESP_ERR_FLASH_OP_FAIL;
    if (command == ESP_BLOCKDEV_CMD_MARK_DELETED ||
        command == ESP_BLOCKDEV_CMD_ERASE_CONTENTS) {
        const auto* eraseArguments =
            static_cast<const esp_blockdev_cmd_arg_erase_t*>(arguments);
        return erase(handle, eraseArguments->start_addr,
                     eraseArguments->erase_len);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t release(esp_blockdev_handle_t handle) {
    if (handle == nullptr) return ESP_ERR_INVALID_ARG;
    auto* ctx = static_cast<Context*>(handle->ctx);
    delete ctx;
    delete handle;
    return ESP_OK;
}

const esp_blockdev_ops_t kOperations{
    .read = read,
    .write = write,
    .erase = erase,
    .sync = sync,
    .ioctl = ioctl,
    .release = release,
};

}  // namespace

StatefulBlockDevice::StatefulBlockDevice(std::size_t size,
                                         std::size_t eraseSize)
    : bytes_(new std::vector<std::uint8_t>(size, 0xffU)),
      trace_(new std::vector<BlockTraceEvent>()) {
    auto* handle = new esp_blockdev_t{};
    auto* ctx = new Context{bytes_, trace_, &failFrom_, 0U};
    handle->ctx = ctx;
    handle->ops = &kOperations;
    ESP_BLOCKDEV_FLAGS_INST_CONFIG_DEFAULT(handle->device_flags);
    handle->geometry.disk_size = size;
    handle->geometry.read_size = 1U;
    handle->geometry.write_size = 1U;
    handle->geometry.erase_size = eraseSize;
    handle->geometry.recommended_read_size = 32U;
    handle->geometry.recommended_write_size = 32U;
    handle->geometry.recommended_erase_size = eraseSize;
    handle_ = handle;
}

StatefulBlockDevice::~StatefulBlockDevice() {
    if (handle_ != nullptr) handle_->ops->release(handle_);
    delete trace_;
    delete bytes_;
}

void StatefulBlockDevice::clearTrace() {
    trace_->clear();
    auto& ctx = context(handle_);
    ctx.sequence = 0U;
}

void StatefulBlockDevice::failFromCallback(std::size_t callbackNumber) {
    failFrom_ = callbackNumber;
}

void StatefulBlockDevice::clearFailure() { failFrom_.reset(); }

}  // namespace issue90_host
