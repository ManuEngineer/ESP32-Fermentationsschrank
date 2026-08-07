#include "program_model.hpp"

#include <array>
#include <cmath>
#include <utility>

#include "program_limits.hpp"

namespace fermentation {
namespace {

struct RequiredField {
    ProgramField field;
    const char* name;
};

constexpr std::array<RequiredField, 17> kRequiredFields{{
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
    {ProgramField::MaximumProductWait, "defaults.max_product_wait_min"},
    {ProgramField::Completion, "defaults.completion"},
    {ProgramField::ReturnStrategy,
     "defaults.product_sensor_failure.return_strategy"},
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

bool validReturnStrategy(ReturnStrategy strategy) {
    switch (strategy) {
        case ReturnStrategy::RemainOnAirUntilEnd:
        case ReturnStrategy::ManualReturnToProduct:
        case ReturnStrategy::AutomaticValidatedReturnToProduct:
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

MigrationResult migrateProgramSchema4To5(const ProgramDocument& source) {
    if ((source.schema.presentFields & ~kSchema4RequiredProgramFields) != 0U ||
        (source.schema.presentFields & kSchema4RequiredProgramFields) !=
            kSchema4RequiredProgramFields) {
        return {MigrationStatus::InvalidSourceDocument, std::nullopt};
    }
    ProgramDocument migrated = source;
    migrated.schema.version = 5U;
    migrated.schema.presentFields = kSchema5RequiredProgramFields;
    migrated.program.maximumProductWaitMinutes = std::nullopt;
    return {MigrationStatus::Migrated, std::move(migrated)};
}

MigrationResult migrateProgramSchema5To6(const ProgramDocument& source) {
    if ((source.schema.presentFields & ~kSchema5RequiredProgramFields) != 0U ||
        (source.schema.presentFields & kSchema5RequiredProgramFields) !=
            kSchema5RequiredProgramFields) {
        return {MigrationStatus::InvalidSourceDocument, std::nullopt};
    }
    ProgramDocument migrated = source;
    migrated.schema.version = 6U;
    migrated.schema.presentFields = kCurrentRequiredProgramFields;
    if (migrated.program.sensorPreference == SensorPreference::AirOnly) {
        // AirOnly-Normalisierung (6.2.2): inert, weil AirOnly Luft nie
        // verlaesst - bewahrt ein vor diesem Update gueltiges Dokument als
        // weiterhin gueltig, statt es an der neuen 6.13-Cross-Field-Regel
        // scheitern zu lassen.
        migrated.program.productSensorFailure.returnStrategy =
            ReturnStrategy::RemainOnAirUntilEnd;
        migrated.program.productSensorFailure.policy =
            ProductSensorFailurePolicy::FallbackToAirAfterTimeout;
        migrated.program.productSensorFailure.fallbackDelaySeconds =
            std::nullopt;
    } else {
        migrated.program.productSensorFailure.returnStrategy =
            ReturnStrategy::AutomaticValidatedReturnToProduct;
    }
    return {MigrationStatus::Migrated, std::move(migrated)};
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
    if (!validReturnStrategy(program.productSensorFailure.returnStrategy)) {
        addError(result, ValidationErrorCode::InvalidEnumValue,
                 "defaults.product_sensor_failure.return_strategy");
    }
    // 6.13 Regel 1: ProductRequired lehnt einen automatischen Luftfallback
    // strukturell ab - dieselbe Praeferenz schliesst spaeter (sensor_selection)
    // auch jeden manuellen Luftfallback aus (6.4.13).
    if (program.sensorPreference == SensorPreference::ProductRequired &&
        program.productSensorFailure.policy ==
            ProductSensorFailurePolicy::FallbackToAirAfterTimeout) {
        addError(result, ValidationErrorCode::IncompatibleCombination,
                 "defaults.product_sensor_failure.policy");
    }
    // 6.13 Regel 2/3: AirOnly hat genau eine gueltige Kombination - Luft wird
    // nie verlassen, die Rueckkehrstrategie und die Ausfallpolicy sind fest.
    if (program.sensorPreference == SensorPreference::AirOnly) {
        if (program.productSensorFailure.returnStrategy !=
            ReturnStrategy::RemainOnAirUntilEnd) {
            addError(result, ValidationErrorCode::IncompatibleCombination,
                     "defaults.product_sensor_failure.return_strategy");
        }
        if (program.productSensorFailure.policy !=
            ProductSensorFailurePolicy::FallbackToAirAfterTimeout) {
            addError(result, ValidationErrorCode::IncompatibleCombination,
                     "defaults.product_sensor_failure.policy");
        }
    }
    // 6.13 Regel 4: fuer AirOnly ist fallbackDelaySeconds unabhaengig von der
    // Policy ein toter Wert - ProductFailureDetected wird nie betreten.
    if (program.sensorPreference == SensorPreference::AirOnly &&
        program.productSensorFailure.fallbackDelaySeconds.has_value()) {
        addError(result, ValidationErrorCode::UnexpectedValue,
                 "defaults.product_sensor_failure.fallback_delay_s");
    }
    // 6.13 Regel 5: generelle Regel fuer jede nicht-FallbackToAirAfterTimeout-
    // Policy (bewusst nicht mit Regel 4 zusammengelegt, siehe 6.4.10).
    if (program.productSensorFailure.policy !=
            ProductSensorFailurePolicy::FallbackToAirAfterTimeout &&
        program.productSensorFailure.fallbackDelaySeconds.has_value()) {
        addError(result, ValidationErrorCode::UnexpectedValue,
                 "defaults.product_sensor_failure.fallback_delay_s");
    }
    if (!validCompletionMode(program.completion.mode)) {
        addError(result, ValidationErrorCode::InvalidEnumValue,
                 "defaults.completion.mode");
    }
    const auto fermentationStageCount = program.fermentationStages.size();
    if (fermentationStageCount <
            program_limits::kMinimumFermentationStageCount ||
        fermentationStageCount >
            program_limits::kMaximumFermentationStageCount) {
        addError(result, ValidationErrorCode::UnsupportedStageCount,
                 "defaults.fermentation_stages");
    } else {
        const auto& stage = program.fermentationStages.front();
        validateOptionalDouble(
            result, stage.targetTemperatureCelsius,
            "defaults.fermentation_temperature_c",
            program_limits::kMinimumFermentationTemperatureCelsius,
            program_limits::kMaximumFermentationTemperatureCelsius, purpose);
        validateOptionalDuration(
            result, stage.durationMinutes, "defaults.fermentation_duration_min",
            program_limits::kMinimumFermentationDurationMinutes,
            program_limits::kMaximumFermentationDurationMinutes, purpose);
    }

    validateOptionalDouble(result, program.targetQualification.bandCelsius,
                           "defaults.target_qualification_band_c",
                           program_limits::kMinimumQualificationBandCelsius,
                           program_limits::kMaximumQualificationBandCelsius,
                           purpose);
    validateOptionalDuration(
        result, program.targetQualification.durationMinutes,
        "defaults.target_qualification_duration_min",
        program_limits::kMinimumQualificationDurationMinutes,
        program_limits::kMaximumQualificationDurationMinutes, purpose);
    validateOptionalDuration(result, program.maximumTargetReachMinutes,
                             "defaults.max_target_reach_min",
                             program_limits::kMinimumTargetReachMinutes,
                             program_limits::kMaximumTargetReachMinutes,
                             purpose);

    if (program.preheat) {
        validateOptionalDuration(result, program.maximumProductWaitMinutes,
                                 "defaults.max_product_wait_min",
                                 program_limits::kMinimumProductWaitMinutes,
                                 program_limits::kMaximumProductWaitMinutes,
                                 purpose);
    } else if (program.maximumProductWaitMinutes.has_value()) {
        addError(result, ValidationErrorCode::UnexpectedValue,
                 "defaults.max_product_wait_min");
    }

    if (program.productSensorFailure.policy ==
            ProductSensorFailurePolicy::FallbackToAirAfterTimeout &&
        program.sensorPreference != SensorPreference::AirOnly) {
        validateOptionalDuration(
            result, program.productSensorFailure.fallbackDelaySeconds,
            "defaults.product_sensor_failure.fallback_delay_s",
            program_limits::kMinimumFallbackDelaySeconds,
            program_limits::kMaximumFallbackDelaySeconds, purpose);
    }

    if (validCompletionMode(program.completion.mode) &&
        hasCooling(program.completion.mode)) {
        validateOptionalDouble(result, program.completion.coolingTargetCelsius,
                               "defaults.completion.cooling_target_c",
                               program_limits::kMinimumCoolingTargetCelsius,
                               program_limits::kMaximumCoolingTargetCelsius,
                               purpose);
    } else if (program.completion.coolingTargetCelsius.has_value()) {
        addError(result, ValidationErrorCode::UnexpectedValue,
                 "defaults.completion.cooling_target_c");
    }

    if (program.completion.mode == CompletionMode::CoolAndHoldForDuration) {
        validateOptionalDuration(result, program.completion.holdDurationMinutes,
                                 "defaults.completion.hold_duration_min",
                                 program_limits::kMinimumHoldDurationMinutes,
                                 program_limits::kMaximumHoldDurationMinutes,
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
    if (source.schema.version < kMinimumMigratableProgramSchemaVersion ||
        source.schema.version > kCurrentProgramSchemaVersion) {
        return {MigrationStatus::UnsupportedSourceVersion, std::nullopt};
    }

    // Kanonische Copy-Migrationskette (docs/CONFIGURATION_PERSISTENCE.md):
    // jeder Schritt einzeln auf einer Kopie, exakt die zwei existierenden
    // Schritte 4->5 und 5->6 - kein generischer Schrittregistry-Mechanismus.
    ProgramDocument current = source;
    while (current.schema.version < kCurrentProgramSchemaVersion) {
        auto step = current.schema.version == 4U
                        ? migrateProgramSchema4To5(current)
                        : migrateProgramSchema5To6(current);
        if (step.status != MigrationStatus::Migrated ||
            !step.document.has_value()) {
            return step;
        }
        current = std::move(*step.document);
    }
    return {MigrationStatus::Migrated, std::move(current)};
}

}  // namespace fermentation
