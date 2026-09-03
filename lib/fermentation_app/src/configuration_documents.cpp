#include "configuration_documents.hpp"

#include <algorithm>
#include <array>
#include <set>

#include "configuration_limits.hpp"
#include "configuration_text.hpp"
#include "firmware_configuration_catalog.hpp"
#include "standard_program_catalog.hpp"

namespace fermentation {
namespace {

bool isReservedFactoryId(const std::string& identifier,
                         const std::array<ProgramDocument, 4>& factory) {
    return std::any_of(factory.begin(), factory.end(),
                       [&identifier](const auto& document) {
                           return identifier == document.program.id;
                       });
}

ProgramCatalogStatus validateProgramContents(const ProgramDocument& document) {
    using namespace configuration_limits;
    const auto& program = document.program;
    if (validateLowercaseIdentifier(program.id, kMinimumProgramIdBytes,
                                    kMaximumProgramIdBytes) !=
        ConfigurationTextStatus::Success) {
        return ProgramCatalogStatus::InvalidProgramId;
    }
    if (validateVisibleName(program.name) != ConfigurationTextStatus::Success) {
        return ProgramCatalogStatus::InvalidProgramName;
    }
    if (validateProgramNotes(program.notes) !=
        ConfigurationTextStatus::Success) {
        return ProgramCatalogStatus::InvalidProgramNotes;
    }
    if (!validateProgram(document, ValidationPurpose::CatalogTemplate)
             .valid()) {
        return ProgramCatalogStatus::InvalidProgramDocument;
    }
    return ProgramCatalogStatus::Success;
}

ProgramCatalogStatus validateProgramPosition(
    const ProgramDefinition& program, std::size_t index,
    const std::array<ProgramDocument, 4>& factory) {
    if (index < configuration_limits::kFactoryProgramCount) {
        if (program.id != factory[index].program.id) {
            return ProgramCatalogStatus::InvalidFactoryOrder;
        }
        if (!program.builtIn || !program.factoryCatalogEntry ||
            !program.resettable) {
            return ProgramCatalogStatus::InvalidFactoryMarkers;
        }
        return ProgramCatalogStatus::Success;
    }
    if (program.builtIn || program.factoryCatalogEntry || program.resettable) {
        return ProgramCatalogStatus::InvalidUserMarkers;
    }
    if (isReservedFactoryId(program.id, factory)) {
        return ProgramCatalogStatus::ReservedFactoryId;
    }
    return ProgramCatalogStatus::Success;
}

}  // namespace

UserConfigurationValidationResult validateUserConfiguration(
    const UserConfiguration& configuration,
    const device_platform::ITimeZoneResolver& resolver) {
    using namespace configuration_limits;
    if (validateLowercaseIdentifier(
            configuration.displayLanguageId, kMinimumLanguageIdBytes,
            kMaximumLanguageIdBytes) != ConfigurationTextStatus::Success) {
        return {UserConfigurationStatus::InvalidLanguageId, std::nullopt};
    }
    if (!firmware_configuration_catalog::containsLanguageId(
            configuration.displayLanguageId)) {
        return {UserConfigurationStatus::UnknownLanguageId, std::nullopt};
    }
    if (validateLowercaseIdentifier(
            configuration.activeThemeId, kMinimumThemeIdBytes,
            kMaximumThemeIdBytes) != ConfigurationTextStatus::Success) {
        return {UserConfigurationStatus::InvalidThemeId, std::nullopt};
    }
    if (!firmware_configuration_catalog::containsThemeId(
            configuration.activeThemeId)) {
        return {UserConfigurationStatus::UnknownThemeId, std::nullopt};
    }
    if (validateTimeZoneIdentifierStructure(configuration.timeZoneId) !=
        ConfigurationTextStatus::Success) {
        return {UserConfigurationStatus::InvalidTimeZoneId, std::nullopt};
    }
    if (!firmware_configuration_catalog::containsTimeZoneId(
            configuration.timeZoneId)) {
        return {UserConfigurationStatus::UnknownTimeZoneId, std::nullopt};
    }
    if (validateVisibleName(configuration.deviceName) !=
        ConfigurationTextStatus::Success) {
        return {UserConfigurationStatus::InvalidDeviceName, std::nullopt};
    }
    auto prepared = resolver.prepare(configuration.timeZoneId);
    if (prepared.status ==
        device_platform::TimeZonePrepareStatus::UnsupportedIdentifier) {
        return {UserConfigurationStatus::TimeZoneRejected, std::nullopt};
    }
    if (prepared.status != device_platform::TimeZonePrepareStatus::Success ||
        !prepared.prepared.has_value() ||
        prepared.prepared->canonicalIdentifier != configuration.timeZoneId) {
        return {UserConfigurationStatus::TimeZonePreparationFailed,
                std::nullopt};
    }
    return {UserConfigurationStatus::Success, std::move(prepared.prepared)};
}

ProgramCatalogStatus validateProgramCatalog(const ProgramCatalog& catalog) {
    using namespace configuration_limits;
    if (catalog.programs.size() < kFactoryProgramCount ||
        catalog.programs.size() > kMaximumProgramCount) {
        return ProgramCatalogStatus::InvalidProgramCount;
    }
    const auto factory = FactoryProgramCatalog::programs();
    std::size_t factoryCount = 0U;
    std::set<std::string> identifiers;
    for (std::size_t index = 0U; index < catalog.programs.size(); ++index) {
        const auto& document = catalog.programs[index];
        const auto& program = document.program;
        const auto positionStatus =
            validateProgramPosition(program, index, factory);
        if (positionStatus != ProgramCatalogStatus::Success) {
            return positionStatus;
        }
        if (!identifiers.insert(program.id).second) {
            return ProgramCatalogStatus::DuplicateProgramId;
        }
        const auto contentsStatus = validateProgramContents(document);
        if (contentsStatus != ProgramCatalogStatus::Success) {
            return contentsStatus;
        }
        if (program.factoryCatalogEntry) {
            ++factoryCount;
        }
    }
    if (factoryCount != kFactoryProgramCount) {
        return ProgramCatalogStatus::InvalidFactoryCount;
    }
    return ProgramCatalogStatus::Success;
}

ProgramCatalog makeFactoryProgramCatalog() {
    const auto factory = FactoryProgramCatalog::programs();
    return ProgramCatalog{
        std::vector<ProgramDocument>(factory.begin(), factory.end())};
}

bool configurationContentEquals(const UserConfiguration& left,
                                const UserConfiguration& right) {
    return left.displayLanguageId == right.displayLanguageId &&
           left.timeZoneId == right.timeZoneId &&
           left.deviceName == right.deviceName &&
           left.activeThemeId == right.activeThemeId;
}

bool configurationContentEquals(const ServiceConfiguration& /*left*/,
                                const ServiceConfiguration& /*right*/) {
    return true;
}

bool configurationContentEquals(const ProgramDocument& left,
                                const ProgramDocument& right) {
    const auto& a = left.program;
    const auto& b = right.program;
    if (left.schema.version != right.schema.version ||
        left.schema.presentFields != right.schema.presentFields ||
        a.id != b.id || a.name != b.name || a.notes != b.notes ||
        a.builtIn != b.builtIn ||
        a.factoryCatalogEntry != b.factoryCatalogEntry ||
        a.resettable != b.resettable || a.userDeletable != b.userDeletable ||
        a.installed != b.installed || a.enabled != b.enabled ||
        a.preheat != b.preheat || a.sensorPreference != b.sensorPreference ||
        a.productSensorFailure.policy != b.productSensorFailure.policy ||
        a.productSensorFailure.fallbackDelaySeconds !=
            b.productSensorFailure.fallbackDelaySeconds ||
        a.fermentationStages.size() != b.fermentationStages.size() ||
        a.targetQualification.bandCelsius !=
            b.targetQualification.bandCelsius ||
        a.targetQualification.durationMinutes !=
            b.targetQualification.durationMinutes ||
        a.maximumTargetReachMinutes != b.maximumTargetReachMinutes ||
        a.maximumProductWaitMinutes != b.maximumProductWaitMinutes ||
        a.completion.mode != b.completion.mode ||
        a.completion.coolingTargetCelsius !=
            b.completion.coolingTargetCelsius ||
        a.completion.holdDurationMinutes != b.completion.holdDurationMinutes) {
        return false;
    }
    for (std::size_t index = 0U; index < a.fermentationStages.size(); ++index) {
        if (a.fermentationStages[index].targetTemperatureCelsius !=
                b.fermentationStages[index].targetTemperatureCelsius ||
            a.fermentationStages[index].durationMinutes !=
                b.fermentationStages[index].durationMinutes) {
            return false;
        }
    }
    return true;
}

bool configurationContentEquals(const ProgramCatalog& left,
                                const ProgramCatalog& right) {
    if (left.programs.size() != right.programs.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < left.programs.size(); ++index) {
        if (!configurationContentEquals(left.programs[index],
                                        right.programs[index])) {
            return false;
        }
    }
    return true;
}

}  // namespace fermentation
