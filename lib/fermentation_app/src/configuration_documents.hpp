#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "actuator_plan_types.hpp"
#include "network_mode.hpp"
#include "program_model.hpp"
#include "storage_types.hpp"
#include "time_zone_resolver.hpp"

namespace fermentation {

enum class UserConfigurationSchema : std::uint8_t {
    Version1 = 1U,
    Version2 = 2U,
    Version3 = 3U,
};

inline constexpr std::uint32_t kCurrentUserConfigurationSchemaVersion =
    static_cast<std::uint32_t>(UserConfigurationSchema::Version3);
enum class ServiceConfigurationSchema : std::uint8_t {
    Version1 = 1U,
    Version2 = 2U,
};
inline constexpr std::uint32_t kCurrentServiceConfigurationSchemaVersion =
    static_cast<std::uint32_t>(ServiceConfigurationSchema::Version2);
enum class ProgramCatalogSchema : std::uint8_t { Version1 = 1U };

namespace detail {
struct UserConfigurationRevisionTag {};
struct ServiceConfigurationRevisionTag {};
struct ProgramCatalogRevisionTag {};
}  // namespace detail

using UserConfigurationRevision =
    device_platform::StrongId<detail::UserConfigurationRevisionTag,
                              std::uint64_t>;
using ServiceConfigurationRevision =
    device_platform::StrongId<detail::ServiceConfigurationRevisionTag,
                              std::uint64_t>;
using ProgramCatalogRevision =
    device_platform::StrongId<detail::ProgramCatalogRevisionTag, std::uint64_t>;

struct HomeWifiCredentials {
    std::string ssid;
    std::string password;

    friend bool operator==(const HomeWifiCredentials& left,
                           const HomeWifiCredentials& right) {
        return left.ssid == right.ssid && left.password == right.password;
    }
    friend bool operator!=(const HomeWifiCredentials& left,
                           const HomeWifiCredentials& right) {
        return !(left == right);
    }
};

struct UserConfiguration {
    UserConfiguration() = default;
    UserConfiguration(
        std::string displayLanguage, std::string timeZone, std::string name,
        std::string theme = "manuengineer-dark",
        device_platform::NetworkMode mode = device_platform::NetworkMode::HOME_WIFI,
        std::optional<HomeWifiCredentials> credentials = std::nullopt)
        : displayLanguageId(std::move(displayLanguage)),
          timeZoneId(std::move(timeZone)),
          deviceName(std::move(name)),
          activeThemeId(std::move(theme)),
          networkMode(mode),
          homeWifiCredentials(std::move(credentials)) {}

    std::string displayLanguageId;
    std::string timeZoneId;
    std::string deviceName;
    // V1 records are normalized to this stable R1 default while being read.
    // New persistent records carry this value in the V2 payload.
    std::string activeThemeId{"manuengineer-dark"};
    // Network mode is explicit and is not inferred from credentials.  V1/V2
    // records migrate to the recommended HOME_WIFI mode without credentials.
    device_platform::NetworkMode networkMode{
        device_platform::NetworkMode::HOME_WIFI};
    // Exactly one optional HOME_WIFI credential set is supported in R1.  The
    // value is never exposed by preview views, summaries, logs, or diagnostics.
    std::optional<HomeWifiCredentials> homeWifiCredentials{std::nullopt};
};

struct ServiceConfiguration {
    std::optional<ActuatorPlannerParameters> actuatorPlannerParameters;
};

struct ProgramCatalog {
    std::vector<ProgramDocument> programs;
};

enum class UserConfigurationStatus : std::uint8_t {
    Success,
    InvalidLanguageId,
    UnknownLanguageId,
    InvalidThemeId,
    UnknownThemeId,
    InvalidTimeZoneId,
    UnknownTimeZoneId,
    TimeZoneRejected,
    TimeZonePreparationFailed,
    InvalidDeviceName,
    InvalidNetworkMode,
    InvalidHomeWifiSsid,
    InvalidHomeWifiPassword,
};

struct UserConfigurationValidationResult {
    UserConfigurationStatus status{UserConfigurationStatus::InvalidLanguageId};
    std::optional<device_platform::PreparedTimeZone> preparedTimeZone;
};

[[nodiscard]] UserConfigurationValidationResult validateUserConfiguration(
    const UserConfiguration& configuration,
    const device_platform::ITimeZoneResolver& resolver);

enum class ProgramCatalogStatus : std::uint8_t {
    Success,
    InvalidProgramCount,
    InvalidFactoryCount,
    InvalidFactoryOrder,
    DuplicateProgramId,
    InvalidProgramId,
    ReservedFactoryId,
    InvalidProgramName,
    InvalidProgramNotes,
    InvalidProgramDocument,
    InvalidFactoryMarkers,
    InvalidUserMarkers,
};

[[nodiscard]] ProgramCatalogStatus validateProgramCatalog(
    const ProgramCatalog& catalog);

[[nodiscard]] ProgramCatalog makeFactoryProgramCatalog();

[[nodiscard]] bool configurationContentEquals(const UserConfiguration& left,
                                              const UserConfiguration& right);
[[nodiscard]] bool configurationContentEquals(
    const ServiceConfiguration& left, const ServiceConfiguration& right);
[[nodiscard]] bool configurationContentEquals(const ProgramDocument& left,
                                              const ProgramDocument& right);
[[nodiscard]] bool configurationContentEquals(const ProgramCatalog& left,
                                              const ProgramCatalog& right);

}  // namespace fermentation
