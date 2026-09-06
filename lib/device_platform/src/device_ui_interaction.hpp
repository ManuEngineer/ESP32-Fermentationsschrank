#pragma once

#include <cstddef>
#include <cstdint>

#include "device_ui_shell.hpp"

namespace device_platform {

enum class DeviceUiTargetKind : std::uint8_t {
    None,
    HeaderLanguage,
    HeaderNetwork,
    HeaderClock,
    BottomSlot,
    HomeOrBack,
    PagerUp,
    PagerDown,
    Confirm,
    Cancel,
    Back,
};

struct DeviceUiTarget {
    DeviceUiTargetKind kind{DeviceUiTargetKind::None};
    std::uint8_t slotIndex{0U};

    [[nodiscard]] bool valid() const noexcept {
        return kind != DeviceUiTargetKind::None &&
               (kind != DeviceUiTargetKind::BottomSlot || slotIndex < 4U);
    }
};

enum class DeviceUiInteractionOutcome : std::uint8_t {
    Ignored,
    WakeOnly,
    TargetSelected,
    Blocked,
};

enum class DeviceUiFeedbackIntent : std::uint8_t {
    None,
    TouchPress,
    WakeOnly,
    ActionAccepted,
    ConfirmationRequired,
    ActionRejected,
    Warning,
    Critical,
    Completion,
};

struct DeviceUiInteractionInput {
    DeviceUiTarget target;
    bool targetEnabled{false};
    bool dimmed{false};
    bool sleeping{false};
    PageExitRequirement exitRequirement{PageExitRequirement::None};
};

struct DeviceUiInteractionResult {
    DeviceUiInteractionOutcome outcome{DeviceUiInteractionOutcome::Ignored};
    DeviceUiTarget target;
    DeviceUiFeedbackIntent feedback{DeviceUiFeedbackIntent::None};
    bool visiblePressFeedback{false};
};

[[nodiscard]] DeviceUiInteractionResult selectDeviceUiTarget(
    const DeviceUiInteractionInput& input) noexcept;

struct VerticalPager {
    std::size_t itemCount{0U};
    std::size_t currentIndex{0U};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool canMoveUp() const noexcept;
    [[nodiscard]] bool canMoveDown() const noexcept;
    [[nodiscard]] bool moveUp() noexcept;
    [[nodiscard]] bool moveDown() noexcept;
};

}  // namespace device_platform
