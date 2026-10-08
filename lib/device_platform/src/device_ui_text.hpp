#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "device_ui_contracts.hpp"

namespace device_platform {

struct TextPackCapabilities {
    std::string declaredCharacterSet;
    std::size_t maximumTextLength{0U};
    bool suitableFor320x240{false};
};

// One firmware-owned translation. Key and value are non-owning views and can
// only be built from string literals (static storage duration), so the text
// data lives in read-only flash and never as a heap copy. The namespace is
// held once by the pack.
struct TextTranslation {
    std::string_view key;
    std::string_view value;

    constexpr TextTranslation() noexcept = default;
    template <std::size_t KeySize, std::size_t ValueSize>
    constexpr TextTranslation(const char (&keyLiteral)[KeySize],
                              const char (&valueLiteral)[ValueSize]) noexcept
        : key(keyLiteral, KeySize - 1U), value(valueLiteral, ValueSize - 1U) {}
};

// Non-owning view of an immutable translation table with static storage
// duration. A temporary table is rejected at compile time.
class TextTranslationTable {
   public:
    constexpr TextTranslationTable() noexcept = default;
    template <std::size_t Count>
    constexpr TextTranslationTable(
        const std::array<TextTranslation, Count>& table) noexcept
        : first_(table.data()), count_(Count) {}
    template <std::size_t Count>
    TextTranslationTable(const std::array<TextTranslation, Count>&&) = delete;

    [[nodiscard]] constexpr const TextTranslation* begin() const noexcept {
        return first_;
    }
    [[nodiscard]] constexpr const TextTranslation* end() const noexcept {
        return first_ + count_;
    }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return count_; }

   private:
    const TextTranslation* first_{nullptr};
    std::size_t count_{0U};
};

struct TextPackManifest {
    TextNamespace nameSpace;
    LocaleId locale;
    TextPackCapabilities capabilities;
    TextTranslationTable translations;
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
