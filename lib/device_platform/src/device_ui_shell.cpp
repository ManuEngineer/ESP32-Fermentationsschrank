#include "device_ui_shell.hpp"

#include <algorithm>

namespace device_platform {

LocalNavigationAction homeOrBackAction(const ShellRoute& route) noexcept {
    if (route.segments.empty()) return LocalNavigationAction::None;
    return route.segments.size() == 1U ? LocalNavigationAction::Home
                                       : LocalNavigationAction::Back;
}

bool applyLocalNavigation(ShellRoute& route,
                          LocalNavigationAction action) noexcept {
    if (route.exitRequirement != PageExitRequirement::None) return false;
    switch (action) {
        case LocalNavigationAction::None:
            return false;
        case LocalNavigationAction::Home:
            if (route.segments.empty()) return false;
            route.segments.clear();
            return true;
        case LocalNavigationAction::Back:
            if (route.segments.size() < 2U) return false;
            route.segments.pop_back();
            return true;
    }
    return false;
}

bool hasExactlyFourVisibleSlots(const LocalDeviceShellState& state) noexcept {
    return std::all_of(state.bottomSlots.begin(), state.bottomSlots.end(),
                       [](const BottomSlot& slot) { return slot.visible(); });
}

std::vector<UiSectionDescriptor> StaticUiExtensionCatalog::orderedSections()
    const {
    std::vector<UiSectionDescriptor> ordered = sections_;
    std::stable_sort(
        ordered.begin(), ordered.end(),
        [](const UiSectionDescriptor& left, const UiSectionDescriptor& right) {
            return left.owner == UiSectionOwner::Platform &&
                   right.owner == UiSectionOwner::Application;
        });
    return ordered;
}

}  // namespace device_platform
