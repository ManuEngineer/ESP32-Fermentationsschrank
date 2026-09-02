#include "device_ui_text.hpp"

#include <algorithm>

namespace device_platform {
namespace {

const std::string* findTranslation(const std::vector<TextPackManifest>& packs,
                                   const LocaleId& locale, const TextKey& key) {
    for (const auto& pack : packs) {
        if (pack.nameSpace != key.nameSpace || pack.locale != locale) continue;
        const auto found =
            std::find_if(pack.translations.begin(), pack.translations.end(),
                         [&key](const TextTranslation& translation) {
                             return translation.key == key;
                         });
        if (found != pack.translations.end() && !found->value.empty()) {
            return &found->value;
        }
    }
    return nullptr;
}

}  // namespace

TextLookupResult resolveText(const std::vector<TextPackManifest>& packs,
                             const LocaleId& activeLocale, const TextKey& key,
                             const LocaleId& englishFallback) {
    if (const auto* active = findTranslation(packs, activeLocale, key)) {
        return {*active, TextLookupSource::ActiveLocale};
    }
    if (const auto* english = findTranslation(packs, englishFallback, key)) {
        return {*english, TextLookupSource::EnglishFallback};
    }
    return {key.visibleTechnicalKey(), TextLookupSource::VisibleTechnicalKey};
}

std::vector<TextPackManifest> composeTextPacks(
    const std::vector<TextPackManifest>& platformPacks,
    const std::vector<TextPackManifest>& applicationPacks) {
    std::vector<TextPackManifest> result;
    result.reserve(platformPacks.size() + applicationPacks.size());
    result.insert(result.end(), platformPacks.begin(), platformPacks.end());
    result.insert(result.end(), applicationPacks.begin(),
                  applicationPacks.end());
    return result;
}

std::vector<TextPackManifest> makePlatformTextPacks() {
    const TextNamespace nameSpace{"platform"};
    const auto capabilities = TextPackCapabilities{"latin-de-en-es", 48U, true};
    return {
        {nameSpace,
         LocaleId{"de"},
         capabilities,
         {{{nameSpace, "home"}, "Home"},
          {{nameSpace, "back"}, "Zurueck"},
          {{nameSpace, "status"}, "Status"},
          {{nameSpace, "service"}, "Service"},
          {{nameSpace, "unavailable"}, "Nicht verfuegbar"}}},
        {nameSpace,
         LocaleId{"en"},
         capabilities,
         {{{nameSpace, "home"}, "Home"},
          {{nameSpace, "back"}, "Back"},
          {{nameSpace, "status"}, "Status"},
          {{nameSpace, "service"}, "Service"},
          {{nameSpace, "unavailable"}, "Unavailable"}}},
        {nameSpace,
         LocaleId{"es"},
         capabilities,
         {{{nameSpace, "home"}, "Inicio"},
          {{nameSpace, "back"}, "Atras"},
          {{nameSpace, "status"}, "Estado"},
          {{nameSpace, "service"}, "Servicio"},
          {{nameSpace, "unavailable"}, "No disponible"}}},
    };
}

}  // namespace device_platform
