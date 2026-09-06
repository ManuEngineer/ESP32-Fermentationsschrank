#include "simulated_device_shell.hpp"

namespace device_platform_test_support {

bool SimulatedDeviceShellFrame::valid() const noexcept {
    const auto within = [](const SimulationRect& rect, std::uint16_t width,
                           std::uint16_t height) {
        return rect.x <= width && rect.y <= height &&
               static_cast<std::uint32_t>(rect.x) + rect.width <= width &&
               static_cast<std::uint32_t>(rect.y) + rect.height <= height;
    };
    const auto overlaps = [](const SimulationRect& left,
                             const SimulationRect& right) {
        return left.x < right.x + right.width &&
               right.x < left.x + left.width &&
               left.y < right.y + right.height &&
               right.y < left.y + left.height;
    };
    if (header.width != kWidth || content.width != kWidth ||
        footer.width != kWidth || header.x != 0U || header.y != 0U ||
        content.x != 0U || content.y != header.height || footer.x != 0U ||
        footer.y != header.height + content.height ||
        header.height + content.height + footer.height != kHeight ||
        !within(header, kWidth, kHeight) || !within(content, kWidth, kHeight) ||
        !within(footer, kWidth, kHeight) ||
        !within(branding, kWidth, kHeight) ||
        !within(splash, kWidth, kHeight) ||
        !within(headerLanguage, kWidth, kHeight) ||
        !within(headerNetwork, kWidth, kHeight) ||
        !within(headerClock, kWidth, kHeight) ||
        !within(pagerUp, kWidth, kHeight) ||
        !within(pagerDown, kWidth, kHeight) || overlaps(pagerUp, pagerDown) ||
        overlaps(branding, headerLanguage) ||
        overlaps(branding, headerNetwork) || overlaps(branding, headerClock) ||
        overlaps(splash, header) || overlaps(splash, footer) ||
        overlaps(headerLanguage, headerNetwork) ||
        overlaps(headerNetwork, headerClock)) {
        return false;
    }
    for (std::size_t index = 0U; index < bottomSlots.size(); ++index) {
        const auto& current = bottomSlots[index];
        if (!within(current, kWidth, kHeight) || current.y != footer.y ||
            current.height != footer.height || current.width != 80U ||
            current.x != index * current.width || overlaps(current, header) ||
            overlaps(current, content)) {
            return false;
        }
        for (std::size_t other = 0U; other < index; ++other) {
            if (overlaps(current, bottomSlots[other])) return false;
        }
    }
    return pagerUp.x >= content.x && pagerDown.x >= content.x &&
           !overlaps(pagerUp, header) && !overlaps(pagerDown, header) &&
           !overlaps(pagerUp, footer) && !overlaps(pagerDown, footer);
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
