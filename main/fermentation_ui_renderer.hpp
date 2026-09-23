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
    std::vector<ScreenDrawCommand> commands;
};

[[nodiscard]] std::uint16_t themeColor565(device_platform::ThemeToken token) noexcept;

[[nodiscard]] RepresentativeScreen makeRepresentativeScreen(
    const FermentationUiSnapshot& snapshot, FermentationTouchWorkspace& workspace,
    const std::vector<device_platform::TextPackManifest>& textPacks,
    const device_platform::LocaleId& locale);

[[nodiscard]] std::optional<device_platform::DeviceUiTarget>
targetAt(const RepresentativeScreen& screen, std::uint16_t x,
         std::uint16_t y) noexcept;

[[nodiscard]] FermentationUiWorkspacePress routePress(
    FermentationTouchWorkspace& workspace, const FermentationUiSnapshot& snapshot,
    const RepresentativeScreen& screen, std::uint16_t x, std::uint16_t y,
    const ProgramCatalog* catalog = nullptr);

}  // namespace fermentation::main_ui
