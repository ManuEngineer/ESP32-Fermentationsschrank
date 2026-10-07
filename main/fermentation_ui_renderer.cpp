#include "fermentation_ui_renderer.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <utility>

#include "local_time.hpp"

namespace fermentation::main_ui {
namespace {

constexpr std::uint16_t kHeaderHeight = 32U;
constexpr std::uint16_t kHeaderLocaleLeft = 188U;
constexpr std::uint16_t kHeaderLocaleWidth = 32U;
constexpr device_platform::DisplayRect kHeaderNetworkRect{
    220U, 4U, 44U, RepresentativeScreen::kTextLineHeight};
// Touch zone around the visible language code (x=188..220). It ends at
// x=219 so it never overlaps kHeaderNetworkRect and starts right of the logo
// (x=4..172).
constexpr device_platform::DisplayRect kHeaderLanguageHitRect{176U, 0U, 44U,
                                                              kHeaderHeight};
// Header clock touch zone (S4): x=264..320 over the full header height. It
// starts exactly where kHeaderNetworkRect ends (x=264), so the zones never
// overlap.
constexpr device_platform::DisplayRect kHeaderClockHitRect{264U, 0U, 56U,
                                                           kHeaderHeight};
constexpr std::uint16_t kControlTop = 200U;
constexpr std::uint16_t kControlHeight = 40U;
// Touch rows of a content list (same height as the bottom controls) and the
// first row top; three rows fit between the title row and the reason line.
constexpr std::uint16_t kContentRowTop = 64U;
constexpr std::uint16_t kContentRowHeight = kControlHeight;
constexpr std::uint16_t kContentRowLeft = 8U;
constexpr std::uint16_t kContentRowWidth = 304U;
// The drawn rows end at y=182 (2 px gap); the 18 px reason line then ends
// exactly at the bottom controls (y=200).
constexpr std::uint16_t kListReasonTop = 182U;

// Pages whose content is a row list over the workspace pager. The pager item
// count is the number of listed entries (programs, messages or languages).
bool isContentListPage(FermentationUiPage page) noexcept {
    return page == FermentationUiPage::ProgramList ||
           page == FermentationUiPage::Messages ||
           page == FermentationUiPage::HeaderLanguage ||
           page == FermentationUiPage::Settings;
}
// ProgramSummary start-field rows share the list geometry but leave the right
// strip (x=256..312) to the two pager buttons (up: y=64..124, down:
// y=124..184); both are ContentCells (rows in column 0, buttons in column 1).
constexpr std::uint16_t kSummaryRowWidth = 244U;
constexpr std::uint16_t kSummaryButtonLeft = 256U;
constexpr std::uint16_t kSummaryButtonWidth = 56U;
constexpr std::uint16_t kSummaryButtonHeight =
    kFermentationUiListVisibleRows * kContentRowHeight / 2U;
// Numeric keypad of the ValueEdit page (D8): 4 x 3 cells of 98 x 32 px on a
// 102 x 34 px pitch from (8, 62).
constexpr std::uint16_t kKeypadLeft = 8U;
constexpr std::uint16_t kKeypadTop = 62U;
constexpr std::uint16_t kKeypadPitchX = 102U;
constexpr std::uint16_t kKeypadPitchY = 34U;
constexpr std::uint16_t kKeypadCellWidth = 98U;
// Drawn key face (leaves a 2 px visual gap); the active touch height of every
// row is the full 34 px pitch (D8 hardware acceptance criterion).
constexpr std::uint16_t kKeypadCellHeight = 32U;
constexpr std::uint16_t kKeypadTouchHeight = kKeypadPitchY;
constexpr std::uint16_t kSummaryNameLeft = 68U;
constexpr std::uint16_t kSummaryNameWidth = 176U;
// On-screen keyboard (S10, O3): 4 rows x 10 columns from (8, 62); every key
// row is active over its full 34 px pitch, each column over 30 px (hardware
// acceptance: 30 px wide keys); the drawn face leaves a 2 px gap.
constexpr std::uint16_t kKeyboardLeft = 8U;
constexpr std::uint16_t kKeyboardTop = 62U;
constexpr std::uint16_t kKeyboardPitchX = 30U;
constexpr std::uint16_t kKeyboardPitchY = 34U;
constexpr std::uint16_t kKeyboardFaceWidth = 28U;
constexpr std::uint16_t kKeyboardFaceHeight = 32U;
constexpr std::size_t kNetworkScreenDrawCommandCapacity = 21U;
constexpr device_platform::DisplayRect kNetworkPageTitleRect{
    8U, 34U, 140U, RepresentativeScreen::kTextLineHeight};
constexpr device_platform::DisplayRect kNetworkCurrentModeRect{
    8U, 52U, 140U, RepresentativeScreen::kTextLineHeight};
constexpr device_platform::DisplayRect kNetworkSsidRect{8U, 72U, 140U, 36U};
constexpr device_platform::DisplayRect kNetworkPasswordRect{8U, 110U, 140U,
                                                            54U};
constexpr device_platform::DisplayRect kNetworkIpRect{
    8U, 166U, 140U, RepresentativeScreen::kTextLineHeight};
constexpr device_platform::DisplayRect kNetworkQrRect{156U, 34U, 164U, 164U};
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
                device_platform::ThemeToken background, bool wrapText = false) {
    commands.push_back({ScreenDrawKind::Text,
                        rect,
                        token,
                        background,
                        std::move(text),
                        {},
                        wrapText});
}

// "<label><value>" in one line; both parts come from the text packs or from
// an already formatted existing value.
void addLabeledRawText(
    std::vector<ScreenDrawCommand>& commands,
    const std::vector<device_platform::TextPackManifest>& packs,
    const device_platform::LocaleId& locale, const char* labelKey,
    const std::string& value, device_platform::DisplayRect rect,
    device_platform::ThemeToken token,
    device_platform::ThemeToken background =
        device_platform::ThemeToken::Canvas) {
    addRawText(
        commands, rect,
        resolve(packs, locale, fermentationTextKey(labelKey)).value + value,
        token, background);
}

constexpr std::uint16_t kPageLineLeft = 8U;
constexpr std::uint16_t kPageLineWidth = 304U;
// Content lines of the read-only pages sit in the free area below the title
// row (y=40..58) and above the reason line (y=182).
constexpr std::uint16_t pageLineTop(std::uint16_t index,
                                    std::uint16_t step) noexcept {
    return static_cast<std::uint16_t>(62U + index * step);
}

void addLockIcon(std::vector<ScreenDrawCommand>& commands,
                 device_platform::DisplayRect rect,
                 device_platform::ThemeToken token,
                 device_platform::ThemeToken background) {
    commands.push_back(
        {ScreenDrawKind::LockIcon, rect, token, background, {}, {}});
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

std::string celsiusText(const std::optional<double>& value) {
    if (!value.has_value()) return "--.- C";
    const auto scaled = static_cast<int>(value.value() * 10.0);
    const auto absolute = scaled < 0 ? -scaled : scaled;
    std::string result;
    if (scaled < 0) result.push_back('-');
    result += std::to_string(absolute / 10);
    result.push_back('.');
    result.push_back(static_cast<char>('0' + absolute % 10));
    result += " C";
    return result;
}

std::string temperatureText(const TemperatureView& temperature) {
    return celsiusText(temperature.valueCelsius);
}

std::string minutesText(const std::optional<std::uint32_t>& minutes) {
    if (!minutes.has_value()) return "--";
    return std::to_string(*minutes) + " min";
}

// Local wall-clock text from the single local-time owner (Issue #178). No
// trusted UTC or no result of toLocalTime() (for example an unavailable zone
// rule) yields "--:--"; the UTC value is never shown as a local time.
std::string formatClockText(
    const device_platform::ClockViewInput& clock) noexcept {
    const auto local =
        device_platform::toLocalTime(clock.trustedUtc, clock.timeZoneRule);
    if (!local.has_value()) return "--:--";
    char buffer[6];
    const auto written = std::snprintf(buffer, sizeof(buffer), "%02u:%02u",
                                       static_cast<unsigned>(local->hour),
                                       static_cast<unsigned>(local->minute));
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

device_platform::TextKey networkModeTextKey(device_platform::NetworkMode mode) {
    switch (mode) {
        case device_platform::NetworkMode::AP_ONLY:
            return fermentationTextKey("network-ap-only");
        case device_platform::NetworkMode::HOME_WIFI:
            return fermentationTextKey("network-home-wifi");
        case device_platform::NetworkMode::UNSELECTED:
            return fermentationTextKey("network-mode-required");
    }
    return fermentationTextKey("network-mode-required");
}

std::string ipv4Text(std::uint32_t address) {
    return std::to_string(address & 0xFFU) + "." +
           std::to_string((address >> 8U) & 0xFFU) + "." +
           std::to_string((address >> 16U) & 0xFFU) + "." +
           std::to_string((address >> 24U) & 0xFFU);
}

std::uint64_t networkInfoFingerprint(
    const device_platform::NetworkAccessPointInfo& info) noexcept {
    constexpr std::uint64_t kOffset = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    auto hash = kOffset;
    const auto append = [&hash](const std::string& value) {
        for (const auto character : value) {
            hash ^= static_cast<unsigned char>(character);
            hash *= kPrime;
        }
        hash ^= 0xFFU;
        hash *= kPrime;
    };
    append(info.ssid);
    append(info.password);
    if (info.ipv4Address.has_value()) {
        const auto address = *info.ipv4Address;
        for (unsigned int shift = 0U; shift < 32U; shift += 8U) {
            hash ^= static_cast<std::uint8_t>(address >> shift);
            hash *= kPrime;
        }
    } else {
        hash ^= 0U;
        hash *= kPrime;
    }
    return hash;
}

}  // namespace

std::uint16_t themeColor565(device_platform::ThemeToken token) noexcept {
    return tokenColor(token);
}

std::optional<std::string> makeSoftApWifiQrPayload(
    const device_platform::NetworkAccessPointInfo& accessPoint) {
    if (accessPoint.ssid.empty() || accessPoint.password.empty()) {
        return std::nullopt;
    }
    const auto escape = [](std::string_view value) {
        std::string escaped;
        escaped.reserve(value.size());
        for (const auto character : value) {
            if (character == '\\' || character == ';' || character == ',' ||
                character == ':' || character == '"') {
                escaped.push_back('\\');
            }
            escaped.push_back(character);
        }
        return escaped;
    };

    std::string payload{"WIFI:T:WPA;S:"};
    payload += escape(accessPoint.ssid);
    payload += ";P:";
    payload += escape(accessPoint.password);
    payload += ";;";
    return payload;
}

RepresentativeScreen makeRepresentativeScreen(
    const FermentationUiSnapshot& snapshot,
    FermentationTouchWorkspace& workspace,
    const std::vector<device_platform::TextPackManifest>& textPacks,
    const device_platform::LocaleId& locale,
    std::optional<device_platform::DeviceUiTarget> pressedTarget,
    const ProgramCatalog* catalog,
    device_platform::DeviceUiNetworkStatus networkStatus,
    device_platform::ClockViewInput clock,
    const std::optional<device_platform::NetworkAccessPointInfo>&
        networkAccessPointInfo) {
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
    if (screen.workspace.page == FermentationUiPage::HeaderNetwork &&
        networkAccessPointInfo.has_value()) {
        screen.localNetworkInfoFingerprint =
            networkInfoFingerprint(*networkAccessPointInfo);
    }
    auto& commands = screen.commands;
    if (screen.workspace.page == FermentationUiPage::HeaderNetwork) {
        // The network projection emits at most 21 commands, including
        // press feedback. Allocate that bounded capacity once so repeated
        // network redraws do not grow and replace the vector buffer twice.
        commands.reserve(kNetworkScreenDrawCommandCapacity);
    }
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
    addNetworkStatusIcon(commands, kHeaderNetworkRect,
                         networkStatusToken(networkStatus),
                         device_platform::ThemeToken::Surface);
    addRawText(commands, {264U, 4U, 52U, RepresentativeScreen::kTextLineHeight},
               screen.clockText, device_platform::ThemeToken::TextSecondary,
               device_platform::ThemeToken::Surface);
    addText(commands, textPacks, locale, screen.workspace.title,
            screen.workspace.page == FermentationUiPage::HeaderNetwork
                ? kNetworkPageTitleRect
            : screen.workspace.page == FermentationUiPage::ProgramSummary
                ? device_platform::
                      DisplayRect{8U, 40U, 56U,
                                  RepresentativeScreen::kTextLineHeight}
            : screen.workspace.page == FermentationUiPage::ValueEdit
                ? device_platform::
                      DisplayRect{8U, 40U, 104U,
                                  RepresentativeScreen::kTextLineHeight}
            : screen.workspace.page == FermentationUiPage::TextEdit
                ? device_platform::
                      DisplayRect{8U, 40U, 84U,
                                  RepresentativeScreen::kTextLineHeight}
                : device_platform::
                      DisplayRect{8U, 40U, 144U,
                                  RepresentativeScreen::kTextLineHeight},
            device_platform::ThemeToken::TextPrimary,
            device_platform::ThemeToken::Canvas);
    if (screen.workspace.page == FermentationUiPage::HeaderNetwork) {
        addText(commands, textPacks, locale,
                networkModeTextKey(snapshot.network.currentMode),
                kNetworkCurrentModeRect,
                device_platform::ThemeToken::StatusInformation,
                device_platform::ThemeToken::Canvas);
    }

    // The two pager buttons (ContentCell column 1, rows 0 and 1) beside a row
    // list with more entries than visible rows.
    const auto drawPagerButtons = [&](bool shown) {
        for (std::uint16_t button = 0U; button < (shown ? 2U : 0U); ++button) {
            const bool enabled = button == 0U
                                     ? screen.workspace.pager.canMoveUp()
                                     : screen.workspace.pager.canMoveDown();
            const auto top = static_cast<std::uint16_t>(
                kContentRowTop + button * kSummaryButtonHeight);
            const auto fill =
                enabled ? device_platform::ThemeToken::PrimaryAction
                        : device_platform::ThemeToken::SecondaryAction;
            addFill(commands,
                    {kSummaryButtonLeft, top, kSummaryButtonWidth,
                     static_cast<std::uint16_t>(kSummaryButtonHeight - 2U)},
                    fill);
            addText(commands, textPacks, locale,
                    fermentationTextKey(button == 0U ? "up" : "down"),
                    {static_cast<std::uint16_t>(kSummaryButtonLeft + 4U),
                     static_cast<std::uint16_t>(
                         top + (kSummaryButtonHeight -
                                RepresentativeScreen::kTextLineHeight) /
                                   2U),
                     static_cast<std::uint16_t>(kSummaryButtonWidth - 8U),
                     RepresentativeScreen::kTextLineHeight},
                    enabled ? device_platform::ThemeToken::OnPrimaryAction
                            : device_platform::ThemeToken::TextSecondary,
                    fill);
        }
    };

    // Rows of a field list (program summary start values or manual run
    // values) with the pager buttons; shared by every page that has one.
    const auto drawFieldRows = [&]() {
        // Display projection only: the binding StartSummary stays the
        // result of the command owner. Absent values show "--"; a value
        // that differs from the stored program is marked with " *".
        const auto& summary = *screen.workspace.programSummary;
        if (!summary.name.empty()) {
            addRawText(commands,
                       {kSummaryNameLeft, 40U, kSummaryNameWidth,
                        RepresentativeScreen::kTextLineHeight},
                       summary.name, device_platform::ThemeToken::TextPrimary,
                       device_platform::ThemeToken::Canvas);
        }
        const auto first = screen.workspace.pager.currentIndex;
        const auto rowCount = std::min<std::size_t>(
            summary.fieldCount > first ? summary.fieldCount - first : 0U,
            kFermentationUiListVisibleRows - summary.rowOffset);
        for (std::size_t index = 0U; index < rowCount; ++index) {
            const auto field = summary.fields[first + index];
            const auto top = static_cast<std::uint16_t>(
                kContentRowTop +
                (index + summary.rowOffset) * kContentRowHeight);
            addFill(commands,
                    {kContentRowLeft, top, kSummaryRowWidth,
                     static_cast<std::uint16_t>(kContentRowHeight - 2U)},
                    device_platform::ThemeToken::Surface);
            const device_platform::DisplayRect rect{
                12U,
                static_cast<std::uint16_t>(
                    top + (kContentRowHeight -
                           RepresentativeScreen::kTextLineHeight) /
                              2U),
                236U, RepresentativeScreen::kTextLineHeight};
            const bool changed =
                summary.changed[static_cast<std::size_t>(field)];
            const auto token =
                changed ? device_platform::ThemeToken::PrimaryAction
                        : (summary.editable
                               ? device_platform::ThemeToken::TextPrimary
                               : device_platform::ThemeToken::TextSecondary);
            const auto surface = device_platform::ThemeToken::Surface;
            const char* mark = changed ? " *" : "";
            switch (field) {
                case FermentationUiStartField::TargetTemperature:
                    addLabeledRawText(
                        commands, textPacks, locale, "label-target",
                        celsiusText(summary.targetTemperatureCelsius) + mark,
                        rect, token, surface);
                    break;
                case FermentationUiStartField::Duration:
                    addLabeledRawText(
                        commands, textPacks, locale, "label-duration",
                        minutesText(summary.durationMinutes) + mark, rect,
                        token, surface);
                    break;
                case FermentationUiStartField::CoolingTarget:
                    addLabeledRawText(
                        commands, textPacks, locale, "label-cooling",
                        celsiusText(summary.coolingTargetCelsius) + mark, rect,
                        token, surface);
                    break;
                case FermentationUiStartField::HoldDuration:
                    addLabeledRawText(
                        commands, textPacks, locale, "label-hold",
                        minutesText(summary.holdDurationMinutes) + mark, rect,
                        token, surface);
                    break;
                case FermentationUiStartField::Preheat:
                    addLabeledRawText(
                        commands, textPacks, locale, "label-preheat",
                        resolve(textPacks, locale,
                                fermentationTextKey(
                                    summary.preheat ? "value-on" : "value-off"))
                                .value +
                            mark,
                        rect, token, surface);
                    break;
                case FermentationUiStartField::SensorMode:
                    addLabeledRawText(
                        commands, textPacks, locale, "label-sensor",
                        resolve(textPacks, locale,
                                summary.manual
                                    ? runSensorModeTextKey(summary.sensorMode)
                                : summary.sensorModeOverride.has_value()
                                    ? runSensorModeTextKey(
                                          *summary.sensorModeOverride)
                                    : sensorPreferenceTextKey(
                                          summary.sensorPreference))
                                .value +
                            mark,
                        rect, token, surface);
                    break;
                case FermentationUiStartField::CompletionMode:
                    addLabeledRawText(
                        commands, textPacks, locale, "label-completion",
                        resolve(textPacks, locale,
                                completionModeTextKey(summary.completionMode))
                                .value +
                            mark,
                        rect, token, surface);
                    break;
            }
        }
        drawPagerButtons(summary.pagerButtons);
    };

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
    } else if (screen.workspace.page == FermentationUiPage::HeaderClock) {
        // The single shared time screen: trust state, canonical zone id and
        // the local time of the owner. It only shows existing values.
        const bool trusted = clock.trustedUtc.has_value();
        addText(commands, textPacks, locale,
                fermentationTextKey(trusted ? "clock-trusted"
                                            : "clock-not-trusted"),
                {8U, 68U, 304U, RepresentativeScreen::kTextLineHeight},
                trusted ? device_platform::ThemeToken::StatusInformation
                        : device_platform::ThemeToken::StatusWarning,
                device_platform::ThemeToken::Canvas);
        addRawText(commands,
                   {8U, 90U, 304U, RepresentativeScreen::kTextLineHeight},
                   clock.canonicalTimeZoneId.empty()
                       ? std::string{"--"}
                       : clock.canonicalTimeZoneId.value(),
                   device_platform::ThemeToken::TextSecondary,
                   device_platform::ThemeToken::Canvas);
        addRawText(commands,
                   {8U, 112U, 304U, RepresentativeScreen::kTextLineHeight},
                   screen.clockText, device_platform::ThemeToken::TextPrimary,
                   device_platform::ThemeToken::Canvas);
    } else if (screen.workspace.page == FermentationUiPage::HeaderWebAccess) {
        // The Application owns the release state; the page only shows it.
        const char* statusKey = "web-access-unavailable";
        auto statusToken = device_platform::ThemeToken::TextSecondary;
        if (snapshot.webAccess == FermentationWebAccessState::WindowOpen) {
            statusKey = "web-access-window-open";
            statusToken = device_platform::ThemeToken::StatusInformation;
        } else if (snapshot.webAccess == FermentationWebAccessState::Closed) {
            statusKey = "web-access-closed";
            statusToken = device_platform::ThemeToken::StatusWarning;
        }
        addText(commands, textPacks, locale, fermentationTextKey(statusKey),
                {8U, 68U, 304U, RepresentativeScreen::kTextLineHeight},
                statusToken, device_platform::ThemeToken::Canvas);
    } else if (screen.workspace.page == FermentationUiPage::HeaderNetwork) {
        if (networkAccessPointInfo.has_value() &&
            !networkAccessPointInfo->ssid.empty() &&
            !networkAccessPointInfo->password.empty()) {
            const auto ssidPrefix =
                resolve(textPacks, locale, fermentationTextKey("network-ssid"));
            const auto passwordPrefix = resolve(
                textPacks, locale, fermentationTextKey("network-password"));
            addRawText(commands, kNetworkSsidRect,
                       ssidPrefix.value + networkAccessPointInfo->ssid,
                       device_platform::ThemeToken::TextPrimary,
                       device_platform::ThemeToken::Canvas, true);
            addRawText(commands, kNetworkPasswordRect,
                       passwordPrefix.value + networkAccessPointInfo->password,
                       device_platform::ThemeToken::TextPrimary,
                       device_platform::ThemeToken::Canvas, true);
            addRawText(commands, kNetworkIpRect,
                       std::string{"IP: "} +
                           (networkAccessPointInfo->ipv4Address.has_value()
                                ? ipv4Text(*networkAccessPointInfo->ipv4Address)
                                : resolve(textPacks, locale,
                                          fermentationTextKey(
                                              "network-ip-unavailable"))
                                      .value),
                       device_platform::ThemeToken::TextPrimary,
                       device_platform::ThemeToken::Canvas);
            if (auto payload = makeSoftApWifiQrPayload(*networkAccessPointInfo);
                payload.has_value()) {
                commands.push_back({ScreenDrawKind::QrCode,
                                    kNetworkQrRect,
                                    device_platform::ThemeToken::TextPrimary,
                                    device_platform::ThemeToken::Canvas,
                                    std::move(*payload),
                                    {},
                                    false});
            }
        } else {
            addText(commands, textPacks, locale,
                    fermentationTextKey("network-access-unavailable"),
                    {8U, 68U, 184U, RepresentativeScreen::kTextLineHeight},
                    device_platform::ThemeToken::StatusWarning,
                    device_platform::ThemeToken::Canvas);
        }
    } else {
        if (!screen.workspace.programList.empty()) {
            // Window over the list: row r shows entry currentIndex + r.
            const auto first = screen.workspace.pager.currentIndex;
            const auto rowCount = std::min<std::size_t>(
                screen.workspace.programList.size() - first,
                kFermentationUiListVisibleRows);
            for (std::size_t index = 0U; index < rowCount; ++index) {
                const auto& entry = screen.workspace.programList[first + index];
                const auto top = static_cast<std::uint16_t>(
                    kContentRowTop + index * kContentRowHeight);
                // The drawn row leaves a 2 px gap; the touch row is the full
                // kContentRowHeight (see targetAt()).
                addFill(commands,
                        {kContentRowLeft, top, kContentRowWidth,
                         static_cast<std::uint16_t>(kContentRowHeight - 2U)},
                        device_platform::ThemeToken::Surface);
                addRawText(commands,
                           {12U,
                            static_cast<std::uint16_t>(
                                top + (kContentRowHeight -
                                       RepresentativeScreen::kTextLineHeight) /
                                          2U),
                            296U, RepresentativeScreen::kTextLineHeight},
                           entry.program.program.name,
                           entry.startable
                               ? device_platform::ThemeToken::TextPrimary
                               : device_platform::ThemeToken::TextSecondary,
                           device_platform::ThemeToken::Surface);
            }
        } else if (screen.workspace.page ==
                   FermentationUiPage::HeaderLanguage) {
            // One row per language included in this build; the active
            // display language is drawn as the selected row.
            const auto catalog = makeFermentationR1DeviceUiBuildCatalog();
            const auto rowCount = std::min<std::size_t>(
                catalog.includedLocales.size(), kFermentationUiListVisibleRows);
            for (std::size_t index = 0U; index < rowCount; ++index) {
                const auto& language = catalog.includedLocales[index];
                const bool active = language.value() == locale.value();
                const auto top = static_cast<std::uint16_t>(
                    kContentRowTop + index * kContentRowHeight);
                const auto fill =
                    active ? device_platform::ThemeToken::PrimaryAction
                           : device_platform::ThemeToken::Surface;
                addFill(commands,
                        {kContentRowLeft, top, kContentRowWidth,
                         static_cast<std::uint16_t>(kContentRowHeight - 2U)},
                        fill);
                addText(commands, textPacks, locale,
                        fermentationTextKey(
                            ("language-" + language.value()).c_str()),
                        {12U,
                         static_cast<std::uint16_t>(
                             top + (kContentRowHeight -
                                    RepresentativeScreen::kTextLineHeight) /
                                       2U),
                         296U, RepresentativeScreen::kTextLineHeight},
                        active ? device_platform::ThemeToken::OnPrimaryAction
                               : device_platform::ThemeToken::TextPrimary,
                        fill);
            }
        } else if (screen.workspace.page == FermentationUiPage::Messages) {
            if (snapshot.messages.empty()) {
                addText(commands, textPacks, locale,
                        fermentationTextKey("messages-empty"),
                        {kPageLineLeft, 68U, kPageLineWidth,
                         RepresentativeScreen::kTextLineHeight},
                        device_platform::ThemeToken::TextSecondary,
                        device_platform::ThemeToken::Canvas);
            }
            // Window over snapshot.messages: row r shows message
            // currentIndex + r (same geometry as the program list).
            const auto first = screen.workspace.pager.currentIndex;
            const auto rowCount =
                std::min<std::size_t>(snapshot.messages.size() > first
                                          ? snapshot.messages.size() - first
                                          : 0U,
                                      kFermentationUiListVisibleRows);
            for (std::size_t index = 0U; index < rowCount; ++index) {
                const auto& message = snapshot.messages[first + index].message;
                const auto top = static_cast<std::uint16_t>(
                    kContentRowTop + index * kContentRowHeight);
                const auto textTop = static_cast<std::uint16_t>(
                    top + (kContentRowHeight -
                           RepresentativeScreen::kTextLineHeight) /
                              2U);
                addFill(commands,
                        {kContentRowLeft, top, kContentRowWidth,
                         static_cast<std::uint16_t>(kContentRowHeight - 2U)},
                        device_platform::ThemeToken::Surface);
                addText(
                    commands, textPacks, locale,
                    messageCodeTextKey(message.code),
                    {12U, textTop, 196U, RepresentativeScreen::kTextLineHeight},
                    message.acknowledged
                        ? device_platform::ThemeToken::TextSecondary
                        : device_platform::ThemeToken::TextPrimary,
                    device_platform::ThemeToken::Surface);
                if (message.acousticMuted || message.acknowledged) {
                    addText(commands, textPacks, locale,
                            fermentationTextKey(message.acousticMuted
                                                    ? "message-muted"
                                                    : "message-acknowledged"),
                            {212U, textTop, 96U,
                             RepresentativeScreen::kTextLineHeight},
                            device_platform::ThemeToken::TextSecondary,
                            device_platform::ThemeToken::Surface);
                }
            }
        } else if (screen.workspace.page == FermentationUiPage::MessageDetail &&
                   screen.workspace.selectedMessageId.has_value()) {
            const auto selected =
                std::find_if(snapshot.messages.begin(), snapshot.messages.end(),
                             [&screen](const MessageView& view) {
                                 return view.message.id ==
                                        *screen.workspace.selectedMessageId;
                             });
            if (selected != snapshot.messages.end()) {
                const auto& message = selected->message;
                addText(commands, textPacks, locale,
                        messageCodeTextKey(message.code),
                        {8U, 68U, 304U, RepresentativeScreen::kTextLineHeight},
                        device_platform::ThemeToken::TextPrimary,
                        device_platform::ThemeToken::Canvas);
                addText(commands, textPacks, locale,
                        messageClassTextKey(message.messageClass),
                        {8U, 92U, 304U, RepresentativeScreen::kTextLineHeight},
                        device_platform::ThemeToken::TextSecondary,
                        device_platform::ThemeToken::Canvas);
                std::uint16_t stateTop = 116U;
                if (message.acknowledged) {
                    addText(commands, textPacks, locale,
                            fermentationTextKey("message-acknowledged"),
                            {8U, stateTop, 304U,
                             RepresentativeScreen::kTextLineHeight},
                            device_platform::ThemeToken::StatusInformation,
                            device_platform::ThemeToken::Canvas);
                    stateTop = static_cast<std::uint16_t>(stateTop + 24U);
                }
                if (message.acousticMuted) {
                    addText(commands, textPacks, locale,
                            fermentationTextKey("message-muted"),
                            {8U, stateTop, 304U,
                             RepresentativeScreen::kTextLineHeight},
                            device_platform::ThemeToken::StatusInformation,
                            device_platform::ThemeToken::Canvas);
                }
            }
        } else if (screen.workspace.page != FermentationUiPage::Completion &&
                   screen.workspace.programSummary.has_value()) {
            drawFieldRows();
        } else if (screen.workspace.valueEdit.has_value()) {
            // Shared numeric edit page: the candidate text and the 4 x 3
            // keypad (`1 2 3 / 4 5 6 / 7 8 9 / . 0 +/-`); the actions
            // (cancel, delete, clear, ok) are the bottom slots.
            const auto& edit = *screen.workspace.valueEdit;
            const bool whole = edit.wholeNumber;
            const char* unit =
                edit.unit == FermentationUiValueUnit::Celsius   ? " C"
                : edit.unit == FermentationUiValueUnit::Seconds ? " s"
                                                                : " min";
            addRawText(commands,
                       {120U, 40U, 192U, RepresentativeScreen::kTextLineHeight},
                       edit.candidate.empty() ? std::string{"--"}
                                              : edit.candidate + unit,
                       device_platform::ThemeToken::StatusInformation,
                       device_platform::ThemeToken::Canvas);
            static constexpr const char* kKeyLabels[4][3] = {{"1", "2", "3"},
                                                             {"4", "5", "6"},
                                                             {"7", "8", "9"},
                                                             {".", "0", "+/-"}};
            for (std::uint16_t row = 0U; row < kFermentationUiKeypadRows;
                 ++row) {
                for (std::uint16_t column = 0U;
                     column < kFermentationUiKeypadColumns; ++column) {
                    const auto left = static_cast<std::uint16_t>(
                        kKeypadLeft + column * kKeypadPitchX);
                    const auto top = static_cast<std::uint16_t>(
                        kKeypadTop + row * kKeypadPitchY);
                    addFill(commands,
                            {left, top, kKeypadCellWidth, kKeypadCellHeight},
                            device_platform::ThemeToken::Surface);
                    const bool dim = whole && row == 3U && column != 1U;
                    addRawText(commands,
                               {static_cast<std::uint16_t>(left + 36U),
                                static_cast<std::uint16_t>(top + 7U), 56U,
                                RepresentativeScreen::kTextLineHeight},
                               kKeyLabels[row][column],
                               dim ? device_platform::ThemeToken::TextSecondary
                                   : device_platform::ThemeToken::TextPrimary,
                               device_platform::ThemeToken::Surface);
                }
            }
        } else if (screen.workspace.settings.has_value()) {
            // Normal settings (O1): one row per setting in the decided order;
            // the device name is a read-only copy of the owner's value and a
            // disabled row names its reason.
            const auto& settings = *screen.workspace.settings;
            const auto first = screen.workspace.pager.currentIndex;
            const auto rowCount = std::min<std::size_t>(
                kFermentationUiSettingsRowCount > first
                    ? kFermentationUiSettingsRowCount - first
                    : 0U,
                kFermentationUiListVisibleRows);
            for (std::size_t index = 0U; index < rowCount; ++index) {
                const auto row =
                    static_cast<FermentationUiSettingsRow>(first + index);
                const auto top = static_cast<std::uint16_t>(
                    kContentRowTop + index * kContentRowHeight);
                const auto textTop = static_cast<std::uint16_t>(
                    top + (kContentRowHeight -
                           RepresentativeScreen::kTextLineHeight) /
                              2U);
                const char* label = "language";
                bool lockIcon = false;
                std::string value;
                std::optional<device_platform::TextKey> valueKey;
                bool enabled = true;
                switch (row) {
                    case FermentationUiSettingsRow::Language:
                        label = "language";
                        valueKey = fermentationTextKey(
                            ("language-" + locale.value()).c_str());
                        break;
                    case FermentationUiSettingsRow::TimeZone:
                        label = "settings-time-zone";
                        break;
                    case FermentationUiSettingsRow::DeviceName:
                        label = "device-name";
                        enabled = settings.deviceNameEditable;
                        if (enabled) {
                            value = settings.deviceName;
                        } else {
                            valueKey =
                                fermentationTextKey("device-name-locked-run");
                        }
                        break;
                    case FermentationUiSettingsRow::Network:
                        label = "network";
                        break;
                    case FermentationUiSettingsRow::WebAccess:
                        label = "web-access";
                        break;
                    case FermentationUiSettingsRow::Service:
                        label = "service";
                        lockIcon = true;
                        enabled = settings.serviceAvailable;
                        if (!enabled && settings.serviceReason.has_value())
                            valueKey = *settings.serviceReason;
                        break;
                }
                addFill(commands,
                        {kContentRowLeft, top, kContentRowWidth,
                         static_cast<std::uint16_t>(kContentRowHeight - 2U)},
                        device_platform::ThemeToken::Surface);
                const auto token =
                    enabled ? device_platform::ThemeToken::TextPrimary
                            : device_platform::ThemeToken::TextSecondary;
                // The lock is the LVGL symbol, not a text-pack glyph.
                if (lockIcon) {
                    addLockIcon(commands,
                                {12U, textTop, 18U,
                                 RepresentativeScreen::kTextLineHeight},
                                token, device_platform::ThemeToken::Surface);
                }
                const auto labelLeft =
                    static_cast<std::uint16_t>(lockIcon ? 32U : 12U);
                addText(commands, textPacks, locale, fermentationTextKey(label),
                        {labelLeft, textTop,
                         static_cast<std::uint16_t>(140U - labelLeft),
                         RepresentativeScreen::kTextLineHeight},
                        token, device_platform::ThemeToken::Surface);
                if (valueKey.has_value()) {
                    addText(commands, textPacks, locale, *valueKey,
                            {144U, textTop, 164U,
                             RepresentativeScreen::kTextLineHeight},
                            device_platform::ThemeToken::TextSecondary,
                            device_platform::ThemeToken::Surface);
                } else if (!value.empty()) {
                    addRawText(commands,
                               {144U, textTop, 164U,
                                RepresentativeScreen::kTextLineHeight},
                               value,
                               device_platform::ThemeToken::StatusInformation,
                               device_platform::ThemeToken::Surface);
                }
            }
        } else if (screen.workspace.programEdit.has_value()) {
            // Program editor rows (label and value in one line, changed
            // values marked) with the pager buttons.
            const auto& edit = *screen.workspace.programEdit;
            const auto first = screen.workspace.pager.currentIndex;
            const auto rowCount = std::min<std::size_t>(
                edit.rowCount > first ? edit.rowCount - first : 0U,
                kFermentationUiListVisibleRows);
            for (std::size_t index = 0U; index < rowCount; ++index) {
                const auto& row = edit.rows[first + index];
                const auto top = static_cast<std::uint16_t>(
                    kContentRowTop + index * kContentRowHeight);
                addFill(commands,
                        {kContentRowLeft, top, kSummaryRowWidth,
                         static_cast<std::uint16_t>(kContentRowHeight - 2U)},
                        device_platform::ThemeToken::Surface);
                std::string text =
                    resolve(textPacks, locale, row.label).value +
                    (row.valueKey.has_value()
                         ? resolve(textPacks, locale, *row.valueKey).value
                         : row.text) +
                    (row.changed ? " *" : "");
                addRawText(commands,
                           {12U,
                            static_cast<std::uint16_t>(
                                top + (kContentRowHeight -
                                       RepresentativeScreen::kTextLineHeight) /
                                          2U),
                            236U, RepresentativeScreen::kTextLineHeight},
                           std::move(text),
                           row.changed
                               ? device_platform::ThemeToken::PrimaryAction
                               : device_platform::ThemeToken::TextPrimary,
                           device_platform::ThemeToken::Surface);
            }
            drawPagerButtons(edit.rowCount > kFermentationUiListVisibleRows);
        } else if (screen.workspace.textEdit.has_value()) {
            // On-screen keyboard (O3): the candidate's tail and the 4 x 10 key
            // grid of the current mode; cancel, mode, backspace and ok are the
            // bottom slots.
            const auto& edit = *screen.workspace.textEdit;
            constexpr std::size_t kTailBytes = 28U;
            std::string shown = edit.candidate;
            if (shown.size() > kTailBytes) {
                auto cut = shown.size() - kTailBytes;
                // Never cut inside a multi-byte character.
                while (cut < shown.size() &&
                       (static_cast<unsigned char>(shown[cut]) & 0xC0U) ==
                           0x80U) {
                    ++cut;
                }
                shown = ".." + shown.substr(cut);
            }
            addRawText(commands,
                       {96U, 40U, 216U, RepresentativeScreen::kTextLineHeight},
                       shown.empty() ? std::string{"_"} : shown + "_",
                       device_platform::ThemeToken::StatusInformation,
                       device_platform::ThemeToken::Canvas);
            for (std::uint8_t row = 0U; row < kFermentationUiKeyboardRows;
                 ++row) {
                std::uint8_t column = 0U;
                while (column < kFermentationUiKeyboardColumns) {
                    const auto key =
                        fermentationUiKeyboardKeyAt(edit.mode, row, column);
                    // Clear (columns 0-1) and Space (2-7) span several cells.
                    std::uint8_t span = 1U;
                    if (row == 3U) {
                        span = column == 0U ? 2U : (column == 2U ? 6U : 1U);
                    }
                    const auto left = static_cast<std::uint16_t>(
                        kKeyboardLeft + column * kKeyboardPitchX);
                    const auto top = static_cast<std::uint16_t>(
                        kKeyboardTop + row * kKeyboardPitchY);
                    const auto width =
                        static_cast<std::uint16_t>(span * kKeyboardPitchX - 2U);
                    if (key.kind != FermentationUiKeyboardKeyKind::None) {
                        addFill(commands,
                                {left, top, width, kKeyboardFaceHeight},
                                device_platform::ThemeToken::Surface);
                        const device_platform::DisplayRect labelRect{
                            static_cast<std::uint16_t>(left +
                                                       (span == 1U ? 9U : 4U)),
                            static_cast<std::uint16_t>(top + 7U),
                            static_cast<std::uint16_t>(span == 1U ? 16U
                                                                  : width - 8U),
                            RepresentativeScreen::kTextLineHeight};
                        if (key.kind == FermentationUiKeyboardKeyKind::Clear) {
                            addText(commands, textPacks, locale,
                                    fermentationTextKey("clear"), labelRect,
                                    device_platform::ThemeToken::TextPrimary,
                                    device_platform::ThemeToken::Surface);
                        } else if (key.character == ' ') {
                            addText(commands, textPacks, locale,
                                    fermentationTextKey("space"), labelRect,
                                    device_platform::ThemeToken::TextPrimary,
                                    device_platform::ThemeToken::Surface);
                        } else {
                            addRawText(commands, labelRect,
                                       std::string(1U, key.character),
                                       device_platform::ThemeToken::TextPrimary,
                                       device_platform::ThemeToken::Surface);
                        }
                    }
                    column = static_cast<std::uint8_t>(column + span);
                }
            }
        } else if (screen.workspace.page == FermentationUiPage::Process ||
                   screen.workspace.page == FermentationUiPage::Completion) {
            // Existing snapshot values only: process state and the effective
            // run values. The remaining time is the owner's value, never
            // derived from the clock.
            const auto line = [](std::uint16_t index) {
                return device_platform::DisplayRect{
                    kPageLineLeft, pageLineTop(index, 22U), kPageLineWidth,
                    RepresentativeScreen::kTextLineHeight};
            };
            const auto& values = snapshot.home.effectiveValues;
            addText(commands, textPacks, locale,
                    processStateTextKey(snapshot.home.processState), line(0U),
                    device_platform::ThemeToken::StatusInformation,
                    device_platform::ThemeToken::Canvas);
            addLabeledRawText(
                commands, textPacks, locale, "label-target",
                celsiusText(
                    values.has_value()
                        ? std::optional<double>{values
                                                    ->targetTemperatureCelsius}
                        : std::nullopt),
                line(1U), device_platform::ThemeToken::TextPrimary);
            if (screen.workspace.page == FermentationUiPage::Process) {
                addLabeledRawText(
                    commands, textPacks, locale, "label-remaining",
                    minutesText(
                        values.has_value()
                            ? std::optional<
                                  std::uint32_t>{values
                                                     ->remainingDurationMinutes}
                            : std::nullopt),
                    line(2U), device_platform::ThemeToken::TextPrimary);
            }
            // The completion page also lists its cooling-plan field.
            if (screen.workspace.programSummary.has_value()) drawFieldRows();
        } else if (screen.workspace.page == FermentationUiPage::Technical) {
            // Window over snapshot.temperatures: row r shows temperature
            // currentIndex + r (same geometry as the other lists, no touch
            // target). The value stays "--.- C" without a sensor producer.
            const auto first = screen.workspace.pager.currentIndex;
            const auto rowCount =
                std::min<std::size_t>(snapshot.temperatures.size() > first
                                          ? snapshot.temperatures.size() - first
                                          : 0U,
                                      kFermentationUiListVisibleRows);
            for (std::size_t index = 0U; index < rowCount; ++index) {
                const auto& temperature = snapshot.temperatures[first + index];
                const auto top = static_cast<std::uint16_t>(
                    kContentRowTop + index * kContentRowHeight);
                const auto textTop = static_cast<std::uint16_t>(
                    top + (kContentRowHeight -
                           RepresentativeScreen::kTextLineHeight) /
                              2U);
                addFill(commands,
                        {kContentRowLeft, top, kContentRowWidth,
                         static_cast<std::uint16_t>(kContentRowHeight - 2U)},
                        device_platform::ThemeToken::Surface);
                addText(
                    commands, textPacks, locale,
                    temperatureRoleTextKey(temperature.role),
                    {12U, textTop, 112U, RepresentativeScreen::kTextLineHeight},
                    device_platform::ThemeToken::TextPrimary,
                    device_platform::ThemeToken::Surface);
                commands.push_back(
                    {ScreenDrawKind::Text,
                     {128U, textTop, 72U,
                      RepresentativeScreen::kTextLineHeight},
                     device_platform::ThemeToken::StatusInformation,
                     device_platform::ThemeToken::Surface,
                     temperatureText(temperature),
                     {}});
                addText(commands, textPacks, locale,
                        sensorQualityTextKey(temperature.quality.quality),
                        {204U, textTop, 104U,
                         RepresentativeScreen::kTextLineHeight},
                        temperature.quality.quality ==
                                device_platform::SensorQuality::Valid
                            ? device_platform::ThemeToken::TextSecondary
                            : device_platform::ThemeToken::StatusWarning,
                        device_platform::ThemeToken::Surface);
            }
        } else if (screen.workspace.page == FermentationUiPage::Status) {
            addText(commands, textPacks, locale,
                    homeModeKey(snapshot.home.mode),
                    {kPageLineLeft, 68U, kPageLineWidth,
                     RepresentativeScreen::kTextLineHeight},
                    device_platform::ThemeToken::StatusInformation,
                    device_platform::ThemeToken::Canvas);
            addText(
                commands, textPacks, locale,
                fermentationTextKey(snapshot.status.ready ? "status-ready"
                                                          : "status-not-ready"),
                {kPageLineLeft, 90U, kPageLineWidth,
                 RepresentativeScreen::kTextLineHeight},
                snapshot.status.ready
                    ? device_platform::ThemeToken::TextPrimary
                    : device_platform::ThemeToken::StatusWarning,
                device_platform::ThemeToken::Canvas);
            if (snapshot.status.presentation.faultCode != FaultCode::None) {
                char code[8];
                std::snprintf(code, sizeof(code), "0x%04X",
                              static_cast<unsigned>(
                                  snapshot.status.presentation.faultCode));
                addLabeledRawText(commands, textPacks, locale,
                                  "label-fault-code", code,
                                  {kPageLineLeft, 112U, kPageLineWidth,
                                   RepresentativeScreen::kTextLineHeight},
                                  device_platform::ThemeToken::StatusError);
            }
        } else if (screen.workspace.page == FermentationUiPage::Recovery) {
            addText(commands, textPacks, locale,
                    recoveryModeTextKey(snapshot.recovery.mode),
                    {kPageLineLeft, 68U, kPageLineWidth,
                     RepresentativeScreen::kTextLineHeight},
                    device_platform::ThemeToken::StatusInformation,
                    device_platform::ThemeToken::Canvas);
            // The time correction has no R1 user path (plan 4.1): the reason
            // is shown as a static line, no slot is enabled for it.
            addText(commands, textPacks, locale,
                    fermentationTextKey("recovery-time-correction-unavailable"),
                    {kPageLineLeft, 90U, kPageLineWidth,
                     RepresentativeScreen::kTextLineHeight},
                    device_platform::ThemeToken::TextSecondary,
                    device_platform::ThemeToken::Canvas);
        } else if (screen.workspace.page == FermentationUiPage::Diagnostics ||
                   screen.workspace.page == FermentationUiPage::Service ||
                   screen.workspace.page == FermentationUiPage::Pin) {
            // Content is owned by #28; no function is promised here.
            addText(commands, textPacks, locale,
                    fermentationTextKey("deferred-28"),
                    {kPageLineLeft, 68U, kPageLineWidth,
                     RepresentativeScreen::kTextLineHeight},
                    device_platform::ThemeToken::TextSecondary,
                    device_platform::ThemeToken::Canvas);
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
                    {8U,
                     screen.workspace.programList.empty() &&
                             screen.workspace.page !=
                                 FermentationUiPage::HeaderLanguage &&
                             !screen.workspace.programSummary.has_value()
                         ? std::uint16_t{128U}
                         : kListReasonTop,
                     304U, RepresentativeScreen::kTextLineHeight},
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
        screen.workspace.pager.valid() &&
        screen.workspace.page != FermentationUiPage::HeaderLanguage) {
        // The n/N counter sits right in the title row so it never overlaps
        // content rows.
        addRawText(commands,
                   {248U, 40U, 64U, RepresentativeScreen::kTextLineHeight},
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
        const auto labelRect =
            screen.workspace.page == FermentationUiPage::HeaderNetwork
                ? device_platform::
                      DisplayRect{static_cast<std::uint16_t>(left + 2U),
                                  static_cast<std::uint16_t>(
                                      kControlTop +
                                      (kControlHeight -
                                       RepresentativeScreen::kTextLineHeight) /
                                          2U),
                                  76U, RepresentativeScreen::kTextLineHeight}
                : device_platform::DisplayRect{
                      static_cast<std::uint16_t>(left + 4U),
                      static_cast<std::uint16_t>(
                          kControlTop +
                          (kControlHeight -
                           RepresentativeScreen::kTextLineHeight) /
                              2U),
                      68U, RepresentativeScreen::kTextLineHeight};
        addFill(commands, {left, kControlTop, 80U, kControlHeight},
                slot.enabled ? device_platform::ThemeToken::PrimaryAction
                             : device_platform::ThemeToken::SecondaryAction);
        addText(commands, textPacks, locale, slot.label, labelRect,
                slot.enabled ? device_platform::ThemeToken::OnPrimaryAction
                             : device_platform::ThemeToken::TextSecondary,
                slot.enabled ? device_platform::ThemeToken::PrimaryAction
                             : device_platform::ThemeToken::SecondaryAction);
    }
    if (pressedTarget.has_value() &&
        pressedTarget->kind ==
            device_platform::DeviceUiTargetKind::ContentCell) {
        std::optional<device_platform::DisplayRect> pressedRect;
        if (isContentListPage(screen.workspace.page) &&
            pressedTarget->column == 0U &&
            pressedTarget->row < kFermentationUiListVisibleRows) {
            pressedRect = device_platform::DisplayRect{
                kContentRowLeft,
                static_cast<std::uint16_t>(
                    kContentRowTop + pressedTarget->row * kContentRowHeight),
                kContentRowWidth, kContentRowHeight};
        } else if (screen.workspace.page != FermentationUiPage::ValueEdit &&
                   screen.workspace.programSummary.has_value()) {
            if (pressedTarget->column == 0U &&
                pressedTarget->row < kFermentationUiListVisibleRows) {
                pressedRect = device_platform::DisplayRect{
                    kContentRowLeft,
                    static_cast<std::uint16_t>(kContentRowTop +
                                               pressedTarget->row *
                                                   kContentRowHeight),
                    kSummaryRowWidth, kContentRowHeight};
            } else if (pressedTarget->column == 1U && pressedTarget->row < 2U) {
                pressedRect = device_platform::DisplayRect{
                    kSummaryButtonLeft,
                    static_cast<std::uint16_t>(kContentRowTop +
                                               pressedTarget->row *
                                                   kSummaryButtonHeight),
                    kSummaryButtonWidth, kSummaryButtonHeight};
            }
        } else if (screen.workspace.page == FermentationUiPage::ProgramEdit) {
            if (pressedTarget->column == 0U &&
                pressedTarget->row < kFermentationUiListVisibleRows) {
                pressedRect = device_platform::DisplayRect{
                    kContentRowLeft,
                    static_cast<std::uint16_t>(kContentRowTop +
                                               pressedTarget->row *
                                                   kContentRowHeight),
                    kSummaryRowWidth, kContentRowHeight};
            } else if (pressedTarget->column == 1U && pressedTarget->row < 2U) {
                pressedRect = device_platform::DisplayRect{
                    kSummaryButtonLeft,
                    static_cast<std::uint16_t>(kContentRowTop +
                                               pressedTarget->row *
                                                   kSummaryButtonHeight),
                    kSummaryButtonWidth, kSummaryButtonHeight};
            }
        } else if (screen.workspace.page == FermentationUiPage::TextEdit &&
                   pressedTarget->row < kFermentationUiKeyboardRows &&
                   pressedTarget->column < kFermentationUiKeyboardColumns) {
            pressedRect = device_platform::DisplayRect{
                static_cast<std::uint16_t>(
                    kKeyboardLeft + pressedTarget->column * kKeyboardPitchX),
                static_cast<std::uint16_t>(kKeyboardTop + pressedTarget->row *
                                                              kKeyboardPitchY),
                kKeyboardPitchX, kKeyboardPitchY};
        } else if (screen.workspace.page == FermentationUiPage::ValueEdit &&
                   pressedTarget->row < kFermentationUiKeypadRows &&
                   pressedTarget->column < kFermentationUiKeypadColumns) {
            pressedRect = device_platform::DisplayRect{
                static_cast<std::uint16_t>(kKeypadLeft + pressedTarget->column *
                                                             kKeypadPitchX),
                static_cast<std::uint16_t>(kKeypadTop +
                                           pressedTarget->row * kKeypadPitchY),
                kKeypadCellWidth, kKeypadTouchHeight};
        }
        if (pressedRect.has_value()) {
            commands.push_back({ScreenDrawKind::PressFeedback,
                                *pressedRect,
                                device_platform::ThemeToken::SecondaryAction,
                                device_platform::ThemeToken::PrimaryAction,
                                {},
                                {}});
        }
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

std::uint64_t localeFingerprint(
    const device_platform::LocaleId& locale) noexcept {
    constexpr std::uint64_t kOffset = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    auto hash = kOffset;
    for (const auto character : locale.value()) {
        hash ^= static_cast<unsigned char>(character);
        hash *= kPrime;
    }
    return hash;
}

ScreenRenderKey makeScreenRenderKey(
    const FermentationUiSnapshot& snapshot,
    const FermentationTouchWorkspace& workspace,
    const device_platform::LocaleId& locale,
    std::optional<device_platform::DeviceUiTarget> pressedTarget,
    const FermentationUiPresentationCache& presentation,
    device_platform::DeviceUiNetworkStatus networkStatus,
    std::optional<std::int64_t> trustedUtc,
    std::uint64_t accessPointRevision) noexcept {
    ScreenRenderKey key;
    key.refreshRevision = snapshot.refreshRevision;
    key.workspaceRevision = workspace.renderRevision();
    key.page = workspace.page();
    key.catalogRevision = presentation.adoptedProgramCatalogRevision();
    key.localeFingerprint = localeFingerprint(locale);
    if (pressedTarget.has_value()) {
        key.hasPressedTarget = true;
        key.pressedKind = pressedTarget->kind;
        key.pressedSlotIndex = pressedTarget->slotIndex;
        key.pressedRow = pressedTarget->row;
        key.pressedColumn = pressedTarget->column;
    }
    key.networkStatus = networkStatus;
    if (trustedUtc.has_value()) {
        key.utcMinute = *trustedUtc / 60;
    }
    // The visible clock is the local time derived from UTC and the prepared
    // zone rule, so the rule is part of the visible inputs.
    key.timeZoneDst =
        static_cast<std::uint8_t>(presentation.timeZoneRule().dst);
    key.timeZoneOffsetMinutes =
        presentation.timeZoneRule().standardOffsetMinutes;
    key.accessPointRevision = accessPointRevision;
    return key;
}

std::optional<device_platform::DeviceUiTarget> targetAt(
    const RepresentativeScreen& screen, std::uint16_t x,
    std::uint16_t y) noexcept {
    if (x >= screen.kWidth) return std::nullopt;
    if (x >= kHeaderLanguageHitRect.left &&
        x < kHeaderLanguageHitRect.left + kHeaderLanguageHitRect.width &&
        y >= kHeaderLanguageHitRect.top &&
        y < kHeaderLanguageHitRect.top + kHeaderLanguageHitRect.height) {
        return device_platform::DeviceUiTarget{
            device_platform::DeviceUiTargetKind::HeaderLanguage, 0U};
    }
    if (x >= kHeaderNetworkRect.left &&
        x < kHeaderNetworkRect.left + kHeaderNetworkRect.width &&
        y >= kHeaderNetworkRect.top &&
        y < kHeaderNetworkRect.top + kHeaderNetworkRect.height) {
        return device_platform::DeviceUiTarget{
            device_platform::DeviceUiTargetKind::HeaderNetwork, 0U};
    }
    if (x >= kHeaderClockHitRect.left &&
        x < kHeaderClockHitRect.left + kHeaderClockHitRect.width &&
        y >= kHeaderClockHitRect.top &&
        y < kHeaderClockHitRect.top + kHeaderClockHitRect.height) {
        return device_platform::DeviceUiTarget{
            device_platform::DeviceUiTargetKind::HeaderClock, 0U};
    }
    if (screen.workspace.page != FermentationUiPage::ValueEdit &&
        screen.workspace.programSummary.has_value() && x >= kContentRowLeft &&
        y >= kContentRowTop &&
        y < kContentRowTop +
                kFermentationUiListVisibleRows * kContentRowHeight) {
        const auto& summary = *screen.workspace.programSummary;
        if (x < kContentRowLeft + kSummaryRowWidth) {
            const auto row = static_cast<std::uint8_t>((y - kContentRowTop) /
                                                       kContentRowHeight);
            if (row >= summary.rowOffset &&
                screen.workspace.pager.currentIndex +
                        static_cast<std::size_t>(row - summary.rowOffset) <
                    summary.fieldCount) {
                return device_platform::DeviceUiTarget{
                    device_platform::DeviceUiTargetKind::ContentCell, 0U, row,
                    0U};
            }
            return std::nullopt;
        }
        if (summary.pagerButtons && x >= kSummaryButtonLeft &&
            x < kSummaryButtonLeft + kSummaryButtonWidth) {
            return device_platform::DeviceUiTarget{
                device_platform::DeviceUiTargetKind::ContentCell, 0U,
                static_cast<std::uint8_t>((y - kContentRowTop) /
                                          kSummaryButtonHeight),
                1U};
        }
        return std::nullopt;
    }
    if (screen.workspace.page == FermentationUiPage::ProgramEdit &&
        screen.workspace.programEdit.has_value() && x >= kContentRowLeft &&
        y >= kContentRowTop &&
        y < kContentRowTop +
                kFermentationUiListVisibleRows * kContentRowHeight) {
        const auto& edit = *screen.workspace.programEdit;
        if (x < kContentRowLeft + kSummaryRowWidth) {
            const auto row = static_cast<std::uint8_t>((y - kContentRowTop) /
                                                       kContentRowHeight);
            if (screen.workspace.pager.currentIndex + row < edit.rowCount) {
                return device_platform::DeviceUiTarget{
                    device_platform::DeviceUiTargetKind::ContentCell, 0U, row,
                    0U};
            }
            return std::nullopt;
        }
        if (edit.rowCount > kFermentationUiListVisibleRows &&
            x >= kSummaryButtonLeft &&
            x < kSummaryButtonLeft + kSummaryButtonWidth) {
            return device_platform::DeviceUiTarget{
                device_platform::DeviceUiTargetKind::ContentCell, 0U,
                static_cast<std::uint8_t>((y - kContentRowTop) /
                                          kSummaryButtonHeight),
                1U};
        }
        return std::nullopt;
    }
    if (screen.workspace.page == FermentationUiPage::TextEdit &&
        x >= kKeyboardLeft &&
        x < kKeyboardLeft + kFermentationUiKeyboardColumns * kKeyboardPitchX &&
        y >= kKeyboardTop &&
        y < kKeyboardTop + kFermentationUiKeyboardRows * kKeyboardPitchY) {
        // Every key row is active over its whole 34 px height, every column
        // over its 30 px pitch.
        return device_platform::DeviceUiTarget{
            device_platform::DeviceUiTargetKind::ContentCell, 0U,
            static_cast<std::uint8_t>((y - kKeyboardTop) / kKeyboardPitchY),
            static_cast<std::uint8_t>((x - kKeyboardLeft) / kKeyboardPitchX)};
    }
    if (screen.workspace.page == FermentationUiPage::ValueEdit &&
        x >= kKeypadLeft && y >= kKeypadTop &&
        y < kKeypadTop + kFermentationUiKeypadRows * kKeypadPitchY) {
        const auto column =
            static_cast<std::uint8_t>((x - kKeypadLeft) / kKeypadPitchX);
        const auto row =
            static_cast<std::uint8_t>((y - kKeypadTop) / kKeypadPitchY);
        // Every row is 34 px high and active over its whole height; the
        // 4 px gap between columns is no target.
        if (column < kFermentationUiKeypadColumns &&
            (x - kKeypadLeft) % kKeypadPitchX < kKeypadCellWidth) {
            return device_platform::DeviceUiTarget{
                device_platform::DeviceUiTargetKind::ContentCell, 0U, row,
                column};
        }
        return std::nullopt;
    }
    // Visible program rows only: row r is entry currentIndex + r, so the hit
    // zone follows exactly what the renderer draws.
    if (isContentListPage(screen.workspace.page) &&
        screen.workspace.pager.itemCount > 0U &&
        screen.workspace.pager.currentIndex <
            screen.workspace.pager.itemCount &&
        x >= kContentRowLeft && x < kContentRowLeft + kContentRowWidth &&
        y >= kContentRowTop &&
        y < kContentRowTop +
                kFermentationUiListVisibleRows * kContentRowHeight) {
        const auto row =
            static_cast<std::uint8_t>((y - kContentRowTop) / kContentRowHeight);
        const auto visible =
            std::min<std::size_t>(screen.workspace.pager.itemCount -
                                      screen.workspace.pager.currentIndex,
                                  kFermentationUiListVisibleRows);
        if (row < visible) {
            return device_platform::DeviceUiTarget{
                device_platform::DeviceUiTargetKind::ContentCell, 0U, row, 0U};
        }
        return std::nullopt;
    }
    if (y < kControlTop || y >= kControlTop + kControlHeight) {
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
