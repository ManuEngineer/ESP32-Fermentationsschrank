#include <unity.h>

#include <optional>
#include <vector>

#include "configuration_limits.hpp"
#include "device_platform.hpp"
#include "fermentation_application.hpp"
#include "fermentation_ui_presentation_cache.hpp"
#include "mock_time_zone_resolver.hpp"
#include "simulated_persistent_state_store.hpp"

namespace fermentation {

class FermentationApplicationTestAccess {
   public:
    static ConfigurationService& configurationService(
        FermentationApplication& application) {
        return *application.configurationService_;
    }
};

}  // namespace fermentation

namespace {

using namespace fermentation;

FermentationUiExpectedRevisions revisions(std::uint64_t user,
                                          std::uint64_t catalog) {
    FermentationUiExpectedRevisions value;
    value.expectedUserConfigurationRevision = UserConfigurationRevision{user};
    value.expectedProgramCatalogRevision = ProgramCatalogRevision{catalog};
    return value;
}

FermentationUiExpectedRevisions undecidableRevisions() {
    return FermentationUiExpectedRevisions{};
}

// Fake fill: counts calls and returns a configurable result.
struct FakeFill {
    int calls{0};
    bool available{true};
    const char* locale{"de"};
    const char* timeZone{"Europe/Zurich"};

    std::optional<FermentationUiPresentationSource> operator()() {
        ++calls;
        if (!available) {
            return std::nullopt;
        }
        FermentationUiPresentationSource source;
        source.displayLocale = device_platform::LocaleId{locale};
        source.canonicalTimeZoneId = device_platform::TimeZoneId{timeZone};
        return source;
    }
};

void test_unchanged_revisions_do_not_call_fill_again() {
    FermentationUiPresentationCache cache;
    FakeFill fill;
    cache.update(false, revisions(1U, 1U), fill);
    TEST_ASSERT_EQUAL_INT(1, fill.calls);
    TEST_ASSERT_TRUE(cache.hasCopy());
    TEST_ASSERT_TRUE(cache.revisionsValid());
    for (int loop = 0; loop < 5; ++loop) {
        cache.update(false, revisions(1U, 1U), fill);
    }
    TEST_ASSERT_EQUAL_INT(1, fill.calls);
}

void test_program_catalog_revision_change_refills_catalog() {
    FermentationUiPresentationCache cache;
    FakeFill fill;
    cache.update(false, revisions(1U, 1U), fill);
    TEST_ASSERT_TRUE(cache.get().programCatalog.programs.empty());
    cache.update(false, revisions(1U, 2U), fill);
    TEST_ASSERT_EQUAL_INT(2, fill.calls);
    cache.update(false, revisions(1U, 2U), fill);
    TEST_ASSERT_EQUAL_INT(2, fill.calls);
}

void test_user_configuration_revision_change_refills_locale_and_time_zone() {
    FermentationUiPresentationCache cache;
    FakeFill fill;
    cache.update(false, revisions(1U, 1U), fill);
    TEST_ASSERT_EQUAL_STRING("de", cache.get().displayLocale.value().c_str());
    fill.locale = "fr";
    fill.timeZone = "Europe/Paris";
    cache.update(false, revisions(2U, 1U), fill);
    TEST_ASSERT_EQUAL_INT(2, fill.calls);
    TEST_ASSERT_EQUAL_STRING("fr", cache.get().displayLocale.value().c_str());
    TEST_ASSERT_EQUAL_STRING("Europe/Paris",
                             cache.get().canonicalTimeZoneId.value().c_str());
}

void test_first_fill_unavailable_is_not_valid_and_retries_every_loop() {
    FermentationUiPresentationCache cache;
    FakeFill fill;
    fill.available = false;
    cache.update(false, revisions(1U, 1U), fill);
    cache.update(false, revisions(1U, 1U), fill);
    TEST_ASSERT_EQUAL_INT(2, fill.calls);
    TEST_ASSERT_FALSE(cache.hasCopy());
    TEST_ASSERT_FALSE(cache.revisionsValid());
    // Safe defaults while no copy exists.
    TEST_ASSERT_EQUAL_STRING("en", cache.get().displayLocale.value().c_str());
    fill.available = true;
    cache.update(false, revisions(1U, 1U), fill);
    TEST_ASSERT_EQUAL_INT(3, fill.calls);
    TEST_ASSERT_TRUE(cache.hasCopy());
    TEST_ASSERT_TRUE(cache.revisionsValid());
}

void test_first_fill_with_undecidable_revisions_is_never_adopted() {
    FermentationUiPresentationCache cache;
    FakeFill fill;
    cache.update(false, undecidableRevisions(), fill);
    TEST_ASSERT_TRUE(cache.hasCopy());
    TEST_ASSERT_FALSE(cache.revisionsValid());
    cache.update(false, undecidableRevisions(), fill);
    TEST_ASSERT_EQUAL_INT(2, fill.calls);
}

void test_lease_unavailable_keeps_copy_and_revisions_and_retries_next_loop() {
    FermentationUiPresentationCache cache;
    FakeFill fill;
    cache.update(false, revisions(1U, 1U), fill);
    TEST_ASSERT_EQUAL_STRING("de", cache.get().displayLocale.value().c_str());

    // Revision changed but the runtime lease is unavailable: old copy and old
    // revisions stay; the next loop tries again.
    fill.available = false;
    fill.locale = "fr";
    cache.update(false, revisions(2U, 1U), fill);
    TEST_ASSERT_EQUAL_INT(2, fill.calls);
    TEST_ASSERT_TRUE(cache.hasCopy());
    TEST_ASSERT_TRUE(cache.revisionsValid());
    TEST_ASSERT_EQUAL_STRING("de", cache.get().displayLocale.value().c_str());
    cache.update(false, revisions(2U, 1U), fill);
    TEST_ASSERT_EQUAL_INT(3, fill.calls);

    fill.available = true;
    cache.update(false, revisions(2U, 1U), fill);
    TEST_ASSERT_EQUAL_INT(4, fill.calls);
    TEST_ASSERT_EQUAL_STRING("fr", cache.get().displayLocale.value().c_str());
    cache.update(false, revisions(2U, 1U), fill);
    TEST_ASSERT_EQUAL_INT(4, fill.calls);
}

void test_undecidable_revision_keeps_existing_copy_without_fill() {
    FermentationUiPresentationCache cache;
    FakeFill fill;
    cache.update(false, revisions(1U, 1U), fill);
    cache.update(false, undecidableRevisions(), fill);
    TEST_ASSERT_EQUAL_INT(1, fill.calls);
    TEST_ASSERT_TRUE(cache.hasCopy());
    TEST_ASSERT_TRUE(cache.revisionsValid());
}

void test_header_network_entry_evicts_copy_and_invalidates_revisions() {
    FermentationUiPresentationCache cache;
    FakeFill fill;
    cache.update(false, revisions(1U, 1U), fill);
    TEST_ASSERT_TRUE(cache.hasCopy());
    // Entering HeaderNetwork: no copy of the catalog stays alive and no fill
    // is performed while the network page is active (renderer gets nullptr).
    cache.update(true, revisions(1U, 1U), fill);
    TEST_ASSERT_FALSE(cache.hasCopy());
    TEST_ASSERT_FALSE(cache.revisionsValid());
    TEST_ASSERT_EQUAL_INT(1, fill.calls);
    TEST_ASSERT_TRUE(cache.get().programCatalog.programs.empty());
}

void test_network_commit_phase_holds_no_additional_catalog_copy() {
    FermentationUiPresentationCache cache;
    FakeFill fill;
    cache.update(false, revisions(1U, 1U), fill);
    // Several loop steps on HeaderNetwork, including a network-mode commit
    // step: the cache never refills while the network page is active.
    for (int loop = 0; loop < 4; ++loop) {
        cache.update(true, revisions(1U, 1U), fill);
        TEST_ASSERT_FALSE(cache.hasCopy());
    }
    TEST_ASSERT_EQUAL_INT(1, fill.calls);
}

void test_leaving_header_network_refills_after_success_only() {
    FermentationUiPresentationCache cache;
    FakeFill fill;
    cache.update(false, revisions(1U, 1U), fill);
    cache.update(true, revisions(1U, 1U), fill);
    cache.update(false, revisions(1U, 1U), fill);
    TEST_ASSERT_EQUAL_INT(2, fill.calls);
    TEST_ASSERT_TRUE(cache.hasCopy());
    TEST_ASSERT_TRUE(cache.revisionsValid());
    TEST_ASSERT_EQUAL_STRING("de", cache.get().displayLocale.value().c_str());
}

void test_leaving_header_network_unavailable_keeps_revisions_invalid_and_retries() {
    FermentationUiPresentationCache cache;
    FakeFill fill;
    cache.update(false, revisions(1U, 1U), fill);
    cache.update(true, revisions(1U, 1U), fill);
    fill.available = false;
    cache.update(false, revisions(1U, 1U), fill);
    TEST_ASSERT_EQUAL_INT(2, fill.calls);
    TEST_ASSERT_FALSE(cache.revisionsValid());
    TEST_ASSERT_FALSE(cache.hasCopy());
    cache.update(false, revisions(1U, 1U), fill);
    TEST_ASSERT_EQUAL_INT(3, fill.calls);
    fill.available = true;
    cache.update(false, revisions(1U, 1U), fill);
    TEST_ASSERT_EQUAL_INT(4, fill.calls);
    TEST_ASSERT_TRUE(cache.revisionsValid());
}

struct AppFixture {
    device_platform::DevicePlatform platform;
    device_platform_test_support::SimulatedPersistentStateStore store;
    device_platform_test_support::MockTimeZoneResolver timeZoneResolver;
    FermentationApplication application;

    AppFixture() {
        TEST_ASSERT_TRUE(platform.begin({true}));
        TEST_ASSERT_TRUE(application.begin(platform, store, timeZoneResolver));
    }
};

void test_application_source_is_unavailable_without_configuration_service() {
    FermentationApplication application;
    TEST_ASSERT_FALSE(application.uiPresentationSource().has_value());
}

void test_application_source_is_a_value_with_granted_runtime_lease() {
    AppFixture fixture;
    const auto source = fixture.application.uiPresentationSource();
    TEST_ASSERT_TRUE(source.has_value());
}

void test_application_source_is_unavailable_when_runtime_lease_is_busy() {
    AppFixture fixture;
    auto& service = FermentationApplicationTestAccess::configurationService(
        fixture.application);
    std::vector<RuntimeConfigurationReadLease> leases;
    for (std::size_t index = 0U;
         index < configuration_limits::kMaxRuntimeConfigurationReadLeases;
         ++index) {
        auto result = service.acquireRuntime();
        TEST_ASSERT_TRUE(result.status ==
                         RuntimeConfigurationReadStatus::RuntimeLeaseGranted);
        leases.push_back(std::move(result.lease));
    }
    TEST_ASSERT_FALSE(fixture.application.uiPresentationSource().has_value());
    leases.pop_back();
    TEST_ASSERT_TRUE(fixture.application.uiPresentationSource().has_value());
}

// D11: the last filled locale and time zone survive the HeaderNetwork
// eviction and follow every later successful fill (a language change).
void test_locale_and_time_zone_survive_eviction_and_follow_refills() {
    FermentationUiPresentationCache cache;
    // Safe defaults before any successful fill.
    TEST_ASSERT_EQUAL_STRING("en", cache.displayLocale().value().c_str());

    FakeFill german;
    cache.update(false, revisions(1U, 1U), german);
    TEST_ASSERT_EQUAL_STRING("de", cache.displayLocale().value().c_str());
    TEST_ASSERT_EQUAL_STRING("Europe/Zurich",
                             cache.canonicalTimeZoneId().value().c_str());

    // HeaderNetwork frees the copy but not the language.
    cache.update(true, revisions(1U, 1U), german);
    TEST_ASSERT_FALSE(cache.hasCopy());
    TEST_ASSERT_EQUAL_STRING("de", cache.displayLocale().value().c_str());
    TEST_ASSERT_EQUAL_STRING("Europe/Zurich",
                             cache.canonicalTimeZoneId().value().c_str());

    // A new user revision refills with the new language.
    FakeFill spanish;
    spanish.locale = "es";
    cache.update(false, revisions(2U, 1U), spanish);
    TEST_ASSERT_EQUAL_STRING("es", cache.displayLocale().value().c_str());
    cache.update(true, revisions(2U, 1U), spanish);
    TEST_ASSERT_EQUAL_STRING("es", cache.displayLocale().value().c_str());

    // An unavailable fill keeps the last value instead of reverting.
    FakeFill unavailable;
    unavailable.available = false;
    cache.update(false, revisions(3U, 1U), unavailable);
    TEST_ASSERT_EQUAL_STRING("es", cache.displayLocale().value().c_str());
    TEST_ASSERT_TRUE(cache.get().displayLocale.value() == "en");
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_unchanged_revisions_do_not_call_fill_again);
    RUN_TEST(test_locale_and_time_zone_survive_eviction_and_follow_refills);
    RUN_TEST(test_program_catalog_revision_change_refills_catalog);
    RUN_TEST(
        test_user_configuration_revision_change_refills_locale_and_time_zone);
    RUN_TEST(test_first_fill_unavailable_is_not_valid_and_retries_every_loop);
    RUN_TEST(test_first_fill_with_undecidable_revisions_is_never_adopted);
    RUN_TEST(
        test_lease_unavailable_keeps_copy_and_revisions_and_retries_next_loop);
    RUN_TEST(test_undecidable_revision_keeps_existing_copy_without_fill);
    RUN_TEST(test_header_network_entry_evicts_copy_and_invalidates_revisions);
    RUN_TEST(test_network_commit_phase_holds_no_additional_catalog_copy);
    RUN_TEST(test_leaving_header_network_refills_after_success_only);
    RUN_TEST(
        test_leaving_header_network_unavailable_keeps_revisions_invalid_and_retries);
    RUN_TEST(
        test_application_source_is_unavailable_without_configuration_service);
    RUN_TEST(test_application_source_is_a_value_with_granted_runtime_lease);
    RUN_TEST(test_application_source_is_unavailable_when_runtime_lease_is_busy);
    return UNITY_END();
}
