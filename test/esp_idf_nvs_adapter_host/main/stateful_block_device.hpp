#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "esp_blockdev.h"

namespace issue90_host {

enum class BlockOperation : std::uint8_t {
    Read,
    Write,
    Erase,
    Sync,
    Ioctl,
};

struct BlockTraceEvent {
    BlockOperation operation;
    std::uint64_t address;
    std::size_t length;
    std::size_t sequence;
};

class StatefulBlockDevice final {
   public:
    StatefulBlockDevice(std::size_t size, std::size_t eraseSize);
    ~StatefulBlockDevice();

    StatefulBlockDevice(const StatefulBlockDevice&) = delete;
    StatefulBlockDevice& operator=(const StatefulBlockDevice&) = delete;

    [[nodiscard]] esp_blockdev_handle_t handle() const { return handle_; }
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const {
        return *bytes_;
    }
    [[nodiscard]] const std::vector<BlockTraceEvent>& trace() const {
        return *trace_;
    }

    void clearTrace();
    void failFromCallback(std::size_t callbackNumber);
    void clearFailure();

   private:
    esp_blockdev_handle_t handle_{nullptr};
    std::vector<std::uint8_t>* bytes_{nullptr};
    std::vector<BlockTraceEvent>* trace_{nullptr};
    std::optional<std::size_t> failFrom_;
};

}  // namespace issue90_host
