#pragma once

#include <cstdint>

#include "esp_idf_display_touch_adapter.hpp"

namespace device_platform_esp_idf::detail {

// This header is private to the Issue-31 comparison composition. It exposes
// no ESP-IDF types and is not part of the portable display/touch port.
using ComparisonDisplayTransferObserver = bool (*)(
    void* context, std::uint64_t completionTimestampUs) noexcept;

struct ComparisonDisplayTransferAccess {
    [[nodiscard]] static bool installObserver(
        EspIdfDisplayTouchAdapter& adapter,
        ComparisonDisplayTransferObserver observer, void* context) noexcept;
    static void clearObserver(EspIdfDisplayTouchAdapter& adapter) noexcept;
    static void resetFrameMetrics(EspIdfDisplayTouchAdapter& adapter) noexcept;
    [[nodiscard]] static std::uint64_t firstSubmitTimestampUs(
        const EspIdfDisplayTouchAdapter& adapter) noexcept;
    [[nodiscard]] static std::uint64_t lastCompleteTimestampUs(
        const EspIdfDisplayTouchAdapter& adapter) noexcept;
    [[nodiscard]] static bool frameTransferCompleted(
        const EspIdfDisplayTouchAdapter& adapter) noexcept;
};

}  // namespace device_platform_esp_idf::detail
