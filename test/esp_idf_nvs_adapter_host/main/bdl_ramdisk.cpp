#include "bdl_ramdisk.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <unistd.h>
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
    BdlCutMode cutMode{BdlCutMode::ReturnError};
    std::string logicalKey;
    std::string abruptImagePath;
    std::string abruptMetadataPath;
    std::string oldHeadPath;
    std::string newHeadPath;
    std::string oldHead;
    std::string newHead;
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
            BdlCutPhase cutPhase, std::uint32_t baselineChecksum) {
    disk.events.push_back({operation, offset, length, occurrence, result,
                           cutPhase, disk.logicalKey, baselineChecksum});
}

std::uint32_t contextChecksum(const Context& disk) {
    std::uint32_t hash = 2166136261U;
    for (std::size_t index = 0U; index < disk.totalSize; ++index) {
        hash ^= std::to_integer<std::uint8_t>(disk.bytes[index]);
        hash *= 16777619U;
    }
    return hash;
}

[[noreturn]] void abruptExitAfterSnapshot(Context& disk, BdlOperation operation,
                                          std::uint64_t offset,
                                          std::size_t length,
                                          std::size_t occurrence,
                                          BdlCutPhase phase,
                                          std::uint32_t baselineChecksum) {
    if (disk.abruptImagePath.empty() || disk.abruptMetadataPath.empty()) {
        _exit(125);
    }
    std::ofstream image(disk.abruptImagePath,
                        std::ios::binary | std::ios::trunc);
    if (!image.good()) _exit(126);
    image.write(reinterpret_cast<const char*>(disk.bytes),
                static_cast<std::streamsize>(disk.totalSize));
    image.close();
    if (!image.good()) _exit(127);

    std::ofstream metadata(disk.abruptMetadataPath,
                           std::ios::out | std::ios::trunc);
    if (!metadata.good()) _exit(128);
    metadata << "operation=" << static_cast<unsigned>(operation) << '\n'
             << "offset=" << offset << '\n'
             << "length=" << length << '\n'
             << "occurrence=" << occurrence << '\n'
             << "phase=" << static_cast<unsigned>(phase) << '\n'
             << "logical_key=" << disk.logicalKey << '\n'
             << "baseline_checksum=" << baselineChecksum << '\n'
             << "image_checksum=" << contextChecksum(disk) << '\n';
    metadata.close();
    if (!metadata.good()) _exit(129);

    if (!disk.oldHeadPath.empty() && !disk.oldHead.empty()) {
        std::ofstream oldHead(disk.oldHeadPath,
                              std::ios::binary | std::ios::trunc);
        if (!oldHead.good()) _exit(130);
        oldHead.write(disk.oldHead.data(),
                      static_cast<std::streamsize>(disk.oldHead.size()));
        oldHead.close();
        if (!oldHead.good()) _exit(131);
    }
    if (!disk.newHeadPath.empty() && !disk.newHead.empty()) {
        std::ofstream newHead(disk.newHeadPath,
                              std::ios::binary | std::ios::trunc);
        if (!newHead.good()) _exit(132);
        newHead.write(disk.newHead.data(),
                      static_cast<std::streamsize>(disk.newHead.size()));
        newHead.close();
        if (!newHead.good()) _exit(133);
    }
    _exit(0);
}

bool abruptCut(Context& disk, BdlOperation operation, BdlCutPhase phase,
               std::uint64_t offset, std::size_t length, std::size_t occurrence,
               std::uint32_t baselineChecksum) {
    if (disk.cutMode != BdlCutMode::AbruptProcessExit) return false;
    abruptExitAfterSnapshot(disk, operation, offset, length, occurrence, phase,
                            baselineChecksum);
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
    const auto baselineChecksum = contextChecksum(disk);
    const auto occurrence = nextOccurrence(disk, BdlOperation::Read);
    if (cutMatches(disk, BdlOperation::Read, BdlCutPhase::Before, occurrence)) {
        record(disk, BdlOperation::Read, sourceAddress, length, occurrence,
               ESP_FAIL, BdlCutPhase::Before, baselineChecksum);
        abruptCut(disk, BdlOperation::Read, BdlCutPhase::Before, sourceAddress,
                  length, occurrence, baselineChecksum);
        return ESP_FAIL;
    }
    std::memcpy(destination, disk.bytes + sourceAddress, length);
    const auto cut =
        cutMatches(disk, BdlOperation::Read, BdlCutPhase::After, occurrence);
    record(disk, BdlOperation::Read, sourceAddress, length, occurrence,
           cut ? ESP_FAIL : ESP_OK,
           cut ? BdlCutPhase::After : BdlCutPhase::None, baselineChecksum);
    if (cut) {
        abruptCut(disk, BdlOperation::Read, BdlCutPhase::After, sourceAddress,
                  length, occurrence, baselineChecksum);
    }
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
    const auto baselineChecksum = contextChecksum(disk);
    const auto occurrence = nextOccurrence(disk, BdlOperation::Write);
    if (cutMatches(disk, BdlOperation::Write, BdlCutPhase::Before,
                   occurrence)) {
        record(disk, BdlOperation::Write, destinationAddress, length,
               occurrence, ESP_FAIL, BdlCutPhase::Before, baselineChecksum);
        abruptCut(disk, BdlOperation::Write, BdlCutPhase::Before,
                  destinationAddress, length, occurrence, baselineChecksum);
        return ESP_FAIL;
    }
    std::memcpy(disk.bytes + destinationAddress, source, length);
    const auto cut =
        cutMatches(disk, BdlOperation::Write, BdlCutPhase::After, occurrence);
    record(disk, BdlOperation::Write, destinationAddress, length, occurrence,
           cut ? ESP_FAIL : ESP_OK,
           cut ? BdlCutPhase::After : BdlCutPhase::None, baselineChecksum);
    if (cut) {
        abruptCut(disk, BdlOperation::Write, BdlCutPhase::After,
                  destinationAddress, length, occurrence, baselineChecksum);
    }
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
    const auto baselineChecksum = contextChecksum(disk);
    const auto occurrence = nextOccurrence(disk, BdlOperation::Erase);
    if (cutMatches(disk, BdlOperation::Erase, BdlCutPhase::Before,
                   occurrence)) {
        record(disk, BdlOperation::Erase, startAddress, length, occurrence,
               ESP_FAIL, BdlCutPhase::Before, baselineChecksum);
        abruptCut(disk, BdlOperation::Erase, BdlCutPhase::Before, startAddress,
                  length, occurrence, baselineChecksum);
        return ESP_FAIL;
    }
    std::memset(disk.bytes + startAddress, 0xFF, length);
    const auto cut =
        cutMatches(disk, BdlOperation::Erase, BdlCutPhase::After, occurrence);
    record(disk, BdlOperation::Erase, startAddress, length, occurrence,
           cut ? ESP_FAIL : ESP_OK,
           cut ? BdlCutPhase::After : BdlCutPhase::None, baselineChecksum);
    if (cut) {
        abruptCut(disk, BdlOperation::Erase, BdlCutPhase::After, startAddress,
                  length, occurrence, baselineChecksum);
    }
    return cut ? ESP_FAIL : ESP_OK;
}

esp_err_t sync(esp_blockdev_handle_t handle) {
    if (handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    auto& disk = context(handle);
    const auto baselineChecksum = contextChecksum(disk);
    const auto occurrence = nextOccurrence(disk, BdlOperation::Sync);
    const auto cut =
        cutMatches(disk, BdlOperation::Sync, BdlCutPhase::After, occurrence);
    record(disk, BdlOperation::Sync, 0U, 0U, occurrence,
           cut ? ESP_FAIL : ESP_OK,
           cut ? BdlCutPhase::After : BdlCutPhase::None, baselineChecksum);
    if (cut) {
        abruptCut(disk, BdlOperation::Sync, BdlCutPhase::After, 0U, 0U,
                  occurrence, baselineChecksum);
    }
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
                             BdlCutPhase phase, BdlCutMode mode) {
    if (handle_ != nullptr) {
        auto& disk = context(handle_);
        disk.cutOperation = operation;
        disk.cutOccurrence = occurrence;
        disk.cutPhase = phase;
        disk.cutMode = mode;
    }
}

void TestRamDisk::armCutForNext(BdlOperation operation, BdlCutPhase phase,
                                BdlCutMode mode) {
    if (handle_ != nullptr) {
        auto& disk = context(handle_);
        const auto index = static_cast<std::size_t>(operation);
        disk.cutOperation = operation;
        disk.cutOccurrence = disk.occurrences[index] + 1U;
        disk.cutPhase = phase;
        disk.cutMode = mode;
    }
}

void TestRamDisk::clearCutPlan() {
    if (handle_ != nullptr) {
        context(handle_).cutPhase = BdlCutPhase::None;
        context(handle_).cutMode = BdlCutMode::ReturnError;
    }
}

void TestRamDisk::setLogicalKey(const char* key) {
    if (handle_ != nullptr) {
        auto& disk = context(handle_);
        disk.logicalKey = key == nullptr ? "" : key;
    }
}

void TestRamDisk::clearLogicalKey() {
    if (handle_ != nullptr) {
        context(handle_).logicalKey.clear();
    }
}

void TestRamDisk::setAbruptCutFiles(const char* imagePath,
                                    const char* metadataPath) {
    if (handle_ != nullptr) {
        auto& disk = context(handle_);
        disk.abruptImagePath = imagePath == nullptr ? "" : imagePath;
        disk.abruptMetadataPath = metadataPath == nullptr ? "" : metadataPath;
    }
}

void TestRamDisk::setMutationHeadFiles(const char* oldHeadPath,
                                       const char* newHeadPath) {
    if (handle_ != nullptr) {
        auto& disk = context(handle_);
        disk.oldHeadPath = oldHeadPath == nullptr ? "" : oldHeadPath;
        disk.newHeadPath = newHeadPath == nullptr ? "" : newHeadPath;
    }
}

void TestRamDisk::setMutationOldHead(const std::string& value) {
    if (handle_ != nullptr) context(handle_).oldHead = value;
}

void TestRamDisk::setMutationNewHead(const std::string& value) {
    if (handle_ != nullptr) context(handle_).newHead = value;
}

bool TestRamDisk::loadImage(const char* imagePath) {
    if (handle_ == nullptr || imagePath == nullptr) return false;
    auto& disk = context(handle_);
    std::ifstream image(imagePath, std::ios::binary);
    if (!image.good()) return false;
    image.read(reinterpret_cast<char*>(disk.bytes),
               static_cast<std::streamsize>(disk.totalSize));
    return image.good() ||
           image.gcount() == static_cast<std::streamsize>(disk.totalSize);
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
    return contextChecksum(context(handle_));
}
