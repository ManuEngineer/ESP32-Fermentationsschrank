#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "device_ui_hardware_ports.hpp"
#include "device_ui_interaction.hpp"
#include "device_ui_shell.hpp"
#include "device_ui_theme.hpp"
#include "device_ui_text.hpp"
#include "network_lifecycle.hpp"
#include "fermentation_touch_workspace.hpp"
#include "fermentation_ui_presentation_cache.hpp"
#include "fermentation_ui_text.hpp"

namespace fermentation::main_ui {

enum class ScreenDrawKind : std::uint8_t {
    Fill,
    Text,
    NetworkStatusIcon,
    LockIcon,
    QrCode,
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
    bool wrapText{false};
};

struct RepresentativeScreen {
    static constexpr std::uint16_t kWidth = 320U;
    static constexpr std::uint16_t kHeight = 240U;
    // R1 uses LVGL's Montserrat 14 default font (16 px line height). The
    // extra two pixels keep glyphs clear of the label clip boundary.
    static constexpr std::uint16_t kTextLineHeight = 18U;

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
    std::uint64_t localNetworkInfoFingerprint{0U};
    FermentationUiWorkspaceView workspace;
    std::optional<device_platform::UiRefreshRevision> refreshRevision;
    std::optional<device_platform::DeviceUiTarget> pressedTarget;
    std::vector<ScreenDrawCommand> commands;
};

[[nodiscard]] std::uint16_t themeColor565(
    device_platform::ThemeToken token) noexcept;

// Standard WLAN QR payload for locally joining the active protected SoftAP.
// This renderer-only projection has no URL/IP input and is never added to the
// general application UI snapshot.
[[nodiscard]] std::optional<std::string> makeSoftApWifiQrPayload(
    const device_platform::NetworkAccessPointInfo& accessPoint);

// pressedTarget reflects only the existing #26 interaction result
// (DeviceUiInteractionResult::visiblePressFeedback) for the currently held
// touch contact, supplied by the caller. It is not a second renderer-owned
// press state: with no touch held (the default and, until persisted touch
// calibration is available, the only reachable production value), no
// PressFeedback command is drawn.
[[nodiscard]] RepresentativeScreen makeRepresentativeScreen(
    const FermentationUiSnapshot& snapshot,
    FermentationTouchWorkspace& workspace,
    const std::vector<device_platform::TextPackManifest>& textPacks,
    const device_platform::LocaleId& locale,
    std::optional<device_platform::DeviceUiTarget> pressedTarget = std::nullopt,
    const ProgramCatalog* catalog = nullptr,
    device_platform::DeviceUiNetworkStatus networkStatus =
        device_platform::DeviceUiNetworkStatus::Unavailable,
    device_platform::ClockViewInput clock = {},
    const std::optional<device_platform::NetworkAccessPointInfo>&
        networkAccessPointInfo = std::nullopt);

// Allocation-free identity of every visible input of render(), formed
// BEFORE any screen model is built: if it equals the key of the last
// successful render nothing visible changed and no model is built. It holds
// only revisions, enum/index values, flags and hashes (no strings), so forming,
// storing and comparing it never allocates. It must be a superset of the
// visible inputs:
//  - application state: snapshot.refreshRevision (published by the tracker
//    from the full semantic snapshot comparison);
//  - local workspace state (page, pager, dialog, selections, bottom slots):
//    FermentationTouchWorkspace::renderRevision(), bumped by every mutator,
//    plus the current page;
//  - program catalog: the revision adopted by the presentation cache, absent
//    while no valid copy exists (HeaderNetwork, unavailable fill);
//  - locale: hash of the locale actually used for drawing;
//  - pressed target (kind, slot, row and column), network status, trusted UTC
//  at display
//    resolution (the clock text is the local HH:MM, minute granularity; the
//    prepared zone rule that derives it is part of the key) and the
//    network lifecycle's access-point change revision (only on HeaderNetwork;
//    it changes exactly when SSID, password or IPv4 address change or the
//    data is set or cleared, and is read without copying the secrets).
struct ScreenRenderKey {
    std::optional<device_platform::UiRefreshRevision> refreshRevision;
    std::uint32_t workspaceRevision{0U};
    FermentationUiPage page{FermentationUiPage::Home};
    std::optional<ProgramCatalogRevision> catalogRevision;
    std::uint64_t localeFingerprint{0U};
    bool hasPressedTarget{false};
    device_platform::DeviceUiTargetKind pressedKind{
        device_platform::DeviceUiTargetKind::None};
    std::uint8_t pressedSlotIndex{0U};
    std::uint8_t pressedRow{0U};
    std::uint8_t pressedColumn{0U};
    device_platform::DeviceUiNetworkStatus networkStatus{
        device_platform::DeviceUiNetworkStatus::Unavailable};
    std::optional<std::int64_t> utcMinute;
    std::uint8_t timeZoneDst{0U};
    std::int16_t timeZoneOffsetMinutes{0};
    std::uint64_t accessPointRevision{0U};

    friend bool operator==(const ScreenRenderKey& left,
                           const ScreenRenderKey& right) noexcept {
        return left.refreshRevision == right.refreshRevision &&
               left.workspaceRevision == right.workspaceRevision &&
               left.page == right.page &&
               left.catalogRevision == right.catalogRevision &&
               left.localeFingerprint == right.localeFingerprint &&
               left.hasPressedTarget == right.hasPressedTarget &&
               left.pressedKind == right.pressedKind &&
               left.pressedSlotIndex == right.pressedSlotIndex &&
               left.pressedRow == right.pressedRow &&
               left.pressedColumn == right.pressedColumn &&
               left.networkStatus == right.networkStatus &&
               left.utcMinute == right.utcMinute &&
               left.timeZoneDst == right.timeZoneDst &&
               left.timeZoneOffsetMinutes == right.timeZoneOffsetMinutes &&
               left.accessPointRevision == right.accessPointRevision;
    }
    friend bool operator!=(const ScreenRenderKey& left,
                           const ScreenRenderKey& right) noexcept {
        return !(left == right);
    }
};

[[nodiscard]] std::uint64_t localeFingerprint(
    const device_platform::LocaleId& locale) noexcept;

[[nodiscard]] ScreenRenderKey makeScreenRenderKey(
    const FermentationUiSnapshot& snapshot,
    const FermentationTouchWorkspace& workspace,
    const device_platform::LocaleId& locale,
    std::optional<device_platform::DeviceUiTarget> pressedTarget,
    const FermentationUiPresentationCache& presentation,
    device_platform::DeviceUiNetworkStatus networkStatus,
    std::optional<std::int64_t> trustedUtc,
    std::uint64_t accessPointRevision) noexcept;

[[nodiscard]] std::optional<device_platform::DeviceUiTarget> targetAt(
    const RepresentativeScreen& screen, std::uint16_t x,
    std::uint16_t y) noexcept;

[[nodiscard]] FermentationUiWorkspacePress routePress(
    FermentationTouchWorkspace& workspace,
    const FermentationUiSnapshot& snapshot, const RepresentativeScreen& screen,
    std::uint16_t x, std::uint16_t y, const ProgramCatalog* catalog = nullptr);

}  // namespace fermentation::main_ui
