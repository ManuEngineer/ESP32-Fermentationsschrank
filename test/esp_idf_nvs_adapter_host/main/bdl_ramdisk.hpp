#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>

#include "esp_blockdev.h"

enum class BdlOperation : std::uint8_t { Read, Write, Erase, Sync };
enum class BdlCutPhase : std::uint8_t { None, Before, After };

struct BdlEvent {
    BdlOperation operation;
    std::uint64_t offset;
    std::size_t length;
    std::size_t occurrence;
    esp_err_t result;
    BdlCutPhase cutPhase;
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
                    BdlCutPhase phase);
    void clearCutPlan();
    [[nodiscard]] std::vector<BdlEvent> events() const;
    [[nodiscard]] std::uint32_t checksum() const;

   private:
    esp_blockdev_handle_t handle_{nullptr};
};
