#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "device_ui_contracts.hpp"

namespace device_platform {

enum class DeviceUiNetworkStatus : std::uint8_t {
    Connected,
    Disconnected,
    Unavailable,
};

struct DeviceShellHeader {
    BrandingId branding;
    LocaleId locale;
    DeviceUiNetworkStatus networkStatus{DeviceUiNetworkStatus::Unavailable};
    ClockViewInput clock;
};

enum class BottomSlotKind : std::uint8_t {
    Empty,
    Action,
};

struct BottomSlot {
    BottomSlotKind kind{BottomSlotKind::Empty};
    TextKey label;

    [[nodiscard]] bool visible() const noexcept { return true; }
};

enum class PageExitRequirement : std::uint8_t {
    None,
    ConfirmDiscard,
    CompletionLocked,
};

struct ShellRoute {
    std::vector<TextKey> segments;
    PageExitRequirement exitRequirement{PageExitRequirement::None};
};

enum class LocalNavigationAction : std::uint8_t {
    None,
    Home,
    Back,
};

struct LocalDeviceShellState {
    DeviceShellHeader header;
    std::array<BottomSlot, 4U> bottomSlots{};
    ShellRoute route;
};

[[nodiscard]] LocalNavigationAction homeOrBackAction(
    const ShellRoute& route) noexcept;
[[nodiscard]] bool applyLocalNavigation(ShellRoute& route,
                                        LocalNavigationAction action) noexcept;
[[nodiscard]] bool hasExactlyFourVisibleSlots(
    const LocalDeviceShellState& state) noexcept;

enum class UiSectionOwner : std::uint8_t {
    Platform,
    Application,
};

enum class UiSectionAvailability : std::uint8_t {
    Available,
    Unavailable,
};

struct UiSectionDescriptor {
    UiSectionOwner owner{UiSectionOwner::Platform};
    TextKey id;
    UiSectionAvailability availability{UiSectionAvailability::Available};
    TextKey unavailableReason;
};

class StaticUiExtensionCatalog {
   public:
    explicit StaticUiExtensionCatalog(std::vector<UiSectionDescriptor> sections)
        : sections_(std::move(sections)) {}

    [[nodiscard]] std::vector<UiSectionDescriptor> orderedSections() const;

   private:
    std::vector<UiSectionDescriptor> sections_;
};

}  // namespace device_platform
