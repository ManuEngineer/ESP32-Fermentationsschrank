#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "device_ui_contracts.hpp"

namespace device_platform {

enum class ThemeToken : std::uint8_t {
    Canvas,
    Surface,
    PrimaryAction,
    SecondaryAction,
    TextPrimary,
    TextSecondary,
    StatusInformation,
    StatusWarning,
    StatusError,
    Overlay,
};

struct ThemeDescriptor {
    ThemeId id;
    std::vector<ThemeToken> declaredTokens;
};

enum class ThemeResolutionSource : std::uint8_t {
    RequestedTheme,
    DefaultThemeFallback,
    Unavailable,
};

struct ThemeResolution {
    const ThemeDescriptor* descriptor{nullptr};
    ThemeResolutionSource source{ThemeResolutionSource::Unavailable};
};

[[nodiscard]] bool isCompleteTheme(const ThemeDescriptor& descriptor);
[[nodiscard]] ThemeResolution resolveTheme(
    const std::vector<ThemeDescriptor>& includedThemes,
    const DeviceUiBuildCatalog& catalog, const ThemeId& requestedTheme);

}  // namespace device_platform
