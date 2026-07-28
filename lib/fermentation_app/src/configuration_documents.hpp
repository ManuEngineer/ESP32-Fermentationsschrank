#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "program_model.hpp"
#include "storage_types.hpp"
#include "time_zone_resolver.hpp"

namespace fermentation {

enum class UserConfigurationSchema : std::uint8_t { Version1 = 1U };
enum class ServiceConfigurationSchema : std::uint8_t { Version1 = 1U };
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

struct UserConfiguration {
    std::string displayLanguageId;
    std::string timeZoneId;
    std::string deviceName;
};

struct ServiceConfiguration {};

struct ProgramCatalog {
    std::vector<ProgramDocument> programs;
};

enum class UserConfigurationStatus : std::uint8_t {
    Success,
    InvalidLanguageId,
    UnknownLanguageId,
    InvalidTimeZoneId,
    UnknownTimeZoneId,
    TimeZoneRejected,
    TimeZonePreparationFailed,
    InvalidDeviceName,
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
