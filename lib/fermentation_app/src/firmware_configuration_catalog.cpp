#include "firmware_configuration_catalog.hpp"

#include <algorithm>
#include <array>

namespace fermentation::firmware_configuration_catalog {
namespace {
constexpr std::array<const char*, 3> kLanguages{{"de", "es", "en"}};
constexpr std::array<const char*, 1> kTimeZones{{"Europe/Zurich"}};
constexpr std::array<const char*, 2> kKnownThemes{
    {kFactoryThemeId, kKnownFutureLightThemeId}};
}  // namespace

bool containsLanguageId(const std::string& identifier) {
    return std::any_of(
        kLanguages.begin(), kLanguages.end(),
        [&identifier](const auto* value) { return identifier == value; });
}

bool containsTimeZoneId(const std::string& identifier) {
    return std::any_of(
        kTimeZones.begin(), kTimeZones.end(),
        [&identifier](const auto* value) { return identifier == value; });
}

bool containsThemeId(const std::string& identifier) {
    return std::any_of(
        kKnownThemes.begin(), kKnownThemes.end(),
        [&identifier](const auto* value) { return identifier == value; });
}

}  // namespace fermentation::firmware_configuration_catalog
