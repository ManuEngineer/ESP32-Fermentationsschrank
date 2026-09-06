#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "device_ui_interaction.hpp"

namespace device_platform_test_support {

struct SimulationRect {
    std::uint16_t x{0U};
    std::uint16_t y{0U};
    std::uint16_t width{0U};
    std::uint16_t height{0U};
};

struct SimulatedDeviceShellFrame {
    static constexpr std::uint16_t kWidth = 320U;
    static constexpr std::uint16_t kHeight = 240U;
    SimulationRect header{0U, 0U, kWidth, 32U};
    SimulationRect content{0U, 32U, kWidth, 168U};
    SimulationRect footer{0U, 200U, kWidth, 40U};
    SimulationRect pagerUp{288U, 48U, 32U, 48U};
    SimulationRect pagerDown{288U, 104U, 32U, 48U};

    [[nodiscard]] bool valid() const noexcept;
};

enum class SimulationTraceKind : std::uint8_t {
    WakeOnly,
    Press,
    Feedback,
};

struct SimulationTraceEvent {
    SimulationTraceKind kind{SimulationTraceKind::Feedback};
    device_platform::DeviceUiTarget target;
    device_platform::DeviceUiFeedbackIntent feedback{
        device_platform::DeviceUiFeedbackIntent::None};
};

class SimulatedDeviceShell {
   public:
    [[nodiscard]] SimulatedDeviceShellFrame frame() const noexcept {
        return frame_;
    }
    [[nodiscard]] const std::vector<SimulationTraceEvent>& trace()
        const noexcept {
        return trace_;
    }
    void clearTrace() noexcept { trace_.clear(); }

    [[nodiscard]] device_platform::DeviceUiInteractionResult press(
        const device_platform::DeviceUiTarget& target, bool targetEnabled,
        bool dimmed = false, bool sleeping = false,
        device_platform::PageExitRequirement exitRequirement =
            device_platform::PageExitRequirement::None);

   private:
    SimulatedDeviceShellFrame frame_;
    std::vector<SimulationTraceEvent> trace_;
};

}  // namespace device_platform_test_support
