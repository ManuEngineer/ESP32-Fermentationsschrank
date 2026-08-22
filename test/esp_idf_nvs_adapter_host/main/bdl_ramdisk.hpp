#pragma once

#include <cstddef>

#include "esp_blockdev.h"

class TestRamDisk final {
   public:
    TestRamDisk(std::size_t totalSize, std::size_t eraseSize);
    ~TestRamDisk();

    TestRamDisk(const TestRamDisk&) = delete;
    TestRamDisk& operator=(const TestRamDisk&) = delete;

    [[nodiscard]] esp_blockdev_handle_t handle() const { return handle_; }

   private:
    esp_blockdev_handle_t handle_{nullptr};
};
