#include "device_ui_theme.hpp"

#include <algorithm>

namespace device_platform {

bool isCompleteTheme(const ThemeDescriptor& descriptor) {
    if (descriptor.id.empty()) return false;
    constexpr ThemeToken required[] = {
        ThemeToken::Canvas,          ThemeToken::Surface,
        ThemeToken::PrimaryAction,   ThemeToken::SecondaryAction,
        ThemeToken::TextPrimary,     ThemeToken::TextSecondary,
        ThemeToken::StatusInformation, ThemeToken::StatusWarning,
        ThemeToken::StatusError,     ThemeToken::Overlay,
    };
    return std::all_of(
        std::begin(required), std::end(required),
        [&descriptor](ThemeToken requiredToken) {
            return std::find(descriptor.declaredTokens.begin(),
                             descriptor.declaredTokens.end(),
                             requiredToken) != descriptor.declaredTokens.end();
        });
}

ThemeResolution resolveTheme(const std::vector<ThemeDescriptor>& includedThemes,
                             const DeviceUiBuildCatalog& catalog,
                             const ThemeId& requestedTheme) {
    const auto findComplete = [&includedThemes, &catalog](
                                  const ThemeId& id) -> const ThemeDescriptor* {
        if (!catalog.contains(id)) return nullptr;
        const auto found = std::find_if(
            includedThemes.begin(), includedThemes.end(),
            [&id](const ThemeDescriptor& candidate) {
                return candidate.id == id;
            });
        return found != includedThemes.end() && isCompleteTheme(*found)
                   ? &*found
                   : nullptr;
    };
    if (const auto* requested = findComplete(requestedTheme)) {
        return {requested, ThemeResolutionSource::RequestedTheme};
    }
    if (const auto* fallback = findComplete(catalog.defaultTheme)) {
        return {fallback, ThemeResolutionSource::DefaultThemeFallback};
    }
    return {};
}

}  // namespace device_platform
