#include "program_model.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace fermentation {
namespace {

constexpr double kMinimumFermentationTemperatureCelsius = 4.0;
constexpr double kMaximumFermentationTemperatureCelsius = 45.0;
constexpr std::uint32_t kMinimumFermentationDurationMinutes = 1U;
constexpr std::uint32_t kMaximumFermentationDurationMinutes = 20160U;
constexpr double kMinimumQualificationBandCelsius = 0.1;
constexpr double kMaximumQualificationBandCelsius = 2.0;
constexpr std::uint32_t kMaximumFallbackDelaySeconds = 3600U;
constexpr double kMinimumCoolingTargetCelsius = 4.0;
constexpr double kMaximumCoolingTargetCelsius = 25.0;

struct RequiredField {
    ProgramField field;
    const char* name;
};

constexpr std::array<RequiredField, 15> kRequiredFields{{
    {ProgramField::Id, "id"},
    {ProgramField::Name, "name"},
    {ProgramField::BuiltIn, "built_in"},
    {ProgramField::FactoryCatalogEntry, "factory_catalog_entry"},
    {ProgramField::Resettable, "resettable"},
    {ProgramField::UserDeletable, "user_deletable"},
    {ProgramField::Installed, "installed"},
    {ProgramField::Enabled, "enabled"},
    {ProgramField::Preheat, "defaults.preheat"},
    {ProgramField::SensorPreference, "defaults.sensor_preference"},
    {ProgramField::ProductSensorFailure, "defaults.product_sensor_failure"},
    {ProgramField::FermentationStages, "defaults.fermentation_stages"},
    {ProgramField::TargetQualification, "defaults.target_qualification"},
    {ProgramField::MaximumTargetReach, "defaults.max_target_reach_min"},
    {ProgramField::Completion, "defaults.completion"},
}};

// Haelt kRequiredFields synchron mit den in program_model.hpp deklarierten
// Feldern: Wird ein ProgramField ergaenzt oder aus kCurrentKnownProgramFields
// entfernt, ohne kRequiredFields anzupassen, schlaegt der Build fehl statt
// die Pruefung stillschweigend luecken zu lassen.
static_assert(kRequiredFields.size() ==
              static_cast<std::size_t>(kCurrentProgramFieldCount));

void addError(ValidationResult& result, ValidationErrorCode code,
              const char* field) {
    result.errors.push_back({code, field});
}

bool inRange(double value, double minimum, double maximum) {
    return std::isfinite(value) && value >= minimum && value <= maximum;
}

void validateOptionalDouble(ValidationResult& result,
                            const std::optional<double>& value,
                            const char* field, double minimum, double maximum,
                            ValidationPurpose purpose) {
    if (!value.has_value()) {
        if (purpose == ValidationPurpose::Runnable) {
            addError(result, ValidationErrorCode::MissingCommissioningValue,
                     field);
        }
        return;
    }
    if (!inRange(*value, minimum, maximum)) {
        addError(result, ValidationErrorCode::ValueOutOfRange, field);
    }
}

void validateOptionalDuration(ValidationResult& result,
                              const std::optional<std::uint32_t>& value,
                              const char* field, std::uint32_t minimum,
                              std::uint32_t maximum,
                              ValidationPurpose purpose) {
    if (!value.has_value()) {
        if (purpose == ValidationPurpose::Runnable) {
            addError(result, ValidationErrorCode::MissingCommissioningValue,
                     field);
        }
        return;
    }
    if (*value < minimum || *value > maximum) {
        addError(result, ValidationErrorCode::ValueOutOfRange, field);
    }
}

bool hasCooling(CompletionMode mode) {
    return mode != CompletionMode::FinishWithoutCooling;
}

bool validSensorPreference(SensorPreference preference) {
    switch (preference) {
        case SensorPreference::ProductIfAvailableElseAir:
        case SensorPreference::AirProductOptional:
        case SensorPreference::ProductRequired:
        case SensorPreference::AirOnly:
            return true;
    }
    return false;
}

bool validFailurePolicy(ProductSensorFailurePolicy policy) {
    switch (policy) {
        case ProductSensorFailurePolicy::FallbackToAirAfterTimeout:
        case ProductSensorFailurePolicy::WaitForUser:
        case ProductSensorFailurePolicy::StopToSafeState:
            return true;
    }
    return false;
}

bool validCompletionMode(CompletionMode mode) {
    switch (mode) {
        case CompletionMode::FinishWithoutCooling:
        case CompletionMode::CoolThenFinish:
        case CompletionMode::CoolAndHoldForDuration:
        case CompletionMode::CoolAndHoldUntilManualStop:
            return true;
    }
    return false;
}

}  // namespace

ValidationResult validateProgram(const ProgramDocument& document,
                                 ValidationPurpose purpose) {
    ValidationResult result;

    if (document.schema.version != kCurrentProgramSchemaVersion) {
        addError(result, ValidationErrorCode::UnsupportedSchemaVersion,
                 "schema_version");
        return result;
    }

    for (const auto& required : kRequiredFields) {
        if ((document.schema.presentFields & fieldMask(required.field)) == 0U) {
            addError(result, ValidationErrorCode::MissingRequiredField,
                     required.name);
        }
    }
    if ((document.schema.presentFields & ~kCurrentKnownProgramFields) != 0U) {
        addError(result, ValidationErrorCode::UnknownField, "program");
    }

    const auto& program = document.program;
    if (program.id.empty()) {
        addError(result, ValidationErrorCode::EmptyId, "id");
    }
    if (program.name.empty()) {
        addError(result, ValidationErrorCode::EmptyName, "name");
    }
    if (!validSensorPreference(program.sensorPreference)) {
        addError(result, ValidationErrorCode::InvalidEnumValue,
                 "defaults.sensor_preference");
    }
    if (!validFailurePolicy(program.productSensorFailure.policy)) {
        addError(result, ValidationErrorCode::InvalidEnumValue,
                 "defaults.product_sensor_failure.policy");
    }
    if (!validCompletionMode(program.completion.mode)) {
        addError(result, ValidationErrorCode::InvalidEnumValue,
                 "defaults.completion.mode");
    }
    if (program.fermentationStages.size() != 1U) {
        addError(result, ValidationErrorCode::UnsupportedStageCount,
                 "defaults.fermentation_stages");
    } else {
        const auto& stage = program.fermentationStages.front();
        validateOptionalDouble(result, stage.targetTemperatureCelsius,
                               "defaults.fermentation_temperature_c",
                               kMinimumFermentationTemperatureCelsius,
                               kMaximumFermentationTemperatureCelsius, purpose);
        validateOptionalDuration(result, stage.durationMinutes,
                                 "defaults.fermentation_duration_min",
                                 kMinimumFermentationDurationMinutes,
                                 kMaximumFermentationDurationMinutes, purpose);
    }

    validateOptionalDouble(result, program.targetQualification.bandCelsius,
                           "defaults.target_qualification_band_c",
                           kMinimumQualificationBandCelsius,
                           kMaximumQualificationBandCelsius, purpose);
    validateOptionalDuration(
        result, program.targetQualification.durationMinutes,
        "defaults.target_qualification_duration_min", 1U,
        std::numeric_limits<std::uint32_t>::max(), purpose);
    validateOptionalDuration(result, program.maximumTargetReachMinutes,
                             "defaults.max_target_reach_min", 1U,
                             std::numeric_limits<std::uint32_t>::max(),
                             purpose);

    if (program.productSensorFailure.policy ==
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout) {
        validateOptionalDuration(
            result, program.productSensorFailure.fallbackDelaySeconds,
            "defaults.product_sensor_failure.fallback_delay_s", 0U,
            kMaximumFallbackDelaySeconds, purpose);
    }

    if (validCompletionMode(program.completion.mode) &&
        hasCooling(program.completion.mode)) {
        validateOptionalDouble(result, program.completion.coolingTargetCelsius,
                               "defaults.completion.cooling_target_c",
                               kMinimumCoolingTargetCelsius,
                               kMaximumCoolingTargetCelsius, purpose);
    } else if (program.completion.coolingTargetCelsius.has_value()) {
        addError(result, ValidationErrorCode::UnexpectedValue,
                 "defaults.completion.cooling_target_c");
    }

    if (program.completion.mode == CompletionMode::CoolAndHoldForDuration) {
        validateOptionalDuration(result, program.completion.holdDurationMinutes,
                                 "defaults.completion.hold_duration_min", 1U,
                                 std::numeric_limits<std::uint32_t>::max(),
                                 purpose);
    } else if (program.completion.holdDurationMinutes.has_value()) {
        addError(result, ValidationErrorCode::UnexpectedValue,
                 "defaults.completion.hold_duration_min");
    }

    return result;
}

MigrationResult migrateProgramToCurrentSchema(const ProgramDocument& source) {
    if (source.schema.version == kCurrentProgramSchemaVersion) {
        return {MigrationStatus::NotRequired, source};
    }
    if (source.schema.version != kMigratableProgramSchemaVersion ||
        (source.schema.presentFields & ~kSchema3RequiredProgramFields) != 0U ||
        (source.schema.presentFields & kSchema3RequiredProgramFields) !=
            kSchema3RequiredProgramFields) {
        const auto status =
            source.schema.version == kMigratableProgramSchemaVersion
                ? MigrationStatus::InvalidSourceDocument
                : MigrationStatus::UnsupportedSourceVersion;
        return {status, std::nullopt};
    }

    ProgramDocument migrated = source;
    migrated.schema.version = kCurrentProgramSchemaVersion;
    migrated.schema.presentFields = kCurrentRequiredProgramFields;
    migrated.program.factoryCatalogEntry = migrated.program.builtIn;
    migrated.program.userDeletable = true;
    migrated.program.installed = true;
    return {MigrationStatus::Migrated, std::move(migrated)};
}

}  // namespace fermentation
