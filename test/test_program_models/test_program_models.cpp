#include <unity.h>

#include <cstring>
#include <limits>
#include <string>

#include "program_limits.hpp"
#include "program_model.hpp"
#include "standard_program_catalog.hpp"

namespace {

using fermentation::CompletionMode;
using fermentation::FactoryProgramCatalog;
using fermentation::MigrationStatus;
using fermentation::ProgramDocument;
using fermentation::ProgramField;
using fermentation::SensorPreference;
using fermentation::ValidationErrorCode;
using fermentation::ValidationPurpose;
namespace program_limits = fermentation::program_limits;

ProgramDocument makeRunnableProgram() {
    auto copy = FactoryProgramCatalog::makeUserCopy(
        "yogurt-mild", "user-program", "Eigenes Programm");
    TEST_ASSERT_TRUE(copy.has_value());

    auto& program = copy->program;
    program.productSensorFailure.fallbackDelaySeconds =
        program_limits::kMinimumFallbackDelaySeconds;
    program.fermentationStages.front().targetTemperatureCelsius =
        program_limits::kMinimumFermentationTemperatureCelsius;
    program.fermentationStages.front().durationMinutes =
        program_limits::kMinimumFermentationDurationMinutes;
    const auto minimumStage = program.fermentationStages.front();
    program.fermentationStages.assign(
        program_limits::kMinimumFermentationStageCount, minimumStage);
    program.targetQualification.bandCelsius =
        program_limits::kMinimumQualificationBandCelsius;
    program.targetQualification.durationMinutes =
        program_limits::kMinimumQualificationDurationMinutes;
    program.maximumTargetReachMinutes =
        program_limits::kMinimumTargetReachMinutes;
    program.maximumProductWaitMinutes =
        program_limits::kMinimumProductWaitMinutes;
    program.completion.mode = CompletionMode::CoolAndHoldForDuration;
    program.completion.coolingTargetCelsius =
        program_limits::kMinimumCoolingTargetCelsius;
    program.completion.holdDurationMinutes =
        program_limits::kMinimumHoldDurationMinutes;
    return *copy;
}

bool containsError(const fermentation::ValidationResult& result,
                   ValidationErrorCode code, const char* field) {
    for (const auto& error : result.errors) {
        if (error.code == code && std::strcmp(error.field, field) == 0) {
            return true;
        }
    }
    return false;
}

void test_documented_boundaries_are_accepted() {
    auto document = makeRunnableProgram();

    TEST_ASSERT_TRUE(validateProgram(document).valid());

    auto& program = document.program;
    program.fermentationStages.front().targetTemperatureCelsius =
        program_limits::kMaximumFermentationTemperatureCelsius;
    program.fermentationStages.front().durationMinutes =
        program_limits::kMaximumFermentationDurationMinutes;
    program.targetQualification.bandCelsius =
        program_limits::kMaximumQualificationBandCelsius;
    program.targetQualification.durationMinutes =
        program_limits::kMaximumQualificationDurationMinutes;
    program.maximumTargetReachMinutes =
        program_limits::kMaximumTargetReachMinutes;
    program.maximumProductWaitMinutes =
        program_limits::kMaximumProductWaitMinutes;
    program.productSensorFailure.fallbackDelaySeconds =
        program_limits::kMaximumFallbackDelaySeconds;
    program.completion.coolingTargetCelsius =
        program_limits::kMaximumCoolingTargetCelsius;
    program.completion.holdDurationMinutes =
        program_limits::kMaximumHoldDurationMinutes;

    TEST_ASSERT_TRUE(validateProgram(document).valid());
}

void test_values_outside_documented_boundaries_are_rejected() {
    static_assert(program_limits::kMinimumFermentationDurationMinutes > 0U);
    static_assert(program_limits::kMinimumQualificationDurationMinutes > 0U);
    static_assert(program_limits::kMinimumTargetReachMinutes > 0U);
    static_assert(program_limits::kMinimumHoldDurationMinutes > 0U);

    auto document = makeRunnableProgram();
    auto& program = document.program;
    program.fermentationStages.front().targetTemperatureCelsius =
        program_limits::kMinimumFermentationTemperatureCelsius - 0.1;
    program.fermentationStages.front().durationMinutes =
        program_limits::kMinimumFermentationDurationMinutes - 1U;
    program.targetQualification.bandCelsius =
        program_limits::kMaximumQualificationBandCelsius + 0.1;
    program.targetQualification.durationMinutes =
        program_limits::kMinimumQualificationDurationMinutes - 1U;
    program.maximumTargetReachMinutes =
        program_limits::kMinimumTargetReachMinutes - 1U;
    program.maximumProductWaitMinutes =
        program_limits::kMaximumProductWaitMinutes + 1U;
    program.productSensorFailure.fallbackDelaySeconds =
        program_limits::kMaximumFallbackDelaySeconds + 1U;
    program.completion.coolingTargetCelsius =
        program_limits::kMaximumCoolingTargetCelsius + 0.1;
    program.completion.holdDurationMinutes =
        program_limits::kMinimumHoldDurationMinutes - 1U;

    const auto result = validateProgram(document);

    TEST_ASSERT_FALSE(result.valid());
    TEST_ASSERT_TRUE(containsError(result, ValidationErrorCode::ValueOutOfRange,
                                   "defaults.fermentation_temperature_c"));
    TEST_ASSERT_TRUE(containsError(result, ValidationErrorCode::ValueOutOfRange,
                                   "defaults.fermentation_duration_min"));
    TEST_ASSERT_TRUE(containsError(result, ValidationErrorCode::ValueOutOfRange,
                                   "defaults.target_qualification_band_c"));
    TEST_ASSERT_TRUE(
        containsError(result, ValidationErrorCode::ValueOutOfRange,
                      "defaults.target_qualification_duration_min"));
    TEST_ASSERT_TRUE(containsError(result, ValidationErrorCode::ValueOutOfRange,
                                   "defaults.max_target_reach_min"));
    TEST_ASSERT_TRUE(containsError(result, ValidationErrorCode::ValueOutOfRange,
                                   "defaults.max_product_wait_min"));
    TEST_ASSERT_TRUE(
        containsError(result, ValidationErrorCode::ValueOutOfRange,
                      "defaults.product_sensor_failure.fallback_delay_s"));
    TEST_ASSERT_TRUE(containsError(result, ValidationErrorCode::ValueOutOfRange,
                                   "defaults.completion.cooling_target_c"));
    TEST_ASSERT_TRUE(containsError(result, ValidationErrorCode::ValueOutOfRange,
                                   "defaults.completion.hold_duration_min"));
}

void test_missing_required_and_unknown_fields_are_rejected() {
    auto missing = makeRunnableProgram();
    missing.schema.presentFields &=
        ~fermentation::fieldMask(ProgramField::Name);
    const auto missingResult = validateProgram(missing);
    TEST_ASSERT_TRUE(containsError(
        missingResult, ValidationErrorCode::MissingRequiredField, "name"));

    auto unknown = makeRunnableProgram();
    unknown.schema.presentFields |= 1ULL << 63U;
    const auto unknownResult = validateProgram(unknown);
    TEST_ASSERT_TRUE(containsError(
        unknownResult, ValidationErrorCode::UnknownField, "program"));
}

void test_catalog_templates_cannot_silently_become_runnable() {
    const auto factory = FactoryProgramCatalog::programs().front();

    TEST_ASSERT_TRUE(
        validateProgram(factory, ValidationPurpose::CatalogTemplate).valid());
    const auto runnableResult =
        validateProgram(factory, ValidationPurpose::Runnable);
    TEST_ASSERT_FALSE(runnableResult.valid());
    TEST_ASSERT_TRUE(containsError(
        runnableResult, ValidationErrorCode::MissingCommissioningValue,
        "defaults.fermentation_temperature_c"));
    TEST_ASSERT_TRUE(containsError(
        runnableResult, ValidationErrorCode::MissingCommissioningValue,
        "defaults.fermentation_duration_min"));
    TEST_ASSERT_TRUE(containsError(
        runnableResult, ValidationErrorCode::MissingCommissioningValue,
        "defaults.max_product_wait_min"));
}

void test_schema_versions_and_v4_migration_are_explicit() {
    auto current = makeRunnableProgram();
    const auto unchanged = fermentation::migrateProgramToCurrentSchema(current);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MigrationStatus::NotRequired),
                          static_cast<int>(unchanged.status));
    TEST_ASSERT_TRUE(unchanged.document.has_value());

    auto versionFour = current;
    versionFour.schema.version = fermentation::kMigratableProgramSchemaVersion;
    versionFour.schema.presentFields =
        fermentation::kSchema4RequiredProgramFields;
    versionFour.program.maximumProductWaitMinutes = std::nullopt;
    const auto migrated =
        fermentation::migrateProgramToCurrentSchema(versionFour);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MigrationStatus::Migrated),
                          static_cast<int>(migrated.status));
    TEST_ASSERT_TRUE(migrated.document.has_value());
    TEST_ASSERT_EQUAL_UINT32(fermentation::kCurrentProgramSchemaVersion,
                             migrated.document->schema.version);
    TEST_ASSERT_FALSE(
        migrated.document->program.maximumProductWaitMinutes.has_value());
    TEST_ASSERT_TRUE(
        validateProgram(*migrated.document, ValidationPurpose::CatalogTemplate)
            .valid());
    TEST_ASSERT_TRUE(containsError(
        validateProgram(*migrated.document, ValidationPurpose::Runnable),
        ValidationErrorCode::MissingCommissioningValue,
        "defaults.max_product_wait_min"));

    auto future = current;
    future.schema.version = fermentation::kCurrentProgramSchemaVersion + 1U;
    TEST_ASSERT_TRUE(containsError(
        validateProgram(future), ValidationErrorCode::UnsupportedSchemaVersion,
        "schema_version"));

    auto unsupportedOld = current;
    unsupportedOld.schema.version = 2U;
    const auto rejected =
        fermentation::migrateProgramToCurrentSchema(unsupportedOld);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MigrationStatus::UnsupportedSourceVersion),
        static_cast<int>(rejected.status));
    TEST_ASSERT_FALSE(rejected.document.has_value());
}

void test_invalid_v4_document_is_not_partially_migrated() {
    auto versionFour = makeRunnableProgram();
    versionFour.schema.version = fermentation::kMigratableProgramSchemaVersion;
    versionFour.schema.presentFields =
        fermentation::kSchema4RequiredProgramFields &
        ~fermentation::fieldMask(ProgramField::Id);

    const auto result =
        fermentation::migrateProgramToCurrentSchema(versionFour);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MigrationStatus::InvalidSourceDocument),
        static_cast<int>(result.status));
    TEST_ASSERT_FALSE(result.document.has_value());
}

void test_product_wait_is_for_preheating_programs_only() {
    auto preheating = makeRunnableProgram();
    preheating.program.maximumProductWaitMinutes = std::nullopt;
    TEST_ASSERT_TRUE(
        containsError(validateProgram(preheating),
                      ValidationErrorCode::MissingCommissioningValue,
                      "defaults.max_product_wait_min"));

    auto withoutPreheat = makeRunnableProgram();
    withoutPreheat.program.preheat = false;
    TEST_ASSERT_TRUE(containsError(validateProgram(withoutPreheat),
                                   ValidationErrorCode::UnexpectedValue,
                                   "defaults.max_product_wait_min"));
    withoutPreheat.program.maximumProductWaitMinutes = std::nullopt;
    TEST_ASSERT_TRUE(validateProgram(withoutPreheat).valid());
}

void test_all_four_factory_programs_load_with_specified_behaviour() {
    const auto programs = FactoryProgramCatalog::programs();

    TEST_ASSERT_EQUAL_UINT32(4U, programs.size());
    TEST_ASSERT_EQUAL_STRING("yogurt-mild", programs[0].program.id.c_str());
    TEST_ASSERT_EQUAL_STRING("yogurt-firm", programs[1].program.id.c_str());
    TEST_ASSERT_EQUAL_STRING("milk-kefir", programs[2].program.id.c_str());
    TEST_ASSERT_EQUAL_STRING("water-kefir", programs[3].program.id.c_str());

    TEST_ASSERT_TRUE(programs[0].program.preheat);
    TEST_ASSERT_TRUE(programs[1].program.preheat);
    TEST_ASSERT_FALSE(programs[2].program.preheat);
    TEST_ASSERT_FALSE(programs[3].program.preheat);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorPreference::ProductIfAvailableElseAir),
        static_cast<int>(programs[0].program.sensorPreference));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SensorPreference::AirProductOptional),
        static_cast<int>(programs[2].program.sensorPreference));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CompletionMode::CoolAndHoldUntilManualStop),
        static_cast<int>(programs[2].program.completion.mode));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(CompletionMode::FinishWithoutCooling),
        static_cast<int>(programs[3].program.completion.mode));

    for (const auto& program : programs) {
        TEST_ASSERT_TRUE(program.program.builtIn);
        TEST_ASSERT_TRUE(program.program.factoryCatalogEntry);
        TEST_ASSERT_TRUE(
            validateProgram(program, ValidationPurpose::CatalogTemplate)
                .valid());
    }
}

void test_active_selection_is_a_copy_separate_from_factory_catalog() {
    const auto factory = FactoryProgramCatalog::find("yogurt-mild");
    TEST_ASSERT_TRUE(factory.has_value());
    fermentation::ActiveProgramSelection selection;

    TEST_ASSERT_TRUE(selection.select(*factory));
    TEST_ASSERT_NOT_NULL(selection.mutableSelected());
    selection.mutableSelected()->program.name = "Geaenderte Auswahl";
    selection.mutableSelected()
        ->program.fermentationStages.front()
        .targetTemperatureCelsius = 40.0;

    const auto pristineFactory = FactoryProgramCatalog::find("yogurt-mild");
    TEST_ASSERT_TRUE(pristineFactory.has_value());
    TEST_ASSERT_EQUAL_STRING("Joghurt mild",
                             pristineFactory->program.name.c_str());
    TEST_ASSERT_FALSE(pristineFactory->program.fermentationStages.front()
                          .targetTemperatureCelsius.has_value());
}

void test_user_copy_has_independent_identity_and_lifecycle() {
    auto copy = FactoryProgramCatalog::makeUserCopy(
        "milk-kefir", "my-milk-kefir", "Mein Milchkefir");

    TEST_ASSERT_TRUE(copy.has_value());
    TEST_ASSERT_EQUAL_STRING("my-milk-kefir", copy->program.id.c_str());
    TEST_ASSERT_EQUAL_STRING("Mein Milchkefir", copy->program.name.c_str());
    TEST_ASSERT_FALSE(copy->program.builtIn);
    TEST_ASSERT_FALSE(copy->program.factoryCatalogEntry);
    TEST_ASSERT_FALSE(copy->program.resettable);
    TEST_ASSERT_TRUE(copy->program.userDeletable);
    TEST_ASSERT_TRUE(copy->program.installed);
    TEST_ASSERT_FALSE(FactoryProgramCatalog::find("my-milk-kefir").has_value());
}

void test_user_copy_rejects_factory_program_ids() {
    TEST_ASSERT_FALSE(FactoryProgramCatalog::makeUserCopy(
                          "milk-kefir", "milk-kefir", "Mein Milchkefir")
                          .has_value());
    TEST_ASSERT_FALSE(FactoryProgramCatalog::makeUserCopy(
                          "milk-kefir", "yogurt-mild", "Mein Milchkefir")
                          .has_value());
}

void test_release_one_stage_count_boundaries_are_enforced() {
    auto minimumStages = makeRunnableProgram();
    const auto validStage = minimumStages.program.fermentationStages.front();
    minimumStages.program.fermentationStages.assign(
        program_limits::kMinimumFermentationStageCount, validStage);
    TEST_ASSERT_TRUE(validateProgram(minimumStages).valid());

    auto maximumStages = makeRunnableProgram();
    maximumStages.program.fermentationStages.assign(
        program_limits::kMaximumFermentationStageCount, validStage);
    TEST_ASSERT_TRUE(validateProgram(maximumStages).valid());

    static_assert(program_limits::kMinimumFermentationStageCount > 0U);
    auto tooFewStages = makeRunnableProgram();
    tooFewStages.program.fermentationStages.assign(
        program_limits::kMinimumFermentationStageCount - 1U, validStage);
    TEST_ASSERT_TRUE(containsError(validateProgram(tooFewStages),
                                   ValidationErrorCode::UnsupportedStageCount,
                                   "defaults.fermentation_stages"));

    static_assert(program_limits::kMaximumFermentationStageCount <
                  std::numeric_limits<std::size_t>::max());
    auto tooManyStages = makeRunnableProgram();
    tooManyStages.program.fermentationStages.assign(
        program_limits::kMaximumFermentationStageCount + 1U, validStage);
    TEST_ASSERT_TRUE(containsError(validateProgram(tooManyStages),
                                   ValidationErrorCode::UnsupportedStageCount,
                                   "defaults.fermentation_stages"));
}

void test_unknown_enum_values_are_rejected() {
    auto document = makeRunnableProgram();
    document.program.sensorPreference = static_cast<SensorPreference>(255U);
    document.program.productSensorFailure.policy =
        static_cast<fermentation::ProductSensorFailurePolicy>(255U);
    document.program.completion.mode = static_cast<CompletionMode>(255U);

    const auto result = validateProgram(document);

    TEST_ASSERT_TRUE(containsError(result,
                                   ValidationErrorCode::InvalidEnumValue,
                                   "defaults.sensor_preference"));
    TEST_ASSERT_TRUE(containsError(result,
                                   ValidationErrorCode::InvalidEnumValue,
                                   "defaults.product_sensor_failure.policy"));
    TEST_ASSERT_TRUE(containsError(result,
                                   ValidationErrorCode::InvalidEnumValue,
                                   "defaults.completion.mode"));
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_documented_boundaries_are_accepted);
    RUN_TEST(test_values_outside_documented_boundaries_are_rejected);
    RUN_TEST(test_missing_required_and_unknown_fields_are_rejected);
    RUN_TEST(test_catalog_templates_cannot_silently_become_runnable);
    RUN_TEST(test_schema_versions_and_v4_migration_are_explicit);
    RUN_TEST(test_invalid_v4_document_is_not_partially_migrated);
    RUN_TEST(test_product_wait_is_for_preheating_programs_only);
    RUN_TEST(test_all_four_factory_programs_load_with_specified_behaviour);
    RUN_TEST(test_active_selection_is_a_copy_separate_from_factory_catalog);
    RUN_TEST(test_user_copy_has_independent_identity_and_lifecycle);
    RUN_TEST(test_user_copy_rejects_factory_program_ids);
    RUN_TEST(test_release_one_stage_count_boundaries_are_enforced);
    RUN_TEST(test_unknown_enum_values_are_rejected);
    return UNITY_END();
}
