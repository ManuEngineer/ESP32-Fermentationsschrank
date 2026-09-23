#pragma once

#include <cstddef>
#include <cstdint>

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

enum class RawTouchSampleStatus : std::uint8_t {
    ControllerError,
    NoContact,
    Contact,
};

// The adapter reports controller-native samples. Calibration, transform and
// application meaning remain outside this port. monotonicTimeUs is supplied
// by the platform adapter, never by a renderer or application clock.
struct RawTouchSample {
    RawTouchSampleStatus status{RawTouchSampleStatus::ControllerError};
    std::uint16_t rawX{0U};
    std::uint16_t rawY{0U};
    std::uint16_t strength{0U};
    std::uint64_t monotonicTimeUs{0U};
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
    // The source buffer is consumed before this call returns. The adapter is
    // responsible for any DMA completion synchronization behind this port.
    [[nodiscard]] virtual bool flushRgb565(DisplayRect rect,
                                           const std::uint16_t* pixels,
                                           std::size_t pixelCount) = 0;
    [[nodiscard]] virtual RawTouchSample sampleTouch() = 0;
};

}  // namespace device_platform
