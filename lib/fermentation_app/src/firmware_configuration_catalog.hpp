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
// Stable known identifier for a future build-included theme.  It is not part
// of the R1 build catalog; retaining it here lets schema-aware configuration
// reads distinguish a known-but-not-included value from a corrupt identifier.
inline constexpr const char* kKnownFutureLightThemeId = "manuengineer-light";

[[nodiscard]] bool containsLanguageId(const std::string& identifier);
[[nodiscard]] bool containsTimeZoneId(const std::string& identifier);
[[nodiscard]] bool containsThemeId(const std::string& identifier);

}  // namespace fermentation::firmware_configuration_catalog
