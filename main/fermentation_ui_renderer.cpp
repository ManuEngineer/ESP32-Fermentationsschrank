#include "fermentation_ui_renderer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace fermentation::main_ui {
namespace {

constexpr std::uint16_t kCanvas = 0x0000U;
constexpr std::uint16_t kSurface = 0x2104U;
constexpr std::uint16_t kPrimary = 0x07E0U;
constexpr std::uint16_t kDisabled = 0x7BEFU;
constexpr std::uint16_t kText = 0xFFFFU;
constexpr std::uint16_t kInfo = 0x07FFU;
constexpr std::uint16_t kWarning = 0xFD20U;
constexpr std::uint16_t kControlTop = 196U;
constexpr std::uint16_t kControlHeight = 40U;

device_platform::TextLookupResult resolve(
    const std::vector<device_platform::TextPackManifest>& packs,
    const device_platform::LocaleId& locale,
    const device_platform::TextKey& key) {
    return device_platform::resolveText(packs, locale, key);
}

void addFill(std::vector<ScreenDrawCommand>& commands,
             device_platform::DisplayRect rect, std::uint16_t color) {
    commands.push_back(
        {ScreenDrawKind::Fill, rect, color, std::string{}});
}

void addText(std::vector<ScreenDrawCommand>& commands,
             const std::vector<device_platform::TextPackManifest>& packs,
             const device_platform::LocaleId& locale,
             const device_platform::TextKey& key,
             device_platform::DisplayRect rect, std::uint16_t color) {
    commands.push_back({ScreenDrawKind::Text, rect, color,
                        resolve(packs, locale, key).value});
}

device_platform::TextKey appKey(std::string_view value) {
    return fermentationTextKey(value.data());
}

device_platform::TextKey homeModeKey(FermentationHomeMode mode) {
    switch (mode) {
        case FermentationHomeMode::Standby:
            return appKey("standby");
        case FermentationHomeMode::ActiveRun:
            return appKey("running");
        case FermentationHomeMode::Waiting:
            return appKey("waiting");
        case FermentationHomeMode::Completed:
            return appKey("completed");
        case FermentationHomeMode::Restricted:
            return appKey("restricted");
        case FermentationHomeMode::Recovery:
            return appKey("recovery");
        case FermentationHomeMode::Unavailable:
            return appKey("unavailable");
    }
    return appKey("unavailable");
}

std::string temperatureText(const TemperatureView& temperature) {
    if (!temperature.valueCelsius.has_value()) return "--.- C";
    const auto scaled = static_cast<int>(temperature.valueCelsius.value() * 10.0);
    const auto absolute = scaled < 0 ? -scaled : scaled;
    std::string result;
    if (scaled < 0) result.push_back('-');
    result += std::to_string(absolute / 10);
    result.push_back('.');
    result.push_back(static_cast<char>('0' + absolute % 10));
    result += " C";
    return result;
}

std::uint8_t glyphColumn(char value, std::size_t column) noexcept {
    const char upper = value >= 'a' && value <= 'z'
                           ? static_cast<char>(value - ('a' - 'A'))
                           : value;
    constexpr std::array<std::array<std::uint8_t, 5U>, 44U> glyphs{{
        {{0x3EU, 0x09U, 0x09U, 0x3EU, 0x00U}},  // A
        {{0x3FU, 0x25U, 0x25U, 0x1AU, 0x00U}},  // B
        {{0x1EU, 0x21U, 0x21U, 0x12U, 0x00U}},  // C
        {{0x3FU, 0x21U, 0x21U, 0x1EU, 0x00U}},  // D
        {{0x3FU, 0x25U, 0x25U, 0x21U, 0x00U}},  // E
        {{0x3FU, 0x05U, 0x05U, 0x01U, 0x00U}},  // F
        {{0x1EU, 0x21U, 0x29U, 0x3AU, 0x00U}},  // G
        {{0x3FU, 0x04U, 0x04U, 0x3FU, 0x00U}},  // H
        {{0x21U, 0x3FU, 0x21U, 0x00U, 0x00U}},  // I
        {{0x10U, 0x20U, 0x21U, 0x1FU, 0x00U}},  // J
        {{0x3FU, 0x0CU, 0x12U, 0x21U, 0x00U}},  // K
        {{0x3FU, 0x20U, 0x20U, 0x20U, 0x00U}},  // L
        {{0x3FU, 0x02U, 0x04U, 0x02U, 0x3FU}},  // M
        {{0x3FU, 0x02U, 0x04U, 0x08U, 0x3FU}},  // N
        {{0x1EU, 0x21U, 0x21U, 0x1EU, 0x00U}},  // O
        {{0x3FU, 0x09U, 0x09U, 0x06U, 0x00U}},  // P
        {{0x1EU, 0x21U, 0x31U, 0x5EU, 0x00U}},  // Q
        {{0x3FU, 0x09U, 0x19U, 0x26U, 0x00U}},  // R
        {{0x26U, 0x25U, 0x25U, 0x19U, 0x00U}},  // S
        {{0x01U, 0x3FU, 0x01U, 0x01U, 0x00U}},  // T
        {{0x1FU, 0x20U, 0x20U, 0x1FU, 0x00U}},  // U
        {{0x0FU, 0x30U, 0x30U, 0x0FU, 0x00U}},  // V
        {{0x1FU, 0x20U, 0x18U, 0x20U, 0x1FU}},  // W
        {{0x33U, 0x0CU, 0x0CU, 0x33U, 0x00U}},  // X
        {{0x03U, 0x04U, 0x38U, 0x04U, 0x03U}},  // Y
        {{0x31U, 0x29U, 0x25U, 0x23U, 0x00U}},  // Z
        {{0x1EU, 0x21U, 0x29U, 0x1EU, 0x00U}},  // 0
        {{0x00U, 0x22U, 0x3FU, 0x20U, 0x00U}},  // 1
        {{0x32U, 0x29U, 0x29U, 0x26U, 0x00U}},  // 2
        {{0x12U, 0x21U, 0x25U, 0x1AU, 0x00U}},  // 3
        {{0x0CU, 0x0AU, 0x3FU, 0x08U, 0x00U}},  // 4
        {{0x17U, 0x25U, 0x25U, 0x19U, 0x00U}},  // 5
        {{0x1EU, 0x25U, 0x25U, 0x18U, 0x00U}},  // 6
        {{0x01U, 0x39U, 0x05U, 0x03U, 0x00U}},  // 7
        {{0x1AU, 0x25U, 0x25U, 0x1AU, 0x00U}},  // 8
        {{0x06U, 0x29U, 0x29U, 0x1EU, 0x00U}},  // 9
        {{0x00U, 0x00U, 0x24U, 0x00U, 0x00U}},  // .
        {{0x00U, 0x00U, 0x08U, 0x00U, 0x00U}},  // -
        {{0x00U, 0x00U, 0x00U, 0x00U, 0x00U}},  // space
        {{0x00U, 0x06U, 0x09U, 0x06U, 0x00U}},  // :
        {{0x00U, 0x00U, 0x04U, 0x00U, 0x00U}},  // _
        {{0x02U, 0x04U, 0x08U, 0x10U, 0x20U}},  // /
        {{0x00U, 0x00U, 0x3FU, 0x00U, 0x00U}},  // =
        {{0x00U, 0x00U, 0x00U, 0x00U, 0x00U}},  // ?
    }};
    std::size_t index = 41U;
    if (upper >= 'A' && upper <= 'Z') index = static_cast<std::size_t>(upper - 'A');
    else if (upper >= '0' && upper <= '9') index = 26U + static_cast<std::size_t>(upper - '0');
    else if (upper == '.') index = 36U;
    else if (upper == '-') index = 37U;
    else if (upper == ' ') index = 38U;
    else if (upper == ':') index = 39U;
    else if (upper == '_') index = 40U;
    else if (upper == '/') index = 41U;
    else if (upper == '=') index = 42U;
    return glyphs[index][column];
}

void renderText(device_platform::IDisplayTouchPort& display,
                const ScreenDrawCommand& command, LeanRenderSummary& summary) {
    constexpr std::uint16_t kGlyphWidth = 6U;
    constexpr std::uint16_t kGlyphHeight = 7U;
    const auto maxChars = std::min<std::size_t>(
        command.text.size(), command.rect.width / kGlyphWidth);
    for (std::size_t character = 0U; character < maxChars; ++character) {
        for (std::size_t column = 0U; column < 5U; ++column) {
            const auto bits = glyphColumn(command.text[character], column);
            for (std::uint16_t row = 0U; row < kGlyphHeight; ++row) {
                if ((bits & (1U << row)) == 0U) continue;
                const device_platform::DisplayRect pixel{
                    static_cast<std::uint16_t>(command.rect.left + character * kGlyphWidth + column),
                    static_cast<std::uint16_t>(command.rect.top + row), 1U, 1U};
                if (!display.fillRect(pixel, command.color565)) {
                    summary.success = false;
                    return;
                }
                ++summary.filledPixels;
            }
        }
    }
}

}  // namespace

RepresentativeScreen makeRepresentativeScreen(
    const FermentationUiSnapshot& snapshot, FermentationTouchWorkspace& workspace,
    const std::vector<device_platform::TextPackManifest>& textPacks,
    const device_platform::LocaleId& locale) {
    RepresentativeScreen screen;
    screen.locale = locale;
    screen.workspace = workspace.view(snapshot);
    auto& commands = screen.commands;
    addFill(commands, {0U, 0U, screen.kWidth, screen.kHeight}, kCanvas);
    addFill(commands, {0U, 0U, screen.kWidth, 36U}, kSurface);
    addText(commands, textPacks, locale, screen.workspace.title,
            {8U, 7U, 140U, 14U}, kText);
    addText(commands, textPacks, locale, homeModeKey(snapshot.home.mode),
            {168U, 7U, 140U, 14U}, kInfo);

    const auto temperatureCount = std::min<std::size_t>(
        snapshot.temperatures.size(), 3U);
    for (std::size_t index = 0U; index < temperatureCount; ++index) {
        const auto left = static_cast<std::uint16_t>(8U + index * 104U);
        addFill(commands, {left, 48U, 96U, 48U}, kSurface);
        addText(commands, textPacks, locale, appKey("status"),
                {static_cast<std::uint16_t>(left + 4U), 52U, 88U, 10U}, kText);
        commands.push_back({ScreenDrawKind::Text,
                            {static_cast<std::uint16_t>(left + 4U), 70U, 88U, 14U},
                            kInfo, temperatureText(snapshot.temperatures[index])});
    }
    addText(commands, textPacks, locale, appKey("messages"),
            {8U, 112U, 88U, 12U}, kWarning);
    addText(commands, textPacks, locale,
            snapshot.service.available ? appKey("service")
                                       : appKey("service-locked"),
            {112U, 112U, 96U, 12U}, snapshot.service.available ? kPrimary : kWarning);
    addText(commands, textPacks, locale, appKey("network"),
            {224U, 112U, 88U, 12U}, kInfo);

    for (std::size_t index = 0U; index < screen.workspace.bottomSlots.size();
         ++index) {
        const auto left = static_cast<std::uint16_t>(index * 80U);
        const auto& slot = screen.workspace.bottomSlots[index];
        addFill(commands,
                {left, kControlTop, 76U, kControlHeight},
                slot.enabled ? kPrimary : kDisabled);
        addText(commands, textPacks, locale, slot.label,
                {static_cast<std::uint16_t>(left + 4U),
                 static_cast<std::uint16_t>(kControlTop + 15U), 68U, 10U},
                slot.enabled ? kCanvas : kSurface);
    }
    return screen;
}

std::optional<device_platform::DeviceUiTarget> targetAt(
    const RepresentativeScreen& screen, std::uint16_t x,
    std::uint16_t y) noexcept {
    if (y < kControlTop || y >= kControlTop + kControlHeight ||
        x >= screen.kWidth) {
        return std::nullopt;
    }
    const auto index = static_cast<std::uint8_t>(x / 80U);
    if (index >= 4U || !screen.workspace.bottomSlots[index].visible()) {
        return std::nullopt;
    }
    return device_platform::DeviceUiTarget{
        device_platform::DeviceUiTargetKind::BottomSlot, index};
}

LeanRenderSummary renderLean(device_platform::IDisplayTouchPort& display,
                             const RepresentativeScreen& screen) {
    LeanRenderSummary summary;
    summary.drawCommands = screen.commands.size();
    for (const auto& command : screen.commands) {
        if (command.kind == ScreenDrawKind::Text) {
            ++summary.textCommands;
            summary.textBytes += command.text.size();
            renderText(display, command, summary);
        } else if (command.rect.width == 0U || command.rect.height == 0U ||
                   !display.fillRect(command.rect, command.color565)) {
            summary.success = false;
            return summary;
        }
        if (command.kind == ScreenDrawKind::Fill) {
            summary.filledPixels +=
                static_cast<std::size_t>(command.rect.width) * command.rect.height;
        }
    }
    summary.success = true;
    return summary;
}

FermentationUiWorkspacePress routePress(
    FermentationTouchWorkspace& workspace, const FermentationUiSnapshot& snapshot,
    const RepresentativeScreen& screen, std::uint16_t x, std::uint16_t y,
    const ProgramCatalog* catalog) {
    const auto target = targetAt(screen, x, y);
    if (!target.has_value()) return {};
    return workspace.press(snapshot, target.value(), catalog);
}

}  // namespace fermentation::main_ui
