#pragma once

#include <string>
#include <vector>

#include "device_ui_contracts.hpp"

namespace device_platform {

struct TextPackCapabilities {
    std::string declaredCharacterSet;
    std::size_t maximumTextLength{0U};
    bool suitableFor320x240{false};
};

struct TextTranslation {
    TextKey key;
    std::string value;
};

struct TextPackManifest {
    TextNamespace nameSpace;
    LocaleId locale;
    TextPackCapabilities capabilities;
    std::vector<TextTranslation> translations;
};

enum class TextLookupSource : std::uint8_t {
    ActiveLocale,
    EnglishFallback,
    VisibleTechnicalKey,
};

struct TextLookupResult {
    std::string value;
    TextLookupSource source{TextLookupSource::VisibleTechnicalKey};
};

[[nodiscard]] TextLookupResult resolveText(
    const std::vector<TextPackManifest>& packs, const LocaleId& activeLocale,
    const TextKey& key, const LocaleId& englishFallback = LocaleId{"en"});

[[nodiscard]] std::vector<TextPackManifest> composeTextPacks(
    const std::vector<TextPackManifest>& platformPacks,
    const std::vector<TextPackManifest>& applicationPacks);

[[nodiscard]] std::vector<TextPackManifest> makePlatformTextPacks();

}  // namespace device_platform
