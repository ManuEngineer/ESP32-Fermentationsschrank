#include "fermentation_ui_renderer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace fermentation::main_ui {
namespace {

constexpr std::uint16_t kHeaderHeight = 32U;
constexpr std::uint16_t kControlTop = 200U;
constexpr std::uint16_t kControlHeight = 40U;
constexpr char kLogoAssetPath[] = "assets/branding/manuengineer/ManuEngineer.svg";
constexpr char kLogoAssetSha256[] =
    "b35788628e5cbda7a82552d5b6969c34a813494392ef9545d69dae4489b1484b";

std::uint16_t rgb565(std::uint32_t rgb) noexcept {
    const auto red = static_cast<std::uint16_t>((rgb >> 16U) & 0xFFU);
    const auto green = static_cast<std::uint16_t>((rgb >> 8U) & 0xFFU);
    const auto blue = static_cast<std::uint16_t>(rgb & 0xFFU);
    return static_cast<std::uint16_t>(((red * 31U / 255U) << 11U) |
                                      ((green * 63U / 255U) << 5U) |
                                      (blue * 31U / 255U));
}

std::uint16_t tokenColor(device_platform::ThemeToken token) noexcept {
    using device_platform::ThemeToken;
    switch (token) {
        case ThemeToken::Canvas:
            return rgb565(0x1C1712U);
        case ThemeToken::Surface:
            return rgb565(0x2A231BU);
        case ThemeToken::PrimaryAction:
            return rgb565(0xD89A3BU);
        case ThemeToken::SecondaryAction:
            return rgb565(0x9A7C4EU);
        case ThemeToken::TextPrimary:
            return rgb565(0xFBF7EFU);
        case ThemeToken::TextSecondary:
            return rgb565(0xD9D2C7U);
        case ThemeToken::StatusInformation:
            return rgb565(0x72B9E8U);
        case ThemeToken::StatusWarning:
            return rgb565(0xE7AE57U);
        case ThemeToken::StatusError:
            return rgb565(0xE36B6BU);
        case ThemeToken::Overlay:
            return rgb565(0x080705U);
        case ThemeToken::OnCanvas:
        case ThemeToken::OnSurface:
        case ThemeToken::OnPrimaryAction:
        case ThemeToken::OnSecondaryAction:
        case ThemeToken::OnStatusInformation:
        case ThemeToken::OnStatusWarning:
        case ThemeToken::OnStatusError:
        case ThemeToken::OnOverlay:
            return rgb565(0x1C1712U);
    }
    return rgb565(0x1C1712U);
}

device_platform::TextLookupResult resolve(
    const std::vector<device_platform::TextPackManifest>& packs,
    const device_platform::LocaleId& locale,
    const device_platform::TextKey& key) {
    return device_platform::resolveText(packs, locale, key);
}

void addFill(std::vector<ScreenDrawCommand>& commands,
             device_platform::DisplayRect rect,
             device_platform::ThemeToken token) {
    commands.push_back(
        {ScreenDrawKind::Fill, rect, token, token, std::string{}, {}});
}

void addText(std::vector<ScreenDrawCommand>& commands,
             const std::vector<device_platform::TextPackManifest>& packs,
             const device_platform::LocaleId& locale,
             const device_platform::TextKey& key,
             device_platform::DisplayRect rect,
             device_platform::ThemeToken token,
             device_platform::ThemeToken background =
                 device_platform::ThemeToken::Canvas) {
    commands.push_back({ScreenDrawKind::Text, rect, token, background,
                        resolve(packs, locale, key).value, {}});
}

void addRawText(std::vector<ScreenDrawCommand>& commands,
                device_platform::DisplayRect rect, std::string text,
                device_platform::ThemeToken token,
                device_platform::ThemeToken background) {
    commands.push_back({ScreenDrawKind::Text, rect, token, background,
                        std::move(text), {}});
}

void addLogo(std::vector<ScreenDrawCommand>& commands,
             device_platform::DisplayRect rect) {
    commands.push_back({ScreenDrawKind::Logo, rect,
                        device_platform::ThemeToken::TextPrimary,
                        device_platform::ThemeToken::Surface, "ManuEngineer",
                        kLogoAssetPath});
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
    if (command.rect.width == 0U || command.rect.height == 0U) {
        summary.success = false;
        return;
    }
    std::vector<std::uint16_t> pixels(
        static_cast<std::size_t>(command.rect.width) * command.rect.height,
        themeColor565(command.backgroundToken));
    for (std::size_t character = 0U; character < maxChars; ++character) {
        for (std::size_t column = 0U; column < 5U; ++column) {
            const auto bits = glyphColumn(command.text[character], column);
            for (std::uint16_t row = 0U; row < kGlyphHeight; ++row) {
                if ((bits & (1U << row)) == 0U) continue;
                const auto x = character * kGlyphWidth + column;
                if (x >= command.rect.width || row >= command.rect.height) continue;
                pixels[static_cast<std::size_t>(row) * command.rect.width + x] =
                    themeColor565(command.token);
                ++summary.filledPixels;
            }
        }
    }
    if (!display.flushRgb565(command.rect, pixels.data(), pixels.size())) {
        summary.success = false;
        return;
    }
    ++summary.displaySubmissions;
}

}  // namespace

std::uint16_t themeColor565(device_platform::ThemeToken token) noexcept {
    return tokenColor(token);
}

RepresentativeScreen makeRepresentativeScreen(
    const FermentationUiSnapshot& snapshot, FermentationTouchWorkspace& workspace,
    const std::vector<device_platform::TextPackManifest>& textPacks,
    const device_platform::LocaleId& locale) {
    RepresentativeScreen screen;
    screen.locale = locale;
    screen.header.locale = locale;
    screen.header.branding = device_platform::BrandingId{"manuengineer"};
    screen.header.networkStatus =
        device_platform::DeviceUiNetworkStatus::Unavailable;
    screen.theme = {device_platform::ThemeId{"manuengineer-dark"},
                    {device_platform::ThemeToken::Canvas,
                     device_platform::ThemeToken::Surface,
                     device_platform::ThemeToken::PrimaryAction,
                     device_platform::ThemeToken::SecondaryAction,
                     device_platform::ThemeToken::TextPrimary,
                     device_platform::ThemeToken::TextSecondary,
                     device_platform::ThemeToken::StatusInformation,
                     device_platform::ThemeToken::StatusWarning,
                     device_platform::ThemeToken::StatusError,
                     device_platform::ThemeToken::Overlay,
                     device_platform::ThemeToken::OnCanvas,
                     device_platform::ThemeToken::OnSurface,
                     device_platform::ThemeToken::OnPrimaryAction,
                     device_platform::ThemeToken::OnSecondaryAction,
                     device_platform::ThemeToken::OnStatusInformation,
                     device_platform::ThemeToken::OnStatusWarning,
                     device_platform::ThemeToken::OnStatusError,
                     device_platform::ThemeToken::OnOverlay}};
    screen.logoAssetPath = kLogoAssetPath;
    screen.logoAssetSha256 = kLogoAssetSha256;
    screen.workspace = workspace.view(snapshot);
    auto& commands = screen.commands;
    addFill(commands, {0U, 0U, screen.kWidth, screen.kHeight},
            device_platform::ThemeToken::Canvas);
    addFill(commands, {0U, 0U, screen.kWidth, kHeaderHeight},
            device_platform::ThemeToken::Surface);
    addLogo(commands, {4U, 4U, 168U, 24U});
    auto localeText = locale.value();
    std::transform(localeText.begin(), localeText.end(), localeText.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::toupper(value));
                   });
    addRawText(commands, {176U, 4U, 44U, 24U}, std::move(localeText),
               device_platform::ThemeToken::TextPrimary,
               device_platform::ThemeToken::Surface);
    addRawText(commands, {220U, 4U, 44U, 24U}, "WLAN",
               device_platform::ThemeToken::StatusInformation,
               device_platform::ThemeToken::Surface);
    addRawText(commands, {264U, 4U, 52U, 24U}, screen.clockText,
               device_platform::ThemeToken::TextSecondary,
               device_platform::ThemeToken::Surface);
    addText(commands, textPacks, locale, screen.workspace.title,
            {8U, 40U, 144U, 16U}, device_platform::ThemeToken::TextPrimary,
            device_platform::ThemeToken::Canvas);
    addText(commands, textPacks, locale, homeModeKey(snapshot.home.mode),
            {168U, 40U, 144U, 16U},
            device_platform::ThemeToken::StatusInformation,
            device_platform::ThemeToken::Canvas);

    const auto temperatureCount = std::min<std::size_t>(
        snapshot.temperatures.size(), 3U);
    for (std::size_t index = 0U; index < temperatureCount; ++index) {
        const auto left = static_cast<std::uint16_t>(8U + index * 104U);
        addFill(commands, {left, 68U, 96U, 48U},
                device_platform::ThemeToken::Surface);
        addText(commands, textPacks, locale, appKey("status"),
                {static_cast<std::uint16_t>(left + 4U), 72U, 88U, 10U},
                device_platform::ThemeToken::TextSecondary,
                device_platform::ThemeToken::Surface);
        commands.push_back({ScreenDrawKind::Text,
                            {static_cast<std::uint16_t>(left + 4U), 90U, 88U, 14U},
                            device_platform::ThemeToken::StatusInformation,
                            device_platform::ThemeToken::Surface,
                            temperatureText(snapshot.temperatures[index]), {}});
    }
    addText(commands, textPacks, locale, appKey("messages"),
            {8U, 128U, 88U, 14U}, device_platform::ThemeToken::StatusWarning,
            device_platform::ThemeToken::Canvas);
    addText(commands, textPacks, locale,
            snapshot.service.available ? appKey("service")
                                       : appKey("service-locked"),
            {112U, 128U, 96U, 14U},
            snapshot.service.available ? device_platform::ThemeToken::PrimaryAction
                                       : device_platform::ThemeToken::StatusWarning,
            device_platform::ThemeToken::Canvas);
    addText(commands, textPacks, locale, appKey("network"),
            {224U, 128U, 88U, 14U},
            device_platform::ThemeToken::StatusInformation,
            device_platform::ThemeToken::Canvas);

    if (screen.workspace.pager.valid()) {
        addRawText(commands, {8U, 156U, 72U, 14U},
                   std::to_string(screen.workspace.pager.currentIndex + 1U) +
                       "/" + std::to_string(screen.workspace.pager.itemCount),
                   device_platform::ThemeToken::TextSecondary,
                   device_platform::ThemeToken::Canvas);
    }
    if (screen.workspace.confirmationWarning.has_value()) {
        addFill(commands, {32U, 148U, 256U, 44U},
                device_platform::ThemeToken::Overlay);
        addText(commands, textPacks, locale,
                *screen.workspace.confirmationWarning, {40U, 154U, 240U, 14U},
                device_platform::ThemeToken::TextPrimary,
                device_platform::ThemeToken::Overlay);
        addRawText(commands, {40U, 174U, 224U, 12U}, "cancel  confirm",
                   device_platform::ThemeToken::PrimaryAction,
                   device_platform::ThemeToken::Overlay);
    }

    for (std::size_t index = 0U; index < screen.workspace.bottomSlots.size();
         ++index) {
        const auto left = static_cast<std::uint16_t>(index * 80U);
        const auto& slot = screen.workspace.bottomSlots[index];
        addFill(commands,
                {left, kControlTop, 80U, kControlHeight},
                slot.enabled ? device_platform::ThemeToken::PrimaryAction
                             : device_platform::ThemeToken::SecondaryAction);
        addText(commands, textPacks, locale, slot.label,
                {static_cast<std::uint16_t>(left + 4U),
                 static_cast<std::uint16_t>(kControlTop + 15U), 68U, 10U},
                slot.enabled ? device_platform::ThemeToken::OnPrimaryAction
                             : device_platform::ThemeToken::TextSecondary,
                slot.enabled ? device_platform::ThemeToken::PrimaryAction
                             : device_platform::ThemeToken::SecondaryAction);
    }
    // A bounded pressed-state command is part of the same #26 shell model;
    // later input plumbing toggles it from the existing DeviceUiTarget result.
    commands.push_back({ScreenDrawKind::PressFeedback, {0U, kControlTop, 80U,
                                                         kControlHeight},
                        device_platform::ThemeToken::SecondaryAction,
                        device_platform::ThemeToken::PrimaryAction, {}, {}});
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
        if (command.kind == ScreenDrawKind::Text ||
            command.kind == ScreenDrawKind::Logo) {
            ++summary.textCommands;
            summary.textBytes += command.text.size();
            renderText(display, command, summary);
        } else if (command.kind == ScreenDrawKind::Fill ||
                   command.kind == ScreenDrawKind::PressFeedback) {
            if (command.rect.width == 0U || command.rect.height == 0U ||
                !display.fillRect(command.rect,
                                 themeColor565(command.token))) {
                summary.success = false;
                return summary;
            }
            ++summary.displaySubmissions;
        } else {
            summary.success = false;
            return summary;
        }
        if (command.kind == ScreenDrawKind::Fill ||
            command.kind == ScreenDrawKind::PressFeedback) {
            summary.filledPixels +=
                static_cast<std::size_t>(command.rect.width) * command.rect.height;
        }
    }
    summary.success = true;
    summary.frameSubmitted = summary.displaySubmissions > 0U;
    summary.frameFullyFlushed = summary.frameSubmitted;
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
