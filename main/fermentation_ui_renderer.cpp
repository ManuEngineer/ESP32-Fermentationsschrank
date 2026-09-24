#include "fermentation_ui_renderer.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string_view>
#include <utility>

namespace fermentation::main_ui {
namespace {

constexpr std::uint16_t kHeaderHeight = 32U;
constexpr std::uint16_t kHeaderLocaleLeft = 188U;
constexpr std::uint16_t kHeaderLocaleWidth = 32U;
constexpr std::uint16_t kControlTop = 200U;
constexpr std::uint16_t kControlHeight = 40U;
constexpr std::uint16_t kProgramRowHeight = 18U;
constexpr char kLogoAssetPath[] =
    "assets/branding/manuengineer/ManuEngineer.svg";

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
    commands.push_back({ScreenDrawKind::Text,
                        rect,
                        token,
                        background,
                        resolve(packs, locale, key).value,
                        {}});
}

void addRawText(std::vector<ScreenDrawCommand>& commands,
                device_platform::DisplayRect rect, std::string text,
                device_platform::ThemeToken token,
                device_platform::ThemeToken background) {
    commands.push_back(
        {ScreenDrawKind::Text, rect, token, background, std::move(text), {}});
}

void addNetworkStatusIcon(std::vector<ScreenDrawCommand>& commands,
                          device_platform::DisplayRect rect,
                          device_platform::ThemeToken token,
                          device_platform::ThemeToken background) {
    commands.push_back(
        {ScreenDrawKind::NetworkStatusIcon, rect, token, background, {}, {}});
}

void addLogo(std::vector<ScreenDrawCommand>& commands,
             device_platform::DisplayRect rect) {
    commands.push_back(
        {ScreenDrawKind::Logo, rect, device_platform::ThemeToken::TextPrimary,
         device_platform::ThemeToken::Surface, "ManuEngineer", kLogoAssetPath});
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
    const auto scaled =
        static_cast<int>(temperature.valueCelsius.value() * 10.0);
    const auto absolute = scaled < 0 ? -scaled : scaled;
    std::string result;
    if (scaled < 0) result.push_back('-');
    result += std::to_string(absolute / 10);
    result.push_back('.');
    result.push_back(static_cast<char>('0' + absolute % 10));
    result += " C";
    return result;
}

// No real IANA time zone database is part of this port (see
// docs/tasks/issue-31-renderer-display-touch-calibration-plan.md, section 3);
// this therefore formats the trusted UTC instant directly rather than
// pretending to apply canonicalTimeZoneId as a local-time offset.
std::string formatClockText(
    const device_platform::ClockViewInput& clock) noexcept {
    if (!clock.trustedUtc.has_value()) return "--:--";
    const auto epoch = static_cast<std::time_t>(*clock.trustedUtc);
    std::tm calendar{};
    gmtime_r(&epoch, &calendar);
    char buffer[6];
    const auto written = std::snprintf(buffer, sizeof(buffer), "%02d:%02d",
                                       calendar.tm_hour, calendar.tm_min);
    if (written != 5) return "--:--";
    return std::string(buffer, 5U);
}

device_platform::ThemeToken networkStatusToken(
    device_platform::DeviceUiNetworkStatus status) noexcept {
    switch (status) {
        case device_platform::DeviceUiNetworkStatus::Connected:
            return device_platform::ThemeToken::StatusInformation;
        case device_platform::DeviceUiNetworkStatus::Disconnected:
            return device_platform::ThemeToken::StatusWarning;
        case device_platform::DeviceUiNetworkStatus::Unavailable:
            return device_platform::ThemeToken::TextSecondary;
    }
    return device_platform::ThemeToken::TextSecondary;
}

}  // namespace

std::uint16_t themeColor565(device_platform::ThemeToken token) noexcept {
    return tokenColor(token);
}

RepresentativeScreen makeRepresentativeScreen(
    const FermentationUiSnapshot& snapshot,
    FermentationTouchWorkspace& workspace,
    const std::vector<device_platform::TextPackManifest>& textPacks,
    const device_platform::LocaleId& locale,
    std::optional<device_platform::DeviceUiTarget> pressedTarget,
    const ProgramCatalog* catalog,
    device_platform::DeviceUiNetworkStatus networkStatus,
    device_platform::ClockViewInput clock) {
    RepresentativeScreen screen;
    screen.locale = locale;
    screen.header.locale = locale;
    screen.header.branding = device_platform::BrandingId{"manuengineer"};
    screen.header.networkStatus = networkStatus;
    screen.header.clock = clock;
    // The single canonical R1 theme/fallback contract: R1 ships exactly one
    // theme, so this reads it from the same catalog the platform build uses
    // instead of duplicating its id and token list as a second literal.
    const auto buildCatalog = makeFermentationR1DeviceUiBuildCatalog();
    const auto themeDescriptors = makeFermentationR1ThemeDescriptors();
    const auto themeIt = std::find_if(
        themeDescriptors.begin(), themeDescriptors.end(),
        [&buildCatalog](const device_platform::ThemeDescriptor& descriptor) {
            return descriptor.id == buildCatalog.defaultTheme;
        });
    screen.theme =
        themeIt != themeDescriptors.end()
            ? *themeIt
            : device_platform::ThemeDescriptor{buildCatalog.defaultTheme, {}};
    screen.logoAssetPath = kLogoAssetPath;
    screen.clockText = formatClockText(clock);
    screen.refreshRevision = snapshot.refreshRevision;
    screen.pressedTarget = pressedTarget;
    screen.workspace = workspace.view(snapshot, catalog);
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
    addRawText(commands,
               {kHeaderLocaleLeft, 4U, kHeaderLocaleWidth,
                RepresentativeScreen::kTextLineHeight},
               std::move(localeText), device_platform::ThemeToken::TextPrimary,
               device_platform::ThemeToken::Surface);
    addNetworkStatusIcon(commands,
                         {220U, 4U, 44U, RepresentativeScreen::kTextLineHeight},
                         networkStatusToken(networkStatus),
                         device_platform::ThemeToken::Surface);
    addRawText(commands, {264U, 4U, 52U, RepresentativeScreen::kTextLineHeight},
               screen.clockText, device_platform::ThemeToken::TextSecondary,
               device_platform::ThemeToken::Surface);
    addText(commands, textPacks, locale, screen.workspace.title,
            {8U, 40U, 144U, RepresentativeScreen::kTextLineHeight},
            device_platform::ThemeToken::TextPrimary,
            device_platform::ThemeToken::Canvas);

    // The content area below the title/home-mode row is page-specific: the
    // #26 workspace already carries the page-specific payload (home status,
    // program list, confirmation target, blocked reason, unavailable
    // recovery capabilities); only Home draws the home summary.
    if (screen.workspace.page == FermentationUiPage::Home) {
        addText(commands, textPacks, locale, homeModeKey(snapshot.home.mode),
                {168U, 40U, 144U, RepresentativeScreen::kTextLineHeight},
                device_platform::ThemeToken::StatusInformation,
                device_platform::ThemeToken::Canvas);

        const auto temperatureCount =
            std::min<std::size_t>(snapshot.temperatures.size(), 3U);
        for (std::size_t index = 0U; index < temperatureCount; ++index) {
            const auto left = static_cast<std::uint16_t>(8U + index * 104U);
            addFill(commands, {left, 68U, 96U, 48U},
                    device_platform::ThemeToken::Surface);
            addText(commands, textPacks, locale, appKey("status"),
                    {static_cast<std::uint16_t>(left + 4U), 72U, 88U,
                     RepresentativeScreen::kTextLineHeight},
                    device_platform::ThemeToken::TextSecondary,
                    device_platform::ThemeToken::Surface);
            commands.push_back({ScreenDrawKind::Text,
                                {static_cast<std::uint16_t>(left + 4U), 90U,
                                 88U, RepresentativeScreen::kTextLineHeight},
                                device_platform::ThemeToken::StatusInformation,
                                device_platform::ThemeToken::Surface,
                                temperatureText(snapshot.temperatures[index]),
                                {}});
        }
        addText(commands, textPacks, locale, appKey("messages"),
                {8U, 128U, 88U, RepresentativeScreen::kTextLineHeight},
                device_platform::ThemeToken::StatusWarning,
                device_platform::ThemeToken::Canvas);
        addText(commands, textPacks, locale,
                snapshot.service.available ? appKey("service")
                                           : appKey("service-home-locked"),
                {112U, 128U, 96U, RepresentativeScreen::kTextLineHeight},
                snapshot.service.available
                    ? device_platform::ThemeToken::PrimaryAction
                    : device_platform::ThemeToken::StatusWarning,
                device_platform::ThemeToken::Canvas);
        addText(commands, textPacks, locale, appKey("network"),
                {224U, 128U, 88U, RepresentativeScreen::kTextLineHeight},
                device_platform::ThemeToken::StatusInformation,
                device_platform::ThemeToken::Canvas);
    } else {
        if (!screen.workspace.programList.empty()) {
            const auto rowCount =
                std::min<std::size_t>(screen.workspace.programList.size(), 3U);
            for (std::size_t index = 0U; index < rowCount; ++index) {
                const auto& entry = screen.workspace.programList[index];
                const auto top = static_cast<std::uint16_t>(68U + index * 18U);
                addFill(commands, {8U, top, 304U, kProgramRowHeight},
                        device_platform::ThemeToken::Surface);
                addRawText(
                    commands,
                    {12U, top, 296U, RepresentativeScreen::kTextLineHeight},
                    entry.program.program.name,
                    entry.startable
                        ? device_platform::ThemeToken::TextPrimary
                        : device_platform::ThemeToken::TextSecondary,
                    device_platform::ThemeToken::Surface);
            }
        } else if (screen.workspace.confirmationProgramName.has_value()) {
            addRawText(commands,
                       {8U, 68U, 304U, RepresentativeScreen::kTextLineHeight},
                       *screen.workspace.confirmationProgramName,
                       device_platform::ThemeToken::TextPrimary,
                       device_platform::ThemeToken::Canvas);
        }
        if (screen.workspace.blockedReason.has_value()) {
            addText(commands, textPacks, locale,
                    *screen.workspace.blockedReason,
                    {8U, 128U, 304U, RepresentativeScreen::kTextLineHeight},
                    device_platform::ThemeToken::StatusWarning,
                    device_platform::ThemeToken::Canvas);
        } else if (!screen.workspace.unavailableCapabilities.empty()) {
            addRawText(commands,
                       {8U, 128U, 304U, RepresentativeScreen::kTextLineHeight},
                       resolve(textPacks, locale, appKey("unavailable")).value +
                           " " +
                           std::to_string(
                               screen.workspace.unavailableCapabilities.size()),
                       device_platform::ThemeToken::StatusWarning,
                       device_platform::ThemeToken::Canvas);
        }
    }

    if (screen.workspace.pager.itemCount > 0U &&
        screen.workspace.pager.valid()) {
        addRawText(commands,
                   {8U, 156U, 72U, RepresentativeScreen::kTextLineHeight},
                   std::to_string(screen.workspace.pager.currentIndex + 1U) +
                       "/" + std::to_string(screen.workspace.pager.itemCount),
                   device_platform::ThemeToken::TextSecondary,
                   device_platform::ThemeToken::Canvas);
    }
    if (screen.workspace.confirmationWarning.has_value()) {
        addFill(commands, {32U, 148U, 256U, 44U},
                device_platform::ThemeToken::Overlay);
        addText(commands, textPacks, locale,
                *screen.workspace.confirmationWarning,
                {40U, 154U, 240U, RepresentativeScreen::kTextLineHeight},
                device_platform::ThemeToken::TextPrimary,
                device_platform::ThemeToken::Overlay);
        addRawText(
            commands, {40U, 174U, 224U, RepresentativeScreen::kTextLineHeight},
            "cancel  confirm", device_platform::ThemeToken::PrimaryAction,
            device_platform::ThemeToken::Overlay);
    }

    for (std::size_t index = 0U; index < screen.workspace.bottomSlots.size();
         ++index) {
        const auto left = static_cast<std::uint16_t>(index * 80U);
        const auto& slot = screen.workspace.bottomSlots[index];
        addFill(commands, {left, kControlTop, 80U, kControlHeight},
                slot.enabled ? device_platform::ThemeToken::PrimaryAction
                             : device_platform::ThemeToken::SecondaryAction);
        addText(
            commands, textPacks, locale, slot.label,
            {static_cast<std::uint16_t>(left + 4U),
             static_cast<std::uint16_t>(
                 kControlTop +
                 (kControlHeight - RepresentativeScreen::kTextLineHeight) / 2U),
             68U, RepresentativeScreen::kTextLineHeight},
            slot.enabled ? device_platform::ThemeToken::OnPrimaryAction
                         : device_platform::ThemeToken::TextSecondary,
            slot.enabled ? device_platform::ThemeToken::PrimaryAction
                         : device_platform::ThemeToken::SecondaryAction);
    }
    if (pressedTarget.has_value() &&
        pressedTarget->kind ==
            device_platform::DeviceUiTargetKind::BottomSlot &&
        pressedTarget->slotIndex < screen.workspace.bottomSlots.size()) {
        const auto left =
            static_cast<std::uint16_t>(pressedTarget->slotIndex * 80U);
        commands.push_back({ScreenDrawKind::PressFeedback,
                            {left, kControlTop, 80U, kControlHeight},
                            device_platform::ThemeToken::SecondaryAction,
                            device_platform::ThemeToken::PrimaryAction,
                            {},
                            {}});
    }
    return screen;
}

bool operator==(const ScreenRenderKey& left,
                const ScreenRenderKey& right) noexcept {
    for (std::size_t index = 0U; index < left.bottomSlots.size(); ++index) {
        const auto& leftSlot = left.bottomSlots[index];
        const auto& rightSlot = right.bottomSlots[index];
        if (leftSlot.kind != rightSlot.kind ||
            leftSlot.label != rightSlot.label ||
            leftSlot.enabled != rightSlot.enabled) {
            return false;
        }
    }
    return left.refreshRevision == right.refreshRevision &&
           left.locale == right.locale && left.page == right.page &&
           left.pagerCurrentIndex == right.pagerCurrentIndex &&
           left.pagerItemCount == right.pagerItemCount &&
           left.hasConfirmationWarning == right.hasConfirmationWarning &&
           left.confirmationProgramName == right.confirmationProgramName &&
           left.completionLocked == right.completionLocked &&
           left.blockedReason == right.blockedReason &&
           left.unavailableCapabilityCount ==
               right.unavailableCapabilityCount &&
           left.programListSize == right.programListSize &&
           left.pressedBottomSlotIndex == right.pressedBottomSlotIndex &&
           left.networkStatus == right.networkStatus &&
           left.trustedUtc == right.trustedUtc && left.themeId == right.themeId;
}

ScreenRenderKey makeScreenRenderKey(
    const RepresentativeScreen& screen) noexcept {
    ScreenRenderKey key;
    key.refreshRevision = screen.refreshRevision;
    key.locale = screen.locale;
    key.page = screen.workspace.page;
    key.pagerCurrentIndex = screen.workspace.pager.currentIndex;
    key.pagerItemCount = screen.workspace.pager.itemCount;
    key.hasConfirmationWarning =
        screen.workspace.confirmationWarning.has_value();
    key.confirmationProgramName =
        screen.workspace.confirmationProgramName.value_or(std::string{});
    key.completionLocked = screen.workspace.completionLocked;
    key.blockedReason = screen.workspace.blockedReason;
    key.unavailableCapabilityCount =
        screen.workspace.unavailableCapabilities.size();
    key.programListSize = screen.workspace.programList.size();
    key.bottomSlots = screen.workspace.bottomSlots;
    if (screen.pressedTarget.has_value() &&
        screen.pressedTarget->kind ==
            device_platform::DeviceUiTargetKind::BottomSlot) {
        key.pressedBottomSlotIndex = screen.pressedTarget->slotIndex;
    }
    key.networkStatus = screen.header.networkStatus;
    key.trustedUtc = screen.header.clock.trustedUtc;
    key.themeId = screen.theme.id;
    return key;
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

FermentationUiWorkspacePress routePress(FermentationTouchWorkspace& workspace,
                                        const FermentationUiSnapshot& snapshot,
                                        const RepresentativeScreen& screen,
                                        std::uint16_t x, std::uint16_t y,
                                        const ProgramCatalog* catalog) {
    const auto target = targetAt(screen, x, y);
    if (!target.has_value()) return {};
    return workspace.press(snapshot, target.value(), catalog);
}

}  // namespace fermentation::main_ui
