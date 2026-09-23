#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "device_ui_hardware_ports.hpp"
#include "device_ui_interaction.hpp"
#include "device_ui_shell.hpp"
#include "device_ui_theme.hpp"
#include "device_ui_text.hpp"
#include "fermentation_touch_workspace.hpp"
#include "fermentation_ui_text.hpp"

namespace fermentation::main_ui {

enum class ScreenDrawKind : std::uint8_t {
    Fill,
    Text,
    Logo,
    PressFeedback,
};

struct ScreenDrawCommand {
    ScreenDrawKind kind{ScreenDrawKind::Fill};
    device_platform::DisplayRect rect;
    device_platform::ThemeToken token{device_platform::ThemeToken::Canvas};
    device_platform::ThemeToken backgroundToken{
        device_platform::ThemeToken::Canvas};
    std::string text;
    std::string assetPath;
};

struct RepresentativeScreen {
    static constexpr std::uint16_t kWidth = 320U;
    static constexpr std::uint16_t kHeight = 240U;

    device_platform::LocaleId locale{"en"};
    device_platform::DeviceShellHeader header{
        device_platform::BrandingId{"manuengineer"},
        device_platform::LocaleId{"en"},
        device_platform::DeviceUiNetworkStatus::Unavailable,
        {}};
    device_platform::ThemeDescriptor theme{
        device_platform::ThemeId{"manuengineer-dark"}, {}};
    std::string clockText{"--:--"};
    std::string logoAssetPath{"assets/branding/manuengineer/ManuEngineer.svg"};
    FermentationUiWorkspaceView workspace;
    std::optional<device_platform::UiRefreshRevision> refreshRevision;
    std::optional<device_platform::DeviceUiTarget> pressedTarget;
    std::vector<ScreenDrawCommand> commands;
};

[[nodiscard]] std::uint16_t themeColor565(device_platform::ThemeToken token) noexcept;

// pressedTarget reflects only the existing #26 interaction result
// (DeviceUiInteractionResult::visiblePressFeedback) for the currently held
// touch contact, supplied by the caller. It is not a second renderer-owned
// press state: with no touch held (the default and, until persisted touch
// calibration is available, the only reachable production value), no
// PressFeedback command is drawn.
[[nodiscard]] RepresentativeScreen makeRepresentativeScreen(
    const FermentationUiSnapshot& snapshot, FermentationTouchWorkspace& workspace,
    const std::vector<device_platform::TextPackManifest>& textPacks,
    const device_platform::LocaleId& locale,
    std::optional<device_platform::DeviceUiTarget> pressedTarget = std::nullopt,
    const ProgramCatalog* catalog = nullptr);

// Identifies every semantic input the concrete renderer must redraw for, so
// it stays independent of the application's UiRefreshRevision. The workspace
// carries local navigation/pager/dialog state that can change without a new
// application snapshot; locale and header values can change independently of
// both. Only genuinely visible inputs participate; the drawn command list
// itself is derived output, not part of the key.
struct ScreenRenderKey {
    std::optional<device_platform::UiRefreshRevision> refreshRevision;
    device_platform::LocaleId locale;
    FermentationUiPage page;
    std::uint32_t pagerCurrentIndex{0U};
    std::uint32_t pagerItemCount{0U};
    bool hasConfirmationWarning{false};
    std::string confirmationProgramName;
    bool completionLocked{false};
    std::optional<device_platform::TextKey> blockedReason;
    std::size_t unavailableCapabilityCount{0U};
    std::size_t programListSize{0U};
    std::optional<std::uint8_t> pressedBottomSlotIndex;
    device_platform::DeviceUiNetworkStatus networkStatus{
        device_platform::DeviceUiNetworkStatus::Unavailable};
    std::optional<std::int64_t> trustedUtc;
    device_platform::ThemeId themeId;

    friend bool operator==(const ScreenRenderKey& left,
                           const ScreenRenderKey& right) noexcept;
    friend bool operator!=(const ScreenRenderKey& left,
                           const ScreenRenderKey& right) noexcept {
        return !(left == right);
    }
};

[[nodiscard]] ScreenRenderKey makeScreenRenderKey(
    const RepresentativeScreen& screen) noexcept;

[[nodiscard]] std::optional<device_platform::DeviceUiTarget>
targetAt(const RepresentativeScreen& screen, std::uint16_t x,
         std::uint16_t y) noexcept;

[[nodiscard]] FermentationUiWorkspacePress routePress(
    FermentationTouchWorkspace& workspace, const FermentationUiSnapshot& snapshot,
    const RepresentativeScreen& screen, std::uint16_t x, std::uint16_t y,
    const ProgramCatalog* catalog = nullptr);

}  // namespace fermentation::main_ui
