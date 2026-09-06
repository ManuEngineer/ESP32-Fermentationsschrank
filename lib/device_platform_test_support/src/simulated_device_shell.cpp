#include "simulated_device_shell.hpp"

namespace device_platform_test_support {

bool SimulatedDeviceShellFrame::valid() const noexcept {
    return header.width == kWidth && content.width == kWidth &&
           footer.width == kWidth &&
           header.height + content.height + footer.height == kHeight &&
           pagerUp.x + pagerUp.width <= kWidth &&
           pagerDown.x + pagerDown.width <= kWidth;
}

device_platform::DeviceUiInteractionResult SimulatedDeviceShell::press(
    const device_platform::DeviceUiTarget& target, bool targetEnabled,
    bool dimmed, bool sleeping,
    device_platform::PageExitRequirement exitRequirement) {
    const auto result = device_platform::selectDeviceUiTarget(
        {target, targetEnabled, dimmed, sleeping, exitRequirement});
    trace_.push_back(
        {result.outcome == device_platform::DeviceUiInteractionOutcome::WakeOnly
             ? SimulationTraceKind::WakeOnly
             : SimulationTraceKind::Press,
         target, result.feedback});
    if (result.feedback != device_platform::DeviceUiFeedbackIntent::None) {
        trace_.push_back(
            {SimulationTraceKind::Feedback, target, result.feedback});
    }
    return result;
}

}  // namespace device_platform_test_support
