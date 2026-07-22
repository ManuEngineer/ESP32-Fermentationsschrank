#include <unity.h>

#include <cstring>
#include <string>

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

ProgramDocument makeRunnableProgram() {
    auto copy = FactoryProgramCatalog::makeUserCopy(
        "yogurt-mild", "user-program", "Eigenes Programm");
    TEST_ASSERT_TRUE(copy.has_value());

    auto& program = copy->program;
    program.productSensorFailure.fallbackDelaySeconds = 0U;
    program.fermentationStages.front().targetTemperatureCelsius = 4.0;
    program.fermentationStages.front().durationMinutes = 1U;
    program.targetQualification.bandCelsius = 0.1;
    program.targetQualification.durationMinutes = 1U;
    program.maximumTargetReachMinutes = 1U;
    program.completion.coolingTargetCelsius = 4.0;
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
    program.fermentationStages.front().targetTemperatureCelsius = 45.0;
    program.fermentationStages.front().durationMinutes = 20160U;
    program.targetQualification.bandCelsius = 2.0;
    program.productSensorFailure.fallbackDelaySeconds = 3600U;
    program.completion.coolingTargetCelsius = 25.0;

    TEST_ASSERT_TRUE(validateProgram(document).valid());
}

void test_values_outside_documented_boundaries_are_rejected() {
    auto document = makeRunnableProgram();
    auto& program = document.program;
    program.fermentationStages.front().targetTemperatureCelsius = 3.9;
    program.fermentationStages.front().durationMinutes = 0U;
    program.targetQualification.bandCelsius = 2.1;
    program.productSensorFailure.fallbackDelaySeconds = 3601U;
    program.completion.coolingTargetCelsius = 25.1;

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
                      "defaults.product_sensor_failure.fallback_delay_s"));
    TEST_ASSERT_TRUE(containsError(result, ValidationErrorCode::ValueOutOfRange,
                                   "defaults.completion.cooling_target_c"));
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
}

void test_schema_versions_and_v3_migration_are_explicit() {
    auto current = makeRunnableProgram();
    const auto unchanged = fermentation::migrateProgramToCurrentSchema(current);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MigrationStatus::NotRequired),
                          static_cast<int>(unchanged.status));
    TEST_ASSERT_TRUE(unchanged.document.has_value());

    auto versionThree = current;
    versionThree.schema.version = fermentation::kMigratableProgramSchemaVersion;
    versionThree.schema.presentFields =
        fermentation::kSchema3RequiredProgramFields;
    versionThree.program.factoryCatalogEntry = false;
    versionThree.program.userDeletable = false;
    versionThree.program.installed = false;
    const auto migrated =
        fermentation::migrateProgramToCurrentSchema(versionThree);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(MigrationStatus::Migrated),
                          static_cast<int>(migrated.status));
    TEST_ASSERT_TRUE(migrated.document.has_value());
    TEST_ASSERT_EQUAL_UINT32(fermentation::kCurrentProgramSchemaVersion,
                             migrated.document->schema.version);
    TEST_ASSERT_TRUE(migrated.document->program.userDeletable);
    TEST_ASSERT_TRUE(migrated.document->program.installed);
    TEST_ASSERT_TRUE(validateProgram(*migrated.document).valid());

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

void test_invalid_v3_document_is_not_partially_migrated() {
    auto versionThree = makeRunnableProgram();
    versionThree.schema.version = fermentation::kMigratableProgramSchemaVersion;
    versionThree.schema.presentFields =
        fermentation::kSchema3RequiredProgramFields &
        ~fermentation::fieldMask(ProgramField::Id);

    const auto result =
        fermentation::migrateProgramToCurrentSchema(versionThree);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(MigrationStatus::InvalidSourceDocument),
        static_cast<int>(result.status));
    TEST_ASSERT_FALSE(result.document.has_value());
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

void test_release_one_rejects_zero_or_multiple_fermentation_stages() {
    auto noStage = makeRunnableProgram();
    noStage.program.fermentationStages.clear();
    TEST_ASSERT_TRUE(containsError(validateProgram(noStage),
                                   ValidationErrorCode::UnsupportedStageCount,
                                   "defaults.fermentation_stages"));

    auto twoStages = makeRunnableProgram();
    twoStages.program.fermentationStages.push_back(
        twoStages.program.fermentationStages.front());
    TEST_ASSERT_TRUE(containsError(validateProgram(twoStages),
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
    RUN_TEST(test_schema_versions_and_v3_migration_are_explicit);
    RUN_TEST(test_invalid_v3_document_is_not_partially_migrated);
    RUN_TEST(test_all_four_factory_programs_load_with_specified_behaviour);
    RUN_TEST(test_active_selection_is_a_copy_separate_from_factory_catalog);
    RUN_TEST(test_user_copy_has_independent_identity_and_lifecycle);
    RUN_TEST(test_user_copy_rejects_factory_program_ids);
    RUN_TEST(test_release_one_rejects_zero_or_multiple_fermentation_stages);
    RUN_TEST(test_unknown_enum_values_are_rejected);
    return UNITY_END();
}
