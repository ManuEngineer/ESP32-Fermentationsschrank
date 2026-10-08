#include "device_ui_text.hpp"

#include <algorithm>

namespace device_platform {
namespace {

const std::string_view* findTranslation(
    const std::vector<TextPackManifest>& packs, const LocaleId& locale,
    const TextKey& key) {
    for (const auto& pack : packs) {
        if (pack.nameSpace != key.nameSpace || pack.locale != locale) continue;
        const auto found =
            std::find_if(pack.translations.begin(), pack.translations.end(),
                         [&key](const TextTranslation& translation) {
                             return translation.key == key.value;
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
        return {std::string{*active}, TextLookupSource::ActiveLocale};
    }
    if (const auto* english = findTranslation(packs, englishFallback, key)) {
        return {std::string{*english}, TextLookupSource::EnglishFallback};
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
    static constexpr std::array<TextTranslation, 5U> kGerman{{
        {"home", "Home"},
        {"back", "Zurueck"},
        {"status", "Status"},
        {"service", "Service"},
        {"unavailable", "Nicht verfuegbar"},
    }};
    static constexpr std::array<TextTranslation, 5U> kEnglish{{
        {"home", "Home"},
        {"back", "Back"},
        {"status", "Status"},
        {"service", "Service"},
        {"unavailable", "Unavailable"},
    }};
    static constexpr std::array<TextTranslation, 5U> kSpanish{{
        {"home", "Inicio"},
        {"back", "Atras"},
        {"status", "Estado"},
        {"service", "Servicio"},
        {"unavailable", "No disponible"},
    }};
    const TextNamespace nameSpace{"platform"};
    const auto capabilities = TextPackCapabilities{"latin-de-en-es", 48U, true};
    return {
        {nameSpace, LocaleId{"de"}, capabilities, kGerman},
        {nameSpace, LocaleId{"en"}, capabilities, kEnglish},
        {nameSpace, LocaleId{"es"}, capabilities, kSpanish},
    };
}

}  // namespace device_platform
