#pragma once

#include <cstdint>
#include <optional>

namespace device_platform {

// Renderer-independent geometry for a bounded display transfer.
struct DisplayRect {
    std::uint16_t left{0U};
    std::uint16_t top{0U};
    std::uint16_t width{0U};
    std::uint16_t height{0U};
};

enum class DisplayRotation : std::uint8_t {
    Rotate0,
    Rotate90,
    Rotate180,
    Rotate270,
};

// The adapter reports controller-native samples.  Calibration, transform and
// application meaning remain outside this port.
struct RawTouchSample {
    std::uint16_t rawX{0U};
    std::uint16_t rawY{0U};
    std::uint16_t strength{0U};
    bool contact{false};
};

class IDisplayTouchPort {
   public:
    IDisplayTouchPort() = default;
    virtual ~IDisplayTouchPort() = default;

    IDisplayTouchPort(const IDisplayTouchPort&) = delete;
    IDisplayTouchPort& operator=(const IDisplayTouchPort&) = delete;
    IDisplayTouchPort(IDisplayTouchPort&&) = delete;
    IDisplayTouchPort& operator=(IDisplayTouchPort&&) = delete;

    [[nodiscard]] virtual bool initialize() = 0;
    [[nodiscard]] virtual bool setRotation(DisplayRotation rotation) = 0;
    [[nodiscard]] virtual bool setBacklight(bool enabled) = 0;
    [[nodiscard]] virtual bool fillRect(DisplayRect rect,
                                        std::uint16_t rgb565) = 0;
    [[nodiscard]] virtual std::optional<RawTouchSample> sampleTouch() = 0;
};

}  // namespace device_platform
