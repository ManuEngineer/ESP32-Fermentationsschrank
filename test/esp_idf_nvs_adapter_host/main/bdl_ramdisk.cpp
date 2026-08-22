#include "bdl_ramdisk.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

struct Context {
    std::byte* bytes;
    std::size_t totalSize;
    std::size_t eraseSize;
    std::vector<BdlEvent> events;
    std::array<std::size_t, 4U> occurrences{};
    BdlOperation cutOperation{BdlOperation::Read};
    std::size_t cutOccurrence{0U};
    BdlCutPhase cutPhase{BdlCutPhase::None};
    std::string logicalKey;
    std::uint32_t logicalBaselineChecksum{0U};
};

Context& context(esp_blockdev_handle_t handle) {
    return *static_cast<Context*>(handle->ctx);
}

std::size_t nextOccurrence(Context& disk, BdlOperation operation) {
    const auto index = static_cast<std::size_t>(operation);
    return ++disk.occurrences[index];
}

bool cutMatches(Context& disk, BdlOperation operation, BdlCutPhase phase,
                std::size_t occurrence) {
    return disk.cutOperation == operation && disk.cutOccurrence == occurrence &&
           disk.cutPhase == phase;
}

void record(Context& disk, BdlOperation operation, std::uint64_t offset,
            std::size_t length, std::size_t occurrence, esp_err_t result,
            BdlCutPhase cutPhase) {
    disk.events.push_back({operation, offset, length, occurrence, result,
                           cutPhase, disk.logicalKey,
                           disk.logicalBaselineChecksum});
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
    auto& disk = context(handle);
    const auto occurrence = nextOccurrence(disk, BdlOperation::Read);
    if (cutMatches(disk, BdlOperation::Read, BdlCutPhase::Before, occurrence)) {
        record(disk, BdlOperation::Read, sourceAddress, length, occurrence,
               ESP_FAIL, BdlCutPhase::Before);
        return ESP_FAIL;
    }
    std::memcpy(destination, disk.bytes + sourceAddress, length);
    const auto cut =
        cutMatches(disk, BdlOperation::Read, BdlCutPhase::After, occurrence);
    record(disk, BdlOperation::Read, sourceAddress, length, occurrence,
           cut ? ESP_FAIL : ESP_OK,
           cut ? BdlCutPhase::After : BdlCutPhase::None);
    return cut ? ESP_FAIL : ESP_OK;
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
    auto& disk = context(handle);
    const auto occurrence = nextOccurrence(disk, BdlOperation::Write);
    if (cutMatches(disk, BdlOperation::Write, BdlCutPhase::Before,
                   occurrence)) {
        record(disk, BdlOperation::Write, destinationAddress, length,
               occurrence, ESP_FAIL, BdlCutPhase::Before);
        return ESP_FAIL;
    }
    std::memcpy(disk.bytes + destinationAddress, source, length);
    const auto cut =
        cutMatches(disk, BdlOperation::Write, BdlCutPhase::After, occurrence);
    record(disk, BdlOperation::Write, destinationAddress, length, occurrence,
           cut ? ESP_FAIL : ESP_OK,
           cut ? BdlCutPhase::After : BdlCutPhase::None);
    return cut ? ESP_FAIL : ESP_OK;
}

esp_err_t erase(esp_blockdev_handle_t handle, uint64_t startAddress,
                std::size_t length) {
    if (handle == nullptr || handle->device_flags.read_only ||
        startAddress + length > context(handle).totalSize ||
        startAddress % context(handle).eraseSize != 0U ||
        length % context(handle).eraseSize != 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    auto& disk = context(handle);
    const auto occurrence = nextOccurrence(disk, BdlOperation::Erase);
    if (cutMatches(disk, BdlOperation::Erase, BdlCutPhase::Before,
                   occurrence)) {
        record(disk, BdlOperation::Erase, startAddress, length, occurrence,
               ESP_FAIL, BdlCutPhase::Before);
        return ESP_FAIL;
    }
    std::memset(disk.bytes + startAddress, 0xFF, length);
    const auto cut =
        cutMatches(disk, BdlOperation::Erase, BdlCutPhase::After, occurrence);
    record(disk, BdlOperation::Erase, startAddress, length, occurrence,
           cut ? ESP_FAIL : ESP_OK,
           cut ? BdlCutPhase::After : BdlCutPhase::None);
    return cut ? ESP_FAIL : ESP_OK;
}

esp_err_t sync(esp_blockdev_handle_t handle) {
    if (handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    auto& disk = context(handle);
    const auto occurrence = nextOccurrence(disk, BdlOperation::Sync);
    const auto cut =
        cutMatches(disk, BdlOperation::Sync, BdlCutPhase::After, occurrence);
    record(disk, BdlOperation::Sync, 0U, 0U, occurrence,
           cut ? ESP_FAIL : ESP_OK,
           cut ? BdlCutPhase::After : BdlCutPhase::None);
    return cut ? ESP_FAIL : ESP_OK;
}

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

void TestRamDisk::clearEvents() {
    if (handle_ != nullptr) {
        auto& disk = context(handle_);
        disk.events.clear();
        disk.occurrences.fill(0U);
    }
}

void TestRamDisk::setCutPlan(BdlOperation operation, std::size_t occurrence,
                             BdlCutPhase phase) {
    if (handle_ != nullptr) {
        auto& disk = context(handle_);
        disk.cutOperation = operation;
        disk.cutOccurrence = occurrence;
        disk.cutPhase = phase;
    }
}

void TestRamDisk::armCutForNext(BdlOperation operation, BdlCutPhase phase) {
    if (handle_ != nullptr) {
        auto& disk = context(handle_);
        const auto index = static_cast<std::size_t>(operation);
        disk.cutOperation = operation;
        disk.cutOccurrence = disk.occurrences[index] + 1U;
        disk.cutPhase = phase;
    }
}

void TestRamDisk::clearCutPlan() {
    if (handle_ != nullptr) {
        context(handle_).cutPhase = BdlCutPhase::None;
    }
}

void TestRamDisk::setLogicalKey(const char* key) {
    if (handle_ != nullptr) {
        auto& disk = context(handle_);
        disk.logicalKey = key == nullptr ? "" : key;
        disk.logicalBaselineChecksum = checksum();
    }
}

void TestRamDisk::clearLogicalKey() {
    if (handle_ != nullptr) {
        context(handle_).logicalKey.clear();
    }
}

std::vector<BdlEvent> TestRamDisk::events() const {
    if (handle_ == nullptr) {
        return {};
    }
    return context(handle_).events;
}

std::uint32_t TestRamDisk::checksum() const {
    if (handle_ == nullptr) {
        return 0U;
    }
    const auto& disk = context(handle_);
    std::uint32_t hash = 2166136261U;
    for (std::size_t index = 0U; index < disk.totalSize; ++index) {
        hash ^= std::to_integer<std::uint8_t>(disk.bytes[index]);
        hash *= 16777619U;
    }
    return hash;
}
