#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fermentation {

inline constexpr std::uint32_t kCurrentProgramSchemaVersion = 4U;
inline constexpr std::uint32_t kMigratableProgramSchemaVersion = 3U;

using ProgramFieldMask = std::uint64_t;

// Absichtlich so breit wie ProgramFieldMask: Die aktuellen 15 Werte
// braeuchten zwar weniger, aber spaetere Schemafelder ueber Bit 15 hinaus
// sollen keinen Typwechsel erzwingen.
// NOLINTNEXTLINE(performance-enum-size): Headroom fuer weitere Felder.
enum class ProgramField : ProgramFieldMask {
    Id = 1ULL << 0U,
    Name = 1ULL << 1U,
    BuiltIn = 1ULL << 2U,
    Resettable = 1ULL << 3U,
    Enabled = 1ULL << 4U,
    Preheat = 1ULL << 5U,
    SensorPreference = 1ULL << 6U,
    ProductSensorFailure = 1ULL << 7U,
    FermentationStages = 1ULL << 8U,
    TargetQualification = 1ULL << 9U,
    MaximumTargetReach = 1ULL << 10U,
    Completion = 1ULL << 11U,
    FactoryCatalogEntry = 1ULL << 12U,
    UserDeletable = 1ULL << 13U,
    Installed = 1ULL << 14U,
};

[[nodiscard]] constexpr ProgramFieldMask fieldMask(ProgramField field) {
    return static_cast<ProgramFieldMask>(field);
}

inline constexpr ProgramFieldMask kSchema3RequiredProgramFields =
    fieldMask(ProgramField::Id) | fieldMask(ProgramField::Name) |
    fieldMask(ProgramField::BuiltIn) | fieldMask(ProgramField::Resettable) |
    fieldMask(ProgramField::Enabled) | fieldMask(ProgramField::Preheat) |
    fieldMask(ProgramField::SensorPreference) |
    fieldMask(ProgramField::ProductSensorFailure) |
    fieldMask(ProgramField::FermentationStages) |
    fieldMask(ProgramField::TargetQualification) |
    fieldMask(ProgramField::MaximumTargetReach) |
    fieldMask(ProgramField::Completion);

inline constexpr ProgramFieldMask kCurrentRequiredProgramFields =
    kSchema3RequiredProgramFields |
    fieldMask(ProgramField::FactoryCatalogEntry) |
    fieldMask(ProgramField::UserDeletable) | fieldMask(ProgramField::Installed);

inline constexpr ProgramFieldMask kCurrentKnownProgramFields =
    kCurrentRequiredProgramFields;

// Anzahl gesetzter Bits, portabel ohne C++20 std::popcount oder
// Compiler-Builtins. Dient nur dazu, `kRequiredFields` (program_model.cpp)
// per static_assert gegen die hier deklarierten Felder abzusichern.
[[nodiscard]] constexpr int countProgramFields(ProgramFieldMask mask) {
    int count = 0;
    while (mask != 0U) {
        mask &= (mask - 1U);
        ++count;
    }
    return count;
}

inline constexpr int kCurrentProgramFieldCount =
    countProgramFields(kCurrentKnownProgramFields);

enum class SensorPreference : std::uint8_t {
    ProductIfAvailableElseAir,
    AirProductOptional,
    ProductRequired,
    AirOnly,
};

enum class ProductSensorFailurePolicy : std::uint8_t {
    FallbackToAirAfterTimeout,
    WaitForUser,
    StopToSafeState,
};

enum class CompletionMode : std::uint8_t {
    FinishWithoutCooling,
    CoolThenFinish,
    CoolAndHoldForDuration,
    CoolAndHoldUntilManualStop,
};

struct ProductSensorFailure {
    ProductSensorFailurePolicy policy{
        ProductSensorFailurePolicy::FallbackToAirAfterTimeout};
    std::optional<std::uint32_t> fallbackDelaySeconds;
};

struct FermentationStage {
    std::optional<double> targetTemperatureCelsius;
    std::optional<std::uint32_t> durationMinutes;
};

struct TargetQualification {
    std::optional<double> bandCelsius;
    std::optional<std::uint32_t> durationMinutes;
};

struct ProgramCompletion {
    CompletionMode mode{CompletionMode::FinishWithoutCooling};
    std::optional<double> coolingTargetCelsius;
    std::optional<std::uint32_t> holdDurationMinutes;
};

struct ProgramDefinition {
    std::string id;
    std::string name;
    std::string notes;
    bool builtIn{false};
    bool factoryCatalogEntry{false};
    bool resettable{false};
    bool userDeletable{true};
    bool installed{true};
    bool enabled{true};
    bool preheat{false};
    SensorPreference sensorPreference{SensorPreference::AirProductOptional};
    ProductSensorFailure productSensorFailure;
    std::vector<FermentationStage> fermentationStages;
    TargetQualification targetQualification;
    std::optional<std::uint32_t> maximumTargetReachMinutes;
    ProgramCompletion completion;
};

struct ProgramSchemaMetadata {
    std::uint32_t version{kCurrentProgramSchemaVersion};
    ProgramFieldMask presentFields{kCurrentRequiredProgramFields};
};

struct ProgramDocument {
    ProgramSchemaMetadata schema;
    ProgramDefinition program;
};

enum class ValidationPurpose : std::uint8_t {
    CatalogTemplate,
    Runnable,
};

enum class ValidationErrorCode : std::uint8_t {
    UnsupportedSchemaVersion,
    MissingRequiredField,
    UnknownField,
    EmptyId,
    EmptyName,
    UnsupportedStageCount,
    MissingCommissioningValue,
    ValueOutOfRange,
    InvalidEnumValue,
    UnexpectedValue,
};

struct ValidationError {
    ValidationErrorCode code;
    const char* field;
};

struct ValidationResult {
    std::vector<ValidationError> errors;

    [[nodiscard]] bool valid() const { return errors.empty(); }
};

[[nodiscard]] ValidationResult validateProgram(
    const ProgramDocument& document,
    ValidationPurpose purpose = ValidationPurpose::Runnable);

enum class MigrationStatus : std::uint8_t {
    NotRequired,
    Migrated,
    UnsupportedSourceVersion,
    InvalidSourceDocument,
};

struct MigrationResult {
    MigrationStatus status{MigrationStatus::UnsupportedSourceVersion};
    std::optional<ProgramDocument> document;
};

[[nodiscard]] MigrationResult migrateProgramToCurrentSchema(
    const ProgramDocument& source);

}  // namespace fermentation
