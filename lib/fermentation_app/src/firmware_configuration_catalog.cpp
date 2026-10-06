#include "firmware_configuration_catalog.hpp"

#include <algorithm>
#include <array>

#include "time_zone_rule.hpp"

namespace fermentation::firmware_configuration_catalog {
namespace {
constexpr std::array<const char*, 3> kLanguages{{"de", "es", "en"}};
constexpr std::array<const char*, 2> kKnownThemes{
    {kFactoryThemeId, kKnownFutureLightThemeId}};
}  // namespace

bool containsLanguageId(const std::string& identifier) {
    return std::any_of(
        kLanguages.begin(), kLanguages.end(),
        [&identifier](const auto* value) { return identifier == value; });
}

// The supported time zones are owned by device_platform::kSupportedTimeZones;
// the catalog derives from it instead of keeping a second list.
bool containsTimeZoneId(const std::string& identifier) {
    return device_platform::findTimeZoneRule(identifier).has_value();
}

bool containsThemeId(const std::string& identifier) {
    return std::any_of(
        kKnownThemes.begin(), kKnownThemes.end(),
        [&identifier](const auto* value) { return identifier == value; });
}

}  // namespace fermentation::firmware_configuration_catalog
