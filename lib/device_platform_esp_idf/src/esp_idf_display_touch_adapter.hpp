#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "device_ui_hardware_ports.hpp"

namespace device_platform_esp_idf {

struct DisplayRotationTransform final {
    bool swap_xy{false};
    bool mirror_x{false};
    bool mirror_y{false};
};

[[nodiscard]] constexpr DisplayRotationTransform displayRotationTransform(
    device_platform::DisplayRotation rotation) noexcept {
    switch (rotation) {
        case device_platform::DisplayRotation::Rotate0:
            return {false, false, false};
        case device_platform::DisplayRotation::Rotate90:
            // The R1 panel needs the landscape axis swap and a physical
            // 180-degree correction. Both mirror axes are required; using
            // only mirror_x leaves the visible product image upside down.
            return {true, true, true};
        case device_platform::DisplayRotation::Rotate180:
            return {false, true, true};
        case device_platform::DisplayRotation::Rotate270:
            return {true, false, true};
    }
    return {false, false, false};
}

class EspIdfDisplayTouchAdapter;

namespace detail {
struct EspIdfDisplayTouchHandles;
[[nodiscard]] bool bindEspIdfDisplayTouchHandles(
    const EspIdfDisplayTouchAdapter& adapter,
    EspIdfDisplayTouchHandles& handles) noexcept;
}  // namespace detail

// Pin numbers are deliberately plain integers at this public boundary.  The
// ESP-IDF GPIO/SPI/LCD types stay in the implementation file.
struct EspIdfDisplayTouchConfig {
    int sclkPin{-1};
    int mosiPin{-1};
    int misoPin{-1};
    int displayChipSelectPin{-1};
    int touchChipSelectPin{-1};
    int dataCommandPin{-1};
    int backlightPin{-1};
    int touchInterruptPin{-1};
    std::uint16_t width{320U};
    std::uint16_t height{240U};
    device_platform::DisplayRotation rotation{
        device_platform::DisplayRotation::Rotate0};
    bool backlightActiveHigh{true};
};

class EspIdfDisplayTouchAdapter final
    : public device_platform::IDisplayTouchPort {
   public:
    explicit EspIdfDisplayTouchAdapter(EspIdfDisplayTouchConfig config);
    ~EspIdfDisplayTouchAdapter() override;

    EspIdfDisplayTouchAdapter(const EspIdfDisplayTouchAdapter&) = delete;
    EspIdfDisplayTouchAdapter& operator=(const EspIdfDisplayTouchAdapter&) =
        delete;
    EspIdfDisplayTouchAdapter(EspIdfDisplayTouchAdapter&&) = delete;
    EspIdfDisplayTouchAdapter& operator=(EspIdfDisplayTouchAdapter&&) = delete;

    [[nodiscard]] bool initialize() override;
    [[nodiscard]] bool setRotation(
        device_platform::DisplayRotation rotation) override;
    [[nodiscard]] bool setBacklight(bool enabled) override;
    [[nodiscard]] bool fillRect(device_platform::DisplayRect rect,
                                std::uint16_t rgb565) override;
    [[nodiscard]] bool flushRgb565(device_platform::DisplayRect rect,
                                   const std::uint16_t* pixels,
                                   std::size_t pixelCount) override;
    [[nodiscard]] device_platform::RawTouchSample sampleTouch() override;

   private:
    friend bool detail::bindEspIdfDisplayTouchHandles(
        const EspIdfDisplayTouchAdapter& adapter,
        detail::EspIdfDisplayTouchHandles& handles) noexcept;
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace device_platform_esp_idf
