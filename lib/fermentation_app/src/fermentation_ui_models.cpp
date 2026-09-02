#include "fermentation_ui_models.hpp"

namespace fermentation {

device_platform::DeviceUiBuildCatalog makeFermentationR1DeviceUiBuildCatalog() {
    return {device_platform::BrandingId{"manuengineer"},
            {device_platform::BrandingId{"manuengineer"}},
            {device_platform::LocaleId{"de"}, device_platform::LocaleId{"en"},
             device_platform::LocaleId{"es"}},
            {device_platform::ThemeId{"manuengineer-dark"}},
            device_platform::LocaleId{"en"},
            device_platform::ThemeId{"manuengineer-dark"}};
}

std::vector<device_platform::ThemeDescriptor>
makeFermentationR1ThemeDescriptors() {
    using device_platform::ThemeDescriptor;
    using device_platform::ThemeId;
    using device_platform::ThemeToken;
    return {{ThemeId{"manuengineer-dark"},
             {ThemeToken::Canvas, ThemeToken::Surface,
              ThemeToken::PrimaryAction, ThemeToken::SecondaryAction,
              ThemeToken::TextPrimary, ThemeToken::TextSecondary,
              ThemeToken::StatusInformation, ThemeToken::StatusWarning,
              ThemeToken::StatusError, ThemeToken::Overlay}}};
}

device_platform::ServiceSessionPolicy fermentationTouchServicePolicy() {
    return {10U * 60U * 1000U, std::nullopt};
}

device_platform::ServiceSessionPolicy fermentationWebServicePolicy() {
    return {5U * 60U * 1000U, 15U * 60U * 1000U};
}

}  // namespace fermentation
