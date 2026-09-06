#include "device_ui_interaction.hpp"

namespace device_platform {

namespace {

bool isNavigationTarget(DeviceUiTargetKind kind) noexcept {
    return kind == DeviceUiTargetKind::HomeOrBack ||
           kind == DeviceUiTargetKind::Back ||
           kind == DeviceUiTargetKind::Cancel;
}

}  // namespace

DeviceUiInteractionResult selectDeviceUiTarget(
    const DeviceUiInteractionInput& input) noexcept {
    DeviceUiInteractionResult result;
    result.target = input.target;
    if (input.dimmed || input.sleeping) {
        result.outcome = DeviceUiInteractionOutcome::WakeOnly;
        result.feedback = DeviceUiFeedbackIntent::WakeOnly;
        return result;
    }
    if (!input.target.valid()) return result;
    // Exit requirements guard only an actual page exit. Completion and
    // discard pages must still accept their explicitly allowed confirmation
    // or action targets on the current page.
    if (input.exitRequirement != PageExitRequirement::None &&
        isNavigationTarget(input.target.kind)) {
        result.outcome = DeviceUiInteractionOutcome::Blocked;
        result.feedback = DeviceUiFeedbackIntent::ConfirmationRequired;
        result.visiblePressFeedback = true;
        return result;
    }
    if (!input.targetEnabled) {
        result.outcome = DeviceUiInteractionOutcome::Blocked;
        result.feedback = DeviceUiFeedbackIntent::ActionRejected;
        result.visiblePressFeedback = true;
        return result;
    }
    result.outcome = DeviceUiInteractionOutcome::TargetSelected;
    result.feedback = DeviceUiFeedbackIntent::TouchPress;
    result.visiblePressFeedback = true;
    return result;
}

bool VerticalPager::valid() const noexcept {
    return itemCount == 0U ? currentIndex == 0U : currentIndex < itemCount;
}

bool VerticalPager::canMoveUp() const noexcept {
    return valid() && currentIndex > 0U;
}

bool VerticalPager::canMoveDown() const noexcept {
    return valid() && itemCount > 0U && currentIndex + 1U < itemCount;
}

bool VerticalPager::moveUp() noexcept {
    if (!canMoveUp()) return false;
    --currentIndex;
    return true;
}

bool VerticalPager::moveDown() noexcept {
    if (!canMoveDown()) return false;
    ++currentIndex;
    return true;
}

}  // namespace device_platform
