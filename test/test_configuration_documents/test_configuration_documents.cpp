#include <unity.h>

#include <limits>
#include <string>

#include "configuration_documents.hpp"
#include "configuration_limits.hpp"
#include "configuration_storage_contract.hpp"
#include "configuration_text.hpp"
#include "firmware_configuration_catalog.hpp"
#include "mock_time_zone_resolver.hpp"
#include "standard_program_catalog.hpp"
#include "storage_types.hpp"

namespace {

using device_platform::CheckedIncrementStatus;
using device_platform::TimeZonePrepareStatus;
using device_platform_test_support::MockTimeZoneResolver;
using fermentation::ConfigurationTextStatus;
using fermentation::ProgramCatalog;
using fermentation::ProgramCatalogStatus;
using fermentation::UserConfiguration;
using fermentation::UserConfigurationStatus;

UserConfiguration validUserConfiguration() {
    return {"de", "Europe/Zurich", "Fermentationsschrank"};
}

fermentation::ProgramDocument userProgram(std::string id) {
    auto copy = fermentation::FactoryProgramCatalog::makeUserCopy(
        "water-kefir", std::move(id), "Benutzerprogramm");
    TEST_ASSERT_TRUE(copy.has_value());
    return std::move(*copy);
}

void test_firmware_catalogs_are_versioned_and_exact() {
    using namespace fermentation::firmware_configuration_catalog;
    TEST_ASSERT_EQUAL_UINT32(1U, kLanguageCatalogVersion);
    TEST_ASSERT_EQUAL_UINT32(1U, kTimeZoneCatalogVersion);
    TEST_ASSERT_TRUE(containsLanguageId("de"));
    TEST_ASSERT_TRUE(containsLanguageId("es"));
    TEST_ASSERT_TRUE(containsLanguageId("en"));
    TEST_ASSERT_FALSE(containsLanguageId("fr"));
    TEST_ASSERT_TRUE(containsTimeZoneId("Europe/Zurich"));
    TEST_ASSERT_FALSE(containsTimeZoneId("Europe/Berlin"));
}

void test_user_configuration_prepares_catalogued_time_zone() {
    MockTimeZoneResolver resolver;
    const auto result = fermentation::validateUserConfiguration(
        validUserConfiguration(), resolver);
    TEST_ASSERT_TRUE(result.status == UserConfigurationStatus::Success);
    TEST_ASSERT_TRUE(result.preparedTimeZone.has_value());
    TEST_ASSERT_EQUAL_STRING(
        "Europe/Zurich", result.preparedTimeZone->canonicalIdentifier.c_str());
    TEST_ASSERT_EQUAL_UINT32(1U, resolver.callCount());
}

void test_user_configuration_rejects_unknown_catalog_values_before_resolver() {
    MockTimeZoneResolver resolver;
    auto configuration = validUserConfiguration();
    configuration.displayLanguageId = "fr";
    TEST_ASSERT_TRUE(
        fermentation::validateUserConfiguration(configuration, resolver)
            .status == UserConfigurationStatus::UnknownLanguageId);
    configuration = validUserConfiguration();
    configuration.timeZoneId = "Europe/Berlin";
    TEST_ASSERT_TRUE(
        fermentation::validateUserConfiguration(configuration, resolver)
            .status == UserConfigurationStatus::UnknownTimeZoneId);
    TEST_ASSERT_EQUAL_UINT32(0U, resolver.callCount());
}

void test_user_configuration_preserves_typed_resolver_failures() {
    MockTimeZoneResolver resolver;
    resolver.setStatus(TimeZonePrepareStatus::UnsupportedIdentifier);
    TEST_ASSERT_TRUE(fermentation::validateUserConfiguration(
                         validUserConfiguration(), resolver)
                         .status == UserConfigurationStatus::TimeZoneRejected);
    resolver.setStatus(TimeZonePrepareStatus::PreparationFailed);
    TEST_ASSERT_TRUE(fermentation::validateUserConfiguration(
                         validUserConfiguration(), resolver)
                         .status ==
                     UserConfigurationStatus::TimeZonePreparationFailed);
}

void test_lowercase_identifier_boundaries_and_structure() {
    using fermentation::validateLowercaseIdentifier;
    TEST_ASSERT_TRUE(validateLowercaseIdentifier("a", 1U, 48U) ==
                     ConfigurationTextStatus::Success);
    TEST_ASSERT_TRUE(
        validateLowercaseIdentifier(std::string(48U, 'a'), 1U, 48U) ==
        ConfigurationTextStatus::Success);
    TEST_ASSERT_TRUE(validateLowercaseIdentifier("a", 2U, 16U) ==
                     ConfigurationTextStatus::TooShort);
    TEST_ASSERT_TRUE(validateLowercaseIdentifier("de", 2U, 16U) ==
                     ConfigurationTextStatus::Success);
    TEST_ASSERT_TRUE(
        validateLowercaseIdentifier(std::string(16U, 'a'), 2U, 16U) ==
        ConfigurationTextStatus::Success);
    TEST_ASSERT_TRUE(
        validateLowercaseIdentifier(std::string(17U, 'a'), 2U, 16U) ==
        ConfigurationTextStatus::TooLong);
    TEST_ASSERT_TRUE(validateLowercaseIdentifier("-ab", 1U, 48U) ==
                     ConfigurationTextStatus::InvalidHyphenPlacement);
    TEST_ASSERT_TRUE(validateLowercaseIdentifier("ab-", 1U, 48U) ==
                     ConfigurationTextStatus::InvalidHyphenPlacement);
    TEST_ASSERT_TRUE(validateLowercaseIdentifier("a--b", 1U, 48U) ==
                     ConfigurationTextStatus::InvalidHyphenPlacement);
    TEST_ASSERT_TRUE(validateLowercaseIdentifier("A", 1U, 48U) ==
                     ConfigurationTextStatus::InvalidAsciiCharacter);
}

void test_time_zone_structure_boundaries() {
    using fermentation::validateTimeZoneIdentifierStructure;
    TEST_ASSERT_TRUE(validateTimeZoneIdentifierStructure("A") ==
                     ConfigurationTextStatus::Success);
    TEST_ASSERT_TRUE(validateTimeZoneIdentifierStructure(std::string(
                         64U, 'A')) == ConfigurationTextStatus::Success);
    TEST_ASSERT_TRUE(validateTimeZoneIdentifierStructure("") ==
                     ConfigurationTextStatus::TooShort);
    TEST_ASSERT_TRUE(validateTimeZoneIdentifierStructure(std::string(
                         65U, 'A')) == ConfigurationTextStatus::TooLong);
    for (const char* invalid :
         {"/Europe", "Europe/", "Europe//Zurich", "Europe/.", "Europe/..",
          "Etc/Unknown", "Europe Zurich"}) {
        TEST_ASSERT_FALSE(validateTimeZoneIdentifierStructure(invalid) ==
                          ConfigurationTextStatus::Success);
    }
}

void test_visible_name_utf8_scalar_and_byte_boundaries() {
    TEST_ASSERT_TRUE(fermentation::validateVisibleName("x") ==
                     ConfigurationTextStatus::Success);
    TEST_ASSERT_TRUE(fermentation::validateVisibleName(std::string(48U, 'x')) ==
                     ConfigurationTextStatus::Success);
    TEST_ASSERT_TRUE(fermentation::validateVisibleName(std::string(49U, 'x')) ==
                     ConfigurationTextStatus::TooManyScalars);
    std::string ninetySixBytes;
    for (std::size_t index = 0U; index < 48U; ++index) {
        ninetySixBytes += "\xC3\xA4";
    }
    TEST_ASSERT_TRUE(fermentation::validateVisibleName(ninetySixBytes) ==
                     ConfigurationTextStatus::Success);
    TEST_ASSERT_TRUE(fermentation::validateVisibleName(ninetySixBytes + "a") ==
                     ConfigurationTextStatus::TooLong);
}

void test_visible_name_rejects_invalid_utf8_controls_and_unicode_whitespace() {
    const std::string invalidValues[] = {std::string("\xC0\xAF", 2),
                                         std::string("\xE2\x82", 2),
                                         std::string("\xED\xA0\x80", 3),
                                         std::string("a\0b", 3),
                                         std::string("a\xC2\x85"
                                                     "b",
                                                     4),
                                         std::string("a\xE2\x80\xA8"
                                                     "b",
                                                     5)};
    for (const auto& invalid : invalidValues) {
        TEST_ASSERT_FALSE(fermentation::validateVisibleName(invalid) ==
                          ConfigurationTextStatus::Success);
    }
    TEST_ASSERT_TRUE(fermentation::validateVisibleName(" name") ==
                     ConfigurationTextStatus::LeadingOrTrailingWhitespace);
    TEST_ASSERT_TRUE(
        fermentation::validateVisibleName(std::string("\xC2\xA0name", 6)) ==
        ConfigurationTextStatus::LeadingOrTrailingWhitespace);
    TEST_ASSERT_TRUE(
        fermentation::validateVisibleName(std::string("name\xE3\x80\x80", 7)) ==
        ConfigurationTextStatus::LeadingOrTrailingWhitespace);
    TEST_ASSERT_TRUE(fermentation::validateVisibleName("   ") ==
                     ConfigurationTextStatus::OnlyWhitespace);
}

void test_notes_allow_lf_but_reject_forbidden_code_points() {
    TEST_ASSERT_TRUE(fermentation::validateProgramNotes("") ==
                     ConfigurationTextStatus::Success);
    TEST_ASSERT_TRUE(fermentation::validateProgramNotes("a\nb") ==
                     ConfigurationTextStatus::Success);
    const std::string invalidValues[] = {std::string("a\rb"),
                                         std::string("a\0b", 3),
                                         std::string("a\xC2\x85"
                                                     "b",
                                                     4),
                                         std::string("a\xE2\x80\xA9"
                                                     "b",
                                                     5)};
    for (const auto& invalid : invalidValues) {
        TEST_ASSERT_TRUE(fermentation::validateProgramNotes(invalid) ==
                         ConfigurationTextStatus::ForbiddenCodePoint);
    }
    TEST_ASSERT_TRUE(
        fermentation::validateProgramNotes(std::string(513U, 'a')) ==
        ConfigurationTextStatus::TooManyScalars);
    TEST_ASSERT_TRUE(fermentation::validateProgramNotes(std::string(
                         1025U, 'a')) == ConfigurationTextStatus::TooLong);
}

void test_note_normalization_is_explicit_and_deterministic() {
    TEST_ASSERT_EQUAL_STRING(
        "a\nb\nc",
        fermentation::normalizeProgramNotesForPreparation("a\r\nb\rc").c_str());
    TEST_ASSERT_TRUE(fermentation::validateProgramNotes("a\r\nb") ==
                     ConfigurationTextStatus::ForbiddenCodePoint);
}

void test_factory_catalog_has_exact_stable_order() {
    const auto catalog = fermentation::makeFactoryProgramCatalog();
    TEST_ASSERT_TRUE(fermentation::validateProgramCatalog(catalog) ==
                     ProgramCatalogStatus::Success);
    TEST_ASSERT_EQUAL_UINT32(4U, catalog.programs.size());
    TEST_ASSERT_EQUAL_STRING("yogurt-mild",
                             catalog.programs[0].program.id.c_str());
    TEST_ASSERT_EQUAL_STRING("yogurt-firm",
                             catalog.programs[1].program.id.c_str());
    TEST_ASSERT_EQUAL_STRING("milk-kefir",
                             catalog.programs[2].program.id.c_str());
    TEST_ASSERT_EQUAL_STRING("water-kefir",
                             catalog.programs[3].program.id.c_str());
}

void test_catalog_accepts_zero_and_twelve_user_programs() {
    auto catalog = fermentation::makeFactoryProgramCatalog();
    TEST_ASSERT_TRUE(fermentation::validateProgramCatalog(catalog) ==
                     ProgramCatalogStatus::Success);
    for (std::size_t index = 0U; index < 12U; ++index) {
        catalog.programs.push_back(
            userProgram("user-" + std::to_string(index)));
    }
    TEST_ASSERT_TRUE(fermentation::validateProgramCatalog(catalog) ==
                     ProgramCatalogStatus::Success);
}

void test_catalog_rejects_factory_count_order_and_total_boundaries() {
    auto three = fermentation::makeFactoryProgramCatalog();
    three.programs.pop_back();
    TEST_ASSERT_TRUE(fermentation::validateProgramCatalog(three) ==
                     ProgramCatalogStatus::InvalidProgramCount);

    auto wrongOrder = fermentation::makeFactoryProgramCatalog();
    std::swap(wrongOrder.programs[0], wrongOrder.programs[1]);
    TEST_ASSERT_TRUE(fermentation::validateProgramCatalog(wrongOrder) ==
                     ProgramCatalogStatus::InvalidFactoryOrder);

    auto fiveFactory = fermentation::makeFactoryProgramCatalog();
    auto extraFactory = userProgram("factory-extra");
    extraFactory.program.builtIn = true;
    extraFactory.program.factoryCatalogEntry = true;
    extraFactory.program.resettable = true;
    fiveFactory.programs.push_back(std::move(extraFactory));
    TEST_ASSERT_FALSE(fermentation::validateProgramCatalog(fiveFactory) ==
                      ProgramCatalogStatus::Success);

    auto seventeen = fermentation::makeFactoryProgramCatalog();
    for (std::size_t index = 0U; index < 13U; ++index) {
        seventeen.programs.push_back(
            userProgram("extra-" + std::to_string(index)));
    }
    TEST_ASSERT_TRUE(fermentation::validateProgramCatalog(seventeen) ==
                     ProgramCatalogStatus::InvalidProgramCount);
}

void test_catalog_rejects_duplicate_and_reserved_factory_ids() {
    auto duplicate = fermentation::makeFactoryProgramCatalog();
    duplicate.programs.push_back(userProgram("duplicate"));
    duplicate.programs.push_back(userProgram("duplicate"));
    TEST_ASSERT_TRUE(fermentation::validateProgramCatalog(duplicate) ==
                     ProgramCatalogStatus::DuplicateProgramId);

    auto reserved = fermentation::makeFactoryProgramCatalog();
    auto user = userProgram("temporary");
    user.program.id = "yogurt-mild";
    reserved.programs.push_back(std::move(user));
    TEST_ASSERT_TRUE(fermentation::validateProgramCatalog(reserved) ==
                     ProgramCatalogStatus::ReservedFactoryId);
}

void test_revision_types_reserve_zero_and_do_not_mix() {
    fermentation::UserConfigurationRevision reserved(0U);
    fermentation::UserConfigurationRevision unchangedReserved(9U);
    TEST_ASSERT_TRUE(
        device_platform::checkedIncrement(reserved, unchangedReserved) ==
        CheckedIncrementStatus::InvalidCurrentValue);
    TEST_ASSERT_EQUAL_UINT64(9U, unchangedReserved.value());

    fermentation::UserConfigurationRevision user(1U);
    fermentation::UserConfigurationRevision nextUser;
    TEST_ASSERT_TRUE(device_platform::checkedIncrement(user, nextUser) ==
                     CheckedIncrementStatus::Success);
    TEST_ASSERT_EQUAL_UINT64(2U, nextUser.value());
    fermentation::ServiceConfigurationRevision service(1U);
    fermentation::ServiceConfigurationRevision nextService;
    TEST_ASSERT_TRUE(device_platform::checkedIncrement(service, nextService) ==
                     CheckedIncrementStatus::Success);
    fermentation::ProgramCatalogRevision maximum(
        std::numeric_limits<std::uint64_t>::max());
    fermentation::ProgramCatalogRevision unchanged(9U);
    TEST_ASSERT_TRUE(device_platform::checkedIncrement(maximum, unchanged) ==
                     CheckedIncrementStatus::Overflow);
    TEST_ASSERT_EQUAL_UINT64(9U, unchanged.value());
}

void test_storage_contract_uses_stable_ids_and_short_keys() {
    using namespace fermentation::configuration_storage_contract;
    TEST_ASSERT_EQUAL_UINT16(1U, kUserConfigurationRecordType.value());
    TEST_ASSERT_EQUAL_UINT16(2U, kServiceConfigurationRecordType.value());
    TEST_ASSERT_EQUAL_UINT16(3U, kProgramCatalogRecordType.value());
    TEST_ASSERT_EQUAL_STRING("uc0", kUserConfigurationSlotKeys[0]);
    TEST_ASSERT_EQUAL_STRING("sc3", kServiceConfigurationSlotKeys[3]);
    TEST_ASSERT_EQUAL_STRING("pc3", kProgramCatalogSlotKeys[3]);
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_firmware_catalogs_are_versioned_and_exact);
    RUN_TEST(test_user_configuration_prepares_catalogued_time_zone);
    RUN_TEST(
        test_user_configuration_rejects_unknown_catalog_values_before_resolver);
    RUN_TEST(test_user_configuration_preserves_typed_resolver_failures);
    RUN_TEST(test_lowercase_identifier_boundaries_and_structure);
    RUN_TEST(test_time_zone_structure_boundaries);
    RUN_TEST(test_visible_name_utf8_scalar_and_byte_boundaries);
    RUN_TEST(
        test_visible_name_rejects_invalid_utf8_controls_and_unicode_whitespace);
    RUN_TEST(test_notes_allow_lf_but_reject_forbidden_code_points);
    RUN_TEST(test_note_normalization_is_explicit_and_deterministic);
    RUN_TEST(test_factory_catalog_has_exact_stable_order);
    RUN_TEST(test_catalog_accepts_zero_and_twelve_user_programs);
    RUN_TEST(test_catalog_rejects_factory_count_order_and_total_boundaries);
    RUN_TEST(test_catalog_rejects_duplicate_and_reserved_factory_ids);
    RUN_TEST(test_revision_types_reserve_zero_and_do_not_mix);
    RUN_TEST(test_storage_contract_uses_stable_ids_and_short_keys);
    return UNITY_END();
}
