#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "device_ui_hardware_ports.hpp"
#include "device_ui_interaction.hpp"
#include "device_ui_text.hpp"
#include "fermentation_touch_workspace.hpp"
#include "fermentation_ui_text.hpp"

namespace fermentation::main_ui {

enum class ScreenDrawKind : std::uint8_t {
    Fill,
    Text,
};

struct ScreenDrawCommand {
    ScreenDrawKind kind{ScreenDrawKind::Fill};
    device_platform::DisplayRect rect;
    std::uint16_t color565{0U};
    std::string text;
};

struct RepresentativeScreen {
    static constexpr std::uint16_t kWidth = 320U;
    static constexpr std::uint16_t kHeight = 240U;

    device_platform::LocaleId locale{"en"};
    FermentationUiWorkspaceView workspace;
    std::vector<ScreenDrawCommand> commands;
};

struct LeanRenderSummary {
    bool success{false};
    std::size_t drawCommands{0U};
    std::size_t textCommands{0U};
    std::size_t textBytes{0U};
    std::size_t filledPixels{0U};
};

struct LvglRenderSummary {
    bool success{false};
    std::size_t drawCommands{0U};
    std::size_t textCommands{0U};
    std::size_t textBytes{0U};
    std::size_t partialBufferPixels{0U};
    std::size_t taskStackBytes{0U};
};

[[nodiscard]] RepresentativeScreen makeRepresentativeScreen(
    const FermentationUiSnapshot& snapshot, FermentationTouchWorkspace& workspace,
    const std::vector<device_platform::TextPackManifest>& textPacks,
    const device_platform::LocaleId& locale);

[[nodiscard]] std::optional<device_platform::DeviceUiTarget>
targetAt(const RepresentativeScreen& screen, std::uint16_t x,
         std::uint16_t y) noexcept;

// This is the deliberately small immediate-mode comparison renderer.  It
// consumes the same command list that the later LVGL comparison consumes and
// returns input only as a DeviceUiTarget; application command ownership stays
// in FermentationTouchWorkspace/FermentationApplication.
[[nodiscard]] LeanRenderSummary renderLean(
    device_platform::IDisplayTouchPort& display,
    const RepresentativeScreen& screen);

[[nodiscard]] FermentationUiWorkspacePress routePress(
    FermentationTouchWorkspace& workspace, const FermentationUiSnapshot& snapshot,
    const RepresentativeScreen& screen, std::uint16_t x, std::uint16_t y,
    const ProgramCatalog* catalog = nullptr);

}  // namespace fermentation::main_ui
