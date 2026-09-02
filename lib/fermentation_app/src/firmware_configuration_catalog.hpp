#pragma once

#include <cstdint>
#include <string>

namespace fermentation::firmware_configuration_catalog {

inline constexpr std::uint32_t kLanguageCatalogVersion = 1U;
inline constexpr std::uint32_t kTimeZoneCatalogVersion = 1U;
inline constexpr std::uint32_t kThemeCatalogVersion = 1U;

inline constexpr const char* kFactoryLanguageId = "de";
inline constexpr const char* kFactoryTimeZoneId = "Europe/Zurich";
inline constexpr const char* kFactoryThemeId = "manuengineer-dark";

[[nodiscard]] bool containsLanguageId(const std::string& identifier);
[[nodiscard]] bool containsTimeZoneId(const std::string& identifier);
[[nodiscard]] bool containsThemeId(const std::string& identifier);

}  // namespace fermentation::firmware_configuration_catalog
