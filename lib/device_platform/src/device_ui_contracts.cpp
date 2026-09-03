#include "device_ui_contracts.hpp"

#include <algorithm>

namespace device_platform {

std::string TextKey::visibleTechnicalKey() const {
    if (!valid()) return "ui.invalid-key";
    return nameSpace.value() + "." + value;
}

namespace {

template <typename Id>
bool containsId(const std::vector<Id>& values, const Id& wanted) {
    return std::find(values.begin(), values.end(), wanted) != values.end();
}

}  // namespace

bool DeviceUiBuildCatalog::contains(const BrandingId& branding) const {
    return containsId(includedBrandings, branding);
}

bool DeviceUiBuildCatalog::contains(const LocaleId& locale) const {
    return containsId(includedLocales, locale);
}

bool DeviceUiBuildCatalog::contains(const ThemeId& theme) const {
    return containsId(includedThemes, theme);
}

bool DeviceUiBuildCatalog::valid() const {
    return !activeBranding.empty() && contains(activeBranding) &&
           !englishFallback.empty() && contains(englishFallback) &&
           !defaultTheme.empty() && contains(defaultTheme);
}

DeviceUiCommandOutcomeCategory safeOutcomeCategory(
    DeviceUiCommandOutcomeCategory category) noexcept {
    switch (category) {
        case DeviceUiCommandOutcomeCategory::Accepted:
        case DeviceUiCommandOutcomeCategory::Rejected:
        case DeviceUiCommandOutcomeCategory::ConfirmationRequired:
        case DeviceUiCommandOutcomeCategory::Busy:
        case DeviceUiCommandOutcomeCategory::Unavailable:
            return category;
    }
    return DeviceUiCommandOutcomeCategory::Unavailable;
}

}  // namespace device_platform
