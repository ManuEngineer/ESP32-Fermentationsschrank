#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <string>
#include <vector>

#include "esp_blockdev.h"

enum class BdlOperation : std::uint8_t { Read, Write, Erase, Sync };
enum class BdlCutPhase : std::uint8_t { None, Before, After };
enum class BdlCutMode : std::uint8_t { ReturnError, AbruptProcessExit };

struct BdlEvent {
    BdlOperation operation;
    std::uint64_t offset;
    std::size_t length;
    std::size_t occurrence;
    esp_err_t result;
    BdlCutPhase cutPhase;
    std::string logicalKey;
    std::uint32_t baselineChecksum;
};

class TestRamDisk final {
   public:
    TestRamDisk(std::size_t totalSize, std::size_t eraseSize);
    ~TestRamDisk();

    TestRamDisk(const TestRamDisk&) = delete;
    TestRamDisk& operator=(const TestRamDisk&) = delete;

    [[nodiscard]] esp_blockdev_handle_t handle() const { return handle_; }

    void clearEvents();
    void setCutPlan(BdlOperation operation, std::size_t occurrence,
                    BdlCutPhase phase,
                    BdlCutMode mode = BdlCutMode::ReturnError);
    void armCutForNext(BdlOperation operation, BdlCutPhase phase,
                       BdlCutMode mode = BdlCutMode::ReturnError);
    void clearCutPlan();
    void setLogicalKey(const char* key);
    void clearLogicalKey();
    void setAbruptCutFiles(const char* imagePath, const char* metadataPath);
    void setMutationHeadFiles(const char* oldHeadPath, const char* newHeadPath);
    void setMutationOldHead(const std::string& value);
    void setMutationNewHead(const std::string& value);
    [[nodiscard]] bool loadImage(const char* imagePath);
    [[nodiscard]] std::vector<BdlEvent> events() const;
    [[nodiscard]] std::uint32_t checksum() const;

   private:
    esp_blockdev_handle_t handle_{nullptr};
};
