#include <unity.h>

#include <optional>
#include <string>

#include "configuration_migration.hpp"
#include "standard_program_catalog.hpp"

namespace {

struct TestDocument {
    std::uint32_t schema{1U};
    std::string value;
};

void test_generic_copy_migration_applies_each_step() {
    const TestDocument source{1U, "source"};
    const auto result = fermentation::migrateDocumentCopy(
        source, 1U, 3U,
        [](const TestDocument& current,
           std::uint32_t schema) -> std::optional<TestDocument> {
            auto next = current;
            next.schema = schema + 1U;
            next.value += ":" + std::to_string(next.schema);
            return next;
        });
    TEST_ASSERT_TRUE(result.status ==
                     fermentation::CopyMigrationStatus::Migrated);
    TEST_ASSERT_TRUE(result.document.has_value());
    TEST_ASSERT_EQUAL_UINT32(3U, result.document->schema);
    TEST_ASSERT_EQUAL_STRING("source:2:3", result.document->value.c_str());
    TEST_ASSERT_EQUAL_UINT32(1U, source.schema);
    TEST_ASSERT_EQUAL_STRING("source", source.value.c_str());
}

void test_generic_copy_migration_failure_has_no_partial_result() {
    const TestDocument source{1U, "source"};
    const auto result = fermentation::migrateDocumentCopy(
        source, 1U, 3U,
        [](const TestDocument& current,
           std::uint32_t schema) -> std::optional<TestDocument> {
            if (schema == 2U) {
                return std::nullopt;
            }
            auto next = current;
            next.schema = schema + 1U;
            next.value = "partial";
            return next;
        });
    TEST_ASSERT_TRUE(result.status ==
                     fermentation::CopyMigrationStatus::StepFailed);
    TEST_ASSERT_FALSE(result.document.has_value());
    TEST_ASSERT_EQUAL_UINT32(1U, source.schema);
    TEST_ASSERT_EQUAL_STRING("source", source.value.c_str());
}

void test_generic_copy_migration_rejects_zero_and_newer_schema() {
    const TestDocument source{1U, "source"};
    TEST_ASSERT_TRUE(
        fermentation::migrateDocumentCopy(
            source, 0U, 1U,
            [](const TestDocument&, std::uint32_t)
                -> std::optional<TestDocument> { return std::nullopt; })
            .status ==
        fermentation::CopyMigrationStatus::UnsupportedSourceSchema);
    TEST_ASSERT_TRUE(
        fermentation::migrateDocumentCopy(
            source, 2U, 1U,
            [](const TestDocument&, std::uint32_t)
                -> std::optional<TestDocument> { return std::nullopt; })
            .status ==
        fermentation::CopyMigrationStatus::UnsupportedSourceSchema);
}

void test_current_catalog_copy_migration_is_not_required() {
    const auto source = fermentation::makeFactoryProgramCatalog();
    const auto result =
        fermentation::migrateProgramCatalogDocumentsToCurrentSchema(source);
    TEST_ASSERT_TRUE(result.status ==
                     fermentation::CopyMigrationStatus::NotRequired);
    TEST_ASSERT_TRUE(result.document.has_value());
    TEST_ASSERT_EQUAL_UINT32(source.programs.size(),
                             result.document->programs.size());
}

void test_catalog_program_migration_rejects_unknown_newer_schema_atomically() {
    auto source = fermentation::makeFactoryProgramCatalog();
    source.programs[2].schema.version =
        fermentation::kCurrentProgramSchemaVersion + 1U;
    const auto result =
        fermentation::migrateProgramCatalogDocumentsToCurrentSchema(source);
    TEST_ASSERT_TRUE(result.status ==
                     fermentation::CopyMigrationStatus::StepFailed);
    TEST_ASSERT_FALSE(result.document.has_value());
    TEST_ASSERT_EQUAL_UINT32(fermentation::kCurrentProgramSchemaVersion + 1U,
                             source.programs[2].schema.version);
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_generic_copy_migration_applies_each_step);
    RUN_TEST(test_generic_copy_migration_failure_has_no_partial_result);
    RUN_TEST(test_generic_copy_migration_rejects_zero_and_newer_schema);
    RUN_TEST(test_current_catalog_copy_migration_is_not_required);
    RUN_TEST(
        test_catalog_program_migration_rejects_unknown_newer_schema_atomically);
    return UNITY_END();
}
